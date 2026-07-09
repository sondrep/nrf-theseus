#include <stdbool.h>
#include <stdint.h>
#include <theseus/rng.h>

void nrf_802154_random_init(void)
{
}

void nrf_802154_random_deinit(void)
{
}

uint32_t nrf_802154_random_get(void)
{
	uint32_t ret;

	theseus_PRNG_get((uint8_t *)(&ret), sizeof(ret));

	return ret;
}
