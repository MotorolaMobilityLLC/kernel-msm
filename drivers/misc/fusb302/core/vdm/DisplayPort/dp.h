#ifdef FSC_HAVE_DP

#ifndef __DISPLAYPORT_DP_H__
#define __DISPLAYPORT_DP_H__

#include "../../platform.h"
#include "../../PD_Types.h"
#include "../vdm_types.h"
#include "dp_types.h"

extern FSC_BOOL DpEnabled;
extern FSC_BOOL DpAutoModeEntryEnabled;
extern DisplayPortCaps_t DpModeEntryMask;
extern DisplayPortCaps_t DpModeEntryValue;
extern DisplayPortCaps_t DpCaps;
extern DisplayPortStatus_t DpStatus;
extern DisplayPortConfig_t DpConfig;
extern DisplayPortStatus_t DpPpStatus;
extern DisplayPortConfig_t DpPpRequestedConfig;
extern DisplayPortConfig_t DpPpConfig;
extern FSC_U32 DpModeEntered;

/* Initialize stuff! */
void initializeDp(void);

/* reset stuff - for detaches */
void resetDp(void);

/* Process incoming DisplayPort messages! */
FSC_BOOL processDpCommand(FSC_U32 * arr_in);

/* Internal function to send Status data to port partner */
void sendStatusData(doDataObject_t svdmh_in);

/* Internal function to reply to a Config request (to port partner) */
void replyToConfig(doDataObject_t svdmh_in, FSC_BOOL success);

/* Evaluate a mode VDO for mode entry */
FSC_BOOL dpEvaluateModeEntry(FSC_U32 mode_in);
FSC_BOOL evaluateModeEntry(FSC_U32 mode_in);

#endif // __DISPLAYPORT_DP_H__

#endif // FSC_HAVE_DP
