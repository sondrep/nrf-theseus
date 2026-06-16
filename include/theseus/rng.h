#include <stdint.h>
#include <stddef.h>

#ifndef THESEUS_RNG_H
#define THESEUS_RNG_H

int theseus_rng_init(void);
int theseus_PRNG_get(uint8_t *buf, size_t size);

#endif /* THESEUS_RNG_H */
