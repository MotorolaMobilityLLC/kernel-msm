/*
 * Fuel gauge driver for Maxim 17050
 *
 * Copyright (C) 2011 Samsung Electronics
 * Copyright (C) 2013 Google Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Based on max17042_battery.h
 */

#ifndef __MAX17050_BATTERY_H_
#define __MAX17050_BATTERY_H_

#define MAX17050_STATUS_BATTABSENT	(1 << 3)
#define MAX17050_BATTERY_FULL		100
#define MAX17050_DEFAULT_SNS_RESISTOR	10000

/* Number of words in model characterisation data */
#define MODEL_SIZE			48

enum max17050_register {
	MAX17050_STATUS		= 0X00,
	MAX17050_VALRT_TH	= 0X01,
	MAX17050_TALRT_TH	= 0X02,
	MAX17050_SALRT_TH	= 0X03,
	MAX17050_ATRATE		= 0X04,
	MAX17050_REPCAP		= 0X05,
	MAX17050_REPSOC		= 0X06,
	MAX17050_AGE		= 0X07,
	MAX17050_TEMP		= 0X08,
	MAX17050_VCELL		= 0X09,
	MAX17050_CURRENT	= 0X0A,
	MAX17050_AVGCURRENT	= 0X0B,

	MAX17050_SOC		= 0X0D,
	MAX17050_AVSOC		= 0X0E,
	MAX17050_REMCAP		= 0X0F,
	MAX17050_FULLCAP	= 0X10,
	MAX17050_TTE		= 0X11,
	MAX17050_QRTABLE00	= 0X12,
	MAX17050_FULLSOCTHR	= 0X13,

	MAX17050_AVGTA		= 0X16,
	MAX17050_CYCLES		= 0X17,
	MAX17050_DESIGNCAP	= 0X18,
	MAX17050_AVGVCELL	= 0X19,
	MAX17050_MINMAXTEMP	= 0X1A,
	MAX17050_MINMAXVOLT	= 0X1B,
	MAX17050_MINMAXCURR	= 0X1C,
	MAX17050_CONFIG		= 0X1D,
	MAX17050_ICHGTERM	= 0X1E,
	MAX17050_AVCAP		= 0X1F,
	MAX17050_CUSTOMVER	= 0X20,
	MAX17050_VERSION	= 0X21,
	MAX17050_QRTABLE10	= 0X22,
	MAX17050_FULLCAPNOM	= 0X23,
	MAX17050_TEMPNOM	= 0X24,
	MAX17050_TEMPLIM	= 0X25,

	MAX17050_AIN		= 0X27,
	MAX17050_LEARNCFG	= 0X28,
	MAX17050_FILTERCFG	= 0X29,
	MAX17050_RELAXCFG	= 0X2A,
	MAX17050_MISCCFG	= 0X2B,
	MAX17050_TGAIN		= 0X2C,
	MAX17050_TOFF		= 0X2D,
	MAX17050_CGAIN		= 0X2E,
	MAX17050_COFF		= 0X2F,

	MAX17050_QRTABLE20	= 0X32,

	MAX17050_FULLCAP0	= 0x35,
	MAX17050_IAVG_EMPTY	= 0X36,
	MAX17050_FCTC		= 0X37,
	MAX17050_RCOMP0		= 0X38,
	MAX17050_TEMPCO		= 0X39,
	MAX17050_V_EMPTY	= 0X3A,

	MAX17050_FSTAT		= 0X3D,
	MAX17050_TIMER		= 0X3E,
	MAX17050_SHDNTIMER	= 0X3F,

	MAX17050_QRTABLE30	= 0X42,

	MAX17050_DQACC		= 0X45,
	MAX17050_DPACC		= 0X46,

	MAX17050_VFSOC0		= 0x48,

	MAX17050_QH0		= 0x4c,
	MAX17050_QH		= 0X4D,

	MAX17050_VFSOC0_LOCK	= 0x60,
	MAX17050_MODEL_LOCK1	= 0x62,
	MAX17050_MODEL_LOCK2	= 0x63,

	MAX17050_MODEL_TABLE	= 0x80,

	MAX17050_OCVINTERNAL	= 0XFB,

	MAX17050_VFSOC		= 0XFF,
};

struct max17050_platform_data {
	bool enable_current_sense;
	bool ext_batt_psy;
	u16 empty_soc;
	u16 full_soc;

	/*
	 * R_sns in micro-ohms.
	 * default 10000 (if r_sns = 0) as it is the recommended value by
	 * the datasheet although it can be changed by board designers.
	 */
	unsigned int r_sns;

	u16 relaxcfg;
	u16 config;
	u16 filtercfg;
	u16 learncfg;
	u16 misccfg;
	u16 fullsocthr;

	u16 rcomp0;
	u16 tempco;
	u16 tempnom;
	u16 tgain;
	u16 toff;

	u16 ichgterm;
	u16 vempty;
	u16 qrtable00;
	u16 qrtable10;
	u16 qrtable20;
	u16 qrtable30;

	u16 capacity;
	u16 vf_fullcap;

	u16 temperature;
	u16 dpacc;
	u16 param_version;

	/* model characterisation data */
	u16 model[MODEL_SIZE];
};

#endif /* __MAX17050_BATTERY_H_ */
