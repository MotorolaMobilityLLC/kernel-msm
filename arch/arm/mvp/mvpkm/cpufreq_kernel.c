/*
 * Linux 2.6.32 and later Kernel module for VMware MVP Hypervisor Support
 *
 * Copyright (C) 2010-2013 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; see the file COPYING.  If not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#line 5

/**
 * @file
 *
 * @brief MVP host kernel cpufreq related
 *
 * Track CPU frequency changes.
 */

#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/cpufreq.h>
#include <linux/smp.h>

#include "mvp.h"
#include "cpufreq_kernel.h"
#include "mvpkm_kernel.h"
#include "mvp_timer.h"


/**
 * @brief Return current CPU frequency
 * @param cpu CPU number
 * @return CPU frequency in kHz, might be 0 (cpufreq)
 *
 * When CPU_FREQ is not available, it uses hardcoded frequencies.
 */
static uint32
GetCpuFrequency(unsigned int cpu)
{
   unsigned int counterKHZ;

#ifdef CONFIG_CPU_FREQ
   counterKHZ = cpufreq_quick_get(cpu);
#elif defined(MVP_HOST_BOARD_ve)
   /**
    * @knownjira{MVP-143}
    * We're only using this under the simulator, and it's almost non perceptible to
    * provide a fixed TSC frequency as the instructions / second executed widely
    * varies depending over time. While we resolve this issue we can use the
    * BogoMIPS reported at boot for now.
    */
   KNOWN_BUG(MVP-143);
   counterKHZ = 125e3;

   printk_once(KERN_INFO "mvpkm: CPU_FREQ not available, forcing TSC to %d kHz\n", counterKHZ);
#elif defined(MVP_HOST_BOARD_panda)
   counterKHZ = 1e6;
#else
   /*
    * If the kernel can't tell us and we have no further host knowledge,
    * time to die.
    */
#error "host TSC frequency unknown."
#endif

   return counterKHZ;
}

/**
 * @brief Compute TSC to RATE64 ratio
 * @param cpuFreq TSC frequency in kHz
 * @param[out] ttr tscToRate64 pointer
 */
static void
TscToRate64(uint32 cpuFreq, struct TscToRate64Cb *ttr)
{
   uint32 shift;
   uint64 mult;

   /*
    * A little bit of math !
    *
    * We need here to convert the TSC value to our RATE64 timebase.
    *
    * In other words:
    *
    *                   tsc * MVP_TIMER_RATE64
    *          rate64 = ----------------------
    *                           cpuFreq
    *
    * But we are limited by CPU performance (does not divide easily), CPU
    * instruction set, and CPU register file width. To fit performance
    * requirement, the math becomes:
    *
    *          rate64 = (cpuFreq * mult) >> shift
    *
    * To respect instruction set, both cpuFreq and mult must be 32-bit
    * numbers. Thus (cpuFreq * mult) will be a 64-bit number.
    *
    *
    *          Log2 rate64 = Log2 cpuFreq + Log2 mult - shift
    *
    *          shift = Log2 mult + Log2 cpuFreq - Log2 rate64
    *
    *                    && Log2 mult < 32
    *
    *    =>    shift < 32 + Log2 cpuFreq - Log2 rate64
    *
    *                 rate64 << shift
    *    =>    mult = ---------------
    *                      cpuFreq
    *
    * (rate64 << shift) must be a 64-bit number:
    *
    *          Log2 rate64 + shift < 64
    *
    *    =>    shift < 64 - Log2 rate64
    *
    * While cpuFreq is lower than 2^32 Hz, we have:
    *
    *          shift < 32 + Log2 cpuFreq - Log2 rate64 < 64 - Log2 rate64
    *
    * As (31 - CLZ32 x) <= Log2 x < (32 - CLZ32 x):
    *
    *          31 - CLZ32 cpuFreq <= Log2 cpuFreq   &&
    *
    *                                  CLZ32 rate64 - 32 < - Log2 rate64
    *
    *          31 + CLZ32 rate64 - CLZ32 cpuFreq < 32 + Log2 cpuFreq - Log2 rate64
    *
    * As we want shift to be as great as possible:
    *
    *    =>    shift = 31 + CLZ32 rate64 - CLZ32 cpuFreq
    *
    *                 rate64 << shift
    *    &&    mult = ---------------
    *                      cpuFreq
    *
    *
    */

   /* cpuFreq comes in kHz */
   cpuFreq *= 1000;

   /* CLZ(MVP_TIMER_RATE64) is optimized by compiler in a constant */
   shift = 31 + CLZ(MVP_TIMER_RATE64) - CLZ(cpuFreq);
   mult = MVP_TIMER_RATE64;
   mult <<= shift;
   do_div(mult, cpuFreq);

   /* verify Log2 mult < 32 */
   ASSERT(mult < (1ULL<<32));

   /* update global variables */
   ttr->mult = mult;
   ttr->shift = shift;
}

/**
 * @brief Compute a new TSC to rate64 ratio if CPU frequency changed
 * @param[in,out] freq Pointer to previous CPU frequency in kHz
 * @param[in,out] ttr Pointer to ratio values, set on change
 * @return 1 if ratio has changed, else 0
 */
int
CpuFreqUpdate(unsigned int *freq, struct TscToRate64Cb *ttr)
{
   unsigned int cur = GetCpuFrequency(smp_processor_id());
   int ret = (cur != *freq);

   if (ret) {
      if (cur) {
         TscToRate64(cur, ttr);
      } else {
         /*
          * Note that cpufreq_quick_get(cpu) can return 0 while cpufreq is
          * not yet ready on that core. This will make monitor run with a
          * degraded time for few ms.
          */
         ttr->mult = 1;
         ttr->shift = 64;
      }
      *freq = cur;
   }

   return ret;
}

/**
 * @brief Nop function
 * @param info Ignored
 */
static void
CpuFreqNop(void *info)
{
}

/**
 * @brief Handle cpufreq transition notifications.
 * @param nb Notifier block
 * @param val Notified event
 * @param data Linux cpufreq_freqs info
 * @return NOTIFY_OK
 *
 * @note A frequency change can fail in which case PRECHANGE and POSTCHANGE
 * will not be paired and you get any number of PRECHANGE and maybe never a
 * POSTCHANGE (i.e. there is not enough battery voltage available to support a
 * high frequency).
 * @note This is called once per cpu core that is changing but not always on
 * the core that is changing.
 */
static int
CpuFreqNotifier(struct notifier_block *nb,
                unsigned long val,
                void *data)
{
   struct cpufreq_freqs *freq = data;

   /*
    * Call CpuFreqNop() on the correct CPU core to force any currently running
    * vCPU's to worldswitch back to the host and update TSC to rate64 ratio
    * on next execution.
    */
   if (freq->old != freq->new &&
       val == CPUFREQ_POSTCHANGE &&
       cpumask_test_cpu(freq->cpu, &inMonitor)) {
      smp_call_function_single(freq->cpu, CpuFreqNop, NULL, false);
   }

   return NOTIFY_OK;
}

/**
 * @brief Notifier block for cpufreq transitions
 */
static struct notifier_block cpuFreqNotifierBlock = {
   .notifier_call = CpuFreqNotifier
};

/**
 * @brief Initialize TSC ratio and register cpufreq transitions.
 */
void
CpuFreq_Init(void)
{
   int ret;

   /* register callback on frequency change */
   ret = cpufreq_register_notifier(&cpuFreqNotifierBlock,
                                   CPUFREQ_TRANSITION_NOTIFIER);
   FATAL_IF(ret < 0);
}

/**
 * @brief Exit cpufreq, unregister cpufreq transitions
 */
void
CpuFreq_Exit(void)
{
   cpufreq_unregister_notifier(&cpuFreqNotifierBlock,
                               CPUFREQ_TRANSITION_NOTIFIER);
}
