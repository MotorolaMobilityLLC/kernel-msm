/*********************************************************************
*
*   Copyright (C) 2012 Motorola, Inc.
*
**********************************************************************

File    : MemMapTempSensor.h
Purpose :
**********************************************************************/
#ifndef __MEMMAP_TEMPSENSOR_H__
#define __MEMMAP_TEMPSENSOR_H__
/****************************** Defines *******************************/
typedef struct memMapTemp {
	u8 version;
	u8 testCmd;
	s16 extrnlTemp;
	s16 intrnlTemp;
} sTempData;

/**************************** Globals ********************************/


/***************************** Prototypes *****************************/


#endif
