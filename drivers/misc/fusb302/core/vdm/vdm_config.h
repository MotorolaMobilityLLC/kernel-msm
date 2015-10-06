#ifndef __FSC_VDM_CONFIG_H__
#define __FSC_VDM_CONFIG_H__
#include "../platform.h"
#include "DisplayPort/dp_types.h"

/* General VDM Things */
extern BOOL svid_enable;
extern BOOL mode_enable;
extern UINT16 my_svid;
extern UINT32 my_mode;
extern BOOL mode_entered;

/* DisplayPort Things */
extern BOOL DpEnabled;
extern BOOL DpAutoModeEntryEnabled;
extern DisplayPortCaps_t DpModeEntryMask;
extern DisplayPortCaps_t DpModeEntryValue;
extern DisplayPortCaps_t DpCaps;
extern DisplayPortStatus_t DpStatus;
extern DisplayPortConfig_t DpConfig;
extern DisplayPortStatus_t DpPpStatus;
extern DisplayPortConfig_t DpPpRequestedConfig;
extern DisplayPortConfig_t DpPpConfig;
extern UINT32 DpModeEntered;

/* Functions to configure VDM code! */
void ConfigureVdmResponses(UINT8 * bytes);
void ReadVdmConfiguration(UINT8 * data);

/* Functions specifically to configure DisplayPort code! */
void configDp(BOOL enabled, UINT32 status);
void configAutoDpModeEntry(BOOL enabled, UINT32 mask, UINT32 value);
void WriteDpControls(UINT8 * data);
void ReadDpControls(UINT8 * data);
void ReadDpStatus(UINT8 * data);

#endif // __FSC_VDM_CONFIG_H__
