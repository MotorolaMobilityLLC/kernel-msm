/*
 * intel_soc_thermal.c - Intel SoC Platform Thermal Driver
 *
 * Copyright (C) 2012 Intel Corporation
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
 * Author: Shravan B M <shravan.k.b.m@intel.com>
 *
 * This driver registers to Thermal framework as SoC zone. It exposes
 * two SoC DTS temperature with two writeable trip points.
 */

#define pr_fmt(fmt)  "intel_soc_thermal: " fmt

#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/thermal.h>
#include <linux/seq_file.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <asm/msr.h>
#include <asm/intel-mid.h>
#include <asm/intel_mid_thermal.h>
#include <asm/processor.h>

#define DRIVER_NAME	"soc_thrm"

/* SOC DTS Registers */
#define SOC_THERMAL_SENSORS	2
#define SOC_THERMAL_TRIPS	2
#define SOC_MAX_STATES		4
#define DTS_ENABLE_REG		0xB0
#define DTS_ENABLE		0x03
#define DTS_TRIP_RW		0x03

#define PUNIT_PORT		0x04
#define PUNIT_TEMP_REG		0xB1
#define PUNIT_AUX_REG		0xB2

#define TJMAX_TEMP		90
#define TJMAX_CODE		0x7F

/* Default hysteresis values in C */
#define DEFAULT_H2C_HYST	3
#define MAX_HYST		7

/* Power Limit registers */
#define PKG_TURBO_POWER_LIMIT	0x610
#define PKG_TURBO_CFG		0x670
#define MSR_THERM_CFG1		0x673

/* PKG_TURBO_PL1 holds PL1 in terms of 32mW */
#define PL_UNIT_MW		32

/* Magic number symbolising Dynamic Turbo OFF */
#define DISABLE_DYNAMIC_TURBO	0xB0FF

/* IRQ details */
#define SOC_DTS_CONTROL		0x80
#define TRIP_STATUS_RO		0xB3
#define TRIP_STATUS_RW		0xB4
/* TE stands for THERMAL_EVENT */
#define TE_AUX0			0xB5
#define ENABLE_AUX_INTRPT	0x0F
#define ENABLE_CPU0		(1 << 16)
#define RTE_ENABLE		(1 << 9)

static int tjmax_temp;
static int turbo_floor_reg;

static DEFINE_MUTEX(thrm_update_lock);

struct platform_soc_data {
	struct thermal_zone_device *tzd[SOC_THERMAL_SENSORS];
	struct thermal_cooling_device *soc_cdev; /* PL1 control */
	int irq;
};

struct cooling_device_info {
	struct soc_throttle_data *soc_data;
	/* Lock protecting the soc_cur_state variable */
	struct mutex lock_state;
	unsigned long soc_cur_state;
};

struct thermal_device_info {
	int sensor_index;
	struct mutex lock_aux;
};

static inline u32 read_soc_reg(unsigned int addr)
{
	return intel_mid_msgbus_read32(PUNIT_PORT, addr);
}

static inline void write_soc_reg(unsigned int addr, u32 val)
{
	intel_mid_msgbus_write32(PUNIT_PORT, addr, val);
}

#ifdef CONFIG_DEBUG_FS
struct dts_regs {
	char *name;
	u32 addr;
} dts_regs[] = {
	/* Thermal Management Registers */
	{"PTMC",	0x80},
	{"TRR0",	0x81},
	{"TRR1",	0x82},
	{"TTS",		0x83},
	{"TELB",	0x84},
	{"TELT",	0x85},
	{"GFXT",	0x88},
	{"VEDT",	0x89},
	{"VECT",	0x8A},
	{"VSPT",	0x8B},
	{"ISPT",	0x8C},
	{"SWT",		0x8D},
	/* Trip Event Registers */
	{"DTSC",	0xB0},
	{"TRR",		0xB1},
	{"PTPS",	0xB2},
	{"PTTS",	0xB3},
	{"PTTSS",	0xB4},
	{"TE_AUX0",	0xB5},
	{"TE_AUX1",	0xB6},
	{"TE_AUX2",	0xB7},
	{"TE_AUX3",	0xB8},
	{"TTE_VRIcc",	0xB9},
	{"TTE_VRHOT",	0xBA},
	{"TTE_PROCHOT",	0xBB},
	{"TTE_SLM0",	0xBC},
	{"TTE_SLM1",	0xBD},
	{"BWTE",	0xBE},
	{"TTE_SWT",	0xBF},
	/* MSI Message Registers */
	{"TMA",		0xC0},
	{"TMD",		0xC1},
};

/* /sys/kernel/debug/soc_thermal/soc_dts */
static struct dentry *soc_dts_dent;
static struct dentry *soc_thermal_dir;

static int soc_dts_debugfs_show(struct seq_file *s, void *unused)
{
	int i;
	u32 val;

	for (i = 0; i < ARRAY_SIZE(dts_regs); i++) {
		val = read_soc_reg(dts_regs[i].addr);
		seq_printf(s,
			"%s[0x%X]	Val: 0x%X\n",
			dts_regs[i].name, dts_regs[i].addr, val);
	}
	return 0;
}

static int debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, soc_dts_debugfs_show, NULL);
}

static const struct file_operations soc_dts_debugfs_fops = {
	.open           = debugfs_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

static void create_soc_dts_debugfs(void)
{
	int err;

	/* /sys/kernel/debug/soc_thermal/ */
	soc_thermal_dir = debugfs_create_dir("soc_thermal", NULL);
	if (IS_ERR(soc_thermal_dir)) {
		err = PTR_ERR(soc_thermal_dir);
		pr_err("debugfs_create_dir failed:%d\n", err);
		return;
	}

	/* /sys/kernel/debug/soc_thermal/soc_dts */
	soc_dts_dent = debugfs_create_file("soc_dts", S_IFREG | S_IRUGO,
					soc_thermal_dir, NULL,
					&soc_dts_debugfs_fops);
	if (IS_ERR(soc_dts_dent)) {
		err = PTR_ERR(soc_dts_dent);
		debugfs_remove_recursive(soc_thermal_dir);
		pr_err("debugfs_create_file failed:%d\n", err);
	}
}

static void remove_soc_dts_debugfs(void)
{
	debugfs_remove_recursive(soc_thermal_dir);
}
#else
static inline void create_soc_dts_debugfs(void) { }
static inline void remove_soc_dts_debugfs(void) { }
#endif

static
struct cooling_device_info *initialize_cdev(struct platform_device *pdev)
{
	struct cooling_device_info *cdev_info =
		kzalloc(sizeof(struct cooling_device_info), GFP_KERNEL);
	if (!cdev_info)
		return NULL;

	cdev_info->soc_data = pdev->dev.platform_data;
	mutex_init(&cdev_info->lock_state);
	return cdev_info;
}

static struct thermal_device_info *initialize_sensor(int index)
{
	struct thermal_device_info *td_info =
		kzalloc(sizeof(struct thermal_device_info), GFP_KERNEL);

	if (!td_info)
		return NULL;
	td_info->sensor_index = index;
	mutex_init(&td_info->lock_aux);

	return td_info;
}

static void initialize_floor_reg_addr(void)
{
	struct cpuinfo_x86 *c = &cpu_data(0);

	if (c->x86_model == 0x4a || c->x86_model == 0x5a)
		turbo_floor_reg = 0xdf;
	else
		turbo_floor_reg = 0x2;
}

static void enable_soc_dts(void)
{
	int i;
	u32 val, eax, edx;

	rdmsr_on_cpu(0, MSR_THERM_CFG1, &eax, &edx);

	/* B[8:10] H2C Hyst */
	eax = (eax & ~(0x7 << 8)) | (DEFAULT_H2C_HYST << 8);

	/* Set the Hysteresis value */
	wrmsr_on_cpu(0, MSR_THERM_CFG1, eax, edx);

	/* Enable the DTS */
	write_soc_reg(DTS_ENABLE_REG, DTS_ENABLE);

	val = read_soc_reg(SOC_DTS_CONTROL);
	write_soc_reg(SOC_DTS_CONTROL, val | ENABLE_AUX_INTRPT | ENABLE_CPU0);

	/* Enable Interrupts for all the AUX trips for the DTS */
	for (i = 0; i < SOC_THERMAL_TRIPS; i++) {
		val = read_soc_reg(TE_AUX0 + i);
		write_soc_reg(TE_AUX0 + i, (val | RTE_ENABLE));
	}
}

static int show_trip_hyst(struct thermal_zone_device *tzd,
				int trip, long *hyst)
{
	u32 eax, edx;
	struct thermal_device_info *td_info = tzd->devdata;

	/* Hysteresis is only supported for trip point 0 */
	if (trip != 0) {
		*hyst = 0;
		return 0;
	}

	mutex_lock(&td_info->lock_aux);

	rdmsr_on_cpu(0, MSR_THERM_CFG1, &eax, &edx);

	/* B[8:10] H2C Hyst, for trip 0. Report hysteresis in mC */
	*hyst = ((eax >> 8) & 0x7) * 1000;

	mutex_unlock(&td_info->lock_aux);
	return 0;
}

static int store_trip_hyst(struct thermal_zone_device *tzd,
				int trip, long hyst)
{
	u32 eax, edx;
	struct thermal_device_info *td_info = tzd->devdata;

	/* Convert from mC to C */
	hyst /= 1000;

	if (trip != 0 || hyst < 0 || hyst > MAX_HYST)
		return -EINVAL;

	mutex_lock(&td_info->lock_aux);

	rdmsr_on_cpu(0, MSR_THERM_CFG1, &eax, &edx);

	/* B[8:10] H2C Hyst */
	eax = (eax & ~(0x7 << 8)) | (hyst << 8);

	wrmsr_on_cpu(0, MSR_THERM_CFG1, eax, edx);

	mutex_unlock(&td_info->lock_aux);
	return 0;
}

static int show_temp(struct thermal_zone_device *tzd, long *temp)
{
	struct thermal_device_info *td_info = tzd->devdata;
	u32 val = read_soc_reg(PUNIT_TEMP_REG);

	/* Extract bits[0:7] or [8:15] using sensor_index */
	*temp =  (val >> (8 * td_info->sensor_index)) & 0xFF;

	if (*temp == 0)
		return 0;

	/* Calibrate the temperature */
	*temp = TJMAX_CODE - *temp + tjmax_temp;

	/* Convert to mC */
	*temp *= 1000;

	return 0;
}

static int show_trip_type(struct thermal_zone_device *tzd,
			int trip, enum thermal_trip_type *trip_type)
{
	/* All are passive trip points */
	*trip_type = THERMAL_TRIP_PASSIVE;

	return 0;
}

static int show_trip_temp(struct thermal_zone_device *tzd,
				int trip, long *trip_temp)
{
	u32 aux_value = read_soc_reg(PUNIT_AUX_REG);

	/* aux0 b[0:7], aux1 b[8:15], aux2 b[16:23], aux3 b[24:31] */
	*trip_temp = (aux_value >> (8 * trip)) & 0xFF;

	/* Calibrate the trip point temperature */
	*trip_temp = tjmax_temp - *trip_temp;

	/* Convert to mC and report */
	*trip_temp *= 1000;

	return 0;
}

static int store_trip_temp(struct thermal_zone_device *tzd,
				int trip, long trip_temp)
{
	u32 aux_trip, aux = 0;
	struct thermal_device_info *td_info = tzd->devdata;

	/* Convert from mC to C */
	trip_temp /= 1000;

	/* The trip temp is 8 bits wide (unsigned) */
	if (trip_temp > 255)
		return -EINVAL;

	/* Assign last byte to unsigned 32 */
	aux_trip = trip_temp & 0xFF;

	/* Calibrate w.r.t TJMAX_TEMP */
	aux_trip = tjmax_temp - aux_trip;

	mutex_lock(&td_info->lock_aux);
	aux = read_soc_reg(PUNIT_AUX_REG);
	switch (trip) {
	case 0:
		/* aux0 bits 0:7 */
		aux = (aux & 0xFFFFFF00) | (aux_trip << (8 * trip));
		break;
	case 1:
		/* aux1 bits 8:15 */
		aux = (aux & 0xFFFF00FF) | (aux_trip << (8 * trip));
		break;
	}
	write_soc_reg(PUNIT_AUX_REG, aux);

	mutex_unlock(&td_info->lock_aux);

	return 0;
}

/* SoC cooling device callbacks */
static int soc_get_max_state(struct thermal_cooling_device *cdev,
				unsigned long *state)
{
	/* SoC has 4 levels of throttling from 0 to 3 */
	*state = SOC_MAX_STATES - 1;
	return 0;
}

static int soc_get_cur_state(struct thermal_cooling_device *cdev,
				unsigned long *state)
{
	struct cooling_device_info *cdev_info =
			(struct cooling_device_info *)cdev->devdata;

	mutex_lock(&cdev_info->lock_state);
	*state = cdev_info->soc_cur_state;
	mutex_unlock(&cdev_info->lock_state);

	return 0;
}

static void set_floor_freq(int val)
{
	u32 eax;

	eax = read_soc_reg(turbo_floor_reg);

	/* Set bits[8:14] of eax to val */
	eax = (eax & ~(0x7F << 8)) | (val << 8);

	write_soc_reg(turbo_floor_reg, eax);
}

static int disable_dynamic_turbo(struct cooling_device_info *cdev_info)
{
	u32 eax, edx;

	mutex_lock(&cdev_info->lock_state);

	rdmsr_on_cpu(0, PKG_TURBO_CFG, &eax, &edx);

	/* Set bits[0:2] to 0 to enable TjMax Turbo mode */
	eax = eax & ~0x07;

	/* Set bit[8] to 0 to disable Dynamic Turbo */
	eax = eax & ~(1 << 8);

	/* Set bits[9:11] to 0 disable Dynamic Turbo Policy */
	eax = eax & ~(0x07 << 9);

	wrmsr_on_cpu(0, PKG_TURBO_CFG, eax, edx);

	/*
	 * Now that we disabled Dynamic Turbo, we can
	 * make the floor frequency ratio also 0.
	 */
	set_floor_freq(0);

	cdev_info->soc_cur_state = DISABLE_DYNAMIC_TURBO;

	mutex_unlock(&cdev_info->lock_state);
	return 0;
}

static int soc_set_cur_state(struct thermal_cooling_device *cdev,
				unsigned long state)
{
	u32 eax, edx;
	struct soc_throttle_data *data;
	struct cooling_device_info *cdev_info =
			(struct cooling_device_info *)cdev->devdata;

	if (state == DISABLE_DYNAMIC_TURBO)
		return disable_dynamic_turbo(cdev_info);

	if (state >= SOC_MAX_STATES) {
		pr_err("Invalid SoC throttle state:%ld\n", state);
		return -EINVAL;
	}

	mutex_lock(&cdev_info->lock_state);

	data = &cdev_info->soc_data[state];

	rdmsr_on_cpu(0, PKG_TURBO_POWER_LIMIT, &eax, &edx);

	/* Set bits[0:14] of eax to 'data->power_limit' */
	eax = (eax & ~0x7FFF) | data->power_limit;

	wrmsr_on_cpu(0, PKG_TURBO_POWER_LIMIT, eax, edx);

	set_floor_freq(data->floor_freq);

	cdev_info->soc_cur_state = state;

	mutex_unlock(&cdev_info->lock_state);
	return 0;
}

#ifdef CONFIG_DEBUG_THERMAL
static int soc_get_force_state_override(struct thermal_cooling_device *cdev,
					char *buf)
{
	int i;
	int pl1_vals_mw[SOC_MAX_STATES];
	struct cooling_device_info *cdev_info =
			(struct cooling_device_info *)cdev->devdata;

	mutex_lock(&cdev_info->lock_state);

	/* PKG_TURBO_PL1 holds PL1 in terms of 32mW. So, multiply by 32 */
	for (i = 0; i < SOC_MAX_STATES; i++) {
		pl1_vals_mw[i] =
			cdev_info->soc_data[i].power_limit * PL_UNIT_MW;
	}

	mutex_unlock(&cdev_info->lock_state);

	return sprintf(buf, "%d %d %d %d\n", pl1_vals_mw[0], pl1_vals_mw[1],
					pl1_vals_mw[2], pl1_vals_mw[3]);
}

static int soc_set_force_state_override(struct thermal_cooling_device *cdev,
					char *buf)
{
	int i, ret;
	int pl1_vals_mw[SOC_MAX_STATES];
	unsigned long cur_state;
	struct cooling_device_info *cdev_info =
				(struct cooling_device_info *)cdev->devdata;

	/*
	 * The four space separated values entered via the sysfs node
	 * override the default values configured through platform data.
	 */
	ret = sscanf(buf, "%d %d %d %d", &pl1_vals_mw[0], &pl1_vals_mw[1],
					&pl1_vals_mw[2], &pl1_vals_mw[3]);
	if (ret != SOC_MAX_STATES) {
		pr_err("Invalid values in soc_set_force_state_override\n");
		return -EINVAL;
	}

	mutex_lock(&cdev_info->lock_state);

	/* PKG_TURBO_PL1 takes PL1 in terms of 32mW. So, divide by 32 */
	for (i = 0; i < SOC_MAX_STATES; i++) {
		cdev_info->soc_data[i].power_limit =
					pl1_vals_mw[i] / PL_UNIT_MW;
	}

	/* Update the cur_state value of this cooling device */
	cur_state = cdev_info->soc_cur_state;

	mutex_unlock(&cdev_info->lock_state);

	return soc_set_cur_state(cdev, cur_state);
}
#endif

static void notify_thermal_event(struct thermal_zone_device *tzd,
				long temp, int event, int level)
{
	char *thermal_event[5];

	pr_info("Thermal Event: sensor: %s, cur_temp: %ld, event: %d, level: %d\n",
				tzd->type, temp, event, level);

	thermal_event[0] = kasprintf(GFP_KERNEL, "NAME=%s", tzd->type);
	thermal_event[1] = kasprintf(GFP_KERNEL, "TEMP=%ld", temp);
	thermal_event[2] = kasprintf(GFP_KERNEL, "EVENT=%d", event);
	thermal_event[3] = kasprintf(GFP_KERNEL, "LEVEL=%d", level);
	thermal_event[4] = NULL;

	kobject_uevent_env(&tzd->device.kobj, KOBJ_CHANGE, thermal_event);

	kfree(thermal_event[3]);
	kfree(thermal_event[2]);
	kfree(thermal_event[1]);
	kfree(thermal_event[0]);

	return;
}

static int get_max_temp(struct platform_soc_data *pdata, long *cur_temp)
{
	int i, ret;
	long temp;

	/*
	 * The SoC has two or more DTS placed, to determine the
	 * temperature of the SoC. The hardware actions are taken
	 * using T(DTS) which is MAX(T(DTS0), T(DTS1), ... T(DTSn))
	 *
	 * Do not report error, as long as we can read at least
	 * one DTS correctly.
	 */
	ret = show_temp(pdata->tzd[0], cur_temp);
	if (ret)
		return ret;

	for (i = 1; i < SOC_THERMAL_SENSORS; i++) {
		ret = show_temp(pdata->tzd[i], &temp);
		if (ret)
			goto fail_safe;

		if (temp > *cur_temp)
			*cur_temp = temp;
	}

fail_safe:
	/*
	 * We have one valid DTS temperature; Use that,
	 * instead of reporting error.
	 */
	return 0;
}

static irqreturn_t soc_dts_intrpt(int irq, void *dev_data)
{
	u32 irq_sts, cur_sts;
	int i, ret, event, level = -1;
	long cur_temp;
	struct thermal_zone_device *tzd;
	struct platform_soc_data *pdata = (struct platform_soc_data *)dev_data;

	if (!pdata || !pdata->tzd[0])
		return IRQ_NONE;

	mutex_lock(&thrm_update_lock);

	tzd = pdata->tzd[0];

	irq_sts = read_soc_reg(TRIP_STATUS_RW);
	cur_sts = read_soc_reg(TRIP_STATUS_RO);

	for (i = 0; i < SOC_THERMAL_TRIPS; i++) {
		if (irq_sts & (1 << i)) {
			level = i;
			event = !!(cur_sts & (1 << i));
			/* Clear the status bit by writing 1 */
			irq_sts |= (1 << i);
			break;
		}
	}

	/* level == -1, indicates an invalid event */
	if (level == -1) {
		dev_err(&tzd->device, "Invalid event from SoC DTS\n");
		goto exit;
	}

	ret = get_max_temp(pdata, &cur_temp);
	if (ret) {
		dev_err(&tzd->device, "Cannot read SoC DTS temperature\n");
		goto exit;
	}

	/* Notify using UEvent */
	notify_thermal_event(tzd, cur_temp, event, level);

	/* Clear the status bits */
	write_soc_reg(TRIP_STATUS_RW, irq_sts);

exit:
	mutex_unlock(&thrm_update_lock);
	return IRQ_HANDLED;
}

static struct thermal_zone_device_ops tzd_ops = {
	.get_temp = show_temp,
	.get_trip_type = show_trip_type,
	.get_trip_temp = show_trip_temp,
	.set_trip_temp = store_trip_temp,
	.get_trip_hyst = show_trip_hyst,
	.set_trip_hyst = store_trip_hyst,
};

static struct thermal_cooling_device_ops soc_cooling_ops = {
	.get_max_state = soc_get_max_state,
	.get_cur_state = soc_get_cur_state,
	.set_cur_state = soc_set_cur_state,
#ifdef CONFIG_DEBUG_THERMAL
	.get_force_state_override = soc_get_force_state_override,
	.set_force_state_override = soc_set_force_state_override,
#endif
};

/*********************************************************************
 *		Driver initialization and finalization
 *********************************************************************/

static irqreturn_t soc_dts_intrpt_handler(int irq, void *dev_data)
{
	return IRQ_WAKE_THREAD;
}

static int soc_thermal_probe(struct platform_device *pdev)
{
	struct platform_soc_data *pdata;
	int i, ret;
	u32 eax, edx;
	static char *name[SOC_THERMAL_SENSORS] = {"SoC_DTS0", "SoC_DTS1"};

	/*
	 * Register to configure floor frequency for DT done
	 * using shadow register for ANN and TNG, Register address
	 * chosen based on cpu model.[Refer:HSD:4380040]
	 */
	initialize_floor_reg_addr();

	pdata = kzalloc(sizeof(struct platform_soc_data), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	ret = rdmsr_safe_on_cpu(0, MSR_IA32_TEMPERATURE_TARGET, &eax, &edx);
	if (ret) {
		tjmax_temp = TJMAX_TEMP;
		dev_err(&pdev->dev, "TjMax read from MSR %x failed, error:%d\n",
				MSR_IA32_TEMPERATURE_TARGET, ret);
	} else {
		tjmax_temp = (eax >> 16) & 0xff;
		dev_dbg(&pdev->dev, "TjMax is %d degrees C\n", tjmax_temp);
	}

	/* Register each sensor with the generic thermal framework */
	for (i = 0; i < SOC_THERMAL_SENSORS; i++) {
		pdata->tzd[i] = thermal_zone_device_register(name[i],
					SOC_THERMAL_TRIPS, DTS_TRIP_RW,
					initialize_sensor(i),
					&tzd_ops, NULL, 0, 0);
		if (IS_ERR(pdata->tzd[i])) {
			ret = PTR_ERR(pdata->tzd[i]);
			dev_err(&pdev->dev, "tzd register failed: %d\n", ret);
			goto exit_reg;
		}
	}

	/* Register a cooling device for PL1 (power limit) control */
	pdata->soc_cdev = thermal_cooling_device_register("SoC",
						initialize_cdev(pdev),
						&soc_cooling_ops);
	if (IS_ERR(pdata->soc_cdev)) {
		ret = PTR_ERR(pdata->soc_cdev);
		pdata->soc_cdev = NULL;
		goto exit_reg;
	}

	platform_set_drvdata(pdev, pdata);

	ret = platform_get_irq(pdev, 0);
	if (ret < 0) {
		dev_err(&pdev->dev, "platform_get_irq failed:%d\n", ret);
		goto exit_cdev;
	}

	pdata->irq = ret;

	/* Register for Interrupt Handler */
	ret = request_threaded_irq(pdata->irq, soc_dts_intrpt_handler,
						soc_dts_intrpt,
						IRQF_TRIGGER_RISING,
						DRIVER_NAME, pdata);
	if (ret) {
		dev_err(&pdev->dev, "request_threaded_irq failed:%d\n", ret);
		goto exit_cdev;
	}

	/* Enable DTS0 and DTS1 */
	enable_soc_dts();

	create_soc_dts_debugfs();

	return 0;

exit_cdev:
	thermal_cooling_device_unregister(pdata->soc_cdev);
exit_reg:
	while (--i >= 0) {
		struct thermal_device_info *td_info = pdata->tzd[i]->devdata;
		kfree(td_info);
		thermal_zone_device_unregister(pdata->tzd[i]);
	}
	platform_set_drvdata(pdev, NULL);
	kfree(pdata);
	return ret;
}

static int soc_thermal_remove(struct platform_device *pdev)
{
	int i;
	struct platform_soc_data *pdata = platform_get_drvdata(pdev);

	/* Unregister each sensor with the generic thermal framework */
	for (i = 0; i < SOC_THERMAL_SENSORS; i++) {
		struct thermal_device_info *td_info = pdata->tzd[i]->devdata;
		kfree(td_info);
		thermal_zone_device_unregister(pdata->tzd[i]);
	}
	thermal_cooling_device_unregister(pdata->soc_cdev);
	platform_set_drvdata(pdev, NULL);
	free_irq(pdata->irq, pdata);
	kfree(pdata);

	remove_soc_dts_debugfs();

	return 0;
}

static const struct platform_device_id therm_id_table[] = {
	{ DRIVER_NAME, 1},
};

static struct platform_driver soc_thermal_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = DRIVER_NAME,
	},
	.probe = soc_thermal_probe,
	.remove = soc_thermal_remove,
	.id_table = therm_id_table,
};

static int __init soc_thermal_module_init(void)
{
	return platform_driver_register(&soc_thermal_driver);
}

static void __exit soc_thermal_module_exit(void)
{
	platform_driver_unregister(&soc_thermal_driver);
}

module_init(soc_thermal_module_init);
module_exit(soc_thermal_module_exit);

MODULE_AUTHOR("Shravan B M <shravan.k.b.m@intel.com>");
MODULE_DESCRIPTION("Intel SoC Thermal Driver");
MODULE_LICENSE("GPL");
