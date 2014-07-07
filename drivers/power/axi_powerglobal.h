/*
	Selftest include file

*/
#ifndef __AXI_POWERGLOBAL_H__
#define __AXI_POWERGLOBAL_H__
#include <linux/types.h>
#define assert(expr) \
	if (!(expr)) {					\
		printk( "Assertion failed! %s,%s,%s,line=%d\n",	\
		#expr,__FILE__,__func__,__LINE__);		\
	}

#endif //__AXI_POWERGLOBAL_H__
