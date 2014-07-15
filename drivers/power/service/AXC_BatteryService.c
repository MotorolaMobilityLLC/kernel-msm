#ifdef CONFIG_BATTERY_ASUS_SERVICE
#include <AXC_BatteryService.h>
#include <linux/kernel.h>
#include <linux/rtc.h>
//ASUS BSP Eason_Chang +++ batteryservice to fsm
#include "../fsm/AXC_Charging_FSM.h"
AXC_Charging_FSM *lpFSM;
//ASUS BSP Eason_Chang --- batteryservice to fsm
//ASUS BSP Eason_Chang +++ batteryservice to gauge
#include "../gauge/axc_gaugefactory.h"
#include "../gauge/axi_gauge.h"
#include "../gauge/AXI_CapacityFilter.h"
#include "../capfilter/axc_cap_filter_factory.h"
#include "../capfilter/axi_cap_filter.h"
#include "../capfilter/axc_cap_filter_a66.h"
#include "../capfilter/axc_cap_filter_p02.h"
#include <linux/time.h>
//ASUS BSP Eason_Chang --- batteryservice to gauge
//ASUS_BSP +++ Eason_Chang BalanceMode

#include <linux/asus_chg.h>
#include <linux/notifier.h>

#ifdef CONFIG_EEPROM_NUVOTON 
#include <linux/microp_api.h> 
#include <linux/microp_pin_def.h>
#include <linux/microp_notify.h>
#include <linux/microp.h> 
#endif //CONFIG_EEPROM_NUVOTON/

//ASUS_BSP +++ Peter_lu "suspend for Battery0% in  fastboot mode issue"
#ifdef CONFIG_FASTBOOT
#include <linux/fastboot.h>
#endif //#ifdef CONFIG_FASTBOOT
//ASUS_BSP ---

//Eason: do ForcePowerBankMode+++
extern int uP_nuvoton_write_reg(int cmd, void *data);
//Eason: do ForcePowerBankMode---

//[ChiaYuan][+++]Add for Broadcast busy state
#include "../../../kernel/power/power.h"
#include <linux/suspend.h>
#define HIGH_POWER_TIMEOUT   1000*60*60		//1 hour timeout
int Total_BatCur = 0;
int HighPower_Count = 0;
bool bTimerEnable = false;
int IsHighPower = 0;
extern suspend_state_t pm_autosleep_state(void);
void HighPower_timer_expired(unsigned long data);
DEFINE_TIMER(HighPower_timer, HighPower_timer_expired, 0, 0);	
//[ChiaYuan][---]Add for Broadcast busy state

#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include "../charger/axi_charger.h" 

#ifdef CONFIG_EEPROM_NUVOTON
static int IsBalanceMode = 1;//default 1.  0:PowerbankMode, 1:balanceMode, 2:ForcePowerBankMode
extern int IsBalanceTest(void);
extern int GetBalanceModeStartRatio(void);
extern int GetBalanceModeStopRatio(void);
extern int GetBalanceModeA66CAP(void);
extern int BatteryServiceGetPADCAP(void);
extern int BatteryServiceReportPADCAP(void);
static int IsBalanceCharge = 1;
static int IsPowerBankCharge = 1;
static int LastTimeIsBalMode = 1;
//Eason: A68 new balance mode +++
#ifndef ASUS_FACTORY_BUILD
static bool IsSystemdraw = false;
static bool IsBalanceSuspendStartcharge = false;
static bool IsKeepChgFrom15pTo19p = false;//Eason:balance mode keep charge from Cap 15 to 19
#endif
//Eason: A68 new balance mode ---
#endif //CONFIG_EEPROM_NUVOTON
static struct AXC_BatteryService *balance_this=NULL;

//ASUS_BSP --- Eason_Chang BalanceMode

//ASUS_BSP +++ Eason_Chang add event log +++
#include <linux/asusdebug.h>
//ASUS_BSP +++ Eason_Chang add event log ---
#include <linux/wakelock.h>
#include <linux/gpio.h> //Eason:get cable In/Out at first time ask Cap

//ASUS_BSP porting charger mode +++
#if defined(ASUS_CHARGING_MODE) && !defined(ASUS_FACTORY_BUILD)
int g_chg_present;
#endif
//ASUS_BSP porting charger mode ---

//Eason: charger_limit_enable is used to decide if Do_Factory5060Mode+++
#ifdef ASUS_FACTORY_BUILD
static bool charger_limit_enable = false;//only do 5060 mode in factory branch, let bsp version can normal charging
bool g_5060modeCharging = true;//only set online 0 to show Notcharging icon in factory branch, let bsp version can normal show online status
#endif
//Eason: charger_limit_enable is used to decide if Do_Factory5060Mode---

#ifdef CONFIG_PM_8226_CHARGER
extern int pm8226_is_ac_usb_in(void);
extern int pm8226_is_full(void);
extern int pm8226_get_prop_batt_temp(void);
#endif

//ASUS_BSP +++ Eason_Chang charger
extern AXI_Charger * getAsusCharger(void);
static struct AXI_Charger *gpCharger = NULL;
//ASUS_BSP --- Eason_Chang charger

#ifdef CONFIG_EEPROM_NUVOTON
extern void BatteryService_P02update(void);
extern void asus_bat_update_PadAcOnline(void);
#endif

extern bool reportRtcReady(void);

#define SUSPEND_DISCHG_CURRENT 10
#define MAX_DISCHG_CURRENT    700
#define USB_CHG_CURRENT       500
#define A91_ILLEGAL_CHG_CURRENT       500
#define USB3p0_ILLEGAL_CURRENT		900
#ifdef CONFIG_IDTP9023_CHARGER  
#define WIRELESS_CHG_CURRENT        700
#endif
#define PAD_CHG_CURRENT       900

#define AC_CHG_CURRENT        900
#define AC_SUSPEND_CHG_CURRENT 1200

#define BAT_CAP_REPLY_ERR	-1
#define RESUME_UPDATE_TIME   600      //10 min
#define RESUME_UPDATE_TIMEwhenCapLess20  600  //10min
#define RESUME_UPDATE_TIMEwhenBATlow  300  //10min
#define FORCERESUME_UPDATE_TIME   300  //5 min
#define DOCKRESUME_UPDATE_TIME   300  //5 min
#define RTC_READY_DELAY_TIME   20
#define KEEP_CAPACITY_TIME 300

//Hank temperature monitor+++
#define DEFAULT_POLLING_INTERVAL 180
#define DEFAULT_MONITOR_INTERVAL 60
//Hank temperature monitor---

//Eason:cap queue  interval plugs a small value, prevent less than target interval
#define DEFAULT_CAP_QUEUE_INTERVAL_PLUGGED 2

#define USE_SW_GAUGE 1

//Hank enterRomMode_test++++
#ifdef CONFIG_TI_GAUGE
int enterRomMode_test = 0;//0:gauge active mode, 1:gauge Rom mode
//Hank unlock gauge+++
extern void TIgauge_LockStep(void);
extern void TIgauge_UnlockStep1(void);
extern void TIgauge_UnlockStep2(void);
//Hank unlock gauge---
extern void TIgauge_EnterRomMode(void);
extern void exitRomMode(void);
extern bool  check_is_RomMode(void);
extern bool  check_is_ActiveMode(void);
#endif
//Hank enterRomMode_test----

//Hank read BatteryID+++
extern int64_t read_battery_id(void);
//Hank read BatteryID---

//Eason set alarm +++
#include <linux/alarmtimer.h>
static struct alarm bat_alarm;
static struct alarm batLow_alarm;
static struct alarm cableIn_alarm;
static DEFINE_SPINLOCK(bat_alarm_slock);
static DEFINE_SPINLOCK(batLow_alarm_slock);
static DEFINE_SPINLOCK(cableIn_alarm_slock);
struct wake_lock bat_alarm_wake_lock;
struct wake_lock batLow_alarm_wake_lock;
struct wake_lock cableIn_alarm_wake_lock;
static DECLARE_WAIT_QUEUE_HEAD(bat_alarm_wait_queue);
static DECLARE_WAIT_QUEUE_HEAD(batLow_alarm_wait_queue);
static DECLARE_WAIT_QUEUE_HEAD(cableIn_alarm_wait_queue);
static uint32_t alarm_enabled;
static uint32_t batLowAlarm_enabled;
static uint32_t cableInAlarm_enabled;
#define RTCSetInterval 610
//Eason: dynamic set Pad alarm +++
//#define RTCSetIntervalwhenCapLess20  610

#ifndef ASUS_FACTORY_BUILD
#define RTCSetIntervalwhenBalSuspendStopChg 3610
#define RTCSetIntervalwhenAlarmIntervalLess3min 190
#ifdef CONFIG_EEPROM_NUVOTON    
static int RTCSetIntervalwhenBalanceMode = RTCSetInterval;
#endif
static bool InSuspendNeedDoPadAlarmHandler=false;//in suspend set true, in late resume set false,
												//Pad alarm handler need to do only when display off												
#endif //ASUS_FACTORY_BUILD
//Eason: dynamic set Pad alarm ---
#define RTCSetIntervalwhenBATlow  310
#define RTCSetIntervalwhenCABLEIn  1810
#define CapChangeRTCInterval 20
//Eason set alarm ---
// when A66 Cap = 0% shutdown device no matter if has cable+++ 
extern bool g_AcUsbOnline_Change0;
extern void AcUsbPowerSupplyChange(void);
#ifdef CONFIG_EEPROM_NUVOTON
extern void Pad_AC_PowerSupplyChange(void);
#endif
// when A66 Cap = 0% shutdown device no matter if has cable---
//Eason boot up in BatLow situation, take off cable can shutdown+++
extern bool g_BootUp_IsBatLow;
//Eason boot up in BatLow situation, take off cable can shutdown---
//Eason : In suspend have same cap don't update savedTime +++
bool SameCapDontUpdateSavedTime = false;
extern bool g_RTC_update;
//Eason : In suspend have same cap don't update savedTime ---
//Eason : prevent thermal too hot, limit charging current in phone call+++
extern bool g_audio_limit;
static bool IsPhoneOn = false;
//Eason : prevent thermal too hot, limit charging current in phone call---
//Eason : when thermal too hot, limit charging current +++ 
extern bool g_padMic_On; 
extern int g_thermal_limit;
static bool IsThermalHot = false;
//Eason : when thermal too hot, limit charging current ---

//Eason judge smb346 full +++
#ifdef CONFIG_SMB_346_CHARGER
extern bool smb346_IsFull(void);
#endif
//Eason judge smb346 full ---
//ASUS BSP Eason add A68 charge mode +++
extern void setFloatVoltage(int StopPercent);
//ASUS BSP Eason add A68 charge mode ---
//Eason: AICL work around +++
bool g_alreadyCalFirstCap = false;
//Eason: AICL work around ---
//Eason: choose Capacity type SWGauge/BMS +++
int g_CapType = 0;// 0:SWgauge 1:BMS
#define DEFAULT_CAP_TYPE_VALUE 0
extern int get_BMS_capacity(void);
//Eason: choose Capacity type SWGauge/BMS ---
//ASUS BSP: Eason check correct BMS RUC+++
extern bool gIsBMSerror;
//ASUS BSP: Eason check correct BMS RUC---
//Eason get BMS Capacity for EventLog+++
static int gBMS_Cap=50;
//Eason get BMS Capacity for EventLog---
//Eason show BMS cap to user+++
static int g_SWgauge_lastCapacity = 50;
//Eason show BMS cap to user---
//Eason:fix Cap drop too slowly in unattended mode+++
int filRealSusT;   //filter Real Suspend Time
//Eason:fix Cap drop too slowly in unattended mode---
//Eason: if BatLow keep 15 min, shutdown devices+++
static time_t batLowTriggerTime;
bool g_batLowLongTimeShut = false;
#define BATLOW_KEEPTIME_SHUTDOWN 900 //15min
//Eason: if BatLow keep 15 min, shutdown devices---
//Eason : if last time is 10mA +++
static bool IsLastTimeMah10mA = false;
static bool IfUpdateSavedTime = false;	
//Eason : if last time is 10mA ---
//Eason: Pad draw rule +++
#include "../charger/axc_Smb346Charger.h"
//Eason: Pad draw rule ---
//Eason: MPdecisionCurrent +++
static int MPdecisionCurrent=0;
extern int get_current_for_ASUSswgauge(void);
//Eason: MPdecisionCurrent ---
//Hank:A86 slowly drop+++
extern int get_Curr_from_TIgauge(void);
extern int gBatteryTemp;
int g_current_now=0;
//Hank:A86 slowly drop---
//Eason: LowCapCpuThrottle +++
bool IsInCpuThrottle = false;
//Eason: LowCapCpuThrottle ---
//Eason: check FC flag to disable charge, prevent Battery over charge, let gauge FCC abnormal+++
extern void setSmb346PreventOverFCC(void);
extern bool get_FC_flage_from_TIgauge(void);
//Eason: check FC flag to disable charge, prevent Battery over charge, let gauge FCC abnormal---

//Eason A91 porting+++
extern int getIfonline(void);
extern void AXC_bq51220_Charger_Init(void);
//Eason A91 porting---

//Eason : Wireless PMA spec Rx turn off Tx +++
#ifdef CONFIG_IDTP9023_CHARGER
#define WC_PD_DET_GPIO 20
static time_t g_WcEocTime; 
extern void IDTP9023_RxTurnOffTx(void);
#endif
//Eason : Wireless PMA spec Rx turn off Tx ---

//Hank: when update gauge need to disable charge+++
extern bool DisChg;
void UpGaugeSetChg(bool enabled)
{
 	gpCharger->EnableCharging(gpCharger,enabled);
	DisChg = !enabled;
	printk("[BAT][Gau]UpGaugeSetChg:%d\n",enabled);
}
//Hank: when update gauge need to disable charge---

#ifdef CONFIG_PM_8226_CHARGER
void asus_fsm_chargingstop(AXE_Charging_Error_Reason reason)
{
	#ifdef ASUS_FACTORY_BUILD
	if(false == charger_limit_enable)
	#endif		
	{
		printk("[BAT]%s\n",__FUNCTION__);
	
		if(balance_this != NULL){
			if(balance_this->mbInit){
				balance_this->fsm->onChargingStop(balance_this->fsm, reason);
			}
		}
	}	
}

void asus_fsm_chargingstart(void)
{
	#ifdef ASUS_FACTORY_BUILD
	if(false == charger_limit_enable)
	#endif	
	{
		printk("[BAT]%s\n",__FUNCTION__);
		
		if(balance_this != NULL){
			if(balance_this->mbInit){
				balance_this->fsm->onChargingStart(balance_this->fsm);
			}
		}
	}	
}
#endif //CONFIG_PM_8226_CHARGER

//Eason : prevent thermal too hot, limit charging current in phone call+++
extern void setChgDrawCurrent(void);
static void judgePhoneOnCurLimit(void)
{
/*	
	if( (true==IsPhoneOn)&&(balance_this->A66_capacity>20) )
   	{ 
		g_audio_limit = true;
		printk("[BAT][Ser]:judge g_audio_limit true\n");
		   

		setChgDrawCurrent();

	}else{
		g_audio_limit = false;
		printk("[BAT][Ser]:judge g_audio_limit false\n");


		setChgDrawCurrent();

	}
*/	
}

//Hank enterRomMode_test++++
#ifdef CONFIG_TI_GAUGE
static int enterRomMode_proc_show(struct seq_file *seq, void *v)
{
	return seq_printf(seq, "EnterRom:%d, IsRom:%d\n", enterRomMode_test,check_is_RomMode());
}

static ssize_t enterRomMode_write_proc(struct file *filp, const char __user *buff, size_t len, loff_t *pos)
{
	int val;

	char messages[256];

	if (len > 256) {
		len = 256;
	}

	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	
	val = (int)simple_strtol(messages, NULL, 10);
 
	enterRomMode_test = val;  

	if(1==val)
	{
		TIgauge_UnlockStep1();
		TIgauge_UnlockStep2();
		TIgauge_EnterRomMode();
	}else if(0==val)
	{
		exitRomMode();
		//TIgauge_LockStep(); X: 4000 in funtion explain_dffs_string already do this (axc_TI_HWgauge_Update_A80.c)
	}
    
   	 printk("[BAT][enterRomMode_test]:%d,Rom:%d,Active:%d\n",val,check_is_RomMode(),check_is_ActiveMode());//check Rom mode first
	
	return len;
}

static void *enterRomMode_proc_start(struct seq_file *seq, loff_t *pos)
{
	static unsigned long counter = 0;
	
	if(*pos == 0){
		return &counter;
	}
	else{
		*pos = 0;
		return NULL;
	}
}

static void *enterRomMode_proc_next(struct seq_file *seq, void *v, loff_t *pos)
{
	return NULL;
}

static void enterRomMode_proc_stop(struct seq_file *seq, void *v)
{
	
}

static const struct seq_operations enterRomMode_proc_seq = {
	.start		= enterRomMode_proc_start,
	.show		= enterRomMode_proc_show,
	.next		= enterRomMode_proc_next,
	.stop		= enterRomMode_proc_stop,
};

static int enterRomMode_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &enterRomMode_proc_seq);
}

static const struct file_operations enterRomMode_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= enterRomMode_proc_open,
	.read		= seq_read,
	.write		= enterRomMode_write_proc,
};

void static create_enterRomMode_proc_file(void)
{
	struct proc_dir_entry *enterRomMode_proc_file = proc_create("driver/enterRomMode", 0666, NULL, &enterRomMode_proc_fops);

	if (!enterRomMode_proc_file) {
		printk("[BAT][Bal] enterRomMode create failed!\n");
    }

	return;
}
#endif //CONFIG_TI_GAUGE
//Hank enterRomMode_test----

//[ChiaYuan][+++]Add for Broadcast busy state
void HighPower_timer_expired(unsigned long data)
{	
	int AveCur = 0;

	if (HighPower_Count != 0) {
		AveCur = Total_BatCur/HighPower_Count;
	}
	printk(KERN_INFO "[Power]Timer expired. Count:%d, AveCur:%d\r\n", HighPower_Count, AveCur);
	if (AveCur > 100) {
		if (IsHighPower==0) {
			IsHighPower=1;
		}
		else if (IsHighPower==1) {
			IsHighPower=2;
		}
		else if (IsHighPower==2) {
			IsHighPower=1;
		}
		printk(KERN_INFO "[Power]Set IsHighPower:%d \r\n", IsHighPower);
	}
	//Reset all calculation variables
	Total_BatCur=0;
	HighPower_Count=0;
	mod_timer(&HighPower_timer, jiffies + msecs_to_jiffies(HIGH_POWER_TIMEOUT));
}

static int HighPower_proc_show(struct seq_file *seq, void *v)
{
	return seq_printf(seq, "%d\n", IsHighPower);
}

static ssize_t HighPower_write_proc(struct file *filp, const char __user *buff, size_t len, loff_t *pos)
{
	int val;

	char messages[256];

	if (len > 256) {
		len = 256;
	}

	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	
	val = (int)simple_strtol(messages, NULL, 10);
	
	IsHighPower = val;

	printk("[Power]Write HighPower:%d\n",IsHighPower);

	return len;
}

static void *HighPower_proc_start(struct seq_file *seq, loff_t *pos)
{
	static unsigned long counter = 0;
	
	if(*pos == 0){
		return &counter;
	}
	else{
		*pos = 0;
		return NULL;
	}
}

static void *HighPower_proc_next(struct seq_file *seq, void *v, loff_t *pos)
{
	return NULL;
}

static void HighPower_proc_stop(struct seq_file *seq, void *v)
{
	
}

static const struct seq_operations HighPower_proc_seq = {
	.start		= HighPower_proc_start,
	.show		= HighPower_proc_show,
	.next		= HighPower_proc_next,
	.stop		= HighPower_proc_stop,
};

static int HighPower_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &HighPower_proc_seq);
}

static const struct file_operations HighPower_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= HighPower_proc_open,
	.read		= seq_read,
	.write		= HighPower_write_proc,
};

void static create_HighPower_proc_file(void)
{
	struct proc_dir_entry *HighPower_proc_file = proc_create("driver/IsHighPower", 0644, NULL, &HighPower_proc_fops);
	if (!HighPower_proc_file) {
		printk("[Power] HighPower_proc_file create failed!\n");
	}
}
//[ChiaYuan][---]Add for Broadcast busy state

//Hank read BatteryID++++
#ifdef ASUS_FACTORY_BUILD
extern long long g_BatteryID_value;

static int BatteryID_proc_show(struct seq_file *seq, void *v)
{
	read_battery_id();
	return seq_printf(seq, "BatteryID: %lld\n", g_BatteryID_value);
}

static ssize_t BatteryID_write_proc(struct file *filp, const char __user *buff, size_t len, loff_t *pos)
{
	int val;

	char messages[256];

	if (len > 256) {
		len = 256;
	}

	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	
	val = (int)simple_strtol(messages, NULL, 10);

	printk("[BAT][SER][BatteryID]mode:%d\n",val);//check Rom mode first

	return len;
}



static void *BatteryID_proc_start(struct seq_file *seq, loff_t *pos)
{
	static unsigned long counter = 0;
	
	if(*pos == 0){
		return &counter;
	}
	else{
		*pos = 0;
		return NULL;
	}
}

static void *BatteryID_proc_next(struct seq_file *seq, void *v, loff_t *pos)
{
	return NULL;
}

static void BatteryID_proc_stop(struct seq_file *seq, void *v)
{
	
}

static const struct seq_operations BatteryID_proc_seq = {
	.start		= BatteryID_proc_start,
	.show		= BatteryID_proc_show,
	.next		= BatteryID_proc_next,
	.stop		= BatteryID_proc_stop,
};

static int BatteryID_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &BatteryID_proc_seq);
}

static const struct file_operations BatteryID_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= BatteryID_proc_open,
	.read		= seq_read,
	.write		= BatteryID_write_proc,
};

void static create_BatteryID_proc_file(void)
{
	struct proc_dir_entry *BatteryID_proc_file = proc_create("driver/BatteryID", 0644, NULL, &BatteryID_proc_fops);

	if (!BatteryID_proc_file) {
		printk("[BAT][SER] BatteryID create failed!\n");
    }

	return;
}
#endif
//Hank read BatteryID----

void SetLimitCurrentInPhoneCall(bool phoneOn)
{  
   if(phoneOn)
   {
        IsPhoneOn = true;
        printk("[BAT][Ser]:Phone call on\n");
   }else{
    	IsPhoneOn = false;
    	g_audio_limit = false;
    	printk("[BAT][Ser]:Phone call off\n");
   }


       	judgePhoneOnCurLimit();

}
//Eason : prevent thermal too hot, limit charging current in phone call---
//Eason : Wireless PMA spec Rx turn off Tx +++
/*
*  wireless charger PMA spec
*  When coil temp(mapping bat temp) over 50.0 degreeC, force Rx turn off Tx
*/
#ifdef CONFIG_IDTP9023_CHARGER
static time_t getWcEocIntervalSinceLastUpdate(void)
{
	struct timespec mtNow;
    
	time_t intervalSinceLastUpdate;
    
	mtNow = current_kernel_time();

	if(mtNow.tv_sec >= g_WcEocTime)
	{
		printk("[BAT][Ser]%s:%ld,%ld\n",__FUNCTION__,mtNow.tv_sec,g_WcEocTime);
            
		intervalSinceLastUpdate = mtNow.tv_sec - g_WcEocTime;


		//cause system time didn't work at first time update capacity (8secs) 
		//filter intervalSinceLastUpdate more than one month
		if(intervalSinceLastUpdate > 2592000)
		{
			printk("[BAT][SER]%s():wrongInt %ld \n",__func__,intervalSinceLastUpdate);
			intervalSinceLastUpdate = 180;
		}        
	}
	else
	{  
		printk("[BAT][Ser]%s:OVERFLOW....%ld,%ld\n",__FUNCTION__,mtNow.tv_sec,g_WcEocTime);              
		//todo: to do the correct calculation here....
		intervalSinceLastUpdate = 180;
	}


    return intervalSinceLastUpdate ; 
}
#define WC_EOC_TURNOFF_TX_TIMER 1800
static inline time_t  updateNowTime(struct AXC_BatteryService *_this);
static void judgeWirelessEocTurnOffTx(void)
{
	time_t WcEocInterval;
	WcEocInterval  = getWcEocIntervalSinceLastUpdate();

	if( (100==gBMS_Cap) && (WcEocInterval >= WC_EOC_TURNOFF_TX_TIMER) && (1 == gpio_get_value(WC_PD_DET_GPIO)) )
	{
		printk("[BAT][WC][PMA][SPEC]WIRELESS EOC KEEP 30MIN, RX TURN TX:%ld\n",WcEocInterval);
		IDTP9023_RxTurnOffTx();
	}
	
	g_WcEocTime = updateNowTime(balance_this);
	
}

#define WC_COIL_HOT_TEMP 550
static void judgeWirelessCoilTempTurnOffTx(void)
{
	int batt_temp = pm8226_get_prop_batt_temp();
	if( batt_temp >= WC_COIL_HOT_TEMP )
	{
		printk("[BAT][WC][PMA]ERROR: COIL TEMP OVER 50 degc, RX TURN TX:%d\n",batt_temp);
		IDTP9023_RxTurnOffTx();
	}

}
#endif //CONFIG_IDTP9023_CHARGER
//Eason : Wireless PMA spec Rx turn off Tx ---

//Eason : when thermal too hot, limit charging current +++
static void judgeThermalCurrentLimit(void)
{
    if( true==IsThermalHot)
    {
	        if(balance_this->A66_capacity>=15)
		 {
	            g_thermal_limit = 3;
	        }
		 else if(balance_this->A66_capacity>=8)
		 {
	             g_thermal_limit = 2;
	        }
		 else
		 {
	             g_thermal_limit = 1;
	        }
	        printk("[BAT][SER][Thermal]:judge g_thermal_limit true\n");    
    }
    else
    {
	        g_thermal_limit = 0;
	        printk("[BAT][SER][Thermal]:judge g_thermal_limit false\n");

    }
    if(1==getIfonline())
    	setChgDrawCurrent();
        
    
}

void notifyThermalLimit(int thermalnotify)
{

	if(0==thermalnotify){
			IsThermalHot = false;
			g_thermal_limit = false;
			printk("[BAT][SER][Thermal]:Thermal normal notify\n");
	}else{
			IsThermalHot = true;
			printk("[BAT][SER][Thermal]:Thermal hot notify!!\n");
	}

    judgeThermalCurrentLimit();
    
}
//Eason : when thermal too hot, limit charging current ---

//Eason: LowCapCpuThrottle +++
#define CPU_THROTTLE_START_BATCAP	14
#define CPU_THROTTLE_STOP_BATCAP		15	
#define CPU_THROTTLE_START_CURR		2000

extern void notifyMpdecisionCpuThrottle(bool IsCpuThrottle);
static void judgeCpuThrottleByCap(void)
{	
	if( (balance_this->A66_capacity <= CPU_THROTTLE_START_BATCAP)
		&&(g_current_now >= CPU_THROTTLE_START_CURR)&&(false == IsInCpuThrottle) )
	{
			notifyMpdecisionCpuThrottle(true);
			IsInCpuThrottle = true;
			printk("[BAT][SER][Throttle]throttle:%d,curr:%d\n",IsInCpuThrottle,g_current_now);
	}
	else if( (balance_this->A66_capacity >= CPU_THROTTLE_STOP_BATCAP)&&(true == IsInCpuThrottle) )
	{
			notifyMpdecisionCpuThrottle(false);
			IsInCpuThrottle = false;
			printk("[BAT][SER][Throttle]throttle:%d,curr:%d\n",IsInCpuThrottle,g_current_now);
	}
}
//Eason: LowCapCpuThrottle ---

#ifdef CONFIG_EEPROM_NUVOTON
//Eason:  Pad draw rule compare thermal +++
static bool DecideIfPadDockHaveExtChgAC(void);
PadDrawLimitCurrent_Type JudgePadRuleDrawLimitCurrent(bool isSuspendCharge)
{
#ifdef CONFIG_EEPROM_NUVOTON
	if( 1==AX_MicroP_IsP01Connected() )
	{
		if((balance_this->A66_capacity <= 8) ||(2==IsBalanceMode) || isSuspendCharge)//ForcePowerBankMode draw 900
		{
				return PadDraw900;	
		}else if( (true == DecideIfPadDockHaveExtChgAC())&&(1==IsBalanceMode) )//only do this rule in balanceMode
		{
				if(balance_this->A66_capacity <= 15)
				{
						return PadDraw700;
				}else{
						if( (balance_this->A66_capacity-balance_this->Pad_capacity)>=20 )
									return PadDraw300;
						else if( (balance_this->A66_capacity-balance_this->Pad_capacity)>=10 )
									return PadDraw500;
						else
									return PadDraw700;
				}

		}else{
				return PadDraw700;
		}
	}else{
		return PadDraw700;
	}
#else
	return PadDraw700;
#endif
}
//Eason:  Pad draw rule compare thermal ---

//ASUS_BSP Eason when audio on, draw 500mA from Pad ++++
extern void setChgDrawPadCurrent(bool audioOn);
void SetPadCurrentDependOnAudio(bool audioOn)
{
/*
   if( 1==AX_MicroP_IsP01Connected() )
   {
   	setChgDrawPadCurrent(audioOn);
   }
*/   
}
//ASUS_BSP Eason when audio on, draw 500mA from Pad ---

//ASUS BSP Eason add A68 charge mode +++
static int decidePowerBankChgModeStopPercent(void)
{
	int StopPercent = 90;

	StopPercent = balance_this->A66_capacity + 2*balance_this->Pad_capacity;

	if(StopPercent >= 90)
	{
		StopPercent = 90;
	}

        printk("[BAT][smb346]PowerBank Stop Percent: %d \n",StopPercent);	
	return StopPercent;
}

static int decideBalanceChgModeStopPercent(void)
{
	int StopPercent = 90;

	StopPercent =  (balance_this->A66_capacity + 2*balance_this->Pad_capacity)/2;

	if(StopPercent >= 90)
	{
		StopPercent = 90;
	}else if(StopPercent <= 15){
		StopPercent = min( (balance_this->A66_capacity + 2*balance_this->Pad_capacity), 15 );
      }

        printk("[BAT][smb346]Balance Stop Percent: %d \n",StopPercent);	
	return StopPercent;
}


static void do_PadBalanceMode_inChgMode(void)
{
	int NeedStopPercent = 90;

	if(1 == IsBalanceMode)// Balance Mode
	{
		NeedStopPercent = decideBalanceChgModeStopPercent();
		setFloatVoltage(NeedStopPercent);
	}else{//PowerBank Mode
		NeedStopPercent = decidePowerBankChgModeStopPercent();
		setFloatVoltage(NeedStopPercent);
	}
}

void decideIfDo_PadBalanceModeInChgMode(void)
{
	int PadChgCable = 1;

	if(1==AX_MicroP_IsP01Connected())
	{	
		PadChgCable = AX_MicroP_get_USBDetectStatus(Batt_P01);
	
		if(1 == PadChgCable)
		{
			printk("[BAT][smb346]with extChg dont do PadChgMode\n");
		}else{
			printk("[BAT][smb346]without extChg need do PadChgMode\n");
			do_PadBalanceMode_inChgMode();			
		}
	}
}
#endif //CONFIG_EEPROM_NUVOTON
//ASUS BSP Eason add A68 charge mode ---

//ASUS BSP Eason_Chang +++ batteryservice to fsm
static void AXC_BatteryService_reportPropertyCapacity(struct AXC_BatteryService *_this, int refcapacity);

#ifdef CONFIG_EEPROM_NUVOTON
int ReportBatteryServiceP02Cap(void)
{
    return balance_this->Pad_capacity;
}
#endif

static void BatteryService_enable_ChargingFsm(AXC_BatteryService *_this)
{
    if(NULL == _this->fsm){

        _this->fsm = getChargingFSM(E_ASUS_A66_FSM_CHARGING_TYPE,&_this->fsmCallback);

        _this->fsmState = _this->fsm->getState(_this->fsm);
    }
}  
//ASUS BSP Eason_Chang --- batteryservice to fsm
//ASUS BSP Eason_Chang +++ batteryservice to gauge
static void BatteryService_enable_Gauge(AXC_BatteryService *_this)
{
    if(NULL == _this->gauge){

        AXC_GaugeFactory_GetGaugeV2(E_SW_GAUGE_V2_TYPE , &_this->gauge, &_this->gaugeCallback);
    }

#ifdef CONFIG_EEPROM_NUVOTON
    if(NULL == _this->P02gauge){

        AXC_GaugeFactory_GetGaugeV2(E_HW_GAUGE_PAD_TYPE , &_this->P02gauge, &_this->P02gaugeCallback);
    }
#endif
}
//ASUS BSP Eason_Chang --- batteryservice to gauge

static void BatteryService_enable_Filter(AXC_BatteryService *_this)
{
	if(NULL == _this->gpCapFilterA66){
		AXC_Cap_Filter_Get(E_CAP_FILTER_PHONE_A66, &_this->gpCapFilterA66, 369); //FCC
	}

#ifdef CONFIG_EEPROM_NUVOTON
	if(NULL == _this->gpCapFilterP02){
		AXC_Cap_Filter_Get(E_CAP_FILTER_PAD_P02, &_this->gpCapFilterP02, 4300);
	}
#endif
}

//ASUS_BSP  +++ Eason_Chang charger
static void NotifyForChargerStateChanged(struct AXI_Charger *apCharger, AXE_Charger_Type aeCharger_Mode)
{
#ifdef CONFIG_BATTERY_ASUS_SERVICE
	if(NULL == balance_this){
		return;
	}

	balance_this->miParent.onCableInOut(&balance_this->miParent,aeCharger_Mode);
	balance_this->isMainBatteryChargingDone = false;     
#endif
}
static void onChargingStart(struct AXI_Charger *apCharger, bool startCharging)
{


}
//ASUS_BSP  --- Eason_Chang charger

#ifndef ASUS_FACTORY_BUILD
#ifdef CONFIG_EEPROM_NUVOTON  //ASUS_BSP Eason_Chang 1120 porting +++
static int get_5VPWR_EN(void)
{
    return AX_MicroP_getGPIOOutputPinLevel(OUT_uP_5V_PWR_EN);
}
#endif //CONFIG_EEPROM_NUVOTON//ASUS_BSP Eason_Chang 1120 porting ---
#endif//#ifndef ASUS_FACTORY_BUILD
//Eason: A68 new balance mode ---


//ASUS_BSP +++ Eason_Chang BalanceMode
#ifdef CONFIG_EEPROM_NUVOTON
static void set_microp_vbus(int level)
{
    int rt;
 	   rt = AX_MicroP_setGPIOOutputPin(OUT_uP_VBUS_EN, level);
    if (rt<0){
           printk("[BAT][Bal]microp set error\n");
    }else if(rt == 0){
           printk("[BAT][Bal]microp set success\n");
    } 
}

static int get_microp_vbus(void)
{
    return AX_MicroP_getGPIOOutputPinLevel(OUT_uP_VBUS_EN);
}
#endif //#ifdef CONFIG_EEPROM_NUVOTON

#ifdef ASUS_FACTORY_BUILD
//Eason: take off 5060rule let A80 can always charge+++
extern enum DEVICE_HWID g_A68_hwID;
//Eason: take off 5060rule let A80 can always charge---
static void Do_Factory5060Mode(void)
{
   
   printk("[BAT][Factory]:DoFactory5060Mode+++\n");
   /*
   if(1==AX_MicroP_IsP01Connected())
   {
   	set_microp_vbus(1);
   }
   gpCharger->EnableCharging(gpCharger,true);
   balance_this->fsm->onChargingStart(balance_this->fsm);
   */
   
	if(balance_this->A66_capacity >= 80){
		g_5060modeCharging = false;
		gpCharger->EnableCharging(gpCharger,false);
		DisChg = true;
		balance_this->fsm->onChargingStop(balance_this->fsm,POWERBANK_STOP);

#ifdef CONFIG_EEPROM_NUVOTON
		printk("[BAT][Factory]mode:%d,StopChg,Vbus:%d\n",IsBalanceMode,get_microp_vbus());
#endif
	}else if(balance_this->A66_capacity <= 60){   
		g_5060modeCharging = true;
		gpCharger->EnableCharging(gpCharger,true);
		DisChg = false;
		balance_this->fsm->onChargingStart(balance_this->fsm);
#ifdef CONFIG_EEPROM_NUVOTON
		printk("[BAT][Factory]mode:%d,StartChg,Vbus:%d\n"
												,IsBalanceMode,get_microp_vbus());
	}else{
		printk("[BAT][Factory]mode:%d,sameChg,Vbus:%d\n"
												,IsBalanceMode,get_microp_vbus());
#endif	
	}
	printk("[BAT][Factory]:DoFactory5060Mode---\n");
   
}
#endif//#ifdef ASUS_FACTORY_BUILD

#ifdef CONFIG_EEPROM_NUVOTON
void Init_Microp_Vbus__Chg(void)
{
#ifndef ASUS_FACTORY_BUILD
	set_microp_vbus(1);
	gpCharger->EnableCharging(gpCharger,true);
	DisChg = false;
	IsBalanceCharge = 1;
	IsPowerBankCharge = 1;
	balance_this->fsm->onChargingStart(balance_this->fsm);
	printk("[BAT][Bal]InitVbus:%d,InitChg:%d\n",get_microp_vbus(),gpCharger->IsCharging(gpCharger));
#else 
	if( true == charger_limit_enable )
		Do_Factory5060Mode();
#endif//#ifndef ASUS_FACTORY_BUILD
}    

void  openMicropVbusBeforeShutDown(void){  
	set_microp_vbus(1);
}    
 
#ifndef ASUS_FACTORY_BUILD
static void Do_PowerBankMode(void)
{
	printk("[BAT][Bal]:DoPowerBank+++\n");
   //set_microp_vbus(1);
   
	if(balance_this->A66_capacity >= 90){
		//set_microp_vbus(0);
		gpCharger->EnableCharging(gpCharger,false);
		DisChg = true;
		balance_this->fsm->onChargingStop(balance_this->fsm,POWERBANK_STOP);

		IsPowerBankCharge = 0;
		printk("[BAT][Bal]mode:%d,StopChg,Vbus:%d\n"
                                        ,IsBalanceMode,get_microp_vbus());
	}else if(balance_this->A66_capacity <= 70){   
		//set_microp_vbus(1);
		gpCharger->EnableCharging(gpCharger,true);
		DisChg = false;
		balance_this->fsm->onChargingStart(balance_this->fsm);

		IsPowerBankCharge = 1;
		printk("[BAT][Bal]mode:%d,StartChg,Vbus:%d\n"
                                        ,IsBalanceMode,get_microp_vbus());
	}else{
         printk("[BAT][Bal]mode:%d,sameChg,Vbus:%d\n"
                                        ,IsBalanceMode,get_microp_vbus());
	}  
   printk("[BAT][Bal]:DoPowerBank---\n");
}
#endif//#ifndef ASUS_FACTORY_BUILD

//Eason: A68 new balance mode +++	
static bool DecideIfPadDockHaveExtChgAC(void);
//Eason: A68 new balance mode ---

//Eason: dynamic set Pad alarm +++
#ifndef ASUS_FACTORY_BUILD
static void judgeIfneedDoBalanceModeWhenSuspend(void)
{

	if( true==IsKeepChgFrom15pTo19p )
	{
		 IsBalanceSuspendStartcharge = true;	
	}else if( (balance_this->A66_capacity>=85)||(balance_this->A66_capacity*10-balance_this->Pad_capacity*12 >=0) )
	{
		 IsBalanceSuspendStartcharge = false;

	}else if((balance_this->A66_capacity<=70)&&(balance_this->A66_capacity*10-balance_this->Pad_capacity*9 <=0) ){

 		 IsBalanceSuspendStartcharge = true;		 
	}
}
#endif
void SetRTCAlarm(void);
//Eason: dynamic set Pad alarm ---

static void BatteryServiceDoBalance(struct AXC_BatteryService *_this)
{
#ifndef ASUS_FACTORY_BUILD
   int StartRatio;
   int StopRatio;

   printk("[BAT][Bal]:DoBalance +++\n");
   //gpCharger->EnableCharging(gpCharger,true);
   StartRatio = GetBalanceModeStartRatio();
   StopRatio = GetBalanceModeStopRatio();

   printk("[BAT][Bal]%d,%d,%d,%d,%d\n",
                      IsBalanceMode,StartRatio,StopRatio,
                      _this->A66_capacity,_this->Pad_capacity);

   if(1 == IsBalanceMode){

         LastTimeIsBalMode = 1;

	//Eason: A68 new balance mode +++	


		//Eason:balance mode keep charge from Cap 15 to 19+++
		if( (_this->A66_capacity>=20)||(false==IsKeepChgFrom15pTo19p) )//16~19 first dobalance will default set_microp_vbus(0) by false==IsKeepChgFrom15pTo19p
		{
			//when forceresume default turn off vbus +++
			if ((false==IsSystemdraw)&&(false == DecideIfPadDockHaveExtChgAC()))//can't take off this, cause if plug extChg and interval calculate Cap can't turn off vbus  
			{
					set_microp_vbus(0);//do this cause in suspend will turn on vbus to charge
					printk("[BAT][Bal]turn off vbus default\n");
			}			
			//when forceresume default turn off vbus ---	
			//judge if draw current to system but does not charge battery +++
			if((_this->A66_capacity>=90)||(_this->A66_capacity-_this->Pad_capacity*StopRatio>=0))
			{
					set_microp_vbus(0);
					//gpCharger->EnableCharging(gpCharger,false); //Hank: remove disable charging when cable off
					DisChg = true;
					_this->fsm->onChargingStop(_this->fsm,BALANCE_STOP);   
	             
					IsBalanceCharge = 0;
					IsSystemdraw = false;
					printk("[BAT][Bal]mode:%d,N_Vbus N_Chg,Vbus:%d,SysD:%d\n"
									,IsBalanceMode,get_microp_vbus(),IsSystemdraw);
					ASUSEvtlog("[BAT][Bal]draw system[stop]\n");
	          
			}else if((_this->A66_capacity*10 - _this->Pad_capacity*StartRatio <= 0)
					&&(_this->A66_capacity <= 70 ))
			{
					set_microp_vbus(1);
					gpCharger->EnableCharging(gpCharger,false);
					DisChg = true;
					_this->fsm->onChargingStop(_this->fsm,BALANCE_STOP);
             
					IsBalanceCharge = 0;
					IsSystemdraw = true;
					printk("[BAT][Bal]mode:%d,Y_Vbus N_Chg,Vbus:%d,SysD:%d\n"
									,IsBalanceMode,get_microp_vbus(),IsSystemdraw);
					ASUSEvtlog("[BAT][Bal]draw system[Start]\n");
			}
			//judge if draw current to system, but does not charge battery ---
		}
		//Eason:balance mode keep charge from Cap 15 to 19---
		
		//judge if charge to battery +++
		if(_this->A66_capacity>=20)
		{
				gpCharger->EnableCharging(gpCharger,false);
				DisChg = true;
				_this->fsm->onChargingStop(_this->fsm,BALANCE_STOP);

				IsBalanceCharge = 0;
				IsKeepChgFrom15pTo19p = false;//Eason:balance mode keep charge from Cap 15 to 19
				
				printk("[BAT][Bal]mode:%d,F_Vbus N_Chg,Vbus:%d\n"
								,IsBalanceMode,get_microp_vbus());
				ASUSEvtlog("[BAT][Bal]active charge[stop]\n");
				
		}else if(_this->A66_capacity<=15)
		{
				set_microp_vbus(1);
				gpCharger->EnableCharging(gpCharger,true);
				DisChg = false;
				_this->fsm->onChargingStart(_this->fsm);

				IsBalanceCharge = 1;
				IsKeepChgFrom15pTo19p = true;//Eason:balance mode keep charge from Cap 15 to 19
				
				printk("[BAT][Bal]mode:%d,Y_Vbus Y_Chg,Vbus:%d\n"
								,IsBalanceMode,get_microp_vbus());
				ASUSEvtlog("[BAT][Bal]active charge[Start]\n");
		}
		//judge if charge to battery ---
		//Eason: dynamic set Pad alarm +++
		judgeIfneedDoBalanceModeWhenSuspend();
		//Eason: dynamic set Pad alarm ---
	//Eason: A68 new balance mode ---

         
   //}else if(0==IsBalanceMode && 1==LastTimeIsBalMode && 0==IsBalanceCharge){
   }else if(0==IsBalanceMode){
         
         LastTimeIsBalMode = 0;

         Do_PowerBankMode();
         
         
   }
   //Eason: do ForcePowerBankMode+++
   else if(2==IsBalanceMode){
   	
         LastTimeIsBalMode = 0;

         Do_PowerBankMode();
   }	
   //Eason: do ForcePowerBankMode---
	
   pr_debug("[BAT][Bal]LastBal:%d,IsBalChg:%d,IsBankChg:%d\n"
                            ,LastTimeIsBalMode,IsBalanceCharge,IsPowerBankCharge);
   
   printk("[BAT][Bal]:DoBalance ---\n");
#endif//#ifndef ASUS_FACTORY_BUILD
}

static bool DecideIfPadDockHaveExtChgAC(void)
{
    bool IsPadDockExtChgAC = false;
    int PadChgCable = 0;
    bool DockChgCable = false;
    int IsDockIn = 0;
    
    PadChgCable = AX_MicroP_get_USBDetectStatus(Batt_P01);
    
	if(1==PadChgCable){
		IsPadDockExtChgAC = true; 
	}

	pr_debug("[BAT][Ser]:DockI:%d,PadAC:%d,DockAC:%d,ExtChg:%d\n",IsDockIn,PadChgCable,DockChgCable,IsPadDockExtChgAC);
 
	return IsPadDockExtChgAC;
}  

//Eason: do ForcePowerBankMode+++
void DoForcePowerBankMode(void)
{
	unsigned short off=0xAA;

	uP_nuvoton_write_reg(MICROP_SOFTWARE_OFF,  &off);
	printk("[BAT][Bal]:ForcePowerBankMode\n");
}
//Eason: do ForcePowerBankMode---
#if 0
static ssize_t balanceChg_read_proc(char *page, char **start, off_t off, int count, 
            	int *eof, void *data)
{
	return sprintf(page, "%d\n", IsBalanceMode);
}
static ssize_t balanceChg_write_proc(struct file *filp, const char __user *buff, 
	            unsigned long len, void *data)
{
	int val;

	char messages[256];

	if (len > 256) {
		len = 256;
	}

	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	
	val = (int)simple_strtol(messages, NULL, 10);


	IsBalanceMode = val;

//when takeoff extChg default turn off vbus +++
#ifndef ASUS_FACTORY_BUILD
	  IsSystemdraw=false;
	  IsBalanceSuspendStartcharge = false;
	  IsKeepChgFrom15pTo19p = false;//Eason:balance mode keep charge from Cap 15 to 19s
#endif
//when takeoff extChg default turn off vbus ---
  
    if(1==AX_MicroP_IsP01Connected()){

		if( false == DecideIfPadDockHaveExtChgAC()){ 
				Init_Microp_Vbus__Chg();
				BatteryServiceDoBalance(balance_this);
		}else{
				Init_Microp_Vbus__Chg();
		}
		//Eason: do ForcePowerBankMode+++
		if(2==IsBalanceMode){
				DoForcePowerBankMode();
		}
		//Eason: do ForcePowerBankMode---
    }
    
    printk("[BAT][Bal]mode:%d\n",val);
	
	return len;
}
#endif
static const struct file_operations balanceChg_proc_fops = {
	.owner		= THIS_MODULE,
//	.read		= balanceChg_read_proc,
//	.write		= balanceChg_write_proc,
};

void static create_balanceChg_proc_file(void)
{
	struct proc_dir_entry *balanceChg_proc_file = proc_create("driver/balanceChg", 0644, NULL, &balanceChg_proc_fops);

	if (!balanceChg_proc_file) {
		printk("[BAT][Bal]proc file create failed!\n");
    }

	return;
}
#endif //CONFIG_EEPROM_NUVOTON

//Eason: MPdecisionCurrent +++
static int MPdecisionCurrent_proc_show(struct seq_file *seq, void *v)
{
	MPdecisionCurrent = get_current_for_ASUSswgauge();
	return seq_printf(seq, "%d\n", MPdecisionCurrent);
}
static ssize_t MPdecisionCurrent_write_proc(struct file *filp, const char __user *buff, size_t len, loff_t *pos)
{
	int val;

	char messages[256];

	if (len > 256) {
		len = 256;
	}

	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}

	val = (int)simple_strtol(messages, NULL, 10);

	MPdecisionCurrent = val;

	printk("[BAT][Bal]mode:%d\n",val);

	return len;
}

static void *MPdecisionCurrent_proc_start(struct seq_file *seq, loff_t *pos)
{
	static unsigned long counter = 0;
	
	if(*pos == 0){
		return &counter;
	}
	else{
		*pos = 0;
		return NULL;
	}
}

static void *MPdecisionCurrent_proc_next(struct seq_file *seq, void *v, loff_t *pos)
{
	return NULL;
}

static void MPdecisionCurrent_proc_stop(struct seq_file *seq, void *v)
{
	
}

static const struct seq_operations MPdecisionCurrent_proc_seq = {
	.start		= MPdecisionCurrent_proc_start,
	.show		= MPdecisionCurrent_proc_show,
	.next		= MPdecisionCurrent_proc_next,
	.stop		= MPdecisionCurrent_proc_stop,
};

static int MPdecisionCurrent_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &MPdecisionCurrent_proc_seq);
}

static const struct file_operations MPdecisionCurrent_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= MPdecisionCurrent_proc_open,
	.read		= seq_read,
	.write		= MPdecisionCurrent_write_proc,
};

void static create_MPdecisionCurrent_proc_file(void)
{
	struct proc_dir_entry *MPdecisionCurrent_proc_file = proc_create("driver/MPdecisionCurrent", 0644, NULL, &MPdecisionCurrent_proc_fops);

	if (!MPdecisionCurrent_proc_file) {
		printk("[BAT]MPdecisionCurrent proc file create failed!\n");
	}

	return;
}
//Eason: MPdecisionCurrent ---

//Eason: charger_limit_enable is used to decide if Do_Factory5060Mode+++
#ifdef ASUS_FACTORY_BUILD
static struct proc_dir_entry *charger_limit_enable_proc_file = NULL;

static int charger_limit_enable_proc_show(struct seq_file *seq, void *v)
{
	return seq_printf(seq, "%d\n", charger_limit_enable);
}

static ssize_t charger_limit_enable_write_proc(struct file *filp, const char __user *buff, size_t len, loff_t *pos)
{
	char messages[256];

	if (len > 256) {
		len = 256;
	}

	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	
	if(messages[0]== '0')
	{
		//Eason:when disable 5060_limit, enable chargering 
		if(1==getIfonline() )
		{
			gpCharger->EnableCharging(gpCharger,true);
			DisChg = false;
			balance_this->fsm->onChargingStart(balance_this->fsm);
		}
		//Eason:when disable 5060_limit, enable chargering 
		
		printk("[BAT] set charger_limit_enable = false !\n");
		g_5060modeCharging = true;
		charger_limit_enable = false;
	}
	else if(messages[0] == '1')
	{
		printk("[BAT] set charger_limit_enable = true !\n");
		charger_limit_enable = true;
	}
	else
		printk("[BAT] charger_limit_enable write error %c \n",messages[0]);
	
	//Eason: Calculate Cap immediately after do 5060 mode+++
	cancel_delayed_work_sync(&balance_this->BatteryServiceUpdateWorker);
	//Hank: cancel BatteryServiceUpdateWorker need calculate capacity+++
	balance_this->NeedCalCap = true;
	//Hank: cancel BatteryServiceUpdateWorker need calculate capacity---
	queue_delayed_work(balance_this->BatteryServiceCapUpdateQueue, \
								&balance_this->BatteryServiceUpdateWorker,\
								0 * HZ);
	//Eason: Calculate Cap immediately after do 5060 mode---

	return len;		
}

static void *charger_limit_enable_proc_start(struct seq_file *seq, loff_t *pos)
{
	static unsigned long counter = 0;
	
	if(*pos == 0){
		return &counter;
	}
	else{
		*pos = 0;
		return NULL;
	}
}

static void *charger_limit_enable_proc_next(struct seq_file *seq, void *v, loff_t *pos)
{
	return NULL;
}

static void charger_limit_enable_proc_stop(struct seq_file *seq, void *v)
{
	
}

static const struct seq_operations charger_limit_enable_proc_seq = {
	.start		= charger_limit_enable_proc_start,
	.show		= charger_limit_enable_proc_show,
	.next		= charger_limit_enable_proc_next,
	.stop		= charger_limit_enable_proc_stop,
};

static int charger_limit_enable_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &charger_limit_enable_proc_seq);
}

static const struct file_operations charger_limit_enable_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= charger_limit_enable_proc_open,
	.read		= seq_read,
	.write		= charger_limit_enable_write_proc,
};

static void charger_limit_enable_create_proc_file(void)
{	
	charger_limit_enable_proc_file = proc_create("driver/charger_limit_enable", 0777, NULL, &charger_limit_enable_proc_fops);

	if (NULL == charger_limit_enable_proc_file) {
		printk(KERN_ERR "[BAT] charger_limit_enable_proc_file created failed!\n");
		return;
	}

	return;
}
#endif //ASUS_FACTORY_BUILD
//Eason: charger_limit_enable is used to decide if Do_Factory5060Mode---

static inline time_t  updateNowTime(struct AXC_BatteryService *_this)
{
    struct timespec mtNow;
    
    mtNow = current_kernel_time();    

    return mtNow.tv_sec;
}

//ASUS_BSP  +++ Eason_Chang "add BAT info time"
#if 0 
static void ReportTime(void)
{
	struct timespec ts;
	struct rtc_time tm;
	getnstimeofday(&ts);
	rtc_time_to_tm(ts.tv_sec, &tm);

	pr_debug ("[BAT][Ser] %d-%02d-%02d %02d:%02d:%02d\n"
		,tm.tm_year + 1900
		,tm.tm_mon + 1
		,tm.tm_mday
		,tm.tm_hour
		,tm.tm_min
		,tm.tm_sec);
}
#endif
//ASUS_BSP  --- Eason_Chang "add BAT info time"

#ifdef CONFIG_EEPROM_NUVOTON
void Init_BalanceMode_Flag(void)
{

         Init_Microp_Vbus__Chg();
		 
//Eason: A68 new balance mode +++			 
#ifndef ASUS_FACTORY_BUILD
	 IsBalanceSuspendStartcharge = false;//when plugIn Pad default false, or in doInBalanceModeWhenSuspend will keep last plugIn time status 
	 IsSystemdraw = false;//when plugIn Pad default false,
	 IsKeepChgFrom15pTo19p = false;//Eason:balance mode keep charge from Cap 15 to 19
#endif
//Eason: A68 new balance mode ---

         LastTimeIsBalMode = 1;
         IsBalanceCharge = 1;
         IsPowerBankCharge =1;
         balance_this->P02_savedTime = updateNowTime(balance_this);
}
//ASUS_BSP --- Eason_Chang BalanceMode
#endif

//ASUS_BSP +++ Eason_Chang BalanceMode
#ifdef CONFIG_EEPROM_NUVOTON
static int batSer_microp_event_handler(
	struct notifier_block *this,
	unsigned long event,
	void *ptr)
{
    unsigned long flags;
	pr_debug( "[BAT][Bal] %s() +++, evt:%lu \n", __FUNCTION__, event);

	switch (event) {
	case P01_ADD:
		printk( "[BAT][Bal]P01_ADD \r\n");
        asus_chg_set_chg_mode(ASUS_CHG_SRC_PAD_BAT);
        balance_this->P02_IsFirstAskCap = true;
        Init_BalanceMode_Flag();
        
        cancel_delayed_work_sync(&balance_this->BatteryServiceUpdateWorker);
	//Hank: cancel BatteryServiceUpdateWorker need calculate capacity+++
        balance_this->NeedCalCap = true;
        //Hank: cancel BatteryServiceUpdateWorker need calculate capacity---
        queue_delayed_work(balance_this->BatteryServiceCapUpdateQueue, \
                               &balance_this->BatteryServiceUpdateWorker,\
                               0 * HZ);

	//Eason: dynamic set Pad alarm +++
#ifdef ASUS_FACTORY_BUILD	
        schedule_delayed_work(&balance_this->SetRTCWorker, 1*HZ);
#endif
	//Eason: dynamic set Pad alarm ---
	 
        //Init_BalanceMode_Flag();
        //BatteryServiceDoBalance(balance_this);
		break;	
	case P01_REMOVE: // means P01 removed
        g_padMic_On = false;//Eason:thermal limit charging current,cause setChgDrawPadCurrent only do inPad
		printk( "[BAT][Bal]P01_REMOVE \r\n");
        
        spin_lock_irqsave(&bat_alarm_slock, flags);
        alarm_try_to_cancel(&bat_alarm);
        spin_unlock_irqrestore(&bat_alarm_slock, flags);

        asus_chg_set_chg_mode(ASUS_CHG_SRC_PAD_NONE);
        asus_bat_update_PadAcOnline();//Eason: In charger mode need update current status
		break;
    case P01_AC_USB_IN:
        //msleep(800);//Eason ,need time delay to get PAD AC/USB
        asus_bat_update_PadAcOnline();    
        printk( "[BAT][Bal]P01_AC_USB_IN\r\n");

        if(true==DecideIfPadDockHaveExtChgAC()){
                Init_Microp_Vbus__Chg();
        }
		break;
    case P01_AC_USB_OUT:
        asus_bat_update_PadAcOnline();    
        printk( "[BAT][Bal]P01_AC_USB_OUT \r\n");
        schedule_delayed_work(&balance_this->CableOffWorker,1*HZ);//keep 100% 5 min
  //when takeoff extChg default turn off vbus +++
#ifndef ASUS_FACTORY_BUILD
	  IsSystemdraw=false;
	 IsBalanceSuspendStartcharge = false;
	 IsKeepChgFrom15pTo19p = false;//Eason:balance mode keep charge from Cap 15 to 19
#endif
  //when takeoff extChg default turn off vbus ---
        BatteryServiceDoBalance(balance_this);
		break;
	//Eason after Pad update firmware, update status +++
	case PAD_UPDATE_FINISH:
		printk( "[BAT][Bal]PAD_UPDATE_FINISH+++\n");
		schedule_delayed_work(&balance_this->UpdatePadWorker, 0*HZ);

		Init_Microp_Vbus__Chg();

		if(delayed_work_pending(&balance_this->BatteryServiceUpdateWorker))
		{
			cancel_delayed_work_sync(&balance_this->BatteryServiceUpdateWorker); 
		}
		//Hank: cancel BatteryServiceUpdateWorker need calculate capacity+++
		balance_this->NeedCalCap = true;
		//Hank: cancel BatteryServiceUpdateWorker need calculate capacity---  
		pr_debug("[BAT][SER][Pad]%s(PAD_UPDATE_FINISH) queue BatteryServiceUpdateWorker with calculate capacity\n",__func__);     
		queue_delayed_work(balance_this->BatteryServiceCapUpdateQueue, \
                               &balance_this->BatteryServiceUpdateWorker,\
                               0 * HZ);
	break;
	//Eason after Pad update firmware, update status ---

	//Hank : change pad icon immediately when pad firmware notify +++
	case P05_BAT_STATUS_CHANGE:
		printk( "[BAT][Bal]P05_BAT_STATUS_CHANGE+++\n");
			schedule_delayed_work(&balance_this->UpdatePadWorker, 0*HZ);
	break;
	//Hank : change pad icon immediately when pad firmware notify ---	

	default:
		pr_debug("[BAT][Bal] %s(), not listened evt: %lu \n", __FUNCTION__, event);
		return NOTIFY_DONE;
	}


	pr_debug("[BAT][Bal] %s() ---\n", __FUNCTION__);
	return NOTIFY_DONE;
}
#endif //CONFIG_EEPROM_NUVOTON
//ASUS_BSP --- Eason_Chang BalanceMode


#ifdef CONFIG_EEPROM_NUVOTON
static struct notifier_block batSer_microp_notifier = {
        .notifier_call = batSer_microp_event_handler,
};

static void CheckBatEcAc(struct work_struct *dat)
{
        if(true == DecideIfPadDockHaveExtChgAC()){
                Init_Microp_Vbus__Chg();
        }
}
#endif //CONFIG_EEPROM_NUVOTON

//Eason: dynamic set Pad alarm +++
#ifndef ASUS_FACTORY_BUILD
#ifdef CONFIG_EEPROM_NUVOTON
static int CalBalanceInterval(void)
{
	int BalanceInterval;
	int StopInterval_Ratio_1p3;
	int StopInterval_90p;
	int StopInterval_20p;

	//f2=f1+(900*100/2100)*x1/3600 
	//P2=p1-25*(x1/3600)    , (900*5V)/(19*0.95)~=25
	//f2/p2<=1.3
	StopInterval_Ratio_1p3 = (balance_this->Pad_capacity*13104 - balance_this->A66_capacity*10080)/211;

	//f2=f1+(900*100/2100)*x1/3600 
	//f2<=90
	StopInterval_90p =  (7560 - (balance_this->A66_capacity*84));

	BalanceInterval=min(StopInterval_Ratio_1p3,StopInterval_90p);

	
	if(balance_this->A66_capacity<20)
	{
			//f2=f1+(900*100/2100)*x1/3600
			StopInterval_20p = (1680 - (balance_this->A66_capacity*84));
			
			BalanceInterval = max(StopInterval_20p,RTCSetIntervalwhenAlarmIntervalLess3min);
			printk("[BAT][Bal]:Phone less 20p:%d\n",BalanceInterval);
	}else if(BalanceInterval<=180)
	{
			BalanceInterval = RTCSetIntervalwhenAlarmIntervalLess3min;
			printk("[BAT][Bal]:interval less 180sec:%d\n",BalanceInterval);
	}else if(BalanceInterval>=3600)
	{
			BalanceInterval = RTCSetIntervalwhenBalSuspendStopChg;
			printk("[BAT][Bal]:interval >1hr :%d\n",BalanceInterval);
	}else{
			printk("[BAT][Bal]:interval :%d\n",BalanceInterval);
	}

	return BalanceInterval;
	
}

static int CalPowerBankInterval(void)
{
	int PowerBankInterval;
	int StopPowerBankInterval_90p;

	StopPowerBankInterval_90p =  (7560 - (balance_this->A66_capacity*84));

	PowerBankInterval = StopPowerBankInterval_90p;

	if(0==IsPowerBankCharge)//PowerBank Mode Stop condition
	{
			PowerBankInterval = RTCSetIntervalwhenBalSuspendStopChg;
			printk("[BAT][Bal][PwrB]:stop chg interval 1hr:%d\n",PowerBankInterval);
	}
	else if(StopPowerBankInterval_90p <= 180)
	{	
			PowerBankInterval = RTCSetIntervalwhenAlarmIntervalLess3min;	
			printk("[BAT][Bal][PwrB]:interval less 180sec:%d\n",PowerBankInterval);
	}else if(PowerBankInterval>=3600)
	{
			PowerBankInterval = RTCSetIntervalwhenBalSuspendStopChg;
			printk("[BAT][Bal][PwrB]:interval >1hr :%d\n",PowerBankInterval);
	}else{
			printk("[BAT][Bal][PwrB]:interval :%d\n",PowerBankInterval);
	}

	return  PowerBankInterval;
}

   
static void decideBalanceModeInterval(void)
{
	if(1==IsBalanceMode)
	{
		RTCSetIntervalwhenBalanceMode= CalBalanceInterval();
	}else if((0==IsBalanceMode)||(2==IsBalanceMode)){//Eason: do ForcePowerBankMode
		RTCSetIntervalwhenBalanceMode= CalPowerBankInterval();
	}
}

static void DoWhenPadAlarmResume(void)
{	
    printk("[BAT][Ser]:PadAlarmResume()+++\n");

        balance_this->IsResumeUpdate = true;
        balance_this->IsResumeMahUpdate = true;
        balance_this->P02_IsResumeUpdate = true;

        if(delayed_work_pending(&balance_this->BatteryServiceUpdateWorker))
        {
            cancel_delayed_work_sync(&balance_this->BatteryServiceUpdateWorker);
        }
	//Hank: cancel BatteryServiceUpdateWorker need calculate capacity+++
	balance_this->NeedCalCap = true;
	 //Hank: cancel BatteryServiceUpdateWorker need calculate capacity--- 
        pr_debug("[BAT][SER][Pad]%s queue BatteryServiceUpdateWorker with calculate capacity\n",__func__);  
        queue_delayed_work(balance_this->BatteryServiceCapUpdateQueue, \
                               &balance_this->BatteryServiceUpdateWorker,\
                               0 * HZ);

        if( false == reportRtcReady()){
            queue_delayed_work(balance_this->BatteryServiceCapUpdateQueue,
                                   &balance_this->BatRtcReadyWorker,
                                   RTC_READY_DELAY_TIME * HZ);
        }

        printk("[BAT][Ser]:PadAlarmResume()---\n");
}
#endif //CONFIG_EEPROM_NUVOTON
#endif //ASUS_FACTORY_BUILD
//Eason: dynamic set Pad alarm ---

//Eason set alarm +++
void SetRTCAlarm(void)
{
    int alarm_type = 0;
    uint32_t alarm_type_mask = 1U << alarm_type;
    unsigned long flags;
    struct timespec new_alarm_time;
    struct timespec mtNow;

    mtNow = current_kernel_time(); 
    new_alarm_time.tv_sec = 0;
    new_alarm_time.tv_nsec = 0;

    pr_debug("[BAT][alarm]:%ld.%ld\n",mtNow.tv_sec,mtNow.tv_nsec);

//Eason: dynamic set Pad alarm +++
#ifdef ASUS_FACTORY_BUILD
		new_alarm_time.tv_sec = mtNow.tv_sec+RTCSetInterval;
#else
	 #ifdef CONFIG_EEPROM_NUVOTON    
		if((1==AX_MicroP_IsP01Connected())&&( true == DecideIfPadDockHaveExtChgAC()))
		{
			new_alarm_time.tv_sec = mtNow.tv_sec+RTCSetInterval;
		}else if(( 0==IsBalanceMode)||( 2==IsBalanceMode))//PowerBankMode//Eason: do ForcePowerBankMode
		{			
			decideBalanceModeInterval();
			new_alarm_time.tv_sec = mtNow.tv_sec+RTCSetIntervalwhenBalanceMode;
		}else if( (true==IsBalanceSuspendStartcharge) && ( 1==IsBalanceMode))//BalanceMode need do suspend charge
		{
			decideBalanceModeInterval();
			new_alarm_time.tv_sec = mtNow.tv_sec+RTCSetIntervalwhenBalanceMode;
		}else{//BalanceMode dont need do suspend charge
			new_alarm_time.tv_sec = mtNow.tv_sec+RTCSetIntervalwhenBalSuspendStopChg;
		}
	#endif	
#endif
//Eason: dynamic set Pad alarm ---
    
    pr_debug("[BAT][alarm]:%ld,A66:%d\n",new_alarm_time.tv_sec,balance_this->A66_capacity);
    //ReportTime();
    spin_lock_irqsave(&bat_alarm_slock, flags);
    alarm_enabled |= alarm_type_mask;
    alarm_start(&bat_alarm,timespec_to_ktime(new_alarm_time));
    spin_unlock_irqrestore(&bat_alarm_slock, flags);

}

static enum alarmtimer_restart alarm_handler(struct alarm *alarm, ktime_t now)
{
	unsigned long flags;

	printk("[BAT]battery alarm triggered\n");
	spin_lock_irqsave(&bat_alarm_slock, flags);

	wake_lock_timeout(&bat_alarm_wake_lock, 3 * HZ);
	wake_up(&bat_alarm_wait_queue);

	spin_unlock_irqrestore(&bat_alarm_slock, flags);
//Eason: dynamic set Pad alarm +++
#ifdef ASUS_FACTORY_BUILD
    SetRTCAlarm();
#else
	if(true==InSuspendNeedDoPadAlarmHandler)//Pad alarm handler need to do only when display off
	{
		queue_delayed_work(balance_this->BatteryServiceCapUpdateQueue, \
		                               &balance_this->PadAlarmResumeWorker,\
		                               0 * HZ);
	}
#endif
//Eason: dynamic set Pad alarm ---
	return ALARMTIMER_NORESTART;
}


static void SetBatLowRTCAlarm(void)
{
    int batLowAlarm_type = 0;
    uint32_t batLowAlarm_type_mask = 1U << batLowAlarm_type;
    unsigned long batlowflags;
    struct timespec new_batLowAlarm_time;
    struct timespec mtNow;

    mtNow = current_kernel_time(); 
    new_batLowAlarm_time.tv_sec = 0;
    new_batLowAlarm_time.tv_nsec = 0;

    printk("[BAT][alarm][BatLow]:%ld.%ld\n",mtNow.tv_sec,mtNow.tv_nsec);


    new_batLowAlarm_time.tv_sec = mtNow.tv_sec+RTCSetIntervalwhenBATlow;

    
    printk("[BAT][alarm][BatLow]:%ld,A66:%d\n",new_batLowAlarm_time.tv_sec
                                ,balance_this->BatteryService_IsBatLow);
    //ReportTime();
    spin_lock_irqsave(&batLow_alarm_slock, batlowflags);
    batLowAlarm_enabled |= batLowAlarm_type_mask;
    alarm_start(&batLow_alarm,timespec_to_ktime(new_batLowAlarm_time));
    spin_unlock_irqrestore(&batLow_alarm_slock, batlowflags);

} 

static enum alarmtimer_restart batLowAlarm_handler(struct alarm *alarm, ktime_t now)
{
	unsigned long batlowflags;

	printk("[BAT][alarm]batLow alarm triggered\n");
	spin_lock_irqsave(&batLow_alarm_slock, batlowflags);

	wake_lock_timeout(&batLow_alarm_wake_lock, 3 * HZ);
	wake_up(&batLow_alarm_wait_queue);

	spin_unlock_irqrestore(&batLow_alarm_slock, batlowflags);
    SetBatLowRTCAlarm();
    
    return ALARMTIMER_NORESTART;
}

static void SetCableInRTCAlarm(void)
{
    int cableInAlarm_type = 0;
    uint32_t cableInAlarm_type_mask = 1U << cableInAlarm_type;
    unsigned long cableInflags;
    struct timespec new_cableInAlarm_time;
    struct timespec mtNow;

    mtNow = current_kernel_time(); 
    new_cableInAlarm_time.tv_sec = 0;
    new_cableInAlarm_time.tv_nsec = 0;

    //printk("[BAT][alarm][cableIn]:%ld.%ld\n",mtNow.tv_sec,mtNow.tv_nsec);


    new_cableInAlarm_time.tv_sec = mtNow.tv_sec+RTCSetIntervalwhenCABLEIn;

    
    printk("[BAT][SER][Alarm]%s(): NowTime:%ld, CableInAlarm:%ld, CableInOut:%d\n",__func__,mtNow.tv_sec,new_cableInAlarm_time.tv_sec
                                ,balance_this->BatteryService_IsCable);
    //ReportTime();
    spin_lock_irqsave(&cableIn_alarm_slock, cableInflags);
    cableInAlarm_enabled |= cableInAlarm_type_mask;
    alarm_start(&cableIn_alarm,timespec_to_ktime(new_cableInAlarm_time));
    spin_unlock_irqrestore(&cableIn_alarm_slock, cableInflags);

} 

static enum alarmtimer_restart cableInAlarm_handler(struct alarm *alarm, ktime_t now)
{
	unsigned long cableInflags;

	printk("[BAT][alarm]cableIn alarm triggered\n");
	spin_lock_irqsave(&cableIn_alarm_slock, cableInflags);

	wake_lock_timeout(&cableIn_alarm_wake_lock, 3 * HZ);
	wake_up(&cableIn_alarm_wait_queue);

	spin_unlock_irqrestore(&cableIn_alarm_slock, cableInflags);
    SetCableInRTCAlarm();
    
    return ALARMTIMER_NORESTART;
}
//Eason set alarm ---

static void CheckBatRtcReady(struct work_struct *dat)
{
       AXC_BatteryService *_this = container_of(dat,AXC_BatteryService,\
                                                BatRtcReadyWorker.work);
       
       if( true == reportRtcReady())
       {
            _this->savedTime=updateNowTime(_this);
		//Eason: when change MaxMah clear interval+++
		_this->ForceSavedTime = updateNowTime(_this);
		//Eason: when change MaxMah clear interval---
            //ASUS_BSP Eason_Chang 1120 porting +++
	     #ifdef CONFIG_EEPROM_NUVOTON              
            if(1==AX_MicroP_IsP01Connected())
            {
                    _this->P02_savedTime = updateNowTime(_this);
            }
	     #endif
	     //ASUS_BSP Eason_Chang 1120 porting ---               
            printk("[BAT][Ser]sys time ready\n");
       }else{
            queue_delayed_work(_this->BatteryServiceCapUpdateQueue,
                                   &_this->BatRtcReadyWorker,
                                   5 * HZ);
       }       
} 
//Hank: Tigauge Temperature Monitor+++
extern int get_Temp_from_TIgauge(void);
extern int smb346_write_reg(int cmd, void *data);
extern int smb346_read_reg(int cmd, void *data);
//Hank: add charger state check +++
#ifdef CONFIG_BAT_DEBUG
extern int smb346_state_check(void);
#endif
//Hank: add charger state check ---	

bool DisChg = false;
extern int ChargerTempLimit;

static void checkCalCapTime(void)
{
	time_t nowTime;
	time_t nowKeep5MinInterval;
	time_t nowKeepInterval;
	nowTime = updateNowTime(balance_this);

	nowKeep5MinInterval = nowTime - balance_this->Keep5MinSavedTime;
	//Hank: Interval overflow handling+++	
      if(nowKeep5MinInterval <0)
      {
    		printk("[BAT][SER]%s():NowKeep5MinInterval overflow set interval KEEP_CAPACITY_TIME!\n",__func__);
		nowKeep5MinInterval = KEEP_CAPACITY_TIME;
      }
      //Hank: Interval overflow handling---

	nowKeepInterval = nowTime - balance_this->savedTime;
	//Hank: Interval overflow handling+++	
      if(nowKeepInterval <0)
      {
    		printk("[BAT][SER]%s():NowKeepInterval overflow set interval DEFAULT_POLLING_INTERVAL!\n",__func__);
		nowKeepInterval = DEFAULT_POLLING_INTERVAL;
      }
      //Hank: Interval overflow handling---
	
       pr_debug("[BAT][SER]%s()+++\n",__func__);      
       //Hank: if need keep 5Min check the interval if not check default interval+++
       if(balance_this->NeedKeep5Min)
       {
           printk("[BAT][SER]:Need keep 5Min \n");
		   if(nowKeep5MinInterval >= KEEP_CAPACITY_TIME){
			   balance_this->NeedCalCap = true;
			   balance_this->NeedKeep5Min = false;
			   printk("[BAT][SER]%s():already keep 5Min \n",__func__);
		   }else
			   pr_debug("[BAT][SER]%s():not already keep 5Min \n",__func__);
			
	  }
	  else
	  {
	  	   if(balance_this->test.ifFixedPollingInterval(&balance_this->test))
    		   {
        		   //Hank: test interval+++
			   if(!(balance_this->NeedCalCap) && (nowKeepInterval >= (balance_this->test.pollingInterval*3))){
				   balance_this->NeedCalCap = true;
				   printk("[BAT][SER]%s(): nowTime = %d, savedTime = %d, nowKeepInterval = %d \n default calculate capacity polling interval \n",__func__,(int)nowTime,(int)balance_this->savedTime,(int)nowKeepInterval);
			   }//Hank: default 3Min calculate capacity---
			   //Hank: capacity < 14% every 1Min calculate capacity+++
			   else if(!(balance_this->NeedCalCap) && (nowKeepInterval >= balance_this->test.pollingInterval) && balance_this-> A66_capacity < 14){
				   balance_this->NeedCalCap = true;
				   printk("[BAT][SER]%s():capacity < 14 calculate capacity every 1Min \n",__func__);
			   }
			   //Hank: test interval---
    		   }
		   else
		   {
			   //Hank: default 3Min calculate capacity+++
			   if(!(balance_this->NeedCalCap) && (nowKeepInterval >= DEFAULT_POLLING_INTERVAL)){
				   balance_this->NeedCalCap = true;
				   printk("[BAT][SER]%s(): nowTime = %d, savedTime = %d, nowKeepInterval = %d \n default calculate capacity polling interval \n",__func__,(int)nowTime,(int)balance_this->savedTime,(int)nowKeepInterval);
			   }//Hank: default 3Min calculate capacity---
			   //Hank: capacity < 14% every 1Min calculate capacity+++
			   else if(!(balance_this->NeedCalCap) && (nowKeepInterval >= DEFAULT_MONITOR_INTERVAL) && balance_this-> A66_capacity < 14){
				   balance_this->NeedCalCap = true;
				   printk("[BAT][SER]%s():capacity < 14 calculate capacity every 1Min \n",__func__);
			   }
			   //Hank: capacity < 14% every 1Min calculate capacity---
		   }
		   
		   
	   }
	   pr_debug("[BAT][SER]%s---\n",__func__);     
	   //Hank: if need keep 5Min check the interval if not check default interval---
}

static inline int AXC_BatteryService_getNextPollingInterval(struct AXC_BatteryService *_this)
{
/* Eason:cap queue  interval plugs a small value, prevent less than target interval.
*   In BatteryServiceCapSample() function checkCalCapTime() will check interval, then decide if calculate capacity.
*   In AXC_BatteryService_getNextPollingInterval() plug DEFAULT_CAP_QUEUE_INTERVAL_PLUGGED.
*/

    if(_this->test.ifFixedPollingInterval(&_this->test))
    {
        //return _this->test.pollingInterval;
        return (_this->test.pollingInterval+DEFAULT_CAP_QUEUE_INTERVAL_PLUGGED);//Eason:cap queue  interval plugs a small value, prevent less than target interval. 
    }
    else
    {
        //Hank: has cable or capacity < 14% return 1Min+++	
        //return _this->gauge->getNextPollingInterval(_this->gauge);
        if(_this->BatteryService_IsCable || _this-> A66_capacity < 14)
        	//return DEFAULT_MONITOR_INTERVAL;
        	return (DEFAULT_MONITOR_INTERVAL + DEFAULT_CAP_QUEUE_INTERVAL_PLUGGED);
	else
		//return DEFAULT_MONITOR_INTERVAL;
		return (DEFAULT_MONITOR_INTERVAL + DEFAULT_CAP_QUEUE_INTERVAL_PLUGGED);
        //Hank: has cable or capacity < 14% return 1Min---
    }

}
//Hank: Tigauge Temperature Monitor---
static int BatteryServiceGauge_OnCapacityReply(struct AXI_Gauge *gauge, struct AXI_Gauge_Callback *gaugeCb, int batCap, int result);

static void BatteryServiceCapSample(struct work_struct *dat)
{
       AXC_BatteryService *_this = container_of(dat,AXC_BatteryService,BatteryServiceUpdateWorker.work);
       wake_lock(&_this->cap_wake_lock);
	pr_debug("[BAT][SER]:%s+++ \n",__func__);

	//Hank: FirstForceResume do not need check Timing+++
	if(_this->IsFirstForceResume)
	{
		printk("[BAT][SER]%s():_this->IsFirstForceResume = true, set _this->IsFirstForceResume = false\n",__func__);
		//Hank first calculate capacity read battery id+++
		read_battery_id();
		//Hank first calculate capacity read battery id---

		_this->IsFirstForceResume = false;
	}
	else
	{
		pr_debug("[BAT][SER]%s():_this->IsFirstForceResume = false, do checkCalCapTime()\n",__func__);
       	checkCalCapTime();
	}
       //Hank: FirstForceResume do not need check Timing---
       
       if(_this->NeedCalCap)
       {
		 pr_debug("[BAT][SER]%s(): Need Calculate Capacity +++\n",__func__);
		 _this->IsCalculateCapOngoing = true;

		 //ASUS_BSP Eason_Chang 1120 porting +++
#ifdef CONFIG_EEPROM_NUVOTON  
		if(1==AX_MicroP_IsP01Connected())
		{
			_this->P02gauge->askCapacity(_this->P02gauge);
		}
		else{
			_this->gauge->askCapacity(_this->gauge);
		}
#else //#else CONFIG_EEPROM_NUVOTON  
			_this->gauge->askCapacity(_this->gauge);
#endif //CONFIG_EEPROM_NUVOTON//ASUS_BSP Eason_Chang 1120 porting ---
			
		_this->NeedCalCap = false;
		pr_debug("[BAT][SER]%s(): Need Calculate Capacity ---\n",__func__);
	}

	queue_delayed_work(_this->BatteryServiceCapUpdateQueue,&_this->BatteryServiceUpdateWorker,AXC_BatteryService_getNextPollingInterval(_this)* HZ);

	//Hank: because do temperature monitor every time move unlock & +++
	wake_unlock(&_this->cap_wake_lock);
	//Hank: because do temperature monitor every time move unlock ---	
	pr_debug("[BAT][SER]:%s()--- \n",__func__);
}

#ifdef CONFIG_EEPROM_NUVOTON
static time_t P02_BatteryService_getIntervalSinceLastUpdate(AXC_BatteryService  *_this)
{
    struct timespec mtNow;
    
    time_t P02_intervalSinceLastUpdate;
    
    mtNow = current_kernel_time();

    if(_this->test.ifFixedFilterLastUpdateInterval(&_this->test)){
        
        P02_intervalSinceLastUpdate = _this->test.filterLastUpdateInterval;
        
    }else if( true == _this->P02_IsFirstAskCap){
    
        P02_intervalSinceLastUpdate = 0;
          
    }else{

        if(mtNow.tv_sec >= _this->P02_savedTime){

            pr_debug("[BAT][Ser][P02]%s:%ld,%ld\n",__FUNCTION__,mtNow.tv_sec,_this->P02_savedTime);
            
            P02_intervalSinceLastUpdate = mtNow.tv_sec - _this->P02_savedTime;

            //cause system time didn't work at first time update capacity (8secs) 
            //filter intervalSinceLastUpdate more than one month
            if(P02_intervalSinceLastUpdate > 2592000){
                printk("[BAT][Ser]wrongInt %ld \n",P02_intervalSinceLastUpdate);
                P02_intervalSinceLastUpdate = 180;
            }    
         
        }else{
        
            printk("[BAT][Ser]%s:OVERFLOW....%ld,%ld\n",__FUNCTION__,mtNow.tv_sec,_this->P02_savedTime);              
            //todo: to do the correct calculation here....
            P02_intervalSinceLastUpdate = mtNow.tv_sec;
        }
    }

    return P02_intervalSinceLastUpdate ; 
}
#endif

static time_t BatteryService_getIntervalSinceLastUpdate(AXC_BatteryService  *_this)
{
    struct timespec mtNow;
    
    time_t intervalSinceLastUpdate;
    
    mtNow = current_kernel_time();

    if(_this->test.ifFixedFilterLastUpdateInterval(&_this->test)){
        intervalSinceLastUpdate = _this->test.filterLastUpdateInterval;        
    }
    else if( true == _this->IsFirstAskCap){ 
        intervalSinceLastUpdate = 0;    
    }
    else{

        if(mtNow.tv_sec >= _this->savedTime){
            	pr_debug("[BAT][Ser]%s:%ld,%ld\n",__FUNCTION__,mtNow.tv_sec,_this->savedTime);
            
            	intervalSinceLastUpdate = mtNow.tv_sec - _this->savedTime;

		//Eason: if BatLow keep 15 min, shutdown devices, prevent gauge not low Cap+++
		if( (true==_this->BatteryService_IsBatLow) && ((mtNow.tv_sec - batLowTriggerTime )>BATLOW_KEEPTIME_SHUTDOWN) )
		{
				g_batLowLongTimeShut = true;
				ASUSEvtlog("[BAT][Fil][BatLow]Long tme\n");
		}
		//Eason: if BatLow keep 15 min, shutdown devices, prevent gauge not low Cap---

            	//cause system time didn't work at first time update capacity (8secs) 
            	//filter intervalSinceLastUpdate more than one month
            	if(intervalSinceLastUpdate > 2592000)
	     	{
                	printk("[BAT][SER]%s():wrongInt %ld \n",__func__,intervalSinceLastUpdate);
                	intervalSinceLastUpdate = 180;
            	}        
        }
	 else{
        
            	printk("[BAT][Ser]%s:OVERFLOW....%ld,%ld\n",__FUNCTION__,mtNow.tv_sec,_this->savedTime);              
            	//todo: to do the correct calculation here....
            	intervalSinceLastUpdate = 180;
        }
    }

    return intervalSinceLastUpdate ; 
}

static int BatteryService_ChooseMaxMah(AXC_BatteryService  *_this, bool MahDrop)
{
    //Eason : In suspend have same cap don't update savedTime +++
    SameCapDontUpdateSavedTime = false;
    //Eason : In suspend have same cap don't update savedTime ---

    //Eason : if last time is 10mA +++
    if ( (NO_CHARGER_TYPE==_this->chargerType)||((NOTDEFINE_TYPE==_this->chargerType)) )
    {
    		//printk("dont change IsLastTimeMah10mA\n");
    }
    else{
	    	//Eason: when change MaxMah clear interval+++
	    	if(true == IsLastTimeMah10mA)
    		{
    			IfUpdateSavedTime = true;
    		}
	       IsLastTimeMah10mA = false;
		//Eason: when change MaxMah clear interval---
    }
    //Eason : if last time is 10mA ---

    switch(_this->chargerType)
    {
        case NO_CHARGER_TYPE:
             if(false == _this->HasCableBeforeSuspend && true==_this->IsResumeMahUpdate)
	      {
                	_this->IsResumeMahUpdate = false;
                	//Eason : In suspend have same cap don't update savedTime +++
                	SameCapDontUpdateSavedTime = true;
                	//Eason : In suspend have same cap don't update savedTime ---
			//Eason : if last time is 10mA +++
			IsLastTimeMah10mA = true;
			//Eason : if last time is 10mA ---
                	return SUSPEND_DISCHG_CURRENT;
             }
	      else
	      {
			//Eason : if last time is 10mA +++
			if(true == IsLastTimeMah10mA)
			{
				IfUpdateSavedTime = true;
			}
			IsLastTimeMah10mA = false;
			//Eason : if last time is 10mA ---
                	return MAX_DISCHG_CURRENT;
             }
		  
		case ILLEGAL_CHARGER_TYPE:
			if(false == MahDrop){
				return A91_ILLEGAL_CHG_CURRENT;//Eason: PM8941 illegal charger can only draw 500mA
			}
			else{
				return MAX_DISCHG_CURRENT-USB_CHG_CURRENT;
			}
  
        case LOW_CURRENT_CHARGER_TYPE:
             if(false == MahDrop){
					return USB_CHG_CURRENT;//Eason: PM8941 usb can only draw 500mA
				}
				else{
					return MAX_DISCHG_CURRENT-USB_CHG_CURRENT;
             }

#ifdef CONFIG_IDTP9023_CHARGER    
	  case WC_CHARGER_TYPE:    
		   return WIRELESS_CHG_CURRENT;
#endif  							
		  
        case NORMAL_CURRENT_CHARGER_TYPE:
                return PAD_CHG_CURRENT;
  
        case HIGH_CURRENT_CHARGER_TYPE:
             return AC_SUSPEND_CHG_CURRENT;
			 
        default:
             printk("[BAT][Ser]:%s():NO mapping\n",__FUNCTION__);
             if(true==_this->IsResumeMahUpdate)
	      {
                	_this->IsResumeMahUpdate = false;
                	//Eason : In suspend have same cap don't update savedTime +++
                	SameCapDontUpdateSavedTime = true;
                	//Eason : In suspend have same cap don't update savedTime ---
			//Eason : if last time is 10mA +++
			IsLastTimeMah10mA = true;
			//Eason : if last time is 10mA ---
                	return SUSPEND_DISCHG_CURRENT;
             }
	      else
	      {
			//Eason : if last time is 10mA +++
			if(true==IsLastTimeMah10mA)
			{
				IfUpdateSavedTime = true;
			}
			IsLastTimeMah10mA = false;
			//Eason : if last time is 10mA ---
                	return MAX_DISCHG_CURRENT;
             }   
        }     
}

static void CheckEoc(struct work_struct *dat)
{
    AXC_BatteryService *_this = container_of(dat,AXC_BatteryService,BatEocWorker.work);

    static int count = 0;
    
    if(NO_CHARGER_TYPE >= gpCharger->GetChargerStatus(gpCharger) ||!gpCharger->IsCharging(gpCharger))//if no charger && not being charging
    {
        count = 0;
        return;
    }

    if(count < 3)//recursive check
    {
	        int nCurrent =  _this->callback->getIBAT(_this->callback);
	        if(0 >= nCurrent &&  -90 < nCurrent && _this->A66_capacity >= 94)
		 {
	            count ++;
	            if(!delayed_work_pending(&_this->BatEocWorker))
		     {
		     		//why 10*Hz?
	                    queue_delayed_work(_this->BatteryServiceCapUpdateQueue, &_this->BatEocWorker,10 * HZ);
	            }

	        }
		 else
		 {
	            count = 0;
	            return;
	        }
     }
	
    printk("[BAT][Ser]%s:chg done\n",__FUNCTION__);
    _this->isMainBatteryChargingDone = true;
    return;
}

static void ResumeCalCap(struct work_struct *dat)
{
    time_t nowResumeTime;
    time_t nowResumeInterval;
    bool needDoResume=false;

    nowResumeTime = updateNowTime(balance_this);
    nowResumeInterval = nowResumeTime - balance_this->savedTime;
    //Hank: Interval overflow handling+++	
    if(nowResumeInterval <0)
    {
    		printk("[BAT][SER]%s():Interval overflow set interval RESUME_UPDATE_TIME!\n",__func__);
		nowResumeInterval = RESUME_UPDATE_TIME;
    }
    //Hank: Interval overflow handling---

    pr_debug("[BAT][SER]:resume queue+++\n");

    if(true == balance_this->BatteryService_IsBatLow 
        && nowResumeInterval > RESUME_UPDATE_TIMEwhenBATlow)
    {
           needDoResume = true; 
    }    
    else if(balance_this->A66_capacity <= CapChangeRTCInterval 
        && nowResumeInterval > RESUME_UPDATE_TIMEwhenCapLess20)
    {
           needDoResume = true;                
    }else if(nowResumeInterval > RESUME_UPDATE_TIME){
           needDoResume = true;
    }
    printk("[BAT][SER]:ResumeCalCap()===:%ld,%ld,%ld,A66:%d\n"
            ,nowResumeTime,balance_this->savedTime,nowResumeInterval,balance_this->A66_capacity);

    //ReportTime();

//Eason resume always calculate capacity no matter if in   Pad or CableIn or BatLow+++
   if(true==needDoResume)
   {
   	  //Eason set these flag when true==needDoResume+++
         balance_this->IsResumeUpdate = true;
         balance_this->IsResumeMahUpdate = true;
         balance_this->P02_IsResumeUpdate = true;
	  //Eason set these flag when true==needDoResume---	
	
        if(delayed_work_pending(&balance_this->BatteryServiceUpdateWorker))
        {
            cancel_delayed_work_sync(&balance_this->BatteryServiceUpdateWorker); 
        }
	 //Hank: need calculate capacity & temperature monitor+++
	balance_this->NeedCalCap = true;
	//Hank: need calculate capacity & temperature monitor--- 
        pr_debug("[Bat][SER]%s(): queue BatteryServiceUpdateWorker with calculate capacity\n",__func__);  
        queue_delayed_work(balance_this->BatteryServiceCapUpdateQueue, \
                               &balance_this->BatteryServiceUpdateWorker,\
                               0 * HZ);
    }
   else{
   	if(delayed_work_pending(&balance_this->BatteryServiceUpdateWorker))
	{
	            cancel_delayed_work_sync(&balance_this->BatteryServiceUpdateWorker);           
	 }
	  //Hank: temperature monitor only+++
	balance_this->NeedCalCap = false;
	  //Hank: temperature monitor only---
        pr_debug("[Bat][Ser]%s queue BatteryServiceUpdateWorker without calculate capacity\n",__func__);     
        queue_delayed_work(balance_this->BatteryServiceCapUpdateQueue, \
                               &balance_this->BatteryServiceUpdateWorker,\
                               0 * HZ);
   }
   pr_debug("[BAT][SER]:resume queue---\n");
//Eason resume always calculate capacity no matter if in   Pad or CableIn or BatLow---		
}

static void CableOffKeep5Min(struct work_struct *dat)
{
    time_t nowCableOffTime;
    time_t nowCableOffInterval;
    bool needDoCableOffKeep5Min=false;

    nowCableOffTime = updateNowTime(balance_this);
    nowCableOffInterval = nowCableOffTime - balance_this->savedTime;

    if(nowCableOffInterval <  KEEP_CAPACITY_TIME){
           needDoCableOffKeep5Min = true;
    }
    printk("[BAT][SER]:CableOffKeep5()===:%ld,%ld,%ld\n"
            ,nowCableOffTime,balance_this->savedTime,nowCableOffInterval);
    //ReportTime();

    if(true==needDoCableOffKeep5Min)
    {
        if(delayed_work_pending(&balance_this->BatteryServiceUpdateWorker))
        {
            cancel_delayed_work_sync(&balance_this->BatteryServiceUpdateWorker); 
        }
		
	 //Hank:	keep capacity 5Min save now time+++
	balance_this->Keep5MinSavedTime = updateNowTime(balance_this);
	//Hank:	keep capacity 5Min save now time---

	//Hank:	need keep capacity 5Min +++
	balance_this->NeedKeep5Min = true;
	balance_this->NeedCalCap = false;
	//Hank:	need keep capacity 5Min ---
	
       pr_debug("[Bat][SER]%s(): queue BatteryServiceUpdateWorker without calculate capacity\n",__func__);
       queue_delayed_work(balance_this->BatteryServiceCapUpdateQueue, \
                               &balance_this->BatteryServiceUpdateWorker,\
                               0 * HZ);
        
        balance_this->savedTime = updateNowTime(balance_this); 
	#ifdef CONFIG_EEPROM_NUVOTON  //ASUS_BSP Eason_Chang 1120 porting +++        
        if(1==AX_MicroP_IsP01Connected())
        {
                balance_this->P02_savedTime = updateNowTime(balance_this);
        } 
	#endif //CONFIG_EEPROM_NUVOTON //ASUS_BSP Eason_Chang 1120 porting ---        

	printk("[BAT][SER]:CableOffKeep5():savedtime:%ld,%ld\n"
            , balance_this->savedTime
            , balance_this->P02_savedTime
            );
    }    
}

#ifdef CONFIG_EEPROM_NUVOTON
//Eason: dynamic set Pad alarm +++		
#ifdef ASUS_FACTORY_BUILD	
static void PlugIntoP02SetRTC(struct work_struct *dat)
{
        SetRTCAlarm();
}
#else
static void PadRTCAlarmResume(struct work_struct *dat)
{
        DoWhenPadAlarmResume();
}
#endif
//Eason: dynamic set Pad alarm ---
#endif //CONFIG_EEPROM_NUVOTON

static void BatLowTriggeredSetRTC(struct work_struct *dat)
{
        SetBatLowRTCAlarm();
}
//Eason cable in set alarm +++
static void CableInTriggeredSetRTC(struct work_struct *dat)
{
        SetCableInRTCAlarm();
}
//Eason cable in set alarm ---

#ifdef CONFIG_EEPROM_NUVOTON
//Eason after Pad update firmware, update status +++
static void UpdatePadInfo(struct work_struct *dat)
{
	//balance_this->callback->onServiceStatusUpdated(balance_this->callback);
	asus_bat_update_PadAcOnline();
}
//Eason after Pad update firmware, update status ---
#endif

//Hank in rom mode show "?" : bootUp check Rom mode queue+++
#ifdef CONFIG_TI_GAUGE
static void CheckIsGaugeRom(struct work_struct *dat)
{
	printk("[BAT][Ser]:bootUpCheckRomMode:%d\n",check_is_RomMode());
}
#endif
//Hank in rom mode show "?" : bootUp check Rom mode queue---

//Eason: use queue doBalanceMode in less 5min forceResume+++ 
#ifdef CONFIG_EEPROM_NUVOTON
static void forceResumeLess5minDobalanceWork(struct work_struct *dat)
{
		if(false == DecideIfPadDockHaveExtChgAC())
		{
	    		printk("[BAT][Ser]:less 5 min forceResume()+++\n");
	    		BatteryServiceDoBalance(balance_this);
			printk("[BAT][Ser]:less 5 min forceResume()---\n");	
		}	
}
#endif
//Eason: use queue doBalanceMode in less 5min forceResume---

static void BatteryService_InitWoker(AXC_BatteryService *_this)
{
	_this->BatteryServiceCapUpdateQueue \
		= create_singlethread_workqueue("BatteryServiceCapUpdateQueue");
	INIT_DELAYED_WORK(&_this->BatteryServiceUpdateWorker, BatteryServiceCapSample);

	INIT_DELAYED_WORK(&_this->BatRtcReadyWorker, CheckBatRtcReady);

#ifdef CONFIG_EEPROM_NUVOTON
	INIT_DELAYED_WORK(&_this->BatEcAcWorker, CheckBatEcAc);
#endif

	INIT_DELAYED_WORK(&_this->BatEocWorker, CheckEoc);

	INIT_DELAYED_WORK(&_this->ResumeWorker, ResumeCalCap);

	INIT_DELAYED_WORK(&_this->CableOffWorker, CableOffKeep5Min);

#ifdef CONFIG_EEPROM_NUVOTON
//Eason: dynamic set Pad alarm +++		
#ifdef ASUS_FACTORY_BUILD	
	INIT_DELAYED_WORK(&_this->SetRTCWorker, PlugIntoP02SetRTC);
#else  
	INIT_DELAYED_WORK(&_this->PadAlarmResumeWorker, PadRTCAlarmResume);
#endif
//Eason: dynamic set Pad alarm ---
#endif

	INIT_DELAYED_WORK(&_this->SetBatLowRTCWorker, BatLowTriggeredSetRTC);

	INIT_DELAYED_WORK(&_this->SetCableInRTCWorker, CableInTriggeredSetRTC); 

#ifdef CONFIG_EEPROM_NUVOTON
	//Eason after Pad update firmware, update status +++
	INIT_DELAYED_WORK(&_this->UpdatePadWorker,UpdatePadInfo);
	//Eason after Pad update firmware, update status ---

	//Eason: use queue doBalanceMode in less 5min forceResume+++ 
	INIT_DELAYED_WORK(&_this->Less5MinDoBalanceWorker,forceResumeLess5minDobalanceWork);
	//Eason: use queue doBalanceMode in less 5min forceResume---
#endif

	//Hank in rom mode show "?" : bootUp check Rom mode queue+++
#ifdef CONFIG_TI_GAUGE
	INIT_DELAYED_WORK(&_this->CheckGaugeRomModeWorker,CheckIsGaugeRom);
#endif
	//Hank in rom mode show "?" : bootUp check Rom mode queue---

}

static AXE_BAT_CHARGING_STATUS  AXC_BatteryService_getChargingStatus(struct AXI_BatteryServiceFacade * bat)
{
    AXE_BAT_CHARGING_STATUS status = BAT_DISCHARGING_STATUS; 

    AXC_BatteryService  *_this=
        container_of(bat, AXC_BatteryService, miParent);

    switch(_this->fsmState){ 
    case DISCHARGING_STATE:
         status = BAT_DISCHARGING_STATUS;
         break;
    case CHARGING_STATE:
         status = BAT_CHARGING_STATUS;
         break;
    case CHARGING_STOP_STATE:
#ifndef ASUS_FACTORY_BUILD       
    #ifdef CONFIG_EEPROM_NUVOTON  //ASUS_BSP Eason_Chang 1120 porting +++        
             //ASUS_BSP +++ Eason_Chang BalanceMode
             if(1==AX_MicroP_IsP01Connected()){       
                    status = BAT_NOT_CHARGING_STATUS;  
             }else{              
             //ASUS_BSP --- Eason_Chang BalanceMode
             status = BAT_CHARGING_STATUS;
             }
    #else//CONFIG_EEPROM_NUVOTON//ASUS_BSP Eason_Chang 1120 porting
                status = BAT_CHARGING_STATUS;
    #endif //CONFIG_EEPROM_NUVOTON//ASUS_BSP Eason_Chang 1120 porting ---
#else
                status = BAT_NOT_CHARGING_STATUS;
#endif//#ifndef ASUS_FACTORY_BUILD
         break;
    case CHARGING_FULL_STATE:
         status = BAT_CHARGING_FULL_STATUS;
         break;
    case CHARGING_FULL_KEEP_STATE:
         status = BAT_CHARGING_FULL_STATUS;
         break;
    case CHARGING_FULL_KEEP_STOP_STATE:
         status = BAT_CHARGING_FULL_STATUS;
         break;
    default:
         printk("[BAT][Ser]%s():status error\n",__FUNCTION__);
    }
    return status;
}
static int  AXC_BatteryService_getCapacity(struct AXI_BatteryServiceFacade * bat)
{
    AXC_BatteryService  *_this=
        container_of(bat, AXC_BatteryService, miParent);

#ifdef CONFIG_EEPROM_NUVOTON
	if(1==IsBalanceTest())
	   return _this->A66_capacity;
	//Eason show BMS cap to user+++
	else
#endif
	{
#if USE_SW_GAUGE //report capacity by SW gauge
		return _this->A66_capacity;
#else //report capacity by BMS
		if(gBMS_Cap <= 1){
			if(true==g_BootUp_IsBatLow ){
				if(1==getIfonline()){
					printk("[BAT][BMS]BootUp IsBatLow, Cable on, dont shutdown:%d\n",gBMS_Cap);
					return 1;
				}
				else{
					printk("[BAT][BMS]BootUp IsBatLow, Cable off, need shutdown:%d\n",gBMS_Cap);

					#ifdef CONFIG_FASTBOOT//Eason: fix fastboot kernel poweroff when BAT cap not 0%
						if(is_fastboot_enable()){
							printk("[BAT][Fastboot]kernel_power_off\n");
							kernel_power_off();
						}
					#endif //#ifdef CONFIG_FASTBOOT
					
					return 0;
				}
			}
			else if(false==_this->BatteryService_IsBatLow){
				printk("[BAT][BMS]Bat low not yet:%d\n",gBMS_Cap);
				return 1;
			}
			else{
				printk("[BAT][BMS]less 1p & BatLow need shutdown:%d\n",gBMS_Cap);

				#ifdef CONFIG_FASTBOOT//Eason: fix fastboot kernel poweroff when BAT cap not 0%
					if(is_fastboot_enable()){
							printk("[BAT][Fastboot]kernel_power_off\n");
							kernel_power_off();
					}
				#endif //#ifdef CONFIG_FASTBOOT
					
				return 0;//BMS table 1% need mapping to 0%
			}
		}
		else if( (true==_this->BatteryService_IsFULL)&&(gBMS_Cap>=99) ) {
			printk("[BAT][BMS]more than 99p & FULL flag, show 100:%d\n",gBMS_Cap);
			return 100;
		}
		else if( (false==_this->BatteryService_IsFULL)&&(100 == gBMS_Cap) ) {
			printk("[BAT][BMS]100p but Not_FULL flag, show 99:%d\n",gBMS_Cap);
			return 99;			
		}
		else
			return gBMS_Cap;
#endif
	}	
	//Eason show BMS cap to user---
}

static void AXC_BatteryService_onCableInOut(struct AXI_BatteryServiceFacade *bat, AXE_Charger_Type type)
{
	AXC_BatteryService  *_this = container_of(bat, AXC_BatteryService, miParent);

	//Eason cable in set alarm +++
	unsigned long cableInFlags;
	//Eason cable in set alarm ---

	printk("[BAT][Ser]:onCableInOut()+++\n");
	_this->chargerType = type ;
	_this->fsm->onCableInOut(_this->fsm,type);
	if( ( 100 == _this->A66_capacity)&&(true==_this->BatteryService_IsFULL) ){
		_this->fsm->onChargingStop(_this->fsm,CHARGING_DONE);   
	}    

	switch(type){
		case NO_CHARGER_TYPE:
			_this->gauge->notifyCableInOut(_this->gauge,false);
			_this->BatteryService_IsCable = false ;

			//Eason :when  low bat Cap draw large current  +++	 
			if(10 <= _this->A66_capacity )
			{
				schedule_delayed_work(&_this->CableOffWorker,1*HZ);//keep 100% 5 min
			}
			//Eason :when  low bat Cap draw large current  ---

			//Eason cable in set alarm +++
			spin_lock_irqsave(&cableIn_alarm_slock, cableInFlags);
			alarm_try_to_cancel(&cableIn_alarm);
			spin_unlock_irqrestore(&cableIn_alarm_slock, cableInFlags);
			//Eason cable in set alarm --- 
			
			//Eason : Wireless PMA spec Rx turn off Tx +++
#ifdef CONFIG_IDTP9023_CHARGER
			g_WcEocTime = updateNowTime(_this);
#endif
			//Eason : Wireless PMA spec Rx turn off Tx ---

			//_this->BatteryService_IsFULL = false;
			//_this->gauge->notifyBatFullChanged(_this->gauge,false);

			break;
				 
		case ILLEGAL_CHARGER_TYPE:
		case LOW_CURRENT_CHARGER_TYPE:
		case NORMAL_CURRENT_CHARGER_TYPE:
		case HIGH_CURRENT_CHARGER_TYPE:
#ifdef CONFIG_IDTP9023_CHARGER    
		case WC_CHARGER_TYPE:    
#endif  				
			_this->gauge->notifyCableInOut(_this->gauge,true);
			_this->BatteryService_IsCable = true ;
			//Eason: dynamic set Pad alarm, Pad has its own alrarm+++
			if( NORMAL_CURRENT_CHARGER_TYPE!=_this->chargerType)
			{
				//Eason cable in set alarm +++
				schedule_delayed_work(&_this->SetCableInRTCWorker, 0*HZ);
				//Eason cable in set alarm ---
			}
			//Eason: dynamic set Pad alarm, Pad has its own alrarm---

#ifdef ASUS_FACTORY_BUILD
			if( true == charger_limit_enable ){
				g_5060modeCharging = true;
				Do_Factory5060Mode();
			}		
#endif//#ifdef ASUS_FACTORY_BUILD
			break;
		default:
			printk("[BAT][Ser]:%s():NO mapping\n",__FUNCTION__);
			break;
		}

	printk("[BAT][Ser]:onCableInOut():%d---\n",type);
}
static void AXC_BatteryService_onChargingStop(struct AXI_BatteryServiceFacade *bat,AXE_Charging_Error_Reason reason)
{/*
    AXC_BatteryService  *_this=
        container_of(bat, AXC_BatteryService, miParent);

    _this->fsm->onChargingStop(_this->fsm,reason);
    _this->BatteryService_IsCharging = false ;

    if (CHARGING_DONE == reason){
        
        _this->BatteryService_IsFULL = true;
        
        _this->gauge->notifyBatFullChanged(_this->gauge,true);

        wake_lock(&_this->cap_wake_lock);
        _this->gauge->askCapacity(_this->gauge);
    }
 */   
}
static void AXC_BatteryService_onChargingStart(struct AXI_BatteryServiceFacade *bat)
{/*
    AXC_BatteryService  *_this=
        container_of(bat, AXC_BatteryService, miParent);

    _this->fsm->onChargingStart(_this->fsm);
    _this->BatteryService_IsCharging = true ;

 */
}
static void AXC_BatteryService_onBatteryLowAlarm(struct AXI_BatteryServiceFacade *bat, bool isCurrentBattlow)
{
	AXC_BatteryService  *_this=
		container_of(bat, AXC_BatteryService, miParent);
    
	unsigned long batLowFlags;

	if(false==_this->BatteryService_IsBatLow && true==isCurrentBattlow){
		schedule_delayed_work(&balance_this->SetBatLowRTCWorker, 0*HZ);
		//Eason: if BatLow keep 15 min, shutdown devices+++
		batLowTriggerTime = updateNowTime(balance_this);
		ASUSEvtlog("[BAT][Fil][BatLow]TriggerTime\n");
		//Eason: if BatLow keep 15 min, shutdown devices---
	}
	else if(true==_this->BatteryService_IsBatLow && false==isCurrentBattlow){
		spin_lock_irqsave(&batLow_alarm_slock, batLowFlags);
		alarm_try_to_cancel(&batLow_alarm);
		spin_unlock_irqrestore(&batLow_alarm_slock, batLowFlags);
		//Eason: if BatLow keep 15 min, shutdown devices+++
		g_batLowLongTimeShut = false;
		ASUSEvtlog("[BAT][Fil][BatLow]Release\n");
		//Eason: if BatLow keep 15 min, shutdown devices---
	}
	_this->BatteryService_IsBatLow = isCurrentBattlow ;
}
static void AXC_BatteryService_onBatteryRemoved(struct AXI_BatteryServiceFacade * bat, bool isRemoved)
{
   // AXC_BatteryService  *_this=
    //    container_of(bat, AXC_BatteryService, miParent);

}

#ifdef CONFIG_EEPROM_NUVOTON
static void Record_P02BeforeSuspend(void)
{
	if( (1==AX_MicroP_get_USBDetectStatus(Batt_P01))||(2==AX_MicroP_get_USBDetectStatus(Batt_P01)) )//Eason: Pad plug usb show icon & cap can increase
    {
		balance_this->P02_IsCable = true;
	}else{
		balance_this->P02_IsCable = false;
	}
}
static int P02_ChooseMaxMahBeforeSuspend(void)
{
    if(true == DecideIfPadDockHaveExtChgAC())
    {
          return AC_CHG_CURRENT;
    }else{
	  return PAD_CHG_CURRENT;
    }
}

static int P02_ChooseMaxMah(void)
{
   if(true == DecideIfPadDockHaveExtChgAC()){
          return AC_CHG_CURRENT;
   }else{
	  return PAD_CHG_CURRENT;
   }
}

//Eason: A68 new balance mode +++
#ifndef ASUS_FACTORY_BUILD
void doInBalanceModeWhenSuspend(void)
{
	//when resume default turn off vbus +++
	IsSystemdraw = false;
	//when resume default turn off vbus ---

	if( false==IsBalanceSuspendStartcharge )
	{
		set_microp_vbus(0);
		printk("[BAT][Bal]stop 5VPWR:%d,Vbus:%d\n"
                                        ,get_5VPWR_EN(),get_microp_vbus());
		//gpCharger->EnableCharging(gpCharger,false); //Hank: remove disable charging when cable off
		DisChg = true;
		balance_this->fsm->onChargingStop(balance_this->fsm,BALANCE_STOP);               
		IsBalanceCharge = 0;
	
	}else if(true==IsBalanceSuspendStartcharge){//remember this flag to do suspendCharge before suspendStopChg condition match

		set_microp_vbus(1);
		printk("[BAT][Bal]start 5VPWR:%d,Vbus:%d\n"
                                        ,get_5VPWR_EN(),get_microp_vbus());	
		gpCharger->EnableCharging(gpCharger,true);
		DisChg = false;
		balance_this->fsm->onChargingStart(balance_this->fsm);                 
		IsBalanceCharge = 1;
	}
}
#endif//ASUS_FACTORY_BUILD
//Eason: A68 new balance mode ---
#endif //CONFIG_EEPROM_NUVOTON

#ifdef CONFIG_EEPROM_NUVOTON
extern void setChgLimitThermalRuleDrawCurrent(bool isSuspendCharge);
#endif
static void AXC_BatteryService_suspend(struct AXI_BatteryServiceFacade *bat)
{
	AXC_BatteryService  *_this = container_of(bat, AXC_BatteryService, miParent);

	printk("[BAT][Ser]:suspend()+++\n");

	_this->HasCableBeforeSuspend = _this->BatteryService_IsCable;

#ifdef CONFIG_EEPROM_NUVOTON
	//Eason: dynamic set Pad alarm +++
	#ifndef ASUS_FACTORY_BUILD		
	InSuspendNeedDoPadAlarmHandler=true;
	#endif
	//Eason: dynamic set Pad alarm ---	

	Record_P02BeforeSuspend();
	_this->P02_HasCableBeforeSuspend = _this->P02_IsCable;
	_this->P02_MaxMahBeforeSuspend = P02_ChooseMaxMahBeforeSuspend();

	//Eason: A68 new balance mode +++
	#ifndef ASUS_FACTORY_BUILD
	if ((1==AX_MicroP_IsP01Connected())&&(1 == IsBalanceMode)&&(false == DecideIfPadDockHaveExtChgAC()))
	{
		printk("[BAT][Bal]Phone:%d,Pad:%d\n",_this->A66_capacity,_this->Pad_capacity);
		doInBalanceModeWhenSuspend();
	}
	if (1==AX_MicroP_IsP01Connected() && (1==getIfonline()) )
		setChgLimitThermalRuleDrawCurrent(true);
	#endif
#endif	//CONFIG_EEPROM_NUVOTON
	//Eason: A68 new balance mode ---

	printk("[BAT][Ser]:suspend()---\n");
}

#ifndef ASUS_FACTORY_BUILD	
#ifdef CONFIG_EEPROM_NUVOTON
extern void setChgLimitInPadWhenChgReset(void);
#endif
#endif
//Eason resume always calculate capacity no matter if in   Pad or CableIn or BatLow+++
extern void setSmb346CC_Curr900mA_Iterm50(void);
static void AXC_BatteryService_resume(struct AXI_BatteryServiceFacade *bat,int delayStartInSeconds)
{
	AXC_BatteryService  *_this=
		container_of(bat, AXC_BatteryService, miParent);

	printk("[BAT][Ser]:resume()+++\n");

#ifdef CONFIG_EEPROM_NUVOTON
	if(1==AX_MicroP_IsP01Connected())
	{
		#ifndef ASUS_FACTORY_BUILD		
		ASUSEvtlog("[BAT][Bal]resume:%d\n",IsBalanceSuspendStartcharge);
		if( 1==getIfonline() )
			setChgLimitInPadWhenChgReset();
		#endif//ASUS_FACTORY_BUILD
	}
#endif		

	schedule_delayed_work(&_this->ResumeWorker,1*HZ);
	wake_lock_timeout(&_this->resume_wake_lock,2* HZ);

	if( false == reportRtcReady()){
		queue_delayed_work(_this->BatteryServiceCapUpdateQueue,
			&_this->BatRtcReadyWorker,
			RTC_READY_DELAY_TIME * HZ);
	}

	printk("[BAT][Ser]:resume()---\n");
}
//Eason resume always calculate capacity no matter if in   Pad or CableIn or BatLow---

static void AXC_BatteryService_forceResume(struct AXI_BatteryServiceFacade *bat,int delayStartInSeconds)
{
	AXC_BatteryService  *_this=
		container_of(bat, AXC_BatteryService, miParent);

	time_t nowForceResumeTime;
	time_t nowForceResumeInterval;
	bool needDoForceResume=false;


//when forceresume default turn off vbus +++
#ifdef CONFIG_EEPROM_NUVOTON
#ifndef ASUS_FACTORY_BUILD		
	IsSystemdraw= false;
	IsBalanceSuspendStartcharge = false;
	//Eason: dynamic set Pad alarm +++
	InSuspendNeedDoPadAlarmHandler=false;
	//Eason: dynamic set Pad alarm ---	
#endif	
#endif	
//when forceresume default turn off vbus ---

	nowForceResumeTime = updateNowTime(_this);
	nowForceResumeInterval = nowForceResumeTime - _this->savedTime;
	//Hank: Interval overflow handling+++	
	if(nowForceResumeInterval <0){
		printk("[BAT][SER]%s():Interval overflow set interval RESUME_UPDATE_TIME!\n",__func__);
		nowForceResumeInterval = RESUME_UPDATE_TIME;
	}
	//Hank: Interval overflow handling---

	pr_debug("[BAT][Ser]:forceResume()===:%ld,%ld,%ld\n"
		,nowForceResumeTime,_this->savedTime,nowForceResumeInterval);

	if( true==_this->IsFirstForceResume ){
		needDoForceResume = true;
	}
	else if( nowForceResumeInterval >= FORCERESUME_UPDATE_TIME
				&&false==_this->IsCalculateCapOngoing){

		needDoForceResume = true;
	}/*else{
            printk("[BAT][Ser]:forceResume queue 5min\n");
            queue_delayed_work(_this->BatteryServiceCapUpdateQueue, 
                               &_this->BatteryServiceUpdateWorker,
                               FORCERESUME_UPDATE_TIME * HZ);    
    }*/
    

    if( true==needDoForceResume )
    {
        printk("[BAT][Ser]:forceResume()+++\n");

    //if(true == _this->IsSuspend){

        //_this->IsSuspend = false;

        _this->IsResumeUpdate = true;
        _this->IsResumeMahUpdate = true;
        _this->P02_IsResumeUpdate = true;

        if(delayed_work_pending(&_this->BatteryServiceUpdateWorker))
        {
            cancel_delayed_work_sync(&_this->BatteryServiceUpdateWorker);
        }
	 //Hank: cancel BatteryServiceUpdateWorker need calculate capacity+++
	  balance_this->NeedCalCap = true;
	 //Hank: cancel BatteryServiceUpdateWorker need calculate capacity--- 
        pr_debug("[Bat][SER]%s(): queue BatteryServiceUpdateWorker with calculate capacity\n",__func__); 
        queue_delayed_work(_this->BatteryServiceCapUpdateQueue,&_this->BatteryServiceUpdateWorker,\
                               delayStartInSeconds * HZ);
        //ReportTime();

        if( false == reportRtcReady()){
            queue_delayed_work(_this->BatteryServiceCapUpdateQueue,&_this->BatRtcReadyWorker,
                                   RTC_READY_DELAY_TIME * HZ);
        }
    //}

    }
//Eason: A68 new balance mode +++	
#ifndef ASUS_FACTORY_BUILD	
	//Eason: use queue doBalanceMode in less 5min forceResume+++
#ifdef CONFIG_EEPROM_NUVOTON
    else if((1==AX_MicroP_IsP01Connected())&&(1 == IsBalanceMode))			
    {
		queue_delayed_work(_this->BatteryServiceCapUpdateQueue,&_this->Less5MinDoBalanceWorker, 0*HZ);
	//Eason: use queue doBalanceMode in less 5min forceResume---
    }
#endif	
#endif
	else
	{//Hank: Temperature monitor only+++
		balance_this->NeedCalCap = false;
		if(delayed_work_pending(&_this->BatteryServiceUpdateWorker))
		{
			cancel_delayed_work_sync(&_this->BatteryServiceUpdateWorker);
		}
		pr_debug("[BAT][SER]%s(): queue BatteryServiceUpdateWorker without calculate capacity\n",__func__); 
		queue_delayed_work(_this->BatteryServiceCapUpdateQueue, \
								&_this->BatteryServiceUpdateWorker,\
								0 * HZ);
	}//Hank: Temperature monitor only---	
	printk("[BAT][SER]:forceResume()---\n");
//Eason: A68 new balance mode ---

}
static void  AXC_BatteryService_constructor(struct AXC_BatteryService *_this,AXI_BatteryServiceFacadeCallback *callback)
{
	_this->callback = callback;
	printk("[BAT][ser]%s():\n",__FUNCTION__);
	if(false == _this->mbInit){
		//todo....add internal module creation & init here...
		BatteryService_enable_ChargingFsm(_this);// batteryservice to fsm
		BatteryService_enable_Gauge(_this);// batteryservice to fsm
		BatteryService_enable_Filter(_this);
		BatteryService_InitWoker(_this);
#ifdef CONFIG_EEPROM_NUVOTON
		create_balanceChg_proc_file();
#endif
		//Eason: MPdecisionCurrent +++
		create_MPdecisionCurrent_proc_file();
		//Eason: MPdecisionCurrent ---

		//Hank enterRomMode_test++++
#ifdef CONFIG_TI_GAUGE
		create_enterRomMode_proc_file();
#endif
		//Hank enterRomMode_test----

		//Hank read BatteryID+++
#ifdef ASUS_FACTORY_BUILD
		create_BatteryID_proc_file();
		//Eason: charger_limit_enable is used to decide if Do_Factory5060Mode+++
		charger_limit_enable_create_proc_file();
		//Eason: charger_limit_enable is used to decide if Do_Factory5060Mode---
#endif
		//Hank read BatteryID---
		balance_this = _this;
#ifdef CONFIG_EEPROM_NUVOTON
		register_microp_notifier(&batSer_microp_notifier);
		notify_register_microp_notifier(&batSer_microp_notifier, "axc_batteryservice"); //ASUS_BSP Lenter+
#endif //CONFIG_EEPROM_NUVOTON
		//ASUS_BSP --- Eason_Chang BalanceMode
		create_HighPower_proc_file();//[ChiaYuan]Add for Broadcast busy state
		mutex_init(&_this->main_lock);
		mutex_init(&_this->filter_lock);
		wake_lock_init(&_this->cap_wake_lock, WAKE_LOCK_SUSPEND, "bat_cap");
		wake_lock_init(&_this->resume_wake_lock, WAKE_LOCK_SUSPEND, "resume_wake"); 
		//Eason set alarm +++
		alarm_init(&bat_alarm, 0, alarm_handler);
		alarm_init(&batLow_alarm, 0, batLowAlarm_handler);
		alarm_init(&cableIn_alarm, 0, cableInAlarm_handler);
		wake_lock_init(&bat_alarm_wake_lock, WAKE_LOCK_SUSPEND, "bat_alarm_wake");
		wake_lock_init(&batLow_alarm_wake_lock, WAKE_LOCK_SUSPEND, "batLow_alarm_wake");
		wake_lock_init(&cableIn_alarm_wake_lock, WAKE_LOCK_SUSPEND, "cableIn_alarm_wake");
		//Eason set alarm ---
		//ASUS_BSP  +++ Eason_Chang charger
		gpCharger = getAsusCharger();
		gpCharger->Init(gpCharger);
		gpCharger->RegisterChargerStateChanged(gpCharger, &balance_this->gChargerStateChangeNotifier, _this->chargerType);
		//ASUS_BSP  --- Eason_Chang charger
		_this->mbInit = true;
	
		//Hank in rom mode show "?" : bootUp check Rom mode queue+++
#ifdef CONFIG_TI_GAUGE
		schedule_delayed_work(&balance_this->CheckGaugeRomModeWorker, 20*HZ);
#endif
		//Hank in rom mode show "?" : bootUp check Rom mode queue---
	}
}

//ASUS BSP Eason_Chang +++ batteryservice to fsm
static void BatteryServiceFsm_OnChangeChargingCurrent(struct AXI_Charging_FSM_Callback *callback,AXE_Charger_Type chargertype)
{
    AXC_BatteryService  *_this=
        container_of(callback, AXC_BatteryService, fsmCallback);

        pr_debug("[BAT][Ser]%s()\n",__FUNCTION__);
    
    _this->callback->changeChargingCurrent(_this->callback,chargertype);
}

static void BatteryServiceFsm_OnStateChanged(struct AXI_Charging_FSM_Callback *callback)
{   
    AXE_Charging_State GetStateFromFsm;
    bool NeedUpdate = 0;
    AXC_BatteryService  *_this=
        container_of(callback, AXC_BatteryService, fsmCallback);
     
    GetStateFromFsm = _this->fsm->getState(_this->fsm);
    

    switch(_this->fsmState){
        case DISCHARGING_STATE:
             if(GetStateFromFsm == CHARGING_STATE){
                NeedUpdate = 1;
             }
            break;
        case CHARGING_STATE:
#ifdef CONFIG_EEPROM_NUVOTON  //ASUS_BSP Eason_Chang 1120 porting +++            
             //ASUS_BSP +++ Eason_Chang BalanceMode
             if(1==AX_MicroP_IsP01Connected()){                   
                  if(    GetStateFromFsm == DISCHARGING_STATE
                       ||GetStateFromFsm == CHARGING_FULL_STATE
                       ||GetStateFromFsm == CHARGING_STOP_STATE){
                       NeedUpdate = 1;  
                  }     

             }else
             //ASUS_BSP --- Eason_Chang BalanceMode    
#endif //CONFIG_EEPROM_NUVOTON//ASUS_BSP Eason_Chang 1120 porting ---             
             if(GetStateFromFsm == DISCHARGING_STATE || GetStateFromFsm == CHARGING_FULL_STATE ){
                NeedUpdate = 1;
             }
            break;
        case CHARGING_STOP_STATE:
#ifdef CONFIG_EEPROM_NUVOTON  //ASUS_BSP Eason_Chang 1120 porting +++              
            //ASUS_BSP +++ Eason_Chang BalanceMode
            if(1==AX_MicroP_IsP01Connected()){
                 if(     GetStateFromFsm == DISCHARGING_STATE
                       ||GetStateFromFsm == CHARGING_STATE){
                       NeedUpdate = 1;
                }
            }else 
            //ASUS_BSP --- Eason_Chang BalanceMode  
#endif //CONFIG_EEPROM_NUVOTON//ASUS_BSP Eason_Chang 1120 porting ---             
            if(GetStateFromFsm == DISCHARGING_STATE ){
                NeedUpdate = 1;
            }   
            break;
        case CHARGING_FULL_STATE:
            if(GetStateFromFsm == DISCHARGING_STATE ){
                NeedUpdate = 1;
            } 
            break;
        case CHARGING_FULL_KEEP_STATE:
            if(GetStateFromFsm == DISCHARGING_STATE ){
                NeedUpdate = 1;
            }    
            break;
        case CHARGING_FULL_KEEP_STOP_STATE:
            if(GetStateFromFsm == DISCHARGING_STATE ){
                NeedUpdate = 1;
            }
            break;
        default:
            printk("[BAT][ser]%s():NOT mapping\n",__FUNCTION__);
            break;
            
    }

    _this->fsmState = GetStateFromFsm;

    if( 1 == NeedUpdate){
    _this->callback->onServiceStatusUpdated(_this->callback);
    }
    
}

//ASUS BSP Eason_Chang --- batteryservice to fsm
//ASUS BSP Eason_Chang +++ batteryservice to gauge
static inline void  AXC_BatteryService_scheduleNextPolling(struct AXC_BatteryService *_this)
{
    pr_debug("[Bat][SER]%s(): queue BatteryServiceUpdateWorker without calculate capacity\n",__func__);
    balance_this->NeedCalCap = false; 
    queue_delayed_work(_this->BatteryServiceCapUpdateQueue,
                            &_this->BatteryServiceUpdateWorker,
                            AXC_BatteryService_getNextPollingInterval(_this)* HZ);
}

static bool Get_CapRiseWhenCableIn(int nowCap, int lastCap)
{
	//Eason: Cap<10 with cable, but Cap decrease. Let Cap drop, ignore rule of lastCap - nowCap>=5 +++
	if( (lastCap - nowCap > 0)&&(balance_this->A66_capacity<10) )
	{
		printk("[BAT][SER]%s():CapDropWhenCableInCapLessThan10\n",__func__);
		return false;
	}	
	//Eason: Cap<10 with cable, but Cap decrease. Let Cap drop, ignore rule of lastCap - nowCap>=5 ---
	else if(lastCap - nowCap >= 5)
	{
		printk("[BAT][SER]%s():CapDropWhenCableIn\n",__func__);
		return false; 
	}
	else
	{
		return true;
	}
}

//ASUS_BSP Eason: fix PM8941 eoc_work change status full, but cap is 99% +++
/* When PM8941 eoc_work change status full, if don't notify BatteryService,
    query capacity with function AXC_BatteryService_getCapacity at this time will show 99% full
    because  flag balance_this->BatteryService_IsFULL does not change to true.
*/
void notify_batteryService_statusFull(void)
{
	if(false == balance_this->BatteryService_IsFULL)
	{
		printk("[BAT]%s\n",__FUNCTION__);
		balance_this->BatteryService_IsFULL = true;
	}
}
//ASUS_BSP Eason: fix PM8941 eoc_work change status full, but cap is 99% ---

static void DoAfterDecideFull(struct AXC_BatteryService *_this)
{
	if(false == _this->BatteryService_IsFULL){
		_this->BatteryService_IsFULL = true;
		_this->fsm->onChargingStop(_this->fsm,CHARGING_DONE);
		_this->gauge->notifyBatFullChanged(_this->gauge,true);
	}
}

static void DoAfterDecideNotFull(struct AXC_BatteryService *_this)
{
	if (true==_this->BatteryService_IsFULL){
		_this->gauge->notifyBatFullChanged(_this->gauge,false);
		_this->fsm->onFullDrop(_this->fsm);
	}

	_this->BatteryService_IsFULL = false;
}
static void DecideIsFull(struct AXC_BatteryService *_this,int nowGaugeCap,bool hasCableInPastTime)
{
	bool chgStatus;
#ifdef CONFIG_EEPROM_NUVOTON
	bool IsPadDock_ExtChg = false;
#endif
	int nCurrent = _this->callback->getIBAT(_this->callback);

	chgStatus = gpCharger->IsCharging(gpCharger);

	printk("[BAT][SER]%s():Charging Status:%d, Battery Current:%d\n",__func__,chgStatus,nCurrent);

	if(chgStatus){//charging
		if(!_this->isMainBatteryChargingDone){// if still charging....
			if(nowGaugeCap >= 94 && 0 >= nCurrent && -90 < nCurrent && !delayed_work_pending(&_this->BatEocWorker)){
				printk("[BAT][Ser]start eoc worker\n");
				queue_delayed_work(_this->BatteryServiceCapUpdateQueue, &_this->BatEocWorker,10 * HZ);
			}
		}
		else{
			chgStatus = false;    
		}
	}
	else{//no charging
		if( 0 >= nCurrent && -90 < nCurrent){
			chgStatus = false;
		}
		else if( 100 == balance_this->A66_capacity){
			chgStatus = false;
			printk("[BAT][Ser]:when cap100 don't judge current\n");
			//Eason : when resume by take off cable can be judge full 
		}
		else{
			chgStatus = true;
			printk("[BAT][Ser]:chg current not in 0~-90, can't judge Full\n");
		}
	}

#ifdef CONFIG_EEPROM_NUVOTON
	//Eason for resume by EXTchg off can be full+++
	if(100 == balance_this->A66_capacity){
		IsPadDock_ExtChg = true;
	}
	else{
		IsPadDock_ExtChg = DecideIfPadDockHaveExtChgAC();
	}
	//Eason for resume by EXTchg off can be full---
#endif

#ifdef CONFIG_EEPROM_NUVOTON  //ASUS_BSP Eason_Chang 1120 porting +++
	if(1==AX_MicroP_IsP01Connected()){
		if(CHARGING_FULL_STATE==balance_this->fsmState||CHARGING_FULL_KEEP_STATE==balance_this->fsmState
			|| CHARGING_FULL_KEEP_STOP_STATE==balance_this->fsmState)
		{
			DoAfterDecideFull(_this);  
		}
#ifdef CONFIG_PM_8226_CHARGER
		else if( true==pm8226_is_full() ){
			printk("[BAT][Ser]pm8226_is_full\n");
			//Eason : Wireless PMA spec Rx turn off Tx +++
			#ifdef CONFIG_IDTP9023_CHARGER
			g_WcEocTime = updateNowTime(_this);
			#endif
			//Eason : Wireless PMA spec Rx turn off Tx ---
			DoAfterDecideFull(_this);
		}
#endif
		else{
			DoAfterDecideNotFull(_this);
		}
	}
	else
#endif //CONFIG_EEPROM_NUVOTON//ASUS_BSP Eason_Chang 1120 porting ---    
	{    
		if(CHARGING_FULL_STATE==balance_this->fsmState || CHARGING_FULL_KEEP_STATE==balance_this->fsmState
			|| CHARGING_FULL_KEEP_STOP_STATE==balance_this->fsmState)
		{
			DoAfterDecideFull(_this);
		}
#ifdef CONFIG_PM_8226_CHARGER
		else if( true==pm8226_is_full() ){
			printk("[BAT][Ser]pm8226_is_full\n");
			//Eason : Wireless PMA spec Rx turn off Tx +++
#ifdef CONFIG_IDTP9023_CHARGER
			g_WcEocTime = updateNowTime(_this);
#endif
			//Eason : Wireless PMA spec Rx turn off Tx ---
			DoAfterDecideFull(_this);
		}
#endif
		else{
			DoAfterDecideNotFull(_this);
		}
	}
} 

bool report_BatteryService_If_HasCable(void)
{
	bool hasCable = false;

	if (true == balance_this->IsFirstAskCable)
	{
		if(1==getIfonline())
		{
			printk("[BAT][Ser]FirstAskCable report true\n");
			hasCable = true;
		}
	}else if (true == balance_this->IsResumeUpdate){
		hasCable = balance_this->HasCableBeforeSuspend;
	}else{
		hasCable = balance_this->BatteryService_IsCable; 
	}

    return hasCable;
}

//Eason: when change MaxMah clear interval+++
static time_t BatteryService_getForceIntervalSinceLastUpdate(AXC_BatteryService  *_this)
{
    struct timespec mtNow;
    
    time_t intervalSinceLastUpdate;
    
    mtNow = current_kernel_time();

	if(mtNow.tv_sec >= _this->ForceSavedTime){

		pr_debug("[BAT][Ser]%s:%ld,%ld\n",__FUNCTION__,mtNow.tv_sec,_this->ForceSavedTime);
	            
		intervalSinceLastUpdate = mtNow.tv_sec - _this->ForceSavedTime;

		if(intervalSinceLastUpdate > 2592000){
			printk("[BAT][SER]%s():wrongInt %ld \n",__func__,intervalSinceLastUpdate);
			intervalSinceLastUpdate = 180;
		}
	}
	else{
		printk("[BAT][Ser]%s:OVERFLOW....%ld,%ld\n",__FUNCTION__,mtNow.tv_sec,_this->ForceSavedTime);              
		//todo: to do the correct calculation here....
		intervalSinceLastUpdate = 180;
	}
	return intervalSinceLastUpdate ; 
}
//Eason: when change MaxMah clear interval---

//ASUS_BSP Eason:when shutdown device set smb346 charger to DisOTG mode +++
extern void UsbSetOtgSwitch(bool switchOtg);
static void set_DisOTGmode_whenCap_0(void)
{
	printk("[BAT]Cap 0 DisOTG+++\n ");
	UsbSetOtgSwitch(false);
	printk("[BAT]Cap 0 DisOTG---\n ");
}
//ASUS_BSP Eason:when shutdown device set smb346 charger to DisOTG mode ---

//ASUS BSP Eason_Chang get Cap from TIgauge+++
int get_Cap_from_TIgauge(void);
//ASUS BSP Eason_Chang get Cap from TIgauge---


//Eason show temp limit +++
extern int showSmb346TempLimitReason(void);
//Eason show temp limit ---
//Eason show AICLsetting & AICLresult+++
extern int showSmb346AICL_Setting(void);
extern int showSmb346AICL_Result(void);
//Eason show AICLsetting & AICLresult---
//check df version do Cap remapping+++
extern int g_gauge_df_version;
#define DF_VERSION_NOT_NEED_REMAP  0
#define DF_VERSION_REMAP_START        1
#define DF_VERSION_REMAP_STOP         10
#define NO_RESERVE_DF_MAP_NUM	100
//check df version do Cap remapping---

//ASUS_BSP Eason read PM8226 register value+++
extern int pm8226_qpnp_chg_read_register(u16 addr);
//ASUS_BSP Eason read PM8226 register value---
extern int get_Volt_from_TIgauge(void);
extern int get_FCC_from_TIgauge(void);
static void AXC_BatteryService_reportPropertyCapacity(struct AXC_BatteryService *_this, int refcapacity)
{
	int A66_capacity;
    
	//int EC_capacity;

#ifdef CONFIG_EEPROM_NUVOTON
	int IsBalTest = 0;//ASUS_BSP Eason_Chang BalanceMode
#endif

	int lastCapacity;
	int maxMah;
	int pmicTemp;
	//ASUS_BSP Eason read PM8226 register value+++
	u32 pm8226_0x105B_value;
	u32 pm8226_0x1040_value;
	u32 pm8226_0x1054_value;
	u32 pm8226_0x1049_value;
	u32 pm8226_0x1344_value;
	u32 pm8226_0x1010_value;
	u32 pm8226_0x1210_value;
	u32 pm8226_0x1044_value;
	u32 pm8226_0x105D_value;
	//ASUS_BSP Eason read PM8226 register value---

	bool hasCable = false;
	bool EnableBATLifeRise;
	bool maxMahDrop = false;
	//Eason boot up in BatLow situation, take off cable can shutdown+++
	bool IsBatLowtoFilter ;
	//Eason boot up in BatLow situation, take off cable can shutdown---

	time_t intervalSinceLastUpdate;

	mutex_lock(&_this->filter_lock);

	intervalSinceLastUpdate  = BatteryService_getIntervalSinceLastUpdate(_this);
    
	//We need do ask capcaity to filter at first time, in case there is FULL orBATLow+++ 
	if(true == _this->IsFirstAskCap){
		lastCapacity = refcapacity;
		_this->IsFirstAskCap = false;
	}
	else{
		lastCapacity = g_SWgauge_lastCapacity;
	}
	//We need do ask capcaity to filter at first time, in case there is FULL orBATLow---

	//Initiatively check cable when power on+++
	if (true == _this->IsFirstAskCable)
	{
		if(1==getIfonline())
		{
			printk("[BAT][Ser]FirstAskCable true\n");
			hasCable = true;
		}
		_this->IsResumeUpdate = false;
		_this->IsFirstAskCable = false;
	}
	//Initiatively check cable when power on---
	//If resume update capacity, check cable before suspend+++
	else if (true == _this->IsResumeUpdate){
		hasCable = _this->HasCableBeforeSuspend;
		_this->IsResumeUpdate = false;
	}
	//If resume update capacity, check cable before suspend---
	//Default check cable +++
	else{
		hasCable = _this->BatteryService_IsCable; 
	}
	//Default check cable ---
      
	_this->BatteryService_IsCharging = gpCharger->IsCharging(gpCharger);

	DecideIsFull(_this,refcapacity,hasCable);

	//Eason: BAT Cap can drop when cable in +++ 
	if( true == hasCable )//A66 has cable  
	{
#ifdef CONFIG_EEPROM_NUVOTON  //ASUS_BSP Eason_Chang 1120 porting +++    
		if(1==AX_MicroP_IsP01Connected()){//cable of A66 is Pad
			if( true == _this->BatteryService_IsCharging){//A66 is charging 
				EnableBATLifeRise = Get_CapRiseWhenCableIn(refcapacity, lastCapacity);
				if( false == EnableBATLifeRise){
					maxMahDrop = true;
				}
			}
			else if(true == _this->BatteryService_IsFULL){//A66 is not charging but full
				//Eason for resume by EXTchg off can be full, after full can drop
				EnableBATLifeRise = Get_CapRiseWhenCableIn(refcapacity, lastCapacity);
				if( false == EnableBATLifeRise){
					maxMahDrop = true;
				}
			}
			else if(true == DecideIfPadDockHaveExtChgAC()){//A66 have ext chg 
				EnableBATLifeRise = Get_CapRiseWhenCableIn(refcapacity, lastCapacity);
				if( false == EnableBATLifeRise){
					maxMahDrop = true;
				}
			}
			else{//A66 is neither charging  nor full
				EnableBATLifeRise = false;
			}
		}
		else//cable of A66 is not Pad
#endif //CONFIG_EEPROM_NUVOTON//ASUS_BSP Eason_Chang 1120 porting ---            
		{
			EnableBATLifeRise = Get_CapRiseWhenCableIn(refcapacity, lastCapacity);
			if( false == EnableBATLifeRise){
				maxMahDrop = true;
			}
		}      
	}
	else{//A66 doesn't has cable 
		EnableBATLifeRise = hasCable;
	}

	maxMah = BatteryService_ChooseMaxMah(_this,maxMahDrop);
	//Eason : if last time is 10mA +++
	if(true==IfUpdateSavedTime)//only do when last time is 10mA
	{
		intervalSinceLastUpdate = BatteryService_getForceIntervalSinceLastUpdate(_this);//Eason: when change MaxMah clear interval
		IfUpdateSavedTime = false;
	}
	//Eason : if last time is 10mA ---	
//Eason: BAT Cap can drop when cable in --- 

//Eason boot up in BatLow situation, take off cable can shutdown+++
	if(true==g_BootUp_IsBatLow )
	{
		if(1==getIfonline())
		{
			IsBatLowtoFilter = false;	
			printk("[BAT][BootUp]BootUp IsBatLow, Cable on, BatLow false\n");
		}
		else{
			IsBatLowtoFilter = g_BootUp_IsBatLow;
			printk("[BAT][BootUp]BootUp IsBatLow, Cable off, BatLow true\n");
		}
	}
	else{
		IsBatLowtoFilter = _this->BatteryService_IsBatLow;
	}
//Eason boot up in BatLow situation, take off cable can shutdown---    

	g_current_now = get_current_for_ASUSswgauge();

	A66_capacity = _this->gpCapFilterA66->filterCapacity
									(_this->gpCapFilterA66,
										refcapacity, 
										lastCapacity,
										EnableBATLifeRise,
										_this->BatteryService_IsCharging,
										_this->BatteryService_IsFULL,
										IsBatLowtoFilter,
										maxMah,
										intervalSinceLastUpdate);

    printk("[BAT][Ser]report Capacity:%d,%d,%d,%d,%d,%d,%d,%d,%ld==>%d\n",
                                    refcapacity,
                                    lastCapacity,
                                      hasCable,
                                      EnableBATLifeRise,
                                      _this->BatteryService_IsCharging,
                                      _this->BatteryService_IsFULL,
                                      IsBatLowtoFilter,
                                      maxMah,
                                      intervalSinceLastUpdate,
                                      A66_capacity);
     
	//ASUS_BSP +++ Eason_Chang add event log +++
	//ASUS_BSP Eason read PM8226 register value+++
	pm8226_0x105B_value = pm8226_qpnp_chg_read_register(0x105B);
	pm8226_0x1040_value = pm8226_qpnp_chg_read_register(0x1040);
	pm8226_0x1054_value = pm8226_qpnp_chg_read_register(0x1054);
	pm8226_0x1049_value = pm8226_qpnp_chg_read_register(0x1049);
	pm8226_0x1344_value = pm8226_qpnp_chg_read_register(0x1344);
	pm8226_0x1010_value = pm8226_qpnp_chg_read_register(0x1010);
	pm8226_0x1210_value = pm8226_qpnp_chg_read_register(0x1210);
	pm8226_0x1044_value = pm8226_qpnp_chg_read_register(0x1044);
	pm8226_0x105D_value = pm8226_qpnp_chg_read_register(0x105D);
	//ASUS_BSP Eason read PM8226 register value---		
	pmicTemp = pm8226_get_prop_batt_temp();

	ASUSEvtlog("[BAT][Ser]report Capacity:%d,%d,%d,%d,%d,%d,%d,%d,%ld==>%d  ,BMS:%d, Cur:%d, Temp:%d, 0x105B:0x%x, 0x1040:0x%x, 0x1054:0x%x, 0x1049:0x%x, 0x1344:0x%x, 0x1010:0x%x, 0x1210:0x%x, 0x1044:0x%x, 0x105D:0x%x\n",
                                    refcapacity,
                                    lastCapacity,
                                      hasCable,
                                      EnableBATLifeRise,
                                      _this->BatteryService_IsCharging,
                                      _this->BatteryService_IsFULL,
                                      IsBatLowtoFilter,
                                      maxMah,
                                      intervalSinceLastUpdate,
                                      A66_capacity,
                                      gBMS_Cap,
                                      g_current_now,
                                      pmicTemp,
                                      pm8226_0x105B_value,
                                      pm8226_0x1040_value,
                                      pm8226_0x1054_value,
                                      pm8226_0x1049_value,
                                      pm8226_0x1344_value,
                                      pm8226_0x1010_value,
                                      pm8226_0x1210_value,
                                      pm8226_0x1044_value,
                                      pm8226_0x105D_value);

	//ASUS_BSP --- Eason_Chang add event log ---   

	//Eason: remember last BMS Cap to filter+++
	//	last_BMS_Cap = gBMS_Cap;
	//Eason: remember last BMS Cap to filter---

#ifdef CONFIG_EEPROM_NUVOTON
//ASUS_BSP +++ Eason_Chang BalanceMode : set A66_cap for cmd test 
	IsBalTest = IsBalanceTest();
	if( 1 == IsBalTest){
		A66_capacity = GetBalanceModeA66CAP();
		printk("[BAT][Bal][test]A66 cap: %d\n",A66_capacity );
	}
//ASUS_BSP --- Eason_Chang BalanceMode : set A66_cap for cmd test 
#endif

	if(A66_capacity < 0 || A66_capacity >100){
		printk("[BAT][Ser]Filter return value fail!!!\n");
	}
	else{
		//Eason show BMS cap to user+++
#ifdef CONFIG_EEPROM_NUVOTON
		if( 1 == IsBalTest ){
			_this->A66_capacity = A66_capacity;
		}
		else
#endif
		{
#if USE_SW_GAUGE
			_this->A66_capacity = A66_capacity;
#else
			_this->A66_capacity = gBMS_Cap;
#endif
			g_SWgauge_lastCapacity = A66_capacity;
		}
		//Eason show BMS cap to user---

	
#ifdef CONFIG_EEPROM_NUVOTON  //ASUS_BSP Eason_Chang 1120 porting +++
		if(1==AX_MicroP_IsP01Connected()){
			if( false == DecideIfPadDockHaveExtChgAC()){ 
				BatteryServiceDoBalance(_this);
			}
			else{
				Init_Microp_Vbus__Chg();
			}

			//Eason: dynamic set Pad alarm +++
#ifndef ASUS_FACTORY_BUILD	  	
			SetRTCAlarm();
#endif
			//Eason: dynamic set Pad alarm ----
		}
#endif //CONFIG_EEPROM_NUVOTON   //ASUS_BSP Eason_Chang 1120 porting ---

		// when A66 Cap = 0% shutdown device no matter if has cable+++ 
		if( 0==_this->A66_capacity )//Eason show BMS cap to user
		{
			g_AcUsbOnline_Change0 = true;
			//ASUS_BSP Eason:when shutdown device set smb346 charger to DisOTG mode +++
			set_DisOTGmode_whenCap_0();
			//ASUS_BSP Eason:when shutdown device set smb346 charger to DisOTG mode ---

			//ASUS_BSP +++ Peter_lu "suspend for Battery0% in  fastboot mode issue"
#ifdef CONFIG_FASTBOOT
			if(is_fastboot_enable()){
				printk("[BAT][Fastboot]kernel_power_off\n");
				kernel_power_off();
			}
			else{
#endif // CONFIG_FASTBOOT
			//ASUS_BSP --- Peter_lu "suspend for Battery0% in  fastboot mode issue"

			AcUsbPowerSupplyChange();
#ifdef CONFIG_EEPROM_NUVOTON
			Pad_AC_PowerSupplyChange();
#endif

			//ASUS_BSP +++ Peter_lu "suspend for Battery0% in  fastboot mode issue"
#ifdef CONFIG_FASTBOOT
			}
#endif
			//ASUS_BSP --- Peter_lu
		} //end if ( 0==_this->A66_capacity )
		// when A66 Cap = 0% shutdown device no matter if has cable---

		//Eason : prevent thermal too hot, limit charging current in phone call+++
		judgePhoneOnCurLimit();
		judgeThermalCurrentLimit(); //when thermal too hot limit charging current
		//Eason : prevent thermal too hot, limit charging current in phone call---

		//Eason : Wireless PMA spec Rx turn off Tx +++
#ifdef CONFIG_IDTP9023_CHARGER
		judgeWirelessCoilTempTurnOffTx();
		judgeWirelessEocTurnOffTx();
#endif
		//Eason : Wireless PMA spec Rx turn off Tx ---
		
		//Eason: LowCapCpuThrottle +++
		judgeCpuThrottleByCap();
		//Eason: LowCapCpuThrottle ---

#ifdef ASUS_FACTORY_BUILD
		if(1==getIfonline() )
		{
			//Eason: take off 5060rule let A80 can always charge+++
			if(true == charger_limit_enable)
				Do_Factory5060Mode();
			//Eason: take off 5060rule let A80 can always charge---
		}
#endif//#ifdef ASUS_FACTORY_BUILD

		_this->callback->onServiceStatusUpdated(_this->callback);
	}   

	_this->IsCalculateCapOngoing = false;

	//Eason: AICL work around +++
	g_alreadyCalFirstCap = true;
	//Eason: AICL work around ---	

	mutex_unlock(&_this->filter_lock);

}
static int BatteryServiceGauge_OnCapacityReply(struct AXI_Gauge *gauge, struct AXI_Gauge_Callback *gaugeCb, int batCap, int result)
{   

    AXC_BatteryService  *_this=
        container_of(gaugeCb, AXC_BatteryService, gaugeCallback);

    //ASUS BSP: Eason check correct BMS RUC+++
    int BMSCap;
    //int BMS_diff_SWgauge;
    //ASUS BSP: Eason check correct BMS RUC---

    //Eason : In suspend have same cap don't update savedTime +++
    int A66_LastTime_capacity;
    A66_LastTime_capacity = _this->A66_capacity;
    //Eason : In suspend have same cap don't update savedTime ---

    pr_debug("[BAT][SER]%s() +++ \n",__func__);		
    mutex_lock(&_this->main_lock);

    //Eason get BMS Capacity for EventLog+++
    BMSCap= get_BMS_capacity();
    gBMS_Cap =  BMSCap;
    //Eason get BMS Capacity for EventLog---

	if((BAT_CAP_REPLY_ERR==result))
	{
		_this->IsResumeUpdate = false;
		_this->IsResumeMahUpdate = false;
		pr_err("[A66][BAT][Ser]:Error askCapacity\n");
	}
	else{
		//Eason: choose Capacity type SWGauge/BMS +++ 
		if(g_CapType == DEFAULT_CAP_TYPE_VALUE){
			pr_debug("[BAT][Ser]:Cap type SWgauge\n");	
			AXC_BatteryService_reportPropertyCapacity(_this,batCap);
		}
		else{
#if 0
        	//Eason get BMS Capacity for EventLog+++
			printk("[BAT][Ser]:Cap type BMS:%d,%d\n",BMSCap,batCap);	
			//Eason get BMS Capacity for EventLog---
			//ASUS BSP: Eason check correct BMS RUC+++
			BMS_diff_SWgauge = BMSCap-batCap;
			if(BMS_diff_SWgauge<0)
				BMS_diff_SWgauge = -BMS_diff_SWgauge;
			
			if(BMS_diff_SWgauge<=5)
				gIsBMSerror = false;
			//ASUS BSP: Eason check correct BMS RUC---

			//ASUS BSP: Eason check correct BMS RUC+++
			if(true==gIsBMSerror){
				  	AXC_BatteryService_reportPropertyCapacity(_this,batCap);
					printk("[BAT][BMS]:Error BMS need to use SWgauge capacity\n");
					ASUSEvtlog("[BAT]:SWgauge\n");
			}else
			//ASUS BSP: Eason check correct BMS RUC---
			if((true==g_BootUp_IsBatLow)||(true==_this->BatteryService_IsBatLow))
			{
			      if(batCap<BMSCap){
				  	AXC_BatteryService_reportPropertyCapacity(_this,batCap);
					printk("[BAT][BMS][low]:use lower SWgauge capacity\n");
					ASUSEvtlog("[BAT]:SWgauge low\n");
			      	}else{
			      		AXC_BatteryService_reportPropertyCapacity(_this,BMSCap);
					ASUSEvtlog("[BAT]:BMS low\n");
		      		}
			}
			else
			{
		   		AXC_BatteryService_reportPropertyCapacity(_this,BMSCap);
				ASUSEvtlog("[BAT]:BMS\n");
			}
#else
			AXC_BatteryService_reportPropertyCapacity(_this,gBMS_Cap);
#endif
		}	
		//Eason: choose Capacity type SWGauge/BMS ---	
		
		//Eason : In suspend have same cap don't update savedTime +++
		if( (A66_LastTime_capacity == _this->A66_capacity)&&(true==SameCapDontUpdateSavedTime)&&(false==g_RTC_update) ){
			printk("[BAT][Ser]:In suspend have same Cap dont update savedTime\n");
		}
		else{
			g_RTC_update = false;
			_this->savedTime=updateNowTime(_this);
			//Eason:fix Cap drop too slowly in unattended mode+++
			filRealSusT = 0;
			//Eason:fix Cap drop too slowly in unattended mode---
		}
		//Eason : In suspend have same cap don't update savedTime ---

		//Eason: when change MaxMah clear interval+++
		_this->ForceSavedTime = updateNowTime(_this);//for A68 will always update no matter if change MaxMAh
		//Eason: when change MaxMah clear interval---
	}

	//Hank: schedule next polling in capacity update worker+++ 
	//AXC_BatteryService_scheduleNextPolling(_this);
	//Hank: schedule next polling in capacity update worker---

	//Hank: move unlock to BatteryServiceCapSample+++
	//wake_unlock(&_this->cap_wake_lock);
	//Hank: move unlock to BatteryServiceCapSample---

	mutex_unlock(&_this->main_lock);
	pr_debug("[BAT][SER]%s() --- \n",__func__);
	return 0;
}
int BatteryServiceGauge_AskSuspendCharging(struct AXI_Gauge_Callback *gaugeCb)
{
    AXC_BatteryService  *_this=
         container_of(gaugeCb, AXC_BatteryService, gaugeCallback);
    _this->callback->changeChargingCurrent(_this->callback,NO_CHARGER_TYPE);
    //gpCharger->EnableCharging(gpCharger,false);//stop curr may need delay
    return 0;
}
int BatteryServiceGauge_AskResumeCharging(struct AXI_Gauge_Callback *gaugeCb)
{
    AXC_BatteryService  *_this=
         container_of(gaugeCb, AXC_BatteryService, gaugeCallback);
    _this->callback->changeChargingCurrent(_this->callback,_this->chargerType);
    //gpCharger->EnableCharging(gpCharger,true);//stop curr may need delay
    return 0;
}

#ifdef CONFIG_EEPROM_NUVOTON
static int Report_P02Cable(void)
{
#ifdef CONFIG_EEPROM_NUVOTON  //ASUS_BSP Eason_Chang 1120 porting +++
	if( (1==AX_MicroP_get_USBDetectStatus(Batt_P01))|| (2==AX_MicroP_get_USBDetectStatus(Batt_P01)) ){//Eason: Pad plug usb show icon & cap can increase
		balance_this->P02_IsCable = true;
	}else{
		balance_this->P02_IsCable = false;
	}
#endif //CONFIG_EEPROM_NUVOTON//ASUS_BSP Eason_Chang 1120 porting ---    

    return balance_this->P02_IsCable;
}
static void Report_P02ChgStatus(int P02Chg) 
{
    balance_this->P02_IsCharging = false;
    balance_this->P02_IsFULL = false; 

    if(1==P02Chg){
            balance_this->P02_IsCharging = true;
    }else if(2==P02Chg){
            balance_this->P02_IsFULL = true; 
    }       
}
static void P02_reportPropertyCapacity(struct AXC_BatteryService *_this, int P02_refcapacity)
{

    int Pad_capacity;
    int IsBalTest;//ASUS_BSP Eason_Chang BalanceMode

    int lastCapacity;

    int P02_maxMah;

    bool P02_hasCable;
    int  P02_chgStatus=0;//ASUS_BSP Eason_Chang 1120 porting

    time_t P02_intervalSinceLastUpdate;

    mutex_lock(&_this->filter_lock);

    P02_intervalSinceLastUpdate  = P02_BatteryService_getIntervalSinceLastUpdate(_this);
    

    //We need do ask capcaity to filter at first time, in case there is FULL orBATLow 
    if(true == _this->P02_IsFirstAskCap){

        lastCapacity = P02_refcapacity;

        _this->P02_IsFirstAskCap = false;
        
    }else{

        lastCapacity = _this->Pad_capacity;

    }

    if (true == _this->P02_IsResumeUpdate){
        P02_hasCable = _this->P02_HasCableBeforeSuspend;
        //P02_chgStatus = _this->P02_ChgStatusBeforeSuspend;
        P02_maxMah = _this->P02_MaxMahBeforeSuspend;
        _this->P02_IsResumeUpdate = false;
 
    }else{
        P02_hasCable = Report_P02Cable();
        //P02_chgStatus = AX_MicroP_get_ChargingStatus(Batt_P01);
        P02_maxMah = P02_ChooseMaxMah();
    }

#ifdef CONFIG_EEPROM_NUVOTON  //ASUS_BSP Eason_Chang 1120 porting +++
    P02_chgStatus = AX_MicroP_get_ChargingStatus(Batt_P01);
#endif //CONFIG_EEPROM_NUVOTON//ASUS_BSP Eason_Chang 1120 porting ---
    Report_P02ChgStatus(P02_chgStatus);

    Pad_capacity = _this->gpCapFilterP02->filterCapacity
                                     (_this->gpCapFilterP02,
                                      P02_refcapacity, lastCapacity,
                                      P02_hasCable,
                                      _this->P02_IsCharging,
                                      _this->P02_IsFULL,
                                      _this->P02_IsBatLow,
                                      P02_maxMah,
                                      P02_intervalSinceLastUpdate);

    pr_debug("[BAT][Ser][P02]report Capacity:%d,%d,%d,%d,%d,%d,%d,%ld==>%d\n",
                                    P02_refcapacity,
                                    lastCapacity,
                                      P02_hasCable,
                                      _this->P02_IsCharging,
                                      _this->P02_IsFULL,
                                      _this->P02_IsBatLow,
                                      P02_maxMah,
                                      P02_intervalSinceLastUpdate,
                                      Pad_capacity);
    //ASUS_BSP Eason_Chang add event log +++

	ASUSEvtlog("[BAT][Ser][P02]report Capacity:%d,%d,%d,%d,%d,%d,%d,%ld==>%d\n",
                                    P02_refcapacity,
                                    lastCapacity,
                                      P02_hasCable,
                                      _this->P02_IsCharging,
                                      _this->P02_IsFULL,
                                      _this->P02_IsBatLow,
                                      P02_maxMah,
                                      P02_intervalSinceLastUpdate,
                                      Pad_capacity);

    //ASUS_BSP Eason_Chang add event log ---

//ASUS_BSP +++ Eason_Chang  : set Pad_cap for cmd test
    IsBalTest = IsBalanceTest();
    if( 1 == IsBalTest){
            Pad_capacity = BatteryServiceGetPADCAP();
    }
//ASUS_BSP --- Eason_Chang  : set Pad_cap for cmd test 


    if(Pad_capacity < 0 || Pad_capacity >100){

        printk("[BAT][Ser]Filter return value fail!!!\n");
    }else if(_this->Pad_capacity == Pad_capacity){    
       pr_debug("[BAT][Ser]Pad have same cap:%d\n",Pad_capacity);
    }else if(_this->Pad_capacity != Pad_capacity){
       _this->Pad_capacity = Pad_capacity;
       
       BatteryService_P02update();
    }   
    
    //wake_unlock(&_this->cap_wake_lock);

    mutex_unlock(&_this->filter_lock);

}
static int P02Gauge_OnCapacityReply(struct AXI_Gauge *gauge, struct AXI_Gauge_Callback *gaugeCb, int batCap, int result)
{   
	AXC_BatteryService  *_this=
		container_of(gaugeCb, AXC_BatteryService, P02gaugeCallback);

	mutex_lock(&_this->main_lock);

	P02_reportPropertyCapacity(_this,batCap);

	_this->P02_savedTime=updateNowTime(_this);
	pr_debug("[BAT][Ser]:P02Gauge_OnCapacityReply\n");
	//ReportTime();

	mutex_unlock(&_this->main_lock);

	_this->gauge->askCapacity(_this->gauge);

	pr_debug("[BAT][Ser]:P02Gauge_askCapacity\n");
	//ReportTime();
	return 0;
}
int P02Gauge_AskSuspendCharging(struct AXI_Gauge_Callback *gaugeCb)
{ 
    return 0;
}
int P02Gauge_AskResumeCharging(struct AXI_Gauge_Callback *gaugeCb)
{
    return 0;
}
#endif //CONFIG_EEPROM_NUVOTON
//ASUS BSP Eason_Chang --- batteryservice to gauge

//static int BatteryService_CalculateBATCAP(AXC_BatteryService *_this)
//{             
//    return _this->gauge->GetBatteryLife(_this->gauge);
//}
static bool BatteryService_ifFixedPollingInterval(struct AXC_BatteryServiceTest *test)
{
    return (-1 != test->pollingInterval);
}
static bool BatteryService_ifFixedFilterLastUpdateInterval(struct AXC_BatteryServiceTest *test)
{
    return (-1 != test->filterLastUpdateInterval);

}
static void BatteryService_changePollingInterval(struct AXC_BatteryServiceTest *test,bool fixed,int interval)
{
    AXC_BatteryService  *_this=
        container_of(test, AXC_BatteryService, test);

    if(fixed){
        printk("%s:fix interval to %d\n",__FUNCTION__,interval);

        test->pollingInterval = interval;

        _this->miParent.suspend(&_this->miParent);

        _this->miParent.resume(&_this->miParent, interval);

    }else{
        printk("%s:don't fix interval\n",__FUNCTION__);

        test->pollingInterval = -1;

    }
}
static void BatteryService_changeFilterLastUpdateInterval(struct AXC_BatteryServiceTest *test,bool fixed,int interval)
{
    if(fixed){

        printk("%s:fix interval to %d\n",__FUNCTION__,interval);

        test->filterLastUpdateInterval = interval;

    }else{
        printk("%s:don't fix interval\n",__FUNCTION__);

        test->filterLastUpdateInterval = -1;
    }
}
static AXC_BatteryService g_AXC_BatteryService={
    .miParent = {
        .getChargingStatus = AXC_BatteryService_getChargingStatus,
        .getCapacity = AXC_BatteryService_getCapacity,
        .onCableInOut =AXC_BatteryService_onCableInOut,
        .onChargingStop =AXC_BatteryService_onChargingStop,
        .onChargingStart = AXC_BatteryService_onChargingStart,
        .onBatteryLowAlarm= AXC_BatteryService_onBatteryLowAlarm,
        .onBatteryRemoved = AXC_BatteryService_onBatteryRemoved,
        .suspend = AXC_BatteryService_suspend,
        .resume =AXC_BatteryService_resume,
        .forceResume = AXC_BatteryService_forceResume,
    },
    //Hank: temperature monitor+++
    .NeedCalCap = false,
    .NeedKeep5Min  = false,
    .TempLimit = 0,
    //Hank: temperature monitor---
    .mbInit = false,
    .IsFirstForceResume = true,
    .callback = NULL,
    .A66_capacity = 50,//saved capacity //Hank: default capacity 50 prevent default full
    .Pad_capacity = 100,
    .ForceSavedTime = 0,//for A68 will always update no matter if change MaxMAh
    .savedTime = 0,//for A68 may dont update if change 10==MaxMAh
    .P02_savedTime = 0,
    .BatteryService_IsCable = false,
    .BatteryService_IsCharging = false,
    .BatteryService_IsFULL = false,
    .BatteryService_IsBatLow = false,
    .isMainBatteryChargingDone= false,
    .IsFirstAskCap = true,
    .IsFirstAskCable = true,
    .HasCableBeforeSuspend = false,
    .IsResumeUpdate = false,
    .IsResumeMahUpdate = false,
    .IsCalculateCapOngoing = false,
    .P02_IsCable = false,
    .P02_IsCharging = false,
    .P02_IsFULL = false,
    .P02_IsBatLow = false,
    .P02_IsFirstAskCap = true,
    .P02_HasCableBeforeSuspend = false,
    .P02_IsResumeUpdate = false,
    //.P02_ChgStatusBeforeSuspend = 0,
    .P02_MaxMahBeforeSuspend = 0,
    .gaugeCallback ={
        .onCapacityReply = BatteryServiceGauge_OnCapacityReply,
        .askSuspendCharging = BatteryServiceGauge_AskSuspendCharging,   
        .askResumeCharging = BatteryServiceGauge_AskResumeCharging,
        },// batteryservice to gauge
#ifdef CONFIG_EEPROM_NUVOTON
    .P02gaugeCallback ={
        .onCapacityReply = P02Gauge_OnCapacityReply,
        .askSuspendCharging = P02Gauge_AskSuspendCharging,   
        .askResumeCharging = P02Gauge_AskResumeCharging,
        },// batteryservice to gauge 
#endif
    .chargerType =  NOTDEFINE_TYPE ,  // batteryservice to gauge
    .gauge = NULL,  // batteryservice to gauge
    .P02gauge = NULL,
    .gpCapFilterA66 = NULL,
    .gpCapFilterP02 = NULL,
    .defaultPollingInterval = DEFAULT_ASUSBAT_POLLING_INTERVAL , // batteryservice to gauge
    .fsmCallback ={
        .onChangeChargingCurrent = BatteryServiceFsm_OnChangeChargingCurrent,
        .onStateChanged = BatteryServiceFsm_OnStateChanged,
        },// batteryservice to fsm
    .fsm = NULL,                 // batteryservice to fsm
    .fsmState = NOTDEFINE_STATE ,// batteryservice to fsm
    .test = {
        .pollingInterval = -1,
        .filterLastUpdateInterval = -1,
        .ifFixedPollingInterval = BatteryService_ifFixedPollingInterval,
        .ifFixedFilterLastUpdateInterval =BatteryService_ifFixedFilterLastUpdateInterval,
        .changePollingInterval=BatteryService_changePollingInterval,
        .changeFilterLastUpdateInterval= BatteryService_changeFilterLastUpdateInterval,
    },
    .gChargerStateChangeNotifier={
        .Notify = NotifyForChargerStateChanged,
        .onChargingStart = onChargingStart,
        },
};

AXI_BatteryServiceFacade *getBatteryService(AXI_BatteryServiceFacadeCallback *callback)
{
    static AXI_BatteryServiceFacade *lpBatteryService = NULL;

    if(NULL == lpBatteryService){

        lpBatteryService = &g_AXC_BatteryService.miParent;

        AXC_BatteryService_constructor(&g_AXC_BatteryService, callback);
    }

    return lpBatteryService;
}

AXC_BatteryServiceTest *getBatteryServiceTest(void)
{
    return &g_AXC_BatteryService.test;
}

#ifdef CONFIG_EEPROM_NUVOTON
int getPowerBankCharge(void)
{
    return IsPowerBankCharge;
}

int getBalanceCharge(void)
{
    return IsBalanceCharge;
}
#endif
#endif //#ifdef CONFIG_BATTERY_ASUS_SERVICE








