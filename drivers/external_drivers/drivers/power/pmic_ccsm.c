/*
 * pmic_ccsm.c - Intel MID PMIC Charger Driver
 *
 * Copyright (C) 2011 Intel Corporation
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Author: Jenny TC <jenny.tc@intel.com>
 * Author: Yegnesh Iyer <yegnesh.s.iyer@intel.com>
 */

/* Includes */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/kfifo.h>
#include <linux/param.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/usb/otg.h>
#include <linux/power_supply.h>
#include <linux/wakelock.h>
#include <linux/power_supply.h>
#include <linux/rpmsg.h>
#include <linux/version.h>
#include <asm/intel_basincove_gpadc.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
#include <linux/iio/consumer.h>
#else
#include "../../../kernel/drivers/staging/iio/consumer.h"
#endif
#include <asm/intel_scu_pmic.h>
#include <asm/intel_mid_rpmsg.h>
#include <asm/intel_mid_remoteproc.h>
#include <linux/io.h>
#include <linux/sched.h>
#include <linux/pm_runtime.h>
#include <linux/sfi.h>
#include <linux/async.h>
#include <linux/reboot.h>
#include <linux/notifier.h>
#include <linux/power/battery_id.h>
#include <linux/gpio.h>
#include "pmic_ccsm.h"

/* Macros */
#define DRIVER_NAME "pmic_ccsm"
#define PMIC_SRAM_INTR_ADDR 0xFFFFF616
#define ADC_TO_TEMP 1
#define TEMP_TO_ADC 0
#define is_valid_temp(tmp)\
	(!(tmp > chc.pdata->adc_tbl[0].temp ||\
	tmp < chc.pdata->adc_tbl[chc.pdata->max_tbl_row_cnt - 1].temp))
#define is_valid_adc_code(val)\
	(!(val < chc.pdata->adc_tbl[0].adc_val ||\
	val > chc.pdata->adc_tbl[chc.pdata->max_tbl_row_cnt - 1].adc_val))
#define CONVERT_ADC_TO_TEMP(adc_val, temp)\
	adc_temp_conv(adc_val, temp, ADC_TO_TEMP)
#define CONVERT_TEMP_TO_ADC(temp, adc_val)\
	adc_temp_conv(temp, adc_val, TEMP_TO_ADC)
#define NEED_ZONE_SPLIT(bprof)\
	 ((bprof->temp_mon_ranges < MIN_BATT_PROF))

#define USB_WAKE_LOCK_TIMEOUT	(5 * HZ)

/* 100mA value definition for setting the inlimit in bq24261 */
#define USBINPUTICC100VAL	100

#define SOC_ACOK 179
static struct workqueue_struct *soc_acok_wq;

/* Type definitions */
static void pmic_bat_zone_changed(void);
static void pmic_battery_overheat_handler(bool);

/* Extern definitions */

/* Global declarations */
static DEFINE_MUTEX(pmic_lock);
static struct pmic_chrgr_drv_context chc;
static struct interrupt_info chgrirq0_info[] = {
	{
		CHGIRQ0_BZIRQ_MASK,
		0,
		"Battery temperature zone changed",
		NULL,
		NULL,
		pmic_bat_zone_changed,
		NULL,
	},
	{
		CHGIRQ0_BAT_CRIT_MASK,
		SCHGIRQ0_SBAT_CRIT_MASK,
		NULL,
		"Battery Over heat exception",
		"Battery Over heat exception Recovered",
		NULL,
		pmic_battery_overheat_handler
	},
	{
		CHGIRQ0_BAT0_ALRT_MASK,
		SCHGIRQ0_SBAT0_ALRT_MASK,
		NULL,
		"Battery0 temperature inside boundary",
		"Battery0 temperature outside boundary",
		NULL,
		pmic_battery_overheat_handler
	},
	{
		CHGIRQ0_BAT1_ALRT_MASK,
		SCHGIRQ0_SBAT1_ALRT_MASK,
		NULL,
		"Battery1 temperature inside boundary",
		"Battery1 temperature outside boundary",
		NULL,
		NULL
	},
};

u16 pmic_inlmt[][2] = {
	{ 100, CHGRCTRL1_FUSB_INLMT_100},
	{ 150, CHGRCTRL1_FUSB_INLMT_150},
	{ 500, CHGRCTRL1_FUSB_INLMT_500},
	{ 900, CHGRCTRL1_FUSB_INLMT_900},
	{ 1500, CHGRCTRL1_FUSB_INLMT_1500},
};


static inline struct power_supply *get_psy_battery(void)
{
	struct class_dev_iter iter;
	struct device *dev;
	struct power_supply *pst;

	class_dev_iter_init(&iter, power_supply_class, NULL, NULL);
	while ((dev = class_dev_iter_next(&iter))) {
		pst = (struct power_supply *)dev_get_drvdata(dev);
		if (pst->type == POWER_SUPPLY_TYPE_BATTERY) {
			class_dev_iter_exit(&iter);
			return pst;
		}
	}
	class_dev_iter_exit(&iter);

	return NULL;
}


/* Function definitions */
static void lookup_regval(u16 tbl[][2], size_t size, u16 in_val, u8 *out_val)
{
	int i;
	for (i = 1; i < size; ++i)
		if (in_val < tbl[i][0])
			break;

	*out_val = (u8)tbl[i-1][1];
}

static int interpolate_y(int dx1x0, int dy1y0, int dxx0, int y0)
{
	return y0 + DIV_ROUND_CLOSEST((dxx0 * dy1y0), dx1x0);
}

static int interpolate_x(int dy1y0, int dx1x0, int dyy0, int x0)
{
	return x0 + DIV_ROUND_CLOSEST((dyy0 * dx1x0), dy1y0);
}

static int adc_temp_conv(int in_val, int *out_val, int conv)
{
	int tbl_row_cnt, i;
	struct temp_lookup *adc_temp_tbl;

	if (!chc.pdata) {
		dev_err(chc.dev, "ADC-lookup table not yet available\n");
		return -ERANGE;
	}

	tbl_row_cnt = chc.pdata->max_tbl_row_cnt;
	adc_temp_tbl = chc.pdata->adc_tbl;

	if (conv == ADC_TO_TEMP) {
		if (!is_valid_adc_code(in_val))
			return -ERANGE;

		if (in_val == adc_temp_tbl[tbl_row_cnt-1].adc_val)
			i = tbl_row_cnt - 1;
		else {
			for (i = 0; i < tbl_row_cnt; ++i)
				if (in_val < adc_temp_tbl[i].adc_val)
					break;
		}

		*out_val =
		    interpolate_y((adc_temp_tbl[i].adc_val
					- adc_temp_tbl[i - 1].adc_val),
				  (adc_temp_tbl[i].temp
				   - adc_temp_tbl[i - 1].temp),
				  (in_val - adc_temp_tbl[i - 1].adc_val),
				  adc_temp_tbl[i - 1].temp);
	} else {
		if (!is_valid_temp(in_val))
			return -ERANGE;

		if (in_val == adc_temp_tbl[tbl_row_cnt-1].temp)
			i = tbl_row_cnt - 1;
		else {
			for (i = 0; i < tbl_row_cnt; ++i)
				if (in_val > adc_temp_tbl[i].temp)
					break;
		}

		*((short int *)out_val) =
		    interpolate_x((adc_temp_tbl[i].temp
					- adc_temp_tbl[i - 1].temp),
				  (adc_temp_tbl[i].adc_val
				   - adc_temp_tbl[i - 1].adc_val),
				  (in_val - adc_temp_tbl[i - 1].temp),
				  adc_temp_tbl[i - 1].adc_val);
	}
	return 0;
}

static int pmic_read_reg(u16 addr, u8 *val)
{
	int ret;

	ret = intel_scu_ipc_ioread8(addr, val);
	if (ret) {
		dev_err(chc.dev,
			"Error in intel_scu_ipc_ioread8 0x%.4x\n", addr);
		return -EIO;
	}
	return 0;
}


static int __pmic_write_tt(u8 addr, u8 data)
{
	int ret;

	ret = intel_scu_ipc_iowrite8(CHRTTADDR_ADDR, addr);
	if (unlikely(ret))
		return ret;

	return intel_scu_ipc_iowrite8(CHRTTDATA_ADDR, data);
}

static inline int pmic_write_tt(u8 addr, u8 data)
{
	int ret;

	mutex_lock(&pmic_lock);
	ret = __pmic_write_tt(addr, data);
	mutex_unlock(&pmic_lock);

	/* If access is blocked return success to avoid additional
	*  error handling at client side
	*/
	if (ret == -EACCES) {
		dev_warn(chc.dev, "IPC write blocked due to unsigned kernel/invalid battery\n");
		ret = 0;
	}
	return ret;
}

static int __pmic_read_tt(u8 addr, u8 *data)
{
	int ret;

	ret = intel_scu_ipc_iowrite8(CHRTTADDR_ADDR, addr);
	if (ret)
		return ret;

	usleep_range(2000, 3000);

	return intel_scu_ipc_ioread8(CHRTTDATA_ADDR, data);
}

static inline int pmic_read_tt(u8 addr, u8 *data)
{
	int ret;

	mutex_lock(&pmic_lock);
	ret = __pmic_read_tt(addr, data);
	mutex_unlock(&pmic_lock);

	return ret;
}

static int pmic_update_tt(u8 addr, u8 mask, u8 data)
{
	u8 tdata;
	int ret;

	mutex_lock(&pmic_lock);
	ret = __pmic_read_tt(addr, &tdata);
	if (unlikely(ret))
		goto exit;

	tdata = (tdata & ~mask) | (data & mask);
	ret = __pmic_write_tt(addr, tdata);
exit:
	mutex_unlock(&pmic_lock);
	return ret;
}

#ifdef CONFIG_DEBUG_FS
static int pmic_chrgr_reg_show(struct seq_file *seq, void *unused)
{
	int ret;
	u16 addr;
	u16 val1;
	u8 val;

	addr = *((u8 *)seq->private);

	if (addr == CHRGRIRQ1_ADDR) {
		val1 = ioread16(chc.pmic_intr_iomap);
		val = (u8)(val1 >> 8);
	} else if (addr == CHGRIRQ0_ADDR) {
		val1 = ioread16(chc.pmic_intr_iomap);
		val = (u8)val1;
	} else {
		ret = pmic_read_reg(addr, &val);
		if (ret != 0) {
			dev_err(chc.dev,
				"Error reading tt register 0x%2x\n",
				addr);
			return -EIO;
		}
	}

	seq_printf(seq, "0x%x\n", val);
	return 0;
}

static int pmic_chrgr_tt_reg_show(struct seq_file *seq, void *unused)
{
	int ret;
	u8 addr;
	u8 val;

	addr = *((u8 *)seq->private);

	ret = pmic_read_tt(addr, &val);
	if (ret != 0) {
		dev_err(chc.dev,
			"Error reading tt register 0x%2x\n",
			addr);
		return -EIO;
	}

	seq_printf(seq, "0x%x\n", val);
	return 0;
}

static int pmic_chrgr_tt_reg_open(struct inode *inode, struct file *file)
{
	return single_open(file, pmic_chrgr_tt_reg_show, inode->i_private);
}

static int pmic_chrgr_reg_open(struct inode *inode, struct file *file)
{
	return single_open(file, pmic_chrgr_reg_show, inode->i_private);
}

static struct dentry *charger_debug_dir;
static struct pmic_regs_def pmic_regs_bc[] = {
	PMIC_REG_DEF(PMIC_ID_ADDR),
	PMIC_REG_DEF(IRQLVL1_ADDR),
	PMIC_REG_DEF(IRQLVL1_MASK_ADDR),
	PMIC_REG_DEF(CHGRIRQ0_ADDR),
	PMIC_REG_DEF(SCHGRIRQ0_ADDR),
	PMIC_REG_DEF(MCHGRIRQ0_ADDR),
	PMIC_REG_DEF(LOWBATTDET0_ADDR),
	PMIC_REG_DEF(LOWBATTDET1_ADDR),
	PMIC_REG_DEF(BATTDETCTRL_ADDR),
	PMIC_REG_DEF(VBUSDETCTRL_ADDR),
	PMIC_REG_DEF(VDCINDETCTRL_ADDR),
	PMIC_REG_DEF(CHRGRIRQ1_ADDR),
	PMIC_REG_DEF(SCHGRIRQ1_ADDR),
	PMIC_REG_DEF(MCHGRIRQ1_ADDR),
	PMIC_REG_DEF(CHGRCTRL0_ADDR),
	PMIC_REG_DEF(CHGRCTRL1_ADDR),
	PMIC_REG_DEF(CHGRSTATUS_ADDR),
	PMIC_REG_DEF(USBIDCTRL_ADDR),
	PMIC_REG_DEF(USBIDSTAT_ADDR),
	PMIC_REG_DEF(WAKESRC_ADDR),
	PMIC_REG_DEF(THRMBATZONE_ADDR_BC),
	PMIC_REG_DEF(THRMZN0L_ADDR_BC),
	PMIC_REG_DEF(THRMZN0H_ADDR_BC),
	PMIC_REG_DEF(THRMZN1L_ADDR_BC),
	PMIC_REG_DEF(THRMZN1H_ADDR_BC),
	PMIC_REG_DEF(THRMZN2L_ADDR_BC),
	PMIC_REG_DEF(THRMZN2H_ADDR_BC),
	PMIC_REG_DEF(THRMZN3L_ADDR_BC),
	PMIC_REG_DEF(THRMZN3H_ADDR_BC),
	PMIC_REG_DEF(THRMZN4L_ADDR_BC),
	PMIC_REG_DEF(THRMZN4H_ADDR_BC),
};

static struct pmic_regs_def pmic_regs_sc[] = {
	PMIC_REG_DEF(PMIC_ID_ADDR),
	PMIC_REG_DEF(IRQLVL1_ADDR),
	PMIC_REG_DEF(IRQLVL1_MASK_ADDR),
	PMIC_REG_DEF(CHGRIRQ0_ADDR),
	PMIC_REG_DEF(SCHGRIRQ0_ADDR),
	PMIC_REG_DEF(MCHGRIRQ0_ADDR),
	PMIC_REG_DEF(LOWBATTDET0_ADDR),
	PMIC_REG_DEF(LOWBATTDET1_ADDR),
	PMIC_REG_DEF(BATTDETCTRL_ADDR),
	PMIC_REG_DEF(VBUSDETCTRL_ADDR),
	PMIC_REG_DEF(VDCINDETCTRL_ADDR),
	PMIC_REG_DEF(CHRGRIRQ1_ADDR),
	PMIC_REG_DEF(SCHGRIRQ1_ADDR),
	PMIC_REG_DEF(MCHGRIRQ1_ADDR),
	PMIC_REG_DEF(CHGRCTRL0_ADDR),
	PMIC_REG_DEF(CHGRCTRL1_ADDR),
	PMIC_REG_DEF(CHGRSTATUS_ADDR),
	PMIC_REG_DEF(USBIDCTRL_ADDR),
	PMIC_REG_DEF(USBIDSTAT_ADDR),
	PMIC_REG_DEF(WAKESRC_ADDR),
	PMIC_REG_DEF(USBPATH_ADDR),
	PMIC_REG_DEF(USBSRCDETSTATUS_ADDR),
	PMIC_REG_DEF(THRMBATZONE_ADDR_SC),
	PMIC_REG_DEF(THRMZN0L_ADDR_SC),
	PMIC_REG_DEF(THRMZN0H_ADDR_SC),
	PMIC_REG_DEF(THRMZN1L_ADDR_SC),
	PMIC_REG_DEF(THRMZN1H_ADDR_SC),
	PMIC_REG_DEF(THRMZN2L_ADDR_SC),
	PMIC_REG_DEF(THRMZN2H_ADDR_SC),
	PMIC_REG_DEF(THRMZN3L_ADDR_SC),
	PMIC_REG_DEF(THRMZN3H_ADDR_SC),
	PMIC_REG_DEF(THRMZN4L_ADDR_SC),
	PMIC_REG_DEF(THRMZN4H_ADDR_SC),
};

static struct pmic_regs_def pmic_tt_regs[] = {
	PMIC_REG_DEF(TT_I2CDADDR_ADDR),
	PMIC_REG_DEF(TT_CHGRINIT0OS_ADDR),
	PMIC_REG_DEF(TT_CHGRINIT1OS_ADDR),
	PMIC_REG_DEF(TT_CHGRINIT2OS_ADDR),
	PMIC_REG_DEF(TT_CHGRINIT3OS_ADDR),
	PMIC_REG_DEF(TT_CHGRINIT4OS_ADDR),
	PMIC_REG_DEF(TT_CHGRINIT5OS_ADDR),
	PMIC_REG_DEF(TT_CHGRINIT6OS_ADDR),
	PMIC_REG_DEF(TT_CHGRINIT7OS_ADDR),
	PMIC_REG_DEF(TT_USBINPUTICCOS_ADDR),
	PMIC_REG_DEF(TT_USBINPUTICCMASK_ADDR),
	PMIC_REG_DEF(TT_CHRCVOS_ADDR),
	PMIC_REG_DEF(TT_CHRCVMASK_ADDR),
	PMIC_REG_DEF(TT_CHRCCOS_ADDR),
	PMIC_REG_DEF(TT_CHRCCMASK_ADDR),
	PMIC_REG_DEF(TT_LOWCHROS_ADDR),
	PMIC_REG_DEF(TT_LOWCHRMASK_ADDR),
	PMIC_REG_DEF(TT_WDOGRSTOS_ADDR),
	PMIC_REG_DEF(TT_WDOGRSTMASK_ADDR),
	PMIC_REG_DEF(TT_CHGRENOS_ADDR),
	PMIC_REG_DEF(TT_CHGRENMASK_ADDR),
	PMIC_REG_DEF(TT_CUSTOMFIELDEN_ADDR),
	PMIC_REG_DEF(TT_CHGRINIT0VAL_ADDR),
	PMIC_REG_DEF(TT_CHGRINIT1VAL_ADDR),
	PMIC_REG_DEF(TT_CHGRINIT2VAL_ADDR),
	PMIC_REG_DEF(TT_CHGRINIT3VAL_ADDR),
	PMIC_REG_DEF(TT_CHGRINIT4VAL_ADDR),
	PMIC_REG_DEF(TT_CHGRINIT5VAL_ADDR),
	PMIC_REG_DEF(TT_CHGRINIT6VAL_ADDR),
	PMIC_REG_DEF(TT_CHGRINIT7VAL_ADDR),
	PMIC_REG_DEF(TT_USBINPUTICC100VAL_ADDR),
	PMIC_REG_DEF(TT_USBINPUTICC150VAL_ADDR),
	PMIC_REG_DEF(TT_USBINPUTICC500VAL_ADDR),
	PMIC_REG_DEF(TT_USBINPUTICC900VAL_ADDR),
	PMIC_REG_DEF(TT_USBINPUTICC1500VAL_ADDR),
	PMIC_REG_DEF(TT_CHRCVEMRGLOWVAL_ADDR),
	PMIC_REG_DEF(TT_CHRCVCOLDVAL_ADDR),
	PMIC_REG_DEF(TT_CHRCVCOOLVAL_ADDR),
	PMIC_REG_DEF(TT_CHRCVWARMVAL_ADDR),
	PMIC_REG_DEF(TT_CHRCVHOTVAL_ADDR),
	PMIC_REG_DEF(TT_CHRCVEMRGHIVAL_ADDR),
	PMIC_REG_DEF(TT_CHRCCEMRGLOWVAL_ADDR),
	PMIC_REG_DEF(TT_CHRCCCOLDVAL_ADDR),
	PMIC_REG_DEF(TT_CHRCCCOOLVAL_ADDR),
	PMIC_REG_DEF(TT_CHRCCWARMVAL_ADDR),
	PMIC_REG_DEF(TT_CHRCCHOTVAL_ADDR),
	PMIC_REG_DEF(TT_CHRCCEMRGHIVAL_ADDR),
	PMIC_REG_DEF(TT_LOWCHRENVAL_ADDR),
	PMIC_REG_DEF(TT_LOWCHRDISVAL_ADDR),
};

void dump_pmic_regs(void)
{
	int vendor_id = chc.pmic_id & PMIC_VENDOR_ID_MASK;
	u32 pmic_reg_cnt = 0;
	u32 reg_index;
	u8 data;
	int retval;
	struct pmic_regs_def *pmic_regs = NULL;

	if (vendor_id == BASINCOVE_VENDORID) {
		pmic_reg_cnt = ARRAY_SIZE(pmic_regs_bc);
		pmic_regs = pmic_regs_bc;
	} else if (vendor_id == SHADYCOVE_VENDORID) {
		pmic_reg_cnt = ARRAY_SIZE(pmic_regs_sc);
		pmic_regs = pmic_regs_sc;
	}

	dev_info(chc.dev, "PMIC Register dump\n");
	dev_info(chc.dev, "====================\n");

	for (reg_index = 0; reg_index < pmic_reg_cnt; reg_index++) {

		retval = intel_scu_ipc_ioread8(pmic_regs[reg_index].addr,
				&data);
		if (retval)
			dev_err(chc.dev, "Error in reading %x\n",
				pmic_regs[reg_index].addr);
		else
			dev_info(chc.dev, "0x%x=0x%x\n",
				pmic_regs[reg_index].addr, data);
	}
	dev_info(chc.dev, "====================\n");
}

void dump_pmic_tt_regs(void)
{
	u32 pmic_tt_reg_cnt = ARRAY_SIZE(pmic_tt_regs);
	u32 reg_index;
	u8 data;
	int retval;

	dev_info(chc.dev, "PMIC CHRGR TT dump\n");
	dev_info(chc.dev, "====================\n");

	for (reg_index = 0; reg_index < pmic_tt_reg_cnt; reg_index++) {

		retval = pmic_read_tt(pmic_tt_regs[reg_index].addr, &data);
		if (retval)
			dev_err(chc.dev, "Error in reading %x\n",
				pmic_tt_regs[reg_index].addr);
		else
			dev_info(chc.dev, "0x%x=0x%x\n",
				pmic_tt_regs[reg_index].addr, data);
	}

	dev_info(chc.dev, "====================\n");
}
static const struct file_operations pmic_chrgr_reg_fops = {
	.open = pmic_chrgr_reg_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release
};

static const struct file_operations pmic_chrgr_tt_reg_fops = {
	.open = pmic_chrgr_tt_reg_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release
};

static void pmic_debugfs_init(void)
{
	struct dentry *fentry;
	struct dentry *pmic_regs_dir;
	struct dentry *pmic_tt_regs_dir;

	u32 reg_index;
	int vendor_id = chc.pmic_id & PMIC_VENDOR_ID_MASK;
	u32 pmic_reg_cnt = 0;
	u32 pmic_tt_reg_cnt = ARRAY_SIZE(pmic_tt_regs);
	char name[PMIC_REG_NAME_LEN] = {0};
	struct pmic_regs_def *pmic_regs = NULL;

	if (vendor_id == BASINCOVE_VENDORID) {
		pmic_reg_cnt = ARRAY_SIZE(pmic_regs_bc);
		pmic_regs = pmic_regs_bc;
	} else if (vendor_id == SHADYCOVE_VENDORID) {
		pmic_reg_cnt = ARRAY_SIZE(pmic_regs_sc);
		pmic_regs = pmic_regs_sc;
	}

	/* Creating a directory under debug fs for charger */
	charger_debug_dir = debugfs_create_dir(DRIVER_NAME , NULL) ;
	if (charger_debug_dir == NULL)
		goto debugfs_root_exit;

	/* Create a directory for pmic charger registers */
	pmic_regs_dir = debugfs_create_dir("pmic_ccsm_regs",
			charger_debug_dir);

	if (pmic_regs_dir == NULL)
		goto debugfs_err_exit;

	for (reg_index = 0; reg_index < pmic_reg_cnt; reg_index++) {

		sprintf(name, "%s",
			pmic_regs[reg_index].reg_name);

		fentry = debugfs_create_file(name,
				S_IRUGO,
				pmic_regs_dir,
				&pmic_regs[reg_index].addr,
				&pmic_chrgr_reg_fops);

		if (fentry == NULL)
			goto debugfs_err_exit;
	}

	/* Create a directory for pmic tt charger registers */
	pmic_tt_regs_dir = debugfs_create_dir("pmic_ccsm_tt_regs",
			charger_debug_dir);

	if (pmic_tt_regs_dir == NULL)
		goto debugfs_err_exit;

	for (reg_index = 0; reg_index < pmic_tt_reg_cnt; reg_index++) {

		sprintf(name, "%s", pmic_tt_regs[reg_index].reg_name);

		fentry = debugfs_create_file(name,
				S_IRUGO,
				pmic_tt_regs_dir,
				&pmic_tt_regs[reg_index].addr,
				&pmic_chrgr_tt_reg_fops);

		if (fentry == NULL)
			goto debugfs_err_exit;
	}

	dev_dbg(chc.dev, "Debugfs created successfully!!");
	return;

debugfs_err_exit:
	debugfs_remove_recursive(charger_debug_dir);
debugfs_root_exit:
	dev_err(chc.dev, "Error creating debugfs entry!!");
	return;
}

static void pmic_debugfs_exit(void)
{
	if (charger_debug_dir != NULL)
		debugfs_remove_recursive(charger_debug_dir);
}
#endif

static void pmic_get_bat_zone(int *bat_zone)
{
	u8 data = 0;
	u16 addr = 0;
	int vendor_id, ret;

	vendor_id = chc.pmic_id & PMIC_VENDOR_ID_MASK;
	if (vendor_id == BASINCOVE_VENDORID)
		addr = THRMBATZONE_ADDR_BC;
	else if (vendor_id == SHADYCOVE_VENDORID)
		addr = THRMBATZONE_ADDR_SC;

	ret = intel_scu_ipc_ioread8(addr, &data);
	if (ret) {
		dev_err(chc.dev, "Error:%d in reading battery zone\n", ret);
		/* Return undetermined zone in case of IPC failure */
		*bat_zone = PMIC_BZONE_UNKNOWN;
		return;
	}

	*bat_zone = (data & THRMBATZONE_MASK);
}

static void pmic_bat_zone_changed(void)
{
	int cur_zone;
	struct power_supply *psy_bat;

	pmic_get_bat_zone(&cur_zone);
	dev_info(chc.dev, "Battery Zone changed. Current zone is %d\n",
			cur_zone);

	/* if current zone is the top and bottom zones then report OVERHEAT
	 */
	if ((cur_zone == PMIC_BZONE_LOW) || (cur_zone == PMIC_BZONE_HIGH))
		chc.health = POWER_SUPPLY_HEALTH_OVERHEAT;
	else if (cur_zone == PMIC_BZONE_UNKNOWN)
		chc.health = POWER_SUPPLY_HEALTH_UNKNOWN;
	else
		chc.health = POWER_SUPPLY_HEALTH_GOOD;

	psy_bat = get_psy_battery();

	if (psy_bat && psy_bat->external_power_changed)
		psy_bat->external_power_changed(psy_bat);

	return;
}

static void pmic_battery_overheat_handler(bool stat)
{
	if (stat)
		chc.health = POWER_SUPPLY_HEALTH_OVERHEAT;
	else
		chc.health = POWER_SUPPLY_HEALTH_GOOD;
	return;
}

int pmic_get_health(void)
{
	return chc.health;
}

int pmic_enable_vbus(bool enable)
{
	int ret = 0;

	if (enable)
		ret = intel_scu_ipc_update_register(CHGRCTRL0_ADDR,
				WDT_NOKICK_ENABLE, CHGRCTRL0_WDT_NOKICK_MASK);
	else
		ret = intel_scu_ipc_update_register(CHGRCTRL0_ADDR,
				WDT_NOKICK_DISABLE, CHGRCTRL0_WDT_NOKICK_MASK);

	/* If access is blocked return success to avoid additional
	*  error handling at client side
	*/
	if (ret == -EACCES) {
		dev_warn(chc.dev, "IPC blocked due to unsigned kernel/invalid battery\n");
		ret = 0;
	}

	return ret;
}

int pmic_handle_otgmode(bool enable)
{
	int ret = 0;
	int vendor_id;

	vendor_id = chc.pmic_id & PMIC_VENDOR_ID_MASK;

	if (vendor_id != SHADYCOVE_VENDORID) {
		dev_err(chc.dev, "Ignore otg-mode event received\n");
		return 0;
	}

	if (enable) {
		ret = intel_scu_ipc_update_register(CHGRCTRL1_ADDR,
				CHGRCTRL1_OTGMODE_MASK,
				CHGRCTRL1_OTGMODE_MASK);

		/* ShadyCove PMIC doesn’t kick charger-WDT during host-mode.
		 * Driver does this regularly as a w/a. But, during suspend,
		 * since this driver code doesn’t run, VBUS drops, DUT wakes
		 * up, and re-enumerates again.
		 * Hence, during host-mode, driver shall hold a wakelock.
		 */
		dev_info(chc.dev, "Hold wakelock for host-mode WDT-kick\n");
		if (!wake_lock_active(&chc.otg_wa_wakelock)) {
			wake_lock(&chc.otg_wa_wakelock);
		}
	} else {
		ret = intel_scu_ipc_update_register(CHGRCTRL1_ADDR,
				0x0, CHGRCTRL1_OTGMODE_MASK);

		dev_info(chc.dev, "Release wakelock for host-mode WDT-kick\n");
		if (wake_lock_active(&chc.otg_wa_wakelock)) {
			wake_unlock(&chc.otg_wa_wakelock);
		}
	}

	/* If access is blocked return success to avoid additional
	*  error handling at client side
	*/
	if (ret == -EACCES) {
		dev_warn(chc.dev, "IPC blocked due to unsigned kernel/invalid battery\n");
		ret = 0;
	}

	return ret;
}

int pmic_enable_charging(bool enable)
{
	int ret;
	u8 val;

	if (enable) {
		ret = intel_scu_ipc_update_register(CHGRCTRL1_ADDR,
			CHGRCTRL1_FTEMP_EVENT_MASK, CHGRCTRL1_FTEMP_EVENT_MASK);
		if (ret)
			return ret;
	}

	val = (enable) ? 0 : EXTCHRDIS_ENABLE;

	ret = intel_scu_ipc_update_register(CHGRCTRL0_ADDR,
			val, CHGRCTRL0_EXTCHRDIS_MASK);
	/* If access is blocked return success to avoid additional
	*  error handling at client side
	*/
	if (ret == -EACCES) {
		dev_warn(chc.dev, "IPC blocked due to unsigned kernel/invalid battery\n");
		ret = 0;
	}

	return ret;
}

static inline int update_zone_cc(int zone, u8 reg_val)
{
	u8 addr_cc = TT_CHRCCHOTVAL_ADDR - zone;
	dev_dbg(chc.dev, "%s:%X=%X\n", __func__, addr_cc, reg_val);
	return pmic_write_tt(addr_cc, reg_val);
}

static inline int update_zone_cv(int zone, u8 reg_val)
{
	u8 addr_cv = TT_CHRCVHOTVAL_ADDR - zone;
	dev_dbg(chc.dev, "%s:%X=%X\n", __func__, addr_cv, reg_val);
	return pmic_write_tt(addr_cv, reg_val);
}

static inline int update_zone_temp(int zone, u16 adc_val)
{
	int ret;
	u16 addr_tzone;
	int vendor_id = chc.pmic_id & PMIC_VENDOR_ID_MASK;

	if (vendor_id == BASINCOVE_VENDORID)
		addr_tzone = THRMZN4H_ADDR_BC - (2 * zone);
	else if (vendor_id == SHADYCOVE_VENDORID) {
		/* to take care of address-discontinuity of zone-registers */
		int offset_zone = zone;
		if (zone >= 3)
			offset_zone += 1;

		addr_tzone = THRMZN4H_ADDR_SC - (2 * offset_zone);

		/*
		 * Override the adc values received from the LUT with the
		 * values received from the PMIC hatdware team. SC pmic gets
		 * 12-bit of ADC results however only 9-bits of the values can
		 * be programmed into the temperature zone registers.
		 */
		switch (zone) {
		case 0:
			adc_val = THRMZN4_SC_ADCVAL;
			break;
		case 1:
			adc_val = THRMZN3_SC_ADCVAL;
			break;
		case 2:
			adc_val = THRMZN2_SC_ADCVAL;
			break;
		case 3:
			adc_val = THRMZN1_SC_ADCVAL;
			break;
		case 4:
			adc_val = THRMZN0_SC_ADCVAL;
			break;
		default:
			dev_err(chc.dev, "no ADC default values\n");
		}
	} else {
		dev_err(chc.dev, "%s: invalid vendor id %X\n", __func__, vendor_id);
		return -EINVAL;
	}

	ret = intel_scu_ipc_iowrite8(addr_tzone, (u8)(adc_val >> 8));
	if (unlikely(ret))
		return ret;
	dev_dbg(chc.dev, "%s:%X:%X=%X\n", __func__, addr_tzone,
				(addr_tzone+1), adc_val);

	return intel_scu_ipc_iowrite8(addr_tzone+1, (u8)(adc_val & 0xFF));
}

int pmic_set_cc(int new_cc)
{
	struct ps_pse_mod_prof *bcprof = chc.actual_bcprof;
	struct ps_pse_mod_prof *r_bcprof = chc.runtime_bcprof;
	int temp_mon_ranges;
	int new_cc1;
	int ret;
	int i, cur_zone;
	u8 reg_val = 0;

	pmic_get_bat_zone(&cur_zone);
	dev_info(chc.dev, "%s: Battery Zone:%d\n", __func__, cur_zone);

	/* No need to write PMIC if CC = 0 */
	if (!new_cc)
		return 0;

	temp_mon_ranges = min_t(u16, bcprof->temp_mon_ranges,
			BATT_TEMP_NR_RNG);

	for (i = 0; i < temp_mon_ranges; ++i) {
		new_cc1 = min_t(int, new_cc,
				bcprof->temp_mon_range[i].full_chrg_cur);

		if (new_cc1 != r_bcprof->temp_mon_range[i].full_chrg_cur) {
			if (chc.pdata->cc_to_reg) {
				chc.pdata->cc_to_reg(new_cc1, &reg_val);
				ret = update_zone_cc(i, reg_val);
				if (unlikely(ret))
					return ret;
			}
			r_bcprof->temp_mon_range[i].full_chrg_cur = new_cc1;
		}
	}

	/* send the new CC and CV */
	intel_scu_ipc_update_register(CHGRCTRL1_ADDR,
		CHGRCTRL1_FTEMP_EVENT_MASK, CHGRCTRL1_FTEMP_EVENT_MASK);

	return 0;
}

int pmic_set_cv(int new_cv)
{
	struct ps_pse_mod_prof *bcprof = chc.actual_bcprof;
	struct ps_pse_mod_prof *r_bcprof = chc.runtime_bcprof;
	int temp_mon_ranges;
	int new_cv1;
	int ret;
	int i, cur_zone;
	u8 reg_val = 0;

	pmic_get_bat_zone(&cur_zone);
	dev_info(chc.dev, "%s: Battery Zone:%d\n", __func__, cur_zone);

	/* No need to write PMIC if CV = 0 */
	if (!new_cv)
		return 0;

	temp_mon_ranges = min_t(u16, bcprof->temp_mon_ranges,
			BATT_TEMP_NR_RNG);

	for (i = 0; i < temp_mon_ranges; ++i) {
		new_cv1 = min_t(int, new_cv,
				bcprof->temp_mon_range[i].full_chrg_vol);

		if (new_cv1 != r_bcprof->temp_mon_range[i].full_chrg_vol) {
			if (chc.pdata->cv_to_reg) {
				chc.pdata->cv_to_reg(new_cv1, &reg_val);
				ret = update_zone_cv(i, reg_val);
				if (unlikely(ret))
					return ret;
			}
			r_bcprof->temp_mon_range[i].full_chrg_vol = new_cv1;
		}
	}

	/* send the new CC and CV */
	intel_scu_ipc_update_register(CHGRCTRL1_ADDR,
		CHGRCTRL1_FTEMP_EVENT_MASK, CHGRCTRL1_FTEMP_EVENT_MASK);

	return 0;
}

int pmic_set_ilimma(int ilim_ma)
{
	u8 reg_val;
	int ret;

	lookup_regval(pmic_inlmt, ARRAY_SIZE(pmic_inlmt),
			ilim_ma, &reg_val);
	dev_dbg(chc.dev, "Setting inlmt %d in register %x=%x\n", ilim_ma,
		CHGRCTRL1_ADDR, reg_val);
	ret = intel_scu_ipc_iowrite8(CHGRCTRL1_ADDR, reg_val);

	/* If access is blocked return success to avoid additional
	*  error handling at client side
	*/
	if (ret == -EACCES) {
		dev_warn(chc.dev, "IPC blocked due to unsigned kernel/invalid battery\n");
		ret = 0;
	}

	return ret;
}

/**
 * pmic_read_adc_val - read ADC value of specified sensors
 * @channel: channel of the sensor to be sampled
 * @sensor_val: pointer to the charger property to hold sampled value
 * @chc :  battery info pointer
 *
 * Returns 0 if success
 */
static int pmic_read_adc_val(int channel, int *sensor_val,
			      struct pmic_chrgr_drv_context *chc)
{
	int val;
	int ret;
	struct iio_channel *indio_chan;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0))
	indio_chan = iio_st_channel_get("BATTEMP", "BATTEMP0");
#else
	indio_chan = iio_channel_get(NULL, "BATTEMP0");
#endif
	if (IS_ERR_OR_NULL(indio_chan)) {
		ret = PTR_ERR(indio_chan);
		goto exit;
	}
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0))
	ret = iio_st_read_channel_raw(indio_chan, &val);
#else
	ret = iio_read_channel_raw(indio_chan, &val);
#endif
	if (ret) {
		dev_err(chc->dev, "IIO channel read error\n");
		goto err_exit;
	}

	switch (channel) {
	case GPADC_BATTEMP0:
		ret = CONVERT_ADC_TO_TEMP(val, sensor_val);
		break;
	default:
		dev_err(chc->dev, "invalid sensor%d", channel);
		ret = -EINVAL;
	}
	dev_dbg(chc->dev, "pmic_ccsm pmic_ccsm.0: %s adc val=%x, %d temp=%d\n",
		__func__, val, val, *sensor_val);

err_exit:
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0))
	iio_st_channel_release(indio_chan);
#else
	iio_channel_release(indio_chan);
#endif
exit:
	return ret;
}

int pmic_get_battery_pack_temp(int *temp)
{
	if (chc.invalid_batt)
		return -ENODEV;
	return pmic_read_adc_val(GPADC_BATTEMP0, temp, &chc);
}

static int scove_get_usbid(void)
{
	int ret;
	struct iio_channel *indio_chan;
	int rid, id = RID_UNKNOWN;
	u8 val;

	ret = pmic_read_reg(SCHGRIRQ1_ADDR, &val);
	if (ret) {
		dev_err(chc.dev,
			"Error reading SCHGRIRQ1-register 0x%2x\n",
			SCHGRIRQ1_ADDR);
		return ret;
	}

	/* SCHGRIRQ1_REG SUSBIDDET bit definition:
	 * 00 = RID_A/B/C ; 01 = RID_GND ; 10 = RID_FLOAT */
	if ((val & SCHRGRIRQ1_SUSBIDGNDDET_MASK) == SHRT_FLT_DET)
		return RID_FLOAT;
	else if ((val & SCHRGRIRQ1_SUSBIDGNDDET_MASK) == SHRT_GND_DET)
		return RID_GND;

	indio_chan = iio_channel_get(NULL, "USBID");
	if (IS_ERR_OR_NULL(indio_chan)) {
		dev_err(chc.dev, "Failed to get IIO channel USBID\n");
		ret = PTR_ERR(indio_chan);
		goto exit;
	}

	ret = iio_read_channel_raw(indio_chan, &rid);
	if (ret) {
		dev_err(chc.dev, "IIO channel read error for USBID\n");
		goto err_exit;
	}

	if ((rid > 11150) && (rid < 13640))
		id = RID_A;
	else if ((rid > 6120) && (rid < 7480))
		id = RID_B;
	else if ((rid > 3285) && (rid < 4015))
		id = RID_C;

err_exit:
	iio_channel_release(indio_chan);
exit:
	return id;
}

static int get_charger_type(void)
{
	int ret, i = 0;
	u8 val;
	int chgr_type, rid;

	do {
		ret = pmic_read_reg(USBSRCDETSTATUS_ADDR, &val);
		if (ret != 0) {
			dev_err(chc.dev,
				"Error reading USBSRCDETSTAT-register 0x%2x\n",
				USBSRCDETSTATUS_ADDR);
			return 0;
		}

		i++;
		dev_info(chc.dev, "Read USBSRCDETSTATUS val: %x\n", val);

		if (val & USBSRCDET_SUSBHWDET_DETSUCC)
			break;
		else
			msleep(USBSRCDET_SLEEP_TIME);
	} while (i < USBSRCDET_RETRY_CNT);

	if (!(val & USBSRCDET_SUSBHWDET_DETSUCC)) {
		dev_err(chc.dev, "Charger detection unsuccessful after %dms\n",
			i * USBSRCDET_SLEEP_TIME);
		return 0;
	}

	chgr_type = (val & USBSRCDET_USBSRCRSLT_MASK) >> 2;
	dev_info(chc.dev, "Charger type after detection complete: %d\n",
			(val & USBSRCDET_USBSRCRSLT_MASK) >> 2);

	switch (chgr_type) {
	case PMIC_CHARGER_TYPE_SDP:
	case PMIC_CHARGER_TYPE_FLOAT_DP_DN:
		return POWER_SUPPLY_CHARGER_TYPE_USB_SDP;
	case PMIC_CHARGER_TYPE_DCP:
		return POWER_SUPPLY_CHARGER_TYPE_USB_DCP;
	case PMIC_CHARGER_TYPE_CDP:
		return POWER_SUPPLY_CHARGER_TYPE_USB_CDP;
	case PMIC_CHARGER_TYPE_ACA:
		rid = scove_get_usbid();
		if (rid == RID_A)
			return POWER_SUPPLY_CHARGER_TYPE_ACA_DOCK;
		else if (rid != RID_UNKNOWN)
			return POWER_SUPPLY_CHARGER_TYPE_USB_ACA;
	case PMIC_CHARGER_TYPE_SE1:
		return POWER_SUPPLY_CHARGER_TYPE_SE1;
	case PMIC_CHARGER_TYPE_MHL:
		return POWER_SUPPLY_CHARGER_TYPE_MHL;
	default:
		return POWER_SUPPLY_CHARGER_TYPE_NONE;
	}
}

static void handle_internal_usbphy_notifications(int mask)
{
	struct power_supply_cable_props cap = {0};

	if (mask) {
		cap.chrg_evt = POWER_SUPPLY_CHARGER_EVENT_CONNECT;
		cap.chrg_type = get_charger_type();
		chc.charger_type = cap.chrg_type;
	} else {
		cap.chrg_evt = POWER_SUPPLY_CHARGER_EVENT_DISCONNECT;
		cap.chrg_type = chc.charger_type;
	}

	if (cap.chrg_type == POWER_SUPPLY_CHARGER_TYPE_USB_SDP)
		cap.ma = 0;
	else if ((cap.chrg_type == POWER_SUPPLY_CHARGER_TYPE_USB_DCP)
			|| (cap.chrg_type == POWER_SUPPLY_CHARGER_TYPE_USB_CDP)
			|| (cap.chrg_type == POWER_SUPPLY_CHARGER_TYPE_SE1)
			|| (cap.chrg_type == POWER_SUPPLY_CHARGER_TYPE_USB_ACA)
			|| (cap.chrg_type == POWER_SUPPLY_CHARGER_TYPE_ACA_DOCK))
		cap.ma = 1500;

	dev_info(chc.dev, "Notifying OTG ev:%d, evt:%d, chrg_type:%d, mA:%d\n",
			USB_EVENT_CHARGER, cap.chrg_evt, cap.chrg_type,
			cap.ma);
	atomic_notifier_call_chain(&chc.otg->notifier,
			USB_EVENT_CHARGER, &cap);
}

/* ShadyCove-WA for VBUS removal detect issue */
int pmic_handle_low_supply(void)
{
	int ret;
	u8 val;
	int vendor_id = chc.pmic_id & PMIC_VENDOR_ID_MASK;

	dev_info(chc.dev, "Low-supply event received from external-charger\n");
	if (vendor_id == BASINCOVE_VENDORID || !chc.vbus_connect_status) {
		dev_err(chc.dev, "Ignore Low-supply event received\n");
		return 0;
	}

	msleep(200);
	ret = pmic_read_reg(SCHGRIRQ1_ADDR, &val);
	if (ret) {
		dev_err(chc.dev,
			"Error reading SCHGRIRQ1-register 0x%2x\n",
			SCHGRIRQ1_ADDR);
		return ret;
	}

	if (!(val & SCHRGRIRQ1_SVBUSDET_MASK)) {
		int mask = 0;

		dev_info(chc.dev, "USB VBUS Removed. Notifying OTG driver\n");
		mutex_lock(&chc.evt_queue_lock);
		chc.vbus_connect_status = false;
		mutex_unlock(&chc.evt_queue_lock);

		if (chc.is_internal_usb_phy && !chc.otg_mode_enabled)
			handle_internal_usbphy_notifications(mask);
		else {
			atomic_notifier_call_chain(&chc.otg->notifier,
					USB_EVENT_VBUS, &mask);
			mutex_lock(&chc.evt_queue_lock);
			chc.otg_mode_enabled = false;
			mutex_unlock(&chc.evt_queue_lock);
		}
	}

	return ret;
}

static void handle_level0_interrupt(u8 int_reg, u8 stat_reg,
				struct interrupt_info int_info[],
				int int_info_size)
{
	int i;
	bool int_stat;
	char *log_msg;

	for (i = 0; i < int_info_size; ++i) {

		/*continue if interrupt register bit is not set */
		if (!(int_reg & int_info[i].int_reg_mask))
			continue;

		/*log message if interrupt bit is set */
		if (int_info[i].log_msg_int_reg_true)
			dev_err(chc.dev, "%s",
					int_info[i].log_msg_int_reg_true);

		/* interrupt bit is set.call int handler. */
		if (int_info[i].int_handle)
			int_info[i].int_handle();

		/* continue if stat_reg_mask is zero which
		 *  means ignore status register
		 */
		if (!(int_info[i].stat_reg_mask))
			continue;

		dev_dbg(chc.dev,
				"stat_reg=%X int_info[i].stat_reg_mask=%X",
				stat_reg, int_info[i].stat_reg_mask);

		/* check if the interrupt status is true */
		int_stat = (stat_reg & int_info[i].stat_reg_mask);

		/* log message */
		log_msg = int_stat ? int_info[i].log_msg_stat_true :
			int_info[i].log_msg_stat_false;

		if (log_msg)
			dev_err(chc.dev, "%s", log_msg);

		/* call status handler function */
		if (int_info[i].stat_handle)
			int_info[i].stat_handle(int_stat);

	}

	return ;
}


static void pmic_ccsm_check_host_connect(u8 int_reg, u8 stat_reg)
{
	int __maybe_unused mask;
	int vendor_id = chc.pmic_id & PMIC_VENDOR_ID_MASK;
	int __maybe_unused ret;
	u8 __maybe_unused val;

	if (vendor_id == SHADYCOVE_VENDORID) {
		if (int_reg & CHRGRIRQ1_SUSBIDFLTDET_MASK)
			dev_info(chc.dev,
				"USBID-FLT interrupt received\n");

		ret = pmic_read_reg(CHGRCTRL1_ADDR, &val);
		mask = ((stat_reg & SCHRGRIRQ1_SUSBIDGNDDET_MASK)
				== SHRT_GND_DET) ? 1 : 0;

		if (int_reg & CHRGRIRQ1_SUSBIDGNDDET_MASK) {
			if (mask) {
				dev_info(chc.dev,
				"USBID-GND Detected. Notifying OTG\n");
				val |= (1<<6);
				ret = intel_scu_ipc_iowrite8(
				CHGRCTRL1_ADDR,
				val);
			} else {
				dev_info(chc.dev,
				"USBID-GND Removed. Notifying OTG\n");
				val &= ~(1<<6);
				ret = intel_scu_ipc_iowrite8(
				CHGRCTRL1_ADDR,
				val);
				chc.otg_mode_enabled = false;
			}

			atomic_notifier_call_chain(&chc.otg->notifier,
					USB_EVENT_ID, &mask);
		}
	}
}

static void handle_level1_interrupt(u8 int_reg, u8 stat_reg)
{
	int mask;
	u8 val;
	u16 val1;
	int ret;
	int vendor_id = chc.pmic_id & PMIC_VENDOR_ID_MASK;

	if (!int_reg)
		return;

	pmic_ccsm_check_host_connect(int_reg, stat_reg);
	mask = !!(int_reg & stat_reg);
	if ((vendor_id == BASINCOVE_VENDORID) &&
			(int_reg & CHRGRIRQ1_SUSBIDDET_MASK)) {
		if (mask)
			dev_info(chc.dev,
				"USB ID Detected. Notifying OTG driver\n");
		else
			dev_info(chc.dev,
				"USB ID Removed. Notifying OTG driver\n");

		atomic_notifier_call_chain(&chc.otg->notifier,
				USB_EVENT_ID, &mask);
	}

	if (int_reg & CHRGRIRQ1_SVBUSDET_MASK) {
		if (mask) {
			dev_info(chc.dev,
				"USB VBUS Detected. Notifying OTG driver\n");
			chc.vbus_connect_status = true;

			msleep(100);
			val1 = ioread16(chc.pmic_intr_iomap);
			int_reg = (u8)(val1 >> 8);
			pmic_read_reg(SCHGRIRQ1_ADDR, &stat_reg);
			pmic_ccsm_check_host_connect(int_reg, stat_reg);
			ret = pmic_read_reg(CHGRCTRL1_ADDR, &val);
			if (ret != 0) {
				dev_err(chc.dev,
				"Error reading CHGRCTRL1-register 0x%2x\n",
				CHGRCTRL1_ADDR);
				return;
			}

			if (val & CHGRCTRL1_OTGMODE_MASK)
				chc.otg_mode_enabled = true;
		} else {
			dev_info(chc.dev, "USB VBUS Removed. Notifying OTG driver\n");
			chc.vbus_connect_status = false;
		}

		/* Avoid charger-detection flow in case of host-mode */
		if (chc.is_internal_usb_phy && !chc.otg_mode_enabled)
			handle_internal_usbphy_notifications(mask);
		else {
			atomic_notifier_call_chain(&chc.otg->notifier,
					USB_EVENT_VBUS, &mask);
			if (!mask)
				chc.otg_mode_enabled = false;
		}
	}

	return;
}

static irqreturn_t soc_acok_isr(int irq, void *dev_id)
{
	pr_info("%s, SOC_ACOK = %d\n", __func__,
		gpio_get_value(SOC_ACOK) ? 1 : 0);

	if ((gpio_get_value(SOC_ACOK) ? 1 : 0))
		queue_delayed_work(soc_acok_wq, &chc.acok_irq_work, 0.5 * HZ);

	return IRQ_HANDLED;
}

static void acok_irq_work_function(struct work_struct *work)
{
	pmic_handle_low_supply();
}

static int soc_acok_init(struct platform_device *pdev)
{
	int err = 0;
	unsigned gpio = SOC_ACOK;
	unsigned irq_num = gpio_to_irq(gpio);

	soc_acok_wq = create_singlethread_workqueue("soc_acok_wq");
	INIT_DELAYED_WORK(&chc.acok_irq_work, acok_irq_work_function);

	err = gpio_request(gpio, "soc_acok");
	if (err)
		pr_info("Fail to request GPIO %d.\n", gpio);

	err = gpio_direction_input(gpio);
	if (err)
		pr_info("Fail to configure GPIO %d as input.\n", gpio);

	err = request_irq(irq_num, soc_acok_isr,
		IRQF_TRIGGER_RISING | IRQF_SHARED, "soc_acok", pdev);
	if (err < 0)
		pr_info("Fail to request IRQ %d.\n", irq_num);

	return 0;
}

static void pmic_event_worker(struct work_struct *work)
{
	struct pmic_event *evt, *tmp;

	dev_dbg(chc.dev, "%s\n", __func__);

	mutex_lock(&chc.evt_queue_lock);
	list_for_each_entry_safe(evt, tmp, &chc.evt_queue, node) {
		list_del(&evt->node);

		dev_info(chc.dev, "CHGRIRQ0=%X SCHGRIRQ0=%X CHGRIRQ1=%x SCHGRIRQ1=%X\n",
				evt->chgrirq0_int, evt->chgrirq0_stat,
				evt->chgrirq1_int, evt->chgrirq1_stat);
		if (evt->chgrirq0_int)
			handle_level0_interrupt(evt->chgrirq0_int,
				evt->chgrirq0_stat, chgrirq0_info,
				ARRAY_SIZE(chgrirq0_info));

		if (evt->chgrirq1_stat)
			handle_level1_interrupt(evt->chgrirq1_int,
							evt->chgrirq1_stat);
		kfree(evt);
	}

	mutex_unlock(&chc.evt_queue_lock);
}

static irqreturn_t pmic_isr(int irq, void *data)
{
	u16 pmic_intr;
	u8 chgrirq0_int;
	u8 chgrirq1_int;
	u8 mask = 0;
	int vendor_id = chc.pmic_id & PMIC_VENDOR_ID_MASK;

	if (vendor_id == BASINCOVE_VENDORID)
		mask = ((CHRGRIRQ1_SVBUSDET_MASK) |
				(CHRGRIRQ1_SUSBIDDET_MASK));
	else if (vendor_id == SHADYCOVE_VENDORID)
		mask = ((CHRGRIRQ1_SVBUSDET_MASK) |
				(CHRGRIRQ1_SUSBIDFLTDET_MASK) |
				(CHRGRIRQ1_SUSBIDGNDDET_MASK));

	pmic_intr = ioread16(chc.pmic_intr_iomap);
	chgrirq0_int = (u8)pmic_intr;
	chgrirq1_int = (u8)(pmic_intr >> 8);

	if (!chgrirq1_int && !(chgrirq0_int & PMIC_CHRGR_INT0_MASK))
		return IRQ_NONE;

	if ((chgrirq1_int & mask) && (!wake_lock_active(&chc.wakelock))) {
		/*
		Setting the Usb wake lock hold timeout to a safe value of 5s.
		*/
		wake_lock_timeout(&chc.wakelock, USB_WAKE_LOCK_TIMEOUT);
	}

	dev_dbg(chc.dev, "%s", __func__);

	return IRQ_WAKE_THREAD;
}
static irqreturn_t pmic_thread_handler(int id, void *data)
{
	u16 pmic_intr;
	struct pmic_event *evt;
	int ret;

	evt = kzalloc(sizeof(*evt), GFP_ATOMIC);
	if (evt == NULL) {
		dev_dbg(chc.dev, "Error allocating evt structure in fn:%s\n",
			__func__);
		return IRQ_NONE;
	}

	pmic_intr = ioread16(chc.pmic_intr_iomap);
	evt->chgrirq0_int = (u8)pmic_intr;
	evt->chgrirq1_int = (u8)(pmic_intr >> 8);
	dev_dbg(chc.dev, "irq0=%x irq1=%x\n",
		evt->chgrirq0_int, evt->chgrirq1_int);

	/*
	In case this is an external charger interrupt, we are
	clearing the level 1 irq register and let external charger
	driver handle the interrupt.
	 */

	if (!(evt->chgrirq1_int) &&
		!(evt->chgrirq0_int & PMIC_CHRGR_CCSM_INT0_MASK)) {
		intel_scu_ipc_update_register(IRQLVL1_MASK_ADDR, 0x00,
				IRQLVL1_CHRGR_MASK);
		if ((chc.invalid_batt) &&
			(evt->chgrirq0_int & PMIC_CHRGR_EXT_CHRGR_INT_MASK)) {
			dev_dbg(chc.dev, "Handling external charger interrupt!!\n");
			kfree(evt);
			return IRQ_HANDLED;
		}
		kfree(evt);
		dev_dbg(chc.dev, "Unhandled interrupt!!\n");
		return IRQ_NONE;
	}

	if (evt->chgrirq0_int & PMIC_CHRGR_CCSM_INT0_MASK) {
		ret = intel_scu_ipc_ioread8(SCHGRIRQ0_ADDR,
				&evt->chgrirq0_stat);
		if (ret) {
			dev_err(chc.dev,
				"%s: Error(%d) in intel_scu_ipc_ioread8. Faile to read SCHGRIRQ0_ADDR\n",
					__func__, ret);
			kfree(evt);
			goto end;
		}
	}
	if (evt->chgrirq1_int) {
		ret = intel_scu_ipc_ioread8(SCHGRIRQ1_ADDR,
				&evt->chgrirq1_stat);
		if (ret) {
			dev_err(chc.dev,
				"%s: Error(%d) in intel_scu_ipc_ioread8. Faile to read SCHGRIRQ1_ADDR\n",
					__func__, ret);
			kfree(evt);
			goto end;
		}
	}

	INIT_LIST_HEAD(&evt->node);

	mutex_lock(&chc.evt_queue_lock);
	list_add_tail(&evt->node, &chc.evt_queue);
	mutex_unlock(&chc.evt_queue_lock);

	queue_work(system_nrt_wq, &chc.evt_work);

end:
	/*clear first level IRQ */
	dev_dbg(chc.dev, "Clearing IRQLVL1_MASK_ADDR\n");
	intel_scu_ipc_update_register(IRQLVL1_MASK_ADDR, 0x00,
			IRQLVL1_CHRGR_MASK);

	return IRQ_HANDLED;
}
static int pmic_init(void)
{
	int ret = 0, i, temp_mon_ranges;
	u16 adc_val;
	u8 reg_val;
	struct ps_pse_mod_prof *bcprof = chc.actual_bcprof;
	temp_mon_ranges = min_t(u16, bcprof->temp_mon_ranges,
			BATT_TEMP_NR_RNG);
	for (i = 0; i < temp_mon_ranges; ++i) {
		ret =
		CONVERT_TEMP_TO_ADC(bcprof->temp_mon_range[i].temp_up_lim,
				(int *)&adc_val);
		if (unlikely(ret)) {
			dev_err(chc.dev,
				"Error converting temperature for zone %d!!\n",
				i);
			return ret;
		}

		ret = update_zone_temp(i, adc_val);
		if (unlikely(ret)) {
			dev_err(chc.dev,
				"Error updating zone temp for zone %d\n",
				i);
			return ret;
		}

		if (chc.pdata->cc_to_reg)
			chc.pdata->cc_to_reg(bcprof->temp_mon_range[i].
					full_chrg_cur, &reg_val);

		ret = update_zone_cc(i, reg_val);
		if (unlikely(ret)) {
			dev_err(chc.dev,
				"Error updating zone cc for zone %d\n",
				i);
			return ret;
		}

		if (chc.pdata->cv_to_reg)
			chc.pdata->cv_to_reg(bcprof->temp_mon_range[i].
					full_chrg_vol, &reg_val);

		ret = update_zone_cv(i, reg_val);
		if (unlikely(ret)) {
			dev_err(chc.dev,
				"Error updating zone cv for zone %d\n",
				i);
			return ret;
		}

		/* Write lowest temp limit */
		if (i == (bcprof->temp_mon_ranges - 1)) {
			ret = CONVERT_TEMP_TO_ADC(bcprof->temp_low_lim,
							(int *)&adc_val);
			if (unlikely(ret)) {
				dev_err(chc.dev,
					"Error converting low lim temp!!\n");
				return ret;
			}

			ret = update_zone_temp(i+1, adc_val);

			if (unlikely(ret)) {
				dev_err(chc.dev,
					"Error updating last temp for zone %d\n",
					i+1);
				return ret;
			}
		}
	}
	ret = pmic_update_tt(TT_CUSTOMFIELDEN_ADDR,
				TT_HOT_COLD_LC_MASK,
				TT_HOT_COLD_LC_DIS);

	if (unlikely(ret)) {
		dev_err(chc.dev, "Error updating TT_CUSTOMFIELD_EN reg\n");
		return ret;
	}

	if (chc.pdata->inlmt_to_reg)
		chc.pdata->inlmt_to_reg(USBINPUTICC100VAL, &reg_val);

	ret = pmic_write_tt(TT_USBINPUTICC100VAL_ADDR, reg_val);
	return ret;
}

static inline void print_ps_pse_mod_prof(struct ps_pse_mod_prof *bcprof)
{
	int i, temp_mon_ranges;

	dev_info(chc.dev, "ChrgProf: batt_id:%s\n", bcprof->batt_id);
	dev_info(chc.dev, "ChrgProf: battery_type:%u\n", bcprof->battery_type);
	dev_info(chc.dev, "ChrgProf: capacity:%u\n", bcprof->capacity);
	dev_info(chc.dev, "ChrgProf: voltage_max:%u\n", bcprof->voltage_max);
	dev_info(chc.dev, "ChrgProf: chrg_term_ma:%u\n", bcprof->chrg_term_ma);
	dev_info(chc.dev, "ChrgProf: low_batt_mV:%u\n", bcprof->low_batt_mV);
	dev_info(chc.dev, "ChrgProf: disch_tmp_ul:%d\n", bcprof->disch_tmp_ul);
	dev_info(chc.dev, "ChrgProf: disch_tmp_ll:%d\n", bcprof->disch_tmp_ll);
	dev_info(chc.dev, "ChrgProf: temp_mon_ranges:%u\n",
			bcprof->temp_mon_ranges);
	temp_mon_ranges = min_t(u16, bcprof->temp_mon_ranges,
			BATT_TEMP_NR_RNG);

	for (i = 0; i < temp_mon_ranges; ++i) {
		dev_info(chc.dev, "ChrgProf: temp_up_lim[%d]:%d\n",
				i, bcprof->temp_mon_range[i].temp_up_lim);
		dev_info(chc.dev, "ChrgProf: full_chrg_vol[%d]:%d\n",
				i, bcprof->temp_mon_range[i].full_chrg_vol);
		dev_info(chc.dev, "ChrgProf: full_chrg_cur[%d]:%d\n",
				i, bcprof->temp_mon_range[i].full_chrg_cur);
		dev_info(chc.dev, "ChrgProf: maint_chrgr_vol_ll[%d]:%d\n",
				i, bcprof->temp_mon_range[i].maint_chrg_vol_ll);
		dev_info(chc.dev, "ChrgProf: maint_chrgr_vol_ul[%d]:%d\n",
				i, bcprof->temp_mon_range[i].maint_chrg_vol_ul);
		dev_info(chc.dev, "ChrgProf: maint_chrg_cur[%d]:%d\n",
				i, bcprof->temp_mon_range[i].maint_chrg_cur);
	}
	dev_info(chc.dev, "ChrgProf: temp_low_lim:%d\n", bcprof->temp_low_lim);
}

static int find_tempzone_index(short int *interval,
				int *num_zones,
				short int *temp_up_lim)
{
	struct ps_pse_mod_prof *bprof = chc.sfi_bcprof->batt_prof;
	int up_lim_index = 0, low_lim_index = -1;
	int diff = 0;
	int i;

	*num_zones = MIN_BATT_PROF - bprof->temp_mon_ranges + 1;
	if ((*num_zones) <= 0)
		return 0;

	for (i = 0 ; i < bprof->temp_mon_ranges ; i++) {
		if (bprof->temp_mon_range[i].temp_up_lim == BATT_TEMP_WARM)
			up_lim_index = i;
	}

	low_lim_index = up_lim_index + 1;

	if (low_lim_index == bprof->temp_mon_ranges)
		diff = bprof->temp_low_lim -
			bprof->temp_mon_range[up_lim_index].temp_up_lim;
	else
		diff = bprof->temp_mon_range[low_lim_index].temp_up_lim -
			bprof->temp_mon_range[up_lim_index].temp_up_lim;

	*interval = diff / (*num_zones);
	*temp_up_lim = bprof->temp_mon_range[up_lim_index].temp_up_lim;

	return up_lim_index;
}


static void set_pmic_batt_prof(struct ps_pse_mod_prof *new_prof,
				struct ps_pse_mod_prof *bprof)
{
	int num_zones;
	int split_index;
	int i, j = 0;
	short int temp_up_lim = 0;
	short int interval = 0;

	if ((new_prof == NULL) || (bprof == NULL))
		return;

	if (!NEED_ZONE_SPLIT(bprof)) {
		dev_info(chc.dev, "No need to split the zones!!\n");
		memcpy(new_prof, bprof, sizeof(struct ps_pse_mod_prof));
		return;
	}

	strcpy(&(new_prof->batt_id[0]), &(bprof->batt_id[0]));
	new_prof->battery_type = bprof->battery_type;
	new_prof->capacity = bprof->capacity;
	new_prof->voltage_max =  bprof->voltage_max;
	new_prof->chrg_term_ma = bprof->chrg_term_ma;
	new_prof->low_batt_mV =  bprof->low_batt_mV;
	new_prof->disch_tmp_ul = bprof->disch_tmp_ul;
	new_prof->disch_tmp_ll = bprof->disch_tmp_ll;

	split_index = find_tempzone_index(&interval, &num_zones, &temp_up_lim);

	for (i = 0 ; i < bprof->temp_mon_ranges; i++) {
		if ((i == split_index) && (num_zones > 0)) {
			for (j = 0; j < num_zones; j++,
					temp_up_lim += interval) {
				memcpy(&new_prof->temp_mon_range[i+j],
					&bprof->temp_mon_range[i],
					sizeof(bprof->temp_mon_range[i]));
				new_prof->temp_mon_range[i+j].temp_up_lim =
					temp_up_lim;
			}
			j--;
		} else {
			memcpy(&new_prof->temp_mon_range[i+j],
				&bprof->temp_mon_range[i],
				sizeof(bprof->temp_mon_range[i]));
		}
	}

	new_prof->temp_mon_ranges = i+j;
	new_prof->temp_low_lim = bprof->temp_low_lim;

	return;
}


static int pmic_check_initial_events(void)
{
	struct pmic_event *evt;
	int ret;
	u8 mask = (CHRGRIRQ1_SVBUSDET_MASK);
	int vendor_id = chc.pmic_id & PMIC_VENDOR_ID_MASK;

	evt = kzalloc(sizeof(struct pmic_event), GFP_KERNEL);
	if (evt == NULL) {
		dev_dbg(chc.dev, "Error allocating evt structure in fn:%s\n",
			__func__);
		return -1;
	}

	ret = intel_scu_ipc_ioread8(SCHGRIRQ0_ADDR, &evt->chgrirq0_stat);
	evt->chgrirq0_int = evt->chgrirq0_stat;
	ret = intel_scu_ipc_ioread8(SCHGRIRQ1_ADDR, &evt->chgrirq1_stat);
	evt->chgrirq1_int = evt->chgrirq1_stat;

	/* For ShadyCove, CHGRIRQ1_REG & SCHGRIRQ1_REG cannot be directly
	 * mapped. If status has (01 = Short to ground detected), it means
	 * USBIDGNDDET should be handled. If status has (10 = Floating pin
	 * detected), it means USBIDFLTDET should be handled.
	 */
	if (vendor_id == SHADYCOVE_VENDORID) {
		if ((evt->chgrirq1_stat & SCHRGRIRQ1_SUSBIDGNDDET_MASK)
				== SHRT_FLT_DET) {
			evt->chgrirq1_int |= CHRGRIRQ1_SUSBIDFLTDET_MASK;
			evt->chgrirq1_int &= ~CHRGRIRQ1_SUSBIDGNDDET_MASK;
		} else if ((evt->chgrirq1_stat & SCHRGRIRQ1_SUSBIDGNDDET_MASK)
				== SHRT_GND_DET)
			evt->chgrirq1_int |= CHRGRIRQ1_SUSBIDGNDDET_MASK;
	}

	if (evt->chgrirq1_stat || evt->chgrirq0_int) {
		INIT_LIST_HEAD(&evt->node);
		mutex_lock(&chc.evt_queue_lock);
		list_add_tail(&evt->node, &chc.evt_queue);
		mutex_unlock(&chc.evt_queue_lock);
		schedule_work(&chc.evt_work);
	}

	if ((evt->chgrirq1_stat & mask) && !wake_lock_active(&chc.wakelock)) {
		/*
		Setting the Usb wake lock hold timeout to a safe value of 5s.
		*/
		wake_lock_timeout(&chc.wakelock, USB_WAKE_LOCK_TIMEOUT);
	}

	pmic_bat_zone_changed();

	return ret;
}

/**
 * pmic_charger_probe - PMIC charger probe function
 * @pdev: pmic platform device structure
 * Context: can sleep
 *
 * pmic charger driver initializes its internal data
 * structure and other  infrastructure components for it
 * to work as expected.
 */
static int pmic_chrgr_probe(struct platform_device *pdev)
{
	int retval = 0;
	u8 val;

	if (!pdev)
		return -ENODEV;

	chc.health = POWER_SUPPLY_HEALTH_UNKNOWN;
	chc.dev = &pdev->dev;
	chc.irq = platform_get_irq(pdev, 0);
	chc.pdata = pdev->dev.platform_data;
	platform_set_drvdata(pdev, &chc);

	if (chc.pdata == NULL) {
		dev_err(chc.dev, "Platform data not initialized\n");
		return -EFAULT;
	}

	retval = intel_scu_ipc_ioread8(PMIC_ID_ADDR, &chc.pmic_id);
	if (retval) {
		dev_err(chc.dev,
			"Error reading PMIC ID register\n");
		return retval;
	}

	dev_info(chc.dev, "PMIC-ID: %x\n", chc.pmic_id);
	if ((chc.pmic_id & PMIC_VENDOR_ID_MASK) == SHADYCOVE_VENDORID) {
		retval = pmic_read_reg(USBPATH_ADDR, &val);
		if (retval) {
			dev_err(chc.dev,
				"Error reading CHGRSTATUS-register 0x%2x\n",
				CHGRSTATUS_ADDR);
			return retval;
		}

		if (val & USBPATH_USBSEL_MASK) {
			dev_info(chc.dev, "SOC-Internal-USBPHY used\n");
			chc.is_internal_usb_phy = true;
		} else
			dev_info(chc.dev, "External-USBPHY used\n");
	}

	chc.sfi_bcprof = kzalloc(sizeof(struct ps_batt_chg_prof),
				GFP_KERNEL);
	if (chc.sfi_bcprof == NULL) {
		dev_err(chc.dev,
			"Error allocating memeory SFI battery profile\n");
		return -ENOMEM;
	}

	retval = get_batt_prop(chc.sfi_bcprof);
	if (retval) {
		dev_err(chc.dev,
			"Error reading battery profile from battid frmwrk\n");
		kfree(chc.sfi_bcprof);
		chc.invalid_batt = true;
		chc.sfi_bcprof = NULL;
	}

	retval = intel_scu_ipc_update_register(CHGRCTRL0_ADDR, SWCONTROL_ENABLE,
			CHGRCTRL0_SWCONTROL_MASK);
	if (retval)
		dev_err(chc.dev, "Error enabling sw control. Charging may continue in h/w control mode\n");

	if (!chc.invalid_batt) {
		chc.actual_bcprof = kzalloc(sizeof(struct ps_pse_mod_prof),
					GFP_KERNEL);
		if (chc.actual_bcprof == NULL) {
			dev_err(chc.dev,
				"Error allocating mem for local battery profile\n");
			kfree(chc.sfi_bcprof);
			return -ENOMEM;
		}

		chc.runtime_bcprof = kzalloc(sizeof(struct ps_pse_mod_prof),
					GFP_KERNEL);
		if (chc.runtime_bcprof == NULL) {
			dev_err(chc.dev,
			"Error allocating mem for runtime batt profile\n");
			kfree(chc.sfi_bcprof);
			kfree(chc.actual_bcprof);
			return -ENOMEM;
		}

		set_pmic_batt_prof(chc.actual_bcprof,
				chc.sfi_bcprof->batt_prof);
		print_ps_pse_mod_prof(chc.actual_bcprof);
		retval = pmic_init();
		if (retval)
			dev_err(chc.dev, "Error in Initializing PMIC. Continue in h/w charging mode\n");

		memcpy(chc.runtime_bcprof, chc.actual_bcprof,
			sizeof(struct ps_pse_mod_prof));
	}

	chc.pmic_intr_iomap = ioremap_nocache(PMIC_SRAM_INTR_ADDR, 8);
	if (!chc.pmic_intr_iomap) {
		dev_err(&pdev->dev, "ioremap Failed\n");
		retval = -ENOMEM;
		goto ioremap_failed;
	}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0))
	chc.otg = usb_get_transceiver();
#else
	chc.otg = usb_get_phy(USB_PHY_TYPE_USB2);
#endif
	if (!chc.otg || IS_ERR(chc.otg)) {
		dev_err(&pdev->dev, "Failed to get otg transceiver!!\n");
		retval = -ENOMEM;
		goto otg_req_failed;
	}

	INIT_WORK(&chc.evt_work, pmic_event_worker);
	INIT_LIST_HEAD(&chc.evt_queue);
	mutex_init(&chc.evt_queue_lock);
	wake_lock_init(&chc.wakelock, WAKE_LOCK_SUSPEND, "pmic_wakelock");
	wake_lock_init(&chc.otg_wa_wakelock, WAKE_LOCK_SUSPEND,
			"pmic_otg_wa_wakelock");

	/* register interrupt */
	retval = request_threaded_irq(chc.irq, pmic_isr,
			pmic_thread_handler,
			IRQF_SHARED|IRQF_NO_SUSPEND ,
			DRIVER_NAME, &chc);
	if (retval) {
		dev_err(&pdev->dev,
			"Error in request_threaded_irq(irq(%d)!!\n",
			chc.irq);
		goto otg_req_failed;
	}

	retval = gpio_request(57, "SOC_5V_EN");
	if (retval)
		pr_info("Failed to request GPIO 57.\n");
	retval = gpio_direction_output(57, 0);
	if (retval)
		pr_info("Failed to configure GPIO 57 as output low.\n");

	retval = pmic_check_initial_events();
	if (unlikely(retval)) {
		dev_err(&pdev->dev,
			"Error posting initial events\n");
		goto req_irq_failed;
	}

	/* unmask charger interrupts in second level IRQ register*/
	retval = intel_scu_ipc_update_register(MCHGRIRQ0_ADDR, 0x00,
			PMIC_CHRGR_INT0_MASK);
	/* unmask charger interrupts in second level IRQ register*/
	retval = intel_scu_ipc_iowrite8(MCHGRIRQ1_ADDR, 0x00);
	if (unlikely(retval))
		goto unmask_irq_failed;


	/* unmask IRQLVL1 register */
	retval = intel_scu_ipc_update_register(IRQLVL1_MASK_ADDR, 0x00,
			IRQLVL1_CHRGR_MASK);
	if (unlikely(retval))
		goto unmask_irq_failed;

	retval = intel_scu_ipc_update_register(USBIDCTRL_ADDR,
			 ACADETEN_MASK | USBIDEN_MASK,
			ACADETEN_MASK | USBIDEN_MASK);
	if (unlikely(retval))
		goto unmask_irq_failed;

	chc.health = POWER_SUPPLY_HEALTH_GOOD;
#ifdef CONFIG_DEBUG_FS
	pmic_debugfs_init();
#endif

	soc_acok_init(pdev);
	return 0;

unmask_irq_failed:
req_irq_failed:
	free_irq(chc.irq, &chc);
otg_req_failed:
	iounmap(chc.pmic_intr_iomap);
ioremap_failed:
	kfree(chc.sfi_bcprof);
	kfree(chc.actual_bcprof);
	kfree(chc.runtime_bcprof);
	return retval;
}

static void pmic_chrgr_do_exit_ops(struct pmic_chrgr_drv_context *chc)
{
	/*TODO:
	 * If charger is connected send IPC message to SCU to continue charging
	 */
#ifdef CONFIG_DEBUG_FS
	pmic_debugfs_exit();
#endif
}

/**
 * pmic_charger_remove - PMIC Charger driver remove
 * @pdev: PMIC charger platform device structure
 * Context: can sleep
 *
 * PMIC charger finalizes its internal data structure and other
 * infrastructure components that it initialized in
 * pmic_chrgr_probe.
 */
static int pmic_chrgr_remove(struct platform_device *pdev)
{
	struct pmic_chrgr_drv_context *chc = platform_get_drvdata(pdev);

	if (chc) {
		pmic_chrgr_do_exit_ops(chc);
		wake_lock_destroy(&chc->wakelock);
		wake_lock_destroy(&chc->otg_wa_wakelock);
		free_irq(chc->irq, chc);
		iounmap(chc->pmic_intr_iomap);
		kfree(chc->sfi_bcprof);
		kfree(chc->actual_bcprof);
		kfree(chc->runtime_bcprof);
	}

	return 0;
}

#ifdef CONFIG_PM
static int pmic_chrgr_suspend(struct device *dev)
{
	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}

static int pmic_chrgr_resume(struct device *dev)
{
	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}
#endif

#ifdef CONFIG_PM_RUNTIME
static int pmic_chrgr_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}

static int pmic_chrgr_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}

static int pmic_chrgr_runtime_idle(struct device *dev)
{
	dev_dbg(dev, "%s called\n", __func__);
	return 0;
}
#endif

/*********************************************************************
 *		Driver initialisation and finalization
 *********************************************************************/

static const struct dev_pm_ops pmic_chrgr_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(pmic_chrgr_suspend,
				pmic_chrgr_resume)
	SET_RUNTIME_PM_OPS(pmic_chrgr_runtime_suspend,
				pmic_chrgr_runtime_resume,
				pmic_chrgr_runtime_idle)
};

static struct platform_driver pmic_chrgr_driver = {
	.driver = {
		   .name = DRIVER_NAME,
		   .owner = THIS_MODULE,
		   .pm = &pmic_chrgr_pm_ops,
		   },
	.probe = pmic_chrgr_probe,
	.remove = pmic_chrgr_remove,
};

static int pmic_chrgr_init(void)
{
	return platform_driver_register(&pmic_chrgr_driver);
}

static void pmic_chrgr_exit(void)
{
	platform_driver_unregister(&pmic_chrgr_driver);
}

static int pmic_ccsm_rpmsg_probe(struct rpmsg_channel *rpdev)
{
	int ret = 0;

	if (rpdev == NULL) {
		pr_err("rpmsg channel not created\n");
		ret = -ENODEV;
		goto out;
	}

	dev_info(&rpdev->dev, "Probed pmic_ccsm rpmsg device\n");

	ret = pmic_chrgr_init();

out:
	return ret;
}

static void pmic_ccsm_rpmsg_remove(struct rpmsg_channel *rpdev)
{
	pmic_chrgr_exit();
	dev_info(&rpdev->dev, "Removed pmic_ccsm rpmsg device\n");
}

static void pmic_ccsm_rpmsg_cb(struct rpmsg_channel *rpdev, void *data,
					int len, void *priv, u32 src)
{
	dev_warn(&rpdev->dev, "unexpected, message\n");

	print_hex_dump(KERN_DEBUG, __func__, DUMP_PREFIX_NONE, 16, 1,
		       data, len,  true);
}

static struct rpmsg_device_id pmic_ccsm_rpmsg_id_table[] = {
	{ .name	= "rpmsg_pmic_ccsm" },
	{ },
};
MODULE_DEVICE_TABLE(rpmsg, pmic_ccsm_rpmsg_id_table);

static struct rpmsg_driver pmic_ccsm_rpmsg = {
	.drv.name	= KBUILD_MODNAME,
	.drv.owner	= THIS_MODULE,
	.id_table	= pmic_ccsm_rpmsg_id_table,
	.probe		= pmic_ccsm_rpmsg_probe,
	.callback	= pmic_ccsm_rpmsg_cb,
	.remove		= pmic_ccsm_rpmsg_remove,
};

static int __init pmic_ccsm_rpmsg_init(void)
{
	return register_rpmsg_driver(&pmic_ccsm_rpmsg);
}

static void __exit pmic_ccsm_rpmsg_exit(void)
{
	return unregister_rpmsg_driver(&pmic_ccsm_rpmsg);
}
/* Defer init call so that dependant drivers will be loaded. Using  async
 * for parallel driver initialization */
late_initcall(pmic_ccsm_rpmsg_init);
module_exit(pmic_ccsm_rpmsg_exit);

MODULE_AUTHOR("Jenny TC <jenny.tc@intel.com>");
MODULE_DESCRIPTION("PMIC Charger  Driver");
MODULE_LICENSE("GPL");
