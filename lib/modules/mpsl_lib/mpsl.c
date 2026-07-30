#include <theseus/module.h>
#include <theseus/log.h>
#include <mpsl.h>
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <assert.h>

static TaskHandle_t mpsl_lp_task_handle;
static SemaphoreHandle_t mpsl_lp_mutex;

void mpsl_low_latency_release_callback(void)
{
}

void mpsl_low_latency_acquire_callback(void)
{
}

void SWI03_IRQHandler(void)
{
	BaseType_t woken = pdFALSE;

	/* Wake the task via its built-in notification slot. */
	vTaskNotifyGiveFromISR(mpsl_lp_task_handle, &woken);
	portYIELD_FROM_ISR(woken);
}

/* Runs the deferred SDC work (which calls sdc_callback_) at task priority. */
static void mpsl_lp_task(void *arg)
{
	(void)arg;
	while (true) {
		/* Sleep until SWI03 pokes us. */
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

		/* Lock so we never overlap another low-priority MPSL caller. */
		xSemaphoreTakeRecursive(mpsl_lp_mutex, portMAX_DELAY);
		mpsl_low_priority_process();
		xSemaphoreGiveRecursive(mpsl_lp_mutex);
	}
}

static void hfclk_started_cb(mpsl_clock_evt_type_t evt_type)
{
	LOG("hfclk_started_cb called: %d\n", evt_type);
}

static void fault_handler_(const char *file, const uint32_t line)
{
	LOG("MPSL fault: %s:%lu\n", file ? file : "?", (unsigned long)line);
	assert(0);
}

static int mpsl_init_(void)
{
	mpsl_lp_mutex = xSemaphoreCreateRecursiveMutex();
	assert(mpsl_lp_mutex != NULL);
	NVIC_SetPriority(RADIO_0_IRQn, MPSL_HIGH_IRQ_PRIORITY);
	NVIC_SetPriority(GRTC_3_IRQn, MPSL_HIGH_IRQ_PRIORITY);
	NVIC_SetPriority(TIMER10_IRQn, MPSL_HIGH_IRQ_PRIORITY);

	NVIC_DisableIRQ(SWI03_IRQn);
	NVIC_ClearPendingIRQ(SWI03_IRQn);
	NVIC_SetPriority(SWI03_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);

	BaseType_t ok = xTaskCreate(
		mpsl_lp_task, "mpsl_Task", configMINIMAL_STACK_SIZE + 1024, NULL,
		tskIDLE_PRIORITY + 4,  /* This is max priority (configMAX_PRIORITIES - 1) */
		&mpsl_lp_task_handle); /* save handle so the ISR can notify it */
	assert(ok = pdPASS);

	int32_t return_value;
	return_value = mpsl_init(NULL, SWI03_IRQn, fault_handler_);
	assert(return_value == 0);

	return_value = mpsl_clock_hfclk_latency_set(MPSL_CLOCK_HF_LATENCY_TYPICAL);
	assert(return_value == 0);

	return_value = mpsl_clock_hfclk_src_request(MPSL_CLOCK_HF_SRC_XO, hfclk_started_cb);
	assert(return_value == 0);

	NVIC_ClearPendingIRQ(RADIO_0_IRQn);
	NVIC_ClearPendingIRQ(GRTC_0_IRQn);
	NVIC_ClearPendingIRQ(TIMER10_IRQn);
	NVIC_EnableIRQ(RADIO_0_IRQn);
	NVIC_EnableIRQ(SWI03_IRQn);
	NVIC_EnableIRQ(TIMER10_IRQn);

	return (int)(return_value);
}

THESEUS_MODULE_SET(mpsl) = {.init = mpsl_init_, .stage = THESEUS_MODULE_STAGE_INTERMEDIARY};
