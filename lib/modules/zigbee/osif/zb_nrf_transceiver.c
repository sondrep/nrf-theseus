/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "mpsl.h"
#include "zb_nrf_platform.h"
#include <string.h>
#include <FreeRTOS.h>
#include <nrf_802154.h>
#include <nrf_802154_const.h>
#include <nrf_802154_nrfx_addons.h>
#include <nrf_802154_types.h>
#include <queue.h>
#include <semphr.h>
#include <theseus/log.h>
#include <theseus/module.h>
#include <zb_macll.h>
#include <zb_transceiver.h>
#include <zboss_api.h>

#if defined(CONFIG_NRF_802154_CALLBACKS_DISPATCHER)
#include <net/nrf_802154_callbacks_dispatcher.h>
#endif

#if defined(CONFIG_NRF_802154_SER_HOST)
#include "nrf_802154_serialization_error.h"
#endif

#if !defined NRF_802154_FRAME_TIMESTAMP_ENABLED || !NRF_802154_FRAME_TIMESTAMP_ENABLED
#warning Must define NRF_802154_FRAME_TIMESTAMP_ENABLED!
#endif

#define FIFO_QUEUE_SIZE 10

void RADIO_0_IRQHandler(void)
{
	MPSL_IRQ_RADIO_Handler();
}

void GRTC_3_IRQHandler(void)
{
	MPSL_IRQ_RTC0_Handler();
}

void TIMER10_IRQHandler(void)
{
	MPSL_IRQ_TIMER0_Handler();
}

void CLOCK_POWER_IRQHandler(void)
{
	MPSL_IRQ_CLOCK_Handler();
}

/** Map ED in dBm to ZBOSS 0..255 scale. */
static uint8_t zboss_normalize_ed_dbm(int8_t ed_dbm)
{
	int32_t ed = ed_dbm;
	const int32_t min_dbm = ED_DBM_MIN;
	const int32_t max_dbm = ED_DBM_MAX;

	if (ed <= min_dbm) {
		return 0U;
	}
	if (ed >= max_dbm) {
		return UINT8_MAX;
	}

	return (uint8_t)(UINT8_MAX * (ed - min_dbm) / (max_dbm - min_dbm));
}

enum zb_radio_state {
	ZB_RADIO_STATE_SLEEP,
	ZB_RADIO_STATE_RECEIVE,
	ZB_RADIO_STATE_TRANSMIT,
};

struct zboss_rx_frame {
	void *fifo_reserved;
	uint8_t *psdu;
	int8_t power;
	uint8_t lqi;
	uint64_t time;
	bool ack_fpb;
};

struct nrf5_data {
	enum zb_radio_state state;

	int8_t tx_power;
	uint8_t channel;
	bool promiscuous;

	struct {
		struct zboss_rx_frame frames[CONFIG_NRF_802154_RX_BUFFERS + 1];
		QueueHandle_t fifo;
		bool last_frame_ack_fpb;
	} rx;

	struct {
		uint8_t *psdu;
	} tx;

	struct {
		uint32_t time_us;
		uint8_t value;
	} energy_detection;

	SemaphoreHandle_t rssi_wait;
};

static struct nrf5_data nrf5_data;

static void semaphore_give_check_isr(void)
{
	if ((zb_bool_t)(__get_IPSR() != 0)) {
		BaseType_t woken = pdFALSE;

		xSemaphoreGiveFromISR(nrf5_data.rssi_wait, &woken);
		portYIELD_FROM_ISR(woken);
	} else {
		xSemaphoreGive(nrf5_data.rssi_wait);
	}
}

#if defined(CONFIG_NRF_802154_SER_HOST)
static void tx_done_ack_work_fn(void *pvParameter1, uint32_t ulParameter2);
#endif

void zigbee_nrf_802154_radio_init(void)
{
	memset(&nrf5_data, 0, sizeof(nrf5_data));
	nrf5_data.rx.fifo = xQueueCreate(FIFO_QUEUE_SIZE, sizeof(struct zboss_rx_frame *));
	nrf5_data.rssi_wait = xSemaphoreCreateBinary();
	semaphore_give_check_isr();
	nrf5_data.state = ZB_RADIO_STATE_SLEEP;

	LOG("Zigbee radio initialized\n");
}

void zb_trans_hw_init(void)
{
	nrf_802154_src_addr_matching_method_set(NRF_802154_SRC_ADDR_MATCH_ZIGBEE);
}

void zb_trans_set_pan_id(zb_uint16_t pan_id)
{
	LOG("%s: 0x%x\n", __func__, pan_id);
	nrf_802154_pan_id_set((zb_uint8_t *)(&pan_id));
}

void zb_trans_set_long_addr(zb_ieee_addr_t long_addr)
{
	LOG("%s: 0x%llx\n", __func__, (uint64_t)*long_addr);
	nrf_802154_extended_address_set(long_addr);
}

void zb_trans_set_short_addr(zb_uint16_t addr)
{
	LOG("%s: 0x%x\n", __func__, addr);
	nrf_802154_short_address_set((uint8_t *)(&addr));
}

static int zboss_energy_detection_start(uint32_t time_us)
{
	nrf5_data.energy_detection.time_us = time_us;

	if (!nrf_802154_energy_detection(time_us)) {
		return -EBUSY;
	}

	return 0;
}

void zb_trans_start_get_rssi(zb_uint8_t scan_duration_bi)
{
	int err;
	uint32_t time_us = ZB_TIME_BEACON_INTERVAL_TO_USEC(scan_duration_bi);

	LOG("%s: %d us\n", __func__, time_us);

	err = zboss_energy_detection_start(time_us);

	while (err != 0) {
		LOG("Energy detection start failed, retrying");
		vTaskDelay(pdMS_TO_TICKS(500));
		err = zboss_energy_detection_start(time_us);
	}
}

void zb_trans_get_rssi(zb_uint8_t *rssi_value_p)
{
	LOG("%s\n", __func__);

	/* Blocking implementation: wait for energy detection to complete.
	 * The semaphore is signaled by nrf_802154_energy_detected() callback
	 * or by nrf_802154_energy_detection_failed() after retry attempt.
	 */
	xSemaphoreTake(nrf5_data.rssi_wait, portMAX_DELAY);
	*rssi_value_p = nrf5_data.energy_detection.value;
	LOG("Energy detected: %d\n", *rssi_value_p);
}

zb_ret_t zb_trans_set_channel(zb_uint8_t channel_number)
{
	LOG("%s: %d\n", __func__, channel_number);
	nrf_802154_channel_set(channel_number);
	return RET_OK;
}

void zb_trans_set_tx_power(zb_int8_t power)
{
	LOG("%s: %d\n", __func__, power);
	nrf_802154_tx_power_set(power);
}

void zb_trans_get_tx_power(zb_int8_t *power)
{
	*power = (zb_int8_t)nrf_802154_tx_power_get();
	LOG("%s: %d\n", __func__, *power);
}

void zb_trans_set_pan_coord(zb_bool_t enabled)
{
	LOG("%s: %d\n", __func__, enabled);
	nrf_802154_pan_coord_set((bool)enabled);
}

void zb_trans_set_promiscuous_mode(zb_bool_t enabled)
{
	LOG("%s: %d\n", __func__, enabled);
	nrf_802154_promiscuous_set((bool)enabled);
}

void zb_trans_enter_receive(void)
{
	LOG("%s\n", __func__);

	while (!nrf_802154_receive()) {
		LOG("Radio could not change state to receive, retry\n");
	}
	nrf5_data.state = ZB_RADIO_STATE_RECEIVE;
}

void zb_trans_enter_sleep(void)
{
	LOG("%s\n", __func__);
	while (nrf_802154_sleep_if_idle() != NRF_802154_SLEEP_ERROR_NONE) {
		LOG("Radio could not change state to sleep, retry\n");
	}
	nrf5_data.state = ZB_RADIO_STATE_SLEEP;
}

zb_bool_t zb_trans_is_receiving(void)
{
	zb_bool_t is_receiv = (nrf5_data.state == ZB_RADIO_STATE_RECEIVE) ? ZB_TRUE : ZB_FALSE;
	LOG("%s: %d\n", __func__, is_receiv);
	return is_receiv;
}

zb_bool_t zb_trans_is_active(void)
{
	zb_bool_t is_active = (nrf5_data.state != ZB_RADIO_STATE_SLEEP) ? ZB_TRUE : ZB_FALSE;
	LOG("%s: %d\n", __func__, is_active);
	return is_active;
}

zb_bool_t zb_trans_transmit(zb_uint8_t wait_type, zb_time_t tx_at, zb_uint8_t *tx_buf,
			    zb_uint8_t current_channel)
{
	LOG("%s: channel %d\n", __func__, current_channel);
	nrf_802154_tx_error_t result;
	nrf_802154_capabilities_t caps = nrf_802154_capabilities_get();

#ifndef ZB_ENABLE_ZGP_DIRECT
	(void)tx_at;
	(void)current_channel;
#endif

	nrf5_data.state = ZB_RADIO_STATE_TRANSMIT;

	switch (wait_type) {
	case ZB_MAC_TX_WAIT_CSMACA:
		if (caps & NRF_802154_CAPABILITY_CSMA) {
			nrf_802154_transmit_csma_ca_metadata_t csma_metadata = {
				.frame_props =
					{
						.is_secured = false,
						.dynamic_data_is_set = false,
					},
			};
			result = nrf_802154_transmit_csma_ca_raw(tx_buf, &csma_metadata);
		} else {
			nrf_802154_transmit_metadata_t cca_metadata = {
				.frame_props =
					{
						.is_secured = false,
						.dynamic_data_is_set = false,
					},
				.cca = true,
			};
			result = nrf_802154_transmit_raw(tx_buf, &cca_metadata);
		}
		break;

#ifdef ZB_ENABLE_ZGP_DIRECT
	case ZB_MAC_TX_WAIT_ZGP:
		if (!(caps & NRF_802154_CAPABILITY_DELAYED_TX)) {
			LOG("NRF_802154_CAPABILITY_DELAYED_TX not supported\n");
			nrf5_data.state = ZB_RADIO_STATE_RECEIVE;
			return ZB_FALSE;
		}
		nrf_802154_transmit_at_metadata_t at_metadata = {
			.frame_props =
				{
					.is_secured = false,
					.dynamic_data_is_set = false,
				},
			.cca = true,
		};
		result = nrf_802154_transmit_raw_at(tx_buf, tx_at, &at_metadata);
		break;
#endif

	case ZB_MAC_TX_WAIT_NONE: {
		nrf_802154_transmit_metadata_t tx_metadata = {
			.frame_props =
				{
					.is_secured = false,
					.dynamic_data_is_set = false,
				},
			.cca = false,
		};
		result = nrf_802154_transmit_raw(tx_buf, &tx_metadata);
		break;
	}

	default:
		LOG("Invalid wait_type: %d\n", wait_type);
		nrf5_data.state = ZB_RADIO_STATE_RECEIVE;
		return ZB_FALSE;
	}

	LOG("TX request result=%d, wait_type=%u\n", result, wait_type);
	return (result == NRF_802154_TX_ERROR_NONE) ? ZB_TRUE : ZB_FALSE;
}

void zb_trans_buffer_free(zb_uint8_t *buf)
{
	LOG("%s\n", __func__);
	nrf_802154_buffer_free_raw(buf);
}

zb_bool_t zb_trans_set_pending_bit(zb_uint8_t *addr, zb_bool_t value, zb_bool_t extended)
{
	LOG("%s: value=%d\n", __func__, value);

	if (!value) {
		return (zb_bool_t)nrf_802154_pending_bit_for_addr_set((const uint8_t *)addr,
								      (bool)extended);
	} else {
		return (zb_bool_t)nrf_802154_pending_bit_for_addr_clear((const uint8_t *)addr,
									(bool)extended);
	}
}

void zb_trans_src_match_tbl_drop(void)
{
	nrf_802154_pending_bit_for_addr_reset(false);
	nrf_802154_pending_bit_for_addr_reset(true);
}

zb_time_t osif_sub_trans_timer(zb_time_t t2, zb_time_t t1)
{
	return ZB_TIME_SUBTRACT(t2, t1);
}

zb_bool_t zb_trans_rx_pending(void)
{
	struct zboss_rx_frame *rx_frame;
	BaseType_t ret = xQueuePeek(nrf5_data.rx.fifo, &rx_frame, pdMS_TO_TICKS(0));
	/* Checks if queue is empty or not, ret is pdTRUE if there is an entry on the queue */
	if (ret == pdTRUE) {
		return ZB_TRUE;
	} else {
		return ZB_FALSE;
	}
}

zb_uint8_t zb_trans_get_next_packet(zb_bufid_t buf)
{
	LOG("%s\n", __func__);
	zb_uint8_t *data_ptr;
	zb_uint8_t length = 0;

	if (!buf) {
		return 0;
	}

	struct zboss_rx_frame *rx_frame = NULL;
	BaseType_t ret = xQueueReceive(nrf5_data.rx.fifo, &rx_frame, portMAX_DELAY);
	if (ret != pdTRUE) {
		LOG("zb_trans_get_next_packet queue timeout.\n");
	} else {
		LOG("zb_trans_get_next_packet successfully got next packet.\n");
	}
	if (!rx_frame) {
		return 0;
	}

	length = rx_frame->psdu[0];
	data_ptr = zb_buf_initial_alloc(buf, length);
	ZB_MEMCPY(data_ptr, (void const *)(rx_frame->psdu + 1), length);

	zb_macll_metadata_t *metadata = ZB_MACLL_GET_METADATA(buf);
	metadata->lqi = rx_frame->lqi;
	metadata->power = rx_frame->power;

	*ZB_BUF_GET_PARAM(buf, zb_time_t) = (zb_time_t)rx_frame->time;
	zb_macll_set_received_data_status(buf, rx_frame->ack_fpb);

	nrf_802154_buffer_free_raw(rx_frame->psdu);
	rx_frame->psdu = NULL;

	return 1;
}

zb_ret_t zb_trans_cca(void)
{
	bool cca_result = nrf_802154_cca();
	return cca_result ? RET_OK : RET_BUSY;
}

#if defined(CONFIG_NRF_802154_SER_HOST)
static void tx_done_ack_work_fn(void *pvParameter1, uint32_t ulParameter2)
{
	(void)ulParameter2;
	zb_uint8_t *ack = pvParameter1;

	zb_macll_transmitted_raw(ack);
	zigbee_event_notify(ZIGBEE_EVENT_TX_DONE);
}
#endif

/* nRF 802.15.4 driver callbacks - modern API with metadata structures */

static void zigbee_nrf_802154_transmitted_raw(uint8_t *p_frame,
					      const nrf_802154_transmit_done_metadata_t *p_metadata)
{
	(void)p_frame;

	uint8_t *ack = p_metadata->data.transmitted.p_ack;

	nrf5_data.state = ZB_RADIO_STATE_RECEIVE;

#if defined(CONFIG_NRF_802154_SER_HOST)
	if (ack != NULL) {
		xTimerPendFunctionCall(&tx_done_ack_work_fn, ack, NULL, pdMS_TO_TICKS(0));
		return;
	}
#endif
	zb_macll_transmitted_raw(ack);
	zigbee_event_notify(ZIGBEE_EVENT_TX_DONE);
}

static void zigbee_nrf_802154_transmit_failed(uint8_t *p_frame, nrf_802154_tx_error_t error,
					      const nrf_802154_transmit_done_metadata_t *p_metadata)
{
	(void)p_frame;
	(void)p_metadata;
	LOG("Transmit failed error: %d\n", error);

	switch (error) {
	case NRF_802154_TX_ERROR_NO_MEM:
	case NRF_802154_TX_ERROR_ABORTED:
	case NRF_802154_TX_ERROR_TIMESLOT_DENIED:
	case NRF_802154_TX_ERROR_TIMESLOT_ENDED:
	case NRF_802154_TX_ERROR_BUSY_CHANNEL:
		zb_macll_transmit_failed(ZB_TRANS_CHANNEL_BUSY_ERROR);
		break;

	case NRF_802154_TX_ERROR_INVALID_ACK:
	case NRF_802154_TX_ERROR_NO_ACK:
		zb_macll_transmit_failed(ZB_TRANS_NO_ACK);
		break;
	default:
		break;
	}

	nrf5_data.state = ZB_RADIO_STATE_RECEIVE;
	zigbee_event_notify(ZIGBEE_EVENT_TX_FAILED);
}

static void zigbee_nrf_802154_tx_ack_started(const uint8_t *p_data)
{
	nrf5_data.rx.last_frame_ack_fpb = p_data[FRAME_PENDING_OFFSET] & FRAME_PENDING_BIT;
}

static void zigbee_nrf_802154_received_timestamp_raw(uint8_t *p_data, int8_t power, uint8_t lqi,
						     uint64_t time)
{
	struct zboss_rx_frame *rx_frame_free_slot = NULL;

	for (uint32_t i = 0; i < sizeof(nrf5_data.rx.frames) / sizeof(nrf5_data.rx.frames[0]);
	     i++) {
		if (nrf5_data.rx.frames[i].psdu == NULL) {
			rx_frame_free_slot = &nrf5_data.rx.frames[i];
			break;
		}
	}

	if (rx_frame_free_slot == NULL) {
		LOG("Not enough rx frames allocated\n");
		return;
	}

	rx_frame_free_slot->psdu = p_data;
	rx_frame_free_slot->power = power;
	rx_frame_free_slot->lqi = lqi;
	rx_frame_free_slot->time = time;

	if (p_data[ACK_REQUEST_OFFSET] & ACK_REQUEST_BIT) {
		rx_frame_free_slot->ack_fpb = nrf5_data.rx.last_frame_ack_fpb;
	} else {
		rx_frame_free_slot->ack_fpb = false;
	}

	BaseType_t woken = pdFALSE;
	nrf5_data.rx.last_frame_ack_fpb = false;
	xQueueSendToBackFromISR(nrf5_data.rx.fifo, &rx_frame_free_slot, &woken);
	portYIELD_FROM_ISR(woken);

	zb_macll_set_rx_flag();
	zb_macll_set_trans_int();
	zigbee_event_notify(ZIGBEE_EVENT_RX_DONE);
}

static void zigbee_nrf_802154_receive_failed(nrf_802154_rx_error_t error, uint32_t id)
{
	(void)error;
	(void)id;
	nrf5_data.rx.last_frame_ack_fpb = false;
}

static void zigbee_nrf_802154_energy_detected(const nrf_802154_energy_detected_t *p_result)
{
	nrf5_data.energy_detection.value = zboss_normalize_ed_dbm(p_result->ed_dbm);
	semaphore_give_check_isr();
}

static void zigbee_nrf_802154_energy_detection_failed(nrf_802154_ed_error_t error)
{
	LOG("Detection failed error: %d\n", error);

	int err = zboss_energy_detection_start(nrf5_data.energy_detection.time_us);

	if (err != 0) {
		LOG("Failed to restart energy detection after failure\n");
		nrf5_data.energy_detection.value = UINT8_MAX;
		semaphore_give_check_isr();
	}
}

#if defined(CONFIG_NRF_802154_SER_HOST)
static void zigbee_nrf_802154_serialization_error(const nrf_802154_ser_err_data_t *err)
{
	(void)err;
}
#endif

#ifdef CONFIG_NRF_802154_CALLBACKS_DISPATCHER
static void zigbee_nrf_802154_release_rx_frames(void)
{
	for (size_t i = 0; i < ARRAY_SIZE(nrf5_data.rx.frames); i++) {
		if (nrf5_data.rx.frames[i].psdu != NULL) {
			nrf_802154_buffer_free_raw(nrf5_data.rx.frames[i].psdu);
			nrf5_data.rx.frames[i].psdu = NULL;
		}
	}
}

static void zigbee_nrf_802154_radio_deinit(void)
{
	zigbee_nrf_802154_release_rx_frames();
}

static const struct nrf_802154_callbacks zigbee_802154_callbacks = {
	.init = zigbee_nrf_802154_radio_init,
	.deinit = zigbee_nrf_802154_radio_deinit,
	.received_timestamp_raw = zigbee_nrf_802154_received_timestamp_raw,
	.receive_failed = zigbee_nrf_802154_receive_failed,
	.tx_ack_started = zigbee_nrf_802154_tx_ack_started,
	.transmitted_raw = zigbee_nrf_802154_transmitted_raw,
	.transmit_failed = zigbee_nrf_802154_transmit_failed,
	.energy_detected = zigbee_nrf_802154_energy_detected,
	.energy_detection_failed = zigbee_nrf_802154_energy_detection_failed,
#if defined(CONFIG_NRF_802154_SER_HOST)
	.serialization_error = zigbee_nrf_802154_serialization_error,
#endif
};

NRF_802154_CALLBACKS_DISPATCHER_REGISTER(zigbee, zigbee_802154_callbacks);

#else
/* Translate the nrf_802154 callbacks to zigbee_nrf_802154_callbacks for
 * backward compatibility */
void nrf_802154_received_timestamp_raw(uint8_t *data, int8_t power, uint8_t lqi, uint64_t time)
{
	zigbee_nrf_802154_received_timestamp_raw(data, power, lqi, time);
}

void nrf_802154_receive_failed(nrf_802154_rx_error_t error, uint32_t id)
{
	zigbee_nrf_802154_receive_failed(error, id);
}

void nrf_802154_tx_ack_started(const uint8_t *data)
{
	zigbee_nrf_802154_tx_ack_started(data);
}

void nrf_802154_transmitted_raw(uint8_t *frame, const nrf_802154_transmit_done_metadata_t *metadata)
{
	zigbee_nrf_802154_transmitted_raw(frame, metadata);
}

void nrf_802154_transmit_failed(uint8_t *frame, nrf_802154_tx_error_t error,
				const nrf_802154_transmit_done_metadata_t *metadata)
{
	zigbee_nrf_802154_transmit_failed(frame, error, metadata);
}

void nrf_802154_energy_detected(const nrf_802154_energy_detected_t *result)
{
	zigbee_nrf_802154_energy_detected(result);
}

void nrf_802154_energy_detection_failed(nrf_802154_ed_error_t error)
{
	zigbee_nrf_802154_energy_detection_failed(error);
}

#if defined(CONFIG_NRF_802154_SER_HOST)
void nrf_802154_serialization_error(const nrf_802154_ser_err_data_t *err)
{
	zigbee_nrf_802154_serialization_error(err);
}
#endif

static int zigbee_802154_radio_init(void)
{
	zigbee_nrf_802154_radio_init();
	nrf_802154_init();
	return 0;
}

THESEUS_MODULE_SET(zigbee_radio) = {.init = zigbee_802154_radio_init,
				    .stage = THESEUS_MODULE_STAGE_LATE};
#endif /* CONFIG_NRF_802154_CALLBACKS_DISPATCHER */
