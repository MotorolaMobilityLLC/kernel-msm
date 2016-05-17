/*
 *  shub_io.h - Linux kernel modules for interface of Sensor Hub 
 *
 *  Copyright (C) 2014 LAPIS SEMICONDUCTOR CO., LTD.
 *
 *  This file is based on :
 *    alps_io.h - Linux kernel modules for interface of acceleration and magnetic field sensor
 *    http://android-dev.kyocera.co.jp/source/versionSelect_URBANO.html
 *    (C) 2012 KYOCERA Corporation
 *    Copyright (C) 2010 ALPS
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef ___SHUB_IO_H_INCLUDED
#define ___SHUB_IO_H_INCLUDED

#ifdef NO_LINUX
typedef int bool;
#include <stdint.h>
#define true 1
#define false 0
#else
#include <linux/ioctl.h>
#include <stdbool.h>
#endif

typedef struct {
  uint8_t m_flag;
  uint8_t m_cData[4];
}IoCtlAccSetFirmwareVersion;

typedef struct {
  union {
    uint8_t *m_cData;
    uint32_t m_cDataAddr;
    uint64_t m_cDataDummy;
  } u;

  uint32_t m_sCmdErrRes;
  uint32_t m_nLen;
  bool     m_bBoot;
}IoCtlAccSetFirmwareData;

typedef struct {
  int32_t m_iOffset[6];
}IoCtlSetOffset;

typedef struct {
  uint16_t m_sCmd;
  uint16_t m_sErr;
  uint8_t  m_cParam[16];
  uint8_t  m_cResult[512];
}IoCtlHostCmd;

typedef struct {
  int32_t  m_iType;
  int32_t  m_iParam[32];
}IoCtlParam;

typedef struct {
  int32_t  m_iData[64];
}IoCtlData;

typedef struct {
  int m_Flasg;
  int64_t m_PeriodNs;
  int64_t m_Latency;
}IoCtlBatchInfo;

typedef struct {
  uint8_t  m_ucSAddr;
  uint8_t  m_ucAddr;
  uint8_t  m_ucData[8];
  uint8_t  m_ucLen;
}IoCtlAccessI2c;

typedef struct {
  uint8_t  m_sSensorType;
  uint8_t  m_ucAddr;
  uint8_t  m_ucData;
}IoCtlAccessSensor;


#define SHUBIO   0xAF

#define SHUBIO_INITIALIZE           _IOW(SHUBIO , 1 , int)
#define SHUBIO_MCU_HOSTCMD          _IOW(SHUBIO , 2 , int)
#define SHUBIO_MCU_FW_UPDATE        _IOW(SHUBIO , 3 , int)
#define SHUBIO_MCU_FW_GET_VERSION   _IOW(SHUBIO , 4 , int)
#define SHUBIO_MCU_SET_PARAM        _IOW(SHUBIO , 5 , int)
#define SHUBIO_MCU_GET_PARAM        _IOW(SHUBIO , 6 , int)
#define SHUBIO_MCU_WRITE_SENSOR     _IOW(SHUBIO , 7 , int)
#define SHUBIO_MCU_READ_SENSOR      _IOW(SHUBIO , 8 , int)

#define SHUBIO_MCU_GET_PEDOM_DATA   _IOW(SHUBIO , 10 , int)
#define SHUBIO_MCU_CLR_DATA         _IOW(SHUBIO , 11 , int)
#define SHUBIO_MCU_GET_ADTECT_DATA  _IOW(SHUBIO , 12 , int)
#define SHUBIO_MCU_GET_MOTION_DATA  _IOW(SHUBIO , 13 , int)
#define SHUBIO_MCU_GET_NORMAL_PEDOM_DATA _IOW(SHUBIO , 14 , int)
#define SHUBIO_MCU_GET_LOW_POWER_DATA    _IOW(SHUBIO , 15 , int)
#define SHUBIO_MCU_INIT_APP         _IOW(SHUBIO , 16 , int)

#define SHUBIO_SET_ACC_ACTIVATE     _IOW(SHUBIO , 20 , int)
#define SHUBIO_SET_DELAY            _IOW(SHUBIO , 21 , int)
#define SHUBIO_ACC_SET_BATCH        _IOW(SHUBIO , 22 , int)
#define SHUBIO_ACC_FLUSH            _IOW(SHUBIO , 23 , int)
#define SHUBIO_ACC_SET_OFFSET       _IOW(SHUBIO , 24 , int)
#define SHUBIO_ACC_GET_OFFSET       _IOW(SHUBIO , 25 , int)
#define SHUBIO_ACC_SET_POSITION     _IOW(SHUBIO , 26 , int)

#define SHUBIO_GYRO_ACTIVATE        _IOW(SHUBIO , 30 , int)
#define SHUBIO_GYRO_SET_FREQ        _IOW(SHUBIO , 31 , int)
#define SHUBIO_GYRO_SET_BATCH       _IOW(SHUBIO , 32 , int)
#define SHUBIO_GYRO_FLUSH           _IOW(SHUBIO , 33 , int)
#define SHUBIO_GYRO_SET_OFFSET      _IOW(SHUBIO , 34 , int)
#define SHUBIO_GYRO_GET_OFFSET      _IOW(SHUBIO , 35 , int)
#define SHUBIO_GYRO_SET_POSITION    _IOW(SHUBIO , 36 , int)

#define SHUBIO_MAG_ACTIVATE         _IOW(SHUBIO , 40 , int)
#define SHUBIO_MAG_SET_BATCH        _IOW(SHUBIO , 41 , int)
#define SHUBIO_MAG_SET_FREQ         _IOW(SHUBIO , 42 , int)
#define SHUBIO_MAG_FLUSH            _IOW(SHUBIO , 43 , int)
#define SHUBIO_MAG_SET_OFFSET       _IOW(SHUBIO , 44 , int)
#define SHUBIO_MAG_GET_OFFSET       _IOW(SHUBIO , 45 , int)
#define SHUBIO_MAG_SET_POSITION     _IOW(SHUBIO , 46 , int)
#define SHUBIO_MAG_CALIBRATE_AXIS_ITNERFERANCE  _IOW(SHUBIO , 47 , int)

#define SHUBIO_GAMERV_ACTIVATE      _IOW(SHUBIO , 50 , int)
#define SHUBIO_GAMERV_SET_FREQ      _IOW(SHUBIO , 51 , int)
#define SHUBIO_GAMERV_SET_BATCH     _IOW(SHUBIO , 52 , int)
#define SHUBIO_GAMERV_FLUSH         _IOW(SHUBIO , 53 , int)

#define SHUBIO_GEORV_ACTIVATE       _IOW(SHUBIO , 60 , int)
#define SHUBIO_GEORV_SET_FREQ       _IOW(SHUBIO , 61 , int)
#define SHUBIO_GEORV_SET_BATCH      _IOW(SHUBIO , 62 , int)
#define SHUBIO_GEORV_FLUSH          _IOW(SHUBIO , 63 , int)

#define SHUBIO_GRAVITY_ACTIVATE     _IOW(SHUBIO , 70 , int)
#define SHUBIO_GRAVITY_SET_FREQ     _IOW(SHUBIO , 71 , int)
#define SHUBIO_GRAVITY_SET_BATCH    _IOW(SHUBIO , 72 , int)
#define SHUBIO_GRAVITY_FLUSH        _IOW(SHUBIO , 73 , int)

#define SHUBIO_GYROUNCAL_ACTIVATE   _IOW(SHUBIO , 80 , int)
#define SHUBIO_GYROUNCAL_SET_FREQ   _IOW(SHUBIO , 81 , int)
#define SHUBIO_GYROUNCAL_SET_BATCH  _IOW(SHUBIO , 82 , int)
#define SHUBIO_GYROUNCAL_FLUSH      _IOW(SHUBIO , 83 , int)

#define SHUBIO_LINEARACC_ACTIVATE   _IOW(SHUBIO , 90 , int)
#define SHUBIO_LINEARACC_SET_FREQ   _IOW(SHUBIO , 91 , int)
#define SHUBIO_LINEARACC_SET_BATCH  _IOW(SHUBIO , 92 , int)
#define SHUBIO_LINEARACC_FLUSH      _IOW(SHUBIO , 93 , int)

#define SHUBIO_MAGUNCAL_ACTIVATE    _IOW(SHUBIO , 100 , int)
#define SHUBIO_MAGUNCAL_SET_FREQ    _IOW(SHUBIO , 101 , int)
#define SHUBIO_MAGUNCAL_SET_BATCH   _IOW(SHUBIO , 102 , int)
#define SHUBIO_MAGUNCAL_FLUSH       _IOW(SHUBIO , 103 , int)

#define SHUBIO_ORI_ACTIVATE         _IOW(SHUBIO , 110 , int)
#define SHUBIO_ORI_SET_FREQ         _IOW(SHUBIO , 111 , int)
#define SHUBIO_ORI_SET_BATCH        _IOW(SHUBIO , 112 , int)
#define SHUBIO_ORI_FLUSH            _IOW(SHUBIO , 113 , int)

#define SHUBIO_RVECT_ACTIVATE       _IOW(SHUBIO , 120 , int)
#define SHUBIO_RVECT_SET_FREQ       _IOW(SHUBIO , 121 , int)
#define SHUBIO_RVECT_SET_BATCH      _IOW(SHUBIO , 122 , int)
#define SHUBIO_RVECT_FLUSH          _IOW(SHUBIO , 123 , int)

#define SHUBIO_PEDO_ACTIVATE        _IOW(SHUBIO , 130 , int)
//#define SHUBIO_PEDO_SET_FREQ        _IOW(SHUBIO , 131 , int)
#define SHUBIO_PEDO_SET_BATCH       _IOW(SHUBIO , 132 , int)
#define SHUBIO_PEDO_FLUSH           _IOW(SHUBIO , 133 , int)

#define SHUBIO_PEDODET_ACTIVATE     _IOW(SHUBIO , 140 , int)
//#define SHUBIO_PEDODET_SET_FREQ     _IOW(SHUBIO , 141 , int)
#define SHUBIO_PEDODET_SET_BATCH    _IOW(SHUBIO , 142 , int)
#define SHUBIO_PEDODET_FLUSH        _IOW(SHUBIO , 143 , int)

#define SHUBIO_SIGNIFICANT_ACTIVATE _IOW(SHUBIO , 150 , int)

#define SHUBIO_MCU_SET_OPENINFO     _IOW(SHUBIO , 160 , int)
#define SHUBIO_MCU_GET_OPENINFO     _IOW(SHUBIO , 161 , int)

#define SHUBIO_BARO_ACTIVATE        _IOW(SHUBIO , 170 , int)
#define SHUBIO_BARO_SET_FREQ        _IOW(SHUBIO , 171 , int)
#define SHUBIO_BARO_SET_BATCH       _IOW(SHUBIO , 172 , int)
#define SHUBIO_BARO_FLUSH           _IOW(SHUBIO , 173 , int)
#define SHUBIO_BARO_SET_OFFSET      _IOW(SHUBIO , 174 , int)
#define SHUBIO_BARO_GET_OFFSET      _IOW(SHUBIO , 175 , int)

#define SHUBIO_YAS533_SET_HW_OFFSET  _IOW(SHUBIO , 10000 , int)
#define SHUBIO_YAS533_GET_HW_OFFSET  _IOW(SHUBIO , 10001 , int)
#define SHUBIO_FUSION_GET_STATUS     _IOW(SHUBIO , 20000 , int)
#define SHUBIO_FUSION_SET_STAUTS     _IOW(SHUBIO , 20001 , int)

//struct IoCtlDiagSetRef{
//  unsigned char addr;
//  unsigned char reg;
//};

struct IoctlDiagRes{
  int  rtn;
  unsigned char addr;
  unsigned char reg;
  uint8_t data;
  int16_t accGyroData[3];
  int32_t magData[3];
  int32_t baroData[2];
};

struct IoctlDiagMagCalData{
  int  rtn;
  int32_t s32Cx, s32Cy1, s32Cy2;
  int32_t s32A2, s32A3, s32A4, s32A5, s32A6, s32A7, s32A8, s32A9, s32K;
};

struct IoctlDiagFirmwareVersion{
  int  rtn;
  uint8_t data[8];
};

struct IoctlDiagCmdReq {
  int  rtn;
  uint16_t m_Cmd;
  uint16_t m_req_size;
  uint16_t m_res_size;
  uint8_t  m_buf[512];
};

struct IoctlDiagAccCalibration{
  int32_t AccCal[3];
};

struct IoctlDiagMagCalibration{
  int32_t  MagCal[9];
};

//#define SHUB_DIAG_SET_REF_REG       _IOW(SHUBIO,1000, struct IoCtlDiagSetRef)
#define SHUB_DIAG_GET_REG           _IOWR(SHUBIO,1001,struct IoctlDiagRes)
#define SHUB_DIAG_SET_REG           _IOWR(SHUBIO,1002,struct IoctlDiagRes)
#define SHUB_DIAG_MES_GYRO          _IOR(SHUBIO,1003, struct IoctlDiagRes)
#define SHUB_DIAG_MES_ACC           _IOR(SHUBIO,1004, struct IoctlDiagRes)
#define SHUB_DIAG_MES_MAG           _IOR(SHUBIO,1005, struct IoctlDiagRes)
#define SHUB_DIAG_MES_MAG_SELF_TEST _IOR(SHUBIO,1006, struct IoctlDiagRes)
#define SHUB_DIAG_MES_GYRO_CONT     _IOR(SHUBIO,1007, struct IoctlDiagRes)
#define SHUB_DIAG_MES_ACC_CONT      _IOR(SHUBIO,1008, struct IoctlDiagRes)
#define SHUB_DIAG_MES_MAG_CONT      _IOR(SHUBIO,1009, struct IoctlDiagRes)
#define SHUB_DIAG_SENSOR_INIT       _IOR(SHUBIO,1010, struct IoctlDiagRes)
#define SHUB_DIAG_MAG_READ_CAL      _IOR(SHUBIO,1011, struct IoctlDiagMagCalData)
#define SHUB_DIAG_FW_GET_VERSION    _IOR(SHUBIO,1012, struct IoctlDiagFirmwareVersion)
#define SHUB_DIAG_FW_CHECKSUM       _IOR(SHUBIO,1013, struct IoctlDiagRes)
#define SHUB_DIAG_GET_PORT_STATE    _IOWR(SHUBIO,1014, int)
#define SHUB_DIAG_MES_ONLY_GYRO     _IOR(SHUBIO,1016, struct IoctlDiagRes)
#define SHUB_DIAG_MES_ONLY_ACC      _IOR(SHUBIO,1017, struct IoctlDiagRes)
#define SHUB_DIAG_CMD_WRITE         _IOWR(SHUBIO,1018, struct IoctlDiagCmdReq)
#define SHUB_DIAG_CMD_READ          _IOWR(SHUBIO,1019, struct IoctlDiagCmdReq)
#define SHUB_DIAG_ACC_SET_CAL       _IOR(SHUBIO,1020, struct IoctlDiagAccCalibration)
#define SHUB_DIAG_MAG_SET_CAL       _IOR(SHUBIO,1021, struct IoctlDiagMagCalibration)
#define SHUB_DIAG_MES_ACC_SELF_TEST    _IOR(SHUBIO, 1022, struct IoctlDiagRes)
#define SHUB_DIAG_MES_GYRO_SELF_TEST   _IOR(SHUBIO, 1023, struct IoctlDiagRes)
#define SHUB_DIAG_MES_BARO          _IOR(SHUBIO,1024, struct IoctlDiagRes)
#define SHUB_DIAG_MES_BARO_CONT     _IOR(SHUBIO,1025, struct IoctlDiagRes)
#define SHUB_DIAG_MES_ONLY_BARO     _IOR(SHUBIO,1026, struct IoctlDiagRes)
#define SHUB_DIAG_SLAVE_WRITE       _IOWR(SHUBIO, 1034, struct IoctlDiagRes)
#define SHUB_DIAG_SLAVE_READ        _IOWR(SHUBIO, 1035, struct IoctlDiagRes)
#define SHUB_DIAG_MES_BARO_HWTEST     _IOR(SHUBIO,1036, struct IoctlDiagRes)
#define SHUB_DIAG_STOP_GYRO         _IOR(SHUBIO,1040, struct IoctlDiagRes)
#define SHUB_DIAG_STOP_ACC          _IOR(SHUBIO,1041, struct IoctlDiagRes)
#define SHUB_DIAG_STOP_BARO         _IOR(SHUBIO,1042, struct IoctlDiagRes)

struct IoctlExifData{
    int rtn;
    int enable;
    unsigned long delay;
    unsigned char data[8];
};

#define SHUB_EXIF_SET_SHEX_ACC_ACTIVATE _IOWR(SHUBIO,1100, struct IoctlExifData)
#define SHUB_EXIF_SET_SHEX_ACC_DELAY    _IOWR(SHUBIO,1101, struct IoctlExifData)
#define SHUB_EXIF_GET_SHEX_ACC_DELAY    _IOR(SHUBIO,1102, struct IoctlExifData)
#define SHUB_EXIF_GET_ADTECT_DATA       _IOR(SHUBIO,1103, struct IoctlExifData)
#endif
