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

struct __packed vr_data {
	struct __packed accel_data {
		uint16_t x;
		uint16_t y;
		uint16_t z;
	} accel;
	struct __packed gyro_data {
		uint16_t x;
		uint16_t y;
		uint16_t z;
	} gyro;
	struct __packed mag_data {
		uint16_t x;
		uint16_t y;
		uint16_t z;
		uint8_t calibrated_status;
	} mag;
	uint8_t timestamp[3];
	uint8_t status;
};

#define VR_BUFFERED_SAMPLES 2
extern volatile struct vr_data vmm_vr_data[VR_BUFFERED_SAMPLES];
extern volatile uint8_t vmm_vr_mode;

#endif /*_UAPI_MOTOSH_VMM_DEFINES_H*/
