/*
 * Copyright (c) 2012-2014, Motorola, Inc. All Rights Reserved.
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __M4SENSORHUB_H__
#define __M4SENSORHUB_H__

#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/m4sensorhub/m4sensorhub_registers.h>
#include <linux/m4sensorhub/m4sensorhub_irqs.h>
#include <linux/firmware.h>
#include <linux/m4sensorhub/MemMapUserSettings.h>

#ifdef __KERNEL__

extern char m4sensorhub_debug;

#define M4SENSORHUB_DRIVER_NAME     "m4sensorhub"

#define KDEBUG(i, format, s...)                         \
	do {                                            \
		if (m4sensorhub_debug >= i)             \
			pr_crit(format, ##s);           \
	} while (0)

enum m4sensorhub_debug_level {
	M4SH_NODEBUG = 0x0,
	M4SH_CRITICAL,
	M4SH_ERROR,
	M4SH_WARNING,
	M4SH_NOTICE,
	M4SH_INFO,
	M4SH_DEBUG,
	M4SH_VERBOSE_DEBUG
};

enum m4sensorhub_mode {
	UNINITIALIZED,
	BOOTMODE,
	NORMALMODE
};

enum m4sensorhub_bootmode {
	BOOTMODE0_HIGH
};

/* This enum is used to register M4 panic callback
 * The sequence of this enum is also the sequence of calling
 *   i.e. it will be called follow this enum 0, 1, 2 ... max
 */
enum m4sensorhub_panichdl_index {
	PANICHDL_DISPLAY_RESTORE,
	PANICHDL_HEARTRATE_RESTORE,
	PANICHDL_PASSIVE_RESTORE,
	PANICHDL_FUSION_RESTORE,
	PANICHDL_ALS_RESTORE,
	PANICHDL_MPU9150_RESTORE,
	PANICHDL_PEDOMETER_RESTORE,
	PANICHDL_EXTERN_RESTORE,
	/*
	 * Please add enum before PANICHDL_IRQ_RESTORE
	 * to make sure IRQ restore will be called last.
	 *
	 * Also, add your debug string name to
	 * m4sensorhub-panic.c.
	 */
	PANICHDL_IRQ_RESTORE, /* Keep it as the last one */
	PANICHDL_MAX = PANICHDL_IRQ_RESTORE+1
};

struct m4sensorhub_hwconfig {
	int irq_gpio;
	int reset_gpio;
	int boot0_gpio;
};

struct m4sensorhub_irq_dbg {
	unsigned char suspend; /* 1 - Suspended, 0 - Normal */
};

struct m4sensorhub_data {
	struct i2c_client *i2c_client;
	void *irqdata;
	void *panicdata;
	enum m4sensorhub_mode mode;
	struct m4sensorhub_hwconfig hwconfig;
	struct m4sensorhub_irq_dbg irq_dbg;
	char *filename;
	u16 fw_version;
};

struct init_calldata {
	struct m4sensorhub_data *p_m4sensorhub_data; /* M4 pointer */
	void *p_data; /* Driver data */
};

/* Global (kernel) functions */

/* Client devices */
struct m4sensorhub_data *m4sensorhub_client_get_drvdata(void);

/* Register access */

/* m4sensorhub_reg_read()

   Read a register from the M4 sensor hub.

   Returns number of bytes read on success.
   Returns negative error code on failure

     m4sensorhub - pointer to the main m4sensorhub data struct
     reg - Register to be read
     value - array to return data.  Needs to be at least register's size
*/
#define m4sensorhub_reg_read(m4sensorhub, reg, value) \
	m4sensorhub_reg_read_n(m4sensorhub, reg, value, \
			       m4sensorhub_reg_getsize(m4sensorhub, reg))

/* m4sensorhub_reg_write()

   Read a register from the M4 sensor hub.

   Returns number of bytes write on success.
   Returns negative error code on failure

     m4sensorhub - pointer to the main m4sensorhub data struct
     reg - Register to be write
     value - array to return data.  Needs to be at least register's size
     mask - mask representing which bits to change in register.  If all bits
	    are to be changed, then &m4sh_no_mask can be passed here.
*/
#define m4sensorhub_reg_write(m4sensorhub, reg, value, mask) \
	m4sensorhub_reg_write_n(m4sensorhub, reg, value, mask, \
				m4sensorhub_reg_getsize(m4sensorhub, reg))
int m4sensorhub_reg_init(struct m4sensorhub_data *m4sensorhub);
int m4sensorhub_reg_shutdown(struct m4sensorhub_data *m4sensorhub);
int m4sensorhub_reg_read_n(struct m4sensorhub_data *m4sensorhub,
			   enum m4sensorhub_reg reg, unsigned char *value,
			   short num);
int m4sensorhub_reg_write_n(struct m4sensorhub_data *m4sensorhub,
			    enum m4sensorhub_reg reg, unsigned char *value,
			    unsigned char *mask, short num);
int m4sensorhub_reg_write_1byte(struct m4sensorhub_data *m4sensorhub,
				enum m4sensorhub_reg reg, unsigned char value,
				unsigned char mask);
int m4sensorhub_reg_getsize(struct m4sensorhub_data *m4sensorhub,
			    enum m4sensorhub_reg reg);
void m4sensorhub_reg_access_lock(void);
void m4sensorhub_reg_access_unlock(void);
int m4sensorhub_i2c_write_read(struct m4sensorhub_data *m4sensorhub,
				      u8 *buf, int writelen, int readlen);

/*
 * Functions for re-initializing M4
 *
 * In general, only m4sensorhub_test_m4_reboot() should ever
 *   be called directly.
 */
int m4sensorhub_test_m4_reboot(struct m4sensorhub_data *m4, bool reboot_first);
int m4sensorhub_l4_load_firmware(struct m4sensorhub_data *m4sensorhub,
	unsigned short force_upgrade,
	const struct firmware *fm);
void m4sensorhub_hw_reset(struct m4sensorhub_data *m4sensorhub);

/* Interrupt handler */
int m4sensorhub_irq_init(struct m4sensorhub_data *m4sensorhub);
void m4sensorhub_irq_shutdown(struct m4sensorhub_data *m4sensorhub);
int m4sensorhub_irq_register(struct m4sensorhub_data *m4sensorhub,
			     enum m4sensorhub_irqs irq,
			     void (*cb_func) (enum m4sensorhub_irqs, void *),
			     void *data, uint8_t enable_timed_wakelock);
int m4sensorhub_irq_unregister(struct m4sensorhub_data *m4sensorhub,
			       enum m4sensorhub_irqs irq);
int m4sensorhub_irq_disable(struct m4sensorhub_data *m4sensorhub,
			    enum m4sensorhub_irqs irq);
int m4sensorhub_irq_enable(struct m4sensorhub_data *m4sensorhub,
			   enum m4sensorhub_irqs irq);
int m4sensorhub_irq_enable_get(struct m4sensorhub_data *m4sensorhub,
			       enum m4sensorhub_irqs irq);


/* M4 Panic Calls */
int m4sensorhub_panic_init(struct m4sensorhub_data *m4sensorhub);
void m4sensorhub_panic_shutdown(struct m4sensorhub_data *m4sensorhub);
int m4sensorhub_panic_register(struct m4sensorhub_data *m4sensorhub,
			   enum m4sensorhub_panichdl_index index,
			   void (*cb_func)(struct m4sensorhub_data *, void *),
			   void *data);
int m4sensorhub_panic_unregister(struct m4sensorhub_data *m4sensorhub,
				enum m4sensorhub_panichdl_index index);
void m4sensorhub_panic_process(struct m4sensorhub_data *m4sensorhub);

/* all M4 based drivers need to register an init call with the core,
 this callback will be executed once M4 core has properly set up FW
 on M4. For registration, a callback and a void* is passed in. When
 the callback is executed, the client provided void* is passed back
 as part of (init_calldata).p_data */
int m4sensorhub_register_initcall(int(*initfunc)(struct init_calldata *),
							void *pdata);
void m4sensorhub_unregister_initcall(
		int(*initfunc)(struct init_calldata *));

/*
 * Some M4 drivers (e.g., RTC) require reading data on boot, even if M4
 * needs a firmware update.  These functions allow drivers to register
 * callbacks with the core to take care of small maintenance tasks before
 * M4 is reflashed (e.g., caching the system time).
 *
 * NOTE:  Drivers should not rely on this call for normal operation.
 *        Reflashing M4 is an uncommon event, and most of the time,
 *        especially in production, these callbacks will never be used.
 */
int m4sensorhub_register_preflash_callback(
		int(*initfunc)(struct init_calldata *), void *pdata);
void m4sensorhub_unregister_preflash_callback(
		int(*initfunc)(struct init_calldata *));
void m4sensorhub_call_preflash_callbacks(void); /* For FW flash core */
bool m4sensorhub_preflash_callbacks_exist(void); /* For FW flash core */

int m4sensorhub_irq_disable_all(struct m4sensorhub_data *m4sensorhub);

/* External System Calls for Non-M4 Drivers */
int m4sensorhub_extern_init(struct m4sensorhub_data *m4); /* Init for core */
/* Utility function called by display driver to sync
display status to M4 */
int m4sensorhub_extern_set_display_status(uint8_t status);

#endif /* __KERNEL__ */
#endif  /* __M4SENSORHUB_H__ */
