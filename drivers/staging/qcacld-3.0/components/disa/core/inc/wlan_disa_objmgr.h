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
 * DOC: This file contains various object manager related wrappers and helpers
 */

#ifndef _WLAN_DISA_OBJMGR_H
#define _WLAN_DISA_OBJMGR_H

#include "wlan_objmgr_vdev_obj.h"
#include "wlan_objmgr_psoc_obj.h"
#include "wlan_disa_obj_mgmt_public_struct.h"

/* Get/Put Ref */

/**
 * disa_psoc_get_ref() - DISA wrapper to increment ref count, if allowed
 * @psoc: PSOC object
 *
 * DISA wrapper to increment ref count after checking valid object state
 *
 * Return: SUCCESS/FAILURE
 */
static inline QDF_STATUS disa_psoc_get_ref(struct wlan_objmgr_psoc *psoc)
{
	return wlan_objmgr_psoc_try_get_ref(psoc, WLAN_DISA_ID);
}

/**
 * disa_psoc_put_ref() - DISA wrapper to decrement ref count
 * @psoc: PSOC object
 *
 * DISA wrapper to decrement ref count of psoc
 *
 * Return: SUCCESS/FAILURE
 */
static inline void disa_psoc_put_ref(struct wlan_objmgr_psoc *psoc)
{
	return wlan_objmgr_psoc_release_ref(psoc, WLAN_DISA_ID);
}

/* Private Data */

/**
 * disa_psoc_get_priv_nolock(): DISA wrapper to retrieve component object
 * @psoc: Psoc pointer
 *
 * DISA wrapper used to get the component private object pointer
 *
 * Return: Component private object
 */
static inline void *disa_psoc_get_priv_nolock(struct wlan_objmgr_psoc *psoc)
{
	return wlan_objmgr_psoc_get_comp_private_obj(psoc, WLAN_UMAC_COMP_DISA);
}

/* Ids */
static inline uint8_t
disa_vdev_get_id(struct wlan_objmgr_vdev *vdev)
{
	uint8_t vdev_id;

	vdev_id = wlan_vdev_get_id(vdev);
	QDF_BUG(vdev_id < WLAN_UMAC_PSOC_MAX_VDEVS);

	return vdev_id;
}

/* Tree Navigation */

/**
 * !PLEASE READ!
 *
 * The following are objmgr navigation helpers for traversing objmgr object
 * trees.
 *
 * Objmgr ensures parents of an objmgr object cannot be freed while a valid
 * reference to one of its children is held. Based on this fact, all of these
 * navigation helpers make the following assumptions to ensure safe usage:
 *
 * 1) The caller must hold a valid reference to the input objmgr object!
 *	E.g. Use disa_[peer|vdev|pdev|psoc]_get_ref() on the input objmgr
 *	object before using these APIs
 * 2) Given assumption #1, the caller does not need to hold a reference to the
 *	parents of the input objmgr object
 * 3) Given assumption #1, parents of the input objmgr object cannot be null
 * 4) Given assumption #1, private contexts of any parent of the input objmgr
 *	object cannot be null
 *
 * These characteristics remove the need for most sanity checks when dealing
 * with objmgr objects. However, please note that if you ever walk the tree
 * from parent to child, references must be acquired all the way down!
 *
 * Example #1:
 *
 *	psoc = disa_vdev_get_psoc(vdev);
 *	if (!psoc)
 *		// this is dead code
 *
 * Example #2:
 *
 *	psoc_priv = disa_psoc_get_priv(psoc);
 *	if (!psoc_priv)
 *		// this is dead code
 *
 * Example #3:
 *
 *	status = disa_psoc_get_ref(psoc);
 *
 *	...
 *
 *	psoc = disa_vdev_get_psoc(vdev);
 *
 *	// the next line is redundant, don't do it!
 *	status = disa_psoc_get_ref(psoc);
 */

/* Tree Navigation: psoc */
static inline struct disa_psoc_priv_obj *
disa_psoc_get_priv(struct wlan_objmgr_psoc *psoc)
{
	struct disa_psoc_priv_obj *psoc_priv;

	psoc_priv = disa_psoc_get_priv_nolock(psoc);
	QDF_BUG(psoc_priv);

	return psoc_priv;
}

static inline struct wlan_objmgr_vdev *
disa_psoc_get_vdev(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;

	QDF_BUG(vdev_id < WLAN_UMAC_PSOC_MAX_VDEVS);
	if (vdev_id >= WLAN_UMAC_PSOC_MAX_VDEVS)
		return NULL;

	wlan_psoc_obj_lock(psoc);
	vdev = psoc->soc_objmgr.wlan_vdev_list[vdev_id];
	wlan_psoc_obj_unlock(psoc);

	return vdev;
}

/* Tree Navigation: pdev */
static inline struct wlan_objmgr_psoc *
disa_pdev_get_psoc(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_objmgr_psoc *psoc;

	psoc = wlan_pdev_get_psoc(pdev);
	QDF_BUG(psoc);

	return psoc;
}

/* Tree Navigation: vdev */
static inline struct wlan_objmgr_pdev *
disa_vdev_get_pdev(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_pdev *pdev;

	pdev = wlan_vdev_get_pdev(vdev);
	QDF_BUG(pdev);

	return pdev;
}

static inline struct wlan_objmgr_psoc *
disa_vdev_get_psoc(struct wlan_objmgr_vdev *vdev)
{
	return disa_pdev_get_psoc(disa_vdev_get_pdev(vdev));
}

#endif /* _WLAN_DISA_OBJMGR_H */
