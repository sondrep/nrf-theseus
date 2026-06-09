#include <nrfx_gpiote.h>



int main(void){
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

  nrfx_gpiote_out_set(&gpiote, NRF_GPIO_PIN_MAP(2, 9));
  for(;;);
  return 0;
}
