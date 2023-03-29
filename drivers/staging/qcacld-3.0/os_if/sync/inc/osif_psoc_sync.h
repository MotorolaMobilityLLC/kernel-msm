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

#ifndef __OSIF_PSOC_SYNC_H
#define __OSIF_PSOC_SYNC_H

#include "linux/device.h"
#include "wlan_dsc_driver.h"
#include "qdf_types.h"

/**
 * struct osif_psoc_sync - opaque synchronization handle for a psoc
 */
struct osif_psoc_sync;

/**
 * osif_psoc_sync_create() - create a psoc synchronization context
 * @out_psoc_sync: out parameter for the new synchronization context
 *
 * Return: Errno
 */
qdf_must_check int
osif_psoc_sync_create(struct osif_psoc_sync **out_psoc_sync);

/**
 * osif_psoc_sync_create_and_trans() - create a psoc synchronization context
 * @out_psoc_sync: out parameter for the new synchronization context
 *
 * For protecting the device creation process.
 *
 * Return: Errno
 */
#define osif_psoc_sync_create_and_trans(out_psoc_sync) \
	__osif_psoc_sync_create_and_trans(out_psoc_sync, __func__)

qdf_must_check int
__osif_psoc_sync_create_and_trans(struct osif_psoc_sync **out_psoc_sync,
				  const char *desc);

/**
 * osif_psoc_sync_destroy() - destroy a psoc synchronization context
 * @psoc_sync: the context to destroy
 *
 * Return: none
 */
void osif_psoc_sync_destroy(struct osif_psoc_sync *psoc_sync);

/**
 * osif_psoc_sync_register() - register a psoc for operations/transitions
 * @dev: the device to use as the operation/transition lookup key
 * @psoc_sync: the psoc synchronization context to register
 *
 * Return: none
 */
void osif_psoc_sync_register(struct device *dev,
			     struct osif_psoc_sync *psoc_sync);

/**
 * osif_psoc_sync_unregister() - unregister a psoc for operations/transitions
 * @dev: the device originally used to register the psoc_sync context
 *
 * Return: the psoc synchronization context that was registered for @dev
 */
struct osif_psoc_sync *osif_psoc_sync_unregister(struct device *dev);

/**
 * osif_psoc_sync_trans_start() - attempt to start a transition on @dev
 * @dev: the device to transition
 * @out_psoc_sync: out parameter for the synchronization context registered with
 *	@dev, populated on success
 *
 * Return: Errno
 */
#define osif_psoc_sync_trans_start(dev, out_psoc_sync) \
	__osif_psoc_sync_trans_start(dev, out_psoc_sync, __func__)

qdf_must_check int
__osif_psoc_sync_trans_start(struct device *dev,
			     struct osif_psoc_sync **out_psoc_sync,
			     const char *desc);

/**
 * osif_psoc_sync_trans_start_wait() - attempt to start a transition on @dev,
 *	blocking if a conflicting transition is in flight
 * @dev: the device to transition
 * @out_psoc_sync: out parameter for the synchronization context registered with
 *	@dev, populated on success
 *
 * Return: Errno
 */
#define osif_psoc_sync_trans_start_wait(dev, out_psoc_sync) \
	__osif_psoc_sync_trans_start_wait(dev, out_psoc_sync, __func__)

qdf_must_check int
__osif_psoc_sync_trans_start_wait(struct device *dev,
				  struct osif_psoc_sync **out_psoc_sync,
				  const char *desc);

/**
 * osif_psoc_sync_trans_resume() - resume a transition on @dev
 * @dev: the device under transition
 * @out_psoc_sync: out parameter for the synchronization context registered with
 *	@dev, populated on success
 *
 * Return: Errno
 */
int osif_psoc_sync_trans_resume(struct device *dev,
				struct osif_psoc_sync **out_psoc_sync);

/**
 * osif_psoc_sync_trans_stop() - stop a transition associated with @psoc_sync
 * @psoc_sync: the synchonization context tracking the transition
 *
 * Return: none
 */
void osif_psoc_sync_trans_stop(struct osif_psoc_sync *psoc_sync);

/**
 * osif_psoc_sync_assert_trans_protected() - assert that @dev is currently
 *	protected by a transition
 * @dev: the device to check
 *
 * Return: none
 */
void osif_psoc_sync_assert_trans_protected(struct device *dev);

/**
 * osif_psoc_sync_op_start() - attempt to start an operation on @dev
 * @dev: the device to operate against
 * @out_psoc_sync: out parameter for the synchronization context registered with
 *	@dev, populated on success
 *
 * Return: Errno
 */
#define osif_psoc_sync_op_start(dev, out_psoc_sync) \
	__osif_psoc_sync_op_start(dev, out_psoc_sync, __func__)

qdf_must_check int
__osif_psoc_sync_op_start(struct device *dev,
			  struct osif_psoc_sync **out_psoc_sync,
			  const char *func);

/**
 * osif_psoc_sync_op_stop() - stop an operation associated with @psoc_sync
 * @psoc_sync: the synchonization context tracking the operation
 *
 * Return: none
 */
#define osif_psoc_sync_op_stop(psoc_sync) \
	__osif_psoc_sync_op_stop(psoc_sync, __func__)

void __osif_psoc_sync_op_stop(struct osif_psoc_sync *psoc_sync,
			      const char *func);

/**
 * osif_psoc_sync_wait_for_ops() - wait until all @psoc_sync operations complete
 * @psoc_sync: the synchonization context tracking the operations
 *
 * Return: None
 */
void osif_psoc_sync_wait_for_ops(struct osif_psoc_sync *psoc_sync);

#endif /* __OSIF_PSOC_SYNC_H */

