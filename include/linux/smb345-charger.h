/*
 * include/linux/smb345-charger.h
 *
 */

#ifndef __LINUX_smb345_CHARGER_H
#define __LINUX_smb345_CHARGER_H

#include <linux/regulator/machine.h>
#include <linux/wakelock.h>

#define SMB_DEBUG			0
#if SMB_DEBUG
#define SMB_INFO(format, arg...)	\
	printk(KERN_INFO "smb345_charger: [%s] " format , __func__ , ## arg)
#else
#define SMB_INFO(format, arg...)
#endif

#define SMB_NOTICE(format, arg...)	\
	printk(KERN_NOTICE "smb345_charger: [%s] " format , __func__ , ## arg)

#define SMB_ERR(format, arg...)	\
	printk(KERN_ERR "smb345_charger: [%s] " format , __func__ , ## arg)

/* Stress Test */
#define SMB345_STRESS_TEST	1
#ifdef SMB345_STRESS_TEST
#define SMB345_IOC_MAGIC	0xEC
#define SMB345_IOC_MAXNR	1
#define SMB345_POLLING_ONOFF	_IOR(SMB345_IOC_MAGIC, 1, int)
#define POLLING_STOP (0)
#define POLLING_START (1)
#endif

/* Debug setting */
#define REG_POLLING_RATE	90

enum charging_states {
	idle,
	progress,
	completed,
	stopped,
};

enum cable_type {
	non_cable =0,
	usb_cable,
	unknow_cable,
	ac_cable,
};

enum charger_type {
	CHARGER_NONE = 0,
	CHARGER_CDP,
	CHARGER_DCP,
	CHARGER_OTHER,
	CHARGER_SDP,
	CHARGER_ACA,
	CHARGER_TBD,
};

enum apsd_results {
	apsd_not_run = 0,
	apsd_CDP,
	apsd_DCP,
	apsd_OCP,
	apsd_SDP,
	apsd_ACA,
	apsd_TBD,
};

typedef void (*charging_callback_t)(enum charging_states state,
enum charger_type chrg_type, void *args);

struct smb345_charger {
	struct i2c_client	*client;
	struct device	*dev;
	#ifdef SMB345_STRESS_TEST
	struct delayed_work	smb345_stress_test_work;
	#endif
	struct delayed_work	inok_isr_work;
	struct delayed_work	cable_det_work;
	struct delayed_work	wireless_isr_work;
	struct delayed_work	wireless_det_work;
	struct delayed_work	wireless_set_current_work;
	struct mutex		apsd_lock;
	struct mutex		usb_lock;
	struct mutex		pinctrl_lock;
	enum charging_states state;
	enum charger_type chrg_type;
	enum cable_type cur_cable_type;
	enum cable_type old_cable_type;
	unsigned long time_of_1800mA_limit;
	unsigned int curr_limit;
	unsigned int wpc_pok_gpio;
	unsigned int wpc_en1;
	unsigned int wpc_en2;
	unsigned int wpc_curr_limit;
	int wpc_curr_limit_count;
};

struct smb345_platform_data {
	unsigned int wpc_pok_gpio;
	unsigned int wpc_en1;
	unsigned int wpc_en2;
};

#endif /*__LINUX_smb345_CHARGER_H */

