/*
 * Copyright (C) 2008 Google, Inc.
 * Copyright (C) 2010-2013, The Linux Foundation. All rights reserved.
 * Author: Nick Pelly <npelly@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __ASM_ARCH_MSM_SERIAL_HS_H
#define __ASM_ARCH_MSM_SERIAL_HS_H

#include<linux/serial_core.h>

/**
 * struct msm_serial_hs_platform_data - platform device data
 *                                      for msm hsuart device
 * @wakeup_irq : IRQ line to be configured as Wakeup source.
 * @inject_rx_on_wakeup : Set 1 if specific character to be inserted on wakeup
 * @rx_to_inject : Character to be inserted on wakeup
 * @config_gpio : Pass number of GPIOs to configure
		 (2 for 2-wire UART, 4 for 4-wire UART)
 * @userid : User-defined number to be used to enumerate device as tty<userid>
 * @uart_tx_gpio: GPIO number for UART Tx Line.
 * @uart_rx_gpio: GPIO number for UART Rx Line.
 * @uart_cts_gpio: GPIO number for UART CTS Line.
 * @uart_rfr_gpio: GPIO number for UART RFR Line.
 * @userid: Number to be used as ttyHS<userid> instead of ttyHS<id>
*/

struct msm_serial_hs_platform_data {
	int wakeup_irq;  /* wakeup irq */
	/* bool: inject char into rx tty on wakeup */
	unsigned char inject_rx_on_wakeup;
	char rx_to_inject;
	unsigned config_gpio;
	int uart_tx_gpio;
	int uart_rx_gpio;
	int uart_cts_gpio;
	int uart_rfr_gpio;
	int userid;
	int uartdm_rx_buf_size;
};

unsigned int msm_hs_tx_empty(struct uart_port *uport);
void msm_hs_request_clock_off(struct uart_port *uport);
void msm_hs_request_clock_on(struct uart_port *uport);
void msm_hs_set_mctrl(struct uart_port *uport,
				    unsigned int mctrl);
#endif
