/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "zb_nrf_platform.h"
#include <zboss_api.h>

/* Forward declarations */
void zb_osif_serial_logger_init(void);
void zb_osif_async_serial_init(void);
void zb_osif_logger_put_bytes(const zb_uint8_t *buf, zb_short_t len);
void zb_osif_serial_logger_put_bytes(const zb_uint8_t *buf, zb_short_t len);
void zb_osif_async_serial_put_bytes(const zb_uint8_t *buf, zb_short_t len);
void zb_osif_logger_set_uart_byte_received_cb(zb_callback_t cb);
void zb_osif_serial_logger_set_uart_byte_received_cb(zb_callback_t cb);
void zb_osif_async_serial_set_uart_byte_received_cb(zb_callback_t cb);
void zb_osif_async_serial_flush(void);
void zb_osif_logger_flush(void);
void zb_osif_serial_logger_flush(void);
void zb_osif_async_serial_sleep(void);
void zb_osif_async_serial_wake_up(void);

void zb_osif_serial_init(void)
{
#if defined(CONFIG_ZBOSS_TRACE_BINARY_LOGGING) && defined(CONFIG_ZIGBEE_HAVE_ASYNC_SERIAL)
	__ASSERT(DEVICE_DT_GET(DT_CHOSEN(ncs_zigbee_uart)) !=
			 DEVICE_DT_GET(DT_CHOSEN(ncs_zboss_trace_uart)),
		 "The same serial device used for serial logger and async serial!");
#endif /* defined(CONFIG_ZBOSS_TRACE_BINARY_LOGGING) && defined(CONFIG_ZIGBEE_HAVE_ASYNC_SERIAL)   \
	*/
}

void zb_osif_serial_put_bytes(const zb_uint8_t *buf, zb_short_t len)
{
}

/* Function set UART RX callback function */
void zb_osif_set_uart_byte_received_cb(zb_callback_t cb)
{
}

/* Prints remaining bytes. */
void zb_osif_serial_flush(void)
{
}

void zb_osif_uart_sleep(void)
{
}

void zb_osif_uart_wake_up(void)
{
}
