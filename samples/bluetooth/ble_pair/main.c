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
#include "store/ram/ble_store_ram.h"
#include <theseus/module.h>
#include <theseus/log.h>

#include <FreeRTOS.h>
#include <task.h>

#define BLE_HOST_TASK_STACK_WORDS  ( 2048 )
#define BLE_HOST_TASK_PRIORITY     ( tskIDLE_PRIORITY + 2 )

static uint16_t conn_handle;
static const char *device_name = "MyNewt Sondre";

static ble_addr_t peer_id_addr;

/* adv_event() calls advertise(), so forward declaration is required */
static void advertise(void);

void ble_store_ram_init(void);

static int adv_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    int rc;

    switch (event->type) {
    case BLE_GAP_EVENT_ADV_COMPLETE:
        LOG("[APP] Advertising completed, termination code: %d\n",
                    event->adv_complete.reason);
        advertise();
        break;
    case BLE_GAP_EVENT_CONNECT:
        assert(event->connect.status == 0);
        LOG("[APP] connection %s; status = 0x%x\n",
                    event->connect.status == 0 ? "established" : "failed",
                    event->connect.status);
        conn_handle = event->connect.conn_handle;
        rc = ble_gap_security_initiate(conn_handle);
        if (rc != 0) LOG("[APP] ble_gap_security_initiate error: %d", rc);
        break;
    case BLE_GAP_EVENT_CONN_UPDATE_REQ:
        /* connected device requests update of connection parameters,
           and these are being filled in - NULL sets default values */
        LOG("[APP] updating conncetion parameters...\n");
        event->conn_update_req.conn_handle = conn_handle;
        event->conn_update_req.peer_params = NULL;
        LOG("[APP] connection parameters updated!\n");
        break;
    case BLE_GAP_EVENT_CONN_UPDATE:
        /* connected device requests update of connection parameters,
           and these are being filled in - NULL sets default values */
        LOG("[APP] updating conncetion parameters...\n");
        event->conn_update_req.conn_handle = conn_handle;
        event->conn_update_req.peer_params = NULL;
        LOG("[APP] connection parameters updated!\n");
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        LOG("[APP] disconnect; reason = 0x%x\n",
        event->disconnect.reason);

        /* reset conn_handle */
        conn_handle = BLE_HS_CONN_HANDLE_NONE;

        /* Connection terminated; resume advertising */
        advertise();
        break;
    case BLE_GAP_EVENT_ENC_CHANGE:
        rc = ble_gap_conn_find(event->enc_change.conn_handle, &desc);
        if (rc != 0) {
            LOG("[APP] ble_gap_conn_find error: %d", rc);
        } else {
            LOG("[APP] encrypted=%d authenticated=%d bonded=%d key_size=%d\n",
            desc.sec_state.encrypted, desc.sec_state.authenticated,
            desc.sec_state.bonded, desc.sec_state.key_size);
        }
        break;
    case BLE_GAP_EVENT_PAIRING_COMPLETE:
        break;
    case BLE_GAP_EVENT_PASSKEY_ACTION:
        struct ble_sm_io pk = { 0 };
        LOG("[APP] passkey action event; conn_handle=%d action=%d numcmp=%ld\n",
				    event->passkey.conn_handle,
				    event->passkey.params.action,
				    event->passkey.params.numcmp);

        switch (event->passkey.params.action)
        {
        case BLE_SM_IOACT_NONE:
            pk.action = BLE_SM_IOACT_NONE;

            rc = ble_sm_inject_io(event->passkey.conn_handle, &pk);
            if (rc != 0)
                LOG("[APP] ble_hs_hci_rand error: %d, in case %d\n",
                    rc, event->passkey.params.action);

            break;
        case BLE_SM_IOACT_OOB:
            LOG("[APP] BLE_SM_IOACT_OOB = %d Passkey action not supported\n",
                event->passkey.params.action);
            break;
        case BLE_SM_IOACT_INPUT:
            LOG("[APP] BLE_SM_IOACT_INPUT = %d Passkey action not supported\n",
                event->passkey.params.action);
            break;
        case BLE_SM_IOACT_DISP:
            pk.action = BLE_SM_IOACT_DISP;
            rc = ble_hs_hci_rand(&pk.passkey, sizeof(pk.passkey));
            if (rc != 0)
                LOG("[APP] ble_hs_hci_rand error: %d, in case %d\n", rc,
                    event->passkey.params.action);
            /* Modulo since max passkey value is 999999 */
            pk.passkey %= 1000000;

            rc = ble_sm_inject_io(event->passkey.conn_handle, &pk);
            if (rc != 0)
                LOG("[APP] ble_sm_inject_io error: %d, in case %d\n",
                    rc, event->passkey.params.action);
            LOG("[APP] PASSKEY = %ld", pk.passkey);
            break;
        case BLE_SM_IOACT_NUMCMP:
            /* Implementation does not seem to work */
            pk.action = BLE_SM_IOACT_NUMCMP;
            pk.numcmp_accept = 1;
            rc = ble_sm_inject_io(event->passkey.conn_handle, &pk);
            if (rc != 0)
                LOG("[APP] ble_sm_inject_io error: %d, in case %d\n",
                    rc, event->passkey.params.action);
            break;
        case BLE_SM_IOACT_OOB_SC:
            LOG("[APP] BLE_SM_IOACT_OOB_SC = %d Passkey action not supported\n",
                event->passkey.params.action);
            break;
        case BLE_SM_IOACT_MAX_PLUS_ONE:
            LOG("[APP] BLE_SM_IOACT_MAX_PLUS_ONE = %d Passkey action not supported\n",
                event->passkey.params.action);
            break;
        default:
            LOG("[APP] Unhandled passkey event: %d\n",
                event->passkey.params.action);
            break;
        }
        break;
    default:
        LOG("[APP] Advertising event not handled,"
                    "event code: %u\n", event->type);
        break;
    }
    return 0;
}

static void
advertise(void)
{
    int rc;

    /* set adv parameters */
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    /* advertising payload is split into advertising data and advertising
       response, because all data cannot fit into single packet; name of device
       is sent as response to scan request */
    struct ble_hs_adv_fields rsp_fields;

    /* fill all fields and parameters with zeros */
    memset(&adv_params, 0, sizeof(adv_params));
    memset(&fields, 0, sizeof(fields));
    memset(&rsp_fields, 0, sizeof(rsp_fields));

    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    fields.flags = BLE_HS_ADV_F_DISC_GEN |
                   BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128 = BLE_UUID128(BLE_UUID128_DECLARE(
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff));
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 0;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    rsp_fields.name = (uint8_t *)device_name;
    rsp_fields.name_len = strlen(device_name);
    rsp_fields.name_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        LOG("[APP] adv_set_fields failed; rc=%d\n", rc);
        return;
    }

    ble_gap_adv_rsp_set_fields(&rsp_fields);

    rc = ble_gap_adv_start(BLE_OWN_ADDR_RANDOM, NULL, BLE_HS_FOREVER,
                           &adv_params, adv_event, NULL);
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        LOG("[APP] adv_start failed; rc=%d\n", rc);
    }
}

static void
on_sync(void)
{
    int rc;

    /* generate new random static address */
    rc = ble_hs_id_gen_rnd(0, &peer_id_addr);
    assert(rc == 0);

    /* set generated address */
    rc = ble_hs_id_set_rnd(peer_id_addr.val);
    assert(rc == 0);
    /* begin advertising */
    advertise();
}

static void
on_reset(int reason)
{
    LOG("[APP] Resetting state; reason=%d\n", reason);
}

static void
ble_host_task(void *param)
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

    nimble_port_init();

    ble_svc_gap_init();

    ble_svc_gatt_init();

    ble_store_ram_init();

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
    int rc;

    printf("\n[APP] ===== ble_adv_rtos boot =====\n");
    BaseType_t ok = xTaskCreate(ble_host_task, "ble_hs",
                                BLE_HOST_TASK_STACK_WORDS, NULL,
                                BLE_HOST_TASK_PRIORITY, NULL);
    printf("[APP] xTaskCreate(ble_hs) ok=%ld\n", (long)ok);
    assert(ok == pdPASS);

    printf("[APP] starting scheduler\n");
    vTaskStartScheduler();

    while (1) {
    }

    return 0;
}
