/*
	Int Sample Data Implementation
*/
#include <linux/kernel.h>
#include "axc_intsampledata.h"
#include <linux/sort.h>
//#include "../../asus_bat_dbg.h"
#include <linux/asus_bat.h>

static void AXC_IntSampleData_Init(AXI_SampleData *apSampleData);
static void AXC_IntSampleData_DeInit(AXI_SampleData *apSampleData);
static void AXC_IntSampleData_Reset(AXI_SampleData *apSampleData);
static int  AXC_IntSampleData_GetAverage(AXI_SampleData *apSampleData);
static void AXC_IntSampleData_AddSample(AXI_SampleData *apSampleData, void *apData);
static bool AXC_IntSampleData_Test(void *apTestObject);


static void AXC_IntSampleData_Init(AXI_SampleData *apSampleData)
{
	AXC_IntSampleData *this = container_of(apSampleData, AXC_IntSampleData, msSampleData);

	pr_debug( "[BAT]AXC_IntSampleData_Init++:%d\n",this->mbInited);
	if(false == this->mbInited)
    {
		this->msSampleData.Reset(apSampleData);
		this->mbInited = true;
	}
}

static void AXC_IntSampleData_DeInit(AXI_SampleData *apSampleData)
{
	AXC_IntSampleData *this = container_of(apSampleData, AXC_IntSampleData, msSampleData);

	if(true == this->mbInited)
    {
		this->msSampleData.Reset(apSampleData);
		this->mbInited = false;
	}
}

int AXC_IntSampleData_GetType(AXI_SampleData *apSampleData)
{
	AXC_IntSampleData *this = container_of(apSampleData, AXC_IntSampleData, msSampleData);
	return this->mnType;
}

void AXC_IntSampleData_SetType(AXI_SampleData *apSampleData ,int anType)
{
	AXC_IntSampleData *this = container_of(apSampleData, AXC_IntSampleData, msSampleData);
	this->mnType = anType;
}

static void AXC_IntSampleData_Reset(AXI_SampleData *apSampleData)
{
	AXC_IntSampleData *this = container_of(apSampleData, AXC_IntSampleData, msSampleData);
	int i = 0;
	
    pr_debug( "[BAT]AXC_IntSampleData_Reset++\n");

#ifdef SUPPORT_MODE		
	for( i = 0;i<MAX_SIZE_SAMPLE_DATA;i++)
		this->mDataArray[i] = 0;
#else
	this->mnSum = 0;
	this->mnMax = 0;
	this->mnMin = 0;
#endif
	this->mnCount = 0;

    pr_debug( "[BAT]AXC_IntSampleData_Reset--\n");
}

#ifdef SUPPORT_MODE	
static int CompareData(const void *a, const void *b)
{
	return *(int *)a - *(int *)b;
}

static void SwapData(void *a, void *b, int size)
{
	int t = *(int *)a;
	*(int *)a = *(int *)b;
	*(int *)b = t;
}
#endif

static int AXC_IntSampleData_GetAverage(AXI_SampleData *apSampleData)
{
	AXC_IntSampleData *this = container_of(apSampleData, AXC_IntSampleData, msSampleData);
    int lnAvg = 0 ;

    pr_debug( "[BAT]AXC_IntSampleData_GetAverage++\n");

    #ifdef SUPPORT_MODE	
    {
	    int i = 0;
    	int lnMaxCount = 0;
	    int lnTempMaxCount = 1;    //this->mDataArray[0]
    	int lnSizeForAverage = 0;
	    int lnSumForAverage = 0;

    	//To Prevent the case of this->mnCount == 0
	    //assert(this->mnCount > 0);

	    if(0 == this->mnCount)
        {
    		//panic("0 == this->mnCount");
    		pr_debug( "[BAT] in AXC_IntSampleData_GetAverage(), 0 == this->mnCount\n");
    		return 0;
	    }
        else if (1 == this->mnCount)
        {
            //Speed up the case:this->mnCount==1
    		return this->mDataArray[0];
	    }

	    sort(this->mDataArray, this->mnCount, sizeof(int), CompareData, SwapData);

	    for(i =1; i< this->mnCount; i++)
        {
    		if(this->mDataArray[i-1] == this->mDataArray[i])
    			lnTempMaxCount ++;
		    else
            {
    			if(lnTempMaxCount > lnMaxCount)
                	lnMaxCount = lnTempMaxCount;
    			lnTempMaxCount = 1;//this->mDataArray[i]
	    	}
	    }

	    if(lnTempMaxCount > lnMaxCount)
			lnMaxCount = lnTempMaxCount;
    
	    for(i =1,lnTempMaxCount=1; i< this->mnCount; i++)
        {
    		if(this->mDataArray[i-1] == this->mDataArray[i])
    			lnTempMaxCount ++;
			else
            {
		    	if(lnTempMaxCount >= lnMaxCount - CALCULATE_RANGE_FOR_AVERAGE)
                {
    				lnSumForAverage += this->mDataArray[i-1]*lnTempMaxCount;
    				lnSizeForAverage +=lnTempMaxCount;
				}

				lnTempMaxCount = 1;//this->mDataArray[i]
	    	}
	    }   

    	if(lnTempMaxCount >= lnMaxCount - CALCULATE_RANGE_FOR_AVERAGE)
        {
			lnSumForAverage += this->mDataArray[i-1]*lnTempMaxCount;
			lnSizeForAverage +=lnTempMaxCount;
		}

    	//To Prevent the case of this->mnCount == 0
    	assert(lnSizeForAverage != 0);
    	if(lnSizeForAverage != 0)
    		lnAvg = lnSumForAverage / lnSizeForAverage;
		else
    		lnAvg = 0;
    }
    #else
	if( this->mnCount > 2 )
		lnAvg = ( this->mnSum - this->mnMax - this->mnMin )/(this->mnCount - 2);
    else if(0 == this->mnCount)
    {
        // Avoid to div 0
    	return 0;
	}
    else
		lnAvg = this->mnSum / this->mnCount;
    #endif

    pr_debug( "[BAT]AXC_IntSampleData_GetAverage:%d --\n", lnAvg);
    return lnAvg;
}

static void AXC_IntSampleData_AddSample(AXI_SampleData *apSampleData, void *apData)
{
	AXC_IntSampleData *this = container_of(apSampleData, AXC_IntSampleData, msSampleData);
	int *apInt = (int *)apData;

    #ifdef SUPPORT_MODE
	{
		int i;
		int j;

		for(i = 0-RANGE_SIZE_FOR_WEIGHTED; i < RANGE_SIZE_FOR_WEIGHTED+1; i++)
        {
			if((*apInt+i) > 0)
            {
				if(this->mnCount < MAX_SIZE_SAMPLE_DATA)
                {
					if(0 == i)
                    {
                        //exact value
						for(j = 0; j < WEIGHTED_FOR_EXACT_VALUE; j++)
							this->mDataArray[this->mnCount++] = (*apInt);	
					}
                    else
						this->mDataArray[this->mnCount++] = (*apInt +i);
				}
                else
                {
					pr_debug( "[BAT][IntSampleDataTest]Over sample data size\n");
					break;
				}
			}
		}
	}
    #else
    this->mnSum += *apInt;
	this->mnCount++;

   	if( 0 == this->mnMax ) this->mnMax = *apInt;
   	if( 0 == this->mnMin ) this->mnMin = *apInt;
   	this->mnMax = max( *apInt , this->mnMax );
	this->mnMin = min( *apInt , this->mnMin );
    #endif
}

static bool AXC_IntSampleData_Test(void *apTestObject)
{
	AXI_SampleData *lpSampleData = (AXI_SampleData *)apTestObject;
	int i;

	lpSampleData->Init(lpSampleData);
	pr_debug( "[BAT][IntSampleDataTest]InitTest...avg=%d\n",lpSampleData->GetAverage(lpSampleData));

	assert(0 == lpSampleData->GetAverage(lpSampleData));
	//Add 0~11, the sum will be 55(1 to 10)
	for(i = 0; i < 12; i++)
		lpSampleData->AddSample(lpSampleData,(void *)&i);
	
	pr_debug( "[BAT][IntSampleDataTest]AccumlateTest...avg=%d\n",lpSampleData->GetAverage(lpSampleData));
	assert((55/10) == lpSampleData->GetAverage(lpSampleData));
	pr_debug( "[BAT][IntSampleDataTest]ResetTest...avg=%d\n",lpSampleData->GetAverage(lpSampleData));
	lpSampleData->Reset(lpSampleData);
	assert(0 == lpSampleData->GetAverage(lpSampleData));

	return 0;
}

bool AXC_IntSampleData_Test1(void *apTestObject)
{
	return AXC_IntSampleData_Test(apTestObject);
}

void AXC_IntSampleData_Binding(AXI_SampleData *apSampleData,int anType)
{

AXC_IntSampleData *this = container_of(apSampleData, AXC_IntSampleData, msSampleData);

    ///Eason_Chang///pr_debug( "[BAT]AXC_IntSampleData_Binding++\n");

    this->msSampleData.Init = AXC_IntSampleData_Init;
    this->msSampleData.DeInit = AXC_IntSampleData_DeInit;
	this->msSampleData.GetType = AXC_IntSampleData_GetType;
	this->msSampleData.SetType = AXC_IntSampleData_SetType;
    this->msSampleData.Reset = AXC_IntSampleData_Reset;
    this->msSampleData.GetAverage = AXC_IntSampleData_GetAverage;
    this->msSampleData.AddSample = AXC_IntSampleData_AddSample;

	this->msSampleData.SetType(apSampleData,anType);
	//this->mbInited = false;
#ifdef INT_SAMPLE_DATA_SELF_TEST_ENABLE
	this->msSelfTest.Test = AXC_IntSampleData_Test;
#endif
    ///Eason_Chang///pr_debug( "[BAT]AXC_IntSampleData_Binding--\n");
}    
