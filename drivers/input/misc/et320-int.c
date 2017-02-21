/*
 * Simple synchronous userspace interface to SPI devices
 *
 * Copyright (C) 2006 SWAPP
 *	Andrea Paterniani <a.paterniani@swapp-eng.it>
 * Copyright (C) 2007 David Brownell (simplification, cleanup)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version 2
 * as published by the Free Software Foundation.
 */

/*
*
*  et320.spi.c
*  Date: 2016/03/16
*  Version: 0.9.0.1
*  Revise Date:  2016/10/20
*  Copyright (C) 2007-2016 Egis Technology Inc.
*
*/


#include <linux/interrupt.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif

#include <linux/gpio.h>
#include <linux/mutex.h>
#include <linux/list.h>

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/spi/spi.h>
#include <soc/qcom/scm.h>
#include <linux/wakelock.h>

#include "et320.h"
#include "navi_input.h"

#define EDGE_TRIGGER_FALLING    0x0
#define	EDGE_TRIGGER_RAISING    0x1
#define	LEVEL_TRIGGER_LOW       0x2
#define	LEVEL_TRIGGER_HIGH      0x3
#define EGIS_NAVI_INPUT 1  /* 1:open ; 0:close */
struct wake_lock et320_wake_lock;

/*
 * FPS interrupt table
 */

struct interrupt_desc fps_ints = {0 , 0, "BUT0" , 0};

unsigned int bufsiz = 4096;

int gpio_irq;
int request_irq_done = 0;
/* int t_mode = 255; */

#define EDGE_TRIGGER_FALLING    0x0
#define EDGE_TRIGGER_RISING    0x1
#define LEVEL_TRIGGER_LOW       0x2
#define LEVEL_TRIGGER_HIGH      0x3


struct ioctl_cmd {
int int_mode;
int detect_period;
int detect_threshold;
};
/* To be compatible with fpc driver */
#ifndef CONFIG_SENSORS_FPC_1020
static struct FPS_data {
	unsigned int enabled;
	unsigned int state;
	struct blocking_notifier_head nhead;
} *fpsData;

struct FPS_data *FPS_init(void)
{
	struct FPS_data *mdata;
	if (!fpsData) {
		mdata = kzalloc(
				sizeof(struct FPS_data), GFP_KERNEL);
		if (mdata) {
			BLOCKING_INIT_NOTIFIER_HEAD(&mdata->nhead);
			pr_debug("%s: FPS notifier data structure init-ed\n", __func__);
		}
		fpsData = mdata;
	}
	return fpsData;
}
int FPS_register_notifier(struct notifier_block *nb,
	unsigned long stype, bool report)
{
	int error;
	struct FPS_data *mdata = fpsData;

	mdata = FPS_init();
	if (!mdata)
		return -ENODEV;
	mdata->enabled = (unsigned int)stype;
	pr_debug("%s: FPS sensor %lu notifier enabled\n", __func__, stype);

	error = blocking_notifier_chain_register(&mdata->nhead, nb);
	if (!error && report) {
		int state = mdata->state;
		/* send current FPS state on register request */
		blocking_notifier_call_chain(&mdata->nhead,
				stype, (void *)&state);
		pr_debug("%s: FPS reported state %d\n", __func__, state);
	}
	return error;
}
EXPORT_SYMBOL_GPL(FPS_register_notifier);

int FPS_unregister_notifier(struct notifier_block *nb,
		unsigned long stype)
{
	int error;
	struct FPS_data *mdata = fpsData;

	if (!mdata)
		return -ENODEV;

	error = blocking_notifier_chain_unregister(&mdata->nhead, nb);
	pr_debug("%s: FPS sensor %lu notifier unregister\n", __func__, stype);

	if (!mdata->nhead.head) {
		mdata->enabled = 0;
		pr_debug("%s: FPS sensor %lu no clients\n", __func__, stype);
	}

	return error;
}
EXPORT_SYMBOL_GPL(FPS_unregister_notifier);

void FPS_notify(unsigned long stype, int state)
{
	struct FPS_data *mdata = fpsData;

	pr_debug("%s: Enter", __func__);

	if (!mdata) {
		pr_err("%s: FPS notifier not initialized yet\n", __func__);
		return;
	}

	pr_debug("%s: FPS current state %d -> (0x%x)\n", __func__,
		mdata->state, state);

	if (mdata->enabled && mdata->state != state) {
		mdata->state = state;
		blocking_notifier_call_chain(&mdata->nhead,
						stype, (void *)&state);
		pr_debug("%s: FPS notification sent\n", __func__);
	} else if (!mdata->enabled) {
		pr_err("%s: !mdata->enabled", __func__);
	} else {
		pr_err("%s: mdata->state==state", __func__);
	}
}
#endif

static struct etspi_data *g_data;

DECLARE_BITMAP(minors, N_SPI_MINORS);
LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);

/* ------------------------------ Interrupt -----------------------------*/
/*
 * Interrupt description
 */

#define FP_INT_DETECTION_PERIOD  10
#define FP_DETECTION_THRESHOLD	10
/* struct interrupt_desc fps_ints; */
static DECLARE_WAIT_QUEUE_HEAD(interrupt_waitq);
/*
 *	FUNCTION NAME.
 *		interrupt_timer_routine
 *
 *	FUNCTIONAL DESCRIPTION.
 *		basic interrupt timer inital routine
 *
 *	ENTRY PARAMETERS.
 *		gpio - gpio address
 *
 *	EXIT PARAMETERS.
 *		Function Return
 */

void interrupt_timer_routine(unsigned long _data)
{
	struct interrupt_desc *bdata = (struct interrupt_desc *)_data;

	DEBUG_PRINT("FPS interrupt count = %d", bdata->int_count);
	if (bdata->int_count >= bdata->detect_threshold) {
		bdata->finger_on = 1;
		DEBUG_PRINT("FPS triggered !!!!!!!\n");
	} else {
		DEBUG_PRINT("FPS not triggered !!!!!!!\n");
	}

	bdata->int_count = 0;
	wake_up_interruptible(&interrupt_waitq);
}

static irqreturn_t fp_eint_func(int irq, void *dev_id)
{
	if (!fps_ints.int_count)
		mod_timer(&fps_ints.timer, jiffies + msecs_to_jiffies(fps_ints.detect_period));
	fps_ints.int_count++;
	/* printk_ratelimited(KERN_WARNING "-----------   zq fp fp_eint_func  ,fps_ints.int_count=%d",fps_ints.int_count);*/
	wake_lock_timeout(&et320_wake_lock, msecs_to_jiffies(1500));
	return IRQ_HANDLED;
}

static irqreturn_t fp_eint_func_ll(int irq , void *dev_id)
{
	DEBUG_PRINT("[egis]fp_eint_func_ll\n");
	fps_ints.finger_on = 1;
	/* fps_ints.int_count = 0; */
	disable_irq_nosync(gpio_irq);
	fps_ints.drdy_irq_flag = DRDY_IRQ_DISABLE;
	wake_up_interruptible(&interrupt_waitq);
	/* printk_ratelimited(KERN_WARNING "-----------   zq fp fp_eint_func  ,fps_ints.int_count=%d",fps_ints.int_count);*/
	wake_lock_timeout(&et320_wake_lock, msecs_to_jiffies(1500));
	return IRQ_RETVAL(IRQ_HANDLED);
}

/*
 *	FUNCTION NAME.
 *		Interrupt_Init
 *
 *	FUNCTIONAL DESCRIPTION.
 *		button initial routine
 *
 *	ENTRY PARAMETERS.
 *		int_mode - determine trigger mode
 *			EDGE_TRIGGER_FALLING    0x0
 *			EDGE_TRIGGER_RAISING    0x1
 *			LEVEL_TRIGGER_LOW        0x2
 *			LEVEL_TRIGGER_HIGH       0x3
 *
 *	EXIT PARAMETERS.
 *		Function Return int
 */

int Interrupt_Init(struct etspi_data *etspi, int int_mode, int detect_period, int detect_threshold)
{

	int err = 0;
	int status = 0;
	DEBUG_PRINT("FP --  %s mode = %d period = %d threshold = %d\n", __func__, int_mode, detect_period, detect_threshold);
	DEBUG_PRINT("FP --  %s request_irq_done = %d gpio_irq = %d  pin = %d  \n", __func__, request_irq_done, gpio_irq, etspi->irqPin);


	fps_ints.detect_period = detect_period;
	fps_ints.detect_threshold = detect_threshold;
	fps_ints.int_count = 0;
	fps_ints.finger_on = 0;


	if (request_irq_done == 0)	{
		gpio_irq = gpio_to_irq(etspi->irqPin);
		if (gpio_irq < 0) {
			DEBUG_PRINT("%s gpio_to_irq failed\n", __func__);
			status = gpio_irq;
			goto done;
		}

		DEBUG_PRINT("[Interrupt_Init] flag current: %d disable: %d enable: %d\n",
		fps_ints.drdy_irq_flag, DRDY_IRQ_DISABLE, DRDY_IRQ_ENABLE);
		/* t_mode = int_mode; */
		if (int_mode == EDGE_TRIGGER_RISING) {
			DEBUG_PRINT("%s EDGE_TRIGGER_RISING\n", __func__);
			err = request_irq(gpio_irq, fp_eint_func, IRQ_TYPE_EDGE_RISING, "fp_detect-eint", etspi);
			if (err) {
				pr_err("request_irq failed==========%s,%d\n", __func__, __LINE__);
			}
		} else if (int_mode == EDGE_TRIGGER_FALLING) {
			DEBUG_PRINT("%s EDGE_TRIGGER_FALLING\n", __func__);
			err = request_irq(gpio_irq, fp_eint_func, IRQ_TYPE_EDGE_FALLING, "fp_detect-eint", etspi);
			if (err) {
				pr_err("request_irq failed==========%s,%d\n", __func__, __LINE__);
			}
		} else if (int_mode == LEVEL_TRIGGER_LOW) {
			DEBUG_PRINT("%s LEVEL_TRIGGER_LOW\n", __func__);
			err = request_irq(gpio_irq, fp_eint_func_ll, IRQ_TYPE_LEVEL_LOW, "fp_detect-eint", etspi);
			if (err) {
				pr_err("request_irq failed==========%s,%d\n", __func__, __LINE__);
			}
		} else if (int_mode == LEVEL_TRIGGER_HIGH) {
			DEBUG_PRINT("%s LEVEL_TRIGGER_HIGH\n", __func__);
			err = request_irq(gpio_irq, fp_eint_func_ll, IRQ_TYPE_LEVEL_HIGH, "fp_detect-eint", etspi);
			if (err) {
				pr_err("request_irq failed==========%s,%d\n", __func__, __LINE__);
			}
		}
		DEBUG_PRINT("[Interrupt_Init]:gpio_to_irq return: %d\n", gpio_irq);
		DEBUG_PRINT("[Interrupt_Init]:request_irq return: %d\n", err);
		/* disable_irq_nosync(gpio_irq); */
		fps_ints.drdy_irq_flag = DRDY_IRQ_ENABLE;
		enable_irq_wake(gpio_irq);
		request_irq_done = 1;
	}


	if (fps_ints.drdy_irq_flag == DRDY_IRQ_DISABLE) {
		fps_ints.drdy_irq_flag = DRDY_IRQ_ENABLE;
		enable_irq_wake(gpio_irq);
		enable_irq(gpio_irq);
	}
done:
	return 0;
}

/*
 *	FUNCTION NAME.
 *		Interrupt_Free
 *
 *	FUNCTIONAL DESCRIPTION.
 *		free all interrupt resource
 *
 *	EXIT PARAMETERS.
 *		Function Return int
 */

int Interrupt_Free(struct etspi_data *etspi)
{
	DEBUG_PRINT("%s\n", __func__);
	fps_ints.finger_on = 0;

	if (fps_ints.drdy_irq_flag == DRDY_IRQ_ENABLE) {
		DEBUG_PRINT("%s (DISABLE IRQ)\n", __func__);
		disable_irq_nosync(gpio_irq);
		/* disable_irq(gpio_irq); */
		del_timer_sync(&fps_ints.timer);
		fps_ints.drdy_irq_flag = DRDY_IRQ_DISABLE;
	}
	return 0;
}




/*
 *	FUNCTION NAME.
 *		fps_interrupt_re d
 *
 *	FUNCTIONAL DESCRIPTION.
 *		FPS interrupt read status
 *
 *	ENTRY PARAMETERS.
 *		wait poll table structure
 *
 *	EXIT PARAMETERS.
 *		Function Return int
 */

unsigned int fps_interrupt_poll(
struct file *file,
struct poll_table_struct *wait)
{
	unsigned int mask = 0;

	/* DEBUG_PRINT("%s %d\n", __func__, fps_ints.finger_on);*/
	/* fps_ints.int_count = 0; */
	poll_wait(file, &interrupt_waitq, wait);
	if (fps_ints.finger_on) {
		mask |= POLLIN | POLLRDNORM;
		/* fps_ints.finger_on = 0; */
	}
	return mask;
}

void fps_interrupt_abort(void)
{
	DEBUG_PRINT("%s\n", __func__);
	fps_ints.finger_on = 0;
	wake_up_interruptible(&interrupt_waitq);
}

/*-------------------------------------------------------------------------*/

static void etspi_reset(struct etspi_data *etspi)
{
	DEBUG_PRINT("%s\n", __func__);
	gpio_set_value(etspi->rstPin, 0);
	msleep(30);
	gpio_set_value(etspi->rstPin, 1);
	msleep(20);
}

static ssize_t etspi_read(struct file *filp,
	char __user *buf,
	size_t count,
	loff_t *f_pos)
{
	/*Implement by vendor if needed*/
	return 0;
}

static ssize_t etspi_write(struct file *filp,
	const char __user *buf,
	size_t count,
	loff_t *f_pos)
{
	/*Implement by vendor if needed*/
	return 0;
}
static ssize_t etspi_enable_set(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int state = (*buf == '1') ? 1 : 0;
	FPS_notify(0xbeef, state);
	DEBUG_PRINT("%s  state = %d\n", __func__, state);
	return 1;
}
static DEVICE_ATTR(etspi_enable, S_IWUSR | S_IWGRP, NULL, etspi_enable_set);
static struct attribute *attributes[] = {
	&dev_attr_etspi_enable.attr,
	NULL
};

static const struct attribute_group attribute_group = {
	.attrs = attributes,
};
static long etspi_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{

	int retval = 0;
	struct etspi_data *etspi;
	struct ioctl_cmd data;

	memset(&data, 0, sizeof(data));

	DEBUG_PRINT("%s\n", __func__);

	etspi = filp->private_data;

	switch (cmd) {
	case INT_TRIGGER_INIT:
		if (copy_from_user(&data, (int __user *)arg, sizeof(data))) {
			retval = -EFAULT;
			goto done;
		}

		DEBUG_PRINT("fp_ioctl >>> fp Trigger function init\n");
		retval = Interrupt_Init(etspi, data.int_mode, data.detect_period, data.detect_threshold);
		DEBUG_PRINT("fp_ioctl trigger init = %x\n", retval);
	break;

	case FP_SENSOR_RESET:
			DEBUG_PRINT("fp_ioctl ioc->opcode == FP_SENSOR_RESET --");
			etspi_reset(etspi);
		goto done;
	case INT_TRIGGER_CLOSE:
			DEBUG_PRINT("fp_ioctl <<< fp Trigger function close\n");
			retval = Interrupt_Free(etspi);
			DEBUG_PRINT("fp_ioctl trigger close = %x\n", retval);
		goto done;
	case INT_TRIGGER_ABORT:
			DEBUG_PRINT("fp_ioctl <<< fp Trigger function close\n");
			fps_interrupt_abort();
		goto done;
	default:
	retval = -ENOTTY;
	break;
	}
done:
	return retval;
}

#ifdef CONFIG_COMPAT
static long etspi_compat_ioctl(struct file *filp,
	unsigned int cmd,
	unsigned long arg)
{
	return etspi_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#else
#define etspi_compat_ioctl NULL
#endif /* CONFIG_COMPAT */

static int etspi_open(struct inode *inode, struct file *filp)
{
	struct etspi_data *etspi;
	int			status = -ENXIO;

	DEBUG_PRINT("%s\n", __func__);

	mutex_lock(&device_list_lock);

	list_for_each_entry(etspi, &device_list, device_entry)	{
		if (etspi->devt == inode->i_rdev) {
			status = 0;
			break;
		}
	}
	if (status == 0) {
		if (etspi->buffer == NULL) {
			etspi->buffer = kmalloc(bufsiz, GFP_KERNEL);
			if (etspi->buffer == NULL) {
				/* dev_dbg(&etspi->spi->dev, "open/ENOMEM\n"); */
				status = -ENOMEM;
			}
		}
		if (status == 0) {
			etspi->users++;
			filp->private_data = etspi;
			nonseekable_open(inode, filp);
		}
	} else {
		pr_debug("%s nothing for minor %d\n"
			, __func__, iminor(inode));
	}
	mutex_unlock(&device_list_lock);
	return status;
}

static int etspi_release(struct inode *inode, struct file *filp)
{
	struct etspi_data *etspi;

	DEBUG_PRINT("%s\n", __func__);

	mutex_lock(&device_list_lock);
	etspi = filp->private_data;
	filp->private_data = NULL;

	/* last close? */
	etspi->users--;
	if (etspi->users == 0) {
		int	dofree;

		kfree(etspi->buffer);
		etspi->buffer = NULL;

		/* ... after we unbound from the underlying device? */
		spin_lock_irq(&etspi->spi_lock);
		dofree = (etspi->spi == NULL);
		spin_unlock_irq(&etspi->spi_lock);

		if (dofree)
			kfree(etspi);
	}
	mutex_unlock(&device_list_lock);
	return 0;

}



int etspi_platformInit(struct etspi_data *etspi)
{
	int status = 0;
	DEBUG_PRINT("%s\n", __func__);

	if (etspi != NULL) {

		/* Initial Reset Pin*/
		status = gpio_request(etspi->rstPin, "reset-gpio");
		if (status < 0) {
			pr_err("%s gpio_requset etspi_Reset failed\n",
				__func__);
			goto etspi_platformInit_rst_failed;
		}
		gpio_direction_output(etspi->rstPin, 1);
		if (status < 0) {
			pr_err("%s gpio_direction_output Reset failed\n",
					__func__);
			status = -EBUSY;
			goto etspi_platformInit_rst_failed;
		}
		/* gpio_set_value(etspi->rstPin, 1); */
		pr_err("et320:  reset to high\n");
		/* initial 33V power pin */
		status = gpio_request(etspi->vcc_33v_Pin, "33v-gpio");
		if (status < 0) {
			pr_err("%s gpio_requset vcc_33v_Pin failed\n",
				__func__);
			goto etspi_platformInit_rst_failed;
		}
		gpio_direction_output(etspi->vcc_33v_Pin, 1);
		if (status < 0) {
			pr_err("%s gpio_direction_output vcc_33v_Pin failed\n",
					__func__);
			status = -EBUSY;
			goto etspi_platformInit_rst_failed;
		}
		gpio_set_value(etspi->vcc_33v_Pin, 1);
		pr_err("ets320:  vcc_33v_Pin set to high\n");

		/* initial 18V power pin */
		status = gpio_request(etspi->vdd_18v_Pin, "18v-gpio");
		if (status < 0) {
			pr_err("%s gpio_requset vdd_18v_Pin failed\n",
				__func__);
			goto etspi_platformInit_rst_failed;
		}
		gpio_direction_output(etspi->vdd_18v_Pin, 1);
		if (status < 0) {
			pr_err("%s gpio_direction_output vdd_18v_Pin failed\n",
					__func__);
			status = -EBUSY;
			goto etspi_platformInit_rst_failed;
		}

		gpio_set_value(etspi->vdd_18v_Pin, 1);
		pr_err("ets320:  vdd_18v_Pin set to high\n");
		/* Initial IRQ Pin*/
		status = gpio_request(etspi->irqPin, "irq-gpio");
		if (status < 0) {
			pr_err("%s gpio_request etspi_irq failed\n",
				__func__);
			goto etspi_platformInit_irq_failed;
		}
		status = gpio_direction_input(etspi->irqPin);
		if (status < 0) {
			pr_err("%s gpio_direction_input IRQ failed\n",
				__func__);
			goto etspi_platformInit_gpio_init_failed;
		}

	}
	DEBUG_PRINT("ets320: %s successful status=%d\n", __func__, status);
	return status;

etspi_platformInit_gpio_init_failed:
	gpio_free(etspi->irqPin);
	gpio_free(etspi->vcc_33v_Pin);
	gpio_free(etspi->vdd_18v_Pin);
etspi_platformInit_irq_failed:
	gpio_free(etspi->rstPin);
etspi_platformInit_rst_failed:

	pr_err("%s is failed\n", __func__);
	return status;
}

static int etspi_parse_dt(struct device *dev,
	struct etspi_data *data)
{
	struct device_node *np = dev->of_node;
	int errorno = 0;
	int gpio;

	gpio = of_get_named_gpio(np, "egistec,gpio_rst", 0);
	if (gpio < 0) {
		errorno = gpio;
		goto dt_exit;
	} else {
		data->rstPin = gpio;
		DEBUG_PRINT("%s: sleepPin=%d\n", __func__, data->rstPin);
	}
	gpio = of_get_named_gpio(np, "egistec,gpio_irq", 0);
	if (gpio < 0) {
		errorno = gpio;
		goto dt_exit;
	} else {
		data->irqPin = gpio;
		DEBUG_PRINT("%s: drdyPin=%d\n", __func__, data->irqPin);
	}

	gpio = of_get_named_gpio(np, "egistec,gpio_ldo3p3_en", 0);
	if (gpio < 0) {
		errorno = gpio;
		goto dt_exit;
	} else {
		data->vcc_33v_Pin = gpio;
		pr_info("%s: 3.3v power pin=%d\n", __func__, data->vcc_33v_Pin);
	}
	gpio = of_get_named_gpio(np, "egistec,gpio_ldo1p8_en", 0);
	if (gpio < 0) {
		errorno = gpio;
		goto dt_exit;
	} else {
		data->vdd_18v_Pin = gpio;
		pr_info("%s: 18v power pin=%d\n", __func__, data->vdd_18v_Pin);
	}
	DEBUG_PRINT("%s is successful\n", __func__);
	return errorno;
dt_exit:
	pr_err("%s is failed\n", __func__);
	return errorno;
}

static const struct file_operations etspi_fops = {
	.owner = THIS_MODULE,
	.write = etspi_write,
	.read = etspi_read,
	.unlocked_ioctl = etspi_ioctl,
	.compat_ioctl = etspi_compat_ioctl,
	.open = etspi_open,
	.release = etspi_release,
	.llseek = no_llseek,
	.poll = fps_interrupt_poll
};

/*-------------------------------------------------------------------------*/

static struct class *etspi_class;

static int etspi_probe(struct platform_device *pdev);
static int etspi_remove(struct platform_device *pdev);

static struct of_device_id etspi_match_table[] = {
	{ .compatible = "egistec,et320",},
	{},
};
MODULE_DEVICE_TABLE(of, etspi_match_table);

/* fp_id is used only when dual sensor need to be support  */
#if 0
static struct of_device_id fp_id_match_table[] = {
	{ .compatible = "fp_id,fp_id",},
	{},
};
MODULE_DEVICE_TABLE(of, fp_id_match_table);
#endif



static struct platform_driver etspi_driver = {
	.driver = {
		.name		= "et320",
		.owner		= THIS_MODULE,
		.of_match_table = etspi_match_table,
	},
    .probe =    etspi_probe,
    .remove =   etspi_remove,
};
/* remark for dual sensors */
/* module_platform_driver(etspi_driver); */



static int etspi_remove(struct platform_device *pdev)
{
	DEBUG_PRINT("%s(#%d)\n", __func__, __LINE__);
	free_irq(gpio_irq, NULL);
	del_timer_sync(&fps_ints.timer);
	wake_lock_destroy(&et320_wake_lock);
	request_irq_done = 0;
	/* t_mode = 255; */
	return 0;
}

static int etspi_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct etspi_data *etspi;
	int status = 0;
	unsigned long minor;
	/* int retval; */

	DEBUG_PRINT("%s initial\n", __func__);

	BUILD_BUG_ON(N_SPI_MINORS > 256);
	status = register_chrdev(ET320_MAJOR, "et320", &etspi_fops);
	if (status < 0) {
		pr_err("%s register_chrdev error.\n", __func__);
		return status;
	}

	etspi_class = class_create(THIS_MODULE, "et320");
	if (IS_ERR(etspi_class)) {
		pr_err("%s class_create error.\n", __func__);
		unregister_chrdev(ET320_MAJOR, etspi_driver.driver.name);
		return PTR_ERR(etspi_class);
	}

	/* Allocate driver data */
	etspi = kzalloc(sizeof(*etspi), GFP_KERNEL);
	if (etspi == NULL) {
		pr_err("%s - Failed to kzalloc\n", __func__);
		return -ENOMEM;
	}

	/* device tree call */
	if (pdev->dev.of_node) {
		status = etspi_parse_dt(&pdev->dev, etspi);
		if (status) {
			pr_err("%s - Failed to parse DT\n", __func__);
			goto etspi_probe_parse_dt_failed;
		}
	}

	/* Initialize the driver data */
	etspi->spi = pdev;
	g_data = etspi;

	spin_lock_init(&etspi->spi_lock);
	mutex_init(&etspi->buf_lock);
	mutex_init(&device_list_lock);

	INIT_LIST_HEAD(&etspi->device_entry);

	/* platform init */
	status = etspi_platformInit(etspi);
	if (status != 0) {
		pr_err("%s platforminit failed\n", __func__);
		goto etspi_probe_platformInit_failed;
	}

	fps_ints.drdy_irq_flag = DRDY_IRQ_DISABLE;

#ifdef ETSPI_NORMAL_MODE
/*
	spi->bits_per_word = 8;
	spi->max_speed_hz = SLOW_BAUD_RATE;
	spi->mode = SPI_MODE_0;
	spi->chip_select = 0;
	status = spi_setup(spi);
	if (status != 0) {
		pr_err("%s spi_setup() is failed. status : %d\n",
			__func__, status);
		return status;
	}
*/
#endif

	/*
	 * If we can allocate a minor number, hook up this device.
	 * Reusing minors is fine so long as udev or mdev is working.
	 */
	mutex_lock(&device_list_lock);
	minor = find_first_zero_bit(minors, N_SPI_MINORS);
	if (minor < N_SPI_MINORS) {
		struct device *fdev;
		etspi->devt = MKDEV(ET320_MAJOR, minor);
		fdev = device_create(etspi_class, &pdev->dev, etspi->devt, etspi, "esfp0");
		status = IS_ERR(fdev) ? PTR_ERR(fdev) : 0;
	} else {
		dev_dbg(&pdev->dev, "no minor number available!\n");
		status = -ENODEV;
	}
	if (status == 0) {
		set_bit(minor, minors);
		list_add(&etspi->device_entry, &device_list);
	}

#if EGIS_NAVI_INPUT
	/*
	 * William Add.
	 */
	sysfs_egis_init(etspi);
	uinput_egis_init(etspi);
#endif

	mutex_unlock(&device_list_lock);

	if (status == 0) {
		/* spi_set_drvdata(pdev, etspi); */
		dev_set_drvdata(dev, etspi);
	} else {
		goto etspi_probe_failed;
	}
	etspi_reset(etspi);

	fps_ints.drdy_irq_flag = DRDY_IRQ_DISABLE;

	/* the timer is for ET310 */
	setup_timer(&fps_ints.timer, interrupt_timer_routine, (unsigned long)&fps_ints);
	add_timer(&fps_ints.timer);
	wake_lock_init(&et320_wake_lock, WAKE_LOCK_SUSPEND, "et320_wake_lock");
	DEBUG_PRINT("  add_timer ---- \n");
	DEBUG_PRINT("%s : initialize success %d\n",
		__func__, status);

	status = sysfs_create_group(&dev->kobj, &attribute_group);
	if (status) {
		pr_err("%s could not create sysfs\n", __func__);
		goto etspi_probe_failed;
	}
	request_irq_done = 0;
	return status;

etspi_probe_failed:
	device_destroy(etspi_class, etspi->devt);
	class_destroy(etspi_class);

etspi_probe_platformInit_failed:
etspi_probe_parse_dt_failed:
	kfree(etspi);
	pr_err("%s is failed\n", __func__);
#if EGIS_NAVI_INPUT
	uinput_egis_destroy(etspi);
	sysfs_egis_destroy(etspi);
#endif

	return status;
}

static int __init et320_init(void)
{
	int status = 0;
	DEBUG_PRINT("%s  enter\n", __func__);
#if 0
	/* fp_id is used only when dual sensor need to be support  */
	struct device_node *fp_id_np = NULL;
	int fp_id_gpio = 0, fp_id_gpio_value;

	fp_id_np = of_find_matching_node(fp_id_np, fp_id_match_table);
	if (fp_id_np)
	    fp_id_gpio = of_get_named_gpio(fp_id_np, "fp,gpio-id", 0);

	if (fp_id_gpio < 0) {
		/* errorno = gpio; */
		DEBUG_PRINT("%s: device tree error \n", __func__);
		return status;
	} else {
		/* data->rstPin = gpio; */
		DEBUG_PRINT("%s: fp_id Pin=%d\n", __func__, fp_id_gpio);
	}
	gpio_direction_input(fp_id_gpio);
	fp_id_gpio_value = gpio_get_value(fp_id_gpio);
	if (fp_id_gpio_value == 0)
	    DEBUG_PRINT("%s:  Load et320-int driver \n", __func__);
	else {
		DEBUG_PRINT("%s:  Load fpc driver \n", __func__);
		return status;
    }
#endif
	status = platform_driver_register(&etspi_driver);
	DEBUG_PRINT("%s  done\n", __func__);

	return status;
}

static void __exit et320_exit(void)
{
	platform_driver_unregister(&etspi_driver);
}

module_init(et320_init);
module_exit(et320_exit);

MODULE_AUTHOR("Wang YuWei, <robert.wang@egistec.com>");
MODULE_DESCRIPTION("SPI Interface for ET320");
MODULE_LICENSE("GPL");
