/*********************************************************************
*
*   Copyright (C) 2012 Motorola, Inc.
*
**********************************************************************

File    : MemMapFusionSensor.h
Purpose :
**********************************************************************/
#ifndef __MEMMAP_FUSIONSENSOR_H__
#define __MEMMAP_FUSIONSENSOR_H__
/****************************** Defines *******************************/
typedef struct memMapFusion {
	u8 version;
	u8 testCmd;
	u16 dummy;
	s32 eulerPitch;
	s32 eulerRoll;
	s32 eulerYaw;
	s32 localX;
	s32 localY;
	s32 localZ;
	s32 worldX;
	s32 worldY;
	s32 worldZ;
	s16 heading;
	s8 heading_accuracy;
} sFusionData;

/**************************** Globals ********************************/


/***************************** Prototypes *****************************/



#endif /* __MEMMAP_FUSIONSENSOR_H__ */
