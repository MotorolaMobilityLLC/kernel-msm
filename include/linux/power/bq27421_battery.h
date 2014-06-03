/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __BQ27421_BATTERY_H_
#define __BQ27421_BATTERY_H_

/* #define BQ27421_DEBUG */
#define UNSEAL_KEY0 0x8000
#define UNSEAL_KEY1 0x8000
#define SOC_WORK_PERIOD 10000

struct bq27421_platform_data {
	bool ext_batt_psy;
	int full_design;
};

/* BQ27541 standard data commands */
#define BQ27421_REG_CONTROL		0x00
#define BQ27421_REG_TEMP		0x02 /* TEMPERATURE */
#define BQ27421_REG_VOLT		0x04 /* VOLTAGE */
#define BQ27421_REG_FLAGS		0x06
#define BQ27421_REG_RC			0x0C /* REMAINING CAPACITY */
#define BQ27421_REG_FCC			0x0E /* FULL CHARGE CAPACITY */
#define BQ27421_REG_AC			0x10 /* AVERAGE CURRENT */
#define BQ27421_REG_SOC			0x1C /* STATE OF CHARGE */
#define BQ27421_REG_SOH			0x20 /* STATE OF HEALTH */
#ifdef BQ27421_DEBUG
#define BQ27421_REG_P_DOD		0x1A /* Debug4() = PresentDOD() */
#define BQ27421_REG_OCV_I		0X22 /* OCVCurrent() */
#define BQ27421_REG_OCV_V		0x24 /* OCVVoltage() */
#define BQ27421_REG_REMCAP_UNF		0x28 /* RemainingCapacityUnfiltered() */
#define BQ27421_REG_FULLCAP_UNF		0x2C /* FullChargeCapacityUnfiltered() */
#define BQ27421_REG_DOD0		0x66 /* DOD0() */
#define BQ27421_REG_PASSEDCHG		0x6C /* PassedCharge */
#define BQ27421_REG_QSTART		0x6E /* Qstart() */
#endif

/* Control subcommands */
#define BQ27421_SUBCMD_CTNL_STATUS		0x0000 /* OK */
#define BQ27421_SUBCMD_BOARD_CAL		0x0009
#define BQ27421_SUBCMD_SET_CFGUPDATE		0x0013
#define BQ27421_SUBCMD_SEALED			0x0020
#define BQ27421_SUBCMD_SOFT_RESET		0x0042
#define BQ27421_SUBCMD_EXIT_CFGUPDATE		0x0043

/* BQ27421 Extended Data Commands */
#define BQ27421_EXTCMD_DESIGNCAPACITY		0x3C /* DesignCapacity() 0x3C ~ 0x3D */
#define BQ27421_EXTCMD_DATACLASS		0x3E /* DataClass() */
#define BQ27421_EXTCMD_DATABLOCK		0x3F /* DataBlock() */
#define BQ27421_EXTCMD_BLOCKDATA		0x40 /* BlockData() 0x40 ~ 0x5F */
#define BQ27421_EXTCMD_BLOCKDATACHECKSUM	0x60 /* BlockDataCheckSum() */
#define BQ27421_EXTCMD_BLOCKDATACONTROL		0x61 /* BlockDataControl() */

/* BQ27421 DataFlash Subclass ID */
#define BQ27421_SUBCLASS_REGISTERS		0x40
#define BQ27421_SUBCLASS_STATE			0x52
#endif
