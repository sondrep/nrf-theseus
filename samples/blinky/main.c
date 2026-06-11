#include <stdio.h>
#include <FreeRTOS.h>
#include <task.h>
#include <nrfx_gpiote.h>
#include <timers.h>


void TaskBlink(void *arg){
  const TickType_t xPeriod = 10;
  TickType_t xLastWakeTime = xTaskGetTickCount();

  nrfx_gpiote_t gpiote = NRFX_GPIOTE_INSTANCE(NRF_GPIOTE20);
  nrfx_gpiote_init(&gpiote, 3);

  nrfx_gpiote_output_config_t pin_config = {
    .drive = NRF_GPIO_PIN_S0S1,
    .input_connect = NRF_GPIO_PIN_INPUT_DISCONNECT,
    .pull = NRF_GPIO_PIN_NOPULL
  };

  nrfx_gpiote_output_configure(&gpiote, NRF_GPIO_PIN_MAP(2, 9), &pin_config, NULL);

  for(;;){
    nrfx_gpiote_out_toggle(&gpiote, NRF_GPIO_PIN_MAP(2, 9));
    xTaskDelayUntil(&xLastWakeTime, xPeriod);
  }

}


void TaskTEST(void *arg){
  nrfx_gpiote_t gpiote = NRFX_GPIOTE_INSTANCE(NRF_GPIOTE20);
  nrfx_gpiote_init(&gpiote, 3);

  nrfx_gpiote_output_config_t pin_config = {
    .drive = NRF_GPIO_PIN_S0S1,
    .input_connect = NRF_GPIO_PIN_INPUT_DISCONNECT,
    .pull = NRF_GPIO_PIN_NOPULL
  };

  nrfx_gpiote_output_configure(&gpiote, NRF_GPIO_PIN_MAP(2, 9), &pin_config, NULL);

  nrfx_gpiote_out_set(&gpiote, NRF_GPIO_PIN_MAP(2, 9));
}

int main(void){
  TaskHandle_t xHandle = NULL;
  BaseType_t ret;

  ret = xTaskCreate(TaskBlink, "blink", 8192, NULL, tskIDLE_PRIORITY, &xHandle);

  vTaskStartScheduler();

  if(ret != pdPASS){
      return -1;
  }
  for(;;);
  return 0;
}
