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

#define CONSOLE_BAUD NRF_UARTE_BAUDRATE_115200

SemaphoreHandle_t xPrintMutex;
static volatile uint8_t tx_byte __attribute__((aligned(4)));

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
		nrf_uarte_tx_buffer_set(BOARD_CONSOLE_UARTE_INST, (uint8_t const *)&tx_byte, 1);
		nrf_uarte_event_clear(BOARD_CONSOLE_UARTE_INST, NRF_UARTE_EVENT_ENDTX);
		nrf_uarte_task_trigger(BOARD_CONSOLE_UARTE_INST, NRF_UARTE_TASK_STARTTX);
		while (!nrf_uarte_event_check(BOARD_CONSOLE_UARTE_INST, NRF_UARTE_EVENT_ENDTX))
			;
	}

	tx_byte = (uint8_t)c;
	nrf_uarte_tx_buffer_set(BOARD_CONSOLE_UARTE_INST, (uint8_t const *)&tx_byte, 1);
	nrf_uarte_event_clear(BOARD_CONSOLE_UARTE_INST, NRF_UARTE_EVENT_ENDTX);
	nrf_uarte_task_trigger(BOARD_CONSOLE_UARTE_INST, NRF_UARTE_TASK_STARTTX);
	while (!nrf_uarte_event_check(BOARD_CONSOLE_UARTE_INST, NRF_UARTE_EVENT_ENDTX))
		;

	return 0;
}

/* ---- picolibc stdio streams ------------------------------------------- */

static FILE uart_file = FDEV_SETUP_STREAM(uart_putc, NULL, NULL, _FDEV_SETUP_WRITE);

FILE *const stdin = NULL;
FILE *const stdout = &uart_file;
FILE *const stderr = &uart_file;

/* ---- Public API ------------------------------------------------------- */

static int console_init(void)
{
	xPrintMutex = xSemaphoreCreateMutex();
	configASSERT(xPrintMutex);

	nrf_gpio_pin_set(BOARD_CONSOLE_TX_PIN);
	nrf_gpio_cfg_output(BOARD_CONSOLE_TX_PIN);

	nrf_uarte_txrx_pins_set(BOARD_CONSOLE_UARTE_INST, BOARD_CONSOLE_TX_PIN,
				NRF_UARTE_PSEL_DISCONNECTED);
	nrf_uarte_baudrate_set(BOARD_CONSOLE_UARTE_INST, CONSOLE_BAUD);

	nrf_uarte_config_t cfg = {
		.hwfc = NRF_UARTE_HWFC_DISABLED,
		.parity = NRF_UARTE_PARITY_EXCLUDED,
	};
	nrf_uarte_configure(BOARD_CONSOLE_UARTE_INST, &cfg);
	nrf_uarte_enable(BOARD_CONSOLE_UARTE_INST);

	return 0;
}

THESEUS_MODULE_SET(log) = {.init = console_init, .stage = THESEUS_MODULE_STAGE_EARLY};
