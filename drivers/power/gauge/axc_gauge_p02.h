/*                                                                                                                                                       
        include file

*/
#ifndef __AXC_GAUGE_P02_H__
#define __AXC_GAUGE_P02_H__
#include "axi_gauge.h"   


typedef struct AXC_Gauge_P02 {
	int mnType;
	int ocvPercent;
        struct AXI_Gauge msParentGauge;
	AXI_Gauge_Callback *gaugeCb;
} AXC_Gauge_P02;

void AXC_Gauge_P02_Constructor(AXI_Gauge *apGauge, int anType, AXI_Gauge_Callback *gaugeCb);

#endif //__AXC_GAUGE_P02_H__

