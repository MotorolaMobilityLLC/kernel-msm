/*
        Charger include file

*/
#ifndef __AXI_CHARGER_H__
#define __AXI_CHARGER_H__
#include <linux/types.h>
#include "../AXI_Basic_Define.h"


/*

typedef enum
{
    NO_CHARGER_TYPE,
    LOW_CURRENT_CHARGER_TYPE,
    HIGH_CURRENT_CHARGER_TYPE,
    TEMP_NO_CHARGER_TYPE,
    ERROR_CHARGER,
    //STABLE_CHARGER,
    //USB_SUSPEND,				
    //UNKNOWN_CHARGER_MODE,
}AXE_Charger_Mode;
*/
struct AXI_Charger;

typedef struct AXI_ChargerStateChangeNotifier{

	void (*Notify)(struct AXI_Charger *apCharger, AXE_Charger_Type aeCharger_Mode);
	void (*onChargingStart)(struct AXI_Charger *apCharger, bool startCharging);

}AXI_ChargerStateChangeNotifier;

typedef struct AXI_Charger {
	void (*Init)(struct AXI_Charger *apCharger);
	void (*DeInit)(struct AXI_Charger *apCharger);
	int (*GetType)(struct AXI_Charger *apCharger);
	void (*SetType)(struct AXI_Charger *apCharger,int anType);
	AXE_Charger_Type (*GetChargerStatus)(struct AXI_Charger *apCharger);
	void (*SetCharger)(struct AXI_Charger *apCharger, AXE_Charger_Type aeChargerType);
       void (*EnableCharging)(struct AXI_Charger *apCharger, bool enabled);
	void (*SetBatConfig)(struct AXI_Charger *apCharger, bool is_bat_valid);
	bool (*IsCharegrPlugin)(struct AXI_Charger *apCharger);
	bool (*IsCharging)(struct AXI_Charger *apCharger);
	void (*RegisterChargerStateChanged)(struct AXI_Charger *apCharger, AXI_ChargerStateChangeNotifier *apNotifier,
                                        AXE_Charger_Type aeCharger_Mode);
	void (*DeregisterChargerStateChanged)(struct AXI_Charger *apCharger, AXI_ChargerStateChangeNotifier *apNotifier);
}AXI_Charger;

//Eason: Pad draw rule +++
typedef enum{
	PadDraw300 = 0,
	PadDraw500,
	PadDraw700,
	PadDraw900
}PadDrawLimitCurrent_Type;
PadDrawLimitCurrent_Type JudgePadRuleDrawLimitCurrent(bool isSuspendCharge);
//Eason: Pad draw rule ---

#endif //__AXI_CHARGER_H__

