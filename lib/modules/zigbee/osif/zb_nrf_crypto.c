/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zboss_api.h>
#include <theseus/rng.h>
#include <tinycrypt/aes.h>
#include <assert.h>

#include "zb_nrf_crypto.h"

#define ECB_AES_KEY_SIZE   16
#define ECB_AES_BLOCK_SIZE 16

void zb_osif_rng_init(void)
{
}

zb_uint32_t zb_random_seed(void)
{
	uint32_t rnd_val;

	theseus_PRNG_get((uint8_t *)(&rnd_val), sizeof(rnd_val));

	return rnd_val;
}

void psa_init(void)
{
}

void zb_osif_aes_init(void)
{
}

void zb_osif_aes128_hw_encrypt(const zb_uint8_t *key, const zb_uint8_t *msg, zb_uint8_t *c)
{
	struct tc_aes_key_sched_struct s;

	tc_aes128_set_encrypt_key(&s, key);
	tc_aes_encrypt(c, msg, &s);
}

zb_int_t zb_osif_scalarmult(zb_uint8_t *result_point, const zb_uint8_t *scalar,
			    const zb_uint8_t *point)
{

	return 0;
}
