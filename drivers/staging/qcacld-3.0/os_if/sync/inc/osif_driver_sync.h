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

#ifndef __OSIF_DRIVER_SYNC_H
#define __OSIF_DRIVER_SYNC_H

#include "qdf_types.h"

/**
 * struct osif_driver_sync - opaque synchronization handle for a driver
 */
struct osif_driver_sync;

/**
 * osif_driver_sync_create() - create a driver synchronization context
 * @out_driver_sync: out parameter for the new synchronization context
 *
 * Return: Errno
 */
qdf_must_check int
osif_driver_sync_create(struct osif_driver_sync **out_driver_sync);

/**
 * osif_driver_sync_create_and_trans() - create a driver synchronization context
 * @out_driver_sync: out parameter for the new synchronization context
 *
 * For protecting the driver initialization process.
 *
 * Return: Errno
 */
#define osif_driver_sync_create_and_trans(out_driver_sync) \
	__osif_driver_sync_create_and_trans(out_driver_sync, __func__)

qdf_must_check int
__osif_driver_sync_create_and_trans(struct osif_driver_sync **out_driver_sync,
				    const char *desc);

/**
 * osif_driver_sync_destroy() - destroy a driver synchronization context
 * @driver_sync: the context to destroy
 *
 * Return: none
 */
void osif_driver_sync_destroy(struct osif_driver_sync *driver_sync);

/**
 * osif_driver_sync_register() - register a driver for operations/transitions
 * @driver_sync: the driver synchronization context to register
 *
 * Return: none
 */
void osif_driver_sync_register(struct osif_driver_sync *driver_sync);

/**
 * osif_driver_sync_unregister() - unregister a driver for
 *	operations/transitions
 *
 * Return: the driver synchronization context that was registered for @dev
 */
struct osif_driver_sync *osif_driver_sync_unregister(void);

/**
 * osif_driver_sync_trans_start() - attempt to start a driver transition
 * @out_driver_sync: out parameter for the synchronization context previously
 *	registered, populated on success
 *
 * Return: Errno
 */
#define osif_driver_sync_trans_start(out_driver_sync) \
	__osif_driver_sync_trans_start(out_driver_sync, __func__)

qdf_must_check int
__osif_driver_sync_trans_start(struct osif_driver_sync **out_driver_sync,
			       const char *desc);

/**
 * osif_driver_sync_trans_start_wait() - attempt to start a driver transition,
 *	blocking if a conflicting transition is in flight
 * @out_driver_sync: out parameter for the synchronization context previously
 *	registered, populated on success
 *
 * Return: Errno
 */
#define osif_driver_sync_trans_start_wait(out_driver_sync) \
	__osif_driver_sync_trans_start_wait(out_driver_sync, __func__)

qdf_must_check int
__osif_driver_sync_trans_start_wait(struct osif_driver_sync **out_driver_sync,
				    const char *desc);

/**
 * osif_driver_sync_trans_stop() - stop a transition associated with
 *	@driver_sync
 * @driver_sync: the synchonization context tracking the transition
 *
 * Return: none
 */
void osif_driver_sync_trans_stop(struct osif_driver_sync *driver_sync);

/**
 * osif_driver_sync_assert_trans_protected() - assert that the driver is
 *	currently protected by a transition
 *
 * Return: none
 */
void osif_driver_sync_assert_trans_protected(void);

/**
 * osif_driver_sync_op_start() - attempt to start a driver operation
 * @out_driver_sync: out parameter for the synchronization context previously
 *	registered, populated on success
 *
 * Return: Errno
 */
#define osif_driver_sync_op_start(out_driver_sync) \
	__osif_driver_sync_op_start(out_driver_sync, __func__)

qdf_must_check int
__osif_driver_sync_op_start(struct osif_driver_sync **out_driver_sync,
			    const char *func);

/**
 * osif_driver_sync_op_stop() - stop an operation associated with @driver_sync
 * @driver_sync: the synchonization context tracking the operation
 *
 * Return: none
 */
#define osif_driver_sync_op_stop(driver_sync) \
	__osif_driver_sync_op_stop(driver_sync, __func__)

void __osif_driver_sync_op_stop(struct osif_driver_sync *driver_sync,
				const char *func);

/**
 * osif_driver_sync_wait_for_ops() - wait until all @driver_sync operations
 *	complete
 * @driver_sync: the synchonization context tracking the operations
 *
 * Return: None
 */
void osif_driver_sync_wait_for_ops(struct osif_driver_sync *driver_sync);

#endif /* __OSIF_DRIVER_SYNC_H */

