/*
 * platform_scu_log.c: Platform data for intel_fw_logging driver.
 *
 * (C) Copyright 2013 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#ifndef _PLATFORM_SCU_LOG_H_
#define _PLATFORM_SCU_LOG_H_

extern void __init *scu_log_platform_data(
	void *info) __attribute__((weak));

#endif
