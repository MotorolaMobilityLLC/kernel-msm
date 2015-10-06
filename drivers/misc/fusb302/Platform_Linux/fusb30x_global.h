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
#include "GenericTypeDefs.h"	// FUSB30x custom types

struct fusb30x_chip		// Contains data required by this driver
{
	struct mutex lock;	// Synchronization lock

#ifdef DEBUG
	u8 dbgTimerTicks;	// Count of timer ticks
	u8 dbgTimerRollovers;	// Timer tick counter rollover counter
	u8 dbgSMTicks;		// Count of state machine ticks
	u8 dbgSMRollovers;	// State machine tick counter rollover counter
	int dbg_gpio_StateMachine;	// Gpio that toggles every time the state machine is triggered
	int dbg_gpio_StateMachine_value;	// Value of sm toggle state machine
#endif

	/* Internal config data */
	INT InitDelayMS;	// Number of milliseconds to wait before initializing the fusb30x
	INT numRetriesI2C;	// Number of times to retry I2C reads/writes

	/* HostComm */
	char HostCommBuf[FSC_HOSTCOMM_BUFFER_SIZE];	// Buffer used to communicate with HostComm

	/* I2C */
	struct i2c_client *client;	// I2C client provided by kernel
	bool use_i2c_blocks;	// True if I2C_FUNC_SMBUS_I2C_BLOCK is supported

	/* GPIO */
	int gpio_VBus5V;	// VBus 5V GPIO pin
	bool gpio_VBus5V_value;	// true if active, false otherwise
	int gpio_VBusOther;	// VBus other GPIO pin (eg. VBus 12V) (NOTE: Optional feature - if set to <0 during GPIO init, then feature is disabled)
	bool gpio_VBusOther_value;	// true if active, false otherwise
	int gpio_IntN;		// INT_N GPIO pin

#ifdef FSC_INTERRUPT_TRIGGERED
	int gpio_IntN_irq;	// IRQ assigned to INT_N GPIO pin
#endif				// FSC_INTERRUPT_TRIGGERED

	/* Threads */
	struct delayed_work init_worker;	// Kicks off our runtime worker
	struct work_struct worker;	// Main state machine actions

	/* Timers */
	struct hrtimer timer_state_machine;	// High-resolution timer for the state machine
};

extern struct fusb30x_chip *g_chip;

struct fusb30x_chip *fusb30x_GetChip(void);	// Getter for the global chip structure
void fusb30x_SetChip(struct fusb30x_chip *newChip);	// Setter for the global chip structure

#endif /* FUSB30X_TYPES_H */
