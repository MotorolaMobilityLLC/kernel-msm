/*                                                                                                                                                       
        SampleData Factory include file

*/
#ifndef __AXC_SAMPLEDATAFACTORY_H__
#define  __AXC_SAMPLEDATAFACTORY_H__
#include "axi_sampledata.h"

typedef enum {

	E_INT_SAMPLE_DATA_TYPE

}AXE_SAMPLE_DATA_TYPE;


extern void AXC_SampleDataFactory_GetSampleData(AXE_SAMPLE_DATA_TYPE aeSampleDataType , AXI_SampleData **appSampleData);
extern void AXC_SampleDataFactory_FreeSampleData( AXI_SampleData *apSampleData);
#endif // __AXC_SAMPLEDATAFACTORY_H__

