/*
 * platform_bcove_adc.h: Head File for Merrifield Basincove GPADC driver
 *
 * (C) Copyright 2012 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#ifndef _PLATFORM_BCOVE_ADC_H_
#define _PLATFORM_BCOVE_ADC_H_

#define BCOVE_ADC_DEV_NAME	"bcove_adc"

extern void __init *bcove_adc_platform_data(void *info) __attribute__((weak));
#endif
