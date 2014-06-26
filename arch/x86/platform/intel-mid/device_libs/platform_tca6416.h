/*
 * platform_tca6416.h: tca6416 platform data header file
 *
 * (C) Copyright 2008 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#ifndef _PLATFORM_TCA6416_H_
#define _PLATFORM_TCA6416_H_

#define TCA6416_NAME	"tca6416"
#define TCA6416_BASE	"tca6416_base"
#define TCA6416_INTR	"tca6416_int"

extern void *tca6416_platform_data(void *info) __attribute__((weak));
#endif
