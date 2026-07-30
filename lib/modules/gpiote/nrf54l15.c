#include <nrf.h>
#include <nrfx_gpiote.h>
#include <theseus/module.h>

/* Interrupt priority for the GPIOTE driver */
#define GPIOTE_IRQ_PRIORITY 3

static nrfx_gpiote_t gpiote = NRFX_GPIOTE_INSTANCE(NRF_GPIOTE20);

static int gpiote_init(void)
{
	NVIC_EnableIRQ(GPIOTE20_IRQn);
	NVIC_SetPriority(GPIOTE20_IRQn, 2);
	return nrfx_gpiote_init(&gpiote, GPIOTE_IRQ_PRIORITY);
}

nrfx_gpiote_t *theseus_gpiote_get(void)
{
	return &gpiote;
}

THESEUS_MODULE_SET(gpiote) = {.init = gpiote_init, .stage = THESEUS_MODULE_STAGE_INTERMEDIARY};

NRFX_INSTANCE_IRQ_HANDLER_DEFINE(gpiote, 20, &gpiote);
