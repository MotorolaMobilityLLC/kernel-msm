/*
 *  Copyright (C) 2014 Motorola, Inc.
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  Adds ability to program periodic interrupts from user space that
 *  can wake the phone out of low power modes.
 *
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/m4sensorhub.h>
#include <linux/slab.h>

#define m4ext_err(format, args...)  KDEBUG(M4SH_ERROR, format, ## args)

struct m4sensorhub_data *m4ext_m4;
DEFINE_MUTEX(m4ext_mutex);
uint8_t m4ext_display_status;

static void m4ext_panic_callback(struct m4sensorhub_data *m4, void *context)
{
	int err;

	mutex_lock(&m4ext_mutex);

	if (m4ext_m4 == NULL) {
		m4ext_err("%s: M4 data is NULL.\n", __func__);
		err = -ENODATA;
		goto m4ext_panic_callback_fail;
	}

	err = m4sensorhub_reg_write(m4, M4SH_REG_USERSETTINGS_SCREENSTATUS,
		(char *)&m4ext_display_status, m4sh_no_mask);
	if (err < 0)
		m4ext_err("%s: Screen status write failed (%d).\n",
			__func__, err);
	else if (err != 1)
		m4ext_err("%s: Screen status wrote %d bytes instead of 1.\n",
			__func__, err);

m4ext_panic_callback_fail:
	mutex_unlock(&m4ext_mutex);
	return;
}

int m4sensorhub_extern_init(struct m4sensorhub_data *m4)
{
	int err;

	mutex_lock(&m4ext_mutex);

	err = m4sensorhub_panic_register(m4, PANICHDL_EXTERN_RESTORE,
		m4ext_panic_callback, NULL);
	if (err < 0) {
		m4ext_err("%s: Failed to register panic callback.\n", __func__);
		goto m4sensorhub_extern_init_fail;
	}

	m4ext_m4 = m4;

m4sensorhub_extern_init_fail:
	mutex_unlock(&m4ext_mutex);
	return err;
}
EXPORT_SYMBOL_GPL(m4sensorhub_extern_init);

int m4sensorhub_extern_set_display_status(uint8_t status)
{
	int err;

	mutex_lock(&m4ext_mutex);

	if (m4ext_m4 == NULL) {
		m4ext_err("%s: M4 data is NULL.\n", __func__);
		err = -ENODATA;
		goto m4sensorhub_extern_set_display_status_fail;
	}

	m4ext_display_status = status;

	err = m4sensorhub_reg_write(m4ext_m4,
		M4SH_REG_USERSETTINGS_SCREENSTATUS,
		(char *)&status, m4sh_no_mask);
	if (err < 0) {
		m4ext_err("%s: I2C write failed (%d).\n", __func__, err);
		goto m4sensorhub_extern_set_display_status_fail;
	} else if (err != 1) {
		m4ext_err("%s: Wrote %d bytes instead of 1.\n", __func__, err);
		err = -EINVAL;
		goto m4sensorhub_extern_set_display_status_fail;
	}

	err = 0;

m4sensorhub_extern_set_display_status_fail:
	mutex_unlock(&m4ext_mutex);
	return err;
}
EXPORT_SYMBOL_GPL(m4sensorhub_extern_set_display_status);
