#include <nrf_modem.h>
#include <nrf_modem_os.h>
#include <nrf_errno.h>
#include <nrf.h>
#include "errno.h"
#include <FreeRTOS.h>
#include <nrfx_rtc.h>
#include <stdlib.h>
#include <portmacro.h>
#include <semphr.h>
#include <event_groups.h>
#include <errno.h>
#include <theseus/module.h>
#include <stdarg.h>
#include <stdio.h>
#include <theseus/log.h>

#define MODEM_OS_EVENT_BIT (1 << 0)
#define WAIT_FOR_ANY_BIT   0xFFFFFF

static EventGroupHandle_t modem_event_group;

void nrf_modem_os_init(void)
{
	modem_event_group = xEventGroupCreate();
}

void nrf_modem_os_shutdown(void)
{
	/* Deinitialize the glue layer.
	   When shutdown is called, all pending calls to nrf_modem_os_timedwait
	   shall exit and return -NRF_ESHUTDOWN. */
	vEventGroupDelete(modem_event_group);
}

void *nrf_modem_os_alloc(size_t bytes)
{
	/* Allocate a buffer on the library heap. */
	void *mem = malloc(bytes);

	if (mem == NULL) {
		LOG("nrf_modem_os_alloc(%u) failed: heap exhausted\n", (unsigned int)bytes);
	}

	return mem;
}

void nrf_modem_os_free(void *mem)
{
	/* Free a memory buffer in the library heap. */
	free(mem);
}

void nrf_modem_os_busywait(int32_t usec)
{
	NRFX_DELAY_US(usec);
}

int32_t nrf_modem_os_timedwait(uint32_t context, int32_t *timeout)
{
	if (!nrf_modem_is_initialized()) {
		return -NRF_ESHUTDOWN;
	}

	/* Put a thread to sleep for a specific time or until an event occurs.
	   Wait for the timeout.
	   All waiting threads shall be woken by nrf_modem_event_notify.
	   A blind return value of zero will cause a blocking wait. */
	const TickType_t current_ticks = xTaskGetTickCount();
	EventBits_t ret;
	TickType_t wait_ticks;

	switch (*timeout) {
	case NRF_MODEM_OS_FOREVER:
		wait_ticks = portMAX_DELAY;
		break;
	case NRF_MODEM_OS_NO_WAIT:
		wait_ticks = 0;
		break;
	default:
		wait_ticks = pdMS_TO_TICKS(*timeout);
		break;
	}
	if (context != 0) {
		ret = xEventGroupWaitBits(modem_event_group, 1 | (1 << context), pdTRUE, pdFALSE,
					  wait_ticks);
	} else {
		ret = xEventGroupWaitBits(modem_event_group, WAIT_FOR_ANY_BIT, pdTRUE, pdFALSE,
					  wait_ticks);
	}
	if (*timeout == NRF_MODEM_OS_FOREVER) {
		return 0;
	}
	const TickType_t after_ticks = xTaskGetTickCount();

	*timeout -= pdTICKS_TO_MS(after_ticks - current_ticks);

	if (*timeout <= 0) {
		LOG("TIMEOUT. EventBits_t: 0x%x\n", ret);
		return -NRF_EAGAIN;
	}

	if (!nrf_modem_is_initialized()) {
		return -NRF_ESHUTDOWN;
	}

	return 0;
}

void nrf_modem_os_event_notify(uint32_t context)
{
	/* Notify the application that an event has occurred.
	   This shall wake all threads sleeping in nrf_modem_os_timedwait. */
	if (context > 23) {
		LOG("We are screwed mate. This is highly illegal. It is nearing a complete "
		    "meltdown, we really need to get our shit together.\n");
	}
	BaseType_t woken;
	if (nrf_modem_os_is_in_isr()) {
		xEventGroupSetBitsFromISR(modem_event_group, 1 << context, &woken);
		portYIELD_FROM_ISR(woken);
	} else {
		xEventGroupSetBits(modem_event_group, 1 << context);
	}
}

int nrf_modem_os_sleep(uint32_t timeout)
{
	vTaskDelay(pdMS_TO_TICKS(timeout));
	return 0;
}

void nrf_modem_os_errno_set(int errno_val)
{
	/* Set OS errno. */
	errno = errno_val;
}

bool nrf_modem_os_is_in_isr(void)
{
	/* Check if executing in interrupt context. */
	return (__get_IPSR() != 0);
}

int nrf_modem_os_sem_init(void **sem, unsigned int initial_count, unsigned int limit)
{
	/* If multithreaded access to modem functionalities is needed, the function must allocate
	 * and initialize a semaphore and return its address through the `sem` parameter. If the
	 * address of an already allocated semaphore is provided as an input, the allocation part is
	 * skipped and the semaphore is only reinitialized.
	 */
	SemaphoreHandle_t handle;
	if (limit == 1) {
		handle = xSemaphoreCreateBinary();
		if (initial_count == 1) {
			if (handle == NULL) {
				return -NRF_ENOMEM;
			}
			xSemaphoreGive(handle);
		}
	} else {
		handle = xSemaphoreCreateCounting(limit, initial_count);
	}
	if (handle == NULL) {
		return -NRF_ENOMEM;
	}
	*sem = handle;

	return 0;
}

void nrf_modem_os_sem_give(void *sem)
{
	/* Give a semaphore. */
	(void)xSemaphoreGive(sem);
}

int nrf_modem_os_sem_take(void *sem, int timeout)
{
	/* Try to take a semaphore with the given timeout. */
	if (xSemaphoreTake(sem, pdMS_TO_TICKS(timeout)) == pdFAIL) {
		return -NRF_EAGAIN;
	}
	return 0;
}

unsigned int nrf_modem_os_sem_count_get(void *sem)
{
	/* Get a semaphore's count. */
	return uxSemaphoreGetCount(sem);
}

int nrf_modem_os_mutex_init(void **mutex)
{
	/* If multithreaded access to modem functionalities is needed, the function must allocate
	 * and initialize a reentrant mutex and return its address through the `mutex` parameter.
	 * If the address of an already allocated mutex is provided as an input, the allocation part
	 * is skipped and the mutex is only reinitialized.
	 * Mutexes are not required if multithreaded access to modem functionalities is not needed.
	 * In this case, the function must blindly return ``0``.
	 */
	SemaphoreHandle_t handle = xSemaphoreCreateMutex();
	if (handle == NULL) {
		return -NRF_ENOMEM;
	}
	*mutex = handle;

	return 0;
}

int nrf_modem_os_mutex_unlock(void *sem)
{
	/* Unlock a mutex. */
	if (xSemaphoreGive(sem) == pdFAIL) {
		return -NRF_EPERM;
	}
	return 0;
}

int nrf_modem_os_mutex_lock(void *sem, int timeout)
{
	if (xSemaphoreTake(sem, pdMS_TO_TICKS(timeout)) == pdFAIL) {
		return -NRF_EAGAIN;
	}
	return 0;
}

void nrf_modem_os_log(int level, const char *fmt, ...)
{
	/* Generic logging procedure. */
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	printf("\n");
	va_end(ap);
}

void nrf_modem_os_logdump(int level, const char *str, const void *data, size_t len)
{
	/* Log hex representation of object. */
	printf("[MODEM HEXDUMP]: %s ", str);
	for (size_t i = 0; i < len; i++) {
		printf("0x%x ", ((uint8_t *)(data))[i]);
	}
	printf("\n");
}
