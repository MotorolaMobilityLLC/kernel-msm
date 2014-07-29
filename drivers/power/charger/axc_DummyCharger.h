/*                                                                                                                                                       
        Maxim8934 Charge IC include file

*/
#ifndef __AXC_DUMMYCHARGER_H__
#define __AXC_DUMMYCHARGER_H__
#include <linux/types.h>
#include "axi_charger.h"         
#include <linux/workqueue.h>
#include <linux/param.h>

typedef struct AXC_DummyCharger {

	bool mbInited;
	int mnType;
	AXE_Charger_Type type;

	bool m_is_bat_valid;

	AXI_ChargerStateChangeNotifier *mpNotifier;

        AXI_Charger msParentCharger;

}AXC_DummyCharger;

extern void AXC_DummyCharger_Binding(AXI_Charger *apCharger,int anType);
#endif //__AXC_DUMMYCHARGER_H__