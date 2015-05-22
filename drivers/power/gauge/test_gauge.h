//ASUS_BSP +++ Josh_Liao "sw gauge v2"
/*
        __TEST_GAUGE_H__

*/
#ifndef __TEST_GAUGE_H__
#define __TEST_GAUGE_H__

extern int test_gauge_askCapacity(void);
extern int test_gauge_notifyCableInOut(bool hasCable);
extern int test_gauge_notifyBatFullChanged(bool isFull);
extern int test_gauge_filterCap(void);

#endif //__TEST_GAUGE_H__
//ASUS_BSP --- Josh_Liao "sw gauge v2"

