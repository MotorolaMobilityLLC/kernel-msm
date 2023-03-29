/*
 * Copyright (c) 2017-2020 The Linux Foundation. All rights reserved.
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

#ifndef _WLAN_PMO_OBJMGR_H
#define _WLAN_PMO_OBJMGR_H

#ifdef WLAN_POWER_MANAGEMENT_OFFLOAD

#include "wlan_cmn.h"
#include "wlan_objmgr_cmn.h"
#include "wlan_objmgr_peer_obj.h"
#include "wlan_objmgr_vdev_obj.h"
#include "wlan_objmgr_pdev_obj.h"
#include "wlan_objmgr_psoc_obj.h"
#include "wlan_pmo_obj_mgmt_public_struct.h"
#include "wlan_utility.h"


/* Get/Put Ref */

#define pmo_peer_get_ref(peer) wlan_objmgr_peer_try_get_ref(peer, WLAN_PMO_ID)
#define pmo_peer_put_ref(peer) wlan_objmgr_peer_release_ref(peer, WLAN_PMO_ID)

#define pmo_vdev_get_ref(vdev) wlan_objmgr_vdev_try_get_ref(vdev, WLAN_PMO_ID)

#define pmo_pdev_get_ref(pdev) wlan_objmgr_pdev_try_get_ref(pdev, WLAN_PMO_ID)
#define pmo_pdev_put_ref(pdev) wlan_objmgr_pdev_release_ref(pdev, WLAN_PMO_ID)

#define pmo_psoc_get_ref(psoc) wlan_objmgr_psoc_try_get_ref(psoc, WLAN_PMO_ID)
#define pmo_psoc_put_ref(psoc) wlan_objmgr_psoc_release_ref(psoc, WLAN_PMO_ID)

/* Private Data */

#define pmo_vdev_get_priv_nolock(vdev) \
	wlan_objmgr_vdev_get_comp_private_obj(vdev, WLAN_UMAC_COMP_PMO)
#define pmo_psoc_get_priv_nolock(psoc) \
	wlan_objmgr_psoc_get_comp_private_obj(psoc, WLAN_UMAC_COMP_PMO)

/* Ids */

static inline uint8_t
pmo_vdev_get_id(struct wlan_objmgr_vdev *vdev)
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
 * The following are objmgr naviation helpers for traversing objmgr object
 * trees.
 *
 * Objmgr ensures parents of an objmgr object cannot be freed while a valid
 * reference to one of its children is held. Based on this fact, all of these
 * navigation helpers make the following assumptions to ensure safe usage:
 *
 * 1) The caller must hold a valid reference to the input objmgr object!
 *	E.g. Use pmo_[peer|vdev|pdev|psoc]_get_ref() on the input objmgr object
 *	before using these APIs
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
 *	psoc = pmo_vdev_get_psoc(vdev);
 *	if (!psoc)
 *		// this is dead code
 *
 * Example #2:
 *
 *	psoc_priv = pmo_psoc_get_priv(psoc);
 *	if (!psoci_priv)
 *		// this is dead code
 *
 * Example #3:
 *
 *	status = pmo_vdev_get_ref(vdev);
 *
 *	...
 *
 *	psoc = pmo_vdev_get_psoc(vdev);
 *
 *	// the next line is redundant, don't do it!
 *	status = pmo_psoc_get_ref(psoc);
 */

/* Tree Navigation: psoc */

static inline struct pmo_psoc_priv_obj *
pmo_psoc_get_priv(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *psoc_priv;

	psoc_priv = pmo_psoc_get_priv_nolock(psoc);
	QDF_BUG(psoc_priv);

	return psoc_priv;
}

static inline bool __pmo_spinlock_bh_safe(struct pmo_psoc_priv_obj *psoc_ctx)
{
	if (!psoc_ctx)
		return false;

	qdf_spin_lock_bh(&psoc_ctx->lock);

	return true;
}

#define pmo_psoc_with_ctx(psoc, cursor) \
	for (cursor = pmo_psoc_get_priv(psoc); \
	     __pmo_spinlock_bh_safe(cursor); \
	     qdf_spin_unlock_bh(&cursor->lock), cursor = NULL)

/* Tree Navigation: pdev */

static inline struct wlan_objmgr_psoc *
pmo_pdev_get_psoc(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_objmgr_psoc *psoc;

	psoc = wlan_pdev_get_psoc(pdev);
	QDF_BUG(psoc);

	return psoc;
}

static inline struct pmo_psoc_priv_obj *
pmo_pdev_get_psoc_priv(struct wlan_objmgr_pdev *pdev)
{
	return pmo_psoc_get_priv(pmo_pdev_get_psoc(pdev));
}

/* Tree Navigation: vdev */

static inline struct pmo_vdev_priv_obj *
pmo_vdev_get_priv(struct wlan_objmgr_vdev *vdev)
{
	struct pmo_vdev_priv_obj *vdev_priv;

	vdev_priv = pmo_vdev_get_priv_nolock(vdev);
	QDF_BUG(vdev_priv);

	return vdev_priv;
}

static inline struct wlan_objmgr_pdev *
pmo_vdev_get_pdev(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_pdev *pdev;

	pdev = wlan_vdev_get_pdev(vdev);
	QDF_BUG(pdev);

	return pdev;
}

static inline struct wlan_objmgr_psoc *
pmo_vdev_get_psoc(struct wlan_objmgr_vdev *vdev)
{
	return pmo_pdev_get_psoc(pmo_vdev_get_pdev(vdev));
}

static inline struct pmo_psoc_priv_obj *
pmo_vdev_get_psoc_priv(struct wlan_objmgr_vdev *vdev)
{
	return pmo_psoc_get_priv(pmo_pdev_get_psoc(pmo_vdev_get_pdev(vdev)));
}

#endif /* WLAN_POWER_MANAGEMENT_OFFLOAD */

#endif /* _WLAN_PMO_OBJMGR_H */
