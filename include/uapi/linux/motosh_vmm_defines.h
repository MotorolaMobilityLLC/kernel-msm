/*
 * Copyright (c) 2016, Motorola Mobility LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     - Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     - Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     - Neither the name of Motorola Mobility nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MOTOROLA MOBILITY LLC BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef _UAPI_MOTOSH_VMM_DEFINES_H
#define _UAPI_MOTOSH_VMM_DEFINES_H

#define VMM_STOWED_EVENT_S 14
extern volatile uint8_t vmm_stowed_event[VMM_STOWED_EVENT_S];

#define VMM_PROXIMITY_DATA_S 14
extern volatile uint8_t vmm_proximity_data[VMM_PROXIMITY_DATA_S];

#define VMM_ALGO_EVT_ACCUM_MODALITY_S 2
extern volatile uint8_t
	vmm_algo_evt_accum_modality[VMM_ALGO_EVT_ACCUM_MODALITY_S];

#define VMM_ALGO_EVT_ACCUM_MVMT_S 4
extern volatile uint8_t
	vmm_algo_evt_accum_mvmt[VMM_ALGO_EVT_ACCUM_MVMT_S];

/* VR defines */
#define MAX_VR_RATE_NS    2500000
#define VR_ACCEL          0x0001
#define VR_GYRO           0x0002
#define VR_MAG            0x0004
#define VR_ALS            0x0008
#define VR_DISPLAY_ROTATE 0x0010
#define VR_GRAVITY        0x0020
#define VR_LINEAR_ACCEL   0x0040
#define VR_ROTV_6AXIS     0x0080
#define VR_ROTV_9AXIS     0x0100
#define VR_ROTV_GAME      0x0200
#define VR_READY          0x8000

struct __packed vr_sensor {
	int16_t x;
	int16_t y;
	int16_t z;
};

struct __packed vr_rotation_vector {
	int16_t x;
	int16_t y;
	int16_t z;
	int16_t cosine;
};

/* Designed so that the size of this structure is less
 * than the QC HW Fifo size on msm8996.  Some of the
 * data members are unioned to meet this size.  The
 * unioned members will report at a slower rate. */
struct __packed vr_data {

	struct vr_sensor accel;

	union linear_accel_gravity_data {
		struct vr_sensor linear_accel;
		struct vr_sensor gravity;
	} la_g_union;

	struct __packed gyro_data {
		struct vr_sensor raw;
		struct vr_sensor cal;
	} gyro;

	struct vr_rotation_vector rv_9axis;
	struct vr_rotation_vector rv_game;

	union {
		struct vr_rotation_vector rv_6axis;

		struct __packed mag_data {
			struct vr_sensor raw;
			struct vr_sensor cal;
			int8_t calibrated_status;
		} mag;
	} mag_game_rv_union;

	uint16_t als;
	uint8_t display_rotate;
	uint8_t timestamp[3];
	uint16_t status;
};

#define VR_BUFFERED_SAMPLES 2
extern volatile struct vr_data vmm_vr_data[VR_BUFFERED_SAMPLES];
extern volatile uint8_t vmm_vr_mode;

#endif /*_UAPI_MOTOSH_VMM_DEFINES_H*/
