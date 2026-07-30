/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zboss_api.h>
#include <theseus/log.h>

#if defined ZB_NRF_TRACE

void zb_osif_logger_put_bytes(const zb_uint8_t *buf, zb_short_t len)
{
	/* Log data directly using Zephyr logging system */
	if (!(buf == NULL || len <= 0)) {
		printf("[ZB ERROR HEXDUMP]: ");
		for (int i = 0; i < len; i++) {
			printf("0x%x ", buf[i]);
		}
		printf("\n");
	}
}

#if defined(CONFIG_ZB_NRF_TRACE_RX_ENABLE)
/* Function set UART RX callback function */
void zb_osif_logger_set_uart_byte_received_cb(zb_callback_t cb)
{
	LOG("Command reception is not available through logger\n");
}
#endif /* CONFIG_ZB_NRF_TRACE_RX_ENABLE */

#endif /* defined ZB_NRF_TRACE */
