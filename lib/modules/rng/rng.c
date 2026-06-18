#include <nrfx_cracen.h>
#include <theseus/module.h>

static int rng_init(void)
{
	return nrfx_cracen_ctr_drbg_init();
}

int theseus_PRNG_get(uint8_t *buf, size_t size)
{
	return nrfx_cracen_ctr_drbg_random_get(buf, size);
}

THESEUS_MODULE_SET(rng) = {.init = rng_init};
