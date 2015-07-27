/* Copyright (c) 2015, Huawei Technologies Co., Ltd. All rights reserved.
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

#ifndef __HUAWEI_RAMOOPS_H
#define __HUAWEI_RAMOOPS_H

#define HUAWEI_PERSISTENT_RAM_SIZE  (SZ_1M)
#define HUAWEI_RAM_CONSOLE_SIZE     (SZ_1K * 128 * 2)

#ifdef CONFIG_PSTORE_RAM
extern void huawei_reserve(void);
extern void huawei_add_persistent_device(void);
#else
static inline void huawei_reserve(void){};
static inline void huawei_add_persistent_device(void){};
#endif /* CONFIG_PSTORE_RAM */

#endif /* __HUAWEI_RAMOOPS_H */
