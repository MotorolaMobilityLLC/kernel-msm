/*
 * Copyright (C) 2010 NXP Semiconductors
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */
#ifndef _PN548_H_
#define _PN548_H_
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>

#define PN548_MAGIC         0xE9

#define PN548_DRV_NAME      "pn548"

/*
 * pn548 power control via ioctl
 * pn548_SET_PWR(0): power off
 * pn548_SET_PWR(1): power on
 * pn548_SET_PWR(2): reset and power on with firmware download enabled
 */
#define pn548_SET_PWR       _IOW(PN548_MAGIC, 0x01, unsigned int)

#define pn548_HW_REVISION   _IOR(PN548_MAGIC, 0x02, unsigned int)

struct pn548_i2c_platform_data {
    unsigned int            sda_gpio;
    unsigned int            scl_gpio;
    unsigned int            irq_gpio;
    unsigned int            ven_gpio;
    unsigned int            firm_gpio;
};

struct pn548_dev {
    wait_queue_head_t       read_wq;
    struct mutex            read_mutex;
    struct i2c_client       *client;
    struct miscdevice       pn548_device;
    unsigned int            ven_gpio;
    unsigned int            firm_gpio;
    unsigned int            irq_gpio;
    struct clk              *clk_cont;
    struct clk              *clk_pin;
    bool                    irq_enabled;
    spinlock_t              irq_enabled_lock;
};

struct pn548_gpio {
    unsigned int            sda_gpio;
    unsigned int            scl_gpio;
    unsigned int            ven_gpio;
    unsigned int            firm_gpio;
    unsigned int            irq_gpio;
};

#define dprintk(fmt, args...) printk(fmt, ##args)

#endif /* _PN548_H_ */
