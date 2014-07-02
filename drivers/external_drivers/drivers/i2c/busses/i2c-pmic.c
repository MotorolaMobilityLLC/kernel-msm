/*
 * i2c-pmic.c: PMIC I2C adapter driver.
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
 * Author: Yegnesh Iyer <yegnesh.s.iyer@intel.com>
 */

#include <linux/slab.h>
#include <linux/rpmsg.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/power_supply.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/pm_runtime.h>
#include <asm/intel_scu_pmic.h>
#include <asm/intel_mid_rpmsg.h>
#include <asm/intel_mid_remoteproc.h>
#include "i2c-pmic-regs.h"

#define DRIVER_NAME "i2c_pmic_adap"
#define PMIC_I2C_ADAPTER 8

enum I2C_STATUS {
	I2C_WR = 1,
	I2C_RD,
	I2C_NACK = 4
};

static struct pmic_i2c_dev *pmic_dev;

/* Function Definitions */

/* PMIC I2C read-write completion interrupt handler */
static irqreturn_t pmic_i2c_handler(int irq, void *data)
{
	u8 irq0_int;

	irq0_int = ioread8(pmic_dev->pmic_intr_map);
	irq0_int &= PMIC_I2C_INTR_MASK;

	if (irq0_int) {
		pmic_dev->i2c_rw = (irq0_int >> IRQ0_I2C_BIT_POS);
		return IRQ_WAKE_THREAD;
	}

	return IRQ_NONE;
}


static irqreturn_t pmic_thread_handler(int id, void *data)
{
#define IRQLVL1_MASK_ADDR		0x0c
#define IRQLVL1_CHRGR_MASK		D5

	dev_dbg(pmic_dev->dev, "Clearing IRQLVL1_MASK_ADDR\n");

	intel_scu_ipc_update_register(IRQLVL1_MASK_ADDR, 0x00,
			IRQLVL1_CHRGR_MASK);
	wake_up(&(pmic_dev->i2c_wait));
	return IRQ_HANDLED;
}

/* PMIC i2c read msg */
static inline int pmic_i2c_read_xfer(struct i2c_msg msg)
{
	int ret;
	u16 i;
	u8 mask = (I2C_RD | I2C_NACK);
	u16 regs[I2C_MSG_LEN] = {0};
	u8 data[I2C_MSG_LEN] = {0};

	for (i = 0; i < msg.len ; i++) {
		pmic_dev->i2c_rw = 0;
		regs[0] = I2COVRDADDR_ADDR;
		data[0] = msg.addr;
		regs[1] = I2COVROFFSET_ADDR;
		data[1] = msg.buf[0] + i;
		/* intel_scu_ipc_function works fine for even number of bytes */
		/* Hence adding a dummy byte transfer */
		regs[2] = I2COVROFFSET_ADDR;
		data[2] = msg.buf[0] + i;
		regs[3] = I2COVRCTRL_ADDR;
		data[3] = I2COVRCTRL_I2C_RD;
		ret = intel_scu_ipc_writev(regs, data, I2C_MSG_LEN);
		if (unlikely(ret))
			return ret;

		ret = wait_event_timeout(pmic_dev->i2c_wait,
				(pmic_dev->i2c_rw & mask),
				HZ);

		if (ret == 0) {
			ret = -ETIMEDOUT;
			goto read_err_exit;
		} else if (pmic_dev->i2c_rw == I2C_NACK) {
			ret = -EIO;
			goto read_err_exit;
		} else {
			ret = intel_scu_ipc_ioread8(I2COVRRDDATA_ADDR,
					&(msg.buf[i]));
			if (unlikely(ret)) {
				ret = -EIO;
				goto read_err_exit;
			}
		}
	}
	return 0;

read_err_exit:
	return ret;
}

/* PMIC i2c write msg */
static inline int pmic_i2c_write_xfer(struct i2c_msg msg)
{
	int ret;
	u16 i;
	u8 mask = (I2C_WR | I2C_NACK);
	u16 regs[I2C_MSG_LEN] = {0};
	u8 data[I2C_MSG_LEN] = {0};

	for (i = 1; i <= msg.len ; i++) {
		pmic_dev->i2c_rw = 0;
		regs[0] = I2COVRDADDR_ADDR;
		data[0] = msg.addr;
		regs[1] = I2COVRWRDATA_ADDR;
		data[1] = msg.buf[i];
		regs[2] = I2COVROFFSET_ADDR;
		data[2] = msg.buf[0] + i - 1;
		regs[3] = I2COVRCTRL_ADDR;
		data[3] = I2COVRCTRL_I2C_WR;
		ret = intel_scu_ipc_writev(regs, data, I2C_MSG_LEN);
		if (unlikely(ret))
			return ret;

		ret = wait_event_timeout(pmic_dev->i2c_wait,
				(pmic_dev->i2c_rw & mask),
				HZ);
		if (ret == 0)
			return -ETIMEDOUT;
		else if (pmic_dev->i2c_rw == I2C_NACK)
			return -EIO;
	}
	return 0;
}

static int (*xfer_fn[]) (struct i2c_msg) = {
	pmic_i2c_write_xfer,
	pmic_i2c_read_xfer
};

/* PMIC I2C Master transfer algorithm function */
static int pmic_master_xfer(struct i2c_adapter *adap,
				struct i2c_msg msgs[],
				int num)
{
	int ret = 0;
	int i;
	u8 index;

	mutex_lock(&pmic_dev->i2c_pmic_rw_lock);
	wake_lock(&pmic_dev->i2c_wake_lock);
	pm_runtime_get_sync(pmic_dev->dev);
	for (i = 0 ; i < num ; i++) {
		index = msgs[i].flags & I2C_M_RD;
		ret = (xfer_fn[index])(msgs[i]);

		if (ret == -EACCES)
			dev_info(pmic_dev->dev, "Blocked Access!\n");

		/* If access is restricted, return true to
		*  avoid extra error handling in client
		*/

		if (ret != 0 && ret != -EACCES)
			goto transfer_err_exit;
	}

	ret = num;

transfer_err_exit:
	mutex_unlock(&pmic_dev->i2c_pmic_rw_lock);
	pm_runtime_put_sync(pmic_dev->dev);
	wake_unlock(&pmic_dev->i2c_wake_lock);
	intel_scu_ipc_update_register(IRQLVL1_MASK_ADDR, 0x00,
			IRQLVL1_CHRGR_MASK);
	return ret;
}

/* PMIC I2C adapter capability function */
static u32 pmic_master_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_BYTE_DATA;
}

static int pmic_smbus_xfer(struct i2c_adapter *adap, u16 addr,
				unsigned short flags, char read_write,
				u8 command, int size,
				union i2c_smbus_data *data)
{
	struct i2c_msg msg;
	u8 buf[2];
	int ret;

	msg.addr = addr;
	msg.flags = flags & I2C_M_TEN;
	msg.buf = buf;
	msg.buf[0] = command;
	if (read_write == I2C_SMBUS_WRITE) {
		msg.len = 1;
		msg.buf[1] = data->byte;
	} else {
		msg.flags |= I2C_M_RD;
		msg.len = 1;
	}

	ret = pmic_master_xfer(adap, &msg, 1);
	if (ret == 1) {
		if (read_write == I2C_SMBUS_READ)
			data->byte = msg.buf[0];
		return 0;
	}
	return ret;
}


static const struct i2c_algorithm pmic_i2c_algo = {
	.master_xfer = pmic_master_xfer,
	.functionality = pmic_master_func,
	.smbus_xfer = pmic_smbus_xfer,
};

static int pmic_i2c_probe(struct platform_device *pdev)
{
	struct i2c_adapter *adap;
	int ret;

	pmic_dev = kzalloc(sizeof(struct pmic_i2c_dev), GFP_KERNEL);
	if (!pmic_dev)
		return -ENOMEM;

	pmic_dev->dev = &pdev->dev;
	pmic_dev->irq = platform_get_irq(pdev, 0);



	mutex_init(&pmic_dev->i2c_pmic_rw_lock);
	wake_lock_init(&pmic_dev->i2c_wake_lock, WAKE_LOCK_SUSPEND,
			"pmic_i2c_wake_lock");
	init_waitqueue_head(&(pmic_dev->i2c_wait));

	pmic_dev->pmic_intr_map = ioremap_nocache(PMIC_SRAM_INTR_ADDR, 8);
	if (!pmic_dev->pmic_intr_map) {
		dev_err(&pdev->dev, "ioremap Failed\n");
		ret = -ENOMEM;
		goto ioremap_failed;
	}
	ret = request_threaded_irq(pmic_dev->irq, pmic_i2c_handler,
					pmic_thread_handler,
					IRQF_SHARED|IRQF_NO_SUSPEND,
					DRIVER_NAME, pmic_dev);
	if (ret)
		goto err_irq_request;

	ret = intel_scu_ipc_update_register(IRQLVL1_MASK_ADDR, 0x00,
			IRQLVL1_CHRGR_MASK);
	if (unlikely(ret))
		goto unmask_irq_failed;
	ret = intel_scu_ipc_update_register(MCHGRIRQ0_ADDR, 0x00,
			PMIC_I2C_INTR_MASK);
	if (unlikely(ret))
		goto unmask_irq_failed;

	/* Init runtime PM state*/
	pm_runtime_put_noidle(pmic_dev->dev);

	adap = &pmic_dev->adapter;
	adap->owner = THIS_MODULE;
	adap->class = I2C_CLASS_HWMON;
	adap->algo = &pmic_i2c_algo;
	strcpy(adap->name, "PMIC I2C Adapter");
	adap->nr = PMIC_I2C_ADAPTER;
	ret = i2c_add_numbered_adapter(adap);

	if (ret) {
		dev_err(&pdev->dev, "Error adding the adapter\n");
		goto err_adap_add;
	}

	pm_schedule_suspend(pmic_dev->dev, MSEC_PER_SEC);
	return 0;

err_adap_add:
	free_irq(pmic_dev->irq, pmic_dev);
unmask_irq_failed:
err_irq_request:
	iounmap(pmic_dev->pmic_intr_map);
ioremap_failed:
	kfree(pmic_dev);
	return ret;
}

static int pmic_i2c_remove(struct platform_device *pdev)
{
	iounmap(pmic_dev->pmic_intr_map);
	free_irq(pmic_dev->irq, pmic_dev);
	pm_runtime_get_noresume(pmic_dev->dev);
	kfree(pmic_dev);
	return 0;
}

#ifdef CONFIG_PM
static int pmic_i2c_suspend(struct device *dev)
{
	dev_info(dev, "%s\n", __func__);
	return 0;
}

static int pmic_i2c_resume(struct device *dev)
{
	dev_info(dev, "%s\n", __func__);
	return 0;
}
#endif

#ifdef CONFIG_PM_RUNTIME
static int pmic_i2c_runtime_suspend(struct device *dev)
{
	dev_info(dev, "%s\n", __func__);
	return 0;
}

static int pmic_i2c_runtime_resume(struct device *dev)
{
	dev_info(dev, "%s\n", __func__);
	return 0;
}

static int pmic_i2c_runtime_idle(struct device *dev)
{
	dev_info(dev, "%s\n", __func__);
	return 0;
}
#endif

static const struct dev_pm_ops pmic_i2c_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(pmic_i2c_suspend,
				pmic_i2c_resume)
	SET_RUNTIME_PM_OPS(pmic_i2c_runtime_suspend,
				pmic_i2c_runtime_resume,
				pmic_i2c_runtime_idle)
};

struct platform_driver pmic_i2c_driver = {
	.probe = pmic_i2c_probe,
	.remove = pmic_i2c_remove,
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.pm = &pmic_i2c_pm_ops,
	},
};

static int pmic_i2c_init(void)
{
	return platform_driver_register(&pmic_i2c_driver);
}

static void pmic_i2c_exit(void)
{
	platform_driver_unregister(&pmic_i2c_driver);
}

static int pmic_i2c_rpmsg_probe(struct rpmsg_channel *rpdev)
{
	int ret = 0;

	if (rpdev == NULL) {
		pr_err("rpmsg channel not created\n");
		ret = -ENODEV;
		goto out;
	}

	dev_info(&rpdev->dev, "Probed pmic_i2c rpmsg device\n");

	ret = pmic_i2c_init();

out:
	return ret;
}

static void pmic_i2c_rpmsg_remove(struct rpmsg_channel *rpdev)
{
	pmic_i2c_exit();
	dev_info(&rpdev->dev, "Removed pmic_i2c rpmsg device\n");
}

static void pmic_i2c_rpmsg_cb(struct rpmsg_channel *rpdev, void *data,
					int len, void *priv, u32 src)
{
	dev_warn(&rpdev->dev, "unexpected, message\n");

	print_hex_dump(KERN_DEBUG, __func__, DUMP_PREFIX_NONE, 16, 1,
		       data, len,  true);
}

static struct rpmsg_device_id pmic_i2c_rpmsg_id_table[] = {
	{ .name	= "rpmsg_i2c_pmic_adap" },
	{ },
};
MODULE_DEVICE_TABLE(rpmsg, pmic_i2c_rpmsg_id_table);

static struct rpmsg_driver pmic_i2c_rpmsg = {
	.drv.name	= KBUILD_MODNAME,
	.drv.owner	= THIS_MODULE,
	.id_table	= pmic_i2c_rpmsg_id_table,
	.probe		= pmic_i2c_rpmsg_probe,
	.callback	= pmic_i2c_rpmsg_cb,
	.remove		= pmic_i2c_rpmsg_remove,
};

static int __init pmic_i2c_rpmsg_init(void)
{
	return register_rpmsg_driver(&pmic_i2c_rpmsg);
}

static void __exit pmic_i2c_rpmsg_exit(void)
{
	return unregister_rpmsg_driver(&pmic_i2c_rpmsg);
}
module_init(pmic_i2c_rpmsg_init);
module_exit(pmic_i2c_rpmsg_exit);

MODULE_AUTHOR("Yegnesh Iyer <yegnesh.s.iyer@intel.com");
MODULE_DESCRIPTION("PMIC I2C Master driver");
MODULE_LICENSE("GPL");
