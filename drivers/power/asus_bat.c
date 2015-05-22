//ASUS_BSP +++ Josh_Liao "add asus battery driver"
/* asus_bat.c */
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/power_supply.h>
#include <linux/mutex.h>

#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/asus_bat.h>
#include <linux/mfd/pm8xxx/batt-alarm.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/mfd/pm8xxx/batt-alarm.h>
#include <linux/asus_chg.h>
//ASUS_BSP  +++ Eason_Chang "add P01 charge"
#include <linux/mfd/pm8xxx/pm8921-charger.h>
//ASUS_BSP  --- Eason_Chang "add P01 charge"
//ASUS_BSP  +++ Eason_Chang "add BAT low debounce"
#include <linux/delay.h>
//ASUS_BSP  --- Eason_Chang "add BAT low debounce"
//ASUS_BSP  +++ Eason_Chang "add BAT info time"
#include <linux/rtc.h>
//ASUS_BSP  --- Eason_Chang "add BAT info time"

//ASUS_BSP  +++ Eason_Chang "add BAT earlysuspend"
#if defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
//ASUS_BSP  --- Eason_Chang "add BAT earlysuspend"
//Hank: 1025 use frame buffer notifier to implement earlysuspend+++
#elif defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>

struct notifier_block bat_fb_notif;
//Hank: 1025 use frame buffer notifier to implement earlysuspend---
#endif

//ASUS_BSP +++ Josh_Liao "sw gauge v2"
#include "../power/gauge/test_gauge.h"
//ASUS_BSP --- Josh_Liao "sw gauge v2"
//ASUS_BSP Eason_Chang add event log +++
#include <linux/asusdebug.h>
//ASUS_BSP Eason_Chang add event log ---

#define ASUS_BAT_DEF_UPDATE_RATE  180	// the update rate is 3 minutes in default                 

#define ASUS_BAT_PROC_FILE  "driver/asus_bat"
#define ASUS_BAT_PROC_FILE_PERMISSION  0777

#define ASUS_BAT_PROC_MAX_BUFF_SIZE  256

/* battery command */
#define BAT_CMD_INTERVAL					2
#define BAT_CMD_READ						'r'
#define BAT_CMD_CHARGING_STATUS			'c'
#define BAT_CMD_CHARGING_MODE			'm'
#define BAT_CMD_CHARGER_CABLE			'r'
#define BAT_CMD_BAT_PRESENT				'p'
#define BAT_CMD_BAT_LIFE					'l'
#define BAT_CMD_WRITE						'w'
#define BAT_CMD_HELP						'h'
#define BAT_CMD_PHONE_ID					'0'
#ifdef CONFIG_BATTERY_ASUS_SERVICE
#define BAT_CMD_FIX_INTERVAL			'i'
#define BAT_CMD_FIX_LAST_UPDATE_INTERVAL			'l'
#endif
#ifdef CONFIG_PM8921_CHARGER
#define BAT_CMD_CHG_STATE_MONITOR_INTERVAL 'p'
#endif
#define BAT_CMD_ENABLE_TEST				't'
#ifdef CONFIG_ASUS_POWER_UTIL
#define BAT_CMD_ENABLE_UNIT_TEST		'e'
#define BAT_CMD_ENABLE_DOITLATER_TEST		'd'
#define BAT_CMD_ENABLE_FEEDING_FILE_INPUT_TEST		'i'
#endif
#ifdef CONFIG_CHARGER_ASUS_FSM
#define BAT_CMD_ENABLE_FSM_STRESS_TEST		'f'
#define BAT_CMD_ENABLE_FSM_SINGLE_STATE_TEST		's'
#endif
#ifdef CONFIG_BATTERY_ASUS_SERVICE
#define BAT_CMD_ENABLE_SERVICE_TEST		's'
#endif

//ASUS_BSP +++ Josh_Liao "sw gauge v2"
#define BAT_CMD_TEST_SWG		's'
#define BAT_CMD_SWG_CAP		'c'
#define BAT_CMD_SWG_CABLE		'b'
#define BAT_CMD_SWG_BAT_FULL	'f'
#define BAT_CMD_SWG_FILTER_CAP 'l'
//ASUS_BSP --- Josh_Liao "sw gauge v2"

//ASUS_BSP +++ Eason_Chang BalanceMode
#define BAT_CMD_ENABLE_BALANCE 'b' 
#define BAT_CMD_BALANCE_ISBALANCE 'i'
#define BAT_CMD_BALANCE_ISTEST 'e'
#define BAT_CMD_BALANCE_STARTRATIO 's'
#define BAT_CMD_BALANCE_STOPRATIO 't'
#define BAT_CMD_BALANCE_A66CAP 'a'
//ASUS_BSP --- Eason_Chang BalanceMode

#define BAT_CMD_ALL_INFO					'a'
#define BAT_CMD_UPDATE					'u'
#define BAT_CMD_SET						's'

#define BAT_STATUS_TXT     "/data/data/battery_status"

#define BAT_TRUE_TXT						"true"
#define BAT_FALSE_TXT						"false"


#define BAT_LOW_SHUTDOWN_CAP			0

#define BAT_FIRST_UPDATE_DELAY			20

#define TIME_FOR_FIRST_START_BATTERY_SERVER_IN_SECONDS (0)

/******************************************************************************/
enum asus_bat_type {
	ASUS_BAT_PHONE = 0,
	ASUS_BAT_UNKNOWN
};

enum asus_bat_charging_status {
	ASUS_BAT_CHARGING_ERR = 0,
	ASUS_BAT_CHARGING_NONE,
	ASUS_BAT_CHARGING_ONGOING,
	ASUS_BAT_CHARGING_FULL,
	ASUS_BAT_CHARGING_UNKNOWN
};

#if 0
enum asus_bat_charger_cable {
	ASUS_BAT_CHARGER_NONE = 0,
	ASUS_BAT_CHARGER_USB,
	ASUS_BAT_CHARGER_AC,
	ASUS_BAT_CHARGER_OTHER_BAT,
	ASUS_BAT_CHARGER_UNKNOWN
};
#endif

const static char *charging_txt[] = {
	"CHARGING_ERR",
	"CHARGING_NONE",
	"CHARGING_ONGOING",
	"CHARGING_FULL",
	"CHARGING_UNKNOWN"
};

const static char *charger_txt[] = {
	"CHARGER_NONE",
	"CHARGER_USB",
	"CHARGER_AC",
	"CHARGER_OTHER_BAT",
	"CHARGER_UNKNOWN"
};

const static char *ps_charging_txt[] = {
	"POWER_SUPPLY_STATUS_UNKNOWN",
	"POWER_SUPPLY_STATUS_CHARGING",
	"POWER_SUPPLY_STATUS_DISCHARGING",
	"POWER_SUPPLY_STATUS_NOT_CHARGING",
	"POWER_SUPPLY_STATUS_FULL",
};

struct asus_bat_basic {
	bool present;
	bool bat_low;
	int capacity;
	enum asus_bat_charger_cable charger;
	enum asus_bat_charging_status charging;
	int volt;		// voltage			
	int curr;		// current
	struct delayed_work bat_low_work;
};

/******************************************************************************/
struct asus_bat_all_info {
	struct mutex hs_evt_lock;
	struct mutex microp_evt_lock;
//	struct mutex microp_rw_lock;
	struct delayed_work bat_update_work;
	struct asus_bat_basic phone_b;
	bool enable_test;
	int bat_update_work_interval;
};

struct asus_bat_phone_bat_struct *phone_bat_info = NULL;


static bool g_done_first_periodic_update = false;


static void asus_bat_update_all_bat(void);

/******************************************************************************/
struct workqueue_struct *asus_bat_wq = NULL;
static struct asus_bat_all_info *asus_bat = NULL;

/******************************************************************************/
//ASUS_BSP +++ Eason_Chang BalanceMode
static int BatteryService_BalanceMode_IsBalTest = 0; //default 0.Change to 1 if BalTest selected
static int BatteryService_BalanceMode_StopRatio = 1;
static int BatteryService_BalanceMode_StartRatio = 9; // divid 2  is the real StartRatiio
static int BatteryService_BalanceMode_A66_CAP = 66;  //just a special default value
//ASUS_BSP --- Eason_Chang BalanceMode

//ASUS_BSP  +++ Eason_Chang "sw gauge support"
#include "gauge/axc_gaugefactory.h"
#include "charger/axc_chargerfactory.h"
#define BATTERY_LAST_CHANGED_LEVEL (1) 
#define BATTERY_LAST_LEVEL (2)
//#include "asus_bat_dbg.h"

#include "gauge/sampledata/axc_sampledatafactory.h"
#include "gauge/sampledata/axc_intsampledata.h"
#include "gauge/axi_gauge.h"
//static bool g_has_fixed_bat_life = false;
//static int g_fixed_bat_life = 0;
int ReportBatteryServiceP02Cap(void);//P02gauge

//Hank get voltage from TIgauge++
#ifdef CONFIG_TI_GAUGE
extern int get_Volt_from_TIgauge(void);
#endif
//Hank get voltage from TIgauge--

//Eason takeoff Battery shutdown +++
bool g_AcUsbOnline_Change0 = false;
extern void AcUsbPowerSupplyChange(void);
//Eason takeoff Battery shutdown ---

//ASUS BSP Eason Chang coincell+++
#include "../../include/linux/mfd/pm8xxx/misc.h" 
int pm8xxx_coincell_chg_config(struct pm8xxx_coincell_chg *chg_config);
struct pm8xxx_coincell_chg bat_chg_config={
            .state = PM8XXX_COINCELL_CHG_ENABLE,
            .voltage = PM8XXX_COINCELL_VOLTAGE_2p5V,
            .resistor = PM8XXX_COINCELL_RESISTOR_800_OHMS,
};
//ASUS BSP Eason Chang coincell---
//Eason boot up in BatLow situation, take off cable can shutdown+++
bool g_BootUp_IsBatLow = false;
//Eason boot up in BatLow situation, take off cable can shutdown---
//Eason if doesn't get correct ADC Vol&Curr at first update Cap show unknow status +++ 
extern bool g_adc_get_correct_VolCurr;
//Eason if doesn't get correct ADC Vol&Curr at first update Cap show unknow status ---
//Eason: LowCapCpuThrottle +++
static int LowCapCpuThrottle = 0 ;
static struct kobject *kobj;
//Eason: LowCapCpuThrottle ---
//Eason:let mpdicision check cpu throttle status+++
extern bool IsInCpuThrottle;
//Eason:let mpdicision check cpu throttle status---
//Hank TIgauge Update++
bool g_IsInRomMode = false;
//Hank TIgauge Update--
//Eason: Factory5060Mode+++
#ifdef ASUS_FACTORY_BUILD
extern bool g_5060modeCharging;
#endif
//Eason: Factory5060Mode---

#include <linux/rtc.h>
AXI_Charger * getAsusCharger(void)
{
    static AXI_Charger *lpCharger = NULL;

    if(lpCharger != NULL){

        return lpCharger;
    }
//ASUS_BSP Eason_Chang 1120 porting +++
/*
    if(g_A60K_hwID >=A66_HW_ID_ER2){

              AXC_ChargerFactory_GetCharger(E_SMB346_CHARGER_TYPE,&lpCharger);

    }else{

            AXC_ChargerFactory_GetCharger(E_PM8921_CHARGER_TYPE,&lpCharger);
    }
*/

#ifdef CONFIG_PM_8226_CHARGER
	AXC_ChargerFactory_GetCharger(E_PM8226_CHARGER_TYPE,&lpCharger);
#else
	AXC_ChargerFactory_GetCharger(E_SMB346_CHARGER_TYPE,&lpCharger);
#endif
//ASUS_BSP Eason_Chang 1120 porting ---    

    return lpCharger;
}

#ifdef CONFIG_BATTERY_ASUS_SERVICE
#include "service/AXI_BatteryServiceFacade.h"
AXI_BatteryServiceFacade *loService = NULL;
#endif


bool asus_bat_has_done_first_periodic_update(void)
{
	return g_done_first_periodic_update;
}


static void asus_bat_set_phone_bat_capacity(int bat_cap)
{	
	if (NULL == asus_bat) {
		printk(DBGMSK_BAT_ERR "[BAT]%s(), null ptr \r\n", __FUNCTION__);
		return;
	}

	asus_bat->phone_b.capacity = bat_cap;
	return;
}


static void asus_bat_set_phone_bat_status(int bat_status)
{	
	if (NULL == asus_bat) {
		printk(DBGMSK_BAT_ERR "[BAT]%s(), null ptr \r\n", __FUNCTION__);
		return;
	}
	switch (bat_status) {
	case POWER_SUPPLY_STATUS_DISCHARGING:
		asus_bat->phone_b.charging = ASUS_BAT_CHARGING_NONE;
		break;
	case POWER_SUPPLY_STATUS_CHARGING:
		asus_bat->phone_b.charging = ASUS_BAT_CHARGING_ONGOING;
		break;
	case POWER_SUPPLY_STATUS_NOT_CHARGING:
		asus_bat->phone_b.charging = ASUS_BAT_CHARGING_NONE;		
		break;
	case POWER_SUPPLY_STATUS_FULL:
		asus_bat->phone_b.charging = ASUS_BAT_CHARGING_FULL;
		break;
	default:
		asus_bat->phone_b.charging = ASUS_BAT_CHARGING_UNKNOWN;
	}

	pr_debug( "[BAT]%s(), phone charging:%d \r\n", __FUNCTION__, asus_bat->phone_b.charging);

	return;
}


int asus_bat_get_phone_bat_capacity(void)
{
	if (NULL == asus_bat) {
		printk(DBGMSK_BAT_ERR "[BAT]%s(), null ptr \r\n", __FUNCTION__);
		return 0;
	}

	return asus_bat->phone_b.capacity;
}


/* For B-MMI usage */
void asus_bat_write_phone_bat_capacity_tofile(void)
{	
	struct file *fp_bat_sts = NULL;
	int used = 0;
	mm_segment_t old_fs;
	unsigned char bat_cap_s[4];
	loff_t pos_bat = 0;

	if (NULL == asus_bat) {
		printk(DBGMSK_BAT_ERR"[BAT]%s(), null ptr \r\n", __FUNCTION__);
		return;
	}

	used = snprintf(bat_cap_s, sizeof(bat_cap_s) ,"%d", asus_bat->phone_b.capacity);
	pr_debug( "[BAT] %s(), phone bat capacity:%d, write bat capcity string:%s, used:%d \r\n", 
		__FUNCTION__, asus_bat->phone_b.capacity, bat_cap_s, used);
	
 	old_fs = get_fs();
 	set_fs(KERNEL_DS);

	fp_bat_sts = filp_open(BAT_STATUS_TXT, O_WRONLY|O_CREAT, 0640);
	if (!IS_ERR(fp_bat_sts)) {
		fp_bat_sts->f_op->write(fp_bat_sts, bat_cap_s, used, &pos_bat);
		set_fs(old_fs);
		filp_close(fp_bat_sts, NULL);
	} else {
		printk(DBGMSK_BAT_ERR "[BAT] error!! bat_status filp open failed. \r\n");
	}

	return;
}
EXPORT_SYMBOL_GPL(asus_bat_write_phone_bat_capacity_tofile);

static void asus_bat_set_phone_bat_present(bool present)
{
	asus_bat->phone_b.present = present;

	return;
}

static bool asus_bat_is_phone_bat_present(void)
{
	if (NULL == asus_bat)
		return false;

	return asus_bat->phone_b.present;

}

static enum asus_bat_charger_cable asus_bat_get_phone_charger(void)
{
	return asus_bat->phone_b.charger;
}


static void asus_bat_set_phone_charger(enum asus_bat_charger_cable charger )
{
	asus_bat->phone_b.charger = charger;
	return;
}

static enum asus_bat_charging_status asus_bat_get_phone_charging(void)
{
	return asus_bat->phone_b.charging;
}


static void asus_bat_set_phone_charging(enum asus_bat_charging_status charging )
{
	asus_bat->phone_b.charging = charging;
	pr_debug("[BAT]%s(), charging:%s\n", __FUNCTION__, charging_txt[charging]);
	return;
}

typedef void (*bat_cmd_func)(const char *msg, int index);

static void bat_cmd_read_phone_charging_status(const char *msg, int index)
{
	// by hw for test
	int charging;
	charging = phone_bat_info->get_prop_bat_status_byhw();
	printk(DBGMSK_BAT_INFO "[BAT] %s(), msg[%d]=%c, charging:%s \r\n",
		__FUNCTION__, index, msg[index], charging_txt[charging]);

	return ;
}

static void bat_cmd_read_phone_charger_cable(const char *msg, int index)
{
	int charger = asus_bat_get_phone_charger();
	printk(DBGMSK_BAT_INFO "[BAT] %s(), msg[%d]=%c, charger:%s \r\n",
		__FUNCTION__, index, msg[index], charger_txt[charger]);

	return;
}

static void bat_cmd_read_phone_bat_life(const char *msg, int index)
{
	int bat_life;
	printk(DBGMSK_BAT_INFO "[BAT] %s(), msg[%d]=%c \r\n", __FUNCTION__, index, msg[index]);
	bat_life = asus_bat_get_phone_bat_capacity();
	printk(DBGMSK_BAT_INFO "[BAT] %s(), bat_life:%d \r\n", __FUNCTION__, bat_life);
	return ;
}

static void bat_cmd_read_phone_bat_present(const char *msg, int index)
{
	printk(DBGMSK_BAT_INFO "[BAT] %s(), msg[%d]=%c \r\n", __FUNCTION__, index, msg[index]);
	if (asus_bat_is_phone_bat_present()) {
		printk(DBGMSK_BAT_INFO "[BAT] %s(), phone bat present \r\n", __FUNCTION__);
	} else {
		printk(DBGMSK_BAT_INFO "[BAT] %s(), phone bat not present \r\n", __FUNCTION__);
	}
	return ;
}

static void bat_cmd_write_phone_charging_status(const char *msg, int index)
{
	int val;
	index += BAT_CMD_INTERVAL;
	val = (int)simple_strtol(&msg[index], NULL, 10);

	printk(DBGMSK_BAT_INFO "[BAT] %s(), msg[%d]=%c, val:%d \r\n", __FUNCTION__, index, msg[index], val);

	asus_bat_set_phone_charging(val);

	return ;
}

static void bat_cmd_set_phone_charging_mode(const char *msg, int index)
{
	int val;
	index += BAT_CMD_INTERVAL;
	val = (int)simple_strtol(&msg[index], NULL, 10);

	printk(DBGMSK_BAT_INFO "[BAT] %s(), msg[%d]=%c, val:%d \r\n", __FUNCTION__, index, msg[index], val);

	if ((val >= ASUS_CHG_SRC_UNKNOWN) || (val < ASUS_CHG_SRC_NONE)) {
		printk(DBGMSK_BAT_ERR "[BAT] Error!! unknown charging mode. \r\n");
		return;
	}

	asus_chg_set_chg_mode(val);

	return ;
}

static void bat_cmd_write_phone_charger_cable(const char *msg, int index)
{
	int val;
	index += BAT_CMD_INTERVAL;
	val = (int)simple_strtol(&msg[index], NULL, 10);

	printk(DBGMSK_BAT_INFO "[BAT] %s(), msg[%d]=%c, val:%d \r\n", __FUNCTION__, index, msg[index], val);

	asus_bat_set_phone_charger(val);

	return ;
}

static void bat_cmd_write_phone_bat_life(const char *msg, int index)
{
	int val;
	index += BAT_CMD_INTERVAL;
	val = (int)simple_strtol(&msg[index], NULL, 10);

	printk(DBGMSK_BAT_INFO "[BAT] %s(), msg[%d]=%c, val:%d \r\n", __FUNCTION__, index, msg[index], val);

    //ASUS_BSP +++ Eason_Chang BalanceMode
    //asus_bat_set_phone_bat_capacity(val);
    BatteryService_BalanceMode_A66_CAP = val;
    //ASUS_BSP --- Eason_Chang BalanceMode

	return ;
}

static void bat_cmd_write_phone_bat_present(const char *msg, int index)
{
	int val;
	index += BAT_CMD_INTERVAL;
	val = (int)simple_strtol(&msg[index], NULL, 10);

	printk(DBGMSK_BAT_INFO "[BAT] %s(), msg[%d]=%c, val:%d \r\n", __FUNCTION__, index, msg[index], val);

	asus_bat_set_phone_bat_present(val);

	return ;
}

static void bat_cmd_read_all_info(const char *msg, int index )
{
	printk(DBGMSK_BAT_INFO "[BAT] %s(), msg[%d]=%c \r\n", __FUNCTION__, index, msg[index]);

	printk(DBGMSK_BAT_INFO "[BAT] phone bat present: %s \r\n", asus_bat->phone_b.present?BAT_TRUE_TXT:BAT_FALSE_TXT);

	printk(DBGMSK_BAT_INFO "[BAT] phone bat life: %d \r\n", asus_bat->phone_b.capacity);

	printk(DBGMSK_BAT_INFO "[BAT] phone charging status: %s \r\n", charging_txt[asus_bat->phone_b.charging]);

	printk(DBGMSK_BAT_INFO "[BAT] phone charger cable: %s \r\n", charger_txt[asus_bat->phone_b.charger]);

	printk(DBGMSK_BAT_INFO "[BAT] enable test:%s \r\n", asus_bat->enable_test?BAT_TRUE_TXT:BAT_FALSE_TXT);
	printk(DBGMSK_BAT_INFO "[BAT] update bat interval:%d \r\n", asus_bat->bat_update_work_interval);


	printk(DBGMSK_BAT_INFO "[BAT] phone bat status from HW: %d \r\n", phone_bat_info->get_prop_bat_status_byhw());
#ifdef CONFIG_TI_GAUGE
	printk(DBGMSK_BAT_INFO "[BAT] phone bat volt from TIgauge: %dmV \r\n", get_Volt_from_TIgauge());
#endif
	printk(DBGMSK_BAT_INFO "[BAT] phone bat curr from HW: %d \r\n", phone_bat_info->get_prop_batt_curr_byhw());
	
	return ;
}
#ifdef CONFIG_BATTERY_ASUS_SERVICE
#include "service/AXC_BatteryServiceTest.h"
static void bat_cmd_fix_interval(const char *msg, int index )
{
       bool fixed;

       int interval;

       AXC_BatteryServiceTest *test =  getBatteryServiceTest();

	index += BAT_CMD_INTERVAL;

	printk(DBGMSK_BAT_INFO "[BAT] %s(), msg[%d]=%c \r\n", __FUNCTION__, index, msg[index]);

	fixed = (bool)((int)simple_strtol(&msg[index], NULL, 10) ?true:false);

       if(fixed){
        
            index += BAT_CMD_INTERVAL;

            interval = (int)simple_strtol(&msg[index], NULL, 10);

            test->changePollingInterval(test, true, interval);

            	printk(DBGMSK_BAT_INFO "[BAT] %s(), fix update interval:%d \r\n", __FUNCTION__, interval);

       }else{
       
           test->changePollingInterval(test, false, 0);

           	printk(DBGMSK_BAT_INFO "[BAT] %s(), release fix update interval\r\n", __FUNCTION__);

       }	

	return ;
}
static void bat_cmd_fix_last_update_interval(const char *msg, int index )
{
       bool fixed;

       int interval;

       AXC_BatteryServiceTest *test =  getBatteryServiceTest();

	index += BAT_CMD_INTERVAL;

	printk(DBGMSK_BAT_INFO "[BAT] %s(), msg[%d]=%c \r\n", __FUNCTION__, index, msg[index]);

	fixed = (bool)((int)simple_strtol(&msg[index], NULL, 10) ?true:false);

       if(fixed){
        
            index += BAT_CMD_INTERVAL;

            interval = (int)simple_strtol(&msg[index], NULL, 10);

            test->changeFilterLastUpdateInterval(test, true, interval);

            	printk(DBGMSK_BAT_INFO "[BAT] %s(), fix update interval:%d \r\n", __FUNCTION__, interval);

       }else{
       
             test->changeFilterLastUpdateInterval(test, false, 0);

           	printk(DBGMSK_BAT_INFO "[BAT] %s(), release fix update interval\r\n", __FUNCTION__);

       }	

	return ;

}
#endif //CONFIG_BATTERY_ASUS_SERVICE
#ifdef CONFIG_PM8921_CHARGER
void change_chg_statue_check_interval(int interval);

static void bat_cmd_fix_chg_state_monitor_interval(const char *msg, int index )
{

       int interval;

	index += BAT_CMD_INTERVAL;

	printk(DBGMSK_BAT_INFO "[BAT] %s(), msg[%d]=%c \r\n", __FUNCTION__, index, msg[index]);

	interval = (int)simple_strtol(&msg[index], NULL, 10);

       change_chg_statue_check_interval(interval);

	return ;

}
#endif //CONFIG_PM8921_CHARGER

static void bat_cmd_write_test(const char *msg, int index )
{
	int val;

	index += BAT_CMD_INTERVAL;

	printk(DBGMSK_BAT_INFO "[BAT] %s(), msg[%d]=%c \r\n", __FUNCTION__, index, msg[index]);

	val = (int)simple_strtol(&msg[index], NULL, 10);

	if (val >= 1) {
		asus_bat->enable_test = true;
		printk(DBGMSK_BAT_INFO "[BAT] enable test");
	} else {
		asus_bat->enable_test = false;
		printk(DBGMSK_BAT_INFO "[BAT] disable test");
	}

	return ;
}

//TODO
static void bat_cmd_get_help(const char *msg, int index )
{
	printk(DBGMSK_BAT_INFO "[BAT] %s(), msg[%d]=%c \r\n", __FUNCTION__, index, msg[index]);
// print help info
	return ;
}

static void bat_cmd_update(const char *msg, int index )
{
	printk(DBGMSK_BAT_INFO "[BAT] %s(), msg[%d]=%c \r\n", __FUNCTION__, index, msg[index]);

	asus_bat_update_all_bat();
	return ;
}

				   
struct asus_bat_proc_cmd {
	char id;
	const bat_cmd_func func;
	struct asus_bat_proc_cmd *cmd_tbl;
	int cmd_tbl_size;
};


struct asus_bat_proc_cmd phone_bat_read_cmd_tbl[] = {
	{BAT_CMD_CHARGING_STATUS, bat_cmd_read_phone_charging_status, NULL, 0},
	{BAT_CMD_CHARGER_CABLE, bat_cmd_read_phone_charger_cable, NULL, 0},		
	{BAT_CMD_BAT_LIFE, bat_cmd_read_phone_bat_life, NULL, 0},
	{BAT_CMD_BAT_PRESENT, bat_cmd_read_phone_bat_present, NULL, 0},
};

struct asus_bat_proc_cmd phone_bat_write_cmd_tbl[] = {
	{BAT_CMD_CHARGING_STATUS, bat_cmd_write_phone_charging_status, NULL, 0},
	{BAT_CMD_CHARGER_CABLE, bat_cmd_write_phone_charger_cable, NULL, 0},
	{BAT_CMD_BAT_LIFE, bat_cmd_write_phone_bat_life, NULL, 0},
	{BAT_CMD_BAT_PRESENT, bat_cmd_write_phone_bat_present, NULL, 0},
};

struct asus_bat_proc_cmd bat_read_cmd_tbl[] = {
	{BAT_CMD_PHONE_ID, NULL, phone_bat_read_cmd_tbl, ARRAY_SIZE(phone_bat_read_cmd_tbl)},
	{BAT_CMD_ALL_INFO, bat_cmd_read_all_info, NULL, 0},
};

#ifdef CONFIG_ASUS_POWER_UTIL
#include "util/AXC_DoitLaterFactory.h"
static AXI_DoitLater* gpDoitLater = NULL ;
static AXI_DoitLaterTask *gpDoitLaterTask = NULL;
static void gDoitLaterTask_dotask(struct AXI_DoitLaterTask *task)
{
    static unsigned int times = 0;

    printk("%s times = %d",__FUNCTION__,times++);

    msleep(100);

    gpDoitLater->start(gpDoitLater,gpDoitLaterTask,0,1);

}
static AXI_DoitLaterTask gDoitLaterTask = {
    .dotask = gDoitLaterTask_dotask,
};

static void bat_cmd_enable_doitLater_test(const char *msg, int index)
{
    int val;

    if(NULL == gpDoitLater){

        gpDoitLater = get_DoitLater(E_ONE_SHOT_WORKER_TYPE);

        gpDoitLaterTask = &gDoitLaterTask;
    }


    index += BAT_CMD_INTERVAL;
    val = (int)simple_strtol(&msg[index], NULL, 10);

    printk("[BAT] %s(), msg[%d]=%c, val:%d \r\n", __FUNCTION__, index, msg[index], val);

    if (val >= 1) {

        printk("doitLater_test start\n");

        gpDoitLater->start(gpDoitLater,&gDoitLaterTask,0,1);
        
    } else {
    
        gpDoitLater->stop(gpDoitLater);

        printk("doitLater_test stop\n");
    }

    return ;
}
#include "util/AXC_FeedingFileInputTest.h"
#define TEST_BUF_SIZE (5)
#define CHECK_PATTERN (0x05)
static char gFeedingFileInputParser_Buf[TEST_BUF_SIZE];
static AXC_FeedingFileInputParser *gpFeedingFileInputParser = NULL;
static AXC_FeedingFileInputTest gFeedingFileInputTest;
void gFeedingFileInputParser_parse(struct AXC_FeedingFileInputParser *test, unsigned int size)
{
    int index = 0;

    BUG_ON(size != TEST_BUF_SIZE);  

    for(index = 0; index< TEST_BUF_SIZE; index++){

        BUG_ON(CHECK_PATTERN != gFeedingFileInputParser_Buf[index]);
    }    

    printk("feeding_file_input_test_test stop\n");

}
char * gFeedingFileInputParser_getBuffer(struct AXC_FeedingFileInputParser *test, unsigned int *size)
{
    *size = TEST_BUF_SIZE;

    return gFeedingFileInputParser_Buf;
}
static AXC_FeedingFileInputParser gFeedingFileInputParser = {
    .parse = gFeedingFileInputParser_parse,
    .getBuffer = gFeedingFileInputParser_getBuffer,
};

static void bat_cmd_enable_feeding_file_input_test(const char *msg, int index)
{
    int val;

    if(NULL == gpFeedingFileInputParser){

        gpFeedingFileInputParser = &gFeedingFileInputParser;

        AXC_FeedingFileInputTest_constructor(&gFeedingFileInputTest,
            "stress_test",
            gpFeedingFileInputParser);

        memset(gFeedingFileInputParser_Buf,CHECK_PATTERN,TEST_BUF_SIZE);

        gFeedingFileInputTest.mImpl.createFileForFeeding(&gFeedingFileInputTest,
            gFeedingFileInputParser_Buf,
            TEST_BUF_SIZE);
    }


    index += BAT_CMD_INTERVAL;
    val = (int)simple_strtol(&msg[index], NULL, 10);

    printk("[BAT] %s(), msg[%d]=%c, val:%d \r\n", __FUNCTION__, index, msg[index], val);

    if (val >= 1) {

        printk("feeding_file_input_test_test start\n");

        gFeedingFileInputTest.miParent.test(&gFeedingFileInputTest.miParent, NULL);
        
    } 
    
    return ;
}
#endif //#ifdef CONFIG_ASUS_POWER_UTIL
#ifdef CONFIG_CHARGER_ASUS_FSM
#include "fsm/AXC_FSM_Stress_Tester.h"
static void bat_cmd_enable_fsm_stress_test(const char *msg, int index)
{
	int val;

      AXC_FSM_Stress_Tester* lpTester = getFSM_Stress_Tester();

	index += BAT_CMD_INTERVAL;
	val = (int)simple_strtol(&msg[index], NULL, 10);

	printk("[BAT] %s(), msg[%d]=%c, val:%d \r\n", __FUNCTION__, index, msg[index], val);

	if (val >= 1) {
             lpTester->mImpl.enable(lpTester, true);
	} else {
	      lpTester->mImpl.enable(lpTester, false);
	}

	return ;
}
#include "fsm/AXC_Charging_FSM.h"

static void bat_cmd_enable_fsm_single_state_test(const char *msg, int index)
{
	int pre_state,event;

      AXC_Charging_FSM *lpFSM = getChargingFSM(E_ASUS_A66_FSM_CHARGING_TYPE,NULL);

	index += BAT_CMD_INTERVAL;
    
	pre_state = (int)simple_strtol(&msg[index], NULL, 10);

	index += BAT_CMD_INTERVAL;
    
	event = (int)simple_strtol(&msg[index], NULL, 10);

       lpFSM->currentState = (AXE_Charging_State)pre_state;

       //precondition
       switch((AXE_Charging_State)pre_state){
           case DISCHARGING_STATE:
            lpFSM->chargerType = NO_CHARGER_TYPE;
            lpFSM->lastChargingErrorReason = NO_ERROR;
            break;
           case CHARGING_STATE:
               lpFSM->chargerType = HIGH_CURRENT_CHARGER_TYPE;
               lpFSM->lastChargingErrorReason = NO_ERROR;
            break;
           case CHARGING_STOP_STATE:
               lpFSM->chargerType = HIGH_CURRENT_CHARGER_TYPE;
               lpFSM->lastChargingErrorReason = HOT_CHARGER_ERROR;
            break;
           case CHARGING_FULL_STATE:
               lpFSM->chargerType = HIGH_CURRENT_CHARGER_TYPE;
               lpFSM->lastChargingErrorReason = NO_ERROR;
            break;
           case CHARGING_FULL_KEEP_STATE:
               lpFSM->chargerType = NORMAL_CURRENT_CHARGER_TYPE;
               lpFSM->lastChargingErrorReason = NO_ERROR;
            break;
           case CHARGING_FULL_KEEP_STOP_STATE:
               lpFSM->chargerType = HIGH_CURRENT_CHARGER_TYPE;
               lpFSM->lastChargingErrorReason = UNKNOWN_ERROR;
            break;
            default:
                printk("Can't mapping any state\n");
                return;
                break;
       }
       //event
        switch((AXE_Charger_Event)event){
            case CABLE_IN_EVENT:
                lpFSM->onCableInOut(lpFSM,LOW_CURRENT_CHARGER_TYPE);
             break;
            case CABLE_OUT_EVENT:
                lpFSM->onCableInOut(lpFSM,NO_CHARGER_TYPE);
             break;
            case CHARGING_DONE_EVENT:
                lpFSM->onChargingStop(lpFSM,CHARGING_DONE);
             break;
            case CHARGING_RESUME_EVENT:
                lpFSM->onChargingStart(lpFSM);
             break;
            case CHARGING_STOP_EVENT:
                lpFSM->onChargingStop(lpFSM,CHARGING_TIMEOUT_ERROR);
             break;
             default:
                 printk("Can't mapping any event\n");
                 return;
                 break;
        }

       printk("FSM state = %d, type=%d, last errorcode=%d"
        ,lpFSM->currentState
        ,lpFSM->chargerType
        ,lpFSM->lastChargingErrorReason);

	return ;
}




#endif // #ifdef CONFIG_CHARGER_ASUS_FSM

#ifdef CONFIG_BATTERY_ASUS_SERVICE
static int charging_current = -1;
static void changeChargingCurrent(struct AXI_BatteryServiceFacadeCallback *callback,AXE_Charger_Type chargertype)
{
    switch(chargertype){

        case NO_CHARGER_TYPE:
            charging_current = 0;
            break;
        case ILLEGAL_CHARGER_TYPE:            
        case LOW_CURRENT_CHARGER_TYPE:
            charging_current = 500;
            break;
        case NORMAL_CURRENT_CHARGER_TYPE:
            charging_current = 1000;
            break;
        case HIGH_CURRENT_CHARGER_TYPE:
            charging_current = 1500;
            break;
        default:
            charging_current = -1;
            break;
    }

}
static void onServiceStatusUpdated(struct AXI_BatteryServiceFacadeCallback *callback)
{

}
static AXI_BatteryServiceFacadeCallback gServiceCallback = {
   . changeChargingCurrent = changeChargingCurrent,
    .onServiceStatusUpdated = onServiceStatusUpdated,
};
static void bat_cmd_enable_service_stress_test(const char *msg, int index)
{
	int val;

       AXI_BatteryServiceFacade *lpService = getBatteryService(&gServiceCallback);

	index += BAT_CMD_INTERVAL;
	val = (int)simple_strtol(&msg[index], NULL, 10);

	printk("[BAT] %s(), msg[%d]=%c, val:%d \r\n", __FUNCTION__, index, msg[index], val);

	if (val >= 1) {
            lpService->onCableInOut(lpService,NO_CHARGER_TYPE);
            BUG_ON(lpService->getChargingStatus(lpService) != BAT_DISCHARGING_STATUS);
            BUG_ON(0  != charging_current);
            lpService->onCableInOut(lpService,LOW_CURRENT_CHARGER_TYPE);
            BUG_ON(lpService->getChargingStatus(lpService) != BAT_CHARGING_STATUS);
            BUG_ON(500  != charging_current);
            lpService->onCableInOut(lpService,NO_CHARGER_TYPE);
            lpService->onCableInOut(lpService,NORMAL_CURRENT_CHARGER_TYPE);
            BUG_ON(lpService->getChargingStatus(lpService) != BAT_CHARGING_STATUS);
            BUG_ON(1000  != charging_current);
            lpService->onCableInOut(lpService,NO_CHARGER_TYPE);
            lpService->onCableInOut(lpService,HIGH_CURRENT_CHARGER_TYPE);
            BUG_ON(lpService->getChargingStatus(lpService) != BAT_CHARGING_STATUS);
            BUG_ON(1500 != charging_current);
            lpService->onChargingStop(lpService,CHARGING_DONE);
            BUG_ON(lpService->getChargingStatus(lpService) != BAT_CHARGING_FULL_STATUS);
            BUG_ON(0 != charging_current);             
	} else {
	      
	}

	return ;
}
#endif


//ASUS_BSP +++ Josh_Liao "sw gauge v2"
static void bat_cmd_test_swg_cap(const char *msg, int index)
{
	int val;

	index += BAT_CMD_INTERVAL;
	val = (int)simple_strtol(&msg[index], NULL, 10);

	printk("[swg] %s(), msg[%d]=%c, val:%d \r\n", __FUNCTION__, index, msg[index], val);

	test_gauge_askCapacity();

	return ;
}

static void bat_cmd_test_swg_cable(const char *msg, int index)
{
	int val;

	index += BAT_CMD_INTERVAL;
	val = (int)simple_strtol(&msg[index], NULL, 10);

	printk("[swg] %s(), msg[%d]=%c, val:%d \r\n", __FUNCTION__, index, msg[index], val);

	if (1 == val) {
		test_gauge_notifyCableInOut(true);
	} else if (0 == val) {
		test_gauge_notifyCableInOut(false);
	}
	
	return;
}

static void bat_cmd_test_swg_bat_full(const char *msg, int index)
{
	int val;

	index += BAT_CMD_INTERVAL;
	val = (int)simple_strtol(&msg[index], NULL, 10);

	printk("[swg] %s(), msg[%d]=%c, val:%d \r\n", __FUNCTION__, index, msg[index], val);

	if (1 == val) {
		test_gauge_notifyBatFullChanged(true);
	} else if (0 == val) {
		test_gauge_notifyBatFullChanged(false);
	}

	return;
}


static void bat_cmd_test_swg_filter_cap(const char *msg, int index)
{
	int val;

	index += BAT_CMD_INTERVAL;
	val = (int)simple_strtol(&msg[index], NULL, 10);

	printk("[swg] %s(), msg[%d]=%c, val:%d \r\n", __FUNCTION__, index, msg[index], val);

	test_gauge_filterCap();
	
	return;
}

//ASUS_BSP +++ Eason_Chang BalanceMode


static void bat_balance_isbalTest(const char *msg, int index)
{
	int val;

	index += BAT_CMD_INTERVAL;
	val = (int)simple_strtol(&msg[index], NULL, 10);

	printk("[BAT][Bal] %s(), msg[%d]=%c, val:%d \r\n", __FUNCTION__, index, msg[index], val);
    printk("[BAT][Bal]isTest:%d\n",val);
	BatteryService_BalanceMode_IsBalTest = val;
	
	return;
}


static void bat_balance_stopratio(const char *msg, int index)  
{
	int val;

	index += BAT_CMD_INTERVAL;
	val = (int)simple_strtol(&msg[index], NULL, 10);

	printk("[BAT][Bal] %s(), msg[%d]=%c, val:%d \r\n", __FUNCTION__, index, msg[index], val);
    printk("[BAT][Bal]stopR:%d\n",val);
	BatteryService_BalanceMode_StopRatio = val;
	
	return;
}    


static void bat_balance_startratio(const char *msg, int index)  
{
	int val;

	index += BAT_CMD_INTERVAL;
	val = (int)simple_strtol(&msg[index], NULL, 10);

	printk("[BAT][Bal] %s(), msg[%d]=%c, val:%d \r\n", __FUNCTION__, index, msg[index], val);
    printk("[BAT][Bal]startR:%d\n",val);
   	BatteryService_BalanceMode_StartRatio = val;
	
	return;
} 

int IsBalanceTest(void){
    return BatteryService_BalanceMode_IsBalTest;
}    

int GetBalanceModeStopRatio(void){
    return BatteryService_BalanceMode_StopRatio;
} 

int GetBalanceModeStartRatio(void){
    return BatteryService_BalanceMode_StartRatio;
} 

int GetBalanceModeA66CAP(void){
    return BatteryService_BalanceMode_A66_CAP;
}

//ASUS_BSP --- Eason_Chang BalanceMode

//ASUS_BSP --- Josh_Liao "sw gauge v2"

struct asus_bat_proc_cmd bat_stress_test_cmd_tbl[] = {
#ifdef CONFIG_CHARGER_ASUS_FSM
	{BAT_CMD_ENABLE_FSM_STRESS_TEST, bat_cmd_enable_fsm_stress_test, NULL, 0},
	{BAT_CMD_ENABLE_FSM_SINGLE_STATE_TEST, bat_cmd_enable_fsm_single_state_test, NULL, 0},
#endif
#ifdef CONFIG_ASUS_POWER_UTIL
       {BAT_CMD_ENABLE_DOITLATER_TEST,bat_cmd_enable_doitLater_test, NULL, 0},
       {BAT_CMD_ENABLE_FEEDING_FILE_INPUT_TEST,bat_cmd_enable_feeding_file_input_test, NULL, 0},
#endif
#ifdef CONFIG_BATTERY_ASUS_SERVICE
       {BAT_CMD_ENABLE_SERVICE_TEST,bat_cmd_enable_service_stress_test, NULL, 0},
#endif

};

//ASUS_BSP +++ Josh_Liao "sw gauge v2"
struct asus_bat_proc_cmd bat_swg_test_cmd_tbl[] = {
	{BAT_CMD_SWG_CAP, bat_cmd_test_swg_cap, NULL, 0},
	{BAT_CMD_SWG_CABLE, bat_cmd_test_swg_cable, NULL, 0},
	{BAT_CMD_SWG_BAT_FULL, bat_cmd_test_swg_bat_full, NULL, 0},	
	{BAT_CMD_SWG_FILTER_CAP, bat_cmd_test_swg_filter_cap, NULL, 0},		
};
//ASUS_BSP --- Josh_Liao "sw gauge v2"


//ASUS_BSP +++ Eason_Chang BalanceMode
struct asus_bat_proc_cmd bat_balance_test_cmd_tbl[] = {	
    {BAT_CMD_BALANCE_ISTEST, bat_balance_isbalTest, NULL, 0},    
    {BAT_CMD_BALANCE_STARTRATIO, bat_balance_startratio, NULL, 0},
    {BAT_CMD_BALANCE_STOPRATIO, bat_balance_stopratio, NULL, 0},
};    
//ASUS_BSP --- Eason_Chang BalanceMode

struct asus_bat_proc_cmd bat_write_cmd_tbl[] = {
	{BAT_CMD_PHONE_ID, NULL, phone_bat_write_cmd_tbl, ARRAY_SIZE(phone_bat_write_cmd_tbl)},
#ifdef CONFIG_BATTERY_ASUS_SERVICE
	{BAT_CMD_FIX_INTERVAL, bat_cmd_fix_interval, NULL, 0},
	{BAT_CMD_FIX_LAST_UPDATE_INTERVAL, bat_cmd_fix_last_update_interval, NULL, 0},
#endif
#ifdef CONFIG_PM8921_CHARGER
        {BAT_CMD_CHG_STATE_MONITOR_INTERVAL, bat_cmd_fix_chg_state_monitor_interval, NULL, 0},
#endif
	{BAT_CMD_ENABLE_TEST, bat_cmd_write_test, NULL, 0},
#ifdef CONFIG_CHARGER_ASUS_FSM
   {BAT_CMD_ENABLE_UNIT_TEST, NULL,bat_stress_test_cmd_tbl, ARRAY_SIZE(bat_stress_test_cmd_tbl)},
#endif
//ASUS_BSP +++ Josh_Liao "sw gauge v2"
   {BAT_CMD_TEST_SWG, NULL,bat_swg_test_cmd_tbl, ARRAY_SIZE(bat_swg_test_cmd_tbl)},
//ASUS_BSP --- Josh_Liao "sw gauge v2"

//ASUS_BSP +++ Eason_Chang BalanceMode
   {BAT_CMD_ENABLE_BALANCE, NULL,bat_balance_test_cmd_tbl, ARRAY_SIZE(bat_balance_test_cmd_tbl)},
//ASUS_BSP --- Eason_Chang BalanceMode

};

struct asus_bat_proc_cmd phone_bat_set_cmd_tbl[] = {
	{BAT_CMD_CHARGING_MODE, bat_cmd_set_phone_charging_mode, NULL, 0},
};

struct asus_bat_proc_cmd bat_set_cmd_tbl[] = {
	{BAT_CMD_PHONE_ID, NULL, phone_bat_set_cmd_tbl, ARRAY_SIZE(phone_bat_set_cmd_tbl)},
};

struct asus_bat_proc_cmd bat_cmd_tbl[] = {
	{ BAT_CMD_READ, NULL, bat_read_cmd_tbl, ARRAY_SIZE(bat_read_cmd_tbl) },	// read cmd
	{ BAT_CMD_HELP, bat_cmd_get_help, NULL, 0},								// help cmd
	{ BAT_CMD_WRITE, NULL, bat_write_cmd_tbl, ARRAY_SIZE(bat_write_cmd_tbl)},	// write cmd
	{ BAT_CMD_SET, NULL, bat_set_cmd_tbl, ARRAY_SIZE(bat_set_cmd_tbl)},	// set cmd	
	{ BAT_CMD_UPDATE, bat_cmd_update, NULL, 0},
};

// Asus BSP Eason_Chang +++ function for AXI_BatteryServiceFacade
extern void asus_bat_status_change(void);

static int asus_getChargingStatus(void)
{  
   int result = POWER_SUPPLY_STATUS_DISCHARGING; 
   AXE_BAT_CHARGING_STATUS status = loService->getChargingStatus(loService);
   
   switch(status){
   case BAT_CHARGING_STATUS:
			//Eason: when Phone 100% can't show charging, workaround when in Pad 100% unplug then plug extChg show charge icon+++
			if(100==loService->getCapacity(loService))
			{
				pr_debug("[BAT]A66 100p cant show CHARGING\n");
				result = POWER_SUPPLY_STATUS_FULL;
			}else{
				result = POWER_SUPPLY_STATUS_CHARGING;
			}
			//Eason: when Phone 100% can't show charging, workaround when in Pad 100% unplug then plug extChg show charge icon ---
                break;
   case BAT_DISCHARGING_STATUS:
             	result = POWER_SUPPLY_STATUS_DISCHARGING;
                break;
   case BAT_NOT_CHARGING_STATUS:
             	result = POWER_SUPPLY_STATUS_NOT_CHARGING;
                break;             
   case BAT_CHARGING_FULL_STATUS:
	            result = POWER_SUPPLY_STATUS_FULL;
                break;
   default:
                printk("[BAT]%s():ERROR mapping\n",__FUNCTION__);
    
   }

   //Eason if doesn't get correct ADC Vol&Curr at first update Cap show unknow status +++ 
   if(false == g_adc_get_correct_VolCurr)
   {
        result = POWER_SUPPLY_STATUS_UNKNOWN;
        printk("[BAT]:cannot get correct ADC Vol&Curr show UNKNOW\n");
   }
   //Eason if doesn't get correct ADC Vol&Curr at first update Cap show unknow status ---
   
   return result;
}

static int asus_getCapacity(void)
{
	int cap = 50;

//Eason: prevent charger mode get capacity at qpnp-charger.c before asus_bat.c ready+++
	if(NULL==loService)
	{
		printk("[BAT]Cap not ready, report fake 50\n");
		return 50;
	}	
//Eason: prevent charger mode get capacity at qpnp-charger.c before asus_bat.c ready---

	cap = loService->getCapacity(loService);
	pr_debug("[BAT]Cap:%d\n", cap);

	return cap;
}

//Eason repoert capacity to gauge for judge interval+++
int ReportCapToGaugeForJudgeInterval(void)
{
	return asus_getCapacity();
}
//Eason repoert capacity to gauge for judge interval---

//Eason: AICL work around +++
#ifdef CONFIG_SMB_346_CHARGER
int smb346_getCapacity(void)
{
	int phone_soc;

	phone_soc = asus_getCapacity();
	return phone_soc;
}
#endif
//Eason: AICL work around ---

int pm8226_getCapacity(void)
{
	return asus_getCapacity();
}
	
#include "AXI_Basic_Define.h"
void asus_onCableInOut(enum asus_chg_src src)
{
    if(NULL == loService){

        return;
    }

    switch(src){ 
		case ASUS_CHG_SRC_NONE:
            loService->onCableInOut(loService,NO_CHARGER_TYPE);
            break;
        case ASUS_CHG_SRC_DC:
            loService->onCableInOut(loService,HIGH_CURRENT_CHARGER_TYPE);
            break;
        case ASUS_CHG_SRC_UNKNOWN:
            loService->onCableInOut(loService,ILLEGAL_CHARGER_TYPE);
            break;
		case ASUS_CHG_SRC_USB:
			loService->onCableInOut(loService,LOW_CURRENT_CHARGER_TYPE);
			break;
        default:
            printk("[BAT]%s():error src\n",__FUNCTION__);
            break;
    }
}

void asus_onChargingStart(void)
{   
    loService->onChargingStart(loService);
}


static void asus_onBatterySuspend(void)
{
    loService->suspend(loService);
}

static void asus_onBatteryResume(void)
{
    loService->resume(loService, 0);
}

void AfterthawProcessResumeBatteryService(void)
{   
    printk("[BAT]thaw Process\n");
    asus_onBatteryResume();
}


static void AsusBatChangeChargingCurrent(struct AXI_BatteryServiceFacadeCallback *callback,AXE_Charger_Type chargertype)
{
//ASUS BSP Eason_Chang : A86 porting +++
#if 0
    switch(chargertype){

        case NO_CHARGER_TYPE:
			asus_chg_set_chg_mode_forBatteryservice(ASUS_CHG_SRC_NONE);
            break;
        case ILLEGAL_CHARGER_TYPE:
            asus_chg_set_chg_mode_forBatteryservice(ASUS_CHG_SRC_UNKNOWN);
            break;
        case LOW_CURRENT_CHARGER_TYPE:
            asus_chg_set_chg_mode_forBatteryservice(ASUS_CHG_SRC_USB);
            break;
        case NORMAL_CURRENT_CHARGER_TYPE:
            break;
        case HIGH_CURRENT_CHARGER_TYPE:
            asus_chg_set_chg_mode_forBatteryservice(ASUS_CHG_SRC_DC);
            break;
        default:
            printk("[BAT]%s():error chargertype\n",__FUNCTION__);
            break;
    }
#endif
//ASUS BSP Eason_Chang : A86 porting ---
}

extern int get_temp_for_ASUSswgauge(void);
extern int get_voltage_for_ASUSswgauge(void);
extern int get_current_for_ASUSswgauge(void);
static int getVBAT(struct AXI_BatteryServiceFacadeCallback *callback)
{
	return get_voltage_for_ASUSswgauge();
}
static int getIBAT(struct AXI_BatteryServiceFacadeCallback *callback)
{
	return get_current_for_ASUSswgauge();
}
static int getTempBAT(struct AXI_BatteryServiceFacadeCallback *callback)
{
	return get_temp_for_ASUSswgauge();
}
static void AsusBatOnServiceStatusUpdated(struct AXI_BatteryServiceFacadeCallback *callback)
{
   asus_bat_status_change();
}
static AXI_BatteryServiceFacadeCallback goServiceCallback = {
    .changeChargingCurrent = AsusBatChangeChargingCurrent,
    .onServiceStatusUpdated = AsusBatOnServiceStatusUpdated,
    .getVBAT = getVBAT,
    .getIBAT = getIBAT,
    .getTempBAT = getTempBAT,
};

// Asus BSP Eason_Chang --- function for AXI_BatteryServiceFacade


/* asus_bat_handle_cmd_tbl */
static void asus_bat_handle_cmd_tbl(
	const char *msg, 
	int index,
	struct asus_bat_proc_cmd *cmd_tbl,
	int cmd_tbl_size)
{
	int i;

	printk(DBGMSK_BAT_INFO "[BAT] %s(), cmd:%c, idx:%d, tbl size:%d \r\n", 
		__FUNCTION__, msg[index], index, cmd_tbl_size);

	for (i = 0; i < cmd_tbl_size; i++) {
		if (msg[index] == cmd_tbl[i].id) {
			if (NULL != cmd_tbl[i].cmd_tbl) { 
				index += BAT_CMD_INTERVAL;
				asus_bat_handle_cmd_tbl(msg, index, cmd_tbl[i].cmd_tbl, cmd_tbl[i].cmd_tbl_size);
				return;
			} else if (NULL != cmd_tbl[i].func) {
				cmd_tbl[i].func(msg, index);
				return;
			} else {
				printk(DBGMSK_BAT_ERR "[BAT] %s(), no func tbl", __FUNCTION__);
				return;
			}
		}
	}

	if (i == cmd_tbl_size) {
		printk(DBGMSK_BAT_ERR "[BAT]error!! in %s(), no cmd found in tbl", __FUNCTION__);
	}

	return;
}

void asus_bat_report_phone_charger(enum asus_bat_charger_cable charger)
{
	if (NULL == asus_bat)
		return;

	// set charger
	if (!asus_bat->enable_test) {
		asus_bat_set_phone_charger(charger);
		pr_debug( "[BAT] %s(), charger:%s \r\n", __FUNCTION__, charger_txt[charger]);
	}
	return;
}

int asus_bat_report_phone_capacity(int phone_bat_capacity)
{
    pr_debug("%s()+++",__FUNCTION__);
	if (NULL == asus_bat) {
		return phone_bat_capacity;
	}

	if (!asus_bat->enable_test)
		asus_bat_set_phone_bat_capacity(phone_bat_capacity);

//Eason takeoff Battery shutdown +++	
	if(true == g_AcUsbOnline_Change0)
    {
        printk("[BAT]Cap:change capacity 0 to shutdown\n");
        return 0;
    }else{    
        return asus_getCapacity();
    }    
//Eason takeoff Battery shutdown ---    
}
EXPORT_SYMBOL_GPL(asus_bat_report_phone_capacity);


int asus_bat_report_phone_status(int phone_bat_status)
{

	if(NULL == asus_bat) {
		return phone_bat_status;
	}

	if (!asus_bat->enable_test) {		// from hw
		asus_bat_set_phone_bat_status(phone_bat_status);
	} else {		// from variable
		switch (asus_bat_get_phone_charger()) {
		case ASUS_BAT_CHARGER_NONE:
			phone_bat_status = POWER_SUPPLY_STATUS_DISCHARGING;
			break;	
		case ASUS_BAT_CHARGER_USB:
		case ASUS_BAT_CHARGER_AC:	
			if (ASUS_BAT_CHARGING_ONGOING == asus_bat_get_phone_charging()) {
				phone_bat_status = POWER_SUPPLY_STATUS_CHARGING;
			} else {
				phone_bat_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
			}
			break;
		default:
			phone_bat_status = POWER_SUPPLY_STATUS_UNKNOWN;
			printk(DBGMSK_BAT_ERR "[BAT] error !! in %s() \r\n", __FUNCTION__);
		}
	}

	pr_debug(DBGMSK_BAT_INFO "[BAT] %s(), ps charing:%s \r\n", __FUNCTION__,ps_charging_txt[phone_bat_status] );
	
	//return phone_bat_status;
	return asus_getChargingStatus();//
}
EXPORT_SYMBOL_GPL(asus_bat_report_phone_status);

int asus_bat_report_phone_present(int phone_bat_present)
{
	if(NULL == asus_bat) {
		return phone_bat_present;
	}

	if (!asus_bat->enable_test) {
		if (phone_bat_present > 0) {
			asus_bat_set_phone_bat_present(true);
		} else {
			asus_bat_set_phone_bat_present(false);
		}
	}

	if (asus_bat_is_phone_bat_present()) {
		return 1;
	} else {
		return 0;
	}
}
EXPORT_SYMBOL_GPL(asus_bat_report_phone_present);

// Just for pad
static void asus_bat_update_all_bat(void)
{
	pr_debug( "[BAT] %s()n", __FUNCTION__);

	return;
}

extern void pm8921_chg_set_chg_mode(enum asus_chg_src chg_src);
#ifdef CONFIG_CHARGER_ASUS
extern void asus_chg_set_chg_mode(enum asus_chg_src chg_src)
{
    AXI_Charger *lpCharger;

	printk("%s,src=%d\n",__FUNCTION__,chg_src );

	lpCharger = getAsusCharger();

	switch (chg_src) {
		case ASUS_CHG_SRC_USB:
			//printk("[BAT]SetCharger  LOW_CURRENT_CHARGER_TYPE\n");
			lpCharger->SetCharger(lpCharger,LOW_CURRENT_CHARGER_TYPE);
			break;
		case ASUS_CHG_SRC_DC:
			//printk("[BAT]SetCharger  HIGH_CURRENT_CHARGER_TYPE\n");
			lpCharger->SetCharger(lpCharger,HIGH_CURRENT_CHARGER_TYPE);
			break;
		case ASUS_CHG_SRC_NONE:
			//printk("[BAT]SetCharger  NO_CHARGER_TYPE\n");
			lpCharger->SetCharger(lpCharger,NO_CHARGER_TYPE);
			break;
		case ASUS_CHG_SRC_UNKNOWN:
			//printk("[BAT]SetCharger  ILLEGAL_CHARGER_TYPE\n");
			lpCharger->SetCharger(lpCharger,ILLEGAL_CHARGER_TYPE);
			break;
		default:
			break;
	}
}
#endif
void asus_bat_set_phone_bat(struct asus_bat_phone_bat_struct *pbs)
{
	BUG_ON(NULL == pbs);
	pr_debug( "[BAT] %s()", __FUNCTION__);

	phone_bat_info= pbs;

	return;
}
EXPORT_SYMBOL_GPL(asus_bat_set_phone_bat);


/* asus_bat_periodic_update - periodically update battery status
  */
static void asus_bat_periodic_update(struct work_struct *work)
{
	if (!g_done_first_periodic_update) {
		g_done_first_periodic_update = true;
		printk(DBGMSK_BAT_INFO "[BAT] done first periodic update \r\n");
	}

	asus_bat_update_all_bat();

	queue_delayed_work(asus_bat_wq,
		&asus_bat->bat_update_work, (asus_bat->bat_update_work_interval)*HZ);
}


static struct proc_dir_entry *bat_proc_file = NULL;

static int asus_bat_proc_show(struct seq_file *seq, void *v)
{
	printk(DBGMSK_BAT_INFO "[BAT] %s() +++\n", __FUNCTION__);

	return 0;
}

static ssize_t asus_bat_write_proc(struct file *filp, const char __user *buff, size_t len, loff_t *pos)
{
	char msg[ASUS_BAT_PROC_MAX_BUFF_SIZE];
	if (len > ASUS_BAT_PROC_MAX_BUFF_SIZE)
		len = ASUS_BAT_PROC_MAX_BUFF_SIZE;

	printk(DBGMSK_BAT_INFO "[BAT] %s() +++\n", __FUNCTION__);

	if (copy_from_user(msg, buff, len))
		return -EFAULT;


	asus_bat_handle_cmd_tbl(msg, 0, bat_cmd_tbl, ARRAY_SIZE(bat_cmd_tbl));

	return len;
}

static void *asus_bat_proc_start(struct seq_file *seq, loff_t *pos)
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

static void *asus_bat_proc_next(struct seq_file *seq, void *v, loff_t *pos)
{
	return NULL;
}

static void asus_bat_proc_stop(struct seq_file *seq, void *v)
{
	
}

static const struct seq_operations asus_bat_proc_seq = {
	.start		= asus_bat_proc_start,
	.show		= asus_bat_proc_show,
	.next		= asus_bat_proc_next,
	.stop		= asus_bat_proc_stop,
};

static int asus_bat_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &asus_bat_proc_seq);
}

static const struct file_operations asus_bat_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= asus_bat_proc_open,
	.read		= seq_read,
	.write		= asus_bat_write_proc,
};

static void asus_bat_create_bat_proc_file(void)
{
	pr_debug( "[BAT] %s()+++", __FUNCTION__);

	bat_proc_file = proc_create(
		ASUS_BAT_PROC_FILE,
		ASUS_BAT_PROC_FILE_PERMISSION,
		NULL, &asus_bat_proc_fops);

	if (NULL == bat_proc_file) {
		printk(KERN_ERR "[BAT] bat proc file created failed!\n");
		return;
	}
	
	return;
}

//Eason: LowCapCpuThrottle +++
void notifyMpdecisionCpuThrottle(bool IsCpuThrottle)
{
	char *envp[3];
	envp[0] = "lowCapCpuThrottle_event";
	envp[1] = NULL;

	if(true == IsCpuThrottle)
	{
		kobject_uevent_env(kobj,KOBJ_OFFLINE,envp);	
		printk("[BAT][ASUS][Throttle]OFF\n");
	}
	else
	{
		kobject_uevent_env(kobj,KOBJ_ONLINE,envp);
		printk("[BAT][Throttle]ON\n");
	}
}

static int LowCapCpuThrottle_proc_show(struct seq_file *seq, void *v)
{
	return seq_printf(seq, "%d\n", IsInCpuThrottle);//Eason:let mpdicision check cpu throttle status
}

static ssize_t LowCapCpuThrottle_write_proc(struct file *filp, const char __user *buff, size_t len, loff_t *pos)
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

	LowCapCpuThrottle = val;

	if(1==LowCapCpuThrottle)
	{
		notifyMpdecisionCpuThrottle(true);
		IsInCpuThrottle = true;
	}else{
		notifyMpdecisionCpuThrottle(false);
		IsInCpuThrottle = false;
	}

	printk("[BAT][ASUS][Throttle]mode:%d\n",val);

	return len;
}

static void *LowCapCpuThrottle_proc_start(struct seq_file *seq, loff_t *pos)
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

static void *LowCapCpuThrottle_proc_next(struct seq_file *seq, void *v, loff_t *pos)
{
	return NULL;
}

static void LowCapCpuThrottle_proc_stop(struct seq_file *seq, void *v)
{
	
}

static const struct seq_operations LowCapCpuThrottle_proc_seq = {
	.start		= LowCapCpuThrottle_proc_start,
	.show		= LowCapCpuThrottle_proc_show,
	.next		= LowCapCpuThrottle_proc_next,
	.stop		= LowCapCpuThrottle_proc_stop,
};

static int LowCapCpuThrottle_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &LowCapCpuThrottle_proc_seq);
}

static const struct file_operations LowCapCpuThrottle_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= LowCapCpuThrottle_proc_open,
	.read		= seq_read,
	.write		= LowCapCpuThrottle_write_proc,
};

void static create_LowCapCpuThrottle_proc_file(void)
{
	struct proc_dir_entry *LowCapCpuThrottle_proc_file = proc_create("driver/LowCapCpuThrottle", 0644, NULL, &LowCapCpuThrottle_proc_fops);

	if (!LowCapCpuThrottle_proc_file) {
		printk("[BAT][ASUS][Throttle]LowCapCpuThrottle proc file create failed!\n");
	}

	return;
}
//Eason: LowCapCpuThrottle ---

//Hank: add charge ststus proc file+++
/*
static ssize_t asus_bat_charge_status_read_proc(char *page, char **start, off_t off, int count, 
            	int *eof, void *data)
{
	if(asus_getChargingStatus() == POWER_SUPPLY_STATUS_FULL || asus_getChargingStatus() == POWER_SUPPLY_STATUS_CHARGING)
		return sprintf(page, "%d\n", 1);
	else
		return sprintf(page, "%d\n", 0);
}
static ssize_t asus_bat_charge_status_write_proc(struct file *filp, const char __user *buff, 
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


    
    	printk("[BAT]Do not support write\n");
	
	return len;
}

void static asus_bat_charge_status_create_proc_file(void)
{
	struct proc_dir_entry *chargeststus_proc_file = create_proc_entry("driver/charge_status", 0644, NULL);

	if (chargeststus_proc_file) {
		chargeststus_proc_file->read_proc = asus_bat_charge_status_read_proc;
		chargeststus_proc_file->write_proc = asus_bat_charge_status_write_proc;
	}
    else {
		printk("[BAT]proc file create failed!\n");
    }

	return;
}
*/
//Hank: add charge ststus proc file---



#define DEFAULT_THRESHOLD_LOWER		3400
#define DEFAULT_HOLD_TIME		PM8XXX_BATT_ALARM_HOLD_TIME_16_MS
#define DEFAULT_USE_PWM			1
#define DEFAULT_PWM_SCALER		9
#define DEFAULT_PWM_DIVIDER		8



static int asus_battery_suspend(struct device *dev)
{
    //printk("[BAT]suspend\r\n");
   
    asus_onBatterySuspend();
    return 0;
}

static int asus_battery_resume(struct device *dev)
{
	//printk("[BAT]resume\r\n");
    asus_onBatteryResume();
      
    return 0;
}

//Hank: 1025 use frame buffer notifier to implement earlysuspend+++
#ifdef CONFIG_FB
static void asus_onBatteryForceResume(void)
{
    loService->forceResume(loService, 0);
}

//ASUS BSP+++  Eason_Chang  SW gauge resume
static void asus_battery_early_suspend(void)
{
	printk("[BAT]asus_battery_early_suspend\r\n");
}

static void asus_battery_late_resume(void)
{
	printk("[BAT]late_resume\r\n");
	asus_onBatteryForceResume();
}

static int asus_bat_fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	static int blank_old = 0;
	int *blank;

	if (evdata && evdata->data && event == FB_EVENT_BLANK) {
		blank = evdata->data;
		if (*blank == FB_BLANK_UNBLANK) {
			if (blank_old == FB_BLANK_POWERDOWN) {
				blank_old = FB_BLANK_UNBLANK;
				asus_battery_late_resume();
			}
		} else if (*blank == FB_BLANK_POWERDOWN) {
			if (blank_old == 0 || blank_old == FB_BLANK_UNBLANK) {
				blank_old = FB_BLANK_POWERDOWN;
				asus_battery_early_suspend();
			}
		}
	}

	return 0;
}
//ASUS BSP---  Eason_Chang SW gauge resume
#endif //#ifdef CONFIG_FB
//Hank: 1025 use frame buffer notifier to implement earlysuspend---

#ifdef CONFIG_HAS_EARLYSUSPEND
static void asus_onBatteryForceResume(void)
{
    loService->forceResume(loService, 0);
}
//ASUS BSP+++  Eason_Chang  SW gauge resume
static void asus_battery_early_suspend(struct early_suspend *h)
{
	printk("[BAT]asus_battery_early_suspend\r\n");
}

static void asus_battery_late_resume(struct early_suspend *h)
{
	printk("[BAT]late_resume\r\n");
	asus_onBatteryForceResume();
}

struct early_suspend asus_battery_early_suspend_handler = {
    .level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN, 
    .suspend = asus_battery_early_suspend,
    .resume = asus_battery_late_resume,
};
//ASUS BSP---  Eason_Chang SW gauge resume
#endif //#ifdef CONFIG_HAS_EARLYSUSPEND

/* Network device notifier chain handler. */
static int bat_low_alarm(struct notifier_block *this, unsigned long event,
			     void *ptr)
{
	pr_debug( "[BAT] %s(), event: %ld \r\n", __FUNCTION__, event);

	pr_debug( "[BAT] phone bat curr from HW: %d \r\n", phone_bat_info->get_prop_batt_curr_byhw());

	if (POWER_SUPPLY_STATUS_CHARGING != phone_bat_info->get_prop_bat_status_byhw()) {
		printk(DBGMSK_BAT_INFO "[BAT]  %s(), set phone bat low as true\r\n", __FUNCTION__);
		asus_bat->phone_b.bat_low = true;
	} else {
		printk(DBGMSK_BAT_INFO "[BAT]  %s(), set phone bat low as false\r\n", __FUNCTION__);
		asus_bat->phone_b.bat_low = false;
	}

	phone_bat_info->phone_bat_power_supply_changed();

	return NOTIFY_DONE;
}


static struct notifier_block bat_alarm_notifier = {
	.notifier_call = bat_low_alarm,
};

static int asus_bat_bat_alarm_cfg(void)
{
	int rc;

	pr_debug( "[BAT] %s()+++ \r\n", __FUNCTION__);
			
	/* Use default values when no platform data is provided. */
	rc = pm8xxx_batt_alarm_threshold_set(PM8XXX_BATT_ALARM_LOWER_COMPARATOR,
		DEFAULT_THRESHOLD_LOWER);
	if (rc) {
		printk( DBGMSK_BAT_ERR "[BAT] threshold_set failed, rc=%d\n", rc);
		goto done;
	}

	rc = pm8xxx_batt_alarm_hold_time_set(DEFAULT_HOLD_TIME);
	if (rc) {
		printk( DBGMSK_BAT_ERR "[BAT] hold_time_set failed, rc=%d\n", rc);
		goto done;
	}

	rc = pm8xxx_batt_alarm_pwm_rate_set(DEFAULT_USE_PWM,
			DEFAULT_PWM_SCALER, DEFAULT_PWM_DIVIDER);
	if (rc) {
		printk( DBGMSK_BAT_ERR "[BAT] pwm_rate_set failed, rc=%d\n", rc);
		goto done;
	}

	rc = pm8xxx_batt_alarm_register_notifier(&bat_alarm_notifier);
	if (rc) {
		printk( DBGMSK_BAT_ERR "[BAT] batt alarm register failed, rc=%d\n", rc);
		goto done;
	}

	rc = pm8xxx_batt_alarm_enable(PM8XXX_BATT_ALARM_LOWER_COMPARATOR);
	if (rc) {
		printk( DBGMSK_BAT_ERR"[BAT] disable lower failed, rc=%d\n", rc);
		goto done;
	}

done:
	return rc;

}

static int asus_bat_probe(struct platform_device *pdev)
{
	int err=-1;

// Asus BSP Eason_Chang +++ function for AXI_BatteryServiceFacade
	AXI_BatteryServiceFacade *batteryloService = getBatteryService(&goServiceCallback);
	loService = batteryloService;
// Asus BSP Eason_Chang --- function for AXI_BatteryServiceFacade
	
	printk( "[BAT] %s() +++\n", __FUNCTION__);

	asus_bat = kzalloc(sizeof(struct asus_bat_all_info), GFP_KERNEL);
	if (!asus_bat) {
		printk(KERN_ERR "[BAT] could not allocate memory\n");
		return -ENOMEM;
	}

	mutex_init(&asus_bat->hs_evt_lock);
	mutex_init(&asus_bat->microp_evt_lock);


	asus_bat->bat_update_work_interval = ASUS_BAT_DEF_UPDATE_RATE;

	INIT_DELAYED_WORK(&asus_bat->bat_update_work, asus_bat_periodic_update);

	asus_bat_wq = create_singlethread_workqueue("asus_bat_workqueue");
	if (!asus_bat_wq) {
		err= -ESRCH;
		goto free_asus_bat_wq;
	}

	queue_delayed_work(asus_bat_wq, &asus_bat->bat_update_work, HZ * BAT_FIRST_UPDATE_DELAY);

	/* set bat low alarm */
	if (0) {
		asus_bat_bat_alarm_cfg();
	}

	//Eason Calculate Cap after judge BatLow+++
	loService ->forceResume(loService,TIME_FOR_FIRST_START_BATTERY_SERVER_IN_SECONDS);
	//Eason Calculate Cap after judge BatLow---

	asus_bat_create_bat_proc_file();
	//Hank: add charge status proc file+++
	//asus_bat_charge_status_create_proc_file();
	//Hank: add charge status proc file---
	//Eason: LowCapCpuThrottle +++
	create_LowCapCpuThrottle_proc_file();
	//Eason: LowCapCpuThrottle ---

//ASUS BSP+++  Eason_Chang  SW gauge resume
#if defined(CONFIG_HAS_EARLYSUSPEND)
	register_early_suspend(&asus_battery_early_suspend_handler);
//ASUS BSP---  Eason_Chang  SW gauge resume
//Hank: 1025 use frame buffer notifier to implement earlysuspend+++
#elif defined(CONFIG_FB)
	bat_fb_notif.notifier_call = asus_bat_fb_notifier_callback;
	err= fb_register_client(&bat_fb_notif);
	if (err)
		dev_err(&pdev->dev, "Unable to register fb_notifier: %d\n", err);
#endif
//Hank: 1025 use frame buffer notifier to implement earlysuspend---
    

//ASUS BSP Eason Chang coincell+++
	pm8xxx_coincell_chg_config(&bat_chg_config);
//ASUS BSP Eason Chang coincell---

	//Eason Send uevent to userspace+++
	kobj = &pdev->dev.kobj;
	//Eason Send uevent to userspace---

	printk( "[BAT] %s() ---\n", __FUNCTION__);
	
	return 0;


free_asus_bat_wq:
	cancel_delayed_work(&asus_bat->bat_update_work);
	destroy_workqueue(asus_bat_wq);
	kfree(asus_bat);
	return err;
}

static int asus_bat_remove(struct platform_device *pdev)
{
	cancel_delayed_work(&asus_bat->bat_update_work);
	destroy_workqueue(asus_bat_wq);
	kfree(asus_bat);
	asus_bat = NULL;
	return 0;
}

static const struct dev_pm_ops asusbat_pm_ops = {
    .suspend = asus_battery_suspend,
    .resume = asus_battery_resume,
};

static struct platform_driver asus_bat_driver = {
	.probe	= asus_bat_probe,
	.remove	= asus_bat_remove,
	.driver	= {
		.name	= "asus_bat",
		.owner	= THIS_MODULE,
		.pm	= &asusbat_pm_ops,
	},
};

static int __init asus_bat_init(void)
{
	return platform_driver_register(&asus_bat_driver);
}

static void __exit asus_bat_exit(void)
{
	platform_driver_unregister(&asus_bat_driver);
}

late_initcall(asus_bat_init);
module_exit(asus_bat_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ASUS battery virtual driver");
MODULE_VERSION("1.0");
MODULE_AUTHOR("Josh Liao <josh_liao@asus.com>");

//ASUS_BSP --- Josh_Liao "add asus battery driver"

