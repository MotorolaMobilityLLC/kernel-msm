/*
 * Copyright (c) 2019 The Linux Foundation. All rights reserved.
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

#include "__osif_driver_sync.h"
#include "osif_driver_sync.h"
#include "qdf_lock.h"
#include "qdf_status.h"
#include "qdf_types.h"
#include "wlan_dsc_driver.h"
#include "wlan_dsc_psoc.h"

/**
 * struct osif_driver_sync - a driver synchronization context
 * @dsc_driver: the dsc_driver used for synchronization
 * @in_use: indicates if the context is being used
 * @is_registered: indicates if the context is registered for
 *	transitions/operations
 */
struct osif_driver_sync {
	struct dsc_driver *dsc_driver;
	bool in_use;
	bool is_registered;
};

static struct osif_driver_sync __osif_driver_sync;
static qdf_spinlock_t __osif_driver_sync_lock;

#define osif_driver_sync_lock_create() \
	qdf_spinlock_create(&__osif_driver_sync_lock)
#define osif_driver_sync_lock_destroy() \
	qdf_spinlock_destroy(&__osif_driver_sync_lock)
#define osif_driver_sync_lock() qdf_spin_lock_bh(&__osif_driver_sync_lock)
#define osif_driver_sync_unlock() qdf_spin_unlock_bh(&__osif_driver_sync_lock)
#define osif_driver_sync_lock_assert() \
	QDF_BUG(qdf_spin_is_locked(&__osif_driver_sync_lock))

static struct osif_driver_sync *osif_driver_sync_lookup(void)
{
	osif_driver_sync_lock_assert();

	if (!__osif_driver_sync.is_registered)
		return NULL;

	return &__osif_driver_sync;
}

static struct osif_driver_sync *osif_driver_sync_get(void)
{
	osif_driver_sync_lock_assert();

	if (__osif_driver_sync.in_use)
		return NULL;

	__osif_driver_sync.in_use = true;

	return &__osif_driver_sync;
}

static void osif_driver_sync_put(struct osif_driver_sync *driver_sync)
{
	osif_driver_sync_lock_assert();

	qdf_mem_zero(driver_sync, sizeof(*driver_sync));
}

int osif_driver_sync_create(struct osif_driver_sync **out_driver_sync)
{
	QDF_STATUS status;
	struct osif_driver_sync *driver_sync;

	QDF_BUG(out_driver_sync);
	if (!out_driver_sync)
		return -EINVAL;

	osif_driver_sync_lock();
	driver_sync = osif_driver_sync_get();
	osif_driver_sync_unlock();
	if (!driver_sync)
		return -ENOMEM;

	status = dsc_driver_create(&driver_sync->dsc_driver);
	if (QDF_IS_STATUS_ERROR(status))
		goto sync_put;

	*out_driver_sync = driver_sync;

	return 0;

sync_put:
	osif_driver_sync_lock();
	osif_driver_sync_put(driver_sync);
	osif_driver_sync_unlock();

	return qdf_status_to_os_return(status);
}

int
__osif_driver_sync_create_and_trans(struct osif_driver_sync **out_driver_sync,
				    const char *desc)
{
	struct osif_driver_sync *driver_sync;
	QDF_STATUS status;
	int errno;

	errno = osif_driver_sync_create(&driver_sync);
	if (errno)
		return errno;

	status = dsc_driver_trans_start(driver_sync->dsc_driver, desc);
	if (QDF_IS_STATUS_ERROR(status))
		goto sync_destroy;

	*out_driver_sync = driver_sync;

	return 0;

sync_destroy:
	osif_driver_sync_destroy(driver_sync);

	return qdf_status_to_os_return(status);
}

void osif_driver_sync_destroy(struct osif_driver_sync *driver_sync)
{
	QDF_BUG(driver_sync);
	if (!driver_sync)
		return;

	dsc_driver_destroy(&driver_sync->dsc_driver);

	osif_driver_sync_lock();
	osif_driver_sync_put(driver_sync);
	osif_driver_sync_unlock();
}

void osif_driver_sync_register(struct osif_driver_sync *driver_sync)
{
	QDF_BUG(driver_sync);
	if (!driver_sync)
		return;

	osif_driver_sync_lock();
	driver_sync->is_registered = true;
	osif_driver_sync_unlock();
}

struct osif_driver_sync *osif_driver_sync_unregister(void)
{
	struct osif_driver_sync *driver_sync;

	osif_driver_sync_lock();
	driver_sync = osif_driver_sync_lookup();
	if (driver_sync)
		driver_sync->is_registered = false;
	osif_driver_sync_unlock();

	return driver_sync;
}

typedef QDF_STATUS (*driver_start_func)(struct dsc_driver *, const char *);

static int
__osif_driver_sync_start_callback(struct osif_driver_sync **out_driver_sync,
				  const char *desc,
				  driver_start_func driver_start_cb)
{
	QDF_STATUS status;
	struct osif_driver_sync *driver_sync;

	osif_driver_sync_lock_assert();

	*out_driver_sync = NULL;

	driver_sync = osif_driver_sync_lookup();
	if (!driver_sync)
		return -EAGAIN;

	status = driver_start_cb(driver_sync->dsc_driver, desc);
	if (QDF_IS_STATUS_ERROR(status))
		return qdf_status_to_os_return(status);

	*out_driver_sync = driver_sync;

	return 0;
}

static int
__osif_driver_sync_start_wait_callback(
				struct osif_driver_sync **out_driver_sync,
				const char *desc,
				driver_start_func driver_start_cb)
{
	QDF_STATUS status;
	struct osif_driver_sync *driver_sync;

	*out_driver_sync = NULL;

	osif_driver_sync_lock();
	driver_sync = osif_driver_sync_lookup();
	osif_driver_sync_unlock();
	if (!driver_sync)
		return -EAGAIN;

	status = driver_start_cb(driver_sync->dsc_driver, desc);
	if (QDF_IS_STATUS_ERROR(status))
		return qdf_status_to_os_return(status);

	*out_driver_sync = driver_sync;

	return 0;
}
int __osif_driver_sync_trans_start(struct osif_driver_sync **out_driver_sync,
				   const char *desc)
{
	int errno;

	osif_driver_sync_lock();
	errno = __osif_driver_sync_start_callback(out_driver_sync, desc,
						  dsc_driver_trans_start);
	osif_driver_sync_unlock();

	return errno;
}

int
__osif_driver_sync_trans_start_wait(struct osif_driver_sync **out_driver_sync,
				    const char *desc)
{
	int errno;

	/* since dsc_driver_trans_start_wait may sleep do not take lock here */
	errno = __osif_driver_sync_start_wait_callback(out_driver_sync, desc,
			dsc_driver_trans_start_wait);

	return errno;
}

void osif_driver_sync_trans_stop(struct osif_driver_sync *driver_sync)
{
	dsc_driver_trans_stop(driver_sync->dsc_driver);
}

void osif_driver_sync_assert_trans_protected(void)
{
	struct osif_driver_sync *driver_sync;

	osif_driver_sync_lock();

	driver_sync = osif_driver_sync_lookup();
	QDF_BUG(driver_sync);
	if (driver_sync)
		dsc_driver_assert_trans_protected(driver_sync->dsc_driver);

	osif_driver_sync_unlock();
}

int __osif_driver_sync_op_start(struct osif_driver_sync **out_driver_sync,
				const char *func)
{
	int errno;

	osif_driver_sync_lock();
	errno = __osif_driver_sync_start_callback(out_driver_sync, func,
						  _dsc_driver_op_start);
	osif_driver_sync_unlock();

	return errno;
}

void __osif_driver_sync_op_stop(struct osif_driver_sync *driver_sync,
				const char *func)
{
	_dsc_driver_op_stop(driver_sync->dsc_driver, func);
}

void osif_driver_sync_wait_for_ops(struct osif_driver_sync *driver_sync)
{
	dsc_driver_wait_for_ops(driver_sync->dsc_driver);
}

void osif_driver_sync_init(void)
{
	osif_driver_sync_lock_create();
}

void osif_driver_sync_deinit(void)
{
	osif_driver_sync_lock_destroy();
}

static QDF_STATUS
__osif_driver_sync_dsc_psoc_create(struct dsc_psoc **out_dsc_psoc)
{
	struct osif_driver_sync *driver_sync;

	osif_driver_sync_lock_assert();

	driver_sync = osif_driver_sync_lookup();
	if (!driver_sync)
		return QDF_STATUS_E_INVAL;

	return dsc_psoc_create(driver_sync->dsc_driver, out_dsc_psoc);
}

QDF_STATUS osif_driver_sync_dsc_psoc_create(struct dsc_psoc **out_dsc_psoc)
{
	QDF_STATUS status;

	osif_driver_sync_lock();
	status = __osif_driver_sync_dsc_psoc_create(out_dsc_psoc);
	osif_driver_sync_unlock();

	return status;
}

