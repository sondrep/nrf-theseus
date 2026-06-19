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

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

/* BLE */
#include <nimble/transport/hci_h4.h>

#include "nimble/ble.h"
#include "nimble/hci_common.h"
#include "nimble/nimble_npl.h"
#include "nimble/nimble_opt.h"
#include "nimble/transport.h"
#include "os/os_mbuf.h"
#include "mpsl.h"
#include "sdc.h"
#include "sdc_hci.h"
#include "sdc_hci_cmd_controller_baseband.h"
#include "sdc_hci_cmd_info_params.h"
#include "sdc_hci_cmd_le.h"
#include "sdc_hci_cmd_link_control.h"
#include "sdc_hci_cmd_status_params.h"
#include "sdc_soc.h"
#include <theseus/rng.h>
#include <nrfx.h>
#include <theseus/log.h>
#include <nrfx_grtc.h>

#define BIT(n) (1UL << (n))
#define BIT_MASK(n) (BIT(n) - 1UL)
#define BT_ACL_HANDLE_MASK BIT_MASK(12)
#define bt_acl_handle(h) ((h) & BT_ACL_HANDLE_MASK)
#define bt_acl_flags(h) ((h) >> 12)
#define bt_acl_flags_bc(f) ((f) >> 2)
#define bt_acl_flags_pb(f) ((f) & BIT_MASK(2))

void RADIO_0_IRQHandler(void){
    MPSL_IRQ_RADIO_Handler();
}


void GRTC_3_IRQHandler(void){
    MPSL_IRQ_RTC0_Handler();
}

void TIMER10_IRQHandler(void){
    MPSL_IRQ_TIMER0_Handler();
}
void CLOCK_POWER_IRQHandler(void){
    MPSL_IRQ_CLOCK_Handler();
}

static SemaphoreHandle_t mpsl_lp_sem;

void SWI03_IRQHandler(void){
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(mpsl_lp_sem, &woken);
    portYIELD_FROM_ISR(woken);
}

/* Task to take sdc callback out of ISR context */
static void mpsl_lp_task(void *arg){
    for (;;) {
        xSemaphoreTake(mpsl_lp_sem, portMAX_DELAY);

        /* The sdc callback is executed in the same context as mpsl_low_priority_process().
         * therefore it can't be called in an ISR.
         */
        mpsl_low_priority_process();
    }
}

void mpsl_low_latency_release_callback(void){

}

void mpsl_low_latency_acquire_callback(void){

}

int ble_transport_to_ll_iso_impl(struct os_mbuf *om) {
  /* Flatten the mbuf chain into one contiguous buffer, prefixed with the H4 type byte. */
  size_t len = 1;
  for (struct os_mbuf *m = om; m; m = SLIST_NEXT(m, om_next)) {
    len += m->om_len;
  }

  uint8_t *buf = malloc(len);

  buf[0] = HCI_H4_ISO;

  size_t i = 1;
  for (struct os_mbuf *m = om; m; m = SLIST_NEXT(m, om_next)) {
    memcpy(&buf[i], m->om_data, m->om_len);
    i += m->om_len;
  }

  int32_t err = sdc_hci_iso_data_put(buf);

  free(buf);

  return err;
}

static void fault_handler_(const char *file, const uint32_t line) {
  printf("MPSL fault: %s:%lu\n", file ? file : "?", (unsigned long)line);
  assert(0);
}

static void sdc_callback_(void) {
  int rc = 0;
  int sr;
  uint8_t buf[HCI_MSG_BUFFER_MAX_SIZE];
  uint8_t msg_type;
  while (1) {
    rc = sdc_hci_get(buf, &msg_type);
    if(rc != 0){
        break;
    }

    uint16_t hf, handle, len;
    uint8_t flags, pb, bc;

    uint16_t handle_buf = *((uint16_t *)buf);
    uint16_t length_buf = *((uint16_t *)buf + 1);

    hf = le16toh(handle_buf);
    handle = bt_acl_handle(hf);
    flags = bt_acl_flags(hf);
    pb = bt_acl_flags_pb(flags);  /* packet boundary */
    bc = bt_acl_flags_bc(flags);  /* broadcast */

    switch (msg_type) {
      case SDC_HCI_MSG_TYPE_NONE:
        len = buf[1] + 2;
        struct ble_hci_ev *hci_ev = ble_transport_alloc_evt(0);
        if (hci_ev == NULL) {
          continue;
        }
        struct ble_hci_ev_command_complete *cmd_complete = (void *)hci_ev->data;
        cmd_complete->status = 0;
        cmd_complete->num_packets = 1;
        cmd_complete->opcode = BLE_HCI_OPCODE_NOP;
        hci_ev->opcode = BLE_HCI_EVCODE_COMMAND_COMPLETE;
        hci_ev->length = sizeof(struct ble_hci_ev_command_complete);
        rc = ble_transport_to_hs_evt(hci_ev);
        if (rc != 0) {
          ble_transport_free(hci_ev);
          return;
        }
        return;
      case SDC_HCI_MSG_TYPE_DATA:
        len = le16toh(length_buf) + 4;
        struct os_mbuf *m_acl = ble_transport_alloc_acl_from_ll();
        rc = os_mbuf_append(m_acl, &buf[0], len);
        if (rc != 0) {
          assert(0);
          os_mbuf_free_chain(m_acl);
          return;
        }
        rc = ble_transport_to_hs_acl(m_acl);
        break;
      case SDC_HCI_MSG_TYPE_EVT:
        len = buf[1] + 2;

        /* Advertising reports are floody and safe to drop: route them to the
         * discardable pool so they can never exhaust RAM. Everything else
        * (command complete/status, conn events, ...) must use the reserved pool. */
        int discardable = (buf[0] == BLE_HCI_EVCODE_LE_META &&
                          (buf[2] == BLE_HCI_LE_SUBEV_ADV_RPT ||
                           buf[2] == BLE_HCI_LE_SUBEV_EXT_ADV_RPT));

        void *m_evt = ble_transport_alloc_evt(discardable);
        if (m_evt == NULL) {
            continue;
        }

        memcpy(m_evt, &buf[0], len);
        rc = ble_transport_to_hs_evt(m_evt);
        if (rc != 0) {
            ble_transport_free(m_evt);
            continue;
        }
        break;
      case SDC_HCI_MSG_TYPE_ISO:
        len = le16toh(length_buf) + 4;
        struct os_mbuf *m_iso = ble_transport_alloc_iso_from_ll();
        rc = os_mbuf_append(m_iso, &buf[0], len);
        if (rc != 0) {
          os_mbuf_free_chain(m_iso);
          return;
        }
        ble_transport_to_hs_iso(m_iso);
        break;
      default:
        assert(0);
        break;
    }
  }
  return;
}

static void rand_poll_(uint8_t *buf, uint8_t size){
  (void)theseus_PRNG_get(buf, size);
}

/* Start the LF clock + GRTC used by the SoftDevice Controller / MPSL for radio timing.
 * Doing it here, once, before mpsl_init() keeps the BLE samples from having to touch the GRTC themselves. */
static void grtc_lfclk_init(void)
{
    uint8_t grtc_channel;

    /* The GRTC is shared: MPSL uses it for radio timing and the FreeRTOS tick (vPortSetupTimerInterrupt() in port.c) uses it as the kernel tick.
     * Only one nrfx_grtc_init() is allowed; a second returns -EALREADY.
     *
     * If BLE is brought up from a task (scheduler already running),
     * the tick setup has already initialised and started the GRTC,
     * so defer to it. */
    if (nrfx_grtc_init_check()) {
        return;
    }

    /* Drive the GRTC from the external LF crystal (LFXO) for a stable clock. */
    nrfx_grtc_clock_source_set(NRF_GRTC_CLKSEL_LFXO);

    /* Bring up the GRTC step by step,
     * assert() halts loudly so a broken clock is caught here rather than failing silently later inside MPSL. */

    /* Init the driver (IRQ priority 0). */
    assert(nrfx_grtc_init(0) == 0);

    /* Start the 1 MHz system counter (claims an unused compare channel). */
    assert(nrfx_grtc_syscounter_start(false, &grtc_channel) == 0);

    /* Sanity check: counter is up and its value can be trusted. */
    assert(nrfx_grtc_ready_check());
}

void ble_transport_ll_init(void) {
  int32_t err;

  /* Bring up the LFXO + GRTC before the controller needs them. */
  grtc_lfclk_init();

  mpsl_lp_sem = xSemaphoreCreateBinary();
  if (mpsl_lp_sem == NULL) {
    return;
  }

  xTaskCreate(mpsl_lp_task, "mpsl_Task",
              configMINIMAL_STACK_SIZE + 1024, NULL,
              tskIDLE_PRIORITY + 3, NULL);

  err = mpsl_init(NULL, SWI03_IRQn, fault_handler_);
  if (err < 0) {
    return;
  }

  err = sdc_init(fault_handler_);
  if (err < 0) {
    return;
  }

  err = sdc_rand_source_register(&(sdc_rand_source_t){.rand_poll = rand_poll_});
  if (err < 0) {
    return;
  }

  sdc_support_adv();

  sdc_support_peripheral();

  sdc_support_le_2m_phy();

  /* Support HCI commands related to scanning */
  sdc_support_scan();


  sdc_cfg_t cfg = {.peripheral_count = {.count = 1}};
  err = sdc_cfg_set(SDC_DEFAULT_RESOURCE_CFG_TAG, SDC_CFG_TYPE_PERIPHERAL_COUNT, &cfg);
  if (err < 0) {
    return;
  }

  cfg.adv_count.count = 1;
  err = sdc_cfg_set(SDC_DEFAULT_RESOURCE_CFG_TAG, SDC_CFG_TYPE_ADV_COUNT, &cfg);
  if (err < 0) {
    return;
  }

  uint8_t *mem = malloc(err);

  err = sdc_enable(sdc_callback_, mem);
  if (err < 0) {
    return;
  }

  struct ble_hci_ev_command_complete_nop *ev;
  struct ble_hci_ev *hci_ev;

  hci_ev = ble_transport_alloc_evt(0);
  if (hci_ev) {
    /* Post a NOP command-complete so the host knows the controller is up. */
    hci_ev->opcode = BLE_HCI_EVCODE_COMMAND_COMPLETE;

    hci_ev->length = sizeof(*ev);
    ev = (void *)hci_ev->data;

    ev->num_packets = 1;
    ev->opcode = BLE_HCI_OPCODE_NOP;

    err = ble_transport_to_hs_evt(hci_ev);
  }

  return;
}

int ble_transport_to_ll_acl_impl(struct os_mbuf *om) {
  int sr;

  /* Flatten the mbuf chain into one contiguous buffer. */
  size_t len = 0;
  for (struct os_mbuf *m = om; m; m = SLIST_NEXT(m, om_next)) {
    len += m->om_len;
  }

  uint8_t *buf = malloc(len);

  size_t i = 0;
  for (struct os_mbuf *m = om; m; m = SLIST_NEXT(m, om_next)) {
    memcpy(&buf[i], m->om_data, m->om_len);
    i += m->om_len;
  }

  OS_ENTER_CRITICAL(sr);
  int32_t err = sdc_hci_data_put(buf);
  OS_EXIT_CRITICAL(sr);

  os_mbuf_free_chain(om);
  free(buf);

  return err;
}

int ble_transport_to_ll_cmd_impl(void *buf) {
  struct ble_hci_cmd *cmd = (struct ble_hci_cmd *)buf;

  uint16_t opcode = le16toh(cmd->opcode);
  uint8_t ogf = BLE_HCI_OGF(opcode);
  uint8_t ocf = BLE_HCI_OCF(opcode);
  void *data = (void *)(cmd->data);
  uint8_t err = NRF_EOPNOTSUPP;
  struct ble_hci_ev *hci_ev = (struct ble_hci_ev *)cmd;
  void *rspbuf = hci_ev->data + sizeof(struct ble_hci_ev_command_complete);
  bool generate_command_status = false;

  switch (ogf) {
    case 0x01:
      switch (ocf) {
        case BLE_HCI_OCF_DISCONNECT_CMD:
          err = sdc_hci_cmd_lc_disconnect(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_RD_REM_VER_INFO:
          err = sdc_hci_cmd_lc_read_remote_version_information(data);
          hci_ev->length = 0;
          break;

        default:

          break;
      }
      break;

    case 0x02:
      /* No commands mapped for this OGF. */
      break;

    case 0x03:
      switch (ocf) {
        case BLE_HCI_OCF_CB_SET_EVENT_MASK:
          err = sdc_hci_cmd_cb_set_event_mask(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_CB_RESET:
          err = sdc_hci_cmd_cb_reset();
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_CB_READ_TX_PWR:
          err = sdc_hci_cmd_cb_read_transmit_power_level(data, rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_cb_read_transmit_power_level_return_t);
          break;

        case BLE_HCI_OCF_CB_SET_CTLR_TO_HOST_FC:
          err = sdc_hci_cmd_cb_set_controller_to_host_flow_control(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_CB_HOST_BUF_SIZE:
          err = sdc_hci_cmd_cb_host_buffer_size(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_CB_HOST_NUM_COMP_PKTS:
          err = sdc_hci_cmd_cb_host_number_of_completed_packets(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_CB_SET_EVENT_MASK2:
          err = sdc_hci_cmd_cb_set_event_mask_page_2(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_CB_RD_AUTH_PYLD_TMO:
          err = sdc_hci_cmd_cb_read_authenticated_payload_timeout(data, rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_cb_read_authenticated_payload_timeout_return_t);
          break;

        case BLE_HCI_OCF_CB_WR_AUTH_PYLD_TMO:
          err = sdc_hci_cmd_cb_write_authenticated_payload_timeout(data, rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_cb_write_authenticated_payload_timeout_return_t);
          break;

        default:

          break;
      }
      break;

    case 0x04:
      switch (ocf) {
        case BLE_HCI_OCF_IP_RD_LOCAL_VER:
          err = sdc_hci_cmd_ip_read_local_version_information(rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_ip_read_local_version_information_return_t);
          break;

        case BLE_HCI_OCF_IP_RD_LOC_SUPP_CMD:
          /* This SDC variant does not export sdc_hci_cmd_ip_read_local_supported_commands(),
           * so return an all-zero (no optional commands) map.
           * The host needs the full 64-byte response,
           * a short reply triggers a host reset that tears advertising back down. */
          memset(rspbuf, 0, sizeof(sdc_hci_cmd_ip_read_local_supported_commands_return_t));
          err = 0;
          hci_ev->length = sizeof(sdc_hci_cmd_ip_read_local_supported_commands_return_t);
          break;

        case BLE_HCI_OCF_IP_RD_LOC_SUPP_FEAT:
          err = sdc_hci_cmd_ip_read_local_supported_features(rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_ip_read_local_supported_features_return_t);
          break;

        case BLE_HCI_OCF_IP_RD_BUF_SIZE:
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_IP_RD_BD_ADDR:
          err = sdc_hci_cmd_ip_read_bd_addr(rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_ip_read_bd_addr_return_t);
          break;

        default:

          break;
      }
      break;

    case 0x05:
      switch (ocf) {
        case BLE_HCI_OCF_RD_RSSI:
          err = sdc_hci_cmd_sp_read_rssi(data, rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_sp_read_rssi_return_t);
          break;

        default:

          break;
      }
      break;

    case 0x06:
      /* No commands mapped for this OGF. */
      break;

    case 0x08:
      switch (ocf) {
        case BLE_HCI_OCF_LE_SET_EVENT_MASK:
          err = sdc_hci_cmd_le_set_event_mask(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_RD_BUF_SIZE:
          err = sdc_hci_cmd_le_read_buffer_size(rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_read_buffer_size_return_t);
          break;

        case BLE_HCI_OCF_LE_RD_BUF_SIZE_V2:
          err = sdc_hci_cmd_le_read_buffer_size_v2(rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_read_buffer_size_v2_return_t);
          break;

        case BLE_HCI_OCF_LE_RD_LOC_SUPP_FEAT:
          err = sdc_hci_cmd_le_read_local_supported_features(rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_read_local_supported_features_return_t);
          break;

        case BLE_HCI_OCF_LE_SET_RAND_ADDR:
          err = sdc_hci_cmd_le_set_random_address(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_SET_ADV_PARAMS:
          err = sdc_hci_cmd_le_set_adv_params(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_RD_ADV_CHAN_TXPWR:
          err = sdc_hci_cmd_le_read_adv_physical_channel_tx_power(rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_read_adv_physical_channel_tx_power_return_t);
          break;

        case BLE_HCI_OCF_LE_SET_ADV_DATA:
          err = sdc_hci_cmd_le_set_adv_data(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_SET_SCAN_RSP_DATA:
          err = sdc_hci_cmd_le_set_scan_response_data(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_SET_ADV_ENABLE:
          err = sdc_hci_cmd_le_set_adv_enable(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_SET_SCAN_PARAMS:
          err = sdc_hci_cmd_le_set_scan_params(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_SET_SCAN_ENABLE:
          err = sdc_hci_cmd_le_set_scan_enable(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_CREATE_CONN:
          err = sdc_hci_cmd_le_create_conn(data);
          hci_ev->length = 0;
          generate_command_status = true;
          break;

        case BLE_HCI_OCF_LE_CREATE_CONN_CANCEL:
          err = sdc_hci_cmd_le_create_conn_cancel();
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_RD_WHITE_LIST_SIZE:
          err = sdc_hci_cmd_le_read_filter_accept_list_size(rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_read_filter_accept_list_size_return_t);
          break;

        case BLE_HCI_OCF_LE_CLEAR_WHITE_LIST:
          err = sdc_hci_cmd_le_clear_filter_accept_list();
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_ADD_WHITE_LIST:
          err = sdc_hci_cmd_le_add_device_to_filter_accept_list(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_RMV_WHITE_LIST:
          err = sdc_hci_cmd_le_remove_device_from_filter_accept_list(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_CONN_UPDATE:
          err = sdc_hci_cmd_le_conn_update(data);
          hci_ev->length = 0;
          generate_command_status = true;
          break;

        case BLE_HCI_OCF_LE_SET_HOST_CHAN_CLASS:
          err = sdc_hci_cmd_le_set_host_channel_classification(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_RD_CHAN_MAP:
          err = sdc_hci_cmd_le_read_channel_map(data, rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_read_channel_map_return_t);
          break;

        case BLE_HCI_OCF_LE_RD_REM_FEAT:
          err = sdc_hci_cmd_le_read_remote_features(data);
          hci_ev->length = 0;
          generate_command_status = true;
          break;

        case BLE_HCI_OCF_LE_ENCRYPT:
          err = sdc_hci_cmd_le_encrypt(data, rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_encrypt_return_t);
          break;

        case BLE_HCI_OCF_LE_RAND:
          err = sdc_hci_cmd_le_rand(rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_rand_return_t);
          break;

        case BLE_HCI_OCF_LE_START_ENCRYPT:
          err = sdc_hci_cmd_le_enable_encryption(data);
          hci_ev->length = 0;
          generate_command_status = true;
          break;

        case BLE_HCI_OCF_LE_LT_KEY_REQ_REPLY:
          err = sdc_hci_cmd_le_long_term_key_request_reply(data, rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_long_term_key_request_reply_return_t);
          break;

        case BLE_HCI_OCF_LE_LT_KEY_REQ_NEG_REPLY:
          err = sdc_hci_cmd_le_long_term_key_request_negative_reply(data, rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_long_term_key_request_negative_reply_return_t);
          break;

        case BLE_HCI_OCF_LE_RD_SUPP_STATES:
          /* TODO: undefined reference to sdc_hci_cmd_le_read_supported_states(),
           * falls through to the test-command stubs below. */

        case BLE_HCI_OCF_LE_RX_TEST:
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_TX_TEST:
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_TEST_END:
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_REM_CONN_PARAM_RR:
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_REM_CONN_PARAM_NRR:
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_SET_DATA_LEN:
          err = sdc_hci_cmd_le_set_data_length(data, rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_set_data_length_return_t);
          break;

        case BLE_HCI_OCF_LE_RD_SUGG_DEF_DATA_LEN:
          err = sdc_hci_cmd_le_read_suggested_default_data_length(rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_read_suggested_default_data_length_return_t);
          break;

        case BLE_HCI_OCF_LE_WR_SUGG_DEF_DATA_LEN:
          err = sdc_hci_cmd_le_write_suggested_default_data_length(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_RD_P256_PUBKEY:
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_GEN_DHKEY:
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_ADD_RESOLV_LIST:
          err = sdc_hci_cmd_le_add_device_to_resolving_list(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_RMV_RESOLV_LIST:
          err = sdc_hci_cmd_le_remove_device_from_resolving_list(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_CLR_RESOLV_LIST:
          err = sdc_hci_cmd_le_clear_resolving_list();
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_RD_RESOLV_LIST_SIZE:
          err = sdc_hci_cmd_le_read_resolving_list_size(rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_read_resolving_list_size_return_t);
          break;

        case BLE_HCI_OCF_LE_RD_PEER_RESOLV_ADDR:
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_RD_LOCAL_RESOLV_ADDR:
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_SET_ADDR_RES_EN:
          err = sdc_hci_cmd_le_set_address_resolution_enable(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_SET_RPA_TMO:
          err = sdc_hci_cmd_le_set_resolvable_private_address_timeout(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_RD_MAX_DATA_LEN:
          err = sdc_hci_cmd_le_read_max_data_length(rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_read_max_data_length_return_t);
          break;

        case BLE_HCI_OCF_LE_RD_PHY:
          err = sdc_hci_cmd_le_read_phy(data, rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_read_phy_return_t);
          break;

        case BLE_HCI_OCF_LE_SET_DEFAULT_PHY:
          err = sdc_hci_cmd_le_set_default_phy(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_SET_PHY:
          err = sdc_hci_cmd_le_set_phy(data);
          hci_ev->length = 0;
          generate_command_status = true;
          break;

        case BLE_HCI_OCF_LE_RX_TEST_V2:
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_TX_TEST_V2:
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_SET_ADV_SET_RND_ADDR:
          err = sdc_hci_cmd_le_set_adv_set_random_address(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_SET_EXT_ADV_PARAM:
          err = sdc_hci_cmd_le_set_ext_adv_params(data, rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_set_ext_adv_params_return_t);
          break;

        case BLE_HCI_OCF_LE_SET_EXT_ADV_DATA:
          err = sdc_hci_cmd_le_set_ext_adv_data(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_SET_EXT_SCAN_RSP_DATA:
          err = sdc_hci_cmd_le_set_ext_scan_response_data(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_SET_EXT_ADV_ENABLE:
          err = sdc_hci_cmd_le_set_ext_adv_enable(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_RD_MAX_ADV_DATA_LEN:
          err = sdc_hci_cmd_le_read_max_adv_data_length(rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_read_max_adv_data_length_return_t);
          break;

        case BLE_HCI_OCF_LE_RD_NUM_OF_ADV_SETS:
          err = sdc_hci_cmd_le_read_number_of_supported_adv_sets(rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_read_number_of_supported_adv_sets_return_t);
          break;

        case BLE_HCI_OCF_LE_REMOVE_ADV_SET:
          err = sdc_hci_cmd_le_remove_adv_set(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_CLEAR_ADV_SETS:
          err = sdc_hci_cmd_le_clear_adv_sets();
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_SET_PERIODIC_ADV_PARAMS:
          err = sdc_hci_cmd_le_set_periodic_adv_params(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_SET_PERIODIC_ADV_DATA:
          err = sdc_hci_cmd_le_set_periodic_adv_data(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_SET_PERIODIC_ADV_ENABLE:
          err = sdc_hci_cmd_le_set_periodic_adv_enable(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_SET_EXT_SCAN_PARAM:
          err = sdc_hci_cmd_le_set_ext_scan_params(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_SET_EXT_SCAN_ENABLE:
          err = sdc_hci_cmd_le_set_ext_scan_enable(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_EXT_CREATE_CONN:
          err = sdc_hci_cmd_le_ext_create_conn(data);
          hci_ev->length = 0;
          generate_command_status = true;
          break;

        case BLE_HCI_OCF_LE_PERIODIC_ADV_CREATE_SYNC:
          err = sdc_hci_cmd_le_periodic_adv_create_sync(data);
          hci_ev->length = 0;
          generate_command_status = true;
          break;

        case BLE_HCI_OCF_LE_PERIODIC_ADV_CREATE_SYNC_CANCEL:
          err = sdc_hci_cmd_le_periodic_adv_create_sync_cancel();
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_PERIODIC_ADV_TERM_SYNC:
          err = sdc_hci_cmd_le_periodic_adv_terminate_sync(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_ADD_DEV_TO_PERIODIC_ADV_LIST:
          err = sdc_hci_cmd_le_add_device_to_periodic_adv_list(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_REM_DEV_FROM_PERIODIC_ADV_LIST:
          err = sdc_hci_cmd_le_remove_device_from_periodic_adv_list(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_CLEAR_PERIODIC_ADV_LIST:
          err = sdc_hci_cmd_le_clear_periodic_adv_list();
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_RD_PERIODIC_ADV_LIST_SIZE:
          err = sdc_hci_cmd_le_read_periodic_adv_list_size(rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_read_periodic_adv_list_size_return_t);
          break;

        case BLE_HCI_OCF_LE_RD_TRANSMIT_POWER:
          err = sdc_hci_cmd_le_read_transmit_power(rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_read_transmit_power_return_t);
          break;

        case BLE_HCI_OCF_LE_RD_RF_PATH_COMPENSATION:
          err = sdc_hci_cmd_le_read_rf_path_compensation(rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_read_rf_path_compensation_return_t);
          break;

        case BLE_HCI_OCF_LE_WR_RF_PATH_COMPENSATION:
          err = sdc_hci_cmd_le_write_rf_path_compensation(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_SET_PRIVACY_MODE:
          err = sdc_hci_cmd_le_set_privacy_mode(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_RX_TEST_V3:
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_TX_TEST_V3:
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_SET_CONNLESS_CTE_TX_PARAMS:
          err = sdc_hci_cmd_le_set_connless_cte_transmit_params(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_SET_CONNLESS_CTE_TX_ENABLE:
          err = sdc_hci_cmd_le_set_connless_cte_transmit_enable(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_SET_CONNLESS_IQ_SAMPLING_ENABLE:
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_SET_CONN_CTE_RX_PARAMS:
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_SET_CONN_CTE_TX_PARAMS:
          err = sdc_hci_cmd_le_set_conn_cte_transmit_params(data, rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_set_conn_cte_transmit_params_return_t);
          break;

        case BLE_HCI_OCF_LE_SET_CONN_CTE_REQ_ENABLE:
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_SET_CONN_CTE_RESP_ENABLE:
          err = sdc_hci_cmd_le_conn_cte_response_enable(data, rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_conn_cte_response_enable_return_t);
          break;

        case BLE_HCI_OCF_LE_RD_ANTENNA_INFO:
          err = sdc_hci_cmd_le_read_antenna_information(rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_read_antenna_information_return_t);
          break;

        case BLE_HCI_OCF_LE_PERIODIC_ADV_RECEIVE_ENABLE:
          err = sdc_hci_cmd_le_set_periodic_adv_receive_enable(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_PERIODIC_ADV_SYNC_TRANSFER:
          err = sdc_hci_cmd_le_periodic_adv_sync_transfer(data, rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_periodic_adv_sync_transfer_return_t);
          break;

        case BLE_HCI_OCF_LE_PERIODIC_ADV_SET_INFO_TRANSFER:
          err = sdc_hci_cmd_le_periodic_adv_set_info_transfer(data, rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_periodic_adv_set_info_transfer_return_t);
          break;

        case BLE_HCI_OCF_LE_PERIODIC_ADV_SYNC_TRANSFER_PARAMS:
          err = sdc_hci_cmd_le_set_periodic_adv_sync_transfer_params(data, rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_set_periodic_adv_sync_transfer_params_return_t);
          break;

        case BLE_HCI_OCF_LE_SET_DEFAULT_SYNC_TRANSFER_PARAMS:
          err = sdc_hci_cmd_le_set_default_periodic_adv_sync_transfer_params(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_GENERATE_DHKEY_V2:
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_MODIFY_SCA:
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_READ_ISO_TX_SYNC:
          err = sdc_hci_cmd_le_read_iso_tx_sync(data, rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_read_iso_tx_sync_return_t);
          break;

        case BLE_HCI_OCF_LE_SET_CIG_PARAMS:
          err = sdc_hci_cmd_le_set_cig_params(data, rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_set_cig_params_return_t);
          break;

        case BLE_HCI_OCF_LE_SET_CIG_PARAMS_TEST:
          err = sdc_hci_cmd_le_set_cig_params_test(data, rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_set_cig_params_test_return_t);
          break;

        case BLE_HCI_OCF_LE_CREATE_CIS:
          err = sdc_hci_cmd_le_create_cis(data);
          hci_ev->length = 0;
          generate_command_status = true;
          break;

        case BLE_HCI_OCF_LE_REMOVE_CIG:
          err = sdc_hci_cmd_le_remove_cig(data, rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_remove_cig_return_t);
          break;

        case BLE_HCI_OCF_LE_ACCEPT_CIS_REQ:
          err = sdc_hci_cmd_le_accept_cis_request(data);
          hci_ev->length = 0;
          generate_command_status = true;
          break;

        case BLE_HCI_OCF_LE_REJECT_CIS_REQ:
          err = sdc_hci_cmd_le_reject_cis_request(data, rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_reject_cis_request_return_t);
          break;

        case BLE_HCI_OCF_LE_CREATE_BIG:
          err = sdc_hci_cmd_le_create_big(data);
          hci_ev->length = 0;
          generate_command_status = true;
          break;

        case BLE_HCI_OCF_LE_CREATE_BIG_TEST:
          err = sdc_hci_cmd_le_create_big_test(data);
          hci_ev->length = 0;
          generate_command_status = true;
          break;

        case BLE_HCI_OCF_LE_TERMINATE_BIG:
          err = sdc_hci_cmd_le_terminate_big(data);
          hci_ev->length = 0;
          generate_command_status = true;
          break;

        case BLE_HCI_OCF_LE_BIG_CREATE_SYNC:
          err = sdc_hci_cmd_le_big_create_sync(data);
          hci_ev->length = 0;
          generate_command_status = true;
          break;

        case BLE_HCI_OCF_LE_BIG_TERMINATE_SYNC:
          err = sdc_hci_cmd_le_big_terminate_sync(data, rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_big_terminate_sync_return_t);
          break;

        case BLE_HCI_OCF_LE_REQ_PEER_SCA:
          err = sdc_hci_cmd_le_request_peer_sca(data);
          hci_ev->length = 0;
          generate_command_status = true;
          break;

        case BLE_HCI_OCF_LE_SETUP_ISO_DATA_PATH:
          err = sdc_hci_cmd_le_setup_iso_data_path(data, rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_setup_iso_data_path_return_t);
          break;

        case BLE_HCI_OCF_LE_REMOVE_ISO_DATA_PATH:
          err = sdc_hci_cmd_le_remove_iso_data_path(data, rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_remove_iso_data_path_return_t);
          break;

        case BLE_HCI_OCF_LE_ISO_TRANSMIT_TEST:
          err = sdc_hci_cmd_le_iso_transmit_test(data, rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_iso_transmit_test_return_t);
          break;

        case BLE_HCI_OCF_LE_ISO_RECEIVE_TEST:
          err = sdc_hci_cmd_le_iso_receive_test(data, rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_iso_receive_test_return_t);
          break;

        case BLE_HCI_OCF_LE_ISO_READ_TEST_COUNTERS:
          err = sdc_hci_cmd_le_iso_read_test_counters(data, rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_iso_read_test_counters_return_t);
          break;

        case BLE_HCI_OCF_LE_ISO_TEST_END:
          err = sdc_hci_cmd_le_iso_test_end(data, rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_iso_test_end_return_t);
          break;

        case BLE_HCI_OCF_LE_SET_HOST_FEATURE:
          err = sdc_hci_cmd_le_set_host_feature(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_READ_ISO_LINK_QUALITY:
          err = sdc_hci_cmd_le_read_iso_link_quality(data, rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_read_iso_link_quality_return_t);
          break;

        case BLE_HCI_OCF_LE_ENH_READ_TRANSMIT_POWER_LEVEL:
          err = sdc_hci_cmd_le_enhanced_read_transmit_power_level(data, rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_enhanced_read_transmit_power_level_return_t);
          break;

        case BLE_HCI_OCF_LE_READ_REMOTE_TRANSMIT_POWER_LEVEL:
          err = sdc_hci_cmd_le_read_remote_transmit_power_level(data);
          hci_ev->length = 0;
          generate_command_status = true;
          break;

        case BLE_HCI_OCF_LE_SET_PATH_LOSS_REPORT_PARAM:
          err = sdc_hci_cmd_le_set_path_loss_reporting_params(data, rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_set_path_loss_reporting_params_return_t);
          break;

        case BLE_HCI_OCF_LE_SET_PATH_LOSS_REPORT_ENABLE:
          err = sdc_hci_cmd_le_set_path_loss_reporting_enable(data, rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_set_path_loss_reporting_enable_return_t);
          break;

        case BLE_HCI_OCF_LE_SET_TRANS_PWR_REPORT_ENABLE:
          err = sdc_hci_cmd_le_set_transmit_power_reporting_enable(data, rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_set_transmit_power_reporting_enable_return_t);
          break;

        case BLE_HCI_OCF_LE_SET_DEFAULT_SUBRATE:
          err = sdc_hci_cmd_le_set_default_subrate(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_SUBRATE_REQ:
          err = sdc_hci_cmd_le_subrate_request(data);
          hci_ev->length = 0;
          generate_command_status = true;
          break;

        case BLE_HCI_OCF_LE_SET_EXT_ADV_PARAM_V2:
          err = sdc_hci_cmd_le_set_ext_adv_params_v2(data, rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_set_ext_adv_params_v2_return_t);
          break;

        case BLE_HCI_OCF_LE_CS_RD_LOC_SUPP_CAP:
          err = sdc_hci_cmd_le_cs_read_local_supported_capabilities(rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_cs_read_local_supported_capabilities_return_t);
          break;

        case BLE_HCI_OCF_LE_CS_RD_REM_SUPP_CAP:
          err = sdc_hci_cmd_le_cs_read_remote_supported_capabilities(data);
          hci_ev->length = 0;
          generate_command_status = true;
          break;

        case BLE_HCI_OCF_LE_CS_WR_CACHED_REM_SUPP_CAP:
          err = sdc_hci_cmd_le_cs_write_cached_remote_supported_capabilities(data, rspbuf);
          hci_ev->length =
              sizeof(sdc_hci_cmd_le_cs_write_cached_remote_supported_capabilities_return_t);
          break;

        case BLE_HCI_OCF_LE_CS_SEC_ENABLE:
          err = sdc_hci_cmd_le_cs_security_enable(data);
          hci_ev->length = 0;
          generate_command_status = true;
          break;

        case BLE_HCI_OCF_LE_CS_SET_DEF_SETTINGS:
          err = sdc_hci_cmd_le_cs_set_default_settings(data, rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_cs_set_default_settings_return_t);
          break;

        case BLE_HCI_OCF_LE_CS_RD_REM_FAE:
          err = sdc_hci_cmd_le_cs_read_remote_fae_table(data);
          hci_ev->length = 0;
          generate_command_status = true;
          break;

        case BLE_HCI_OCF_LE_CS_WR_CACHED_REM_FAE:
          err = sdc_hci_cmd_le_cs_write_cached_remote_fae_table(data, rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_cs_write_cached_remote_fae_table_return_t);
          break;

        case BLE_HCI_OCF_LE_CS_CREATE_CONFIG:
          err = sdc_hci_cmd_le_cs_create_config(data);
          hci_ev->length = 0;
          generate_command_status = true;
          break;

        case BLE_HCI_OCF_LE_CS_REMOVE_CONFIG:
          err = sdc_hci_cmd_le_cs_remove_config(data);
          hci_ev->length = 0;
          generate_command_status = true;
          break;

        case BLE_HCI_OCF_LE_CS_SET_CHAN_CLASS:
          err = sdc_hci_cmd_le_cs_set_channel_classification(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_CS_SET_PROC_PARAMS:
          err = sdc_hci_cmd_le_cs_set_procedure_params(data, rspbuf);
          hci_ev->length = sizeof(sdc_hci_cmd_le_cs_set_procedure_params_return_t);
          break;

        case BLE_HCI_OCF_LE_CS_PROC_ENABLE:
          err = sdc_hci_cmd_le_cs_procedure_enable(data);
          hci_ev->length = 0;
          generate_command_status = true;
          break;

        case BLE_HCI_OCF_LE_CS_TEST:
          err = sdc_hci_cmd_le_cs_test(data);
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_LE_CS_TEST_END:
          err = sdc_hci_cmd_le_cs_test_end();
          hci_ev->length = 0;
          generate_command_status = true;
          break;

        default:

          break;
      }
      break;

    case 0x3F:

      switch (ocf) {
        case BLE_HCI_OCF_VS_RD_STATIC_ADDR:
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_VS_SET_TX_PWR:
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_VS_CSS_CONFIGURE:
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_VS_CSS_ENABLE:
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_VS_CSS_SET_NEXT_SLOT:
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_VS_CSS_SET_CONN_SLOT:
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_VS_CSS_READ_CONN_SLOT:
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_VS_SET_DATA_LEN:
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_VS_SET_ANTENNA:
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_VS_SET_LOCAL_IRK:
          hci_ev->length = 0;
          break;

        case BLE_HCI_OCF_VS_SET_SCAN_CFG:
          hci_ev->length = 0;
          break;

        default:

          break;
      }

      break;

    default:

      break;
  }

  if (generate_command_status){
    struct ble_hci_ev_command_status *cmd_status = (void *)hci_ev->data;
    hci_ev->opcode = BLE_HCI_EVCODE_COMMAND_STATUS;
    hci_ev->length = sizeof(struct ble_hci_ev_command_status);
    cmd_status->status = err;
    cmd_status->num_packets = 1;
    cmd_status->opcode = htole16(opcode);
  } else {
    struct ble_hci_ev_command_complete *cmd_complete = (void *)hci_ev->data;
    hci_ev->opcode = BLE_HCI_EVCODE_COMMAND_COMPLETE;
    hci_ev->length += sizeof(struct ble_hci_ev_command_complete);
    cmd_complete->status = err;
    cmd_complete->num_packets = 1;
    cmd_complete->opcode = htole16(opcode);
  }

  ble_transport_to_hs_evt(hci_ev);

  return 0;
}
