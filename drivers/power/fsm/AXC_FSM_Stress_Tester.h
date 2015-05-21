/*
        FSM Stress Test include file

*/
#ifndef __AXC_FSM_STRESS_TESTER_H__
#define __AXC_FSM_STRESS_TESTER_H__
#include "../util/AXC_FeedingFileInputTest.h"
#include "AXC_Charging_FSM.h"

typedef struct AXC_FSM_Stress_A_Set_of_Test_Content {
    char pre_state;
    char pre_type;
    char pre_errorReason;
    char event;
    char type;
    char errorReason;    
    char post_state;
    char post_type;
    char post_errorReason;
    char separator;
}AXC_FSM_Stress_A_Set_of_Test_Content;

struct AXC_FSM_Stress_Tester;
struct AXC_FSM_Stress_Tester_Impl;
typedef struct AXC_FSM_Stress_Tester_Impl {
    void (*createBuffer)(struct AXC_FSM_Stress_Tester  *_this);
    void (*createInputFileForTest)(struct AXC_FSM_Stress_Tester  *_this);
    void (*enable)(struct AXC_FSM_Stress_Tester *_this, bool enabled);
}AXC_FSM_Stress_Tester_Impl;

typedef struct AXC_FSM_Stress_Tester {
    char *buf;
    AXC_FeedingFileInputParser parser;
    AXC_FeedingFileInputTest filereader;
    AXC_FSM_Stress_Tester_Impl mImpl;
}AXC_FSM_Stress_Tester;

extern AXC_FSM_Stress_Tester* getFSM_Stress_Tester(void);
#endif //__AXC_FSM_STRESS_TESTER_H__

