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

/*
 * ble_adv_rtos -- BLE advertising sample running the NimBLE host under FreeRTOS.
 *
 * The host event loop (nimble_port_run) runs from a dedicated task and the
 * scheduler is started in main(). This is the recommended setup:
 * nimble_port_run() blocks on its event queue forever, which is only valid
 * inside a real task once the scheduler is running.
 *
 * Compare with ble_adv_bm, which runs the same advertiser bare-metal (no
 * scheduler) using a non-blocking polled event loop.
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

static const char *device_name = "Apache Asil";

/* NimBLE host task stack/priority. nimble_port_run() blocks on an empty event
 * queue with portMAX_DELAY, which is only valid once the scheduler is running
 * and a real task (non-NULL pxCurrentTCB) is executing; otherwise FreeRTOS
 * derefs a NULL current-task pointer and the chip hard-faults. */
#define BLE_HOST_TASK_STACK_WORDS (2048)
#define BLE_HOST_TASK_PRIORITY	  (tskIDLE_PRIORITY + 2)

/* adv_event() calls advertise(). */
static void advertise(void);

static void set_ble_addr(void)
{
	int rc;
	ble_addr_t addr;

	/* Generate and set a non-resolvable private address. */
	rc = ble_hs_id_gen_rnd(1, &addr);
	printf("[APP] ble_hs_id_gen_rnd rc=%d addr=%02x:%02x:%02x:%02x:%02x:%02x type=%d\n", rc,
	       addr.val[5], addr.val[4], addr.val[3], addr.val[2], addr.val[1], addr.val[0],
	       addr.type);
	assert(rc == 0);

	rc = ble_hs_id_set_rnd(addr.val);
	printf("[APP] ble_hs_id_set_rnd rc=%d\n", rc);
	assert(rc == 0);
}

static int adv_event(struct ble_gap_event *event, void *arg)
{
	printf("[APP] adv_event type=%d\n", event->type);
	switch (event->type) {
	case BLE_GAP_EVENT_ADV_COMPLETE:
		advertise();
		return 0;
	default:
		return 0;
	}
}

static void advertise(void)
{
	int rc;
	struct ble_gap_adv_params adv_params;
	struct ble_hs_adv_fields fields;

	memset(&adv_params, 0, sizeof(adv_params));
	adv_params.conn_mode = BLE_GAP_CONN_MODE_NON;
	adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

	/* Advertising data: flags, tx power level, complete name. */
	memset(&fields, 0, sizeof(fields));
	fields.flags = BLE_HS_ADV_F_DISC_GEN;
	fields.tx_pwr_lvl_is_present = 1;
	fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
	fields.name = (uint8_t *)device_name;
	fields.name_len = strlen(device_name);
	fields.name_is_complete = 1;

	printf("[APP] advertise(): calling ble_gap_adv_set_fields\n");
	rc = ble_gap_adv_set_fields(&fields);
	printf("[APP] ble_gap_adv_set_fields rc=%d\n", rc);
	assert(rc == 0);

	/* Own address type is hard-coded RANDOM since we use an NRPA.
	 *
	 * BLE_HS_FOREVER: the controller advertises autonomously and the host never
	 * tears it down and re-runs the adv setup. A finite duration takes that
	 * fragile re-setup path on expiry, which previously dropped advertising
	 * entirely. */
	rc = ble_gap_adv_start(BLE_OWN_ADDR_RANDOM, NULL, BLE_HS_FOREVER, &adv_params, adv_event,
			       NULL);
	printf("[APP] ble_gap_adv_start rc=%d\n", rc);
	assert(rc == 0);
	printf("[APP] advertise(): advertising ENABLED\n");
}

static void on_sync(void)
{
	printf("[APP] on_sync: host/controller synced\n");
	set_ble_addr();
	advertise();
}

static void on_reset(int reason)
{
	printf("[APP] on_reset reason=%d\n", reason);
}

static void ble_host_task(void *param)
{
	(void)param;
	int rc;

	/* All NimBLE + controller bring-up runs here, inside the task, after the
	 * scheduler has started, for two reasons:
	 *
	 *  1. The GRTC is shared between MPSL (radio timing) and the FreeRTOS tick
	 *     (vPortSetupTimerInterrupt in port.c). The tick setup runs first as
	 *     part of vTaskStartScheduler() and does the one-and-only
	 *     nrfx_grtc_init(), so grtc_lfclk_init() here finds it already running
	 *     and skips re-init. Initialising the controller before the scheduler
	 *     made the tick's nrfx_grtc_init() return -EALREADY, tripping a
	 *     configASSERT() that hung the CPU (watchdog reset, no advertising).
	 *
	 *  2. nimble_port_run() blocks with portMAX_DELAY, only valid from a real
	 *     task once the scheduler is running. */
	printf("[APP] ble_host_task: init start\n");

	/*** Stage 0: os_pkg_init (kernel/os) */
	nimble_port_init();

	/*** Stage 301: ble_svc_gap_init (nimble/host/services/gap) */
	ble_svc_gap_init();

	/*** Stage 302: ble_svc_gatt_init (nimble/host/services/gatt) */
	ble_svc_gatt_init();

	ble_hs_cfg.sync_cb = on_sync;
	ble_hs_cfg.reset_cb = on_reset;

	rc = ble_svc_gap_device_name_set(device_name);
	printf("[APP] ble_svc_gap_device_name_set rc=%d\n", rc);
	assert(rc == 0);

	printf("[APP] ble_host_task: entering nimble_port_run\n");
	nimble_port_run();

	/* Only reached if the host is torn down. */
	vTaskDelete(NULL);
}

int main(void)
{
	printf("\n[APP] ===== ble_adv_rtos boot =====\n");

	/* Create the host task and hand control to FreeRTOS. The scheduler brings
	 * up the GRTC tick before the task body runs, so controller init inside
	 * ble_host_task() sees an already-initialised GRTC. */
	BaseType_t ok = xTaskCreate(ble_host_task, "ble_hs", BLE_HOST_TASK_STACK_WORDS, NULL,
				    BLE_HOST_TASK_PRIORITY, NULL);
	printf("[APP] xTaskCreate(ble_hs) ok=%ld\n", (long)ok);
	assert(ok == pdPASS);

	printf("[APP] starting scheduler\n");
	vTaskStartScheduler();

	/* Only reached if the scheduler fails to start (e.g. out of heap). */
	while (1) {
	}

	return 0;
}
