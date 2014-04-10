#include <linux/kernel.h>
#ifdef CONFIG_EEPROM_NUVOTON
#include <linux/microp_api.h>
#endif /*CONFIG_EEPROM_NUVOTON */
#include "axc_gauge_p02.h"

/***********************************************************************************************/
#define OCV_PER_SPEEDUP_UPDATE 12
#define PAD_BAT  0

/* UPDATE_INERVAL */
#define UPDATE_INERVAL_DEFAULT          180
#define UPDATE_INERVAL_BAT_LOW_IRQ       60



/***********************************************************************************************/
static int get_p02_bat_cap_byhw(void)
{
#ifdef CONFIG_EEPROM_NUVOTON
	int bat_cap = AX_MicroP_readBattCapacity(PAD_BAT);
#else
	int bat_cap = -1;
#endif /* CONFIG_EEPROM_NUVOTON */

	if (bat_cap < 0) {
		pr_err( "[BAT]error!! in %s(), read bat capacity error\n", __FUNCTION__);
	}
	
	return bat_cap;
}

/***********************************************************************************************/





bool AXC_Gauge_P02_IsBatExisted(struct AXI_Gauge *gauge)
{
//	AXC_Gauge_P02 *this = container_of(gauge, AXC_Gauge_P02, msParentGauge);
#ifdef CONFIG_EEPROM_NUVOTON  //ASUS_BSP Eason_Chang 1120 porting +++
	if (1 == AX_MicroP_IsP01Connected())
		return true;
	else
		return false;
#else//ASUS_BSP Eason_Chang 1120 porting 
        return false;
#endif //CONFIG_EEPROM_NUVOTON//ASUS_BSP Eason_Chang 1120 porting ---
}

int AXC_Gauge_P02_AskCapacity(struct AXI_Gauge *gauge)
{
	int cap;
	AXC_Gauge_P02 *this = container_of(gauge, AXC_Gauge_P02, msParentGauge);
	BUG_ON(NULL == gauge);

	cap = get_p02_bat_cap_byhw();

	this->ocvPercent = cap;
	this->gaugeCb->onCapacityReply(&this->msParentGauge, this->gaugeCb, cap, BAT_CAP_REPLY_OK);
	
	return 0;
}

void AXC_Gauge_P02_SetType(AXI_Gauge *apGauge ,int anType)
{
	AXC_Gauge_P02 *this = container_of(apGauge, AXC_Gauge_P02, msParentGauge);
	this->mnType = anType;

}

int AXC_Gauge_P02_GetType(AXI_Gauge *apGauge)
{
	AXC_Gauge_P02 *this = container_of(apGauge, AXC_Gauge_P02, msParentGauge);
	return this->mnType;
}


int AXC_Gauge_P02_GetNextPollingInterval(struct AXI_Gauge *gauge)
{
	int interval;
	AXC_Gauge_P02 *this = container_of(gauge, AXC_Gauge_P02, msParentGauge);

	BUG_ON(NULL == gauge);

	if (this->ocvPercent < OCV_PER_SPEEDUP_UPDATE)
		interval = UPDATE_INERVAL_BAT_LOW_IRQ;
	else
		interval = UPDATE_INERVAL_DEFAULT;

	pr_debug("[BAT][Gau]next polling interval:%d\n", interval);

	return interval;
}

int AXC_Gauge_P02_NotifyCableInOut(struct AXI_Gauge *gauge, bool hasCable)
{

	return 0;
}

int AXC_Gauge_P02_NotifyBatFullChanged(struct AXI_Gauge *gauge, bool isFull)
{

	return 0;
}



void AXC_Gauge_P02_Constructor(AXI_Gauge *apGauge, int anType, AXI_Gauge_Callback *gaugeCb)
{
	AXC_Gauge_P02 *this = container_of(apGauge, AXC_Gauge_P02, msParentGauge);

	BUG_ON((NULL == gaugeCb) || (NULL == apGauge));

	this->gaugeCb = gaugeCb;
	this->msParentGauge.SetType = AXC_Gauge_P02_SetType;
	this->msParentGauge.GetType = AXC_Gauge_P02_GetType;	
	this->msParentGauge.askCapacity = AXC_Gauge_P02_AskCapacity;
	this->msParentGauge.isBatExisted = AXC_Gauge_P02_IsBatExisted;
	this->msParentGauge.getNextPollingInterval = AXC_Gauge_P02_GetNextPollingInterval;
	this->msParentGauge.notifyCableInOut = AXC_Gauge_P02_NotifyCableInOut;
	this->msParentGauge.notifyBatFullChanged = AXC_Gauge_P02_NotifyBatFullChanged;

	this->msParentGauge.SetType(apGauge,anType);

}



