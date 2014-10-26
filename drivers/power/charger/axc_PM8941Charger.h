/*                                                                                                                                                       
        PM8941 Charge IC include file

*/
#ifndef __AXC_PM8941CHARGER_H__
#define __AXC_PM8941CHARGER_H__
#include <linux/types.h>
#include "axi_charger.h"         
#include <linux/workqueue.h>
#include <linux/param.h>
#include <linux/wakelock.h>

//#define STAND_ALONE_WITHOUT_USB_DRIVER

#define TIME_FOR_NOTIFY_AFTER_PLUGIN_CABLE (60*HZ) //5s

typedef struct AXC_PM8941Charger {
	bool mbInited;
	int mnType;
	AXE_Charger_Type type;
	unsigned int usb_current_max;
	
	AXI_ChargerStateChangeNotifier *mpNotifier;
	AXI_Charger msParentCharger;

	struct delayed_work asus_chg_work;    
	struct wake_lock cable_in_out_wakelock;
    struct timer_list charger_in_out_timer;
	//struct delayed_work msNotifierWorker;

#ifdef CHARGER_SELF_TEST_ENABLE
	AXI_SelfTest msParentSelfTest; 
#endif //CHARGER_SELF_TEST_ENABLE
}AXC_PM8941Charger;

extern void AXC_PM8941Charger_Binding(AXI_Charger *apCharger,int anType);
#endif //__AXC_PM8921CHARGER_H__

