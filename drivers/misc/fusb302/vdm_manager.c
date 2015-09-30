#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <mach/mt_pm_ldo.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <cust_eint.h>
#include <mach/eint.h>
#include <cust_eint.h>
#include <linux/kthread.h>
#include <mach/mt_gpio.h>

#include "vdm_types.h"
#include "vdm_manager.h"
#include "vdm_bitfield_translators.h"
#include "fsc_vdm_defs.h"
#include "usbpd.h" // get visibility into the system

// initialize the VDM Manager. returns 0 on success.
int initialize_with_def (VdmManager* vdmm, VendorDefinition vdef) {
    initialize(vdmm);
    vdmm->vdef = vdef;
    return 0;
}

// initialize the VDM Manager (no definition/configuration object necessary). returns 0 on success.
int initialize (VdmManager* vdmm) {
    int i;
	
    // ensure modes are cleared
    for (i = 0; i < NUM_VDM_MODES; i++)
        vdmm->mode_status[i] = false;

    vdmm->num_sop_svids = 0;
    vdmm->num_sop1_svids = 0;
    vdmm->num_sop2_svids = 0;
	
    vdmm->last_svid_sent = -1;

    for (i = 0; i < MAX_NUM_SVIDS_PER_SOP; i++) {
        vdmm->num_sop_modes_per_svid[i] = 0;
        vdmm->num_sop1_modes_per_svid[i] = 0;
        vdmm->num_sop2_modes_per_svid[i] = 0;
    }
    return 0;
}

// call this routine to issue Discover Identity commands 
// Discover Identity only valid for SOP/SOP'
int discoverIdentities (VdmManager* vdmm, SopType sop) {
	StructuredVdmHeader svdmh;
	unsigned int length = 1;
	u32 arr[length];
	
	// TODO: Clear previously discovered Identities.
	// Should we also clear previously discovered SVIDs and Modes?
	
	// TODO/FIXME: May want to add an SopType argument and only discover a single identity at a time.
	
	svdmh.svid 		= PD_SID; 					// PD SID to be used for Discover Identity command
	svdmh.vdm_type 		= STRUCTURED_VDM;			// structured VDM Header
	svdmh.svdm_version 	= STRUCTURED_VDM_VERSION;	// version 1.0 = 0
	svdmh.obj_pos 		= 0;						// does not matter for Discover Identity
	svdmh.cmd_type 		= INITIATOR;				// we are initiating discovery
	svdmh.command 		= DISCOVER_IDENTITY;		// discover identity command!
	
	arr[0] = getBitsForStructuredVdmHeader(svdmh);
	
	sendVdmMessage(vdmm, sop, arr, length);
	// TODO: Wait for response
	
	return 0;
}

// call this routine to issue Discover SVID commands
// Discover SVIDs command valid with SOP*.
int discoverSvids (VdmManager* vdmm, SopType sop) {
	StructuredVdmHeader svdmh;
	unsigned int length = 1;
	u32 arr[length];
	
	// TODO: Clear previously discovered SVIDs and Modes.
	// TODO: Validate sop is SOP, SOP' or SOP''

	// TODO/FIXME: May want to add an SopType argument and only discover one set of SVIDs at a time.
	
	svdmh.svid 		= PD_SID; 					// PD SID to be used for Discover SVIDs command
	svdmh.vdm_type 		= STRUCTURED_VDM;			// structured VDM Header
	svdmh.svdm_version 	= STRUCTURED_VDM_VERSION;	// version as defined
	svdmh.obj_pos 		= 0;						// does not matter for Discover SVIDs
	svdmh.cmd_type 		= INITIATOR;				// we are initiating discovery
	svdmh.command 		= DISCOVER_SVIDS;			// Discover SVIDs command!
	
	arr[0] = getBitsForStructuredVdmHeader(svdmh);
	
	sendVdmMessage(vdmm, sop, arr, length);
	// TODO: Wait for response

	return 0;
}

// call this routine to issue Discover Modes
// Discover Modes command valid with SOP*.
int discoverModes(VdmManager* vdmm, SopType sop, u16 svid) {
	StructuredVdmHeader svdmh;
	unsigned int length = 1;
	u32 arr[length];
	
	// TODO: Clear previously discovered Modes?
	
	// TODO: PD Spec says only send Discover Modes for SVIDs that you support.
	// Right now it's sending out Discover Modes for ALL SVIDs discovered.
	// Need a way to determine what SVIDs we support. - Gabe
	
	// TODO: Validate that sop is SOP, SOP', or SOP'' 
		
	svdmh.svid 			= svid; 					// Use the SVID that was discovered
	svdmh.vdm_type 		= STRUCTURED_VDM;			// structured VDM Header
	svdmh.svdm_version 	= STRUCTURED_VDM_VERSION;	// Version as defined
	svdmh.obj_pos 		= 0;						// does not matter for Discover Modes
	svdmh.cmd_type 		= INITIATOR;				// we are initiating discovery
	svdmh.command 		= DISCOVER_MODES;			// Discover MODES command!
	
	arr[0] = getBitsForStructuredVdmHeader(svdmh);
	length = 1;
	sendVdmMessage(vdmm, sop, arr, length);
	// TODO: Wait for response...
	
	
	/*
	// BELOW: Old code that would just blast out Discover Modes messages.
	// TODO/FIXME: Remove if not necessary to automate.
	// TODO: Slim down this code. A lot of repetition here. - Gabe
	int i;
	// Send out SOP messages
	for (i = 0; i < (*vdmm).num_sop_svids; i++) {
		svdmh.svid 			= (*vdmm).sop_svids[i]; 	// Use the SVID that was discovered
		svdmh.vdm_type 		= STRUCTURED_VDM;			// structured VDM Header
		svdmh.svdm_version 	= STRUCTURED_VDM_VERSION;	// Version as defined
		svdmh.obj_pos 		= 0;						// does not matter for Discover Modes
		svdmh.cmd_type 		= INITIATOR;				// we are initiating discovery
		svdmh.command 		= DISCOVER_MODES;			// Discover MODES command!
	
		arr[0] = getBitsForStructuredVdmHeader(svdmh);
		length = 1;
		sendVdmMessage(vdmm, SOP, arr, length);
		// TODO: Wait for response...
	}
	
	// Send out SOP1 messages
	for (i = 0; i < (*vdmm).num_sop1_svids; i++) {
		svdmh.svid 			= (*vdmm).sop1_svids[i]; 	// Use the SVID that was discovered
		svdmh.vdm_type 		= STRUCTURED_VDM;			// structured VDM Header
		svdmh.svdm_version 	= STRUCTURED_VDM_VERSION;	// Version as defined
		svdmh.obj_pos 		= 0;						// does not matter for Discover Modes
		svdmh.cmd_type 		= INITIATOR;				// we are initiating discovery
		svdmh.command 		= DISCOVER_MODES;			// Discover MODES command!
	
		arr[0] = getBitsForStructuredVdmHeader(svdmh);
		length = 1;
		sendVdmMessage(vdmm, SOP1, arr, length);
		// TODO: Wait for response...
	}
	
	// Send out SOP2 messages
	for (i = 0; i < (*vdmm).num_sop2_svids; i++) {
		svdmh.svid 			= (*vdmm).sop2_svids[i]; 	// Use the SVID that was discovered
		svdmh.vdm_type 		= STRUCTURED_VDM;			// structured VDM Header
		svdmh.svdm_version 	= STRUCTURED_VDM_VERSION;	// Version as defined
		svdmh.obj_pos 		= 0;						// does not matter for Discover Modes
		svdmh.cmd_type 		= INITIATOR;				// we are initiating discovery
		svdmh.command 		= DISCOVER_MODES;			// Discover MODES command!
	
		arr[0] = getBitsForStructuredVdmHeader(svdmh);
		length = 1;
		sendVdmMessage(vdmm, SOP2, arr, length);
		// TODO: Wait for response...
	}
	*/
	
	return 0;
}

// returns 0 on success, 1+ otherwise
int processVdmMessage	(VdmManager* vdmm, SopType sop, u32* arr_in, unsigned int length_in) {
	StructuredVdmHeader 	svdmh_in;
	UnstructuredVdmHeader 	uvdmh_in;
	VdmType 		vdm_type_in;
	
	StructuredVdmHeader 	svdmh_out;
	UnstructuredVdmHeader 	uvdmh_out;
	IdHeader		idh_out;
	CertStatVdo		csvdo_out;
	u32 			arr_out[7];
	unsigned int		length_out;
	
	// TODO: Error checking on input (ex. bad values of length)
	
	vdm_type_in = getVdmTypeOf(arr_in[0]);
	
	if (vdm_type_in == STRUCTURED_VDM) {
		svdmh_in = getStructuredVdmHeader(arr_in[0]);
		
		// switch on command type, tell us if this is a response or if should be responding
		switch (svdmh_in.cmd_type) {
			case INITIATOR		:
				// different actions for different commands
				switch (svdmh_in.command) {
					case DISCOVER_IDENTITY:
                                        {
						Identity my_id;
						svdmh_out.svid 			= PD_SID;					// always use PS_SID for Discover Identity, even on response
						svdmh_out.vdm_type 		= STRUCTURED_VDM;			// Discovery Identity is Structured
						svdmh_out.svdm_version	= STRUCTURED_VDM_VERSION;	// stick the vdm version in
						svdmh_out.obj_pos		= 0;						// doesn't matter for Discover Identity
						svdmh_out.cmd_type		= RESPONDER_ACK;			// TODO: When to NAK or be BUSY?
						svdmh_out.command		= DISCOVER_IDENTITY;		// Reply with same command, Discover Identity
								
						my_id = vdmm->req_id_info();
						
						// put capabilities into ID Header
						idh_out = my_id.id_header;
									
						// put test ID into Cert Stat VDO Object
						csvdo_out = my_id.cert_stat_vdo;
						
						arr_out[0] = getBitsForStructuredVdmHeader(svdmh_out);
						arr_out[1] = getBitsForIdHeader(idh_out);
						arr_out[2] = getBitsForCertStatVdo(csvdo_out);
						length_out = 3;
						
						// Product VDO should be sent for Hubs, Peripherals, and AMAs
						if ((idh_out.product_type == HUB)			||
							(idh_out.product_type == PERIPHERAL)	||
							(idh_out.product_type == AMA)) {
							// if VID is unassigned, do not send Product VDO
							if (idh_out.usb_vid != VID_UNASSIGNED) {
								ProductVdo pvdo_out;
								pvdo_out = my_id.product_vdo;
								arr_out[length_out] = getBitsForProductVdo(pvdo_out);
								length_out++;
							}
						}
						
						// Cable VDO should be sent when we are a Passive Cable or Active Cable
						if ((idh_out.product_type == PASSIVE_CABLE)	||
							(idh_out.product_type == ACTIVE_CABLE)) {
							// TODO: This copying/making VDO object process could probably be more efficient.
							// Could construct these VDOs at initialization, or have the definition objects be
							// pre-formed into VDO form. Or something. - Gabe
							CableVdo cvdo_out;
							cvdo_out = my_id.cable_vdo;
							arr_out[length_out] = getBitsForCableVdo(cvdo_out);
							length_out++;
						}
						
						// AMA VDO should be sent when we are an AMA!
						if (((*vdmm).vdef.product_type == AMA)) {
							AmaVdo amavdo_out;
							amavdo_out = my_id.ama_vdo;
							
							arr_out[length_out] = getBitsForAmaVdo(amavdo_out);
							length_out++;
						}
						
						sendVdmMessage(vdmm, sop, arr_out, length_out);
                                        }
					break;
					case DISCOVER_SVIDS:
                                        {
						unsigned int i;
						svdmh_out.svid 			= PD_SID;					// always use PS_SID for Discover SVIDs, even on response
						svdmh_out.vdm_type 		= STRUCTURED_VDM;			// Discovery SVIDs is Structured
						svdmh_out.svdm_version	= STRUCTURED_VDM_VERSION;	// stick the vdm version in
						svdmh_out.obj_pos		= 0;						// doesn't matter for Discover SVIDs
						svdmh_out.cmd_type		= RESPONDER_ACK;			// TODO: When to NAK or be BUSY?
						svdmh_out.command		= DISCOVER_SVIDS;			// Reply with same command, Discover SVIDs
								
						arr_out[0] = getBitsForStructuredVdmHeader(svdmh_out);
						length_out = 1;
						
						
						// WARNING WARNING
						// if 12+ SVIDs, this code currently assumes that req_svid_info will return the same values
						// in subsequent calls. TODO FIXME
						SvidInfo my_svids_info = (*vdmm).req_svid_info();
						
						// loop through SVIDs and create the bit fields for transmission
						// more than 12 SVIDs requires splitting into multiple PD messages
						// but Discover SVIDs must keep being sent out to stimulate the response
						for (i = (*vdmm).last_svid_sent+1; (i < my_svids_info.num_svids) && (i < ((*vdmm).last_svid_sent+1+MAX_SVIDS_PER_MESSAGE)) ; i++) {
							
							// check if i is even
							if (!(i & 0x1)) {
								// if even, shift SVID up to the top 16 bits
								arr_out[length_out] = 0;
								arr_out[length_out] |= (my_svids_info.svids[i] << 16);
							} else {
								// if odd, fill out the bottom 16 bits
								arr_out[length_out] |= (my_svids_info.svids[i] <<  0);
								
								// and prep it to be sent
								length_out++;
							}
						}
						
						// DONE: handle the case where we have a multiple of 12 number of SVIDs (ex 12, 24, 36...)
						// In that case, we have to finish sending all SVIDs, but also reply to another final Discover SVIDs command
						// with an empty (all zeroes) reply to signal that we've run out of SVIDs to send.
						
						// If we've sent all SVIDs, reset last_svid_sent because we no longer need to track it -
						// if we get another Discover SVIDs command, we'll send the entire list once more
						// UNLESS we have a multiple of 12 SVIDs, in which case since we've filled up the entire response,
						// we have to have one last Discover SVID response with SVIDs of 0s to indicate that we have indeed
						// sent all SVIDs.
						if ((i == (my_svids_info.num_svids)) && (length_out == 7)) {
							// must hang on to send one more reply with 0s to signify end of SVIDs
							(*vdmm).last_svid_sent = i - 1;
						} else if (i >= my_svids_info.num_svids) {
							(*vdmm).last_svid_sent = -1;
						} else {
							// track the last svid we sent so we can continue later for more Discover SVID commands
							(*vdmm).last_svid_sent = i - 1;
						}

						// only terminate with zeros if there is room in the message for it
						if (length_out < (1 + MAX_SVIDS_PER_MESSAGE/2)) { // assuming two SVIDs per object plus a header
							if (!(i & 0x1)) {
								// if i is even, that means we need to send another object with all 0s for SVIDs
								// to show that we're done sending SVIDs
								arr_out[length_out] = 0x00000000;
								length_out++;
							} else {
								// if i is odd, that means we have half of an SVID Responder VDO.
								// bottom 16 bits should be 0 anyway, just need to actually send it.
								length_out++;
							}
						}
							
						sendVdmMessage(vdmm, sop, arr_out, length_out);
						return 0;
                                        }
					break;
					case DISCOVER_MODES:
                                        {
						int index_svid = -1;
						int j;
						ModesInfo my_modes_info;

						svdmh_out.svid 			= svdmh_in.svid;			// reply with SVID we're being asked about
						svdmh_out.vdm_type 		= STRUCTURED_VDM;			// Discovery Modes is Structured
						svdmh_out.svdm_version	= STRUCTURED_VDM_VERSION;	// stick the vdm version in
						svdmh_out.obj_pos		= 0;						// doesn't matter for Discover Modes
						svdmh_out.cmd_type		= RESPONDER_ACK;			// TODO: When to NAK or be BUSY?
						svdmh_out.command		= DISCOVER_MODES;			// Reply with same command, Discover Modes
								
						arr_out[0] = getBitsForStructuredVdmHeader(svdmh_out);
						length_out = 1;
						
						my_modes_info = (*vdmm).req_modes_info(svdmh_in.svid);
						
						for (j = 0; j < my_modes_info.num_modes; j++) {
							arr_out[j+1] = my_modes_info.modes[j];
							length_out++;
						}
						
						/* OLD CODE BELOW - scaling back the VDM block function.
							Could be useful in DPM stuff, though.

						// find index of SVID in question
						for (j = 0; j < (*vdmm).vdef.num_svids; j++) {
							if ((*vdmm).vdef.svids[j] == svdmh_in.svid) {
								index_svid = j;
							}
						}
						
						if (index_svid == -1) {
							// TODO: Respond with a NAK here?
							// SVID not found.
						} else {
							for (j = 0; j < (*vdmm).vdef.num_modes_for_svid[index_svid]; j++) {
								arr_out[j+1] = (*vdmm).vdef.modes[index_svid][j];
								length_out++;
							}
						}
						*/
						sendVdmMessage(vdmm, sop, arr_out, length_out);
						return 0;
                                        }
					break;
					case ENTER_MODE			: ;
						bool mode_entered;
						mode_entered = (*vdmm).req_mode_entry(svdmh_in.svid, svdmh_in.obj_pos);
						
						// most of the message response will be the same whether we entered the mode or not
						svdmh_out.svid 			= svdmh_in.svid;			// reply with SVID we're being asked about
						svdmh_out.vdm_type 		= STRUCTURED_VDM;			// Enter Mode is Structured
						svdmh_out.svdm_version	= STRUCTURED_VDM_VERSION;	// stick the vdm version in
						svdmh_out.obj_pos		= svdmh_in.obj_pos;			// reflect the object position
						svdmh_out.command		= ENTER_MODE;				// Reply with same command, Enter Mode
							
						// if DPM says OK, respond with ACK
						if (mode_entered) {
							svdmh_out.cmd_type		= RESPONDER_ACK;		// entered mode successfully
						} else {
							svdmh_out.cmd_type 		= RESPONDER_NAK;		// NAK if mode not entered
						}
						
						arr_out[0] = getBitsForStructuredVdmHeader(svdmh_out);
						length_out = 1;
						
						sendVdmMessage(vdmm, sop, arr_out, length_out);
						
						return 0;
					break;
					case EXIT_MODE			: ;
						bool mode_exited;
						mode_exited = (*vdmm).req_mode_exit(svdmh_in.svid, svdmh_in.obj_pos);
						
						// most of the message response will be the same whether we exited the mode or not
						svdmh_out.svid 			= svdmh_in.svid;			// reply with SVID we're being asked about
						svdmh_out.vdm_type 		= STRUCTURED_VDM;			// Exit Mode is Structured
						svdmh_out.svdm_version	= STRUCTURED_VDM_VERSION;	// stick the vdm version in
						svdmh_out.obj_pos		= svdmh_in.obj_pos;			// reflect the object position
						svdmh_out.command		= EXIT_MODE;				// Reply with same command, Exit Mode
							
						// if DPM says OK, respond with ACK
						if (mode_exited) {
							svdmh_out.cmd_type		= RESPONDER_ACK;		// exited mode successfully
						} else {
							svdmh_out.cmd_type 		= RESPONDER_NAK;		// NAK if mode not exited
						}
						
						arr_out[0] = getBitsForStructuredVdmHeader(svdmh_out);
						length_out = 1;
						
						sendVdmMessage(vdmm, sop, arr_out, length_out);
						
						return 0;
						return 1;
					break;
					case ATTENTION			:
						// TODO
						return 1;
					break;
					default					:
						// TODO: SVID-Specific commands could go here?
						
						// in this case the command is unrecognized. Reply with a NAK.
						svdmh_out.svid 			= svdmh_in.svid;			// reply with SVID we received
						svdmh_out.vdm_type 		= STRUCTURED_VDM;			// All are structured in this switch-case
						svdmh_out.svdm_version	= STRUCTURED_VDM_VERSION;	// stick the vdm version in
						svdmh_out.obj_pos		= 0;						// value doesn't matter here
						svdmh_out.cmd_type		= RESPONDER_NAK;			// Command unrecognized, so NAK
						svdmh_out.command		= svdmh_in.command;			// Reply with same command
						
						arr_out[0] = getBitsForStructuredVdmHeader(svdmh_out);
						length_out = 1;
						
						sendVdmMessage(vdmm, sop, arr_out, length_out);
						return 0;
					break;
				}
			break;
			case RESPONDER_NAK	:
			case RESPONDER_BUSY	:
			case RESPONDER_ACK	:
				switch (svdmh_in.command) {
					case DISCOVER_IDENTITY	:
						// TODO: What to do here for a NAK/Busy Discover Identity response?
						// Try again? Delay, THEN try again? Simply report a failure?
						if (svdmh_in.cmd_type != RESPONDER_ACK) break;
						
						// Discover Identity responses should have at least VDM Header, ID Header, and Cert Stat VDO
						if (length_in < MIN_DISC_ID_RESP_SIZE) {
							return 1; 
						}
						
						Identity new_identity;
						new_identity.id_header = 		getIdHeader(arr_in[1]);
						new_identity.cert_stat_vdo =	getCertStatVdo(arr_in[2]);
						
						// TODO: Is there any need to check for expectations on Cable VDOs and AMA VDOs?
						// Do we need to flag errors or ignore the identity if those are missing? I'm assuming they are present.
						// This could cause a segfault if they aren't! oh no! FIXME
						
						// TODO/FIXME: I am making some assumptions about the ordering of the extra VDOs that can come in here.
						// May have to revisit this if the ordering is different or configurable
						
						// TODO/FIXME: Well, the only Product Type that might send two of these VDO is the AMA. 
						// But I don't see anywhere in the spec where it specifies the order of the two.
						// The PD spec does introduce the Product VDO first, so I'm assuming that it will be first,
						// and that for AMAs BOTH Product VDO and AMA VDO will always come in.
						
						if (	(new_identity.id_header.product_type == HUB) 		||
								(new_identity.id_header.product_type == PERIPHERAL)	||
								(new_identity.id_header.product_type == AMA)) {
								new_identity.has_product_vdo = true;
								new_identity.product_vdo = getProductVdo(arr_in[3]); // !!! assuming it is before AMA VDO
						}
						
						if (	(new_identity.id_header.product_type == PASSIVE_CABLE)	||
								(new_identity.id_header.product_type == ACTIVE_CABLE)) {
								new_identity.has_cable_vdo = true;
								new_identity.cable_vdo = getCableVdo(arr_in[3]);
						}
						
						if (	(new_identity.id_header.product_type == AMA)) {
								new_identity.has_ama_vdo = true;
								new_identity.ama_vdo = getAmaVdo(arr_in[4]); // !!! assuming it is after Product VDO
						}
						
						if (sop == SOP) {
							(*vdmm).sop_identity = new_identity;
						} else if (sop == SOP1) {
							(*vdmm).sop1_identity = new_identity;
						} else {
							// error! Discover Identity is only for SOP and SOP1.
							(*vdmm).inform_id(false, sop, new_identity);
							return 1;
						}
						(*vdmm).inform_id(true, sop, new_identity);
						return 0;
					break;
					case DISCOVER_SVIDS		:
						// TODO: What to do here for a NAK/Busy Discover SVIDs response?
						// Try again? Delay, THEN try again? Simply report a failure?
						if (svdmh_in.cmd_type != RESPONDER_ACK) break;
					
					
						u16 top16;
						u16 bottom16;
						
						unsigned int* num_sopstar_svids; 
						u16* svid_arr;
						// determine which count to increase based on SOP*
						if (sop == SOP) {
							num_sopstar_svids = &( (*vdmm).num_sop_svids);
							svid_arr =			(*vdmm).sop_svids;
						} else if (sop == SOP1) {
							num_sopstar_svids = &( (*vdmm).num_sop1_svids);
							svid_arr =			(*vdmm).sop1_svids;
						} else if (sop == SOP2) {
							num_sopstar_svids = &( (*vdmm).num_sop2_svids);
							svid_arr =			(*vdmm).sop2_svids;
						}
						
						int i;
						bool found_end = false;
						for (i = 1; i < length_in; i++) { 
							top16 = 	(arr_in[i] >> 16) & 0x0000FFFF;
							bottom16 =	(arr_in[i] >>  0) & 0x0000FFFF;
							
							// TODO: Check for repeated SVIDs here. Don't allow copies of the same SVID to populate the list.
							
							// if top 16 bits are 0, we're done getting SVIDs
							if (top16 == 0x0000) {
								found_end = true;
								break;
							} else {
								svid_arr[*num_sopstar_svids] = top16;
								*num_sopstar_svids += 1;
							} 
							
							// if bottom 16 bits are 0 we're done getting SVIDs
							if (bottom16 == 0x0000) {
								found_end = true;
								break;
							} else {
								svid_arr[*num_sopstar_svids] = bottom16;
								*num_sopstar_svids += 1;
							}
						}
								
						if (found_end != true) {
							// send another Discover SVID command, because what we received didn't end with 0000s.
							// it means that the device has more SVIDs to give to us.
							svdmh_out.svid 			= PD_SID; 					// PD SID to be used for Discover SVID command
							svdmh_out.vdm_type 		= STRUCTURED_VDM;			// structured VDM Header
							svdmh_out.svdm_version 	= STRUCTURED_VDM_VERSION;	// version as defined
							svdmh_out.obj_pos 		= 0;						// does not matter for Discover SVID
							svdmh_out.cmd_type 		= INITIATOR;				// we are initiating discovery
							svdmh_out.command 		= DISCOVER_SVIDS;			// Discover SVIDs command!
	
							arr_out[0] = getBitsForStructuredVdmHeader(svdmh_out);
							sendVdmMessage(vdmm, sop, arr_out, 1);
							// TODO: Wait for response
						} else {
							(*vdmm).inform_svids(found_end, sop, *num_sopstar_svids, svid_arr);
						}
						
						return 0;
					break;
					case DISCOVER_MODES		: 
						// TODO: What to do here for a NAK/Busy Discover Modes response?
						// Try again? Delay, THEN try again? Simply report a failure?
						if (svdmh_in.cmd_type != RESPONDER_ACK) break;
						
						unsigned int* my_num_svids;
						u16* my_svids;
						u32 (*my_modes)[MAX_MODES_PER_SVID];
						u16 svid_in = svdmh_in.svid;		// grab the SVID
						unsigned int svid_index;
						unsigned int* my_modes_per_svid;
						
						ModesInfo my_modes_info;
						my_modes_info.svid 		= svid_in;
						my_modes_info.num_modes = length_in - 1;
						
						for (i = 1; i < length_in; i++) {
							my_modes_info.modes[i - 1] = arr_in[i];
						}
						
						(*vdmm).inform_modes(true, sop, my_modes_info);

						/* OLD CODE BELOW - which would blast out discover modes for all discovered SVIDs.
							Toning back this VDM block though for simplicity and flexibility.
							This code may be useful in the DPM, though.
							
						// TODO/FIXME: If an SOP messages an SOP', does the SOP' reply with ordered set SOP? or SOP'? 
						// gotta look at the spec... - Gabe
						// right now assuming it would reply with ordered set SOP'. I think this is true based on section 2.4 of PD spec 1.0
						
						// grab the correct svid count
						if (sop == SOP) 	my_num_svids = &((*vdmm).num_sop_svids);
						if (sop == SOP1) 	my_num_svids = &((*vdmm).num_sop1_svids);
						if (sop == SOP2) 	my_num_svids = &((*vdmm).num_sop2_svids);
						
						// grab the corret svids array
						if (sop == SOP)		my_svids = ((*vdmm).sop_svids);
						if (sop == SOP1)	my_svids = ((*vdmm).sop1_svids);
						if (sop == SOP2)	my_svids = ((*vdmm).sop2_svids);
						
						// grab the correct modes array
						if (sop == SOP)		my_modes = ((*vdmm).sop_modes);
						if (sop == SOP1)	my_modes = ((*vdmm).sop1_modes);
						if (sop == SOP2)	my_modes = ((*vdmm).sop2_modes);
						
						// find the index for that SVID
						for (i = 0; i < (*my_num_svids); i++) {
							if (svid_in == my_svids[i]) {
								break;
							}
						}				
						
						// TODO: What if SVID not found!?!?! - Gabe
						svid_index = i;
						
						// grab the count of modes for each svid
						if (sop == SOP)		my_modes_per_svid = ((*vdmm).num_sop_modes_per_svid);
						if (sop == SOP1)	my_modes_per_svid = ((*vdmm).num_sop1_modes_per_svid);
						if (sop == SOP2)	my_modes_per_svid = ((*vdmm).num_sop2_modes_per_svid);
						
						for (i = 0; i < (length_in - 1); i++) {
							my_modes[svid_index][my_modes_per_svid[svid_index]] = arr_in[i+1];
							my_modes_per_svid[svid_index]++;
						}*/
						
						return 0;
					break;
					case ENTER_MODE			:
						// this does not distinguish between NAKs and BUSYs.
						// TODO: is that okay? - Gabe
						if (svdmh_in.cmd_type != RESPONDER_ACK) {
							(*vdmm).enter_mode_result(false, svdmh_in.svid, svdmh_in.obj_pos);
						} else {
							(*vdmm).enter_mode_result(true, svdmh_in.svid, svdmh_in.obj_pos);
						}
						return 0;
					break;
					case EXIT_MODE			:
						// this does not distinguish between NAKs and BUSYs.
						// TODO: is that okay? - Gabe
						if (svdmh_in.cmd_type != RESPONDER_ACK) {
							(*vdmm).exit_mode_result(false, svdmh_in.svid, svdmh_in.obj_pos);
						} else {
							(*vdmm).exit_mode_result(true, svdmh_in.svid, svdmh_in.obj_pos);
						}
						return 0;
					break;
					case ATTENTION			:
						// TODO
						if (svdmh_in.cmd_type != RESPONDER_ACK) break; // TODO
						return 1;
					break;
					default					:
						// TODO
						// TODO: SVID-specific commands could go here?
						return 1;
					break;
				}
			break;
			default				:
				// TODO, flag error
				return 1;
			break;
		}		
	} else {
		uvdmh_in = getUnstructuredVdmHeader(arr_in[0]);
		// TODO: Do stuff with unstructured messages
		return 1;
	}
	
	return 0;
}

// call this function to enter a mode
int enterMode(VdmManager* vdmm, SopType sop, u16 svid, unsigned int mode_index) {
	StructuredVdmHeader svdmh;
	unsigned int length = 1;	
	u32 arr[length];
	
	// TODO
	
	// TODO: Validate input values to avoid segfaults
	
	// TODO: Clear previously discovered Identities.
	// Should we also clear previously discovered SVIDs and Modes?
	
	svdmh.svid 			= svid; 					// Use SVID specified upon function call
	svdmh.vdm_type 		= STRUCTURED_VDM;			// structured VDM Header
	svdmh.svdm_version 	= STRUCTURED_VDM_VERSION;	// version 1.0 = 0
	svdmh.obj_pos 		= mode_index;				// select mode
	svdmh.cmd_type 		= INITIATOR;				// we are initiating mode entering
	svdmh.command 		= ENTER_MODE;				// Enter Mode command!
	
	arr[0] = getBitsForStructuredVdmHeader(svdmh);
	sendVdmMessage(vdmm, sop, arr, 1);
	
	return 0;
}

// call this function to exit a mode
int exitMode(VdmManager* vdmm, SopType sop, u16 svid, unsigned int mode_index) {
	// TODO
	
	// TODO: Validate input values to avoid segfaults
	return 1;
}

// call this function to send an attention command to specified SOP type
int sendAttention(VdmManager* vdmm, SopType sop) {
	// TODO
	return 1;
}
