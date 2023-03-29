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
/**
 * DOC: wlan_ext_mlme_obj_types.h
 *
 * This header file is included by the converged control path mlme
 * component to map legacy private structures to the external
 * (ext)typedefs used by the converged code. This mechanism allows the
 * legacy structs to be included as part of the global objmgr objects.
 */

#ifndef __WLAN_EXT_MLME_OBJ_TYPE_H__
#define __WLAN_EXT_MLME_OBJ_TYPE_H__

/**
 * typedef mlme_pdev_ext_t - Opaque definition of pdev mlme pointer
 * Define ext_pdev_ptr from external umac/mlme component point to this type
 */
struct opaque_mlme_pdev_ext;
typedef struct opaque_mlme_pdev_ext mlme_pdev_ext_t;

/**
 * typedef mlme_vdev_ext_t - Definition of vdev mlme pointer
 * Define ext_vdev_ptr from external umac/mlme component point to this type
 */
typedef struct mlme_legacy_priv mlme_vdev_ext_t;

/**
 * typedef mlme_psoc_ext_t - Definition of psoc mlme pointer
 * Define ext_psoc_ptr from external umac/mlme component point to this type
 */
typedef struct wlan_mlme_psoc_ext_obj mlme_psoc_ext_t;

#endif
