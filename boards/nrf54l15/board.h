#ifndef BOARD_NRF54L15DK_H
#define BOARD_NRF54L15DK_H

#include <hal/nrf_gpio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- LEDs (active high) ----------------------------------------------- */

#ifndef BOARD_PIN_LED_0
#define BOARD_PIN_LED_0 NRF_GPIO_PIN_MAP(2, 9)
#endif
#ifndef BOARD_PIN_LED_1
#define BOARD_PIN_LED_1 NRF_GPIO_PIN_MAP(1, 10)
#endif
#ifndef BOARD_PIN_LED_2
#define BOARD_PIN_LED_2 NRF_GPIO_PIN_MAP(2, 7)
#endif
#ifndef BOARD_PIN_LED_3
#define BOARD_PIN_LED_3 NRF_GPIO_PIN_MAP(1, 14)
#endif

#ifndef BOARD_LED_ACTIVE_HIGH
#define BOARD_LED_ACTIVE_HIGH 1
#endif

/* ---- Buttons (active low) --------------------------------------------- */

#ifndef BOARD_PIN_BTN_0
#define BOARD_PIN_BTN_0 NRF_GPIO_PIN_MAP(1, 13)
#endif
#ifndef BOARD_PIN_BTN_1
#define BOARD_PIN_BTN_1 NRF_GPIO_PIN_MAP(1, 9)
#endif
#ifndef BOARD_PIN_BTN_2
#define BOARD_PIN_BTN_2 NRF_GPIO_PIN_MAP(1, 8)
#endif
#ifndef BOARD_PIN_BTN_3
#define BOARD_PIN_BTN_3 NRF_GPIO_PIN_MAP(0, 4)
#endif

/* ---- Console UART (VCOM0 on DK) -------------------------------------- */

#ifndef BOARD_CONSOLE_UARTE_INST
#define BOARD_CONSOLE_UARTE_INST NRF_UARTE20
#endif
#ifndef BOARD_CONSOLE_TX_PIN
#define BOARD_CONSOLE_TX_PIN NRF_GPIO_PIN_MAP(1, 4)
#endif
#ifndef BOARD_CONSOLE_RX_PIN
#define BOARD_CONSOLE_RX_PIN NRF_GPIO_PIN_MAP(1, 5)
#endif
#ifndef BOARD_CONSOLE_CTS_PIN
#define BOARD_CONSOLE_CTS_PIN NRF_GPIO_PIN_MAP(1, 7)
#endif

/* ---- Application UART (VCOM2 on DK) ---------------------------------- */

#ifndef BOARD_APP_UARTE_INST
#define BOARD_APP_UARTE_INST NRF_UARTE30
#endif
#ifndef BOARD_APP_UARTE_TX_PIN
#define BOARD_APP_UARTE_TX_PIN NRF_GPIO_PIN_MAP(0, 0)
#endif
#ifndef BOARD_APP_UARTE_RX_PIN
#define BOARD_APP_UARTE_RX_PIN NRF_GPIO_PIN_MAP(0, 1)
#endif
#ifndef BOARD_APP_UARTE_RTS_PIN
#define BOARD_APP_UARTE_RTS_PIN NRF_GPIO_PIN_MAP(0, 2)
#endif
#ifndef BOARD_APP_UARTE_CTS_PIN
#define BOARD_APP_UARTE_CTS_PIN NRF_GPIO_PIN_MAP(0, 3)
#endif

#ifdef __cplusplus
}
#endif

#endif /* BOARD_NRF54L15DK_H */
