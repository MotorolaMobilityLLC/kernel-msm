/*
 * Copyright (c) 2011-2019 The Linux Foundation. All rights reserved.
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
 * @file VossWrapper.h
 *
 * @brief This header file contains the various structure definitions and
 * function prototypes for the RTOS abstraction layer, implemented for VOSS
 */

#ifndef __VOSS_WRAPPER_H
#define __VOSS_WRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif


/*---------------------------------------------------------------------------
 * Include Files
 * ------------------------------------------------------------------------*/

#include "sir_types.h"
#include "sir_params.h"
#include "sys_def.h"
#include "qdf_mc_timer.h"
#include "qdf_types.h"
#include "qdf_trace.h"
#include "qdf_mem.h"

/* Interlocked Compare Exchange related definitions */

/* Define basic constants for the ThreadX kernel.  */

#define TX_AUTO_ACTIVATE    1
#define TX_NO_ACTIVATE      0

/* API return values.  */
#define TX_SUCCESS          0x00
#define TX_QUEUE_FULL    0x01
/* ... */
#define TX_NO_INSTANCE      0x0D
/* ... */
#define TX_TIMER_ERROR      0x15
#define TX_TICK_ERROR       0x16
/* ... */

#ifndef true
#define true                1
#endif

#ifndef false
#define false               0
#endif

/* Following macro specifies the number of milliseconds which constitute 1 ThreadX timer tick. Used
   for mimicking the ThreadX timer behaviour on VOSS. */
/* Use the same MACRO used by firmware modules to calculate TICKs from mSec */
/* Mismatch would cause worng timer value to be programmed */
#define TX_MSECS_IN_1_TICK  SYS_TICK_DUR_MS

/* Signature with which the TX_TIMER struct is initialized, when the timer is created */
#define TX_AIRGO_TMR_SIGNATURE   0xDEADBEEF

#ifdef TIMER_MANAGER
#define  tx_timer_create(a, b, c, d, e, f, g, h)    tx_timer_create_intern_debug((void *)a, b, c, d, e, f, g, h, __FILE__, __LINE__)
#else
#define  tx_timer_create(a, b, c, d, e, f, g, h)    tx_timer_create_intern((void *)a, b, c, d, e, f, g, h)
#endif

/*--------------------------------------------------------------------*/
/* Timer structure                                                    */
/* This structure is used to implement ThreadX timer facility.  Just  */
/* like ThreadX, timer expiration handler executes at the highest     */
/* possible priority level, i.e. DISPATCH_LEVEL.                      */
/*--------------------------------------------------------------------*/
typedef struct TX_TIMER_STRUCT {
#ifdef WLAN_DEBUG
#define TIMER_MAX_NAME_LEN 50
	char timerName[TIMER_MAX_NAME_LEN];
#endif
	uint8_t sessionId;
	uint32_t expireInput;
	uint64_t tmrSignature;
	void (*pExpireFunc)(void *, uint32_t);
	uint64_t initScheduleTimeInMsecs;
	uint64_t rescheduleTimeInMsecs;
	qdf_mc_timer_t qdf_timer;

	/* Pointer to the MAC global structure, which stores the context for the NIC, */
	/* for which this timer is supposed to operate. */
	void *mac;

} TX_TIMER;

#define TX_TIMER_VALID(timer) (timer.mac != 0)

uint32_t tx_timer_activate(TX_TIMER *);
uint32_t tx_timer_change(TX_TIMER *, uint64_t, uint64_t);
uint32_t tx_timer_change_context(TX_TIMER *, uint32_t);
#ifdef TIMER_MANAGER
uint32_t tx_timer_create_intern_debug(void *, TX_TIMER *,
				      char *, void (*)(void *,
						       uint32_t),
				      uint32_t, uint64_t,
				      uint64_t, uint64_t,
				      char *fileName,
				      uint32_t lineNum);
#else
uint32_t tx_timer_create_intern(void *, TX_TIMER *, char *,
				void (*)(void *, uint32_t),
				uint32_t, uint64_t, uint64_t,
				uint64_t);
#endif
uint32_t tx_timer_deactivate(TX_TIMER *);
uint32_t tx_timer_delete(TX_TIMER *);
bool tx_timer_running(TX_TIMER *);

#ifdef __cplusplus
}
#endif
#endif
