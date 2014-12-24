/*                                                                                                                                                       
        Int Sample Data include file

*/
#ifndef __AXC_SWGAUGE_H__
#define __AXC_SWGAUGE_H__
#include <linux/types.h>
#include "axi_gauge.h"         
#include "../charger/axi_charger.h"                                                                                                                    
#include "sampledata/axi_sampledata.h"

#define SW_GAUGE_SELF_TEST_ENABLE
#ifdef SW_GAUGE_SELF_TEST_ENABLE
#include "../selftest/axi_selftest.h"
#endif //SW_GAUGE_SELF_TEST_ENABLE

#define BATTERY_LIFE_PERCENTAGE_IN_VALID (-1)

#include <linux/workqueue.h>
#include <linux/param.h>
#include <linux/time.h>
#include <linux/wakelock.h>

#include <linux/mutex.h>

#include "../asus_battery.h"

#define TIME_OF_SAMPLING_GAUGE_DATA (5*HZ) //5s
#define TIME_OF_UPDATE_BATTERY_INFO (60*HZ) //60s

/* The delay time need to be updated with the boot time. Such as bootup in charging
 * mode, the battery life presented to user may be not updated in real time, and it
 * shows default value 100 firstly.
 */
#define DALAY_SAMPLING_AFTER_BOOT_UP (25*HZ) //25s

#define DALAY_SAMPLING_AFTER_RESUME (1*HZ/10) //0.1s

#define MAX_PERC_DROP 1
#define MAX_PERC_RAISE 1

#define BATT_CAP_MAH      1500
#define MAX_DROP_MAH      500
#define MAX_RAISE_AC_MAH  800
#define MAX_RAISE_USB_MAH 450

//#define SAMPLE_THREAD_ENABLE

enum swgauge_bat_update_state {
	SWGAUGE_BAT_UPDATE_FIRST = 0,
	SWGAUGE_BAT_UPDATE_CYCLE,
	SWGAUGE_BAT_UPDATE_RESUME,
#ifdef CFG_VF_TUNING
	SWGAUGE_BAT_UPDATE_CAL_OCV,
	SWGAUGE_BAT_UPDATE_CAL_VF,
	SWGAUGE_BAT_UPDATE_CAL_VF_DONE,
#endif /* CFG_VF_TUNING */
	SWGAUGE_BAT_UPDATE_LIMIT
};

typedef struct AXC_SWGauge {

//ASUS_BSP +++ Josh_Liao "sw gauge v2"
	int ocvPercent;
	//bool firstCalBatCapacity;
	bool hasCable;
	bool isBatFull;
	int Per100OCV;
	int resistorCali;
	AXI_Gauge_Callback *gaugeCb;
	struct delayed_work calBatCapacityWorker;
	struct delayed_work calBat100PerOCVWorker;
	struct delayed_work calBatResistorWorker;
	struct delayed_work readBatResistorWorker;
	struct workqueue_struct *calBatCapacityQueue;
	struct workqueue_struct *gaugeQueue;
//ASUS_BSP --- Josh_Liao "sw gauge v2"
	bool mbInited;
	bool mbKeep100Percentage;
	bool mbISChargerStatusChangedAfterResume;
	struct timespec mtTimeWhenCablePluggedOutAnd100Percentage;
	bool mbNeedtoUpdateBatteryLifePercentage;
	bool mbChargeStable;
	int mnType;
	int mnBatteryLifePercentage;	/* battery life to report to OMS */
	unsigned long mnjiffiesFromLastUpdateBattery;
	struct timespec mtLastUpdatedTime;

/* ASUS_BPS +++ Josh_Liao "add get prop func" */
	int m_prop_volt;
	int m_prop_volt_without_cali;
	int m_prop_curr;
	int m_prop_curr_without_cali;
/* ASUS_BPS --- Josh_Liao "add get prop func" */

	int mnVoltageCalibration;
	int mnCurrentCalibration;
	int mnResistorCalibration;

	bool m_is_bat_valid;	/* can not charging when battery is invalid */

	int mn100PercentageOCV;
// joshtest
	int m_truely_bat_life_percent;
	struct mutex swg_mutex_lock;
	AXE_Charger_Type m_prev_update_charger_mode;
	unsigned long m_jiffies_of_suspend;
	/* g_prev_volt_when_charging - previous charging voltage when charging. Sometimes 
 	 * voltage will drop when charging. We need to ignore the drop voltage.
 	 */
	int m_prev_volt_when_charging;

	int m_vf_ocv_base;
	int m_vf_tuning_ratio;

	int m_prev_bat_life_percent;   /* previous battery life percentage */
	AXE_Charger_Type m_charger_mode_before_suspend; /* charger mode before suspend */

#ifdef CFG_CHECK_BAT_LOW_DETECTOR
	bool m_has_bat_low_detector;	/* true when devices has bat low detecor */
#endif /* CFG_CHECK_BAT_LOW_DETECTOR */

	bool m_bat_low;	/* true means bat low happen, maybe by irq or by ocv */
// joshtest

	AXI_Charger *mpCharger;
	AXI_GaugeResultNotifier *mpNotifier;

	AXI_ChargerStateChangeNotifier msNotifier;
#ifdef SAMPLE_THREAD_ENABLE
	AXI_SampleData *mpVoltageSampleData;
	AXI_SampleData *mpCurrentSampleData;
	AXI_SampleData *mpTemperatureSampleData;
#endif
	int mnBatteryUpdateInterval;
    bool mbForceFixedUpdateInterval;
	struct workqueue_struct *mpBatteryUpdateQueue;
	struct delayed_work msBatteryUpdateWorker;
//ASUS BSP Eason_Chang+++  SWgauge resume worker
    struct workqueue_struct *resBatteryUpdateQueue;
	struct delayed_work resumeBatteryUpdateWorker;
//ASUS BSP Eason_Chang---  SWgauge resume worker	
#ifdef SAMPLE_THREAD_ENABLE
		int mnSampleInterval;
		struct delayed_work msSampleWorker;
#endif

	struct wake_lock msReadingBatteryInfoWakeLock;


	int mnDelayAfterBoot;
	struct delayed_work msDelayAfterBootWorker;	

        struct AXI_Gauge msParentGauge;

#ifdef SW_GAUGE_SELF_TEST_ENABLE

        struct AXI_SelfTest msParentSelfTest; 

#endif
} AXC_SWGauge;
extern void AXC_SWGauge_Binding(AXI_Gauge *apGauge,int anType);

//ASUS_BSP +++ Josh_Liao "sw gauge v2"
void AXC_SWGauge_Constructor(AXI_Gauge *apGauge, int anType, AXI_Gauge_Callback *gaugeCb);
//ASUS_BSP --- Josh_Liao "sw gauge v2"	
#endif //__AXC_SWGAUGE_H__

