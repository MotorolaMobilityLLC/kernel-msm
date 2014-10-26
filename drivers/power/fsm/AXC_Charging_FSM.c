#ifdef CONFIG_CHARGER_ASUS_FSM
#include "AXC_Charging_FSM.h"
#include <linux/kernel.h>
static char DEBUG_STR_JUST_FUN_NAME[] = "[BAT][Fsm]%s\n"; 
#define CHARGING_FSM_STRESS_TEST_DEBUG_JUST_FUN_NAME() \
printk(DEBUG_STR_JUST_FUN_NAME, __FUNCTION__)

/*
static char DEBUG_STR_1_STR_1_INT[] = "[FSMTest]%s,%s=%d\n"; 
#define CHARGING_FSM_STRESS_TEST_DEBUG_1_STR_1_INT(_str,_int) \
printk(DEBUG_STR_1_STR_1_INT,   \
__FUNCTION__,   \
_str,   \
_int)

static char DEBUG_STR_WITHOUT_FUN_NAME[] = "[FSMTest]%s\n"; 
#define CHARGING_FSM_STRESS_TEST_DEBUG_WITHOUT_FUN_NAME(_str)   \
printk( DEBUG_STR_WITHOUT_FUN_NAME,  \
_str)   
*/

static char DEBUG_THREE_STR_WITHOUT_FUN_NAME[] = "[BAT][Fsm]%s,%s,%s\n"; 
#define CHARGING_FSM_STRESS_TEST_DEBUG_THREE_STR_WITHOUT_FUN_NAME(_str1,_str2, _str3)   \
printk( DEBUG_THREE_STR_WITHOUT_FUN_NAME,  \
_str1,  \
_str2,  \
_str3)   


static char DEBUG_STR[] = "[BAT][Fsm]%s%s\n"; 
#define CHARGING_FSM_STRESS_TEST_DEBUG(_str)   \
printk( DEBUG_STR,  \
__FUNCTION__,   \
_str) 

#if defined(ASUS_CHARGING_MODE) && !defined(ASUS_FACTORY_BUILD)
extern char g_CHG_mode;
#endif


static void AXC_DisCharging_State_handle(AXI_Charging_State *state,AXE_Charger_Event event,AXE_Charger_Type type,AXE_Charging_Error_Reason reason)
{
    AXA_Charging_State  *_this=
        container_of(state, AXA_Charging_State, miParent);
    
    pr_debug("[BAT][Fsm]%s,event=%d\n", __FUNCTION__,event);

    switch(event)
    {
        case CABLE_IN_EVENT:
                if(type > NO_CHARGER_TYPE && type <CHARGING_TYPE_COUNT){

                    _this->mImpl.enterChargingState(_this, type);

                }else{               
                    printk("%s,param error,event = %d,tyep=%d\n", __FUNCTION__,event, type);
                }
            break;
        case CHARGING_RESUME_EVENT:
        case CABLE_OUT_EVENT:
        case CHARGING_DONE_EVENT:
        case CHARGING_STOP_EVENT: 
        case CHARGING_FULLDROP_EVENT:    
        default : 
            //don't care...
            break;
    }
}
static void AXC_Charging_State_handle(AXI_Charging_State *state,AXE_Charger_Event event,AXE_Charger_Type type,AXE_Charging_Error_Reason reason)
{
    AXA_Charging_State  *_this=
        container_of(state, AXA_Charging_State, miParent);

    printk("%s,event=%d\n", __FUNCTION__,event);

    switch(event)
    {
        case CABLE_OUT_EVENT:
                if(type == NO_CHARGER_TYPE){

                    _this->mImpl.enterDischargingState(_this);

                }else{               
                    printk("%s,param error,event = %d,type=%d\n", __FUNCTION__,event, type);
                }
            break;
        case CHARGING_DONE_EVENT:
            _this->mImpl.enterChargingFullState(_this);            
            break;
        case CHARGING_STOP_EVENT:
                if(reason!= CHARGING_DONE){

                    _this->mImpl.enterChargingStopState(_this,reason);  
                    
                }else{
                    printk("%s,param error,event = %d,reason=%d\n", __FUNCTION__,event, reason);
                }
            break;
        case CABLE_IN_EVENT:
#if defined(ASUS_CHARGING_MODE) && !defined(ASUS_FACTORY_BUILD) //ASUS_BSP Eason_Chang 1120 porting +++
            if( 1==g_CHG_mode )
            {
                    if(type > NO_CHARGER_TYPE && type <CHARGING_TYPE_COUNT){

                        _this->mImpl.enterChargingState(_this, type);
                        printk("[BAT]%s,in chg mode,event = %d,tyep=%d\n", __FUNCTION__,event, type);

                    }else{               
                        printk("%s,param error,event = %d,tyep=%d\n", __FUNCTION__,event, type);
                    }
            break; 
            }
#endif//ASUS_CHARGING_MODE//ASUS_BSP Eason_Chang 1120 porting ---
        case CHARGING_RESUME_EVENT: 
        case CHARGING_FULLDROP_EVENT:    
        default : 
            //don't care...
            break;
    }

}

static void AXC_Charging_Stop_State_handle(AXI_Charging_State *state,AXE_Charger_Event event,AXE_Charger_Type type,AXE_Charging_Error_Reason reason)
{
    AXA_Charging_State  *_this=
        container_of(state, AXA_Charging_State, miParent);

    printk("%s,event=%d\n", __FUNCTION__,event);

    switch(event)
    {
        case CABLE_OUT_EVENT:
            _this->mImpl.enterDischargingState(_this);            
            break;
        case CHARGING_RESUME_EVENT:
            _this->mImpl.enterChargingState(_this,type);
            break;
        case CHARGING_DONE_EVENT:
        case CHARGING_STOP_EVENT:
        case CABLE_IN_EVENT:  
        case CHARGING_FULLDROP_EVENT:    
        default : 
            //don't care...
            break;
    }

}

static void AXC_Charging_Full_State_handle(AXI_Charging_State *state,AXE_Charger_Event event,AXE_Charger_Type type,AXE_Charging_Error_Reason reason)
{
    AXA_Charging_State  *_this=
        container_of(state, AXA_Charging_State, miParent);

    printk("%s,event=%d\n", __FUNCTION__,event);

    switch(event)
    {
        case CABLE_OUT_EVENT:
            _this->mImpl.enterDischargingState(_this);            
            break;
        case CHARGING_RESUME_EVENT:
            _this->mImpl.enterChargingFullKeepState(_this);
            break;
        case CHARGING_STOP_EVENT:
            if(    POWERBANK_STOP == reason ||
                BALANCE_STOP== reason){
                
                _this->mImpl.enterChargingStopState(_this,reason);

            }
            break;
        case CHARGING_FULLDROP_EVENT:
                if(type > NO_CHARGER_TYPE && type <CHARGING_TYPE_COUNT){

                    _this->mImpl.enterChargingState(_this, type);

                }else{               
                    printk("%s,param error,event = %d,tyep=%d\n", __FUNCTION__,event, type);
                }
            break;
        case CHARGING_DONE_EVENT:
        case CABLE_IN_EVENT:  
        default : 
            //don't care...
            break;
    }
}

static void AXC_Charging_Full_Keep_State_handle(AXI_Charging_State *state,AXE_Charger_Event event,AXE_Charger_Type type,AXE_Charging_Error_Reason reason)
{
    AXA_Charging_State  *_this=
        container_of(state, AXA_Charging_State, miParent);

    printk("%s,event=%d\n", __FUNCTION__,event);

    switch(event)
    {
        case CABLE_OUT_EVENT:
            _this->mImpl.enterDischargingState(_this);            
            break;
        case CHARGING_DONE_EVENT:
            _this->mImpl.enterChargingFullState(_this);
            break;
        case CHARGING_STOP_EVENT:
            if(    POWERBANK_STOP == reason ||
                BALANCE_STOP== reason){
                
                _this->mImpl.enterChargingStopState(_this,reason);

            }else{
                _this->mImpl.enterChargingFullKeepStopState(_this,reason);
            }
            break;
        case CHARGING_FULLDROP_EVENT:
                if(type > NO_CHARGER_TYPE && type <CHARGING_TYPE_COUNT){

                    _this->mImpl.enterChargingState(_this, type);

                }else{               
                    printk("%s,param error,event = %d,tyep=%d\n", __FUNCTION__,event, type);
                }
            break;
        case CABLE_IN_EVENT:  
        case CHARGING_RESUME_EVENT:
        default : 
            //don't care...
            break;
    }
}
static void AXC_Charging_Full_Keep_Stop_State_handle(AXI_Charging_State *state,AXE_Charger_Event event,AXE_Charger_Type type,AXE_Charging_Error_Reason reason)
{
    AXA_Charging_State  *_this=
        container_of(state, AXA_Charging_State, miParent);

    printk("%s,event=%d\n", __FUNCTION__,event);

    switch(event)
    {
        case CABLE_OUT_EVENT:
            _this->mImpl.enterDischargingState(_this);            
            break;
        case CHARGING_RESUME_EVENT:
            _this->mImpl.enterChargingFullKeepState(_this);
            break;
        case CHARGING_STOP_EVENT:
            if(    POWERBANK_STOP == reason ||
                BALANCE_STOP== reason){
                
                _this->mImpl.enterChargingStopState(_this,reason);

            }
            break;
        case CHARGING_FULLDROP_EVENT:
                if(type > NO_CHARGER_TYPE && type <CHARGING_TYPE_COUNT){

                    _this->mImpl.enterChargingState(_this, type);

                }else{               
                    printk("%s,param error,event = %d,tyep=%d\n", __FUNCTION__,event, type);
                }
            break;
        case CHARGING_DONE_EVENT:
        case CABLE_IN_EVENT:  
        default : 
            //don't care...
            break;
    }
}

static char* translate_state_str(AXE_Charging_State state)
{
    char *lp = NULL;

    switch(state){

        case DISCHARGING_STATE:
            lp = "Discharging state";
            break;
            
        case CHARGING_STATE:
            lp = "Charging state";
            break;
            
        case CHARGING_STOP_STATE:
            lp = "Charging Stop state";
            break;
            
        case CHARGING_FULL_STATE:
            lp = "Charging Full state";
            break;
            
        case CHARGING_FULL_KEEP_STATE:
            lp = "Charging Full Keep state";
            break;
            
        case CHARGING_FULL_KEEP_STOP_STATE:
            lp = "Charging Full Keep Stop state";
            break;

        default:
            break;
    }

    return lp;
}



static void A66_Charging_FSM_transit(struct AXI_Charging_Context * context, AXE_Charging_State newstate,AXE_Charger_Type type,AXE_Charging_Error_Reason reason)
{
    AXC_Charging_FSM  *_this=
        container_of(context, AXC_Charging_FSM, miContext);    

    CHARGING_FSM_STRESS_TEST_DEBUG_THREE_STR_WITHOUT_FUN_NAME(
        translate_state_str(_this->currentState ),
        "to",
        translate_state_str(newstate));
    
    _this->currentState = newstate;

    _this->chargerType = type;

    _this->lastChargingErrorReason = reason;

    if(NULL != _this->callback){

        _this->callback->onStateChanged(_this->callback);
    }

}
static void A66_Charging_FSM_setCharingCurrent(struct AXI_Charging_Context * context, AXE_Charger_Type chargertype)
{
    AXC_Charging_FSM  *_this=
        container_of(context, AXC_Charging_FSM, miContext);    

    if(NULL != _this->callback){

        _this->callback->onChangeChargingCurrent(_this->callback,chargertype);
    }
}
static AXE_Charger_Type A66_Charging_FSM_getChargerType(struct AXI_Charging_Context * context)
{

    AXC_Charging_FSM  *_this=
        container_of(context, AXC_Charging_FSM, miContext);

    return _this->chargerType;
}
static AXE_Charging_Error_Reason A66_Charging_FSM_getChargingErrorReason(struct AXI_Charging_Context * context)
{

    AXC_Charging_FSM  *_this=
        container_of(context, AXC_Charging_FSM, miContext);

    return _this->lastChargingErrorReason;
}
AXE_Charging_State A66_Charging_FSM_getState(struct AXC_Charging_FSM *_this)
{
    return _this->currentState;
}
static void A66_Charging_FSM_onCableInOut(struct AXC_Charging_FSM *_this, AXE_Charger_Type type)
{
    CHARGING_FSM_STRESS_TEST_DEBUG_JUST_FUN_NAME();    

    if(type == NO_CHARGER_TYPE){

        _this->statePointerList[_this->currentState]->handle(
            _this->statePointerList[_this->currentState],
            CABLE_OUT_EVENT,
            NO_CHARGER_TYPE,
            NO_ERROR
            );        
        
    }else if(type > NO_CHARGER_TYPE && type <CHARGING_TYPE_COUNT){

        _this->statePointerList[_this->currentState]->handle(
            _this->statePointerList[_this->currentState],
            CABLE_IN_EVENT,
            type,
            NO_ERROR
            );
        
    }else{

        printk(KERN_ERR"%s:param error!!skip it.\n",__FUNCTION__);
    }

    CHARGING_FSM_STRESS_TEST_DEBUG("---");
}
static void A66_Charging_FSM_onChargingStop(struct AXC_Charging_FSM *_this,AXE_Charging_Error_Reason reason)
{
    //CHARGING_FSM_STRESS_TEST_DEBUG_JUST_FUN_NAME();    

    if(reason == CHARGING_DONE){

        _this->statePointerList[_this->currentState]->handle(
            _this->statePointerList[_this->currentState],
            CHARGING_DONE_EVENT,
            _this->chargerType,
            CHARGING_DONE
            );
   
    }else{

        _this->statePointerList[_this->currentState]->handle(
            _this->statePointerList[_this->currentState],
            CHARGING_STOP_EVENT,
            _this->chargerType,
            reason);

    }
}
static void A66_Charging_FSM_onChargingStart(struct AXC_Charging_FSM *_this)
{

    //CHARGING_FSM_STRESS_TEST_DEBUG_JUST_FUN_NAME();    

    _this->statePointerList[_this->currentState]->handle(
        _this->statePointerList[_this->currentState],
        CHARGING_RESUME_EVENT,
        _this->chargerType,
        NO_ERROR);
}
static void A66_Charging_FSM_onFullDrop(struct AXC_Charging_FSM *_this)
{

    CHARGING_FSM_STRESS_TEST_DEBUG_JUST_FUN_NAME();    

    _this->statePointerList[_this->currentState]->handle(
        _this->statePointerList[_this->currentState],
        CHARGING_FULLDROP_EVENT,
        _this->chargerType,
        NO_ERROR);
}
static void A66_Charging_FSM_constructor(struct AXC_Charging_FSM *_this, AXI_Charging_FSM_Callback* callback)
{
    if(false == _this->mbInited){

        _this->currentState = DISCHARGING_STATE;

        _this->callback = callback;

        _this->lastChargingErrorReason = NO_ERROR;

        _this->chargerType = NO_CHARGER_TYPE;

        _this->mbInited = true;

    }
}
static const char AXC_Discharging_State_name[] = "DisCharging";
static void AXC_Discharging_State_constructor(AXA_Charging_State *_this)
{
    AXA_Charging_State_constructor(_this);

    _this->state = DISCHARGING_STATE;
    _this->name = AXC_Discharging_State_name;
    _this->context = NULL;
    _this->miParent.handle = AXC_DisCharging_State_handle;
}
static const char AXC_Charging_State_name[] = "Charging";
static void AXC_Charging_State_constructor(AXA_Charging_State *_this)
{
    AXA_Charging_State_constructor(_this);

    _this->state = CHARGING_STATE;
    _this->name = AXC_Charging_State_name;
    _this->context = NULL;
    _this->miParent.handle = AXC_Charging_State_handle;
}
static const char AXC_Charging_Stop_State_name[] = "Charging Stop";
static void AXC_Charging_Stop_State_constructor(AXA_Charging_State *_this)
{
    AXA_Charging_State_constructor(_this);

    _this->state = CHARGING_STOP_STATE;
    _this->name = AXC_Charging_Stop_State_name;
    _this->context = NULL;
    _this->miParent.handle = AXC_Charging_Stop_State_handle;
}
static const char AXC_Charging_Full_State_name[] = "Charging_Full";
static void AXC_Charging_Full_State_constructor(AXA_Charging_State *_this)
{
    AXA_Charging_State_constructor(_this);

    _this->state = CHARGING_FULL_STATE;
    _this->name = AXC_Charging_Full_State_name;
    _this->context = NULL;
    _this->miParent.handle = AXC_Charging_Full_State_handle;
}
static const char AXC_Charging_Full_Keep_State_name[] = "Charging_Full_Keep";
static void AXC_Charging_Full_Keep_State_constructor(AXA_Charging_State *_this)
{
    AXA_Charging_State_constructor(_this);

    _this->state = CHARGING_FULL_KEEP_STATE;
    _this->name = AXC_Charging_Full_Keep_State_name;
    _this->context = NULL;
    _this->miParent.handle = AXC_Charging_Full_Keep_State_handle;
}
static const char AXC_Charging_Full_Keep_Stop_State_name[] = "Charging_Full_Keep_Stop";
static void AXC_Charging_Full_Keep_Stop_State_constructor(AXA_Charging_State *_this)
{
    AXA_Charging_State_constructor(_this);

    _this->state = CHARGING_FULL_KEEP_STOP_STATE;
    _this->name = AXC_Charging_Full_Keep_Stop_State_name;
    _this->context = NULL;
    _this->miParent.handle = AXC_Charging_Full_Keep_Stop_State_handle;
}
static AXI_Charging_State *A66_Charging_State_List[CHARGING_STATE_COUNT]; 

static AXC_Charging_FSM A66_Charging_FSM ={
    .mbInited = false,
    .miContext = {
        .transit = A66_Charging_FSM_transit,
        .setCharingCurrent = A66_Charging_FSM_setCharingCurrent,
        .getChargerType = A66_Charging_FSM_getChargerType,
        .getChargingErrorReason = A66_Charging_FSM_getChargingErrorReason,
    },
    .currentState = DISCHARGING_STATE,
    .statePointerList = A66_Charging_State_List,
    .lastChargingErrorReason = NO_ERROR,
    .chargerType = NO_CHARGER_TYPE,
    .callback = NULL,
    .getState= A66_Charging_FSM_getState,
    .onCableInOut= A66_Charging_FSM_onCableInOut,
    .onChargingStop = A66_Charging_FSM_onChargingStop,
    .onChargingStart = A66_Charging_FSM_onChargingStart,
    .onFullDrop = A66_Charging_FSM_onFullDrop,
};
static AXA_Charging_State gDischargingstate;
static AXA_Charging_State gChargingstate;
static AXA_Charging_State gChargingStopstate;
static AXA_Charging_State gChargingFullstate;
static AXA_Charging_State gChargingFullKeepstate;
static AXA_Charging_State gChargingFullKeepStopstate;
static AXI_Charging_State *getChargingState(AXE_Charging_State type)
{
    AXI_Charging_State *lpstate = NULL; 
    switch(type){
        case DISCHARGING_STATE:
            {
                AXC_Discharging_State_constructor(&gDischargingstate);
                lpstate = &gDischargingstate.miParent;
            }
            break;
        
        case CHARGING_STATE:
            {
                AXC_Charging_State_constructor(&gChargingstate);
                lpstate = &gChargingstate.miParent;
            }
            break;
            
        case CHARGING_STOP_STATE:
            {
                AXC_Charging_Stop_State_constructor(&gChargingStopstate);
                lpstate = &gChargingStopstate.miParent;
            }
            break;
                
        case CHARGING_FULL_STATE:
            {
                AXC_Charging_Full_State_constructor(&gChargingFullstate);
                lpstate = &gChargingFullstate.miParent;
            }
            break;
            
        case CHARGING_FULL_KEEP_STATE:
            {
                AXC_Charging_Full_Keep_State_constructor(&gChargingFullKeepstate);
                lpstate = &gChargingFullKeepstate.miParent;
            }
            break;
            
        case CHARGING_FULL_KEEP_STOP_STATE:
            {
                AXC_Charging_Full_Keep_Stop_State_constructor(&gChargingFullKeepStopstate);
                lpstate = &gChargingFullKeepStopstate.miParent;
            }
            break;
        default:
            break;
    }

    return lpstate;
}


AXC_Charging_FSM *getChargingFSM(AXE_Charging_FSM_Type type,AXI_Charging_FSM_Callback* callback)
{
    AXC_Charging_FSM *lpFSM = NULL;

    switch(type){

        case E_ASUS_A66_FSM_CHARGING_TYPE:
                {
                    static AXC_Charging_FSM *lpA66FSM = NULL;
                    if(NULL == lpA66FSM){
                        AXE_Charging_State index;
                        lpA66FSM = &A66_Charging_FSM;
                        A66_Charging_FSM_constructor(lpA66FSM,callback);

                        for(index = DISCHARGING_STATE; index <CHARGING_STATE_COUNT; index++){

                            AXI_Charging_State *lpState = NULL;

                            lpState = getChargingState(index);

                            BUG_ON(NULL == lpState);

                            lpA66FSM->statePointerList[index]=lpState;

                            lpA66FSM->statePointerList[index]->setContext(lpA66FSM->statePointerList[index], &lpA66FSM->miContext);

                        }
                        
                    }

                    lpFSM = lpA66FSM;
                }

            break;
        default:
            break;
    }

    return lpFSM;

}
#endif//CONFIG_CHARGER_ASUS_FSM
