/*
 *  shub-input_significant.c - Linux kernel modules for interface of ML630Q790
 *
 *  Copyright (C) 2014 LAPIS SEMICONDUCTOR CO., LTD.
 *
 *  This file is based on :
 *    alps-input.c - Linux kernel modules for interface of ML610Q792
 *    http://android-dev.kyocera.co.jp/source/versionSelect_URBANO.html
 *    (C) 2012 KYOCERA Corporation
 *    Copyright (C) 2010 ALPS
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <asm/uaccess.h> 

#include "shub_io.h"
#include "ml630q790.h"
#define INDEX_TM           0
#define INDEX_TMNS         1
#define INDEX_SUM          2

#define TOTAL_DETECT_ENABLE     1

#ifdef CONFIG_ANDROID_ENGINEERING
static int shub_signif_log = 0;
module_param(shub_signif_log, int, 0600);
#define DBG_SIGNIF_IO(msg, ...) {                      \
    if(shub_signif_log & 0x01)                         \
        printk("[shub][signif] " msg, ##__VA_ARGS__);  \
}
#define DBG_SIGNIF_DATA(msg, ...) {                    \
    if(shub_signif_log & 0x02)                         \
        printk("[shub][signif] " msg, ##__VA_ARGS__);  \
}
#else
#define DBG_SIGNIF_IO(msg, ...)
#define DBG_SIGNIF_DATA(msg, ...)
#endif

#define INPUT_DEV_NAME "shub_significant"
#define INPUT_DEV_PHYS "shub_significant/input0"
#define MISC_DEV_NAME  "shub_io_significant"

static DEFINE_MUTEX(shub_lock);

static struct platform_device *pdev;
static struct input_dev *shub_idev;

static int32_t shub_probe_signif(struct platform_device *pfdev);
static int32_t shub_remove_signif(struct platform_device *pfdev);

#ifdef CONFIG_OF
    static struct of_device_id shub_of_match_tb_signif[] = {
        { .compatible = "sharp,shub_signif" ,},
        {}
    };
#else
    #define shub_of_match_tb_signif NULL
#endif

static long shub_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    void __user *argp = (void __user *)arg;
    int32_t ret = -1, tmpval = 0;

    switch (cmd) {
        case SHUBIO_SIGNIFICANT_ACTIVATE:
            {
                ret = copy_from_user(&tmpval, argp, sizeof(tmpval));
                if (ret) {
                    printk("error : shub_ioctl(cmd = SHUBIO_SIGNIFICANT_ACTIVATE)\n");
                    return -EFAULT;
                }
                DBG_SIGNIF_IO("ioctl(cmd = Set_Active) : val=%d\n", tmpval);
                mutex_lock(&shub_lock);
                ret = shub_activate( SHUB_ACTIVE_SIGNIFICANT, tmpval);
                mutex_unlock(&shub_lock);
            }
            break;
        default:
            return -ENOTTY;
    }
    return 0;
}

static long shub_ioctl_wrapper(struct file *filp, unsigned int cmd, unsigned long arg)
{
    SHUB_DBG_TIME_INIT
    long ret = 0;

    shub_qos_start();
    SHUB_DBG_TIME_START
    ret = shub_ioctl(filp, cmd , arg);
    SHUB_DBG_TIME_END(cmd)
    shub_qos_end();

    return ret;
}

void shub_input_report_significant(int32_t *data)
{

    int32_t ret = 0;

    if(shub_vibe_notify_check(1)){
        mutex_lock(&shub_lock);
        ret = shub_activate( SHUB_ACTIVE_SIGNIFICANT, TOTAL_DETECT_ENABLE);
        mutex_unlock(&shub_lock);
        return;
    }

    DBG_SIGNIF_DATA("data t(s)=%d, t(ns)=%d\n", data[INDEX_TM],data[INDEX_TMNS]);
    shub_input_sync_init(shub_idev);
#if 1
    SHUB_INPUT_VAL_CLEAR(shub_idev, ABS_HAT3Y, data[INDEX_TM]);
    input_report_abs(shub_idev, ABS_HAT3Y, data[INDEX_TM]);
    input_report_abs(shub_idev, ABS_HAT3X, data[INDEX_TMNS]);
#else
    SHUB_INPUT_VAL_CLEAR(shub_idev, ABS_MISC, data[INDEX_TM]);
    input_report_abs(shub_idev, ABS_MISC, data[INDEX_TM]);
    input_report_abs(shub_idev, ABS_VOLUME, data[INDEX_TMNS]);
#endif
#if 1
    input_event(shub_idev, EV_SYN, SYN_REPORT, SHUB_INPUT_SIGNIFICANT);
#else
    input_event(shub_idev, EV_SYN, SYN_REPORT, 1);
#endif
}

#if 0
static void shub_set_abs_params(void)
{
    input_set_abs_params(shub_idev, ABS_MISC, 0, 0xFFFFFFFF, 0, 0);
    input_set_abs_params(shub_idev, ABS_VOLUME, 0, 0xFFFFFFFF, 0, 0);
    input_set_abs_params(shub_idev, ABS_X, 0, 1, 0, 0);
}
#endif

static struct file_operations shub_fops = {
    .owner   = THIS_MODULE,
    .unlocked_ioctl = shub_ioctl_wrapper,
#ifdef CONFIG_COMPAT
    .compat_ioctl   = shub_ioctl_wrapper,
#endif
};

static struct miscdevice shub_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = MISC_DEV_NAME,
    .fops  = &shub_fops,
};

static int32_t shub_probe_signif(struct platform_device *pfdev)
{
    int32_t ret = 0;


    if(!shub_connect_check()){
        printk(KERN_INFO "shub_significant Connect Error!!\n");
        ret = -ENODEV;
        goto out_driver;
    }


    pdev = platform_device_register_simple(INPUT_DEV_NAME, -1, NULL, 0);
    if (IS_ERR(pdev)) {
        ret = PTR_ERR(pdev);
        goto out_driver;
    }

#if 1
    shub_idev = shub_com_allocate_device(SHUB_INPUT_SIGNIFICANT, &pdev->dev);
    if (!shub_idev) {
        ret = -ENOMEM;
        goto out_device;
    }
#else
    shub_idev = input_allocate_device();
    if (!shub_idev) {
        ret = -ENOMEM;
        goto out_device;
    }

    shub_idev->name = INPUT_DEV_NAME;
    shub_idev->phys = INPUT_DEV_PHYS;
    shub_idev->id.bustype = BUS_HOST;
    shub_idev->dev.parent = &pdev->dev;
    shub_idev->evbit[0] = BIT_MASK(EV_ABS);

    shub_set_abs_params();

    ret = input_register_device(shub_idev);
    if (ret)
        goto out_idev;
#endif

    ret = misc_register(&shub_device);
    if (ret) {
        printk("shub-init: shub_io_device register failed\n");
        goto exit_misc_device_register_failed;
    }


    return 0;

exit_misc_device_register_failed:
#if 0
out_idev:
    input_free_device(shub_idev);
#endif
out_device:
    platform_device_unregister(pdev);
out_driver:
    return ret;
}

static int32_t shub_remove_signif(struct platform_device *pfdev)
{
    misc_deregister(&shub_device);
#if 1
    shub_com_unregister_device(SHUB_INPUT_SIGNIFICANT);
#else
    input_unregister_device(shub_idev);
    input_free_device(shub_idev);
#endif
    platform_device_unregister(pdev);
    return 0;
}

static struct platform_driver shub_signif_driver = {
    .probe = shub_probe_signif,
    .remove = shub_remove_signif,
    .shutdown = NULL,
    .driver = {
        .name = "shub_dev_signif",
        .of_match_table = shub_of_match_tb_signif,
    },
};

static int32_t __init shub_init(void)
{
    int ret;
    
    ret = platform_driver_register(&shub_signif_driver);

    return ret;
}

static void __exit shub_exit(void)
{
    platform_driver_unregister(&shub_signif_driver);
}

late_initcall(shub_init);
module_exit(shub_exit);

MODULE_DESCRIPTION("SensorHub Input Device (SignificantMotion)");
MODULE_AUTHOR("LAPIS SEMICOMDUCTOR");
MODULE_LICENSE("GPL v2");
