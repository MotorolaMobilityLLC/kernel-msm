#ifndef __CALLBACKS_H__
#define __CALLBACKS_H__
/*
 * This file defines types for callbacks that the VDM block will use.
 * The intention is for the user to define functions that return data
 * that VDM messages require, ex whether or not to enter a mode.
 */

#include "vdm_types.h"

typedef Identity(*RequestIdentityInfo) (void);

typedef SvidInfo(*RequestSvidInfo) (void);

typedef ModesInfo(*RequestModesInfo) (u16);

// ask DPM if we can enter a mode
typedef bool(*ModeEntryRequest) (u16 svid, unsigned int mode_index);

// ask DPM if we can exit a mode - TODO: use mode_index = -1 to signify exit ALL modes?
typedef bool(*ModeExitRequest) (u16 svid, unsigned int mode_index);

// Hmmm - Not sure if we can depend on the mode being entered in the callback function?
// might have to set up a function for the system to tell us that a mode has been entered/exited.
// may need a more complex return code instead of boolean success, if it's important to distinguish
// NAK vs BUSY vs timeout
typedef bool(*EnterModeResult) (bool success, u16 svid,
				unsigned int mode_index);

// may need a more complex return code instead of boolean success, if it's important to distinguish
// NAK vs BUSY vs timeout
// TODO: How to manage transition to hard reset if exit mode fails?
typedef void (*ExitModeResult) (bool success, u16 svid,
				unsigned int mode_index);

typedef void (*InformIdentity) (bool success, SopType sop, Identity id);

typedef void (*InformSvids) (bool success, SopType sop, unsigned int num_svids,
			     u16 svids[MAX_NUM_SVIDS]);

typedef void (*InformModes) (bool success, SopType sop, ModesInfo modes_info);

typedef void (*InformAttention) (void);

#endif // header guard
