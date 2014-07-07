/*
	Do it later include file

*/
#ifndef __AXI_DOITLATER_H__
#define __AXI_DOITLATER_H__
#include <linux/types.h>
#define DOITLATER_DEBUG "[DoitLater]%s\n"

struct AXI_DoitLater;
struct AXI_DoitLaterTask;
    
typedef struct AXI_DoitLater{
    void(*start)(struct AXI_DoitLater* doitLater, struct AXI_DoitLaterTask *task,int delayTimeInMS, int times);// 0 means infinite....
    void(*stop)(struct AXI_DoitLater* doitLater);
}AXI_DoitLater;

typedef struct AXI_DoitLaterTask{
    void(*dotask)(struct AXI_DoitLaterTask *task);
}AXI_DoitLaterTask;

#endif //__AXI_DOITLATER_H__