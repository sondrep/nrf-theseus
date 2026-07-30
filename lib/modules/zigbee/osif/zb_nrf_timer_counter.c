/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zboss_api.h>
#include <zb_types.h>
#include <zb_osif_platform.h>
#include <FreeRTOS.h>
#include <timers.h>
#include <nrfx_glue.h>

#define ALARM_CHANNEL_ID 0

typedef struct {
	uint32_t counter_period_us;
	uint32_t counter_acc_us;
	uint8_t alarm_ch_id;
	volatile zb_bool_t is_init;
	volatile nrfx_atomic_t is_running;
	TimerHandle_t handle;
} zb_timer_t;

static zb_timer_t zb_timer = {.is_init = ZB_FALSE};

/* Forward declaration, dependency to ZBOSS */
void zb_osif_zboss_timer_tick(void);

/* Timer interrupt handler. */
static void zb_timer_alarm_handler(TimerHandle_t xTimer)
{
	(void)xTimer;

	if (__atomic_exchange_n(&zb_timer.is_running, 0, __ATOMIC_SEQ_CST) == 1) {
		/* The atomic flag is_running was 1 and now set to 0. */
		zb_timer.counter_acc_us += zb_timer.counter_period_us;

		/* ZBOSS reschedules the timer inside the
		 * zb_osif_zboss_timer_tick(), so it is required to
		 * reshedule it manually if the function will not be
		 * called at the end of this function.
		 */
		if (zb_timer.counter_acc_us < ZB_BEACON_INTERVAL_USEC) {
			zb_osif_timer_start();
		}
	}

	if (zb_timer.counter_acc_us >= ZB_BEACON_INTERVAL_USEC) {
		zb_timer.counter_acc_us -= ZB_BEACON_INTERVAL_USEC;
		zb_osif_zboss_timer_tick();
	}
}

static void zb_timer_init(void)
{
	zb_timer.counter_period_us = ZB_BEACON_INTERVAL_USEC / 2;
	zb_timer.counter_acc_us = 0;
	zb_timer.handle =
		xTimerCreate("zb_timer", pdMS_TO_TICKS(ZB_BEACON_INTERVAL_USEC / 2) / 1000, pdFALSE,
			     NULL, zb_timer_alarm_handler);
	zb_timer.is_init = ZB_TRUE;
}

void zb_osif_timer_stop(void)
{
	if (__atomic_exchange_n(&zb_timer.is_running, 0, __ATOMIC_SEQ_CST) == 1) {
		/* The atomic flag is_running was 1 and now set to 0. */
		xTimerStop(zb_timer.handle, portMAX_DELAY);
	}
}

void zb_osif_timer_start(void)
{
	if (__atomic_exchange_n(&zb_timer.is_running, 1, __ATOMIC_SEQ_CST) == 0) {
		/* The atomic flag is_running was 0 and now set to 1. */
		if (zb_timer.is_init == ZB_FALSE) {
			zb_timer_init();
		}
		xTimerStart(zb_timer.handle, portMAX_DELAY);
	}
}

zb_bool_t zb_osif_timer_is_on(void)
{
	return __atomic_load_n(&zb_timer.is_running, __ATOMIC_SEQ_CST) ? ZB_TRUE : ZB_FALSE;
}

/*
 * Get current time, us.
 */
zb_uint64_t osif_transceiver_time_get_long(void)
{
	zb_uint64_t time_sys;
	zb_uint64_t time_cur;

	if (zb_osif_timer_is_on() == ZB_TRUE) {
		time_cur =
			pdTICKS_TO_MS(xTimerGetExpiryTime(zb_timer.handle) - xTaskGetTickCount()) *
			1000;
	} else {
		time_cur = 0;
	}
	time_sys = ZB_TIME_BEACON_INTERVAL_TO_USEC(ZB_TIMER_GET());

	return time_sys + time_cur;
}

zb_time_t osif_transceiver_time_get(void)
{
	return (zb_time_t)osif_transceiver_time_get_long();
}

void osif_sleep_using_transc_timer(zb_time_t timeout_us)
{
	vTaskDelay(pdMS_TO_TICKS(timeout_us) / 1000);
}
