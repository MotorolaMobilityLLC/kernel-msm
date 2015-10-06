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
#ifndef __DPM_EMULATION_H__
#define __DPM_EMULATION_H__

#include "../platform.h"
#include "vdm_types.h"
#include "../PD_Types.h"

Identity vdmRequestIdentityInfo(VOID);
SvidInfo vdmRequestSvidInfo(VOID);
ModesInfo vdmRequestModesInfo(UINT16 svid);
BOOL vdmModeEntryRequest(UINT16 svid, UINT32 mode_index);
BOOL vdmModeExitRequest(UINT16 svid, UINT32 mode_index);
BOOL vdmEnterModeResult(BOOL success, UINT16 svid, UINT32 mode_index);
void vdmExitModeResult(BOOL success, UINT16 svid, UINT32 mode_index);
void vdmInformIdentity(BOOL success, SopType sop, Identity id);
void vdmInformSvids(BOOL success, SopType sop, SvidInfo svid_info);
void vdmInformModes(BOOL success, SopType sop, ModesInfo modes_info);
void vdmInformAttention(UINT16 svid, UINT8 mode_index);

void vdmInitDpm(VOID);

#endif
