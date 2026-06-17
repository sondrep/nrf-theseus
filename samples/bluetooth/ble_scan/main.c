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
#include "mpsl.h"
#include <theseus/rng.h>
#include <theseus/log.h>
#include <nrfx_grtc.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#define BLE_HOST_TASK_STACK_WORDS  ( 2048 )
#define BLE_HOST_TASK_PRIORITY     ( tskIDLE_PRIORITY + 2 )

#define SCAN_PRINT_TASK_STACK_WORDS  ( 1024 )
#define SCAN_PRINT_TASK_PRIORITY     ( tskIDLE_PRIORITY + 1 )
#define SCAN_Q_DEPTH   16
static QueueHandle_t scan_q; /* Queue handle for data to print */

/* scan_event() calls scan(), so forward declaration is required */
static void scan(void);

struct scan_msg {
    uint8_t addr[6];
    int8_t  rssi;
};

static void
scan_print_task(void *param)
{
    (void)param;
    struct scan_msg msg;

    for (;;) {
        /* Block until a report is available; this task owns all printf cost. */
        if (xQueueReceive(scan_q, &msg, portMAX_DELAY) == pdTRUE) {
            printf("[APP] UUID %02x:%02x:%02x:%02x:%02x:%02x rssi=%d \n",
                   msg.addr[5], msg.addr[4], msg.addr[3],
                   msg.addr[2], msg.addr[1], msg.addr[0],
                   msg.rssi);
        }
    }
}

static void
ble_app_set_addr(void)
{
    ble_addr_t addr;
    int rc;

    /* generate new non-resolvable private address */
    rc = ble_hs_id_gen_rnd(1, &addr);
    assert(rc == 0);

    /* set generated address */
    rc = ble_hs_id_set_rnd(addr.val);
    assert(rc == 0);
}

static int scan_event(struct ble_gap_event *event, void *arg)
{
    struct ble_hs_adv_fields fields;
    struct scan_msg msg;
    int rc;

    switch (event->type) {
    /* advertising report has been received during discovery procedure */
    case BLE_GAP_EVENT_DISC:
        rc = ble_hs_adv_parse_fields(&fields, event->disc.data,
                                     event->disc.length_data);
        if (rc != 0) {
            return 0;
        }
        memcpy(msg.addr, event->disc.addr.val, 6);
        msg.rssi = event->disc.rssi;

        /* Non-blocking queue so that the nimble host can keep up with SDC,
         * if the queue is full it will silently drop the corresponding data
         */
        xQueueSend(scan_q, &msg, 0);
        return 0;
    /* discovery procedure has terminated */
    case BLE_GAP_EVENT_DISC_COMPLETE:
        scan();
        return 0;
    default:
        return 0;
    }
}

static void scan(void)
{
    /* set scan parameters */
    struct ble_gap_disc_params scan_params = {
        .itvl = 1000, /* is multiplied by 0.625 ms */
        .window = 10, /* is multiplied by 0.625 ms, must be smaller or equal to interval */
        .filter_policy = 0,
        .limited = 0,
        .passive = 1,
        .filter_duplicates = 1
    };
    /* performs discovery procedure; value of own_addr_type is hard-coded,
       because NRPA is used */
    ble_gap_disc(BLE_OWN_ADDR_RANDOM, 1000, &scan_params, scan_event, NULL);
}

static void
on_sync(void)
{
    /* Generate a non-resolvable private address. */
    ble_app_set_addr();

    /* begin scanning */
    scan();
}

static void
on_reset(int reason)
{
}

static void
ble_host_task(void *param)
{
    (void)param;

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

    /*** Stage 250: ble_transport_init (nimble/transport) */
    ble_transport_init();

    /*** Stage 251: ble_transport_hs_init (nimble/transport) */
    ble_transport_hs_init();

    /*** Stage 301: ble_svc_gap_init (nimble/host/services/gap) */
    ble_svc_gap_init();

    /*** Stage 302: ble_svc_gatt_init (nimble/host/services/gatt) */
    ble_svc_gatt_init();

    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.reset_cb = on_reset;

    scan_q = xQueueCreate(SCAN_Q_DEPTH, sizeof(struct scan_msg));
    assert(scan_q != NULL);

    BaseType_t pok = xTaskCreate(scan_print_task, "scan_pr",
                                 SCAN_PRINT_TASK_STACK_WORDS, NULL,
                                 SCAN_PRINT_TASK_PRIORITY, NULL);
    assert(pok == pdPASS);

    printf("[APP] ble_host_task: entering nimble_port_run\n");
    nimble_port_run();

    /* Only reached if the host is torn down. */
    vTaskDelete(NULL);
}

int main(void)
{
    int rc;
    theseus_console_init();

    rc = theseus_rng_init();
    if(rc) {
        return rc;
    }

    BaseType_t ok = xTaskCreate(ble_host_task, "ble_hs",
                                BLE_HOST_TASK_STACK_WORDS, NULL,
                                BLE_HOST_TASK_PRIORITY, NULL);
    printf("[APP] xTaskCreate(ble_hs) ok=%ld\n", (long)ok);
    assert(ok == pdPASS);

    printf("[APP] starting scheduler\n");
    vTaskStartScheduler();

    /* As the last thing, process events from default event queue. */
    while (1) {
    }

    return 0;
}
