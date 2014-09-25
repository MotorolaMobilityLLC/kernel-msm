/*********************************************************************
*
*   Copyright (C) 2012 Motorola, Inc.
*
**********************************************************************

File    : MemMapPressureSensor.h
Purpose :
**********************************************************************/
#ifndef __PRESS_SENSOR_H__
#define __PRESS_SENSOR_H__
/****************************** Defines *******************************/
typedef struct memMapPressure {
	u8 version;
	u8 dummy; /* Align the boundary */
	u16 sampleRate;
	u32 pressure;
	s32 referenceAltitude;
	u32 seaLevelPressure;
	s32 absoluteAltitude;
	s16 temperature;
} sPressureData;

/**************************** Globals ********************************/


/***************************** Prototypes *****************************/

#endif
