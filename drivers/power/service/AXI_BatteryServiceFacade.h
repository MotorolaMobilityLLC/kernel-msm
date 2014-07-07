/*
         bat facade interface include file

*/
#ifndef __AXI_BATTERY_SERVICE_FACADE_H__
#define __AXI_BATTERY_SERVICE_FACADE_H__
#include "../AXI_Basic_Define.h"

typedef enum {
    BAT_CHARGING_STATUS,
    BAT_DISCHARGING_STATUS,
    BAT_NOT_CHARGING_STATUS,
    BAT_CHARGING_FULL_STATUS,
    BAT_CHARGING_STATUS_COUNT,
}AXE_BAT_CHARGING_STATUS;


struct AXI_BatteryServiceFacade;
struct AXI_BatteryServiceFacadeCallback;


typedef struct AXI_BatteryServiceFacadeCallback{

    void(*changeChargingCurrent)(struct AXI_BatteryServiceFacadeCallback *callback,AXE_Charger_Type chargertype);
    void(*onServiceStatusUpdated)(struct AXI_BatteryServiceFacadeCallback *callback);
    int(*getVBAT)(struct AXI_BatteryServiceFacadeCallback *callback);
    int(*getIBAT)(struct AXI_BatteryServiceFacadeCallback *callback);
    int(*getTempBAT)(struct AXI_BatteryServiceFacadeCallback *callback);
}AXI_BatteryServiceFacadeCallback;

typedef struct AXI_BatteryServiceFacade {
    AXE_BAT_CHARGING_STATUS (*getChargingStatus)(struct AXI_BatteryServiceFacade * bat);
    int (*getCapacity)(struct AXI_BatteryServiceFacade * bat);
    void(*onCableInOut)(struct AXI_BatteryServiceFacade *bat, AXE_Charger_Type type);
    void(*onChargingStop)(struct AXI_BatteryServiceFacade *bat,AXE_Charging_Error_Reason reason);
    void(*onChargingStart)(struct AXI_BatteryServiceFacade *bat);    
    void(*onBatteryLowAlarm)(struct AXI_BatteryServiceFacade *bat, bool isCurrentBattlow);
    void(*onBatteryRemoved)(struct AXI_BatteryServiceFacade * bat, bool isRemoved);
    void(*suspend)(struct AXI_BatteryServiceFacade *bat);
    void(*resume)(struct AXI_BatteryServiceFacade *bat,int delayStartInSeconds);
    void(*forceResume)(struct AXI_BatteryServiceFacade *bat,int delayStartInSeconds);
    void(*dockSuspend)(struct AXI_BatteryServiceFacade *bat);
}AXI_BatteryServiceFacade;

extern AXI_BatteryServiceFacade *getBatteryService(AXI_BatteryServiceFacadeCallback *callback);

#endif //__AXI_BATTERY_SERVICE_FACADE_H__

