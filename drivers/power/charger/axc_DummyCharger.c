/*
        Maxim 8934 Charger IC Implementation

*/
/*
	Definition on TD
	GPIO57	GPIO58	N/A
	PEN1	PEN2	USUS
	H	x	x	3000/Rpset
	L	H	L	475mA
	L	L	L	95mA
	L	x	H	0mA(UsbSuspend)
*/
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
//#include <mach/mfp-pxa910.h>
#include "axc_DummyCharger.h"
#include <asm/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/asus_bat.h>

 #if 0 //Eason_Chang//
// For checking initializing or not
static bool AXC_Maxim8934_Charger_GetGPIO(int anPin)
{
	return gpio_get_value(anPin);
}

static void AXC_Maxim8934_Charger_SetGPIO(int anPin, bool abHigh)
{
	gpio_set_value(anPin,abHigh);
	printk(DBGMSK_BAT_DEBUG "[BAT][Maxim8934]SetGPIO pin%d,%d:%d\n",anPin,abHigh,AXC_Maxim8934_Charger_GetGPIO(anPin));
}

static int AXC_Maxim8934_Charger_RequestPin(int anPin, const char *apGPIORequestLabel)
{
    int err = 0;

	err = gpio_request(anPin, apGPIORequestLabel);
	if (err) {
		printk(DBGMSK_BAT_ERR "[BAT][Maxim8934]Can't config GPIO %s:%d\n",apGPIORequestLabel,err);
	}
	return err;
}

static void AXC_Maxim8934_Charger_SetPIN1(AXC_Maxim8934Charger *this,bool abHigh)
{
	AXC_Maxim8934_Charger_SetGPIO(this->mnPin1,abHigh);
}

static void AXC_Maxim8934_Charger_SetPIN2(AXC_Maxim8934Charger *this,bool abHigh)
{
	AXC_Maxim8934_Charger_SetGPIO(this->mnPin2,abHigh);
}

static void AXC_Maxim8934_Charger_SetChargrDisbalePin(AXC_Maxim8934Charger *this,bool abDisabled)
{
	AXC_Maxim8934_Charger_SetGPIO(this->mnChargerDisablePin,abDisabled);
}

static int AXC_Maxim8934_Charger_InitGPIO(AXC_Maxim8934Charger *this)
{
	int err = 0;

	this->mnPin1 = mfp_to_gpio(CHARGE_PIN1_GPIO);
	this->mnPin2 = mfp_to_gpio(CHARGE_PIN2_GPIO);
	this->mnChargerDisablePin = mfp_to_gpio(CHARGE_DISABLE_GPIO);
	this->mnChargePlugInPin = mfp_to_gpio(CHARGE_PLUG_IN_GPIO);
	this->mnChargeFaultPin = mfp_to_gpio(CHARGE_FAULT_GPIO);
	this->mnChargeDonePin = mfp_to_gpio(CHARGE_DONE_GPIO);
	this->mnChargeStatusPin = mfp_to_gpio(CHARGE_STATUS_GPIO);
	//this->mnBatteryLowPin = mfp_to_gpio(BATTERY_LOW);

	err = AXC_Maxim8934_Charger_RequestPin(this->mnPin1,"Maxim8934 Charger PEN1");
	if(err) goto err_exit;

	err = AXC_Maxim8934_Charger_RequestPin(this->mnPin2, "Maxim8934 Charger PEN2");	
	if(err) goto err_exit;

#ifdef STAND_ALONE_WITHOUT_USB_DRIVER
	//Should be configured by usb driver...
	err = AXC_Maxim8934_Charger_RequestPin(this->mnChargePlugInPin, "DC In");
	if(err) goto err_exit;
#endif /* STAND_ALONE_WITHOUT_USB_DRIVER */

	err = AXC_Maxim8934_Charger_RequestPin(this->mnChargerDisablePin, "Charger Disable");
	if(err) goto err_exit;

    err = AXC_Maxim8934_Charger_RequestPin(this->mnChargeFaultPin, "Charge Fault");
    if(err) goto err_exit;

    err = AXC_Maxim8934_Charger_RequestPin(this->mnChargeDonePin, "Charge Done");
    if(err) goto err_exit;

    err = AXC_Maxim8934_Charger_RequestPin(this->mnChargeStatusPin, "Charge Status");
    if(err) goto err_exit;

    //err = AXC_Maxim8934_Charger_RequestPin(this->mnBatteryLowPin, "Battery Low");
    //if(err) goto err_exit;

	//if nr error
	gpio_direction_output(this->mnPin1, 0);
	gpio_direction_output(this->mnPin2, 1);

#ifdef STAND_ALONE_WITHOUT_USB_DRIVER
	//Should be configured by usb driver...
	gpio_direction_input(this->mnChargePlugInPin);
#endif

	gpio_direction_output(this->mnChargerDisablePin, 0);
	gpio_direction_input(this->mnChargeFaultPin);
	gpio_direction_input(this->mnChargeDonePin);
	gpio_direction_input(this->mnChargeStatusPin);
	//gpio_direction_input(this->mnBatteryLowPin);
	gpio_free(this->mnPin1);
	gpio_free(this->mnPin2);

#ifdef STAND_ALONE_WITHOUT_USB_DRIVER
	//Should be configured by usb driver...
	gpio_free(this->mnChargePlugInPin);
#endif

	gpio_free(this->mnChargerDisablePin);
	gpio_free(this->mnChargeFaultPin);
	gpio_free(this->mnChargeDonePin);
	gpio_free(this->mnChargeStatusPin);
	//gpio_free(this->mnBatteryLowPin);

err_exit:
	return err;
}

static void AXC_Maxim8934_Charger_DeInitGPIO(AXC_Maxim8934Charger *this)
{
}
 #endif  //Eason_Chang//
/*
static void AXC_Maxim8934_Charger_NotifyClientForStablePlugIn(struct work_struct *dat)
{
	AXC_Maxim8934Charger *this = container_of(dat,
                                                AXC_Maxim8934Charger,
                                                msNotifierWorker.work);

	this->msParentCharger.SetCharger(&this->msParentCharger, STABLE_CHARGER);
}
*/

#if 0//Eason_Chang// 
extern void create_charger_proc_file(void);
//Function implementation for AXC_Maxim8934_Charger
#endif //Eason_Chang// 

static void AXC_Dummy_Charger_Init(AXI_Charger *apCharger)
{
    AXC_DummyCharger *this = container_of(apCharger,
                                            AXC_DummyCharger,
                                            msParentCharger);

    if(false == this->mbInited)
    {
		printk(DBGMSK_BAT_DEBUG "[BAT][Dummy]Init++\n");
		this->type = NO_CHARGER_TYPE;
		this->mpNotifier = NULL;
		//INIT_DELAYED_WORK(&this->msNotifierWorker, AXC_Maxim8934_Charger_NotifyClientForStablePlugIn) ;

#if 0 //Eason_Chang//
		if (0 == AXC_Maxim8934_Charger_InitGPIO(this)) {
			this->mbInited = true;
		} else {
			printk(DBGMSK_BAT_ERR "[BAT][Maxim8934]Charger can't init\n");
        }
#endif  //Eason_Chang//
        
		//Eason_Chang//create_charger_proc_file();
		printk(DBGMSK_BAT_DEBUG "[BAT][Dummy]Init--\n");
    }
}

static void AXC_Dummy_Charger_DeInit(AXI_Charger *apCharger)
{
 #if 0 //Eason_Chang//
    AXC_DummyCharger *this = container_of(apCharger,
                                            AXC_DummyCharger,
                                            msParentCharger);
    if(true == this->mbInited) {
	AXC_Maxim8934_Charger_DeInitGPIO(this);
        this->mbInited = false;
    }
 #endif  //Eason_Chang//
}

int AXC_Dummy_Charger_GetType(AXI_Charger *apCharger)
{
    AXC_DummyCharger *this = container_of(apCharger,
                                            AXC_DummyCharger,
                                            msParentCharger);

    return this->mnType;
}

void AXC_Dummy_Charger_SetType(AXI_Charger *apCharger ,int anType)
{
    AXC_DummyCharger *this = container_of(apCharger,
                                            AXC_DummyCharger,
                                            msParentCharger);

    this->mnType = anType;
}

static AXE_Charger_Type AXC_Dummy_Charger_GetChargerStatus(AXI_Charger *apCharger)
{
	AXC_DummyCharger *this = container_of(apCharger,
                                                AXC_DummyCharger,
                                                msParentCharger);

	return this->type;
}

// joshtest
#if 0//Eason_Chang//
static struct timespec g_charger_update_time = {0};

static void chg_set_charger_update_time(void)
{
	g_charger_update_time = current_kernel_time();
	printk(DBGMSK_BAT_TRACE "[BAT][Dummy] %s(), tv_sec:%d\n",
		__func__,
		(int)g_charger_update_time.tv_sec);
	return;
}


void chg_get_charger_update_time(struct timespec *charger_update_time)
{
	assert(NULL != charger_update_time);
	*charger_update_time = g_charger_update_time;
	printk(DBGMSK_BAT_TRACE "[BAT][Dummy] %s(), tv_sec:%d\n",
		__func__,
		(int)charger_update_time->tv_sec);
	return;
}
#endif //Eason_Chang//

// joshtest

static void AXC_Dummy_Charger_SetCharger(AXI_Charger *apCharger , AXE_Charger_Type aeChargerType)
{
	static bool first_call = true;

    AXC_DummyCharger *this = container_of(apCharger,
                                            AXC_DummyCharger,
                                            msParentCharger);

	if(false == this->mbInited)
		return;

	if (!first_call && !this->m_is_bat_valid) {
		printk(KERN_INFO "[BAT][Dummy] %s(), battery is invalid and cannot charging\n", __func__);
		aeChargerType = NO_CHARGER_TYPE;
	}

	printk(DBGMSK_BAT_DEBUG "[BAT][Dummy]CharegeModeSet:%d\n",aeChargerType);

	switch(aeChargerType)
    {
		case NO_CHARGER_TYPE:
 #if 0 //Eason_Chang//			
			AXC_Maxim8934_Charger_SetPIN1(this,LOW);
			AXC_Maxim8934_Charger_SetPIN2(this,LOW);
			AXC_Maxim8934_Charger_SetChargrDisbalePin(this,HIGH);
 #endif  //Eason_Chang//			
			this->type = aeChargerType;
			//cancel_delayed_work(&this->msNotifierWorker);
			if(NULL != this->mpNotifier)
				this->mpNotifier->Notify(&this->msParentCharger,this->type);
			break;
            case ILLEGAL_CHARGER_TYPE:
		case LOW_CURRENT_CHARGER_TYPE:
 #if 0 	 //Eason_Chang//
			AXC_Maxim8934_Charger_SetPIN1(this,LOW);
			AXC_Maxim8934_Charger_SetPIN2(this,HIGH);
			AXC_Maxim8934_Charger_SetChargrDisbalePin(this,LOW);
 #endif  //Eason_Chang//			
			this->type = aeChargerType;
			if(NULL != this->mpNotifier)
				this->mpNotifier->Notify(&this->msParentCharger,this->type);
			//cancel_delayed_work(&this->msNotifierWorker);
			//schedule_delayed_work(&this->msNotifierWorker , round_jiffies_relative(TIME_FOR_NOTIFY_AFTER_PLUGIN_CABLE));
			break;

		case HIGH_CURRENT_CHARGER_TYPE:
		    //For SR1, we should not support ac mode to prevent device broken.
		    //Reenable for SR2@100325
		    
            //Eason_Chang//AXC_Maxim8934_Charger_SetPIN1(this,HIGH);
		    //AXC_Maxim8934_Charger_SetPIN1(this,LOW);
		    //AXC_Maxim8934_Charger_SetPIN2(this,HIGH);
            //Eason_Chang//AXC_Maxim8934_Charger_SetChargrDisbalePin(this,LOW);
			this->type = aeChargerType;
			if(NULL != this->mpNotifier)
				this->mpNotifier->Notify(&this->msParentCharger,this->type);
			//cancel_delayed_work(&this->msNotifierWorker);
			//schedule_delayed_work(&this->msNotifierWorker , round_jiffies_relative(TIME_FOR_NOTIFY_AFTER_PLUGIN_CABLE));
			break;
 #if 0 //Eason_Chang//	
		case TEMP_NO_CHARGER_TYPE:
		
				AXC_Maxim8934_Charger_SetPIN1(this,LOW);
				AXC_Maxim8934_Charger_SetPIN2(this,LOW);
				AXC_Maxim8934_Charger_SetChargrDisbalePin(this,HIGH);

			break;
 #endif  //Eason_Chang//				
		/*
		case STABLE_CHARGER:
			this->type = aeChargerType;
			if(true == this->msParentCharger.IsCharegrPlugin(&this->msParentCharger))
            {
				if(NULL != this->mpNotifier)
					this->mpNotifier->Notify(&this->msParentCharger,this->type);
			}
            else
				this->msParentCharger.SetCharger(&this->msParentCharger, NO_CHARGER_TYPE);
		    break;
		*/

		default:
			printk(DBGMSK_BAT_ERR "[BAT][Dummy]Wrong ChargerMode:%d\n",aeChargerType);
			break;
	}

	if (first_call) {
		first_call = false;
	}
}

static void AXC_Dummy_Charger_SetBatConfig(AXI_Charger *apCharger , bool is_bat_valid)
{
	AXC_DummyCharger *this = container_of(apCharger,
                                                AXC_DummyCharger,
                                                msParentCharger);
	if (is_bat_valid) {
		printk(KERN_INFO "[BAT]%s, bat is valid\n", __func__);
	}

	this->m_is_bat_valid = is_bat_valid;

	return;
}

static bool AXC_Dummy_Charger_IsCharegrPlugin(AXI_Charger *apCharger)
{
#if 0 //Eason_Chang//
	AXC_DummyCharger *this = container_of(apCharger,
                                                AXC_DummyCharger,
                                                msParentCharger);

//Eason_Chang//#ifdef STAND_ALONE_WITHOUT_USB_DRIVER
	//Should be configured by usb driver...
	//Eason_Chang//return (!AXC_Maxim8934_Charger_GetGPIO(this->mnChargePlugInPin));
//Eason_Chang//#else
	return (this->type != NO_CHARGER_TYPE);
#endif
    return true;//Eason_Chang//
}

static bool AXC_Dummy_Charger_IsCharging(AXI_Charger *apCharger)
{
#if 0 //Eason_Chang//
	AXC_DummyCharger *this = container_of(apCharger,
                                                AXC_DummyCharger,
                                                msParentCharger);

	if(0 != gpio_get_value(this->mnChargeStatusPin))
		return false;
#endif//Eason_Chang//	
	return true;
}
/*
static bool AXC_Dummy_Charger_Test(void *apTestObject)
{
	return true;
}
*/
static void AXC_DummyCharger_RegisterChargerStateChanged(struct AXI_Charger *apCharger, AXI_ChargerStateChangeNotifier *apNotifier
                                                            ,AXE_Charger_Type chargerType)
{
    AXC_DummyCharger *this = container_of(apCharger,
                                            AXC_DummyCharger,
                                            msParentCharger);

	this->mpNotifier = apNotifier;
}

static void AXC_DummyCharger_DeregisterChargerStateChanged(struct AXI_Charger *apCharger,AXI_ChargerStateChangeNotifier *apNotifier)
{
    AXC_DummyCharger *this = container_of(apCharger,
                                            AXC_DummyCharger,
                                            msParentCharger);

	if(this->mpNotifier == apNotifier)
		this->mpNotifier = NULL;
}
static void EnableCharging(struct AXI_Charger *apCharger, bool enabled)
{
}
void AXC_DummyCharger_Binding(AXI_Charger *apCharger,int anType)
{
    AXC_DummyCharger *this = container_of(apCharger,
                                            AXC_DummyCharger,
                                            msParentCharger);

    this->msParentCharger.Init = AXC_Dummy_Charger_Init;
    this->msParentCharger.DeInit = AXC_Dummy_Charger_DeInit;
    this->msParentCharger.GetType = AXC_Dummy_Charger_GetType;
    this->msParentCharger.SetType = AXC_Dummy_Charger_SetType;
    this->msParentCharger.GetChargerStatus = AXC_Dummy_Charger_GetChargerStatus;
    this->msParentCharger.SetCharger = AXC_Dummy_Charger_SetCharger;
    this->msParentCharger.EnableCharging = EnableCharging;
	this->msParentCharger.SetBatConfig = AXC_Dummy_Charger_SetBatConfig;
    this->msParentCharger.IsCharegrPlugin = AXC_Dummy_Charger_IsCharegrPlugin;
    this->msParentCharger.IsCharging = AXC_Dummy_Charger_IsCharging;
	this->msParentCharger.RegisterChargerStateChanged= AXC_DummyCharger_RegisterChargerStateChanged;	
	this->msParentCharger.DeregisterChargerStateChanged= AXC_DummyCharger_DeregisterChargerStateChanged;	
    this->msParentCharger.SetType(apCharger,anType);

    //this->mbInited = false;
#ifdef CHARGER_SELF_TEST_ENABLE
    this->msParentSelfTest.Test = AXC_Dummy_Charger_Test;
#endif
}

#if 0//Eason_Chang// 
/*
Implement Interface for USB Driver
*/
#include "axc_chargerfactory.h"
unsigned GetChargerStatus(void)
{
	static AXI_Charger *lpCharger = NULL;

	if(NULL == lpCharger)
    {
		AXC_ChargerFactory_GetCharger(E_MAXIM8934_CHARGER_TYPE ,&lpCharger);
		lpCharger->Init(lpCharger);
	}

	return (unsigned) lpCharger->GetChargerStatus(lpCharger);
}

void SetCharegerUSBMode(void)
{
	static AXI_Charger *lpCharger = NULL;

	if(NULL == lpCharger)
    {
		AXC_ChargerFactory_GetCharger(E_MAXIM8934_CHARGER_TYPE ,&lpCharger);
		lpCharger->Init(lpCharger);
	}

	printk(DBGMSK_BAT_INFO "[BAT][Maxim8934]SetCharegerUSBMode\n");
	lpCharger->SetCharger(lpCharger,LOW_CURRENT_CHARGER_TYPE);
}

void SetCharegerACMode(void)
{
	static AXI_Charger *lpCharger = NULL;

	if(NULL == lpCharger)
    {
		AXC_ChargerFactory_GetCharger(E_MAXIM8934_CHARGER_TYPE ,&lpCharger);
		lpCharger->Init(lpCharger);
	}

	printk(DBGMSK_BAT_INFO "[BAT][Maxim8934]SetCharegerACMode\n");
	lpCharger->SetCharger(lpCharger,HIGH_CURRENT_CHARGER_TYPE);
}

void SetCharegerNoPluginMode(void)
{
	static AXI_Charger *lpCharger = NULL;

	if(NULL == lpCharger)
    {
		AXC_ChargerFactory_GetCharger(E_MAXIM8934_CHARGER_TYPE ,&lpCharger);
		lpCharger->Init(lpCharger);
	}

    printk(DBGMSK_BAT_INFO "[BAT][Maxim8934]SetCharegerNoPluginMode\n");
	lpCharger->SetCharger(lpCharger,NO_CHARGER_TYPE);
}

static ssize_t charger_read_proc(char *page, char **start, off_t off,
				int count, int *eof, void *data)
{
	static AXI_Charger *lpCharger = NULL;

	if(NULL == lpCharger)
    {
		AXC_ChargerFactory_GetCharger(E_MAXIM8934_CHARGER_TYPE ,&lpCharger);
		lpCharger->Init(lpCharger);
	}

	return sprintf(page, "%d\n", lpCharger->GetChargerStatus(lpCharger));
}

static ssize_t charger_write_proc(struct file *filp,
		const char *buff, size_t len, loff_t *off)
{
	int val;

	char messages[256], vol[256];

	if (len > 256) {
		len = 256;
	}

	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	
	val = (int)simple_strtol(messages, NULL, 10);

    {
	    static AXI_Charger *lpCharger = NULL;

	    if(NULL == lpCharger)
        {
		    AXC_ChargerFactory_GetCharger(E_MAXIM8934_CHARGER_TYPE ,&lpCharger);
		    lpCharger->Init(lpCharger);
	    }

	    lpCharger->SetCharger(lpCharger,val);
    }
	//UpdateBatteryLifePercentage(gpGauge, val);
	
	return len;
}

void create_charger_proc_file(void)
{
	struct proc_dir_entry *charger_proc_file = create_proc_entry("driver/charger", 0644, NULL);

	if (charger_proc_file) {
		charger_proc_file->read_proc = charger_read_proc;
		charger_proc_file->write_proc = charger_write_proc;
	}
    else {
		printk(DBGMSK_BAT_ERR "[BAT]proc file create failed!\n");
    }

	return;
}

#endif //Eason_Chang// 