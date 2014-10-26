#ifndef __AXA_CHARGING_STATE_H__
#define __AXA_CHARGING_STATE_H__
#include "AXI_Charging_State.h"
struct AXA_Charging_State ;

typedef struct AXA_Charging_State_Impl {
    //final protected member functions
    void(*enterDischargingState)(struct AXA_Charging_State *this);
    void(*enterChargingState)(struct AXA_Charging_State *this,AXE_Charger_Type type);
    void(*enterChargingStopState)(struct AXA_Charging_State *this,AXE_Charging_Error_Reason reason);
    void(*enterChargingFullState)(struct AXA_Charging_State *this);
    void(*enterChargingFullKeepState)(struct AXA_Charging_State *this);
    void(*enterChargingFullKeepStopState)(struct AXA_Charging_State *this,AXE_Charging_Error_Reason reason);
}AXA_Charging_State_Impl;

typedef struct AXA_Charging_State {
    AXI_Charging_State miParent;

//protected functions
    AXE_Charging_State state;
    const char *name;
    AXI_Charging_Context *context;

    AXA_Charging_State_Impl mImpl;
}AXA_Charging_State;

extern void AXA_Charging_State_constructor(AXA_Charging_State *state);
#endif //__AXA_CHARGING_STATE_H__

