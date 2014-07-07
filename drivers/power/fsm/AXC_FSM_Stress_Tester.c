#ifdef CONFIG_CHARGER_ASUS_FSM
#include "AXC_FSM_Stress_Tester.h"
#include "AXC_Charging_FSM.h"
#include <linux/kernel.h>
#include <linux/slab.h>
/*
static char DEBUG_STR_1_STR_1_INT[] = "[FSMTest]%s,%s=%d\n"; 
#define CHARGING_FSM_STRESS_TEST_DEBUG_1_STR_1_INT(_str,_int) \
printk(DEBUG_STR_1_STR_1_INT,   \
__FUNCTION__,   \
_str,   \
_int)
*/
/*
static char DEBUG_STR_WITHOUT_FUN_NAME[] = "[FSMTest]%s\n"; 
#define CHARGING_FSM_STRESS_TEST_DEBUG_WITHOUT_FUN_NAME(_str)   \
printk( DEBUG_STR_WITHOUT_FUN_NAME,  \
_str)   
*/

static char DEBUG_STR[] = "[FSMTest]%s%s\n"; 
#define CHARGING_FSM_STRESS_TEST_DEBUG(_str)   \
printk( DEBUG_STR,  \
__FUNCTION__,   \
_str)   


#define MAX_FEEDING_CONTENT_READ_FROM_FILE (8192*2) //8192*2/8 = 2048 ets > state*event*type*errorreason*dize_of_each_set(6*6*5*6)=1080
static void AXC_FSM_Stress_Tester_parse(struct AXC_FeedingFileInputParser *parser, unsigned int size)
{
    /*

    AXC_FSM_Stress_A_Set_of_Test_Content *content;

    struct AXC_FSM_Stress_Tester  *_this=
        container_of(parser, AXC_FSM_Stress_Tester, parser);

    AXC_Charging_FSM *lpFSM = getChargingFSM(E_ASUS_A66_FSM_CHARGING_TYPE,NULL);

    int index =0;

    CHARGING_FSM_STRESS_TEST_DEBUG("...Start");

    CHARGING_FSM_STRESS_TEST_DEBUG_1_STR_1_INT("filesize",size);

    while(size > index*sizeof(AXC_FSM_Stress_A_Set_of_Test_Content)){

        CHARGING_FSM_STRESS_TEST_DEBUG_1_STR_1_INT("times",index);

        content = (AXC_FSM_Stress_A_Set_of_Test_Content*)(&_this->buf[index*sizeof(AXC_FSM_Stress_A_Set_of_Test_Content)]);

        CHARGING_FSM_STRESS_TEST_DEBUG_1_STR_1_INT("state",content->state);
        CHARGING_FSM_STRESS_TEST_DEBUG_1_STR_1_INT("event",content->event);
        CHARGING_FSM_STRESS_TEST_DEBUG_1_STR_1_INT("type",content->type);
        CHARGING_FSM_STRESS_TEST_DEBUG_1_STR_1_INT("errorCode",content->errorCode);
        CHARGING_FSM_STRESS_TEST_DEBUG_1_STR_1_INT("predictState",content->predictState);
        CHARGING_FSM_STRESS_TEST_DEBUG_1_STR_1_INT("predictType",content->predictType);
        CHARGING_FSM_STRESS_TEST_DEBUG_1_STR_1_INT("predictErrorReason",content->predictErrorReason);

        //pass condition & event to FSM
        lpFSM->currentState = (AXE_Charging_State)content->state;

        switch((AXE_Charger_Event)content->event){

            case CABLE_IN_EVENT:
            case CABLE_OUT_EVENT:
                lpFSM->onCableInOut(lpFSM, (AXE_Charger_Type)content->type);
                break;
            case CHARGING_STOP_EVENT:
            case CHARGING_DONE_EVENT:
                lpFSM->onChargingStop(lpFSM,(AXE_Charging_Error_Reason)content->predictErrorReason);
                break;
            case CHARGING_RESUME_EVENT:
                lpFSM->onChargingStart(lpFSM);
                break;
            default:
                break;
        }

        //check if the result is correct or not
        //BUG_ON((AXE_Charging_State)content->predictState != lpFSM->currentState);
        //BUG_ON((AXE_Charger_Type)content->predictType!= lpFSM->chargerType);
        //BUG_ON((AXE_Charging_Error_Reason)content->predictErrorReason!= lpFSM->lastChargingErrorReason);
        
        index ++;
    }
*/
    CHARGING_FSM_STRESS_TEST_DEBUG("...End");
}
static void AXC_FSM_Stress_Tester_createBuffer(AXC_FSM_Stress_Tester  *_this)
{
    if(NULL == _this->buf){

        _this->buf = kzalloc(MAX_FEEDING_CONTENT_READ_FROM_FILE, 
            GFP_KERNEL);

        BUG_ON(NULL == _this->buf);
    }    
}
/*

static int AXC_FSM_Stress_Tester_copyOneContent(AXC_FSM_Stress_Tester  *_this,
        AXE_Charging_State pre_state,
    AXE_Charger_Type pre_type ,
    AXE_Charging_Error_Reason pre_ErrorReason ,
    AXE_Charger_Event event ,
    AXE_Charger_Type type ,
    AXE_Charging_Error_Reason ErrorReason ,     
    AXE_Charging_State post_state ,
    AXE_Charger_Type post_type ,
    AXE_Charging_Error_Reason post_ErrorReason ,
    int index)  
{
    AXC_FSM_Stress_A_Set_of_Test_Content content;
    int i = index;

    if(index*sizeof(AXC_FSM_Stress_A_Set_of_Test_Content) >= MAX_FEEDING_CONTENT_READ_FROM_FILE){

        i =-1;

        CHARGING_FSM_STRESS_TEST_DEBUG("exceed file buf size");

    }else{

        content.pre_state = (char)pre_state;
        content.pre_type = (char)pre_type;
        content.pre_errorReason = (char)pre_ErrorReason;

        content.event = (char)event;
        content.type = (char)type;
        content.errorReason = (char)ErrorReason;

        content.post_state = (char)post_state;
        content.post_type = (char)post_type;
        content.post_errorReason = (char)post_ErrorReason;

        content.separator = (char)0x0a;

        memcpy(&_this->buf[index*sizeof(AXC_FSM_Stress_A_Set_of_Test_Content)], &content,sizeof(AXC_FSM_Stress_A_Set_of_Test_Content));

        i ++;
    }

    return i;
}

static void AXC_FSM_Stress_Tester_createInputFileForDischargingStateTest(AXC_FSM_Stress_Tester  *_this)
{

    int index = 0;
    int result= 0;

    _this->mImpl.createBuffer(_this);


do{
    AXE_Charging_State pre_state = DISCHARGING_STATE;
    AXE_Charger_Type pre_type = NO_CHARGER_TYPE;
    AXE_Charging_Error_Reason pre_ErrorReason = NO_ERROR;
    AXE_Charger_Event event = CABLE_IN_EVENT;
    AXE_Charger_Type type = LOW_CURRENT_CHARGER_TYPE;
    AXE_Charging_Error_Reason ErrorReason = NO_ERROR;      
    AXE_Charging_State post_state = DISCHARGING_STATE;
    AXE_Charger_Type post_type = NO_CHARGER_TYPE;
    AXE_Charging_Error_Reason post_ErrorReason = NO_ERROR;    
    

    //cable in
    {
        event = CABLE_IN_EVENT;
        type = LOW_CURRENT_CHARGER_TYPE;
        ErrorReason = NO_ERROR;      
        post_state = CHARGING_STATE;
        post_type = LOW_CURRENT_CHARGER_TYPE;
        post_ErrorReason = NO_ERROR;  
    }
    
    result = AXC_FSM_Stress_Tester_copyOneContent(
        _this,
        pre_state,
        pre_type ,
        pre_ErrorReason ,
        event ,
        type ,
        ErrorReason ,     
        post_state ,
        post_type ,
        post_ErrorReason ,
        index
    );

    if(-1 == result){
        break;
    }else{
        index = result;
    }

}while(0);

        _this->filereader.mImpl.createFileForFeeding(&_this->filereader, _this->buf, index*sizeof(AXC_FSM_Stress_A_Set_of_Test_Content));

}
*/
static void AXC_FSM_Stress_Tester_createInputFileForTest(AXC_FSM_Stress_Tester  *_this)
{
/*
    int index = 0;

    _this->mImpl.createBuffer(_this);

#if 1

    {
        AXC_FSM_Stress_A_Set_of_Test_Content content;

        content.state = (char)DISCHARGING_STATE;

        content.event = (char)CABLE_IN_EVENT;

        content.type = (char)NO_CHARGER_TYPE;

        content.errorCode = (char)NO_ERROR;

        content.predictState = (char)DISCHARGING_STATE;

        content.predictType = (char)NO_CHARGER_TYPE;

        content.predictErrorReason =  (char)NO_ERROR;

        content.separator = (char)0x0a;

        index ++;

        memcpy(_this->buf, &content, sizeof(AXC_FSM_Stress_A_Set_of_Test_Content));

    }
#else
{
    AXE_Charging_State state;

        for(state = 0;state <CHARGING_STATE_COUNT; state ++){

            AXE_Charger_Event event;

            for(event = 0; event <CHARGING_EVENT_COUNT; event ++){

                AXE_Charger_Type type;

                for(type =0; type <CHARGING_TYPE_COUNT; type ++){

                    AXE_Charging_Error_Reason reason;

                    for(reason =0; reason <CHARGING_ERROR_COUNT; reason ++){

                        AXC_FSM_Stress_A_Set_of_Test_Content content;

                        content.state = (char)state;

                        content.event = (char)event;

                        content.type = (char)type;

                        content.errorCode = (char)reason;

                        content.predictState = (char)CHARGING_STATE_COUNT;

                        content.predictType = (char)CHARGING_TYPE_COUNT;

                        content.predictErrorReason =  (char)CHARGING_ERROR_COUNT;

                        content.separator = (char)0x0a;

                        memcpy(&_this->buf[index*sizeof(AXC_FSM_Stress_A_Set_of_Test_Content)], &content,sizeof(AXC_FSM_Stress_A_Set_of_Test_Content));

                        index ++;

                        if(index*sizeof(AXC_FSM_Stress_A_Set_of_Test_Content) >= MAX_FEEDING_CONTENT_READ_FROM_FILE){

                            index --;

                            CHARGING_FSM_STRESS_TEST_DEBUG("exceed file buf size");
                        }                                                
                    }

                }
            }
        }
}
#endif
        _this->filereader.mImpl.createFileForFeeding(&_this->filereader, _this->buf, index*sizeof(AXC_FSM_Stress_A_Set_of_Test_Content));
*/
}

static char *AXC_FSM_Stress_Tester_getBuffer(struct AXC_FeedingFileInputParser *parser, unsigned int *size)
{
    struct AXC_FSM_Stress_Tester  *_this=
        container_of(parser, AXC_FSM_Stress_Tester, parser);

    _this->mImpl.createBuffer(_this);

    *size =  MAX_FEEDING_CONTENT_READ_FROM_FILE;
    
    return _this->buf;

}

static void AXC_FSM_Stress_Tester_enable(struct AXC_FSM_Stress_Tester *_this, bool enabled)
{
    if(enabled){

        CHARGING_FSM_STRESS_TEST_DEBUG("enabled...");

        _this->filereader.miParent.test(&_this->filereader.miParent, NULL);
    }
}
void AXC_FSM_Stress_Tester_constructor(AXC_FSM_Stress_Tester *_this)
{
    _this->buf = NULL;
    _this->parser.getBuffer = AXC_FSM_Stress_Tester_getBuffer;
    _this->parser.parse =AXC_FSM_Stress_Tester_parse; 
    AXC_FeedingFileInputTest_constructor(&_this->filereader,
        "FSMtest",
        &_this->parser);
    _this->mImpl.enable = AXC_FSM_Stress_Tester_enable;
    _this->mImpl.createInputFileForTest = AXC_FSM_Stress_Tester_createInputFileForTest;
    _this->mImpl.createBuffer = AXC_FSM_Stress_Tester_createBuffer;

    _this->mImpl.createInputFileForTest(_this);
}

static AXC_FSM_Stress_Tester gTester;
AXC_FSM_Stress_Tester* getFSM_Stress_Tester(void)
{
    AXC_FSM_Stress_Tester *lpTester = NULL;

    if(NULL  == lpTester){

        lpTester = &gTester;

        AXC_FSM_Stress_Tester_constructor(lpTester);
    }

    return lpTester;
}
#endif
