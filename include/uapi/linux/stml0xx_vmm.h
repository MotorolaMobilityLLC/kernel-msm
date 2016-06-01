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

/* A write to the first register is valid to unlock test command interface */
VMM_ENTRY(0x00, ID,							TRUE,
		&vmm_id,
		sizeof(vmm_id))
VMM_ENTRY(0x01, REV_ID,							FALSE,
		vmm_rev_id,
		sizeof(vmm_rev_id))
VMM_ENTRY(0x02, ERROR_STATUS,						FALSE,
		vmm_error_status,
		sizeof(vmm_error_status))
VMM_ENTRY(0x03, LOWPOWER_REG,						TRUE,
		vmm_lowpower_reg,
		sizeof(vmm_lowpower_reg))
VMM_ENTRY(0x04, INIT_COMPLETE,						TRUE,
		&vmm_init_complete,
		sizeof(vmm_init_complete))
VMM_ENTRY(0x05, RESET_REASON,						FALSE,
		&vmm_reset_reason,
		sizeof(vmm_reset_reason))
VMM_ENTRY(0x06, ACCEL_ORIENTATION,					TRUE,
		vmm_accel_orientation,
		sizeof(vmm_accel_orientation))
VMM_ENTRY(0x07, ACCEL_SWAP,						TRUE,
		&vmm_accel_swap,
		sizeof(vmm_accel_swap))
VMM_ENTRY(0x08, ACCEL_CAL,						TRUE,
		&vmm_accel_cal,
		sizeof(vmm_accel_cal))
VMM_ENTRY(0x09, UNUSED_09,						FALSE,
		0,
		0)
VMM_ENTRY(0x0A, UNUSED_0A,						FALSE,
		0,
		0)
VMM_ENTRY(0x0B, UNUSED_0B,						FALSE,
		0,
		0)
VMM_ENTRY(0x0C, UNUSED_0C,						FALSE,
		0,
		0)
VMM_ENTRY(0x0D, UNUSED_0D,						FALSE,
		0,
		0)
VMM_ENTRY(0x0E, UNUSED_0E,						FALSE,
		0,
		0)
VMM_ENTRY(0x0F, UNUSED_0F,						FALSE,
		0,
		0)
VMM_ENTRY(0x10, QUEUE_TEST_LIMIT,					TRUE,
		&vmm_queue_test_limit,
		sizeof(vmm_queue_test_limit))
VMM_ENTRY(0x11, LED_NOTIF_DATA,						TRUE,
		vmm_led_notif_data,
		sizeof(vmm_led_notif_data))
VMM_ENTRY(0x12, HS_DETECT_TEST,						FALSE,
		vmm_hs_detect_test,
		sizeof(vmm_hs_detect_test))
VMM_ENTRY(0x13, ACCEL2_UPDATE_RATE,					TRUE,
		&vmm_accel2_update_rate,
		sizeof(vmm_accel2_update_rate))
VMM_ENTRY(0x14, DISP_ROTATE_UPDATE_RATE,				TRUE,
		&vmm_disp_rotate_update_rate,
		sizeof(vmm_disp_rotate_update_rate))
VMM_ENTRY(0x15, STEP_COUNTER_INFO,					TRUE,
		vmm_step_counter_info,
		sizeof(vmm_step_counter_info))
VMM_ENTRY(0x16, ACCEL_UPDATE_RATE,					TRUE,
		&vmm_accel_update_rate,
		sizeof(vmm_accel_update_rate))
VMM_ENTRY(0x17, MAG_UPDATE_RATE,					TRUE,
		&vmm_mag_update_rate,
		sizeof(vmm_mag_update_rate))
VMM_ENTRY(0x18, UNUSED_18,						FALSE,
		0, /* vmm_pressure_update_rate */
		0)
VMM_ENTRY(0x19, GYRO_UPDATE_RATE,					TRUE,
		&vmm_gyro_update_rate,
		sizeof(vmm_gyro_update_rate))
VMM_ENTRY(0x1A, NONWAKESENSOR_CONFIG,					TRUE,
		vmm_module_config,
		sizeof(vmm_module_config))
VMM_ENTRY(0x1B, WAKESENSOR_CONFIG,					TRUE,
		vmm_wakesensor_config,
		sizeof(vmm_wakesensor_config))
VMM_ENTRY(0x1C, UNUSED_1C,						FALSE,
		0, /* vmm_ir_gesture */
		0)
VMM_ENTRY(0x1D, UNUSED_1D,						FALSE,
		0, /* vmm_ir_data */
		0)
VMM_ENTRY(0x1E, UNUSED_1E,						FALSE,
		0, /* vmm_ir_config */
		0)
VMM_ENTRY(0x1F, SENSOR_TEST_CONFIG,					TRUE,
		&vmm_sensor_test_config,
		sizeof(vmm_sensor_test_config))
VMM_ENTRY(0x20, MOTION_DUR,						TRUE,
		&vmm_motion_dur0,
		sizeof(vmm_motion_dur0))
VMM_ENTRY(0x21, MOTION_HISTORY,						FALSE,
		&vmm_motion_history,
		sizeof(vmm_motion_history))
VMM_ENTRY(0x22, ZMOTION_DUR,						TRUE,
		&vmm_zmotion_dur0,
		sizeof(vmm_zmotion_dur0))
VMM_ENTRY(0x23, UNUSED_23,						FALSE,
		0,
		0)
VMM_ENTRY(0x24, UNUSED_24,						FALSE,
		0,
		0)
VMM_ENTRY(0x25, UNUSED_25,						FALSE,
		0,
		0)
VMM_ENTRY(0x26, ALGO_CONFIG,						TRUE,
		vmm_algo_config,
		sizeof(vmm_algo_config))
VMM_ENTRY(0x27, ALGO_INT_STATUS,					TRUE,
		vmm_algo_int_status,
		sizeof(vmm_algo_int_status))
VMM_ENTRY(0x28, GENERIC_INT,						FALSE,
		vmm_generic_int,
		sizeof(vmm_generic_int))
VMM_ENTRY(0x29, GENERIC_LISTENERS,					TRUE,
		vmm_generic_listeners,
		sizeof(vmm_generic_listeners))
VMM_ENTRY(0x2A, UNUSED_2A,						FALSE,
		0,
		0)
VMM_ENTRY(0x2B, HEADSET_HW_VER,						TRUE,
		&vmm_headset_hw_ver,
		sizeof(vmm_headset_hw_ver))
VMM_ENTRY(0x2C, HEADSET_CONTROL,					TRUE,
		&vmm_headset_control,
		sizeof(vmm_headset_control))
VMM_ENTRY(0x2D, MOTION_DATA,						FALSE,
		vmm_motion_data,
		sizeof(vmm_motion_data))
VMM_ENTRY(0x2E, HEADSET_SETTINGS,					TRUE,
		vmm_headset_settings,
		sizeof(vmm_headset_settings))
VMM_ENTRY(0x2F, HEADSET_STATE,						FALSE,
		&vmm_headset_state,
		sizeof(vmm_headset_state))
VMM_ENTRY(0x30, SWI_COUNTER_SIZE,					FALSE,
		&vmm_swi_counter_size,
		sizeof(vmm_swi_counter_size))
VMM_ENTRY(0x31, SWI_COUNTER,						FALSE,
		vmm_swi_counter,
		sizeof(vmm_swi_counter))
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
VMM_ENTRY(0x36, FW_CRC,							FALSE,
		&vmm_fw_crc,
		sizeof(vmm_fw_crc))
VMM_ENTRY(0x37, FW_VERSION_LEN,						FALSE,
		&vmm_fw_version_len,
		sizeof(vmm_fw_version_len))
VMM_ENTRY(0x38, FW_VERSION_STR,						FALSE,
		fw_version_str,
		0)
VMM_ENTRY(0x39, WAKESENSOR_STATUS,					FALSE,
		vmm_wakesensor_status,
		sizeof(vmm_wakesensor_status))
VMM_ENTRY(0x3A, INTERRUPT_STATUS,					FALSE,
		vmm_interrupt_status,
		sizeof(vmm_interrupt_status))
VMM_ENTRY(0x3B, ACCEL_DATA,						FALSE,
		vmm_accel_data,
		sizeof(vmm_accel_data))
VMM_ENTRY(0x3C, I2C_PASSTHROUGH_COMMAND,				TRUE,
		vmm_i2c_passthrough_command,
		sizeof(vmm_i2c_passthrough_command))
VMM_ENTRY(0x3D, I2C_PASSTHROUGH_RESPONSE,				FALSE,
		vmm_i2c_passthrough_response,
		sizeof(vmm_i2c_passthrough_response))
VMM_ENTRY(0x3E, ACCEL2_DATA,						FALSE,
		vmm_accel2_data,
		sizeof(vmm_accel2_data))
VMM_ENTRY(0x3F, DOCKED_DATA,						FALSE,
		&vmm_docked_data,
		sizeof(vmm_docked_data))
VMM_ENTRY(0x40, COVER_DATA,						FALSE,
		&vmm_cover_data,
		sizeof(vmm_cover_data))
#ifdef GYRO_CALIBRATION
VMM_ENTRY(0x41, GYRO_CAL,						TRUE,
		vmm_gyro_cal,
		GYRO_CAL_ARRAY_FIRST * sizeof(cal_value_t))
VMM_ENTRY(0x42, GYRO_CAL_2,						TRUE,
		&vmm_gyro_cal[GYRO_CAL_ARRAY_FIRST],
		GYRO_CAL_ARRAY_SECOND * sizeof(cal_value_t))
#else
		VMM_ENTRY(0x41, UNUSED_41,				FALSE,
				0,
				0)
		VMM_ENTRY(0x42, UNUSED_42,				FALSE,
				0,
				0)
#endif
VMM_ENTRY(0x43, GYRO_DATA,						FALSE,
		vmm_gyro_data,
		sizeof(vmm_gyro_data))
VMM_ENTRY(0x44, UNUSED_44,						FALSE,
		0, /* vmm_quaterion */
		0)
VMM_ENTRY(0x45, UNCALIB_GYRO_DATA,					FALSE,
		vmm_uncalib_gyro_data,
		sizeof(vmm_uncalib_gyro_data))
VMM_ENTRY(0x46, UNCALIB_MAG_DATA,					FALSE,
		vmm_uncalib_mag_data,
		sizeof(vmm_uncalib_mag_data))
VMM_ENTRY(0x47, UNUSED_47,						FALSE,
		0,
		0)
VMM_ENTRY(0x48, UNUSED_48,						FALSE,
		0, /* vmm_mag_calibration */
		0)
VMM_ENTRY(0x49, UNUSED_49,						FALSE,
		0, /* vmm_mag_data */
		0)
VMM_ENTRY(0x4A, DROTATE,						FALSE,
		&vmm_drotate,
		sizeof(vmm_drotate))
VMM_ENTRY(0x4B, FLAT_DATA,						FALSE,
		&vmm_flat_data,
		sizeof(vmm_flat_data))
VMM_ENTRY(0x4C, CAMERA_GESTURE,						FALSE,
		vmm_camera_gesture,
		sizeof(vmm_camera_gesture))
VMM_ENTRY(0x4D, CAMERA_CONFIG,						TRUE,
		vmm_camera_config,
		sizeof(vmm_camera_config))
VMM_ENTRY(0x4E, SIM_DATA,						FALSE,
		vmm_sim_data,
		sizeof(vmm_sim_data))
VMM_ENTRY(0x4F, CHOPCHOP_DATA,						FALSE,
		vmm_chopchop_data,
		sizeof(vmm_chopchop_data))
VMM_ENTRY(0x50, CHOPCHOP_CONFIG,					TRUE,
		vmm_chopchop_config,
		sizeof(vmm_chopchop_config))
VMM_ENTRY(0x51, LIFT_DATA,						FALSE,
		vmm_lift_data,
		sizeof(vmm_lift_data))
VMM_ENTRY(0x52, LIFT_CONFIG,						TRUE,
		vmm_lift_config,
		sizeof(vmm_lift_config))
VMM_ENTRY(0x53, UNUSED_53,						FALSE,
		0,
		0)
VMM_ENTRY(0x54, LOG_TAG_ENABLED,					TRUE,
		&vmm_log_tag_enabled,
		sizeof(vmm_log_tag_enabled))
VMM_ENTRY(0x55, SH_LOG_LEVEL,						TRUE,
		&vmm_log_level,
		sizeof(vmm_log_level))
VMM_ENTRY(0x56, UNUSED_56,						FALSE,
		0, /* &s_sensor_data */
		0)
VMM_ENTRY(0x57, UNUSED_57,						FALSE,
		0,
		0)

#ifdef DSP
	VMM_ENTRY(0x58, DSP_CONTROL,					TRUE,
			&vmm_dsp_control,
			sizeof(vmm_dsp_control))
	VMM_ENTRY(0x59, DSP_ALGO_CONFIG_A,				TRUE,
			vmm_dsp_algo_config_a,
			DSP_ALGO_CONFIG_LEN)
	VMM_ENTRY(0x5A, DSP_ALGO_STATUS_A,				FALSE,
			vmm_dsp_algo_status_a,
			sizeof(vmm_dsp_algo_status_a))
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
#endif

VMM_ENTRY(0x5B, UNUSED_5B,						FALSE,
		0,
		0)
VMM_ENTRY(0x5C, GLANCE_REG,						FALSE,
		vmm_glance_data,
		sizeof(vmm_glance_data))
VMM_ENTRY(0x5D, ALGO_ACCUM_ALL_MODALITY,				FALSE,
		vmm_algo_accum_all_modality,
		sizeof(vmm_algo_accum_all_modality))
VMM_ENTRY(0x5E, UNUSED_5E,						FALSE,
		0,
		0)
VMM_ENTRY(0x5F, UNUSED_5F,						FALSE,
		0,
		0)
VMM_ENTRY(0x60, ALGO_REQ_ACCUM_MODALITY,				TRUE,
		vmm_algo_req_accum_modality,
		sizeof(vmm_algo_req_accum_modality))
VMM_ENTRY(0x61, UNUSED_61,						FALSE,
		0,
		0)
VMM_ENTRY(0x62, UNUSED_62,						FALSE,
		0,
		0)
VMM_ENTRY(0x63, ALGO_EVT_ACCUM_MODALITY,				FALSE,
		vmm_algo_evt_accum_modality,
		sizeof(vmm_algo_evt_accum_modality))
VMM_ENTRY(0x64, UNUSED_64,						FALSE,
		0,
		0)
VMM_ENTRY(0x65, UNUSED_65,						FALSE,
		0,
		0)
VMM_ENTRY(0x66, UNUSED_66,						FALSE,
		0, /* vmm_pressure_data */
		0)
VMM_ENTRY(0x67, PROX_TEST,						FALSE,
		vmm_prox_test,
		sizeof(vmm_prox_test))
VMM_ENTRY(0x68, ALS_TEST,						FALSE,
		vmm_als_test,
		sizeof(vmm_als_test))
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
		&vmm_stowed_event,
		sizeof(vmm_stowed_event))
VMM_ENTRY(0x6E, UNUSED_6E,						FALSE,
		0,
		0)
VMM_ENTRY(0x6F, ALS_UPDATE_RATE,					TRUE,
		vmm_als_update_rate,
		sizeof(vmm_als_update_rate))
VMM_ENTRY(0x70, CHIP_ID,						FALSE,
		vmm_chip_id,
		sizeof(vmm_chip_id))
VMM_ENTRY(0x71, UNUSED_71,						FALSE,
		0, /* &mm_last_message1 */
		0)
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
VMM_ENTRY(0x7B, PORT_H,							TRUE,
		0x50001C00,
		0x25)
VMM_ENTRY(0x7C, PORT_D,							TRUE,
		0x50000C00,
		0x25)
VMM_ENTRY(0x7D, PORT_C,							TRUE,
		0x50000800,
		0x25)
VMM_ENTRY(0x7E, PORT_B,							TRUE,
		0x50000400,
		0x25)
VMM_ENTRY(0x7F, PORT_A,							TRUE,
		0x50000000,
		0x25)


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

