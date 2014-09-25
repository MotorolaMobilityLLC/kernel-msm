/*********************************************************************
*
*   Copyright (C) 2012 Motorola, Inc.
*
**********************************************************************

File    : MemMapGyroSensor.h
Purpose :
**********************************************************************/
#ifndef __MEMMAP_GYROSENSOR_H__
#define __MEMMAP_GYROSENSOR_H__
/****************************** Defines *******************************/
typedef struct memMapGyro {
	u8 version;
	u8 testCmd;
	u16 dummy; /* Align to 32-bit boundary */
	s32 x;
	s32 y;
	s32 z;
} sGyroData;

/**************************** Globals ********************************/


/***************************** Prototypes *****************************/



#endif /* __MEMMAP_GYROSENSOR_H__*/
