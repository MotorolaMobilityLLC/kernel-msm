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

static int msm_actuator_bivcm_handle_i2c_ops(
	struct msm_actuator_ctrl_t *a_ctrl,
	int16_t next_lens_position, uint32_t hw_params, uint16_t delay);

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
		msm_actuator_bivcm_handle_i2c_ops(a_ctrl,
			code_boundary, damping_params->hw_params, wait_time);
	}
	CDBG("Exit %s\n", __func__);
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
	a_ctrl->i2c_tbl_index = 0;
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
		a_ctrl->i2c_tbl_index = 0;
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

	a_ctrl->i2c_tbl_index = 0;
	for (index = 0; index < set_pos->number_of_steps; index++) {
		next_lens_position = set_pos->pos[index];
		delay = set_pos->delay[index];
		msm_actuator_bivcm_handle_i2c_ops(a_ctrl,
		next_lens_position, hw_params, delay);

		a_ctrl->i2c_tbl_index = 0;
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
		.actuator_parse_i2c_params = NULL,
		.actuator_set_position = msm_mot_actuator_set_position,
		.actuator_park_lens = NULL,
	},
};

