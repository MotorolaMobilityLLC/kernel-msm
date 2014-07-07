/*
	Gauge include file

*/
#ifndef __AXI_GAUGE_H__
#define __AXI_GAUGE_H__
#include <linux/types.h>
#include "../charger/axi_charger.h"

#include "../asus_battery.h"

enum swgauge_bat_update_state;

struct AXI_Gauge;

typedef struct AXI_GaugeResultNotifier{

	void (*UpdateBatteryLifePercentage)(struct AXI_Gauge *apGauge, int anBatteryLifePercentage);
	void (*OnBatteryRemoved)(struct AXI_Gauge *apGauge);

}AXI_GaugeResultNotifier;

//ASUS_BSP +++ Josh_Liao "sw gauge v2"

/* BAT_CAT_REPLY_RESULT */
#define BAT_CAP_REPLY_OK	0
#define BAT_CAP_REPLY_ERR	-1

typedef struct AXI_Gauge_Callback {
    /* onCapacityReply - called when battery capacity calculation done.
    *  @gauge: AXI_Gauge struct
     * @gaugeCb: AXI_Gauge_Callback struct
     * @batCap: battery capacity
     * @result: ok or err happened when calculate
     * RETURNS: 0 means succeed. Others are error code.
     */
    int (*onCapacityReply)(struct AXI_Gauge *gauge, struct AXI_Gauge_Callback *gaugeCb, int batCap, int result);

    /* askSuspendCharging - ask to suspend charging. It also means disable charing.
     * @gaugeCb: AXI_Gauge_Callback struct
     * RETURNS: 0 means succeed. Others are error code.
     */
    int (*askSuspendCharging)(struct AXI_Gauge_Callback *gaugeCb);

    /* askResumeCharging - ask to resume charging. It means resume the present charging state.
     *     It could be charging or discharging.
     * @gaugeCb: AXI_Gauge_Callback struct
     * RETURNS: 0 means succeed. Others are error code.
     */
    int (*askResumeCharging)(struct AXI_Gauge_Callback *gaugeCb);
}AXI_Gauge_Callback;
//ASUS_BSP --- Josh_Liao "sw gauge v2"

typedef struct AXI_Gauge {
//ASUS_BSP +++ Josh_Liao "sw gauge v2"	
	/* askCapacity - ask battery capacity by gauge. It may be sw gauge or hw gauge.
	* It is asynchronization. The result shall be replied by onCapacityReply().
	* @gauge: AXI_Gauge gauge structure
	* RETURNS: 0 means succeed. Others are error code. 
	*/
	int (*askCapacity)(struct AXI_Gauge *gauge);

	/* getNextPollingInterval - get next polling interval.
	* @gauge: AXI_Gauge gauge structure
	* RETURNS: next polling interval. Return value unit is second.  
	*/
	int (*getNextPollingInterval)(struct AXI_Gauge *gauge);

	/* notifyCableInOut - notfiy cable in or out
	* @gauge: AXI_Gauge gauge structure
	* @hasCable: has cable
	* RETURNS:
	*/	
	int (*notifyCableInOut)(struct AXI_Gauge *gauge, bool hasCable);

	/* notifyBatFullChanged - notfiy bat full or bat become not full from full
	* @gauge: AXI_Gauge gauge structure
	* @isFull: is full?
	* RETURNS:
	*/
	int (*notifyBatFullChanged)(struct AXI_Gauge *gauge, bool isFull);

	/* isBatExisted - tell if battery existed
	* @gauge: AXI_Gauge gauge structure
	* RETURNS: true or false
	*/
	bool (*isBatExisted)(struct AXI_Gauge *gauge);
//ASUS_BSP --- Josh_Liao "sw gauge v2"
	
	void (*Init)(struct AXI_Gauge *apGauge, AXI_Charger *apCharger,AXI_GaugeResultNotifier *apNotifier);
	void (*DeInit)(struct AXI_Gauge *apGauge);
        int (*GetType)(struct AXI_Gauge *apGauge);
        void (*SetType)(struct AXI_Gauge *apGauge,int anType);
	int (*ReadVoltage)(struct AXI_Gauge *apGauge,unsigned int anTimes);
	int (*ReadCurrent)(struct AXI_Gauge *apGauge,unsigned int anTimes);
	int (*SetConfig)(struct AXI_Gauge *apGauge);
	bool (*IsBatValid)(struct AXI_Gauge *apGauge);
/* ASUS_BPS +++ Josh_Liao "add function to read voltage and current simultaneously" */
	void (*ReadVoltCurr)(struct AXI_Gauge *apGauge, unsigned int anTimes, int *volt, int *curr);
	void (*ReadVoltCurrWithoutCali)(struct AXI_Gauge *apGauge, unsigned int anTimes, int *volt, int *curr);
/* ASUS_BPS --- Josh_Liao "add function to read voltage and current simultaneously" */
	int (*ReadVoltageWithoutCalibration)(struct AXI_Gauge *apGauge,unsigned int anTimes);
	int (*ReadCurrentWithoutCalibration)(struct AXI_Gauge *apGauge,unsigned int anTimes);
	int (*ReloadFactoryCalibration)(struct AXI_Gauge *apGauge);

/* ASUS_BPS +++ Josh_Liao "add get prop func" */
	int (*GetPropVolt)(struct AXI_Gauge *apGauge);
	int (*GetPropVoltWithoutCali)(struct AXI_Gauge *apGauge);
	int (*GetPropCurr)(struct AXI_Gauge *apGauge);
	int (*GetPropCurrWithoutCali)(struct AXI_Gauge *apGauge);
/* ASUS_BPS --- Josh_Liao "add get prop func" */

// joshtest

#ifdef CFG_REF_LAST_BAT_INFO
	int (*LoadLastBatInfo)(struct AXI_Gauge *apGauge);
	int (*SaveLastBatInfo)(struct AXI_Gauge *apGauge);
#endif /* CFG_REF_LAST_BAT_INFO */

	void (*UpdateBatInfo)(struct AXI_Gauge *apGauge, enum swgauge_bat_update_state set_state);
	void (*SetBatLow)(struct AXI_Gauge *apGauge, bool is_bat_low);
	bool (*GetBatLow)(struct AXI_Gauge *apGauge);
#ifdef CFG_CHECK_BAT_LOW_DETECTOR
	void (*SetBatLowDetector)(struct AXI_Gauge *apGauge, bool has_bat_low_detector);
	bool (*GetBatLowDetector)(struct AXI_Gauge *apGauge);
#endif /* CFG_CHECK_BAT_LOW_DETECTOR */
// joshtest

	int (*ReadTemperature)(struct AXI_Gauge *apGauge);
	int (*GetBatteryLife)(struct AXI_Gauge *apGauge);
	int (*Resume)(struct AXI_Gauge *apGauge);
	int (*Suspend)(struct AXI_Gauge *apGauge);
	int (*OnCableStatueChanged)(struct AXI_Gauge *apGauge);
	int (*SetFixedUpdateInterval)(struct AXI_Gauge *apGauge,unsigned int anInterval);
	int (*IsBatteryRemoved)(struct AXI_Gauge *apGauge);

	int (*GetVoltageCalibrationVal)(struct AXI_Gauge *apGauge);
	int (*GetCurrentCalibrationVal)(struct AXI_Gauge *apGauge);
	int (*GetResistorCalibrationVal)(struct AXI_Gauge *apGauge);
}AXI_Gauge;

#endif //__AXI_GAUGE_H__
