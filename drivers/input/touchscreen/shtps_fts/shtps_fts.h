/*
 * Copyright (c) 2016, Sharp. All rights reserved.
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
#ifndef __SHTPS_FTS_H__
#define __SHTPS_FTS_H__
/* --------------------------------------------------------------------------- */
#include <linux/module.h>
#include <linux/wakelock.h>
#include <linux/slab.h>
#include <linux/pm_qos.h>
#include <linux/version.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/cdev.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/i2c.h>

#include <linux/input/shtps_dev.h>

#include "shtps_fts_cfg_ft8607.h"

/* ===================================================================================
 * Common
 */
#define SHTPS_ERR_CHECK(check, label) \
	if((check)) goto label

/* ===================================================================================
 * Structure / enum
 */
struct fingers{
	unsigned short	x;
	unsigned short	y;
	unsigned char	state;
	unsigned char	wx;
	unsigned char	wy;
	unsigned char	z;
};

struct shtps_touch_info {
	struct fingers		fingers[SHTPS_FINGER_MAX];
	unsigned char		finger_num;
};

struct shtps_irq_info{
	int							irq;
	u8							state;
	u8							wake;
};

struct shtps_state_info{
	int							state;
};

enum{
	SHTPS_DETER_SUSPEND_I2C_PROC_IRQ = 0x00,
	SHTPS_DETER_SUSPEND_I2C_PROC_SETSLEEP,
	SHTPS_DETER_SUSPEND_I2C_PROC_OPEN,
	SHTPS_DETER_SUSPEND_I2C_PROC_CLOSE,
	SHTPS_DETER_SUSPEND_I2C_PROC_ENABLE,

	SHTPS_DETER_SUSPEND_I2C_PROC_NUM,
};

struct shtps_deter_suspend_i2c{
	u8							suspend;
	struct work_struct			pending_proc_work;
	u8							wake_lock_state;
	struct wake_lock			wake_lock;
	struct pm_qos_request		pm_qos_lock_idle;

	struct shtps_deter_suspend_i2c_pending_info{
		u8						pending;
		u8						param;
	} pending_info[SHTPS_DETER_SUSPEND_I2C_PROC_NUM];

	u8							suspend_irq_state;
	u8							suspend_irq_wake_state;
	u8							suspend_irq_detect;
};

struct shtps_touch_state {
	u8				numOfFingers;
};

#ifdef CONFIG_OF
	; // nothing
#else /* CONFIG_OF */
struct shtps_platform_data {
	int (*setup)(struct device *);
	void (*teardown)(struct device *);
	int gpio_rst;
};
#endif /* CONFIG_OF */

/* -------------------------------------------------------------------------- */
struct shtps_fts {
	struct input_dev*			input;
	int							rst_pin;
	struct shtps_irq_info		irq_mgr;

	struct shtps_touch_info		fw_report_info;
	struct shtps_touch_info		fw_report_info_store;
	struct shtps_touch_info		report_info;

	struct shtps_state_info		state_mgr;
	struct shtps_touch_state	touch_state;
	char						phys[32];
	u8							is_lcd_on;

	struct shtps_deter_suspend_i2c		deter_suspend_i2c;

	/* ------------------------------------------------------------------------ */
	/* acync */
	struct workqueue_struct		*workqueue_p;
	struct work_struct			work_data;
	struct list_head			queue;
	spinlock_t					queue_lock;
	struct shtps_req_msg		*cur_msg_p;
	struct wake_lock			work_wake_lock;
	struct pm_qos_request		work_pm_qos_lock_idle;

	struct shtps_cpu_idle_sleep_ctrl_info				*cpu_idle_sleep_ctrl_p;
	struct shtps_cpu_sleep_ctrl_fwupdate_info			*cpu_sleep_ctrl_fwupdate_p;

	/* ------------------------------------------------------------------------ */
	struct device				*ctrl_dev_p;
	struct shtps_fwctl_info		*fwctl_p;
};

/* ----------------------------------------------------------------------------
*/
enum{
	SHTPS_EVENT_TU,
	SHTPS_EVENT_TD,
	SHTPS_EVENT_DRAG,
	SHTPS_EVENT_MTDU,
};

enum{
	SHTPS_TOUCH_STATE_NO_TOUCH		= 0x00,
	SHTPS_TOUCH_STATE_FINGER		= 0x01,
};

enum{
	SHTPS_IRQ_WAKE_DISABLE,
	SHTPS_IRQ_WAKE_ENABLE,
};

enum{
	SHTPS_IRQ_STATE_DISABLE,
	SHTPS_IRQ_STATE_ENABLE,
};

enum{
	SHTPS_EVENT_START,
	SHTPS_EVENT_STOP,
	SHTPS_EVENT_SLEEP,
	SHTPS_EVENT_WAKEUP,
	SHTPS_EVENT_STARTLOADER,
	SHTPS_EVENT_INTERRUPT,
};

enum{
	SHTPS_STATE_IDLE,
	SHTPS_STATE_ACTIVE,
	SHTPS_STATE_BOOTLOADER,
	SHTPS_STATE_SLEEP,
};

enum{
	SHTPS_FUNC_REQ_EVEMT_OPEN = 0,
	SHTPS_FUNC_REQ_EVEMT_CLOSE,
	SHTPS_FUNC_REQ_EVEMT_ENABLE,
	SHTPS_FUNC_REQ_EVEMT_DISABLE,
	SHTPS_FUNC_REQ_EVEMT_LCD_ON,
	SHTPS_FUNC_REQ_EVEMT_LCD_OFF,
	SHTPS_FUNC_REQ_BOOT_FW_UPDATE,
};

enum{
	SHTPS_DEV_STATE_SLEEP = 0,
	SHTPS_DEV_STATE_ACTIVE,
	SHTPS_DEV_STATE_LOADER,
};

/* ----------------------------------------------------------------------------
*/
extern struct shtps_fts*	gShtps_fts;

int shtps_device_setup(int irq, int rst);
void shtps_device_teardown(int irq, int rst);
void shtps_device_reset(int rst);
void shtps_device_poweroff_reset(int rst);

void shtps_mutex_lock_ctrl(void);
void shtps_mutex_unlock_ctrl(void);
void shtps_mutex_lock_loader(void);
void shtps_mutex_unlock_loader(void);
void shtps_mutex_lock_proc(void);
void shtps_mutex_unlock_proc(void);

void shtps_system_set_sleep(struct shtps_fts *ts);
void shtps_system_set_wakeup(struct shtps_fts *ts);

int shtps_check_suspend_state(struct shtps_fts *ts, int proc, u8 param);
void shtps_set_suspend_state(struct shtps_fts *ts);
void shtps_clr_suspend_state(struct shtps_fts *ts);

int shtps_start(struct shtps_fts *ts);
void shtps_shutdown(struct shtps_fts *ts);

void shtps_reset(struct shtps_fts *ts);



u16 shtps_fwver(struct shtps_fts *ts);
u16 shtps_fwver_builtin(struct shtps_fts *ts);
int shtps_fwsize_builtin(struct shtps_fts *ts);
unsigned char* shtps_fwdata_builtin(struct shtps_fts *ts);

int shtps_get_fingermax(struct shtps_fts *ts);
int shtps_get_diff(unsigned short pos1, unsigned short pos2);
int shtps_get_fingerwidth(struct shtps_fts *ts, int num, struct shtps_touch_info *info);
void shtps_set_eventtype(u8 *event, u8 type);
void shtps_report_touch_on(struct shtps_fts *ts, int finger, int x, int y, int w, int wx, int wy, int z);
void shtps_report_touch_off(struct shtps_fts *ts, int finger, int x, int y, int w, int wx, int wy, int z);

void shtps_event_report(struct shtps_fts *ts, struct shtps_touch_info *info, u8 event);

void shtps_irq_disable(struct shtps_fts *ts);
void shtps_irq_enable(struct shtps_fts *ts);

void shtps_read_touchevent(struct shtps_fts *ts, int state);

int shtps_enter_bootloader(struct shtps_fts *ts);
int request_event(struct shtps_fts *ts, int event, int param);

void shtps_event_force_touchup(struct shtps_fts *ts);

int shtps_fw_update(struct shtps_fts *ts, const unsigned char *fw_data, int fw_size);

void shtps_sleep(struct shtps_fts *ts, int on);


/* -------------------------------------------------------------------------- */
struct shtps_cpu_idle_sleep_ctrl_info{
	struct pm_qos_request		qos_cpu_latency;
	int							wake_lock_idle_state;
};

void shtps_wake_lock_idle(struct shtps_fts *ts);
void shtps_wake_unlock_idle(struct shtps_fts *ts);
void shtps_cpu_idle_sleep_wake_lock_init( struct shtps_fts *ts );
void shtps_cpu_idle_sleep_wake_lock_deinit( struct shtps_fts *ts );

/* -------------------------------------------------------------------------- */
struct shtps_cpu_sleep_ctrl_fwupdate_info{
	struct wake_lock			wake_lock_for_fwupdate;
	struct pm_qos_request		qos_cpu_latency_for_fwupdate;
	int							wake_lock_for_fwupdate_state;
};

void shtps_wake_lock_for_fwupdate(struct shtps_fts *ts);
void shtps_wake_unlock_for_fwupdate(struct shtps_fts *ts);
void shtps_fwupdate_wake_lock_init( struct shtps_fts *ts );
void shtps_fwupdate_wake_lock_deinit( struct shtps_fts *ts );

/* -------------------------------------------------------------------------- */
struct shtps_req_msg;

void shtps_func_request_async( struct shtps_fts *ts, int event);
int shtps_func_request_sync( struct shtps_fts *ts, int event);
int shtps_func_async_init( struct shtps_fts *ts);
void shtps_func_async_deinit( struct shtps_fts *ts);
/* -------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------- */
#define FTS_MAX_POINTS			10

#define FTS_META_REGS			3
#define FTS_ONE_TCH_LEN			6
#define FTS_TCH_LEN(x)			(FTS_META_REGS + (FTS_ONE_TCH_LEN * (x)))

#define FTS_PRESS				0x7F
#define FTS_TOUCH_X_H_POS		0
#define FTS_TOUCH_X_L_POS		1
#define FTS_TOUCH_Y_H_POS		2
#define FTS_TOUCH_Y_L_POS		3
#define FTS_TOUCH_WEIGHT		4
#define FTS_TOUCH_AREA			5
#define FTS_TOUCH_POINT_NUM		2
#define FTS_TOUCH_EVENT_POS		0
#define FTS_TOUCH_ID_POS		2

#define FTS_TOUCH_DOWN			0
#define FTS_TOUCH_UP			1
#define FTS_TOUCH_CONTACT		2

#define FTS_REG_FW_VER						0xA6
#define FTS_REG_MODEL_VER					0xA8
#define FTS_REG_PMODE						0xA5
#define FTS_REG_STATUS						0x01

/* power register bits*/
#define FTS_PMODE_ACTIVE					0x00
#define FTS_PMODE_MONITOR					0x01
#define FTS_PMODE_STANDBY					0x02
#define FTS_PMODE_HIBERNATE					0x03

/*-----------------------------------------------------------
Error Code for Comm
-----------------------------------------------------------*/
#define ERROR_CODE_OK						0
#define ERROR_CODE_CHECKSUM_ERROR			-1
#define ERROR_CODE_INVALID_COMMAND			-2
#define ERROR_CODE_INVALID_PARAM			-3
#define ERROR_CODE_IIC_WRITE_ERROR			-4
#define ERROR_CODE_IIC_READ_ERROR			-5
#define ERROR_CODE_WRITE_USB_ERROR			-6
#define ERROR_CODE_WAIT_RESPONSE_TIMEOUT	-7
#define ERROR_CODE_PACKET_RE_ERROR			-8
#define ERROR_CODE_NO_DEVICE				-9
#define ERROR_CODE_WAIT_WRITE_TIMEOUT		-10
#define ERROR_CODE_READ_USB_ERROR			-11
#define ERROR_CODE_COMM_ERROR				-12
#define ERROR_CODE_ALLOCATE_BUFFER_ERROR	-13
#define ERROR_CODE_DEVICE_OPENED			-14
#define ERROR_CODE_DEVICE_CLOSED			-15

/*-----------------------------------------------------------
FW Upgrade
-----------------------------------------------------------*/
#define FTS_PACKET_LENGTH					120
#define FTS_UPGRADE_AA						0xAA
#define FTS_UPGRADE_55						0x55
#define FTS_RST_DELAY_AA_MS					2

#define FTS_UPGRADE_LOOP					30

#define FTS_LEN_FLASH_ECC_MAX				0xFFFE

/* --------------------------------------------------------------------------- */
struct shtps_fwctl_info{
	void							*tps_ctrl_p;	/* same as 'struct i2c_client* client;' */
	struct shtps_ctrl_functbl		*devctrl_func_p;

	u8								dev_state;
};

/* --------------------------------------------------------------------------- */
int shtps_fwctl_ic_init(struct shtps_fts *ts_p);
int shtps_fwctl_loader_write_pram(struct shtps_fts *ts_p, u8 *pbt_buf, u32 dw_lenth);
int shtps_fwctl_loader_upgrade(struct shtps_fts *ts_p, u8 *pbt_buf, u32 dw_lenth);
int shtps_fwctl_get_device_status(struct shtps_fts *ts_p, u8 *status_p);
int shtps_fwctl_set_active(struct shtps_fts *ts_p);
int shtps_fwctl_set_sleepmode_on(struct shtps_fts *ts_p);
int shtps_fwctl_set_sleepmode_off(struct shtps_fts *ts_p);
int shtps_fwctl_get_fingermax(struct shtps_fts *ts_p);
int shtps_fwctl_get_fingerinfo(struct shtps_fts *ts_p, u8 *buf_p, int read_cnt, u8 *irqsts_p, u8 *extsts_p, u8 **finger_pp);
int shtps_fwctl_get_num_of_touch_fingers(struct shtps_fts *ts_p, u8 *buf_p);
u8* shtps_fwctl_get_finger_info_buf(struct shtps_fts *ts_p, int fingerid, int fingerMax, u8 *buf_p);
int shtps_fwctl_get_finger_state(struct shtps_fts *ts_p, int fingerid, int fingerMax, u8 *buf_p);
int shtps_fwctl_get_finger_pointid(struct shtps_fts *ts_p, u8 *buf_p);
int shtps_fwctl_get_finger_pos_x(struct shtps_fts *ts_p, u8 *buf_p);
int shtps_fwctl_get_finger_pos_y(struct shtps_fts *ts_p, u8 *buf_p);
int shtps_fwctl_get_finger_wx(struct shtps_fts *ts_p, u8 *buf_p);
int shtps_fwctl_get_finger_wy(struct shtps_fts *ts_p, u8 *buf_p);
int shtps_fwctl_get_finger_z(struct shtps_fts *ts_p, u8 *buf_p);
int shtps_fwctl_get_fwver(struct shtps_fts *ts_p, u16 *ver_p);
int shtps_fwctl_initparam(struct shtps_fts *ts_p);
void shtps_fwctl_set_dev_state(struct shtps_fts *ts_p, u8 state);
u8 shtps_fwctl_get_dev_state(struct shtps_fts *ts_p);
int shtps_fwctl_get_maxXPosition(struct shtps_fts *ts_p);
int shtps_fwctl_get_maxYPosition(struct shtps_fts *ts_p);

int shtps_fwctl_init(struct shtps_fts *, void *, struct shtps_ctrl_functbl *);
void shtps_fwctl_deinit(struct shtps_fts *);
/* --------------------------------------------------------------------------- */
typedef int (*tps_write_t)(void *, u16, u8);
typedef int (*tps_read_t)( void *, u16, u8 *, u32);
typedef int (*tps_write_packet_t)(void *, u16, u8 *, u32);
typedef int (*tps_read_packet_t)( void *, u16, u8 *, u32);
typedef int (*tps_direct_write_t)(void *, u8 *, u32);
typedef int (*tps_direct_read_t)( void *, u8 *, u32, u8 *, u32);

struct shtps_ctrl_functbl{
	tps_write_t				write_f;
	tps_read_t				read_f;
	tps_write_packet_t		packet_write_f;
	tps_read_packet_t		packet_read_f;
	tps_direct_write_t		direct_write_f;
	tps_direct_read_t		direct_read_f;
};

#define M_WRITE_FUNC(A, B, C)				(A)->devctrl_func_p->write_f((A)->tps_ctrl_p, B, C)
#define M_READ_FUNC(A, B, C, D)				(A)->devctrl_func_p->read_f((A)->tps_ctrl_p, B, C, D)
#define M_WRITE_PACKET_FUNC(A, B, C, D)		(A)->devctrl_func_p->packet_write_f((A)->tps_ctrl_p, B, C, D)
#define M_READ_PACKET_FUNC(A, B, C, D)		(A)->devctrl_func_p->packet_read_f((A)->tps_ctrl_p, B, C, D)
#define M_DIRECT_WRITE_FUNC(A, B, C)		(A)->devctrl_func_p->direct_write_f((A)->tps_ctrl_p, B, C)
#define M_DIRECT_READ_FUNC(A, B, C, D, E)	(A)->devctrl_func_p->direct_read_f((A)->tps_ctrl_p, B, C, D, E)

#endif /* __SHTPS_FTS_H__ */
