#include <nrf_spu.h>
#include <theseus/module.h>

static int spu_init(void)
{
	nrf_spu_int_enable(NRF_SPU, NRF_SPU_INT_PERIPHACCERR_MASK | NRF_SPU_INT_RAMACCERR_MASK);
	nrf_spu_peripheral_set(NRF_SPU, NRFX_PERIPHERAL_ID_GET(NRF_POWER_NS_BASE), false, false,
			       false);
	nrf_spu_peripheral_set(NRF_SPU, NRFX_PERIPHERAL_ID_GET(NRF_IPC_NS_BASE), false, false,
			       false);
	nrf_spu_peripheral_set(NRF_SPU, NRFX_PERIPHERAL_ID_GET(NRF_P0_NS_BASE), false, false,
			       false);
	nrf_spu_peripheral_set(NRF_SPU, NRFX_PERIPHERAL_ID_GET(NRF_GPIOTE0_S_BASE), false, false,
			       false);
	nrf_spu_peripheral_set(NRF_SPU, NRFX_PERIPHERAL_ID_GET(NRF_RTC0_NS_BASE), false, false,
			       false);
	nrf_spu_peripheral_set(NRF_SPU, NRFX_PERIPHERAL_ID_GET(NRF_RTC1_NS_BASE), false, false,
			       false);
	nrf_spu_peripheral_set(NRF_SPU, NRFX_PERIPHERAL_ID_GET(NRF_GPIOTE1_NS_BASE), false, false,
			       false);
	nrf_spu_peripheral_set(NRF_SPU, NRFX_PERIPHERAL_ID_GET(NRF_UARTE0_S_BASE), false, false,
			       false);
	nrf_spu_peripheral_set(NRF_SPU, NRFX_PERIPHERAL_ID_GET(NRF_UARTE1_S_BASE), false, false,
			       false);
	nrf_spu_peripheral_set(NRF_SPU, NRFX_PERIPHERAL_ID_GET(NRF_UARTE2_S_BASE), false, false,
			       false);
	nrf_spu_peripheral_set(NRF_SPU, NRFX_PERIPHERAL_ID_GET(NRF_UARTE3_S_BASE), false, false,
			       false);
	// nrf_spu_peripheral_set(NRF_SPU, NRFX_PERIPHERAL_ID_GET(NRF_IPC_S_BASE), false, false,
	// true);
	//  nrf_spu_peripheral_set(NRF_SPU, NRFX_PERIPHERAL_ID_GET(NRF_FPU_NS_BASE), false, false,
	//		       false);

	/* Set RAM region config */
	for (uint8_t i = 0; i < 28; ++i) {
		nrf_spu_ramregion_set(NRF_SPU, i, false,
				      NRF_SPU_MEM_PERM_EXECUTE | NRF_SPU_MEM_PERM_WRITE |
					      NRF_SPU_MEM_PERM_READ,
				      false);
	}

	nrf_spu_flashregion_set(NRF_SPU, 0, true, NRF_SPU_MEM_PERM_EXECUTE | NRF_SPU_MEM_PERM_READ,
				false);
	for (uint8_t i = 1; i < 32; ++i) {
		nrf_spu_flashregion_set(NRF_SPU, i, false,
					NRF_SPU_MEM_PERM_EXECUTE | NRF_SPU_MEM_PERM_READ, false);
	}

	if (nrf_spu_int_enable_check(NRF_SPU,
				     NRF_SPU_INT_PERIPHACCERR_MASK | NRF_SPU_INT_RAMACCERR_MASK)) {
		// LOG("INTERRUPT SET\n");
	}
	return 0;
}

void SPU_IRQHandler(void)
{
	// LOG("SPU_IRQHandler\n");
}

THESEUS_MODULE_SET(spu) = {.init = spu_init, .stage = THESEUS_MODULE_STAGE_INTERMEDIARY};
