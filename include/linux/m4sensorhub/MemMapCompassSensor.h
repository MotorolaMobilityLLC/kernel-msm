/*********************************************************************
*
*   Copyright (C) 2012 Motorola, Inc.
*
**********************************************************************

File    : MemMapCompassSensor.h
Purpose :
**********************************************************************/
#ifndef __MEMMAP_COMPASSSENSOR_H__
#define __MEMMAP_COMPASSSENSOR_H__
/****************************** Defines *******************************/
typedef struct memMapCompass {
	u8 version;
	u8 testCmd;
	u16 dummy; /* Align to 32-bit boundary */
	s32 x;
	s32 y;
	s32 z;
	s8 accuracy;
} sCompassData;

/**************************** Globals ********************************/


/***************************** Prototypes *****************************/



#endif /* __MEMMAP_COMPASSSENSOR_H__ */
