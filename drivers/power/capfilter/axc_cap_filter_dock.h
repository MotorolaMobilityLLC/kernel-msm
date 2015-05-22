/*                                                                                                                                                       
	include file

*/
#ifndef __AXC_CAP_FILTER_DOCK_H__
#define __AXC_CAP_FILTER_DOCK_H__

#include "axi_cap_filter.h"

typedef struct AXC_Cap_Filter_Dock {
	int capMah;
	int filterType;
	struct AXI_Cap_Filter parentCapFilter;
} AXC_Cap_Filter_Dock;


extern void AXC_Cap_Filter_Dock_Constructor(AXI_Cap_Filter *apCapFilter, int filterType, int capMah);


#endif //__AXC_CAP_FILTER_DOCK_H__

