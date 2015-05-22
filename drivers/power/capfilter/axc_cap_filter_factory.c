#include <linux/kernel.h>
#include <linux/slab.h>

#include "axc_cap_filter_factory.h"
#include "axc_cap_filter_a66.h"
#include "axc_cap_filter_p02.h"
#include "axc_cap_filter_dock.h"


extern void AXC_Cap_Filter_Get(AXE_CAP_FILTER_TYPE filterType , AXI_Cap_Filter **apCapFilter, int capMah)
{
	if (NULL != *apCapFilter) {
		pr_err("[BAT][Fil]Memory leak...\n");
		return;
	}

	*apCapFilter =  NULL;
	switch(filterType)
	{
		case E_CAP_FILTER_PHONE_A66:
		{
		AXC_Cap_Filter_A66 *lpCapFilterA66 = NULL;
		lpCapFilterA66 = kzalloc(sizeof(AXC_Cap_Filter_A66), GFP_KERNEL);
		BUG_ON(NULL == lpCapFilterA66);
		*apCapFilter = &lpCapFilterA66->parentCapFilter;
		AXC_Cap_Filter_A66_Constructor(*apCapFilter, filterType, capMah);
		}
		break;
		default:
		pr_err("[BAT][Fil]Error!!%s(), Not defined type...\n", __func__);
		break;
	}

	return;
}

extern void AXC_Cap_Filter_Free(AXI_Cap_Filter *apCapFilter)
{
 	if(NULL != apCapFilter)
 	{
		switch(apCapFilter->getType(apCapFilter))
		{
			case E_CAP_FILTER_PHONE_A66:
			{
			AXC_Cap_Filter_A66 *this = container_of(apCapFilter, AXC_Cap_Filter_A66, parentCapFilter);
			kfree(this);
			}
			break;
			default:
			pr_err("[BAT][Fil]Error!!%s(), Not defined type...\n", __func__);
			break;
		}
	}

	return;
}
