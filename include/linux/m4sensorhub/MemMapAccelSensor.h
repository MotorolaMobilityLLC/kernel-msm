/*********************************************************************
*
*   Copyright (C) 2012 Motorola, Inc.
*
**********************************************************************

File    : MemMapAccelSensor.h
Purpose :
**********************************************************************/
#ifndef __MEMMAP_ACCELSENSOR_H__
#define __MEMMAP_ACCELSENSOR_H__
/****************************** Defines *******************************/
typedef struct memMapAccel {
	u8 version;
	u8 testCmd;
	u16 dummy; /* Align to 32-bit boundary */
	s32 x;
	s32 y;
	s32 z;
} sAccelData;

/**************************** Globals ********************************/


/***************************** Prototypes *****************************/



#endif /* __MEMMAP_ACCELSENSOR_H__ */
