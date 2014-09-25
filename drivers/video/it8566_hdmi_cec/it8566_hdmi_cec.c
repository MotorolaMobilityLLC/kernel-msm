/*
 * ITE it8566 HDMI CEC driver
 *
 * Copyright(c) 2014 ASUSTek COMPUTER INC. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/fcntl.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include "it8566_hdmi_cec_ioctl.h"
#include "it8566_hdmi_cec.h"

#define CEC_DEV_NAME		"it8566_hdmi_cec"
#define CEC_IRQ_GPIO_PIN	55
/*debug*/
/*#define CEC_REV_WORK*/
/*#define DEBUG*/

#define CEC_WRITE_DATA_LEN	17
#define CEC_READ_DATA_LEN	17
#define CEC_WRITE_CMD		0x90
#define CEC_READ_CMD		0x91
#define CEC_LA_CMD		0x92
#define CEC_CHECK_STATUS_CMD	0x93
#define CEC_GET_VERSION_CMD	0x94

#define NUM_CHECK_TX_BUSY	25
#define NUM_I2C_READ_RETRY	5

static struct i2c_client	*cec_client;
static struct miscdevice hdmi_cec_device;
static struct workqueue_struct  *cec_rev_wq;
static int cec_irq;



static int cec_i2c_read(unsigned char *to_header, unsigned char *to_opcode,
		unsigned char to_data[], int *to_opds_len);

static void cec_rev_worker(struct work_struct *work)
{
#ifndef CEC_REV_WORK
	char *envp[2] = { "CEC_MSG_RCEV=1", NULL };
	/*send event to user*/
	dev_dbg(&cec_client->dev, "%s:send CEC_MSG_RCEV uevent\n", __func__);
	kobject_uevent_env(&hdmi_cec_device.this_device->kobj
			, KOBJ_CHANGE, envp);
#else
	unsigned char header, opcode;
	int op_len;
	unsigned char data[14] = {0};
	int i, result;

	dev_info(&cec_client->dev, "%s\n", __func__);

	result = cec_i2c_read(&header, &opcode, data, &op_len);
	if (result < 0) {
		dev_err(&cec_client->dev, "%s: cec_i2c_read fail...", __func__);
		return;
	}
	dev_info(&cec_client->dev, "%s: header=0x%x, opcode=0x%x, op_len=0x%x\n"
			, __func__, header, opcode, op_len);
	for (i = 0; i < op_len; i++)
		dev_info(&cec_client->dev, "op[%d]=0x%x", i, data[i]);
	dev_info(&cec_client->dev, "\n");
#endif
}
static DECLARE_WORK(cec_rev_work, cec_rev_worker);

static irqreturn_t hdmi_cec_irq_handler(int irq, void *ptr_not_use)
{
	dev_dbg(&cec_client->dev, "%s: receive cec irq\n", __func__);
	queue_work(cec_rev_wq, &cec_rev_work);
	return IRQ_HANDLED;
}

static inline int it8566_setup_irq(void)
{
	int ret;

	ret = gpio_request(CEC_IRQ_GPIO_PIN, "it8566_irq");
	if (ret) {
		dev_err(&cec_client->dev,
			"failed to request GPIO %d\n",
			CEC_IRQ_GPIO_PIN);
		return ret;
	}
	ret = gpio_direction_input(CEC_IRQ_GPIO_PIN);
	if (ret) {
		dev_err(&cec_client->dev,
			"failed to set GPIO %d as input\n",
			CEC_IRQ_GPIO_PIN);
		return ret;
	}

	cec_irq = gpio_to_irq(CEC_IRQ_GPIO_PIN);
	dev_info(&cec_client->dev, "IRQ number assigned = %d\n", cec_irq);

	ret = request_irq(cec_irq, hdmi_cec_irq_handler,
				IRQF_TRIGGER_FALLING,
				"hdmi_cec_irq", NULL);
	if (ret) {
		dev_err(&cec_client->dev,
			"request_irq %d failed\n", cec_irq);
		return ret;
	}
	return 0;
}

/*cec transmit/receive ++*/
static unsigned char check_it8566_status(void)
{
	int st = 0;

	if (!cec_client) {
		dev_err(&cec_client->dev, "no cec_client\n");
		return 0;
	}

	st = i2c_smbus_read_byte_data(cec_client, CEC_CHECK_STATUS_CMD);

	if (st < 0) {
		dev_err(&cec_client->dev,
			"%s:i2c transfer FAIL: err=%d\n", __func__, st);
		return RESULT_CEC_BUS_ERR;
	}
	dev_dbg(&cec_client->dev, "%s:status=0x%x\n", __func__, st);
	return (unsigned char)st;
}

static int cec_i2c_write(unsigned char header, unsigned char opcode,
		unsigned char opds[], int opds_len)
{
	unsigned char tx_data[CEC_WRITE_DATA_LEN] = {0};
	int i, err;

	if (!cec_client) {
		dev_err(&cec_client->dev, "no cec_client\n");
		return -1;
	}

	if (opds_len > 14) {
		dev_err(&cec_client->dev, "operands can not exceed 14: %d\n",
			opds_len);
		return -1;
	}

	/*length: header,opcode,data0,data1 ... , region:1~16*/
	tx_data[0] = opds_len + 2;
	tx_data[1] = header;
	tx_data[2] = opcode;

	for (i = 0; i < opds_len; i++)
		tx_data[i+3] = opds[i];

	for (i = 0; i < CEC_WRITE_DATA_LEN; i++)
		dev_dbg(&cec_client->dev, "tx_data[%d]= 0x%x\n", i, tx_data[i]);

	err = i2c_smbus_write_i2c_block_data(cec_client, CEC_WRITE_CMD,
			CEC_WRITE_DATA_LEN, tx_data);
	if (err < 0) {
		dev_err(&cec_client->dev,
			"%s:i2c transfer FAIL: err=%d\n", __func__, err);
		return -1;
	}

	return 0;
}

static unsigned char cec_i2c_write_with_status_check(unsigned char header,
		unsigned char opcode, unsigned char *body_operads,
		int body_operads_len)
{
		unsigned char ck_st;
		int chk_tx_busy_cnt = 0;
		int err;
		bool is_broadcast, nak;

		/* 1. check if rx busy or bus err*/
		ck_st = check_it8566_status();
		if ((ck_st & RESULT_RX_BUSY) == RESULT_RX_BUSY) {
			dev_info(&cec_client->dev, "check it8566 rx busy, abort cec tx\n");
			return RESULT_RX_BUSY;
		} else if ((ck_st & RESULT_CEC_BUS_ERR) == RESULT_CEC_BUS_ERR) {
			dev_info(&cec_client->dev, "check it8566 tx bus err, abort cec tx\n");
			return RESULT_CEC_BUS_ERR;
		}

		/* 2. cec tx */
		err = cec_i2c_write(header, opcode,
				body_operads, body_operads_len);
		if (err < 0)
			return RESULT_CEC_BUS_ERR;

		/* 3. check tx busy, wait for it8566 done cec transfer */
		do {
			/* at least start bit + header */
			/*about 4.5 + 10*2.4 = 28.5ms */
			usleep_range(20000, 20500);
			ck_st = check_it8566_status();
			chk_tx_busy_cnt++;
		} while ((ck_st & RESULT_TX_BUSY) == RESULT_TX_BUSY &&
			  chk_tx_busy_cnt < NUM_CHECK_TX_BUSY);

		/* 4. check cec transfer result */
		is_broadcast = (header & 0xF) == 0xF;
		nak = (ck_st & RESULT_CEC_NACK) == RESULT_CEC_NACK;
		if ((ck_st & RESULT_TX_BUSY) == RESULT_TX_BUSY) {
			dev_err(&cec_client->dev, "cec tx busy\n");
			return RESULT_CEC_BUS_ERR;
		} else if ((ck_st & RESULT_CEC_BUS_ERR) == RESULT_CEC_BUS_ERR) {
			dev_info(&cec_client->dev, "cec tx bus err\n");
			return RESULT_CEC_BUS_ERR;
		} else if ((!is_broadcast && nak) || (is_broadcast && !nak)) {
			/* For broadcast messages the sense of
			   the ACK bit is inverted:
			   A ‘0’ read by the Initiator indicates that
			   one or more devices have rejected the message */
			dev_info(&cec_client->dev, "cec tx direct nack / bcst reject\n");
			return RESULT_CEC_NACK;
		} else {
			dev_dbg(&cec_client->dev, "cec tx success\n");
			return RESULT_TX_SUCCESS;
		}
}

/*
   number of elements of to_data[] passed in must be 14
*/
static int cec_i2c_read(unsigned char *to_header, unsigned char *to_opcode,
		unsigned char to_data[], int *to_opds_len)
{
	unsigned char rx_data[CEC_READ_DATA_LEN] = {0};
	int i, err;
	int retry = 0;

	if (!cec_client) {
		dev_err(&cec_client->dev, "no cec_client\n");
		return -1;
	}

read_retry:
	if (retry > NUM_I2C_READ_RETRY) {
		dev_err(&cec_client->dev, "cec read fail\n");
		return -1;
	}

	err = i2c_smbus_read_i2c_block_data(cec_client, CEC_READ_CMD,
			CEC_READ_DATA_LEN, rx_data);
	if (err < 0) {
		dev_err(&cec_client->dev,
		"%s:i2c transfer FAIL: err=%d, retry=%d\n",
		__func__, err, retry);
		retry++;
		goto read_retry;
	}

	for (i = 0; i < rx_data[0] + 1; i++)
		dev_dbg(&cec_client->dev, "rx_data[%d]= 0x%x\n", i, rx_data[i]);

	/*may only receive header*/
	*to_opds_len = rx_data[0] - 2; /*rx_data[0] region: 1~16*/

	*to_header = rx_data[1];

	if (*to_opds_len >= 0)
		*to_opcode = rx_data[2];

	for (i = 0; i < *to_opds_len; i++)
		to_data[i] = rx_data[i+3];

	return 0;
}
/*cec transmit/receive --*/

/* set/get LA ++ */
static int set_logical_address(unsigned char la)
{
	int err;

	if (!cec_client) {
		dev_err(&cec_client->dev, "no cec_client\n");
		return -1;
	}

	dev_dbg(&cec_client->dev, "%s:la=0x%x\n", __func__, la);

	err = i2c_smbus_write_byte_data(cec_client,
			CEC_LA_CMD, la);
	if (err < 0) {
		dev_err(&cec_client->dev,
			"%s:i2c transfer FAIL: err=%d\n", __func__, err);
		return -1;
	}

	return 0;
}

static int get_logical_address(unsigned char *to_la)
{
	int la;

	if (!cec_client) {
		dev_err(&cec_client->dev, "no cec_client\n");
		return -1;
	}

	la = i2c_smbus_read_byte_data(cec_client, CEC_LA_CMD);
	if (la < 0) {
		dev_err(&cec_client->dev,
			"%s:i2c transfer FAIL...: err=%d\n", __func__, la);
		return -1;
	}

	dev_dbg(&cec_client->dev, "%s:get la=0x%x ---\n", __func__, la);
	*to_la = (unsigned char)la;
	return 0;
}
/* set/get LA -- */

/* dbg_cec_tx ++ */
static ssize_t dbg_cec_tx_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[128];
	char *b;
	unsigned char user_data[17] = {0};
	unsigned int d;
	int offset = 0, i = 0;

	unsigned char user_header = 0, user_opcode = 0;
	int user_data_len = 0;

	size_t buf_size = min(count, sizeof(buf)-1);

	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	buf[buf_size] = 0;

	b = buf;
	while (sscanf(b, "%x%n", &d, &offset) == 1) {
		b += offset;
		user_data[i] = (unsigned char)d;
		i++;
	}

	user_header = user_data[0];
	user_opcode = user_data[1];
	user_data_len = user_data[2];

	if (user_opcode == 0)
		user_data_len = user_data_len - 1;

	dev_info(&cec_client->dev,
		"user_header=0x%x, user_opcode=0x%x, user_data_len=%d\n",
		user_header, user_opcode, user_data_len);


	cec_i2c_write_with_status_check(user_header, user_opcode,
			&user_data[3], user_data_len);

	return buf_size;
}

static const struct file_operations dbg_cec_tx_fops = {
	.open           = simple_open,
	.write          = dbg_cec_tx_write,
};
/* dbg_cec_tx -- */
/* dbg_cec_rx ++ */
static ssize_t dbg_cec_rx_read(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[64];
	int result;
	unsigned char header, opcode;
	int len;
	unsigned char data[14] = {0};

	result = cec_i2c_read(&header, &opcode, data, &len);
	if (result < 0)
		goto err;

	dev_dbg(&cec_client->dev, "%s: header=0x%x, opcode=0x%x, len=0x%x,"
		"data1=0x%x, data2=0x%x, data3=0x%x\n",
		__func__, header, opcode, len, data[0], data[1], data[3]);
	snprintf(buf, 64,
			"header=0x%x, opcode=0x%x, len=0x%x, data1=0x%x, data2=0x%x, data3=0x%x\n",
			header, opcode, len, data[0], data[1], data[2]);
	return simple_read_from_buffer(user_buf, count, ppos,
			buf, strlen(buf));
err:
	snprintf(buf, 32, "%s\n",
			"cec read fail...");
	return simple_read_from_buffer(user_buf, count, ppos,
			buf, strlen(buf));
}

static const struct file_operations dbg_cec_rx_fops = {
	.open           = simple_open,
	.read           = dbg_cec_rx_read,
};
/* dbg_cec_rx -- */

/* dbg_cec_la ++ */
static ssize_t dbg_cec_la_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[32];
	unsigned char la;
	int result;

	size_t buf_size = min(count, sizeof(buf)-1);

	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	buf[buf_size] = 0;
	if (kstrtou8(buf, 0, &la))
		return -EINVAL;

	dev_info(&cec_client->dev, "set la:0x%x\n", la);

	result = set_logical_address(la);
	if (result < 0)
		dev_err(&cec_client->dev, "set la fail\n");
	return buf_size;
}

static ssize_t dbg_cec_la_read(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos)
{
	unsigned char get_la = 0xf;
	char buf[32];
	int result;

	result = get_logical_address(&get_la);
	if (result < 0)
		dev_err(&cec_client->dev, "get la fail\n");

	dev_info(&cec_client->dev, "get la:0x%x\n", get_la);

	snprintf(buf, 32, "%d\n", get_la);

	return simple_read_from_buffer(user_buf, count, ppos,
			buf, strlen(buf));
}

static const struct file_operations dbg_cec_la_fops = {
	.open           = simple_open,
	.write           = dbg_cec_la_write,
	.read            = dbg_cec_la_read,
};
/* dbg_cec_la -- */

/* ioctl ++ */
static long hdmi_cec_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	/*compat_ptr() may need*/
	switch (cmd) {
	case IT8566_HDMI_CEC_IOCTL_SEND_MESSAGE:
	{
		struct it8566_cec_msg cec_msg;
		int rst;
#ifdef DEBUG
		int i;
#endif
		unsigned char header = 0, opcode = 0, body_len = 0;
		unsigned char *body_operads;

		if (copy_from_user(&cec_msg, (void __user *)arg,
				sizeof(cec_msg))) {
			dev_err(&cec_client->dev, "get arg error\n");
			return -EFAULT;
		}
#ifdef DEBUG
		dev_info(&cec_client->dev, "from user: init:0x%x, dest:0x%x, len:0x%x\n",
				cec_msg.initiator,
				cec_msg.destination,
				cec_msg.length);
		for (i = 0; i < cec_msg.length; i++)
			dev_info(&cec_client->dev,
				", body[%d]= 0x%x", i, cec_msg.body[i]);
		dev_info(&cec_client->dev, "\n");
#endif
		header = (cec_msg.initiator & 0xF) << 4 |
			(cec_msg.destination & 0xF);
		body_len = cec_msg.length;

		if (body_len > 0)
			opcode = cec_msg.body[0];

		if (body_len > 1)
			body_operads = &cec_msg.body[1];

		rst = cec_i2c_write_with_status_check(header, opcode,
				body_operads, body_len - 1);
		cec_msg.result = rst;

		if (copy_to_user((void __user *)arg, &cec_msg,
				sizeof(cec_msg))) {
			dev_err(&cec_client->dev, "pass arg fail\n");
			return -EFAULT;
		}
		break;
	}
	case IT8566_HDMI_CEC_IOCTL_RCEV_MESSAGE:
	{
		struct it8566_cec_msg cec_msg;
		int i, err;
		unsigned char header = 0, opcode = 0;
		int body_operads_len = 0;
		unsigned char body_operads[14] = {0};

		err = cec_i2c_read(&header, &opcode,
				body_operads, &body_operads_len);
		if (err) {
			dev_err(&cec_client->dev, "i2c read err\n");
			return err;
		}

		cec_msg.initiator = (header >> 4) & 0xF;
		cec_msg.destination = header & 0xF;
		cec_msg.length = body_operads_len + 1; /*region:0~15*/
		cec_msg.body[0] = opcode;
		for (i = 0; i < body_operads_len; i++)
			cec_msg.body[i+1] = body_operads[i];
#ifdef DEBUG
		dev_info(&cec_client->dev, "to user: init:0x%x, dest:0x%x, len:0x%x\n",
				cec_msg.initiator,
				cec_msg.destination,
				cec_msg.length);
		for (i = 0; i < cec_msg.length; i++)
			dev_info(&cec_client->dev,
				", body[%d]= 0x%x", i, cec_msg.body[i]);
		dev_info(&cec_client->dev, "\n");
#endif
		if (copy_to_user((void __user *)arg, &cec_msg,
				sizeof(cec_msg))) {
			dev_err(&cec_client->dev, "pass arg fail\n");
			return -EFAULT;
		}
		break;
	}
	case IT8566_HDMI_CEC_IOCTL_SET_LA:
	{
		unsigned char logical_addr;
		int err;

		if (copy_from_user(&logical_addr, (void __user *)arg,
				sizeof(unsigned char))) {
			dev_err(&cec_client->dev, "get arg error\n");
			return -EFAULT;
		}
		dev_dbg(&cec_client->dev,
			"set logical addr to %d\n", logical_addr);

		err = set_logical_address(logical_addr);
		if (err < 0)
			return -EBUSY;
		break;
	}
	case IT8566_HDMI_CEC_IOCTL_FW_UPDATE_IF_NEEDED:
	{
		int err;

		/* no need to update: err > 0,
		 * update success: err = 0,
		 * update fail: err < 0 */
		mutex_lock(&it8566_fw_lock);
		err = it8566_fw_update(0);
		mutex_unlock(&it8566_fw_lock);

		if (copy_to_user((void __user *)arg, &err,
				sizeof(int))) {
			dev_err(&cec_client->dev, "pass arg fail\n");
			return -EFAULT;
		}
		break;
	}
	default:
		dev_err(&cec_client->dev, "%s:unknown cmd=%d\n", __func__, cmd);
		return -EINVAL;
	}
	return 0;
}

static int hdmi_cec_open(struct inode *inode, struct file *filp)
{
	dev_info(&cec_client->dev, "%s\n", __func__);
	return nonseekable_open(inode, filp);
}

static int hdmi_cec_release(struct inode *inode, struct file *file)
{
	dev_info(&cec_client->dev, "%s\n", __func__);
	return 0;
}

static const struct file_operations hdmi_cec_fops = {
	.owner = THIS_MODULE,
	.open  = hdmi_cec_open,
	.unlocked_ioctl = hdmi_cec_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = hdmi_cec_ioctl,
#endif
	.release = hdmi_cec_release
};
/* ioctl -- */

struct i2c_client *it8566_get_cec_client(void)
{
	return cec_client;
}

struct dentry *it8566_get_debugfs_dir(void)
{
	static struct dentry *d;

	if (!d)
		d = debugfs_create_dir("it8566_hdmi_cec", NULL);

	return d;
}

static void add_debugfs(void)
{
	struct dentry *cecdir, *d;

	cecdir = it8566_get_debugfs_dir();
	if (!cecdir) {
		dev_err(&cec_client->dev, "can not create debugfs dir\n");
		return;
	}
	d = debugfs_create_file("cec_tx", S_IWUSR , cecdir,
			NULL, &dbg_cec_tx_fops);
	if (!d) {
		dev_err(&cec_client->dev, "can not create debugfs cec_tx\n");
		return;
	}
	d = debugfs_create_file("cec_rx", S_IRUSR , cecdir,
			NULL, &dbg_cec_rx_fops);
	if (!d) {
		dev_err(&cec_client->dev, "can not create debugfs cec_rx\n");
		return;
	}
	d = debugfs_create_file("cec_la", S_IWUSR , cecdir,
			NULL, &dbg_cec_la_fops);
	if (!d) {
		dev_err(&cec_client->dev, "can not create debugfs cec_rx\n");
		return;
	}
}

static int it8566_cec_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter;
	int ret;

	dev_info(&client->dev, "%s\n", __func__);
	adapter = to_i2c_adapter(client->dev.parent);

	if (!i2c_check_functionality(adapter,
			I2C_FUNC_SMBUS_BYTE_DATA |
			I2C_FUNC_SMBUS_I2C_BLOCK)) {
		dev_err(&client->dev,
			"I2C adapter %s doesn't support"
			" SMBus BYTE DATA & I2C BLOCK\n",
			adapter->name);
		return -EIO;
	}

	cec_client = client;

	ret = it8566_setup_irq();
	if (ret)
		return ret;

	/*for ioctl*/
	hdmi_cec_device.minor = MISC_DYNAMIC_MINOR;
	hdmi_cec_device.name = "it8566_hdmi_cec";
	hdmi_cec_device.fops = &hdmi_cec_fops;

	ret = misc_register(&hdmi_cec_device);
	if (ret) {
		dev_err(&client->dev,
			"fail to register misc device\n");
		return ret;
	}

	add_debugfs();

	cec_rev_wq = create_workqueue("cec_rev_wq");

	return 0;
}

static int it8566_cec_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id it8566_cec_id[] = {
	{CEC_DEV_NAME, 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, it8566_cec_id);

static struct i2c_driver it8566_i2c_cec_driver = {
	.driver = {
		.name = CEC_DEV_NAME,
	},
	.probe = it8566_cec_probe,
	.remove = it8566_cec_remove,
	.id_table = it8566_cec_id,
};

static int __init it8566_cec_init(void)
{
	return i2c_add_driver(&it8566_i2c_cec_driver);
}

module_init(it8566_cec_init);

static void __exit it8566_cec_exit(void)
{
	i2c_del_driver(&it8566_i2c_cec_driver);
}

module_exit(it8566_cec_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ASUS IT8566 HDMI CEC driver");
MODULE_AUTHOR("Joey SY Chen <joeysy_chen@asus.com>");
