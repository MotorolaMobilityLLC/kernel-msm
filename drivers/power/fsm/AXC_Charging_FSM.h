/*
        Charging State include file

*/
#ifndef __AXC_CHARGING_FSM_H__
#define __AXC_CHARGING_FSM_H__

#include "AXA_Charging_State.h"


typedef enum {
        E_ASUS_A66_FSM_CHARGING_TYPE,
        E_FSM_CHARGING_TYPE_COUNT,
}AXE_Charging_FSM_Type;        

struct AXI_Charging_FSM_Callback;

typedef struct AXI_Charging_FSM_Callback {
    void(*onChangeChargingCurrent)(struct AXI_Charging_FSM_Callback *callback,AXE_Charger_Type chargertype);
    void(*onStateChanged)(struct AXI_Charging_FSM_Callback *callback);
}AXI_Charging_FSM_Callback;

struct AXC_Charging_FSM;

typedef struct AXC_Charging_FSM {
    bool mbInited;
    AXI_Charging_Context miContext;
    AXE_Charging_State currentState;
    AXI_Charging_State **statePointerList;
    AXE_Charging_Error_Reason lastChargingErrorReason;
    AXE_Charger_Type chargerType;
    AXI_Charging_FSM_Callback* callback;
    AXE_Charging_State(*getState)(struct AXC_Charging_FSM *fsm);
    void(*onCableInOut)(struct AXC_Charging_FSM *fsm, AXE_Charger_Type type);
    void(*onChargingStop)(struct AXC_Charging_FSM *fsm,AXE_Charging_Error_Reason reason);
    void(*onChargingStart)(struct AXC_Charging_FSM *fsm);
    void(*onFullDrop)(struct AXC_Charging_FSM *fsm);
}AXC_Charging_FSM;

extern AXC_Charging_FSM *getChargingFSM(AXE_Charging_FSM_Type type, AXI_Charging_FSM_Callback* callback);

#endif //__AXC_CHARGING_FSM_H__

