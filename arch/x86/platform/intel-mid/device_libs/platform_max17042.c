/*
 * platform_max17042.c: max17042 platform data initilization file
 *
 * (C) Copyright 2008 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/export.h>
#include <linux/gpio.h>
#include <asm/intel-mid.h>
#include <linux/i2c.h>
#include <linux/lnw_gpio.h>
#include <linux/power_supply.h>
#include <linux/power/max17042_battery.h>
#include <linux/power/smb347-charger.h>
#include <linux/power/bq24192_charger.h>
#include <linux/power/bq24261_charger.h>
#include <linux/power/battery_id.h>
#include <asm/pmic_pdata.h>
#include <asm/intel-mid.h>
#include <asm/delay.h>
#include <asm/intel_scu_ipc.h>
#include "platform_max17042.h"
#include "platform_bq24192.h"

#define MRFL_SMIP_SRAM_ADDR		0xFFFCE000
#define MOFD_SMIP_SRAM_ADDR		0xFFFC5C00
#define MRFL_PLATFORM_CONFIG_OFFSET	0x3B3
#define MRFL_SMIP_SHUTDOWN_OFFSET	1
#define MRFL_SMIP_RESV_CAP_OFFSET	3

#define MRFL_VOLT_SHUTDOWN_MASK (1 << 1)
#define MRFL_NFC_RESV_MASK	(1 << 3)
#define I2C_GPIO_PIN 		21

static bool msic_battery_check(struct max17042_platform_data *pdata)
{
	struct sfi_table_simple *sb;
	char *mrfl_batt_str = "INTN0001";
#ifdef CONFIG_SFI
	sb = (struct sfi_table_simple *)get_oem0_table();
#else
	sb = NULL;
#endif
	if (sb == NULL) {
		pr_info("invalid battery detected\n");
		snprintf(pdata->battid, BATTID_LEN + 1, "UNKNOWNB");
		snprintf(pdata->serial_num, SERIAL_NUM_LEN + 1, "000000");
		return false;
	} else {
		pr_info("valid battery detected\n");
		/* First entry in OEM0 table is the BATTID. Read battid
		 * if pentry is not NULL and header length is greater
		 * than BATTID length*/
		if (sb->pentry && sb->header.len >= BATTID_LEN) {
			if (strncmp((char *)sb->pentry,
				"PG000001", (BATTID_LEN)) == 0) {
				snprintf(pdata->battid,
					(BATTID_LEN + 1),
					"%s", mrfl_batt_str);
			} else {
				snprintf(pdata->battid,
					(BATTID_LEN + 1),
					"%s", (char *)sb->pentry);
			}

			/* First 2 bytes represent the model name
			 * and the remaining 6 bytes represent the
			 * serial number. */
			if (pdata->battid[0] == 'I' &&
				pdata->battid[1] >= '0'
					&& pdata->battid[1] <= '9') {
				unsigned char tmp[SERIAL_NUM_LEN + 2];
				int i;
				snprintf(pdata->model_name,
					(MODEL_NAME_LEN) + 1,
						"%s", pdata->battid);
				memcpy(tmp, sb->pentry, BATTID_LEN);
				for (i = 0; i < SERIAL_NUM_LEN; i++) {
					sprintf(pdata->serial_num + i*2,
					"%02x", tmp[i + MODEL_NAME_LEN]);
				}
				if ((2 * SERIAL_NUM_LEN) <
					ARRAY_SIZE(pdata->serial_num))
					pdata->serial_num[2 * SERIAL_NUM_LEN]
								 = '\0';
			} else {
				snprintf(pdata->model_name,
						(MODEL_NAME_LEN + 1),
						"%s", pdata->battid);
				snprintf(pdata->serial_num,
						(SERIAL_NUM_LEN + 1), "%s",
				pdata->battid + MODEL_NAME_LEN);
			}
		}
		return true;
	}
	return false;
}

#define UMIP_REF_FG_TBL			0x806	/* 2 bytes */
#define BATT_FG_TBL_BODY		14	/* 144 bytes */
/**
 * mfld_fg_restore_config_data - restore config data
 * @name : Power Supply name
 * @data : config data output pointer
 * @len : length of config data
 *
 */
int mfld_fg_restore_config_data(const char *name, void *data, int len)
{
	int ret = 0;
#ifdef CONFIG_X86_MDFLD
	int mip_offset;
	/* Read the fuel gauge config data from umip */
	mip_offset = UMIP_REF_FG_TBL + BATT_FG_TBL_BODY;
	ret = intel_scu_ipc_read_mip((u8 *)data, len, mip_offset, 0);
#endif
	return ret;
}
EXPORT_SYMBOL(mfld_fg_restore_config_data);

/**
 * mfld_fg_save_config_data - save config data
 * @name : Power Supply name
 * @data : config data input pointer
 * @len : length of config data
 *
 */
int mfld_fg_save_config_data(const char *name, void *data, int len)
{
	int ret = 0;
#ifdef CONFIG_X86_MDFLD
	int mip_offset;
	/* write the fuel gauge config data to umip */
	mip_offset = UMIP_REF_FG_TBL + BATT_FG_TBL_BODY;
	ret = intel_scu_ipc_write_umip((u8 *)data, len, mip_offset);
#endif
	return ret;
}
EXPORT_SYMBOL(mfld_fg_save_config_data);

static int mrfl_get_bat_health(void)
{

	int pbat_health = -ENODEV;
	int bqbat_health = -ENODEV;
#ifdef CONFIG_BQ24261_CHARGER
	 bqbat_health = bq24261_get_bat_health();
#endif
#ifdef CONFIG_PMIC_CCSM
	pbat_health = pmic_get_health();
#endif

	/*Battery temperature exceptions are reported to PMIC. ALl other
	* exceptions are reported to bq24261 charger. Need to read the
	* battery health reported by both drivers, before reporting
	* the actual battery health
	*/

	/* FIXME: need to have a time stamp based implementation to
	* report battery health
	*/

	if (pbat_health < 0 && bqbat_health < 0)
		return pbat_health;
	if (pbat_health > 0 && pbat_health != POWER_SUPPLY_HEALTH_GOOD)
		return pbat_health;
	else
		return bqbat_health;
}

#define DEFAULT_VMIN	3400000		/* 3400mV */
static int mrfl_get_vsys_min(void)
{
	struct ps_batt_chg_prof batt_profile;
	int ret;
	ret = get_batt_prop(&batt_profile);
	if (!ret)
		return ((struct ps_pse_mod_prof *)batt_profile.batt_prof)
					->low_batt_mV * 1000;
	return DEFAULT_VMIN;
}
#define DEFAULT_VMAX_LIM	4200000		/* 4200mV */
static int mrfl_get_volt_max(void)
{
	struct ps_batt_chg_prof batt_profile;
	int ret;
	ret = get_batt_prop(&batt_profile);
	if (!ret)
		return ((struct ps_pse_mod_prof *)batt_profile.batt_prof)
					->voltage_max * 1000;
	return DEFAULT_VMAX_LIM;
}

static bool is_mapped;
static void __iomem *smip;
int get_smip_plat_config(int offset)
{
	unsigned long sram_addr;

	sram_addr = MOFD_SMIP_SRAM_ADDR;

	if (!is_mapped) {
		smip = ioremap_nocache(sram_addr +
				MRFL_PLATFORM_CONFIG_OFFSET, 8);
		is_mapped = true;
	}

	return ioread8(smip + offset);
}

static void init_tgain_toff(struct max17042_platform_data *pdata)
{
	pdata->tgain = NTC_10K_MURATA_TGAIN;
	pdata->toff = NTC_10K_MURATA_TOFF;
}

static void init_callbacks(struct max17042_platform_data *pdata)
{
	pdata->battery_health = mrfl_get_bat_health;
	pdata->battery_pack_temp = pmic_get_battery_pack_temp;
	pdata->get_vmin_threshold = mrfl_get_vsys_min;
	pdata->get_vmax_threshold = mrfl_get_volt_max;
}

static void init_platform_params(struct max17042_platform_data *pdata)
{
	pdata->fg_algo_model = 100;

	if (msic_battery_check(pdata)) {
		pdata->enable_current_sense = true;
		pdata->technology = POWER_SUPPLY_TECHNOLOGY_LION;
		pdata->file_sys_storage_enabled = 1;
		pdata->soc_intr_mode_enabled = true;
		pdata->valid_battery = true;
	}
	pdata->is_init_done = 0;
}

static void init_platform_thresholds(struct max17042_platform_data *pdata)
{
	u8 shutdown_method;

	/* Bit 1 of shutdown method determines if voltage based
	 * shutdown in enabled.
	 * Bit 3 specifies if capacity for NFC should be reserved.
	 * Reserve capacity only if Bit 3 of shutdown method
	 * is enabled.
	 */
	shutdown_method =
		get_smip_plat_config(MRFL_SMIP_SHUTDOWN_OFFSET);
	if (shutdown_method & MRFL_NFC_RESV_MASK)
		pdata->resv_cap =
			get_smip_plat_config
				(MRFL_SMIP_RESV_CAP_OFFSET);

	pdata->is_volt_shutdown = (shutdown_method &
		MRFL_VOLT_SHUTDOWN_MASK) ? 1 : 0;
}

void *max17042_platform_data(void *info)
{
	static struct max17042_platform_data platform_data;

	init_tgain_toff(&platform_data);
	init_callbacks(&platform_data);
	init_platform_params(&platform_data);
	init_platform_thresholds(&platform_data);

	if (smip)
		iounmap(smip);
	return &platform_data;
}
