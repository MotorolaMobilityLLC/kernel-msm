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
 * DOC: wlan_ext_cp_stats_types.h
 *
 * This header file is included by the converged control path statistics
 * component to map legacy statistic structures to the external
 * (ext)typedefs used by the converged code. This mechanism allows the
 * legacy structs to be included as part of the global objmgr objects.
 */

#ifndef __WLAN_EXT_CP_STATS_TYPE_H__
#define __WLAN_EXT_CP_STATS_TYPE_H__

/**
 * typedef psoc_ext_cp_stats_t  - Definition of psoc cp stats pointer
 * Define obj_stats from external umac/cp_stats component point to this type
 */
typedef struct psoc_mc_cp_stats psoc_ext_cp_stats_t;

/**
 * typedef pdev_ext_cp_stats_t  - Definition of pdev cp stats pointer
 * Define pdev_stats from external umac/cp_stats component point to this type
 */
typedef struct pdev_mc_cp_stats pdev_ext_cp_stats_t;

/**
 * typedef vdev_ext_cp_stats_t  - Definition of vdev cp stats pointer
 * Define vdev_stats from external umac/cp_stats component point to this type
 */
typedef struct vdev_mc_cp_stats vdev_ext_cp_stats_t;

/**
 * typedef peer_ext_cp_stats_t  - Definition of peer cp stats pointer
 * Define peer_stats from external umac/cp_stats component point to this type
 */
typedef struct peer_mc_cp_stats peer_ext_cp_stats_t;

/**
 * typedef peer_ext_adv_cp_stats_t  - Definition of peer adv cp stats pointer
 * Define peer_adv_stats from external umac/cp_stats component point to this
 * type
 */
typedef struct peer_adv_mc_cp_stats peer_ext_adv_cp_stats_t;

#endif /* __WLAN_EXT_CP_STATS_TYPE_H__ */

