/*
	Selftest include file

*/
#ifndef __AXI_SELFTEST_H__
#define __AXI_SELFTEST_H__
#include <linux/types.h>
#define assert(expr) \
	if (!(expr)) {					\
		printk( "Assertion failed! %s,%s,%s,line=%d\n",	\
		#expr,__FILE__,__func__,__LINE__);		\
	}

typedef struct AXI_SelfTest {
	bool (*Test)(void *apTestObject);
}AXI_SelfTest;

#endif //__AXI_SELFTEST_H__
