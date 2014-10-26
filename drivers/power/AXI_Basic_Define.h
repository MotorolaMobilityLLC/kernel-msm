/*
        Charging Basic definition include file

*/
#ifndef __AXI_BASIC_DEFINE_H__
#define __AXI_BASIC_DEFINE_H__
#include <linux/types.h>

typedef enum
{
    NOTDEFINE_TYPE = -1,
    NO_CHARGER_TYPE = 0,
    ILLEGAL_CHARGER_TYPE,
    LOW_CURRENT_CHARGER_TYPE,
    NORMAL_CURRENT_CHARGER_TYPE,
    HIGH_CURRENT_CHARGER_TYPE,
#ifdef CONFIG_IDTP9023_CHARGER    
    WC_CHARGER_TYPE,    
#endif        
    CHARGING_TYPE_COUNT
}AXE_Charger_Type;

typedef enum
{   NO_ERROR,
    CHARGING_DONE,
    BATT_REMOVED_ERROR,
    HOT_CHARGER_ERROR,
    BATT_HOT_ERROR,
    CHARGING_TIMEOUT_ERROR,
    UNKNOWN_ERROR,
    POWERBANK_STOP,
    BALANCE_STOP,
    CHARGING_ERROR_COUNT
}AXE_Charging_Error_Reason;


#endif //__AXI_BASIC_DEFINE_H__



