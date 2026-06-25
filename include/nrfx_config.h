/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * nrfx configuration header for the NEE bare-metal SDK.
 * Pulls in autoconf.h (Kconfig-generated) and the SoC-specific
 * nrfx template configuration.
 */

#ifndef NRFX_CONFIG_H__
#define NRFX_CONFIG_H__

#define NRFX_CONFIG_API_VER_MAJOR 4
#define NRFX_CONFIG_API_VER_MINOR 3
#define NRFX_CONFIG_API_VER_MICRO 0

/* Pull in Kconfig-generated defines (CONFIG_NRFX_*, CONFIG_SOC_*, ...) */
#if __has_include("autoconf.h")
#include "autoconf.h"
#endif

/*
 * Map Kconfig CONFIG_NRFX_<peripheral> symbols to the NRFX_<peripheral>_ENABLED
 * defines that nrfx driver sources check.
 */
#ifdef CONFIG_NRFX_GPIOTE
#define NRFX_GPIOTE_ENABLED 1
#endif
#ifdef CONFIG_NRFX_UARTE
#define NRFX_UARTE_ENABLED 1
#endif
#ifdef CONFIG_NRFX_TIMER
#define NRFX_TIMER_ENABLED 1
#endif
#ifdef CONFIG_NRFX_SAADC
#define NRFX_SAADC_ENABLED 1
#endif
#ifdef CONFIG_NRFX_SPIM
#define NRFX_SPIM_ENABLED 1
#endif
#ifdef CONFIG_NRFX_TWIM
#define NRFX_TWIM_ENABLED 1
#endif
#ifdef CONFIG_NRFX_GRTC
#define NRFX_GRTC_ENABLED 1
#endif
#ifdef CONFIG_NRFX_CRACEN
#define NRFX_CRACEN_ENABLED 1
#endif

/* SoC-specific config from the nrfx templates */
#include "nrfx_templates_config.h"

#endif /* NRFX_CONFIG_H__ */
