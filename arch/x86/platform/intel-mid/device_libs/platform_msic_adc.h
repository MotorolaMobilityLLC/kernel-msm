/*
 * platform_msic_adc.h: MSIC ADC platform data header file
 *
 * (C) Copyright 2008 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#ifndef _PLATFORM_MSIC_ADC_H_
#define _PLATFORM_MSIC_ADC_H_

#define ADC_DEVICE_NAME "msic_adc"

extern void __init *msic_adc_platform_data(void *info) __attribute__((weak));
#endif
