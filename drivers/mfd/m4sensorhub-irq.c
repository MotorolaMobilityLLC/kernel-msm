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

#define NUM_INT_REGS      2
#define NUM_INTS_PER_REG  32

#define EVENT_MASK(event)  (1 << ((event >= M4SH_WAKEIRQ__START) ? \
				(event - M4SH_WAKEIRQ__START) : event))

#define DBG_BUF_LINE_LEN  80

/* --------------- Global Declarations -------------- */

/* ------------ Local Function Prototypes ----------- */
static unsigned short get_enable_reg(enum m4sensorhub_irqs event);
static irqreturn_t wake_event_threaded(int irq, void *devid);
static irqreturn_t nowake_event_threaded(int irq, void *devid);
#ifdef CONFIG_DEBUG_FS
static int m4sensorhub_dbg_irq_open(struct inode *inode, struct file *file);
#endif
static void m4sensorhub_irq_restore(struct m4sensorhub_data *m4sensorhub,
	void *data);

/* ---------------- Local Declarations -------------- */

static const char * const irq_name[] = {
	[M4SH_NOWAKEIRQ_ACCEL]           = "NOWAKE_ACCEL",
	[M4SH_NOWAKEIRQ_GYRO]            = "NOWAKE_GYRO",
	[M4SH_NOWAKEIRQ_COMPASS]         = "NOWAKE_COMPASS",
	[M4SH_NOWAKEIRQ_FUSION]          = "NOWAKE_FUSION",
	[M4SH_NOWAKEIRQ_PRESSURE]        = "NOWAKE_PRESSURE",
	[M4SH_NOWAKEIRQ_ADS]             = "NOWAKE_ADS",
	[M4SH_NOWAKEIRQ_PPG]             = "NOWAKE_PPG",
	[M4SH_NOWAKEIRQ_HEARTRATE]       = "NOWAKE_HEARTRATE",
	[M4SH_NOWAKEIRQ_PEDOMETER]       = "NOWAKE_PEDOMETER",
	[M4SH_NOWAKEIRQ_ALS]             = "NOWAKE_ALS",
	[M4SH_NOWAKEIRQ_ACTIVITY_CHANGE] = "NOWAKE_ACTIVITY_CHANGE",
	[M4SH_WAKEIRQ_STILL]            = "WAKE_STILL",
	[M4SH_WAKEIRQ_MOTION]           = "WAKE_MOTION",
	[M4SH_WAKEIRQ_GESTURE]          = "WAKE_GESTURE",
	[M4SH_WAKEIRQ_PASSIVE]          = "WAKE_PASSIVE",
	[M4SH_WAKEIRQ_AP_ALARM_EXPIRED] = "WAKE_ALARM_EXPIRED",
	[M4SH_WAKEIRQ_M4_READY]         = "WAKE_M4_READY",
	[M4SH_WAKEIRQ_SIGNIFICANT_MOTION] = "WAKE_SIGNIFICANT_MOTION",
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
	uint32_t valid_bits;
} int_registers[NUM_INT_REGS] = {
	{M4SH_REG_GENERAL_NOWAKEINTSTATUS,
	 M4SH_REG_GENERAL_NOWAKEINTENABLE,
	 0xFFFFFFFF},
	{M4SH_REG_GENERAL_WAKEINTSTATUS,
	M4SH_REG_GENERAL_WAKEINTENABLE,
	0xFFFFFFFF},
};

static irqreturn_t wake_event_isr(int irq, void *data)
{
	struct m4sensorhub_irqdata *irq_data = data;

	/*
	 * This wakelock is held to prevent the kernel from going
	 * to sleep before the event_thread for this irq gets to run
	 * and so this is released when the event_thread finishes execution
	 */
	wake_lock(&irq_data->wake_lock);

	return IRQ_WAKE_THREAD;
}

static irqreturn_t nowake_event_isr(int irq, void *data)
{
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

	retval = m4sensorhub_irq_disable_all(m4sensorhub);
	if (retval < 0) {
		KDEBUG(M4SH_ERROR, "%s: Failed disable all irqs\n", __func__);
		goto err_free;
	}

	wake_lock_init(&data->wake_lock, WAKE_LOCK_SUSPEND, "m4sensorhub-irq");
	wake_lock_init(&data->tm_wake_lock, WAKE_LOCK_SUSPEND,
		       "m4sensorhub-timed-irq");

	/* request wake irq */
	retval = request_threaded_irq(m4sensorhub->hwconfig.wakeirq,
		wake_event_isr, wake_event_threaded,
		IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
		"m4sensorhub-wakeirq", data);
	if (retval) {
		KDEBUG(M4SH_ERROR, "%s: Failed requesting wakeirq = %d\n",
			__func__, retval);
		goto err_destroy_wq;
	}
	retval = enable_irq_wake(i2c->irq);
	if (retval) {
		KDEBUG(M4SH_ERROR, "%s: Failed enabling irq wake.\n", __func__);
		goto err_free_wakeirq;
	}

	/* request nowake irq */
	retval = request_threaded_irq(m4sensorhub->hwconfig.nowakeirq,
		nowake_event_isr, nowake_event_threaded,
		IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
		"m4sensorhub-nowakeirq", data);
	if (retval) {
		KDEBUG(M4SH_ERROR, "%s: Failed requesting nowakeirq = %d\n",
			__func__, retval);
		goto err_free_nowakeirq;
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

err_free_nowakeirq:
	free_irq(m4sensorhub->hwconfig.nowakeirq, data);
#ifdef CONFIG_DEBUG_FS
err_disa_irq:
#endif
	disable_irq_wake(i2c->irq);
err_free_wakeirq:
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
	free_irq(m4sensorhub->hwconfig.nowakeirq, data);

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
	uint32_t mask;
	uint32_t value = 0;

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

	mask = EVENT_MASK(irq);
	retval = m4sensorhub_reg_write(m4sensorhub, get_enable_reg(irq),
		(unsigned char *)&value, (unsigned char *)&mask);
	if (retval < 0) {
		KDEBUG(M4SH_ERROR, "%s: Register write failed\n", __func__);
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
	uint32_t mask;

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

	mask = EVENT_MASK(irq);
	retval = m4sensorhub_reg_write(m4sensorhub, get_enable_reg(irq),
		(unsigned char *)&mask, (unsigned char *)&mask);
	if (retval < 0) {
		KDEBUG(M4SH_ERROR, "%s: Register write failed\n", __func__);
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
	uint32_t value = 0;

	if (m4sensorhub == NULL) {
		KDEBUG(M4SH_ERROR, "%s: M4 data is NULL\n", __func__);
		retval = -ENODATA;
		goto m4sensorhub_irq_disable_all_fail;
	} else if (m4sensorhub->irqdata == NULL) {
		KDEBUG(M4SH_ERROR, "%s: IRQ data is NULL\n", __func__);
		retval = -ENODATA;
		goto m4sensorhub_irq_disable_all_fail;
	}

	retval = m4sensorhub_reg_write(m4sensorhub,
		M4SH_REG_GENERAL_NOWAKEINTENABLE,
		(unsigned char *)&value, m4sh_no_mask);
	if (retval < 0) {
		KDEBUG(M4SH_ERROR, "%s: Failed to disable no wake irq\n",
			__func__);
		goto m4sensorhub_irq_disable_all_fail;
	}
	retval = m4sensorhub_reg_write(m4sensorhub,
			M4SH_REG_GENERAL_WAKEINTENABLE,
			(unsigned char *)&value, m4sh_no_mask);
	if (retval < 0) {
		KDEBUG(M4SH_ERROR, "%s: Failed to disable wake irq\n",
			__func__);
		goto m4sensorhub_irq_disable_all_fail;
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
	else if ((event >= M4SH_NOWAKEIRQ__START) &&
		(event < M4SH_NOWAKEIRQ__MAX))
		ret = M4SH_REG_GENERAL_NOWAKEINTENABLE;
	else if ((event >= M4SH_WAKEIRQ__START) && (event < M4SH_WAKEIRQ__MAX))
		ret = M4SH_REG_GENERAL_WAKEINTENABLE;
	else
		ret = M4SH_REG__INVALID;

	return ret;
}

static void m4sensorhub_print_irq_sources(uint32_t en_ints, char nowake)
{
	int i;
	unsigned char index;

	if (m4sensorhub_debug < M4SH_NOTICE)
		goto error;

	KDEBUG(M4SH_NOTICE, "%s: M4 %s IRQ register: 0x%08X", __func__,
	       (nowake ? "NOWAKE" : "WAKE"), en_ints);

	/* Decode the bits */
	for (i = 0; ((i < sizeof(en_ints)) && (en_ints != 0)); i++) {
		if (en_ints & (0x01 << i)) {
			if (nowake)
				index = i;
			else
				index = i + M4SH_WAKEIRQ__START;

			if (index >= M4SH_IRQ__NUM)
				KDEBUG(M4SH_NOTICE, "%s: IRQ index is %d\n",
				       __func__, index);

			if (index <= ARRAY_SIZE(irq_name))
				KDEBUG(M4SH_NOTICE, "%s: %s\n",
				       __func__, irq_name[index]);
			else
				KDEBUG(M4SH_NOTICE, "%s: %s IRQ %d\n",
				       __func__,
				       (nowake ? "NOWAKE" : "WAKE"), index);

		}

		/* Clear the bit that we finished processing */
		en_ints &= ~(0x01 << i);
	}

error:
	return;
}

static irqreturn_t wake_event_threaded(int irq, void *devid)
{
	uint32_t en_ints = 0, value = 0, is_irq_set = 0;
	int index;
	struct m4sensorhub_irqdata *data = devid;
	struct m4sensorhub_data *m4sensorhub;
	struct i2c_client *i2c;

	m4sensorhub = data->m4sensorhub;
	i2c = m4sensorhub->i2c_client;

	/* M4 is expected to clear these bits when read */
	if (m4sensorhub_reg_read(m4sensorhub,
			M4SH_REG_GENERAL_WAKEINTSTATUS,
			(unsigned char *)&value) < 0) {
			dev_err(&m4sensorhub->i2c_client->dev,
			"Error reading wake int status register\n");
		goto error;
	}

	en_ints = value;
	is_irq_set |= value;

	if (!is_irq_set) {
		/* Got the checkpoint to check if M4 panicked */
		m4sensorhub_panic_process(m4sensorhub);
		goto error;
	}

	if (m4sensorhub->irq_dbg.suspend == 1)
		m4sensorhub_print_irq_sources(en_ints, 0);

	while (en_ints > 0) {
		struct m4sensorhub_event_handler *event_handler;

		/* find the first set bit */
		index = (ffs(en_ints) - 1);
		if (index >= M4SH_WAKEIRQ__MAX)
			goto error;
		/* clear the bit */
		en_ints &= ~(1 << index);
		/* find the event that occurred */
		index += M4SH_WAKEIRQ__START;
		if (index >= M4SH_WAKEIRQ__MAX)
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
error:
	wake_unlock(&data->wake_lock);

	return IRQ_HANDLED;
}

static irqreturn_t nowake_event_threaded(int irq, void *devid)
{
	uint32_t en_ints = 0, value = 0, is_irq_set = 0;
	int index;
	struct m4sensorhub_irqdata *data = devid;
	struct m4sensorhub_data *m4sensorhub;
	struct i2c_client *i2c;

	m4sensorhub = data->m4sensorhub;
	i2c = m4sensorhub->i2c_client;

	/* M4 is expected to clear these bits when read */
	if (m4sensorhub_reg_read(m4sensorhub, M4SH_REG_GENERAL_NOWAKEINTSTATUS,
		(unsigned char *)&value) < 0) {
			dev_err(&m4sensorhub->i2c_client->dev,
			"Error reading nowake INT status register\n");
		goto error;
	}

	en_ints = value;
	is_irq_set |= value;

	if (!is_irq_set) {
		KDEBUG(M4SH_ERROR, "%s: Got nowake int with no bits set",
			__func__);
		goto error;
	}

	if (m4sensorhub->irq_dbg.suspend == 1)
		m4sensorhub_print_irq_sources(en_ints, 1);

	while (en_ints > 0) {
		struct m4sensorhub_event_handler *event_handler;

		/* find the first set bit */
		index = (ffs(en_ints) - 1);
		if (index >= M4SH_NOWAKEIRQ__MAX)
			goto error;
		/* clear the bit */
		en_ints &= ~(1 << index);
		if (data->irq_info[index].enabled) {
			event_handler = &data->event_handler[index];

			if (event_handler && event_handler->func) {
				event_handler->func(index,
						    event_handler->data);
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
error:
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

/*
 * m4sensorhub_irq_restore()
 *
 * Callback Handler is called by Panic after M4 has been restarted
 *
 */
static void m4sensorhub_irq_restore(struct m4sensorhub_data *m4sensorhub,
	void *data)
{
	int i;
	uint32_t nowake_en_ints = 0, wake_en_ints = 0;

	mutex_lock(&((struct m4sensorhub_irqdata *)data)->lock);

	for (i = 0; i < M4SH_IRQ__NUM; i++) {
		if (!((struct m4sensorhub_irqdata *)data)->irq_info[i].enabled)
			continue;
		if (i < M4SH_NOWAKEIRQ__MAX)
			nowake_en_ints |= EVENT_MASK(i);
		else
			wake_en_ints |= EVENT_MASK(i);
	}
	mutex_unlock(&((struct m4sensorhub_irqdata *)data)->lock);

	KDEBUG(M4SH_INFO, "%s: Resetting NOWAKEINT-%x\n", __func__,
		nowake_en_ints);
	if (m4sensorhub_reg_write(m4sensorhub,
				      M4SH_REG_GENERAL_NOWAKEINTENABLE,
				      (unsigned char *)&nowake_en_ints,
				      m4sh_no_mask) < 0) {
		KDEBUG(M4SH_ERROR, "%s: Failed to reset NOWAKEINT\n",
			__func__);
	}

	KDEBUG(M4SH_INFO, "%s: Resetting WAKEINT-%x\n", __func__,
		wake_en_ints);
	if (m4sensorhub_reg_write(m4sensorhub,
			      M4SH_REG_GENERAL_WAKEINTENABLE,
			      (unsigned char *)&wake_en_ints,
			      m4sh_no_mask) < 0) {
		KDEBUG(M4SH_ERROR, "%s: Failed to reset WAKEINT\n", __func__);
	}
}
