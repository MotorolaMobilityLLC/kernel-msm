/*********************************************************************
*  
*   Copyright (C) 2012 Motorola, Inc.
*
**********************************************************************

File    : MemMapPower.h
Purpose : 
**********************************************************************/
#ifndef __MEMMAP_POWER_H__
#define __MEMMAP_POWER_H__
/****************************** Defines *******************************/
typedef struct memMapPower
{
        u8 version; 
        u8 enable; 
        u16 stillModeTimeout; // in secs 
        u8 motionDuration; // in secs 
        u8 motionThreshold; // in milliG 
        u8 noMotionDuration;// in secs 
        u8 noMotionThreshold; // in milliG 
        u8 deviceState; //This indicates if the device is in motion(1) or is still(0)        
}sPower;

typedef enum {
  PM_STILL,
  PM_MOTION
}eDeviceState;
#endif // __MEMMAP_POWER_H__
