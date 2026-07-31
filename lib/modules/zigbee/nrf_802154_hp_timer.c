/**
 * @brief Module that defines API of High Precision Timer for the 802.15.4 driver.
 *
 */

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <nrf_802154_sl_periphs.h>
#include <nrfx_timer.h>
/**
 * @defgroup nrf_802154_hp_timer High Precision Timer for the 802.15.4 driver
 * @{
 * @ingroup nrf_802154_hp_timer
 * @brief High Precision Timer for the 802.15.4 driver.
 *
 * The High Precision Timer is used only when the radio is in use. It is not
 * used when the radio is in the sleep mode or out of the RAAL timeslots.
 * This timer is meant to provide at least 1-microsecond precision. It is intended to be used
 * for precise frame timestamps or synchronous radio operations.
 *
 * @note The High Precision Timer is relative. To use it as an absolute timer,
 *       synchronize it with the Low Power Timer using the Timer Coordinator module.
 *
 */

/**@brief Timer instance. */
#define TIMER NRF_802154_HIGH_PRECISION_TIMER_INSTANCE

/**@brief Timer compare channel definitions. */
#define TIMER_CC_CAPTURE      NRF_TIMER_CC_CHANNEL1
#define TIMER_CC_CAPTURE_TASK NRF_TIMER_TASK_CAPTURE1

#define TIMER_CC_SYNC	    NRF_TIMER_CC_CHANNEL2
#define TIMER_CC_SYNC_TASK  NRF_TIMER_TASK_CAPTURE2
#define TIMER_CC_SYNC_EVENT NRF_TIMER_EVENT_COMPARE2
#define TIMER_CC_SYNC_INT   NRF_TIMER_INT_COMPARE2_MASK

#define TIMER_CC_EVT	  NRF_TIMER_CC_CHANNEL3
#define TIMER_CC_EVT_TASK NRF_TIMER_TASK_CAPTURE3
#define TIMER_CC_EVT_INT  NRF_TIMER_INT_COMPARE3_MASK

/**@brief Unexpected value in the sync compare channel. */
static uint32_t m_unexpected_sync;

/**@brief Get current time on the Timer. */
static inline uint32_t timer_time_get(void)
{
	nrf_timer_task_trigger(TIMER, TIMER_CC_CAPTURE_TASK);
	return nrf_timer_cc_get(TIMER, TIMER_CC_CAPTURE);
}

/**
 * @brief Initializes the timer.
 */
void nrf_802154_hp_timer_init(void)
{
	nrf_timer_bit_width_set(TIMER, NRF_TIMER_BIT_WIDTH_32);
	nrf_timer_prescaler_set(TIMER, NRF_TIMER_FREQ_1MHz);
	nrf_timer_mode_set(TIMER, NRF_TIMER_MODE_TIMER);
}

/**
 * @brief Deinitializes the timer.
 */
void nrf_802154_hp_timer_deinit(void)
{
	nrf_timer_task_trigger(TIMER, NRF_TIMER_TASK_STOP);
}

/**
 * @brief Starts the timer.
 *
 * The timer starts counting when this command is called.
 */
void nrf_802154_hp_timer_start(void)
{
	nrf_timer_task_trigger(TIMER, NRF_TIMER_TASK_START);
}

/**
 * @brief Stops the timer.
 *
 * The timer stops counting and enters the low power mode.
 */
void nrf_802154_hp_timer_stop(void)
{
	nrf_timer_task_trigger(TIMER, NRF_TIMER_TASK_STOP);
}

/**
 * @brief Gets the value indicated by the timer right now.
 *
 * @note The returned value is relative to the @ref nrf_802154_hp_timer_start call time. It is not
 *       synchronized with the LP timer.
 *
 * @returns Current timer value in microseconds.
 *
 */
uint32_t nrf_802154_hp_timer_current_time_get(void)
{
	return timer_time_get();
}

/**
 * @brief Gets the task used to synchronize the timer with the LP timer.
 *
 * @returns  Address of the task.
 *
 */
uint32_t nrf_802154_hp_timer_sync_task_get(void)
{
	return nrf_timer_task_address_get(TIMER, TIMER_CC_SYNC_TASK);
}

/**
 * @brief Configures the timer to detect if the synchronization task was triggered.
 */
void nrf_802154_hp_timer_sync_prepare(void)
{
	uint32_t past_time = timer_time_get() - 1;

	m_unexpected_sync = past_time;
	nrf_timer_cc_set(TIMER, TIMER_CC_SYNC, past_time);
}

/**
 * @brief Gets the timestamp of the synchronization event.
 *
 * @param[out]  p_timestamp  Timestamp of the synchronization event.
 *
 * @retval true   Synchronization was performed and @p p_timestamp is valid.
 * @retval false  Synchronization was not performed. @p p_timestamp was not modified.
 *
 */
bool nrf_802154_hp_timer_sync_time_get(uint32_t *p_timestamp)
{
	bool result = false;
	uint32_t sync_time = nrf_timer_cc_get(TIMER, TIMER_CC_SYNC);

	assert(p_timestamp != NULL);

	if (sync_time != m_unexpected_sync) {
		*p_timestamp = sync_time;
		result = true;
	}

	return result;
}

/**
 * @brief Gets the task used to make timestamp of an event.
 *
 * This function is to be used to configure PPI.
 * It configures the timer to detect if the returned task was triggered to return
 * a valid value by @ref nrf_802154_hp_timer_timestamp_get.
 *
 * @returns  Address of the task.
 */
uint32_t nrf_802154_hp_timer_timestamp_task_get(void)
{
	return nrf_timer_task_address_get(TIMER, TIMER_CC_EVT_TASK);
}

/**
 * @brief Gets the timestamp of the last event.
 *
 * @returns Timestamp of the last event that triggered
 *          the @ref nrf_802154_hp_timer_timestamp_task_get task.
 */
uint32_t nrf_802154_hp_timer_timestamp_get(void)
{
	return nrf_timer_cc_get(TIMER, TIMER_CC_EVT);
}
