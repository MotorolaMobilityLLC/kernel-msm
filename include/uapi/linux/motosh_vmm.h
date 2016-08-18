/* vim: set ts=8 sw=8 tw=80 cc=+1 noet: */

/*********************************************************************
*
*   Copyright (C) 2015 Motorola, Inc.
*
*********************************************************************/

/*
No #ifdef guard here on purposes, since this file is not a true header file
and may be included multiple times in the same c file.

The contents of this file constitute the list that will be used by an X-macro
expansion. Look up X-macros for more details on the technique, and see the end
of this file for some example expansions.

The intent is to maintain a single file describing the SensorHub registers and
make it usable from the SensorHub FW, the kernel, the HAL, etc...

Each VMM entry below consists of:
	- Register number
	- Name of the register
	- A flag indicating if the register is writable (TRUE/FALSE)
	- Memory location of the register contents
	- Length of the register contents, AKA the read/write size. A value of 0
	  could mean either that the register is unused, or that the size is
	  not a constant (ex: firmware string).
*/

VMM_ENTRY(0x00, ID,							TRUE,
		&vmm_id,
		sizeof(vmm_id))
VMM_ENTRY(0x01, REV_ID,							FALSE,
		vmm_rev_id,
		sizeof(vmm_rev_id))
VMM_ENTRY(0x02, UNUSED_02,						FALSE,
		0,
		0)
VMM_ENTRY(0x03, LOWPOWER_REG,						TRUE,
		vmm_lowpower_reg,
		sizeof(vmm_lowpower_reg))
VMM_ENTRY(0x04, SLAVE_I2C_ERROR,					FALSE,
		vmm_slave_i2c_error,
		sizeof(vmm_slave_i2c_error))
VMM_ENTRY(0x05, COVER_DEBOUNCE_TIME,					TRUE,
		vmm_cover_debounce_time,
		sizeof(vmm_cover_debounce_time))
VMM_ENTRY(0x06, ELAPSED_RT,						FALSE,
		vmm_elapsed_rt,
		sizeof(vmm_elapsed_rt))
VMM_ENTRY(0x07, RESET_REASON,						FALSE,
		&vmm_reset_reason,
		sizeof(vmm_reset_reason))
VMM_ENTRY(0x08, ACCEL_CAL,						TRUE,
		&vmm_accel_cal,
		sizeof(vmm_accel_cal))
#ifdef PDISPLAY
	VMM_ENTRY(0x09, PD_QUICK_PEEK,					FALSE,
			pd_quick_peek,
			sizeof(pd_quick_peek))
	VMM_ENTRY(0x0A, PD_PEEK_RESPONSE,				TRUE,
			pd_peek_response,
			sizeof(pd_peek_response))
	VMM_ENTRY(0x0B, P_DISPLAY_STATUS,				FALSE,
			vmm_p_display_status,
			sizeof(vmm_p_display_status))
	VMM_ENTRY(0x0C, TOUCH_INFO,					FALSE,
			vmm_touch_info,
			sizeof(vmm_touch_info))
	VMM_ENTRY(0x0D, P_DISPLAY_CONFIG,				TRUE,
			vmm_p_display_config,
			sizeof(vmm_p_display_config))
	VMM_ENTRY(0x0E, P_DISPLAY_DATA_VALID,				FALSE,
			&vmm_pdl.data_valid,
			sizeof(vmm_pdl.data_valid))
#else

	VMM_ENTRY(0x09, UNUSED_09,					FALSE,
			0,
			0)
	VMM_ENTRY(0x0A, UNUSED_0A,					FALSE,
			0,
			0)
	VMM_ENTRY(0x0B, UNUSED_0B,					FALSE,
			0,
			0)
	VMM_ENTRY(0x0C, UNUSED_0C,					FALSE,
			0,
			0)
	VMM_ENTRY(0x0D, UNUSED_0D,					FALSE,
			0,
			0)
	VMM_ENTRY(0x0E, UNUSED_0E,					FALSE,
			0,
			0)
#endif /*PDISPLAY*/


VMM_ENTRY(0x0F, MOTO_MOD_CURRENT_DRAIN,					FALSE,
		vmm_moto_mod_current_drain,
		sizeof(vmm_moto_mod_current_drain))


VMM_ENTRY(0x10, POSIX_TIME,						TRUE,
		vmm_posix_time,
		sizeof(vmm_posix_time))

#ifdef IRGESTURE
	VMM_ENTRY(0x11, IR_STATUS,					FALSE,
			vmm_ir_status,
			sizeof(vmm_ir_status))
	VMM_ENTRY(0x12, IR_GESTURE_RATE,				TRUE,
			&vmm_ir_gesture_rate,
			sizeof(vmm_ir_gesture_rate))
	VMM_ENTRY(0x13, IR_RAW_RATE,					TRUE,
			&vmm_ir_raw_rate,
			sizeof(vmm_ir_raw_rate))
#else
	VMM_ENTRY(0x11, UNUSED_11,					FALSE,
			0,
			0)
	VMM_ENTRY(0x12, UNUSED_12,					FALSE,
			0,
			0)
	VMM_ENTRY(0x13, UNUSED_13,					FALSE,
			0,
			0)
#endif /*IRGESTURE*/

VMM_ENTRY(0x14, VR_DATA,						FALSE,
		vmm_vr_data,
		sizeof(vmm_vr_data))
VMM_ENTRY(0x15, DERIVED_SENS_UPDATE_RATE,				TRUE,
		vmm_derived_sens_update_rate,
		sizeof(vmm_derived_sens_update_rate))
VMM_ENTRY(0x16, ACCEL_UPDATE_RATE,					TRUE,
		vmm_accel_update_cfg,
		sizeof(vmm_accel_update_cfg))
VMM_ENTRY(0x17, MAG_UPDATE_RATE,					TRUE,
		&vmm_mag_update_rate,
		sizeof(vmm_mag_update_rate))
VMM_ENTRY(0x18, PRESSURE_UPDATE_RATE,					TRUE,
		vmm_pressure_update_rate,
		sizeof(vmm_pressure_update_rate))
VMM_ENTRY(0x19, GYRO_UPDATE_RATE,					TRUE,
		&vmm_gyro_update_rate,
		sizeof(vmm_gyro_update_rate))
VMM_ENTRY(0x1A, NONWAKESENSOR_CONFIG,					TRUE,
		vmm_module_request,
		sizeof(vmm_module_request))
VMM_ENTRY(0x1B, WAKESENSOR_CONFIG,					TRUE,
		vmm_wakesensor_config,
		sizeof(vmm_wakesensor_config))

#ifdef IRGESTURE
	VMM_ENTRY(0x1C, IR_GESTURE,					FALSE,
			vmm_ir_gesture,
			sizeof(vmm_ir_gesture))
	VMM_ENTRY(0x1D, IR_RAW,						FALSE,
			vmm_ir_data,
			sizeof(vmm_ir_data))
	VMM_ENTRY(0x1E, IR_CONFIG,					TRUE,
			&vmm_ir_config,
			sizeof(vmm_ir_config))
	VMM_ENTRY(0x1F, IR_STATE,					FALSE,
			&vmm_ir_state,
			sizeof(vmm_ir_state))
#else
	VMM_ENTRY(0x1C, UNUSED_1C,					FALSE,
			0,
			0)
	VMM_ENTRY(0x1D, UNUSED_1D,					FALSE,
			0,
			0)
	VMM_ENTRY(0x1E, UNUSED_1E,					FALSE,
			0,
			0)
	VMM_ENTRY(0x1F, UNUSED_1F,					FALSE,
			0,
			0)
#endif /*IRGESTURE*/

VMM_ENTRY(0x20, MOTION_DUR,						TRUE,
		&vmm_motion_dur0,
		sizeof(vmm_motion_dur0))
VMM_ENTRY(0x21, MOTION_HISTORY,						FALSE,
		&vmm_motion_history,
		sizeof(vmm_motion_history))
VMM_ENTRY(0x22, ZMOTION_DUR,						TRUE,
		&vmm_zmotion_dur0,
		sizeof(vmm_zmotion_dur0))
VMM_ENTRY(0x23, VR_MODE,						TRUE,
		&vmm_vr_mode,
		sizeof(vmm_vr_mode))
VMM_ENTRY(0x24, BYPASS_MODE,						TRUE,
		vmm_test_mode,
		sizeof(vmm_test_mode))
VMM_ENTRY(0x25, SLAVE_ADDRESS,						TRUE,
		&vmm_slave_address,
		sizeof(vmm_slave_address))
VMM_ENTRY(0x26, ALGO_CONFIG,						TRUE,
		vmm_algo_config,
		sizeof(vmm_algo_config))
VMM_ENTRY(0x27, HEADSET_CONTROL,					TRUE,
		&vmm_headset_control,
		sizeof(vmm_headset_control))
VMM_ENTRY(0x28, GENERIC_INT,						FALSE,
		vmm_generic_int,
		sizeof(vmm_generic_int))
VMM_ENTRY(0x29, GENERIC_LISTENERS,					TRUE,
		vmm_generic_listeners,
		sizeof(vmm_generic_listeners))
VMM_ENTRY(0x2A, SENSOR_ORIENTATIONS,					TRUE,
		vmm_sensorhub_devicetree,
		sizeof(vmm_sensorhub_devicetree))
VMM_ENTRY(0x2B, FW_VERSION_LEN,						FALSE,
		&vmm_fw_version_len,
		sizeof(vmm_fw_version_len))
VMM_ENTRY(0x2C, FW_VERSION_STR,						FALSE,
		fw_version_str,
		0)
VMM_ENTRY(0x2D, MOTION_DATA,						FALSE,
		vmm_motion_data,
		sizeof(vmm_motion_data))

#ifdef DSP
	VMM_ENTRY(0x2E, DSP_ALGO_CONFIG_A,				TRUE,
			vmm_dsp_algo_config_a,
			DSP_ALGO_CONFIG_LEN)
	VMM_ENTRY(0x2F, DSP_ALGO_STATUS_A,				FALSE,
			vmm_dsp_algo_status_a,
			sizeof(vmm_dsp_algo_status_a))
#else
	VMM_ENTRY(0x2E, UNUSED_2E,					FALSE,
			0,
			0)
	VMM_ENTRY(0x2F, UNUSED_2F,					FALSE,
			0,
			0)
#endif

VMM_ENTRY(0x30, CHOPCHOP_CONFIG,					TRUE,
		vmm_chopchop_config,
		sizeof(vmm_chopchop_config))
VMM_ENTRY(0x31, LIFT_CONFIG,						TRUE,
		vmm_lift_config,
		sizeof(vmm_lift_config))
VMM_ENTRY(0x32, GLANCE_PARAMETERS,					TRUE,
		vmm_glance_config,
		sizeof(vmm_glance_config))
VMM_ENTRY(0x33, PROX_ALS_SETTINGS,					TRUE,
		vmm_prox_als_settings,
		sizeof(vmm_prox_als_settings))
VMM_ENTRY(0x34, LUX_TABLE,						TRUE,
		vmm_lux_table,
		sizeof(vmm_lux_table))
VMM_ENTRY(0x35, BRIGHTNESS_TABLE,					TRUE,
		vmm_brightness_table,
		sizeof(vmm_brightness_table))
VMM_ENTRY(0x36, TOUCH_CONFIG,						TRUE,
		vmm_touch_config,
		sizeof(vmm_touch_config))
VMM_ENTRY(0x37, UNUSED_37,						FALSE,
		0,
		0)
VMM_ENTRY(0x38, FW_CRC,							FALSE,
		&vmm_fw_crc,
		sizeof(vmm_fw_crc))
VMM_ENTRY(0x39, WAKESENSOR_STATUS,					FALSE,
		vmm_wakesensor_status,
		sizeof(vmm_wakesensor_status))
VMM_ENTRY(0x3A, HEADSET_SETTINGS,					TRUE,
		vmm_headset_settings,
		sizeof(vmm_headset_settings))
VMM_ENTRY(0x3B, ACCEL_DATA,						FALSE,
		vmm_accel_data,
		sizeof(vmm_accel_data))
VMM_ENTRY(0x3C, LINEAR_ACCEL,						FALSE,
		vmm_linear_accel,
		sizeof(vmm_linear_accel))
VMM_ENTRY(0x3D, GRAVITY,						FALSE,
		vmm_gravity,
		sizeof(vmm_gravity))
VMM_ENTRY(0x3E, STEP_COUNTER_INFO,					TRUE,
		vmm_step_counter_info,
		sizeof(vmm_step_counter_info))
VMM_ENTRY(0x3F, DOCKED_DATA,						FALSE,
		&vmm_docked_data,
		sizeof(vmm_docked_data))
VMM_ENTRY(0x40, COVER_DATA,						FALSE,
		&vmm_cover_data,
		sizeof(vmm_cover_data))
VMM_ENTRY(0x41, GAME_RV_DATA,						FALSE,
		vmm_game_rv,
		sizeof(vmm_game_rv))

#ifdef GYRO_CALIBRATION
	VMM_ENTRY(0x42, GYRO_CAL,					TRUE,
			vmm_gyro_cal,
			sizeof(vmm_gyro_cal))
#else
	VMM_ENTRY(0x42, UNUSED_42,					FALSE,
			0,
			0)
#endif

VMM_ENTRY(0x43, GYRO_DATA,						FALSE,
		vmm_gyro_data,
		sizeof(vmm_gyro_data))
VMM_ENTRY(0x44, I2C_PASSTHROUGH_COMMAND,				TRUE,
		vmm_i2c_passthrough_command,
		sizeof(vmm_i2c_passthrough_command))
VMM_ENTRY(0x45, UNCALIB_GYRO_DATA,					FALSE,
		vmm_uncalib_gyro_data,
		sizeof(vmm_uncalib_gyro_data))
VMM_ENTRY(0x46, UNCALIB_MAG_DATA,					FALSE,
		vmm_uncalib_mag_data,
		sizeof(vmm_uncalib_mag_data))
VMM_ENTRY(0x47, I2C_PASSTHROUGH_RESPONSE,				FALSE,
		vmm_i2c_passthrough_response,
		sizeof(vmm_i2c_passthrough_response))
VMM_ENTRY(0x48, MAG_CALIBRATION,					TRUE,
		vmm_mag_calibration,
		sizeof(vmm_mag_calibration))
VMM_ENTRY(0x49, MAG_DATA,						FALSE,
		vmm_mag_data,
		sizeof(vmm_mag_data))
VMM_ENTRY(0x4A, DROTATE,						FALSE,
		&vmm_drotate,
		sizeof(vmm_drotate))
VMM_ENTRY(0x4B, FLAT_DATA,						FALSE,
		&vmm_flat_data,
		sizeof(vmm_flat_data))
VMM_ENTRY(0x4C, CAMERA_GESTURE,						FALSE,
		vmm_camera_gesture,
		sizeof(vmm_camera_gesture))
VMM_ENTRY(0x4D, NFC_DATA,						FALSE,
		&vmm_nfc_data,
		sizeof(vmm_nfc_data))
VMM_ENTRY(0x4E, SIM_DATA,						FALSE,
		vmm_sim_data,
		sizeof(vmm_sim_data))
VMM_ENTRY(0x4F, CHOPCHOP_DATA,						FALSE,
		vmm_chopchop_data,
		sizeof(vmm_chopchop_data))

#ifdef IRGESTURE
	VMM_ENTRY(0x50, IR_ANALYTICS,					FALSE,
			vmm_ir_analytics,
			sizeof(vmm_ir_analytics))
#else
	VMM_ENTRY(0x50, UNUSED_50,					FALSE,
			0,
			0)
#endif /*IRGESTURE*/

VMM_ENTRY(0x51, LIFT_DATA,						FALSE,
		vmm_lift_data,
		sizeof(vmm_lift_data))
VMM_ENTRY(0x52, WAKE_MSG_QUEUE,						FALSE,
		vmm_wake_msg_queue,
		sizeof(vmm_wake_msg_queue))
VMM_ENTRY(0x53, NWAKE_STATUS,						TRUE,
		vmm_nwake_status,
		sizeof(vmm_nwake_status))
VMM_ENTRY(0x54, NWAKE_MSG_QUEUE,					FALSE,
		vmm_nwake_msg_queue,
		sizeof(vmm_nwake_msg_queue))
VMM_ENTRY(0x55, LOG_CONFIG,						TRUE,
		vmm_log_config,
		sizeof(vmm_log_config))
VMM_ENTRY(0x56, S_SENSOR_DATA,						FALSE,
		&s_sensor_data,
		sizeof(s_sensor_data))
VMM_ENTRY(0x57, WAKE_MSG_QUEUE_LEN,					TRUE,
		&vmm_wake_msg_queue_len,
		sizeof(vmm_wake_msg_queue_len))

#ifdef CAPSENSE
	VMM_ENTRY(0x58, ANTCAP_CTRL,					TRUE,
			vmm_antcap_ctrl,
			sizeof(vmm_antcap_ctrl))
	VMM_ENTRY(0x59, ANTCAP_INDEX,					TRUE,
			vmm_antcap_index,
			sizeof(vmm_antcap_index))
	VMM_ENTRY(0x5A, ANTCAP_CONFIG,					TRUE,
			vmm_antcap_config,
			sizeof(vmm_antcap_config))
	VMM_ENTRY(0x5B, ANTCAP_DEBUG,					TRUE,
			vmm_antcap_debug,
			sizeof(vmm_antcap_debug))
#else
	VMM_ENTRY(0x58, UNUSED_58,					FALSE,
			0,
			0)
	VMM_ENTRY(0x59, UNUSED_59,					FALSE,
			0,
			0)
	VMM_ENTRY(0x5A, UNUSED_5A,					FALSE,
			0,
			0)
	VMM_ENTRY(0x5B, UNUSED_5B,					FALSE,
			0,
			0)
#endif

VMM_ENTRY(0x5C, GLANCE_REG,						FALSE,
		vmm_glance_data,
		sizeof(vmm_glance_data))
VMM_ENTRY(0x5D, ALGO_ACCUM_ALL_MODALITY,				FALSE,
		vmm_algo_accum_all_modality,
		sizeof(vmm_algo_accum_all_modality))
VMM_ENTRY(0x5E, SENSOR_USAGE_LEN,					FALSE,
		&vmm_sensor_usage_len,
		sizeof(vmm_sensor_usage_len))
VMM_ENTRY(0x5F, SENSOR_USAGE,						FALSE,
		vmm_sensor_usage,
		sizeof(vmm_sensor_usage))
VMM_ENTRY(0x60, ALGO_REQ_ACCUM_MODALITY,				TRUE,
		vmm_algo_req_accum_modality,
		sizeof(vmm_algo_req_accum_modality))
VMM_ENTRY(0x61, LOG_MSG_LEN,						TRUE,
		&vmm_log_msg_len,
		sizeof(vmm_log_msg_len))
VMM_ENTRY(0x62, LOG_MSG,						FALSE,
		vmm_log_msg,
		sizeof(vmm_log_msg))
VMM_ENTRY(0x63, ALGO_EVT_ACCUM_MODALITY,				FALSE,
		vmm_algo_evt_accum_modality,
		sizeof(vmm_algo_evt_accum_modality))
VMM_ENTRY(0x64, QUATERNION_6AXIS,					FALSE,
		vmm_quaternion_6axis,
		sizeof(vmm_quaternion_6axis))
VMM_ENTRY(0x65, QUATERNION_9AXIS,					FALSE,
		vmm_quaternion_9axis,
		sizeof(vmm_quaternion_9axis))
VMM_ENTRY(0x66, PRESSURE_DATA,						FALSE,
		vmm_pressure_data,
		sizeof(vmm_pressure_data))
VMM_ENTRY(0x67, HEADSET_STATE,						FALSE,
		&vmm_headset_state,
		sizeof(vmm_headset_state))

#ifdef BLD_ALS_TEST
	VMM_ENTRY(0x68, ALS_TEST,					FALSE,
			vmm_als_test,
			sizeof(vmm_als_test))
#else
	VMM_ENTRY(0x68, UNUSED_68,					FALSE,
			0,
			0)
#endif

VMM_ENTRY(0x69, PROX_NOISE_FLOOR,					FALSE,
		&vmm_prox_noise_floor,
		sizeof(vmm_prox_noise_floor))
VMM_ENTRY(0x6A, ALS_LUX,						FALSE,
		vmm_als_lux,
		sizeof(vmm_als_lux))
VMM_ENTRY(0x6B, DISP_BRIGHTNESS_DATA,					FALSE,
		&vmm_disp_brightness_data,
		sizeof(vmm_disp_brightness_data))
VMM_ENTRY(0x6C, PROXIMITY_DATA,						FALSE,
		vmm_proximity_data,
		sizeof(vmm_proximity_data))
VMM_ENTRY(0x6D, STOWED_DATA,						FALSE,
		vmm_stowed_event,
		sizeof(vmm_stowed_event))
VMM_ENTRY(0x6E, MEMORY_INFO,						TRUE,
		vmm_memory_info,
		sizeof(vmm_memory_info))
VMM_ENTRY(0x6F, ALS_UPDATE_RATE,					TRUE,
		vmm_als_update_rate,
		sizeof(vmm_als_update_rate))
VMM_ENTRY(0x70, CHIP_ID,						FALSE,
		vmm_chip_id,
		sizeof(vmm_chip_id))
VMM_ENTRY(0x71, MM_LAST_MESSAGEPD,					FALSE,
		&mm_last_messagePD,
		sizeof(mm_last_messagePD))
VMM_ENTRY(0x72, ALGO_REQ_MODALITY,					TRUE,
		vmm_algo_req_modality,
		sizeof(vmm_algo_req_modality))
VMM_ENTRY(0x73, ALGO_REQ_ORIENT,					TRUE,
		vmm_algo_req_orient,
		sizeof(vmm_algo_req_orient))
VMM_ENTRY(0x74, ALGO_REQ_STOWED,					TRUE,
		vmm_algo_req_stowed,
		sizeof(vmm_algo_req_stowed))
VMM_ENTRY(0x75, ALGO_REQ_ACCUM_MVMT,					TRUE,
		vmm_algo_req_accum_mvmt,
		sizeof(vmm_algo_req_accum_mvmt))
VMM_ENTRY(0x76, ALGO_EVT_MODALITY,					FALSE,
		vmm_algo_evt_modality,
		sizeof(vmm_algo_evt_modality))
VMM_ENTRY(0x77, ALGO_EVT_ORIENT,					FALSE,
		vmm_algo_evt_orient,
		sizeof(vmm_algo_evt_orient))
VMM_ENTRY(0x78, ALGO_EVT_STOWED,					FALSE,
		vmm_algo_evt_stowed,
		sizeof(vmm_algo_evt_stowed))
VMM_ENTRY(0x79, ALGO_EVT_ACCUM_MVMT,					FALSE,
		vmm_algo_evt_accum_mvmt,
		sizeof(vmm_algo_evt_accum_mvmt))
VMM_ENTRY(0x7A, UNUSED_7A,						FALSE,
		0,
		0)
VMM_ENTRY(0x7B, PORT_G,							TRUE,
		0x48001800,
		0x25)
VMM_ENTRY(0x7C, PORT_H,							TRUE,
		0x48001C00,
		0x25)
VMM_ENTRY(0x7D, PORT_C,							TRUE,
		0x48000800,
		0x25)
VMM_ENTRY(0x7E, PORT_B,							TRUE,
		0x48000400,
		0x25)
VMM_ENTRY(0x7F, PORT_A,							TRUE,
		0x48000000,
		0x25)

/* Event only "registers", beyond 0x7F with no data associated, these may
   overlap into the passthrough address range since there is no physical
   address for data
*/
VMM_ENTRY(0x80, STEP_DETECTOR,						FALSE,
		0,
		0)
VMM_ENTRY(0x81, CAMSENSORSYNC,						FALSE,
		0,
		0)
/* Here are a few example expansions:


- If the registers are numbered sequentially, we can simply:
#define VMM_ENTRY(reg, id, writable, addr, size) VMM_##id,
- If they're not sequential, or we want to map them to a different base:
#define VMM_BASE 0x0100
#define VMM_ENTRY(reg, id, writable, addr, size) VMM_##id = VMM_BASE + reg,
- Now we expand the table per the definition above:
typedef enum {
	#include "VmmTable.h"
} vmm_ids_t;
#undef VMM_ENTRY


- This expansion creates a table of write sizes:
#define VMM_ENTRY(reg, id, writable, addr, size) (writable ? 1 : 0) * (size),
- Or better yet:
#define VMM_ENTRY(reg, id, writable, addr, size) (writable ? 1 : -1) * (size),
const unsigned short a_vmm_size[VMM_MAP_SIZE] = {
	#include "VmmTable.h"
};
#undef VMM_ENTRY


- This contrived example clears all the registers:
void zero_all_registers(void) {
#define VMM_ENTRY(reg, id, writable, addr, size) \
	if (writable) { \
		printf("Clearing " #id " (" #reg ", %d bytes)\n", size); \
		memset(addr, 0, size); \
	} else { printf("Register " #id " is not writable\n"); }
#include "VmmTable.h"
#undef VMM_ENTRY
}

*/

