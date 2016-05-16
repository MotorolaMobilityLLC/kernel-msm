/* include/soc/qcom/sharp/shrlog_hooks.h
 *
 * Copyright (C) 2010- Sharp Corporation
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __SHARP_SHRLOG_HOOKS_H
#define __SHARP_SHRLOG_HOOKS_H

#include <linux/device.h>
#include <linux/platform_device.h>

#ifdef CONFIG_SHARP_PANIC_ON_SUBSYS
#include <soc/qcom/subsystem_restart.h>
#endif /* CONFIG_SHARP_PANIC_ON_SUBSYS */

int shrlog_is_enabled(void);

#ifdef CONFIG_SHARP_PANIC_ON_SUBSYS
int shrlog_subsystem_restart_hook(struct subsys_desc *dev);
int shrlog_subsystem_device_panic_hook(struct subsys_desc *dev);
#endif /* CONFIG_SHARP_PANIC_ON_SUBSYS */

#if defined(CONFIG_SHARP_HANDLE_PANIC)
void sharp_panic_init_restart_reason_addr(void *addr);
bool sharp_panic_needs_warm_reset(const char *cmd, bool dload_mode, int restart_mode, int in_panic);
void sharp_panic_clear_restart_reason(void);
int sharp_panic_preset_restart_reason(int download_mode);
void sharp_panic_set_restart_reason(const char *cmd, bool dload_mode, int restart_mode, int in_panic);
#endif /* CONFIG_SHARP_HANDLE_PANIC */


#endif /* __SHARP_SHRLOG_HOOKS_H */


