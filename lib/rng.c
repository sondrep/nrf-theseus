#include <nrfx_cracen.h>

int theseus_rng_init(void){
    return nrfx_cracen_ctr_drbg_init();
}

int theseus_PRNG_get(uint8_t *buf, size_t size){
    return nrfx_cracen_ctr_drbg_random_get(buf, size);
}

int default_CSPRNG(uint8_t *dest, unsigned int size) {
    return theseus_PRNG_get(dest, size);
}
