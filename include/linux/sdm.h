/*
 *  Copyright (C) Intel 2011
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * The SDM (System Debug Monitor) directs trace data routed from
 * various parts in the system out through the Intel Tangier PTI port and
 * out of the mobile device for analysis with a debugging tool
 * (Lauterbach, Fido). This is part of a solution for the MIPI P1149.7,
 * compact JTAG, standard and USB Debug-Class
 *
 * This header file will allow other parts of the OS to use the
 * interface to write out it's contents for debugging a mobile system.
 */

#ifndef SDM_H_
#define SDM_H_

#ifdef CONFIG_INTEL_PTI_STM
/* the following functions are defined in drivers/misc/stm.c */
int stm_kernel_get_out(void);
int stm_kernel_set_out(int bus_type);
int stm_is_enabled(void);
#else
static inline int stm_kernel_get_out(void) { return -EOPNOTSUPP; };
static inline int stm_kernel_set_out(int bus_type) { return -EOPNOTSUPP; };
static inline int stm_is_enabled(void) { return 0; };
#endif

/* Temporary : To be replace later with dynamic*/
#define STM_NB_IN_PINS                  0

/* STM output configurations */
#define STM_PTI_4BIT_LEGACY                    0
#define STM_PTI_4BIT_NIDNT                     1
#define STM_PTI_16BIT                          2
#define STM_PTI_12BIT                          3
#define STM_PTI_8BIT                           4
#define STM_USB                                15

/* Buffer configurations */
#define DFX_BULK_BUFFER_SIZE		64 /* for Tangier A0 */
#define DFX_BULK_OUT_BUFFER_ADDR	0xF90B0000
#define DFX_BULK_IN_BUFFER_ADDR		0xF90B0000
#define DFX_BULK_IN_BUFFER_ADDR_2	0xF90B0400

#define TRACE_BULK_BUFFER_SIZE		65536 /* revision */
#define TRACE_BULK_IN_BUFFER_ADDR	0xF90A0000 /* revision */

#endif /*SDM_H_*/

