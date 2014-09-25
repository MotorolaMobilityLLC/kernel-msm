/*
 * Copyright (C) 2012-2014 Motorola, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/mutex.h>
#include <linux/wakelock.h>
#include <linux/i2c.h>
#include <linux/slab.h>

#include <linux/debugfs.h>
#include <linux/seq_file.h>

#include <linux/m4sensorhub.h>

#ifdef CONFIG_PM_DEEPSLEEP
#include <linux/suspend.h>
#endif

#define NUM_INT_REGS      3
#define NUM_INTS_PER_REG  8
#define NUM_INTS_LAST_REG (((M4SH_IRQ__NUM-1)%NUM_INTS_PER_REG)+1)
#define INTR_VALID_BITS(n) (unsigned char)((1 << (n)) - 1)

#define EVENT_MASK(event) (1 << ((event) % NUM_INTS_PER_REG))

#define DBG_BUF_LINE_LEN  80

/* --------------- Global Declarations -------------- */

/* ------------ Local Function Prototypes ----------- */
static unsigned short get_enable_reg(enum m4sensorhub_irqs event);
static irqreturn_t event_threaded(int irq, void *devid);
#ifdef CONFIG_DEBUG_FS
static int m4sensorhub_dbg_irq_open(struct inode *inode, struct file *file);
#endif
static void m4sensorhub_irq_restore(struct m4sensorhub_data *m4sensorhub,
	void *data);

/* ---------------- Local Declarations -------------- */

static const char *irq_name[] = {
	[M4SH_IRQ_TMP_DATA_READY]         = "TMP_DATA_READY",
	[M4SH_IRQ_PRESSURE_DATA_READY]    = "PRES_DATA_READY",
	[M4SH_IRQ_GYRO_DATA_READY]        = "GYRO_DATA_READY",
	[M4SH_IRQ_PEDOMETER_DATA_READY]   = "PEDO_DATA_READY",
	[M4SH_IRQ_COMPASS_DATA_READY]     = "COMPASS_DATA_READY",
	[M4SH_IRQ_FUSION_DATA_READY]      = "FUSION_DATA_READY",
	[M4SH_IRQ_ACCEL_DATA_READY]       = "ACCEL_DATA_READY",
	[M4SH_IRQ_GESTURE_DETECTED]       = "GESTURE_DETECTED",
	[M4SH_IRQ_STILL_DETECTED]         = "STILL_DETECTED",
	[M4SH_IRQ_MOTION_DETECTED]        = "MOTION_DETECTED",
	[M4SH_IRQ_ACTIVITY_CHANGE]        = "ACTIVITY_CHANGE",
	[M4SH_IRQ_DLCMD_RESP_READY]       = "DLCMD_RESP_READY",
	[M4SH_IRQ_MIC_DATA_READY]         = "MIC_DATA_READY",
	[M4SH_IRQ_WRIST_READY]            = "WRIST_READY",
	[M4SH_IRQ_PASSIVE_BUFFER_FULL]    = "PASSIVE_BUFFER_FULL",
	[M4SH_IRQ_LIGHTSENSOR_DATA_READY] = "ALS_DATA_READY",
	[M4SH_IRQ_HRSENSOR_DATA_READY]    = "HRSENSOR_DATA_READY",
	[M4SH_IRQ_AP_ALARM_EXPIRED]       = "AP_ALARM_EXPIRED",
	[M4SH_IRQ_HEARTRATE_DATA_READY]   = "HR_DATA_READY",
};

/* -------------- Local Data Structures ------------- */

struct m4sensorhub_event_handler {
	void (*func)(enum m4sensorhub_irqs, void *);
	void *data;
};

struct m4sensorhub_irq_info {
	uint8_t registered;
	uint8_t enabled;
	uint32_t ena_fired;
	uint32_t disa_fired;
	uint8_t tm_wakelock;
};

struct m4sensorhub_irqdata {
	struct mutex lock;           /* lock event handlers and data */
	struct m4sensorhub_data *m4sensorhub;
	struct m4sensorhub_event_handler event_handler[M4SH_IRQ__NUM];
	struct m4sensorhub_irq_info irq_info[M4SH_IRQ__NUM];
	struct wake_lock wake_lock;
	struct wake_lock tm_wake_lock; /* timeout wakelock */
#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs;
#endif
};


static const struct {
	enum m4sensorhub_reg status_reg;
	enum m4sensorhub_reg enable_reg;
	unsigned char valid_bits;
} int_registers[NUM_INT_REGS] = {
	{M4SH_REG_GENERAL_INTERRUPT0STATUS,
	 M4SH_REG_GENERAL_INTERRUPT0ENABLE,
	 INTR_VALID_BITS(NUM_INTS_PER_REG)},
	{M4SH_REG_GENERAL_INTERRUPT1STATUS,
	M4SH_REG_GENERAL_INTERRUPT1ENABLE,
	INTR_VALID_BITS(NUM_INTS_PER_REG)},
	{M4SH_REG_GENERAL_INTERRUPT2STATUS,
	M4SH_REG_GENERAL_INTERRUPT2ENABLE,
	INTR_VALID_BITS(NUM_INTS_LAST_REG)},
};

static irqreturn_t event_isr(int irq, void *data)
{
	struct m4sensorhub_irqdata *irq_data = data;

	wake_lock(&irq_data->wake_lock);

	return IRQ_WAKE_THREAD;
}

#ifdef CONFIG_DEBUG_FS
static const struct file_operations debug_fops = {
	.open    = m4sensorhub_dbg_irq_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};
#endif

/* -------------- Global Functions ----------------- */

/* m4sensorhub_irq_init()

   Intialize M4 sensor hub IRQ subsystem

   Returns 0 on success.  Returns negative error code on failure

     m4sensorhub - pointer to the main m4sensorhub data struct
*/
int m4sensorhub_irq_init(struct m4sensorhub_data *m4sensorhub)
{
	int retval = 0;
	struct i2c_client *i2c = NULL;
	struct m4sensorhub_irqdata *data = NULL;

	if (m4sensorhub == NULL) {
		KDEBUG(M4SH_ERROR, "%s: M4 data is NULL\n", __func__);
		retval = -ENODATA;
		goto done;
	} else if (m4sensorhub->i2c_client == NULL) {
		KDEBUG(M4SH_ERROR, "%s: I2C client is missing\n", __func__);
		retval = -ENODATA;
		goto done;
	}

	i2c = m4sensorhub->i2c_client;

	data = kzalloc(sizeof(struct m4sensorhub_irqdata), GFP_KERNEL);
	if (data == NULL) {
		KDEBUG(M4SH_ERROR, "%s: Failed to allocat irqdata\n", __func__);
		retval = -ENOMEM;
		goto done;
	}

	mutex_init(&data->lock);

	data->m4sensorhub = m4sensorhub;
	m4sensorhub->irqdata = data;

	KDEBUG(M4SH_INFO, "%s: %u IRQs with valid_bits %02X%02X%02X\n",
		__func__, M4SH_IRQ__NUM, int_registers[2].valid_bits,
		int_registers[1].valid_bits, int_registers[0].valid_bits);
	retval = m4sensorhub_irq_disable_all(m4sensorhub);
	if (retval < 0) {
		KDEBUG(M4SH_ERROR, "%s: Failed disable all irqs\n", __func__);
		goto err_free;
	}

	wake_lock_init(&data->wake_lock, WAKE_LOCK_SUSPEND, "m4sensorhub-irq");
	wake_lock_init(&data->tm_wake_lock, WAKE_LOCK_SUSPEND,
		       "m4sensorhub-timed-irq");

	retval = request_threaded_irq(i2c->irq, event_isr, event_threaded,
		IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
		"m4sensorhub-irq", data);
	if (retval) {
		KDEBUG(M4SH_ERROR, "%s: Failed requesting irq.\n", __func__);
		goto err_destroy_wq;
	}

	retval = enable_irq_wake(i2c->irq);
	if (retval) {
		KDEBUG(M4SH_ERROR, "%s: Failed enabling irq wake.\n", __func__);
		goto err_free_irq;
	}

#ifdef CONFIG_DEBUG_FS
	data->debugfs = debugfs_create_file("m4sensorhub-irq", S_IRUGO, NULL,
					    data, &debug_fops);
	if (data->debugfs == NULL) {
		KDEBUG(M4SH_ERROR, "%s: Error creating debufs\n", __func__);
		retval = -EINVAL;
		goto err_disa_irq;
	}
#endif
	m4sensorhub_panic_register(m4sensorhub, PANICHDL_IRQ_RESTORE,
		m4sensorhub_irq_restore, data);
	KDEBUG(M4SH_INFO, "%s: m4sensorhub IRQ subsystem initialized\n",
		__func__);
	retval = 0;
	goto done;

#ifdef CONFIG_DEBUG_FS
err_disa_irq:
#endif
	disable_irq_wake(i2c->irq);
err_free_irq:
	free_irq(i2c->irq, data);
err_destroy_wq:
	wake_lock_destroy(&data->wake_lock);
	wake_lock_destroy(&data->tm_wake_lock);
err_free:
	mutex_destroy(&data->lock);
	m4sensorhub->irqdata = NULL;
	data->m4sensorhub = NULL;
	kfree(data);
done:
	return retval;
}
EXPORT_SYMBOL_GPL(m4sensorhub_irq_init);

/* m4sensorhub_irq_shutdown()

   Shutdown the M4 sensor hub IRQ subsystem

     m4sensorhub - pointer to the main m4sensorhub data struct
*/
void m4sensorhub_irq_shutdown(struct m4sensorhub_data *m4sensorhub)
{
	struct i2c_client *i2c = NULL;
	struct m4sensorhub_irqdata *data = NULL;

	KDEBUG(M4SH_INFO, "%s: shutdown m4sensorhub IRQ subsystem\n",
		__func__);

	if (m4sensorhub == NULL) {
		KDEBUG(M4SH_ERROR, "%s: M4 data is NULL\n", __func__);
		return;
	} else if (m4sensorhub->i2c_client) {
		KDEBUG(M4SH_ERROR, "%s: I2C client is NULL\n", __func__);
		return;
	} else if (m4sensorhub->irqdata) {
		KDEBUG(M4SH_ERROR, "%s: IRQ data is NULL\n", __func__);
		return;
	}

	i2c = m4sensorhub->i2c_client;
	data = m4sensorhub->irqdata;

	m4sensorhub_panic_unregister(m4sensorhub, PANICHDL_IRQ_RESTORE);

#ifdef CONFIG_DEBUG_FS
	debugfs_remove(data->debugfs);
#endif

	disable_irq_wake(i2c->irq);
	free_irq(i2c->irq, data);

	m4sensorhub->irqdata = NULL;
	data->m4sensorhub = NULL;

	if (wake_lock_active(&data->wake_lock))
		wake_unlock(&data->wake_lock);
	wake_lock_destroy(&data->wake_lock);

	if (wake_lock_active(&data->tm_wake_lock))
		wake_unlock(&data->tm_wake_lock);
	wake_lock_destroy(&data->tm_wake_lock);

	if (mutex_is_locked(&data->lock))
		mutex_unlock(&data->lock);
	mutex_destroy(&data->lock);

	kfree(data);
}
EXPORT_SYMBOL_GPL(m4sensorhub_irq_shutdown);

/* m4sensorhub_irq_register()

   Register an interupt handler in the M4 Sensor Hub IRQ subsystem.
   This does not enable the IRQ, that needs to be done by caller
   with m4sensorhub_irq_enable()

   Returns 0 on success.  Returns negative error code on failure

     m4sensorhub - pointer to the main m4sensorhub data struct
     irq - M4 Sensor Hub interupt to resiter for
     cb_func - IRQ handler function to execute on inturrupt
     data - pointer to data for IRQ handler function
*/
int m4sensorhub_irq_register(struct m4sensorhub_data *m4sensorhub,
			     enum m4sensorhub_irqs irq,
			     void (*cb_func) (enum m4sensorhub_irqs, void *),
			     void *data, uint8_t enable_timed_wakelock)
{
	struct m4sensorhub_irqdata *irqdata = NULL;
	int retval = 0;

	if (m4sensorhub == NULL) {
		KDEBUG(M4SH_ERROR, "%s: M4 data is NULL\n", __func__);
		retval = -ENODATA;
		goto m4sensorhub_irq_register_exit;
	} else if (irq >= M4SH_IRQ__NUM) {
		KDEBUG(M4SH_ERROR, "%s: IRQ %d exceeds maximum %d\n",
			__func__, irq, M4SH_IRQ__NUM);
		retval = -EINVAL;
		goto m4sensorhub_irq_register_exit;
	} else if (cb_func == NULL) {
		KDEBUG(M4SH_ERROR, "%s: Handler missing for IRQ %d\n",
			__func__, irq);
		retval = -ENOSYS;
		goto m4sensorhub_irq_register_exit;
	}

	irqdata = m4sensorhub->irqdata;
	if (irqdata == NULL) {
		KDEBUG(M4SH_ERROR, "%s: Caller irqdata is NULL (irq=%d)\n",
			__func__, irq);
		retval = -ENODATA;
		goto m4sensorhub_irq_register_exit;
	}

	mutex_lock(&irqdata->lock);

	if (irqdata->event_handler[irq].func == NULL) {
		irqdata->irq_info[irq].registered = 1;
		irqdata->irq_info[irq].tm_wakelock = enable_timed_wakelock;
		irqdata->event_handler[irq].func = cb_func;
		irqdata->event_handler[irq].data = data;
		KDEBUG(M4SH_NOTICE, "%s: %s IRQ registered\n",
			__func__, irq_name[irq]);
	} else {
		KDEBUG(M4SH_ERROR, "%s: %s IRQ registration failed\n",
			__func__, irq_name[irq]);
		retval = -EPERM;
		goto m4sensorhub_irq_register_fail;
	}

m4sensorhub_irq_register_fail:
	mutex_unlock(&irqdata->lock);
m4sensorhub_irq_register_exit:
	return retval;
}
EXPORT_SYMBOL_GPL(m4sensorhub_irq_register);

/* m4sensorhub_irq_unregister()

   Unregister an interupt handler in the M4 Sensor Hub IRQ subsystem

   Returns 0 on success.  Returns negative error code on failure

     m4sensorhub - pointer to the main m4sensorhub data struct
     irq - M4 Sensor Hub interupt to unresiter for
*/
int m4sensorhub_irq_unregister(struct m4sensorhub_data *m4sensorhub,
			       enum m4sensorhub_irqs irq)
{
	struct m4sensorhub_irqdata *data = NULL;
	int retval = 0;

	if (m4sensorhub == NULL) {
		KDEBUG(M4SH_ERROR, "%s: M4 data is NULL\n", __func__);
		retval = -ENODATA;
		goto m4sensorhub_irq_unregister_exit;
	} else if (irq >= M4SH_IRQ__NUM) {
		KDEBUG(M4SH_ERROR, "%s: IRQ %d exceeds maximum %d\n",
			__func__, irq, M4SH_IRQ__NUM);
		retval = -ENODATA;
		goto m4sensorhub_irq_unregister_exit;
	} else if (m4sensorhub->irqdata == NULL) {
		KDEBUG(M4SH_ERROR, "%s: IRQ data is NULL\n", __func__);
		retval = -ENODATA;
		goto m4sensorhub_irq_unregister_exit;
	}

	data = m4sensorhub->irqdata;
	retval = m4sensorhub_irq_disable(m4sensorhub, irq);
	if (retval < 0) {
		KDEBUG(M4SH_ERROR, "%s: Failed to disable IRQ %d\n",
			__func__, irq);
		goto m4sensorhub_irq_unregister_exit;
	}

	mutex_lock(&data->lock);
	data->event_handler[irq].func = NULL;
	data->event_handler[irq].data = NULL;
	data->irq_info[irq].registered = 0;
	mutex_unlock(&data->lock);

	KDEBUG(M4SH_NOTICE, "%s: %s IRQ un-registered\n",
			    __func__, irq_name[irq]);

m4sensorhub_irq_unregister_exit:
	return retval;
}
EXPORT_SYMBOL_GPL(m4sensorhub_irq_unregister);

/* m4sensorhub_irq_enable_get()

   Check if an IRQ is enabled

   Returns 1 if enabled, 0 if disabled.
   Returns negative error code on failure

     m4sensorhub - pointer to the main m4sensorhub data struct
     irq - M4 Sensor Hub interupt to check
*/
int m4sensorhub_irq_enable_get(struct m4sensorhub_data *m4sensorhub,
		       enum m4sensorhub_irqs irq)
{
	struct m4sensorhub_irqdata *data = NULL;
	int retval = -EINVAL;

	if (m4sensorhub == NULL) {
		KDEBUG(M4SH_ERROR, "%s: M4 data is NULL\n", __func__);
		retval = -ENODATA;
		goto m4sensorhub_irq_enable_get_fail;
	} else if (irq >= M4SH_IRQ__NUM) {
		KDEBUG(M4SH_ERROR, "%s: IRQ %d exceeds maximum %d\n",
			__func__, irq, M4SH_IRQ__NUM);
		retval = -ENODATA;
		goto m4sensorhub_irq_enable_get_fail;
	} else if (m4sensorhub->irqdata == NULL) {
		KDEBUG(M4SH_ERROR, "%s: IRQ data is NULL\n", __func__);
		retval = -ENODATA;
		goto m4sensorhub_irq_enable_get_fail;
	}

	data = m4sensorhub->irqdata;
	retval = data->irq_info[irq].enabled;

m4sensorhub_irq_enable_get_fail:
	return retval;
}
EXPORT_SYMBOL_GPL(m4sensorhub_irq_enable_get);

/* m4sensorhub_irq_disable()

   Disable M4 Sensor Hub subsystem IRQ

   Returns 0 on success.  Returns negative error code on failure

     m4sensorhub - pointer to the main m4sensorhub data struct
     irq - M4 Sensor Hub interupt to disable
*/
int m4sensorhub_irq_disable(struct m4sensorhub_data *m4sensorhub,
		   enum m4sensorhub_irqs irq)
{
	struct m4sensorhub_irqdata *data = NULL;
	int retval = -EINVAL;
	bool enabled = true;

	if (m4sensorhub == NULL) {
		KDEBUG(M4SH_ERROR, "%s: M4 data is NULL\n", __func__);
		retval = -ENODATA;
		goto m4sensorhub_irq_disable_fail;
	} else if (irq >= M4SH_IRQ__NUM) {
		KDEBUG(M4SH_ERROR, "%s: IRQ %d exceeds maximum %d\n",
			__func__, irq, M4SH_IRQ__NUM);
		retval = -ENODATA;
		goto m4sensorhub_irq_disable_fail;
	} else if (m4sensorhub->irqdata == NULL) {
		KDEBUG(M4SH_ERROR, "%s: IRQ data is NULL\n", __func__);
		retval = -ENODATA;
		goto m4sensorhub_irq_disable_fail;
	}

	data = m4sensorhub->irqdata;
	mutex_lock(&data->lock);
	if (data->irq_info[irq].enabled == 0)
		enabled = false;
	mutex_unlock(&data->lock);

	/* Checking if the IRQ was previously disabled only to print an error */
	if (!enabled) {
		KDEBUG(M4SH_INFO, "%s: IRQ %d is already disabled\n",
			__func__, irq);
		retval = 0;
		goto m4sensorhub_irq_disable_fail;
	}

	retval = m4sensorhub_reg_write_1byte(m4sensorhub,
		get_enable_reg(irq), 0, EVENT_MASK(irq));
	if (retval < 0) {
		KDEBUG(M4SH_ERROR, "%s: Register write failed\n", __func__);
		goto m4sensorhub_irq_disable_fail;
	} else if (retval != 1) {
		KDEBUG(M4SH_ERROR, "%s: Wrote %d bytes instead of 1\n",
			__func__, retval);
		goto m4sensorhub_irq_disable_fail;
	}

	mutex_lock(&data->lock);
	data->irq_info[irq].enabled = 0;
	mutex_unlock(&data->lock);

m4sensorhub_irq_disable_fail:
	return retval;
}
EXPORT_SYMBOL_GPL(m4sensorhub_irq_disable);

/* m4sensorhub_irq_enable()

   Enable M4 Sensor Hub subsystem IRQ

   Returns 0 on success.  Returns negative error code on failure

     m4sensorhub - pointer to the main m4sensorhub data struct
     irq - M4 Sensor Hub interupt to enable
*/
int m4sensorhub_irq_enable(struct m4sensorhub_data *m4sensorhub,
		     enum m4sensorhub_irqs irq)
{
	struct m4sensorhub_irqdata *data = NULL;
	int retval = -EINVAL;
	bool disabled = true;

	if (m4sensorhub == NULL) {
		KDEBUG(M4SH_ERROR, "%s: M4 data is NULL\n", __func__);
		retval = -ENODATA;
		goto m4sensorhub_irq_enable_fail;
	} else if (irq >= M4SH_IRQ__NUM) {
		KDEBUG(M4SH_ERROR, "%s: IRQ %d exceeds maximum %d\n",
			__func__, irq, M4SH_IRQ__NUM);
		retval = -ENODATA;
		goto m4sensorhub_irq_enable_fail;
	} else if (m4sensorhub->irqdata == NULL) {
		KDEBUG(M4SH_ERROR, "%s: IRQ data is NULL\n", __func__);
		retval = -ENODATA;
		goto m4sensorhub_irq_enable_fail;
	}

	data = m4sensorhub->irqdata;
	mutex_lock(&data->lock);
	if (data->irq_info[irq].enabled == 1)
		disabled = false;
	mutex_unlock(&data->lock);

	/* Checking if the IRQ was previously enabled only to print an error */
	if (!disabled) {
		KDEBUG(M4SH_INFO, "%s: IRQ %d is already enabled\n",
			__func__, irq);
		retval = 0;
		goto m4sensorhub_irq_enable_fail;
	}

	retval = m4sensorhub_reg_write_1byte(m4sensorhub,
		get_enable_reg(irq), EVENT_MASK(irq), EVENT_MASK(irq));
	if (retval < 0) {
		KDEBUG(M4SH_ERROR, "%s: Register write failed\n", __func__);
		goto m4sensorhub_irq_enable_fail;
	} else if (retval != 1) {
		KDEBUG(M4SH_ERROR, "%s: Wrote %d bytes instead of 1\n",
			__func__, retval);
		goto m4sensorhub_irq_enable_fail;
	}

	mutex_lock(&data->lock);
	data->irq_info[irq].enabled = 1;
	mutex_unlock(&data->lock);

m4sensorhub_irq_enable_fail:
	return retval;
}
EXPORT_SYMBOL_GPL(m4sensorhub_irq_enable);

/* m4sensorhub_irq_disable_all()

   Disables all M4 IRQs (bypasses m4sensorhub_irq_disable())

*/
int m4sensorhub_irq_disable_all(struct m4sensorhub_data *m4sensorhub)
{
	int retval = 0;
	int i = 0;
	struct m4sensorhub_irqdata *data = NULL;

	if (m4sensorhub == NULL) {
		KDEBUG(M4SH_ERROR, "%s: M4 data is NULL\n", __func__);
		retval = -ENODATA;
		goto m4sensorhub_irq_disable_all_fail;
	} else if (m4sensorhub->irqdata == NULL) {
		KDEBUG(M4SH_ERROR, "%s: IRQ data is NULL\n", __func__);
		retval = -ENODATA;
		goto m4sensorhub_irq_disable_all_fail;
	}

	for (i = 0; i < NUM_INT_REGS; i++) {
		retval = m4sensorhub_reg_write_1byte(m4sensorhub,
			int_registers[i].enable_reg, 0,
			int_registers[i].valid_bits);
		if (retval < 0) {
			KDEBUG(M4SH_ERROR, "%s: Failed to disable INT%d\n",
				__func__, i);
			goto m4sensorhub_irq_disable_all_fail;
		} else if (retval != 1) {
			KDEBUG(M4SH_ERROR, "%s: Wrote %d bytes instead of 1\n",
				__func__, retval);
			retval = -EINVAL;
			goto m4sensorhub_irq_disable_all_fail;
		}
	}

	data = m4sensorhub->irqdata;
	mutex_lock(&data->lock);
	for (i = 0; i < M4SH_IRQ__NUM; i++)
		data->irq_info[i].enabled = 0;
	mutex_unlock(&data->lock);

m4sensorhub_irq_disable_all_fail:
	return retval;
}
EXPORT_SYMBOL_GPL(m4sensorhub_irq_disable_all);

/* --------------- Local Functions ----------------- */

static unsigned short get_enable_reg(enum m4sensorhub_irqs event)
{
	unsigned short ret;

	if ((event) >= M4SH_IRQ__NUM)
		ret = M4SH_REG__INVALID;
	else if ((event) >= M4SH_IRQ_INT2_INDEX)
		ret = M4SH_REG_GENERAL_INTERRUPT2ENABLE;
	else if ((event) >= M4SH_IRQ_INT1_INDEX)
		ret = M4SH_REG_GENERAL_INTERRUPT1ENABLE;
	else if ((event) >= M4SH_IRQ_INT0_INDEX)
		ret = M4SH_REG_GENERAL_INTERRUPT0ENABLE;
	else
		ret = M4SH_REG__INVALID;

	return ret;
}

static void m4sensorhub_print_irq_sources(unsigned short *en_ints)
{
	char buffer[DBG_BUF_LINE_LEN];
	int i;
	unsigned char index;
	unsigned short cur_ints;

	if (m4sensorhub_debug < M4SH_NOTICE)
		goto error;

	strcpy(buffer, "M4 IRQ Registers:");
	for (i = 0; (i < NUM_INT_REGS) &&
			(strlen(buffer) < DBG_BUF_LINE_LEN-5); ++i) {
		sprintf(&buffer[strlen(buffer)], " 0x%02x", en_ints[i]);
	}

	KDEBUG(M4SH_NOTICE, "%s: %s\n", __func__, buffer);

	/* Decode the bits */
	KDEBUG(M4SH_NOTICE, "%s: M4 IRQ Sources:\n", __func__);
	for (i = 0; i < NUM_INT_REGS; ++i) {
		cur_ints = en_ints[i];
		while (cur_ints > 0) {
			/* find the first set bit */
			index = (unsigned char) (ffs(cur_ints) - 1);
			if (index >= M4SH_IRQ__NUM) {
				KDEBUG(M4SH_NOTICE, "%s: No set bits found\n",
					__func__);
				goto error;
			}

			/* clear the bit */
			cur_ints &= ~(1 << index);
			/* find the event that occurred */
			index += M4SH_IRQ__START + (i * NUM_INTS_PER_REG);
			if (index >= M4SH_IRQ__NUM) {
				KDEBUG(M4SH_NOTICE, "%s: IRQ index is %d\n",
					__func__, index);
				goto error;
			}

			if (index <= ARRAY_SIZE(irq_name))
				KDEBUG(M4SH_NOTICE, "\t%s\n", irq_name[index]);
			else
				KDEBUG(M4SH_NOTICE, "\tIRQ %d\n", index);
		}
	}

error:
	return;
}

static irqreturn_t event_threaded(int irq, void *devid)
{
	unsigned short en_ints[NUM_INT_REGS] = { 0 };
	int i;
	struct m4sensorhub_irqdata *data = devid;
	struct m4sensorhub_data *m4sensorhub;
	struct i2c_client *i2c;
	unsigned char value, is_irq_set = 0;

	m4sensorhub = data->m4sensorhub;
	i2c = m4sensorhub->i2c_client;

	for (i = 0; i < NUM_INT_REGS; ++i) {
		/* M4 is expected to clear these bits when read */
		if (1 != m4sensorhub_reg_read(m4sensorhub,
			int_registers[i].status_reg, &value)) {
			dev_err(&m4sensorhub->i2c_client->dev,
				"Error reading INT%d\n", i);
			goto error;
		}
		en_ints[i] = value;
		is_irq_set |= value;
	}

	if (!is_irq_set) {
		/* Got the checkpoint to check if M4 panicked */
		m4sensorhub_panic_process(m4sensorhub);
		goto error;
	}

	if (m4sensorhub->irq_dbg.suspend == 1)
		m4sensorhub_print_irq_sources(en_ints);

	for (i = 0; i < NUM_INT_REGS; ++i) {
		unsigned char index;

		while (en_ints[i] > 0) {
			struct m4sensorhub_event_handler *event_handler;

			/* find the first set bit */
			index = (unsigned char)(ffs(en_ints[i]) - 1);
			if (index >= M4SH_IRQ__NUM)
				goto error;
			/* clear the bit */
			en_ints[i] &= ~(1 << index);
			/* find the event that occurred */
			index += M4SH_IRQ__START + (i * NUM_INTS_PER_REG);
			if (index >= M4SH_IRQ__NUM)
				goto error;

			if (data->irq_info[index].enabled) {
				event_handler = &data->event_handler[index];

				if (event_handler && event_handler->func) {
					event_handler->func(index,
							   event_handler->data);
					if (data->irq_info[index].tm_wakelock) {
						/* Hold a 500ms wakelock to
						   let data get to apps */
						wake_lock_timeout(
							&data->tm_wake_lock,
							0.5 * HZ);
					}
				}
				mutex_lock(&data->lock);
				data->irq_info[index].ena_fired++;
				mutex_unlock(&data->lock);
			} else {
				mutex_lock(&data->lock);
				data->irq_info[index].disa_fired++;
				mutex_unlock(&data->lock);
			}
		}
	}
error:
	wake_unlock(&data->wake_lock);

	return IRQ_HANDLED;
}

#ifdef CONFIG_DEBUG_FS
static int m4sensorhub_dbg_irq_show(struct seq_file *s, void *data)
{
	unsigned int i;
	struct m4sensorhub_irqdata *irqdata = s->private;

	seq_printf(s, "%21s%9s%12s%15s%16s\n",
		   "M4SENSORHUB IRQ", "Enabled", "Registered",
		   "Fired Enabled", "Fired Disabled");

	for (i = 0; i < M4SH_IRQ__NUM; i++) {
		seq_printf(s, "%21s%9d%12d%15d%16d\n",
			   irq_name[i],
			   irqdata->irq_info[i].enabled,
			   irqdata->irq_info[i].registered,
			   irqdata->irq_info[i].ena_fired,
			   irqdata->irq_info[i].disa_fired);
	}
	return 0;
}

static int m4sensorhub_dbg_irq_open(struct inode *inode, struct file *file)
{
	return single_open(file, m4sensorhub_dbg_irq_show, inode->i_private);
}
#endif

/* m4sensorhub_irq_restore()

   Callback Handler is called by Panic after M4 has been restarted

*/
static void m4sensorhub_irq_restore(struct m4sensorhub_data *m4sensorhub,
	void *data)
{
	int i;
	unsigned short en_ints[NUM_INT_REGS] = {0};

	mutex_lock(&((struct m4sensorhub_irqdata *)data)->lock);
	for (i = 0; i < M4SH_IRQ__NUM; i++) {
		if (!((struct m4sensorhub_irqdata *)data)->irq_info[i].enabled)
			continue;
		en_ints[i/NUM_INTS_PER_REG] |= EVENT_MASK(i);
	}
	mutex_unlock(&((struct m4sensorhub_irqdata *)data)->lock);

	for (i = 0; i < NUM_INT_REGS; i++) {
		KDEBUG(M4SH_INFO, "%s: Reseting INT%d-%02X\n", __func__,
			i, en_ints[i]);
		if (1 != m4sensorhub_reg_write_1byte(m4sensorhub,
				int_registers[i].enable_reg, en_ints[i],
				int_registers[i].valid_bits)) {
			KDEBUG(M4SH_ERROR, "%s: Failed reseting INT%d\n",
				__func__, i);
		}
	}
}
