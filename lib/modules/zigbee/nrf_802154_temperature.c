#include <stdint.h>
#include <nrfx_temp.h>

void temp_handler(int32_t temperature)
{
	nrf_802154_temperature_changed();
}

/**
 * @brief Initializes the thermometer.
 */
void nrf_802154_temperature_init(void)
{
	nrfx_temp_config_t temp_config = NRFX_TEMP_DEFAULT_CONFIG;
	nrfx_temp_init(&temp_config, temp_handler);
}

/**
 * @brief Deinitializes the thermometer.
 */
void nrf_802154_temperature_deinit(void)
{
	nrfx_temp_uninit();
}

/**
 * @brief Gets the current temperature.
 *
 * @returns Current temperature, in centigrades (C).
 */
int8_t nrf_802154_temperature_get(void)
{
	int ret = nrfx_temp_measure();
	if (ret != 0) {
		return 0;
	}
	return nrfx_temp_calculate(nrfx_temp_result_get());
}
