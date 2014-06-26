/*
 * intel_scu_mip.c: Driver for the Intel scu mip and umip access
 *
 * (C) Copyright 2012 Intel Corporation
 * Author: Shijie Zhang (shijie.zhang@intel.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/rpmsg.h>
#include <linux/blkdev.h>
#include <linux/pagemap.h>
#include <asm/intel_scu_ipc.h>
#include <asm/intel_mip.h>
#include <asm/intel_mid_rpmsg.h>
#include <linux/platform_data/intel_mid_remoteproc.h>

#define DRIVER_NAME "intel_scu_mip"

#define IPC_MIP_BASE     0xFFFD8000	/* sram base address for mip accessing*/
#define IPC_MIP_MAX_ADDR 0x1000

#define KOBJ_MIP_ATTR(_name, _mode, _show, _store) \
	struct kobj_attribute _name##_attr = __ATTR(_name, _mode, _show, _store)

static struct kobject *scu_mip_kobj;
static struct rpmsg_instance *mip_instance;
static struct scu_mip_platform_data *pdata;

static void __iomem *intel_mip_base;
#define SECTOR_SIZE			512
#define UMIP_TOTAL_CHKSUM_ENTRY		126
#define UMIP_HEADER_HEADROOM_SECTOR	1
#define UMIP_HEADER_SECTOR		0
#define UMIP_HEADER_CHKSUM_ADDR		7
#define UMIP_START_CHKSUM_ADDR		8
#define UMIP_TOTAL_HEADER_SECTOR_NO	2

#define UMIP_BLKDEVICE			"mmcblk0boot0"

static int xorblock(u32 *buf, u32 size)
{
	u32 cs = 0;

	size >>= 2;
	while (size--)
		cs ^= *buf++;

	return cs;
}

static u8 dword_to_byte_chksum(u32 dw)
{
	int n = 0;
	u32 cs = dw;
	for (n = 0; n < 3; n++) {
		dw >>= 8;
		cs ^= dw;
	}

	return (u8)cs;
}

static u8 calc_checksum(void *_buf, int size)
{
	int i;
	u8 checksum = 0, *buf = (u8 *)_buf;

	for (i = 0; i < size; i++)
		checksum = checksum ^ (buf[i]);

	return checksum;
}

static int mmcblk0boot0_match(struct device *dev, const void *data)
{
	if (strcmp(dev_name(dev), UMIP_BLKDEVICE) == 0)
		return 1;

	return 0;
}

static struct block_device *get_emmc_bdev(void)
{
	struct block_device *bdev;
	struct device *emmc_disk;

	emmc_disk = class_find_device(&block_class, NULL, NULL,
					mmcblk0boot0_match);
	if (emmc_disk == 0) {
		pr_err("emmc not found!\n");
		return NULL;
	}

	/* partition 0 means raw disk */
	bdev = bdget_disk(dev_to_disk(emmc_disk), 0);
	if (bdev == NULL) {
		dev_err(emmc_disk, "unable to get disk\n");
		return NULL;
	}

	/* Note: this bdev ref will be freed after first
	 * bdev_get/bdev_put cycle
	 */

	return bdev;
}


static int read_mip(u8 *data, int len, int offset, int issigned)
{
	int ret;
	u32 sptr, dptr, cmd, cmdid, data_off;

	dptr = offset;
	sptr = (len + 3) / 4;

	cmdid = issigned ? IPC_CMD_SMIP_RD : IPC_CMD_UMIP_RD;
	cmd = 4 << 16 | cmdid << 12 | IPCMSG_MIP_ACCESS;

	ret = rpmsg_send_raw_command(mip_instance, cmd, 0, NULL,
		(u32 *)&data_off, 0, 1, sptr, dptr);


	if (!ret)
		memcpy(data, intel_mip_base + data_off, len);

	return ret;
}

int intel_scu_ipc_read_mip(u8 *data, int len, int offset, int issigned)
{
	int ret = 0;
	Sector sect;
	struct block_device *bdev;
	char *buffer = NULL;
	int *holderId = NULL;
	int sect_no, remainder;

	/* Only SMIP read for Cloverview is supported */
	if ((intel_mid_identify_cpu() == INTEL_MID_CPU_CHIP_CLOVERVIEW)
			&& (issigned != 1)) { /* CTP read UMIP from eMMC */

		/* Opening the mmcblk0boot0 */
		bdev = get_emmc_bdev();
		if (bdev == NULL) {
			pr_err("%s: get_emmc failed!\n", __func__);
			return -ENODEV;
		}

		/* make sure the block device is open read only */
		ret = blkdev_get(bdev, FMODE_READ, holderId);
		if (ret < 0) {
			pr_err("%s: blk_dev_get failed!\n", __func__);
			return -ret;
		}

		/* Get sector number of where data located */
		sect_no = offset / SECTOR_SIZE;
		remainder = offset % SECTOR_SIZE;
		buffer = read_dev_sector(bdev, sect_no +
					UMIP_HEADER_HEADROOM_SECTOR, &sect);

		/* Shouldn't need to access UMIP sector 0/1 */
		if (sect_no < UMIP_TOTAL_HEADER_SECTOR_NO) {
			pr_err("invalid umip offset\n");
			ret = -EINVAL;
			goto bd_put;
		} else if (data == NULL || buffer == NULL) {
			pr_err("buffer is empty\n");
			ret = -ENODEV;
			goto bd_put;
		} else if (len > (SECTOR_SIZE - remainder)) {
			pr_err("not enough data to read\n");
			ret = -EINVAL;
			goto bd_put;
		}

		memcpy(data, buffer + remainder, len);
bd_put:
		if (buffer)
			put_dev_sector(sect);

		blkdev_put(bdev, FMODE_READ);
		return ret;
	} else {

		if (!intel_mip_base)
			return -ENODEV;

		if (offset + len > IPC_MIP_MAX_ADDR)
			return -EINVAL;

		rpmsg_global_lock();
		ret = read_mip(data, len, offset, issigned);
		rpmsg_global_unlock();

		return ret;
	}
}
EXPORT_SYMBOL(intel_scu_ipc_read_mip);

int get_smip_property_by_name(enum platform_prop pp)
{
	u8 data[SMIP_MAX_PROP_LEN];
	int i, val, ret;
	struct smip_platform_prop prop[SMIP_NUM_CONFIG_PROPS];

	if (!pdata->smip_prop)
		return -EINVAL;

	for (i = 0; i < SMIP_NUM_CONFIG_PROPS; i++)
		prop[i] = pdata->smip_prop[i];

	/* Read the property requested by the caller */
	ret = intel_scu_ipc_read_mip(data, prop[pp].len, prop[pp].offset, 1);
	if (ret)
		return ret;

	/* Adjust the bytes according to the length and return the int */
	val = data[0];
	for (i = 1; i < prop[pp].len; i++)
		val = val << 8 | data[i];

	/* If the requested property is a bit field, return that bit value */
	if (prop[pp].is_bit_field)
		val &= prop[pp].mask;

	return val;
}
EXPORT_SYMBOL(get_smip_property_by_name);

int intel_scu_ipc_write_umip(u8 *data, int len, int offset)
{
	int i, ret = 0, offset_align;
	int remainder, len_align = 0;
	u32 dptr, sptr, cmd;
	u8 cs, tbl_cs = 0, *buf = NULL;
	Sector sect;
	struct block_device *bdev;
	char *buffer = NULL;
	int *holderId = NULL;
	int sect_no;
	u8 checksum;

	if (intel_mid_identify_cpu() == INTEL_MID_CPU_CHIP_CLOVERVIEW) {

		/* Opening the mmcblk0boot0 */
		bdev = get_emmc_bdev();
		if (bdev == NULL) {
			pr_err("%s: get_emmc failed!\n", __func__);
			return -ENODEV;
		}

		/* make sure the block device is open rw */
		ret = blkdev_get(bdev, FMODE_READ|FMODE_WRITE, holderId);
		if (ret < 0) {
			pr_err("%s: blk_dev_get failed!\n", __func__);
			return -ret;
		}

		/* get memmap of the UMIP header */
		sect_no = offset / SECTOR_SIZE;
		remainder = offset % SECTOR_SIZE;
		buffer = read_dev_sector(bdev, sect_no +
					UMIP_HEADER_HEADROOM_SECTOR, &sect);

		/* Shouldn't need to access UMIP sector 0/1 */
		if (sect_no < UMIP_TOTAL_HEADER_SECTOR_NO) {
			pr_err("invalid umip offset\n");
			ret = -EINVAL;
			goto bd_put;
		} else if (data == NULL || buffer == NULL) {
			pr_err("buffer is empty\n");
			ret = -ENODEV;
			goto bd_put;
		} else if (len > (SECTOR_SIZE - remainder)) {
			pr_err("too much data to write\n");
			ret = -EINVAL;
			goto bd_put;
		}

		lock_page(sect.v);
		memcpy(buffer + remainder, data, len);
		checksum = calc_checksum(buffer, SECTOR_SIZE);

		set_page_dirty(sect.v);
		unlock_page(sect.v);
		sync_blockdev(bdev);
		put_dev_sector(sect);

		/*
		 * Updating the checksum, sector 0 (starting from UMIP
		 * offset 0x08), we maintains 4 bytes for tracking each of
		 * sector changes individually. For example, the dword at
		 * offset 0x08 is used to checksum data integrity of sector
		 * number 2, and so on so forth. It's worthnoting that only
		 * the first byte in each 4 bytes stores checksum.
		 * For detail, please check CTP FAS UMIP header definition
		 */

		buffer = read_dev_sector(bdev, UMIP_HEADER_SECTOR +
					UMIP_HEADER_HEADROOM_SECTOR, &sect);

		if (buffer == NULL) {
			pr_err("buffer is empty\n");
			ret = -ENODEV;
			goto bd_put;
		}

		lock_page(sect.v);
		memcpy(buffer + 4 * (sect_no - UMIP_TOTAL_HEADER_SECTOR_NO) +
			UMIP_START_CHKSUM_ADDR, &checksum, 1/* one byte */);

		/* Change UMIP prologue chksum to zero */
		*(buffer + UMIP_HEADER_CHKSUM_ADDR) = 0;

		for (i = 0; i < UMIP_TOTAL_CHKSUM_ENTRY; i++) {
			tbl_cs ^= *(u8 *)(buffer + 4 * i +
					UMIP_START_CHKSUM_ADDR);
		}

		/* Finish up with re-calcuating UMIP prologue checksum */
		cs = dword_to_byte_chksum(xorblock((u32 *)buffer,
							SECTOR_SIZE));

		*(buffer + UMIP_HEADER_CHKSUM_ADDR) = tbl_cs ^ cs;

		set_page_dirty(sect.v);
		unlock_page(sect.v);
		sync_blockdev(bdev);
bd_put:
		if (buffer)
			put_dev_sector(sect);

		blkdev_put(bdev, FMODE_READ|FMODE_WRITE);
		return ret;
	} else {

		if (!intel_mip_base)
			return -ENODEV;

		if (offset + len > IPC_MIP_MAX_ADDR)
			return -EINVAL;

		rpmsg_global_lock();

		offset_align = offset & (~0x3);
		len_align = (len + (offset - offset_align) + 3) & (~0x3);

		if (len != len_align) {
			buf = kzalloc(len_align, GFP_KERNEL);
			if (!buf) {
				pr_err("Alloc memory failed\n");
				ret = -ENOMEM;
				goto fail;
			}
			ret = read_mip(buf, len_align, offset_align, 0);
			if (ret)
				goto fail;
			memcpy(buf + offset - offset_align, data, len);
		} else {
			buf = data;
		}

		dptr = offset_align;
		sptr = len_align / 4;
		cmd = IPC_CMD_UMIP_WR << 12 | IPCMSG_MIP_ACCESS;

		memcpy(intel_mip_base, buf, len_align);

		ret = rpmsg_send_raw_command(mip_instance, cmd, 0, NULL,
			NULL, 0, 0, sptr, dptr);

fail:
		if (buf && len_align != len)
			kfree(buf);

		rpmsg_global_unlock();

		return ret;
	}
}
EXPORT_SYMBOL(intel_scu_ipc_write_umip);


#define MAX_DATA_NR 8
#define MIP_CMD_LEN 11

enum {
	MIP_DBG_DATA,
	MIP_DBG_LEN,
	MIP_DBG_OFFSET,
	MIP_DBG_ISSIGNED,
	MIP_DBG_ERROR,
};

static u8 mip_data[MAX_DATA_NR];
static int valid_data_nr;
static int mip_len;
static int mip_offset;
static int mip_issigned;
static int mip_dbg_error;
static char mip_cmd[MIP_CMD_LEN];

static ssize_t mip_generic_show(char *buf, int type, int *data)
{
	int i;
	ssize_t ret = 0;

	switch (type) {
	case MIP_DBG_DATA:
		for (i = 0; i < valid_data_nr; i++) {
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
					"data[%d]: %#x\n",
					i, mip_data[i]);
		}
		break;
	case MIP_DBG_LEN:
		ret = snprintf(buf, PAGE_SIZE, "len: %d\n", *data);
		break;
	case MIP_DBG_OFFSET:
		ret = snprintf(buf, PAGE_SIZE, "offset: %#x\n", *data);
		break;
	case MIP_DBG_ISSIGNED:
		ret = snprintf(buf, PAGE_SIZE, "issigned: %d\n", *data);
		break;
	case MIP_DBG_ERROR:
		ret = snprintf(buf, PAGE_SIZE, "error: %d\n", *data);
		break;
	default:
		break;
	}

	return ret;
}

static void mip_generic_store(const char *buf, int type, int *data)
{
	int i, ret;

	if (type == MIP_DBG_DATA) {
		u32 t[MAX_DATA_NR];

		valid_data_nr = 0;
		memset(mip_data, 0, sizeof(mip_data));

		ret = sscanf(buf, "%x %x %x %x %x %x %x %x", &t[0], &t[1],
				&t[2], &t[3], &t[4], &t[5], &t[6], &t[7]);
		if (ret == 0 || ret > MAX_DATA_NR) {
			mip_dbg_error = -EINVAL;
			return;
		} else {
			for (i = 0; i < ret; i++)
				mip_data[i] = (u8)t[i];
			valid_data_nr = ret;
		}
	} else {
		*data = 0;
		switch (type) {
		case MIP_DBG_OFFSET:
			ret = sscanf(buf, "%x", data);
			break;
		case MIP_DBG_LEN:
		case MIP_DBG_ISSIGNED:
			ret = sscanf(buf, "%d", data);
			break;
		default:
			ret = -1;
			break;
		}
	}

	if (ret)
		mip_dbg_error = 0;
	else
		mip_dbg_error = -EINVAL;

	return;
}

static ssize_t mip_data_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return mip_generic_show(buf, MIP_DBG_DATA, NULL);
}

static ssize_t mip_data_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t size)
{
	mip_generic_store(buf, MIP_DBG_DATA, NULL);
	return size;
}

static ssize_t mip_len_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return mip_generic_show(buf, MIP_DBG_LEN, &mip_len);
}

static ssize_t mip_len_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t size)
{
	mip_generic_store(buf, MIP_DBG_LEN, &mip_len);
	return size;
}

static ssize_t mip_offset_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return mip_generic_show(buf, MIP_DBG_OFFSET, &mip_offset);
}

static ssize_t mip_offset_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t size)
{
	mip_generic_store(buf, MIP_DBG_OFFSET, &mip_offset);
	return size;
}

static ssize_t mip_issigned_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return mip_generic_show(buf, MIP_DBG_ISSIGNED, &mip_issigned);
}

static ssize_t mip_issigned_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t size)
{
	mip_generic_store(buf, MIP_DBG_ISSIGNED, &mip_issigned);
	return size;
}

static ssize_t mip_error_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return mip_generic_show(buf, MIP_DBG_ERROR, &mip_dbg_error);
}

static ssize_t mip_cmd_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t size)
{

	int ret;

	memset(mip_cmd, 0, sizeof(mip_cmd));

	ret = sscanf(buf, "%10s", mip_cmd);
	if (ret == 0) {
		mip_dbg_error = -EINVAL;
		goto end;
	}

	if (!strncmp("read_mip", mip_cmd, MIP_CMD_LEN)) {
		memset(mip_data, 0, sizeof(mip_data));
		ret = intel_scu_ipc_read_mip(mip_data, mip_len, mip_offset,
				mip_issigned);
		if (!ret)
			valid_data_nr = mip_len;

	} else if (!strncmp("write_umip", mip_cmd, MIP_CMD_LEN)) {
		if (mip_len == valid_data_nr) {
			ret = intel_scu_ipc_write_umip(mip_data, mip_len,
					mip_offset);
		} else
			goto error;
	} else
		goto error;

	if (ret)
		goto error;
	else
		goto end;

error:
	mip_dbg_error = -EINVAL;

end:
	return size;
}

static KOBJ_MIP_ATTR(data, S_IRUSR|S_IWUSR, mip_data_show, mip_data_store);
static KOBJ_MIP_ATTR(len, S_IRUSR|S_IWUSR, mip_len_show, mip_len_store);
static KOBJ_MIP_ATTR(offset, S_IRUSR|S_IWUSR, mip_offset_show,
		mip_offset_store);
static KOBJ_MIP_ATTR(issigned, S_IRUSR|S_IWUSR, mip_issigned_show,
		mip_issigned_store);
static KOBJ_MIP_ATTR(cmd, S_IWUSR, NULL, mip_cmd_store);
static KOBJ_MIP_ATTR(error, S_IRUSR, mip_error_show, NULL);

static struct attribute *mip_attrs[] = {
	&data_attr.attr,
	&len_attr.attr,
	&offset_attr.attr,
	&issigned_attr.attr,
	&cmd_attr.attr,
	&error_attr.attr,
	NULL,
};

static struct attribute_group mip_attr_group = {
	.name = "mip_debug",
	.attrs = mip_attrs,
};

static int scu_mip_probe(struct platform_device *pdev)
{
	if (intel_mid_identify_cpu() != INTEL_MID_CPU_CHIP_PENWELL) {
		if (!pdev->dev.platform_data)
			return -EINVAL;
		pdata =
		(struct scu_mip_platform_data *)pdev->dev.platform_data;
	}
	return 0;
}

static int scu_mip_remove(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static const struct platform_device_id scu_mip_table[] = {
		{DRIVER_NAME, 1 },
};

static struct platform_driver scu_mip_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
	},
	.probe = scu_mip_probe,
	.remove = scu_mip_remove,
	.id_table = scu_mip_table,
};

static int __init scu_mip_init(void)
{
	return platform_driver_register(&scu_mip_driver);
}

static void scu_mip_exit(void)
{
	platform_driver_unregister(&scu_mip_driver);
}

static int mip_rpmsg_probe(struct rpmsg_channel *rpdev)
{
	int ret = 0;

	if (rpdev == NULL) {
		pr_err("rpmsg channel not created\n");
		ret = -ENODEV;
		goto out;
	}

	dev_info(&rpdev->dev, "Probed mip rpmsg device\n");

	/* Allocate rpmsg instance for mip*/
	ret = alloc_rpmsg_instance(rpdev, &mip_instance);
	if (!mip_instance) {
		dev_err(&rpdev->dev, "kzalloc mip instance failed\n");
		goto out;
	}
	/* Initialize rpmsg instance */
	init_rpmsg_instance(mip_instance);

	/* Init mip base */
	intel_mip_base = ioremap_nocache(IPC_MIP_BASE, IPC_MIP_MAX_ADDR);
	if (!intel_mip_base) {
		ret = -ENOMEM;
		goto rpmsg_err;
	}

	/* Create debugfs for mip regs */
	scu_mip_kobj = kobject_create_and_add(mip_attr_group.name,
						kernel_kobj);

	if (!scu_mip_kobj) {
		ret = -ENOMEM;
		goto mip_base_err;
	}

	ret = sysfs_create_group(scu_mip_kobj, &mip_attr_group);

	if (ret) {
		kobject_put(scu_mip_kobj);
		goto mip_base_err;
	}

	ret = scu_mip_init();
	goto out;
mip_base_err:
	iounmap(intel_mip_base);
rpmsg_err:
	free_rpmsg_instance(rpdev, &mip_instance);
out:
	return ret;
}

static void mip_rpmsg_remove(struct rpmsg_channel *rpdev)
{
	scu_mip_exit();
	iounmap(intel_mip_base);
	free_rpmsg_instance(rpdev, &mip_instance);
	sysfs_remove_group(scu_mip_kobj, &mip_attr_group);
	kobject_put(scu_mip_kobj);
	dev_info(&rpdev->dev, "Removed mip rpmsg device\n");
}

static void mip_rpmsg_cb(struct rpmsg_channel *rpdev, void *data,
					int len, void *priv, u32 src)
{
	dev_warn(&rpdev->dev, "unexpected, message\n");

	print_hex_dump(KERN_DEBUG, __func__, DUMP_PREFIX_NONE, 16, 1,
		       data, len,  true);
}

static struct rpmsg_device_id mip_rpmsg_id_table[] = {
	{ .name	= "rpmsg_mip" },
	{ },
};
MODULE_DEVICE_TABLE(rpmsg, mip_rpmsg_id_table);

static struct rpmsg_driver mip_rpmsg = {
	.drv.name	= KBUILD_MODNAME,
	.drv.owner	= THIS_MODULE,
	.id_table	= mip_rpmsg_id_table,
	.probe		= mip_rpmsg_probe,
	.callback	= mip_rpmsg_cb,
	.remove		= mip_rpmsg_remove,
};

static int __init mip_rpmsg_init(void)
{
	if ((intel_mid_identify_cpu() != INTEL_MID_CPU_CHIP_PENWELL)
		&& (intel_mid_identify_cpu() != INTEL_MID_CPU_CHIP_CLOVERVIEW))
		return -EINVAL;

	return register_rpmsg_driver(&mip_rpmsg);
}

#ifdef MODULE
module_init(mip_rpmsg_init);
#else
fs_initcall_sync(mip_rpmsg_init);
#endif

static void __exit mip_rpmsg_exit(void)
{
	return unregister_rpmsg_driver(&mip_rpmsg);
}
module_exit(mip_rpmsg_exit);

MODULE_AUTHOR("Shijie Zhang <shijie.zhang@intel.com>");
MODULE_DESCRIPTION("Intel SCU MIP driver");
MODULE_LICENSE("GPL v2");
