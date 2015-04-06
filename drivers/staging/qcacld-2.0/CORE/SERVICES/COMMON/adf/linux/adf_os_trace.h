/*
 * Copyright (c) 2013 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */


#ifndef _ADF_TRACE_H
#define _ADF_TRACE_H

#include<adf_os_module_pvt.h>

/*
 * Log levels
*/
#define ADF_DEBUG_FUNCTRACE     0x01
#define ADF_DEBUG_LEVEL0        0x02
#define ADF_DEBUG_LEVEL1        0x04
#define ADF_DEBUG_LEVEL2        0x08
#define ADf_DEBUG_LEVEL3        0x10
#define ADF_DEBUG_ERROR         0x20
#define ADF_DEBUG_CFG           0x40

#define adf_trace(log_level, args...)                       \
                      do{                                   \
                            extern int adf_dbg_mask;        \
                            if(adf_dbg_mask >= log_level) { \
                            printk("adf: "args);            \
                            printk("\n");                   \
                            }                               \
                        }while(0)


#endif //_ADF_TRACE_H
