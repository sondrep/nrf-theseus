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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "os/os.h"
#include "sysinit/sysinit.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "nimble/nimble_port.h"
#include <theseus/module.h>
#include <theseus/log.h>

#include <FreeRTOS.h>
#include <task.h>

static const char *device_name = "Theseus ble_peripheral";

/* Host task settings.
 * The stack/priority just need to be big enough for the NimBLE host,
 * the task must run under the scheduler since the event loop blocks forever. */
#define BLE_HOST_TASK_STACK_WORDS (2048)
#define BLE_HOST_TASK_PRIORITY	  (tskIDLE_PRIORITY + 2)

/* Set once we accept a connection, cleared on disconnect. */
static uint16_t conn_handle;

/* adv_event() restarts advertising, so it needs the forward declaration. */
static void advertise(void);

static int adv_event(struct ble_gap_event *event, void *arg)
{
	switch (event->type) {
	case BLE_GAP_EVENT_CONNECT:
		/* A central connected to us; remember the handle. */
		assert(event->connect.status == 0);
		LOG("[APP] connected; handle=%d\n", event->connect.conn_handle);
		conn_handle = event->connect.conn_handle;
		break;
	case BLE_GAP_EVENT_CONN_UPDATE_REQ:
		/* Peer wants new connection parameters; NULL accepts our defaults. */
		event->conn_update_req.conn_handle = conn_handle;
		event->conn_update_req.peer_params = NULL;
		break;
	case BLE_GAP_EVENT_DISCONNECT:
		/* Connection dropped; clear the handle and advertise again. */
		LOG("[APP] disconnected; reason=0x%x\n", event->disconnect.reason);
		conn_handle = BLE_HS_CONN_HANDLE_NONE;
		advertise();
		break;
	case BLE_GAP_EVENT_ADV_COMPLETE:
		/* Advertising stopped on its own; restart it. */
		advertise();
		break;
	default:
		break;
	}
	return 0;
}

static void advertise(void)
{
	int rc;
	struct ble_gap_adv_params adv_params;
	struct ble_hs_adv_fields fields;
	struct ble_hs_adv_fields rsp_fields;

	/* Connectable + general-discoverable: a central can find us and connect. */
	memset(&adv_params, 0, sizeof(adv_params));
	adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
	adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

	/* Advertising packet: flags, a service UUID, and TX power. */
	memset(&fields, 0, sizeof(fields));
	fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
	fields.uuids128 =
		BLE_UUID128(BLE_UUID128_DECLARE(0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
						0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff));
	fields.num_uuids128 = 1;
	fields.uuids128_is_complete = 0;
	fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

	/* Name goes in the scan response: it won't fit alongside the UUID in the
	 * 31-byte advertising packet, so we send it only when a scanner asks. */
	memset(&rsp_fields, 0, sizeof(rsp_fields));
	rsp_fields.name = (uint8_t *)device_name;
	rsp_fields.name_len = strlen(device_name);
	rsp_fields.name_is_complete = 1;

	rc = ble_gap_adv_set_fields(&fields);
	if (rc != 0) {
		/* Don't assert: the host may be resetting, just bail and retry later. */
		LOG("[APP] adv_set_fields failed; rc=%d\n", rc);
		return;
	}
	ble_gap_adv_rsp_set_fields(&rsp_fields);

	/* Advertise forever with a RANDOM address to match the address set on sync. */
	rc = ble_gap_adv_start(BLE_OWN_ADDR_RANDOM, NULL, BLE_HS_FOREVER, &adv_params, adv_event, NULL);
	if (rc != 0 && rc != BLE_HS_EALREADY) {
		LOG("[APP] adv_start failed; rc=%d\n", rc);
		return;
	}

	LOG("[APP] advertising as \"%s\"\n", device_name);
}

static void on_sync(void)
{
	int rc;
	ble_addr_t addr;

	/* Host and controller are ready; make up a random address and use it. */
	rc = ble_hs_id_gen_rnd(0, &addr);
	assert(rc == 0);

	rc = ble_hs_id_set_rnd(addr.val);
	assert(rc == 0);

	advertise();
}

static void on_reset(int reason)
{
	LOG("[APP] on_reset reason=%d\n", reason);
}

static void ble_host_task(void *param)
{
	(void)param;
	int rc;

	/* All NimBLE and controller setup happens here, inside the task,
	 * after the scheduler has started, for two reasons:
	 *  1. The GRTC timer is shared between the radio and the FreeRTOS tick.
	 *     The scheduler initialises it first, so doing controller setup
	 *     beforehand would try to init it twice and hang the CPU.
	 *  2. nimble_port_run() blocks forever, which is only safe in a running task. */

	/* Core NimBLE setup. */
	nimble_port_init();

	/* Standard GATT services every device needs (generic access + profile). */
	ble_svc_gap_init();
	ble_svc_gatt_init();

	/* Hook up our callbacks for when the stack is ready or gets reset. */
	ble_hs_cfg.sync_cb = on_sync;
	ble_hs_cfg.reset_cb = on_reset;

	rc = ble_svc_gap_device_name_set(device_name);
	assert(rc == 0);

	/* Run the host event loop. This never returns under normal use. */
	LOG("[APP] host ready, entering event loop\n");
	nimble_port_run();

	/* Only get here if the host shuts down. */
	vTaskDelete(NULL);
}

int main(void)
{
	/* Create the host task, then start FreeRTOS.
	 * The scheduler sets up the shared timer before our task runs,
	 * so the controller setup inside it finds everything ready. */
	BaseType_t ok = xTaskCreate(ble_host_task,
				    "ble_hs",
				    BLE_HOST_TASK_STACK_WORDS,
				    NULL,
				    BLE_HOST_TASK_PRIORITY,
				    NULL);
	assert(ok == pdPASS);

	LOG("[APP] starting scheduler\n");
	vTaskStartScheduler();

	while (1) {
		/* Should never reach here unless the scheduler can't start (e.g. no heap). */
	}

	return 0;
}