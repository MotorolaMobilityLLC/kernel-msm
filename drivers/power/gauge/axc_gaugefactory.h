/*
        GaugeFactory include file

*/
#ifndef __AXC_GAUGEFACTORY_H__
#define __AXC_GAUGEFACTORY_H__
#include "axi_gauge.h"

typedef enum {
//ASUS_BSP +++ Josh_Liao "sw gauge v2"
        E_SW_GAUGE_V2_TYPE,
        E_HW_GAUGE_TYPE,
        E_HW_GAUGE_PAD_TYPE,
        E_HW_GAUGE_DOCK_TYPE,
        E_GAUGE_TYPE_COUNT,
//ASUS_BSP --- Josh_Liao "sw gauge v2"

}AXE_SW_GAUGE_TYPE;

//ASUS_BSP +++ Josh_Liao "sw gauge v2"
extern void AXC_GaugeFactory_GetGaugeV2(AXE_SW_GAUGE_TYPE aeGaugeType , AXI_Gauge **appGauge, AXI_Gauge_Callback *gaugeCb);
extern void AXC_GaugeFactory_FreeGaugeV2(AXI_Gauge *apGauge);
//ASUS_BSP --- Josh_Liao "sw gauge v2"

#endif //__AXC_GAUGEFACTORY_H__


