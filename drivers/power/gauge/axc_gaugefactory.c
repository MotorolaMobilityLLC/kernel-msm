#include <linux/slab.h>
#include "axc_gaugefactory.h"

#include "../axi_powerglobal.h"
#include <linux/asus_bat.h>
#include <linux/kernel.h>

#include "axc_gauge_a66.h"

#ifdef CONFIG_EEPROM_NUVOTON
#include "axc_gauge_p02.h"
#include "axc_gauge_dock.h"
#endif

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
#ifdef CONFIG_EEPROM_NUVOTON
		case E_HW_GAUGE_PAD_TYPE:
		{
		AXC_Gauge_P02 *gaugeP02 = NULL;
		gaugeP02 = kzalloc(sizeof(AXC_Gauge_P02), GFP_KERNEL);
		BUG_ON(NULL == gaugeP02);
		*appGauge = &gaugeP02->msParentGauge;
		AXC_Gauge_P02_Constructor(*appGauge, aeGaugeType, gaugeCb);
		}
		break;
		case E_HW_GAUGE_DOCK_TYPE:
		{
		AXC_Gauge_Dock *gaugeDock = NULL;
		gaugeDock = kzalloc(sizeof(AXC_Gauge_Dock), GFP_KERNEL);
		BUG_ON(NULL == gaugeDock);
		*appGauge = &gaugeDock->msParentGauge;
		AXC_Gauge_Dock_Constructor(*appGauge, aeGaugeType, gaugeCb);
		}
		break;
#endif
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
#ifdef CONFIG_EEPROM_NUVOTON
			case E_HW_GAUGE_PAD_TYPE:
			{
			AXC_Gauge_P02 *gaugeP02 = container_of(apGauge, AXC_Gauge_P02, msParentGauge);
			kfree(gaugeP02);
			}
			break;
			case E_HW_GAUGE_DOCK_TYPE:
			{
			AXC_Gauge_Dock *gaugeDock = container_of(apGauge, AXC_Gauge_Dock, msParentGauge);
			kfree(gaugeDock);
			}
			break;
#endif
			default:
			pr_err("[GaugeFact]Error!!%s(), Not defined type...\n", __func__);
			break;
		}
	}

	return;
}

//ASUS_BSP --- Josh_Liao "sw gauge v2"
