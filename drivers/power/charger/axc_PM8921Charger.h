/*                                                                                                                                                       
        PM8921 Charge IC include file

*/
#ifndef __AXC_PM8921CHARGER_H__
#define __AXC_PM8921CHARGER_H__
#include <linux/types.h>
#include "axi_charger.h"         
#include <linux/workqueue.h>
#include <linux/param.h>

//#define STAND_ALONE_WITHOUT_USB_DRIVER

#if 0
#define HIGH (1)                                                                                                                                         
#define LOW  (0)
#define CHARGE_PIN1_GPIO (MFP_PIN_GPIO57)
#define CHARGE_PIN2_GPIO (MFP_PIN_GPIO58)
#define CHARGE_DISABLE_GPIO (MFP_PIN_GPIO99)
#define CHARGE_PLUG_IN_GPIO     (MFP_PIN_GPIO13)        
#define CHARGE_FAULT_GPIO (MFP_PIN_GPIO20)
#define CHARGE_DONE_GPIO (MFP_PIN_GPIO36)
#define CHARGE_STATUS_GPIO (MFP_PIN_GPIO19)
#define BATTERY_LOW     (MFP_PIN_GPIO8) 
#endif

#define TIME_FOR_NOTIFY_AFTER_PLUGIN_CABLE (60*HZ) //5s

typedef struct AXC_PM8921Charger {
	bool mbInited;
	int mnType;
	AXE_Charger_Type type;
	int mnPin1;
	int mnPin2;
	int mnChargerDisablePin;
	int mnChargePlugInPin;
	int mnChargeFaultPin;
	int mnChargeDonePin;
	int mnChargeStatusPin;
	//int mnBatteryLowPin;

	bool m_is_bat_valid;

	AXI_ChargerStateChangeNotifier *mpNotifier;

        AXI_Charger msParentCharger;

	//struct delayed_work msNotifierWorker;

#ifdef CHARGER_SELF_TEST_ENABLE
	AXI_SelfTest msParentSelfTest; 
#endif //CHARGER_SELF_TEST_ENABLE
}AXC_PM8921Charger;

extern void AXC_PM8921Charger_Binding(AXI_Charger *apCharger,int anType);
#endif //__AXC_PM8921CHARGER_H__

