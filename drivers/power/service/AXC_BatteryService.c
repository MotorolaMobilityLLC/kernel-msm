#ifdef CONFIG_BATTERY_ASUS_SERVICE
#include "AXC_BatteryService.h"
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
#include <linux/time.h>
//ASUS BSP Eason_Chang --- batteryservice to gauge
//ASUS_BSP +++ Eason_Chang BalanceMode

#include <linux/asus_chg.h>
#include <linux/notifier.h>

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

extern bool reportRtcReady(void);

#define SUSPEND_DISCHG_CURRENT 10
#define MAX_DISCHG_CURRENT    300
#define USB_CHG_CURRENT       500
#define ROBIN_MAX_CHG_CURRENT	200
#define USB3p0_ILLEGAL_CURRENT		900
#ifdef CONFIG_IDTP9023_CHARGER  
#define WIRELESS_CHG_CURRENT        700
#endif

#define AC_CHG_CURRENT        900
#define AC_SUSPEND_CHG_CURRENT 1000

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
#define RTCSetIntervalwhenBATlow  310
#define RTCSetIntervalwhenCABLEIn  1810
#define CapChangeRTCInterval 20
//Eason set alarm ---

// when A66 Cap = 0% shutdown device no matter if has cable+++ 
extern bool g_AcUsbOnline_Change0;
extern void AcUsbPowerSupplyChange(void);
// when A66 Cap = 0% shutdown device no matter if has cable---
//Eason boot up in BatLow situation, take off cable can shutdown+++
extern bool g_BootUp_IsBatLow;
//Eason boot up in BatLow situation, take off cable can shutdown---
//Eason : prevent thermal too hot, limit charging current in phone call+++
extern bool g_audio_limit;
static bool IsPhoneOn = false;
//Eason : prevent thermal too hot, limit charging current in phone call---
//Eason : when thermal too hot, limit charging current +++ 
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
int g_CapType = 1;// 0:SWgauge 1:BMS
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
#include "../charger/axc_Smb346Charger.h"
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

bool g_already_read_bat_cap = false;
static void read_bat_cap_work(struct work_struct *work);
#if defined(ASUS_CHARGING_MODE) && !defined(ASUS_FACTORY_BUILD)
extern int g_asus_CHG_mode;
#endif

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
		pr_debug("[BAT]%s\n",__FUNCTION__);
		
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
	        pr_debug("[BAT][SER][Thermal]:judge g_thermal_limit true\n");    
    }
    else
    {
	        g_thermal_limit = 0;
	        pr_debug("[BAT][SER][Thermal]:judge g_thermal_limit false\n");

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

//ASUS BSP Eason_Chang +++ batteryservice to fsm
static void AXC_BatteryService_reportPropertyCapacity(struct AXC_BatteryService *_this, int refcapacity);

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
}
//ASUS BSP Eason_Chang --- batteryservice to gauge

static void BatteryService_enable_Filter(AXC_BatteryService *_this)
{
	if(NULL == _this->gpCapFilterA66){
		AXC_Cap_Filter_Get(E_CAP_FILTER_PHONE_A66, &_this->gpCapFilterA66, 369); //FCC
	}
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
//Eason: A68 new balance mode ---

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
	}else if(balance_this->A66_capacity <= 60){   
		g_5060modeCharging = true;
		gpCharger->EnableCharging(gpCharger,true);
		DisChg = false;
		balance_this->fsm->onChargingStart(balance_this->fsm);
	}
	printk("[BAT][Factory]:DoFactory5060Mode---\n");
   
}
#endif//#ifdef ASUS_FACTORY_BUILD

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

    printk("[BAT][alarm][cableIn]:%ld.%ld\n",mtNow.tv_sec,mtNow.tv_nsec);


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
	if(nowKeep5MinInterval <0){
		printk("[BAT][SER]%s():NowKeep5MinInterval overflow set interval KEEP_CAPACITY_TIME!\n",__func__);
		nowKeep5MinInterval = KEEP_CAPACITY_TIME;
	}
	//Hank: Interval overflow handling---

	nowKeepInterval = nowTime - balance_this->savedTime;
	//Hank: Interval overflow handling+++	
	if(nowKeepInterval <0){
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
		}
		else{
			pr_debug("[BAT][SER]%s():not already keep 5Min \n",__func__);
		}
	}
	else{
		if(balance_this->test.ifFixedPollingInterval(&balance_this->test)){
			//Hank: test interval+++
			if(!(balance_this->NeedCalCap) && (nowKeepInterval >= (balance_this->test.pollingInterval*3))){
				balance_this->NeedCalCap = true;
				pr_debug("[BAT][SER]%s(): nowTime = %d, savedTime = %d, nowKeepInterval = %d \n default calculate capacity polling interval \n",__func__,(int)nowTime,(int)balance_this->savedTime,(int)nowKeepInterval);
			}//Hank: default 3Min calculate capacity---
			//Hank: capacity < 14% every 1Min calculate capacity+++
			else if(!(balance_this->NeedCalCap) && (nowKeepInterval >= balance_this->test.pollingInterval) && balance_this-> A66_capacity < 14){
				balance_this->NeedCalCap = true;
				pr_debug("[BAT][SER]%s():capacity < 14 calculate capacity every 1Min \n",__func__);
			}
			//Hank: test interval---
		}
		else{
			//Hank: default 3Min calculate capacity+++
			if(!(balance_this->NeedCalCap) && (nowKeepInterval >= DEFAULT_POLLING_INTERVAL)){
				balance_this->NeedCalCap = true;
				pr_debug("[BAT][SER]%s(): nowTime = %d, savedTime = %d, nowKeepInterval = %d \n default calculate capacity polling interval \n",__func__,(int)nowTime,(int)balance_this->savedTime,(int)nowKeepInterval);
			}//Hank: default 3Min calculate capacity---
			//Hank: capacity < 14% every 1Min calculate capacity+++
			else if(!(balance_this->NeedCalCap) && (nowKeepInterval >= DEFAULT_MONITOR_INTERVAL) && balance_this-> A66_capacity < 14){
				balance_this->NeedCalCap = true;
				pr_debug("[BAT][SER]%s():capacity < 14 calculate capacity every 1Min \n",__func__);
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

	if(_this->test.ifFixedPollingInterval(&_this->test)){
		return (_this->test.pollingInterval+DEFAULT_CAP_QUEUE_INTERVAL_PLUGGED);//Eason:cap queue  interval plugs a small value, prevent less than target interval. 
	}
	else{
		if(_this->BatteryService_IsCable || _this-> A66_capacity < 14){
			return (DEFAULT_MONITOR_INTERVAL + DEFAULT_CAP_QUEUE_INTERVAL_PLUGGED);
		}
		else{
			return (DEFAULT_MONITOR_INTERVAL + DEFAULT_CAP_QUEUE_INTERVAL_PLUGGED);
		}
	}
}
//Hank: Tigauge Temperature Monitor---
static int BatteryServiceGauge_OnCapacityReply(struct AXI_Gauge *gauge, struct AXI_Gauge_Callback *gaugeCb, int batCap, int result);

bool g_call_update_cap_work_by_resume = false;
static void BatteryServiceCapSample(struct work_struct *dat)
{
	AXC_BatteryService *_this = container_of(dat,AXC_BatteryService,BatteryServiceUpdateWorker.work);
	wake_lock(&_this->cap_wake_lock);
	pr_debug("[BAT][SER]:%s+++ \n",__func__);

	//Hank: FirstForceResume do not need check Timing+++
	if(_this->IsFirstForceResume){
		printk("[BAT][SER]%s():_this->IsFirstForceResume = true, set _this->IsFirstForceResume = false\n",__func__);
		//Hank first calculate capacity read battery id+++
		read_battery_id();
		//Hank first calculate capacity read battery id---

		_this->IsFirstForceResume = false;
	}
	else{
		pr_debug("[BAT][SER]%s():_this->IsFirstForceResume = false, do checkCalCapTime()\n",__func__);
		if(!g_call_update_cap_work_by_resume){
			checkCalCapTime();
		}
	}
	//Hank: FirstForceResume do not need check Timing---
	
	g_call_update_cap_work_by_resume = false;
	
	if(_this->NeedCalCap){
		pr_debug("[BAT][SER]%s(): Need Calculate Capacity +++\n",__func__);
		_this->IsCalculateCapOngoing = true;

		_this->gauge->askCapacity(_this->gauge);
			
		_this->NeedCalCap = false;
		pr_debug("[BAT][SER]%s(): Need Calculate Capacity ---\n",__func__);
	}

	queue_delayed_work(_this->BatteryServiceCapUpdateQueue,&_this->BatteryServiceUpdateWorker,AXC_BatteryService_getNextPollingInterval(_this)* HZ);

	//Hank: because do temperature monitor every time move unlock & +++
	wake_unlock(&_this->cap_wake_lock);
	//Hank: because do temperature monitor every time move unlock ---	
	pr_debug("[BAT][SER]:%s()--- \n",__func__);
}

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
				return ROBIN_MAX_CHG_CURRENT;
			}
			else{
				return MAX_DISCHG_CURRENT;
			}
  
        case LOW_CURRENT_CHARGER_TYPE:
             if(false == MahDrop){
					return ROBIN_MAX_CHG_CURRENT;
				}
				else{
					return MAX_DISCHG_CURRENT;
             }

#ifdef CONFIG_IDTP9023_CHARGER    
	  case WC_CHARGER_TYPE:    
		   return WIRELESS_CHG_CURRENT;
#endif  							
		  
        case NORMAL_CURRENT_CHARGER_TYPE:
                return ROBIN_MAX_CHG_CURRENT;
  
        case HIGH_CURRENT_CHARGER_TYPE:
             return ROBIN_MAX_CHG_CURRENT;
			 
        default:
             printk("[BAT][Ser]:%s():NO mapping\n",__FUNCTION__);
             if(true==_this->IsResumeMahUpdate)
	      {
                	_this->IsResumeMahUpdate = false;
                	
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
		if(0 >= nCurrent &&  -35 < nCurrent && _this->A66_capacity >= 94)
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
	if(nowResumeInterval <0){
		printk("[BAT][SER]%s():Interval overflow set interval RESUME_UPDATE_TIME!\n",__func__);
		nowResumeInterval = RESUME_UPDATE_TIME;
	}
    //Hank: Interval overflow handling---

	pr_debug("[BAT][SER]:resume queue+++\n");

	if(balance_this->A66_capacity < 14 
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
	g_call_update_cap_work_by_resume = true;
	if(true==needDoResume)
	{
		//Eason set these flag when true==needDoResume+++
		balance_this->IsResumeUpdate = true;
		balance_this->IsResumeMahUpdate = true;
		//Eason set these flag when true==needDoResume---	
	
		if(delayed_work_pending(&balance_this->BatteryServiceUpdateWorker))
		{
			cancel_delayed_work_sync(&balance_this->BatteryServiceUpdateWorker); 
		}
		//Hank: need calculate capacity & temperature monitor+++
		balance_this->NeedCalCap = true;
		//Hank: need calculate capacity & temperature monitor--- 
		printk("[Bat][SER]%s(): queue BatteryServiceUpdateWorker with calculate capacity\n",__func__);  
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
	
	printk("[BAT][SER]:CableOffKeep5():savedtime:%ld\n"
            , balance_this->savedTime
            );
    }    
}

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

//Hank in rom mode show "?" : bootUp check Rom mode queue+++
#ifdef CONFIG_TI_GAUGE
static void CheckIsGaugeRom(struct work_struct *dat)
{
	printk("[BAT][Ser]:bootUpCheckRomMode:%d\n",check_is_RomMode());
}
#endif
//Hank in rom mode show "?" : bootUp check Rom mode queue---

static void BatteryService_InitWoker(AXC_BatteryService *_this)
{
	_this->BatteryServiceCapUpdateQueue \
		= create_singlethread_workqueue("BatteryServiceCapUpdateQueue");
	INIT_DELAYED_WORK(&_this->BatteryServiceUpdateWorker, BatteryServiceCapSample);

	INIT_DELAYED_WORK(&_this->BatRtcReadyWorker, CheckBatRtcReady);

	INIT_DELAYED_WORK(&_this->BatEocWorker, CheckEoc);

	INIT_DELAYED_WORK(&_this->ResumeWorker, ResumeCalCap);

	INIT_DELAYED_WORK(&_this->CableOffWorker, CableOffKeep5Min);

	INIT_DELAYED_WORK(&_this->SetBatLowRTCWorker, BatLowTriggeredSetRTC);

	INIT_DELAYED_WORK(&_this->SetCableInRTCWorker, CableInTriggeredSetRTC); 

	//Hank in rom mode show "?" : bootUp check Rom mode queue+++
#ifdef CONFIG_TI_GAUGE
	INIT_DELAYED_WORK(&_this->CheckGaugeRomModeWorker,CheckIsGaugeRom);
#endif
	//Hank in rom mode show "?" : bootUp check Rom mode queue---

	INIT_DELAYED_WORK(&_this->ReadBatteryCapacityWorker, read_bat_cap_work);
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
		status = BAT_CHARGING_STATUS;
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

	{
#if USE_SW_GAUGE //report capacity by SW gauge
		if( (true==_this->BatteryService_IsFULL)&&(_this->A66_capacity>=99) ) {
			pr_debug("[BAT][Ser]more than 99p & FULL flag, show 100\n");
			_this->A66_capacity = 100;
		}
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

#if 0
			//Eason :when  low bat Cap draw large current  +++	 
			if(10 <= _this->A66_capacity )
			{
				schedule_delayed_work(&_this->CableOffWorker,1*HZ);//keep 100% 5 min
			}
			//Eason :when  low bat Cap draw large current  ---
#endif

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

static void AXC_BatteryService_suspend(struct AXI_BatteryServiceFacade *bat)
{
	AXC_BatteryService  *_this = container_of(bat, AXC_BatteryService, miParent);

	_this->HasCableBeforeSuspend = _this->BatteryService_IsCable;
}

//Eason resume always calculate capacity no matter if in   Pad or CableIn or BatLow+++
extern void setSmb346CC_Curr900mA_Iterm50(void);
static void AXC_BatteryService_resume(struct AXI_BatteryServiceFacade *bat,int delayStartInSeconds)
{
	AXC_BatteryService  *_this=
		container_of(bat, AXC_BatteryService, miParent);

	schedule_delayed_work(&_this->ResumeWorker,0*HZ);
//	wake_lock_timeout(&_this->resume_wake_lock,2* HZ);

	if( false == reportRtcReady()){
		queue_delayed_work(_this->BatteryServiceCapUpdateQueue,
			&_this->BatRtcReadyWorker,
			RTC_READY_DELAY_TIME * HZ);
	}
}
//Eason resume always calculate capacity no matter if in   Pad or CableIn or BatLow---

static void AXC_BatteryService_forceResume(struct AXI_BatteryServiceFacade *bat,int delayStartInSeconds)
{
	AXC_BatteryService  *_this=
		container_of(bat, AXC_BatteryService, miParent);

	time_t nowForceResumeTime;
	time_t nowForceResumeInterval;
	bool needDoForceResume=false;

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
             if(GetStateFromFsm == DISCHARGING_STATE || GetStateFromFsm == CHARGING_FULL_STATE ){
                NeedUpdate = 1;
             }
            break;
        case CHARGING_STOP_STATE:
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
	int nCurrent = _this->callback->getIBAT(_this->callback);

	chgStatus = gpCharger->IsCharging(gpCharger);

	pr_debug("[BAT][SER]%s():Charging Status:%d, Battery Current:%d\n",__func__,chgStatus,nCurrent);

	if(chgStatus){//charging
		if(!_this->isMainBatteryChargingDone){// if still charging....
			if(nowGaugeCap >= 94 && 0 >= nCurrent && -35 < nCurrent && !delayed_work_pending(&_this->BatEocWorker)){
				printk("[BAT][Ser]start eoc worker\n");
				queue_delayed_work(_this->BatteryServiceCapUpdateQueue, &_this->BatEocWorker,10 * HZ);
			}
		}
		else{
			chgStatus = false;    
		}
	}
	else{//no charging
		if( 0 >= nCurrent && -35 < nCurrent){
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

static void write_battery_capacity(int cap)
{
	struct file *cfile;
	mm_segment_t orgfs;
	char bat_cap_str[14];
	struct timespec mtNow;

	mtNow = current_kernel_time();

	orgfs = get_fs();
	set_fs(KERNEL_DS);

	cfile = filp_open("/data/data/bat_cap", O_CREAT | O_RDWR | O_SYNC, 0666);
	if (!IS_ERR(cfile)){	
		sprintf(bat_cap_str, "%3d%10ld", cap, mtNow.tv_sec);
		pr_debug("[BAT][Ser]save bat_cap: %s\n", bat_cap_str);
		cfile->f_op->write(cfile, bat_cap_str, 13, &cfile->f_pos);
		
		if(filp_close(cfile, NULL)){
			pr_err("[BAT][Ser] fail to close bat_cap cali file in %s()\n", __func__);	
		}
	}

	set_fs (orgfs);
	return;
}

static int init_battery_capacity(void)
{
	struct file *fd;
	mm_segment_t mmseg_fs;
	int FileLength = 0;
	char bat_cap_str[14];
	int bat_cap_int = -1;
	unsigned long bat_cap_save_time = 0;
	struct timespec mtNow;

	mmseg_fs = get_fs();
	set_fs(KERNEL_DS);

	fd = filp_open("/data/data/bat_cap", O_RDONLY,0);
	if (IS_ERR(fd))
	{
		pr_err( "[BAT][Ser] can not open battery capacity file\n");
		set_fs(mmseg_fs);
		return -1;
	}

#if defined(ASUS_CHARGING_MODE) && !defined(ASUS_FACTORY_BUILD)
	if(g_asus_CHG_mode){
		printk("[BAT][Ser] g_asus_CHG_mode = 1, don't read bat_cap\n");

		if (filp_close(fd, NULL)){
			pr_err("[BAT][Ser]fail to close battery capacity file in %s\n", __func__);
		}
	
		write_battery_capacity(balance_this->A66_capacity);

		set_fs(mmseg_fs);
		g_already_read_bat_cap = true;
		return 0;
	}
#endif
	
	FileLength = fd->f_op->read(fd, bat_cap_str, 13, &fd->f_pos);
	if (13 == FileLength) {
		if(bat_cap_str[1] == 0x20){
			char bat_cap_int_temp = bat_cap_str[2];
			bat_cap_int = (int)simple_strtol(&bat_cap_int_temp, NULL, 10);
		}
		else if(bat_cap_str[0] == 0x20){
			char bat_cap_int_temp[2];
			memcpy(bat_cap_int_temp, bat_cap_str+1, 2);
			bat_cap_int = (int)simple_strtol(bat_cap_int_temp, NULL, 10);
		}
		else{
			bat_cap_int = 100;
		}

		bat_cap_save_time = simple_strtoul(bat_cap_str+3, NULL, 10);
		mtNow = current_kernel_time();
		
		printk("[BAT][ser] init_battery_capacity bat_cap_int = %d\n", bat_cap_int);
		printk("[BAT][ser] init_battery_capacity bat_cap_save_time = %ld\n", bat_cap_save_time);
		
		if(bat_cap_int <= 2){
			bat_cap_int = 3;
		}

		if(mtNow.tv_sec - bat_cap_save_time < 259200){
			if( (bat_cap_int-30 > balance_this->A66_capacity)
				|| (bat_cap_int+30 < balance_this->A66_capacity))
			{
				printk("[BAT][ser] the difference is more than 30, don't apply bat_cap\n");
			}
			else if(bat_cap_int-10 > balance_this->A66_capacity){
				balance_this->A66_capacity = bat_cap_int - 10;
			}
			else if(bat_cap_int+10 < balance_this->A66_capacity){
				balance_this->A66_capacity = bat_cap_int + 10;
			}
			else{
				balance_this->A66_capacity = bat_cap_int;
			}
			
			g_SWgauge_lastCapacity = balance_this->A66_capacity;
			printk("[BAT][ser] init_battery_capacity set battery capacity to %d\n", balance_this->A66_capacity);
		}
	}
	else {
		printk("[BAT][Ser] error init_battery_capacity, FileLength = %d\n", FileLength);
	}

	if (filp_close(fd, NULL)){
		pr_err("[BAT][Ser]fail to close battery capacity file in %s\n", __func__);
	}
	
	set_fs(mmseg_fs);
	
	g_already_read_bat_cap = true;
	write_battery_capacity(balance_this->A66_capacity);
  	return 0;
}

static void read_bat_cap_work(struct work_struct *work)
{
	init_battery_capacity();

	g_already_read_bat_cap = true;
	return;		
}

//ASUS_BSP Eason read PM8226 register value+++
extern int pm8226_qpnp_chg_read_register(u16 addr);
//ASUS_BSP Eason read PM8226 register value---
extern int get_Volt_from_TIgauge(void);
extern int get_FCC_from_TIgauge(void);
static void AXC_BatteryService_reportPropertyCapacity(struct AXC_BatteryService *_this, int refcapacity)
{
	int A66_capacity;
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
		EnableBATLifeRise = Get_CapRiseWhenCableIn(refcapacity, lastCapacity);
		if( false == EnableBATLifeRise){
			maxMahDrop = true;
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

    pr_debug("[BAT][Ser]report Capacity:%d,%d,%d,%d,%d,%d,%d,%d,%ld==>%d\n",
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

#if 0
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
#else
	ASUSEvtlog("[BAT][Ser]report Capacity:%d,%d,%d,%d,%d,%d,%d,%d,%ld==>%d  ,BMS:%d, Cur:%d, Temp:%d\n",
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
                                      pmicTemp);
#endif
	//ASUS_BSP --- Eason_Chang add event log ---   

	//Eason: remember last BMS Cap to filter+++
	//	last_BMS_Cap = gBMS_Cap;
	//Eason: remember last BMS Cap to filter---

	if(A66_capacity < 0 || A66_capacity >100){
		printk("[BAT][Ser]Filter return value fail!!!\n");
	}
	else{
		//Eason show BMS cap to user+++
#if USE_SW_GAUGE
		_this->A66_capacity = A66_capacity;
#else
		_this->A66_capacity = gBMS_Cap;
#endif
		g_SWgauge_lastCapacity = A66_capacity;
		//Eason show BMS cap to user---

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

	if(!g_already_read_bat_cap){
		if (init_battery_capacity() < 0) {
			printk("[BAT][Ser] cannot read battery capacity from file, maybe file system not ready, try again!\n");
			schedule_delayed_work(&balance_this->ReadBatteryCapacityWorker, 20*HZ);
		}
	}
	
	if(g_already_read_bat_cap == true){
		write_battery_capacity(A66_capacity);
	}
}
static int BatteryServiceGauge_OnCapacityReply(struct AXI_Gauge *gauge, struct AXI_Gauge_Callback *gaugeCb, int batCap, int result)
{   

    AXC_BatteryService  *_this=
        container_of(gaugeCb, AXC_BatteryService, gaugeCallback);

    //ASUS BSP: Eason check correct BMS RUC+++
    int BMSCap;
    //int BMS_diff_SWgauge;
    //ASUS BSP: Eason check correct BMS RUC---

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
		
		_this->savedTime=updateNowTime(_this);
		//Eason:fix Cap drop too slowly in unattended mode+++
		filRealSusT = 0;
		//Eason:fix Cap drop too slowly in unattended mode---
		
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

//ASUS BSP Eason_Chang --- batteryservice to gauge

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
    .ForceSavedTime = 0,//for A68 will always update no matter if change MaxMAh
    .savedTime = 0,//for A68 may dont update if change 10==MaxMAh
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
    .gaugeCallback ={
        .onCapacityReply = BatteryServiceGauge_OnCapacityReply,
        .askSuspendCharging = BatteryServiceGauge_AskSuspendCharging,   
        .askResumeCharging = BatteryServiceGauge_AskResumeCharging,
        },// batteryservice to gauge
    .chargerType =  NOTDEFINE_TYPE ,  // batteryservice to gauge
    .gauge = NULL,  // batteryservice to gauge
    .gpCapFilterA66 = NULL,
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

#endif //#ifdef CONFIG_BATTERY_ASUS_SERVICE








