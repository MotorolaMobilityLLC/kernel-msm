/*
*
* NFC Driver ( Toshiba )
*
* Copyright (C) 2015, TOSHIBA CORPORATION
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation version 2.
*
* This program is distributed "as is" WITHOUT ANY WARRANTY of any
* kind, whether express or implied; without even the implied warranty
* of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
*/


/******************************************************************************
 * include
 ******************************************************************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/poll.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/kthread.h>
#include <linux/version.h>
#include <linux/uaccess.h>
#include <linux/clk.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/wakelock.h>
#include <linux/input.h>
#include <asm/uaccess.h>
#include <linux/irq.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37)
#include <linux/mutex.h>
#endif

#include <linux/miscdevice.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/qpnp/pin.h>

#include "nfc_gpio_drv.h"
#include "nfc_gpio_drv_config.h"
#include "nfc_gpio_drv_sys.h"
#include "../I2C/nfc_i2c_drv.h"

/******************************************************************************
 *
 ******************************************************************************/

#define D_VFEL_DEV_LOW 			(0)
#define D_VFEL_DEV_HIGH 		(1)
#define D_VFEL_SLEEP_USEC		(100000)

struct nfc_gpio_platform_data {
    unsigned int nfc_pon;
    unsigned int vfel;
    unsigned int nint;
    unsigned int status;
    unsigned int hvdd;
};

typedef enum nfc_gpio_irq_enum {
    INTR_NONE,
    INTR_OCCURS,
} nfc_gpio_irq;

typedef struct nfc_gpio_info_tag
{
    struct miscdevice   nfc_gpio_misc_driver;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37)
    struct mutex        nfc_gpio_irq_mutex;              /* exclusive control       */
#endif
    wait_queue_head_t   read_wait;
    loff_t              read_offset;
    nfc_gpio_irq        read_irq;
    int                 pon_gpio;
    int                 irq_gpio;
    int                 irq_no;
    struct device *dev;
    struct nfc_gpio_platform_data *pdata;
} nfc_gpio_info;

/******************************************************************************
 * data
 ******************************************************************************/
static int                  open_cnt;
static nfc_gpio_info*       g_nfc_gpio_info = NULL;
static struct wake_lock     g_wake_lock;

/******************************************************************************
 * function
 ******************************************************************************/
/*** GPIO Device Operation ***/
static ssize_t nfc_gpio_read        ( struct file* ,
                                      char* ,
                                      size_t ,
                                      loff_t*  );

static ssize_t nfc_gpio_write       ( struct file* ,
                                      const char* ,
                                      size_t ,
                                      loff_t*  );

static unsigned int nfc_gpio_poll   ( struct file *,
                                      poll_table* );

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
static int nfc_gpio_ioctl           ( struct inode* ,
                                      struct file* ,
                                      unsigned int ,
                                      unsigned long  );
#else  /* LINUX_VERSION_CODE */
static long nfc_gpio_ioctl          ( struct file *,
                                      unsigned int ,
                                      unsigned long  );
#endif /* LINUX_VERSION_CODE */
static int nfc_gpio_open            ( struct inode* ,
                                      struct file*  );

static int nfc_gpio_close           ( struct inode* ,
                                      struct file*  );

/*** Other Operation ***/
#ifndef CONFIG_NFC_T6NE6
static irqreturn_t      nfc_gpio_irq_thread_handler    ( int,
                                                         void* );
#endif  /* CONFIG_NFC_T6NE6 */

/******************************************************************************
 *
 ******************************************************************************/
/* entry point */
static struct file_operations nfc_gpio_drv_fops = {
                                .owner   = THIS_MODULE,
                                .read    = nfc_gpio_read,
                                .write   = nfc_gpio_write,
                                .poll    = nfc_gpio_poll,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
                                .ioctl   = nfc_gpio_ioctl,
#else  /* LINUX_VERSION_CODE */
                                .unlocked_ioctl = nfc_gpio_ioctl,
#ifdef CONFIG_COMPAT
                                .compat_ioctl   = nfc_gpio_ioctl,
#endif //CONFIG_COMPAT
#endif /* LINUX_VERSION_CODE */
                                .open    = nfc_gpio_open,
                                .release = nfc_gpio_close
                            };

static int nfc_gpio_probe(struct platform_device *pdev);
static int nfc_gpio_remove(struct platform_device *pdev);

static struct platform_device_id nfc_gpio_id_table[] = {
    { "sharp,nfc_gpio_drv", 0 },
    { }
};

static struct of_device_id nfc_gpio_match_table[] = {
    { .compatible = "sharp,nfc_gpio_drv", },
    {},
};

MODULE_DEVICE_TABLE(platform, nfc_gpio_id_table);
static struct platform_driver nfc_gpio_driver = {
    .probe = nfc_gpio_probe,
    .id_table = nfc_gpio_id_table,
    .remove = nfc_gpio_remove,
    .driver = {
        .name = NFC_GPIO_CONFIG_DRIVER_NAME,
        .of_match_table = nfc_gpio_match_table,
    },
};

module_platform_driver(nfc_gpio_driver);

/******************************************************************************
 * code area
 ******************************************************************************/
 /******************************************************************************
 * Kernel Init
 ******************************************************************************/
static int nfc_gpio_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    nfc_gpio_info *info = NULL;
    struct nfc_gpio_platform_data *pdata = NULL;
    struct device_node *np = NULL;
    int ret = 0;
    struct pinctrl *pin;
    struct pinctrl_state *pin_state;

    if(dev) {
        pdata = kzalloc(sizeof(struct nfc_gpio_platform_data), GFP_KERNEL);
        if (!pdata) {
            ret = -ENOMEM;
            goto err_pdata;
        }
    } else {
        ret = -ENOMEM;
        goto err_pdata;
    }

    np = dev->of_node;
    pdata->nfc_pon  = of_get_named_gpio(np, "qcom,nfc-pon"   , 0);
    pdata->nint     = of_get_named_gpio(np, "qcom,nfc-nint"  , 0);
    pdata->status   = of_get_named_gpio(np, "qcom,nfc-status", 0);
    pdata->hvdd     = of_get_named_gpio(np, "qcom,nfc-hvdd"  , 0);
    pdata->vfel     = of_get_named_gpio(np, "qcom,nfc-vfel", 0);

    info = kzalloc(sizeof(nfc_gpio_info), GFP_KERNEL);
    if (!info) {
        ret = -ENOMEM;
        kfree(pdata);
        goto err_info_alloc;
    }

    info->dev = dev;
    info->pdata = pdata;

    mutex_init( &info->nfc_gpio_irq_mutex );
    init_waitqueue_head ( &info->read_wait );
    dev_set_drvdata(dev, info);

    info->nfc_gpio_misc_driver.minor = MISC_DYNAMIC_MINOR;
    info->nfc_gpio_misc_driver.name  = NFC_GPIO_CONFIG_DRIVER_NAME;
    info->nfc_gpio_misc_driver.fops  = &nfc_gpio_drv_fops;
    info->read_irq                   = INTR_NONE;
    info->pon_gpio                   = pdata->nfc_pon;
#ifndef CONFIG_NFC_T6NE6
    info->irq_gpio                   = D_INT_GPIO_NO;
#else   /* CONFIG_NFC_T6NE6 */
    info->irq_gpio                   = 0;
#endif  /* CONFIG_NFC_T6NE6 */
    info->irq_no                     = 0;
    ret = misc_register ( &info->nfc_gpio_misc_driver );
    if ( ret < 0 )
    {
        return ( ret );
    }
    g_nfc_gpio_info = info;

    /*** GPIO PON Setting */
    pin = devm_pinctrl_get(info->dev);
    pin_state = pinctrl_lookup_state(pin, "nfc_pon_active");
    ret = pinctrl_select_state(pin, pin_state);
    ret = gpio_request ( info->pon_gpio, NFC_GPIO_CONFIG_DRIVER_NAME );
    ret = gpio_direction_output ( info->pon_gpio, 0 );

    pin = devm_pinctrl_get(info->dev);
    pin_state = pinctrl_lookup_state(pin, "nfc_vfel_active");
    ret = pinctrl_select_state(pin, pin_state);
    ret = gpio_request(info->pdata->vfel, NFC_GPIO_CONFIG_DRIVER_NAME);
    ret = gpio_direction_output(info->pdata->vfel, 0);

    pin = devm_pinctrl_get(info->dev);
    pin_state = pinctrl_lookup_state(pin, "nfc_hvdd_active");
    ret = pinctrl_select_state(pin, pin_state);
    ret = gpio_request(info->pdata->hvdd, NFC_GPIO_CONFIG_DRIVER_NAME );
    ret = gpio_direction_output(info->pdata->hvdd, 0);

    gpio_set_value(info->pdata->hvdd, 1);
    ret = nfc_i2c_nint_active();
    ret = nfc_gpio_status_active();

    wake_lock_init(&g_wake_lock, WAKE_LOCK_SUSPEND, "NFCGPIOWAKE");

    return 0;

err_info_alloc:
    kfree(info);
err_pdata:
    return ret;
}

static int nfc_gpio_remove(struct platform_device *pdev)
{
    nfc_gpio_info *info = dev_get_drvdata(&pdev->dev);
    int ret;
    struct pinctrl *pin;
    struct pinctrl_state *pin_state;

    pin = devm_pinctrl_get(info->dev);
    pin_state = pinctrl_lookup_state(pin, "nfc_pon_suspend");
    ret = pinctrl_select_state(pin, pin_state);
    gpio_free ( info->pon_gpio );

    nfc_gpio_status_suspend();

    pin = devm_pinctrl_get(info->dev);
    pin_state = pinctrl_lookup_state(pin, "nfc_vfel_suspend");
    ret = pinctrl_select_state(pin, pin_state);
    gpio_free(info->pdata->vfel);

    ret = nfc_i2c_nint_suspend();

    ret = nfc_gpio_status_suspend();
    gpio_set_value(info->pdata->hvdd, 0);
    pin = devm_pinctrl_get(info->dev);
    pin_state = pinctrl_lookup_state(pin, "nfc_hvdd_suspend");
    ret = pinctrl_select_state(pin, pin_state);
    gpio_free(info->pdata->hvdd);

    if (wake_lock_active(&g_wake_lock)) {
        wake_unlock(&g_wake_lock);
    }
    wake_lock_destroy(&g_wake_lock);

    misc_deregister ( &info->nfc_gpio_misc_driver );
    kfree(info);

    return 0;
}

/******************************************************************************
 * GPIO Driver fops
 ******************************************************************************/
/******************************************************************************
 *    function:   nfc_gpio_open
 *    brief   :   open control of a driver
 *    date    :
 *    author  :
 *
 *    return  :    0                   normal exit
 *            :   -1                   error exit
 *    input   :   Inode
 *            :   File
 *    output  :   none
 ******************************************************************************/
static int  nfc_gpio_open   ( struct inode* Inode,
                              struct file*  File )
{
#ifndef CONFIG_NFC_T6NE6
    nfc_gpio_info *info = NULL;
    int  ret;
#endif  /* CONFIG_NFC_T6NE6 */

    if (( NULL == File ) ||
        ( NULL == g_nfc_gpio_info ))
    {
        return ( -EINVAL );
    }
    if ( 0 != open_cnt )
    {
        open_cnt++;
        return ( 0 );

    }
    open_cnt++;

#ifndef CONFIG_NFC_T6NE6
    info = g_nfc_gpio_info;
    /*** GPIO Interrupt Setting */
    ret = gpio_request ( info->irq_gpio, NFC_GPIO_CONFIG_DRIVER_NAME );
    if ( ret )
    {
        open_cnt--;
        return -EINVAL;
    }
    ret = gpio_direction_input ( info->irq_gpio );
    if ( 0 > ret )
    {
        open_cnt--;
        return -EINVAL;
    }

    info->irq_no = gpio_to_irq ( info->irq_gpio );
    if ( 0 > info->irq_no )
    {
        gpio_free ( info->irq_gpio );
        open_cnt--;
        return -EINVAL;
    }
    /* Set Interrupt Handler */
    ret = request_threaded_irq ( info->irq_no,
                                 NULL,
                                 nfc_gpio_irq_thread_handler,
                                 IRQF_TRIGGER_FALLING,
                                 NFC_GPIO_CONFIG_DRIVER_NAME,
                                 info );

    if ( 0 > ret )
    {
        gpio_free ( info->irq_gpio );
        open_cnt--;
        return -EINVAL;
    }
#endif  /* CONFIG_NFC_T6NE6 */

    return 0;
}

/******************************************************************************
 *    function:   nfc_gpio_close
 *    brief   :   close control of a driver
 *    date    :
 *    author  :
 *
 *    return  :    0                   normal exit
 *            :   -1                   error exit
 *    input   :   Inode
 *            :   FIle
 *    output  :   none
 ******************************************************************************/
static int  nfc_gpio_close  ( struct inode* Inode,
                              struct file*  FIle )
{
#ifndef CONFIG_NFC_T6NE6
    static nfc_gpio_info* info = NULL;
#endif  /* CONFIG_NFC_T6NE6 */

    if (( open_cnt <= 0 ) ||
        ( NULL == FIle ))
    {
        open_cnt = 0;

        return ( -EINVAL );
    }
    open_cnt--;

    /* close all open */
    if ( 0 != open_cnt )
    {
        return 0;
    }

#ifndef CONFIG_NFC_T6NE6
    info = g_nfc_gpio_info;

    gpio_free ( info->irq_gpio );

    /* interrupt release */
    free_irq ( info->irq_no, info );
#endif  /* CONFIG_NFC_T6NE6 */

    return 0;

}

/******************************************************************************
 *    function:   nfc_gpio_read
 *    brief   :   read control of a driver
 *    date    :
 *    author  :
 *
 *    return  :    0                   normal exit
 *            :   -1                   error exit
 *    input   :   FIle
 *            :   Buffer
 *            :   Count
 *            :   OffsetPosition
 *    output  :   none
 ******************************************************************************/
static ssize_t nfc_gpio_read ( struct file*     FIle,
                               char*            Buffer,
                               size_t           Count,
                               loff_t*          OffsetPosition )
{
    return ( -ENOTSUPP );
}

/******************************************************************************
 *    function:   nfc_gpio_write
 *    brief   :   write control of a driver
 *    date    :
 *    author  :
 *
 *    return  :    0        normal exit
 *            :   -1        error exit
 *    input   :   FIle
 *            :   Buffer    [slave][reg.addr][data-0][data-1]...[data-(n-1)]
 *            :   Count     (>=3) n+2
 *            :   OffsetPosition
 *    output  :   none
 ******************************************************************************/
static ssize_t nfc_gpio_write ( struct file*    FIle,
                                const char*     Buffer,
                                size_t          Count,
                                loff_t*         OffsetPosition )
{
    char sw;
    ssize_t ret = Count;

    /* length check */
    if (Count < 1) {
        return -EIO;
    }

    if (copy_from_user(&sw, Buffer, 1)) {
        return -EFAULT;
    }

    switch(sw){
    case 0:
        gpio_set_value(g_nfc_gpio_info->pon_gpio, 0);
        usleep(10 * 1000);
        break;
    case 1:
        gpio_set_value(g_nfc_gpio_info->pon_gpio, 1);
        usleep(10 * 1000);
        break;
    case 2:
        gpio_set_value(g_nfc_gpio_info->pdata->vfel, D_VFEL_DEV_HIGH);
        usleep(D_VFEL_SLEEP_USEC);
        gpio_set_value(g_nfc_gpio_info->pdata->vfel, D_VFEL_DEV_LOW);
        break;

    default:
        ret = -EFAULT;
        break;
    }

    return ret;
}

/******************************************************************************
 *    function:   nfc_gpio_poll
 *    brief   :   poll control of a driver
 *    date    :
 *    author  :
 *
 *    return  :    0                   normal exit
 *            :   -1                   error exit
 *    input   :   File
 *            :   poll_tbl
 *    output  :   none
 ******************************************************************************/
static unsigned int nfc_gpio_poll ( struct file*    File,
                                    poll_table*     poll_tbl )
{
    int ret = 0;

#ifdef CONFIG_NFC_T6NE6
    ret = POLLERR;
#else
    nfc_gpio_irq     irq_sts = INTR_NONE;
    nfc_gpio_info    *info = NULL;

    if ( NULL == File )
    {
        return ( POLLERR );
    }
    info = g_nfc_gpio_info;

    poll_wait ( File,
                &info->read_wait,
                poll_tbl );

    mutex_lock ( &info->nfc_gpio_irq_mutex );
    irq_sts = info->read_irq;
    mutex_unlock ( &info->nfc_gpio_irq_mutex );

    if ( INTR_OCCURS == irq_sts )
    {
        ret = POLLIN | POLLRDNORM;
        info->read_irq = INTR_NONE;
    }
#endif  /* CONFIG_NFC_T6NE6 */

    return ( ret );
}

/******************************************************************************
 *    function:   nfc_gpio_ioctl
 *    brief   :   ioctl control of a driver
 *    date    :
 *    author  :
 *
 *    return  :    0                   normal exit
 *            :   -1                   error exit
 *    input   :   Inode
 *            :   FIle
 *            :   uCommand
 *            :   uArgument
 *    output  :   none
 ******************************************************************************/
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
static int nfc_gpio_ioctl ( struct inode*   Inode,
                            struct file*    FIle,
                            unsigned int    uCommand,
                            unsigned long   uArgument )
#else  /* LINUX_VERSION_CODE */
static long nfc_gpio_ioctl ( struct file*   file,
                             unsigned int   uCommand,
                             unsigned long  uArgument )
#endif /* LINUX_VERSION_CODE */
{
    int                 ret = 0;
    nfc_gpio_info *info = container_of(file->private_data, struct nfc_gpio_info_tag, nfc_gpio_misc_driver);
    NFC_GPIO_DATA_RW*   ioctl_cmd = (NFC_GPIO_DATA_RW*)uArgument;
    NFC_GPIO_DATA_RW    pon;

    switch ( uCommand )
    {
        case NFC_GPIO_IOCTL_SET_PON_L:
            gpio_set_value(info->pon_gpio, 0);
            usleep(10 * 1000);
            break;

        case NFC_GPIO_IOCTL_SET_PON_H:
            gpio_set_value(info->pon_gpio, 1);
            usleep(50 * 1000);
            break;

        case NFC_GPIO_IOCTL_SET_PON_P:
            gpio_set_value(info->pon_gpio, 0);
            mdelay(1);
            gpio_set_value(info->pon_gpio, 1);
            usleep(10 * 1000);
            break;

        case NFC_GPIO_IOCTL_GET_PON:
            if(gpio_get_value(info->pon_gpio))
            {
                pon.data = NFC_GPIO_IOCTL_GET_PON_HIGH;
            }
            else
            {
                pon.data = NFC_GPIO_IOCTL_GET_PON_LOW;
            }
            ret = copy_to_user((unsigned int *)ioctl_cmd, &pon, sizeof(NFC_GPIO_DATA_RW));
            break;

        case NFC_GPIO_IOCTL_GET_NINT:
            ret = gpio_get_value(info->pdata->nint);
            ret = copy_to_user((unsigned int *)uArgument, &ret, sizeof(unsigned int));
            break;

        case NFC_GPIO_IOCTL_GET_STATUS:
            ret = gpio_get_value(info->pdata->status);
            ret = copy_to_user((unsigned int *)uArgument, &ret, sizeof(unsigned int));
            break;

        case NFC_GPIO_IOCTL_REQ_CHIPRESET:
            gpio_set_value(info->pdata->vfel, D_VFEL_DEV_HIGH);
            usleep(D_VFEL_SLEEP_USEC);
            gpio_set_value(info->pdata->vfel, D_VFEL_DEV_LOW);

            if(wake_lock_active(&g_wake_lock)){
                wake_unlock(&g_wake_lock);
            }
            wake_lock_timeout(&g_wake_lock,(HZ*3));
            break;

        default:
            ret = -ENOIOCTLCMD;
            break;
    }

    return ( 0 );
}


/******************************************************************************
 * Other Module
 ******************************************************************************/
 #ifndef    CONFIG_NFC_T6NE6
/******************************************************************************
 *    function:   nfc_gpio_irq_thread_handler
 *    brief   :   interrpu control of a driver
 *    date    :
 *    author  :
 *
 *    return  :    0                   normal exit
 *            :   -1                   error exit
 *    input   :   irq
 *            :   dev_id
 *    output  :   none
 ******************************************************************************/
irqreturn_t     nfc_gpio_irq_thread_handler   ( int   irq,
                                                void* dev_id )
{
    nfc_gpio_info  *info = NULL;

    if ( NULL == dev_id )
    {
        return ( IRQ_NONE );
    }

    info = (nfc_gpio_info*)dev_id;

    if ( 1 == gpio_get_value( info->irq_gpio ))
    {
        return ( IRQ_HANDLED );
    }
    mutex_lock ( &info->nfc_gpio_irq_mutex );

    info->read_irq = INTR_OCCURS;

    mutex_unlock ( &info->nfc_gpio_irq_mutex );

    wake_up_interruptible ( &info->read_wait );

    return ( IRQ_HANDLED );
}
#endif  /* CONFIG_NFC_T6NE6 */

/******************************************************************************
 *    function:   nfc_gpio_status_active
 *    brief   :
 *    date    :
 *    author  :
 *
 *    return  :    0                   normal exit
 *            :   -1                   error exit
 *    input   :
 *            :
 *    output  :
 ******************************************************************************/
int nfc_gpio_status_active(void)
{
    int ret = 0;
    struct pinctrl *pin;
    struct pinctrl_state *pin_state;

    pin = devm_pinctrl_get(g_nfc_gpio_info->dev);
    pin_state = pinctrl_lookup_state(pin, "nfc_status_active");
    ret = pinctrl_select_state(pin, pin_state);

    return ret;
}

/******************************************************************************
 *    function:   nfc_gpio_status_suspend
 *    brief   :
 *    date    :
 *    author  :
 *
 *    return  :    0                   normal exit
 *            :   -1                   error exit
 *    input   :
 *            :
 *    output  :
 ******************************************************************************/
int nfc_gpio_status_suspend(void)
{
    int ret = 0;
    struct pinctrl *pin;
    struct pinctrl_state *pin_state;

    pin = devm_pinctrl_get(g_nfc_gpio_info->dev);
    pin_state = pinctrl_lookup_state(pin, "nfc_status_suspend");
    ret = pinctrl_select_state(pin, pin_state);

    return ret;
}

/******************************************************************************
 *    function:   nfc_gpio_pon_low
 *    brief   :
 *    date    :
 *    author  :
 *
 *    return  :none
 *    input   :none
 *    output  :none
 ******************************************************************************/
void nfc_gpio_pon_low(void)
{
    gpio_set_value(g_nfc_gpio_info->pon_gpio, 0);
    usleep(10 * 1000);
}
/******************************************************************************/
/***                                                                        ***/
/******************************************************************************/
MODULE_AUTHOR("Toshiba");
MODULE_DESCRIPTION("NFC GPIO Driver");
MODULE_LICENSE("GPL v2");
