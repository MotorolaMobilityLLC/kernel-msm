#ifdef CONFIG_CHARGER_ASUS_FSM

#include "AXA_Charging_State.h"
#include <linux/kernel.h>

static AXE_Charging_State AXA_Charging_State_getState(AXI_Charging_State *state)
{
    AXA_Charging_State  *this=
        container_of(state, AXA_Charging_State, miParent);

    return this->state;
}
static const char *AXA_Charging_State_getName(AXI_Charging_State *state)
{
    struct AXA_Charging_State  *this=
        container_of(state, AXA_Charging_State, miParent);

    return this->name;

}
static void AXA_Charging_State_setContext(AXI_Charging_State *state, AXI_Charging_Context *context)
{
    AXA_Charging_State  *this=
        container_of(state, AXA_Charging_State, miParent);

    this->context = context;
}
static void AXA_Charging_State_enterDischargingState(AXA_Charging_State *this)
{

    BUG_ON(NULL == this->context);

    if(this->context){

        this->context->setCharingCurrent(this->context,
            NO_CHARGER_TYPE);

        this->context->transit(this->context,
            DISCHARGING_STATE,
            NO_CHARGER_TYPE,
            NO_ERROR);
    }
}
static void AXA_Charging_State_enterChargingState(AXA_Charging_State *this,AXE_Charger_Type type)
{

    BUG_ON(NULL == this->context);

    if(this->context){

        this->context->setCharingCurrent(this->context,
            type);

        this->context->transit(this->context,
            CHARGING_STATE,
            type,
            NO_ERROR);
    }
}
static void AXA_Charging_State_enterChargingStopState(AXA_Charging_State *this,AXE_Charging_Error_Reason reason)
{

    BUG_ON(NULL == this->context);

    if(this->context){

        this->context->setCharingCurrent(this->context,
            NO_CHARGER_TYPE);

        this->context->transit(this->context,
            CHARGING_STOP_STATE,
            this->context->getChargerType(this->context),
            reason);
    }
}
static void AXA_Charging_State_enterChargingFullState(AXA_Charging_State *this)
{

    BUG_ON(NULL == this->context);

    if(this->context){

//        this->context->setCharingCurrent(this->context,
//            NO_CHARGER_TYPE);

        this->context->transit(this->context,
            CHARGING_FULL_STATE,
            this->context->getChargerType(this->context),
            NO_ERROR);
    }
}
static void AXA_Charging_State_enterChargingFullKeepState(AXA_Charging_State *this)
{

    BUG_ON(NULL == this->context);

    if(this->context){

        this->context->setCharingCurrent(this->context,
            this->context->getChargerType(this->context));

        this->context->transit(this->context,
            CHARGING_FULL_KEEP_STATE,
            this->context->getChargerType(this->context),
            NO_ERROR);
    }
}
static void AXA_Charging_State_enterChargingFullKeepStopState(AXA_Charging_State *this,AXE_Charging_Error_Reason reason)
{

    BUG_ON(NULL == this->context);

    if(this->context){

        this->context->setCharingCurrent(this->context,
            NO_CHARGER_TYPE);

        this->context->transit(this->context,
            CHARGING_FULL_KEEP_STOP_STATE,
            this->context->getChargerType(this->context),
            reason);
    }
}
void AXA_Charging_State_constructor(AXA_Charging_State *this)
{
    this->miParent.getState = AXA_Charging_State_getState;
    this->miParent.getName = AXA_Charging_State_getName;
    this->miParent.setContext = AXA_Charging_State_setContext;

    this->mImpl.enterDischargingState = AXA_Charging_State_enterDischargingState;
    this->mImpl.enterChargingState = AXA_Charging_State_enterChargingState;
    this->mImpl.enterChargingStopState = AXA_Charging_State_enterChargingStopState;
    this->mImpl.enterChargingFullState = AXA_Charging_State_enterChargingFullState;
    this->mImpl.enterChargingFullKeepState = AXA_Charging_State_enterChargingFullKeepState;
    this->mImpl.enterChargingFullKeepStopState = AXA_Charging_State_enterChargingFullKeepStopState;
}
#endif //#ifdef CONFIG_CHARGER_ASUS_FSM