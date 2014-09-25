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

#define M4SH_IRQ_INT0_INDEX 0
#define M4SH_IRQ_INT1_INDEX 8
#define M4SH_IRQ_INT2_INDEX 16

/* Keep name mapping in m4sensorhub-irq.c 'irq_name' synchronized */
enum m4sensorhub_irqs {
	/* 1st supported interrupt */
	M4SH_IRQ__START,

	/* INT0 0x01: Pressure interrupt */
	M4SH_IRQ_PRESSURE_DATA_READY = M4SH_IRQ_INT0_INDEX,

	/* INT0 0x02: Temperature interrupt */
	M4SH_IRQ_TMP_DATA_READY,

	/* INT0 0x04: Gyro interrupt */
	M4SH_IRQ_GYRO_DATA_READY,

	/* INT0 0x80: Pedometer interrupt */
	M4SH_IRQ_PEDOMETER_DATA_READY,

	/* INT0 0x10: Compass data interrupt */
	M4SH_IRQ_COMPASS_DATA_READY,

	/* INT0 0x20: Fusion data interrupt */
	M4SH_IRQ_FUSION_DATA_READY,

	/* INT0 0x40: Accel data interrupt */
	M4SH_IRQ_ACCEL_DATA_READY,

	/* INT0 0x80: Gesture interrupt */
	M4SH_IRQ_GESTURE_DETECTED,

	/* INT1 0x01 : still mode interrupt */
	M4SH_IRQ_STILL_DETECTED = M4SH_IRQ_INT1_INDEX,

	/* INT1 0x02 : motion detected interrupt */
	M4SH_IRQ_MOTION_DETECTED,

	/* INT1 0x04 : activity change interrupt */
	M4SH_IRQ_ACTIVITY_CHANGE,

	/* INT1 0x08 : download command complete interrupt */
	M4SH_IRQ_DLCMD_RESP_READY,

	/* INT1 0x10 : mic data ready interrupt */
	M4SH_IRQ_MIC_DATA_READY,

	/* INT1 0x20 : wrist command interrupt */
	M4SH_IRQ_WRIST_READY,

	/* INT1 0x40 : passive buffer full */
	M4SH_IRQ_PASSIVE_BUFFER_FULL,

	/* INT1 0x80 : lightsensor data ready */
	M4SH_IRQ_LIGHTSENSOR_DATA_READY,

	/* INT2 0x01 : heartratesensor data ready */
	M4SH_IRQ_HRSENSOR_DATA_READY = M4SH_IRQ_INT2_INDEX,

	/* INT2 0x02 : AP-RTC alarm expired */
	M4SH_IRQ_AP_ALARM_EXPIRED,

	/* INT2 0x04 : HR data ready */
	M4SH_IRQ_HEARTRATE_DATA_READY,

	/* Number of allocated interrupts */
	M4SH_IRQ__NUM
};
#endif  /* __M4SENSORHUB_IRQS_H__ */

