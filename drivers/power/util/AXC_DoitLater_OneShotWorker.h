
/*
	AXC_DoitLater_OneShotWorker include file

*/
#ifndef __AXC_DOITLATER_ONESHOTWORKER_H__
#define __AXC_DOITLATER_ONESHOTWORKER_H__
#include "AXI_DoitLater.h"
#include <linux/workqueue.h>
struct AXC_DoitLater_OneShotWorker_Impl;
struct AXC_DoitLater_OneShotWorker;


typedef struct AXC_DoitLater_OneShotWorker_Impl{
    void (*doWhenScheduled)(struct work_struct *dat);
}AXC_DoitLater_OneShotWorker_Impl;

//provide one shot Do it later interface...
typedef struct AXC_DoitLater_OneShotWorker{
    AXI_DoitLater miParent;
    
    AXI_DoitLaterTask *mpTask;
    struct workqueue_struct *mpQueue;
    struct delayed_work msWorker;

    AXC_DoitLater_OneShotWorker_Impl mImpl;
    
}AXC_DoitLater_OneShotWorker;

extern void AXC_DoitLater_OneShotWorker_constructor(AXC_DoitLater_OneShotWorker *worker);

#endif //__AXC_DOITLATER_ONESHOTWORKER_H__

