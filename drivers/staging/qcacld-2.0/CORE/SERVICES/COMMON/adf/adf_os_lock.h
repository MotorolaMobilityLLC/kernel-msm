/*
 * Copyright (c) 2011 The Linux Foundation. All rights reserved.
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



/**
 * @ingroup adf_os_public
 * @file adf_os_lock.h
 * This file abstracts locking operations.
 */

#ifndef _ADF_OS_LOCK_H
#define _ADF_OS_LOCK_H

#include <adf_os_types.h>
#include <adf_os_lock_pvt.h>

/**
 * @brief Platform spinlock object
 */
typedef __adf_os_spinlock_t        adf_os_spinlock_t;
/**
 * @brief Platform mutex object
 */
typedef __adf_os_mutex_t        adf_os_mutex_t;



/**
 * @brief Initialize a mutex
 *
 * @param[in] m mutex to initialize
 */
static inline void adf_os_init_mutex(adf_os_mutex_t *m)
{
    __adf_os_init_mutex(m);
}


/**
 * @brief Take the mutex
 *
 * @param[in] m mutex to take
 */
static inline int adf_os_mutex_acquire(adf_os_device_t osdev, adf_os_mutex_t *m)
{
    return (__adf_os_mutex_acquire(osdev, m));
}

/**
 * @brief Give the mutex
 *
 * @param[in] m mutex to give
 */
static inline void adf_os_mutex_release(adf_os_device_t osdev, adf_os_mutex_t *m)
{
    __adf_os_mutex_release(osdev, m);
}
/**
 * @brief Platform specific semaphore object
 */
typedef __adf_os_mutex_t          adf_os_sem_t;
#define adf_os_sem_init adf_os_init_mutex
#define adf_os_sem_acquire adf_os_mutex_acquire
#define adf_os_sem_release adf_os_mutex_release


/**
 * @brief Initialize a spinlock
 *
 * @param[in] lock spinlock object pointer
 */
static inline void
adf_os_spinlock_init(adf_os_spinlock_t *lock)
{
    __adf_os_spinlock_init(lock);
}

/**
 * @brief Delete a spinlock
 *
 * @param[in] lock spinlock object pointer
 */
static inline void
adf_os_spinlock_destroy(adf_os_spinlock_t *lock)
{
    __adf_os_spinlock_destroy(lock);
}

#define adf_os_spin_lock( _lock) __adf_os_spin_lock(_lock)
#define adf_os_spin_unlock( _lock ) __adf_os_spin_unlock(_lock)
#define adf_os_spin_lock_irqsave( _lock) __adf_os_spin_lock_irqsave(_lock)
#define adf_os_spin_unlock_irqrestore( _lock ) \
	__adf_os_spin_unlock_irqrestore(_lock)

/**
 * @brief locks the spinlock mutex in soft irq context
 *
 * @param[in] lock  spinlock object pointer
 */
static inline void
adf_os_spin_lock_bh(adf_os_spinlock_t   *lock)
{
    __adf_os_spin_lock_bh(lock);
}

void adf_os_spin_lock_bh_outline(adf_os_spinlock_t *lock);

/**
 * @brief unlocks the spinlock mutex in soft irq context
 *
 * @param[in] lock  spinlock object pointer
 */
static inline void
adf_os_spin_unlock_bh(adf_os_spinlock_t *lock)
{
    __adf_os_spin_unlock_bh(lock);
}

void adf_os_spin_unlock_bh_outline(adf_os_spinlock_t *lock);

/**
 * @brief Execute the input function with spinlock held and interrupt disabled.
 *
 * @param[in] hdl       OS handle
 * @param[in] lock      spinlock to be held for the critical region
 * @param[in] func      critical region function that to be executed
 * @param[in] context   context of the critical region function
 *
 * @return Boolean status returned by the critical region function
 */
static inline a_bool_t
adf_os_spinlock_irq_exec(adf_os_handle_t           hdl,
                         adf_os_spinlock_t        *lock,
                         adf_os_irqlocked_func_t  func,
                         void                     *arg)
{
    return __adf_os_spinlock_irq_exec(hdl, lock, func, arg);
}

/**
 * @brief locks the spinlock in irq context
 *
 * @param[in] lock  spinlock object pointer
 * @param[in] flags flags value
 *
 */
#define adf_os_spin_lock_irq(_pLock, _flags)   __adf_os_spin_lock_irq(_pLock, _flags)
#define adf_os_spin_unlock_irq(_pLock, _flags) __adf_os_spin_unlock_irq(_pLock, _flags)

#define adf_os_in_softirq() __adf_os_in_softirq()

#endif
