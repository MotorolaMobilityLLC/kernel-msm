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

#include "linux/device.h"
#include "__osif_driver_sync.h"
#include "__osif_psoc_sync.h"
#include "osif_psoc_sync.h"
#include "qdf_lock.h"
#include "qdf_status.h"
#include "qdf_types.h"
#include "wlan_dsc_psoc.h"
#include "wlan_dsc_vdev.h"

/**
 * struct osif_psoc_sync - a psoc synchronization context
 * @dev: the device used as a lookup key
 * @dsc_psoc: the dsc_psoc used for synchronization
 * @in_use: indicates if the context is being used
 */
struct osif_psoc_sync {
	struct device *dev;
	struct dsc_psoc *dsc_psoc;
	bool in_use;
};

static struct osif_psoc_sync __osif_psoc_sync_arr[WLAN_MAX_PSOCS];
static qdf_spinlock_t __osif_psoc_sync_lock;

#define osif_psoc_sync_lock_create() qdf_spinlock_create(&__osif_psoc_sync_lock)
#define osif_psoc_sync_lock_destroy() \
	qdf_spinlock_destroy(&__osif_psoc_sync_lock)
#define osif_psoc_sync_lock() qdf_spin_lock_bh(&__osif_psoc_sync_lock)
#define osif_psoc_sync_unlock() qdf_spin_unlock_bh(&__osif_psoc_sync_lock)
#define osif_psoc_sync_lock_assert() \
	QDF_BUG(qdf_spin_is_locked(&__osif_psoc_sync_lock))

static struct osif_psoc_sync *osif_psoc_sync_lookup(struct device *dev)
{
	int i;

	osif_psoc_sync_lock_assert();

	for (i = 0; i < QDF_ARRAY_SIZE(__osif_psoc_sync_arr); i++) {
		struct osif_psoc_sync *psoc_sync = __osif_psoc_sync_arr + i;

		if (!psoc_sync->in_use)
			continue;

		if (psoc_sync->dev == dev)
			return psoc_sync;
	}

	return NULL;
}

static struct osif_psoc_sync *osif_psoc_sync_get(void)
{
	int i;

	osif_psoc_sync_lock_assert();

	for (i = 0; i < QDF_ARRAY_SIZE(__osif_psoc_sync_arr); i++) {
		struct osif_psoc_sync *psoc_sync = __osif_psoc_sync_arr + i;

		if (!psoc_sync->in_use) {
			psoc_sync->in_use = true;
			return psoc_sync;
		}
	}

	return NULL;
}

static void osif_psoc_sync_put(struct osif_psoc_sync *psoc_sync)
{
	osif_psoc_sync_lock_assert();

	qdf_mem_zero(psoc_sync, sizeof(*psoc_sync));
}

int osif_psoc_sync_create(struct osif_psoc_sync **out_psoc_sync)
{
	QDF_STATUS status;
	struct osif_psoc_sync *psoc_sync;

	QDF_BUG(out_psoc_sync);
	if (!out_psoc_sync)
		return -EINVAL;

	osif_psoc_sync_lock();
	psoc_sync = osif_psoc_sync_get();
	osif_psoc_sync_unlock();
	if (!psoc_sync)
		return -ENOMEM;

	status = osif_driver_sync_dsc_psoc_create(&psoc_sync->dsc_psoc);
	if (QDF_IS_STATUS_ERROR(status))
		goto sync_put;

	*out_psoc_sync = psoc_sync;

	return 0;

sync_put:
	osif_psoc_sync_lock();
	osif_psoc_sync_put(psoc_sync);
	osif_psoc_sync_unlock();

	return qdf_status_to_os_return(status);
}

int __osif_psoc_sync_create_and_trans(struct osif_psoc_sync **out_psoc_sync,
				      const char *desc)
{
	struct osif_psoc_sync *psoc_sync;
	QDF_STATUS status;
	int errno;

	errno = osif_psoc_sync_create(&psoc_sync);
	if (errno)
		return errno;

	status = dsc_psoc_trans_start(psoc_sync->dsc_psoc, desc);
	if (QDF_IS_STATUS_ERROR(status))
		goto sync_destroy;

	*out_psoc_sync = psoc_sync;

	return 0;

sync_destroy:
	osif_psoc_sync_destroy(psoc_sync);

	return qdf_status_to_os_return(status);
}

void osif_psoc_sync_destroy(struct osif_psoc_sync *psoc_sync)
{
	QDF_BUG(psoc_sync);
	if (!psoc_sync)
		return;

	dsc_psoc_destroy(&psoc_sync->dsc_psoc);

	osif_psoc_sync_lock();
	osif_psoc_sync_put(psoc_sync);
	osif_psoc_sync_unlock();
}

void osif_psoc_sync_register(struct device *dev,
			     struct osif_psoc_sync *psoc_sync)
{
	QDF_BUG(dev);
	QDF_BUG(psoc_sync);
	if (!psoc_sync)
		return;

	osif_psoc_sync_lock();
	psoc_sync->dev = dev;
	osif_psoc_sync_unlock();
}

struct osif_psoc_sync *osif_psoc_sync_unregister(struct device *dev)
{
	struct osif_psoc_sync *psoc_sync;

	QDF_BUG(dev);
	if (!dev)
		return NULL;

	osif_psoc_sync_lock();
	psoc_sync = osif_psoc_sync_lookup(dev);
	if (psoc_sync)
		psoc_sync->dev = NULL;
	osif_psoc_sync_unlock();

	return psoc_sync;
}

typedef QDF_STATUS (*psoc_start_func)(struct dsc_psoc *, const char *);

static int
__osif_psoc_sync_start_callback(struct device *dev,
				struct osif_psoc_sync **out_psoc_sync,
				const char *desc,
				psoc_start_func psoc_start_cb)
{
	QDF_STATUS status;
	struct osif_psoc_sync *psoc_sync;

	osif_psoc_sync_lock_assert();

	*out_psoc_sync = NULL;

	psoc_sync = osif_psoc_sync_lookup(dev);
	if (!psoc_sync)
		return -EAGAIN;

	status = psoc_start_cb(psoc_sync->dsc_psoc, desc);
	if (QDF_IS_STATUS_ERROR(status))
		return qdf_status_to_os_return(status);

	*out_psoc_sync = psoc_sync;

	return 0;
}

static int
__osif_psoc_sync_start_wait_callback(struct device *dev,
				     struct osif_psoc_sync **out_psoc_sync,
				     const char *desc,
				     psoc_start_func psoc_start_cb)
{
	QDF_STATUS status;
	struct osif_psoc_sync *psoc_sync;

	*out_psoc_sync = NULL;

	osif_psoc_sync_lock();
	psoc_sync = osif_psoc_sync_lookup(dev);
	osif_psoc_sync_unlock();
	if (!psoc_sync)
		return -EAGAIN;

	status = psoc_start_cb(psoc_sync->dsc_psoc, desc);
	if (QDF_IS_STATUS_ERROR(status))
		return qdf_status_to_os_return(status);

	*out_psoc_sync = psoc_sync;

	return 0;
}

int __osif_psoc_sync_trans_start(struct device *dev,
				 struct osif_psoc_sync **out_psoc_sync,
				 const char *desc)
{
	int errno;

	osif_psoc_sync_lock();
	errno = __osif_psoc_sync_start_callback(dev, out_psoc_sync, desc,
						dsc_psoc_trans_start);
	osif_psoc_sync_unlock();

	return errno;
}

int __osif_psoc_sync_trans_start_wait(struct device *dev,
				      struct osif_psoc_sync **out_psoc_sync,
				      const char *desc)
{
	int errno;

	/* since dsc_psoc_trans_start_wait may sleep do not take lock here */
	errno = __osif_psoc_sync_start_wait_callback(dev, out_psoc_sync, desc,
						     dsc_psoc_trans_start_wait);

	return errno;
}

static QDF_STATUS __assert_trans_cb(struct dsc_psoc *dsc_psoc, const char *desc)
{
	dsc_psoc_assert_trans_protected(dsc_psoc);

	return QDF_STATUS_SUCCESS;
}

int osif_psoc_sync_trans_resume(struct device *dev,
				struct osif_psoc_sync **out_psoc_sync)
{
	int errno;

	osif_psoc_sync_lock();
	errno = __osif_psoc_sync_start_callback(dev, out_psoc_sync, NULL,
						__assert_trans_cb);
	osif_psoc_sync_unlock();

	return errno;
}

void osif_psoc_sync_trans_stop(struct osif_psoc_sync *psoc_sync)
{
	dsc_psoc_trans_stop(psoc_sync->dsc_psoc);
}

void osif_psoc_sync_assert_trans_protected(struct device *dev)
{
	struct osif_psoc_sync *psoc_sync;

	osif_psoc_sync_lock();

	psoc_sync = osif_psoc_sync_lookup(dev);
	QDF_BUG(psoc_sync);
	if (psoc_sync)
		dsc_psoc_assert_trans_protected(psoc_sync->dsc_psoc);

	osif_psoc_sync_unlock();
}

int __osif_psoc_sync_op_start(struct device *dev,
			      struct osif_psoc_sync **out_psoc_sync,
			      const char *func)
{
	int errno;

	osif_psoc_sync_lock();
	errno = __osif_psoc_sync_start_callback(dev, out_psoc_sync, func,
						_dsc_psoc_op_start);
	osif_psoc_sync_unlock();

	return errno;
}

void __osif_psoc_sync_op_stop(struct osif_psoc_sync *psoc_sync,
			      const char *func)
{
	_dsc_psoc_op_stop(psoc_sync->dsc_psoc, func);
}

void osif_psoc_sync_wait_for_ops(struct osif_psoc_sync *psoc_sync)
{
	dsc_psoc_wait_for_ops(psoc_sync->dsc_psoc);
}

void osif_psoc_sync_init(void)
{
	osif_psoc_sync_lock_create();
}

void osif_psoc_sync_deinit(void)
{
	osif_psoc_sync_lock_destroy();
}

static QDF_STATUS
__osif_psoc_sync_dsc_vdev_create(struct device *dev,
				 struct dsc_vdev **out_dsc_vdev)
{
	struct osif_psoc_sync *psoc_sync;

	osif_psoc_sync_lock_assert();

	psoc_sync = osif_psoc_sync_lookup(dev);
	if (!psoc_sync)
		return QDF_STATUS_E_INVAL;

	return dsc_vdev_create(psoc_sync->dsc_psoc, out_dsc_vdev);
}

QDF_STATUS osif_psoc_sync_dsc_vdev_create(struct device *dev,
					  struct dsc_vdev **out_dsc_vdev)
{
	QDF_STATUS status;

	osif_psoc_sync_lock();
	status = __osif_psoc_sync_dsc_vdev_create(dev, out_dsc_vdev);
	osif_psoc_sync_unlock();

	return status;
}

