/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "os/os.h"
#include "console/console.h"
#include "nimble/ble.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "nimble/nimble_port.h"
#include <nimble/nimble_npl_os.h>
#include <theseus/log.h>
#include <FreeRTOS.h>

#define GATT_HRS_UUID		      0x180D
#define GATT_HRS_MEASUREMENT_UUID     0x2A37
#define GATT_HRS_BODY_SENSOR_LOC_UUID 0x2A38
#define GATT_DEVICE_INFO_UUID	      0x180A
#define GATT_MANUFACTURER_NAME_UUID   0x2A29
#define GATT_MODEL_NUMBER_UUID	      0x2A24

static const char *manuf_name = "Apache Mynewt";
static const char *model_num = "Mynewt HR Sensor";
uint16_t hrs_hrm_handle;

/* Host task settings.
 * The stack/priority just need to be big enough for the NimBLE host,
 * the task must run under the scheduler since the event loop blocks forever. */
#define BLE_HOST_TASK_STACK_WORDS (2048)
#define BLE_HOST_TASK_PRIORITY	  (tskIDLE_PRIORITY + 2)

static int gatt_svr_chr_access_heart_rate(uint16_t conn_handle, uint16_t attr_handle,
					  struct ble_gatt_access_ctxt *ctxt, void *arg);

static int gatt_svr_chr_access_device_info(uint16_t conn_handle, uint16_t attr_handle,
					   struct ble_gatt_access_ctxt *ctxt, void *arg);

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
	{/* Service: Heart-rate */
	 .type = BLE_GATT_SVC_TYPE_PRIMARY,
	 .uuid = BLE_UUID16_DECLARE(GATT_HRS_UUID),
	 .characteristics =
		 (struct ble_gatt_chr_def[]){
			 {
				 /* Characteristic: Heart-rate measurement */
				 .uuid = BLE_UUID16_DECLARE(GATT_HRS_MEASUREMENT_UUID),
				 .access_cb = gatt_svr_chr_access_heart_rate,
				 .val_handle = &hrs_hrm_handle,
				 .flags = BLE_GATT_CHR_F_NOTIFY,
			 },
			 {
				 /* Characteristic: Body sensor location */
				 .uuid = BLE_UUID16_DECLARE(GATT_HRS_BODY_SENSOR_LOC_UUID),
				 .access_cb = gatt_svr_chr_access_heart_rate,
				 .flags = BLE_GATT_CHR_F_READ,
			 },
			 {
				 0, /* No more characteristics in this service */
			 },
		 }},

	{/* Service: Device Information */
	 .type = BLE_GATT_SVC_TYPE_PRIMARY,
	 .uuid = BLE_UUID16_DECLARE(GATT_DEVICE_INFO_UUID),
	 .characteristics =
		 (struct ble_gatt_chr_def[]){
			 {
				 /* Characteristic: * Manufacturer name */
				 .uuid = BLE_UUID16_DECLARE(GATT_MANUFACTURER_NAME_UUID),
				 .access_cb = gatt_svr_chr_access_device_info,
				 .flags = BLE_GATT_CHR_F_READ,
			 },
			 {
				 /* Characteristic: Model number string */
				 .uuid = BLE_UUID16_DECLARE(GATT_MODEL_NUMBER_UUID),
				 .access_cb = gatt_svr_chr_access_device_info,
				 .flags = BLE_GATT_CHR_F_READ,
			 },
			 {
				 0, /* No more characteristics in this service */
			 },
		 }},

	{
		0, /* No more services */
	},
};

static int gatt_svr_chr_access_heart_rate(uint16_t conn_handle, uint16_t attr_handle,
					  struct ble_gatt_access_ctxt *ctxt, void *arg)
{
	/* Sensor location, set to "Chest" */
	static uint8_t body_sens_loc = 0x01;
	uint16_t uuid;
	int rc;

	uuid = ble_uuid_u16(ctxt->chr->uuid);

	if (uuid == GATT_HRS_BODY_SENSOR_LOC_UUID) {
		rc = os_mbuf_append(ctxt->om, &body_sens_loc, sizeof(body_sens_loc));

		return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
	}

	assert(0);
	return BLE_ATT_ERR_UNLIKELY;
}

static int gatt_svr_chr_access_device_info(uint16_t conn_handle, uint16_t attr_handle,
					   struct ble_gatt_access_ctxt *ctxt, void *arg)
{
	uint16_t uuid;
	int rc;

	uuid = ble_uuid_u16(ctxt->chr->uuid);

	if (uuid == GATT_MODEL_NUMBER_UUID) {
		rc = os_mbuf_append(ctxt->om, model_num, strlen(model_num));
		return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
	}

	if (uuid == GATT_MANUFACTURER_NAME_UUID) {
		rc = os_mbuf_append(ctxt->om, manuf_name, strlen(manuf_name));
		return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
	}

	assert(0);
	return BLE_ATT_ERR_UNLIKELY;
}

void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
	char buf[BLE_UUID_STR_LEN];

	switch (ctxt->op) {
	case BLE_GATT_REGISTER_OP_SVC:
		LOG("registered service %s with handle=%d\n",
		    ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf), ctxt->svc.handle);
		break;

	case BLE_GATT_REGISTER_OP_CHR:
		LOG("registering characteristic %s with "
		    "def_handle=%d val_handle=%d\n",
		    ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf), ctxt->chr.def_handle,
		    ctxt->chr.val_handle);
		break;

	case BLE_GATT_REGISTER_OP_DSC:
		LOG("registering descriptor %s with handle=%d\n",
		    ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf), ctxt->dsc.handle);
		break;

	default:
		assert(0);
		break;
	}
}

int gatt_svr_init(void)
{
	int rc;

	rc = ble_gatts_count_cfg(gatt_svr_svcs);
	if (rc != 0) {
		return rc;
	}

	rc = ble_gatts_add_svcs(gatt_svr_svcs);
	if (rc != 0) {
		return rc;
	}

	return 0;
}

static bool notify_state;

/* Connection handle */
static uint16_t conn_handle;

static const char *device_name = "blehr_sensor";

static int blehr_gap_event(struct ble_gap_event *event, void *arg);

static uint8_t blehr_addr_type;

/* Sending notify data timer */
static struct ble_npl_callout blehr_tx_timer;

/* Variable to simulate heart beats */
static uint8_t heartrate = 90;

/*
 * Enables advertising with parameters:
 *     o General discoverable mode
 *     o Undirected connectable mode
 */
static void blehr_advertise(void)
{
	struct ble_gap_adv_params adv_params;
	struct ble_hs_adv_fields fields;
	int rc;

	/*
	 *  Set the advertisement data included in our advertisements:
	 *     o Flags (indicates advertisement type and other general info)
	 *     o Advertising tx power
	 *     o Device name
	 */
	memset(&fields, 0, sizeof(fields));

	/*
	 * Advertise two flags:
	 *      o Discoverability in forthcoming advertisement (general)
	 *      o BLE-only (BR/EDR unsupported)
	 */
	fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

	/*
	 * Indicate that the TX power level field should be included; have the
	 * stack fill this value automatically.  This is done by assigning the
	 * special value BLE_HS_ADV_TX_PWR_LVL_AUTO.
	 */
	fields.tx_pwr_lvl_is_present = 1;
	fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

	fields.name = (uint8_t *)device_name;
	fields.name_len = strlen(device_name);
	fields.name_is_complete = 1;

	rc = ble_gap_adv_set_fields(&fields);
	if (rc != 0) {
		LOG("error setting advertisement data; rc=%d\n", rc);
		return;
	}

	/* Begin advertising */
	memset(&adv_params, 0, sizeof(adv_params));
	adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
	adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
	rc = ble_gap_adv_start(blehr_addr_type, NULL, BLE_HS_FOREVER, &adv_params, blehr_gap_event,
			       NULL);
	if (rc != 0) {
		LOG("error enabling advertisement; rc=%d\n", rc);
		return;
	}
}

static void blehr_tx_hrate_stop(void)
{
	ble_npl_callout_stop(&blehr_tx_timer);
}

/* Reset heartrate measurement */
static void blehr_tx_hrate_reset(void)
{
	int rc;

	rc = ble_npl_callout_reset(&blehr_tx_timer, pdMS_TO_TICKS(1000));
	assert(rc == 0);
}

/* This functions simulates heart beat and notifies it to the client */
static void blehr_tx_hrate(struct ble_npl_event *ev)
{
	static uint8_t hrm[2];
	int rc;
	struct os_mbuf *om;

	if (!notify_state) {
		blehr_tx_hrate_stop();
		heartrate = 90;
		return;
	}

	hrm[0] = 0x06;	    /* contact of a sensor */
	hrm[1] = heartrate; /* storing dummy data */

	/* Simulation of heart beats */
	heartrate++;
	if (heartrate == 160) {
		heartrate = 90;
	}

	om = ble_hs_mbuf_from_flat(hrm, sizeof(hrm));

	rc = ble_gatts_notify_custom(conn_handle, hrs_hrm_handle, om);

	assert(rc == 0);
	blehr_tx_hrate_reset();
}

static int blehr_gap_event(struct ble_gap_event *event, void *arg)
{
	switch (event->type) {
	case BLE_GAP_EVENT_CONNECT:
		/* A new connection was established or a connection attempt failed */
		LOG("connection %s; status=%d\n",
		    event->connect.status == 0 ? "established" : "failed", event->connect.status);

		if (event->connect.status != 0) {
			/* Connection failed; resume advertising */
			blehr_advertise();
			conn_handle = 0;
		} else {
			conn_handle = event->connect.conn_handle;
		}

		break;

	case BLE_GAP_EVENT_DISCONNECT:
		LOG("disconnect; reason=%d\n", event->disconnect.reason);
		conn_handle = BLE_HS_CONN_HANDLE_NONE; /* reset conn_handle */

		/* Connection terminated; resume advertising */
		blehr_advertise();
		break;

	case BLE_GAP_EVENT_ADV_COMPLETE:
		LOG("adv complete\n");
		blehr_advertise();
		break;

	case BLE_GAP_EVENT_SUBSCRIBE:
		LOG("subscribe event; cur_notify=%d\n value handle; "
		    "val_handle=%d\n",
		    event->subscribe.cur_notify, hrs_hrm_handle);
		if (event->subscribe.attr_handle == hrs_hrm_handle) {
			notify_state = event->subscribe.cur_notify;
			blehr_tx_hrate_reset();
		}
		break;

	case BLE_GAP_EVENT_MTU:
		LOG("mtu update event; conn_handle=%d mtu=%d\n", event->mtu.conn_handle,
		    event->mtu.value);
		break;
	}

	return 0;
}

static void blehr_on_sync(void)
{
	int rc;

	rc = ble_hs_util_ensure_addr(0);
	assert(rc == 0);
	/* Use privacy */
	rc = ble_hs_id_infer_auto(0, &blehr_addr_type);
	assert(rc == 0);

	/* Begin advertising */
	blehr_advertise();
}

static void ble_host_task(void *param)
{
	(void)param;
	int rc;

	nimble_port_init();

	/* Initialize the NimBLE host configuration */
	ble_hs_cfg.sync_cb = blehr_on_sync;
	ble_npl_callout_init(&blehr_tx_timer, nimble_port_get_dflt_eventq(), blehr_tx_hrate, NULL);

	rc = gatt_svr_init();
	assert(rc == 0);

	/* Set the default device name */
	rc = ble_svc_gap_device_name_set(device_name);
	assert(rc == 0);

	nimble_port_run();
	vTaskDelete(NULL);
}

/*
 * main
 *
 * The main task for the project. This function initializes the packages,
 * then starts serving events from default event queue.
 *
 * @return int NOTE: this function should never return!
 */
int main(void)
{
	/* Create the host task, then start FreeRTOS.
	 * The scheduler sets up the shared timer before our task runs,
	 * so the controller setup inside it finds everything ready. */
	BaseType_t ok = xTaskCreate(ble_host_task, "ble_hs", BLE_HOST_TASK_STACK_WORDS, NULL,
				    BLE_HOST_TASK_PRIORITY, NULL);
	assert(ok == pdPASS);

	LOG("[APP] starting scheduler\n");
	vTaskStartScheduler();

	while (1) {
		/* Should never reach here unless the scheduler can't start (e.g. no heap). */
	}
	return 0;
}
