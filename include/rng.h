#include <stdint.h>
#include <stddef.h>

#ifndef THESEUS_RNG_H__
#define THESEUS_RNG_H__

int theseus_rng_init(void);
int theseus_PRNG_get(uint8_t *buf, size_t size);

#endif
