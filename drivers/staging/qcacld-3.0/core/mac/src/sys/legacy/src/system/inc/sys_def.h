/*
 * Copyright (c) 2011-2014, 2018 The Linux Foundation. All rights reserved.
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
 *
 * This file sys_def.h contains the common definitions used to bring up
 * Sirius system.
 * Author:      V. K. Kandarpa
 * Date:        04/13/2002
 * History:-
 * Date         Modified by    Modification Information
 * --------------------------------------------------------------------
 */

#ifndef __SYSDEF_H
#define __SYSDEF_H

/* / Sirius system level definitions */
/* NOTE: Do not program system timer tick duration to less than 1msec */

/* / System timer tick duration in nanoseconds */
#define SYS_TICK_DUR_NS     10000000    /* 10ms */
#define SYS_TICK_TO_MICRO_SECOND   10000

/* / System timer tick duration in milliseconds */
#define SYS_TICK_DUR_MS     (SYS_TICK_DUR_NS/1000000)

/* / Macro to convert MS to Ticks */
#define SYS_MS_TO_TICKS(x)   ((x) / SYS_TICK_DUR_MS)

/* / MS to Time Units */
#define SYS_MS_TO_TU(x)      ((x * 1000) >> 10)

/* / TU to MS */
#define SYS_TU_TO_MS(x)      ((x << 10) / 1000)

#endif /* __SYSDEF_H */
