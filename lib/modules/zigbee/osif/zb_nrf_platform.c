/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdlib.h>
#include <string.h>
#include <FreeRTOS.h>
#include <queue.h>
#include <semphr.h>
#include <task.h>
#include <theseus/log.h>
#include <assert.h>
#include <theseus/module.h>
// #include <ram_pwrdn.h>

#include <hal/nrf_power.h>
#include <hal/nrf_ficr.h>
#if !NRF_POWER_HAS_RESETREAS
#include <hal/nrf_reset.h>
#endif

#ifdef CONFIG_ZIGBEE_SHELL
#include <zigbee/zigbee_shell.h>
#endif
#include <zboss_api.h>
#include "zb_nrf_platform.h"
#include "zb_nrf_crypto.h"

#ifdef CONFIG_ZIGBEE_LIBRARY_NCP_DEV
#include <zb_ncp_nrf_platform.h>
#define SYS_REBOOT_NCP 0x10
#endif /* CONFIG_ZIGBEE_LIBRARY_NCP_DEV */

/* Value that is returned while reading a single byte from the erased flash page .*/
#define FLASH_EMPTY_BYTE       0xFF
/* Broadcast Pan ID value */
#define ZB_BROADCAST_PAN_ID    0xFFFFU
/* The number of bytes to be checked before concluding that the ZBOSS NVRAM is not initialized. */
#define ZB_PAGE_INIT_CHECK_LEN 32

/* EUI64 address configuration */
#if defined(CONFIG_ZIGBEE_UICR_EUI64_ENABLE)
#if defined(CONFIG_SOC_NRF5340_CPUAPP) || defined(CONFIG_SOC_SERIES_NRF54LX)
#define EUI64_ADDR (NRF_UICR->OTP)
#else
#define EUI64_ADDR (NRF_UICR->CUSTOMER)
#endif
#define EUI64_ADDR_HIGH CONFIG_ZIGBEE_UICR_EUI64_REG
#define EUI64_ADDR_LOW	(CONFIG_ZIGBEE_UICR_EUI64_REG + 1)
#else
#define EUI64_ADDR_HIGH 0
#define EUI64_ADDR_LOW	1
#endif /* CONFIG_ZIGBEE_UICR_EUI64_ENABLE */

/**
 * Enumeration representing type of application callback to execute from ZBOSS
 * context.
 */
typedef enum {
	ZB_CALLBACK_TYPE_SINGLE_PARAM,
	ZB_CALLBACK_TYPE_TWO_PARAMS,
	ZB_CALLBACK_TYPE_ALARM_SET,
	ZB_CALLBACK_TYPE_ALARM_CANCEL,
	ZB_GET_OUT_BUF_DELAYED,
	ZB_GET_IN_BUF_DELAYED,
	ZB_GET_OUT_BUF_DELAYED_EXT,
	ZB_GET_IN_BUF_DELAYED_EXT,
} zb_callback_type_t;

/**
 * Type definition of element of the application callback and alarm queue.
 */
typedef struct {
	zb_callback_type_t type;
	zb_callback_t func;
	zb_callback2_t func2;
	zb_uint16_t param;
	zb_uint16_t user_param;
	int64_t alarm_timestamp;
} zb_app_cb_t;

// BUILD_ASSERT(CONFIG_ZBOSS_INIT_PRIORITY > CONFIG_ZBOSS_RADIO_INIT_PRIORITY,
//	     "ZBOSS init priority must be greater than radio init priority");

/** Global mutex to protect access to the ZBOSS global state.
 *
 * @note Functions for locking/unlocking the mutex are called directly from
 *       ZBOSS core, when the main ZBOSS global variable is accessed.
 */
static SemaphoreHandle_t zigbee_mutex;

/**
 * Message queue, that is used to pass ZBOSS callbacks and alarms from
 * ISR and other threads to ZBOSS main loop context.
 */
static QueueHandle_t zb_app_cb_msgq;

static SemaphoreHandle_t zigbee_event_semaphore;
static SemaphoreHandle_t schedule_semaphore;

/**
 * Atomic flag, indicating that the processing callback is still scheduled for
 * execution,
 */
volatile unsigned int zb_app_cb_process_scheduled;

static TaskHandle_t zboss_tid;
static bool stack_is_started;

#ifdef CONFIG_ZIGBEE_DEBUG_FUNCTIONS
/**@brief Function for checking if the ZBOSS thread has been created.
 */
bool zigbee_debug_zboss_thread_is_created(void)
{
	if (zboss_tid) {
		return true;
	}
	return false;
}

/**@brief Function for suspending ZBOSS thread.
 */
void zigbee_debug_suspend_zboss_thread(void)
{
	vTaskSuspend(zboss_tid);
}

/**@brief Function for resuming ZBOSS thread.
 */
void zigbee_debug_resume_zboss_thread(void)
{
	vTaskResume(zboss_tid);
}

/**@brief Function for getting the state of the Zigbee stack thread
 *        processing suspension.
 */
bool zigbee_is_zboss_thread_suspended(void)
{
	if (eTaskGetState(zboss_tid) == eSuspended) {
		return false;
	}
	return true;
}
#endif /* defined(CONFIG_ZIGBEE_DEBUG_FUNCTIONS) */

/**@brief Function for checking if the Zigbee stack has been started.
 *
 * @retval true   Zigbee stack has been started.
 * @retval false  Zigbee stack has not been started yet.
 */
bool zigbee_is_stack_started(void)
{
	return stack_is_started;
}

static void zb_app_cb_process(zb_bufid_t bufid)
{
	zb_ret_t ret_code = RET_OK;
	zb_app_cb_t new_app_cb;

	/* Mark the processing callback as non-scheduled. */
	__atomic_exchange_n(&zb_app_cb_process_scheduled, 0, __ATOMIC_SEQ_CST);
	//(void)atomic_set((atomic_t *)&zb_app_cb_process_scheduled, 0);

	/**
	 * From ZBOSS main loop context: process all requests.
	 *
	 * Note: the ZB_SCHEDULE_APP_ALARM is not thread-safe.
	 */
	while (xQueuePeek(zb_app_cb_msgq, &new_app_cb, 0) == pdTRUE) {
		switch (new_app_cb.type) {
		case ZB_CALLBACK_TYPE_SINGLE_PARAM:
			ret_code = zb_schedule_app_callback(new_app_cb.func,
							    (zb_uint8_t)new_app_cb.param);
			break;
		case ZB_CALLBACK_TYPE_TWO_PARAMS:
			ret_code = zb_schedule_app_callback2(new_app_cb.func2,
							     (zb_uint8_t)new_app_cb.param,
							     new_app_cb.user_param);
			break;
		case ZB_CALLBACK_TYPE_ALARM_SET: {
			/**
			 * Check if the timeout already passed. If so, use the
			 * lowest value that schedules an alarm, so the user
			 * is still able to cancel the alarm.
			 */
			zb_time_t delay =
				pdTICKS_TO_MS(xTaskGetTickCount()) > new_app_cb.alarm_timestamp
					? 1
					: ZB_MILLISECONDS_TO_BEACON_INTERVAL(
						  new_app_cb.alarm_timestamp -
						  (pdTICKS_TO_MS(xTaskGetTickCount())));
			ret_code = zb_schedule_app_alarm(new_app_cb.func,
							 (zb_uint8_t)new_app_cb.param, delay);
			break;
		}
		case ZB_CALLBACK_TYPE_ALARM_CANCEL:
			ret_code = zb_schedule_alarm_cancel(new_app_cb.func,
							    (zb_uint8_t)new_app_cb.param, NULL);
			break;
		case ZB_GET_OUT_BUF_DELAYED:
			ret_code = zb_buf_get_out_delayed_func(TRACE_CALL(new_app_cb.func));
			break;
		case ZB_GET_IN_BUF_DELAYED:
			ret_code = zb_buf_get_in_delayed_func(TRACE_CALL(new_app_cb.func));
			break;
		case ZB_GET_OUT_BUF_DELAYED_EXT:
			ret_code = zb_buf_get_out_delayed_ext_func(TRACE_CALL(new_app_cb.func2),
								   new_app_cb.user_param,
								   new_app_cb.param);
			break;
		case ZB_GET_IN_BUF_DELAYED_EXT:
			ret_code = zb_buf_get_in_delayed_ext_func(TRACE_CALL(new_app_cb.func2),
								  new_app_cb.user_param,
								  new_app_cb.param);
			break;
		default:
			break;
		}

		/* Check for ZBOSS scheduler queue overflow. */
		if (ret_code == RET_OVERFLOW) {
			break;
		}

		/* Flush the element from the message queue. */
		xQueueReceive(zb_app_cb_msgq, &new_app_cb, 0);
	}

	/**
	 * In case of overflow error - reschedule the processing callback
	 * to process remaining requests later.
	 */
	if (ret_code == RET_OVERFLOW) {
		xSemaphoreGive(schedule_semaphore);
	}
}

static void zb_app_cb_process_schedule(void *arg)
{
	zb_app_cb_t new_app_cb;
	(void)arg;

	while (1) {

		xSemaphoreTake(schedule_semaphore, portMAX_DELAY);

		if (xQueuePeek(zb_app_cb_msgq, &new_app_cb, 0) != pdTRUE) {
			continue;
		}

		/* Check if processing callback is already scheduled. */
		if (__atomic_exchange_n(&zb_app_cb_process_scheduled, 1, __ATOMIC_SEQ_CST) == 1) {
			continue;
		}

		/**
		 * From working thread, non-ISR context: schedule processing callback.
		 * Repeat endlessly, because the user was already informed that the
		 * request will be handled.
		 *
		 * Note: the ZB_SCHEDULE_APP_CALLBACK is thread-safe.
		 */
		while (zb_schedule_app_callback(zb_app_cb_process, 0) != RET_OK) {
			vTaskDelay(pdMS_TO_TICKS(1000));
		}
		zigbee_event_notify(ZIGBEE_EVENT_APP);
	}
}

int zigbee_init(void)
{
	/* Initialise work queue for processing app callback and alarms. */
	zb_app_cb_msgq = xQueueCreate(CONFIG_ZIGBEE_APP_CB_QUEUE_LENGTH, sizeof(zb_app_cb_t));
	zigbee_mutex = xSemaphoreCreateMutex();
	zigbee_event_semaphore = xSemaphoreCreateBinary();
	schedule_semaphore = xSemaphoreCreateBinary();
	xTaskCreate(zb_app_cb_process_schedule, "zigbee work", 1024, NULL, 2, NULL);
	// k_work_init(&zb_app_cb_work, zb_app_cb_process_schedule);

#if ZB_TRACE_LEVEL
	/* Set Zigbee stack logging level and traffic dump subsystem. */
	ZB_SET_TRACE_LEVEL(CONFIG_ZBOSS_TRACE_LOG_LEVEL);
	ZB_SET_TRACE_MASK(CONFIG_ZBOSS_TRACE_MASK);
#if CONFIG_ZBOSS_TRAF_DUMP
	ZB_SET_TRAF_DUMP_ON();
#else  /* CONFIG_ZBOSS_TRAF_DUMP */
	ZB_SET_TRAF_DUMP_OFF();
#endif /* CONFIG_ZBOSS_TRAF_DUMP */
#endif /* ZB_TRACE_LEVEL */

#ifndef CONFIG_ZB_TEST_MODE_MAC
	/* Initialize Zigbee stack. */
	ZB_INIT("zigbee_thread");

	/* Set device address to the value read from FICR registers. */
	zb_ieee_addr_t ieee_addr;
	zb_osif_get_ieee_eui64(ieee_addr);
	zb_set_long_address(ieee_addr);

	/* Keep or erase NVRAM to save the network parameters
	 * after device reboot or power-off.
	 */
	zb_set_nvram_erase_at_start(ZB_FALSE);

	/* Don't set zigbee role for NCP device */
#ifndef CONFIG_ZIGBEE_LIBRARY_NCP_DEV

	/* Set channels on which the coordinator will try
	 * to create a new network
	 */
#if defined(CONFIG_ZIGBEE_CHANNEL_SELECTION_MODE_SINGLE)
	zb_uint32_t channel_mask = (1UL << CONFIG_ZIGBEE_CHANNEL);
#elif defined(CONFIG_ZIGBEE_CHANNEL_SELECTION_MODE_MULTI)
	zb_uint32_t channel_mask = CONFIG_ZIGBEE_CHANNEL_MASK;
#else
#error Channel mask undefined!
#endif

#if defined(CONFIG_ZIGBEE_ROLE_COORDINATOR)
	zb_set_network_coordinator_role(channel_mask);
#elif defined(CONFIG_ZIGBEE_ROLE_ROUTER)
	zb_set_network_router_role(channel_mask);

#elif defined(CONFIG_ZIGBEE_ROLE_END_DEVICE)
	zb_set_network_ed_role(channel_mask);
#else
#error Zigbee device role undefined!
#endif

#endif /* CONFIG_ZIGBEE_LIBRARY_NCP_DEV */

#endif /* CONFIG_ZB_TEST_MODE_MAC */

	return 0;
}

THESEUS_MODULE_SET(zigbee) = {.init = zigbee_init, .stage = THESEUS_MODULE_STAGE_INTERMEDIARY};

void zigbee_deinit(void)
{
	if (zboss_tid) {
		vTaskDelete(zboss_tid);
		zboss_tid = NULL;
	}

	stack_is_started = false;
	//(void)k_work_cancel(&zb_app_cb_work);
	// k_msgq_purge(&zb_app_cb_msgq);
	// k_poll_signal_reset(&zigbee_sig);
	//(void)atomic_clear((atomic_t *)&zb_app_cb_process_scheduled);
}

static void zboss_thread(void *arg1)
{
	zb_ret_t err;

	(void)arg1;

	err = zboss_start_no_autostart();
	assert(err == RET_OK);

	stack_is_started = true;

	for (;;) {
		zboss_main_loop();
	}
}

zb_bool_t zb_osif_is_inside_isr(void)
{
	return (zb_bool_t)(__get_IPSR() != 0);
}

void zb_osif_enable_all_inter(void)
{
	assert(zb_osif_is_inside_isr() == 0);
	xSemaphoreGive(zigbee_mutex);
}

void zb_osif_disable_all_inter(void)
{
	assert(zb_osif_is_inside_isr() == 0);
	xSemaphoreTake(zigbee_mutex, portMAX_DELAY);
}

zb_ret_t zigbee_schedule_callback(zb_callback_t func, zb_uint8_t param)
{
	if ((zboss_tid) && (xTaskGetCurrentTaskHandle() == zboss_tid) &&
	    (!zb_osif_is_inside_isr())) {
		return zb_schedule_app_callback(func, param);
	}

	zb_app_cb_t new_app_cb = {
		.type = ZB_CALLBACK_TYPE_SINGLE_PARAM,
		.func = func,
		.param = param,
	};

	if (xQueueSend(zb_app_cb_msgq, &new_app_cb, 0) != pdTRUE) {
		return RET_OVERFLOW;
	}

	xSemaphoreGive(schedule_semaphore);
	return RET_OK;
}

zb_ret_t zigbee_schedule_callback2(zb_callback2_t func, zb_uint8_t param, zb_uint16_t user_param)
{
	if ((zboss_tid) && (xTaskGetCurrentTaskHandle() == zboss_tid) &&
	    (!zb_osif_is_inside_isr())) {
		return zb_schedule_app_callback2(func, param, user_param);
	}

	zb_app_cb_t new_app_cb = {
		.type = ZB_CALLBACK_TYPE_TWO_PARAMS,
		.func2 = func,
		.param = param,
		.user_param = user_param,
	};

	if (xQueueSend(zb_app_cb_msgq, &new_app_cb, 0) != pdTRUE) {
		return RET_OVERFLOW;
	}

	xSemaphoreGive(schedule_semaphore);
	return RET_OK;
}

zb_ret_t zigbee_schedule_alarm(zb_callback_t func, zb_uint8_t param, zb_time_t run_after)
{
	if ((zboss_tid) && (xTaskGetCurrentTaskHandle() == zboss_tid) &&
	    (!zb_osif_is_inside_isr())) {
		return zb_schedule_app_alarm(func, param, run_after);
	}

	zb_app_cb_t new_app_cb = {
		.type = ZB_CALLBACK_TYPE_ALARM_SET,
		.func = func,
		.param = param,
		.alarm_timestamp = pdTICKS_TO_MS(xTaskGetTickCount()) +
				   ZB_TIME_BEACON_INTERVAL_TO_MSEC(run_after),
	};

	if (xQueueSend(zb_app_cb_msgq, &new_app_cb, 0) != pdTRUE) {
		return RET_OVERFLOW;
	}

	xSemaphoreGive(schedule_semaphore);
	return RET_OK;
}

zb_ret_t zigbee_schedule_alarm_cancel(zb_callback_t func, zb_uint8_t param)
{
	if ((zboss_tid) && (xTaskGetCurrentTaskHandle() == zboss_tid) &&
	    (!zb_osif_is_inside_isr())) {
		return zb_schedule_alarm_cancel(func, param, NULL);
	}

	zb_app_cb_t new_app_cb = {
		.type = ZB_CALLBACK_TYPE_ALARM_CANCEL,
		.func = func,
		.param = param,
	};

	if (xQueueSend(zb_app_cb_msgq, &new_app_cb, 0) != pdTRUE) {
		return RET_OVERFLOW;
	}

	xSemaphoreGive(schedule_semaphore);
	return RET_OK;
}

zb_ret_t zigbee_get_out_buf_delayed(zb_callback_t func)
{
	if ((zboss_tid) && (xTaskGetCurrentTaskHandle() == zboss_tid) &&
	    (!zb_osif_is_inside_isr())) {
		return zb_buf_get_out_delayed_func(func);
	}

	zb_app_cb_t new_app_cb = {
		.type = ZB_GET_OUT_BUF_DELAYED,
		.func = func,
	};

	if (xQueueSend(zb_app_cb_msgq, &new_app_cb, 0) != pdTRUE) {
		return RET_OVERFLOW;
	}

	xSemaphoreGive(schedule_semaphore);
	return RET_OK;
}

zb_ret_t zigbee_get_in_buf_delayed(zb_callback_t func)
{
	if ((zboss_tid) && (xTaskGetCurrentTaskHandle() == zboss_tid) &&
	    (!zb_osif_is_inside_isr())) {
		return zb_buf_get_in_delayed_func(func);
	}

	zb_app_cb_t new_app_cb = {
		.type = ZB_GET_IN_BUF_DELAYED,
		.func = func,
	};

	if (xQueueSend(zb_app_cb_msgq, &new_app_cb, 0) != pdTRUE) {
		return RET_OVERFLOW;
	}

	xSemaphoreGive(schedule_semaphore);
	return RET_OK;
}

zb_ret_t zigbee_get_out_buf_delayed_ext(zb_callback2_t func, zb_uint16_t param,
					zb_uint16_t max_size)
{
	if ((zboss_tid) && (xTaskGetCurrentTaskHandle() == zboss_tid) &&
	    (!zb_osif_is_inside_isr())) {
		return zb_buf_get_out_delayed_ext_func(func, param, max_size);
	}

	zb_app_cb_t new_app_cb = {
		.type = ZB_GET_OUT_BUF_DELAYED_EXT,
		.func2 = func,
		.user_param = param,
		.param = max_size,
	};

	if (xQueueSend(zb_app_cb_msgq, &new_app_cb, 0) != pdTRUE) {
		return RET_OVERFLOW;
	}

	xSemaphoreGive(schedule_semaphore);
	return RET_OK;
}

zb_ret_t zigbee_get_in_buf_delayed_ext(zb_callback2_t func, zb_uint16_t param, zb_uint16_t max_size)
{
	if ((zboss_tid) && (xTaskGetCurrentTaskHandle() == zboss_tid) &&
	    (!zb_osif_is_inside_isr())) {
		return zb_buf_get_in_delayed_ext_func(func, param, max_size);
	}

	zb_app_cb_t new_app_cb = {
		.type = ZB_GET_IN_BUF_DELAYED_EXT,
		.func2 = func,
		.user_param = param,
		.param = max_size,
	};

	if (xQueueSend(zb_app_cb_msgq, &new_app_cb, 0) != pdTRUE) {
		return RET_OVERFLOW;
	}

	xSemaphoreGive(schedule_semaphore);
	return RET_OK;
}

/**@brief SoC general initialization. */
void zb_osif_init(void)
{
	static bool platform_inited;

	if (platform_inited) {
		return;
	}
	platform_inited = true;

#ifdef CONFIG_ZIGBEE_HAVE_SERIAL
	/* Initialise serial trace */
	zb_osif_serial_init();
#endif

	/* Initialise random generator */
	zb_osif_rng_init();

	/* Initialise AES ECB */
	zb_osif_aes_init();

#ifdef ZB_USE_SLEEP
	/* Initialise power consumption routines */
	zb_osif_sleep_init();
#endif /*ZB_USE_SLEEP*/
}

void zb_osif_abort(void)
{
	/* Log ZBOSS error message and flush logs. */
	LOG("ZBOSS fatal error occurred");
}

uint32_t zigbee_pibcache_pan_id_clear(void)
{
	/* For consistency with zb_nwk_nib_init(), the 0xFFFFU is used,
	 * i.e. ZB_BROADCAST_PAN_ID.
	 */
	ZB_PIBCACHE_PAN_ID() = ZB_BROADCAST_PAN_ID;
	return ZB_BROADCAST_PAN_ID;
}

void zb_osif_busy_loop_delay(zb_uint32_t count)
{
	for (volatile int i = 0; i < count; ++i) {
	}
}

zb_uint32_t zb_get_utc_time(void)
{
	LOG("Unable to obtain UTC time. "
	    "Please implement %s in your application to provide the current UTC time.",
	    __func__);
	return ZB_TIME_BEACON_INTERVAL_TO_MSEC(ZB_TIMER_GET()) / 1000;
}

void zb_osif_get_ieee_eui64(zb_ieee_addr_t ieee_eui64)
{
	uint64_t addr;

#if defined(CONFIG_ZIGBEE_UICR_EUI64_ENABLE)
	addr = (uint64_t)EUI64_ADDR[EUI64_ADDR_HIGH] << 32 | EUI64_ADDR[EUI64_ADDR_LOW];
#else
	uint32_t deviceid[2];

	deviceid[0] = nrf_ficr_deviceid_get(NRF_FICR, 0);
	deviceid[1] = nrf_ficr_deviceid_get(NRF_FICR, 1);

	addr = ((uint64_t)deviceid[EUI64_ADDR_HIGH] << 32 | deviceid[EUI64_ADDR_LOW]) &
	       0x000000FFFFFFFFFFULL;
	addr = (addr << 24) | ((CONFIG_ZIGBEE_VENDOR_OUI & 0xFFU) << 16) |
	       (((CONFIG_ZIGBEE_VENDOR_OUI >> 8) & 0xFFU) << 8) | (CONFIG_ZIGBEE_VENDOR_OUI >> 16);
#endif

	memcpy(ieee_eui64, &addr, sizeof(zb_ieee_addr_t));
}

static zigbee_event_t zigbee_event;

void zigbee_event_notify(zigbee_event_t event)
{
	zigbee_event = event;
	if (zb_osif_is_inside_isr()) {
		BaseType_t woken = pdFALSE;

		xSemaphoreGiveFromISR(zigbee_event_semaphore, &woken);
		portYIELD_FROM_ISR(woken);
	} else {
		xSemaphoreGive(zigbee_event_semaphore);
	}
}

uint32_t zigbee_event_poll(uint32_t timeout_us)
{
	unsigned int signaled = 0;
	/* Store timestamp of event polling start. */
	TickType_t timestamp_poll_start = xTaskGetTickCount();

	xSemaphoreTake(zigbee_event_semaphore, portMAX_DELAY);

	return pdTICKS_TO_MS(xTaskGetTickCount() - timestamp_poll_start);
}

void zigbee_enable(void)
{
	xTaskCreate(zboss_thread, "zboss", configMINIMAL_STACK_SIZE + 1024, NULL,
		    CONFIG_ZBOSS_DEFAULT_THREAD_PRIORITY, &zboss_tid);
}

/**
 * @brief Get the reason that triggered the last reset
 *
 * @return @ref reset_source
 * */
zb_uint8_t zb_get_reset_source(void)
{
	uint32_t reas;
	uint8_t zb_reason;
#ifdef CONFIG_ZIGBEE_LIBRARY_NCP_DEV
	static uint8_t zephyr_reset_type = 0xFF;

	/* Read the value at the first API call, then use data from RAM. */
	if (zephyr_reset_type == 0xFF) {
		zephyr_reset_type = nrf_power_gpregret_get(NRF_POWER, 0);
	}
#endif /* CONFIG_ZIGBEE_LIBRARY_NCP_DEV */

#if NRF_POWER_HAS_RESETREAS

	reas = nrf_power_resetreas_get(NRF_POWER);
	nrf_power_resetreas_clear(NRF_POWER, reas);
	if (reas & NRF_POWER_RESETREAS_RESETPIN_MASK) {
		zb_reason = ZB_RESET_SRC_RESET_PIN;
	} else if (reas & NRF_POWER_RESETREAS_SREQ_MASK) {
		zb_reason = ZB_RESET_SRC_SW_RESET;
	} else if (reas) {
		zb_reason = ZB_RESET_SRC_OTHER;
	} else {
		zb_reason = ZB_RESET_SRC_POWER_ON;
	}

#else

	reas = nrf_reset_resetreas_get(NRF_RESET);
	nrf_reset_resetreas_clear(NRF_RESET, reas);
	if (reas & NRF_RESET_RESETREAS_RESETPIN_MASK) {
		zb_reason = ZB_RESET_SRC_RESET_PIN;
	} else if (reas & NRF_RESET_RESETREAS_SREQ_MASK) {
		zb_reason = ZB_RESET_SRC_SW_RESET;
	} else if (reas) {
		zb_reason = ZB_RESET_SRC_OTHER;
	} else {
		zb_reason = ZB_RESET_SRC_POWER_ON;
	}

#endif

#ifdef CONFIG_ZIGBEE_LIBRARY_NCP_DEV
	if ((zb_reason == ZB_RESET_SRC_SW_RESET) && (zephyr_reset_type != SYS_REBOOT_NCP)) {
		zb_reason = ZB_RESET_SRC_OTHER;
	}

	/* The NCP reset type is used only by this API call.
	 * Reset the value inside the register, so after the next, external
	 * SW reset, the value will not trigger NCP logic.
	 */
	if (zephyr_reset_type == SYS_REBOOT_NCP) {
		nrf_power_gpregret_set(NRF_POWER, 0, (uint8_t)SYS_REBOOT_COLD);
	}
#endif /* CONFIG_ZIGBEE_LIBRARY_NCP_DEV */

	return zb_reason;
}

zb_bool_t zigbee_is_nvram_initialised(void)
{
	zb_uint8_t buf[ZB_PAGE_INIT_CHECK_LEN] = {0};
	zb_uint8_t i;
	zb_ret_t ret_code;

	ret_code = zb_osif_nvram_read(0, 0, buf, sizeof(buf));
	if (ret_code != RET_OK) {
		return ZB_FALSE;
	}

	for (i = 0; i < sizeof(buf); i++) {
		if (buf[i] != FLASH_EMPTY_BYTE) {
			return ZB_TRUE;
		}
	}

	return ZB_FALSE;
}

ZB_WEAK_PRE zb_uint32_t ZB_WEAK zb_osif_get_fw_version(void)
{
	return 0x01;
}

ZB_WEAK_PRE zb_uint32_t ZB_WEAK zb_osif_get_ncp_protocol_version(void)
{
#ifdef ZB_NCP_PROTOCOL_VERSION
	return ZB_NCP_PROTOCOL_VERSION;
#else  /* ZB_NCP_PROTOCOL_VERSION */
	return 0x01;
#endif /* ZB_NCP_PROTOCOL_VERSION */
}

ZB_WEAK_PRE zb_ret_t ZB_WEAK zb_osif_bootloader_run_after_reboot(void)
{
	return RET_OK;
}

ZB_WEAK_PRE void ZB_WEAK zb_osif_bootloader_report_successful_loading(void)
{
}
