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
 * The software supplied herewith by Fairchild Semiconductor (the Company)
 * is supplied to you, the Company's customer, for exclusive use with its
 * USB Type C / USB PD products.  The software is owned by the Company and/or
 * its supplier, and is protected under applicable copyright laws.
 * All rights are reserved. Any use in violation of the foregoing restrictions
 * may subject the user to criminal sanctions under applicable laws, as well
 * as to civil liability for the breach of the terms and conditions of this
 * license.
 *
 * THIS SOFTWARE IS PROVIDED IN AN AS IS CONDITION. NO WARRANTIES,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT NOT LIMITED
 * TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. THE COMPANY SHALL NOT,
 * IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL OR
 * CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 *
 *****************************************************************************/
#ifdef FSC_HAVE_VDM

#include "vdm_callbacks.h"
#include "vdm_types.h"
#include "../../Platform_Linux/fusb30x_global.h"
#ifdef FSC_HAVE_DP
#include "DisplayPort/dp.h"
#include "DisplayPort/interface_dp.h"
#endif // FSC_HAVE_DP
#include "../TypeC.h"
FSC_BOOL svid_enable;
FSC_BOOL mode_enable;
FSC_U16 my_svid;
FSC_U32 my_mode;

FSC_BOOL mode_entered;
SvidInfo core_svid_info;

#ifdef FSC_HAVE_DP
int AutoModeEntryObjPos;
#endif // FSC_HAVE_DP

Identity vdmRequestIdentityInfo()
{
	Identity id = { 0 };

	if (mode_enable == TRUE && svid_enable == TRUE) {
		id.id_header.modal_op_supported = 1;
	} else {
		id.id_header.modal_op_supported = 0;
	}
	id.nack = FALSE;
	id.id_header.usb_vid = my_svid;
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

ModesInfo vdmRequestModesInfo(FSC_U16 svid)
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

FSC_BOOL vdmModeEntryRequest(FSC_U16 svid, FSC_U32 mode_index)
{
	if (svid_enable && mode_enable && (svid == my_svid)
	    && (mode_index == 1)) {
		mode_entered = TRUE;

#ifdef FSC_HAVE_DP
		if (my_svid == DP_SID) {
			DpModeEntered = mode_index;
		}
#endif // FSC_HAVE_DP
		return TRUE;
	}

	return FALSE;
}

FSC_BOOL vdmModeExitRequest(FSC_U16 svid, FSC_U32 mode_index)
{
	if (mode_entered && (svid == my_svid) && (mode_index == 1)) {
		mode_entered = FALSE;

#ifdef FSC_HAVE_DP
		if (DpModeEntered && (DpModeEntered == mode_index)
		    && (svid == DP_SID)) {
			DpModeEntered = 0;
		}
#endif // FSC_HAVE_DP

		return TRUE;
	}
	return FALSE;
}

FSC_BOOL vdmEnterModeResult(FSC_BOOL success, FSC_U16 svid, FSC_U32 mode_index)
{
#ifdef FSC_HAVE_DP
	if (AutoModeEntryObjPos > 0) {
		AutoModeEntryObjPos = 0;
	}

	if (svid == DP_SID) {
		DpModeEntered = mode_index;
	}
#endif // FSC_HAVE_DP
	if (svid == MOTOROL_VENDOR_ID && success)
		DpModeEntered = mode_index;
	return TRUE;
}

void vdmExitModeResult(FSC_BOOL success, FSC_U16 svid, FSC_U32 mode_index)
{
#ifdef FSC_HAVE_DP
	if (svid == DP_SID && DpModeEntered == mode_index) {
		DpModeEntered = 0;
	}
#endif // FSC_HAVE_DP
}

void vdmInformIdentity(FSC_BOOL success, SopType sop, Identity id)
{

}

void vdmInformSvids(FSC_BOOL success, SopType sop, SvidInfo svid_info)
{
	if (success) {
		FSC_U32 i;
		core_svid_info.num_svids = svid_info.num_svids;
		for (i = 0; (i < svid_info.num_svids) && (i < MAX_NUM_SVIDS);
		     i++) {
			core_svid_info.svids[i] = svid_info.svids[i];
		}
	}
}
FSC_BOOL vdmEvaluateModeEntry(FSC_U32 mode_in)
{
	doDataObject_t modeObj;

	modeObj.object = mode_in;
	FUSB_LOG("MaxCur %d ma", modeObj.ModeInfo.MaxCurrentLimit*10);
	if (1 == modeObj.ModeInfo.ModeID) {
		gChargerMaxCurrent = modeObj.ModeInfo.MaxCurrentLimit;
		return TRUE;
	} else
		return FALSE;
}

void vdmInformModes(FSC_BOOL success, SopType sop, ModesInfo modes_info)
{
#ifdef FSC_HAVE_DP
	FSC_U32 i;

	if (modes_info.svid == DP_SID && modes_info.nack == FALSE) {
		for (i = 0; i < modes_info.num_modes; i++) {
			if (dpEvaluateModeEntry(modes_info.modes[i]))
				AutoModeEntryObjPos = i + 1;
		}
	}
#endif

	if (modes_info.svid == MOTOROL_VENDOR_ID && modes_info.nack == FALSE) {
		for (i = 0; i < modes_info.num_modes; i++) {
			if (vdmEvaluateModeEntry(modes_info.modes[i]))
				AutoModeEntryObjPos = i + 1;
		}
	}
}

void vdmInformAttention(FSC_U16 svid, FSC_U8 mode_index)
{

}

void vdmInitDpm()
{
#ifdef FM150911A
	svid_enable = TRUE;
	mode_enable = TRUE;
#else
	svid_enable = TRUE;
	mode_enable = TRUE;
#endif
	my_svid = MOTOROL_VENDOR_ID;
	my_mode = 0x0001;

	mode_entered = FALSE;
}

#endif // FSC_HAVE_VDM
