/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Theseus -- Console abstraction over UARTE.
 */

#ifndef THESEUS_CONSOLE_H
#define THESEUS_CONSOLE_H

/**
 * @brief Initialise the UART console.
 *
 * Configures the UARTE peripheral selected by Kconfig (CONFIG_CONSOLE_UART_*)
 * and wires picolibc stdout/stderr to it.  Must be called before any printf.
 */
void console_init(void);

#endif /* THESEUS_CONSOLE_H */
