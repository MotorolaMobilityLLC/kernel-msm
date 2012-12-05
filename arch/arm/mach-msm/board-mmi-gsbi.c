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
#include <asm/mach/map.h>
#include <linux/clk.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <mach/board.h>
#include <mach/msm_iomap-8960-mmi.h>
#include <mach/msm_serial_hs_lite.h>
#include <mach/msm_spi.h>
#include "clock-gsbi-8960.h"
#include "devices-mmi.h"

/* must match type identifiers defined in DT schema */
#define GSBI_IDLE	0
#define GSBI_I2C_SIM	1
#define GSBI_I2C	2
#define GSBI_SPI	3
#define GSBI_UART	4
#define GSBI_SIM	5
#define GSBI_I2C_UART	6
#define GSBI_UARTDM	7

#define GSBI_MIN	1
#define GSBI_MAX	12

/* MMI - Device Tree Lookup Tables */
struct platform_device *i2c_dt_lookup_table[] __initdata = {
	NULL,
	NULL,
	NULL,
	&msm8960_device_qup_i2c_gsbi3,
	&mmi_msm8960_device_qup_i2c_gsbi4,
	NULL,
	NULL,
	NULL,
	&mmi_msm8960_device_qup_i2c_gsbi8,
	&msm8960_device_qup_i2c_gsbi9,
	&msm8960_device_qup_i2c_gsbi10,
	NULL,
	&msm8960_device_qup_i2c_gsbi12,
};

struct platform_device *spi_dt_lookup_table[] __initdata = {
	NULL,
	&msm8960_device_qup_spi_gsbi1,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
};

struct platform_device *uart_dt_lookup_table[] __initdata = {
	NULL,
	NULL,
	&mmi_msm8960_device_uart_gsbi2,
	NULL,
	NULL,
	&mmi_msm8960_device_uart_gsbi5,
	NULL,
	NULL,
	&mmi_msm8960_device_uart_gsbi8,
	NULL,
	NULL,
	NULL,
	NULL,
};

struct platform_device *uart_dm_dt_lookup_table[] __initdata = {
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	&mmi_msm8960_device_uart_dm5,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
};

static u32 gsbi_bases[] __initdata = {
	0x00000000, /* INVALID */
	MSM_GSBI1_PHYS,
	MSM_GSBI2_PHYS,
	MSM_GSBI3_PHYS,
	MSM_GSBI4_PHYS,
	MSM_GSBI5_PHYS,
	MSM_GSBI6_PHYS,
	MSM_GSBI7_PHYS,
	MSM_GSBI8_PHYS,
	MSM_GSBI9_PHYS,
	MSM_GSBI10_PHYS,
	MSM_GSBI11_PHYS,
	MSM_GSBI12_PHYS,
};

struct clk *gsbi_clock_lookup[] __initdata = {
	NULL, /* INVALID */
	&(gsbi1_p_clk.c),
	&(gsbi2_p_clk.c),
	&(gsbi3_p_clk.c),
	&(gsbi4_p_clk.c),
	&(gsbi5_p_clk.c),
	&(gsbi6_p_clk.c),
	&(gsbi7_p_clk.c),
	&(gsbi8_p_clk.c),
	&(gsbi9_p_clk.c),
	&(gsbi10_p_clk.c),
	&(gsbi11_p_clk.c),
	&(gsbi12_p_clk.c),
};

static void __init mmi_init_gsbi_protocol(int gsbi_id, u16 protocol)
{
	void *gsbi_pclk = gsbi_clock_lookup[gsbi_id];
	u32 gsbi_base = gsbi_bases[gsbi_id];
	void *gsbi_ctrl = ioremap_nocache(gsbi_base, MT_UNCACHED);

	clk_prepare_enable(gsbi_pclk);

	/* Write the protocol. */
	writel_relaxed(protocol << 4, gsbi_ctrl);
	mb(); /* Ensure that the data is completely written. */

	clk_disable_unprepare(gsbi_pclk);

	iounmap(gsbi_ctrl);
}

static void __init mmi_init_i2c_dev_from_dt(int gsbi_id,
						struct device_node *node,
						struct platform_device *dev)
{
	struct msm_i2c_platform_data *pdata;
	int len = 0;
	const void *prop;

	pdata = kzalloc(sizeof(struct msm_i2c_platform_data), GFP_KERNEL);
	if (!pdata)
		goto error;

	prop = of_get_property(node, "clk_freq", &len);
	if (!prop)
		goto error;
	pdata->clk_freq = *(u32 *)prop;
	prop = of_get_property(node, "src_clk_rate", &len);
	if (!prop)
		goto error;
	pdata->src_clk_rate = *(u32 *)prop;

	prop = of_get_property(node, "shared_mode", &len);
	if (prop)
		pdata->use_gsbi_shared_mode = *(u8 *)prop;

	dev->dev.platform_data = pdata;

	if (platform_device_register(dev)) {
		kfree(pdata);
		pr_err("%s: Failed to register platform device - GSBI%d!\n",
			__func__, gsbi_id);
	}

	return;
error:
	kfree(pdata);
	pr_err("%s: Missing device tree properties! Aborting GSBI%d!\n",
		__func__, gsbi_id);
}

static void __init mmi_init_spi_dev_from_dt(int gsbi_id,
						struct device_node *node,
						struct platform_device *dev)
{
	struct msm_spi_platform_data *pdata;
	int len = 0;
	const void *prop;

	pdata = kzalloc(sizeof(struct msm_spi_platform_data), GFP_KERNEL);
	if (!pdata)
		goto error;

	prop = of_get_property(node, "max_clock_speed", &len);
	if (!prop)
		goto error;
	pdata->max_clock_speed = *(u32 *)prop;

	dev->dev.platform_data = pdata;

	if (platform_device_register(dev)) {
		kfree(pdata);
		pr_err("%s: Failed to register platform device - GSBI%d!\n",
			__func__, gsbi_id);
	}

	return;
error:
	kfree(pdata);
	pr_err("%s: Missing device tree properties! Aborting GSBI%d!\n",
		__func__, gsbi_id);
}

static void __init mmi_init_uartdm_dev_from_dt(struct device_node *node,
						struct platform_device *dev)
{
	struct resource *r = NULL;
	int len = 0;
	int num;
	const void *prop;

	prop = of_get_property(node, "deviceid", &len);
	if (!prop || len != sizeof(u8))
		goto error;
	dev->id = *(u8 *)prop;

	num = dev->num_resources;
	r = kzalloc((num + 1) * sizeof(struct resource), GFP_KERNEL);
	if (r == NULL)
		goto error;

	r->name = "uartdm_channels";
	r->flags = IORESOURCE_DMA;

	prop = of_get_property(node, "txchan", &len);
	if (!prop || len != sizeof(u8))
		goto error;
	r->start = *(u8 *)prop;

	prop = of_get_property(node, "rxchan", &len);
	if (!prop || len != sizeof(u8))
		goto error;
	r->end = *(u8 *)prop;

	memcpy(r + 1, dev->resource, num * sizeof(struct resource));
	dev->resource = r;
	dev->num_resources++;

	if (platform_device_register(dev))
		goto error;

	return;
error:
	kfree(r);
	pr_err("%s: Failed to config UARTDM: %pF!\n", __func__, dev);
}

static void __init mmi_init_uart_dev_from_dt(struct device_node *node,
						struct platform_device *dev)
{
	struct msm_serial_hslite_platform_data *pdata = NULL;
	int len = 0;
	const void *prop;

	prop = of_get_property(node, "uart_line", &len);
	if (!prop || len != sizeof(u8))
		goto devreg;

	pdata = kzalloc(sizeof(struct msm_serial_hslite_platform_data),
			GFP_KERNEL);
	if (!pdata) {
		pr_err("%s: Couldn't allocate platform data...\n", __func__);
		goto devreg;
	}

	pdata->line = *(u8 *)prop;
	dev->dev.platform_data = pdata;

devreg:
	if (platform_device_register(dev)) {
		kfree(pdata);
		pr_err("%s: Failed to register platform device!\n", __func__);
	}
}

void __init mmi_init_gsbi_devices_from_dt(void)
{
	struct device_node *parent, *child;

	parent = of_find_node_by_path("/System@0/GSBISetup@0");
	if (!parent)
		goto out;

	/* fill out the array */
	for_each_child_of_node(parent, child) {
		int len = 0;
		const void *type_prop;

		type_prop = of_get_property(child, "gsbi_type", &len);
		if (type_prop && (len == sizeof(u8))) {
			int gsbi_id;
			struct platform_device *dev = NULL;

			sscanf(child->full_name,
				"/System@0/GSBISetup@0/GSBI@%d", &gsbi_id);

			if (gsbi_id < GSBI_MIN || gsbi_id > GSBI_MAX) {
				pr_warn("%s: GSBI does not exist! [%d]\n",
					__func__, gsbi_id);
				continue;
			}

			/* must match type identifiers defined in DT schema */
			switch (*(u8 *)type_prop) {
			case GSBI_IDLE:
				mmi_init_gsbi_protocol(gsbi_id, GSBI_IDLE);
				break;
			case GSBI_I2C_SIM:
				mmi_init_gsbi_protocol(gsbi_id, GSBI_I2C_SIM);

				dev = i2c_dt_lookup_table[gsbi_id];
				if (dev)
					mmi_init_i2c_dev_from_dt(gsbi_id,
								 child, dev);
				break;
			case GSBI_I2C:
				mmi_init_gsbi_protocol(gsbi_id, GSBI_I2C);

				dev = i2c_dt_lookup_table[gsbi_id];
				if (dev)
					mmi_init_i2c_dev_from_dt(gsbi_id,
								 child, dev);
				break;
			case GSBI_SPI:
				mmi_init_gsbi_protocol(gsbi_id, GSBI_SPI);
				dev = spi_dt_lookup_table[gsbi_id];
				if (!dev)
					pr_warn("%s: GSBI%d configured as " \
						"SPI but no device found\n",
						__func__, gsbi_id);
				else
					mmi_init_spi_dev_from_dt(gsbi_id,
								 child, dev);
				break;
			case GSBI_UART:
				mmi_init_gsbi_protocol(gsbi_id, GSBI_UART);
				dev = uart_dt_lookup_table[gsbi_id];
				if (!dev)
					pr_warn("%s: GSBI%d configured as " \
						"UART but no device found\n",
						__func__, gsbi_id);
				else
					mmi_init_uart_dev_from_dt(child, dev);
				break;
			case GSBI_UARTDM:
				mmi_init_gsbi_protocol(gsbi_id, GSBI_UART);
				dev = uart_dm_dt_lookup_table[gsbi_id];
				if (!dev)
					pr_warn("%s: GSBI%d configured as " \
						"UART DM but no device found\n",
						__func__, gsbi_id);
				else
					mmi_init_uartdm_dev_from_dt(child,
								    dev);
				break;
			case GSBI_SIM:
				mmi_init_gsbi_protocol(gsbi_id, GSBI_SIM);
				break;
			case GSBI_I2C_UART:
				mmi_init_gsbi_protocol(gsbi_id, GSBI_I2C_UART);

				dev = i2c_dt_lookup_table[gsbi_id];

				if (dev)
					mmi_init_i2c_dev_from_dt(gsbi_id,
								 child, dev);

				dev = uart_dt_lookup_table[gsbi_id];
				if (dev)
					mmi_init_uart_dev_from_dt(child, dev);
				break;
			default:
				pr_err("Unknown gsbi type... Ignoring\n");
				break;
			}
		}
	}

out:
	of_node_put(parent);

	return;
}
