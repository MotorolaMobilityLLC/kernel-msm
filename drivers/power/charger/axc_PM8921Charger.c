/*
        Qualcomm PM8921 Charger IC Implementation

*/
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mfd/pm8xxx/pm8921-charger.h>
#include "axc_PM8921Charger.h"
#include <asm/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/asus_bat.h>
#include <linux/asus_chg.h>

/*
Implement Interface for USB Driver
*/
#include "axc_chargerfactory.h"

extern void create_charger_proc_file(void);

static void AXC_PM8921_Charger_Init(AXI_Charger *apCharger)
{
	AXC_PM8921Charger *this = container_of(apCharger,
		AXC_PM8921Charger,
		msParentCharger);

	if(false == this->mbInited) {
		printk(DBGMSK_BAT_DEBUG "[BAT][PM8921]Init++\n");
		this->type = NO_CHARGER_TYPE;
		this->mpNotifier = NULL;
		//INIT_DELAYED_WORK(&this->msNotifierWorker, AXC_PM8921_Charger_NotifyClientForStablePlugIn) ;

		this->mbInited = true;

		create_charger_proc_file();
		printk(DBGMSK_BAT_DEBUG "[BAT][PM8921]Init--\n");
	}	
}

static void AXC_PM8921_Charger_DeInit(AXI_Charger *apCharger)
{
	AXC_PM8921Charger *this = container_of(apCharger,
		AXC_PM8921Charger,
		msParentCharger);

	if(true == this->mbInited) {
		this->mbInited = false;
	}
}

int AXC_PM8921_Charger_GetType(AXI_Charger *apCharger)
{
	AXC_PM8921Charger *this = container_of(apCharger,
		AXC_PM8921Charger,
		msParentCharger);

	return this->mnType;
}

void AXC_PM8921_Charger_SetType(AXI_Charger *apCharger ,int anType)
{
    AXC_PM8921Charger *this = container_of(apCharger,
                                            AXC_PM8921Charger,
                                            msParentCharger);

    this->mnType = anType;
}

static AXE_Charger_Type AXC_PM8921_Charger_GetChargerStatus(AXI_Charger *apCharger)
{
	enum asus_chg_src charging_src = ASUS_CHG_SRC_NONE;

	AXC_PM8921Charger *this = container_of(apCharger,
                                                AXC_PM8921Charger,
                                                msParentCharger);

	
//ASUS BSP Eason_Chang : A86 porting +++
#if 0
	charging_src = asus_chg_get_chg_mode();
#endif
//ASUS BSP Eason_Chang : A86 porting ---
	switch (charging_src) {
	case ASUS_CHG_SRC_NONE:
		this->type = NO_CHARGER_TYPE;
		break;
	case ASUS_CHG_SRC_USB:
	case ASUS_CHG_SRC_PAD_BAT:	
		this->type = LOW_CURRENT_CHARGER_TYPE;
		break;
	case ASUS_CHG_SRC_DC:
		this->type = HIGH_CURRENT_CHARGER_TYPE;
		break;
	default:
		printk(DBGMSK_BAT_DEBUG "[BAT][PM8921]Error!! unknown charging src:%d\n", charging_src);
	}

	printk(DBGMSK_BAT_DEBUG "[BAT][PM8921]pm8921 charging src:%d, asus charger mode:%d \n",charging_src, this->type);

	return this->type;
	
#if 0
	int charging_src = -1;

	pm8921_is_battery_charging(&charging_src);

	switch (charging_src) {
	case PM8921_CHG_SRC_NONE:
		this->type = NO_CHARGER_TYPE;
		break;
	case PM8921_CHG_SRC_USB:
		this->type = LOW_CURRENT_CHARGER_TYPE;
		break;
	case PM8921_CHG_SRC_DC:
		this->type = HIGH_CURRENT_CHARGER_TYPE;
		break;
	default:
		printk(DBGMSK_BAT_DEBUG "[BAT][PM8921]Error!! unknown charging src:%d\n",charging_src);
	}
	
	printk(DBGMSK_BAT_DEBUG "[BAT][PM8921]pm8921 charging src:%d, asus charger mode:%d \n",charging_src, this->type);

	return this->type;
#endif
}

static void AXC_PM8921_Charger_SetCharger(AXI_Charger *apCharger , AXE_Charger_Type aeChargerType)
{
	static bool first_call = true;

	AXC_PM8921Charger *this = container_of(apCharger,
		AXC_PM8921Charger,
		msParentCharger);

	if(false == this->mbInited)
		return;

	if (!first_call && !this->m_is_bat_valid) {
		printk(DBGMSK_BAT_INFO "[BAT][PM8921] %s(), battery is invalid and cannot charging\n", __func__);
		aeChargerType = NO_CHARGER_TYPE;
	}

	printk(DBGMSK_BAT_DEBUG "[BAT][PM8921]CharegeModeSet:%d\n",aeChargerType);

	switch(aeChargerType) {
	case NO_CHARGER_TYPE:
		asus_chg_set_chg_mode(ASUS_CHG_SRC_NONE);
		this->type = aeChargerType;
		//cancel_delayed_work(&this->msNotifierWorker);
		if(NULL != this->mpNotifier)
			this->mpNotifier->Notify(&this->msParentCharger,this->type);
		break;
	case LOW_CURRENT_CHARGER_TYPE:
		asus_chg_set_chg_mode(ASUS_CHG_SRC_USB);
		this->type = aeChargerType;
		if(NULL != this->mpNotifier)
			this->mpNotifier->Notify(&this->msParentCharger,this->type);
		//cancel_delayed_work(&this->msNotifierWorker);
		//schedule_delayed_work(&this->msNotifierWorker , round_jiffies_relative(TIME_FOR_NOTIFY_AFTER_PLUGIN_CABLE));
		break;
	case HIGH_CURRENT_CHARGER_TYPE:
		asus_chg_set_chg_mode(ASUS_CHG_SRC_DC);
		this->type = aeChargerType;
		if(NULL != this->mpNotifier)
			this->mpNotifier->Notify(&this->msParentCharger,this->type);
		//cancel_delayed_work(&this->msNotifierWorker);
		//schedule_delayed_work(&this->msNotifierWorker , round_jiffies_relative(TIME_FOR_NOTIFY_AFTER_PLUGIN_CABLE));
		break;
/*
	case TEMP_NO_CHARGER_TYPE:
		asus_chg_set_chg_mode(ASUS_CHG_SRC_NONE);
		break;
*/
	default:
		printk(DBGMSK_BAT_ERR "[BAT][PM8921]Wrong ChargerMode:%d\n",aeChargerType);
		break;
	}

	if (first_call) {
		first_call = false;
	}
}

static void AXC_PM8921_Charger_SetBatConfig(AXI_Charger *apCharger , bool is_bat_valid)
{
	AXC_PM8921Charger *this = container_of(apCharger,
                                                AXC_PM8921Charger,
                                                msParentCharger);
	if (is_bat_valid) {
		printk(KERN_INFO "[BAT]%s, bat is valid\n", __func__);
	}

	this->m_is_bat_valid = is_bat_valid;

	return;
}

static bool AXC_PM8921_Charger_IsCharegrPlugin(AXI_Charger *apCharger)
{
	//ASUS BSP Eason_Chang : A86 porting +++
	/*
	if (pm8921_is_chg_plugged_in())
		return true;
	else
	*/
	//ASUS BSP Eason_Chang : A86 porting ---	
		return false;
}

static bool AXC_PM8921_Charger_IsCharging(AXI_Charger *apCharger)
{
	int charging_src = 0;

	if (pm8921_is_battery_charging(&charging_src)) {
		printk(KERN_INFO "[BAT]%s, charging now, charging_src: %d \r\n", __func__, charging_src);
		return true;
	} else {
		printk(KERN_INFO "[BAT]%s, not charging now, charging_src: %d \r\n", __func__, charging_src);
		return false;
	}
}

static void AXC_PM8921Charger_RegisterChargerStateChanged(struct AXI_Charger *apCharger, AXI_ChargerStateChangeNotifier *apNotifier
                                                            ,AXE_Charger_Type chargerType)
{
	AXC_PM8921Charger *this = container_of(apCharger,
		AXC_PM8921Charger,
		msParentCharger);

	this->mpNotifier = apNotifier;
}

static void AXC_PM8921Charger_DeregisterChargerStateChanged(struct AXI_Charger *apCharger,AXI_ChargerStateChangeNotifier *apNotifier)
{
	AXC_PM8921Charger *this = container_of(apCharger,
		AXC_PM8921Charger,
		msParentCharger);

	if(this->mpNotifier == apNotifier)
		this->mpNotifier = NULL;
}
//extern void pm8921_chg_enable_charging(bool enabled);//ASUS BSP Eason_Chang : A86 porting
static void EnableCharging(struct AXI_Charger *apCharger, bool enabled)
{
//ASUS BSP Eason_Chang : A86 porting +++
#if 0
    pm8921_chg_enable_charging(enabled);
#endif
//ASUS BSP Eason_Chang : A86 porting ---
}
void AXC_PM8921Charger_Binding(AXI_Charger *apCharger,int anType)
{
	AXC_PM8921Charger *this = container_of(apCharger,
		AXC_PM8921Charger,
		msParentCharger);

	this->msParentCharger.Init = AXC_PM8921_Charger_Init;
	this->msParentCharger.DeInit = AXC_PM8921_Charger_DeInit;
	this->msParentCharger.GetType = AXC_PM8921_Charger_GetType;
	this->msParentCharger.SetType = AXC_PM8921_Charger_SetType;
	this->msParentCharger.GetChargerStatus = AXC_PM8921_Charger_GetChargerStatus;
	this->msParentCharger.SetCharger = AXC_PM8921_Charger_SetCharger;
       this->msParentCharger.EnableCharging = EnableCharging;
	this->msParentCharger.SetBatConfig = AXC_PM8921_Charger_SetBatConfig;
	this->msParentCharger.IsCharegrPlugin = AXC_PM8921_Charger_IsCharegrPlugin;
	this->msParentCharger.IsCharging = AXC_PM8921_Charger_IsCharging;
	this->msParentCharger.RegisterChargerStateChanged= AXC_PM8921Charger_RegisterChargerStateChanged;	
	this->msParentCharger.DeregisterChargerStateChanged= AXC_PM8921Charger_DeregisterChargerStateChanged;	
	this->msParentCharger.SetType(apCharger,anType);

	//this->mbInited = false;
}


unsigned GetChargerStatus(void)
{
	static AXI_Charger *lpCharger = NULL;
	if (NULL == lpCharger) {
		AXC_ChargerFactory_GetCharger(E_PM8921_CHARGER_TYPE ,&lpCharger);
		lpCharger->Init(lpCharger);
	}

	return (unsigned) lpCharger->GetChargerStatus(lpCharger);
}
void asus_onChargingStop(int chg_stop_reason)
{   
        AXC_PM8921Charger *this;

	static AXI_Charger *lpCharger = NULL;
	if (NULL == lpCharger) {
		AXC_ChargerFactory_GetCharger(E_PM8921_CHARGER_TYPE ,&lpCharger);
		lpCharger->Init(lpCharger);
	}

	this = container_of(lpCharger,
                                                AXC_PM8921Charger,
                                                msParentCharger);


    switch(chg_stop_reason){ 
		case exCHGDOWN:
            this->mpNotifier->onChargingStart(&this->msParentCharger, false);
            break;
        case exBATT_REMOVED_ERROR:
            break;
		case exBATTEMP_HOT_ERROR:	
            break;
        case exBATTEMP_COLD_ERROR:
		case exTEMP_OK_ERROR:
        case exDCIN_OV:
        case exDCIN_UV:
        case exUNREG_extDC:
        case ex8921FSM_change:    
            break;			
        default:
            printk("[BAT]%s():error reason\n",__FUNCTION__);
            break;
    }
}

void SetCharegerUSBMode(void)
{
	static AXI_Charger *lpCharger = NULL;
	if (NULL == lpCharger) {
		AXC_ChargerFactory_GetCharger(E_PM8921_CHARGER_TYPE ,&lpCharger);
		lpCharger->Init(lpCharger);
	}

	printk(DBGMSK_BAT_INFO "[BAT][PM8921]SetCharegerUSBMode\n");
	lpCharger->SetCharger(lpCharger,LOW_CURRENT_CHARGER_TYPE);
}

void SetCharegerACMode(void)
{
	static AXI_Charger *lpCharger = NULL;
	if (NULL == lpCharger) {
		AXC_ChargerFactory_GetCharger(E_PM8921_CHARGER_TYPE ,&lpCharger);
		lpCharger->Init(lpCharger);
	}

	printk(DBGMSK_BAT_INFO "[BAT][PM8921]SetCharegerACMode\n");
	lpCharger->SetCharger(lpCharger,HIGH_CURRENT_CHARGER_TYPE);
}

void SetCharegerNoPluginMode(void)
{
	static AXI_Charger *lpCharger = NULL;
	if (NULL == lpCharger) {
		AXC_ChargerFactory_GetCharger(E_PM8921_CHARGER_TYPE ,&lpCharger);
		lpCharger->Init(lpCharger);
	}
	printk(DBGMSK_BAT_INFO "[BAT][PM8921]SetCharegerNoPluginMode\n");
	lpCharger->SetCharger(lpCharger,NO_CHARGER_TYPE);
}

static ssize_t charger_read_proc(char *page, char **start, off_t off,
				int count, int *eof, void *data)
{
	static AXI_Charger *lpCharger = NULL;

	if (NULL == lpCharger) {
		AXC_ChargerFactory_GetCharger(E_PM8921_CHARGER_TYPE ,&lpCharger);
		lpCharger->Init(lpCharger);
	}

	printk("[BAT][PM8921] is charger plugged in: %s", lpCharger->IsCharegrPlugin(lpCharger)?"true":"false");
	printk("[BAT][PM8921] is charging: %s", lpCharger->IsCharging(lpCharger)?"true":"false");
	printk("[BAT][PM8921] get charger mode %d", lpCharger->GetChargerStatus(lpCharger));

	return sprintf(page, "%d\n", lpCharger->GetChargerStatus(lpCharger));
}


static ssize_t charger_write_proc(struct file *filp, const char __user *buff, 
	unsigned long len, void *data)
{
	int val;
	char messages[256];
	static AXI_Charger *lpCharger = NULL;

	if (len > 256) {
		len = 256;
	}

	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	
	val = (int)simple_strtol(messages, NULL, 10);

	if (NULL == lpCharger) {
		AXC_ChargerFactory_GetCharger(E_PM8921_CHARGER_TYPE ,&lpCharger);
		lpCharger->Init(lpCharger);
	}

	lpCharger->SetCharger(lpCharger,val);

	return len;
}

void create_charger_proc_file(void)
{
	struct proc_dir_entry *charger_proc_file = create_proc_entry("driver/axc_pm8921charger", 0644, NULL);

	if (charger_proc_file) {
		charger_proc_file->read_proc = charger_read_proc;
		charger_proc_file->write_proc = charger_write_proc;
	} else {
		printk(DBGMSK_BAT_ERR "[BAT][PM8921]proc file create failed!\n");
	}
	return;
}
