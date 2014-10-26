/*                                                                                                                                                       
        Sample Data include file

*/
#ifndef __AXI_SAMPLEDATA_H__
#define __AXI_SAMPLEDATA_H__ 
#include <linux/types.h>


typedef struct AXI_SampleData {
        void (*Init)(struct AXI_SampleData *apSampleData);
        void (*DeInit)(struct AXI_SampleData *apSampleData);
	int (*GetType)(struct AXI_SampleData *apSampleData);
	void  (*SetType)(struct AXI_SampleData *apSampleData,int anType);
	void (*Reset)(struct AXI_SampleData *apSampleData);
	int (*GetAverage)(struct AXI_SampleData *apSampleData);
	void (*AddSample)(struct AXI_SampleData *apSampleData, void *apData);
}AXI_SampleData;

#endif //__AXI_SAMPLEDATA_H__ 

