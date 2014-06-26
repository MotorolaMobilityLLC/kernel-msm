/*
 *  intel_sst_ctp.h - Common enum of the Clovertrail platform
 *
 *  Copyright (C) 2013 Intel Corp
 *  Author: Samreen Nilofer <samreen.nilofer@intel.com>
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
#ifndef _INTEL_SST_CTP_H
#define _INTEL_SST_CTP_H

enum {
	CTP_RHB_AUD_ASP_DEV = 0,
	CTP_RHB_AUD_VSP_DEV,
	CTP_RHB_AUD_COMP_ASP_DEV,
	CTP_RHB_COMMS_BT_SCO_DEV,
	CTP_RHB_COMMS_MSIC_VOIP_DEV,
	CTP_RHB_COMMS_IFX_MODEM_DEV,
	CTP_RHB_AUD_VIRTUAL_ASP_DEV,
	CTP_RHB_AUD_PROBE_DEV,
	CTP_RHB_COMMS_FM_DEV
};

enum {
	CTP_VB_AUD_ASP_DEV = 0,
	CTP_VB_AUD_VSP_DEV,
	CTP_VB_AUD_COMP_ASP_DEV,
	CTP_VB_COMMS_BT_SCO_DEV,
	CTP_VB_COMMS_FM_DEV,
	CTP_VB_AUD_PROBE_DEV,
	CTP_VB_AUD_VIRTUAL_ASP_DEV,
	CTP_VB_COMMS_VOIP_DEV,
	CTP_VB_COMMS_MIXING_DEV,
};

enum {
	MERR_BB_AUD_ASP_DEV = 0,
	MERR_BB_AUD_VSP_DEV,
	MERR_BB_COMMS_BT_SCO_DEV,
	MERR_BB_COMMS_FM_DEV,
	MERR_BB_COMMS_VOIP_DEV,
	MERR_BB_COMMS_MIXING_DEV,
	MERR_BB_AUD_VIRTUAL_ASP_DEV,
	MERR_BB_AUD_PROBE_DEV,
};

enum {
	CTP_HT_AUD_ASP_DEV = 0,
	CTP_HT_AUD_VSP_DEV,
	CTP_HT_AUD_COMP_ASP_DEV,
	CTP_HT_COMMS_BT_SCO_DEV,
	CTP_HT_COMMS_FM_DEV,
	CTP_HT_COMMS_VOIP_DEV,
	CTP_HT_AUD_VIRTUAL_ASP_DEV,
	CTP_HT_AUD_PROBE_DEV,
};

enum {
	CTP_LT_AUD_ASP_DEV = 0,
	CTP_LT_AUD_VSP_DEV,
	CTP_LT_AUD_COMP_ASP_DEV,
	CTP_LT_COMMS_BT_SCO_DEV,
	CTP_LT_COMMS_MSIC_VOIP_DEV,
	CTP_LT_AUD_PROBE_DEV,
};

#endif
