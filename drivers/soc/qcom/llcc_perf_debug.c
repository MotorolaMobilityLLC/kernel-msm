// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/device.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/soc/qcom/llcc-qcom.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/init.h>

#define PCB_COUNT                     320
#define PCBs_PER_REG                  16
#define PCB_NAME_LEN                  20
#define REG_COUNT                     (PCB_COUNT/PCBs_PER_REG)
#define LLCC_PCB_STATUS               0x25AA6698
#define MMIO_SIZE                     0x400
#define REG_STRIDE                    4
#define PCB_BITS                      2
#define PCB_MASK                      ((1U << PCB_BITS) - 1)
#define PCB_SHIFT(j)                  ((j) * PCB_BITS)
#define LLCC_TRP_ADR_CTRL_CFG         0x25A452A8
#define LLCC_TRP_ADR_CTRL_IDLE_CNTR_CFG  0x25A452AC

static struct dentry *dir_entry, *dir_adr_entry;
static void __iomem  *status_reg_base, *adr_cmd, *adr_idle_thres_cmd;
static struct regmap *regmap, *adr_idle_thres_regmap_cmd, *adr_regmap_cmd;
static u8 status[PCB_COUNT];
static DEFINE_MUTEX(llcc_debug_lock);

static struct regmap_config regmap_debug_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.fast_io = true,
};

static void status_reg_fun(void)
{
	int index = 0, offset = 0, ret, i, j, reg_val = 0;

	mutex_lock(&llcc_debug_lock);
	for (i = 0; i < REG_COUNT; i++) {
		offset = i*REG_STRIDE;
		ret = regmap_read(regmap, offset, &reg_val);
		if (ret) {
			pr_err("failed to read register at offset 0x%x\n", LLCC_PCB_STATUS+offset);
			continue;
		}
		for (j = 0; j < PCBs_PER_REG; j++) {
			index = i*PCBs_PER_REG+j;
			status[index] = (reg_val >> PCB_SHIFT(j)) & PCB_MASK;
		}
	}
	mutex_unlock(&llcc_debug_lock);
}

static ssize_t get_pcb_status(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	int pcb_index, len, ret;
	char *status_buf;
	ssize_t bufsz = 64;

	pcb_index = (int)(uintptr_t)file_inode(file)->i_private;
	if ((pcb_index < 0) || (pcb_index >= PCB_COUNT))
		return -EINVAL;

	status_buf = kmalloc(bufsz, GFP_KERNEL);
	if (!status_buf)
		return -ENOMEM;

	status_reg_fun();
	len = snprintf(status_buf, bufsz, "Status of  pcb%d : %d\n",  pcb_index,
	status[pcb_index]);

	ret = simple_read_from_buffer(user_buf, count, ppos, status_buf, len);
	kfree(status_buf);
	return ret;
}

static ssize_t get_status_all(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	int i, len = 0;
	char *pcb_buf;
	ssize_t ret;

	pcb_buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!pcb_buf)
		return -ENOMEM;
	status_reg_fun();
	for (i = 0; i < PCB_COUNT && len < PAGE_SIZE-32; i++)
		len += snprintf(pcb_buf+len, PAGE_SIZE-len, "pcb %d:%d\n", i, status[i]);

	ret = simple_read_from_buffer(user_buf, count, ppos, pcb_buf, len);
	kfree(pcb_buf);
	return ret;
}

static ssize_t set_adr_hysteresis_value(struct file *file, const char __user *user_buf,
			size_t count, loff_t *ppos)
{
	unsigned long  value = 0;
	int reg_val, ret;
	char kbuf[10];
	size_t copy_size;

	if (count == 0)
		return -EINVAL;

	copy_size = min(count, sizeof(kbuf) - 1);
	if (copy_from_user(kbuf, user_buf, copy_size))
		return -EFAULT;

	kbuf[copy_size] = '\0';
	if (kstrtoul(kbuf, 0, &value))
		return -EINVAL;

	if (value > 0xFFF) {
		pr_err("Invalid threshold value: 0x%lx\n", value);
		return -EINVAL;
	}

	ret = regmap_read(adr_regmap_cmd, 0, &reg_val);
	if (ret) {
		pr_err("Failed to read ADR control register: %d\n", ret);
		return ret;
	}
	if (reg_val) {
		ret = regmap_write(adr_idle_thres_regmap_cmd, 0, value);
		if (ret) {
			pr_err("Failed to write ADR threshold: %d\n", ret);
			return ret;
		}
	} else
		pr_err("ADR is not enabled\n");

	return count;
}

static ssize_t get_adr_hysteresis_value(struct file *file, char __user *user_buf,
					size_t count, loff_t *ppos)
{
	int len, reg_val, ret;
	char *buf;
	ssize_t bufsz = 64;

	buf = kmalloc(bufsz, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	regmap_read(adr_idle_thres_regmap_cmd, 0, &reg_val);
	len = scnprintf(buf, bufsz, "thresh value : 0x%x\n", reg_val);
	ret = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);
	return ret;
}

static const struct file_operations status_fops = {
	.open = simple_open,
	.read = get_status_all,
};

static const struct file_operations fops = {
	.open = simple_open,
	.read = get_pcb_status,
};

static const struct file_operations adr_fops = {
	.open = simple_open,
	.write = set_adr_hysteresis_value,
	.read = get_adr_hysteresis_value,
};

static int llcc_qcom_create_fs_entries(void)
{
	static struct dentry *ret, *pcb_dir;
	int i;
	char *pcb_name;

	dir_entry = debugfs_create_dir("llcc_power_debug", NULL);
	if (IS_ERR(dir_entry))
		return PTR_ERR(dir_entry);

	for (i = 0; i < PCB_COUNT; i++) {
		pcb_name = kmalloc(PCB_NAME_LEN, GFP_KERNEL);
		if (!pcb_name)
			goto error_cleanup;

		snprintf(pcb_name, PCB_NAME_LEN, "PCB%d", i);
		pcb_dir = debugfs_create_dir(pcb_name, dir_entry);
		if (IS_ERR(pcb_dir)) {
			kfree(pcb_name);
			goto error_cleanup;
		}
		kfree(pcb_name);

		ret = debugfs_create_file("status", 0444, pcb_dir, (void *)(uintptr_t)i, &fops);
		if (IS_ERR(ret))
			goto error_cleanup;
	}
	ret = debugfs_create_file("status_all", 0444, dir_entry, NULL, &status_fops);
		if (IS_ERR(ret))
			goto error_cleanup;

	dir_adr_entry = debugfs_create_dir("adr", dir_entry);
	ret = debugfs_create_file("idle-threshold", 0600, dir_adr_entry, NULL, &adr_fops);
		if (IS_ERR(ret))
			goto error_cleanup;
	return 0;

error_cleanup:
	debugfs_remove_recursive(dir_entry);
	return -ENOENT;
}

static int __init qcom_llcc_debug_init(void)
{
	int ret;

	status_reg_base = ioremap(LLCC_PCB_STATUS, MMIO_SIZE);
	if (!status_reg_base) {
		pr_err("failed to map physical region\n");
		return -ENOMEM;
	}

	regmap = regmap_init_mmio(NULL, status_reg_base, &regmap_debug_config);
	if (IS_ERR(regmap)) {
		pr_err("failed to initialize regmap\n");
		ret = PTR_ERR(regmap);
		goto err_unmap_status;
	}

	adr_cmd = ioremap(LLCC_TRP_ADR_CTRL_CFG, MMIO_SIZE);
	if (!adr_cmd) {
		ret = -ENOMEM;
		goto err_exit_regmap;
	}

	adr_regmap_cmd = regmap_init_mmio(NULL, adr_cmd, &regmap_debug_config);
	if (IS_ERR(adr_regmap_cmd)) {
		ret = PTR_ERR(adr_regmap_cmd);
		goto err_unmap_adr;
	}

	adr_idle_thres_cmd = ioremap(LLCC_TRP_ADR_CTRL_IDLE_CNTR_CFG, MMIO_SIZE);
	if (!adr_idle_thres_cmd) {
		ret = -ENOMEM;
		goto err_exit_adr_regmap;
	}

	adr_idle_thres_regmap_cmd = regmap_init_mmio(NULL, adr_idle_thres_cmd,
			&regmap_debug_config);
	if (IS_ERR(adr_idle_thres_regmap_cmd)) {
		ret = PTR_ERR(adr_idle_thres_regmap_cmd);
		goto err_unmap_idle_thres;
	}

	ret = llcc_qcom_create_fs_entries();
	if (ret)
		goto err_exit_idle_regmap;

	pr_debug("module loaded for debug entries\n");
	return 0;

err_exit_idle_regmap:
	regmap_exit(adr_idle_thres_regmap_cmd);
err_unmap_idle_thres:
	iounmap(adr_idle_thres_cmd);
err_exit_adr_regmap:
	regmap_exit(adr_regmap_cmd);
err_unmap_adr:
	iounmap(adr_cmd);
err_exit_regmap:
	regmap_exit(regmap);
err_unmap_status:
	iounmap(status_reg_base);
	return ret;
}

static void __exit qcom_llcc_debug_exit(void)
{
	debugfs_remove_recursive(dir_entry);
	regmap_exit(adr_idle_thres_regmap_cmd);
	iounmap(adr_idle_thres_cmd);
	regmap_exit(adr_regmap_cmd);
	iounmap(adr_cmd);
	regmap_exit(regmap);
	iounmap(status_reg_base);
}

module_init(qcom_llcc_debug_init);
module_exit(qcom_llcc_debug_exit);

MODULE_DESCRIPTION("Qualcomm LLCC Debug Driver");
MODULE_LICENSE("GPL");
