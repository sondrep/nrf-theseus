#include <theseus/module.h>
#include <nrfx_grtc.h>

#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <FreeRTOS.h>

static uint8_t global_grtc_channel;

uint8_t theseus_grtc_global_channel_get(void)
{
	return global_grtc_channel;
}

static int grtc_lfclk_init(void)
{
	assert(nrfx_grtc_init_check() == false);
	nrfx_grtc_clock_source_set(NRF_GRTC_CLKSEL_LFXO);

	assert(nrfx_grtc_init(configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY) == 0);

	assert(nrfx_grtc_syscounter_start(false, &global_grtc_channel) == 0);
	assert(nrfx_grtc_ready_check() == true);
	return 0;
}

THESEUS_MODULE_SET(grtc) = {.init = grtc_lfclk_init, .stage = THESEUS_MODULE_STAGE_EARLY};
