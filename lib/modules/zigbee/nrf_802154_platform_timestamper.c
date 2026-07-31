
/**
 * @brief Abstraction layer for peripheral-to-peripheral hardware connections needed for
 * timestamping.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <nrf.h>
#include <nrf_dppi.h>
#include <nrf_grtc.h>
#include <nrfx_gppi.h>
#include <nrfx_gppi_d2ppi.h>
#include <nrfx_grtc.h>
#include <nrfy_grtc.h>

#define COUNTER_SPAN                                                                               \
	(GRTC_SYSCOUNTER_SYSCOUNTERL_VALUE_Msk |                                                   \
	 ((uint64_t)GRTC_SYSCOUNTER_SYSCOUNTERH_VALUE_Msk << 32))

static nrfx_gppi_handle_t rad_peri_handle;
static uint32_t ppib_chan;
static uint8_t m_timestamp_cc_channel;

/**
 * @brief Initializes the timestamper platform.
 */
void nrf_802154_platform_timestamper_init(void)
{
	int err_code;

	err_code = nrfx_grtc_channel_alloc(&m_timestamp_cc_channel);
}

/**
 * @brief Sets up cross-domain hardware connections necessary to capture a timestamp.
 *
 * This function configures cross-domain hardware connections necessary to capture a timestamp of
 * an event from the local domain. These connections are identical for all local domain events.
 *
 * @note Every call to this function must be paired with a call to @ref
 * nrf_802154_platform_timestamper_cross_domain_connections_clear.
 */
void nrf_802154_platform_timestamper_cross_domain_connections_setup(void)
{
	nrfx_gppi_resource_t resource = {.domain_id = NRFX_GPPI_DOMAIN_RAD, .channel = 0};
	nrf_grtc_task_t capture_task =
		nrfy_grtc_sys_counter_capture_task_get(m_timestamp_cc_channel);
	uint32_t tep = nrfy_grtc_task_address_get(NRF_GRTC, capture_task);
	int err;

	err = nrfx_gppi_ext_conn_alloc(NRFX_GPPI_DOMAIN_RAD, NRFX_GPPI_DOMAIN_PERI,
				       &rad_peri_handle, &resource);
	assert(err == 0);

	/* Add task endpoint (GRTC capture) to the previously configured connection. */
	err = nrfx_gppi_ep_attach(tep, rad_peri_handle);
	assert(err == 0);

	/* Get PPIB channel used in the connection. */
	ppib_chan = nrfx_gppi_domain_channel_get(rad_peri_handle, NRFX_GPPI_NODE_PPIB11_21);
	assert(ppib_chan >= 0);

	/* Disable PPIB for now until exact channel from RADIO domain is known. */
	nrf_ppib_subscribe_clear(NRF_PPIB11, nrf_ppib_send_task_get(ppib_chan));

	nrfx_gppi_conn_enable(rad_peri_handle);
}

/**
 * @brief Clears cross-domain hardware connections necessary to capture a timestamp.
 */
void nrf_802154_platform_timestamper_cross_domain_connections_clear(void)
{
	nrf_grtc_task_t capture_task =
		nrfy_grtc_sys_counter_capture_task_get(m_timestamp_cc_channel);

	NRF_DPPI_ENDPOINT_CLEAR(nrfy_grtc_task_address_get(NRF_GRTC, capture_task));
}

/**
 * @brief Sets up local domain hardware connections necessary to capture a timestamp.
 *
 * This function configures local domain hardware connections necessary to capture a timestamp of
 * an event from the local domain. These connections must be setup separately for every local domain
 * event.
 *
 * @param[in]  dppi_ch   Local domain DPPI channel that the event to be timestamped publishes to.
 */
void nrf_802154_platform_timestamper_local_domain_connections_setup(uint32_t dppi_ch)
{
	nrfx_grtc_channel_t user_channel_data = {
		.handler = NULL,
		.p_context = NULL,
		.channel = m_timestamp_cc_channel,
	};

	/* Set the CC value to mark channel as not triggered and also to enable it
	 * (makes CCEN=1). COUNTER_SPAN is used so as not to fire an event unnecessarily
	 * - it can be assumed that such a large value will never be reached.
	 */
	nrfx_grtc_syscounter_cc_absolute_set(&user_channel_data, COUNTER_SPAN, false);
	/* Configure PPIB to forward provided DPPI channel from Radio domain and enable
	 * the connection.
	 */
	nrf_ppib_subscribe_set(NRF_PPIB11, nrf_ppib_send_task_get(ppib_chan), dppi_ch);
}

/**
 * @brief Clears local domain hardware connections necessary to capture a timestamp.
 *
 * @param[in]  dppi_ch   Local domain DPPI channel that the event to be timestamped publishes to.
 */
void nrf_802154_platform_timestamper_local_domain_connections_clear(uint32_t dppi_ch)
{
	/* Intentionally empty. */
}

/**
 * @brief Reads timestamp captured using the configured hardware connections.
 *
 * @param[out]  p_timestamp   Captured timestamp. Only valid if @c true is returned, undefined
 * otherwise.
 *
 * @retval  true   The timestamp was captured and read successfully.
 * @retval  false  The timestamp could not be retrieved.
 */
bool nrf_802154_platform_timestamper_captured_timestamp_read(uint64_t *p_captured)
{
	/* @todo: check if this can be replaced with:
	 *
	 * z_nrf_grtc_timer_capture_read(m_timestamp_cc_channel, p_captured);
	 */
	if (nrf_grtc_sys_counter_cc_enable_check(NRF_GRTC, m_timestamp_cc_channel)) {
		return false;
	}

	*p_captured = nrfy_grtc_sys_counter_cc_get(NRF_GRTC, m_timestamp_cc_channel);
	return true;
}
