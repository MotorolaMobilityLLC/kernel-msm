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

/**
 * DOC: Driver Synchronization Core (DSC) psoc-level APIs
 */

#ifndef __WLAN_DSC_PSOC_H
#define __WLAN_DSC_PSOC_H

#include "qdf_status.h"
#include "wlan_dsc_driver.h"

/**
 * struct dsc_psoc - opaque dsc psoc context
 */
struct dsc_psoc;

/**
 * dsc_psoc_create() - create a dsc psoc context
 * @driver: parent dsc driver context
 * @out_psoc: opaque double pointer to assign the new context to
 *
 * Note: this attaches @out_psoc to @driver
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
dsc_psoc_create(struct dsc_driver *driver, struct dsc_psoc **out_psoc);

/**
 * dsc_psoc_destroy() - destroy a dsc psoc context
 * @out_psoc: opaque double pointer to context to destroy and NULL
 *
 * Note, this:
 *	- detaches @out_psoc from its parent driver context
 *	- aborts all queued transitions on @psoc
 *	- asserts @psoc has no attached vdev's
 *	- asserts @psoc has no operations in flight
 *
 * Return: None
 */
void dsc_psoc_destroy(struct dsc_psoc **out_psoc);

/**
 * dsc_psoc_trans_start() - start a transition on @psoc
 * @psoc: the psoc to start a transition on
 * @desc: a unique description of the transition to start
 *
 * This API immediately aborts if a transition on @psoc is already in flight
 *
 * Call dsc_psoc_trans_stop() to complete the transition.
 *
 * Return:
 *	QDF_STATUS_SUCCESS - transition started succcessfully
 *	QDF_STATUS_E_INVAL - invalid request (causes debug panic)
 *	QDF_STATUS_E_AGAIN - transition cannot currently be started
 *	QDF_STATUS_E_ALREADY - transition with @desc already in flight
 */
QDF_STATUS dsc_psoc_trans_start(struct dsc_psoc *psoc, const char *desc);

/**
 * dsc_psoc_trans_start_wait() - start a transition on @psoc, blocking if a
 *	transition is already in flight
 * @psoc: the psoc to start a transition on
 * @desc: a unique description of the transition to start
 *
 * Call dsc_psoc_trans_stop() to complete the transition.
 *
 * Return:
 *	QDF_STATUS_SUCCESS - transition started succcessfully
 *	QDF_STATUS_E_INVAL - invalid request (causes debug panic)
 *	QDF_STATUS_E_AGAIN - transition cannot currently be started
 *	QDF_STATUS_E_ALREADY - transition with @desc already queued or in flight
 *	QDF_STATUS_E_ABORTED - transition was aborted
 */
QDF_STATUS dsc_psoc_trans_start_wait(struct dsc_psoc *psoc, const char *desc);

/**
 * dsc_psoc_trans_stop() - complete current transition in flight on @psoc
 * @psoc: the psoc to complete the transition on
 *
 * Note: this asserts a transition is currently in flight on @psoc
 *
 * Return: None
 */
void dsc_psoc_trans_stop(struct dsc_psoc *psoc);

/**
 * dsc_psoc_assert_trans_protected() - assert @psoc is protected by a transition
 * @psoc: the psoc to check
 *
 * The protecting transition may be in flight on @psoc or its parent.
 *
 * Return: None
 */
void dsc_psoc_assert_trans_protected(struct dsc_psoc *psoc);

/**
 * dsc_psoc_op_start() - start an operation on @psoc
 * @psoc: the psoc to start an operation on
 *
 * Return:
 *	QDF_STATUS_SUCCESS - operation started succcessfully
 *	QDF_STATUS_E_INVAL - invalid request (causes debug panic)
 *	QDF_STATUS_E_AGAIN - operation cannot currently be started
 *	QDF_STATUS_E_NOMEM - out of memory
 */
#define dsc_psoc_op_start(psoc) _dsc_psoc_op_start(psoc, __func__)
QDF_STATUS _dsc_psoc_op_start(struct dsc_psoc *psoc, const char *func);

/**
 * dsc_psoc_op_stop() - complete operation with matching @func on @psoc
 * @psoc: the psoc to stop an operation on
 *
 * Note: this asserts @func was previously started
 *
 * Return: None
 */
#define dsc_psoc_op_stop(psoc) _dsc_psoc_op_stop(psoc, __func__)
void _dsc_psoc_op_stop(struct dsc_psoc *psoc, const char *func);

/**
 * dsc_psoc_wait_for_ops() - blocks until all operations on @psoc have stopped
 * @psoc: the psoc to wait for operations on
 *
 * Note: this asserts that @psoc cannot currently transition
 *
 * Return: None
 */
void dsc_psoc_wait_for_ops(struct dsc_psoc *psoc);

#endif /* __WLAN_DSC_PSOC_H */
