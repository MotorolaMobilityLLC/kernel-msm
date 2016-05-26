/* drivers/input/misc/proximity_cal.c  (proximitySensor Driver)
 *
 * Copyright (C) 2016 SHARP CORPORATION All rights reserved.
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
/* SHARP PROXIMITY SENSOR DRIVER FOR KERNEL MODE                             */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* INCLUDE FILES                                                             */
/* ------------------------------------------------------------------------- */

#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/device.h>
#include <linux/hrtimer.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/wakelock.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>

#include <linux/ir2e71y.h>
#include <linux/input/proximity.h>

/* ------------------------------------------------------------------------- */
/* MACRAOS                                                                   */
/* ------------------------------------------------------------------------- */
#define PROX_USE_SMEM

#ifdef PROX_USE_SMEM
#include <soc/qcom/sh_smem.h>
#endif /* PROX_USE_SMEM */

#define PROX_IRQ_DISABLED		0x00	/* Disabled interruption     */
#define PROX_IRQ_ENABLED		0x01	/* Enabled interruption      */

/* adb debug_log */
static int	proximity_dbg_func_log = 0;		/* log : Init = OFF */
static int	proximity_dbg_func_fin_log = 0;	/* log : Init = OFF */
static int	proximity_dbg_enable_log = 0;	/* log : Init = OFF */
static int	proximity_dbg_sensor_log = 0;	/* log : Init = OFF */
static int	proximity_dbg_error_log = 1;	/* log : Init = ON  */

#if defined (CONFIG_ANDROID_ENGINEERING)
	module_param(proximity_dbg_func_log, int, 0600);
	module_param(proximity_dbg_func_fin_log, int, 0600);
	module_param(proximity_dbg_enable_log, int, 0600);
	module_param(proximity_dbg_sensor_log, int, 0600);
	module_param(proximity_dbg_error_log, int, 0600);
#endif  /* CONFIG_ANDROID_ENGINEERING */

#define FUNC_LOG() \
	if(proximity_dbg_func_log == 1){ \
		printk(KERN_DEBUG "[PROXIMITY] %s is called\n", __func__); \
	}

#define FUNC_FIN_LOG() \
	if(proximity_dbg_func_fin_log == 1){ \
		printk(KERN_DEBUG "[PROXIMITY] %s is finished\n", __func__); \
	}

#define DEBUG_LOG(format, ...) \
	if(proximity_dbg_enable_log == 1){ \
		printk(KERN_DEBUG "[PROXIMITY][%s] " format "\n", __func__, ## __VA_ARGS__); \
	}

#define DEBUG_SENSOR_LOG(format, ...) \
	if(proximity_dbg_sensor_log == 1){ \
		printk(KERN_DEBUG "[PROXIMITY][%s] " format "\n", __func__, ## __VA_ARGS__); \
	}

#define DEBUG_ERROR_LOG(format, ...) \
	if(proximity_dbg_error_log == 1){ \
		printk(KERN_DEBUG "[PROXIMITY][%s] " format "\n", __func__, ## __VA_ARGS__); \
	}

/* ------------------------------------------------------------------------- */
/* VARIABLES                                                                 */
/* ------------------------------------------------------------------------- */
typedef struct drv_data_tag	drv_data;
typedef struct work_struct	WorkStruct;
typedef struct input_dev	InputDev;
typedef struct device		Device;

struct drv_data_tag
{
	int			irq_gpio;
	InputDev	*input_dev;
	WorkStruct	IrqWork;
	int			irq;
};

static int			Threshold_Low;
static int			Threshold_High;

static drv_data		*poProximityRec;
static atomic_t		open_flag = ATOMIC_INIT(0);
static atomic_t		sensor_data = ATOMIC_INIT(7);			/* Init = Far										*/
static atomic_t		enable_mode = ATOMIC_INIT(0);    		/* 0=Disable,1=Enable								*/
static atomic_t		dataread_func_flag = ATOMIC_INIT(0);	/* 0=Disable,1=Enable								*/
static atomic_t		disabling_flag = ATOMIC_INIT(0);		/* 0: Not on IOECS_Disable(), 1: On IOECS_Disable()	*/
static atomic_t		recovering_flag = ATOMIC_INIT(0);		/* 0: Not on recovering, 1: On recovering			*/
static int			prox_irq_status;						/* 0: Disabled, 1: Enabled							*/
static spinlock_t	lock;

/* adb debug */
static int			sensor_data_tmp = 7;					/* for debug : Init = Far							*/
static int			prox_dbg_mode   = 0;					/* 0=Disable, 1=Enable								*/

module_param(sensor_data_tmp, int, 0600);
module_param(prox_dbg_mode, int, 0600);

static struct wake_lock		prox_timeout_wake_lock;
static struct wake_lock		prox_interrupt_wake_lock;
static struct work_struct	prox_updata_val_work;
static struct mutex			prox_val_lock;
static struct mutex			prox_enable_lock;

enum {
	PROX_GPIO_STATE_ACTIVE  = 0,
	PROX_GPIO_STATE_STANDBY = 1,
	PROX_GPIO_STATE_MAX
};

/* ------------------------------------------------------------------------- */
/* PROTOTYPES                                                                */
/* ------------------------------------------------------------------------- */
static int PROX_I2cRead(unsigned char RegAdr,unsigned char *rData);
static int IOECS_Enable(void);
static int IOECS_Disable(void);
static int IOECS_SetCycle(short cycle_data);
static int IOECS_GetVO_DATA(uint8_t *cycle_data);
static int IOECS_GetD2_DATA(unsigned short *psresult);
static int IOECS_LT_Thresshold_Write(unsigned short lt_threshold);
static int IOECS_HT_Thresshold_Write(unsigned short ht_threshold);
static int PROX_open(struct inode *inode, struct file *filp);
static int PROX_release(struct inode *inode, struct file *filp);
static ssize_t PROX_read(struct file *filp, char __user *buf, size_t count, loff_t *ppos);
static long PROX_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
static irqreturn_t PROX_interrupt(int irq, void *dev_id);
static void PROX_Irq_workfunc( WorkStruct *work );
static int prox_parse_dt(struct device *dev);
static int PROX_Initialize(void);
static int PROX_Remove(struct platform_device *pdev);
static int PROX_Probe(struct platform_device *pdev);
static void PROX_disable_irq(void);
static void PROX_enable_irq(void);
static int __init PROX_Init(void);
static void __exit PROX_Exit(void);
static void PROX_updata_val(WorkStruct *work);


static struct file_operations PROX_ctl_fops = {
	.owner			= THIS_MODULE,
	.open			= PROX_open,
	.release		= PROX_release,
	.unlocked_ioctl	= PROX_ioctl,
	.compat_ioctl   = PROX_ioctl,
	.read			= PROX_read,
};

static struct miscdevice PROX_device = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "proximity_dev",
	.fops	= &PROX_ctl_fops,
};

/* ------------------------------------------------------------------------- */
/* FUNCTIONS                                                                 */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* PROX_I2cWrite                                                             */
/* ------------------------------------------------------------------------- */
#if 0
static int PROX_I2cWrite(unsigned char RegAdr, unsigned char wData)
{
	int nResult = 0;
	struct struct ir2e71y_bdic_i2c_msg i2c_msg;
	unsigned char i2c_wbuf[2];

	FUNC_LOG();

	i2c_wbuf[0]  = RegAdr;
	i2c_wbuf[1]  = wData;
	i2c_msg.addr = 0x39;
	i2c_msg.mode = IR2E71Y_BDIC_I2C_M_W;
	i2c_msg.wlen = 2;
	i2c_msg.rlen = 0;
	i2c_msg.wbuf = &i2c_wbuf[0];
	i2c_msg.rbuf = NULL;

	nResult = ir2e71y_write_bdic_i2c(&i2c_msg);
	if (nResult != IR2E71Y_RESULT_SUCCESS) {
		DEBUG_ERROR_LOG("I2C_WriteError id = %d",nResult);
	}

	DEBUG_SENSOR_LOG("I2cWrite(reg:%02X,Data:%02X)", i2c_wbuf[0], i2c_wbuf[1]);

	FUNC_FIN_LOG();

	return nResult;
}
#endif

/* ------------------------------------------------------------------------- */
/* PROX_I2cRead                                                              */
/* ------------------------------------------------------------------------- */
static int PROX_I2cRead(unsigned char RegAdr,unsigned char *rData)
{
	int nResult = 0;
	int nRetry = 0;
	struct ir2e71y_bdic_i2c_msg i2c_msg;
	unsigned char i2c_wbuf[1];
	unsigned char i2c_rbuf[1];

	FUNC_LOG();

	if (rData == NULL) {
		DEBUG_ERROR_LOG("[PROX_I2cRead NULL_Error : rData]");
		return SH_PROXIMITY_RESULT_FAILURE;
	}

	i2c_wbuf[0]  = RegAdr;
	i2c_rbuf[0]  = 0x00;
	i2c_msg.addr = 0x39;
	i2c_msg.mode = IR2E71Y_BDIC_I2C_M_R;
	i2c_msg.wlen = 1;
	i2c_msg.rlen = 1;
	i2c_msg.wbuf = &i2c_wbuf[0];
	i2c_msg.rbuf = &i2c_rbuf[0];

	for(nRetry=0;nRetry<5;nRetry++) {
		nResult = ir2e71y_read_bdic_i2c(&i2c_msg);
		if (nResult == IR2E71Y_RESULT_SUCCESS) {
			/* DEBUG_SENSOR_LOG("I2cRead : nResult = %d", nResult); */
			break;
		}
		DEBUG_SENSOR_LOG("I2cReadError retry : nResult = %d", nResult);
		if (nRetry < 4) msleep(20);
	}

	if (nResult != IR2E71Y_RESULT_SUCCESS) {
		DEBUG_ERROR_LOG("I2C_ReadError id = %d  retry = %d", nResult, nRetry);
		return nResult;
	}
	/* DEBUG_SENSOR_LOG("I2cRead(reg:%02X,Data:%02X)", i2c_wbuf[0], i2c_rbuf[0]);*/
	DEBUG_SENSOR_LOG("Addr: %02X  Data:%02X)", i2c_wbuf[0], i2c_rbuf[0]);

	*rData = i2c_rbuf[0];

	FUNC_FIN_LOG();

	return nResult;
}

/* ------------------------------------------------------------------------- */
/* IOECS_Enable                                                              */
/* ------------------------------------------------------------------------- */
static int IOECS_Enable(void)
{
	int			nResult = SH_PROXIMITY_RESULT_SUCCESS;
	int			prox_data = 1;

	struct ir2e71y_prox_params	prox_params;
#ifdef PROX_USE_SMEM
	unsigned short				proxadj[2];
	sharp_smem_common_type		*p_sh_smem_common_type = NULL;
#endif /* PROX_USE_SMEM */
	mutex_lock(&prox_enable_lock);

	FUNC_LOG();

	if (atomic_read(&enable_mode) == 0) {

		if (atomic_read(&dataread_func_flag) != 1) {

#ifdef PROX_USE_SMEM
			memset((void*)proxadj, 0x00, sizeof(proxadj));
			p_sh_smem_common_type = sh_smem_get_common_address();

			if (p_sh_smem_common_type != NULL) {
				if ((p_sh_smem_common_type->shdiag_proxadj[0] == 0) || (p_sh_smem_common_type->shdiag_proxadj[0] == 0xFFFF)
				||  (p_sh_smem_common_type->shdiag_proxadj[1] == 0) || (p_sh_smem_common_type->shdiag_proxadj[1] == 0xFFFF)) {
					prox_params.threshold_low  = 0xFFFF;
					prox_params.threshold_high = 0xFFFF;
					DEBUG_LOG("[%s][local] LT:0x%04x, HT:0x%04x.\n", __func__, prox_params.threshold_low, prox_params.threshold_high);
				} else {
					memcpy(proxadj, p_sh_smem_common_type->shdiag_proxadj, sizeof(proxadj));
					prox_params.threshold_low  = proxadj[0];
					prox_params.threshold_high = proxadj[1];
					DEBUG_LOG("[%s][smem] LT:0x%04x, HT:0x%04x.\n", __func__, prox_params.threshold_low, prox_params.threshold_high);
				}
			}
			else {
				prox_params.threshold_low  = Threshold_Low;
				prox_params.threshold_high = Threshold_High;
				DEBUG_LOG("[%s][local] LT:0x%04x, HT:0x%04x.\n", __func__, prox_params.threshold_low, prox_params.threshold_high);
			}
#else   /* PROX_USE_SMEM */
			prox_params.threshold_low  = Threshold_Low;
			prox_params.threshold_high = Threshold_High;
			DEBUG_LOG("[%s][local] LT:0x%04x, HT:0x%04x.\n", __func__, prox_params.threshold_low, prox_params.threshold_high);
#endif  /* PROX_USE_SMEM */

			if (ir2e71y_prox_sensor_pow_ctl(IR2E71Y_PROX_SENSOR_POWER_ON, &prox_params) != IR2E71Y_RESULT_SUCCESS) {
				DEBUG_ERROR_LOG("PowerON Error");
				nResult = SH_PROXIMITY_RESULT_FAILURE;
				goto func_fin;
			}
		}

		prox_data = gpio_get_value(poProximityRec->irq_gpio);
		if (prox_data == SH_PROXIMITY_NEAR){ /*Near*/
			atomic_set(&sensor_data, SH_PROXIMITY_NEAR);
		}else {
			atomic_set(&sensor_data, SH_PROXIMITY_FAR);
		}
		DEBUG_SENSOR_LOG("PROX_Enable_SENSOR_DATA = %x",atomic_read(&sensor_data));

		if (atomic_read(&sensor_data) == SH_PROXIMITY_NEAR){ /*Near*/
			irq_set_irq_type(poProximityRec->irq, IRQ_TYPE_LEVEL_HIGH);
			DEBUG_SENSOR_LOG("PROX_Enable_TRIGER HIGH LEVEL");
		}else {
			irq_set_irq_type(poProximityRec->irq, IRQ_TYPE_LEVEL_LOW);
			DEBUG_SENSOR_LOG("PROX_Enable_TRIGER LOW LEVEL");
		}

		input_abs_set_val(poProximityRec->input_dev, ABS_DISTANCE, -1);
		input_report_abs(poProximityRec->input_dev, ABS_DISTANCE, atomic_read(&sensor_data));
		input_sync(poProximityRec->input_dev);

		if (atomic_read(&recovering_flag) == 0) {
			PROX_enable_irq();
		}

		atomic_set(&enable_mode, 1);
	}

func_fin:
	FUNC_FIN_LOG();
	mutex_unlock(&prox_enable_lock);
	return nResult;
}

/* ------------------------------------------------------------------------- */
/* IOECS_Disable                                                             */
/* ------------------------------------------------------------------------- */
static int IOECS_Disable(void)
{
	int nResult = SH_PROXIMITY_RESULT_SUCCESS;

	mutex_lock(&prox_enable_lock);
	atomic_set(&disabling_flag, 1);

	FUNC_LOG();

	cancel_work_sync(&poProximityRec->IrqWork);
	wake_unlock(&prox_interrupt_wake_lock);

	if (atomic_read(&enable_mode) == 1) {

		PROX_disable_irq();
		irq_set_irq_type(poProximityRec->irq, IRQ_TYPE_LEVEL_LOW);
		if (atomic_read(&dataread_func_flag) != 1) {
			if (ir2e71y_prox_sensor_pow_ctl(IR2E71Y_PROX_SENSOR_POWER_OFF, NULL) != IR2E71Y_RESULT_SUCCESS) {
				DEBUG_ERROR_LOG("PowerOFF Error");
				nResult = SH_PROXIMITY_RESULT_FAILURE;
				goto func_fin;
			}
		}
		atomic_set(&enable_mode, 0);
		atomic_set(&recovering_flag, 0);
	}

func_fin:
	atomic_set(&disabling_flag, 0);

	FUNC_FIN_LOG();
	mutex_unlock(&prox_enable_lock);
	return nResult;
}

/* ------------------------------------------------------------------------- */
/* IOECS_SetCycle                                                            */
/* ------------------------------------------------------------------------- */
static int IOECS_SetCycle(short cycle_data)
{
	FUNC_LOG();

	FUNC_FIN_LOG();

	return SH_PROXIMITY_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* IOECS_GetVO_DATA                                                          */
/* ------------------------------------------------------------------------- */
static int IOECS_GetVO_DATA(uint8_t *cycle_data)
{
	int nResult = SH_PROXIMITY_RESULT_SUCCESS;

	FUNC_LOG();

	if (cycle_data == NULL) {
		DEBUG_ERROR_LOG("[IOECS_GetVO_DATA NULL_Error : cycle_data]");
		nResult = SH_PROXIMITY_RESULT_FAILURE;
	}

	FUNC_FIN_LOG();

	return nResult;
}

/* ------------------------------------------------------------------------- */
/* IOECS_GetD2_DATA                                                          */
/* ------------------------------------------------------------------------- */
static int IOECS_GetD2_DATA(unsigned short *psresult)
{
	int nResult = SH_PROXIMITY_RESULT_SUCCESS;
	unsigned char lData  = 0x00;
	unsigned short LData = 0x00;
	unsigned char mData  = 0x00;
	unsigned short MData = 0x00;

	FUNC_LOG();

	if (psresult == NULL) {
		DEBUG_ERROR_LOG("[IOECS_GetD2_DATA NULL_Error : psresult]");
		return SH_PROXIMITY_RESULT_FAILURE;
	}

	if (atomic_read(&enable_mode) == 1) {

		nResult = PROX_I2cRead(GP2AP030_REG_D2_LSB,&lData);

		if (nResult != 0) {
			DEBUG_ERROR_LOG("I2CRead_Error");
			return SH_PROXIMITY_RESULT_FAILURE;
		}

		LData = ( lData & 0x00FF );
		nResult = PROX_I2cRead(GP2AP030_REG_D2_MSB,&mData);

		if (nResult != 0) {
			DEBUG_ERROR_LOG("I2CRead_Error");
			return SH_PROXIMITY_RESULT_FAILURE;
		}

		MData = ( mData & 0x00FF );
		*psresult = (( MData<<8 ) | LData );
		DEBUG_SENSOR_LOG("psresult = 0x%04x",*psresult);
	}

	FUNC_FIN_LOG();
	return SH_PROXIMITY_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* IOECS_LT_Thresshold_Write                                                 */
/* ------------------------------------------------------------------------- */
static int IOECS_LT_Thresshold_Write(unsigned short lt_threshold)
{
	FUNC_LOG();

	Threshold_Low = lt_threshold;

	FUNC_FIN_LOG();
	return SH_PROXIMITY_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* IOECS_HT_Thresshold_Write                                                 */
/* ------------------------------------------------------------------------- */
static int IOECS_HT_Thresshold_Write(unsigned short ht_threshold)
{
	FUNC_LOG();

	Threshold_High = ht_threshold;

	FUNC_FIN_LOG();
	return SH_PROXIMITY_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* PROX_open                                                                 */
/* ------------------------------------------------------------------------- */
static int PROX_open(struct inode *inode, struct file *filp)
{
	int nResult = SH_PROXIMITY_RESULT_FAILURE;

	FUNC_LOG();

	if (inode == NULL) {
		DEBUG_ERROR_LOG("[PROX_open NULL_Error : inode]");
		return SH_PROXIMITY_RESULT_FAILURE;
	}

	if (filp == NULL) {
		DEBUG_ERROR_LOG("[PROX_open NULL_Error : filp]");
		return SH_PROXIMITY_RESULT_FAILURE;
	}

	if (atomic_cmpxchg(&open_flag, 0, 1) == 0) {
		/* Init = Far */
		atomic_set(&sensor_data, SH_PROXIMITY_FAR);
		nResult = 0;
	}else{
		DEBUG_ERROR_LOG("[PROX_open flag_Error : open_flag ]");
	}

	FUNC_FIN_LOG();
	return nResult;
}

/* ------------------------------------------------------------------------- */
/* PROX_release                                                              */
/* ------------------------------------------------------------------------- */
static int PROX_release(struct inode *inode, struct file *filp)
{
	FUNC_LOG();

	if (inode == NULL) {
		DEBUG_ERROR_LOG("[PROX_release NULL_Error : inode]");
		return SH_PROXIMITY_RESULT_FAILURE;
	}

	if (filp == NULL) {
		DEBUG_ERROR_LOG("[PROX_release NULL_Error : filp]");
		return SH_PROXIMITY_RESULT_FAILURE;
	}

	if (atomic_read(&dataread_func_flag) != 1){
		IOECS_Disable();
	}

	atomic_set(&open_flag, 0);

	wake_unlock(&prox_timeout_wake_lock);

	FUNC_FIN_LOG();
	return SH_PROXIMITY_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* PROX_read                                                                 */
/* ------------------------------------------------------------------------- */
static ssize_t PROX_read(struct file *filp, char __user *buf, size_t count, loff_t *ppos)
{
	char tmp;

	FUNC_LOG();

	if (filp == NULL) {
		DEBUG_ERROR_LOG("[PROX_read NULL_Error : filp]");
		return SH_PROXIMITY_RESULT_FAILURE;
	}

	if (buf == NULL) {
		DEBUG_ERROR_LOG("[PROX_read NULL_Error : buf]");
		return SH_PROXIMITY_RESULT_FAILURE;
	}

	if (ppos == NULL) {
		DEBUG_ERROR_LOG("[PROX_read NULL_Error : ppos]");
		return SH_PROXIMITY_RESULT_FAILURE;
	}

	if (prox_dbg_mode == 0) {
		tmp = (char)atomic_read(&sensor_data);
	}
	else {
		tmp = (char)sensor_data_tmp;
	}

	DEBUG_SENSOR_LOG("PROX_read_SENSOR_DATA = %x",tmp);

	if (copy_to_user(buf, &tmp, sizeof(tmp))) {
		return -EFAULT;
	}

	FUNC_FIN_LOG();

	return SH_PROXIMITY_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* PROX_ioctl                                                                */
/* ------------------------------------------------------------------------- */
static long PROX_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	uint8_t bData;
	short cycle;
	unsigned short lt_threshold;
	unsigned short ht_threshold;
	unsigned short psresult;

	FUNC_LOG();

	if (filp == NULL) {
		DEBUG_ERROR_LOG("[PROX_ioctl NULL_Error : filp]");
		return SH_PROXIMITY_RESULT_FAILURE;
	}

	switch (cmd) {
		case ECS_IOCTL_SET_CYCLE:
			DEBUG_LOG("ECS_IOCTL_SET_CYCLE");
			if (copy_from_user(&cycle, argp, sizeof(cycle))) {
				DEBUG_ERROR_LOG("ECS_IOCTL_SET_CYCLE ERR");
				return -EFAULT;
			}
			break;
		case ECS_IOCTL_LT_THRESHOLD_WRITE:
			DEBUG_LOG("ECS_IOCTL_LT_THRESHOLD_WRITE");
			if (copy_from_user(&lt_threshold, argp, sizeof(lt_threshold))) {
				DEBUG_ERROR_LOG("ECS_IOCTL_LT_THRESHOLD_WRITE ERR");
				return -EFAULT;
			}
			break;
		case ECS_IOCTL_HT_THRESHOLD_WRITE:
			DEBUG_LOG("ECS_IOCTL_HT_THRESHOLD_WRITE");
			if (copy_from_user(&ht_threshold, argp, sizeof(ht_threshold))) {
				DEBUG_ERROR_LOG("ECS_IOCTL_HT_THRESHOLD_WRITE ERR");
				return -EFAULT;
			}
			break;
		case ECS_IOCTL_GET_D2_DATA:
			DEBUG_LOG("ECS_IOCTL_GET_D2_DATA");
			if (copy_from_user(&psresult, argp, sizeof(psresult))) {
				DEBUG_ERROR_LOG("ECS_IOCTL_GET_D2_DATA ERR");
				return -EFAULT;
			}
			break;
		default:
			break;
	}

	switch (cmd) {
		case ECS_IOCTL_ENABLE:
			DEBUG_LOG("ECS_IOCTL_ENABLE");
			if (IOECS_Enable() < 0) {
			   DEBUG_ERROR_LOG("ECS_IOCTL_ENABLE ERR");
				return -EIO;
			}
			break;
		case ECS_IOCTL_DISABLE:
			DEBUG_LOG("ECS_IOCTL_DISABLE");
			if (IOECS_Disable() < 0) {
				DEBUG_ERROR_LOG("ECS_IOCTL_DISABLE ERR");
				return -EIO;
			}
			break;
		case ECS_IOCTL_SET_CYCLE:
			DEBUG_LOG("ECS_IOCTL_SET_CYCLE");
			if (IOECS_SetCycle(cycle) < 0) {
			   DEBUG_ERROR_LOG("ECS_IOCTL_SET_CYCLE ERR");
				return -EIO;
			}
			break;
		case ECS_IOCTL_GET_VO_DATA:
			DEBUG_LOG("ECS_IOCTL_GET_VO_DATA");
			if (IOECS_GetVO_DATA(&bData) < 0) {
			   DEBUG_ERROR_LOG("ECS_IOCTL_GET_VO_DATA ERR");
				return -EIO;
			}
			break;
		case ECS_IOCTL_GET_D2_DATA:
			DEBUG_LOG("ECS_IOCTL_GET_D2_DATA");
			if (IOECS_GetD2_DATA(&psresult) < 0) {
			   DEBUG_ERROR_LOG("ECS_IOCTL_GET_D2_DATA ERR");
				return -EIO;
			}
			break;
		case ECS_IOCTL_LT_THRESHOLD_WRITE:
			DEBUG_LOG("ECS_IOCTL_LT_THRESHOLD_WRITE");
			if (IOECS_LT_Thresshold_Write(lt_threshold) < 0) {
			   DEBUG_ERROR_LOG("ECS_IOCTL_LT_THRESHOLD_WRITE ERR");
				return -EIO;
			}
			break;
		case ECS_IOCTL_HT_THRESHOLD_WRITE:
			DEBUG_LOG("ECS_IOCTL_HT_THRESHOLD_WRITE");
			if (IOECS_HT_Thresshold_Write(ht_threshold) < 0) {
			   DEBUG_ERROR_LOG("ECS_IOCTL_HT_THRESHOLD_WRITE ERR");
				return -EIO;
			}
			break;
		default:
			break;
	}

	switch (cmd) {
		case ECS_IOCTL_GET_VO_DATA:
			DEBUG_LOG("ECS_IOCTL_GET_VO_DATA");
			if (copy_to_user(argp, &bData, sizeof(bData))) {
				DEBUG_ERROR_LOG("ECS_IOCTL_GET_VO_DATA ERR");
				return -EFAULT;
			}
			break;
		case ECS_IOCTL_GET_D2_DATA:
			DEBUG_LOG("ECS_IOCTL_GET_D2_DATA");
			if (copy_to_user(argp, &psresult, sizeof(psresult))) {
				DEBUG_ERROR_LOG("ECS_IOCTL_GET_D2_DATA ERR");
				return -EFAULT;
			}
			break;
		default:
			break;
	}

	FUNC_FIN_LOG();
	return SH_PROXIMITY_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* PROX_interrupt                                                            */
/* ------------------------------------------------------------------------- */
static irqreturn_t PROX_interrupt(int irq, void *dev_id)
{
	FUNC_LOG();

	if (atomic_read(&disabling_flag) == 0 && atomic_read(&recovering_flag) == 0) {
		PROX_disable_irq();
		schedule_work(&poProximityRec->IrqWork);
		wake_lock(&prox_interrupt_wake_lock);
	}

	FUNC_FIN_LOG();

	return IRQ_HANDLED;
}

/* ------------------------------------------------------------------------- */
/* PROX_Irq_workfunc                                                         */
/* ------------------------------------------------------------------------- */
static void PROX_Irq_workfunc( WorkStruct *work )
{
	int prox_data = 1;

	FUNC_LOG();

	mutex_lock(&prox_val_lock);
	prox_data = gpio_get_value(poProximityRec->irq_gpio);
	if (prox_data == SH_PROXIMITY_NEAR) { /*Near*/
		atomic_set(&sensor_data, SH_PROXIMITY_NEAR);
	}else {
		atomic_set(&sensor_data, SH_PROXIMITY_FAR);
	}
	DEBUG_SENSOR_LOG("PROX_Irq_workfunc_SENSOR_DATA = %x\n",atomic_read(&sensor_data));

	if (atomic_read(&sensor_data) == SH_PROXIMITY_NEAR) { /*Near*/
		irq_set_irq_type(poProximityRec->irq, IRQ_TYPE_LEVEL_HIGH);
	}else {
		irq_set_irq_type(poProximityRec->irq, IRQ_TYPE_LEVEL_LOW);
	}

	wake_lock_timeout(&prox_timeout_wake_lock, 1 * HZ);

	if (atomic_read(&recovering_flag) == 0) {
		input_report_abs(poProximityRec->input_dev, ABS_DISTANCE, atomic_read(&sensor_data));
		input_sync(poProximityRec->input_dev);
	}

	PROX_enable_irq();

	FUNC_FIN_LOG();

	wake_unlock(&prox_interrupt_wake_lock);
	mutex_unlock(&prox_val_lock);
}

/* ------------------------------------------------------------------------- */
/* PROX_recovery_start_func                                                  */
/* ------------------------------------------------------------------------- */
int PROX_recovery_start_func(void)
{
	mutex_lock(&prox_enable_lock);

	FUNC_LOG();

	atomic_set(&recovering_flag, 1);

	cancel_work_sync(&poProximityRec->IrqWork);
	wake_unlock(&prox_interrupt_wake_lock);

	PROX_disable_irq();

	FUNC_FIN_LOG();

	mutex_unlock(&prox_enable_lock);
	return 0;
}

/* ------------------------------------------------------------------------- */
/* PROX_recovery_end_func                                                    */
/* ------------------------------------------------------------------------- */
int PROX_recovery_end_func(void)
{
	mutex_lock(&prox_enable_lock);

	FUNC_LOG();

	if (atomic_read(&enable_mode) != 0) {
		atomic_set(&recovering_flag, 0);
		PROX_enable_irq();
	}

    FUNC_FIN_LOG();

	mutex_unlock(&prox_enable_lock);
	return 0;
}

/* ------------------------------------------------------------------------- */
/* PROX_updata_val                                                           */
/* ------------------------------------------------------------------------- */
static void PROX_updata_val(WorkStruct *work)
{
	int prox_data = 1;

	FUNC_LOG();

	mutex_lock(&prox_val_lock);

	prox_data = gpio_get_value(poProximityRec->irq_gpio);
	if (prox_data == SH_PROXIMITY_NEAR) { /*Near*/
		atomic_set(&sensor_data, SH_PROXIMITY_NEAR);
	}else {
		atomic_set(&sensor_data, SH_PROXIMITY_FAR);
	}
	DEBUG_SENSOR_LOG("PROX_updata_val_SENSOR_DATA = %x\n",atomic_read(&sensor_data));

	wake_lock_timeout(&prox_timeout_wake_lock, 1 * HZ);

	input_report_abs(poProximityRec->input_dev, ABS_DISTANCE, atomic_read(&sensor_data));
	input_sync(poProximityRec->input_dev);

	if (atomic_read(&sensor_data) == SH_PROXIMITY_NEAR) { /*Near*/
		irq_set_irq_type(poProximityRec->irq, IRQ_TYPE_LEVEL_HIGH);
	}else {
		irq_set_irq_type(poProximityRec->irq, IRQ_TYPE_LEVEL_LOW);
	}

	mutex_unlock(&prox_val_lock);

	FUNC_FIN_LOG();
}

/* ------------------------------------------------------------------------- */
/* PROX_ConfigGPIO                                                           */
/* ------------------------------------------------------------------------- */
static int prox_parse_dt(struct device *dev)
{
	struct device_node *np = dev->of_node;

	FUNC_LOG();

	poProximityRec->irq_gpio = of_get_named_gpio(np, "qcom,prox-int-gpio" ,0);
	poProximityRec->irq = gpio_to_irq(poProximityRec->irq_gpio);

	return 0;
}

/* ------------------------------------------------------------------------- */
/* PROX_Initialize                                                           */
/* ------------------------------------------------------------------------- */
static int PROX_Initialize(void)
{
	FUNC_LOG();

	Threshold_Low  = 0x012c;
	Threshold_High = 0x0190;

	FUNC_FIN_LOG();
	return SH_PROXIMITY_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* PROX_Remove                                                               */
/* ------------------------------------------------------------------------- */
static int PROX_Remove(struct platform_device *pdev)
{
	FUNC_LOG();

	wake_unlock(&prox_interrupt_wake_lock);
	wake_lock_destroy(&prox_interrupt_wake_lock);
	wake_unlock(&prox_timeout_wake_lock);
	wake_lock_destroy(&prox_timeout_wake_lock);

	input_free_device(poProximityRec->input_dev);
	misc_deregister(&PROX_device);
	kfree(poProximityRec);

	FUNC_FIN_LOG();
	return SH_PROXIMITY_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* PROX_Probe                                                                */
/* ------------------------------------------------------------------------- */
static int PROX_Probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int nResult = 0;

	FUNC_LOG();
	if(dev) {
		DEBUG_SENSOR_LOG("alloc for platform data");

		poProximityRec = kzalloc(sizeof(drv_data), GFP_KERNEL);
		if (!poProximityRec) {
			DEBUG_ERROR_LOG("memory allocation failed.");
			nResult = -ENOMEM;
			goto memory_error;
		}
	}
	else {
		DEBUG_ERROR_LOG("memory allocation failed.");
		nResult = -ENOMEM;
	}

	INIT_WORK(&poProximityRec->IrqWork, PROX_Irq_workfunc);
	INIT_WORK(&prox_updata_val_work, PROX_updata_val);

	nResult = prox_parse_dt(dev);
	if (nResult < 0) {
		DEBUG_ERROR_LOG("PROX GPIO Config error_Result = %d" , nResult );
		goto configgpio_error;
	}

	nResult = PROX_Initialize();
	if (nResult < 0) {
		DEBUG_ERROR_LOG("initialize failed.");
		goto initialize_error;
	}

	nResult = misc_register(&PROX_device);
	if (nResult) {
		DEBUG_ERROR_LOG("misc_register failed.");
		goto misc_register_error;
	}

	/* irq will not be enabled on request irq */
	irq_set_status_flags(poProximityRec->irq, IRQ_NOAUTOEN);
	prox_irq_status = PROX_IRQ_DISABLED;

	/* IRQ */
	nResult = request_irq(poProximityRec->irq, PROX_interrupt, IRQF_TRIGGER_LOW, "PROX_VOUT", poProximityRec);
	if (nResult < 0) {
		DEBUG_ERROR_LOG("request_irq_Result  %d" , nResult );
		goto request_irq_error;
	}

	/* Declare input device */
	poProximityRec->input_dev = input_allocate_device();
	if (!poProximityRec->input_dev) {
		nResult = -ENOMEM;
		DEBUG_ERROR_LOG("Failed to allocate input device.");
		goto input_dev_error;
	}

	/* Setup input device */
	set_bit(EV_ABS, poProximityRec->input_dev->evbit);

	/* proximity value near=7, far=0 */
	input_set_abs_params(poProximityRec->input_dev, ABS_DISTANCE, 0, 7, 0, 0);
	input_set_abs_params(poProximityRec->input_dev, ABS_RUDDER, 0, 0x1, 0, 0);
	/* Set name */
	poProximityRec->input_dev->name = "proximity";

	/* Register */
	nResult = input_register_device(poProximityRec->input_dev);
	if (nResult) {
		DEBUG_ERROR_LOG("Unable to register input device.");
		goto input_register_error;
	}

	wake_lock_init(&prox_timeout_wake_lock, WAKE_LOCK_SUSPEND, "prox_timeout_wake_lock");
	wake_lock_init(&prox_interrupt_wake_lock, WAKE_LOCK_SUSPEND, "proximity_wake_lock");
	mutex_init(&prox_val_lock);
	mutex_init(&prox_enable_lock);
	spin_lock_init(&lock);

	FUNC_FIN_LOG();
	return SH_PROXIMITY_RESULT_SUCCESS;

input_register_error:
	input_free_device(poProximityRec->input_dev);
input_dev_error:
request_irq_error:
	misc_deregister(&PROX_device);
misc_register_error:
initialize_error:
configgpio_error:
	kfree(poProximityRec);
memory_error:
	return nResult;
}

/* ------------------------------------------------------------------------- */
/* PROX_disable_irq                                                          */
/* ------------------------------------------------------------------------- */
static void PROX_disable_irq(void)
{
	unsigned long flags;

	spin_lock_irqsave(&lock, flags);

	FUNC_LOG();

	if (prox_irq_status == PROX_IRQ_ENABLED) {
		disable_irq_wake(poProximityRec->irq);
		disable_irq_nosync(poProximityRec->irq);
		prox_irq_status = PROX_IRQ_DISABLED;
	}

	FUNC_FIN_LOG();

	spin_unlock_irqrestore(&lock, flags);
	return;
}

/* ------------------------------------------------------------------------- */
/* PROX_enable_irq                                                           */
/* ------------------------------------------------------------------------- */
static void PROX_enable_irq(void)
{
	unsigned long flags;

	spin_lock_irqsave(&lock, flags);

	FUNC_LOG();

	if (prox_irq_status == PROX_IRQ_DISABLED) {
		enable_irq_wake(poProximityRec->irq);
		enable_irq(poProximityRec->irq);
		prox_irq_status = PROX_IRQ_ENABLED;
	}

	FUNC_FIN_LOG();

	spin_unlock_irqrestore(&lock, flags);
	return;
}

/* ------------------------------------------------------------------------- */
/* PROX_Init                                                                 */
/* ------------------------------------------------------------------------- */
static struct of_device_id prox_match_table[] = {
	{ .compatible = "sharp,gp2a-prox", },
	{},
};

static struct platform_driver gp2a_prox_driver = {
	.probe  = PROX_Probe,
	.remove = PROX_Remove,
	.driver = {
		.owner = THIS_MODULE,
		.name  = PROX_DRIVER_NAME,
		.of_match_table = prox_match_table,
	},
};

static int __init PROX_Init(void)
{
	return platform_driver_register(&gp2a_prox_driver);
}

/* ------------------------------------------------------------------------- */
/* PROX_Exit                                                                 */
/* ------------------------------------------------------------------------- */
static void __exit PROX_Exit(void)
{
	platform_driver_unregister(&gp2a_prox_driver);
}


module_init(PROX_Init);
module_exit(PROX_Exit);

MODULE_DESCRIPTION("proximity sensor driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("SHARP CORPORATION");
MODULE_VERSION("1.0");

/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */
