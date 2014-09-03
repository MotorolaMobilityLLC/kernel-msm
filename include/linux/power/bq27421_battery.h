/*
* Fuel gauge driver for TI BQ27421
*
* Copyright(c) 2014, LGE Inc. All rights reserved.
* ChoongRyeol Lee <choongryeol.lee@lge.com>
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
*/

#ifndef __BQ27421_BATTERY_H__
#define __BQ27421_BATTERY_H__

#define UNSEAL_KEY0 0x8000
#define UNSEAL_KEY1 0x8000

struct bq27421_dataram {
	u8	data_class;
	u8	data_block;
	u8	data[2][1+16];
	u8	checksum;
};

struct bq27421_platform_data {
	bool ext_batt_psy;
};

/* Standard data commands */
#define BQ27421_CONTROL	0x00
#define BQ27421_TEMP		0x02 /* TEMPERATURE */
#define BQ27421_VOLT		0x04 /* VOLTAGE */
#define BQ27421_FLAGS		0x06
#define BQ27421_RC		0x0C /* REMAINING CAPACITY */
#define BQ27421_FCC		0x0E /* FULL CHARGE CAPACITY */
#define BQ27421_AC		0x10 /* AVERAGE CURRENT */
#define BQ27421_SOC		0x1C /* STATE OF CHARGE */
#define BQ27421_SOH		0x20 /* STATE OF HEALTH */
#define BQ27421_P_DOD		0x1A /* Debug4() = PresentDOD() */
#define BQ27421_OCV_I		0X22 /* OCVCurrent() */
#define BQ27421_OCV_V		0x24 /* OCVVoltage() */
#define BQ27421_SCURRENT	0x26 /* Simultaneous Current */
#define BQ27421_REMCAP_UNF	0x28 /* RemainingCapacityUnfiltered() */
#define BQ27421_FULLCAP_UNF	0x2C /* FullChargeCapacityUnfiltered() */
#define BQ27421_DOD0		0x66 /* DOD0() */
#define BQ27421_PASSEDCHG	0x6C /* PassedCharge */
#define BQ27421_QSTART		0x6E /* Qstart() */

/* Control subcommands */
#define BQ27421_SCMD_CTRL_STAT		0x0000
#define BQ27421_SCMD_BOARD_CAL		0x0009
#define BQ27421_SCMD_CFGUPDATE		0x0013
#define BQ27421_SCMD_SEALED		0x0020
#define BQ27421_SCMD_SOFT_RESET	0x0042
#define BQ27421_SCMD_EXIT_CFGUPDATE	0x0043

/* Extended data commands */
#define BQ27421_ECMD_DESIGNCAP		0x3C /* DesignCapacity() 0x3C ~ 0x3D */
#define BQ27421_ECMD_DATACLASS		0x3E /* DataClass() */
#define BQ27421_ECMD_DATABLK		0x3F /* DataBlock() */
#define BQ27421_ECMD_BLKDATA		0x40 /* BlockData() 0x40 ~ 0x5F */
#define BQ27421_ECMD_BLKCHECKSUM	0x60 /* BlockDataCheckSum() */
#define BQ27421_ECMD_BLKCTRL		0x61 /* BlockDataControl() */

/* Data flash subclass ID */
#define SUBCLASS_REGISTERS		0x40
#define SUBCLASS_STATE			0x52

#define OFFSET_OPCONFIG		0x00
#define OFFSET_UPDATESTAT		0x02
#endif
