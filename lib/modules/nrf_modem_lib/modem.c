#include <nrf_modem.h>
#include <theseus/module.h>
#include <theseus/log.h>
#include <stdint.h>

extern const void *shmem_ctrl_addr, shmem_ctrl_size, shmem_rx_addr, shmem_rx_size, shmem_tx_addr,
	shmem_tx_size, shmem_trace_addr, shmem_trace_size;

#define NRF_MODEM_NETWORK_IRQ_PRIORITY 0

static void nrf_modem_fault_handler(struct nrf_modem_fault_info *fault_info)
{
	LOG("Modem fault: err = %lu", fault_info->reason);
}

static void nrf_modem_lib_dfu_handler(uint32_t dfu_result)
{
	LOG("DFU result: err = %lu", dfu_result);
}

static const struct nrf_modem_init_params init_params = {
	.ipc_irq_prio = NRF_MODEM_NETWORK_IRQ_PRIORITY,
	.shmem.ctrl =
		{
			.base = ((uint32_t)(&shmem_ctrl_addr)),
			.size = ((uint32_t)(&shmem_ctrl_size)),
		},
	.shmem.tx =
		{
			.base = ((uint32_t)(&shmem_tx_addr)),
			.size = ((uint32_t)(&shmem_tx_size)),
		},
	.shmem.rx =
		{
			.base = ((uint32_t)(&shmem_rx_addr)),
			.size = ((uint32_t)(&shmem_rx_size)),
		},
	.shmem.trace =
		{
			.base = ((uint32_t)(&shmem_trace_addr)),
			.size = ((uint32_t)(&shmem_trace_size)),
		},
	.fault_handler = nrf_modem_fault_handler,
	.dfu_handler = nrf_modem_lib_dfu_handler};

int modem_init(void)
{
	// struct nrf_modem_init_params *init_params_ptr = &init_params;
	int err = nrf_modem_init(&init_params);
	LOG("modem initialized, %d", err);
	return err;
}

// THESEUS_MODULE_SET(modem) = {.init = modem_init, .stage = THESEUS_MODULE_STAGE_INTERMEDIARY};
