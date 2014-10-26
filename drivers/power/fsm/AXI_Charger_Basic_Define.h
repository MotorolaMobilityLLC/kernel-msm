/*
        Charging Basic definition include file

*/
#ifndef __AXI_CHARGING_BASIC_DEFINE_H__
#define __AXI_CHARGING_BASIC_DEFINE_H__
#include "../AXI_Basic_Define.h"

typedef enum
{
    NOTDEFINE_STATE = -1,
    DISCHARGING_STATE = 0,
    CHARGING_STATE,
    CHARGING_STOP_STATE,
    CHARGING_FULL_STATE,
    CHARGING_FULL_KEEP_STATE,
    CHARGING_FULL_KEEP_STOP_STATE,
    CHARGING_STATE_COUNT
}AXE_Charging_State;

typedef enum
{
    CABLE_IN_EVENT,
    CABLE_OUT_EVENT,
    CHARGING_DONE_EVENT,
    CHARGING_RESUME_EVENT,
    CHARGING_STOP_EVENT,
    CHARGING_FULLDROP_EVENT,
    CHARGING_EVENT_COUNT
}AXE_Charger_Event;


#endif //__AXI_CHARGING_BASIC_DEFINE_H__


