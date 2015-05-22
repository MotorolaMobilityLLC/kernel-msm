/*                                                                                                                                                       
        Int Sample Data include file

*/
#ifndef __AXC_GAUGE_A66_H__
#define __AXC_GAUGE_A66_H__
#include <linux/types.h>
#include "axi_gauge.h"
#include <linux/workqueue.h>
#include <linux/wakelock.h>

typedef struct AXC_SWGauge {
//ASUS_BSP +++ Josh_Liao "sw gauge v2"
	int mnType;
	int ocvPercent;
	bool firstCalBatCapacity;
	bool hasCable;
	bool isBatFull;
	int Per100OCV;
	int resistorCali;
	int mnVoltageCalibration;
	int mnCurrentCalibration;
	bool mbInited;
	AXI_Gauge_Callback *gaugeCb;
	struct wake_lock calBatCapWlock;
	struct wake_lock calBat100PerOCVWlock;
	struct wake_lock calBatResistorWlock;
	struct delayed_work calBatCapacityWorker;
	struct delayed_work calBat100PerOCVWorker;
	struct delayed_work calBatResistorWorker;
	struct delayed_work readBatResistorWorker;
	struct workqueue_struct *calBatCapacityQueue;
	struct workqueue_struct *gaugeQueue;
        struct AXI_Gauge msParentGauge;	
//ASUS_BSP --- Josh_Liao "sw gauge v2"

} AXC_Gauge_A66;

extern void AXC_Gauge_A66_Constructor(AXI_Gauge *apGauge, int anType, AXI_Gauge_Callback *gaugeCb);
	
#endif //__AXC_GAUGE_A66_H__


