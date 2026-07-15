/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <theseus/nfc/t4t/ndef_file.h>

int nfc_t4t_ndef_file_encode(uint8_t *file_buf, uint32_t *size)
{
	if (!file_buf || !size || (*size < 1)) {
		return -EINVAL;
	}

	if (*size > UINT16_MAX) {
		return -ENOTSUP;
	}

	/* NLEN field: NDEF message length, 2-byte big-endian */
	file_buf[0] = (uint8_t)(*size >> 8);
	file_buf[1] = (uint8_t)(*size);
	*size += NFC_NDEF_FILE_NLEN_FIELD_SIZE;

	return 0;
}
