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

#include "vdm.h"
#include "../platform.h"
#include "../PDPolicy.h"
#include "../PD_Types.h"
#include "vdm_types.h"
#include "bitfield_translators.h"
#include "fsc_vdm_defs.h"

#include "DisplayPort/dp.h"

// assuming policy state is made elsewhere
extern PolicyState_t PolicyState;
extern UINT32 VdmTimer;
extern BOOL VdmTimerStarted;

VdmManager vdmm;
PolicyState_t originalPolicyState;
BOOL vdm_timeout;
BOOL ExpectingVdmResponse;

// initialize the VDM Manager (no definition/configuration object necessary). returns 0 on success.
INT32 initializeVdm()
{
	vdm_timeout = FALSE;
	ExpectingVdmResponse = FALSE;
	initializeDp();

	return 0;
}

// call this routine to issue Discover Identity commands 
// Discover Identity only valid for SOP/SOP'
// returns 0 if successful
// returns >0 if not SOP or SOP', or if Policy State is wrong 
INT32 requestDiscoverIdentity(SopType sop)
{
	doDataObject_t __vdmh;
	UINT32 __length = 1;
	UINT32 __arr[__length];
	PolicyState_t __n_pe;

	if ((PolicyState == peSinkReady) || (PolicyState == peSourceReady)) {
		// different states for port partner discovery vs cable discovery
		originalPolicyState = PolicyState;
		if (sop == SOP_TYPE_SOP) {
			__n_pe = peDfpUfpVdmIdentityRequest;
		} else if (sop == SOP_TYPE_SOP1) {
			__n_pe = peDfpCblVdmIdentityRequest;
			// TODO: DiscoverIdentityCounter
		} else {
			return 1;
		}

		__vdmh.SVDM.SVID = PD_SID;	// PD SID to be used for Discover Identity command
		__vdmh.SVDM.VDMType = STRUCTURED_VDM;	// structured VDM Header
		__vdmh.SVDM.Version = STRUCTURED_VDM_VERSION;	// version 1.0 = 0
		__vdmh.SVDM.ObjPos = 0;	// does not matter for Discover Identity
		__vdmh.SVDM.CommandType = INITIATOR;	// we are initiating discovery
		__vdmh.SVDM.Command = DISCOVER_IDENTITY;	// discover identity command!

		__arr[0] = __vdmh.object;
		sendVdmMessageWithTimeout(sop, __arr, __length, __n_pe);
	} else if ((sop == SOP_TYPE_SOP1) &&	// allow cable discovery in special earlier states
		   ((PolicyState == peSourceStartup) ||
		    (PolicyState == peSourceDiscovery))) {
		originalPolicyState = PolicyState;
		__n_pe = peSrcVdmIdentityRequest;
		__vdmh.SVDM.SVID = PD_SID;	// PD SID to be used for Discover Identity command
		__vdmh.SVDM.VDMType = STRUCTURED_VDM;	// structured VDM Header
		__vdmh.SVDM.Version = STRUCTURED_VDM_VERSION;	// version 1.0 = 0
		__vdmh.SVDM.ObjPos = 0;	// does not matter for Discover Identity
		__vdmh.SVDM.CommandType = INITIATOR;	// we are initiating discovery
		__vdmh.SVDM.Command = DISCOVER_IDENTITY;	// discover identity command!

		__arr[0] = __vdmh.object;

		sendVdmMessageWithTimeout(sop, __arr, __length, __n_pe);
	} else {
		return 1;
	}

	return 0;
}

// call this routine to issue Discover SVID commands
// Discover SVIDs command valid with SOP*.
INT32 requestDiscoverSvids(SopType sop)
{
	doDataObject_t __vdmh;
	UINT32 __length = 1;
	UINT32 arr[__length];
	PolicyState_t __n_pe;

	if ((PolicyState != peSinkReady) && (PolicyState != peSourceReady)) {
		return 1;
	} else {
		originalPolicyState = PolicyState;
		__n_pe = peDfpVdmSvidsRequest;

		__vdmh.SVDM.SVID = PD_SID;	// PD SID to be used for Discover SVIDs command
		__vdmh.SVDM.VDMType = STRUCTURED_VDM;	// structured VDM Header
		__vdmh.SVDM.Version = STRUCTURED_VDM_VERSION;	// version as defined
		__vdmh.SVDM.ObjPos = 0;	// does not matter for Discover SVIDs
		__vdmh.SVDM.CommandType = INITIATOR;	// we are initiating discovery
		__vdmh.SVDM.Command = DISCOVER_SVIDS;	// Discover SVIDs command!

		arr[0] = __vdmh.object;

		sendVdmMessageWithTimeout(sop, arr, __length, __n_pe);
	}
	return 0;
}

// call this routine to issue Discover Modes
// Discover Modes command valid with SOP*.
INT32 requestDiscoverModes(SopType sop, UINT16 svid)
{
	doDataObject_t __vdmh;
	UINT32 __length = 1;
	UINT32 __arr[__length];
	PolicyState_t __n_pe;

	if ((PolicyState != peSinkReady) && (PolicyState != peSourceReady)) {
		return 1;
	} else {
		originalPolicyState = PolicyState;
		__n_pe = peDfpVdmModesRequest;

		__vdmh.SVDM.SVID = svid;	// Use the SVID that was discovered
		__vdmh.SVDM.VDMType = STRUCTURED_VDM;	// structured VDM Header
		__vdmh.SVDM.Version = STRUCTURED_VDM_VERSION;	// Version as defined
		__vdmh.SVDM.ObjPos = 0;	// does not matter for Discover Modes
		__vdmh.SVDM.CommandType = INITIATOR;	// we are initiating discovery
		__vdmh.SVDM.Command = DISCOVER_MODES;	// Discover MODES command!

		__arr[0] = __vdmh.object;

		sendVdmMessageWithTimeout(sop, __arr, __length, __n_pe);
	}
	return 0;
}

// DPM (UFP) calls this routine to request sending an attention command
INT32 requestSendAttention(SopType sop, UINT16 svid, UINT8 mode)
{
	doDataObject_t __vdmh;
	UINT32 __length = 1;
	UINT32 __arr[__length];

	originalPolicyState = PolicyState;

	if ((PolicyState != peSinkReady) && (PolicyState != peSourceReady)) {
		return 1;
	} else {
		__vdmh.SVDM.SVID = svid;	// Use the SVID that needs attention
		__vdmh.SVDM.VDMType = STRUCTURED_VDM;	// structured VDM Header
		__vdmh.SVDM.Version = STRUCTURED_VDM_VERSION;	// Version as defined
		__vdmh.SVDM.ObjPos = mode;	// use the mode index that needs attention
		__vdmh.SVDM.CommandType = INITIATOR;	// we are initiating attention
		__vdmh.SVDM.Command = ATTENTION;	// Attention command!

		__arr[0] = __vdmh.object;
		__length = 1;
		sendVdmMessage(sop, __arr, __length, PolicyState);
	}
	return 0;
}

INT32 processDiscoverIdentity(SopType sop, UINT32 * arr_in, UINT32 length_in)
{
	doDataObject_t __vdmh_in;
	doDataObject_t __vdmh_out;

	IdHeader __idh;
	CertStatVdo __csvdo;
	Identity __id;
	ProductVdo __pvdo;

	UINT32 __arr[7];
	UINT32 __length;

	BOOL __result;

	__vdmh_in.object = arr_in[0];

	/* Must NAK or not respond to Discover ID with wrong SVID */
	if (__vdmh_in.SVDM.SVID != PD_SID)
		return -1;

	if (__vdmh_in.SVDM.CommandType == INITIATOR) {

		if ((sop == SOP_TYPE_SOP)
		    && ((PolicyState == peSourceReady)
			|| (PolicyState == peSinkReady))) {
			originalPolicyState = PolicyState;
			PolicyState = peUfpVdmGetIdentity;
			__id = vdmm.req_id_info();
			PolicyState = peUfpVdmSendIdentity;
		} else if ((sop == SOP_TYPE_SOP1)
			   && (PolicyState == peCblReady)) {
			originalPolicyState = PolicyState;
			PolicyState = peCblGetIdentity;
			__id = vdmm.req_id_info();

			if (__id.nack) {
				PolicyState = peCblGetIdentityNak;
			} else {
				PolicyState = peCblSendIdentity;
			}
		} else {
			return 1;
		}

		__vdmh_out.SVDM.SVID = PD_SID;	// always use PS_SID for Discover Identity, even on response
		__vdmh_out.SVDM.VDMType = STRUCTURED_VDM;	// Discovery Identity is Structured
		__vdmh_out.SVDM.Version = STRUCTURED_VDM_VERSION;	// stick the vdm version in
		__vdmh_out.SVDM.ObjPos = 0;	// doesn't matter for Discover Identity
		if (__id.nack) {
			__vdmh_out.SVDM.CommandType = RESPONDER_NAK;
		} else {
			__vdmh_out.SVDM.CommandType = RESPONDER_ACK;
		}
		__vdmh_out.SVDM.Command = DISCOVER_IDENTITY;	// Reply with same command, Discover Identity
		__arr[0] = __vdmh_out.object;
		__length = 1;

		if (PolicyState == peCblGetIdentityNak) {
			__vdmh_out.SVDM.CommandType = RESPONDER_NAK;
		} else {
			// put capabilities into ID Header
			__idh = __id.id_header;

			// put test ID into Cert Stat VDO Object
			__csvdo = __id.cert_stat_vdo;

			__arr[1] = getBitsForIdHeader(__idh);
			__length++;
			__arr[2] = getBitsForCertStatVdo(__csvdo);
			__length++;

			// Product VDO should be sent for all
			__pvdo = __id.product_vdo;
			__arr[__length] = getBitsForProductVdo(__pvdo);
			__length++;

			// Cable VDO should be sent when we are a Passive Cable or Active Cable
			if ((__idh.product_type == PASSIVE_CABLE)
			    || (__idh.product_type == ACTIVE_CABLE)) {
				CableVdo cvdo_out;
				cvdo_out = __id.cable_vdo;
				__arr[__length] = getBitsForCableVdo(cvdo_out);
				__length++;
			}
			// AMA VDO should be sent when we are an AMA!
			if (__idh.product_type == AMA) {
				AmaVdo amavdo_out;
				amavdo_out = __id.ama_vdo;

				__arr[__length] = getBitsForAmaVdo(amavdo_out);
				__length++;
			}
		}

		sendVdmMessage(sop, __arr, __length, originalPolicyState);
		return 0;
	} else {		/* Incoming responses, ACKs, NAKs, BUSYs */
		if (vdm_timeout)
			return 0;
		if ((PolicyState != peDfpUfpVdmIdentityRequest) &&
		    (PolicyState != peDfpCblVdmIdentityRequest) &&
		    (PolicyState != peSrcVdmIdentityRequest))
			return 0;

		// Discover Identity responses should have at least VDM Header, ID Header, and Cert Stat VDO
		if (length_in < MIN_DISC_ID_RESP_SIZE) {
			PolicyState = originalPolicyState;
			return 1;
		}
		if ((PolicyState == peDfpUfpVdmIdentityRequest)
		    && (sop == SOP_TYPE_SOP)) {
			if (__vdmh_in.SVDM.CommandType == RESPONDER_ACK) {
				PolicyState = peDfpUfpVdmIdentityAcked;
			} else {
				PolicyState = peDfpUfpVdmIdentityNaked;
			}
		} else if ((PolicyState == peDfpCblVdmIdentityRequest)
			   && (sop == SOP_TYPE_SOP1)) {
			if (__vdmh_in.SVDM.CommandType == RESPONDER_ACK) {
				PolicyState = peDfpCblVdmIdentityAcked;
			} else {
				PolicyState = peDfpCblVdmIdentityNaked;
			}
		} else if ((PolicyState == peSrcVdmIdentityRequest)
			   && (sop == SOP_TYPE_SOP1)) {
			if (__vdmh_in.SVDM.CommandType == RESPONDER_ACK) {
				PolicyState = peSrcVdmIdentityAcked;
			} else {
				PolicyState = peSrcVdmIdentityNaked;
			}
		} else {
			// TODO: something weird happened.
		}

		if (__vdmh_in.SVDM.CommandType == RESPONDER_ACK) {
			__id.id_header = getIdHeader(arr_in[1]);
			__id.cert_stat_vdo = getCertStatVdo(arr_in[2]);

			if ((__id.id_header.product_type == HUB) ||
			    (__id.id_header.product_type == PERIPHERAL) ||
			    (__id.id_header.product_type == AMA)) {
				__id.has_product_vdo = TRUE;
				__id.product_vdo = getProductVdo(arr_in[3]);	// !!! assuming it is before AMA VDO
			}

			if ((__id.id_header.product_type == PASSIVE_CABLE) ||
			    (__id.id_header.product_type == ACTIVE_CABLE)) {
				__id.has_cable_vdo = TRUE;
				__id.cable_vdo = getCableVdo(arr_in[3]);
			}

			if ((__id.id_header.product_type == AMA)) {
				__id.has_ama_vdo = TRUE;
				__id.ama_vdo = getAmaVdo(arr_in[4]);	// !!! assuming it is after Product VDO
			}
		}

		__result = (PolicyState == peDfpUfpVdmIdentityAcked) ||
		    (PolicyState == peDfpCblVdmIdentityAcked) ||
		    (PolicyState == peSrcVdmIdentityAcked);
		vdmm.inform_id(__result, sop, __id);
		ExpectingVdmResponse = FALSE;
		PolicyState = originalPolicyState;
		VdmTimer = 0;
		VdmTimerStarted = FALSE;
		return 0;
	}
}

INT32 processDiscoverSvids(SopType sop, UINT32 * arr_in, UINT32 length_in)
{
	doDataObject_t __vdmh_in;
	doDataObject_t __vdmh_out;

	SvidInfo __svid_info;

	UINT32 __i;
	UINT16 __top16;
	UINT16 __bottom16;

	UINT32 __arr[7];
	UINT32 __length;

	__vdmh_in.object = arr_in[0];

	/* Must NAK or not respond to Discover SVIDs with wrong SVID */
	if (__vdmh_in.SVDM.SVID != PD_SID)
		return -1;

	if (__vdmh_in.SVDM.CommandType == INITIATOR) {
		if ((sop == SOP_TYPE_SOP)
		    && ((PolicyState == peSourceReady)
			|| (PolicyState == peSinkReady))) {
			originalPolicyState = PolicyState;
			// assuming that the splitting of SVID info is done outside this block
			__svid_info = vdmm.req_svid_info();
			PolicyState = peUfpVdmGetSvids;
			PolicyState = peUfpVdmSendSvids;
		} else if ((sop == SOP_TYPE_SOP1)
			   && (PolicyState == peCblReady)) {
			originalPolicyState = PolicyState;
			PolicyState = peCblGetSvids;
			__svid_info = vdmm.req_svid_info();
			if (__svid_info.nack) {
				PolicyState = peCblGetSvidsNak;
			} else {
				PolicyState = peCblSendSvids;
			}
		} else {
			return 1;
		}

		__vdmh_out.SVDM.SVID = PD_SID;	// always use PS_SID for Discover SVIDs, even on response
		__vdmh_out.SVDM.VDMType = STRUCTURED_VDM;	// Discovery SVIDs is Structured
		__vdmh_out.SVDM.Version = STRUCTURED_VDM_VERSION;	// stick the vdm version in
		__vdmh_out.SVDM.ObjPos = 0;	// doesn't matter for Discover SVIDs
		if (__svid_info.nack) {
			__vdmh_out.SVDM.CommandType = RESPONDER_NAK;
		} else {
			__vdmh_out.SVDM.CommandType = RESPONDER_ACK;
		}
		__vdmh_out.SVDM.Command = DISCOVER_SVIDS;	// Reply with same command, Discover SVIDs

		__length = 0;
		__arr[__length] = __vdmh_out.object;
		__length++;

		if (PolicyState == peCblGetSvidsNak) {
			__vdmh_out.SVDM.CommandType = RESPONDER_NAK;
		} else {
			UINT32 i;
			// prevent segfaults
			if (__svid_info.num_svids > MAX_NUM_SVIDS) {
				PolicyState = originalPolicyState;
				return 1;
			}

			for (i = 0; i < __svid_info.num_svids; i++) {
				// check if i is even
				if (!(i & 0x1)) {
					__length++;

					// setup new word to send
					__arr[__length - 1] = 0;

					// if even, shift SVID up to the top 16 bits
					__arr[__length - 1] |=
					    __svid_info.svids[i];
					__arr[__length - 1] <<= 16;
				} else {
					// if odd, fill out the bottom 16 bits
					__arr[__length - 1] |=
					    __svid_info.svids[i];
				}
			}
		}

		sendVdmMessage(sop, __arr, __length, originalPolicyState);
		return 0;
	} else {		/* Incoming responses, ACKs, NAKs, BUSYs */
		__svid_info.num_svids = 0;

		if (PolicyState != peDfpVdmSvidsRequest) {
			return 1;
		} else if (__vdmh_in.SVDM.CommandType == RESPONDER_ACK) {
			for (__i = 1; __i < length_in; __i++) {
				__top16 = (arr_in[__i] >> 16) & 0x0000FFFF;
				__bottom16 = (arr_in[__i] >> 0) & 0x0000FFFF;

				// if top 16 bits are 0, we're done getting SVIDs
				if (__top16 == 0x0000) {
					break;
				} else {
					__svid_info.svids[2 * (__i - 1)] =
					    __top16;
					__svid_info.num_svids += 1;
				}
				// if bottom 16 bits are 0 we're done getting SVIDs
				if (__bottom16 == 0x0000) {
					break;
				} else {
					__svid_info.svids[2 * (__i - 1) + 1] =
					    __bottom16;
					__svid_info.num_svids += 1;
				}
			}
			PolicyState = peDfpVdmSvidsAcked;
		} else {
			PolicyState = peDfpVdmSvidsNaked;
		}

		vdmm.inform_svids(PolicyState == peDfpVdmSvidsAcked,
				  sop, __svid_info);

		ExpectingVdmResponse = FALSE;
		VdmTimer = 0;
		VdmTimerStarted = FALSE;
		PolicyState = originalPolicyState;
		return 0;
	}
}

INT32 processDiscoverModes(SopType sop, UINT32 * arr_in, UINT32 length_in)
{
	doDataObject_t __vdmh_in;
	doDataObject_t __vdmh_out;

	ModesInfo __modes_info = { 0 };

	UINT32 __i;
	UINT32 __arr[7];
	UINT32 __length;

	__vdmh_in.object = arr_in[0];
	if (__vdmh_in.SVDM.CommandType == INITIATOR) {
		if ((sop == SOP_TYPE_SOP)
		    && ((PolicyState == peSourceReady)
			|| (PolicyState == peSinkReady))) {
			originalPolicyState = PolicyState;
			__modes_info = vdmm.req_modes_info(__vdmh_in.SVDM.SVID);
			PolicyState = peUfpVdmGetModes;
			PolicyState = peUfpVdmSendModes;
		} else if ((sop == SOP_TYPE_SOP1)
			   && (PolicyState == peCblReady)) {
			originalPolicyState = PolicyState;
			PolicyState = peCblGetModes;
			__modes_info = vdmm.req_modes_info(__vdmh_in.SVDM.SVID);

			if (__modes_info.nack) {
				PolicyState = peCblGetModesNak;
			} else {
				PolicyState = peCblSendModes;
			}
		} else {
			return 1;
		}

		__vdmh_out.SVDM.SVID = __vdmh_in.SVDM.SVID;	// reply with SVID we're being asked about
		__vdmh_out.SVDM.VDMType = STRUCTURED_VDM;	// Discovery Modes is Structured
		__vdmh_out.SVDM.Version = STRUCTURED_VDM_VERSION;	// stick the vdm version in
		__vdmh_out.SVDM.ObjPos = 0;	// doesn't matter for Discover Modes
		if (__modes_info.nack) {
			__vdmh_out.SVDM.CommandType = RESPONDER_NAK;
		} else {
			__vdmh_out.SVDM.CommandType = RESPONDER_ACK;
		}
		__vdmh_out.SVDM.Command = DISCOVER_MODES;	// Reply with same command, Discover Modes

		__length = 0;
		__arr[__length] = __vdmh_out.object;
		__length++;

		if (PolicyState == peCblGetModesNak) {
			__vdmh_out.SVDM.CommandType = RESPONDER_NAK;
		} else {
			INT32 j;

			for (j = 0; j < __modes_info.num_modes; j++) {
				__arr[j + 1] = __modes_info.modes[j];
				__length++;
			}
		}

		sendVdmMessage(sop, __arr, __length, originalPolicyState);
		return 0;
	} else {		/* Incoming responses, ACKs, NAKs, BUSYs */
		if (PolicyState != peDfpVdmModesRequest) {
			return 1;
		} else {
			if (__vdmh_in.SVDM.CommandType == RESPONDER_ACK) {
				__modes_info.svid = __vdmh_in.SVDM.SVID;
				__modes_info.num_modes = length_in - 1;

				for (__i = 1; __i < length_in; __i++) {
					__modes_info.modes[__i - 1] =
					    arr_in[__i];
				}
				PolicyState = peDfpVdmModesAcked;
			} else {
				PolicyState = peDfpVdmModesNaked;
			}

			vdmm.inform_modes(PolicyState == peDfpVdmModesAcked,
					  sop, __modes_info);
			ExpectingVdmResponse = FALSE;
			VdmTimer = 0;
			VdmTimerStarted = FALSE;
			PolicyState = originalPolicyState;
		}

		return 0;
	}
}

INT32 processEnterMode(SopType sop, UINT32 * arr_in, UINT32 length_in)
{
	doDataObject_t __svdmh_in;
	doDataObject_t __svdmh_out;

	BOOL __mode_entered;
	UINT32 __arr_out[7];
	UINT32 __length_out;

	__svdmh_in.object = arr_in[0];
	if (__svdmh_in.SVDM.CommandType == INITIATOR) {
		if ((sop == SOP_TYPE_SOP)
		    && ((PolicyState == peSourceReady)
			|| (PolicyState == peSinkReady))) {
			originalPolicyState = PolicyState;
			PolicyState = peUfpVdmEvaluateModeEntry;
			__mode_entered =
			    vdmm.req_mode_entry(__svdmh_in.SVDM.SVID,
						__svdmh_in.SVDM.ObjPos);

			// if DPM says OK, respond with ACK
			if (__mode_entered) {
				PolicyState = peUfpVdmModeEntryAck;
				__svdmh_out.SVDM.CommandType = RESPONDER_ACK;	// entered mode successfully
			} else {
				PolicyState = peUfpVdmModeEntryNak;
				__svdmh_out.SVDM.CommandType = RESPONDER_NAK;	// NAK if mode not entered
			}
		} else if ((sop == SOP_TYPE_SOP1)
			   && (PolicyState == peCblReady)) {
			originalPolicyState = PolicyState;
			PolicyState = peCblEvaluateModeEntry;
			__mode_entered =
			    vdmm.req_mode_entry(__svdmh_in.SVDM.SVID,
						__svdmh_in.SVDM.ObjPos);
			// if DPM says OK, respond with ACK
			if (__mode_entered) {
				PolicyState = peCblModeEntryAck;
				__svdmh_out.SVDM.CommandType = RESPONDER_ACK;	// entered mode successfully
			} else {
				PolicyState = peCblModeEntryNak;
				__svdmh_out.SVDM.CommandType = RESPONDER_NAK;	// NAK if mode not entered
			}
		} else {
			return 1;
		}

		// most of the message response will be the same whether we entered the mode or not
		__svdmh_out.SVDM.SVID = __svdmh_in.SVDM.SVID;	// reply with SVID we're being asked about
		__svdmh_out.SVDM.VDMType = STRUCTURED_VDM;	// Enter Mode is Structured
		__svdmh_out.SVDM.Version = STRUCTURED_VDM_VERSION;	// stick the vdm version in
		__svdmh_out.SVDM.ObjPos = __svdmh_in.SVDM.ObjPos;	// reflect the object position
		__svdmh_out.SVDM.Command = ENTER_MODE;	// Reply with same command, Enter Mode

		__arr_out[0] = __svdmh_out.object;
		__length_out = 1;

		sendVdmMessage(sop, __arr_out, __length_out,
			       originalPolicyState);
		return 0;
	} else {		/* Incoming responses, ACKs, NAKs, BUSYs */
		if (__svdmh_in.SVDM.CommandType != RESPONDER_ACK) {
			PolicyState = peDfpVdmModeEntryNaked;
			vdmm.enter_mode_result(FALSE, __svdmh_in.SVDM.SVID,
					       __svdmh_in.SVDM.ObjPos);
		} else {
			PolicyState = peDfpVdmModeEntryAcked;
			vdmm.enter_mode_result(TRUE, __svdmh_in.SVDM.SVID,
					       __svdmh_in.SVDM.ObjPos);
		}
		PolicyState = originalPolicyState;
		ExpectingVdmResponse = FALSE;
		return 0;
	}
}

INT32 processExitMode(SopType sop, UINT32 * arr_in, UINT32 length_in)
{
	doDataObject_t __vdmh_in;
	doDataObject_t __vdmh_out;

	BOOL __mode_exited;
	UINT32 __arr[7];
	UINT32 __length;

	__vdmh_in.object = arr_in[0];
	if (__vdmh_in.SVDM.CommandType == INITIATOR) {
		if ((sop == SOP_TYPE_SOP)
		    && ((PolicyState == peSourceReady)
			|| (PolicyState == peSinkReady))) {
			originalPolicyState = PolicyState;
			PolicyState = peUfpVdmModeExit;
			__mode_exited =
			    vdmm.req_mode_exit(__vdmh_in.SVDM.SVID,
					       __vdmh_in.SVDM.ObjPos);

			// if DPM says OK, respond with ACK
			if (__mode_exited) {
				PolicyState = peUfpVdmModeExitAck;
				__vdmh_out.SVDM.CommandType = RESPONDER_ACK;	// exited mode successfully
			} else {
				PolicyState = peUfpVdmModeExitNak;
				__vdmh_out.SVDM.CommandType = RESPONDER_NAK;	// NAK if mode not exited
			}
		} else if ((sop == SOP_TYPE_SOP1)
			   && (PolicyState == peCblReady)) {
			originalPolicyState = PolicyState;
			PolicyState = peCblModeExit;
			__mode_exited =
			    vdmm.req_mode_exit(__vdmh_in.SVDM.SVID,
					       __vdmh_in.SVDM.ObjPos);

			// if DPM says OK, respond with ACK
			if (__mode_exited) {
				PolicyState = peCblModeExitAck;
				__vdmh_out.SVDM.CommandType = RESPONDER_ACK;	// exited mode successfully
			} else {
				PolicyState = peCblModeExitNak;
				__vdmh_out.SVDM.CommandType = RESPONDER_NAK;	// NAK if mode not exited
			}
		} else {
			return 1;
		}

		// most of the message response will be the same whether we exited the mode or not
		__vdmh_out.SVDM.SVID = __vdmh_in.SVDM.SVID;	// reply with SVID we're being asked about
		__vdmh_out.SVDM.VDMType = STRUCTURED_VDM;	// Exit Mode is Structured
		__vdmh_out.SVDM.Version = STRUCTURED_VDM_VERSION;	// stick the vdm version in
		__vdmh_out.SVDM.ObjPos = __vdmh_in.SVDM.ObjPos;	// reflect the object position
		__vdmh_out.SVDM.Command = EXIT_MODE;	// Reply with same command, Exit Mode

		__arr[0] = __vdmh_out.object;
		__length = 1;

		sendVdmMessage(sop, __arr, __length, originalPolicyState);
		return 0;
	} else {
		if (__vdmh_in.SVDM.CommandType != RESPONDER_ACK) {
			vdmm.exit_mode_result(FALSE, __vdmh_in.SVDM.SVID,
					      __vdmh_in.SVDM.ObjPos);
			// when exit mode not ACKed, go to hard reset state!
			if (originalPolicyState == peSourceReady) {
				PolicyState = peSourceHardReset;
			} else if (originalPolicyState == peSinkReady) {
				PolicyState = peSinkHardReset;
			} else {
				// TODO: should never reach here, but you never know...
			}
		} else {
			PolicyState = peDfpVdmExitModeAcked;
			vdmm.exit_mode_result(TRUE, __vdmh_in.SVDM.SVID,
					      __vdmh_in.SVDM.ObjPos);
			PolicyState = originalPolicyState;

			VdmTimer = 0;
			VdmTimerStarted = FALSE;
		}
		ExpectingVdmResponse = FALSE;
		return 0;
	}
}

INT32 processAttention(SopType sop, UINT32 * arr_in, UINT32 length_in)
{
	doDataObject_t __vdmh_in;
	__vdmh_in.object = arr_in[0];

	originalPolicyState = PolicyState;
	PolicyState = peDfpVdmAttentionRequest;
	vdmm.inform_attention(__vdmh_in.SVDM.SVID, __vdmh_in.SVDM.ObjPos);
	PolicyState = originalPolicyState;

	return 0;
}

INT32 processSvidSpecific(SopType sop, UINT32 * arr_in, UINT32 length_in)
{
	doDataObject_t __vdmh_out;
	doDataObject_t __vdmh_in;
	UINT32 __arr[7];
	UINT32 __length;

	__vdmh_in.object = arr_in[0];
	if (__vdmh_in.SVDM.SVID == DP_SID) {
		if (!processDpCommand(arr_in)) {
			return 0;	/* DP code will send response, so return */
		}
	}
	// in this case the command is unrecognized. Reply with a NAK.
	__vdmh_out.SVDM.SVID = __vdmh_in.SVDM.SVID;	// reply with SVID we received
	__vdmh_out.SVDM.VDMType = STRUCTURED_VDM;	// All are structured in this switch-case
	__vdmh_out.SVDM.Version = STRUCTURED_VDM_VERSION;	// stick the vdm version in
	__vdmh_out.SVDM.ObjPos = 0;	// value doesn't matter here
	__vdmh_out.SVDM.CommandType = RESPONDER_NAK;	// Command unrecognized, so NAK
	__vdmh_out.SVDM.Command = __vdmh_in.SVDM.Command;	// Reply with same command

	__arr[0] = __vdmh_out.object;
	__length = 1;

	sendVdmMessage(sop, __arr, __length, originalPolicyState);
	return 0;
}

// returns 0 on success, 1+ otherwise
INT32 processVdmMessage(SopType sop, UINT32 * arr_in, UINT32 length_in)
{
	doDataObject_t __vdmh_in;

	__vdmh_in.object = arr_in[0];
	if (__vdmh_in.SVDM.VDMType == STRUCTURED_VDM) {

		// different actions for different commands
		switch (__vdmh_in.SVDM.Command) {
		case DISCOVER_IDENTITY:
			return processDiscoverIdentity(sop, arr_in, length_in);
			break;
		case DISCOVER_SVIDS:
			return processDiscoverSvids(sop, arr_in, length_in);
			break;
		case DISCOVER_MODES:
			return processDiscoverModes(sop, arr_in, length_in);
			break;
		case ENTER_MODE:
			return processEnterMode(sop, arr_in, length_in);
			break;
		case EXIT_MODE:
			return processExitMode(sop, arr_in, length_in);
			break;
		case ATTENTION:
			return processAttention(sop, arr_in, length_in);
			break;
		default:
			// SVID-Specific commands go here
			return processSvidSpecific(sop, arr_in, length_in);
			break;
		}
	} else {
		// TODO: Unstructured messages
		return 1;
	}

	return 0;
}

// call this function to enter a mode
INT32 requestEnterMode(SopType sop, UINT16 svid, UINT32 mode_index)
{
	doDataObject_t __vdmh;
	UINT32 __length = 1;
	UINT32 __arr[__length];
	PolicyState_t __n_pe;

	if ((PolicyState != peSinkReady) && (PolicyState != peSourceReady)) {
		return 1;
	} else {
		originalPolicyState = PolicyState;
		__n_pe = peDfpVdmModeEntryRequest;

		__vdmh.SVDM.SVID = svid;	// Use SVID specified upon function call
		__vdmh.SVDM.VDMType = STRUCTURED_VDM;	// structured VDM Header
		__vdmh.SVDM.Version = STRUCTURED_VDM_VERSION;	// version 1.0 = 0
		__vdmh.SVDM.ObjPos = mode_index;	// select mode
		__vdmh.SVDM.CommandType = INITIATOR;	// we are initiating mode entering
		__vdmh.SVDM.Command = ENTER_MODE;	// Enter Mode command!

		__arr[0] = __vdmh.object;
		sendVdmMessageWithTimeout(sop, __arr, __length, __n_pe);
	}
	return 0;
}

// call this function to exit a mode
INT32 requestExitMode(SopType sop, UINT16 svid, UINT32 mode_index)
{
	doDataObject_t __vdmh;
	UINT32 __length = 1;
	UINT32 __arr[__length];
	PolicyState_t __n_pe;

	if ((PolicyState != peSinkReady) && (PolicyState != peSourceReady)) {
		return 1;
	} else {
		originalPolicyState = PolicyState;
		__n_pe = peDfpVdmModeExitRequest;

		__vdmh.SVDM.SVID = svid;	// Use SVID specified upon function call
		__vdmh.SVDM.VDMType = STRUCTURED_VDM;	// structured VDM Header
		__vdmh.SVDM.Version = STRUCTURED_VDM_VERSION;	// version 1.0 = 0
		__vdmh.SVDM.ObjPos = mode_index;	// select mode
		__vdmh.SVDM.CommandType = INITIATOR;	// we are initiating mode entering
		__vdmh.SVDM.Command = EXIT_MODE;	// Exit Mode command!

		__arr[0] = __vdmh.object;
		sendVdmMessageWithTimeout(sop, __arr, __length, __n_pe);
	}
	return 0;
}

// call this function to send an attention command to specified SOP type
INT32 sendAttention(SopType sop, UINT32 obj_pos)
{
	// TODO
	return 1;
}

void sendVdmMessageWithTimeout(SopType sop, UINT32 * arr, UINT32 length,
			       INT32 n_pe)
{
	sendVdmMessage(sop, arr, length, n_pe);
	ExpectingVdmResponse = TRUE;
}

BOOL expectingVdmResponse(VOID)
{
	return ExpectingVdmResponse;
}

void startVdmTimer(INT32 n_pe)
{
	// start the appropriate timer
	switch (n_pe) {
	case peDfpUfpVdmIdentityRequest:
	case peDfpCblVdmIdentityRequest:
	case peSrcVdmIdentityRequest:
	case peDfpVdmSvidsRequest:
	case peDfpVdmModesRequest:
		VdmTimer = tVDMSenderResponse;
		VdmTimerStarted = TRUE;
		break;
	case peDfpVdmModeEntryRequest:
		VdmTimer = tVDMWaitModeEntry;
		VdmTimerStarted = TRUE;
		break;
	case peDfpVdmModeExitRequest:
		VdmTimer = tVDMWaitModeExit;
		VdmTimerStarted = TRUE;
		break;
	case peDpRequestStatus:
		VdmTimer = tVDMSenderResponse;
		VdmTimerStarted = TRUE;
		break;
	default:
		VdmTimer = 0;
		VdmTimerStarted = TRUE;	// timeout immediately
		return;
	}
}

void sendVdmMessageFailed()
{
	resetPolicyState();
}

// call this function when VDM Message Timer expires
void vdmMessageTimeout(VOID)
{
	resetPolicyState();
}

void resetPolicyState(VOID)
{
	Identity __id = { 0 };	// fake empty id Discover Identity for NAKs
	SvidInfo __svid_info = { 0 };	// etc.
	ModesInfo __modes_info = { 0 };	// etc..

	ExpectingVdmResponse = FALSE;
	VdmTimerStarted = FALSE;

	switch (PolicyState) {
	case peDfpUfpVdmIdentityRequest:
		PolicyState = peDfpUfpVdmIdentityNaked;
		vdmm.inform_id(FALSE, SOP_TYPE_SOP, __id);	// informing of a NAK
		PolicyState = originalPolicyState;
		break;
	case peDfpCblVdmIdentityRequest:
		PolicyState = peDfpCblVdmIdentityNaked;
		vdmm.inform_id(FALSE, SOP_TYPE_SOP1, __id);	// informing of a NAK from cable
		PolicyState = originalPolicyState;
		break;
	case peDfpVdmSvidsRequest:
		PolicyState = peDfpVdmSvidsNaked;
		vdmm.inform_svids(FALSE, SOP_TYPE_SOP, __svid_info);
		PolicyState = originalPolicyState;
		break;
	case peDfpVdmModesRequest:
		PolicyState = peDfpVdmModesNaked;
		vdmm.inform_modes(FALSE, SOP_TYPE_SOP, __modes_info);
		PolicyState = originalPolicyState;
		break;
	case peDfpVdmModeEntryRequest:
		PolicyState = peDfpVdmModeEntryNaked;
		vdmm.enter_mode_result(FALSE, 0, 0);
		PolicyState = originalPolicyState;
		break;
	case peDfpVdmModeExitRequest:
		vdmm.exit_mode_result(FALSE, 0, 0);

		// if Mode Exit request is NAKed, go to hard reset state!
		if (originalPolicyState == peSinkReady) {
			PolicyState = peSinkHardReset;
		} else if (originalPolicyState == peSourceReady) {
			PolicyState = peSourceHardReset;
		} else {
			// TODO: should never reach here, but...
		}
		PolicyState = originalPolicyState;
		return;
	case peSrcVdmIdentityRequest:
		PolicyState = peSrcVdmIdentityNaked;
		vdmm.inform_id(FALSE, SOP_TYPE_SOP1, __id);	// informing of a NAK from cable
		PolicyState = originalPolicyState;
		break;
	default:
		PolicyState = originalPolicyState;
		break;
	}
}
