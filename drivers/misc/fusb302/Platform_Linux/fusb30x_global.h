/*
* File:   fusb30x_global.h
* Author: Tim Bremm <tim.bremm@fairchildsemi.com>
* Company: Fairchild Semiconductor
*
* Created on September 11, 2015, 15:28 AM
*/

#ifndef FUSB30X_TYPES_H
#define FUSB30X_TYPES_H

#include <linux/i2c.h>		// i2c_client, spinlock_t
#include <linux/hrtimer.h>	// hrtimer
#include <linux/usb/class-dual-role.h>
#include <linux/power_supply.h>
#include <linux/ipc_logging.h>
#include <linux/atomic.h>
#include "FSCTypes.h"		// FUSB30x custom types

#ifdef FSC_DEBUG
#define FSC_HOSTCOMM_BUFFER_SIZE    64	// Length of the hostcomm buffer
#endif // FSC_DEBUG
#define FUSB_NUM_GPIOS (3)
#define FUSB_INT_INDEX 0
#define FUSB_AUD_SW_SEL_INDEX 1
#define FUSB_AUD_DET_INDEX 2

struct fusb30x_chip		// Contains data required by this driver
{
	struct mutex lock;	// Synchronization lock

#ifdef FSC_DEBUG
	FSC_U8 dbgTimerTicks;	// Count of timer ticks
	FSC_U8 dbgTimerRollovers;	// Timer tick counter rollover counter
	FSC_U8 dbgSMTicks;	// Count of state machine ticks
	FSC_U8 dbgSMRollovers;	// State machine tick counter rollover counter
	FSC_S32 dbg_gpio_StateMachine;	// Gpio that toggles every time the state machine is triggered
	FSC_BOOL dbg_gpio_StateMachine_value;	// Value of sm toggle state machine
	char HostCommBuf[FSC_HOSTCOMM_BUFFER_SIZE];	// Buffer used to communicate with HostComm
#endif				// FSC_DEBUG

	/* Internal config data */
	FSC_S32 InitDelayMS;	// Number of milliseconds to wait before initializing the fusb30x
	FSC_S32 numRetriesI2C;	// Number of times to retry I2C reads/writes

	/* I2C */
	struct i2c_client *client;	// I2C client provided by kernel
	FSC_BOOL use_i2c_blocks;	// True if I2C_FUNC_SMBUS_I2C_BLOCK is supported

	/* GPIO */
	FSC_S32 gpio_VBus5V;	// VBus 5V GPIO pin
	FSC_BOOL gpio_VBus5V_value;	// true if active, false otherwise
	FSC_S32 gpio_VBusOther;	// VBus other GPIO pin (eg. VBus 12V) (NOTE: Optional feature - if set to <0 during GPIO init, then feature is disabled)
	FSC_BOOL gpio_VBusOther_value;	// true if active, false otherwise
	FSC_S32 gpio_IntN;	// INT_N GPIO pin

#ifdef FSC_INTERRUPT_TRIGGERED
	FSC_S32 gpio_IntN_irq;	// IRQ assigned to INT_N GPIO pin
#endif				// FSC_INTERRUPT_TRIGGERED

	/* Threads */
	struct delayed_work init_worker;	// Kicks off our runtime worker
	struct work_struct worker;	// Main state machine actions
	struct work_struct wake_worker;
	struct workqueue_struct		*wake_worker_wq;

	/* Timers */
	struct hrtimer timer_state_machine;	// High-resolution timer for the state machine
	int gpios[FUSB_NUM_GPIOS];
	bool fsa321_switch;
	bool factory_mode;
	struct dual_role_phy_instance *dual_role;
	struct dual_role_phy_desc *desc;
	struct dentry *debug_root;
	u32 debug_address;
	atomic_t irq_disabled;
};

extern struct fusb30x_chip *g_chip;
/* Variable to indicate switch state for SS lines */
extern struct power_supply usbc_psy;
extern int fusb_power_supply_set_property(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *val);
extern int fusb_power_supply_is_writeable(struct power_supply *psy,
		enum power_supply_property prop);
extern int fusb_power_supply_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val);
extern FSC_U8 GetTypeCSMControl(void);
extern void GetDeviceTypeCStatus(FSC_U8 abytData[]);
extern struct power_supply switch_psy;
struct fusb30x_chip *fusb30x_GetChip(void);	// Getter for the global chip structure
void fusb30x_SetChip(struct fusb30x_chip *newChip);	// Setter for the global chip structure
extern void *fusb302_ipc_log;
extern bool debug_audio;
#ifdef FSC_DEBUG
#define FUSB_LOG(fmt, args...) do {                         \
if (fusb302_ipc_log)   \
	ipc_log_string(fusb302_ipc_log, \
				   fmt, ## args); \
pr_debug(fmt, ## args);  \
} while (0)
#else
#define FUSB_LOG(fmt, args...)
#endif

#endif /* FUSB30X_TYPES_H */
