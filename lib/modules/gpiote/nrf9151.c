#include <nrfx_gpiote.h>
#include <theseus/module.h>

/* Interrupt priority for the GPIOTE driver */
#define GPIOTE_IRQ_PRIORITY 3

static nrfx_gpiote_t gpiote = NRFX_GPIOTE_INSTANCE(NRF_GPIOTE);

static int gpiote_init(void)
{
	return nrfx_gpiote_init(&gpiote, GPIOTE_IRQ_PRIORITY);
}

nrfx_gpiote_t *theseus_gpiote_get(void)
{
	return &gpiote;
}

THESEUS_MODULE_SET(gpiote) = {.init = gpiote_init, .stage = THESEUS_MODULE_STAGE_INTERMEDIARY};
