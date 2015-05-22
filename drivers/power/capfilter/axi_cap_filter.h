/*
       include file

*/
#ifndef __AXI_CAP_FILTER_H__
#define __AXI_CAP_FILTER_H__

struct AXI_Cap_Filter;

typedef struct AXI_Cap_Filter {
	int (*getType)(struct AXI_Cap_Filter *apCapFilter);
	int (*filterCapacity)(struct AXI_Cap_Filter *apCapFilter, int nowCap, int lastCap, bool hasCable, bool isCharing, bool isBatFull, bool isBatLow, int maxMah, int interval);
} AXI_Cap_Filter;

#endif //__AXI_CAP_FILTER_H__

