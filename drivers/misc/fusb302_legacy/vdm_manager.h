#ifndef __VDM_MANAGER_H__
#define __VDM_MANAGER_H__

#include "fsc_vdm_defs.h"
#include "callback_defs.h"

#define NUM_VDM_MODES 6
#define MAX_NUM_SVIDS_PER_SOP 30	// lots of slack here, but also lots of wasted memory. TODO: Make this smarter. - Gabe
#define MAX_SVIDS_PER_MESSAGE 12
#define MIN_DISC_ID_RESP_SIZE 3

/*
 * VDM Manager object, so I can have multiple instances intercommunicating using the same functions!
 */
typedef enum {
	IDLE,			// TODO: More states
	DISCOVERING_IDENTITIES,
	DISCOVERING_SVIDS,
	DISCOVERING_MODES,
} VdmState;

typedef struct {
	bool mode_status[NUM_VDM_MODES];
	VdmState vdm_state;
	VendorDefinition vdef;

	u16 sop_svids[MAX_NUM_SVIDS_PER_SOP];
	unsigned int num_sop_svids;

	u16 sop1_svids[MAX_NUM_SVIDS_PER_SOP];
	unsigned int num_sop1_svids;

	u16 sop2_svids[MAX_NUM_SVIDS_PER_SOP];
	unsigned int num_sop2_svids;

	// TODO: Is it possible to need multiple Identities for that same SOP? (Probably not?) - Gabe
	Identity sop_identity;
	Identity sop1_identity;

	// TODO: Big potential waste of memory here! Might be good to dynamically increase
	// the size of this as necessary... - Gabe
	u32 sop_modes[MAX_NUM_SVIDS_PER_SOP][MAX_MODES_PER_SVID];
	unsigned int num_sop_modes_per_svid[MAX_NUM_SVIDS_PER_SOP];

	u32 sop1_modes[MAX_NUM_SVIDS_PER_SOP][MAX_MODES_PER_SVID];
	unsigned int num_sop1_modes_per_svid[MAX_NUM_SVIDS_PER_SOP];

	u32 sop2_modes[MAX_NUM_SVIDS_PER_SOP][MAX_MODES_PER_SVID];
	unsigned int num_sop2_modes_per_svid[MAX_NUM_SVIDS_PER_SOP];

	// need this member for sending multiple responses to Discover SVID commands...
	// if we happen to have a lot of SVIDs
	int last_svid_sent;

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

	// if simulating, need a string name and SOP*
#ifdef VDM_BLOCK_SIM
	char *name;
	SopType my_sop;
#endif

} VdmManager;

/*
 * Initialization functions.
 */
int initialize_with_def(VdmManager * vdmm, VendorDefinition vdef);	// initializes members
int initialize(VdmManager * vdmm);	// initializes members without needing a definition

/*
 * Functions to go through PD VDM flow.
 */
int discoverIdentities(VdmManager * vdmm, SopType sop);	// Discovers identities in the system!
int discoverSvids(VdmManager * vdmm, SopType sop);	// Discovers SVIDs in the system!
int discoverModes(VdmManager * vdmm, SopType sop, u16 svid);	// Discovers Modes available for each SVID
int sendAttention(VdmManager * vdmm, SopType sop);	// send attention command to specified sop
int processVdmMessage(VdmManager * vdmm, SopType sop, u32 * arr, unsigned int length);	// function to call when we receive VDM messages
int enterMode(VdmManager * vdmm, SopType sop, u16 svid, unsigned int mode_index);	// enter mode specified by SVID and mode index
int exitMode(VdmManager * vdmm, SopType sop, u16 svid, unsigned int mode_index);	// exit mode specified by SVID and mode index
int exitAllModes(VdmManager * vdmm);	// exits all modes

#endif // header guard
