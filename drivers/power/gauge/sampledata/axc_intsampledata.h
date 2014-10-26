/*                                                                                                                                                       
        Int Sample Data include file

*/
#ifndef __AXI_INTSAMPLEDATA_H__
#define __AXI_INTSAMPLEDATA_H__ 
#include <linux/types.h>
#include "axi_sampledata.h"                                                                                                                             

#define INT_SAMPLE_DATA_SELF_TEST_ENABLE

#ifdef INT_SAMPLE_DATA_SELF_TEST_ENABLE

#include "../../selftest/axi_selftest.h"

#endif

#define SUPPORT_MODE

#ifdef SUPPORT_MODE

#define MAX_SIZE_SAMPLE_DATA 350
#define RANGE_SIZE_FOR_WEIGHTED 1
#define WEIGHTED_FOR_EXACT_VALUE 1
#define CALCULATE_RANGE_FOR_AVERAGE 2

#endif


typedef struct AXC_IntSampleData {
	bool mbInited;
	int mnType;
	int mnSum;
	int mnMax;
	int mnMin;
	int mnCount;
#ifdef SUPPORT_MODE
	int mDataArray[MAX_SIZE_SAMPLE_DATA];
#endif

	struct AXI_SampleData msSampleData;

#ifdef INT_SAMPLE_DATA_SELF_TEST_ENABLE

	struct AXI_SelfTest msSelfTest;	

#endif
}AXC_IntSampleData;
extern void AXC_IntSampleData_Binding(AXI_SampleData *apSampleData,int anType);
#endif //__AXI_INTSAMPLEDATA_H_


