/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Bare-metal nrfx glue layer for the NEE SDK.
 * Provides CMSIS-based implementations of the nrfx platform abstraction.
 */

#ifndef NRFX_GLUE_H__
#define NRFX_GLUE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <soc/nrfx_irqs.h>

/*--------------------------------------------------------------------------
 * Assertions
 *------------------------------------------------------------------------*/

#ifndef NRFX_ASSERT
#define NRFX_ASSERT(expression)     do { if (!(expression)) { __BKPT(0); } } while (0)
#endif

#define NRFX_STATIC_ASSERT(expression) _Static_assert(expression, "NRFX_STATIC_ASSERT")

/*--------------------------------------------------------------------------
 * IRQ management (CMSIS NVIC)
 *------------------------------------------------------------------------*/

#define NRFX_IRQ_PRIORITY_SET(irq_number, priority) NVIC_SetPriority(irq_number, priority)
#define NRFX_IRQ_ENABLE(irq_number)                 NVIC_EnableIRQ(irq_number)
#define NRFX_IRQ_IS_ENABLED(irq_number)             NVIC_GetEnableIRQ(irq_number)
#define NRFX_IRQ_DISABLE(irq_number)                NVIC_DisableIRQ(irq_number)
#define NRFX_IRQ_PENDING_SET(irq_number)            NVIC_SetPendingIRQ(irq_number)
#define NRFX_IRQ_PENDING_CLEAR(irq_number)          NVIC_ClearPendingIRQ(irq_number)
#define NRFX_IRQ_IS_PENDING(irq_number)             NVIC_GetPendingIRQ(irq_number)

/*--------------------------------------------------------------------------
 * Critical sections (interrupt-based nesting)
 *------------------------------------------------------------------------*/

extern volatile unsigned int theseus_critical_nesting;

#define NRFX_CRITICAL_SECTION_ENTER()                                       \
    do {                                                                    \
        __disable_irq();                                                    \
        theseus_critical_nesting++;                                             \
    } while (0)

#define NRFX_CRITICAL_SECTION_EXIT()                                        \
    do {                                                                    \
        theseus_critical_nesting--;                                             \
        if (theseus_critical_nesting == 0) {                                    \
            __enable_irq();                                                 \
        }                                                                   \
    } while (0)

/*--------------------------------------------------------------------------
 * Delay
 *------------------------------------------------------------------------*/

#define NRFX_DELAY_DWT_BASED 0

#include <lib/nrfx_coredep.h>
#define NRFX_DELAY_US(us_time) nrfx_coredep_delay_us(us_time)

/*--------------------------------------------------------------------------
 * Atomic operations (GCC builtins)
 *------------------------------------------------------------------------*/

typedef unsigned int nrfx_atomic_t;

#define NRFX_ATOMIC_FETCH_STORE(p_data, value) __atomic_exchange_n(p_data, value, __ATOMIC_SEQ_CST)
#define NRFX_ATOMIC_FETCH_OR(p_data, value)    __atomic_fetch_or(p_data, value, __ATOMIC_SEQ_CST)
#define NRFX_ATOMIC_FETCH_AND(p_data, value)   __atomic_fetch_and(p_data, value, __ATOMIC_SEQ_CST)
#define NRFX_ATOMIC_FETCH_XOR(p_data, value)   __atomic_fetch_xor(p_data, value, __ATOMIC_SEQ_CST)
#define NRFX_ATOMIC_FETCH_ADD(p_data, value)   __atomic_fetch_add(p_data, value, __ATOMIC_SEQ_CST)
#define NRFX_ATOMIC_FETCH_SUB(p_data, value)   __atomic_fetch_sub(p_data, value, __ATOMIC_SEQ_CST)
#define NRFX_ATOMIC_CAS(p_data, old_value, new_value)                       \
    __extension__ ({                                                        \
        nrfx_atomic_t _expected = (old_value);                              \
        __atomic_compare_exchange_n(p_data, &_expected, new_value, 0,       \
                                   __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);    \
    })

#define NRFX_CLZ(value) __builtin_clz(value)
#define NRFX_CTZ(value) __builtin_ctz(value)

/*--------------------------------------------------------------------------
 * Custom error codes
 *------------------------------------------------------------------------*/

#define NRFX_CUSTOM_ERROR_CODES 0

/*--------------------------------------------------------------------------
 * Event readback
 *------------------------------------------------------------------------*/

#define NRFX_EVENT_READBACK_ENABLED 1

/*--------------------------------------------------------------------------
 * Cache management (no data cache on nRF54L15 application core)
 *------------------------------------------------------------------------*/

#define NRFY_CACHE_WB(p_buffer, size)
#define NRFY_CACHE_INV(p_buffer, size)
#define NRFY_CACHE_WBINV(p_buffer, size)

/*--------------------------------------------------------------------------
 * Resource reservation bitmasks
 *------------------------------------------------------------------------*/

#define NRFX_DPPI_CHANNELS_USED   0
#define NRFX_DPPI_GROUPS_USED     0
#define NRFX_PPI_CHANNELS_USED    0
#define NRFX_PPI_GROUPS_USED      0
#define NRFX_GPIOTE_CHANNELS_USED 0
#define NRFX_EGUS_USED            0
#define NRFX_TIMERS_USED          0

#ifdef __cplusplus
}
#endif

#endif /* NRFX_GLUE_H__ */
