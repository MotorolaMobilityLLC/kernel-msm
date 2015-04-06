/*
 * Copyright (c) 2013-2014 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
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

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

#ifndef WMA_STUB
#define WMA_STUB

#include "vos_api.h"
#include "vos_packet.h"
#include "vos_types.h"

static inline VOS_STATUS wma_shutdown(v_PVOID_t pVosContext, v_BOOL_t closeTransport)
{
	return VOS_STATUS_SUCCESS;
}

static inline void WMA_TimerTrafficStatsInd(void *pWMA) {
	return;
}

static inline VOS_STATUS WMA_GetWcnssHardwareVersion(v_PVOID_t pvosGCtx,
		tANI_U8 *pVersion,
		tANI_U32 versionBufferSize)
{
	return VOS_STATUS_SUCCESS;
}

static inline VOS_STATUS WMA_GetWcnssWlanCompiledVersion(v_PVOID_t pvosGCtx,
		tSirVersionType *pVersion)
{
	return VOS_STATUS_SUCCESS;
}

static inline tANI_U8 WMA_getFwWlanFeatCaps(tANI_U8 featEnumValue)
{
	return featEnumValue;
}

static inline void WMA_disableCapablityFeature(tANI_U8 feature_index) {
	return;
}

static inline VOS_STATUS WMA_HALDumpCmdReq(tpAniSirGlobal   pMac, tANI_U32  cmd,
		tANI_U32   arg1, tANI_U32   arg2, tANI_U32   arg3,
		tANI_U32   arg4, tANI_U8   *pBuffer)	{
	return VOS_STATUS_SUCCESS;
}

static inline void WMA_TrafficStatsTimerActivate(v_BOOL_t activate)
{
	return;
}

static inline VOS_STATUS WMA_GetWcnssWlanReportedVersion(v_PVOID_t pvosGCtx,
		tSirVersionType *pVersion)
{
	return VOS_STATUS_SUCCESS;
}

static inline void WMA_featureCapsExchange(v_PVOID_t pVosContext) {
	return;
}

static inline void WMA_UpdateRssiBmps(v_PVOID_t pvosGCtx,
			  v_U8_t staId, v_S7_t rssi)
{
}

#endif
