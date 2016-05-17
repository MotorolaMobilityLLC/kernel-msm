/*
 *  shub-spi.c - Linux kernel modules for Sensor Hub 
 *
 *  Copyright (C) 2014 LAPIS SEMICONDUCTOR CO., LTD.
 *
 *  This file is based on :
 *    ml610q792.c - Linux kernel modules for acceleration sensor
 *    http://android-dev.kyocera.co.jp/source/versionSelect_URBANO.html
 *    (C) 2012 KYOCERA Corporation
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
#include <linux/moduleparam.h>
#include <linux/input-polldev.h>

#include <asm/uaccess.h> 
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/delay.h>
#include <linux/wakelock.h>

#include <linux/module.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/poll.h>
//#include <asm/gpio.h>
#include <linux/types.h>
#include "ml630q790.h"
#include <linux/sched.h>
//#include <linux/earlysuspend.h>
#include <linux/miscdevice.h>

#define SSIO_MASK_WRITE              (0x7f)
#define SSIO_MASK_READ               (0x80)

static int32_t spi_probe( struct spi_device *client );
static int32_t spi_remove( struct spi_device *client );
static int32_t spi_suspend( struct spi_device *client, pm_message_t mesg );
static int32_t spi_resume( struct spi_device *client );
static void    spi_shutdown( struct spi_device *client );

static struct spi_device *client_mcu;

static int shub_acc_axis_val = 0;
static int shub_gyro_axis_val = 0;
static int shub_mag_axis_val = 0;

#if 1
#ifdef CONFIG_OF
static const struct of_device_id shshub_dev_dt_match[] = {
	{ .compatible = "sharp,sensorhub",},
	{}
};
#else
#define shshub_dev_dt_match NULL;
#endif /* CONFIG_OF */
#endif

static struct spi_driver interface_driver = {
    .probe       = spi_probe,
    .driver = {
        .name    = SENOSR_HUB_DRIVER_NAME,
        .bus     = &spi_bus_type,
        .owner   = THIS_MODULE,
        .of_match_table = shshub_dev_dt_match,
    },
    .remove      = spi_remove,
    .suspend     = spi_suspend,
    .resume      = spi_resume,
    .shutdown    = spi_shutdown,
};

static int32_t spi_remove( struct spi_device *client )
{
    return shub_remove();
}

static int32_t spi_suspend( struct spi_device *client, pm_message_t mesg )
{
    shub_suspend();
    return 0;
}

static int32_t spi_resume( struct spi_device *client )
{
    shub_resume();
    return 0;
}

static void spi_shutdown( struct spi_device *client )
{
}

static int32_t spi_probe( struct spi_device *client )
{

    int rc;
    struct device_node *np = client->dev.of_node;
    u32 temp_val;
    rc = of_property_read_u32(np, "shub,shub_acc_axis_val", &temp_val);
    if (rc) {
        printk("[shub]Unable to read. shub,shub_acc_axis_val\n");
        shub_acc_axis_val = 0;
    }
    else {
        shub_acc_axis_val = temp_val;
    }

    rc = of_property_read_u32(np, "shub,shub_gyro_axis_val", &temp_val);
    if (rc) {
        printk("[shub]Unable to read. shub,shub_gyro_axis_val\n");
        shub_gyro_axis_val = 0;
    }
    else {
        shub_gyro_axis_val = temp_val;
    }

    rc = of_property_read_u32(np, "shub,shub_mag_axis_val", &temp_val);
    if (rc) {
        printk("[shub]Unable to read. shub,shub_mag_axis_val\n");
        shub_mag_axis_val = 0;
    }
    else {
        shub_mag_axis_val = temp_val;
    }

    shub_set_gpio_no(client);

    client_mcu = client;
    client_mcu->bits_per_word = 8;

    client_mcu->max_speed_hz = 4*1000*1000;

    return shub_probe();
}

void shub_sensor_rep_spi(struct seq_file *s)
{
    seq_printf(s, "[spi       ]");
    seq_printf(s, "shub_acc_axis_val=%d, ",shub_acc_axis_val);
    seq_printf(s, "shub_gyro_axis_val=%d, ",shub_gyro_axis_val);
    seq_printf(s, "shub_mag_axis_val=%d\n",shub_mag_axis_val);
}

int32_t hostif_write_proc(uint8_t adr, const uint8_t *data, uint16_t size)
{
    int32_t ret = 0;
    struct spi_message  message;
    struct spi_transfer transfer;
    uint8_t send_data[512+128];

    if((data == NULL) || (size == 0)){
        printk("SPI write input error(data %p, size %x)\n", data, size);
        return -1;
    }

    send_data[0] = adr;
    memcpy(&send_data[1], data, size);
    memset(&transfer, 0, sizeof(transfer));

    adr &= SSIO_MASK_WRITE;

#ifndef SHUB_SW_PINCTRL
    ret = spi_setup(client_mcu);
    if(ret < 0) {
        printk("init SPI failed. ret=%x\n", ret);
        return ret;
    }
#endif
    spi_message_init(&message);

    transfer.tx_buf = send_data;
    transfer.rx_buf = NULL;
    transfer.len    = 1 + size;
    transfer.speed_hz = 2*1000*1000;
    spi_message_add_tail(&transfer, &message);

    ret = spi_sync(client_mcu, &message);
    if(ret < 0){
        printk("SPI write error(ret %x, adr %x, size %x)", ret, adr, size);
    }   

    return ret;
}
EXPORT_SYMBOL(hostif_write_proc);

int32_t hostif_read_proc(uint8_t adr, uint8_t *data, uint16_t size)
{
    int32_t ret = 0;
    struct spi_message  message;
    struct spi_transfer transfer[2];

    if( (data == NULL) || (size == 0)){
        printk("SPI read input error(data %p, size %x)", data, size);
        return -1;
    }

    memset(&transfer, 0, sizeof(transfer));

    adr |= SSIO_MASK_READ;

#ifndef SHUB_SW_PINCTRL
    ret = spi_setup(client_mcu);
    if(ret < 0){
        printk("init SPI failed. ret=%x\n", ret);
        return ret;
    }
#endif
    spi_message_init(&message);

    transfer[0].tx_buf = &adr;
    transfer[0].rx_buf = NULL;
    transfer[0].len    = 1;
    transfer[0].speed_hz = 2*1000*1000;
    spi_message_add_tail(&transfer[0], &message);

    transfer[1].tx_buf = NULL;
    transfer[1].rx_buf = (void *)data;
    transfer[1].len    = size;
    transfer[1].speed_hz = 2*1000*1000;
    spi_message_add_tail(&transfer[1], &message);

    ret = spi_sync(client_mcu, &message);
    if(ret < 0){
        printk("read error(ret %x, adr %x, size %x)", ret, adr, size);
    }

    return ret;
}

int shub_get_acc_axis_val(void)
{
    printk("[shub]acc_axis_val=%d\n", shub_acc_axis_val);
    return shub_acc_axis_val;
}

int shub_get_gyro_axis_val(void)
{
    printk("[shub]gyro_axis_val=%d\n", shub_gyro_axis_val);
    return shub_gyro_axis_val;
}

int shub_get_mag_axis_val(void)
{
    printk("[shub]mag_axis_val=%d\n", shub_mag_axis_val);
    return shub_mag_axis_val;
}

EXPORT_SYMBOL(hostif_read_proc);

module_spi_driver(interface_driver);

MODULE_DESCRIPTION("SensorHub SPI Driver");
MODULE_AUTHOR("LAPIS SEMICONDUCTOR");
MODULE_LICENSE("GPL v2");

