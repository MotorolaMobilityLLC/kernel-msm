/*
         bat test interface include file

*/
#ifndef __AXC_BATTERY_SERVICE_TEST_H__
#define __AXC_BATTERY_SERVICE_TEST_H__
#include <linux/types.h>


struct AXC_BatteryServiceTest;


typedef struct AXC_BatteryServiceTest{
    int pollingInterval;
    time_t filterLastUpdateInterval;
    bool(*ifFixedPollingInterval)(struct AXC_BatteryServiceTest *test);
    bool(*ifFixedFilterLastUpdateInterval)(struct AXC_BatteryServiceTest *test);    
    void(*changePollingInterval)(struct AXC_BatteryServiceTest *test,bool fixed,int interval);
    void(*changeFilterLastUpdateInterval)(struct AXC_BatteryServiceTest *test,bool fixed,int interval);
}AXC_BatteryServiceTest;

extern AXC_BatteryServiceTest *getBatteryServiceTest(void);


#endif //__AXC_BATTERY_SERVICE_TEST_H__


