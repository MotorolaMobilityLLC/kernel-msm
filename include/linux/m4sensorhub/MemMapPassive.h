/*********************************************************************
*
*   Copyright (C) 2012 Motorola, Inc.
*
**********************************************************************

File    : MemMapPassive.h
Purpose :
**********************************************************************/
#ifndef __MEMMAP_PASSIVE_H__
#define __MEMMAP_PASSIVE_H__
/********************************** Defines **************************/

#define MAX_PASSIVE_BUFFERS 12

typedef struct memMapPassive {
	u32 timestamp[MAX_PASSIVE_BUFFERS];
	u32 steps[MAX_PASSIVE_BUFFERS];
	u32 mets[MAX_PASSIVE_BUFFERS];
	u32 floorsClimbed[MAX_PASSIVE_BUFFERS];
} sPassive;

/********************************** Globals ***************************/

/********************************** Prototypes ************************/
#endif
