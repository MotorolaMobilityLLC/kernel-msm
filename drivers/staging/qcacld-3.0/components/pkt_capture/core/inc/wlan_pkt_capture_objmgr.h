/*
 * Copyright (c) 2020 The Linux Foundation. All rights reserved.
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
 * DOC: This file contains various object manager related wrappers and helpers
 */

#ifndef _WLAN_PKT_CAPTURE_OBJMGR_H
#define _WLAN_PKT_CAPTURE_OBJMGR_H

#include "wlan_cmn.h"
#include "wlan_objmgr_cmn.h"
#include "wlan_objmgr_vdev_obj.h"
#include "wlan_objmgr_global_obj.h"

/**
 * pkt_capture_vdev_get_ref() - Wrapper to increment pkt_capture ref count
 * @vdev: vdev object
 *
 * Wrapper for pkt_capture to increment ref count after checking valid
 * object state.
 *
 * Return: SUCCESS/FAILURE
 */
static inline
QDF_STATUS pkt_capture_vdev_get_ref(struct wlan_objmgr_vdev *vdev)
{
	return wlan_objmgr_vdev_try_get_ref(vdev, WLAN_PKT_CAPTURE_ID);
}

/**
 * pkt_capture_vdev_put_ref() - Wrapper to decrement pkt_capture ref count
 * @vdev: vdev object
 *
 * Wrapper for pkt_capture to decrement ref count of vdev.
 *
 * Return: SUCCESS/FAILURE
 */
static inline
void pkt_capture_vdev_put_ref(struct wlan_objmgr_vdev *vdev)
{
	wlan_objmgr_vdev_release_ref(vdev, WLAN_PKT_CAPTURE_ID);
}

/**
 * pkt_capture_vdev_get_priv() - Wrapper to retrieve vdev priv obj
 * @vdev: vdev pointer
 *
 * Wrapper for pkt_capture to get vdev private object pointer.
 *
 * Return: Private object of vdev
 */
static inline struct pkt_capture_vdev_priv *
pkt_capture_vdev_get_priv(struct wlan_objmgr_vdev *vdev)
{
	struct pkt_capture_vdev_priv *vdev_priv;

	vdev_priv = wlan_objmgr_vdev_get_comp_private_obj(
					vdev,
					WLAN_UMAC_COMP_PKT_CAPTURE);
	QDF_BUG(vdev_priv);

	return vdev_priv;
}

/**
 * pkt_capture_psoc_get_ref() - Wrapper to increment pkt_capture ref count
 * @psoc: psoc object
 *
 * Wrapper for pkt_capture to increment ref count after checking valid
 * object state.
 *
 * Return: QDF_STATUS
 */
static inline
QDF_STATUS pkt_capture_psoc_get_ref(struct wlan_objmgr_psoc *psoc)
{
	return wlan_objmgr_psoc_try_get_ref(psoc, WLAN_PKT_CAPTURE_ID);
}

/**
 * pkt_capture_psoc_put_ref() - Wrapper to decrement pkt_capture ref count
 * @psoc: psoc object
 *
 * Wrapper for pkt_capture to decrement ref count of psoc.
 *
 * Return: None
 */
static inline
void pkt_capture_psoc_put_ref(struct wlan_objmgr_psoc *psoc)
{
	wlan_objmgr_psoc_release_ref(psoc, WLAN_PKT_CAPTURE_ID);
}

/**
 * pkt_capture_psoc_get_priv(): Wrapper to retrieve psoc priv obj
 * @psoc: psoc pointer
 *
 * Wrapper for pkt_capture to get psoc private object pointer.
 *
 * Return: pkt_capture psoc private object
 */
static inline struct pkt_psoc_priv *
pkt_capture_psoc_get_priv(struct wlan_objmgr_psoc *psoc)
{
	struct pkt_psoc_priv *psoc_priv;

	psoc_priv = wlan_objmgr_psoc_get_comp_private_obj(psoc,
					WLAN_UMAC_COMP_PKT_CAPTURE);
	QDF_BUG(psoc_priv);

	return psoc_priv;
}
#endif /* _WLAN_PKT_CAPTURE_OBJMGR_H */
