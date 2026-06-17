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
 * ble_adv_bm -- BLE advertising sample running the NimBLE host bare-metal,
 * without the FreeRTOS scheduler.
 *
 * The radio is driven entirely by MPSL / the SoftDevice Controller off its own
 * interrupts (RADIO/TIMER/GRTC/SWI03), so once advertising is enabled it runs
 * autonomously regardless of the main thread. The host's only blocking point,
 * nimble_port_run()'s eventq wait, is replaced here by a non-blocking polled
 * loop.
 *
 * Vs. ble_adv_rtos: no scheduler/tasks (everything runs in thread mode), so the
 * GRTC-based kernel tick never runs and grtc_lfclk_init() does the real GRTC
 * init MPSL needs.
 *
 * LIMITATION: with no kernel tick, NimBLE callouts/timers never fire. Fine for
 * this static BLE_HS_FOREVER non-connectable advertiser; anything needing host
 * timers (finite duration, connections, GATT timeouts) must use ble_adv_rtos.
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
#include "nimble/nimble_npl.h"
#include "mpsl.h"
#include <theseus/rng.h>
#include <theseus/module.h>
#include <theseus/log.h>

#include <nrf.h>    /* __WFI() */

static const char *device_name = "Apache Asil";

/* adv_event() calls advertise(). */
static void advertise(void);

static void
set_ble_addr(void)
{
    int rc;
    ble_addr_t addr;

    /* Generate and set a non-resolvable private address. */
    rc = ble_hs_id_gen_rnd(1, &addr);
    printf("[APP] ble_hs_id_gen_rnd rc=%d addr=%02x:%02x:%02x:%02x:%02x:%02x type=%d\n",
           rc, addr.val[5], addr.val[4], addr.val[3], addr.val[2],
           addr.val[1], addr.val[0], addr.type);
    assert(rc == 0);

    rc = ble_hs_id_set_rnd(addr.val);
    printf("[APP] ble_hs_id_set_rnd rc=%d\n", rc);
    assert(rc == 0);
}

static int
adv_event(struct ble_gap_event *event, void *arg)
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

static void
advertise(void)
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
     * BLE_HS_FOREVER: the controller advertises autonomously and the host
     * never tears it down or relies on a host callout (a finite duration
     * would), which matters here since the bare-metal build has no kernel tick
     * to drive callouts. */
    rc = ble_gap_adv_start(BLE_OWN_ADDR_RANDOM, NULL, BLE_HS_FOREVER,
                           &adv_params, adv_event, NULL);
    printf("[APP] ble_gap_adv_start rc=%d\n", rc);
    assert(rc == 0);
    printf("[APP] advertise(): advertising ENABLED\n");
}

static void
on_sync(void)
{
    printf("[APP] on_sync: host/controller synced\n");
    set_ble_addr();
    advertise();
}

static void
on_reset(int reason)
{
    printf("[APP] on_reset reason=%d\n", reason);
}

int main(void)
{
    int rc;

    printf("\n[APP] ===== ble_adv_bm boot =====\n");

    /* No scheduler is ever started; all NimBLE + controller bring-up runs here
     * in thread mode. Controller init calls grtc_lfclk_init(), which performs
     * the real nrfx_grtc_init() MPSL needs. */
    printf("[APP] init start\n");

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

    rc = ble_svc_gap_device_name_set(device_name);
    printf("[APP] ble_svc_gap_device_name_set rc=%d\n", rc);
    assert(rc == 0);

    /* Bare-metal event loop. nimble_port_run() can't be used: it blocks on the
     * eventq with portMAX_DELAY, which needs a running scheduler (it derefs
     * pxCurrentTCB, NULL here). Instead poll non-blocking (tmo == 0 returns
     * immediately) and WFI when idle. Events are posted from ISR context by
     * sdc_callback_, so the queue is up to date after each interrupt. */
    struct ble_npl_eventq *evq = nimble_port_get_dflt_eventq();
    printf("[APP] entering bare-metal event loop\n");
    while (1) {
        struct ble_npl_event *ev = ble_npl_eventq_get(evq, 0);
        if (ev != NULL) {
            ble_npl_event_run(ev);
        } else {
            /* Sleep until the next interrupt (radio/SWI/etc.). */
            __WFI();
        }
    }

    return 0;
}
