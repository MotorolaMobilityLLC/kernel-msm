#include <linux/kernel.h>
#ifdef CONFIG_EEPROM_NUVOTON
#include <linux/microp_api.h>
#endif /*CONFIG_EEPROM_NUVOTON */
#include "axc_gauge_dock.h"

/***********************************************************************************************/
#define OCV_PER_SPEEDUP_UPDATE 12
#define PAD_BAT  0
#define DOCK_BAT  1

/* UPDATE_INERVAL */
#define UPDATE_INERVAL_DEFAULT          180
#define UPDATE_INERVAL_BAT_LOW_IRQ       60



/***********************************************************************************************/
static int get_dock_bat_cap_byhw(void)
{
#ifdef CONFIG_EEPROM_NUVOTON
	int bat_cap = AX_MicroP_readBattCapacity(DOCK_BAT);
#else
	int bat_cap = -1;
#endif /* CONFIG_EEPROM_NUVOTON */

	if (bat_cap < 0) {
		pr_err( "[BAT]error!! in %s(), read bat capacity error\n", __FUNCTION__);
	}
	
	return bat_cap;
}

/***********************************************************************************************/





bool AXC_Gauge_Dock_IsBatExisted(struct AXI_Gauge *gauge)
{

	return false;

}

int AXC_Gauge_Dock_AskCapacity(struct AXI_Gauge *gauge)
{
	int cap;
	AXC_Gauge_Dock *this = container_of(gauge, AXC_Gauge_Dock, msParentGauge);
	BUG_ON(NULL == gauge);

	cap = get_dock_bat_cap_byhw();

	this->ocvPercent = cap;
	this->gaugeCb->onCapacityReply(&this->msParentGauge, this->gaugeCb, cap, BAT_CAP_REPLY_OK);
	
	return 0;
}

void AXC_Gauge_Dock_SetType(AXI_Gauge *apGauge ,int anType)
{
	AXC_Gauge_Dock *this = container_of(apGauge, AXC_Gauge_Dock, msParentGauge);
	this->mnType = anType;

}

int AXC_Gauge_Dock_GetType(AXI_Gauge *apGauge)
{
	AXC_Gauge_Dock *this = container_of(apGauge, AXC_Gauge_Dock, msParentGauge);
	return this->mnType;
}


int AXC_Gauge_Dock_GetNextPollingInterval(struct AXI_Gauge *gauge)
{
	int interval;
	AXC_Gauge_Dock *this = container_of(gauge, AXC_Gauge_Dock, msParentGauge);

	BUG_ON(NULL == gauge);

	if (this->ocvPercent < OCV_PER_SPEEDUP_UPDATE)
		interval = UPDATE_INERVAL_BAT_LOW_IRQ;
	else
		interval = UPDATE_INERVAL_DEFAULT;

	pr_debug("[BAT][Gau]next polling interval:%d\n", interval);

	return interval;
}

int AXC_Gauge_Dock_NotifyCableInOut(struct AXI_Gauge *gauge, bool hasCable)
{

	return 0;
}

int AXC_Gauge_Dock_NotifyBatFullChanged(struct AXI_Gauge *gauge, bool isFull)
{

	return 0;
}



void AXC_Gauge_Dock_Constructor(AXI_Gauge *apGauge, int anType, AXI_Gauge_Callback *gaugeCb)
{
	AXC_Gauge_Dock *this = container_of(apGauge, AXC_Gauge_Dock, msParentGauge);

	BUG_ON((NULL == gaugeCb) || (NULL == apGauge));

	this->gaugeCb = gaugeCb;
	this->msParentGauge.SetType = AXC_Gauge_Dock_SetType;
	this->msParentGauge.GetType = AXC_Gauge_Dock_GetType;	
	this->msParentGauge.askCapacity = AXC_Gauge_Dock_AskCapacity;
	this->msParentGauge.isBatExisted = AXC_Gauge_Dock_IsBatExisted;
	this->msParentGauge.getNextPollingInterval = AXC_Gauge_Dock_GetNextPollingInterval;
	this->msParentGauge.notifyCableInOut = AXC_Gauge_Dock_NotifyCableInOut;
	this->msParentGauge.notifyBatFullChanged = AXC_Gauge_Dock_NotifyBatFullChanged;

	this->msParentGauge.SetType(apGauge,anType);

}



