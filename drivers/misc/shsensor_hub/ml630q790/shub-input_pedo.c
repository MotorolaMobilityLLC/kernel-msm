/*
 *  shub-input_pedo.c - Linux kernel modules for interface of ML630Q790
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

#ifdef CONFIG_ANDROID_ENGINEERING
static int shub_pedo_log = 0;
module_param(shub_pedo_log, int, 0600);
#define DBG_PEDO_IO(msg, ...) {                      \
    if(shub_pedo_log & 0x01)                         \
        printk("[shub][pedo] " msg, ##__VA_ARGS__);  \
}
#define DBG_PEDO_DATA(msg, ...) {                    \
    if(shub_pedo_log & 0x02)                         \
        printk("[shub][pedo] " msg, ##__VA_ARGS__);  \
}
#else
#define DBG_PEDO_IO(msg, ...)
#define DBG_PEDO_DATA(msg, ...)
#endif

#define PEDOM_MAX 1000000
#define PEDOM_MIN 0

#define INDEX_STPS         0
#define INDEX_TM           1
#define INDEX_TMNS         2
#define INDEX_SUM          3
#define DEFAULT_DELAY_MS (200)
#define INPUT_DEV_NAME "shub_pedo"
#define INPUT_DEV_PHYS "shub_pedo/input0"
#define MISC_DEV_NAME  "shub_io_pedo"
#define SHUB_EVENTS_PER_PACKET ((LOGGING_RAM_SIZE/(DATA_SIZE_PEDOCNT+1))*2) /*+1 is ID size*/

static DEFINE_MUTEX(shub_lock);

static struct platform_device *pdev;
static struct input_dev *shub_idev;
static int32_t        power_state     = 0;
static IoCtlBatchInfo batch_param     = { 0, 0, 0 };
static struct work_struct sensor_poll_work;

static void shub_sensor_poll_work_func(struct work_struct *work);
static void shub_set_sensor_poll(int32_t en);
#if 0
static void shub_set_abs_params(void);
#endif
static int32_t currentActive;

static struct hrtimer poll_timer;
extern int32_t setMaxBatchReportLatency(uint32_t sensor, int64_t latency);

static int32_t shub_probe_pedo(struct platform_device *pfdev);
static int32_t shub_remove_pedo(struct platform_device *pfdev);

#ifdef CONFIG_OF
    static struct of_device_id shub_of_match_tb_pedo[] = {
        { .compatible = "sharp,shub_pedo" ,},
        {}
    };
#else
    #define shub_of_match_tb_pedo NULL
#endif


static long shub_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    void __user *argp = (void __user *)arg;
    int32_t ret = -1, tmpval = 0;
    switch (cmd) {
        case SHUBIO_PEDO_ACTIVATE:
            ret = copy_from_user(&tmpval, argp, sizeof(tmpval));
            if (ret) {
                printk("error : shub_ioctl(cmd = SHUBIO_PEDO_ACTIVATE)\n");
                return -EFAULT;
            }
            DBG_PEDO_IO("ioctl(cmd = Set_Active) : val=%d\n", tmpval);
            mutex_lock(&shub_lock);
            currentActive = shub_get_current_active() & SHUB_ACTIVE_PEDOM;
            if((batch_param.m_Latency != 0) && (currentActive == 0)){
                //polling off and batch enable/disable
                if(tmpval != 0){
                    //batch start/stop
                    if(batch_param.m_Latency > 0){
                        ret = shub_activate_logging(SHUB_ACTIVE_PEDOM_NO_NOTIFY, 1);
                    }else{
                        ret = shub_activate_logging(SHUB_ACTIVE_PEDOM_NO_NOTIFY, 0);
                    }
                }else{
                    //batch stop
                    ret = shub_activate_logging(SHUB_ACTIVE_PEDOM_NO_NOTIFY, 0);
                    setMaxBatchReportLatency(SHUB_ACTIVE_PEDOM, 0);
                }
            }else{
                //set mcu sensor measure
                ret = shub_activate( SHUB_ACTIVE_PEDOM, tmpval);
                //set polling start 
                shub_set_sensor_poll(tmpval);

                //batch stop 
                ret = shub_activate_logging(SHUB_ACTIVE_PEDOM_NO_NOTIFY, 0);
                setMaxBatchReportLatency(SHUB_ACTIVE_PEDOM, 0);
            }
            if(ret != -1){
                power_state = tmpval;
            }else{
                mutex_unlock(&shub_lock);
                return -EFAULT;
            }
            mutex_unlock(&shub_lock);
            break;

        case SHUBIO_PEDO_SET_BATCH :
            {
                IoCtlBatchInfo param;
                uint64_t delayNs;
                int32_t delay;
                ret = copy_from_user(&param, argp, sizeof(param));
                if (ret) {
                    printk( "error(copy_from_user) : shub_ioctl(cmd = SHUBIO_PEDO_SET_BATCH)\n" );
                    return -EFAULT;
                }
                DBG_PEDO_IO("ioctl(cmd = Set_Batch) : flg=%d, Period=%lld, Latency=%lld\n", param.m_Flasg, param.m_PeriodNs, param.m_Latency);
                if((param.m_Flasg & 0x01) == 0x01){
                    return 0;
                }
                mutex_lock(&shub_lock);
                delayNs = param.m_PeriodNs;
                delay = (int32_t)do_div(delayNs, 1000000);
                delay = (int32_t)delayNs;
                delay = (delay > SHUB_TIMER_MAX) ? SHUB_TIMER_MAX : delay;
                if(power_state != 0){
                    //poll on -> batch on
                    if((batch_param.m_Latency == 0) && (param.m_Latency > 0))
                    {
                        batch_param.m_Flasg    = param.m_Flasg;
                        batch_param.m_PeriodNs = param.m_PeriodNs;
                        batch_param.m_Latency  = param.m_Latency;
                        //disable poll 
                        shub_set_delay(SHUB_ACTIVE_PEDOM, delay);
                        ret = shub_activate( SHUB_ACTIVE_PEDOM, 0);
                        currentActive = shub_get_current_active() & SHUB_ACTIVE_PEDOM;
                        if(ret == -1){
                            mutex_unlock(&shub_lock);
                            return -EFAULT;
                        }

                        //enable batch
                        setMaxBatchReportLatency(SHUB_ACTIVE_PEDOM, batch_param.m_Latency);
                        shub_set_delay_logging(SHUB_ACTIVE_PEDOM_NO_NOTIFY, batch_param.m_PeriodNs);
                        ret = shub_activate_logging(SHUB_ACTIVE_PEDOM_NO_NOTIFY, 1);
                        if(ret == -1){
                            mutex_unlock(&shub_lock);
                            return -EFAULT;
                        }

                        mutex_unlock(&shub_lock);
                        return 0;
                    }
                    //batch on -> poll on
                    if((batch_param.m_Latency > 0) && (param.m_Latency == 0))
                    {
                        batch_param.m_Flasg    = param.m_Flasg;
                        batch_param.m_PeriodNs = param.m_PeriodNs;
                        batch_param.m_Latency  = param.m_Latency;

                        //disable batch
                        setMaxBatchReportLatency(SHUB_ACTIVE_PEDOM, batch_param.m_Latency);
                        shub_set_delay_logging(SHUB_ACTIVE_PEDOM_NO_NOTIFY, batch_param.m_PeriodNs);
                        ret = shub_activate_logging(SHUB_ACTIVE_PEDOM_NO_NOTIFY, 0);
                        if(ret == -1){
                            mutex_unlock(&shub_lock);
                            return -EFAULT;
                        }

                        //enable poll
                        shub_set_delay(SHUB_ACTIVE_PEDOM, delay);
                        ret = shub_activate( SHUB_ACTIVE_PEDOM, 1);
                        currentActive = shub_get_current_active() & SHUB_ACTIVE_PEDOM;
                        if(ret == -1){
                            mutex_unlock(&shub_lock);
                            return -EFAULT;
                        }
                        shub_set_sensor_poll(1);
                        mutex_unlock(&shub_lock);
                        return 0;
                    }
                }
                /* flag SENSORS_BATCH_DRY_RUN is OFF */
                batch_param.m_Flasg    = param.m_Flasg;
                batch_param.m_PeriodNs = param.m_PeriodNs;
                batch_param.m_Latency  = param.m_Latency;

                if(param.m_Latency == 0){
                    shub_set_delay(SHUB_ACTIVE_PEDOM, delay);
                    if(currentActive != 0){
                        shub_set_sensor_poll(1);
                    }
                }else{
                    shub_set_delay_logging(SHUB_ACTIVE_PEDOM_NO_NOTIFY, batch_param.m_PeriodNs);
                }
                setMaxBatchReportLatency(SHUB_ACTIVE_PEDOM, batch_param.m_Latency);
                mutex_unlock(&shub_lock);
            }
            break;

        case SHUBIO_PEDO_FLUSH :
            {
                DBG_PEDO_IO("ioctl(cmd = Flush)\n");
                mutex_lock(&shub_lock);
                if(power_state != 0){
                    shub_logging_flush();
                    setMaxBatchReportLatency(SHUB_ACTIVE_PEDOM, batch_param.m_Latency);
                    shub_input_sync_init(shub_idev);
//                  input_event(shub_idev, EV_SYN, SYN_REPORT, 2);
                    shub_input_first_report(shub_idev, 1);
                    input_event(shub_idev, EV_SYN, SYN_REPORT, (SHUB_INPUT_META_DATA | SHUB_INPUT_PEDO));
                }else{
                    mutex_unlock(&shub_lock);
                    return -EFAULT;
                }
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
    ret =  shub_ioctl(filp, cmd , arg);
    SHUB_DBG_TIME_END(cmd)
    shub_qos_end();

    return ret;
}

static void shub_sensor_poll_work_func(struct work_struct *work)
{
    int32_t data[INDEX_SUM]= {0};
    mutex_lock(&shub_lock);
    shub_qos_start();
    shub_get_sensors_data(SHUB_ACTIVE_PEDOM, data);

    DBG_PEDO_DATA("data Step_Cnt=%d\n", data[INDEX_STPS]);

    SHUB_INPUT_VAL_CLEAR(shub_idev, ABS_X, data[INDEX_STPS]);
    input_report_abs(shub_idev, ABS_X, data[INDEX_STPS]);
    input_report_abs(shub_idev, ABS_MISC, data[INDEX_TM]);
    input_report_abs(shub_idev, ABS_VOLUME, data[INDEX_TMNS]);
    shub_input_sync_init(shub_idev);
#if 1
    input_event(shub_idev, EV_SYN, SYN_REPORT, SHUB_INPUT_PEDO);
#else
    input_event(shub_idev, EV_SYN, SYN_REPORT, 0);
#endif
    shub_qos_end();
    mutex_unlock(&shub_lock);
}

static enum hrtimer_restart shub_sensor_poll(struct hrtimer *tm)
{
    schedule_work(&sensor_poll_work);
    return HRTIMER_NORESTART;
}

void shub_input_report_stepcnt(int32_t *data)
{
    if(data == NULL) {
        return;
    }

    DBG_PEDO_DATA("data Step_Cnt=%d, t(s)=%d, t(ns)=%d\n", data[INDEX_STPS],data[INDEX_TM],data[INDEX_TMNS]);

    SHUB_INPUT_VAL_CLEAR(shub_idev, ABS_X, (uint32_t)data[INDEX_STPS]);
    input_report_abs(shub_idev, ABS_X, (uint32_t)data[INDEX_STPS]);
    input_report_abs(shub_idev, ABS_MISC, data[INDEX_TM]);
    input_report_abs(shub_idev, ABS_VOLUME, data[INDEX_TMNS]);
    shub_input_sync_init(shub_idev);
#if 1
    input_event(shub_idev, EV_SYN, SYN_REPORT, SHUB_INPUT_PEDO);
#else
    input_event(shub_idev, EV_SYN, SYN_REPORT, 1);
#endif
}

#if 0
static void shub_set_abs_params(void)
{
    input_set_abs_params(shub_idev, ABS_MISC, 0, 0xFFFFFFFF, 0, 0);
    input_set_abs_params(shub_idev, ABS_VOLUME, 0, 0xFFFFFFFF, 0, 0);
    input_set_abs_params(shub_idev, ABS_X, PEDOM_MIN, PEDOM_MAX, 0, 0);
    shub_set_param_first(shub_idev);
}
#endif

static void shub_set_sensor_poll(int32_t en)
{
    hrtimer_cancel(&poll_timer);
    if(en){
        hrtimer_start(&poll_timer, ns_to_ktime((int64_t)(DEFAULT_DELAY_MS * NSEC_PER_MSEC)), HRTIMER_MODE_REL);
    }
}

void shub_sensor_rep_input_pedo(struct seq_file *s)
{
    seq_printf(s, "[pedo      ]");
    seq_printf(s, "power_state=%d, ",power_state);
    seq_printf(s, "batch_param.m_Flasg=%d, ",batch_param.m_Flasg);
    seq_printf(s, "batch_param.m_PeriodNs=%lld, ",batch_param.m_PeriodNs);
    seq_printf(s, "batch_param.m_Latency=%lld\n",batch_param.m_Latency);
}

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

static int32_t shub_probe_pedo(struct platform_device *pfdev)
{
    int32_t ret = 0;


    if(!shub_connect_check()){
        printk(KERN_INFO "shub_stepcnt Connect Error!!\n");
        ret = -ENODEV;
        goto out_driver;
    }


    pdev = platform_device_register_simple(INPUT_DEV_NAME, -1, NULL, 0);
    if (IS_ERR(pdev)) {
        ret = PTR_ERR(pdev);
        goto out_driver;
    }

#if 1
    shub_idev = shub_com_allocate_device(SHUB_INPUT_PEDO, &pdev->dev);
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
    input_set_events_per_packet(shub_idev, SHUB_EVENTS_PER_PACKET);

    ret = input_register_device(shub_idev);
    if (ret)
        goto out_idev;
#endif

    ret = misc_register(&shub_device);
    if (ret) {
        printk("shub-init: shub_io_device register failed\n");
        goto exit_misc_device_register_failed;
    }
    INIT_WORK(&sensor_poll_work, shub_sensor_poll_work_func);
    hrtimer_init(&poll_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    poll_timer.function = shub_sensor_poll;
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

static int32_t shub_remove_pedo(struct platform_device *pfdev)
{
    misc_deregister(&shub_device);
#if 1
    shub_com_unregister_device(SHUB_INPUT_PEDO);
#else
    input_unregister_device(shub_idev);
    input_free_device(shub_idev);
#endif
    platform_device_unregister(pdev);

    cancel_work_sync(&sensor_poll_work);
    return 0;
}

static struct platform_driver shub_pedo_driver = {
    .probe = shub_probe_pedo,
    .remove = shub_remove_pedo,
    .shutdown = NULL,
    .driver = {
        .name = "shub_dev_pedo",
        .of_match_table = shub_of_match_tb_pedo,
    },
};

static int32_t __init shub_init(void)
{
    int ret;
    
    ret = platform_driver_register(&shub_pedo_driver);

    return ret;
}

static void __exit shub_exit(void)
{
    platform_driver_unregister(&shub_pedo_driver);
}

late_initcall(shub_init);
module_exit(shub_exit);

MODULE_DESCRIPTION("SensorHub Input Device (StepCounter)");
MODULE_AUTHOR("LAPIS SEMICOMDUCTOR");
MODULE_LICENSE("GPL v2");
