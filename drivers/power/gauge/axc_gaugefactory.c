#include <linux/slab.h>
#include "axc_gaugefactory.h"

#include "../axi_powerglobal.h"
#include <linux/asus_bat.h>
#include <linux/kernel.h>

#include "axc_gauge_a66.h"

//ASUS_BSP +++ Josh_Liao "sw gauge v2"
extern void AXC_GaugeFactory_GetGaugeV2(AXE_SW_GAUGE_TYPE aeGaugeType , AXI_Gauge **appGauge, AXI_Gauge_Callback *gaugeCb)
{
	if (NULL != *appGauge) {
		printk("[BAT][Gau]Memory leak...\n");
		return;
	}

	if (NULL == gaugeCb) {
		pr_err( "[BAT][Gau] Error!!%s(), null ptr\n", __func__);
		return;
	}	

	*appGauge =  NULL;
	switch(aeGaugeType)
	{
		case E_SW_GAUGE_V2_TYPE:
		{
			AXC_Gauge_A66 *lpSWGauge = NULL;
			lpSWGauge = kzalloc(sizeof(AXC_Gauge_A66), GFP_KERNEL);
			BUG_ON(NULL == lpSWGauge);
			*appGauge = &lpSWGauge->msParentGauge;
			AXC_Gauge_A66_Constructor(*appGauge, aeGaugeType, gaugeCb);
		}
		break;
		default:
			pr_err("[BAT][Gau]Error!!%s(), Not defined type...\n", __func__);
		break;
	}

	return;
}

extern void AXC_GaugeFactory_FreeGaugeV2(AXI_Gauge *apGauge)
{
 	if(NULL != apGauge)
 	{
		switch(apGauge->GetType(apGauge))
		{
			case E_SW_GAUGE_V2_TYPE:
			{
				AXC_Gauge_A66 *lpSWGauge = container_of(apGauge, AXC_Gauge_A66, msParentGauge);
				kfree(lpSWGauge);
			}
			break;
			default:
				pr_err("[GaugeFact]Error!!%s(), Not defined type...\n", __func__);
			break;
		}
	}

	return;
}

//ASUS_BSP --- Josh_Liao "sw gauge v2"
