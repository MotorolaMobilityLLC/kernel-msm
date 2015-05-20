/*
 * Copyright (c) 2012, Motorola, Inc. All Rights Reserved.
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

#ifndef __M4SENSORHUB_IRQS_H__
#define __M4SENSORHUB_IRQS_H__

/* Keep name mapping in m4sensorhub-irq.c 'irq_name' synchronized */
enum m4sensorhub_irqs {
	M4SH_NOWAKEIRQ__START = 0,

	M4SH_NOWAKEIRQ_ACCEL = M4SH_NOWAKEIRQ__START,

	M4SH_NOWAKEIRQ_GYRO,

	M4SH_NOWAKEIRQ_COMPASS,

	M4SH_NOWAKEIRQ_FUSION,

	M4SH_NOWAKEIRQ_PRESSURE,

	M4SH_NOWAKEIRQ_ADS,

	M4SH_NOWAKEIRQ_PPG,

	M4SH_NOWAKEIRQ_HEARTRATE,

	M4SH_NOWAKEIRQ_PEDOMETER,

	M4SH_NOWAKEIRQ_ALS,

	M4SH_NOWAKEIRQ_ACTIVITY_CHANGE,

	M4SH_NOWAKEIRQ__MAX,

	M4SH_WAKEIRQ__START = M4SH_NOWAKEIRQ__MAX,

	M4SH_WAKEIRQ_STILL = M4SH_WAKEIRQ__START,

	M4SH_WAKEIRQ_MOTION,

	M4SH_WAKEIRQ_GESTURE,

	M4SH_WAKEIRQ_PASSIVE,

	M4SH_WAKEIRQ_AP_ALARM_EXPIRED,

	M4SH_WAKEIRQ_M4_READY,

	M4SH_WAKEIRQ_SIGNIFICANT_MOTION,

	M4SH_WAKEIRQ__MAX,
	
	M4SH_IRQ__NUM = M4SH_WAKEIRQ__MAX,
};
#endif  /* __M4SENSORHUB_IRQS_H__ */

