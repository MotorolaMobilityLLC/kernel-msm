#ifdef CONFIG_I2C_STRESS_TEST
#include "i2c-stresstest.h"
#include <linux/kernel.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/random.h>
#include <linux/kthread.h>
#include <linux/slab.h>

#define ARRAY_AND_SIZE(x)       (x), ARRAY_SIZE(x)

#define MAGIC_NUM_OF_TEST_RUN_ID (1000)
#define MAX_NUM_OF_TEST_RUN (32)
#define MAX_NUM_OF_TEST_RUN_ID (MAGIC_NUM_OF_TEST_RUN_ID + MAX_NUM_OF_TEST_RUN -1)
#define MIN_NUM_OF_TEST_RUN_ID (MAGIC_NUM_OF_TEST_RUN_ID)
#define TEST_IF_VALID_TEST_RUN_ID(x) (MIN_NUM_OF_TEST_RUN_ID <= x && x <= MAX_NUM_OF_TEST_RUN_ID)
#define ENCODE_TEST_RUN_ID(x) (x+MAGIC_NUM_OF_TEST_RUN_ID)
#define DECODE_TEST_RUN_ID(x) (x-MAGIC_NUM_OF_TEST_RUN_ID)
#define DEFAULT_MAX_INTERVAL (100)//0.1S
#define DEFAULT_MAX_TEST_TIMES INFINITE_I2C_TEST_TIMES//INFINITE....
#define INITINAL_TEST_RUN_ID (MIN_NUM_OF_TEST_RUN_ID-1)
#define IF_INFINITE_RUN(x) (0 == x)



#define MAGIC_NUM_OF_TEST_SET_ID (2000)
#define MAX_NUM_OF_TEST_SET (32)
#define MAX_NUM_OF_TEST_SET_ID (MAGIC_NUM_OF_TEST_SET_ID + MAX_NUM_OF_TEST_SET -1)
#define MIN_NUM_OF_TEST_SET_ID (MAGIC_NUM_OF_TEST_SET_ID)
#define TEST_IF_VALID_TEST_SET_ID(x) (MIN_NUM_OF_TEST_SET_ID <= x && x <= MAX_NUM_OF_TEST_SET_ID)
#define ENCODE_TEST_SET_ID(x) (x+MAGIC_NUM_OF_TEST_SET_ID)
#define DECODE_TEST_SET_ID(x) (x-MAGIC_NUM_OF_TEST_SET_ID)
#define INITINAL_TEST_SET_ID (MIN_NUM_OF_TEST_SET_ID-1)

static int I2CST_LOG(AXE_I2C_LOG_LEVEL aeLevel, const char *fmt, ...);

static int i2c_test_case_Init(struct i2c_test_case *this, 
			struct i2c_client   *apClient,
			struct i2c_test_case_info *apTestCaseInfo)
{
	assert(NULL != this);

	if(NULL != this){

		this->mpClient = apClient;

		this->mTestCaseInfo = *apTestCaseInfo;

		return 0;
	}

	return (-1);
}
static int i2c_test_case_Deinit(struct i2c_test_case *this)
{
	return 0;
}

static int i2c_test_case_Run(struct i2c_test_case *this)
{
	assert(NULL != this);

	if(NULL != this){

		assert(NULL != this->mTestCaseInfo.run);

		if(NULL != this->mTestCaseInfo.run){

			int lnResult ;

			assert(NULL != this->mpClient);

			I2CST_LOG(I2C_LOG_LEVEL_TEST_CASE, "testcase-%s ++\n", this->GetTestName(this));

			lnResult = this->mTestCaseInfo.run(this->mpClient);

			if(I2C_TEST_PASS != lnResult){

				I2CST_LOG(I2C_LOG_LEVEL_TEST_CASE, "testcase-%s -- Fail,error code=%d\n", this->GetTestName(this),lnResult);

				this->mnTestFailTimes ++;

				return (-1);
				
			}else{
				I2CST_LOG(I2C_LOG_LEVEL_TEST_CASE, "testcase-%s -- pass\n", this->GetTestName(this));

				this->mnTestPassedTimes ++;

				return (0);
			}
		}
		
		I2CST_LOG(I2C_LOG_LEVEL_ERROR, "testcase-%s doesn't have run function\n");
		
		return -1;
	}

	return (-1);	
}

static const char * i2c_test_case_GetTestName(struct i2c_test_case *this)
{
	assert(NULL != this);

	if(NULL != this){

		return this->mTestCaseInfo.name;
	}

	return NULL;

}
static unsigned i2c_test_case_GetPassTimes(struct i2c_test_case *this)
{
	assert(NULL != this);

	if(NULL != this){

		return this->mnTestPassedTimes;
	}

	return (-1);

}
static unsigned i2c_test_case_GetFailTimes(struct i2c_test_case *this)
{
	assert(NULL != this);

	if(NULL != this){

		return this->mnTestFailTimes;
	}

	return (-1);

}
static unsigned i2c_test_case_GetTotalTimes(struct i2c_test_case *this)
{
	assert(NULL != this);

	if(NULL != this){

		return (this->mnTestFailTimes + this->mnTestPassedTimes);
	}

	return (-1);

}
static int i2c_test_case_ResetTimes(struct i2c_test_case *this)
{
	assert(NULL != this);

	if(NULL != this){

		this->mnTestFailTimes = 0;

		this->mnTestPassedTimes = 0;

		return (0);
	}

	return (-1);

}
static struct i2c_client   *i2c_test_case_GetI2CClient(struct i2c_test_case *this)
{
	assert(NULL != this);

	if(NULL != this){

		if(NULL != this->mpClient){

			return this->mpClient;
		}
	}
	return (NULL);
}




static int i2c_test_case_Summary(struct i2c_test_summary *apSummaryObject)
{
	struct i2c_test_case *this = container_of(apSummaryObject,
											struct i2c_test_case,
											mSummary);	

	assert(NULL != this);

	if(NULL != this){	

		I2CST_LOG(I2C_LOG_LEVEL_JUST_SUMMARY,
			"Case:%s,pass %d, fail %d\n",
			this->mTestCaseInfo.name,
			this->GetPassTimes(this),
			this->GetFailTimes(this));

		return 0;
	}
	
	return (-1);

}

static int i2c_test_case_Bind(struct i2c_test_case *apTestCase)
{
	assert(NULL != apTestCase);

	if(NULL != apTestCase){

		apTestCase->mpClient = NULL;

		INIT_LIST_HEAD(&apTestCase->mTestCaseList);

		memset(&apTestCase->mTestCaseInfo, 0, sizeof(struct i2c_test_case_info));
		
		//apTestCase->mpTestCaseInfo = NULL;
		
		apTestCase->mnTestPassedTimes = 0;
		
		apTestCase->mnTestFailTimes = 0;

		
		apTestCase->Init = i2c_test_case_Init;
		
		apTestCase->Deinit = i2c_test_case_Deinit;
		
		apTestCase->Run = i2c_test_case_Run;

		apTestCase->GetTestName = i2c_test_case_GetTestName;
		
		apTestCase->GetPassTimes = i2c_test_case_GetPassTimes;
		
		apTestCase->GetFailTimes = i2c_test_case_GetFailTimes;

		apTestCase->GetTotalTimes = i2c_test_case_GetTotalTimes;
		
		apTestCase->ResetTimes = i2c_test_case_ResetTimes;

		apTestCase->GetI2CClient = i2c_test_case_GetI2CClient;

		apTestCase->mSummary.Summary = i2c_test_case_Summary;

		return 0;
	}

	return (-1);
}
static int i2c_test_case_Create(struct i2c_test_case **appTestCase)
{
	
	struct i2c_test_case *lpTestCase = NULL;
	
	lpTestCase = kzalloc(sizeof(*lpTestCase), GFP_KERNEL);

	assert(NULL != lpTestCase);
	
	if (NULL != lpTestCase) {

		*appTestCase = lpTestCase;

		i2c_test_case_Bind(*appTestCase);
		
		return 0;
	}

	I2CST_LOG(I2C_LOG_LEVEL_ERROR,"Can't create test case\n");

	return (-1);
}
static int i2c_test_case_Delete(struct i2c_test_case *apTestCase)
{
	if(NULL != apTestCase){

		kfree(apTestCase);

		return 0;
	}

	I2CST_LOG(I2C_LOG_LEVEL_ERROR,"Can't delete test case(null)\n");

	return (-1);
}


static unsigned i2c_test_interval_generator_GetInterval(struct i2c_test_interval_generator *this)
{

	if(false == this->mbFixedIntervalMode){

		if(0 == this->mnMaxInterval){

			return this->mnMaxInterval;
		}

		//printk("I:%d",lnInterval,get_random_int());
		
		return (unsigned)(get_random_int()%this->mnMaxInterval);

	}else{//fixed mode

		return this->mnMaxInterval;

	}

}
static void i2c_test_interval_generator_SetMaxInterval(struct i2c_test_interval_generator *this,
unsigned anInterval)
{

	this->mnMaxInterval = anInterval;
}
static unsigned i2c_test_interval_generator_GetMaxInterval(struct i2c_test_interval_generator *this)
{
	return this->mnMaxInterval;
}
static void i2c_test_interval_generator_SetMode(struct i2c_test_interval_generator *this,
bool abFixedIntervalMode)
{

	this->mbFixedIntervalMode = abFixedIntervalMode;
}
static bool i2c_test_interval_generator_IsRandom(struct i2c_test_interval_generator *this)
{
	return false == this->mbFixedIntervalMode;
}
static int i2c_test_interval_generator_Bind(struct i2c_test_interval_generator *apIntervalGenerator)
{
	assert(NULL != apIntervalGenerator);

	if(NULL != apIntervalGenerator){

		apIntervalGenerator->GetInterval = i2c_test_interval_generator_GetInterval;
		apIntervalGenerator->SetMaxInterval = i2c_test_interval_generator_SetMaxInterval;
		apIntervalGenerator->GetMaxInterval = i2c_test_interval_generator_GetMaxInterval;
		apIntervalGenerator->SetMode = i2c_test_interval_generator_SetMode;
		apIntervalGenerator->IsRandom = i2c_test_interval_generator_IsRandom;
		apIntervalGenerator->mbFixedIntervalMode = false;
		apIntervalGenerator->mnMaxInterval = INTERVAL_BETWEEN_RUNNING_EVERY_TEST_CASE;

		return 0;
	}

	return (-1);
}
/*
static int i2c_test_interval_generator_Create(struct i2c_test_interval_generator **appIntervalGenerator)
{
	
	struct i2c_test_interval_generator *lpIntervalGenerator = NULL;
	
	lpIntervalGenerator = kzalloc(sizeof(*lpIntervalGenerator), GFP_KERNEL);

	assert(NULL != lpIntervalGenerator);
	
	if (NULL != lpIntervalGenerator) {

		*appIntervalGenerator = lpIntervalGenerator;

		i2c_test_interval_generator_Bind(*appIntervalGenerator);
		
		return 0;
	}

	I2CST_LOG(I2C_LOG_LEVEL_ERROR,"Can't create test interval generator\n");

	return (-1);
}

static int i2c_test_interval_generator_Delete(struct i2c_test_interval_generator *apIntervalGenerator)
{
	if(NULL != apIntervalGenerator){

		kfree(apIntervalGenerator);

		return 0;
	}

	I2CST_LOG(I2C_LOG_LEVEL_ERROR,"Can't delete test interval generator(null)\n");

	return (-1);
}

*/

static int i2c_test_interval_generator_SelfTestCreate(struct i2c_test_interval_generator *apIntervalGenerator)
{
	assert( i2c_test_interval_generator_GetInterval == apIntervalGenerator->GetInterval);
	assert( i2c_test_interval_generator_SetMaxInterval == apIntervalGenerator->SetMaxInterval);
	assert( i2c_test_interval_generator_GetMaxInterval == apIntervalGenerator->GetMaxInterval);
	assert( i2c_test_interval_generator_SetMode == apIntervalGenerator->SetMode );
	assert( i2c_test_interval_generator_IsRandom == apIntervalGenerator->IsRandom);
	assert( false == apIntervalGenerator->mbFixedIntervalMode);
	assert( INTERVAL_BETWEEN_RUNNING_EVERY_TEST_CASE == apIntervalGenerator->mnMaxInterval);

	return 0;
}
static int i2c_test_set_Init(struct i2c_test_set *this, 
	unsigned anID,
	const char *apName)
{
	assert(NULL != this);

	if(NULL != this){

		this->mnID = anID;

		this->mpName = apName;

		return 0;
	}

	return (-1);	
}
static int i2c_test_set_RemoveAndDestroyAllTestCase(struct i2c_test_set *this)
{
	struct i2c_test_case *lpTestCase, *_n;

	list_for_each_entry_safe(lpTestCase, 
							_n, 
							&this->mTestCaseListHead, 
							mTestCaseList) {

		list_del(&lpTestCase->mTestCaseList);

		i2c_test_case_Delete(lpTestCase);

	}	

	return 0;
}
static int i2c_test_set_Deinit(struct i2c_test_set *this)
{
	assert(NULL != this);

	if(NULL != this){

		return i2c_test_set_RemoveAndDestroyAllTestCase(this);
	}

	return (-1);	
}
static int i2c_test_set_Run(struct i2c_test_set *this)
{
	assert(NULL != this);

	if(NULL != this){

		struct i2c_test_case *lpTestCase, *_n;

		assert(false == this->mbIsRunning);

		if(!list_empty(&this->mTestCaseListHead)){

			this->mbIsRunning = true;
		
			list_for_each_entry_safe(lpTestCase, 
									_n, 
									&this->mTestCaseListHead, 
									mTestCaseList) {

				if(false == this->mbIsRunning){

					I2CST_LOG(I2C_LOG_LEVEL_TEST_CASE,"Manual stop test\n");

					break;

				}
				
				lpTestCase->Run(lpTestCase);
{
				unsigned lnInterval;
				
				if(NULL != this->mpIntervalGenerator){
				
					lnInterval = this->mpIntervalGenerator->GetInterval(this->mpIntervalGenerator);
				
				}else{

					lnInterval = this->mnIntervalBetweenRunningEveryTestCase;
				}

				//printk("i2c_test_set_Run%d\n",lnInterval);


				if(0!= lnInterval){
				
					msleep_interruptible(lnInterval);
				
				}
}
			}

			//printk("i2c_test_set_Run_2\n");

			this->mbIsRunning = false;

			return 0;
		}

	}

	return (-1);	
}
static int i2c_test_set_Stop(struct i2c_test_set *this)
{
	assert(NULL != this);

	if(NULL != this){

		assert(true == this->mbIsRunning);

		this->mbIsRunning = false;

		return 0;
	}

	return (-1);	
}
static int i2c_test_set_AddTestCase(struct i2c_test_set *this,
			struct i2c_test_case *apTestCase)
{
	assert(NULL != this);

	if(NULL != this){
		
		list_add_tail(&apTestCase->mTestCaseList,&this->mTestCaseListHead);

		
		return 0;
	}

	return (-1);	
}
static int i2c_test_set_SetIntervalBetweenRunningEveryTestCase(struct i2c_test_set *this,
	unsigned anIntervalBetweenRunningEveryTestCase)
{
	assert(NULL != this);

	if(NULL != this){
		
		this->mnIntervalBetweenRunningEveryTestCase = anIntervalBetweenRunningEveryTestCase;

		return 0;
	}

	return (-1);

}

struct i2c_client *i2c_test_set_GetI2CClient(struct i2c_test_set *this)
{
	assert(NULL != this);

	if(NULL != this){

		if(!list_empty(&this->mTestCaseListHead)){

			struct i2c_test_case *lpTestCase = container_of(this->mTestCaseListHead.next,
													struct i2c_test_case,
													mTestCaseList);	
			

			return lpTestCase->GetI2CClient(lpTestCase);
			
		}
	}
	return NULL;	
}
static unsigned i2c_test_set_GetID(struct i2c_test_set *this)
{
	assert(NULL != this);

	if(NULL != this){

		return this->mnID;
			
	}
	return 0;		
}
static int i2c_test_set_SetOwner(struct i2c_test_set *this,struct i2c_test_run *apTestRun)
{
	assert(NULL != this);

	if(NULL != this){

		assert(NULL == this->mpTestRun);

		this->mpTestRun = apTestRun;

		return 0;
			
	}
	return -1;

}
static int i2c_test_set_RemoveOwner(struct i2c_test_set *this)
{
	assert(NULL != this);

	if(NULL != this){

		this->mpTestRun = NULL;

		return 0;
			
	}
	return -1;

}
static int i2c_test_set_SetIntervalGenerator(struct i2c_test_set *this,
	struct i2c_test_interval_generator *lpIntervalGenerator)
{
	assert(NULL != this);

	if(NULL != this){

		this->mpIntervalGenerator = lpIntervalGenerator;

		return 0;
			
	}
	return -1;

}
static int i2c_test_set_Reset(struct i2c_test_set *this)
{
	struct i2c_test_case *lpTestCase, *_n;

	assert(NULL != this);

	if(NULL != this){

		list_for_each_entry_safe(lpTestCase, 
							_n, 
							&this->mTestCaseListHead, 
							mTestCaseList) {

			lpTestCase->ResetTimes(lpTestCase);

		}	

		return 0;
			
	}
	return -1;	
}

static int i2c_test_set_Summary(struct i2c_test_summary *apSummaryObject)
{
	struct i2c_test_set *this = container_of(apSummaryObject,
											struct i2c_test_set,
											mSummary);	

	struct i2c_test_case *lpTestCase, *_n;

	assert(NULL != this);

	if(NULL != this){	

		struct i2c_client * lpClient = this->GetI2CClient(this);

		assert(NULL != lpClient);
		
		I2CST_LOG(I2C_LOG_LEVEL_JUST_SUMMARY,
			"Set:%s,Addr:0x%x(%s)\n",
			this->mpName,
			lpClient->addr,
			lpClient->name);

		list_for_each_entry_safe(lpTestCase, 
								_n, 
								&this->mTestCaseListHead, 
								mTestCaseList) {
		
			lpTestCase->mSummary.Summary(&lpTestCase->mSummary);

		}

		return 0;
	}
	
	return (-1);

}

static int i2c_test_set_Bind(struct i2c_test_set *apTestSet)
{
	assert(NULL != apTestSet);

	if(NULL != apTestSet){

		apTestSet->mpName = NULL;

		apTestSet->mnID = INITINAL_TEST_SET_ID;

		apTestSet->mpTestRun = NULL;

		INIT_LIST_HEAD(&apTestSet->mTestSetListInTheSameRun);

		INIT_LIST_HEAD(&apTestSet->mAllTestSetList);

		INIT_LIST_HEAD(&apTestSet->mTestCaseListHead);
		
		apTestSet->Init = i2c_test_set_Init;
		apTestSet->Deinit = i2c_test_set_Deinit;
		apTestSet->Run = i2c_test_set_Run;
		apTestSet->Stop = i2c_test_set_Stop;
		apTestSet->AddTestCase = i2c_test_set_AddTestCase;
		apTestSet->SetIntervalBetweenRunningEveryTestCase = i2c_test_set_SetIntervalBetweenRunningEveryTestCase;
		apTestSet->GetI2CClient = i2c_test_set_GetI2CClient;
		apTestSet->GetID = i2c_test_set_GetID;

		apTestSet->SetOwner = i2c_test_set_SetOwner;

		apTestSet->RemoveOwner = i2c_test_set_RemoveOwner;

		apTestSet->SetIntervalGenerator = i2c_test_set_SetIntervalGenerator;

		apTestSet->Reset =  i2c_test_set_Reset;

		apTestSet->mnIntervalBetweenRunningEveryTestCase = INTERVAL_BETWEEN_RUNNING_EVERY_TEST_CASE;		

		apTestSet->mbIsRunning = false;

		apTestSet->mpIntervalGenerator = NULL;

		apTestSet->mSummary.Summary = i2c_test_set_Summary;
		
		return 0;
	}

	return (-1);
}
static int i2c_test_set_Create(struct i2c_test_set **appTestSet)
{
	
	struct i2c_test_set *lpTestSet = NULL;
	
	lpTestSet = kzalloc(sizeof(*lpTestSet), GFP_KERNEL);
	
	if (NULL != lpTestSet) {

		*appTestSet = lpTestSet;

		i2c_test_set_Bind(*appTestSet);
		
		return 0;
	}

	I2CST_LOG(I2C_LOG_LEVEL_ERROR,"Can't create test set\n");

	return (-1);
}
static int i2c_test_set_Delete(struct i2c_test_set *apTestSet)
{
	if(NULL != apTestSet){

		kfree(apTestSet);

		return 0;
	}

	I2CST_LOG(I2C_LOG_LEVEL_ERROR,"Can't delete test set(null)\n");

	return (-1);
}

static int i2c_test_run_Summary(struct i2c_test_summary *apSummaryObject)
{
	struct i2c_test_run *this = container_of(apSummaryObject,
											struct i2c_test_run,
											mSummary);	

	struct i2c_test_set *lpTestSet, *_n;

	assert(NULL != this);

	if(NULL != this){	

		
		I2CST_LOG(I2C_LOG_LEVEL_JUST_SUMMARY,
			"Summary for test run %d\n",
			this->mnID);

		list_for_each_entry_safe(lpTestSet, 
								_n, 
								&this->mTestSetListHead, 
								mTestSetListInTheSameRun) {

		
			lpTestSet->mSummary.Summary(&lpTestSet->mSummary);

		}

		return 0;
	}
	
	return (-1);

}
static void i2c_test_run_RunForTest(struct i2c_test_run *this)
{
	//struct i2c_test_set *lpTestSet, *_n;

	//static int lnTestTimes = 1;

	this->mnCurrentTimes = 1;

loop:
	//printk("1\n");

	if(true == this->mbIsRunning &&	
#ifdef TEST_RUN_IMPLEMENTED_BY_KTHREAD
		!kthread_should_stop() &&
#endif		
		(INFINITE_I2C_TEST_TIMES == this->mnMaxTestTimes ||
		this->mnCurrentTimes <= this->mnMaxTestTimes)){


		I2CST_LOG(I2C_LOG_LEVEL_JUST_SUMMARY,
			"TestRun[%d-%d]\n",
			this->mnID,
			this->mnCurrentTimes);


		this->mpCurrentTestSet->Run(this->mpCurrentTestSet);

		this->mnCurrentTimes ++;
		
		if(this->mpCurrentTestSet->mTestSetListInTheSameRun.next ==
			&this->mTestSetListHead){

			//printk("3_1\n");
		
			this->mpCurrentTestSet = list_first_entry(&this->mTestSetListHead,
				struct i2c_test_set ,
				mTestSetListInTheSameRun);

		
		}else{

			//printk("3_2\n");

			this->mpCurrentTestSet = list_first_entry(&this->mpCurrentTestSet->mTestSetListInTheSameRun,
				struct i2c_test_set ,
				mTestSetListInTheSameRun);

		}
		
		//assert(0 <= this->mnMaxIntervalBetweenRunningEveryTestSet);
		
		//if(0 < this->mnMaxIntervalBetweenRunningEveryTestSet ){

		{
			unsigned lnInterval; 
		
			//printk("schedule_work\n");

			//lnInterval = this->mnMaxIntervalBetweenRunningEveryTestSet;

			lnInterval = this->mIntervalGenerator.GetInterval(&this->mIntervalGenerator);
		
			if(true == this->mbIsTestMode){
		
				//printk("4\n");
				//printk("i2c_test_run_RunForTest%d\n",lnInterval);
		
				if(0 != lnInterval){

					msleep_interruptible(lnInterval);
				}
		
				goto loop;
		
			}else{

				//printk("i2c_test_run_RunForTest%d\n",lnInterval);
				if(0 != lnInterval){
#ifdef TEST_RUN_IMPLEMENTED_BY_KTHREAD
					msleep_interruptible(lnInterval);

					goto loop;
#else
					schedule_delayed_work(&this->mStressTestWorker,round_jiffies_relative(lnInterval*HZ/1000)); 

#endif		
				}else{

					goto loop;
				}		
				
			}
		}
		
	}else{ // 1.infinite and not manual stop 2.test times is not up.

		//Manual stop...
		if(false == this->mbIsRunning){	

			I2CST_LOG(I2C_LOG_LEVEL_JUST_SUMMARY,
				"%d,Test manual stop\n",this->mnID);

		}else{ // stop for test time is up.
			I2CST_LOG(I2C_LOG_LEVEL_JUST_SUMMARY,
				"%d,Test finish\n",this->mnID);
		}

		this->mSummary.Summary(&this->mSummary);

		this->mnCurrentTimes = 1;

		this->mpCurrentTestSet = NULL;

		this->mbIsRunning = false;


		
	}	

}
#ifdef TEST_RUN_IMPLEMENTED_BY_KTHREAD
static int i2c_test_run_thread(void *p)
{
	struct i2c_test_run *this = p;
	
	i2c_test_run_RunForTest(this);


	return 0;
}


#else
static void i2c_test_run_RunForWorker(struct work_struct *dat)
{
	struct i2c_test_run *this = container_of(dat,
											struct i2c_test_run,
											mStressTestWorker.work);

	i2c_test_run_RunForTest(this);

}
#endif




static int i2c_test_run_Init(struct i2c_test_run *this, unsigned anID, struct i2c_test_module *apTestModule)
{
	assert(NULL != this);

	if(NULL != this){

		this->mnID = anID;

		this->mpTestModule = apTestModule;

		return (0);
		
	}
	return (-1);	
}
static int i2c_test_run_Deinit(struct i2c_test_run *this)
{
	assert(NULL != this);

	if(NULL != this){

		
		if(!list_empty(&this->mTestSetListHead)){

			struct i2c_test_set *lpTestSet, *_n;
			
			list_for_each_entry_safe(lpTestSet, 
									_n, 
									&this->mTestSetListHead, 
									mTestSetListInTheSameRun) {

				lpTestSet->RemoveOwner(lpTestSet);

				list_del(&lpTestSet->mTestSetListInTheSameRun);

			}

			

		}
		return (0);
		
	}
	return (-1);
}
static int i2c_test_run_Run(struct i2c_test_run *this)
{
	struct i2c_test_set *lpTestSet, *_n;

	assert(NULL != this);

	if(NULL != this){

		

		//printk("1\n");
		assert(false == this->mbIsRunning);

		if(false == this->mbIsRunning){
		
			//printk("2\n");

			if(false == list_empty(&this->mTestSetListHead)){

				//printk("3\n");

				this->mbIsRunning = true;

				list_for_each_entry_safe(lpTestSet, 
						_n, 
						&this->mTestSetListHead, 
						mTestSetListInTheSameRun) {

					lpTestSet->Reset(lpTestSet);

				}	

				I2CST_LOG(I2C_LOG_LEVEL_JUST_SUMMARY,
						"%d,Start running\n",this->mnID);

				this->mpCurrentTestSet = list_first_entry(&this->mTestSetListHead,
					struct i2c_test_set ,
					mTestSetListInTheSameRun);

				if(true == this->mbIsTestMode){

					//printk("4\n");

					i2c_test_run_RunForTest(this);
					
				}else{

					//printk("schedule_work\n");
#ifdef TEST_RUN_IMPLEMENTED_BY_KTHREAD
					this->mpkthread = kthread_run(i2c_test_run_thread, this, "i2c-test-run:%d",this->mnID);
					if (IS_ERR(this->mpkthread)){

						I2CST_LOG(I2C_LOG_LEVEL_ERROR,
							"%d,kthread_run, error\n",this->mnID);

					}
#else
						schedule_delayed_work(&this->mStressTestWorker,round_jiffies_relative(5*HZ/10)); //0.5 sec to run
#endif

				}


				

				return (0);
			}
		}
	}
	return (-1);

}
static int i2c_test_run_Stop(struct i2c_test_run *this)
{
	assert(NULL != this);

	if(NULL != this){


		assert(true == this->mbIsRunning);

		if(true == this->mbIsRunning){
#ifdef TEST_RUN_IMPLEMENTED_BY_KTHREAD

			printk("i2c_test_run_Stop\n");

			this->mbIsRunning = false;

			this->mpCurrentTestSet->Stop(this->mpCurrentTestSet);

			kthread_stop(this->mpkthread);
#endif		
		}


		
			

		
		return (0);
		
	}
	return (-1);

}
static int i2c_test_run_AddTestSet(struct i2c_test_run *this,
			struct i2c_test_set *apTestSet)
{

	int lnResult = -1;
	
	assert(NULL != this);

	assert(NULL != apTestSet);

	if(NULL != this && NULL != apTestSet){

		lnResult = apTestSet->SetOwner(apTestSet, this);

		assert(0 == lnResult);

		if(0 == lnResult){
			
			list_add_tail(&apTestSet->mTestSetListInTheSameRun,&this->mTestSetListHead);
		}

		
	}
	return (lnResult);


}
static unsigned i2c_test_run_GetID(struct i2c_test_run *this)
{
	assert(NULL != this);

	if(NULL != this){

		return this->mnID;
		
	}
	return 0;
}
static int i2c_test_run_SetMaxIntervalBetweenRunningEveryTestSet(struct i2c_test_run *this, unsigned anMaxIntervalBetweenRunningEveryTestSet)
{
	assert(NULL != this);

	if(NULL != this){

		//this->mnMaxIntervalBetweenRunningEveryTestSet = anMaxIntervalBetweenRunningEveryTestSet;

		this->mIntervalGenerator.SetMaxInterval(&this->mIntervalGenerator,
			anMaxIntervalBetweenRunningEveryTestSet);
		return (0);
		
	}
	return (-1);

	
}
static int i2c_test_run_SetFixedIntervalMode(struct i2c_test_run *this, bool abFixedIntervalMode)
{
	assert(NULL != this);

	if(NULL != this){

		//this->mnMaxIntervalBetweenRunningEveryTestSet = anMaxIntervalBetweenRunningEveryTestSet;

		this->mIntervalGenerator.SetMode(&this->mIntervalGenerator,
			abFixedIntervalMode);
		return (0);
		
	}
	return (-1);

	
}

static int i2c_test_run_SetMaxTestTimes(struct i2c_test_run *this, int anMaxTestTimes)
{
	assert(NULL != this);

	if(NULL != this){

		this->mnMaxTestTimes = anMaxTestTimes;
			
		return (0);
		
	}
	return (-1);

	
}
static bool i2c_test_run_IsRunning(struct i2c_test_run *this)
{
	assert(NULL != this);

	if(NULL != this){

		return (this->mbIsRunning);
			
	}
	return (false);	
}
static int i2c_test_run_ShowStatus(struct i2c_test_run *this, 
	const char *buf,
	bool abShowTitle)
{
	//strcpy(buf,"i2c_test_run_ShowStatus\n");

	assert(NULL != this);

	if(NULL != this){

		char lpStr[256];

		struct i2c_test_set *lpTestSet, *_n;

		if(true == abShowTitle){

			strcpy((char *)buf,"[Test Run]\nID\tStatus\t\tTimes\t\tInterval\tTestSet\n");

		}

		if(INFINITE_I2C_TEST_TIMES == this->mnMaxTestTimes){

			if(this->mIntervalGenerator.IsRandom(&this->mIntervalGenerator)){

				sprintf(lpStr, "%d\t%s\t\t%s\t%d(random)\t", 
				this->mnID,
				(this->IsRunning(this))?"running":"stopped",
				"infinite",
				this->mIntervalGenerator.GetMaxInterval(&this->mIntervalGenerator));		

			}else{

				sprintf(lpStr, "%d\t%s\t\t%s\t%d(fixed)\t", 
				this->mnID,
				(this->IsRunning(this))?"running":"stopped",
				"infinite",
				this->mIntervalGenerator.GetMaxInterval(&this->mIntervalGenerator));			
			}
			
		}else{

			if(this->mIntervalGenerator.IsRandom(&this->mIntervalGenerator)){
				sprintf(lpStr, "%d\t%s\t\t%d\t\t%d(random)\t", 
					this->mnID,
					(this->IsRunning(this))?"running":"stopped",
					this->mnMaxTestTimes,
					this->mIntervalGenerator.GetMaxInterval(&this->mIntervalGenerator));
			}else{
				sprintf(lpStr, "%d\t%s\t\t%d\t\t%d(fixed)\t", 
					this->mnID,
					(this->IsRunning(this))?"running":"stopped",
					this->mnMaxTestTimes,
					this->mIntervalGenerator.GetMaxInterval(&this->mIntervalGenerator));


			}

		}


		strcat((char *)buf, lpStr);

		list_for_each_entry_safe(lpTestSet, 
								_n, 
								&this->mTestSetListHead, 
								mTestSetListInTheSameRun) {

			sprintf(lpStr, "%d(%s),", 
				lpTestSet->GetID(lpTestSet),
				lpTestSet->mpName);

			strcat((char *)buf, lpStr);

		}	

		strcat((char *)buf, "\n");
			
		return (0);
		
	}
	return (-1);		
}

static int i2c_test_run_Bind(struct i2c_test_run *apTestRun)
{
	assert(NULL != apTestRun);

	if(NULL != apTestRun){

		INIT_LIST_HEAD(&apTestRun->mTestRunList);
		
		apTestRun->mnID = INITINAL_TEST_RUN_ID;
#ifdef TEST_RUN_IMPLEMENTED_BY_KTHREAD
		apTestRun->mpkthread = NULL;
#else
		INIT_DELAYED_WORK(&apTestRun->mStressTestWorker, i2c_test_run_RunForWorker) ;
#endif


		INIT_LIST_HEAD(&apTestRun->mTestSetListHead);

		apTestRun->mpCurrentTestSet = NULL;
		
		apTestRun->mpTestModule = NULL;
#ifdef TEST_RUN_IMPLEMENTED_BY_KTHREAD
		apTestRun->mpkthread = NULL;

#endif

		apTestRun->mbIsRunning = false;
			
		//apTestRun->mnMaxIntervalBetweenRunningEveryTestSet = DEFAULT_MAX_INTERVAL;
		
		apTestRun->mnMaxTestTimes = DEFAULT_MAX_TEST_TIMES;
		
		apTestRun->Init = i2c_test_run_Init;

		apTestRun->Deinit = i2c_test_run_Deinit;

		apTestRun->Run = i2c_test_run_Run;

		apTestRun->Stop = i2c_test_run_Stop;

		apTestRun->AddTestSet = i2c_test_run_AddTestSet;

		apTestRun->GetID = i2c_test_run_GetID;

		apTestRun->SetMaxIntervalBetweenRunningEveryTestSet = i2c_test_run_SetMaxIntervalBetweenRunningEveryTestSet;

		apTestRun->SetFixedIntervalMode = i2c_test_run_SetFixedIntervalMode;

		apTestRun->SetMaxTestTimes = i2c_test_run_SetMaxTestTimes;

		apTestRun->IsRunning = i2c_test_run_IsRunning;

		apTestRun->ShowStatus = i2c_test_run_ShowStatus;

		apTestRun->mSummary.Summary = i2c_test_run_Summary;

		apTestRun->mbIsTestMode = false;

		i2c_test_interval_generator_Bind(&apTestRun->mIntervalGenerator);

		return 0;
	}

	return (-1);
}

static int i2c_test_run_Create(struct i2c_test_run **appTestRun)
{
	
	struct i2c_test_run *lpTestRun = NULL;
	
	lpTestRun = kzalloc(sizeof(*lpTestRun), GFP_KERNEL);
	
	if (NULL != lpTestRun) {

		*appTestRun = lpTestRun;

		i2c_test_run_Bind(*appTestRun);
		
		return 0;
	}

	I2CST_LOG(I2C_LOG_LEVEL_ERROR,"Can't create test run\n");

	return (-1);
}
static int i2c_test_run_Delete(struct i2c_test_run *apTestRun)
{
	if(NULL != apTestRun){

		kfree(apTestRun);

		return 0;
	}

	I2CST_LOG(I2C_LOG_LEVEL_ERROR,"Can't delete test run(null)\n");

	return (-1);
}

static int i2c_test_module_Init(struct i2c_test_module *this)
{
	assert(false == this->mbInited);

	if(false == this->mbInited){

		INIT_LIST_HEAD(&this->mTestRunListHead);

		INIT_LIST_HEAD(&this->mAllTestSetListHead);

		this->mbInited = true;

		return 0;
	}

	return (-1);
}
static int i2c_test_module_Deinit(struct i2c_test_module *this)
{
	assert(true == this->mbInited);

	if(true == this->mbInited){

		
		

		this->mbInited = false;

		return 0;
	}

	return (-1);

}
static int i2c_test_module_ShowStatus(struct i2c_test_module *this,const char *buf)
{
	char lpStr[256];	

	assert(NULL != this);

	if(NULL != this){

		struct i2c_test_set *lpTestSet, *_n;

		struct i2c_test_run *lpTestRun, *_n1;
		
		memset(lpStr, 0 , sizeof(lpStr));

		strcpy((char *)buf,"[Test Set]\nID\tAddr\tName\n");

		list_for_each_entry_safe(lpTestSet, 
								_n, 
								&this->mAllTestSetListHead, 
								mAllTestSetList) {

			struct i2c_client * lpI2CClient = NULL;

			lpI2CClient = lpTestSet->GetI2CClient(lpTestSet);

			assert(NULL != lpI2CClient);

			sprintf(lpStr, "%d\t0x%x\t%s\n",
				lpTestSet->GetID(lpTestSet), lpI2CClient->addr, lpTestSet->mpName);

			strcat((char *)buf, lpStr);

		}	

		list_for_each_entry_safe(lpTestRun, 
								_n1, 
								&this->mTestRunListHead, 
								mTestRunList) {
								
			memset(lpStr, 0 , sizeof(lpStr));

			if( list_first_entry(&this->mTestRunListHead,
					struct i2c_test_run ,
					mTestRunList) == lpTestRun){

				
				lpTestRun->ShowStatus(lpTestRun,lpStr,true);

			}else{

				lpTestRun->ShowStatus(lpTestRun,lpStr,false);
			}

			strcat((char *)buf, lpStr);
		}	

		return strlen(buf);
	}
	
	return (0);

}
static int i2c_test_module_CreateTestRun(struct i2c_test_module *this, unsigned *apTestRunID)
{
	struct i2c_test_run *lpTestRun = NULL;

	int lnIndex = MAX_NUM_OF_TEST_RUN;

	int lnResult = 0;

	lnIndex = find_first_zero_bit(&this->mTestRunIDBitArray, MAX_NUM_OF_TEST_RUN);


	/* client list is full */
	if (MAX_NUM_OF_TEST_RUN > lnIndex ){

		unsigned lnID;

		lnID = ENCODE_TEST_RUN_ID(lnIndex);

		lnResult = i2c_test_run_Create(&lpTestRun);

		assert(0 == lnResult);
		
		assert(NULL != lpTestRun);

		if (NULL != lpTestRun) {


			lnResult = lpTestRun->Init(lpTestRun,
						lnID,
						this);

			assert(0 == lnResult);

			if(0 == lnResult){

				set_bit(lnIndex, &this->mTestRunIDBitArray);

				*apTestRunID = lnID;

				list_add_tail(&lpTestRun->mTestRunList, &this->mTestRunListHead);

				return 0;
			}

		}

		if(NULL != lpTestRun){

			kfree(lpTestRun);

			lpTestRun = NULL;

		}

	}


	return -1;
	
}
static int i2c_test_module_CreateN2NTestRun(struct i2c_test_module *this)
{
	struct i2c_test_set *lpTestSet, *_n;

	list_for_each_entry_safe(lpTestSet, 
							_n, 
							&this->mAllTestSetListHead, 
							mAllTestSetList) {
							
		if(NULL == lpTestSet->mpTestRun){

			unsigned lnTestRunID;

			this->CreateTestRun(this,&lnTestRunID);

			this->AttachTestSetToTestRun(this,
				lnTestRunID,
				lpTestSet->GetID(lpTestSet));

			printk("Create Test Run(%d)and Attach TestSet(%d) To it",
				lnTestRunID,
				lpTestSet->GetID(lpTestSet));
		}
	}		

	return 0;	
}

static struct i2c_test_run *i2c_test_module_FindTestRunByID(struct i2c_test_module *this, unsigned anID)
{
	struct i2c_test_run *lpTestRun, *_n;

	list_for_each_entry_safe(lpTestRun, 
							_n, 
							&this->mTestRunListHead, 
							mTestRunList) {

		if(lpTestRun->GetID(lpTestRun) == anID){

			return lpTestRun;
		}

	}		

	return NULL;
}

static struct i2c_test_set *i2c_test_module_FindTestSetByID(struct i2c_test_module *this, unsigned anID)
{
	struct i2c_test_set *lpTestSet, *_n;

	list_for_each_entry_safe(lpTestSet, 
							_n, 
							&this->mAllTestSetListHead, 
							mAllTestSetList) {

		if(lpTestSet->GetID(lpTestSet) == anID){

			return lpTestSet;
		}

	}		

	return NULL;
}

static int i2c_test_module_AttachTestSetToTestRun(struct i2c_test_module *this,
					unsigned anTestRunID,
					unsigned anTestSetID)
{
	int lnResult = 0;

	struct i2c_test_set *lpTestSet = NULL;

	struct i2c_test_run *lpTestRun = NULL;

	char lpStr[256];

	lpTestSet = i2c_test_module_FindTestSetByID(this,anTestSetID);

	assert(NULL != lpTestSet);

	if(NULL == lpTestSet){

		return (-1);
	}

	lpTestRun = i2c_test_module_FindTestRunByID(this,anTestRunID);

	assert(NULL != lpTestRun);

	if(NULL == lpTestRun){

		return (-1);
	}
	
	lnResult = lpTestRun->AddTestSet(lpTestRun, lpTestSet);

	lpTestRun->ShowStatus(lpTestRun,lpStr,true);

	I2CST_LOG(I2C_LOG_LEVEL_JUST_SUMMARY,
		lpStr);

	assert(0 == lnResult);

	return lnResult;


}
static int i2c_test_module_AttachAllNoOwnerTestSetsToTestRun(struct i2c_test_module *this,
					unsigned anTestRunID)
{
	struct i2c_test_set *lpTestSet, *_n;

	list_for_each_entry_safe(lpTestSet, 
							_n, 
							&this->mAllTestSetListHead, 
							mAllTestSetList) {
							
		if(NULL == lpTestSet->mpTestRun){

			printk("AttachAllNoOwnerTestSetsToTestRun%d",lpTestSet->GetID(lpTestSet));

			this->AttachTestSetToTestRun(this,
				anTestRunID,
				lpTestSet->GetID(lpTestSet));
		}
	}		

	return 0;	

}
static int i2c_test_module_RunAllTestRun(struct i2c_test_module *this)
{
	struct i2c_test_run *lpTestRun, *_n;

	list_for_each_entry_safe(lpTestRun, 
							_n, 
							&this->mTestRunListHead, 
							mTestRunList) {

		this->RunTestRun(this,
			lpTestRun->GetID(lpTestRun));

	}		

	return 0;
}
static int i2c_test_module_SetAllMaxIntervalBetweenRunningEveryTestSet(struct i2c_test_module *this,
	unsigned anMaxIntervalBetweenRunningEveryTestSet)
{
	struct i2c_test_run *lpTestRun, *_n;

	list_for_each_entry_safe(lpTestRun, 
							_n, 
							&this->mTestRunListHead, 
							mTestRunList) {

		this->SetMaxIntervalBetweenRunningEveryTestSet(this,
			lpTestRun->GetID(lpTestRun),
			anMaxIntervalBetweenRunningEveryTestSet);

	}		

	return 0;
}
static int i2c_test_module_SetAllFixedIntervalMode(struct i2c_test_module *this
	,bool abFixedIntervalMode)
{
	struct i2c_test_run *lpTestRun, *_n;

	list_for_each_entry_safe(lpTestRun, 
							_n, 
							&this->mTestRunListHead, 
							mTestRunList) {

		this->SetFixedIntervalMode(this,
			lpTestRun->GetID(lpTestRun),
			abFixedIntervalMode);

	}		

	return 0;
}

static int i2c_test_module_SetAllMaxTestTimes(struct i2c_test_module *this
	, int anMaxTestTimes)
{
	struct i2c_test_run *lpTestRun, *_n;

	list_for_each_entry_safe(lpTestRun, 
							_n, 
							&this->mTestRunListHead, 
							mTestRunList) {

		this->SetMaxTestTimes(this,
			lpTestRun->GetID(lpTestRun),
			anMaxTestTimes);

	}		

	return 0;
}



static int i2c_test_module_RunTestRun(struct i2c_test_module *this, unsigned anID)
{
	int lnResult = 0;

	struct i2c_test_run *lpTestRun = NULL;

	lpTestRun = i2c_test_module_FindTestRunByID(this,anID);

	assert(NULL != lpTestRun);

	if(NULL == lpTestRun){

		return (-1);
	}	

	if(true == lpTestRun->IsRunning(lpTestRun)){

		lnResult = lpTestRun->Stop(lpTestRun);
	}else{

		lnResult = lpTestRun->Run(lpTestRun);
	}

	return lnResult;
}
static int i2c_test_module_SetMaxIntervalBetweenRunningEveryTestSet(struct i2c_test_module *this, unsigned anID, unsigned anMaxIntervalBetweenRunningEveryTestSet)
{
	int lnResult = 0;

	char lpStr[256];

	struct i2c_test_run *lpTestRun = NULL;

	lpTestRun = i2c_test_module_FindTestRunByID(this,anID);

	assert(NULL != lpTestRun);

	if(NULL == lpTestRun){

		return (-1);
	}	

	lnResult = lpTestRun->SetMaxIntervalBetweenRunningEveryTestSet(lpTestRun,
		anMaxIntervalBetweenRunningEveryTestSet);

	lpTestRun->ShowStatus(lpTestRun,lpStr,true);

	I2CST_LOG(I2C_LOG_LEVEL_JUST_SUMMARY,
		lpStr);	

	return lnResult;

}
static int i2c_test_module_SetFixedIntervalMode(struct i2c_test_module *this, unsigned anID, bool abFixedIntervalMode)
{
	int lnResult = 0;

	char lpStr[256];

	struct i2c_test_run *lpTestRun = NULL;

	lpTestRun = i2c_test_module_FindTestRunByID(this,anID);

	assert(NULL != lpTestRun);

	if(NULL == lpTestRun){

		return (-1);
	}	

	lnResult = lpTestRun->SetFixedIntervalMode(lpTestRun,
		abFixedIntervalMode);

	lpTestRun->ShowStatus(lpTestRun,lpStr,true);

	I2CST_LOG(I2C_LOG_LEVEL_JUST_SUMMARY,
		lpStr);	


	return lnResult;

}

static int i2c_test_module_SetMaxTestTimes(struct i2c_test_module *this, unsigned anID, int anMaxTestTimes)
{
	int lnResult = 0;

	char lpStr[256];

	struct i2c_test_run *lpTestRun = NULL;

	lpTestRun = i2c_test_module_FindTestRunByID(this,anID);

	assert(NULL != lpTestRun);

	if(NULL == lpTestRun){

		return (-1);
	}	

	lnResult = lpTestRun->SetMaxTestTimes(lpTestRun,
		anMaxTestTimes);

	lpTestRun->ShowStatus(lpTestRun,lpStr,true);

	I2CST_LOG(I2C_LOG_LEVEL_JUST_SUMMARY,
		lpStr);	


	return lnResult;

}
static bool i2c_test_module_IsDuplicateClientAlreadyAdded(
						struct i2c_test_module *this,
						struct i2c_client * apI2CClient)
{
	struct i2c_test_set *lpTestSet, *_n;

	assert(NULL != apI2CClient);

	list_for_each_entry_safe(lpTestSet, 
							_n, 
							&this->mAllTestSetListHead, 
							mAllTestSetList) {
							
		struct i2c_client * lpI2CClient = NULL;

		lpI2CClient = lpTestSet->GetI2CClient(lpTestSet);

		assert(NULL != lpI2CClient);

		if(apI2CClient->addr == lpI2CClient->addr &&
			0 == strcmp(apI2CClient->name,lpI2CClient->name )){

				return true;
		}
	}		

	return false;
}
static int i2c_test_module_SetLogLevel(struct i2c_test_module *this, AXE_I2C_LOG_LEVEL leLogLevel)
{
	if(I2C_LOG_LEVEL_START <= leLogLevel || I2C_LOG_LEVEL_END >=leLogLevel){

		this->meLogLevel = leLogLevel;

		return 0;
	}

	return (-1);

}

static int i2c_test_module_ShowLastSummary(struct i2c_test_module *this,unsigned anID)
{
//	int lnResult = 0;

	struct i2c_test_run *lpTestRun = NULL;

	lpTestRun = i2c_test_module_FindTestRunByID(this,anID);

	assert(NULL != lpTestRun);

	if(NULL == lpTestRun){

		return (-1);
	}

	lpTestRun->mSummary.Summary(&lpTestRun->mSummary);

	return 0;
}
static int i2c_test_module_ShowAllLastSummary(struct i2c_test_module *this)
{

	struct i2c_test_run *lpTestRun, *_n;

	list_for_each_entry_safe(lpTestRun, 
							_n, 
							&this->mTestRunListHead, 
							mTestRunList) {

		lpTestRun->mSummary.Summary(&lpTestRun->mSummary);

	}		

	return 0;

}

static int gnReturnValueOfSelfTestRun = 0;
static int i2c_test_case_SelfTestRun(struct i2c_client *apClient)
{
	I2CST_LOG(I2C_LOG_LEVEL_SELFTEST,"i2c_test_case_SelfTestRun...\n");

	return gnReturnValueOfSelfTestRun;
}
static struct i2c_client gDummyI2CClient1 =
{
	.addr = 0x01,
	.name = "DummyClient1"
};	
static struct i2c_test_case_info i2c_dummy_test_case_info1[] =
{
	{
		.name = "dummyi2ctestcase1",
		.run = i2c_test_case_SelfTestRun,
	},
};
static struct i2c_client gDummyI2CClient2 =
{
	.addr = 0x02,
	.name = "DummyClient2"
};	
static struct i2c_test_case_info i2c_dummy_test_case_info2[] =
{
	{
		.name = "dummyi2ctestcase2",
		.run = i2c_test_case_SelfTestRun,
	},
};
static struct i2c_test_module gI2CTestModule = {
	.mTestRunIDBitArray = 0,
	.mTestSetIDBitArray = 0,
	.meLogLevel = I2C_LOG_LEVEL_ALL_OPEN,
	.mbInited = false,
	.Init = i2c_test_module_Init,
	.Deinit = i2c_test_module_Deinit,
	.ShowStatus = i2c_test_module_ShowStatus,
	.CreateTestRun = i2c_test_module_CreateTestRun,
	.CreateN2NTestRun = i2c_test_module_CreateN2NTestRun,
	.AttachAllNoOwnerTestSetsToTestRun = i2c_test_module_AttachAllNoOwnerTestSetsToTestRun,
	.AttachTestSetToTestRun = i2c_test_module_AttachTestSetToTestRun,
	.RunTestRun = i2c_test_module_RunTestRun,
	.RunAllTestRun = i2c_test_module_RunAllTestRun,
	.SetMaxIntervalBetweenRunningEveryTestSet = i2c_test_module_SetMaxIntervalBetweenRunningEveryTestSet,
	.SetFixedIntervalMode = i2c_test_module_SetFixedIntervalMode,
	.SetMaxTestTimes =i2c_test_module_SetMaxTestTimes,
	.SetAllMaxIntervalBetweenRunningEveryTestSet = i2c_test_module_SetAllMaxIntervalBetweenRunningEveryTestSet,
	.SetAllFixedIntervalMode = i2c_test_module_SetAllFixedIntervalMode,
	.SetAllMaxTestTimes =i2c_test_module_SetAllMaxTestTimes,
	.IsDuplicateClientAlreadyAdded = i2c_test_module_IsDuplicateClientAlreadyAdded,
	.SetLogLevel = i2c_test_module_SetLogLevel,
	.ShowLastSummary = i2c_test_module_ShowLastSummary,
	.ShowAllLastSummary = i2c_test_module_ShowAllLastSummary,
};
static int i2c_test_case_SelfTestCreate(struct i2c_test_case *apTestCase)
{
	assert(NULL == apTestCase->mpClient);

	assert( true == list_empty(&apTestCase->mTestCaseList));

	//assert(NULL == apTestCase->mpTestCaseInfo);

	assert(0 == apTestCase->mnTestPassedTimes);
	
	assert(0 == apTestCase->mnTestFailTimes);
	
	assert(i2c_test_case_Init == apTestCase->Init);

	assert(i2c_test_case_Deinit == apTestCase->Deinit);

	assert(i2c_test_case_Run == apTestCase->Run);

	assert(i2c_test_case_GetTestName == apTestCase->GetTestName);

	assert(i2c_test_case_GetPassTimes == apTestCase->GetPassTimes);

	assert(i2c_test_case_GetFailTimes == apTestCase->GetFailTimes);

	assert(i2c_test_case_GetTotalTimes == apTestCase->GetTotalTimes);

	assert(i2c_test_case_ResetTimes == apTestCase->ResetTimes);

	assert(i2c_test_case_GetI2CClient == apTestCase->GetI2CClient);

	assert(i2c_test_case_Summary == apTestCase->mSummary.Summary);
	
	return 0;
}
static void i2c_test_case_Selftest(struct work_struct *work)
{
	struct i2c_test_case *lpTestCase = NULL;

	int lnResult;

	struct i2c_client lI2CClient;	

	struct i2c_test_case_info lInfo = {"test",i2c_test_case_SelfTestRun};

	AXE_I2C_LOG_LEVEL leLogLevel = gI2CTestModule.meLogLevel;

	lI2CClient.addr = 0x33;

	strcpy(lI2CClient.name,"selftest");

	gI2CTestModule.meLogLevel = I2C_LOG_LEVEL_JUST_SUMMARY;

	//Create
	if(NULL == lpTestCase){

		lnResult = i2c_test_case_Create(&lpTestCase);

		assert(0 == lnResult);
	}
	
	assert(NULL != lpTestCase);

	if(NULL != lpTestCase){

		//Test Create
		I2CST_LOG(I2C_LOG_LEVEL_SELFTEST,"TestCase:Create...\n");
		
		i2c_test_case_SelfTestCreate(lpTestCase);

		//Test Init
		lnResult = lpTestCase->Init(lpTestCase,&lI2CClient,&lInfo);

		assert(0 == lnResult);

		//Test Get
		I2CST_LOG(I2C_LOG_LEVEL_SELFTEST,"TestCase:Get Pass and Fail Times\n");
		
		assert(0 == lpTestCase->GetFailTimes(lpTestCase));

		assert(0 == lpTestCase->GetPassTimes(lpTestCase));

		assert(0 == lpTestCase->GetTotalTimes(lpTestCase));


		//Test Run & pass/fail/total times
		I2CST_LOG(I2C_LOG_LEVEL_SELFTEST,"TestCase:Run...\n");
		gnReturnValueOfSelfTestRun = 0;

		assert(gnReturnValueOfSelfTestRun == lpTestCase->Run(lpTestCase));

		assert(0 == lpTestCase->GetFailTimes(lpTestCase));

		assert(1 == lpTestCase->GetPassTimes(lpTestCase));

		assert(1 == lpTestCase->GetTotalTimes(lpTestCase));

		gnReturnValueOfSelfTestRun = -1;

		assert(gnReturnValueOfSelfTestRun == lpTestCase->Run(lpTestCase));

		assert(1 == lpTestCase->GetFailTimes(lpTestCase));

		assert(1 == lpTestCase->GetPassTimes(lpTestCase));

		assert(2 == lpTestCase->GetTotalTimes(lpTestCase));

		lnResult = lpTestCase->ResetTimes(lpTestCase);

		assert(0 == lpTestCase->GetFailTimes(lpTestCase));

		assert(0 == lpTestCase->GetPassTimes(lpTestCase));

		assert(0 == lpTestCase->GetTotalTimes(lpTestCase));

		//Test get name
		I2CST_LOG(I2C_LOG_LEVEL_SELFTEST,"TestCase:Get Name...\n");

		assert(0 == strcmp(lInfo.name,lpTestCase->GetTestName(lpTestCase)));

		//Test Summary
		I2CST_LOG(I2C_LOG_LEVEL_SELFTEST,"TestCase:Summary...\n");

		lnResult = lpTestCase->mSummary.Summary(&lpTestCase->mSummary);

		assert(0 == lnResult);

		//Test Deinit
		I2CST_LOG(I2C_LOG_LEVEL_SELFTEST,"TestCase:Deinit & Delete...\n");
		
		lnResult = lpTestCase->Deinit(lpTestCase);

		assert(0 == lnResult);

		//Delete
		i2c_test_case_Delete(lpTestCase);

		lpTestCase = NULL;


	}
	
	gI2CTestModule.meLogLevel = leLogLevel;
	
}

static int i2c_test_set_SelfTestCreate(struct i2c_test_set *apTestSet)
{
	assert(NULL == apTestSet->mpName);

	assert(INITINAL_TEST_SET_ID == apTestSet->mnID);

	assert(NULL == apTestSet->mpTestRun);

	assert( true == list_empty(&apTestSet->mTestSetListInTheSameRun));

	assert( true == list_empty(&apTestSet->mAllTestSetList));

	assert( true == list_empty(&apTestSet->mTestCaseListHead));
	
	assert(i2c_test_set_Init == apTestSet->Init);

	assert(i2c_test_set_Deinit == apTestSet->Deinit);

	assert(i2c_test_set_Run == apTestSet->Run);

	assert(i2c_test_set_Stop == apTestSet->Stop);

	assert(i2c_test_set_AddTestCase == apTestSet->AddTestCase);

	assert(i2c_test_set_SetIntervalBetweenRunningEveryTestCase == apTestSet->SetIntervalBetweenRunningEveryTestCase);

	assert(i2c_test_set_GetI2CClient == apTestSet->GetI2CClient);

	assert(i2c_test_set_GetID == apTestSet->GetID);

	assert(i2c_test_set_SetOwner == apTestSet->SetOwner);

	assert(i2c_test_set_RemoveOwner == apTestSet->RemoveOwner);

	assert(i2c_test_set_SetIntervalGenerator == apTestSet->SetIntervalGenerator);

	assert(i2c_test_set_Reset == apTestSet->Reset);

	assert(INTERVAL_BETWEEN_RUNNING_EVERY_TEST_CASE == apTestSet->mnIntervalBetweenRunningEveryTestCase);

	assert(false == apTestSet->mbIsRunning);

	assert(NULL == apTestSet->mpIntervalGenerator);

	assert(i2c_test_set_Summary == apTestSet->mSummary.Summary);

	return 0;
}
#define TEST_CASE_2
static void i2c_test_set_Selftest(struct work_struct *work)
{
	struct i2c_test_case *lpTestCase1 =NULL;
	struct i2c_test_case *lpTestCase2 = NULL;
	struct i2c_test_case *lpTempTestSet= NULL;

	struct i2c_test_set *lpTestSet = NULL;

	//static bool lbTestCaseAdded = false;

	int lnResult;

	struct i2c_client lI2CClient;

	struct i2c_test_case_info lInfo = {"test",i2c_test_case_SelfTestRun};

	struct i2c_test_case_info lInfo2 = {"test2",i2c_test_case_SelfTestRun};


	AXE_I2C_LOG_LEVEL leLogLevel = gI2CTestModule.meLogLevel;

	lI2CClient.addr = 0x33;

	strcpy(lI2CClient.name,"selftest");	

	gI2CTestModule.meLogLevel = I2C_LOG_LEVEL_JUST_SUMMARY;

	//Create
	if(NULL == lpTestSet){

		lnResult = i2c_test_set_Create(&lpTestSet);

		assert(0 == lnResult);

	}
	
	assert(NULL != lpTestSet);

	if(NULL != lpTestSet){

		//Test Create...
		I2CST_LOG(I2C_LOG_LEVEL_SELFTEST,"TestSet:Create...\n");
		i2c_test_set_SelfTestCreate(lpTestSet);

		//Test Init..
		lnResult = lpTestSet->Init(lpTestSet, 0,"Set_ForSelfTestSet");

		assert (0 == lnResult);

		lnResult = lpTestSet->Run(lpTestSet);

		assert (0 != lnResult);

		//Add test case
		I2CST_LOG(I2C_LOG_LEVEL_SELFTEST,"TestSet:Add test cases...\n");

		if(NULL  == lpTestCase1){

			lnResult = i2c_test_case_Create(&lpTestCase1);

			assert (0 == lnResult);
		}
		
#ifdef TEST_CASE_2	

		if(NULL  == lpTestCase2){

			lnResult = i2c_test_case_Create(&lpTestCase2);

			assert (0 == lnResult);
		}

		//printk("Test add test case 1\n");
#endif	
		lnResult = lpTestCase1->Init(lpTestCase1,&lI2CClient,&lInfo);

		assert(0 == lnResult);
#ifdef TEST_CASE_2	

		lnResult = lpTestCase2->Init(lpTestCase2,&lI2CClient,&lInfo2);

		assert(0 == lnResult);
#endif

		//printk("Test add test case 2\n");

		//if(false == lbTestCaseAdded){
		{

			//lbTestCaseAdded = true;

			lnResult = lpTestSet->AddTestCase(lpTestSet,lpTestCase1);

			//printk("Test add test case 2--\n");

			lpTempTestSet = container_of(lpTestSet->mTestCaseListHead.next,
												struct i2c_test_case,
												mTestCaseList);		

			assert (lpTestCase1 == lpTempTestSet);

			assert(0 == strcmp(lInfo.name,lpTempTestSet->GetTestName(lpTempTestSet)));


#ifdef TEST_CASE_2

			//printk("Test add test case 3\n");

			lnResult = lpTestSet->AddTestCase(lpTestSet,lpTestCase2);

			lpTempTestSet = container_of(lpTestSet->mTestCaseListHead.next->next,
												struct i2c_test_case,
												mTestCaseList);


			assert (lpTestCase2 == lpTempTestSet);

			
			assert(0 == strcmp(lInfo2.name,lpTempTestSet->GetTestName(lpTempTestSet)));

#endif


		}


		//Test Run
		I2CST_LOG(I2C_LOG_LEVEL_SELFTEST,"TestSet:Run...\n");

		//Set 0 to prevent msleep...
		lnResult = lpTestSet->SetIntervalBetweenRunningEveryTestCase(
					lpTestSet,
					0);

		assert(0 == lnResult);

		assert(0 == lpTestSet->mnIntervalBetweenRunningEveryTestCase);

		gnReturnValueOfSelfTestRun = 0;

		lnResult = lpTestSet->Run(lpTestSet);

		assert(0 == lnResult);

		//printk("1:pass%d,fail%d\n",
			//lpTestCase1->GetPassTimes(lpTestCase1),
			//lpTestCase1->GetFailTimes(lpTestCase1));
#ifdef TEST_CASE_2

		//printk("2:pass%d,fail%d\n",
		//	lpTestCase2->GetPassTimes(lpTestCase2),
		//	lpTestCase2->GetFailTimes(lpTestCase2));
#endif


		assert(1 == lpTestCase1->GetPassTimes(lpTestCase1));

		assert(0 == lpTestCase1->GetFailTimes(lpTestCase1));

		assert(1 == lpTestCase1->GetTotalTimes(lpTestCase1));
#ifdef TEST_CASE_2

		assert(1 == lpTestCase2->GetPassTimes(lpTestCase2));

		assert(0 == lpTestCase2->GetFailTimes(lpTestCase2));

		assert(1 == lpTestCase2->GetTotalTimes(lpTestCase2));
#endif	
		gnReturnValueOfSelfTestRun = -1;

		lnResult = lpTestSet->Run(lpTestSet);

		assert(0 == lnResult);

		assert(1 == lpTestCase1->GetPassTimes(lpTestCase1));

		assert(1 == lpTestCase1->GetFailTimes(lpTestCase1));

		assert(2 == lpTestCase1->GetTotalTimes(lpTestCase1));
#ifdef TEST_CASE_2

		assert(1 == lpTestCase2->GetPassTimes(lpTestCase2));

		assert(1 == lpTestCase2->GetFailTimes(lpTestCase2));

		assert(2 == lpTestCase2->GetTotalTimes(lpTestCase2));
#endif

		//Test GetClient
		I2CST_LOG(I2C_LOG_LEVEL_SELFTEST,"TestSet:GetClient...\n");

		assert(&lI2CClient == lpTestSet->GetI2CClient(lpTestSet));

		//Test Summary
		I2CST_LOG(I2C_LOG_LEVEL_SELFTEST,"TestSet:Summary...\n");

		lpTestSet->mSummary.Summary(&lpTestSet->mSummary);

		assert (0 == lnResult);

		//Deinit...
		lnResult = lpTestSet->Deinit(lpTestSet);

		assert (0 == lnResult);

		I2CST_LOG(I2C_LOG_LEVEL_SELFTEST,"TestSet:Deinit & Delete...\n");

		//Delete
		i2c_test_set_Delete(lpTestSet);


	}


	gI2CTestModule.meLogLevel = leLogLevel;

	//printk("Test Deinit--\n");
	
}
static int i2c_test_run_SelfTestCreate(struct i2c_test_run *apTestRun)
{
	assert( true == list_empty(&apTestRun->mTestRunList));

	assert( INITINAL_TEST_RUN_ID == apTestRun->mnID);
	
	assert( true == list_empty(&apTestRun->mTestSetListHead));

	assert( NULL == apTestRun->mpCurrentTestSet);
	
	assert( NULL == apTestRun->mpTestModule);

#ifdef TEST_RUN_IMPLEMENTED_BY_KTHREAD
	assert(NULL ==  apTestRun->mpkthread);
#endif

	assert( false == apTestRun->mbIsRunning);
	

	//assert( DEFAULT_MAX_INTERVAL == apTestRun->mnMaxIntervalBetweenRunningEveryTestSet);
	
	assert( DEFAULT_MAX_TEST_TIMES == apTestRun->mnMaxTestTimes);

	assert( i2c_test_run_Init == apTestRun->Init);

	assert( i2c_test_run_Deinit == apTestRun->Deinit);

	assert( i2c_test_run_Run == apTestRun->Run);

	assert( i2c_test_run_Stop == apTestRun->Stop);

	assert( i2c_test_run_AddTestSet == apTestRun->AddTestSet);

	assert( i2c_test_run_GetID == apTestRun->GetID);
	
	assert( i2c_test_run_SetMaxIntervalBetweenRunningEveryTestSet == apTestRun->SetMaxIntervalBetweenRunningEveryTestSet);

	assert( i2c_test_run_SetFixedIntervalMode == apTestRun->SetFixedIntervalMode);

	assert( i2c_test_run_SetMaxTestTimes == apTestRun->SetMaxTestTimes);

	assert( i2c_test_run_IsRunning == apTestRun->IsRunning);

	assert( i2c_test_run_ShowStatus == apTestRun->ShowStatus);
	
	assert( i2c_test_run_Summary == apTestRun->mSummary.Summary);

	assert( false == apTestRun->mbIsTestMode);

	i2c_test_interval_generator_SelfTestCreate(&apTestRun->mIntervalGenerator);
	
	return 0;
}

static void i2c_test_run_Selftest(struct work_struct *work)
{
	
	struct i2c_test_run *lpTestRun = NULL;

	struct i2c_test_set *lpTestSet = NULL;

	struct i2c_test_case *lpTestCase = NULL;

	struct i2c_client lI2CClient;

	struct i2c_test_case_info lInfo = {"test",i2c_test_case_SelfTestRun};


	int lnResult;

	AXE_I2C_LOG_LEVEL leLogLevel = gI2CTestModule.meLogLevel;

	lI2CClient.addr = 0x33;

	strcpy(lI2CClient.name,"selftest");

	gI2CTestModule.meLogLevel = I2C_LOG_LEVEL_JUST_SUMMARY;		

	if(NULL  == lpTestRun){

		I2CST_LOG(I2C_LOG_LEVEL_SELFTEST,"TestRun:Create...\n");

		lnResult = i2c_test_run_Create(&lpTestRun);

		assert(0 == lnResult);

		i2c_test_run_SelfTestCreate(lpTestRun);

		assert(0 == lnResult);
	}

	assert(NULL != lpTestRun);

	if(NULL != lpTestRun){

		//Init
		I2CST_LOG(I2C_LOG_LEVEL_SELFTEST,"TestRun:Init...\n");
		lnResult = lpTestRun->Init(lpTestRun,1001, NULL);

		assert(0 == lnResult);

		assert(1001 == lpTestRun->GetID(lpTestRun));

		//Run, no set inhere...
		lnResult = lpTestRun->Run(lpTestRun);

		assert(0 != lnResult);

		if(NULL == lpTestSet){

			//Create Test Case & Set
			lnResult = i2c_test_set_Create(&lpTestSet);

			assert(0 == lnResult);

			//Test Init..
			lnResult = lpTestSet->Init(lpTestSet, 0, "Test_ForSelftestRun");

			assert(0 == lnResult);

		}


		if(NULL  == lpTestCase){

			lnResult = i2c_test_case_Create(&lpTestCase);

			assert (0 == lnResult);


			lnResult = lpTestCase->Init(lpTestCase,&lI2CClient,&lInfo);

			assert(0 == lnResult);

			
		}
		
		I2CST_LOG(I2C_LOG_LEVEL_SELFTEST,"TestRun:Add test case & set...\n");
		lnResult = lpTestSet->AddTestCase(lpTestSet,lpTestCase);

		assert (0 == lnResult);

		lnResult = lpTestRun->AddTestSet(lpTestRun,lpTestSet);

		assert (0 == lnResult);

		lnResult = lpTestRun->SetMaxIntervalBetweenRunningEveryTestSet(lpTestRun,100);

		assert (100 == lpTestRun->mIntervalGenerator.GetMaxInterval(&lpTestRun->mIntervalGenerator));

		lnResult = lpTestRun->SetMaxTestTimes(lpTestRun, 3);

		assert( 3 == lpTestRun->mnMaxTestTimes);

		//turn on test mode
		I2CST_LOG(I2C_LOG_LEVEL_SELFTEST,"TestRun:Run...\n");
		lpTestRun->mbIsTestMode = true;

		lnResult = lpTestRun->SetMaxIntervalBetweenRunningEveryTestSet(lpTestRun,0);

		gnReturnValueOfSelfTestRun = 0;

		lnResult = lpTestRun->Run(lpTestRun);

		assert(0 == lnResult);

		assert(3 == lpTestCase->GetPassTimes(lpTestCase));

		assert(0 == lpTestCase->GetFailTimes(lpTestCase));

		assert(3 == lpTestCase->GetTotalTimes(lpTestCase));

		//turo off test mode
		lpTestRun->mbIsTestMode = false;


		//Test Summary
		//I2CST_LOG(I2C_LOG_LEVEL_SELFTEST,"TestRun:Summary...\n");

		//lnResult = lpTestRun->mSummary.Summary(&lpTestRun->mSummary);

		//assert(0 == lnResult);


		//Deinit
		I2CST_LOG(I2C_LOG_LEVEL_SELFTEST,"TestRun:Deinit & Delete...\n");
		lnResult = lpTestRun->Deinit(lpTestRun);	

		assert(0 == lnResult);
		
		//Delete
		lnResult = i2c_test_set_Delete(lpTestSet);

		assert(0 == lnResult);

		lnResult = i2c_test_run_Delete(lpTestRun);

		lpTestRun = NULL;		

		assert(0 == lnResult);
	}

	gI2CTestModule.meLogLevel = leLogLevel ;
}

static void i2c_test_module_Selftest(struct work_struct *work)
{

	struct i2c_test_module * lpTestModule = &gI2CTestModule;

	static struct i2c_test_run * lpTestRun1 = NULL;

	static struct i2c_test_run * lpTestRun2 = NULL;	

	static struct i2c_test_set * lpTestSet1 = NULL;

	static struct i2c_test_set * lpTestSet2 = NULL;

	static struct i2c_test_case * lpTestCase1 = NULL;

	static struct i2c_test_case * lpTestCase2 = NULL;

	struct i2c_test_run *lpTestRunTemp ,*_n;	

	struct i2c_test_set *lpTestSetTemp ,*_n1;

	AXE_I2C_LOG_LEVEL leLogLevel = lpTestModule->meLogLevel;

	//Create 1st test run
	int lnIndex = MAX_NUM_OF_TEST_RUN;

	int lnResult = 0;

	static unsigned lnTestRun1ID, lnTestRun2ID;

	lpTestModule->meLogLevel = I2C_LOG_LEVEL_JUST_SUMMARY;

	I2CST_LOG(I2C_LOG_LEVEL_SELFTEST,"TestMod:ID test\n");

	if(NULL  == lpTestRun1){

		lnIndex = find_first_zero_bit(&lpTestModule->mTestRunIDBitArray, MAX_NUM_OF_TEST_RUN);

		lpTestModule->CreateTestRun(lpTestModule, &lnTestRun1ID);

		assert(ENCODE_TEST_RUN_ID(lnIndex) == lnTestRun1ID);

		lnIndex = find_first_zero_bit(&lpTestModule->mTestRunIDBitArray, MAX_NUM_OF_TEST_RUN);

		lpTestModule->CreateTestRun(lpTestModule, &lnTestRun2ID);

		assert(ENCODE_TEST_RUN_ID(lnIndex) == lnTestRun2ID);


		I2CST_LOG(I2C_LOG_LEVEL_SELFTEST,"TestMod:Add Test Run\n");

		list_for_each_entry_safe(lpTestRunTemp, 
								_n, 
								&lpTestModule->mTestRunListHead, 
								mTestRunList) {

			if(lnTestRun1ID == lpTestRunTemp->GetID(lpTestRunTemp)){

				lpTestRun1 = lpTestRunTemp;
			}

			if(lnTestRun2ID == lpTestRunTemp->GetID(lpTestRunTemp)){

				lpTestRun2 = lpTestRunTemp;
			}
		}	

		assert(NULL !=lpTestRun1 );

		assert(NULL !=lpTestRun2 );	

		I2CST_LOG(I2C_LOG_LEVEL_SELFTEST,"TestMod:Add Test Case\n");

		//add test set & case
		assert(false == lpTestModule->IsDuplicateClientAlreadyAdded(lpTestModule,&gDummyI2CClient1));

		assert(false == lpTestModule->IsDuplicateClientAlreadyAdded(lpTestModule,&gDummyI2CClient2));


		//I2CST_LOG(I2C_LOG_LEVEL_SELFTEST,"TestMod:Add Test Case1\n");

		assert (0 == i2c_add_test_case(&gDummyI2CClient1,
		"DummySet1",
		ARRAY_AND_SIZE(i2c_dummy_test_case_info1)));

		//I2CST_LOG(I2C_LOG_LEVEL_SELFTEST,"TestMod:Add Test Case2\n");

		assert (0 == i2c_add_test_case(&gDummyI2CClient2,
		"DummySet2",
		ARRAY_AND_SIZE(i2c_dummy_test_case_info2)));
		
		assert(true == lpTestModule->IsDuplicateClientAlreadyAdded(lpTestModule,&gDummyI2CClient1));
		
		assert(true == lpTestModule->IsDuplicateClientAlreadyAdded(lpTestModule,&gDummyI2CClient2));
		
		
		list_for_each_entry_safe(lpTestSetTemp, 
								_n1, 
								&lpTestModule->mAllTestSetListHead, 
								mAllTestSetList) {
								
			struct i2c_client * lpI2CClient = NULL;
		
			lpI2CClient = lpTestSetTemp->GetI2CClient(lpTestSetTemp);
		
			assert(NULL != lpI2CClient);
		
			if(gDummyI2CClient1.addr == lpI2CClient->addr &&
				0 == strcmp(gDummyI2CClient1.name,lpI2CClient->name )){
		
					lpTestSet1 = lpTestSetTemp;
		
					lpTestCase1 = list_first_entry(&lpTestSet1->mTestCaseListHead,
						struct i2c_test_case ,
						mTestCaseList);
		
					lnResult = lpTestModule->AttachTestSetToTestRun(lpTestModule,
									lnTestRun1ID,
									lpTestSet1->GetID(lpTestSet1));
		
					assert(0 == lnResult);				
			}
		
			if(gDummyI2CClient2.addr == lpI2CClient->addr &&
				0 == strcmp(gDummyI2CClient2.name,lpI2CClient->name )){
			
					lpTestSet2 = lpTestSetTemp;
		
					lpTestCase2 = list_first_entry(&lpTestSet2->mTestCaseListHead,
						struct i2c_test_case ,
						mTestCaseList); 	
					
					lnResult = lpTestModule->AttachTestSetToTestRun(lpTestModule,
									lnTestRun2ID,
									lpTestSet2->GetID(lpTestSet2));
					
					assert(0 == lnResult);
		
		
			}
		
		}

	}

	assert(NULL != lpTestSet1);

	assert(NULL != lpTestSet2);

	assert(NULL != lpTestCase1);

	assert(NULL != lpTestCase2);

	I2CST_LOG(I2C_LOG_LEVEL_SELFTEST,"TestMod:Set value\n");
	
	lnResult = lpTestModule->SetMaxIntervalBetweenRunningEveryTestSet(lpTestModule,
		lnTestRun1ID,
		1000);

	assert(1000 == lpTestRun1->mIntervalGenerator.GetMaxInterval(&lpTestRun1->mIntervalGenerator));

	lnResult = lpTestModule->SetMaxTestTimes(lpTestModule,
		lnTestRun2ID,
		1000);

	assert(1000 == lpTestRun2->mnMaxTestTimes);

	lnResult = lpTestModule->SetMaxIntervalBetweenRunningEveryTestSet(lpTestModule,
		lnTestRun1ID,
		1000);

	assert(1000 == lpTestRun1->mIntervalGenerator.GetMaxInterval(&lpTestRun1->mIntervalGenerator));

	lnResult = lpTestModule->SetMaxTestTimes(lpTestModule,
		lnTestRun2ID,
		1000);

	assert(1000 == lpTestRun2->mnMaxTestTimes);
 

	lnResult = lpTestRun1->SetMaxIntervalBetweenRunningEveryTestSet(lpTestRun1,100);

	assert(0 == lnResult);
	
	lnResult = lpTestRun2->SetMaxIntervalBetweenRunningEveryTestSet(lpTestRun2,100);

	assert(0 == lnResult);

	lnResult = lpTestRun1->SetMaxTestTimes(lpTestRun1, 3);

	assert(0 == lnResult);

	lnResult = lpTestRun2->SetMaxTestTimes(lpTestRun2, 3);

	assert(0 == lnResult);

	lpTestRun1->mbIsTestMode = true;

	lpTestRun2->mbIsTestMode = true;

	I2CST_LOG(I2C_LOG_LEVEL_SELFTEST,"TestMod:Run\n");

	gnReturnValueOfSelfTestRun = 0;

	lnResult = lpTestRun1->Run(lpTestRun1);

	assert(0 == lnResult);

	gnReturnValueOfSelfTestRun = -1;

	lnResult = lpTestRun2->Run(lpTestRun2);

	assert(0 == lnResult);

	I2CST_LOG(I2C_LOG_LEVEL_SELFTEST,"TestMod:Test Result\n");

	assert(3 == lpTestCase1->GetPassTimes(lpTestCase1));
	
	assert(0 == lpTestCase1->GetFailTimes(lpTestCase1));
	
	assert(3 == lpTestCase1->GetTotalTimes(lpTestCase1));

	assert(0 == lpTestCase2->GetPassTimes(lpTestCase2));

	assert(3 == lpTestCase2->GetFailTimes(lpTestCase2));

	assert(3 == lpTestCase2->GetTotalTimes(lpTestCase2));

	I2CST_LOG(I2C_LOG_LEVEL_SELFTEST,"TestMod:End\n");

	lpTestCase1->ResetTimes(lpTestCase1);

	lpTestCase2->ResetTimes(lpTestCase2);

	lpTestRun1->mbIsTestMode = false;
	
	lpTestRun2->mbIsTestMode = false;

	lpTestModule->meLogLevel = leLogLevel;

}




static int I2CST_LOG(AXE_I2C_LOG_LEVEL aeLevel, const char *fmt, ...)
{	
	va_list args;

	int r = 0;;
	
	if (aeLevel >= gI2CTestModule.meLogLevel) {

			r += printk("[I2CST]");

			va_start(args, fmt);

        	r = vprintk(fmt, args);

        	va_end(args);
	}

	return r;
}

/*
//	The following is provided to every client
static bool i2c_has_test_set_added(struct i2c_test_module *apTestModule,
										struct i2c_client *apClient)
{
	
	struct i2c_test_set *lpTestSet, *_n;

	//if Test Run existed
	if(NULL != apTestModule->mpAllTestSetHead){

		list_for_each_entry_safe(lpTestSet, 
								_n, 
								&apTestModule->mpAllTestSetHead->mAllTestSetList, 
								mAllTestSetList) {
		
								
			if(apClient == lpTestSet->mpClient){
		
				return true;
			}
		}
	}

	return false;

}
static int i2c_init_test_set(struct i2c_test_module *apTestModule,
							struct i2c_test_set	*apTestSet,
							struct i2c_client *apClient,
	  						struct i2c_test_case const *apTestCaseList,
	  						unsigned anSize)
{

	apTestSet->mpClient = apClient;

	INIT_LIST_HEAD(&apTestSet->mTestSetList);

	INIT_LIST_HEAD(&apTestSet->mAllTestSetList);

	apTestSet->mpTestCaseList = apTestCaseList;

	apTestSet->mnTestCaseNum = anSize;

	apTestSet->mpTestRun = NULL;

	if(NULL == apTestModule->mpAllTestSetHead){

		apTestModule->mpAllTestSetHead = apTestSet;

	}else{

		list_add_tail(&apTestSet->mAllTestSetList, &apTestModule->mpAllTestSetHead->mAllTestSetList);

	}

	return 0;


}
static int i2c_bind_test_set(struct i2c_test_set* apTestSet)
{
	if(NULL != apTestSet){

		apTestSet->mpClient = NULL;

		INIT_LIST_HEAD(&apTestSet->mTestSetList);

		INIT_LIST_HEAD(&apTestSet->mAllTestSetList);

		apTestSet->mpTestCaseList = NULL;

		apTestSet->
	unsigned mnTestCaseNum;
	
	struct i2c_test_run *mpTestRun;
	
	bool mbStartRunning;
	int (*init)(struct i2c_test_set *apTestSet);
	int (*deinit)(struct i2c_test_set *apTestSet);
	int (*run_test_set)(struct i2c_test_set *apTestSet);
	int (*stop_test_set)(struct i2c_test_set *apTestSet);

	unsigned mnIntervalBetweenRunningEveryTestCase;

		return 0;
	}

	return (-1);

}



int i2c_add_test_case(struct i2c_client *apClient,
	  struct i2c_test_case_info const *apTestCaseList, 
	  unsigned anSize)
{
	assert(NULL != apClient);

	assert(NULL != apTestCaseList);

	if(false == i2c_has_test_set_added(&gI2CTestModule, apClient)){

		struct i2c_test_set *lpTestSet = NULL;

		lpTestSet = kzalloc(sizeof(*i2c_test_set), GFP_KERNEL);

		assert(NULL != lpTestSet);

		i2c_init_test_set(lpTestSet,
						apClient,
						apTestCaseList,
						anSize);

		return 0;
	}

	return (-1);

}
*/
#ifdef COMMAND_WORKER_EXECUTING
struct work_struct mCommandWorker;
static bool gbInitCommandWorker = false;
#endif
int i2c_stresstest_print_status(const char *buf)
{
	if(false == gI2CTestModule.mbInited){

		gI2CTestModule.Init(&gI2CTestModule);
	}

	return gI2CTestModule.ShowStatus(&gI2CTestModule, buf);
}
static int i2c_stresstest_command_CreateAndRunTestRun(char *buf)
{
	unsigned lnTestRunID, lnTestSetID;

	int lnResult;

	//char *p;

	if(0 == strncmp("n2n", &buf[1],3)){

		lnResult = gI2CTestModule.CreateN2NTestRun(&gI2CTestModule);

		assert(0 == lnResult);

		lnResult = gI2CTestModule.RunAllTestRun(&gI2CTestModule);

		assert(0 == lnResult);

	}else{

		lnResult = gI2CTestModule.CreateTestRun(&gI2CTestModule,
			&lnTestRunID);

		assert(0 == lnResult);

		if( 0 == lnResult){

			printk("%d\n", lnTestRunID);
		}

		//printk("%s detected\n",&buf[1]);
		
		if(0 == strncmp("all", &buf[1],3)){

			lnResult = gI2CTestModule.AttachAllNoOwnerTestSetsToTestRun(&gI2CTestModule,
				lnTestRunID);


		}else if(' ' == buf[0]){

			lnTestSetID = simple_strtoul(&buf[1], NULL, 10);

			lnResult = gI2CTestModule.AttachTestSetToTestRun(&gI2CTestModule,
				lnTestRunID,
				lnTestSetID);
		
		}

		assert(0 == lnResult);

		lnResult = gI2CTestModule.RunTestRun(&gI2CTestModule,
			lnTestRunID);

		assert(0 == lnResult);

	}
	
	


	return lnResult;

	
}

static int i2c_stresstest_command_CreateTestRun(char *buf)
{
	unsigned lnTestRunID, lnTestSetID;

	int lnResult;

	//char *p;
	


	//printk("%s detected\n",&buf[1]);

	if(0 == strncmp("n2n", &buf[1],3)){

		lnResult = gI2CTestModule.CreateN2NTestRun(&gI2CTestModule);

		assert(0 == lnResult);

	}else{

		lnResult = gI2CTestModule.CreateTestRun(&gI2CTestModule,
			&lnTestRunID);
		
		assert(0 == lnResult);
		
		if( 0 == lnResult){
		
			printk("%d\n", lnTestRunID);
		}


		if(0 == strncmp("all", &buf[1],3)){

			lnResult = gI2CTestModule.AttachAllNoOwnerTestSetsToTestRun(&gI2CTestModule,
				lnTestRunID);

			assert(0 == lnResult);


		}else if(' ' == buf[0]){

			lnTestSetID = simple_strtoul(&buf[1], NULL, 10);

			lnResult = gI2CTestModule.AttachTestSetToTestRun(&gI2CTestModule,
				lnTestRunID,
				lnTestSetID);

			assert(0 == lnResult);
		
		}
	}

	return lnResult;

	
}
static int i2c_stresstest_command_DoSelftest(const char c)
{
	int lnResult = 0;

#ifdef COMMAND_WORKER_EXECUTING
	
	if(false == gbInitCommandWorker){

		INIT_WORK(&mCommandWorker, NULL) ;

		gbInitCommandWorker = true;
	}
#endif


	switch(c){

		case 'c':// for test case
			#ifdef COMMAND_WORKER_EXECUTING

			cancel_work_sync(&mCommandWorker);

			mCommandWorker.func = i2c_test_case_Selftest;
			
			schedule_work(&mCommandWorker); 

			#else

			i2c_test_case_Selftest(NULL);

			#endif
			break;
			
		case 's'://for test set

			#ifdef COMMAND_WORKER_EXECUTING

			cancel_work_sync(&mCommandWorker);

			mCommandWorker.func = i2c_test_set_Selftest;

			schedule_work(&mCommandWorker);

			#else

			i2c_test_set_Selftest(NULL);

			#endif
			break;

		case 'r'://for test run

#ifdef COMMAND_WORKER_EXECUTING

			cancel_work_sync(&mCommandWorker);

			mCommandWorker.func = i2c_test_run_Selftest;

			schedule_work(&mCommandWorker);

#else

			i2c_test_run_Selftest(NULL);

#endif
			break;
			
		case 'm'://for test module
		
#ifdef COMMAND_WORKER_EXECUTING

			cancel_work_sync(&mCommandWorker);

			mCommandWorker.func = i2c_test_module_Selftest;

			schedule_work(&mCommandWorker);

#else

			i2c_test_module_Selftest(NULL);

#endif				

			break;
		default:
			lnResult = -1;
			break;
	}

	return lnResult;
}
static int i2c_stresstest_command_AttachTestSet(char *buf)
{


	int lnResult;

	char *p;

	unsigned lnTestRunID, lnTestSetID;

	
	//printk(KERN_INFO "i2c_stresstest_command_AttachTestSet\n");

	if (!(p = strsep(&buf, " ")) || !*p){

		printk(KERN_INFO "err1");
		
		return -1;
	}

	lnTestRunID = simple_strtoul(p, NULL, 10);

	if (!(p = strsep(&buf, " ")) || !*p){

		printk(KERN_INFO "err2");
		
		return -1;
	}

	

	lnTestSetID = simple_strtoul(p, NULL, 10);
	

	//printk(KERN_INFO "TestRunID = %d,TestSetID = %d\n",lnTestRunID,lnTestSetID);

	
	lnResult = gI2CTestModule.AttachTestSetToTestRun(&gI2CTestModule,
		lnTestRunID,
		lnTestSetID);

	assert(0 == lnResult);

	return lnResult;
	
}
static int i2c_stresstest_command_RunTest(char *buf)
{

	int lnResult;

	char *p;

	unsigned lnTestRunID;

	if(0 == strncmp("all", buf,3)){

		lnResult = gI2CTestModule.RunAllTestRun(&gI2CTestModule);

		assert(0 == lnResult);
		
	}else{
	
		if (!(p = strsep(&buf, " ")) || !*p){
		
			printk(KERN_INFO "err1");
			
			return -1;
		}
		
		lnTestRunID = simple_strtoul(p, NULL, 10);
		
		printk(KERN_INFO "TestRunID = %d\n",lnTestRunID);
		printk("### TestRunID = %d\n",lnTestRunID);		
		
		lnResult = gI2CTestModule.RunTestRun(&gI2CTestModule,
			lnTestRunID);
		
		assert(0 == lnResult);


	}

	return lnResult;
	
}
static int i2c_stresstest_command_PrintLastSummary(char *buf, size_t count)
{
	int lnResult;

//	char *p;

	unsigned lnTestRunID;

	//printk("PrintLastSummary%d",count);
	
	if(count <=1 ){

		
		lnResult = gI2CTestModule.ShowAllLastSummary(&gI2CTestModule);
		
		assert(0 == lnResult);

	}else{		
		lnTestRunID = simple_strtoul(&buf[1], NULL, 10);
		
		//printk(KERN_INFO "TestRunID = %d\n",lnTestRunID);
		
		lnResult = gI2CTestModule.ShowLastSummary(&gI2CTestModule,
			lnTestRunID);
		
		assert(0 == lnResult);

		
	}

	return lnResult;
}
static int i2c_stresstest_command_SetInterval(char *buf)
{


	int lnResult;

	char *p;

	unsigned lnTestRunID, lnInterval;

	//printk(KERN_INFO "i2c_stresstest_command_SetInterval\n");

	if(0 == strncmp("all", buf,3)){

		p = &buf[4];

		lnInterval = simple_strtoul(p, NULL, 10);
		

		//printk(KERN_INFO "TestRunID = %d,TestSetID = %d\n",lnTestRunID,lnTestSetID);

		
		lnResult = gI2CTestModule.SetAllMaxIntervalBetweenRunningEveryTestSet(&gI2CTestModule,
			lnInterval);

		assert(0 == lnResult);


		
	}else{

		if (!(p = strsep(&buf, " ")) || !*p){

			printk(KERN_INFO "err1");
			
			return -1;
		}

		lnTestRunID = simple_strtoul(p, NULL, 10);

		if (!(p = strsep(&buf, " ")) || !*p){

			printk(KERN_INFO "err2");
			
			return -1;
		}

		

		lnInterval = simple_strtoul(p, NULL, 10);
		

		//printk(KERN_INFO "TestRunID = %d,TestSetID = %d\n",lnTestRunID,lnTestSetID);

		
		lnResult = gI2CTestModule.SetMaxIntervalBetweenRunningEveryTestSet(&gI2CTestModule,
			lnTestRunID,
			lnInterval);

		assert(0 == lnResult);

	}




	return lnResult;
	
}
static int i2c_stresstest_command_SetLogLevel(char *buf)
{


	int lnResult;

	char *p;

	unsigned lnLevel;

	//printk(KERN_INFO "i2c_stresstest_command_SetInterval\n");

	if (!(p = strsep(&buf, " ")) || !*p){

		printk(KERN_INFO "err1");
		
		return -1;
	}

	lnLevel = simple_strtoul(p, NULL, 10);

	
	lnResult = gI2CTestModule.SetLogLevel(&gI2CTestModule,
		(AXE_I2C_LOG_LEVEL)lnLevel);


	return lnResult;
	
}


static int i2c_stresstest_command_SetTestMode(char *buf)
{


	int lnResult;

	char *p;

	unsigned lnTestRunID;

	bool lbFixedMode;

	//printk(KERN_INFO "i2c_stresstest_command_SetInterval\n");
	if(0 == strncmp("all", buf,3)){

		p = &buf[4];

		assert('f' == *p || 'r' == *p);

		if('f' == *p){

			lbFixedMode = true;

		}else{

			lbFixedMode = false;
		}

		//lnInterval = simple_strtoul(p, NULL, 10);
		

		//printk(KERN_INFO "TestRunID = %d,TestSetID = %d\n",lnTestRunID,lnTestSetID);

		
		lnResult = gI2CTestModule.SetAllFixedIntervalMode(&gI2CTestModule,
			lbFixedMode);

		assert(0 == lnResult);


		
	}else{

		if (!(p = strsep(&buf, " ")) || !*p){

			printk(KERN_INFO "err1");
			
			return -1;
		}

		lnTestRunID = simple_strtoul(p, NULL, 10);

		if (!(p = strsep(&buf, " ")) || !*p){

			printk(KERN_INFO "err2");
			
			return -1;
		}

		assert('f' == *p || 'r' == *p);

		if('f' == *p){

			lbFixedMode = true;

		}else{

			lbFixedMode = false;
		}

		//lnInterval = simple_strtoul(p, NULL, 10);
		

		//printk(KERN_INFO "TestRunID = %d,TestSetID = %d\n",lnTestRunID,lnTestSetID);

		
		lnResult = gI2CTestModule.SetFixedIntervalMode(&gI2CTestModule,
			lnTestRunID,
			lbFixedMode);

		assert(0 == lnResult);


	}



	return lnResult;
	
}


static int i2c_stresstest_command_SetTestTimes(char *buf)
{


	int lnResult;

	char *p;

	//printk(KERN_INFO "i2c_stresstest_command_SetTestTimes\n");

	unsigned lnTestRunID, lnTestTimes;

	if(0 == strncmp("all", buf,3)){

		p = &buf[4];

		lnTestTimes = simple_strtoul(p, NULL, 10);
		
		
		//printk(KERN_INFO "TestRunID = %d,TestSetID = %d\n",lnTestRunID,lnTestSetID);
		
		
		lnResult = gI2CTestModule.SetAllMaxTestTimes(&gI2CTestModule,
			lnTestTimes);
		
		assert(0 == lnResult);

		
	}else{
	
		if (!(p = strsep(&buf, " ")) || !*p){
		
			printk(KERN_INFO "err1");
			
			return -1;
		}
		
		lnTestRunID = simple_strtoul(p, NULL, 10);
		
		if (!(p = strsep(&buf, " ")) || !*p){
		
			printk(KERN_INFO "err2");
			
			return -1;
		}
		
		
		
		lnTestTimes = simple_strtoul(p, NULL, 10);
		
		
		//printk(KERN_INFO "TestRunID = %d,TestSetID = %d\n",lnTestRunID,lnTestSetID);
		
		
		lnResult = gI2CTestModule.SetMaxTestTimes(&gI2CTestModule,
			lnTestRunID,
			lnTestTimes);
		
		assert(0 == lnResult);


	}



	return lnResult;
	
}


#define LEN_OF_TEMP_STR 128
ssize_t i2c_stresstest_command_parser(const char *buf, size_t count)
{
	char lpStr[LEN_OF_TEMP_STR];

	if(false == gI2CTestModule.mbInited){

		gI2CTestModule.Init(&gI2CTestModule);
	}

	memset(lpStr, 0, LEN_OF_TEMP_STR);

	strncpy(lpStr,buf, count);

	switch(lpStr[0]){

		case 's'://self test component..
			if(0 != i2c_stresstest_command_DoSelftest(lpStr[1])){

				goto I2CST_USAGE;
			}
	
			break;
		case 'c':

			if(0 == strncmp("cr", lpStr,2)){

				if(0 != i2c_stresstest_command_CreateAndRunTestRun(&lpStr[2])){

					goto I2CST_USAGE;
				}


			}else if(0 != i2c_stresstest_command_CreateTestRun(&lpStr[1])){

				goto I2CST_USAGE;
			}			
			
			break;
		case 'a':

			if(' ' != lpStr[1]){

				goto I2CST_USAGE;
			}

			//printk(KERN_INFO "call i2c_stresstest_command_AttachTestSet=%s\n",&lpStr[2]);
		
			if(0 != i2c_stresstest_command_AttachTestSet(&lpStr[2])){
		
				goto I2CST_USAGE;
			}			
			
			break;		
		case 't':
		
			if(' ' != lpStr[1]){
		
				goto I2CST_USAGE;
			}
		
			if(0 != i2c_stresstest_command_SetTestTimes(&lpStr[2])){
		
				goto I2CST_USAGE;
			}			
			
			break;
		case 'f':
		
			if(' ' != lpStr[1]){
		
				goto I2CST_USAGE;
			}
		
			if(0 != i2c_stresstest_command_SetTestMode(&lpStr[2])){
		
				goto I2CST_USAGE;
			}			
			
			break;


		case 'i':
		
			if(' ' != lpStr[1]){
		
				goto I2CST_USAGE;
			}
		
			if(0 != i2c_stresstest_command_SetInterval(&lpStr[2])){
		
				goto I2CST_USAGE;
			}			
			
			break;

		case 'l':
		
			if(' ' != lpStr[1]){
		
				goto I2CST_USAGE;
			}
		
			if(0 != i2c_stresstest_command_SetLogLevel(&lpStr[2])){
		
				goto I2CST_USAGE;
			}			
			
			break;


		case 'r':
		
			if(' ' != lpStr[1]){
		
				goto I2CST_USAGE;
			}

			//printk(KERN_INFO "call i2c_stresstest_command_RunTest=%s\n",&lpStr[2]);
		
		
			if(0 != i2c_stresstest_command_RunTest(&lpStr[2])){
		
				goto I2CST_USAGE;
			}			
			
			break;
		case 'p':
				
			if(0 != i2c_stresstest_command_PrintLastSummary(&lpStr[1],count-1)){
		
				goto I2CST_USAGE;
			}			
			
			break;


		default:
			goto I2CST_USAGE;
			break;
	}

	return count;
	
I2CST_USAGE:

	printk(KERN_INFO "\n");
	printk(KERN_INFO "\nSelftest Case/Set/Run/Module\t\t: s[c|s|r|m]\n");
	printk(KERN_INFO "Create test run task\t\t\t: c [<ID-TestSet>|all|n2n]\n");
	printk(KERN_INFO "Create and run test\t\t\t: cr [<ID-TestSet>|all|n2n]\n");
	printk(KERN_INFO "Attach test set to task\t\t\t: a <ID-TestRun> <ID-TestSet>\n");
	printk(KERN_INFO "Configure Test Times\t\t\t: t <ID-TestRun> <MaxTestTimes>\n");
	printk(KERN_INFO "Set Test Mode(Random or Fixed interval)\t: f <ID-TestRun> [r/f]\n");
	printk(KERN_INFO "Configure Max Interval\t\t\t: i <ID-TestRun> <MaxInterval>\n");
	printk(KERN_INFO "Change log level\t\t\t\t:l <Log-Level>\n");
	printk(KERN_INFO "Run/Stop test\t\t\t\t: r [<ID-TestRun>\all]\n");
	printk(KERN_INFO "Print last summary\t\t\t\t:p [<ID-TestRun>]\n");

	return (count);


}
int i2c_add_test_case(struct i2c_client *apClient,
	  const char		*apTestSetName,
	  struct i2c_test_case_info const *apTestCaseList, unsigned anNum)
{
	int lnResult = 0;

	int i;

	struct i2c_test_set *lpTestSet = NULL;

	int lnIndex = MAX_NUM_OF_TEST_SET;

	if(false == gI2CTestModule.mbInited){

		gI2CTestModule.Init(&gI2CTestModule);
	}


	if(0 == anNum){

		I2CST_LOG(I2C_LOG_LEVEL_ERROR,
			"No test case can be added\n");

		goto FAIL_LABEL;
	}
/*
	if(true == gI2CTestModule.IsDuplicateClientAlreadyAdded(&gI2CTestModule,
		apClient)){

		I2CST_LOG(I2C_LOG_LEVEL_ERROR,
			"This client has already been added\n");

		goto FAIL_LABEL; 
	}
*/	
	lnResult = i2c_test_set_Create(&lpTestSet);

	assert(0 == lnResult);

	assert(NULL != lpTestSet);

	if(NULL == lpTestSet){

		goto FAIL_LABEL;
	}
	//printk("i2c_add_test_case++\n");

	for (i = 0; anNum; anNum--, apTestCaseList++) {

		struct i2c_test_case *lpTestCase = NULL;

		lnResult = i2c_test_case_Create(&lpTestCase);

		assert(0 == lnResult);

		assert(NULL != lpTestCase);

		if(NULL != lpTestCase){

			//printk("i2c_add_test_case:Init\n");
			
			lnResult = lpTestCase->Init(lpTestCase,
				apClient,
				(struct i2c_test_case_info *)apTestCaseList);
			
			assert(0 == lnResult);

			if(0 == lnResult){

				//printk("i2c_add_test_case:AddTestCase\n");
				lnResult = lpTestSet->AddTestCase(lpTestSet,
								lpTestCase);

				assert(0 == lnResult);

			}else{

				goto FAIL_LABEL;
			}

			

		}else{
			goto FAIL_LABEL;

		}
		
	}


	//Get Test Set ID


	lnIndex = find_first_zero_bit(&gI2CTestModule.mTestSetIDBitArray,
		MAX_NUM_OF_TEST_SET);

	/* client list is full */
	if (MAX_NUM_OF_TEST_SET > lnIndex ){

		unsigned lnID;

		lnID = ENCODE_TEST_SET_ID(lnIndex);

		set_bit(lnIndex, &gI2CTestModule.mTestSetIDBitArray);

		lpTestSet->Init(lpTestSet,
						lnID,
						apTestSetName);

		list_add_tail(&lpTestSet->mAllTestSetList, &gI2CTestModule.mAllTestSetListHead);

	}else{

		goto FAIL_LABEL;
	}

	return 0;

FAIL_LABEL:

	if(NULL != lpTestSet){

		lpTestSet->Deinit(lpTestSet);

		i2c_test_set_Delete(lpTestSet);
	}

	return (-1);

}


//I2CST_LOG function for I2CST_LOGging in running tests
int i2c_log_in_test_case(const char *fmt, ...)
{
	
	va_list args;
	int r = 0;

	if (I2C_LOG_LEVEL_ALL_OPEN >= gI2CTestModule.meLogLevel) {
			r += printk("[I2CST]");
        	va_start(args, fmt);
        	r += vprintk(fmt, args);
        	va_end(args);
	}

	return r;
}
void i2c_pxa_retry_logging(
	char *apI2CSlaveName,
	unsigned int anSlaveAddr,
	int anRetryTimes)
{
	if (I2C_LOG_LEVEL_BUS_RETRY >= gI2CTestModule.meLogLevel) {
			printk("asus_i2c: <%s> slave_0x%x retried %d times\n", apI2CSlaveName, 
			anSlaveAddr, anRetryTimes);	
	}
}



#endif //#ifdef CONFIG_I2C_STRESS_TEST


