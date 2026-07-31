
/**
 * @brief This module defines the Clock Abstraction Layer for the 802.15.4 driver.
 *
 * Clock Abstraction Layer can be used by other modules to start and stop nRF52840 clocks.
 *
 * It is used by the Radio Scheduler (RSCH) to start the HF clock when entering continuous mode
 * and to stop the HF clock after exiting the continuous mode.
 *
 * It is also used by the standalone Low Power Timer Abstraction Layer implementation
 * to start the LF clock during the initialization.
 *
 */

#include <stdbool.h>
#include <stdint.h>
#include <nrfx_clock.h>
#include <nrfx_clock_xo.h>

#include <platform/nrf_802154_clock.h>

/**
 * @defgroup nrf_802154_clock Clock Abstraction Layer for the 802.15.4 driver
 * @{
 * @ingroup nrf_802154_clock
 * @brief Clock Abstraction Layer interface for the 802.15.4 driver.
 *
 */

static void hfclk_cb(nrfx_clock_xo_event_type_t event)
{
	if (event == NRFX_CLOCK_XO_EVT_HFCLK_STARTED) {
		nrf_802154_clock_hfclk_ready();
		nrf_802154_clock_hfclk_latency_set(1);
	}
}

static void lfclk_cb(nrfx_clock_lfclk_evt_type_t evt)
{
	if (evt == NRFX_CLOCK_LFCLK_EVT_LFCLK_STARTED) {
	}
}

/**
 * @brief Initializes the clock driver.
 */
void nrf_802154_clock_init(void)
{
	(void)nrfx_clock_xo_init(hfclk_cb);
	(void)nrfx_clock_lfclk_init(lfclk_cb);
}

/**
 * @brief Deinitializes the clock driver.
 */
void nrf_802154_clock_deinit(void)
{
	nrfx_clock_xo_uninit();
	nrfx_clock_lfclk_uninit();
}

/**
 * @brief Starts the High Frequency Clock.
 *
 * This function is asynchronous, meant to request the ramp-up of the High Frequency Clock and exit.
 * When the High Frequency Clock is ready, @ref nrf_802154_hfclk_ready() is called.
 *
 */
void nrf_802154_clock_hfclk_start(void)
{
	nrfx_clock_xo_start();
}

/**
 * @brief Stops the High Frequency Clock.
 */
void nrf_802154_clock_hfclk_stop(void)
{
	nrfx_clock_xo_stop();
}

/**
 * @brief Checks if the High Frequency Clock is running.
 *
 * @retval true  High Frequency Clock is running.
 * @retval false High Frequency Clock is not running.
 *
 */
bool nrf_802154_clock_hfclk_is_running(void)
{
	return nrfx_clock_xo_init_check();
}

/**
 * @brief Starts the Low Frequency Clock.
 *
 * This function is asynchronous, meant to request the ramp-up of the Low Frequency Clock and exit.
 * When the Low Frequency Clock is ready, @ref nrf_802154_lfclk_ready() is called.
 *
 */
void nrf_802154_clock_lfclk_start(void)
{
	nrfx_clock_lfclk_start();
}

/**
 * @brief Stops the Low Frequency Clock.
 */
void nrf_802154_clock_lfclk_stop(void)
{
	nrfx_clock_lfclk_stop();
}

/**
 * @brief Checks if the Low Frequency Clock is running.
 *
 * @retval true  Low Frequency Clock is running.
 * @retval false Low Frequency Clock is not running.
 */
bool nrf_802154_clock_lfclk_is_running(void)
{
	return nrfx_clock_lfclk_running_check(NULL);
}
