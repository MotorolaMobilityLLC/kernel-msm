
/*
        Charging State include file

*/
#ifndef __AXI_CHARGING_STATE_H__
#define __AXI_CHARGING_STATE_H__
#include "AXI_Charger_Basic_Define.h"

struct AXI_Charging_Context;
struct AXI_Charging_State;


typedef struct AXI_Charging_Context{
    void(*transit)(struct AXI_Charging_Context * context, AXE_Charging_State newstate,AXE_Charger_Type type,AXE_Charging_Error_Reason reason);
    void(*setCharingCurrent)(struct AXI_Charging_Context * context, AXE_Charger_Type chargertype);
    AXE_Charger_Type (*getChargerType)(struct AXI_Charging_Context * context);
    AXE_Charging_Error_Reason (*getChargingErrorReason)(struct AXI_Charging_Context * context);
}AXI_Charging_Context;

typedef struct AXI_Charging_State {
    AXE_Charging_State(*getState)(struct AXI_Charging_State *state);
    const char*(*getName)(struct AXI_Charging_State *state);    
    void(*setContext)(struct AXI_Charging_State *state, struct AXI_Charging_Context *context);
    void(*handle)(struct AXI_Charging_State *state,AXE_Charger_Event event,AXE_Charger_Type type,AXE_Charging_Error_Reason reason);
}AXI_Charging_State;


#endif //__AXI_CHARGING_STATE_H__



