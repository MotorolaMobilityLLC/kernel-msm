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

#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/m4sensorhub.h>
#include <linux/slab.h>


/* --------------- Global Declarations -------------- */
#define PANIC_BANK              0xFF /* Reserved for Panic bank */
#define PANIC_CMD_CHECK         0xCD /* Panic Handoff command */
#define PANIC_RESP_CHECK        0xDeadBeef /* Panic Handoff Magic code */

/* ------------ Local Function Prototypes ----------- */

/* --------------- Local Declarations -------------- */
static const char *callback_name[PANICHDL_MAX] = {
	[PANICHDL_DISPLAY_RESTORE] = "display_restore",
	[PANICHDL_IRQ_RESTORE] = "irq_restore",
	[PANICHDL_HEARTRATE_RESTORE] = "heartrate_restore",
	[PANICHDL_PASSIVE_RESTORE] = "passive_restore",
	[PANICHDL_ALS_RESTORE] = "als_restore",
	[PANICHDL_FUSION_RESTORE] = "fusion_restore",
	[PANICHDL_MPU9150_RESTORE] = "mpu9150_restore",
	[PANICHDL_PEDOMETER_RESTORE] = "pedometer_restore",
	[PANICHDL_EXTERN_RESTORE] = "extern_restore",
	[PANICHDL_PRESSURE_RESTORE] = "pressure_restore",
	[PANICHDL_ADS_RESTORE] = "ads_restore",
};

struct m4sensorhub_panic_callback {
	void (*callback)(struct m4sensorhub_data *, void *);
	void *data;
};

struct m4sensorhub_panicdata {
	struct mutex lock;  /* lock callback and data */
	struct m4sensorhub_panic_callback funcs[PANICHDL_MAX];
};

union panic_buf {
	struct _in {
		unsigned char bank;
		unsigned char cmd;
	} in;
	unsigned int data;
};

/* -------------- Local Data Structures ------------- */

/* -------------- Global Functions ----------------- */

/* m4sensorhub_panic_init()

   Initialized panic private data structures.

   Returns 0 on success or negative error code on failure

     m4sensorhub - pointer to the main m4sensorhub data struct
*/
int m4sensorhub_panic_init(struct m4sensorhub_data *m4sensorhub)
{
	int retval = 0;
	struct m4sensorhub_panicdata *data;

	if (m4sensorhub == NULL) {
		KDEBUG(M4SH_ERROR, "%s: M4 data is NULL\n", __func__);
		retval = -ENODATA;
		goto m4sensorhub_panic_init_fail;
	}

	if (m4sensorhub->panicdata != NULL) {
		KDEBUG(M4SH_ERROR,
			"%s: Trying to overwrite previous panic data\n",
			__func__);
		retval = -EPERM;
		goto m4sensorhub_panic_init_fail;
	}

	data = kzalloc(sizeof(struct m4sensorhub_panicdata), GFP_KERNEL);
	if (data == NULL) {
		KDEBUG(M4SH_ERROR, "%s: Memory error in panic_init\n",
			__func__);
		retval = -ENOMEM;
		goto m4sensorhub_panic_init_fail;
	}

	mutex_init(&data->lock);
	m4sensorhub->panicdata = data;

m4sensorhub_panic_init_fail:
	return retval;
}
EXPORT_SYMBOL_GPL(m4sensorhub_panic_init);

/* m4sensorhub_panic_shutdown()

   Shutdown the M4 sensor hub Panic subsystem

     m4sensorhub - pointer to the main m4sensorhub data struct
*/
void m4sensorhub_panic_shutdown(struct m4sensorhub_data *m4sensorhub)
{
	struct m4sensorhub_panicdata *data = NULL;

	if (m4sensorhub == NULL) {
		KDEBUG(M4SH_ERROR, "%s: M4 data is NULL\n", __func__);
		return;
	}

	data = m4sensorhub->panicdata;
	if (data != NULL) {
		m4sensorhub->panicdata = NULL;
		if (mutex_is_locked(&data->lock))
			mutex_unlock(&data->lock);
		mutex_destroy(&data->lock);
		/*
		 * Callback and data pointers were passed to us, so
		 * we leave it to the registering functions to
		 * clean up their own memory.
		 */
		kfree(data);
	}

	return;
}
EXPORT_SYMBOL_GPL(m4sensorhub_panic_shutdown);

/* m4sensorhub_panic_register()

   Register a panic handler to monitor M4 panic reset

   Returns 0 on success or negative error code on failure

     m4sensorhub - pointer to the main m4sensorhub data struct
     index - M4 Sensor Hub panic handler to register for
     cb_func - panic handler function to execute after M4 reset
     data - pointer to data for panic handler function
*/
int m4sensorhub_panic_register(struct m4sensorhub_data *m4sensorhub,
	enum m4sensorhub_panichdl_index index,
	void (*cb_func) (struct m4sensorhub_data *, void *),
	void *data)
{
	struct m4sensorhub_panicdata *panicdata = NULL;
	int retval = 0;

	if (m4sensorhub == NULL) {
		KDEBUG(M4SH_ERROR, "%s: M4 data is NULL\n", __func__);
		retval = -ENODATA;
		goto m4sensorhub_panic_register_exit;
	} else if (cb_func == NULL) {
		KDEBUG(M4SH_ERROR, "%s: Callback is NULL\n", __func__);
		retval = -ENOSYS;
		goto m4sensorhub_panic_register_exit;
	} else if (index >= PANICHDL_MAX) {
		KDEBUG(M4SH_ERROR, "%s: Number of panic handlers exceeded\n",
			__func__);
		retval = -EOVERFLOW;
		goto m4sensorhub_panic_register_exit;
	} else if (m4sensorhub->panicdata == NULL) {
		KDEBUG(M4SH_ERROR, "%s: Panic data is missing\n", __func__);
		retval = -ENODATA;
		goto m4sensorhub_panic_register_exit;
	}

	panicdata = (struct m4sensorhub_panicdata *)m4sensorhub->panicdata;
	mutex_lock(&panicdata->lock);
	if (panicdata->funcs[index].callback == NULL) {
		panicdata->funcs[index].callback = cb_func;
		panicdata->funcs[index].data = data;
		KDEBUG(M4SH_NOTICE, "%s: %s callback registered\n",
			__func__, callback_name[index]);
	} else {
		KDEBUG(M4SH_ERROR, "%s: %s %s", __func__,
			callback_name[index], "callback registration failed\n");
		retval = -EPERM;
		goto m4sensorhub_panic_register_fail;
	}

m4sensorhub_panic_register_fail:
	mutex_unlock(&panicdata->lock);
m4sensorhub_panic_register_exit:
	return retval;
}
EXPORT_SYMBOL_GPL(m4sensorhub_panic_register);

/* m4sensorhub_panic_unregister()

   Unregister an panic handler to monitor M4 panic reset

   Returns 0 on success or negative error code on failure

     m4sensorhub - pointer to the main m4sensorhub data struct
     index - M4 Sensor Hub panic handler to unresiter for
*/
int m4sensorhub_panic_unregister(struct m4sensorhub_data *m4sensorhub,
	enum m4sensorhub_panichdl_index index)
{
	int retval = 0;
	struct m4sensorhub_panicdata *panicdata = NULL;

	if (m4sensorhub == NULL) {
		KDEBUG(M4SH_ERROR, "%s: M4 data is NULL\n", __func__);
		retval = -ENODATA;
		goto m4sensorhub_panic_unregister_fail;
	} else if (index >= PANICHDL_MAX) {
		KDEBUG(M4SH_ERROR, "%s: Number of panic handlers exceeded\n",
			__func__);
		retval = -EOVERFLOW;
		goto m4sensorhub_panic_unregister_fail;
	} else if (m4sensorhub->panicdata == NULL) {
		KDEBUG(M4SH_ERROR, "%s: Panic data is missing\n", __func__);
		retval = -ENODATA;
		goto m4sensorhub_panic_unregister_fail;
	}

	panicdata = (struct m4sensorhub_panicdata *)m4sensorhub->panicdata;
	mutex_lock(&panicdata->lock);
	panicdata->funcs[index].callback = NULL;
	panicdata->funcs[index].data = NULL;
	mutex_unlock(&panicdata->lock);
	KDEBUG(M4SH_NOTICE, "%s: %s callback un-registered\n",
			__func__, callback_name[index]);

m4sensorhub_panic_unregister_fail:
	return retval;
}
EXPORT_SYMBOL_GPL(m4sensorhub_panic_unregister);


/* m4sensorhub_panic_process()

   Check M4 if it's panicked, use I2C to communicate with M4 panic handler
   OMAP use the same i2c sequences to send command via i2c master, then M4
   i2c slave program will handle these commands, it may have 2 slave programs
   1. Normal i2c slave program handles all vaild banks'(limit on
      M4SH_TYPE__NUM) command, for invalid bank, it always responses 0xFF
   2. Panic i2c slave program handles panic bank(reserved 0xFF for it) command,
      for others, it always responses 0x00

   To detect whether M4 is panicked, the process should be
   i. When OMAP got interrupt from M4, OMAP will check which irq is raised, it
      send normal banks' command to M4, for panic case, it always returns 0x00,
      so OMAP has a checkpoint as there's interrupt request from M4 without
      active IRQ
   ii.Then OMAP will confirm if M4 is panic via send panic bank command, if M4
      is panicked, it will handle this bank and response panic magic code;
      Otherwise, if no panic magic code returned from M4, it always means M4
      isn't panicked.

     m4sensorhub - pointer to the main m4sensorhub data struct
	*/
void m4sensorhub_panic_process(struct m4sensorhub_data *m4sensorhub)
{
	int i, ret;
	union panic_buf buf;
	struct m4sensorhub_panic_callback handler;

	if (m4sensorhub == NULL) {
		KDEBUG(M4SH_ERROR, "%s: M4 data is NULL\n", __func__);
		return;
	} else if (m4sensorhub->panicdata == NULL) {
		KDEBUG(M4SH_ERROR, "%s: Panic data is missing\n", __func__);
		return;
	}

	m4sensorhub_reg_access_lock();

	buf.in.bank = PANIC_BANK;
	buf.in.cmd = PANIC_CMD_CHECK;
	ret = m4sensorhub_i2c_write_read(m4sensorhub,
		(u8 *)&buf, sizeof(buf.in), sizeof(buf.data));
	if ((ret != sizeof(buf.data)) || (buf.data != PANIC_RESP_CHECK)) {
		/* TODO maybe we shall check if M4/OMAP i2c broken */
		KDEBUG(M4SH_ERROR, "m4sensorhub: %s ret=%d, data=0x%x\n",
			"Unknown IRQ status! M4 panic handoff", ret, buf.data);
		m4sensorhub_reg_access_unlock();
		return;
	}

	KDEBUG(M4SH_ERROR, "%s: Detected M4 panic, reset M4!\n", __func__);
	/* Passing "true" will reset M4 before trying to communicate */
	ret = m4sensorhub_test_m4_reboot(m4sensorhub, true);
	if (ret < 0) {
		KDEBUG(M4SH_ERROR, "%s: Failed to restart M4, ret = %d\n",
			__func__, ret);
		BUG();
	}
	m4sensorhub_reg_access_unlock();

	for (i = 0; i < PANICHDL_MAX; i++) {
		handler = ((struct m4sensorhub_panicdata *)
			(m4sensorhub->panicdata))->funcs[i];
		if (handler.callback) {
			KDEBUG(M4SH_NOTICE, "%s: Calling %s as M4 restarted!\n",
				__func__, callback_name[i]);
			handler.callback(m4sensorhub, handler.data);
		}
	}
}
EXPORT_SYMBOL_GPL(m4sensorhub_panic_process);
