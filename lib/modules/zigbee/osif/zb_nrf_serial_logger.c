/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <theseus/log.h>
#include <zboss_api.h>

#if defined ZB_NRF_TRACE

void zb_osif_serial_logger_put_bytes(const zb_uint8_t *buf, zb_short_t len)
{
	if (!(buf == NULL || len <= 0)) {
		printf("[Zigbee LOG HEXDUMP]: ");
		for (int i = 0; i < len; i++) {
			printf("0x%x ", buf[i]);
		}
		printf("\n");
	}
}

void zb_osif_serial_logger_flush(void)
{
	/* No action needed since we're using direct transmission */
}

#undef CONFIG_ZB_NRF_TRACE_RX_ENABLE
#if defined(CONFIG_ZB_NRF_TRACE_RX_ENABLE)
/* Function set UART RX callback function */
void zb_osif_serial_logger_set_uart_byte_received_cb(zb_callback_t cb)
{
	char_handler = cb;
}
#endif /* CONFIG_ZB_NRF_TRACE_RX_ENABLE */

#endif /* defined ZB_NRF_TRACE */
