/*********************************************************************
*
*   Copyright (C) 2012 Motorola, Inc.
*
**********************************************************************

File    : MemMapUserSettings.h
Purpose :
**********************************************************************/
#ifndef __MEMMAP_USERSETTING_H__
#define __MEMMAP_USERSETTING_H__
/****************************** Defines *******************************/
typedef struct memMapUserSettings
{
  u8 screenStatus;
  u8 userAge;
  u8 userGender;
  u8 userHeight;
  u16 userWeight;
  u8 audioStatus;
  u8 rtcReset;
}sUserData;

typedef enum
{
  FEMALE = 0,
  MALE = 1
}eGender;

typedef enum
{
  SCREEN_STATUS_NORMAL_OFF  = 0,
  SCREEN_STATUS_NORMAL_ON   = 1,
  SCREEN_STATUS_AMBIENT_OFF = 2,
  SCREEN_STATUS_AMBIENT_ON  = 3
}eScreenStatus;

typedef enum
{
  AUDIO_STATUS_OFF = 0, // was SCREEN_OFF
  AUDIO_STATUS_ON  = 1, // was SCREEN_ON
}eAudioStatus;

typedef enum
{
  USERSETTINGS_SCREENSTATUS = 0,
  USERSETTINGS_USERAGE = 1,
  USERSETTINGS_USERGENDER = 2,
  USERSETTINGS_USERHEIGHT = 3,
  USERSETTINGS_USERWEIGHT = 4,
  USERSETTINGS_AUDIOSTATUS = 6,
  USERSETTINGS_RTCRESET = 7,
}eUserSettingsOffset;

#endif // __MEMMAP_USERSETTING_H__
