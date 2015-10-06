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

#ifndef __VDM_MANAGER_H__
#define __VDM_MANAGER_H__

#include "../platform.h"
#include "fsc_vdm_defs.h"
#include "vdm_callbacks_defs.h"
#include "../PD_Types.h"

#define NUM_VDM_MODES 6
#define MAX_NUM_SVIDS_PER_SOP 30
#define MAX_SVIDS_PER_MESSAGE 12
#define MIN_DISC_ID_RESP_SIZE 3

#define tVDMSenderResponse 27	// Milliseconds
#define tVDMWaitModeEntry  100	// Milliseconds
#define tVDMWaitModeExit   100	// Milliseconds

/*
 * VDM Manager object, so I can have multiple instances intercommunicating using the same functions!
 */
typedef struct {
	// callbacks!
	RequestIdentityInfo req_id_info;
	RequestSvidInfo req_svid_info;
	RequestModesInfo req_modes_info;
	ModeEntryRequest req_mode_entry;
	ModeExitRequest req_mode_exit;
	EnterModeResult enter_mode_result;
	ExitModeResult exit_mode_result;
	InformIdentity inform_id;
	InformSvids inform_svids;
	InformModes inform_modes;
	InformAttention inform_attention;
} VdmManager;

/*
 * Initialization functions.
 */
INT32 initializeVdm(VOID);

/*
 * Functions to go through PD VDM flow. 
 */
	// Initiations from DPM
INT32 requestDiscoverIdentity(SopType sop);	// Discovers identities in the system!
INT32 requestDiscoverSvids(SopType sop);	// Discovers SVIDs in the system!
INT32 requestDiscoverModes(SopType sop, UINT16 svid);	// Discovers Modes available for each SVID
INT32 requestSendAttention(SopType sop, UINT16 svid, UINT8 mode);	// send attention command to specified sop
INT32 requestEnterMode(SopType sop, UINT16 svid, UINT32 mode_index);	// enter mode specified by SVID and mode index
INT32 requestExitMode(SopType sop, UINT16 svid, UINT32 mode_index);	// exit mode specified by SVID and mode index                                                                       
INT32 requestExitAllModes(VOID);	// exits all modes

	// receiving end
INT32 processVdmMessage(SopType sop, UINT32 * arr, UINT32 length);	// function to call when we receive VDM messages
INT32 processDiscoverIdentity(SopType sop, UINT32 * arr_in, UINT32 length_in);
INT32 processDiscoverSvids(SopType sop, UINT32 * arr_in, UINT32 length_in);
INT32 processDiscoverModes(SopType sop, UINT32 * arr_in, UINT32 length_in);
INT32 processEnterMode(SopType sop, UINT32 * arr_in, UINT32 length_in);
INT32 processExitMode(SopType sop, UINT32 * arr_in, UINT32 length_in);
INT32 processAttention(SopType sop, UINT32 * arr_in, UINT32 length_in);
INT32 processSvidSpecific(SopType sop, UINT32 * arr_in, UINT32 length_in);

void sendVdmMessageWithTimeout(SopType sop, UINT32 * arr, UINT32 length, INT32 n_pe);	// for internal use
void vdmMessageTimeout(VOID);
BOOL expectingVdmResponse(VOID);
void startVdmTimer(INT32 n_pe);
void sendVdmMessageFailed(VOID);
void resetPolicyState(VOID);

BOOL sendVdmMessage(SopType sop, UINT32 * arr, UINT32 length,
		    PolicyState_t next_ps);

#endif // header guard
