/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * nrfx logging stubs for the NEE bare-metal SDK.
 */
#include <stdio.h>
#include <theseus/log.h>

#ifndef NRFX_LOG_H__
#define NRFX_LOG_H__

#ifdef __cplusplus
extern "C" {
#endif

#define NRFX_LOG_ERROR(format, ...)   LOG("[NRFX ERR] " format "\n", ##__VA_ARGS__)
#define NRFX_LOG_WARNING(format, ...) LOG("[NRFX WRN] " format "\n", ##__VA_ARGS__)
#define NRFX_LOG_INFO(format, ...)
#define NRFX_LOG_DEBUG(format, ...) LOG("[NRFX DBG] " format "\n", ##__VA_ARGS__)

#define NRFX_LOG_HEXDUMP_ERROR(p_memory, length)                                                   \
	if (!(p_memory == NULL || length <= 0)) {                                                  \
		LOG("[NRFX ERROR HEXDUMP]: ");                                                     \
		for (int i = 0; i < length; i++) {                                                 \
			LOG("0x%x ", p_memory[i]);                                                 \
		}                                                                                  \
		LOG("\n");                                                                         \
	}

#define NRFX_LOG_HEXDUMP_WARNING(p_memory, length)                                                 \
	if (!(p_memory == NULL || length <= 0)) {                                                  \
		LOG("[NRFX WARNING HEXDUMP]: ");                                                   \
		for (int i = 0; i < length; i++) {                                                 \
			LOG("0x%x ", p_memory[i]);                                                 \
		}                                                                                  \
		LOG("\n");                                                                         \
	}

#define NRFX_LOG_HEXDUMP_INFO(p_memory, length)                                                    \
	if (!(p_memory == NULL || length <= 0)) {                                                  \
		LOG("[NRFX INFO HEXDUMP]: ");                                                      \
		for (int i = 0; i < length; i++) {                                                 \
			LOG("0x%x ", p_memory[i]);                                                 \
		}                                                                                  \
		LOG("\n");                                                                         \
	}

#define NRFX_LOG_HEXDUMP_DEBUG(p_memory, length)                                                   \
	if (!(p_memory == NULL || length <= 0)) {                                                  \
		LOG("[NRFX DEBUG HEXDUMP]: ");                                                     \
		for (int i = 0; i < length; i++) {                                                 \
			LOG("0x%x ", p_memory[i]);                                                 \
		}                                                                                  \
		LOG("\n");                                                                         \
	}

#define NRFX_LOG_ERROR_STRING_GET(error_code) nrfx_error_string_get(error_code)
extern const char *nrfx_error_string_get(int code);

#ifdef __cplusplus
}
#endif

#endif /* NRFX_LOG_H__ */
