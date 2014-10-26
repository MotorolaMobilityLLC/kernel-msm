/*
        ChargerFactory include file

*/
#ifndef __AXC_CHARGERFACTORY_H__
#define __AXC_CHARGERFACTORY_H__
#include "axi_charger.h"

typedef enum {
        E_SMB346_CHARGER_TYPE,
        E_PM8921_CHARGER_TYPE,		// for Qualcomm msm8960, pm8921
        E_PM8941_CHARGER_TYPE,		// for Qualcomm msm8974AB, pm8941
        E_PM8226_CHARGER_TYPE,
        E_DUMMY_CHARGER_TYPE,
        E_UNKNOWN_CHARGER_TYPE
}AXE_CHARGER_TYPE;                                                                                                                                      


extern void AXC_ChargerFactory_GetCharger(AXE_CHARGER_TYPE aeChargerType , AXI_Charger **appCharger);
extern void AXC_ChargerFactory_FreeCharger(AXI_Charger *apCharger);


#endif //__AXC_CHARGERFACTORY_H__

