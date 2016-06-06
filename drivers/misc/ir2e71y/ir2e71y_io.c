/* drivers/misc/ir2e71y/ir2e71y_io.c  (Display Driver)
 *
 * Copyright (C) 2016 SHARP CORPORATION
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/* ------------------------------------------------------------------------- */
/* INCLUDE FILES                                                             */
/* ------------------------------------------------------------------------- */
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/mm.h>
#include <linux/idr.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/ioctl.h>
#include <linux/gpio.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/hrtimer.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/spi/spi.h>
#include <asm/io.h>
#include <linux/clk.h>
#include <linux/qpnp/qpnp-adc.h>
#include "ir2e71y_define.h"
#include <linux/ir2e71y.h>
#include "ir2e71y_io.h"
#include "ir2e71y_dbg.h"


/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */

#define IR2E71Y_I2C_SPEED_KHZ (400)
#define IR2E71Y_I2C_RETRY (10)

#define IR2E71Y_INT_FLAGS (IRQF_TRIGGER_LOW | IRQF_DISABLED)

/* ------------------------------------------------------------------------- */
/* PROTOTYPES                                                                */
/* ------------------------------------------------------------------------- */
static int ir2e71y_host_gpio_request(int num, char *label);
static int ir2e71y_host_gpio_free(int num);
static int ir2e71y_IO_set_Host_gpio(int num, int value);

static int  ir2e71y_bdic_i2c_probe(struct i2c_client *client, const struct i2c_device_id * devid);
static int  ir2e71y_bdic_i2c_remove(struct i2c_client *client);

static int  ir2e71y_sensor_i2c_probe(struct i2c_client *client, const struct i2c_device_id * devid);
static int  ir2e71y_sensor_i2c_remove(struct i2c_client *client);


/* ------------------------------------------------------------------------- */
/* TYPES                                                                     */
/* ------------------------------------------------------------------------- */
typedef struct bdic_data_tag
{
    struct i2c_client *this_client;
} bdic_i2c_data_t;

typedef struct sensor_data_tag {
    struct i2c_client *this_client;
} sensor_data_t;

#ifdef CONFIG_OF
static const struct of_device_id ir2e71y_system_bdic_dt_match[] = {
    { .compatible = IR2E71Y_BDIC_I2C_DEVNAME, },
    {}
};
#endif /* CONFIG_OF */

static const struct i2c_device_id ir2e71y_bdic_id[] = {
    { IR2E71Y_BDIC_I2C_DEVNAME, 0 },
    { }
};


#ifdef CONFIG_OF
static const struct of_device_id ir2e71y_system_sensor_dt_match[] = {
    { .compatible = IR2E71Y_SENSOR_DEVNAME, },
    {}
};
#endif /* CONFIG_OF */

static const struct i2c_device_id ir2e71y_sensor_id[] = {
    { IR2E71Y_SENSOR_DEVNAME, 0 },
    { }
};

static struct i2c_driver bdic_driver =
{
    .driver = {
        .owner   = THIS_MODULE,
        .name    = IR2E71Y_BDIC_I2C_DEVNAME,
#ifdef CONFIG_OF
        .of_match_table = ir2e71y_system_bdic_dt_match,
#endif /* CONFIG_OF */
    },
    .class    = I2C_CLASS_HWMON,
    .probe    = ir2e71y_bdic_i2c_probe,
    .id_table = ir2e71y_bdic_id,
    .remove   = ir2e71y_bdic_i2c_remove,
};

static struct i2c_driver sensor_driver =
{
    .driver = {
        .owner   = THIS_MODULE,
        .name    = IR2E71Y_SENSOR_DEVNAME,
#ifdef CONFIG_OF
        .of_match_table = ir2e71y_system_sensor_dt_match,
#endif /* CONFIG_OF */
    },
    .class    = I2C_CLASS_HWMON,
    .probe    = ir2e71y_sensor_i2c_probe,
    .id_table = ir2e71y_sensor_id,
    .remove   = ir2e71y_sensor_i2c_remove,
};

/* ------------------------------------------------------------------------- */
/* VARIABLES                                                                 */
/* ------------------------------------------------------------------------- */
static bdic_i2c_data_t *bdic_i2c_p = NULL;

static sensor_data_t   *sensor_data_p = NULL;

static unsigned int ir2e71y_int_irq_port = 0;
static struct platform_device *ir2e71y_int_irq_port_pdev = NULL;
static int ir2e71y_int_irq_port_staus = 0;
static spinlock_t ir2e71y_set_irq_spinlock;

/* ------------------------------------------------------------------------- */
/* DEBUG MACROS                                                              */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* FUNCTIONS                                                                 */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* API                                                                       */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/*ir2e71y_IO_API_delay_us                                                    */
/* ------------------------------------------------------------------------- */
void ir2e71y_IO_API_delay_us(unsigned long usec)
{
    struct timespec tu;

    IR2E71Y_WAIT_LOG("wait start. expect = %ld.%ld(ms). caller = %pS",
        (usec/1000), (usec%1000),__builtin_return_address(0));

    if (usec >= 1000 * 1000) {
        tu.tv_sec  = usec / 1000000;
        tu.tv_nsec = (usec % 1000000) * 1000;
    } else {
        tu.tv_sec  = 0;
        tu.tv_nsec = usec * 1000;
    }

    hrtimer_nanosleep(&tu, NULL, HRTIMER_MODE_REL, CLOCK_MONOTONIC);

    IR2E71Y_WAIT_LOG("wait end.");
    return;
}

/* ------------------------------------------------------------------------- */
/* void ir2e71y_IO_API_msleep                                                 */
/* ------------------------------------------------------------------------- */
void ir2e71y_IO_API_msleep(unsigned int msec)
{
    IR2E71Y_WAIT_LOG("wait start. expect = %u. caller = %pS",
        msec,__builtin_return_address(0));
    msleep(msec);

    IR2E71Y_WAIT_LOG("wait end.");
    return;
}

/* ------------------------------------------------------------------------- */
/* void ir2e71y_IO_API_usleep                                                */
/* ------------------------------------------------------------------------- */
void ir2e71y_IO_API_usleep(unsigned int usec)
{
    IR2E71Y_WAIT_LOG("wait start. expect = %u.%u(ms). caller = %pS",
        (usec/1000), (usec%1000),__builtin_return_address(0));
    usleep_range(usec, usec);

    IR2E71Y_WAIT_LOG("wait end.");
    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_IO_API_Host_gpio_init                                              */
/* ------------------------------------------------------------------------- */
void ir2e71y_IO_API_Host_gpio_init(void)
{
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_IO_API_Host_gpio_exit                                              */
/* ------------------------------------------------------------------------- */
void ir2e71y_IO_API_Host_gpio_exit(void)
{
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_IO_API_Host_gpio_request                                          */
/* ------------------------------------------------------------------------- */
int ir2e71y_IO_API_Host_gpio_request(int num, char *label)
{
    return ir2e71y_host_gpio_request(num, label);
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_IO_API_Host_gpio_free                                             */
/* ------------------------------------------------------------------------- */
int  ir2e71y_IO_API_Host_gpio_free(int num)
{
    return ir2e71y_host_gpio_free(num);
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_IO_API_set_Host_gpio                                              */
/* ------------------------------------------------------------------------- */
int  ir2e71y_IO_API_set_Host_gpio(int num, int value)
{
    return ir2e71y_IO_set_Host_gpio(num, value);
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_IO_API_set_irq_port                                               */
/* ------------------------------------------------------------------------- */
void ir2e71y_IO_API_set_irq_port(int irq_port, struct platform_device *pdev)
{
    ir2e71y_int_irq_port = irq_port;
    ir2e71y_int_irq_port_pdev = pdev;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_IO_API_request_irq                                                */
/* ------------------------------------------------------------------------- */
int ir2e71y_IO_API_request_irq(irqreturn_t (*irq_handler)(int , void *))
{
    int ret = IR2E71Y_RESULT_SUCCESS;
    
    if (!ir2e71y_int_irq_port) {
        return IR2E71Y_RESULT_FAILURE;
    }
    if ((irq_handler == NULL)
    ||  (ir2e71y_int_irq_port_pdev == NULL)) {
        return IR2E71Y_RESULT_FAILURE;
    }

    ret = devm_request_irq(&ir2e71y_int_irq_port_pdev->dev, ir2e71y_int_irq_port, *irq_handler,
                        IR2E71Y_INT_FLAGS,   "bl71y8", NULL);

    if (ret == 0) {
        disable_irq(ir2e71y_int_irq_port);
    }

    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_IO_API_free_irq                                                   */
/* ------------------------------------------------------------------------- */
void ir2e71y_IO_API_free_irq(void)
{
    free_irq(ir2e71y_int_irq_port, NULL);
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_IO_API_set_irq_init                                               */
/* ------------------------------------------------------------------------- */
void ir2e71y_IO_API_set_irq_init(void)
{
    spin_lock_init(&ir2e71y_set_irq_spinlock);
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_IO_API_set_irq                                                    */
/* ------------------------------------------------------------------------- */
int ir2e71y_IO_API_set_irq(int enable)
{
    int ret = IR2E71Y_RESULT_SUCCESS;
    unsigned long flags = 0;

    spin_lock_irqsave( &ir2e71y_set_irq_spinlock, flags);

    if (enable == ir2e71y_int_irq_port_staus) {
        spin_unlock_irqrestore( &ir2e71y_set_irq_spinlock, flags);
        return IR2E71Y_RESULT_SUCCESS;
    }

    if (enable == IR2E71Y_IRQ_ENABLE) {
        enable_irq_wake(ir2e71y_int_irq_port);
        enable_irq(ir2e71y_int_irq_port);
        ir2e71y_int_irq_port_staus = enable;
    } else if (enable == IR2E71Y_IRQ_DISABLE) {
        disable_irq_nosync(ir2e71y_int_irq_port);
        disable_irq_wake(ir2e71y_int_irq_port);
        ir2e71y_int_irq_port_staus = enable;
    } else {
        IR2E71Y_ERR("<INVALID_VALUE> enable=%d", enable);
        ret = IR2E71Y_RESULT_FAILURE;
    }
    spin_unlock_irqrestore( &ir2e71y_set_irq_spinlock, flags);
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_IO_set_Host_gpio                                                   */
/* ------------------------------------------------------------------------- */
static int  ir2e71y_IO_set_Host_gpio(int num, int value)
{
    if (value != IR2E71Y_GPIO_CTL_LOW &&
        value != IR2E71Y_GPIO_CTL_HIGH) {
        IR2E71Y_ERR("<INVALID_VALUE> value(%d).", value);
        return IR2E71Y_RESULT_FAILURE;
    }

    IR2E71Y_INFO("num(%d), value(%d)", num, value);
    gpio_set_value(num, value);

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_IO_API_get_Host_gpio                                              */
/* ------------------------------------------------------------------------- */
int ir2e71y_IO_API_get_Host_gpio(int num)
{
    int ret;


    ret = gpio_get_value(num);
    IR2E71Y_INFO("num = %d, ret = %d", num, ret);


    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_IO_API_Host_i2c_send                                               */
/* ------------------------------------------------------------------------- */
int  ir2e71y_IO_API_Host_i2c_send(unsigned char slaveaddr, unsigned char *sendval, unsigned char size)
{
    struct i2c_adapter *adap;
    struct i2c_msg msg;
    int i2c_ret;
    int result = 1;
    int retry;
#ifdef IR2E71Y_LOG_ENABLE
    int i;
    char logbuf[512], work[16];
#endif /* IR2E71Y_LOG_ENABLE */

    if (slaveaddr == sensor_data_p->this_client->addr) {
        adap = sensor_data_p->this_client->adapter;
    } else {
        IR2E71Y_ERR("<OTHER> slaveaddr(0x%02x) device nothing.", slaveaddr);
        return IR2E71Y_RESULT_FAILURE;
    }

    if (sendval == NULL) {
        IR2E71Y_ERR("<NULL_POINTER> data.");
        return IR2E71Y_RESULT_FAILURE;
    }

    memset(&msg, 0, sizeof(msg));
    msg.addr     = slaveaddr;
    msg.flags    = 0;
    msg.len      = size;
    msg.buf      = sendval;


    for (retry = 0; retry <= IR2E71Y_I2C_RETRY; retry++) {
        i2c_ret = i2c_transfer(adap, &msg, 1);
        if (i2c_ret > 0) {
            result = 0;
            break;
        }
    }

#ifdef IR2E71Y_LOG_ENABLE
    memset(logbuf, 0x00, sizeof(logbuf));
    for (i = 0; i < size; i++) {
        sprintf(work, "%02X", msg.buf[i]);
        strcat(logbuf, work);
    }
    IR2E71Y_I2CLOG("slaveaddr=0x%02X, sendval=0x%s, size=%d", slaveaddr, logbuf, size);
#endif /* IR2E71Y_LOG_ENABLE */
    if (result == 1) {
        IR2E71Y_ERR("<OTHER> i2c_transfer time out(slaveaddr = 0x%02x, i2c_ret = %d).", slaveaddr, i2c_ret);
        return IR2E71Y_RESULT_FAILURE_I2C_TMO;
    }

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_IO_API_Host_i2c_recv                                              */
/* ------------------------------------------------------------------------- */
int  ir2e71y_IO_API_Host_i2c_recv(unsigned char slaveaddr, unsigned char *sendval, unsigned char sendsize,
                                   unsigned char *recvval, unsigned char recvsize)
{
    struct i2c_adapter *adap;
    struct i2c_msg msg[2];
    int i2c_ret;
    int result = 1;
    int retry;
#ifdef IR2E71Y_LOG_ENABLE
    int i;
    char logbuf[512], work[16];
#endif /* IR2E71Y_LOG_ENABLE */

    if (slaveaddr == sensor_data_p->this_client->addr) {
        adap = sensor_data_p->this_client->adapter;
    } else {
        IR2E71Y_ERR("<OTHER> slaveaddr(0x%02x) device nothing.", slaveaddr);
        return IR2E71Y_RESULT_FAILURE;
    }

    if ((sendval == NULL) || (recvval == NULL)) {
        IR2E71Y_ERR("<NULL_POINTER> data.");
        return IR2E71Y_RESULT_FAILURE;
    }

    memset(msg, 0, sizeof(*msg) * ARRAY_SIZE(msg));
    msg[0].addr     = slaveaddr;
    msg[0].flags    = 0;
    msg[0].len      = sendsize;
    msg[0].buf      = sendval;

    msg[1].addr  = slaveaddr;
    msg[1].flags = I2C_M_RD;
    msg[1].len   = recvsize;
    msg[1].buf   = recvval;

    for (retry = 0; retry <= IR2E71Y_I2C_RETRY; retry++) {

        i2c_ret = i2c_transfer(adap, msg, 2);

        if (i2c_ret > 0) {
            result = 0;
            break;
        }
    }

#ifdef IR2E71Y_LOG_ENABLE
    memset(logbuf, 0x00, sizeof(logbuf));
    for (i = 0; i < sendsize; i++) {
        sprintf(work, "%02X", msg[0].buf[i]);
        strcat(logbuf, work);
    }
    IR2E71Y_I2CLOG("msg[0]: slaveaddr=0x%02X, sendval=0x%s, size=%d", slaveaddr, logbuf, sendsize);
    memset(logbuf, 0x00, sizeof(logbuf));
    for (i = 0; i < recvsize; i++) {
        sprintf(work, "%02X", msg[1].buf[i]);
        strcat(logbuf, work);
    }
    IR2E71Y_I2CLOG("msg[1]: slaveaddr=0x%02X, recvval=0x%s, size=%d", slaveaddr, logbuf, recvsize);
#endif /* IR2E71Y_LOG_ENABLE */
    if (result == 1) {
        IR2E71Y_ERR("<OTHER> i2c_transfer time out(i2c_ret = %d).", i2c_ret);
        return IR2E71Y_RESULT_FAILURE_I2C_TMO;
    }
    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_IO_API_bdic_i2c_init                                               */
/* ------------------------------------------------------------------------- */
int  ir2e71y_IO_API_bdic_i2c_init(void)
{
    int ret;

    ret = i2c_add_driver(&bdic_driver);
    if (ret < 0) {
        IR2E71Y_ERR("<RESULT_FAILURE> i2c_add_driver.");
        return IR2E71Y_RESULT_FAILURE;
    }
    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_IO_API_bdic_i2c_exit                                               */
/* ------------------------------------------------------------------------- */
int  ir2e71y_IO_API_bdic_i2c_exit(void)
{
    i2c_del_driver(&bdic_driver);

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_IO_API_bdic_i2c_read                                              */
/* ------------------------------------------------------------------------- */
int  ir2e71y_IO_API_bdic_i2c_read(unsigned char addr, unsigned char *data)
{
    struct i2c_msg msg;
    unsigned char write_buf[2];
    unsigned char read_buf[2];
    int i2c_ret;
    int result = 1;
    int retry;

    if (data == NULL) {
        IR2E71Y_ERR("<NULL_POINTER> data.");
        return IR2E71Y_RESULT_FAILURE;
    }

    for (retry = 0; retry <= IR2E71Y_I2C_RETRY; retry++) {
        msg.addr     = bdic_i2c_p->this_client->addr;
        msg.flags    = 0;
        msg.len      = 1;
        msg.buf      = write_buf;
        write_buf[0] = addr;

        i2c_ret = i2c_transfer(bdic_i2c_p->this_client->adapter, &msg, 1);

        if (i2c_ret > 0) {

            msg.addr  = bdic_i2c_p->this_client->addr;
            msg.flags = I2C_M_RD;
            msg.len   = 1;
            msg.buf   = read_buf;

            i2c_ret = i2c_transfer(bdic_i2c_p->this_client->adapter, &msg, 1);
            if (i2c_ret > 0) {
                *data = read_buf[0];
                result = 0;
                break;
            }
        }
    }

    IR2E71Y_I2CLOG("(addr=0x%02X, data=0x%02X)", addr, *data);

    if (result == 1) {
        IR2E71Y_ERR("<OTHER> i2c_transfer time out(i2c_ret = %d).", i2c_ret);
        return IR2E71Y_RESULT_FAILURE_I2C_TMO;
    }

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_IO_API_bdic_i2c_write                                              */
/* ------------------------------------------------------------------------- */
int  ir2e71y_IO_API_bdic_i2c_write(unsigned char addr, unsigned char data)
{
    struct i2c_msg msg;
    unsigned char write_buf[2];
    int i2c_ret;
    int result = 1;
    int retry;

    IR2E71Y_I2CLOG("(addr=0x%02X, data=0x%02X)", addr, data);

    msg.addr     = bdic_i2c_p->this_client->addr;
    msg.flags    = 0;
    msg.len      = 2;
    msg.buf      = write_buf;
    write_buf[0] = addr;
    write_buf[1] = data;

    for (retry = 0; retry <= IR2E71Y_I2C_RETRY; retry++) {
        i2c_ret = i2c_transfer(bdic_i2c_p->this_client->adapter, &msg, 1);
        if (i2c_ret > 0) {
            result = 0;
            break;
        }
    }

    if (result == 1) {
        IR2E71Y_ERR("<OTHER> i2c_transfer time out(i2c_ret = %d).", i2c_ret);
        return IR2E71Y_RESULT_FAILURE_I2C_TMO;
    }

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_IO_API_bdic_i2c_mask_write                                         */
/* ------------------------------------------------------------------------- */
int  ir2e71y_IO_API_bdic_i2c_mask_write(unsigned char addr, unsigned char data, unsigned char mask)
{
    unsigned char read_data;
    unsigned char write_data;
    int ret;

    IR2E71Y_I2CLOG("(addr=0x%02X, data=0x%02X, mask=0x%02X)", addr, data, mask);
    ret = ir2e71y_IO_API_bdic_i2c_read(addr, &read_data);
    if (ret == IR2E71Y_RESULT_SUCCESS) {
        write_data = ((read_data & ~mask) | (data & mask));
        if (write_data != read_data) {
            ret =  ir2e71y_IO_API_bdic_i2c_write(addr, write_data);
        }
    }

    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_IO_API_bdic_i2c_multi_write                                        */
/* ------------------------------------------------------------------------- */
int  ir2e71y_IO_API_bdic_i2c_multi_write(unsigned char addr, unsigned char *wval, unsigned char size)
{
    struct i2c_msg msg;
    unsigned char write_buf[21];
    int i2c_ret;
    int result = 1;
    int retry;

    if ((size < 1) || (size > 20)) {
        IR2E71Y_ERR("<INVALID_VALUE> size(%d).", size);
        return IR2E71Y_RESULT_FAILURE;
    }

    if (wval == NULL) {
        IR2E71Y_ERR("<NULL_POINTER> val.");
        return IR2E71Y_RESULT_FAILURE;
    }

    msg.addr     = bdic_i2c_p->this_client->addr;
    msg.flags    = 0;
    msg.len      = size + 1;
    msg.buf      = write_buf;
    memset(write_buf, 0x00, sizeof(write_buf));
    write_buf[0] = addr;
    memcpy(&write_buf[1], wval, (int)size);
    IR2E71Y_I2CLOG("(addr=0x%02X, size=0x%02X", addr, size);
    IR2E71Y_I2CLOG("*wval=%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X)",
                                write_buf[1], write_buf[2], write_buf[3], write_buf[4], write_buf[5],
                                write_buf[6], write_buf[7], write_buf[8], write_buf[9], write_buf[10],
                                write_buf[11], write_buf[12], write_buf[13], write_buf[14], write_buf[15],
                                write_buf[16], write_buf[17], write_buf[18], write_buf[19], write_buf[20]);

    for (retry = 0; retry <= IR2E71Y_I2C_RETRY; retry++) {
        i2c_ret = i2c_transfer(bdic_i2c_p->this_client->adapter, &msg, 1);
        if (i2c_ret > 0) {
            result = 0;
            break;
        }
    }

    if (result == 1) {
        IR2E71Y_ERR("<OTHER> i2c_transfer time out(i2c_ret = %d).", i2c_ret);
        return IR2E71Y_RESULT_FAILURE_I2C_TMO;
    }

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_IO_API_bdic_i2c_multi_read                                        */
/* ------------------------------------------------------------------------- */
int  ir2e71y_IO_API_bdic_i2c_multi_read(unsigned char addr, unsigned char *data, int size)
{
    struct i2c_msg msg;
    unsigned char write_buf[2];
    unsigned char read_buf[1 + 8];
    int i2c_ret;
    int result = 1;
    int retry;

    if ((size < 1) || (size > 8)) {
        IR2E71Y_ERR("<INVALID_VALUE> size(%d).", size);
        return IR2E71Y_RESULT_FAILURE;
    }

    if (data == NULL) {
        IR2E71Y_ERR("<NULL_POINTER> data.");
        return IR2E71Y_RESULT_FAILURE;
    }

    msg.addr     = bdic_i2c_p->this_client->addr;
    msg.flags    = 0;
    msg.len      = 1;
    msg.buf      = write_buf;
    write_buf[0] = addr;

    for (retry = 0; retry <= IR2E71Y_I2C_RETRY; retry++) {
        i2c_ret = i2c_transfer(bdic_i2c_p->this_client->adapter, &msg, 1);
        if (i2c_ret > 0) {
            result = 0;
            break;
        }
    }

    if (result == 0) {
        msg.addr  = bdic_i2c_p->this_client->addr;
        msg.flags = I2C_M_RD;
        msg.len   = size;
        msg.buf   = read_buf;
        memset(read_buf, 0x00, sizeof(read_buf));
        for (retry = 0; retry <= IR2E71Y_I2C_RETRY; retry++) {
            i2c_ret = i2c_transfer(bdic_i2c_p->this_client->adapter, &msg, 1);
            if (i2c_ret > 0) {
                memcpy(data, &read_buf[0], size);
                result = 0;
                break;
            }
        }
    }

    IR2E71Y_I2CLOG("(addr=0x%02X, size=0x%02X, *data=%02X%02X%02X%02X%02X%02X%02X%02X)",
                        addr, size,
                        read_buf[0], read_buf[1], read_buf[2], read_buf[3],
                        read_buf[4], read_buf[5], read_buf[6], read_buf[7]);

    if (result == 1) {
        IR2E71Y_ERR("<OTHER> i2c_transfer time out(i2c_ret = %d).", i2c_ret);
        return IR2E71Y_RESULT_FAILURE_I2C_TMO;
    }

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_IO_API_sensor_i2c_init                                             */
/* ------------------------------------------------------------------------- */
int  ir2e71y_IO_API_sensor_i2c_init(void)
{
    int ret;

    ret = i2c_add_driver(&sensor_driver);
    if (ret < 0) {
        IR2E71Y_ERR("<RESULT_FAILURE> i2c_add_driver.");
        return IR2E71Y_RESULT_FAILURE;
    }
    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_IO_API_sensor_i2c_exit                                             */
/* ------------------------------------------------------------------------- */
int  ir2e71y_IO_API_sensor_i2c_exit(void)
{
    i2c_del_driver(&sensor_driver);
    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* SUB ROUTINE                                                               */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/*ir2e71y_host_gpio_request                                                  */
/* ------------------------------------------------------------------------- */
static int ir2e71y_host_gpio_request(int num, char *label)
{
    int ret = IR2E71Y_RESULT_SUCCESS;

    gpio_request(num, label);

    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_host_gpio_free                                                     */
/* ------------------------------------------------------------------------- */
static int ir2e71y_host_gpio_free(int num)
{
    int ret = IR2E71Y_RESULT_SUCCESS;

    gpio_free(num);

    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_i2c_probe                                                     */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_i2c_probe(struct i2c_client *client, const struct i2c_device_id *devid)
{
    bdic_i2c_data_t *i2c_p;

    if (bdic_i2c_p != NULL) {
        return -EPERM;
    }

    i2c_p = (bdic_i2c_data_t *)kzalloc(sizeof(bdic_i2c_data_t), GFP_KERNEL);
    if (i2c_p == NULL) {
        return -ENOMEM;
    }

    bdic_i2c_p = i2c_p;

    i2c_set_clientdata(client, i2c_p);
    i2c_p->this_client = client;

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_i2c_remove                                                    */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_i2c_remove(struct i2c_client *client)
{
    bdic_i2c_data_t *i2c_p;

    i2c_p = i2c_get_clientdata(client);

    kfree(i2c_p);

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_sensor_i2c_probe                                                   */
/* ------------------------------------------------------------------------- */
static int ir2e71y_sensor_i2c_probe(struct i2c_client *client, const struct i2c_device_id *devid)
{
    sensor_data_t *i2c_p;

    if (sensor_data_p != NULL) {
        return -EPERM;
    }

    i2c_p = (sensor_data_t *)kzalloc(sizeof(sensor_data_t), GFP_KERNEL);
    if (i2c_p == NULL) {
        return -ENOMEM;
    }

    sensor_data_p = i2c_p;

    i2c_set_clientdata(client, i2c_p);
    i2c_p->this_client = client;

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_sensor_i2c_remove                                                  */
/* ------------------------------------------------------------------------- */
static int ir2e71y_sensor_i2c_remove(struct i2c_client *client)
{
    sensor_data_t *i2c_p;

    i2c_p = i2c_get_clientdata(client);

    kfree(i2c_p);
    sensor_data_p = NULL;

    return IR2E71Y_RESULT_SUCCESS;
}
MODULE_DESCRIPTION("SHARP DISPLAY DRIVER MODULE");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("SHARP CORPORATION");
MODULE_VERSION("1.00");
/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */
