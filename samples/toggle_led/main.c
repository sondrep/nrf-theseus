#include <nrfx_gpiote.h>



int main(void){
  nrfx_gpiote_t gpiote = NRFX_GPIOTE_INSTANCE(NRF_GPIOTE20);
  nrfx_gpiote_init(&gpiote, 3);

  nrfx_gpiote_output_config_t pin_config = {
    .drive = NRF_GPIO_PIN_S0S1,
    .input_connect = NRF_GPIO_PIN_INPUT_DISCONNECT,
    .pull = NRF_GPIO_PIN_NOPULL
  };

  nrfx_gpiote_output_configure(&gpiote, NRF_GPIO_PIN_MAP(2, 9), &pin_config, NULL);

  nrfx_gpiote_out_set(&gpiote, NRF_GPIO_PIN_MAP(2, 9));
  for(;;);
  return 0;
}
