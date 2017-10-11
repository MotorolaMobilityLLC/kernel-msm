/*
** =============================================================================
** Copyright (c) 2016  Texas Instruments Inc.
**
** This program is free software; you can redistribute it and/or modify it under
** the terms of the GNU General Public License as published by the Free Software
** Foundation; version 2.
**
** This program is distributed in the hope that it will be useful, but WITHOUT
** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
** FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
** File:
**     tas2560-calib.c
**
** Description:
**     Dsp control driver for Texas Instruments TAS2560 High Performance 4W Smart
**     Amplifier Algorithm running as CAPIv2 Module in AFE.
**
** =============================================================================
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/semaphore.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <sound/tas2560_algo.h>

/* Holds the Packet data required for processing */
struct tas_dsp_pkt {
	u8 slave_id;
	u8 book;
	u8 page;
	u8 offset;
	u8 data[TAS2560_ALGO_PAYLOAD_SIZE * sizeof(u32)];
};

static int tas_params_ctrl(uint8_t *input, u8 dir, u8 count)
{
	u32 length = count / sizeof(u32);
	u32 paramid = 0, moduleid = AFE_TAS2560_ALGO_MODULE_RX;
	u32 index;
	int ret = 0;
	int special_index = 0;
	struct tas_dsp_pkt *ppacket;

	ppacket = (struct tas_dsp_pkt *)kmalloc(sizeof(struct tas_dsp_pkt), GFP_KERNEL);
	if (!ppacket) {
		pr_err ("TAS2560_ALGO:%s kmalloc failed", __func__);
		return -ENOMEM;
	}

	memset(ppacket, 0, sizeof(struct tas_dsp_pkt));
	ret = copy_from_user(ppacket, input, sizeof(struct tas_dsp_pkt));
	if (ret) {
		pr_err("TAS2560_ALGO:%s Error copying from user\n", __func__);
		kfree (ppacket);
		return -EFAULT;
	}
	index = TAS2560_ALGO_GET_IDX(ppacket->page, ppacket->offset);
	special_index = TAS2560_ALGO_IS_SPL_IDX(index);
	if (special_index == 0) {
		if ((index < 0 || index > MAX_TAS2560_ALGO_PARAM_INDEX)) {
			pr_err("TAS2560_ALGO:%s invalid index = %d\n", __func__, index);
			kfree (ppacket);
			return -EINVAL;
		}
	}
	pr_info("TAS2560_ALGO:%s valid index = %d special = %s\n", __func__, index, special_index ? "Yes" : "No");

	/* speakers are differentiated by slave-ids and instance combination(channel) */
	if ((ppacket->slave_id == SLAVE1) || (ppacket->slave_id == SLAVE2))
		paramid = TAS2560_ALGO_CALC_PIDX(paramid, index, length, ppacket->slave_id);
	else
		pr_err("TAS2560_ALGO:%s Wrong slaveid = %x\n", __func__, ppacket->slave_id);

	/*
	 * Note: In any case calculated paramid should not match with
	 * default parameters from ACDB
	 */
	if (paramid == AFE_PARAM_ID_TAS2560_ALGO_RX_ENABLE ||
		paramid == AFE_PARAM_ID_TAS2560_ALGO_TX_ENABLE ||
		paramid == AFE_PARAM_ID_TAS2560_ALGO_RX_CFG ||
		paramid == AFE_PARAM_ID_TAS2560_ALGO_TX_CFG) {
		pr_err("TAS2560_ALGO:%s %s Slave 0x%x params failed, paramid = 0x%x mismatch\n", __func__,
				dir == TAS2560_ALGO_GET_PARAM ? "get" : "set", ppacket->slave_id, paramid);
		kfree (ppacket);
		return -EINVAL;
	}
	ret = afe_tas2560_algo_ctrl(ppacket->data, paramid, moduleid,
			dir, length * sizeof(u32));
	if (ret)
		pr_err("TAS2560_ALGO:%s %s Slave 0x%x param control failed from afe\n", __func__,
				dir == TAS2560_ALGO_GET_PARAM ? "get" : "set", ppacket->slave_id);
	else
		pr_info("TAS2560_ALGO:%s Algo control returned %d\n", __func__, ret);

	if (dir == TAS2560_ALGO_GET_PARAM) {
		ret = copy_to_user(input, ppacket, sizeof(struct tas_dsp_pkt));
		if (ret) {
			pr_err("TAS2560_ALGO:%s Error copying to user after DSP", __func__);
			ret = -EFAULT;
		}
	}
	pr_info("TAS2560_ALGO:%s ppacket.data 0x%x\n", __func__, ((u32 *)(ppacket->data))[0]);
	kfree(ppacket);
	return ret;
}

static int tas_calib_open(struct inode *inode, struct file *fd)
{
	return 0;
}

static ssize_t tas_calib_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *offp)
{
	int rc = 0;
	rc = tas_params_ctrl((uint8_t *)buffer, TAS2560_ALGO_SET_PARAM, count);
	return rc;
}

static ssize_t tas_calib_read(struct file *file, char __user *buffer,
		size_t count, loff_t *ptr)
{
	int rc;
	rc = tas_params_ctrl((uint8_t *)buffer, TAS2560_ALGO_GET_PARAM, count);
	/*No need of retry*/
	if (rc < 0)
		count = rc;
	return count;
}

static long tas_calib_ioctl(struct file *filp, uint cmd, ulong arg)
{
	return 0;
}

static int tas_calib_release(struct inode *inode, struct file *fd)
{
	return 0;
}

const struct file_operations tas_calib_fops = {
	.owner			= THIS_MODULE,
	.open			= tas_calib_open,
	.write			= tas_calib_write,
	.read			= tas_calib_read,
	.release		= tas_calib_release,
	.unlocked_ioctl	= tas_calib_ioctl,
};

static struct miscdevice tas_calib_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "tas_calib",
	.fops = &tas_calib_fops,
};

static int __init tas_calib_init(void)
{
	int rc;
	pr_err("TAS2560_ALGO:%s", __func__);
	rc = misc_register(&tas_calib_misc);
	if (rc)
		pr_err("TAS2560_ALGO:%s register calib misc failed\n", __func__);
	return rc;
}

static void __exit tas_calib_exit(void)
{
	misc_deregister(&tas_calib_misc);
}

module_init(tas_calib_init);
module_exit(tas_calib_exit);

MODULE_AUTHOR("Texas Instruments Inc.");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("DSP Driver To Control SmartAmp Algorithm");
