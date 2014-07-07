#include <linux/slab.h>
#include "axc_sampledatafactory.h"
#include "axc_intsampledata.h"
#include "../../axi_powerglobal.h"
//#include "../../asus_bat_dbg.h"
#include <linux/asus_bat.h>

void AXC_SampleDataFactory_GetSampleData(AXE_SAMPLE_DATA_TYPE aeSampleDataType , AXI_SampleData **appSampleData)
{
    pr_debug( "[BAT]AXC_SampleDataFactory_GetSampleData\n");
	if(NULL != *appSampleData){
		printk(DBGMSK_BAT_ERR "[BAT][SampleDataFactory]Memory leak...\n");
	}

	*appSampleData =  NULL;
	
	switch(aeSampleDataType){

		case E_INT_SAMPLE_DATA_TYPE:
			{
				 AXC_IntSampleData *lpIntSampleData = NULL;
				lpIntSampleData = kzalloc(sizeof( AXC_IntSampleData), GFP_KERNEL);
				assert(NULL != lpIntSampleData);		
				*appSampleData = &lpIntSampleData->msSampleData ;
				pr_debug( "[BAT]AXC_IntSampleDat init flag:%d\n",lpIntSampleData->mbInited);
				AXC_IntSampleData_Binding(*appSampleData,aeSampleDataType);
		
			}
		break;
		default:
			printk(DBGMSK_BAT_ERR "[BAT][GetSampleData]Not defined type...\n");
			break;

	}

	return;
}
void AXC_SampleDataFactory_FreeSampleData( AXI_SampleData *apSampleData)
{
	if(NULL != apSampleData){

		switch(apSampleData->GetType(apSampleData)){

			case E_INT_SAMPLE_DATA_TYPE:
				{
					 AXC_IntSampleData *lpIntSampleData = container_of(apSampleData,
						 AXC_IntSampleData,
						msSampleData);
					kfree(lpIntSampleData);
				}
				break;
			default:
				printk(DBGMSK_BAT_ERR "[BAT][FreeSampleData]Not defined type...\n");
				break;
		}

	}

}
