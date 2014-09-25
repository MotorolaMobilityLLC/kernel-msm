/*********************************************************************
*
*   Copyright (C) 2012 Motorola, Inc.
*
**********************************************************************

File    : MemMapGesture.h
Purpose :
**********************************************************************/
#ifndef __MEMMAP_GESTURE_H__
#define __MEMMAP_GESTURE_H__
/********************************** Defines **************************/
typedef struct memMapGesture {
  u8 version;
  u8 gesture1;
  u8 confidence1;
  s8 value1;
  u8 gesture2;
  u8 confidence2;
  s8 value2;
  u8 gesture3;
  u8 confidence3;
  s8 value3;
} sGestureData;

typedef enum  {
  GESTURE_NONE,
  GESTURE_VIEW,
  GESTURE_WRIST_ROTATE,
  GESTURE_TAP,
  GESTURE_HANDSHAKE,
  GESTURE_HANDWAVE,
  GESTURE_FISTBUMP,
  GESTURE_WATCH_ON,
  GESTURE_WATCH_OFF,
  GESTURE_TILT_SCROLL,
  GESTURE_MAX
} eGestureType;

typedef enum  {
  GESTURE_VIEW_ON,
  GESTURE_VIEW_OFF
} eGestureViewValue;

typedef enum  {
  GESTURE_WRIST_ROTATE_NORMAL,
  GESTURE_WRIST_ROTATE_FLAT,
  GESTURE_WRIST_ROTATE_BED,
  GESTURE_WRIST_ROTATE_GENT,
  GESTURE_WRIST_ROTATE_INSIDE,
  GESTURE_WRIST_ROTATE_FAST,
} eGestureWristRotateValue;

/********************************** Globals **************************/

/********************************** Prototypes ***********************/



#endif /* __MEMMAP_GESTURE_H__ */
