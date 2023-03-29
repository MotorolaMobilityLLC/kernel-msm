/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
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
#include "qdf_status.h"
#include "qdf_talloc.h"
#include "qdf_types.h"
#include "__wlan_dsc.h"
#include "wlan_dsc.h"
#include "qdf_platform.h"

#define __dsc_driver_lock(vdev) __dsc_lock((vdev)->psoc->driver)
#define __dsc_driver_unlock(vdev) __dsc_unlock((vdev)->psoc->driver)

static QDF_STATUS
__dsc_vdev_create(struct dsc_psoc *psoc, struct dsc_vdev **out_vdev)
{
	struct dsc_vdev *vdev;

	if (!dsc_assert(psoc))
		return QDF_STATUS_E_INVAL;

	if (!dsc_assert(out_vdev))
		return QDF_STATUS_E_INVAL;

	*out_vdev = NULL;

	vdev = qdf_talloc_type(psoc, vdev);
	if (!vdev)
		return QDF_STATUS_E_NOMEM;

	/* init */
	vdev->psoc = psoc;
	__dsc_trans_init(&vdev->trans);
	__dsc_ops_init(&vdev->ops);

	/* attach */
	__dsc_driver_lock(vdev);
	qdf_list_insert_back(&psoc->vdevs, &vdev->node);
	__dsc_driver_unlock(vdev);

	*out_vdev = vdev;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dsc_vdev_create(struct dsc_psoc *psoc, struct dsc_vdev **out_vdev)
{
	QDF_STATUS status;

	status =  __dsc_vdev_create(psoc, out_vdev);

	return status;
}

static void __dsc_vdev_destroy(struct dsc_vdev **out_vdev)
{
	struct dsc_vdev *vdev;

	if (!dsc_assert(out_vdev))
		return;

	vdev = *out_vdev;
	if (!dsc_assert(vdev))
		return;

	*out_vdev = NULL;

	/* flush pending transitions */
	while (__dsc_trans_abort(&vdev->trans))
		;

	/* detach */
	__dsc_driver_lock(vdev);
	qdf_list_remove_node(&vdev->psoc->vdevs, &vdev->node);
	__dsc_driver_unlock(vdev);

	/* de-init */
	__dsc_ops_deinit(&vdev->ops);
	__dsc_trans_deinit(&vdev->trans);
	vdev->psoc = NULL;

	qdf_tfree(vdev);
}

void dsc_vdev_destroy(struct dsc_vdev **out_vdev)
{
	__dsc_vdev_destroy(out_vdev);
}

#define __dsc_vdev_can_op(vdev) __dsc_vdev_can_trans(vdev)

/*
 * __dsc_vdev_can_trans() - Returns if the vdev transition can occur or not
 * @vdev: The DSC vdev
 *
 * This function checks if the vdev transition can occur or not by checking if
 * any other down the tree/up the tree transition/operation is taking place.
 *
 * If there are any driver transition taking place, then the vdev trans/ops
 * should be rejected and not queued in the DSC queue. Return QDF_STATUS_E_INVAL
 * in this case.
 *
 * If there are any psoc transition taking place because of SSR, then vdev
 * trans/op should be rejected and queued in the DSC queue so that it may be
 * resumed after the current trans/op is completed. return QDF_STATUS_E_AGAIN
 * in this case.
 *
 * If there is a psoc transition taking place becasue of psoc idle shutdown,
 * then the vdev trans/ops should be rejected and queued in the DSC queue so
 * that it may be resumed after the current trans/ops is completed. Return
 * QDF_STATUS_E_AGAIN in this case.
 *
 * If there are any vdev trans/ops taking place, then the vdev trans/ops
 * should be rejected and queued in the DSC queue so that it may be resumed
 * after the current trans/ops is completed. Return QDF_STATUS_E_AGAIN in this
 * case.
 *
 * Return: QDF_STATUS_SUCCESS if transition is allowed, error code if not.
 */
static QDF_STATUS __dsc_vdev_can_trans(struct dsc_vdev *vdev)
{
	if (__dsc_trans_active_or_queued(&vdev->psoc->driver->trans))
		return QDF_STATUS_E_INVAL;

	if (qdf_is_recovering())
		return QDF_STATUS_E_INVAL;

	if (__dsc_trans_active_or_queued(&vdev->psoc->trans)) {
		/* psoc idle shutdown(wifi off) needs to be added in DSC queue
		 * to avoid wifi on failure while previous psoc idle shutdown
		 * is in progress and wifi is turned on. And Wifi On also needs
		 * to be added to the queue so that it waits for SSR to
		 * complete.
		 */
		if (qdf_is_driver_unloading())
			return QDF_STATUS_E_INVAL;
		else
			return QDF_STATUS_E_AGAIN;
	}

	if (__dsc_trans_active_or_queued(&vdev->trans))
		return QDF_STATUS_E_AGAIN;

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
__dsc_vdev_trans_start_nolock(struct dsc_vdev *vdev, const char *desc)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	status = __dsc_vdev_can_trans(vdev);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	return __dsc_trans_start(&vdev->trans, desc);
}

static QDF_STATUS
__dsc_vdev_trans_start(struct dsc_vdev *vdev, const char *desc)
{
	QDF_STATUS status;

	if (!dsc_assert(vdev))
		return QDF_STATUS_E_INVAL;

	if (!dsc_assert(desc))
		return QDF_STATUS_E_INVAL;

	__dsc_driver_lock(vdev);
	status = __dsc_vdev_trans_start_nolock(vdev, desc);
	__dsc_driver_unlock(vdev);

	return status;
}

QDF_STATUS dsc_vdev_trans_start(struct dsc_vdev *vdev, const char *desc)
{
	QDF_STATUS status;

	dsc_enter_str(desc);
	status = __dsc_vdev_trans_start(vdev, desc);
	if (QDF_IS_STATUS_ERROR(status))
		dsc_exit_status(status);

	return status;
}

static QDF_STATUS
__dsc_vdev_trans_start_wait(struct dsc_vdev *vdev, const char *desc)
{
	QDF_STATUS status;
	struct dsc_tran tran = { 0 };

	if (!dsc_assert(vdev))
		return QDF_STATUS_E_INVAL;

	if (!dsc_assert(desc))
		return QDF_STATUS_E_INVAL;

	__dsc_driver_lock(vdev);

	/* try to start without waiting */
	status = __dsc_vdev_trans_start_nolock(vdev, desc);
	if (QDF_IS_STATUS_SUCCESS(status) || status == QDF_STATUS_E_INVAL)
		goto unlock;

	status = __dsc_trans_queue(&vdev->trans, &tran, desc);
	if (QDF_IS_STATUS_ERROR(status))
		goto unlock;

	__dsc_driver_unlock(vdev);

	return __dsc_tran_wait(&tran);

unlock:
	__dsc_driver_unlock(vdev);

	return status;
}

QDF_STATUS dsc_vdev_trans_start_wait(struct dsc_vdev *vdev, const char *desc)
{
	QDF_STATUS status;

	dsc_enter_str(desc);
	status = __dsc_vdev_trans_start_wait(vdev, desc);
	if (QDF_IS_STATUS_ERROR(status))
		dsc_exit_status(status);

	return status;
}

static void __dsc_vdev_trigger_trans(struct dsc_vdev *vdev)
{
	if (__dsc_driver_trans_trigger_checked(vdev->psoc->driver))
		return;

	if (__dsc_psoc_trans_trigger_checked(vdev->psoc))
		return;

	__dsc_trans_trigger(&vdev->trans);
}

static void __dsc_vdev_trans_stop(struct dsc_vdev *vdev)
{
	if (!dsc_assert(vdev))
		return;

	__dsc_driver_lock(vdev);

	__dsc_trans_stop(&vdev->trans);
	__dsc_vdev_trigger_trans(vdev);

	__dsc_driver_unlock(vdev);
}

void dsc_vdev_trans_stop(struct dsc_vdev *vdev)
{
	__dsc_vdev_trans_stop(vdev);
}

static void __dsc_vdev_assert_trans_protected(struct dsc_vdev *vdev)
{
	if (!dsc_assert(vdev))
		return;

	__dsc_driver_lock(vdev);
	dsc_assert(__dsc_trans_active(&vdev->trans) ||
		   __dsc_trans_active(&vdev->psoc->trans) ||
		   __dsc_trans_active(&vdev->psoc->driver->trans));
	__dsc_driver_unlock(vdev);
}

void dsc_vdev_assert_trans_protected(struct dsc_vdev *vdev)
{
	__dsc_vdev_assert_trans_protected(vdev);
}

static QDF_STATUS __dsc_vdev_op_start(struct dsc_vdev *vdev, const char *func)
{
	QDF_STATUS status;

	if (!dsc_assert(vdev))
		return QDF_STATUS_E_INVAL;

	if (!dsc_assert(func))
		return QDF_STATUS_E_INVAL;

	__dsc_driver_lock(vdev);

	status = __dsc_vdev_can_op(vdev);
	if (QDF_IS_STATUS_ERROR(status))
		goto unlock;

	status = __dsc_ops_insert(&vdev->ops, func);

unlock:
	__dsc_driver_unlock(vdev);

	return status;
}

QDF_STATUS _dsc_vdev_op_start(struct dsc_vdev *vdev, const char *func)
{
	QDF_STATUS status;

	/* do not log from here because it can flood log message because vdev
	 * op protect is per vdev operation
	 */

	status = __dsc_vdev_op_start(vdev, func);

	return status;
}

static void __dsc_vdev_op_stop(struct dsc_vdev *vdev, const char *func)
{
	if (!dsc_assert(vdev))
		return;

	if (!dsc_assert(func))
		return;

	__dsc_driver_lock(vdev);
	if (__dsc_ops_remove(&vdev->ops, func))
		qdf_event_set(&vdev->ops.event);
	__dsc_driver_unlock(vdev);
}

void _dsc_vdev_op_stop(struct dsc_vdev *vdev, const char *func)
{
	/* do not log from here because it can flood log message because vdev
	 * op protect is per vdev operation
	 */
	__dsc_vdev_op_stop(vdev, func);
}

static void __dsc_vdev_wait_for_ops(struct dsc_vdev *vdev)
{
	bool wait;

	if (!dsc_assert(vdev))
		return;

	__dsc_driver_lock(vdev);

	wait = vdev->ops.count > 0;
	if (wait)
		qdf_event_reset(&vdev->ops.event);

	__dsc_driver_unlock(vdev);

	if (wait)
		qdf_wait_single_event(&vdev->ops.event, 0);
}

void dsc_vdev_wait_for_ops(struct dsc_vdev *vdev)
{
	__dsc_vdev_wait_for_ops(vdev);
}

uint8_t dsc_vdev_get_cached_cmd(struct dsc_vdev *vdev)
{
	return vdev->nb_cmd_during_ssr;
}

void dsc_vdev_cache_command(struct dsc_vdev *vdev, uint8_t cmd_id)
{
	vdev->nb_cmd_during_ssr = cmd_id;
}

