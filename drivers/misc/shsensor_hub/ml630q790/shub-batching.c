/*
 *  shub-batching.c - Linux kernel modules for interface of ML610Q79x
 *
 *  Copyright (C) 2014 LAPIS SEMICONDUCTOR CO., LTD.
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
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include <asm/uaccess.h> 
#include <asm/div64.h> 
#include <linux/fs.h>
#include "shub_io.h"
#include "ml630q790.h"
#include <linux/limits.h>

static void batching_proc_work_function(struct work_struct *work);
static enum hrtimer_restart batch_poll(struct hrtimer *tm);

enum{
    SENSOR_BATCHING_STOP = 0,
    SENSOR_BATCHING_RUN,
};

#define SHUB_BATCHING_SENSOR_MAX (20)
static struct{
    uint32_t sensor;
    int64_t  latency;
} shub_batching_info[SHUB_BATCHING_SENSOR_MAX];


static int64_t curLatency = LLONG_MAX;
static int status = SENSOR_BATCHING_STOP;
static struct work_struct batching_proc_work;
static struct hrtimer batch_timer;

static int addBatchingInfo(uint32_t sensor, int64_t latency)
{
    int ret = 0;
    int i;
    int pos = -1;
    int pos_blank = -1;
    int64_t curLatency_tmp = LLONG_MAX;

    for(i = 0;i < SHUB_BATCHING_SENSOR_MAX;i++){
        if(shub_batching_info[i].sensor == 0){
            if(pos_blank == -1){
                //printk("shub batching: addBatchingInfo() blank pos = %d\n", i);
                pos_blank = i;
            }
        }else{
            if(shub_batching_info[i].sensor == sensor){
                //printk("shub batching: addBatchingInfo() sensor(0x%x) pos = %d\n", sensor, i);
                pos = i;
            }
        }
    }

    if(pos == -1){
        if(pos_blank != -1){
            pos = pos_blank;
        }else{
            //printk("shub batching: addBatchingInfo() buffer over flow\n");
            return 0;
        }
    }

    if(latency > 0){
        //printk("shub batching: addBatchingInfo() set enable latency\n");
        shub_batching_info[pos].sensor   = sensor;
        shub_batching_info[pos].latency  = latency;
    }else{
        //printk("shub batching: addBatchingInfo() clear latency\n");
        shub_batching_info[pos].sensor   = 0;
        shub_batching_info[pos].latency  = 0;
    }

    for(i = 0;i < SHUB_BATCHING_SENSOR_MAX;i++){
        if(shub_batching_info[i].sensor != 0){
            if(curLatency_tmp > shub_batching_info[i].latency){
                curLatency_tmp = shub_batching_info[i].latency;
            }
            ret = 1;
        }
    }
    curLatency = curLatency_tmp;

    return ret;
}

int32_t initBatchingProc(void)
{
    status = SENSOR_BATCHING_STOP;
    INIT_WORK(&batching_proc_work, batching_proc_work_function);
    hrtimer_init(&batch_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    batch_timer.function = batch_poll;
    return 0;
}
EXPORT_SYMBOL(initBatchingProc);

int32_t startBatchingProc(void)
{
    //printk("shub batching: start (status=%s)\n", 
    //		(status == SENSOR_BATCHING_RUN)? "SENSOR_BATCHING_RUN" : "SENSOR_BATCHING_STOP" );
    if(status != SENSOR_BATCHING_RUN){
        status = SENSOR_BATCHING_RUN;
        hrtimer_cancel(&batch_timer);
        hrtimer_start(&batch_timer, ns_to_ktime(curLatency), HRTIMER_MODE_REL);
    }
    return 0;
}
EXPORT_SYMBOL(startBatchingProc);

int32_t stopBatchingProc(void)
{
    //printk("shub batching: stop (status=%s)\n", 
    //		(status == SENSOR_BATCHING_RUN)? "SENSOR_BATCHING_RUN" : "SENSOR_BATCHING_STOP" );
    if(status != SENSOR_BATCHING_STOP){
        status = SENSOR_BATCHING_STOP;
        hrtimer_cancel(&batch_timer);
    }
    return 0;
}
EXPORT_SYMBOL(stopBatchingProc);

int32_t suspendBatchingProc(void)
{
    if(status == SENSOR_BATCHING_RUN){
        hrtimer_cancel(&batch_timer);
    }
    return 0;
}
EXPORT_SYMBOL(suspendBatchingProc);

int32_t resumeBatchingProc(void)
{
    if(status == SENSOR_BATCHING_RUN){
        hrtimer_start(&batch_timer, ns_to_ktime(curLatency), HRTIMER_MODE_REL);
    }
    return 0;
}
EXPORT_SYMBOL(resumeBatchingProc);

int32_t setMaxBatchReportLatency(uint32_t sensor, int64_t latency)
{
    int ret;
    ret = addBatchingInfo(sensor, latency);
    //printk("shub batching: setMaxBatchReportLatency(0x%08x: %lld ns) ret=%d\n", sensor, latency, ret);
    if(ret == 0){
        stopBatchingProc();
    }else if(status == SENSOR_BATCHING_STOP){
        startBatchingProc();
    }else if(status == SENSOR_BATCHING_RUN){
        hrtimer_cancel(&batch_timer);
        hrtimer_start(&batch_timer, ns_to_ktime(curLatency), HRTIMER_MODE_REL);
    }
    return 0;
}
EXPORT_SYMBOL(setMaxBatchReportLatency);

static enum hrtimer_restart batch_poll(struct hrtimer *tm)
{
    if(status == SENSOR_BATCHING_RUN){
        schedule_work(&batching_proc_work);
        hrtimer_forward_now(&batch_timer, ns_to_ktime(curLatency));
        return HRTIMER_RESTART;
    }
    return HRTIMER_NORESTART;
}

static void batching_proc_work_function(struct work_struct *work)
{
    //printk("shub batching: flush\n");
    shub_qos_start();
    shub_logging_flush();
    shub_qos_end();
}
