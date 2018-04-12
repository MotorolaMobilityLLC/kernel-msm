
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
 * @file    ilitek_drv_main.c
 *
 * @brief   This file defines the interface of touch screen
 *
 *
 */

/*=============================================================*/

/*INCLUDE FILE*/

/*=============================================================*/

#include "ilitek_drv_common.h"

#ifdef CONFIG_ENABLE_ITO_MP_TEST
#include "ilitek_drv_mp_test.h"
#endif /*CONFIG_ENABLE_ITO_MP_TEST */

/*=============================================================*/

/*VARIABLE DECLARATION*/

/*=============================================================*/

#if defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM)

static struct work_struct gfinger_touch_work;
int girq = -1;

#ifdef CONFIG_ENABLE_TOUCH_PIN_CONTROL
static int g_gpio_rst = 0;
static int g_gpio_irq = 0;
int ms_ts_msg_ic_gpio_rst = 0;
int ms_ts_msg_ic_gpio_int = 0;

static struct pinctrl *g_ts_pin_ctrl = NULL;
static struct pinctrl_state *g_pin_ctrl_state_active = NULL;
static struct pinctrl_state *g_pin_ctrl_state_suspend = NULL;
static struct pinctrl_state *g_pin_ctrl_state_release = NULL;
#endif /*CONFIG_ENABLE_TOUCH_PIN_CONTROL */

#ifdef CONFIG_ENABLE_NOTIFIER_FB
static struct notifier_block g_fb_notifier;
#else
#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend g_early_suspend;
#endif
#endif /*CONFIG_ENABLE_NOTIFIER_FB */

#ifdef CONFIG_ENABLE_PROXIMITY_DETECTION
struct input_dev *g_proximityinput_device = NULL;

u8 g_enable_tp_proximity = 0;
u8 g_face_closing_tp = 0; /*for QCOM platform -> 1 : close to, 0 : far away */
#endif /*CONFIG_ENABLE_PROXIMITY_DETECTION */

#elif defined(CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM)

#ifdef CONFIG_USE_IRQ_INTERRUPT_FOR_MTK_PLATFORM
static struct work_struct gfinger_touch_work;
#else

/*summer*/
static declare_wait_queue_head(g_waiter);
static struct task_struct *g_thread = NULL;
static int g_tpd_flag = 0;
#endif /*CONFIG_USE_IRQ_INTERRUPT_FOR_MTK_PLATFORM */

#ifdef CONFIG_ENABLE_PROXIMITY_DETECTION
u8 g_enable_tp_proximity = 0;
u8 g_face_closing_tp = 1; /*for MTK platform -> 0 : close to, 1 : far away */
#endif /*CONFIG_ENABLE_PROXIMITY_DETECTION */

#ifdef CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD

int girq = -1;
int ms_ts_msg_ic_gpio_rst = 0;  /*Must set a value other than 1 */
int ms_ts_msg_ic_gpio_int = 1;  /*Must set value as 1 */
#endif /*CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD */
#endif

#ifdef CONFIG_ENABLE_JNI_INTERFACE
static struct msg_tool_drv_cmd_t *g_msg_tool_cmd_in = NULL;
static u8 *g_snd_cmd_data = NULL;
static u8 *g_rtn_cmd_data = NULL;
#endif /*CONFIG_ENABLE_JNI_INTERFACE */

#ifdef CONFIG_ENABLE_ITO_MP_TEST
static ito_test_mode_e g_ito_test_mode = 0;
#endif /*CONFIG_ENABLE_ITO_MP_TEST */

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
#if defined(CONFIG_ENABLE_GESTURE_DEBUG_MODE)
u8 g_gesture_wakeup_packet[GESTURE_DEBUG_MODE_PACKET_LENGTH] = { 0 };
#elif defined(CONFIG_ENABLE_GESTURE_INFORMATION_MODE)
u8 g_gesture_wakeup_packet[GESTURE_WAKEUP_INFORMATION_PACKET_LENGTH] = { 0 };
#else
u8 g_gesture_wakeup_packet[GESTURE_WAKEUP_PACKET_LENGTH] = { 0 };
/*for MSG22xx/MSG28xx : packet length = GESTURE_WAKEUP_PACKET_LENGTH */
#endif /*CONFIG_ENABLE_GESTURE_DEBUG_MODE */

#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
u8 g_gesture_debug_flag = 0x00;
u8 g_gesture_debug_mode = 0x00;
u8 g_log_gesture_debug[GESTURE_DEBUG_MODE_PACKET_LENGTH] = { 0 };

struct kset *g_gesture_k_set = NULL;
struct kobject *g_gesture_k_obj = NULL;
#endif /*CONFIG_ENABLE_GESTURE_DEBUG_MODE */

#ifdef CONFIG_ENABLE_GESTURE_INFORMATION_MODE
static u32 g_log_gesture_count = 0;
static u8 g_log_gesture_infor_type = 0;
u32 g_log_gesture_infor[GESTURE_WAKEUP_INFORMATION_PACKET_LENGTH] = { 0 };
#endif /*CONFIG_ENABLE_GESTURE_INFORMATION_MODE */

static u32 g_gesture_wakeup_value[2] = { 0 };

u8 g_gesture_wakeup_flag = 0;

#ifdef CONFIG_SUPPORT_64_TYPES_GESTURE_WAKEUP_MODE
/*support at most 64 types of gesture wakeup mode */
u32 g_gesture_wakeup_mode[2] = { 0xFFFFFFFF, 0xFFFFFFFF };
#else /*support at most 16 types of gesture wakeup mode */
u32 g_gesture_wakeup_mode[2] = { 0x0000FFFF, 0x00000000 };
#endif /*CONFIG_SUPPORT_64_TYPES_GESTURE_WAKEUP_MODE */
#endif /*CONFIG_ENABLE_GESTURE_WAKEUP */

#ifdef CONFIG_ENABLE_SEGMENT_READ_FINGER_TOUCH_DATA
/*for MSG28xx/MSG58xxA/ILI21xx */
static u8 g_touch_packet_flag[2] = { 0 };

u16 g_fw_packet_data_address = 0;
u16 g_fw_packet_flag_address = 0;
u8 g_fw_support_segment = 0;
#endif /*CONFIG_ENABLE_SEGMENT_READ_FINGER_TOUCH_DATA */

#ifdef CONFIG_ENABLE_CHARGER_DETECTION
static u8 g_charger_plugin = 0;
u8 g_force_update = 0;
int g_is_enable_charger_plugin_out_check = 1;
struct delayed_work g_charger_plug_in_out_check_work;
struct workqueue_struct *g_charger_plug_in_out_check_workqueue = NULL;
#endif /*CONFIG_ENABLE_CHARGER_DETECTION */

#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
static u8 g_curr_press[SELF_MAX_TOUCH_NUM] = { 0 }; /*for MSG22xx */
static u8 g_prior_press[SELF_MAX_TOUCH_NUM] = { 0 };    /*for MSG22xx */

static u8 g_prev_touch_status = 0;

static u8 g_previous_touch[MUTUAL_MAX_TOUCH_NUM] = { 0 };
/*for MSG28xx/MSG58xxA/ILI21xx */
static u8 g_current_touch[MUTUAL_MAX_TOUCH_NUM] = { 0 };
#endif /*CONFIG_ENABLE_TYPE_B_PROTOCOL */

static spinlock_t g_irq_lock;

static int g_interrupt_flag = 0;

static u16 g_debug_reg[MAX_DEBUG_REGISTER_NUM] = { 0 };
static u16 g_debug_reg_value[MAX_DEBUG_REGISTER_NUM] = { 0 };

static u32 g_debug_reg_count = 0;

static u8 g_debug_cmd_argu[MAX_DEBUG_COMMAND_ARGUMENT_NUM] = { 0 };

static u16 g_debug_cmd_argu_count = 0;
static u32 g_debug_read_data_size = 0;

static char g_debug_buf[1024] = { 0 };

u8 *g_platform_fw_version = NULL;  /*internal use firmware version for MStar */
u32 ga_platform_fw_version[3] = { 0 };

u8 g_fw_version_flag = 0;   /*0: old 1:new ,platform FW version V01.010.03 */
u8 *g_fw_version = NULL;        /*customer firmware version */

static u32 g_is_update_complete = 0;
static u32 g_feature_support_status = 0;

static u8 g_is_first_touch_key_pressed[MUTUAL_MAX_TOUCH_NUM] = { 0 };

static u8 g_ndebug_log_times_stamp = 0;
static u8 g_read_trim_data[3] = { 0 };

#define SYSFS_AUTHORITY (0644)
#define TS_DEV_MINOR_BASE 128
#define TS_DEV_MINOR_MAX 32
#define MS_INPUT_MAJOR 26
static struct class *g_firmware_class;
static struct device *g_firmware_cmd_dev;
u8 g_system_update = 0;
#define MSG_TP_IC_TYPE "msg20xx"
/*---------------------------------------------------------------------------*/

static struct proc_dir_entry *g_proc_class_entry = NULL;
static struct proc_dir_entry *g_proc_ms_touchscreen_msg20xx_entry = NULL;
static struct proc_dir_entry *g_proc_device_entry = NULL;
static struct proc_dir_entry *g_proc_chip_type_entry = NULL;
static struct proc_dir_entry *g_proc_firmware_data_entry = NULL;
static struct proc_dir_entry *g_proc_apk_firmware_update_entry = NULL;
static struct proc_dir_entry *g_proc_customer_firmware_version_entry = NULL;
static struct proc_dir_entry *g_proc_platform_firmware_version_entry = NULL;
static struct proc_dir_entry *g_proc_device_driver_version_entry = NULL;
static struct proc_dir_entry *g_proc_sd_card_firmware_update_entry = NULL;
static struct proc_dir_entry *g_proc_firmware_debug_entry = NULL;
static struct proc_dir_entry *g_proc_firmware_set_debug_value_entry = NULL;
static struct proc_dir_entry *g_proc_firmware_sm_bus_debug_entry = NULL;
static struct proc_dir_entry *g_proc_firmware_set_dq_mem_value_entry = NULL;
#ifdef CONFIG_ENABLE_ITO_MP_TEST
static struct proc_dir_entry *g_proc_mp_test_entry = NULL;
static struct proc_dir_entry *g_proc_mp_test_log_entry = NULL;
static struct proc_dir_entry *g_proc_mp_test_fail_channel_entry = NULL;
static struct proc_dir_entry *g_proc_mp_test_scope_entry = NULL;
#ifdef CONFIG_ENABLE_CHIP_TYPE_MSG28XX
static struct proc_dir_entry *g_proc_mp_test_log_all_entry = NULL;
#endif /*CONFIG_ENABLE_CHIP_TYPE_MSG28XX */
#endif /*CONFIG_ENABLE_ITO_MP_TEST */
static struct proc_dir_entry *g_proc_firmware_mode_entry = NULL;
static struct proc_dir_entry *g_proc_firmware_sensor_entry = NULL;
static struct proc_dir_entry *g_proc_firmware_packet_header_entry = NULL;
static struct proc_dir_entry *g_proc_query_feature_support_status_entry = NULL;
static struct proc_dir_entry *g_proc_change_feature_support_status_entry = NULL;
#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
static struct proc_dir_entry *g_proc_gesture_wakeup_mode_entry = NULL;
#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
static struct proc_dir_entry *g_proc_gesture_debug_mode_entry = NULL;
#endif /*CONFIG_ENABLE_GESTURE_DEBUG_MODE */
#ifdef CONFIG_ENABLE_GESTURE_INFORMATION_MODE
static struct proc_dir_entry *g_proc_gesture_infor_mode_entry = NULL;
#endif /*CONFIG_ENABLE_GESTURE_INFORMATION_MODE */
#endif /*CONFIG_ENABLE_GESTURE_WAKEUP */
#ifdef CONFIG_ENABLE_COUNT_REPORT_RATE
static struct proc_dir_entry *g_proc_report_rate_entry = NULL;
#endif /*CONFIG_ENABLE_COUNT_REPORT_RATE */
static struct proc_dir_entry *g_proc_glove_mode_entry = NULL;
static struct proc_dir_entry *g_proc_open_glove_mode_entry = NULL;
static struct proc_dir_entry *g_proc_close_glove_mode_entry = NULL;
static struct proc_dir_entry *g_proc_leather_sheath_mode_entry = NULL;
static struct proc_dir_entry *g_proc_film_mode_entry = NULL;
#ifdef CONFIG_ENABLE_JNI_INTERFACE
static struct proc_dir_entry *g_proc_jni_method_entry = NULL;
#endif /*CONFIG_ENABLE_JNI_INTERFACE */
static struct proc_dir_entry *g_proc_selinux_limit_firmware_update_entry = NULL;
static struct proc_dir_entry *g_proc_force_firmware_update_entry = NULL;
#ifdef CONFIG_ENABLE_ITO_MP_TEST
static struct proc_dir_entry *g_proc_mp_test_customised_entry = NULL;
#endif
static struct proc_dir_entry *g_proc_trim_code_entry = NULL;

static ssize_t drv_procfs_chip_type_read(struct file *pfile,
                                         char __user *pbuffer, size_t n_count,
                                         loff_t *ppos);
static ssize_t drv_procfs_chip_type_write(struct file *pfile,
                                          const char __user *pbuffer,
                                          size_t n_count, loff_t *ppos);
static ssize_t drv_procfs_firmware_data_read(struct file *pfile,
                                             char __user *pbuffer,
                                             size_t n_count, loff_t *ppos);
static ssize_t drv_procfs_firmware_data_write(struct file *pfile,
                                              const char __user *pbuffer,
                                              size_t n_count, loff_t *ppos);
static ssize_t drv_procfs_firmware_update_read(struct file *pfile,
                                               char __user *pbuffer,
                                               size_t n_count, loff_t *ppos);
static ssize_t drv_procfs_firmware_update_write(struct file *pfile,
                                                const char __user *pbuffer,
                                                size_t n_count, loff_t *ppos);
static ssize_t drv_procfs_customer_firmware_version_read(struct file *pfile,
                                                         char __user *pbuffer,
                                                         size_t n_count,
                                                         loff_t *ppos);
static ssize_t drv_procfs_customer_firmware_version_write(struct file *pfile,
                                                          const char __user *
                                                          pbuffer,
                                                          size_t n_count,
                                                          loff_t *ppos);
static ssize_t drv_procfs_platform_firmware_version_read(struct file *pfile,
                                                         char __user *pbuffer,
                                                         size_t n_count,
                                                         loff_t *ppos);
static ssize_t drv_procfs_platform_firmware_version_write(struct file *pfile,
                                                          const char __user *
                                                          pbuffer,
                                                          size_t n_count,
                                                          loff_t *ppos);
static ssize_t drv_procfs_device_driver_version_read(struct file *pfile,
                                                     char __user *pbuffer,
                                                     size_t n_count,
                                                     loff_t *ppos);
static ssize_t drv_procfs_device_driver_version_write(struct file *pfile,
                                                      const char __user *
                                                      pbuffer, size_t n_count,
                                                      loff_t *ppos);
static ssize_t drv_procfs_sd_card_firmware_update_read(struct file *pfile,
                                                       char __user *pbuffer,
                                                       size_t n_count,
                                                       loff_t *ppos);
static ssize_t drv_procfs_sd_card_firmware_update_write(struct file *pfile,
                                                        const char __user *
                                                        pbuffer, size_t n_count,
                                                        loff_t *ppos);
static ssize_t drv_procfs_firmware_debug_read(struct file *pfile,
                                              char __user *pbuffer,
                                              size_t n_count, loff_t *ppos);
static ssize_t drv_procfs_firmware_debug_write(struct file *pfile,
                                               const char __user *pbuffer,
                                               size_t n_count, loff_t *ppos);
static ssize_t drv_procfs_firmware_set_debug_value_read(struct file *pfile,
                                                        char __user *pbuffer,
                                                        size_t n_count,
                                                        loff_t *ppos);
static ssize_t drv_procfs_firmware_set_debug_value_write(struct file *pfile,
                                                         const char __user *
                                                         pbuffer, size_t n_count,
                                                         loff_t *ppos);
static ssize_t drv_procfs_firmware_sm_bus_debug_read(struct file *pfile,
                                                     char __user *pbuffer,
                                                     size_t n_count,
                                                     loff_t *ppos);
static ssize_t drv_procfs_firmware_sm_bus_debug_write(struct file *pfile,
                                                      const char __user *
                                                      pbuffer, size_t n_count,
                                                      loff_t *ppos);

static ssize_t drv_procfs_firmware_set_dq_mem_value_read(struct file *pfile,
                                                         char __user *pbuffer,
                                                         size_t n_count,
                                                         loff_t *ppos);
static ssize_t drv_procfs_firmware_set_dq_mem_value_write(struct file *pfile,
                                                          const char __user *
                                                          pbuffer,
                                                          size_t n_count,
                                                          loff_t *ppos);

#ifdef CONFIG_ENABLE_ITO_MP_TEST
static ssize_t drv_procfs_mp_test_read(struct file *pfile,
                                       char __user *pbuffer, size_t n_count,
                                       loff_t *ppos);
static ssize_t drv_procfs_mp_test_write(struct file *pfile,
                                        const char __user *pbuffer,
                                        size_t n_count, loff_t *ppos);
static ssize_t drv_procfs_mp_test_log_read(struct file *pfile,
                                           char __user *pbuffer, size_t n_count,
                                           loff_t *ppos);
static ssize_t drv_procfs_mp_test_log_write(struct file *pfile,
                                            const char __user *pbuffer,
                                            size_t n_count, loff_t *ppos);
static ssize_t drv_procfs_mp_test_fail_channel_read(struct file *pfile,
                                                    char __user *pbuffer,
                                                    size_t n_count,
                                                    loff_t *ppos);
static ssize_t drv_procfs_mp_test_fail_channel_write(struct file *pfile,
                                                     const char __user *
                                                     pbuffer, size_t n_count,
                                                     loff_t *ppos);
static ssize_t drv_procfs_mp_test_scope_read(struct file *pfile,
                                             char __user *pbuffer,
                                             size_t n_count, loff_t *ppos);
static ssize_t drv_procfs_mp_test_scope_write(struct file *pfile,
                                              const char __user *pbuffer,
                                              size_t n_count, loff_t *ppos);
#ifdef CONFIG_ENABLE_CHIP_TYPE_MSG28XX
static ssize_t drv_procfs_mp_test_log_all_read(struct file *pfile,
                                               char __user *pbuffer,
                                               size_t n_count, loff_t *ppos);
static ssize_t drv_procfs_mp_test_log_all_write(struct file *pfile,
                                                const char __user *pbuffer,
                                                size_t n_count, loff_t *ppos);
#endif /*CONFIG_ENABLE_CHIP_TYPE_MSG28XX */
#endif /*CONFIG_ENABLE_ITO_MP_TEST */

static ssize_t drv_procfs_firmware_mode_read(struct file *pfile,
                                             char __user *pbuffer,
                                             size_t n_count, loff_t *ppos);
static ssize_t drv_procfs_firmware_mode_write(struct file *pfile,
                                              const char __user *pbuffer,
                                              size_t n_count, loff_t *ppos);
static ssize_t drv_procfs_firmware_sensor_read(struct file *pfile,
                                               char __user *pbuffer,
                                               size_t n_count, loff_t *ppos);
static ssize_t drv_procfs_firmware_sensor_write(struct file *pfile,
                                                const char __user *pbuffer,
                                                size_t n_count, loff_t *ppos);
static ssize_t drv_procfs_firmware_packet_header_read(struct file *pfile,
                                                      char __user *pbuffer,
                                                      size_t n_count,
                                                      loff_t *ppos);
static ssize_t drv_procfs_firmware_packet_header_write(struct file *pfile,
                                                       const char __user *
                                                       pbuffer, size_t n_count,
                                                       loff_t *ppos);
static ssize_t drv_kobject_packet_show(struct kobject *pkobj,
                                       struct kobj_attribute *pattr,
                                       char *p_buf);
static ssize_t drv_kobject_packet_store(struct kobject *pkobj,
                                        struct kobj_attribute *pattr,
                                        const char *p_buf, size_t n_count);
static ssize_t drv_kobject_packet_store(struct kobject *pkobj,
                                        struct kobj_attribute *pattr,
                                        const char *p_buf, size_t n_count);

static ssize_t drv_procfs_query_feature_support_status_read(struct file *pfile,
                                                            char __user *
                                                            pbuffer,
                                                            size_t n_count,
                                                            loff_t *ppos);
static ssize_t drv_procfs_query_feature_support_status_write(struct file *pfile,
                                                             const char __user *
                                                             pbuffer,
                                                             size_t n_count,
                                                             loff_t *ppos);
static ssize_t drv_procfs_change_feature_support_status_read(struct file *pfile,
                                                             char __user *
                                                             pbuffer,
                                                             size_t n_count,
                                                             loff_t *ppos);
static ssize_t drv_procfs_change_feature_support_status_write(struct file
                                                              *pfile,
                                                              const char __user
                                                              *pbuffer,
                                                              size_t n_count,
                                                              loff_t *ppos);

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
static ssize_t drv_procfs_gesture_wakeup_mode_read(struct file *pfile,
                                                   char __user *pbuffer,
                                                   size_t n_count,
                                                   loff_t *ppos);
static ssize_t drv_procfs_gesture_wakeup_mode_write(struct file *pfile,
                                                    const char __user *pbuffer,
                                                    size_t n_count,
                                                    loff_t *ppos);

#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
static ssize_t drv_procfs_gesture_debug_mode_read(struct file *pfile,
                                                  char __user *pbuffer,
                                                  size_t n_count, loff_t *ppos);
static ssize_t drv_procfs_gesture_debug_mode_write(struct file *pfile,
                                                   const char __user *pbuffer,
                                                   size_t n_count,
                                                   loff_t *ppos);
static ssize_t drv_kobject_gesture_debug_show(struct kobject *pkobj,
                                              struct kobj_attribute *pattr,
                                              char *p_buf);
static ssize_t drv_kobject_gesture_debug_store(struct kobject *pkobj,
                                               struct kobj_attribute *pattr,
                                               const char *p_buf, size_t n_count);
#endif /*CONFIG_ENABLE_GESTURE_DEBUG_MODE */

#ifdef CONFIG_ENABLE_GESTURE_INFORMATION_MODE
static ssize_t drv_procfs_gesture_infor_mode_read(struct file *pfile,
                                                  char __user *pbuffer,
                                                  size_t n_count, loff_t *ppos);
static ssize_t drv_procfs_gesture_infor_mode_write(struct file *pfile,
                                                   const char __user *pbuffer,
                                                   size_t n_count,
                                                   loff_t *ppos);
#endif /*CONFIG_ENABLE_GESTURE_INFORMATION_MODE */
#endif /*CONFIG_ENABLE_GESTURE_WAKEUP */

#ifdef CONFIG_ENABLE_COUNT_REPORT_RATE
static ssize_t drv_procfs_report_rate_read(struct file *pfile,
                                        char __user *pbuffer, size_t n_count,
                                        loff_t *ppos);
static ssize_t drv_procfs_report_rate_write(struct file *pfile,
                                         const char __user *pbuffer,
                                         size_t n_count, loff_t *ppos);
#endif /*CONFIG_ENABLE_COUNT_REPORT_RATE */

static ssize_t drv_procfs_glove_mode_read(struct file *pfile,
                                       char __user *pbuffer, size_t n_count,
                                       loff_t *ppos);
static ssize_t drv_procfs_glove_mode_write(struct file *pfile,
                                        const char __user *pbuffer,
                                        size_t n_count, loff_t *ppos);
static ssize_t drv_procfs_open_glove_mode_read(struct file *pfile,
                                           char __user *pbuffer, size_t n_count,
                                           loff_t *ppos);
static ssize_t drv_procfs_close_glove_mode_read(struct file *pfile,
                                            char __user *pbuffer,
                                            size_t n_count, loff_t *ppos);

static ssize_t drv_procfs_leather_sheath_mode_read(struct file *pfile,
                                               char __user *pbuffer,
                                               size_t n_count, loff_t *ppos);
static ssize_t drv_procfs_leather_sheath_mode_write(struct file *pfile,
                                                const char __user *pbuffer,
                                                size_t n_count, loff_t *ppos);

#ifdef CONFIG_ENABLE_JNI_INTERFACE
static ssize_t drv_jni_msg_tool_read(struct file *pfile, char __user *pbuffer,
                                  size_t n_count, loff_t *ppos);
static ssize_t drv_jni_msg_tool_write(struct file *pfile,
                                   const char __user *pbuffer, size_t n_count,
                                   loff_t *ppos);
static long drv_jni_msg_tool_Ioctl(struct file *pfile, unsigned int ncmd,
                                unsigned long narg);
#endif /*CONFIG_ENABLE_JNI_INTERFACE */

static ssize_t drv_procfs_selinux_limit_firmware_update_read(struct file *pfile,
                                                        char __user *pbuffer,
                                                        size_t n_count,
                                                        loff_t *ppos);
static ssize_t drv_procfs_force_firmware_update_read(struct file *pfile,
                                                 char __user *pbuffer,
                                                 size_t n_count, loff_t *ppos);

#ifdef CONFIG_ENABLE_ITO_MP_TEST
static ssize_t drv_main_procfs_mp_test_customised_write(struct file *pfile,
                                                  const char __user *pbuffer,
                                                  size_t n_count, loff_t *ppos);
static ssize_t drv_main_procfs_mp_test_customised_read(struct file *pfile,
                                                 char __user *pbuffer,
                                                 size_t n_count, loff_t *ppos);
static const struct file_operations g_proc_mp_test_customised = {
    .write = drv_main_procfs_mp_test_customised_write,
    .read = drv_main_procfs_mp_test_customised_read,
};
#endif
static ssize_t drv_procfs_trim_code_write(struct file *pfile,
                                       const char __user *pbuffer,
                                       size_t n_count, loff_t *ppos);
static ssize_t drv_procfs_trim_code_read(struct file *pfile, char __user *pbuffer,
                                      size_t n_count, loff_t *ppos);

static ssize_t drv_procfs_get_film_mode_read(struct file *pfile,
                                         char __user *pbuffer, size_t n_count,
                                         loff_t *ppos);
static ssize_t drv_procfs_set_film_mode_write(struct file *pfile,
                                          const char __user *pbuffer,
                                          size_t n_count, loff_t *ppos);

static const struct file_operations g_proc_chip_type = {
    .read = drv_procfs_chip_type_read,
    .write = drv_procfs_chip_type_write,
};

static const struct file_operations g_proc_firmware_data = {
    .read = drv_procfs_firmware_data_read,
    .write = drv_procfs_firmware_data_write,
};

static const struct file_operations g_proc_apk_firmware_update = {
    .read = drv_procfs_firmware_update_read,
    .write = drv_procfs_firmware_update_write,
};

static const struct file_operations g_proc_customer_firmware_version = {
    .read = drv_procfs_customer_firmware_version_read,
    .write = drv_procfs_customer_firmware_version_write,
};

static const struct file_operations g_proc_platform_firmware_version = {
    .read = drv_procfs_platform_firmware_version_read,
    .write = drv_procfs_platform_firmware_version_write,
};

static const struct file_operations g_proc_device_driver_version = {
    .read = drv_procfs_device_driver_version_read,
    .write = drv_procfs_device_driver_version_write,
};

static const struct file_operations g_proc_sd_card_firmware_update = {
    .read = drv_procfs_sd_card_firmware_update_read,
    .write = drv_procfs_sd_card_firmware_update_write,
};

static const struct file_operations g_proc_firmware_debug = {
    .read = drv_procfs_firmware_debug_read,
    .write = drv_procfs_firmware_debug_write,
};

static const struct file_operations g_proc_firmware_set_debug_value = {
    .read = drv_procfs_firmware_set_debug_value_read,
    .write = drv_procfs_firmware_set_debug_value_write,
};

static const struct file_operations g_proc_firmware_sm_bus_debug = {
    .read = drv_procfs_firmware_sm_bus_debug_read,
    .write = drv_procfs_firmware_sm_bus_debug_write,
};

static const struct file_operations g_proc_firmware_set_dq_mem_value = {
    .read = drv_procfs_firmware_set_dq_mem_value_read,
    .write = drv_procfs_firmware_set_dq_mem_value_write,
};

#ifdef CONFIG_ENABLE_ITO_MP_TEST
static const struct file_operations g_proc_mp_test = {
    .read = drv_procfs_mp_test_read,
    .write = drv_procfs_mp_test_write,
};

static const struct file_operations g_proc_mp_test_log = {
    .read = drv_procfs_mp_test_log_read,
    .write = drv_procfs_mp_test_log_write,
};

static const struct file_operations g_proc_mp_test_fail_channel = {
    .read = drv_procfs_mp_test_fail_channel_read,
    .write = drv_procfs_mp_test_fail_channel_write,
};

static const struct file_operations g_proc_mp_test_scope = {
    .read = drv_procfs_mp_test_scope_read,
    .write = drv_procfs_mp_test_scope_write,
};

#ifdef CONFIG_ENABLE_CHIP_TYPE_MSG28XX
static const struct file_operations g_proc_mp_test_log_all = {
    .read = drv_procfs_mp_test_log_all_read,
    .write = drv_procfs_mp_test_log_all_write,
};
#endif /*CONFIG_ENABLE_CHIP_TYPE_MSG28XX */
#endif /*CONFIG_ENABLE_ITO_MP_TEST */

static const struct file_operations g_proc_firmware_mode = {
    .read = drv_procfs_firmware_mode_read,
    .write = drv_procfs_firmware_mode_write,
};

static const struct file_operations g_proc_firmware_sensor = {
    .read = drv_procfs_firmware_sensor_read,
    .write = drv_procfs_firmware_sensor_write,
};

static const struct file_operations g_proc_firmware_packet_header = {
    .read = drv_procfs_firmware_packet_header_read,
    .write = drv_procfs_firmware_packet_header_write,
};

static const struct file_operations g_proc_query_feature_support_status = {
    .read = drv_procfs_query_feature_support_status_read,
    .write = drv_procfs_query_feature_support_status_write,
};

static const struct file_operations g_proc_change_feature_support_status = {
    .read = drv_procfs_change_feature_support_status_read,
    .write = drv_procfs_change_feature_support_status_write,
};

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
static const struct file_operations g_proc_gesture_wake_up_mode = {
    .read = drv_procfs_gesture_wakeup_mode_read,
    .write = drv_procfs_gesture_wakeup_mode_write,
};

#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
static const struct file_operations g_proc_gesture_debug_mode = {
    .read = drv_procfs_gesture_debug_mode_read,
    .write = drv_procfs_gesture_debug_mode_write,
};
#endif /*CONFIG_ENABLE_GESTURE_DEBUG_MODE */
#ifdef CONFIG_ENABLE_GESTURE_INFORMATION_MODE
static const struct file_operations g_proc_gesture_infor_mode = {

    .read = drv_procfs_gesture_infor_mode_read,
    .write = drv_procfs_gesture_infor_mode_write,
};
#endif /*CONFIG_ENABLE_GESTURE_INFORMATION_MODE */
#endif /*CONFIG_ENABLE_GESTURE_WAKEUP */

#ifdef CONFIG_ENABLE_COUNT_REPORT_RATE
static const struct file_operations g_proc_report_rate = {
    .read = drv_procfs_report_rate_read,
    .write = drv_procfs_report_rate_write,
};
#endif /*CONFIG_ENABLE_COUNT_REPORT_RATE */

static const struct file_operations g_proc_glove_mode = {
    .read = drv_procfs_glove_mode_read,
    .write = drv_procfs_glove_mode_write,
};

static const struct file_operations g_proc_open_glove_mode = {
    .read = drv_procfs_open_glove_mode_read,
};

static const struct file_operations g_proc_close_glove_mode = {
    .read = drv_procfs_close_glove_mode_read,
};

static const struct file_operations g_proc_leather_sheath_mode = {
    .read = drv_procfs_leather_sheath_mode_read,
    .write = drv_procfs_leather_sheath_mode_write,
};

static const struct file_operations g_proc_film_mode = {
    .read = drv_procfs_get_film_mode_read,
    .write = drv_procfs_set_film_mode_write,
};

#ifdef CONFIG_ENABLE_JNI_INTERFACE
static const struct file_operations g_proc_jni_method = {
    .read = drv_jni_msg_tool_read,
    .write = drv_jni_msg_tool_write,
    .unlocked_ioctl = drv_jni_msg_tool_Ioctl,
    .compat_ioctl = drv_jni_msg_tool_Ioctl,
};
#endif /*CONFIG_ENABLE_JNI_INTERFACE */

static const struct file_operations g_proc_selinux_limit_firmware_update = {
    .read = drv_procfs_selinux_limit_firmware_update_read,
};

static const struct file_operations g_proc_force_firmware_update = {
    .read = drv_procfs_force_firmware_update_read,
};

static const struct file_operations g_proc_trim_code = {
    .read = drv_procfs_trim_code_read,
    .write = drv_procfs_trim_code_write,
};

/*--------------------------------------------------------------------------*/

u32 slave_i2c_id_db_bus = (0xB2>>1);
u32 slave_i2c_id_dw_i2c = (0x4C >> 1);

u16 firmware_mode_unknown_mode = 0xFFFF;
u16 firmware_mode_demo_mode = 0xFFFF;
u16 firmware_mode_debug_mode = 0xFFFF;
u16 firmware_mode_raw_data_mode = 0xFFFF;

u16 demo_mode_packet_length = 0;
/*
 *If project use MSG28xx, set MUTUAL_DEMO_MODE_PACKET_LENGTH as default.
 *If project use MSG22xx, set SELF_DEMO_MODE_PACKET_LENGTH as default.
 */
u16 debug_mode_packet_length = 0;
/*
 *If project use MSG28xx, set MUTUAL_DEBUG_MODE_PACKET_LENGTH as default.
 * If project use MSG22xx, set SELF_DEBUG_MODE_PACKET_LENGTH as default.
 */
u16 max_touch_num = 0;
/*
 *If project use MSG28xx, set MUTUAL_MAX_TOUCH_NUM as default.
 *If project use MSG22xx, set SELF_MAX_TOUCH_NUM as default.
 */

struct kset *g_touch_k_set = NULL;
struct kobject *g_touch_k_obj = NULL;
u8 g_is_switch_mode_by_apk = 0;

u8 is_gesture_wakeup_enabled = 0;
u8 is_gesture_debug_mode_enabled = 0;
u8 is_gesture_information_mode_enabled = 0;
u8 is_gesture_wakeup_mode_support_64_types_enabled = 0;

u8 touch_driver_debug_log_level = CONFIG_TOUCH_DRIVER_DEBUG_LOG_LEVEL;
u8 is_firmware_data_log_enabled = CONFIG_ENABLE_FIRMWARE_DATA_LOG;
u8 is_apk_print_firmware_specific_log_enabled =
    CONFIG_ENABLE_APK_PRINT_FIRMWARE_SPECIFIC_LOG;
u8 is_self_freq_scan_enabled = 0;
u8 is_force_to_update_firmware_enabled = 0;
u8 is_disable_esd_protection_check = 0;

#ifdef CONFIG_TP_HAVE_KEY
int g_tp_virtual_key[] = {TOUCH_KEY_MENU, TOUCH_KEY_HOME,
                                     TOUCH_KEY_BACK, TOUCH_KEY_SEARCH };

#ifdef CONFIG_ENABLE_REPORT_KEY_WITH_COORDINATE
#if defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM)
struct kobject *g_properties_kobj = NULL;
#elif defined(CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM)
#define BUTTON_W (100)
#define BUTTON_H (100)

int g_tp_virtual_key_dim_local[MAX_KEY_NUM][4] = {{(TOUCH_SCREEN_X_MAX / 4) / 2*1,
TOUCH_SCREEN_Y_MAX + BUTTON_H / 2,
BUTTON_W, BUTTON_H},
{(TOUCH_SCREEN_X_MAX / 4) / 2 * 3, TOUCH_SCREEN_Y_MAX + BUTTON_H / 2, BUTTON_W,
 BUTTON_H},
{(TOUCH_SCREEN_X_MAX / 4) / 2 * 5, TOUCH_SCREEN_Y_MAX + BUTTON_H / 2, BUTTON_W,
 BUTTON_H},
{(TOUCH_SCREEN_X_MAX / 4) / 2 * 7, TOUCH_SCREEN_Y_MAX + BUTTON_H / 2, BUTTON_W,
 BUTTON_H}
};
#endif
#endif /*CONFIG_ENABLE_REPORT_KEY_WITH_COORDINATE */
#endif /*CONFIG_TP_HAVE_KEY */

#ifdef CONFIG_ENABLE_ESD_PROTECTION
int g_is_enable_esd_check = 1;
struct delayed_work g_esd_check_work;
struct workqueue_struct *g_esd_check_work_queue = NULL;
u8 g_is_hw_reset_by_driver = 0;
void drv_esd_check(struct work_struct *p_work);
#endif /*CONFIG_ENABLE_ESD_PROTECTION */

struct input_dev *g_input_device = NULL;
struct mutex g_mutex;
struct mutex g_mutex_protect;

#ifdef CONFIG_ENABLE_COUNT_REPORT_RATE
u32 g_is_enable_report_rate = 0;
u32 g_interrupt_count = 0;
u32 g_valid_touch_count = 0;
u32 g_interrupt_report_rate = 0;
u32 g_valid_touch_report_rate = 0;
struct timeval g_start_time;
#endif /*CONFIG_ENABLE_COUNT_REPORT_RATE */

u8 g_is_enable_glove_mode = 0;
u8 g_is_enable_leather_sheath_mode = 0;

u8 g_fw_data[MAX_UPDATE_FIRMWARE_BUFFER_SIZE][1024];
u32 g_fw_data_count = 0;

u8 g_is_hotknot_enabled = 0;
u8 g_is_bypass_hotknot = 0;

u16 g_chip_type = 0;
u16 g_original_chip_type = 0;

/*
 *u8 g_demo_mode_packet[MUTUAL_DEMO_MODE_PACKET_LENGTH] = {0};
 *for MSG22xx : demo_mode_packet_length = SELF_DEMO_MODE_PACKET_LENGTH,
 *for MSG28xx/MSG58xxA:demo_mode_packet_length=MUTUAL_DEMO_MODE_PACKET_LENGTH,
 *for ILI21xx : demo_mode_packet_length = ILI21XX_DEMO_MODE_PACKET_LENGTH
*/
u8 g_demo_mode_packet[ILI21XX_DEMO_MODE_PACKET_LENGTH] = {0};
/*
 *for MSG22xx : demo_mode_packet_length = SELF_DEMO_MODE_PACKET_LENGTH,
 * for MSG28xx/MSG58xxA:demo_mode_packet_length=MUTUAL_DEMO_MODE_PACKET_LENGTH,
 * for ILI21xx : demo_mode_packet_length = ILI21XX_DEMO_MODE_PACKET_LENGTH
 */

struct mutual_firmware_info_t g_mutual_firmware_info;
struct self_firmware_info_t g_self_firmware_info;
struct ilitek_tp_info_t g_ilitek_tp_info;

u8 g_ilitek_fw_data[ILITEK_MAX_UPDATE_FIRMWARE_BUFFER_SIZE * 1024];
u8 g_ilitek_fw_data_buf[ILITEK_ILI21XX_FIRMWARE_SIZE * 1024] = { 0 };
/*for update firmware(ILI21xx) from SD card */

u8 g_debug_mode_packet[MUTUAL_DEBUG_MODE_PACKET_LENGTH] = { 0 };
/*
 *for MSG22xx : debug_mode_packet_length = SELF_DEBUG_MODE_PACKET_LENGTH,
 *for MSG28xx/MSG58xxA/ILI21xx : debug_mode_packet_length =
 * MUTUAL_DEBUG_MODE_PACKET_LENGTH
*/
u8 g_log_mode_packet[MUTUAL_DEBUG_MODE_PACKET_LENGTH] = { 0 };
/*
 *for MSG22xx : debug_mode_packet_length = SELF_DEBUG_MODE_PACKET_LENGTH,
 *for MSG28xx/MSG58xxA/ILI21xx : debug_mode_packet_length =
 * MUTUAL_DEBUG_MODE_PACKET_LENGTH
 */

u16 g_firmware_mode;

u8 g_is_update_firmware = 0x00;
u8 g_msg22xx_chip_revision = 0x00;
u8 g_is_disable_finger_touch = 0;

u8 g_is_suspend = 0x00;
bool g_gesture_state = false;

/*=============================================================*/

/*FUNCTION DECLARATION*/

/*=============================================================*/

static s32 drv_create_procfs_dir_entry(void);
static void drv_remove_procfs_dir_entry(void);
static s32 drv_create_system_dir_entry(void);
static void drv_remove_system_dir_entry(void);
static s32 drv_self_parse_packet(u8 *p_packet, u16 n_length,
                               struct self_touch_info_t *p_info);
static s32 drv_mutual_parse_packet(u8 *p_packet, u16 n_length,
                                struct mutual_touch_info_t *p_info);
void drv_touch_device_power_on(void);
void drv_touch_device_power_off(void);
void drv_touch_device_hw_reset(void);

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
void drv_open_gesture_debug_mode(u8 n_gesture_flag);
void drv_close_gesture_debug_mode(void);
#endif /*CONFIG_ENABLE_GESTURE_DEBUG_MODE */
#ifdef CONFIG_ENABLE_GESTURE_INFORMATION_MODE
static void drv_convert_gesture_information_mode_coordinate(u8 *p_raw_data,
                                                        u32 *p_tran_x,
                                                        u32 *p_tran_y);
#endif /*CONFIG_ENABLE_GESTURE_INFORMATION_MODE */
#endif /*CONFIG_ENABLE_GESTURE_WAKEUP */

#ifdef CONFIG_ENABLE_PROXIMITY_DETECTION
s32 drv_enable_proximity(void);
s32 drv_disable_proximity(void);
#if defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM)
void drv_tp_ps_enable(int n_enable);
#elif defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM)
int drv_tp_ps_enable(struct sensors_classdev *p_proximity_cdev,
                  unsigned int n_enable);
#endif
#endif /*CONFIG_ENABLE_PROXIMITY_DETECTION */

/*=============================================================*/

/*GLOBAL FUNCTION DEFINITION*/

/*=============================================================*/

#ifdef CONFIG_ENABLE_DMA_IIC
#include <linux/dma-mapping.h>
#include <linux/mm_types.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/page.h>
#include <linux/vmalloc.h>

static unsigned char *i2c_dma_buf_va = NULL;
static dma_addr_t i2c_dma_buf_pa = 0;

void dma_alloc(void)
{
    if (i2c_dma_buf_va == NULL) {
        if (g_input_device != NULL) {
            g_input_device->dev.coherent_dma_mask = DMA_BIT_MASK(32);
            i2c_dma_buf_va =
                (u8 *) dma_alloc_coherent(&g_input_device->dev,
                                          MAX_I2C_TRANSACTION_LENGTH_LIMIT,
                                          &i2c_dma_buf_pa, GFP_KERNEL);
        }
    }

    if (i2c_dma_buf_va == NULL)
        DBG(&g_i2c_client->dev, "dma_alloc FAILED!\n");
     else
        DBG(&g_i2c_client->dev, "dma_alloc SUCCESS!\n");
}

void dma_reset(void)
{
    DBG(&g_i2c_client->dev, "Dma memory reset!\n");

    memset(i2c_dma_buf_va, 0, MAX_I2C_TRANSACTION_LENGTH_LIMIT);
}

void dma_free(void)
{
    if (i2c_dma_buf_va != NULL) {
        if (g_input_device != NULL) {
            dma_free_coherent(&g_input_device->dev,
                              MAX_I2C_TRANSACTION_LENGTH_LIMIT, i2c_dma_buf_va,
                              i2c_dma_buf_pa);
        }
        i2c_dma_buf_va = NULL;
        i2c_dma_buf_pa = 0;

        DBG(&g_i2c_client->dev, "dma_free SUCCESS!\n");
    }
}
#endif /*CONFIG_ENABLE_DMA_IIC */

/*---------------------------------------------------------------------------*/
 s32 in_write(u32 n_addr)
{
    s32 rc = 0, n_data = 0;
    char sz_out_buf[64] = { 0 };

/*DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);*/

    sz_out_buf[0] = 0x25;
    sz_out_buf[1] = (char)((n_addr & 0x000000FF) >> 0);
    sz_out_buf[2] = (char)((n_addr & 0x0000FF00) >> 8);
    sz_out_buf[3] = (char)((n_addr & 0x00FF0000) >> 16);

    rc = iic_write_data(slave_i2c_id_dw_i2c, sz_out_buf, 4);

/*udelay(10);*/
    rc = iic_read_data(slave_i2c_id_dw_i2c, sz_out_buf, 4);

    n_data =
        (sz_out_buf[0] + sz_out_buf[1] * 256 + sz_out_buf[2] * 256 * 256 +
         sz_out_buf[3] * 256 * 256 * 256);

    return n_data;
}

s32 out_write(u32 n_addr, u32 n_data, s32 n_size)
{
    s32 rc = 0, i = 0;
    char sz_out_buf[64] = { 0 };

    sz_out_buf[0] = 0x25;
    sz_out_buf[1] = (char)((n_addr & 0x000000FF) >> 0);
    sz_out_buf[2] = (char)((n_addr & 0x0000FF00) >> 8);
    sz_out_buf[3] = (char)((n_addr & 0x00FF0000) >> 16);

    for (i = 0; i < n_size; i++)
        sz_out_buf[i + 4] = (char)(n_data >> (8 * i));

    rc = iic_write_data(slave_i2c_id_dw_i2c, sz_out_buf, n_size + 4);

    return rc;
}

s32 in_write_one_byte(u32 n_addr)
{
    s32 rc = 0, n_data = 0;
    char sz_out_buf[64] = { 0 };

    sz_out_buf[0] = 0x25;
    sz_out_buf[1] = (char)((n_addr & 0x000000FF) >> 0);
    sz_out_buf[2] = (char)((n_addr & 0x0000FF00) >> 8);
    sz_out_buf[3] = (char)((n_addr & 0x00FF0000) >> 16);
    rc = iic_write_data(slave_i2c_id_dw_i2c, sz_out_buf, 4);
    rc = iic_read_data(slave_i2c_id_dw_i2c, sz_out_buf, 1);
    n_data = (sz_out_buf[0]);

    return n_data;
}

s32 vfice_reg_read(s32 n_in_addr)
{
    s32 i = 0;
    s32 n_in_time_count = 100;
    u8 sz_buf[4] = { 0 };

    out_write(0x41000, 0x3B | (n_in_addr << 8), 4);
    out_write(0x041004, 0x66AA5500, 4);
    /*Check Flag */
    /*Check Busy Flag */
    for (i = 0; i < n_in_time_count; i++) {
        sz_buf[0] = in_write_one_byte(0x41011);

        if ((sz_buf[0] & 0x01) == 0)
            break;

        mdelay(5);
    }

    return in_write_one_byte(0x41012);
}

s32 exit_ice_mode(void)
{
    s32 rc = 0;

#if defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM)
    if (ms_ts_msg_ic_gpio_rst > 0)
#elif defined(CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM)
#ifdef CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD
    if (ms_ts_msg_ic_gpio_rst >= 0 && ms_ts_msg_ic_gpio_rst != 1)
        /*ms_ts_msg_ic_gpio_rst must be a value other than 1 */
#else
    if (ms_ts_msg_ic_gpio_rst > 0)
#endif /*CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD */
#endif
    {
        drv_touch_device_hw_reset();
    } else {
        DBG(&g_i2c_client->dev, "ICE mode reset\n");

        rc = out_write(0x04004C, 0x2120, 2);
        if (rc < 0) {
            DBG(&g_i2c_client->dev,
                "out_write(0x04004C, 0x2120, 2) error, rc = %d\n", rc);
        }
        mdelay(10);

        rc = out_write(0x04004E, 0x01, 1);
        if (rc < 0) {
            DBG(&g_i2c_client->dev,
                "out_write(0x04004E, 0x01, 1) error, rc = %d\n", rc);
        }
    }

    mdelay(50);

    return 0;
}

/*-------------------------------------------------------------------------*/
    u16 reg_get16_bit_value(u16 n_addr)
{
    u8 sz_tx_data[3] = { 0x10, (n_addr >> 8) & 0xFF, n_addr & 0xFF };
    u8 sz_rx_data[2] = { 0 };

    iic_write_data(slave_i2c_id_db_bus, &sz_tx_data[0], 3);
    iic_read_data(slave_i2c_id_db_bus, &sz_rx_data[0], 2);

    return (sz_rx_data[1] << 8 | sz_rx_data[0]);
}

u8 reg_get_l_byte_value(u16 n_addr)
{
    u8 sz_tx_data[3] = { 0x10, (n_addr >> 8) & 0xFF, n_addr & 0xFF };
    u8 sz_rx_data = { 0 };

    iic_write_data(slave_i2c_id_db_bus, &sz_tx_data[0], 3);
    iic_read_data(slave_i2c_id_db_bus, &sz_rx_data, 1);

    return sz_rx_data;
}

u8 reg_get_h_byte_value(u16 n_addr)
{
    u8 sz_tx_data[3] = { 0x10, (n_addr >> 8) & 0xFF, (n_addr & 0xFF) + 1 };
    u8 sz_rx_data = { 0 };

    iic_write_data(slave_i2c_id_db_bus, &sz_tx_data[0], 3);
    iic_read_data(slave_i2c_id_db_bus, &sz_rx_data, 1);

    return sz_rx_data;
}

void reg_get_x_bit_value(u16 n_addr, u8 *p_rx_data, u16 n_length,
                     u16 n_max_i2c_length_limit)
{
    u16 n_read_addr = n_addr;
    u16 n_read_size = 0;
    u16 n_left = n_length;
    u16 n_offset = 0;
    u8 sz_tx_data[3] = { 0 };

    sz_tx_data[0] = 0x10;

    mutex_lock(&g_mutex);

#ifdef CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM
#ifdef CONFIG_ENABLE_DMA_IIC
    dma_reset();
#endif /*CONFIG_ENABLE_DMA_IIC */
#endif /*CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM */

    while (n_left > 0) {
        if (n_left >= n_max_i2c_length_limit) {
            n_read_size = n_max_i2c_length_limit;

            sz_tx_data[1] = (n_read_addr >> 8) & 0xFF;
            sz_tx_data[2] = n_read_addr & 0xFF;

            iic_write_data(slave_i2c_id_db_bus, &sz_tx_data[0], 3);
            iic_read_data(slave_i2c_id_db_bus, &p_rx_data[n_offset], n_read_size);

            n_read_addr = n_read_addr + n_read_size;  /*set next read address */
            n_left = n_left - n_read_size;
            n_offset = n_offset + n_read_size;

        } else {
            n_read_size = n_left;

            sz_tx_data[1] = (n_read_addr >> 8) & 0xFF;
            sz_tx_data[2] = n_read_addr & 0xFF;

            iic_write_data(slave_i2c_id_db_bus, &sz_tx_data[0], 3);
            iic_read_data(slave_i2c_id_db_bus, &p_rx_data[n_offset], n_read_size);

            n_left = 0;
            n_offset = n_offset + n_read_size;
        }
    }

    mutex_unlock(&g_mutex);
}

s32 reg_set_16bit_value(u16 n_addr, u16 n_data)
{
    s32 rc = 0;
    u8 sz_tx_data[5] = { 0x10, (n_addr >> 8) & 0xFF,
                                  n_addr & 0xFF, n_data & 0xFF, n_data >> 8 };

    rc = iic_write_data(slave_i2c_id_db_bus, &sz_tx_data[0], 5);

    return rc;
}

s32 retry_reg_set_16bit_value(u16 n_addr, u16 n_data)
{
    s32 rc = 0;
    u32 n_retry_count = 0;

    while (n_retry_count < 5) {
        mdelay(5);
        rc = reg_set_16bit_value(n_addr, n_data);
        if (rc > 0) {
            DBG(&g_i2c_client->dev,
                "reg_set_16bit_value(0x%x, 0x%x) success, rc = %d\n", n_addr,
                                                                    n_data, rc);
            break;
        }

        n_retry_count++;
    }
    if (n_retry_count == 5) {
        DBG(&g_i2c_client->dev,
            "reg_set_16bit_value(0x%x, 0x%x) failed, rc = %d\n", n_addr,
                                                                    n_data, rc);
    }

    return rc;
}

void reg_set_l_byte_value(u16 n_addr, u8 n_data)
{
    u8 sz_tx_data[4] = { 0x10, (n_addr >> 8) & 0xFF, n_addr & 0xFF, n_data };

    iic_write_data(slave_i2c_id_db_bus, &sz_tx_data[0], 4);
}

void reg_set_h_byte_value(u16 n_addr, u8 n_data)
{
    u8 sz_tx_data[4] = { 0x10, (n_addr >> 8) & 0xFF, (n_addr & 0xFF) + 1, n_data };

    iic_write_data(slave_i2c_id_db_bus, &sz_tx_data[0], 4);
}

void reg_set_16bit_value_on(u16 n_addr, u16 n_data)
{                               /*Set bit on n_data from 0 to 1 */
    u16 r_data = reg_get16_bit_value(n_addr);

    r_data |= n_data;
    reg_set_16bit_value(n_addr, r_data);
}

void reg_set_16Bit_value_off(u16 n_addr, u16 n_data)
{                               /*Set bit on n_data from 1 to 0 */
    u16 r_data = reg_get16_bit_value(n_addr);

    r_data &= (~n_data);
    reg_set_16bit_value(n_addr, r_data);
}

u16 reg_get_16bit_value_by_address_mode(u16 n_addr, enum address_mode_e e_address_mode)
{
    u16 n_data = 0;

    if (e_address_mode == ADDRESS_MODE_16BIT)
        n_addr = n_addr - (n_addr & 0xFF) + ((n_addr & 0xFF) << 1);

    n_data = reg_get16_bit_value(n_addr);

    return n_data;
}

void reg_set_16bit_value_by_address_mode(u16 n_addr, u16 n_data,
                                   enum address_mode_e e_address_mode)
{
    if (e_address_mode == ADDRESS_MODE_16BIT)
        n_addr = n_addr - (n_addr & 0xFF) + ((n_addr & 0xFF) << 1);

    reg_set_16bit_value(n_addr, n_data);
}

void reg_mask_16bit_value(u16 n_addr, u16 n_mask, u16 n_data,
                       enum address_mode_e e_address_mode)
{
    u16 n_tmp_data = 0;

    if (n_data > n_mask)
        return;

    n_tmp_data = reg_get_16bit_value_by_address_mode(n_addr, e_address_mode);
    n_tmp_data = (n_tmp_data & (~n_mask));
    n_tmp_data = (n_tmp_data | n_data);
    reg_set_16bit_value_by_address_mode(n_addr, n_tmp_data, e_address_mode);
}

s32 db_bus_enter_serial_debug_mode(void)
{
    s32 rc = 0;
    u8 data[5];

    /*Enter the Serial Debug Mode */
    data[0] = 0x53;
    data[1] = 0x45;
    data[2] = 0x52;
    data[3] = 0x44;
    data[4] = 0x42;

    rc = iic_write_data(slave_i2c_id_db_bus, data, 5);

    return rc;
}

void db_bus_exit_serial_debug_mode(void)
{
    u8 data[1];

    /*Exit the Serial Debug Mode */
    data[0] = 0x45;

    iic_write_data(slave_i2c_id_db_bus, data, 1);
}

void db_bus_iic_use_bus(void)
{
    u8 data[1];

    /*IIC Use Bus */
    data[0] = 0x35;

    iic_write_data(slave_i2c_id_db_bus, data, 1);
}

void db_bus_iic_not_use_bus(void)
{
    u8 data[1];

    /*IIC Not Use Bus */
    data[0] = 0x34;

    iic_write_data(slave_i2c_id_db_bus, data, 1);
}

void db_bus_iic_reshape(void)
{
    u8 data[1];

    /*IIC Re-shape */
    data[0] = 0x71;

    iic_write_data(slave_i2c_id_db_bus, data, 1);
}

void db_bus_stop_mcu(void)
{
    u8 data[1];

    /*Stop the MCU */
    data[0] = 0x37;

    iic_write_data(slave_i2c_id_db_bus, data, 1);
}

void db_bus_not_stop_mcu(void)
{
    u8 data[1];

    /*Not Stop the MCU */
    data[0] = 0x36;

    iic_write_data(slave_i2c_id_db_bus, data, 1);
}

void db_bus_reset_slave(void)
{
    u8 data[1];

    /*IIC Reset Slave */
    data[0] = 0x00;

    iic_write_data(slave_i2c_id_db_bus, data, 1);
}

void db_bus_wait_mcu(void)
{
    u8 data[1];

    /*Stop the MCU */
    data[0] = 0x37;
    iic_write_data(slave_i2c_id_db_bus, data, 1);

    data[0] = 0x61;
    iic_write_data(slave_i2c_id_db_bus, data, 1);
}

void set_cfb(u8 cfb)
{
    /*Setting cfb */
    switch (cfb) {
    case _50p:                 /*cfb = 50p */
        reg_mask_16bit_value(0x1528, (u16) 0x0070, (u16) 0x0000,
                          ADDRESS_MODE_16BIT);
        reg_mask_16bit_value(0x1523, (u16) 0x0700, (u16) 0x0000,
                          ADDRESS_MODE_16BIT);
        /*0x1523[10:8] = 0x0, Rfb: 180kohm */
        break;

    case _40p:                 /*cfb = 40p */
        reg_mask_16bit_value(0x1528, (u16) 0x0070, (u16) 0x0020,
                          ADDRESS_MODE_16BIT);
        reg_mask_16bit_value(0x1523, (u16) 0x0700, (u16) 0x0100,
                          ADDRESS_MODE_16BIT);
        /*0x1523[10:8] = 0x1, Rfb: 225kohm */
        break;

    case _30p:                 /*cfb = 30p */
        reg_mask_16bit_value(0x1528, (u16) 0x0070, (u16) 0x0040,
                          ADDRESS_MODE_16BIT);
        reg_mask_16bit_value(0x1523, (u16) 0x0700, (u16) 0x0200,
                          ADDRESS_MODE_16BIT);
        /*0x1523[10:8] = 0x2, Rfb: 300kohm */
        break;

    case _20p:                 /*cfb = 20p */
        reg_mask_16bit_value(0x1528, (u16) 0x0070, (u16) 0x0060,
                          ADDRESS_MODE_16BIT);
        reg_mask_16bit_value(0x1523, (u16) 0x0700, (u16) 0x0200,
                          ADDRESS_MODE_16BIT);
        /*0x1523[10:8] = 0x2, Rfb: 300kohm */
        break;

    case _10p:                 /*cfb = 10p */
        reg_mask_16bit_value(0x1528, (u16) 0x0070, (u16) 0x0070,
                          ADDRESS_MODE_16BIT);
        reg_mask_16bit_value(0x1523, (u16) 0x0700, (u16) 0x0200,
                          ADDRESS_MODE_16BIT);
        /*0x1523[10:8] = 0x2, Rfb: 300kohm */
        break;

    default:
        break;
    }
}

s32 iic_write_data(u8 n_slave_id, u8 *p_buf, u16 n_size)
{
    s32 rc = 0;

#if defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM)
    struct i2c_msg msgs[] = {
        {
         .addr = n_slave_id,
         .flags = 0,
         /*if read flag is undefined, then it means write flag. */
         .len = n_size,
         .buf = p_buf,
         },
    };
#ifdef ROCKCHIP_PLATFORM
    msgs[0].scl_rate = 400000;
#endif /*ROCKCHIP_PLATFORM */
    /* If everything went ok (i.e. 1 msg transmitted), return #bytes
     * transmitted, else error code.
     */
    if (g_i2c_client != NULL) {
        if ((g_chip_type == CHIP_TYPE_MSG28XX ||
            g_chip_type == CHIP_TYPE_MSG58XXA ||
            g_chip_type == CHIP_TYPE_ILI2118A ||
            g_chip_type == CHIP_TYPE_ILI2117A) &&
            n_slave_id == slave_i2c_id_dw_i2c &&
            g_is_update_firmware != 0) {
            PRINTF_ERR
                ("Not allow to execute SmBus command while update firmware.\n");
        } else {
            rc = i2c_transfer(g_i2c_client->adapter, msgs, 1);

            /*no return error if command is for serialDebug mode */
            if (n_size == 5) {
                if (p_buf[0] == 0x53 && p_buf[1] == 0x45 && p_buf[2] == 0x52 &&
                    p_buf[3] == 0x44 && p_buf[4] == 0x42) {
                    rc = n_size;
                    goto out;
                }
            }

            if (n_size == 1) {
                if (p_buf[0] == 0x45) {
                    rc = n_size;
                    goto out;
                }
            }
            if (rc == 1) {
                rc = n_size;
            } else {            /*rc < 0 */

                PRINTF_ERR("iic_write_data() error %d\n", rc);
            }
        }
    } else {
        PRINTF_ERR("i2c client is NULL\n");
    }
#elif defined(CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM)
    if (g_i2c_client != NULL) {
        if ((g_chip_type == CHIP_TYPE_MSG28XX ||
            g_chip_type == CHIP_TYPE_MSG58XXA ||
            g_chip_type == CHIP_TYPE_ILI2118A ||
            g_chip_type == CHIP_TYPE_ILI2117A) &&
            n_slave_id == slave_i2c_id_dw_i2c &&
            g_is_update_firmware != 0) {
            PRINTF_ERR
                ("Not allow to execute SmBus command while update firmware.\n");
        } else {
            u8 n_addr_before = g_i2c_client->addr;

            g_i2c_client->addr = n_slave_id;

#ifdef CONFIG_ENABLE_DMA_IIC
            if (n_size > 8 && NULL != i2c_dma_buf_va) {
                s32 i = 0;

                for (i = 0; i < n_size; i++)
                    i2c_dma_buf_va[i] = p_buf[i];

                g_i2c_client->ext_flag = g_i2c_client->ext_flag | I2C_DMA_FLAG;
                rc = i2c_master_send(g_i2c_client,
                                     (unsigned char *)i2c_dma_buf_pa, n_size);
            } else {
                g_i2c_client->ext_flag =
                    g_i2c_client->ext_flag & (~I2C_DMA_FLAG);
                rc = i2c_master_send(g_i2c_client, p_buf, n_size);
            }
#else
            rc = i2c_master_send(g_i2c_client, p_buf, n_size);
#endif /*CONFIG_ENABLE_DMA_IIC */
            g_i2c_client->addr = n_addr_before;

            /*no return error if command is for serialDebug mode */
            if (n_size == 5) {
                if (p_buf[0] == 0x53 && p_buf[1] == 0x45 && p_buf[2] == 0x52 &&
                    p_buf[3] == 0x44 && p_buf[4] == 0x42) {
                    rc = n_size;
                    goto out;
                }
            }

            if (n_size == 1) {
                if (p_buf[0] == 0x45) {
                    rc = n_size;
                    goto out;
                }
            }
            if (rc < 0) {
                PRINTF_ERR("iic_write_data() error %d, n_slave_id=%d, n_size=%d\n",
                           rc, n_slave_id, n_size);
            }
        }
    } else {
        PRINTF_ERR("i2c client is NULL\n");
    }
#endif

out:
    return rc;
}

s32 iic_read_data(u8 n_slave_id, u8 *p_buf, u16 n_size)
{
    s32 rc = 0;

#if defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM)
    struct i2c_msg msgs[] = {
        {
         .addr = n_slave_id,
         .flags = I2C_M_RD,     /*read flag */
         .len = n_size,
         .buf = p_buf,
         },
    };
#ifdef ROCKCHIP_PLATFORM
    msgs[0].scl_rate = 400000;
#endif /*ROCKCHIP_PLATFORM */
    /* If everything went ok (i.e. 1 msg transmitted), return #bytes
     * transmitted, else error code.
     */
    if (g_i2c_client != NULL) {
        if ((g_chip_type == CHIP_TYPE_MSG28XX || g_chip_type == CHIP_TYPE_MSG58XXA
             || g_chip_type == CHIP_TYPE_ILI2118A ||
             g_chip_type == CHIP_TYPE_ILI2117A)
            && n_slave_id == slave_i2c_id_dw_i2c && g_is_update_firmware != 0) {
            PRINTF_ERR
                ("Not allow to execute SmBus command while update firmware.\n");
        } else {
            rc = i2c_transfer(g_i2c_client->adapter, msgs, 1);

            if (rc == 1) {
                rc = n_size;
            } else {            /*rc < 0 */

                PRINTF_ERR("iic_read_data() error %d\n", rc);
            }
        }
    } else {
        PRINTF_ERR("i2c client is NULL\n");
    }
#elif defined(CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM)
    if (g_i2c_client != NULL) {
        if ((g_chip_type == CHIP_TYPE_MSG28XX || g_chip_type == CHIP_TYPE_MSG58XXA
             || g_chip_type == CHIP_TYPE_ILI2118A ||
             g_chip_type == CHIP_TYPE_ILI2117A)
            && n_slave_id == slave_i2c_id_dw_i2c && g_is_update_firmware != 0) {
            PRINTF_ERR
                ("Not allow to execute SmBus command while update firmware.\n");
        } else {
            u8 n_addr_before = g_i2c_client->addr;

            g_i2c_client->addr = n_slave_id;

#ifdef CONFIG_ENABLE_DMA_IIC
            if (n_size > 8 && NULL != i2c_dma_buf_va) {
                s32 i = 0;

                g_i2c_client->ext_flag = g_i2c_client->ext_flag | I2C_DMA_FLAG;
                rc = i2c_master_recv(g_i2c_client,
                                     (unsigned char *)i2c_dma_buf_pa, n_size);

                for (i = 0; i < n_size; i++)
                    p_buf[i] = i2c_dma_buf_va[i];
            } else {
                g_i2c_client->ext_flag =
                    g_i2c_client->ext_flag & (~I2C_DMA_FLAG);
                rc = i2c_master_recv(g_i2c_client, p_buf, n_size);
            }
#else
            rc = i2c_master_recv(g_i2c_client, p_buf, n_size);
#endif /*CONFIG_ENABLE_DMA_IIC */
            g_i2c_client->addr = n_addr_before;

            if (rc < 0) {
                PRINTF_ERR("iic_read_data() error %d, n_slave_id=%d, n_size=%d\n",
                           rc, n_slave_id, n_size);
            }
        }
    } else {
        PRINTF_ERR("i2c client is NULL\n");
    }
#endif

    return rc;
}

s32 iic_segment_read_data_by_db_bus(u8 n_reg_bank, u8 n_reg_addr, u8 *p_buf, u16 n_size,
                              u16 n_max_i2c_length_limit)
{
    s32 rc = 0;
    u16 n_left = n_size;
    u16 n_offset = 0;
    u16 n_segment_length = 0;
    u16 n_read_size = 0;
    u16 n_over = 0;
    u8 sz_write_buf[3] = { 0 };
    u8 n_next_reg_bank = n_reg_bank;
    u8 n_next_reg_addr = n_reg_addr;

#if defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM)
    struct i2c_msg msgs[2] = {
        {
         .addr = slave_i2c_id_db_bus,
         .flags = 0,            /*write flag */
         .len = 3,
         .buf = sz_write_buf,
         },
        {
         .addr = slave_i2c_id_db_bus,
         .flags = I2C_M_RD,     /*read flag */
         },
    };
#ifdef ROCKCHIP_PLATFORM
    msgs[0].scl_rate = 400000;
    msgs[1].scl_rate = 400000;
#endif /*ROCKCHIP_PLATFORM */
    /*If everything went ok (i.e. 1 msg transmitted),
    *return #bytes transmitted, else error code.
    */
    if (g_i2c_client != NULL) {
        if (n_max_i2c_length_limit >= 256)
            n_segment_length = 256;
        else
            n_segment_length = 128;

        PRINTF_ERR("n_segment_length = %d\n", n_segment_length); /*add for debug */

        while (n_left > 0) {
            sz_write_buf[0] = 0x10;
            n_reg_bank = n_next_reg_bank;
            sz_write_buf[1] = n_reg_bank;
            n_reg_addr = n_next_reg_addr;
            sz_write_buf[2] = n_reg_addr;

            PRINTF_ERR("n_reg_bank = 0x%x\n", n_reg_bank);  /*add for debug */
            PRINTF_ERR("n_reg_addr = 0x%x\n", n_reg_addr);  /*add for debug */

            msgs[1].buf = &p_buf[n_offset];

            if (n_left > n_segment_length) {
                if ((n_reg_addr + n_segment_length) <
                    MAX_TOUCH_IC_REGISTER_BANK_SIZE) {
                    n_next_reg_addr = n_reg_addr + n_segment_length;

                    PRINTF_ERR("n_next_reg_addr = 0x%x\n", n_next_reg_addr);
                    /*add for debug */

                    msgs[1].len = n_segment_length;
                    n_left -= n_segment_length;
                    n_offset += msgs[1].len;
                } else if ((n_reg_addr + n_segment_length) ==
                           MAX_TOUCH_IC_REGISTER_BANK_SIZE) {
                    n_next_reg_addr = 0x00;
                    n_next_reg_bank = n_reg_bank + 1;
                    /*shift to read data from next register bank */

                    PRINTF_ERR("n_next_reg_bank = 0x%x\n", n_next_reg_bank);
                    /*add for debug */

                    msgs[1].len = n_segment_length;
                    n_left -= n_segment_length;
                    n_offset += msgs[1].len;
                } else {
                    n_next_reg_addr = 0x00;
                    n_next_reg_bank = n_reg_bank + 1;
                    /*shift to read data from next register bank */

                    PRINTF_INFO("n_next_reg_bank = 0x%x\n", n_next_reg_bank);
                    /*add for debug */

                    n_over =
                        (n_reg_addr + n_segment_length) -
                        MAX_TOUCH_IC_REGISTER_BANK_SIZE;

                    PRINTF_ERR("n_over = 0x%x\n", n_over);    /*add for debug */

                    msgs[1].len = n_segment_length - n_over;
                    n_left -= msgs[1].len;
                    n_offset += msgs[1].len;
                }
            } else {
                if ((n_reg_addr + n_left) < MAX_TOUCH_IC_REGISTER_BANK_SIZE) {
                    n_next_reg_addr = n_reg_addr + n_left;

                    PRINTF_ERR("n_next_reg_addr = 0x%x\n", n_next_reg_addr);
                    /*add for debug */

                    msgs[1].len = n_left;
                    n_left = 0;

/*n_offset += msgs[1].len;*/
                } else if ((n_reg_addr + n_left) ==
                           MAX_TOUCH_IC_REGISTER_BANK_SIZE) {
                    n_next_reg_addr = 0x00;
                    n_next_reg_bank = n_reg_bank + 1;
                    /*shift to read data from next register bank */

                    PRINTF_ERR("n_next_reg_bank = 0x%x\n", n_next_reg_bank);
                    /*add for debug */

                    msgs[1].len = n_left;
                    n_left = 0;

/*n_offset += msgs[1].len;*/
                } else {
                /*((n_reg_addr + n_left) > MAX_TOUCH_IC_REGISTER_BANK_SIZE) */

                    n_next_reg_addr = 0x00;
                    n_next_reg_bank = n_reg_bank + 1;
                    /*shift to read data from next register bank */

                    PRINTF_ERR("n_next_reg_bank = 0x%x\n", n_next_reg_bank);
                    /*add for debug */

                    n_over =
                        (n_reg_addr + n_left) - MAX_TOUCH_IC_REGISTER_BANK_SIZE;

                    PRINTF_ERR("n_over = 0x%x\n", n_over);    /*add for debug */

                    msgs[1].len = n_left - n_over;
                    n_left -= msgs[1].len;
                    n_offset += msgs[1].len;
                }
            }

            rc = i2c_transfer(g_i2c_client->adapter, &msgs[0], 2);
            if (rc == 2)
                n_read_size = n_read_size + msgs[1].len;
             else {            /*rc < 0 */

                PRINTF_ERR
                    ("iic_segment_read_data_by_db_bus() -> i2c_transfer() error %d\n",
                     rc);

                return rc;
            }
        }
    } else {
        PRINTF_ERR("i2c client is NULL\n");
    }
#elif defined(CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM)
    if (g_i2c_client != NULL) {
#ifndef CONFIG_ENABLE_DMA_IIC
        u8 *p_readBuf = NULL;
#endif /*CONFIG_ENABLE_DMA_IIC */
        u16 n_length = 0;
        u8 n_addr_before = g_i2c_client->addr;

        g_i2c_client->addr = slave_i2c_id_db_bus;

        if (n_max_i2c_length_limit >= 256)
            n_segment_length = 256;
        else
            n_segment_length = 128;

        PRINTF_ERR("n_segment_length = %d\n", n_segment_length); /*add for debug */

#ifdef CONFIG_ENABLE_DMA_IIC
        if (i2c_dma_buf_va != NULL) {
            s32 i = 0;

            while (n_left > 0) {
                sz_write_buf[0] = 0x10;
                n_reg_bank = n_next_reg_bank;
                sz_write_buf[1] = n_reg_bank;
                n_reg_addr = n_next_reg_addr;
                sz_write_buf[2] = n_reg_addr;

                PRINTF_ERR("n_reg_bank = 0x%x\n", n_reg_bank);  /*add for debug */
                PRINTF_ERR("n_reg_addr = 0x%x\n", n_reg_addr);  /*add for debug */

                if (n_left > n_segment_length) {
                    if ((n_reg_addr + n_segment_length) <
                        MAX_TOUCH_IC_REGISTER_BANK_SIZE) {
                        n_next_reg_addr = n_reg_addr + n_segment_length;

                        PRINTF_ERR("n_next_reg_addr = 0x%x\n", n_next_reg_addr);
                        /*add for debug */

                        n_length = n_segment_length;
                        n_left -= n_segment_length;
                    } else if ((n_reg_addr + n_segment_length) ==
                               MAX_TOUCH_IC_REGISTER_BANK_SIZE) {
                        n_next_reg_addr = 0x00;
                        n_next_reg_bank = n_reg_bank + 1;
                        /*shift to read data from next register bank */

                        PRINTF_ERR("n_next_reg_bank = 0x%x\n", n_next_reg_bank);
                        /*add for debug */

                        n_length = n_segment_length;
                        n_left -= n_segment_length;
                    } else {

                        n_next_reg_addr = 0x00;
                        n_next_reg_bank = n_reg_bank + 1;
                        /*shift to read data from next register bank */

                        PRINTF_INFO("n_next_reg_bank = 0x%x\n", n_next_reg_bank);
                        /*add for debug */

                        n_over =
                            (n_reg_addr + n_segment_length) -
                            MAX_TOUCH_IC_REGISTER_BANK_SIZE;

                        PRINTF_ERR("n_over = 0x%x\n", n_over);
                        /*add for debug */

                        n_length = n_segment_length - n_over;
                        n_left -= n_length;
                    }
                } else {
                    if ((n_reg_addr + n_left) < MAX_TOUCH_IC_REGISTER_BANK_SIZE) {
                        n_next_reg_addr = n_reg_addr + n_left;

                        PRINTF_ERR("n_next_reg_addr = 0x%x\n", n_next_reg_addr);
                        /*add for debug */

                        n_length = n_left;
                        n_left = 0;
                    } else if ((n_reg_addr + n_left) ==
                               MAX_TOUCH_IC_REGISTER_BANK_SIZE) {
                        n_next_reg_addr = 0x00;
                        n_next_reg_bank = n_reg_bank + 1;
                        /*shift to read data from next register bank */

                        PRINTF_ERR("n_next_reg_bank = 0x%x\n", n_next_reg_bank);
                        /*add for debug */

                        n_length = n_left;
                        n_left = 0;
                    } else {

                        n_next_reg_addr = 0x00;
                        n_next_reg_bank = n_reg_bank + 1;
                        /*shift to read data from next register bank */

                        PRINTF_ERR("n_next_reg_bank = 0x%x\n", n_next_reg_bank);
                        /*add for debug */

                        n_over =
                            (n_reg_addr + n_left) -
                            MAX_TOUCH_IC_REGISTER_BANK_SIZE;

                        PRINTF_ERR("n_over = 0x%x\n", n_over);/*add for debug */

                        n_length = n_left - n_over;
                        n_left -= n_length;
                    }
                }

                g_i2c_client->ext_flag =
                    g_i2c_client->ext_flag & (~I2C_DMA_FLAG);
                rc = i2c_master_send(g_i2c_client, &sz_write_buf[0], 3);
                if (rc < 0) {
                    PRINTF_ERR
                    ("iic_segment_read_data_by_db_bus()->i2c_master_send()error%d\n",
                         rc);

                    return rc;
                }

                g_i2c_client->ext_flag = g_i2c_client->ext_flag | I2C_DMA_FLAG;
                rc = i2c_master_recv(g_i2c_client,
                                     (unsigned char *)i2c_dma_buf_pa, n_length);
                if (rc < 0) {
                    PRINTF_ERR
                    ("iic_segment_read_data_by_db_bus()->i2c_master_recv()error%d\n",
                         rc);

                    return rc;
                } else {
                    for (i = 0; i < n_length; i++)
                        p_buf[i + n_offset] = i2c_dma_buf_va[i];

                    n_offset += n_length;

                    n_read_size = n_read_size + n_length;
                }
            }
        } else {
            PRINTF_ERR("iic_segment_read_data_by_db_bus()->i2c_dma_buf_va is NULL\n");
        }
#else
        while (n_left > 0) {
            sz_write_buf[0] = 0x10;
            n_reg_bank = n_next_reg_bank;
            sz_write_buf[1] = n_reg_bank;
            n_reg_addr = n_next_reg_addr;
            sz_write_buf[2] = n_reg_addr;

            PRINTF_ERR("n_reg_bank = 0x%x\n", n_reg_bank);  /*add for debug */
            PRINTF_ERR("n_reg_addr = 0x%x\n", n_reg_addr);  /*add for debug */

            p_readBuf = &p_buf[n_offset];

            if (n_left > n_segment_length) {
                if ((n_reg_addr + n_segment_length) <
                    MAX_TOUCH_IC_REGISTER_BANK_SIZE) {
                    n_next_reg_addr = n_reg_addr + n_segment_length;

                    PRINTF_ERR("n_next_reg_addr = 0x%x\n", n_next_reg_addr);
                    /*add for debug */

                    n_length = n_segment_length;
                    n_left -= n_segment_length;
                    n_offset += n_length;
                } else if ((n_reg_addr + n_segment_length) ==
                           MAX_TOUCH_IC_REGISTER_BANK_SIZE) {
                    n_next_reg_addr = 0x00;
                    n_next_reg_bank = n_reg_bank + 1;
                    /*shift to read data from next register bank */

                    PRINTF_ERR("n_next_reg_bank = 0x%x\n", n_next_reg_bank);
                    /*add for debug */

                    n_length = n_segment_length;
                    n_left -= n_segment_length;
                    n_offset += n_length;
                } else {
           /*((n_reg_addr + n_segment_length) > MAX_TOUCH_IC_REGISTER_BANK_SIZE) */

                    n_next_reg_addr = 0x00;
                    n_next_reg_bank = n_reg_bank + 1;
                    /*shift to read data from next register bank */

                    PRINTF_INFO("n_next_reg_bank = 0x%x\n", n_next_reg_bank);
                    /*add for debug */

                    n_over =
                        (n_reg_addr + n_segment_length) -
                        MAX_TOUCH_IC_REGISTER_BANK_SIZE;

                    PRINTF_ERR("n_over = 0x%x\n", n_over);    /*add for debug */

                    n_length = n_segment_length - n_over;
                    n_left -= n_length;
                    n_offset += n_length;
                }
            } else {
                if ((n_reg_addr + n_left) < MAX_TOUCH_IC_REGISTER_BANK_SIZE) {
                    n_next_reg_addr = n_reg_addr + n_left;

                    PRINTF_ERR("n_next_reg_addr = 0x%x\n", n_next_reg_addr);
                    /*add for debug */

                    n_length = n_left;
                    n_left = 0;

/*n_offset += n_length;*/
                } else if ((n_reg_addr + n_left) ==
                           MAX_TOUCH_IC_REGISTER_BANK_SIZE) {
                    n_next_reg_addr = 0x00;
                    n_next_reg_bank = n_reg_bank + 1;
                    /*shift to read data from next register bank */

                    PRINTF_ERR("n_next_reg_bank = 0x%x\n", n_next_reg_bank);
                    /*add for debug */

                    n_length = n_left;
                    n_left = 0;

/*n_offset += n_length;*/
                } else {
                /*((n_reg_addr + n_left) > MAX_TOUCH_IC_REGISTER_BANK_SIZE) */

                    n_next_reg_addr = 0x00;
                    n_next_reg_bank = n_reg_bank + 1;
                    /*shift to read data from next register bank */

                    PRINTF_ERR("n_next_reg_bank = 0x%x\n", n_next_reg_bank);
                    /*add for debug */

                    n_over =
                        (n_reg_addr + n_left) - MAX_TOUCH_IC_REGISTER_BANK_SIZE;

                    PRINTF_ERR("n_over = 0x%x\n", n_over);    /*add for debug */

                    n_length = n_left - n_over;
                    n_left -= n_length;
                    n_offset += n_length;
                }
            }

            rc = i2c_master_send(g_i2c_client, &sz_write_buf[0], 3);
            if (rc < 0) {
                PRINTF_ERR
                ("iic_segment_read_data_by_db_bus()->i2c_master_send() error %d\n",
                     rc);

                return rc;
            }

            rc = i2c_master_recv(g_i2c_client, p_readBuf, n_length);
            if (rc < 0) {
                PRINTF_ERR
                ("iic_segment_read_data_by_db_bus()->i2c_master_recv() error %d\n",
                     rc);

                return rc;
            } else {
                n_read_size = n_read_size + n_length;
            }
        }
#endif /*CONFIG_ENABLE_DMA_IIC */
        g_i2c_client->addr = n_addr_before;
    } else {
        PRINTF_ERR("i2c client is NULL\n");
    }
#endif

    return n_read_size;
}

s32 iic_segment_read_data_by_sm_bus(u16 n_addr, u8 *p_buf, u16 n_size,
                              u16 n_max_i2c_length_limit)
{
    s32 rc = 0;
    u16 n_left = n_size;
    u16 n_offset = 0;
    u16 n_read_size = 0;
    u8 sz_write_buf[3] = { 0 };

#if defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM)
    struct i2c_msg msgs[2] = {
        {
         .addr = slave_i2c_id_dw_i2c,
         .flags = 0,            /*write flag */
         .len = 3,
         .buf = sz_write_buf,
         },
        {
         .addr = slave_i2c_id_dw_i2c,
         .flags = I2C_M_RD,     /*read flag */
         },
    };
#ifdef ROCKCHIP_PLATFORM
    msgs[0].scl_rate = 400000;
    msgs[1].scl_rate = 400000;
#endif /*ROCKCHIP_PLATFORM */
/*If everything went ok (i.e. 1 msg transmitted),
 *return #bytes transmitted, else error code.
 */
    if (g_i2c_client != NULL) {
        while (n_left > 0) {
            sz_write_buf[0] = 0x53;
            sz_write_buf[1] = ((n_addr + n_offset) >> 8) & 0xFF;
            sz_write_buf[2] = (n_addr + n_offset) & 0xFF;

            msgs[1].buf = &p_buf[n_offset];

            if (n_left > n_max_i2c_length_limit) {
                msgs[1].len = n_max_i2c_length_limit;
                n_left -= n_max_i2c_length_limit;
                n_offset += msgs[1].len;
            } else {
                msgs[1].len = n_left;
                n_left = 0;

/*n_offset += msgs[1].len;*/
            }

#ifdef CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM
            mdelay(I2C_SMBUS_WRITE_COMMAND_DELAY_FOR_PLATFORM);
#endif /*CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM */

            rc = i2c_transfer(g_i2c_client->adapter, &msgs[0], 2);
            if (rc == 2) {
                n_read_size = n_read_size + msgs[1].len;
            } else {            /*rc < 0 */

                PRINTF_ERR
                ("iic_segment_read_data_by_sm_bus() -> i2c_transfer() error %d\n",
                rc);

                return rc;
            }
        }
    } else {
        PRINTF_ERR("i2c client is NULL\n");
    }
#elif defined(CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM)
    if (g_i2c_client != NULL) {
#ifndef CONFIG_ENABLE_DMA_IIC
        u8 *p_readBuf = NULL;
#endif /*CONFIG_ENABLE_DMA_IIC */
        u16 n_length = 0;
        u8 n_addr_before = g_i2c_client->addr;

        g_i2c_client->addr = slave_i2c_id_dw_i2c;

#ifdef CONFIG_ENABLE_DMA_IIC
        while (n_left > 0) {
            s32 i = 0;

            sz_write_buf[0] = 0x53;
            sz_write_buf[1] = ((n_addr + n_offset) >> 8) & 0xFF;
            sz_write_buf[2] = (n_addr + n_offset) & 0xFF;

            if (n_left > n_max_i2c_length_limit) {
                n_length = n_max_i2c_length_limit;
                n_left -= n_max_i2c_length_limit;
            } else {
                n_length = n_left;
                n_left = 0;
            }

            g_i2c_client->ext_flag = g_i2c_client->ext_flag & (~I2C_DMA_FLAG);
            rc = i2c_master_send(g_i2c_client, &sz_write_buf[0], 3);
            if (rc < 0) {
                PRINTF_ERR
                ("iic_segment_read_data_by_sm_bus() -> i2c_master_send() error %d\n",
                 rc);

                return rc;
            }

            g_i2c_client->ext_flag = g_i2c_client->ext_flag | I2C_DMA_FLAG;
            rc = i2c_master_recv(g_i2c_client, (unsigned char *)i2c_dma_buf_pa,
                                 n_length);
            if (rc < 0) {
                PRINTF_ERR
                ("iic_segment_read_data_by_sm_bus() -> i2c_master_recv() error %d\n",
                 rc);

                return rc;
            } else {
                for (i = 0; i < n_length; i++)
                    p_buf[i + n_offset] = i2c_dma_buf_va[i];

                n_offset += n_length;

                n_read_size = n_read_size + n_length;
            }
        }
#else
        while (n_left > 0) {
            sz_write_buf[0] = 0x53;
            sz_write_buf[1] = ((n_addr + n_offset) >> 8) & 0xFF;
            sz_write_buf[2] = (n_addr + n_offset) & 0xFF;

            p_readBuf = &p_buf[n_offset];

            if (n_left > n_max_i2c_length_limit) {
                n_length = n_max_i2c_length_limit;
                n_left -= n_max_i2c_length_limit;
                n_offset += n_length;
            } else {
                n_length = n_left;
                n_left = 0;

/*n_offset += n_length;*/
            }

            rc = i2c_master_send(g_i2c_client, &sz_write_buf[0], 3);
            if (rc < 0) {
                PRINTF_ERR
                ("iic_segment_read_data_by_sm_bus() -> i2c_master_send() error %d\n",
                 rc);

                return rc;
            }

            rc = i2c_master_recv(g_i2c_client, p_readBuf, n_length);
            if (rc < 0) {
                PRINTF_ERR
                ("iic_segment_read_data_by_sm_bus() -> i2c_master_recv() error %d\n",
                 rc);

                return rc;
            } else {
                n_read_size = n_read_size + n_length;
            }
        }
#endif /*CONFIG_ENABLE_DMA_IIC */
        g_i2c_client->addr = n_addr_before;
    } else {
        PRINTF_ERR("i2c client is NULL\n");
    }
#endif

    return n_read_size;
}

/*---------------------------------------------------------------------------*/
static u8 drv_calculate_check_sum(u8 *p_msg, u32 n_length)
{
    s32 n_check_sum = 0;
    u32 i;

    for (i = 0; i < n_length; i++)
        n_check_sum += p_msg[i];


    return (u8) ((-n_check_sum) & 0xFF);
}

static u32 drv_convert_char_to_hex_digit(char *pCh, u32 n_length)
{
    u32 n_ret_val = 0;
    u32 i;

    DBG(&g_i2c_client->dev, "n_length = %d\n", n_length);

    for (i = 0; i < n_length; i++) {
        char ch = *pCh++;
        u32 n = 0;
        u8 nIsValidDigit = 0;

        if ((i == 0 && ch == '0') || (i == 1 && ch == 'x'))
            continue;


        if ('0' <= ch && ch <= '9') {
            n = ch - '0';
            nIsValidDigit = 1;
        } else if ('a' <= ch && ch <= 'f') {
            n = 10 + ch - 'a';
            nIsValidDigit = 1;
        } else if ('A' <= ch && ch <= 'F') {
            n = 10 + ch - 'A';
            nIsValidDigit = 1;
        }

        if (nIsValidDigit == 1)
            n_ret_val = n + n_ret_val * 16;

    }

    return n_ret_val;
}

void drv_read_file(char *p_file_path, u8 *p_buf, u16 n_length)
{
    struct file *pfile = NULL;
    mm_segment_t old_fs;
    ssize_t n_read_bytes = 0;

    old_fs = get_fs();
    set_fs(get_ds());

    pfile = filp_open(p_file_path, O_RDONLY, 0);
    if (IS_ERR(pfile)) {
        DBG(&g_i2c_client->dev, "Open file failed: %s\n", p_file_path);
        return;
    }

    pfile->f_op->llseek(pfile, 0, SEEK_SET);
    n_read_bytes = pfile->f_op->read(pfile, p_buf, n_length, &pfile->f_pos);
    DBG(&g_i2c_client->dev, "Read %d bytes!\n", (int)n_read_bytes);

    set_fs(old_fs);
    filp_close(pfile, NULL);
}

/*---------------------------------------------------------------------------*/
#ifdef CONFIG_ENABLE_JNI_INTERFACE
u64 ptr_to_u64(u8 *p_value)
{
    uintptr_t n_value = (uintptr_t) p_value;

    return (u64) (0xFFFFFFFFFFFFFFFF & n_value);
}

u8 *u64_to_ptr(u64 n_value)
{
    uintptr_t p_value = (uintptr_t) n_value;

    return (u8 *) p_value;
}

static ssize_t drv_jni_msg_tool_read(struct file *pfile, char __user *pbuffer,
                                  size_t n_count, loff_t *ppos)
{
    long n_ret = 0;
    u8 n_bus_type = 0;
    u16 n_read_len = 0;
    u8 sz_cmd_data[20] = { 0 };

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);
    /*DBG(&g_i2c_client->dev, "*** n_count = %d ***\n", (int)n_count); */
    n_bus_type = n_count & 0xFF;
    n_read_len = (n_count >> 8) & 0xFFFF;
    if (n_bus_type == slave_i2c_id_db_bus || n_bus_type == slave_i2c_id_dw_i2c)
        iic_read_data(n_bus_type, &sz_cmd_data[0], n_read_len);

    n_ret = copy_to_user(pbuffer, &sz_cmd_data[0], n_read_len);
    return n_ret;
}

static ssize_t drv_jni_msg_tool_write(struct file *pfile,
                                   const char __user *pbuffer, size_t n_count,
                                   loff_t *ppos)
{
    long n_ret = 0;
    u8 n_bus_type = 0;
    u16 n_write_len = 0;
    u8 sz_cmd_data[20] = { 0 };
    u8 buffer[128] = { 0 };

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);
    DBG(&g_i2c_client->dev, "*** n_count = %d ***\n", (int)n_count);
    /*copy data from user space */
    n_bus_type = n_count & 0xFF;
    if (n_bus_type == slave_i2c_id_db_bus || n_bus_type == slave_i2c_id_dw_i2c) {
        n_write_len = (n_count >> 8) & 0xFFFF;
        n_ret = copy_from_user(sz_cmd_data, &pbuffer[0], n_write_len);
        iic_write_data(n_bus_type, &sz_cmd_data[0], n_write_len);
    } else {
        n_ret = copy_from_user(buffer, pbuffer, n_count - 1);
        if (n_ret < 0) {
            DBG(0, "%s, copy data from user space, failed", __func__);
            return -1;
        }
        if (strcmp(buffer, "erase_flash") == 0) {
            DBG(&g_i2c_client->dev, "start Erase Flash\n");
            drv_msg28xx_erase_emem(EMEM_MAIN);
            DBG(&g_i2c_client->dev, "end Erase Flash\n");
        }
    }
    return n_count;
}

static void drv_jni_reg_get_x_byte_data(struct msg_tool_drv_cmd_t *p_cmd)
{
    u16 n_addr = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    n_addr = (g_snd_cmd_data[1] << 8) | g_snd_cmd_data[0];
    reg_get_x_bit_value(n_addr, g_rtn_cmd_data, p_cmd->n_rtncmd_len,
                    MAX_I2C_TRANSACTION_LENGTH_LIMIT);

    /*_DebugJniShowArray(g_rtn_cmd_data, p_cmd->n_rtncmd_len);*/
}

static void drv_jni_clear_msg_tool_mem(void)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    memset(g_msg_tool_cmd_in, 0, sizeof(struct msg_tool_drv_cmd_t));
    memset(g_snd_cmd_data, 0, 1024);
    memset(g_rtn_cmd_data, 0, 1024);
}

static struct msg_tool_drv_cmd_t *drv_jni_trans_cmd_from_user(unsigned long narg)
{
    long n_ret;
    struct msg_tool_drv_cmd_t t_cmd_in;
    struct msg_tool_drv_cmd_t *p_trans_cmd;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    drv_jni_clear_msg_tool_mem();

    p_trans_cmd = (struct msg_tool_drv_cmd_t *) g_msg_tool_cmd_in;
    n_ret = copy_from_user(&t_cmd_in, (void *)narg, sizeof(struct msg_tool_drv_cmd_t));
    p_trans_cmd->n_cmdId = t_cmd_in.n_cmdId;

    /*_DebugJniShowArray(&t_cmd_in, sizeof( msg_tool_drv_cmd_t));*/
    if (t_cmd_in.n_snd_cmd_len > 0) {
        p_trans_cmd->n_snd_cmd_len = t_cmd_in.n_snd_cmd_len;
        n_ret =
            copy_from_user(g_snd_cmd_data, u64_to_ptr(t_cmd_in.n_snd_cmd_data_ptr),
                           p_trans_cmd->n_snd_cmd_len);
    }

    if (t_cmd_in.n_rtncmd_len > 0) {
        p_trans_cmd->n_rtncmd_len = t_cmd_in.n_rtncmd_len;
        n_ret =
            copy_from_user(g_rtn_cmd_data, u64_to_ptr(t_cmd_in.n_rtn_cmd_data_ptr),
                           p_trans_cmd->n_rtncmd_len);
    }

    return p_trans_cmd;
}

static void drv_jni_trans_cmd_to_user(struct msg_tool_drv_cmd_t *p_trans_cmd,
                                  unsigned long narg)
{
    struct msg_tool_drv_cmd_t tCmdOut;
    long n_ret;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    n_ret = copy_from_user(&tCmdOut, (void *)narg, sizeof(struct msg_tool_drv_cmd_t));

    /*_DebugJniShowArray(&tCmdOut, sizeof( msg_tool_drv_cmd_t));*/
    n_ret =
        copy_to_user(u64_to_ptr(tCmdOut.n_rtn_cmd_data_ptr), g_rtn_cmd_data,
                     tCmdOut.n_rtncmd_len);
}

static long drv_jni_msg_tool_Ioctl(struct file *pfile, unsigned int ncmd,
                                unsigned long narg)
{
    long n_ret = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    switch (ncmd) {
    case MSGTOOL_IOCTL_RUN_CMD:
        {
            struct msg_tool_drv_cmd_t *p_trans_cmd;

            p_trans_cmd = drv_jni_trans_cmd_from_user(narg);

            switch (p_trans_cmd->n_cmdId) {
            case MSGTOOL_RESETHW:
                drv_touch_device_hw_reset();
                break;
            case MSGTOOL_REGGETXBYTEVALUE:
                drv_jni_reg_get_x_byte_data(p_trans_cmd);
                drv_jni_trans_cmd_to_user(p_trans_cmd, narg);
                break;
            case MSGTOOL_HOTKNOTSTATUS:
                g_rtn_cmd_data[0] = g_is_hotknot_enabled;
                drv_jni_trans_cmd_to_user(p_trans_cmd, narg);
                break;
            case MSGTOOL_FINGERTOUCH:
                if (p_trans_cmd->n_snd_cmd_len == 1) {
                    DBG(&g_i2c_client->dev, "*** JNI enable touch ***\n");
                    drv_enable_finger_touch_report();
                    g_is_disable_finger_touch = 0;
                    /*Resume finger touch ISR handling after MTPTool
                    * APK have sent i2c command to firmware.
                    */
                } else if (p_trans_cmd->n_snd_cmd_len == 0) {
                    DBG(&g_i2c_client->dev, "*** JNI disable touch ***\n");
                    drv_disable_finger_touch_report();
                    g_is_disable_finger_touch = 1;
                    /*Skip finger touch ISR handling temporarily for MTPTool
                    * APK can send i2c command to firmware.
                    */
                }
                break;
            case MSGTOOL_BYPASSHOTKNOT:
                if (p_trans_cmd->n_snd_cmd_len == 1) {
                    DBG(&g_i2c_client->dev,
                        "*** JNI enable bypass hotknot ***\n");
                    g_is_bypass_hotknot = 1;
                } else if (p_trans_cmd->n_snd_cmd_len == 0) {
                    DBG(&g_i2c_client->dev,
                        "*** JNI disable bypass hotknot ***\n");
                    g_is_bypass_hotknot = 0;
                }
                break;
            case MSGTOOL_DEVICEPOWEROFF:
                drv_touch_device_power_off();
                break;
            case MSGTOOL_GETSMDBBUS:
                DBG(&g_i2c_client->dev, "*** MSGTOOL_GETSMDBBUS ***\n");
                g_rtn_cmd_data[0] = slave_i2c_id_db_bus & 0xFF;
                g_rtn_cmd_data[1] = slave_i2c_id_dw_i2c & 0xFF;
                drv_jni_trans_cmd_to_user(p_trans_cmd, narg);
                break;
            case MSGTOOL_SETIICDATARATE:
                DBG(&g_i2c_client->dev, "*** MSGTOOL_SETIICDATARATE ***\n");
                drv_set_iic_data_rate(g_i2c_client,
                                  ((g_snd_cmd_data[1] << 8) | g_snd_cmd_data[0])
                                  * 1000);
                break;
            case MSGTOOL_ERASE_FLASH:
                DBG(&g_i2c_client->dev, "*** MSGTOOL_ERASE_FLASH ***\n");
                if (p_trans_cmd->n_snd_cmd_data_ptr == 0) {
                    DBG(&g_i2c_client->dev, "*** erase Main block ***\n");
                    drv_msg28xx_erase_emem(EMEM_MAIN);
                } else if (p_trans_cmd->n_snd_cmd_data_ptr == 1) {
                    DBG(&g_i2c_client->dev, "*** erase INFO block ***\n");
                    drv_msg28xx_erase_emem(EMEM_INFO);
                }
                break;
            default:
                break;
            }
        }
        break;

    default:
        n_ret = -EINVAL;
        break;
    }

    return n_ret;
}

static void drv_jni_create_msg_tool_mem(void)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);
 /*summer*/
    g_msg_tool_cmd_in = (struct msg_tool_drv_cmd_t *)kmalloc(sizeof(struct msg_tool_drv_cmd_t)
        , GFP_KERNEL);
    g_snd_cmd_data = (u8 *) kmalloc(1024, GFP_KERNEL);
    g_rtn_cmd_data = (u8 *) kmalloc(1024, GFP_KERNEL);
}

static void drv_jni_delete_msg_tool_mem(void)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    if (g_msg_tool_cmd_in) {
        kfree(g_msg_tool_cmd_in);
        g_msg_tool_cmd_in = NULL;/*summer*/
    }

    if (g_snd_cmd_data) {
        kfree(g_snd_cmd_data);
        g_snd_cmd_data = NULL;
    }

    if (g_rtn_cmd_data) {
        kfree(g_rtn_cmd_data);
        g_rtn_cmd_data = NULL;
    }
}
#endif /*CONFIG_ENABLE_JNI_INTERFACE */

/*--------------------------------------------------------------------------*/
#ifdef CONFIG_ENABLE_REGULATOR_POWER_ON
void drv_touch_device_regulator_power_on(bool n_flag)
{
#if defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM)
    s32 n_ret_val = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    if (g_regu_vdd != NULL && g_regu_vcc_i2c != NULL) {
        if (n_flag == true) {
            n_ret_val = regulator_enable(g_regu_vdd);
            if (n_ret_val) {
                DBG_INFO(&g_i2c_client->dev,
                    "regulator_enable(g_regu_vdd) failed. n_ret_val=%d\n",
                    n_ret_val);
            }
            n_ret_val = regulator_enable(g_regu_vcc_i2c);
            if (n_ret_val) {
                DBG_INFO(&g_i2c_client->dev,
                    "regulator_enable(g_regu_vcc_i2c) failed. n_ret_val=%d\n",
                    n_ret_val);
            }
            msleep(20);
        } else {
            n_ret_val = regulator_disable(g_regu_vdd);
            if (n_ret_val) {
                DBG_INFO(&g_i2c_client->dev,
                    "regulator_disable(g_regu_vdd) failed. n_ret_val=%d\n",
                    n_ret_val);
            }
            n_ret_val = regulator_disable(g_regu_vcc_i2c);
            if (n_ret_val) {
                DBG_INFO(&g_i2c_client->dev,
                    "regulator_disable(g_regu_vcc_i2c) failed. n_ret_val=%d\n",
                    n_ret_val);
            }
            msleep(20);
        }
    }
#elif defined(CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM)

#ifdef CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD
    s32 n_ret_val = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    if (g_regu_vdd != NULL) {
        if (n_flag == true) {
            n_ret_val = regulator_enable(g_regu_vdd);
            if (n_ret_val) {
                DBG(&g_i2c_client->dev,
                    "regulator_enable(g_regu_vdd) failed. n_ret_val=%d\n",
                    n_ret_val);
            }
            mdelay(20);
        } else {
            n_ret_val = regulator_disable(g_regu_vdd);
            if (n_ret_val) {
                DBG(&g_i2c_client->dev,
                    "regulator_disable(g_regu_vdd) failed. n_ret_val=%d\n",
                    n_ret_val);
            }
            mdelay(20);
        }
    }
#else
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

/*hwPowerOn(MT6323_POWER_LDO_VGP1, VOL_2800, "TP");*/
/*For specific MTK BB chip(ex. MT6582), need to enable this function
 *call for correctly power on Touch IC.
 */
    hwPowerOn(PMIC_APP_CAP_TOUCH_VDD, VOL_2800, "TP");
/*For specific MTK BB chip(ex. MT6735), need to enable this function
* call for correctly power on Touch IC.
*/
#endif /*CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD */

#endif
}
#endif /*CONFIG_ENABLE_REGULATOR_POWER_ON */

void drv_touch_device_power_on(void)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

#if defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM)
#ifdef ALLWINNER_PLATFORM

    input_set_power_enable(&(config_info.input_type), 1);

    gpio_direction_output(config_info.wakeup_gpio.gpio, 1);
    mudelay(10);
    __gpio_set_value(config_info.wakeup_gpio.gpio, 0);
    msleep(10);
    __gpio_set_value(config_info.wakeup_gpio.gpio, 1);
    msleep(25);

#else
    if (ms_ts_msg_ic_gpio_rst > 0) {
        gpio_direction_output(ms_ts_msg_ic_gpio_rst, 1);

/*gpio_set_value(ms_ts_msg_ic_gpio_rst, 1);*/
        udelay(10);
        gpio_set_value(ms_ts_msg_ic_gpio_rst, 0);
        msleep(10);
        gpio_set_value(ms_ts_msg_ic_gpio_rst, 1);
        msleep(25);
    }
#endif /*ALLWINNER_PLATFORM */
#elif defined(CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM)
#ifdef CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD
    if (ms_ts_msg_ic_gpio_rst >= 0 && ms_ts_msg_ic_gpio_rst != 1) {
        /*ms_ts_msg_ic_gpio_rst must be a value other than 1 */
        tpd_gpio_output(ms_ts_msg_ic_gpio_rst, 1);
        mdelay(10);
        tpd_gpio_output(ms_ts_msg_ic_gpio_rst, 0);
        mdelay(10);
        tpd_gpio_output(ms_ts_msg_ic_gpio_rst, 1);
        mdelay(25);
    }
#else
    if (ms_ts_msg_ic_gpio_rst > 0) {
        mt_set_gpio_mode(ms_ts_msg_ic_gpio_rst, GPIO_CTP_RST_PIN_M_GPIO);
        mt_set_gpio_dir(ms_ts_msg_ic_gpio_rst, GPIO_DIR_OUT);
        mt_set_gpio_out(ms_ts_msg_ic_gpio_rst, GPIO_OUT_ONE);
        mdelay(10);

        mt_set_gpio_mode(ms_ts_msg_ic_gpio_rst, GPIO_CTP_RST_PIN_M_GPIO);
        mt_set_gpio_dir(ms_ts_msg_ic_gpio_rst, GPIO_DIR_OUT);
        mt_set_gpio_out(ms_ts_msg_ic_gpio_rst, GPIO_OUT_ZERO);
        mdelay(10);

#ifdef TPD_CLOSE_POWER_IN_SLEEP
        hwPowerDown(TPD_POWER_SOURCE, "TP");
        mdelay(10);
        hwPowerOn(TPD_POWER_SOURCE, VOL_2800, "TP");
        mdelay(10);             /*reset pulse */
#endif /*TPD_CLOSE_POWER_IN_SLEEP */

        mt_set_gpio_mode(ms_ts_msg_ic_gpio_rst, GPIO_CTP_RST_PIN_M_GPIO);
        mt_set_gpio_dir(ms_ts_msg_ic_gpio_rst, GPIO_DIR_OUT);
        mt_set_gpio_out(ms_ts_msg_ic_gpio_rst, GPIO_OUT_ONE);
        mdelay(25);
    }
#endif /*CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD */
#endif

#ifdef CONFIG_ENABLE_CHARGER_DETECTION
    g_is_enable_charger_plugin_out_check = 1;
#endif /*CONFIG_ENABLE_CHARGER_DETECTION */

#ifdef CONFIG_ENABLE_ESD_PROTECTION
    g_is_enable_esd_check = 1;
    g_is_hw_reset_by_driver = 1;
/*Indicate HwReset is triggered by Device Driver instead
 * of Firmware or Touch IC
 */
#endif /*CONFIG_ENABLE_ESD_PROTECTION */
}

void drv_touch_device_power_off(void)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    drv_optimize_current_consumption();

#if defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM)
#ifdef ALLWINNER_PLATFORM
    input_set_power_enable(&(config_info.input_type), 0);
    __gpio_set_value(config_info.wakeup_gpio.gpio, 0);
#else
    if (ms_ts_msg_ic_gpio_rst > 0) {

/*gpio_direction_output(ms_ts_msg_ic_gpio_rst, 0);*/
        gpio_set_value(ms_ts_msg_ic_gpio_rst, 0);
    }
#endif /*ALLWINNER_PLATFORM */
#elif defined(CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM)
#ifdef CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD
    if (ms_ts_msg_ic_gpio_rst >= 0 && ms_ts_msg_ic_gpio_rst != 1) {
        /*ms_ts_msg_ic_gpio_rst must be a value other than 1 */
        tpd_gpio_output(ms_ts_msg_ic_gpio_rst, 0);
    }
#else
    if (ms_ts_msg_ic_gpio_rst > 0) {
        mt_set_gpio_mode(ms_ts_msg_ic_gpio_rst, GPIO_CTP_RST_PIN_M_GPIO);
        mt_set_gpio_dir(ms_ts_msg_ic_gpio_rst, GPIO_DIR_OUT);
        mt_set_gpio_out(ms_ts_msg_ic_gpio_rst, GPIO_OUT_ZERO);

#ifdef TPD_CLOSE_POWER_IN_SLEEP
        hwPowerDown(TPD_POWER_SOURCE, "TP");
#endif /*TPD_CLOSE_POWER_IN_SLEEP */
    }
#endif /*CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD */
#endif

#ifdef CONFIG_ENABLE_CHARGER_DETECTION
    g_is_enable_charger_plugin_out_check = 0;
#endif /*CONFIG_ENABLE_CHARGER_DETECTION */

#ifdef CONFIG_ENABLE_ESD_PROTECTION
    g_is_enable_esd_check = 0;
#endif /*CONFIG_ENABLE_ESD_PROTECTION */
}

void drv_touch_device_hw_reset(void)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

#if defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM)
#ifdef ALLWINNER_PLATFORM

    gpio_direction_output(config_info.wakeup_gpio.gpio, 1);
    udelay(10);
    __gpio_set_value(config_info.wakeup_gpio.gpio, 0);
    msleep(10);
    __gpio_set_value(config_info.wakeup_gpio.gpio, 1);

    msleep(25);
#else
    if (ms_ts_msg_ic_gpio_rst > 0) {
        gpio_direction_output(ms_ts_msg_ic_gpio_rst, 1);

/*gpio_set_value(ms_ts_msg_ic_gpio_rst, 1);*/
        udelay(10);
        gpio_set_value(ms_ts_msg_ic_gpio_rst, 0);
        msleep(10);
        gpio_set_value(ms_ts_msg_ic_gpio_rst, 1);
        msleep(25);
    }
#endif /*ALLWINNER_PLATFORM */
#elif defined(CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM)
#ifdef CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD
    if (ms_ts_msg_ic_gpio_rst >= 0 && ms_ts_msg_ic_gpio_rst != 1) {
        /*ms_ts_msg_ic_gpio_rst must be a value other than 1 */
        tpd_gpio_output(ms_ts_msg_ic_gpio_rst, 1);
        mdelay(10);
        tpd_gpio_output(ms_ts_msg_ic_gpio_rst, 0);
        mdelay(10);
        tpd_gpio_output(ms_ts_msg_ic_gpio_rst, 1);
        mdelay(25);
    }
#else
    if (ms_ts_msg_ic_gpio_rst > 0) {
        mt_set_gpio_mode(ms_ts_msg_ic_gpio_rst, GPIO_CTP_RST_PIN_M_GPIO);
        mt_set_gpio_dir(ms_ts_msg_ic_gpio_rst, GPIO_DIR_OUT);
        mt_set_gpio_out(ms_ts_msg_ic_gpio_rst, GPIO_OUT_ONE);
        mdelay(10);
        mt_set_gpio_mode(ms_ts_msg_ic_gpio_rst, GPIO_CTP_RST_PIN_M_GPIO);
        mt_set_gpio_dir(ms_ts_msg_ic_gpio_rst, GPIO_DIR_OUT);
        mt_set_gpio_out(ms_ts_msg_ic_gpio_rst, GPIO_OUT_ZERO);
        mdelay(10);
        mt_set_gpio_mode(ms_ts_msg_ic_gpio_rst, GPIO_CTP_RST_PIN_M_GPIO);
        mt_set_gpio_dir(ms_ts_msg_ic_gpio_rst, GPIO_DIR_OUT);
        mt_set_gpio_out(ms_ts_msg_ic_gpio_rst, GPIO_OUT_ONE);
        mdelay(25);
    }
#endif /*CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD */
#endif

#ifdef CONFIG_ENABLE_CHARGER_DETECTION
    g_is_enable_charger_plugin_out_check = 1;
#endif /*CONFIG_ENABLE_CHARGER_DETECTION */

#ifdef CONFIG_ENABLE_ESD_PROTECTION
    g_is_enable_esd_check = 1;
    g_is_hw_reset_by_driver = 1;
    /*Indicate HwReset is triggered by Device Driver instead
    * of Firmware or Touch IC
    */
#endif /*CONFIG_ENABLE_ESD_PROTECTION */
}

void drv_disable_finger_touch_report(void)
{
    unsigned long nIrqFlag;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);
    DBG(&g_i2c_client->dev, "*** %s() g_interrupt_flag = %d ***\n", __func__,
        g_interrupt_flag);

    spin_lock_irqsave(&g_irq_lock, nIrqFlag);

#if defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM)

    if (g_interrupt_flag == 1) {
        disable_irq_nosync(girq);
        g_interrupt_flag = 0;
    }
#elif defined(CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM)

    if (g_interrupt_flag == 1) {
#ifdef CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD
        disable_irq_nosync(girq);
#else
        mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
#endif /*CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD */

        g_interrupt_flag = 0;
    }
#endif

    spin_unlock_irqrestore(&g_irq_lock, nIrqFlag);
}

void drv_enable_finger_touch_report(void)
{
    unsigned long nIrqFlag;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);
    DBG(&g_i2c_client->dev, "*** %s() g_interrupt_flag = %d ***\n", __func__,
        g_interrupt_flag);

    spin_lock_irqsave(&g_irq_lock, nIrqFlag);

#if defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM)

    if (g_interrupt_flag == 0) {
        enable_irq(girq);

        g_interrupt_flag = 1;
    }
#elif defined(CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM)

    if (g_interrupt_flag == 0) {
#ifdef CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD
        enable_irq(girq);
#else
        mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
#endif /*CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD */

        g_interrupt_flag = 1;
    }
#endif

    spin_unlock_irqrestore(&g_irq_lock, nIrqFlag);
}

void drv_finger_touch_pressed(s32 n_x, s32 n_y, s32 nPressure, s32 n_id)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);
    DBG(&g_i2c_client->dev, "point touch pressed\n");

#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL    /*TYPE B PROTOCOL */
    input_mt_slot(g_input_device, n_id);
    input_mt_report_slot_state(g_input_device, MT_TOOL_FINGER, true);
    input_report_abs(g_input_device, ABS_MT_POSITION_X, n_x);
    input_report_abs(g_input_device, ABS_MT_POSITION_Y, n_y);

    DBG(&g_i2c_client->dev, "n_id=%d, n_x=%d, n_y=%d\n", n_id, n_x, n_y);
    /*TODO : add for debug */
#else /*TYPE A PROTOCOL */
    input_report_key(g_input_device, BTN_TOUCH, 1);
    if (g_chip_type == CHIP_TYPE_MSG28XX || g_chip_type == CHIP_TYPE_MSG58XXA
        || g_chip_type == CHIP_TYPE_ILI2118A || g_chip_type == CHIP_TYPE_ILI2117A
        || g_chip_type == CHIP_TYPE_ILI2120 || g_chip_type == CHIP_TYPE_ILI2121) {
        input_report_abs(g_input_device, ABS_MT_TRACKING_ID, n_id);
        /*ABS_MT_TRACKING_ID is used for MSG28xx/MSG58xxA/ILI21xx only */
    }
    input_report_abs(g_input_device, ABS_MT_TOUCH_MAJOR, 1);
    input_report_abs(g_input_device, ABS_MT_WIDTH_MAJOR, 1);
    input_report_abs(g_input_device, ABS_MT_POSITION_X, n_x);
    input_report_abs(g_input_device, ABS_MT_POSITION_Y, n_y);

    input_mt_sync(g_input_device);
#endif /*CONFIG_ENABLE_TYPE_B_PROTOCOL */

#ifdef CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM
#ifdef CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD
#ifdef CONFIG_MTK_BOOT
/*summer*/
    if (tpd_dts_data.use_tpd_button) {
        if (FACTORY_BOOT == get_boot_mode() ||
            RECOVERY_BOOT == get_boot_mode()) {
            tpd_button(n_x, n_y, 1);
            DBG(&g_i2c_client->dev,
                "tpd_button(n_x, n_y, 1) = tpd_button(%d, %d, 1)\n", n_x, n_y);
            /*TODO : add for debug */
        }
    }
#endif /*CONFIG_MTK_BOOT */
#else
#ifdef CONFIG_TP_HAVE_KEY
    if (FACTORY_BOOT == get_boot_mode() || RECOVERY_BOOT == get_boot_mode()) {
        tpd_button(n_x, n_y, 1);
        DBG(&g_i2c_client->dev,
            "tpd_button(n_x, n_y, 1) = tpd_button(%d, %d, 1)\n", n_x, n_y);
        /*TODO : add for debug */
    }
#endif /*CONFIG_TP_HAVE_KEY */
#endif /*CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD */

    TPD_EM_PRINT(n_x, n_y, n_x, n_y, n_id, 1);
#endif /*CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM */
}

void drv_finger_touch_released(s32 n_x, s32 n_y, s32 n_id)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);
    DBG(&g_i2c_client->dev, "point touch released\n");

#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL    /*TYPE B PROTOCOL */
    /*if (n_id == 0) */
    /*u32TouchCount = 0; */
    input_mt_slot(g_input_device, n_id);
    input_mt_report_slot_state(g_input_device, MT_TOOL_FINGER, false);

    DBG(&g_i2c_client->dev, "n_id=%d\n", n_id);   /*TODO : add for debug */
#else /*TYPE A PROTOCOL */
    input_report_key(g_input_device, BTN_TOUCH, 0);
    input_mt_sync(g_input_device);
#endif /*CONFIG_ENABLE_TYPE_B_PROTOCOL */

#ifdef CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM
#ifdef CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD
#ifdef CONFIG_MTK_BOOT
    if (tpd_dts_data.use_tpd_button) {
        if (FACTORY_BOOT == get_boot_mode() ||
            RECOVERY_BOOT == get_boot_mode()) {
            tpd_button(n_x, n_y, 0);
            DBG(&g_i2c_client->dev,
                "tpd_button(n_x, n_y, 0) = tpd_button(%d, %d, 0)\n", n_x, n_y);
            /*TODO : add for debug */
        }
    }
#endif /*CONFIG_MTK_BOOT */
#else
#ifdef CONFIG_TP_HAVE_KEY
    if (FACTORY_BOOT == get_boot_mode() || RECOVERY_BOOT == get_boot_mode()) {
        tpd_button(n_x, n_y, 0);
        DBG(&g_i2c_client->dev,
            "tpd_button(n_x, n_y, 0) = tpd_button(%d, %d, 0)\n", n_x, n_y);
        /*TODO : add for debug */
    }
#endif /*CONFIG_TP_HAVE_KEY */
#endif /*CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD */

    TPD_EM_PRINT(n_x, n_y, n_x, n_y, 0, 0);
#endif /*CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM */
}

void drv_set_iic_data_rate(struct i2c_client *pClient, u32 nIic_dataRate)
{
    DBG(&g_i2c_client->dev, "*** %s() nIic_dataRate = %d ***\n", __func__,
        nIic_dataRate);

#if defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM)
    /*TODO : Please FAE colleague to confirm with customer device driver
    * engineer for how to set i2c data rate on SPRD platform
    */
    sprd_i2c_ctl_chg_clk(pClient->adapter->nr, nIic_dataRate);
    mdelay(100);
#elif defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM)
    /*TODO : Please FAE colleague to confirm with customer device driver
    * engineer for how to set i2c data rate on QCOM platform
    */
#elif defined(CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM)
    pClient->timing = nIic_dataRate / 1000;
#endif
}

/*--------------------------------------------------------------------------*/
#ifdef CONFIG_ENABLE_PROXIMITY_DETECTION
    s32 drv_enable_proximity(void)
{
    u8 sz_tx_data[4] = { 0 };
    u32 i = 0;
    s32 rc;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    sz_tx_data[0] = 0x52;
    sz_tx_data[1] = 0x00;

    if (g_chip_type == CHIP_TYPE_MSG22XX) {
        sz_tx_data[2] = 0x4a;
    } else if (g_chip_type == CHIP_TYPE_MSG28XX
               || g_chip_type == CHIP_TYPE_MSG58XXA
               || g_chip_type == CHIP_TYPE_ILI2118A ||
               g_chip_type == CHIP_TYPE_ILI2117A) {
        sz_tx_data[2] = 0x47;
    } else {
        DBG(&g_i2c_client->dev, "*** Un-recognized chip type = 0x%x ***\n",
            g_chip_type);
		return -EFAULT;
    }

    sz_tx_data[3] = 0xa0;

    while (i < 5) {
        mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);   /*delay 20ms */
        rc = iic_write_data(slave_i2c_id_dw_i2c, &sz_tx_data[0], 4);
        if (rc > 0) {
            g_enable_tp_proximity = 1;

            DBG(&g_i2c_client->dev, "Enable proximity detection success\n");
            break;
        }

        i++;
    }
    if (i == 5) {
        DBG(&g_i2c_client->dev, "Enable proximity detection failed, rc = %d\n",
            rc);
    }

    return rc;
}

s32 drv_disable_proximity(void)
{
    u8 sz_tx_data[4] = { 0 };
    u32 i = 0;
    s32 rc;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    sz_tx_data[0] = 0x52;
    sz_tx_data[1] = 0x00;

    if (g_chip_type == CHIP_TYPE_MSG22XX) {
        sz_tx_data[2] = 0x4a;
    } else if (g_chip_type == CHIP_TYPE_MSG28XX
               || g_chip_type == CHIP_TYPE_MSG58XXA
               || g_chip_type == CHIP_TYPE_ILI2118A ||
               g_chip_type == CHIP_TYPE_ILI2117A) {
        sz_tx_data[2] = 0x47;
    } else {
        DBG(&g_i2c_client->dev, "*** Un-recognized chip type = 0x%x ***\n",
            g_chip_type);
		return -EFAULT;
    }

    sz_tx_data[3] = 0xa1;

    while (i < 5) {
        mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
        rc = iic_write_data(slave_i2c_id_dw_i2c, &sz_tx_data[0], 4);
        if (rc > 0) {
            g_enable_tp_proximity = 0;

            DBG(&g_i2c_client->dev, "Disable proximity detection success\n");
            break;
        }

        i++;
    }
    if (i == 5) {
        DBG(&g_i2c_client->dev, "Disable proximity detection failed, rc = %d\n",
            rc);
    }
#if defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM)
    g_face_closing_tp = 0;
#endif
/*CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM||CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM */

    return rc;
}

int drv_get_tp_ps_data(void)
{
    DBG(&g_i2c_client->dev, "*** %s() g_face_closing_tp = %d ***\n", __func__,
        g_face_closing_tp);

    return g_face_closing_tp;
}

#if defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM)
void drv_tp_ps_enable(int n_enable)
{
    DBG(&g_i2c_client->dev, "*** %s() n_enable = %d ***\n", __func__, n_enable);

    if (n_enable)
        drv_enable_proximity();
    else
        drv_disable_proximity();

}

static int drv_proximity_open(struct inode *inode, struct file *file)
{
    int n_ret_val = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    n_ret_val = nonseekable_open(inode, file);
    if (n_ret_val < 0)
        return n_ret_val;

    file->private_data = i2c_get_clientdata(g_i2c_client);

    return 0;
}

static int drv_proximity_release(struct inode *inode, struct file *file)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    return 0;
}

static atomic_t g_ps_flag;

static long drv_proximity_ioctl(struct file *file, unsigned int cmd,
                               unsigned long arg)
{
#if 0
    DBG(&g_i2c_client->dev, "*** %s() *** cmd = %d\n", __func__, _IOC_NR(cmd));

    switch (cmd) {
    case GTP_IOCTL_PROX_ON:
        drv_tp_ps_enable(1);
        break;
    case GTP_IOCTL_PROX_OFF:
        drv_tp_ps_enable(0);
        break;
    default:
        return -EINVAL;
    }
#else
    void __user *argp = (void __user *)arg;
    int flag;
    unsigned char data;

    DBG(&g_i2c_client->dev, "*** %s() *** cmd = %d\n", __func__, _IOC_NR(cmd));

    switch (cmd) {
    case LTR_IOCTL_SET_PFLAG:
        if (copy_from_user(&flag, argp, sizeof(flag)))
            return -EFAULT;


        if (flag < 0 || flag > 1)
            return -EINVAL;

        DBG(&g_i2c_client->dev, "flag = %d", flag);

        atomic_set(&g_ps_flag, flag);

        if (flag == 1)
            drv_tp_ps_enable(1);
        else if (flag == 0)
            drv_tp_ps_enable(0);

        break;

    case LTR_IOCTL_GET_PFLAG:
        flag = atomic_read(&g_ps_flag);

        if (copy_to_user(argp, &flag, sizeof(flag)))
            return -EFAULT;

        DBG(&g_i2c_client->dev, "flag = %d", flag);
        break;

    case LTR_IOCTL_GET_DATA:
        if (copy_to_user(argp, &data, sizeof(data)))
            return -EFAULT;

        DBG(&g_i2c_client->dev, "flag = %d", flag);
        break;

    case GTP_IOCTL_PROX_ON:
        drv_tp_ps_enable(1);
        break;

    case GTP_IOCTL_PROX_OFF:
        drv_tp_ps_enable(0);
        break;

    default:
        DBG(&g_i2c_client->dev, "*** %s() *** Invalid cmd = %d\n", __func__,
            _IOC_NR(cmd));
        return -EINVAL;
    }
#endif

    return 0;
}

static const struct file_operations proximity_file_ops = {
    .owner = THIS_MODULE,
    .open = drv_proximity_open,

    .release = NULL, /*drv_proximity_release,*/
    .unlocked_ioctl = drv_proximity_ioctl,
};

static struct miscdevice proximity_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "ltr_558als",       /*Match the hal's name */
    .fops = &proximity_file_ops,
};

static int drv_proximity_input_deviceInit(struct i2c_client *pClient)
{
    int n_ret_val = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    n_ret_val = misc_register(&proximity_misc);
    if (n_ret_val) {
        DBG(&g_i2c_client->dev,
            "*** Failed to misc_register() for proximity *** n_ret_val=%d\n",
            n_ret_val);
        goto ERROR_MISC_REGISTER_FAILED;
    }

    g_proximityinput_device = input_allocate_device();
    if (g_proximityinput_device == NULL) {
        DBG(&g_i2c_client->dev,
            "*** Failed to allocate proximity input device ***\n");
        n_ret_val = -ENOMEM;
        goto ERROR_INPUT_DEVICE_ALLOCATE_FAILED;
    }

    g_proximityinput_device->name = "alps_pxy";
    g_proximityinput_device->phys = "alps_pxy";
    g_proximityinput_device->id.bustype = BUS_I2C;
    g_proximityinput_device->dev.parent = &pClient->dev;
    g_proximityinput_device->id.vendor = 0x0001;
    g_proximityinput_device->id.product = 0x0001;
    g_proximityinput_device->id.version = 0x0010;

    set_bit(EV_ABS, g_proximityinput_device->evbit);

    input_set_abs_params(g_proximityinput_device, ABS_DISTANCE, 0, 1, 0, 0);

    n_ret_val = input_register_device(g_proximityinput_device);
    if (n_ret_val < 0) {
        DBG(&g_i2c_client->dev,
            "*** Unable to register proximity input device *** n_ret_val=%d\n",
            n_ret_val);
        goto ERROR_INPUT_DEVICE_REGISTER_FAILED;
    }

    return 0;

ERROR_INPUT_DEVICE_REGISTER_FAILED:
    if (g_proximityinput_device) {
        input_free_device(g_proximityinput_device);
        g_proximityinput_device = NULL;
    }
ERROR_INPUT_DEVICE_ALLOCATE_FAILED:
    misc_deregister(&proximity_misc);
ERROR_MISC_REGISTER_FAILED:

    return n_ret_val;
}

static int drv_proximity_input_device_uninit(void)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    misc_deregister(&proximity_misc);

    if (g_proximityinput_device) {
        input_unregister_device(g_proximityinput_device);
        g_proximityinput_device = NULL;
    }

    return 0;
}
#elif defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM)
static struct sensors_classdev g_proximity_cdev;

static struct sensors_classdev sensors_proximity_cdev = {
    .name = "msg2xxx-proximity",
    .vendor = "MStar",
    .version = 1,
    .handle = SENSORS_PROXIMITY_HANDLE,
    .type = SENSOR_TYPE_PROXIMITY,
    .max_range = "5.0",
    .resolution = "5.0",
    .sensor_power = "0.1",
    .min_delay = 0,
    .fifo_reserved_event_count = 0,
    .fifo_max_event_count = 0,
    .enabled = 0,
    .delay_msec = 200,
    .sensors_enable = NULL,
    .sensors_poll_delay = NULL,
};

int drv_tp_ps_enable(struct sensors_classdev *p_proximity_cdev,
                                    unsigned int n_enable)
{
    DBG(&g_i2c_client->dev, "*** %s() n_enable = %d ***\n", __func__, n_enable);

    if (n_enable)
        drv_enable_proximity();
    else
        drv_disable_proximity();


    return 0;
}

static ssize_t drv_proximity_detection_show(struct device *dev,
                                          struct device_attribute *attr,
                                          char *buf)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    DBG(&g_i2c_client->dev, "*** Tp Proximity State = %s ***\n",
        g_enable_tp_proximity ? "open" : "close");

    return snprintf(buf, 10, "%s\n", g_enable_tp_proximity ? "open" : "close");
}

static ssize_t drv_proximity_detection_store(struct device *dev,
                                           struct device_attribute *attr,
                                           const char *buf, size_t size)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    if (buf != NULL) {
        if (sysfs_streq(buf, "1"))
            drv_tp_ps_enable(&g_proximity_cdev, 1);
        else if (sysfs_streq(buf, "0"))
            drv_tp_ps_enable(&g_proximity_cdev, 0);
    }

    return size;
}

static struct device_attribute proximity_attribute =
__ATTR(proximity, 0666 /*0664 */,  drv_proximity_detection_show,
       drv_proximity_detection_store);

static struct attribute *proximity_detection_attrs[] = {
    &proximity_attribute.attr,
    NULL
};

static struct attribute_group proximity_detection_attribute_group = {
    .name = "Driver",
    .attrs = proximity_detection_attrs,
};

static int drv_proximity_input_deviceInit(struct i2c_client *pClient)
{
    int n_ret_val = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    g_proximityinput_device = input_allocate_device();
    if (g_proximityinput_device == NULL) {
        DBG(&g_i2c_client->dev,
            "*** Failed to allocate proximity input device ***\n");
        n_ret_val = -ENOMEM;
        goto ERROR_INPUT_DEVICE_ALLOCATE_FAILED;
    }

    g_proximityinput_device->name = "msg2xxx-ps";
    g_proximityinput_device->phys = "I2C";
    g_proximityinput_device->dev.parent = &pClient->dev;
    g_proximityinput_device->id.bustype = BUS_I2C;

    set_bit(EV_ABS, g_proximityinput_device->evbit);

    input_set_abs_params(g_proximityinput_device, ABS_DISTANCE, 0, 1, 0, 0);

    n_ret_val = input_register_device(g_proximityinput_device);
    if (n_ret_val < 0) {
        DBG(&g_i2c_client->dev,
            "*** Unable to register proximity input device *** n_ret_val=%d\n",
            n_ret_val);
        goto ERROR_INPUT_DEVICE_REGISTER_FAILED;
    }

    mdelay(10);

    n_ret_val =
        sysfs_create_group(&g_proximityinput_device->dev.kobj,
                           &proximity_detection_attribute_group);
    if (n_ret_val < 0) {
        DBG(&g_i2c_client->dev,
         "*** Failed to sysfs_create_group() for proximity *** n_ret_val=%d\n",
            n_ret_val);
        goto ERROR_SYSFS_CREATE_GROUP_FAILED;
    }

    input_set_drvdata(g_proximityinput_device, NULL);

    sensors_proximity_cdev.sensors_enable = drv_tp_ps_enable;
    n_ret_val = sensors_classdev_register(&pClient->dev,
        &sensors_proximity_cdev);
    if (n_ret_val < 0) {
        DBG(&g_i2c_client->dev,
    "*** Failed to sensors_classdev_register() for proximity *** n_ret_val=%d\n",
            n_ret_val);
        goto ERROR_SENSORS_CLASSDEV_REGISTER_FAILED;
    }

    return 0;

ERROR_SENSORS_CLASSDEV_REGISTER_FAILED:
ERROR_SYSFS_CREATE_GROUP_FAILED:
    if (g_proximityinput_device) {
        input_unregister_device(g_proximityinput_device);
        g_proximityinput_device = NULL;
    }
ERROR_INPUT_DEVICE_REGISTER_FAILED:
    if (g_proximityinput_device) {
        input_free_device(g_proximityinput_device);
        g_proximityinput_device = NULL;
    }
ERROR_INPUT_DEVICE_ALLOCATE_FAILED:

    return n_ret_val;
}

static int drv_proximity_input_device_uninit(void)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    if (g_proximityinput_device) {
        input_unregister_device(g_proximityinput_device);
        g_proximityinput_device = NULL;
    }

    sensors_classdev_unregister(&sensors_proximity_cdev);

    return 0;
}
#elif defined(CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM)
int drv_tp_ps_operate(void *pSelf, u32 nCommand, void *pBuffIn, int nSizeIn,
                   void *pBuffOut, int nSizeOut, int *pActualOut)
{
    int nErr = 0;
    int n_value;
    hwm_sensor_data *pSensor_data;

    switch (nCommand) {
    case SENSOR_DELAY:
        if ((pBuffIn == NULL) || (nSizeIn < sizeof(int))) {
            DBG(&g_i2c_client->dev, "Set delay parameter error!\n");
            nErr = -EINVAL;
        }
        /*Do nothing */
        break;

    case SENSOR_ENABLE:
        if ((pBuffIn == NULL) || (nSizeIn < sizeof(int))) {
            DBG(&g_i2c_client->dev, "Enable sensor parameter error!\n");
            nErr = -EINVAL;
        } else {
            n_value = *(int *)pBuffIn;
            if (n_value) {
                if (drv_enable_proximity() < 0) {
                    DBG(&g_i2c_client->dev, "Enable ps fail: %d\n", nErr);
					return -EFAULT;
                }
            } else {
                if (drv_disable_proximity() < 0) {
                    DBG(&g_i2c_client->dev, "Disable ps fail: %d\n", nErr);
					return -EFAULT;
                }
            }
        }
        break;

    case SENSOR_GET_DATA:
        if ((pBuffOut == NULL) || (nSizeOut < sizeof(hwm_sensor_data))) {
            DBG(&g_i2c_client->dev, "Get sensor data parameter error!\n");
            nErr = -EINVAL;
        } else {
            pSensor_data = (hwm_sensor_data *) pBuffOut;

            pSensor_data->values[0] = drv_get_tp_ps_data();
            pSensor_data->value_divide = 1;
            pSensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
        }
        break;

    default:
        DBG(&g_i2c_client->dev, "Un-recognized parameter %d!\n", nCommand);
        nErr = -1;
        break;
    }

    return nErr;
}
#endif
#endif /*CONFIG_ENABLE_PROXIMITY_DETECTION */
void drv_set_film_mode(u8 nFilmtype)
{
    u8 sz_tx_data[2] = { 0x13, nFilmtype };
    s32 rc;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
    rc = iic_write_data(slave_i2c_id_dw_i2c, &sz_tx_data[0], 2);
    if (rc > 0)
        DBG(&g_i2c_client->dev, "Set Film Mode success,\n");
}

int drv_get_film_mode(void)
{
    u8 sz_tx_data[1] = { 0x12 };
    u8 sz_rx_data[3] = { 0 };
    s32 rc = -1;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);
    mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
    rc = iic_write_data(slave_i2c_id_dw_i2c, &sz_tx_data[0], 1);
    if (rc > 0)
        DBG(&g_i2c_client->dev, "Get firmware info iic_write_data() success\n");

    mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
    rc = iic_read_data(slave_i2c_id_dw_i2c, &sz_rx_data[0], 3);
    if (rc > 0) {
        if (sz_rx_data[1] == FEATURE_FILM_MODE_LOW_STRENGTH) {
            DBG(&g_i2c_client->dev, "Film mode: Low strength\n");
            return FEATURE_FILM_MODE_LOW_STRENGTH;
        } else if (sz_rx_data[1] == FEATURE_FILM_MODE_HIGH_STRENGTH) {
            DBG(&g_i2c_client->dev, "Film mode: High strength\n");
            return FEATURE_FILM_MODE_HIGH_STRENGTH;
        } else if (sz_rx_data[1] == FEATURE_FILM_MODE_DEFAULT) {
            DBG(&g_i2c_client->dev, "Film mode: Default\n");
            return FEATURE_FILM_MODE_DEFAULT;
        }
    }

    return rc;
}

/*--------------------------------------------------------------------------*/
#ifdef CONFIG_ENABLE_ESD_PROTECTION
void drv_esd_check(struct work_struct *p_work)
{
#ifdef CONFIG_ENABLE_ESD_CHECK_COMMAND_BY_FIRMWARE
    static u8 szEsdCheckValue[8];
    u8 sz_tx_data[3] = { 0 };
    u8 sz_rx_data[8] = { 0 };
    u32 i = 0;
    s32 retW = -1;
    s32 retR = -1;
#else
    u8 sz_data[1] = { 0x00 };
    u32 i = 0;
    s32 rc = 0;
#endif /*CONFIG_ENABLE_ESD_CHECK_COMMAND_BY_FIRMWARE */

    DBG(&g_i2c_client->dev, "*** %s() g_is_enable_esd_check = %d ***\n", __func__,
        g_is_enable_esd_check);

    if (g_is_enable_esd_check == 0)
        return;

    if (g_interrupt_flag == 0) {    /*Skip ESD check while finger touch */
        DBG(&g_i2c_client->dev,
            "Not allow to do ESD check while finger touch.\n");
        goto EsdCheckEnd;
    }

    if (g_is_update_firmware != 0) {
        /*Check whether update frimware is finished */
        DBG(&g_i2c_client->dev,
            "Not allow to do ESD check while update firmware is proceeding.\n");
        goto EsdCheckEnd;
    }
#ifdef CONFIG_ENABLE_ITO_MP_TEST
    if (g_is_in_mp_test == 1) { /*Check whether mp test is proceeding */
        DBG(&g_i2c_client->dev,
            "Not allow to do ESD check while mp test is proceeding.\n");
        goto EsdCheckEnd;
    }
#endif /*CONFIG_ENABLE_ITO_MP_TEST */

    if (is_firmware_data_log_enabled) {
        if (g_firmware_mode == firmware_mode_debug_mode) {
            /*Skip ESD check while firmware mode is DEBUG MODE */
            DBG(&g_i2c_client->dev,
           "Not allow to do ESD check while firmware mode is DEBUG MODE.\n");
            goto EsdCheckEnd;
        }
    }

    if (is_disable_esd_protection_check) {
        /*Skip ESD check while mp test is triggered by
        * MTPTool APK through JNI interface
        */
        DBG(&g_i2c_client->dev, "Not allow to do ESD check while mp test is triggered by MTPTool APK through JNI interface.\n");
        goto EsdCheckEnd;
    }
#ifdef CONFIG_ENABLE_ESD_CHECK_COMMAND_BY_FIRMWARE
/* Method 1. Require the new ESD check command(CmdId:0x55) support from
 * firmware which is currently implemented for MSG22XX only.
 * So default is not supported.
 */
    sz_tx_data[0] = 0x55;
    sz_tx_data[1] = 0xAA;
    sz_tx_data[2] = 0x55;

    mutex_lock(&g_mutex);

    retW = iic_write_data(slave_i2c_id_dw_i2c, &sz_tx_data[0], 3);
    retR = iic_read_data(slave_i2c_id_dw_i2c, &sz_rx_data[0], 8);

    mutex_unlock(&g_mutex);

    DBG(&g_i2c_client->dev,
        "sz_rx_data[] : 0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x  0x%x\n",
        sz_rx_data[0], sz_rx_data[1], sz_rx_data[2], sz_rx_data[3], sz_rx_data[4],
        sz_rx_data[5], sz_rx_data[6], sz_rx_data[7]);

    DBG(&g_i2c_client->dev, "retW = %d, retR = %d\n", retW, retR);

    if (retW > 0 && retR > 0) {
        while (i < 8) {
            if (szEsdCheckValue[i] != sz_rx_data[i])
                break;

            i++;
        }

        if (i == 8) {
            if (sz_rx_data[0] == 0 && sz_rx_data[1] == 0 && sz_rx_data[2] == 0
                && sz_rx_data[3] == 0 && sz_rx_data[4] == 0 && sz_rx_data[5] == 0 &&
                sz_rx_data[6] == 0 && sz_rx_data[7] == 0) {
                DBG(&g_i2c_client->dev,
                    "Firmware not support ESD check command.\n");
            } else {
                DBG(&g_i2c_client->dev, "ESD check failed case1.\n");

                drv_touch_device_hw_reset();
            }
        } else {
            DBG(&g_i2c_client->dev, "ESD check success.\n");
        }

        for (i = 0; i < 8; i++)
            szEsdCheckValue[i] = sz_rx_data[i];

    } else {
        DBG(&g_i2c_client->dev, "ESD check failed case2.\n");

        drv_touch_device_hw_reset();
    }
#else
/* Method 2. Use I2C write command for checking whether I2C
* connection is still available under ESD testing.
*/

    sz_data[0] = 0x00;           /*Dummy command for ESD check */

    mutex_lock(&g_mutex);

    while (i < 3) {
        mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
        if (g_chip_type == CHIP_TYPE_MSG22XX || g_chip_type == CHIP_TYPE_MSG28XX
            || g_chip_type == CHIP_TYPE_MSG58XXA
            || g_chip_type == CHIP_TYPE_ILI2118A ||
            g_chip_type == CHIP_TYPE_ILI2117A) {
            rc = iic_write_data(slave_i2c_id_dw_i2c, &sz_data[0], 1);
        } else {
            DBG(&g_i2c_client->dev, "Un-recognized chip type = 0x%x\n",
                g_chip_type);
            break;
        }

        if (rc > 0) {
            DBG(&g_i2c_client->dev, "ESD check success\n");
            break;
        }

        i++;
    }
    if (i == 3)
        DBG(&g_i2c_client->dev, "ESD check failed, rc = %d\n", rc);

    mutex_unlock(&g_mutex);

    if (i >= 3)
        drv_touch_device_hw_reset();

#endif /*CONFIG_ENABLE_ESD_CHECK_COMMAND_BY_FIRMWARE */

EsdCheckEnd:

    if (g_is_enable_esd_check == 1) {
        queue_delayed_work(g_esd_check_work_queue, &g_esd_check_work,
                           ESD_PROTECT_CHECK_PERIOD);
    }
}
#endif /*CONFIG_ENABLE_ESD_PROTECTION */

/*---------------------------------------------------------------------------*/
static void drv_variable_initialize(void)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    if (g_chip_type == CHIP_TYPE_MSG22XX) {

/*firmware_mode_unknown_mode = MSG22XX_FIRMWARE_MODE_UNKNOWN_MODE;*/
        firmware_mode_demo_mode = MSG22XX_FIRMWARE_MODE_DEMO_MODE;
        firmware_mode_debug_mode = MSG22XX_FIRMWARE_MODE_DEBUG_MODE;
        firmware_mode_raw_data_mode = MSG22XX_FIRMWARE_MODE_RAW_DATA_MODE;

        g_firmware_mode = firmware_mode_demo_mode;

        demo_mode_packet_length = SELF_DEMO_MODE_PACKET_LENGTH;
        debug_mode_packet_length = SELF_DEBUG_MODE_PACKET_LENGTH;
        max_touch_num = SELF_MAX_TOUCH_NUM;
    } else if (g_chip_type == CHIP_TYPE_MSG28XX
               || g_chip_type == CHIP_TYPE_MSG58XXA
               || g_chip_type == CHIP_TYPE_ILI2118A ||
               g_chip_type == CHIP_TYPE_ILI2117A) {
        firmware_mode_unknown_mode = MSG28XX_FIRMWARE_MODE_UNKNOWN_MODE;
        firmware_mode_demo_mode = MSG28XX_FIRMWARE_MODE_DEMO_MODE;
        firmware_mode_debug_mode = MSG28XX_FIRMWARE_MODE_DEBUG_MODE;

        g_firmware_mode = firmware_mode_demo_mode;

        demo_mode_packet_length = MUTUAL_DEMO_MODE_PACKET_LENGTH;
        debug_mode_packet_length = MUTUAL_DEBUG_MODE_PACKET_LENGTH;
        max_touch_num = MUTUAL_MAX_TOUCH_NUM;
    } else if (g_chip_type == CHIP_TYPE_ILI2120 ||
               g_chip_type == CHIP_TYPE_ILI2121) {
        firmware_mode_unknown_mode = ILI21XX_FIRMWARE_MODE_UNKNOWN_MODE;
        firmware_mode_demo_mode = ILI21XX_FIRMWARE_MODE_DEMO_MODE;
        firmware_mode_debug_mode = ILI21XX_FIRMWARE_MODE_DEBUG_MODE;

        g_firmware_mode = firmware_mode_demo_mode;

        demo_mode_packet_length = ILI21XX_DEMO_MODE_PACKET_LENGTH;
        debug_mode_packet_length = MUTUAL_DEBUG_MODE_PACKET_LENGTH;
        max_touch_num = MUTUAL_MAX_TOUCH_NUM; /* g_ilitek_tp_info.n_max_touch_num */
    }
}

void drv_get_customer_firmware_version(u16 *pMajor, u16 *pMinor, u8 **ppVersion)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    if (g_chip_type == CHIP_TYPE_MSG22XX) {
        u16 nReg_data1, nReg_data2;

        mutex_lock(&g_mutex);

        /*drv_touch_device_hw_reset(); */

        db_bus_enter_serial_debug_mode();
        db_bus_stop_mcu();
        db_bus_iic_use_bus();
        db_bus_iic_reshape();

#ifdef CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K
        /*Stop MCU */
        reg_set_l_byte_value(0x0FE6, 0x01);

        /*Exit flash low power mode */
        reg_set_l_byte_value(0x1619, BIT1);

        /*Change PIU clock to 48MHz */
        reg_set_l_byte_value(0x1E23, BIT6);

        /*Change mcu clock deglitch mux source */
        reg_set_l_byte_value(0x1E54, BIT0);
#else
        /*Stop MCU */
        reg_set_l_byte_value(0x0FE6, 0x01);
#endif /*CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K */

        /*RIU password */
        reg_set_16bit_value(0x161A, 0xABBA);

        reg_set_16bit_value(0x1600, 0xBFF4);
        /*Set start address for customer firmware version on main block */

        /*Enable burst mode */

/*reg_set_16bit_value(0x160C, (reg_get16_bit_value(0x160C) | 0x01));*/

        reg_set_l_byte_value(0x160E, 0x01);

        nReg_data1 = reg_get16_bit_value(0x1604);
        nReg_data2 = reg_get16_bit_value(0x1606);

        *pMajor = (((nReg_data1 >> 8) & 0xFF) << 8) + (nReg_data1 & 0xFF);
        *pMinor = (((nReg_data2 >> 8) & 0xFF) << 8) + (nReg_data2 & 0xFF);

        /*Clear burst mode */

/*reg_set_16bit_value(0x160C, reg_get16_bit_value(0x160C) & (~0x01));*/

        reg_set_16bit_value(0x1600, 0x0000);

        /*Clear RIU password */
        reg_set_16bit_value(0x161A, 0x0000);

        db_bus_iic_not_use_bus();
        db_bus_not_stop_mcu();
        db_bus_exit_serial_debug_mode();

        drv_touch_device_hw_reset();

        mutex_unlock(&g_mutex);
    } else if (g_chip_type == CHIP_TYPE_MSG28XX
               || g_chip_type == CHIP_TYPE_MSG58XXA
               || g_chip_type == CHIP_TYPE_ILI2118A ||
               g_chip_type == CHIP_TYPE_ILI2117A) {
        u8 sz_tx_data[3] = { 0 };
        u8 sz_rx_data[4] = { 0 };

        sz_tx_data[0] = 0x03;

        mutex_lock(&g_mutex);

        /*drv_touch_device_hw_reset(); */

        iic_write_data(slave_i2c_id_dw_i2c, &sz_tx_data[0], 1);
        mdelay(I2C_SMBUS_READ_COMMAND_DELAY_FOR_PLATFORM);
        iic_read_data(slave_i2c_id_dw_i2c, &sz_rx_data[0], 4);

        mutex_unlock(&g_mutex);

        DBG(&g_i2c_client->dev, "sz_rx_data[0] = 0x%x\n", sz_rx_data[0]);
        DBG(&g_i2c_client->dev, "sz_rx_data[1] = 0x%x\n", sz_rx_data[1]);
        DBG(&g_i2c_client->dev, "sz_rx_data[2] = 0x%x\n", sz_rx_data[2]);
        DBG(&g_i2c_client->dev, "sz_rx_data[3] = 0x%x\n", sz_rx_data[3]);

        *pMajor = (sz_rx_data[1] << 8) + sz_rx_data[0];
        *pMinor = (sz_rx_data[3] << 8) + sz_rx_data[2];
    }
    if (*ppVersion == NULL)
		*ppVersion = kzalloc(sizeof(u8) * 12, GFP_KERNEL);

    DBG(&g_i2c_client->dev, "*** Major = %d ***\n", *pMajor);
    if (g_fw_version_flag) {
        DBG(&g_i2c_client->dev, "*** Minor = %d.%d ***\n", (*pMinor & 0xFF),
            ((*pMinor >> 8) & 0xFF));
    } else {
        DBG(&g_i2c_client->dev, "*** Minor = %d ***\n", *pMinor);
    }
    snprintf(*ppVersion, 12, "%05d.%05d", *pMajor, *pMinor);
}

void drv_get_platform_firmware_version(u8 **ppVersion)
{
    int ret = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    if (g_chip_type == CHIP_TYPE_MSG22XX) {
        u32 i;
        u16 nReg_data1, nReg_data2;
        u8 sz_rx_data[12] = { 0 };

        mutex_lock(&g_mutex);

        /*drv_touch_device_hw_reset(); */

        db_bus_enter_serial_debug_mode();
        db_bus_stop_mcu();
        db_bus_iic_use_bus();
        db_bus_iic_reshape();

#ifdef CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K
        /*Stop MCU */
        reg_set_l_byte_value(0x0FE6, 0x01);

        /*Exit flash low power mode */
        reg_set_l_byte_value(0x1619, BIT1);

        /*Change PIU clock to 48MHz */
        reg_set_l_byte_value(0x1E23, BIT6);

        /*Change mcu clock deglitch mux source */
        reg_set_l_byte_value(0x1E54, BIT0);
#else
        /*Stop MCU */
        reg_set_l_byte_value(0x0FE6, 0x01);
#endif /*CONFIG_ENABLE_UPDATE_FIRMWARE_WITH_SUPPORT_I2C_SPEED_400K */

        /*RIU password */
        reg_set_16bit_value(0x161A, 0xABBA);

        reg_set_16bit_value(0x1600, 0xC1F2);
        /*Set start address for platform firmware version on info block
        *(Actually, start reading from 0xC1F0)
        */

        /*Enable burst mode */
        reg_set_16bit_value(0x160C, (reg_get16_bit_value(0x160C) | 0x01));

        for (i = 0; i < 3; i++) {
            reg_set_l_byte_value(0x160E, 0x01);

            nReg_data1 = reg_get16_bit_value(0x1604);
            nReg_data2 = reg_get16_bit_value(0x1606);

            sz_rx_data[i * 4 + 0] = (nReg_data1 & 0xFF);
            sz_rx_data[i * 4 + 1] = ((nReg_data1 >> 8) & 0xFF);

            sz_rx_data[i * 4 + 2] = (nReg_data2 & 0xFF);
            sz_rx_data[i * 4 + 3] = ((nReg_data2 >> 8) & 0xFF);
        }

        /*Clear burst mode */
        reg_set_16bit_value(0x160C, reg_get16_bit_value(0x160C) & (~0x01));

        reg_set_16bit_value(0x1600, 0x0000);

        /*Clear RIU password */
        reg_set_16bit_value(0x161A, 0x0000);

        if (*ppVersion == NULL)
            *ppVersion = kzalloc(sizeof(u8) * 16, GFP_KERNEL);


		snprintf(*ppVersion, 256, "%c%c%c%c%c%c%c%c%c%c", sz_rx_data[2], sz_rx_data[3],
                sz_rx_data[4], sz_rx_data[5], sz_rx_data[6], sz_rx_data[7], sz_rx_data[8],
                sz_rx_data[9], sz_rx_data[10], sz_rx_data[11]);

        db_bus_iic_not_use_bus();
        db_bus_not_stop_mcu();
        db_bus_exit_serial_debug_mode();

        drv_touch_device_hw_reset();

        mutex_unlock(&g_mutex);
    } else if (g_chip_type == CHIP_TYPE_MSG28XX
               || g_chip_type == CHIP_TYPE_MSG58XXA
               || g_chip_type == CHIP_TYPE_ILI2118A ||
               g_chip_type == CHIP_TYPE_ILI2117A) {
        u8 sz_tx_data[1] = { 0 };
        u8 sz_rx_data[10] = { 0 };

        sz_tx_data[0] = 0x0C;

        mutex_lock(&g_mutex);

        /*drv_touch_device_hw_reset(); */

#ifdef CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM
#ifdef CONFIG_ENABLE_DMA_IIC
        dma_reset();
#endif /*CONFIG_ENABLE_DMA_IIC */
#endif /*CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM */

        iic_write_data(slave_i2c_id_dw_i2c, &sz_tx_data[0], 1);
        mdelay(I2C_SMBUS_READ_COMMAND_DELAY_FOR_PLATFORM);
        iic_read_data(slave_i2c_id_dw_i2c, &sz_rx_data[0], 10);

        mutex_unlock(&g_mutex);

        DBG(&g_i2c_client->dev, "sz_rx_data[0] = 0x%x , %c\n",
                                                sz_rx_data[0], sz_rx_data[0]);
        DBG(&g_i2c_client->dev, "sz_rx_data[0] = 0x%x , %c\n",
                                                sz_rx_data[1], sz_rx_data[1]);
         DBG(&g_i2c_client->dev, "sz_rx_data[0] = 0x%x , %c\n",
                                                sz_rx_data[2], sz_rx_data[2]);
         DBG(&g_i2c_client->dev, "sz_rx_data[0] = 0x%x , %c\n",
                                                sz_rx_data[3], sz_rx_data[3]);
         DBG(&g_i2c_client->dev, "sz_rx_data[0] = 0x%x , %c\n",
                                                sz_rx_data[4], sz_rx_data[4]);
         DBG(&g_i2c_client->dev, "sz_rx_data[0] = 0x%x , %c\n",
                                                sz_rx_data[5], sz_rx_data[5]);
         DBG(&g_i2c_client->dev, "sz_rx_data[0] = 0x%x , %c\n",
                                                sz_rx_data[6], sz_rx_data[6]);
         DBG(&g_i2c_client->dev, "sz_rx_data[0] = 0x%x , %c\n",
                                                sz_rx_data[7], sz_rx_data[7]);
         DBG(&g_i2c_client->dev, "sz_rx_data[0] = 0x%x , %c\n",
                                                sz_rx_data[8], sz_rx_data[8]);
         DBG(&g_i2c_client->dev, "sz_rx_data[0] = 0x%x , %c\n",
                                                sz_rx_data[9], sz_rx_data[9]);

        if (*ppVersion == NULL)
            *ppVersion = kzalloc(sizeof(u8) * 16, GFP_KERNEL);

		snprintf(*ppVersion, 128, "%.10s", sz_rx_data);
        ret = sscanf(*ppVersion, "V%u.%u.%u\n", &ga_platform_fw_version[0],
               &ga_platform_fw_version[1], &ga_platform_fw_version[2]);

        if (ret < 0) {
            DBG(0, "error\n");
            return;
        }
        DBG(0, "*** %d, ILITEK %s() Platform FW version= %02u.%03u.%02u ***\n",
               __LINE__, __func__, ga_platform_fw_version[0],
               ga_platform_fw_version[1], ga_platform_fw_version[2]);
        /*platform FW version V01.010.03 is demarcation point */
        if (ga_platform_fw_version[0] * 100000 +
            (ga_platform_fw_version[1]) * 100 + ga_platform_fw_version[2] >=
            101003) {
            g_fw_version_flag = 1;
        }
    } else {
        if (*ppVersion == NULL)
            *ppVersion = kzalloc(sizeof(u8) * 10, GFP_KERNEL);

		snprintf(*ppVersion, 10, "%s", "N/A");
    }

    DBG(&g_i2c_client->dev, "***platform firmware version = %s ***\n",
        *ppVersion);
}

static s32 drv_update_firmware(u8 szFw_data[][1024], enum emem_type_e eEmemType)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    return drv_update_firmware_cash(szFw_data, eEmemType);
}

s32 drv_check_update_firmware_by_sd_card(const char *p_file_path)
{
    s32 n_ret_val = -1;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    if (g_chip_type == CHIP_TYPE_MSG22XX || g_chip_type == CHIP_TYPE_MSG28XX
        || g_chip_type == CHIP_TYPE_MSG58XXA || g_chip_type == CHIP_TYPE_ILI2118A
        || g_chip_type == CHIP_TYPE_ILI2117A) {
        n_ret_val = drv_update_firmware_by_sd_card(p_file_path);
    } else {
        DBG(&g_i2c_client->dev,
            "This chip type (0x%x) does not support update firmware by sd card\n",
            g_chip_type);
    }
    drv_touch_device_hw_reset();
    return n_ret_val;
}

/*---------------------------------------------------------------------------*/
static u16 drv_change_firmware_mode(u16 nMode)
{
    DBG(&g_i2c_client->dev, "*** %s() *** nMode = 0x%x\n", __func__, nMode);

    if (g_chip_type == CHIP_TYPE_MSG22XX || g_chip_type == CHIP_TYPE_MSG28XX
        || g_chip_type == CHIP_TYPE_MSG58XXA || g_chip_type == CHIP_TYPE_ILI2118A
        || g_chip_type == CHIP_TYPE_ILI2117A || g_chip_type == CHIP_TYPE_ILI2120
        || g_chip_type == CHIP_TYPE_ILI2121) {
        u8 sz_tx_data[2] = { 0 };
        u32 i = 0;
        s32 rc;

        g_is_disable_finger_touch = 1;
       /*Skip finger touch ISR handling temporarily for device driver can send
       * change firmware mode i2c command to firmware.
       */

        if (g_chip_type == CHIP_TYPE_ILI2120 || g_chip_type == CHIP_TYPE_ILI2121)
            sz_tx_data[0] = 0xD1;
        else
            sz_tx_data[0] = 0x02;

        sz_tx_data[1] = (u8) nMode;

        mutex_lock(&g_mutex);
        DBG(&g_i2c_client->dev,
            "*** %s() *** mutex_lock(&g_mutex)\n", __func__);
        /*add for debug */

        while (i < 5) {
            mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
            rc = iic_write_data(slave_i2c_id_dw_i2c, &sz_tx_data[0], 2);
            if (rc > 0) {
                DBG(&g_i2c_client->dev, "Change firmware mode success\n");
                break;
            }

            i++;
        }
        if (i == 5) {
            DBG(&g_i2c_client->dev, "Change firmware mode failed, rc = %d\n",
                rc);
        }

        DBG(&g_i2c_client->dev,
            "*** %s() *** mutex_unlock(&g_mutex)\n", __func__);
        /*add for debug */
        mutex_unlock(&g_mutex);

        g_is_disable_finger_touch = 0;
        /*Resume finger touch ISR handling after device
        * driver have sent i2c command to firmware.
        */
    }

    return nMode;
}

static void drv_self_get_firmware_info(struct self_firmware_info_t *p_info)
{                               /*for MSG22xx */
    u8 sz_tx_data[1] = { 0 };
    u8 sz_rx_data[8] = { 0 };
    u32 i = 0;
    s32 rc;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    g_is_disable_finger_touch = 1;
    /*Skip finger touch ISR handling temporarily for device driver can
    * send get firmware info i2c command to firmware.
    */

    sz_tx_data[0] = 0x01;

    mutex_lock(&g_mutex);

    while (i < 5) {
        mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
        rc = iic_write_data(slave_i2c_id_dw_i2c, &sz_tx_data[0], 1);
        if (rc > 0) {
            DBG(&g_i2c_client->dev,
                "Get firmware info iic_write_data() success\n");
        }
        mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
        rc = iic_read_data(slave_i2c_id_dw_i2c, &sz_rx_data[0], 8);
        if (rc > 0) {
            DBG(&g_i2c_client->dev,
                "Get firmware info iic_read_data() success\n");
            break;
        }

        i++;
    }
    if (i == 5)
        DBG(&g_i2c_client->dev, "Get firmware info failed, rc = %d\n", rc);

    mutex_unlock(&g_mutex);

    if ((sz_rx_data[1] & 0x80) == 0x80)
        p_info->n_ls_can_change_firmware_mode = 0;
    else
        p_info->n_ls_can_change_firmware_mode = 1;

    p_info->n_firmware_mode = sz_rx_data[1] & 0x7F;
    p_info->n_log_mode_packet_header = sz_rx_data[2];
    p_info->n_log_mode_packet_length = (sz_rx_data[3] << 8) + sz_rx_data[4];

    DBG(&g_i2c_client->dev,
        "p_info->n_firmware_mode=0x%x, p_info->n_log_mode_packet_header=0x%x, p_info->n_log_mode_packet_length=%d, p_info->n_ls_can_change_firmware_mode=%d\n",
        p_info->n_firmware_mode, p_info->n_log_mode_packet_header,
        p_info->n_log_mode_packet_length, p_info->n_ls_can_change_firmware_mode);

    g_is_disable_finger_touch = 0;
    /*Resume finger touch ISR handling after device driver
    * have sent i2c command to firmware.
    */
}

static void drv_mutual_get_firmware_info(struct mutual_firmware_info_t *p_info)
{                               /*for MSG28xx/MSG58xxA/ILI21xx */
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    if (g_chip_type == CHIP_TYPE_MSG28XX ||
        g_chip_type == CHIP_TYPE_MSG58XXA ||
        g_chip_type == CHIP_TYPE_ILI2118A ||
        g_chip_type == CHIP_TYPE_ILI2117A ||
        g_chip_type == CHIP_TYPE_ILI2120 ||
        g_chip_type == CHIP_TYPE_ILI2121) {
        u8 sz_tx_data[1] = { 0 };
        u8 sz_rx_data[10] = { 0 };
        u32 i = 0;
        s32 rc;

        g_is_disable_finger_touch = 1;
        /*Skip finger touch ISR handling temporarily for device driver can
        * send get firmware info i2c command to firmware.
        */

        if (g_chip_type == CHIP_TYPE_ILI2120 || g_chip_type == CHIP_TYPE_ILI2121)
            sz_tx_data[0] = 0xD0;
        else
            sz_tx_data[0] = 0x01;

        mutex_lock(&g_mutex);
        DBG(&g_i2c_client->dev,
        "*** %s() *** mutex_lock(&g_mutex)\n", __func__);   /*add for debug */

#ifdef CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM
#ifdef CONFIG_ENABLE_DMA_IIC
        dma_reset();
#endif /*CONFIG_ENABLE_DMA_IIC */
#endif /*CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM */

        while (i < 5) {
            mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
            rc = iic_write_data(slave_i2c_id_dw_i2c, &sz_tx_data[0], 1);
            if (rc > 0) {
                DBG(&g_i2c_client->dev,
                    "Get firmware info iic_write_data() success\n");
            }

            mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
            rc = iic_read_data(slave_i2c_id_dw_i2c, &sz_rx_data[0], 10);
            if (rc > 0) {
                DBG(&g_i2c_client->dev,
                    "Get firmware info iic_read_data() success\n");

                if (sz_rx_data[1] == firmware_mode_demo_mode ||
                    sz_rx_data[1] == firmware_mode_debug_mode) {
                    break;
                }

				i = 0;
            }

            i++;
        }
        if (i == 5)
            DBG(&g_i2c_client->dev, "Get firmware info failed, rc = %d\n", rc);

        DBG(&g_i2c_client->dev,
        "*** %s() *** mutex_unlock(&g_mutex)\n", __func__); /*add for debug */
        mutex_unlock(&g_mutex);

        /*Add protection for incorrect firmware info check */
        if ((sz_rx_data[1] == firmware_mode_debug_mode && sz_rx_data[2] == 0xA7 &&
             sz_rx_data[5] == PACKET_TYPE_TOOTH_PATTERN) ||
             (sz_rx_data[1] == firmware_mode_demo_mode && sz_rx_data[2] == 0x5A)) {
            p_info->n_firmware_mode = sz_rx_data[1];
            DBG(&g_i2c_client->dev, "p_info->n_firmware_mode = 0x%x\n",
                p_info->n_firmware_mode);

            p_info->n_log_mode_packet_header = sz_rx_data[2];
            p_info->n_log_mode_packet_length = (sz_rx_data[3] << 8) + sz_rx_data[4];
            p_info->n_type = sz_rx_data[5];
            p_info->n_my = sz_rx_data[6];
            p_info->n_mx = sz_rx_data[7];
            p_info->n_sd = sz_rx_data[8];
            p_info->n_ss = sz_rx_data[9];

            DBG(&g_i2c_client->dev, "p_info->n_log_mode_packet_header = 0x%x\n",
                p_info->n_log_mode_packet_header);
            DBG(&g_i2c_client->dev, "p_info->n_log_mode_packet_length = %d\n",
                p_info->n_log_mode_packet_length);
            DBG(&g_i2c_client->dev, "p_info->n_type = 0x%x\n", p_info->n_type);
            DBG(&g_i2c_client->dev, "p_info->n_my = %d\n", p_info->n_my);
            DBG(&g_i2c_client->dev, "p_info->n_mx = %d\n", p_info->n_mx);
            DBG(&g_i2c_client->dev, "p_info->n_sd = %d\n", p_info->n_sd);
            DBG(&g_i2c_client->dev, "p_info->n_ss = %d\n", p_info->n_ss);
        } else {
            DBG(&g_i2c_client->dev, "Firmware info before correcting :\n");

            DBG(&g_i2c_client->dev, "FirmwareMode = 0x%x\n", sz_rx_data[1]);
            DBG(&g_i2c_client->dev, "LogModePacketHeader = 0x%x\n",
                sz_rx_data[2]);
            DBG(&g_i2c_client->dev, "LogModePacketLength = %d\n",
                (sz_rx_data[3] << 8) + sz_rx_data[4]);
            DBG(&g_i2c_client->dev, "Type = 0x%x\n", sz_rx_data[5]);
            DBG(&g_i2c_client->dev, "My = %d\n", sz_rx_data[6]);
            DBG(&g_i2c_client->dev, "Mx = %d\n", sz_rx_data[7]);
            DBG(&g_i2c_client->dev, "Sd = %d\n", sz_rx_data[8]);
            DBG(&g_i2c_client->dev, "Ss = %d\n", sz_rx_data[9]);

            /*Set firmware mode to demo mode(default) */
            p_info->n_firmware_mode = firmware_mode_demo_mode;
            p_info->n_log_mode_packet_header = 0x5A;
            p_info->n_log_mode_packet_length = demo_mode_packet_length;
            p_info->n_type = 0;
            p_info->n_my = 0;
            p_info->n_mx = 0;
            p_info->n_sd = 0;
            p_info->n_ss = 0;

            DBG(&g_i2c_client->dev, "Firmware info after correcting :\n");

            DBG(&g_i2c_client->dev, "p_info->n_firmware_mode = 0x%x\n",
                p_info->n_firmware_mode);
            DBG(&g_i2c_client->dev, "p_info->n_log_mode_packet_header = 0x%x\n",
                p_info->n_log_mode_packet_header);
            DBG(&g_i2c_client->dev, "p_info->n_log_mode_packet_length = %d\n",
                p_info->n_log_mode_packet_length);
            DBG(&g_i2c_client->dev, "p_info->n_type = 0x%x\n", p_info->n_type);
            DBG(&g_i2c_client->dev, "p_info->n_my = %d\n", p_info->n_my);
            DBG(&g_i2c_client->dev, "p_info->n_mx = %d\n", p_info->n_mx);
            DBG(&g_i2c_client->dev, "p_info->n_sd = %d\n", p_info->n_sd);
            DBG(&g_i2c_client->dev, "p_info->n_ss = %d\n", p_info->n_ss);
        }

        g_is_disable_finger_touch = 0;
        /*Resume finger touch ISR handling after device driver
        * have sent i2c command to firmware.
        */
    }
}

void drv_restore_firmware_mode_to_log_data_mode(void)
{
    DBG(&g_i2c_client->dev,
    "*** %s() g_is_switch_mode_by_apk = %d ***\n", __func__, g_is_switch_mode_by_apk);

    if (g_chip_type == CHIP_TYPE_MSG22XX) {
        if (g_is_switch_mode_by_apk == 1) {
            struct self_firmware_info_t tInfo;

            memset(&tInfo, 0x0, sizeof(struct self_firmware_info_t));

            drv_self_get_firmware_info(&tInfo);

            DBG(&g_i2c_client->dev,
                "g_firmware_mode = 0x%x, tInfo.n_firmware_mode = 0x%x\n",
                g_firmware_mode, tInfo.n_firmware_mode);

            /*Since reset_hw() will reset the firmware mode to demo mode,
            * we must reset the firmware mode again after reset_hw().
            */
            if (g_firmware_mode == firmware_mode_debug_mode &&
                firmware_mode_debug_mode !=
                tInfo.n_firmware_mode) {
                g_firmware_mode =
                    drv_change_firmware_mode(firmware_mode_debug_mode);
            } else if (g_firmware_mode == firmware_mode_raw_data_mode &&
                       firmware_mode_raw_data_mode !=
                       tInfo.n_firmware_mode) {
                g_firmware_mode =
                    drv_change_firmware_mode(firmware_mode_raw_data_mode);
            } else {
                DBG(&g_i2c_client->dev, "firmware mode is not restored\n");
            }
        }
    } else if (g_chip_type == CHIP_TYPE_MSG28XX
               || g_chip_type == CHIP_TYPE_MSG58XXA
               || g_chip_type == CHIP_TYPE_ILI2118A
               || g_chip_type == CHIP_TYPE_ILI2117A
               || g_chip_type == CHIP_TYPE_ILI2120 ||
               g_chip_type == CHIP_TYPE_ILI2121) {
        if (g_is_switch_mode_by_apk == 1) {
            struct mutual_firmware_info_t tInfo;

            memset(&tInfo, 0x0, sizeof(struct mutual_firmware_info_t));

            drv_mutual_get_firmware_info(&tInfo);

            DBG(&g_i2c_client->dev,
                "g_firmware_mode = 0x%x, tInfo.n_firmware_mode = 0x%x\n",
                g_firmware_mode, tInfo.n_firmware_mode);

            /*Since reset_hw() will reset the firmware mode to demo mode,
            * we must reset the firmware mode again after reset_hw().
            */
            if (g_firmware_mode == firmware_mode_debug_mode &&
                firmware_mode_debug_mode !=
                tInfo.n_firmware_mode) {
                g_firmware_mode =
                    drv_change_firmware_mode(firmware_mode_debug_mode);
            } else {
                DBG(&g_i2c_client->dev, "firmware mode is not restored\n");
            }
        }
    }
}

/*---------------------------------------------------------------------------*/
#ifdef CONFIG_ENABLE_SEGMENT_READ_FINGER_TOUCH_DATA
static void drv_get_touch_packet_address(u16 *p_dataAddress, u16 *pFlagAddress)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    if (g_chip_type == CHIP_TYPE_MSG28XX || g_chip_type == CHIP_TYPE_MSG58XXA
        || g_chip_type == CHIP_TYPE_ILI2118A || g_chip_type == CHIP_TYPE_ILI2117A
        || g_chip_type == CHIP_TYPE_ILI2120 || g_chip_type == CHIP_TYPE_ILI2121
        || (g_chip_type == CHIP_TYPE_MSG22XX && is_self_freq_scan_enabled)) {
        s32 rc = 0;
        u32 i = 0;
        u8 sz_tx_data[1] = { 0 };
        u8 sz_rx_data[4] = { 0 };

        if (g_chip_type == CHIP_TYPE_ILI2120 || g_chip_type == CHIP_TYPE_ILI2121)
            sz_tx_data[0] = 0xD2;
        else
            sz_tx_data[0] = 0x05;


        mutex_lock(&g_mutex);
        DBG(&g_i2c_client->dev,
        "*** %s() *** mutex_lock(&g_mutex)\n", __func__);   /*add for debug */

        while (i < 5) {
            mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
            rc = iic_write_data(slave_i2c_id_dw_i2c, &sz_tx_data[0], 1);
            if (rc > 0) {
                DBG(&g_i2c_client->dev,
                    "Get touch packet address iic_write_data() success\n");
            }

            mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
            rc = iic_read_data(slave_i2c_id_dw_i2c, &sz_rx_data[0], 4);
            if (rc > 0) {
                DBG(&g_i2c_client->dev,
                    "Get touch packet address iic_read_data() success\n");
                break;
            }

            i++;
        }
        if (i == 5) {
            DBG(&g_i2c_client->dev,
                "Get touch packet address failed, rc = %d\n", rc);
        }

        if (rc < 0) {
            g_fw_support_segment = 0;
        } else {
            if (g_chip_type == CHIP_TYPE_MSG22XX) {
                *p_dataAddress = 0;
                *pFlagAddress = (sz_rx_data[0] << 8) + sz_rx_data[1];
            } else {
                *p_dataAddress = (sz_rx_data[0] << 8) + sz_rx_data[1];
                *pFlagAddress = (sz_rx_data[2] << 8) + sz_rx_data[3];
            }

            g_fw_support_segment = 1;

            DBG(&g_i2c_client->dev,
            "*** *p_dataAddress = 0x%2X ***\n", *p_dataAddress);/*add for debug*/
            DBG(&g_i2c_client->dev,
            "*** *pFlagAddress = 0x%2X ***\n", *pFlagAddress);/*add for debug*/
        }

        DBG(&g_i2c_client->dev,
        "*** %s() *** mutex_unlock(&g_mutex)\n", __func__); /*add for debug */
        mutex_unlock(&g_mutex);
    }
}

static int drv_check_finger_touch_packet_flag_bit1(void)
{
    u8 sz_tx_data[3] = { 0 };
    s32 n_ret_val;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    sz_tx_data[0] = 0x53;
    sz_tx_data[1] = (g_fw_packet_flag_address >> 8) & 0xFF;
    sz_tx_data[2] = g_fw_packet_flag_address & 0xFF;

    iic_write_data(slave_i2c_id_dw_i2c, &sz_tx_data[0], 3);
    iic_read_data(slave_i2c_id_dw_i2c, &g_touch_packet_flag[0], 2);

    if ((g_touch_packet_flag[0] & BIT1) == 0x00)
        n_ret_val = 0;            /*Bit1 is 0 */
    else
        n_ret_val = 1;            /*Bit1 is 1 */

    DBG(&g_i2c_client->dev, "Bit1 = %d\n", n_ret_val);

    return n_ret_val;
}

static void drv_reset_finger_touch_packet_flag_bit1(void)
{
    u8 sz_tx_data[4] = { 0 };

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    sz_tx_data[0] = 0x52;
    sz_tx_data[1] = (g_fw_packet_flag_address >> 8) & 0xFF;
    sz_tx_data[2] = g_fw_packet_flag_address & 0xFF;
    sz_tx_data[3] = g_touch_packet_flag[0] | BIT1;

    iic_write_data(slave_i2c_id_dw_i2c, &sz_tx_data[0], 4);
}
#endif /*CONFIG_ENABLE_SEGMENT_READ_FINGER_TOUCH_DATA */

/*---------------------------------------------------------------------------*/
static void drv_open_glove_mode(void)
{                               /*used for MSG28xx only */
    s32 rc = 0;
    u8 sz_tx_data[3] = { 0 };
    u32 i = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    g_is_disable_finger_touch = 1;
    /*Skip finger touch ISR handling temporarily for device driver can
    * send i2c command to firmware.
    */

    sz_tx_data[0] = 0x06;
    sz_tx_data[1] = 0x01;
    sz_tx_data[2] = 0x01;

    mutex_lock(&g_mutex);

    while (i < 5) {
        mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
        rc = iic_write_data(slave_i2c_id_dw_i2c, &sz_tx_data[0], 3);
        if (rc > 0) {
            g_is_enable_glove_mode = 1;

            DBG(&g_i2c_client->dev, "Open glove mode success\n");
            break;
        }

        i++;
    }
    if (i == 5)
        DBG(&g_i2c_client->dev, "Open glove mode failed, rc = %d\n", rc);


    mutex_unlock(&g_mutex);

    g_is_disable_finger_touch = 0;
    /*Resume finger touch ISR handling after device driver have
    * sent i2c command to firmware.
    */
}

static void drv_close_glove_mode(void)
{                               /*used for MSG28xx only */
    s32 rc = 0;
    u8 sz_tx_data[3] = { 0 };
    u32 i = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    g_is_disable_finger_touch = 1;
    /*Skip finger touch ISR handling temporarily for device driver can
    * send i2c command to firmware.
    */

    sz_tx_data[0] = 0x06;
    sz_tx_data[1] = 0x01;
    sz_tx_data[2] = 0x00;

    mutex_lock(&g_mutex);

    while (i < 5) {
        mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
        rc = iic_write_data(slave_i2c_id_dw_i2c, &sz_tx_data[0], 3);
        if (rc > 0) {
            g_is_enable_glove_mode = 0;

            DBG(&g_i2c_client->dev, "Close glove mode success\n");
            break;
        }

        i++;
    }
    if (i == 5)
        DBG(&g_i2c_client->dev, "Close glove mode failed, rc = %d\n", rc);


    mutex_unlock(&g_mutex);

    g_is_disable_finger_touch = 0;
    /*Resume finger touch ISR handling after device driver have
    * sent i2c command to firmware.
    */
}

static void drv_get_glove_info(u8 *pGloveMode)
{                               /*used for MSG28xx only */
    u8 sz_tx_data[3] = { 0 };
    u8 sz_rx_data[2] = { 0 };
    u32 i = 0;
    s32 rc;

    g_is_disable_finger_touch = 1;
    /*Skip finger touch ISR handling temporarily for device driver can
    * send i2c command to firmware.
    */

    sz_tx_data[0] = 0x06;
    sz_tx_data[1] = 0x01;
    sz_tx_data[2] = 0x02;

    mutex_lock(&g_mutex);

    while (i < 5) {
        mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
        rc = iic_write_data(slave_i2c_id_dw_i2c, &sz_tx_data[0], 3);
        if (rc > 0)
            DBG(&g_i2c_client->dev, "Get glove info iic_write_data() success\n");

        mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
        rc = iic_read_data(slave_i2c_id_dw_i2c, &sz_rx_data[0], 1);
        if (rc > 0) {
            DBG(&g_i2c_client->dev, "Get glove info iic_read_data() success\n");

            if (sz_rx_data[0] == 0x00 || sz_rx_data[0] == 0x01)
                break;
             else
                i = 0;

        }

        i++;
    }
    if (i == 5)
        DBG(&g_i2c_client->dev, "Get glove info failed, rc = %d\n", rc);


    mutex_unlock(&g_mutex);

    *pGloveMode = sz_rx_data[0];

    DBG(&g_i2c_client->dev, "*pGloveMode = 0x%x\n", *pGloveMode);

    g_is_disable_finger_touch = 0;
    /*Resume finger touch ISR handling after device driver have
    * sent i2c command to firmware.
    */
}

/*---------------------------------------------------------------------------*/
static void drv_open_leather_sheath_mode(void)
{                               /*used for MSG28xx only */
    s32 rc = 0;
    u8 sz_tx_data[3] = { 0 };
    u32 i = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    g_is_disable_finger_touch = 1;
    /*Skip finger touch ISR handling temporarily for device driver can
    * send i2c command to firmware.
    */

    sz_tx_data[0] = 0x06;
    sz_tx_data[1] = 0x02;
    sz_tx_data[2] = 0x01;

    mutex_lock(&g_mutex);

    while (i < 5) {
        mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
        rc = iic_write_data(slave_i2c_id_dw_i2c, &sz_tx_data[0], 3);
        if (rc > 0) {
            g_is_enable_leather_sheath_mode = 1;

            DBG(&g_i2c_client->dev, "Open leather sheath mode success\n");
            break;
        }

        i++;
    }
    if (i == 5)
        DBG(&g_i2c_client->dev, "Open leather sheath mode failed, rc = %d\n",
            rc);


    mutex_unlock(&g_mutex);

    g_is_disable_finger_touch = 0;
    /*Resume finger touch ISR handling after device driver have
    * sent i2c command to firmware.
    */
    }

static void drv_close_leather_sheath_mode(void)
{                               /*used for MSG28xx only */
    s32 rc = 0;
    u8 sz_tx_data[3] = { 0 };
    u32 i = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    g_is_disable_finger_touch = 1;
    /*Skip finger touch ISR handling temporarily for device driver can
    * send i2c command to firmware.
    */

    sz_tx_data[0] = 0x06;
    sz_tx_data[1] = 0x02;
    sz_tx_data[2] = 0x00;

    mutex_lock(&g_mutex);

    while (i < 5) {
        mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
        rc = iic_write_data(slave_i2c_id_dw_i2c, &sz_tx_data[0], 3);
        if (rc > 0) {
            g_is_enable_leather_sheath_mode = 0;

            DBG(&g_i2c_client->dev, "Close leather sheath mode success\n");
            break;
        }

        i++;
    }
    if (i == 5)
        DBG(&g_i2c_client->dev, "Close leather sheath mode failed, rc = %d\n",
            rc);


    mutex_unlock(&g_mutex);

    g_is_disable_finger_touch = 0;
   /*Resume finger touch ISR handling after device driver have
    * sent i2c command to firmware.
    */
}

static void drv_get_leather_sheath_info(u8 *pLeatherSheathMode)
{                               /*used for MSG28xx only */
    u8 sz_tx_data[3] = { 0 };
    u8 sz_rx_data[2] = { 0 };
    u32 i = 0;
    s32 rc;

    g_is_disable_finger_touch = 1;
    /*Skip finger touch ISR handling temporarily for device driver can
    * send i2c command to firmware.
    */

    sz_tx_data[0] = 0x06;
    sz_tx_data[1] = 0x02;
    sz_tx_data[2] = 0x02;

    mutex_lock(&g_mutex);

    while (i < 5) {
        mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
        rc = iic_write_data(slave_i2c_id_dw_i2c, &sz_tx_data[0], 3);
        if (rc > 0) {
            DBG(&g_i2c_client->dev,
                "Get leather sheath info iic_write_data() success\n");
        }

        mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
        rc = iic_read_data(slave_i2c_id_dw_i2c, &sz_rx_data[0], 1);
        if (rc > 0) {
            DBG(&g_i2c_client->dev,
                "Get leather sheath info iic_read_data() success\n");

            if (sz_rx_data[0] == 0x00 || sz_rx_data[0] == 0x01)
                break;

			i = 0;
        }

        i++;
    }
    if (i == 5) {
        DBG(&g_i2c_client->dev, "Get leather sheath info failed, rc = %d\n",
            rc);
    }

    mutex_unlock(&g_mutex);

    *pLeatherSheathMode = sz_rx_data[0];

    DBG(&g_i2c_client->dev, "*pLeatherSheathMode = 0x%x\n",
        *pLeatherSheathMode);

    g_is_disable_finger_touch = 0;
    /*Resume finger touch ISR handling after device driver have
    * sent i2c command to firmware.
    */
}

/*---------------------------------------------------------------------------*/
#ifdef CONFIG_ENABLE_CHARGER_DETECTION
void drv_charger_detection(u8 nChargerStatus)
{
    u32 i = 0;
    u8 sz_tx_data[2] = { 0 };
    s32 rc = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    DBG(&g_i2c_client->dev,
        "g_charger_plugin = %d, nChargerStatus = %d, g_force_update = %d\n",
        g_charger_plugin, nChargerStatus, g_force_update);

    if (g_chip_type == CHIP_TYPE_MSG28XX ||
        g_chip_type == CHIP_TYPE_MSG58XXA ||
        g_chip_type == CHIP_TYPE_ILI2118A ||
        g_chip_type == CHIP_TYPE_ILI2117A ||
        g_chip_type == CHIP_TYPE_ILI2120 ||
        g_chip_type == CHIP_TYPE_ILI2121) {
        if (g_chip_type == CHIP_TYPE_ILI2120 ||
            g_chip_type == CHIP_TYPE_ILI2121)
            sz_tx_data[0] = 0xD5;
        else
            sz_tx_data[0] = 0x09;

    }

    if (nChargerStatus) {       /*charger plug in */
        if (g_charger_plugin == 0 || g_force_update == 1) {
            sz_tx_data[1] = 0xA5;

            while (i < 5) {
                mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
                mutex_lock(&g_mutex);
                rc = iic_write_data(slave_i2c_id_dw_i2c, &sz_tx_data[0], 2);
                mutex_unlock(&g_mutex);
                if (rc > 0) {
                    g_charger_plugin = 1;

                    DBG(&g_i2c_client->dev,
                        "Update status for charger plug in success.\n");
                    break;
                }

                i++;
            }
            if (i == 5) {
                DBG(&g_i2c_client->dev,
                    "Update status for charger plug in failed, rc = %d\n", rc);
            }

            g_force_update = 0;
            /*Clear flag after force update charger status */
        }
    } else {                    /*charger plug out */

        if (g_charger_plugin == 1 || g_force_update == 1) {
            sz_tx_data[1] = 0x5A;

            while (i < 5) {
                mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
                mutex_lock(&g_mutex);
                rc = iic_write_data(slave_i2c_id_dw_i2c, &sz_tx_data[0], 2);
                mutex_unlock(&g_mutex);
                if (rc > 0) {
                    g_charger_plugin = 0;

                    DBG(&g_i2c_client->dev,
                        "Update status for charger plug out success.\n");
                    break;
                }

                i++;
            }
            if (i == 5) {
                DBG(&g_i2c_client->dev,
                   "Update status for charger plug out failed, rc = %d\n", rc);
            }

            g_force_update = 0; /*Clear flag after force update charger status */
        }
    }
}

static void drv_charger_plug_in_out_check(struct work_struct *p_work)
{
    u8 szChargerStatus[20] = { 0 };

    DBG(&g_i2c_client->dev,
        "*** %s() g_is_enable_charger_plugin_out_check = %d ***\n", __func__,
        g_is_enable_charger_plugin_out_check);

    if (g_is_enable_charger_plugin_out_check == 0)
        return;


    if (g_is_update_firmware != 0) {
        /*Check whether update frimware is finished */
        DBG(&g_i2c_client->dev,
            "Not allow to do charger plug in/out check while update firmware is proceeding.\n");
        goto ChargerPlugInOutCheckEnd;
    }
#ifdef CONFIG_ENABLE_ITO_MP_TEST
    if (g_is_in_mp_test == 1) { /*Check whether mp test is proceeding */
        DBG(&g_i2c_client->dev,
            "Not allow to do charger plug in/out check while mp test is proceeding.\n");
        goto ChargerPlugInOutCheckEnd;
    }
#endif /*CONFIG_ENABLE_ITO_MP_TEST */

    drv_read_file(POWER_SUPPLY_BATTERY_STATUS_PATCH, szChargerStatus, 20);

    DBG(&g_i2c_client->dev, "*** Battery Status : %s ***\n", szChargerStatus);

    if (strnstr(szChargerStatus, "Charging", 8) != NULL
		|| strnstr(szChargerStatus, "Full", 4) != NULL ||
		strnstr(szChargerStatus, "Fully charged", 13) != NULL) {
        drv_charger_detection(1); /*charger plug-in */
    } else {                    /*Not charging */

        drv_charger_detection(0); /*charger plug-out */
    }

ChargerPlugInOutCheckEnd:

    if (g_is_enable_charger_plugin_out_check == 1) {
        queue_delayed_work(g_charger_plug_in_out_check_workqueue,
                           &g_charger_plug_in_out_check_work,
                           CHARGER_DETECT_CHECK_PERIOD);
    }
}
#endif /*CONFIG_ENABLE_CHARGER_DETECTION */

/*---------------------------------------------------------------------------*/
static s32 drv_mutual_read_finger_touch_data(u8 *p_packet, u16 nReportPacketLength)
{
    s32 rc;

    if (is_firmware_data_log_enabled) {
        if (g_firmware_mode == firmware_mode_demo_mode) {
            rc = iic_read_data(slave_i2c_id_dw_i2c, &p_packet[0],
                             nReportPacketLength);
            if (rc < 0) {
                DBG(&g_i2c_client->dev,
                    "I2C read packet data failed, rc = %d\n", rc);
				return -EFAULT;
            }
        } else if (g_firmware_mode == firmware_mode_debug_mode) {
#ifdef CONFIG_ENABLE_SEGMENT_READ_FINGER_TOUCH_DATA
            DBG(&g_i2c_client->dev,
        "*** g_fw_packet_data_address = 0x%2X ***\n", g_fw_packet_data_address);
            DBG(&g_i2c_client->dev,
       "*** g_fw_packet_flag_address = 0x%2X ***\n", g_fw_packet_flag_address);

            if (g_fw_support_segment == 0) {
                DBG(&g_i2c_client->dev,
                    "g_fw_packet_data_address & g_fw_packet_flag_address is un-initialized\n");
				return -EFAULT;
            }

            if (g_is_disable_finger_touch == 1) {
                DBG(&g_i2c_client->dev,
                    "Skip finger touch for handling get firmware info or change firmware mode\n");
				return -EFAULT;
            }

            if (drv_check_finger_touch_packet_flag_bit1() != 0) {
                DBG(&g_i2c_client->dev,
                    "Bit1 is not 0. FW is not ready for providing debug mode packet to Device Driver\n");
				return -EFAULT;
            }

            rc = iic_segment_read_data_by_sm_bus(g_fw_packet_data_address,
                                           &p_packet[0], nReportPacketLength,
                                           MAX_I2C_TRANSACTION_LENGTH_LIMIT);

            drv_reset_finger_touch_packet_flag_bit1();

            if (rc < 0) {
                DBG(&g_i2c_client->dev,
                    "I2C read debug mode packet data failed, rc = %d\n", rc);
				return -EFAULT;
            }
#else

#if defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM)
            mdelay(I2C_SMBUS_READ_COMMAND_DELAY_FOR_PLATFORM);
#endif /*CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM */

            rc = iic_read_data(slave_i2c_id_dw_i2c, &p_packet[0],
                             nReportPacketLength);
            if (rc < 0) {
                DBG(&g_i2c_client->dev,
                    "I2C read packet data failed, rc = %d\n", rc);
				return -EFAULT;
            }
#endif /*CONFIG_ENABLE_SEGMENT_READ_FINGER_TOUCH_DATA */
        } else {
            DBG(&g_i2c_client->dev, "WRONG FIRMWARE MODE : 0x%x\n",
                g_firmware_mode);
			return -EFAULT;
        }
    } else {
        rc = iic_read_data(slave_i2c_id_dw_i2c, &p_packet[0], nReportPacketLength);
        if (rc < 0) {
            DBG(&g_i2c_client->dev, "I2C read packet data failed, rc = %d\n",
                rc);
			return -EFAULT;
        }
    }                           /*is_firmware_data_log_enabled */

    return 0;
}

/*---------------------------------------------------------------------------*/
    s32 drv_ilitek_read_touch_panelInfo(struct ilitek_tp_info_t *p_tp_info)
{
    u32 i = 0;
    u8 sz_write_buf[2] = { 0 };
    u8 szReadBuf[64] = { 0 };
    s32 rc = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

#ifdef CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM
#ifdef CONFIG_ENABLE_DMA_IIC
    dma_reset();
#endif /*CONFIG_ENABLE_DMA_IIC */
#endif /*CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM */

    mutex_lock(&g_mutex);

    if (g_chip_type == CHIP_TYPE_ILI2120) {
        for (i = 0; i < 20; i++) {
            sz_write_buf[0] = ILITEK_TP_CMD_READ_DATA;
            rc = iic_write_data(slave_i2c_id_dw_i2c, &sz_write_buf[0], 1);
            if (rc < 0) {
                DBG(&g_i2c_client->dev,
                    "ILITEK_TP_CMD_READ_DATA : iic_write_data() failed, rc = %d\n",
                    rc);
            }

            mdelay(10);

            rc = iic_read_data(slave_i2c_id_dw_i2c, &szReadBuf[0], 3);
            if (rc < 0) {
                DBG(&g_i2c_client->dev,
                    "ILITEK_TP_CMD_READ_DATA : iic_read_data() failed, rc = %d\n",
                    rc);
            }

            DBG(&g_i2c_client->dev,
                "Check FW status : szReadBuf[0][1][2] = 0x%X, 0x%X, 0x%X\n",
                szReadBuf[0], szReadBuf[1], szReadBuf[2]);

            if (szReadBuf[1] >= 0x80) {
                DBG(&g_i2c_client->dev, "FW is ready\n");
                break;
            } else {
                mdelay(50);
            }
        }

        /*read firmware version */
        sz_write_buf[0] = ILITEK_TP_CMD_READ_DATA_CONTROL;
        sz_write_buf[1] = ILITEK_TP_ILI2120_CMD_GET_FIRMWARE_VERSION;
        rc = iic_write_data(slave_i2c_id_dw_i2c, &sz_write_buf[0], 2);
        if (rc < 0) {
            DBG(&g_i2c_client->dev,
            "ILITEK_TP_CMD_READ_DATA_CONTROL & ILITEK_TP_ILI2120_CMD_GET_FIRMWARE_VERSION : iic_write_data() failed, rc = %d\n", rc);
			return -EFAULT;
        }

        mdelay(10);

        sz_write_buf[0] = ILITEK_TP_ILI2120_CMD_GET_FIRMWARE_VERSION;
        rc = iic_write_data(slave_i2c_id_dw_i2c, &sz_write_buf[0], 1);
        if (rc < 0) {
            DBG(&g_i2c_client->dev,
           "ILITEK_TP_ILI2120_CMD_GET_FIRMWARE_VERSION : iic_write_data() failed, rc = %d\n", rc);
			return -EFAULT;
        }

        mdelay(10);

        rc = iic_read_data(slave_i2c_id_dw_i2c, &szReadBuf[0], 3);
        if (rc < 0) {
            DBG(&g_i2c_client->dev,
            "ILITEK_TP_ILI2120_CMD_GET_FIRMWARE_VERSION : iic_read_data() failed, rc = %d\n", rc);
			return -EFAULT;
        }

        p_tp_info->sz_fw_version[0] = 0;

        for (i = 1; i < 4; i++)
            p_tp_info->sz_fw_version[i] = szReadBuf[i - 1];

        DBG(&g_i2c_client->dev, "Firmware Version = %d.%d.%d\n", szReadBuf[0],
            szReadBuf[1], szReadBuf[2]);

        /*read protocol version */
        sz_write_buf[0] = ILITEK_TP_CMD_READ_DATA_CONTROL;
        sz_write_buf[1] = ILITEK_TP_ILI2120_CMD_GET_PROTOCOL_VERSION;
        rc = iic_write_data(slave_i2c_id_dw_i2c, &sz_write_buf[0], 2);
        if (rc < 0) {
            DBG(&g_i2c_client->dev,
            "ILITEK_TP_CMD_READ_DATA_CONTROL & ILITEK_TP_ILI2120_CMD_GET_PROTOCOL_VERSION : iic_write_data() failed, rc = %d\n", rc);
			return -EFAULT;
        }

        mdelay(10);

        sz_write_buf[0] = ILITEK_TP_ILI2120_CMD_GET_PROTOCOL_VERSION;
        rc = iic_write_data(slave_i2c_id_dw_i2c, &sz_write_buf[0], 1);
        if (rc < 0) {
            DBG(&g_i2c_client->dev,
            "ILITEK_TP_ILI2120_CMD_GET_PROTOCOL_VERSION : iic_write_data()failed, rc = %d\n", rc);
			return -EFAULT;
        }

        mdelay(10);

        rc = iic_read_data(slave_i2c_id_dw_i2c, &szReadBuf[0], 2);
        if (rc < 0) {
            DBG(&g_i2c_client->dev,
            "ILITEK_TP_ILI2120_CMD_GET_PROTOCOL_VERSION : iic_read_data() failed, rc = %d\n", rc);
			return -EFAULT;
        }

        DBG(&g_i2c_client->dev, "Protocol Version = %d.%d\n", szReadBuf[0],
            szReadBuf[1]);

        if (szReadBuf[0] == 0)
            p_tp_info->n_protocol_version = ((szReadBuf[1]) << 8) + szReadBuf[0];
        else
            p_tp_info->n_protocol_version = ((szReadBuf[0]) << 8) + szReadBuf[1];

        /*read touch resolution */
        sz_write_buf[0] = ILITEK_TP_CMD_READ_DATA_CONTROL;
        sz_write_buf[1] = ILITEK_TP_CMD_GET_RESOLUTION;
        rc = iic_write_data(slave_i2c_id_dw_i2c, &sz_write_buf[0], 2);
        if (rc < 0) {
            DBG(&g_i2c_client->dev,
            "ILITEK_TP_CMD_READ_DATA_CONTROL & ILITEK_TP_CMD_GET_RESOLUTION : iic_write_data() failed, rc = %d\n", rc);
			return -EFAULT;
        }

        mdelay(10);

        sz_write_buf[0] = ILITEK_TP_CMD_GET_RESOLUTION;
        rc = iic_write_data(slave_i2c_id_dw_i2c, &sz_write_buf[0], 1);
        if (rc < 0) {
            DBG(&g_i2c_client->dev,
            "ILITEK_TP_CMD_GET_RESOLUTION : iic_write_data() failed, rc = %d\n",
                rc);
			return -EFAULT;
        }

        mdelay(10);

        rc = iic_read_data(slave_i2c_id_dw_i2c, &szReadBuf[0], 10);
        if (rc < 0) {
            DBG(&g_i2c_client->dev,
            "ILITEK_TP_CMD_GET_RESOLUTION : iic_read_data() failed, rc = %d\n",
                rc);
			return -EFAULT;
        }

        /*calculate the resolution for x and y direction */
        p_tp_info->n_max_x = szReadBuf[2];
        p_tp_info->n_max_x += (szReadBuf[3] * 256);
        p_tp_info->n_max_y = szReadBuf[4];
        p_tp_info->n_max_y += (szReadBuf[5] * 256);
        p_tp_info->n_min_x = szReadBuf[0];
        p_tp_info->n_min_y = szReadBuf[1];
        p_tp_info->n_x_channel_num = szReadBuf[6];
        p_tp_info->n_y_channel_num = szReadBuf[7];
        p_tp_info->n_max_touch_num = szReadBuf[8];
        p_tp_info->n_key_count = szReadBuf[9];

        DBG(&g_i2c_client->dev,
            "n_min_x=%d, n_max_x=%d, n_min_y=%d, n_max_y=%d, n_x_channel_num=%d, n_y_channel_num=%d, n_max_touch_num=%d, n_key_count=%d\n",
            p_tp_info->n_min_x, p_tp_info->n_max_x, p_tp_info->n_min_y, p_tp_info->n_max_y,
            p_tp_info->n_x_channel_num, p_tp_info->n_y_channel_num, p_tp_info->n_max_touch_num,
            p_tp_info->n_key_count);

        /*read key information */
        if (p_tp_info->n_key_count) {
            sz_write_buf[0] = ILITEK_TP_CMD_READ_DATA_CONTROL;
            sz_write_buf[1] = ILITEK_TP_ILI2120_CMD_GET_KEY_INFORMATION;
            rc = iic_write_data(slave_i2c_id_dw_i2c, &sz_write_buf[0], 2);
            if (rc < 0) {
                DBG(&g_i2c_client->dev,
             "ILITEK_TP_CMD_READ_DATA_CONTROL & ILITEK_TP_ILI2120_CMD_GET_KEY_INFORMATION : iic_write_data() failed, rc = %d\n", rc);
				return -EFAULT;
            }

            mdelay(10);

            sz_write_buf[0] = ILITEK_TP_ILI2120_CMD_GET_KEY_INFORMATION;
            rc = iic_write_data(slave_i2c_id_dw_i2c, &sz_write_buf[0], 1);
            if (rc < 0) {
                DBG(&g_i2c_client->dev,
            "ILITEK_TP_ILI2120_CMD_GET_KEY_INFORMATION : iic_write_data() failed, rc = %d\n", rc);
				return -EFAULT;
            }

            mdelay(10);

            rc = iic_read_data(slave_i2c_id_dw_i2c, &szReadBuf[0], 29);
            if (rc < 0) {
                DBG(&g_i2c_client->dev,
             "ILITEK_TP_ILI2120_CMD_GET_KEY_INFORMATION : iic_read_data() failed, rc = %d\n", rc);
				return -EFAULT;
            }

            if (p_tp_info->n_key_count > 5) {
                rc = iic_read_data(slave_i2c_id_dw_i2c, (szReadBuf + 29), 25);
                if (rc < 0) {
                    DBG(&g_i2c_client->dev,
             "ILITEK_TP_ILI2120_CMD_GET_KEY_INFORMATION : iic_read_data() failed, rc = %d\n", rc);
					return -EFAULT;
                }
            }

            p_tp_info->n_key_area_x_length = (szReadBuf[0] << 8) + szReadBuf[1];
            p_tp_info->n_key_area_y_length = (szReadBuf[2] << 8) + szReadBuf[3];

            DBG(&g_i2c_client->dev, "n_key_area_x_length=%d, n_key_area_y_length=%d\n",
                p_tp_info->n_key_area_x_length, p_tp_info->n_key_area_y_length);

            for (i = 0; i < p_tp_info->n_key_count; i++) {
                p_tp_info->t_key_info[i].n_id = szReadBuf[i * 5 + 4];
                p_tp_info->t_key_info[i].n_x =
                    (szReadBuf[i * 5 + 5] << 8) + szReadBuf[i * 5 + 6];
                p_tp_info->t_key_info[i].n_y =
                    (szReadBuf[i * 5 + 7] << 8) + szReadBuf[i * 5 + 8];
                p_tp_info->t_key_info[i].n_status = 0;

                DBG(&g_i2c_client->dev,
                    "KeyInfo[%d].n_id=%d, KeyInfo[%d].n_x=%d, KeyInfo[%d].n_y=%d, KeyInfo[%d].n_status=%d\n",
                    i, p_tp_info->t_key_info[i].n_id, i, p_tp_info->t_key_info[i].n_x, i,
                    p_tp_info->t_key_info[i].n_y, i, p_tp_info->t_key_info[i].n_status);
            }
        }
    } else if (g_chip_type == CHIP_TYPE_ILI2121) {
        /*read firmware version */
        sz_write_buf[0] = ILITEK_TP_ILI2121_CMD_GET_FIRMWARE_VERSION;
        rc = iic_write_data(slave_i2c_id_dw_i2c, &sz_write_buf[0], 1);
        if (rc < 0) {
            DBG(&g_i2c_client->dev,
                "ILITEK_TP_ILI2121_CMD_GET_FIRMWARE_VERSION : iic_write_data() failed, rc = %d\n", rc);
			return -EFAULT;
        }

        mdelay(10);

        rc = iic_read_data(slave_i2c_id_dw_i2c, &szReadBuf[0], 4);
        if (rc < 0) {
            DBG(&g_i2c_client->dev,
                "ILITEK_TP_ILI2121_CMD_GET_FIRMWARE_VERSION : iic_read_data() failed, rc = %d\n", rc);
			return -EFAULT;
        }

        for (i = 0; i < 4; i++)
            p_tp_info->sz_fw_version[i] = szReadBuf[i];


        DBG(&g_i2c_client->dev, "Firmware Version = %d.%d.%d.%d\n",
            szReadBuf[0], szReadBuf[1], szReadBuf[2], szReadBuf[3]);

        /*read protocol version */
        sz_write_buf[0] = ILITEK_TP_ILI2121_CMD_GET_PROTOCOL_VERSION;
        rc = iic_write_data(slave_i2c_id_dw_i2c, &sz_write_buf[0], 1);
        if (rc < 0) {
            DBG(&g_i2c_client->dev,
                "ILITEK_TP_ILI2121_CMD_GET_PROTOCOL_VERSION : iic_write_data() failed, rc = %d\n", rc);
			return -EFAULT;
        }

        mdelay(10);

        rc = iic_read_data(slave_i2c_id_dw_i2c, &szReadBuf[0], 2);
        if (rc < 0) {
            DBG(&g_i2c_client->dev,
                "ILITEK_TP_ILI2121_CMD_GET_PROTOCOL_VERSION : iic_read_data() failed, rc = %d\n", rc);
			return -EFAULT;
        }

        p_tp_info->n_protocol_version = (szReadBuf[0] << 8) + szReadBuf[1];

        DBG(&g_i2c_client->dev, "Protocol Version = %d.%d\n", szReadBuf[0],
            szReadBuf[1]);

        /*read touch resolution */
        sz_write_buf[0] = ILITEK_TP_CMD_GET_RESOLUTION;
        rc = iic_write_data(slave_i2c_id_dw_i2c, &sz_write_buf[0], 1);
        if (rc < 0) {
            DBG(&g_i2c_client->dev,
            "ILITEK_TP_CMD_GET_RESOLUTION : iic_write_data() failed, rc = %d\n",
            rc);
			return -EFAULT;
        }

        mdelay(10);

        rc = iic_read_data(slave_i2c_id_dw_i2c, &szReadBuf[0], 10);
        if (rc < 0) {
            DBG(&g_i2c_client->dev,
            "ILITEK_TP_CMD_GET_RESOLUTION : iic_read_data() failed, rc = %d\n",
             rc);
			return -EFAULT;
        }

        /*calculate the resolution for x and y direction */
        p_tp_info->n_max_x = szReadBuf[0];
        p_tp_info->n_max_x += (szReadBuf[1] * 256);
        p_tp_info->n_max_y = szReadBuf[2];
        p_tp_info->n_max_y += (szReadBuf[3] * 256);
        p_tp_info->n_min_x = 0;
        p_tp_info->n_min_y = 0;
        p_tp_info->n_x_channel_num = szReadBuf[4];
        p_tp_info->n_y_channel_num = szReadBuf[5];
        p_tp_info->n_max_touch_num = szReadBuf[6];
        p_tp_info->n_max_key_button_num = szReadBuf[7];
        p_tp_info->n_key_count = szReadBuf[8];

        DBG(&g_i2c_client->dev,
            "n_max_x=%d, n_max_y=%d, n_x_channel_num=%d, n_y_channel_num=%d, n_max_touch_num=%d, n_max_key_button_num=%d, n_key_count=%d\n",
            p_tp_info->n_max_x, p_tp_info->n_max_y, p_tp_info->n_x_channel_num,
            p_tp_info->n_y_channel_num, p_tp_info->n_max_touch_num,
            p_tp_info->n_max_key_button_num, p_tp_info->n_key_count);

        /*read key information */
        if ((p_tp_info->n_protocol_version & 0x300) == 0x300) {
            if (p_tp_info->n_key_count) {
                sz_write_buf[0] = ILITEK_TP_CMD_GET_KEY_INFORMATION;
                rc = iic_write_data(slave_i2c_id_dw_i2c, &sz_write_buf[0], 1);
                if (rc < 0) {
                    DBG(&g_i2c_client->dev,
                        "ILITEK_TP_CMD_GET_KEY_INFORMATION : iic_write_data() failed, rc = %d\n",
                        rc);
					return -EFAULT;
                }

                rc = iic_read_data(slave_i2c_id_dw_i2c, &szReadBuf[0], 29);
                if (rc < 0) {
                    DBG(&g_i2c_client->dev,
                        "ILITEK_TP_CMD_GET_KEY_INFORMATION : iic_read_data() failed, rc = %d\n",
                        rc);
					return -EFAULT;
                }

                if (p_tp_info->n_key_count > 5) {
                    rc = iic_read_data(slave_i2c_id_dw_i2c, (szReadBuf + 29), 25);
                    if (rc < 0) {
                        DBG(&g_i2c_client->dev,
                            "ILITEK_TP_CMD_GET_KEY_INFORMATION : iic_read_data() failed, rc = %d\n",
                            rc);
						return -EFAULT;
                    }
                }

                p_tp_info->n_key_area_x_length = (szReadBuf[0] << 8) + szReadBuf[1];
                p_tp_info->n_key_area_y_length = (szReadBuf[2] << 8) + szReadBuf[3];

                DBG(&g_i2c_client->dev,
                    "n_key_area_x_length=%d, n_key_area_y_length=%d\n",
                    p_tp_info->n_key_area_x_length, p_tp_info->n_key_area_y_length);

                for (i = 0; i < p_tp_info->n_key_count; i++) {
                    p_tp_info->t_key_info[i].n_id = szReadBuf[i * 5 + 4];
                    p_tp_info->t_key_info[i].n_x =
                        (szReadBuf[i * 5 + 5] << 8) + szReadBuf[i * 5 + 6];
                    p_tp_info->t_key_info[i].n_y =
                        (szReadBuf[i * 5 + 7] << 8) + szReadBuf[i * 5 + 8];
                    p_tp_info->t_key_info[i].n_status = 0;

                    DBG(&g_i2c_client->dev,
                        "KeyInfo[%d].n_id=%d, KeyInfo[%d].n_x=%d, KeyInfo[%d].n_y=%d, KeyInfo[%d].n_status=%d\n",
                        i, p_tp_info->t_key_info[i].n_id, i, p_tp_info->t_key_info[i].n_x,
                        i, p_tp_info->t_key_info[i].n_y, i,
                        p_tp_info->t_key_info[i].n_status);
                }
            }
        }
    }

    mutex_unlock(&g_mutex);

    return rc;
}

static s32 drv_iliTek_read_and_parse_finger_touch_data(u8 *p_packet,
                                                 u16 nReportPacketLength,
                                                 struct mutual_touch_info_t *p_info)
{
    u32 i = 0, j = 0;
    s32 rc;
    u16 n_x = 0;
    u16 n_y = 0;
    u8 nTouchId = 0;

/*if (is_firmware_data_log_enabled)*/

/*{*/
    if (g_firmware_mode == firmware_mode_demo_mode) {
        u16 n_length = 0;
        u8 sz_write_buf[1] = { 0 };
        u8 nTouchStatus = 0;
#ifdef CONFIG_SWAP_X_Y
        u16 nTemp = 0;
#endif /*CONFIG_SWAP_X_Y */
        if (g_chip_type == CHIP_TYPE_ILI2121
            || (g_chip_type == CHIP_TYPE_ILI2120 &&
                g_ilitek_tp_info.n_protocol_version > 0x100)) {
        } else {
            nReportPacketLength = 0x3;
        }

        sz_write_buf[0] = ILITEK_TP_CMD_READ_DATA;
        rc = iic_write_data(slave_i2c_id_dw_i2c, &sz_write_buf[0], 1);
        if (rc < 0) {
            DBG(&g_i2c_client->dev,
                "ILITEK_TP_CMD_READ_DATA : iic_write_data() failed, rc = %d\n",
                rc);
			return -EFAULT;
        }

        rc = iic_read_data(slave_i2c_id_dw_i2c, &p_packet[0], nReportPacketLength);
        if (rc < 0) {
            DBG(&g_i2c_client->dev,
                "ILITEK_TP_CMD_READ_DATA : iic_read_data() failed, rc = %d\n",
                rc);
			return -EFAULT;
        }

        DBG(&g_i2c_client->dev,
            "received raw data from touch panel as following:\n");
        DBG(&g_i2c_client->dev, "p_packet[0]=%x p_packet[1]=%x p_packet[2]=%x\n",
            p_packet[0], p_packet[1], p_packet[2]);

#ifdef DEBUG_NETLINK
        if (debug_flag)
            udp_reply(pid, seq, p_packet, 53);

        if (p_packet[1] == 0x5F)
            return 0;

#endif /*DEBUG_NETLINK */

        n_length = p_packet[0];
        if (n_length > 20)
			return -EFAULT;

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
        if (g_gesture_wakeup_flag == 1) {
            u8 nWakeupMode = 0;

            nWakeupMode = p_packet[2];

            DBG(&g_i2c_client->dev, "nWakeupMode = 0x%x\n", nWakeupMode);

            switch (nWakeupMode) {
            case 0x60:
                DBG(&g_i2c_client->dev, "gesture wake up this is c\n");

                input_report_key(g_input_device, KEY_C, 1);
                input_sync(g_input_device);
                input_report_key(g_input_device, KEY_C, 0);
                input_sync(g_input_device);
                break;
            case 0x62:
                DBG(&g_i2c_client->dev, "gesture wake up this is e\n");

                input_report_key(g_input_device, KEY_E, 1);
                input_sync(g_input_device);
                input_report_key(g_input_device, KEY_E, 0);
                input_sync(g_input_device);
                break;
            case 0x64:
                DBG(&g_i2c_client->dev, "gesture wake up this is m\n");

                input_report_key(g_input_device, KEY_M, 1);
                input_sync(g_input_device);
                input_report_key(g_input_device, KEY_M, 0);
                input_sync(g_input_device);
                break;
            case 0x66:
                DBG(&g_i2c_client->dev, "gesture wake up this is w\n");

                input_report_key(g_input_device, KEY_W, 1);
                input_sync(g_input_device);
                input_report_key(g_input_device, KEY_W, 0);
                input_sync(g_input_device);
                break;
            case 0x68:
                DBG(&g_i2c_client->dev, "gesture wake up this is o\n");

                input_report_key(g_input_device, KEY_O, 1);
                input_sync(g_input_device);
                input_report_key(g_input_device, KEY_O, 0);
                input_sync(g_input_device);
                break;
            case 0x84:
                DBG(&g_i2c_client->dev,
                    "gesture wake up this is slide right\n");

                input_report_key(g_input_device, KEY_RIGHT, 1);
                input_sync(g_input_device);
                input_report_key(g_input_device, KEY_RIGHT, 0);
                input_sync(g_input_device);
                break;
            case 0x8c:
                DBG(&g_i2c_client->dev, "gesture wake up this is slide left\n");

                input_report_key(g_input_device, KEY_LEFT, 1);
                input_sync(g_input_device);
                input_report_key(g_input_device, KEY_LEFT, 0);
                input_sync(g_input_device);
                break;
            case 0x88:
                DBG(&g_i2c_client->dev, "gesture wake up this is slide down\n");

                input_report_key(g_input_device, KEY_DOWN, 1);
                input_sync(g_input_device);
                input_report_key(g_input_device, KEY_DOWN, 0);
                input_sync(g_input_device);
                break;
            case 0x80:
                DBG(&g_i2c_client->dev, "gesture wake up this is slide up\n");

                input_report_key(g_input_device, KEY_UP, 1);
                input_sync(g_input_device);
                input_report_key(g_input_device, KEY_UP, 0);
                input_sync(g_input_device);
                break;
            case 0x22:
                DBG(&g_i2c_client->dev,
                    "gesture wake up this is duble click\n");

                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            default:
                DBG(&g_i2c_client->dev, "no support this gesture!\n");
                break;
            }

			return -EFAULT;
        }
#endif /*CONFIG_ENABLE_GESTURE_WAKEUP */

#ifdef CONFIG_TP_HAVE_KEY
        p_info->n_key_code = 0xFF;
#endif /*CONFIG_TP_HAVE_KEY */

        for (i = 0; i < g_ilitek_tp_info.n_max_touch_num /* max_touch_num */ ; i++) {
            if (g_chip_type == CHIP_TYPE_ILI2121
                || (g_chip_type == CHIP_TYPE_ILI2120 &&
                    g_ilitek_tp_info.n_protocol_version > 0x100)) {
                nTouchStatus = (p_packet[i * 5 + 3] >> 7);
                n_x = (((p_packet[i * 5 + 3] & 0x3F) << 8) + p_packet[i * 5 + 4]);
                n_y = (((p_packet[i * 5 + 5] & 0x3F) << 8) + p_packet[i * 5 + 6]);
            } else {
                if (i >= n_length)
                    return 0;

                sz_write_buf[0] = ILITEK_TP_CMD_READ_SUB_DATA;
                rc = iic_write_data(slave_i2c_id_dw_i2c, &sz_write_buf[0], 1);
                if (rc < 0) {
                    DBG(&g_i2c_client->dev,
                        "ILITEK_TP_CMD_READ_SUB_DATA : iic_write_data() failed, rc = %d\n",
                        rc);
					return -EFAULT;
                }

                rc = iic_read_data(slave_i2c_id_dw_i2c, &p_packet[0], 7);
                if (rc < 0) {
                    DBG(&g_i2c_client->dev,
                        "ILITEK_TP_CMD_READ_SUB_DATA : iic_read_data() failed, rc = %d\n",
                        rc);
					return -EFAULT;
                }

                if ((p_packet[0] & 0xC0) == 0x80)
                    nTouchStatus = 1;
                else
                    nTouchStatus = 0;

                n_x = ((p_packet[2] << 8) + p_packet[1]);
                n_y = ((p_packet[4] << 8) + p_packet[3]);
                nTouchId = (p_packet[0] & 0x3F) - 1;
            }
#ifdef CONFIG_SWAP_X_Y
            nTemp = n_x;
            n_x = n_y;
            n_y = nTemp;
#endif /*CONFIG_SWAP_X_Y */

#ifdef CONFIG_REVERSE_X
            n_x = g_ilitek_tp_info.n_max_x - n_x;
#endif /*CONFIG_REVERSE_X */

#ifdef CONFIG_REVERSE_Y
            n_y = g_ilitek_tp_info.n_max_y - n_y;
#endif /*CONFIG_REVERSE_X */

            if (nTouchStatus) {
                DBG(&g_i2c_client->dev, "TOUCH_POINT : i=%d, n_x=%d, n_y=%d\n", i,
                    n_x, n_y);

/*#ifdef VIRTUAL_KEY_PAD*/
                if (0 == g_ilitek_tp_info.n_handle_key_flag) {   /*can handle key */
                    for (j = 0; j < g_ilitek_tp_info.n_key_count; j++) {
                        if ((n_x >= g_ilitek_tp_info.t_key_info[j].n_x
                             && n_x <=
                             g_ilitek_tp_info.t_key_info[j].n_x +
                             g_ilitek_tp_info.n_key_area_x_length)
                            && (n_y >= g_ilitek_tp_info.t_key_info[j].n_y &&
                                n_y <=
                                g_ilitek_tp_info.t_key_info[j].n_y +
                                g_ilitek_tp_info.n_key_area_y_length)) {
                            input_report_key(g_input_device, BTN_TOUCH, 1);
                            input_report_key(g_input_device,
                                             g_ilitek_tp_info.t_key_info[j].n_id, 1);
                            input_sync(g_input_device);

                            g_ilitek_tp_info.t_key_info[j].n_status = 1;
                            g_is_first_touch_key_pressed[i] = 1;

                            DBG(&g_i2c_client->dev,
                                "KEY : Key Down => n_id=%d, n_x=%d, n_y=%d, n_status=%d, n_handle_key_flag=%d\n",
                                g_ilitek_tp_info.t_key_info[j].n_id, n_x, n_y,
                                g_ilitek_tp_info.t_key_info[j].n_status,
                                g_ilitek_tp_info.n_handle_key_flag);
                            break;
                        }
                    }
                }

                if (0 == g_is_first_touch_key_pressed[i]) {
                    p_info->tPoint[p_info->n_count].n_x = n_x;
                    p_info->tPoint[p_info->n_count].n_y = n_y;
#ifdef CONFIG_ENABLE_REPORT_PRESSURE
                    p_info->tPoint[p_info->n_count].nP = p_packet[i * 5 + 7];
#else
                    p_info->tPoint[p_info->n_count].nP = 10;
#endif /*CONFIG_ENABLE_REPORT_PRESSURE */

                    if (g_chip_type == CHIP_TYPE_ILI2121
                        || (g_chip_type == CHIP_TYPE_ILI2120 &&
                            g_ilitek_tp_info.n_protocol_version > 0x100)) {
                        p_info->tPoint[p_info->n_count].n_id = i;
#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
                        g_current_touch[i] = 1;
#endif /*CONFIG_ENABLE_TYPE_B_PROTOCOL */
                    } else {
                        p_info->tPoint[p_info->n_count].n_id = nTouchId;
#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
                        g_current_touch[nTouchId] = 1;
#endif /*CONFIG_ENABLE_TYPE_B_PROTOCOL */
                    }
                    p_info->n_count++;

                    g_ilitek_tp_info.n_handle_key_flag = 1;  /*can not handle key */
                    }

/*#ifdef VIRTUAL_KEY_PAD*/
                if (g_is_first_touch_key_pressed[i] == 1) {
                    for (j = 0; j < g_ilitek_tp_info.n_key_count; j++) {
                        if ((g_ilitek_tp_info.t_key_info[j].n_status == 1)
                            && (n_x < g_ilitek_tp_info.t_key_info[j].n_x
                                || n_x >
                                g_ilitek_tp_info.t_key_info[j].n_x +
                                g_ilitek_tp_info.n_key_area_x_length
                                || n_y < g_ilitek_tp_info.t_key_info[j].n_y
                                || n_y >
                                g_ilitek_tp_info.t_key_info[j].n_y +
                                g_ilitek_tp_info.n_key_area_y_length)) {
                            input_report_key(g_input_device, BTN_TOUCH, 0);
                            input_report_key(g_input_device,
                                             g_ilitek_tp_info.t_key_info[j].n_id, 0);
                            input_sync(g_input_device);

                            g_ilitek_tp_info.t_key_info[j].n_status = 0;
                            g_is_first_touch_key_pressed[i] = 0;

                            DBG(&g_i2c_client->dev,
                                "KEY : Key Up => n_id=%d, n_x=%d, n_y=%d, n_status=%d, n_handle_key_flag=%d\n",
                                g_ilitek_tp_info.t_key_info[j].n_id, n_x, n_y,
                                g_ilitek_tp_info.t_key_info[j].n_status,
                                g_ilitek_tp_info.n_handle_key_flag);
                            break;
                        }
                    }
                }

            } else {
                DBG(&g_i2c_client->dev, "RELEASE_POINT : i=%d\n", i);

                if (g_chip_type == CHIP_TYPE_ILI2121
                    || (g_chip_type == CHIP_TYPE_ILI2120 &&
                        g_ilitek_tp_info.n_protocol_version > 0x100)) {
                    if (g_is_first_touch_key_pressed[i] == 1) {
                        for (j = 0; j < g_ilitek_tp_info.n_key_count; j++) {
                            if ((g_ilitek_tp_info.t_key_info[j].n_status == 1) &&
                                (n_x < g_ilitek_tp_info.t_key_info[j].n_x
                                 || n_x >
                                 g_ilitek_tp_info.t_key_info[j].n_x +
                                 g_ilitek_tp_info.n_key_area_x_length
                                 || n_y < g_ilitek_tp_info.t_key_info[j].n_y
                                 || n_y >
                                 g_ilitek_tp_info.t_key_info[j].n_y +
                                 g_ilitek_tp_info.n_key_area_y_length)) {
                                input_report_key(g_input_device, BTN_TOUCH, 0);
                                input_report_key(g_input_device,
                                                 g_ilitek_tp_info.t_key_info[j].n_id,
                                                 0);
                                input_sync(g_input_device);

                                g_ilitek_tp_info.t_key_info[j].n_status = 0;
                                g_is_first_touch_key_pressed[i] = 0;

                                DBG(&g_i2c_client->dev,
                                    "KEY : Key Up => n_id=%d, n_x=%d, n_y=%d, n_status=%d, n_handle_key_flag=%d\n",
                                    g_ilitek_tp_info.t_key_info[j].n_id, n_x, n_y,
                                    g_ilitek_tp_info.t_key_info[j].n_status,
                                    g_ilitek_tp_info.n_handle_key_flag);
                                break;
                            }
                        }
                    }
#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
                    g_current_touch[i] = 0;
#endif /*CONFIG_ENABLE_TYPE_B_PROTOCOL */
                } else {
#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
                    g_current_touch[nTouchId] = 0;
#endif /*CONFIG_ENABLE_TYPE_B_PROTOCOL */
                }
            }
        }

        if (0 == n_length)
            g_ilitek_tp_info.n_handle_key_flag = 0;  /*can handle key */

    } else if (g_firmware_mode == firmware_mode_debug_mode) {
#ifdef CONFIG_SWAP_X_Y
        u16 nTemp = 0;
#endif /*CONFIG_SWAP_X_Y */

#ifdef CONFIG_ENABLE_SEGMENT_READ_FINGER_TOUCH_DATA
        DBG(&g_i2c_client->dev,
       "*** g_fw_packet_data_address = 0x%2X ***\n", g_fw_packet_data_address);
        DBG(&g_i2c_client->dev,
       "*** g_fw_packet_flag_address = 0x%2X ***\n", g_fw_packet_flag_address);

        if (g_fw_support_segment == 0) {
            DBG(&g_i2c_client->dev, "g_fw_packet_data_address & g_fw_packet_flag_address is un-initialized\n");
			return -EFAULT;
        }

        if (g_is_disable_finger_touch == 1) {
            DBG(&g_i2c_client->dev, "Skip finger touch for handling get firmware info or change firmware mode\n");
			return -EFAULT;
        }

        if (0 != drv_check_finger_touch_packet_flag_bit1()) {
            DBG(&g_i2c_client->dev, "Bit1 is not 0. FW is not ready for providing debug mode packet to Device Driver\n");
			return -EFAULT;
        }

        rc = iic_segment_read_data_by_sm_bus(g_fw_packet_data_address, &p_packet[0],
                                       nReportPacketLength,
                                       MAX_I2C_TRANSACTION_LENGTH_LIMIT);

        drv_reset_finger_touch_packet_flag_bit1();

        if (rc < 0) {
            DBG(&g_i2c_client->dev,
                "I2C read debug mode packet data failed, rc = %d\n", rc);
			return -EFAULT;
        }
#else

#if defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM)
        mdelay(I2C_SMBUS_READ_COMMAND_DELAY_FOR_PLATFORM);
#endif /*CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM */

        rc = iic_read_data(slave_i2c_id_dw_i2c, &p_packet[0], nReportPacketLength);
        if (rc < 0) {
            DBG(&g_i2c_client->dev, "I2C read packet data failed, rc = %d\n",
                rc);
			return -EFAULT;
        }
#endif /*CONFIG_ENABLE_SEGMENT_READ_FINGER_TOUCH_DATA */

        if (p_packet[0] != 0xA7 && p_packet[3] != PACKET_TYPE_TOOTH_PATTERN) {
            DBG(&g_i2c_client->dev, "WRONG DEBUG MODE HEADER\n");
			return -EFAULT;
        }

        for (i = 0; i < max_touch_num; i++) {
            if ((p_packet[(3 * i) + 5] == 0xFF) && (p_packet[(3 * i) + 6] == 0xFF)
                && (p_packet[(3 * i) + 7] == 0xFF)) {
#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
                g_current_touch[i] = 0;
#endif /*CONFIG_ENABLE_TYPE_B_PROTOCOL */

                continue;
            }

            n_x = (((p_packet[(3 * i) + 5] & 0xF0) << 4) |
                  (p_packet[(3 * i) + 6]));
            n_y = (((p_packet[(3 * i) + 5] & 0x0F) << 8) |
                  (p_packet[(3 * i) + 7]));

#ifdef CONFIG_SWAP_X_Y
            nTemp = n_x;
            n_x = n_y;
            n_y = nTemp;
#endif /*CONFIG_SWAP_X_Y */

#ifdef CONFIG_REVERSE_X
            n_x = g_ilitek_tp_info.n_max_x - n_x;
#endif /*CONFIG_REVERSE_X */

#ifdef CONFIG_REVERSE_Y
            n_y = g_ilitek_tp_info.n_max_y - n_y;
#endif /*CONFIG_REVERSE_X */

            p_info->tPoint[p_info->n_count].n_x = n_x;
            p_info->tPoint[p_info->n_count].n_y = n_y;
            p_info->tPoint[p_info->n_count].nP = 1;
            p_info->tPoint[p_info->n_count].n_id = i;

            DBG(&g_i2c_client->dev, "[x,y]=[%d,%d]\n", n_x, n_y);
            DBG(&g_i2c_client->dev, "point[%d] : (%d,%d) = %d\n",
                p_info->tPoint[p_info->n_count].n_id,
                p_info->tPoint[p_info->n_count].n_x,
                p_info->tPoint[p_info->n_count].n_y,
                p_info->tPoint[p_info->n_count].nP);

            p_info->n_count++;

#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
            g_current_touch[i] = 1;
#endif /*CONFIG_ENABLE_TYPE_B_PROTOCOL */
        }

        /*Notify android application to retrieve debug mode
        * packet from device driver by sysfs.
        */
        if (g_touch_k_obj != NULL) {
            char *pEnvp[2];
            s32 n_ret_val = 0;

           memcpy(g_log_mode_packet, p_packet, nReportPacketLength);
           /*Copy g_debug_mode_packet to g_log_mode_packet for avoiding the
           * debug mode data which is received by MTPTool APK may be modified.
           */

            pEnvp[0] = "STATUS=GET_DEBUG_MODE_PACKET";
            pEnvp[1] = NULL;

            n_ret_val = kobject_uevent_env(g_touch_k_obj, KOBJ_CHANGE, pEnvp);
            DBG(&g_i2c_client->dev,
                "kobject_uevent_env() STATUS=GET_DEBUG_MODE_PACKET, n_ret_val = %d\n", n_ret_val);
        }
#ifdef CONFIG_TP_HAVE_KEY
        if (p_packet[0] == 0xA7 && p_packet[3] == PACKET_TYPE_TOOTH_PATTERN) {
            u8 nButton = 0xFF;

            nButton = p_packet[4];
 /*The pressed virtual key is stored in 5th byte for debug mode packet 0xA7. */

            if (nButton != 0xFF) {
                DBG(&g_i2c_client->dev, "button = %x\n", nButton);

                for (i = 0; i < max_touch_num; i++) {
                    if ((nButton & (1 << i)) == (1 << i)) {
                        if (p_info->n_key_code == 0) {
                            p_info->n_key_code = i;

#if defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM)
                            DBG(&g_i2c_client->dev, "key[%d]=%d\n", i,
                                g_tp_virtual_key[i]);

#elif defined(CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM)
#ifdef CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD
                            if (tpd_dts_data.use_tpd_button) {
                                DBG(&g_i2c_client->dev, "key[%d]=%d\n", i,
                                    tpd_dts_data.tpd_key_local[i]);

#ifdef CONFIG_ENABLE_REPORT_KEY_WITH_COORDINATE
                                p_info->n_key_code = 0xFF;
                                p_info->tPoint[p_info->n_count].n_x =
                                    tpd_dts_data.tpd_key_dim_local[i].key_x;
                                p_info->tPoint[p_info->n_count].n_y =
                                    tpd_dts_data.tpd_key_dim_local[i].key_y;
                                p_info->tPoint[p_info->n_count].nP = 1;
                                p_info->tPoint[p_info->n_count].n_id =
                                    p_info->n_count;
#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
                                g_current_touch[p_info->n_count] = 1;
#endif /*CONFIG_ENABLE_TYPE_B_PROTOCOL */
                                DBG(&g_i2c_client->dev,
                                    "virtual key point[%d] : (%d,%d) = %d\n",
                                    p_info->tPoint[p_info->n_count].n_id,
                                    p_info->tPoint[p_info->n_count].n_x,
                                    p_info->tPoint[p_info->n_count].n_y,
                                    p_info->tPoint[p_info->n_count].nP);
                                p_info->n_count++;
#endif /*CONFIG_ENABLE_REPORT_KEY_WITH_COORDINATE */
                            }
#else
                            DBG(&g_i2c_client->dev, "key[%d]=%d\n", i,
                                g_tp_virtual_key[i]);

#ifdef CONFIG_ENABLE_REPORT_KEY_WITH_COORDINATE
                            p_info->n_key_code = 0xFF;
                            p_info->tPoint[p_info->n_count].n_x =
                                g_tp_virtual_key_dim_local[i][0];
                            p_info->tPoint[p_info->n_count].n_y =
                                g_tp_virtual_key_dim_local[i][1];
                            p_info->tPoint[p_info->n_count].nP = 1;
                            p_info->tPoint[p_info->n_count].n_id = p_info->n_count;
#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
                            g_current_touch[p_info->n_count] = 1;
#endif /*CONFIG_ENABLE_TYPE_B_PROTOCOL */
                            DBG(&g_i2c_client->dev,
                                "virtual key point[%d] : (%d,%d) = %d\n",
                                p_info->tPoint[p_info->n_count].n_id,
                                p_info->tPoint[p_info->n_count].n_x,
                                p_info->tPoint[p_info->n_count].n_y,
                                p_info->tPoint[p_info->n_count].nP);
                            p_info->n_count++;
#endif /*CONFIG_ENABLE_REPORT_KEY_WITH_COORDINATE */
#endif /*CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD */
#endif
                        } else {
                            /*if pressing multi-key => no report */
                            p_info->n_key_code = 0xFF;
                        }
                    }
                }
            } else {
                p_info->n_key_code = 0xFF;
            }
        }
#endif /*CONFIG_TP_HAVE_KEY */
    } else {
        DBG(&g_i2c_client->dev, "WRONG FIRMWARE MODE :0x%x\n", g_firmware_mode);
		return -EFAULT;
    }

    return 0;
}

static void drv_self_handle_finger_touch(void)
{                               /*for MSG22xx */
    struct self_touch_info_t tInfo;
    u32 i;
    u8 sz_tx_data[4] = { 0 };
#ifdef CONFIG_TP_HAVE_KEY
    u8 n_touch_key_code = 0;
#endif /*CONFIG_TP_HAVE_KEY */
    static u32 nLastKeyCode = 0;
    u8 *p_packet = NULL;
    u16 nReportPacketLength = 0;
    s32 rc;

    if (g_is_disable_finger_touch == 1) {
        DBG(&g_i2c_client->dev, "Skip finger touch for handlingget firmware info or change firmware mode\n");
        return;
    }

    mutex_lock(&g_mutex);
    memset(&tInfo, 0x0, sizeof(struct self_touch_info_t));

    if (is_firmware_data_log_enabled) {
        if (g_firmware_mode == firmware_mode_demo_mode) {
            DBG(&g_i2c_client->dev, "firmware_mode_demo_mode\n");

            nReportPacketLength = demo_mode_packet_length;
            p_packet = g_demo_mode_packet;
        } else if (g_firmware_mode == firmware_mode_debug_mode) {
            DBG(&g_i2c_client->dev, "firmware_mode_debug_mode\n");

            if (g_self_firmware_info.n_log_mode_packet_header != 0x62 &&
                g_self_firmware_info.n_log_mode_packet_header != 0xA7) {
                DBG(&g_i2c_client->dev, "WRONG DEBUG MODE HEADER : 0x%x\n",
                    g_self_firmware_info.n_log_mode_packet_header);
                goto TouchHandleEnd;
            }

            if (g_chip_type == CHIP_TYPE_MSG22XX && is_self_freq_scan_enabled) {
                nReportPacketLength = SELF_FREQ_SCAN_PACKET_LENGTH;
                sz_tx_data[0] = 0x53;
                sz_tx_data[1] = g_fw_packet_flag_address >> 8;
                sz_tx_data[2] = g_fw_packet_flag_address;
                iic_write_data(slave_i2c_id_dw_i2c, &sz_tx_data[0], 3);
            } else {
                nReportPacketLength = g_self_firmware_info.n_log_mode_packet_length;
            }

            p_packet = g_debug_mode_packet;
        } else if (g_firmware_mode == firmware_mode_raw_data_mode) {
            DBG(&g_i2c_client->dev, "firmware_mode_raw_data_mode\n");

            if (g_self_firmware_info.n_log_mode_packet_header != 0x62 &&
                g_self_firmware_info.n_log_mode_packet_header != 0xA7) {
                DBG(&g_i2c_client->dev, "WRONG RAW DATA MODE HEADER : 0x%x\n",
                    g_self_firmware_info.n_log_mode_packet_header);
                goto TouchHandleEnd;
            }

            if (g_chip_type == CHIP_TYPE_MSG22XX && is_self_freq_scan_enabled) {
                nReportPacketLength = SELF_FREQ_SCAN_PACKET_LENGTH;
                sz_tx_data[0] = 0x53;
                sz_tx_data[1] = g_fw_packet_flag_address >> 8;
                sz_tx_data[2] = g_fw_packet_flag_address;
                iic_write_data(slave_i2c_id_dw_i2c, &sz_tx_data[0], 3);
                DBG(&g_i2c_client->dev, "nReportPacketLength = %d\n",
                    nReportPacketLength);
            } else {
                nReportPacketLength = g_self_firmware_info.n_log_mode_packet_length;
            }
            p_packet = g_debug_mode_packet;
        } else {
            DBG(&g_i2c_client->dev, "WRONG FIRMWARE MODE : 0x%x\n",
                g_firmware_mode);
            goto TouchHandleEnd;
        }
    } else {
        DBG(&g_i2c_client->dev, "firmware_mode_demo_mode\n");

        nReportPacketLength = demo_mode_packet_length;
        p_packet = g_demo_mode_packet;
    }                           /*is_firmware_data_log_enabled */

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
    if (g_gesture_debug_mode == 1 && g_gesture_wakeup_flag == 1) {
        DBG(&g_i2c_client->dev,
            "Set gesture debug mode packet length, g_chip_type=0x%x\n",
            g_chip_type);

        if (g_chip_type == CHIP_TYPE_MSG22XX) {
            nReportPacketLength = GESTURE_DEBUG_MODE_PACKET_LENGTH;
            p_packet = g_gesture_wakeup_packet;
        } else {
            DBG(&g_i2c_client->dev,
                "This chip type does not support gesture debug mode.\n");
            goto TouchHandleEnd;
        }
    } else if (g_gesture_wakeup_flag == 1) {
        DBG(&g_i2c_client->dev,
            "Set gesture wakeup packet length, g_chip_type=0x%x\n", g_chip_type);

        if (g_chip_type == CHIP_TYPE_MSG22XX) {
#ifdef CONFIG_ENABLE_GESTURE_INFORMATION_MODE
            nReportPacketLength = GESTURE_WAKEUP_INFORMATION_PACKET_LENGTH;
#else
            nReportPacketLength = GESTURE_WAKEUP_PACKET_LENGTH;
#endif /*CONFIG_ENABLE_GESTURE_INFORMATION_MODE */
            p_packet = g_gesture_wakeup_packet;
        } else {
            DBG(&g_i2c_client->dev,
                "This chip type does not support gesture wakeup.\n");
            goto TouchHandleEnd;
        }
    }
#else

    if (g_gesture_wakeup_flag == 1) {
        DBG(&g_i2c_client->dev,
            "Set gesture wakeup packet length, g_chip_type=0x%x\n", g_chip_type);

        if (g_chip_type == CHIP_TYPE_MSG22XX) {
#ifdef CONFIG_ENABLE_GESTURE_INFORMATION_MODE
            nReportPacketLength = GESTURE_WAKEUP_INFORMATION_PACKET_LENGTH;
#else
            nReportPacketLength = GESTURE_WAKEUP_PACKET_LENGTH;
#endif /*CONFIG_ENABLE_GESTURE_INFORMATION_MODE */

            p_packet = g_gesture_wakeup_packet;
        } else {
            DBG(&g_i2c_client->dev,
                "This chip type does not support gesture wakeup.\n");
            goto TouchHandleEnd;
        }
    }
#endif /*CONFIG_ENABLE_GESTURE_DEBUG_MODE */
#endif /*CONFIG_ENABLE_GESTURE_WAKEUP */

#if defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM)
#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
    if (g_gesture_wakeup_flag == 1) {
        u32 i = 0;

        while (i < 5) {
            mdelay(50);

            rc = iic_read_data(slave_i2c_id_dw_i2c, &p_packet[0],
                             nReportPacketLength);

            if (rc > 0) {
                break;
            }

            i++;
        }
        if (i == 5) {
            DBG(&g_i2c_client->dev, "I2C read packet data failed, rc = %d\n",
                rc);
            goto TouchHandleEnd;
        }
    } else {
        rc = iic_read_data(slave_i2c_id_dw_i2c, &p_packet[0], nReportPacketLength);
        if (rc < 0) {
            DBG(&g_i2c_client->dev, "I2C read packet data failed, rc = %d\n",
                rc);
            goto TouchHandleEnd;
        }
    }
#else
    rc = iic_read_data(slave_i2c_id_dw_i2c, &p_packet[0], nReportPacketLength);
    if (rc < 0) {
        DBG(&g_i2c_client->dev, "I2C read packet data failed, rc = %d\n", rc);
        goto TouchHandleEnd;
    }
#endif /*CONFIG_ENABLE_GESTURE_WAKEUP */
#elif defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM)
    rc = iic_read_data(slave_i2c_id_dw_i2c, &p_packet[0], nReportPacketLength);
    if (rc < 0) {
        DBG(&g_i2c_client->dev, "I2C read packet data failed, rc = %d\n", rc);
        goto TouchHandleEnd;
    }
#elif defined(CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM)
    if (nReportPacketLength > 8) {
#ifdef CONFIG_ENABLE_DMA_IIC
        dma_reset();
#endif /*CONFIG_ENABLE_DMA_IIC */
        rc = iic_read_data(slave_i2c_id_dw_i2c, &p_packet[0], nReportPacketLength);
    } else {
        rc = iic_read_data(slave_i2c_id_dw_i2c, &p_packet[0], nReportPacketLength);
    }

    if (rc < 0) {
        DBG(&g_i2c_client->dev, "I2C read packet data failed, rc = %d\n", rc);
        goto TouchHandleEnd;
    }
#endif
    DBG(&g_i2c_client->dev, "nReportPacketLength = %d\n", nReportPacketLength);
    if (0 == drv_self_parse_packet(p_packet, nReportPacketLength, &tInfo)) {
        /*report... */
        if ((tInfo.n_finger_num) == 0) {  /*touch end */
            if (nLastKeyCode != 0) {
                DBG(&g_i2c_client->dev, "key touch released\n");

                input_report_key(g_input_device, BTN_TOUCH, 0);
                input_report_key(g_input_device, nLastKeyCode, 0);
                input_sync(g_input_device);

                nLastKeyCode = 0;   /*clear key status.. */
            } else {
#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL    /*TYPE B PROTOCOL */
                input_report_key(g_input_device, BTN_TOUCH, 0);

                for (i = 0; i < max_touch_num; i++) {
                    drv_finger_touch_released(0, 0, i);
                    g_prior_press[i] = g_curr_press[i];
                }

                input_report_key(g_input_device, BTN_TOOL_FINGER, 0);
#else /*TYPE A PROTOCOL */
                drv_finger_touch_released(0, 0, 0);
#endif /*CONFIG_ENABLE_TYPE_B_PROTOCOL */

                input_sync(g_input_device);
            }
        } else {                /*touch on screen */

            if (tInfo.n_touch_key_code != 0) {
#ifdef CONFIG_TP_HAVE_KEY
#if defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM)
                if (tInfo.n_touch_key_code == 4) { /*TOUCH_KEY_HOME */
                    n_touch_key_code = g_tp_virtual_key[1];
                } else if (tInfo.n_touch_key_code == 1) {  /*TOUCH_KEY_MENU */
                    n_touch_key_code = g_tp_virtual_key[0];
                } else if (tInfo.n_touch_key_code == 2) {  /*TOUCH_KEY_BACK */
                    n_touch_key_code = g_tp_virtual_key[2];
                } else if (tInfo.n_touch_key_code == 8) {  /*TOUCH_KEY_SEARCH */
                    n_touch_key_code = g_tp_virtual_key[3];
                }
#elif defined(CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM)
#ifdef CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD
                if (tpd_dts_data.use_tpd_button) {
                    if (tInfo.n_touch_key_code == 4) { /*TOUCH_KEY_HOME */
                        n_touch_key_code = tpd_dts_data.tpd_key_local[1];
                    } else if (tInfo.n_touch_key_code == 1) {  /*TOUCH_KEY_MENU */
                        n_touch_key_code = tpd_dts_data.tpd_key_local[0];
                    } else if (tInfo.n_touch_key_code == 2) {  /*TOUCH_KEY_BACK */
                        n_touch_key_code = tpd_dts_data.tpd_key_local[2];
                    } else if (tInfo.n_touch_key_code == 8) {  /*TOUCH_KEY_SEARCH */
                        n_touch_key_code = tpd_dts_data.tpd_key_local[3];
                    }
                }
#else
                if (tInfo.n_touch_key_code == 4) { /*TOUCH_KEY_HOME */
                    n_touch_key_code = g_tp_virtual_key[1];
                } else if (tInfo.n_touch_key_code == 1) {  /*TOUCH_KEY_MENU */
                    n_touch_key_code = g_tp_virtual_key[0];
                } else if (tInfo.n_touch_key_code == 2) {  /*TOUCH_KEY_BACK */
                    n_touch_key_code = g_tp_virtual_key[2];
                } else if (tInfo.n_touch_key_code == 8) {  /*TOUCH_KEY_SEARCH */
                    n_touch_key_code = g_tp_virtual_key[3];
                }
#endif /*CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD */
#endif

                if (nLastKeyCode != n_touch_key_code) {
                    DBG(&g_i2c_client->dev, "key touch pressed\n");
                    DBG(&g_i2c_client->dev,
                        "n_touch_key_code = %d, nLastKeyCode = %d\n",
                        n_touch_key_code, nLastKeyCode);

                    nLastKeyCode = n_touch_key_code;

                    input_report_key(g_input_device, BTN_TOUCH, 1);
                    input_report_key(g_input_device, n_touch_key_code, 1);
                    input_sync(g_input_device);

#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
                    g_prev_touch_status = 0;
#endif /*CONFIG_ENABLE_TYPE_B_PROTOCOL */
                }
#endif /*CONFIG_TP_HAVE_KEY */
            } else {
                DBG(&g_i2c_client->dev,
                    "tInfo->n_finger_num = %d...............\n",
                    tInfo.n_finger_num);

#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
                for (i = 0; i < max_touch_num; i++) {
                    if (tInfo.n_finger_num != 0) {
                        if (g_curr_press[i]) {
                            input_report_key(g_input_device, BTN_TOUCH, 1);
                            drv_finger_touch_pressed(tInfo.tPoint[i].n_x,
                                                  tInfo.tPoint[i].n_y, 1, i);
                            input_report_key(g_input_device, BTN_TOOL_FINGER, 1);
                        } else if (0 == g_curr_press[i]
                                   && 1 == g_prior_press[i]) {
                            drv_finger_touch_released(0, 0, i);
                        }
                    }
                    g_prior_press[i] = g_curr_press[i];
                }
#else /*TYPE A PROTOCOL */
                for (i = 0; i < tInfo.n_finger_num; i++) {
                    drv_finger_touch_pressed(tInfo.tPoint[i].n_x,
                                          tInfo.tPoint[i].n_y, 1, 0);
                }
#endif /*CONFIG_ENABLE_TYPE_B_PROTOCOL */

                input_sync(g_input_device);
            }
        }

#ifdef CONFIG_ENABLE_COUNT_REPORT_RATE
        if (g_is_enable_report_rate == 1) {
            if (g_valid_touch_count == 4294967295UL) {
                g_valid_touch_count = 0;  /*Reset count if overflow */
                DBG(&g_i2c_client->dev, "g_valid_touch_count reset to 0\n");
            }

            g_valid_touch_count++;

            DBG(&g_i2c_client->dev, "g_valid_touch_count = %d\n",
                g_valid_touch_count);
        }
#endif /*CONFIG_ENABLE_COUNT_REPORT_RATE */
    }

TouchHandleEnd:
        mutex_unlock(&g_mutex);
}

static void drv_mutual_handle_finger_Touch(void)
{                               /*for MSG28xx/MSG58xxA */
    struct mutual_touch_info_t tInfo;
    u32 i = 0;
#ifdef CONFIG_TP_HAVE_KEY
    static u32 nLastKeyCode = 0xFF;
#endif /*CONFIG_TP_HAVE_KEY */
    static u32 nLastCount = 0;
    u8 *p_packet = NULL;
    u16 nReportPacketLength = 0;

    if (g_is_disable_finger_touch == 1) {
        DBG(&g_i2c_client->dev,
        "Skip finger touch for handling get firmware info or change firmware mode\n");
        return;
    }

    mutex_lock(&g_mutex);

        memset(&tInfo, 0x0, sizeof(struct mutual_touch_info_t));

    if (is_firmware_data_log_enabled) {
        if (g_firmware_mode == firmware_mode_demo_mode) {

            DBG(&g_i2c_client->dev, "firmware_mode_demo_mode\n");

            nReportPacketLength = demo_mode_packet_length;
            p_packet = g_demo_mode_packet;

        } else if (g_firmware_mode == firmware_mode_debug_mode) {
            DBG(&g_i2c_client->dev, "firmware_mode_debug_mode\n");

            if (g_mutual_firmware_info.n_log_mode_packet_header != 0xA7) {
                DBG(&g_i2c_client->dev, "WRONG DEBUG MODE HEADER : 0x%x\n",
                    g_mutual_firmware_info.n_log_mode_packet_header);
                goto TouchHandleEnd;
            }

            nReportPacketLength = g_mutual_firmware_info.n_log_mode_packet_length;
            p_packet = g_debug_mode_packet;

        } else {
            DBG(&g_i2c_client->dev, "WRONG FIRMWARE MODE : 0x%x\n",
                g_firmware_mode);
            goto TouchHandleEnd;
        }
    } else {
        {
            DBG(&g_i2c_client->dev, "firmware_mode_demo_mode\n");

            nReportPacketLength = demo_mode_packet_length;
            p_packet = g_demo_mode_packet;
        }
    }                           /*is_firmware_data_log_enabled */

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
    if (g_gesture_debug_mode == 1 && g_gesture_wakeup_flag == 1) {
        DBG(&g_i2c_client->dev,
            "Set gesture debug mode packet length, g_chip_type=0x%x\n",
            g_chip_type);

        if (g_chip_type == CHIP_TYPE_MSG28XX || g_chip_type == CHIP_TYPE_MSG58XXA
            || g_chip_type == CHIP_TYPE_ILI2118A ||
            g_chip_type == CHIP_TYPE_ILI2117A) {
            nReportPacketLength = GESTURE_DEBUG_MODE_PACKET_LENGTH;
            p_packet = g_gesture_wakeup_packet;
        } else {
            DBG(&g_i2c_client->dev,
                "This chip type does not support gesture debug mode.\n");
            goto TouchHandleEnd;
        }
    } else if (g_gesture_wakeup_flag == 1) {
        DBG(&g_i2c_client->dev, "Set gesture wakeup packet length\n");

#ifdef CONFIG_ENABLE_GESTURE_INFORMATION_MODE
        nReportPacketLength = GESTURE_WAKEUP_INFORMATION_PACKET_LENGTH;
#else
        nReportPacketLength = GESTURE_WAKEUP_PACKET_LENGTH;
#endif /*CONFIG_ENABLE_GESTURE_INFORMATION_MODE */

        p_packet = g_gesture_wakeup_packet;
    }
#else

    if (g_gesture_wakeup_flag == 1) {
        DBG(&g_i2c_client->dev, "Set gesture wakeup packet length\n");

#ifdef CONFIG_ENABLE_GESTURE_INFORMATION_MODE
        nReportPacketLength = GESTURE_WAKEUP_INFORMATION_PACKET_LENGTH;
#else
        nReportPacketLength = GESTURE_WAKEUP_PACKET_LENGTH;
#endif /*CONFIG_ENABLE_GESTURE_INFORMATION_MODE */

        p_packet = g_gesture_wakeup_packet;
    }
#endif /*CONFIG_ENABLE_GESTURE_DEBUG_MODE */
#endif /*CONFIG_ENABLE_GESTURE_WAKEUP */

#if defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM)
#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
    if (g_gesture_wakeup_flag == 1) {
        s32 rc;

        while (i < 5) {
            mdelay(50);

            rc = iic_read_data(slave_i2c_id_dw_i2c, &p_packet[0],
                             nReportPacketLength);
            if (rc > 0)
                break;


            i++;
        }
        if (i == 5) {
            DBG(&g_i2c_client->dev, "I2C read packet data failed, rc = %d\n",
                rc);
            goto TouchHandleEnd;
        }
    } else {
        if (0 !=
            drv_mutual_read_finger_touch_data(&p_packet[0], nReportPacketLength)) {
            goto TouchHandleEnd;
        }
    }
#else
    if (0 != drv_mutual_read_finger_touch_data(&p_packet[0], nReportPacketLength))
        goto TouchHandleEnd;

#endif /*CONFIG_ENABLE_GESTURE_WAKEUP */
#elif defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM)
    if (0 != drv_mutual_read_finger_touch_data(&p_packet[0], nReportPacketLength))
        goto TouchHandleEnd;

#elif defined(CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM)
    {
#ifdef CONFIG_ENABLE_DMA_IIC
        dma_reset();
#endif /*CONFIG_ENABLE_DMA_IIC */

        if (0 !=
            drv_mutual_read_finger_touch_data(&p_packet[0], nReportPacketLength)) {
            goto TouchHandleEnd;
        }
    }
#endif

    if (0 == drv_mutual_parse_packet(p_packet, nReportPacketLength, &tInfo)) {
#ifdef CONFIG_TP_HAVE_KEY
        if (tInfo.n_key_code != 0xFF) {   /*key touch pressed */
#if defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM)
            DBG(&g_i2c_client->dev,
                "tInfo.n_key_code=%x, nLastKeyCode=%x, g_tp_virtual_key[%d]=%d\n",
                tInfo.n_key_code, nLastKeyCode, tInfo.n_key_code,
                g_tp_virtual_key[tInfo.n_key_code]);
#elif defined(CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM)
#ifdef CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD
            if (tpd_dts_data.use_tpd_button) {
                DBG(&g_i2c_client->dev,
                    "tInfo.n_key_code=%x, nLastKeyCode=%x, tpd_dts_data.tpd_key_local[%d]=%d\n",
                    tInfo.n_key_code, nLastKeyCode, tInfo.n_key_code,
                    tpd_dts_data.tpd_key_local[tInfo.n_key_code]);
            }
#else
            DBG(&g_i2c_client->dev,
                "tInfo.n_key_code=%x, nLastKeyCode=%x, g_tp_virtual_key[%d]=%d\n",
                tInfo.n_key_code, nLastKeyCode, tInfo.n_key_code,
                g_tp_virtual_key[tInfo.n_key_code]);
#endif /*CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD */
#endif

            if (tInfo.n_key_code < MAX_KEY_NUM) {
                if (tInfo.n_key_code != nLastKeyCode) {
                    DBG(&g_i2c_client->dev, "key touch pressed\n");

                    input_report_key(g_input_device, BTN_TOUCH, 1);
#if defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM)
                    input_report_key(g_input_device,
                                     g_tp_virtual_key[tInfo.n_key_code], 1);
#elif defined(CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM)
#ifdef CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD
                    if (tpd_dts_data.use_tpd_button) {
                        input_report_key(g_input_device,
                                         tpd_dts_data.tpd_key_local[tInfo.
                                                                    n_key_code],
                                         1);
                    }
#else
                    input_report_key(g_input_device,
                                     g_tp_virtual_key[tInfo.n_key_code], 1);
#endif /*CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD */
#endif
                    input_sync(g_input_device);

                    nLastKeyCode = tInfo.n_key_code;
                } else {
                    /*pass duplicate key-pressing */
                    DBG(&g_i2c_client->dev, "REPEATED KEY\n");
                }
            } else {
                DBG(&g_i2c_client->dev, "WRONG KEY\n");
            }
        } else {                /*key touch released */

            if (nLastKeyCode != 0xFF) {
                DBG(&g_i2c_client->dev, "key touch released\n");

                input_report_key(g_input_device, BTN_TOUCH, 0);
#if defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM)
                input_report_key(g_input_device, g_tp_virtual_key[nLastKeyCode],
                                 0);
#elif defined(CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM)
#ifdef CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD
                if (tpd_dts_data.use_tpd_button) {
                    input_report_key(g_input_device,
                                     tpd_dts_data.tpd_key_local[nLastKeyCode],
                                     0);
                }
#else
                input_report_key(g_input_device, g_tp_virtual_key[nLastKeyCode],
                                 0);
#endif /*CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD */
#endif
                input_sync(g_input_device);

                nLastKeyCode = 0xFF;
            }
        }
#endif /*CONFIG_TP_HAVE_KEY */

        DBG(&g_i2c_client->dev, "tInfo.n_count = %d, nLastCount = %d\n",
            tInfo.n_count, nLastCount);

        if (tInfo.n_count > 0) { /*point touch pressed */
#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
            for (i = 0; i < tInfo.n_count; i++) {
                input_report_key(g_input_device, BTN_TOUCH, 1);
                drv_finger_touch_pressed(tInfo.tPoint[i].n_x, tInfo.tPoint[i].n_y,
                                      tInfo.tPoint[i].nP, tInfo.tPoint[i].n_id);
                input_report_key(g_input_device, BTN_TOOL_FINGER, 1);
            }

            for (i = 0; i < max_touch_num; i++) {
                DBG(&g_i2c_client->dev, "g_previous_touch[%d]=%d, g_current_touch[%d]=%d\n", i, g_previous_touch[i], i, g_current_touch[i]);    /*TODO : add for debug */

                if (g_current_touch[i] == 0 && g_previous_touch[i] == 1)
                    drv_finger_touch_released(0, 0, i);

                g_previous_touch[i] = g_current_touch[i];
            }
#else /*TYPE A PROTOCOL */
            for (i = 0; i < tInfo.n_count; i++) {
                drv_finger_touch_pressed(tInfo.tPoint[i].n_x, tInfo.tPoint[i].n_y,
                                      tInfo.tPoint[i].nP, tInfo.tPoint[i].n_id);
            }
#endif /*CONFIG_ENABLE_TYPE_B_PROTOCOL */

            input_sync(g_input_device);

            nLastCount = tInfo.n_count;
        } else {                /*point touch released */

            if (nLastCount > 0) {
#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL    /*TYPE B PROTOCOL */
                input_report_key(g_input_device, BTN_TOUCH, 0);

                for (i = 0; i < max_touch_num; i++) {
                    DBG(&g_i2c_client->dev, "g_previous_touch[%d]=%d, g_current_touch[%d]=%d\n", i, g_previous_touch[i], i, g_current_touch[i]);    /*TODO : add for debug */

                    if (g_current_touch[i] == 0 && g_previous_touch[i] == 1)
                        drv_finger_touch_released(0, 0, i);

                    g_previous_touch[i] = g_current_touch[i];
                }

                input_report_key(g_input_device, BTN_TOOL_FINGER, 0);
#else /*TYPE A PROTOCOL */
                drv_finger_touch_released(0, 0, 0);
#endif /*CONFIG_ENABLE_TYPE_B_PROTOCOL */

                input_sync(g_input_device);

                nLastCount = 0;
            }
        }

#ifdef CONFIG_ENABLE_COUNT_REPORT_RATE
        if (g_is_enable_report_rate == 1) {
            if (g_valid_touch_count == 4294967295UL) {
                g_valid_touch_count = 0;  /*Reset count if overflow */
                DBG(&g_i2c_client->dev, "g_valid_touch_count reset to 0\n");
            }

            g_valid_touch_count++;

            DBG(&g_i2c_client->dev, "g_valid_touch_count = %d\n",
                g_valid_touch_count);
        }
#endif /*CONFIG_ENABLE_COUNT_REPORT_RATE */
    }

TouchHandleEnd:

        mutex_unlock(&g_mutex);
}

/*---------------------------------------------------------------------------*/
static void drv_ilitek_handle_finger_touch(void)
{                               /*for ILI21xx */
   struct mutual_touch_info_t tInfo;
    u32 i = 0;
#ifdef CONFIG_TP_HAVE_KEY
    static u32 nLastKeyCode = 0xFF;
#endif /*CONFIG_TP_HAVE_KEY */
    static u32 nLastCount = 0;
    u8 *p_packet = NULL;
    u16 nReportPacketLength = 0;

        if (g_is_disable_finger_touch == 1) {
        DBG(&g_i2c_client->dev,
            "Skip finger touch for handling get firmware info or change firmware mode\n");
        return;
    }

    mutex_lock(&g_mutex);

        memset(&tInfo, 0x0, sizeof(struct mutual_touch_info_t));

    if (is_firmware_data_log_enabled) {
        if (g_firmware_mode == firmware_mode_demo_mode) {
            DBG(&g_i2c_client->dev, "firmware_mode_demo_mode\n");

            nReportPacketLength = demo_mode_packet_length;
            p_packet = g_demo_mode_packet;
        } else if (g_firmware_mode == firmware_mode_debug_mode) {
            DBG(&g_i2c_client->dev, "firmware_mode_debug_mode\n");

            if (g_mutual_firmware_info.n_log_mode_packet_header != 0xA7) {
                DBG(&g_i2c_client->dev, "WRONG DEBUG MODE HEADER : 0x%x\n",
                    g_mutual_firmware_info.n_log_mode_packet_header);
                goto TouchHandleEnd;
            }

            nReportPacketLength = g_mutual_firmware_info.n_log_mode_packet_length;
            p_packet = g_debug_mode_packet;
        } else {
            DBG(&g_i2c_client->dev, "WRONG FIRMWARE MODE : 0x%x\n",
                g_firmware_mode);
            goto TouchHandleEnd;
        }
    } else {
        DBG(&g_i2c_client->dev, "firmware_mode_demo_mode\n");

        nReportPacketLength = demo_mode_packet_length;
        p_packet = g_demo_mode_packet;
    }                           /*is_firmware_data_log_enabled */

#if defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM)
    if (0 !=
        drv_iliTek_read_and_parse_finger_touch_data(&p_packet[0], nReportPacketLength,
                                              &tInfo)) {
        goto TouchHandleEnd;
    }
#elif defined(CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM)
    {
#ifdef CONFIG_ENABLE_DMA_IIC
        dma_reset();
#endif /*CONFIG_ENABLE_DMA_IIC */

        if (0 !=
            drv_iliTek_read_and_parse_finger_touch_data(&p_packet[0],
                                                  nReportPacketLength,
                                                  &tInfo)) {
            goto TouchHandleEnd;
        }
    }
#endif
    if (g_chip_type == CHIP_TYPE_ILI2120 || g_chip_type == CHIP_TYPE_ILI2121) {
        if (p_packet[1] == 0x5F)
            goto TouchHandleEnd;

    }

/*The below is key handling for Debug Mode packet*/
#ifdef CONFIG_TP_HAVE_KEY
    if (tInfo.n_key_code != 0xFF) {   /*key touch pressed */
#if defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM)
        DBG(&g_i2c_client->dev,
            "tInfo.n_key_code=%x, nLastKeyCode=%x, g_tp_virtual_key[%d]=%d\n",
            tInfo.n_key_code, nLastKeyCode, tInfo.n_key_code,
            g_tp_virtual_key[tInfo.n_key_code]);
#elif defined(CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM)
#ifdef CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD
        if (tpd_dts_data.use_tpd_button) {
            DBG(&g_i2c_client->dev,
                "tInfo.n_key_code=%x, nLastKeyCode=%x, tpd_dts_data.tpd_key_local[%d]=%d\n",
                tInfo.n_key_code, nLastKeyCode, tInfo.n_key_code,
                tpd_dts_data.tpd_key_local[tInfo.n_key_code]);
        }
#else
        DBG(&g_i2c_client->dev,
            "tInfo.n_key_code=%x, nLastKeyCode=%x, g_tp_virtual_key[%d]=%d\n",
            tInfo.n_key_code, nLastKeyCode, tInfo.n_key_code,
            g_tp_virtual_key[tInfo.n_key_code]);
#endif /*CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD */
#endif

        if (tInfo.n_key_code < MAX_KEY_NUM) {
            if (tInfo.n_key_code != nLastKeyCode) {
                DBG(&g_i2c_client->dev, "key touch pressed\n");

                input_report_key(g_input_device, BTN_TOUCH, 1);
#if defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM)
                input_report_key(g_input_device, g_tp_virtual_key[tInfo.n_key_code],
                                 1);
#elif defined(CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM)
#ifdef CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD
                if (tpd_dts_data.use_tpd_button) {
                    input_report_key(g_input_device,
                                     tpd_dts_data.tpd_key_local[tInfo.n_key_code],
                                     1);
                }
#else
                input_report_key(g_input_device, g_tp_virtual_key[tInfo.n_key_code],
                                 1);
#endif /*CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD */
#endif
                input_sync(g_input_device);

                nLastKeyCode = tInfo.n_key_code;
            } else {
                /*pass duplicate key-pressing */
                DBG(&g_i2c_client->dev, "REPEATED KEY\n");
            }
        } else {
            DBG(&g_i2c_client->dev, "WRONG KEY\n");
        }
    } else {                    /*key touch released */

        if (nLastKeyCode != 0xFF) {
            DBG(&g_i2c_client->dev, "key touch released\n");

            input_report_key(g_input_device, BTN_TOUCH, 0);
#if defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM)
            input_report_key(g_input_device, g_tp_virtual_key[nLastKeyCode], 0);
#elif defined(CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM)
#ifdef CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD
            if (tpd_dts_data.use_tpd_button) {
                input_report_key(g_input_device,
                                 tpd_dts_data.tpd_key_local[nLastKeyCode], 0);
            }
#else
            input_report_key(g_input_device, g_tp_virtual_key[nLastKeyCode], 0);
#endif /*CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD */
#endif
            input_sync(g_input_device);

            nLastKeyCode = 0xFF;
        }
    }
#endif /*CONFIG_TP_HAVE_KEY */

    DBG(&g_i2c_client->dev, "tInfo.n_count = %d, nLastCount = %d\n",
        tInfo.n_count, nLastCount);

    if (tInfo.n_count > 0) {     /*point touch pressed */
#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
        for (i = 0; i < tInfo.n_count; i++) {
            input_report_key(g_input_device, BTN_TOUCH, 1);
            drv_finger_touch_pressed(tInfo.tPoint[i].n_x, tInfo.tPoint[i].n_y,
                                  tInfo.tPoint[i].nP, tInfo.tPoint[i].n_id);
            input_report_key(g_input_device, BTN_TOOL_FINGER, 1);
        }

        for (i = 0; i < g_ilitek_tp_info.n_max_touch_num /* max_touch_num */ ; i++) {
            DBG(&g_i2c_client->dev, "g_previous_touch[%d]=%d, g_current_touch[%d]=%d\n", i, g_previous_touch[i], i, g_current_touch[i]);

            if (g_current_touch[i] == 0 && g_previous_touch[i] == 1)
                drv_finger_touch_released(0, 0, i);

            g_previous_touch[i] = g_current_touch[i];
        }
#else /*TYPE A PROTOCOL */
        for (i = 0; i < tInfo.n_count; i++) {
            drv_finger_touch_pressed(tInfo.tPoint[i].n_x, tInfo.tPoint[i].n_y,
                                  tInfo.tPoint[i].nP, tInfo.tPoint[i].n_id);
        }
#endif /*CONFIG_ENABLE_TYPE_B_PROTOCOL */

        input_sync(g_input_device);

        nLastCount = tInfo.n_count;
    } else {                    /*point touch released */

        if (nLastCount > 0) {
#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL    /*TYPE B PROTOCOL */
            input_report_key(g_input_device, BTN_TOUCH, 0);

            for (i = 0; i < g_ilitek_tp_info.n_max_touch_num; i++) {
                DBG(&g_i2c_client->dev, "g_previous_touch[%d]=%d, g_current_touch[%d]=%d\n", i, g_previous_touch[i], i, g_current_touch[i]);

                if (g_current_touch[i] == 0 && g_previous_touch[i] == 1)
                    drv_finger_touch_released(0, 0, i);

                g_previous_touch[i] = g_current_touch[i];
            }

            input_report_key(g_input_device, BTN_TOOL_FINGER, 0);
#else /*TYPE A PROTOCOL */
            drv_finger_touch_released(0, 0, 0);
#endif /*CONFIG_ENABLE_TYPE_B_PROTOCOL */

            input_sync(g_input_device);

            nLastCount = 0;
        }
    }

TouchHandleEnd:

        mutex_unlock(&g_mutex);
}

static void drv_handle_finger_touch(void)
{
    mutex_lock(&g_mutex_protect);
        if (g_chip_type == CHIP_TYPE_MSG22XX) {
        drv_self_handle_finger_touch();
    } else if (g_chip_type == CHIP_TYPE_MSG28XX
               || g_chip_type == CHIP_TYPE_MSG58XXA
               || g_chip_type == CHIP_TYPE_ILI2118A ||
               g_chip_type == CHIP_TYPE_ILI2117A) {
        drv_mutual_handle_finger_Touch();
    } else if (g_chip_type == CHIP_TYPE_ILI2120 ||
               g_chip_type == CHIP_TYPE_ILI2121) {
        drv_ilitek_handle_finger_touch();
    }
    mutex_unlock(&g_mutex_protect);
}

/*---------------------------------------------------------------------------*/
#ifdef CONFIG_ENABLE_ITO_MP_TEST
static ssize_t drv_main_procfs_mp_test_customised_write(struct file *pfile,
                                                  const char __user *pbuffer,
                                                  size_t n_count, loff_t *ppos)
{
    return n_count;
}

static ssize_t drv_main_procfs_mp_test_customised_read(struct file *pfile,
                                                 char __user *pbuffer,
                                                 size_t n_count, loff_t *ppos)
{
#if 0
    u16 nMajor = 0, nMinor = 0;
    u32 n_length = 0;
    u8 result[16];
    int res = 0;
    char *ini_path = INI_PATH;

    if (*ppos != 0)
        return 0;

    drv_get_customer_firmware_version(&nMajor, &nMinor, &g_fw_version);
    DBG(&g_i2c_client->dev, "*** %s() g_fw_version = %s ***\n", __func__,
        g_fw_version);

    drv_get_platform_firmware_version(&g_platform_fw_version);
    DBG(&g_i2c_client->dev, "*** %s() g_platform_fw_version = %s ***\n",
        __func__, g_platform_fw_version);

    getFirmwareVersionOnFlash();

    DBG(&g_i2c_client->dev, "*** %s() Driver version = %s ***, %s\n", __func__,
        DEVICE_DRIVER_RELEASE_VERSION, ini_path);

    res = startMPTest(g_chip_type, ini_path);
    pr_info("MP Test _result = %d\n", res);

    if (res == 1 || res == 2) {
        save_test_data(g_chip_type, res);
    } else {
        pr_err("MP Test got unknown failure...won't save data as CSV\n");
    }

    n_length = snprintf(result, sizeof(result), "%d", res);

    res = copy_to_user((int *)pbuffer, &res, n_length);
    if (res < 0)
        DBG(0, "Failed to copy data to user space\n");

    *ppos += n_length;
    EndMpTest(g_chip_type);
    DBG(&g_i2c_client->dev, "end\n");
    return n_length;
#endif
    return 0;
}
#endif

/*---------------------------------------------------------------------------*/
static ssize_t drv_procfs_chip_type_read(struct file *pfile,
                                         char __user *pbuffer, size_t n_count,
                                         loff_t *ppos)
{
    u32 n_length = 0;
    u8 n_user_temp_buffer[16];

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    /*If file position is non-zero, then assume the string has been read and indicate there is no more data to be read. */
    if (*ppos != 0)
        return 0;


    n_length = snprintf(n_user_temp_buffer, sizeof(n_user_temp_buffer), "%d", g_chip_type);
    if (copy_to_user(pbuffer, n_user_temp_buffer, n_length))
        return -EFAULT;

    DBG(&g_i2c_client->dev, "g_chip_type = 0x%x, g_original_chip_type = 0x%x\n",
        g_chip_type, g_original_chip_type);

    if (g_chip_type == CHIP_TYPE_MSG22XX) {  /*(0x7A) */
        DBG(&g_i2c_client->dev, "g_msg22xx_chip_revision = 0x%x\n",
            g_msg22xx_chip_revision);
    } else if (g_chip_type == CHIP_TYPE_ILI2120 ||
               g_chip_type == CHIP_TYPE_ILI2121) {
        DBG(&g_i2c_client->dev, "n_max_x=%d, n_max_y=%d, n_x_channel_num=%d, n_y_channel_num=%d, n_max_touch_num=%d, n_max_key_button_num=%d, n_key_count=%d\n", g_ilitek_tp_info.n_max_x, g_ilitek_tp_info.n_max_y, g_ilitek_tp_info.n_x_channel_num, g_ilitek_tp_info.n_y_channel_num, g_ilitek_tp_info.n_max_touch_num, g_ilitek_tp_info.n_max_key_button_num, g_ilitek_tp_info.n_key_count);
        DBG(&g_i2c_client->dev, "Firmware Version = %d.%d.%d.%d\n", g_ilitek_tp_info.sz_fw_version[0], g_ilitek_tp_info.sz_fw_version[1], g_ilitek_tp_info.sz_fw_version[2], g_ilitek_tp_info.sz_fw_version[3]);
        DBG(&g_i2c_client->dev, "Protocol Version = %d\n", g_ilitek_tp_info.n_protocol_version);
    }

    *ppos += n_length;

    return n_length;
}

static ssize_t drv_procfs_chip_type_write(struct file *pfile,
                                          const char __user *pbuffer,
                                          size_t n_count, loff_t *ppos)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

/*g_chip_type = drv_get_chip_type();*/

    return n_count;
}

static ssize_t drv_procfs_firmware_data_read(struct file *pfile,
                                             char __user *pbuffer,
                                             size_t n_count, loff_t *ppos)
{
    DBG(&g_i2c_client->dev, "*** %s() g_fw_data_count = %d ***\n", __func__,
        g_fw_data_count);

    /*If file position is non-zero, then assume the string has been read and indicate there is no more data to be read. */
    if (*ppos != 0)
        return 0;


    *ppos += g_fw_data_count;

    return g_fw_data_count;
}

static ssize_t drv_procfs_firmware_data_write(struct file *pfile,
                                              const char __user *pbuffer,
                                              size_t n_count, loff_t *ppos)
{
    u32 nNum = n_count / 1024;
    u32 nRemainder = n_count % 1024;
    u32 i;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    if (g_chip_type == CHIP_TYPE_MSG22XX || g_chip_type == CHIP_TYPE_MSG28XX
        || g_chip_type == CHIP_TYPE_MSG58XXA || g_chip_type == CHIP_TYPE_ILI2118A
        || g_chip_type == CHIP_TYPE_ILI2117A) {
        if (nNum > 0) {         /*n_count >= 1024 */
            for (i = 0; i < nNum; i++) {
                memset(g_debug_buf, 0, 1024);
                if (copy_from_user(g_debug_buf, pbuffer + (i * 1024), 1024)) {
                    DBG(&g_i2c_client->dev, "copy_from_user() failed\n");

                    return -EFAULT;
                }
                memcpy(g_fw_data[g_fw_data_count], g_debug_buf, 1024);

                g_fw_data_count++;
            }

            if (nRemainder > 0) {   /*Handle special firmware size like MSG22XX(48.5KB) */
                DBG(&g_i2c_client->dev, "nRemainder = %d\n", nRemainder);
                memset(g_debug_buf, 0, 1024);
                if (copy_from_user
                    (g_debug_buf, pbuffer + (i * 1024), nRemainder)) {
                    DBG(&g_i2c_client->dev, "copy_from_user() failed\n");

                    return -EFAULT;
                }
                memcpy(g_fw_data[g_fw_data_count], g_debug_buf, nRemainder);

                g_fw_data_count++;
            }
        } else {                /*n_count < 1024 */

            if (n_count > 0) {
                memset(g_debug_buf, 0, 1024);
                if (copy_from_user(g_debug_buf, pbuffer, n_count)) {
                    DBG(&g_i2c_client->dev, "copy_from_user() failed\n");

                    return -EFAULT;
                }
                memcpy(g_fw_data[g_fw_data_count], g_debug_buf, n_count);

                g_fw_data_count++;
            }
        }
    } else if (g_chip_type == CHIP_TYPE_ILI2120 ||
               g_chip_type == CHIP_TYPE_ILI2121) {
        if (nNum > 0) {         /*n_count >= 1024 */
            for (i = 0; i < nNum; i++) {
                memset(g_debug_buf, 0, 1024);
                if (copy_from_user(g_debug_buf, pbuffer + (i * 1024), 1024)) {
                    DBG(&g_i2c_client->dev, "copy_from_user() failed\n");

                    return -EFAULT;
                }
                memcpy(g_ilitek_fw_data_buf + (g_fw_data_count), g_debug_buf, 1024);

                g_fw_data_count = g_fw_data_count + 1024;
            }

            if (nRemainder > 0) {
                DBG(&g_i2c_client->dev, "nRemainder = %d\n", nRemainder);
                memset(g_debug_buf, 0, 1024);
                if (copy_from_user
                    (g_debug_buf, pbuffer + (i * 1024), nRemainder)) {
                    DBG(&g_i2c_client->dev, "copy_from_user() failed\n");

                    return -EFAULT;
                }
                memcpy(g_ilitek_fw_data_buf + (g_fw_data_count), g_debug_buf,
                       nRemainder);

                g_fw_data_count = g_fw_data_count + nRemainder;
            }
        } else {                /*n_count < 1024 */

            if (n_count > 0) {
                memset(g_debug_buf, 0, 1024);
                if (copy_from_user(g_debug_buf, pbuffer, n_count)) {
                    DBG(&g_i2c_client->dev, "copy_from_user() failed\n");

                    return -EFAULT;
                }
                memcpy(g_ilitek_fw_data_buf + (g_fw_data_count), pbuffer, n_count);

                g_fw_data_count = g_fw_data_count + n_count;
            }
        }
    } else {
        DBG(&g_i2c_client->dev, "Unsupported chip type = 0x%x\n", g_chip_type);
    }

    DBG(&g_i2c_client->dev, "*** g_fw_data_count = %d ***\n", g_fw_data_count);

    if (g_debug_buf != NULL)
        DBG(&g_i2c_client->dev, "*** buf[0] = %c ***\n", g_debug_buf[0]);


    return n_count;
}

static ssize_t drv_procfs_firmware_update_read(struct file *pfile,
                                               char __user *pbuffer,
                                               size_t n_count, loff_t *ppos)
{
    u32 n_length = 0;
    u8 n_user_temp_buffer[16];

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    /*If file position is non-zero, then assume the string has been read and indicate there is no more data to be read. */
    if (*ppos != 0)
        return 0;


    n_length = snprintf(n_user_temp_buffer, sizeof(n_user_temp_buffer), "%d", g_is_update_complete);
    if (copy_to_user(pbuffer, n_user_temp_buffer, n_length))
        return -EFAULT;

    DBG(&g_i2c_client->dev, "*** g_is_update_complete = %d ***\n",
        g_is_update_complete);

    *ppos += n_length;

    return n_length;
}

static ssize_t drv_procfs_firmware_update_write(struct file *pfile,
                                                const char __user *pbuffer,
                                                size_t n_count, loff_t *ppos)
{
    s32 n_ret_val = 0;

    DBG(&g_i2c_client->dev, "*** %s() g_fw_data_count = %d ***\n", __func__,
        g_fw_data_count);

    drv_disable_finger_touch_report();

    if (g_chip_type == CHIP_TYPE_MSG22XX || g_chip_type == CHIP_TYPE_MSG28XX
        || g_chip_type == CHIP_TYPE_MSG58XXA || g_chip_type == CHIP_TYPE_ILI2118A
        || g_chip_type == CHIP_TYPE_ILI2117A) {
        n_ret_val = drv_update_firmware(g_fw_data, EMEM_ALL);
    } else if (g_chip_type == CHIP_TYPE_ILI2120 ||
               g_chip_type == CHIP_TYPE_ILI2121) {
        n_ret_val =
            drv_ilitek_convert_firmware_data(g_ilitek_fw_data_buf, g_fw_data_count);

        if (0 == n_ret_val) {
            n_ret_val = drv_ilitek_update_firmware(g_ilitek_fw_data, false);

            drv_ilitek_read_touch_panelInfo(&g_ilitek_tp_info);   /*Update TP panel info for ILI21xx */
        }
    } else {
        DBG(&g_i2c_client->dev,
            "This chip type (0x%x) does not support update firmware by MTPTool APK\n",
            g_chip_type);
        n_ret_val = -1;
    }

    if (0 != n_ret_val) {
        g_is_update_complete = 0;
        DBG(&g_i2c_client->dev, "Update FAILED\n");
    } else {
        g_is_update_complete = 1;
        DBG(&g_i2c_client->dev, "Update SUCCESS\n");
    }

    drv_enable_finger_touch_report();

    return n_count;
}

static ssize_t drv_procfs_customer_firmware_version_read(struct file *pfile,
                                                         char __user *pbuffer,
                                                         size_t n_count,
                                                         loff_t *ppos)
{
    u32 n_length = 0;
    u8 n_user_temp_buffer[16];

    DBG(&g_i2c_client->dev, "*** %s() g_fw_version = %s ***\n", __func__,
        g_fw_version);

    /*If file position is non-zero, then assume the string has been read and indicate there is no more data to be read. */
    if (*ppos != 0)
        return 0;

    n_length = snprintf(n_user_temp_buffer, sizeof(n_user_temp_buffer), "%s", g_fw_version);
    if (copy_to_user(pbuffer, n_user_temp_buffer, n_length)) {
        DBG(&g_i2c_client->dev, "copy to user error\n");
        return -EFAULT;
    }
    *ppos += n_length;

    return n_length;
}

static ssize_t drv_procfs_customer_firmware_version_write(struct file *pfile,
                                                          const char __user *
                                                          pbuffer,
                                                          size_t n_count,
                                                          loff_t *ppos)
{
    u16 nMajor = 0, nMinor = 0;

    drv_get_customer_firmware_version(&nMajor, &nMinor, &g_fw_version);

    DBG(&g_i2c_client->dev, "*** %s() g_fw_version = %s ***\n", __func__,
        g_fw_version);

    return n_count;
}

static ssize_t drv_procfs_platform_firmware_version_read(struct file *pfile,
                                                         char __user *pbuffer,
                                                         size_t n_count,
                                                         loff_t *ppos)
{
    u32 n_length = 0;
    u8 n_user_temp_buffer[16];

    DBG(&g_i2c_client->dev, "*** %s() g_platform_fw_version = %s ***\n",
        __func__, g_platform_fw_version);

    if (*ppos != 0)
        return 0;

    n_length = snprintf(n_user_temp_buffer, sizeof(n_user_temp_buffer), "%s", g_platform_fw_version);
    if (copy_to_user(pbuffer, n_user_temp_buffer, n_length)) {
        DBG(&g_i2c_client->dev, "copy to user error\n");
        return -EFAULT;
    }

    *ppos += n_length;

    return n_length;
}

static ssize_t drv_procfs_platform_firmware_version_write(struct file *pfile,
                                                          const char __user *
                                                          pbuffer,
                                                          size_t n_count,
                                                          loff_t *ppos)
{
    drv_get_platform_firmware_version(&g_platform_fw_version);

    DBG(&g_i2c_client->dev, "*** %s() g_platform_fw_version = %s ***\n",
        __func__, g_platform_fw_version);

    return n_count;
}

static ssize_t drv_procfs_device_driver_version_read(struct file *pfile,
                                                     char __user *pbuffer,
                                                     size_t n_count,
                                                     loff_t *ppos)
{
    u32 n_length = 0;
    u8 n_user_temp_buffer[16];

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    if (*ppos != 0)
        return 0;

    n_length = snprintf(n_user_temp_buffer, sizeof(n_user_temp_buffer), "%s", DEVICE_DRIVER_RELEASE_VERSION);
    if (copy_to_user(pbuffer, n_user_temp_buffer, n_length)) {
        DBG(&g_i2c_client->dev, "copy to user error\n");
        return -EFAULT;
    }

    *ppos += n_length;

    return n_length;
}

static ssize_t drv_procfs_device_driver_version_write(struct file *pfile,
                                                      const char __user *
                                                      pbuffer, size_t n_count,
                                                      loff_t *ppos)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    return n_count;
}

static ssize_t drv_procfs_sd_card_firmware_update_read(struct file *pfile,
                                                       char __user *pbuffer,
                                                       size_t n_count,
                                                       loff_t *ppos)
{
    u16 nMajor = 0, nMinor = 0;
    u32 n_length = 0;
    u8 n_user_temp_buffer[16];

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    if (*ppos != 0)
        return 0;


    drv_get_customer_firmware_version(&nMajor, &nMinor, &g_fw_version);

    DBG(&g_i2c_client->dev, "*** %s() g_fw_version = %s ***\n", __func__,
        g_fw_version);

    n_length = snprintf(n_user_temp_buffer, sizeof(n_user_temp_buffer), "%s\n", g_fw_version);
    if (copy_to_user(pbuffer, n_user_temp_buffer, n_length)) {
        DBG(&g_i2c_client->dev, "copy to user error\n");
        return -EFAULT;
    }

    *ppos += n_length;

    return n_length;
}

static ssize_t drv_procfs_sd_card_firmware_update_write(struct file *pfile,
                                                        const char __user *
                                                        pbuffer, size_t n_count,
                                                        loff_t *ppos)
{
    char *pValid = NULL;
    char *pTmpFilePath = NULL;
    char szFilePath[100] = { 0 };
    char *pStr = NULL;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);
    DBG(&g_i2c_client->dev, "pbuffer = %s\n", pbuffer);
    memset(g_debug_buf, 0, 1024);

    if (copy_from_user(g_debug_buf, pbuffer, n_count)) {
        DBG(&g_i2c_client->dev, "copy_from_user() failed\n");

		return -EFAULT;
    }
    pStr = g_debug_buf;
    if (pStr != NULL) {
		pValid = strnstr(pStr, ".bin", 4);

        if (pValid) {
            pTmpFilePath = strsep((char **)&pStr, ".");

            DBG(&g_i2c_client->dev, "pTmpFilePath = %s\n", pTmpFilePath);

			strlcat(szFilePath, pTmpFilePath, sizeof(szFilePath));
			strlcat(szFilePath, ".bin", sizeof(szFilePath));

            DBG(&g_i2c_client->dev, "szFilePath = %s\n", szFilePath);

            if (0 != drv_check_update_firmware_by_sd_card(szFilePath)) {
                DBG(&g_i2c_client->dev, "Update FAILED\n");
            } else {
                DBG(&g_i2c_client->dev, "Update SUCCESS\n");
            }
        } else {
            DBG(&g_i2c_client->dev,
                "The file type of the update firmware bin file is not a .bin file.\n");
        }
    } else {
        DBG(&g_i2c_client->dev,
            "The file path of the update firmware bin file is NULL.\n");
    }

    return n_count;
}

static ssize_t drv_procfs_selinux_limit_firmware_update_read(struct file *pfile,
                                                        char __user *pbuffer,
                                                        size_t n_count,
                                                        loff_t *ppos)
{
    u32 n_length = 0;
    s32 n_ret_val = 0;
    u8 n_user_temp_buffer[16];

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    if (*ppos != 0)
        return 0;

    if (g_chip_type == CHIP_TYPE_MSG22XX || g_chip_type == CHIP_TYPE_MSG28XX
        || g_chip_type == CHIP_TYPE_MSG58XXA || g_chip_type == CHIP_TYPE_ILI2118A
        || g_chip_type == CHIP_TYPE_ILI2117A) {
        DBG(&g_i2c_client->dev, "FIRMWARE_FILE_PATH_ON_SD_CARD = %s\n",
            FIRMWARE_FILE_PATH_ON_SD_CARD);
        n_ret_val = drv_update_firmware_by_sd_card(FIRMWARE_FILE_PATH_ON_SD_CARD);
    } else if (g_chip_type == CHIP_TYPE_ILI2120 ||
               g_chip_type == CHIP_TYPE_ILI2121) {
        DBG(&g_i2c_client->dev, "ILITEK_FIRMWARE_FILE_PATH_ON_SD_CARD = %s\n",
            ILITEK_FIRMWARE_FILE_PATH_ON_SD_CARD);
        n_ret_val =
            drv_ilitek_update_firmware_by_sd_card
            (ILITEK_FIRMWARE_FILE_PATH_ON_SD_CARD);
    } else {
        DBG(&g_i2c_client->dev,
            "This chip type (0x%x) does not support selinux limit firmware update\n",
            g_chip_type);
        n_ret_val = -1;
    }

    if (0 != n_ret_val) {
        g_is_update_complete = 0;
        DBG(&g_i2c_client->dev, "Update FAILED\n");
    } else {
        g_is_update_complete = 1;
        DBG(&g_i2c_client->dev, "Update SUCCESS\n");
    }

    n_length = snprintf(n_user_temp_buffer, sizeof(n_user_temp_buffer), "%d", g_is_update_complete);
    if (copy_to_user(pbuffer, n_user_temp_buffer, n_length)) {
        DBG(&g_i2c_client->dev, "copy to user error\n");
        return -EFAULT;
    }

    DBG(&g_i2c_client->dev, "*** g_is_update_complete = %d ***\n",
        g_is_update_complete);

    *ppos += n_length;

    return n_length;
}

static ssize_t drv_procfs_force_firmware_update_read(struct file *pfile,
                                                 char __user *pbuffer,
                                                 size_t n_count, loff_t *ppos)
{
    u32 n_length = 0;
    u8 n_user_temp_buffer[16];

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    if (*ppos != 0)
        return 0;


    DBG(&g_i2c_client->dev,
        "*** is_force_to_update_firmware_enabled = %d ***\n",
        is_force_to_update_firmware_enabled);

    is_force_to_update_firmware_enabled = 1;    /*Enable force firmware update */
    g_feature_support_status = is_force_to_update_firmware_enabled;

    n_length = snprintf(n_user_temp_buffer, sizeof(n_user_temp_buffer), "%d", g_feature_support_status);
    if (copy_to_user(pbuffer, n_user_temp_buffer, n_length)) {
        DBG(&g_i2c_client->dev, "copy to user error\n");
        return -EFAULT;
    }

    DBG(&g_i2c_client->dev, "*** g_feature_support_status = %d ***\n",
        g_feature_support_status);

    *ppos += n_length;

    return n_length;
}

static ssize_t drv_procfs_firmware_debug_read(struct file *pfile,
                                              char __user *pbuffer,
                                              size_t n_count, loff_t *ppos)
{
    u32 i, n_length = 0;
    u8 nBank, n_addr;
    u16 szReg_data[MAX_DEBUG_REGISTER_NUM] = { 0 };
    u8 szOut[MAX_DEBUG_REGISTER_NUM * 25] = { 0 }, szValue[10] = {
    0};

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    if (*ppos != 0)
        return 0;

    db_bus_enter_serial_debug_mode();
    db_bus_stop_mcu();
    db_bus_iic_use_bus();
    db_bus_iic_reshape();

    for (i = 0; i < g_debug_reg_count; i++)
        szReg_data[i] = reg_get16_bit_value(g_debug_reg[i]);


    db_bus_iic_not_use_bus();
    db_bus_not_stop_mcu();
    db_bus_exit_serial_debug_mode();

    for (i = 0; i < g_debug_reg_count; i++) {
        nBank = (g_debug_reg[i] >> 8) & 0xFF;
        n_addr = g_debug_reg[i] & 0xFF;

        DBG(&g_i2c_client->dev, "reg(0x%02X,0x%02X)=0x%04X\n", nBank, n_addr,
            szReg_data[i]);

		strlcat(szOut, "reg(", sizeof(szOut));
		snprintf(szValue, sizeof(szValue), "0x%02X", nBank);
		strlcat(szOut, szValue, sizeof(szOut));
		strlcat(szOut, ",", sizeof(szOut));
		snprintf(szValue, sizeof(szValue), "0x%02X", n_addr);
		strlcat(szOut, szValue, sizeof(szOut));
		strlcat(szOut, ")=", sizeof(szOut));
		snprintf(szValue, sizeof(szValue), "0x%04X", szReg_data[i]);
		strlcat(szOut, szValue, sizeof(szOut));
		strlcat(szOut, "\n", sizeof(szOut));
    }

    n_length = strlen(szOut);
    if (copy_to_user(pbuffer, szOut, n_length)) {
        DBG(&g_i2c_client->dev, "copy to user error\n");
        return -EFAULT;
    }

    *ppos += n_length;

    return n_length;
}

static ssize_t drv_procfs_firmware_debug_write(struct file *pfile,
                                               const char __user *pbuffer,
                                               size_t n_count, loff_t *ppos)
{
    u32 i;
    char *pCh = NULL;
    char *pStr = NULL;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);
    memset(g_debug_buf, 0, 1024);
    if (copy_from_user(g_debug_buf, pbuffer, n_count)) {
        DBG(&g_i2c_client->dev, "copy_from_user() failed\n");

        return -EFAULT;
    }
    if (g_debug_buf != NULL) {
        DBG(&g_i2c_client->dev, "***pbuffer[0] = %c ***\n", g_debug_buf[0]);
        DBG(&g_i2c_client->dev, "***pbuffer[1] = %c ***\n", g_debug_buf[1]);
        DBG(&g_i2c_client->dev, "***pbuffer[2] = %c ***\n", g_debug_buf[2]);
        DBG(&g_i2c_client->dev, "***pbuffer[3] = %c ***\n", g_debug_buf[3]);
        DBG(&g_i2c_client->dev, "***pbuffer[4] = %c ***\n", g_debug_buf[4]);
        DBG(&g_i2c_client->dev, "***pbuffer[5] = %c ***\n", g_debug_buf[5]);

        DBG(&g_i2c_client->dev, "n_count = %d\n", (int)n_count);

        g_debug_buf[n_count] = '\0';
        pStr = g_debug_buf;

        i = 0;

        while ((pCh = strsep((char **)&pStr, " ,"))
               && (i < MAX_DEBUG_REGISTER_NUM)) {
            DBG(&g_i2c_client->dev, "pCh = %s\n", pCh);

            g_debug_reg[i] = drv_convert_char_to_hex_digit(pCh, strlen(pCh));

            DBG(&g_i2c_client->dev, "g_debug_reg[%d] = 0x%04X\n", i,
                g_debug_reg[i]);
            i++;
        }
        g_debug_reg_count = i;

        DBG(&g_i2c_client->dev, "g_debug_reg_count = %d\n", g_debug_reg_count);
    }

    return n_count;
}

static ssize_t drv_procfs_firmware_set_debug_value_read(struct file *pfile,
                                                        char __user *pbuffer,
                                                        size_t n_count,
                                                        loff_t *ppos)
{
    u32 i, n_length = 0;
    u8 nBank, n_addr;
    u16 szReg_data[MAX_DEBUG_REGISTER_NUM] = { 0 };
    u8 szOut[MAX_DEBUG_REGISTER_NUM * 25] = { 0 }, szValue[10] = {
    0};

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    if (*ppos != 0)
        return 0;

    db_bus_enter_serial_debug_mode();
    db_bus_stop_mcu();
    db_bus_iic_use_bus();
    db_bus_iic_reshape();

    for (i = 0; i < g_debug_reg_count; i++)
        szReg_data[i] = reg_get16_bit_value(g_debug_reg[i]);


    db_bus_iic_not_use_bus();
    db_bus_not_stop_mcu();
    db_bus_exit_serial_debug_mode();

    for (i = 0; i < g_debug_reg_count; i++) {
        nBank = (g_debug_reg[i] >> 8) & 0xFF;
        n_addr = g_debug_reg[i] & 0xFF;

        DBG(&g_i2c_client->dev, "reg(0x%02X,0x%02X)=0x%04X\n", nBank, n_addr,
            szReg_data[i]);

		strlcat(szOut, "reg(", sizeof(szOut));
		snprintf(szValue, sizeof(szValue), "0x%02X", nBank);
		strlcat(szOut, szValue, sizeof(szOut));
		strlcat(szOut, ",", sizeof(szOut));
		snprintf(szValue, sizeof(szValue), "0x%02X", n_addr);
		strlcat(szOut, szValue, sizeof(szOut));
		strlcat(szOut, ")=", sizeof(szOut));
		snprintf(szValue, sizeof(szValue), "0x%04X", szReg_data[i]);
		strlcat(szOut, szValue, sizeof(szOut));
		strlcat(szOut, "\n", sizeof(szOut));
    }

    n_length = strlen(szOut);
    if (copy_to_user(pbuffer, szOut, n_length)) {
        DBG(&g_i2c_client->dev, "copy to user error\n");
        return -EFAULT;
    }

    *ppos += n_length;

    return n_length;
}

static ssize_t drv_procfs_firmware_set_debug_value_write(struct file *pfile,
                                                         const char __user *
                                                         pbuffer, size_t n_count,
                                                         loff_t *ppos)
{
    u32 i, j, k;
    char *pCh = NULL;
    char *pStr = NULL;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);
    memset(g_debug_buf, 0, 1024);

    if (copy_from_user(g_debug_buf, pbuffer, n_count)) {
        DBG(&g_i2c_client->dev, "copy_from_user() failed\n");

        return -EFAULT;
    }
    if (g_debug_buf != NULL) {
        DBG(&g_i2c_client->dev, "***pbuffer[0] = %c ***\n", g_debug_buf[0]);
        DBG(&g_i2c_client->dev, "***pbuffer[1] = %c ***\n", g_debug_buf[1]);

        DBG(&g_i2c_client->dev, "n_count = %d\n", (int)n_count);
        g_debug_buf[n_count] = '\0';
        pStr = g_debug_buf;

        i = 0;
        j = 0;
        k = 0;

        while ((pCh = strsep((char **)&pStr, " ,")) && (i < 2)) {
            DBG(&g_i2c_client->dev, "pCh = %s\n", pCh);

            if ((i % 2) == 0) {
                g_debug_reg[j] = drv_convert_char_to_hex_digit(pCh, strlen(pCh));
                DBG(&g_i2c_client->dev, "g_debug_reg[%d] = 0x%04X\n", j,
                    g_debug_reg[j]);
                j++;
            } else {            /*(i%2) == 1 */

                g_debug_reg_value[k] =
                    drv_convert_char_to_hex_digit(pCh, strlen(pCh));
                DBG(&g_i2c_client->dev, "g_debug_reg_value[%d] = 0x%04X\n", k,
                    g_debug_reg_value[k]);
                k++;
            }

            i++;
        }
        g_debug_reg_count = j;

        DBG(&g_i2c_client->dev, "g_debug_reg_count = %d\n", g_debug_reg_count);

        db_bus_enter_serial_debug_mode();
        db_bus_stop_mcu();
        db_bus_iic_use_bus();
        db_bus_iic_reshape();

        for (i = 0; i < g_debug_reg_count; i++) {
            reg_set_16bit_value(g_debug_reg[i], g_debug_reg_value[i]);
            DBG(&g_i2c_client->dev, "g_debug_reg[%d] = 0x%04X, g_debug_reg_value[%d] = 0x%04X\n", i, g_debug_reg[i], i, g_debug_reg_value[i]);  /*add for debug */
        }

        db_bus_iic_not_use_bus();
        db_bus_not_stop_mcu();
        db_bus_exit_serial_debug_mode();
    }

    return n_count;
}

static ssize_t drv_procfs_firmware_sm_bus_debug_read(struct file *pfile,
                                                     char __user *pbuffer,
                                                     size_t n_count,
                                                     loff_t *ppos)
{
    u32 i = 0, n_length = 0;
    u8 szSmBusRx_data[MAX_I2C_TRANSACTION_LENGTH_LIMIT] = { 0 };
    u8 szOut[MAX_I2C_TRANSACTION_LENGTH_LIMIT * 2] = { 0 };
    u8 szValue[10] = { 0 };
    s32 rc = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    if (*ppos != 0)
        return 0;

#ifdef CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM
#ifdef CONFIG_ENABLE_DMA_IIC
    dma_reset();
#endif /*CONFIG_ENABLE_DMA_IIC */
#endif /*CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM */

    mutex_lock(&g_mutex);
    DBG(&g_i2c_client->dev, "*** %s() *** mutex_lock(&g_mutex)\n", __func__);   /*add for debug */

    while (i < 5) {
        if (g_debug_cmd_argu_count > 0) {   /*Send write command */
            DBG(&g_i2c_client->dev, "Execute I2C SMBUS write command\n");

            mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
            rc = iic_write_data(slave_i2c_id_dw_i2c, &g_debug_cmd_argu[0],
                              g_debug_cmd_argu_count);
            if (rc > 0) {
                DBG(&g_i2c_client->dev,
                    "iic_write_data(0x%X, 0x%X, %d) success\n",
                    slave_i2c_id_dw_i2c, g_debug_cmd_argu[0],
                    g_debug_cmd_argu_count);

                if (g_debug_read_data_size == 0)
                    break;      /*No need to execute I2C SMBUS read command later. So, break here. */

            }
        }

        if (g_debug_read_data_size > 0) {   /*Send read command */
            DBG(&g_i2c_client->dev, "Execute I2C SMBUS read command\n");

            mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
            rc = iic_read_data(slave_i2c_id_dw_i2c, &szSmBusRx_data[0],
                             g_debug_read_data_size);
            if (rc > 0) {
                DBG(&g_i2c_client->dev, "iic_read_data(0x%X, 0x%X, %d) success\n",
                    slave_i2c_id_dw_i2c, szSmBusRx_data[0],
                    g_debug_read_data_size);
                break;
            }
        }

        i++;
    }
    if (i == 5) {
        DBG(&g_i2c_client->dev,
            "iic_write_data() & iic_read_data() failed, rc = %d\n", rc);
    }

    for (i = 0; i < g_debug_read_data_size; i++) {  /*Output format 2. */
        DBG(&g_i2c_client->dev, "szSmBusRx_data[%d] = 0x%x\n", i,
            szSmBusRx_data[i]);

		snprintf(szValue, sizeof(szValue), "%02x", szSmBusRx_data[i]);
		strlcat(szOut, szValue, sizeof(szOut));

        if (i < (g_debug_read_data_size - 1)) {
			strlcat(szOut, ",", sizeof(szOut));
        }
    }

    DBG(&g_i2c_client->dev, "*** %s() *** mutex_unlock(&g_mutex)\n", __func__); /*add for debug */
    mutex_unlock(&g_mutex);
    n_length = strlen(szOut);
    if (copy_to_user(pbuffer, szOut, n_length)) {
        DBG(&g_i2c_client->dev, "copy to user error\n");
        return -EFAULT;
    }

    *ppos += n_length;

    return n_length;
}

static ssize_t drv_procfs_firmware_sm_bus_debug_write(struct file *pfile,
                                                      const char __user *
                                                      pbuffer, size_t n_count,
                                                      loff_t *ppos)
{
    u32 i, j;
    char szCmdType[5] = { 0 };
    char *pCh = NULL;
    char *pStr = NULL;
    int ret;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);
    memset(g_debug_buf, 0, 1024);

    if (copy_from_user(g_debug_buf, pbuffer, n_count)) {
        DBG(&g_i2c_client->dev, "copy_from_user() failed\n");

        return -EFAULT;
    }
    if (g_debug_buf != NULL) {
        DBG(&g_i2c_client->dev, "***pbuffer[0] = %c ***\n", g_debug_buf[0]);
        DBG(&g_i2c_client->dev, "***pbuffer[1] = %c ***\n", g_debug_buf[1]);
        DBG(&g_i2c_client->dev, "***pbuffer[2] = %c ***\n", g_debug_buf[2]);
        DBG(&g_i2c_client->dev, "***pbuffer[3] = %c ***\n", g_debug_buf[3]);
        DBG(&g_i2c_client->dev, "***pbuffer[4] = %c ***\n", g_debug_buf[4]);
        DBG(&g_i2c_client->dev, "***pbuffer[5] = %c ***\n", g_debug_buf[5]);

        DBG(&g_i2c_client->dev, "n_count = %d\n", (int)n_count);

        /*Reset to 0 before parsing the adb command */
        g_debug_cmd_argu_count = 0;
        g_debug_read_data_size = 0;

        g_debug_buf[n_count] = '\0';
        pStr = g_debug_buf;

        i = 0;
        j = 0;

        while ((pCh = strsep((char **)&pStr, " ,"))
               && (j < MAX_DEBUG_COMMAND_ARGUMENT_NUM)) {
            DBG(&g_i2c_client->dev, "pCh = %s\n", pCh);

            if (strcmp(pCh, "w") == 0 || strcmp(pCh, "r") == 0) {
                memcpy(szCmdType, pCh, strlen(pCh));
            } else if (strcmp(szCmdType, "w") == 0) {
                g_debug_cmd_argu[j] =
                    drv_convert_char_to_hex_digit(pCh, strlen(pCh));
                DBG(&g_i2c_client->dev, "g_debug_cmd_argu[%d] = 0x%02X\n", j,
                    g_debug_cmd_argu[j]);

                j++;

                g_debug_cmd_argu_count = j;
                DBG(&g_i2c_client->dev, "g_debug_cmd_argu_count = %d\n",
                    g_debug_cmd_argu_count);
            } else if (strcmp(szCmdType, "r") == 0) {
                ret = sscanf(pCh, "%d", &g_debug_read_data_size);
                if (ret < 0) {
                    DBG(0, "error\n");
                    return -EFAULT;
                }                

                DBG(&g_i2c_client->dev, "g_debug_read_data_size = %d\n",
                    g_debug_read_data_size);
            } else {
                DBG(&g_i2c_client->dev, "Un-supported adb command format!\n");
            }

            i++;
        }
    }

    return n_count;
}

static ssize_t drv_procfs_firmware_set_dq_mem_value_read(struct file *pfile,
                                                         char __user *pbuffer,
                                                         size_t n_count,
                                                         loff_t *ppos)
{
    u32 i, n_length = 0;
    u8 nBank, n_addr;
    u32 szReg_data[MAX_DEBUG_REGISTER_NUM] = { 0 };
    u8 szOut[MAX_DEBUG_REGISTER_NUM * 25] = { 0 }, szValue[10] = {
    0};

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    /*If file position is non-zero, then assume the string has been read and indicate there is no more data to be read. */
    if (*ppos != 0) {
        return 0;
    }

    for (i = 0; i < g_debug_reg_count; i++) {
        szReg_data[i] = drv_read_dq_mem_value(g_debug_reg[i]);
    }

    for (i = 0; i < g_debug_reg_count; i++) {
        nBank = (g_debug_reg[i] >> 8) & 0xFF;
        n_addr = g_debug_reg[i] & 0xFF;

        DBG(&g_i2c_client->dev, "reg(0x%02X,0x%02X)=0x%08X\n", nBank, n_addr,
            szReg_data[i]);

		strlcat(szOut, "reg(", sizeof(szOut));
		snprintf(szValue, sizeof(szValue), "0x%02X", nBank);
		strlcat(szOut, szValue, sizeof(szOut));
		strlcat(szOut, ",", sizeof(szOut));
		snprintf(szValue, sizeof(szValue), "0x%02X", n_addr);
		strlcat(szOut, szValue, sizeof(szOut));
		strlcat(szOut, ")=", sizeof(szOut));
		snprintf(szValue, sizeof(szValue), "0x%04X", szReg_data[i]);
		strlcat(szOut, szValue, sizeof(szOut));
		strlcat(szOut, "\n", sizeof(szOut));
    }
    n_length = strlen(szOut);
    if (copy_to_user(pbuffer, szOut, n_length)) {
        DBG(&g_i2c_client->dev, "copy to user error\n");
        return -EFAULT;
    }

    *ppos += n_length;

    return n_length;
}

static ssize_t drv_procfs_firmware_set_dq_mem_value_write(struct file *pfile,
                                                          const char __user *
                                                          pbuffer,
                                                          size_t n_count,
                                                          loff_t *ppos)
{
    u32 i, j, k;
    char *pCh = NULL;
    char *pStr = NULL;
    u16 nRealDQMemAddr = 0;
    u32 nRealDQMemValue = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);
    memset(g_debug_buf, 0, 1024);

    if (copy_from_user(g_debug_buf, pbuffer, n_count)) {
        DBG(&g_i2c_client->dev, "copy_from_user() failed\n");

        return -EFAULT;
    }
    if (g_debug_buf != NULL) {
        DBG(&g_i2c_client->dev, "***pbuffer[0] = %c ***\n", g_debug_buf[0]);
        DBG(&g_i2c_client->dev, "***pbuffer[1] = %c ***\n", g_debug_buf[1]);

        DBG(&g_i2c_client->dev, "n_count = %d\n", (int)n_count);
        g_debug_buf[n_count] = '\0';
        pStr = g_debug_buf;

        i = 0;
        j = 0;
        k = 0;

        while ((pCh = strsep((char **)&pStr, " ,")) && (i < 2)) {
            DBG(&g_i2c_client->dev, "pCh = %s\n", pCh);

            if ((i % 2) == 0) {
                g_debug_reg[j] = drv_convert_char_to_hex_digit(pCh, strlen(pCh));
                DBG(&g_i2c_client->dev, "g_debug_reg[%d] = 0x%04X\n", j,
                    g_debug_reg[j]);
                j++;
            } else {            /*(i%2) == 1 */

                g_debug_reg_value[k] =
                    drv_convert_char_to_hex_digit(pCh, strlen(pCh));
                DBG(&g_i2c_client->dev, "g_debug_reg_value[%d] = 0x%04X\n", k,
                    g_debug_reg_value[k]);
                k++;
            }

            i++;
        }
        g_debug_reg_count = j;

        DBG(&g_i2c_client->dev, "g_debug_reg_count = %d\n", g_debug_reg_count);

        if ((g_debug_reg[0] % 4) == 0) {
            nRealDQMemAddr = g_debug_reg[0];
            nRealDQMemValue = drv_read_dq_mem_value(nRealDQMemAddr);
            g_debug_reg[0] = nRealDQMemAddr;
            DBG(&g_i2c_client->dev, "nRealDQMemValue Raw = %X\n",
                nRealDQMemValue);
            nRealDQMemValue &= 0xFFFF0000;
            nRealDQMemValue |= g_debug_reg_value[0];
            DBG(&g_i2c_client->dev, "nRealDQMemValue Modify = %X\n",
                nRealDQMemValue);
            drv_write_dq_mem_value(nRealDQMemAddr, nRealDQMemValue);
        } else if ((g_debug_reg[0] % 4) == 2) {
            nRealDQMemAddr = g_debug_reg[0] - 2;
            nRealDQMemValue = drv_read_dq_mem_value(nRealDQMemAddr);
            g_debug_reg[0] = nRealDQMemAddr;
            DBG(&g_i2c_client->dev, "nRealDQMemValue Raw = %X\n",
                nRealDQMemValue);

            nRealDQMemValue &= 0x0000FFFF;
            nRealDQMemValue |= (g_debug_reg_value[0] << 16);
            DBG(&g_i2c_client->dev, "nRealDQMemValue Modify = %X\n",
                nRealDQMemValue);
            drv_write_dq_mem_value(nRealDQMemAddr, nRealDQMemValue);
        }
    }

    return n_count;
}

/*--------------------------------------------------------------------------*/

#ifdef CONFIG_ENABLE_ITO_MP_TEST
static ssize_t drv_procfs_mp_test_read(struct file *pfile,
                                       char __user *pbuffer, size_t n_count,
                                       loff_t *ppos)
{
    u32 n_length = 0;
    u8 n_user_temp_buffer[16];

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    /*If file position is non-zero, then assume the string has been read and indicate there is no more data to be read. */
    if (*ppos != 0) {
        return 0;
    }

    DBG(&g_i2c_client->dev, "*** ctp mp test status = %d ***\n",
        drv_mp_test_get_test_result());

    n_length = snprintf(n_user_temp_buffer, sizeof(n_user_temp_buffer), "%d", drv_mp_test_get_test_result());
    if (copy_to_user(pbuffer, n_user_temp_buffer, n_length)) {
        DBG(&g_i2c_client->dev, "copy to user error\n");
        return -EFAULT;
    }

    *ppos += n_length;

    return n_length;
}

static ssize_t drv_procfs_mp_test_write(struct file *pfile,
                                        const char __user *pbuffer,
                                        size_t n_count, loff_t *ppos)
{
    u32 nMode = 0;
    u32 i = 0;
    char *pCh = NULL;
    char *pStr = NULL;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);
    memset(g_debug_buf, 0, 1024);

    if (copy_from_user(g_debug_buf, pbuffer, n_count)) {
        DBG(&g_i2c_client->dev, "copy_from_user() failed\n");

        return -EFAULT;
    }
    pStr = g_debug_buf;
    if (pStr != NULL) {
        i = 0;
        while ((pCh = strsep((char **)&pStr, ",")) && (i < 1)) {
            DBG(&g_i2c_client->dev, "pCh = %s\n", pCh);

            nMode = drv_convert_char_to_hex_digit(pCh, strlen(pCh));

            i++;
        }

        DBG(&g_i2c_client->dev, "Mp Test Mode = 0x%x\n", nMode);

        if (nMode == ITO_TEST_MODE_OPEN_TEST) { /*open test */
            g_ito_test_mode = ITO_TEST_MODE_OPEN_TEST;
            drv_mp_test_schedule_mp_test_work(ITO_TEST_MODE_OPEN_TEST);
        } else if (nMode == ITO_TEST_MODE_SHORT_TEST) { /*short test */
            g_ito_test_mode = ITO_TEST_MODE_SHORT_TEST;
            drv_mp_test_schedule_mp_test_work(ITO_TEST_MODE_SHORT_TEST);
        } else if (nMode == ITO_TEST_MODE_WATERPROOF_TEST) {    /*waterproof test */
            g_ito_test_mode = ITO_TEST_MODE_WATERPROOF_TEST;
            drv_mp_test_schedule_mp_test_work(ITO_TEST_MODE_WATERPROOF_TEST);
        } else {
            DBG(&g_i2c_client->dev, "*** Undefined MP Test Mode ***\n");
        }
    }

    return n_count;
}

static ssize_t drv_procfs_mp_test_log_read(struct file *pfile,
                                           char __user *pbuffer, size_t n_count,
                                           loff_t *ppos)
{
    u32 n_length = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);
    memset(g_debug_buf, 0, 1024);

    if (copy_from_user(g_debug_buf, pbuffer, n_count)) {
        DBG(&g_i2c_client->dev, "copy_from_user() failed\n");

        return -EFAULT;
    }
    /*If file position is non-zero, then assume the string has been read and indicate there is no more data to be read. */
    if (*ppos != 0) {
        return 0;
    }

    drv_mp_test_get_test_data_log(g_ito_test_mode, g_debug_buf, &n_length);

    *ppos += n_length;

    return n_length;
}

static ssize_t drv_procfs_mp_test_log_write(struct file *pfile,
                                            const char __user *pbuffer,
                                            size_t n_count, loff_t *ppos)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    return n_count;
}

static ssize_t drv_procfs_mp_test_fail_channel_read(struct file *pfile,
                                                    char __user *pbuffer,
                                                    size_t n_count,
                                                    loff_t *ppos)
{
    u32 n_length = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    /*If file position is non-zero, then assume the string has been read and indicate there is no more data to be read. */
    if (*ppos != 0) {
        return 0;
    }

    drv_mp_test_get_test_fail_channel(g_ito_test_mode, pbuffer, &n_length);

    *ppos += n_length;

    return n_length;
}

static ssize_t drv_procfs_mp_test_fail_channel_write(struct file *pfile,
                                                     const char __user *
                                                     pbuffer, size_t n_count,
                                                     loff_t *ppos)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    return n_count;
}

static ssize_t drv_procfs_mp_test_scope_read(struct file *pfile,
                                             char __user *pbuffer,
                                             size_t n_count, loff_t *ppos)
{
    u32 n_length = 0;
#ifdef CONFIG_ENABLE_CHIP_TYPE_MSG28XX
    u8 n_user_temp_buffer[16];
#endif

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    /*If file position is non-zero, then assume the string has been read and indicate there is no more data to be read. */
    if (*ppos != 0) {
        return 0;
    }
#ifdef CONFIG_ENABLE_CHIP_TYPE_MSG28XX
    if (g_chip_type == CHIP_TYPE_MSG28XX || g_chip_type == CHIP_TYPE_MSG58XXA
        || g_chip_type == CHIP_TYPE_ILI2118A ||
        g_chip_type == CHIP_TYPE_ILI2117A) {
        drv_mp_test_get_test_scope(&g_test_scope_info);

        n_length =
			snprintf(n_user_temp_buffer, sizeof(n_user_temp_buffer), "%d,%d,%d", g_test_scope_info.n_mx,
                    g_test_scope_info.n_my, g_test_scope_info.n_key_num);
        if (copy_to_user(pbuffer, n_user_temp_buffer, n_length)) {
            DBG(&g_i2c_client->dev, "copy to user error\n");
            return -EFAULT;
        }

    }
#endif /*CONFIG_ENABLE_CHIP_TYPE_MSG28XX */

    *ppos += n_length;

    return n_length;
}

static ssize_t drv_procfs_mp_test_scope_write(struct file *pfile,
                                              const char __user *pbuffer,
                                              size_t n_count, loff_t *ppos)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    return n_count;
}

#ifdef CONFIG_ENABLE_CHIP_TYPE_MSG28XX
static ssize_t drv_procfs_mp_test_log_all_read(struct file *pfile,
                                               char __user *pbuffer,
                                               size_t n_count, loff_t *ppos)
{
    u32 n_length = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);
    memset(g_debug_buf, 0, 1024);

    if (copy_from_user(g_debug_buf, pbuffer, n_count)) {
        DBG(&g_i2c_client->dev, "copy_from_user() failed\n");

        return -EFAULT;
    }

    /*If file position is non-zero, then assume the string has been read and indicate there is no more data to be read. */
    if (*ppos != 0) {
        return 0;
    }

    if (g_chip_type == CHIP_TYPE_MSG28XX || g_chip_type == CHIP_TYPE_MSG58XXA
        || g_chip_type == CHIP_TYPE_ILI2118A ||
        g_chip_type == CHIP_TYPE_ILI2117A) {
        drv_mp_test_get_test_log_all(g_debug_buf, &n_length);
    }

    *ppos += n_length;

    return n_length;
}

static ssize_t drv_procfs_mp_test_log_all_write(struct file *pfile,
                                                const char __user *pbuffer,
                                                size_t n_count, loff_t *ppos)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    return n_count;
}
#endif /*CONFIG_ENABLE_CHIP_TYPE_MSG28XX */
#endif /*CONFIG_ENABLE_ITO_MP_TEST */

/*--------------------------------------------------------------------------*/

static ssize_t drv_procfs_firmware_mode_read(struct file *pfile,
                                             char __user *pbuffer,
                                             size_t n_count, loff_t *ppos)
{
    u32 n_length = 0;
    u8 n_user_temp_buffer[16];

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    /*If file position is non-zero, then assume the string has been read and indicate there is no more data to be read. */
    if (*ppos != 0) {
        return 0;
    }

    if (g_chip_type == CHIP_TYPE_MSG28XX || g_chip_type == CHIP_TYPE_MSG58XXA
        || g_chip_type == CHIP_TYPE_ILI2118A || g_chip_type == CHIP_TYPE_ILI2117A
        || g_chip_type == CHIP_TYPE_ILI2120 || g_chip_type == CHIP_TYPE_ILI2121) {
        drv_mutual_get_firmware_info(&g_mutual_firmware_info);
        mutex_lock(&g_mutex_protect);
            g_firmware_mode = g_mutual_firmware_info.n_firmware_mode;
        mutex_unlock(&g_mutex_protect);
            DBG(&g_i2c_client->dev, "%s() firmware mode = 0x%x\n", __func__,
                g_mutual_firmware_info.n_firmware_mode);

        n_length =
			snprintf(n_user_temp_buffer, sizeof(n_user_temp_buffer), "%x", g_mutual_firmware_info.n_firmware_mode);
    } else if (g_chip_type == CHIP_TYPE_MSG22XX && is_self_freq_scan_enabled) {
        drv_self_get_firmware_info(&g_self_firmware_info);
        mutex_lock(&g_mutex_protect);
            g_firmware_mode = g_self_firmware_info.n_firmware_mode;
        mutex_unlock(&g_mutex_protect);
            g_mutual_firmware_info.n_firmware_mode = g_firmware_mode;
        g_mutual_firmware_info.n_log_mode_packet_header = 0;
        g_mutual_firmware_info.n_log_mode_packet_length = 0;
        g_mutual_firmware_info.n_type = 0;
        g_mutual_firmware_info.n_my = 0;
        g_mutual_firmware_info.n_mx = 0;
        g_mutual_firmware_info.n_sd = 0;
        g_mutual_firmware_info.n_ss = 0;
        DBG(&g_i2c_client->dev, "%d,%d,%d,%d\n", g_mutual_firmware_info.n_mx,
            g_mutual_firmware_info.n_my, g_mutual_firmware_info.n_ss,
            g_mutual_firmware_info.n_sd);
        DBG(&g_i2c_client->dev,
            "%s() firmware mode = 0x%x, can change firmware mode = %d\n",
            __func__, g_self_firmware_info.n_firmware_mode,
            g_self_firmware_info.n_ls_can_change_firmware_mode);
        n_length =
			snprintf(n_user_temp_buffer, sizeof(n_user_temp_buffer), "%x", g_mutual_firmware_info.n_firmware_mode);
    } else if (g_chip_type == CHIP_TYPE_MSG22XX) {
        drv_self_get_firmware_info(&g_self_firmware_info);
        mutex_lock(&g_mutex_protect);
            g_firmware_mode = g_self_firmware_info.n_firmware_mode;
        mutex_unlock(&g_mutex_protect);
            DBG(&g_i2c_client->dev,
                "%s() firmware mode = 0x%x, can change firmware mode = %d\n",
                __func__, g_self_firmware_info.n_firmware_mode,
                g_self_firmware_info.n_ls_can_change_firmware_mode);

        n_length =
			snprintf(n_user_temp_buffer, sizeof(n_user_temp_buffer), "%x,%d", g_self_firmware_info.n_firmware_mode,
                    g_self_firmware_info.n_ls_can_change_firmware_mode);
    }
    if (copy_to_user(pbuffer, n_user_temp_buffer, n_length)) {
        DBG(&g_i2c_client->dev, "copy to user error\n");
        return -EFAULT;
    }
    *ppos += n_length;

    return n_length;
}

static ssize_t drv_procfs_firmware_mode_write(struct file *pfile,
                                              const char __user *pbuffer,
                                              size_t n_count, loff_t *ppos)
{
    u32 nMode;
    int ret = 0;

    memset(g_debug_buf, 0, 16);

    if (copy_from_user(g_debug_buf, pbuffer, n_count)) {
        DBG(&g_i2c_client->dev, "copy_from_user() failed\n");

        return -EFAULT;
    }
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    if (g_debug_buf != NULL) {
        ret = sscanf(g_debug_buf, "%x", &nMode);

        if (ret < 0) {
            DBG(0, "error\n");
            return -EFAULT;
        }        
        DBG(&g_i2c_client->dev, "firmware mode = 0x%x\n", nMode);

        g_is_switch_mode_by_apk = 0;

        if (nMode == firmware_mode_demo_mode) { /*demo mode */
            g_firmware_mode = drv_change_firmware_mode(firmware_mode_demo_mode);
        } else if (nMode == firmware_mode_debug_mode) { /*debug mode */
            g_firmware_mode = drv_change_firmware_mode(firmware_mode_debug_mode);
            g_is_switch_mode_by_apk = 1;
            g_ndebug_log_times_stamp = 0;   /*Set g_ndebug_log_times_stamp for filter duplicate packet on MTPTool APK */
        } else if (nMode == firmware_mode_raw_data_mode) {  /*raw data mode */
            if (g_chip_type == CHIP_TYPE_MSG22XX) {
                g_firmware_mode =
                    drv_change_firmware_mode(firmware_mode_raw_data_mode);
                g_is_switch_mode_by_apk = 1;
            }
        } else {
            DBG(&g_i2c_client->dev, "*** Undefined Firmware Mode ***\n");
        }
    }

    DBG(&g_i2c_client->dev, "*** g_firmware_mode = 0x%x ***\n", g_firmware_mode);

    return n_count;
}

static ssize_t drv_procfs_firmware_sensor_read(struct file *pfile,
                                               char __user *pbuffer,
                                               size_t n_count, loff_t *ppos)
{
    u32 n_length = 0;
    u8 n_user_temp_buffer[16];

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    /*If file position is non-zero, then assume the string has been read and indicate there is no more data to be read. */
    if (*ppos != 0) {
        return 0;
    }

    if (g_chip_type == CHIP_TYPE_MSG28XX || g_chip_type == CHIP_TYPE_MSG58XXA
        || g_chip_type == CHIP_TYPE_ILI2118A || g_chip_type == CHIP_TYPE_ILI2117A
        || g_chip_type == CHIP_TYPE_ILI2120 || g_chip_type == CHIP_TYPE_ILI2121) {
        if (g_mutual_firmware_info.n_log_mode_packet_header == 0xA7) {
            n_length =
				snprintf(n_user_temp_buffer, sizeof(n_user_temp_buffer), "%d,%d,%d,%d",
                        g_mutual_firmware_info.n_mx, g_mutual_firmware_info.n_my,
                        g_mutual_firmware_info.n_ss, g_mutual_firmware_info.n_sd);
        } else {
            DBG(&g_i2c_client->dev,
                "Undefined debug mode packet format : 0x%x\n",
                g_mutual_firmware_info.n_log_mode_packet_header);
            n_length = 0;
        }
    } else if (g_chip_type == CHIP_TYPE_MSG22XX) {
        n_length =
			snprintf(n_user_temp_buffer, sizeof(n_user_temp_buffer), "%d",
                    g_self_firmware_info.n_log_mode_packet_length);
    }
    if (copy_to_user(pbuffer, n_user_temp_buffer, n_length)) {
        DBG(&g_i2c_client->dev, "copy to user error\n");
        return -EFAULT;
    }
    *ppos += n_length;

    return n_length;
}

static ssize_t drv_procfs_firmware_sensor_write(struct file *pfile,
                                                const char __user *pbuffer,
                                                size_t n_count, loff_t *ppos)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    return n_count;
}

static ssize_t drv_procfs_firmware_packet_header_read(struct file *pfile,
                                                      char __user *pbuffer,
                                                      size_t n_count,
                                                      loff_t *ppos)
{
    u32 n_length = 0;
    u8 n_user_temp_buffer[16];

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    /*If file position is non-zero, then assume the string has been read and indicate there is no more data to be read. */
    if (*ppos != 0) {
        return 0;
    }

    if (g_chip_type == CHIP_TYPE_MSG28XX || g_chip_type == CHIP_TYPE_MSG58XXA
        || g_chip_type == CHIP_TYPE_ILI2118A || g_chip_type == CHIP_TYPE_ILI2117A
        || g_chip_type == CHIP_TYPE_ILI2120 || g_chip_type == CHIP_TYPE_ILI2121) {
        n_length =
			snprintf(n_user_temp_buffer, sizeof(n_user_temp_buffer), "%d",
                    g_mutual_firmware_info.n_log_mode_packet_header);

    } else if (g_chip_type == CHIP_TYPE_MSG22XX) {
        n_length =
			snprintf(n_user_temp_buffer, sizeof(n_user_temp_buffer), "%d",
                    g_self_firmware_info.n_log_mode_packet_header);
    }
    if (copy_to_user(pbuffer, n_user_temp_buffer, n_length)) {
        DBG(&g_i2c_client->dev, "copy to user error\n");
        return -EFAULT;
    }
    *ppos += n_length;

    return n_length;
}

static ssize_t drv_procfs_firmware_packet_header_write(struct file *pfile,
                                                       const char __user *
                                                       pbuffer, size_t n_count,
                                                       loff_t *ppos)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    return n_count;
}

static ssize_t drv_kobject_packet_show(struct kobject *pkobj,
                                       struct kobj_attribute *pattr, char *p_buf)
{
    u32 n_length = 0;
    u32 i = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    if (strcmp(pattr->attr.name, "packet") == 0) {
        if (g_firmware_mode == firmware_mode_demo_mode) {
            if (g_demo_mode_packet != NULL) {
                DBG(&g_i2c_client->dev,
                    "g_firmware_mode=%x, g_demo_mode_packet[0]=%x, g_demo_mode_packet[1]=%x\n",
                    g_firmware_mode, g_demo_mode_packet[0], g_demo_mode_packet[1]);
                DBG(&g_i2c_client->dev,
                    "g_demo_mode_packet[2]=%x, g_demo_mode_packet[3]=%x\n",
                    g_demo_mode_packet[2], g_demo_mode_packet[3]);
                DBG(&g_i2c_client->dev,
                    "g_demo_mode_packet[4]=%x, g_demo_mode_packet[5]=%x\n",
                    g_demo_mode_packet[4], g_demo_mode_packet[5]);

                memcpy(p_buf, g_demo_mode_packet, demo_mode_packet_length);

                n_length = demo_mode_packet_length;
                DBG(&g_i2c_client->dev, "n_length = %d\n", n_length);
            } else {
                DBG(&g_i2c_client->dev, "g_demo_mode_packet is NULL\n");
            }
        } else {                /*g_firmware_mode == firmware_mode_debug_mode || g_firmware_mode == firmware_mode_raw_data_mode */

            if (g_log_mode_packet != NULL) {
                DBG(&g_i2c_client->dev,
                    "g_firmware_mode=%x, g_log_mode_packet[0]=%x, g_log_mode_packet[1]=%x\n",
                    g_firmware_mode, g_log_mode_packet[0], g_log_mode_packet[1]);
                DBG(&g_i2c_client->dev,
                    "g_log_mode_packet[2]=%x, g_log_mode_packet[3]=%x\n",
                    g_log_mode_packet[2], g_log_mode_packet[3]);
                DBG(&g_i2c_client->dev,
                    "g_log_mode_packet[4]=%x, g_log_mode_packet[5]=%x\n",
                    g_log_mode_packet[4], g_log_mode_packet[5]);

                if ((g_chip_type == CHIP_TYPE_MSG28XX
                     || g_chip_type == CHIP_TYPE_MSG58XXA
                     || g_chip_type == CHIP_TYPE_ILI2118A
                     || g_chip_type == CHIP_TYPE_ILI2117A
                     || g_chip_type == CHIP_TYPE_ILI2120 ||
                     g_chip_type == CHIP_TYPE_ILI2121)
                    && (g_firmware_mode == firmware_mode_debug_mode)
                    && (g_log_mode_packet[0] == 0xA7)) {
                    memcpy(p_buf, g_log_mode_packet,
                           g_mutual_firmware_info.n_log_mode_packet_length);

                    if (g_ndebug_log_times_stamp >= 255) {
                        g_ndebug_log_times_stamp = 0;
                    } else {
                        g_ndebug_log_times_stamp++;
                    }

                    p_buf[g_mutual_firmware_info.n_log_mode_packet_length] =
                        g_ndebug_log_times_stamp;
                    DBG(&g_i2c_client->dev, "g_ndebug_log_times_stamp=%d\n", p_buf[g_mutual_firmware_info.n_log_mode_packet_length]);  /*TODO : add for debug */

                    n_length = g_mutual_firmware_info.n_log_mode_packet_length + 1;
                    DBG(&g_i2c_client->dev, "n_length = %d\n", n_length);
                } else if ((g_chip_type == CHIP_TYPE_MSG22XX)
                           && (g_firmware_mode == firmware_mode_debug_mode
                               || g_firmware_mode == firmware_mode_raw_data_mode)
                           && (g_log_mode_packet[0] == 0x62)) {
                    memcpy(p_buf, g_log_mode_packet,
                           g_self_firmware_info.n_log_mode_packet_length);

                    n_length = g_self_firmware_info.n_log_mode_packet_length;
                    DBG(&g_i2c_client->dev, "n_length = %d\n", n_length);
                } else if ((g_chip_type == CHIP_TYPE_MSG22XX) && (g_firmware_mode == firmware_mode_debug_mode) && is_self_freq_scan_enabled) {    /*&& (g_log_mode_packet[0] == 0xA7) */
                    for (i = 0; i < SELF_FREQ_SCAN_PACKET_LENGTH + 30; i++) {
                        if (i < 5) {
                            p_buf[i] = g_log_mode_packet[i];
                        } else if (i < (5 + 30)) {
                            p_buf[i] = 0;
                        } else {
                            p_buf[i] = g_log_mode_packet[i - 30];
                        }
                        DBG(0, "0x%x,", p_buf[i]);
                    }
                    DBG(0, "\n");
                    /*memcpy(p_buf, g_log_mode_packet, SELF_FREQ_SCAN_PACKET_LENGTH); */

                    n_length = SELF_FREQ_SCAN_PACKET_LENGTH;
                    DBG(&g_i2c_client->dev, "n_length = %d\n", n_length);
                } else {
                    DBG(&g_i2c_client->dev,
                        "CURRENT MODE IS NOT DEBUG MODE/WRONG DEBUG MODE HEADER\n");
                }
            } else {
                DBG(&g_i2c_client->dev, "g_log_mode_packet is NULL\n");
            }
        }
    } else {
        DBG(&g_i2c_client->dev, "pattr->attr.name = %s\n", pattr->attr.name);
    }

    return n_length;
}

static ssize_t drv_kobject_packet_store(struct kobject *pkobj,
                                        struct kobj_attribute *pattr,
                                        const char *p_buf, size_t n_count)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

/*
    if (strcmp(pattr->attr.name, "packet") == 0)
    {

    }
*/
    return n_count;
}

static struct kobj_attribute packet_attr =
__ATTR(packet, 0664, drv_kobject_packet_show, drv_kobject_packet_store);

/* Create a group of attributes so that we can create and destroy them all at once. */
static struct attribute *attrs[] = {
    &packet_attr.attr,
    NULL,                       /* need to NULL terminate the list of attributes */
};

/*
 * An unnamed attribute group will put all of the attributes directly in
 * the kobject directory. If we specify a name, a subdirectory will be
 * created for the attributes with the directory being the name of the
 * attribute group.
 */
static struct attribute_group attr_group = {
    .attrs = attrs,
};

/*---------------------------------------------------------------------------*/
static ssize_t drv_procfs_query_feature_support_status_read(struct file *pfile,
                                                            char __user *
                                                            pbuffer,
                                                            size_t n_count,
                                                            loff_t *ppos)
{
    u32 n_length = 0;
    u8 n_user_temp_buffer[16];

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    /*If file position is non-zero, then assume the string has been read and indicate there is no more data to be read. */
    if (*ppos != 0) {
        return 0;
    }

    n_length = snprintf(n_user_temp_buffer, sizeof(n_user_temp_buffer), "%d", g_feature_support_status);
    if (copy_to_user(pbuffer, n_user_temp_buffer, n_length)) {
        DBG(&g_i2c_client->dev, "copy to user error\n");
        return -EFAULT;
    }

    DBG(&g_i2c_client->dev, "*** g_feature_support_status = %d ***\n",
        g_feature_support_status);

    *ppos += n_length;

    return n_length;
}

static ssize_t drv_procfs_query_feature_support_status_write(struct file *pfile,
                                                             const char __user *
                                                             pbuffer,
                                                             size_t n_count,
                                                             loff_t *ppos)
{
    u32 nFeature;
    int ret;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);
    memset(g_debug_buf, 0, 1024);
    if (copy_from_user(g_debug_buf, pbuffer, n_count)) {
        DBG(&g_i2c_client->dev, "copy_from_user() failed\n");

        return -EFAULT;
    }
    if (g_debug_buf != NULL) {
        ret = sscanf(g_debug_buf, "%x", &nFeature);
        if (ret < 0) {
            DBG(0, "error\n");
            return -EFAULT;
        }   
        DBG(&g_i2c_client->dev, "nFeature = 0x%x\n", nFeature);

        if (nFeature == FEATURE_GESTURE_WAKEUP_MODE) {
            g_feature_support_status = is_gesture_wakeup_enabled;
        } else if (nFeature == FEATURE_GESTURE_DEBUG_MODE) {
            g_feature_support_status = is_gesture_debug_mode_enabled;
        } else if (nFeature == FEATURE_GESTURE_INFORMATION_MODE) {
            g_feature_support_status = is_gesture_information_mode_enabled;
        } else if (nFeature == FEATURE_TOUCH_DRIVER_DEBUG_LOG) {
            g_feature_support_status = touch_driver_debug_log_level;
        } else if (nFeature == FEATURE_FIRMWARE_DATA_LOG) {
            g_feature_support_status = is_firmware_data_log_enabled;

#ifdef CONFIG_ENABLE_SEGMENT_READ_FINGER_TOUCH_DATA
            if (g_chip_type == CHIP_TYPE_MSG28XX
                || g_chip_type == CHIP_TYPE_MSG58XXA
                || g_chip_type == CHIP_TYPE_ILI2118A
                || g_chip_type == CHIP_TYPE_ILI2117A
                || g_chip_type == CHIP_TYPE_ILI2120 ||
                g_chip_type == CHIP_TYPE_ILI2121) {
                if (g_feature_support_status == 1) { 
                    drv_get_touch_packet_address(&g_fw_packet_data_address,
                                              &g_fw_packet_flag_address);
                }
            }
#endif /*CONFIG_ENABLE_SEGMENT_READ_FINGER_TOUCH_DATA */
        } else if (nFeature == FEATURE_FORCE_TO_UPDATE_FIRMWARE) {
            g_feature_support_status = is_force_to_update_firmware_enabled;
        } else if (nFeature == FEATURE_DISABLE_ESD_PROTECTION_CHECK) {
            g_feature_support_status = is_disable_esd_protection_check;
        } else if (nFeature == FEATURE_APK_PRINT_FIRMWARE_SPECIFIC_LOG) {
            g_feature_support_status =
                is_apk_print_firmware_specific_log_enabled;
        } else if (nFeature == FEATURE_SELF_FREQ_SCAN) {
            DBG(&g_i2c_client->dev,
                "*** change to  FEATURE_SELF_FREQ_SCAN ***\n");
            g_feature_support_status = is_self_freq_scan_enabled;
        } else {
            DBG(&g_i2c_client->dev, "*** Undefined Feature ***\n");
        }
    }

    DBG(&g_i2c_client->dev, "*** g_feature_support_status = %d ***\n",
        g_feature_support_status);

    return n_count;
}

static ssize_t drv_procfs_change_feature_support_status_read(struct file *pfile,
                                                             char __user *
                                                             pbuffer,
                                                             size_t n_count,
                                                             loff_t *ppos)
{
    u32 n_length = 0;
    u8 n_user_temp_buffer[16];

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    /*If file position is non-zero, then assume the string has been read and indicate there is no more data to be read. */
    if (*ppos != 0) {
        return 0;
    }

    n_length = snprintf(n_user_temp_buffer, sizeof(n_user_temp_buffer), "%d", g_feature_support_status);
    if (copy_to_user(pbuffer, n_user_temp_buffer, n_length)) {
        DBG(&g_i2c_client->dev, "copy to user error\n");
        return -EFAULT;
    }

    DBG(&g_i2c_client->dev, "*** g_feature_support_status = %d ***\n",
        g_feature_support_status);

    *ppos += n_length;

    return n_length;
}

static ssize_t drv_procfs_change_feature_support_status_write(struct file
                                                              *pfile,
                                                              const char __user
                                                              *pbuffer,
                                                              size_t n_count,
                                                              loff_t *ppos)
{
    u32 i;
    u32 nFeature = 0, nNewValue = 0;
    char *pCh = NULL;
    char *pStr = NULL;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);
    memset(g_debug_buf, 0, 1024);

    if (copy_from_user(g_debug_buf, pbuffer, n_count)) {
        DBG(&g_i2c_client->dev, "copy_from_user() failed\n");

        return -EFAULT;
    }
    if (g_debug_buf != NULL) {
        DBG(&g_i2c_client->dev, "n_count = %d\n", (int)n_count);
        g_debug_buf[n_count] = '\0';
        pStr = g_debug_buf;

        i = 0;

        while ((pCh = strsep((char **)&pStr, " ,")) && (i < 3)) {
            DBG(&g_i2c_client->dev, "pCh = %s\n", pCh);

            if (i == 0) {
                nFeature = drv_convert_char_to_hex_digit(pCh, strlen(pCh));
                DBG(&g_i2c_client->dev, "nFeature = 0x%04X\n", nFeature);
            } else if (i == 1) {
                nNewValue = drv_convert_char_to_hex_digit(pCh, strlen(pCh));
                DBG(&g_i2c_client->dev, "nNewValue = %d\n", nNewValue);
            } else {
                DBG(&g_i2c_client->dev, "End of parsing adb command.\n");
            }

            i++;
        }
        if (nFeature == FEATURE_GESTURE_WAKEUP_MODE) {
            is_gesture_wakeup_enabled = nNewValue;
            g_feature_support_status = is_gesture_wakeup_enabled;
        } else if (nFeature == FEATURE_GESTURE_DEBUG_MODE) {
            is_gesture_debug_mode_enabled = nNewValue;
            g_feature_support_status = is_gesture_debug_mode_enabled;
        } else if (nFeature == FEATURE_GESTURE_INFORMATION_MODE) {
            is_gesture_information_mode_enabled = nNewValue;
            g_feature_support_status = is_gesture_information_mode_enabled;
        } else if (nFeature == FEATURE_TOUCH_DRIVER_DEBUG_LOG) {
            touch_driver_debug_log_level = nNewValue;
            g_feature_support_status = touch_driver_debug_log_level;
        } else if (nFeature == FEATURE_FIRMWARE_DATA_LOG) {
            is_firmware_data_log_enabled = nNewValue;
            g_feature_support_status = is_firmware_data_log_enabled;
        } else if (nFeature == FEATURE_FORCE_TO_UPDATE_FIRMWARE) {
            is_force_to_update_firmware_enabled = nNewValue;
            g_feature_support_status = is_force_to_update_firmware_enabled;
        } else if (nFeature == FEATURE_DISABLE_ESD_PROTECTION_CHECK) {
            is_disable_esd_protection_check = nNewValue;
            g_feature_support_status = is_disable_esd_protection_check;
        } else if (nFeature == FEATURE_APK_PRINT_FIRMWARE_SPECIFIC_LOG) {
            is_apk_print_firmware_specific_log_enabled = nNewValue;
            g_feature_support_status =
                is_apk_print_firmware_specific_log_enabled;
        } else if (nFeature == FEATURE_SELF_FREQ_SCAN) {
            is_self_freq_scan_enabled = nNewValue;
            g_feature_support_status = is_self_freq_scan_enabled;
        } else {
            DBG(&g_i2c_client->dev, "*** Undefined Feature ***\n");
        }

        DBG(&g_i2c_client->dev, "*** g_feature_support_status = %d ***\n",
            g_feature_support_status);
    }

    return n_count;
}

/*---------------------------------------------------------------------------*/
#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
static ssize_t drv_procfs_gesture_wakeup_mode_read(struct file *pfile,
                                                   char __user *pbuffer,
                                                   size_t n_count, loff_t *ppos)
{
    u32 n_length = 0;
    u8 n_user_temp_buffer[16];

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    /*If file position is non-zero, then assume the string has been read and indicate there is no more data to be read. */
    if (*ppos != 0) {
        return 0;
    }
#ifdef CONFIG_SUPPORT_64_TYPES_GESTURE_WAKEUP_MODE
    DBG(&g_i2c_client->dev, "g_gesture_wakeup_mode = 0x%x, 0x%x\n",
        g_gesture_wakeup_mode[0], g_gesture_wakeup_mode[1]);

    n_length =
		snprintf(n_user_temp_buffer, sizeof(n_user_temp_buffer), "%x,%x", g_gesture_wakeup_mode[0],
                g_gesture_wakeup_mode[1]);
#else
    DBG(&g_i2c_client->dev, "g_gesture_wakeup_mode = 0x%x\n",
        g_gesture_wakeup_mode[0]);

    n_length = snprintf(n_user_temp_buffer, sizeof(n_user_temp_buffer), "%x", g_gesture_wakeup_mode[0]);
#endif /*CONFIG_SUPPORT_64_TYPES_GESTURE_WAKEUP_MODE */
    if (copy_to_user(pbuffer, n_user_temp_buffer, n_length)) {
        DBG(&g_i2c_client->dev, "copy to user error\n");
        return -EFAULT;
    }

    *ppos += n_length;

    return n_length;
}

static ssize_t drv_procfs_gesture_wakeup_mode_write(struct file *pfile,
                                                    const char __user *pbuffer,
                                                    size_t n_count,
                                                    loff_t *ppos)
{
    u32 n_length;
    u32 nWakeupMode[2] = { 0 };
    int ret;
    
    char *pStr = NULL;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);
    memset(g_debug_buf, 0, 1024);
    if (copy_from_user(g_debug_buf, pbuffer, n_count)) {
        DBG(&g_i2c_client->dev, "copy_from_user() failed\n");

        return -EFAULT;
    }
    pStr = g_debug_buf;
    if (pStr != NULL) {
#ifdef CONFIG_SUPPORT_64_TYPES_GESTURE_WAKEUP_MODE
        u32 i;
        char *pCh;

        i = 0;
        while ((pCh = strsep((char **)&pStr, " ,")) && (i < 2)) {
            DBG(&g_i2c_client->dev, "pCh = %s\n", pCh);

            nWakeupMode[i] = drv_convert_char_to_hex_digit(pCh, strlen(pCh));

            DBG(&g_i2c_client->dev, "nWakeupMode[%d] = 0x%04X\n", i,
                nWakeupMode[i]);
            i++;
        }
#else
        ret = sscanf(g_debug_buf, "%x", &nWakeupMode[0]);
        if (ret < 0) {
            DBG(0, "error\n");
            return -EFAULT;
        } 
        DBG(&g_i2c_client->dev, "nWakeupMode = 0x%x\n", nWakeupMode[0]);
#endif /*CONFIG_SUPPORT_64_TYPES_GESTURE_WAKEUP_MODE */

        n_length = n_count;
        DBG(&g_i2c_client->dev, "n_length = %d\n", n_length);

        if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_DOUBLE_CLICK_FLAG) ==
            GESTURE_WAKEUP_MODE_DOUBLE_CLICK_FLAG) {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] |
                GESTURE_WAKEUP_MODE_DOUBLE_CLICK_FLAG;
        } else {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] &
                (~GESTURE_WAKEUP_MODE_DOUBLE_CLICK_FLAG);
        }

        if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_UP_DIRECT_FLAG) ==
            GESTURE_WAKEUP_MODE_UP_DIRECT_FLAG) {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] | GESTURE_WAKEUP_MODE_UP_DIRECT_FLAG;
        } else {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] &
                (~GESTURE_WAKEUP_MODE_UP_DIRECT_FLAG);
        }

        if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_DOWN_DIRECT_FLAG) ==
            GESTURE_WAKEUP_MODE_DOWN_DIRECT_FLAG) {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] | GESTURE_WAKEUP_MODE_DOWN_DIRECT_FLAG;
        } else {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] &
                (~GESTURE_WAKEUP_MODE_DOWN_DIRECT_FLAG);
        }

        if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_LEFT_DIRECT_FLAG) ==
            GESTURE_WAKEUP_MODE_LEFT_DIRECT_FLAG) {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] | GESTURE_WAKEUP_MODE_LEFT_DIRECT_FLAG;
        } else {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] &
                (~GESTURE_WAKEUP_MODE_LEFT_DIRECT_FLAG);
        }

        if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_RIGHT_DIRECT_FLAG) ==
            GESTURE_WAKEUP_MODE_RIGHT_DIRECT_FLAG) {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] |
                GESTURE_WAKEUP_MODE_RIGHT_DIRECT_FLAG;
        } else {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] &
                (~GESTURE_WAKEUP_MODE_RIGHT_DIRECT_FLAG);
        }

        if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_m_CHARACTER_FLAG) ==
            GESTURE_WAKEUP_MODE_m_CHARACTER_FLAG) {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] | GESTURE_WAKEUP_MODE_m_CHARACTER_FLAG;
        } else {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] &
                (~GESTURE_WAKEUP_MODE_m_CHARACTER_FLAG);
        }

        if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_W_CHARACTER_FLAG) ==
            GESTURE_WAKEUP_MODE_W_CHARACTER_FLAG) {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] | GESTURE_WAKEUP_MODE_W_CHARACTER_FLAG;
        } else {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] &
                (~GESTURE_WAKEUP_MODE_W_CHARACTER_FLAG);
        }

        if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_C_CHARACTER_FLAG) ==
            GESTURE_WAKEUP_MODE_C_CHARACTER_FLAG) {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] | GESTURE_WAKEUP_MODE_C_CHARACTER_FLAG;
        } else {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] &
                (~GESTURE_WAKEUP_MODE_C_CHARACTER_FLAG);
        }

        if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_e_CHARACTER_FLAG) ==
            GESTURE_WAKEUP_MODE_e_CHARACTER_FLAG) {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] | GESTURE_WAKEUP_MODE_e_CHARACTER_FLAG;
        } else {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] &
                (~GESTURE_WAKEUP_MODE_e_CHARACTER_FLAG);
        }

        if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_V_CHARACTER_FLAG) ==
            GESTURE_WAKEUP_MODE_V_CHARACTER_FLAG) {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] | GESTURE_WAKEUP_MODE_V_CHARACTER_FLAG;
        } else {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] &
                (~GESTURE_WAKEUP_MODE_V_CHARACTER_FLAG);
        }

        if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_O_CHARACTER_FLAG) ==
            GESTURE_WAKEUP_MODE_O_CHARACTER_FLAG) {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] | GESTURE_WAKEUP_MODE_O_CHARACTER_FLAG;
        } else {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] &
                (~GESTURE_WAKEUP_MODE_O_CHARACTER_FLAG);
        }

        if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_S_CHARACTER_FLAG) ==
            GESTURE_WAKEUP_MODE_S_CHARACTER_FLAG) {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] | GESTURE_WAKEUP_MODE_S_CHARACTER_FLAG;
        } else {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] &
                (~GESTURE_WAKEUP_MODE_S_CHARACTER_FLAG);
        }

        if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_Z_CHARACTER_FLAG) ==
            GESTURE_WAKEUP_MODE_Z_CHARACTER_FLAG) {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] | GESTURE_WAKEUP_MODE_Z_CHARACTER_FLAG;
        } else {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] &
                (~GESTURE_WAKEUP_MODE_Z_CHARACTER_FLAG);
        }

        if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_RESERVE1_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE1_FLAG) {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] | GESTURE_WAKEUP_MODE_RESERVE1_FLAG;
        } else {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] & (~GESTURE_WAKEUP_MODE_RESERVE1_FLAG);
        }

        if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_RESERVE2_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE2_FLAG) {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] | GESTURE_WAKEUP_MODE_RESERVE2_FLAG;
        } else {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] & (~GESTURE_WAKEUP_MODE_RESERVE2_FLAG);
        }

        if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_RESERVE3_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE3_FLAG) {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] | GESTURE_WAKEUP_MODE_RESERVE3_FLAG;
        } else {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] & (~GESTURE_WAKEUP_MODE_RESERVE3_FLAG);
        }

#ifdef CONFIG_SUPPORT_64_TYPES_GESTURE_WAKEUP_MODE
        if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_RESERVE4_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE4_FLAG) {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] | GESTURE_WAKEUP_MODE_RESERVE4_FLAG;
        } else {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] & (~GESTURE_WAKEUP_MODE_RESERVE4_FLAG);
        }

        if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_RESERVE5_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE5_FLAG) {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] | GESTURE_WAKEUP_MODE_RESERVE5_FLAG;
        } else {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] & (~GESTURE_WAKEUP_MODE_RESERVE5_FLAG);
        }

        if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_RESERVE6_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE6_FLAG) {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] | GESTURE_WAKEUP_MODE_RESERVE6_FLAG;
        } else {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] & (~GESTURE_WAKEUP_MODE_RESERVE6_FLAG);
        }

        if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_RESERVE7_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE7_FLAG) {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] | GESTURE_WAKEUP_MODE_RESERVE7_FLAG;
        } else {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] & (~GESTURE_WAKEUP_MODE_RESERVE7_FLAG);
        }

        if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_RESERVE8_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE8_FLAG) {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] | GESTURE_WAKEUP_MODE_RESERVE8_FLAG;
        } else {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] & (~GESTURE_WAKEUP_MODE_RESERVE8_FLAG);
        }

        if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_RESERVE9_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE9_FLAG) {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] | GESTURE_WAKEUP_MODE_RESERVE9_FLAG;
        } else {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] & (~GESTURE_WAKEUP_MODE_RESERVE9_FLAG);
        }

        if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_RESERVE10_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE10_FLAG) {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] | GESTURE_WAKEUP_MODE_RESERVE10_FLAG;
        } else {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] &
                (~GESTURE_WAKEUP_MODE_RESERVE10_FLAG);
        }

        if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_RESERVE11_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE11_FLAG) {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] | GESTURE_WAKEUP_MODE_RESERVE11_FLAG;
        } else {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] &
                (~GESTURE_WAKEUP_MODE_RESERVE11_FLAG);
        }

        if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_RESERVE12_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE12_FLAG) {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] | GESTURE_WAKEUP_MODE_RESERVE12_FLAG;
        } else {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] &
                (~GESTURE_WAKEUP_MODE_RESERVE12_FLAG);
        }

        if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_RESERVE13_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE13_FLAG) {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] | GESTURE_WAKEUP_MODE_RESERVE13_FLAG;
        } else {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] &
                (~GESTURE_WAKEUP_MODE_RESERVE13_FLAG);
        }

        if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_RESERVE14_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE14_FLAG) {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] | GESTURE_WAKEUP_MODE_RESERVE14_FLAG;
        } else {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] &
                (~GESTURE_WAKEUP_MODE_RESERVE14_FLAG);
        }

        if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_RESERVE15_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE15_FLAG) {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] | GESTURE_WAKEUP_MODE_RESERVE15_FLAG;
        } else {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] &
                (~GESTURE_WAKEUP_MODE_RESERVE15_FLAG);
        }

        if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_RESERVE16_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE16_FLAG) {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] | GESTURE_WAKEUP_MODE_RESERVE16_FLAG;
        } else {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] &
                (~GESTURE_WAKEUP_MODE_RESERVE16_FLAG);
        }

        if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_RESERVE17_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE17_FLAG) {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] | GESTURE_WAKEUP_MODE_RESERVE17_FLAG;
        } else {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] &
                (~GESTURE_WAKEUP_MODE_RESERVE17_FLAG);
        }

        if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_RESERVE18_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE18_FLAG) {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] | GESTURE_WAKEUP_MODE_RESERVE18_FLAG;
        } else {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] &
                (~GESTURE_WAKEUP_MODE_RESERVE18_FLAG);
        }

        if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_RESERVE19_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE19_FLAG) {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] | GESTURE_WAKEUP_MODE_RESERVE19_FLAG;
        } else {
            g_gesture_wakeup_mode[0] =
                g_gesture_wakeup_mode[0] &
                (~GESTURE_WAKEUP_MODE_RESERVE19_FLAG);
        }

        if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE20_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE20_FLAG) {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] | GESTURE_WAKEUP_MODE_RESERVE20_FLAG;
        } else {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] &
                (~GESTURE_WAKEUP_MODE_RESERVE20_FLAG);
        }

        if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE21_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE21_FLAG) {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] | GESTURE_WAKEUP_MODE_RESERVE21_FLAG;
        } else {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] &
                (~GESTURE_WAKEUP_MODE_RESERVE21_FLAG);
        }

        if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE22_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE22_FLAG) {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] | GESTURE_WAKEUP_MODE_RESERVE22_FLAG;
        } else {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] &
                (~GESTURE_WAKEUP_MODE_RESERVE22_FLAG);
        }

        if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE23_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE23_FLAG) {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] | GESTURE_WAKEUP_MODE_RESERVE23_FLAG;
        } else {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] &
                (~GESTURE_WAKEUP_MODE_RESERVE23_FLAG);
        }

        if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE24_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE24_FLAG) {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] | GESTURE_WAKEUP_MODE_RESERVE24_FLAG;
        } else {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] &
                (~GESTURE_WAKEUP_MODE_RESERVE24_FLAG);
        }

        if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE25_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE25_FLAG) {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] | GESTURE_WAKEUP_MODE_RESERVE25_FLAG;
        } else {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] &
                (~GESTURE_WAKEUP_MODE_RESERVE25_FLAG);
        }

        if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE26_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE26_FLAG) {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] | GESTURE_WAKEUP_MODE_RESERVE26_FLAG;
        } else {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] &
                (~GESTURE_WAKEUP_MODE_RESERVE26_FLAG);
        }

        if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE27_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE27_FLAG) {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] | GESTURE_WAKEUP_MODE_RESERVE27_FLAG;
        } else {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] &
                (~GESTURE_WAKEUP_MODE_RESERVE27_FLAG);
        }

        if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE28_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE28_FLAG) {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] | GESTURE_WAKEUP_MODE_RESERVE28_FLAG;
        } else {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] &
                (~GESTURE_WAKEUP_MODE_RESERVE28_FLAG);
        }

        if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE29_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE29_FLAG) {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] | GESTURE_WAKEUP_MODE_RESERVE29_FLAG;
        } else {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] &
                (~GESTURE_WAKEUP_MODE_RESERVE29_FLAG);
        }

        if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE30_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE30_FLAG) {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] | GESTURE_WAKEUP_MODE_RESERVE30_FLAG;
        } else {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] &
                (~GESTURE_WAKEUP_MODE_RESERVE30_FLAG);
        }

        if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE31_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE31_FLAG) {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] | GESTURE_WAKEUP_MODE_RESERVE31_FLAG;
        } else {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] &
                (~GESTURE_WAKEUP_MODE_RESERVE31_FLAG);
        }

        if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE32_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE32_FLAG) {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] | GESTURE_WAKEUP_MODE_RESERVE32_FLAG;
        } else {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] &
                (~GESTURE_WAKEUP_MODE_RESERVE32_FLAG);
        }

        if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE33_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE33_FLAG) {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] | GESTURE_WAKEUP_MODE_RESERVE33_FLAG;
        } else {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] &
                (~GESTURE_WAKEUP_MODE_RESERVE33_FLAG);
        }

        if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE34_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE34_FLAG) {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] | GESTURE_WAKEUP_MODE_RESERVE34_FLAG;
        } else {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] &
                (~GESTURE_WAKEUP_MODE_RESERVE34_FLAG);
        }

        if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE35_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE35_FLAG) {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] | GESTURE_WAKEUP_MODE_RESERVE35_FLAG;
        } else {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] &
                (~GESTURE_WAKEUP_MODE_RESERVE35_FLAG);
        }

        if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE36_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE36_FLAG) {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] | GESTURE_WAKEUP_MODE_RESERVE36_FLAG;
        } else {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] &
                (~GESTURE_WAKEUP_MODE_RESERVE36_FLAG);
        }

        if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE37_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE37_FLAG) {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] | GESTURE_WAKEUP_MODE_RESERVE37_FLAG;
        } else {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] &
                (~GESTURE_WAKEUP_MODE_RESERVE37_FLAG);
        }

        if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE38_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE38_FLAG) {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] | GESTURE_WAKEUP_MODE_RESERVE38_FLAG;
        } else {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] &
                (~GESTURE_WAKEUP_MODE_RESERVE38_FLAG);
        }

        if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE39_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE39_FLAG) {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] | GESTURE_WAKEUP_MODE_RESERVE39_FLAG;
        } else {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] &
                (~GESTURE_WAKEUP_MODE_RESERVE39_FLAG);
        }

        if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE40_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE40_FLAG) {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] | GESTURE_WAKEUP_MODE_RESERVE40_FLAG;
        } else {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] &
                (~GESTURE_WAKEUP_MODE_RESERVE40_FLAG);
        }

        if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE41_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE41_FLAG) {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] | GESTURE_WAKEUP_MODE_RESERVE41_FLAG;
        } else {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] &
                (~GESTURE_WAKEUP_MODE_RESERVE41_FLAG);
        }

        if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE42_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE42_FLAG) {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] | GESTURE_WAKEUP_MODE_RESERVE42_FLAG;
        } else {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] &
                (~GESTURE_WAKEUP_MODE_RESERVE42_FLAG);
        }

        if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE43_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE43_FLAG) {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] | GESTURE_WAKEUP_MODE_RESERVE43_FLAG;
        } else {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] &
                (~GESTURE_WAKEUP_MODE_RESERVE43_FLAG);
        }

        if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE44_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE44_FLAG) {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] | GESTURE_WAKEUP_MODE_RESERVE44_FLAG;
        } else {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] &
                (~GESTURE_WAKEUP_MODE_RESERVE44_FLAG);
        }

        if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE45_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE45_FLAG) {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] | GESTURE_WAKEUP_MODE_RESERVE45_FLAG;
        } else {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] &
                (~GESTURE_WAKEUP_MODE_RESERVE45_FLAG);
        }

        if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE46_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE46_FLAG) {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] | GESTURE_WAKEUP_MODE_RESERVE46_FLAG;
        } else {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] &
                (~GESTURE_WAKEUP_MODE_RESERVE46_FLAG);
        }

        if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE47_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE47_FLAG) {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] | GESTURE_WAKEUP_MODE_RESERVE47_FLAG;
        } else {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] &
                (~GESTURE_WAKEUP_MODE_RESERVE47_FLAG);
        }

        if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE48_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE48_FLAG) {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] | GESTURE_WAKEUP_MODE_RESERVE48_FLAG;
        } else {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] &
                (~GESTURE_WAKEUP_MODE_RESERVE48_FLAG);
        }

        if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE49_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE49_FLAG) {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] | GESTURE_WAKEUP_MODE_RESERVE49_FLAG;
        } else {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] &
                (~GESTURE_WAKEUP_MODE_RESERVE49_FLAG);
        }

        if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE50_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE50_FLAG) {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] | GESTURE_WAKEUP_MODE_RESERVE50_FLAG;
        } else {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] &
                (~GESTURE_WAKEUP_MODE_RESERVE50_FLAG);
        }

        if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE51_FLAG) ==
            GESTURE_WAKEUP_MODE_RESERVE51_FLAG) {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] | GESTURE_WAKEUP_MODE_RESERVE51_FLAG;
        } else {
            g_gesture_wakeup_mode[1] =
                g_gesture_wakeup_mode[1] &
                (~GESTURE_WAKEUP_MODE_RESERVE51_FLAG);
        }
#endif /*CONFIG_SUPPORT_64_TYPES_GESTURE_WAKEUP_MODE */

        DBG(&g_i2c_client->dev, "g_gesture_wakeup_mode = 0x%x,  0x%x\n",
            g_gesture_wakeup_mode[0], g_gesture_wakeup_mode[1]);
    }

    return n_count;
}

#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
static ssize_t drv_procfs_gesture_debug_mode_read(struct file *pfile,
                                                  char __user *pbuffer,
                                                  size_t n_count, loff_t *ppos)
{
    u32 n_length = 0;
    u8 n_user_temp_buffer[16];

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    /*If file position is non-zero, then assume the string has been read and indicate there is no more data to be read. */
    if (*ppos != 0) {
        return 0;
    }

    DBG(&g_i2c_client->dev, "g_gesture_debug_mode = 0x%x\n", g_gesture_debug_mode); /*add for debug */

    n_length = snprintf(n_user_temp_buffer, sizeof(n_user_temp_buffer), "%d", g_gesture_debug_mode);
    if (copy_to_user(pbuffer, n_user_temp_buffer, n_length)) {
        DBG(&g_i2c_client->dev, "copy to user error\n");
        return -EFAULT;
    }

    *ppos += n_length;

    return n_length;
}

static ssize_t drv_procfs_gesture_debug_mode_write(struct file *pfile,
                                                   const char __user *pbuffer,
                                                   size_t n_count, loff_t *ppos)
{
    u8 ucGestureMode[2];
    u8 i;
    char *pCh;
    char *pStr = NULL;

    memset(g_debug_buf, 0, 1024);
    if (copy_from_user(g_debug_buf, pbuffer, n_count)) {
        DBG(&g_i2c_client->dev, "copy_from_user() failed\n");

        return -EFAULT;
    }
    pStr = g_debug_buf;
    if (pStr != NULL) {
        i = 0;
        while ((pCh = strsep((char **)&pStr, " ,")) && (i < 2)) {
            DBG(&g_i2c_client->dev, "pCh = %s\n", pCh);

            ucGestureMode[i] = drv_convert_char_to_hex_digit(pCh, strlen(pCh));

            DBG(&g_i2c_client->dev, "ucGestureMode[%d] = 0x%04X\n", i,
                ucGestureMode[i]);
            i++;
        }

        g_gesture_debug_mode = ucGestureMode[0];
        g_gesture_debug_flag = ucGestureMode[1];

        DBG(&g_i2c_client->dev, "Gesture flag = 0x%x\n", g_gesture_debug_flag);

        if (g_gesture_debug_mode == 0x01) { /*open gesture debug mode */
            drv_open_gesture_debug_mode(g_gesture_debug_flag);

/*input_report_key(g_input_device, RESERVER42, 1);*/
            input_report_key(g_input_device, KEY_POWER, 1);
            input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER42, 0);*/
            input_report_key(g_input_device, KEY_POWER, 0);
            input_sync(g_input_device);
        } else if (g_gesture_debug_mode == 0x00) {  /*close gesture debug mode */
            drv_close_gesture_debug_mode();
        } else {
            DBG(&g_i2c_client->dev, "*** Undefined Gesture Debug Mode ***\n");
        }
    }

    return n_count;
}

static ssize_t drv_kobject_gesture_debug_show(struct kobject *pkobj,
                                              struct kobj_attribute *pattr,
                                              char *p_buf)
{
    u32 i = 0;
    u32 n_length = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    if (strcmp(pattr->attr.name, "gesture_debug") == 0) {
        if (g_log_gesture_debug != NULL) {
            DBG(&g_i2c_client->dev,
                "g_log_gesture_debug[0]=%x, g_log_gesture_debug[1]=%x\n",
                g_log_gesture_debug[0], g_log_gesture_debug[1]);
            DBG(&g_i2c_client->dev,
                "g_log_gesture_debug[2]=%x, g_log_gesture_debug[3]=%x\n",
                g_log_gesture_debug[2], g_log_gesture_debug[3]);
            DBG(&g_i2c_client->dev,
                "g_log_gesture_debug[4]=%x, g_log_gesture_debug[5]=%x\n",
                g_log_gesture_debug[4], g_log_gesture_debug[5]);

            if (g_log_gesture_debug[0] == 0xA7 &&
                g_log_gesture_debug[3] == 0x51) {
                for (i = 0; i < 0x80; i++) {
                    p_buf[i] = g_log_gesture_debug[i];
                }

                n_length = 0x80;
                DBG(&g_i2c_client->dev, "n_length = %d\n", n_length);
            } else {
                DBG(&g_i2c_client->dev,
                    "CURRENT MODE IS NOT GESTURE DEBUG MODE/WRONG GESTURE DEBUG MODE HEADER\n");
            }
        } else {
            DBG(&g_i2c_client->dev, "g_log_gesture_debug is NULL\n");
        }
    } else {
        DBG(&g_i2c_client->dev, "pattr->attr.name = %s\n", pattr->attr.name);
    }

    return n_length;
}

static ssize_t drv_kobject_gesture_debug_store(struct kobject *pkobj,
                                               struct kobj_attribute *pattr,
                                               const char *p_buf, size_t n_count)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

/*
    if (strcmp(pattr->attr.name, "gesture_debug") == 0)
    {

    }
*/
    return n_count;
}

static struct kobj_attribute gesture_attr =
__ATTR(gesture_debug, 0664, drv_kobject_gesture_debug_show,
       drv_kobject_gesture_debug_store);

/* Create a group of attributes so that we can create and destroy them all at once. */
static struct attribute *gestureattrs[] = {
    &gesture_attr.attr,
    NULL,                       /* need to NULL terminate the list of attributes */
};

/*
 * An unnamed attribute group will put all of the attributes directly in
 * the kobject directory. If we specify a name, a subdirectory will be
 * created for the attributes with the directory being the name of the
 * attribute group.
 */
struct attribute_group gestureattr_group = {
    .attrs = gestureattrs,
};
#endif /*CONFIG_ENABLE_GESTURE_DEBUG_MODE */

#ifdef CONFIG_ENABLE_GESTURE_INFORMATION_MODE
static ssize_t drv_procfs_gesture_infor_mode_read(struct file *pfile,
                                                  char __user *pbuffer,
                                                  size_t n_count, loff_t *ppos)
{
    u8 szOut[GESTURE_WAKEUP_INFORMATION_PACKET_LENGTH * 5] = { 0 }, szValue[10] = { 0 };
    u32 szLogGestureInfo[GESTURE_WAKEUP_INFORMATION_PACKET_LENGTH] = { 0 };
    u32 i = 0;
    u32 n_length = 0;
    u8 n_user_temp_buffer[16];

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    /*If file position is non-zero, then assume the string has been read and indicate there is no more data to be read. */
    if (*ppos != 0) {
        return 0;
    }

    g_log_gesture_count = 0;
    if (g_log_gesture_infor_type == FIRMWARE_GESTURE_INFORMATION_MODE_A) {  /*FIRMWARE_GESTURE_INFORMATION_MODE_A */
        for (i = 0; i < 2; i++) {   /*0 EventFlag; 1 RecordNum */
            szLogGestureInfo[g_log_gesture_count] = g_log_gesture_infor[4 + i];
            g_log_gesture_count++;
        }

        for (i = 2; i < 8; i++) {   /*2~3 Xst Yst; 4~5 Xend Yend; 6~7 char_width char_height */
            szLogGestureInfo[g_log_gesture_count] = g_log_gesture_infor[4 + i];
            g_log_gesture_count++;
        }
    } else if (g_log_gesture_infor_type == FIRMWARE_GESTURE_INFORMATION_MODE_B) {   /*FIRMWARE_GESTURE_INFORMATION_MODE_B */
        for (i = 0; i < 2; i++) {   /*0 EventFlag; 1 RecordNum */
            szLogGestureInfo[g_log_gesture_count] = g_log_gesture_infor[4 + i];
            g_log_gesture_count++;
        }

        for (i = 0; i < g_log_gesture_infor[5] * 2; i++) {  /*(X and Y)*RecordNum */
            szLogGestureInfo[g_log_gesture_count] = g_log_gesture_infor[12 + i];
            g_log_gesture_count++;
        }
    } else if (g_log_gesture_infor_type == FIRMWARE_GESTURE_INFORMATION_MODE_C) {   /*FIRMWARE_GESTURE_INFORMATION_MODE_C */
        for (i = 0; i < 6; i++) {   /*header */
            szLogGestureInfo[g_log_gesture_count] = g_log_gesture_infor[i];
            g_log_gesture_count++;
        }

        for (i = 6; i < 86; i++) {
            szLogGestureInfo[g_log_gesture_count] = g_log_gesture_infor[i];
            g_log_gesture_count++;
        }

        szLogGestureInfo[g_log_gesture_count] = g_log_gesture_infor[86];    /*dummy */
        g_log_gesture_count++;
        szLogGestureInfo[g_log_gesture_count] = g_log_gesture_infor[87];    /*checksum */
        g_log_gesture_count++;
    } else {
        DBG(&g_i2c_client->dev, "*** Undefined GESTURE INFORMATION MODE ***\n");
    }

    for (i = 0; i < g_log_gesture_count; i++) {
		snprintf(szValue, sizeof(szValue), "%d", szLogGestureInfo[i]);
		strlcat(szOut, szValue, sizeof(szOut));
		strlcat(szOut, ",", sizeof(szOut));
    }

    n_length = strlen(szOut);

    if (copy_to_user(pbuffer, n_user_temp_buffer, n_length)) {
        DBG(&g_i2c_client->dev, "copy to user error\n");
        return -EFAULT;
    }

    *ppos += n_length;

    return n_length;
}

static ssize_t drv_procfs_gesture_infor_mode_write(struct file *pfile,
                                                   const char __user *pbuffer,
                                                   size_t n_count, loff_t *ppos)
{
    u32 nMode;
    int ret;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);
    memset(g_debug_buf, 0, 16);

    if (copy_from_user(g_debug_buf, pbuffer, n_count)) {
        DBG(&g_i2c_client->dev, "copy_from_user() failed\n");

        return -EFAULT;
    }
    if (g_debug_buf != NULL) {
        ret = sscanf(g_debug_buf, "%x", &nMode);
        if (ret < 0) {
            DBG(0, "error\n");
            return -EFAULT;
        }           
        g_log_gesture_infor_type = nMode;
    }

    DBG(&g_i2c_client->dev, "*** g_log_gesture_infor_type type = 0x%x ***\n",
        g_log_gesture_infor_type);

    return n_count;
}
#endif /*CONFIG_ENABLE_GESTURE_INFORMATION_MODE */
#endif /*CONFIG_ENABLE_GESTURE_WAKEUP */

/*--------------------------------------------------------------------------*/

#ifdef CONFIG_ENABLE_COUNT_REPORT_RATE
static ssize_t drv_procfs_report_rate_read(struct file *pfile,
                                        char __user *pbuffer, size_t n_count,
                                        loff_t *ppos)
{
    struct timeval tEndTime;
    suseconds_t nStartTime, nEndTime, nElapsedTime;
    u32 n_length = 0;
    u8 n_user_temp_buffer[16];

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    DBG(&g_i2c_client->dev, "g_interrupt_count = %d, g_valid_touch_count = %d\n",
        g_interrupt_count, g_valid_touch_count);

    /*If file position is non-zero, then assume the string has been read and indicate there is no more data to be read. */
    if (*ppos != 0) {
        return 0;
    }

    /*Get end time */
    do_gettimeofday(&tEndTime);

    nStartTime = g_start_time.tv_sec + g_start_time.tv_usec / 1000000;
    nEndTime = tEndTime.tv_sec + tEndTime.tv_usec / 1000000;

    nElapsedTime = nEndTime - nStartTime;

    DBG(&g_i2c_client->dev, "Start time : %lu sec, %lu msec\n",
        g_start_time.tv_sec, g_start_time.tv_usec);
    DBG(&g_i2c_client->dev, "End time : %lu sec, %lu msec\n", tEndTime.tv_sec,
        tEndTime.tv_usec);

    DBG(&g_i2c_client->dev, "Elapsed time : %lu sec\n", nElapsedTime);

    /*Calculate report rate */
    if (nElapsedTime != 0) {
        g_interrupt_report_rate = g_interrupt_count / nElapsedTime;
        g_valid_touch_report_rate = g_valid_touch_count / nElapsedTime;
    } else {
        g_interrupt_report_rate = 0;
        g_valid_touch_report_rate = 0;
    }

    DBG(&g_i2c_client->dev,
        "g_interrupt_report_rate = %d, g_valid_touch_report_rate = %d\n",
        g_interrupt_report_rate, g_valid_touch_report_rate);

    g_interrupt_count = 0;       /*Reset count */
    g_valid_touch_count = 0;

    n_length =
		snprintf(n_user_temp_buffer, sizeof(n_user_temp_buffer), "%d,%d", g_interrupt_report_rate,
                g_valid_touch_report_rate);
    if (copy_to_user(pbuffer, n_user_temp_buffer, n_length)) {
        DBG(&g_i2c_client->dev, "copy to user error\n");
        return -EFAULT;
    }

    *ppos += n_length;

    return n_length;
}

static ssize_t drv_procfs_report_rate_write(struct file *pfile,
                                         const char __user *pbuffer,
                                         size_t n_count, loff_t *ppos)
{
    int ret;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);
    memset(g_debug_buf, 0, 16);

    if (copy_from_user(g_debug_buf, pbuffer, n_count)) {
        DBG(&g_i2c_client->dev, "copy_from_user() failed\n");

        return -EFAULT;
    }
    if (g_debug_buf != NULL) {
        ret = sscanf(g_debug_buf, "%d", &g_is_enable_report_rate);
        if (ret < 0) {
            DBG(0, "error\n");
            return -EFAULT;
        }   
        DBG(&g_i2c_client->dev, "g_is_enable_report_rate = %d\n", g_is_enable_report_rate);   /*1 : enable report rate calculation, 0 : disable report rate calculation, 2 : reset count */

        g_interrupt_count = 0;   /*Reset count */
        g_valid_touch_count = 0;
    }

    return n_count;
}
#endif /*CONFIG_ENABLE_COUNT_REPORT_RATE */

/*--------------------------------------------------------------------------*/

static ssize_t drv_procfs_glove_mode_read(struct file *pfile,
                                       char __user *pbuffer, size_t n_count,
                                       loff_t *ppos)
{
    u32 n_length = 0;
    u8 nGloveMode = 0;
    u8 n_user_temp_buffer[16];

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    /*If file position is non-zero, then assume the string has been read and indicate there is no more data to be read. */
    if (*ppos != 0) {
        return 0;
    }

    if (g_chip_type == CHIP_TYPE_MSG28XX || g_chip_type == CHIP_TYPE_MSG58XXA
        || g_chip_type == CHIP_TYPE_ILI2118A ||
        g_chip_type == CHIP_TYPE_ILI2117A) {
        drv_disable_finger_touch_report();

        drv_get_glove_info(&nGloveMode);

        drv_enable_finger_touch_report();

        DBG(&g_i2c_client->dev, "Glove Mode = 0x%x\n", nGloveMode);

		n_length = snprintf(n_user_temp_buffer, sizeof(n_user_temp_buffer), "%x", nGloveMode);
        if (copy_to_user(pbuffer, n_user_temp_buffer, n_length)) {
            DBG(&g_i2c_client->dev, "copy to user error\n");
            return -EFAULT;
        }

    }

    *ppos += n_length;

    return n_length;
}

static ssize_t drv_procfs_glove_mode_write(struct file *pfile,
                                        const char __user *pbuffer,
                                        size_t n_count, loff_t *ppos)
{
    u32 nGloveMode = 0;
    u32 i = 0;
    char *pCh = NULL;
    char *pStr = NULL;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);
    memset(g_debug_buf, 0, 16);
    if (copy_from_user(g_debug_buf, pbuffer, n_count)) {
        DBG(&g_i2c_client->dev, "copy_from_user() failed\n");

        return -EFAULT;
    }
    pStr = g_debug_buf;
    if (g_debug_buf != NULL) {
        i = 0;
        while ((pCh = strsep((char **)&pStr, ",")) && (i < 1)) {
            DBG(&g_i2c_client->dev, "pCh = %s\n", pCh);

            nGloveMode = drv_convert_char_to_hex_digit(pCh, strlen(pCh));

            i++;
        }

        DBG(&g_i2c_client->dev, "Glove Mode = 0x%x\n", nGloveMode);

        drv_disable_finger_touch_report();

        if (nGloveMode == 0x01) {   /*open glove mode */
            drv_open_glove_mode();
        } else if (nGloveMode == 0x00) {    /*close glove mode */
            drv_close_glove_mode();
        } else {
            DBG(&g_i2c_client->dev, "*** Undefined Glove Mode ***\n");
        }
        DBG(&g_i2c_client->dev, "g_is_enable_glove_mode = 0x%x\n",
            g_is_enable_glove_mode);

        drv_enable_finger_touch_report();
    }

    return n_count;
}

static ssize_t drv_procfs_open_glove_mode_read(struct file *pfile,
                                           char __user *pbuffer, size_t n_count,
                                           loff_t *ppos)
{
    u32 n_length = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    /*If file position is non-zero, then assume the string has been read and indicate there is no more data to be read. */
    if (*ppos != 0) {
        return 0;
    }

    if (g_chip_type == CHIP_TYPE_MSG28XX || g_chip_type == CHIP_TYPE_MSG58XXA
        || g_chip_type == CHIP_TYPE_ILI2118A ||
        g_chip_type == CHIP_TYPE_ILI2117A) {
        drv_disable_finger_touch_report();

        drv_open_glove_mode();

        drv_enable_finger_touch_report();
    }
    DBG(&g_i2c_client->dev, "g_is_enable_glove_mode = 0x%x\n",
        g_is_enable_glove_mode);

    *ppos += n_length;

    return n_length;
}

static ssize_t drv_procfs_close_glove_mode_read(struct file *pfile,
                                            char __user *pbuffer,
                                            size_t n_count, loff_t *ppos)
{
    u32 n_length = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    /*If file position is non-zero, then assume the string has been read and indicate there is no more data to be read. */
    if (*ppos != 0) {
        return 0;
    }

    if (g_chip_type == CHIP_TYPE_MSG28XX || g_chip_type == CHIP_TYPE_MSG58XXA
        || g_chip_type == CHIP_TYPE_ILI2118A ||
        g_chip_type == CHIP_TYPE_ILI2117A) {
        drv_disable_finger_touch_report();

        drv_close_glove_mode();

        drv_enable_finger_touch_report();
    }
    DBG(&g_i2c_client->dev, "g_is_enable_glove_mode = 0x%x\n",
        g_is_enable_glove_mode);

    *ppos += n_length;

    return n_length;
}

/*--------------------------------------------------------------------------*/

static ssize_t drv_procfs_leather_sheath_mode_read(struct file *pfile,
                                               char __user *pbuffer,
                                               size_t n_count, loff_t *ppos)
{
    u32 n_length = 0;
    u8 nLeatherSheathMode = 0;
    u8 n_user_temp_buffer[16];

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    /*If file position is non-zero, then assume the string has been read and indicate there is no more data to be read. */
    if (*ppos != 0) {
        return 0;
    }

    if (g_chip_type == CHIP_TYPE_MSG28XX || g_chip_type == CHIP_TYPE_MSG58XXA
        || g_chip_type == CHIP_TYPE_ILI2118A ||
        g_chip_type == CHIP_TYPE_ILI2117A) {
        drv_disable_finger_touch_report();

        drv_get_leather_sheath_info(&nLeatherSheathMode);

        drv_enable_finger_touch_report();

        DBG(&g_i2c_client->dev, "Leather Sheath Mode = 0x%x\n",
            nLeatherSheathMode);

		n_length = snprintf(n_user_temp_buffer, sizeof(n_user_temp_buffer), "%x", nLeatherSheathMode);
        if (copy_to_user(pbuffer, n_user_temp_buffer, n_length)) {
            DBG(&g_i2c_client->dev, "copy to user error\n");
            return -EFAULT;
        }

    }

    *ppos += n_length;

    return n_length;
}

static ssize_t drv_procfs_leather_sheath_mode_write(struct file *pfile,
                                                const char __user *pbuffer,
                                                size_t n_count, loff_t *ppos)
{
    u32 nLeatherSheathMode = 0;
    u32 i = 0;
    char *pCh = NULL;
    char *pStr = NULL;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);
    memset(g_debug_buf, 0, 16);
    if (copy_from_user(g_debug_buf, pbuffer, n_count)) {
        DBG(&g_i2c_client->dev, "copy_from_user() failed\n");

        return -EFAULT;
    }
    pStr = g_debug_buf;
    if (pStr != NULL) {
        i = 0;
        while ((pCh = strsep((char **)&pStr, ",")) && (i < 1)) {
            DBG(&g_i2c_client->dev, "pCh = %s\n", pCh);

            nLeatherSheathMode = drv_convert_char_to_hex_digit(pCh, strlen(pCh));

            i++;
        }

        DBG(&g_i2c_client->dev, "Leather Sheath Mode = 0x%x\n",
            nLeatherSheathMode);

        drv_disable_finger_touch_report();

        if (nLeatherSheathMode == 0x01) {   /*open leather sheath mode */
            drv_open_leather_sheath_mode();
        } else if (nLeatherSheathMode == 0x00) {    /*close leather sheath mode */
            drv_close_leather_sheath_mode();
        } else {
            DBG(&g_i2c_client->dev, "*** Undefined Leather Sheath Mode ***\n");
        }
        DBG(&g_i2c_client->dev, "g_is_enable_leather_sheath_mode = 0x%x\n",
            g_is_enable_leather_sheath_mode);

        drv_enable_finger_touch_report();
    }

    return n_count;
}

static ssize_t drv_procfs_trim_code_write(struct file *pfile,
                                       const char __user *pbuffer,
                                       size_t n_count, loff_t *ppos)
{

    return n_count;
}

void write_trimcode(u8 *uArray_data, u32 n_addr)
{
    u8 *nBuf;

    nBuf = uArray_data;
    drv_disable_finger_touch_report();
    db_bus_enter_serial_debug_mode();
    db_bus_stop_mcu();
    db_bus_iic_use_bus();
    db_bus_iic_reshape();
    nBuf[0] = ((nBuf[0] & 0x01) << 7) + ((nBuf[0] & 0x7F) >> 1);
    /*DEBUG("*** set Write *** 0x%x 0x%x\n", nBuf[0],nBuf[1]); */
    drv_msg_28xx_set_protect_bit();

/*set write done*/

/*reg_set_l_byte_value 0x1606 0x01*/
    /*Set Password */
    reg_set_l_byte_value(0x1616, 0xAA);
    reg_set_l_byte_value(0x1617, 0x55);
    reg_set_l_byte_value(0x1618, 0xA5);
    reg_set_l_byte_value(0x1619, 0x5A);

    /*disable cpu read, initial read */
    reg_set_l_byte_value(0x1608, 0x20);
    reg_set_l_byte_value(0x1606, 0x20);

    /*set info block */
    reg_set_l_byte_value(0x1606, 0x40);
    reg_set_l_byte_value(0x1607, 0x00);

    /*set info double buffer */
    reg_set_l_byte_value(0x1610, 0x00);

    /*data align */
    reg_set_l_byte_value(0x1640, 0x01);

    /*set info block */
    reg_set_l_byte_value(0x1607, 0x08);
    /*set info double buffer */
    reg_set_l_byte_value(0x1604, 0x01);
    /*eflash mode trigger */
    reg_set_l_byte_value(0x1606, 0x41);
    /*set initial data */
    reg_set_l_byte_value(0x1602, 0xA5);
    reg_set_l_byte_value(0x1602, 0x5A);
    reg_set_l_byte_value(0x1602, nBuf[1]);
    reg_set_l_byte_value(0x1602, nBuf[0]);
    /*set initial address (for latch SA, CA) */
    reg_set_l_byte_value(0x1600, 0x00);
    reg_set_l_byte_value(0x1601, 0x00);

    /*set initial address (for latch PA) */
    reg_set_l_byte_value(0x1600, 0x00);
    reg_set_l_byte_value(0x1601, 0x00);
    /*set write done */
    reg_set_l_byte_value(0x1606, 0x84);
    db_bus_iic_not_use_bus();
    db_bus_not_stop_mcu();
    db_bus_exit_serial_debug_mode();
    mdelay(100);
    drv_enable_finger_touch_report();
    drv_touch_device_hw_reset();
    /*DisableBypassHotknot(); */
    kfree(nBuf);
}

static int read_trimcode(u16 n_addr, u16 n_length)
{
    u8 tx_data[4] = { 0 };
    u8 rx_data[20] = { 0 };
    u8 *p_buf;
    u8 result;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);
    p_buf = (u8 *) kmalloc(n_length, GFP_KERNEL);
    drv_disable_finger_touch_report();
    db_bus_enter_serial_debug_mode();
    db_bus_stop_mcu();
    db_bus_iic_use_bus();
    db_bus_iic_reshape();
    DBG(&g_i2c_client->dev, "*** set read ***0x%x 0x%x, 0x%x\n", n_addr,
        n_addr >> 8, n_addr & (0x00FF));
    tx_data[0] = 0x10;
    tx_data[1] = n_addr >> 8;
    tx_data[2] = (n_addr & (0x00FF)) * 2;
    result = iic_write_data(slave_i2c_id_db_bus, &tx_data[0], 3);
    mdelay(50);
    result = iic_read_data(slave_i2c_id_db_bus, &rx_data[0], 2);
    DBG(&g_i2c_client->dev,
        "0x%x, (rx_data[0]&0x3F) << 1 = 0x%x, (rx_data[0] >> 7 = 0x%x\n",
        rx_data[1], (rx_data[1] & 0x7F) << 1, (rx_data[1] >> 7));
    p_buf[0] = rx_data[1];
    p_buf[1] = ((rx_data[1] & 0x7F) << 1) + (rx_data[1] >> 7);
    p_buf[2] = rx_data[0];
    DBG(&g_i2c_client->dev, "0x%x,0x%x,0x%x\n", p_buf[0], p_buf[1], p_buf[2]);
    g_read_trim_data[0] = p_buf[0];
    g_read_trim_data[1] = p_buf[1];
    g_read_trim_data[2] = p_buf[2];
    db_bus_iic_not_use_bus();
    db_bus_not_stop_mcu();
    db_bus_exit_serial_debug_mode();
    mdelay(100);
    drv_enable_finger_touch_report();
    /*TouchDeviceResetHw(); */
    kfree(p_buf);
    return result;
}

static ssize_t drv_procfs_trim_code_read(struct file *pfile, char __user *pbuffer,
                                      size_t n_count, loff_t *ppos)
{
    struct file *p_file = NULL, *ffile = NULL;
    mm_segment_t fs;
    s32 i32Temp = 0;
    u8 nTrimCodeMin = 0, nTrimCodeMax = 0, nInitTrimCode = 0, nTempBuf[3] = { 0 }, nSet_data = 0, i = 0;
    int ret;

    fs = get_fs();
    set_fs(KERNEL_DS);
    pfile = filp_open(ILITEK_TRIMCODE_INITIAL_PATH_ON_SD_CARD, O_RDONLY, 0);
    read_trimcode(0x143D, 3);
    if (IS_ERR(p_file)) {

        DBG(&g_i2c_client->dev, "Error occurred while opening file %s.\n",
            ILITEK_TRIMCODE_INITIAL_PATH_ON_SD_CARD);
        mdelay(100);
        ffile =
            filp_open(ILITEK_TRIMCODE_INITIAL_PATH_ON_SD_CARD, O_CREAT | O_RDWR,
                      0);
		snprintf(g_debug_buf, sizeof(g_debug_buf), "trim code initial data:0x%x\n",
				g_read_trim_data[1]);
        nInitTrimCode = g_read_trim_data[1];
        ffile->f_op->write(ffile, g_debug_buf,
                           strlen(g_debug_buf) * sizeof(char), &ffile->f_pos);
        set_fs(fs);
        filp_close(ffile, NULL);
    } else {
        pfile->f_op->read(p_file, g_debug_buf, 1024, &p_file->f_pos);
        ret = sscanf(g_debug_buf, "trim code initial data:0x%x", &i32Temp);
        if (ret < 0) {
            DBG(0, "error\n");
            return -EFAULT;
        }           
        nInitTrimCode = (u8) i32Temp;
        DBG(0, "%s\n,nInitTrimCode=0x%x\n", g_debug_buf, nInitTrimCode);
    }
    nTrimCodeMax = nInitTrimCode + 2;
    if (nInitTrimCode - 2 < 0) {
        nTrimCodeMin = 0;
    } else {
        nTrimCodeMin = nInitTrimCode - 2;
    }
    DBG(&g_i2c_client->dev, "max:%d,min:%d,read trim:%d\n", nTrimCodeMax,
        nTrimCodeMin, g_read_trim_data[1]);
    {
        switch (g_read_trim_data[1]) {
        case 0:
        case 64:
        case 128:
        case 192:
            nTempBuf[0] = g_read_trim_data[1] + 1;
            nTempBuf[1] = g_read_trim_data[2];
            DBG(&g_i2c_client->dev, "Read trim code: %d, modify level: 1\n",
                g_read_trim_data[1]);
            if (nTempBuf[0] < nTrimCodeMin && nTempBuf[0] < nTrimCodeMax) {
                DBG(&g_i2c_client->dev, "modify value overflow\n");
				return -EFAULT;
            }
            break;
        case 255:
        case 63:
        case 127:
        case 191:
            nTempBuf[0] = g_read_trim_data[1] - 1;
            nTempBuf[1] = g_read_trim_data[2];
            DBG(&g_i2c_client->dev,
                "Read trim code: %d, modify level: -1, -2\n",
                g_read_trim_data[1]);
            if (nTempBuf[0] < nTrimCodeMin && nTempBuf[0] < nTrimCodeMax) {
                DBG(&g_i2c_client->dev, "modify value overflow\n");
				return -EFAULT;
            }
            break;
        default:
            nTempBuf[0] = g_read_trim_data[1] - 1;
            nTempBuf[1] = g_read_trim_data[2];
            DBG(&g_i2c_client->dev,
                "Read trim code: %d, modify level: 1, -1, -2\n",
                g_read_trim_data[1]);
            if (nTempBuf[0] < nTrimCodeMin && nTempBuf[0] < nTrimCodeMax) {
                DBG(&g_i2c_client->dev, "modify value overflow\n");
				return -EFAULT;
            }
        }
    }
    nSet_data = nTempBuf[0];
    for (i = 0; i < 6; i++) {
        write_trimcode(nTempBuf, 0);
        read_trimcode(0x143D, 3);
        if (g_read_trim_data[1] == nSet_data) {
            DBG(&g_i2c_client->dev, "Set Trim code: %d,status:Pass\n",
                g_read_trim_data[1]);
            return 0;
            break;
        } else {
            DBG(&g_i2c_client->dev,
                "Set Trim code error,Read Trim code: %d,retry count:%d\n",
                g_read_trim_data[1], i);
        }
    }
    DBG(&g_i2c_client->dev, "Read Trim code: %d,status:error\n",
        g_read_trim_data[1]);
    return -EFAULT;
}

static ssize_t drv_procfs_set_film_mode_write(struct file *pfile,
                                          const char __user *pbuffer,
                                          size_t n_count, loff_t *ppos)
{
    u8 nFilmType = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);
    memset(g_debug_buf, 0, 1024);

    if (copy_from_user(g_debug_buf, pbuffer, n_count)) {
        DBG(&g_i2c_client->dev, "copy_from_user() failed\n");

        return -EFAULT;
    }
    if (g_debug_buf != NULL) {
        DBG(&g_i2c_client->dev, "n_count = %d\n", (int)n_count);
        g_debug_buf[n_count] = '\0';
        nFilmType = drv_convert_char_to_hex_digit(g_debug_buf, strlen(g_debug_buf));
        DBG(&g_i2c_client->dev, "nFeature = 0x%02X\n", nFilmType);
        drv_set_film_mode(nFilmType);
    }

    return n_count;
}

static ssize_t drv_procfs_get_film_mode_read(struct file *pfile,
                                         char __user *pbuffer, size_t n_count,
                                         loff_t *ppos)
{
    u8 nFilmType = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);
    nFilmType = drv_get_film_mode();
    DBG(&g_i2c_client->dev, "*** %s() ***, nFilmType = %d\n", __func__,
        nFilmType);
    if (copy_to_user(pbuffer, &nFilmType, 1)) {
        return -EFAULT;
    }
    return 0;
}

/*---------------------------------------------------------------------------*/
s32 drv_touch_device_initialize(void)
{
    s32 n_ret_val = 0;
#ifdef CONFIG_ENABLE_PROXIMITY_DETECTION
#ifdef CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM
    int nErr;
    struct hwmsen_object tObjPs;
#endif /*CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM */
#endif /*CONFIG_ENABLE_PROXIMITY_DETECTION */

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    drv_create_procfs_dir_entry(); /*Create procfs directory entry */

    drv_create_system_dir_entry();

    /*Create ILITEK Node */

#ifdef CONFIG_ENABLE_JNI_INTERFACE
    drv_jni_create_msg_tool_mem();
#endif /*CONFIG_ENABLE_JNI_INTERFACE */

#ifdef CONFIG_ENABLE_ITO_MP_TEST
    drv_mp_test_create_mp_test_work_queue();
#endif /*CONFIG_ENABLE_ITO_MP_TEST */

    g_chip_type = drv_get_chip_type();  /*Try to get chip type by slave_i2c_id_db_bus(0x62) firstly. */

    if (g_chip_type == 0) {      /*If failed, try to get chip type by slave_i2c_id_db_bus(0x59) again. */
        slave_i2c_id_db_bus = (0xB2 >> 1);   /*0x59 */

        g_chip_type = drv_get_chip_type();
    }

    drv_touch_device_hw_reset();

    if (g_chip_type != 0) {      /*To make sure TP is attached on cell phone. */
        if (g_chip_type == CHIP_TYPE_MSG28XX || g_chip_type == CHIP_TYPE_MSG58XXA
            || g_chip_type == CHIP_TYPE_ILI2118A ||
            g_chip_type == CHIP_TYPE_ILI2117A) {
            memset(&g_mutual_firmware_info, 0x0, sizeof(struct mutual_firmware_info_t));
        } else if (g_chip_type == CHIP_TYPE_MSG22XX) {
            memset(&g_self_firmware_info, 0x0, sizeof(struct self_firmware_info_t));
            memset(&g_mutual_firmware_info, 0x0, sizeof(struct mutual_firmware_info_t));
        } else if (g_chip_type == CHIP_TYPE_ILI2120 ||
                   g_chip_type == CHIP_TYPE_ILI2121) {
            memset(&g_ilitek_tp_info, 0x0, sizeof(struct ilitek_tp_info_t));
        }

        drv_variable_initialize();

#ifdef CONFIG_ENABLE_CHARGER_DETECTION
        {
            u8 szChargerStatus[20] = { 0 };

            drv_read_file(POWER_SUPPLY_BATTERY_STATUS_PATCH, szChargerStatus, 20);

            DBG(&g_i2c_client->dev, "*** Battery Status : %s ***\n",
                szChargerStatus);

			if (strnstr(szChargerStatus, "Charging", 8) != NULL || strnstr(szChargerStatus, "Full", 4) != NULL
							|| strnstr(szChargerStatus, "Fully charged", 13) != NULL) { /*Charging */
                drv_charger_detection(1); /*charger plug-in */
            } else {            /*Not charging */

                drv_charger_detection(0); /*charger plug-out */
            }
        }
#endif /*CONFIG_ENABLE_CHARGER_DETECTION */

#ifdef CONFIG_ENABLE_PROXIMITY_DETECTION
#if defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM)

/*tsps_assist_register_callback("msg2xxx", &drv_tp_ps_enable, &drv_get_tp_ps_data);*/
#elif defined(CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM)
        tObjPs.polling = 0;     /*0 : interrupt mode, 1 : polling mode */
        tObjPs.sensor_operate = drv_tp_ps_operate;

		nErr = hwmsen_attach(ID_PROXIMITY, &tObjPs);
		if (nErr) {
            DBG(&g_i2c_client->dev, "call hwmsen_attach() failed = %d\n", nErr);
        }
#endif
#endif /*CONFIG_ENABLE_PROXIMITY_DETECTION */
    } else {
        n_ret_val = -ENODEV;
    }

    return n_ret_val;
}

static void drv_remove_procfs_dir_entry(void)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    if (g_proc_glove_mode_entry != NULL) {
        remove_proc_entry(PROC_NODE_GLOVE_MODE, g_proc_device_entry);
        g_proc_glove_mode_entry = NULL;
        DBG(&g_i2c_client->dev, "Remove procfs file node(%s) OK!\n",
            PROC_NODE_GLOVE_MODE);
    }

    if (g_proc_open_glove_mode_entry != NULL) {
        remove_proc_entry(PROC_NODE_OPEN_GLOVE_MODE, g_proc_device_entry);
        g_proc_open_glove_mode_entry = NULL;
        DBG(&g_i2c_client->dev, "Remove procfs file node(%s) OK!\n",
            PROC_NODE_OPEN_GLOVE_MODE);
    }

    if (g_proc_close_glove_mode_entry != NULL) {
        remove_proc_entry(PROC_NODE_CLOSE_GLOVE_MODE, g_proc_device_entry);
        g_proc_close_glove_mode_entry = NULL;
        DBG(&g_i2c_client->dev, "Remove procfs file node(%s) OK!\n",
            PROC_NODE_CLOSE_GLOVE_MODE);
    }

    if (g_proc_leather_sheath_mode_entry != NULL) {
        remove_proc_entry(PROC_NODE_LEATHER_SHEATH_MODE, g_proc_device_entry);
        g_proc_leather_sheath_mode_entry = NULL;
        DBG(&g_i2c_client->dev, "Remove procfs file node(%s) OK!\n",
            PROC_NODE_LEATHER_SHEATH_MODE);
    }
    if (g_proc_film_mode_entry != NULL) {
        remove_proc_entry(PROC_NODE_CONTROL_FILM_MODE, g_proc_device_entry);
        g_proc_film_mode_entry = NULL;
        DBG(&g_i2c_client->dev, "Remove procfs file node(%s) OK!\n",
           PROC_NODE_CONTROL_FILM_MODE);
    }
#ifdef CONFIG_ENABLE_JNI_INTERFACE
    if (g_proc_jni_method_entry != NULL) {
        remove_proc_entry(PROC_NODE_JNI_NODE, g_proc_device_entry);
        g_proc_jni_method_entry = NULL;
        DBG(&g_i2c_client->dev, "Remove procfs file node(%s) OK!\n",
            PROC_NODE_JNI_NODE);
    }
#endif /*CONFIG_ENABLE_JNI_INTERFACE */

#ifdef CONFIG_ENABLE_COUNT_REPORT_RATE
    if (g_proc_report_rate_entry != NULL) {
        remove_proc_entry(PROC_NODE_REPORT_RATE, g_proc_device_entry);
        g_proc_report_rate_entry = NULL;
        DBG(&g_i2c_client->dev, "Remove procfs file node(%s) OK!\n",
            PROC_NODE_REPORT_RATE);
    }
#endif /*CONFIG_ENABLE_COUNT_REPORT_RATE */

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
    if (g_proc_gesture_wakeup_mode_entry != NULL) {
        remove_proc_entry(PROC_NODE_GESTURE_WAKEUP_MODE, g_proc_device_entry);
        g_proc_gesture_wakeup_mode_entry = NULL;
        DBG(&g_i2c_client->dev, "Remove procfs file node(%s) OK!\n",
            PROC_NODE_GESTURE_WAKEUP_MODE);
    }
#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
    if (g_proc_gesture_debug_mode_entry != NULL) {
        remove_proc_entry(PROC_NODE_GESTURE_DEBUG_MODE, g_proc_device_entry);
        g_proc_gesture_debug_mode_entry = NULL;
        DBG(&g_i2c_client->dev, "Remove procfs file node(%s) OK!\n",
            PROC_NODE_GESTURE_DEBUG_MODE);
    }
#endif /*CONFIG_ENABLE_GESTURE_DEBUG_MODE */
#ifdef CONFIG_ENABLE_GESTURE_INFORMATION_MODE
    if (g_proc_gesture_infor_mode_entry != NULL) {
        remove_proc_entry(PROC_NODE_GESTURE_INFORMATION_MODE,
                          g_proc_device_entry);
        g_proc_gesture_infor_mode_entry = NULL;
        DBG(&g_i2c_client->dev, "Remove procfs file node(%s) OK!\n",
            PROC_NODE_GESTURE_INFORMATION_MODE);
    }
#endif /*CONFIG_ENABLE_GESTURE_INFORMATION_MODE */
#endif /*CONFIG_ENABLE_GESTURE_WAKEUP */

    if (g_proc_firmware_mode_entry != NULL) {
        remove_proc_entry(PROC_NODE_FIRMWARE_MODE, g_proc_device_entry);
        g_proc_firmware_mode_entry = NULL;
        DBG(&g_i2c_client->dev, "Remove procfs file node(%s) OK!\n",
            PROC_NODE_FIRMWARE_MODE);
    }

    if (g_proc_firmware_sensor_entry != NULL) {
        remove_proc_entry(PROC_NODE_FIRMWARE_SENSOR, g_proc_device_entry);
        g_proc_firmware_sensor_entry = NULL;
        DBG(&g_i2c_client->dev, "Remove procfs file node(%s) OK!\n",
            PROC_NODE_FIRMWARE_SENSOR);
    }

    if (g_proc_firmware_packet_header_entry != NULL) {
        remove_proc_entry(PROC_NODE_FIRMWARE_PACKET_HEADER,
                          g_proc_device_entry);
        g_proc_firmware_packet_header_entry = NULL;
        DBG(&g_i2c_client->dev, "Remove procfs file node(%s) OK!\n",
            PROC_NODE_FIRMWARE_PACKET_HEADER);
    }

    if (g_proc_query_feature_support_status_entry != NULL) {
        remove_proc_entry(PROC_NODE_QUERY_FEATURE_SUPPORT_STATUS,
                          g_proc_device_entry);
        g_proc_query_feature_support_status_entry = NULL;
        DBG(&g_i2c_client->dev, "Remove procfs file node(%s) OK!\n",
            PROC_NODE_QUERY_FEATURE_SUPPORT_STATUS);
    }

    if (g_proc_change_feature_support_status_entry != NULL) {
        remove_proc_entry(PROC_NODE_CHANGE_FEATURE_SUPPORT_STATUS,
                          g_proc_device_entry);
        g_proc_change_feature_support_status_entry = NULL;
        DBG(&g_i2c_client->dev, "Remove procfs file node(%s) OK!\n",
            PROC_NODE_CHANGE_FEATURE_SUPPORT_STATUS);
    }
#ifdef CONFIG_ENABLE_ITO_MP_TEST
    if (g_proc_mp_test_entry != NULL) {
        remove_proc_entry(PROC_NODE_MP_TEST, g_proc_device_entry);
        g_proc_mp_test_entry = NULL;
        DBG(&g_i2c_client->dev, "Remove procfs file node(%s) OK!\n",
            PROC_NODE_MP_TEST);
    }

    if (g_proc_mp_test_log_entry != NULL) {
        remove_proc_entry(PROC_NODE_MP_TEST_LOG, g_proc_device_entry);
        g_proc_mp_test_log_entry = NULL;
        DBG(&g_i2c_client->dev, "Remove procfs file node(%s) OK!\n",
            PROC_NODE_MP_TEST_LOG);
    }

    if (g_proc_mp_test_fail_channel_entry != NULL) {
        remove_proc_entry(PROC_NODE_MP_TEST_FAIL_CHANNEL, g_proc_device_entry);
        g_proc_mp_test_fail_channel_entry = NULL;
        DBG(&g_i2c_client->dev, "Remove procfs file node(%s) OK!\n",
            PROC_NODE_MP_TEST_FAIL_CHANNEL);
    }

    if (g_proc_mp_test_scope_entry != NULL) {
        remove_proc_entry(PROC_NODE_MP_TEST_SCOPE, g_proc_device_entry);
        g_proc_mp_test_scope_entry = NULL;
        DBG(&g_i2c_client->dev, "Remove procfs file node(%s) OK!\n",
            PROC_NODE_MP_TEST_SCOPE);
    }
#ifdef CONFIG_ENABLE_CHIP_TYPE_MSG28XX
    if (g_proc_mp_test_log_all_entry != NULL) {
        remove_proc_entry(PROC_NODE_MP_TEST_LOG_ALL, g_proc_device_entry);
        g_proc_mp_test_log_all_entry = NULL;
        DBG(&g_i2c_client->dev, "Remove procfs file node(%s) OK!\n",
            PROC_NODE_MP_TEST_LOG_ALL);
    }
#endif /*CONFIG_ENABLE_CHIP_TYPE_MSG28XX */
#endif /*CONFIG_ENABLE_ITO_MP_TEST */

    if (g_proc_firmware_set_dq_mem_value_entry != NULL) {
        remove_proc_entry(PROC_NODE_FIRMWARE_SET_DQMEM_VALUE,
                          g_proc_device_entry);
        g_proc_firmware_set_dq_mem_value_entry = NULL;
        DBG(&g_i2c_client->dev, "Remove procfs file node(%s) OK!\n",
            PROC_NODE_FIRMWARE_SET_DQMEM_VALUE);
    }

    if (g_proc_firmware_sm_bus_debug_entry != NULL) {
        remove_proc_entry(PROC_NODE_FIRMWARE_SMBUS_DEBUG, g_proc_device_entry);
        g_proc_firmware_sm_bus_debug_entry = NULL;
        DBG(&g_i2c_client->dev, "Remove procfs file node(%s) OK!\n",
            PROC_NODE_FIRMWARE_SMBUS_DEBUG);
    }

    if (g_proc_firmware_set_debug_value_entry != NULL) {
        remove_proc_entry(PROC_NODE_FIRMWARE_SET_DEBUG_VALUE,
                          g_proc_device_entry);
        g_proc_firmware_set_debug_value_entry = NULL;
        DBG(&g_i2c_client->dev, "Remove procfs file node(%s) OK!\n",
            PROC_NODE_FIRMWARE_SET_DEBUG_VALUE);
    }

    if (g_proc_firmware_debug_entry != NULL) {
        remove_proc_entry(PROC_NODE_FIRMWARE_DEBUG, g_proc_device_entry);
        g_proc_firmware_debug_entry = NULL;
        DBG(&g_i2c_client->dev, "Remove procfs file node(%s) OK!\n",
            PROC_NODE_FIRMWARE_DEBUG);
    }

    if (g_proc_sd_card_firmware_update_entry != NULL) {
        remove_proc_entry(PROC_NODE_SD_CARD_FIRMWARE_UPDATE,
                          g_proc_device_entry);
        g_proc_sd_card_firmware_update_entry = NULL;
        DBG(&g_i2c_client->dev, "Remove procfs file node(%s) OK!\n",
            PROC_NODE_SD_CARD_FIRMWARE_UPDATE);
    }

    if (g_proc_selinux_limit_firmware_update_entry != NULL) {
        remove_proc_entry(PROC_NODE_SELINUX_LIMIT_FIRMWARE_UPDATE,
                          g_proc_device_entry);
        g_proc_selinux_limit_firmware_update_entry = NULL;
        DBG(&g_i2c_client->dev, "Remove procfs file node(%s) OK!\n",
            PROC_NODE_SELINUX_LIMIT_FIRMWARE_UPDATE);
    }

    if (g_proc_force_firmware_update_entry != NULL) {
        remove_proc_entry(PROC_NODE_FORCE_FIRMWARE_UPDATE, g_proc_device_entry);
        g_proc_force_firmware_update_entry = NULL;
        DBG(&g_i2c_client->dev, "Remove procfs file node(%s) OK!\n",
            PROC_NODE_FORCE_FIRMWARE_UPDATE);
    }

    if (g_proc_device_driver_version_entry != NULL) {
        remove_proc_entry(PROC_NODE_DEVICE_DRIVER_VERSION, g_proc_device_entry);
        g_proc_device_driver_version_entry = NULL;
        DBG(&g_i2c_client->dev, "Remove procfs file node(%s) OK!\n",
            PROC_NODE_DEVICE_DRIVER_VERSION);
    }

    if (g_proc_platform_firmware_version_entry != NULL) {
        remove_proc_entry(PROC_NODE_PLATFORM_FIRMWARE_VERSION,
                          g_proc_device_entry);
        g_proc_platform_firmware_version_entry = NULL;
        DBG(&g_i2c_client->dev, "Remove procfs file node(%s) OK!\n",
            PROC_NODE_PLATFORM_FIRMWARE_VERSION);
    }

    if (g_proc_customer_firmware_version_entry != NULL) {
        remove_proc_entry(PROC_NODE_CUSTOMER_FIRMWARE_VERSION,
                          g_proc_device_entry);
        g_proc_customer_firmware_version_entry = NULL;
        DBG(&g_i2c_client->dev, "Remove procfs file node(%s) OK!\n",
            PROC_NODE_CUSTOMER_FIRMWARE_VERSION);
    }

    if (g_proc_apk_firmware_update_entry != NULL) {
        remove_proc_entry(PROC_NODE_FIRMWARE_UPDATE, g_proc_device_entry);
        g_proc_apk_firmware_update_entry = NULL;
        DBG(&g_i2c_client->dev, "Remove procfs file node(%s) OK!\n",
            PROC_NODE_FIRMWARE_UPDATE);
    }

    if (g_proc_firmware_data_entry != NULL) {
        remove_proc_entry(PROC_NODE_FIRMWARE_DATA, g_proc_device_entry);
        g_proc_firmware_data_entry = NULL;
        DBG(&g_i2c_client->dev, "Remove procfs file node(%s) OK!\n",
            PROC_NODE_FIRMWARE_DATA);
    }

    if (g_proc_chip_type_entry != NULL) {
        remove_proc_entry(PROC_NODE_CHIP_TYPE, g_proc_device_entry);
        g_proc_chip_type_entry = NULL;
        DBG(&g_i2c_client->dev, "Remove procfs file node(%s) OK!\n",
            PROC_NODE_CHIP_TYPE);
    }

    if (g_proc_trim_code_entry != NULL) {
        remove_proc_entry(PROC_NODE_TRIM_CODE, g_proc_device_entry);
        g_proc_trim_code_entry = NULL;
        DBG(&g_i2c_client->dev, "Remove procfs file node(%s) OK!\n",
            PROC_NODE_TRIM_CODE);
    }

    if (g_proc_device_entry != NULL) {
        remove_proc_entry(PROC_NODE_DEVICE,
                          g_proc_ms_touchscreen_msg20xx_entry);
        g_proc_device_entry = NULL;
        DBG(&g_i2c_client->dev, "Remove procfs file node(%s) OK!\n",
            PROC_NODE_DEVICE);
    }

    if (g_proc_ms_touchscreen_msg20xx_entry != NULL) {
        remove_proc_entry(PROC_NODE_MS_TOUCHSCREEN_MSG20XX, g_proc_class_entry);
        g_proc_ms_touchscreen_msg20xx_entry = NULL;
        DBG(&g_i2c_client->dev, "Remove procfs file node(%s) OK!\n",
            PROC_NODE_MS_TOUCHSCREEN_MSG20XX);
    }

    if (g_proc_class_entry != NULL) {
        remove_proc_entry(PROC_NODE_CLASS, NULL);
        g_proc_class_entry = NULL;
        DBG(&g_i2c_client->dev, "Remove procfs file node(%s) OK!\n",
            PROC_NODE_CLASS);
    }
}

/*=============================================================*/

/*LOCAL FUNCTION DEFINITION*/

/*=============================================================*/

#if defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM)

#ifdef CONFIG_TP_HAVE_KEY
#ifdef CONFIG_ENABLE_REPORT_KEY_WITH_COORDINATE
static ssize_t drv_virtual_keys_show(struct kobject *kobj,
                                   struct kobj_attribute *attr, char *buf)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    return snprintf(buf, 512, __stringify(EV_KEY) ":" __stringify(KEY_HOMEPAGE)
                   ":90:1330:100:100" ":" __stringify(EV_KEY) ":"
                   __stringify(KEY_MENU) ":270:1330:100:100" ":"
                   __stringify(EV_KEY) ":" __stringify(KEY_BACK)
                   ":450:1330:100:100" ":" __stringify(EV_KEY) ":"
                   __stringify(KEY_SEARCH) ":630:1330:100:100" "\n");
}

static struct kobj_attribute virtual_keys_attr = {
    .attr = {
             .name = "virtualkeys.msg2xxx_ts",
             .mode = S_IRUGO,
             },
    .show = &drv_virtual_keys_show,
};

static struct attribute *properties_attrs[] = {
    &virtual_keys_attr.attr,
    NULL
};

static struct attribute_group properties_attr_group = {
    .attrs = properties_attrs,
};

static void drv_virtual_keys_init(void)
{
    s32 n_ret_val = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    g_properties_kobj = kobject_create_and_add("board_properties", NULL);
    if (g_properties_kobj == NULL) {
        DBG(&g_i2c_client->dev,
            "*** Failed to kobject_create_and_add() for virtual keys *** n_ret_val=%d\n",
            n_ret_val);
        return;
    }

    n_ret_val = sysfs_create_group(g_properties_kobj, &properties_attr_group);
    if (n_ret_val < 0) {
        DBG(&g_i2c_client->dev,
            "*** Failed to sysfs_create_group() for virtual keys *** n_ret_val=%d\n",
            n_ret_val);

        kobject_put(g_properties_kobj);
        g_properties_kobj = NULL;
    }
}

static void drv_virtual_keys_uninit(void)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    if (g_properties_kobj) {
        kobject_put(g_properties_kobj);
        g_properties_kobj = NULL;
    }
}
#endif /*CONFIG_ENABLE_REPORT_KEY_WITH_COORDINATE */
#endif /*CONFIG_TP_HAVE_KEY */

#ifdef CONFIG_ENABLE_TOUCH_PIN_CONTROL
static s32 drv_touch_pin_ctrl_init(struct i2c_client *pClient)
{
    s32 n_ret_val = 0;
    u32 n_flag = 0;
    struct device_node *pDeviceNode = pClient->dev.of_node;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    g_gpio_rst = of_get_named_gpio_flags(pDeviceNode, "touch,reset-gpio", 0, &n_flag);   /*generally used for ILI21XX */

        ms_ts_msg_ic_gpio_rst = g_gpio_rst;

    if (g_gpio_rst < 0) {
        return g_gpio_rst;
    }

    g_gpio_irq = of_get_named_gpio_flags(pDeviceNode, "touch,irq-gpio", 0, &n_flag); /*generally used for ILI21XX */

        ms_ts_msg_ic_gpio_int = g_gpio_irq;

    DBG(&g_i2c_client->dev, "g_gpio_rst = %d, g_gpio_irq = %d\n", g_gpio_rst,
        g_gpio_irq);

    if (g_gpio_irq < 0) {
        return g_gpio_irq;
    }
    /* Get pinctrl if target uses pinctrl */
    g_ts_pin_ctrl = devm_pinctrl_get(&(pClient->dev));
    if (IS_ERR_OR_NULL(g_ts_pin_ctrl)) {
        n_ret_val = PTR_ERR(g_ts_pin_ctrl);
        DBG(&g_i2c_client->dev, "Target does not use pinctrl n_ret_val=%d\n",
            n_ret_val);
        goto ERROR_PINCTRL_GET;
    }

    g_pin_ctrl_state_active =
        pinctrl_lookup_state(g_ts_pin_ctrl, PINCTRL_STATE_ACTIVE);
    if (IS_ERR_OR_NULL(g_pin_ctrl_state_active)) {
        n_ret_val = PTR_ERR(g_pin_ctrl_state_active);
        DBG(&g_i2c_client->dev, "Can not lookup %s pinstate n_ret_val=%d\n",
            PINCTRL_STATE_ACTIVE, n_ret_val);
        goto ERROR_PINCTRL_LOOKUP;
    }

    g_pin_ctrl_state_suspend =
        pinctrl_lookup_state(g_ts_pin_ctrl, PINCTRL_STATE_SUSPEND);
    if (IS_ERR_OR_NULL(g_pin_ctrl_state_suspend)) {
        n_ret_val = PTR_ERR(g_pin_ctrl_state_suspend);
        DBG(&g_i2c_client->dev, "Can not lookup %s pinstate n_ret_val=%d\n",
            PINCTRL_STATE_SUSPEND, n_ret_val);
        goto ERROR_PINCTRL_LOOKUP;
    }

    g_pin_ctrl_state_release =
        pinctrl_lookup_state(g_ts_pin_ctrl, PINCTRL_STATE_RELEASE);
    if (IS_ERR_OR_NULL(g_pin_ctrl_state_release)) {
        n_ret_val = PTR_ERR(g_pin_ctrl_state_release);
        DBG(&g_i2c_client->dev, "Can not lookup %s pinstate n_ret_val=%d\n",
            PINCTRL_STATE_RELEASE, n_ret_val);
    }

    pinctrl_select_state(g_ts_pin_ctrl, g_pin_ctrl_state_active);

    return 0;

ERROR_PINCTRL_LOOKUP:
    devm_pinctrl_put(g_ts_pin_ctrl);
ERROR_PINCTRL_GET:
    g_ts_pin_ctrl = NULL;

    return n_ret_val;
}

static void drv_touch_pin_ctrl_uninit(void)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    if (g_ts_pin_ctrl) {
        devm_pinctrl_put(g_ts_pin_ctrl);
        g_ts_pin_ctrl = NULL;
    }
}
#endif /*CONFIG_ENABLE_TOUCH_PIN_CONTROL */

/* read data through I2C then report data to input sub-system when interrupt occurred */
static void drv_finger_touch_do_work(struct work_struct *p_work)
{
    unsigned long nIrqFlag;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    drv_handle_finger_touch();

    DBG(&g_i2c_client->dev, "*** %s() g_interrupt_flag = %d ***\n", __func__, g_interrupt_flag);    /*add for debug */

    spin_lock_irqsave(&g_irq_lock, nIrqFlag);

    if (g_interrupt_flag == 0
#ifdef CONFIG_ENABLE_ITO_MP_TEST
        && g_is_in_mp_test == 0
#endif /*CONFIG_ENABLE_ITO_MP_TEST */
        ) {
        enable_irq(girq);

        g_interrupt_flag = 1;
    }

    spin_unlock_irqrestore(&g_irq_lock, nIrqFlag);
}

/* The interrupt service routine will be triggered when interrupt occurred */
static irqreturn_t drv_finger_touch_interrupt_handler(s32 nIrq, void *pDeviceId)
{
    unsigned long nIrqFlag;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    DBG(&g_i2c_client->dev, "*** %s() g_interrupt_flag = %d ***\n", __func__,
        g_interrupt_flag);

    spin_lock_irqsave(&g_irq_lock, nIrqFlag);

    if (g_interrupt_flag == 1
#ifdef CONFIG_ENABLE_ITO_MP_TEST
        && g_is_in_mp_test == 0
#endif /*CONFIG_ENABLE_ITO_MP_TEST */
        ) {
        disable_irq_nosync(girq);

        g_interrupt_flag = 0;

        schedule_work(&gfinger_touch_work);
    }

    spin_unlock_irqrestore(&g_irq_lock, nIrqFlag);

    return IRQ_HANDLED;
}
#elif defined(CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM)

#ifdef CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD

/*static irqreturn_t drv_finger_touch_interrupt_handler(s32 nIrq, struct irq_desc *desc)*/
static irqreturn_t drv_finger_touch_interrupt_handler(s32 nIrq, void *pDeviceId)
#else
static void drv_finger_touch_interrupt_handler(void)
#endif                          /*CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD */
{
    unsigned long nIrqFlag;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    DBG(&g_i2c_client->dev, "*** %s() g_interrupt_flag = %d ***\n", __func__,
        g_interrupt_flag);

    spin_lock_irqsave(&g_irq_lock, nIrqFlag);

#ifdef CONFIG_USE_IRQ_INTERRUPT_FOR_MTK_PLATFORM

    if (g_interrupt_flag == 1
#ifdef CONFIG_ENABLE_ITO_MP_TEST
        && g_is_in_mp_test == 0
#endif /*CONFIG_ENABLE_ITO_MP_TEST */
        ) {
#ifdef CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD
        disable_irq_nosync(girq);
#else
        mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
#endif /*CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD */

        g_interrupt_flag = 0;

        schedule_work(&gfinger_touch_work);
    }
#else

    if (g_interrupt_flag == 1
#ifdef CONFIG_ENABLE_ITO_MP_TEST
        && g_is_in_mp_test == 0
#endif /*CONFIG_ENABLE_ITO_MP_TEST */
        ) {
#ifdef CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD
        disable_irq_nosync(girq);
#else
        mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);
#endif /*CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD */

        g_interrupt_flag = 0;

        g_tpd_flag = 1;
        wake_up_interruptible(&g_waiter);
    }
#endif /*CONFIG_USE_IRQ_INTERRUPT_FOR_MTK_PLATFORM */

    spin_unlock_irqrestore(&g_irq_lock, nIrqFlag);

#ifdef CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD
    return IRQ_HANDLED;
#endif /*CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD */
}

#ifdef CONFIG_USE_IRQ_INTERRUPT_FOR_MTK_PLATFORM

/* read data through I2C then report data to input sub-system when interrupt occurred */
static void drv_finger_touch_do_work(struct work_struct *p_work)
{
    unsigned long nIrqFlag;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    drv_handle_finger_touch();

    DBG(&g_i2c_client->dev, "*** %s() g_interrupt_flag = %d ***\n", __func__, g_interrupt_flag);    /*add for debug */

    spin_lock_irqsave(&g_irq_lock, nIrqFlag);

    if (g_interrupt_flag == 0
#ifdef CONFIG_ENABLE_ITO_MP_TEST
        && g_is_in_mp_test == 0
#endif /*CONFIG_ENABLE_ITO_MP_TEST */
        ) {
#ifdef CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD
        enable_irq(girq);
#else
        mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
#endif /*CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD */

        g_interrupt_flag = 1;
    }

    spin_unlock_irqrestore(&g_irq_lock, nIrqFlag);
}

#else

static int drv_finger_touch_handler(void *pUnUsed)
{
    unsigned long nIrqFlag;
    struct sched_param param = {.sched_priority = RTPM_PRIO_TPD };

    sched_setscheduler(current, SCHED_RR, &param);

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    do {
        set_current_state(TASK_INTERRUPTIBLE);
        wait_event_interruptible(g_waiter, g_tpd_flag != 0);
        g_tpd_flag = 0;

        set_current_state(TASK_RUNNING);

#ifdef CONFIG_ENABLE_ITO_MP_TEST
        if (g_is_in_mp_test == 0) {
#endif /*CONFIG_ENABLE_ITO_MP_TEST */
            drv_handle_finger_touch();
#ifdef CONFIG_ENABLE_ITO_MP_TEST
        }
#endif /*CONFIG_ENABLE_ITO_MP_TEST */

        DBG(&g_i2c_client->dev, "*** %s() g_interrupt_flag = %d ***\n",
            __func__, g_interrupt_flag);

        spin_lock_irqsave(&g_irq_lock, nIrqFlag);

        if (g_interrupt_flag == 0
#ifdef CONFIG_ENABLE_ITO_MP_TEST
            && g_is_in_mp_test == 0
#endif /*CONFIG_ENABLE_ITO_MP_TEST */
            ) {
#ifdef CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD
            enable_irq(girq);
#else
            mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
#endif /*CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD */

            g_interrupt_flag = 1;
        }

        spin_unlock_irqrestore(&g_irq_lock, nIrqFlag);

    } while (!kthread_should_stop());

    return 0;
}
#endif /*CONFIG_USE_IRQ_INTERRUPT_FOR_MTK_PLATFORM */
#endif

void drv_mutex_variable_initialize(void)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    mutex_init(&g_mutex);
    mutex_init(&g_mutex_protect);
    spin_lock_init(&g_irq_lock);
}

s32 drv_input_device_initialize(struct i2c_client *pClient)
{
    s32 n_ret_val = 0;
    u32 i;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

#if defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM)
    /* allocate an input device */
    g_input_device = input_allocate_device();
    if (g_input_device == NULL) {
        DBG(&g_i2c_client->dev,
            "*** Failed to allocate touch input device ***\n");
        return -ENOMEM;
    }

    g_input_device->name = pClient->name;
    g_input_device->phys = "I2C";
    g_input_device->dev.parent = &pClient->dev;
    g_input_device->id.bustype = BUS_I2C;

    /* set the supported event type for input device */
    set_bit(EV_ABS, g_input_device->evbit);
    set_bit(EV_SYN, g_input_device->evbit);
    set_bit(EV_KEY, g_input_device->evbit);
    set_bit(BTN_TOUCH, g_input_device->keybit);
    set_bit(INPUT_PROP_DIRECT, g_input_device->propbit);

#ifdef CONFIG_TP_HAVE_KEY
    /*Method 1. */
    {
        for (i = 0; i < MAX_KEY_NUM; i++) {
            input_set_capability(g_input_device, EV_KEY, g_tp_virtual_key[i]);
        }
    }

#ifdef CONFIG_ENABLE_REPORT_KEY_WITH_COORDINATE
    drv_virtual_keys_init();      /*Initialize virtual keys for specific SPRC/QCOM platform. */
#endif /*CONFIG_ENABLE_REPORT_KEY_WITH_COORDINATE */
#endif /*CONFIG_TP_HAVE_KEY */


       if (g_chip_type == CHIP_TYPE_ILI2120 || g_chip_type == CHIP_TYPE_ILI2121) {
        n_ret_val = drv_ilitek_read_touch_panelInfo(&g_ilitek_tp_info);

        for (i = 0; i < g_ilitek_tp_info.n_key_count; i++) {
            if (g_ilitek_tp_info.t_key_info[i].n_id <= 0) {
                continue;
            }

            set_bit(g_ilitek_tp_info.t_key_info[i].n_id & KEY_MAX,
                    g_input_device->keybit);
        }
    }

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
    input_set_capability(g_input_device, EV_KEY, KEY_POWER);
    input_set_capability(g_input_device, EV_KEY, KEY_UP);
    input_set_capability(g_input_device, EV_KEY, KEY_DOWN);
    input_set_capability(g_input_device, EV_KEY, KEY_LEFT);
    input_set_capability(g_input_device, EV_KEY, KEY_RIGHT);
    input_set_capability(g_input_device, EV_KEY, KEY_W);
    input_set_capability(g_input_device, EV_KEY, KEY_Z);
    input_set_capability(g_input_device, EV_KEY, KEY_V);
    input_set_capability(g_input_device, EV_KEY, KEY_O);
    input_set_capability(g_input_device, EV_KEY, KEY_M);
    input_set_capability(g_input_device, EV_KEY, KEY_C);
    input_set_capability(g_input_device, EV_KEY, KEY_E);
    input_set_capability(g_input_device, EV_KEY, KEY_S);
#endif /*CONFIG_ENABLE_GESTURE_WAKEUP */

    if (g_chip_type == CHIP_TYPE_MSG28XX || g_chip_type == CHIP_TYPE_MSG58XXA
        || g_chip_type == CHIP_TYPE_ILI2118A || g_chip_type == CHIP_TYPE_ILI2117A
        || g_chip_type == CHIP_TYPE_ILI2120 || g_chip_type == CHIP_TYPE_ILI2121) {
        input_set_abs_params(g_input_device, ABS_MT_TRACKING_ID, 0, (MUTUAL_MAX_TOUCH_NUM - 1), 0, 0);   /*ABS_MT_TRACKING_ID is used for MSG28xx/MSG58xxA/ILI21xx only */
    }
#ifndef CONFIG_ENABLE_TYPE_B_PROTOCOL
    input_set_abs_params(g_input_device, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
    input_set_abs_params(g_input_device, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);
#endif /*CONFIG_ENABLE_TYPE_B_PROTOCOL */
    input_set_abs_params(g_input_device, ABS_MT_POSITION_X, TOUCH_SCREEN_X_MIN,
                         TOUCH_SCREEN_X_MAX, 0, 0);
    input_set_abs_params(g_input_device, ABS_MT_POSITION_Y, TOUCH_SCREEN_Y_MIN,
                         TOUCH_SCREEN_Y_MAX, 0, 0);

#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
    if (g_chip_type == CHIP_TYPE_MSG22XX) {
        set_bit(BTN_TOOL_FINGER, g_input_device->keybit);
        input_mt_init_slots(g_input_device, SELF_MAX_TOUCH_NUM, 0);  /*for MSG22xx */
    } else if (g_chip_type == CHIP_TYPE_MSG28XX
               || g_chip_type == CHIP_TYPE_MSG58XXA
               || g_chip_type == CHIP_TYPE_ILI2118A
               || g_chip_type == CHIP_TYPE_ILI2117A
               || g_chip_type == CHIP_TYPE_ILI2120 ||
               g_chip_type == CHIP_TYPE_ILI2121) {
        set_bit(BTN_TOOL_FINGER, g_input_device->keybit);
        input_mt_init_slots(g_input_device, MUTUAL_MAX_TOUCH_NUM, 0);    /*for MSG28xx/MSG58xxA/ILI21xx */
    }
#endif /*CONFIG_ENABLE_TYPE_B_PROTOCOL */

    /* register the input device to input sub-system */
    n_ret_val = input_register_device(g_input_device);
    if (n_ret_val < 0) {
        DBG(&g_i2c_client->dev,
            "*** Unable to register touch input device *** n_ret_val=%d\n",
            n_ret_val);
        return n_ret_val;
    }
#ifdef CONFIG_ENABLE_PROXIMITY_DETECTION
    n_ret_val = drv_proximity_input_deviceInit(pClient);
#endif /*CONFIG_ENABLE_PROXIMITY_DETECTION */

#elif defined(CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM)
    g_input_device = tpd->dev;

#ifdef CONFIG_ENABLE_DMA_IIC
        dma_alloc();             /*dma_alloc() shall be called after g_input_device is available. */
#endif /*CONFIG_ENABLE_DMA_IIC */

#ifdef CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD
    if (tpd_dts_data.use_tpd_button) {
        for (i = 0; i < tpd_dts_data.tpd_key_num; i++) {
            input_set_capability(g_input_device, EV_KEY,
                                 tpd_dts_data.tpd_key_local[i]);
        }
    }
#else
#ifdef CONFIG_TP_HAVE_KEY
    {
        for (i = 0; i < MAX_KEY_NUM; i++) {
            input_set_capability(g_input_device, EV_KEY, g_tp_virtual_key[i]);
        }
    }
#endif /*CONFIG_TP_HAVE_KEY */
#endif /*CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD */

/*#ifdef VIRTUAL_KEY_PAD*/
    if (g_chip_type == CHIP_TYPE_ILI2120 || g_chip_type == CHIP_TYPE_ILI2121) {
        n_ret_val = drv_ilitek_read_touch_panelInfo(&g_ilitek_tp_info);

        for (i = 0; i < g_ilitek_tp_info.n_key_count; i++) {
            if (g_ilitek_tp_info.t_key_info[i].n_id <= 0) {
                continue;
            }

            set_bit(g_ilitek_tp_info.t_key_info[i].n_id & KEY_MAX,
                    g_input_device->keybit);
        }
    }

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
    input_set_capability(g_input_device, EV_KEY, KEY_POWER);
    input_set_capability(g_input_device, EV_KEY, KEY_UP);
    input_set_capability(g_input_device, EV_KEY, KEY_DOWN);
    input_set_capability(g_input_device, EV_KEY, KEY_LEFT);
    input_set_capability(g_input_device, EV_KEY, KEY_RIGHT);
    input_set_capability(g_input_device, EV_KEY, KEY_W);
    input_set_capability(g_input_device, EV_KEY, KEY_Z);
    input_set_capability(g_input_device, EV_KEY, KEY_V);
    input_set_capability(g_input_device, EV_KEY, KEY_O);
    input_set_capability(g_input_device, EV_KEY, KEY_M);
    input_set_capability(g_input_device, EV_KEY, KEY_C);
    input_set_capability(g_input_device, EV_KEY, KEY_E);
    input_set_capability(g_input_device, EV_KEY, KEY_S);
#endif /*CONFIG_ENABLE_GESTURE_WAKEUP */

    if (g_chip_type == CHIP_TYPE_MSG28XX || g_chip_type == CHIP_TYPE_MSG58XXA
        || g_chip_type == CHIP_TYPE_ILI2118A || g_chip_type == CHIP_TYPE_ILI2117A
        || g_chip_type == CHIP_TYPE_ILI2120 || g_chip_type == CHIP_TYPE_ILI2121) {
        input_set_abs_params(g_input_device, ABS_MT_TRACKING_ID, 0, (MUTUAL_MAX_TOUCH_NUM - 1), 0, 0);   /*ABS_MT_TRACKING_ID is used for MSG28xx/MSG58xxA/ILI21xx only */
    }

#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
        if (g_chip_type == CHIP_TYPE_MSG22XX) {
        set_bit(BTN_TOOL_FINGER, g_input_device->keybit);
        input_mt_init_slots(g_input_device, SELF_MAX_TOUCH_NUM, 0);  /*for MSG22xx */
    } else if (g_chip_type == CHIP_TYPE_MSG28XX
               || g_chip_type == CHIP_TYPE_MSG58XXA
               || g_chip_type == CHIP_TYPE_ILI2118A
               || g_chip_type == CHIP_TYPE_ILI2117A
               || g_chip_type == CHIP_TYPE_ILI2120 ||
               g_chip_type == CHIP_TYPE_ILI2121) {
        set_bit(BTN_TOOL_FINGER, g_input_device->keybit);
        input_mt_init_slots(g_input_device, MUTUAL_MAX_TOUCH_NUM, 0);    /*for MSG28xx/MSG58xxA/ILI21xx */
    }
#endif /*CONFIG_ENABLE_TYPE_B_PROTOCOL */

#endif

    return n_ret_val;
}

s32 drv_touch_device_requestGPIO(struct i2c_client *pClient)
{
    s32 n_ret_val = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

#if defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM)
#ifdef ALLWINNER_PLATFORM

    n_ret_val = gpio_request(CTP_IRQ_NUMBER, "C_TP_INT");

    if (n_ret_val < 0) {
        DBG_INFO(&g_i2c_client->dev, "*** Failed to request GPIO , error %d ***\n",
            n_ret_val);
    }

    gpio_direction_input(CTP_IRQ_NUMBER);

#else
#ifdef CONFIG_ENABLE_TOUCH_PIN_CONTROL
    drv_touch_pin_ctrl_init(pClient);
#endif /*CONFIG_ENABLE_TOUCH_PIN_CONTROL */

    n_ret_val = gpio_request(ms_ts_msg_ic_gpio_rst, "C_TP_RST");
    if (n_ret_val < 0) {
        DBG_INFO(&g_i2c_client->dev, "*** Failed to request GPIO %d, error %d ***\n",
            ms_ts_msg_ic_gpio_rst, n_ret_val);
		goto exit;
    }

    n_ret_val = gpio_request(ms_ts_msg_ic_gpio_int, "C_TP_INT");
    if (n_ret_val < 0) {
        DBG_INFO(&g_i2c_client->dev, "*** Failed to request GPIO %d, error %d ***\n",
            ms_ts_msg_ic_gpio_int, n_ret_val);
		goto free_gpio_rst;
    }

    gpio_direction_input(ms_ts_msg_ic_gpio_int);
#endif /*ALLWINNER_PLATFORM */
#endif /*CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM || CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM */

free_gpio_rst:
    gpio_free(ms_ts_msg_ic_gpio_rst);
exit:
    return n_ret_val;
}

s32 drv_touch_device_register_finger_touch_interrupt_handler(void)
{
    s32 n_ret_val = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

#if defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM)
    /* initialize the finger touch work queue */
    INIT_WORK(&gfinger_touch_work, drv_finger_touch_do_work);
#ifdef ALLWINNER_PLATFORM
    int ret = 0;

    girq = gpio_to_irq(CTP_IRQ_NUMBER);

    config_info.dev = &(g_input_device->dev);

    ret =
        input_request_int(&(config_info.input_type),
                          drv_finger_touch_interrupt_handler, CTP_IRQ_MODE, NULL);

    g_interrupt_flag = 1;

    if (ret) {
        DBG_INFO(0, "Ilitek: request irq failed\n");
    }
#else
    girq = gpio_to_irq(ms_ts_msg_ic_gpio_int);

    /* request an irq and register the isr */
    n_ret_val = request_threaded_irq(girq /*ms_ts_msg_ic_gpio_int */ , NULL,
                                   drv_finger_touch_interrupt_handler,
                                   /*IRQF_TRIGGER_RISING */
                                   IRQF_TRIGGER_FALLING | IRQF_ONESHOT
                                   /* | IRQF_NO_SUSPEND */ ,
                                   "msg2xxx", NULL);

        g_interrupt_flag = 1;

    if (n_ret_val != 0) {
        DBG_INFO(&g_i2c_client->dev, "*** Unable to claim irq %d; error %d ***\n",
            girq, n_ret_val);
    }
#endif /*ALLWINNER_PLATFORM */
#elif defined(CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM)

#ifdef CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD
    {
        struct device_node *pDeviceNode = NULL;
        u32 ints[2] = { 0, 0 };

        tpd_gpio_as_int(ms_ts_msg_ic_gpio_int);

/*pDeviceNode = of_find_compatible_node(NULL, NULL, "mediatek, TOUCH_PANEL-eint");*/
        pDeviceNode = of_find_matching_node(pDeviceNode, touch_of_match);

        if (pDeviceNode) {
            of_property_read_u32_array(pDeviceNode, "debounce", ints,
                                       ARRAY_SIZE(ints));
            gpio_set_debounce(ints[0], ints[1]);

            girq = irq_of_parse_and_map(pDeviceNode, 0);
            if (girq == 0) {
                DBG_INFO(&g_i2c_client->dev,
                    "*** Unable to irq_of_parse_and_map() ***\n");
            }

            /* request an irq and register the isr */
            n_ret_val =
                request_threaded_irq(girq /*ms_ts_msg_ic_gpio_int */ , NULL,
                                     drv_finger_touch_interrupt_handler,
                                     /*IRQF_TRIGGER_RISING */
                                     IRQF_TRIGGER_FALLING   /*IRQF_TRIGGER_NONE */
                                     | IRQF_ONESHOT /* | IRQF_NO_SUSPEND */ ,
                                     "touch_panel-eint", NULL);
                if (n_ret_val != 0) {
                DBG_INFO(&g_i2c_client->dev,
                    "*** Unable to claim irq %d; error %d ***\n", girq,
                    n_ret_val);
                DBG_INFO(&g_i2c_client->dev, "*** gpio_pin=%d, debounce=%d ***\n",
                    ints[0], ints[1]);
            }
        } else {
            DBG_INFO(&g_i2c_client->dev,
                "*** request_irq() can not find touch eint device node! ***\n");
        }

/*enable_irq(girq);*/
    }
#else
    mt_set_gpio_mode(ms_ts_msg_ic_gpio_int, GPIO_CTP_EINT_PIN_M_EINT);
    mt_set_gpio_dir(ms_ts_msg_ic_gpio_int, GPIO_DIR_IN);
    mt_set_gpio_pull_enable(ms_ts_msg_ic_gpio_int, GPIO_PULL_ENABLE);
    mt_set_gpio_pull_select(ms_ts_msg_ic_gpio_int, GPIO_PULL_UP);

    mt_eint_set_hw_debounce(CUST_EINT_TOUCH_PANEL_NUM,
                            CUST_EINT_TOUCH_PANEL_DEBOUNCE_CN);
    mt_eint_registration(CUST_EINT_TOUCH_PANEL_NUM,
                         CUST_EINT_TOUCH_PANEL_TYPE /* EINTF_TRIGGER_RISING */ ,
                         drv_finger_touch_interrupt_handler, 1);

    mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);
#endif /*CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD */

    g_interrupt_flag = 1;

#ifdef CONFIG_USE_IRQ_INTERRUPT_FOR_MTK_PLATFORM
    /* initialize the finger touch work queue */
    INIT_WORK(&gfinger_touch_work, drv_finger_touch_do_work);
#else
    g_thread = kthread_run(drv_finger_touch_handler, 0, TPD_DEVICE);
    if (IS_ERR(g_thread)) {
        n_ret_val = PTR_ERR(g_thread);
        DBG(&g_i2c_client->dev, "Failed to create kernel thread: %d\n",
            n_ret_val);
    }
#endif /*CONFIG_USE_IRQ_INTERRUPT_FOR_MTK_PLATFORM */
#endif

    return n_ret_val;
}

void drv_touch_device_register_early_suspend(void)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

#if defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM)
#ifdef CONFIG_ENABLE_NOTIFIER_FB
    g_fb_notifier.notifier_call = ms_drvInterface_touch_device_fb_notifier_callback;
    fb_register_client(&g_fb_notifier);
#else
#ifdef CONFIG_HAS_EARLYSUSPEND
    g_early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
    g_early_suspend.suspend = ms_drv_interface_touch_device_suspend; /*C9?\A8?\C6\F0??\A3\AC\B9\D8\D6ж?\AC\B9??\E7 */
    g_early_suspend.resume = ms_drv_interface_touch_device_resume;
    register_early_suspend(&g_early_suspend);
#endif
#endif /*CONFIG_ENABLE_NOTIFIER_FB */

#ifdef ALLWINNER_PLATFORM
    device_enable_async_suspend(&g_i2c_client->dev);
    pm_runtime_set_active(&g_i2c_client->dev);
    pm_runtime_get(&g_i2c_client->dev);
    pm_runtime_enable(&g_i2c_client->dev);
#endif
#endif /*CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM || CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM */
}

/* remove function is triggered when the input device is removed from input sub-system */
s32 drv_touch_device_remove(struct i2c_client *pClient)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

#if defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM)
#ifdef ALLWINNER_PLATFORM
    gpio_free(CTP_IRQ_NUMBER);
#else
    gpio_free(ms_ts_msg_ic_gpio_int);
    gpio_free(ms_ts_msg_ic_gpio_rst);
#endif

    if (g_input_device) {
#ifdef ALLWINNER_PLATFORM
        input_free_int(&(config_info.input_type), NULL);
#else
        /*destroy_workqueue(irq_work_queue_allwiner); */
        free_irq(girq, g_input_device);
#endif
        input_unregister_device(g_input_device);
        g_input_device = NULL;
    }
#ifdef CONFIG_ENABLE_TOUCH_PIN_CONTROL
    drv_touch_pin_ctrl_uninit();
#endif /*CONFIG_ENABLE_TOUCH_PIN_CONTROL */

#ifdef CONFIG_TP_HAVE_KEY
#ifdef CONFIG_ENABLE_REPORT_KEY_WITH_COORDINATE
    drv_virtual_keys_uninit();
#endif /*CONFIG_ENABLE_REPORT_KEY_WITH_COORDINATE */
#endif /*CONFIG_TP_HAVE_KEY */

#ifdef CONFIG_ENABLE_PROXIMITY_DETECTION
    drv_proximity_input_device_uninit();
#endif /*CONFIG_ENABLE_PROXIMITY_DETECTION */

#ifdef CONFIG_ENABLE_REGULATOR_POWER_ON
    drv_touch_device_regulator_power_on(false);

    if (g_regu_vdd != NULL) {
        regulator_put(g_regu_vdd);
        g_regu_vdd = NULL;
    }

    if (g_regu_vcc_i2c != NULL) {
        regulator_put(g_regu_vcc_i2c);
        g_regu_vcc_i2c = NULL;
    }
#endif /*CONFIG_ENABLE_REGULATOR_POWER_ON */

#elif defined(CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM)

#ifdef CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD
#ifdef CONFIG_ENABLE_REGULATOR_POWER_ON
    drv_touch_device_regulator_power_on(false);

    if (g_regu_vdd != NULL) {
        regulator_put(g_regu_vdd);
        g_regu_vdd = NULL;
    }
#endif /*CONFIG_ENABLE_REGULATOR_POWER_ON */
#endif /*CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD */

#endif

    if (g_touch_k_set) {
        kset_unregister(g_touch_k_set);
        g_touch_k_set = NULL;
    }

    if (g_touch_k_obj) {
        kobject_put(g_touch_k_obj);
        g_touch_k_obj = NULL;
    }
#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
    if (g_gesture_k_set) {
        kset_unregister(g_gesture_k_set);
        g_gesture_k_set = NULL;
    }

    if (g_gesture_k_obj) {
        kobject_put(g_gesture_k_obj);
        g_gesture_k_obj = NULL;
    }
#endif /*CONFIG_ENABLE_GESTURE_DEBUG_MODE */
#endif /*CONFIG_ENABLE_GESTURE_WAKEUP */

    drv_remove_procfs_dir_entry();

    drv_remove_system_dir_entry();

#ifdef CONFIG_ENABLE_JNI_INTERFACE
    drv_jni_delete_msg_tool_mem();
#endif /*CONFIG_ENABLE_JNI_INTERFACE */

#ifdef CONFIG_ENABLE_CHARGER_DETECTION
    if (g_charger_plug_in_out_check_workqueue) {
        destroy_workqueue(g_charger_plug_in_out_check_workqueue);
        g_charger_plug_in_out_check_workqueue = NULL;
    }
#endif /*CONFIG_ENABLE_CHARGER_DETECTION */

#ifdef CONFIG_ENABLE_ESD_PROTECTION
    if (g_esd_check_work_queue) {
        destroy_workqueue(g_esd_check_work_queue);
        g_esd_check_work_queue = NULL;
    }
#endif /*CONFIG_ENABLE_ESD_PROTECTION */

#ifdef CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM
#ifdef CONFIG_ENABLE_DMA_IIC
    dma_free();
#endif /*CONFIG_ENABLE_DMA_IIC */
#endif /*CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM */

    return 0;
}

/*---------------------------------------------------------------------------*/
#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
static u32 drv_point_distance(u16 n_x, u16 n_y, u16 nPrevX, u16 nPrevY)
{
    u32 n_ret_val = 0;

    n_ret_val =
        (((n_x - nPrevX) * (n_x - nPrevX)) + ((n_y - nPrevY) * (n_y - nPrevY)));

    return n_ret_val;
}
#endif /*CONFIG_ENABLE_TYPE_B_PROTOCOL */

#define CONFIG_ENABLE_APK_PRINT_FW_LOG
#ifdef CONFIG_ENABLE_APK_PRINT_FW_LOG
int apk_printf(unsigned char *pPacket, u16 n_length)
{
	int i = 0, len = 0;
	static int cnt;
	static char fwLog[256] = {0};

	if (pPacket[0] == 0x2C) {
		if (pPacket[1] == 1) {
			for (i = 0; i < (n_length-2); i++)
				fwLog[i+cnt*(n_length-2)] = pPacket[2+i];

			cnt = 0;
			printk("APK_PRINT_FW_LOG:%s\n", fwLog);
			if (g_touch_k_obj != NULL) {
				char printLog[256] = {0};
				char *pEnvp[3];

				len = strlen(fwLog);
				strlcat(printLog, "FWLOG=", sizeof(printLog));
				for (i = 6; i < len+6; i++) {
					printLog[i] = fwLog[i-6];
					fwLog[i-6] = 0;
				}

				pEnvp[0] = "STATUS=GET_FW_LOG";
				pEnvp[1] = printLog;
				pEnvp[2] = NULL;

				kobject_uevent_env(g_touch_k_obj, KOBJ_CHANGE, pEnvp);
			}
		} else {
			for (i = 0; i < (n_length-2); i++)
				fwLog[i+cnt*(n_length-2)] = pPacket[2+i];
			cnt++;
		}
		return 0;
	}

	return -EFAULT;
}
#endif

static s32 drv_self_parse_packet(u8 *p_packet, u16 n_length,
                               struct self_touch_info_t *p_info)
{                               /*for MSG22xx */
    u8 n_check_sum = 0;
    u32 nDeltaX = 0, nDeltaY = 0;
    u32 n_x = 0;
    u32 n_y = 0;
#ifdef CONFIG_SWAP_X_Y
    u32 nTempX;
    u32 nTempY;
#endif
#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
    static u8 nPrevTouchNum = 0;
    static u16 szPrevX[SELF_MAX_TOUCH_NUM] = { 0xFFFF, 0xFFFF };
    static u16 szPrevY[SELF_MAX_TOUCH_NUM] = { 0xFFFF, 0xFFFF };
    static u8 szPrevPress[SELF_MAX_TOUCH_NUM] = { 0 };
    u32 i = 0;
    u16 szX[SELF_MAX_TOUCH_NUM] = { 0 };
    u16 szY[SELF_MAX_TOUCH_NUM] = { 0 };
    u16 nTemp = 0;
    u8 nChangePoints = 0;
#endif /*CONFIG_ENABLE_TYPE_B_PROTOCOL */
    u16 nCheckSumIndex = n_length - 1;   /*Set default checksum index for demo mode */

    DBG(&g_i2c_client->dev, "check address : %d, n_length : %d\n",
        nCheckSumIndex, n_length);
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
    g_curr_press[0] = 0;
    g_curr_press[1] = 0;
#endif /*CONFIG_ENABLE_TYPE_B_PROTOCOL */

#ifdef CONFIG_ENABLE_COUNT_REPORT_RATE
    if (g_is_enable_report_rate == 1) {
        if (g_interrupt_count == 4294967295UL) {
            g_interrupt_count = 0;   /*Reset count if overflow */
            DBG(&g_i2c_client->dev, "g_interrupt_count reset to 0\n");
        }

        if (g_interrupt_count == 0) {
            /*Get start time */
            do_gettimeofday(&g_start_time);

            DBG(&g_i2c_client->dev, "Start time : %lu sec, %lu msec\n",
                g_start_time.tv_sec, g_start_time.tv_usec);
        }

        g_interrupt_count++;

        DBG(&g_i2c_client->dev, "g_interrupt_count = %d\n", g_interrupt_count);
    }
#endif /*CONFIG_ENABLE_COUNT_REPORT_RATE */
#ifdef CONFIG_ENABLE_APK_PRINT_FW_LOG
	if (0 == apk_printf(p_packet, 8))
		return -EFAULT;
#endif
    if (is_firmware_data_log_enabled) {
        if (g_firmware_mode == firmware_mode_demo_mode) {
            nCheckSumIndex = 7;
        } else if (g_firmware_mode == firmware_mode_debug_mode ||
                   g_firmware_mode == firmware_mode_raw_data_mode) {
            if (g_chip_type == CHIP_TYPE_MSG22XX && is_self_freq_scan_enabled)
                nCheckSumIndex = n_length - 1;
            else
                nCheckSumIndex = 31;
        }
#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
        if (g_gesture_wakeup_flag == 1) {
            nCheckSumIndex = n_length - 1;
        }
#endif /*CONFIG_ENABLE_GESTURE_WAKEUP */
    }                           /*is_firmware_data_log_enabled */
    DBG(&g_i2c_client->dev, "check address : %d, n_length : %d\n",
        nCheckSumIndex, n_length);
    n_check_sum = drv_calculate_check_sum(&p_packet[0], nCheckSumIndex);
    DBG(&g_i2c_client->dev, "check sum : [%x] == [%x]?\n",
        p_packet[nCheckSumIndex], n_check_sum);
    DBG(&g_i2c_client->dev, "check address : %d, n_length : %d\n",
        nCheckSumIndex, n_length);

#ifdef CONFIG_ENABLE_ESD_PROTECTION
    if (g_chip_type == CHIP_TYPE_MSG22XX) {
        if (p_packet[0] == 0xA7 && p_packet[1] == 0x00 && p_packet[2] == 0x08
            && p_packet[3] == PACKET_TYPE_ESD_CHECK_HW_RESET
            && p_packet[4] == 0xFF && p_packet[5] == 0xFF && p_packet[6] == 0xFF) {
            DBG(&g_i2c_client->dev,
                "ESD HwReset check : g_is_update_firmware=%d, g_is_hw_reset_by_driver=%d\n",
                g_is_update_firmware, g_is_hw_reset_by_driver);

            if (g_is_update_firmware == 0 && g_is_hw_reset_by_driver == 0
#ifdef CONFIG_ENABLE_ITO_MP_TEST
                && g_is_in_mp_test == 0
#endif /*CONFIG_ENABLE_ITO_MP_TEST */
                ) {
#ifdef CONFIG_ENABLE_PROXIMITY_DETECTION
                if (g_enable_tp_proximity == 1) {
                    drv_enable_proximity();

#if defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM)
                    g_face_closing_tp = 0;  /*far away for SPRD/QCOM platform */

                    if (g_proximityinput_device != NULL) {
                        input_report_abs(g_proximityinput_device, ABS_DISTANCE,
                                         1);
                        input_sync(g_proximityinput_device);
                    }
#elif defined(CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM)
                    {
                        int nErr;
                        hwm_sensor_data tSensor_data;

                        g_face_closing_tp = 1;  /*far away for MTK platform */

                        /*map and store data to hwm_sensor_data */
                        tSensor_data.values[0] = drv_get_tp_ps_data();
                        tSensor_data.value_divide = 1;
                        tSensor_data.status = SENSOR_STATUS_ACCURACY_MEDIUM;
                        /*let up layer to know */
                        if ((nErr == hwmsen_get_interrupt_data(ID_PROXIMITY,
                                                       &tSensor_data))) {
                            DBG(&g_i2c_client->dev,
                                "call hwmsen_get_interrupt_data() failed = %d\n",
                                nErr);
                        }
                    }
#endif
                }
#endif /*CONFIG_ENABLE_PROXIMITY_DETECTION */
            }

            g_is_hw_reset_by_driver = 0;    /*Reset check flag to 0 after HwReset check */

			return -EFAULT;
        }
    }
#endif /*CONFIG_ENABLE_ESD_PROTECTION */

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
    if (g_gesture_wakeup_flag == 1) {
        u8 nWakeupMode = 0;
        u8 bIsCorrectFormat = 0;

        DBG(&g_i2c_client->dev,
            "received raw data from touch panel as following:\n");
        DBG(&g_i2c_client->dev,
            "p_packet[0]=%x\n p_packet[1]=%x p_packet[2]=%x p_packet[3]=%x p_packet[4]=%x p_packet[5]=%x\n",
            p_packet[0], p_packet[1], p_packet[2], p_packet[3], p_packet[4],
            p_packet[5]);

        if (g_chip_type == CHIP_TYPE_MSG22XX && p_packet[0] == 0xA7
            && p_packet[1] == 0x00 && p_packet[2] == 0x06 &&
            p_packet[3] == PACKET_TYPE_GESTURE_WAKEUP) {
            nWakeupMode = p_packet[4];
            bIsCorrectFormat = 1;
        }
#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
        else if (g_chip_type == CHIP_TYPE_MSG22XX && p_packet[0] == 0xA7
                 && p_packet[1] == 0x00 && p_packet[2] == 0x80 &&
                 p_packet[3] == PACKET_TYPE_GESTURE_DEBUG) {
            u32 a = 0;

            nWakeupMode = p_packet[4];
            bIsCorrectFormat = 1;

            for (a = 0; a < 0x80; a++) {
                g_log_gesture_debug[a] = p_packet[a];
            }

            if (!(p_packet[5] >> 7)) {   /*LCM Light Flag = 0 */
                nWakeupMode = 0xFE;
                DBG(&g_i2c_client->dev, "gesture debug mode LCM flag = 0\n");
            }
        }
#endif /*CONFIG_ENABLE_GESTURE_DEBUG_MODE */

#ifdef CONFIG_ENABLE_GESTURE_INFORMATION_MODE
        else if (g_chip_type == CHIP_TYPE_MSG22XX && p_packet[0] == 0xA7
                 && p_packet[1] == 0x00 && p_packet[2] == 0x80 &&
                 p_packet[3] == PACKET_TYPE_GESTURE_INFORMATION) {
            u32 a = 0;
            u32 nTmpCount = 0;
            u32 nWidth = 0;
            u32 nHeight = 0;

            nWakeupMode = p_packet[4];
            bIsCorrectFormat = 1;

            for (a = 0; a < 6; a++) {   /*header */
                g_log_gesture_infor[nTmpCount] = p_packet[a];
                nTmpCount++;
            }

            for (a = 6; a < 126; a = a + 3) {   /*parse packet to coordinate */
                u32 nTran_x = 0;
                u32 nTran_y = 0;

                drv_convert_gesture_information_mode_coordinate(&p_packet[a],
                                                            &nTran_x, &nTran_y);
                g_log_gesture_infor[nTmpCount] = nTran_x;
                nTmpCount++;
                g_log_gesture_infor[nTmpCount] = nTran_y;
                nTmpCount++;
            }

            nWidth = (((p_packet[12] & 0xF0) << 4) | p_packet[13]);   /*parse width & height */
            nHeight = (((p_packet[12] & 0x0F) << 8) | p_packet[14]);

            DBG(&g_i2c_client->dev, "Before convert [width,height]=[%d,%d]\n",
                nWidth, nHeight);

            if ((p_packet[12] == 0xFF) && (p_packet[13] == 0xFF)
                && (p_packet[14] == 0xFF)) {
                nWidth = 0;
                nHeight = 0;
            } else {
                nWidth = (nWidth * TOUCH_SCREEN_X_MAX) / TPD_WIDTH;
                nHeight = (nHeight * TOUCH_SCREEN_Y_MAX) / TPD_HEIGHT;
                DBG(&g_i2c_client->dev,
                    "After convert [width,height]=[%d,%d]\n", nWidth, nHeight);
            }

            g_log_gesture_infor[10] = nWidth;
            g_log_gesture_infor[11] = nHeight;

            g_log_gesture_infor[nTmpCount] = p_packet[126];  /*Dummy */
            nTmpCount++;
            g_log_gesture_infor[nTmpCount] = p_packet[127];  /*checksum */
            nTmpCount++;
            DBG(&g_i2c_client->dev, "gesture information mode Count = %d\n",
                nTmpCount);
        }
#endif /*CONFIG_ENABLE_GESTURE_INFORMATION_MODE */

        if (bIsCorrectFormat) {
            DBG(&g_i2c_client->dev, "nWakeupMode = 0x%x\n", nWakeupMode);

            switch (nWakeupMode) {
            case 0x58:
                g_gesture_wakeup_value[0] =
                    GESTURE_WAKEUP_MODE_DOUBLE_CLICK_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by DOUBLE_CLICK gesture wakeup.\n");

                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x60:
                g_gesture_wakeup_value[0] = GESTURE_WAKEUP_MODE_UP_DIRECT_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by UP_DIRECT gesture wakeup.\n");

/*input_report_key(g_input_device, KEY_UP, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, KEY_UP, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x61:
                g_gesture_wakeup_value[0] =
                    GESTURE_WAKEUP_MODE_DOWN_DIRECT_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by DOWN_DIRECT gesture wakeup.\n");

/*input_report_key(g_input_device, KEY_DOWN, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, KEY_DOWN, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x62:
                g_gesture_wakeup_value[0] =
                    GESTURE_WAKEUP_MODE_LEFT_DIRECT_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by LEFT_DIRECT gesture wakeup.\n");

/*input_report_key(g_input_device, KEY_LEFT, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, KEY_LEFT, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x63:
                g_gesture_wakeup_value[0] =
                    GESTURE_WAKEUP_MODE_RIGHT_DIRECT_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by RIGHT_DIRECT gesture wakeup.\n");

/*input_report_key(g_input_device, KEY_RIGHT, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, KEY_RIGHT, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x64:
                g_gesture_wakeup_value[0] =
                    GESTURE_WAKEUP_MODE_m_CHARACTER_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by m_CHARACTER gesture wakeup.\n");

/*input_report_key(g_input_device, KEY_M, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, KEY_M, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x65:
                g_gesture_wakeup_value[0] =
                    GESTURE_WAKEUP_MODE_W_CHARACTER_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by W_CHARACTER gesture wakeup.\n");

/*input_report_key(g_input_device, KEY_W, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, KEY_W, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x66:
                g_gesture_wakeup_value[0] =
                    GESTURE_WAKEUP_MODE_C_CHARACTER_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by C_CHARACTER gesture wakeup.\n");

/*input_report_key(g_input_device, KEY_C, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, KEY_C, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x67:
                g_gesture_wakeup_value[0] =
                    GESTURE_WAKEUP_MODE_e_CHARACTER_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by e_CHARACTER gesture wakeup.\n");

/*input_report_key(g_input_device, KEY_E, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, KEY_E, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x68:
                g_gesture_wakeup_value[0] =
                    GESTURE_WAKEUP_MODE_V_CHARACTER_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by V_CHARACTER gesture wakeup.\n");

/*input_report_key(g_input_device, KEY_V, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, KEY_V, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x69:
                g_gesture_wakeup_value[0] =
                    GESTURE_WAKEUP_MODE_O_CHARACTER_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by O_CHARACTER gesture wakeup.\n");

/*input_report_key(g_input_device, KEY_O, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, KEY_O, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x6A:
                g_gesture_wakeup_value[0] =
                    GESTURE_WAKEUP_MODE_S_CHARACTER_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by S_CHARACTER gesture wakeup.\n");

/*input_report_key(g_input_device, KEY_S, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, KEY_S, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x6B:
                g_gesture_wakeup_value[0] =
                    GESTURE_WAKEUP_MODE_Z_CHARACTER_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by Z_CHARACTER gesture wakeup.\n");

/*input_report_key(g_input_device, KEY_Z, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, KEY_Z, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x6C:
                g_gesture_wakeup_value[0] = GESTURE_WAKEUP_MODE_RESERVE1_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE1_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER1, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER1, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x6D:
                g_gesture_wakeup_value[0] = GESTURE_WAKEUP_MODE_RESERVE2_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE2_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER2, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER2, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x6E:
                g_gesture_wakeup_value[0] = GESTURE_WAKEUP_MODE_RESERVE3_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE3_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER3, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER3, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
#ifdef CONFIG_SUPPORT_64_TYPES_GESTURE_WAKEUP_MODE
            case 0x6F:
                g_gesture_wakeup_value[0] = GESTURE_WAKEUP_MODE_RESERVE4_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE4_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER4, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER4, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x70:
                g_gesture_wakeup_value[0] = GESTURE_WAKEUP_MODE_RESERVE5_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE5_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER5, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER5, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x71:
                g_gesture_wakeup_value[0] = GESTURE_WAKEUP_MODE_RESERVE6_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE6_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER6, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER6, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x72:
                g_gesture_wakeup_value[0] = GESTURE_WAKEUP_MODE_RESERVE7_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE7_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER7, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER7, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x73:
                g_gesture_wakeup_value[0] = GESTURE_WAKEUP_MODE_RESERVE8_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE8_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER8, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER8, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x74:
                g_gesture_wakeup_value[0] = GESTURE_WAKEUP_MODE_RESERVE9_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE9_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER9, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER9, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x75:
                g_gesture_wakeup_value[0] = GESTURE_WAKEUP_MODE_RESERVE10_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE10_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER10, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER10, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x76:
                g_gesture_wakeup_value[0] = GESTURE_WAKEUP_MODE_RESERVE11_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE11_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER11, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER11, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x77:
                g_gesture_wakeup_value[0] = GESTURE_WAKEUP_MODE_RESERVE12_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE12_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER12, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER12, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x78:
                g_gesture_wakeup_value[0] = GESTURE_WAKEUP_MODE_RESERVE13_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE13_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER13, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER13, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x79:
                g_gesture_wakeup_value[0] = GESTURE_WAKEUP_MODE_RESERVE14_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE14_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER14, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER14, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x7A:
                g_gesture_wakeup_value[0] = GESTURE_WAKEUP_MODE_RESERVE15_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE15_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER15, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER15, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x7B:
                g_gesture_wakeup_value[0] = GESTURE_WAKEUP_MODE_RESERVE16_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE16_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER16, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER16, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x7C:
                g_gesture_wakeup_value[0] = GESTURE_WAKEUP_MODE_RESERVE17_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE17_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER17, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER17, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x7D:
                g_gesture_wakeup_value[0] = GESTURE_WAKEUP_MODE_RESERVE18_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE18_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER18, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER18, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x7E:
                g_gesture_wakeup_value[0] = GESTURE_WAKEUP_MODE_RESERVE19_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE19_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER19, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER19, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x7F:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE20_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE20_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER20, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER20, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x80:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE21_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE21_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER21, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER21, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x81:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE22_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE22_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER22, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER22, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x82:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE23_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE23_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER23, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER23, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x83:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE24_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE24_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER24, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER24, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x84:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE25_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE25_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER25, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER25, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x85:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE26_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE26_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER26, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER26, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x86:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE27_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE27_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER27, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER27, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x87:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE28_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE28_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER28, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER28, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x88:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE29_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE29_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER29, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER29, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x89:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE30_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE30_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER30, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER30, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x8A:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE31_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE31_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER31, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER31, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x8B:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE32_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE32_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER32, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER32, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x8C:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE33_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE33_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER33, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER33, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x8D:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE34_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE34_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER34, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER34, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x8E:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE35_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE35_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER35, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER35, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x8F:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE36_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE36_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER36, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER36, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x90:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE37_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE37_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER37, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER37, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x91:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE38_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE38_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER38, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER38, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x92:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE39_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE39_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER39, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER39, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x93:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE40_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE40_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER40, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER40, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x94:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE41_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE41_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER41, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER41, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x95:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE42_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE42_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER42, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER42, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x96:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE43_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE43_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER43, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER43, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x97:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE44_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE44_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER44, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER44, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x98:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE45_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE45_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER45, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER45, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x99:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE46_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE46_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER46, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER46, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x9A:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE47_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE47_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER47, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER47, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x9B:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE48_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE48_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER48, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER48, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x9C:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE49_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE49_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER49, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER49, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x9D:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE50_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE50_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER50, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER50, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x9E:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE51_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE51_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER51, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER51, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
#endif /*CONFIG_SUPPORT_64_TYPES_GESTURE_WAKEUP_MODE */

#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
            case 0xFF:         /*Gesture Fail */
                g_gesture_wakeup_value[1] = 0xFF;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_FAIL gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER51, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER51, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
#endif /*CONFIG_ENABLE_GESTURE_DEBUG_MODE */
            default:
                g_gesture_wakeup_value[0] = 0;
                g_gesture_wakeup_value[1] = 0;
                DBG(&g_i2c_client->dev,
                    "Un-supported gesture wakeup mode. Please check your device driver code.\n");
                break;
            }

            DBG(&g_i2c_client->dev, "g_gesture_wakeup_value[0] = 0x%x\n",
                g_gesture_wakeup_value[0]);
            DBG(&g_i2c_client->dev, "g_gesture_wakeup_value[1] = 0x%x\n",
                g_gesture_wakeup_value[1]);
        } else {
            DBG(&g_i2c_client->dev,
                "gesture wakeup packet format is incorrect.\n");
        }

#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
        /*Notify android application to retrieve log data mode packet from device driver by sysfs. */
        if (g_gesture_k_obj != NULL && p_packet[3] == PACKET_TYPE_GESTURE_DEBUG) {
            char *pEnvp[2];
            s32 n_ret_val = 0;

            pEnvp[0] = "STATUS=GET_GESTURE_DEBUG";
            pEnvp[1] = NULL;

            n_ret_val = kobject_uevent_env(g_gesture_k_obj, KOBJ_CHANGE, pEnvp);
            DBG(&g_i2c_client->dev, "kobject_uevent_env() n_ret_val = %d\n",
                n_ret_val);
        }
#endif /*CONFIG_ENABLE_GESTURE_DEBUG_MODE */

		return -EFAULT;
    }
#endif /*CONFIG_ENABLE_GESTURE_WAKEUP */

    DBG(&g_i2c_client->dev,
        "received raw data from touch panel as following:\n");
    DBG(&g_i2c_client->dev,
        "p_packet[0]=%x\n p_packet[1]=%x p_packet[2]=%x p_packet[3]=%x p_packet[4]=%x\n p_packet[5]=%x p_packet[6]=%x p_packet[7]=%x\n",
        p_packet[0], p_packet[1], p_packet[2], p_packet[3], p_packet[4], p_packet[5],
        p_packet[6], p_packet[7]);

    if ((p_packet[nCheckSumIndex] == n_check_sum) && (p_packet[0] == 0x52)) {   /*check the checksum of packet */
        n_x = (((p_packet[1] & 0xF0) << 4) | p_packet[2]); /*parse the packet to coordinate */
        n_y = (((p_packet[1] & 0x0F) << 8) | p_packet[3]);

        nDeltaX = (((p_packet[4] & 0xF0) << 4) | p_packet[5]);
        nDeltaY = (((p_packet[4] & 0x0F) << 8) | p_packet[6]);

        DBG(&g_i2c_client->dev, "[x,y]=[%d,%d]\n", n_x, n_y);
        DBG(&g_i2c_client->dev, "[delta_x,delta_y]=[%d,%d]\n", nDeltaX,
            nDeltaY);

#ifdef CONFIG_SWAP_X_Y
        nTempY = n_x;
        nTempX = n_y;
        n_x = nTempX;
        n_y = nTempY;

        nTempY = nDeltaX;
        nTempX = nDeltaY;
        nDeltaX = nTempX;
        nDeltaY = nTempY;
#endif

#ifdef CONFIG_REVERSE_X
        n_x = 2047 - n_x;
        nDeltaX = 4095 - nDeltaX;
#endif

#ifdef CONFIG_REVERSE_Y
        n_y = 2047 - n_y;
        nDeltaY = 4095 - nDeltaY;
#endif

        /*
         *p_packet[0]:id, p_packet[1]~p_packet[3]:the first point abs, p_packet[4]~p_packet[6]:the relative distance between the first point abs and the second point abs
         * when p_packet[1]~p_packet[4], p_packet[6] is 0xFF, keyevent, p_packet[5] to judge which key press.
         *p_packet[1]~p_packet[6] all are 0xFF, release touch
         */
        if ((p_packet[1] == 0xFF) && (p_packet[2] == 0xFF) && (p_packet[3] == 0xFF)
            && (p_packet[4] == 0xFF) && (p_packet[6] == 0xFF)) {
            p_info->tPoint[0].n_x = 0;    /*final X coordinate */
            p_info->tPoint[0].n_y = 0;    /*final Y coordinate */

            if ((p_packet[5] != 0x00) && (p_packet[5] != 0xFF)) { /*p_packet[5] is key value *//* 0x00 is key up, 0xff is touch screen up */
#ifdef CONFIG_ENABLE_PROXIMITY_DETECTION
#if defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM)
                DBG(&g_i2c_client->dev,
                    "g_enable_tp_proximity = %d, p_packet[5] = 0x%x\n",
                    g_enable_tp_proximity, p_packet[5]);

                if (g_enable_tp_proximity &&
                    ((p_packet[5] == 0x80) || (p_packet[5] == 0x40))) {
                    if (p_packet[5] == 0x80) {
                        g_face_closing_tp = 1;

                        input_report_abs(g_proximityinput_device, ABS_DISTANCE,
                                         0);
                        input_sync(g_proximityinput_device);
                    } else if (p_packet[5] == 0x40) {    /*far away */
                        g_face_closing_tp = 0;

                        input_report_abs(g_proximityinput_device, ABS_DISTANCE,
                                         1);
                        input_sync(g_proximityinput_device);
                    }

                    DBG(&g_i2c_client->dev, "g_face_closing_tp = %d\n",
                        g_face_closing_tp);

					return -EFAULT;
                }
#elif defined(CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM)
                if (g_enable_tp_proximity &&
                    ((p_packet[5] == 0x80) || (p_packet[5] == 0x40))) {
                    int nErr;
                    hwm_sensor_data tSensor_data;

                    if (p_packet[5] == 0x80) {   /*close to */
                        g_face_closing_tp = 0;
                    } else if (p_packet[5] == 0x40) {    /*far away */
                        g_face_closing_tp = 1;
                    }

                    DBG(&g_i2c_client->dev, "g_face_closing_tp = %d\n",
                        g_face_closing_tp);

                    /*map and store data to hwm_sensor_data */
                    tSensor_data.values[0] = drv_get_tp_ps_data();
                    tSensor_data.value_divide = 1;
                    tSensor_data.status = SENSOR_STATUS_ACCURACY_MEDIUM;
                    /*let up layer to know */
                    if ((nErr == hwmsen_get_interrupt_data(ID_PROXIMITY,
                                                   &tSensor_data))) {
                        DBG(&g_i2c_client->dev,
                            "call hwmsen_get_interrupt_data() failed = %d\n",
                            nErr);
                    }

					return -EFAULT;
                }
#endif
#endif /*CONFIG_ENABLE_PROXIMITY_DETECTION */

                /* 0x00 is key up, 0xff is touch screen up */
                DBG(&g_i2c_client->dev, "touch key down p_packet[5]=%d\n",
                    p_packet[5]);

                p_info->n_finger_num = 1;
                p_info->n_touch_key_code = p_packet[5];
                p_info->n_touch_key_mode = 1;

#ifdef CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM
#ifdef CONFIG_ENABLE_REPORT_KEY_WITH_COORDINATE
                p_info->n_finger_num = 1;
                p_info->n_touch_key_code = 0;
                p_info->n_touch_key_mode = 0;

#ifdef CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD
                if (tpd_dts_data.use_tpd_button) {
                    if (p_packet[5] == 4) {  /*TOUCH_KEY_HOME */
                        p_info->tPoint[0].n_x =
                            tpd_dts_data.tpd_key_dim_local[1].key_x;
                        p_info->tPoint[0].n_y =
                            tpd_dts_data.tpd_key_dim_local[1].key_y;
                    } else if (p_packet[5] == 1) {   /*TOUCH_KEY_MENU */
                        p_info->tPoint[0].n_x =
                            tpd_dts_data.tpd_key_dim_local[0].key_x;
                        p_info->tPoint[0].n_y =
                            tpd_dts_data.tpd_key_dim_local[0].key_y;
                    } else if (p_packet[5] == 2) {   /*TOUCH_KEY_BACK */
                        p_info->tPoint[0].n_x =
                            tpd_dts_data.tpd_key_dim_local[2].key_x;
                        p_info->tPoint[0].n_y =
                            tpd_dts_data.tpd_key_dim_local[2].key_y;
                    } else if (p_packet[5] == 8) {   /*TOUCH_KEY_SEARCH */
                        p_info->tPoint[0].n_x =
                            tpd_dts_data.tpd_key_dim_local[3].key_x;
                        p_info->tPoint[0].n_y =
                            tpd_dts_data.tpd_key_dim_local[3].key_y;
                    } else {
                        DBG(&g_i2c_client->dev, "multi-key is pressed.\n");

						return -EFAULT;
                    }

#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
                    g_curr_press[0] = 1;
                    g_curr_press[1] = 0;
#endif /*CONFIG_ENABLE_TYPE_B_PROTOCOL */
                }
#else
                if (p_packet[5] == 4) {  /*TOUCH_KEY_HOME */
                    p_info->tPoint[0].n_x = g_tp_virtual_key_dim_local[1][0];
                    p_info->tPoint[0].n_y = g_tp_virtual_key_dim_local[1][1];
                } else if (p_packet[5] == 1) {   /*TOUCH_KEY_MENU */
                    p_info->tPoint[0].n_x = g_tp_virtual_key_dim_local[0][0];
                    p_info->tPoint[0].n_y = g_tp_virtual_key_dim_local[0][1];
                } else if (p_packet[5] == 2) {   /*TOUCH_KEY_BACK */
                    p_info->tPoint[0].n_x = g_tp_virtual_key_dim_local[2][0];
                    p_info->tPoint[0].n_y = g_tp_virtual_key_dim_local[2][1];
                } else if (p_packet[5] == 8) {   /*TOUCH_KEY_SEARCH */
                    p_info->tPoint[0].n_x = g_tp_virtual_key_dim_local[3][0];
                    p_info->tPoint[0].n_y = g_tp_virtual_key_dim_local[3][1];
                } else {
                    DBG(&g_i2c_client->dev, "multi-key is pressed.\n");

					return -EFAULT;
                }

#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
                g_curr_press[0] = 1;
                g_curr_press[1] = 0;
#endif /*CONFIG_ENABLE_TYPE_B_PROTOCOL */

#endif /*CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD */

#endif /*CONFIG_ENABLE_REPORT_KEY_WITH_COORDINATE */
#endif /*CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM */
            } else {            /* key up or touch up */
                DBG(&g_i2c_client->dev, "touch end\n");
                p_info->n_finger_num = 0;  /*touch end */
                p_info->n_touch_key_code = 0;
                p_info->n_touch_key_mode = 0;
            }

#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
            g_prev_touch_status = 0;
#endif /*CONFIG_ENABLE_TYPE_B_PROTOCOL */
        }

        else {
            p_info->n_touch_key_mode = 0;   /*Touch on screen... */

/*if ((nDeltaX == 0) && (nDeltaY == 0))*/
            if (
#ifdef CONFIG_REVERSE_X
                   (nDeltaX == 4095)
#else
                   (nDeltaX == 0)
#endif
                   &&
#ifdef CONFIG_REVERSE_Y
                   (nDeltaY == 4095)
#else
                   (nDeltaY == 0)
#endif
                ) {             /* one touch point */
                p_info->n_finger_num = 1;  /*one touch */
                p_info->tPoint[0].n_x = (n_x * TOUCH_SCREEN_X_MAX) / TPD_WIDTH;
                p_info->tPoint[0].n_y = (n_y * TOUCH_SCREEN_Y_MAX) / TPD_HEIGHT;
                DBG(&g_i2c_client->dev, "[%s]: [x,y]=[%d,%d]\n", __func__, n_x,
                    n_y);
                DBG(&g_i2c_client->dev, "[%s]: point[x,y]=[%d,%d]\n", __func__,
                    p_info->tPoint[0].n_x, p_info->tPoint[0].n_y);

#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
                g_curr_press[0] = 1;
                g_curr_press[1] = 0;
#endif /*CONFIG_ENABLE_TYPE_B_PROTOCOL */
            } else {            /* two touch points */
                u32 n_x2, n_y2;

                p_info->n_finger_num = 2;  /*two touch */
                /* Finger 1 */
                p_info->tPoint[0].n_x = (n_x * TOUCH_SCREEN_X_MAX) / TPD_WIDTH;
                p_info->tPoint[0].n_y = (n_y * TOUCH_SCREEN_Y_MAX) / TPD_HEIGHT;
                DBG(&g_i2c_client->dev, "[%s]: point1[x,y]=[%d,%d]\n", __func__,
                    p_info->tPoint[0].n_x, p_info->tPoint[0].n_y);
                /* Finger 2 */
                if (nDeltaX > 2048) {   /*transform the unsigned value to signed value */
                    nDeltaX -= 4096;
                }

                if (nDeltaY > 2048) {
                    nDeltaY -= 4096;
                }

                n_x2 = (u32) (n_x + nDeltaX);
                n_y2 = (u32) (n_y + nDeltaY);

                p_info->tPoint[1].n_x = (n_x2 * TOUCH_SCREEN_X_MAX) / TPD_WIDTH;
                p_info->tPoint[1].n_y = (n_y2 * TOUCH_SCREEN_Y_MAX) / TPD_HEIGHT;
                DBG(&g_i2c_client->dev, "[%s]: point2[x,y]=[%d,%d]\n", __func__,
                    p_info->tPoint[1].n_x, p_info->tPoint[1].n_y);

#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
                g_curr_press[0] = 1;
                g_curr_press[1] = 1;
#endif /*CONFIG_ENABLE_TYPE_B_PROTOCOL */
            }

#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
            if (g_prev_touch_status == 1) {
                for (i = 0; i < max_touch_num; i++) {
                    szX[i] = p_info->tPoint[i].n_x;
                    szY[i] = p_info->tPoint[i].n_y;
                }

                if (/*(p_info->n_finger_num == 1)&& */ (nPrevTouchNum == 2)) {
                    if (drv_point_distance
                        (szX[0], szY[0], szPrevX[0],
                         szPrevY[0]) > drv_point_distance(szX[0], szY[0],
                                                         szPrevX[1],
                                                         szPrevY[1])) {
                        nChangePoints = 1;
                    }
                } else if ((p_info->n_finger_num == 2) && (nPrevTouchNum == 1)) {
                    if (szPrevPress[0] == 1) {
                        if (drv_point_distance
                            (szX[0], szY[0], szPrevX[0],
                             szPrevY[0]) > drv_point_distance(szX[1], szY[1],
                                                             szPrevX[0],
                                                             szPrevY[0])) {
                            nChangePoints = 1;
                        }
                    } else {
                        if (drv_point_distance
                            (szX[0], szY[0], szPrevX[1],
                             szPrevY[1]) < drv_point_distance(szX[1], szY[1],
                                                             szPrevX[1],
                                                             szPrevY[1])) {
                            nChangePoints = 1;
                        }
                    }
                } else if ((p_info->n_finger_num == 1) && (nPrevTouchNum == 1)) {
                    if (g_curr_press[0] != szPrevPress[0]) {
                        nChangePoints = 1;
                    }
                }

/*else if ((p_info->n_finger_num == 2) && (nPrevTouchNum == 2))*/

/*{*/

/*}*/

                if (nChangePoints == 1) {
                    nTemp = g_curr_press[0];
                    g_curr_press[0] = g_curr_press[1];
                    g_curr_press[1] = nTemp;

                    nTemp = p_info->tPoint[0].n_x;
                    p_info->tPoint[0].n_x = p_info->tPoint[1].n_x;
                    p_info->tPoint[1].n_x = nTemp;

                    nTemp = p_info->tPoint[0].n_y;
                    p_info->tPoint[0].n_y = p_info->tPoint[1].n_y;
                    p_info->tPoint[1].n_y = nTemp;
                }
            }

            /*Save current status */
            for (i = 0; i < max_touch_num; i++) {
                szPrevPress[i] = g_curr_press[i];
                szPrevX[i] = p_info->tPoint[i].n_x;
                szPrevY[i] = p_info->tPoint[i].n_y;
            }
            nPrevTouchNum = p_info->n_finger_num;

            g_prev_touch_status = 1;
#endif /*CONFIG_ENABLE_TYPE_B_PROTOCOL */
        }
    }

    else if (p_packet[nCheckSumIndex] == n_check_sum && p_packet[0] == 0x62) {
        n_x = ((p_packet[1] << 8) | p_packet[2]);  /*Position_x */
        n_y = ((p_packet[3] << 8) | p_packet[4]);  /*Position_Y */

        nDeltaX = ((p_packet[13] << 8) | p_packet[14]);   /*Distance_X */
        nDeltaY = ((p_packet[15] << 8) | p_packet[16]);   /*Distance_Y */

        DBG(&g_i2c_client->dev, "[x,y]=[%d,%d]\n", n_x, n_y);
        DBG(&g_i2c_client->dev, "[delta_x,delta_y]=[%d,%d]\n", nDeltaX,
            nDeltaY);

#ifdef CONFIG_SWAP_X_Y
        nTempY = n_x;
        nTempX = n_y;
        n_x = nTempX;
        n_y = nTempY;

        nTempY = nDeltaX;
        nTempX = nDeltaY;
        nDeltaX = nTempX;
        nDeltaY = nTempY;
#endif

#ifdef CONFIG_REVERSE_X
        n_x = 2047 - n_x;
        nDeltaX = 4095 - nDeltaX;
#endif

#ifdef CONFIG_REVERSE_Y
        n_y = 2047 - n_y;
        nDeltaY = 4095 - nDeltaY;
#endif

        /*
         *p_packet[0]:id, p_packet[1]~p_packet[4]:the first point abs, p_packet[13]~p_packet[16]:the relative distance between the first point abs and the second point abs
         * when p_packet[1]~p_packet[7] is 0xFF, keyevent, p_packet[8] to judge which key press.
         *p_packet[1]~p_packet[8] all are 0xFF, release touch
         */
        if ((p_packet[1] == 0xFF) && (p_packet[2] == 0xFF) && (p_packet[3] == 0xFF)
            && (p_packet[4] == 0xFF) && (p_packet[5] == 0xFF)
            && (p_packet[6] == 0xFF) && (p_packet[7] == 0xFF)) {
            p_info->tPoint[0].n_x = 0;    /*final X coordinate */
            p_info->tPoint[0].n_y = 0;    /*final Y coordinate */

            if ((p_packet[8] != 0x00) && (p_packet[8] != 0xFF)) { /*p_packet[8] is key value *//* 0x00 is key up, 0xff is touch screen up */
                DBG(&g_i2c_client->dev, "touch key down p_packet[8]=%d\n",
                    p_packet[8]);
                p_info->n_finger_num = 1;
                p_info->n_touch_key_code = p_packet[8];
                p_info->n_touch_key_mode = 1;

#ifdef CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM
#ifdef CONFIG_ENABLE_REPORT_KEY_WITH_COORDINATE
                p_info->n_finger_num = 1;
                p_info->n_touch_key_code = 0;
                p_info->n_touch_key_mode = 0;

#ifdef CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD
                if (tpd_dts_data.use_tpd_button) {
                    if (p_packet[8] == 4) {
                        p_info->tPoint[0].n_x =
                            tpd_dts_data.tpd_key_dim_local[1].key_x;
                        p_info->tPoint[0].n_y =
                            tpd_dts_data.tpd_key_dim_local[1].key_y;
                    } else if (p_packet[8] == 1) {   /*TOUCH_KEY_MENU */
                        p_info->tPoint[0].n_x =
                            tpd_dts_data.tpd_key_dim_local[0].key_x;
                        p_info->tPoint[0].n_y =
                            tpd_dts_data.tpd_key_dim_local[0].key_y;
                    } else if (p_packet[8] == 2) {   /*TOUCH_KEY_BACK */
                        p_info->tPoint[0].n_x =
                            tpd_dts_data.tpd_key_dim_local[2].key_x;
                        p_info->tPoint[0].n_y =
                            tpd_dts_data.tpd_key_dim_local[2].key_y;
                    } else if (p_packet[8] == 8) {   /*TOUCH_KEY_SEARCH */
                        p_info->tPoint[0].n_x =
                            tpd_dts_data.tpd_key_dim_local[3].key_x;
                        p_info->tPoint[0].n_y =
                            tpd_dts_data.tpd_key_dim_local[3].key_y;
                    } else {
                        DBG(&g_i2c_client->dev, "multi-key is pressed.\n");

						return -EFAULT;
                    }

#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
                    g_curr_press[0] = 1;
                    g_curr_press[1] = 0;
#endif /*CONFIG_ENABLE_TYPE_B_PROTOCOL */
                }
#else
                if (p_packet[8] == 4) {  /*TOUCH_KEY_HOME */
                    p_info->tPoint[0].n_x = g_tp_virtual_key_dim_local[1][0];
                    p_info->tPoint[0].n_y = g_tp_virtual_key_dim_local[1][1];
                } else if (p_packet[8] == 1) {   /*TOUCH_KEY_MENU */
                    p_info->tPoint[0].n_x = g_tp_virtual_key_dim_local[0][0];
                    p_info->tPoint[0].n_y = g_tp_virtual_key_dim_local[0][1];
                } else if (p_packet[8] == 2) {   /*TOUCH_KEY_BACK */
                    p_info->tPoint[0].n_x = g_tp_virtual_key_dim_local[2][0];
                    p_info->tPoint[0].n_y = g_tp_virtual_key_dim_local[2][1];
                } else if (p_packet[8] == 8) {   /*TOUCH_KEY_SEARCH */
                    p_info->tPoint[0].n_x = g_tp_virtual_key_dim_local[3][0];
                    p_info->tPoint[0].n_y = g_tp_virtual_key_dim_local[3][1];
                } else {
                    DBG(&g_i2c_client->dev, "multi-key is pressed.\n");

					return -EFAULT;
                }

#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
                g_curr_press[0] = 1;
                g_curr_press[1] = 0;
#endif /*CONFIG_ENABLE_TYPE_B_PROTOCOL */

#endif /*CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD */

#endif /*CONFIG_ENABLE_REPORT_KEY_WITH_COORDINATE */
#endif /*CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM */
            } else {            /* key up or touch up */
                DBG(&g_i2c_client->dev, "touch end\n");
                p_info->n_finger_num = 0;  /*touch end */
                p_info->n_touch_key_code = 0;
                p_info->n_touch_key_mode = 0;
            }

#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
            g_prev_touch_status = 0;
#endif /*CONFIG_ENABLE_TYPE_B_PROTOCOL */
        }

        else {
            p_info->n_touch_key_mode = 0;   /*Touch on screen... */

/*if ((nDeltaX == 0) && (nDeltaY == 0))*/
            if (
#ifdef CONFIG_REVERSE_X
                   (nDeltaX == 4095)
#else
                   (nDeltaX == 0)
#endif
                   &&
#ifdef CONFIG_REVERSE_Y
                   (nDeltaY == 4095)
#else
                   (nDeltaY == 0)
#endif
                ) {             /* one touch point */
                p_info->n_finger_num = 1;  /*one touch */
                p_info->tPoint[0].n_x = (n_x * TOUCH_SCREEN_X_MAX) / TPD_WIDTH;
                p_info->tPoint[0].n_y = (n_y * TOUCH_SCREEN_Y_MAX) / TPD_HEIGHT;
                DBG(&g_i2c_client->dev, "[%s]: [x,y]=[%d,%d]\n", __func__, n_x,
                    n_y);
                DBG(&g_i2c_client->dev, "[%s]: point[x,y]=[%d,%d]\n", __func__,
                    p_info->tPoint[0].n_x, p_info->tPoint[0].n_y);

#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
                g_curr_press[0] = 1;
                g_curr_press[1] = 0;
#endif /*CONFIG_ENABLE_TYPE_B_PROTOCOL */
            } else {            /* two touch points */
                u32 n_x2, n_y2;

                p_info->n_finger_num = 2;  /*two touch */
                /* Finger 1 */
                p_info->tPoint[0].n_x = (n_x * TOUCH_SCREEN_X_MAX) / TPD_WIDTH;
                p_info->tPoint[0].n_y = (n_y * TOUCH_SCREEN_Y_MAX) / TPD_HEIGHT;
                DBG(&g_i2c_client->dev, "[%s]: point1[x,y]=[%d,%d]\n", __func__,
                    p_info->tPoint[0].n_x, p_info->tPoint[0].n_y);
                /* Finger 2 */
                if (nDeltaX > 2048) {   /*transform the unsigned value to signed value */
                    nDeltaX -= 4096;
                }

                if (nDeltaY > 2048) {
                    nDeltaY -= 4096;
                }

                n_x2 = (u32) (n_x + nDeltaX);
                n_y2 = (u32) (n_y + nDeltaY);

                p_info->tPoint[1].n_x = (n_x2 * TOUCH_SCREEN_X_MAX) / TPD_WIDTH;
                p_info->tPoint[1].n_y = (n_y2 * TOUCH_SCREEN_Y_MAX) / TPD_HEIGHT;
                DBG(&g_i2c_client->dev, "[%s]: point2[x,y]=[%d,%d]\n", __func__,
                    p_info->tPoint[1].n_x, p_info->tPoint[1].n_y);

#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
                g_curr_press[0] = 1;
                g_curr_press[1] = 1;
#endif /*CONFIG_ENABLE_TYPE_B_PROTOCOL */
            }

#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
            if (g_prev_touch_status == 1) {
                for (i = 0; i < max_touch_num; i++) {
                    szX[i] = p_info->tPoint[i].n_x;
                    szY[i] = p_info->tPoint[i].n_y;
                }

                if (/*(p_info->n_finger_num == 1)&& */ (nPrevTouchNum == 2)) {
                    if (drv_point_distance
                        (szX[0], szY[0], szPrevX[0],
                         szPrevY[0]) > drv_point_distance(szX[0], szY[0],
                                                         szPrevX[1],
                                                         szPrevY[1])) {
                        nChangePoints = 1;
                    }
                } else if ((p_info->n_finger_num == 2) && (nPrevTouchNum == 1)) {
                    if (szPrevPress[0] == 1) {
                        if (drv_point_distance
                            (szX[0], szY[0], szPrevX[0],
                             szPrevY[0]) > drv_point_distance(szX[1], szY[1],
                                                             szPrevX[0],
                                                             szPrevY[0])) {
                            nChangePoints = 1;
                        }
                    } else {
                        if (drv_point_distance
                            (szX[0], szY[0], szPrevX[1],
                             szPrevY[1]) < drv_point_distance(szX[1], szY[1],
                                                             szPrevX[1],
                                                             szPrevY[1])) {
                            nChangePoints = 1;
                        }
                    }
                } else if ((p_info->n_finger_num == 1) && (nPrevTouchNum == 1)) {
                    if (g_curr_press[0] != szPrevPress[0]) {
                        nChangePoints = 1;
                    }
                }

/*else if ((p_info->n_finger_num == 2) && (nPrevTouchNum == 2))*/

/*{*/

/*}*/

                if (nChangePoints == 1) {
                    nTemp = g_curr_press[0];
                    g_curr_press[0] = g_curr_press[1];
                    g_curr_press[1] = nTemp;

                    nTemp = p_info->tPoint[0].n_x;
                    p_info->tPoint[0].n_x = p_info->tPoint[1].n_x;
                    p_info->tPoint[1].n_x = nTemp;

                    nTemp = p_info->tPoint[0].n_y;
                    p_info->tPoint[0].n_y = p_info->tPoint[1].n_y;
                    p_info->tPoint[1].n_y = nTemp;
                }
            }

            /*Save current status */
            for (i = 0; i < max_touch_num; i++) {
                szPrevPress[i] = g_curr_press[i];
                szPrevX[i] = p_info->tPoint[i].n_x;
                szPrevY[i] = p_info->tPoint[i].n_y;
            }
            nPrevTouchNum = p_info->n_finger_num;

            g_prev_touch_status = 1;
#endif /*CONFIG_ENABLE_TYPE_B_PROTOCOL */

            /*Notify android application to retrieve log data mode packet from device driver by sysfs. */
            if (g_touch_k_obj != NULL) {
                char *pEnvp[2];
                s32 n_ret_val = 0;

                memcpy(g_log_mode_packet, p_packet, n_length);  /*Copy g_debug_mode_packet to g_log_mode_packet for avoiding the debug mode data which is received by MTPTool APK may be modified. */

                pEnvp[0] = "STATUS=GET_PACKET";
                pEnvp[1] = NULL;

                n_ret_val = kobject_uevent_env(g_touch_k_obj, KOBJ_CHANGE, pEnvp);
                DBG(&g_i2c_client->dev, "kobject_uevent_env() n_ret_val = %d\n",
                    n_ret_val);
            }
        }
    } else if (g_chip_type == CHIP_TYPE_MSG22XX && is_self_freq_scan_enabled) {

/*Notify android application to retrieve log data mode packet from device driver by sysfs. */
            if (g_touch_k_obj != NULL) {
            char *pEnvp[2];
            s32 n_ret_val = 0;

            memcpy(g_log_mode_packet, p_packet, n_length);  /*Copy g_debug_mode_packet to g_log_mode_packet for avoiding the self freq data which is received by MTPTool APK may be modified. */
            pEnvp[0] = "STATUS=GET_DEBUG_MODE_PACKET";
            pEnvp[1] = NULL;

            n_ret_val = kobject_uevent_env(g_touch_k_obj, KOBJ_CHANGE, pEnvp);
            DBG(&g_i2c_client->dev, "kobject_uevent_env() n_ret_val = %d\n",
                n_ret_val);
        }
    } else {
        DBG(&g_i2c_client->dev,
            "p_packet[0]=0x%x, p_packet[7]=0x%x, n_check_sum=0x%x\n", p_packet[0],
            p_packet[7], n_check_sum);

        if (p_packet[nCheckSumIndex] != n_check_sum) {
            DBG(&g_i2c_client->dev, "WRONG CHECKSUM\n");
			return -EFAULT;
        }

        if (g_firmware_mode == firmware_mode_demo_mode && p_packet[0] != 0x52) {
            DBG(&g_i2c_client->dev, "WRONG DEMO MODE HEADER\n");
			return -EFAULT;
        } else if (g_firmware_mode == firmware_mode_debug_mode &&
                   p_packet[0] != 0x62) {
            DBG(&g_i2c_client->dev, "WRONG DEBUG MODE HEADER\n");
			return -EFAULT;
        } else if (g_firmware_mode == firmware_mode_raw_data_mode &&
                   p_packet[0] != 0x62) {
            DBG(&g_i2c_client->dev, "WRONG RAW DATA MODE HEADER\n");
			return -EFAULT;
        }
    }

    return 0;
}

static s32 drv_mutual_parse_packet(u8 *p_packet, u16 n_length,
                                 struct mutual_touch_info_t *p_info)
{
    /*for MSG28xx/MSG58xxA */
    u32 i;
    u8 n_check_sum = 0;
    u32 n_x = 0, n_y = 0;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

#ifdef CONFIG_ENABLE_COUNT_REPORT_RATE
    if (g_is_enable_report_rate == 1) {
        if (g_interrupt_count == 4294967295UL) {
            g_interrupt_count = 0;   /*Reset count if overflow */
            DBG(&g_i2c_client->dev, "g_interrupt_count reset to 0\n");
        }

        if (g_interrupt_count == 0) {
            /*Get start time */
            do_gettimeofday(&g_start_time);

            DBG(&g_i2c_client->dev, "Start time : %lu sec, %lu msec\n",
                g_start_time.tv_sec, g_start_time.tv_usec);
        }

        g_interrupt_count++;

        DBG(&g_i2c_client->dev, "g_interrupt_count = %d\n", g_interrupt_count);
    }
#endif /*CONFIG_ENABLE_COUNT_REPORT_RATE */

    DBG(&g_i2c_client->dev,
        "received raw data from touch panel as following:\n");
    DBG(&g_i2c_client->dev,
        "p_packet[0]=%x\n p_packet[1]=%x p_packet[2]=%x p_packet[3]=%x p_packet[4]=%x\n p_packet[5]=%x p_packet[6]=%x p_packet[7]=%x p_packet[8]=%x\n",
        p_packet[0], p_packet[1], p_packet[2], p_packet[3], p_packet[4], p_packet[5],
        p_packet[6], p_packet[7], p_packet[8]);

    if (is_apk_print_firmware_specific_log_enabled) {
        if (p_packet[0] == 0x2C) {
            /*Notify android application to retrieve firmware specific debug value packet from device driver by sysfs. */
            if (g_touch_k_obj != NULL) {
                char szRspFwSpecificLogPacket[512] = { 0 };
                char szValue[3] = { 0 };
                char *pEnvp[3];
                s32 n_ret_val = 0;

				strlcat(szRspFwSpecificLogPacket, "VALUE=", sizeof(szRspFwSpecificLogPacket));

                for (i = 0; i < n_length; i++) {
					snprintf(szValue, sizeof(szValue), "%02x", p_packet[i]);
					strlcat(szRspFwSpecificLogPacket, szValue, sizeof(szRspFwSpecificLogPacket));
                }

                pEnvp[0] = "STATUS=GET_FW_LOG";
                pEnvp[1] = szRspFwSpecificLogPacket;
                pEnvp[2] = NULL;
                DBG(&g_i2c_client->dev, "pEnvp[1] = %s\n", pEnvp[1]);   /*TODO : add for debug */
                DBG(&g_i2c_client->dev, "g_demo_mode_packet[] = %s\n", p_packet);  /*TODO : add for debug */

                n_ret_val = kobject_uevent_env(g_touch_k_obj, KOBJ_CHANGE, pEnvp);
                DBG(&g_i2c_client->dev,
                    "kobject_uevent_env() STATUS=GET_FW_LOG, n_ret_val = %d\n",
                    n_ret_val);
            }

			return -EFAULT;
        }
    }

    n_check_sum = drv_calculate_check_sum(&p_packet[0], (n_length - 1));
    DBG(&g_i2c_client->dev, "checksum : [%x] == [%x]?\n", p_packet[n_length - 1],
        n_check_sum);

    if (p_packet[n_length - 1] != n_check_sum) {
        DBG(&g_i2c_client->dev, "WRONG CHECKSUM\n");
		return -EFAULT;
    }
#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
    if (g_gesture_wakeup_flag == 1) {
        u8 nWakeupMode = 0;
        u8 bIsCorrectFormat = 0;

        DBG(&g_i2c_client->dev,
            "received raw data from touch panel as following:\n");
        DBG(&g_i2c_client->dev,
            "p_packet[0]=%x\n p_packet[1]=%x p_packet[2]=%x p_packet[3]=%x p_packet[4]=%x p_packet[5]=%x\n",
            p_packet[0], p_packet[1], p_packet[2], p_packet[3], p_packet[4],
            p_packet[5]);

        if (p_packet[0] == 0xA7 && p_packet[1] == 0x00 && p_packet[2] == 0x06 &&
            p_packet[3] == PACKET_TYPE_GESTURE_WAKEUP) {
            nWakeupMode = p_packet[4];
            bIsCorrectFormat = 1;
        }
#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
        else if (p_packet[0] == 0xA7 && p_packet[1] == 0x00 && p_packet[2] == 0x80
                 && p_packet[3] == PACKET_TYPE_GESTURE_DEBUG) {
            u32 a = 0;

            nWakeupMode = p_packet[4];
            bIsCorrectFormat = 1;

            for (a = 0; a < 0x80; a++) {
                g_log_gesture_debug[a] = p_packet[a];
            }

            if (!(p_packet[5] >> 7)) {   /*LCM Light Flag = 0 */
                nWakeupMode = 0xFE;
                DBG(&g_i2c_client->dev, "gesture debug mode LCM flag = 0\n");
            }
        }
#endif /*CONFIG_ENABLE_GESTURE_DEBUG_MODE */

#ifdef CONFIG_ENABLE_GESTURE_INFORMATION_MODE
        else if (p_packet[0] == 0xA7 && p_packet[1] == 0x00 && p_packet[2] == 0x80
                 && p_packet[3] == PACKET_TYPE_GESTURE_INFORMATION) {
            u32 a = 0;
            u32 nTmpCount = 0;
            u32 nWidth = 0;
            u32 nHeight = 0;

            nWakeupMode = p_packet[4];
            bIsCorrectFormat = 1;

            for (a = 0; a < 6; a++) {   /*header */
                g_log_gesture_infor[nTmpCount] = p_packet[a];
                nTmpCount++;
            }

            for (a = 6; a < 126; a = a + 3) {   /*parse packet to coordinate */
                u32 nTran_x = 0;
                u32 nTran_y = 0;

                drv_convert_gesture_information_mode_coordinate(&p_packet[a],
                                                            &nTran_x, &nTran_y);
                g_log_gesture_infor[nTmpCount] = nTran_x;
                nTmpCount++;
                g_log_gesture_infor[nTmpCount] = nTran_y;
                nTmpCount++;
            }

            nWidth = (((p_packet[12] & 0xF0) << 4) | p_packet[13]);   /*parse width & height */
            nHeight = (((p_packet[12] & 0x0F) << 8) | p_packet[14]);

            DBG(&g_i2c_client->dev, "Before convert [width,height]=[%d,%d]\n",
                nWidth, nHeight);

            if ((p_packet[12] == 0xFF) && (p_packet[13] == 0xFF)
                && (p_packet[14] == 0xFF)) {
                nWidth = 0;
                nHeight = 0;
            } else {
                nWidth = (nWidth * TOUCH_SCREEN_X_MAX) / TPD_WIDTH;
                nHeight = (nHeight * TOUCH_SCREEN_Y_MAX) / TPD_HEIGHT;
                DBG(&g_i2c_client->dev,
                    "After convert [width,height]=[%d,%d]\n", nWidth, nHeight);
            }

            g_log_gesture_infor[10] = nWidth;
            g_log_gesture_infor[11] = nHeight;

            g_log_gesture_infor[nTmpCount] = p_packet[126];  /*Dummy */
            nTmpCount++;
            g_log_gesture_infor[nTmpCount] = p_packet[127];  /*checksum */
            nTmpCount++;
            DBG(&g_i2c_client->dev, "gesture information mode Count = %d\n",
                nTmpCount);
        }
#endif /*CONFIG_ENABLE_GESTURE_INFORMATION_MODE */

        if (bIsCorrectFormat) {
            DBG(&g_i2c_client->dev, "nWakeupMode = 0x%x\n", nWakeupMode);

            switch (nWakeupMode) {
            case 0x58:
                g_gesture_wakeup_value[0] =
                    GESTURE_WAKEUP_MODE_DOUBLE_CLICK_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by DOUBLE_CLICK gesture wakeup.\n");

                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x60:
                g_gesture_wakeup_value[0] = GESTURE_WAKEUP_MODE_UP_DIRECT_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by UP_DIRECT gesture wakeup.\n");

/*input_report_key(g_input_device, KEY_UP, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, KEY_UP, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x61:
                g_gesture_wakeup_value[0] =
                    GESTURE_WAKEUP_MODE_DOWN_DIRECT_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by DOWN_DIRECT gesture wakeup.\n");

/*input_report_key(g_input_device, KEY_DOWN, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, KEY_DOWN, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x62:
                g_gesture_wakeup_value[0] =
                    GESTURE_WAKEUP_MODE_LEFT_DIRECT_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by LEFT_DIRECT gesture wakeup.\n");

/*input_report_key(g_input_device, KEY_LEFT, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, KEY_LEFT, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x63:
                g_gesture_wakeup_value[0] =
                    GESTURE_WAKEUP_MODE_RIGHT_DIRECT_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by RIGHT_DIRECT gesture wakeup.\n");

/*input_report_key(g_input_device, KEY_RIGHT, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, KEY_RIGHT, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x64:
                g_gesture_wakeup_value[0] =
                    GESTURE_WAKEUP_MODE_m_CHARACTER_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by m_CHARACTER gesture wakeup.\n");

/*input_report_key(g_input_device, KEY_M, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, KEY_M, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x65:
                g_gesture_wakeup_value[0] =
                    GESTURE_WAKEUP_MODE_W_CHARACTER_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by W_CHARACTER gesture wakeup.\n");

/*input_report_key(g_input_device, KEY_W, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, KEY_W, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x66:
                g_gesture_wakeup_value[0] =
                    GESTURE_WAKEUP_MODE_C_CHARACTER_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by C_CHARACTER gesture wakeup.\n");

/*input_report_key(g_input_device, KEY_C, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, KEY_C, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x67:
                g_gesture_wakeup_value[0] =
                    GESTURE_WAKEUP_MODE_e_CHARACTER_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by e_CHARACTER gesture wakeup.\n");

/*input_report_key(g_input_device, KEY_E, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, KEY_E, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x68:
                g_gesture_wakeup_value[0] =
                    GESTURE_WAKEUP_MODE_V_CHARACTER_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by V_CHARACTER gesture wakeup.\n");

/*input_report_key(g_input_device, KEY_V, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, KEY_V, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x69:
                g_gesture_wakeup_value[0] =
                    GESTURE_WAKEUP_MODE_O_CHARACTER_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by O_CHARACTER gesture wakeup.\n");

/*input_report_key(g_input_device, KEY_O, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, KEY_O, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x6A:
                g_gesture_wakeup_value[0] =
                    GESTURE_WAKEUP_MODE_S_CHARACTER_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by S_CHARACTER gesture wakeup.\n");

/*input_report_key(g_input_device, KEY_S, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, KEY_S, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x6B:
                g_gesture_wakeup_value[0] =
                    GESTURE_WAKEUP_MODE_Z_CHARACTER_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by Z_CHARACTER gesture wakeup.\n");

/*input_report_key(g_input_device, KEY_Z, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, KEY_Z, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x6C:
                g_gesture_wakeup_value[0] = GESTURE_WAKEUP_MODE_RESERVE1_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE1_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER1, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER1, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x6D:
                g_gesture_wakeup_value[0] = GESTURE_WAKEUP_MODE_RESERVE2_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE2_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER2, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER2, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x6E:
                g_gesture_wakeup_value[0] = GESTURE_WAKEUP_MODE_RESERVE3_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE3_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER3, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER3, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
#ifdef CONFIG_SUPPORT_64_TYPES_GESTURE_WAKEUP_MODE
            case 0x6F:
                g_gesture_wakeup_value[0] = GESTURE_WAKEUP_MODE_RESERVE4_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE4_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER4, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER4, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x70:
                g_gesture_wakeup_value[0] = GESTURE_WAKEUP_MODE_RESERVE5_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE5_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER5, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER5, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x71:
                g_gesture_wakeup_value[0] = GESTURE_WAKEUP_MODE_RESERVE6_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE6_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER6, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER6, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x72:
                g_gesture_wakeup_value[0] = GESTURE_WAKEUP_MODE_RESERVE7_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE7_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER7, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER7, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x73:
                g_gesture_wakeup_value[0] = GESTURE_WAKEUP_MODE_RESERVE8_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE8_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER8, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER8, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x74:
                g_gesture_wakeup_value[0] = GESTURE_WAKEUP_MODE_RESERVE9_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE9_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER9, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER9, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x75:
                g_gesture_wakeup_value[0] = GESTURE_WAKEUP_MODE_RESERVE10_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE10_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER10, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER10, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x76:
                g_gesture_wakeup_value[0] = GESTURE_WAKEUP_MODE_RESERVE11_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE11_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER11, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER11, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x77:
                g_gesture_wakeup_value[0] = GESTURE_WAKEUP_MODE_RESERVE12_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE12_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER12, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER12, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x78:
                g_gesture_wakeup_value[0] = GESTURE_WAKEUP_MODE_RESERVE13_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE13_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER13, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER13, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x79:
                g_gesture_wakeup_value[0] = GESTURE_WAKEUP_MODE_RESERVE14_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE14_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER14, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER14, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x7A:
                g_gesture_wakeup_value[0] = GESTURE_WAKEUP_MODE_RESERVE15_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE15_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER15, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER15, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x7B:
                g_gesture_wakeup_value[0] = GESTURE_WAKEUP_MODE_RESERVE16_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE16_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER16, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER16, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x7C:
                g_gesture_wakeup_value[0] = GESTURE_WAKEUP_MODE_RESERVE17_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE17_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER17, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER17, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x7D:
                g_gesture_wakeup_value[0] = GESTURE_WAKEUP_MODE_RESERVE18_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE18_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER18, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER18, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x7E:
                g_gesture_wakeup_value[0] = GESTURE_WAKEUP_MODE_RESERVE19_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE19_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER19, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER19, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x7F:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE20_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE20_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER20, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER20, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x80:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE21_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE21_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER21, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER21, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x81:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE22_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE22_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER22, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER22, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x82:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE23_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE23_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER23, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER23, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x83:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE24_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE24_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER24, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER24, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x84:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE25_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE25_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER25, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER25, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x85:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE26_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE26_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER26, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER26, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x86:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE27_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE27_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER27, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER27, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x87:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE28_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE28_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER28, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER28, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x88:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE29_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE29_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER29, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER29, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x89:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE30_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE30_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER30, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER30, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x8A:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE31_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE31_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER31, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER31, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x8B:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE32_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE32_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER32, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER32, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x8C:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE33_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE33_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER33, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER33, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x8D:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE34_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE34_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER34, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER34, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x8E:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE35_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE35_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER35, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER35, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x8F:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE36_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE36_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER36, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER36, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x90:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE37_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE37_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER37, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER37, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x91:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE38_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE38_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER38, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER38, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x92:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE39_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE39_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER39, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER39, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x93:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE40_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE40_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER40, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER40, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x94:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE41_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE41_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER41, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER41, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x95:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE42_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE42_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER42, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER42, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x96:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE43_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE43_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER43, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER43, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x97:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE44_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE44_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER44, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER44, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x98:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE45_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE45_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER45, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER45, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x99:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE46_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE46_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER46, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER46, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x9A:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE47_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE47_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER47, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER47, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x9B:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE48_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE48_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER48, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER48, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x9C:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE49_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE49_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER49, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER49, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x9D:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE50_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE50_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER50, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER50, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
            case 0x9E:
                g_gesture_wakeup_value[1] = GESTURE_WAKEUP_MODE_RESERVE51_FLAG;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_RESERVE51_FLAG gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER51, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER51, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
#endif /*CONFIG_SUPPORT_64_TYPES_GESTURE_WAKEUP_MODE */

#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
            case 0xFF:         /*Gesture Fail */
                g_gesture_wakeup_value[1] = 0xFF;

                DBG(&g_i2c_client->dev,
                    "Light up screen by GESTURE_WAKEUP_MODE_FAIL gesture wakeup.\n");

/*input_report_key(g_input_device, RESERVER51, 1);*/
                input_report_key(g_input_device, KEY_POWER, 1);
                input_sync(g_input_device);

/*input_report_key(g_input_device, RESERVER51, 0);*/
                input_report_key(g_input_device, KEY_POWER, 0);
                input_sync(g_input_device);
                break;
#endif /*CONFIG_ENABLE_GESTURE_DEBUG_MODE */

            default:
                g_gesture_wakeup_value[0] = 0;
                g_gesture_wakeup_value[1] = 0;
                DBG(&g_i2c_client->dev,
                    "Un-supported gesture wakeup mode. Please check your device driver code.\n");
                break;
            }

            DBG(&g_i2c_client->dev, "g_gesture_wakeup_value = 0x%x, 0x%x\n",
                g_gesture_wakeup_value[0], g_gesture_wakeup_value[1]);
        } else {
            DBG(&g_i2c_client->dev,
                "gesture wakeup packet format is incorrect.\n");
        }

#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
        /*Notify android application to retrieve log data mode packet from device driver by sysfs. */
        if (g_gesture_k_obj != NULL && p_packet[3] == PACKET_TYPE_GESTURE_DEBUG) {
            char *pEnvp[2];
            s32 n_ret_val = 0;

            pEnvp[0] = "STATUS=GET_GESTURE_DEBUG";
            pEnvp[1] = NULL;

            n_ret_val = kobject_uevent_env(g_gesture_k_obj, KOBJ_CHANGE, pEnvp);
            DBG(&g_i2c_client->dev,
                "kobject_uevent_env() STATUS=GET_GESTURE_DEBUG, n_ret_val = %d\n",
                n_ret_val);
        }
#endif /*CONFIG_ENABLE_GESTURE_DEBUG_MODE */

		return -EFAULT;
    }
#endif /*CONFIG_ENABLE_GESTURE_WAKEUP */

    if (is_firmware_data_log_enabled) {
        if (g_firmware_mode == firmware_mode_demo_mode && p_packet[0] != 0x5A) {
            {
                DBG(&g_i2c_client->dev, "WRONG DEMO MODE HEADER\n");
				return -EFAULT;
            }
        } else if (g_firmware_mode == firmware_mode_debug_mode
                   && (p_packet[0] != 0xA7 &&
                       p_packet[3] != PACKET_TYPE_TOOTH_PATTERN)) {
            DBG(&g_i2c_client->dev, "WRONG DEBUG MODE HEADER\n");
			return -EFAULT;
        }
    } else {
        if (p_packet[0] != 0x5A) {
            DBG(&g_i2c_client->dev, "WRONG DEMO MODE HEADER\n");
			return -EFAULT;
        }
    }                           /*is_firmware_data_log_enabled */

    /*Process raw data... */
    if (p_packet[0] == 0x5A) {
        for (i = 0; i < max_touch_num; i++) {
            if ((p_packet[(4 * i) + 1] == 0xFF) && (p_packet[(4 * i) + 2] == 0xFF)
                && (p_packet[(4 * i) + 3] == 0xFF)) {
#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
                g_current_touch[i] = 0;
#endif /*CONFIG_ENABLE_TYPE_B_PROTOCOL */

                continue;
            }

            n_x = (((p_packet[(4 * i) + 1] & 0xF0) << 4) |
                  (p_packet[(4 * i) + 2]));
            n_y = (((p_packet[(4 * i) + 1] & 0x0F) << 8) |
                  (p_packet[(4 * i) + 3]));

            p_info->tPoint[p_info->n_count].n_x =
                n_x * TOUCH_SCREEN_X_MAX / TPD_WIDTH;
            p_info->tPoint[p_info->n_count].n_y =
                n_y * TOUCH_SCREEN_Y_MAX / TPD_HEIGHT;
            p_info->tPoint[p_info->n_count].nP = p_packet[4 * (i + 1)];
            p_info->tPoint[p_info->n_count].n_id = i;

            DBG(&g_i2c_client->dev, "[x,y]=[%d,%d]\n", n_x, n_y);
            DBG(&g_i2c_client->dev, "point[%d] : (%d,%d) = %d\n",
                p_info->tPoint[p_info->n_count].n_id,
                p_info->tPoint[p_info->n_count].n_x,
                p_info->tPoint[p_info->n_count].n_y,
                p_info->tPoint[p_info->n_count].nP);

            p_info->n_count++;

#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
            g_current_touch[i] = 1;
#endif /*CONFIG_ENABLE_TYPE_B_PROTOCOL */
        }

        if (is_firmware_data_log_enabled) {
            /*Notify android application to retrieve demo mode packet from device driver by sysfs. */
            if (g_touch_k_obj != NULL) {
                char szRspDemoModePacket[512] = { 0 };
                char szValue[3] = { 0 };
                char *pEnvp[3];
                s32 n_ret_val = 0;

				strlcat(szRspDemoModePacket, "VALUE=", sizeof(szRspDemoModePacket));

                for (i = 0; i < n_length; i++) {
					snprintf(szValue, sizeof(szValue), "%02x", p_packet[i]);
					strlcat(szRspDemoModePacket, szValue, sizeof(szRspDemoModePacket));
                }

                pEnvp[0] = "STATUS=GET_DEMO_MODE_PACKET";
                pEnvp[1] = szRspDemoModePacket;
                pEnvp[2] = NULL;
                DBG(&g_i2c_client->dev, "pEnvp[1] = %s\n", pEnvp[1]);   /*TODO : add for debug */

                n_ret_val = kobject_uevent_env(g_touch_k_obj, KOBJ_CHANGE, pEnvp);
                DBG(&g_i2c_client->dev,
                    "kobject_uevent_env() STATUS=GET_DEMO_MODE_PACKET, n_ret_val = %d\n",
                    n_ret_val);
            }
        }
    } else if (p_packet[0] == 0xA7 && p_packet[3] == PACKET_TYPE_TOOTH_PATTERN) {
        for (i = 0; i < max_touch_num; i++) {
            if ((p_packet[(3 * i) + 5] == 0xFF) && (p_packet[(3 * i) + 6] == 0xFF)
                && (p_packet[(3 * i) + 7] == 0xFF)) {
#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
                g_current_touch[i] = 0;
#endif /*CONFIG_ENABLE_TYPE_B_PROTOCOL */

                continue;
            }

            n_x = (((p_packet[(3 * i) + 5] & 0xF0) << 4) |
                  (p_packet[(3 * i) + 6]));
            n_y = (((p_packet[(3 * i) + 5] & 0x0F) << 8) |
                  (p_packet[(3 * i) + 7]));

            p_info->tPoint[p_info->n_count].n_x =
                n_x * TOUCH_SCREEN_X_MAX / TPD_WIDTH;
            p_info->tPoint[p_info->n_count].n_y =
                n_y * TOUCH_SCREEN_Y_MAX / TPD_HEIGHT;
            p_info->tPoint[p_info->n_count].nP = 1;
            p_info->tPoint[p_info->n_count].n_id = i;

            DBG(&g_i2c_client->dev, "[x,y]=[%d,%d]\n", n_x, n_y);
            DBG(&g_i2c_client->dev, "point[%d] : (%d,%d) = %d\n",
                p_info->tPoint[p_info->n_count].n_id,
                p_info->tPoint[p_info->n_count].n_x,
                p_info->tPoint[p_info->n_count].n_y,
                p_info->tPoint[p_info->n_count].nP);

            p_info->n_count++;

#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
            g_current_touch[i] = 1;
#endif /*CONFIG_ENABLE_TYPE_B_PROTOCOL */
        }

        /*Notify android application to retrieve debug mode packet from device driver by sysfs. */
        if (g_touch_k_obj != NULL) {
            char *pEnvp[2];
            s32 n_ret_val = 0;

            memcpy(g_log_mode_packet, p_packet, n_length);  /*Copy g_debug_mode_packet to g_log_mode_packet for avoiding the debug mode data which is received by MTPTool APK may be modified. */

            pEnvp[0] = "STATUS=GET_DEBUG_MODE_PACKET";
            pEnvp[1] = NULL;

            n_ret_val = kobject_uevent_env(g_touch_k_obj, KOBJ_CHANGE, pEnvp);
            DBG(&g_i2c_client->dev,
                "kobject_uevent_env() STATUS=GET_DEBUG_MODE_PACKET, n_ret_val = %d\n",
                n_ret_val);
        }
    }
#ifdef CONFIG_TP_HAVE_KEY
    if (p_packet[0] == 0x5A) {
        u8 nButton = p_packet[n_length - 2];  /*Since the key value is stored in 0th~3th bit of variable "button", we can only retrieve 0th~3th bit of it. */

/*if (nButton)*/
        if (nButton != 0xFF) {
            DBG(&g_i2c_client->dev, "button = %x\n", nButton);

#ifdef CONFIG_ENABLE_PROXIMITY_DETECTION
#if defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM)
            DBG(&g_i2c_client->dev,
                "g_enable_tp_proximity = %d, p_packet[n_length-2] = 0x%x\n",
                g_enable_tp_proximity, p_packet[n_length - 2]);

            if (g_enable_tp_proximity && ((p_packet[n_length - 2] == 0x80)
                                          || (p_packet[n_length - 2] == 0x40))) {
                if (p_packet[n_length - 2] == 0x80) { /*close to */
                    g_face_closing_tp = 1;

                    input_report_abs(g_proximityinput_device, ABS_DISTANCE, 0);
                    input_sync(g_proximityinput_device);
                } else if (p_packet[n_length - 2] == 0x40) {  /*far away */
                    g_face_closing_tp = 0;

                    input_report_abs(g_proximityinput_device, ABS_DISTANCE, 1);
                    input_sync(g_proximityinput_device);
                }

                DBG(&g_i2c_client->dev, "g_face_closing_tp = %d\n",
                    g_face_closing_tp);

				return -EFAULT;
            }
#elif defined(CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM)
            if (g_enable_tp_proximity && ((p_packet[n_length - 2] == 0x80)
                                          || (p_packet[n_length - 2] == 0x40))) {
                int nErr;
                hwm_sensor_data tSensor_data;

                if (p_packet[n_length - 2] == 0x80) { /*close to */
                    g_face_closing_tp = 0;
                } else if (p_packet[n_length - 2] == 0x40) {  /*far away */
                    g_face_closing_tp = 1;
                }

                DBG(&g_i2c_client->dev, "g_face_closing_tp = %d\n",
                    g_face_closing_tp);

                /*map and store data to hwm_sensor_data */
                tSensor_data.values[0] = drv_get_tp_ps_data();
                tSensor_data.value_divide = 1;
                tSensor_data.status = SENSOR_STATUS_ACCURACY_MEDIUM;
                /*let up layer to know */
                if ((nErr == hwmsen_get_interrupt_data(ID_PROXIMITY, &tSensor_data))) {
                    DBG(&g_i2c_client->dev,
                        "call hwmsen_get_interrupt_data() failed = %d\n", nErr);
                }

				return -EFAULT;
            }
#endif
#endif /*CONFIG_ENABLE_PROXIMITY_DETECTION */

            for (i = 0; i < MAX_KEY_NUM; i++) {
                if ((nButton & (1 << i)) == (1 << i)) {
                    if (p_info->n_key_code == 0) {
                        p_info->n_key_code = i;

#if defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM)
                        DBG(&g_i2c_client->dev, "key[%d]=%d\n", i,
                            g_tp_virtual_key[i]);

#elif defined(CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM)
#ifdef CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD
                        if (tpd_dts_data.use_tpd_button) {
                            DBG(&g_i2c_client->dev, "key[%d]=%d\n", i,
                                tpd_dts_data.tpd_key_local[i]);

#ifdef CONFIG_ENABLE_REPORT_KEY_WITH_COORDINATE
                            p_info->n_key_code = 0xFF;
                            p_info->tPoint[p_info->n_count].n_x =
                                tpd_dts_data.tpd_key_dim_local[i].key_x;
                            p_info->tPoint[p_info->n_count].n_y =
                                tpd_dts_data.tpd_key_dim_local[i].key_y;
                            p_info->tPoint[p_info->n_count].nP = 1;
                            p_info->tPoint[p_info->n_count].n_id = p_info->n_count;
#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
                            g_current_touch[p_info->n_count] = 1;
#endif /*CONFIG_ENABLE_TYPE_B_PROTOCOL */
                            DBG(&g_i2c_client->dev,
                                "virtual key point[%d] : (%d,%d) = %d\n",
                                p_info->tPoint[p_info->n_count].n_id,
                                p_info->tPoint[p_info->n_count].n_x,
                                p_info->tPoint[p_info->n_count].n_y,
                                p_info->tPoint[p_info->n_count].nP);
                            p_info->n_count++;
#endif /*CONFIG_ENABLE_REPORT_KEY_WITH_COORDINATE */
                        }
#else
                        DBG(&g_i2c_client->dev, "key[%d]=%d\n", i,
                            g_tp_virtual_key[i]);

#ifdef CONFIG_ENABLE_REPORT_KEY_WITH_COORDINATE
                        p_info->n_key_code = 0xFF;
                        p_info->tPoint[p_info->n_count].n_x =
                            g_tp_virtual_key_dim_local[i][0];
                        p_info->tPoint[p_info->n_count].n_y =
                            g_tp_virtual_key_dim_local[i][1];
                        p_info->tPoint[p_info->n_count].nP = 1;
                        p_info->tPoint[p_info->n_count].n_id = p_info->n_count;
#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
                        g_current_touch[p_info->n_count] = 1;
#endif /*CONFIG_ENABLE_TYPE_B_PROTOCOL */
                        DBG(&g_i2c_client->dev,
                            "virtual key point[%d] : (%d,%d) = %d\n",
                            p_info->tPoint[p_info->n_count].n_id,
                            p_info->tPoint[p_info->n_count].n_x,
                            p_info->tPoint[p_info->n_count].n_y,
                            p_info->tPoint[p_info->n_count].nP);
                        p_info->n_count++;
#endif /*CONFIG_ENABLE_REPORT_KEY_WITH_COORDINATE */
#endif /*CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD */
#endif
                    } else {
                        /*if pressing multi-key => no report */
                        p_info->n_key_code = 0xFF;
                    }
                }
            }
        } else {
            p_info->n_key_code = 0xFF;
        }
    } else if ((p_packet[0] == 0xA7 && p_packet[3] == PACKET_TYPE_TOOTH_PATTERN)) {
        if ((p_packet[0] == 0xA7 && p_packet[3] == PACKET_TYPE_TOOTH_PATTERN)) {
            u8 nButton = 0xFF;

            if (p_packet[0] == 0xA7 && p_packet[3] == PACKET_TYPE_TOOTH_PATTERN) {
                nButton = p_packet[4];   /*The pressed virtual key is stored in 5th byte for debug mode packet 0xA7. */
            }

            if (nButton != 0xFF) {
                DBG(&g_i2c_client->dev, "button = %x\n", nButton);

                for (i = 0; i < MAX_KEY_NUM; i++) {
                    if ((nButton & (1 << i)) == (1 << i)) {
                        if (p_info->n_key_code == 0) {
                            p_info->n_key_code = i;

#if defined(CONFIG_TOUCH_DRIVER_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_ON_QCOM_PLATFORM)
                            DBG(&g_i2c_client->dev, "key[%d]=%d\n", i,
                                g_tp_virtual_key[i]);

#elif defined(CONFIG_TOUCH_DRIVER_ON_MTK_PLATFORM)
#ifdef CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD
                            if (tpd_dts_data.use_tpd_button) {
                                DBG(&g_i2c_client->dev, "key[%d]=%d\n", i,
                                    tpd_dts_data.tpd_key_local[i]);

#ifdef CONFIG_ENABLE_REPORT_KEY_WITH_COORDINATE
                                p_info->n_key_code = 0xFF;
                                p_info->tPoint[p_info->n_count].n_x =
                                    tpd_dts_data.tpd_key_dim_local[i].key_x;
                                p_info->tPoint[p_info->n_count].n_y =
                                    tpd_dts_data.tpd_key_dim_local[i].key_y;
                                p_info->tPoint[p_info->n_count].nP = 1;
                                p_info->tPoint[p_info->n_count].n_id =
                                    p_info->n_count;
#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
                                g_current_touch[p_info->n_count] = 1;
#endif /*CONFIG_ENABLE_TYPE_B_PROTOCOL */
                                DBG(&g_i2c_client->dev,
                                    "virtual key point[%d] : (%d,%d) = %d\n",
                                    p_info->tPoint[p_info->n_count].n_id,
                                    p_info->tPoint[p_info->n_count].n_x,
                                    p_info->tPoint[p_info->n_count].n_y,
                                    p_info->tPoint[p_info->n_count].nP);
                                p_info->n_count++;
#endif /*CONFIG_ENABLE_REPORT_KEY_WITH_COORDINATE */
                            }
#else
                            DBG(&g_i2c_client->dev, "key[%d]=%d\n", i,
                                g_tp_virtual_key[i]);

#ifdef CONFIG_ENABLE_REPORT_KEY_WITH_COORDINATE
                            p_info->n_key_code = 0xFF;
                            p_info->tPoint[p_info->n_count].n_x =
                                g_tp_virtual_key_dim_local[i][0];
                            p_info->tPoint[p_info->n_count].n_y =
                                g_tp_virtual_key_dim_local[i][1];
                            p_info->tPoint[p_info->n_count].nP = 1;
                            p_info->tPoint[p_info->n_count].n_id = p_info->n_count;
#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
                            g_current_touch[p_info->n_count] = 1;
#endif /*CONFIG_ENABLE_TYPE_B_PROTOCOL */
                            DBG(&g_i2c_client->dev,
                                "virtual key point[%d] : (%d,%d) = %d\n",
                                p_info->tPoint[p_info->n_count].n_id,
                                p_info->tPoint[p_info->n_count].n_x,
                                p_info->tPoint[p_info->n_count].n_y,
                                p_info->tPoint[p_info->n_count].nP);
                            p_info->n_count++;
#endif /*CONFIG_ENABLE_REPORT_KEY_WITH_COORDINATE */
#endif /*CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD */
#endif
                        } else {
                            /*if pressing multi-key => no report */
                            p_info->n_key_code = 0xFF;
                        }
                    }
                }
            } else {
                p_info->n_key_code = 0xFF;
            }
        }
    }
#endif /*CONFIG_TP_HAVE_KEY */

    return 0;
}

/*---------------------------------------------------------------------------*/
#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
void drv_open_gesture_wakeup(u32 *pMode)
{
    u8 sz_tx_data[4] = { 0 };
    u32 i = 0;
    s32 rc;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    if (g_chip_type == CHIP_TYPE_MSG22XX || g_chip_type == CHIP_TYPE_MSG28XX
        || g_chip_type == CHIP_TYPE_MSG58XXA || g_chip_type == CHIP_TYPE_ILI2118A
        || g_chip_type == CHIP_TYPE_ILI2117A) {
        DBG(&g_i2c_client->dev, "wakeup mode 0 = 0x%x\n", pMode[0]);
        DBG(&g_i2c_client->dev, "wakeup mode 1 = 0x%x\n", pMode[1]);

#ifdef CONFIG_SUPPORT_64_TYPES_GESTURE_WAKEUP_MODE
        sz_tx_data[0] = 0x59;
        sz_tx_data[1] = 0x00;
        sz_tx_data[2] = ((pMode[1] & 0xFF000000) >> 24);
        sz_tx_data[3] = ((pMode[1] & 0x00FF0000) >> 16);

        while (i < 5) {
            mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);   /*delay 20ms */
            rc = iic_write_data(slave_i2c_id_dw_i2c, &sz_tx_data[0], 4);
            if (rc > 0) {
                DBG(&g_i2c_client->dev,
                    "Enable gesture wakeup index 0 success\n");
                break;
            }

            i++;
        }
        if (i == 5) {
            DBG(&g_i2c_client->dev, "Enable gesture wakeup index 0 failed\n");
        }

        sz_tx_data[0] = 0x59;
        sz_tx_data[1] = 0x01;
        sz_tx_data[2] = ((pMode[1] & 0x0000FF00) >> 8);
        sz_tx_data[3] = ((pMode[1] & 0x000000FF) >> 0);

        while (i < 5) {
            mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);   /*delay 20ms */
            rc = iic_write_data(slave_i2c_id_dw_i2c, &sz_tx_data[0], 4);
            if (rc > 0) {
                DBG(&g_i2c_client->dev,
                    "Enable gesture wakeup index 1 success\n");
                break;
            }

            i++;
        }
        if (i == 5)
            DBG(&g_i2c_client->dev, "Enable gesture wakeup index 1 failed\n");


        sz_tx_data[0] = 0x59;
        sz_tx_data[1] = 0x02;
        sz_tx_data[2] = ((pMode[0] & 0xFF000000) >> 24);
        sz_tx_data[3] = ((pMode[0] & 0x00FF0000) >> 16);

        while (i < 5) {
            mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);   /*delay 20ms */
            rc = iic_write_data(slave_i2c_id_dw_i2c, &sz_tx_data[0], 4);
            if (rc > 0) {
                DBG(&g_i2c_client->dev,
                    "Enable gesture wakeup index 2 success\n");
                break;
            }

            i++;
        }
        if (i == 5)
            DBG(&g_i2c_client->dev, "Enable gesture wakeup index 2 failed\n");


        sz_tx_data[0] = 0x59;
        sz_tx_data[1] = 0x03;
        sz_tx_data[2] = ((pMode[0] & 0x0000FF00) >> 8);
        sz_tx_data[3] = ((pMode[0] & 0x000000FF) >> 0);

        while (i < 5) {
            mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);   /*delay 20ms */
            rc = iic_write_data(slave_i2c_id_dw_i2c, &sz_tx_data[0], 4);
            if (rc > 0) {
                DBG(&g_i2c_client->dev,
                    "Enable gesture wakeup index 3 success\n");
                break;
            }

            i++;
        }
        if (i == 5)
            DBG(&g_i2c_client->dev, "Enable gesture wakeup index 3 failed\n");

        mutex_lock(&g_mutex_protect);
        g_gesture_wakeup_flag = 1;  /*gesture wakeup is enabled */
        mutex_unlock(&g_mutex_protect);
#else

        sz_tx_data[0] = 0x58;
        sz_tx_data[1] = ((pMode[0] & 0x0000FF00) >> 8);
        sz_tx_data[2] = ((pMode[0] & 0x000000FF) >> 0);

        while (i < 5) {
            mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);   /*delay 20ms */
            rc = iic_write_data(slave_i2c_id_dw_i2c, &sz_tx_data[0], 3);
            if (rc > 0) {
                DBG(&g_i2c_client->dev, "Enable gesture wakeup success\n");
                break;
            }

            i++;
        }
        if (i == 5)
            DBG(&g_i2c_client->dev, "Enable gesture wakeup failed\n");

        mutex_lock(&g_mutex_protect);
        g_gesture_wakeup_flag = 1;  /*gesture wakeup is enabled */
        mutex_unlock(&g_mutex_protect);
#endif /*CONFIG_SUPPORT_64_TYPES_GESTURE_WAKEUP_MODE */
    } else if (g_chip_type == CHIP_TYPE_ILI2120 ||
               g_chip_type == CHIP_TYPE_ILI2121) {
        sz_tx_data[0] = 0x01;
        sz_tx_data[1] = 0x00;

        rc = iic_write_data(slave_i2c_id_dw_i2c, &sz_tx_data[0], 2);
        if (rc > 0) {
            DBG(&g_i2c_client->dev,
                "Enable gesture wakeup success(Cmd : 0x%x 0x%x)\n", sz_tx_data[0],
                sz_tx_data[1]);
        } else {
            DBG(&g_i2c_client->dev,
                "Enable gesture wakeup failed(Cmd : 0x%x 0x%x), rc = %d\n",
                sz_tx_data[0], sz_tx_data[1], rc);
        }

        mdelay(5);

        if (g_gesture_state) {
            sz_tx_data[0] = 0x0A;
            sz_tx_data[1] = 0x01;
        } else {
            sz_tx_data[0] = 0x02;
            sz_tx_data[1] = 0x00;
        }

        rc = iic_write_data(slave_i2c_id_dw_i2c, &sz_tx_data[0], 2);
        if (rc > 0) {
            DBG(&g_i2c_client->dev,
                "Enable gesture wakeup success(Cmd : 0x%x 0x%x)\n", sz_tx_data[0],
                sz_tx_data[1]);
        } else {
            DBG(&g_i2c_client->dev,
                "Enable gesture wakeup failed(Cmd : 0x%x 0x%x), rc = %d\n",
                sz_tx_data[0], sz_tx_data[1], rc);
        }
        mutex_lock(&g_mutex_protect);
        g_gesture_wakeup_flag = 1;  /*gesture wakeup is enabled */
        mutex_unlock(&g_mutex_protect);
    }
}

void drv_close_gesture_wakeup(void)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);
    mutex_lock(&g_mutex_protect);
    g_gesture_wakeup_flag = 0;  /*gesture wakeup is disabled */
    mutex_unlock(&g_mutex_protect);
}

#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
void drv_open_gesture_debug_mode(u8 n_gesture_flag)
{
    u8 sz_tx_data[3] = { 0 };
    s32 rc;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    DBG(&g_i2c_client->dev, "Gesture Flag = 0x%x\n", n_gesture_flag);

    sz_tx_data[0] = 0x30;
    sz_tx_data[1] = 0x01;
    sz_tx_data[2] = n_gesture_flag;

    mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
    rc = iic_write_data(slave_i2c_id_dw_i2c, &sz_tx_data[0], 3);
    if (rc < 0) {
        DBG(&g_i2c_client->dev, "Enable gesture debug mode failed\n");
    } else {
        g_gesture_debug_mode = 1;   /*gesture debug mode is enabled */

        DBG(&g_i2c_client->dev, "Enable gesture debug mode success\n");
    }
}

void drv_close_gesture_debug_mode(void)
{
    u8 sz_tx_data[3] = { 0 };
    s32 rc;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    sz_tx_data[0] = 0x30;
    sz_tx_data[1] = 0x00;
    sz_tx_data[2] = 0x00;

    mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
    rc = iic_write_data(slave_i2c_id_dw_i2c, &sz_tx_data[0], 3);
    if (rc < 0) {
        DBG(&g_i2c_client->dev, "Disable gesture debug mode failed\n");
    } else {
        g_gesture_debug_mode = 0;   /*gesture debug mode is disabled */

        DBG(&g_i2c_client->dev, "Disable gesture debug mode success\n");
    }
}
#endif /*CONFIG_ENABLE_GESTURE_DEBUG_MODE */

#ifdef CONFIG_ENABLE_GESTURE_INFORMATION_MODE
static void drv_convert_gesture_information_mode_coordinate(u8 *p_raw_data,
                                                        u32 *p_tran_x,
                                                        u32 *p_tran_y)
{
    u32 n_x;
    u32 n_y;
#ifdef CONFIG_SWAP_X_Y
    u32 nTempX;
    u32 nTempY;
#endif

    n_x = (((p_raw_data[0] & 0xF0) << 4) | p_raw_data[1]);
    /*parse the packet to coordinate */
    n_y = (((p_raw_data[0] & 0x0F) << 8) | p_raw_data[2]);

    DBG(&g_i2c_client->dev, "[x,y]=[%d,%d]\n", n_x, n_y);

#ifdef CONFIG_SWAP_X_Y
    nTempY = n_x;
    nTempX = n_y;
    n_x = nTempX;
    n_y = nTempY;
#endif

#ifdef CONFIG_REVERSE_X
    n_x = 2047 - n_x;
#endif

#ifdef CONFIG_REVERSE_Y
    n_y = 2047 - n_y;
#endif

    /*
     *p_raw_data[0]~p_raw_data[2] : the point abs,
     *p_raw_data[0]~p_raw_data[2] all are 0xFF, release touch
     */
    if ((p_raw_data[0] == 0xFF) && (p_raw_data[1] == 0xFF) && (p_raw_data[2] == 0xFF)) {
        *p_tran_x = 0;            /*final X coordinate */
        *p_tran_y = 0;            /*final Y coordinate */
    } else {
        /* one touch point */
        *p_tran_x = (n_x * TOUCH_SCREEN_X_MAX) / TPD_WIDTH;
        *p_tran_y = (n_y * TOUCH_SCREEN_Y_MAX) / TPD_HEIGHT;
        DBG(&g_i2c_client->dev, "[%s]: [x,y]=[%d,%d]\n", __func__, n_x, n_y);
        DBG(&g_i2c_client->dev, "[%s]: point[x,y]=[%d,%d]\n", __func__, *p_tran_x,
            *p_tran_y);
    }
}
#endif /*CONFIG_ENABLE_GESTURE_INFORMATION_MODE */

static ssize_t class_ts_info_show(struct class *class,
                                  struct class_attribute *attr, char *buf)
{
    return snprintf(buf, 512, "ilitek FW version:%d.%d.%d\n",
                   g_ilitek_tp_info.sz_fw_version[1], g_ilitek_tp_info.sz_fw_version[2],
                   g_ilitek_tp_info.sz_fw_version[3]);
}

static class_attr(ts_info, S_IRUSR | S_IWUSR, class_ts_info_show, NULL);

static struct class *touchscreen_class;

static ssize_t gesture_show(struct class *class, struct class_attribute *attr,
                            char *buf)
{
    if (g_gesture_state)
		return snprintf(buf, 512, "gesture: on\n");
    else
		return snprintf(buf, 512, "gesture: off\n");
}

static ssize_t gesture_store(struct class *class, struct class_attribute *attr,
                             const char *buf, size_t count)
{
    if (!strncmp(buf, "on", 2))
        g_gesture_state = true;
    else if (!strncmp(buf, "off", 3))
        g_gesture_state = false;
    pr_debug("buf = %s, g_gesture_state = %d, count = %zu\n", buf,
             g_gesture_state, count);
    return count;
}

static class_attr(gesture, S_IRUSR | S_IWUSR, gesture_show, gesture_store);

#endif /*CONFIG_ENABLE_GESTURE_WAKEUP */

/*---------------------------------------------------------------------------*/
static s32 drv_create_procfs_dir_entry(void)
{
    s32 n_ret_val = 0;
#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
    u8 *p_gesture_path = NULL;
#endif /*CONFIG_ENABLE_GESTURE_DEBUG_MODE */
#endif /*CONFIG_ENABLE_GESTURE_WAKEUP */
    u8 *p_device_path = NULL;

    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    g_proc_class_entry = proc_mkdir(PROC_NODE_CLASS, NULL);

    g_proc_ms_touchscreen_msg20xx_entry =
        proc_mkdir(PROC_NODE_MS_TOUCHSCREEN_MSG20XX, g_proc_class_entry);

    g_proc_device_entry =
        proc_mkdir(PROC_NODE_DEVICE, g_proc_ms_touchscreen_msg20xx_entry);
#ifdef CONFIG_ENABLE_ITO_MP_TEST
    g_proc_mp_test_customised_entry =
        proc_create(PROC_NODE_MP_TEST_CUSTOMISED, PROCFS_AUTHORITY,
                    g_proc_device_entry, &g_proc_mp_test_customised);
    if (g_proc_mp_test_customised_entry == NULL) {
        DBG(&g_i2c_client->dev, "Failed to create procfs file node(%s)!\n",
            PROC_NODE_MP_TEST_CUSTOMISED);
    } else {
        DBG(&g_i2c_client->dev, "Create procfs file node(%s) OK!\n",
            PROC_NODE_MP_TEST_CUSTOMISED);
    }
#endif
    g_proc_chip_type_entry =
        proc_create(PROC_NODE_CHIP_TYPE, PROCFS_AUTHORITY, g_proc_device_entry,
                    &g_proc_chip_type);
    if (g_proc_chip_type_entry == NULL) {
        DBG(&g_i2c_client->dev, "Failed to create procfs file node(%s)!\n",
            PROC_NODE_CHIP_TYPE);
    } else {
        DBG(&g_i2c_client->dev, "Create procfs file node(%s) OK!\n",
            PROC_NODE_CHIP_TYPE);
    }

    g_proc_firmware_data_entry =
        proc_create(PROC_NODE_FIRMWARE_DATA, PROCFS_AUTHORITY,
                    g_proc_device_entry, &g_proc_firmware_data);
    if (g_proc_firmware_data_entry == NULL) {
        DBG(&g_i2c_client->dev, "Failed to create procfs file node(%s)!\n",
            PROC_NODE_FIRMWARE_DATA);
    } else {
        DBG(&g_i2c_client->dev, "Create procfs file node(%s) OK!\n",
            PROC_NODE_FIRMWARE_DATA);
    }

    g_proc_apk_firmware_update_entry =
        proc_create(PROC_NODE_FIRMWARE_UPDATE, PROCFS_AUTHORITY,
                    g_proc_device_entry, &g_proc_apk_firmware_update);
    if (g_proc_apk_firmware_update_entry == NULL) {
        DBG(&g_i2c_client->dev, "Failed to create procfs file node(%s)!\n",
            PROC_NODE_FIRMWARE_UPDATE);
    } else {
        DBG(&g_i2c_client->dev, "Create procfs file node(%s) OK!\n",
            PROC_NODE_FIRMWARE_UPDATE);
    }

    g_proc_customer_firmware_version_entry =
        proc_create(PROC_NODE_CUSTOMER_FIRMWARE_VERSION, PROCFS_AUTHORITY,
                    g_proc_device_entry, &g_proc_customer_firmware_version);
    if (g_proc_customer_firmware_version_entry == NULL) {
        DBG(&g_i2c_client->dev, "Failed to create procfs file node(%s)!\n",
            PROC_NODE_CUSTOMER_FIRMWARE_VERSION);
    } else {
        DBG(&g_i2c_client->dev, "Create procfs file node(%s) OK!\n",
            PROC_NODE_CUSTOMER_FIRMWARE_VERSION);
    }

    g_proc_platform_firmware_version_entry =
        proc_create(PROC_NODE_PLATFORM_FIRMWARE_VERSION, PROCFS_AUTHORITY,
                    g_proc_device_entry, &g_proc_platform_firmware_version);
    if (g_proc_platform_firmware_version_entry == NULL) {
        DBG(&g_i2c_client->dev, "Failed to create procfs file node(%s)!\n",
            PROC_NODE_PLATFORM_FIRMWARE_VERSION);
    } else {
        DBG(&g_i2c_client->dev, "Create procfs file node(%s) OK!\n",
            PROC_NODE_PLATFORM_FIRMWARE_VERSION);
    }

    g_proc_device_driver_version_entry =
        proc_create(PROC_NODE_DEVICE_DRIVER_VERSION, PROCFS_AUTHORITY,
                    g_proc_device_entry, &g_proc_device_driver_version);
    if (g_proc_device_driver_version_entry == NULL) {
        DBG(&g_i2c_client->dev, "Failed to create procfs file node(%s)!\n",
            PROC_NODE_DEVICE_DRIVER_VERSION);
    } else {
        DBG(&g_i2c_client->dev, "Create procfs file node(%s) OK!\n",
            PROC_NODE_DEVICE_DRIVER_VERSION);
    }

    g_proc_sd_card_firmware_update_entry =
        proc_create(PROC_NODE_SD_CARD_FIRMWARE_UPDATE, PROCFS_AUTHORITY,
                    g_proc_device_entry, &g_proc_sd_card_firmware_update);
    if (g_proc_sd_card_firmware_update_entry == NULL) {
        DBG(&g_i2c_client->dev, "Failed to create procfs file node(%s)!\n",
            PROC_NODE_SD_CARD_FIRMWARE_UPDATE);
    } else {
        DBG(&g_i2c_client->dev, "Create procfs file node(%s) OK!\n",
            PROC_NODE_SD_CARD_FIRMWARE_UPDATE);
    }

    g_proc_firmware_debug_entry =
        proc_create(PROC_NODE_FIRMWARE_DEBUG, PROCFS_AUTHORITY,
                    g_proc_device_entry, &g_proc_firmware_debug);
    if (g_proc_firmware_debug_entry == NULL) {
        DBG(&g_i2c_client->dev, "Failed to create procfs file node(%s)!\n",
            PROC_NODE_FIRMWARE_DEBUG);
    } else {
        DBG(&g_i2c_client->dev, "Create procfs file node(%s) OK!\n",
            PROC_NODE_FIRMWARE_DEBUG);
    }

    g_proc_firmware_set_debug_value_entry =
        proc_create(PROC_NODE_FIRMWARE_SET_DEBUG_VALUE, PROCFS_AUTHORITY,
                    g_proc_device_entry, &g_proc_firmware_set_debug_value);
    if (g_proc_firmware_set_debug_value_entry == NULL) {
        DBG(&g_i2c_client->dev, "Failed to create procfs file node(%s)!\n",
            PROC_NODE_FIRMWARE_SET_DEBUG_VALUE);
    } else {
        DBG(&g_i2c_client->dev, "Create procfs file node(%s) OK!\n",
            PROC_NODE_FIRMWARE_SET_DEBUG_VALUE);
    }

    g_proc_firmware_sm_bus_debug_entry =
        proc_create(PROC_NODE_FIRMWARE_SMBUS_DEBUG, PROCFS_AUTHORITY,
                    g_proc_device_entry, &g_proc_firmware_sm_bus_debug);
    if (NULL == g_proc_firmware_sm_bus_debug_entry) {
        DBG(&g_i2c_client->dev, "Failed to create procfs file node(%s)!\n",
            PROC_NODE_FIRMWARE_SMBUS_DEBUG);
    } else {
        DBG(&g_i2c_client->dev, "Create procfs file node(%s) OK!\n",
            PROC_NODE_FIRMWARE_SMBUS_DEBUG);
    }

    g_proc_firmware_set_dq_mem_value_entry =
        proc_create(PROC_NODE_FIRMWARE_SET_DQMEM_VALUE, PROCFS_AUTHORITY,
                    g_proc_device_entry, &g_proc_firmware_set_dq_mem_value);
    if (g_proc_firmware_set_dq_mem_value_entry == NULL) {
        DBG(&g_i2c_client->dev, "Failed to create procfs file node(%s)!\n",
            PROC_NODE_FIRMWARE_SET_DQMEM_VALUE);
    } else {
        DBG(&g_i2c_client->dev, "Create procfs file node(%s) OK!\n",
            PROC_NODE_FIRMWARE_SET_DQMEM_VALUE);
    }

#ifdef CONFIG_ENABLE_ITO_MP_TEST
    g_proc_mp_test_entry =
        proc_create(PROC_NODE_MP_TEST, PROCFS_AUTHORITY, g_proc_device_entry,
                    &g_proc_mp_test);
    if (g_proc_mp_test_entry == NULL) {
        DBG(&g_i2c_client->dev, "Failed to create procfs file node(%s)!\n",
            PROC_NODE_MP_TEST);
    } else {
        DBG(&g_i2c_client->dev, "Create procfs file node(%s) OK!\n",
            PROC_NODE_MP_TEST);
    }

    g_proc_mp_test_log_entry =
        proc_create(PROC_NODE_MP_TEST_LOG, PROCFS_AUTHORITY,
                    g_proc_device_entry, &g_proc_mp_test_log);
    if (g_proc_mp_test_log_entry == NULL) {
        DBG(&g_i2c_client->dev, "Failed to create procfs file node(%s)!\n",
            PROC_NODE_MP_TEST_LOG);
    } else {
        DBG(&g_i2c_client->dev, "Create procfs file node(%s) OK!\n",
            PROC_NODE_MP_TEST_LOG);
    }

    g_proc_mp_test_fail_channel_entry =
        proc_create(PROC_NODE_MP_TEST_FAIL_CHANNEL, PROCFS_AUTHORITY,
                    g_proc_device_entry, &g_proc_mp_test_fail_channel);
    if (g_proc_mp_test_fail_channel_entry == NULL) {
        DBG(&g_i2c_client->dev, "Failed to create procfs file node(%s)!\n",
            PROC_NODE_MP_TEST_FAIL_CHANNEL);
    } else {
        DBG(&g_i2c_client->dev, "Create procfs file node(%s) OK!\n",
            PROC_NODE_MP_TEST_FAIL_CHANNEL);
    }

    g_proc_mp_test_scope_entry =
        proc_create(PROC_NODE_MP_TEST_SCOPE, PROCFS_AUTHORITY,
                    g_proc_device_entry, &g_proc_mp_test_scope);
    if (g_proc_mp_test_scope_entry == NULL) {
        DBG(&g_i2c_client->dev, "Failed to create procfs file node(%s)!\n",
            PROC_NODE_MP_TEST_SCOPE);
    } else {
        DBG(&g_i2c_client->dev, "Create procfs file node(%s) OK!\n",
            PROC_NODE_MP_TEST_SCOPE);
    }

#ifdef CONFIG_ENABLE_CHIP_TYPE_MSG28XX
    g_proc_mp_test_log_all_entry =
        proc_create(PROC_NODE_MP_TEST_LOG_ALL, PROCFS_AUTHORITY,
                    g_proc_device_entry, &g_proc_mp_test_log_all);
    if (g_proc_mp_test_log_all_entry == NULL) {
        DBG(&g_i2c_client->dev, "Failed to create procfs file node(%s)!\n",
            PROC_NODE_MP_TEST_LOG_ALL);
    } else {
        DBG(&g_i2c_client->dev, "Create procfs file node(%s) OK!\n",
            PROC_NODE_MP_TEST_LOG_ALL);
    }
#endif /*CONFIG_ENABLE_CHIP_TYPE_MSG28XX */
#endif /*CONFIG_ENABLE_ITO_MP_TEST */

    g_proc_firmware_mode_entry =
        proc_create(PROC_NODE_FIRMWARE_MODE, PROCFS_AUTHORITY,
                    g_proc_device_entry, &g_proc_firmware_mode);
    if (g_proc_firmware_mode_entry == NULL) {
        DBG(&g_i2c_client->dev, "Failed to create procfs file node(%s)!\n",
            PROC_NODE_FIRMWARE_MODE);
    } else {
        DBG(&g_i2c_client->dev, "Create procfs file node(%s) OK!\n",
            PROC_NODE_FIRMWARE_MODE);
    }

    g_proc_firmware_sensor_entry =
        proc_create(PROC_NODE_FIRMWARE_SENSOR, PROCFS_AUTHORITY,
                    g_proc_device_entry, &g_proc_firmware_sensor);
    if (g_proc_firmware_sensor_entry == NULL) {
        DBG(&g_i2c_client->dev, "Failed to create procfs file node(%s)!\n",
            PROC_NODE_FIRMWARE_SENSOR);
    } else {
        DBG(&g_i2c_client->dev, "Create procfs file node(%s) OK!\n",
            PROC_NODE_FIRMWARE_SENSOR);
    }

    g_proc_firmware_packet_header_entry =
        proc_create(PROC_NODE_FIRMWARE_PACKET_HEADER, PROCFS_AUTHORITY,
                    g_proc_device_entry, &g_proc_firmware_packet_header);
    if (g_proc_firmware_packet_header_entry == NULL) {
        DBG(&g_i2c_client->dev, "Failed to create procfs file node(%s)!\n",
            PROC_NODE_FIRMWARE_PACKET_HEADER);
    } else {
        DBG(&g_i2c_client->dev, "Create procfs file node(%s) OK!\n",
            PROC_NODE_FIRMWARE_PACKET_HEADER);
    }

    /* create a kset with the name of "kset_example" which is located under /sys/kernel/ */
    g_touch_k_set = kset_create_and_add("kset_example", NULL, kernel_kobj);
    if (!g_touch_k_set != 0) {
        DBG(&g_i2c_client->dev,
            "*** kset_create_and_add() failed, n_ret_val = %d ***\n", n_ret_val);

        n_ret_val = -ENOMEM;
    }

    g_touch_k_obj = kobject_create();
    if (!g_touch_k_obj != 0) {
        DBG(&g_i2c_client->dev,
            "*** kobject_create() failed, n_ret_val = %d ***\n", n_ret_val);

        n_ret_val = -ENOMEM;
        kset_unregister(g_touch_k_set);
        g_touch_k_set = NULL;
    }

    g_touch_k_obj->kset = g_touch_k_set;

    n_ret_val = kobject_add(g_touch_k_obj, NULL, "%s", "kobject_example");
    if (n_ret_val != 0) {
        DBG(&g_i2c_client->dev, "*** kobject_add() failed, n_ret_val = %d ***\n",
            n_ret_val);

        kobject_put(g_touch_k_obj);
        g_touch_k_obj = NULL;
        kset_unregister(g_touch_k_set);
        g_touch_k_set = NULL;
    }

    /* create the files associated with this kobject */
    n_ret_val = sysfs_create_group(g_touch_k_obj, &attr_group);
    if (n_ret_val != 0) {
        DBG(&g_i2c_client->dev,
            "*** sysfs_create_file() failed, n_ret_val = %d ***\n", n_ret_val);

        kobject_put(g_touch_k_obj);
        g_touch_k_obj = NULL;
        kset_unregister(g_touch_k_set);
        g_touch_k_set = NULL;
    }

    p_device_path = kobject_get_path(g_touch_k_obj, GFP_KERNEL);
    DBG(&g_i2c_client->dev, "DEVPATH = %s\n", p_device_path);

    g_proc_query_feature_support_status_entry =
        proc_create(PROC_NODE_QUERY_FEATURE_SUPPORT_STATUS, PROCFS_AUTHORITY,
                    g_proc_device_entry, &g_proc_query_feature_support_status);
    if (g_proc_query_feature_support_status_entry == NULL) {
        DBG(&g_i2c_client->dev, "Failed to create procfs file node(%s)!\n",
            PROC_NODE_QUERY_FEATURE_SUPPORT_STATUS);
    } else {
        DBG(&g_i2c_client->dev, "Create procfs file node(%s) OK!\n",
            PROC_NODE_QUERY_FEATURE_SUPPORT_STATUS);
    }

    g_proc_change_feature_support_status_entry =
        proc_create(PROC_NODE_CHANGE_FEATURE_SUPPORT_STATUS, PROCFS_AUTHORITY,
                    g_proc_device_entry, &g_proc_change_feature_support_status);
    if (g_proc_query_feature_support_status_entry == NULL) {
        DBG(&g_i2c_client->dev, "Failed to create procfs file node(%s)!\n",
            PROC_NODE_CHANGE_FEATURE_SUPPORT_STATUS);
    } else {
        DBG(&g_i2c_client->dev, "Create procfs file node(%s) OK!\n",
            PROC_NODE_CHANGE_FEATURE_SUPPORT_STATUS);
    }

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
    g_proc_gesture_wakeup_mode_entry =
        proc_create(PROC_NODE_GESTURE_WAKEUP_MODE, PROCFS_AUTHORITY,
                    g_proc_device_entry, &g_proc_gesture_wake_up_mode);
    if (g_proc_gesture_wakeup_mode_entry == NULL) {
        DBG(&g_i2c_client->dev, "Failed to create procfs file node(%s)!\n",
            PROC_NODE_GESTURE_WAKEUP_MODE);
    } else {
        DBG(&g_i2c_client->dev, "Create procfs file node(%s) OK!\n",
            PROC_NODE_GESTURE_WAKEUP_MODE);
    }

    touchscreen_class = class_create(THIS_MODULE, "touchscreen");
    if (IS_ERR_OR_NULL(touchscreen_class)) {
        DBG(&g_i2c_client->dev, "%s: create class error!\n", __func__);
        return -ENOMEM;
    }

    n_ret_val = class_create_file(touchscreen_class, &class_attr_ts_info);
    if (n_ret_val < 0) {
        DBG(&g_i2c_client->dev, "%s class_create_file failed!\n", __func__);
        class_destroy(touchscreen_class);
        return -ENOMEM;
    }

    n_ret_val = class_create_file(touchscreen_class, &class_attr_gesture);
    if (n_ret_val < 0) {
        DBG(&g_i2c_client->dev, "%s create gesture file failed!\n", __func__);
        class_destroy(touchscreen_class);
        return -ENOMEM;
    }
#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
    g_proc_gesture_debug_mode_entry =
        proc_create(PROC_NODE_GESTURE_DEBUG_MODE, PROCFS_AUTHORITY,
                    g_proc_device_entry, &g_proc_gesture_debug_mode);
    if (NULL == g_proc_gesture_debug_mode_entry) {
        DBG(&g_i2c_client->dev, "Failed to create procfs file node(%s)!\n",
            PROC_NODE_GESTURE_DEBUG_MODE);
    } else {
        DBG(&g_i2c_client->dev, "Create procfs file node(%s) OK!\n",
            PROC_NODE_GESTURE_DEBUG_MODE);
    }

    /* create a kset with the name of "kset_gesture" which is located under /sys/kernel/ */
    g_gesture_k_set = kset_create_and_add("kset_gesture", NULL, kernel_kobj);
    if (!g_gesture_k_set) {
        DBG(&g_i2c_client->dev,
            "*** kset_create_and_add() failed, n_ret_val = %d ***\n", n_ret_val);
        n_ret_val = -ENOMEM;
    }

    g_gesture_k_obj = kobject_create();
    if (!g_gesture_k_obj) {
        DBG(&g_i2c_client->dev,
            "*** kobject_create() failed, n_ret_val = %d ***\n", n_ret_val);

        n_ret_val = -ENOMEM;
        kset_unregister(g_gesture_k_set);
        g_gesture_k_set = NULL;
    }

    g_gesture_k_obj->kset = g_gesture_k_set;

    n_ret_val = kobject_add(g_gesture_k_obj, NULL, "%s", "kobject_gesture");
    if (n_ret_val != 0) {
        DBG(&g_i2c_client->dev, "*** kobject_add() failed, n_ret_val = %d ***\n",
            n_ret_val);

        kobject_put(g_gesture_k_obj);
        g_gesture_k_obj = NULL;
        kset_unregister(g_gesture_k_set);
        g_gesture_k_set = NULL;
    }

    /* create the files associated with this g_gesture_k_obj */
    n_ret_val = sysfs_create_group(g_gesture_k_obj, &gestureattr_group);
    if (n_ret_val != 0) {
        DBG(&g_i2c_client->dev,
            "*** sysfs_create_file() failed, n_ret_val = %d ***\n", n_ret_val);

        kobject_put(g_gesture_k_obj);
        g_gesture_k_obj = NULL;
        kset_unregister(g_gesture_k_set);
        g_gesture_k_set = NULL;
    }

    p_gesture_path = kobject_get_path(g_gesture_k_obj, GFP_KERNEL);
    DBG(&g_i2c_client->dev, "DEVPATH = %s\n", p_gesture_path);
#endif /*CONFIG_ENABLE_GESTURE_DEBUG_MODE */

#ifdef CONFIG_ENABLE_GESTURE_INFORMATION_MODE
    g_proc_gesture_infor_mode_entry =
        proc_create(PROC_NODE_GESTURE_INFORMATION_MODE, PROCFS_AUTHORITY,
                    g_proc_device_entry, &g_proc_gesture_infor_mode);
    if (NULL == g_proc_gesture_infor_mode_entry) {
        DBG(&g_i2c_client->dev, "Failed to create procfs file node(%s)!\n",
            PROC_NODE_GESTURE_INFORMATION_MODE);
    } else {
        DBG(&g_i2c_client->dev, "Create procfs file node(%s) OK!\n",
            PROC_NODE_GESTURE_INFORMATION_MODE);
    }
#endif /*CONFIG_ENABLE_GESTURE_INFORMATION_MODE */
#endif /*CONFIG_ENABLE_GESTURE_WAKEUP */

#ifdef CONFIG_ENABLE_COUNT_REPORT_RATE
    g_proc_report_rate_entry =
        proc_create(PROC_NODE_REPORT_RATE, PROCFS_AUTHORITY,
                    g_proc_device_entry, &g_proc_report_rate);
    if (NULL == g_proc_report_rate_entry) {
        DBG(&g_i2c_client->dev, "Failed to create procfs file node(%s)!\n",
            PROC_NODE_REPORT_RATE);
    } else {
        DBG(&g_i2c_client->dev, "Create procfs file node(%s) OK!\n",
            PROC_NODE_REPORT_RATE);
    }
#endif /*CONFIG_ENABLE_COUNT_REPORT_RATE */

    g_proc_glove_mode_entry =
        proc_create(PROC_NODE_GLOVE_MODE, PROCFS_AUTHORITY, g_proc_device_entry,
                    &g_proc_glove_mode);
    if (g_proc_glove_mode_entry == NULL) {
        DBG(&g_i2c_client->dev, "Failed to create procfs file node(%s)!\n",
            PROC_NODE_GLOVE_MODE);
    } else {
        DBG(&g_i2c_client->dev, "Create procfs file node(%s) OK!\n",
            PROC_NODE_GLOVE_MODE);
    }

    g_proc_open_glove_mode_entry =
        proc_create(PROC_NODE_OPEN_GLOVE_MODE, PROCFS_AUTHORITY,
                    g_proc_device_entry, &g_proc_open_glove_mode);
    if (g_proc_open_glove_mode_entry == NULL) {
        DBG(&g_i2c_client->dev, "Failed to create procfs file node(%s)!\n",
            PROC_NODE_OPEN_GLOVE_MODE);
    } else {
        DBG(&g_i2c_client->dev, "Create procfs file node(%s) OK!\n",
            PROC_NODE_OPEN_GLOVE_MODE);
    }

    g_proc_close_glove_mode_entry =
        proc_create(PROC_NODE_CLOSE_GLOVE_MODE, PROCFS_AUTHORITY,
                    g_proc_device_entry, &g_proc_close_glove_mode);
    if (g_proc_close_glove_mode_entry == NULL) {
        DBG(&g_i2c_client->dev, "Failed to create procfs file node(%s)!\n",
            PROC_NODE_CLOSE_GLOVE_MODE);
    } else {
        DBG(&g_i2c_client->dev, "Create procfs file node(%s) OK!\n",
            PROC_NODE_CLOSE_GLOVE_MODE);
    }

    g_proc_leather_sheath_mode_entry =
        proc_create(PROC_NODE_LEATHER_SHEATH_MODE, PROCFS_AUTHORITY,
                    g_proc_device_entry, &g_proc_leather_sheath_mode);
    if (g_proc_leather_sheath_mode_entry == NULL) {
        DBG(&g_i2c_client->dev, "Failed to create procfs file node(%s)!\n",
            PROC_NODE_LEATHER_SHEATH_MODE);
    } else {
        DBG(&g_i2c_client->dev, "Create procfs file node(%s) OK!\n",
            PROC_NODE_LEATHER_SHEATH_MODE);
    }

    g_proc_film_mode_entry =
        proc_create(PROC_NODE_CONTROL_FILM_MODE, PROCFS_AUTHORITY,
                    g_proc_device_entry, &g_proc_film_mode);
    if (g_proc_film_mode_entry == NULL) {
        DBG(&g_i2c_client->dev, "Failed to create procfs file node(%s)!\n",
            PROC_NODE_CONTROL_FILM_MODE);
    } else {
        DBG(&g_i2c_client->dev, "Create procfs file node(%s) OK!\n",
            PROC_NODE_CONTROL_FILM_MODE);
    }

#ifdef CONFIG_ENABLE_JNI_INTERFACE
    g_proc_jni_method_entry =
        proc_create(PROC_NODE_JNI_NODE, PROCFS_AUTHORITY, g_proc_device_entry,
                    &g_proc_jni_method);
    if (g_proc_jni_method_entry == NULL) {
        DBG(&g_i2c_client->dev, "Failed to create procfs file node(%s)!\n",
            PROC_NODE_JNI_NODE);
    } else {
        DBG(&g_i2c_client->dev, "Create procfs file node(%s) OK!\n",
            PROC_NODE_JNI_NODE);
    }
#endif /*CONFIG_ENABLE_JNI_INTERFACE */

    g_proc_selinux_limit_firmware_update_entry =
        proc_create(PROC_NODE_SELINUX_LIMIT_FIRMWARE_UPDATE, PROCFS_AUTHORITY,
                    g_proc_device_entry, &g_proc_selinux_limit_firmware_update);
    if (g_proc_selinux_limit_firmware_update_entry == NULL) {
        DBG(&g_i2c_client->dev, "Failed to create procfs file node(%s)!\n",
            PROC_NODE_SELINUX_LIMIT_FIRMWARE_UPDATE);
    } else {
        DBG(&g_i2c_client->dev, "Create procfs file node(%s) OK!\n",
            PROC_NODE_SELINUX_LIMIT_FIRMWARE_UPDATE);
    }

    g_proc_force_firmware_update_entry =
        proc_create(PROC_NODE_FORCE_FIRMWARE_UPDATE, PROCFS_AUTHORITY,
                    g_proc_device_entry, &g_proc_force_firmware_update);
    if (g_proc_force_firmware_update_entry == NULL) {
        DBG(&g_i2c_client->dev, "Failed to create procfs file node(%s)!\n",
            PROC_NODE_FORCE_FIRMWARE_UPDATE);
    } else {
        DBG(&g_i2c_client->dev, "Create procfs file node(%s) OK!\n",
            PROC_NODE_FORCE_FIRMWARE_UPDATE);
    }

    g_proc_trim_code_entry =
        proc_create(PROC_NODE_TRIM_CODE, PROCFS_AUTHORITY, g_proc_device_entry,
                    &g_proc_trim_code);
    if (g_proc_trim_code_entry == NULL) {
        DBG(&g_i2c_client->dev, "Failed to create procfs file node(%s)!\n",
            PROC_NODE_TRIM_CODE);
    } else {
        DBG(&g_i2c_client->dev, "Create procfs file node(%s) OK!\n",
            PROC_NODE_TRIM_CODE);
    }
    return n_ret_val;
}

static ssize_t drv_build_id_show(struct device *pDevice, struct device_attribute *pAttr, char *pBuf)
{
	u16 n_major = 0, n_minor = 0;
	char version[8] = {0};
	int retry = 0;

	while ((n_major | n_minor) == 0 && retry++ < 3) {
		drv_get_customer_firmware_version(&n_major, &n_minor, &g_fw_version);
		snprintf(version, ARRAY_SIZE(version), "%02d-%04d", n_major, n_minor);
	}
	DBG(&g_i2c_client->dev, "*** %s() _gFwVersion = %s ***\n", __func__, version);

	return scnprintf(pBuf, PAGE_SIZE, "%s\n", version);
}

static DEVICE_ATTR(buildid, 0444, drv_build_id_show, NULL);

static ssize_t drv_reset_store(struct device *pDevice, struct device_attribute *pAttr,
						const char *pBuf, size_t nSize)
{
	if ('1' != pBuf[0]) {
		pr_err("Invaild argument for reset\n");
		return -EINVAL;
	}

	drv_disable_finger_touch_report();
	drv_touch_device_hw_reset();
	msleep(50);
	drv_enable_finger_touch_report();

	return nSize;
}

static DEVICE_ATTR(reset, 0220, NULL, drv_reset_store);

static ssize_t drv_dore_flash_store(struct device *pDevice, struct device_attribute *pAttr, const char *pBuf, size_t nSize)
{
	char *p_valid = NULL;
	char *p_tmp_file_path = NULL;
	char sz_file_path[100] = { 0 };
	char *p_str = NULL;

	DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);
	DBG(&g_i2c_client->dev, "pBuf = %s\n", pBuf);
	memset(g_debug_buf, 0, 1024);
	memcpy(g_debug_buf, pBuf, nSize - 1);

	p_str = g_debug_buf;
	if (p_str != NULL) {
		p_valid = strnstr(p_str, ".bin", sizeof(g_debug_buf));

		if (p_valid) {
			p_tmp_file_path = strsep((char **)&p_str, ".");

			DBG(&g_i2c_client->dev, "p_tmp_file_path = %s\n", p_tmp_file_path);

			strlcat(sz_file_path, p_tmp_file_path, sizeof(sz_file_path));
			strlcat(sz_file_path, ".bin", sizeof(sz_file_path));

			DBG(&g_i2c_client->dev, "sz_file_path = %s\n", sz_file_path);

			if (!g_is_suspend) {
				is_force_to_update_firmware_enabled = 0;
				g_system_update = 1;
				if (0 != drv_check_update_firmware_by_sd_card(sz_file_path)) {
					DBG(&g_i2c_client->dev, "Update FAILED\n");
				} else {
					DBG(&g_i2c_client->dev, "Update SUCCESS\n");
				}
			} else {
				DBG(&g_i2c_client->dev, "Suspend state,can not update.\n");
			}
		} else {
			DBG(&g_i2c_client->dev, "The file type of the update firmware bin file is not a .bin file.\n");
		}
	} else {
		DBG(&g_i2c_client->dev, "The file path of the update firmware bin file is NULL.\n");
	}

	return nSize;
}
static DEVICE_ATTR(doreflash, 0220, NULL, drv_dore_flash_store);

static ssize_t drv_flash_prog_show(struct device *pDevice, struct device_attribute *pAttr, char *pBuf)
{
	DBG(&g_i2c_client->dev, "*** %s() g_is_update_firmware = %d ***\n", __func__, g_is_update_firmware);

	return scnprintf(pBuf, PAGE_SIZE, "%d\n", g_is_update_firmware);
}

static DEVICE_ATTR(flashprog, 0444, drv_flash_prog_show, NULL);

static ssize_t drv_force_reflash_store(struct device *pDevice, struct device_attribute *pAttr, const char *pBuf, size_t nSize)
{
	char *p_valid = NULL;
	char *p_tmp_file_path = NULL;
	char sz_file_path[100] = { 0 };
	char *p_str = NULL;

	DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);
	DBG(&g_i2c_client->dev, "pBuf = %s\n", pBuf);
	memset(g_debug_buf, 0, 1024);
	memcpy(g_debug_buf, pBuf, nSize - 1);
	p_str = g_debug_buf;

	if (p_str != NULL) {
		p_valid = strnstr(p_str, ".bin", sizeof(g_debug_buf));

		if (p_valid) {
			p_tmp_file_path = strsep((char **)&p_str, ".");

			DBG(&g_i2c_client->dev, "p_tmp_file_path = %s\n", p_tmp_file_path);

			strlcat(sz_file_path, p_tmp_file_path, sizeof(sz_file_path));
			strlcat(sz_file_path, ".bin", sizeof(sz_file_path));

			DBG(&g_i2c_client->dev, "sz_file_path = %s\n", sz_file_path);

			if (!g_is_suspend) {
				is_force_to_update_firmware_enabled = 1;
				g_system_update = 1;
				if (0 != drv_check_update_firmware_by_sd_card(sz_file_path)) {
					DBG(&g_i2c_client->dev, "Update FAILED\n");
				} else {
					DBG(&g_i2c_client->dev, "Update SUCCESS\n");
				}
			} else {
				DBG(&g_i2c_client->dev, "Suspend state,can not update.\n");
			}
		} else {
			DBG(&g_i2c_client->dev, "The file type of the update firmware bin file is not a .bin file.\n");
		}
	} else {
		DBG(&g_i2c_client->dev, "The file path of the update firmware bin file is NULL.\n");
	}

	return nSize;
}
static DEVICE_ATTR(forcereflash, 0220, NULL, drv_force_reflash_store);

static ssize_t drv_product_info_show(struct device *pDevice, struct device_attribute *pAttr, char *pBuf)
{
	DBG(&g_i2c_client->dev, "*** %s() productinfo = %s ***\n", __func__, MSG_TP_IC_TYPE);

	return scnprintf(pBuf, PAGE_SIZE, "%s\n", MSG_TP_IC_TYPE);
}

static DEVICE_ATTR(productinfo, 0444, drv_product_info_show, NULL);

static ssize_t drv_power_on_show(struct device *pDevice, struct device_attribute *pAttr, char *pBuf)
{
	DBG(&g_i2c_client->dev, "*** %s() g_is_suspend = %d ***\n", __func__, g_is_suspend);

	return snprintf(pBuf, PAGE_SIZE, "%d\n", g_is_suspend == 0);
}

static DEVICE_ATTR(poweron, 0444, drv_power_on_show, NULL);

static ssize_t drv_path_show(struct device *pDevice, struct device_attribute *pAttr, char *pBuf)
{
	ssize_t blen;
	const char *path;

	path = kobject_get_path(&g_i2c_client->dev.kobj, GFP_KERNEL);
	blen = scnprintf(pBuf, PAGE_SIZE, "%s", path ? path : "na");
	kfree(path);

	return blen;
}

static DEVICE_ATTR(path, 0444, drv_path_show, NULL);

static ssize_t drv_vendor_show(struct device *pDevice, struct device_attribute *pAttr, char *pBuf)
{
	DBG(&g_i2c_client->dev, "*** %s() vendor = %s ***\n", __func__, "mstar");

	return scnprintf(pBuf, PAGE_SIZE, "%s\n", "mstar");
}

static DEVICE_ATTR(vendor, 0444, drv_vendor_show, NULL);

static s32 drv_create_system_dir_entry(void)
{
	s32 n_ret_val = 0;
	static int minor;
	dev_t devno;

	minor = input_get_new_minor(g_i2c_client->addr, 1, false);
	if (minor < 0) {
		minor = input_get_new_minor(TS_DEV_MINOR_BASE, TS_DEV_MINOR_MAX, true);
		DBG(&g_i2c_client->dev, "assigned minor %d\n", minor);
	}
	/* set sysfs for firmware */
	g_firmware_class = class_create(THIS_MODULE, "touchscreen");
	if (IS_ERR(g_firmware_class)) {
		n_ret_val = PTR_ERR(g_firmware_class);
		g_firmware_class = NULL;
		DBG(&g_i2c_client->dev, "Failed to create class(firmware)!\n");
		return n_ret_val;
	}
	n_ret_val = alloc_chrdev_region(&devno, 0, 1, MSG_TP_IC_TYPE);
	if (n_ret_val) {
		DBG(&g_i2c_client->dev, "can't allocate chrdev\n");
		return n_ret_val;
	} else {
		DBG(&g_i2c_client->dev, "register chrdev(%d, %d)\n", MAJOR(devno), MINOR(devno));
		g_firmware_cmd_dev = device_create(g_firmware_class, NULL, devno, NULL, MSG_TP_IC_TYPE);
		if (IS_ERR(g_firmware_cmd_dev)) {
			n_ret_val = PTR_ERR(g_firmware_cmd_dev);
			g_firmware_cmd_dev = NULL;
			DBG(&g_i2c_client->dev, "Failed to create device(g_firmware_cmd_dev)!\n");
		}

		if (device_create_file(&g_i2c_client->dev, &dev_attr_buildid) < 0) {
			DBG(&g_i2c_client->dev, "Failed to create device file(%s)!\n", dev_attr_buildid.attr.name);
		}
		if (device_create_file(&g_i2c_client->dev, &dev_attr_doreflash) < 0) {
			DBG(&g_i2c_client->dev, "Failed to create device file(%s)!\n", dev_attr_doreflash.attr.name);
		}
		if (device_create_file(&g_i2c_client->dev, &dev_attr_flashprog) < 0) {
			DBG(&g_i2c_client->dev, "Failed to create device file(%s)!\n", dev_attr_flashprog.attr.name);
		}
		if (device_create_file(&g_i2c_client->dev, &dev_attr_forcereflash) < 0) {
			DBG(&g_i2c_client->dev, "Failed to create device file(%s)!\n", dev_attr_forcereflash.attr.name);
		}
		if (device_create_file(&g_i2c_client->dev, &dev_attr_productinfo) < 0) {
			DBG(&g_i2c_client->dev, "Failed to create device file(%s)!\n", dev_attr_productinfo.attr.name);
		}
		if (device_create_file(&g_i2c_client->dev, &dev_attr_poweron) < 0) {
			DBG(&g_i2c_client->dev, "Failed to create device file(%s)!\n", dev_attr_poweron.attr.name);
		}

		if (device_create_file(&g_i2c_client->dev, &dev_attr_reset) < 0) {
			DBG(&g_i2c_client->dev, "Failed to create device file(%s)!\n", dev_attr_reset.attr.name);
		}

		if (device_create_file(g_firmware_cmd_dev, &dev_attr_path) < 0) {
			DBG(&g_i2c_client->dev, "Failed to create device file(%s)!\n", dev_attr_path.attr.name);
		}

		if (device_create_file(g_firmware_cmd_dev, &dev_attr_vendor) < 0) {
			DBG(&g_i2c_client->dev, "Failed to create device file(%s)!\n", dev_attr_vendor.attr.name);
		}
    }
    return n_ret_val;
}
static void drv_remove_system_dir_entry(void)
{
	DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

	if (g_firmware_cmd_dev)
	{
		device_remove_file(g_firmware_cmd_dev, &dev_attr_path);

		device_remove_file(g_firmware_cmd_dev, &dev_attr_vendor);

		device_unregister(g_firmware_cmd_dev);
	}

	if (g_firmware_class)
		class_unregister(g_firmware_class);

    device_remove_file(&g_i2c_client->dev, &dev_attr_buildid);

    device_remove_file(&g_i2c_client->dev, &dev_attr_doreflash);

    device_remove_file(&g_i2c_client->dev, &dev_attr_flashprog);

    device_remove_file(&g_i2c_client->dev, &dev_attr_forcereflash);

    device_remove_file(&g_i2c_client->dev, &dev_attr_productinfo);

    device_remove_file(&g_i2c_client->dev, &dev_attr_poweron);

	device_remove_file(&g_i2c_client->dev, &dev_attr_reset);

}

#ifdef CONFIG_ENABLE_NOTIFIER_FB
int ms_drvInterface_touch_device_fb_notifier_callback(struct notifier_block *pSelf,
                                                unsigned long nEvent,
                                                void *p_data)
{
    struct fb_event *p_event_data = p_data;
    int *p_blank;

    if (p_event_data && p_event_data->data &&
        (nEvent == FB_EVENT_BLANK || nEvent == FB_EARLY_EVENT_BLANK)) {
        p_blank = p_event_data->data;

        DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

        if ((*p_blank == FB_BLANK_UNBLANK || *p_blank == FB_BLANK_NORMAL)
            && nEvent == FB_EARLY_EVENT_BLANK) {
            dev_info(&g_i2c_client->dev, "*** %s() TP Resume ***\n", __func__);
	     g_is_suspend = 0;
            if (g_is_update_firmware != 0) {  /*Check whether update frimware is finished */
                DBG(&g_i2c_client->dev,
                    "Not allow to power on/off touch ic while update firmware.\n");
                return 0;
            }
#ifdef CONFIG_ENABLE_PROXIMITY_DETECTION
            if (g_enable_tp_proximity == 1) {
                DBG(&g_i2c_client->dev, "g_enable_tp_proximity = %d\n",
                    g_enable_tp_proximity);
                return 0;
            }
#endif /*CONFIG_ENABLE_PROXIMITY_DETECTION */

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP

#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
            if (g_gesture_debug_mode == 1)
                drv_close_gesture_debug_mode();

#endif /*CONFIG_ENABLE_GESTURE_DEBUG_MODE */

            if (g_gesture_wakeup_flag == 1) {
                DBG(&g_i2c_client->dev,
                    "g_gesture_state = %d, g_chip_type = 0x%X\n", g_gesture_state,
                    g_chip_type);

                if (g_chip_type == CHIP_TYPE_ILI2120 ||
                    g_chip_type == CHIP_TYPE_ILI2121) {
                    if (g_gesture_state) {
                        drv_close_gesture_wakeup();
                        disable_irq_wake(girq);
                    }
                } else {
                    drv_close_gesture_wakeup();
                    disable_irq_wake(girq);
                }
            } else {
                drv_enable_finger_touch_report();
            }

#endif /*CONFIG_ENABLE_GESTURE_WAKEUP */

            {
#ifdef CONFIG_ENABLE_REGULATOR_POWER_ON
                drv_touch_device_regulator_power_on(true);
#endif /*CONFIG_ENABLE_REGULATOR_POWER_ON */
                drv_touch_device_power_on();
            }

#ifdef CONFIG_ENABLE_CHARGER_DETECTION
            {
                u8 szChargerStatus[20] = { 0 };

                drv_read_file(POWER_SUPPLY_BATTERY_STATUS_PATCH, szChargerStatus,
                            20);

                DBG(&g_i2c_client->dev, "*** Battery Status : %s ***\n",
                    szChargerStatus);

                g_force_update = 1; /*Set flag to force update charger status */

				if (strnstr(szChargerStatus, "Charging", 8) != NULL || strnstr(szChargerStatus, "Full", 4) != NULL
							|| strnstr(szChargerStatus, "Fully charged", 13) != NULL) { /*Charging */
                    drv_charger_detection(1); /*charger plug-in */
                } else {        /*Not charging */

                    drv_charger_detection(0); /*charger plug-out */
                }

                g_force_update = 0; /*Clear flag after force update charger status */
            }
#endif /*CONFIG_ENABLE_CHARGER_DETECTION */

            if (g_is_enable_glove_mode == 1)
                drv_open_glove_mode();


            if (g_is_enable_leather_sheath_mode == 1)
                drv_open_leather_sheath_mode();


            if (is_firmware_data_log_enabled)
                drv_restore_firmware_mode_to_log_data_mode();  /*Mark this function call for avoiding device driver may spend longer time to resume from suspend state. */

            /*is_firmware_data_log_enabled */
#ifndef CONFIG_ENABLE_GESTURE_WAKEUP
            drv_enable_finger_touch_report();
#endif /*CONFIG_ENABLE_GESTURE_WAKEUP */

#ifdef CONFIG_ENABLE_CHARGER_DETECTION
            g_is_enable_charger_plugin_out_check = 1;
            queue_delayed_work(g_charger_plug_in_out_check_workqueue,
                               &g_charger_plug_in_out_check_work,
                               CHARGER_DETECT_CHECK_PERIOD);
#endif /*CONFIG_ENABLE_CHARGER_DETECTION */

#ifdef CONFIG_ENABLE_ESD_PROTECTION
            g_is_enable_esd_check = 1;
            queue_delayed_work(g_esd_check_work_queue, &g_esd_check_work,
                               ESD_PROTECT_CHECK_PERIOD);
#endif /*CONFIG_ENABLE_ESD_PROTECTION */
        } else if (*p_blank == FB_BLANK_POWERDOWN && nEvent == FB_EVENT_BLANK) {
            dev_info(&g_i2c_client->dev, "*** %s() TP Suspend ***\n", __func__);
	     g_is_suspend = 1;
#ifdef CONFIG_ENABLE_CHARGER_DETECTION
            g_is_enable_charger_plugin_out_check = 0;
            cancel_delayed_work_sync(&g_charger_plug_in_out_check_work);
#endif /*CONFIG_ENABLE_CHARGER_DETECTION */

#ifdef CONFIG_ENABLE_ESD_PROTECTION
            g_is_enable_esd_check = 0;
            cancel_delayed_work_sync(&g_esd_check_work);
#endif /*CONFIG_ENABLE_ESD_PROTECTION */

            if (g_is_update_firmware != 0) {  /*Check whether update frimware is finished */
                DBG(&g_i2c_client->dev,
                    "Not allow to power on/off touch ic while update firmware.\n");
                return 0;
            }
#ifdef CONFIG_ENABLE_PROXIMITY_DETECTION
            if (g_enable_tp_proximity == 1) {
                DBG(&g_i2c_client->dev, "g_enable_tp_proximity = %d\n",
                    g_enable_tp_proximity);
                return 0;
            }
#endif /*CONFIG_ENABLE_PROXIMITY_DETECTION */

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP

            {
                DBG(&g_i2c_client->dev,
                    " g_gesture_state = %d, g_chip_type = 0x%X\n", g_gesture_state,
                    g_chip_type);

                if (g_chip_type == CHIP_TYPE_ILI2120 ||
                    g_chip_type == CHIP_TYPE_ILI2121) {
                    if (g_gesture_state) {
                        if (g_gesture_wakeup_mode[0] != 0x00000000 ||
                            g_gesture_wakeup_mode[1] != 0x00000000) {
                            drv_open_gesture_wakeup(&g_gesture_wakeup_mode[0]);
                            enable_irq_wake(girq);
                            return 0;
                        }
                    }
                } else {
                    if (g_gesture_wakeup_mode[0] != 0x00000000 ||
                        g_gesture_wakeup_mode[1] != 0x00000000) {
                        drv_open_gesture_wakeup(&g_gesture_wakeup_mode[0]);
                        enable_irq_wake(girq);
                        dev_info(&g_i2c_client->dev, "enable gesture");
                        return 0;
                    }
                }
            }
#endif /*CONFIG_ENABLE_GESTURE_WAKEUP */

            drv_finger_touch_released(0, 0, 0);    /*Send touch end for clearing point touch */
            input_sync(g_input_device);

            drv_disable_finger_touch_report();

            {
                drv_touch_device_power_off();
#ifdef CONFIG_ENABLE_REGULATOR_POWER_ON
                drv_touch_device_regulator_power_on(false);
#endif /*CONFIG_ENABLE_REGULATOR_POWER_ON */
            }
        }
    }

    return 0;
}

#else

#ifndef CONFIG_HAS_EARLYSUSPEND
void ms_drv_interface_touch_device_suspend(struct device *pDevice)
#else
void ms_drv_interface_touch_device_suspend(struct early_suspend *pSuspend)
#endif                          /*CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD */
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

#ifdef CONFIG_ENABLE_CHARGER_DETECTION
    g_is_enable_charger_plugin_out_check = 0;
    cancel_delayed_work_sync(&g_charger_plug_in_out_check_work);
#endif /*CONFIG_ENABLE_CHARGER_DETECTION */

#ifdef CONFIG_ENABLE_ESD_PROTECTION
    g_is_enable_esd_check = 0;
    cancel_delayed_work_sync(&g_esd_check_work);
#endif /*CONFIG_ENABLE_ESD_PROTECTION */

    if (g_is_update_firmware != 0) {  /*Check whether update frimware is finished */
        DBG(&g_i2c_client->dev,
            "Not allow to power on/off touch ic while update firmware.\n");
        return;
    }
#ifdef CONFIG_ENABLE_PROXIMITY_DETECTION
    if (g_enable_tp_proximity == 1) {
        DBG(&g_i2c_client->dev, "g_enable_tp_proximity = %d\n",
            g_enable_tp_proximity);
        return;
    }
#endif /*CONFIG_ENABLE_PROXIMITY_DETECTION */

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP

    {
        DBG(&g_i2c_client->dev, " g_gesture_state = %d, g_chip_type = 0x%X\n",
            g_gesture_state, g_chip_type);

        if (g_chip_type == CHIP_TYPE_ILI2120 || g_chip_type == CHIP_TYPE_ILI2121) {
            if (g_gesture_state) {
                if (g_gesture_wakeup_mode[0] != 0x00000000 ||
                    g_gesture_wakeup_mode[1] != 0x00000000) {
                    drv_open_gesture_wakeup(&g_gesture_wakeup_mode[0]);
                    enable_irq_wake(girq);
                    return;
                }
            }
        } else {
            if (g_gesture_wakeup_mode[0] != 0x00000000 ||
                g_gesture_wakeup_mode[1] != 0x00000000) {
                drv_open_gesture_wakeup(&g_gesture_wakeup_mode[0]);
                enable_irq_wake(girq);
                return;
            }
        }
    }
#endif /*CONFIG_ENABLE_GESTURE_WAKEUP */

    drv_finger_touch_released(0, 0, 0);    /*Send touch end for clearing point touch */
    input_sync(g_input_device);

    drv_disable_finger_touch_report();
    {
        drv_touch_device_power_off();
#ifdef CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD
#ifdef CONFIG_ENABLE_REGULATOR_POWER_ON
        drv_touch_device_regulator_power_on(false);
#endif /*CONFIG_ENABLE_REGULATOR_POWER_ON */
#endif /*CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD */
#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL    /*TYPE B PROTOCOL */
        input_report_key(g_input_device, BTN_TOUCH, 0);

        for (i = 0; i < max_touch_num; i++)
            drv_finger_touch_released(0, 0, i);


        input_report_key(g_input_device, BTN_TOOL_FINGER, 0);
#else /*TYPE A PROTOCOL */
        drv_finger_touch_released(0, 0, 0);
#endif /*CONFIG_ENABLE_TYPE_B_PROTOCOL */
        input_sync(g_input_device);
    }
}

#ifndef CONFIG_HAS_EARLYSUSPEND
void ms_drv_interface_touch_device_resume(struct device *pDevice)
#else
void ms_drv_interface_touch_device_resume(struct early_suspend *pSuspend)
#endif                          /*CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD */
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    if (g_is_update_firmware != 0) {  /*Check whether update frimware is finished */
        DBG(&g_i2c_client->dev,
            "Not allow to power on/off touch ic while update firmware.\n");
        return;
    }
#ifdef CONFIG_ENABLE_PROXIMITY_DETECTION
    if (g_enable_tp_proximity == 1) {
        DBG(&g_i2c_client->dev, "g_enable_tp_proximity = %d\n",
            g_enable_tp_proximity);
        return;
    }
#endif /*CONFIG_ENABLE_PROXIMITY_DETECTION */

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP

    {
#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
        if (g_gesture_debug_mode == 1)
            drv_close_gesture_debug_mode();

#endif /*CONFIG_ENABLE_GESTURE_DEBUG_MODE */

        if (g_gesture_wakeup_flag == 1) {
            DBG(&g_i2c_client->dev, "g_gesture_state = %d, g_chip_type = 0x%X\n",
                g_gesture_state, g_chip_type);

            if (g_chip_type == CHIP_TYPE_ILI2120 ||
                g_chip_type == CHIP_TYPE_ILI2121) {
                if (g_gesture_state) {
                    drv_close_gesture_wakeup();
                    disable_irq_wake(girq);
                }
            } else {
                drv_close_gesture_wakeup();
                disable_irq_wake(girq);
            }
        } else {
            drv_enable_finger_touch_report();
        }
    }

#endif /*CONFIG_ENABLE_GESTURE_WAKEUP */

    {
#ifdef CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD
#ifdef CONFIG_ENABLE_REGULATOR_POWER_ON
        drv_touch_device_regulator_power_on(true);
#endif /*CONFIG_ENABLE_REGULATOR_POWER_ON */
#endif /*CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD */
        drv_touch_device_power_on();
    }

#ifdef CONFIG_ENABLE_CHARGER_DETECTION
    {
        u8 szChargerStatus[20] = { 0 };

        drv_read_file(POWER_SUPPLY_BATTERY_STATUS_PATCH, szChargerStatus, 20);

        DBG(&g_i2c_client->dev, "*** Battery Status : %s ***\n",
            szChargerStatus);

        g_force_update = 1;     /*Set flag to force update charger status */

		if (strnstr(szChargerStatus, "Charging", 8) != NULL || strnstr(szChargerStatus, "Full", 4) != NULL
			|| strnstr(szChargerStatus, "Fully charged", 13) != NULL) { /*Charging */
            drv_charger_detection(1); /*charger plug-in */
        else                /*Not charging */

            drv_charger_detection(0); /*charger plug-out */


        g_force_update = 0;
    }
#endif /*CONFIG_ENABLE_CHARGER_DETECTION */

    if (g_is_enable_glove_mode == 1)
        drv_open_glove_mode();

    if (g_is_enable_leather_sheath_mode == 1)
        drv_open_leather_sheath_mode();

    if (is_firmware_data_log_enabled)
        drv_restore_firmware_mode_to_log_data_mode();
    /*is_firmware_data_log_enabled */
#ifndef CONFIG_ENABLE_GESTURE_WAKEUP
    drv_enable_finger_touch_report();
#endif /*CONFIG_ENABLE_GESTURE_WAKEUP */

#ifdef CONFIG_ENABLE_CHARGER_DETECTION
    g_is_enable_charger_plugin_out_check = 1;
    queue_delayed_work(g_charger_plug_in_out_check_workqueue,
                       &g_charger_plug_in_out_check_work,
                       CHARGER_DETECT_CHECK_PERIOD);
#endif /*CONFIG_ENABLE_CHARGER_DETECTION */

#ifdef CONFIG_ENABLE_ESD_PROTECTION
    g_is_enable_esd_check = 1;
    queue_delayed_work(g_esd_check_work_queue, &g_esd_check_work,
                       ESD_PROTECT_CHECK_PERIOD);
#endif /*CONFIG_ENABLE_ESD_PROTECTION */
}
#endif /*CONFIG_ENABLE_NOTIFIER_FB */

/*probe function is used for matching and initializing input device */

s32 /*__devinit*/ ms_drv_interface_touch_device_probe(struct i2c_client *pClient,
                                                 const struct i2c_device_id
                                                 *pDeviceId)
{
    s32 n_ret_val = 0;
    u16 nMajor = 0, nMinor = 0;

    DBG_INFO(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    drv_mutex_variable_initialize();

    n_ret_val = drv_touch_device_requestGPIO(pClient);
    if (n_ret_val < 0) {
		return n_ret_val;
    }

#ifdef CONFIG_ENABLE_REGULATOR_POWER_ON
    drv_touch_device_regulator_power_on(true);
#endif /*CONFIG_ENABLE_REGULATOR_POWER_ON */

    drv_touch_device_power_on();
    DBG(&g_i2c_client->dev, "***  int_status = %d ***\n",
        gpio_get_value(ms_ts_msg_ic_gpio_int));
    n_ret_val = drv_touch_device_initialize();
    if (n_ret_val == -ENODEV) {
        drv_touch_device_remove(pClient);
        return n_ret_val;
    }

    drv_input_device_initialize(pClient);

    drv_touch_device_register_finger_touch_interrupt_handler();
    DBG(&g_i2c_client->dev, "***  int_status = %d ***\n",
        gpio_get_value(ms_ts_msg_ic_gpio_int));
    drv_touch_device_register_early_suspend();

    drv_get_platform_firmware_version(&g_platform_fw_version);
    DBG(&g_i2c_client->dev, "*** %s() g_platform_fw_version = %s ***\n",
        __func__, g_platform_fw_version);
    drv_get_customer_firmware_version(&nMajor, &nMinor, &g_fw_version);
    if (g_fw_version_flag) {
        printk
            ("*** ILITEK %s() Major = %d, Minor = %d.%d, g_fw_version = %s ***\n",
             __func__, nMajor, (nMinor & 0xFF), ((nMinor >> 8) & 0xFF),
             g_fw_version);
    } else {
        printk
            ("*** ILITEK %s() Major = %d, Minor = %d, g_fw_version = %s ***\n",
             __func__, nMajor, nMinor, g_fw_version);
    }
#ifdef CONFIG_UPDATE_FIRMWARE_BY_SW_ID
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);
    drv_check_firmware_update_by_sw_id();
#endif /*CONFIG_UPDATE_FIRMWARE_BY_SW_ID */

#ifdef CONFIG_ENABLE_CHARGER_DETECTION
    INIT_DELAYED_WORK(&g_charger_plug_in_out_check_work,
                      drv_charger_plug_in_out_check);
    g_charger_plug_in_out_check_workqueue =
        create_workqueue("charger_plug_in_out_check");
    queue_delayed_work(g_charger_plug_in_out_check_workqueue,
                       &g_charger_plug_in_out_check_work,
                       CHARGER_DETECT_CHECK_PERIOD);
#endif /*CONFIG_ENABLE_CHARGER_DETECTION */

#ifdef CONFIG_ENABLE_ESD_PROTECTION
    INIT_DELAYED_WORK(&g_esd_check_work, drv_esd_check);
    g_esd_check_work_queue = create_workqueue("esd_check");
    queue_delayed_work(g_esd_check_work_queue, &g_esd_check_work,
                       ESD_PROTECT_CHECK_PERIOD);
#endif /*CONFIG_ENABLE_ESD_PROTECTION */

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
    device_init_wakeup(&g_i2c_client->dev, 1);
#endif /*CONFIG_ENABLE_GESTURE_WAKEUP */
    DBG_INFO(&g_i2c_client->dev, "*** ILITEK/MStar touch driver registered ***\n");
    return n_ret_val;
}

/* remove function is triggered when the input device is removed from input sub-system */

s32 /*__devexit*/ ms_drv_interface_touch_device_remove(struct i2c_client *pClient)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    return drv_touch_device_remove(pClient);
}

void ms_drv_interface_touch_device_setIic_data_rate(struct i2c_client *pClient,
                                             u32 nIic_dataRate)
{
    DBG(&g_i2c_client->dev, "*** %s() ***\n", __func__);

    drv_set_iic_data_rate(pClient, nIic_dataRate);
}
