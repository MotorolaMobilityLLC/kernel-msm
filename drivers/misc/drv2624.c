/*
** =============================================================================
** Copyright (c) 2016  Texas Instruments Inc.
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** File:
**     drv2624.c
**
** Description:
**     DRV2624 chip driver
**
** =============================================================================
*/
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/semaphore.h>
#include <linux/device.h>
#include <linux/syscalls.h>
#include <asm/uaccess.h>
#include <linux/gpio.h>
#include <linux/sched.h>
#include <linux/spinlock_types.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/firmware.h>
#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <linux/drv2624.h>

/* #define	AUTOCALIBRATION_ENABLE */

static struct drv2624_data *g_DRV2624data = NULL;

static int drv2624_reg_read(struct drv2624_data *pDrv2624data, unsigned char reg)
{
	unsigned int val;
	int ret;
	
	ret = regmap_read(pDrv2624data->mpRegmap, reg, &val);
    
	if (ret < 0){
		dev_err(pDrv2624data->dev, 
			"%s reg=0x%x error %d\n", __FUNCTION__, reg, ret);
		return ret;
	}
	else
		return val;
}

static int drv2624_reg_write(struct drv2624_data *pDrv2624data, 
	unsigned char reg, unsigned char val)
{
	int ret;
	
	ret = regmap_write(pDrv2624data->mpRegmap, reg, val);
	if (ret < 0){
		dev_err(pDrv2624data->dev, 
			"%s reg=0x%x, value=0%x error %d\n", 
			__FUNCTION__, reg, val, ret);
	}
	
	return ret;
}

static int drv2624_bulk_read(struct drv2624_data *pDrv2624data, 
	unsigned char reg, unsigned int count, u8 *buf)
{
	int ret;
	ret = regmap_bulk_read(pDrv2624data->mpRegmap, reg, buf, count);
	if (ret < 0){
		dev_err(pDrv2624data->dev, 
			"%s reg=0%x, count=%d error %d\n", 
			__FUNCTION__, reg, count, ret);
	}
	
	return ret;
}

static int drv2624_bulk_write(struct drv2624_data *pDrv2624data, 
	unsigned char reg, unsigned int count, const u8 *buf)
{
	int ret;
	ret = regmap_bulk_write(pDrv2624data->mpRegmap, reg, buf, count);
	if (ret < 0){
		dev_err(pDrv2624data->dev, 
			"%s reg=0%x, count=%d error %d\n", 
			__FUNCTION__, reg, count, ret);
	}
	
	return ret;	
}

static int drv2624_set_bits(struct drv2624_data *pDrv2624data, 
	unsigned char reg, unsigned char mask, unsigned char val)
{
	int ret;
	ret = regmap_update_bits(pDrv2624data->mpRegmap, reg, mask, val);
	if (ret < 0){
		dev_err(pDrv2624data->dev, 
			"%s reg=%x, mask=0x%x, value=0x%x error %d\n", 
			__FUNCTION__, reg, mask, val, ret);
	}
	
	return ret;	
}

static int drv2624_set_go_bit(struct drv2624_data *pDrv2624data, unsigned char val)
{
	return drv2624_reg_write(pDrv2624data, DRV2624_REG_GO, (val&0x01));
}

static void drv2624_change_mode(struct drv2624_data *pDrv2624data, unsigned char work_mode)
{
	drv2624_set_bits(pDrv2624data, DRV2624_REG_MODE, MODE_MASK , work_mode);
}

static int vibrator_get_time(struct timed_output_dev *dev)
{
	struct drv2624_data *pDrv2624data = container_of(dev, struct drv2624_data, to_dev);

    if (hrtimer_active(&pDrv2624data->timer)) {
        ktime_t r = hrtimer_get_remaining(&pDrv2624data->timer);
        return ktime_to_ms(r);
    }

    return 0;
}

static void drv2624_stop(struct drv2624_data *pDrv2624data)
{
	if(pDrv2624data->mnVibratorPlaying == YES){
		hrtimer_cancel(&pDrv2624data->timer);	
		drv2624_set_go_bit(pDrv2624data, STOP);
		pDrv2624data->mnVibratorPlaying = NO;
		wake_unlock(&pDrv2624data->wklock);
	}
}

static void vibrator_enable( struct timed_output_dev *dev, int value)
{
	struct drv2624_data *pDrv2624data = 
		container_of(dev, struct drv2624_data, to_dev);
	
    mutex_lock(&pDrv2624data->lock);
	
	pDrv2624data->mnWorkMode = WORK_IDLE;
	drv2624_stop(pDrv2624data);

    if (value > 0) {
		wake_lock(&pDrv2624data->wklock);
	
		drv2624_change_mode(pDrv2624data, MODE_RTP);
		pDrv2624data->mnVibratorPlaying = YES;
		drv2624_set_go_bit(pDrv2624data, GO);
		value = (value>MAX_TIMEOUT)?MAX_TIMEOUT:value;
        hrtimer_start(&pDrv2624data->timer, 
			ns_to_ktime((u64)value * NSEC_PER_MSEC), HRTIMER_MODE_REL);
    }
	
	mutex_unlock(&pDrv2624data->lock);
}

static enum hrtimer_restart vibrator_timer_func(struct hrtimer *timer)
{
	struct drv2624_data *pDrv2624data = 
		container_of(timer, struct drv2624_data, timer);

	pDrv2624data->mnWorkMode |= WORK_VIBRATOR;
    schedule_work(&pDrv2624data->vibrator_work);
	
    return HRTIMER_NORESTART;
}

static void vibrator_work_routine(struct work_struct *work)
{
	struct drv2624_data *pDrv2624data = 
		container_of(work, struct drv2624_data, vibrator_work);
	unsigned char mode;

	mutex_lock(&pDrv2624data->lock);
		
	if(pDrv2624data->mnWorkMode & WORK_IRQ){		
		unsigned char status = pDrv2624data->mnIntStatus;	
		if(status & OVERCURRENT_MASK){
			dev_err(pDrv2624data->dev, 
				"ERROR, Over Current detected!!\n");
		}
		
		if(status & OVERTEMPRATURE_MASK){
			dev_err(pDrv2624data->dev, 
				"ERROR, Over Temperature detected!!\n");
		}
		
		if(status & ULVO_MASK){
			dev_err(pDrv2624data->dev, 
				"ERROR, VDD drop observed!!\n");
		}

		if(status & PRG_ERR_MASK){
			dev_err(pDrv2624data->dev, 
				"ERROR, PRG error!!\n");			
		}

		if(status & PROCESS_DONE_MASK){
			mode = drv2624_reg_read(pDrv2624data, DRV2624_REG_MODE) & MODE_MASK;
			if(mode == MODE_CALIBRATION){
				if((status&DIAG_MASK) != DIAG_SUCCESS){
					dev_err(pDrv2624data->dev, "Calibration fail\n");
				}else{
					unsigned char calComp = 
						drv2624_reg_read(pDrv2624data, DRV2624_REG_CAL_COMP);
					unsigned char calBemf = 
						drv2624_reg_read(pDrv2624data, DRV2624_REG_CAL_BEMF);
					unsigned char calBemfGain = 
						drv2624_reg_read(pDrv2624data, DRV2624_REG_CAL_COMP) & BEMFGAIN_MASK;
					dev_info(pDrv2624data->dev, 
						"AutoCal : Comp=0x%x, Bemf=0x%x, Gain=0x%x\n",
						calComp, calBemf, calBemfGain);
				}	
			}else if(mode == MODE_DIAGNOSTIC){
				if((status&DIAG_MASK) != DIAG_SUCCESS){
					dev_err(pDrv2624data->dev, "Diagnostic fail\n");
				}else{
					unsigned char diagZ = drv2624_reg_read(pDrv2624data, DRV2624_REG_DIAG_Z);
					unsigned char diagK = drv2624_reg_read(pDrv2624data, DRV2624_REG_DIAG_K);
					dev_info(pDrv2624data->dev, 
						"Diag : ZResult=0x%x, CurrentK=0x%x\n",
						diagZ, diagK);
				}						
			}else if(mode == MODE_WAVEFORM_SEQUENCER){
				dev_info(pDrv2624data->dev, 
					"Waveform Sequencer Playback finished\n");
			}
			
			if(pDrv2624data->mnVibratorPlaying == YES){
				pDrv2624data->mnVibratorPlaying = NO;
				wake_unlock(&pDrv2624data->wklock);
			}
		}
		
		pDrv2624data->mnWorkMode &= ~WORK_IRQ;
	}
	
	if(pDrv2624data->mnWorkMode & WORK_VIBRATOR){
		drv2624_stop(pDrv2624data);
	
		pDrv2624data->mnWorkMode &= ~WORK_VIBRATOR;
	}
	
	mutex_unlock(&pDrv2624data->lock);
}

static int fw_chksum(const struct firmware *fw){
	int sum = 0;
	int i=0;
	int size = fw->size;
	const unsigned char *pBuf = fw->data;
	
	for (i=0; i< size; i++){
		if((i>11) && (i<16)){
		
		}else{
			sum += pBuf[i];
		}
	}

	return sum;
}

static void drv2624_firmware_load(const struct firmware *fw, void *context)
{
	struct drv2624_data *pDrv2624data = context;
	int size = 0, fwsize = 0, i=0;
	const unsigned char *pBuf = NULL;
	
	if(fw != NULL){
		pBuf = fw->data;
		size = fw->size;
	
		memcpy(&(pDrv2624data->msFwHeader), pBuf, sizeof(struct drv2624_fw_header));
		if((pDrv2624data->msFwHeader.fw_magic != DRV2624_MAGIC) 
			||(pDrv2624data->msFwHeader.fw_size != size)
			||(pDrv2624data->msFwHeader.fw_chksum != fw_chksum(fw))){
			dev_err(pDrv2624data->dev,
				"%s, ERROR!! firmware not right:Magic=0x%x,Size=%d,chksum=0x%x\n", 
				__FUNCTION__, pDrv2624data->msFwHeader.fw_magic, 
				pDrv2624data->msFwHeader.fw_size, pDrv2624data->msFwHeader.fw_chksum);
		}else{
			dev_err(pDrv2624data->dev,
				"%s, firmware good\n", __FUNCTION__);

			pBuf += sizeof(struct drv2624_fw_header);
			
			drv2624_reg_write(pDrv2624data, DRV2624_REG_RAM_ADDR_UPPER, 0);
			drv2624_reg_write(pDrv2624data, DRV2624_REG_RAM_ADDR_LOWER, 0);
			
			fwsize = size - sizeof(struct drv2624_fw_header);
			for(i = 0; i < fwsize; i++){
				drv2624_reg_write(pDrv2624data, DRV2624_REG_RAM_DATA, pBuf[i]);
			}			
		}	
	}else{
		dev_err(pDrv2624data->dev,
			"%s, ERROR!! firmware not found\n", __FUNCTION__);
	}
}

static void HapticsFirmwareLoad(const struct firmware *fw, void *context)
{
	struct drv2624_data *pDrv2624data = context;

	mutex_lock(&pDrv2624data->lock);
	
	drv2624_firmware_load(fw, context);
	release_firmware(fw);
	
	mutex_unlock(&pDrv2624data->lock);
}

static int drv2624_set_seq_loop(struct drv2624_data *pDrv2624data, unsigned long arg)
{
	int ret = 0, i;
	struct drv2624_seq_loop seqLoop;
	unsigned char halfSize = DRV2624_SEQUENCER_SIZE / 2;
	unsigned char loop[2] = {0, 0};
	
	if (copy_from_user(&seqLoop, 
		(void __user *)arg, sizeof(struct drv2624_seq_loop)))
		return -EFAULT;
	
	for( i=0; i < DRV2624_SEQUENCER_SIZE; i++){
		if(i < halfSize){
			loop[0] |= (seqLoop.mpLoop[i] << (i*2));
		}else{
			loop[1] |= (seqLoop.mpLoop[i] << ((i-halfSize)*2));
		}
	}
	
	ret = drv2624_bulk_write(pDrv2624data, DRV2624_REG_SEQ_LOOP_1, 2, loop);
		
	return ret;	
}

static int drv2624_set_main(struct drv2624_data *pDrv2624data, unsigned long arg)
{
	int ret = 0;
	struct drv2624_wave_setting mainSetting;
	unsigned char control = 0;
	
	if (copy_from_user(&mainSetting, 
		(void __user *)arg, sizeof(struct drv2624_wave_setting)))
		return -EFAULT;
	
	control |= mainSetting.meScale;
	control |= (mainSetting.meInterval << INTERVAL_SHIFT);
	drv2624_set_bits(pDrv2624data, 
		DRV2624_REG_CONTROL2,
		SCALE_MASK | INTERVAL_MASK,
		control);
	
	drv2624_set_bits(pDrv2624data, 
		DRV2624_REG_MAIN_LOOP,
		0x07, mainSetting.meLoop);
		
	return ret;	
}

static int drv2624_set_wave_seq(struct drv2624_data *pDrv2624data, unsigned long arg)
{
	int ret = 0;
	struct drv2624_wave_seq waveSeq;
	
	if (copy_from_user(&waveSeq, 
		(void __user *)arg, sizeof(struct drv2624_wave_seq)))
		return -EFAULT;
	
	ret = drv2624_bulk_write(pDrv2624data, 
		DRV2624_REG_SEQUENCER_1, DRV2624_SEQUENCER_SIZE, waveSeq.mpWaveIndex);
		
	return ret;	
}

static int drv2624_get_diag_result(struct drv2624_data *pDrv2624data, unsigned long arg)
{
	int ret = 0;
	struct drv2624_diag_result diagResult;
	unsigned char mode, go;

	memset(&diagResult, 0, sizeof(struct drv2624_diag_result));
	
	mode = drv2624_reg_read(pDrv2624data, DRV2624_REG_MODE) & MODE_MASK;
	if(mode != MODE_DIAGNOSTIC){
		diagResult.mnFinished = -EFAULT;
		return ret;
	}
	
	go = drv2624_reg_read(pDrv2624data, DRV2624_REG_GO) & 0x01;
	if(go){
		diagResult.mnFinished = NO;
	}else{
		diagResult.mnFinished = YES;
		diagResult.mnResult = 
			((pDrv2624data->mnIntStatus & DIAG_MASK) >> DIAG_SHIFT);
		diagResult.mnDiagZ = drv2624_reg_read(pDrv2624data, DRV2624_REG_DIAG_Z);
		diagResult.mnDiagK= drv2624_reg_read(pDrv2624data, DRV2624_REG_DIAG_K);	
	}
	
	if (copy_to_user((void __user *)arg, &diagResult, sizeof(struct drv2624_diag_result)))
		return -EFAULT;
		
	return ret;	
}

static int drv2624_get_autocal_result(struct drv2624_data *pDrv2624data, unsigned long arg)
{
	int ret = 0;
	struct drv2624_autocal_result autocalResult;
	unsigned char mode, go;

	memset(&autocalResult, 0, sizeof(struct drv2624_autocal_result));
	
	mode = drv2624_reg_read(pDrv2624data, DRV2624_REG_MODE) & MODE_MASK;
	if(mode != MODE_CALIBRATION){
		autocalResult.mnFinished = -EFAULT;
		return ret;
	}
	
	go = drv2624_reg_read(pDrv2624data, DRV2624_REG_GO) & 0x01;
	if(go){
		autocalResult.mnFinished = NO;
	}else{
		autocalResult.mnFinished = YES;
		autocalResult.mnResult = 
			((pDrv2624data->mnIntStatus & DIAG_MASK) >> DIAG_SHIFT);
		autocalResult.mnCalComp = drv2624_reg_read(pDrv2624data, DRV2624_REG_CAL_COMP);
		autocalResult.mnCalBemf = drv2624_reg_read(pDrv2624data, DRV2624_REG_CAL_BEMF);
		autocalResult.mnCalGain = 
			drv2624_reg_read(pDrv2624data, DRV2624_REG_CAL_COMP) & BEMFGAIN_MASK;
	}
	
	if (copy_to_user((void __user *)arg, &autocalResult, sizeof(struct drv2624_autocal_result)))
		return -EFAULT;
		
	return ret;	
}

static int drv2624_file_open(struct inode *inode, struct file *file)
{
	if (!try_module_get(THIS_MODULE)) return -ENODEV;

	file->private_data = (void*)g_DRV2624data;
	return 0;
}

static int drv2624_file_release(struct inode *inode, struct file *file)
{
	file->private_data = (void*)NULL;
	module_put(THIS_MODULE);

	return 0;
}

static long drv2624_file_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct drv2624_data *pDrv2624data = file->private_data;
	//void __user *user_arg = (void __user *)arg;
	int ret = 0;
	
	mutex_lock(&pDrv2624data->lock);
	
	switch (cmd) {
		case DRV2624_SET_SEQ_LOOP:
			ret = drv2624_set_seq_loop(pDrv2624data, arg);
		break;
	
		case DRV2624_SET_MAIN:
			ret = drv2624_set_main(pDrv2624data, arg);
		break;
		
		case DRV2624_SET_WAV_SEQ:
			ret = drv2624_set_wave_seq(pDrv2624data, arg);
		break;
		
		case DRV2624_WAVSEQ_PLAY:
		{	
			drv2624_stop(pDrv2624data);	
			
			wake_lock(&pDrv2624data->wklock);
			pDrv2624data->mnVibratorPlaying = YES;
			drv2624_change_mode(pDrv2624data, MODE_WAVEFORM_SEQUENCER);	
			drv2624_set_go_bit(pDrv2624data, GO);			
		}
		break;
		
		case DRV2624_STOP:
		{
			drv2624_stop(pDrv2624data);		
		}
		break;
		
		case DRV2624_RUN_DIAGNOSTIC:
		{
			drv2624_stop(pDrv2624data);	
			
			wake_lock(&pDrv2624data->wklock);
			pDrv2624data->mnVibratorPlaying = YES;
			drv2624_change_mode(pDrv2624data, MODE_DIAGNOSTIC);			
			drv2624_set_go_bit(pDrv2624data, GO);			
		}
		break;		
		
		case DRV2624_GET_DIAGRESULT:
			ret = drv2624_get_diag_result(pDrv2624data, arg);
		break;	
		
		case DRV2624_RUN_AUTOCAL:
		{
			drv2624_stop(pDrv2624data);	
			
			wake_lock(&pDrv2624data->wklock);
			pDrv2624data->mnVibratorPlaying = YES;
			drv2624_change_mode(pDrv2624data, MODE_CALIBRATION);			
			drv2624_set_go_bit(pDrv2624data, GO);			
		}
		break;		
		
		case DRV2624_GET_CALRESULT:
			ret = drv2624_get_autocal_result(pDrv2624data, arg);
		break;			
	}
	
	mutex_unlock(&pDrv2624data->lock);
	
	return ret;
}

static ssize_t drv2624_file_read(struct file* filp, char* buff, size_t length, loff_t* offset)
{
	struct drv2624_data *pDrv2624data = (struct drv2624_data *)filp->private_data;
	int ret = 0;
	unsigned char value = 0;
	unsigned char *p_kBuf = NULL;

	mutex_lock(&pDrv2624data->lock);

	switch(pDrv2624data->mnFileCmd)
	{
		case HAPTIC_CMDID_REG_READ:
			if(length == 1){
				ret = drv2624_reg_read(pDrv2624data, pDrv2624data->mnCurrentReg);
				if( 0 > ret) {
					dev_err(pDrv2624data->dev, "dev read fail %d\n", ret);
					return ret;
				}
				value = ret;
				
				ret = copy_to_user(buff, &value, 1);
				if (0 != ret) {
					/* Failed to copy all the data, exit */
					dev_err(pDrv2624data->dev, "copy to user fail %d\n", ret);
					return 0;
				}	
			}else if(length > 1){
				p_kBuf = (unsigned char *)kzalloc(length, GFP_KERNEL);
				if(p_kBuf != NULL){
					ret = drv2624_bulk_read(pDrv2624data, 
						pDrv2624data->mnCurrentReg, length, p_kBuf);
					if( 0 > ret) {
						dev_err(pDrv2624data->dev, "dev bulk read fail %d\n", ret);
					}else{
						ret = copy_to_user(buff, p_kBuf, length);
						if (0 != ret) {
							dev_err(pDrv2624data->dev, "copy to user fail %d\n", ret);
						}
					}
					
					kfree(p_kBuf);
				}else{
					dev_err(pDrv2624data->dev, "read no mem\n");
					return -ENOMEM;
				}
			}
		break;
		
		case HAPTIC_CMDID_READ_FIRMWARE:
		{
			int i;
			p_kBuf = (unsigned char *)kzalloc(length, GFP_KERNEL);
			if(p_kBuf != NULL){
				drv2624_reg_write(pDrv2624data, DRV2624_REG_RAM_ADDR_UPPER, pDrv2624data->mnFwAddUpper);
				drv2624_reg_write(pDrv2624data, DRV2624_REG_RAM_ADDR_LOWER, pDrv2624data->mnFwAddLower);

				for(i=0; i < length; i++){
					p_kBuf[i] = drv2624_reg_read(pDrv2624data, DRV2624_REG_RAM_DATA);
				}

				ret = copy_to_user(buff, p_kBuf, length);
				if (0 != ret) {
					/* Failed to copy all the data, exit */
					dev_err(pDrv2624data->dev, "copy to user fail %d\n", ret);
				}
					
				kfree(p_kBuf);
			}else{
				dev_err(pDrv2624data->dev, "read no mem\n");
				return -ENOMEM;
			}
		}
		break;
		
		default:
			pDrv2624data->mnFileCmd = 0;
		break;
	}
	
	mutex_unlock(&pDrv2624data->lock);
	
    return length;
}

static ssize_t drv2624_file_write(struct file* filp, const char* buff, size_t len, loff_t* off)
{
	struct drv2624_data *pDrv2624data = 
		(struct drv2624_data *)filp->private_data;
	
    mutex_lock(&pDrv2624data->lock);
	
	pDrv2624data->mnFileCmd = buff[0];
	
    switch(pDrv2624data->mnFileCmd)
    {
		case HAPTIC_CMDID_REG_READ:
		{
			if(len == 2){
				pDrv2624data->mnCurrentReg = buff[1];
			}else{
				dev_err(pDrv2624data->dev, 
					" read cmd len %zu err\n", len);
			}
			break;
		}
		
		case HAPTIC_CMDID_REG_WRITE:
		{
			if((len-1) == 2){
				drv2624_reg_write(pDrv2624data, buff[1], buff[2]);	
			}else if((len-1)>2){
				unsigned char *data = (unsigned char *)kzalloc(len-2, GFP_KERNEL);
				if(data != NULL){
					if(copy_from_user(data, &buff[2], len-2) != 0){
						dev_err(pDrv2624data->dev, 
							"%s, reg copy err\n", __FUNCTION__);	
					}else{
						drv2624_bulk_write(pDrv2624data, buff[1], len-2, data);
					}
					kfree(data);
				}else{
					dev_err(pDrv2624data->dev, "memory fail\n");	
				}
			}else{
				dev_err(pDrv2624data->dev, 
					"%s, reg_write len %zu error\n", __FUNCTION__, len);
			}
			break;
		}
		
		case HAPTIC_CMDID_REG_SETBIT:
		{
			int i=1;			
			for(i=1; i< len; ){
				drv2624_set_bits(pDrv2624data, buff[i], buff[i+1], buff[i+2]);
				i += 3;
			}
			break;
		}	

		case HAPTIC_CMDID_UPDATE_FIRMWARE:
		{
			struct firmware fw;
			unsigned char *fw_buffer = (unsigned char *)kzalloc(len-1, GFP_KERNEL);
			int result = -1;
						
			drv2624_stop(pDrv2624data);	
			
			if(fw_buffer != NULL){	
				fw.size = len-1;
			
				wake_lock(&pDrv2624data->wklock);
				result = copy_from_user(fw_buffer, &buff[1], fw.size);
				if(result == 0){
					dev_info(pDrv2624data->dev, 
						"%s, fwsize=%zu, f:%x, l:%x\n", 
						__FUNCTION__, fw.size, buff[1], buff[len-1]);
					fw.data = (const unsigned char *)fw_buffer;
					drv2624_firmware_load(&fw, (void *)pDrv2624data);	
				}
				wake_unlock(&pDrv2624data->wklock);
				
				kfree(fw_buffer);
			}
			break;
		}

		case HAPTIC_CMDID_READ_FIRMWARE:
		{
			if(len == 3){
				pDrv2624data->mnFwAddUpper = buff[2];
				pDrv2624data->mnFwAddLower = buff[1];
			}else{
				dev_err(pDrv2624data->dev, 
					"%s, read fw len error\n", __FUNCTION__);
			}
			break;
		}
		
		default:
			dev_err(pDrv2624data->dev, 
				"%s, unknown cmd\n", __FUNCTION__);
		break;
    }

    mutex_unlock(&pDrv2624data->lock);

    return len;
}

static struct file_operations fops =
{
	.owner = THIS_MODULE,
	.read = drv2624_file_read,
	.write = drv2624_file_write,
	.unlocked_ioctl = drv2624_file_unlocked_ioctl,
	.open = drv2624_file_open,
	.release = drv2624_file_release,
};

static struct miscdevice drv2624_misc =
{
	.minor = MISC_DYNAMIC_MINOR,
	.name = HAPTICS_DEVICE_NAME,
	.fops = &fops,
};
 
static int Haptics_init(struct drv2624_data *pDrv2624data)
{
    int ret = 0;
	
	pDrv2624data->to_dev.name = "vibrator";
	pDrv2624data->to_dev.get_time = vibrator_get_time;
	pDrv2624data->to_dev.enable = vibrator_enable;

	ret = timed_output_dev_register(&(pDrv2624data->to_dev));
    if ( ret < 0){
        dev_err(pDrv2624data->dev, 
			"drv2624: fail to create timed output dev\n");
        return ret;
    }
	
  	ret = misc_register(&drv2624_misc);
	if (ret) {
		dev_err(pDrv2624data->dev, 
			"drv2624 misc fail: %d\n", ret);
		return ret;
	}
	
    hrtimer_init(&pDrv2624data->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    pDrv2624data->timer.function = vibrator_timer_func;
    INIT_WORK(&pDrv2624data->vibrator_work, vibrator_work_routine);
    
    wake_lock_init(&pDrv2624data->wklock, WAKE_LOCK_SUSPEND, "vibrator");
    mutex_init(&pDrv2624data->lock);
	
    return 0;
}

static void dev_init_platform_data(struct drv2624_data *pDrv2624data)
{
	struct drv2624_platform_data *pDrv2624Platdata = &pDrv2624data->msPlatData;
	struct actuator_data actuator = pDrv2624Platdata->msActuator;
	unsigned char value_temp = 0;
	unsigned char mask_temp = 0;

	drv2624_set_bits(pDrv2624data, 
		DRV2624_REG_INT_ENABLE, INT_MASK_ALL, INT_ENABLE_ALL);
		
	drv2624_set_bits(pDrv2624data, 
		DRV2624_REG_MODE, PINFUNC_MASK, (PINFUNC_INT<<PINFUNC_SHIFT));
	
	if((actuator.meActuatorType == ERM)||
		(actuator.meActuatorType == LRA)){
		mask_temp |= ACTUATOR_MASK;
		value_temp |= (actuator.meActuatorType << ACTUATOR_SHIFT);
	}
	
	if((pDrv2624Platdata->meLoop == CLOSE_LOOP)||
		(pDrv2624Platdata->meLoop == OPEN_LOOP)){
		mask_temp |= LOOP_MASK;
		value_temp |= (pDrv2624Platdata->meLoop << LOOP_SHIFT);
	}
	
	if(value_temp != 0){
		drv2624_set_bits(pDrv2624data, 
			DRV2624_REG_CONTROL1, 
			mask_temp|AUTOBRK_OK_MASK, value_temp|AUTOBRK_OK_ENABLE);
	}
	
	if(actuator.mnRatedVoltage != 0){
		drv2624_reg_write(pDrv2624data, 
			DRV2624_REG_RATED_VOLTAGE, actuator.mnRatedVoltage);
	}else{
		dev_err(pDrv2624data->dev, 
			"%s, ERROR Rated ZERO\n", __FUNCTION__);
	}

	if(actuator.mnOverDriveClampVoltage != 0){
		drv2624_reg_write(pDrv2624data, 
			DRV2624_REG_OVERDRIVE_CLAMP, actuator.mnOverDriveClampVoltage);
	}else{
		dev_err(pDrv2624data->dev,
			"%s, ERROR OverDriveVol ZERO\n", __FUNCTION__);
	}
		
	if(actuator.meActuatorType == LRA){
		unsigned char DriveTime = 5*(1000 - actuator.mnLRAFreq)/actuator.mnLRAFreq;
		unsigned short openLoopPeriod = 
			(unsigned short)((unsigned int)1000000000 / (24619 * actuator.mnLRAFreq)); 
			
		if(actuator.mnLRAFreq < 125) 
			DriveTime |= (MINFREQ_SEL_45HZ << MINFREQ_SEL_SHIFT);
		drv2624_set_bits(pDrv2624data, 
			DRV2624_REG_DRIVE_TIME, 
			DRIVE_TIME_MASK | MINFREQ_SEL_MASK, DriveTime);	
		drv2624_set_bits(pDrv2624data, 
			DRV2624_REG_OL_PERIOD_H, 0x03, (openLoopPeriod&0x0300)>>8);			
		drv2624_reg_write(pDrv2624data, 
			DRV2624_REG_OL_PERIOD_L, (openLoopPeriod&0x00ff));
		
		dev_info(pDrv2624data->dev,
			"%s, LRA = %d, DriveTime=0x%x\n", 
			__FUNCTION__, actuator.mnLRAFreq, DriveTime);
	}	
}

static irqreturn_t drv2624_irq_handler(int irq, void *dev_id)
{
	struct drv2624_data *pDrv2624data = (struct drv2624_data *)dev_id;

	pDrv2624data->mnIntStatus = 
		drv2624_reg_read(pDrv2624data,DRV2624_REG_STATUS);
	if(pDrv2624data->mnIntStatus & INT_MASK){
		pDrv2624data->mnWorkMode |= WORK_IRQ;
		schedule_work(&pDrv2624data->vibrator_work);
	}
	return IRQ_HANDLED;
}

#ifdef AUTOCALIBRATION_ENABLE	
static int dev_auto_calibrate(struct drv2624_data *pDrv2624data)
{
	wake_lock(&pDrv2624data->wklock);
	pDrv2624data->mnVibratorPlaying = YES;
	drv2624_change_mode(pDrv2624data, MODE_CALIBRATION);
	drv2624_set_go_bit(pDrv2624data, GO);

	return 0;
}
#endif

static struct regmap_config drv2624_i2c_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.cache_type = REGCACHE_NONE,
};

static int drv2624_probe(struct i2c_client* client, const struct i2c_device_id* id)
{
	struct drv2624_data *pDrv2624data;
	struct drv2624_platform_data *pDrv2624Platdata = client->dev.platform_data;
	int err = 0;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
	{
		dev_err(&client->dev, "%s:I2C check failed\n", __FUNCTION__);
		return -ENODEV;
	}

	pDrv2624data = devm_kzalloc(&client->dev, sizeof(struct drv2624_data), GFP_KERNEL);
	if (pDrv2624data == NULL){
		dev_err(&client->dev, "%s:no memory\n", __FUNCTION__);
		return -ENOMEM;
	}

	pDrv2624data->dev = &client->dev;
	pDrv2624data->mpRegmap = devm_regmap_init_i2c(client, &drv2624_i2c_regmap);
	if (IS_ERR(pDrv2624data->mpRegmap)) {
		err = PTR_ERR(pDrv2624data->mpRegmap);
		dev_err(pDrv2624data->dev, 
			"%s:Failed to allocate register map: %d\n",__FUNCTION__,err);
		return err;
	}

	memcpy(&pDrv2624data->msPlatData, pDrv2624Platdata, sizeof(struct drv2624_platform_data));
	i2c_set_clientdata(client,pDrv2624data);

	if(pDrv2624data->msPlatData.mnGpioNRST){
		err = gpio_request(pDrv2624data->msPlatData.mnGpioNRST,HAPTICS_DEVICE_NAME"NRST");
		if(err < 0){
			dev_err(pDrv2624data->dev, 
				"%s: GPIO %d request NRST error\n", 
				__FUNCTION__, pDrv2624data->msPlatData.mnGpioNRST);				
			return err;
		}
		
		gpio_direction_output(pDrv2624data->msPlatData.mnGpioNRST, 0);
		udelay(1000);
		gpio_direction_output(pDrv2624data->msPlatData.mnGpioNRST, 1);
		udelay(500);
	}

	err = drv2624_reg_read(pDrv2624data, DRV2624_REG_ID);
	if(err < 0){
		dev_err(pDrv2624data->dev, 
			"%s, i2c bus fail (%d)\n", __FUNCTION__, err);
		goto exit_gpio_request_failed;
	}else{
		dev_info(pDrv2624data->dev, 
			"%s, ID status (0x%x)\n", __FUNCTION__, err);
		pDrv2624data->mnDeviceID = err;
	}
	
	if(pDrv2624data->mnDeviceID != DRV2624_ID){
		dev_err(pDrv2624data->dev, 
			"%s, device_id(%d) fail\n",
			__FUNCTION__, pDrv2624data->mnDeviceID);
		goto exit_gpio_request_failed;
	}
	
	dev_init_platform_data(pDrv2624data);

	if(pDrv2624data->msPlatData.mnGpioINT){
		err = gpio_request(pDrv2624data->msPlatData.mnGpioINT,HAPTICS_DEVICE_NAME"INT");
		if(err < 0){
			dev_err(pDrv2624data->dev, 
				"%s: GPIO %d request INT error\n", 
				__FUNCTION__, pDrv2624data->msPlatData.mnGpioINT);					
			goto exit_gpio_request_failed;
		}

		gpio_direction_input(pDrv2624data->msPlatData.mnGpioINT);

		err = request_threaded_irq(client->irq, drv2624_irq_handler,
				NULL, IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				client->name, pDrv2624data);

		if (err < 0) {
			dev_err(pDrv2624data->dev, 
				"%s: request_irq failed\n", __FUNCTION__);							
			goto exit_gpio_request_failed;
		}
	}

	g_DRV2624data = pDrv2624data;
	
    Haptics_init(pDrv2624data);

	err = request_firmware_nowait(THIS_MODULE, 
		FW_ACTION_HOTPLUG,	"drv2624.bin",	&(client->dev), 
		GFP_KERNEL, pDrv2624data, HapticsFirmwareLoad);	
									
#ifdef AUTOCALIBRATION_ENABLE	
	err = dev_auto_calibrate(pDrv2624data);
	if(err < 0){
		dev_err(pDrv2624data->dev, 
			"%s, ERROR, calibration fail\n",	
			__FUNCTION__);
	}
#endif
	
    dev_info(pDrv2624data->dev, 
		"drv2624 probe succeeded\n");

    return 0;

exit_gpio_request_failed:
	if(pDrv2624data->msPlatData.mnGpioNRST){
		gpio_free(pDrv2624data->msPlatData.mnGpioNRST);
	}

	if(pDrv2624data->msPlatData.mnGpioINT){
		gpio_free(pDrv2624data->msPlatData.mnGpioINT);
	}
	
    dev_err(pDrv2624data->dev, 
		"%s failed, err=%d\n",
		__FUNCTION__, err);
	return err;
}

static int drv2624_remove(struct i2c_client* client)
{
	struct drv2624_data *pDrv2624data = i2c_get_clientdata(client);

	if(pDrv2624data->msPlatData.mnGpioNRST)
		gpio_free(pDrv2624data->msPlatData.mnGpioNRST);

	if(pDrv2624data->msPlatData.mnGpioINT)
		gpio_free(pDrv2624data->msPlatData.mnGpioINT);

	misc_deregister(&drv2624_misc);
	
    return 0;
}

static struct i2c_device_id drv2624_id_table[] =
{
    { HAPTICS_DEVICE_NAME, 0 },
    {}
};
MODULE_DEVICE_TABLE(i2c, drv2624_id_table);

static struct i2c_driver drv2624_driver =
{
    .driver = {
        .name = HAPTICS_DEVICE_NAME,
		.owner = THIS_MODULE,
    },
    .id_table = drv2624_id_table,
    .probe = drv2624_probe,
    .remove = drv2624_remove,
};

static int __init drv2624_init(void)
{
	return i2c_add_driver(&drv2624_driver);
}

static void __exit drv2624_exit(void)
{
	i2c_del_driver(&drv2624_driver);
}

module_init(drv2624_init);
module_exit(drv2624_exit);

MODULE_AUTHOR("Texas Instruments Inc.");
MODULE_DESCRIPTION("Driver for "HAPTICS_DEVICE_NAME);
