#include <linux/kernel.h>
#include <linux/slab.h>
#include "AXC_DoitLater_OneShotWorker.h"
#include "AXC_DoitLaterFactory.h"
#ifdef CONFIG_ASUS_POWER_UTIL 

AXI_DoitLater* get_DoitLater(AXE_DoitLater_Type type)
{
    AXI_DoitLater* lpDoitLater = NULL;

    switch(type){

        case E_ONE_SHOT_WORKER_TYPE:
        {
            AXC_DoitLater_OneShotWorker *lpWorker= NULL;

            lpWorker = kzalloc(sizeof(AXC_DoitLater_OneShotWorker), GFP_KERNEL);

            BUG_ON(NULL == lpWorker);

            AXC_DoitLater_OneShotWorker_constructor(lpWorker);

            lpDoitLater = &lpWorker->miParent;

            pr_debug(DOITLATER_DEBUG,"AXC_DoitLater_OneShotWorker created");
        }	
            break;
        default:
            break;
    }

    return lpDoitLater;
}
#endif

