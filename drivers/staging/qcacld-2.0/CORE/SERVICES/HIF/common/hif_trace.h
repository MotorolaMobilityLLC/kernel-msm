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

//==============================================================================
// HIF specific declarations and prototypes
//
// Author(s): ="Atheros"
//==============================================================================
#ifndef _HIF_TRACE_H_
#define _HIF_TRACE_H_

#include "vos_trace.h"
#ifdef ENTER
#undef ENTER
#endif
#define ENTER(fmt,...) VOS_TRACE(VOS_MODULE_ID_HIF, VOS_TRACE_LEVEL_INFO, "Enter: %s "fmt, __FUNCTION__,## __VA_ARGS__)

#ifdef EXIT
#undef EXIT
#endif
#define EXIT(fmt,...) VOS_TRACE(VOS_MODULE_ID_HIF, VOS_TRACE_LEVEL_INFO, "Exit: %s "fmt, __FUNCTION__,## __VA_ARGS__)

#endif //_HIF_TRACE_H_
