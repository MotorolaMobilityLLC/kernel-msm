/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#if !defined(__FTM_TIME_SYNC_CFG_H__)
#define __FTM_TIME_SYNC_CFG_H__

/**
 *
 * DOC: ftm_time_sync_cfg.h
 *
 * FTM TIME SYNC feature INI configuration parameter definitions
 */

#ifdef FEATURE_WLAN_TIME_SYNC_FTM

#include "cfg_define.h"
#include "cfg_converged.h"
#include "qdf_types.h"

/*
 * <ini>
 * enable_time_sync_ftm - Time Sync FTM feature support
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * When set to 1 Time Sync FTM feature will be enabled.
 *
 * Supported Feature: Time Sync FTM
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_TIME_SYNC_FTM CFG_INI_BOOL("enable_time_sync_ftm", \
				    0, \
				    "Enable Time Sync FTM Support")
/*
 * <ini>
 * time_sync_ftm_mode- Time Sync FTM feature Mode configuration
 * @Min: 0 - Aggregated Mode
 * @Max: 1 - Burst Mode
 * @Default: 0
 *
 * This ini is applicable only if enable_time_sync_ftm  is set to 1.
 *
 * Supported Feature: Time Sync FTM
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_TIME_SYNC_FTM_MODE CFG_INI_BOOL("time_sync_ftm_mode", \
				    0, \
				    "Configure Time Sync FTM Mode")
/*
 * <ini>
 * time_sync_ftm_role- Time Sync FTM feature Role configuration
 * @Min: 0 - Slave Role
 * @Max: 1 - Master Role
 * @Default: 0
 *
 * This ini is applicable only if enable_time_sync_ftm is set to 1.
 *
 * Supported Feature: Time Sync FTM
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_TIME_SYNC_FTM_ROLE CFG_INI_BOOL("time_sync_ftm_role", \
				    0, \
				    "Configure Time Sync FTM Role")

#define CFG_TIME_SYNC_FTM_ALL \
	CFG(CFG_ENABLE_TIME_SYNC_FTM) \
	CFG(CFG_TIME_SYNC_FTM_MODE) \
	CFG(CFG_TIME_SYNC_FTM_ROLE)

#else
#define CFG_TIME_SYNC_FTM_ALL
#endif /* FEATURE_WLAN_TIME_SYNC_FTM */
#endif /* __FTM_TIME_SYNC_CFG_H__ */
