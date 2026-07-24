/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Theseus -- Console abstraction over UARTE.
 */

#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include <nrf.h>
#include <task.h>

/* Serializes console output across tasks. Created by console_init(). */
extern SemaphoreHandle_t xPrintMutex;

/**
 * @brief Atomic, task-safe logging.
 *
 * printf emits one byte at a time, so the scheduler can preempt mid-message
 * and interleave output from another task. Holding the mutex across the whole
 * call keeps each message intact.
 */
#define LOG(...)                                                                                   \
	do {                                                                                       \
		if (__get_IPSR() == 0U) {                                                          \
			xSemaphoreTake(xPrintMutex, portMAX_DELAY);                                \
			printf(__VA_ARGS__);                                                       \
			fflush(stdout);                                                            \
			xSemaphoreGive(xPrintMutex);                                               \
		} else {                                                                           \
			if (xSemaphoreTakeFromISR(xPrintMutex, NULL)) {                            \
				printf(__VA_ARGS__);                                               \
				fflush(stdout);                                                    \
				xSemaphoreGiveFromISR(xPrintMutex, NULL);                          \
			}                                                                          \
		}                                                                                  \
	} while (0)
#endif /* LOG_H */
