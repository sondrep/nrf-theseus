/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <FreeRTOS.h>
#include <task.h>
#include <theseus/gpiote.h>
#include <theseus/log.h>
#include <theseus/module.h>
#include <board.h>

/* LED descriptor: name, GPIO port/pin, and blink period in ms */
typedef struct {
	const char *name;
	uint32_t pin;
	uint32_t period_ms;
} led_blink_t;

/* LED table: each entry gets its own independent blink task */
static const led_blink_t leds[] = {
	{.name = "LED1", .pin = BOARD_PIN_LED_0, .period_ms = 30},
	{.name = "LED2", .pin = BOARD_PIN_LED_1, .period_ms = 200},
	{.name = "LED3", .pin = BOARD_PIN_LED_2, .period_ms = 1000},
	{.name = "LED4", .pin = BOARD_PIN_LED_3, .period_ms = 5000},
};

#define NUM_LEDS (sizeof(leds) / sizeof(leds[0]))

/* Interrupt priority for the GPIOTE driver */
#define GPIOTE_IRQ_PRIORITY 3

void TaskBlink(void *arg)
{
	/* Recover this LED's config and build its GPIO pin number */
	const led_blink_t *led = (const led_blink_t *)arg;
	uint32_t pin = led->pin;

	nrfx_gpiote_t *gpiote = theseus_gpiote_get();

	/* Set the pin as an output so we can turn the LED on and off */
	nrfx_gpiote_output_config_t pin_config = {.drive = NRF_GPIO_PIN_S0S1,
						  .input_connect = NRF_GPIO_PIN_INPUT_DISCONNECT,
						  .pull = NRF_GPIO_PIN_NOPULL};
	nrfx_gpiote_output_configure(gpiote, pin, &pin_config, NULL);

	/* Turn the blink time (ms) into ticks, and note the current time */
	const TickType_t xPeriod = pdMS_TO_TICKS(led->period_ms);
	TickType_t xLastWakeTime = xTaskGetTickCount();

	/* Main loop: toggle the LED, then sleep until the next period */
	while (true) {
		nrfx_gpiote_out_toggle(gpiote, pin);
		xTaskDelayUntil(&xLastWakeTime, xPeriod);
	}
}

int main(void)
{
	/* Create one blink task per LED, each with its own settings */
	for (size_t i = 0; i < NUM_LEDS; i++) {
		BaseType_t ret =
			xTaskCreate(TaskBlink,	  /* function the task runs */
				    leds[i].name, /* task name (for debugging) */
				    configMINIMAL_STACK_SIZE +
					    128, /* stack size: blinking LED doesn't need a lot */
				    (void *)&leds[i],	  /* give the task its LED's settings */
				    tskIDLE_PRIORITY + 1, /* task priority (just above idle) */
				    NULL);		  /* we don't need the task handle */

		/* If a task can't be created (usually out of heap), stop here */
		if (ret != pdPASS) {
			return -1;
		}
	}

	/* Hand control to FreeRTOS, the tasks now run on their own */
	vTaskStartScheduler();

	/* We only get here if the scheduler couldn't start (out of heap) */
	while (1)
		;
	return 0;
}
