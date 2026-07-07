#include <nrf_spu.h>
#include <core_cm33.h>

#define AIRCR_VECT_KEY_PERMIT_WRITE 0x05FAUL

static void tz_nonsecure_exception_prio_config(int secure_boost)
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

void theseus_spu_init(void)
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

	uint32_t ret;
	ret = NVIC_SetTargetState(RTC1_IRQn);
	if (ret == 0) {
		NVIC_SetTargetState(RTC1_IRQn);
	}
	ret = NVIC_SetTargetState(IPC_IRQn);
	ret = NVIC_SetTargetState(IPC_IRQn);

	NVIC_SetPriority(SecureFault_IRQn, 0);
	SCB->SHCSR |= SCB_SHCSR_SECUREFAULTENA_Msk;
}

void SecureFault_Handler(void)
{
}

void SPU_IRQHandler(void)
{
}
