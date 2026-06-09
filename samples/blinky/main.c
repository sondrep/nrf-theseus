#include <stdio.h>
#include <FreeRTOS.h>
#include <task.h>
#include <nrfx_gpiote.h>
#include <timers.h>


void TaskBlink(void *arg){
  const TickType_t xFrequency = 10;
  TickType_t xLastWakeTime = xTaskGetTickCount();

  nrfx_gpiote_t gpiote = NRFX_GPIOTE_INSTANCE(NRF_GPIOTE20);
  nrfx_gpiote_init(&gpiote, 3);

  nrfx_gpiote_output_config_t pin_config = {
    .drive = NRF_GPIO_PIN_S0S1,
    .input_connect = NRF_GPIO_PIN_INPUT_DISCONNECT,
    .pull = NRF_GPIO_PIN_NOPULL
  };

  uint8_t out_channel_primary;
  nrfx_gpiote_task_config_t task_config =
    {
        .task_ch = out_channel_primary,
        .polarity = NRF_GPIOTE_POLARITY_TOGGLE,
        .init_val = NRF_GPIOTE_INITIAL_VALUE_HIGH,
    };
  nrfx_gpiote_channel_alloc(&gpiote, &out_channel_primary);
  nrfx_gpiote_output_configure(&gpiote, NRF_GPIO_PIN_MAP(2, 9), &pin_config, &task_config);

  for(;;){
    nrfx_gpiote_out_toggle(&gpiote, NRF_GPIO_PIN_MAP(2, 9));
    xTaskDelayUntil(&xLastWakeTime, xFrequency);
  }

}

StaticTimer_t myTimerBuffer;
TaskHandle_t  led_toggle_task_handle;
TimerHandle_t led_toggle_timer_handle;  /**< Reference to LED1 toggling FreeRTOS timer. */

static void led_toggle_timer_callback (xTimerHandle xTimer )
{
}

int main(void){
  TaskHandle_t xHandle = NULL;
  BaseType_t ret;

  ret = xTaskCreate(TaskBlink, "blink", 255, NULL, tskIDLE_PRIORITY, &xHandle);
  led_toggle_timer_handle = xTimerCreateStatic( "LED0", 1000, pdTRUE, NULL, led_toggle_timer_callback,&myTimerBuffer);
  xTimerStart(led_toggle_timer_handle, 0);

  vTaskStartScheduler();

  if(ret != pdPASS){
      return -1;
  }
  for(;;);
  return 0;
}
