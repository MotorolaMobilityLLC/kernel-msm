/*
        Qualcomm PM8226 Charger IC Implementation

*/
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/power_supply.h>
#include <asm/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/irq.h>
#include <linux/wakelock.h>
#include <linux/interrupt.h>
#include <linux/asus_bat.h>
#include <linux/asus_chg.h>

#include "axc_PM8226Charger.h"
#include "axc_chargerfactory.h"

//Eason takeoff Battery shutdown +++
extern bool g_AcUsbOnline_Change0;
//Eason takeoff Battery shutdown ---

//Eason : when thermal too hot, limit charging current +++ 
//thermal limit level 0 : no limit
//thermal limit level 1: hot and low battery
//thermal limit level 2: hot but not very low battery
//thermal limit level 3: hot but no low battery
int g_thermal_limit=0;
bool g_audio_limit = false;
//Eason : when thermal too hot, limit charging current ---

extern int pm8226_getCapacity(void);

//Eason: AICL work around +++
#ifndef ASUS_FACTORY_BUILD
static bool g_chgTypeBeenSet = false;//Eason : prevent setChgDrawCurrent before get chgType
#endif
#include <linux/jiffies.h>
AXE_Charger_Type lastTimeCableType = NO_CHARGER_TYPE;
#define ADAPTER_PROTECT_DELAY (7*HZ)
//Eason: AICL work around ---

//Eason: Factory5060Mode+++
#ifdef ASUS_FACTORY_BUILD
extern bool g_5060modeCharging;
#endif
//Eason: Factory5060Mode---

void create_charger_proc_file(void);

extern void pm8226_chg_enable_charging(bool enable);
extern int pm8226_is_ac_usb_in(void);
extern void pm8226_chg_usb_suspend_enable(int enable);
extern int pm8226_get_prop_batt_status(void);

static void (*notify_charger_in_out_func_ptr)(int) = NULL;
AXC_PM8226Charger *gpCharger = NULL;

static bool isAcUsbPsyRegst = 0;
//ASUS_BSP porting charger mode +++
#if defined(ASUS_CHARGING_MODE) && !defined(ASUS_FACTORY_BUILD)
extern int g_chg_present;
#endif
//ASUS_BSP porting charger mode ---

//Eason: schedule work to check cable status later+++
static struct delayed_work DetectChgWorker;
//Eason: schedule work to check cable status later---

static int pm8226chg_proc_show(struct seq_file *seq, void *v)
{
	int status;
	
	status = pm8226_get_prop_batt_status();
	
	//defined in power_supply.h: unknown = 0, charging = 1, discharging = 2, not_charging = 3, full = 4
	//defined by ATD:            unknown = 1, charging = 2, discharging = 3, not_charging = 4, full = 5
	status++;
	return seq_printf(seq, "%d\n", status);
}

static void *pm8226chg_proc_start(struct seq_file *seq, loff_t *pos)
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

static void *pm8226chg_proc_next(struct seq_file *seq, void *v, loff_t *pos)
{
	return NULL;
}

static void pm8226chg_proc_stop(struct seq_file *seq, void *v)
{
	
}

static const struct seq_operations pm8226chg_proc_seq = {
	.start		= pm8226chg_proc_start,
	.show		= pm8226chg_proc_show,
	.next		= pm8226chg_proc_next,
	.stop		= pm8226chg_proc_stop,
};

static int pm8226chg_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &pm8226chg_proc_seq);
}

static const struct file_operations pm8226chg_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= pm8226chg_proc_open,
	.read		= seq_read,
};

void static create_pm8226_proc_file(void)
{
	struct proc_dir_entry *pm8226chg_proc_file = proc_create("driver/pm8226chg", 0644, NULL, &pm8226chg_proc_fops);

	if (!pm8226chg_proc_file) {
		printk("[BAT][Bal]proc file create failed!\n");
    }

	return;
}

static char *pm_power_supplied_to[] = {
	"battery",
};

static enum power_supply_property pm_power_props[] = {
    POWER_SUPPLY_PROP_PRESENT,
    POWER_SUPPLY_PROP_ONLINE,
    POWER_SUPPLY_PROP_CURRENT_MAX,
};

static int pm_power_get_property(struct power_supply *psy,
				  enum power_supply_property psp,
				  union power_supply_propval *val)
{
	AXE_Charger_Type type;

	if(NULL == gpCharger){
		val->intval = 0;
		return 0;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = 0;

		type =gpCharger->msParentCharger.GetChargerStatus(&gpCharger->msParentCharger);

		if(NO_CHARGER_TYPE < type){
			//Eason: in Pad AC powered, judge AC powered true+++		   
			if(psy->type == POWER_SUPPLY_TYPE_MAINS && 
					(type == HIGH_CURRENT_CHARGER_TYPE || type == ILLEGAL_CHARGER_TYPE))
			{
				val->intval = 1;
			}
			else if((psy->type == POWER_SUPPLY_TYPE_MAINS) && (type == NORMAL_CURRENT_CHARGER_TYPE))
			{

			}
			//Eason: in Pad AC powered, judge AC powered true---	

			if(psy->type ==POWER_SUPPLY_TYPE_USB && type == LOW_CURRENT_CHARGER_TYPE){            
				val->intval = 1;
			}
		}

		if(true == g_AcUsbOnline_Change0){
			val->intval = 0;
			printk("[BAT][Chg]: set online 0 to shutdown device\n");   
		}

		//Eason: Factory5060Mode+++
		#ifdef ASUS_FACTORY_BUILD
		if( false == g_5060modeCharging)
		{
			val->intval = 0;
			printk("[BAT][Factory][5060][usb_ac]: set online 0 to show Notcharging icon\n");   
		}
		#endif	
		//Eason: Factory5060Mode---
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = gpCharger->usb_current_max;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static struct power_supply usb_psy = {
	.name		= "usb",
	.type		= POWER_SUPPLY_TYPE_USB,
	.supplied_to = pm_power_supplied_to,
	.num_supplicants = ARRAY_SIZE(pm_power_supplied_to),
	.properties	= pm_power_props,
	.num_properties	= ARRAY_SIZE(pm_power_props),
	.get_property	= pm_power_get_property,
};

static struct power_supply main_psy = {
	.name		= "ac",
	.type		= POWER_SUPPLY_TYPE_MAINS,
	.supplied_to = pm_power_supplied_to,
	.num_supplicants = ARRAY_SIZE(pm_power_supplied_to),
	.properties	= pm_power_props,
	.num_properties	= ARRAY_SIZE(pm_power_props),
	.get_property	= pm_power_get_property,
};

//Eason: Do VF with Cable when boot up+++
int getIfonline_pm8226(void)
{
    return pm8226_is_ac_usb_in();
}
//Eason: Do VF with Cable when boot up---

//ASUS_BSP Eason_Chang BAT charger driver set usb path charge current +++ 
extern void asus_usbPath_chg_current_set(int mA);
//ASUS_BSP Eason_Chang BAT charger driver set usb path charge current ---

void AcUsbPowerSupplyChange_pm8226(void)
{
	if(isAcUsbPsyRegst){
		power_supply_changed(&usb_psy);
		power_supply_changed(&main_psy);
		printk("[BAT][CHG]:Ac Usb PowerSupply Change\n");
	}else{
		printk("[BAT][CHG]:Ac Usb PowerSupply is not registered\n");
	}
}

static void limitPM8226chg900(void)
{
	gpCharger->usb_current_max = 900*1000;
	asus_usbPath_chg_current_set(900);
}

#ifndef ASUS_FACTORY_BUILD
static void limitPM8226chg700(void)
{
	gpCharger->usb_current_max = 700*1000;
	asus_usbPath_chg_current_set(700);
}

static void limitPM8226chg500(void)
{
	gpCharger->usb_current_max = 500*1000;
	asus_usbPath_chg_current_set(500);
}
#endif

static void limitPM8226chg200(void)
{
	gpCharger->usb_current_max = 200*1000;
	asus_usbPath_chg_current_set(200);
}

static void defaultPM8226chgSetting(void)
{
	gpCharger->usb_current_max = 200*1000;// default:200mA
	asus_usbPath_chg_current_set(200);
}

//Eason : when thermal too hot, limit charging current +++ 
void setChgDrawCurrent_pm8226(void)
{
#ifndef ASUS_FACTORY_BUILD
	if(true == g_chgTypeBeenSet)//Eason : prevent setChgDrawCurrent_pm8226 before get chgType
	{
		if(ILLEGAL_CHARGER_TYPE==gpCharger->type){
			limitPM8226chg200();
			printk("[BAT][CHG][Illegal]: limit chgCur,  darw 200\n");
		}
		else if(NORMAL_CURRENT_CHARGER_TYPE==gpCharger->type){

		}
		else if(NOTDEFINE_TYPE==gpCharger->type || NO_CHARGER_TYPE==gpCharger->type){
			defaultPM8226chgSetting();
			printk("[BAT][CHG]:default 200mA\n");
		}
		else if( (3==g_thermal_limit)||(true==g_audio_limit) ){
			limitPM8226chg500();
			printk("[BAT][CHG]:limit charging current 500mA\n");
		}
		else if( 2==g_thermal_limit ){
			limitPM8226chg700();
			printk("[BAT][CHG]:limit charging current 700mA\n");
		}
		else if(1 == g_thermal_limit){
			limitPM8226chg900();
			printk("[BAT][CHG]:limit charging current 900mA\n");
		}
		else{
			if(LOW_CURRENT_CHARGER_TYPE==gpCharger->type){
				limitPM8226chg500();
				printk("[BAT][CHG][Low]: limit chgCur,  darw 500\n");
			}
			else if(HIGH_CURRENT_CHARGER_TYPE==gpCharger->type){
				limitPM8226chg900();
				printk("[BAT][CHG][AC]: limit charging current 900mA\n");
			}
		}
	}
#endif//#ifndef ASUS_FACTORY_BUILD
}
//Eason : when thermal too hot, limit charging current ---

//Eason: AICL work around +++
void setChgDrawACTypeCurrent_withCheckAICL_pm8226(void)
{
#ifndef ASUS_FACTORY_BUILD   
	if(NORMAL_CURRENT_CHARGER_TYPE==gpCharger->type){

	}
	else if( (3==g_thermal_limit)||(true==g_audio_limit) ){
		limitPM8226chg500();
		printk("[BAT][CHG]:limit charging current 500mA\n");
	}
	else if( 2 == g_thermal_limit){
		limitPM8226chg700();
		printk("[BAT][CHG]:limit charging current 900mA\n");
	}
	else if( 1 == g_thermal_limit){
		limitPM8226chg900();
		printk("[BAT][CHG]:limit charging current 900mA\n");
	}
	else{
		if(HIGH_CURRENT_CHARGER_TYPE==gpCharger->type){
			limitPM8226chg900();
		}
	}
#endif//#ifndef ASUS_FACTORY_BUILD    
}
//Eason: AICL work around ---

//ASUS_BSP +++ frank_tao "suspend for fastboot mode"
#ifdef CONFIG_FASTBOOT
#include <linux/fastboot.h>
#endif //#ifdef CONFIG_FASTBOOT
//ASUS_BSP --- frank_tao "suspend for fastboot mode"
static DEFINE_SPINLOCK(charger_in_out_debounce_lock);
static void charger_in_out_debounce_time_expired(unsigned long _data)
{
	unsigned long flags;
	int online;

	struct AXC_PM8226Charger *this = (struct AXC_PM8226Charger *)_data;

	spin_lock_irqsave(&charger_in_out_debounce_lock, flags);

	online = getIfonline_pm8226();

	printk("[BAT][CHG]%s,%d\n",__FUNCTION__,online);

	wake_lock_timeout(&this->cable_in_out_wakelock, 3 * HZ);

	//ASUS_BSP +++ frank_tao "suspend for fastboot mode"
#ifdef CONFIG_FASTBOOT
	if(is_fastboot_enable()){
		if(online){
			ready_to_wake_up_and_send_power_key_press_event_in_fastboot();
		}
	}
#endif //#ifdef CONFIG_FASTBOOT
	//ASUS_BSP --- Frank_tao "suspend for fastboot mode"

//ASUS_BSP porting charger mode +++
#if defined(ASUS_CHARGING_MODE) && !defined(ASUS_FACTORY_BUILD)
    g_chg_present = online;
    printk("PM8226charger [BAT][CHG]%s,g_chg_present %d\n",__FUNCTION__,online);
#endif
//ASUS_BSP porting charger mode ---
	
	if(NULL != notify_charger_in_out_func_ptr){
		(*notify_charger_in_out_func_ptr) (online);
    }
	else{
		printk("Nobody registed..\n");
	}

	spin_unlock_irqrestore(&charger_in_out_debounce_lock, flags);
}

irqreturn_t PM8226_charger_in_out_handler(int irq, void *dev_id)
{
	unsigned long flags;

	if(gpCharger == NULL){
		return IRQ_HANDLED;
	}

	if(!timer_pending(&gpCharger->charger_in_out_timer)){
		spin_lock_irqsave(&charger_in_out_debounce_lock, flags);
		mod_timer(&gpCharger->charger_in_out_timer, jiffies + msecs_to_jiffies(40));
		spin_unlock_irqrestore(&charger_in_out_debounce_lock, flags);
	}

	return IRQ_HANDLED;
}

//Eason: schedule work to check cable status later+++
static void detectChg(struct work_struct *dat)
{
	charger_in_out_debounce_time_expired((unsigned long)gpCharger);
}
//Eason: schedule work to check cable status later---

static void AXC_PM8226_Charger_Init(AXI_Charger *apCharger)
{
	AXC_PM8226Charger *this = container_of(apCharger,
		AXC_PM8226Charger,
		msParentCharger);

	if(false == this->mbInited) {
		printk("[BAT][PM8226]Init++\n");
		this->type = NO_CHARGER_TYPE;
		this->mpNotifier = NULL;

		wake_lock_init(&this->cable_in_out_wakelock, WAKE_LOCK_SUSPEND, "cable in out");
		setup_timer(&this->charger_in_out_timer, charger_in_out_debounce_time_expired,(unsigned long)this);

		this->mbInited = true;
		
		//create_charger_proc_file();
		gpCharger = this;
		gpCharger->usb_current_max = 200*1000;

		//Eason: schedule work to check cable status later+++
		INIT_DELAYED_WORK(&DetectChgWorker,detectChg); 
		schedule_delayed_work(&DetectChgWorker, 5*HZ);
		//Eason: schedule work to check cable status later---

		printk("[BAT][PM8226]Init--\n");
	}	
}

static void AXC_PM8226_Charger_DeInit(AXI_Charger *apCharger)
{
	AXC_PM8226Charger *this = container_of(apCharger,
		AXC_PM8226Charger,
		msParentCharger);

	if(true == this->mbInited) {
		this->mbInited = false;
	}
}

int AXC_PM8226_Charger_GetType(AXI_Charger *apCharger)
{
	AXC_PM8226Charger *this = container_of(apCharger,
		AXC_PM8226Charger,
		msParentCharger);

	return this->mnType;
}

void AXC_PM8226_Charger_SetType(AXI_Charger *apCharger ,int anType)
{
    AXC_PM8226Charger *this = container_of(apCharger,
                                            AXC_PM8226Charger,
                                            msParentCharger);

    this->mnType = anType;
}

static AXE_Charger_Type AXC_PM8226_Charger_GetChargerStatus(AXI_Charger *apCharger)
{
	AXC_PM8226Charger *this = container_of(apCharger,
                                                AXC_PM8226Charger,
                                                msParentCharger);

	return this->type;
}

static void AXC_PM8226_Charger_SetCharger(AXI_Charger *apCharger , AXE_Charger_Type aeChargerType)
{
	AXC_PM8226Charger *this = container_of(apCharger, AXC_PM8226Charger, msParentCharger);

	if(false == this->mbInited){
		return;
	}

	printk("[BAT][CHG]CharegeModeSet:%d\n", aeChargerType);

//ASUS BSP Eason_Chang prevent P02 be set as illegal charger +++ 
	if(NORMAL_CURRENT_CHARGER_TYPE==this->type && ILLEGAL_CHARGER_TYPE==aeChargerType){
		printk("[BAT][CHG]prevent P02 be set as illegal charger\n");
		return;
	}
//ASUS BSP Eason_Chang prevent P02 be set as illegal charger ---

//A68 set smb346 default charging setting+++
	//Eason: when AC dont set default current. when phone Cap low can always draw 1200mA from boot to kernel+++ 
	if(HIGH_CURRENT_CHARGER_TYPE!=aeChargerType){
		defaultPM8226chgSetting();
		printk("[BAT][CHG]default setting\n");
	}
	//Eason: when AC dont set default current. when phone Cap low can always draw 1200mA from boot to kernel---
#ifndef ASUS_FACTORY_BUILD
	g_chgTypeBeenSet = true;//Eason : prevent setChgDrawCurrent before get chgType
#endif//#ifndef ASUS_FACTORY_BUILD
//A68 set smb346 default charging setting---            

	switch(aeChargerType)
	{
		case NO_CHARGER_TYPE:
			this->type = aeChargerType;

			if(NULL != this->mpNotifier)
				this->mpNotifier->Notify(&this->msParentCharger,this->type);
			break;

		case ILLEGAL_CHARGER_TYPE:
		case LOW_CURRENT_CHARGER_TYPE:	
			this->type = aeChargerType;
			//Eason: AICL work around +++
			lastTimeCableType = aeChargerType;
			//Eason: AICL work around ---

			setChgDrawCurrent_pm8226();
			pm8226_chg_enable_charging(true);

			if(NULL != this->mpNotifier)
				this->mpNotifier->Notify(&this->msParentCharger,this->type);
			break;
		case NORMAL_CURRENT_CHARGER_TYPE:
		case HIGH_CURRENT_CHARGER_TYPE:
			//Eason : when thermal too hot, limit charging current +++ 
			this->type = aeChargerType;
			//Eason: AICL work around +++
			lastTimeCableType = aeChargerType;

			//Eason Factory version AC:1200 Pad:900+++
			#ifdef ASUS_FACTORY_BUILD
			if(HIGH_CURRENT_CHARGER_TYPE==aeChargerType)
				limitPM8226chg900();
			else if(NORMAL_CURRENT_CHARGER_TYPE==aeChargerType)
				limitPM8226chg900();
			#endif
			//Eason Factory version AC:1200 Pad:900---

			setChgDrawACTypeCurrent_withCheckAICL_pm8226();
			pm8226_chg_enable_charging(true);
			//Eason: AICL work around ---
			//Eason : when thermal too hot, limit charging current --- 

			if(NULL != this->mpNotifier)
				this->mpNotifier->Notify(&this->msParentCharger,this->type);

			break;
		default:
			printk( "[BAT][CHG]Wrong ChargerMode:%d\n",aeChargerType);
			break;
	}

	AcUsbPowerSupplyChange_pm8226();
}

static void AXC_PM8226_Charger_SetBatConfig(AXI_Charger *apCharger , bool is_bat_valid)
{
//	AXC_PM8226Charger *this = container_of(apCharger,
//                                                AXC_PM8226Charger,
//                                                msParentCharger);
	if (is_bat_valid) {
		printk(KERN_INFO "[BAT]%s, bat is valid\n", __func__);
	}

//	this->m_is_bat_valid = is_bat_valid;

	return;
}

static bool AXC_PM8226_Charger_IsCharegrPlugin(AXI_Charger *apCharger)
{
	AXC_PM8226Charger *this = container_of(apCharger,
                                                AXC_PM8226Charger,
                                                msParentCharger);

	return (this->type != NO_CHARGER_TYPE);
}

static bool AXC_PM8226_Charger_IsCharging(AXI_Charger *apCharger)
{
	int status = pm8226_get_prop_batt_status();
	
	if (status == POWER_SUPPLY_STATUS_CHARGING) {
		pr_debug(KERN_INFO "[BAT]%s, charging now\r\n", __func__);
		return true;
	} else {
		pr_debug(KERN_INFO "[BAT]%s, not charging now, status=%d\r\n", __func__, status);
		return false;
	}
}

static void AXC_PM8226Charger_RegisterChargerStateChanged(struct AXI_Charger *apCharger, AXI_ChargerStateChangeNotifier *apNotifier
                                                            ,AXE_Charger_Type chargerType)
{
	AXC_PM8226Charger *this = container_of(apCharger,
		AXC_PM8226Charger,
		msParentCharger);

	this->mpNotifier = apNotifier;
}

static void AXC_PM8226Charger_DeregisterChargerStateChanged(struct AXI_Charger *apCharger,AXI_ChargerStateChangeNotifier *apNotifier)
{
	AXC_PM8226Charger *this = container_of(apCharger,
		AXC_PM8226Charger,
		msParentCharger);

	if(this->mpNotifier == apNotifier)
		this->mpNotifier = NULL;
}

static void EnableCharging(struct AXI_Charger *apCharger, bool enabled)
{
	pm8226_chg_enable_charging(enabled);
}

void AXC_PM8226Charger_Binding(AXI_Charger *apCharger,int anType)
{
	AXC_PM8226Charger *this = container_of(apCharger,
		AXC_PM8226Charger,
		msParentCharger);

	this->msParentCharger.Init = AXC_PM8226_Charger_Init;
	this->msParentCharger.DeInit = AXC_PM8226_Charger_DeInit;
	this->msParentCharger.GetType = AXC_PM8226_Charger_GetType;
	this->msParentCharger.SetType = AXC_PM8226_Charger_SetType;
	this->msParentCharger.GetChargerStatus = AXC_PM8226_Charger_GetChargerStatus;
	this->msParentCharger.SetCharger = AXC_PM8226_Charger_SetCharger;
	this->msParentCharger.EnableCharging = EnableCharging;
	this->msParentCharger.SetBatConfig = AXC_PM8226_Charger_SetBatConfig;
	this->msParentCharger.IsCharegrPlugin = AXC_PM8226_Charger_IsCharegrPlugin;
	this->msParentCharger.IsCharging = AXC_PM8226_Charger_IsCharging;
	this->msParentCharger.RegisterChargerStateChanged= AXC_PM8226Charger_RegisterChargerStateChanged;	
	this->msParentCharger.DeregisterChargerStateChanged= AXC_PM8226Charger_DeregisterChargerStateChanged;	
	this->msParentCharger.SetType(apCharger,anType);

	//this->mbInited = false;
}

void UsbSetOtgSwitch_pm8226(bool switchOtg)
{
/*
	if(true==switchOtg)
	{
		smb346_write_enable();
		smb346_otg_enable(true);
	}else{
		smb346_write_enable();
		smb346_otg_enable(false);
	}
*/
}


int registerChargerInOutNotificaition_pm8226(void (*callback)(int))
{
	printk("[BAT][CHG]%s\n",__FUNCTION__);

	notify_charger_in_out_func_ptr = callback;  

    return 0;
}

void pm8226_register_usb(void)
{
	int err;
	err = power_supply_register(NULL, &usb_psy);
	if (err < 0) {
		printk("power_supply_register usb failed rc = %d\n", err);
	}
}

static int __init pm8226_charger_init(void)
{
	int err;

	printk("pm8226_charger_init +\n");

	err = power_supply_register(NULL, &usb_psy);
	if (err < 0) {
		printk("power_supply_register usb failed rc = %d\n", err);
	}

	err = power_supply_register(NULL, &main_psy);
	if (err < 0) {
		printk("power_supply_register ac failed rc = %d\n", err);
	}
	
	isAcUsbPsyRegst = 1;

	create_pm8226_proc_file();
	
	return err;
}

static void __exit pm8226_charger_exit(void)
{
	isAcUsbPsyRegst = 0;
	power_supply_unregister(&usb_psy);
	power_supply_unregister(&main_psy);
}

module_init(pm8226_charger_init);
module_exit(pm8226_charger_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ASUS pm8226 charger driver");
MODULE_VERSION("1.0");
MODULE_AUTHOR("Lenter Chang <lenter_chang@asus.com>");
