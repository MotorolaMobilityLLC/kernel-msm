/*
 * Copyright (c) 2013-2014 The Linux Foundation. All rights reserved.
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

#ifndef __WMMAPSD_H__
#define __WMMAPSD_H__

#include "ani_global.h"

/* UAPSD Flag for each AC (WMM spec 2.2.1) */
#define LIM_UAPSD_BITOFFSET_ACVO     0
#define LIM_UAPSD_BITOFFSET_ACVI     1
#define LIM_UAPSD_BITOFFSET_ACBK     2
#define LIM_UAPSD_BITOFFSET_ACBE     3

#define LIM_UAPSD_FLAG_ACVO     (1 << LIM_UAPSD_BITOFFSET_ACVO)
#define LIM_UAPSD_FLAG_ACVI     (1 << LIM_UAPSD_BITOFFSET_ACVI)
#define LIM_UAPSD_FLAG_ACBK     (1 << LIM_UAPSD_BITOFFSET_ACBK)
#define LIM_UAPSD_FLAG_ACBE     (1 << LIM_UAPSD_BITOFFSET_ACBE)

#define LIM_UAPSD_GET(ac, mask)      (((mask) & (LIM_UAPSD_FLAG_ ## ac)) >> LIM_UAPSD_BITOFFSET_ ## ac)

/* Definition for setting/clearing Uapsd Mask */
#define SET_UAPSD_MASK    1
#define CLEAR_UAPSD_MASK  0

#endif /* __WMMAPSD_H__ */
