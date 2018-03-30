/*
 * Copyright (C) 2006-2017 ILITEK TECHNOLOGY CORP.
 *
 * Description: ILITEK I2C touchscreen driver for linux platform.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Johnson Yeh
 * Maintain: Luca Hsu, Tigers Huang, Dicky Chiang
 */

/**
 *
 * @file    ilitek_drv_qcom.c
 *
 * @brief   This file defines the interface of touch screen
 *
 *
 */

/*=============================================================*/
/*INCLUDE FILE*/
/*=============================================================*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/kobject.h>
#include <linux/irq.h>
#include <linux/io.h>

#include "ilitek_drv_common.h"

/*=============================================================*/
/*CONSTANT VALUE DEFINITION*/
/*=============================================================*/

#define MSG_TP_IC_NAME "ILITEK "
/*=============================================================*/
/*VARIABLE DEFINITION*/
/*=============================================================*/

struct i2c_client *g_i2c_client = NULL;

#ifdef CONFIG_ENABLE_REGULATOR_POWER_ON
struct regulator *g_regu_vdd = NULL;
struct regulator *g_regu_vcc_i2c = NULL;
#endif /*CONFIG_ENABLE_REGULATOR_POWER_ON */

/*=============================================================*/
/*FUNCTION DEFINITION*/
/*=============================================================*/

/*probe function is used for matching and initializing input device */
static int /*__devinit*/ touch_driver_probe(struct i2c_client *client,
                                            const struct i2c_device_id *id)
{
#ifdef CONFIG_ENABLE_REGULATOR_POWER_ON
    int ret = 0;
    const char *vdd_name = "vdd";
    const char *vcc_i2c_name = "vcc_i2c";
#endif /*CONFIG_ENABLE_REGULATOR_POWER_ON */

    DBG_INFO(0, "*** %s ***\n", __func__);

    if (client == NULL) {
        DBG_INFO(0, "i2c client is NULL\n");
        return -1;
    }
    g_i2c_client = client;

#ifdef CONFIG_ENABLE_REGULATOR_POWER_ON
    g_regu_vdd = regulator_get(&g_i2c_client->dev, vdd_name);
    if (IS_ERR(g_regu_vdd)) {
        DBG_INFO(0, "regulator_get vdd fail\n");
        /*return -EINVAL; */
    } else {
        ret = regulator_set_voltage(g_regu_vdd, 2800000, 3300000);
        if (ret) {
            DBG_INFO(0, "Could not set to 2800mv.\n");
        }
    }
    g_regu_vcc_i2c = regulator_get(&g_i2c_client->dev, vcc_i2c_name);
    if (IS_ERR(g_regu_vcc_i2c)) {
        DBG_INFO(0, "regulator_get vcc fail\n");
        /*return -EINVAL; */
    } else {
        ret = regulator_set_voltage(g_regu_vcc_i2c, 1800000, 1800000);
        if (ret) {
            DBG_INFO(0, "Could not set to 1800mv.\n");
        }
    }
#endif /*CONFIG_ENABLE_REGULATOR_POWER_ON */

    return ms_drv_interface_touch_device_probe(g_i2c_client, id);
}

/* remove function is triggered when the input device is removed from input sub-system */
static int touch_driver_remove(struct i2c_client *client)
{
    DBG_INFO(0, "*** %s ***\n", __func__);

    return ms_drv_interface_touch_device_remove(client);
}

/* The I2C device list is used for matching I2C device and I2C device driver. */
static const struct i2c_device_id touch_device_id[] = {
    {MSG_TP_IC_NAME, 0},
    {},                         /* should not omitted */
};

MODULE_DEVICE_TABLE(i2c, touch_device_id);

static struct of_device_id touch_match_table[] = {
    {.compatible = "mstar,msg2856a",},
    {},
};

static struct i2c_driver touch_device_driver = {
    .driver = {
        .name = MSG_TP_IC_NAME,
        .owner = THIS_MODULE,
        .of_match_table = touch_match_table,
    },
    .probe = touch_driver_probe,
    .remove = touch_driver_remove,
    .id_table = touch_device_id,
};

static int __init touch_driver_init(void)
{
    int ret;

    /* register driver */
    ret = i2c_add_driver(&touch_device_driver);
    if (ret < 0) {
        DBG_INFO(0, "add ILITEK/MStar touch device driver i2c driver failed.\n");
        return -ENODEV;
    }
    DBG_INFO(0, "add ILITEK/MStar touch device driver i2c driver.\n");

    return ret;
}

static void __exit touch_driver_exit(void)
{
    DBG_INFO(0, "remove ILITEK/MStar touch device driver i2c driver.\n");

    i2c_del_driver(&touch_device_driver);
}

module_param(touch_driver_debug_log_level, byte, 0644);
device_initcall_sync(touch_driver_init);
module_exit(touch_driver_exit);
MODULE_LICENSE("GPL");
