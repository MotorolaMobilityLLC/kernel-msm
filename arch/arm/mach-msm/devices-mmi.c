/* Copyright (c) 2012, Motorola Mobility, Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <asm/mach-types.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/w1-gpio.h>
#include <mach/board.h>
#include <mach/dma.h>
#include <mach/irqs-8960.h>
#include <mach/msm_iomap-8960-mmi.h>

static struct resource resources_uart_gsbi2[] = {
	{
		.start	= MSM8960_GSBI2_UARTDM_IRQ,
		.end	= MSM8960_GSBI2_UARTDM_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= MSM_UART2DM_PHYS,
		.end	= MSM_UART2DM_PHYS + PAGE_SIZE - 1,
		.name	= "uartdm_resource",
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= MSM_GSBI2_PHYS,
		.end	= MSM_GSBI2_PHYS + PAGE_SIZE - 1,
		.name	= "gsbi_resource",
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device mmi_msm8960_device_uart_gsbi2 = {
	.name	= "msm_serial_hsl",
	.id	= 2,
	.num_resources	= ARRAY_SIZE(resources_uart_gsbi2),
	.resource	= resources_uart_gsbi2,
};

static struct resource resources_uart_gsbi5[] = {
	{
		.start	= GSBI5_UARTDM_IRQ,
		.end	= GSBI5_UARTDM_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= MSM_UART5DM_PHYS,
		.end	= MSM_UART5DM_PHYS + PAGE_SIZE - 1,
		.name	= "uartdm_resource",
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= MSM_GSBI5_PHYS,
		.end	= MSM_GSBI5_PHYS + PAGE_SIZE - 1,
		.name	= "gsbi_resource",
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device mmi_msm8960_device_uart_gsbi5 = {
	.name	= "msm_serial_hsl",
	.id	= 5,
	.num_resources	= ARRAY_SIZE(resources_uart_gsbi5),
	.resource	= resources_uart_gsbi5,
};

static struct resource resources_uart_gsbi8[] = {
	{
		.start	= GSBI8_UARTDM_IRQ,
		.end	= GSBI8_UARTDM_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= MSM_UART8DM_PHYS,
		.end	= MSM_UART8DM_PHYS + PAGE_SIZE - 1,
		.name	= "uartdm_resource",
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= MSM_GSBI8_PHYS,
		.end	= MSM_GSBI8_PHYS + PAGE_SIZE - 1,
		.name	= "gsbi_resource",
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device mmi_msm8960_device_uart_gsbi8 = {
	.name	= "msm_serial_hsl",
	.id	= 8,
	.num_resources	= ARRAY_SIZE(resources_uart_gsbi8),
	.resource	= resources_uart_gsbi8,
};

/* GSBI 5 used into UARTDM Mode */
static struct resource msm_uart_dm5_resources[] = {
	{
		.start	= MSM_UART5DM_PHYS,
		.end	= MSM_UART5DM_PHYS + PAGE_SIZE - 1,
		.name	= "uartdm_resource",
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= GSBI5_UARTDM_IRQ,
		.end	= GSBI5_UARTDM_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= MSM_GSBI5_PHYS,
		.end	= MSM_GSBI5_PHYS + 4 - 1,
		.name	= "gsbi_resource",
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= DMOV_HSUART_GSBI6_TX_CHAN, /* chan 8 is unused */
		.end	= DMOV_HSUART_GSBI6_RX_CHAN, /* chan 7 is unused */
		.name	= "uartdm_channels",
		.flags	= IORESOURCE_DMA,
	},
	{
		.start	= DMOV_HSUART_GSBI5_TX_CRCI,
		.end	= DMOV_HSUART_GSBI5_RX_CRCI,
		.name	= "uartdm_crci",
		.flags	= IORESOURCE_DMA,
	},
};

static u64 msm_uart_dm5_dma_mask = DMA_BIT_MASK(32);
struct platform_device mmi_msm8960_device_uart_dm5 = {
	.name	= "msm_serial_hs",
	.id	= 0,
	.num_resources	= ARRAY_SIZE(msm_uart_dm5_resources),
	.resource	= msm_uart_dm5_resources,
	.dev	= {
		.dma_mask		= &msm_uart_dm5_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
};

static struct w1_gpio_platform_data mmi_w1_gpio_device_pdata = {
	.pin = -1,
	.is_open_drain = 0,
};

struct platform_device mmi_w1_gpio_device = {
	.name	= "w1-gpio",
	.dev	= {
		.platform_data = &mmi_w1_gpio_device_pdata,
	},
};
