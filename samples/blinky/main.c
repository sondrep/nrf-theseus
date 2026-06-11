#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <FreeRTOS.h>
#include <task.h>
#include <nrfx_gpiote.h>

/* nRF54L15 DK LED pin mapping
 *   LED1 = P2.09
 *   LED2 = P1.10
 *   LED3 = P2.07
 *   LED4 = P1.14
 */

 /* LED descriptor: name, GPIO port/pin, and blink period in ms */
typedef struct {
    const char *name;
    uint8_t     port;
    uint8_t     pin;
    uint32_t    period_ms;
} led_blink_t;

/* LED table: each entry gets its own independent blink task */
static const led_blink_t leds[] = {
    {
        .name = "LED1",
        .port = 2,
        .pin = 9,
        .period_ms = 30
    },
    {
        .name = "LED2",
        .port = 1,
        .pin = 10,
        .period_ms = 200
    },
    {
        .name = "LED3",
        .port = 2,
        .pin = 7,
        .period_ms = 1000
    },
    {
        .name = "LED4",
        .port = 1,
        .pin = 14,
        .period_ms = 5000
    },
};

#define NUM_LEDS (sizeof(leds)/sizeof(leds[0]))

/* Interrupt priority for the GPIOTE driver */
#define GPIOTE_IRQ_PRIORITY 3

/* Shared GPIOTE instance used to drive all LED output pins */
static nrfx_gpiote_t gpiote = NRFX_GPIOTE_INSTANCE(NRF_GPIOTE20);

void TaskBlink(void *arg)
{
    /* Recover this LED's config and build its GPIO pin number */
    const led_blink_t *led = (const led_blink_t *)arg;
    uint32_t pin = NRF_GPIO_PIN_MAP(led->port, led->pin);

    /* Set the pin as an output so we can turn the LED on and off */
    nrfx_gpiote_output_config_t pin_config = {
        .drive         = NRF_GPIO_PIN_S0S1,
        .input_connect = NRF_GPIO_PIN_INPUT_DISCONNECT,
        .pull          = NRF_GPIO_PIN_NOPULL
    };
    nrfx_gpiote_output_configure(&gpiote, pin, &pin_config, NULL);

    /* Turn the blink time (ms) into ticks, and note the current time */
    const TickType_t xPeriod = pdMS_TO_TICKS(led->period_ms);
    TickType_t xLastWakeTime = xTaskGetTickCount();

    /* Main loop: toggle the LED, then sleep until the next period */
    while (true) {
        nrfx_gpiote_out_toggle(&gpiote, pin);
        xTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

int main(void)
{
    /* Set up the GPIOTE hardware once before any task uses it */
    nrfx_gpiote_init(&gpiote, GPIOTE_IRQ_PRIORITY);

    /* Create one blink task per LED, each with its own settings */
    for (size_t i = 0; i < NUM_LEDS; i++) {
        BaseType_t ret = xTaskCreate(
            TaskBlink,                        /* function the task runs */
            leds[i].name,                     /* task name (for debugging) */
            configMINIMAL_STACK_SIZE + 128,   /* stack size: blinking LED doesn't need a lot */
            (void *)&leds[i],                 /* give the task its LED's settings */
            tskIDLE_PRIORITY + 1,             /* task priority (just above idle) */
            NULL);                            /* we don't need the task handle */

        /* If a task can't be created (usually out of heap), stop here */
        if (ret != pdPASS) {
            return -1;
        }
    }

    /* Hand control to FreeRTOS, the tasks now run on their own */
    vTaskStartScheduler();

    /* We only get here if the scheduler couldn't start (out of heap) */
    while (1);
    return 0;
}