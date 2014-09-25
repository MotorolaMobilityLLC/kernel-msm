/*********************************************************************
*
*   Copyright (C) 2012 Motorola, Inc.
*
**********************************************************************

File    : MemMapAudio.h
Purpose :
**********************************************************************/
#ifndef __MEMMAP_AUDIO_H__
#define __MEMMAP_AUDIO_H__
/****************************** Defines *******************************/
typedef struct memMapAudio {
	u8 version;
	u8 enable;
	u16 dummy;
	u32 totalPackets;
} sAudioData;

#define AUDIO_BUFFER_SIZE 800
/**************************** Globals ********************************/


/***************************** Prototypes *****************************/



#endif /* __MEMMAP_AUDIO_H__ */

