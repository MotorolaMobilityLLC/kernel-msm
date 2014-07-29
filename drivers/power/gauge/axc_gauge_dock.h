/*                                                                                                                                                       
        include file

*/
#ifndef __AXC_GAUGE_DOCK_H__
#define __AXC_GAUGE_DOCK_H__
#include "axi_gauge.h"   


typedef struct AXC_Gauge_Dock {
	int mnType;
	int ocvPercent;
        struct AXI_Gauge msParentGauge;
	AXI_Gauge_Callback *gaugeCb;
} AXC_Gauge_Dock;

void AXC_Gauge_Dock_Constructor(AXI_Gauge *apGauge, int anType, AXI_Gauge_Callback *gaugeCb);

#endif // __AXC_GAUGE_DOCK_H__

