/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Theseus -- Polled UART console.
 */

#include <stdio.h>
#include <nrfx.h>
#include <hal/nrf_uarte.h>
#include <hal/nrf_gpio.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include <theseus/log.h>
#include <theseus/module.h>
#include <board.h>
#include <nrfx_uarte.h>
#include <nrf.h>

#define CONSOLE_BAUD NRF_UARTE_BAUDRATE_115200
#define BUF_SIZE     128

SemaphoreHandle_t xPrintMutex;
SemaphoreHandle_t tx_sem;
static volatile uint8_t tx_byte __attribute__((aligned(4)));
static volatile uint8_t tx_bytes[BUF_SIZE] __attribute__((aligned(4)));
static uint8_t log_buf[BUF_SIZE];
static size_t tx_count;
static nrfx_uarte_t uart_instance = NRFX_UARTE_INSTANCE(BOARD_CONSOLE_UARTE_INST);

/* ---- picolibc per-char hook ------------------------------------------- *
 * Sends one byte and blocks until it is fully shifted out (polled, no IRQ). */

static int uart_putc(char c, FILE *stream)
{
	(void)stream;

	/* Convert a Unix newline into a terminal-friendly one.
	 *
	 * C strings terminate a line with a single LF ('\n'),
	 * whereas most serial terminals expect CR + LF ('\r\n'):
	 * the CR returns the cursor to the start of the line and the LF advances it one row.
	 * Omitting the CR produces the familiar "staircase" effect.
	 *
	 * Therefore, when the application sends '\n',
	 * we emit '\r' first and then fall through to send the '\n' below. */
	if (c == '\n') {
		tx_byte = (uint8_t)'\r';
		nrfx_uarte_tx(&uart_instance, &tx_byte, sizeof(tx_byte), NRFX_UARTE_TX_BLOCKING);
	}

	tx_byte = (uint8_t)c;
	nrfx_uarte_tx(&uart_instance, &tx_byte, sizeof(tx_byte), NRFX_UARTE_TX_BLOCKING);

	return 0;
}

static int flush(FILE *stream)
{
	(void)stream;
	if (tx_count == 0) {
		return 0;
	}

	int err = nrfx_uarte_tx(&uart_instance, tx_bytes, tx_count, 0);
	tx_count = 0;
	return err;
} /*VTASKDELAY*/

static int buf_putc(char c, FILE *stream)
{
	if (c == '\n') {
		log_buf[tx_count++] = '\r';
	}
	log_buf[tx_count++] = c;

	if (tx_count >= sizeof(log_buf) - 1) {
		return flush(stream);
	}
	return 0;
}

static void uart_cb(nrfx_uarte_event_t const *p_event, void *p_context)
{
	(void)p_context;

	if (p_event->type == NRFX_UARTE_EVT_TX_DONE || p_event->type == NRFX_UARTE_EVT_TRIGGER) {
		xSemaphoreTakeFromISR(xPrintMutex, portMAX_DELAY);
		memcpy(tx_bytes, log_buf, tx_count);
		if (tx_count > 0) {
			nrfx_uarte_tx(&uart_instance, tx_bytes, tx_count, 0);
		}
		tx_count = 0;
		xSemaphoreGiveFromISR(xPrintMutex, NULL);
	}
}

void SERIAL20_IRQHandler(void)
{
	nrfx_uarte_irq_handler(&uart_instance);
}

/* ---- picolibc stdio streams ------------------------------------------- */

static FILE uart_file = FDEV_SETUP_STREAM(buf_putc, NULL, NULL, _FDEV_SETUP_WRITE);

FILE *const stdin = NULL;
FILE *const stdout = &uart_file;
FILE *const stderr = &uart_file;

static int console_init(void)
{
	NVIC_EnableIRQ(NRFX_IRQ_NUMBER_GET(BOARD_CONSOLE_UARTE_INST));
	xPrintMutex = xSemaphoreCreateBinary();
	tx_sem = xSemaphoreCreateBinary();
	xSemaphoreGive(xPrintMutex);
	configASSERT(xPrintMutex);

	nrfx_uarte_config_t cfg =
		NRFX_UARTE_DEFAULT_CONFIG(BOARD_CONSOLE_TX_PIN, BOARD_CONSOLE_RX_PIN);
	nrfx_uarte_init(&uart_instance, &cfg, uart_cb);
	// nrfx_uarte_int_trigger(&uart_instance);

	return 0;
}

THESEUS_MODULE_SET(log) = {.init = console_init, .stage = THESEUS_MODULE_STAGE_LOG};
