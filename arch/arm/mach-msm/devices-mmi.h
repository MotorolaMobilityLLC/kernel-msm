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

#ifndef __ARCH_ARM_MACH_MSM_DEVICES_MMI_H
#define __ARCH_ARM_MACH_MSM_DEVICES_MMI_H
#include "devices.h"

extern struct platform_device mmi_msm8960_device_qup_i2c_gsbi4;
extern struct platform_device mmi_msm8960_device_qup_i2c_gsbi8;

extern struct platform_device mmi_msm8960_device_uart_gsbi2;
extern struct platform_device mmi_msm8960_device_uart_gsbi5;
extern struct platform_device mmi_msm8960_device_uart_dm5;
extern struct platform_device mmi_msm8960_device_uart_gsbi8;

extern struct platform_device mmi_w1_gpio_device;

extern struct platform_device mmi_ram_console_device;
extern struct platform_device mmi_pm8xxx_rgb_leds_device;
extern struct persistent_ram mmi_ram_console_pram;
extern struct platform_device mmi_factory_device;
extern struct platform_device mmi_device_ext_5v_vreg;
#endif
