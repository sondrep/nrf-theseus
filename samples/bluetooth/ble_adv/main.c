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
#include <theseus/rng.h>
#include <theseus/module.h>
#include <theseus/log.h>

#include <FreeRTOS.h>
#include <task.h>

static const char *device_name = "Theseus ble_adv";

/* Host task settings.
 * The stack/priority just need to be big enough for the NimBLE host,
 * the task must run under the scheduler since the event loop blocks forever. */
#define BLE_HOST_TASK_STACK_WORDS (2048)
#define BLE_HOST_TASK_PRIORITY	  (tskIDLE_PRIORITY + 2)

static void advertise(void);

static void set_ble_addr(void)
{
	int rc;
	ble_addr_t addr;

	/* Make up a random private address and use it as our identity. */
	rc = ble_hs_id_gen_rnd(1, &addr);
	assert(rc == 0);

	rc = ble_hs_id_set_rnd(addr.val);
	assert(rc == 0);

	LOG("[APP] using addr %02x:%02x:%02x:%02x:%02x:%02x\n",
	    addr.val[5], addr.val[4], addr.val[3], addr.val[2], addr.val[1], addr.val[0]);
}

static int adv_event(struct ble_gap_event *event, void *arg)
{
	LOG("[APP] adv_event type=%d\n", event->type);
	return 0;
}

static void advertise(void)
{
	int rc;
	struct ble_gap_adv_params adv_params;
	struct ble_hs_adv_fields fields;

	memset(&adv_params, 0, sizeof(adv_params));
	adv_params.conn_mode = BLE_GAP_CONN_MODE_NON;
	adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

	/* What we broadcast: discoverable flag, TX power, and our device name. */
	memset(&fields, 0, sizeof(fields));
	fields.flags = BLE_HS_ADV_F_DISC_GEN;
	fields.tx_pwr_lvl_is_present = 1;
	fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
	fields.name = (uint8_t *)device_name;
	fields.name_len = strlen(device_name);
	fields.name_is_complete = 1;

	rc = ble_gap_adv_set_fields(&fields);
	assert(rc == 0);

	/* Advertise forever with a RANDOM address to match the private address set above. */
	rc = ble_gap_adv_start(BLE_OWN_ADDR_RANDOM, NULL, BLE_HS_FOREVER, &adv_params, adv_event, NULL);
	assert(rc == 0);

	LOG("[APP] advertising as \"%s\"\n", device_name);
}

static void on_sync(void)
{
	/* Host and controller are ready; set our address and start advertising. */
	set_ble_addr();
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
		/* Should never reach here unless the scheduler can't start (eks: no heap). */
	}

	return 0;
}