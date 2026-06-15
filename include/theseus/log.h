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

/* Serializes console output across tasks. Created by console_init(). */
extern SemaphoreHandle_t xPrintMutex;

/**
 * @brief Initialise the UART console.
 *
 * Configures the UARTE peripheral, wires picolibc stdout/stderr to it, and
 * creates the print mutex. Must be called before any printf or LOG().
 */
void theseus_console_init(void);

/**
 * @brief Atomic, task-safe logging.
 *
 * printf emits one byte at a time, so the scheduler can preempt mid-message
 * and interleave output from another task. Holding the mutex across the whole
 * call keeps each message intact.
 */
#define LOG(...) do {                               \
        xSemaphoreTake(xPrintMutex, portMAX_DELAY); \
        printf(__VA_ARGS__);                        \
        fflush(stdout);                             \
        xSemaphoreGive(xPrintMutex);                \
    } while (0)

#endif /* LOG_H */