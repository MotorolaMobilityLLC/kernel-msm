/*
	Test include file

*/
#ifndef __AXI_TEST_H__
#define __AXI_TEST_H__
#include <linux/types.h>
#define assert(expr) \
	if (!(expr)) {					\
		printk( "Assertion failed! %s,%s,%s,line=%d\n",	\
		#expr,__FILE__,__func__,__LINE__);		\
	}

struct AXI_Test;
struct AXI_TestReport;

typedef struct AXI_Test {
	void (*test)(struct AXI_Test *test,struct AXI_TestReport* report);
}AXI_Test;

typedef struct AXI_TestReport {
    void (*report)(struct AXI_TestReport *report, bool result);
}AXI_TestReport;

#endif //__AXI_TEST_H__
