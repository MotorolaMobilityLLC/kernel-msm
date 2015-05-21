/*
       include file

*/
#ifndef __AXC_CAP_FILTER_FACTORY_H__
#define __AXC_CAP_FILTER_FACTORY_H__
#include "axi_cap_filter.h"

typedef enum {
        E_CAP_FILTER_PHONE_A66,
        E_CAP_FILTER_PAD_P02,
        E_CAP_FILTER_DOCK,
        E_CAP_FILTER_UNKNOWN
} AXE_CAP_FILTER_TYPE;                                                                                                                                     


extern void AXC_Cap_Filter_Get(AXE_CAP_FILTER_TYPE filterType , AXI_Cap_Filter **capFilter, int capMah);
extern void AXC_Cap_Filter_Free(AXI_Cap_Filter *capFilter);


#endif // __AXC_CAP_FILTER_FACTORY_H__

