#ifndef __DISPLAYPORT_DP_H__
#define __DISPLAYPORT_DP_H__

#include "../../platform.h"
#include "../../PD_Types.h"
#include "../vdm_types.h"
#include "dp_types.h"

/* Initialize stuff! */
VOID initializeDp(VOID);

/* reset stuff - for detaches */
VOID resetDp(VOID);

/* Process incoming DisplayPort messages! */
BOOL processDpCommand(UINT32 * arr_in);

/* Internal function to send Status data to port partner */
VOID sendStatusData(doDataObject_t svdmh_in);

/* Internal function to reply to a Config request (to port partner) */
VOID replyToConfig(doDataObject_t svdmh_in, BOOL success);

/* Evaluate a mode VDO for mode entry */
BOOL dpEvaluateModeEntry(UINT32 mode_in);

#endif
