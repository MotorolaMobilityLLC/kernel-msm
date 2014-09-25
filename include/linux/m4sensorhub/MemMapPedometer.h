/*********************************************************************
*
*   Copyright (C) 2012 Motorola, Inc.
*
**********************************************************************

File    : Pedometer.h
Purpose :
**********************************************************************/
#ifndef __PEDOMETER_H__
#define __PEDOMETER_H__
/****************************** Defines *******************************/
typedef struct memMapPedometer {
	u8 version;
	u8 testCmd;
	u16 totalSteps;
	u32 totatDistance;
	u32 currentSpeed;
	u8  activity;
} sPedoData;

typedef enum {
  WALK,
  JOG,
  RUN
} eActivity;
/**************************** Globals ********************************/


/***************************** Prototypes *****************************/



#endif /* __PEDOMETER_H__ */
