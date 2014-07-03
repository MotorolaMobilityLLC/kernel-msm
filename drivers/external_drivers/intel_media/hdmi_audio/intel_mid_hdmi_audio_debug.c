/*
 *   intel_mid_hdmi_audio_debug.c - Debug interface for Intel HDMI audio
 *                                  driver for MID
 *
 *  Copyright (C) 2014 Intel Corp
 *  Authors:	Jayachandran.B  <jayachandran.b@intel.com>
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Debug interface code for Intel MID HDMI audio Driver
 */

#define pr_fmt(fmt)	"had_debug: " fmt
#include <linux/debugfs.h>
#include "intel_mid_hdmi_audio.h"

#ifdef CONFIG_DEBUG_FS

extern struct snd_intelhad *had_data;

struct had_debug {
	const char *name;
	const struct file_operations *fops;
	umode_t mode;
};

struct had_reg_offset_info {
	const char *name;
	u32 offset;
};
/* HDMI controller register offsets */
static struct had_reg_offset_info had_debug_reg_offset_table_v1[] = {
	{ "AUD_CONFIG",		AUD_CONFIG },
	{ "AUD_CH_STATUS_0",	AUD_CH_STATUS_0 },
	{ "AUD_CH_STATUS_1",	AUD_CH_STATUS_1 },
	{ "AUD_HDMI_CTS",	AUD_HDMI_CTS },
	{ "AUD_N_ENABLE",	AUD_N_ENABLE },
	{ "AUD_SAMPLE_RATE",	AUD_SAMPLE_RATE },
	{ "AUD_BUF_CONFIG",	AUD_BUF_CONFIG },
	{ "AUD_BUF_CH_SWAP",	AUD_BUF_CH_SWAP },
	{ "AUD_BUF_A_ADDR",	AUD_BUF_A_ADDR },
	{ "AUD_BUF_A_LENGTH",	AUD_BUF_A_LENGTH },
	{ "AUD_BUF_B_ADDR",	AUD_BUF_B_ADDR },
	{ "AUD_BUF_B_LENGTH",	AUD_BUF_B_LENGTH },
	{ "AUD_BUF_C_ADDR",	AUD_BUF_C_ADDR },
	{ "AUD_BUF_C_LENGTH",	AUD_BUF_C_LENGTH },
	{ "AUD_BUF_D_ADDR",	AUD_BUF_D_ADDR },
	{ "AUD_BUF_D_LENGTH",	AUD_BUF_D_LENGTH },
	{ "AUD_CNTL_ST",	AUD_CNTL_ST },
	{ "AUD_HDMI_STATUS",	AUD_HDMI_STATUS },
	{ "AUD_HDMIW_INFOFR",	AUD_HDMIW_INFOFR },
};

static struct had_reg_offset_info had_debug_reg_offset_table_v2[] = {
	{ "AUD_CONFIG",		AUD_CONFIG },
	{ "AUD_CH_STATUS_0",	AUD_CH_STATUS_0 },
	{ "AUD_CH_STATUS_1",	AUD_CH_STATUS_1 },
	{ "AUD_HDMI_CTS",	AUD_HDMI_CTS },
	{ "AUD_N_ENABLE",	AUD_N_ENABLE },
	{ "AUD_SAMPLE_RATE",	AUD_SAMPLE_RATE },
	{ "AUD_BUF_CONFIG",	AUD_BUF_CONFIG },
	{ "AUD_BUF_CH_SWAP",	AUD_BUF_CH_SWAP },
	{ "AUD_BUF_A_ADDR",	AUD_BUF_A_ADDR },
	{ "AUD_BUF_A_LENGTH",	AUD_BUF_A_LENGTH },
	{ "AUD_BUF_B_ADDR",	AUD_BUF_B_ADDR },
	{ "AUD_BUF_B_LENGTH",	AUD_BUF_B_LENGTH },
	{ "AUD_BUF_C_ADDR",	AUD_BUF_C_ADDR },
	{ "AUD_BUF_C_LENGTH",	AUD_BUF_C_LENGTH },
	{ "AUD_BUF_D_ADDR",	AUD_BUF_D_ADDR },
	{ "AUD_BUF_D_LENGTH",	AUD_BUF_D_LENGTH },
	{ "AUD_CNTL_ST",	AUD_CNTL_ST },
	{ "AUD_HDMI_STATUS",	AUD_HDMI_STATUS_v2 },
	{ "AUD_HDMIW_INFOFR",	AUD_HDMIW_INFOFR_v2 },
};

static int had_debugfs_read_register(uint32_t *data)
{
	struct snd_intelhad *intelhaddata = had_data;
	u32 offset = intelhaddata->debugfs.reg_offset;
	u32 base = intelhaddata->audio_reg_base;
	int retval;

	retval = had_read_register(offset, data);
	if (retval)
		pr_err("Failed reading hdmi reg %x\n", base + offset);
	else
		pr_debug("Reading reg %x : value = %x\n", base + offset, *data);

	return retval;
}

static int had_debugfs_write_register(uint32_t data)
{
	struct snd_intelhad *intelhaddata = had_data;
	u32 offset = intelhaddata->debugfs.reg_offset;
	u32 base = intelhaddata->audio_reg_base;
	int retval;

	retval = had_write_register(offset, data);
	if (retval)
		pr_err("Failed writing hdmi reg %x\n", base + offset);
	else
		pr_debug("Writing reg %x : value = %x\n", base + offset, data);

	add_taint(TAINT_USER, LOCKDEP_STILL_OK);

	return retval;
}

static ssize_t had_debug_reg_base_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[32];
	u32 reg_base;
	struct snd_intelhad *intelhaddata = had_data;
	size_t buf_size = min(count, sizeof(buf)-1);

	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	buf[buf_size] = 0;
	if (kstrtouint(buf, 0, &reg_base))
		return -EINVAL;

	intelhaddata->audio_reg_base = reg_base;

	pr_debug("wrote hdmi_audio reg_base = %x\n", (int) intelhaddata->audio_reg_base);
	return buf_size;
}

static ssize_t had_debug_reg_base_read(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos)
{
	struct snd_intelhad *intelhaddata = had_data;
	char buf[32];
	u32 reg_base = intelhaddata->audio_reg_base;

	snprintf(buf, 32, "0x%08x\n", reg_base);
	return simple_read_from_buffer(user_buf, count, ppos,
			buf, strlen(buf));

}

static const struct file_operations had_debug_reg_base_ops = {
	.open = simple_open,
	.write = had_debug_reg_base_write,
	.read = had_debug_reg_base_read,
};

static ssize_t had_debug_reg_offset_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[32];
	u32 reg_offset;
	struct snd_intelhad *intelhaddata = had_data;
	size_t buf_size = min(count, sizeof(buf)-1);

	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	buf[buf_size] = 0;
	if (kstrtouint(buf, 0, &reg_offset))
		return -EINVAL;

	intelhaddata->debugfs.reg_offset = reg_offset;

	pr_info("wrote hdmi_audio reg_offset = %x\n", (int) intelhaddata->debugfs.reg_offset);
	return buf_size;
}

static ssize_t had_debug_reg_offset_read(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos)
{
	struct snd_intelhad *intelhaddata = had_data;
	char buf[32];
	u32 reg_offset = intelhaddata->debugfs.reg_offset;

	snprintf(buf, 32, "0x%08x\n", reg_offset);
	return simple_read_from_buffer(user_buf, count, ppos,
			buf, strlen(buf));
}

static const struct file_operations had_debug_reg_offset_ops = {
	.open = simple_open,
	.write = had_debug_reg_offset_write,
	.read = had_debug_reg_offset_read,
};

static ssize_t had_debug_reg_val_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	u32 data;
	int retval;
	char buf[32];
	size_t buf_size = min(count, sizeof(buf)-1);

	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	buf[buf_size] = 0;
	if (kstrtouint(buf, 0, &data))
		return -EINVAL;

	retval = had_debugfs_write_register(data);

	if (retval)
		return retval;

	return buf_size;
}

static ssize_t had_debug_reg_val_read(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos)
{
	u32 data;
	int retval;
	char buf[32];

	retval = had_debugfs_read_register(&data);
	if (retval)
		return retval;

	snprintf(buf, 32, "0x%08x\n", data);
	return simple_read_from_buffer(user_buf, count, ppos,
			buf, strlen(buf));
}

static const struct file_operations had_debug_reg_val_ops = {
	.open = simple_open,
	.write = had_debug_reg_val_write,
	.read = had_debug_reg_val_read,
};

/* max number of chars in the register details printed for a given register
 * in the format : name = 0x<u32 value>
 * assuming name is 16 chars long, total = 29 chars; With additional 11
 * reserved chars incluing newline; total = 40 chars per register
 */
#define HAD_DEBUG_REG_DETAILS_SIZE      40
static ssize_t had_debug_reg_all_read(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos)
{
	u32 data;
	int retval;
	char *buf;
	int i;
	u32 offset;
	const char *name;
	int pos = 0;
	struct snd_intelhad *intelhaddata = had_data;
	struct had_reg_offset_info *info;
	int buf_size = intelhaddata->debugfs.reg_offset_table_size
		* HAD_DEBUG_REG_DETAILS_SIZE;

	info = (struct had_reg_offset_info *)
		intelhaddata->debugfs.reg_offset_table;
	if (!info) {
		pr_err("%s: offset table null\n", __func__);
		return -EINVAL;
	}

	buf = kmalloc(buf_size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	buf[0] = 0;

	for (i = 0; i < intelhaddata->debugfs.reg_offset_table_size; i++) {
		offset = info[i].offset;
		name   = info[i].name;
		intelhaddata->debugfs.reg_offset = offset;
		data = 0xDEADBEAD;
		had_debugfs_read_register(&data);

		pos += snprintf(buf+pos, HAD_DEBUG_REG_DETAILS_SIZE,
				"%s = 0x%08x\n", name, data);
	}

	retval = simple_read_from_buffer(user_buf, count, ppos, buf, pos);
	kfree(buf);
	return retval;

}

static const struct file_operations had_debug_reg_all_ops = {
	.open = simple_open,
	.read = had_debug_reg_all_read,
};


static ssize_t had_debug_readme_read(struct file *file, char __user *user_buf,
		size_t count, loff_t *ppos)
{
	const char *buf =
		"\nAll files can be read using 'cat'\n"
		"\n1.reg_base : HDMI audio register base depending on the\n"
		"pipe id used for HDMI\n"
		"To set : echo 0x..... > reg_base\n"
		"\n2.reg_offset : Offset of a particular register from the\n"
		"register base\n"
		"To set : echo 0x..... > reg_offset\n"
		"\n3.reg_val : value of the register at offset 'reg_offset'\n"
		"from 'reg_base' or default register base\n"
		"To set : echo 0x..... > reg_val\n"
		"\nTo Read/write a particular register :\n"
		"i) Set reg_base if you want to change the reg_base.\n"
		"Otherwise the default for the platform will be used\n"
		"ii) Set reg_offset to the offset of the particular register\n"
		"iii) To read, use 'cat reg_val'\n"
		"     To write a value, use echo 0xvalue > reg_val\n"
		" Eg. to read register at offset 0x10\n"
		"echo 0x10 > reg_offset\n"
		"cat reg_val\n"
		" Eg. to write value 0xabcd to register at offset 0x10\n"
		"echo 0x10 > reg_offset\n"
		"echo 0xabcd > reg_val\n"
		"\n4.reg_all : This is to read in one shot all the registers\n"
		"at respective offsets from 'reg_base' or default reg base\n"
		"Only reading of all registers (cat reg_all) is supported.\n"
		"Writing to all registers in one shot is not supported\n";

	return simple_read_from_buffer(user_buf, count, ppos,
			buf, strlen(buf));
}

static const struct file_operations had_debug_readme_ops = {
	.open = simple_open,
	.read = had_debug_readme_read,
};

static const struct had_debug dbg_entries[] = {
	{"reg_base", &had_debug_reg_base_ops, 0600},
	{"reg_offset", &had_debug_reg_offset_ops, 0600},
	{"reg_val", &had_debug_reg_val_ops, 0600},
	{"reg_all", &had_debug_reg_all_ops, 0400},
	{"README", &had_debug_readme_ops, 0400},
};

static void had_debugfs_create_files(struct snd_intelhad *intelhaddata,
		const struct had_debug *entries, int size)
{
	int i;

	for (i = 0; i < size; i++) {
		struct dentry *dentry;
		const struct had_debug *entry = &entries[i];

		dentry = debugfs_create_file(entry->name, entry->mode,
				intelhaddata->debugfs.root, NULL, entry->fops);
		if (dentry == NULL) {
			pr_err("Failed to create %s file\n", entry->name);
			return;
		}
	}
}

void had_debugfs_init(struct snd_intelhad *intelhaddata, char *version)
{

	intelhaddata->debugfs.root = debugfs_create_dir("hdmi-audio", NULL);
	if (IS_ERR(intelhaddata->debugfs.root) || !intelhaddata->debugfs.root) {
		pr_err("Failed to create hdmi audio debugfs directory\n");
		return;
	}

	had_debugfs_create_files(intelhaddata, dbg_entries,
			ARRAY_SIZE(dbg_entries));

	intelhaddata->debugfs.reg_offset_table = NULL;

	if (!version) {
		pr_err("%s:hdmi ops/Register set version null\n", __func__);
		return;
	}

	if (!strncmp(version, "v1", 2)) {
		intelhaddata->debugfs.reg_offset_table =
			(void *)had_debug_reg_offset_table_v1;
		intelhaddata->debugfs.reg_offset_table_size =
			ARRAY_SIZE(had_debug_reg_offset_table_v1);
	} else if (!strncmp(version, "v2", 2)) {
		intelhaddata->debugfs.reg_offset_table =
			(void *)had_debug_reg_offset_table_v2;
		intelhaddata->debugfs.reg_offset_table_size =
			ARRAY_SIZE(had_debug_reg_offset_table_v2);
	} else
		pr_err("%s:invalid hdmi ops/Register set version\n", __func__);
}

void had_debugfs_exit(struct snd_intelhad *intelhaddata)
{
	debugfs_remove_recursive(intelhaddata->debugfs.root);
}
#endif
