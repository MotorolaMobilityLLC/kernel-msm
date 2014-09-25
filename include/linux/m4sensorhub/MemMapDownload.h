/*********************************************************************
*
*   Copyright (C) 2012 Motorola, Inc.
*
**********************************************************************
File    : MemMapDownload.h
Purpose :
**********************************************************************/
#ifndef __MEMMAP_DOWNLOAD_H__
#define __MEMMAP_DOWNLOAD_H__
/****************************** Defines *******************************/

/* These enums and defines need to match up with the enums
 * in m4sensorhub_client_ioctl.h
 */

#define M4SH_DL_FILENAME_SIZE 16
#define M4SH_DL_PACKET_SIZE 100

enum download_error_codes {
  DOWNLOAD_SUCCESS,
  DOWNLOAD_ERROR_GET_CHECKSUM,
  DOWNLOAD_ERROR_OPEN_FILE,
  DOWNLOAD_ERROR_WRITE_FILE,
  DOWNLOAD_ERROR_CLOSE_FILE,
  DOWNLOAD_ERROR_DELETE_FILE,
  DOWNLOAD_ERROR_INVALID_SIZE,
  /*internal error code for M4<==>Kernel*/
  DOWNLOAD_ERROR_SEND_CMD = 0x80,
  DOWNLOAD_ERROR_DATA_CHECKSUM,
};

enum downloadCmds {
  DOWNLOAD_CMD_GET_CHECKSUM,
  DOWNLOAD_CMD_OPEN_FILE,
  DOWNLOAD_CMD_WRITE_FILE,
  DOWNLOAD_CMD_CLOSE_FILE,
  DOWNLOAD_CMD_DELETE_FILE,
};

typedef struct memMapDownload {
  u8 version;
  u8 command;
  u8 status;
  u8 size;
  u32 checksum;
  u8 filename[M4SH_DL_FILENAME_SIZE];
  u8 packet[M4SH_DL_PACKET_SIZE];
} sDownload;

#endif /*__MEMMAP_DOWNLOAD_H__*/
