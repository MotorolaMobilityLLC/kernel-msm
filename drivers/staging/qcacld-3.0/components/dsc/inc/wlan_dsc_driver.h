/*
 * Copyright (c) 2018 The Linux Foundation. All rights reserved.
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
 * DOC: Driver Synchronization Core (DSC) driver-level APIs
 */

#ifndef __WLAN_DSC_DRIVER_H
#define __WLAN_DSC_DRIVER_H

#include "qdf_status.h"

/**
 * struct dsc_driver - opaque dsc driver context
 */
struct dsc_driver;

/**
 * dsc_driver_create() - create a dsc driver context
 * @out_driver: opaque double pointer to assign the new context to
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dsc_driver_create(struct dsc_driver **out_driver);

/**
 * dsc_driver_destroy() - destroy a dsc driver context
 * @out_driver: opaque double pointer to context to destroy and NULL
 *
 * Note, this:
 *	- aborts all queued transitions on @driver
 *	- asserts @driver has no attached psoc's
 *	- asserts @driver has no operations in flight
 *
 * Return: None
 */
void dsc_driver_destroy(struct dsc_driver **out_driver);

/**
 * dsc_driver_trans_start() - start a transition on @driver
 * @driver: the driver to start a transition on
 * @desc: a unique description of the transition to start
 *
 * This API immediately aborts if a transition on @driver is already in flight
 *
 * Call dsc_driver_trans_stop() to complete the transition.
 *
 * Return:
 *	QDF_STATUS_SUCCESS - transition started succcessfully
 *	QDF_STATUS_E_INVAL - invalid request (causes debug panic)
 *	QDF_STATUS_E_AGAIN - transition cannot currently be started
 *	QDF_STATUS_E_ALREADY - transition with @desc already in flight
 */
QDF_STATUS dsc_driver_trans_start(struct dsc_driver *driver, const char *desc);

/**
 * dsc_driver_trans_start_wait() - start a transition on @driver, blocking if a
 *	transition is already in flight
 * @driver: the driver to start a transition on
 * @desc: a unique description of the transition to start
 *
 * Call dsc_driver_trans_stop() to complete the transition.
 *
 * Return:
 *	QDF_STATUS_SUCCESS - transition started succcessfully
 *	QDF_STATUS_E_INVAL - invalid request (causes debug panic)
 *	QDF_STATUS_E_AGAIN - transition cannot currently be started
 *	QDF_STATUS_E_ALREADY - transition with @desc already queued or in flight
 */
QDF_STATUS
dsc_driver_trans_start_wait(struct dsc_driver *driver, const char *desc);

/**
 * dsc_driver_trans_stop() - complete current transition in flight on @driver
 * @driver: the driver to complete the transition on
 *
 * Note: this asserts a transition is currently in flight on @driver
 *
 * Return: None
 */
void dsc_driver_trans_stop(struct dsc_driver *driver);

/**
 * dsc_driver_assert_trans_protected() - assert @driver is protected by a
 *	transition
 * @driver: the driver to check
 *
 * Return: None
 */
void dsc_driver_assert_trans_protected(struct dsc_driver *driver);

/**
 * dsc_driver_op_start() - start an operation on @driver
 * @driver: the driver to start an operation on
 *
 * Return:
 *	QDF_STATUS_SUCCESS - operation started succcessfully
 *	QDF_STATUS_E_INVAL - invalid request (causes debug panic)
 *	QDF_STATUS_E_AGAIN - operation cannot currently be started
 *	QDF_STATUS_E_NOMEM - out of memory
 */
#define dsc_driver_op_start(driver) _dsc_driver_op_start(driver, __func__)
QDF_STATUS _dsc_driver_op_start(struct dsc_driver *driver, const char *func);

/**
 * dsc_driver_op_stop() - complete operation with matching @func on @driver
 * @driver: the driver to stop an operation on
 *
 * Note: this asserts @func was previously started
 *
 * Return: None
 */
#define dsc_driver_op_stop(driver) _dsc_driver_op_stop(driver, __func__)
void _dsc_driver_op_stop(struct dsc_driver *driver, const char *func);

/**
 * dsc_driver_wait_for_ops() - blocks until all operations on @driver have
 *	stopped
 * @driver: the driver to wait for operations on
 *
 * Note: this asserts that @driver cannot currently transition
 *
 * Return: None
 */
void dsc_driver_wait_for_ops(struct dsc_driver *driver);

#endif /* __WLAN_DSC_DRIVER_H */
