/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "zb_nrf_platform.h"
#include <FreeRTOS.h>
#include <nrfx_uarte.h>
#include <semphr.h>
#include <timers.h>
#include <zboss_api.h>

#define DEFAULT_SINGLE_PORT_INSTANCE 0

static SemaphoreHandle_t tx_done_sem;
static SemaphoreHandle_t rx_done_sem;

static nrfx_uarte_t uart_inst = NRFX_UARTE_INSTANCE(30);

static bool is_sleeping;
static bool uart_initialized;

static zb_callback_t char_handler;
static zb_mserial_recv_data_cb_t rx_data_cb;
static zb_serial_send_data_cb_t tx_data_cb;
static zb_serial_send_data_cb_t tx_trx_data_cb;

#ifdef CONFIG_ZBOSS_TRACE_BINARY_NCP_TRANSPORT_LOGGING
static uint8_t uart_tx_buf_mem[CONFIG_ZIGBEE_UART_TX_BUF_LEN];
static size_t uart_tx_buf_size;
#endif /* CONFIG_ZBOSS_TRACE_BINARY_NCP_TRANSPORT_LOGGING */

static uint8_t *uart_tx_buf;
static uint8_t *uart_tx_buf_bak;
static volatile size_t uart_tx_buf_offset;
static volatile size_t uart_tx_buf_len;

static uint8_t uart_rx_buf_mem[CONFIG_ZIGBEE_UART_RX_BUF_LEN];
static struct ring_buf rx_ringbuf;

static uint8_t *uart_rx_buf;
static volatile size_t uart_rx_buf_offset;
static volatile size_t uart_rx_buf_len;

static TimerHandle_t uart_tx_timer;
static TimerHandle_t uart_rx_timer;

/**
 * Inform user about received data and unlock for the next reception.
 */
static void uart_rx_notify(zb_bufid_t bufid)
{
	(void)bufid;
	uint8_t *rx_buf = uart_rx_buf;
	size_t rx_buf_len = uart_rx_buf_offset;

	uart_rx_buf_len = 0;
	uart_rx_buf_offset = 0;
	uart_rx_buf = NULL;
	BaseType_t ok = xTimerStop(uart_rx_timer, portMAX_DELAY);
	assert(ok == pdPASS);
	ok = xSemaphoreGive(rx_done_sem);

	if (rx_data_cb) {
		rx_data_cb(DEFAULT_SINGLE_PORT_INSTANCE, rx_buf, rx_buf_len);
	}
}

static void uart_rx_bytes(uint8_t *buf, size_t len)
{
	if (char_handler) {
		for (size_t i = 0; i < len; i++) {
			char_handler(buf[i]);
		}
	}
}

/**
 * Inform user about transmission timeout.
 */
static void uart_tx_timeout(TimerHandle_t pxTimer)
{
	uart_tx_buf_offset = 0;
	uart_tx_buf_len = 0;
	uart_tx_buf = uart_tx_buf_bak;
	xSemaphoreGive(tx_done_sem);

	if (tx_trx_data_cb) {
		zigbee_schedule_callback(tx_trx_data_cb, SERIAL_SEND_TIMEOUT_EXPIRED);
		tx_trx_data_cb = NULL;
	}
}

/**
 * Inform user about reception timeout.
 */
static void uart_rx_timeout(TimerHandle_t pxTimer)
{
	if (uart_rx_buf) {
		uart_rx_buf_len = 0;

		if (zigbee_schedule_callback(uart_rx_notify, 0)) {
			uart_rx_buf_offset = 0;
			uart_rx_buf = NULL;
			xSemaphoreGive(rx_done_sem);
		}
	}
}

void zb_osif_async_serial_init(void)
{
	if (uart_initialized) {
		return;
	}

	rx_done_sem = xSemaphoreCreateBinary();
	tx_done_sem = xSemaphoreCreateBinary();

	xSemaphoreGive(rx_done_sem);
	xSemaphoreGive(tx_done_sem);

	uart_rx_timer =
		xTimerCreate("uart rx timer", pdMS_TO_TICKS(10), pdFALSE, NULL, uart_rx_timeout);
	uart_tx_timer =
		xTimerCreate("uart tx timer", pdMS_TO_TICKS(10), pdFALSE, NULL, uart_tx_timeout);

	/*
	 * Reset all static variables in case of runtime init/uninit sequence.
	 */
	char_handler = NULL;
	rx_data_cb = NULL;
	tx_data_cb = NULL;
	tx_trx_data_cb = NULL;
	uart_tx_buf_len = 0;
	uart_tx_buf_offset = 0;
	uart_rx_buf = NULL;
	uart_rx_buf_len = 0;
	uart_rx_buf_offset = 0;

#ifdef CONFIG_ZBOSS_TRACE_BINARY_NCP_TRANSPORT_LOGGING
	uart_tx_buf_size = sizeof(uart_tx_buf_mem);
	uart_tx_buf = uart_tx_buf_mem;
	uart_tx_buf_bak = uart_tx_buf_mem;
#else
	uart_tx_buf = NULL;
	uart_tx_buf_bak = NULL;
#endif /* CONFIG_ZBOSS_TRACE_BINARY_NCP_TRANSPORT_LOGGING */

	if (!device_is_ready(uart_dev)) {
		return;
	}

	ring_buf_init(&rx_ringbuf, sizeof(uart_rx_buf_mem), uart_rx_buf_mem);
	uart_irq_callback_set(uart_dev, interrupt_handler);

	/* Enable rx interrupts. */
	uart_irq_rx_enable(uart_dev);

	uart_initialized = true;
}

void zb_osif_async_serial_sleep(void)
{
	if (uart_dev == NULL) {
		return;
	}

	is_sleeping = true;
	uart_irq_tx_disable(uart_dev);
	uart_irq_rx_disable(uart_dev);
}

void zb_osif_async_serial_wake_up(void)
{
	if (uart_dev == NULL) {
		return;
	}

	is_sleeping = false;

	/* Enable rx interrupts. */
	uart_irq_rx_enable(uart_dev);
}

void zb_osif_serial_recv_data(zb_uint8_t *buf, zb_ushort_t len)
{
	if (!rx_data_cb) {
		return;
	}

	if ((uart_dev == NULL) || (len == 0) || is_sleeping) {
		if (rx_data_cb) {
			rx_data_cb(DEFAULT_SINGLE_PORT_INSTANCE, NULL, 0);
		}
		return;
	}

	if (xSemaphoreTake(rx_done_sem, pdMS_TO_TICKS(CONFIG_ZIGBEE_UART_RX_TIMEOUT))) {
		/* Ongoing asynchronous reception. */
		if (rx_data_cb) {
			rx_data_cb(DEFAULT_SINGLE_PORT_INSTANCE, NULL, 0);
		}
		return;
	}

	/*
	 * Flush already received data.
	 * Disable interrupt to block buffer reads from the interrupt handler.
	 */
	uart_irq_rx_disable(uart_dev);
	uart_rx_buf_offset = ring_buf_get(&rx_ringbuf, buf, len);
	uart_irq_rx_enable(uart_dev);

	if (uart_rx_buf_offset == len) {
		uart_rx_buf_offset = 0;
		xSemaphoreGive(rx_done_sem);
		rx_data_cb(0, buf, len);
		return;
	}

	/*
	 * Since the driver is kept in a continuous reception, it is enough to
	 * pass the buffer through a variable.
	 */
	uart_rx_buf_len = len;
	uart_rx_buf = buf;
}

void zb_osif_serial_set_cb_recv_data(zb_mserial_recv_data_cb_t cb)
{
	rx_data_cb = cb;
}

void zb_osif_serial_send_data(zb_uint8_t *buf, zb_ushort_t len)
{
	if ((uart_dev == NULL) || is_sleeping) {
		if (tx_data_cb) {
			tx_data_cb(SERIAL_SEND_ERROR);
		}
		return;
	}

	if (xSemaphoreTake(tx_done_sem, pdMS_TO_TICKS(CONFIG_ZIGBEE_UART_TX_TIMEOUT))) {
		/* Ongoing synchronous transmission. */
		if (tx_data_cb) {
			tx_data_cb(SERIAL_SEND_BUSY);
		}
		return;
	}

	uart_tx_buf_bak = uart_tx_buf;
	uart_tx_buf = buf;
	uart_tx_buf_len = len;
	uart_tx_buf_offset = 0;

	/* Pass the TX callback for a single (ongoing) transmission. */
	tx_trx_data_cb = tx_data_cb;

	/* Enable TX ready event. */
	uart_irq_tx_enable(uart_dev);
}

void zb_osif_serial_set_cb_send_data(zb_serial_send_data_cb_t cb)
{
	tx_data_cb = cb;
}

void zb_osif_async_serial_flush(void)
{
	xSemaphoreTake(tx_done_sem, portMAX_DELAY);
	xSemaphoreGive(tx_done_sem);
}

void zb_osif_async_serial_set_uart_byte_received_cb(zb_callback_t hnd)
{
	char_handler = hnd;
}

#ifdef CONFIG_ZBOSS_TRACE_BINARY_NCP_TRANSPORT_LOGGING
void zb_osif_set_user_io_buffer(zb_byte_array_t *buf_ptr, zb_ushort_t capacity)
{
	xSemaphoreTake(tx_done_sem, portMAX_DELAY);

	uart_tx_buf = buf_ptr->ring_buf;
	uart_tx_buf_bak = uart_tx_buf;
	uart_tx_buf_size = capacity;

	xSemaphoreGive(tx_done_sem);
}

void zb_osif_async_serial_put_bytes(const zb_uint8_t *buf, zb_short_t len)
{
#if !(defined(ZB_HAVE_ASYNC_SERIAL) && defined(CONFIG_ZBOSS_TRACE_LOG_LEVEL_OFF))

	if ((uart_dev == NULL) || is_sleeping) {
		return;
	}

	if (len > uart_tx_buf_size) {
		return;
	}

	/*
	 * Wait forever since there is no way to inform higher layer
	 * about TX busy state.
	 */
	xSemaphoreTake(tx_done_sem, portMAX_DELAY);
	memcpy(uart_tx_buf, buf, len);

	uart_tx_buf_len = len;
	uart_tx_buf_offset = 0;

	/* Enable tx interrupts. */
	uart_irq_tx_enable(uart_dev);

#endif /* !(ZB_HAVE_ASYNC_SERIAL && CONFIG_ZBOSS_TRACE_LOG_LEVEL_OFF) */
}
#endif /* CONFIG_ZBOSS_TRACE_BINARY_NCP_TRANSPORT_LOGGING */
