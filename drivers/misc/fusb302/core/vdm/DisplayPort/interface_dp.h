#ifndef _FSC_INTERFACE_DP_H
#define _FSC_INTERFACE_DP_H

#include "../../platform.h"
#include "dp_types.h"

/* vvvvv Functions in the core to be called by the system vvvvv */
/* Initiate status request - called by 'system' to get status of port partner */
VOID requestDpStatus(VOID);
/* Initiate config request - called by 'system' to configure port partner */
VOID requestDpConfig(DisplayPortConfig_t in);
/* Configure DP */
VOID configDp(BOOL enabled, UINT32 status);
VOID configAutoDpModeEntry(BOOL enabled, UINT32 mask, UINT32 value);
VOID WriteDpControls(UINT8 * data);
VOID ReadDpControls(UINT8 * data);
VOID ReadDpStatus(UINT8 * data);
/* ^^^^^ Functions in the core to be called by the system ^^^^^ */

/* vvvvv functions for the system to implement vvvvv */
VOID informStatus(DisplayPortStatus_t stat);
VOID updateStatusData(VOID);
VOID informConfigResult(BOOL success);
BOOL DpReconfigure(DisplayPortConfig_t config);

/* Automatic DisplayPort features! */
/* Automatic Mode Entry */
/* Uses a value/mask scheme */
/* 1 in mask = don't care. 0 in mask = compare to specified value */
extern BOOL DpEnabled;
extern BOOL DpAutoModeEntryEnabled;
extern DisplayPortCaps_t DpModeEntryMask;
extern DisplayPortCaps_t DpModeEntryValue;

/* DisplayPort Status/Config objects */
extern DisplayPortCaps_t DpCaps;
extern DisplayPortStatus_t DpStatus;
extern DisplayPortConfig_t DpConfig;

/* Port Partner Status/Config objects */
extern DisplayPortStatus_t DpPpStatus;
extern DisplayPortConfig_t DpPpRequestedConfig;
extern DisplayPortConfig_t DpPpConfig;
extern UINT32 DpModeEntered;

#endif /* #ifndef _FSC_INTERFACE_DP_H */
