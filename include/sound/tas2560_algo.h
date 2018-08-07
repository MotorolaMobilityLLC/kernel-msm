/*
** =============================================================================
** Copyright (c) 2016  Texas Instruments Inc.
**
** This program is free software; you can redistribute it and/or modify it under
** the terms of the GNU General Public License as published by the Free Software
** Foundation; version 2.
**
** This program is distributed in the hope that it will be useful, but WITHOUT
** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
** FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details
** File:
**     tas2560_algo.h
**
** Description:
**     Parameter IDs, Control IDs and Functions to communicate with tas2560
**     algo Module present in AFE.
**
** ============================================================================
*/

#ifndef _TAS2560_ALGO_H
#define _TAS2560_ALGO_H

#include <linux/types.h>
#include <sound/apr_audio-v2.h>
#include <linux/delay.h>
#include <sound/soc.h>

/* Below 4 IDs should be same as in CAPIv2 tas2560 algo Module */
#define AFE_PARAM_ID_TAS2560_ALGO_RX_ENABLE	0x00012D13
#define AFE_PARAM_ID_TAS2560_ALGO_TX_ENABLE	0x00012D14
#define AFE_PARAM_ID_TAS2560_ALGO_RX_CFG	0x00012D15
#define AFE_PARAM_ID_TAS2560_ALGO_TX_CFG	0x00012D16

/* Below 2 IDs should be same as in CAPIv2 tas2560 algo Module */
#define AFE_TAS2560_ALGO_MODULE_RX		0x1000FC00
#define AFE_TAS2560_ALGO_MODULE_TX		0x1000FD00

#define MAX_TAS2560_ALGO_PARAM_INDEX		600

#define TAS2560_ALGO_GET_PARAM		1
#define TAS2560_ALGO_SET_PARAM		0
#define TAS2560_ALGO_PAYLOAD_SIZE	14
#define TAS2560_ALGO_RX_PORT_ID		0x1004 /*Quinary Port-Rx*/
#define TAS2560_ALGO_TX_PORT_ID		0x1005 /*Quinary Port-Tx*/
/*I2C Slave addresses, 0x98 is used in Mono-Case*/
#define SLAVE1		0x98
#define SLAVE2		0x9A
#define SLAVE3		0x9C
#define SLAVE4		0x9E
#define SLAVE_BASE	0x96
#define GET_SLAVE(X)	((X * 2) + SLAVE_BASE)

/*(Page,Offset) -> Parameter Index Formula*/
#define TAS2560_ALGO_GET_IDX(X1, X2) \
	((X1 - 1) * 30 + (X2 - 8) / 4)
/*Special parameter ranges from 3810 to 3899
*  (these are handled in capiv2 framework and doesn't reach core algorithm)
*/
#define TAS2560_ALGO_IS_SPL_IDX(X) \
	((((X) >= 3810) && ((X) < 3899)) ? 1 : 0)
/*Param Id to be sent to capiv2 library
* |31 . . . . 24|23 . . . . . 24|15 . . . . . . . . . . 0|
* |Instance ID	|Length		|Parameter Index	 |
*/
#define TAS2560_ALGO_CALC_PIDX(X1, X2, X3, X4) \
	(X1 | X2 | (X3 << 16) | (((X4 - SLAVE_BASE) / 2) << 24))
#define TAS2560_ALGO_MIN(X1, X2) \
	((X1 < X2) ? (X1) : (X2))

#define TAS2560_ALGO_CMD			0
#define TAS2560_ALGO_CMD_SET_RE			1

/*Param ID to bypass algorithm dynamically*/
#define TAS2560_ALGO_CFG_ENABLE			415

/*Param ID to disable/enable FB*/
#define TAS2560_ALGO_FB				101
#define TAS2560_ALGO_FB_ENABLE			(0xc0000000)
#define TAS2560_ALGO_FB_DISABLE			(0x40000000)
#define TAS2560_ALGO_SWAP			413


/*Special parameter ranges from 3810 to 3899
* (these are handled in capiv2 framework and doesn't reach core algorithm)
*/
/*Read F0*/
#define TAS2560_ALGO_GET_F0			3810
/*Read Q*/
#define TAS2560_ALGO_GET_Q			3811
/* Tv */
#define TAS2560_ALGO_GET_TV			3812
/*Read Rdc*/
#define TAS2560_ALGO_GET_RDC			3813
/*Start Command for Calibration*/
#define TAS2560_ALGO_CALIB_START_RDC		3814
/*Stop Command for Calibration*/
#define TAS2560_ALGO_CALIB_STOP_RDC		3815
/*Set Calibrated Rdc*/
#define TAS2560_ALGO_CFG_SET_RE			3816
/*Start Command for F0,Q test*/
#define TAS2560_ALGO_CALIB_START_F0_Q		3817
/*Stop Command for F0,Q test*/
#define TAS2560_ALGO_CALIB_STOP_F0_Q		3818
/*Set Profile(Music(0),Ringtone(1)...etc)*/
#define TAS2560_ALGO_PROFILE			3819


struct afe_tas2560_algo_params_t {
	uint32_t  payload[TAS2560_ALGO_PAYLOAD_SIZE];
} __packed;

int tas2560_algo_routing_init(struct snd_soc_pcm_runtime *rtd);

int afe_tas2560_algo_ctrl(u8 *data, u32 param_id,
	u32 module_id, u8 dir, u8 size);

#endif
