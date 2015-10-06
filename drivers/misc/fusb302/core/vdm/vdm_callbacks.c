/****************************************************************************
 * Company:         Fairchild Semiconductor
 *
 * Author           Date          Comment
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * G. Noblesmith
 *
 *
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * Software License Agreement:
 *
 * The software supplied herewith by Fairchild Semiconductor (the “Company”)
 * is supplied to you, the Company's customer, for exclusive use with its
 * USB Type C / USB PD products.  The software is owned by the Company and/or
 * its supplier, and is protected under applicable copyright laws.
 * All rights are reserved. Any use in violation of the foregoing restrictions
 * may subject the user to criminal sanctions under applicable laws, as well
 * as to civil liability for the breach of the terms and conditions of this
 * license.
 *
 * THIS SOFTWARE IS PROVIDED IN AN “AS IS” CONDITION. NO WARRANTIES,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT NOT LIMITED
 * TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. THE COMPANY SHALL NOT,
 * IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL OR
 * CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 *
 *****************************************************************************/

#include "vdm_callbacks.h"
#include "vdm_types.h"
#include "DisplayPort/dp.h"

BOOL svid_enable;
BOOL mode_enable;
UINT16 my_svid;
UINT32 my_mode;

BOOL mode_entered;
SvidInfo core_svid_info;

extern unsigned int DpModeEntered;

int AutoDpModeEntryObjPos;

Identity vdmRequestIdentityInfo()
{
	Identity id = { 0 };

	if (mode_enable == TRUE && svid_enable == TRUE) {
		id.id_header.modal_op_supported = 1;
	} else {
		id.id_header.modal_op_supported = 0;
	}
	id.nack = FALSE;
	return id;
}

SvidInfo vdmRequestSvidInfo()
{
	SvidInfo svid_info = { 0 };

	if (svid_enable) {
		svid_info.nack = FALSE;
		svid_info.num_svids = 1;
		svid_info.svids[0] = my_svid;
	} else {
		svid_info.nack = TRUE;
		svid_info.num_svids = 0;
		svid_info.svids[0] = 0x0000;
	}

	return svid_info;
}

ModesInfo vdmRequestModesInfo(UINT16 svid)
{
	ModesInfo modes_info = { 0 };

	if (svid_enable && mode_enable && (svid == my_svid)) {
		modes_info.nack = FALSE;
		modes_info.svid = svid;
		modes_info.num_modes = 1;
		modes_info.modes[0] = my_mode;
	} else {
		modes_info.nack = TRUE;
		modes_info.svid = svid;
		modes_info.num_modes = 0;
		modes_info.modes[0] = 0;
	}
	return modes_info;
}

BOOL vdmModeEntryRequest(UINT16 svid, UINT32 mode_index)
{
	if (svid_enable && mode_enable && (svid == my_svid)
	    && (mode_index == 1)) {
		mode_entered = TRUE;
		if (my_svid == DP_SID) {
			DpModeEntered = mode_index;
		}
		return TRUE;
	}

	return FALSE;
}

BOOL vdmModeExitRequest(UINT16 svid, UINT32 mode_index)
{
	if (mode_entered && (svid == my_svid) && (mode_index == 1)) {
		mode_entered = FALSE;
		if (DpModeEntered && (DpModeEntered == mode_index)
		    && (svid == DP_SID)) {
			DpModeEntered = 0;
		}
		return TRUE;
	}
	return FALSE;
}

BOOL vdmEnterModeResult(BOOL success, UINT16 svid, UINT32 mode_index)
{
	if (AutoDpModeEntryObjPos > 0) {
		AutoDpModeEntryObjPos = 0;
	}

	if (svid == DP_SID) {
		DpModeEntered = mode_index;
	}

	return TRUE;
}

void vdmExitModeResult(BOOL success, UINT16 svid, UINT32 mode_index)
{
	if (svid == DP_SID && DpModeEntered == mode_index) {
		DpModeEntered = 0;
	}
}

void vdmInformIdentity(BOOL success, SopType sop, Identity id)
{

}

void vdmInformSvids(BOOL success, SopType sop, SvidInfo svid_info)
{
	if (success) {
		INT32 i;
		core_svid_info.num_svids = svid_info.num_svids;
		for (i = 0; (i < svid_info.num_svids) && (i < MAX_NUM_SVIDS);
		     i++) {
			core_svid_info.svids[i] = svid_info.svids[i];
		}
	}
}

void vdmInformModes(BOOL success, SopType sop, ModesInfo modes_info)
{
	INT32 i;

	if (modes_info.svid == DP_SID && modes_info.nack == FALSE) {
		for (i = 0; i < modes_info.num_modes; i++) {
			if (dpEvaluateModeEntry(modes_info.modes[i])) {
				AutoDpModeEntryObjPos = i + 1;
			}
		}
	}
}

void vdmInformAttention(UINT16 svid, UINT8 mode_index)
{

}

void vdmInitDpm()
{
	svid_enable = FALSE;
	mode_enable = FALSE;
	my_svid = 0x0779;	// 0x0779 = FC VID
	my_mode = 0x0001;

	mode_entered = FALSE;
}
