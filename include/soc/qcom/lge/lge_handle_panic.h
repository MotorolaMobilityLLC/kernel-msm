/*
 * include/soc/qcom/lge/lge_handle_panic.h
 *
 * Copyright (C) 2014-2015 LG Electronics, Inc
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

#ifndef __MACH_LGE_HANDLE_PANIC_H
#define __MACH_LGE_HANDLE_PANIC_H

#include <soc/qcom/lge/reboot_reason.h>

void lge_set_subsys_crash_reason(const char *name, int type);
void lge_set_ram_console_addr(unsigned int addr, unsigned int size);
bool lge_set_panic_reason(void);
void lge_set_fb_addr(unsigned int addr);
void lge_set_restart_reason(unsigned int);
void lge_check_crash_skiped(char *reason);
bool lge_is_crash_skipped(void);
void lge_clear_crash_skipped(void);
void lge_disable_watchdog(void);
void lge_panic_handler_fb_cleanup(void);

struct panic_handler_data {
	unsigned long	fb_addr;
	unsigned long	fb_size;
};

#endif
