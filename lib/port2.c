/*
 * FreeRTOS Kernel V10.0.0
 * Copyright (C) 2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software. If you wish to use our Amazon
 * FreeRTOS name, please do so in a fair use way that does not cause confusion.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://www.FreeRTOS.org
 * http://aws.amazon.com/freertos
 *
 * 1 tab == 4 spaces!
 */

/*-----------------------------------------------------------
 * Implementation of functions defined in portable.h for the ARM CM4F port.
 *----------------------------------------------------------*/

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"
#include <nrf.h>
#include <core_cm33.h>
#include <nrfx_config.h>
#include <nrfx_grtc.h>
#include <nrfx_gpiote.h>

/*
 * Start first task is a separate function so it can be tested in isolation.
 */
void vPortStartFirstTask( void ) __attribute__ (( naked ));

/*
 * Exception handlers.
 */
void vPortSVCHandler( void ) __attribute__ (( naked ));
void xPortPendSVHandler( void ) __attribute__ (( naked ));


/*-----------------------------------------------------------*/

__attribute__ ( ( naked ) ) void vPortStartFirstTask( void )
{
    __asm volatile(
#if defined(__SES_ARM)
                    " ldr r0, =_vectors \n"     /* Locate the stack using _vectors table. */
#else
                    " ldr r0, =__isr_vector \n" /* Locate the stack using __isr_vector table. */
#endif
                    " ldr r0, [r0]          \n"
                    " msr msp, r0           \n" /* Set the msp back to the start of the stack. */
                    " cpsie i               \n" /* Globally enable interrupts. */
                    " cpsie f               \n"
                    " dsb                   \n"
                    " isb                   \n"
#ifdef SOFTDEVICE_PRESENT
                    /* Block kernel interrupts only (PendSV) before calling SVC */
                    " mov r0, %0            \n"
                    " msr basepri, r0       \n"
#endif
                    " svc 0                 \n" /* System call to start first task. */
                    "                       \n"
                    " .align 2              \n"
#ifdef SOFTDEVICE_PRESENT
                    ::"i"(configKERNEL_INTERRUPT_PRIORITY  << (8 - configPRIO_BITS))
#endif
                );
}

/*-----------------------------------------------------------*/

__attribute__ ( ( naked ) ) void SVC_Handler( void )
{
    __asm volatile (
                    "   ldr r3, =pxCurrentTCB           \n" /* Restore the context. */
                    "   ldr r1, [r3]                    \n" /* Use pxCurrentTCBConst to get the pxCurrentTCB address. */
                    "   ldr r0, [r1]                    \n" /* The first item in pxCurrentTCB is the task top of stack. */
                    "   ldmia r0!, {r4-r11, r14}        \n" /* Pop the registers that are not automatically saved on exception entry and the critical nesting count. */
                    "   msr psp, r0                     \n" /* Restore the task stack pointer. */
                    "   isb                             \n"
                    "   mov r0, #0                      \n"
                    "   msr basepri, r0                 \n"
                    "   bx r14                          \n"
                    "                                   \n"
                    "   .align 2                        \n"
                );
}

/*-----------------------------------------------------------*/

__attribute__ ( ( naked ) ) void PendSV_Handler( void )
{
    /* This is a naked function. */

    __asm volatile
    (
    "   mrs r0, psp                         \n"
    "   isb                                 \n"
    "                                       \n"
    "   ldr r3, =pxCurrentTCB               \n" /* Get the location of the current TCB. */
    "   ldr r2, [r3]                        \n"
    "                                       \n"
    "   tst r14, #0x10                      \n" /* Is the task using the FPU context?  If so, push high vfp registers. */
    "   it eq                               \n"
    "   vstmdbeq r0!, {s16-s31}             \n"
    "                                       \n"
    "   stmdb r0!, {r4-r11, r14}            \n" /* Save the core registers. */
    "                                       \n"
    "   str r0, [r2]                        \n" /* Save the new top of stack into the first member of the TCB. */
    "                                       \n"
    "   stmdb sp!, {r3}                     \n"
    "   mov r0, %0                          \n"
    "   msr basepri, r0                     \n"
    "   dsb                                 \n"
    "   isb                                 \n"
    "   bl vTaskSwitchContext               \n"
    "   mov r0, #0                          \n"
    "   msr basepri, r0                     \n"
    "   ldmia sp!, {r3}                     \n"
    "                                       \n"
    "   ldr r1, [r3]                        \n" /* The first item in pxCurrentTCB is the task top of stack. */
    "   ldr r0, [r1]                        \n"
    "                                       \n"
    "   ldmia r0!, {r4-r11, r14}            \n" /* Pop the core registers. */
    "                                       \n"
    "   tst r14, #0x10                      \n" /* Is the task using the FPU context?  If so, pop the high vfp registers too. */
    "   it eq                               \n"
    "   vldmiaeq r0!, {s16-s31}             \n"
    "                                       \n"
    "   msr psp, r0                         \n"
    "   isb                                 \n"
    "                                       \n"
    "                                       \n"
    "   bx r14                              \n"
    "                                       \n"
    "   .align 2                            \n"
    ::"i"(configMAX_SYSCALL_INTERRUPT_PRIORITY  << (8 - configPRIO_BITS))
    );
}

/*
 * FreeRTOS Kernel V10.0.0
 * Copyright (C) 2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software. If you wish to use our Amazon
 * FreeRTOS name, please do so in a fair use way that does not cause confusion.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://www.FreeRTOS.org
 * http://aws.amazon.com/freertos
 *
 * 1 tab == 4 spaces!
 */

/*-----------------------------------------------------------
 * Implementation of functions defined in portable.h for the ARM CM4F port.
 *----------------------------------------------------------*/

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"
#ifdef SOFTDEVICE_PRESENT
#include "nrf_soc.h"
#include "app_util.h"
#include "app_util_platform.h"
#endif

#if !(__FPU_USED) && !(__LINT__)
    #error This port can only be used when the project options are configured to enable hardware floating point support.
#endif

#if configMAX_SYSCALL_INTERRUPT_PRIORITY == 0
    #error configMAX_SYSCALL_INTERRUPT_PRIORITY must not be set to 0. See http://www.FreeRTOS.org/RTOS-Cortex-M3-M4.html
#endif

/* Constants used to detect a Cortex-M7 r0p1 core, which should use the ARM_CM7
r0p1 port. */
#define portCORTEX_M33               ( 0x411FD210 )

/* Constants required to check the validity of an interrupt priority. */
#define portFIRST_USER_INTERRUPT_NUMBER     ( 16 )
#define portMAX_8_BIT_VALUE                 ( ( uint8_t ) 0xff )
#define portTOP_BIT_OF_BYTE                 ( ( uint8_t ) 0x80 )

/* Constants required to set up the initial stack. */
#define portINITIAL_XPSR                    (((xPSR_Type){.b.T = 1}).w)
#define portINITIAL_EXEC_RETURN             ( 0xfffffffd )

/* Let the user override the pre-loading of the initial LR with the address of
prvTaskExitError() in case is messes up unwinding of the stack in the
debugger. */
#ifdef configTASK_RETURN_ADDRESS
    #define portTASK_RETURN_ADDRESS configTASK_RETURN_ADDRESS
#else
    #define portTASK_RETURN_ADDRESS prvTaskExitError
#endif

/* Each task maintains its own interrupt status in the critical nesting
variable. */
static UBaseType_t uxCriticalNesting = 0;

/*
 * Setup the timer to generate the tick interrupts.  The implementation in this
 * file is weak to allow application writers to change the timer used to
 * generate the tick interrupt.
 */
extern void vPortSetupTimerInterrupt( void );

/*
 * Exception handlers.
 */
void xPortSysTickHandler( void );

/*
 * Start first task is a separate function so it can be tested in isolation.
 */
extern void vPortStartFirstTask( void );

/*
 * Function to enable the VFP.
 */
static void vPortEnableVFP( void );

/*
 * Used to catch tasks that attempt to return from their implementing function.
 */
static void prvTaskExitError( void );

/*-----------------------------------------------------------*/

/*
 * Used by the portASSERT_IF_INTERRUPT_PRIORITY_INVALID() macro to ensure
 * FreeRTOS API functions are not called from interrupts that have been assigned
 * a priority above configMAX_SYSCALL_INTERRUPT_PRIORITY.
 */
#if ( configASSERT_DEFINED == 1 )
     static uint8_t ucMaxSysCallPriority = 0;
     static uint32_t ulMaxPRIGROUPValue = 0;
#endif /* configASSERT_DEFINED */

/*-----------------------------------------------------------*/

/*
 * See header file for description.
 */
StackType_t *pxPortInitialiseStack( StackType_t *pxTopOfStack, TaskFunction_t pxCode, void *pvParameters )
{
    /* Simulate the stack frame as it would be created by a context switch
    interrupt. */

    /* Offset added to account for the way the MCU uses the stack on entry/exit
    of interrupts, and to ensure alignment. */
    pxTopOfStack--;

    *pxTopOfStack = portINITIAL_XPSR;   /* xPSR */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) pxCode; /* PC */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) portTASK_RETURN_ADDRESS;    /* LR */

    /* Save code space by skipping register initialisation. */
    pxTopOfStack -= 5;  /* R12, R3, R2 and R1. */
    *pxTopOfStack = ( StackType_t ) pvParameters;   /* R0 */

    /* A save method is being used that requires each task to maintain its
    own exec return value. */
    pxTopOfStack--;
    *pxTopOfStack = portINITIAL_EXEC_RETURN;

    pxTopOfStack -= 8;  /* R11, R10, R9, R8, R7, R6, R5 and R4. */

    return pxTopOfStack;
}
/*-----------------------------------------------------------*/

static void prvTaskExitError( void )
{
    /* A function that implements a task must not exit or attempt to return to
    its caller as there is nothing to return to.  If a task wants to exit it
    should instead call vTaskDelete( NULL ).

    Artificially force an assert() to be triggered if configASSERT() is
    defined, then stop here so application writers can catch the error. */
    configASSERT( uxCriticalNesting == ~0UL );
    portDISABLE_INTERRUPTS();
    for ( ;; );
}


/*-----------------------------------------------------------*/

/*
 * See header file for description.
 */
BaseType_t xPortStartScheduler( void )
{
    /* configMAX_SYSCALL_INTERRUPT_PRIORITY must not be set to 0.
    See http://www.FreeRTOS.org/RTOS-Cortex-M3-M4.html */
    configASSERT( configMAX_SYSCALL_INTERRUPT_PRIORITY );

    /* This port is designed for nRF52, this is Cortex-M4 r0p1. */
    configASSERT( SCB->CPUID == portCORTEX_M33);

    #if ( configASSERT_DEFINED == 1 )
    {
        volatile uint32_t ulOriginalPriority;
        volatile uint8_t * const pucFirstUserPriorityRegister = &NVIC->IPR[0];
        volatile uint8_t ucMaxPriorityValue;

        /* Determine the maximum priority from which ISR safe FreeRTOS API
        functions can be called.  ISR safe functions are those that end in
        "FromISR".  FreeRTOS maintains separate thread and ISR API functions to
        ensure interrupt entry is as fast and simple as possible.

        Save the interrupt priority value that is about to be clobbered. */
        ulOriginalPriority = *pucFirstUserPriorityRegister;

        /* Determine the number of priority bits available.  First write to all
        possible bits. */
        *pucFirstUserPriorityRegister = portMAX_8_BIT_VALUE;

        /* Read the value back to see how many bits stuck. */
        ucMaxPriorityValue = *pucFirstUserPriorityRegister;

        /* Use the same mask on the maximum system call priority. */
        ucMaxSysCallPriority = configMAX_SYSCALL_INTERRUPT_PRIORITY & ucMaxPriorityValue;

        /* Calculate the maximum acceptable priority group value for the number
        of bits read back. */
        ulMaxPRIGROUPValue = SCB_AIRCR_PRIGROUP_Msk >> SCB_AIRCR_PRIGROUP_Pos;
        while ( ( ucMaxPriorityValue & portTOP_BIT_OF_BYTE ) == portTOP_BIT_OF_BYTE )
        {
            ulMaxPRIGROUPValue--;
            ucMaxPriorityValue <<= ( uint8_t ) 0x01;
        }

        /* Remove any bits that are more than actually existing. */
        ulMaxPRIGROUPValue &= SCB_AIRCR_PRIGROUP_Msk >> SCB_AIRCR_PRIGROUP_Pos;

        /* Restore the clobbered interrupt priority register to its original
        value. */
        *pucFirstUserPriorityRegister = ulOriginalPriority;
    }
    #endif /* conifgASSERT_DEFINED */

    /* Make PendSV the lowest priority interrupts. */
    NVIC_SetPriority(PendSV_IRQn, configKERNEL_INTERRUPT_PRIORITY);

    /* Start the timer that generates the tick ISR.  Interrupts are disabled
    here already. */
    vPortSetupTimerInterrupt();

    /* Initialise the critical nesting count ready for the first task. */
    uxCriticalNesting = 0;

    /* Ensure the VFP is enabled - it should be anyway. */
    vPortEnableVFP();

    /* Lazy save always. */
    FPU->FPCCR |= FPU_FPCCR_ASPEN_Msk | FPU_FPCCR_LSPEN_Msk;

    /* Finally this port requires SEVONPEND to be active */
    SCB->SCR |= SCB_SCR_SEVONPEND_Msk;

    /* Start the first task. */
    vPortStartFirstTask();

    /* Should never get here as the tasks will now be executing!  Call the task
    exit error function to prevent compiler warnings about a static function
    not being called in the case that the application writer overrides this
    functionality by defining configTASK_RETURN_ADDRESS. */
    prvTaskExitError();

    /* Should not get here! */
    return 0;
}
/*-----------------------------------------------------------*/

void vPortEndScheduler( void )
{
    /* Not implemented in ports where there is nothing to return to.
    Artificially force an assert. */
    configASSERT( uxCriticalNesting == 1000UL );
}
/*-----------------------------------------------------------*/

void vPortEnterCritical( void )
{
    portDISABLE_INTERRUPTS();
    uxCriticalNesting++;

    /* This is not the interrupt safe version of the enter critical function so
    assert() if it is being called from an interrupt context.  Only API
    functions that end in "FromISR" can be used in an interrupt.  Only assert if
    the critical nesting count is 1 to protect against recursive calls if the
    assert function also uses a critical section. */
    if ( uxCriticalNesting == 1 )
    {
        configASSERT( ( SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk ) == 0 );
    }
}
/*-----------------------------------------------------------*/

void vPortExitCritical( void )
{
    configASSERT( uxCriticalNesting );
    uxCriticalNesting--;
    if ( uxCriticalNesting == 0 )
    {
        portENABLE_INTERRUPTS();
    }
}

/*-----------------------------------------------------------*/

/* This is a naked function. */
static void vPortEnableVFP( void )
{
    SCB->CPACR |= 0xf << 20;
}
/*-----------------------------------------------------------*/

#if ( configASSERT_DEFINED == 1 )

    void vPortValidateInterruptPriority( void )
    {
    uint32_t ulCurrentInterrupt;
    uint8_t ucCurrentPriority;
    IPSR_Type ipsr;

        /* Obtain the number of the currently executing interrupt. */
        ipsr.w = __get_IPSR();
        ulCurrentInterrupt = ipsr.b.ISR;

        /* Is the interrupt number a user defined interrupt? */
        if ( ulCurrentInterrupt >= portFIRST_USER_INTERRUPT_NUMBER )
        {
            /* Look up the interrupt's priority. */
            ucCurrentPriority = NVIC->IPR[ ulCurrentInterrupt - portFIRST_USER_INTERRUPT_NUMBER ];

            /* The following assertion will fail if a service routine (ISR) for
            an interrupt that has been assigned a priority above
            configMAX_SYSCALL_INTERRUPT_PRIORITY calls an ISR safe FreeRTOS API
            function.  ISR safe FreeRTOS API functions must *only* be called
            from interrupts that have been assigned a priority at or below
            configMAX_SYSCALL_INTERRUPT_PRIORITY.

            Numerically low interrupt priority numbers represent logically high
            interrupt priorities, therefore the priority of the interrupt must
            be set to a value equal to or numerically *higher* than
            configMAX_SYSCALL_INTERRUPT_PRIORITY.

            Interrupts that use the FreeRTOS API must not be left at their
            default priority of zero as that is the highest possible priority,
            which is guaranteed to be above configMAX_SYSCALL_INTERRUPT_PRIORITY,
            and therefore also guaranteed to be invalid.

            FreeRTOS maintains separate thread and ISR API functions to ensure
            interrupt entry is as fast and simple as possible.

            The following links provide detailed information:
            http://www.freertos.org/RTOS-Cortex-M3-M4.html
            http://www.freertos.org/FAQHelp.html */
            configASSERT( ucCurrentPriority >= ucMaxSysCallPriority );
        }

        /* Priority grouping:  The interrupt controller (NVIC) allows the bits
        that define each interrupt's priority to be split between bits that
        define the interrupt's pre-emption priority bits and bits that define
        the interrupt's sub-priority.  For simplicity all bits must be defined
        to be pre-emption priority bits.  The following assertion will fail if
        this is not the case (if some bits represent a sub-priority).

        If the application only uses CMSIS libraries for interrupt
        configuration then the correct setting can be achieved on all Cortex-M
        devices by calling NVIC_SetPriorityGrouping( 0 ); before starting the
        scheduler.  Note however that some vendor specific peripheral libraries
        assume a non-zero priority group setting, in which cases using a value
        of zero will result in unpredicable behaviour. */
        configASSERT( NVIC_GetPriorityGrouping() <= ulMaxPRIGROUPValue );
    }

static uint8_t grtc_channel;

void GRTC_1_IRQHandler(void){
    //nrfx_grtc_irq_handler();
    BaseType_t ret;
    ret = xTaskIncrementTick();
    if (ret != pdFALSE){
        /* A context switch is required.  Context switching is performed in
        the PendSV interrupt.  Pend the PendSV interrupt. */
        SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
        __SEV();
    }
}
nrfx_gpiote_t gpiote = NRFX_GPIOTE_INSTANCE(NRF_GPIOTE20);
void clock_timeout(int32_t id, uint64_t cc_value, void * p_context){
    nrfx_gpiote_out_toggle(&gpiote, NRF_GPIO_PIN_MAP(2, 9));
}

static void increment_tick_cb(int32_t id, uint64_t cc_value, void * p_context){
    BaseType_t ret;
    ret = xTaskIncrementTick();
    if (ret != pdFALSE){
        /* A context switch is required.  Context switching is performed in
        the PendSV interrupt.  Pend the PendSV interrupt. */
        SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
        __SEV();
    }
}
/*
 * Setup the RTC time to generate the tick interrupts at the required
 * frequency.
 */
void vPortSetupTimerInterrupt( void )
{
  nrfx_gpiote_init(&gpiote, 3);

  nrfx_gpiote_output_config_t pin_config = {
    .drive = NRF_GPIO_PIN_S0S1,
    .input_connect = NRF_GPIO_PIN_INPUT_DISCONNECT,
    .pull = NRF_GPIO_PIN_NOPULL
  };

  nrfx_gpiote_output_configure(&gpiote, NRF_GPIO_PIN_MAP(2, 9), &pin_config, NULL);


    int ret = 0;
    /* Request LF clock */
    // nrf_drv_clock_lfclk_request(NULL);


    nrfx_grtc_clock_source_set(NRF_GRTC_CLKSEL_LFXO);
    /* Configure SysTick to interrupt at the requested rate. */
    ret = nrfx_grtc_init(0);
    if(ret){
        return;
    }
    ret = nrfx_grtc_syscounter_start(0, &grtc_channel);
    if(ret){
        return;
    }
    ret = nrfx_grtc_syscounter_cc_int_enable(grtc_channel);
    if(ret){
        return;
    }
    ret = nrfx_grtc_ready_check();
    if(!ret){
        return;
    }
    ret = nrfx_grtc_syscounter_cc_int_enable_check(grtc_channel);
    if(!ret){
        return;
    }



    nrfx_grtc_action_perform(NRFX_GRTC_ACTION_CLEAR);
    nrfx_grtc_action_perform(NRFX_GRTC_ACTION_START);

    nrfx_grtc_syscounter_cc_interval_set(grtc_channel, 0, 1000);
    nrfx_grtc_channel_callback_set(grtc_channel, clock_timeout, NULL);

    //nrf_grtc_int_enable   (portNRF_GRTC_REG, NRF_GRTC_INT_COMPARE4_MASK);
    //nrf_grtc_task_trigger (portNRF_GRTC_REG, NRF_GRTC_TASK_CLEAR);
    //nrf_grtc_task_trigger (portNRF_GRTC_REG, NRF_GRTC_TASK_START);
    //nrf_grtc_event_enable(portNRF_GRTC_REG, GRTC_EVTEN_OVRFLW_Msk);

    //NVIC_SetPriority(GRTC_1_IRQn, configKERNEL_INTERRUPT_PRIORITY);
    //NVIC_EnableIRQ(GRTC_1_IRQn);
}

void vPortYield(void){
    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
    __SEV();
    /* Barriers are normally not required but do ensure the code is completely
    within the specified behaviour for the architecture. */
    __DSB();
    __ISB();
}

#endif /* configASSERT_DEFINED */
