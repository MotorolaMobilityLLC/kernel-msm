/* Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "msm_actuator.h"

static int32_t msm_mot_actuator_init_step_table(
	struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_set_info_t *set_info);

static void msm_mot_actuator_write_focus(
	struct msm_actuator_ctrl_t *a_ctrl,
	uint16_t curr_lens_pos,
	struct damping_params_t *damping_params,
	int8_t sign_direction,
	int16_t code_boundary);

static int32_t msm_actuator_set_default_focus(
	struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_move_params_t *move_params);

static int32_t msm_actuator_init_focus(struct msm_actuator_ctrl_t *a_ctrl,
	uint16_t size, struct reg_settings_t *settings);

static int32_t msm_actuator_park_lens(struct msm_actuator_ctrl_t *a_ctrl);

static int32_t msm_mot_actuator_write_reg_settings(
	struct msm_actuator_ctrl_t *a_ctrl)
{
	int32_t rc = 0;
	int32_t i = 0;

	for (i = 0; i < a_ctrl->mot_i2c_tbl_index; i++) {
		switch (a_ctrl->mot_i2c_reg_tbl[i].addr_type) {
		case MSM_ACTUATOR_BYTE_ADDR:
			a_ctrl->i2c_client.addr_type =
				MSM_CAMERA_I2C_BYTE_ADDR;
			break;
		case MSM_ACTUATOR_WORD_ADDR:
			a_ctrl->i2c_client.addr_type =
				MSM_CAMERA_I2C_WORD_ADDR;
			break;
		default:
			pr_err("Unsupported addr type: %d\n",
					a_ctrl->mot_i2c_reg_tbl[i].addr_type);
			break;
		}

		switch (a_ctrl->mot_i2c_reg_tbl[i].i2c_operation) {
		case MSM_ACT_WRITE:
			rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write(
					&a_ctrl->i2c_client,
					a_ctrl->mot_i2c_reg_tbl[i].reg_addr,
					a_ctrl->mot_i2c_reg_tbl[i].reg_data,
					a_ctrl->mot_i2c_reg_tbl[i].data_type);
			break;
		case MSM_ACT_POLL:
			rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_poll(
					&a_ctrl->i2c_client,
					a_ctrl->mot_i2c_reg_tbl[i].reg_addr,
					a_ctrl->mot_i2c_reg_tbl[i].reg_data,
					a_ctrl->mot_i2c_reg_tbl[i].data_type);
			break;
		default:
			pr_err("Unsupported i2c_operation: %d\n",
					a_ctrl->mot_i2c_reg_tbl[i].
					i2c_operation);
			break;

		if (0 != a_ctrl->mot_i2c_reg_tbl[i].delay)
			msleep(a_ctrl->mot_i2c_reg_tbl[i].delay);
		if (rc < 0)
			break;
		}
	}

	return rc;
}

static void msm_mot_actuator_write_focus(
	struct msm_actuator_ctrl_t *a_ctrl,
	uint16_t curr_lens_pos,
	struct damping_params_t *damping_params,
	int8_t sign_direction,
	int16_t code_boundary)
{
	uint16_t wait_time = 0;
	CDBG("Enter %s\n", __func__);

	wait_time = damping_params->damping_delay;

	if (curr_lens_pos != code_boundary) {
		a_ctrl->func_tbl->actuator_parse_i2c_params(a_ctrl,
			code_boundary, damping_params->hw_params, wait_time);
	}
	CDBG("Exit %s\n", __func__);
}

static void msm_mot_actuator_parse_i2c_params(
	struct msm_actuator_ctrl_t *a_ctrl, int16_t next_lens_position,
	uint32_t hw_params, uint16_t delay)
{
	struct msm_actuator_reg_params_t *write_arr = a_ctrl->reg_tbl;
	uint32_t hw_dword = hw_params;
	uint16_t i2c_byte1 = 0, i2c_byte2 = 0;
	uint16_t value = 0;
	static int16_t last_lens_position = 1;
	uint32_t size = a_ctrl->reg_tbl_size, i = 0;
	struct reg_settings_t *i2c_tbl = a_ctrl->mot_i2c_reg_tbl;
	enum msm_actuator_data_type data_type = MSM_ACTUATOR_WORD_DATA;

	CDBG("%s: Enter\n", __func__);

	for (i = 0; i < size; i++) {
		/* check that the index into i2c_tbl cannot grow larger that
		   the allocated size of i2c_tbl */
		if ((a_ctrl->total_steps + 1) < (a_ctrl->mot_i2c_tbl_index) ||
				a_ctrl->mot_i2c_tbl_index >=
				MAX_ACTUATOR_REG_TBL_SIZE) {
			break;
		}

		if (write_arr[i].reg_write_type == MSM_ACTUATOR_WRITE_DAC) {
			value = ((uint16_t)next_lens_position <<
					write_arr[i].data_shift) |
				((hw_dword & write_arr[i].hw_mask) >>
				 write_arr[i].hw_shift);

			CDBG("DAC: 0x%x, lens pos: %d\n",
					value, next_lens_position);
			if (write_arr[i].reg_addr != 0xFFFF) {
				i2c_byte1 = write_arr[i].reg_addr;
				i2c_byte2 = value;
				data_type = MSM_ACTUATOR_WORD_DATA;
			} else {
				i2c_byte1 = (value & 0xFF00) >> 8;
				i2c_byte2 = value & 0xFF;
			}
		} else if (write_arr[i].reg_write_type ==
				MSM_ACTUATOR_WRITE_REG) {
			i2c_byte1 = write_arr[i].reg_addr;

			if (i2c_byte1 == 0x16) {
				if (next_lens_position > last_lens_position)
					i2c_byte2 = 0x0180; /* toward macro */
				else
					i2c_byte2 = 0xFE80; /* toward inf */
			} else {
				i2c_byte2 = write_arr[i].hw_shift;
			}

			if (write_arr[i].data_shift == 1)
				data_type = MSM_ACTUATOR_BYTE_DATA;
			else
				data_type = MSM_ACTUATOR_WORD_DATA;
		} else {
			i2c_byte1 = write_arr[i].reg_addr;
			i2c_byte2 = (hw_dword & write_arr[i].hw_mask) >>
				write_arr[i].hw_shift;
		}

		i2c_tbl[a_ctrl->mot_i2c_tbl_index].addr_type =
			MSM_ACTUATOR_BYTE_ADDR;
		i2c_tbl[a_ctrl->mot_i2c_tbl_index].data_type = data_type;
		i2c_tbl[a_ctrl->mot_i2c_tbl_index].i2c_operation =
			MSM_ACT_WRITE;
		i2c_tbl[a_ctrl->mot_i2c_tbl_index].reg_addr = i2c_byte1;
		i2c_tbl[a_ctrl->mot_i2c_tbl_index].reg_data = i2c_byte2;
		i2c_tbl[a_ctrl->mot_i2c_tbl_index].delay = delay;
		a_ctrl->mot_i2c_tbl_index++;
	}

	last_lens_position = next_lens_position;
	CDBG("%s: Exit\n", __func__);
}

static int32_t msm_mot_actuator_move_focus(
	struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_move_params_t *move_params)
{
	int32_t rc = 0;
	struct damping_params_t ringing_params_kernel;
	int8_t sign_dir = move_params->sign_dir;
	uint16_t step_boundary = 0;
	uint16_t target_step_pos = 0;
	int16_t target_lens_pos = 0;
	int16_t dest_step_pos = move_params->dest_step_pos;
	int16_t curr_lens_pos = 0;
	int dir = move_params->dir;
	int32_t num_steps = move_params->num_steps;
	CDBG("Enter %s\n", __func__);

	if (copy_from_user(&ringing_params_kernel,
		&(move_params->ringing_params[a_ctrl->curr_region_index]),
		sizeof(struct damping_params_t))) {
		pr_err("%s: copy_from_user failed\n", __func__);
		return -EFAULT;
	}

	CDBG("called, dir %d, num_steps %d\n", dir, num_steps);
	if (dest_step_pos == a_ctrl->curr_step_pos)
		return rc;

	if ((sign_dir > MSM_ACTUATOR_MOVE_SIGNED_NEAR) ||
		(sign_dir < MSM_ACTUATOR_MOVE_SIGNED_FAR)) {
		pr_err("Invalid sign_dir = %d\n", sign_dir);
		return -EFAULT;
	}
	if ((dir > MOVE_FAR) || (dir < MOVE_NEAR)) {
		pr_err("Invalid direction = %d\n", dir);
		return -EFAULT;
	}
	if (dest_step_pos >= a_ctrl->total_steps) {
		pr_err("Step pos greater than total steps = %d\n",
		dest_step_pos);
		return -EFAULT;
	}
	curr_lens_pos = a_ctrl->step_position_table[a_ctrl->curr_step_pos];
	a_ctrl->mot_i2c_tbl_index = 0;
	CDBG("curr_step_pos =%d dest_step_pos =%d curr_lens_pos=%d\n",
		a_ctrl->curr_step_pos, dest_step_pos, curr_lens_pos);

	while (a_ctrl->curr_step_pos != dest_step_pos) {
		step_boundary =
			a_ctrl->region_params[a_ctrl->curr_region_index].
			step_bound[dir];
		if ((dest_step_pos * sign_dir) <=
			(step_boundary * sign_dir)) {

			target_step_pos = dest_step_pos;
			target_lens_pos =
				a_ctrl->step_position_table[target_step_pos];
			a_ctrl->func_tbl->actuator_write_focus(a_ctrl,
					curr_lens_pos,
					&ringing_params_kernel,
					sign_dir,
					target_lens_pos);
			curr_lens_pos = target_lens_pos;

		} else {
			target_step_pos = step_boundary;
			target_lens_pos =
				a_ctrl->step_position_table[target_step_pos];
			a_ctrl->func_tbl->actuator_write_focus(a_ctrl,
					curr_lens_pos,
					&ringing_params_kernel,
					sign_dir,
					target_lens_pos);
			curr_lens_pos = target_lens_pos;

			a_ctrl->curr_region_index += sign_dir;
		}
		a_ctrl->curr_step_pos = target_step_pos;

		rc = msm_mot_actuator_write_reg_settings(a_ctrl);
		if (rc < 0) {
			pr_err("i2c write error:%d\n", rc);
			return rc;
		}
		a_ctrl->mot_i2c_tbl_index = 0;
	}

	move_params->curr_lens_pos = curr_lens_pos;

	CDBG("Exit\n");
	return rc;
}

static int32_t msm_mot_actuator_set_position(
	struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_set_position_t *set_pos)
{
	int32_t rc = 0;
	int32_t index;
	uint16_t next_lens_position;
	uint16_t delay;
	uint32_t hw_params = 0;
	CDBG("%s Enter %d\n", __func__, __LINE__);
	if (set_pos->number_of_steps  == 0)
		return rc;

	a_ctrl->mot_i2c_tbl_index = 0;
	for (index = 0; index < set_pos->number_of_steps; index++) {
		next_lens_position = set_pos->pos[index];
		delay = set_pos->delay[index];
		a_ctrl->func_tbl->actuator_parse_i2c_params(a_ctrl,
		next_lens_position, hw_params, delay);

		rc = msm_mot_actuator_write_reg_settings(a_ctrl);
		if (rc < 0) {
			pr_err("%s Failed I2C write Line %d\n",
				__func__, __LINE__);
			return rc;
		}
		a_ctrl->mot_i2c_tbl_index = 0;
	}
	CDBG("%s exit %d\n", __func__, __LINE__);
	return rc;
}

static int32_t msm_mot_actuator_init_step_table(
	struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_set_info_t *set_info)
{
	int macro_dac = 0;
	int inf_dac = 0;
	int cur_code = 0;
	int16_t step_index = 0, region_index = 0;
	uint16_t step_boundary = 0;
	int32_t max_code_size = 1;
	uint16_t data_size = set_info->actuator_params.data_size;
	CDBG("Enter %s\n", __func__);

	for (; data_size > 0; data_size--)
		max_code_size *= 2;

	a_ctrl->max_code_size = max_code_size;
	kfree(a_ctrl->step_position_table);
	a_ctrl->step_position_table = NULL;

	if (set_info->af_tuning_params.total_steps
		>  MAX_ACTUATOR_AF_TOTAL_STEPS) {
		pr_err("Max actuator totalsteps exceeded = %d\n",
		set_info->af_tuning_params.total_steps);
		return -EFAULT;
	}

	/* Fill step position table */
	a_ctrl->step_position_table =
		kmalloc(sizeof(uint16_t) *
		(set_info->af_tuning_params.total_steps), GFP_KERNEL);

	if (a_ctrl->step_position_table == NULL)
		return -ENOMEM;

	inf_dac = set_info->mot_af_tuning_params.infinity_dac;
	macro_dac = set_info->mot_af_tuning_params.macro_dac;

	for (region_index = 0;
		region_index < a_ctrl->region_size;
		region_index++) {
		step_boundary =
			a_ctrl->region_params[region_index].
			step_bound[MOVE_NEAR];
		for (step_index = 0; step_index < step_boundary;
			step_index++) {
			cur_code =
				((int)step_index * (macro_dac - inf_dac)) /
				(set_info->af_tuning_params.total_steps - 1) +
				inf_dac;
			if (step_index <
				set_info->af_tuning_params.total_steps) {
				if (cur_code > max_code_size) {
					a_ctrl->step_position_table[step_index]
						= (uint16_t)max_code_size;
				} else {
					a_ctrl->step_position_table[step_index]
						= (uint16_t)cur_code;
				}
			}
		}
	}

	CDBG("Exit %s\n", __func__);
	return 0;
}

static struct msm_actuator msm_mot_hvcm_actuator_table = {
	.act_type = ACTUATOR_MOT_HVCM,
	.func_tbl = {
		.actuator_init_step_table = msm_mot_actuator_init_step_table,
		.actuator_move_focus = msm_mot_actuator_move_focus,
		.actuator_write_focus = msm_mot_actuator_write_focus,
		.actuator_set_default_focus = msm_actuator_set_default_focus,
		.actuator_init_focus = msm_actuator_init_focus,
		.actuator_parse_i2c_params = msm_mot_actuator_parse_i2c_params,
		.actuator_set_position = msm_mot_actuator_set_position,
		.actuator_park_lens = msm_actuator_park_lens,
	},
};

