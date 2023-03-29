/*
 * Copyright (c) 2012-2020 The Linux Foundation. All rights reserved.
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

/*===========================================================================
   @file VossWrapper.c

   @brief This source file contains the various function definitions for the
   RTOS abstraction layer, implemented for VOSS
   ===========================================================================*/

/*===========================================================================

			EDIT HISTORY FOR FILE

   This section contains comments describing changes made to the module.
   Notice that changes are listed in reverse chronological order.

   $Header:$ $DateTime: $ $Author: $

   when        who    what, where, why
   --------    ---    --------------------------------------------------------
   03/31/09    sho    Remove the use of qdf_timerIsActive flag as it is not
			thread-safe
   02/17/08    sho    Fix the timer callback function to work when it is called
			after the timer has stopped due to a race condition.
   02/10/08    sho    Refactor the TX timer to use VOS timer directly instead
			of using VOS utility timer
   12/15/08    sho    Resolved errors and warnings from the AMSS compiler when
			this is ported from WM
   11/20/08    sho    Renamed this to VosWrapper.c; remove all dependencies on
			WM platform and allow this to work on all VOSS enabled
			platform
   06/24/08    tbh    Modified the file to remove the dependecy on HDD files as
			part of Gen6 bring up process.
   10/29/02 Neelay Das Created file.

   ===========================================================================*/

/*---------------------------------------------------------------------------
 * Include Files
 * ------------------------------------------------------------------------*/
#include "sys_wrapper.h"

#ifdef WLAN_DEBUG
#define TIMER_NAME (timer_ptr->timerName)
#else
#define TIMER_NAME "N/A"
#endif

/**---------------------------------------------------------------------
 * tx_timer_activate()
 *
 * FUNCTION:
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param
 *
 * @return TX_SUCCESS.
 *
 */
uint32_t tx_timer_activate(TX_TIMER *timer_ptr)
{
	QDF_STATUS status;

	/* Uncomment the asserts, if the intention is to debug the occurrence of the */
	/* following anomalous cnditions. */

	/* Assert that the timer structure pointer passed, is not NULL */
	/* dbgAssert(timer_ptr); */

	/* If the NIC is halting just spoof a successful timer activation, so that all */
	/* the timers can be cleaned up. */

	if (!timer_ptr)
		return TX_TIMER_ERROR;

	/* Put a check for the free builds */
	if (TX_AIRGO_TMR_SIGNATURE != timer_ptr->tmrSignature) {
		QDF_ASSERT(timer_ptr->tmrSignature == 0);

		return TX_TIMER_ERROR;

	}
	/* Check for an uninitialized timer */
	QDF_ASSERT(0 != strlen(TIMER_NAME));

	status = qdf_mc_timer_start(&timer_ptr->qdf_timer,
				    timer_ptr->initScheduleTimeInMsecs);

	if (QDF_STATUS_SUCCESS == status) {
		return TX_SUCCESS;
	} else if (QDF_STATUS_E_ALREADY == status) {
		/* starting timer fails because timer is already started; this is okay */
		QDF_TRACE(QDF_MODULE_ID_SYS, QDF_TRACE_LEVEL_DEBUG,
			  "Timer %s is already running\n", TIMER_NAME);
		return TX_SUCCESS;
	} else {
		QDF_TRACE(QDF_MODULE_ID_SYS, QDF_TRACE_LEVEL_ERROR,
			  "Timer %s fails to activate\n", TIMER_NAME);
		return TX_TIMER_ERROR;
	}
} /*** tx_timer_activate() ***/

/**---------------------------------------------------------------------
 * tx_timer_change()
 *
 * FUNCTION:
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param
 *
 * @return TX_SUCCESS.
 *
 */
uint32_t tx_timer_change(TX_TIMER *timer_ptr,
			 uint64_t initScheduleTimeInTicks,
			 uint64_t rescheduleTimeInTicks)
{
	/* Put a check for the free builds */
	if (TX_AIRGO_TMR_SIGNATURE != timer_ptr->tmrSignature) {
		QDF_ASSERT(timer_ptr->tmrSignature == 0);
		return TX_TIMER_ERROR;
	}
	/* changes cannot be applied until timer stops running */
	if (QDF_TIMER_STATE_STOPPED ==
	    qdf_mc_timer_get_current_state(&timer_ptr->qdf_timer)) {
		timer_ptr->initScheduleTimeInMsecs =
			TX_MSECS_IN_1_TICK * initScheduleTimeInTicks;
		timer_ptr->rescheduleTimeInMsecs =
			TX_MSECS_IN_1_TICK * rescheduleTimeInTicks;
		return TX_SUCCESS;
	} else {
		return TX_TIMER_ERROR;
	}
} /*** tx_timer_change() ***/

/**---------------------------------------------------------------------
 * tx_timer_change_context()
 *
 * FUNCTION:
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param
 *
 * @return TX_SUCCESS.
 *
 */
uint32_t tx_timer_change_context(TX_TIMER *timer_ptr,
				 uint32_t expiration_input)
{

	/* Put a check for the free builds */
	if (TX_AIRGO_TMR_SIGNATURE != timer_ptr->tmrSignature) {
		QDF_ASSERT(timer_ptr->tmrSignature == 0);

		return TX_TIMER_ERROR;
	}
	/* changes cannot be applied until timer stops running */
	if (QDF_TIMER_STATE_STOPPED ==
	    qdf_mc_timer_get_current_state(&timer_ptr->qdf_timer)) {
		timer_ptr->expireInput = expiration_input;
		return TX_SUCCESS;
	} else {
		return TX_TIMER_ERROR;
	}
} /*** tx_timer_change() ***/

/**---------------------------------------------------------------------
 * tx_main_timer_func()
 *
 * FUNCTION:
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param
 *
 * @return None.
 *
 */
static void tx_main_timer_func(void *functionContext)
{
	TX_TIMER *timer_ptr = (TX_TIMER *) functionContext;

	if (!timer_ptr) {
		QDF_ASSERT(0);
		return;
	}

	if (!timer_ptr->pExpireFunc) {
		QDF_ASSERT(0);
		return;
	}

	/* Now call the actual timer function, taking the function pointer, */
	/* from the timer structure. */
	(*timer_ptr->pExpireFunc)(timer_ptr->mac, timer_ptr->expireInput);

	/* check if this needs to be rescheduled */
	if (0 != timer_ptr->rescheduleTimeInMsecs) {
		QDF_STATUS status;

		status = qdf_mc_timer_start(&timer_ptr->qdf_timer,
					    timer_ptr->rescheduleTimeInMsecs);
		timer_ptr->rescheduleTimeInMsecs = 0;

		if (QDF_STATUS_SUCCESS != status) {
			QDF_TRACE(QDF_MODULE_ID_SYS, QDF_TRACE_LEVEL_WARN,
				  "Unable to reschedule timer %s; status=%d",
				  TIMER_NAME, status);
		}
	}
} /*** tx_timer_change() ***/

#ifdef TIMER_MANAGER
uint32_t tx_timer_create_intern_debug(void *pMacGlobal,
				      TX_TIMER *timer_ptr, char *name_ptr,
				      void (*expiration_function)(void *,
								  uint32_t),
				      uint32_t expiration_input,
				      uint64_t initScheduleTimeInTicks,
				      uint64_t rescheduleTimeInTicks,
				      uint64_t auto_activate, char *fileName,
				      uint32_t lineNum)
{
	QDF_STATUS status;

	if (!expiration_function) {
		QDF_TRACE(QDF_MODULE_ID_SYS, QDF_TRACE_LEVEL_ERROR,
			  "NULL timer expiration");
		QDF_ASSERT(0);
		return TX_TIMER_ERROR;
	}

	if (!name_ptr) {

		QDF_TRACE(QDF_MODULE_ID_SYS, QDF_TRACE_LEVEL_ERROR,
			  "NULL name pointer for timer");
		QDF_ASSERT(0);
		return TX_TIMER_ERROR;
	}
	if (!initScheduleTimeInTicks)
		return TX_TICK_ERROR;

	if (!timer_ptr)
		return TX_TIMER_ERROR;

	/* Initialize timer structure */
	timer_ptr->pExpireFunc = expiration_function;
	timer_ptr->expireInput = expiration_input;
	timer_ptr->initScheduleTimeInMsecs =
		TX_MSECS_IN_1_TICK * initScheduleTimeInTicks;
	timer_ptr->rescheduleTimeInMsecs =
		TX_MSECS_IN_1_TICK * rescheduleTimeInTicks;
	timer_ptr->mac = pMacGlobal;

	/* Set the flag indicating that the timer was created */
	timer_ptr->tmrSignature = TX_AIRGO_TMR_SIGNATURE;

#ifdef WLAN_DEBUG
	/* Store the timer name */
	strlcpy(timer_ptr->timerName, name_ptr, sizeof(timer_ptr->timerName));
#endif /* Store the timer name, for Debug build only */

	status =
		qdf_mc_timer_init_debug(&timer_ptr->qdf_timer, QDF_TIMER_TYPE_SW,
					tx_main_timer_func, (void *) timer_ptr,
					fileName, lineNum);
	if (QDF_STATUS_SUCCESS != status) {
		QDF_TRACE(QDF_MODULE_ID_SYS, QDF_TRACE_LEVEL_ERROR,
			  "Cannot create timer for %s\n", TIMER_NAME);
		return TX_TIMER_ERROR;
	}

	if (0 != rescheduleTimeInTicks) {
		QDF_TRACE(QDF_MODULE_ID_SYS, QDF_TRACE_LEVEL_DEBUG,
			  "Creating periodic timer for %s\n", TIMER_NAME);
	}
	/* Activate this timer if required */
	if (auto_activate) {
		tx_timer_activate(timer_ptr);
	}

	return TX_SUCCESS;

} /* ** tx_timer_create() *** / */
#else
uint32_t tx_timer_create_intern(void *pMacGlobal, TX_TIMER *timer_ptr,
				char *name_ptr,
				void (*expiration_function)(void *,
							    uint32_t),
				uint32_t expiration_input,
				uint64_t initScheduleTimeInTicks,
				uint64_t rescheduleTimeInTicks,
				uint64_t auto_activate)
{
	QDF_STATUS status;

	if ((!name_ptr) || (!expiration_function))
		return TX_TIMER_ERROR;

	if (!initScheduleTimeInTicks)
		return TX_TICK_ERROR;

	if (!timer_ptr)
		return TX_TIMER_ERROR;

	/* Initialize timer structure */
	timer_ptr->pExpireFunc = expiration_function;
	timer_ptr->expireInput = expiration_input;
	timer_ptr->initScheduleTimeInMsecs =
		TX_MSECS_IN_1_TICK * initScheduleTimeInTicks;
	timer_ptr->rescheduleTimeInMsecs =
		TX_MSECS_IN_1_TICK * rescheduleTimeInTicks;
	timer_ptr->mac = pMacGlobal;

	/* Set the flag indicating that the timer was created */
	timer_ptr->tmrSignature = TX_AIRGO_TMR_SIGNATURE;

#ifdef WLAN_DEBUG
	/* Store the timer name */
	strlcpy(timer_ptr->timerName, name_ptr, sizeof(timer_ptr->timerName));
#endif /* Store the timer name, for Debug build only */

	status = qdf_mc_timer_init(&timer_ptr->qdf_timer, QDF_TIMER_TYPE_SW,
				   tx_main_timer_func, (void *) timer_ptr);
	if (QDF_STATUS_SUCCESS != status) {
		QDF_TRACE(QDF_MODULE_ID_SYS, QDF_TRACE_LEVEL_ERROR,
			  "Cannot create timer for %s\n", TIMER_NAME);
		return TX_TIMER_ERROR;
	}

	if (0 != rescheduleTimeInTicks) {
		QDF_TRACE(QDF_MODULE_ID_SYS, QDF_TRACE_LEVEL_INFO,
			  "Creating periodic timer for %s\n", TIMER_NAME);
	}
	/* Activate this timer if required */
	if (auto_activate) {
		tx_timer_activate(timer_ptr);
	}

	return TX_SUCCESS;

} /* ** tx_timer_create() *** / */
#endif

/**---------------------------------------------------------------------
 * tx_timer_deactivate()
 *
 * FUNCTION:
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param
 *
 * @return TX_SUCCESS.
 *
 */
uint32_t tx_timer_deactivate(TX_TIMER *timer_ptr)
{
	QDF_STATUS vStatus;

	/* Put a check for the free builds */
	if (TX_AIRGO_TMR_SIGNATURE != timer_ptr->tmrSignature) {
		return TX_TIMER_ERROR;
	}
	/* if the timer is not running then we do not need to do anything here */
	vStatus = qdf_mc_timer_stop(&timer_ptr->qdf_timer);
	if (QDF_STATUS_SUCCESS != vStatus) {
		QDF_TRACE(QDF_MODULE_ID_SYS, QDF_TRACE_LEVEL_INFO_HIGH,
			  "Unable to stop timer %s; status =%d\n",
			  TIMER_NAME, vStatus);
	}

	return TX_SUCCESS;

} /*** tx_timer_deactivate() ***/

uint32_t tx_timer_delete(TX_TIMER *timer_ptr)
{
	/* Put a check for the free builds */
	if (TX_AIRGO_TMR_SIGNATURE != timer_ptr->tmrSignature) {
		return TX_TIMER_ERROR;
	}

	qdf_mc_timer_destroy(&timer_ptr->qdf_timer);
	timer_ptr->tmrSignature = 0;

	return TX_SUCCESS;
} /*** tx_timer_delete() ***/

/**---------------------------------------------------------------------
 * tx_timer_running()
 *
 * FUNCTION:
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param
 *
 * @return TX_SUCCESS.
 *
 */
bool tx_timer_running(TX_TIMER *timer_ptr)
{
	/* Put a check for the free builds */
	if (TX_AIRGO_TMR_SIGNATURE != timer_ptr->tmrSignature)
		return false;

	if (QDF_TIMER_STATE_RUNNING ==
	    qdf_mc_timer_get_current_state(&timer_ptr->qdf_timer)) {
		return true;
	}
	return false;

} /*** tx_timer_running() ***/
