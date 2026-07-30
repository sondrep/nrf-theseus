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
#define BUF_SIZE     256

SemaphoreHandle_t xPrintMutex;
static volatile uint8_t tx_byte __attribute__((aligned(4)));

static volatile uint8_t tx_bytes[2][BUF_SIZE];
static size_t active_buf_index = 0;
static size_t tx_count;
static nrfx_uarte_t uart_instance =
	NRFX_UARTE_INSTANCE(NRF_UARTE_INST_GET(BOARD_CONSOLE_UARTE_INDEX));

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
	if (!nrfx_uarte_tx_in_progress(&uart_instance)) {
		nrfx_uarte_tx(&uart_instance, tx_bytes[active_buf_index], tx_count, 0);
		active_buf_index = (~active_buf_index) & 1;
		tx_count = 0;
	}
	return 0;
} /*VTASKDELAY*/

static int buf_putc(char c, FILE *stream)
{
	/* Change to do while loop instead? */
	while (tx_count >= sizeof(tx_bytes[active_buf_index]) - 1) {
		flush(stream);
	}

	if (c == '\n') {
		tx_bytes[active_buf_index][tx_count++] = '\r';
	}
	tx_bytes[active_buf_index][tx_count++] = c;

	return 0;
}

/* ---- picolibc stdio streams ------------------------------------------- */

static FILE uart_file = FDEV_SETUP_STREAM(buf_putc, NULL, flush, _FDEV_SETUP_WRITE);

FILE *const stdin = NULL;
FILE *const stdout = &uart_file;
FILE *const stderr = &uart_file;

static int console_init(void)
{
	xPrintMutex = xSemaphoreCreateBinary();
	configASSERT(xPrintMutex);
	xSemaphoreGive(xPrintMutex);

	nrfx_uarte_config_t cfg =
		NRFX_UARTE_DEFAULT_CONFIG(BOARD_CONSOLE_TX_PIN, BOARD_CONSOLE_RX_PIN);
	nrfx_uarte_init(&uart_instance, &cfg, NULL);

	return 0;
}

THESEUS_MODULE_SET(log) = {.init = console_init, .stage = THESEUS_MODULE_STAGE_LOG};
