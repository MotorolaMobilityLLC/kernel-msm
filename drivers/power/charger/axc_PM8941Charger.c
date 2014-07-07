/*
        Qualcomm PM8941 Charger IC Implementation

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


#include "axc_PM8941Charger.h"
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
bool g_padMic_On = false;
//Eason : when thermal too hot, limit charging current ---
extern int pm8941_getCapacity(void);

//Eason: AICL work around +++
static struct delayed_work AICLWorker;
#ifndef ASUS_FACTORY_BUILD
static bool g_chgTypeBeenSet = false;//Eason : prevent setChgDrawCurrent_pm8941 before get chgType
static bool g_AICLlimit = false;
static bool g_AICLSuccess = false;
static unsigned long AICL_success_jiffies;
#endif
#include <linux/jiffies.h>
static AXE_Charger_Type lastTimeCableType = NO_CHARGER_TYPE;
#define ADAPTER_PROTECT_DELAY (7*HZ)
//Eason: AICL work around ---
//Eason: Factory5060Mode+++
#ifdef ASUS_FACTORY_BUILD
extern bool g_5060modeCharging;
#endif
//Eason: Factory5060Mode---

void create_charger_proc_file(void);

extern void pm8941_chg_enable_charging(bool enable);
extern int pm8941_is_usb_in(void);
extern int pm8941_is_dc_usb_in(void);
extern void pm8941_chg_usb_suspend_enable(int enable);
extern int pm8941_get_prop_batt_status(void);

//Eason: in Pad AC powered, judge AC powered true+++
extern int InP03JudgeACpowered(void);
//Eason: in Pad AC powered, judge AC powered true---

static void (*notify_charger_in_out_func_ptr)(int) = NULL;
static AXC_PM8941Charger *gpCharger = NULL;

//Jorney_dong +++
#if defined(ASUS_CHARGING_MODE) && !defined(ASUS_FACTORY_BUILD)
extern int g_chg_present;
extern char g_CHG_mode;
#endif
//Jorney_dong---

//ASUS_BSP Eason read PM8941 register value+++
u32 g_pm8941_register_val;
extern u32 asus_pmic_arb_read(u32 offset);
//ASUS_BSP Eason read PM8941 register value---

//Eason: schedule work to check cable status later+++
static struct delayed_work DetectChgWorker;
//Eason: schedule work to check cable status later---

static ssize_t pm8941charger_read_proc(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int status;
	
	status = pm8941_get_prop_batt_status();

	//ASUS_BSP Eason read PM8941 register value+++
	g_pm8941_register_val = asus_pmic_arb_read(0x105B);
	printk("[BAT][PM8941]0x105B:0x%x\n",g_pm8941_register_val);

	g_pm8941_register_val = asus_pmic_arb_read(0x1049);
	printk("[BAT][PM8941]0x1049:0x%x\n",g_pm8941_register_val);
	//ASUS_BSP Eason read PM8941 register value---

	//defined in power_supply.h: unknown = 0, charging = 1, discharging = 2, not_charging = 3, full = 4
	//defined by ATD:            unknown = 1, charging = 2, discharging = 3, not_charging = 4, full = 5
	status++;
	return sprintf(page, "%d\n", status);
}

void static create_pm8941_proc_file(void)
{
	struct proc_dir_entry *pm8941_proc_file = create_proc_entry("driver/pm8941chg", 0644, NULL);

	if (pm8941_proc_file) {
		pm8941_proc_file->read_proc = pm8941charger_read_proc;
	}
    else {
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
				if(1==InP03JudgeACpowered())
					val->intval = 1;
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
int getIfonline_pm8941(void)
{
    return pm8941_is_dc_usb_in();
}
//Eason: Do VF with Cable when boot up---

//ASUS_BSP Eason_Chang BAT charger driver set usb path charge current +++ 
extern void asus_usbPath_chg_current_set(int mA);
//ASUS_BSP Eason_Chang BAT charger driver set usb path charge current ---

void AcUsbPowerSupplyChange_pm8941(void)
{
	power_supply_changed(&usb_psy);
	power_supply_changed(&main_psy);
	printk("[BAT][CHG]:Ac Usb PowerSupply Change\n");
}

static void limitPM8941chg1200(void)
{
	gpCharger->usb_current_max = 1200*1000;
	asus_usbPath_chg_current_set(1200);
}

void limitPM8941chg900(void)
{
	gpCharger->usb_current_max = 900*1000;
	asus_usbPath_chg_current_set(900);
}

#ifndef ASUS_FACTORY_BUILD
static void limitPM8941chg700(void)
{
	gpCharger->usb_current_max = 700*1000;
	asus_usbPath_chg_current_set(700);
}

static void limitPM8941chg500(void)
{
	gpCharger->usb_current_max = 500*1000;
	asus_usbPath_chg_current_set(500);
}

static void limitPM8941chg300(void)
{
	gpCharger->usb_current_max = 300*1000;
	asus_usbPath_chg_current_set(300);
}
#endif

static void defaultPM8941chgSetting(void)
{
	gpCharger->usb_current_max = 500*1000;// default:500mA
	asus_usbPath_chg_current_set(500);
}

#ifndef ASUS_FACTORY_BUILD
//Eason:  Pad draw rule compare thermal +++
static PadDrawLimitCurrent_Type JudgePadThermalDrawLimitCurrent_pm8941(void)
{
	if( (3==g_thermal_limit)||(true==g_audio_limit) )
	{
		return PadDraw500;
   	}else if(true==g_padMic_On){

		return PadDraw500;
	}else if( 2==g_thermal_limit )
	{
		return PadDraw700;
	}else if(1 == g_thermal_limit)
	{
		return PadDraw900;
	}else
		return PadDraw900;
}
//Eason: set Chg Limit In Pad When Charger Reset +++ 
static PadDrawLimitCurrent_Type g_InPadChgNoResetDraw_limit_pm8941 = PadDraw700;
void setChgLimitInPadWhenChgReset_pm8941(void)
{	
	switch(g_InPadChgNoResetDraw_limit_pm8941)
	{
		case PadDraw300:
				limitPM8941chg300();
				printk("[BAT][CHG][PAD][Reset]:draw 300\n");
				break;
		case PadDraw500:
				limitPM8941chg500();
				printk("[BAT][CHG][PAD][Reset]:draw 500\n");
				break;
		case PadDraw700:
				limitPM8941chg700();
				printk("[BAT][CHG][PAD][Reset]:draw 700\n");
				break;
		case PadDraw900:
				limitPM8941chg900();
				printk("[BAT][CHG][PAD][Reset]:draw 900\n");
				break;				
		default:
				limitPM8941chg700();
				printk("[BAT][CHG][PAD][Reset]:draw 700\n");
				break;
	}
}

//Eason: set Chg Limit In Pad When Charger Reset --- 
void setChgLimitThermalRuleDrawCurrent_pm8941(bool isSuspendCharge)
{
	PadDrawLimitCurrent_Type PadThermalDraw_limit;
	PadDrawLimitCurrent_Type PadRuleDraw_limit;
	PadDrawLimitCurrent_Type MinThermalRule_limit;

	PadThermalDraw_limit=JudgePadThermalDrawLimitCurrent_pm8941();
	PadRuleDraw_limit=JudgePadRuleDrawLimitCurrent(isSuspendCharge);
	MinThermalRule_limit = min(PadThermalDraw_limit,PadRuleDraw_limit);
	printk("[BAT][CHG][PAD][Cur]:Thermal:%d,Rule:%d,Min:%d\n",PadThermalDraw_limit,PadRuleDraw_limit,MinThermalRule_limit);

	
	//ForcePowerBankMode draw 900, but  still compare thermal result unless PhoneCap<=8
	if( (PadDraw900==PadRuleDraw_limit)&&(pm8941_getCapacity()<= 8))
	{
		limitPM8941chg900();
		g_InPadChgNoResetDraw_limit_pm8941 = PadDraw900;
		printk("[BAT][CHG][PAD][Cur]:draw 900\n");
	}else{
		switch(MinThermalRule_limit)
		{
			case PadDraw300:
				limitPM8941chg300();
				g_InPadChgNoResetDraw_limit_pm8941 = PadDraw300;
				printk("[BAT][CHG][PAD][Cur]:draw 300\n");
				break;
			case PadDraw500:
				limitPM8941chg500();
				g_InPadChgNoResetDraw_limit_pm8941 = PadDraw500;
				printk("[BAT][CHG][PAD][Cur]:draw 500\n");
				break;
			case PadDraw700:
				limitPM8941chg700();
				g_InPadChgNoResetDraw_limit_pm8941 = PadDraw700;
				printk("[BAT][CHG][PAD][Cur]:draw 700\n");
				break;
			case PadDraw900:
				limitPM8941chg900();
				if(!isSuspendCharge)//Hank: if suspend set 900mA do not remeber the current
					g_InPadChgNoResetDraw_limit_pm8941 = PadDraw900;
				printk("[BAT][CHG][P03]:draw 900\n");
				break;
			default:
				limitPM8941chg700();
				g_InPadChgNoResetDraw_limit_pm8941 = PadDraw700;
				printk("[BAT][CHG][PAD][Cur]:draw 700\n");
				break;
		}
	}
}
#endif
//Eason:  Pad draw rule compare thermal ---
//Eason : when thermal too hot, limit charging current +++ 
void setChgDrawCurrent_pm8941(void)
{
#ifndef ASUS_FACTORY_BUILD
	if(true == g_chgTypeBeenSet)//Eason : prevent setChgDrawCurrent_pm8941 before get chgType
	{
		if(NORMAL_CURRENT_CHARGER_TYPE==gpCharger->type){
			setChgLimitThermalRuleDrawCurrent_pm8941(false);
		}
		//Eason: pm8941 AICL funtion not ready, set USB charging current 500
		else if(NOTDEFINE_TYPE==gpCharger->type || NO_CHARGER_TYPE==gpCharger->type
			||LOW_CURRENT_CHARGER_TYPE==gpCharger->type||ILLEGAL_CHARGER_TYPE==gpCharger->type){
			defaultPM8941chgSetting();
			printk("[BAT][CHG]:default 500mA\n");
		}
		else if( (3==g_thermal_limit)||(true==g_audio_limit) ){
			limitPM8941chg500();
			printk("[BAT][CHG]:limit charging current 500mA\n");
		}
		else if(true==g_padMic_On){
			limitPM8941chg500();
			printk("[BAT][CHG]:InPad onCall limit curent 500mA\n");
		}
#ifdef CONFIG_IDTP9023_CHARGER  
		else if(WC_CHARGER_TYPE==gpCharger->type){
			limitPM8941chg700();
			printk("[BAT][CHG][WC]:default 700mA\n");
		}
#endif
		else if( 2==g_thermal_limit ){
			limitPM8941chg700();
			printk("[BAT][CHG]:limit charging current 700mA\n");
		}
		else if(1 == g_thermal_limit){
			limitPM8941chg900();
			printk("[BAT][CHG]:limit charging current 900mA\n");
		}
		else{
			 if(HIGH_CURRENT_CHARGER_TYPE==gpCharger->type){
				//Eason: AICL work around +++
				if(true==g_AICLlimit){
					limitPM8941chg900();
					printk("[BAT][CHG][AICL]: g_AICLlimit = true\n");
				}
				//Eason: AICL work around ---
				else{
					limitPM8941chg1200();
					printk("[BAT][CHG][AC]: dont limit chgCur, use default setting\n");
				}
			}
		}
	}
#endif//#ifndef ASUS_FACTORY_BUILD
}
//Eason : when thermal too hot, limit charging current ---

//Eason: AICL work around +++
void setChgDrawACTypeCurrent_withCheckAICL_pm8941(void)
{
#ifndef ASUS_FACTORY_BUILD   
	if(NORMAL_CURRENT_CHARGER_TYPE==gpCharger->type){
		setChgLimitThermalRuleDrawCurrent_pm8941(false);
	}
	else if( (3==g_thermal_limit)||(true==g_audio_limit) ){
		limitPM8941chg500();
		printk("[BAT][CHG]:limit charging current 500mA\n");
	}
	else if(true==g_padMic_On){
		limitPM8941chg500();
		printk("[BAT][CHG]:InPad onCall limit cur500mA\n");
	}
	else if( 2 == g_thermal_limit){
		limitPM8941chg700();
		printk("[BAT][CHG]:limit charging current 900mA\n");
	}
	else if( 1 == g_thermal_limit){
		limitPM8941chg900();
		printk("[BAT][CHG]:limit charging current 900mA\n");
	}
	else{
		if(HIGH_CURRENT_CHARGER_TYPE==gpCharger->type){
			//Hank: remove 1A adapter protection+++
			#if 0
			if( (time_after(AICL_success_jiffies+ADAPTER_PROTECT_DELAY , jiffies))
				&& (HIGH_CURRENT_CHARGER_TYPE==lastTimeCableType) 
				&& (true == g_AICLSuccess) )
			{
				limitPM8941chg900();
				g_AICLlimit = true;
				g_AICLSuccess = false;
				printk("[BAT][CHG][AICL]:AICL fail, always limit 900mA charge \n");
			}
			//else if( (pm8941_getCapacity()>5) &&(true== g_alreadyCalFirstCap)){
				//limitSmb346chg900();
				//g_AICLlimit = true;
				//schedule_delayed_work(&AICLWorker, 60*HZ);
				//printk("[BAT][CHG][AICL]: check AICL\n");
			//}
			else{
				limitPM8941chg1200();
				printk("[BAT][CHG][AICL]: dont limit chgCur, use default setting\n");
			}
			#endif
			limitPM8941chg1200();
			//Hank: remove 1A adapter protection---
		}
	}
#endif//#ifndef ASUS_FACTORY_BUILD    
}

void checkIfAICLSuccess(void)
{
#ifndef ASUS_FACTORY_BUILD  
	if ( 0==pm8941_is_usb_in()){
		limitPM8941chg900();
		g_AICLlimit = true;
		g_AICLSuccess = false;
		printk("[BAT][CHG][AICL]:AICL fail, limit 900mA charge \n");
	}else{
		limitPM8941chg1200(); 
		g_AICLlimit =false;
		g_AICLSuccess = true;
		//Eason: AICL work around +++
		AICL_success_jiffies = jiffies;
		//Eason: AICL work around ---
		printk("[BAT][CHG][AICL]:AICL success, use default charge \n");
	}
#endif//#ifndef ASUS_FACTORY_BUILD	
}

static void checkAICL(struct work_struct *dat)
{
	checkIfAICLSuccess();
}
//Eason: AICL work around ---

//ASUS_BSP +++ Peter_lu "For fastboot mode"
#ifdef CONFIG_FASTBOOT
#include <linux/fastboot.h>
#endif //#ifdef CONFIG_FASTBOOT
//ASUS_BSP ---

static DEFINE_SPINLOCK(charger_in_out_debounce_lock);
static void charger_in_out_debounce_time_expired(unsigned long _data)
{
	unsigned long flags;
	int online;

	struct AXC_PM8941Charger *this = (struct AXC_PM8941Charger *)_data;

	spin_lock_irqsave(&charger_in_out_debounce_lock, flags);

	online = pm8941_is_usb_in();

	printk("[BAT][CHG]%s,%d\n",__FUNCTION__,online);

	wake_lock_timeout(&this->cable_in_out_wakelock, 3 * HZ);

	//ASUS_BSP +++ Peter_lu "For fastboot mode wake up because cable in"
#ifdef CONFIG_FASTBOOT
	if(is_fastboot_enable()){
		if(online){
			ready_to_wake_up_and_send_power_key_press_event_in_fastboot( true );
		}
	}
#endif //#ifdef CONFIG_FASTBOOT
	//ASUS_BSP --- Peter_lu

	   //+++ Jorney_dong add support charging mode
#if defined(ASUS_CHARGING_MODE) && !defined(ASUS_FACTORY_BUILD)
    g_chg_present = online;
    printk("PM8941charger [BAT][CHG]%s,g_chg_present %d\n",__FUNCTION__,online);
#endif
	//--- Jorney_dong add support charging mode
	
	if(NULL != notify_charger_in_out_func_ptr){
		(*notify_charger_in_out_func_ptr) (online);
    }
	else{
		printk("Nobody registed..\n");
	}

	spin_unlock_irqrestore(&charger_in_out_debounce_lock, flags);
}

irqreturn_t PM8941_charger_in_out_handler(int irq, void *dev_id)
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

static void AXC_PM8941_Charger_Init(AXI_Charger *apCharger)
{
	AXC_PM8941Charger *this = container_of(apCharger,
		AXC_PM8941Charger,
		msParentCharger);

	if(false == this->mbInited) {
		printk("[BAT][PM8941]Init++\n");
		this->type = NO_CHARGER_TYPE;
		this->mpNotifier = NULL;
		//INIT_DELAYED_WORK(&this->msNotifierWorker, AXC_PM8941_Charger_NotifyClientForStablePlugIn) ;

		wake_lock_init(&this->cable_in_out_wakelock, WAKE_LOCK_SUSPEND, "cable in out");
		setup_timer(&this->charger_in_out_timer, charger_in_out_debounce_time_expired,(unsigned long)this);

		this->mbInited = true;
		
		
		//create_charger_proc_file();
		gpCharger = this;
		gpCharger->usb_current_max = 500*1000;
		//AcUsbPowerSupplyChange_pm8941();

		//Eason: schedule work to check cable status later+++
		INIT_DELAYED_WORK(&DetectChgWorker,detectChg); 
		schedule_delayed_work(&DetectChgWorker, 5*HZ);
		//Eason: schedule work to check cable status later---
		printk("[BAT][PM8941]Init--\n");
	}	
}

static void AXC_PM8941_Charger_DeInit(AXI_Charger *apCharger)
{
	AXC_PM8941Charger *this = container_of(apCharger,
		AXC_PM8941Charger,
		msParentCharger);

	if(true == this->mbInited) {
		this->mbInited = false;
	}
}

int AXC_PM8941_Charger_GetType(AXI_Charger *apCharger)
{
	AXC_PM8941Charger *this = container_of(apCharger,
		AXC_PM8941Charger,
		msParentCharger);

	return this->mnType;
}

void AXC_PM8941_Charger_SetType(AXI_Charger *apCharger ,int anType)
{
    AXC_PM8941Charger *this = container_of(apCharger,
                                            AXC_PM8941Charger,
                                            msParentCharger);

    this->mnType = anType;
}

static AXE_Charger_Type AXC_PM8941_Charger_GetChargerStatus(AXI_Charger *apCharger)
{
	AXC_PM8941Charger *this = container_of(apCharger,
                                                AXC_PM8941Charger,
                                                msParentCharger);

	return this->type;
}

#ifdef CONFIG_IDTP9023_CHARGER  
extern bool Is_WC_connect(void);
#endif

static void AXC_PM8941_Charger_SetCharger(AXI_Charger *apCharger , AXE_Charger_Type aeChargerType)
{
	AXC_PM8941Charger *this = container_of(apCharger, AXC_PM8941Charger, msParentCharger);

	if(false == this->mbInited){
		return;
	}

	printk("[BAT][CHG]CharegeModeSet:%d\n", aeChargerType);

//ASUS_BSP Eason_Chang: for Wireless charger+++
#ifdef CONFIG_IDTP9023_CHARGER  
	if(NO_CHARGER_TYPE!=this->type && WC_CHARGER_TYPE==aeChargerType){
		printk("[BAT][CHG][WC]already has cable, ignore wireless charger\n");
		return;
	}else if(NO_CHARGER_TYPE!= this->type && WC_CHARGER_TYPE!= this->type && NO_CHARGER_TYPE==aeChargerType && (pm8941_is_usb_in())){
		printk("[BAT][CHG][WC]still has USB path chg type detection, ignore WC handler NO_CHARGER_TYPE\n");
		return;
	}else if(NO_CHARGER_TYPE!= this->type  && NO_CHARGER_TYPE==aeChargerType &&Is_WC_connect()){
		printk("[BAT][CHG][WC]when USB path plug out, check WC status\n");	
		aeChargerType = WC_CHARGER_TYPE;
	}	
#endif
//ASUS_BSP Eason_Chang: for Wireless charger---

//ASUS BSP Eason_Chang prevent P02 be set as illegal charger +++ 
	if(NORMAL_CURRENT_CHARGER_TYPE==this->type && ILLEGAL_CHARGER_TYPE==aeChargerType){
		printk("[BAT][CHG]prevent P02 be set as illegal charger\n");
		return;
	}
//ASUS BSP Eason_Chang prevent P02 be set as illegal charger ---

//A68 set smb346 default charging setting+++
	//Eason: when AC dont set default current. when phone Cap low can always draw 1200mA from boot to kernel+++ 
	if(HIGH_CURRENT_CHARGER_TYPE!=aeChargerType){
		defaultPM8941chgSetting();
		printk("[BAT][CHG]default setting\n");
	}
	//Eason: when AC dont set default current. when phone Cap low can always draw 1200mA from boot to kernel---
#ifndef ASUS_FACTORY_BUILD
	g_AICLlimit = false;
	g_chgTypeBeenSet = true;//Eason : prevent setChgDrawCurrent_pm8941 before get chgType
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
#ifdef CONFIG_IDTP9023_CHARGER  
		case WC_CHARGER_TYPE:  
#endif
			this->type = aeChargerType;
			//Eason: AICL work around +++
			lastTimeCableType = aeChargerType;
			//Eason: AICL work around ---

			setChgDrawCurrent_pm8941();
			pm8941_chg_enable_charging(true);

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
				limitPM8941chg1200();
			else if(NORMAL_CURRENT_CHARGER_TYPE==aeChargerType)
				limitPM8941chg900();
			#endif
			//Eason Factory version AC:1200 Pad:900---

			setChgDrawACTypeCurrent_withCheckAICL_pm8941();
			pm8941_chg_enable_charging(true);
			//Eason: AICL work around ---
			//Eason : when thermal too hot, limit charging current --- 

			if(NULL != this->mpNotifier)
				this->mpNotifier->Notify(&this->msParentCharger,this->type);

			break;
		default:
			printk( "[BAT][CHG]Wrong ChargerMode:%d\n",aeChargerType);
			break;
	}

	AcUsbPowerSupplyChange_pm8941();
}

static void AXC_PM8941_Charger_SetBatConfig(AXI_Charger *apCharger , bool is_bat_valid)
{
//	AXC_PM8941Charger *this = container_of(apCharger,
//                                                AXC_PM8941Charger,
//                                                msParentCharger);
	if (is_bat_valid) {
		printk(KERN_INFO "[BAT]%s, bat is valid\n", __func__);
	}

//	this->m_is_bat_valid = is_bat_valid;

	return;
}

static bool AXC_PM8941_Charger_IsCharegrPlugin(AXI_Charger *apCharger)
{
	AXC_PM8941Charger *this = container_of(apCharger,
                                                AXC_PM8941Charger,
                                                msParentCharger);

	return (this->type != NO_CHARGER_TYPE);
}

static bool AXC_PM8941_Charger_IsCharging(AXI_Charger *apCharger)
{
	int status = pm8941_get_prop_batt_status();
	
	if (status == POWER_SUPPLY_STATUS_CHARGING) {
		printk(KERN_INFO "[BAT]%s, charging now\r\n", __func__);
		return true;
	} else {
		printk(KERN_INFO "[BAT]%s, not charging now:%d\r\n", __func__,status);
		return false;
	}
}

static void AXC_PM8941Charger_RegisterChargerStateChanged(struct AXI_Charger *apCharger, AXI_ChargerStateChangeNotifier *apNotifier
                                                            ,AXE_Charger_Type chargerType)
{
	AXC_PM8941Charger *this = container_of(apCharger,
		AXC_PM8941Charger,
		msParentCharger);

	this->mpNotifier = apNotifier;
}

static void AXC_PM8941Charger_DeregisterChargerStateChanged(struct AXI_Charger *apCharger,AXI_ChargerStateChangeNotifier *apNotifier)
{
	AXC_PM8941Charger *this = container_of(apCharger,
		AXC_PM8941Charger,
		msParentCharger);

	if(this->mpNotifier == apNotifier)
		this->mpNotifier = NULL;
}

static void EnableCharging(struct AXI_Charger *apCharger, bool enabled)
{
    pm8941_chg_enable_charging(enabled);
}

void AXC_PM8941Charger_Binding(AXI_Charger *apCharger,int anType)
{
	AXC_PM8941Charger *this = container_of(apCharger,
		AXC_PM8941Charger,
		msParentCharger);

	this->msParentCharger.Init = AXC_PM8941_Charger_Init;
	this->msParentCharger.DeInit = AXC_PM8941_Charger_DeInit;
	this->msParentCharger.GetType = AXC_PM8941_Charger_GetType;
	this->msParentCharger.SetType = AXC_PM8941_Charger_SetType;
	this->msParentCharger.GetChargerStatus = AXC_PM8941_Charger_GetChargerStatus;
	this->msParentCharger.SetCharger = AXC_PM8941_Charger_SetCharger;
	this->msParentCharger.EnableCharging = EnableCharging;
	this->msParentCharger.SetBatConfig = AXC_PM8941_Charger_SetBatConfig;
	this->msParentCharger.IsCharegrPlugin = AXC_PM8941_Charger_IsCharegrPlugin;
	this->msParentCharger.IsCharging = AXC_PM8941_Charger_IsCharging;
	this->msParentCharger.RegisterChargerStateChanged= AXC_PM8941Charger_RegisterChargerStateChanged;	
	this->msParentCharger.DeregisterChargerStateChanged= AXC_PM8941Charger_DeregisterChargerStateChanged;	
	this->msParentCharger.SetType(apCharger,anType);

	//this->mbInited = false;
}

void UsbSetOtgSwitch_pm8941(bool switchOtg)
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



int registerChargerInOutNotificaition_pm8941(void (*callback)(int))
{
	printk("[BAT][CHG]%s\n",__FUNCTION__);

	notify_charger_in_out_func_ptr = callback;  

    return 0;
}

static int __init pm8941_charger_init(void)
{
	int err;

	printk("pm8941_charger_init +\n");

	err = power_supply_register(NULL, &usb_psy);
	if (err < 0) {
		printk("power_supply_register usb failed rc = %d\n", err);
	}

	err = power_supply_register(NULL, &main_psy);
	if (err < 0) {
		printk("power_supply_register ac failed rc = %d\n", err);
	}

	//Eason: AICL work around +++
	INIT_DELAYED_WORK(&AICLWorker,checkAICL); 
	//Eason: AICL work around ---

	create_pm8941_proc_file();

	
	return err;
}

static void __exit pm8941_charger_exit(void)
{
	power_supply_unregister(&usb_psy);
	power_supply_unregister(&main_psy);
}

module_init(pm8941_charger_init);
module_exit(pm8941_charger_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ASUS pm8941 charger driver");
MODULE_VERSION("1.0");
MODULE_AUTHOR("Lenter Chang <lenter_chang@asus.com>");
