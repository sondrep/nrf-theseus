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

#include "sysinit/sysinit.h"
#include "os/os.h"
#include "host/ble_hs.h"
#include "host/util/util.h"

#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "nimble/nimble_port.h"
#include <theseus/module.h>
#include <theseus/log.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

/* Host task settings.
 * The stack/priority just need to be big enough for the NimBLE host,
 * the task must run under the scheduler since the event loop blocks forever. */
#define BLE_HOST_TASK_STACK_WORDS  ( 2048 )
#define BLE_HOST_TASK_PRIORITY     ( tskIDLE_PRIORITY + 2 )

/* The scan callback runs in the host task,
 * so it just hands each report off to a separate task over a queue and the printing happens there.
 * That keeps the host loop fast enough to keep up with the controller. */
#define SCAN_PRINT_TASK_STACK_WORDS  ( 1024 )
#define SCAN_PRINT_TASK_PRIORITY     ( tskIDLE_PRIORITY + 1 )
#define SCAN_Q_DEPTH   16
static QueueHandle_t scan_q;

static void scan(void);

struct scan_msg {
	uint8_t addr[6];
	int8_t  rssi;
};

static void scan_print_task(void *param)
{
	(void)param;
	struct scan_msg msg;

	while (true) {
		/* Block until the scan callback queues a report, then print it. */
		if (xQueueReceive(scan_q, &msg, portMAX_DELAY) == pdTRUE) {
			LOG("[APP] found %02x:%02x:%02x:%02x:%02x:%02x rssi=%d\n",
				msg.addr[5], msg.addr[4], msg.addr[3], msg.addr[2], msg.addr[1], msg.addr[0],
				msg.rssi);
		}
	}
}

static void ble_app_set_addr(void)
{
	ble_addr_t addr;
	int rc;

	/* Make up a random private address and use it as our identity. */
	rc = ble_hs_id_gen_rnd(1, &addr);
	assert(rc == 0);

	rc = ble_hs_id_set_rnd(addr.val);
	assert(rc == 0);
}

static int scan_event(struct ble_gap_event *event, void *arg)
{
	struct ble_hs_adv_fields fields;
	struct scan_msg msg;
	int rc;

	switch (event->type) {
	case BLE_GAP_EVENT_DISC:
		/* Got an advertising report from a nearby device. */
		rc = ble_hs_adv_parse_fields(&fields, event->disc.data,
		                             event->disc.length_data);
		if (rc != 0) {
			return 0;
		}
		memcpy(msg.addr, event->disc.addr.val, 6);
		msg.rssi = event->disc.rssi;

		/* Non-blocking send: if the print task is behind and the queue is
		 * full, drop the report rather than stall the host. */
		xQueueSend(scan_q, &msg, 0);
		return 0;
	case BLE_GAP_EVENT_DISC_COMPLETE:
		/* Scan window ended, so start another one. */
		scan();
		return 0;
	default:
		return 0;
	}
}

static void scan(void)
{
	/* Passive scan:
	 * just listen for adverts, don't send scan requests.
	 * itvl/window are in units of 0.625 ms, so window must be <= interval. */
	struct ble_gap_disc_params scan_params = {
		.itvl = 1000,
		.window = 10,
		.filter_policy = 0,
		.limited = 0,
		.passive = 1,
		.filter_duplicates = 1
	};

	/* RANDOM address to match the private address set above. */
	ble_gap_disc(BLE_OWN_ADDR_RANDOM, 1000, &scan_params, scan_event, NULL);
}

static void on_sync(void)
{
	/* Host and controller are ready; set our address and start scanning. */
	ble_app_set_addr();
	scan();
}

static void on_reset(int reason)
{
	LOG("[APP] on_reset reason=%d\n", reason);
}

static void ble_host_task(void *param)
{
	(void)param;

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

	/* Run the host event loop. This never returns under normal use. */
	LOG("[APP] host ready, entering event loop\n");
	nimble_port_run();

	/* Only get here if the host shuts down. */
	vTaskDelete(NULL);
}

int main(void)
{
	/* Queue + task that print scan reports off the host loop.
	 * Neither touches the radio/GRTC,
	 * so we set them up here before the scheduler starts. */
	scan_q = xQueueCreate(SCAN_Q_DEPTH, sizeof(struct scan_msg));
	assert(scan_q != NULL);

	BaseType_t pok = xTaskCreate(scan_print_task,
	                             "scan_pr",
	                             SCAN_PRINT_TASK_STACK_WORDS,
	                             NULL,
	                             SCAN_PRINT_TASK_PRIORITY,
	                             NULL);
	assert(pok == pdPASS);

	/* Host task does all BLE bring-up.
	 * The scheduler sets up the shared timer before this task runs,
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