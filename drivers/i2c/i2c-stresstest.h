/*
        I2C Stress Test include file

*/
#ifndef __AXI_I2C_STRESS_TEST_H__
#define __AXI_I2C_STRESS_TEST_H__
#include <linux/i2c_testcase.h>
#include <linux/list.h>
#include <linux/workqueue.h>
#include <linux/types.h>

#define assert(expr) \
	if (!(expr)) {					\
		printk( "[I2CST]Assertion failed!\n %s,%s,%s,line=%d\n",	\
		#expr,__FILE__,__func__,__LINE__);		\
	}


typedef enum {

	I2C_LOG_LEVEL_START,
	I2C_LOG_LEVEL_ALL_OPEN = I2C_LOG_LEVEL_START,
	I2C_LOG_LEVEL_TEST_CASE,
	I2C_LOG_LEVEL_BUS_RETRY,
	I2C_LOG_LEVEL_JUST_SUMMARY,
	I2C_LOG_LEVEL_ERROR,
	I2C_LOG_LEVEL_SELFTEST,
	I2C_LOG_LEVEL_END = I2C_LOG_LEVEL_SELFTEST

}AXE_I2C_LOG_LEVEL;

typedef enum {

	I2C_TEST_RUN_STATE_RUNNING,
	//I2C_TEST_RUN_STATE_STOPPING,
	I2C_TEST_RUN_STATE_STOPPED

}AXE_I2C_TEST_RUN_STATE;

struct i2c_test_module;
struct i2c_test_run;
struct i2c_test_set;
struct i2c_test_case;
struct i2c_test_summary;

#define INFINITE_I2C_TEST_TIMES (-1)

struct i2c_test_summary {

	int (*Summary)(struct i2c_test_summary *apSummaryObject);
};
/**
 * struct i2c_test_case - the i2c test case for i2c stress test
 * @mTestCaseList: the list of test cases, used by test set.
  * @mpClient: the pointer of i2c_client.
 * @mpTestCaseInfo: the test info, passed by client. 
 * @mnTestPassedTimes: pass times; will be reset by ResetTimes().
 * @mnTestFailTimes: fail times; will be reset by ResetTimes().
 * @Init: Init test case.
 * @Deinit: Deinit test case.
 * @Run: Run the test case.
 * @GetTestName: Get the test name.
 * @GetPassTimes: Get the pass times.
 * @GetFailTimes: Get the fail times. 
 * @GetTotalTimes: Get the total test times.
 * @ResetTimes: Reset the pass/fail/total times.
 * @mSummary: for supporting Summary Interface
 */
struct i2c_test_case {

	struct list_head	mTestCaseList;

	struct i2c_client   *mpClient;
	
	struct i2c_test_case_info mTestCaseInfo;
	
	unsigned mnTestPassedTimes;
	unsigned mnTestFailTimes;
	
	int (*Init)(struct i2c_test_case *this, 
		struct i2c_client   *apClient,
		struct i2c_test_case_info *apTestCaseInfo);
	int (*Deinit)(struct i2c_test_case *this);
	int (*Run)(struct i2c_test_case *this);
	const char *(*GetTestName)(struct i2c_test_case *this);
	unsigned (*GetPassTimes)(struct i2c_test_case *this);
	unsigned (*GetFailTimes)(struct i2c_test_case *this);
	unsigned (*GetTotalTimes)(struct i2c_test_case *this);
	int (*ResetTimes)(struct i2c_test_case *this);
	struct i2c_client   *(*GetI2CClient)(struct i2c_test_case *this);

	struct i2c_test_summary mSummary;
};

#define INTERVAL_BETWEEN_RUNNING_EVERY_TEST_CASE (50)//50ms
struct i2c_test_interval_generator;
struct i2c_test_interval_generator {

	unsigned (*GetInterval)(struct i2c_test_interval_generator *this);
	void (*SetMaxInterval)(struct i2c_test_interval_generator *this,
		unsigned anInterval);
	unsigned (*GetMaxInterval)(struct i2c_test_interval_generator *this);	

	void (*SetMode)(struct i2c_test_interval_generator *this,
		bool abFixedIntervalMode);
	bool (*IsRandom)(struct i2c_test_interval_generator *this);
	bool mbFixedIntervalMode;
	unsigned mnMaxInterval;
};

/**
 * struct i2c_test_set - for i2c stress test
 * @mnID:  unique ID for identify Test Set
 * @mpTestRun: Belongs to which test run. used by test run
 * @mTestSetList: the link list for linking to test sets belong to the same test run. All entries of this list will be destroyed when calling Deinit. used by test run
 * @mAllTestSetList: the link list for linking all test sets. used by test run
 * @mpFirstTestCase: the pointer of test case first-added.
 * @Init: init the member of this object.
 * @Deinit: deinit the member of this object. Destrory all memory allocated in init()
 * @Run: run the test case of this test set one by one. 
 * @stop: stop running.
 * @AddTestCase: Add one test case to this test set.
 * @GetTestClient: Get the client of test set.
 * @mnIntervalBetweenRunningEveryTestCase: the interval for running every test case.
 * @mbIsRunning: record test is running or not
 * @mSummary: for supporting Summary Interface
 * A i2c_test_set can have 0~n test cases.
 */
struct i2c_test_set {

	const char *mpName;

	unsigned mnID;

	struct i2c_test_run *mpTestRun;

	struct list_head	mTestSetListInTheSameRun;

	struct list_head	mAllTestSetList;

	struct list_head mTestCaseListHead;

	int (*Init)(struct i2c_test_set *this, 
		unsigned anID,
		const char *apName);
	int (*Deinit)(struct i2c_test_set *this);
	int (*Run)(struct i2c_test_set *this);
	int (*Stop)(struct i2c_test_set *this);
	int (*AddTestCase)(struct i2c_test_set *this,
			struct i2c_test_case *apTestCase);
	int (*SetIntervalBetweenRunningEveryTestCase)(struct i2c_test_set *this,
		unsigned anIntervalBetweenRunningEveryTestCase);
	struct i2c_client *(*GetI2CClient)(struct i2c_test_set *this);
	unsigned (*GetID)(struct i2c_test_set *this);
	int (*SetOwner)(struct i2c_test_set *this,struct i2c_test_run *apTestRun);
	int (*RemoveOwner)(struct i2c_test_set *this);
	int (*SetIntervalGenerator)(struct i2c_test_set *this,
		struct i2c_test_interval_generator *lpIntervalGenerator);	

	int (*Reset)(struct i2c_test_set *this);

	unsigned mnIntervalBetweenRunningEveryTestCase;

	bool mbIsRunning;

	struct i2c_test_interval_generator *mpIntervalGenerator;

	struct i2c_test_summary mSummary;
};

#define TEST_RUN_IMPLEMENTED_BY_KTHREAD
/**
 * struct i2c_test_run - for i2c stress test run
 * @mTestRunList: List for Test Run. Used by test module
 * @mnID: unique ID for identify Test Run
 * @mStressTestWorker: dalay worker for context to run
 * @mpFirstTestSet: Link to TestSet
 * @mpTestModule: the pointer to test module
 * @mbIsRunning: record test is running or not
 * @mnMaxIntervalBetweenRunningEveryTestSet: Max interval for running test sets
 * @mnMaxTestTimes: The max test times for this run. It counts a round if all test cases in all test set have been run.
 * @Init: init the member of this object.
 * @Deinit: deinit the member of this object. Destrory all memory allocated in init()
 * @Run: run the test run of this test set one by one. 
 * @stop: stop running.
 * @AddTestCase: Add one test set to this test run.
 * @GetID: Get the ID of this test run.
 * @SetMaxIntervalBetweenRunningEveryTestSet: Set the interval for running every test set.
 * @SetMaxTestTimes: Get the Test times.
 * @mSummary: for supporting Summary Interface
 * A i2c_test_run can have 0~n test sets.
 */
struct i2c_test_run {

	struct list_head	mTestRunList;

	unsigned mnID;
#ifdef TEST_RUN_IMPLEMENTED_BY_KTHREAD
	struct task_struct *mpkthread;
#else
	struct delayed_work mStressTestWorker;
#endif
	struct list_head	mTestSetListHead;
	struct i2c_test_set *mpCurrentTestSet;
	struct i2c_test_module *mpTestModule;

	bool mbIsRunning;
	
	
	//unsigned mnMaxIntervalBetweenRunningEveryTestSet;
	int mnMaxTestTimes;
	int mnCurrentTimes;

	int (*Init)(struct i2c_test_run *this, unsigned anID, struct i2c_test_module *apTestModule);
	int (*Deinit)(struct i2c_test_run *this);
	int (*Run)(struct i2c_test_run *this);
	int (*Stop)(struct i2c_test_run *this);
	int (*AddTestSet)(struct i2c_test_run *this,
			struct i2c_test_set *apTestSet);	
	unsigned (*GetID)(struct i2c_test_run *this);
	int (*SetMaxIntervalBetweenRunningEveryTestSet)(struct i2c_test_run *this, unsigned anMaxIntervalBetweenRunningEveryTestSet);
	int (*SetFixedIntervalMode)(struct i2c_test_run *this,
		bool abFixedIntervalMode);
	int (*SetMaxTestTimes)(struct i2c_test_run *this, int anMaxTestTimes);
	bool (*IsRunning)(struct i2c_test_run *this);
	int (*ShowStatus)(struct i2c_test_run *this, const char *buf,bool abShowTitle);

	struct i2c_test_summary mSummary;

	bool mbIsTestMode;

	struct i2c_test_interval_generator mIntervalGenerator;
};


/**
 * struct i2c_test_module - for i2c stress test module
 * @mpTestRunHead: the head pointing to  i2c_test_run
 * @mpAllTestSetHead: the head pointing to  i2c_test_set
 * @meLogLevel: Log level for test module
 *
 * A i2c_client can have 0~n test cases.
 * Restriction: One test test could just exist in at most one test run.
 */
struct i2c_test_module {

	unsigned long mTestRunIDBitArray;
	unsigned long mTestSetIDBitArray;
	struct list_head mTestRunListHead;
	struct list_head mAllTestSetListHead;
	AXE_I2C_LOG_LEVEL meLogLevel;

	bool mbInited;

	int (*Init)(struct i2c_test_module *this);
	int (*Deinit)(struct i2c_test_module *this);
	int (*ShowStatus)(struct i2c_test_module *this, const char *buf);
	int (*CreateTestRun)(struct i2c_test_module *this, unsigned *apTestRunID);
	int (*CreateN2NTestRun)(struct i2c_test_module *this);
	int (*AttachTestSetToTestRun)(struct i2c_test_module *this,
						unsigned anTestRunID,
						unsigned anTestSetID);
	int (*AttachAllNoOwnerTestSetsToTestRun)(struct i2c_test_module *this,
						unsigned anTestRunID);
	int (*RunTestRun)(struct i2c_test_module *this, unsigned anID);
	int (*RunAllTestRun)(struct i2c_test_module *this);
	int (*SetMaxIntervalBetweenRunningEveryTestSet)(struct i2c_test_module *this, unsigned anID, unsigned anMaxIntervalBetweenRunningEveryTestSet);
	int (*SetFixedIntervalMode)(struct i2c_test_module *this, unsigned anID, bool abFixedIntervalMode);
	int (*SetMaxTestTimes)(struct i2c_test_module *this, unsigned anID, int anMaxTestTimes);
	int (*SetAllMaxIntervalBetweenRunningEveryTestSet)(struct i2c_test_module *this, unsigned anMaxIntervalBetweenRunningEveryTestSet);
	int (*SetAllFixedIntervalMode)(struct i2c_test_module *this, bool abFixedIntervalMode);
	int (*SetAllMaxTestTimes)(struct i2c_test_module *this, int anMaxTestTimes);
	bool (*IsDuplicateClientAlreadyAdded)(struct i2c_test_module *this,
								struct i2c_client * apI2CClient);
	int (*SetLogLevel)(struct i2c_test_module *this, AXE_I2C_LOG_LEVEL leLogLevel);
	int (*ShowLastSummary)(struct i2c_test_module *this,unsigned anID);
	int (*ShowAllLastSummary)(struct i2c_test_module *this);
};
#endif //__AXI_I2C_STRESS_TEST_H__


