/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * nrfx logging stubs for the NEE bare-metal SDK.
 */

#ifndef NRFX_LOG_H__
#define NRFX_LOG_H__

#ifdef __cplusplus
extern "C" {
#endif

#define NRFX_LOG_ERROR(format, ...)
#define NRFX_LOG_WARNING(format, ...)
#define NRFX_LOG_INFO(format, ...)
#define NRFX_LOG_DEBUG(format, ...)

#define NRFX_LOG_HEXDUMP_ERROR(p_memory, length)
#define NRFX_LOG_HEXDUMP_WARNING(p_memory, length)
#define NRFX_LOG_HEXDUMP_INFO(p_memory, length)
#define NRFX_LOG_HEXDUMP_DEBUG(p_memory, length)

#define NRFX_LOG_ERROR_STRING_GET(error_code)  ""

#ifdef __cplusplus
}
#endif

#endif /* NRFX_LOG_H__ */
