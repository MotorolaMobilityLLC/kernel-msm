/*
 * Copyright (c) 2018-2019, 2021 The Linux Foundation. All rights reserved.
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

#ifndef __OSIF_VDEV_SYNC_H
#define __OSIF_VDEV_SYNC_H

#include "linux/device.h"
#include "linux/netdevice.h"
#include "qdf_types.h"

/**
 * struct osif_vdev_sync - vdev synchronization context
 * @net_dev: the net_device used as a lookup key
 * @dsc_vdev: the dsc_vdev used for synchronization
 * @in_use: indicates if the context is being used
 */
struct osif_vdev_sync {
	struct net_device *net_dev;
	struct dsc_vdev *dsc_vdev;
	bool in_use;
};

/**
 * osif_vdev_sync_create() - create a vdev synchronization context
 * @dev: parent device to the vdev
 * @out_vdev_sync: out parameter for the new synchronization context
 *
 * Return: Errno
 */
qdf_must_check int
osif_vdev_sync_create(struct device *dev,
		      struct osif_vdev_sync **out_vdev_sync);

/**
 * osif_vdev_sync_create_and_trans() - create a vdev synchronization context
 * @dev: parent device to the vdev
 * @out_vdev_sync: out parameter for the new synchronization context
 *
 * For protecting the net_device creation process.
 *
 * Return: Errno
 */
#define osif_vdev_sync_create_and_trans(dev, out_vdev_sync) \
	__osif_vdev_sync_create_and_trans(dev, out_vdev_sync, __func__)

qdf_must_check int
__osif_vdev_sync_create_and_trans(struct device *dev,
				  struct osif_vdev_sync **out_vdev_sync,
				  const char *desc);

/**
 * osif_vdev_sync_destroy() - destroy a vdev synchronization context
 * @vdev_sync: the context to destroy
 *
 * Return: none
 */
void osif_vdev_sync_destroy(struct osif_vdev_sync *vdev_sync);

/**
 * osif_vdev_sync_register() - register a vdev for operations/transitions
 * @net_dev: the net_device to use as the operation/transition lookup key
 * @vdev_sync: the vdev synchronization context to register
 *
 * Return: none
 */
void osif_vdev_sync_register(struct net_device *net_dev,
			     struct osif_vdev_sync *vdev_sync);

/**
 * osif_vdev_sync_unregister() - unregister a vdev for operations/transitions
 * @net_dev: the net_device originally used to register the vdev_sync context
 *
 * Return: the vdev synchronization context that was registered for @net_dev
 */
struct osif_vdev_sync *osif_vdev_sync_unregister(struct net_device *net_dev);

/**
 * osif_vdev_sync_trans_start() - attempt to start a transition on @net_dev
 * @net_dev: the net_device to transition
 * @out_vdev_sync: out parameter for the synchronization context registered with
 *	@net_dev, populated on success
 *
 * Return: Errno
 */
#define osif_vdev_sync_trans_start(net_dev, out_vdev_sync) \
	__osif_vdev_sync_trans_start(net_dev, out_vdev_sync, __func__)

qdf_must_check int
__osif_vdev_sync_trans_start(struct net_device *net_dev,
			     struct osif_vdev_sync **out_vdev_sync,
			     const char *desc);

/**
 * osif_vdev_sync_trans_start_wait() - attempt to start a transition on
 *	@net_dev, blocking if a conflicting transition is in flight
 * @net_dev: the net_device to transition
 * @out_vdev_sync: out parameter for the synchronization context registered with
 *	@net_dev, populated on success
 *
 * Return: Errno
 */
#define osif_vdev_sync_trans_start_wait(net_dev, out_vdev_sync) \
	__osif_vdev_sync_trans_start_wait(net_dev, out_vdev_sync, __func__)

qdf_must_check int
__osif_vdev_sync_trans_start_wait(struct net_device *net_dev,
				  struct osif_vdev_sync **out_vdev_sync,
				  const char *desc);

/**
 * osif_vdev_sync_trans_stop() - stop a transition associated with @vdev_sync
 * @vdev_sync: the synchonization context tracking the transition
 *
 * Return: none
 */
void osif_vdev_sync_trans_stop(struct osif_vdev_sync *vdev_sync);

/**
 * osif_vdev_sync_assert_trans_protected() - assert that @net_dev is currently
 *	protected by a transition
 * @net_dev: the net_device to check
 *
 * Return: none
 */
void osif_vdev_sync_assert_trans_protected(struct net_device *net_dev);

/**
 * osif_vdev_sync_op_start() - attempt to start an operation on @net_dev
 * @net_dev: the net_device to operate against
 * @out_vdev_sync: out parameter for the synchronization context registered with
 *	@net_dev, populated on success
 *
 * Return: Errno
 */
#define osif_vdev_sync_op_start(net_dev, out_vdev_sync) \
	__osif_vdev_sync_op_start(net_dev, out_vdev_sync, __func__)

qdf_must_check int
__osif_vdev_sync_op_start(struct net_device *net_dev,
			  struct osif_vdev_sync **out_vdev_sync,
			  const char *func);

/**
 * osif_vdev_sync_op_stop() - stop an operation associated with @vdev_sync
 * @vdev_sync: the synchonization context tracking the operation
 *
 * Return: none
 */
#define osif_vdev_sync_op_stop(net_dev) \
	__osif_vdev_sync_op_stop(net_dev, __func__)

void __osif_vdev_sync_op_stop(struct osif_vdev_sync *vdev_sync,
			      const char *func);

/**
 * osif_vdev_sync_wait_for_ops() - wait until all @vdev_sync operations complete
 * @vdev_sync: the synchonization context tracking the operations
 *
 * Return: None
 */
void osif_vdev_sync_wait_for_ops(struct osif_vdev_sync *vdev_sync);

/**
 * osif_vdev_get_cached_cmd() - Get north bound cmd cached during SSR
 * @vdev_sync: osif vdev sync corresponding to the network interface
 *
 * This api will be invoked after completion of SSR re-initialization to get
 * the last north bound command received during SSR
 *
 * Return: North bound command ID
 */
uint8_t osif_vdev_get_cached_cmd(struct osif_vdev_sync *vdev_sync);

/**
 * osif_vdev_cache_command() - Cache north bound command during SSR
 * @vdev_sync: osif vdev sync corresponding to the network interface
 * @cmd_id: North bound command ID
 *
 * This api will be invoked when a north bound command is received during SSR
 * and it should be handled after SSR re-initialization.
 *
 * Return: None
 */
void osif_vdev_cache_command(struct osif_vdev_sync *vdev_sync, uint8_t cmd_id);

/**
 * osif_get_vdev_sync_arr() - Get vdev sync array base pointer
 *
 * Return: Base pointer to the vdev sync array
 */
struct osif_vdev_sync *osif_get_vdev_sync_arr(void);

#endif /* __OSIF_VDEV_SYNC_H */

