/*
 * Copyright (c) 2018-2020 The Linux Foundation. All rights reserved.
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

#include "qdf_list.h"
#include "qdf_mem.h"
#include "qdf_status.h"
#include "qdf_types.h"
#include "__wlan_dsc.h"
#include "wlan_dsc.h"

void __dsc_lock(struct dsc_driver *driver)
{
	dsc_assert(driver);
	qdf_spin_lock_bh(&driver->lock);
}

void __dsc_unlock(struct dsc_driver *driver)
{
	dsc_assert(driver);
	qdf_spin_unlock_bh(&driver->lock);
}

static QDF_STATUS __dsc_driver_create(struct dsc_driver **out_driver)
{
	struct dsc_driver *driver;

	if (!dsc_assert(out_driver))
		return QDF_STATUS_E_INVAL;

	*out_driver = NULL;

	driver = qdf_mem_malloc(sizeof(*driver));
	if (!driver)
		return QDF_STATUS_E_NOMEM;

	qdf_spinlock_create(&driver->lock);
	qdf_list_create(&driver->psocs, 0);
	__dsc_trans_init(&driver->trans);
	__dsc_ops_init(&driver->ops);

	*out_driver = driver;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dsc_driver_create(struct dsc_driver **out_driver)
{
	QDF_STATUS status;

	status = __dsc_driver_create(out_driver);

	return status;
}

static void __dsc_driver_destroy(struct dsc_driver **out_driver)
{
	struct dsc_driver *driver;

	if (!dsc_assert(out_driver))
		return;

	driver = *out_driver;
	if (!dsc_assert(driver))
		return;

	*out_driver = NULL;

	/* assert no children */
	dsc_assert(qdf_list_empty(&driver->psocs));

	/* flush pending transitions */
	while (__dsc_trans_abort(&driver->trans))
		;

	/* de-init */
	__dsc_ops_deinit(&driver->ops);
	__dsc_trans_deinit(&driver->trans);
	qdf_list_destroy(&driver->psocs);
	qdf_spinlock_destroy(&driver->lock);

	qdf_mem_free(driver);
}

void dsc_driver_destroy(struct dsc_driver **out_driver)
{
	__dsc_driver_destroy(out_driver);
}

static bool __dsc_driver_trans_active_down_tree(struct dsc_driver *driver)
{
	struct dsc_psoc *psoc;
	struct dsc_vdev *vdev;

	dsc_for_each_driver_psoc(driver, psoc) {
		if (__dsc_trans_active(&psoc->trans))
			return true;

		dsc_for_each_psoc_vdev(psoc, vdev) {
			if (__dsc_trans_active(&vdev->trans))
				return true;
		}
	}

	return false;
}

#define __dsc_driver_can_op(driver) __dsc_driver_can_trans(driver)

static bool __dsc_driver_can_trans(struct dsc_driver *driver)
{
	return !__dsc_trans_active_or_queued(&driver->trans) &&
		!__dsc_driver_trans_active_down_tree(driver);
}

static bool __dsc_driver_can_trigger(struct dsc_driver *driver)
{
	return !__dsc_trans_active(&driver->trans) &&
		!__dsc_driver_trans_active_down_tree(driver);
}

static QDF_STATUS
__dsc_driver_trans_start_nolock(struct dsc_driver *driver, const char *desc)
{
	if (!__dsc_driver_can_trans(driver))
		return QDF_STATUS_E_AGAIN;

	return __dsc_trans_start(&driver->trans, desc);
}

static QDF_STATUS
__dsc_driver_trans_start(struct dsc_driver *driver, const char *desc)
{
	QDF_STATUS status;

	if (!dsc_assert(driver))
		return QDF_STATUS_E_INVAL;

	if (!dsc_assert(desc))
		return QDF_STATUS_E_INVAL;

	__dsc_lock(driver);
	status = __dsc_driver_trans_start_nolock(driver, desc);
	__dsc_unlock(driver);

	return status;
}

QDF_STATUS dsc_driver_trans_start(struct dsc_driver *driver, const char *desc)
{
	QDF_STATUS status;

	dsc_enter_str(desc);
	status = __dsc_driver_trans_start(driver, desc);
	if (QDF_IS_STATUS_ERROR(status))
		dsc_exit_status(status);

	return status;
}

static QDF_STATUS
__dsc_driver_trans_start_wait(struct dsc_driver *driver, const char *desc)
{
	QDF_STATUS status;
	struct dsc_tran tran = { 0 };

	if (!dsc_assert(driver))
		return QDF_STATUS_E_INVAL;

	if (!dsc_assert(desc))
		return QDF_STATUS_E_INVAL;

	__dsc_lock(driver);

	/* try to start without waiting */
	status = __dsc_driver_trans_start_nolock(driver, desc);
	if (QDF_IS_STATUS_SUCCESS(status))
		goto unlock;

	status = __dsc_trans_queue(&driver->trans, &tran, desc);
	if (QDF_IS_STATUS_ERROR(status))
		goto unlock;

	__dsc_unlock(driver);

	return __dsc_tran_wait(&tran);

unlock:
	__dsc_unlock(driver);

	return status;
}

QDF_STATUS
dsc_driver_trans_start_wait(struct dsc_driver *driver, const char *desc)
{
	QDF_STATUS status;

	dsc_enter_str(desc);
	status = __dsc_driver_trans_start_wait(driver, desc);
	if (QDF_IS_STATUS_ERROR(status))
		dsc_exit_status(status);

	return status;
}

bool __dsc_driver_trans_trigger_checked(struct dsc_driver *driver)
{
	if (!__dsc_trans_queued(&driver->trans))
		return false;

	/* handled, but don't trigger; we need to wait for more children */
	if (!__dsc_driver_can_trigger(driver))
		return true;

	return __dsc_trans_trigger(&driver->trans);
}

static void __dsc_driver_trigger_trans(struct dsc_driver *driver)
{
	struct dsc_psoc *psoc;
	struct dsc_vdev *vdev;

	if (__dsc_trans_trigger(&driver->trans))
		return;

	dsc_for_each_driver_psoc(driver, psoc) {
		if (__dsc_trans_trigger(&psoc->trans))
			continue;

		dsc_for_each_psoc_vdev(psoc, vdev)
			__dsc_trans_trigger(&vdev->trans);
	}
}

static void __dsc_driver_trans_stop(struct dsc_driver *driver)
{
	if (!dsc_assert(driver))
		return;

	__dsc_lock(driver);

	__dsc_trans_stop(&driver->trans);
	__dsc_driver_trigger_trans(driver);

	__dsc_unlock(driver);
}

void dsc_driver_trans_stop(struct dsc_driver *driver)
{
	__dsc_driver_trans_stop(driver);
}

static void __dsc_driver_assert_trans_protected(struct dsc_driver *driver)
{
	if (!dsc_assert(driver))
		return;

	__dsc_lock(driver);
	dsc_assert(__dsc_trans_active(&driver->trans));
	__dsc_unlock(driver);
}

void dsc_driver_assert_trans_protected(struct dsc_driver *driver)
{
	__dsc_driver_assert_trans_protected(driver);
}

static QDF_STATUS
__dsc_driver_op_start(struct dsc_driver *driver, const char *func)
{
	QDF_STATUS status;

	if (!dsc_assert(driver))
		return QDF_STATUS_E_INVAL;

	if (!dsc_assert(func))
		return QDF_STATUS_E_INVAL;

	__dsc_lock(driver);

	if (!__dsc_driver_can_op(driver)) {
		status = QDF_STATUS_E_AGAIN;
		goto unlock;
	}

	status = __dsc_ops_insert(&driver->ops, func);

unlock:
	__dsc_unlock(driver);

	return status;
}

QDF_STATUS _dsc_driver_op_start(struct dsc_driver *driver, const char *func)
{
	QDF_STATUS status;

	dsc_enter_str(func);
	status = __dsc_driver_op_start(driver, func);
	if (QDF_IS_STATUS_ERROR(status))
		dsc_exit_status(status);

	return status;
}

static void __dsc_driver_op_stop(struct dsc_driver *driver, const char *func)
{
	if (!dsc_assert(driver))
		return;

	if (!dsc_assert(func))
		return;

	__dsc_lock(driver);
	if (__dsc_ops_remove(&driver->ops, func))
		qdf_event_set(&driver->ops.event);
	__dsc_unlock(driver);
}

void _dsc_driver_op_stop(struct dsc_driver *driver, const char *func)
{
	__dsc_driver_op_stop(driver, func);
}

static void __dsc_driver_wait_for_ops(struct dsc_driver *driver)
{
	struct dsc_psoc *psoc;
	bool wait;

	if (!dsc_assert(driver))
		return;

	__dsc_lock(driver);

	/* flushing without preventing new ops is almost certainly a bug */
	dsc_assert(!__dsc_driver_can_op(driver));

	wait = driver->ops.count > 0;
	if (wait)
		qdf_event_reset(&driver->ops.event);

	__dsc_unlock(driver);

	if (wait)
		qdf_wait_single_event(&driver->ops.event, 0);

	/* wait for down-tree ops to complete as well */
	dsc_for_each_driver_psoc(driver, psoc)
		dsc_psoc_wait_for_ops(psoc);
}

void dsc_driver_wait_for_ops(struct dsc_driver *driver)
{
	__dsc_driver_wait_for_ops(driver);
}

