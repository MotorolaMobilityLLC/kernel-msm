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

/**
 * DOC: Driver Synchronization Core (DSC) vdev-level APIs
 */

#ifndef __WLAN_DSC_VDEV_H
#define __WLAN_DSC_VDEV_H

#include "qdf_status.h"
#include "wlan_dsc_psoc.h"

/**
 * struct dsc_vdev - opaque dsc vdev context
 */
struct dsc_vdev;

/**
 * dsc_vdev_create() - create a dsc vdev context
 * @psoc: parent dsc psoc context
 * @out_vdev: opaque double pointer to assign the new context to
 *
 * Note: this attaches @out_vdev to @psoc
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dsc_vdev_create(struct dsc_psoc *psoc, struct dsc_vdev **out_vdev);

/**
 * dsc_vdev_destroy() - destroy a dsc vdev context
 * @out_vdev: opaque double pointer to context to destroy and NULL
 *
 * Note, this:
 *	- detaches @out_vdev from its parent psoc context
 *	- aborts all queued transitions on @vdev
 *	- asserts @vdev has no operations in flight
 *
 * Return: None
 */
void dsc_vdev_destroy(struct dsc_vdev **out_vdev);

/**
 * dsc_vdev_trans_start() - start a transition on @vdev
 * @vdev: the vdev to start a transition on
 * @desc: a unique description of the transition to start
 *
 * This API immediately aborts if a transition on @vdev is already in flight
 *
 * Call dsc_vdev_trans_stop() to complete the transition.
 *
 * Return:
 *	QDF_STATUS_SUCCESS - transition started succcessfully
 *	QDF_STATUS_E_INVAL - invalid request (causes debug panic)
 *	QDF_STATUS_E_AGAIN - transition cannot currently be started
 *	QDF_STATUS_E_ALREADY - transition with @desc already in flight
 */
QDF_STATUS dsc_vdev_trans_start(struct dsc_vdev *vdev, const char *desc);

/**
 * dsc_vdev_trans_start_wait() - start a transition on @vdev, blocking if a
 *	transition is already in flight
 * @vdev: the vdev to start a transition on
 * @desc: a unique description of the transition to start
 *
 * Call dsc_vdev_trans_stop() to complete the transition.
 *
 * Return:
 *	QDF_STATUS_SUCCESS - transition started succcessfully
 *	QDF_STATUS_E_INVAL - invalid request (causes debug panic)
 *	QDF_STATUS_E_AGAIN - transition cannot currently be started
 *	QDF_STATUS_E_ALREADY - transition with @desc already queued or in flight
 *	QDF_STATUS_E_ABORTED - transition was aborted
 */
QDF_STATUS dsc_vdev_trans_start_wait(struct dsc_vdev *vdev, const char *desc);

/**
 * dsc_vdev_trans_stop() - complete current transition in flight on @vdev
 * @vdev: the vdev to complete the transition on
 *
 * Note: this asserts a transition is currently in flight on @vdev
 *
 * Return: None
 */
void dsc_vdev_trans_stop(struct dsc_vdev *vdev);

/**
 * dsc_vdev_assert_trans_protected() - assert @vdev is protected by a transition
 * @vdev: the vdev to check
 *
 * The protecting transition may be in flight on @vdev or its ancestors.
 *
 * Return: None
 */
void dsc_vdev_assert_trans_protected(struct dsc_vdev *vdev);

/**
 * dsc_vdev_op_start() - start an operation on @vdev
 * @vdev: the vdev to start an operation on
 *
 * Return:
 *	QDF_STATUS_SUCCESS - operation started succcessfully
 *	QDF_STATUS_E_INVAL - invalid request (causes debug panic)
 *	QDF_STATUS_E_AGAIN - operation cannot currently be started
 *	QDF_STATUS_E_NOMEM - out of memory
 */
#define dsc_vdev_op_start(vdev) _dsc_vdev_op_start(vdev, __func__)
QDF_STATUS _dsc_vdev_op_start(struct dsc_vdev *vdev, const char *func);

/**
 * dsc_vdev_op_stop() - complete operation with matching @func on @vdev
 * @vdev: the vdev to stop an operation on
 *
 * Note: this asserts @func was previously started
 *
 * Return: None
 */
#define dsc_vdev_op_stop(vdev) _dsc_vdev_op_stop(vdev, __func__)
void _dsc_vdev_op_stop(struct dsc_vdev *vdev, const char *func);

/**
 * dsc_vdev_wait_for_ops() - blocks until all operations on @vdev have stopped
 * @vdev: the vdev to wait for operations on
 *
 * Note: this asserts that @vdev cannot currently transition
 *
 * Return: None
 */
void dsc_vdev_wait_for_ops(struct dsc_vdev *vdev);

/**
 * dsc_vdev_get_cached_cmd() - Get north bound cmd cached during SSR
 * @vdev: Pointer to the dsc vdev
 *
 * This api will be invoked after completion of SSR re-initialization to get
 * the last north bound command received during SSR
 *
 * Return: North bound command ID
 */
uint8_t dsc_vdev_get_cached_cmd(struct dsc_vdev *vdev);

/**
 * dsc_vdev_cache_command() - Cache north bound command during SSR
 * @vdev: Pointer to the dsc vdev corresponding to the network interface
 * @cmd_id: North bound command ID
 *
 * This api will be invoked when a north bound command is received during SSR
 * and it should be handled after SSR re-initialization.
 *
 * Return: None
 */
void dsc_vdev_cache_command(struct dsc_vdev *vdev, uint8_t cmd_id);

#endif /* __WLAN_DSC_VDEV_H */
