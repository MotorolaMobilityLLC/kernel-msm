/*                                                                                                                                                       
        Smb346 Charge IC include file

*/
#ifndef __AXC_SMB346Charger_H__
#define __AXC_SMB346Charger_H__
#include <linux/types.h>
#include "axi_charger.h"         

//#define STAND_ALONE_WITHOUT_USB_DRIVER

// #define CHARGER_SELF_TEST_ENABLE

#ifdef CHARGER_SELF_TEST_ENABLE

#include "../selftest/axi_selftest.h"

#endif //CHARGER_SELF_TEST_ENABLE

//#define CONFIG_ENABLE_PIN2
//#define CONFIG_ENABLE_CHG_DONE_PIN
//#define CONFIG_ENABLE_CHG_FAULT_PIN

#include <linux/workqueue.h>
#include <linux/param.h>
#include <linux/irq.h>
#include <linux/wakelock.h>

//#define ENABLE_WATCHING_STATUS_PIN_IN_IRQ
typedef struct Smb346_PIN {
    int gpio;
    const char * name;
    int irq;
    int in_out_flag;
    irqreturn_t (*handler)(int, void *);
    int init_value;
    unsigned long trigger_flag;
    bool irq_enabled;
}Smb346_PIN;

typedef enum
{
    Smb346_DC_IN,
#ifdef CONFIG_ENABLE_PIN1        
    Smb346_PIN1,
#endif    
#ifdef CONFIG_ENABLE_PIN2
    Smb346_PIN2,
#endif
//#ifdef ASUS_A91_PROJECT
    Smb346_CHARGING_DISABLE,
//#endif
#ifdef CONFIG_ENABLE_CHG_IRQ_PIN
    Smb346_CHARGING_IRQ,
#endif
#ifdef CONFIG_ENABLE_CHG_DONE_PIN
    Smb346_CHARGING_DONE,
#endif
#ifdef CONFIG_ENABLE_CHG_FAULT_PIN
    Smb346_CHARGING_FAULT,
#endif
    Smb346_PIN_COUNT
}Smb346_PIN_DEF;

typedef struct AXC_SMB346Charger {

	bool mbInited;
	int mnType;
	AXE_Charger_Type type;
    
	Smb346_PIN *mpGpio_pin;
	//bool m_is_bat_valid;

	AXI_ChargerStateChangeNotifier *mpNotifier;

        AXI_Charger msParentCharger;

    struct delayed_work asus_chg_work;    

	//Eason: set Chg Limit In Pad When Charger Reset +++
    struct delayed_work setChgLimitInPadWhenChgResetWorker;
    //Eason: set Chg Limit In Pad When Charger Reset ---

    //Eason: check FC flag to disable charge, prevent Battery over charge, let gauge FCC abnormal+++
    struct delayed_work checkGaugeFCNeedDisableChgWorker;
    //Eason: check FC flag to disable charge, prevent Battery over charge, let gauge FCC abnormal---

    //struct delayed_work smb346_irq_worker;   //ASUS BSP Eason_Chang smb346 

    struct wake_lock cable_in_out_wakelock;

    struct timer_list charger_in_out_timer;

	//struct delayed_work msNotifierWorker;

#ifdef CHARGER_SELF_TEST_ENABLE

        AXI_SelfTest msParentSelfTest; 

#endif //CHARGER_SELF_TEST_ENABLE
}AXC_SMB346Charger;

extern void AXC_SMB346Charger_Binding(AXI_Charger *apCharger,int anType);
#endif //__AXC_SMB346Charger_H__

//ASUS BSP Eason_Chang smb346 +++
enum SMB346_CMD_ID {
        SMB346_CHG_CUR=0,
        SMB346_INPUT_CUR_LIM,
        SMB346_VARIOUS_FUNCTIONS,
        SMB346_FLOAT_VOLTAGE,
        SMB346_CHARGE_CONTROL,
        SMB346_STAT_TIMER_CONTROL,
        SMB346_PIN_ENABLE_CONTROL,
        SMB346_THERM_SYSTEM_CONTROL,
        SMB346_SYSOK_USB3p0,
        SMB346_OTHER_CONTROL_A,
        SMB346_TLIM_THERM_CONTROL,
        SMB346_HW_SW_LIMIT_CELL_TEMP,
        SMB346_FAULT_INTERRUPT,
        SMB346_STATUS_INTERRUPT,
        SMB346_I2C_BUS_SLAVE,
        SMB346_COMMAND_REG_A,
        SMB346_COMMAND_REG_B,
        SMB346_COMMAND_REG_C,
        SMB346_INTERRUPT_REG_A,
        SMB346_INTERRUPT_REG_B,
        SMB346_INTERRUPT_REG_C,
        SMB346_INTERRUPT_REG_D,
        SMB346_INTERRUPT_REG_E,
        SMB346_INTERRUPT_REG_F,
        SMB346_STATUS_REG_A,
        SMB346_STATUS_REG_B,
        SMB346_STATUS_REG_C,
        SMB346_STATUS_REG_D,
        SMB346_STATUS_REG_E,
};
//ASUS BSP Eason_Chang smb346 ---

