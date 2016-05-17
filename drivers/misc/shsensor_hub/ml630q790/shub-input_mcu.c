/*
 *  shub-input_mcu.c - Linux kernel modules for interface of ML630Q790
 *
 *  Copyright (C) 2012-2014 LAPIS SEMICONDUCTOR CO., LTD.
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
static int shub_mcu_ped_enable_dbg = 0;
module_param(shub_mcu_ped_enable_dbg, int, 0600);
#endif
static int shub_mcu_ped_enable = 0;

static int shub_mcu_open_info = 0;

static int32_t shub_probe_mcu(struct platform_device *pfdev);
static int32_t shub_remove_mcu(struct platform_device *pfdev);

#ifdef CONFIG_OF
    static struct of_device_id shub_of_match_tb_mcu[] = {
        { .compatible = "sharp,shub_mcu" ,},
        {}
    };
#else
    #define shub_of_match_tb_mcu NULL
#endif

#ifdef CONFIG_ANDROID_ENGINEERING
static void shub_get_param_type_log( int32_t type, int32_t *dt, uint8_t *logbuff )
{
//  memset(logbuff, 0x00, sizeof(logbuff));
    switch(type) {
    case 1: // APP_PEDOMETER
//      sprintf(logbuff, "type=PED, data=%x,%x,%x,%x,%x,%x,%x,%x, %x,%x,%x,%x", *dt, *(dt+1), *(dt+2), *(dt+3), *(dt+4), *(dt+5), *(dt+6), *(dt+7), *(dt+8), *(dt+9), *(dt+10), *(dt+11));
        sprintf(logbuff, "type=PED, data=%x,%x,%x,%x,%x,%x,%x,%x, %x,%x,%x,%x,%x", *dt, *(dt+1), *(dt+2), *(dt+3), *(dt+4), *(dt+5), *(dt+6), *(dt+7), *(dt+8), *(dt+9), *(dt+10), *(dt+11), *(dt+12));
        break;
    case 2: // APP_CALORIE_FACTOR
        sprintf(logbuff, "type=CAL, data=%x,%x,%x,%x,%x,%x", *dt, *(dt+1), *(dt+2), *(dt+3), *(dt+4), *(dt+5));
        break;
    case 3: // APP_RUN_DETECTION
        sprintf(logbuff, "type=RUN, data=%x,%x,%x", *dt, *(dt+1), *(dt+2));
        break;
    case 4: // APP_VEICHLE_DETECTION
        sprintf(logbuff, "type=VEI, data=%x,%x,%x,%x,%x,%x,%x,%x, %x,%x,%x,%x", *dt, *(dt+1), *(dt+2), *(dt+3), *(dt+4), *(dt+5), *(dt+6), *(dt+7), *(dt+8), *(dt+9), *(dt+10), *(dt+11));
        break;
    case 5: // APP_TOTAL_STATUS_DETECTION
        sprintf(logbuff, "type=TOTAL, data=%x,%x,%x,%x,%x,%x,%x,%x", *dt, *(dt+1), *(dt+2), *(dt+3), *(dt+4), *(dt+5), *(dt+6), *(dt+7));
        break;
    case 6: // APP_GDETECTION
        sprintf(logbuff, "type=GDET, data=%x,%x,%x,%x,%x,%x,%x", *dt, *(dt+1), *(dt+2), *(dt+3), *(dt+4), *(dt+5), *(dt+6));
        break;
    case 7: // APP_MOTDTECTION
        sprintf(logbuff, "type=MOT, data=%x,%x,%x,%x,%x,%x,%x,%x, %x,%x", *dt, *(dt+1), *(dt+2), *(dt+3), *(dt+4), *(dt+5), *(dt+6), *(dt+7), *(dt+8), *(dt+9));
        break;
    case 8: // MCU_TASK_CYCLE
        sprintf(logbuff, "type=CYCLE, data=%x,%x,%x", *dt, *(dt+1), *(dt+2));
        break;
    case 12: // APP_LOW_POWER
        sprintf(logbuff, "type=LP, data=%x,%x,%x,%x,%x,%x", *dt, *(dt+1), *(dt+2), *(dt+3), *(dt+4), *(dt+5));
        break;
    case 14: // APP_VEICHLE_DETECTION2
        sprintf(logbuff, "type=VEI2, data=%x,%x,%x,%x,%x,%x,%x,%x,%x", *dt, *(dt+1), *(dt+2), *(dt+3), *(dt+4), *(dt+5), *(dt+6), *(dt+7), *(dt+8));
        break;
    case 19: // APP_PICKUP
        sprintf(logbuff, "type=PICKUP, data=%x", *dt);
        break;
    default:
        sprintf(logbuff, "type=%d, data=...", type);
        break;
    }
}

static int shub_mcu_log = 0;
module_param(shub_mcu_log, int, 0600);
#define DBG_MCU_IO(msg, ...) {                      \
    if(shub_mcu_log & 0x01)                         \
        printk("[shub][mcu] " msg, ##__VA_ARGS__);  \
}
#else
#define DBG_MCU_IO(msg, ...)
#endif

#define INPUT_DEV_NAME "shub_mcu"
#define INPUT_DEV_PHYS "shub_mcu/input0"
#define MISC_DEV_NAME  "shub_io_mcu"

static DEFINE_MUTEX(shub_lock);

static struct platform_device *pdev;
#if 0
static struct input_dev *shub_idev;
#endif

static long shub_ioctl(struct file *filp, unsigned int cmd, unsigned long arg, int env)
{
    void __user *argp = (void __user *)arg;
    int32_t ret = -1;
    int32_t *p;

    switch (cmd) {
        case SHUBIO_INITIALIZE : 
            {
                DBG_MCU_IO("ioctl(cmd = Initialize)\n");
                mutex_lock(&shub_lock);
                shub_set_exif_md_mode_flg(0);
                shub_initialize();
                mutex_unlock(&shub_lock);
            }
            break;
        case SHUBIO_MCU_HOSTCMD : 
            {
                IoCtlHostCmd ioc;

                mutex_lock(&shub_lock);

                ret = copy_from_user(&ioc,argp,sizeof(ioc));
                if (ret) {
                    printk( "error(copy_from_user) : shub_ioctl(cmd = SHUBIO_MCU_HOSTCMD)\n" );
                    mutex_unlock(&shub_lock);
                    return -EFAULT;
                }

                if((ioc.m_cParam == NULL) || (ioc.m_cResult == NULL)){
                    printk( "error(Input param) : shub_ioctl(cmd = SHUBIO_MCU_HOSTCMD)\n" );
                    mutex_unlock(&shub_lock);
                    return -EFAULT;
                }

                DBG_MCU_IO("ioctl(cmd = Hostcmd) : cmd=0x%04x\n", ioc.m_sCmd);
                ioc.m_sErr = shub_direct_hostcmd(ioc.m_sCmd, ioc.m_cParam, ioc.m_cResult);

                ret = copy_to_user(argp, &ioc, sizeof(ioc));
                if (ret) {
                    printk( "error(copy_to_user) : shub_ioctl(cmd = SHUBIO_MCU_HOSTCMD)\n" );
                    mutex_unlock(&shub_lock);
                    return -EFAULT;
                }

                mutex_unlock(&shub_lock);
            }
            break;
        case SHUBIO_MCU_FW_UPDATE:
            {
                IoCtlAccSetFirmwareData ioc;
                uint8_t *fw_data_page1 = NULL;
                uint8_t *fw_data_page2 = NULL;
                uint32_t allocate_size;
                uint8_t *data_addr;
                ret = copy_from_user(&ioc,argp,sizeof(ioc));
                if (ret) {
                    printk( "error(copy_from_user) : shub_ioctl(cmd = SHUBIO_MCU_FW_UPDATE)\n" );
                    return -EFAULT;
                }
                DBG_MCU_IO( "ioctl(cmd = FW_Update) : env=%d\n", env);
                if(env == 32) {
                    unsigned long data_ulong;
                    data_ulong = (unsigned long)ioc.u.m_cDataAddr;
                    data_ulong &= 0xFFFFFFFF;
                    data_addr = (uint8_t *)data_ulong;
                    if((data_addr == NULL) || (ioc.m_nLen == 0)){
                        printk( "error(Input param) : shub_ioctl(cmd = SHUBIO_MCU_FW_UPDATE) env=32\n" );
                        return -EFAULT;
                    }
                } else {
                    if((ioc.u.m_cData == NULL) || (ioc.m_nLen == 0)){
                        printk( "error(Input param) : shub_ioctl(cmd = SHUBIO_MCU_FW_UPDATE) env=64\n" );
                        return -EFAULT;
                    }
                }

                DBG_MCU_IO("ioctl(cmd = FW_Update) : size=%d\n", ioc.m_nLen);
                if(ioc.m_nLen > (64*1024)){
                    allocate_size = (64*1024);
                }else{
                    allocate_size = ioc.m_nLen;
                }
                fw_data_page1 = (uint8_t *)kmalloc( allocate_size , GFP_KERNEL );
                if(fw_data_page1 == NULL)
                {
                    printk( "error(kmalloc) : shub_ioctl(cmd = SHUBIO_MCU_FW_UPDATE)\n" );
                    return -ENOMEM;
                }
                if(env == 32) {
                    ret = copy_from_user( fw_data_page1, &data_addr[0], allocate_size );
                } else {
                    ret = copy_from_user( fw_data_page1, &ioc.u.m_cData[0], allocate_size );
                }
                if( ret != 0 )
                {
                    printk( "error(copy_from_user(data)) : shub_ioctl(cmd = SHUBIO_MCU_FW_UPDATE)\n" );
                    kfree( fw_data_page1 );
                    return -EFAULT;
                }

                if(allocate_size == (64*1024)){
                    allocate_size = ioc.m_nLen - (64*1024); 
                    fw_data_page2 = (uint8_t *)kmalloc( allocate_size , GFP_KERNEL );
                    if(fw_data_page2 == NULL)
                    {
                        printk( "error(kmalloc) : shub_ioctl(cmd = SHUBIO_MCU_FW_UPDATE)\n" );
                        kfree( fw_data_page1 );
                        return -ENOMEM;
                    }
                    if(env == 32) {
                        ret = copy_from_user( fw_data_page2, &data_addr[64*1024], allocate_size );
                    } else {
                        ret = copy_from_user( fw_data_page2, &ioc.u.m_cData[64*1024], allocate_size );
                    }
                    if( ret != 0 )
                    {
                        printk( "error(copy_from_user(data)) : shub_ioctl(cmd = SHUBIO_MCU_FW_UPDATE)\n" );
                        kfree( fw_data_page1 );
                        kfree( fw_data_page2 );
                        return -EFAULT;
                    }
                }
                shub_set_error_code(SHUB_FUP_NO_COMMAND);
                
                ret = shub_update_fw(true, fw_data_page1,fw_data_page2, ioc.m_nLen);
                kfree( fw_data_page1 );
                if(ioc.m_nLen > (64*1024)){
                    kfree( fw_data_page2 );
                }
                ioc.m_sCmdErrRes = shub_get_error_code();
//                if (ret){
                if (ioc.m_sCmdErrRes) {
                    printk( "error(shub_update_fw) : shub_ioctl(cmd = SHUBIO_MCU_FW_UPDATE)[%d] ret=%d CmdErr=%x\n", ioc.m_bBoot, ret, ioc.m_sCmdErrRes);
                    ret = copy_to_user(argp, &ioc, sizeof(ioc));
                    if(ret){
                        printk("error(copy_to_user(data)) : shub_ioctl(cmd = SHUBIO_MCU_FW_UPDATE)\n");
                    }

                    return -EFAULT;
                }
            }
            break;

        case SHUBIO_MCU_FW_GET_VERSION:
            {
                IoCtlAccSetFirmwareVersion ioc;

                memset(&ioc, 0x00, sizeof(ioc));
                if(shub_fw_update_check()){
                    ioc.m_flag = 1;
                    printk( "error(shub_get_fw_version) : FW update Error!!\n" );
                    ret = copy_to_user(argp, &ioc, sizeof(IoCtlAccSetFirmwareVersion));
                    if (ret) {
                        printk( "error(copy_from_user) : shub_ioctl(cmd = SHUBIO_MCU_FW_GET_VERSION)\n" );
                    }
                    return -EFAULT;
                }
                ret = shub_get_fw_version(ioc.m_cData);
                if(ret) {
                    printk( "error(shub_get_fw_version) : shub_ioctl(cmd = SHUBIO_MCU_FW_GET_VERSION)\n" );
                    return -EFAULT;
                }
                DBG_MCU_IO("ioctl(cmd = Get_Version) : ver=%02x,%02x,%02x,%02x\n", ioc.m_cData[0],ioc.m_cData[1],ioc.m_cData[2],ioc.m_cData[3]);
                ret = copy_to_user(argp, &ioc, sizeof(IoCtlAccSetFirmwareVersion));
                if (ret) {
                    printk( "error(copy_from_user) : shub_ioctl(cmd = SHUBIO_MCU_FW_GET_VERSION)\n" );
                    return -EFAULT;
                }
            }
            break;
        case SHUBIO_MCU_SET_PARAM  : 
            {
#ifdef CONFIG_ANDROID_ENGINEERING
                uint8_t logbuff[200];
#endif
                IoCtlParam param;
                ret = copy_from_user(&param, argp, sizeof(param));
                if (ret) {
                    printk( "error(copy_from_user) : shub_ioctl(cmd = SHUBIO_MCU_SET_PARAM)\n" );
                    return -EFAULT;
                }
#ifdef CONFIG_ANDROID_ENGINEERING
                memset(logbuff, 0x00, sizeof(logbuff));
                shub_get_param_type_log(param.m_iType, param.m_iParam, logbuff);
                DBG_MCU_IO("ioctl(cmd = Set_Param) : %s\n", logbuff);
#endif
                mutex_lock(&shub_lock);
                shub_set_exif_md_mode_flg(param.m_iParam[31]);
                ret = shub_set_param(param.m_iType, param.m_iParam);
                mutex_unlock(&shub_lock);
                if(ret) {
                    printk( "error(shub_set_param) : shub_ioctl(cmd = SHUBIO_MCU_SET_PARAM)\n" );
                    return -EFAULT;
                }
                if(param.m_iType == 1) {    // APP_PEDOMETER
                    shub_mcu_ped_enable = param.m_iParam[0];
#ifdef CONFIG_ANDROID_ENGINEERING
                    shub_mcu_ped_enable_dbg = shub_mcu_ped_enable;
#endif
                }
            }
            break;
        case SHUBIO_MCU_GET_PARAM  : 
            {
#ifdef CONFIG_ANDROID_ENGINEERING
                uint8_t logbuff[200];
#endif
                IoCtlParam param;

                ret = copy_from_user(&param, argp, sizeof(param));
                if (ret) {
                    printk("error(copy_from_user) : shub_ioctl(cmd = SHUBIO_MCU_GET_PARAM)\n");
                    return -EFAULT;
                }

                mutex_lock(&shub_lock);
                ret = shub_get_param(param.m_iType, param.m_iParam);
                shub_get_param_check_exif(param.m_iType, param.m_iParam);
                param.m_iParam[31] = shub_get_exif_md_mode_flg();
                mutex_unlock(&shub_lock);
                if(ret) {
                    printk( "error(shub_get_param) : shub_ioctl(cmd = SHUBIO_MCU_GET_PARAM)\n" );
                    return -EFAULT;
                }

#ifdef CONFIG_ANDROID_ENGINEERING
                memset(logbuff, 0x00, sizeof(logbuff));
                shub_get_param_type_log(param.m_iType, param.m_iParam, logbuff);
                DBG_MCU_IO("ioctl(cmd = Get_Param) : %s\n", logbuff);
#endif
                ret = copy_to_user(argp, &param, sizeof(IoCtlParam));
                if (ret) {
                    printk( "error(copy_from_user) : shub_ioctl(cmd = SHUBIO_MCU_GET_PARAM)\n" );
                    return -EFAULT;
                }
            }
            break;
        case SHUBIO_MCU_GET_MOTION_DATA   : 
            {
                IoCtlData data;

                mutex_lock(&shub_lock);
                ret = shub_get_data_motion(data.m_iData);
                mutex_unlock(&shub_lock);
                if(ret) {
                    printk( "error(shub_get_data_pedometer) : shub_ioctl(cmd = SHUBIO_MCU_GET_MOTION_DATA)\n" );
                    return -EFAULT;
                }

                DBG_MCU_IO("ioctl(cmd = Get_Motion_Data) : data=0x%x,0x%x,0x%x\n", data.m_iData[0],data.m_iData[1],data.m_iData[2]);
                ret = copy_to_user(argp, &data, sizeof(IoCtlData));
                if (ret) {
                    printk( "error(copy_from_user) : shub_ioctl(cmd = SHUBIO_MCU_GET_MOTION_DATA)\n" );
                    return -EFAULT;
                }
            }
            break;
        case SHUBIO_MCU_GET_PEDOM_DATA   : 
            {
                IoCtlData data;

                mutex_lock(&shub_lock);
                ret = shub_get_data_pedometer(data.m_iData);
                mutex_unlock(&shub_lock);
                if(ret) {
                    printk( "error(shub_get_data_pedometer) : shub_ioctl(cmd = SHUBIO_MCU_GET_PEDOM_DATA)\n" );
                    return -EFAULT;
                }

                p = &data.m_iData[0];
                DBG_MCU_IO("ioctl(cmd = Get_Pedom_Data) : data=%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n",
                           *p,*(p+1),*(p+2),*(p+3),*(p+4),*(p+5),*(p+6),*(p+7),*(p+8),*(p+9),*(p+10),*(p+11),*(p+12),*(p+13),*(p+14),*(p+15));
                ret = copy_to_user(argp, &data, sizeof(IoCtlData));
                if (ret) {
                    printk( "error(copy_from_user) : shub_ioctl(cmd = SHUBIO_MCU_GET_PEDOM_DATA)\n" );
                    return -EFAULT;
                }
            }
            break;

        case SHUBIO_MCU_GET_NORMAL_PEDOM_DATA   : 
            {
                IoCtlData data;

                mutex_lock(&shub_lock);
                ret = shub_get_data_normal_pedometer(data.m_iData);
                mutex_unlock(&shub_lock);
                if(ret) {
                    printk( "error(shub_get_data_normal_pedometer) : shub_ioctl(cmd = SHUBIO_MCU_GET_NORMAL_PEDOM_DATA)\n" );
                    return -EFAULT;
                }

                p = &data.m_iData[0];
                DBG_MCU_IO("ioctl(cmd = Get_Nomal_Pedom_Data) : data=%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n",
                           *p,*(p+1),*(p+2),*(p+3),*(p+4),*(p+5),*(p+6),*(p+7),*(p+8),*(p+9),*(p+10),*(p+11),*(p+12),*(p+13),*(p+14),*(p+15));
                ret = copy_to_user(argp, &data, sizeof(IoCtlData));
                if (ret) {
                    printk( "error(copy_from_user) : shub_ioctl(cmd = SHUBIO_MCU_GET_NORMAL_PEDOM_DATA)\n" );
                    return -EFAULT;
                }
            }
            break;

        case SHUBIO_MCU_CLR_DATA   : 
            {
                IoCtlParam param;
                ret = copy_from_user(&param, argp, sizeof(param));
                if (ret) {
                    printk( "error(copy_from_user) : shub_ioctl(cmd = SHUBIO_MCU_CLR_DATA)\n" );
                    return -EFAULT;
                }

                DBG_MCU_IO("ioctl(cmd = Clear_Data) : type=%d\n", param.m_iType);
                mutex_lock(&shub_lock);
                ret = shub_clear_data(param.m_iType);
                mutex_unlock(&shub_lock);
                if (ret) {
                    printk( "error(shub_clear_data) : shub_ioctl(cmd = SHUBIO_MCU_CLR_DATA)\n" );
                    return -EFAULT;
                }
            }
            break;
        case SHUBIO_MCU_GET_ADTECT_DATA  : 
            {
                IoCtlData data;

                mutex_lock(&shub_lock);
                ret = shub_get_data_act_detection(data.m_iData);
                mutex_unlock(&shub_lock);
                if(ret) {
                    printk( "error(shub_get_data_act_detection) : shub_ioctl(cmd = SHUBIO_MCU_GET_ADTECT_DATA)\n" );
                    return -EFAULT;
                }

                p = &data.m_iData[0];
                DBG_MCU_IO("ioctl(cmd = Get_Adtect_Data) : data=%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n",
                           *p,*(p+1),*(p+2),*(p+3),*(p+4),*(p+5),*(p+6),*(p+7),*(p+8),*(p+9),*(p+10),*(p+11),*(p+12),*(p+13),*(p+14),*(p+15));
                ret = copy_to_user(argp, &data, sizeof(IoCtlData));
                if (ret) {
                    printk( "error(copy_from_user) : shub_ioctl(cmd = SHUBIO_MCU_GET_ADTECT_DATA)\n" );
                    return -EFAULT;
                }
            }
            break;
        case SHUBIO_MCU_WRITE_SENSOR  : 
            {
                IoCtlAccessSensor param;

                ret = copy_from_user(&param, argp, sizeof(param));
                if (ret) {
                    printk( "error(copy_from_user) : shub_ioctl(cmd = SHUBIO_MCU_WRITE_SENSOR)\n" );
                    return -EFAULT;
                }

                DBG_MCU_IO("ioctl(cmd = Write_Sensor) : type=0x%02x, addr=0x%02x, data=0x%02x\n", param.m_sSensorType,param.m_ucAddr,param.m_ucData);
                mutex_lock(&shub_lock);
                ret = shub_write_sensor(param.m_sSensorType, param.m_ucAddr,param.m_ucData);
                mutex_unlock(&shub_lock);
                if (ret) {
                    printk( "error(shub_write_sensor) : shub_ioctl(cmd = SHUBIO_MCU_WRITE_SENSOR)\n" );
                    return -EFAULT;
                }
            }
            break;
        case SHUBIO_MCU_READ_SENSOR : 
            {
                IoCtlAccessSensor param;

                ret = copy_from_user(&param, argp, sizeof(param));
                if (ret) {
                    printk( "error(copy_from_user) : shub_ioctl(cmd = SHUBIO_MCU_READ_SENSOR)\n" );
                    return -EFAULT;
                }

                mutex_lock(&shub_lock);
                ret = shub_read_sensor(param.m_sSensorType, param.m_ucAddr,&param.m_ucData);
                mutex_unlock(&shub_lock);

                if(ret) {
                    printk( "error(shub_read_sensor) : shub_ioctl(cmd = SHUBIO_MCU_READ_SENSOR)\n" );
                    return -EFAULT;
                }

                DBG_MCU_IO("ioctl(cmd = Read_Sensor) : type=0x%02x, addr=0x%02x, data=0x%02x\n", param.m_sSensorType,param.m_ucAddr,param.m_ucData);
                ret = copy_to_user(argp, &param, sizeof(param));
                if (ret) {
                    printk( "error(copy_from_user) : shub_ioctl(cmd = SHUBIO_MCU_READ_SENSOR)\n" );
                    return -EFAULT;
                }
            }
            break;
        case SHUBIO_MCU_GET_LOW_POWER_DATA  : 
            {
                IoCtlData data;

                mutex_lock(&shub_lock);
                ret = shub_get_data_low_power(data.m_iData);
                mutex_unlock(&shub_lock);
                if(ret) {
                    printk( "error(shub_get_data_low_power) : shub_ioctl(cmd = SHUBIO_MCU_GET_LOW_POWER_DATA)\n" );
                    return -EFAULT;
                }

                p = &data.m_iData[0];
                DBG_MCU_IO("ioctl(cmd = Get_Low_Power_Data) : data=%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n",
                           *p,*(p+1),*(p+2),*(p+3),*(p+4),*(p+5),*(p+6),*(p+7),*(p+8),*(p+9),*(p+10),*(p+11),*(p+12),*(p+13),*(p+14),*(p+15));
                ret = copy_to_user(argp, &data, sizeof(IoCtlData));
                if (ret) {
                    printk( "error(copy_from_user) : shub_ioctl(cmd = SHUBIO_MCU_GET_LOW_POWER_DATA)\n" );
                    return -EFAULT;
                }
            }
            break;
        case SHUBIO_MCU_INIT_APP  : 
            {
                IoCtlParam param;

                ret = copy_from_user(&param, argp, sizeof(param));
                if (ret) {
                    printk( "error(copy_from_user) : shub_ioctl(cmd = SHUBIO_MCU_INIT_APP)\n" );
                    return -EFAULT;
                }
                p = &param.m_iParam[0];
                DBG_MCU_IO("ioctl(cmd = Get_Param) : type=%d, data=%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n",
                           param.m_iType, *p,*(p+1),*(p+2),*(p+3),*(p+4),*(p+5),*(p+6),*(p+7),*(p+8),*(p+9),*(p+10),*(p+11),*(p+12),*(p+13),*(p+14),*(p+15));
                mutex_lock(&shub_lock);
                ret = shub_init_app(param.m_iType);
                mutex_unlock(&shub_lock);
                if(ret) {
                    printk( "error(shub_init_app) : shub_ioctl(cmd = SHUBIO_MCU_INIT_APP)\n" );
                    return -EFAULT;
                }
            }
            break;
        case SHUBIO_MCU_SET_OPENINFO : 
            {
                int open_info;
                
                ret = copy_from_user(&open_info, argp, sizeof(open_info));
                if (ret) {
                    printk( "error(copy_from_user) : shub_ioctl(cmd = SHUBIO_MCU_SET_OPENINFO)\n" );
                    return -EFAULT;
                }
                
                DBG_MCU_IO("ioctl(cmd = Set_Open_Info) : info=%d -> %d\n", shub_mcu_open_info, open_info);
                shub_mcu_open_info = open_info;
            }
            break;
        case SHUBIO_MCU_GET_OPENINFO : 
            {
                int open_info;
                
                open_info = shub_mcu_open_info;
                DBG_MCU_IO("ioctl(cmd = Get_Open_Info) : info=%d\n", open_info);
                ret = copy_to_user(argp, &open_info, sizeof(open_info));
                if (ret) {
                    printk( "error(copy_from_user) : shub_ioctl(cmd = SHUBIO_MCU_GET_OPENINFO)\n" );
                    return -EFAULT;
                }
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
    ret = shub_ioctl(filp, cmd , arg, 64);
    SHUB_DBG_TIME_END(cmd)
    shub_qos_end();

    return ret;
}

#ifdef CONFIG_COMPAT
static long shub_ioctl_wrapper32(struct file *filp, unsigned int cmd, unsigned long arg)
{
    SHUB_DBG_TIME_INIT
    long ret = 0;

    shub_qos_start();
    SHUB_DBG_TIME_START
    ret = shub_ioctl(filp, cmd , arg, 32);
    SHUB_DBG_TIME_END(cmd)
    shub_qos_end();

    return ret;
}
#endif

void shub_sensor_rep_input_mcu(struct seq_file *s)
{
    seq_printf(s, "[mcu       ]shub_mcu_open_info=%d\n", shub_mcu_open_info);
}

static struct file_operations shub_fops = {
    .owner   = THIS_MODULE,
    .unlocked_ioctl = shub_ioctl_wrapper,
#ifdef CONFIG_COMPAT
//    .compat_ioctl   = shub_ioctl_wrapper,
    .compat_ioctl   = shub_ioctl_wrapper32,
#endif
};

static struct miscdevice shub_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = MISC_DEV_NAME,
    .fops  = &shub_fops,
};

static int32_t shub_probe_mcu(struct platform_device *pfdev)
{
    int32_t ret = 0;


    if(!shub_connect_check()){
        printk(KERN_INFO "shub_mcu Connect Error!!\n");
        ret = -ENODEV;
        goto out_driver;
    }


    pdev = platform_device_register_simple(INPUT_DEV_NAME, -1, NULL, 0);
    if (IS_ERR(pdev)) {
        ret = PTR_ERR(pdev);
        goto out_driver;
    }

#if 0
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


    ret = input_register_device(shub_idev);
    if (ret)
        goto out_idev;
#endif

    ret = misc_register(&shub_device);
    if (ret) {
        printk("shub-init: shub_io_device register failed\n");
        goto exit_misc_device_register_failed;
    }

    return 0;

exit_misc_device_register_failed:
#if 0
out_idev:
    input_free_device(shub_idev);
out_device:
#endif
    platform_device_unregister(pdev);
out_driver:
    return ret;
}

static int32_t shub_remove_mcu(struct platform_device *pfdev)
{
    misc_deregister(&shub_device);
#if 0
    input_unregister_device(shub_idev);
    input_free_device(shub_idev);
#endif
    platform_device_unregister(pdev);
    return 0;
}

static struct platform_driver shub_mcu_driver = {
    .probe = shub_probe_mcu,
    .remove = shub_remove_mcu,
    .shutdown = NULL,
    .driver = {
        .name = "shub_dev_mcu",
        .of_match_table = shub_of_match_tb_mcu,
    },
};

static int32_t __init shub_init(void)
{
    int ret;
    
    ret = platform_driver_register(&shub_mcu_driver);

    return ret;
}

static void __exit shub_exit(void)
{
    platform_driver_unregister(&shub_mcu_driver);
}

int shub_get_mcu_ped_enable(void)
{
    return shub_mcu_ped_enable;
}

late_initcall(shub_init);
module_exit(shub_exit);

MODULE_DESCRIPTION("SensorHub Input Device (misc)");
MODULE_AUTHOR("LAPIS SEMICOMDUCTOR");
MODULE_LICENSE("GPL v2");
