#include <nrf_spu.h>
#include <core_cm33.h>
#include <theseus/module.h>

#define AIRCR_VECT_KEY_PERMIT_WRITE 0x05FAUL

void tz_nonsecure_exception_prio_config(int secure_boost)
{
	uint32_t aircr_payload = SCB->AIRCR & (~(SCB_AIRCR_VECTKEY_Msk));
	if (secure_boost) {
		aircr_payload |= SCB_AIRCR_PRIS_Msk;
	} else {
		aircr_payload &= ~(SCB_AIRCR_PRIS_Msk);
	}
	aircr_payload |= 13 << 0b1;
	SCB->AIRCR =
		((AIRCR_VECT_KEY_PERMIT_WRITE << SCB_AIRCR_VECTKEY_Pos) & SCB_AIRCR_VECTKEY_Msk) |
		aircr_payload;
}

static int spu_init(void)
{
	nrf_spu_int_enable(NRF_SPU, NRF_SPU_INT_PERIPHACCERR_MASK | NRF_SPU_INT_RAMACCERR_MASK |
					    NRF_SPU_INT_FLASHACCERR_MASK);
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

	nrf_spu_gpio_config_set(NRF_SPU, 0, 0, false);
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

	// TZ_NVIC_SetPriority_NS(RTC1_IRQn, (0x07 << (8 - 3)));
	// TZ_NVIC_EnableIRQ_NS(RTC1_IRQn);
	tz_nonsecure_exception_prio_config(0);
	NVIC_SetTargetState(RTC1_IRQn);

	return 0;
}

void SPU_IRQHandler(void)
{
	// LOG("SPU_IRQHandler\n");
}

THESEUS_MODULE_SET(spu) = {.init = spu_init, .stage = THESEUS_MODULE_STAGE_INTERMEDIARY};
