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

#ifndef _WLAN_ACTION_OUI_OBJMGR_H
#define _WLAN_ACTION_OUI_OBJMGR_H

#include "wlan_cmn.h"
#include "wlan_objmgr_cmn.h"
#include "wlan_objmgr_psoc_obj.h"

/**
 * action_oui_psoc_get_ref() - Wrapper to increment action_oui ref count
 * @psoc: psoc object
 *
 * Wrapper for action_oui to increment ref count after checking valid
 * object state.
 *
 * Return: SUCCESS/FAILURE
 */
static inline
QDF_STATUS action_oui_psoc_get_ref(struct wlan_objmgr_psoc *psoc)
{
	return wlan_objmgr_psoc_try_get_ref(psoc, WLAN_ACTION_OUI_ID);
}

/**
 * action_oui_psoc_put_ref() - Wrapper to decrement action_oui ref count
 * @psoc: psoc object
 *
 * Wrapper for action_oui to decrement ref count of psoc.
 *
 * Return: SUCCESS/FAILURE
 */
static inline
void action_oui_psoc_put_ref(struct wlan_objmgr_psoc *psoc)
{
	return wlan_objmgr_psoc_release_ref(psoc, WLAN_ACTION_OUI_ID);
}

/**
 * action_oui_psoc_get_priv(): Wrapper to retrieve psoc priv obj
 * @psoc: psoc pointer
 *
 * Wrapper for action_oui to get psoc private object pointer.
 *
 * Return: Private object of psoc
 */
static inline struct action_oui_psoc_priv *
action_oui_psoc_get_priv(struct wlan_objmgr_psoc *psoc)
{
	struct action_oui_psoc_priv *psoc_priv;

	psoc_priv = wlan_objmgr_psoc_get_comp_private_obj(psoc,
					WLAN_UMAC_COMP_ACTION_OUI);
	QDF_BUG(psoc_priv);

	return psoc_priv;
}

#endif /* _WLAN_ACTION_OUI_OBJMGR_H */
