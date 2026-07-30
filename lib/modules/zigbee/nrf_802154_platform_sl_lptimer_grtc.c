/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "platform/nrf_802154_platform_sl_lptimer.h"
#include "nrf_802154_platform_sl_lptimer_grtc_hw_task.h"

#include <assert.h>

#include <nrfx_grtc.h>

#include "nrf_802154_sl_config.h"
#include "nrf_802154_sl_atomics.h"
#include "nrf_802154_sl_utils.h"
#include "timer/nrf_802154_timer_coord.h"
// #include "nrf_802154_sl_periphs.h"

/* TODO: Check if this is correct, I just guessed */
#define BIT(n) (1 << n)

static unsigned int m_enabled;
static nrfx_atomic_t int_mask;
static bool m_compare_int_lock_key;
static uint32_t m_critical_section_cnt;
static uint8_t m_callbacks_cc_channel;
static uint8_t m_hw_task_cc_channel;

static inline bool is_lptimer_enabled(void)
{
	return __atomic_load_n(&m_enabled, __ATOMIC_SEQ_CST) != 0;
}

static int compare_set_nolocks(int32_t chan, uint64_t target_time, nrfx_grtc_cc_handler_t handler,
			       void *user_data)
{
	nrfx_grtc_channel_t user_channel_data = {
		.handler = handler,
		.p_context = user_data,
		.channel = chan,
	};
	return nrfx_grtc_syscounter_cc_absolute_set(&user_channel_data, target_time, true);
}

static bool compare_int_lock(int32_t chan)
{
	unsigned int prev = __atomic_fetch_and(&int_mask, ~BIT(chan), __ATOMIC_SEQ_CST);

	nrfx_grtc_syscounter_cc_int_disable(chan);

	return prev & BIT(chan);
}

static void compare_int_unlock(int32_t chan, bool key)
{
	if (key) {
		__atomic_fetch_or(&int_mask, BIT(chan), __ATOMIC_SEQ_CST);
		nrfx_grtc_syscounter_cc_int_enable(chan);
	}
}

static void timer_compare_handler(int32_t id, uint64_t expire_time, void *user_data)
{
	(void)user_data;
	(void)expire_time;

	assert(id == m_callbacks_cc_channel);

	if (!is_lptimer_enabled()) {
		/* The interrupt is late. Ignore it */
		return;
	}

	uint64_t curr_ticks = nrfx_grtc_syscounter_get();

	nrf_802154_sl_timer_handler(curr_ticks);
}

void nrf_802154_platform_sl_lp_timer_init(void)
{
	m_critical_section_cnt = 0UL;

	nrfx_grtc_channel_alloc(&m_callbacks_cc_channel);
	nrfx_grtc_channel_alloc(&m_hw_task_cc_channel);

	/* TODO: What is this function? Joe? */
	nrf_802154_platform_sl_lptimer_hw_task_cross_domain_connections_setup(m_hw_task_cc_channel);
	int_mask = NRFX_GRTC_CONFIG_ALLOWED_CC_CHANNELS_MASK;
}

void nrf_802154_platform_sl_lp_timer_deinit(void)
{
	nrfx_grtc_syscounter_cc_int_disable(m_callbacks_cc_channel);

	nrfx_grtc_channel_free(m_callbacks_cc_channel);
	nrfx_grtc_channel_free(m_hw_task_cc_channel);
}

uint64_t nrf_802154_platform_sl_lptimer_current_lpticks_get(void)
{
	return nrfx_grtc_syscounter_get();
}

uint64_t nrf_802154_platform_sl_lptimer_us_to_lpticks_convert(uint64_t us, bool round_up)
{
	(void)round_up;

	return us;
}

uint64_t nrf_802154_platform_sl_lptimer_lpticks_to_us_convert(uint64_t lpticks)
{
	return lpticks;
}

void nrf_802154_platform_sl_lptimer_schedule_at(uint64_t fire_lpticks)
{
	/* This function is not required to be reentrant, hence no critical section. */
	__atomic_test_and_set(&m_enabled, __ATOMIC_SEQ_CST);

	/* TODO: Fix locks */
	bool key = compare_int_lock(m_callbacks_cc_channel);
	compare_set_nolocks(m_callbacks_cc_channel, fire_lpticks, timer_compare_handler, NULL);
	compare_int_unlock(m_callbacks_cc_channel, key);
}

void nrf_802154_platform_sl_lptimer_disable(void)
{
	__atomic_clear(&m_enabled, __ATOMIC_SEQ_CST);

	/* TODO: Fix locks */
	bool key = compare_int_lock(m_callbacks_cc_channel);
	nrfx_grtc_syscounter_cc_disable(m_callbacks_cc_channel);
	compare_int_unlock(m_callbacks_cc_channel, key);
}

void nrf_802154_platform_sl_lptimer_critical_section_enter(void)
{
	nrf_802154_sl_mcu_critical_state_t state;

	nrf_802154_sl_mcu_critical_enter(state);

	m_critical_section_cnt++;

	if (m_critical_section_cnt == 1UL) {
		unsigned int prev = __atomic_fetch_and(&int_mask, ~BIT(m_callbacks_cc_channel),
						       __ATOMIC_SEQ_CST);

		nrfx_grtc_syscounter_cc_int_disable(m_callbacks_cc_channel);

		m_compare_int_lock_key = prev & BIT(m_callbacks_cc_channel);
	}

	nrf_802154_sl_mcu_critical_exit(state);
}

void nrf_802154_platform_sl_lptimer_critical_section_exit(void)
{
	nrf_802154_sl_mcu_critical_state_t state;

	nrf_802154_sl_mcu_critical_enter(state);

	if (m_compare_int_lock_key) {
		__atomic_fetch_or(&int_mask, BIT(m_callbacks_cc_channel), __ATOMIC_SEQ_CST);
		nrfx_grtc_syscounter_cc_int_enable(m_callbacks_cc_channel);
	}

	m_critical_section_cnt--;

	nrf_802154_sl_mcu_critical_exit(state);
}

typedef uint8_t hw_task_state_t;
#define HW_TASK_STATE_IDLE	 0u
#define HW_TASK_STATE_SETTING_UP 1u
#define HW_TASK_STATE_READY	 2u
#define HW_TASK_STATE_CLEANING	 3u
#define HW_TASK_STATE_UPDATING	 4u

static volatile hw_task_state_t m_hw_task_state = HW_TASK_STATE_IDLE;
static uint64_t m_hw_task_fire_lpticks;

static bool hw_task_state_set(hw_task_state_t expected_state, hw_task_state_t new_state)
{
	return nrf_802154_sl_atomic_cas_u8((uint8_t *)&m_hw_task_state, &expected_state, new_state);
}

nrf_802154_sl_lptimer_platform_result_t
nrf_802154_platform_sl_lptimer_hw_task_prepare(uint64_t fire_lpticks, uint32_t ppi_channel)
{
	const uint64_t grtc_cc_minimum_margin = 1uLL;
	uint64_t syscnt_now;
	bool done_on_time = true;
	nrf_802154_sl_mcu_critical_state_t mcu_cs_state;

	if (!hw_task_state_set(HW_TASK_STATE_IDLE, HW_TASK_STATE_SETTING_UP)) {
		/* the only one available set of peripherals is already used */
		return NRF_802154_SL_LPTIMER_PLATFORM_NO_RESOURCES;
	}

	nrf_802154_platform_sl_lptimer_hw_task_local_domain_connections_setup(ppi_channel,
									      m_hw_task_cc_channel);

	m_hw_task_fire_lpticks = fire_lpticks;

	/* TODO: Fix locks */
	bool key = compare_int_lock(m_hw_task_cc_channel);
	compare_set_nolocks(m_hw_task_cc_channel, fire_lpticks, NULL, NULL);
	compare_int_unlock(m_hw_task_cc_channel, key);

	nrf_802154_sl_mcu_critical_enter(mcu_cs_state);

	/* @todo: can this read be done outside of critical section? */
	syscnt_now = nrfx_grtc_syscounter_get();

	if (syscnt_now + grtc_cc_minimum_margin >= fire_lpticks) {
		/* it is too late */
		nrf_802154_platform_sl_lptimer_hw_task_local_domain_connections_clear();
		/* TODO: Fix locks */
		bool key = compare_int_lock(m_hw_task_cc_channel);
		nrfx_grtc_syscounter_cc_disable(m_hw_task_cc_channel);
		compare_int_unlock(m_hw_task_cc_channel, key);
		done_on_time = false;
	}

	nrf_802154_sl_mcu_critical_exit(mcu_cs_state);

	hw_task_state_set(HW_TASK_STATE_SETTING_UP,
			  done_on_time ? HW_TASK_STATE_READY : HW_TASK_STATE_IDLE);

	return done_on_time ? NRF_802154_SL_LPTIMER_PLATFORM_SUCCESS
			    : NRF_802154_SL_LPTIMER_PLATFORM_TOO_LATE;
}

nrf_802154_sl_lptimer_platform_result_t nrf_802154_platform_sl_lptimer_hw_task_cleanup(void)
{
	if (!hw_task_state_set(HW_TASK_STATE_READY, HW_TASK_STATE_CLEANING)) {
		return NRF_802154_SL_LPTIMER_PLATFORM_WRONG_STATE;
	}

	nrf_802154_platform_sl_lptimer_hw_task_local_domain_connections_clear();

	/* TODO: Fix locks */
	bool key = compare_int_lock(m_hw_task_cc_channel);
	nrfx_grtc_syscounter_cc_disable(m_hw_task_cc_channel);
	compare_int_unlock(m_hw_task_cc_channel, key);

	hw_task_state_set(HW_TASK_STATE_CLEANING, HW_TASK_STATE_IDLE);

	return NRF_802154_SL_LPTIMER_PLATFORM_SUCCESS;
}

nrf_802154_sl_lptimer_platform_result_t
nrf_802154_platform_sl_lptimer_hw_task_update_ppi(uint32_t ppi_channel)
{
	bool cc_triggered;

	if (!hw_task_state_set(HW_TASK_STATE_READY, HW_TASK_STATE_UPDATING)) {
		return NRF_802154_SL_LPTIMER_PLATFORM_WRONG_STATE;
	}

	nrf_802154_platform_sl_lptimer_hw_task_local_domain_connections_setup(ppi_channel,
									      m_hw_task_cc_channel);

	cc_triggered = (*(volatile uint32_t *)nrfx_grtc_event_compare_address_get(
				m_hw_task_cc_channel) != 0);
	if (nrfx_grtc_syscounter_get() >= m_hw_task_fire_lpticks) {
		cc_triggered = true;
	}

	hw_task_state_set(HW_TASK_STATE_UPDATING, HW_TASK_STATE_READY);

	return cc_triggered ? NRF_802154_SL_LPTIMER_PLATFORM_TOO_LATE
			    : NRF_802154_SL_LPTIMER_PLATFORM_SUCCESS;
}
