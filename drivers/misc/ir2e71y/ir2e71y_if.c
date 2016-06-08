/* drivers/misc/ir2e71y/ir2e71y_if.c  (Display Driver)
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
#include <linux/of_gpio.h>
#include <linux/hrtimer.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/fb.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/wakelock.h>

#include "ir2e71y_define.h"
#include <linux/ir2e71y.h>
#ifdef CONFIG_PROXIMITY_INT_HOST
#include <sharp/proximity.h>
#endif /* CONFIG_PROXIMITY_INT_HOST */
#include "ir2e71y_priv.h"
#include "ir2e71y_context.h"
#include "ir2e71y_io.h"
#include "ir2e71y_dbg.h"
#include "ir2e71y_pm.h"
#include "ir2e71y_main.h"
#include "ir2e71y_led.h"
#include "ir2e71y_func.h"
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/delay.h>
#include <asm/param.h>
#ifdef IR2E71Y_ALS_INT
#include <linux/input.h>
#include <linux/miscdevice.h>
#endif /* IR2E71Y_ALS_INT */
#include <linux/leds.h>
#ifdef IR2E71Y_SYSFS_LED
#include "ir2e71y_led.h"
#endif /* IR2E71Y_SYSFS_LED */
#include <soc/qcom/sh_smem.h>

static int ir2e71y_if_register_driver(void);

extern int mdss_ir2e71y_mdp_hr_video_suspend(void);
extern int mdss_ir2e71y_mdp_hr_video_resume(void);
extern int mdss_ir2e71y_set_lp00_mode(int enable);
extern int mdss_ir2e71y_fps_led_start(void);
extern void mdss_ir2e71y_fps_led_stop(void);

/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */
#define IR2E71Y_NAME "ir2e71y"

#define IR2E71Y_ALS_IRQ_REQ_BK_CTL           (0x01)
#define IR2E71Y_ALS_IRQ_REQ_DBC              (0x02)

/* ------------------------------------------------------------------------- */
/* VARIABLES                                                                 */
/* ------------------------------------------------------------------------- */
static dev_t ir2e71y_dev;
static dev_t ir2e71y_major = 0;
static dev_t ir2e71y_minor = 0;
static struct cdev ir2e71y_cdev;
static struct class *ir2e71y_class;
static struct ir2e71y_kernel_context ir2e71y_if_ctx;
static struct semaphore ir2e71y_sem;
static struct semaphore ir2e71y_sem_callback;
static struct semaphore ir2e71y_sem_irq_fac;
static struct semaphore ir2e71y_sem_timer;
static struct timer_list ir2e71y_timer;
static int ir2e71y_timer_stop = 1;
static struct work_struct ir2e71y_bkl_work;
static int ir2e71y_bkl_request_brightness;

static int ir2e71y_subscribe_type_table[NUM_IR2E71Y_IRQ_TYPE] = {
    IR2E71Y_SUBSCRIBE_TYPE_INT,
    IR2E71Y_SUBSCRIBE_TYPE_INT,
    IR2E71Y_SUBSCRIBE_TYPE_INT,
    IR2E71Y_SUBSCRIBE_TYPE_INT
};


static void (*ir2e71y_callback_table[NUM_IR2E71Y_IRQ_TYPE])(void) = {
    NULL,
    NULL,
    NULL,
    NULL
};

static spinlock_t                 ir2e71y_q_lock;
static struct ir2e71y_queue_data_t ir2e71y_queue_data;

static struct workqueue_struct    *ir2e71y_wq_gpio;
static struct work_struct         ir2e71y_wq_gpio_wk;

static struct workqueue_struct    *ir2e71y_wq_gpio_task;
static struct work_struct         ir2e71y_wq_gpio_task_wk;


static struct workqueue_struct    *ir2e71y_wq_timer_task;
static struct work_struct         ir2e71y_wq_timer_task_wk;

static int ir2e71y_smem_read_flag = 0;



static struct       wake_lock ir2e71y_wake_lock_wq;
static int          ir2e71y_wake_lock_wq_refcnt;

static spinlock_t   ir2e71y_wake_spinlock;

static struct workqueue_struct    *ir2e71y_wq_recovery;
static struct semaphore ir2e71y_sem_req_recovery_psals;
static unsigned int ir2e71y_recovery_psals_queued_flag;
static struct work_struct         ir2e71y_wq_recovery_psals_wk;

#ifdef IR2E71Y_ALS_INT
static struct wake_lock ir2e71y_timeout_wake_lock;
static struct input_dev *ir2e71y_input_dev;
static int als_int_enable = 0;
#endif /* IR2E71Y_ALS_INT */

#if defined(CONFIG_ANDROID_ENGINEERING)
static struct file_operations proc_fops;
static int ir2e71y_dbg_recovery_check_error_flag = IR2E71Y_DBG_RECOVERY_ERROR_NONE;
static int ir2e71y_dbg_recovery_check_error_count = 0;
#endif /* CONFIG_ANDROID_ENGINEERING */

struct leds_led_data {
    struct led_classdev cdev;
    int no;
    int color;
    int num_leds;
};

/* ------------------------------------------------------------------------- */
/* PROTOTYPES                                                                */
/* ------------------------------------------------------------------------- */
static void ir2e71y_init_context(void);
#ifdef IR2E71Y_NOT_SUPPORT_NO_OS
#endif  /* IR2E71Y_NOT_SUPPORT_NO_OS */

#ifdef IR2E71Y_SYSFS_LED
static int  ir2e71y_SQE_tri_led_on(int no, struct ir2e71y_tri_led *sysfs_ledx);
static int  ir2e71y_SQE_tri_led_blink_on(int no, struct ir2e71y_tri_led *sysfs_ledx);
static int  ir2e71y_SQE_tri_led_blink_store(struct leds_led_data *led_sys, unsigned int data);
static void ir2e71y_led_brightness_set(struct led_classdev *led_cdev, enum led_brightness value);
static enum led_brightness ir2e71y_led_brightness_get(struct led_classdev *led_cdev);
static ssize_t ir2e71y_led_blink_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t ir2e71y_led_blink_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t ir2e71y_led_on_off_ms_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size);
static void ir2e71y_clean_sysfs_led(void);
static void ir2e71y_clean_normal_led(void);
#endif /* IR2E71Y_SYSFS_LED */

static int ir2e71y_check_initialized(void);
static int ir2e71y_check_upper_unit(void);
static int ir2e71y_check_bdic_exist(void);
static int ir2e71y_bdic_subscribe_check(struct ir2e71y_subscribe *subscribe);
static int ir2e71y_bdic_unsubscribe_check(int irq_type);

static int ir2e71y_open(struct inode *inode, struct file *filp);
static long ir2e71y_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
static int ir2e71y_release(struct inode *inode, struct file *filp);

static int ir2e71y_ioctl_get_lux(void __user *argp);
static int ir2e71y_ioctl_photo_sensor_pow_ctl(void __user *argp);
static int ir2e71y_ioctl_get_als(void __user *argp);
#ifdef IR2E71Y_ALS_INT
static int ir2e71y_ioctl_set_alsint(void __user *argp);
static int ir2e71y_ioctl_get_alsint(void __user *argp);
static int ir2e71y_ioctl_get_light_info(void __user *argp);
#endif /* IR2E71Y_ALS_INT */
static int ir2e71y_ioctl_get_lut_info(void __user *argp);

static int ir2e71y_SQE_main_lcd_power_on(void);
static int ir2e71y_SQE_main_lcd_power_off(void);
static int ir2e71y_SQE_main_bkl_ctl(int type, struct ir2e71y_main_bkl_ctl *bkl);

static bool ir2e71y_led_set_color_reject(void);
#if defined(CONFIG_ANDROID_ENGINEERING)
static int ir2e71y_tri_led_set_color(struct ir2e71y_tri_led *tri_led);
static int ir2e71y_SQE_tri_led_set_color(struct ir2e71y_tri_led *tri_led);
static int ir2e71y_SQE_bdic_write_reg(unsigned char reg, unsigned char val);
static int ir2e71y_SQE_bdic_read_reg(unsigned char reg, unsigned char *val);
#endif /* CONFIG_ANDROID_ENGINEERING */
static int ir2e71y_SQE_get_lux(struct ir2e71y_photo_sensor_val *value);
static int ir2e71y_SQE_write_bdic_i2c(struct ir2e71y_bdic_i2c_msg *i2c_msg);
static int ir2e71y_SQE_read_bdic_i2c(struct ir2e71y_bdic_i2c_msg *i2c_msg);
static int ir2e71y_SQE_photo_sensor_pow_ctl(struct ir2e71y_photo_sensor_power_ctl *ctl);
static int ir2e71y_SQE_prox_sensor_pow_ctl(int power_mode);
static int ir2e71y_SQE_event_subscribe(struct ir2e71y_subscribe *subscribe);
static int ir2e71y_SQE_event_unsubscribe(int irq_type);
static int ir2e71y_SQE_get_als(struct ir2e71y_photo_sensor_raw_val *val);
#ifdef IR2E71Y_ALS_INT
static int ir2e71y_SQE_set_alsint(struct ir2e71y_photo_sensor_int_trigger *val);
static int ir2e71y_SQE_get_alsint(struct ir2e71y_photo_sensor_int_trigger *val);
static int ir2e71y_SQE_get_light_info(struct ir2e71y_light_info *val);
#endif /* IR2E71Y_ALS_INT */
static int ir2e71y_SQE_psals_recovery(void);

static irqreturn_t ir2e71y_gpio_int_isr(int irq_num, void *data);
static void ir2e71y_workqueue_handler_gpio(struct work_struct *work);
static void ir2e71y_workqueue_gpio_task(struct work_struct *work);
static void ir2e71y_bkl_workhandler(struct work_struct *work);
static void ir2e71y_wake_lock_init(void);
static void ir2e71y_wake_lock(void);
static void ir2e71y_wake_unlock(void);
static void ir2e71y_timer_int_isr(unsigned long data);
static void ir2e71y_timer_int_register(void);
static void ir2e71y_timer_int_delete(void);
static void ir2e71y_timer_int_mod(void);
static void ir2e71y_workqueue_timer_task(struct work_struct *work);
#ifdef IR2E71Y_ALS_INT
static int ir2e71y_do_als_int_report(int ginf2_val);
static void ir2e71y_do_als_int_report_dummy(void);
static int ir2e71y_set_als_int_subscribe(int trigger);
static int ir2e71y_set_als_int_unsubscribe(int trigger);
#endif /* IR2E71Y_ALS_INT */
static int ir2e71y_do_psals_recovery(void);
static void ir2e71y_workqueue_handler_recovery_psals(struct work_struct *work);
static void ir2e71y_psals_recovery(void);
static int ir2e71y_event_subscribe(struct ir2e71y_subscribe *subscribe);
static int ir2e71y_event_unsubscribe(int irq_type);
static void ir2e71y_semaphore_start(void);
static void ir2e71y_semaphore_end(const char *func);
#if defined(CONFIG_ANDROID_ENGINEERING)
static int ir2e71y_proc_write(struct file *file, const char *buffer, unsigned long count, void *data);
static int ir2e71y_proc_read(char *page, char **start, off_t offset, int count, int *eof, void *data);
static ssize_t ir2e71y_proc_file_read(struct file *file, char __user *buf, size_t nbytes, loff_t *ppos);
static ssize_t ir2e71y_proc_file_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos);
static void ir2e71y_dbg_info_output(int mode);
static void ir2e71y_dbg_que(int kind);
#endif /* CONFIG_ANDROID_ENGINEERING */
static void ir2e71y_fb_close(void);
#ifdef IR2E71Y_ALS_INT
static int ir2e71y_input_subsystem_init(void);
static int ir2e71y_input_subsystem_report(int val);
#endif /* IR2E71Y_ALS_INT */
static int ir2e71y_main_lcd_power_on(void);
static int ir2e71y_main_lcd_power_off(void);

static void ir2e71y_set_wled_brightness(struct led_classdev *led_cdev,
                                         enum led_brightness level);
static int ir2e71y_main_bkl_on(struct ir2e71y_main_bkl_ctl *bkl);
static int ir2e71y_main_bkl_off(void);

static struct file_operations ir2e71y_fops = {
    .owner          = THIS_MODULE,
    .open           = ir2e71y_open,
    .write          = NULL,
    .read           = NULL,
    .mmap           = NULL,
    .unlocked_ioctl = ir2e71y_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl   = ir2e71y_ioctl,
#endif
    .release        = ir2e71y_release,
};

#ifdef IR2E71Y_SYSFS_LED
static DEVICE_ATTR(blink, 0664, ir2e71y_led_blink_show, ir2e71y_led_blink_store);
static DEVICE_ATTR(on_off_ms, 0664, NULL, ir2e71y_led_on_off_ms_store);

static struct attribute *blink_attrs[] = {
    &dev_attr_blink.attr,
    &dev_attr_on_off_ms.attr,
    NULL
};

static const struct attribute_group blink_attr_group = {
    .attrs = blink_attrs,
};
#endif /* IR2E71Y_SYSFS_LED */

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
/*ir2e71y_enable_analog_power                                                */
/* ------------------------------------------------------------------------- */
int ir2e71y_enable_analog_power(int enable)
{
    int ret = 0;
    if (enable) {
        ret = ir2e71y_main_lcd_power_on();
    } else {
        ret = ir2e71y_main_lcd_power_off();
    }
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_main_lcd_power_on                                                  */
/* ------------------------------------------------------------------------- */
static int ir2e71y_main_lcd_power_on(void)
{

    int ret;

    IR2E71Y_TRACE("in");
    IR2E71Y_PERFORMANCE("RESUME PANEL POWER-ON 0010 START");

    if (ir2e71y_check_initialized() != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_DEBUG("out1");
        return IR2E71Y_RESULT_SUCCESS;
    }

    if (ir2e71y_check_bdic_exist() != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_DEBUG("out2");
        return IR2E71Y_RESULT_SUCCESS;
    }

    if (ir2e71y_check_upper_unit() != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_DEBUG("out3");
        return IR2E71Y_RESULT_SUCCESS;
    }

    ir2e71y_semaphore_start();

    ret = ir2e71y_SQE_main_lcd_power_on();

    ir2e71y_semaphore_end(__func__);

    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_SQE_main_lcd_power_on.");
        return IR2E71Y_RESULT_FAILURE;
    }

    IR2E71Y_PERFORMANCE("RESUME PANEL POWER-ON 0010 END");
    IR2E71Y_TRACE("out");
    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_main_lcd_power_off                                                 */
/* ------------------------------------------------------------------------- */
static int ir2e71y_main_lcd_power_off(void)
{
    int ret;

    IR2E71Y_TRACE("in");
    IR2E71Y_PERFORMANCE("SUSPEND PANEL POWER-OFF 0010 START");

    if (ir2e71y_check_initialized() != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_DEBUG("out1");
        return IR2E71Y_RESULT_SUCCESS;
    }

    if (ir2e71y_check_bdic_exist() != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_DEBUG("out2");
        return IR2E71Y_RESULT_SUCCESS;
    }

    if (ir2e71y_check_upper_unit() != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_DEBUG("out3");
        return IR2E71Y_RESULT_SUCCESS;
    }

    ir2e71y_semaphore_start();

    ret = ir2e71y_SQE_main_lcd_power_off();

    ir2e71y_semaphore_end(__func__);

    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_SQE_main_lcd_power_off.");
        return IR2E71Y_RESULT_FAILURE;
    }

    IR2E71Y_PERFORMANCE("SUSPEND PANEL POWER-OFF 0010 END");
    IR2E71Y_TRACE("out");
    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_set_wled_brightness                                                */
/* ------------------------------------------------------------------------- */
static void ir2e71y_set_wled_brightness(struct led_classdev *led_cdev,
                                         enum led_brightness level)
{
    IR2E71Y_TRACE("in");

    if(level < LED_OFF)
        level = LED_OFF;
    else if(level > led_cdev->max_brightness)
        level = led_cdev->max_brightness;
    ir2e71y_bkl_request_brightness = level;
    schedule_work(&ir2e71y_bkl_work);
    IR2E71Y_TRACE("out");
    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_main_bkl_on                                                        */
/* ------------------------------------------------------------------------- */
static int ir2e71y_main_bkl_on(struct ir2e71y_main_bkl_ctl *bkl)
{
    int ret;

    IR2E71Y_TRACE("in");
    IR2E71Y_PERFORMANCE("RESUME PANEL BACKLIGHT-ON 0010 START");

    if (bkl == NULL) {
        IR2E71Y_ERR("<NULL_POINTER> bkl.");
        return IR2E71Y_RESULT_FAILURE;
    }

    if (ir2e71y_check_initialized() != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_DEBUG("out1");
        return IR2E71Y_RESULT_SUCCESS;
    }

    if (ir2e71y_check_bdic_exist() != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_DEBUG("out2");
        return IR2E71Y_RESULT_SUCCESS;
    }

    if (ir2e71y_check_upper_unit() != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_DEBUG("out3");
        return IR2E71Y_RESULT_SUCCESS;
    }

    if (ir2e71y_if_ctx.shutdown_in_progress) {
        IR2E71Y_DEBUG("canceled: shutdown is in progress\n");
        return IR2E71Y_RESULT_SUCCESS;
    }

    if ((bkl->mode <= IR2E71Y_MAIN_BKL_MODE_OFF) ||
        (bkl->mode >= IR2E71Y_MAIN_BKL_MODE_DTV_OFF)) {
        IR2E71Y_ERR("<INVALID_VALUE> bkl->mode.");
        return IR2E71Y_RESULT_FAILURE;
    }

    if (bkl->mode != IR2E71Y_MAIN_BKL_MODE_FIX) {
        IR2E71Y_DEBUG("out4");
        return IR2E71Y_RESULT_SUCCESS;
    }

    if (bkl->param <= IR2E71Y_MAIN_BKL_PARAM_MIN) {
        IR2E71Y_ERR("<INVALID_VALUE> bkl->param.");
        return IR2E71Y_RESULT_FAILURE;
    }

    if (bkl->param > IR2E71Y_MAIN_BKL_PARAM_MAX) {
        bkl->param = IR2E71Y_MAIN_BKL_PARAM_MAX;
    }

    ir2e71y_semaphore_start();

    ret = ir2e71y_SQE_main_bkl_ctl(IR2E71Y_MAIN_BKL_DEV_TYPE_APP, bkl);

    ir2e71y_semaphore_end(__func__);

    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_SQE_main_bkl_ctl.");
        return IR2E71Y_RESULT_FAILURE;
    }

    IR2E71Y_PERFORMANCE("RESUME PANEL BACKLIGHT-ON 0010 END");
    IR2E71Y_TRACE("out");

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_api_main_bkl_off                                                   */
/* ------------------------------------------------------------------------- */
static int ir2e71y_main_bkl_off(void)
{
    int ret;
    struct ir2e71y_main_bkl_ctl bkl_ctl;

    IR2E71Y_TRACE("in");
    IR2E71Y_PERFORMANCE("SUSPEND PANEL BACKLIGHT-OFF 0010 START");

    if (ir2e71y_check_initialized() != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_DEBUG("out1");
        return IR2E71Y_RESULT_SUCCESS;
    }

    if (ir2e71y_check_bdic_exist() != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_DEBUG("out2");
        return IR2E71Y_RESULT_SUCCESS;
    }

    if (ir2e71y_check_upper_unit() != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_DEBUG("out3");
        return IR2E71Y_RESULT_SUCCESS;
    }

    if (ir2e71y_if_ctx.shutdown_in_progress) {
        IR2E71Y_DEBUG("canceled: bkl_off is in progress");
        return IR2E71Y_RESULT_SUCCESS;
    }

    bkl_ctl.mode  = IR2E71Y_MAIN_BKL_MODE_OFF;
    bkl_ctl.param = IR2E71Y_MAIN_BKL_PARAM_OFF;

    ir2e71y_semaphore_start();

    ret = ir2e71y_SQE_main_bkl_ctl(IR2E71Y_MAIN_BKL_DEV_TYPE_APP, &(bkl_ctl));

    ir2e71y_semaphore_end(__func__);

    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_SQE_main_bkl_ctl.");
        return IR2E71Y_RESULT_FAILURE;
    }

    IR2E71Y_PERFORMANCE("SUSPEND PANEL BACKLIGHT-OFF 0010 END");
    IR2E71Y_TRACE("out");

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_write_bdic_i2c                                                     */
/* ------------------------------------------------------------------------- */
int ir2e71y_write_bdic_i2c(struct ir2e71y_bdic_i2c_msg *i2c_msg)
{
    int ret;

    IR2E71Y_PERFORMANCE("RESUME BDIC WRITE-BDICI2C 0010 START");

    if (ir2e71y_check_upper_unit() != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_DEBUG("upper_unit nothing");
        return IR2E71Y_RESULT_SUCCESS;
    }

    if (i2c_msg == NULL) {
        IR2E71Y_ERR("<NULL_POINTER> i2c_msg.");
        return IR2E71Y_RESULT_FAILURE;
    }

    if (i2c_msg->wbuf == NULL) {
        IR2E71Y_ERR("<NULL_POINTER> i2c_msg->wbuf.");
        return IR2E71Y_RESULT_FAILURE;
    }

    if (ir2e71y_check_initialized() != IR2E71Y_RESULT_SUCCESS) {
        return IR2E71Y_RESULT_SUCCESS;
    }

    if (ir2e71y_check_bdic_exist() != IR2E71Y_RESULT_SUCCESS) {
        return IR2E71Y_RESULT_SUCCESS;
    }

    if (ir2e71y_if_ctx.shutdown_in_progress) {
        return IR2E71Y_RESULT_SUCCESS;
    }

    if (ir2e71y_pm_API_is_ps_active() != IR2E71Y_DEV_STATE_ON) {
        IR2E71Y_ERR("<RESULT_FAILURE> ps is not active.");
        return IR2E71Y_RESULT_FAILURE;
    }

    if (i2c_msg->mode != IR2E71Y_BDIC_I2C_M_W) {
        i2c_msg->mode = IR2E71Y_BDIC_I2C_M_W;
    }

    ir2e71y_semaphore_start();

    ret = ir2e71y_SQE_write_bdic_i2c(i2c_msg);

    ir2e71y_semaphore_end(__func__);

    if (ret == IR2E71Y_RESULT_FAILURE_I2C_TMO) {
        IR2E71Y_ERR("<TIME_OUT> ir2e71y_SQE_write_bdic_i2c.");
        return IR2E71Y_RESULT_FAILURE_I2C_TMO;
    } else if (ret == IR2E71Y_RESULT_FAILURE) {
        IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_SQE_write_bdic_i2c.");
        return IR2E71Y_RESULT_FAILURE;
    }

    IR2E71Y_PERFORMANCE("RESUME BDIC WRITE-BDICI2C 0010 END");

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_read_bdic_i2c                                                      */
/* ------------------------------------------------------------------------- */
int ir2e71y_read_bdic_i2c(struct ir2e71y_bdic_i2c_msg *i2c_msg)
{
    int ret;

    IR2E71Y_PERFORMANCE("RESUME BDIC READ-BDICI2C 0010 START");

    if (ir2e71y_check_upper_unit() != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_DEBUG("upper_unit nothing");
        return IR2E71Y_RESULT_SUCCESS;
    }

    if (i2c_msg == NULL) {
        IR2E71Y_ERR("<NULL_POINTER> i2c_msg.");
        return IR2E71Y_RESULT_FAILURE;
    }

    if (i2c_msg->wbuf == NULL) {
        IR2E71Y_ERR("<NULL_POINTER> i2c_msg->wbuf.");
        return IR2E71Y_RESULT_FAILURE;
    }

    if (i2c_msg->rbuf == NULL) {
        IR2E71Y_ERR("<NULL_POINTER> i2c_msg->rbuf.");
        return IR2E71Y_RESULT_FAILURE;
    }

    if (ir2e71y_check_initialized() != IR2E71Y_RESULT_SUCCESS) {
        return IR2E71Y_RESULT_SUCCESS;
    }

    if (ir2e71y_check_bdic_exist() != IR2E71Y_RESULT_SUCCESS) {
        return IR2E71Y_RESULT_SUCCESS;
    }

    if (ir2e71y_if_ctx.shutdown_in_progress) {
        return IR2E71Y_RESULT_SUCCESS;
    }

    if (ir2e71y_pm_API_is_ps_active() != IR2E71Y_DEV_STATE_ON) {
        IR2E71Y_ERR("<RESULT_FAILURE> ps is not active.");
        return IR2E71Y_RESULT_FAILURE;
    }

    if (i2c_msg->mode != IR2E71Y_BDIC_I2C_M_R) {
        i2c_msg->mode = IR2E71Y_BDIC_I2C_M_R;
    }

    ir2e71y_semaphore_start();

    ret = ir2e71y_SQE_read_bdic_i2c(i2c_msg);

    ir2e71y_semaphore_end(__func__);

    if (ret == IR2E71Y_RESULT_FAILURE_I2C_TMO) {
        IR2E71Y_ERR("<TIME_OUT> ir2e71y_SQE_read_bdic_i2c.");
        return IR2E71Y_RESULT_FAILURE_I2C_TMO;
    } else if (ret == IR2E71Y_RESULT_FAILURE) {
        IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_SQE_read_bdic_i2c.");
        return IR2E71Y_RESULT_FAILURE;
    }

    IR2E71Y_PERFORMANCE("RESUME BDIC READ-BDICI2C 0010 END");

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_prox_sensor_pow_ctl                                                */
/* ------------------------------------------------------------------------- */
int ir2e71y_prox_sensor_pow_ctl(int power_mode, struct ir2e71y_prox_params *prox_params)
{
    int ret;

    IR2E71Y_PERFORMANCE("RESUME PANEL PROXSENSOR-CTL 0010 START");

    if (ir2e71y_check_bdic_exist() != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_DEBUG("bdic is not exist");
        return IR2E71Y_RESULT_SUCCESS;
    }

    if (ir2e71y_check_upper_unit() != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_DEBUG("upper_unit nothing");
        return IR2E71Y_RESULT_SUCCESS;
    }

    if (ir2e71y_if_ctx.shutdown_in_progress) {
        IR2E71Y_DEBUG("canceled: prox_pow_ctl is in progress");
        return IR2E71Y_RESULT_SUCCESS;
    }

    if (power_mode >= NUM_IR2E71Y_PROX_SENSOR_POWER) {
        IR2E71Y_ERR("<INVALID_VALUE> power_mode(%d).", power_mode);
        return IR2E71Y_RESULT_FAILURE;
    }

    IR2E71Y_DEBUG(":power_mode=%d", power_mode);

    if (power_mode == IR2E71Y_PROX_SENSOR_POWER_ON) {
        if (prox_params == NULL) {
            IR2E71Y_ERR("<NULL_POINTER> prox_params.");
            return IR2E71Y_RESULT_FAILURE;
        }
        ir2e71y_bdic_API_set_prox_sensor_param(prox_params);
    }

    ir2e71y_semaphore_start();

    ret = ir2e71y_SQE_prox_sensor_pow_ctl(power_mode);

    ir2e71y_semaphore_end(__func__);

    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_SQE_prox_sensor_pow_ctl.");
        return IR2E71Y_RESULT_FAILURE;
    }

    IR2E71Y_PERFORMANCE("RESUME PANEL PROXSENSOR-CTL 0010 START");

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_API_get_bdic_is_exist                                              */
/* ------------------------------------------------------------------------- */
int ir2e71y_API_get_bdic_is_exist(void)
{
    return IR2E71Y_BDIC_IS_EXIST;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_API_psals_recovery_subscribe                                       */
/* ------------------------------------------------------------------------- */
void ir2e71y_API_psals_recovery_subscribe(void)
{
    struct ir2e71y_subscribe subscribe;

    IR2E71Y_TRACE("in");

    subscribe.irq_type = IR2E71Y_IRQ_TYPE_I2CERR;
    subscribe.callback = ir2e71y_psals_recovery;

    ir2e71y_event_subscribe(&subscribe);

    IR2E71Y_TRACE("out");
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_API_psals_recovery_unsubscribe                                     */
/* ------------------------------------------------------------------------- */
void ir2e71y_API_psals_recovery_unsubscribe(void)
{
    IR2E71Y_TRACE("in");

    ir2e71y_event_unsubscribe(IR2E71Y_IRQ_TYPE_I2CERR);

    IR2E71Y_TRACE("out");
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_API_check_upper_unit                                               */
/* ------------------------------------------------------------------------- */
int ir2e71y_API_check_upper_unit(void)
{
    return ir2e71y_check_upper_unit();
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_API_semaphore_start                                                */
/* ------------------------------------------------------------------------- */
void ir2e71y_API_semaphore_start(void)
{
    IR2E71Y_INFO("in");
    ir2e71y_semaphore_start();
    IR2E71Y_INFO("out");
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_API_semaphore_end                                                  */
/* ------------------------------------------------------------------------- */
void ir2e71y_API_semaphore_end(void)
{
    IR2E71Y_INFO("in");
    ir2e71y_semaphore_end(__func__);
    IR2E71Y_INFO("out");
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_subscribe_check                                               */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_subscribe_check(struct ir2e71y_subscribe *subscribe)
{


    if (ir2e71y_check_bdic_exist() != IR2E71Y_RESULT_SUCCESS) {
        return IR2E71Y_RESULT_FAILURE;
    }

    if (subscribe == NULL) {
        IR2E71Y_ERR("<NULL POINTER> INT_SUBSCRIBE subscribe");
        return IR2E71Y_RESULT_FAILURE;
    }

    if ((subscribe->irq_type < IR2E71Y_IRQ_TYPE_ALS) || (subscribe->irq_type >= NUM_IR2E71Y_IRQ_TYPE)) {
        IR2E71Y_ERR("<INVALID_VALUE> subscribe->irq_type(%d)", subscribe->irq_type);
        return IR2E71Y_RESULT_FAILURE;
    }

    if (subscribe->callback == NULL) {
        IR2E71Y_ERR("<NULL_POINTER> subscribe->callback");
        return IR2E71Y_RESULT_FAILURE;
    }

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_bdic_unsubscribe_check                                             */
/* ------------------------------------------------------------------------- */
static int ir2e71y_bdic_unsubscribe_check(int irq_type)
{
    if (ir2e71y_check_initialized() != IR2E71Y_RESULT_SUCCESS) {
        return IR2E71Y_RESULT_FAILURE;
    }

    if (ir2e71y_check_bdic_exist() != IR2E71Y_RESULT_SUCCESS) {
        return IR2E71Y_RESULT_FAILURE;
    }

    if ((irq_type < IR2E71Y_IRQ_TYPE_ALS) || (irq_type >= NUM_IR2E71Y_IRQ_TYPE)) {
        IR2E71Y_ERR("<INVALID_VALUE> irq_type(%d)", irq_type);
        return IR2E71Y_RESULT_FAILURE;
    }
    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* INITIALIZE                                                                */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/*ir2e71y_init_context                                                       */
/* ------------------------------------------------------------------------- */
static void ir2e71y_init_context(void)
{
#ifndef IR2E71Y_NOT_SUPPORT_NO_OS
    sharp_smem_common_type *sh_smem_common_adr = NULL;
#endif  /* IR2E71Y_NOT_SUPPORT_NO_OS */
    if (ir2e71y_smem_read_flag != 0) {
        return;
    }

    memset(&ir2e71y_if_ctx, 0, sizeof(ir2e71y_if_ctx));

    ir2e71y_if_ctx.driver_is_open              = false;
    ir2e71y_if_ctx.driver_open_cnt             = 0;
    ir2e71y_if_ctx.driver_is_initialized       = IR2E71Y_DRIVER_IS_NOT_INITIALIZED;
    ir2e71y_if_ctx.shutdown_in_progress        = false;
    ir2e71y_if_ctx.thermal_status              = IR2E71Y_MAIN_BKL_EMG_OFF;
    ir2e71y_if_ctx.usb_chg_status              = IR2E71Y_MAIN_BKL_CHG_OFF;

#ifdef IR2E71Y_SYSFS_LED
    ir2e71y_if_ctx.sysfs_led1.red              =0;
    ir2e71y_if_ctx.sysfs_led1.green            =0;
    ir2e71y_if_ctx.sysfs_led1.blue             =0;
    ir2e71y_if_ctx.sysfs_led1.ext_mode         =0;
    ir2e71y_if_ctx.sysfs_led1.led_mode         =IR2E71Y_TRI_LED_MODE_OFF;
    ir2e71y_if_ctx.sysfs_led1.ontime           =0;
    ir2e71y_if_ctx.sysfs_led1.interval         =0;
    ir2e71y_if_ctx.sysfs_led1.count            =0;
#ifdef IR2E71Y_COLOR_LED_TWIN
    ir2e71y_if_ctx.sysfs_led2.red              =0;
    ir2e71y_if_ctx.sysfs_led2.green            =0;
    ir2e71y_if_ctx.sysfs_led2.blue             =0;
    ir2e71y_if_ctx.sysfs_led2.ext_mode         =0;
    ir2e71y_if_ctx.sysfs_led2.led_mode         =IR2E71Y_TRI_LED_MODE_OFF;
    ir2e71y_if_ctx.sysfs_led2.ontime           =0;
    ir2e71y_if_ctx.sysfs_led2.interval         =0;
    ir2e71y_if_ctx.sysfs_led2.count            =0;
#endif /* IR2E71Y_COLOR_LED_TWIN */
#endif /* IR2E71Y_SYSFS_LED */

#ifndef IR2E71Y_NOT_SUPPORT_NO_OS
    sh_smem_common_adr = (sharp_smem_common_type *)sh_smem_get_common_address();
    if (sh_smem_common_adr != NULL) {
        memcpy(&(ir2e71y_if_ctx.boot_ctx), &sh_smem_common_adr->shdisp_data_buf, sizeof(struct ir2e71y_boot_context));
        ir2e71y_if_ctx.bdic_is_exist = ir2e71y_if_ctx.boot_ctx.bdic_is_exist;
        ir2e71y_if_ctx.bdic_chipver = ir2e71y_if_ctx.boot_ctx.bdic_chipver;

        ir2e71y_if_ctx.main_bkl.mode = ir2e71y_if_ctx.boot_ctx.main_bkl.mode;
        ir2e71y_if_ctx.main_bkl.param = ir2e71y_if_ctx.boot_ctx.main_bkl.param;

        ir2e71y_if_ctx.tri_led.red = ir2e71y_if_ctx.boot_ctx.tri_led.red;
        ir2e71y_if_ctx.tri_led.green = ir2e71y_if_ctx.boot_ctx.tri_led.green;
        ir2e71y_if_ctx.tri_led.blue = ir2e71y_if_ctx.boot_ctx.tri_led.blue;
        ir2e71y_if_ctx.tri_led.ext_mode = ir2e71y_if_ctx.boot_ctx.tri_led.ext_mode;
        ir2e71y_if_ctx.tri_led.led_mode = ir2e71y_if_ctx.boot_ctx.tri_led.led_mode;
        ir2e71y_if_ctx.tri_led.ontime = ir2e71y_if_ctx.boot_ctx.tri_led.ontime;
        ir2e71y_if_ctx.tri_led.interval = ir2e71y_if_ctx.boot_ctx.tri_led.interval;
        ir2e71y_if_ctx.tri_led.count = ir2e71y_if_ctx.boot_ctx.tri_led.count;
        ir2e71y_smem_read_flag = 1;
    }
#endif  /* IR2E71Y_NOT_SUPPORT_NO_OS */

#if defined(CONFIG_ANDROID_ENGINEERING)
#ifdef IR2E71Y_LOG_ENABLE   /* for debug */
    ir2e71y_dbg_info_output(IR2E71Y_DEBUG_INFO_TYPE_POWERON);
#endif  /* IR2E71Y_LOG_ENABLE */
#endif  /* CONFIG_ANDROID_ENGINEERING */
    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_get_common_address                                                 */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* CHECKER                                                                   */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/*ir2e71y_check_initialized                                                  */
/* ------------------------------------------------------------------------- */
static int ir2e71y_check_initialized(void)
{
    if (ir2e71y_if_ctx.driver_is_initialized == IR2E71Y_DRIVER_IS_INITIALIZED) {
        return IR2E71Y_RESULT_SUCCESS;
    } else {
        IR2E71Y_DEBUG("[IR2E71Y] ir2e71y_check_initialized error : driver is not initialized.");
        return IR2E71Y_RESULT_FAILURE;
    }
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_check_upper_unit                                                   */
/* ------------------------------------------------------------------------- */
static int ir2e71y_check_upper_unit(void)
{
    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_check_bdic_exist                                                   */
/* ------------------------------------------------------------------------- */
static int ir2e71y_check_bdic_exist(void)
{
    if (ir2e71y_if_ctx.bdic_is_exist  == IR2E71Y_BDIC_IS_EXIST) {
        return IR2E71Y_RESULT_SUCCESS;
    } else {
        IR2E71Y_ERR("[IR2E71Y] ir2e71y_check_bdic_exist error : bdic is not exist.");
        return IR2E71Y_RESULT_FAILURE;
    }
}

/* ------------------------------------------------------------------------- */
/* FOPS                                                                      */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/*ir2e71y_open                                                               */
/* ------------------------------------------------------------------------- */
static int ir2e71y_open(struct inode *inode, struct file *filp)
{

    if (ir2e71y_if_ctx.driver_open_cnt == 0) {
        IR2E71Y_DEBUG("[IR2E71Y] new open ir2e71y device driver.");
    }

    ir2e71y_if_ctx.driver_open_cnt++;

    if (ir2e71y_if_ctx.driver_open_cnt == 1 && !ir2e71y_if_ctx.driver_is_open) {
        ir2e71y_if_ctx.driver_is_open = true;
    }

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_ioctl                                                              */
/* ------------------------------------------------------------------------- */
static long ir2e71y_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    int ret;
    void __user *argp = (void __user*)arg;

    IR2E71Y_INFO("ir2e71y ioctl. cmd=%08X", cmd);

    switch (cmd) {
    case IR2E71Y_IOCTL_GET_LUX:
        IR2E71Y_PERFORMANCE("RESUME PANEL GET-LUX 0010 START");
        ret = ir2e71y_ioctl_get_lux(argp);
        IR2E71Y_PERFORMANCE("RESUME PANEL GET-LUX 0010 END");
        break;
    case IR2E71Y_IOCTL_PHOTO_SENSOR_POW_CTL:
        IR2E71Y_PERFORMANCE("RESUME PANEL PHOTO-SENCOR 0010 START");
        ret = ir2e71y_ioctl_photo_sensor_pow_ctl(argp);
        IR2E71Y_PERFORMANCE("RESUME PANEL PHOTO-SENCOR 0010 END");
        break;
    case IR2E71Y_IOCTL_GET_ALS:
        IR2E71Y_TRACE("IR2E71Y_IOCTL_GET_ALS Requested. cmd=%08X", cmd);
        ret = ir2e71y_ioctl_get_als(argp);
        IR2E71Y_TRACE("IR2E71Y_IOCTL_GET_ALS Completed ret:%d", ret);
        break;
#ifdef IR2E71Y_ALS_INT
    case IR2E71Y_IOCTL_SET_ALSINT:
        ret = ir2e71y_ioctl_set_alsint(argp);
        break;
    case IR2E71Y_IOCTL_GET_ALSINT:
        ret = ir2e71y_ioctl_get_alsint(argp);
        break;
    case IR2E71Y_IOCTL_GET_LIGHT_INFO:
        ret = ir2e71y_ioctl_get_light_info(argp);
        break;
#endif /* IR2E71Y_ALS_INT */
    case IR2E71Y_IOCTL_GET_LUT_INFO:
        ret = ir2e71y_ioctl_get_lut_info(argp);
        break;

    default:
        IR2E71Y_ERR("<INVALID_VALUE> cmd(0x%08x).", cmd);
        ret = -EFAULT;
        break;
    }

    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_release                                                            */
/* ------------------------------------------------------------------------- */
static int ir2e71y_release(struct inode *inode, struct file *filp)
{
    if (ir2e71y_if_ctx.driver_open_cnt > 0) {
        ir2e71y_if_ctx.driver_open_cnt--;

        if (ir2e71y_if_ctx.driver_open_cnt == 0) {
            IR2E71Y_DEBUG("[IR2E71Y] all close ir2e71y device driver.");
        }
    }

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* IOCTL                                                                     */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/*ir2e71y_ioctl_get_lux                                                      */
/* ------------------------------------------------------------------------- */
static int ir2e71y_ioctl_get_lux(void __user *argp)
{
    int ret;
    struct ir2e71y_photo_sensor_val val;

    if (ir2e71y_check_bdic_exist() != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_DEBUG("bdic is not exist");
        return IR2E71Y_RESULT_SUCCESS;
    }

    if (ir2e71y_check_upper_unit() != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_DEBUG("upper_unit nothing");
        return IR2E71Y_RESULT_SUCCESS;
    }

    if (ir2e71y_if_ctx.shutdown_in_progress) {
        IR2E71Y_DEBUG("canceled: get_lux is in progress");
        return IR2E71Y_RESULT_SUCCESS;
    }

    ir2e71y_semaphore_start();

    ret = copy_from_user(&val, argp, sizeof(struct ir2e71y_photo_sensor_val));

    if (ret != 0) {
        IR2E71Y_ERR("<RESULT_FAILURE> copy_from_user.");
        ir2e71y_semaphore_end(__func__);
        return ret;
    }

    ret = ir2e71y_SQE_get_lux(&(val));

    IR2E71Y_DEBUG(" value=0x%04X, lux=%u, mode=%d", val.value, val.lux, val.mode);

    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_SQE_get_lux.");
    }

    ret = copy_to_user(argp, &val, sizeof(struct ir2e71y_photo_sensor_val));

    ir2e71y_semaphore_end(__func__);

    if (ret != 0) {
        IR2E71Y_ERR("<RESULT_FAILURE> copy_to_user.");
        return ret;
    }

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_ioctl_photo_sensor_pow_ctl                                         */
/* ------------------------------------------------------------------------- */
static int ir2e71y_ioctl_photo_sensor_pow_ctl(void __user *argp)
{
    int ret;
    struct ir2e71y_photo_sensor_power_ctl power_ctl;

    if (ir2e71y_check_bdic_exist() != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_DEBUG("bdic is not exist");
        return IR2E71Y_RESULT_SUCCESS;
    }

    if (ir2e71y_check_upper_unit() != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_DEBUG("upper_unit nothing");
        return IR2E71Y_RESULT_SUCCESS;
    }

    if (ir2e71y_if_ctx.shutdown_in_progress) {
        IR2E71Y_DEBUG("canceled: photo_pow_ctl is in progress");
        return IR2E71Y_RESULT_SUCCESS;
    }

    ir2e71y_semaphore_start();

    ret = copy_from_user(&power_ctl, argp, sizeof(struct ir2e71y_photo_sensor_power_ctl));

    if (ret != 0) {
        IR2E71Y_ERR("<RESULT_FAILURE> copy_from_user.");
        ir2e71y_semaphore_end(__func__);
        return ret;
    }

    ret = ir2e71y_SQE_photo_sensor_pow_ctl(&power_ctl);

    ir2e71y_semaphore_end(__func__);

    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_SQE_photo_sensor_pow_ctl.");
        return -EIO;
    }

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_ioctl_get_als                                                      */
/* ------------------------------------------------------------------------- */
static int ir2e71y_ioctl_get_als(void __user *argp)
{
    int ret;
    struct ir2e71y_photo_sensor_raw_val val;

    if (ir2e71y_check_initialized() != IR2E71Y_RESULT_SUCCESS) {
        return -EIO;
    }

    if (ir2e71y_check_bdic_exist() != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_DEBUG("bdic is not exist");
        return IR2E71Y_RESULT_SUCCESS;
    }

    if (ir2e71y_check_upper_unit() != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_DEBUG("upper_unit nothing");
        return IR2E71Y_RESULT_SUCCESS;
    }

    if (ir2e71y_if_ctx.shutdown_in_progress) {
        IR2E71Y_DEBUG("canceled: get_als is in progress");
        return IR2E71Y_RESULT_SUCCESS;
    }

    ir2e71y_semaphore_start();

    ret = copy_from_user(&val, argp, sizeof(struct ir2e71y_photo_sensor_raw_val));
    if (ret != 0) {
        IR2E71Y_ERR("<RESULT_FAILURE> copy_from_user.");
        ir2e71y_semaphore_end(__func__);
        return ret;
    }

    ret = ir2e71y_SQE_get_als(&val);
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_SQE_get_als.");
    }

    ret = copy_to_user(argp, &val, sizeof(struct ir2e71y_photo_sensor_raw_val));

    ir2e71y_semaphore_end(__func__);

    if (ret != 0) {
        IR2E71Y_ERR("<RESULT_FAILURE> copy_to_user.");
        return ret;
    }

    return IR2E71Y_RESULT_SUCCESS;
}

#ifdef IR2E71Y_ALS_INT
/* ------------------------------------------------------------------------- */
/*ir2e71y_ioctl_set_alsint                                                   */
/* ------------------------------------------------------------------------- */
static int ir2e71y_ioctl_set_alsint(void __user *argp)
{
    int ret;
    struct ir2e71y_photo_sensor_int_trigger val;

    if (ir2e71y_check_bdic_exist() != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_DEBUG("bdic is not exist");
        return IR2E71Y_RESULT_SUCCESS;
    }

    if (ir2e71y_if_ctx.shutdown_in_progress) {
        IR2E71Y_DEBUG("canceled: set_alsint is in progress");
        return IR2E71Y_RESULT_SUCCESS;
    }

    ir2e71y_semaphore_start();
    ret = copy_from_user(&val, argp, sizeof(struct ir2e71y_photo_sensor_int_trigger));
    if (ret != 0) {
        IR2E71Y_ERR("<RESULT_FAILURE> copy_from_user.");
        ir2e71y_semaphore_end(__func__);
        return ret;
    }

    ret = ir2e71y_SQE_set_alsint(&val);
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_SQE_set_alsint.");
    }

    ret = copy_to_user(argp, &val, sizeof(struct ir2e71y_photo_sensor_int_trigger));
    ir2e71y_semaphore_end(__func__);

    if (ret != 0) {
        IR2E71Y_ERR("<RESULT_FAILURE> copy_to_user.");
        return ret;
    }
    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_ioctl_get_alsint                                                   */
/* ------------------------------------------------------------------------- */
static int ir2e71y_ioctl_get_alsint(void __user *argp)
{
    int ret;
    struct ir2e71y_photo_sensor_int_trigger val;

    if (ir2e71y_check_bdic_exist() != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_DEBUG("bdic is not exist");
        return IR2E71Y_RESULT_SUCCESS;
    }

    if (ir2e71y_if_ctx.shutdown_in_progress) {
        IR2E71Y_DEBUG("canceled: get_alsint is in progress");
        return IR2E71Y_RESULT_SUCCESS;
    }

    ir2e71y_semaphore_start();
    ret = copy_from_user(&val, argp, sizeof(struct ir2e71y_photo_sensor_int_trigger));
    if (ret != 0) {
        IR2E71Y_ERR("<RESULT_FAILURE> copy_from_user.");
        ir2e71y_semaphore_end(__func__);
        return ret;
    }

    ret = ir2e71y_SQE_get_alsint(&val);
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_SQE_get_alsint.");
    }

    ret = copy_to_user(argp, &val, sizeof(struct ir2e71y_photo_sensor_int_trigger));
    ir2e71y_semaphore_end(__func__);

    if (ret != 0) {
        IR2E71Y_ERR("<RESULT_FAILURE> copy_to_user.");
        return ret;
    }
    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_ioctl_get_light_info                                               */
/* ------------------------------------------------------------------------- */
static int ir2e71y_ioctl_get_light_info(void __user *argp)
{
    int ret;
    struct ir2e71y_light_info val;

    if (ir2e71y_check_bdic_exist() != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_DEBUG("bdic is not exist");
        return IR2E71Y_RESULT_SUCCESS;
    }

    if (ir2e71y_if_ctx.shutdown_in_progress) {
        IR2E71Y_DEBUG("canceled: get_light_info is in progress");
        return IR2E71Y_RESULT_SUCCESS;
    }

    ir2e71y_semaphore_start();
    ret = ir2e71y_SQE_get_light_info(&val);
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_SQE_get_light_info.");
    }

    ret = copy_to_user(argp, &val, sizeof(struct ir2e71y_light_info));
    ir2e71y_semaphore_end(__func__);

    if (ret != 0) {
        IR2E71Y_ERR("<RESULT_FAILURE> copy_to_user.");
        return ret;
    }
    return IR2E71Y_RESULT_SUCCESS;
}
#endif /* IR2E71Y_ALS_INT */

/* ------------------------------------------------------------------------- */
/* ir2e71y_ioctl_get_lut_info                                                */
/* ------------------------------------------------------------------------- */
static int ir2e71y_ioctl_get_lut_info(void __user *argp)
{
    int ret = IR2E71Y_RESULT_FAILURE;
    struct ir2e71y_lut_info lut_info;
    sharp_smem_common_type *sh_smem = NULL;
    struct ir2e71y_boot_context *boot_ctx;

    memset(&lut_info, 0x00, sizeof(lut_info));
    sh_smem = (sharp_smem_common_type *)sh_smem_get_common_address();

    if (sh_smem == NULL) {
        return ret;
    }
    boot_ctx = (struct ir2e71y_boot_context *)sh_smem->shdisp_data_buf;

    lut_info.lut_status = boot_ctx->lut_status;
    memcpy(&lut_info.lut, &boot_ctx->lut, sizeof(lut_info.lut));

    ret = copy_to_user(argp, &lut_info, sizeof(struct ir2e71y_lut_info));
    if (ret != 0) {
        return ret;
    }

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* SEQUENCE                                                                  */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/*ir2e71y_SQE_main_lcd_power_on                                              */
/* ------------------------------------------------------------------------- */
static int ir2e71y_SQE_main_lcd_power_on(void)
{

    IR2E71Y_TRACE("in");

    (void)ir2e71y_pm_API_bdic_power_manager(IR2E71Y_DEV_TYPE_LCD, IR2E71Y_DEV_REQ_ON);
    ir2e71y_bdic_API_LCD_power_on();
    ir2e71y_bdic_API_LCD_m_power_on();

    IR2E71Y_TRACE("out");
    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_SQE_main_lcd_power_off                                             */
/* ------------------------------------------------------------------------- */
static int ir2e71y_SQE_main_lcd_power_off(void)
{
    IR2E71Y_TRACE("in");

    ir2e71y_bdic_API_LCD_m_power_off();
    ir2e71y_bdic_API_LCD_power_off();

    (void)ir2e71y_pm_API_bdic_power_manager(IR2E71Y_DEV_TYPE_LCD, IR2E71Y_DEV_REQ_OFF);

    IR2E71Y_TRACE("out");
    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_SQE_main_bkl_ctl                                                   */
/* ------------------------------------------------------------------------- */
static int ir2e71y_SQE_main_bkl_ctl(int type, struct ir2e71y_main_bkl_ctl *bkl)
{
    struct ir2e71y_main_bkl_ctl temp, request;

    IR2E71Y_TRACE("in");
    if (type >= NUM_IR2E71Y_MAIN_BKL_DEV_TYPE) {
        IR2E71Y_ERR("<INVALID_VALUE> type(%d).", type);
        return IR2E71Y_RESULT_FAILURE;
    }

    temp.mode  = bkl->mode;
    temp.param = bkl->param;
    ir2e71y_bdic_API_LCD_BKL_get_request(type, &temp, &request);

    if ((request.mode == ir2e71y_if_ctx.main_bkl.mode) && (request.param == ir2e71y_if_ctx.main_bkl.param)) {
        return IR2E71Y_RESULT_SUCCESS;
    }

    switch (request.mode) {
    case IR2E71Y_MAIN_BKL_MODE_OFF:
        ir2e71y_bdic_API_LCD_BKL_off();
        break;
    case IR2E71Y_MAIN_BKL_MODE_FIX:
        ir2e71y_bdic_API_LCD_BKL_fix_on(request.param);
        break;
    case IR2E71Y_MAIN_BKL_MODE_AUTO:
        ir2e71y_bdic_API_LCD_BKL_auto_on(request.param);
        break;
    default:
        break;
    }

    ir2e71y_if_ctx.main_bkl.mode  = request.mode;
    ir2e71y_if_ctx.main_bkl.param = request.param;

    IR2E71Y_TRACE("out");
    return IR2E71Y_RESULT_SUCCESS;
}

#ifdef IR2E71Y_SYSFS_LED
/* ------------------------------------------------------------------------- */
/*ir2e71y_SQE_tri_led_on                                                     */
/* ------------------------------------------------------------------------- */
static int ir2e71y_SQE_tri_led_on(int no, struct ir2e71y_tri_led *led)
{
    struct ir2e71y_tri_led sys_led;
    int ret;

    if (ir2e71y_led_set_color_reject()) {
        IR2E71Y_DEBUG("reject request.");
        return IR2E71Y_RESULT_SUCCESS;
    }

    memset(&sys_led, 0x00, sizeof(sys_led));
    switch (no) {
    case SYSFS_LED_SH_LED_1:
        sys_led = ir2e71y_if_ctx.sysfs_led1;
        break;
#ifdef IR2E71Y_COLOR_LED_TWIN
    case SYSFS_LED_SH_LED_2:
        sys_led = ir2e71y_if_ctx.sysfs_led2;
        break;
#endif /* IR2E71Y_COLOR_LED_TWIN */
    default:
        break;
    }

    if ((sys_led.red   == led->red) &&
        (sys_led.green == led->green) &&
        (sys_led.blue  == led->blue)) {
        IR2E71Y_DEBUG("same leds request.");
        return IR2E71Y_RESULT_SUCCESS;
    }

    if ((led->red   == 0) &&
        (led->green == 0) &&
        (led->blue  == 0)) {
        IR2E71Y_DEBUG("leds off. no=%d", no);
        led->led_mode = IR2E71Y_TRI_LED_MODE_OFF;
        ret = ir2e71y_bdic_API_LED_off(no);
    } else {
        led->led_mode = IR2E71Y_TRI_LED_MODE_NORMAL;

        IR2E71Y_DEBUG("leds on. no=%d, red:%d, green:%d, blue:%d",
                      no, led->red, led->green, led->blue);
        ret = ir2e71y_bdic_API_LED_on(no, *led);
        ir2e71y_clean_normal_led();
    }

    switch (no) {
    case SYSFS_LED_SH_LED_1:
        ir2e71y_if_ctx.sysfs_led1 = *led;
        break;
#ifdef IR2E71Y_COLOR_LED_TWIN
    case SYSFS_LED_SH_LED_2:
        ir2e71y_if_ctx.sysfs_led2 = *led;
        break;
#endif /* IR2E71Y_COLOR_LED_TWIN */
    default:
        break;
    }

    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_SQE_tri_led_blink_on                                               */
/* ------------------------------------------------------------------------- */
static int ir2e71y_SQE_tri_led_blink_on(int no, struct ir2e71y_tri_led *led)
{
    struct ir2e71y_tri_led led_on;
    unsigned char color = 0;
    int ret;

    if (ir2e71y_led_set_color_reject()) {
        IR2E71Y_DEBUG("reject request.");
        return IR2E71Y_RESULT_SUCCESS;
    }


    color = ir2e71y_bdic_API_TRI_LED_get_color_index_and_reedit(led);
    if (color == 0) {
        IR2E71Y_DEBUG("leds off. no=%d", no);
        led->led_mode = IR2E71Y_TRI_LED_MODE_OFF;
        ret = ir2e71y_bdic_API_LED_off(no);
    } else {
        IR2E71Y_DEBUG("leds blink on. no=%d, red:%d, green:%d, blue:%d",
                      no, led->red, led->green, led->blue);
        led->led_mode = IR2E71Y_TRI_LED_MODE_BLINK;
        memcpy(&led_on, led, sizeof(led_on));
        led_on.ontime   = IR2E71Y_TRI_LED_ONTIME_TYPE1;
        led_on.interval = IR2E71Y_TRI_LED_INTERVAL_5S;
        led_on.count    = 0;
        ret = ir2e71y_bdic_API_LED_blink_on(no, color, led_on);
        ir2e71y_clean_normal_led();
    }

    switch (no) {
    case SYSFS_LED_SH_LED_1:
        ir2e71y_if_ctx.sysfs_led1 = *led;
        break;
#ifdef IR2E71Y_COLOR_LED_TWIN
    case SYSFS_LED_SH_LED_2:
        ir2e71y_if_ctx.sysfs_led2 = *led;
        break;
#endif /* IR2E71Y_COLOR_LED_TWIN */
    default:
        break;
    }
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_SQE_tri_led_blink_store                                            */
/* ------------------------------------------------------------------------- */
static int ir2e71y_SQE_tri_led_blink_store(struct leds_led_data *led_sys, unsigned int data)
{
    struct ir2e71y_tri_led led;

    memset(&led, 0x00, sizeof(led));

    if (ir2e71y_check_initialized() != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_DEBUG("out1");
        return IR2E71Y_RESULT_FAILURE;
    }

    if (ir2e71y_check_bdic_exist() != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_DEBUG("out2");
        return IR2E71Y_RESULT_FAILURE;
    }

    IR2E71Y_DEBUG("before mode:%d", ir2e71y_if_ctx.sysfs_led1.led_mode);
    switch (ir2e71y_if_ctx.sysfs_led1.led_mode) {
    case IR2E71Y_TRI_LED_MODE_OFF:
        break;

    case IR2E71Y_TRI_LED_MODE_BLINK:
        switch (led_sys->no) {
        case SYSFS_LED_SH_LED_1:
            led.red   = ir2e71y_if_ctx.sysfs_led1.red;
            led.green = ir2e71y_if_ctx.sysfs_led1.green;
            led.blue  = ir2e71y_if_ctx.sysfs_led1.blue;
            break;
#ifdef IR2E71Y_COLOR_LED_TWIN
        case SYSFS_LED_SH_LED_2:
            led.red   = ir2e71y_if_ctx.sysfs_led2.red;
            led.green = ir2e71y_if_ctx.sysfs_led2.green;
            led.blue  = ir2e71y_if_ctx.sysfs_led2.blue;
            break;
#endif /* IR2E71Y_COLOR_LED_TWIN */
        default:
            IR2E71Y_ERR("invalid id. id no:%d", led_sys->no);
            return IR2E71Y_RESULT_FAILURE;
        }
        break;

    case IR2E71Y_TRI_LED_MODE_NORMAL:
    default:
        ir2e71y_clean_sysfs_led();
        if (data == 0) {
            IR2E71Y_DEBUG("leds off. no=%d", led_sys->no);
            ir2e71y_semaphore_start();
            ir2e71y_bdic_API_TRI_LED_off();
            ir2e71y_semaphore_end(__func__);
            return IR2E71Y_RESULT_SUCCESS;
        }
        break;
    }

    switch (led_sys->color) {
    case SYSFS_LED_SH_RED:
        led.red   = (unsigned int)data;
        break;
    case SYSFS_LED_SH_GREEN:
        led.green = (unsigned int)data;
        break;
    case SYSFS_LED_SH_BLUE:
        led.blue  = (unsigned int)data;
        break;
    default:
        IR2E71Y_ERR("invalid color. color no:%d", led_sys->color);
        return IR2E71Y_RESULT_FAILURE;
    }

    IR2E71Y_DEBUG("%s: name:%s(id:%d, color:%d) value:%d\n", __func__, led_sys->cdev.name, led_sys->no, led_sys->color, (int)data);

    ir2e71y_semaphore_start();

    ir2e71y_SQE_tri_led_blink_on(led_sys->no, &led);

    ir2e71y_semaphore_end(__func__);
    return IR2E71Y_RESULT_SUCCESS;
}
#endif /* IR2E71Y_SYSFS_LED */

/* ------------------------------------------------------------------------- */
/*ir2e71y_led_set_color_reject                                               */
/* ------------------------------------------------------------------------- */
static bool ir2e71y_led_set_color_reject(void)
{
    bool ret = false;
    if (ir2e71y_if_ctx.led_set_color_reject) {
        ret = true;
    }
    return ret;
}

#if defined(CONFIG_ANDROID_ENGINEERING)
/* ------------------------------------------------------------------------- */
/*ir2e71y_tri_led_set_color                                                  */
/* ------------------------------------------------------------------------- */
static int ir2e71y_tri_led_set_color(struct ir2e71y_tri_led *tri_led)
{

    unsigned char color, xstb_ch012;
    struct ir2e71y_tri_led  led;

    led.red   = tri_led->red;
    led.green = tri_led->green;
    led.blue  = tri_led->blue;
    color = ir2e71y_bdic_API_TRI_LED_get_color_index_and_reedit(&led);

    if (tri_led->led_mode == IR2E71Y_TRI_LED_MODE_NORMAL) {
        if ((ir2e71y_if_ctx.tri_led.red      == led.red) &&
            (ir2e71y_if_ctx.tri_led.green    == led.green) &&
            (ir2e71y_if_ctx.tri_led.blue     == led.blue) &&
            (ir2e71y_if_ctx.tri_led.led_mode == tri_led->led_mode)) {
            return IR2E71Y_RESULT_SUCCESS;
        }
    }

    xstb_ch012 = (color == 0) ? 0 : 1;

    if (xstb_ch012 != 0) {
        IR2E71Y_DEBUG("led_mode=%d color:%d, ontime:%d, interval:%d, count:%d",
                      tri_led->led_mode, color, tri_led->ontime, tri_led->interval, tri_led->count);

        switch (tri_led->led_mode) {
        case IR2E71Y_TRI_LED_MODE_NORMAL:
            ir2e71y_bdic_API_TRI_LED_normal_on(color);
            break;
        case IR2E71Y_TRI_LED_MODE_BLINK:
            ir2e71y_bdic_API_TRI_LED_blink_on(color, tri_led->ontime, tri_led->interval, tri_led->count);
            break;
        case IR2E71Y_TRI_LED_MODE_FIREFLY:
            ir2e71y_bdic_API_TRI_LED_firefly_on(color, tri_led->ontime, tri_led->interval, tri_led->count);
            break;
#ifdef IR2E71Y_ANIME_COLOR_LED
#ifdef IR2E71Y_ILLUMI_COLOR_LED
        case IR2E71Y_TRI_LED_MODE_HISPEED:
            ir2e71y_bdic_API_TRI_LED_high_speed_on(color, tri_led->interval, tri_led->count);
            break;
        case IR2E71Y_TRI_LED_MODE_STANDARD:
            ir2e71y_bdic_API_TRI_LED_standard_on(color, tri_led->interval, tri_led->count);
            break;
        case IR2E71Y_TRI_LED_MODE_BREATH:
            ir2e71y_bdic_API_TRI_LED_breath_on(color, tri_led->interval, tri_led->count);
            break;
        case IR2E71Y_TRI_LED_MODE_LONG_BREATH:
            ir2e71y_bdic_API_TRI_LED_long_breath_on(color, tri_led->interval, tri_led->count);
            break;
        case IR2E71Y_TRI_LED_MODE_WAVE:
            ir2e71y_bdic_API_TRI_LED_wave_on(color, tri_led->interval, tri_led->count);
            break;
        case IR2E71Y_TRI_LED_MODE_FLASH:
            ir2e71y_bdic_API_TRI_LED_flash_on(color, tri_led->interval, tri_led->count);
            break;
        case IR2E71Y_TRI_LED_MODE_AURORA:
            ir2e71y_bdic_API_TRI_LED_aurora_on(tri_led->interval, tri_led->count);
            break;
        case IR2E71Y_TRI_LED_MODE_RAINBOW:
            ir2e71y_bdic_API_TRI_LED_rainbow_on(tri_led->interval, tri_led->count);
            break;
#endif  /* IR2E71Y_ILLUMI_COLOR_LED */
        case IR2E71Y_TRI_LED_MODE_EMOPATTERN:
            ir2e71y_bdic_API_TRI_LED_emopattern_on(tri_led->interval, tri_led->count);
            break;
#endif  /* IR2E71Y_ANIME_COLOR_LED */
        default:
            IR2E71Y_ERR("led_mode=%d not supported.", tri_led->led_mode);
            break;
        }
#ifdef IR2E71Y_SYSFS_LED
        ir2e71y_clean_sysfs_led();
#endif /* IR2E71Y_SYSFS_LED */
    } else {
        ir2e71y_bdic_API_TRI_LED_off();
    }

    ir2e71y_if_ctx.tri_led.red      = led.red;
    ir2e71y_if_ctx.tri_led.green    = led.green;
    ir2e71y_if_ctx.tri_led.blue     = led.blue;
    ir2e71y_if_ctx.tri_led.led_mode = tri_led->led_mode;
    switch (tri_led->led_mode) {
    case IR2E71Y_TRI_LED_MODE_NORMAL:
        break;
    case IR2E71Y_TRI_LED_MODE_BLINK:
    case IR2E71Y_TRI_LED_MODE_FIREFLY:
        ir2e71y_if_ctx.tri_led.ontime   = tri_led->ontime;
        ir2e71y_if_ctx.tri_led.interval = tri_led->interval;
        ir2e71y_if_ctx.tri_led.count    = tri_led->count;
        break;
#ifdef IR2E71Y_ANIME_COLOR_LED
    case IR2E71Y_TRI_LED_MODE_EMOPATTERN:
        ir2e71y_if_ctx.tri_led.red = 1;
        ir2e71y_if_ctx.tri_led.green = 0;
        ir2e71y_if_ctx.tri_led.blue = 1;
        ir2e71y_if_ctx.tri_led.interval = 0;
        ir2e71y_if_ctx.tri_led.count = 0;
        break;
#endif  /* IR2E71Y_ANIME_COLOR_LED */
    default:
        ir2e71y_if_ctx.tri_led.interval = tri_led->interval;
        ir2e71y_if_ctx.tri_led.count    = tri_led->count;
        break;
    }
    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_SQE_tri_led_set_color                                              */
/* ------------------------------------------------------------------------- */
static int ir2e71y_SQE_tri_led_set_color(struct ir2e71y_tri_led *tri_led)
{
    if (ir2e71y_led_set_color_reject()) {
        IR2E71Y_DEBUG("reject request.");
        return IR2E71Y_RESULT_SUCCESS;
    }

    return ir2e71y_tri_led_set_color(tri_led);
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_SQE_bdic_write_reg                                                 */
/* ------------------------------------------------------------------------- */
static int ir2e71y_SQE_bdic_write_reg(unsigned char reg, unsigned char val)
{
    ir2e71y_bdic_API_DIAG_write_reg(reg, val);
    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_SQE_bdic_read_reg                                                  */
/* ------------------------------------------------------------------------- */
static int ir2e71y_SQE_bdic_read_reg(unsigned char reg, unsigned char *val)
{
    ir2e71y_bdic_API_DIAG_read_reg(reg, val);
    return IR2E71Y_RESULT_SUCCESS;
}
#endif /* CONFIG_ANDROID_ENGINEERING */

/* ------------------------------------------------------------------------- */
/*ir2e71y_SQE_get_lux                                                        */
/* ------------------------------------------------------------------------- */
static int ir2e71y_SQE_get_lux(struct ir2e71y_photo_sensor_val *value)
{
    int ret;

    ret = ir2e71y_bdic_API_PHOTO_SENSOR_get_lux(&(value->value), &(value->lux));

    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_bdic_API_PHOTO_SENSOR_get_lux.");
        value->result = IR2E71Y_RESULT_FAILURE;
    } else {
        value->result = IR2E71Y_RESULT_SUCCESS;
    }

    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_SQE_get_als                                                        */
/* ------------------------------------------------------------------------- */
static int ir2e71y_SQE_get_als(struct ir2e71y_photo_sensor_raw_val *raw_val)
{
    int ret;

    ret = ir2e71y_bdic_API_PHOTO_SENSOR_get_raw_als(&(raw_val->clear), &(raw_val->ir));

    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_bdic_API_PHOTO_SENSOR_get_raw_als.");
        raw_val->result = IR2E71Y_RESULT_FAILURE;
    } else {
        raw_val->result = IR2E71Y_RESULT_SUCCESS;
    }

    return ret;
}

#ifdef IR2E71Y_ALS_INT
/* ------------------------------------------------------------------------- */
/*ir2e71y_SQE_set_alsint                                                     */
/* ------------------------------------------------------------------------- */
static int ir2e71y_SQE_set_alsint(struct ir2e71y_photo_sensor_int_trigger *val)
{
    int ret;
    int trigger = 0;

    ret = ir2e71y_bdic_API_PHOTO_SENSOR_set_alsint(val);
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_bdic_API_PHOTO_SENSOR_set_alsint.");
    }

    if (val->result == IR2E71Y_RESULT_SUCCESS) {
        if (val->trigger1.enable | val->trigger2.enable) {
            if (val->trigger1.enable) {
                trigger = trigger | IR2E71Y_OPT_CHANGE_INT_1;
            }
            if (val->trigger2.enable) {
                trigger = trigger | IR2E71Y_OPT_CHANGE_INT_2;
            }
            IR2E71Y_DEBUG("<OTHER>als int is enable");
            ir2e71y_set_als_int_subscribe(trigger);
        } else {
            IR2E71Y_DEBUG("<OTHER>als int is disable");
            ir2e71y_set_als_int_unsubscribe(IR2E71Y_OPT_CHANGE_INT_1 | IR2E71Y_OPT_CHANGE_INT_2);
        }
    }
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_SQE_get_alsint                                                     */
/* ------------------------------------------------------------------------- */
static int ir2e71y_SQE_get_alsint(struct ir2e71y_photo_sensor_int_trigger *val)
{
    int ret;

    ret = ir2e71y_bdic_API_PHOTO_SENSOR_get_alsint(val);
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_bdic_API_PHOTO_SENSOR_get_alsint.");
    }
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_SQE_get_light_info                                                 */
/* ------------------------------------------------------------------------- */
static int ir2e71y_SQE_get_light_info(struct ir2e71y_light_info *val)
{
    int ret;

    ret = ir2e71y_bdic_API_PHOTO_SENSOR_get_light_info(val);
    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_bdic_API_PHOTO_SENSOR_get_light_info.");
    }
    return ret;
}
#endif /* IR2E71Y_ALS_INT */

/* ------------------------------------------------------------------------- */
/*ir2e71y_SQE_write_bdic_i2c                                                 */
/* ------------------------------------------------------------------------- */
static int ir2e71y_SQE_write_bdic_i2c(struct ir2e71y_bdic_i2c_msg *i2c_msg)
{
    int ret;

    ret = ir2e71y_bdic_API_i2c_transfer(i2c_msg);

    if (ret == IR2E71Y_RESULT_FAILURE_I2C_TMO) {
        IR2E71Y_ERR("<TIME_OUT> ir2e71y_bdic_API_i2c_transfer.");
        return IR2E71Y_RESULT_FAILURE_I2C_TMO;
    } else if (ret == IR2E71Y_RESULT_FAILURE) {
        IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_bdic_API_i2c_transfer.");
        return IR2E71Y_RESULT_FAILURE;
    }

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_SQE_read_bdic_i2c                                                  */
/* ------------------------------------------------------------------------- */
static int ir2e71y_SQE_read_bdic_i2c(struct ir2e71y_bdic_i2c_msg *i2c_msg)
{
    int ret;

    ret = ir2e71y_bdic_API_i2c_transfer(i2c_msg);

    if (ret == IR2E71Y_RESULT_FAILURE_I2C_TMO) {
        IR2E71Y_ERR("<TIME_OUT> ir2e71y_bdic_API_i2c_transfer.");
        return IR2E71Y_RESULT_FAILURE_I2C_TMO;
    } else if (ret == IR2E71Y_RESULT_FAILURE) {
        IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_bdic_API_i2c_transfer.");
        return IR2E71Y_RESULT_FAILURE;
    }

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_SQE_photo_sensor_pow_ctl                                           */
/* ------------------------------------------------------------------------- */
static int ir2e71y_SQE_photo_sensor_pow_ctl(struct ir2e71y_photo_sensor_power_ctl *ctl)
{
    int ret;

    ret = ir2e71y_bdic_API_als_sensor_pow_ctl(ctl->type, ctl->power);
    if (ret != IR2E71Y_RESULT_SUCCESS) {
#ifdef IR2E71Y_ALS_INT
        if (ret == IR2E71Y_RESULT_ALS_INT_OFF) {
            IR2E71Y_DEBUG("<OTHER>als int is disable");
            ir2e71y_set_als_int_unsubscribe(IR2E71Y_OPT_CHANGE_INT_1 | IR2E71Y_OPT_CHANGE_INT_2);
            return IR2E71Y_RESULT_SUCCESS;
        }
#endif /* IR2E71Y_ALS_INT */
        IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_bdic_API_als_sensor_pow_ctl.");
        return IR2E71Y_RESULT_FAILURE;
    }
    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_SQE_prox_sensor_pow_ctl                                            */
/* ------------------------------------------------------------------------- */
static int ir2e71y_SQE_prox_sensor_pow_ctl(int power_mode)
{
    int ret = IR2E71Y_RESULT_SUCCESS;
    IR2E71Y_TRACE("in power_mode=%d", power_mode);

    switch (power_mode) {
    case IR2E71Y_PROX_SENSOR_POWER_OFF:
        ir2e71y_pm_API_ps_user_manager(IR2E71Y_DEV_TYPE_PS, IR2E71Y_DEV_REQ_OFF);
        break;

    case IR2E71Y_PROX_SENSOR_POWER_ON:
        ir2e71y_pm_API_ps_user_manager(IR2E71Y_DEV_TYPE_PS, IR2E71Y_DEV_REQ_ON);
        break;

    default:
        ret = IR2E71Y_RESULT_FAILURE;
        IR2E71Y_ERR("POWER_MODE ERROR(mode=%d)", power_mode);
        break;
    }

    IR2E71Y_TRACE("out ret=%d", ret);
    return ret;

}

/* ------------------------------------------------------------------------- */
/*ir2e71y_SQE_event_subscribe                                                */
/* ------------------------------------------------------------------------- */
static int ir2e71y_SQE_event_subscribe(struct ir2e71y_subscribe *subscribe)
{
    int ret;
    int i;
    int bAllNull = 0;

    if (ir2e71y_subscribe_type_table[subscribe->irq_type] == IR2E71Y_SUBSCRIBE_TYPE_INT) {
        ret = ir2e71y_bdic_API_IRQ_check_type(subscribe->irq_type);
        if (ret != IR2E71Y_RESULT_SUCCESS) {
            IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_bdic_API_IRQ_check_type.");
            return IR2E71Y_RESULT_FAILURE;
        }
    }

    down(&ir2e71y_sem_callback);

    bAllNull = 1;
    for (i = 0; i < NUM_IR2E71Y_IRQ_TYPE; i++) {
        if ((ir2e71y_subscribe_type_table[i] == IR2E71Y_SUBSCRIBE_TYPE_INT) &&
            (ir2e71y_callback_table[i] != NULL)) {
            bAllNull = 0;
        }
    }

    if (ir2e71y_callback_table[subscribe->irq_type] != NULL) {
        IR2E71Y_DEBUG("INT_SUBSCRIBE CHANGE(irq_type=%d)", subscribe->irq_type);
    } else {
        IR2E71Y_DEBUG("INT_SUBSCRIBE NEW ENTRY(irq_type=%d)", subscribe->irq_type);
    }

    ir2e71y_callback_table[subscribe->irq_type] = subscribe->callback;

    if (ir2e71y_subscribe_type_table[subscribe->irq_type] != IR2E71Y_SUBSCRIBE_TYPE_INT) {
        ir2e71y_timer_int_register();
    }

    if (subscribe->irq_type == IR2E71Y_IRQ_TYPE_DET) {
        ir2e71y_bdic_API_IRQ_det_irq_ctrl(true);
    }

    up(&ir2e71y_sem_callback);

    if (ir2e71y_subscribe_type_table[subscribe->irq_type] == IR2E71Y_SUBSCRIBE_TYPE_INT) {
        if (bAllNull) {
            IR2E71Y_DEBUG("INT_SUBSCRIBE enable_irq");
            ir2e71y_IO_API_set_irq(IR2E71Y_IRQ_ENABLE);
        }
    }

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_SQE_event_unsubscribe                                              */
/* ------------------------------------------------------------------------- */
static int ir2e71y_SQE_event_unsubscribe(int irq_type)
{
    int ret;
    int i;
    int bAllNull = 0;

    if (ir2e71y_subscribe_type_table[irq_type] == IR2E71Y_SUBSCRIBE_TYPE_INT) {
        ret = ir2e71y_bdic_API_IRQ_check_type(irq_type);
        if (ret !=  IR2E71Y_RESULT_SUCCESS) {
            IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_bdic_API_IRQ_check_type.");
            return IR2E71Y_RESULT_FAILURE;
        }
    }

    down(&ir2e71y_sem_callback);

    if (ir2e71y_callback_table[irq_type] == NULL) {
        IR2E71Y_DEBUG("INT_UNSUBSCRIBE DONE(irq_type=%d)", irq_type);
    } else {
        if (ir2e71y_subscribe_type_table[irq_type] == IR2E71Y_SUBSCRIBE_TYPE_INT) {
        } else {
            ir2e71y_timer_int_delete();
        }
        if (irq_type == IR2E71Y_IRQ_TYPE_DET) {
            ir2e71y_bdic_API_IRQ_det_irq_ctrl(false);
        }

        ir2e71y_callback_table[irq_type] = NULL;

        if (ir2e71y_subscribe_type_table[irq_type] == IR2E71Y_SUBSCRIBE_TYPE_INT) {
            bAllNull = 1;
            for (i = 0; i < NUM_IR2E71Y_IRQ_TYPE; i++) {
                if ((ir2e71y_subscribe_type_table[i] == IR2E71Y_SUBSCRIBE_TYPE_INT) &&
                    (ir2e71y_callback_table[i] != NULL)) {
                    bAllNull = 0;
                }
            }
            if (bAllNull) {
                ir2e71y_IO_API_set_irq(IR2E71Y_IRQ_DISABLE);
                IR2E71Y_DEBUG("INT_UNSUBSCRIBE disable_irq");
            }
        }

        IR2E71Y_DEBUG("INT_UNSUBSCRIBE SUCCESS(irq_type=%d)", irq_type);
    }

    up(&ir2e71y_sem_callback);

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_SQE_psals_recovery                                                 */
/* ------------------------------------------------------------------------- */
static int ir2e71y_SQE_psals_recovery(void)
{
    int result = IR2E71Y_RESULT_SUCCESS;
    int ps_flg = 0;

    IR2E71Y_TRACE("in");

    ps_flg = ir2e71y_pm_API_is_ps_active();

    if (ps_flg == IR2E71Y_DEV_STATE_ON) {
        /* notify to proximity module that recovery is staring */
#ifdef CONFIG_PROXIMITY_INT_HOST
        PROX_recovery_start_func();
#endif /* CONFIG_PROXIMITY_INT_HOST */
    }

    ir2e71y_semaphore_start();

    ir2e71y_pm_API_psals_power_off();

    ir2e71y_IO_API_delay_us(10 * 1000);
    ir2e71y_bdic_API_IRQ_i2c_error_Clear();

    ir2e71y_pm_API_psals_error_power_recovery();

    down(&ir2e71y_sem_req_recovery_psals);
    ir2e71y_recovery_psals_queued_flag = 0;
    up(&ir2e71y_sem_req_recovery_psals);

    result = ir2e71y_bdic_API_psals_is_recovery_successful();
    if (result != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("PALS Recovery Error!!");
    }

    ir2e71y_semaphore_end(__func__);

    if (ps_flg == IR2E71Y_DEV_STATE_ON) {

        /* notify to proximity module that recovery is ending */
       ir2e71y_IO_API_msleep(20);
#ifdef CONFIG_PROXIMITY_INT_HOST
        PROX_recovery_end_func();
#endif /* CONFIG_PROXIMITY_INT_HOST */
    }

    IR2E71Y_DEBUG("main_disp_status=%d", ir2e71y_if_ctx.main_disp_status)
    if (result == IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_DEBUG("enable_irq for psals_recovery");
        ir2e71y_IO_API_set_irq(IR2E71Y_IRQ_ENABLE);
    }
    IR2E71Y_TRACE("out");
    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_semaphore_start                                                    */
/* ------------------------------------------------------------------------- */
static void ir2e71y_semaphore_start(void)
{
    IR2E71Y_INFO("in");
    down(&ir2e71y_sem);
    IR2E71Y_INFO("out");
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_semaphore_end                                                      */
/* ------------------------------------------------------------------------- */
static void ir2e71y_semaphore_end(const char *func)
{
    IR2E71Y_INFO("in");
    up(&ir2e71y_sem);
    IR2E71Y_INFO("out");
}

/* ------------------------------------------------------------------------- */
/* INTERRUPT                                                                 */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/*ir2e71y_gpio_int_isr                                                       */
/* ------------------------------------------------------------------------- */
static irqreturn_t ir2e71y_gpio_int_isr(int irq_num, void *data)
{
    irqreturn_t rc = IRQ_HANDLED;
    int ret;
    unsigned long flags = 0;

    ir2e71y_IO_API_set_irq(IR2E71Y_IRQ_DISABLE);

    spin_lock_irqsave(&ir2e71y_q_lock, flags);

    IR2E71Y_TRACE(":Start");

    if (ir2e71y_wq_gpio) {
        ir2e71y_wake_lock();
        ret = queue_work(ir2e71y_wq_gpio, &ir2e71y_wq_gpio_wk);
        if (ret == 0) {
            ir2e71y_wake_unlock();
            IR2E71Y_ERR("<QUEUE_WORK_FAILURE>");
        }
    }
    spin_unlock_irqrestore(&ir2e71y_q_lock, flags);

    return rc;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_workqueue_handler_gpio                                             */
/* ------------------------------------------------------------------------- */
static void ir2e71y_workqueue_handler_gpio(struct work_struct *work)
{
    struct ir2e71y_queue_data_t *qdata = NULL;
    int    i;
    int    bFirstQue = 0;
    int    ret;
    int    nBDIC_QueFac = 0;

    IR2E71Y_TRACE("Start");

    ir2e71y_semaphore_start();
    ir2e71y_bdic_API_IRQ_save_fac();

    do {
        ret = ir2e71y_bdic_API_IRQ_check_fac();
        if (ret != IR2E71Y_RESULT_SUCCESS) {
            IR2E71Y_DEBUG("no factor");
            break;
        }

        down(&ir2e71y_sem_irq_fac);
        for (i = 0; i < IR2E71Y_IRQ_MAX_KIND; i++) {
            nBDIC_QueFac = ir2e71y_bdic_API_IRQ_get_fac(i);
            if (nBDIC_QueFac == IR2E71Y_BDIC_IRQ_TYPE_NONE) {
                break;
            }

            if (ir2e71y_wq_gpio_task) {
                qdata = kmalloc(sizeof(ir2e71y_queue_data), GFP_KERNEL);
                if (qdata != NULL) {
                    qdata->irq_GFAC = nBDIC_QueFac;
                    list_add_tail(&qdata->list, &ir2e71y_queue_data.list);
                    if (bFirstQue == 0) {
                        bFirstQue = 1;
                        ir2e71y_wake_lock();
                        ret = queue_work(ir2e71y_wq_gpio_task, &ir2e71y_wq_gpio_task_wk);
                        if (ret == 0) {
                            ir2e71y_wake_unlock();
                            IR2E71Y_DEBUG("<QUEUE_WORK_FAILURE> queue_work failed");
                        }
                    }
                } else {
                   IR2E71Y_ERR("<QUEUE_WORK_FAILURE> kmalloc failed (BDIC_QueFac=%d)", nBDIC_QueFac);
                }
            }
        }
        up(&ir2e71y_sem_irq_fac);

    } while (0);

    ir2e71y_bdic_API_IRQ_Clear();
    ir2e71y_semaphore_end(__func__);

    if (ir2e71y_bdic_API_IRQ_check_DET() != IR2E71Y_BDIC_IRQ_TYPE_DET &&
        ir2e71y_bdic_API_IRQ_check_I2C_ERR() != IR2E71Y_BDIC_IRQ_TYPE_I2C_ERR) {
        IR2E71Y_DEBUG("enable_irq for \"No DET\" or \"No I2C_ERR\"");
        ir2e71y_IO_API_set_irq(IR2E71Y_IRQ_ENABLE);
    }
    IR2E71Y_TRACE("Finish");
    ir2e71y_wake_unlock();
    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_workqueue_gpio_task                                                */
/* ------------------------------------------------------------------------- */
static void ir2e71y_workqueue_gpio_task(struct work_struct *work)
{
    struct list_head *listptr;
    struct ir2e71y_queue_data_t  *entry;
    struct ir2e71y_queue_data_t  *entryFirst = NULL;
    int     nFirstBDIC_GFAC = 0;
    int     nFirst_GFAC = -1;
    int     bFirst = 0;
    int     bThrough = 0;
    void (*temp_callback)(void);

    IR2E71Y_TRACE("Start");

    do {
        down(&ir2e71y_sem_irq_fac);
        bThrough = 0;
        entryFirst = NULL;
        bFirst = 0;
        nFirstBDIC_GFAC = 0;
        list_for_each(listptr, &ir2e71y_queue_data.list) {
            entry = list_entry( listptr, struct ir2e71y_queue_data_t, list);
            if (bFirst == 0) {
                entryFirst = entry;
                nFirstBDIC_GFAC = entry->irq_GFAC;
                bFirst = 1;
            } else {
                if (entry->irq_GFAC == nFirstBDIC_GFAC) {
                    bThrough = 1;
                }
            }
        }

        if (entryFirst != NULL) {
            list_del(&entryFirst->list);
            kfree(entryFirst);
        } else {
            IR2E71Y_DEBUG("no entry");
            up(&ir2e71y_sem_irq_fac);
            break;
        }
        up(&ir2e71y_sem_irq_fac);


        if (bThrough == 0) {
            if (nFirstBDIC_GFAC == IR2E71Y_BDIC_IRQ_TYPE_NONE) {
                IR2E71Y_DEBUG("failed (no BDIC_GFAC=%d)", nFirstBDIC_GFAC);
            } else {
                nFirst_GFAC = -1;
                switch (nFirstBDIC_GFAC) {
                case IR2E71Y_BDIC_IRQ_TYPE_ALS:
                        nFirst_GFAC = IR2E71Y_IRQ_TYPE_ALS_REQ;
                        break;
                case IR2E71Y_BDIC_IRQ_TYPE_PS:
                        nFirst_GFAC = IR2E71Y_IRQ_TYPE_PS;
                        break;
                default:
                        break;
                }

                IR2E71Y_DEBUG("Callback[%d] Start", nFirstBDIC_GFAC);
                if (nFirst_GFAC >= 0) {
                    down(&ir2e71y_sem_callback);
                    temp_callback = ir2e71y_callback_table[nFirst_GFAC];
                    up(&ir2e71y_sem_callback);

                    if (temp_callback != NULL) {
                        (*temp_callback)();
                    } else {
                        IR2E71Y_DEBUG("Callback is Nulle pointer(irq_type=%d)", nFirst_GFAC);
                    }
                } else {
                    switch (nFirstBDIC_GFAC) {
                    case IR2E71Y_BDIC_IRQ_TYPE_I2C_ERR:
                        ir2e71y_do_psals_recovery();
                        break;
#ifdef IR2E71Y_ALS_INT
                    case IR2E71Y_BDIC_IRQ_TYPE_ALS_TRIGGER:
                        ir2e71y_do_als_int_report(IR2E71Y_OPT_CHANGE_INT_1 | IR2E71Y_OPT_CHANGE_INT_2);
                        break;
                    case IR2E71Y_BDIC_IRQ_TYPE_ALS_TRIGGER1:
                        ir2e71y_do_als_int_report(IR2E71Y_OPT_CHANGE_INT_1);
                        break;
                    case IR2E71Y_BDIC_IRQ_TYPE_ALS_TRIGGER2:
                        ir2e71y_do_als_int_report(IR2E71Y_OPT_CHANGE_INT_2);
                        break;
#endif /* IR2E71Y_ALS_INT */
                    default:
                        break;
                    }
                }
            }
        } else {
            IR2E71Y_DEBUG("Skip (BDIC_GFAC=%d)", nFirstBDIC_GFAC);
        }
    } while (1);


    IR2E71Y_TRACE("Finish");
    ir2e71y_wake_unlock();

    return;
}

static void ir2e71y_bkl_workhandler(struct work_struct *work)
{
    int ret = 0;
    struct ir2e71y_main_bkl_ctl bkl;

    IR2E71Y_TRACE("in");
    if (ir2e71y_bkl_request_brightness > 0) {
        bkl.mode = IR2E71Y_MAIN_BKL_MODE_FIX;
        bkl.param = ir2e71y_bkl_request_brightness;
        ret = ir2e71y_main_bkl_on(&bkl);
    } else {
        ret = ir2e71y_main_bkl_off();
    }
    IR2E71Y_TRACE("out");
    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_wake_lock_init                                                     */
/* ------------------------------------------------------------------------- */
static void ir2e71y_wake_lock_init(void)
{
    unsigned long flags = 0;

    spin_lock_irqsave(&ir2e71y_wake_spinlock, flags);
    ir2e71y_wake_lock_wq_refcnt = 0;
    wake_lock_init(&ir2e71y_wake_lock_wq, WAKE_LOCK_SUSPEND, "ir2e71y_wake_lock_wq");
    spin_unlock_irqrestore(&ir2e71y_wake_spinlock, flags);
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_wake_lock                                                          */
/* ------------------------------------------------------------------------- */
static void ir2e71y_wake_lock(void)
{
    unsigned long flags = 0;

    spin_lock_irqsave(&ir2e71y_wake_spinlock, flags);
    if (ir2e71y_wake_lock_wq_refcnt++ == 0) {
        wake_lock(&ir2e71y_wake_lock_wq);
    }
    spin_unlock_irqrestore(&ir2e71y_wake_spinlock, flags);
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_wake_unlock                                                        */
/* ------------------------------------------------------------------------- */
static void ir2e71y_wake_unlock(void)
{
    unsigned long flags = 0;

    spin_lock_irqsave(&ir2e71y_wake_spinlock, flags);
    if (--ir2e71y_wake_lock_wq_refcnt <= 0) {
        wake_unlock(&ir2e71y_wake_lock_wq);
        ir2e71y_wake_lock_wq_refcnt = 0;
    }
    spin_unlock_irqrestore(&ir2e71y_wake_spinlock, flags);
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_timer_int_isr                                                      */
/* ------------------------------------------------------------------------- */
static void ir2e71y_timer_int_isr(unsigned long data)
{
    int ret;

    IR2E71Y_DEBUG("Timeout ( registered %ld, now %ld ).", data, jiffies);
    IR2E71Y_TRACE(":Start");

    if (ir2e71y_timer_stop) {
        IR2E71Y_DEBUG("Timer is not to be restarted.");
        return;
    }
    if (ir2e71y_wq_timer_task) {
        ret = queue_work(ir2e71y_wq_timer_task, &ir2e71y_wq_timer_task_wk);
        if (ret == 0) {
            IR2E71Y_ERR("<QUEUE_WORK_FAILURE> ");
        }
    }

    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_timer_int_register                                                 */
/* ------------------------------------------------------------------------- */
static void ir2e71y_timer_int_register(void)
{
    down(&ir2e71y_sem_timer);

    ir2e71y_timer.expires  = jiffies + (10 * HZ);
    ir2e71y_timer.data     = (unsigned long)jiffies;
    ir2e71y_timer.function = ir2e71y_timer_int_isr;

    if (!ir2e71y_timer_stop) {
        up(&ir2e71y_sem_timer);
        return;
    }

    add_timer(&ir2e71y_timer);
    ir2e71y_timer_stop = 0;

    up(&ir2e71y_sem_timer);

    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_timer_int_delete                                                   */
/* ------------------------------------------------------------------------- */
static void ir2e71y_timer_int_delete(void)
{
    down(&ir2e71y_sem_timer);

    del_timer_sync(&ir2e71y_timer);
    ir2e71y_timer_stop = 1;

    up(&ir2e71y_sem_timer);

    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_timer_int_mod                                                      */
/* ------------------------------------------------------------------------- */
static void ir2e71y_timer_int_mod(void)
{
    down(&ir2e71y_sem_timer);

    if (ir2e71y_timer_stop) {
        up(&ir2e71y_sem_timer);
        return;
    }

    mod_timer(&ir2e71y_timer, (unsigned long)(jiffies + (10 * HZ)));
    ir2e71y_timer_stop = 0;

    up(&ir2e71y_sem_timer);

    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_workqueue_timer_task                                               */
/* ------------------------------------------------------------------------- */
static void ir2e71y_workqueue_timer_task(struct work_struct *work)
{
    int     ret = 0;
    int     nFirst_GFAC = -1;
    void    (*temp_callback)(void);

    nFirst_GFAC = IR2E71Y_IRQ_TYPE_DET;

    if (ret == IR2E71Y_RESULT_SUCCESS) {
        ir2e71y_timer_int_mod();
        return;
    }
    IR2E71Y_DEBUG("A recovery processing is required.");

    down(&ir2e71y_sem_callback);
    temp_callback = ir2e71y_callback_table[nFirst_GFAC];
    up(&ir2e71y_sem_callback);

    if (temp_callback != NULL) {
        (*temp_callback)();
    } else {
        IR2E71Y_DEBUG(" Callback is Nulle pointer(irq_type=%d)", nFirst_GFAC);
    }

    ir2e71y_timer_int_mod();

    return;
}

#ifdef IR2E71Y_ALS_INT
/* ------------------------------------------------------------------------- */
/*ir2e71y_do_als_int_report                                                  */
/* ------------------------------------------------------------------------- */
static int ir2e71y_do_als_int_report(int ginf2_val)
{
    IR2E71Y_TRACE("in");

    if (ginf2_val != 0) {
        ir2e71y_input_subsystem_report(ginf2_val);
        ir2e71y_set_als_int_unsubscribe(ginf2_val);
    }

    IR2E71Y_TRACE("out");
    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_do_als_int_report_dummy                                            */
/* ------------------------------------------------------------------------- */
static void ir2e71y_do_als_int_report_dummy(void)
{
    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_set_als_int_subscribe                                              */
/* ------------------------------------------------------------------------- */
static int ir2e71y_set_als_int_subscribe(int trigger)
{
    int ret = 0;
    struct ir2e71y_subscribe alsint_subs;
    int before_flag = als_int_enable;

    IR2E71Y_TRACE("in");

    if (trigger == 0) {
        IR2E71Y_ERR("trigger zero");
        return IR2E71Y_RESULT_FAILURE;
    }

    als_int_enable = (als_int_enable | trigger) & (IR2E71Y_OPT_CHANGE_INT_1 | IR2E71Y_OPT_CHANGE_INT_2);
    if (before_flag != 0) {
        IR2E71Y_DEBUG("<caution>double subscribe");
        return IR2E71Y_RESULT_SUCCESS;
    }

    alsint_subs.irq_type = IR2E71Y_IRQ_TYPE_ALS;
    alsint_subs.callback = ir2e71y_do_als_int_report_dummy;
    ret = ir2e71y_event_subscribe(&alsint_subs);

    IR2E71Y_TRACE("out ret=%04x", ret);
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_set_als_int_unsubscribe                                            */
/* ------------------------------------------------------------------------- */
static int ir2e71y_set_als_int_unsubscribe(int trigger)
{
    int ret = 0;

    IR2E71Y_TRACE("in");

    if (trigger == 0) {
        IR2E71Y_ERR("trigger zero");
        return IR2E71Y_RESULT_FAILURE;
    }

    if (als_int_enable == 0) {
        IR2E71Y_DEBUG("<caution>double unsubscribe");
        return IR2E71Y_RESULT_SUCCESS;
    }

    als_int_enable = (als_int_enable & (~trigger)) & (IR2E71Y_OPT_CHANGE_INT_1 | IR2E71Y_OPT_CHANGE_INT_2);
    if (als_int_enable == 0) {
        ret = ir2e71y_event_unsubscribe(IR2E71Y_IRQ_TYPE_ALS);
    } else {
        IR2E71Y_DEBUG("<caution>double unsubscribe");
        return IR2E71Y_RESULT_SUCCESS;
    }

    IR2E71Y_TRACE("out ret=%04x", ret);
    return ret;
}
#endif /* IR2E71Y_ALS_INT */

/* ------------------------------------------------------------------------- */
/*ir2e71y_do_psals_recovery                                                  */
/* ------------------------------------------------------------------------- */
static int ir2e71y_do_psals_recovery(void)
{
    int ret = IR2E71Y_RESULT_SUCCESS;

    int queued = 0;
    IR2E71Y_TRACE("in");

    if (ir2e71y_check_initialized() != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_DEBUG("out1");
        return IR2E71Y_RESULT_SUCCESS;
    }

    if (ir2e71y_check_bdic_exist() != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_DEBUG("out2");
        return IR2E71Y_RESULT_SUCCESS;
    }

    if (ir2e71y_check_upper_unit() != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_DEBUG("out3");
        return IR2E71Y_RESULT_SUCCESS;
    }

    if (!ir2e71y_wq_recovery) {
        IR2E71Y_ERR(" workqueue nothing.");
        return IR2E71Y_RESULT_SUCCESS;
    }

    ir2e71y_wake_lock();

    down(&ir2e71y_sem_req_recovery_psals);

    if (!ir2e71y_recovery_psals_queued_flag) {
        ir2e71y_recovery_psals_queued_flag = 1;
        ret = queue_work(ir2e71y_wq_recovery, &ir2e71y_wq_recovery_psals_wk);

        if (ret == 0) {
            ir2e71y_recovery_psals_queued_flag = 0;
            IR2E71Y_ERR("<QUEUE_WORK_FAILURE> .");
            ret = IR2E71Y_RESULT_FAILURE;
        } else {
            queued = 1;
            ret = IR2E71Y_RESULT_SUCCESS;
        }
    } else {
        IR2E71Y_DEBUG("queued. now recovering... ");
        ret = IR2E71Y_RESULT_SUCCESS;
    }
    up(&ir2e71y_sem_req_recovery_psals);

    if (!queued) {
        ir2e71y_wake_unlock();
    }

    IR2E71Y_TRACE("out");

    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_workqueue_handler_recovery_psals                                   */
/* ------------------------------------------------------------------------- */
static void ir2e71y_workqueue_handler_recovery_psals(struct work_struct *work)
{
    void (*temp_callback)(void);

    IR2E71Y_TRACE("in");

    down(&ir2e71y_sem_callback);
    temp_callback = ir2e71y_callback_table[IR2E71Y_IRQ_TYPE_I2CERR];
    up(&ir2e71y_sem_callback);

    if (temp_callback != NULL) {
        (*temp_callback)();
    }

    ir2e71y_wake_unlock();

    IR2E71Y_TRACE("End");
    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_psals_recovery                                                     */
/* ------------------------------------------------------------------------- */
static void ir2e71y_psals_recovery(void)
{
    IR2E71Y_TRACE("in");

    ir2e71y_SQE_psals_recovery();

    IR2E71Y_TRACE("out");
    return;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_event_unsubscribe                                                  */
/* ------------------------------------------------------------------------- */
static int ir2e71y_event_unsubscribe(int irq_type)
{
    int ret;

    IR2E71Y_PERFORMANCE("RESUME PANEL EVENT-UNSUBSCRIBE 0010 START");

    if (ir2e71y_bdic_unsubscribe_check(irq_type) != IR2E71Y_RESULT_SUCCESS) {
        return IR2E71Y_RESULT_FAILURE;
    }

    ret = ir2e71y_SQE_event_unsubscribe(irq_type);

    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_SQE_event_unsubscribe.");
        return IR2E71Y_RESULT_FAILURE;
    }

    IR2E71Y_PERFORMANCE("RESUME PANEL EVENT-UNSUBSCRIBE 0010 END");

    return IR2E71Y_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_event_subscribe                                                    */
/* ------------------------------------------------------------------------- */
static int ir2e71y_event_subscribe(struct ir2e71y_subscribe *subscribe)
{
    int ret;

    IR2E71Y_PERFORMANCE("RESUME PANEL EVENT-SUBSCRIBE 0010 START");


    IR2E71Y_TRACE(":Start(irq_type=%d)", subscribe->irq_type);

    if (ir2e71y_bdic_subscribe_check(subscribe) != IR2E71Y_RESULT_SUCCESS) {
        return IR2E71Y_RESULT_FAILURE;
    }

    if (subscribe->irq_type == IR2E71Y_IRQ_TYPE_DET) {
        if (ir2e71y_check_upper_unit() != IR2E71Y_RESULT_SUCCESS) {
            return IR2E71Y_RESULT_SUCCESS;
        }
    }

    ret = ir2e71y_SQE_event_subscribe(subscribe);

    if (ret != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_ERR("<RESULT_FAILURE> ir2e71y_SQE_event_subscribe.");
        return IR2E71Y_RESULT_FAILURE;
    }

    IR2E71Y_PERFORMANCE("RESUME PANEL EVENT-SUBSCRIBE 0010 END");

    return IR2E71Y_RESULT_SUCCESS;

}

#ifdef IR2E71Y_SYSFS_LED
/* ------------------------------------------------------------------------- */
/*ir2e71y_led_brightness_set                                                 */
/* ------------------------------------------------------------------------- */
static void ir2e71y_led_brightness_set(struct led_classdev *led_cdev, enum led_brightness value)
{

    struct leds_led_data *led_sys = container_of(led_cdev, struct leds_led_data, cdev);
    struct ir2e71y_tri_led led;

    memset(&led, 0x00, sizeof(led));

    if (ir2e71y_check_initialized() != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_DEBUG("out1");
        return;
    }

    if (ir2e71y_check_bdic_exist() != IR2E71Y_RESULT_SUCCESS) {
        IR2E71Y_DEBUG("out2");
        return;
    }

#if defined(IR2E71Y_ILLUMI_TRIPLE_COLOR_LED) && defined(IR2E71Y_ANIME_COLOR_LED)
    if (ir2e71y_bdic_API_LED_is_running_illumi_triple_color()) {
        ir2e71y_bdic_API_LED_clear_illumi_triple_color();
    }
#endif /* IR2E71Y_ILLUMI_TRIPLE_COLOR_LED && IR2E71Y_ANIME_COLOR_LED */

    IR2E71Y_DEBUG("before mode:%d", ir2e71y_if_ctx.sysfs_led1.led_mode);
    switch (ir2e71y_if_ctx.sysfs_led1.led_mode) {
    case IR2E71Y_TRI_LED_MODE_OFF:
        break;

    case IR2E71Y_TRI_LED_MODE_NORMAL:
        switch (led_sys->no) {
        case SYSFS_LED_SH_LED_1:
            led.red   = ir2e71y_if_ctx.sysfs_led1.red;
            led.green = ir2e71y_if_ctx.sysfs_led1.green;
            led.blue  = ir2e71y_if_ctx.sysfs_led1.blue;
            break;
#ifdef IR2E71Y_COLOR_LED_TWIN
        case SYSFS_LED_SH_LED_2:
            led.red   = ir2e71y_if_ctx.sysfs_led2.red;
            led.green = ir2e71y_if_ctx.sysfs_led2.green;
            led.blue  = ir2e71y_if_ctx.sysfs_led2.blue;
            break;
#endif /* IR2E71Y_COLOR_LED_TWIN */
        default:
            IR2E71Y_ERR("invalid id. id no:%d", led_sys->no);
            return;
        }
        break;

    case IR2E71Y_TRI_LED_MODE_BLINK:
    default:
        ir2e71y_clean_sysfs_led();
        if (value == 0) {
            IR2E71Y_DEBUG("leds off. no=%d", led_sys->no);
            ir2e71y_semaphore_start();
            ir2e71y_bdic_API_TRI_LED_off();
            ir2e71y_semaphore_end(__func__);
            return;
        }
        break;
    }

    switch (led_sys->color) {
    case SYSFS_LED_SH_RED:
        led.red   = value;
        break;
    case SYSFS_LED_SH_GREEN:
        led.green = value;
        break;
    case SYSFS_LED_SH_BLUE:
        led.blue  = value;
        break;
    default:
        IR2E71Y_ERR("invalid color. color no:%d", led_sys->color);
        return;
    }

    IR2E71Y_DEBUG("%s(id:%d, color:%d) is %d\n", led_sys->cdev.name, led_sys->no, led_sys->color, value);
    ir2e71y_semaphore_start();

    ir2e71y_SQE_tri_led_on(led_sys->no, &led);

    ir2e71y_semaphore_end(__func__);

}

/* ------------------------------------------------------------------------- */
/*ir2e71y_led_brightness_get                                                 */
/* ------------------------------------------------------------------------- */
static enum led_brightness ir2e71y_led_brightness_get(struct led_classdev *led_cdev)
{
    struct leds_led_data *led_sys = container_of(led_cdev, struct leds_led_data, cdev);
    int brightness = 0;


    switch (ir2e71y_if_ctx.sysfs_led1.led_mode) {
    case IR2E71Y_TRI_LED_MODE_NORMAL:
        switch (led_sys->no) {
        case SYSFS_LED_SH_LED_1:
            switch (led_sys->color) {
            case SYSFS_LED_SH_RED:
                brightness = ir2e71y_if_ctx.sysfs_led1.red;
                break;
            case SYSFS_LED_SH_GREEN:
                brightness = ir2e71y_if_ctx.sysfs_led1.green;
                break;
            case SYSFS_LED_SH_BLUE:
                brightness = ir2e71y_if_ctx.sysfs_led1.blue;
                break;
            default:
                IR2E71Y_ERR("invalid color. color no:%d", led_sys->color);
                goto brightness_read_fail;
            }
            break;
#ifdef IR2E71Y_COLOR_LED_TWIN
        case SYSFS_LED_SH_LED_2:
            switch (led_sys->color) {
            case SYSFS_LED_SH_RED:
                brightness = ir2e71y_if_ctx.sysfs_led2.red;
                break;
            case SYSFS_LED_SH_GREEN:
                brightness = ir2e71y_if_ctx.sysfs_led2.green;
                break;
            case SYSFS_LED_SH_BLUE:
                brightness = ir2e71y_if_ctx.sysfs_led2.blue;
                break;
            default:
                IR2E71Y_ERR("invalid color. color no:%d", led_sys->color);
                goto brightness_read_fail;
            }
            break;
#endif /* IR2E71Y_COLOR_LED_TWIN */
        default:
            IR2E71Y_ERR("invalid id. id no:%d", led_sys->no);
            goto brightness_read_fail;
        }
        break;

    default:
        IR2E71Y_DEBUG("mode:%d", ir2e71y_if_ctx.sysfs_led1.led_mode);
        goto brightness_read_fail;
    }

brightness_read_fail:
    return brightness;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_led_blink_store                                                    */
/* ------------------------------------------------------------------------- */
static ssize_t ir2e71y_led_blink_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    unsigned long data;
    ssize_t ret = -EINVAL;
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct leds_led_data *led_sys = container_of(led_cdev, struct leds_led_data, cdev);

    ret = kstrtoul(buf, 10, &data);
    if (ret) {
        return ret;
    }

    ir2e71y_SQE_tri_led_blink_store(led_sys, (unsigned int)data);

    return count;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_led_blink_show                                                     */
/* ------------------------------------------------------------------------- */
static ssize_t ir2e71y_led_blink_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct leds_led_data *led_sys = container_of(led_cdev, struct leds_led_data, cdev);
    int blinking = 0;


    switch (ir2e71y_if_ctx.sysfs_led1.led_mode) {
    case IR2E71Y_TRI_LED_MODE_BLINK:
        switch (led_sys->no) {
        case SYSFS_LED_SH_LED_1:
            switch (led_sys->color) {
            case SYSFS_LED_SH_RED:
                blinking = ir2e71y_if_ctx.sysfs_led1.red;
                break;
            case SYSFS_LED_SH_GREEN:
                blinking = ir2e71y_if_ctx.sysfs_led1.green;
                break;
            case SYSFS_LED_SH_BLUE:
                blinking = ir2e71y_if_ctx.sysfs_led1.blue;
                break;
            default:
                IR2E71Y_ERR("invalid color. color no:%d", led_sys->color);
                goto blink_read_fail;
            }
            break;
#ifdef IR2E71Y_COLOR_LED_TWIN
        case SYSFS_LED_SH_LED_2:
            switch (led_sys->color) {
            case SYSFS_LED_SH_RED:
                blinking = ir2e71y_if_ctx.sysfs_led2.red;
                break;
            case SYSFS_LED_SH_GREEN:
                blinking = ir2e71y_if_ctx.sysfs_led2.green;
                break;
            case SYSFS_LED_SH_BLUE:
                blinking = ir2e71y_if_ctx.sysfs_led2.blue;
                break;
            default:
                IR2E71Y_ERR("invalid color. color no:%d", led_sys->color);
                goto blink_read_fail;
            }
            break;
#endif /* IR2E71Y_COLOR_LED_TWIN */
        default:
            IR2E71Y_ERR("invalid id. id no:%d", led_sys->no);
            goto blink_read_fail;
        }
        break;

    default:
        IR2E71Y_DEBUG("mode:%d", ir2e71y_if_ctx.sysfs_led1.led_mode);
        goto blink_read_fail;
    }

blink_read_fail:
    return snprintf(buf, PAGE_SIZE, "%d\n", blinking);
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_led_on_off_ms_store                                                */
/* ------------------------------------------------------------------------- */
static ssize_t ir2e71y_led_on_off_ms_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
    struct led_classdev *led_cdev = dev_get_drvdata(dev);
    struct leds_led_data *led_sys = container_of(led_cdev, struct leds_led_data, cdev);
    int on_ms  = 0;
    int off_ms = 0;
    int ret;
    unsigned int data = 1;

    IR2E71Y_TRACE("in");
    ret = sscanf(buf, "%d %d", &on_ms, &off_ms);
    if (ret < 2) {
        return -EINVAL;
    }

    if ((on_ms <= 0) && (off_ms <= 0)) {
        data = 0;
    }

    IR2E71Y_DEBUG("on_ms:%d, off_ms:%d", on_ms, off_ms);
    ir2e71y_SQE_tri_led_blink_store(led_sys, data);
    IR2E71Y_TRACE("out");

    return size;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_clear_sysfs_led                                                    */
/* ------------------------------------------------------------------------- */
static void ir2e71y_clean_sysfs_led(void) {

    ir2e71y_if_ctx.sysfs_led1.red = 0;
    ir2e71y_if_ctx.sysfs_led1.green = 0;
    ir2e71y_if_ctx.sysfs_led1.blue = 0;
    ir2e71y_if_ctx.sysfs_led1.led_mode = IR2E71Y_TRI_LED_MODE_OFF;
#ifdef IR2E71Y_COLOR_LED_TWIN
    ir2e71y_if_ctx.sysfs_led2.red = 0;
    ir2e71y_if_ctx.sysfs_led2.green = 0;
    ir2e71y_if_ctx.sysfs_led2.blue = 0;
    ir2e71y_if_ctx.sysfs_led2.led_mode = IR2E71Y_TRI_LED_MODE_OFF;
#endif /* IR2E71Y_COLOR_LED_TWIN */

}

/* ------------------------------------------------------------------------- */
/*ir2e71y_clear_normal_led                                                   */
/* ------------------------------------------------------------------------- */
static void ir2e71y_clean_normal_led(void) {

      ir2e71y_if_ctx.tri_led.red = 0;
      ir2e71y_if_ctx.tri_led.green = 0;
      ir2e71y_if_ctx.tri_led.blue = 0;
      ir2e71y_if_ctx.tri_led.led_mode = IR2E71Y_TRI_LED_MODE_NORMAL;
}

#endif /* IR2E71Y_SYSFS_LED */

/* ------------------------------------------------------------------------- */
/* OTHER                                                                     */
/* ------------------------------------------------------------------------- */
#if defined(CONFIG_ANDROID_ENGINEERING)
/* ------------------------------------------------------------------------- */
/*ir2e71y_dbg_API_set_recovery_check_error                                   */
/* ------------------------------------------------------------------------- */
static void ir2e71y_dbg_API_set_recovery_check_error(int flag, int count)
{
    ir2e71y_dbg_recovery_check_error_flag = flag;
    ir2e71y_dbg_recovery_check_error_count = count;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_dbg_API_get_recovery_check_error                                   */
/* ------------------------------------------------------------------------- */
int ir2e71y_dbg_API_get_recovery_check_error(void)
{
    return ir2e71y_dbg_recovery_check_error_flag;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_dbg_API_update_recovery_check_error                                */
/* ------------------------------------------------------------------------- */
void ir2e71y_dbg_API_update_recovery_check_error(int flag)
{
    if (ir2e71y_dbg_recovery_check_error_count != 0) {
        ir2e71y_dbg_recovery_check_error_count--;
        if (ir2e71y_dbg_recovery_check_error_count == 0) {
            ir2e71y_dbg_recovery_check_error_flag = flag;
        }
    }
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_proc_write                                                         */
/* ------------------------------------------------------------------------- */
#define PROC_BUF_LENGTH                 (4096)
#define PROC_BUF_REWIND_LENGTH          (4000)
#define IR2E71Y_DEBUG_CONSOLE(fmt, args...) \
        do { \
            int buflen = 0; \
            int remain = PROC_BUF_LENGTH - proc_buf_pos - 1; \
            if (remain > 0) { \
                buflen = snprintf(&proc_buf[proc_buf_pos], remain, fmt, ## args); \
                proc_buf_pos += (buflen > 0) ? buflen : 0; \
            } \
        } while (0)
static unsigned char proc_buf[PROC_BUF_LENGTH] = {0};
static unsigned int  proc_buf_pos = 0;

static int ir2e71y_proc_write(struct file *file, const char *buffer, unsigned long count, void *data)
{
#define IR2E71Y_LEN_ID                   (2)
#define IR2E71Y_LEN_PARAM                (4)
#define IR2E71Y_PARAM_MAX                (4)

    unsigned long len = count;
    struct ir2e71y_procfs ir2e71y_pfs;
    char buf[IR2E71Y_LEN_PARAM + 1];
    char kbuf[IR2E71Y_LEN_ID + IR2E71Y_PARAM_MAX * IR2E71Y_LEN_PARAM];
    int i;
    int ret = 0;
    struct ir2e71y_bdic_i2c_msg i2c_msg;
    unsigned char i2c_wbuf[6];
    unsigned char i2c_rbuf[6];
    struct ir2e71y_prox_params prox_params;
    struct ir2e71y_main_bkl_ctl bkl;
    unsigned char   val;

    char *kbufex;
    unsigned char *param = NULL;
    int paramlen = 0;
    int needalloc = 0;
#ifdef IR2E71Y_KEY_LED
    struct ir2e71y_key_bkl_ctl key_bkl;
#endif /* IR2E71Y_KEY_LED */
    struct ir2e71y_tri_led tri_led;
    int recovery_error_flag;
    int recovery_error_count;

    len--;
    /* Check length */
    if (len < IR2E71Y_LEN_ID) {
        return count;
    }
    if (len > (IR2E71Y_LEN_ID + IR2E71Y_PARAM_MAX * IR2E71Y_LEN_PARAM)) {
        len = IR2E71Y_LEN_ID + IR2E71Y_PARAM_MAX * IR2E71Y_LEN_PARAM;
        needalloc = 1;
    }

    if (copy_from_user(kbuf, buffer, len)) {
        return -EFAULT;
    }
    /* Get FunctionID */
    memcpy(buf, kbuf, IR2E71Y_LEN_ID);
    buf[IR2E71Y_LEN_ID] = '\0';
    ir2e71y_pfs.id = simple_strtol(buf, NULL, 10);
    ir2e71y_pfs.par[0] = 0;
    ir2e71y_pfs.par[1] = 0;
    ir2e71y_pfs.par[2] = 0;
    ir2e71y_pfs.par[3] = 0;

    /* Get Parameters */
    for (i = 0; (i + 1) * IR2E71Y_LEN_PARAM <= (len - IR2E71Y_LEN_ID); i++) {
        memcpy(buf, &(kbuf[IR2E71Y_LEN_ID + i * IR2E71Y_LEN_PARAM]), IR2E71Y_LEN_PARAM);
        buf[IR2E71Y_LEN_PARAM] = '\0';
        ir2e71y_pfs.par[i] = simple_strtol(buf, NULL, 16);
    }

    printk("[IR2E71Y] ir2e71y_proc_write(%d, 0x%04x, 0x%04x, 0x%04x, 0x%04x)\n", ir2e71y_pfs.id,
                                                                               ir2e71y_pfs.par[0], ir2e71y_pfs.par[1],
                                                                               ir2e71y_pfs.par[2], ir2e71y_pfs.par[3]);

    if (needalloc) {
        len = count - (IR2E71Y_LEN_ID + 1);
        if (len > (1024 * IR2E71Y_PARAM_MAX) - (IR2E71Y_LEN_ID + 1)) {
           len = (1024 * IR2E71Y_PARAM_MAX) - (IR2E71Y_LEN_ID + 1);
        }
        kbufex = kmalloc(len, GFP_KERNEL);
        if (!kbufex) {
            return -EFAULT;
        }
        buffer += IR2E71Y_LEN_ID;
        if (copy_from_user(kbufex, buffer, len)) {
            kfree(kbufex);
            return -EFAULT;
        }
        paramlen = len / (IR2E71Y_LEN_PARAM / 2);
        param = kmalloc(paramlen, GFP_KERNEL);
        if (!param) {
            kfree(kbufex);
            return -EFAULT;
        }
        /* Get Parameters */
        for (i = 0; i < paramlen; i++) {
            memcpy(buf, &(kbufex[i * (IR2E71Y_LEN_PARAM / 2)]), (IR2E71Y_LEN_PARAM / 2));
            buf[(IR2E71Y_LEN_PARAM / 2)] = '\0';
            param[i] = simple_strtol(buf, NULL, 16);
        }
        kfree(kbufex);
    }

    switch (ir2e71y_pfs.id) {
    case IR2E71Y_DEBUG_PROCESS_STATE_OUTPUT:
        ir2e71y_semaphore_start();
        ir2e71y_dbg_info_output((int)ir2e71y_pfs.par[0]);
        ir2e71y_semaphore_end(__func__);
        break;

    case IR2E71Y_DEBUG_TRACE_LOG_SWITCH:
        if (ir2e71y_pfs.par[0] == 0) {
            printk("[IR2E71Y] Trace log OFF\n");
        } else {
            printk("[IR2E71Y] Trace log ON(%d)\n", ir2e71y_pfs.par[0]);
        }
        IR2E71Y_SET_LOG_LV((unsigned short)ir2e71y_pfs.par[0]);
        IR2E71Y_DEBUG("TraceLog enable check!!");
        break;

    case IR2E71Y_DEBUG_BDIC_I2C_WRITE:
        printk("[IR2E71Y] BDIC-I2C WRITE (addr : 0x%02x, data : 0x%02x)\n", ir2e71y_pfs.par[0], ir2e71y_pfs.par[1]);
        i2c_wbuf[0] = ir2e71y_pfs.par[0];
        i2c_wbuf[1] = ir2e71y_pfs.par[1];

        i2c_msg.addr = IR2E71Y_BDIC_SENSOR_SLAVE_ADDR;
        i2c_msg.mode = IR2E71Y_BDIC_I2C_M_W;
        i2c_msg.wlen = 2;
        i2c_msg.rlen = 0;
        i2c_msg.wbuf = &i2c_wbuf[0];
        i2c_msg.rbuf = NULL;
        ir2e71y_semaphore_start();
        ret = ir2e71y_SQE_write_bdic_i2c(&i2c_msg);
        ir2e71y_semaphore_end(__func__);
        break;

    case IR2E71Y_DEBUG_BDIC_I2C_READ:
        printk("[IR2E71Y] BDIC-I2C READ (addr : 0x%02x)\n", ir2e71y_pfs.par[0]);
        i2c_wbuf[0] = ir2e71y_pfs.par[0];
        i2c_rbuf[0] = 0x00;

        i2c_msg.addr = IR2E71Y_BDIC_SENSOR_SLAVE_ADDR;
        i2c_msg.mode = IR2E71Y_BDIC_I2C_M_R;
        i2c_msg.wlen = 1;
        i2c_msg.rlen = 1;
        i2c_msg.wbuf = &i2c_wbuf[0];
        i2c_msg.rbuf = &i2c_rbuf[0];
        ir2e71y_semaphore_start();
        ret = ir2e71y_SQE_read_bdic_i2c(&i2c_msg);
        ir2e71y_semaphore_end(__func__);
        printk("[IR2E71Y] Read data(0x%02x)\n", i2c_rbuf[0]);
        IR2E71Y_DEBUG_CONSOLE("<COMMAND = I2C_READ>\n");
        IR2E71Y_DEBUG_CONSOLE("  IN     = 0x%02x\n", i2c_wbuf[0]);
        IR2E71Y_DEBUG_CONSOLE("  OUT    = 0x%02x\n", i2c_rbuf[0]);
        if (ret == IR2E71Y_RESULT_SUCCESS) {
            IR2E71Y_DEBUG_CONSOLE("  RESULT = OK\n");
        } else {
            IR2E71Y_DEBUG_CONSOLE("  RESULT = NG\n");
        }
        break;

    case IR2E71Y_DEBUG_PROX_SENSOR_CTL:
        ir2e71y_semaphore_start();
        switch (ir2e71y_pfs.par[0]) {
        case 0:
            ret = ir2e71y_SQE_prox_sensor_pow_ctl(IR2E71Y_PROX_SENSOR_POWER_OFF);
            break;
        case 1:
            printk("[IR2E71Y] POWER_ON_PARAM (LOW : %d, HIGH : %d)\n", ir2e71y_pfs.par[1], ir2e71y_pfs.par[2]);
            prox_params.threshold_low  = (unsigned int)ir2e71y_pfs.par[1];
            prox_params.threshold_high = (unsigned int)ir2e71y_pfs.par[2];

            ret = ir2e71y_SQE_prox_sensor_pow_ctl(IR2E71Y_PROX_SENSOR_POWER_ON);
            break;
        default:
            break;
        }
        ir2e71y_semaphore_end(__func__);
        break;

    case IR2E71Y_DEBUG_BKL_CTL:
        ir2e71y_semaphore_start();
        if (ir2e71y_pfs.par[0] == 0) {
            printk("[IR2E71Y] BKL_OFF\n");
            bkl.mode  = IR2E71Y_MAIN_BKL_MODE_OFF;
            bkl.param = IR2E71Y_MAIN_BKL_PARAM_OFF;
            ret = ir2e71y_SQE_main_bkl_ctl(IR2E71Y_MAIN_BKL_DEV_TYPE_APP, &bkl);
        } else {
            printk("[IR2E71Y] BKL_ON (mode : %d, param : %d)\n", ir2e71y_pfs.par[1], ir2e71y_pfs.par[2]);
            bkl.mode  = ir2e71y_pfs.par[1];
            bkl.param = ir2e71y_pfs.par[2];
            if (bkl.mode == IR2E71Y_MAIN_BKL_MODE_AUTO) {
                ret = ir2e71y_SQE_main_bkl_ctl(IR2E71Y_MAIN_BKL_DEV_TYPE_APP_AUTO, &bkl);
            } else {
                ret = ir2e71y_SQE_main_bkl_ctl(IR2E71Y_MAIN_BKL_DEV_TYPE_APP, &bkl);
            }
        }
        ir2e71y_semaphore_end(__func__);
        break;

    case IR2E71Y_DEBUG_LIGHT_SENSOR_CTL:
        ir2e71y_semaphore_start();
        switch (ir2e71y_pfs.par[0]) {
        case 0:
            ir2e71y_pm_API_als_user_manager(
                    IR2E71Y_DEV_TYPE_ALS_MASK & (unsigned long)ir2e71y_pfs.par[1],
                    IR2E71Y_DEV_REQ_OFF);
            break;
        case 1:
            ir2e71y_pm_API_als_user_manager(
                    IR2E71Y_DEV_TYPE_ALS_MASK & (unsigned long)ir2e71y_pfs.par[1],
                    IR2E71Y_DEV_REQ_ON);
            break;
        default:
            break;
        }
        ir2e71y_semaphore_end(__func__);
        break;

    case IR2E71Y_DEBUG_IRQ_LOGIC_CHK:
        i = ir2e71y_pfs.par[0];
        printk("[IR2E71Y] ir2e71y_proc_write(%d):Test Pattern=%d\n", ir2e71y_pfs.id, i);
        ir2e71y_dbg_que(i);
        break;

    case IR2E71Y_DEBUG_BDIC_IRQ_ALL_CLEAR:
        printk("[IR2E71Y] ir2e71y_proc_write(%d):Interrupt Clear All\n", ir2e71y_pfs.id);
        ir2e71y_semaphore_start();
        ir2e71y_bdic_API_IRQ_dbg_Clear_All();
        ir2e71y_semaphore_end(__func__);
        break;

    case IR2E71Y_DEBUG_BDIC_IRQ_CLEAR:
        printk("[IR2E71Y] ir2e71y_proc_write(%d):Interrupt Clear\n", ir2e71y_pfs.id);
        ir2e71y_semaphore_start();
        ir2e71y_bdic_API_IRQ_Clear();
        ir2e71y_semaphore_end(__func__);
        break;

    case IR2E71Y_DEBUG_BDIC_WRITE:
        printk("[IR2E71Y] ir2e71y_proc_write(%d): BDIC register write\n", ir2e71y_pfs.id);
        val = ir2e71y_pfs.par[0] & 0x00FF;
        printk("[IR2E71Y] ir2e71y_SQE_bdic_write_reg() reg=0x%02x val=0x%02x\n", ((ir2e71y_pfs.par[0] >> 8) & 0x00FF),
                                                                               val);
        ir2e71y_semaphore_start();
        ir2e71y_SQE_bdic_write_reg(((ir2e71y_pfs.par[0] >> 8) & 0x00FF), val);
        ir2e71y_semaphore_end(__func__);
        break;

    case IR2E71Y_DEBUG_BDIC_READ:
        printk("[IR2E71Y] ir2e71y_proc_write(%d): BDIC register read\n", ir2e71y_pfs.id);
        val = 0;
        printk("[IR2E71Y] ir2e71y_SQE_bdic_read_reg() reg=0x%02x\n", ((ir2e71y_pfs.par[0] >> 8) & 0x00FF));
        ir2e71y_semaphore_start();
        ret = ir2e71y_SQE_bdic_read_reg(((ir2e71y_pfs.par[0] >> 8) & 0x00FF), &val);
        ir2e71y_semaphore_end(__func__);
        printk("[IR2E71Y] value=0x%02x\n", val);
        IR2E71Y_DEBUG_CONSOLE("<COMMAND = BDIC_register_READ>\n");
        IR2E71Y_DEBUG_CONSOLE("  IN     = 0x%02x\n", ((ir2e71y_pfs.par[0] >> 8) & 0x00FF));
        IR2E71Y_DEBUG_CONSOLE("  OUT    = 0x%02x\n", val);
        if (ret == IR2E71Y_RESULT_SUCCESS) {
            IR2E71Y_DEBUG_CONSOLE("  RESULT = OK\n");
        } else {
            IR2E71Y_DEBUG_CONSOLE("  RESULT = NG\n");
        }
        break;

    case IR2E71Y_DEBUG_RGB_LED:
#if defined(IR2E71Y_ILLUMI_TRIPLE_COLOR_LED) && defined(IR2E71Y_ANIME_COLOR_LED)
        if (ir2e71y_bdic_API_LED_is_running_illumi_triple_color()) {
            ir2e71y_bdic_API_LED_clear_illumi_triple_color();
        }
#endif /* IR2E71Y_ILLUMI_TRIPLE_COLOR_LED && IR2E71Y_ANIME_COLOR_LED */
        tri_led.red      = ((ir2e71y_pfs.par[0] >> 8) & 0x00FF);
        tri_led.green    = ( ir2e71y_pfs.par[0]       & 0x00FF);
        tri_led.blue     = ((ir2e71y_pfs.par[1] >> 8) & 0x00FF);
        tri_led.ext_mode = ( ir2e71y_pfs.par[1]       & 0x00FF);
        tri_led.led_mode = ((ir2e71y_pfs.par[2] >> 8) & 0x00FF);
        tri_led.ontime   = ( ir2e71y_pfs.par[2]       & 0x00FF);
        tri_led.interval = ((ir2e71y_pfs.par[3] >> 8) & 0x00FF);
        tri_led.count    = ( ir2e71y_pfs.par[3]       & 0x00FF);
        ir2e71y_semaphore_start();
        ret = ir2e71y_SQE_tri_led_set_color(&tri_led);
        ir2e71y_semaphore_end(__func__);
        break;

    case IR2E71Y_DEBUG_LED_REG_DUMP:
        ir2e71y_semaphore_start();
        if (ir2e71y_pfs.par[0] == 1) {
            ir2e71y_bdic_API_TRI_LED2_INFO_output();
        } else {
            ir2e71y_bdic_API_TRI_LED_INFO_output();
        }
        ir2e71y_semaphore_end(__func__);
        break;

    case IR2E71Y_DEBUG_BDIC_RESTART:
        ir2e71y_semaphore_start();
        ir2e71y_event_unsubscribe(IR2E71Y_IRQ_TYPE_I2CERR);
        ir2e71y_event_unsubscribe(IR2E71Y_IRQ_TYPE_DET);
        ir2e71y_pm_API_bdic_shutdown();
        ir2e71y_bdic_API_boot_init(NULL);
        ir2e71y_bdic_API_boot_init_2nd();
        ir2e71y_pm_API_bdic_resume();
        ir2e71y_semaphore_end(__func__);
        break;

    case IR2E71Y_DEBUG_RECOVERY_NG:
        if (ir2e71y_pfs.par[0] == 3) {
            recovery_error_flag = IR2E71Y_DBG_RECOVERY_ERROR_PSALS;
            printk("[IR2E71Y] set recovery check error psals\n");
        } else if (ir2e71y_pfs.par[0] == 5) {
            recovery_error_flag = IR2E71Y_DBG_BDIC_ERROR_DCDC1;
            printk("[IR2E71Y] set bdic dcdc1 error\n");
        } else {
            recovery_error_flag = IR2E71Y_DBG_RECOVERY_ERROR_NONE;
            printk("[IR2E71Y] set recovery check error none\n");
        }

        recovery_error_count = ir2e71y_pfs.par[1];
        printk("[IR2E71Y] set recovery check error retry count=%d\n", recovery_error_count);

        ir2e71y_dbg_API_set_recovery_check_error(recovery_error_flag, recovery_error_count);
        break;

#ifdef IR2E71Y_ALS_INT
    case IR2E71Y_DEBUG_ALS_REPORT:
        val = ir2e71y_pfs.par[0];
        IR2E71Y_DEBUG("value    : %X", val);
        ret = ir2e71y_input_subsystem_report(val);
        IR2E71Y_DEBUG("ret    : %d\n", ret);
        break;
#endif /* IR2E71Y_ALS_INT */

    default:
        break;
    }

    printk("[IR2E71Y] result : %d.\n", ret);

    if (needalloc) {
        kfree(param);
    }
    return count;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_proc_read                                                          */
/* ------------------------------------------------------------------------- */
static int ir2e71y_proc_read(char *page, char **start, off_t offset, int count, int *eof, void *data)
{
    int len = 0;

    len += snprintf(page, count, "%s", proc_buf);
    proc_buf[0] = 0;
    proc_buf_pos = 0;

    return len;
}

#define PROC_BLOCK_SIZE    (PAGE_SIZE - 1024)
/* ------------------------------------------------------------------------- */
/*ir2e71y_proc_file_read                                                     */
/* ------------------------------------------------------------------------- */
static ssize_t ir2e71y_proc_file_read(struct file *file, char __user *buf, size_t nbytes, loff_t *ppos)
{
    char    *page;
    ssize_t retval=0;
    int eof=0;
    ssize_t n, count;
    char    *start;
    unsigned long long pos;

    /*
     * Gaah, please just use "seq_file" instead. The legacy /proc
     * interfaces cut loff_t down to off_t for reads, and ignore
     * the offset entirely for writes..
     */
    pos = *ppos;
    if (pos > MAX_NON_LFS) {
        return 0;
    }
    if (nbytes > (MAX_NON_LFS - pos)) {
        nbytes = MAX_NON_LFS - pos;
    }

    if (!(page = (char*) __get_free_page(GFP_TEMPORARY))) {
        return -ENOMEM;
    }

    while ((nbytes > 0) && !eof) {
        count = min_t(size_t, PROC_BLOCK_SIZE, nbytes);

        start = NULL;
        n = ir2e71y_proc_read(page, &start, *ppos,
                  count, &eof, NULL);

        if (n == 0) {    /* end of file */
            break;
        }
        if (n < 0) {  /* error */
            if (retval == 0)
                retval = n;
            break;
        }

        if (start == NULL) {
            if (n > PAGE_SIZE) {
                printk(KERN_ERR
                       "proc_file_read: Apparent buffer overflow!\n");
                n = PAGE_SIZE;
            }
            n -= *ppos;
            if (n <= 0)
                break;
            if (n > count)
                n = count;
            start = page + *ppos;
        } else if (start < page) {
            if (n > PAGE_SIZE) {
                printk(KERN_ERR
                       "proc_file_read: Apparent buffer overflow!\n");
                n = PAGE_SIZE;
            }
            if (n > count) {
                /*
                 * Don't reduce n because doing so might
                 * cut off part of a data block.
                 */
                printk(KERN_WARNING
                       "proc_file_read: Read count exceeded\n");
            }
        } else /* start >= page */ {
            unsigned long startoff = (unsigned long)(start - page);
            if (n > (PAGE_SIZE - startoff)) {
                printk(KERN_ERR
                       "proc_file_read: Apparent buffer overflow!\n");
                n = PAGE_SIZE - startoff;
            }
            if (n > count) {
                n = count;
            }
        }

        n -= copy_to_user(buf, start < page ? page : start, n);
        if (n == 0) {
            if (retval == 0) {
                retval = -EFAULT;
            }
            break;
        }

        *ppos += start < page ? (unsigned long)start : n;
        nbytes -= n;
        buf += n;
        retval += n;
    }
    free_page((unsigned long) page);

    return retval;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_proc_file_write                                                    */
/* ------------------------------------------------------------------------- */
static ssize_t ir2e71y_proc_file_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    ssize_t rv = -EIO;

    rv = ir2e71y_proc_write(file, buffer, count, NULL);

    return rv;
}
#endif /* CONFIG_ANDROID_ENGINEERING */


#if defined(CONFIG_ANDROID_ENGINEERING)
/* ------------------------------------------------------------------------- */
/*ir2e71y_dbg_info_output                                                    */
/* ------------------------------------------------------------------------- */
static void ir2e71y_dbg_info_output(int mode)
{
    int i;

    switch (mode) {
    case IR2E71Y_DEBUG_INFO_TYPE_BOOT:
        printk("[IR2E71Y] BOOT INFO ->>\n");

        printk("[IR2E71Y] BOOT INFO <<-\n");
        break;
    case IR2E71Y_DEBUG_INFO_TYPE_KERNEL:
        printk("[IR2E71Y] KERNEL INFO ->>\n");
        printk("[IR2E71Y] sizeof(ir2e71y_if_ctx)                = %d.\n", sizeof(ir2e71y_if_ctx));
        printk("[IR2E71Y] if_ctx.driver_is_open                = %d.\n", ir2e71y_if_ctx.driver_is_open);
        printk("[IR2E71Y] if_ctx.driver_open_cnt               = %d.\n", ir2e71y_if_ctx.driver_open_cnt);
        printk("[IR2E71Y] if_ctx.driver_is_initialized         = %d.\n", ir2e71y_if_ctx.driver_is_initialized);
        printk("[IR2E71Y] if_ctx.thermal_status                = %d.\n", ir2e71y_if_ctx.thermal_status);
        printk("[IR2E71Y] if_ctx.usb_chg_status                = %d.\n", ir2e71y_if_ctx.usb_chg_status);
        printk("[IR2E71Y] if_ctx.main_disp_status              = %d.\n", ir2e71y_if_ctx.main_disp_status);
        printk("[IR2E71Y] if_ctx.main_bkl.mode                 = %d.\n", ir2e71y_if_ctx.main_bkl.mode);
        printk("[IR2E71Y] if_ctx.main_bkl.param                = %d.\n", ir2e71y_if_ctx.main_bkl.param);
        printk("[IR2E71Y] if_ctx.tri_led.red                   = %d.\n", (int)ir2e71y_if_ctx.tri_led.red);
        printk("[IR2E71Y] if_ctx.tri_led.green                 = %d.\n", (int)ir2e71y_if_ctx.tri_led.green);
        printk("[IR2E71Y] if_ctx.tri_led.blue                  = %d.\n", (int)ir2e71y_if_ctx.tri_led.blue);
        printk("[IR2E71Y] if_ctx.tri_led.ext_mode              = %d.\n", ir2e71y_if_ctx.tri_led.ext_mode);
        printk("[IR2E71Y] if_ctx.tri_led.led_mode              = %d.\n", ir2e71y_if_ctx.tri_led.led_mode);
        printk("[IR2E71Y] if_ctx.tri_led.ontime                = %d.\n", ir2e71y_if_ctx.tri_led.ontime);
        printk("[IR2E71Y] if_ctx.tri_led.interval              = %d.\n", ir2e71y_if_ctx.tri_led.interval);
        printk("[IR2E71Y] if_ctx.tri_led.count                 = %d.\n", ir2e71y_if_ctx.tri_led.count);
#ifdef IR2E71Y_SYSFS_LED
        printk("[IR2E71Y] if_ctx.sysfs_led1.red                = %d.\n", (int)ir2e71y_if_ctx.sysfs_led1.red);
        printk("[IR2E71Y] if_ctx.sysfs_led1.green              = %d.\n", (int)ir2e71y_if_ctx.sysfs_led1.green);
        printk("[IR2E71Y] if_ctx.sysfs_led1.blue               = %d.\n", (int)ir2e71y_if_ctx.sysfs_led1.blue);
        printk("[IR2E71Y] if_ctx.sysfs_led1.ext_mode           = %d.\n", ir2e71y_if_ctx.sysfs_led1.ext_mode);
        printk("[IR2E71Y] if_ctx.sysfs_led1.led_mode           = %d.\n", ir2e71y_if_ctx.sysfs_led1.led_mode);
        printk("[IR2E71Y] if_ctx.sysfs_led1.ontime             = %d.\n", ir2e71y_if_ctx.sysfs_led1.ontime);
        printk("[IR2E71Y] if_ctx.sysfs_led1.interval           = %d.\n", ir2e71y_if_ctx.sysfs_led1.interval);
        printk("[IR2E71Y] if_ctx.sysfs_led1.count              = %d.\n", ir2e71y_if_ctx.sysfs_led1.count);
#ifdef IR2E71Y_COLOR_LED_TWIN
        printk("[IR2E71Y] if_ctx.sysfs_led2.red                = %d.\n", (int)ir2e71y_if_ctx.sysfs_led2.red);
        printk("[IR2E71Y] if_ctx.sysfs_led2.green              = %d.\n", (int)ir2e71y_if_ctx.sysfs_led2.green);
        printk("[IR2E71Y] if_ctx.sysfs_led2.blue               = %d.\n", (int)ir2e71y_if_ctx.sysfs_led2.blue);
        printk("[IR2E71Y] if_ctx.sysfs_led2.ext_mode           = %d.\n", ir2e71y_if_ctx.sysfs_led2.ext_mode);
        printk("[IR2E71Y] if_ctx.sysfs_led2.led_mode           = %d.\n", ir2e71y_if_ctx.sysfs_led2.led_mode);
        printk("[IR2E71Y] if_ctx.sysfs_led2.ontime             = %d.\n", ir2e71y_if_ctx.sysfs_led2.ontime);
        printk("[IR2E71Y] if_ctx.sysfs_led2.interval           = %d.\n", ir2e71y_if_ctx.sysfs_led2.interval);
        printk("[IR2E71Y] if_ctx.sysfs_led2.count              = %d.\n", ir2e71y_if_ctx.sysfs_led2.count);
#endif /* IR2E71Y_COLOR_LED_TWIN */
#endif /* IR2E71Y_SYSFS_LED */
#ifdef IR2E71Y_LED_INT
        printk("[IR2E71Y] if_ctx.led_auto_low_enable           = %d.\n", ir2e71y_if_ctx.led_auto_low_enable);
#endif /* IR2E71Y_LED_INT */
        printk("[IR2E71Y] if_ctx.led_set_color_reject          = %d.\n", ir2e71y_if_ctx.led_set_color_reject);
        printk("\n");

        for (i = 0; i < NUM_IR2E71Y_IRQ_TYPE ; i++) {
            if (ir2e71y_callback_table[i] != NULL) {
                printk("[IR2E71Y] ir2e71y_callback_table[%d]              = subscribed.\n", i);
            } else {
                printk("[IR2E71Y] ir2e71y_callback_table[%d]              = no subscribed.\n", i);
            }
        }

        printk("[IR2E71Y] if_ctx.shutdown_in_progress          = %d.\n", ir2e71y_if_ctx.shutdown_in_progress);

        printk("[IR2E71Y] KERNEL INFO <<-\n");
        break;

    case IR2E71Y_DEBUG_INFO_TYPE_POWERON:
        printk("[IR2E71Y] BOOT INFO ->>\n");
        printk("[IR2E71Y] sizeof(boot_ctx)                       = %ld.\n", (long)sizeof(ir2e71y_if_ctx.boot_ctx));
        printk("[IR2E71Y] boot_ctx.driver_is_initialized         = %d.\n", ir2e71y_if_ctx.boot_ctx.driver_is_initialized);
        printk("[IR2E71Y] boot_ctx.hw_handset                    = %d.\n", (int)ir2e71y_if_ctx.boot_ctx.hw_handset);
        printk("[IR2E71Y] boot_ctx.hw_revision                   = %d.\n", (int)ir2e71y_if_ctx.boot_ctx.hw_revision);
        printk("[IR2E71Y] boot_ctx.device_code                   = %d.\n", (int)ir2e71y_if_ctx.boot_ctx.device_code);
        printk("[IR2E71Y] boot_ctx.disp_on_status                = %d.\n", (int)ir2e71y_if_ctx.boot_ctx.disp_on_status);
        printk("[IR2E71Y] boot_ctx.handset_color                 = %d.\n", ir2e71y_if_ctx.boot_ctx.handset_color);
        printk("[IR2E71Y] boot_ctx.upper_unit_is_connected       = %d.\n", ir2e71y_if_ctx.boot_ctx.upper_unit_is_connected);
        printk("[IR2E71Y] boot_ctx.main_disp_status              = %d.\n", ir2e71y_if_ctx.boot_ctx.main_disp_status);
        printk("[IR2E71Y] boot_ctx.is_trickled                   = %d.\n", ir2e71y_if_ctx.boot_ctx.is_trickled);
        printk("[IR2E71Y] boot_ctx.main_bkl.mode                 = %d.\n", ir2e71y_if_ctx.boot_ctx.main_bkl.mode);
        printk("[IR2E71Y] boot_ctx.main_bkl.param                = %d.\n", ir2e71y_if_ctx.boot_ctx.main_bkl.param);
        printk("[IR2E71Y] boot_ctx.tri_led.red                   = %d.\n", (int)ir2e71y_if_ctx.boot_ctx.tri_led.red);
        printk("[IR2E71Y] boot_ctx.tri_led.green                 = %d.\n", (int)ir2e71y_if_ctx.boot_ctx.tri_led.green);
        printk("[IR2E71Y] boot_ctx.tri_led.blue                  = %d.\n", (int)ir2e71y_if_ctx.boot_ctx.tri_led.blue);
        printk("[IR2E71Y] boot_ctx.bdic_is_exist                 = %d.\n", ir2e71y_if_ctx.boot_ctx.bdic_is_exist);
        printk("[IR2E71Y] boot_ctx.photo_sensor_adj.status       = 0x%02X.\n",
                (unsigned int)ir2e71y_if_ctx.boot_ctx.photo_sensor_adj.status);
        printk("[IR2E71Y] boot_ctx.photo_sensor_adj.als_adj0     = 0x%04X.\n",
                (unsigned int)ir2e71y_if_ctx.boot_ctx.photo_sensor_adj.als_adjust[0].als_adj0);
        printk("[IR2E71Y] boot_ctx.photo_sensor_adj.als_adj1     = 0x%04X.\n",
                (unsigned int)ir2e71y_if_ctx.boot_ctx.photo_sensor_adj.als_adjust[0].als_adj1);
        printk("[IR2E71Y] boot_ctx.photo_sensor_adj.als_shift    = 0x%02X.\n",
                (unsigned int)ir2e71y_if_ctx.boot_ctx.photo_sensor_adj.als_adjust[0].als_shift);
        printk("[IR2E71Y] boot_ctx.photo_sensor_adj.clear_offset = 0x%02X.\n",
                (unsigned int)ir2e71y_if_ctx.boot_ctx.photo_sensor_adj.als_adjust[0].clear_offset);
        printk("[IR2E71Y] boot_ctx.photo_sensor_adj.ir_offset    = 0x%02X.\n",
                (unsigned int)ir2e71y_if_ctx.boot_ctx.photo_sensor_adj.als_adjust[0].ir_offset);
        printk("[IR2E71Y] boot_ctx.photo_sensor_adj.als_adj0     = 0x%04X.\n",
                (unsigned int)ir2e71y_if_ctx.boot_ctx.photo_sensor_adj.als_adjust[1].als_adj0);
        printk("[IR2E71Y] boot_ctx.photo_sensor_adj.als_adj1     = 0x%04X.\n",
                (unsigned int)ir2e71y_if_ctx.boot_ctx.photo_sensor_adj.als_adjust[1].als_adj1);
        printk("[IR2E71Y] boot_ctx.photo_sensor_adj.als_shift    = 0x%02X.\n",
                (unsigned int)ir2e71y_if_ctx.boot_ctx.photo_sensor_adj.als_adjust[1].als_shift);
        printk("[IR2E71Y] boot_ctx.photo_sensor_adj.clear_offset = 0x%02X.\n",
                (unsigned int)ir2e71y_if_ctx.boot_ctx.photo_sensor_adj.als_adjust[1].clear_offset);
        printk("[IR2E71Y] boot_ctx.photo_sensor_adj.ir_offset    = 0x%02X.\n",
                (unsigned int)ir2e71y_if_ctx.boot_ctx.photo_sensor_adj.als_adjust[1].ir_offset);
        printk("[IR2E71Y] boot_ctx.photo_sensor_adj.chksum       = 0x%06X.\n",
                (unsigned int)ir2e71y_if_ctx.boot_ctx.photo_sensor_adj.chksum);
        printk("[IR2E71Y] boot_ctx.bdic_chipver                  = %d.\n", ir2e71y_if_ctx.boot_ctx.bdic_chipver);
        printk("[IR2E71Y] boot_ctx.bdic_status.power_status      = %d.\n", ir2e71y_if_ctx.boot_ctx.bdic_status.power_status);
        printk("[IR2E71Y] boot_ctx.bdic_status.users             = %d.\n", (int)ir2e71y_if_ctx.boot_ctx.bdic_status.users);
        printk("[IR2E71Y] boot_ctx.psals_status.power_status     = %d.\n", ir2e71y_if_ctx.boot_ctx.psals_status.power_status);
        printk("[IR2E71Y] boot_ctx.psals_status.als_users        = %d.\n", (int)ir2e71y_if_ctx.boot_ctx.psals_status.als_users);
        printk("[IR2E71Y] boot_ctx.psals_status.ps_um_status     = %d.\n", ir2e71y_if_ctx.boot_ctx.psals_status.ps_um_status);
        printk("[IR2E71Y] boot_ctx.psals_status.als_um_status    = %d.\n", ir2e71y_if_ctx.boot_ctx.psals_status.als_um_status);
        printk("[IR2E71Y] BOOT INFO <<-\n");
        printk("[IR2E71Y] KERNEL INFO ->>\n");
        printk("[IR2E71Y] sizeof(ir2e71y_if_ctx)                = %d.\n", sizeof(ir2e71y_if_ctx));
        printk("[IR2E71Y] if_ctx.driver_is_open                = %d.\n", ir2e71y_if_ctx.driver_is_open);
        printk("[IR2E71Y] if_ctx.driver_open_cnt               = %d.\n", ir2e71y_if_ctx.driver_open_cnt);
        printk("[IR2E71Y] if_ctx.driver_is_initialized         = %d.\n", ir2e71y_if_ctx.driver_is_initialized);
        printk("[IR2E71Y] if_ctx.thermal_status                = %d.\n", ir2e71y_if_ctx.thermal_status);
        printk("[IR2E71Y] if_ctx.usb_chg_status                = %d.\n", ir2e71y_if_ctx.usb_chg_status);
        printk("[IR2E71Y] if_ctx.main_disp_status              = %d.\n", ir2e71y_if_ctx.main_disp_status);
        printk("[IR2E71Y] if_ctx.main_bkl.mode                 = %d.\n", ir2e71y_if_ctx.main_bkl.mode);
        printk("[IR2E71Y] if_ctx.main_bkl.param                = %d.\n", ir2e71y_if_ctx.main_bkl.param);
        printk("[IR2E71Y] if_ctx.tri_led.red                   = %d.\n", (int)ir2e71y_if_ctx.tri_led.red);
        printk("[IR2E71Y] if_ctx.tri_led.green                 = %d.\n", (int)ir2e71y_if_ctx.tri_led.green);
        printk("[IR2E71Y] if_ctx.tri_led.blue                  = %d.\n", (int)ir2e71y_if_ctx.tri_led.blue);
        printk("[IR2E71Y] if_ctx.tri_led.ext_mode              = %d.\n", ir2e71y_if_ctx.tri_led.ext_mode);
        printk("[IR2E71Y] if_ctx.tri_led.led_mode              = %d.\n", ir2e71y_if_ctx.tri_led.led_mode);
        printk("[IR2E71Y] if_ctx.tri_led.ontime                = %d.\n", ir2e71y_if_ctx.tri_led.ontime);
        printk("[IR2E71Y] if_ctx.tri_led.interval              = %d.\n", ir2e71y_if_ctx.tri_led.interval);
        printk("[IR2E71Y] if_ctx.tri_led.count                 = %d.\n", ir2e71y_if_ctx.tri_led.count);
        printk("[IR2E71Y] KERNEL INFO <<-\n");
        break;

    case IR2E71Y_DEBUG_INFO_TYPE_BDIC:
        ir2e71y_bdic_API_DBG_INFO_output();
        break;
    case IR2E71Y_DEBUG_INFO_TYPE_SENSOR:
        ir2e71y_bdic_API_PSALS_INFO_output();
        break;
    case IR2E71Y_DEBUG_INFO_TYPE_PM:
        ir2e71y_pm_API_power_manager_users_dump();
        break;
    case IR2E71Y_DEBUG_INFO_TYPE_BDIC_OPT:
        ir2e71y_bdic_API_OPT_INFO_output();
        break;
    default:
        break;
    }

    return;
}
#endif /* CONFIG_ANDROID_ENGINEERING */

#if defined(CONFIG_ANDROID_ENGINEERING)
/* ------------------------------------------------------------------------- */
/*ir2e71y_dbg_que                                                            */
/* ------------------------------------------------------------------------- */
static void ir2e71y_dbg_que(int kind)
{
    unsigned int nRcvGFAC = 0;
    struct ir2e71y_queue_data_t *qdata = NULL;
    int    i;
    int    bFirstQue = 0;
    int    ret;
    int    nBDIC_QueFac = 0;


    IR2E71Y_TRACE(": Start");

    switch (kind) {
    case 1:
        nRcvGFAC = 0x00000000;
        break;
    case 2:
        nRcvGFAC = 0x00200000;
        break;
    case 3:
        nRcvGFAC = 0x00000100;
        break;
    case 4:
        nRcvGFAC = 0x00200100;
        break;
    case 5:
        nRcvGFAC = 0x00000008;
        break;
    case 6:
        nRcvGFAC = 0x00200008;
        break;
    case 7:
        nRcvGFAC = 0x00000108;
        break;
    case 8:
        nRcvGFAC = 0x00200108;
        break;
    case 9:
        nRcvGFAC = 0x00080000;
        break;
    case 10:
        nRcvGFAC = 0x00280000;
        break;
    case 11:
        nRcvGFAC = 0x00080100;
        break;
    case 12:
        nRcvGFAC = 0x00280100;
        break;
    case 13:
        nRcvGFAC = 0x00080008;
        break;
    case 14:
        nRcvGFAC = 0x00280008;
        break;
    case 15:
        nRcvGFAC = 0x00080108;
        break;
    case 16:
        nRcvGFAC = 0x00280108;
        break;
    case 17:
        nRcvGFAC = 0x00040000;
        break;
    case 18:
        nRcvGFAC = 0x00240000;
        break;
    case 19:
        nRcvGFAC = 0x00040100;
        break;
    case 20:
        nRcvGFAC = 0x00240100;
        break;
    case 21:
        nRcvGFAC = 0x00040008;
        break;
    case 22:
        nRcvGFAC = 0x00240008;
        break;
    case 23:
        nRcvGFAC = 0x00040108;
        break;
    case 24:
        nRcvGFAC = 0x00240108;
        break;
    case 25:
        nRcvGFAC = 0x000C0000;
        break;
    case 26:
        nRcvGFAC = 0x002C0000;
        break;
    case 27:
        nRcvGFAC = 0x000C0100;
        break;
    case 28:
        nRcvGFAC = 0x002C0100;
        break;
    case 29:
        nRcvGFAC = 0x000C0008;
        break;
    case 30:
        nRcvGFAC = 0x002C0008;
        break;
    case 31:
        nRcvGFAC = 0x000C0108;
        break;
    case 32:
        nRcvGFAC = 0x002C0108;
        break;
    case 33:
        nRcvGFAC = 0x00000200;
        break;
    case 34:
        nRcvGFAC = 0x00080200;
        break;
    case 35:
        nRcvGFAC = 0x00200200;
        break;
    case 36:
        nRcvGFAC = 0x00280200;
        break;
    case 37:
        nRcvGFAC = 0x00000300;
        break;
    case 38:
        nRcvGFAC = 0x00080300;
        break;
    case 39:
        nRcvGFAC = 0x00200300;
        break;
    case 40:
        nRcvGFAC = 0x00280300;
        break;
    case 41:
        nRcvGFAC = 0x00000208;
        break;
    case 42:
        nRcvGFAC = 0x00080208;
        break;
    case 43:
        nRcvGFAC = 0x00200208;
        break;
    case 44:
        nRcvGFAC = 0x00280208;
        break;
    case 45:
        nRcvGFAC = 0x00000308;
        break;
    case 46:
        nRcvGFAC = 0x00080308;
        break;
    case 47:
        nRcvGFAC = 0x00200308;
        break;
    case 48:
        nRcvGFAC = 0x00280308;
        break;
    case 49:
        nRcvGFAC = 0x00040200;
        break;
    case 50:
        nRcvGFAC = 0x000C0200;
        break;
    case 51:
        nRcvGFAC = 0x00240200;
        break;
    case 52:
        nRcvGFAC = 0x002C0200;
        break;
    case 53:
        nRcvGFAC = 0x00040300;
        break;
    case 54:
        nRcvGFAC = 0x000C0300;
        break;
    case 55:
        nRcvGFAC = 0x00240300;
        break;
    case 56:
        nRcvGFAC = 0x002C0300;
        break;
    case 57:
        nRcvGFAC = 0x00040208;
        break;
    case 58:
        nRcvGFAC = 0x000C0208;
        break;
    case 59:
        nRcvGFAC = 0x00240208;
        break;
    case 60:
        nRcvGFAC = 0x002C0208;
        break;
    case 61:
        nRcvGFAC = 0x00040308;
        break;
    case 62:
        nRcvGFAC = 0x000C0308;
        break;
    case 63:
        nRcvGFAC = 0x00240308;
        break;
    case 64:
        nRcvGFAC = 0x002C0308;
        break;

    default:
        nRcvGFAC = 0;
        break;
    }

    ir2e71y_IO_API_set_irq(IR2E71Y_IRQ_DISABLE);
    ir2e71y_wake_lock();

    ir2e71y_bdic_API_IRQ_dbg_set_fac(nRcvGFAC);

    do {
        ir2e71y_semaphore_start();
        ret = ir2e71y_bdic_API_IRQ_check_fac();
        ir2e71y_semaphore_end(__func__);
        if (ret != IR2E71Y_RESULT_SUCCESS) {
            IR2E71Y_DEBUG(": no factory");
            break;
        }

        down(&ir2e71y_sem_irq_fac);
        for (i = 0; i < IR2E71Y_IRQ_MAX_KIND; i++) {
            ir2e71y_semaphore_start();
            nBDIC_QueFac = ir2e71y_bdic_API_IRQ_get_fac(i);
            ir2e71y_semaphore_end(__func__);
            if (nBDIC_QueFac == IR2E71Y_BDIC_IRQ_TYPE_NONE) {
                break;
            }

            if (ir2e71y_wq_gpio_task) {
                qdata = kmalloc(sizeof(ir2e71y_queue_data), GFP_KERNEL);
                if (qdata != NULL) {
                    qdata->irq_GFAC = nBDIC_QueFac;
                    list_add_tail(&qdata->list, &ir2e71y_queue_data.list);
                    if (bFirstQue == 0) {
                        bFirstQue = 1;
                        ir2e71y_wake_lock();
                        ret = queue_work(ir2e71y_wq_gpio_task, &ir2e71y_wq_gpio_task_wk);
                        if (ret == 0) {
                            ir2e71y_wake_unlock();
                            IR2E71Y_ERR("<QUEUE_WORK_FAILURE> ");
                        }
                    }
                } else {
                   IR2E71Y_ERR("<QUEUE_WORK_FAILURE> :kmalloc failed (BDIC_QueFac=%d)", nBDIC_QueFac);
                }
            }
        }
        up(&ir2e71y_sem_irq_fac);

    } while (0);

    ir2e71y_semaphore_start();
    ir2e71y_bdic_API_IRQ_Clear();
    ir2e71y_semaphore_end(__func__);

    if (ir2e71y_bdic_API_IRQ_check_DET() != IR2E71Y_BDIC_IRQ_TYPE_DET) {
        ir2e71y_IO_API_set_irq(IR2E71Y_IRQ_ENABLE);
    }

    IR2E71Y_TRACE(": Finish");
    ir2e71y_wake_unlock();
}

#endif /* CONFIG_ANDROID_ENGINEERING */


/* ------------------------------------------------------------------------- */
/*ir2e71y_fb_close                                                           */
/* ------------------------------------------------------------------------- */
static void ir2e71y_fb_close(void)
{
    struct fb_info *info = NULL;

    info = registered_fb[0];
    if (!info) {
        return;
    }
    if (info->fbops->fb_release) {
        info->fbops->fb_release(info, 0);
    }
    module_put(info->fbops->owner);
    return;
}

#ifdef IR2E71Y_ALS_INT
/* ------------------------------------------------------------------------- */
/*ir2e71y_input_subsystem_init                                               */
/* ------------------------------------------------------------------------- */
static int ir2e71y_input_subsystem_init(void)
{
    int ret;

    wake_lock_init(&ir2e71y_timeout_wake_lock, WAKE_LOCK_SUSPEND, "ir2e71y_timeout_wake_lock");

    ir2e71y_input_dev = input_allocate_device();
    if (!ir2e71y_input_dev) {
        IR2E71Y_ERR("could not allocate input device\n");
        ret = -ENOMEM;
    }

    ir2e71y_input_dev->name = "als_interrupt";
    set_bit(EV_ABS, ir2e71y_input_dev->evbit);
    input_set_abs_params(ir2e71y_input_dev, ABS_MISC, 0, 9, 0, 0);

    ret = input_register_device(ir2e71y_input_dev);
    if (ret < 0) {
        IR2E71Y_ERR("can not register ls input device\n");
        input_free_device(ir2e71y_input_dev);
    }
    return ret;
}
/* ------------------------------------------------------------------------- */
/*ir2e71y_input_subsystem_report                                             */
/* ------------------------------------------------------------------------- */
static int ir2e71y_input_subsystem_report(int val)
{
    int ret = 0;
    IR2E71Y_TRACE("in");

    wake_lock_timeout(&ir2e71y_timeout_wake_lock, 1 * HZ);

    input_report_abs(ir2e71y_input_dev, ABS_MISC, val);
    input_sync(ir2e71y_input_dev);

    input_report_abs(ir2e71y_input_dev, ABS_MISC, 0x00);
    input_sync(ir2e71y_input_dev);

    IR2E71Y_TRACE("out");
    return ret;
}
#endif /* IR2E71Y_ALS_INT */

/* ------------------------------------------------------------------------- */
/*ir2e71y_init                                                               */
/* ------------------------------------------------------------------------- */
static int __init ir2e71y_init(void)
{
    IR2E71Y_TRACE("ir2e71y_init S.");
    ir2e71y_if_register_driver();
    IR2E71Y_TRACE("ir2e71y_init E.");
    return IR2E71Y_RESULT_SUCCESS;
}
/* ------------------------------------------------------------------------- */
/*ir2e71y_driver_initialize                                                  */
/* ------------------------------------------------------------------------- */
static int ir2e71y_driver_initialize(struct platform_device *pdev)
{
    int ret;
    struct ir2e71y_bdic_state_str    state_str;
    int ir2e71y_subscribe_type;
    int i;
    struct ir2e71y_main_bkl_ctl bkl_ctl;
    struct ir2e71y_tri_led tri_led;
#if defined(CONFIG_ANDROID_ENGINEERING)
    struct proc_dir_entry *entry;
#endif /* CONFIG_ANDROID_ENGINEERING */

    memset(&tri_led,   0x00, sizeof(tri_led));
    memset(&bkl_ctl,   0x00, sizeof(bkl_ctl));
    memset(&state_str, 0x00, sizeof(state_str));
#ifdef IR2E71Y_NOT_SUPPORT_NO_OS
    IR2E71Y_TRACE("in NOT SUPPORT NO_OS\n")
#else   /* IR2E71Y_NOT_SUPPORT_NO_OS */
    IR2E71Y_TRACE("in SUPPORT NO_OS\n")
#endif  /* IR2E71Y_NOT_SUPPORT_NO_OS */


    ir2e71y_init_context();
    ir2e71y_IO_API_Host_gpio_init();

    ret = alloc_chrdev_region(&ir2e71y_dev, 0, 1, IR2E71Y_NAME);

    if (!ret) {
        ir2e71y_major = MAJOR(ir2e71y_dev);
        ir2e71y_minor = MINOR(ir2e71y_dev);
    } else {
        IR2E71Y_ERR("init err. no.1\n")
        goto ir2e71y_err_1;
    }

    cdev_init(&ir2e71y_cdev, &ir2e71y_fops);

    ir2e71y_cdev.owner = THIS_MODULE;
    ir2e71y_cdev.ops   = &ir2e71y_fops;

    ret = cdev_add(&ir2e71y_cdev, MKDEV(ir2e71y_major, 0), 1);

    if (ret) {
        IR2E71Y_ERR("init err. no.2\n")
        goto ir2e71y_err_2;
    }

    ir2e71y_class = class_create(THIS_MODULE, IR2E71Y_NAME);

    if (IS_ERR(ir2e71y_class)) {
        IR2E71Y_ERR("init err. no.3\n")
        goto ir2e71y_err_3;
    }

    device_create(ir2e71y_class, NULL,
                  ir2e71y_dev, &ir2e71y_cdev, IR2E71Y_NAME);

#if defined(CONFIG_ANDROID_ENGINEERING)
    memset(&proc_fops, 0, sizeof(struct file_operations));
    proc_fops.read = ir2e71y_proc_file_read;
    proc_fops.write = ir2e71y_proc_file_write;

    entry = proc_create("driver/IR2E71Y", 0666, NULL, &proc_fops);

    if (entry == NULL) {
        IR2E71Y_ERR("init err. no.4\n")
        goto ir2e71y_err_4;
    }
#endif /* CONFIG_ANDROID_ENGINEERING */

    sema_init(&ir2e71y_sem, 1);

    sema_init(&ir2e71y_sem_callback, 1);
    sema_init(&ir2e71y_sem_irq_fac, 1);
    sema_init(&ir2e71y_sem_timer, 1);
    sema_init(&ir2e71y_sem_req_recovery_psals, 1);

    spin_lock_init(&ir2e71y_q_lock);
    spin_lock_init(&ir2e71y_wake_spinlock);

    ir2e71y_IO_API_set_irq_init();

    ir2e71y_wake_lock_init();

    memset(&ir2e71y_queue_data, 0, sizeof(ir2e71y_queue_data));
    INIT_LIST_HEAD( &ir2e71y_queue_data.list);

    ir2e71y_wq_gpio = create_singlethread_workqueue("ir2e71y_gpio_queue");

    if (ir2e71y_wq_gpio) {
        INIT_WORK(&ir2e71y_wq_gpio_wk,
                  ir2e71y_workqueue_handler_gpio);
    } else {
        IR2E71Y_ERR("init err. no.5\n")
        goto ir2e71y_err_4;
    }

    ir2e71y_wq_gpio_task = create_singlethread_workqueue("ir2e71y_gpio_queue_task");

    if (ir2e71y_wq_gpio_task) {
        INIT_WORK(&ir2e71y_wq_gpio_task_wk,
                  ir2e71y_workqueue_gpio_task);
    } else {
        IR2E71Y_ERR("init err. no.6\n")
        goto ir2e71y_err_5;
    }

    ir2e71y_wq_recovery = create_singlethread_workqueue("ir2e71y_recovery_task");

    if (ir2e71y_wq_recovery) {
        INIT_WORK(&ir2e71y_wq_recovery_psals_wk,
                  ir2e71y_workqueue_handler_recovery_psals);
    } else {
        IR2E71Y_ERR("init err. no.7\n")
        goto ir2e71y_err_6;
    }

    down(&ir2e71y_sem_callback);
    for (i = 0; i < NUM_IR2E71Y_IRQ_TYPE ; i++) {
        ir2e71y_callback_table[i] = NULL;
    }
    up(&ir2e71y_sem_callback);

    init_timer(&ir2e71y_timer);

    ir2e71y_wq_timer_task = create_singlethread_workqueue("ir2e71y_timer_queue_task");
    if (ir2e71y_wq_timer_task) {
        INIT_WORK(&ir2e71y_wq_timer_task_wk, ir2e71y_workqueue_timer_task);
    } else {
        IR2E71Y_ERR("init err. no.8\n")
        goto ir2e71y_err_7;
    }

    for (i = 0; i < NUM_IR2E71Y_IRQ_TYPE ; i++) {
        ir2e71y_subscribe_type = IR2E71Y_SUBSCRIBE_TYPE_INT;
        ir2e71y_subscribe_type_table[i] = ir2e71y_subscribe_type;
    }
    ir2e71y_pm_API_init(&ir2e71y_if_ctx.boot_ctx);

    ret = ir2e71y_IO_API_bdic_i2c_init();
    if (ret) {
        IR2E71Y_ERR("init err. no.9\n")
        goto ir2e71y_err_8;
    }

    ret = ir2e71y_IO_API_sensor_i2c_init();
    if (ret) {
       IR2E71Y_ERR("init err. no.10\n")
       goto ir2e71y_err_9;
    }

    ret = ir2e71y_bdic_API_kerl_init(pdev);
    if (ret) {
       IR2E71Y_ERR("init err. no.11\n")
       goto ir2e71y_err_10;
    }
#ifdef IR2E71Y_NOT_SUPPORT_NO_OS
    ir2e71y_if_ctx.bdic_is_exist = ir2e71y_bdic_API_boot_init(&state_str.bdic_chipver);
    ir2e71y_bdic_API_boot_init_2nd();
    ir2e71y_if_ctx.bdic_chipver = state_str.bdic_chipver;
    ret = ir2e71y_bdic_API_initialize(&state_str);
#else  /* IR2E71Y_NOT_SUPPORT_NO_OS*/
    state_str.handset_color = ir2e71y_if_ctx.boot_ctx.handset_color;
    state_str.bdic_chipver = ir2e71y_if_ctx.bdic_chipver;
    memcpy(&(state_str.photo_sensor_adj),
        &(ir2e71y_if_ctx.boot_ctx.photo_sensor_adj), sizeof(struct ir2e71y_photo_sensor_adj));
    if (ir2e71y_check_bdic_exist() == IR2E71Y_RESULT_SUCCESS) {
        ret = ir2e71y_bdic_API_initialize(&state_str);
    }
#endif /* IR2E71Y_NOT_SUPPORT_NO_OS*/
    if (ret) {
       IR2E71Y_ERR("init err. no.12\n")
        goto ir2e71y_err_10;
    }

    bkl_ctl.mode  = ir2e71y_if_ctx.main_bkl.mode;
    bkl_ctl.param = ir2e71y_if_ctx.main_bkl.param;
    ir2e71y_bdic_API_LCD_BKL_set_request(IR2E71Y_MAIN_BKL_DEV_TYPE_APP, &bkl_ctl);

    tri_led.red      = ir2e71y_if_ctx.tri_led.red;
    tri_led.green    = ir2e71y_if_ctx.tri_led.green;
    tri_led.blue     = ir2e71y_if_ctx.tri_led.blue;
    tri_led.ext_mode = ir2e71y_if_ctx.tri_led.ext_mode;
    tri_led.led_mode = ir2e71y_if_ctx.tri_led.led_mode;
    tri_led.ontime   = ir2e71y_if_ctx.tri_led.ontime;
    tri_led.interval = ir2e71y_if_ctx.tri_led.interval;
    tri_led.count    = ir2e71y_if_ctx.tri_led.count;

    ir2e71y_if_ctx.sysfs_led1.red   = ((ir2e71y_if_ctx.tri_led.red   > 0) ? 255 : 0);
    ir2e71y_if_ctx.sysfs_led1.green = ((ir2e71y_if_ctx.tri_led.green > 0) ? 255 : 0);
    ir2e71y_if_ctx.sysfs_led1.blue  = ((ir2e71y_if_ctx.tri_led.blue  > 0) ? 255 : 0);
    if ((tri_led.red + tri_led.green + tri_led.blue) != 0) {
        ir2e71y_if_ctx.sysfs_led1.led_mode = IR2E71Y_TRI_LED_MODE_NORMAL;
    } else {
        ir2e71y_if_ctx.sysfs_led1.led_mode = IR2E71Y_TRI_LED_MODE_OFF;
    }

    ir2e71y_bdic_API_TRI_LED_set_request(&tri_led);
    ret = ir2e71y_IO_API_request_irq(ir2e71y_gpio_int_isr);

    if (ret) {
       IR2E71Y_ERR("init err. no.13\n")
        goto ir2e71y_err_11;
    }

#ifdef IR2E71Y_ALS_INT
    ir2e71y_input_subsystem_init();
#endif /* IR2E71Y_ALS_INT */

    INIT_WORK(&ir2e71y_bkl_work, ir2e71y_bkl_workhandler);

    ir2e71y_if_ctx.driver_is_initialized = IR2E71Y_DRIVER_IS_INITIALIZED;
    IR2E71Y_TRACE("out\n")

    return IR2E71Y_RESULT_SUCCESS;

ir2e71y_err_11:
    ir2e71y_IO_API_free_irq();

ir2e71y_err_10:
    ir2e71y_IO_API_sensor_i2c_exit();

ir2e71y_err_9:
    ir2e71y_IO_API_bdic_i2c_exit();

ir2e71y_err_8:
    flush_workqueue(ir2e71y_wq_timer_task);
    destroy_workqueue(ir2e71y_wq_timer_task);
    ir2e71y_wq_timer_task = NULL;

ir2e71y_err_7:
    flush_workqueue(ir2e71y_wq_recovery);
    destroy_workqueue(ir2e71y_wq_recovery);
    ir2e71y_wq_recovery = NULL;

ir2e71y_err_6:
    flush_workqueue(ir2e71y_wq_gpio_task);
    destroy_workqueue(ir2e71y_wq_gpio_task);
    ir2e71y_wq_gpio_task = NULL;

ir2e71y_err_5:
    flush_workqueue(ir2e71y_wq_gpio);
    destroy_workqueue(ir2e71y_wq_gpio);
    ir2e71y_wq_gpio = NULL;

ir2e71y_err_4:
    device_destroy(ir2e71y_class, MKDEV(ir2e71y_major, 0));

ir2e71y_err_3:
    class_destroy(ir2e71y_class);

ir2e71y_err_2:
    cdev_del(&ir2e71y_cdev);

ir2e71y_err_1:
    ir2e71y_IO_API_Host_gpio_exit();
    unregister_chrdev_region(MKDEV(ir2e71y_major, 0), 1);


    return -EIO;
}
module_init(ir2e71y_init);

/* ------------------------------------------------------------------------- */
/*ir2e71y_leds_initialize                                                    */
/* ------------------------------------------------------------------------- */
static int ir2e71y_leds_initialize(struct platform_device *pdev)
{
    int rc = 0, num_leds = 0, parsed_leds = 0, max_current = 0, ret = 0;
    struct leds_led_data *led, *led_array;
    struct device_node *node, *temp;

    IR2E71Y_TRACE("in\n")

    node = pdev->dev.of_node;
    if (node == NULL) {
        IR2E71Y_ERR("node NULL\n");
        return -ENODEV;
    }

    temp = NULL;
    while ((temp = of_get_next_child(node, temp))) {
        num_leds++;
    }

    if (!num_leds) {
        IR2E71Y_ERR("num_leds is 0\n");
        return -ECHILD;
    }
    IR2E71Y_DEBUG("chk num_leds:%d\n", num_leds);

    led_array = devm_kzalloc(&pdev->dev,
        (sizeof(struct leds_led_data) * num_leds), GFP_KERNEL);
    if (!led_array) {
        IR2E71Y_ERR("Unable to allocate memory\n");
        return -ENOMEM;
    }

    for_each_child_of_node(node, temp) {
        led = &led_array[parsed_leds];
        led->num_leds = num_leds;

        rc = of_property_read_string(temp, "linux,name", &led->cdev.name);
        if (rc < 0) {
            IR2E71Y_ERR("Failure reading label, rc = %d\n", rc);
        }
        IR2E71Y_DEBUG("rc(%d) read name:%s\n", rc, led->cdev.name);
        if (!strcmp(led->cdev.name, "wled")) {
            led->cdev.brightness_set = ir2e71y_set_wled_brightness;
            rc = of_property_read_string(temp, "linux,default-trigger",
            &led->cdev.default_trigger);
            if (rc && (rc != -EINVAL)) {
                IR2E71Y_ERR("Unable to read led trigger");
                ret = rc;
                goto end;
            }
#ifdef IR2E71Y_SYSFS_LED
        } else {
            rc = of_property_read_u32(temp, "no", &led->no);
            if (rc < 0) {
                IR2E71Y_ERR("Failure reading led no, rc = %d\n", rc);
            }
            IR2E71Y_DEBUG("rc(%d) read no:%d\n", rc, led->no);

            rc = of_property_read_u32(temp, "color", &led->color);
            if (rc < 0) {
                IR2E71Y_ERR("Failure reading led color, rc = %d\n", rc);
            }
            IR2E71Y_DEBUG("rc(%d) read color:%d\n", rc, led->color);

            led->cdev.brightness_set = ir2e71y_led_brightness_set;
            led->cdev.brightness_get = ir2e71y_led_brightness_get;
        }
#else /* IR2E71Y_SYSFS_LED */
        } else {
            IR2E71Y_ERR("unknown led %s\n", led->cdev.name);
            ret = -ECHILD;
            goto end;
        }
#endif /* IR2E71Y_SYSFS_LED */

        rc = of_property_read_u32(temp, "max-current", &max_current);
        if (rc < 0) {
            IR2E71Y_ERR("Failure reading max_current, rc = %d\n", rc);
            max_current = 255;
        }
        led->cdev.max_brightness = max_current;
        IR2E71Y_DEBUG("rc(%d) read max_brightness:%d\n", rc, max_current);

        led->cdev.brightness = LED_OFF;
        rc = led_classdev_register(&pdev->dev, &led->cdev);
        if (rc) {
            IR2E71Y_ERR("unable to register led %s, rc=%d\n", led->cdev.name, rc);
        }
        IR2E71Y_DEBUG("rc(%d) led_reg name:%s\n", rc, led->cdev.name);

#ifdef IR2E71Y_SYSFS_LED
        if (strcmp(led->cdev.name, "wled")) {
            if((!rc) && (&led->cdev.dev->kobj != NULL)) {
                IR2E71Y_DEBUG("blink make name:%s\n", led->cdev.name);
                rc = sysfs_create_group(&led->cdev.dev->kobj, &blink_attr_group);
                if (rc) {
                    IR2E71Y_ERR("error sysfs blink_attr_group\n");
                }
            }
        }
#endif /* IR2E71Y_SYSFS_LED */

        parsed_leds++;
    }
    IR2E71Y_TRACE("parsed_leds = %d\n", parsed_leds);
end:
    return ret;
}

/* ------------------------------------------------------------------------- */
/*ir2e71y_exit                                                               */
/* ------------------------------------------------------------------------- */
static void ir2e71y_exit(void)
{
    ir2e71y_fb_close();

#ifdef IR2E71Y_ALS_INT
    wake_unlock(&ir2e71y_timeout_wake_lock);
    wake_lock_destroy(&ir2e71y_timeout_wake_lock);
#endif /* IR2E71Y_ALS_INT */
    wake_lock_destroy(&ir2e71y_wake_lock_wq);

    ir2e71y_IO_API_free_irq();
    if (ir2e71y_wq_gpio) {
        flush_workqueue(ir2e71y_wq_gpio);
        destroy_workqueue(ir2e71y_wq_gpio);
        ir2e71y_wq_gpio = NULL;
    }

    if (ir2e71y_wq_gpio_task) {
        flush_workqueue(ir2e71y_wq_gpio_task);
        destroy_workqueue(ir2e71y_wq_gpio_task);
        ir2e71y_wq_gpio_task = NULL;
    }

    ir2e71y_bdic_API_TRI_LED_exit();
    ir2e71y_IO_API_sensor_i2c_exit();
    ir2e71y_IO_API_bdic_i2c_exit();
    ir2e71y_IO_API_Host_gpio_exit();
    device_destroy(ir2e71y_class, MKDEV(ir2e71y_major, 0));
    class_destroy(ir2e71y_class);
    cdev_del(&ir2e71y_cdev);
    unregister_chrdev_region(MKDEV(ir2e71y_major, 0), 1);
    return;
}
module_exit(ir2e71y_exit);

/* ------------------------------------------------------------------------- */
/*ir2e71y_if_probe                                                         */
/* ------------------------------------------------------------------------- */
static int ir2e71y_if_probe(struct platform_device *pdev)
{
#ifdef CONFIG_OF
    int rc = IR2E71Y_RESULT_SUCCESS;

    IR2E71Y_TRACE("in pdev = 0x%p", pdev);

    if (pdev) {
        if (&(pdev->dev) != NULL) {
            ir2e71y_leds_initialize(pdev);
            ir2e71y_driver_initialize(pdev);
        } else {
            IR2E71Y_ERR("pdev->dev is NULL");
        }
    } else {
        IR2E71Y_ERR("pdev is NULL");
    }
    return rc;
#else   /* CONFIG_OF */
    return IR2E71Y_RESULT_SUCCESS;
#endif  /* CONFIG_OF */
}

#ifdef CONFIG_OF
static const struct of_device_id ir2e71y_if_match[] = {
    { .compatible = "sharp,ir2e71y8", },
    {}
};
#else   /* CONFIG_OF */
#define ir2e71y_if_match NULL
#endif  /* CONFIG_OF */

static struct platform_driver ir2e71y_if_driver = {
    .probe = ir2e71y_if_probe,
    .remove = NULL,
    .shutdown = NULL,
    .driver = {
        /*
         * Driver name must match the device name added in
         * platform.c.
         */
        .name = "ir2e71y8",
        .of_match_table = ir2e71y_if_match,
    },
};

/* ------------------------------------------------------------------------- */
/*ir2e71y_if_register_driver                                               */
/* ------------------------------------------------------------------------- */
static int ir2e71y_if_register_driver(void)
{
    return platform_driver_register(&ir2e71y_if_driver);
}

MODULE_DESCRIPTION("SHARP IR2E71Y8 MODULE");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("SHARP CORPORATION");
MODULE_VERSION("1.00");

/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */
