/*
 * SAMSUNG NFC Controller
 *
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 * Author: Woonki Lee <woonki84.lee@samsung.com>
 *         Heejae Kim <heejae12.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation;  either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program
 *
 */
#ifdef CONFIG_SEC_NFC_IF_I2C_GPIO
#define CONFIG_SEC_NFC_IF_I2C
#endif

#include <linux/wait.h>
#include <linux/delay.h>

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/clk-provider.h>
#ifdef CONFIG_SEC_NFC_GPIO_CLK
#include <linux/interrupt.h>
#endif
#ifdef CONFIG_SEC_NFC_WAKE_LOCK
#include <linux/wakelock.h>
#endif
#include <linux/of_gpio.h>
#include <linux/clk.h>

#ifndef CONFIG_SEC_NFC_IF_I2C
struct sec_nfc_i2c_info {};
#define sec_nfc_read            NULL
#define sec_nfc_write           NULL
#define sec_nfc_poll            NULL
#define sec_nfc_i2c_irq_clear(x)

#define SEC_NFC_GET_INFO(dev) platform_get_drvdata(to_platform_device(dev))

#else /* CONFIG_SEC_NFC_IF_I2C */
#include <linux/interrupt.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/i2c.h>
#include "sec_nfc.h"

#define SEC_NFC_GET_INFO(dev) i2c_get_clientdata(to_i2c_client(dev))
enum sec_nfc_irq {
    SEC_NFC_SKIP = -1,
    SEC_NFC_NONE,
    SEC_NFC_INT,
    SEC_NFC_READ_TIMES,
};

struct sec_nfc_i2c_info {
    struct i2c_client *i2c_dev;
    struct mutex read_mutex;
    enum sec_nfc_irq read_irq;
    wait_queue_head_t read_wait;
    size_t buflen;
    u8 *buf;
};
#endif

struct sec_nfc_info {
    struct miscdevice miscdev;
    struct mutex mutex;
    enum sec_nfc_mode mode;
    struct device *dev;
    struct sec_nfc_platform_data *pdata;
    struct sec_nfc_i2c_info i2c_info;
#ifdef CONFIG_SEC_NFC_WAKE_LOCK
    struct wake_lock nfc_wake_lock;
#endif
#ifdef CONFIG_SEC_NFC_GPIO_CLK
    bool clk_ctl;
    bool clk_state;
#endif
#ifdef CONFIG_SEC_NFC_DEDICATED_CLK
    void __iomem *clkctrl;
#endif
};

#ifdef CONFIG_SEC_ESE_COLDRESET
struct mutex coldreset_mutex;
struct mutex sleep_wake_mutex;
bool sleep_wakeup_state[2];
u8 disable_combo_reset_cmd[4] = {0x2F, 0x30, 0x01, 0x00};
bool is_shutdown = false;
enum sec_nfc_mode cur_mode;
#endif

#ifdef CONFIG_SEC_NFC_IF_I2C
static irqreturn_t sec_nfc_irq_thread_fn(int irq, void *dev_id)
{
    struct sec_nfc_info *info = dev_id;
    struct sec_nfc_platform_data *pdata = info->pdata;

    dev_dbg(info->dev, "[NFC] Read Interrupt is occurred!\n");

    if (gpio_get_value(pdata->irq) == 0) {
        dev_err(info->dev, "[NFC] Warning,irq-gpio state is low!\n");
        return IRQ_HANDLED;
    }

    mutex_lock(&info->i2c_info.read_mutex);
    /* Skip interrupt during power switching
     * It is released after first write */
    if (info->i2c_info.read_irq == SEC_NFC_SKIP) {
        dev_dbg(info->dev, "%s: Now power swiching. Skip this IRQ\n", __func__);
        mutex_unlock(&info->i2c_info.read_mutex);
        return IRQ_HANDLED;
    }

    info->i2c_info.read_irq += SEC_NFC_READ_TIMES;
    mutex_unlock(&info->i2c_info.read_mutex);

    wake_up_interruptible(&info->i2c_info.read_wait);
#ifdef CONFIG_SEC_NFC_WAKE_LOCK
    wake_lock_timeout(&info->nfc_wake_lock, 2 * HZ);
#endif

    return IRQ_HANDLED;
}

static ssize_t sec_nfc_read(struct file *file, char __user *buf,
                            size_t count, loff_t *ppos)
{
    struct sec_nfc_info *info = container_of(file->private_data,
                                                struct sec_nfc_info, miscdev);
    enum sec_nfc_irq irq;
    int ret = 0;

    dev_dbg(info->dev, "%s: info: %p, count: %zu\n", __func__,
            info, count);

    mutex_lock(&info->mutex);
    if (info->mode == SEC_NFC_MODE_OFF) {
        dev_err(info->dev, "sec_nfc is not enabled\n");
        ret = -ENODEV;
        goto out;
    }

    mutex_lock(&info->i2c_info.read_mutex);
    if (count == 0) {
        if (info->i2c_info.read_irq >= SEC_NFC_INT)
            info->i2c_info.read_irq--;
        mutex_unlock(&info->i2c_info.read_mutex);
        goto out;
    }

    irq = info->i2c_info.read_irq;
    mutex_unlock(&info->i2c_info.read_mutex);
    if (irq == SEC_NFC_NONE) {
        if (file->f_flags & O_NONBLOCK) {
            dev_err(info->dev, "it is nonblock\n");
            ret = -EAGAIN;
            goto out;
        }
    }

    /* i2c recv */
    if (count > info->i2c_info.buflen)
        count = info->i2c_info.buflen;

    if (count > SEC_NFC_MSG_MAX_SIZE) {
        dev_err(info->dev, "user required wrong size :%d\n", (u32)count);
        ret = -EINVAL;
        goto out;
    }

    mutex_lock(&info->i2c_info.read_mutex);
    memset(info->i2c_info.buf, 0, count);
    ret = i2c_master_recv(info->i2c_info.i2c_dev, info->i2c_info.buf, (u32)count);
    dev_dbg(info->dev, "recv size : %d\n", ret);

    if (ret == -EREMOTEIO) {
        ret = -ERESTART;
        goto read_error;
    } else if (ret != count) {
        dev_err(info->dev, "read failed: return: %d count: %d\n",
                ret, (u32)count);
        /* ret = -EREMOTEIO; */
        goto read_error;
    }

    if (info->i2c_info.read_irq >= SEC_NFC_INT)
        info->i2c_info.read_irq--;

    if (info->i2c_info.read_irq == SEC_NFC_READ_TIMES)
        wake_up_interruptible(&info->i2c_info.read_wait);

    mutex_unlock(&info->i2c_info.read_mutex);

    if (copy_to_user(buf, info->i2c_info.buf, ret)) {
        dev_err(info->dev, "copy failed to user\n");
        ret = -EFAULT;
    }

    goto out;

read_error:
    info->i2c_info.read_irq = SEC_NFC_NONE;
    mutex_unlock(&info->i2c_info.read_mutex);
out:
    mutex_unlock(&info->mutex);

    return ret;
}

static ssize_t sec_nfc_write(struct file *file, const char __user *buf,
                                size_t count, loff_t *ppos)
{
    struct sec_nfc_info *info = container_of(file->private_data,
                                                struct sec_nfc_info, miscdev);
    int ret = 0;

    dev_dbg(info->dev, "%s: info: %p, count %d\n", __func__,
            info, (u32)count);

    mutex_lock(&info->mutex);

    if (info->mode == SEC_NFC_MODE_OFF) {
        dev_err(info->dev, "sec_nfc is not enabled\n");
        ret = -ENODEV;
        goto out;
    }

    if (count > info->i2c_info.buflen)
        count = info->i2c_info.buflen;

    if (count > SEC_NFC_MSG_MAX_SIZE) {
        dev_err(info->dev, "user required wrong size :%d\n", (u32)count);
        ret = -EINVAL;
        goto out;
    }

    if (copy_from_user(info->i2c_info.buf, buf, count)) {
        dev_err(info->dev, "copy failed from user\n");
        ret = -EFAULT;
        goto out;
    }

    /* Skip interrupt during power switching
     * It is released after first write
     */
    mutex_lock(&info->i2c_info.read_mutex);
    ret = i2c_master_send(info->i2c_info.i2c_dev, info->i2c_info.buf, count);
    if (info->i2c_info.read_irq == SEC_NFC_SKIP)
        info->i2c_info.read_irq = SEC_NFC_NONE;
    mutex_unlock(&info->i2c_info.read_mutex);

    if (ret == -EREMOTEIO) {
        dev_err(info->dev, "send failed: return: %d count: %d\n",
        ret, (u32)count);
        ret = -ERESTART;
        goto out;
    }

    if (ret != count) {
        dev_err(info->dev, "send failed: return: %d count: %d\n",
        ret, (u32)count);
        ret = -EREMOTEIO;
    }

out:
    mutex_unlock(&info->mutex);

    return ret;
}

static unsigned int sec_nfc_poll(struct file *file, poll_table *wait)
{
    struct sec_nfc_info *info = container_of(file->private_data,
                                                struct sec_nfc_info, miscdev);
    enum sec_nfc_irq irq;

    int ret = 0;

    dev_dbg(info->dev, "%s: info: %p\n", __func__, info);

    mutex_lock(&info->mutex);

    if (info->mode == SEC_NFC_MODE_OFF) {
        dev_err(info->dev, "sec_nfc is not enabled\n");
        ret = -ENODEV;
        goto out;
    }

    poll_wait(file, &info->i2c_info.read_wait, wait);

    mutex_lock(&info->i2c_info.read_mutex);
    irq = info->i2c_info.read_irq;
    if (irq == SEC_NFC_READ_TIMES)
        ret = (POLLIN | POLLRDNORM);
    mutex_unlock(&info->i2c_info.read_mutex);

out:
    mutex_unlock(&info->mutex);

    return ret;
}

void sec_nfc_i2c_irq_clear(struct sec_nfc_info *info)
{
    /* clear interrupt. Interrupt will be occurred at power off */
    mutex_lock(&info->i2c_info.read_mutex);
    info->i2c_info.read_irq = SEC_NFC_NONE;
    mutex_unlock(&info->i2c_info.read_mutex);
}

int sec_nfc_i2c_probe(struct i2c_client *client)
{
    struct device *dev = &client->dev;
    struct sec_nfc_info *info = dev_get_drvdata(dev);
    struct sec_nfc_platform_data *pdata = info->pdata;
    int ret;

    pr_info("%s: start: %p\n", __func__, info);

    info->i2c_info.buflen = SEC_NFC_MAX_BUFFER_SIZE;
    info->i2c_info.buf = kzalloc(SEC_NFC_MAX_BUFFER_SIZE, GFP_KERNEL);
    if (!info->i2c_info.buf) {
        dev_err(dev, "failed to allocate memory for sec_nfc_info->buf\n");
        return -ENOMEM;
    }
    info->i2c_info.i2c_dev = client;
    info->i2c_info.read_irq = SEC_NFC_NONE;
    mutex_init(&info->i2c_info.read_mutex);
    init_waitqueue_head(&info->i2c_info.read_wait);
    i2c_set_clientdata(client, info);

    info->dev = dev;

    ret = gpio_request(pdata->irq, "nfc_int");
    if (ret) {
        pr_err("GPIO request is failed to register IRQ\n");
        goto err_irq_req;
    }
    client->irq = gpio_to_irq(pdata->irq);
    pr_info("%s: push interrupt no = %d\n", __func__, client->irq);

    ret = request_threaded_irq(client->irq, NULL, sec_nfc_irq_thread_fn,
                                IRQF_TRIGGER_RISING | IRQF_ONESHOT, SEC_NFC_DRIVER_NAME,
                                info);
    if (ret < 0) {
        pr_err("failed to register IRQ handler\n");
        kfree(info->i2c_info.buf);
        return ret;
    }

    pr_info("%s success\n", __func__);
    return 0;

err_irq_req:
    return ret;
}

void sec_nfc_i2c_remove(struct device *dev)
{
    struct sec_nfc_info *info = dev_get_drvdata(dev);
    struct i2c_client *client = info->i2c_info.i2c_dev;
    struct sec_nfc_platform_data *pdata = info->pdata;
    free_irq(client->irq, info);
    gpio_free(pdata->irq);
}
#endif /* CONFIG_SEC_NFC_IF_I2C */

#ifdef CONFIG_SEC_NFC_GPIO_CLK
static irqreturn_t sec_nfc_clk_irq_thread(int irq, void *dev_id)
{
    struct sec_nfc_info *info = dev_id;
    struct sec_nfc_platform_data *pdata = info->pdata;
    bool value;

    dev_dbg(info->dev, "[NFC]Clock Interrupt is occurred!\n");
    value = gpio_get_value(pdata->clk_req) > 0 ? true : false;

    if (value == info->clk_state)
        return IRQ_HANDLED;

    if (value)
        clk_prepare_enable(pdata->clk);
    else
        clk_disable_unprepare(pdata->clk);

    info->clk_state = value;

    return IRQ_HANDLED;
}

void sec_nfc_clk_ctl_enable(struct sec_nfc_info *info)
{
    struct sec_nfc_platform_data *pdata = info->pdata;
    unsigned int irq;
    int ret;

    irq = gpio_to_irq(pdata->clk_req);

    if (info->clk_ctl)
        return;

    info->clk_state = false;
    ret = request_threaded_irq(irq, NULL, sec_nfc_clk_irq_thread,
                                IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
                                SEC_NFC_DRIVER_NAME, info);
    if (ret < 0)
        dev_err(info->dev, "failed to register CLK REQ IRQ handler\n");

    info->clk_ctl = true;
}

void sec_nfc_clk_ctl_disable(struct sec_nfc_info *info)
{
    struct sec_nfc_platform_data *pdata = info->pdata;
    unsigned int irq;

    if (pdata->clk_req)
        irq = gpio_to_irq(pdata->clk_req);
    else
        return;

    if (!info->clk_ctl)
        return;

    free_irq(irq, info);
    if (info->clk_state)
        clk_disable_unprepare(pdata->clk);
    info->clk_state = false;
    info->clk_ctl = false;
}
#else
#define sec_nfc_clk_ctl_enable(x)
#define sec_nfc_clk_ctl_disable(x)
#endif /* CONFIG_SEC_NFC_GPIO_CLK */

static void sec_nfc_set_mode(struct sec_nfc_info *info,
                                enum sec_nfc_mode mode)
{
    struct sec_nfc_platform_data *pdata = info->pdata;
#ifdef CONFIG_SEC_ESE_COLDRESET
    int alreadFirmHigh = 0;
    int ret ;
    enum sec_nfc_mode oldmode = info->mode;
#endif
#ifdef CONFIG_SEC_NFC_DEDICATED_CLK
    unsigned int val = readl(info->clkctrl);
#endif

    /* intfo lock is aleady gotten before calling this function */
    if (info->mode == mode) {
        pr_err("Power mode is already %d", mode);
        return;
    }
    info->mode = mode;

#ifdef CONFIG_SEC_NFC_IF_I2C
    /* Skip interrupt during power switching
     * It is released after first write */
    mutex_lock(&info->i2c_info.read_mutex);
#ifdef CONFIG_SEC_ESE_COLDRESET
    cur_mode = mode;
    memset(sleep_wakeup_state, false, sizeof(sleep_wakeup_state));
    if (oldmode == SEC_NFC_MODE_OFF) {
        if (gpio_get_value(pdata->firm) == 1) {
            alreadFirmHigh = 1;
            pr_err("Firm is already high; do not anything");
        } else { /*Firm pin is low*/
            gpio_set_value(pdata->firm, SEC_NFC_FW_ON);
            msleep(SEC_NFC_VEN_WAIT_TIME);
        }

        if (gpio_get_value(pdata->ven) == SEC_NFC_PW_ON) {
            ret = i2c_master_send(info->i2c_info.i2c_dev, disable_combo_reset_cmd,
                                    sizeof(disable_combo_reset_cmd)/sizeof(u8));
            pr_err("disable combo_reset_command ret: %d", ret);
        } else {
            pr_err("skip disable combo_reset_command");
        }

        if (alreadFirmHigh == 1) {
            pr_err("Firm is already high; do not anything2");
        } else { /*Firm pin is low*/
            mdelay(3); /*wait for FW*/
            gpio_set_value(pdata->firm, SEC_NFC_FW_OFF);
        }
    }
#endif
    info->i2c_info.read_irq = SEC_NFC_SKIP;
    mutex_unlock(&info->i2c_info.read_mutex);
#endif

#ifdef CONFIG_SEC_ESE_COLDRESET
    mdelay(1); 
    pr_err("FIRMWARE_GUARD_TIME(+1ms) in PW_OFF(total:4ms)\n");
#endif

    gpio_set_value(pdata->ven, SEC_NFC_PW_OFF);
    if (pdata->firm)
        gpio_set_value(pdata->firm, SEC_NFC_FW_OFF);

    if (mode == SEC_NFC_MODE_BOOTLOADER)
        if (pdata->firm)
            gpio_set_value(pdata->firm, SEC_NFC_FW_ON);

    if (mode != SEC_NFC_MODE_OFF) {
        msleep(SEC_NFC_VEN_WAIT_TIME);
        sec_nfc_clk_ctl_enable(info);
        gpio_set_value(pdata->ven, SEC_NFC_PW_ON);
#ifdef CONFIG_SEC_NFC_DEDICATED_CLK
        val |= 0xC0000000;
        writel(val, info->clkctrl);
#endif
#ifdef CONFIG_SEC_NFC_IF_I2C
        enable_irq_wake(info->i2c_info.i2c_dev->irq);
#endif
        msleep(SEC_NFC_VEN_WAIT_TIME/2);
    } else {
#ifdef CONFIG_SEC_ESE_COLDRESET
        int PW_OFF_DURATION = 20;
        struct timespec t0, t1;
        getnstimeofday(&t0);
        mdelay(PW_OFF_DURATION);

        if (is_shutdown == false)
            gpio_set_value(pdata->ven, SEC_NFC_PW_ON);

        getnstimeofday(&t1);
        pr_err("DeepStby: PW_OFF duration (%d)ms, real PW_OFF duration is (%ld-%ld)ms\n",
                    PW_OFF_DURATION ,t0.tv_nsec*1000, t1.tv_nsec*1000);
        pr_err("DeepStby: enter DeepStby(PW_ON)\n");
#endif
        sec_nfc_clk_ctl_disable(info);
#ifdef CONFIG_SEC_NFC_DEDICATED_CLK
        val &= ~0xC0000000;
        writel(val, info->clkctrl);
#endif
#ifdef CONFIG_SEC_NFC_IF_I2C
        disable_irq_wake(info->i2c_info.i2c_dev->irq);
#endif
    }

#ifdef CONFIG_SEC_NFC_WAKE_LOCK
    if (wake_lock_active(&info->nfc_wake_lock))
        wake_unlock(&info->nfc_wake_lock);
#endif

    pr_info("%s NFC mode is : %d\n", __func__, mode);
}

#ifdef CONFIG_SEC_ESE_COLDRESET
struct cold_reset_gpio {
    int firm_gpio;
    int coldreset_gpio;
};

struct cold_reset_gpio cold_reset_gpio_data;

void init_coldreset_mutex(void)
{
    mutex_init(&coldreset_mutex);
}

void init_sleep_wake_mutex(void)
{
    mutex_init(&sleep_wake_mutex);
}

void check_and_sleep_nfc(unsigned int gpio, int value) {
    if (sleep_wakeup_state[IDX_SLEEP_WAKEUP_NFC] == true ||
            sleep_wakeup_state[IDX_SLEEP_WAKEUP_ESE] == true) {
        pr_info("%s keep wake up state\n", __func__);
        return;
    }
    gpio_set_value(gpio, value);
}

int trig_cold_reset_id(int id)
{
    int wakeup_delay = 20;
    int duration = 18;
    struct timespec t0, t1, t2;
    int isFirmHigh = 0;

    if (id == ESE_ID)
        mutex_lock(&coldreset_mutex);

    getnstimeofday(&t0);
    if (gpio_get_value(cold_reset_gpio_data.firm_gpio) == 1) {
        isFirmHigh = 1;
    } else {
        gpio_set_value(cold_reset_gpio_data.firm_gpio, SEC_NFC_FW_ON);
        mdelay(wakeup_delay);
    }

    getnstimeofday(&t1);
    gpio_set_value(cold_reset_gpio_data.coldreset_gpio, SEC_NFC_COLDRESET_ON);
    mdelay(duration);
    gpio_set_value(cold_reset_gpio_data.coldreset_gpio, SEC_NFC_COLDRESET_OFF);
    getnstimeofday(&t2);

    if (isFirmHigh == 1)
        pr_err("COLDRESET: FW_PIN already high, do not FW_OFF\n");
    else
        gpio_set_value(cold_reset_gpio_data.firm_gpio, SEC_NFC_FW_OFF);

    pr_err("COLDRESET: FW_ON time (%ld-%ld)\n", t0.tv_nsec*1000, t1.tv_nsec*1000); 
    pr_err("COLDRESET: GPIO3 ON time (%ld-%ld)\n", t1.tv_nsec*1000, t2.tv_nsec*1000);

    if (id == ESE_ID)
        mutex_unlock(&coldreset_mutex);

    pr_err("COLDRESET: exit");
    return 0;
}

extern int trig_cold_reset(void) // Called from eSE driver
{
    pr_info("%s trig_cold_reset\n", __func__);
    return trig_cold_reset_id(ESE_ID);
}
EXPORT_SYMBOL(trig_cold_reset);

extern int trig_nfc_wakeup(void)
{
    pr_info("%s trig_nfc_wakeup\n", __func__);

    if (cur_mode == SEC_NFC_MODE_OFF || cur_mode == SEC_NFC_MODE_BOOTLOADER) {
        pr_err("nfc mode not support to wake up");
        return -EPERM;;
    }
    mutex_lock(&sleep_wake_mutex);
    gpio_set_value(cold_reset_gpio_data.firm_gpio, SEC_NFC_WAKE_UP);
    sleep_wakeup_state[IDX_SLEEP_WAKEUP_ESE] = true;
    mutex_unlock(&sleep_wake_mutex);
    return 0;
}
EXPORT_SYMBOL(trig_nfc_wakeup);

extern int trig_nfc_sleep(void)
{
    pr_info("%s trig_nfc_sleep\n", __func__);
    if (cur_mode == SEC_NFC_MODE_OFF || cur_mode == SEC_NFC_MODE_BOOTLOADER) {
        pr_err("nfc mode not support to sleep");
        return -EPERM;;
    }
    mutex_lock(&sleep_wake_mutex);
    sleep_wakeup_state[IDX_SLEEP_WAKEUP_ESE] = false;
    check_and_sleep_nfc(cold_reset_gpio_data.firm_gpio, SEC_NFC_WAKE_SLEEP);
    mutex_unlock(&sleep_wake_mutex);
    return 0;
}
EXPORT_SYMBOL(trig_nfc_sleep);
#endif

static long sec_nfc_ioctl(struct file *file, unsigned int cmd,
                            unsigned long arg)
{
    struct sec_nfc_info *info = container_of(file->private_data,
                                                struct sec_nfc_info, miscdev);
    struct sec_nfc_platform_data *pdata = info->pdata;
    unsigned int new = (unsigned int)arg;
    int ret = 0;

    dev_dbg(info->dev, "%s: info: %p, cmd: 0x%x\n",
            __func__, info, cmd);

    mutex_lock(&info->mutex);
#ifdef CONFIG_SEC_ESE_COLDRESET
    mutex_lock(&coldreset_mutex);
#endif

    switch (cmd) {
    case SEC_NFC_SET_MODE:
        if (info->mode == new)
            break;

        if (new >= SEC_NFC_MODE_COUNT) {
            dev_err(info->dev, "wrong mode (%d)\n", new);
            ret = -EFAULT;
            break;
        }
        sec_nfc_set_mode(info, new);
        break;

#if defined(CONFIG_SEC_NFC_PRODUCT_N3)
    case SEC_NFC_SLEEP:
    case SEC_NFC_WAKEUP:
        break;

#elif defined(CONFIG_SEC_NFC_PRODUCT_N5) || defined(CONFIG_SEC_NFC_PRODUCT_N7)
    case SEC_NFC_SLEEP:
        if (info->mode != SEC_NFC_MODE_BOOTLOADER) {
#ifdef CONFIG_SEC_NFC_WAKE_LOCK
            if (wake_lock_active(&info->nfc_wake_lock))
                wake_unlock(&info->nfc_wake_lock);
#endif
#ifdef CONFIG_SEC_ESE_COLDRESET
            mutex_lock(&sleep_wake_mutex);
            sleep_wakeup_state[IDX_SLEEP_WAKEUP_NFC] = false;
            check_and_sleep_nfc(pdata->wake, SEC_NFC_WAKE_SLEEP);
            mutex_unlock(&sleep_wake_mutex);
#else
            gpio_set_value(pdata->wake, SEC_NFC_WAKE_SLEEP);
#endif
        }
        break;

    case SEC_NFC_WAKEUP:
        if (info->mode != SEC_NFC_MODE_BOOTLOADER) {
            gpio_set_value(pdata->wake, SEC_NFC_WAKE_UP);
#ifdef CONFIG_SEC_ESE_COLDRESET
            sleep_wakeup_state[IDX_SLEEP_WAKEUP_NFC] = true;
#endif
#ifdef CONFIG_SEC_NFC_WAKE_LOCK
            if (!wake_lock_active(&info->nfc_wake_lock))
                wake_lock(&info->nfc_wake_lock);
#endif
        }
        break;
#endif
#ifdef CONFIG_SEC_ESE_COLDRESET
    case SEC_NFC_COLD_RESET:
        trig_cold_reset_id(DEVICEHOST_ID);
        break;
    case SEC_NFC_SHUTDOWN:
        is_shutdown = true;
        break;
#endif
    default:
        dev_err(info->dev, "Unknown ioctl 0x%x\n", cmd);
        ret = -ENOIOCTLCMD;
        break;
    }

#ifdef CONFIG_SEC_ESE_COLDRESET
    mutex_unlock(&coldreset_mutex);
#endif
    mutex_unlock(&info->mutex);

    return ret;
}

static int sec_nfc_open(struct inode *inode, struct file *file)
{
    struct sec_nfc_info *info = container_of(file->private_data,
                                                struct sec_nfc_info, miscdev);
    int ret = 0;

    pr_info("%s info : %p\n", __func__, info);

    mutex_lock(&info->mutex);
    if (info->mode != SEC_NFC_MODE_OFF) {
        dev_err(info->dev, "sec_nfc is busy\n");
        ret = -EBUSY;
        goto out;
    }

    sec_nfc_set_mode(info, SEC_NFC_MODE_OFF);

out:
    mutex_unlock(&info->mutex);
    return ret;
}

static int sec_nfc_close(struct inode *inode, struct file *file)
{
    struct sec_nfc_info *info = container_of(file->private_data,
                                                struct sec_nfc_info, miscdev);

    pr_info("%s: info : %p\n", __func__, info);

    mutex_lock(&info->mutex);
    sec_nfc_set_mode(info, SEC_NFC_MODE_OFF);
    mutex_unlock(&info->mutex);

    return 0;
}

static const struct file_operations sec_nfc_fops = {
    .owner      = THIS_MODULE,
    .read       = sec_nfc_read,
    .write      = sec_nfc_write,
    .poll       = sec_nfc_poll,
    .open       = sec_nfc_open,
    .release    = sec_nfc_close,
    .unlocked_ioctl = sec_nfc_ioctl,
};

#ifdef CONFIG_PM
static int sec_nfc_suspend(struct device *dev)
{
    struct sec_nfc_info *info = SEC_NFC_GET_INFO(dev);
    int ret = 0;

    mutex_lock(&info->mutex);

    if (info->mode == SEC_NFC_MODE_BOOTLOADER)
        ret = -EPERM;

    mutex_unlock(&info->mutex);

    return ret;
}

static int sec_nfc_resume(struct device *dev)
{
    return 0;
}

static SIMPLE_DEV_PM_OPS(sec_nfc_pm_ops, sec_nfc_suspend, sec_nfc_resume);
#endif

#ifdef CONFIG_OF
/*device tree parsing*/
static int sec_nfc_parse_dt(struct device *dev,
                            struct sec_nfc_platform_data *pdata)
{
    struct device_node *np = dev->of_node;

    pdata->ven = of_get_named_gpio(np, "sec-nfc,ven-gpio", 0);
    pdata->firm = of_get_named_gpio(np, "sec-nfc,firm-gpio", 0);
    pdata->wake = pdata->firm;
#ifdef CONFIG_SEC_NFC_IF_I2C
    pdata->irq = of_get_named_gpio(np, "sec-nfc,irq-gpio", 0);
#endif

#ifdef CONFIG_SEC_ESE_COLDRESET
    pdata->coldreset = of_get_named_gpio(np, "sec-nfc,coldreset-gpio",0);
    cold_reset_gpio_data.firm_gpio = pdata->firm;
    cold_reset_gpio_data.coldreset_gpio = pdata->coldreset;
#endif

#ifdef CONFIG_SEC_NFC_LDO_EN
    pdata->pvdd_en = of_get_named_gpio(np, "sec-nfc,ldo_en",0);
#endif
#ifdef CONFIG_SEC_NFC_PMIC_LDO
    if (of_property_read_string(np, "sec-nfc,pmic-ldo", &pdata->pvdd_regulator_name))
        pr_err("%s: Failed to get %s regulator.\n", __func__, pdata->pvdd_regulator_name);
#endif

#ifdef CONFIG_SEC_NFC_DEDICATED_CLK
    if(of_property_read_u32(np, "clkctrl-reg", (u32 *)&pdata->clkctrl_addr))
        return -EINVAL;
#endif
#ifdef CONFIG_SEC_NFC_GPIO_CLK
    pdata->clk_req = of_get_named_gpio(np, "sec-nfc,clk_req-gpio", 0);
    pdata->clk = clk_get(dev, "OSC_NFC");
#endif

    pr_info("%s: irq : %d, ven : %d, firm : %d\n",
            __func__, pdata->irq, pdata->ven, pdata->firm);
    return 0;
}
#else /* CONFIG_OF */
static int sec_nfc_parse_dt(struct device *dev,
                            struct sec_nfc_platform_data *pdata)
{
    return -ENODEV;
}
#endif

static int __sec_nfc_probe(struct device *dev)
{
    struct sec_nfc_info *info;
    struct sec_nfc_platform_data *pdata = NULL;
    int ret = 0;

    pr_info("%s : start\n", __func__);
    if (dev->of_node) {
        pdata = devm_kzalloc(dev, sizeof(struct sec_nfc_platform_data), GFP_KERNEL);
        if (!pdata)
            return -ENOMEM;

        ret = sec_nfc_parse_dt(dev, pdata);
        if (ret)
            return ret;
    } else {
        pdata = dev->platform_data;
    }

    if (!pdata) {
        dev_err(dev, "No platform data\n");
        ret = -ENOMEM;
        goto err_pdata;
    }

    info = kzalloc(sizeof(struct sec_nfc_info), GFP_KERNEL);
    if (!info) {
        ret = -ENOMEM;
        goto err_info_alloc;
    }
    info->dev = dev;
    info->pdata = pdata;
    info->mode = SEC_NFC_MODE_OFF;

    mutex_init(&info->mutex);
    dev_set_drvdata(dev, info);

    info->miscdev.minor = MISC_DYNAMIC_MINOR;
    info->miscdev.name = SEC_NFC_DRIVER_NAME;
    info->miscdev.fops = &sec_nfc_fops;
    info->miscdev.parent = dev;
    ret = misc_register(&info->miscdev);
    if (ret < 0) {
        dev_err(dev, "failed to register Device\n");
        goto err_dev_reg;
    }

#ifdef CONFIG_SEC_NFC_DEDICATED_CLK
    if (pdata->clkctrl_addr != 0) {
        info->clkctrl = ioremap_nocache(pdata->clkctrl_addr, 0x80);
        if (!info->clkctrl) {
            dev_err(dev, "cannot remap nfc clock register\n");
            ret = -ENXIO;
            goto err_iomap;
        }
    }
#endif
#ifdef CONFIG_SEC_NFC_LDO_EN
    {
        ret = gpio_request(pdata->pvdd_en, "ldo_en");
        if (ret < 0) {
            pr_err("failed to request about pvdd_en pin\n");
            goto err_dev_reg;
        }
        if (!is_charging_mode)
            gpio_direction_output(pdata->pvdd_en, NFC_I2C_LDO_ON);
        pr_info("pvdd en: %d\n", gpio_get_value(pdata->pvdd_en));
    }
#endif
#ifdef CONFIG_SEC_NFC_PMIC_LDO
    if (!is_charging_mode) {
        pdata->pvdd_regulator = regulator_get(NULL, pdata->pvdd_regulator_name);
        if (IS_ERR(pdata->pvdd_regulator))
            pr_err("%s: Failed to get %s pmic regulator.\n", __func__, pdata->pvdd_regulator);
        ret = regulator_enable(pdata->pvdd_regulator);
        pr_err("%s: Failed to enable pmic regulator: %d\n", __func__, ret);
    }
#endif

    ret = gpio_request(pdata->ven, "nfc_ven");
    if (ret) {
        dev_err(dev, "failed to get gpio ven\n");
        goto err_gpio_ven;
    }
    if (!is_charging_mode)
        gpio_direction_output(pdata->ven, SEC_NFC_PW_OFF);

    if (pdata->firm) {
        ret = gpio_request(pdata->firm, "nfc_firm");
        if (ret) {
            dev_err(dev, "failed to get gpio firm\n");
            goto err_gpio_firm;
        }
        gpio_direction_output(pdata->firm, SEC_NFC_FW_OFF);
    }
#ifdef CONFIG_SEC_ESE_COLDRESET
    init_coldreset_mutex();
    init_sleep_wake_mutex();
    memset(sleep_wakeup_state, false, sizeof(sleep_wakeup_state));
    ret = gpio_request(pdata->coldreset, "nfc_coldreset");
    if (ret) {
        dev_err(dev, "failed to get gpio coldreset(NFC-GPIO3)\n");
        goto err_gpio_coldreset;
    }
    gpio_direction_output(pdata->coldreset, SEC_NFC_COLDRESET_OFF);
#endif
#ifdef CONFIG_SEC_NFC_WAKE_LOCK
    wake_lock_init(&info->nfc_wake_lock, WAKE_LOCK_SUSPEND, "nfc_wake_lock");
#endif

    dev_info(dev, "%s: success info: %p, pdata %p\n", __func__, info, pdata);

    return 0;

#ifdef CONFIG_SEC_ESE_COLDRESET
err_gpio_coldreset:
    gpio_free(pdata->coldreset);
#endif
err_gpio_firm:
    gpio_free(pdata->ven);
err_gpio_ven:
#ifdef CONFIG_SEC_NFC_DEDICATED_CLK
    iounmap(info->clkctrl);
err_iomap:
#endif
err_dev_reg:
    mutex_destroy(&info->mutex);
    kfree(info);
err_info_alloc:
    devm_kfree(dev, pdata);
err_pdata:
    return ret;
}

static int __sec_nfc_remove(struct device *dev)
{
    struct sec_nfc_info *info = dev_get_drvdata(dev);
    struct i2c_client *client = info->i2c_info.i2c_dev;
    struct sec_nfc_platform_data *pdata = info->pdata;

    dev_dbg(info->dev, "%s\n", __func__);

#ifdef CONFIG_SEC_NFC_GPIO_CLK
    if (pdata->clk)
        clk_unprepare(pdata->clk);
#endif

    misc_deregister(&info->miscdev);
    sec_nfc_set_mode(info, SEC_NFC_MODE_OFF);
    free_irq(client->irq, info);
    gpio_free(pdata->irq);
    gpio_set_value(pdata->firm, 0);
    gpio_free(pdata->ven);
    if (pdata->firm)
        gpio_free(pdata->firm);

#ifdef CONFIG_SEC_NFC_WAKE_LOCK
    wake_lock_destroy(&info->nfc_wake_lock);
#endif

    kfree(info);

    return 0;
}

#ifdef CONFIG_SEC_NFC_IF_I2C
static struct i2c_device_id sec_nfc_id_table[] = {
    { SEC_NFC_DRIVER_NAME, 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, sec_nfc_id_table);
typedef struct i2c_driver sec_nfc_driver_type;
#define SEC_NFC_INIT(driver)    i2c_add_driver(driver)
#define SEC_NFC_EXIT(driver)    i2c_del_driver(driver)

static int sec_nfc_probe(struct i2c_client *client,
                            const struct i2c_device_id *id)
{
    int ret = 0;

    ret = __sec_nfc_probe(&client->dev);
    if (ret)
        return ret;

    if (sec_nfc_i2c_probe(client))
        __sec_nfc_remove(&client->dev);

    return ret;
}

static int sec_nfc_remove(struct i2c_client *client)
{
    sec_nfc_i2c_remove(&client->dev);
    return __sec_nfc_remove(&client->dev);
}
#else /* CONFIG_SEC_NFC_IF_I2C */
MODULE_DEVICE_TABLE(platform, sec_nfc_id_table);
typedef struct platform_driver sec_nfc_driver_type;
#define SEC_NFC_INIT(driver)    platform_driver_register(driver);
#define SEC_NFC_EXIT(driver)    platform_driver_unregister(driver);

static int sec_nfc_probe(struct platform_device *pdev)
{
    return __sec_nfc_probe(&pdev->dev);
}

static int sec_nfc_remove(struct platform_device *pdev)
{
    return __sec_nfc_remove(&pdev->dev);
}

static struct platform_device_id sec_nfc_id_table[] = {
    { SEC_NFC_DRIVER_NAME, 0 },
    { }
};
#endif /* CONFIG_SEC_NFC_IF_I2C */

#ifdef CONFIG_OF
static const struct of_device_id nfc_match_table[] = {
    { .compatible = SEC_NFC_DRIVER_NAME,},
    {},
};
#else /* CONFIG_OF */
#define nfc_match_table NULL
#endif

static struct i2c_driver sec_nfc_driver = {
    .probe = sec_nfc_probe,
    .id_table = sec_nfc_id_table,
    .remove = sec_nfc_remove,
    .driver = {
        .name = SEC_NFC_DRIVER_NAME,
#ifdef CONFIG_PM
        .pm = &sec_nfc_pm_ops,
#endif
        .of_match_table = nfc_match_table,
    },
};

#if 0
static int __init is_poweroff_charging_mode(char *str)
{
    if (strncmp("charger", str, 7) == 0)
        is_charging_mode = true;
    else
        is_charging_mode = false;

    return 0;
} early_param("androidboot.mode", is_poweroff_charging_mode);
#endif

static int __init sec_nfc_init(void)
{
    return SEC_NFC_INIT(&sec_nfc_driver);
}

static void __exit sec_nfc_exit(void)
{
    SEC_NFC_EXIT(&sec_nfc_driver);
}

module_init(sec_nfc_init);
module_exit(sec_nfc_exit);

MODULE_DESCRIPTION("Samsung sec_nfc driver");
MODULE_LICENSE("GPL");
