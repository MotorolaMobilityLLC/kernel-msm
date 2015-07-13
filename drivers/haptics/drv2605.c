/*
** =============================================================================
** Copyright (c) 2014  Texas Instruments Inc.
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
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**
** File:
**     drv2605.c
**
** Description:
**     DRV2605 chip driver
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
#include <linux/haptics/drv2605.h>

static struct drv2605_data *pDRV2605data = NULL;

static int drv2605_reg_read(struct drv2605_data *pDrv2605data, unsigned int reg)
{
	unsigned int val;
	int ret;
	
	ret = regmap_read(pDrv2605data->regmap, reg, &val);
    
	if (ret < 0)
		return ret;
	else
		return val;
}

static int drv2605_reg_write(struct drv2605_data *pDrv2605data, unsigned char reg, char val)
{
    return regmap_write(pDrv2605data->regmap, reg, val);
}

static int drv2605_bulk_read(struct drv2605_data *pDrv2605data, unsigned char reg, unsigned int count, u8 *buf)
{
	return regmap_bulk_read(pDrv2605data->regmap, reg, buf, count);
}

static int drv2605_bulk_write(struct drv2605_data *pDrv2605data, unsigned char reg, unsigned int count, const u8 *buf)
{
	return regmap_bulk_write(pDrv2605data->regmap, reg, buf, count);
}

static int drv2605_set_bits(struct drv2605_data *pDrv2605data, unsigned char reg, unsigned char mask, unsigned char val)
{
	return regmap_update_bits(pDrv2605data->regmap, reg, mask, val);
}

static int drv2605_set_go_bit(struct drv2605_data *pDrv2605data, unsigned char val)
{
	return drv2605_reg_write(pDrv2605data, GO_REG, (val&0x01));
}

static void drv2605_poll_go_bit(struct drv2605_data *pDrv2605data)
{
    while (drv2605_reg_read(pDrv2605data, GO_REG) == GO)
      schedule_timeout_interruptible(msecs_to_jiffies(GO_BIT_POLL_INTERVAL));
}

static int drv2605_select_library(struct drv2605_data *pDrv2605data, unsigned char lib)
{
	return drv2605_reg_write(pDrv2605data, LIBRARY_SELECTION_REG, (lib&0x07));
}

static int drv2605_set_rtp_val(struct drv2605_data *pDrv2605data, char value)
{
	/* please be noted: in unsigned mode, maximum is 0xff, in signed mode, maximum is 0x7f */
	return drv2605_reg_write(pDrv2605data, REAL_TIME_PLAYBACK_REG, value);
}

static int drv2605_set_waveform_sequence(struct drv2605_data *pDrv2605data, unsigned char* seq, unsigned int size)
{
	return drv2605_bulk_write(pDrv2605data, WAVEFORM_SEQUENCER_REG, (size>WAVEFORM_SEQUENCER_MAX)?WAVEFORM_SEQUENCER_MAX:size, seq);
}

static void drv2605_change_mode(struct drv2605_data *pDrv2605data, char work_mode, char dev_mode)
{
	/* please be noted : LRA open loop cannot be used with analog input mode */
	if(dev_mode == DEV_IDLE){
		pDrv2605data->dev_mode = dev_mode;
		pDrv2605data->work_mode = work_mode;
	}else if(dev_mode == DEV_STANDBY){
		if(pDrv2605data->dev_mode != DEV_STANDBY){
			pDrv2605data->dev_mode = DEV_STANDBY;
			drv2605_reg_write(pDrv2605data, MODE_REG, MODE_STANDBY);
			schedule_timeout_interruptible(msecs_to_jiffies(WAKE_STANDBY_DELAY));
		}
		pDrv2605data->work_mode = WORK_IDLE;
	}else if(dev_mode == DEV_READY){
		if((work_mode != pDrv2605data->work_mode)
			||(dev_mode != pDrv2605data->dev_mode)){
			pDrv2605data->work_mode = work_mode;
			pDrv2605data->dev_mode = dev_mode;
			if((pDrv2605data->work_mode == WORK_VIBRATOR)
				||(pDrv2605data->work_mode == WORK_PATTERN_RTP_ON)
				||(pDrv2605data->work_mode == WORK_SEQ_RTP_ON)
				||(pDrv2605data->work_mode == WORK_RTP)){
					drv2605_reg_write(pDrv2605data, MODE_REG, MODE_REAL_TIME_PLAYBACK);
			}else if(pDrv2605data->work_mode == WORK_AUDIO2HAPTIC){
				drv2605_reg_write(pDrv2605data, MODE_REG, MODE_AUDIOHAPTIC);
			}else if(pDrv2605data->work_mode == WORK_CALIBRATION){
				drv2605_reg_write(pDrv2605data, MODE_REG, AUTO_CALIBRATION);
			}else{
				drv2605_reg_write(pDrv2605data, MODE_REG, MODE_INTERNAL_TRIGGER);
				schedule_timeout_interruptible(msecs_to_jiffies(STANDBY_WAKE_DELAY));
			}
		}
	}
}

static void setAudioHapticsEnabled(struct drv2605_data *pDrv2605data, int enable)
{
    if (enable)
    {
		if(pDrv2605data->work_mode != WORK_AUDIO2HAPTIC){
			pDrv2605data->vibrator_is_playing = YES;
			drv2605_change_mode(pDrv2605data, WORK_IDLE, DEV_READY);
			
			drv2605_set_bits(pDrv2605data, 
					Control1_REG, 
					Control1_REG_AC_COUPLE_MASK, 
					AC_COUPLE_ENABLED );
					
			drv2605_set_bits(pDrv2605data, 
					Control3_REG, 
					Control3_REG_PWMANALOG_MASK, 
					INPUT_ANALOG);	

			drv2605_change_mode(pDrv2605data, WORK_AUDIO2HAPTIC, DEV_READY);
			switch_set_state(&pDrv2605data->sw_dev, SW_STATE_AUDIO2HAPTIC);
		}
    } else {
        // Chip needs to be brought out of standby to change the registers
		if(pDrv2605data->work_mode == WORK_AUDIO2HAPTIC){
			pDrv2605data->vibrator_is_playing = NO;
			drv2605_change_mode(pDrv2605data, WORK_IDLE, DEV_READY);
						
			drv2605_set_bits(pDrv2605data, 
					Control1_REG, 
					Control1_REG_AC_COUPLE_MASK, 
					AC_COUPLE_DISABLED );
					
			drv2605_set_bits(pDrv2605data, 
					Control3_REG, 
					Control3_REG_PWMANALOG_MASK, 
					INPUT_PWM);	
					
			switch_set_state(&pDrv2605data->sw_dev, SW_STATE_IDLE);		
			drv2605_change_mode(pDrv2605data, WORK_IDLE, DEV_STANDBY); // Disable audio-to-haptics
		}
    }
}

static void play_effect(struct drv2605_data *pDrv2605data)
{
	switch_set_state(&pDrv2605data->sw_dev, SW_STATE_SEQUENCE_PLAYBACK);
	drv2605_change_mode(pDrv2605data, WORK_SEQ_PLAYBACK, DEV_READY);
    drv2605_set_waveform_sequence(pDrv2605data, pDrv2605data->sequence, WAVEFORM_SEQUENCER_MAX);
	pDrv2605data->vibrator_is_playing = YES;
    drv2605_set_go_bit(pDrv2605data, GO);

    while((drv2605_reg_read(pDrv2605data, GO_REG) == GO) && (pDrv2605data->should_stop == NO)){
        schedule_timeout_interruptible(msecs_to_jiffies(GO_BIT_POLL_INTERVAL));
	}
	
	if(pDrv2605data->should_stop == YES){
		drv2605_set_go_bit(pDrv2605data, STOP);
	}
  
    if (pDrv2605data->audio_haptics_enabled){
        setAudioHapticsEnabled(pDrv2605data, YES);
    } else {
        drv2605_change_mode(pDrv2605data, WORK_IDLE, DEV_STANDBY);
		switch_set_state(&pDrv2605data->sw_dev, SW_STATE_IDLE);		
		pDrv2605data->vibrator_is_playing = NO;
		wake_unlock(&pDrv2605data->wklock);
    }
}

static void play_Pattern_RTP(struct drv2605_data *pDrv2605data)
{
	if(pDrv2605data->work_mode == WORK_PATTERN_RTP_ON){
		drv2605_change_mode(pDrv2605data, WORK_PATTERN_RTP_OFF, DEV_READY);
		if(pDrv2605data->repeat_times == 0){
			drv2605_change_mode(pDrv2605data, WORK_IDLE, DEV_STANDBY);
			pDrv2605data->vibrator_is_playing = NO;
			switch_set_state(&pDrv2605data->sw_dev, SW_STATE_IDLE);	
			wake_unlock(&pDrv2605data->wklock);
		}else{
			hrtimer_start(&pDrv2605data->timer, ns_to_ktime((u64)pDrv2605data->silience_time * NSEC_PER_MSEC), HRTIMER_MODE_REL);
		}
	}else if(pDrv2605data->work_mode == WORK_PATTERN_RTP_OFF){
		pDrv2605data->repeat_times--;
		drv2605_change_mode(pDrv2605data, WORK_PATTERN_RTP_ON, DEV_READY);
		hrtimer_start(&pDrv2605data->timer, ns_to_ktime((u64)pDrv2605data->vibration_time * NSEC_PER_MSEC), HRTIMER_MODE_REL);
	}
}

static void play_Seq_RTP(struct drv2605_data *pDrv2605data)
{
	if(pDrv2605data->RTPSeq.RTPindex < pDrv2605data->RTPSeq.RTPCounts){
		int RTPTime = pDrv2605data->RTPSeq.RTPData[pDrv2605data->RTPSeq.RTPindex] >> 8;
		int RTPVal = pDrv2605data->RTPSeq.RTPData[pDrv2605data->RTPSeq.RTPindex] & 0x00ff ;
			
		pDrv2605data->vibrator_is_playing = YES;
		pDrv2605data->RTPSeq.RTPindex++;
		drv2605_change_mode(pDrv2605data, WORK_SEQ_RTP_ON, DEV_READY);
		drv2605_set_rtp_val(pDrv2605data,  RTPVal);
							
		hrtimer_start(&pDrv2605data->timer, ns_to_ktime((u64)RTPTime * NSEC_PER_MSEC), HRTIMER_MODE_REL);
	}else{
		drv2605_change_mode(pDrv2605data, WORK_IDLE, DEV_STANDBY);
		pDrv2605data->vibrator_is_playing = NO;
		switch_set_state(&pDrv2605data->sw_dev, SW_STATE_IDLE);	
		wake_unlock(&pDrv2605data->wklock);
	}
}

static void vibrator_off(struct drv2605_data *pDrv2605data)
{
    if (pDrv2605data->vibrator_is_playing) {
		if(pDrv2605data->audio_haptics_enabled == YES){
			setAudioHapticsEnabled(pDrv2605data, YES);
		}else{
			pDrv2605data->vibrator_is_playing = NO;
			drv2605_set_go_bit(pDrv2605data, STOP);
			drv2605_change_mode(pDrv2605data, WORK_IDLE, DEV_STANDBY);
			switch_set_state(&pDrv2605data->sw_dev, SW_STATE_IDLE);				
			wake_unlock(&pDrv2605data->wklock);		
		}
    }
}

static void drv2605_stop(struct drv2605_data *pDrv2605data)
{
	if(pDrv2605data->vibrator_is_playing){
		if(pDrv2605data->work_mode == WORK_AUDIO2HAPTIC){
			setAudioHapticsEnabled(pDrv2605data, NO);		
		}else if((pDrv2605data->work_mode == WORK_VIBRATOR)
				||(pDrv2605data->work_mode == WORK_PATTERN_RTP_ON)
				||(pDrv2605data->work_mode == WORK_PATTERN_RTP_OFF)
				||(pDrv2605data->work_mode == WORK_SEQ_RTP_ON)
				||(pDrv2605data->work_mode == WORK_SEQ_RTP_OFF)
				||(pDrv2605data->work_mode == WORK_RTP)){
			vibrator_off(pDrv2605data);
		}else if(pDrv2605data->work_mode == WORK_SEQ_PLAYBACK){
		}else{
			printk("%s, err mode=%d \n", __FUNCTION__, pDrv2605data->work_mode);
		}
	}	
}

static int vibrator_get_time(struct timed_output_dev *dev)
{
	struct drv2605_data *pDrv2605data = container_of(dev, struct drv2605_data, to_dev);

    if (hrtimer_active(&pDrv2605data->timer)) {
        ktime_t r = hrtimer_get_remaining(&pDrv2605data->timer);
        return ktime_to_ms(r);
    }

    return 0;
}

static void vibrator_enable( struct timed_output_dev *dev, int value)
{
	struct drv2605_data *pDrv2605data = container_of(dev, struct drv2605_data, to_dev);
	
	pDrv2605data->should_stop = YES;	
	hrtimer_cancel(&pDrv2605data->timer);
	cancel_work_sync(&pDrv2605data->vibrator_work);

    mutex_lock(&pDrv2605data->lock);
	
	drv2605_stop(pDrv2605data);

    if (value > 0) {
		if(pDrv2605data->audio_haptics_enabled == NO){
			wake_lock(&pDrv2605data->wklock);
		}
		mdelay(WAKE_STANDBY_DELAY);
		drv2605_change_mode(pDrv2605data, WORK_VIBRATOR, DEV_READY);
		pDrv2605data->vibrator_is_playing = YES;
		switch_set_state(&pDrv2605data->sw_dev, SW_STATE_RTP_PLAYBACK);			

		value = (value>MAX_TIMEOUT)?MAX_TIMEOUT:value;
        hrtimer_start(&pDrv2605data->timer, ns_to_ktime((u64)value * NSEC_PER_MSEC), HRTIMER_MODE_REL);
    }
	
	mutex_unlock(&pDrv2605data->lock);
	mdelay(WAKE_STANDBY_DELAY);
}

static enum hrtimer_restart vibrator_timer_func(struct hrtimer *timer)
{
	struct drv2605_data *pDrv2605data = container_of(timer, struct drv2605_data, timer);

    schedule_work(&pDrv2605data->vibrator_work);
	
    return HRTIMER_NORESTART;
}

static void vibrator_work_routine(struct work_struct *work)
{
	struct drv2605_data *pDrv2605data = container_of(work, struct drv2605_data, vibrator_work);

	mutex_lock(&pDrv2605data->lock);
	
	if((pDrv2605data->work_mode == WORK_VIBRATOR)
		||(pDrv2605data->work_mode == WORK_RTP)){
		vibrator_off(pDrv2605data);
	}else if(pDrv2605data->work_mode == WORK_SEQ_PLAYBACK){
		play_effect(pDrv2605data);
	}else if((pDrv2605data->work_mode == WORK_PATTERN_RTP_ON)||(pDrv2605data->work_mode == WORK_PATTERN_RTP_OFF)){
		play_Pattern_RTP(pDrv2605data);
	}else if((pDrv2605data->work_mode == WORK_SEQ_RTP_ON)||(pDrv2605data->work_mode == WORK_SEQ_RTP_OFF)){
		play_Seq_RTP(pDrv2605data);
	}
	
	mutex_unlock(&pDrv2605data->lock);
}

static int dev2605_open (struct inode * i_node, struct file * filp)
{
	if(pDRV2605data == NULL){
		return -ENODEV;
	}
	
	filp->private_data = pDRV2605data;
	return 0;
}

static ssize_t dev2605_read(struct file* filp, char* buff, size_t length, loff_t* offset)
{
	struct drv2605_data *pDrv2605data = (struct drv2605_data *)filp->private_data;
	int ret = 0;

	if(pDrv2605data->ReadLen > 0){
		ret = copy_to_user(buff, pDrv2605data->ReadBuff, pDrv2605data->ReadLen);
		if (ret != 0){
			printk("%s, copy_to_user err=%d \n", __FUNCTION__, ret);
		}else{
			ret = pDrv2605data->ReadLen;
		}
		pDrv2605data->ReadLen = 0;
	}
	
    return ret;
}

static bool isforDebug(int cmd){
	return ((cmd == HAPTIC_CMDID_REG_WRITE)
		||(cmd == HAPTIC_CMDID_REG_READ)
		||(cmd == HAPTIC_CMDID_REG_SETBIT));
}

static ssize_t dev2605_write(struct file* filp, const char* buff, size_t len, loff_t* off)
{
	struct drv2605_data *pDrv2605data = (struct drv2605_data *)filp->private_data;
	
	if(isforDebug(buff[0])){
	}else{
		pDrv2605data->should_stop = YES;	
		hrtimer_cancel(&pDrv2605data->timer);
		cancel_work_sync(&pDrv2605data->vibrator_work);
	}
	
    mutex_lock(&pDrv2605data->lock);
	
	if(isforDebug(buff[0])){
	}else{
		drv2605_stop(pDrv2605data);
	}
	
    switch(buff[0])
    {
        case HAPTIC_CMDID_PLAY_SINGLE_EFFECT:
        case HAPTIC_CMDID_PLAY_EFFECT_SEQUENCE:
		{	
            memset(&pDrv2605data->sequence, 0, WAVEFORM_SEQUENCER_MAX);
            if (!copy_from_user(&pDrv2605data->sequence, &buff[1], len - 1))
            {
				if(pDrv2605data->audio_haptics_enabled == NO){
					wake_lock(&pDrv2605data->wklock);
				}
				pDrv2605data->should_stop = NO;
				drv2605_change_mode(pDrv2605data, WORK_SEQ_PLAYBACK, DEV_IDLE);
                schedule_work(&pDrv2605data->vibrator_work);
            }
            break;
        }
        case HAPTIC_CMDID_PLAY_TIMED_EFFECT:
        {	
            unsigned int value = 0;
            value = buff[2];
            value <<= 8;
            value |= buff[1];
		
            if (value > 0)
            {
				if(pDrv2605data->audio_haptics_enabled == NO){			
					wake_lock(&pDrv2605data->wklock);
				}
				switch_set_state(&pDrv2605data->sw_dev, SW_STATE_RTP_PLAYBACK);
				pDrv2605data->vibrator_is_playing = YES;
  				value = (value > MAX_TIMEOUT)?MAX_TIMEOUT:value;
				drv2605_change_mode(pDrv2605data, WORK_RTP, DEV_READY);
				
				hrtimer_start(&pDrv2605data->timer, ns_to_ktime((u64)value * NSEC_PER_MSEC), HRTIMER_MODE_REL);
            }
            break;
        }

       case HAPTIC_CMDID_PATTERN_RTP:
        {
			unsigned char strength = 0;

			pDrv2605data->vibration_time = (int)((((int)buff[2])<<8) | (int)buff[1]);
			pDrv2605data->silience_time = (int)((((int)buff[4])<<8) | (int)buff[3]);
			strength = buff[5];
			pDrv2605data->repeat_times = buff[6];
			
            if(pDrv2605data->vibration_time > 0){
				if(pDrv2605data->audio_haptics_enabled == NO){
					wake_lock(&pDrv2605data->wklock);			
				}
				switch_set_state(&pDrv2605data->sw_dev, SW_STATE_RTP_PLAYBACK);
				pDrv2605data->vibrator_is_playing = YES;
                if(pDrv2605data->repeat_times > 0)
					pDrv2605data->repeat_times--;
                if (pDrv2605data->vibration_time > MAX_TIMEOUT)
                    pDrv2605data->vibration_time = MAX_TIMEOUT;
				drv2605_change_mode(pDrv2605data, WORK_PATTERN_RTP_ON, DEV_READY);
				drv2605_set_rtp_val(pDrv2605data, strength);
				
                hrtimer_start(&pDrv2605data->timer, ns_to_ktime((u64)pDrv2605data->vibration_time * NSEC_PER_MSEC), HRTIMER_MODE_REL);
            }
            break;
        }		
 		
		case HAPTIC_CMDID_RTP_SEQUENCE:
		{
            memset(&pDrv2605data->RTPSeq, 0, sizeof(struct RTP_Seq));
			if(((len-1)%2) == 0){
				pDrv2605data->RTPSeq.RTPCounts = (len-1)/2;
				if((pDrv2605data->RTPSeq.RTPCounts <= MAX_RTP_SEQ)&&(pDrv2605data->RTPSeq.RTPCounts>0)){
					if(copy_from_user(pDrv2605data->RTPSeq.RTPData, &buff[1], pDrv2605data->RTPSeq.RTPCounts*2) != 0){
						printk("%s, rtp_seq copy seq err\n", __FUNCTION__);	
						break;
					}
					
					if(pDrv2605data->audio_haptics_enabled == NO){
						wake_lock(&pDrv2605data->wklock);
					}
					switch_set_state(&pDrv2605data->sw_dev, SW_STATE_RTP_PLAYBACK);
					drv2605_change_mode(pDrv2605data, WORK_SEQ_RTP_OFF, DEV_IDLE);
					schedule_work(&pDrv2605data->vibrator_work);
				}else{
					printk("%s, rtp_seq count error,maximum=%d\n", __FUNCTION__,MAX_RTP_SEQ);
				}
			}else{
				printk("%s, rtp_seq len error\n", __FUNCTION__);
			}
			break;
		}
		
        case HAPTIC_CMDID_STOP:
        {
            break;
        }
		
        case HAPTIC_CMDID_AUDIOHAPTIC_ENABLE:
        {
			if(pDrv2605data->audio_haptics_enabled == NO){
				wake_lock(&pDrv2605data->wklock);
			}
			pDrv2605data->audio_haptics_enabled = YES;
			setAudioHapticsEnabled(pDrv2605data, YES);
            break;
        }
		
        case HAPTIC_CMDID_AUDIOHAPTIC_DISABLE:
        {
			if(pDrv2605data->audio_haptics_enabled == YES){
				pDrv2605data->audio_haptics_enabled = NO;
				wake_unlock(&pDrv2605data->wklock);	
			}
            break;
        }
		
		case HAPTIC_CMDID_REG_READ:
		{
			if(len == 2){
				pDrv2605data->ReadLen = 1;
				pDrv2605data->ReadBuff[0] = drv2605_reg_read(pDrv2605data, buff[1]);
			}else if(len == 3){
				pDrv2605data->ReadLen = (buff[2]>MAX_READ_BYTES)?MAX_READ_BYTES:buff[2];
				drv2605_bulk_read(pDrv2605data, buff[1], pDrv2605data->ReadLen, pDrv2605data->ReadBuff);
			}else{
				printk("%s, reg_read len error\n", __FUNCTION__);
			}
			break;
		}
		
		case HAPTIC_CMDID_REG_WRITE:
		{
			if((len-1) == 2){
				drv2605_reg_write(pDrv2605data, buff[1], buff[2]);	
			}else if((len-1)>2){
				unsigned char *data = (unsigned char *)kzalloc(len-2, GFP_KERNEL);
				if(data != NULL){
					if(copy_from_user(data, &buff[2], len-2) != 0){
						printk("%s, reg copy err\n", __FUNCTION__);	
					}else{
						drv2605_bulk_write(pDrv2605data, buff[1], len-2, data);
					}
					kfree(data);
				}
			}else{
				printk("%s, reg_write len error\n", __FUNCTION__);
			}
			break;
		}
		
		case HAPTIC_CMDID_REG_SETBIT:
		{
			int i=1;			
			for(i=1; i< len; ){
				drv2605_set_bits(pDrv2605data, buff[i], buff[i+1], buff[i+2]);
				i += 3;
			}
			break;
		}		
    default:
		printk("%s, unknown HAPTIC cmd\n", __FUNCTION__);
      break;
    }

    mutex_unlock(&pDrv2605data->lock);

    return len;
}


static struct file_operations fops =
{
	.open = dev2605_open,
    .read = dev2605_read,
    .write = dev2605_write,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
void drv2605_early_suspend(struct early_suspend *h){
	struct drv2605_data *pDrv2605data = container_of(h, struct drv2605_data, early_suspend); 

	pDrv2605data->should_stop = YES;	
	hrtimer_cancel(&pDrv2605data->timer);
	cancel_work_sync(&pDrv2605data->vibrator_work);
	
	mutex_lock(&pDrv2605data->lock);	
	
	drv2605_stop(pDrv2605data);
	if(pDrv2605data->audio_haptics_enabled == YES){
		wake_unlock(&pDrv2605data->wklock);
	}
	
	mutex_unlock(&pDrv2605data->lock);
    return ;
}

void drv2605_late_resume(struct early_suspend *h) {
	struct drv2605_data *pDrv2605data = container_of(h, struct drv2605_data, early_suspend); 
	
	mutex_lock(&pDrv2605data->lock);	
	if(pDrv2605data->audio_haptics_enabled == YES){
		wake_lock(&pDrv2605data->wklock);
		setAudioHapticsEnabled(pDrv2605data, YES);
	}
	mutex_unlock(&pDrv2605data->lock);
    return ; 
 }
#endif
 
static int Haptics_init(struct drv2605_data *pDrv2605data)
{
    int reval = -ENOMEM;
   
    pDrv2605data->version = MKDEV(0,0);
    reval = alloc_chrdev_region(&pDrv2605data->version, 0, 1, HAPTICS_DEVICE_NAME);
    if (reval < 0)
    {
        printk(KERN_ALERT"drv2605: error getting major number %d\n", reval);
        goto fail0;
    }

    pDrv2605data->class = class_create(THIS_MODULE, HAPTICS_DEVICE_NAME);
    if (!pDrv2605data->class)
    {
        printk(KERN_ALERT"drv2605: error creating class\n");
        goto fail1;
    }

    pDrv2605data->device = device_create(pDrv2605data->class, NULL, pDrv2605data->version, NULL, HAPTICS_DEVICE_NAME);
    if (!pDrv2605data->device)
    {
        printk(KERN_ALERT"drv2605: error creating device 2605\n");
        goto fail2;
    }

    cdev_init(&pDrv2605data->cdev, &fops);
    pDrv2605data->cdev.owner = THIS_MODULE;
    pDrv2605data->cdev.ops = &fops;
    reval = cdev_add(&pDrv2605data->cdev, pDrv2605data->version, 1);
    if (reval)
    {
        printk(KERN_ALERT"drv2605: fail to add cdev\n");
        goto fail3;
    }

	pDrv2605data->sw_dev.name = "haptics";
	reval = switch_dev_register(&pDrv2605data->sw_dev);
	if (reval < 0) {
		printk(KERN_ALERT"drv2605: fail to register switch\n");
		goto fail4;
	}	
	
	pDrv2605data->to_dev.name = "vibrator";
	pDrv2605data->to_dev.get_time = vibrator_get_time;
	pDrv2605data->to_dev.enable = vibrator_enable;

    if (timed_output_dev_register(&(pDrv2605data->to_dev)) < 0)
    {
        printk(KERN_ALERT"drv2605: fail to create timed output dev\n");
        goto fail3;
    }

#ifdef CONFIG_HAS_EARLYSUSPEND
    pDrv2605data->early_suspend.suspend = drv2605_early_suspend;
	pDrv2605data->early_suspend.resume = drv2605_late_resume;
	pDrv2605data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 1;
	register_early_suspend(&pDrv2605data->early_suspend);
#endif  
	
    hrtimer_init(&pDrv2605data->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    pDrv2605data->timer.function = vibrator_timer_func;
    INIT_WORK(&pDrv2605data->vibrator_work, vibrator_work_routine);
    
    wake_lock_init(&pDrv2605data->wklock, WAKE_LOCK_SUSPEND, "vibrator");
    mutex_init(&pDrv2605data->lock);
	
    return 0;

fail4:
	switch_dev_unregister(&pDrv2605data->sw_dev);
fail3:
	device_destroy(pDrv2605data->class, pDrv2605data->version);
fail2:
    class_destroy(pDrv2605data->class);	
fail1:
    unregister_chrdev_region(pDrv2605data->version, 1);	
fail0:
    return reval;
}

static void dev_init_platform_data(struct drv2605_data *pDrv2605data)
{
	struct drv2605_platform_data *pDrv2605Platdata = &pDrv2605data->PlatData;
	struct actuator_data actuator = pDrv2605Platdata->actuator;
	struct audio2haptics_data a2h = pDrv2605Platdata->a2h;
	unsigned char temp = 0;
	
	drv2605_select_library(pDrv2605data, actuator.g_effect_bank);
	
	//OTP memory saves data from 0x16 to 0x1a
	if(pDrv2605data->OTP == 0) {
		if(actuator.rated_vol != 0){
			drv2605_reg_write(pDrv2605data, RATED_VOLTAGE_REG, actuator.rated_vol);
		}else{
			printk("%s, ERROR Rated ZERO\n", __FUNCTION__);
		}

		if(actuator.over_drive_vol != 0){
			drv2605_reg_write(pDrv2605data, OVERDRIVE_CLAMP_VOLTAGE_REG, actuator.over_drive_vol);
		}else{
			printk("%s, ERROR OverDriveVol ZERO\n", __FUNCTION__);
		}
		
		drv2605_set_bits(pDrv2605data, 
						FEEDBACK_CONTROL_REG, 
						FEEDBACK_CONTROL_DEVICE_TYPE_MASK, 
						(actuator.device_type == LRA)?FEEDBACK_CONTROL_MODE_LRA:FEEDBACK_CONTROL_MODE_ERM);
	}else{
		printk("%s, OTP programmed\n", __FUNCTION__);
	}
	
	if(pDrv2605Platdata->loop == OPEN_LOOP){
		temp = BIDIR_INPUT_BIDIRECTIONAL;
	}else{
		if(pDrv2605Platdata->BIDIRInput == UniDirectional){
			temp = BIDIR_INPUT_UNIDIRECTIONAL;
		}else{
			temp = BIDIR_INPUT_BIDIRECTIONAL;
		}
	}

	if(actuator.device_type == LRA){
		unsigned char DriveTime = 5*(1000 - actuator.LRAFreq)/actuator.LRAFreq;
		drv2605_set_bits(pDrv2605data, 
				Control1_REG, 
				Control1_REG_DRIVE_TIME_MASK, 
				DriveTime);	
		printk("%s, LRA = %d, DriveTime=0x%x\n", __FUNCTION__, actuator.LRAFreq, DriveTime);
	}
	
	drv2605_set_bits(pDrv2605data, 
				Control2_REG, 
				Control2_REG_BIDIR_INPUT_MASK, 
				temp);	
		
	if((pDrv2605Platdata->loop == OPEN_LOOP)&&(actuator.device_type == LRA))
	{
		temp = LRA_OpenLoop_Enabled;
	}
	else if((pDrv2605Platdata->loop == OPEN_LOOP)&&(actuator.device_type == ERM))
	{
		temp = ERM_OpenLoop_Enabled;
	}
	else
	{
		temp = ERM_OpenLoop_Disable|LRA_OpenLoop_Disable;
	}

	if((pDrv2605Platdata->loop == CLOSE_LOOP) &&(pDrv2605Platdata->BIDIRInput == UniDirectional))
	{
		temp |= RTP_FORMAT_UNSIGNED;
		drv2605_reg_write(pDrv2605data, REAL_TIME_PLAYBACK_REG, 0xff);
	}
	else
	{
		if(pDrv2605Platdata->RTPFormat == Signed)
		{
			temp |= RTP_FORMAT_SIGNED;
			drv2605_reg_write(pDrv2605data, REAL_TIME_PLAYBACK_REG, 0x7f);
		}
		else
		{
			temp |= RTP_FORMAT_UNSIGNED;
			drv2605_reg_write(pDrv2605data, REAL_TIME_PLAYBACK_REG, 0xff);
		}
	}
	drv2605_set_bits(pDrv2605data, 
					Control3_REG, 
					Control3_REG_LOOP_MASK|Control3_REG_FORMAT_MASK, 
					temp);	

	//for audio to haptics
	if(pDrv2605Platdata->GpioTrigger == 0)	//not used as external trigger
	{
		drv2605_reg_write(pDrv2605data, AUDIO_HAPTICS_MIN_INPUT_REG, a2h.a2h_min_input);
		drv2605_reg_write(pDrv2605data, AUDIO_HAPTICS_MAX_INPUT_REG, a2h.a2h_max_input);
		drv2605_reg_write(pDrv2605data, AUDIO_HAPTICS_MIN_OUTPUT_REG, a2h.a2h_min_output);
		drv2605_reg_write(pDrv2605data, AUDIO_HAPTICS_MAX_OUTPUT_REG, a2h.a2h_max_output);
	}
}

static int dev_auto_calibrate(struct drv2605_data *pDrv2605data)
{
	int err = 0, status=0;
	
	drv2605_change_mode(pDrv2605data, WORK_CALIBRATION, DEV_READY);
	drv2605_set_go_bit(pDrv2605data, GO);
			
	/* Wait until the procedure is done */
	drv2605_poll_go_bit(pDrv2605data);
	/* Read status */
	status = drv2605_reg_read(pDrv2605data, STATUS_REG);

	printk("%s, calibration status =0x%x\n", __FUNCTION__, status);

	/* Read calibration results */
	drv2605_reg_read(pDrv2605data, AUTO_CALI_RESULT_REG);
	drv2605_reg_read(pDrv2605data, AUTO_CALI_BACK_EMF_RESULT_REG);
	drv2605_reg_read(pDrv2605data, FEEDBACK_CONTROL_REG);
	
	return err;
}

static struct regmap_config drv2605_i2c_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.cache_type = REGCACHE_NONE,
};

static int drv2605_probe(struct i2c_client* client, const struct i2c_device_id* id)
{
	struct drv2605_data *pDrv2605data;
	struct drv2605_platform_data *pDrv2605Platdata = client->dev.platform_data;
	
	int err = 0;
	int status = 0;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
	{
		printk(KERN_ERR"%s:I2C check failed\n", __FUNCTION__);
		return -ENODEV;
	}

	pDrv2605data = devm_kzalloc(&client->dev, sizeof(struct drv2605_data), GFP_KERNEL);
	if (pDrv2605data == NULL){
		printk(KERN_ERR"%s:no memory\n", __FUNCTION__);
		return -ENOMEM;
	}

	pDrv2605data->regmap = devm_regmap_init_i2c(client, &drv2605_i2c_regmap);
	if (IS_ERR(pDrv2605data->regmap)) {
		err = PTR_ERR(pDrv2605data->regmap);
		printk(KERN_ERR"%s:Failed to allocate register map: %d\n",__FUNCTION__,err);
		return err;
	}

	memcpy(&pDrv2605data->PlatData, pDrv2605Platdata, sizeof(struct drv2605_platform_data));
	i2c_set_clientdata(client,pDrv2605data);

	if(pDrv2605data->PlatData.GpioTrigger){
		err = gpio_request(pDrv2605data->PlatData.GpioTrigger,HAPTICS_DEVICE_NAME"Trigger");
		if(err < 0){
			printk(KERN_ERR"%s: GPIO request Trigger error\n", __FUNCTION__);				
			goto exit_gpio_request_failed;
		}
	}

	if(pDrv2605data->PlatData.GpioEnable){
		err = gpio_request(pDrv2605data->PlatData.GpioEnable,HAPTICS_DEVICE_NAME"Enable");
		if(err < 0){
			printk(KERN_ERR"%s: GPIO request enable error\n", __FUNCTION__);					
			goto exit_gpio_request_failed;
		}

	    /* Enable power to the chip */
	    gpio_direction_output(pDrv2605data->PlatData.GpioEnable, 1);

	    /* Wait 30 us */
	    udelay(30);
	}

	err = drv2605_reg_read(pDrv2605data, STATUS_REG);
	if(err < 0){
		printk("%s, i2c bus fail (%d)\n", __FUNCTION__, err);
		goto exit_gpio_request_failed;
	}else{
		printk("%s, i2c status (0x%x)\n", __FUNCTION__, err);
		status = err;
	}
	/* Read device ID */
	pDrv2605data->device_id = (status & DEV_ID_MASK);
	switch (pDrv2605data->device_id)
	{
		case DRV2605_VER_1DOT1:
		printk("drv2605 driver found: drv2605 v1.1.\n");
		break;
		case DRV2605_VER_1DOT0:
		printk("drv2605 driver found: drv2605 v1.0.\n");
		break;
		case DRV2604:
		printk(KERN_ALERT"drv2605 driver found: drv2604.\n");
		break;
		default:
		printk(KERN_ERR"drv2605 driver found: unknown.\n");
		break;
	}

	if((pDrv2605data->device_id != DRV2605_VER_1DOT1)
		&&(pDrv2605data->device_id != DRV2605_VER_1DOT0)){
		printk("%s, status(0x%x),device_id(%d) fail\n",
			__FUNCTION__, status, pDrv2605data->device_id);
		goto exit_gpio_request_failed;
	}

	drv2605_change_mode(pDrv2605data, WORK_IDLE, DEV_READY);
	schedule_timeout_interruptible(msecs_to_jiffies(STANDBY_WAKE_DELAY));
	
	pDrv2605data->OTP = drv2605_reg_read(pDrv2605data, AUTOCAL_MEM_INTERFACE_REG) & AUTOCAL_MEM_INTERFACE_REG_OTP_MASK;
	
	dev_init_platform_data(pDrv2605data);
	
	if(pDrv2605data->OTP == 0){
		err = dev_auto_calibrate(pDrv2605data);
		if(err < 0){
			printk("%s, ERROR, calibration fail\n",	__FUNCTION__);
		}
	}

	/* Put hardware in standby */
	drv2605_change_mode(pDrv2605data, WORK_IDLE, DEV_STANDBY);
	/* Set max VOLTAGE*/
	drv2605_reg_write(pDrv2605data, OVERDRIVE_CLAMP_VOLTAGE_REG, 0x96);
	Haptics_init(pDrv2605data);
	pDRV2605data = pDrv2605data;
	printk("drv2605 probe succeeded\n");
	return 0;

exit_gpio_request_failed:
	if(pDrv2605data->PlatData.GpioTrigger){
		gpio_free(pDrv2605data->PlatData.GpioTrigger);
	}

	if(pDrv2605data->PlatData.GpioEnable){
		gpio_free(pDrv2605data->PlatData.GpioEnable);
	}
	
    printk(KERN_ERR"%s failed, err=%d\n",__FUNCTION__, err);
	return err;
}

static int drv2605_remove(struct i2c_client* client)
{
	struct drv2605_data *pDrv2605data = i2c_get_clientdata(client);

    device_destroy(pDrv2605data->class, pDrv2605data->version);
    class_destroy(pDrv2605data->class);
    unregister_chrdev_region(pDrv2605data->version, 1);

	if(pDrv2605data->PlatData.GpioTrigger)
		gpio_free(pDrv2605data->PlatData.GpioTrigger);

	if(pDrv2605data->PlatData.GpioEnable)
		gpio_free(pDrv2605data->PlatData.GpioEnable);

#ifdef CONFIG_HAS_EARLYSUSPEND		
	unregister_early_suspend(&pDrv2605data->early_suspend);
#endif
	
    printk(KERN_ALERT"drv2605 remove");
	
    return 0;
}

static struct i2c_device_id drv2605_id_table[] =
{
    { HAPTICS_DEVICE_NAME, 0 },
    {}
};
MODULE_DEVICE_TABLE(i2c, drv2605_id_table);

static struct i2c_driver drv2605_driver =
{
    .driver = {
        .name = HAPTICS_DEVICE_NAME,
		.owner = THIS_MODULE,
    },
    .id_table = drv2605_id_table,
    .probe = drv2605_probe,
    .remove = drv2605_remove,
};

static int __init drv2605_init(void)
{
	return i2c_add_driver(&drv2605_driver);
}

static void __exit drv2605_exit(void)
{
	i2c_del_driver(&drv2605_driver);
}

rootfs_initcall(drv2605_init);
module_exit(drv2605_exit);

MODULE_AUTHOR("Texas Instruments Inc.");

MODULE_DESCRIPTION("Driver for "HAPTICS_DEVICE_NAME);


/******************************************************************************
 FUNCTION: vibrator_ctrl_kernel
 
 DESCRIPTION: vibrator control interface in kernel
                      value:vibrate time, unit ms
 ******************************************************************************/
void vibrator_ctrl_kernel(int value)
{
    pDRV2605data->should_stop = YES;	
    hrtimer_cancel(&pDRV2605data->timer);
    cancel_work_sync(&pDRV2605data->vibrator_work);

    mutex_lock(&pDRV2605data->lock);
    
    drv2605_stop(pDRV2605data);

    if (value > 0) {
        if(pDRV2605data->audio_haptics_enabled == NO){
            wake_lock(&pDRV2605data->wklock);
        }

        drv2605_change_mode(pDRV2605data, WORK_VIBRATOR, DEV_READY);
        pDRV2605data->vibrator_is_playing = YES;
        switch_set_state(&pDRV2605data->sw_dev, SW_STATE_RTP_PLAYBACK);

        value = (value>MAX_TIMEOUT)?MAX_TIMEOUT:value;
        hrtimer_start(&pDRV2605data->timer, ns_to_ktime((u64)value * NSEC_PER_MSEC), HRTIMER_MODE_REL);
    }
    
    mutex_unlock(&pDRV2605data->lock);
}

EXPORT_SYMBOL(vibrator_ctrl_kernel);
MODULE_DESCRIPTION("Driver for "HAPTICS_DEVICE_NAME);

