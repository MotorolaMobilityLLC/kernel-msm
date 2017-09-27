/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/notifier.h>

void register_hssp_update_notify(struct notifier_block *nb);
void unregister_hssp_update_notify(struct notifier_block *nb);
int cycapsense_fw_update(void);
int cycapsense_reset(void);
/*
static inline void register_hssp_update_notify(struct notifier_block *nb) { }
static inline void unregister_hssp_update_notify(struct notifier_block *nb) { }
static inline int cycapsense_fw_update(void) { return 0; }
static inline int cycapsense_reset(void) { return 0; }
*/
#define HSSP_START 0
#define HSSP_STOP 1
