/*
	Do it later include file

*/
#ifndef __AXC_DOITLATER_FACTORY_H__
#define __AXC_DOITLATER_FACTORY_H__
#include <linux/types.h>
#include "AXI_DoitLater.h"

typedef enum{
    E_ONE_SHOT_WORKER_TYPE,
}AXE_DoitLater_Type;

//provide one shot Do it later interface...
extern AXI_DoitLater* get_DoitLater(AXE_DoitLater_Type type);

#endif //__AXC_DOITLATER_FACTORY_H__