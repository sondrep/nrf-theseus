/*
 * Copyright (c) 2018-2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <errno.h>
#include <nrfx_clock.h>
#include <nrfx_nfct.h>
#include <nrfx_timer.h>
#include <hal/nrf_ficr.h>

#include <nfc_platform.h>

#include <theseus/log.h>

#if NRF54L_ERRATA_60_ENABLE_WORKAROUND
#define NFC_PLATFORM_USE_TIMER_WORKAROUND 1
#else
#define NFC_PLATFORM_USE_TIMER_WORKAROUND 0
#endif

#if NFC_PLATFORM_USE_TIMER_WORKAROUND
#define NFC_TIMER_IRQn NRFX_CONCAT_3(TIMER, NRFX_NFCT_CONFIG_TIMER_INSTANCE_ID, _IRQn)
#endif /* NFC_PLATFORM_USE_TIMER_WORKAROUND */

/* IS_ENABLED(X) evaluates to 1 only when X is defined as 1, and to 0 when X is
 * defined as 0 or left undefined — so it's safe to use in #if and in ?: expressions.
 * Copied from Zephyr's sys/util_internal.h; it's pure preprocessor with no dependencies.
 * Also add a guard to prevent multiple definitions. */
#ifndef IS_ENABLED
#define IS_ENABLED(config_macro)      Z_IS_ENABLED1(config_macro)
#define Z_IS_ENABLED1(config_macro)   Z_IS_ENABLED2(_XXXX##config_macro)
#define _XXXX1                        _YYYY,
#define Z_IS_ENABLED2(one_or_two_args) Z_IS_ENABLED3(one_or_two_args 1, 0)
#define Z_IS_ENABLED3(ignore, val, ...) val
#endif /* IS_ENABLED */

#define NFC_T2T_BUFFER_SIZE (IS_ENABLED(CONFIG_NFC_T2T_NRFXLIB) ? NFC_PLATFORM_T2T_BUFFER_SIZE : 0U)
#define NFC_T4T_BUFFER_SIZE (IS_ENABLED(CONFIG_NFC_T4T_NRFXLIB) ? \
							    (2 * NFC_PLATFORM_T4T_BUFFER_SIZE) : 0U)

/* MAX: guard in case a toolchain/nrfx header already defines it */
#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif /* MAX */
#define NFCT_PLATFORM_BUFFER_SIZE MAX(NFC_T4T_BUFFER_SIZE, NFC_T2T_BUFFER_SIZE)

/* Library-provided callback resolver,
 * saved in nfc_platform_setup(),
 * invoked from nfc_platform_cb_request() in IRQ context. */
static nfc_lib_cb_resolve_t nfc_cb_resolve;

/* NFC platform buffer. This buffer is used directly by the NFCT peripheral.
 * It might need to be allocated in the specific memory section which can be accessed
 * by EasyDMA.
 */
static uint8_t nfc_platform_buffer[NFCT_PLATFORM_BUFFER_SIZE];

#if CONFIG_NFC_T2T_NRFXLIB
_Static_assert(sizeof(nfc_platform_buffer) >= NFC_T2T_BUFFER_SIZE,
		      "Minimal buffer size for the NFC T2T operations must be at least 16 bytes");
#endif /* CONFIG_NFC_T2T_NRFXLIB */

#if CONFIG_NFC_T4T_NRFXLIB
_Static_assert(sizeof(nfc_platform_buffer) >= NFC_T4T_BUFFER_SIZE,
		      "Minimal buffer size for the NFC T4T operations must be at least 518 bytes");
#endif /* CONFIG_NFC_T4T_NRFXLIB */

/* NOTE: no NFCT_IRQHandler is defined here on purpose
 *
 * With nrfx's flat IRQ model, nrfx_glue.h -> <soc/nrfx_irqs.h> already maps
 *     #define nrfx_nfct_irq_handler  NFCT_IRQHandler
 * so the handler in nrfx_nfct.c lands directly on the NFCT vector.
 * Defining our own here would be a duplicate symbol,
 * and calling nrfx_nfct_irq_handler() from it would just be NFCT_IRQHandler() calling itself
 * (infinite recursion)
 * All we have to do is enable the NVIC line
 * (in nfc_platform_setup)
 *
 * The TIMER handler below IS defined manually,
 * since it must route to nrfx_nfct_workaround_timer_handler() instead of the generic timer handler.
 */

#if NFC_PLATFORM_USE_TIMER_WORKAROUND
void TIMER24_IRQHandler(void)
{
	nrfx_nfct_workaround_timer_handler();
}
#endif

/* Called from the CLOCK IRQ once a clock event completes.
 * Per the nrfxlib NFC library documentation (NFCT overview),
 * NFCT must not be ACTIVATED until HFXO is running,
 * so we defer activation to here instead of the field-detect handler. */
static void clock_handler(nrfx_clock_evt_type_t event)
{
	if (event == NRFX_CLOCK_EVT_HFCLK_STARTED) {
		nrfx_nfct_state_force(NRFX_NFCT_STATE_ACTIVATED);
	}
}

/*
 * nfc_platform_setup(), called once by the NFC library on init (see <nfc_platform.h>).
 * Sets up the NFCT (and errata-60 timer) IRQs, sets their priority, and saves the
 * callback resolver the library hands us.
 */
int nfc_platform_setup(nfc_lib_cb_resolve_t nfc_lib_cb_resolve, uint8_t *p_irq_priority)
{
	int err;

	if (!nfc_lib_cb_resolve) {
		LOG("[ERROR] NFC platform init fail: callback resolution function pointer is invalid");
		return -EFAULT;
	}

	/* Save the library's callback resolver for nfc_platform_cb_request() to use later. */
	nfc_cb_resolve = nfc_lib_cb_resolve;

	/* Bring up the CLOCK driver so we can start HFXO on demand,
	 * NFCT can only reach ACTIVATED once HFXO is running, so the clock gates that.
	 * Treat ALREADY_INITIALIZED as success:
	 * another module may have init'd nrfx_clock first and we just piggyback on it.
	 * nrfx_clock_init also enables the shared CLOCK_POWER NVIC line,
	 * so there's no manual NVIC setup needed here. */
	err = nrfx_clock_init(clock_handler);
	if ((err != 0) && (err != -EALREADY)) {
		LOG("[ERROR] nrfx_clock_init failed");
		return -EIO;
	}

	/* Ungate the driver so HFCLK events (NRFX_CLOCK_EVT_HFCLK_STARTED) reach clock_handler(),
	 * where NFCT gets activated.
	 * Without this the started event never fires and the tag stays stuck in SENSE mode. */
	nrfx_clock_enable();

	/* Set up the NFCT IRQs. */
	NVIC_SetPriority(NFCT_IRQn, NRFX_NFCT_DEFAULT_CONFIG_IRQ_PRIORITY);
	NVIC_EnableIRQ(NFCT_IRQn);

#if NFC_PLATFORM_USE_TIMER_WORKAROUND
	NVIC_SetPriority(NFC_TIMER_IRQn, NRFX_NFCT_DEFAULT_CONFIG_IRQ_PRIORITY);
	NVIC_EnableIRQ(NFC_TIMER_IRQn);
#endif

	*p_irq_priority = NRFX_NFCT_DEFAULT_CONFIG_IRQ_PRIORITY;

	LOG("[DEBUG] NFC platform initialized");
	return 0;
}

static int nfc_platform_tagheaders_get(uint32_t tag_header[3])
{
	tag_header[0] = nrf_ficr_nfc_tagheader_get(NRF_FICR, 0);
	tag_header[1] = nrf_ficr_nfc_tagheader_get(NRF_FICR, 1);
	tag_header[2] = nrf_ficr_nfc_tagheader_get(NRF_FICR, 2);

	return 0;
}

int nfc_platform_nfcid1_default_bytes_get(uint8_t * const buf,
					  uint32_t        buf_len)
{
	if (!buf) {
		return -EINVAL;
	}

	if ((buf_len != NRFX_NFCT_NFCID1_SINGLE_SIZE) &&
	    (buf_len != NRFX_NFCT_NFCID1_DOUBLE_SIZE) &&
	    (buf_len != NRFX_NFCT_NFCID1_TRIPLE_SIZE)) {
		return -E2BIG;
	}

	int err;
	uint32_t nfc_tag_header[3];

	err = nfc_platform_tagheaders_get(nfc_tag_header);
	if (err != 0) {
		return err;
	}

	buf[0] = (uint8_t) (nfc_tag_header[0] >> 0);
	buf[1] = (uint8_t) (nfc_tag_header[0] >> 8);
	buf[2] = (uint8_t) (nfc_tag_header[0] >> 16);
	buf[3] = (uint8_t) (nfc_tag_header[1] >> 0);

	if (buf_len != NRFX_NFCT_NFCID1_SINGLE_SIZE) {
		buf[4] = (uint8_t) (nfc_tag_header[1] >> 8);
		buf[5] = (uint8_t) (nfc_tag_header[1] >> 16);
		buf[6] = (uint8_t) (nfc_tag_header[1] >> 24);

		if (buf_len == NRFX_NFCT_NFCID1_TRIPLE_SIZE) {
			buf[7] = (uint8_t) (nfc_tag_header[2] >> 0);
			buf[8] = (uint8_t) (nfc_tag_header[2] >> 8);
			buf[9] = (uint8_t) (nfc_tag_header[2] >> 16);
		}
		/* Workaround for errata 181 "NFCT: Invalid value in FICR for double-size NFCID1"
		 * found at the Errata document for your device located at
		 * https://infocenter.nordicsemi.com/index.jsp
		 */
		else if (buf[3] == 0x88) {
			buf[3] |= 0x11;
		}
	}

	return 0;
}

uint8_t *nfc_platform_buffer_alloc(size_t size)
{
	if (size > sizeof(nfc_platform_buffer)) {
		NRFX_ASSERT(false);
		return NULL;
	}

	return nfc_platform_buffer;
}

void nfc_platform_buffer_free(uint8_t *p_buffer)
{
	(void)p_buffer;
}

void nfc_platform_event_handler(const nrfx_nfct_evt_t *event)
{
	switch (event->evt_id) {
	case NRFX_NFCT_EVT_FIELD_DETECTED:
		/* Field present:
		 * start HFXO asynchronously.
		 * NFCT activation happens in clock_handler() on NRFX_CLOCK_EVT_HFCLK_STARTED. */
		nrfx_clock_start(NRF_CLOCK_DOMAIN_HFCLK);
		break;

	case NRFX_NFCT_EVT_FIELD_LOST:
		/* Errata workaround:
		 * restore FRAMEDELAYMAX default on field loss. */
		nrf_nfct_frame_delay_max_set(NRF_NFCT, NRF_NFCT_FAME_DELAY_MAX_DEFAULT);
		/* Field gone:
		 * NFCT falls back to SENSE mode, so HFXO can be stopped. */
		nrfx_clock_stop(NRF_CLOCK_DOMAIN_HFCLK);
		break;

	default:
		break;
	}
}

/* nrfxlib NFC platform hook (see <nfc_platform.h>).
 * The prebuilt NFC library calls this from NFCT interrupt context to deliver a callback.
 * We dispatch directly by invoking the resolver saved in nfc_platform_setup();
 * the ctx_len/data_len/copy_data args are only for a deferred implementation, so they're unused. */
void nfc_platform_cb_request(const void *p_ctx, size_t ctx_len, const uint8_t *p_data,
			     size_t data_len, bool copy_data)
{
	(void)ctx_len;
	(void)data_len;
	(void)copy_data;

	(*nfc_cb_resolve)(p_ctx, p_data);
}
