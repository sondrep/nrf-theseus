/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Theseus -- Cortex-M33 fault handlers.
 *
 * On a crash, prints the exception stack frame and fault status registers over the console UART before halting,
 * so the cause is visible in the serial log without needing to attach a debugger.
 *
 * Uses printf() directly rather than the LOG() macro,
 * since printf() writes straight to the polled UART driver (see lib/modules/log/log.c)
 * and works even if the task that faulted was holding LOG()'s print mutex.
 */

#include <stdint.h>
#include <stdio.h>
#include <nrf.h>
#include <FreeRTOS.h>
#include <task.h>

/* Registers pushed onto the active stack by the CPU on exception entry. */
struct fault_stack_frame {
	uint32_t r0, r1, r2, r3, r12;
	uint32_t lr, pc, psr;
};

static void fault_report(const char *name, const struct fault_stack_frame *sf)
{
	printf("\n[FATAL] %s\n", name);
	printf("  PC=0x%08lx LR=0x%08lx PSR=0x%08lx\n", (unsigned long)sf->pc,
	       (unsigned long)sf->lr, (unsigned long)sf->psr);
	printf("  R0=0x%08lx R1=0x%08lx R2=0x%08lx R3=0x%08lx R12=0x%08lx\n",
	       (unsigned long)sf->r0, (unsigned long)sf->r1, (unsigned long)sf->r2,
	       (unsigned long)sf->r3, (unsigned long)sf->r12);
	printf("  CFSR=0x%08lx HFSR=0x%08lx MMFAR=0x%08lx BFAR=0x%08lx\n",
	       (unsigned long)SCB->CFSR, (unsigned long)SCB->HFSR, (unsigned long)SCB->MMFAR,
	       (unsigned long)SCB->BFAR);
}

/* MMFAR/BFAR only hold a meaningful address when the corresponding fault actually set it,
 * hence the SCB_CFSR_*VALID_Msk checks below. */

void theseus_hard_fault_c(struct fault_stack_frame *sf)
{
	fault_report("HardFault", sf);
	taskDISABLE_INTERRUPTS();
	for (;;) {
	}
}

void theseus_mem_fault_c(struct fault_stack_frame *sf)
{
	fault_report("MemoryManagement fault", sf);
	if (SCB->CFSR & SCB_CFSR_MMARVALID_Msk) {
		printf("  Faulting address (MMFAR): 0x%08lx\n", (unsigned long)SCB->MMFAR);
	}
	taskDISABLE_INTERRUPTS();
	for (;;) {
	}
}

void theseus_bus_fault_c(struct fault_stack_frame *sf)
{
	fault_report("BusFault", sf);
	if (SCB->CFSR & SCB_CFSR_BFARVALID_Msk) {
		printf("  Faulting address (BFAR): 0x%08lx\n", (unsigned long)SCB->BFAR);
	}
	taskDISABLE_INTERRUPTS();
	for (;;) {
	}
}

void theseus_usage_fault_c(struct fault_stack_frame *sf)
{
	fault_report("UsageFault", sf);
	taskDISABLE_INTERRUPTS();
	for (;;) {
	}
}

/*
 * Naked trampoline: 
 * figures out which stack (MSP or PSP) was active when the fault occurred 
 * (EXC_RETURN bit 2, tested via LR)
 * and passes that pointer to the C handler as its argument (r0),
 * per the AAPCS. Naked + pure asm because a normal C prologue would touch the stack before we've read the right pointer off it.
 */
#define THESEUS_FAULT_TRAMPOLINE(handler_name, c_name)                                   \
	void handler_name(void) __attribute__((naked));                                      \
	void handler_name(void)                                                              \
	{                                                                                    \
		__asm volatile("tst lr, #4      \n"                                              \
					   "ite eq          \n"                                              \
					   "mrseq r0, msp   \n"                                              \
					   "mrsne r0, psp   \n"                                              \
					   "b " #c_name "   \n");                                            \
	}

THESEUS_FAULT_TRAMPOLINE(HardFault_Handler, theseus_hard_fault_c)
THESEUS_FAULT_TRAMPOLINE(MemoryManagement_Handler, theseus_mem_fault_c)
THESEUS_FAULT_TRAMPOLINE(BusFault_Handler, theseus_bus_fault_c)
THESEUS_FAULT_TRAMPOLINE(UsageFault_Handler, theseus_usage_fault_c)
