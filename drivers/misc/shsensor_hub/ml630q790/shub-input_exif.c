/* shub-input_exif.c
 *
 * Copyright (C) 2014 Sharp Corporation
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
static int shub_exif_log = 0;
module_param(shub_exif_log, int, 0600);
#define DBG_EXIF_IO(msg, ...) {                      \
    if(shub_exif_log & 0x01)                         \
        printk("[shub][exif] " msg, ##__VA_ARGS__);  \
}
#define DBG_EXIF_DATA(msg, ...) {                    \
    if(shub_exif_log & 0x02)                         \
        printk("[shub][exif] " msg, ##__VA_ARGS__);  \
}
#else
#define DBG_EXIF_IO(msg, ...)
#define DBG_EXIF_DATA(msg, ...)
#endif

#define HC_LOGGING_SET_PEDO                 (0xf020u)
#define HC_LOGGING_GET_PEDO                 (0xf021u)

#define HC_FLG_LOGGING_PEDO                 (0x0001u)
#define HC_FLG_LOGGING_TOTAL_STATUS         (0x0002u)

#define ACC_MAX    ACC_CMN_MAX
#define ACC_MIN    ACC_CMN_MIN
#define INPUT_UIT8_MAX 255
#define INPUT_UIT8_MIN 0
#define INPUT_UIT16_MAX 65535
#define INPUT_UIT16_MIN 0
#define INDEX_X            0
#define INDEX_Y            1
#define INDEX_Z            2
#define INDEX_TM           3
#define INDEX_TMNS         4
#define INDEX_SUM          5
#define MISC_DEV_NAME_EXIF  "shub_io_exif"

#define APP_PEDOMETER               (0x01)
#define APP_CALORIE_FACTOR          (0x02)
#define APP_RUN_DETECTION           (0x03)
#define APP_VEICHLE_DETECTION       (0x04)
#define APP_TOTAL_STATUS_DETECTION  (0x05)
#define APP_GDETECTION              (0x06)
#define APP_MOTDTECTION             (0x07)
#define MCU_TASK_CYCLE              (0x08)
#define APP_TOTAL_STATUS_DETECTION_CLR_CONT_STEPS   (0x09)
#define APP_TOTAL_STATUS_DETECTION_CLR_CONT_STOP    (0x0A)
#define APP_NORMAL_PEDOMETER        (0x0B)
#define APP_LOW_POWER               (0x0C)
#define APP_VEICHLE_DETECTION2      (0x0E)
#define APP_CLEAR_PEDOM_AND_TOTAL_STATUS_DETECTION  (0x0F)
#define APP_VEICHLE_DETECTION3      (0x10)
#define APP_PEDOMETER2              (0x11)
#define APP_PEDOMETER2_2            (0x12)
#define APP_PICKUP_ENABLE           (0x13)
//#define APP_PICKUP_PARAM1           (0x14)
//#define APP_PICKUP_PARAM2           (0x15)
#define APP_PAUSE_PARAM             (0x16)
#define APP_PAUSE_STATUS_PARAM      (0x17)
#define APP_LPM_PARAM               (0x18)
#define APP_LPM_DEV_PARAM           (0x19)
#define APP_RIDE_PEDO_PARAM         (0x20)
#define APP_RIDE_PEDO2_PARAM        (0x21)
#define APP_LPM_DEV_PARAM2          (0x22)

#define APP_TOTAL_STATUS_DETECTION2 (0x23)
#define APP_PEDO_ALL_STATE_2ND      (0x26)
#define APP_PEDOMETER_N             (0x81)
#define SHUB_TMP_BUFSIZE            3
#define SHUB_DETECT_NORMAL          0
#define SHUB_DETECT_ECONOMIZE       1
#define SHUB_INTERVAL_MOT_DET_MS    5000
#define SHUB_EXIF_DETECT_PICKUP     0x0100
#define SHUB_EXIF_TWIST_RIGHT       0x0200
#define SHUB_EXIF_TWIST_LEFT        0x0400
#define TYPE_VIB                    (0x01)
#define TYPE_TPS                    (0x02)
#define TYPE_SPE                    (0x04)
#define TYPE_STOPPING_NOW           (0x10)
#define TYPE_STOP_NOW               (0x20)
#define TYPE_RESTART_NOW            (0x40)
#define SHUB_INTERVAL_STOP_PED_MS    3000

#define INT_GYRO_TWIST_RIGHT        0x02
#define INT_GYRO_TWIST_LEFT         0x04

enum{
    SHUB_IDX_PEDO = 0,
    SHUB_IDX_STOP,
    SHUB_IDX_RUN,
    SHUB_IDX_VEICHLE,
    SHUB_IDX_TOTAL,
    SHUB_IDX_MAXNUM
};

static int shub_stop_ped_onoff(int onoff);

static DEFINE_MUTEX(shub_exif_lock);
static DEFINE_MUTEX(shub_mutex_if);
static DEFINE_MUTEX(shub_mutex_req);
static DEFINE_MUTEX(shub_mutex_vibe);
static DEFINE_MUTEX(shub_mutex_speaker);

enum{
    SHUB_COND_STOP = 0,
    SHUB_COND_WALK,
    SHUB_COND_RIDE,
    SHUB_COND_CAR,
    SHUB_COND_TRAIN,
    SHUB_COND_MAX
};

enum{
    SHUB_PEDO_ASN_STOP = 0,
    SHUB_PEDO_ASN_STOP_CONT,
    SHUB_PEDO_ASN_WALK,
    SHUB_PEDO_ASN_WALK_CONT,
    SHUB_PEDO_ASN_TRANS_DET,
    SHUB_PEDO_ASN_TRANS_FIX,
    SHUB_PEDO_ASN_TRANS_TYPE,
    SHUB_PEDO_ASN_TRANS_RATE,
    SHUB_PEDO_ASN_TRANS_REL_WALK,
    SHUB_PEDO_ASN_TRANS_REL_STIL,
    SHUB_PEDO_ASN_NON
};

enum{
    SHUB_EXIF_NOTIFY = 0,
    SHUB_EXIF_ACC_SHEX,
    SHUB_EXIF_MAXNUM
};

static char dname0[] = "shub_ex_notify";
static char dname1[] = "shub_ex_acc";
static char dphys0[] = "shub_ex_notify/input0";
static char dphys1[] = "shub_ex_acc/input0";
static char *shub_dev_name[] = {dname0, dname1};
static char *shub_dev_phys[] = {dphys0, dphys1};
static struct platform_device *shub_pdev[SHUB_EXIF_MAXNUM];
static struct input_dev *shub_idev[SHUB_EXIF_MAXNUM];
static int shub_exif_shex_acc_enable;
static int shub_exif_delay_ms;
static int shub_exif_notify_cont = 1;
static int shub_exif_notify_info = 0;
static int32_t shub_latest_ex_AccData[INDEX_SUM]= {0};
static struct work_struct sensor_poll_work;
static struct hrtimer poll_timer;
static struct work_struct stop_ped_work;
static struct work_struct restart_ped_work;
static struct hrtimer poll_restart_ped_timer;
static int shub_check_ped_type;
static int shub_ped_on_inf[SHUB_IDX_MAXNUM];
static int shub_enable_ped_exif_flg;
static int shub_check_suspend_restart;

static int shub_full_buffer_flg = 0;
static unsigned long shub_count = 0;
static int shub_buf_size = 10;
static int shub_buf_size_temp = 10;
static int shub_md_thresh = 204;
static signed short *shub_detect_buf[3];
static signed short shub_tmp_buf[3][SHUB_TMP_BUFSIZE];
static signed short center_data[3];
atomic_t shub_detect_mode_flg = ATOMIC_INIT(SHUB_DETECT_NORMAL);
static int shub_already_md_flg = 0;
static void shub_disable_internal_mot_det(void);

static int32_t shub_exif_md_mode_flg = 0;

static int shub_not_notify_vibe_flg = 0;
static uint64_t shub_vibe_endtime_ns = 0;

static int32_t shub_probe_exif(struct platform_device *pfdev);
static int32_t shub_remove_exif(struct platform_device *pfdev);

#ifdef CONFIG_OF
    static struct of_device_id shub_of_match_tb_exif[] = {
        { .compatible = "sharp,shub_exif" ,},
        {}
    };
#else
    #define shub_of_match_tb_exif NULL
#endif

static uint64_t shub_get_time_ns(void)
{
    struct timespec ts;
    ktime_get_ts(&ts);
    monotonic_to_bootbased(&ts);
    return timespec_to_ns(&ts);
}

//static int shub_vibe_notify_check(int kind)
int shub_vibe_notify_check(int kind)
{
    int check_ret = 0;
    mutex_lock(&shub_mutex_vibe);
    if(shub_not_notify_vibe_flg) {
        DBG_EXIF_DATA( "det_kind(%d) : not notify return.(vibe) already_md_flg=%d\n", kind, shub_get_already_md_flg());
        check_ret = 1;
    }
    else if(shub_vibe_endtime_ns != 0) {
        uint64_t sub_time = shub_get_time_ns() - shub_vibe_endtime_ns;
        if(sub_time < 1000000000) {
            DBG_EXIF_DATA( "det_kind(%d) : not notify return.(vibe under 1s) already_md_flg=%d sub_time=%lld\n", kind, shub_get_already_md_flg(), sub_time);
            check_ret = 1;
        }
        else {
            shub_vibe_endtime_ns = 0;
        }
    }
    mutex_unlock(&shub_mutex_vibe);
    return check_ret;
}

static int shub_not_notify_speaker_flg = 0;
static uint64_t shub_speaker_endtime_ns = 0;
static int shub_speaker_notify_check(int kind)
{
    int check_ret = 0;
    mutex_lock(&shub_mutex_speaker);
    if(shub_not_notify_speaker_flg) {
        DBG_EXIF_DATA( "det_kind(%d) : not notify return.(speaker) already_md_flg=%d\n", kind, shub_get_already_md_flg());
        check_ret = 1;
    }
    else if(shub_speaker_endtime_ns != 0) {
        uint64_t sub_time = shub_get_time_ns() - shub_speaker_endtime_ns;
        if(sub_time < 1000000000) {
            DBG_EXIF_DATA( "det_kind(%d) : not notify return.(speaker under 1s) already_md_flg=%d sub_time=%lld\n", kind, shub_get_already_md_flg(), sub_time);
            check_ret = 1;
        }
        else {
            shub_speaker_endtime_ns = 0;
        }
    }
    mutex_unlock(&shub_mutex_speaker);
    return check_ret;
}

void shub_exif_input_val_init(void)
{
    if(shub_idev[SHUB_EXIF_NOTIFY] == NULL){
        DBG_EXIF_DATA("[shub]%s : shub_idev is Null!!", __func__);
        return;
    }
    
    if(shub_idev[SHUB_EXIF_NOTIFY]->absinfo == NULL){
        DBG_EXIF_DATA("[shub]%s : absinfo is Null!!", __func__);
        return;
    }
    
    SHUB_INPUT_VAL_CLEAR(shub_idev[SHUB_EXIF_NOTIFY], ABS_X, 1);
    SHUB_INPUT_VAL_CLEAR(shub_idev[SHUB_EXIF_NOTIFY], ABS_Y, 1);
    SHUB_INPUT_VAL_CLEAR(shub_idev[SHUB_EXIF_NOTIFY], ABS_RX, 1);
    SHUB_INPUT_VAL_CLEAR(shub_idev[SHUB_EXIF_NOTIFY], ABS_RY, 1);
    SHUB_INPUT_VAL_CLEAR(shub_idev[SHUB_EXIF_NOTIFY], ABS_RZ, 1);
//  shub_input_set_value(shub_idev[SHUB_EXIF_NOTIFY], ABS_Y, 0);
//  shub_idev[SHUB_EXIF_NOTIFY]->absinfo[ABS_Y].value = 0;
    return;
}

static void shub_get_center_data(signed short *c_data)
{
    int i;
    int j;
    int k;
    signed short CustodyData;
    signed short buff[3][SHUB_TMP_BUFSIZE];

    memcpy( buff, shub_tmp_buf, sizeof(shub_tmp_buf) );

    for(i = 0; i < 3; i++)
    {
        for (j = 0; j < SHUB_TMP_BUFSIZE - 1; j++)
        {
            for (k = 0; k < (SHUB_TMP_BUFSIZE - 1 - j); k++)
            {
                if ( buff[i][k] > buff[i][k + 1] )
                {
                    CustodyData = buff[i][k];
                    buff[i][k] = buff[i][k + 1];
                    buff[i][k + 1] = CustodyData;
                }
            }
        }
    }

    for(i = 0; i < 3; i++)
    {
        c_data[i] = buff[i][SHUB_TMP_BUFSIZE / 2];
    }
    return;
}

static void shub_get_detect_buf_maxmin_sub(signed long *sub)
{
    int i;
    int j;
    signed short max;
    signed short min;

    for (i = 0; i < 3; i++)
    {
        max = ACC_MIN;
        min = ACC_MAX;
        for (j = 0; j < shub_buf_size; j++)
        {
            if (shub_detect_buf[i][j] > max) {
                max = shub_detect_buf[i][j];
            }
            if (shub_detect_buf[i][j] < min) {
                min = shub_detect_buf[i][j];
            }
        }
        sub[i] = max - min;
    }
}

static void shub_check_mot_move(int32_t *accData)
{
    int i;
    int j;
    signed long sub[3] = {0,0,0};

    if (shub_buf_size != shub_buf_size_temp)
    {
        for (i = 0; i < 3; i++)
        {
            kfree(shub_detect_buf[i]);
            shub_detect_buf[i] = NULL;
        }

        for (i = 0; i < 3; i++)
        {
            shub_detect_buf[i] = (signed short *)kmalloc(shub_buf_size_temp * sizeof(signed short), GFP_KERNEL);
            if (NULL == shub_detect_buf[i])
            {
                for (j = 0; j < i; j ++)
                {
                    kfree(shub_detect_buf[j]);
                }
                printk("[shub]%s shub_detect_buf is NULL", __func__);
                return ;
            }
            memset(shub_detect_buf[i], 0, shub_buf_size_temp * sizeof(signed short));
        }
        shub_buf_size = shub_buf_size_temp;
        shub_count = 0;
        shub_full_buffer_flg = 0;
    }

    if ( atomic_read(&shub_detect_mode_flg) == SHUB_DETECT_NORMAL )
    {
        if (( !shub_count ) && ( !shub_full_buffer_flg ))
        {
            for (i = 0; i < SHUB_TMP_BUFSIZE; i++)
            {
                shub_tmp_buf[0][i] = (signed short)accData[0];
                shub_tmp_buf[1][i] = (signed short)accData[1];
                shub_tmp_buf[2][i] = (signed short)accData[2];
            }
        }
        else
        {
            for (i = 0; i < ( SHUB_TMP_BUFSIZE - 1 ); i++ )
            {
                shub_tmp_buf[0][i] = shub_tmp_buf[0][i + 1];
                shub_tmp_buf[1][i] = shub_tmp_buf[1][i + 1];
                shub_tmp_buf[2][i] = shub_tmp_buf[2][i + 1];
            }
            shub_tmp_buf[0][SHUB_TMP_BUFSIZE - 1] = (signed short)accData[0];
            shub_tmp_buf[1][SHUB_TMP_BUFSIZE - 1] = (signed short)accData[1];
            shub_tmp_buf[2][SHUB_TMP_BUFSIZE - 1] = (signed short)accData[2];
        }

        shub_get_center_data( center_data );

        shub_detect_buf[0][shub_count] = center_data[0];
        shub_detect_buf[1][shub_count] = center_data[1];
        shub_detect_buf[2][shub_count] = center_data[2];

        if ( (shub_count+1) == shub_buf_size )
        {
            shub_full_buffer_flg = 1;
        }

        if ( shub_full_buffer_flg )
        {
            shub_get_detect_buf_maxmin_sub(sub);
            if ((sub[0] <= shub_md_thresh) && (sub[1] <= shub_md_thresh) && (sub[2] <= shub_md_thresh))
            {
                int32_t ret = 0;
                IoCtlParam param;
                shub_detect_buf[0][shub_count] = accData[0];
                shub_detect_buf[1][shub_count] = accData[1];
                shub_detect_buf[2][shub_count] = accData[2];
                shub_get_detect_buf_maxmin_sub(sub);
                if ((sub[0] > shub_md_thresh) || (sub[1] > shub_md_thresh) || (sub[2] > shub_md_thresh))
                {
                    shub_count = (shub_count + 1) % shub_buf_size;
                    return ;
                }
//                shub_count = 0;
//                shub_full_buffer_flg = 0;
//                for (i = 0; i < 3; i++)
//                {
//                    memset(shub_detect_buf[i], 0, (shub_buf_size * sizeof(signed short)));
//                }
                memset(&param, 0, sizeof(IoCtlParam));
                param.m_iType = APP_MOTDTECTION;
                ret = shub_get_param(param.m_iType, param.m_iParam);
                if(ret != 0) {
                    printk( "[shub]%s get MOTDTECTION error. ret=%d\n", __func__, ret);
                    return ;
                }
                shub_set_already_md_flg(2);
                ret = shub_set_param(param.m_iType, param.m_iParam);
                if(ret != 0) {
                    printk( "[shub]%s set MOTDTECTION error. ret=%d\n", __func__, ret);
                    return ;
                }
                atomic_set(&shub_detect_mode_flg, SHUB_DETECT_ECONOMIZE);
                mutex_lock(&shub_exif_lock);
                hrtimer_cancel(&poll_timer);
//                hrtimer_start(&poll_timer, ns_to_ktime((int64_t)SHUB_INTERVAL_MOT_DET_MS * NSEC_PER_MSEC), HRTIMER_MODE_REL);
                hrtimer_start(&poll_timer, ns_to_ktime(300 * NSEC_PER_MSEC), HRTIMER_MODE_REL);
                mutex_unlock(&shub_exif_lock);
                DBG_EXIF_DATA( "ECONOMIZE already_md_flg=%d\n", shub_get_already_md_flg());
                return ;
            }
        }
        shub_count = (shub_count + 1) % shub_buf_size;
    }
    else if ( atomic_read(&shub_detect_mode_flg) == SHUB_DETECT_ECONOMIZE )
    {
        if(shub_full_buffer_flg) {
            shub_count = (shub_count + 1) % shub_buf_size;
            shub_detect_buf[0][shub_count] = accData[0];
            shub_detect_buf[1][shub_count] = accData[1];
            shub_detect_buf[2][shub_count] = accData[2];
            
            shub_get_detect_buf_maxmin_sub(sub);
            if ((sub[0] > shub_md_thresh) || (sub[1] > shub_md_thresh) || (sub[2] > shub_md_thresh))
            {
                shub_disable_internal_mot_det();
                atomic_set(&shub_detect_mode_flg, SHUB_DETECT_NORMAL);
                DBG_EXIF_DATA( "NORMAL2 already_md_flg=%d\n", shub_get_already_md_flg());
                mutex_lock(&shub_exif_lock);
                hrtimer_cancel(&poll_timer);
                hrtimer_start(&poll_timer, ns_to_ktime((int64_t)shub_exif_delay_ms * NSEC_PER_MSEC), HRTIMER_MODE_REL);
                mutex_unlock(&shub_exif_lock);
            }
            shub_count = 0;
            shub_full_buffer_flg = 0;
            for (i = 0; i < 3; i++)
            {
                memset(shub_detect_buf[i], 0, (shub_buf_size * sizeof(signed short)));
            }
        }
    }
}

static void shub_disable_internal_mot_det(void)
{
    int32_t ret = 0;
    IoCtlParam param;
    
    memset(&param, 0, sizeof(IoCtlParam));
    param.m_iType = APP_MOTDTECTION;
    ret = shub_get_param(param.m_iType, param.m_iParam);
    if(ret != 0) {
        printk( "[shub]%s get MOTDTECTION error. ret=%d\n", __func__, ret);
        return ;
    }
    shub_clr_already_md_flg(2);
    if(!(shub_get_already_md_flg() & 0x01)) {
        if(param.m_iParam[9] & 0x01) {
            param.m_iParam[9] &= ~0x02;
        }
        else {
            param.m_iParam[0] = 0;
            param.m_iParam[9] = 0x00;
        }
        ret = shub_set_param(param.m_iType, param.m_iParam);
        if(ret != 0) {
            printk( "[shub]%s set MOTDTECTION error. ret=%d\n", __func__, ret);
            return ;
        }
    }
}


static void shub_sensor_poll_work_func(struct work_struct *work)
{
//  int32_t xyz[INDEX_SUM]= {0};

    mutex_lock(&shub_exif_lock);
    shub_qos_start();
    shub_get_sensors_data(SHUB_ACTIVE_ACC, shub_latest_ex_AccData);
    shub_input_report_exif_shex_acc(shub_latest_ex_AccData);
    shub_qos_end();
    mutex_unlock(&shub_exif_lock);
    shub_check_mot_move(shub_latest_ex_AccData);
}

static enum hrtimer_restart shub_sensor_poll(struct hrtimer *tm)
{
    schedule_work(&sensor_poll_work);
    if(shub_exif_shex_acc_enable != 0){
        if(atomic_read(&shub_detect_mode_flg) == SHUB_DETECT_ECONOMIZE) {
            hrtimer_forward_now(&poll_timer, ns_to_ktime((int64_t)SHUB_INTERVAL_MOT_DET_MS * NSEC_PER_MSEC));
        }
        else {
            hrtimer_forward_now(&poll_timer, ns_to_ktime((int64_t)shub_exif_delay_ms * NSEC_PER_MSEC));
        }
        return HRTIMER_RESTART;
    }

//    return HRTIMER_RESTART;
    return HRTIMER_NORESTART;
}


static void shub_set_sensor_poll(int32_t en)
{
    int i;
    hrtimer_cancel(&poll_timer);
    if(en) {
        hrtimer_start(&poll_timer, ns_to_ktime((int64_t)shub_exif_delay_ms * NSEC_PER_MSEC), HRTIMER_MODE_REL);
    }
    else {
        shub_count = 0;
        shub_full_buffer_flg = 0;
        atomic_set(&shub_detect_mode_flg, SHUB_DETECT_NORMAL);
        if( shub_buf_size == shub_buf_size_temp )
        {
            for (i = 0; i < 3; i++)
            {
                memset(shub_detect_buf[i], 0, (shub_buf_size * sizeof(signed short)));
            }
        }
        if(shub_get_already_md_flg() & 0x02) {
            shub_disable_internal_mot_det();
        }
    }
}

static int shub_transdata_chage(int32_t org)
{
    int change = org;
    switch(org){
    case 0:
        change = SHUB_COND_STOP;
        break;
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
        change = SHUB_COND_WALK;
        break;
    case 10:
        change = SHUB_COND_RIDE;
        break;
    case 7:
    case 8:
        change = SHUB_COND_CAR;
        break;
    case 9:
        change = SHUB_COND_TRAIN;
        break;
    }
    return change;
}

static int shub_get_notifyinfo(int32_t notify_event)
{
    int notify = SHUB_PEDO_ASN_NON;
    
    if(notify_event == 0x0001) {
        notify = SHUB_PEDO_ASN_STOP;
    }
    else if(notify_event == 0x0002) {
        notify = SHUB_PEDO_ASN_STOP_CONT;
    }
    else if(notify_event == 0x0004) {
        notify = SHUB_PEDO_ASN_WALK;
    }
    else if(notify_event == 0x0008) {
        notify = SHUB_PEDO_ASN_WALK_CONT;
    }
    else if(notify_event == 0x0010) {
        notify = SHUB_PEDO_ASN_TRANS_DET;
    }
    else if(notify_event == 0x0020) {
        notify = SHUB_PEDO_ASN_TRANS_FIX;
    }
    else if(notify_event == 0x0040) {
        notify = SHUB_PEDO_ASN_TRANS_TYPE;
    }
    else if(notify_event == 0x0080) {
        notify = SHUB_PEDO_ASN_TRANS_RATE;
    }
    else if(notify_event == 0x0100) {
        notify = SHUB_PEDO_ASN_TRANS_REL_WALK;
    }
    else if(notify_event == 0x0200) {
        notify = SHUB_PEDO_ASN_TRANS_REL_STIL;
    }
    return notify;
}

static long shub_exif_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    void __user *argp = (void __user *)arg;
    int32_t ret = -1;
    
    switch (cmd) {
        case SHUB_EXIF_SET_SHEX_ACC_ACTIVATE:
            {
                struct IoctlExifData res;
                
                ret = copy_from_user(&res,argp,sizeof(struct IoctlExifData));
                if (ret) {
                    printk("error : %s(cmd = SHUB_EXIF_SET_SHEX_ACC_ACTIVATE) copy_from_user\n", __func__);
                    return -EFAULT;
                }
                DBG_EXIF_IO("ioctl(cmd = Set_Active) : val=%d\n", res.enable);
                mutex_lock(&shub_exif_lock);
                res.rtn = 0;
                if(shub_exif_shex_acc_enable != res.enable) {
                    //set mcu sensor measure
                    ret = shub_activate(SHUB_ACTIVE_SHEX_ACC, res.enable);
                    if(ret != 0) {
                        printk("error : %s(cmd = SHUB_EXIF_SET_SHEX_ACC_ACTIVATE) shub_activate\n", __func__);
                        res.rtn = ret;
                    }
                    else {
                        shub_exif_shex_acc_enable = res.enable;
                        //set polling start 
                        shub_set_sensor_poll(res.enable);
                    }
                }
                mutex_unlock(&shub_exif_lock);
                ret = copy_to_user(argp, &res, sizeof(struct IoctlExifData));
                if (ret) {
                    printk("error : %s(cmd = SHUB_EXIF_SET_SHEX_ACC_ACTIVATE) copy_to_user\n", __func__);
                    return -EFAULT;
                }
            }
            break;
        case SHUB_EXIF_SET_SHEX_ACC_DELAY:
            {
                struct IoctlExifData res;
                
                ret = copy_from_user(&res,argp,sizeof(struct IoctlExifData));
                if (ret) {
                    printk("error : %s(cmd = SHUB_EXIF_SET_SHEX_ACC_DELAY) copy_from_user\n", __func__);
                    return -EFAULT;
                }
                DBG_EXIF_IO("ioctl(cmd = Set_Delay) : delay=%ld\n", res.delay);
                mutex_lock(&shub_exif_lock);
                res.rtn = 0;
                ret = shub_set_delay(SHUB_ACTIVE_SHEX_ACC, res.delay);
                if(ret != 0) {
                    printk("error : %s(cmd = SHUB_EXIF_SET_SHEX_ACC_DELAY) shub_set_delay\n", __func__);
                    res.rtn = ret;
                }
                else {
                    shub_exif_delay_ms = res.delay;
                    shub_buf_size_temp = 2400 / shub_exif_delay_ms;
                    if(shub_exif_shex_acc_enable != 0) {
                        shub_set_sensor_poll(1);
                    }
                }
                mutex_unlock(&shub_exif_lock);
                ret = copy_to_user(argp, &res, sizeof(struct IoctlExifData));
                if (ret) {
                    printk("error : %s(cmd = SHUB_EXIF_SET_SHEX_ACC_DELAY) copy_to_user\n", __func__);
                    return -EFAULT;
                }
            }
            break;
        case SHUB_EXIF_GET_SHEX_ACC_DELAY:
            {
                struct IoctlExifData res;
                
                memset(&res, 0, sizeof(struct IoctlExifData));
                mutex_lock(&shub_exif_lock);
                res.rtn = 0;
                res.delay = shub_exif_delay_ms;
                mutex_unlock(&shub_exif_lock);
                DBG_EXIF_IO("ioctl(cmd = Get_Delay) : delay=%ld\n", res.delay);
                ret = copy_to_user(argp, &res, sizeof(struct IoctlExifData));
                if (ret) {
                    printk("error : %s(cmd = SHUB_EXIF_GET_SHEX_ACC_DELAY) copy_to_user\n", __func__);
                    return -EFAULT;
                }
            }
            break;
        case SHUB_EXIF_GET_ADTECT_DATA:
            {
                IoCtlData dt;
                struct IoctlExifData res;
                
                memset(&res, 0, sizeof(struct IoctlExifData));
                mutex_lock(&shub_exif_lock);
                ret = shub_get_data_act_detection(dt.m_iData);
                if(ret) {
                    printk("error : %s(cmd = SHUB_EXIF_GET_ADTECT_DATA) shub_get_data_act_detection\n", __func__);
                    res.rtn = ret;
                }
                else {
                    if(dt.m_iData[0] > 0) {
                        res.data[0] = 1;
                    }
                    else {
                        res.data[0] = 0;
                    }
                    res.data[1] = (unsigned char)shub_transdata_chage(dt.m_iData[1]);
                    res.data[2] = (unsigned char)dt.m_iData[7];
                    switch(dt.m_iData[8]){
                    case 0:
                        res.data[3] = 0;
                        break;
                    case 1:
                    case 2:
                        res.data[3] = 1;
                        break;
                    case 3:
                        res.data[3] = 2;
                        break;
                    default:
                        res.data[3] = (unsigned char)dt.m_iData[8];
                        break;
                    }
                    res.data[4] = (unsigned char)dt.m_iData[10];
                    res.data[5] = (unsigned char)dt.m_iData[11];
                    res.data[6] = (unsigned char)dt.m_iData[12];
                    res.data[7] = (unsigned char)dt.m_iData[13];
                }
                mutex_unlock(&shub_exif_lock);
                DBG_EXIF_IO("ioctl(cmd = Get_Adtect_Data) : data=0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x\n",
                            res.data[0],res.data[1],res.data[2],res.data[3],res.data[4],res.data[5],res.data[6],res.data[7]);
                ret = copy_to_user(argp, &res, sizeof(struct IoctlExifData));
                if (ret) {
                    printk("error : %s(cmd = SHUB_EXIF_GET_ADTECT_DATA) copy_to_user\n", __func__);
                    return -EFAULT;
                }
            }
            break;
        default:
            printk("error : %s(cmd = %d) invalid data.\n", __func__, cmd);
            return -ENOTTY;
    }
    return 0;
}

static long shub_exif_ioctl_wrapper(struct file *filp, unsigned int cmd, unsigned long arg)
{
    SHUB_DBG_TIME_INIT
    long ret = 0;

    shub_qos_start();
    SHUB_DBG_TIME_START
    ret = shub_exif_ioctl(filp, cmd , arg);
    SHUB_DBG_TIME_END(cmd)
    shub_qos_end();

    return ret;
}

static void shub_set_abs_params(int dev_kind)
{
    switch(dev_kind){
    case SHUB_EXIF_NOTIFY:
        input_set_abs_params(shub_idev[dev_kind], ABS_X,  INPUT_UIT8_MIN, INPUT_UIT8_MAX, 0, 0);
//      input_set_abs_params(shub_idev[dev_kind], ABS_Y,  INPUT_UIT8_MIN, INPUT_UIT8_MAX, 0, 0);
        input_set_abs_params(shub_idev[dev_kind], ABS_Y,  INPUT_UIT16_MIN, INPUT_UIT16_MAX, 0, 0);
        input_set_abs_params(shub_idev[dev_kind], ABS_RX, 0, 0xFFFFFFFF, 0, 0);
        input_set_abs_params(shub_idev[dev_kind], ABS_RY, 0, 0xFFFFFFFF, 0, 0);
        input_set_abs_params(shub_idev[dev_kind], ABS_RZ, 0, 0xFFFFFFFF, 0, 0);
        break;
    case SHUB_EXIF_ACC_SHEX:
        input_set_abs_params(shub_idev[dev_kind], ABS_MISC, 0, 0xFFFFFFFF, 0, 0);
        input_set_abs_params(shub_idev[dev_kind], ABS_VOLUME, 0, 0xFFFFFFFF, 0, 0);
        input_set_abs_params(shub_idev[dev_kind], ABS_X, ACC_MIN, ACC_MAX, 0, 0);
        input_set_abs_params(shub_idev[dev_kind], ABS_Y, ACC_MIN, ACC_MAX, 0, 0);
        input_set_abs_params(shub_idev[dev_kind], ABS_Z, ACC_MIN, ACC_MAX, 0, 0);
        break;
    default:
        break;
    }
}

static void shub_change_notify_val(int *data)
{
    int checkData = *data;
    if(checkData == 1) {
        *data = 2;
    }
    else {
        *data = 1;
    }
}


static void shub_input_report_exif_stat_det(int32_t *data)
{
    unsigned long send_data;
    DBG_EXIF_DATA("stat_det : notify=0x%x cond=%d car=%d train=%d ride=%d(%d)\n", shub_exif_notify_info, shub_transdata_chage(data[0]), data[9], data[10], data[11], shub_exif_notify_cont);
    send_data = shub_transdata_chage(data[0]);
    send_data |= (data[9] << 8) & 0xFF00;
    send_data |= (data[10] << 16) & 0xFF0000;
    send_data |= (data[11] << 24) & 0xFF000000;
    SHUB_INPUT_VAL_CLEAR(shub_idev[SHUB_EXIF_NOTIFY], ABS_Y, shub_exif_notify_info);
    input_report_abs(shub_idev[SHUB_EXIF_NOTIFY], ABS_X, shub_exif_notify_cont);
    input_report_abs(shub_idev[SHUB_EXIF_NOTIFY], ABS_Y, shub_exif_notify_info);
    input_report_abs(shub_idev[SHUB_EXIF_NOTIFY], ABS_RX, send_data);
    input_report_abs(shub_idev[SHUB_EXIF_NOTIFY], ABS_RY, 0);
    input_report_abs(shub_idev[SHUB_EXIF_NOTIFY], ABS_RZ, 0);
    shub_input_sync_init(shub_idev[SHUB_EXIF_NOTIFY]);
    input_event(shub_idev[SHUB_EXIF_NOTIFY], EV_SYN, SYN_REPORT, 1);
    shub_exif_notify_info = 0;
    shub_change_notify_val(&shub_exif_notify_cont);
}

static void shub_input_report_exif_step_det(int32_t data)
{
    DBG_EXIF_DATA("step_det : notify=0x%x step=%d(%d)\n", shub_exif_notify_info, data, shub_exif_notify_cont);
    SHUB_INPUT_VAL_CLEAR(shub_idev[SHUB_EXIF_NOTIFY], ABS_Y, shub_exif_notify_info);
    input_report_abs(shub_idev[SHUB_EXIF_NOTIFY], ABS_X, shub_exif_notify_cont);
    input_report_abs(shub_idev[SHUB_EXIF_NOTIFY], ABS_Y, shub_exif_notify_info);
    input_report_abs(shub_idev[SHUB_EXIF_NOTIFY], ABS_RX, 0);
    input_report_abs(shub_idev[SHUB_EXIF_NOTIFY], ABS_RY, data);
    input_report_abs(shub_idev[SHUB_EXIF_NOTIFY], ABS_RZ, 0);
    shub_input_sync_init(shub_idev[SHUB_EXIF_NOTIFY]);
    input_event(shub_idev[SHUB_EXIF_NOTIFY], EV_SYN, SYN_REPORT, 1);
    shub_exif_notify_info = 0;
    shub_change_notify_val(&shub_exif_notify_cont);
}

static void shub_input_report_exif_stop_tm(int32_t data)
{
    DBG_EXIF_DATA("stop_tm : notify=0x%x time=%d(%d)\n", shub_exif_notify_info, data, shub_exif_notify_cont);
    SHUB_INPUT_VAL_CLEAR(shub_idev[SHUB_EXIF_NOTIFY], ABS_Y, shub_exif_notify_info);
    input_report_abs(shub_idev[SHUB_EXIF_NOTIFY], ABS_X, shub_exif_notify_cont);
    input_report_abs(shub_idev[SHUB_EXIF_NOTIFY], ABS_Y, shub_exif_notify_info);
    input_report_abs(shub_idev[SHUB_EXIF_NOTIFY], ABS_RX, 0);
    input_report_abs(shub_idev[SHUB_EXIF_NOTIFY], ABS_RY, 0);
    input_report_abs(shub_idev[SHUB_EXIF_NOTIFY], ABS_RZ, data);
    shub_input_sync_init(shub_idev[SHUB_EXIF_NOTIFY]);
    input_event(shub_idev[SHUB_EXIF_NOTIFY], EV_SYN, SYN_REPORT, 1);
    shub_exif_notify_info = 0;
    shub_change_notify_val(&shub_exif_notify_cont);
}

void shub_input_report_exif_grav_det(bool send)
{
    if(shub_vibe_notify_check(1) || shub_speaker_notify_check(1)) {
        return ;
    }
    shub_exif_notify_info |= 0x08;
    DBG_EXIF_DATA("grav_det : notify=0x%x send=%d(%d)\n", shub_exif_notify_info, send, shub_exif_notify_cont);
    if(send) {
        SHUB_INPUT_VAL_CLEAR(shub_idev[SHUB_EXIF_NOTIFY], ABS_Y, shub_exif_notify_info);
        input_report_abs(shub_idev[SHUB_EXIF_NOTIFY], ABS_X, shub_exif_notify_cont);
        input_report_abs(shub_idev[SHUB_EXIF_NOTIFY], ABS_Y, shub_exif_notify_info);
        input_report_abs(shub_idev[SHUB_EXIF_NOTIFY], ABS_RX, 0);
        input_report_abs(shub_idev[SHUB_EXIF_NOTIFY], ABS_RY, 0);
        input_report_abs(shub_idev[SHUB_EXIF_NOTIFY], ABS_RZ, 0);
        shub_input_sync_init(shub_idev[SHUB_EXIF_NOTIFY]);
        input_event(shub_idev[SHUB_EXIF_NOTIFY], EV_SYN, SYN_REPORT, 1);
        shub_exif_notify_info = 0;
        shub_change_notify_val(&shub_exif_notify_cont);
    }
}

void shub_input_report_exif_ride_pause_det(bool send, int32_t info)
{
    switch(info) {
    case 1:     // Move
        shub_exif_notify_info |= 0x80;
        break;
    case 2:     // Pause
        shub_exif_notify_info |= 0x40;
        break;
    default:    // invalid
        if(shub_exif_notify_info == 0) {
            send = false;
        }
        break;
    }
    DBG_EXIF_DATA("ride_pause_det : notify=0x%x info=%d send=%d(%d)\n", shub_exif_notify_info, info, send, shub_exif_notify_cont);
    if(send) {
        SHUB_INPUT_VAL_CLEAR(shub_idev[SHUB_EXIF_NOTIFY], ABS_Y, shub_exif_notify_info);
        input_report_abs(shub_idev[SHUB_EXIF_NOTIFY], ABS_X, shub_exif_notify_cont);
        input_report_abs(shub_idev[SHUB_EXIF_NOTIFY], ABS_Y, shub_exif_notify_info);
        input_report_abs(shub_idev[SHUB_EXIF_NOTIFY], ABS_RX, 0);
        input_report_abs(shub_idev[SHUB_EXIF_NOTIFY], ABS_RY, 0);
        input_report_abs(shub_idev[SHUB_EXIF_NOTIFY], ABS_RZ, 0);
        shub_input_sync_init(shub_idev[SHUB_EXIF_NOTIFY]);
        input_event(shub_idev[SHUB_EXIF_NOTIFY], EV_SYN, SYN_REPORT, 1);
        shub_exif_notify_info = 0;
        shub_change_notify_val(&shub_exif_notify_cont);
    }
}

void shub_input_report_exif_pickup_det(void)
{
    shub_exif_notify_info |= SHUB_EXIF_DETECT_PICKUP;
    DBG_EXIF_DATA("pickup : notify=0x%x send=1(%d)\n", shub_exif_notify_info, shub_exif_notify_cont);
    SHUB_INPUT_VAL_CLEAR(shub_idev[SHUB_EXIF_NOTIFY], ABS_Y, shub_exif_notify_info);
    input_report_abs(shub_idev[SHUB_EXIF_NOTIFY], ABS_X, shub_exif_notify_cont);
    input_report_abs(shub_idev[SHUB_EXIF_NOTIFY], ABS_Y, shub_exif_notify_info);
    input_report_abs(shub_idev[SHUB_EXIF_NOTIFY], ABS_RX, 0);
    input_report_abs(shub_idev[SHUB_EXIF_NOTIFY], ABS_RY, 0);
    input_report_abs(shub_idev[SHUB_EXIF_NOTIFY], ABS_RZ, 0);
    shub_input_sync_init(shub_idev[SHUB_EXIF_NOTIFY]);
    input_event(shub_idev[SHUB_EXIF_NOTIFY], EV_SYN, SYN_REPORT, 1);
    shub_exif_notify_info = 0;
    shub_change_notify_val(&shub_exif_notify_cont);
}

#ifdef SHUB_SW_TWIST_DETECT
void shub_input_report_exif_twist_det(unsigned char info)
{
    if(info & INT_GYRO_TWIST_LEFT){
        shub_exif_notify_info |= SHUB_EXIF_TWIST_LEFT;
    }
    if(info & INT_GYRO_TWIST_RIGHT){
        shub_exif_notify_info |= SHUB_EXIF_TWIST_RIGHT;
    }
    DBG_EXIF_DATA("twist : notify=0x%x send=1(%d)\n", shub_exif_notify_info, shub_exif_notify_cont);
    SHUB_INPUT_VAL_CLEAR(shub_idev[SHUB_EXIF_NOTIFY], ABS_Y, shub_exif_notify_info);
    input_report_abs(shub_idev[SHUB_EXIF_NOTIFY], ABS_X, shub_exif_notify_cont);
    input_report_abs(shub_idev[SHUB_EXIF_NOTIFY], ABS_Y, shub_exif_notify_info);
    input_report_abs(shub_idev[SHUB_EXIF_NOTIFY], ABS_RX, 0);
    input_report_abs(shub_idev[SHUB_EXIF_NOTIFY], ABS_RY, 0);
    input_report_abs(shub_idev[SHUB_EXIF_NOTIFY], ABS_RZ, 0);
    shub_input_sync_init(shub_idev[SHUB_EXIF_NOTIFY]);
    input_event(shub_idev[SHUB_EXIF_NOTIFY], EV_SYN, SYN_REPORT, 1);
    shub_exif_notify_info = 0;
    shub_change_notify_val(&shub_exif_notify_cont);
}
#endif

void shub_input_report_exif_mot_det(unsigned char det_info)
{
	int32_t motionData[] = {0,0,0};
    DBG_EXIF_DATA("mot_det : notify=0x%x det=%d(%d)\n", shub_exif_notify_info, det_info, shub_exif_notify_cont);
    if(det_info & 0x01) {
        shub_exif_notify_info |= 0x20;
    }
    if(det_info & 0x02) {
        if(shub_exif_shex_acc_enable && (atomic_read(&shub_detect_mode_flg) == SHUB_DETECT_ECONOMIZE)) {
            mutex_lock(&shub_exif_lock);
            hrtimer_cancel(&poll_timer);
            hrtimer_start(&poll_timer, ns_to_ktime((int64_t)shub_exif_delay_ms * NSEC_PER_MSEC), HRTIMER_MODE_REL);
            mutex_unlock(&shub_exif_lock);
            if(shub_get_already_md_flg() & 0x02) {
                shub_disable_internal_mot_det();
            }
            atomic_set(&shub_detect_mode_flg, SHUB_DETECT_NORMAL);
            DBG_EXIF_DATA( "NORMAL already_md_flg=%d\n", shub_get_already_md_flg());
        }
        if(!(shub_get_already_md_flg() & 0x01) && shub_exif_notify_info == 0) {
            DBG_EXIF_DATA( "mot_det : not notify return. already_md_flg=%d\n", shub_get_already_md_flg());
            return ;
        }
        else {
            shub_exif_notify_info |= 0x10;
            shub_get_data_motion( motionData );
        }
        if(shub_vibe_notify_check(0) || shub_speaker_notify_check(0)) {
            shub_exif_notify_info &= ~0x10;
            if(shub_exif_notify_info == 0){
                return ;
            } else {
                memset( motionData, 0, sizeof(motionData) );
            }
        }
    }
    if(shub_exif_notify_info != 0) {
        SHUB_INPUT_VAL_CLEAR(shub_idev[SHUB_EXIF_NOTIFY], ABS_Y, shub_exif_notify_info);
        input_report_abs(shub_idev[SHUB_EXIF_NOTIFY], ABS_X, shub_exif_notify_cont);
        input_report_abs(shub_idev[SHUB_EXIF_NOTIFY], ABS_Y, shub_exif_notify_info);
        input_report_abs(shub_idev[SHUB_EXIF_NOTIFY], ABS_RX, motionData[0]);
        input_report_abs(shub_idev[SHUB_EXIF_NOTIFY], ABS_RY, motionData[1]);
        input_report_abs(shub_idev[SHUB_EXIF_NOTIFY], ABS_RZ, motionData[2]);
        shub_input_sync_init(shub_idev[SHUB_EXIF_NOTIFY]);
        input_event(shub_idev[SHUB_EXIF_NOTIFY], EV_SYN, SYN_REPORT, 1);
        shub_exif_notify_info = 0;
        shub_change_notify_val(&shub_exif_notify_cont);
    }
}

void shub_input_report_exif_shex_acc(int32_t *data)
{
    if(data == NULL) {
        return;
    }

    data[INDEX_X] = shub_adjust_value(ACC_MIN, ACC_MAX,data[INDEX_X]);
    data[INDEX_Y] = shub_adjust_value(ACC_MIN, ACC_MAX,data[INDEX_Y]);
    data[INDEX_Z] = shub_adjust_value(ACC_MIN, ACC_MAX,data[INDEX_Z]);

    if(shub_exif_shex_acc_enable == 1){
        DBG_EXIF_DATA("shex_acc : x=%d y=%d z=%d\n", data[INDEX_X],  data[INDEX_Y],  data[INDEX_Z]);
        SHUB_INPUT_VAL_CLEAR(shub_idev[SHUB_EXIF_ACC_SHEX], ABS_X, data[INDEX_X]);
        input_report_abs(shub_idev[SHUB_EXIF_ACC_SHEX], ABS_X, data[INDEX_X]);
        input_report_abs(shub_idev[SHUB_EXIF_ACC_SHEX], ABS_Y, data[INDEX_Y]);
        input_report_abs(shub_idev[SHUB_EXIF_ACC_SHEX], ABS_Z, data[INDEX_Z]);
        input_report_abs(shub_idev[SHUB_EXIF_ACC_SHEX], ABS_MISC, data[INDEX_TM]);
        input_report_abs(shub_idev[SHUB_EXIF_ACC_SHEX], ABS_VOLUME, data[INDEX_TMNS]);
        shub_input_sync_init(shub_idev[SHUB_EXIF_ACC_SHEX]);
        input_event(shub_idev[SHUB_EXIF_ACC_SHEX], EV_SYN, SYN_REPORT, 1);
    }
}

void shub_input_report_exif_judge(void)
{
    int32_t ret;
    int notify;
    IoCtlData dt;
    
    ret = shub_get_data_act_detection(dt.m_iData);
    if(ret) {
        printk("[shub]%s shub_get_data_act_detection error\n", __func__);
        return ;
    }
    
    notify = shub_get_notifyinfo(dt.m_iData[6]);
    switch(notify) {
    case SHUB_PEDO_ASN_WALK_CONT:
        shub_exif_notify_info |= 0x02;
        shub_input_report_exif_step_det(dt.m_iData[4]);
        break;
    case SHUB_PEDO_ASN_STOP_CONT:
        shub_exif_notify_info |= 0x04;
        shub_input_report_exif_stop_tm(dt.m_iData[5]);
        break;
    case SHUB_PEDO_ASN_TRANS_DET:
        ret = shub_clear_data(APP_TOTAL_STATUS_DETECTION_CLR_CONT_STEPS);
        if(ret) {
            printk("[shub]shub_clear_data CLR_CONT_STEPS error. ret=%d\n", ret);
        }
        shub_exif_notify_info |= 0x01;
        shub_input_report_exif_stat_det(&dt.m_iData[1]);
        break;
    case SHUB_PEDO_ASN_STOP:
    case SHUB_PEDO_ASN_WALK:
    case SHUB_PEDO_ASN_TRANS_FIX:
    case SHUB_PEDO_ASN_TRANS_TYPE:
    case SHUB_PEDO_ASN_TRANS_REL_WALK:
    case SHUB_PEDO_ASN_TRANS_REL_STIL:
        shub_exif_notify_info |= 0x01;
        shub_input_report_exif_stat_det(&dt.m_iData[1]);
        break;
    default:
        break;
    }
}

int shub_set_default_parameter(void)
{
    int32_t ret = 0;
    IoCtlParam param;
    
    memset(&param, 0, sizeof(IoCtlParam));
    param.m_iType = APP_PEDOMETER_N;
    ret = shub_get_param(param.m_iType, param.m_iParam);
    if(ret != 0) {
        printk( "[shub]%s get PEDOMETER_N error. ret=%d\n", __func__, ret);
        return ret;
    }
    param.m_iParam[1] = 74;
    param.m_iParam[2] = 60;
    param.m_iParam[3] = 1;
    param.m_iParam[4] = 10;
    param.m_iParam[8] = 5;
    param.m_iParam[9] = 5;
    param.m_iParam[10] = 30;
    param.m_iParam[11] = 200;
    param.m_iParam[12] = 0;
    param.m_iParam[13] = 0;
    ret = shub_set_param(param.m_iType, param.m_iParam);
    if(ret != 0) {
        printk( "[shub]%s set PEDOMETER_N error. ret=%d\n", __func__, ret);
        return ret;
    }

    memset(&param, 0, sizeof(IoCtlParam));
    param.m_iType = APP_GDETECTION;
    ret = shub_get_param(param.m_iType, param.m_iParam);
    if(ret != 0) {
        printk( "[shub]%s get APP_GDETECTION error. ret=%d\n", __func__, ret);
        return ret;
    }
    param.m_iParam[1] = 0;
    param.m_iParam[2] = 200;
    param.m_iParam[3] = 0;
    param.m_iParam[4] = 7;
    param.m_iParam[5] = 7;
    param.m_iParam[6] = 0;
    ret = shub_set_param(param.m_iType, param.m_iParam);
    if(ret != 0) {
        printk( "[shub]%s set APP_GDETECTION error. ret=%d\n", __func__, ret);
        return ret;
    }

    memset(&param, 0, sizeof(IoCtlParam));
    param.m_iType = APP_MOTDTECTION;
    ret = shub_get_param(param.m_iType, param.m_iParam);
    if(ret != 0) {
        printk( "[shub]%s get APP_MOTDTECTION error. ret=%d\n", __func__, ret);
        return ret;
    }
    param.m_iParam[2] = 16;
    param.m_iParam[3] = 16;
    param.m_iParam[4] = 16;
//    param.m_iParam[5] = 38;
//    param.m_iParam[6] = 38;
//    param.m_iParam[7] = 63;
    param.m_iParam[9] |= 0x80;
    ret = shub_set_param(param.m_iType, param.m_iParam);
    if(ret != 0) {
        printk( "[shub]%s set APP_MOTDTECTION error. ret=%d\n", __func__, ret);
        return ret;
    }

    memset(&param, 0, sizeof(IoCtlParam));
    param.m_iType = APP_PEDOMETER2_2;
    ret = shub_get_param(param.m_iType, param.m_iParam);
    if(ret != 0) {
        printk( "[shub]%s get APP_PEDOMETER2_2 error. ret=%d\n", __func__, ret);
        return ret;
    }
    param.m_iParam[1] = 20;
    param.m_iParam[3] = 1;
    param.m_iParam[5] = 20;
    ret = shub_set_param(param.m_iType, param.m_iParam);
    if(ret != 0) {
        printk( "[shub]%s set APP_PEDOMETER2_2 error. ret=%d\n", __func__, ret);
        return ret;
    }

#ifdef SHUB_SW_PICKUP_DETECT
    memset(&param, 0, sizeof(IoCtlParam));
    param.m_iType = APP_PICKUP_ENABLE;
    param.m_iParam[0] = 0;
    param.m_iParam[1] = SHUB_PICKUP_ENABLE_PARAM;
    ret = shub_set_param(param.m_iType, param.m_iParam);
    if(ret != 0) {
        printk( "[shub]%s set APP_PICKUP_ENABLE error. ret=%d\n", __func__, ret);
        ret = 0;
//      return ret;
    }
#endif

    memset(&param, 0, sizeof(IoCtlParam));
    param.m_iType = APP_LPM_PARAM;
    ret = shub_get_param(param.m_iType, param.m_iParam);
    if(ret != 0) {
        printk( "[shub]%s get APP_LPM_PARAM error. ret=%d\n", __func__, ret);
        return ret;
    }
    param.m_iParam[0]  = 1;
    param.m_iParam[3]  = 0;
    param.m_iParam[4]  = 0x1B;
    param.m_iParam[5]  = 32;
    param.m_iParam[6]  = 10;
    param.m_iParam[7]  = 0x07;
    param.m_iParam[8]  = 32;
    param.m_iParam[9]  = 32;
    param.m_iParam[10] = 32;
    ret = shub_set_param(param.m_iType, param.m_iParam);
    if(ret != 0) {
        printk( "[shub]%s set APP_LPM_PARAM error. ret=%d\n", __func__, ret);
        return ret;
    }

    memset(&param, 0, sizeof(IoCtlParam));
    param.m_iType = APP_LPM_DEV_PARAM;
    ret = shub_get_param(param.m_iType, param.m_iParam);
    if(ret != 0) {
        printk( "[shub]%s get APP_LPM_DEVPARAM error. ret=%d\n", __func__, ret);
        return ret;
    }
#ifdef CONFIG_ACC_U2DH
    param.m_iParam[1]  = 2;
    param.m_iParam[5]  = 2;
    param.m_iParam[7]  = 1;
#else
    param.m_iParam[0]  = 1;
    param.m_iParam[2]  = 1;
    param.m_iParam[3]  = 1;
#endif
    ret = shub_set_param(param.m_iType, param.m_iParam);
    if(ret != 0) {
        printk( "[shub]%s set APP_LPM_DEVPARAM error. ret=%d\n", __func__, ret);
        return ret;
    }

#if 0
    memset(&param, 0, sizeof(IoCtlParam));
    param.m_iType = APP_RIDE_PEDO2_PARAM;
    ret = shub_get_param(param.m_iType, param.m_iParam);
    if(ret != 0) {
        printk( "[shub]%s get APP_RIDE_PEDO2_PARAM error. ret=%d\n", __func__, ret);
        return ret;
    }
    ret = shub_set_param(param.m_iType, param.m_iParam);
    if(ret != 0) {
        printk( "[shub]%s set APP_RIDE_PEDO2_PARAM error. ret=%d\n", __func__, ret);
        return ret;
    }
#endif
    memset(&param, 0, sizeof(IoCtlParam));
    param.m_iType = APP_LPM_DEV_PARAM2;
    ret = shub_get_param(param.m_iType, param.m_iParam);
    if(ret != 0) {
        printk( "[shub]%s get APP_LPM_DEV_PARAM2 error. ret=%d\n", __func__, ret);
        return ret;
    }
    param.m_iParam[2]  = 3600;
    param.m_iParam[3]  = 10;
    ret = shub_set_param(param.m_iType, param.m_iParam);
    if(ret != 0) {
        printk( "[shub]%s set APP_LPM_DEV_PARAM2 error. ret=%d\n", __func__, ret);
        return ret;
    }
// #endif

    ret = shub_set_default_ped_parameter();

    return ret;
}

int shub_set_default_ped_parameter(void)
{
    int32_t ret = 0;
    IoCtlParam param;
    
    memset(&param, 0, sizeof(IoCtlParam));
    param.m_iType = APP_PEDOMETER;
    ret = shub_get_param(param.m_iType, param.m_iParam);
    if(ret != 0) {
        printk( "[shub]%s get PEDOMETER error. ret=%d\n", __func__, ret);
        return ret;
    }
    param.m_iParam[1] = 74;
    param.m_iParam[2] = 60;
    param.m_iParam[4] = 10;
    param.m_iParam[8] = 5;
    param.m_iParam[9] = 5;
    param.m_iParam[10] = 30;
    param.m_iParam[11] = 200;
    param.m_iParam[12] = 0;
    param.m_iParam[13] = 2;
    ret = shub_set_param(param.m_iType, param.m_iParam);
    if(ret != 0) {
        printk( "[shub]%s set PEDOMETER error. ret=%d\n", __func__, ret);
        return ret;
    }

    memset(&param, 0, sizeof(IoCtlParam));
    param.m_iType = APP_VEICHLE_DETECTION;
    ret = shub_get_param(param.m_iType, param.m_iParam);
    if(ret != 0) {
        printk( "[shub]%s get VEICHLE_DETECTION error. ret=%d\n", __func__, ret);
        return ret;
    }
    param.m_iParam[2] = 20;
    param.m_iParam[4] = 6;
    param.m_iParam[5] = 10;
    param.m_iParam[7] = 50;
    param.m_iParam[8] = 90;
    param.m_iParam[9] = 6;
    param.m_iParam[10] = 0;
    param.m_iParam[11] = 2;
    param.m_iParam[12] = 0;
    ret = shub_set_param(param.m_iType, param.m_iParam);
    if(ret != 0) {
        printk( "[shub]%s set VEICHLE_DETECTION error. ret=%d\n", __func__, ret);
        return ret;
    }

    memset(&param, 0, sizeof(IoCtlParam));
    param.m_iType = APP_TOTAL_STATUS_DETECTION;
    ret = shub_get_param(param.m_iType, param.m_iParam);
    if(ret != 0) {
        printk( "[shub]%s get TOTAL_STATUS_DETECTION error. ret=%d\n", __func__, ret);
        return ret;
    }
    param.m_iParam[1] = 0x37a;
    param.m_iParam[2] = 21;
    param.m_iParam[3] = 21;
    param.m_iParam[4] = 2;
    param.m_iParam[5] = 1;
    param.m_iParam[8] = 0x0202;
    param.m_iParam[9] = 1;
    ret = shub_set_param(param.m_iType, param.m_iParam);
    if(ret != 0) {
        printk( "[shub]%s set TOTAL_STATUS_DETECTION error. ret=%d\n", __func__, ret);
        return ret;
    }

    memset(&param, 0, sizeof(IoCtlParam));
    param.m_iType = APP_PEDO_ALL_STATE_2ND;
    ret = shub_get_param(param.m_iType, param.m_iParam);
    if(ret != 0) {
        printk( "[shub]%s get PEDO_ALL_STATE_2ND error. ret=%d\n", __func__, ret);
        return ret;
    }
    param.m_iParam[1] = 0x01;
    ret = shub_set_param(param.m_iType, param.m_iParam);
    if(ret != 0) {
        printk( "[shub]%s set PEDO_ALL_STATE_2ND error. ret=%d\n", __func__, ret);
        return ret;
    }

    memset(&param, 0, sizeof(IoCtlParam));
    param.m_iType = APP_TOTAL_STATUS_DETECTION2;
    ret = shub_get_param(param.m_iType, param.m_iParam);
    if(ret != 0) {
        printk( "[shub]%s get TOTAL_STATUS_DETECTION2 error. ret=%d\n", __func__, ret);
        return ret;
    }
    //param.m_iParam[4] = 2;
    param.m_iParam[5] = 1;
    param.m_iParam[8] = 0x027E;
    param.m_iParam[9] = 1;
    ret = shub_set_param(param.m_iType, param.m_iParam);
    if(ret != 0) {
        printk( "[shub]%s set TOTAL_STATUS_DETECTION2 error. ret=%d\n", __func__, ret);
        return ret;
    }

    memset(&param, 0, sizeof(IoCtlParam));
    param.m_iType = APP_VEICHLE_DETECTION2;
    ret = shub_get_param(param.m_iType, param.m_iParam);
    if(ret != 0) {
        printk( "[shub]%s get VEICHLE_DETECTION2 error. ret=%d\n", __func__, ret);
        return ret;
    }
    param.m_iParam[1] = 0x384;
    param.m_iParam[2] = 0x384;
//  param.m_iParam[3] = 0x07;
//  param.m_iParam[4] = 6;
//  param.m_iParam[5] = 20;
//  param.m_iParam[6] = 0x07;
//  param.m_iParam[7] = 6;
//  param.m_iParam[8] = 40;
    ret = shub_set_param(param.m_iType, param.m_iParam);
    if(ret != 0) {
        printk( "[shub]%s set VEICHLE_DETECTION2 error. ret=%d\n", __func__, ret);
        return ret;
    }

    memset(&param, 0, sizeof(IoCtlParam));
    param.m_iType = APP_PEDOMETER2;
    ret = shub_get_param(param.m_iType, param.m_iParam);
    if(ret != 0) {
        printk( "[shub]%s get APP_PEDOMETER2 error. ret=%d\n", __func__, ret);
        return ret;
    }
    param.m_iParam[1] = 20;
    param.m_iParam[3] = 1;
    param.m_iParam[5] = 20;
    param.m_iParam[8] = 260;
    param.m_iParam[9] = 1500;
    ret = shub_set_param(param.m_iType, param.m_iParam);
    if(ret != 0) {
        printk( "[shub]%s set APP_PEDOMETER2 error. ret=%d\n", __func__, ret);
        return ret;
    }

    memset(&param, 0, sizeof(IoCtlParam));
    param.m_iType = APP_VEICHLE_DETECTION3;
    ret = shub_get_param(param.m_iType, param.m_iParam);
    if(ret != 0) {
        printk( "[shub]%s get VEICHLE_DETECTION3 error. ret=%d\n", __func__, ret);
        return ret;
    }
    param.m_iParam[0] = 0;
    param.m_iParam[4] = 5;
    ret = shub_set_param(param.m_iType, param.m_iParam);
    if(ret != 0) {
        printk( "[shub]%s set VEICHLE_DETECTION3 error. ret=%d\n", __func__, ret);
        return ret;
    }

    //memset(&param, 0, sizeof(IoCtlParam));
    //param.m_iType = APP_PAUSE_PARAM;
    //ret = shub_get_param(param.m_iType, param.m_iParam);
    //if(ret != 0) {
    //    printk( "[shub]%s get APP_PAUSE_PARAM error. ret=%d\n", __func__, ret);
    //    return ret;
    //}
    //param.m_iParam[0] = 0x07;
    //param.m_iParam[1] = 0x07;
    //param.m_iParam[2] = 0;
    //ret = shub_set_param(param.m_iType, param.m_iParam);
    //if(ret != 0) {
    //    printk( "[shub]%s set APP_PAUSE_PARAM error. ret=%d\n", __func__, ret);
    //    return ret;
    //}

    memset(&param, 0, sizeof(IoCtlParam));
    param.m_iType = APP_PAUSE_STATUS_PARAM;
    //ret = shub_get_param(param.m_iType, param.m_iParam);
    //if(ret != 0) {
    //    printk( "[shub]%s get APP_PAUSE_STATUS_PARAM error. ret=%d\n", __func__, ret);
    //    return ret;
    //}
    param.m_iParam[0]  = 0x07;
    param.m_iParam[1]  = 0;
    param.m_iParam[2]  = 6;
    param.m_iParam[3]  = 20;
    param.m_iParam[4]  = 200;
    param.m_iParam[5]  = 100;
    param.m_iParam[6]  = 10;
    param.m_iParam[7]  = 1;
    param.m_iParam[8]  = 1;
    param.m_iParam[9]  = 0;
    param.m_iParam[10] = 6;
    param.m_iParam[11] = 40;
    param.m_iParam[12] = 0;
    ret = shub_set_param(param.m_iType, param.m_iParam);
    if(ret != 0) {
        printk( "[shub]%s set APP_PAUSE_STATUS_PARAM error. ret=%d\n", __func__, ret);
        return ret;
    }

#if 0
    memset(&param, 0, sizeof(IoCtlParam));
    param.m_iType = APP_RIDE_PEDO_PARAM;
    ret = shub_get_param(param.m_iType, param.m_iParam);
    if(ret != 0) {
        printk( "[shub]%s get APP_RIDE_PEDO_PARAM error. ret=%d\n", __func__, ret);
        return ret;
    }
    ret = shub_set_param(param.m_iType, param.m_iParam);
    if(ret != 0) {
        printk( "[shub]%s set APP_RIDE_PEDO_PARAM error. ret=%d\n", __func__, ret);
        return ret;
    }
#endif
    return ret;
}

void shub_set_param_exif_md_mode(int32_t *param)
{
    if(param == NULL) {
        return ;
    }
    switch(shub_exif_md_mode_flg) {
    case 0:     // MODE_NORMAL
        param[5] = 38;
        param[6] = 38;
        param[7] = 63;
        break;
    case 1:     // MODE_1
        param[5] = 6;
        param[6] = 6;
        param[7] = 6;
        break;
    case 2:     // MODE_2
        param[5] = 4;
        param[6] = 4;
        param[7] = 4;
        break;
    case 3:     // MODE_3
        param[5] = 2;
        param[6] = 2;
        param[7] = 2;
        break;
    default:
        break;
    }
    DBG_EXIF_DATA( "%s mode=%d\n", __func__, shub_exif_md_mode_flg);
}

void shub_set_exif_md_mode_flg(int32_t mode)
{
    shub_exif_md_mode_flg = mode;
}

int32_t shub_get_exif_md_mode_flg(void)
{
    return shub_exif_md_mode_flg;
}

void shub_set_already_md_flg(int data)
{
    shub_already_md_flg |= data;
}

void shub_clr_already_md_flg(int data)
{
    shub_already_md_flg &= ~data;
}

int shub_get_already_md_flg(void)
{
    return shub_already_md_flg;
}

void shub_suspend_exif(void)
{
    DBG_EXIF_DATA( "%s en=%d\n", __func__, shub_exif_shex_acc_enable);
    if(shub_exif_shex_acc_enable != 0){
        shub_set_sensor_poll(0);
        cancel_work_sync(&sensor_poll_work);
    }
//    if(shub_check_ped_type & (TYPE_STOP_NOW | TYPE_STOPPING_NOW)) {
    if(shub_get_mcu_ped_enable() && shub_check_suspend_restart) {
        DBG_EXIF_DATA( "%s restart start\n", __func__);
        hrtimer_cancel(&poll_restart_ped_timer);
        mutex_lock(&shub_mutex_req);
        shub_check_ped_type |= TYPE_RESTART_NOW;
        shub_stop_ped_onoff(2);
        shub_check_ped_type = 0;
        shub_enable_ped_exif_flg = 1;
        mutex_unlock(&shub_mutex_req);
        DBG_EXIF_DATA( "%s restart end\n", __func__);
    }
    shub_check_suspend_restart = 0;
    mutex_lock(&shub_mutex_vibe);
    shub_vibe_endtime_ns = 0;
    shub_not_notify_vibe_flg = 0;
    mutex_unlock(&shub_mutex_vibe);
    mutex_lock(&shub_mutex_speaker);
    shub_speaker_endtime_ns = 0;
    shub_not_notify_speaker_flg = 0;
    mutex_unlock(&shub_mutex_speaker);
}

void shub_resume_exif(void)
{
    DBG_EXIF_DATA( "%s en=%d\n", __func__, shub_exif_shex_acc_enable);
    if(shub_exif_shex_acc_enable != 0){
        shub_set_sensor_poll(1);
    }
}

int shub_get_exif_delay_ms(void)
{
    return shub_exif_delay_ms;
}

static int shub_stop_ped_cmd_setting(int onoff, int iType, int idx)
{
    int ret = 0;
    int set_flg = 0;
    IoCtlParam param;

    memset(&param, 0, sizeof(IoCtlParam));
    param.m_iType = iType;
    ret = shub_get_param(param.m_iType, param.m_iParam);
    if(ret != 0) {
        printk( "[shub]%s get error. ret=%d\n", __func__, ret);
        return ret;
    }
    if(onoff == 0) {
        shub_ped_on_inf[idx] = param.m_iParam[0];
        if(param.m_iParam[0] == 1) {
            param.m_iParam[0] = 0;
            set_flg = 1;
        }
    }
    else if(onoff == 1) {
        if((shub_ped_on_inf[idx] == 1) && (param.m_iParam[0] == 0)) {
            param.m_iParam[0] = 1;
            set_flg = 1;
        }
    }
    else {
        if((shub_ped_on_inf[idx] == 1) && (param.m_iParam[0] == 0)) {
            param.m_iParam[0] = 1;
            set_flg = 1;
        }
        else if((idx == SHUB_IDX_PEDO || idx == SHUB_IDX_TOTAL) && (param.m_iParam[0] == 0)) {
            param.m_iParam[0] = 1;
            set_flg = 1;
        }
    }
    if(set_flg) {
        ret = shub_set_param(param.m_iType, param.m_iParam);
        if(ret != 0) {
            printk( "[shub]%s set error. ret=%d\n", __func__, ret);
            return ret;
        }
    }
    return ret;
}

static int shub_stop_ped_onoff(int onoff)
{
    int ret = 0;

    shub_stop_ped_cmd_setting(onoff, APP_PEDOMETER, SHUB_IDX_PEDO);
    if(ret != 0) {
        printk( "[shub]%s PEDOMETER error. ret=%d\n", __func__, ret);
        return ret;
    }
    shub_stop_ped_cmd_setting(onoff, APP_RUN_DETECTION, SHUB_IDX_RUN);
    if(ret != 0) {
        printk( "[shub]%s RUN error. ret=%d\n", __func__, ret);
        return ret;
    }
    shub_stop_ped_cmd_setting(onoff, APP_VEICHLE_DETECTION, SHUB_IDX_VEICHLE);
    if(ret != 0) {
        printk( "[shub]%s VEICHLE error. ret=%d\n", __func__, ret);
        return ret;
    }
    shub_stop_ped_cmd_setting(onoff, APP_TOTAL_STATUS_DETECTION, SHUB_IDX_TOTAL);
    if(ret != 0) {
        printk( "[shub]%s TOTAL error. ret=%d\n", __func__, ret);
        return ret;
    }
    return ret;
}

static enum hrtimer_restart shub_restart_ped(struct hrtimer *tm)
{
    DBG_EXIF_DATA( "%s ---timer end---\n", __func__);
    schedule_work(&restart_ped_work);
    return HRTIMER_NORESTART;
}

static void shub_stop_ped_func_work(struct work_struct *work)
{
    DBG_EXIF_DATA( "%s check_ped_type=%02x start\n", __func__, shub_check_ped_type);
    mutex_lock(&shub_mutex_req);
    shub_check_ped_type |= TYPE_STOPPING_NOW;
    shub_stop_ped_onoff(0);
    shub_check_ped_type |= TYPE_STOP_NOW;
    mutex_unlock(&shub_mutex_req);
    DBG_EXIF_DATA( "%s check_ped_type=%02x end\n", __func__, shub_check_ped_type);
    return ;
}

static void shub_restart_ped_func_work(struct work_struct *work)
{
    DBG_EXIF_DATA( "%s check_ped_type=%02x start\n", __func__, shub_check_ped_type);
    mutex_lock(&shub_mutex_req);
    shub_check_ped_type |= TYPE_RESTART_NOW;
    shub_stop_ped_onoff(1);
    shub_check_ped_type = 0;
    mutex_unlock(&shub_mutex_req);
    DBG_EXIF_DATA( "%s check_ped_type=%02x end\n", __func__, shub_check_ped_type);
    return ;
}

void shub_set_param_check_exif(int type, int *data)
{
    if(shub_check_ped_type & TYPE_RESTART_NOW) {
        return ;
    }
    if(shub_check_ped_type & TYPE_STOP_NOW) {
        switch(type) {
        case APP_PEDOMETER:
            shub_ped_on_inf[SHUB_IDX_PEDO] = data[0];
            break;
        case APP_RUN_DETECTION:
            shub_ped_on_inf[SHUB_IDX_RUN] = data[0];
            break;
        case APP_VEICHLE_DETECTION:
            shub_ped_on_inf[SHUB_IDX_VEICHLE] = data[0];
            break;
        case APP_TOTAL_STATUS_DETECTION:
            shub_ped_on_inf[SHUB_IDX_TOTAL] = data[0];
            break;
        default:
            return ;
        }
        DBG_EXIF_DATA( "%s type=%02x en=%d->0\n", __func__, type, data[0]);
        data[0] = 0;
    }
}

void shub_get_param_check_exif(int type, int *data)
{
//  if(shub_check_ped_type & TYPE_RESTART_NOW) {
//      return ;
//  }
//  if(shub_check_ped_type & TYPE_STOP_NOW) {
    if(shub_check_ped_type & (TYPE_STOP_NOW | TYPE_RESTART_NOW)) {
#ifdef CONFIG_ANDROID_ENGINEERING
        int getData = data[0];
#endif
        switch(type) {
        case APP_PEDOMETER:
            data[0] = shub_ped_on_inf[SHUB_IDX_PEDO];
            break;
        case APP_RUN_DETECTION:
            data[0] = shub_ped_on_inf[SHUB_IDX_RUN];
            break;
        case APP_VEICHLE_DETECTION:
            data[0] = shub_ped_on_inf[SHUB_IDX_VEICHLE];
            break;
        case APP_TOTAL_STATUS_DETECTION:
            data[0] = shub_ped_on_inf[SHUB_IDX_TOTAL];
            break;
        default:
            return ;
        }
        DBG_EXIF_DATA( "%s type=%02x en=%d->%d\n", __func__, type, getData, data[0]);
    }
}

void shub_set_enable_ped_exif_flg(int en)
{
    if(shub_check_ped_type & TYPE_STOP_NOW) {
        shub_enable_ped_exif_flg = shub_ped_on_inf[SHUB_IDX_PEDO];
    }
    else {
        shub_enable_ped_exif_flg = en;
    }
    DBG_EXIF_DATA( "%s en=%d\n", __func__, shub_enable_ped_exif_flg);
}

static struct file_operations shub_fops = {
    .owner   = THIS_MODULE,
    .unlocked_ioctl = shub_exif_ioctl_wrapper,
#ifdef CONFIG_COMPAT
    .compat_ioctl   = shub_exif_ioctl_wrapper,
#endif
};

void shub_sensor_rep_input_exif(struct seq_file *s)
{
    seq_printf(s, "[exif      ]shub_exif_notify_cont=%d\n",shub_exif_notify_cont);
    seq_printf(s, "[exif      ]shub_exif_notify_info=%d\n",shub_exif_notify_info);
    seq_printf(s, "[exif      ]shub_check_ped_type  =%d\n",shub_check_ped_type);
    seq_printf(s, "[exif      ]shub_ped_on_inf ");
    seq_printf(s, "[0]=%d, ",shub_ped_on_inf[0]);
    seq_printf(s, "[1]=%d, ",shub_ped_on_inf[1]);
    seq_printf(s, "[2]=%d, ",shub_ped_on_inf[2]);
    seq_printf(s, "[3]=%d, ",shub_ped_on_inf[3]);
    seq_printf(s, "[4]=%d\n",shub_ped_on_inf[4]);
    
    seq_printf(s, "[exif      ]shub_latest_ex_AccData ");
    seq_printf(s, "[0]=%d, ",shub_latest_ex_AccData[0]);
    seq_printf(s, "[1]=%d, ",shub_latest_ex_AccData[1]);
    seq_printf(s, "[2]=%d, ",shub_latest_ex_AccData[2]);
    seq_printf(s, "[3]=%d, ",shub_latest_ex_AccData[3]);
    seq_printf(s, "[4]=%d\n",shub_latest_ex_AccData[4]);
    
    seq_printf(s, "[exif      ]shub_enable_ped_exif_flg=%d\n",shub_enable_ped_exif_flg);
    seq_printf(s, "[exif      ]shub_check_suspend_restart=%d\n",shub_check_suspend_restart);
    seq_printf(s, "[exif      ]shub_detect_mode_flg=%d\n",atomic_read(&shub_detect_mode_flg));
    seq_printf(s, "[exif      ]shub_already_md_flg=%d\n",shub_already_md_flg);
    seq_printf(s, "[exif      ]shub_not_notify_vibe_flg=%d\n",shub_not_notify_vibe_flg);
}

static struct miscdevice shub_exif_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = MISC_DEV_NAME_EXIF,
    .fops  = &shub_fops,
};

int32_t shub_probe_exif(struct platform_device *pfdev)
{
    int cnt;
    int i;
    int j;
    int32_t ret = 0;

    if(!shub_connect_check()){
        printk(KERN_INFO "shub_exif Connect Error!!\n");
        ret = -ENODEV;
        goto out_driver;
    }

    shub_exif_shex_acc_enable = 0;
    shub_exif_delay_ms = 240;
    
    for(cnt = 0; cnt < SHUB_EXIF_MAXNUM; cnt++){
        shub_pdev[cnt] = NULL;
        shub_idev[cnt] = NULL;
    }
    for(cnt = 0; cnt < SHUB_EXIF_MAXNUM; cnt++){

        shub_pdev[cnt] = platform_device_register_simple(shub_dev_name[cnt], -1, NULL, 0);
        if (IS_ERR(shub_pdev[cnt])) {
            ret = PTR_ERR(shub_pdev[cnt]);
            goto out_driver;
        }
        shub_idev[cnt] = input_allocate_device();
        if (!shub_idev[cnt]) {
            ret = -ENOMEM;
            goto out_driver;
        }
        shub_idev[cnt]->name = shub_dev_name[cnt];
        shub_idev[cnt]->phys = shub_dev_phys[cnt];
        shub_idev[cnt]->id.bustype = BUS_HOST;
        shub_idev[cnt]->dev.parent = &shub_pdev[cnt]->dev;
        shub_idev[cnt]->evbit[0] = BIT_MASK(EV_ABS);

        shub_set_abs_params(cnt);

        ret = input_register_device(shub_idev[cnt]);
        if (ret)
            goto out_driver;
    }

    ret = misc_register(&shub_exif_device);
    if (ret) {
        printk("%s: shub_io_device register failed\n", __func__);
        goto out_driver;
    }
    INIT_WORK(&sensor_poll_work, shub_sensor_poll_work_func);
    shub_check_ped_type = 0;
    shub_enable_ped_exif_flg = 0;
    shub_check_suspend_restart = 0;
    memset(shub_ped_on_inf, 0, sizeof(shub_ped_on_inf));
    INIT_WORK(&stop_ped_work, shub_stop_ped_func_work);
    INIT_WORK(&restart_ped_work, shub_restart_ped_func_work);
    hrtimer_init(&poll_restart_ped_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    poll_restart_ped_timer.function = shub_restart_ped;
    hrtimer_init(&poll_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    poll_timer.function = shub_sensor_poll;

    shub_count = 0;
    shub_full_buffer_flg = 0;
    shub_clr_already_md_flg(1);
    shub_clr_already_md_flg(2);
    for (i = 0; i < 3; i++)
    {
        shub_detect_buf[i] = (signed short *)kmalloc(shub_buf_size * sizeof(signed short), GFP_KERNEL);
        if (NULL == shub_detect_buf[i])
        {
            for (j = 0; j < i; j ++)
            {
                kfree(shub_detect_buf[j]);
            }
            printk("%s: kmalloc failed\n", __func__);
            goto out_driver;
        }
        memset(shub_detect_buf[i], 0, shub_buf_size * sizeof(signed short));
    }

    shub_vibe_endtime_ns = 0;
    shub_not_notify_vibe_flg = 0;

    return 0;

out_driver:
    for(cnt = 0; cnt < SHUB_EXIF_MAXNUM; cnt++){
        if(shub_idev[cnt] != NULL){
            input_free_device(shub_idev[cnt]);
            printk("%s: input_free_device[%d]\n", __func__, cnt);
        }
        if(shub_pdev[cnt] != NULL){
            platform_device_unregister(shub_pdev[cnt]);
            printk("%s: platform_device_unregister[%d]\n", __func__, cnt);
        }
    }
    return ret;
}

int32_t shub_remove_exif(struct platform_device *pfdev)
{
    int cnt;

    misc_deregister(&shub_exif_device);

    for(cnt = 0; cnt < SHUB_EXIF_MAXNUM; cnt++){
        input_unregister_device(shub_idev[cnt]);
    }
    for(cnt = 0; cnt < SHUB_EXIF_MAXNUM; cnt++){
        input_free_device(shub_idev[cnt]);
    }
    for(cnt = 0; cnt < SHUB_EXIF_MAXNUM; cnt++){
        platform_device_unregister(shub_pdev[cnt]);
    }
    for (cnt = 0; cnt < 3; cnt++){
        kfree(shub_detect_buf[cnt]);
        shub_detect_buf[cnt] = NULL;
    }
    cancel_work_sync(&sensor_poll_work);
    cancel_work_sync(&stop_ped_work);
    cancel_work_sync(&restart_ped_work);
    return 0;
}

static struct platform_driver shub_exif_driver = {
    .probe = shub_probe_exif,
    .remove = shub_remove_exif,
    .shutdown = NULL,
    .driver = {
        .name = "shub_dev_exif",
        .of_match_table = shub_of_match_tb_exif,
    },
};

static int32_t __init shub_exif_init(void)
{
    int ret;
    
    ret = platform_driver_register(&shub_exif_driver);

    return ret;
}

static void __exit shub_exif_exit(void)
{
    platform_driver_unregister(&shub_exif_driver);
}

late_initcall(shub_exif_init);
module_exit(shub_exif_exit);

MODULE_DESCRIPTION("SensorHub Input Device (ExpansionIF)");
MODULE_AUTHOR("LAPIS SEMICOMDUCTOR");
MODULE_LICENSE("GPL v2");

