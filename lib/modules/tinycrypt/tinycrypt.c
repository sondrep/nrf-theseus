#include <theseus/rng.h>

int default_CSPRNG(uint8_t *dest, unsigned int size)
{
	return theseus_PRNG_get(dest, size);
}
