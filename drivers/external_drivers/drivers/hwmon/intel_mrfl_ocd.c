/*
 * intel_mrfl_ocd.c - Intel Merrifield Platform Over Current Detection Driver
 *
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
 * Author: Durgadoss R <durgadoss.r@intel.com>
 *
 * This driver monitors the voltage level of the system. When the voltage
 * drops below a programmed threshold, it notifies the CPU of the drop.
 * Also, the driver configures the HW to take some actions to prevent
 * system crash due to sudden drop in voltage.
 * DEVICE_NAME: Intel Merrifield platform - PMIC: Burst Control Unit
 */

#define pr_fmt(fmt)  "intel_mrfl_ocd: " fmt

#include <linux/module.h>
#include <linux/init.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/hwmon-sysfs.h>
#include <linux/hwmon.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/rpmsg.h>
#include <linux/debugfs.h>
#include <linux/power_supply.h>
#include <asm/intel_scu_ipc.h>
#include <asm/intel_scu_pmic.h>
#include <asm/intel_basincove_ocd.h>
#include <linux/version.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
#include <linux/platform_data/intel_mid_remoteproc.h>
#else
#include <asm/intel_mid_remoteproc.h>
#endif

#define DRIVER_NAME "bcove_bcu"

#define CAMFLASH_STATE_NORMAL	0
#define CAMFLASH_STATE_CRITICAL	3

/* 'enum' of BCU events */
enum bcu_events { VWARN1, VWARN2, VCRIT, GSMPULSE, TXPWRTH, UNKNOWN, __COUNT };

static DEFINE_MUTEX(ocd_update_lock);

/* Warning levels for Voltage (in mV) */
static const unsigned long volt_thresholds[NUM_THRESHOLDS] = {
			2550, 2600, 2700, 2750, 2800, 2900, 3000, 3100 };

/* Warning levels for Current (in mA) */
static const unsigned long curr_thresholds[NUM_THRESHOLDS] = {
			1600, 2000, 2400, 2600, 3000, 3200, 3400, 3600 };

struct ocd_info {
	struct device *dev;
	struct platform_device *pdev;
	struct delayed_work vwarn2_irq_work;
	void *bcu_intr_addr;
	int irq;
};

static uint8_t cam_flash_state;
static uint32_t intr_count_lvl1;
static uint32_t intr_count_lvl2;
static uint32_t intr_count_lvl3;

static void enable_volt_trip_points(void)
{
	int i;
	int ret;

	/*
	 * Enable the Voltage comparator logic, so that the output
	 * signals are asserted when a voltage drop occurs.
	 */
	for (i = 0; i < NUM_VOLT_LEVELS; i++) {
		ret = intel_scu_ipc_update_register(VWARN1_CFG + i,
						VWARN_EN,
						VWARN_EN_MASK);
		if (ret)
			pr_err("EM_BCU: Error in %s updating register 0x%x\n",
					__func__, (VWARN1_CFG + i));
	}
}

static void enable_current_trip_points(void)
{
	int i;
	int ret;

	/*
	 * Enable the Current comparator logic, so that the output
	 * signals are asserted when the platform current surges.
	 */
	for (i = 0; i < NUM_CURR_LEVELS; i++) {
		ret = intel_scu_ipc_update_register(ICCMAXVCC_CFG + i,
						ICCMAXVCC_EN,
						ICCMAXVCC_EN_MASK);
		if (ret)
			pr_err("EM_BCU: Error in %s updating reg 0x%0x\n",
					__func__, (ICCMAXVCC_CFG + i));
	}
}

static int find_threshold(const unsigned long *arr,
				unsigned long value)
{
	int pos = 0;

	if (value < arr[0] || value > arr[NUM_THRESHOLDS - 1])
		return -EINVAL;

	/* Find the index of 'value' in the thresholds array */
	while (pos < NUM_THRESHOLDS && value >= arr[pos])
		++pos;

	return pos - 1;
}

static int set_threshold(u16 reg_addr, int pos)
{
	int ret;
	uint8_t data;

	mutex_lock(&ocd_update_lock);

	ret = intel_scu_ipc_ioread8(reg_addr, &data);
	if (ret)
		goto ipc_fail;

	/* Set bits [0-2] to value of pos */
	data = (data & 0xF8) | pos;

	ret = intel_scu_ipc_iowrite8(reg_addr, data);

ipc_fail:
	mutex_unlock(&ocd_update_lock);
	return ret;
}

static int program_bcu(void *ocd_smip_addr)
{
	int ret, i;
	u8 *smip_data;

	if (!ocd_smip_addr)
		return -ENXIO;

	smip_data = (u8 *)ocd_smip_addr;
	mutex_lock(&ocd_update_lock);

	for (i = 0; i < NUM_SMIP_BYTES-1; i++, smip_data++) {
		ret = intel_scu_ipc_iowrite8(VWARN1_CFG + i, *smip_data);
		if (ret)
			goto ipc_fail;
	}

	/* MBCUIRQ register address not consecutive with other BCU registers */
	ret = intel_scu_ipc_iowrite8(MBCUIRQ, *smip_data);
	if (ret) {
		pr_err("EM_BCU: Inside %s error(%d) in writing addr 0x%02x\n",
				__func__, ret, MBCUIRQ);
		goto ipc_fail;
	}
	pr_debug("EM_BCU: Registers are programmed successfully.\n");

ipc_fail:
	mutex_unlock(&ocd_update_lock);
	return ret;
}

static ssize_t store_curr_thres(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long curnt;
	int pos, ret;
	struct sensor_device_attribute_2 *s_attr =
					to_sensor_dev_attr_2(attr);

	if (kstrtoul(buf, 10, &curnt))
		return -EINVAL;

	pos = find_threshold(curr_thresholds, curnt);
	if (pos < 0)
		return -EINVAL;

	/*
	 * Since VCC_CFG and VNN_CFG are consecutive registers, calculate the
	 * required register address using s_attr->nr.
	 */
	ret = set_threshold(ICCMAXVCC_CFG + s_attr->nr, pos);

	return ret ? ret : count;
}

static ssize_t show_curr_thres(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret;
	uint8_t data;
	struct sensor_device_attribute_2 *s_attr =
					to_sensor_dev_attr_2(attr);

	mutex_lock(&ocd_update_lock);

	ret = intel_scu_ipc_ioread8(ICCMAXVCC_CFG + s_attr->nr, &data);

	mutex_unlock(&ocd_update_lock);

	if (ret)
		return ret;

	/* Read bits [0-2] of data to get the index into the array */
	return sprintf(buf, "%lu\n", curr_thresholds[data & 0x07]);
}

static ssize_t store_volt_thres(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long volt;
	int pos, ret;
	struct sensor_device_attribute_2 *s_attr =
					to_sensor_dev_attr_2(attr);

	if (kstrtoul(buf, 10, &volt))
		return -EINVAL;

	pos = find_threshold(volt_thresholds, volt);
	if (pos < 0)
		return -EINVAL;

	/*
	 * The voltage thresholds are in descending order in VWARN*_CFG
	 * registers. So calculate 'pos' by substracting from NUM_THRESHOLDS.
	 */
	pos = NUM_THRESHOLDS - pos - 1;

	/*
	 * Since VWARN*_CFG are consecutive registers, calculate the
	 * required register address using s_attr->nr.
	 */
	ret = set_threshold(VWARN1_CFG + s_attr->nr, pos);

	return ret ? ret : count;
}

static ssize_t show_volt_thres(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret, index;
	uint8_t data;
	struct sensor_device_attribute_2 *s_attr =
					to_sensor_dev_attr_2(attr);

	mutex_lock(&ocd_update_lock);

	ret = intel_scu_ipc_ioread8(VWARN1_CFG + s_attr->nr, &data);

	mutex_unlock(&ocd_update_lock);

	if (ret)
		return ret;

	/* Read bits [0-2] of data to get the index into the array */
	index = NUM_THRESHOLDS - (data & 0x07) - 1;

	return sprintf(buf, "%lu\n", volt_thresholds[index]);
}

static ssize_t store_crit_shutdown(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	uint8_t data;
	unsigned long flag;

	if (kstrtoul(buf, 10, &flag) || (flag != 0 && flag != 1))
		return -EINVAL;

	mutex_lock(&ocd_update_lock);

	ret = intel_scu_ipc_ioread8(VCRIT_CFG, &data);
	if (ret)
		goto ipc_fail;
	/*
	 * flag:1 enables shutdown due to burst current
	 * flag:0 disables shutdown due to burst current
	 */
	if (flag)
		data |= VCRIT_SHUTDOWN;
	else
		data &= ~VCRIT_SHUTDOWN;

	ret = intel_scu_ipc_iowrite8(VCRIT_CFG, data);
	if (!ret)
		ret = count;

ipc_fail:
	mutex_unlock(&ocd_update_lock);
	return ret;
}

static ssize_t show_crit_shutdown(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int flag, ret;
	uint8_t data;

	mutex_lock(&ocd_update_lock);

	ret = intel_scu_ipc_ioread8(VCRIT_CFG, &data);
	if (!ret) {
		/* 'flag' is 1 if CRIT_SHUTDOWN is enabled, 0 otherwise */
		flag = !!(data & VCRIT_SHUTDOWN);
	}

	mutex_unlock(&ocd_update_lock);

	return ret ? ret : sprintf(buf, "%d\n", flag);
}


static ssize_t show_intr_count(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	uint32_t value;
	int level = to_sensor_dev_attr(attr)->index;

	switch (level) {
	case VWARN1:
		value = intr_count_lvl1;
		break;
	case VWARN2:
		value = intr_count_lvl2;
		break;
	case VCRIT:
		value = intr_count_lvl3;
		break;
	default:
		return -EINVAL;
	}

	return sprintf(buf, "%d\n", value);
}

static ssize_t store_camflash_ctrl(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	uint8_t value;
	if (kstrtou8(buf, 10, &value))
		return -EINVAL;

	if ((value < CAMFLASH_STATE_NORMAL) ||
		(value > CAMFLASH_STATE_CRITICAL))
		return -EINVAL;

	cam_flash_state = value;
	return count;
}

static ssize_t show_camflash_ctrl(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", cam_flash_state);
}

#ifdef CONFIG_DEBUG_FS

struct dentry *bcbcu_dbgfs_root;

static struct bcu_reg_info bcbcu_reg[] = {
	reg_info(S_BCUINT),
	reg_info(BCUIRQ),
	reg_info(IRQLVL1),
	reg_info(VWARN1_CFG),
	reg_info(VWARN2_CFG),
	reg_info(VCRIT_CFG),
	reg_info(ICCMAXVSYS_CFG),
	reg_info(ICCMAXVCC_CFG),
	reg_info(ICCMAXVNN_CFG),
	reg_info(VFLEXSRC_BEH),
	reg_info(VFLEXDIS_BEH),
	reg_info(VIBDIS_BEH),
	reg_info(CAMFLTORCH_BEH),
	reg_info(CAMFLDIS_BEH),
	reg_info(BCUDISW2_BEH),
	reg_info(BCUDISCRIT_BEH),
	reg_info(S_BCUCTRL),
	reg_info(MBCUIRQ),
	reg_info(MIRQLVL1)
};

/**
 * bcbcu_dbgfs_write - debugfs: write the new state to an endpoint.
 * @file: The seq_file to write data to.
 * @user_buf: the user data which is need to write to an endpoint
 * @count: the size of the user data
 * @pos: loff_t" is a "long offset", which is the current reading or writing
 *       position.
 *
 * Send data to the device. If NULL,-EINVAL/-EFAULT return to the write call to
 * the calling program if it is non-negative return value represents the number
 * of bytes successfully written.
 */
static ssize_t bcbcu_dbgfs_write(struct file *file,
			const char __user *user_buf, size_t count, loff_t *pos)
{
	char buf[count];
	u8 data;
	u16 addr;
	int ret;
	struct seq_file *s = file->private_data;

	if (!s) {
		ret = -EINVAL;
		goto error;
	}

	addr = *((u16 *)s->private);
	if ((addr == BCUIRQ) || (addr == S_BCUINT) || (addr == IRQLVL1)) {
		pr_err("EM_BCU: DEBUGFS no permission to write Addr(0x%04x)\n",
				addr);
		ret = -EIO;
		goto error;
	}

	if (copy_from_user(buf, user_buf, count)) {
		pr_err("EM_BCU: DEBUGFS unable to copy the user data.\n");
		ret = -EFAULT;
		goto error;
	}

	buf[count-1] = '\0';
	if (kstrtou8(buf, 16, &data)) {
		pr_err("EM_BCU: DEBUGFS invalid user data.\n");
		ret = -EINVAL;
		goto error;
	}

	ret = intel_scu_ipc_iowrite8(addr, data);
	if (ret < 0) {
		pr_err("EM_BCU: Dbgfs write error Addr: 0x%04x Data: 0x%02x\n",
				addr, data);
		goto error;
	}
	pr_debug("EM_BCU: DEBUGFS written Data: 0x%02x Addr: 0x%04x\n",
			data, addr);
	return count;

error:
	return ret;
}

/**
 * bcbcu_reg_show - debugfs: show the state of an endpoint.
 * @s: The seq_file to read data from.
 * @unused: not used
 *
 * This debugfs entry shows the content of the register
 * given in the data parameter.
 */
static int bcbcu_reg_show(struct seq_file *s, void *unused)
{
	u16 addr = 0;
	u8 data = 0;
	int ret;

	addr = *((u16 *)s->private);
	ret = intel_scu_ipc_ioread8(addr, &data);
	if (ret) {
		pr_err("EM_BCU: Error in reading 0x%04x register!!\n", addr);
		return ret;
	}
	seq_printf(s, "0x%02x\n", data);

	return 0;
}

/**
 * bcbcu_dbgfs_open - debugfs: to open the endpoint for read/write operation.
 * @inode: inode structure is used by the kernel internally to represent files.
 * @file: It is created by the kernel on open and is passed to any function
 *        that operates on the file, until the last close. After all instances
 *        of the file are closed, the kernel releases the data structure.
 *
 * This is the first operation of the files on the device, does not require the
 * driver to declare a corresponding method. If this is NULL, the device is
 * turned on has been successful, but the driver will not be notified
 */
static int bcbcu_dbgfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, bcbcu_reg_show, inode->i_private);
}

static const struct file_operations bcbcu_dbgfs_fops = {
	.owner		= THIS_MODULE,
	.open		= bcbcu_dbgfs_open,
	.release	= single_release,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.write		= bcbcu_dbgfs_write,
};

static void bcbcu_create_debugfs(struct ocd_info *info)
{
	char reg_name[MAX_REGNAME_LEN] = {0};
	u32 idx;
	u32 max_dbgfs_num = ARRAY_SIZE(bcbcu_reg);
	struct dentry *entry;

	bcbcu_dbgfs_root = debugfs_create_dir(DRIVER_NAME, NULL);
	if (IS_ERR(bcbcu_dbgfs_root)) {
		dev_warn(info->dev, "DEBUGFS directory(%s) create failed!\n",
				DRIVER_NAME);
		return;
	}

	for (idx = 0; idx < max_dbgfs_num; idx++) {
		snprintf(reg_name, MAX_REGNAME_LEN, "%s", bcbcu_reg[idx].name);
		entry = debugfs_create_file(reg_name,
						bcbcu_reg[idx].mode,
						bcbcu_dbgfs_root,
						&bcbcu_reg[idx].addr,
						&bcbcu_dbgfs_fops);
		if (IS_ERR(entry)) {
			debugfs_remove_recursive(bcbcu_dbgfs_root);
			bcbcu_dbgfs_root = NULL;
			dev_warn(info->dev, "DEBUGFS %s creation failed!!\n",
					reg_name);
			return;
		}
	}
	dev_info(info->dev, "DEBUGFS %s created successfully.\n", DRIVER_NAME);
}

static inline void bcbcu_remove_debugfs(struct ocd_info *info)
{
	if (bcbcu_dbgfs_root)
		debugfs_remove_recursive(bcbcu_dbgfs_root);
}
#else
static inline void bcbcu_create_debugfs(struct ocd_info *info) { }
static inline void bcbcu_remove_debugfs(struct ocd_info *info) { }
#endif /* CONFIG_DEBUG_FS */

/**
 * vwarn2_irq_enable_work: delayed work queue function, which is used to unmask
 * (enable) the VWARN2 interrupt after the specified delay time while sceduling.
 */
static void vwarn2_irq_enable_work(struct work_struct *work)
{
	int ret = 0;
	struct ocd_info *info = container_of(work,
						struct ocd_info,
						vwarn2_irq_work.work);

	dev_dbg(info->dev, "EM_BCU: Inside %s\n", __func__);

	/* Unmasking BCU MVWARN2 Interrupt, to see the interrupt occurrence */
	ret = intel_scu_ipc_update_register(MBCUIRQ, ~MVWARN2, MVWARN2_MASK);
	if (ret) {
		dev_err(info->dev, "EM_BCU: Error in %s updating reg 0x%x\n",
				__func__, MBCUIRQ);
	}
}

static inline struct power_supply *get_psy_battery(void)
{
	struct class_dev_iter iter;
	struct device *dev;
	static struct power_supply *pst;

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

/* Reading the Voltage now value of the battery */
static inline int bcu_get_battery_voltage(int *volt)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	psy = get_psy_battery();
	if (!psy)
		return -EINVAL;

	ret = psy->get_property(psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
	if (!ret)
		*volt = (val.intval);

	return ret;
}

static void handle_VW1_event(void *dev_data)
{
	uint8_t irq_status, beh_data;
	struct ocd_info *cinfo = (struct ocd_info *)dev_data;
	int ret;
	char *bcu_envp[2];

	dev_info(cinfo->dev, "EM_BCU: VWARN1 Event has occured\n");

	/**
	 * Notify using uevent along with env info. Here sending vwarn2 info
	 * upon receiving vwarn1 interrupt since the vwarn1 & vwarn2 threshold
	 * values is swapped.
	 */
	bcu_envp[0] = get_envp(VWARN2);
	bcu_envp[1] = NULL;
	kobject_uevent_env(&cinfo->dev->kobj, KOBJ_CHANGE, bcu_envp);

	/**
	 * Masking the BCU MVWARN1 Interrupt, since software does graceful
	 * shutdown once VWARN1 interrupt occurs. So we never expect another
	 * VWARN1 interrupt.
	 */
	ret = intel_scu_ipc_update_register(MBCUIRQ, MVWARN1, MVWARN1_MASK);
	if (ret) {
		dev_err(cinfo->dev, "EM_BCU: Error in %s updating reg 0x%x\n",
				__func__, MBCUIRQ);
		goto ipc_fail;
	}

	ret = intel_scu_ipc_ioread8(S_BCUINT, &irq_status);
	if (ret)
		goto ipc_fail;
	dev_dbg(cinfo->dev, "EM_BCU: S_BCUINT: %x\n", irq_status);

	if (!(irq_status & SVWARN1)) {
		/* Vsys is above WARN1 level */
		dev_info(cinfo->dev, "EM_BCU: Recovered from VWARN1 Level\n");
	}

	return;

ipc_fail:
	dev_err(cinfo->dev, "EM_BCU: ipc read/write failed:func:%s()\n",
								__func__);
	return;
}

static void handle_VW2_event(void *dev_data)
{
	uint8_t irq_status, beh_data;
	struct ocd_info *cinfo = (struct ocd_info *)dev_data;
	int ret;
	char *bcu_envp[2];

	dev_info(cinfo->dev, "EM_BCU: VWARN2 Event has occured\n");

	/**
	 * Notify using uevent along with env info. Here sending vwarn1 info
	 * upon receiving vwarn2 interrupt since the vwarn1 & vwarn2 threshold
	 * values is swapped.
	 */
	bcu_envp[0] = get_envp(VWARN1);
	bcu_envp[1] = NULL;
	kobject_uevent_env(&cinfo->dev->kobj, KOBJ_CHANGE, bcu_envp);

	ret = intel_scu_ipc_ioread8(S_BCUINT, &irq_status);
	if (ret)
		goto ipc_fail;
	dev_dbg(cinfo->dev, "EM_BCU: S_BCUINT: %x\n", irq_status);

	/* If Vsys is below WARN2 level-No action required from driver */
	if (!(irq_status & SVWARN2)) {
		/* Vsys is above WARN2 level */
		dev_info(cinfo->dev, "EM_BCU: Recovered from VWARN2 Level\n");

		/* clearing BCUDISW2 signal if asserted */
		ret = intel_scu_ipc_ioread8(BCUDISW2_BEH, &beh_data);
		if (ret)
			goto ipc_fail;
		if (IS_ASSRT_ON_VW2(beh_data) && IS_STICKY(beh_data)) {
			ret = intel_scu_ipc_update_register(S_BCUCTRL,
					S_BCUDISW2, S_BCUDISW2_MASK);
			if (ret)
				goto ipc_fail;
		}

		/* clearing CAMFLDIS# signal if asserted */
		ret = intel_scu_ipc_ioread8(CAMFLDIS_BEH, &beh_data);
		if (ret)
			goto ipc_fail;
		if (IS_ASSRT_ON_VW2(beh_data) && IS_STICKY(beh_data)) {
			ret = intel_scu_ipc_update_register(S_BCUCTRL,
					S_CAMFLDIS, S_CAMFLDIS_MASK);
			if (ret)
				goto ipc_fail;
		}

		/* clearing CAMFLTORCH signal if asserted */
		ret = intel_scu_ipc_ioread8(CAMFLTORCH_BEH, &beh_data);
		if (ret)
			goto ipc_fail;
		if (IS_ASSRT_ON_VW2(beh_data) && IS_STICKY(beh_data)) {
			ret = intel_scu_ipc_update_register(S_BCUCTRL,
					S_CAMFLTORCH, S_CAMFLTORCH_MASK);
			if (ret)
				goto ipc_fail;
		}
	} else {
		/**
		 * Masking BCU VWARN2 Interrupt, to avoid multiple VWARN2
		 * interrupt occurrence continuously.
		 */
		ret = intel_scu_ipc_update_register(MBCUIRQ,
							MVWARN2,
							MVWARN2_MASK);
		if (ret) {
			dev_err(cinfo->dev,
				"EM_BCU: Error in %s updating reg 0x%x\n",
				__func__, MBCUIRQ);
		}

		cancel_delayed_work_sync(&cinfo->vwarn2_irq_work);
		/**
		 * Schedule the work to re-enable the VWARN2 interrupt after
		 * 30sec delay
		 */
		schedule_delayed_work(&cinfo->vwarn2_irq_work,
					VWARN2_INTR_EN_DELAY);
	}
	return;

ipc_fail:
	dev_err(cinfo->dev, "EM_BCU: ipc read/write failed:func:%s()\n",
								__func__);
	return;
}

static void handle_VC_event(void *dev_data)
{
	struct ocd_info *cinfo = (struct ocd_info *)dev_data;
	char *bcu_envp[2];
	int ret = 0;

	dev_info(cinfo->dev, "EM_BCU: VCRIT Event has occured\n");

	/* Notify using uevent along with env info */
	bcu_envp[0] = get_envp(VCRIT);
	bcu_envp[1] = NULL;
	kobject_uevent_env(&cinfo->dev->kobj, KOBJ_CHANGE, bcu_envp);

	/**
	 * Masking BCU VCRIT Interrupt, since hardware does critical hardware
	 * shutdown once VCRIT interrupt occurs. So we never expect another
	 * VCRIT interrupt.
	 */
	ret = intel_scu_ipc_update_register(MBCUIRQ, MVCRIT, MVCRIT_MASK);
	if (ret)
		dev_err(cinfo->dev, "EM_BCU: Error in %s updating reg 0x%x\n",
			__func__, MBCUIRQ);
	return;
}

static irqreturn_t ocd_intrpt_handler(int irq, void *dev_data)
{
	return IRQ_WAKE_THREAD;
}

static irqreturn_t ocd_intrpt_thread_handler(int irq, void *dev_data)
{
	int ret;
	int bat_volt;
	unsigned int irq_data;
	struct ocd_info *cinfo = (struct ocd_info *)dev_data;

	if (!cinfo)
		return IRQ_NONE;

	mutex_lock(&ocd_update_lock);

	ret = bcu_get_battery_voltage(&bat_volt);
	if (ret)
		dev_err(cinfo->dev,
			"EM_BCU: Error in getting battery voltage\n");
	else
		dev_info(cinfo->dev, "EM_BCU: Battery Volatge = %dmV\n",
				(bat_volt/1000));

	irq_data = ioread8(cinfo->bcu_intr_addr);

	/* we are not handling(no action taken) GSMPULSE_IRQ and
						TXPWRTH_IRQ event */
	if (irq_data & VCRIT_IRQ) {
		++intr_count_lvl3;
		handle_VC_event(dev_data);
	}
	if (irq_data & VWARN2_IRQ) {
		++intr_count_lvl2;
		handle_VW2_event(dev_data);
	}
	if (irq_data & VWARN1_IRQ) {
		++intr_count_lvl1;
		handle_VW1_event(dev_data);
	}
	if (irq_data & GSMPULSE_IRQ) {
		dev_info(cinfo->dev, "EM_BCU: GSMPULSE Event has occured\n");
	}
	if (irq_data & TXPWRTH_IRQ) {
		dev_info(cinfo->dev, "EM_BCU: TXPWRTH Event has occured\n");
	}

	/* Unmask BCU Interrupt in the mask register */
	ret = intel_scu_ipc_update_register(MIRQLVL1, 0x00, BCU_ALERT);
	if (ret) {
		dev_err(cinfo->dev,
			"EM_BCU: Unmasking of BCU failed:%d\n", ret);
		goto ipc_fail;
	}

	ret = IRQ_HANDLED;

ipc_fail:
	mutex_unlock(&ocd_update_lock);
	return ret;
}

static SENSOR_DEVICE_ATTR_2(volt_warn1, S_IRUGO | S_IWUSR,
				show_volt_thres, store_volt_thres, 0, 0);
static SENSOR_DEVICE_ATTR_2(volt_warn2, S_IRUGO | S_IWUSR,
				show_volt_thres, store_volt_thres, 1, 0);
static SENSOR_DEVICE_ATTR_2(volt_crit, S_IRUGO | S_IWUSR,
				show_volt_thres, store_volt_thres, 2, 0);

static SENSOR_DEVICE_ATTR_2(core_current, S_IRUGO | S_IWUSR,
				show_curr_thres, store_curr_thres, 0, 0);
static SENSOR_DEVICE_ATTR_2(uncore_current, S_IRUGO | S_IWUSR,
				show_curr_thres, store_curr_thres, 1, 0);

static SENSOR_DEVICE_ATTR_2(enable_crit_shutdown, S_IRUGO | S_IWUSR,
				show_crit_shutdown, store_crit_shutdown, 0, 0);

static SENSOR_DEVICE_ATTR(intr_count_level1, S_IRUGO,
				show_intr_count, NULL, 0);

static SENSOR_DEVICE_ATTR(intr_count_level2, S_IRUGO,
				show_intr_count, NULL, 1);

static SENSOR_DEVICE_ATTR(intr_count_level3, S_IRUGO,
				show_intr_count, NULL, 2);

static SENSOR_DEVICE_ATTR(camflash_ctrl, S_IRUGO | S_IWUSR,
				show_camflash_ctrl, store_camflash_ctrl, 0);

static struct attribute *mrfl_ocd_attrs[] = {
	&sensor_dev_attr_core_current.dev_attr.attr,
	&sensor_dev_attr_uncore_current.dev_attr.attr,
	&sensor_dev_attr_volt_warn1.dev_attr.attr,
	&sensor_dev_attr_volt_warn2.dev_attr.attr,
	&sensor_dev_attr_volt_crit.dev_attr.attr,
	&sensor_dev_attr_enable_crit_shutdown.dev_attr.attr,
	&sensor_dev_attr_intr_count_level1.dev_attr.attr,
	&sensor_dev_attr_intr_count_level2.dev_attr.attr,
	&sensor_dev_attr_intr_count_level3.dev_attr.attr,
	&sensor_dev_attr_camflash_ctrl.dev_attr.attr,
	NULL
};

static struct attribute_group mrfl_ocd_gr = {
	.attrs = mrfl_ocd_attrs
};

static int mrfl_ocd_probe(struct platform_device *pdev)
{
	int ret;
	struct ocd_platform_data *ocd_plat_data;
	struct ocd_bcove_config_data ocd_config_data;
	struct ocd_info *cinfo = devm_kzalloc(&pdev->dev,
			sizeof(struct ocd_info), GFP_KERNEL);

	if (!cinfo) {
		dev_err(&pdev->dev, "kzalloc failed\n");
		return -ENOMEM;
	}
	cinfo->pdev = pdev;
	cinfo->irq = platform_get_irq(pdev, 0);
	platform_set_drvdata(pdev, cinfo);

	/* Creating a sysfs group with mrfl_ocd_gr attributes */
	ret = sysfs_create_group(&pdev->dev.kobj, &mrfl_ocd_gr);
	if (ret) {
		dev_err(&pdev->dev, "sysfs create group failed\n");
		goto exit_free;
	}

	/* Registering with hwmon class */
	cinfo->dev = hwmon_device_register(&pdev->dev);
	if (IS_ERR(cinfo->dev)) {
		ret = PTR_ERR(cinfo->dev);
		cinfo->dev = NULL;
		dev_err(&pdev->dev, "hwmon_dev_regs failed\n");
		goto exit_sysfs;
	}

	cinfo->bcu_intr_addr = ioremap_nocache(PMIC_SRAM_BCU_ADDR, IOMAP_LEN);
	if (!cinfo->bcu_intr_addr) {
		ret = -ENOMEM;
		dev_err(&pdev->dev, "ioremap_nocache failed\n");
		goto exit_hwmon;
	}

	/* Unmask 1st level BCU interrupt in the mask register */
	ret = intel_scu_ipc_update_register(MIRQLVL1, 0x00, BCU_ALERT);
	if (ret) {
		dev_err(&pdev->dev,
			"EM_BCU: Unmasking of BCU failed:%d\n", ret);
		goto exit_ioremap;
	}

	/* Register for Interrupt Handler */
	ret = request_threaded_irq(cinfo->irq, ocd_intrpt_handler,
						ocd_intrpt_thread_handler,
						IRQF_NO_SUSPEND,
						DRIVER_NAME, cinfo);
	if (ret) {
		dev_err(&pdev->dev,
			"EM_BCU: request_threaded_irq failed:%d\n", ret);
		goto exit_ioremap;
	}

	/*Read BCU configuration values from smip*/
	ocd_plat_data = pdev->dev.platform_data;

	ret = ocd_plat_data->bcu_config_data(&ocd_config_data);
	if (ret) {
		dev_err(&pdev->dev, "EM_BCU: Read SMIP failed:%d\n", ret);
		goto exit_freeirq;
	}

	/* Program the BCU with default values read from the smip*/
	ret = program_bcu(&ocd_config_data);
	if (ret) {
		dev_err(&pdev->dev, "EM_BCU: program_bcu() failed:%d\n", ret);
		goto exit_freeirq;
	}

	enable_volt_trip_points();
	enable_current_trip_points();
	cam_flash_state = CAMFLASH_STATE_NORMAL;

	/* Initializing delayed work for re-enabling vwarn1 interrupt */
	INIT_DELAYED_WORK(&cinfo->vwarn2_irq_work, vwarn2_irq_enable_work);

	/* Create debufs for the basincove bcu registers */
	bcbcu_create_debugfs(cinfo);

	return 0;

exit_freeirq:
	free_irq(cinfo->irq, cinfo);
exit_ioremap:
	iounmap(cinfo->bcu_intr_addr);
exit_hwmon:
	hwmon_device_unregister(cinfo->dev);
exit_sysfs:
	sysfs_remove_group(&pdev->dev.kobj, &mrfl_ocd_gr);
exit_free:
	kfree(cinfo);
	return ret;
}

static int mrfl_ocd_resume(struct device *dev)
{
	dev_info(dev, "Resume called.\n");
	return 0;
}

static int mrfl_ocd_suspend(struct device *dev)
{
	dev_info(dev, "Suspend called.\n");
	return 0;
}

static int mrfl_ocd_remove(struct platform_device *pdev)
{
	struct ocd_info *cinfo = platform_get_drvdata(pdev);

	if (cinfo) {
		flush_scheduled_work();
		free_irq(cinfo->irq, cinfo);
		iounmap(cinfo->bcu_intr_addr);
		bcbcu_remove_debugfs(cinfo);
		hwmon_device_unregister(cinfo->dev);
		sysfs_remove_group(&pdev->dev.kobj, &mrfl_ocd_gr);
		kfree(cinfo);
	}
	return 0;
}

/*********************************************************************
 *		Driver initialisation and finalization
 *********************************************************************/

static const struct dev_pm_ops mrfl_ocd_pm_ops = {
	.suspend = mrfl_ocd_suspend,
	.resume = mrfl_ocd_resume,
};

static const struct platform_device_id mrfl_ocd_table[] = {
	{DRIVER_NAME, 1 },
};

static struct platform_driver mrfl_over_curr_detect_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.pm = &mrfl_ocd_pm_ops,
		},
	.probe = mrfl_ocd_probe,
	.remove = mrfl_ocd_remove,
	.id_table = mrfl_ocd_table,
};

static int mrfl_ocd_module_init(void)
{
	return platform_driver_register(&mrfl_over_curr_detect_driver);
}

static void mrfl_ocd_module_exit(void)
{
	platform_driver_unregister(&mrfl_over_curr_detect_driver);
}

/* RPMSG related functionality */
static int mrfl_ocd_rpmsg_probe(struct rpmsg_channel *rpdev)
{
	int ret = 0;
	if (rpdev == NULL) {
		pr_err("rpmsg channel not created\n");
		ret = -ENODEV;
		goto out;
	}

	dev_info(&rpdev->dev, "Probed mrfl_ocd rpmsg device\n");

	ret = mrfl_ocd_module_init();
out:
	return ret;
}

static void mrfl_ocd_rpmsg_remove(struct rpmsg_channel *rpdev)
{
	mrfl_ocd_module_exit();
	dev_info(&rpdev->dev, "Removed mrfl_ocd rpmsg device\n");
}

static void mrfl_ocd_rpmsg_cb(struct rpmsg_channel *rpdev, void *data,
			int len, void *priv, u32 src)
{
	dev_warn(&rpdev->dev, "unexpected, message\n");

	print_hex_dump(KERN_DEBUG, __func__, DUMP_PREFIX_NONE, 16, 1,
				data, len, true);
}

static struct rpmsg_device_id mrfl_ocd_id_table[] = {
	{ .name = "rpmsg_mrfl_ocd" },
	{ },
};

MODULE_DEVICE_TABLE(rpmsg, mrfl_ocd_id_table);

static struct rpmsg_driver mrfl_ocd_rpmsg = {
	.drv.name	= DRIVER_NAME,
	.drv.owner	= THIS_MODULE,
	.probe		= mrfl_ocd_rpmsg_probe,
	.callback	= mrfl_ocd_rpmsg_cb,
	.remove		= mrfl_ocd_rpmsg_remove,
	.id_table	= mrfl_ocd_id_table,
};

static int __init mrfl_ocd_rpmsg_init(void)
{
	return register_rpmsg_driver(&mrfl_ocd_rpmsg);
}

static void __init mrfl_ocd_rpmsg_exit(void)
{
	unregister_rpmsg_driver(&mrfl_ocd_rpmsg);
}

module_init(mrfl_ocd_rpmsg_init);
module_exit(mrfl_ocd_rpmsg_exit);

MODULE_AUTHOR("Durgadoss R <durgadoss.r@intel.com>");
MODULE_DESCRIPTION("Intel Merrifield Over Current Detection Driver");
MODULE_LICENSE("GPL");
