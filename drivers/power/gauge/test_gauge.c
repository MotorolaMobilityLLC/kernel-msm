//ASUS_BSP +++ Josh_Liao "sw gauge v2"
/*
	test gauge

*/

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/delay.h>


#include "AXI_CapacityFilter.h"
#include "axi_gauge.h"
#include "test_gauge.h"
#include "axc_gaugefactory.h"

#include "axc_gauge_a66.h"
#include "axc_gauge_p02.h"
#include "axc_gauge_dock.h"

#include "../capfilter/axc_cap_filter_factory.h"
#include "../capfilter/axc_cap_filter_a66.h"
#include "../capfilter/axc_cap_filter_p02.h"
#include "../capfilter/axi_cap_filter.h"

static struct AXI_Gauge *gpGaugeV2 = NULL;

static struct AXI_Gauge *gpGaugeP02 = NULL;

static struct AXI_Gauge *gpGaugeDock = NULL;

static AXI_Gauge_Callback gaugeCb;

static AXI_Gauge_Callback gaugeP02Cb;

static AXI_Gauge_Callback gaugeDockCb;

static int lastBatCapA66 = 0;

static int lastBatCapP02 = 0;

static int lastBatCapDock = 0;

static struct AXI_Cap_Filter *gpCapFilterA66 = NULL;

static struct AXI_Cap_Filter *gpCapFilterP02 = NULL;


int gaugeV2_onCapacityReply(struct AXI_Gauge *gauge, struct AXI_Gauge_Callback *gaugeCb, int batCap, int result)
{
	pr_info("[swg][test] %s(), batCap:%d \n" , __func__, batCap);

	if (0 != lastBatCapA66)
		filterCapacity(batCap, lastBatCapA66, false, false, false,  false, 500, 180);

	lastBatCapA66 = batCap;

	return 0;
}

int gaugeV2_askSuspendCharging(struct AXI_Gauge_Callback *gaugeCb)
{

	pr_info("[swg][test] %s() \n",  __func__);

	return 0;
}

int gaugeV2_askResumeCharging(struct AXI_Gauge_Callback *gaugeCb)
{

	pr_info("[swg][test] %s() \n", __func__);

	return 0;
}


int gaugeP02_onCapacityReply(struct AXI_Gauge *gauge, struct AXI_Gauge_Callback *gaugeCb, int batCap, int result)
{
	pr_info("[swg][test] %s(), batCap:%d \n" , __func__, batCap);

	if (0 != lastBatCapP02)
		filterCapacity(batCap, lastBatCapP02, false, false, false,  false, 500, 180);

	lastBatCapP02 = batCap;

	return 0;
}

int gaugeP02_askSuspendCharging(struct AXI_Gauge_Callback *gaugeCb)
{

	pr_info("[swg][test] %s() \n",  __func__);

	return 0;
}

int gaugeP02_askResumeCharging(struct AXI_Gauge_Callback *gaugeCb)
{

	pr_info("[swg][test] %s() \n", __func__);

	return 0;
}


int gaugeDock_onCapacityReply(struct AXI_Gauge *gauge, struct AXI_Gauge_Callback *gaugeCb, int batCap, int result)
{
	pr_info("[swg][test] %s(), batCap:%d \n" , __func__, batCap);

	if (0 != lastBatCapDock)
		filterCapacity(batCap, lastBatCapDock, false, false, false,  false, 500, 180);

	lastBatCapDock = batCap;

	return 0;
}

int gaugeDock_askSuspendCharging(struct AXI_Gauge_Callback *gaugeCb)
{

	pr_info("[swg][test] %s() \n",  __func__);

	return 0;
}

int gaugeDock_askResumeCharging(struct AXI_Gauge_Callback *gaugeCb)
{

	pr_info("[swg][test] %s() \n", __func__);

	return 0;
}

static void init_test_gauge(void)
{
	if (NULL == gpGaugeV2) {
		gaugeCb.onCapacityReply = gaugeV2_onCapacityReply;
		gaugeCb.askSuspendCharging = gaugeV2_askSuspendCharging;
		gaugeCb.askResumeCharging = gaugeV2_askResumeCharging;
		AXC_GaugeFactory_GetGaugeV2(E_SW_GAUGE_V2_TYPE, &gpGaugeV2, &gaugeCb);
	}

	if (NULL == gpGaugeP02) {
		gaugeP02Cb.onCapacityReply = gaugeP02_onCapacityReply;
		gaugeP02Cb.askSuspendCharging = gaugeP02_askSuspendCharging;
		gaugeP02Cb.askResumeCharging = gaugeP02_askResumeCharging;
		AXC_GaugeFactory_GetGaugeV2(E_HW_GAUGE_PAD_TYPE, &gpGaugeP02, &gaugeP02Cb);
	}

	if (NULL == gpGaugeDock) {
		gaugeDockCb.onCapacityReply = gaugeDock_onCapacityReply;
		gaugeDockCb.askSuspendCharging = gaugeDock_askSuspendCharging;
		gaugeDockCb.askResumeCharging = gaugeDock_askResumeCharging;
		AXC_GaugeFactory_GetGaugeV2(E_HW_GAUGE_DOCK_TYPE, &gpGaugeDock, &gaugeDockCb);
	}
	return;
}

static void init_test_cap_filter(void)
{

	if (NULL == gpCapFilterA66) {
		AXC_Cap_Filter_Get(E_CAP_FILTER_PHONE_A66, &gpCapFilterA66, 1500);
	}

	if (NULL == gpCapFilterP02) {
		AXC_Cap_Filter_Get(E_CAP_FILTER_PAD_P02, &gpCapFilterP02, 3300);
	}
}

int test_gauge_askCapacity(void)
{
	init_test_gauge();
	gpGaugeV2->askCapacity(gpGaugeV2);
	gpGaugeP02->askCapacity(gpGaugeP02);
	gpGaugeDock->askCapacity(gpGaugeDock);

	return 0;
}


int test_gauge_notifyCableInOut(bool hasCable)
{
	init_test_gauge();
//	pr_info("[swg][test] notifyCableInOut as false \n");
//	gpGaugeV2->notifyCableInOut(gpGaugeV2, false);
	pr_info("[swg][test] cable : %s\n", hasCable?"in":"out");	
	gpGaugeV2->notifyCableInOut(gpGaugeV2, hasCable);

	return 0;
}


int test_gauge_notifyBatFullChanged(bool isFull)
{
	init_test_gauge();
	pr_info("[swg][test] bat full:%s \n", isFull?"true":"false");	
	gpGaugeV2->notifyBatFullChanged(gpGaugeV2, isFull);


	return 0;
}


int test_gauge_filterCap(void)
{
	int nowCap = 75;
	int lastCap = 80;
	bool hasCable = false;
	bool isCharing = false;
	bool isBatFull = false;
	bool isBatLow = true;
	int maxMah = 500;
	int interval = 180;

	pr_info("[swg][test] %s() \n", __FUNCTION__);	

#if 0
	filterCapacity(nowCap, lastCap, hasCable, isCharing, isBatFull,  isBatLow, maxMah, interval);
#endif

	init_test_cap_filter();

	gpCapFilterA66->filterCapacity(gpCapFilterA66, nowCap, lastCap, hasCable, isCharing, isBatFull,  isBatLow, maxMah, interval);

	gpCapFilterA66->filterCapacity(gpCapFilterA66, 95, 97, true, false, true,  false, 500, 180);

	gpCapFilterA66->filterCapacity(gpCapFilterA66, 94, 100, false, false, false,  false, 500, 180);

	gpCapFilterP02->filterCapacity(gpCapFilterP02, 95, 100, true, false, true,  false, 700, 180);

	gpCapFilterP02->filterCapacity(gpCapFilterP02, 94, 100, false, false, false,  false, 700, 180);

	gpCapFilterP02->filterCapacity(gpCapFilterP02, 0, 5, false, false, false,  false, 700, 180);

	return 0;
}
//ASUS_BSP --- Josh_Liao "sw gauge v2"
