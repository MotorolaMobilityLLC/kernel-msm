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

/*
 * Using this file to emulate the callbacks that a real DPM would set up for the VDM block.
 * Setting default values etc in here too.
 */
#ifdef FSC_HAVE_VDM

#ifndef __DPM_EMULATION_H__
#define __DPM_EMULATION_H__

#include "../platform.h"
#include "vdm_types.h"
#include "../PD_Types.h"

Identity vdmRequestIdentityInfo(void);
SvidInfo vdmRequestSvidInfo(void);
ModesInfo vdmRequestModesInfo(FSC_U16 svid);
FSC_BOOL vdmModeEntryRequest(FSC_U16 svid, FSC_U32 mode_index);
FSC_BOOL vdmModeExitRequest(FSC_U16 svid, FSC_U32 mode_index);
FSC_BOOL vdmEnterModeResult(FSC_BOOL success, FSC_U16 svid, FSC_U32 mode_index);
void vdmExitModeResult(FSC_BOOL success, FSC_U16 svid, FSC_U32 mode_index);
void vdmInformIdentity(FSC_BOOL success, SopType sop, Identity id);
void vdmInformSvids(FSC_BOOL success, SopType sop, SvidInfo svid_info);
void vdmInformModes(FSC_BOOL success, SopType sop, ModesInfo modes_info);
void vdmInformAttention(FSC_U16 svid, FSC_U8 mode_index);

void vdmInitDpm(void);

#endif // __DPM_EMULATION_H__

#endif // FSC_HAVE_VDM
