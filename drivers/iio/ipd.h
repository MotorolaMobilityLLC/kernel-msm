/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _IPD_STEP_MOTOR_H_
#define _IPD_STEP_MOTOR_H_

#define MAX_FREQUENCY_HZ           250000
#define DEFAULT_IPD_FREQUENCY      1000
#define MIN_PULSE_CYCLE            16
#define MAX_PULSE_CYCLE            (MIN_PULSE_CYCLE * 20)

#define INCREASE_IPD_VAL           1
#define DECREASE_IPD_VAL           0
#define MAX_STEP_COUNT             3200
#define VOLTAGE_SETTLING_TIME      100

static const char * const name[] = {"LEFT", "RIGHT"};

struct ipd_data {
	struct iio_channel  *adc[2];
	struct mutex mlock[2];
	struct device *dev;
	struct platform_device *pdev;
	struct regulator *vdd;
	struct work_struct     work_queue_step_motor[2];
	struct workqueue_struct *ipd_workqueue[2];
	struct task_struct  *distance_thread[2];

	int nstep_pin[2];            /* step output pin */
	int nfault_pin[2];           /* fault input pin */
	int nvdd_pin;             /* vdd  pin */
	int nsleep_pin;           /* sleep outpin pin */
	int ndir_pin[2];             /* direction output pin */
	int nen_pin[2];              /* control output pin */
	int nthread_enable[2];       /* thread enable */

	uint32_t dir[2];             /* direction */
	uint32_t step_num[2];        /* step number */
	uint32_t counter[2];         /* counter */
	uint32_t step_freq[2];       /* step frequency */
	uint32_t adc_data[2];        /* voltage data from data */
	uint32_t sleep_interval[2];  /* interval sleep time */
	uint32_t half_period[2];     /* half period */
	uint32_t max_steps[2];       /* max steps */
	uint32_t pulse_val[2];       /* pulse */
	uint32_t hall_gpio_state[2]; /* hall gpio state */
	uint32_t pre_gpio_state[2]; /* pre gpio state */
};

enum MOTOR {
	LEFT = 0,
	RIGHT = 1,
};

enum ipd_gpio_state {
	IPD_GPIO_LOW = 0,
	IPD_GPIO_HIGH = 1,
};

#endif
