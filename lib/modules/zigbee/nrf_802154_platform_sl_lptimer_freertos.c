/*
 * Copyright (c) 2021, Nordic Semiconductor ASA
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY, AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

/**
 * @brief Module that defines the Low Power Timer Abstraction Layer for the nrf_802154_sl_timer
 * service
 *
 * @details
 * Design claims:
 * 1. lptimer works in @c lpticks (Low Power Timer Ticks), it is not aware of other timer units with
 *    exception to convenience functions for calculating lpticks to other units.
 * 2. One @c lptick is at least 1 microsecond, but can be greater.
 * 3. lptimer can rarely fire spurious "timer fired" callout as well as rarely perform spurious
 * (D)PPI triggering (if connections have been made). This phenomena should be avoided, but modules
 * relying on lptimer must be immune to such behavior.
 * 4. lptimer counts @c lpticks as @c uint64_t type, handling (and hiding from higher layers) any
 *    underlying RTC overflow events.
 */

#include <stdint.h>
#include <stdbool.h>
#include <FreeRTOS.h>
#include "timers.h"
#include <nrfx_grtc.h>

static TimerHandle_t lptimer_handle;
static TimerHandle_t lpsync_handle;
static uint8_t channel;

static bool hw_task_state_set(enum hw_task_state_type expected_state,
			      enum hw_task_state_type new_state)
{
	return atomic_cas(&m_hw_task.state, expected_state, new_state);
}

/** @brief Initializes the Timer.
 */
void nrf_802154_platform_sl_lp_timer_init(void)
{
	lptimer_handle = xTimerCreate("nrf_802154 timer", 0, pdFALSE, 0, lptimer_cb);
	lpsync_handle = xTimerCreate("nrf_802154 sync timer", 0, pdFALSE, 0, lpsync_cb);
}

/** @brief Deinitializes the Timer.
 */
void nrf_802154_platform_sl_lp_timer_deinit(void)
{
	xTimerDelete(lptimer_handle, portMAX_DELAY);
	xTimerDelete(lpsync_handle, portMAX_DELAY);
}

/**@brief Returns current state of the lptimer in @c lpticks .
 */
uint64_t nrf_802154_platform_sl_lptimer_current_lpticks_get(void)
{
	return xTimerGetExpiryTime(lptimer_handle) - xTaskGetTickCount();
}

/**@brief Converts time in microseconds to @c lpticks
 *
 * @param[in] us         Number of microseconds to convert.
 * @param[in] round_up   @c true, to force rounding up. @c false to round down.
 *
 * @return Time corresponding to @p us in @c lpticks.
 */
uint64_t nrf_802154_platform_sl_lptimer_us_to_lpticks_convert(uint64_t us, bool round_up)
{
	(void)round_up;
	return pdMS_TO_TICKS(us) / 1000;
}

/**@brief Converts time in @c lpticks to microseconds.
 *
 * @param[in] lpticks   Number of lpticks to convert.
 *
 * @return Time corresponding to @p lpticks in microseconds.
 */
uint64_t nrf_802154_platform_sl_lptimer_lpticks_to_us_convert(uint64_t lpticks)
{
	return pdTICKS_TO_MS(lpticks) * 1000;
}

static void lpsync_cb(TimerHandle_t xTimer)
{
	nrf_802154_sl_timestamper_synchronized();
}

static void lptimer_cb(TimerHandle_t xTimer)
{
	nrf_802154_sl_timer_handler((uint64_t)(xTaskGetTickCount()));
}

/**@brief Schedules a lptimer event to happen at given time.
 *
 * @param[in]   fire_lpticks    Timer state (in @c lpticks) at which timer event should fire.
 *                              If @p fire_lpticks are in the past, the lptimer event will be
 * triggered asap, but still from the context of an lptimer's ISR. If
 *                              @ref nrf_802154_platform_sl_lptimer_critical_section_enter is
 *                              in effect, processing will be delayed until pending interrupt
 *                              can be processed.
 */
void nrf_802154_platform_sl_lptimer_schedule_at(uint64_t fire_lpticks)
{
	xTimerChangePeriod(lptimer_handle, fire_lpticks - xTaskGetTickCount(), portMAX_DELAY);
}

/**@brief Disables generation of any event scheduled by @ref
 * nrf_802154_platform_sl_lptimer_schedule_at
 *
 * @note If called from ISR priority higher than the priority from which
 * @ref nrf_802154_sl_timer_handler is called, the @ref nrf_802154_sl_timer_handler function
 * may be still called after @ref nrf_802154_platform_sl_lptimer_disable. It is due to the fact
 * that ISR can be preempted in such way, that decision to call @ref nrf_802154_sl_timer_handler
 * has already been taken and the call is inevitable (or is already in process of execution)
 */
void nrf_802154_platform_sl_lptimer_disable(void)
{
	xTimerStop(lptimer_handle, portMAX_DELAY);
}

/**@brief Enters into critical section of the lptimer module.
 * @note Critical section can be nested
 *
 * Inside critical section calls to @ref nrf_802154_sl_timer_handler do not happen. This
 * includes indirect effect of calling @ref nrf_802154_platform_sl_lptimer_schedule_at. If necessary
 * the interrupt calling the @ref nrf_802154_sl_timer_handler function it will remain pending
 * at least until a corresponding call to @ref nrf_802154_platform_sl_lptimer_critical_section_exit.
 */
void nrf_802154_platform_sl_lptimer_critical_section_enter(void)
{
	taskENTER_CRITICAL();
}

/**@brief Exits out of critical section of the lptimer module.
 */
void nrf_802154_platform_sl_lptimer_critical_section_exit(void)
{
	taskEXIT_CRITICAL();
}

/**@brief Prepares hardware bindings for triggering hardware task.
 *
 * This function configures timer compare channel to fire at a specified time and publish
 * the signal to a specific PPI channel. The PPI channel may initially be set to
 * @ref NRF_802154_SL_HW_TASK_PPI_INVALID and later updated with
 * @ref nrf_802154_platform_sl_lptimer_hw_task_update_ppi.
 *
 * @param[in]  fire_lpticks  Timer state (in @c lpticks) at which timer event should fire.
 * @param[in]  ppi_channel   (D)PPI channel that compare event will be published to.
 *
 * @retval NRF_802154_SL_LPTIMER_PLATFORM_SUCCESS
 *      The timer was started successfuly and (D)PPI channel was connected, if requested.
 * @retval NRF_802154_SL_LPTIMER_PLATFORM_TOO_LATE
 *      The timer was scheduled too early in the future. The (D)PPI channel was not connected.
 * @retval NRF_802154_SL_LPTIMER_PLATFORM_TOO_DISTANT
 *      The timer was scheduled too far in the future. The (D)PPI channel was not connected.
 * @retval NRF_802154_SL_LPTIMER_PLATFORM_NO_RESOURCES
 *      No available compare channels to schedule a timer. The (D)PPI channel was not connected.
 */
nrf_802154_sl_lptimer_platform_result_t
nrf_802154_platform_sl_lptimer_hw_task_prepare(uint64_t fire_lpticks, uint32_t ppi_channel)
{
	uint32_t evt_address;
	nrf_802154_sl_mcu_critical_state_t mcu_cs_state;

	if (!hw_task_state_set(HW_TASK_STATE_IDLE, HW_TASK_STATE_SETTING_UP)) {
		/* The only one available set of peripherals is already used. */
		return NRF_802154_SL_LPTIMER_PLATFORM_NO_RESOURCES;
	}

	if (hw_task_rtc_timer_set(fire_lpticks) != 0) {
		hw_task_state_set(HW_TASK_STATE_SETTING_UP, HW_TASK_STATE_IDLE);
		uint64_t now = z_nrf_rtc_timer_read();

		if ((fire_lpticks > now) &&
		    (fire_lpticks - now > NRF_RTC_TIMER_MAX_SCHEDULE_SPAN / 2)) {
			return NRF_802154_SL_LPTIMER_PLATFORM_TOO_DISTANT;
		}
		return NRF_802154_SL_LPTIMER_PLATFORM_TOO_LATE;
	}

	evt_address = hw_task_rtc_get_compare_evt_address(m_hw_task.chan);

	nrf_802154_sl_mcu_critical_enter(mcu_cs_state);

	/* For triggering to take place a safe margin is 2 lpticks from `now`. */
	if ((z_nrf_rtc_timer_read() + 2) > fire_lpticks) {
		/* it is too late */
		nrf_802154_sl_mcu_critical_exit(mcu_cs_state);
		hw_task_rtc_cc_unbind(m_hw_task.chan, ppi_channel);
		m_hw_task.ppi = NRF_802154_SL_HW_TASK_PPI_INVALID;
		hw_task_rtc_timer_abort();
		hw_task_state_set(HW_TASK_STATE_SETTING_UP, HW_TASK_STATE_IDLE);
		return NRF_802154_SL_LPTIMER_PLATFORM_TOO_LATE;
	}

	if (ppi_channel != NRF_802154_SL_HW_TASK_PPI_INVALID) {
		nrfx_gppi_ep_to_ch_attach(evt_address, ppi_channel);
	}
	m_hw_task.ppi = ppi_channel;
	m_hw_task.fire_lpticks = fire_lpticks;
	nrf_802154_sl_mcu_critical_exit(mcu_cs_state);
	hw_task_state_set(HW_TASK_STATE_SETTING_UP, HW_TASK_STATE_READY);
	return NRF_802154_SL_LPTIMER_PLATFORM_SUCCESS;
}

/**@brief Removes hardware bindings created for hardware task and stops the timer.
 *
 * @retval NRF_802154_SL_LPTIMER_PLATFORM_SUCCESS
 *      The cleaning was successful.
 * @retval NRF_802154_SL_LPTIMER_PLATFORM_WRONG_STATE
 *      Cleaning was not performed because the module is in an unsuitable state.
 */
nrf_802154_sl_lptimer_platform_result_t nrf_802154_platform_sl_lptimer_hw_task_cleanup(void)
{
}

/**@brief Updates the hardware bindings for triggering hardware task.
 *
 * If the timer was started with @ref nrf_802154_platform_sl_lptimer_hw_task_prepare
 * without specifying a valid (D)PPI channel (@ref NRF_802154_SL_HW_TASK_PPI_INVALID)
 * it is possible to use this function to update the (D)PPI channel to a valid one.
 *
 * @param[in]  ppi_channel  (D)PPI channel that compare event will be published to.
 *
 * @retval NRF_802154_SL_LPTIMER_PLATFORM_SUCCESS
 *      The (D)PPI channel was connected and it was done on time, i.e. before specified
 *      fire time.
 * @retval NRF_802154_SL_LPTIMER_PLATFORM_TOO_LATE
 *      The (D)PPI channel was connected, but it is not sure if it was done in time:
 *      it has been detected that the timer has already fired.
 * @retval NRF_802154_SL_LPTIMER_PLATFORM_WRONG_STATE
 *      The (D)PPI channel was not connected, because the timer is not properly configured.
 */
nrf_802154_sl_lptimer_platform_result_t
nrf_802154_platform_sl_lptimer_hw_task_update_ppi(uint32_t ppi_channel)
{
}

/**
 * @brief Starts a one-shot synchronization timer that expires at the nearest possible timepoint.
 *
 * On timer expiration, the @ref nrf_802154_sl_timestamper_synchronized function is called and the
 * event returned by @ref nrf_802154_platform_sl_lptimer_sync_event_get is triggered.
 *
 * @note @ref nrf_802154_sl_timestamper_synchronized may be called multiple times.
 */
void nrf_802154_platform_sl_lptimer_sync_schedule_now(void)
{
	xTimerChangePeriod(lpsync_handle, 1, portMAX_DELAY);
}

/**
 * @brief Starts a one-shot synchronization timer that expires at the specified time.
 *
 * This function starts a one-shot synchronization timer that expires at @p fire_lpticks.
 *
 * On timer expiration, @ref nrf_802154_sl_timestamper_synchronized function is called and
 * the event returned by @ref nrf_802154_platform_sl_lptimer_sync_event_get is triggered.
 *
 * @param[in]  fire_lpticks  Timer state (in @c lpticks) at which timer event should fire.
 */
void nrf_802154_platform_sl_lptimer_sync_schedule_at(uint64_t fire_lpticks)
{
	xTimerChangePeriod(lpsync_handle, fire_lpticks, portMAX_DELAY);
}

/**
 * @brief Stops the currently running synchronization timer.
 */
void nrf_802154_platform_sl_lptimer_sync_abort(void)
{
	xTimerStop(lpsync_handle, portMAX_DELAY);
}

/**
 * @brief Gets the event used to synchronize this timer with the HP Timer.
 *
 * @returns  Address of the peripheral register corresponding to the event
 *           to be used for the timer synchronization.
 *
 */
uint32_t nrf_802154_platform_sl_lptimer_sync_event_get(void)
{
	return nrfx_grtc_event_compare_address_get(channel);
}

/**
 * @brief Gets the timestamp of the synchronization event.
 *
 * @returns  Timestamp of the synchronization event.
 */
uint64_t nrf_802154_platform_sl_lptimer_sync_lpticks_get(void)
{
}

/**
 * @brief Gets the granularity of the timer.
 *
 * This function can be used to round up or round down the time calculations.
 *
 * @returns Timer granularity in microseconds.
 */
uint32_t nrf_802154_platform_sl_lptimer_granularity_get(void)
{
}
