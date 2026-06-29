/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * nrfx logging stubs for the NEE bare-metal SDK.
 */
#include <stdio.h>

#ifndef NRFX_LOG_H__
#define NRFX_LOG_H__

#ifdef __cplusplus
extern "C" {
#endif

#define NRFX_LOG_ERROR(format, ...)   printf("[NRFX ERR] " format "\n", ##__VA_ARGS__)
#define NRFX_LOG_WARNING(format, ...) printf("[NRFX WRN] " format "\n", ##__VA_ARGS__)
#define NRFX_LOG_INFO(format, ...)    printf("[NRFX INF] " format "\n", ##__VA_ARGS__)
#define NRFX_LOG_DEBUG(format, ...)   printf("[NRFX DBG] " format "\n", ##__VA_ARGS__)

#define NRFX_LOG_HEXDUMP_ERROR(p_memory, length)                                                   \
	if (!(p_memory == NULL || length <= 0)) {                                                  \
		printf("[NRFX ERROR HEXDUMP]: ");                                                  \
		for (int i = 0; i < length; i++) {                                                 \
			printf("0x%x ", p_memory[i]);                                              \
		}                                                                                  \
		printf("\n");                                                                      \
	}

#define NRFX_LOG_HEXDUMP_WARNING(p_memory, length)                                                 \
	if (!(p_memory == NULL || length <= 0)) {                                                  \
		printf("[NRFX WARNING HEXDUMP]: ");                                                \
		for (int i = 0; i < length; i++) {                                                 \
			printf("0x%x ", p_memory[i]);                                              \
		}                                                                                  \
		printf("\n");                                                                      \
	}

#define NRFX_LOG_HEXDUMP_INFO(p_memory, length)                                                    \
	if (!(p_memory == NULL || length <= 0)) {                                                  \
		printf("[NRFX INFO HEXDUMP]: ");                                                   \
		for (int i = 0; i < length; i++) {                                                 \
			printf("0x%x ", p_memory[i]);                                              \
		}                                                                                  \
		printf("\n");                                                                      \
	}

#define NRFX_LOG_HEXDUMP_DEBUG(p_memory, length)                                                   \
	if (!(p_memory == NULL || length <= 0)) {                                                  \
		printf("[NRFX DEBUG HEXDUMP]: ");                                                  \
		for (int i = 0; i < length; i++) {                                                 \
			printf("0x%x ", p_memory[i]);                                              \
		}                                                                                  \
		printf("\n");                                                                      \
	}

#define NRFX_LOG_ERROR_STRING_GET(error_code) nrfx_error_string_get(error_code)
extern const char *nrfx_error_string_get(int code);

#ifdef __cplusplus
}
#endif

#endif /* NRFX_LOG_H__ */
