/**
 * @brief This module defines the Interrupt Abstraction Layer for the 802.15.4 driver.
 *
 * Interrupt Abstraction Layer can be used by other modules to configure, enable and disable
 * interrupts controlled by the 802.15.4 driver.
 */

#include <stdbool.h>
#include <stdint.h>
#include <nrf.h>

/**
 * @brief Function pointer used for IRQ handling.
 *
 * This type intentionally does not specify any parameters of the function.
 * In contrast to C++, where unspecified function's parameter would be equivalent
 * to void parameter, in C such syntax indicates that the function in question
 * accepts parameters of any type. Therefore, it's convenient for compatibility reasons,
 * as the following functions:
 *
 * @code
 * void foo(void);
 * void bar(void * p_parameter);
 * @endcode
 *
 * are both of type nrf_802154_isr_t and can both be passed to functions expecting a parameter
 * of this type.
 */
typedef void (*nrf_802154_isr_t)();

/**
 * @brief Initializes a provided interrupt line.
 *
 * @note This function does not enable the interrupt. In order to enable it,  additional call
 *       to @ref nrf_802154_irq_enable is necessary.
 *
 * @param[in] irqn  IRQ line number.
 * @param[in] prio  Priority of the IRQ.
 * @param[in] isr   Pointer to ISR.
 *
 * @note Interpretation of the value represented by @p prio is platform-dependent and defined by
 *       the implementation.
 */
void nrf_802154_irq_init(uint32_t irqn, int32_t prio, nrf_802154_isr_t isr)
{
	NVIC_SetPriority(irqn, prio);
}

/**
 * @brief Enables an interrupt.
 *
 * @param[in] irqn  IRQ line number.
 */
void nrf_802154_irq_enable(uint32_t irqn)
{
	NVIC_EnableIRQ(irqn);
}

/**
 * @brief Disables an interrupt.
 *
 * @param[in] irqn  IRQ line number.
 */
void nrf_802154_irq_disable(uint32_t irqn)
{
	NVIC_DisableIRQ(irqn);
}

/**
 * @brief Sets an interrupt pending.
 *
 * @param[in] irqn  IRQ line number.
 */
void nrf_802154_irq_set_pending(uint32_t irqn)
{
	NVIC_SetPendingIRQ(irqn);
}

/**
 * @brief Clears a pending interrupt.
 *
 * @param[in] irqn  IRQ line number.
 */
void nrf_802154_irq_clear_pending(uint32_t irqn)
{
	NVIC_ClearPendingIRQ(irqn);
}

/**
 * @brief Checks if an interrupt is enabled.
 *
 * @param[in] irqn  IRQ line number.
 *
 * @retval true   IRQ is enabled.
 * @retval false  Otherwise.
 */
bool nrf_802154_irq_is_enabled(uint32_t irqn)
{
	return NVIC_GetEnableIRQ(irqn);
}

/**
 * @brief Gets priority of an interrupt.
 *
 * This function returns the actual value of the IRQ priority present in the IRQ configuration
 * register. It might not be equal to the IRQ priority specified in @ref nrf_802154_irq_init.
 * In order to reliably compare IRQ priorities, do not rely on the values of IRQ priorities
 * passed to @ref nrf_802154_irq_init, but rather always use this function instead.
 *
 * @param[in] irqn  IRQ line number.
 *
 * @return Priority of the provided interrupt.
 */
uint32_t nrf_802154_irq_priority_get(uint32_t irqn)
{
	return NVIC_GetPriority(irqn);
}
