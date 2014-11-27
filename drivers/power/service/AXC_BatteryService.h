/*
         bat facade interface include file

*/
#ifndef __AXC_BATTERY_SERVICE_H__
#define __AXC_BATTERY_SERVICE_H__
#include "AXI_BatteryServiceFacade.h"
#include "../fsm/AXC_Charging_FSM.h"
#include "../gauge/axi_gauge.h"
#include <linux/workqueue.h>
#include "AXC_BatteryServiceTest.h"
#include <linux/mutex.h>
#include <linux/wakelock.h>
#include "../charger/axi_charger.h" 
#include "../capfilter/axi_cap_filter.h"

#define DEFAULT_ASUSBAT_POLLING_INTERVAL (0)

typedef struct AXC_BatteryService {
    AXI_BatteryServiceFacade miParent;
    bool mbInit;
    bool IsFirstForceResume;
    AXI_BatteryServiceFacadeCallback *callback;
    int A66_capacity;
    time_t ForceSavedTime;
    time_t savedTime;
    //Hank: temperature monitor+++
    time_t Keep5MinSavedTime;
    bool NeedCalCap;
    bool NeedKeep5Min;
    int TempLimit; //Notmal Temperature: 0, HTHL: 1, HTSL: 2, CTSL: 3, CTHL: 4
    //Hank: temperature monitor---
    bool BatteryService_IsCable;
    bool BatteryService_IsCharging;
    bool BatteryService_IsFULL;
    bool BatteryService_IsBatLow;
    bool isMainBatteryChargingDone;
    bool IsFirstAskCap;
    bool IsFirstAskCable;
    bool HasCableBeforeSuspend;
    bool IsResumeUpdate;
    bool IsResumeMahUpdate;
    bool IsCalculateCapOngoing;
    
    int IsSuspend;
    AXI_Gauge_Callback gaugeCallback;
    AXE_Charger_Type chargerType;
    AXI_Gauge *gauge;
    AXI_Cap_Filter *gpCapFilterA66;
    time_t defaultPollingInterval;
    struct workqueue_struct *BatteryServiceCapUpdateQueue;
    struct delayed_work BatteryServiceUpdateWorker;
    struct delayed_work BatRtcReadyWorker;
    struct delayed_work BatEocWorker;
    struct delayed_work BatEcAcWorker;
    struct delayed_work ResumeWorker;
    struct delayed_work CableOffWorker;
    struct delayed_work SetRTCWorker;
    struct delayed_work SetBatLowRTCWorker;
    struct delayed_work SetCableInRTCWorker;
    struct delayed_work BmsChgBeganWorker;
    struct delayed_work BmsChgEndWorker;
    struct delayed_work Less5MinDoBalanceWorker;//Eason: use queue doBalanceMode in less 5min forceResume
    struct delayed_work CheckGaugeRomModeWorker;//Hank in rom mode show "?", bootup	check Rom mode queue 
    struct delayed_work ReadBatteryCapacityWorker;
    AXI_Charging_FSM_Callback fsmCallback;
    AXC_Charging_FSM *fsm;
    AXE_Charging_State fsmState;
    AXC_BatteryServiceTest test;
    struct mutex main_lock;
    struct mutex filter_lock;
    struct wake_lock cap_wake_lock;
    struct wake_lock resume_wake_lock;
    struct wake_lock cableoff_wake_lock;
    AXI_ChargerStateChangeNotifier gChargerStateChangeNotifier;
}AXC_BatteryService;


#endif //__AXC_BATTERY_SERVICE_H__


