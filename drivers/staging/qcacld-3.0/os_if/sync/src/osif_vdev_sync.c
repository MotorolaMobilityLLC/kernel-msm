/*
 * Copyright (c) 2018-2019 The Linux Foundation. All rights reserved.
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
#include "linux/netdevice.h"
#include "__osif_psoc_sync.h"
#include "__osif_vdev_sync.h"
#include "osif_vdev_sync.h"
#include "qdf_lock.h"
#include "qdf_status.h"
#include "qdf_types.h"

static struct osif_vdev_sync __osif_vdev_sync_arr[WLAN_MAX_VDEVS];
static qdf_spinlock_t __osif_vdev_sync_lock;

#define osif_vdev_sync_lock_create() qdf_spinlock_create(&__osif_vdev_sync_lock)
#define osif_vdev_sync_lock_destroy() \
	qdf_spinlock_destroy(&__osif_vdev_sync_lock)
#define osif_vdev_sync_lock() qdf_spin_lock_bh(&__osif_vdev_sync_lock)
#define osif_vdev_sync_unlock() qdf_spin_unlock_bh(&__osif_vdev_sync_lock)
#define osif_vdev_sync_lock_assert() \
	QDF_BUG(qdf_spin_is_locked(&__osif_vdev_sync_lock))

static struct osif_vdev_sync *osif_vdev_sync_lookup(struct net_device *net_dev)
{
	int i;

	osif_vdev_sync_lock_assert();

	for (i = 0; i < QDF_ARRAY_SIZE(__osif_vdev_sync_arr); i++) {
		struct osif_vdev_sync *vdev_sync = __osif_vdev_sync_arr + i;

		if (!vdev_sync->in_use)
			continue;

		if (vdev_sync->net_dev == net_dev)
			return vdev_sync;
	}

	return NULL;
}

struct osif_vdev_sync *osif_get_vdev_sync_arr(void)
{
	return __osif_vdev_sync_arr;
}

static struct osif_vdev_sync *osif_vdev_sync_get(void)
{
	int i;

	osif_vdev_sync_lock_assert();

	for (i = 0; i < QDF_ARRAY_SIZE(__osif_vdev_sync_arr); i++) {
		struct osif_vdev_sync *vdev_sync = __osif_vdev_sync_arr + i;

		if (!vdev_sync->in_use) {
			vdev_sync->in_use = true;
			return vdev_sync;
		}
	}

	return NULL;
}

static void osif_vdev_sync_put(struct osif_vdev_sync *vdev_sync)
{
	osif_vdev_sync_lock_assert();

	qdf_mem_zero(vdev_sync, sizeof(*vdev_sync));
}

int osif_vdev_sync_create(struct device *dev,
			  struct osif_vdev_sync **out_vdev_sync)
{
	struct osif_vdev_sync *vdev_sync;
	QDF_STATUS status;

	QDF_BUG(dev);
	if (!dev)
		return -EINVAL;

	QDF_BUG(out_vdev_sync);
	if (!out_vdev_sync)
		return -EINVAL;

	osif_vdev_sync_lock();
	vdev_sync = osif_vdev_sync_get();
	osif_vdev_sync_unlock();
	if (!vdev_sync)
		return -ENOMEM;

	status = osif_psoc_sync_dsc_vdev_create(dev, &vdev_sync->dsc_vdev);
	if (QDF_IS_STATUS_ERROR(status))
		goto sync_put;

	*out_vdev_sync = vdev_sync;

	return 0;

sync_put:
	osif_vdev_sync_lock();
	osif_vdev_sync_put(vdev_sync);
	osif_vdev_sync_unlock();

	return qdf_status_to_os_return(status);
}

int __osif_vdev_sync_create_and_trans(struct device *dev,
				      struct osif_vdev_sync **out_vdev_sync,
				      const char *desc)
{
	struct osif_vdev_sync *vdev_sync;
	QDF_STATUS status;
	int errno;

	errno = osif_vdev_sync_create(dev, &vdev_sync);
	if (errno)
		return errno;

	status = dsc_vdev_trans_start(vdev_sync->dsc_vdev, desc);
	if (QDF_IS_STATUS_ERROR(status))
		goto sync_destroy;

	*out_vdev_sync = vdev_sync;

	return 0;

sync_destroy:
	osif_vdev_sync_destroy(vdev_sync);

	return qdf_status_to_os_return(status);
}

void osif_vdev_sync_destroy(struct osif_vdev_sync *vdev_sync)
{
	QDF_BUG(vdev_sync);
	if (!vdev_sync)
		return;

	dsc_vdev_destroy(&vdev_sync->dsc_vdev);

	osif_vdev_sync_lock();
	osif_vdev_sync_put(vdev_sync);
	osif_vdev_sync_unlock();
}

void osif_vdev_sync_register(struct net_device *net_dev,
			     struct osif_vdev_sync *vdev_sync)
{
	QDF_BUG(net_dev);
	QDF_BUG(vdev_sync);
	if (!vdev_sync)
		return;

	osif_vdev_sync_lock();
	vdev_sync->net_dev = net_dev;
	osif_vdev_sync_unlock();
}

struct osif_vdev_sync *osif_vdev_sync_unregister(struct net_device *net_dev)
{
	struct osif_vdev_sync *vdev_sync;

	QDF_BUG(net_dev);
	if (!net_dev)
		return NULL;

	osif_vdev_sync_lock();
	vdev_sync = osif_vdev_sync_lookup(net_dev);
	if (vdev_sync)
		vdev_sync->net_dev = NULL;
	osif_vdev_sync_unlock();

	return vdev_sync;
}

typedef QDF_STATUS (*vdev_start_func)(struct dsc_vdev *, const char *);

static int
__osif_vdev_sync_start_callback(struct net_device *net_dev,
				struct osif_vdev_sync **out_vdev_sync,
				const char *desc,
				vdev_start_func vdev_start_cb)
{
	QDF_STATUS status;
	struct osif_vdev_sync *vdev_sync;

	osif_vdev_sync_lock_assert();

	*out_vdev_sync = NULL;

	vdev_sync = osif_vdev_sync_lookup(net_dev);
	if (!vdev_sync)
		return -EAGAIN;

	*out_vdev_sync = vdev_sync;

	status = vdev_start_cb(vdev_sync->dsc_vdev, desc);
	if (QDF_IS_STATUS_ERROR(status))
		return qdf_status_to_os_return(status);

	return 0;
}

static int
__osif_vdev_sync_start_wait_callback(struct net_device *net_dev,
				     struct osif_vdev_sync **out_vdev_sync,
				     const char *desc,
				     vdev_start_func vdev_start_cb)
{
	QDF_STATUS status;
	struct osif_vdev_sync *vdev_sync;

	*out_vdev_sync = NULL;

	osif_vdev_sync_lock();
	vdev_sync = osif_vdev_sync_lookup(net_dev);
	osif_vdev_sync_unlock();
	if (!vdev_sync)
		return -EAGAIN;

	status = vdev_start_cb(vdev_sync->dsc_vdev, desc);
	if (QDF_IS_STATUS_ERROR(status))
		return qdf_status_to_os_return(status);

	*out_vdev_sync = vdev_sync;

	return 0;
}

int __osif_vdev_sync_trans_start(struct net_device *net_dev,
				 struct osif_vdev_sync **out_vdev_sync,
				 const char *desc)
{
	int errno;

	osif_vdev_sync_lock();
	errno = __osif_vdev_sync_start_callback(net_dev, out_vdev_sync, desc,
						dsc_vdev_trans_start);
	osif_vdev_sync_unlock();

	return errno;
}

int __osif_vdev_sync_trans_start_wait(struct net_device *net_dev,
				      struct osif_vdev_sync **out_vdev_sync,
				      const char *desc)
{
	int errno;

	/* since dsc_vdev_trans_start_wait may sleep do not take lock here */
	errno = __osif_vdev_sync_start_wait_callback(net_dev,
						     out_vdev_sync, desc,
						     dsc_vdev_trans_start_wait);

	return errno;
}

void osif_vdev_sync_trans_stop(struct osif_vdev_sync *vdev_sync)
{
	dsc_vdev_trans_stop(vdev_sync->dsc_vdev);
}

void osif_vdev_sync_assert_trans_protected(struct net_device *net_dev)
{
	struct osif_vdev_sync *vdev_sync;

	osif_vdev_sync_lock();

	vdev_sync = osif_vdev_sync_lookup(net_dev);
	QDF_BUG(vdev_sync);
	if (vdev_sync)
		dsc_vdev_assert_trans_protected(vdev_sync->dsc_vdev);

	osif_vdev_sync_unlock();
}

int __osif_vdev_sync_op_start(struct net_device *net_dev,
			      struct osif_vdev_sync **out_vdev_sync,
			      const char *func)
{
	int errno;

	osif_vdev_sync_lock();
	errno = __osif_vdev_sync_start_callback(net_dev, out_vdev_sync, func,
						_dsc_vdev_op_start);
	osif_vdev_sync_unlock();

	return errno;
}

void __osif_vdev_sync_op_stop(struct osif_vdev_sync *vdev_sync,
			      const char *func)
{
	_dsc_vdev_op_stop(vdev_sync->dsc_vdev, func);
}

void osif_vdev_sync_wait_for_ops(struct osif_vdev_sync *vdev_sync)
{
	dsc_vdev_wait_for_ops(vdev_sync->dsc_vdev);
}

void osif_vdev_sync_init(void)
{
	osif_vdev_sync_lock_create();
}

void osif_vdev_sync_deinit(void)
{
	osif_vdev_sync_lock_destroy();
}

uint8_t osif_vdev_get_cached_cmd(struct osif_vdev_sync *vdev_sync)
{
	return dsc_vdev_get_cached_cmd(vdev_sync->dsc_vdev);
}

void osif_vdev_cache_command(struct osif_vdev_sync *vdev_sync, uint8_t cmd_id)
{
	dsc_vdev_cache_command(vdev_sync->dsc_vdev, cmd_id);
}

