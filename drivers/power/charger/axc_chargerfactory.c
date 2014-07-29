#include <linux/slab.h>
#include "axc_chargerfactory.h"
#include "axc_Smb346Charger.h"
#include "axc_PM8226Charger.h"

#include <linux/asus_bat.h>
#include "axc_DummyCharger.h"
#include "../axi_powerglobal.h"


//#include "../asus_bat_dbg.h"
void AXC_ChargerFactory_GetCharger(AXE_CHARGER_TYPE aeChargerType , AXI_Charger **appCharger)
{
	if (NULL != *appCharger)  {
		printk(DBGMSK_BAT_ERR "[BAT][ChagerFactory]Memory leak...\n");
	}

	*appCharger =  NULL;
	switch(aeChargerType) {
#ifdef CONFIG_SMB_346_CHARGER
	case E_SMB346_CHARGER_TYPE:
	{
		static AXC_SMB346Charger *lpCharger = NULL;
		if(NULL == lpCharger)
		{
			lpCharger = kzalloc(sizeof(AXC_SMB346Charger), GFP_KERNEL);
			BUG_ON(NULL == lpCharger);
		}		

		*appCharger = &lpCharger->msParentCharger ;
		AXC_SMB346Charger_Binding(*appCharger, aeChargerType);
		break;
	}
#endif

#ifdef CONFIG_PM_8921_CHARGER
	case E_PM8921_CHARGER_TYPE:
	{
		static AXC_PM8921Charger *lpCharger = NULL;
		if(NULL == lpCharger)
		{
			lpCharger = kzalloc(sizeof(AXC_PM8921Charger), GFP_KERNEL);
		}		

		*appCharger = &lpCharger->msParentCharger ;
		AXC_PM8921Charger_Binding(*appCharger, aeChargerType);
		break;
	}
#endif

#ifdef CONFIG_PM_8941_CHARGER
	case E_PM8941_CHARGER_TYPE:
	{
		static AXC_PM8941Charger *lpCharger = NULL;
		if(NULL == lpCharger){
			lpCharger = kzalloc(sizeof(AXC_PM8941Charger), GFP_KERNEL);
		}

		*appCharger = &lpCharger->msParentCharger ;
		AXC_PM8941Charger_Binding(*appCharger, aeChargerType);
		break;
	}
#endif

#ifdef CONFIG_PM_8226_CHARGER
	case E_PM8226_CHARGER_TYPE:
	{
		static AXC_PM8226Charger *lpCharger = NULL;
		if(NULL == lpCharger){
			lpCharger = kzalloc(sizeof(AXC_PM8226Charger), GFP_KERNEL);
		}

		*appCharger = &lpCharger->msParentCharger ;
		AXC_PM8226Charger_Binding(*appCharger, aeChargerType);
		break;
	}
#endif
	
//#ifdef CONFIG_DUMMY_CHARGER		
    case  E_DUMMY_CHARGER_TYPE:
    {
            static AXC_DummyCharger *lpCharger = NULL;
			if(NULL == lpCharger)
            {
                lpCharger = kzalloc(sizeof(AXC_DummyCharger), GFP_KERNEL);
                assert(NULL != lpCharger);
            }		

            *appCharger = &lpCharger->msParentCharger ;
            AXC_DummyCharger_Binding(*appCharger, aeChargerType);
            break;
     }
//#endif
	default:
		printk(DBGMSK_BAT_ERR "[BAT][ChagerFactory]Not defined type...\n");
		break;
	}
	return;
}

void AXC_ChargerFactory_FreeCharger(AXI_Charger *apCharger)
{
	if (NULL == apCharger)
		return;

	switch(apCharger->GetType(apCharger)) {
 #ifdef CONFIG_SMB_346_CHARGER
	case E_SMB346_CHARGER_TYPE:
	{
		AXC_SMB346Charger *lpCharger = container_of(apCharger, AXC_SMB346Charger, msParentCharger);
		kfree(lpCharger);
		break;
	}
#endif

#ifdef CONFIG_PM_8921_CHARGER
	case E_PM8921_CHARGER_TYPE:
	{
		AXC_PM8921Charger *lpCharger = container_of(apCharger, AXC_PM8921Charger, msParentCharger);
		kfree(lpCharger);
		break;
	}
#endif

#ifdef CONFIG_PM_8226_CHARGER
	case E_PM8226_CHARGER_TYPE:
	{
		AXC_PM8226Charger *lpCharger = container_of(apCharger, AXC_PM8226Charger, msParentCharger);
		kfree(lpCharger);
		break;
	}
#endif

#ifdef CONFIG_DUMMY_CHARGER			
	case E_DUMMY_CHARGER_TYPE:
   {
                AXC_DummyCharger *lpCharger = container_of(apCharger, AXC_DummyCharger, msParentCharger);
                kfree(lpCharger);
				break;
   }
#endif
	default:
		printk(DBGMSK_BAT_ERR "[BAT][FreeChager]Not defined type...\n");
		break;
	}
}

//Eason_Chang:for PM8226 internal ChgGau+++
#ifdef CONFIG_PM_8226_CHARGER
extern int getIfonline_pm8226(void);
extern void AcUsbPowerSupplyChange_pm8226(void);
extern void setChgDrawCurrent_pm8226(void);
extern void UsbSetOtgSwitch_pm8226(bool switchOtg);
extern int registerChargerInOutNotificaition_pm8226(void (*callback)(int));
#else
extern int getIfonline_smb346(void);
extern void AcUsbPowerSupplyChange_smb346(void);
extern void setChgDrawCurrent_smb346(void);
extern void UsbSetOtgSwitch_smb346(bool switchOtg);
extern int registerChargerInOutNotificaition_smb346(void (*callback)(int));
#endif

int getIfonline(void)
{
#ifdef CONFIG_PM_8226_CHARGER
	return getIfonline_pm8226();
#else
	return getIfonline_smb346();
#endif
}

void AcUsbPowerSupplyChange(void)
{
#ifdef CONFIG_PM_8226_CHARGER
	AcUsbPowerSupplyChange_pm8226();
#else
	AcUsbPowerSupplyChange_smb346();
#endif
}

void setChgDrawCurrent(void)
{
#ifdef CONFIG_PM_8226_CHARGER
	setChgDrawCurrent_pm8226();
#else
	setChgDrawCurrent_smb346();
#endif
}

void UsbSetOtgSwitch(bool switchOtg)
{
#ifdef CONFIG_PM_8226_CHARGER
	UsbSetOtgSwitch_pm8226(switchOtg);
#else
	UsbSetOtgSwitch_smb346(switchOtg);
#endif
}

int registerChargerInOutNotificaition(void (*callback)(int))
{
#ifdef CONFIG_PM_8226_CHARGER
	return registerChargerInOutNotificaition_pm8226(callback);
#else
	return registerChargerInOutNotificaition_smb346(callback);
#endif
}

#ifndef ASUS_FACTORY_BUILD
#ifdef CONFIG_PM_8226_CHARGER
extern void setChgLimitThermalRuleDrawCurrent_pm8226(bool isSuspendCharge);
extern void setChgLimitInPadWhenChgReset_pm8226(void);
#else
extern void setChgLimitThermalRuleDrawCurrent_smb346(bool isSuspendCharge);
extern void setChgLimitInPadWhenChgReset_smb346(void);
#endif

#ifdef CONFIG_EEPROM_NUVOTON
void setChgLimitThermalRuleDrawCurrent(bool isSuspendCharge)
{
#ifdef CONFIG_PM_8226_CHARGER
	setChgLimitThermalRuleDrawCurrent_pm8226(isSuspendCharge);
#else
	setChgLimitThermalRuleDrawCurrent_smb346(isSuspendCharge);
#endif
}
#endif

#ifdef CONFIG_EEPROM_NUVOTON
void setChgLimitInPadWhenChgReset(void)
{
#ifdef CONFIG_PM_8226_CHARGER
	setChgLimitInPadWhenChgReset_pm8226();
#else
	setChgLimitInPadWhenChgReset_smb346();
#endif
}
#endif

#endif //ASUS_FACTORY_BUILD
//Eason_Chang:for PM8226 internal ChgGau---
