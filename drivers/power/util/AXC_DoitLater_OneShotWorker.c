#ifdef CONFIG_ASUS_POWER_UTIL 
#include <linux/kernel.h>
#include "AXC_DoitLater_OneShotWorker.h"
#include <linux/delay.h>

static void  AXC_DoitLater_OneShotWorker_start(struct AXI_DoitLater* doitLater, struct AXI_DoitLaterTask *task,int delayTimeInMS, int times)
{
    AXC_DoitLater_OneShotWorker *this = container_of(doitLater,
    										AXC_DoitLater_OneShotWorker,
    										miParent);
    if(NULL  == this->mpQueue){

        this->mpQueue = create_singlethread_workqueue("DoitLater");

        INIT_DELAYED_WORK(&this->msWorker, this->mImpl.doWhenScheduled) ;

    }
    this->mpTask = task;
    
    pr_debug(DOITLATER_DEBUG,"schedule a task..");

    queue_delayed_work(this->mpQueue, &this->msWorker, delayTimeInMS*HZ);
    
}
static void AXC_DoitLater_OneShotWorker_stop(struct AXI_DoitLater* doitLater)
{
    AXC_DoitLater_OneShotWorker *this = container_of(doitLater,
    										AXC_DoitLater_OneShotWorker,
    										miParent);

    pr_debug(DOITLATER_DEBUG,"stop the task");

    cancel_delayed_work_sync(&this->msWorker);      
}
static void AXC_DoitLater_OneShotWorker_doWhenScheduled(struct work_struct *dat)
{
    AXC_DoitLater_OneShotWorker *this = container_of(dat, AXC_DoitLater_OneShotWorker, msWorker.work);

    pr_debug(DOITLATER_DEBUG,"start doing the task");

    BUG_ON(NULL == this->mpTask);

    if(NULL != this->mpTask){

        this->mpTask->dotask(this->mpTask);
    }
}
void AXC_DoitLater_OneShotWorker_constructor(AXC_DoitLater_OneShotWorker *this)
{
    pr_debug(DOITLATER_DEBUG,__FUNCTION__);
    this->miParent.start = AXC_DoitLater_OneShotWorker_start;
    this->miParent.stop = AXC_DoitLater_OneShotWorker_stop;
    this->mImpl.doWhenScheduled = AXC_DoitLater_OneShotWorker_doWhenScheduled;
    this->mpQueue = NULL;
    this->mpTask = NULL;
}
#endif

