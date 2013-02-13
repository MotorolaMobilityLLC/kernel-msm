/*
 * Copyright (C) 2010-2012 Motorola Mobility LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/param.h>
#include <linux/uaccess.h>
#include <linux/wakelock.h>
#include <linux/diagchar.h>
#ifdef CONFIG_DIAG_OVER_USB
#include <mach/usbdiag.h>
#endif
#include "diagchar.h"
#include "diagfwd.h"
#include "diagaddon_slate.h"

#ifdef SLATE_DEBUG
#define slatedbg(fmt, ...) printk(KERN_INFO fmt, ##__VA_ARGS__)
#define slatedbg_hex_dump(level, prefix_str, prefix_type, rowsize, groupsize,\
		buf, len, ascii) \
		print_hex_dump(level, prefix_str, prefix_type, rowsize,\
			 groupsize, buf, len, ascii)
#define slatedbg_saved_log_masks(saved_log_masks) \
	do { \
		int i;\
		printk(KERN_INFO "SAVED LOG MASK\n");\
		for (i = 0; i < 16; i++) {\
			printk(KERN_INFO " %02X",\
				 ((char *)(saved_log_masks))[i]);\
			if (((i+1) % 20) == 0)\
				printk(KERN_INFO "\n");\
		} \
		printk(KERN_INFO "\n");\
	} while (0)
#else
#define slatedbg(fmt, ...)
#define slatedbg_hex_dump(level, prefix_str, prefix_type, rowsize, groupsize,\
		buf, len, ascii)
#define slatedbg_saved_log_masks(saved_log_masks)
#endif

#define SLATE_PROGRAM_NAME    "SLATE_DIAG"
#define SLATE_EVENT_MASK_BIT_SET(id)\
	(driver->event_masks[(id)/8] & (1 << ((id) & 0x07)))

struct slate_ctx {
	uint8_t *saved_log_masks;
	int slate_cmd;
	int slate_log_count;
	int log_cmd;
	int log_count;
	bool slate_enable;

	struct wake_lock slate_diag_wake_lock;
	struct delayed_work work;
	struct workqueue_struct *slate_wq;
};

static int  slate_diag_process_modem_pkt(unsigned char *buf, int len);
static void slate_diag_process_user_pkt(unsigned char *data, unsigned len);

static void slate_diag_process_get_stateAndConnInfo_log(void);
static int  slate_diag_log_is_enabled(unsigned char log_type);
static void slate_diag_cfg_event_mask(void);
static int slate_ioctl(struct file *filp, unsigned int iocmd,
			unsigned long ioarg, int *retval);
static int slate_force_returntype(int pkt_type, int *retval);
static int slate_channel_diag_write(struct diag_request *write_ptr,
							int *retval);
static struct diag_addon slate_addon = {
	.ioctl = slate_ioctl,
	.force_returntype = slate_force_returntype,
	.addon_channel_diag_write = slate_channel_diag_write,
};

static int slate_ioctl(struct file *filp, unsigned int iocmd,
					unsigned long ioarg, int *retval)
{
	struct slate_ctx *ctx = slate_addon.private;
	int next_addon_call = false;
	long success = 0;
	int status;

	switch (iocmd) {
	case DIAG_IOCTL_GET_STATE_AND_CONN_INFO:
		ctx->slate_enable = true;
		slate_diag_process_get_stateAndConnInfo_log();
		success = 1;
		break;
	case DIAG_IOCTL_GET_KEY_EVENT_MASK:
		ctx->slate_enable = true;
		slate_diag_cfg_event_mask();
		status = (int)SLATE_EVENT_MASK_BIT_SET(0x1AE);
		if (copy_to_user((void __user *)ioarg, &status, sizeof(int)))
			success = -EFAULT;
		else
			success = 1;
		break;
	case DIAG_IOCTL_GET_PEN_EVENT_MASK:
		ctx->slate_enable = true;
		slate_diag_cfg_event_mask();
		status = (int)SLATE_EVENT_MASK_BIT_SET(0x1AF);
		if (copy_to_user((void __user *)ioarg, &status, sizeof(int)))
			success = -EFAULT;
		else
			success = 1;
		break;
	default:
		next_addon_call = true;
		break;
	}

	if (retval)
		*retval = success;

	return next_addon_call;
}

static void __clear_in_busy(struct diag_request *write_ptr)
{
	struct diag_smd_info *smd_info = &(driver->smd_data[MODEM_DATA]);

	if (write_ptr->buf == (void *)smd_info->buf_in_1)
		smd_info->in_busy_1 = 0;
	else if (write_ptr->buf == (void *)smd_info->buf_in_2)
		smd_info->in_busy_2 = 0;
}

static int slate_channel_diag_write(struct diag_request *write_ptr,
							int *retval)
{
	struct slate_ctx *ctx = slate_addon.private;
	int err = 0;

	if (ctx->slate_enable == false) {
		err = slate_addon.channel_diag_write(driver->legacy_ch,
								write_ptr);
	} else if (slate_diag_process_modem_pkt(write_ptr->buf,
							write_ptr->length)) {
		err = slate_addon.channel_diag_write(driver->legacy_ch,
								write_ptr);
	} else {
		err = 0;
		__clear_in_busy(write_ptr);
	}

	if (retval)
		*retval = err;

	return false;
}

static void slate_cfg_log_mask(unsigned char cmd_type)
{
	struct slate_ctx *ctx = slate_addon.private;
	unsigned len = 0;
	unsigned char *ptr = driver->hdlc_buf;

	slatedbg(KERN_INFO "slate_cfg_log_mask cmd_type=%d\n", cmd_type);
	switch (cmd_type) {
	case DIAG_MASK_CMD_ADD_GET_RSSI:
		if (ptr) {
			len = 159; /* 0x4F5 bits / 8 = 159 bytes */
			memset(ptr, 0, len+16);
			*ptr = 0x73;
			ptr += 4;
			*(int *)ptr = 0x0003;
			ptr += 4;
			*(int *)ptr = 0x0001;
			ptr += 4;
			*(int *)ptr = 0x04F5;
			ptr += 4;
			memcpy(ptr, driver->log_masks, len);
			ptr += 13;
			*ptr |= 0x02; /* Enable 0x1069 EVDO POWER log bit */
			/* len param not used for 0x73 */
			slate_diag_process_user_pkt(driver->hdlc_buf, len+16);
		}
		break;
	case DIAG_MASK_CMD_ADD_GET_STATE_AND_CONN_ATT:
		if (ptr) {
			len = 159; /* 0x4F5 bits / 8 = 159 bytes */
			memset(ptr, 0, len+16);
			*ptr = 0x73;
			ptr += 4;
			*(int *)ptr = 0x0003;
			ptr += 4;
			*(int *)ptr = 0x0001;
			ptr += 4;
			*(int *)ptr = 0x04F5;
			ptr += 4;
			memcpy(ptr, driver->log_masks, len);
			ptr += 13;
			/* Enable 0x106E EVDO CONN ATTEMPTS log bit */
			*ptr |= 0x40;
			ptr += 2;
			/* Enable 0x107E EVDO STATE log bit */
			*ptr |= 0x40;
			/* len param not used for 0x73 */
			slate_diag_process_user_pkt(driver->hdlc_buf, len+16);
		}
		break;
	case DIAG_MASK_CMD_ADD_GET_SEARCHER_DUMP:
		if (ptr) {
			len = 159; /* 0x4F5 bits / 8 = 159 bytes */
			memset(ptr, 0, len+16);
			*ptr = 0x73;
			ptr += 4;
			*(int *)ptr = 0x0003;
			ptr += 4;
			*(int *)ptr = 0x0001;
			ptr += 4;
			*(int *)ptr = 0x04F5;
			ptr += 4;
			memcpy(ptr, driver->log_masks, len);
			ptr += 5;
			*ptr |= 0x20; /* 1020 (Searcher and Finger) */
			ptr += 38;
			*ptr |= 0x01; /* 1158 (Internal - Core Dump) */
			ptr += 8;
			/* 119A, 119B (Srch TNG Finger Status
			   and Srch TNG 1x Searcher Dump) */
			*ptr |= 0x08;
			/* len param not used for 0x73 */
			slate_diag_process_user_pkt(driver->hdlc_buf, len+16);
		}
		break;
	case DIAG_MASK_CMD_SAVE:
		memcpy(ctx->saved_log_masks, driver->log_masks, LOG_MASK_SIZE);
		break;
	case DIAG_MASK_CMD_RESTORE:
	default:
		if (ptr) {
			len = 159; /* 0x4F5 bits / 8 = 159 bytes */
			memset(ptr, 0, len+16);
			*ptr = 0x73;
			ptr += 4;
			*(int *)ptr = 0x0003;
			ptr += 4;
			*(int *)ptr = 0x0001;
			ptr += 4;
			*(int *)ptr = 0x04F5;
			ptr += 4;
			memcpy(ptr, ctx->saved_log_masks, len);
			/* len param not used for 0x73 */
			slate_diag_process_user_pkt(driver->hdlc_buf, len+16);
		}
		break;
	}
}

void diag_init_log_cmd(unsigned char log_cmd)
{
	struct slate_ctx *ctx = slate_addon.private;
	switch (log_cmd) {
	case DIAG_LOG_CMD_TYPE_GET_1x_SEARCHER_DUMP:
		slate_cfg_log_mask(DIAG_MASK_CMD_SAVE);
		slate_cfg_log_mask(DIAG_MASK_CMD_ADD_GET_SEARCHER_DUMP);
		slate_cfg_log_mask(DIAG_MASK_CMD_SAVE);
		slate_cfg_log_mask(DIAG_MASK_CMD_ADD_GET_SEARCHER_DUMP);
		break;
	default:
		printk(KERN_ERR "Invalid log_cmd=%d\n", log_cmd);
		break;
	}
	ctx->log_count = 0;
	ctx->log_cmd = log_cmd;
}

static void slate_diag_init_log_cmd(unsigned char cmd_type)
{
	struct slate_ctx *ctx = slate_addon.private;
	switch (cmd_type) {
	case DIAG_LOG_CMD_TYPE_GET_RSSI:
		slate_cfg_log_mask(DIAG_MASK_CMD_SAVE);
		slate_cfg_log_mask(DIAG_MASK_CMD_ADD_GET_RSSI);
		break;
	case DIAG_LOG_CMD_TYPE_GET_STATE_AND_CONN_ATT:
		slate_cfg_log_mask(DIAG_MASK_CMD_SAVE);
		slate_cfg_log_mask(DIAG_MASK_CMD_ADD_GET_STATE_AND_CONN_ATT);
		break;
	default:
		printk(KERN_ERR "Invalid SLATE log cmd_type %d\n", cmd_type);
		break;
	}
	ctx->slate_log_count = 0;
	ctx->slate_cmd = cmd_type;
}

int slate_diag_log_is_enabled(unsigned char log_type)
{
	struct slate_ctx *ctx = slate_addon.private;
	int log_offset = 0;
	uint8_t log_mask = 0;
	int ret = 0;

	switch (log_type) {
	case DIAG_LOG_TYPE_RSSI:
		log_offset = 13;
		log_mask = 0x02;
		break;
	case DIAG_LOG_TYPE_STATE:
		log_offset = 15;
		log_mask = 0x40;
		break;
	case DIAG_LOG_TYPE_CONN_ATT:
		log_offset = 13;
		log_mask = 0x40;
		break;
	default:
		pr_err("[%s] INVALID log_type = %d\n", __func__, log_type);
		break;
	}

	slatedbg_saved_log_masks(ctx->saved_log_masks);

	if ((ctx->saved_log_masks[log_offset] & log_mask) == log_mask)
		ret = 1;

	slatedbg(KERN_INFO "%s log_type=%d ret=%d\n", __func__, log_type, ret);

	return ret;
}

void diag_restore_log_masks(void)
{
	struct slate_ctx *ctx = slate_addon.private;
	ctx->log_cmd = DIAG_LOG_CMD_TYPE_RESTORE_LOG_MASKS;
	slate_cfg_log_mask(DIAG_MASK_CMD_RESTORE);
}

static void slate_diag_restore_log_masks(void)
{
	struct slate_ctx *ctx = slate_addon.private;
	slatedbg(KERN_ERR "\nSLATE: slate_diag_restore_log_masks\n");
	ctx->slate_cmd = DIAG_LOG_CMD_TYPE_RESTORE_LOG_MASKS;
	slate_cfg_log_mask(DIAG_MASK_CMD_RESTORE);
}
void slate_diag_process_get_rssi_log(void)
{
	slate_diag_init_log_cmd(DIAG_LOG_CMD_TYPE_GET_RSSI);
}
static void slate_log_mask_timer_handle(struct work_struct *work)
{
	struct slate_ctx *ctx = slate_addon.private;
	slatedbg(KERN_ERR "\nSLATE: slate_log_mask_timer_handle\n");
	slate_diag_restore_log_masks();

	/* don't need the wake lock anymore after restoring the logmask */
	wake_lock_destroy(&ctx->slate_diag_wake_lock);
}
static void slate_diag_restore_log_masks_timer(void)
{
	struct slate_ctx *ctx = slate_addon.private;

	if (queue_delayed_work(ctx->slate_wq, &ctx->work,
					msecs_to_jiffies(2*60*1000)) == 0) {
		cancel_delayed_work(&ctx->work);
		queue_delayed_work(ctx->slate_wq, &ctx->work,
					msecs_to_jiffies(2*60*1000));
	}
}
static void slate_diag_process_get_stateAndConnInfo_log(void)
{
	struct slate_ctx *ctx = slate_addon.private;

	if (ctx->slate_wq == NULL) { /* it is used for first-connection*/
		/* This wake lock is to prevent phone from going to sleep and
		   thereby leaving BP logmask on that causes a current drain.
		   After the timer below is handled, we will kill the wake lock
		   (in approximately two mins) */
		wake_lock_init(&ctx->slate_diag_wake_lock,
					WAKE_LOCK_SUSPEND, SLATE_PROGRAM_NAME);
		/* setup a wake lock timer for 2.5 mins. */
		wake_lock_timeout(&ctx->slate_diag_wake_lock, 2.5*60*HZ);
	}
	slate_diag_init_log_cmd(DIAG_LOG_CMD_TYPE_GET_STATE_AND_CONN_ATT);
	if (ctx->slate_wq == NULL) {
		ctx->slate_wq = create_singlethread_workqueue("slate_wq");
		INIT_DELAYED_WORK(&ctx->work, slate_log_mask_timer_handle);
		slate_diag_restore_log_masks_timer();
	}
}

static void slate_diag_cfg_event_mask(void)
{
	unsigned len = 0;
	unsigned char *ptr = driver->hdlc_buf;

	if (ptr) {
		len = 2;
		memset(ptr, 0, len);
		*ptr = 0x60;
		ptr++;
		*ptr = 0x01;
		slate_diag_process_user_pkt(driver->hdlc_buf, len);
	}

	ptr = driver->hdlc_buf;
	if (ptr) {
		len = 270;
		memset(ptr, 0, len);
		*ptr = 0x82;
		ptr += 4;
		*ptr = 0x40;
		ptr++;
		*ptr = 0x08;
		ptr += 54;
		*ptr = 0xC0;
		slate_diag_process_user_pkt(driver->hdlc_buf, len);
	}
}

static int slate_diag_process_modem_pkt(unsigned char *buf, int len)
{
	struct slate_ctx *ctx = slate_addon.private;
	/* 1 - pkt needs to be forwarded to QXDM; 0 - drop packet */
	int ret = 1;

	slatedbg_hex_dump(KERN_DEBUG, "MODEM_DATA writing data to USB ",
				16, 1, DUMP_PREFIX_ADDRESS, buf, len, 1);

	if (buf[0] == 0x10 && buf[6] == 0x69 && buf[7] == 0x10) {
		slatedbg(KERN_INFO
			"MODEM_DATA **** EVDO POWER PKT ***** slate_cmd=%d\n",
			ctx->slate_cmd);

		switch (ctx->slate_cmd) {
		case DIAG_LOG_CMD_TYPE_GET_RSSI:
			slate_diag_restore_log_masks();
			slate_addon.diag_process_apps_pkt(buf, len);
			if (slate_diag_log_is_enabled(DIAG_LOG_TYPE_RSSI)
									== 0) {
				ret = 0;
				slatedbg(KERN_INFO
				"SLATE EVDO POWER LOG PACKET DROPPED!\n");
			}
			break;
		case DIAG_LOG_CMD_TYPE_GET_STATE_AND_CONN_ATT:
		default:
			break;
		}
	} else if (buf[0] == 0x10 && buf[6] == 0x6E && buf[7] == 0x10) {
		slatedbg(KERN_INFO
			"MODEM_DATA **** EVDO CONN ATT ***** slate_cmd=%d\n",
			ctx->slate_cmd);

		switch (ctx->slate_cmd) {
		case DIAG_LOG_CMD_TYPE_GET_STATE_AND_CONN_ATT:
			if ((ctx->slate_log_count & 0x02) == 0x00) {
				ctx->slate_log_count |= 2;
				if (ctx->slate_log_count == 3)
					slate_diag_restore_log_masks();
				slate_addon.diag_process_apps_pkt(buf, len);
			}
			if (slate_diag_log_is_enabled(DIAG_LOG_TYPE_CONN_ATT)
									== 0) {
				ret = 0;
				slatedbg(KERN_INFO
				"SLATE EVDO CONN ATT LOG PACKET DROPPED!\n");
			}
			break;
		case DIAG_LOG_CMD_TYPE_GET_RSSI:
		default:
			break;
		}
	} /* the byte 0x7E is transmitted as 0x7D followed by 0x5E */
	else if (buf[0] == 0x10 && buf[6]  == 0x7D &&
		 buf[7]  == 0x5E && buf[8]  == 0x10) {
		slatedbg(KERN_INFO
			"MODEM_DATA **** EVDO STATE ***** slate_cmd=%d\n",
			ctx->slate_cmd);
		switch (ctx->slate_cmd) {
		case DIAG_LOG_CMD_TYPE_GET_STATE_AND_CONN_ATT:
			if ((ctx->slate_log_count & 0x01) == 0x00) {
				ctx->slate_log_count |= 1;
				if (ctx->slate_log_count == 3)
					slate_diag_restore_log_masks();
				slate_addon.diag_process_apps_pkt(buf, len);
			}
			if (slate_diag_log_is_enabled(DIAG_LOG_TYPE_STATE)
									== 0) {
				ret = 0;
				slatedbg(KERN_INFO
				"SLATE EVDO STATE LOG PACKET DROPPED!!!!!\n");
			}
			break;
		case DIAG_LOG_CMD_TYPE_GET_RSSI:
		default:
			break;
		}
	}
	return ret;
}
static void slate_diag_process_user_pkt(unsigned char *data, unsigned len)
{
	int type = 0;
	struct diag_smd_info *smd_info = &(driver->smd_data[MODEM_DATA]);

	slatedbg_hex_dump(KERN_DEBUG, "slate_diag_process_user_pkt ",
				 16, 1, DUMP_PREFIX_ADDRESS, data, len, 1);

	type = slate_addon.diag_process_apps_pkt(data, len);
	if (smd_info->ch && type)
		smd_write(smd_info->ch, data, len);
	else
		pr_err("DIAG User Packet not written to SMD (MODEM) type=%d\n",
			type);
}

/* when pkt_type is 0,
   return pkt_type to DATA_TYPE_RESPONSE for slate by force */
static int slate_force_returntype(int pkt_type, int *retval)
{
	struct slate_ctx *ctx = slate_addon.private;

	if (ctx->slate_enable == false)
		return true;

	if (pkt_type == 0 && retval != NULL)
		*retval = DATA_TYPE_RESPONSE;

	return false;
}

static int __init diagaddon_slate_init(void)
{
	struct slate_ctx *ctx;
	ctx = kzalloc(sizeof(struct slate_ctx), GFP_KERNEL);
	if (ctx == NULL)
		goto err;

	ctx->saved_log_masks = kzalloc(LOG_MASK_SIZE, GFP_KERNEL);
	if (ctx->saved_log_masks == NULL) {
		kfree(ctx);
		goto err;
	}

	ctx->slate_cmd = 0;
	ctx->slate_log_count = 0;
	ctx->slate_enable = false;
	slate_addon.private = ctx;

	diag_addon_register(&slate_addon);
	if (slate_addon.diag_process_apps_pkt == NULL)
		goto err_funcs;
	if (slate_addon.channel_diag_write == NULL)
		goto err_funcs;

	return 0;

err_funcs:
	kfree(ctx->saved_log_masks);
	kfree(ctx);
err:
	return -EPERM;
}

static void __exit diagaddon_slate_exit(void)
{
	struct slate_ctx *ctx = slate_addon.private;

	if (ctx->slate_wq)
		destroy_workqueue(ctx->slate_wq);

	diag_addon_unregister(&slate_addon);
	if (ctx)
		kfree(ctx->saved_log_masks);
	kfree(ctx);
}

MODULE_DESCRIPTION("Slate-addon for Diag Char Driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");
module_init(diagaddon_slate_init);
module_exit(diagaddon_slate_exit);
