/*
 * ILITEK Touch IC driver
 *
 * Copyright (C) 2011 ILI Technology Corporation.
 *
 * Author: Dicky Chiang <dicky_chiang@ilitek.com>
 * Based on TDD v7.0 implemented by Mstar & ILITEK
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#ifndef __PLATFORM_H
#define __PLATFORM_H

struct ilitek_platform_data {

	struct i2c_client *client;

	struct input_dev *input_device;

	const struct i2c_device_id *i2c_id;

	struct regulator *vdd;
	struct regulator *vdd_i2c;

	struct mutex plat_mutex;
	spinlock_t plat_spinlock;

	uint32_t chip_id;

	int int_gpio;
	int reset_gpio;
	int isr_gpio;

	int delay_time_high;
	int delay_time_low;
	int edge_delay;

	bool isEnableIRQ;
	bool isEnablePollCheckPower;

#ifdef USE_KTHREAD
	struct task_struct *irq_thread;
	bool irq_trigger;
	bool free_irq_thread;
#else
	struct work_struct report_work_queue;
#endif

#ifdef CONFIG_FB
	struct notifier_block notifier_fb;
	struct workqueue_struct *ilitek_att_wq;
	struct delayed_work work_att;
#else
	struct early_suspend early_suspend;
#endif

#ifdef BOOT_FW_UPGRADE
	struct task_struct *update_thread;
#endif

	/* obtain msg when battery status has changed */
	struct delayed_work check_power_status_work;
	struct workqueue_struct *check_power_status_queue;
	unsigned long work_delay;
	bool vpower_reg_nb;

	/* Sending report data to users for the debug */
	bool debug_node_open;
	int debug_data_frame;
	wait_queue_head_t inq;
	unsigned char debug_buf[1024][2048];
	struct mutex ilitek_debug_mutex;
	struct mutex ilitek_debug_read_mutex;

	struct spi_device *spi;
	bool suspended;

	const char *TP_IC_TYPE;
	bool MT_B_TYPE;
	bool REGULATOR_POWER_ON;
	bool BATTERY_CHECK;
	bool ENABLE_BATTERY_CHECK;

	u32 x_max;
	u32 y_max;
	u32 x_min;
	u32 y_min;

	bool x_flip;
	bool y_flip;

	int TPD_WIDTH;
	int TPD_HEIGHT;
	int MAX_TOUCH_NUM;

};

extern struct ilitek_platform_data *ipd;
extern struct tpd_device *tpd;
/* exported from platform.c */
extern void ilitek_platform_disable_irq(void);
extern void ilitek_platform_enable_irq(void);
extern int ilitek_platform_read_tp_info(void);
extern void ilitek_platform_tp_hw_reset(bool isEnable);
#ifdef ENABLE_REGULATOR_POWER_ON
extern void ilitek_regulator_power_on(bool status);
#endif

/* exported from userspsace.c */
extern void netlink_reply_msg(void *raw, int size);
extern int ilitek_proc_init(void);
extern void ilitek_proc_remove(void);
extern int ilitek_sys_init(void);
extern void ilitek_sys_remove(void);

#endif /* __PLATFORM_H */
