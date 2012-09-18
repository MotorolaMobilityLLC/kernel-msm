/*
 * File: VibeOSKernelLinuxTime.c
 *
 * Description:
 *     Time helper functions for Linux.
 *
 * Portions Copyright (c) 2008-2011 Immersion Corporation. All Rights Reserved.
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the GNU Public License v2 -
 * (the 'License'). You may not use this file except in compliance with the
 * License. You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or contact
 * TouchSenseSales@immersion.com.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND IMMERSION HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see
 * the License for the specific language governing rights and limitations
 * under the License.
 */

#error "Please read the following statement"
/*
 * Kernel standard software timer is used as an example but another type
 * of timer (such as HW timer or high-resolution software timer) might be used
 * to achieve the 5ms required rate.
 */
#error "End of statement"

#if (HZ != 1000)
#error The Kernel timer is not configured at 1ms. Please update TIMER_INCR to generate a proper 5ms timer.
#endif

#include <linux/mutex.h>

#define TIMER_INCR              5       /* run timer at 5 jiffies (== 5ms) */
#define WATCHDOG_TIMEOUT        10      /* 10 timer cycles = 50ms */

/* For compatibility with older Kernels */
#ifndef DEFINE_SEMAPHORE
#define DEFINE_SEMAPHORE(name) \
	struct semaphore name = __SEMAPHORE_INITIALIZER(name, 1)
#endif

/* Global variables */
static bool g_bTimerStarted = false;
static struct timer_list g_timerList;
static int g_nWatchdogCounter = 0;

DEFINE_SEMAPHORE(g_hMutex);

/* Forward declarations */
static void VibeOSKernelLinuxStartTimer(void);
static void VibeOSKernelLinuxStopTimer(void);
static int VibeOSKernelProcessData(void* data);
#define VIBEOSKERNELPROCESSDATA

static inline int VibeSemIsLocked(struct semaphore *lock)
{
#if ((LINUX_VERSION_CODE & 0xFFFFFF) < KERNEL_VERSION(2,6,27))
	return atomic_read(&lock->count) != 1;
#else
	return (lock->count) != 1;
#endif
}

static void tsp_timer_interrupt(unsigned long param)
{
	/* Scheduling next timeout value right away */
	mod_timer(&g_timerList, jiffies + TIMER_INCR);

	if(g_bTimerStarted) {
		if (VibeSemIsLocked(&g_hMutex))
			up(&g_hMutex);
	}
}

static int VibeOSKernelProcessData(void* data)
{
	int i;
	int nActuatorNotPlaying = 0;

	for (i = 0; i < NUM_ACTUATORS; i++) {
		actuator_samples_buffer *pCurrentActuatorSample = &(g_SamplesBuffer[i]);

		if (-1 == pCurrentActuatorSample->nIndexPlayingBuffer) {
			nActuatorNotPlaying++;
			if ((NUM_ACTUATORS == nActuatorNotPlaying) &&
					((++g_nWatchdogCounter) > WATCHDOG_TIMEOUT)) {
				VibeInt8 cZero[1] = {0};

				/* Nothing to play for all actuators,
				 * turn off the timer when we reach
				 * the watchdog tick count limit
				 */
				ImmVibeSPI_ForceOut_SetSamples(i, 8, 1, cZero);
				ImmVibeSPI_ForceOut_AmpDisable(i);
				VibeOSKernelLinuxStopTimer();

				/* Reset watchdog counter */
				g_nWatchdogCounter = 0;
			}
		} else {
			/* Play the current buffer */
			if (VIBE_E_FAIL == ImmVibeSPI_ForceOut_SetSamples(
				pCurrentActuatorSample->actuatorSamples[(int)pCurrentActuatorSample->nIndexPlayingBuffer].nActuatorIndex,
				pCurrentActuatorSample->actuatorSamples[(int)pCurrentActuatorSample->nIndexPlayingBuffer].nBitDepth,
				pCurrentActuatorSample->actuatorSamples[(int)pCurrentActuatorSample->nIndexPlayingBuffer].nBufferSize,
				pCurrentActuatorSample->actuatorSamples[(int)pCurrentActuatorSample->nIndexPlayingBuffer].dataBuffer)) {
				/* VIBE_E_FAIL means NAK has been handled.
					Schedule timer to restart 5 ms from now */
				mod_timer(&g_timerList, jiffies + TIMER_INCR);
			}

			pCurrentActuatorSample->nIndexOutputValue += pCurrentActuatorSample->actuatorSamples[(int)pCurrentActuatorSample->nIndexPlayingBuffer].nBufferSize;

			if (pCurrentActuatorSample->nIndexOutputValue >= pCurrentActuatorSample->actuatorSamples[(int)pCurrentActuatorSample->nIndexPlayingBuffer].nBufferSize) {
				/* Reach the end of the current buffer */
				pCurrentActuatorSample->actuatorSamples[(int)pCurrentActuatorSample->nIndexPlayingBuffer].nBufferSize = 0;

				/* Switch buffer */
				(pCurrentActuatorSample->nIndexPlayingBuffer) ^= 1;
				pCurrentActuatorSample->nIndexOutputValue = 0;

				/* Finished playing, disable amp for actuator (i) */
				if (g_bStopRequested) {
					pCurrentActuatorSample->nIndexPlayingBuffer = -1;

					ImmVibeSPI_ForceOut_AmpDisable(i);
				}
			}
		}
	}

	/* If finished playing, stop timer */
	if (g_bStopRequested) {
		VibeOSKernelLinuxStopTimer();

		/* Reset watchdog counter */
		g_nWatchdogCounter = 0;

		if (VibeSemIsLocked(&g_hMutex))
			up(&g_hMutex);
		return 1;   /* tell the caller this is the last iteration */
	}

	return 0;
}

static void VibeOSKernelLinuxInitTimer(void)
{
	/* Initialize a 5ms-timer with VibeOSKernelTimerProc as timer callback */
	init_timer(&g_timerList);
	g_timerList.function = tsp_timer_interrupt;
}

static void VibeOSKernelLinuxStartTimer(void)
{
	int i;
	int res;

	/* Reset watchdog counter */
	g_nWatchdogCounter = 0;

	if (!g_bTimerStarted) {
		if (!VibeSemIsLocked(&g_hMutex))
			res = down_interruptible(&g_hMutex); /* start locked */

		g_bTimerStarted = true;

		/* Start the timer */
		g_timerList.expires = jiffies + TIMER_INCR;
		add_timer(&g_timerList);

		/* Don't block the write() function
		 * after the first sample to allow the host sending
		 * the next samples with no delay
		 */
		for (i = 0; i < NUM_ACTUATORS; i++) {
			if ((g_SamplesBuffer[i].actuatorSamples[0].nBufferSize) ||
				(g_SamplesBuffer[i].actuatorSamples[1].nBufferSize)) {
				g_SamplesBuffer[i].nIndexOutputValue = 0;
				return;
			}
		}
	}

	if (0 != VibeOSKernelProcessData(NULL))
		return;

	/*
	 * Use interruptible version of down to be safe
	 * (try to not being stuck here
	 * if the mutex is not freed for any reason)
	 */
	res = down_interruptible(&g_hMutex); /* wait for the mutex to be freed by the timer */
	if (res != 0) {
		DbgOut((KERN_INFO "VibeOSKernelLinuxStartTimer: down_interruptible interrupted by a signal.\n"));
	}
}

static void VibeOSKernelLinuxStopTimer(void)
{
	int i;

	if (g_bTimerStarted) {
		g_bTimerStarted = false;

		/*
		 * Stop the timer.
		 * Use del_timer vs. del_timer_sync
		 * del_timer_sync may cause a Kernel "soft lockup"
		 * on multi-CPU platforms
		 * as VibeOSKernelLinuxStopTimer is called
		 * from the timer tick handler.
		 */
		del_timer(&g_timerList);
	}

	/* Reset samples buffers */
	for (i = 0; i < NUM_ACTUATORS; i++) {
		g_SamplesBuffer[i].nIndexPlayingBuffer = -1;
		g_SamplesBuffer[i].actuatorSamples[0].nBufferSize = 0;
		g_SamplesBuffer[i].actuatorSamples[1].nBufferSize = 0;
	}
	g_bStopRequested = false;
	g_bIsPlaying = false;
}

static void VibeOSKernelLinuxTerminateTimer(void)
{
	VibeOSKernelLinuxStopTimer();
	if (VibeSemIsLocked(&g_hMutex))
		up(&g_hMutex);
}
