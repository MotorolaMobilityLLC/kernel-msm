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

#ifndef __FSC_VDM_CALLBACKS_DEFS_H__
#define __FSC_VDM_CALLBACKS_DEFS_H__
/*
 * This file defines types for callbacks that the VDM block will use.
 * The intention is for the user to define functions that return data 
 * that VDM messages require, ex whether or not to enter a mode.
 */

#include "vdm_types.h"
#include "../PD_Types.h"

typedef Identity(*RequestIdentityInfo) (VOID);

typedef SvidInfo(*RequestSvidInfo) (VOID);

typedef ModesInfo(*RequestModesInfo) (UINT16);

typedef BOOL(*ModeEntryRequest) (UINT16 svid, UINT32 mode_index);

typedef BOOL(*ModeExitRequest) (UINT16 svid, UINT32 mode_index);

typedef BOOL(*EnterModeResult) (BOOL success, UINT16 svid, UINT32 mode_index);

typedef void (*ExitModeResult) (BOOL success, UINT16 svid, UINT32 mode_index);

typedef void (*InformIdentity) (BOOL success, SopType sop, Identity id);

typedef void (*InformSvids) (BOOL success, SopType sop, SvidInfo svid_info);

typedef void (*InformModes) (BOOL success, SopType sop, ModesInfo modes_info);

typedef void (*InformAttention) (UINT16 svid, UINT8 mode_index);

#endif // __FSC_VDM_CALLBACKS_DEFS_H__
