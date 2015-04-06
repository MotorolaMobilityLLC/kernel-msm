/*
 * Copyright (c) 2013-2014 The Linux Foundation. All rights reserved.
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


#ifndef _ADF_CMN_OS_LOCK_PVT_H
#define _ADF_CMN_OS_LOCK_PVT_H

#include <linux/semaphore.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <adf_os_types.h>

typedef struct __adf_os_linux_spinlock {
	spinlock_t spinlock;
	unsigned int flags;
	unsigned long _flags;
} adf_os_linux_spinlock_t;

/* define for flag */
#define ADF_OS_LINUX_UNLOCK_BH	1

typedef adf_os_linux_spinlock_t   __adf_os_spinlock_t;
typedef struct semaphore __adf_os_mutex_t;

/**
 * @brief Initialize the mutex
 *
 * @param mutex
 *
 * @return
 */
static inline a_status_t
__adf_os_init_mutex(struct semaphore *m)
{
    sema_init(m, 1);
    return A_STATUS_OK;
}




static inline int
__adf_os_mutex_acquire(adf_os_device_t osdev, struct semaphore *m)
{
    down(m);
    return 0;
}

static inline void
__adf_os_mutex_release(adf_os_device_t osdev, struct semaphore *m)
{
    up(m);
}

static inline a_status_t
__adf_os_spinlock_init(__adf_os_spinlock_t *lock)
{
    spin_lock_init(&lock->spinlock);
    lock->flags = 0;

    return A_STATUS_OK;
}

#define __adf_os_spinlock_destroy(lock)
/**
 * @brief Acquire a Spinlock (SMP) & disable Preemption (Preemptive)
 *
 * @param lock      (Lock object)
 * @param flags     (Current IRQ mask)
 */
static inline void
__adf_os_spin_lock(__adf_os_spinlock_t *lock)
{
    spin_lock(&lock->spinlock);
}

/**
 * @brief Unlock the spinlock and enables the Preemption
 *
 * @param lock
 * @param flags
 */
static inline void
__adf_os_spin_unlock(__adf_os_spinlock_t *lock)
{
    spin_unlock(&lock->spinlock);
}

/**
 * @brief Acquire a Spinlock (SMP) & disable Preemption (Preemptive)
 *        Disable IRQs
 * @param lock      (Lock object)
 */
static inline void
__adf_os_spin_lock_irqsave(__adf_os_spinlock_t *lock)
{
    spin_lock_irqsave(&lock->spinlock, lock->_flags);
}

/**
 * @brief Unlock the spinlock and enables the Preemption
 *        Enable IRQ
 * @param lock      (Lock object)
 */
static inline void
__adf_os_spin_unlock_irqrestore(__adf_os_spinlock_t *lock)
{
    spin_unlock_irqrestore(&lock->spinlock, lock->_flags);
}

/**
 * @brief Acquire the spinlock and disable bottom halves
 *
 * @param lock
 */

/*
 * Synchronous versions - only for OS' that have interrupt disable
 */
#define __adf_os_spin_lock_irq(_pLock, _flags)    spin_lock_irqsave(_pLock, _flags)
#define __adf_os_spin_unlock_irq(_pLock, _flags)  spin_unlock_irqrestore(_pLock, _flags)

static inline void
__adf_os_spin_lock_bh(__adf_os_spinlock_t *lock)
{
	if (likely(irqs_disabled() || in_softirq())) {
		spin_lock(&lock->spinlock);
	} else {
		spin_lock_bh(&lock->spinlock);
		lock->flags |= ADF_OS_LINUX_UNLOCK_BH;
	}

}
static inline void
__adf_os_spin_unlock_bh(__adf_os_spinlock_t *lock)
{
	if (unlikely(lock->flags & ADF_OS_LINUX_UNLOCK_BH)) {
		lock->flags &= ~ADF_OS_LINUX_UNLOCK_BH;
		spin_unlock_bh(&lock->spinlock);
	} else
		spin_unlock(&lock->spinlock);
}

static inline a_bool_t
__adf_os_spinlock_irq_exec(adf_os_handle_t  hdl,
                           __adf_os_spinlock_t      *lock,
                           adf_os_irqlocked_func_t func,
                           void            *arg)
{
    unsigned long flags;
    a_bool_t ret;

    spin_lock_irqsave(&lock->spinlock, flags);
    ret = func(arg);
    spin_unlock_irqrestore(&lock->spinlock, flags);

    return ret;
}


static inline a_bool_t
__adf_os_in_softirq(void)
{
    return (in_softirq());
}
#endif /*_ADF_CMN_OS_LOCK_PVT_H*/
