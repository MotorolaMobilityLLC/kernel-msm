/* FPC1020 Touch sensor driver
 *
 * Copyright (c) 2013,2014 Fingerprint Cards AB <tech@fingerprints.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version 2
 * as published by the Free Software Foundation.
 */

#define DEBUG

#include <linux/input.h>
#include <linux/delay.h>

#include "fpc1020_common.h"
#include "fpc1020_capture.h"

int fpc1020_init_capture(fpc1020_data_t * fpc1020)
{
	fpc1020->capture.state = FPC1020_CAPTURE_STATE_IDLE;
	fpc1020->capture.current_mode = FPC1020_MODE_IDLE;
	fpc1020->capture.available_bytes = 0;

	init_waitqueue_head(&fpc1020->capture.wq_data_avail);

	return 0;
}

/* -------------------------------------------------------------------- */
int fpc1020_write_capture_setup(fpc1020_data_t * fpc1020)
{
	return fpc1020_write_sensor_setup(fpc1020);
}

/* -------------------------------------------------------------------- */
int fpc1020_write_test_setup(fpc1020_data_t * fpc1020, u16 pattern)
{
	int error = 0;

	dev_dbg(&fpc1020->spi->dev, "%s, pattern 0x%x\n", __func__, pattern);
	switch (fpc1020->chip.type) {
	case FPC1020_CHIP_1020A:
		error =
		    fpc1020_write_sensor_checkerboard_setup_1020(fpc1020,
								 pattern);
		break;
	case FPC1020_CHIP_1021A:
	case FPC1020_CHIP_1021B:
		error =
		    fpc1020_write_sensor_checkerboard_setup_1021(fpc1020,
								 pattern);
		break;
	case FPC1020_CHIP_1150A:
		error =
		    fpc1020_write_sensor_checkerboard_setup_1020(fpc1020,
								 pattern);
		break;
	case FPC1020_CHIP_1140B:
		if (0xaa55u == pattern) {
			error =
			    fpc1020_write_cb_test_setup_1140(fpc1020, false);
		} else if (0x55aau == pattern) {
			error = fpc1020_write_cb_test_setup_1140(fpc1020, true);
		}
		break;
	default:
		error = -EINVAL;
		break;
	}

	return error;
}

/* -------------------------------------------------------------------- */
bool fpc1020_capture_check_ready(fpc1020_data_t * fpc1020)
{
	fpc1020_capture_state_t state = fpc1020->capture.state;

	return (state == FPC1020_CAPTURE_STATE_IDLE) ||
	    (state == FPC1020_CAPTURE_STATE_COMPLETED) ||
	    (state == FPC1020_CAPTURE_STATE_FAILED);
}

/* -------------------------------------------------------------------- */
int fpc1020_capture_task(fpc1020_data_t * fpc1020)
{
	int error = 0;
	bool wait_finger_down = false;
	bool wait_finger_up = false;
	bool adjust_settings;
	fpc1020_capture_mode_t mode = fpc1020->capture.current_mode;
	int current_capture, capture_count;
	int image_offset;

	u32 image_byte_size;

	fpc1020->capture.state = FPC1020_CAPTURE_STATE_WRITE_SETTINGS;

	error = fpc1020_wake_up(fpc1020);
	if (error < 0)
		goto out_error;
	switch (mode) {
	case FPC1020_MODE_WAIT_AND_CAPTURE:
		wait_finger_down = wait_finger_up = true;

	case FPC1020_MODE_SINGLE_CAPTURE:
	case FPC1020_MODE_WAIT_FINGER_UP_AND_CAPTURE:
		capture_count = fpc1020->setup.capture_count;
		adjust_settings = true;
		error = fpc1020_write_capture_setup(fpc1020);
		break;

	case FPC1020_MODE_CHECKERBOARD_TEST_NORM:
		capture_count = 1;
		adjust_settings = false;
		error = fpc1020_write_test_setup(fpc1020, 0xaa55);
		break;

	case FPC1020_MODE_CHECKERBOARD_TEST_INV:
		capture_count = 1;
		adjust_settings = false;
		error = fpc1020_write_test_setup(fpc1020, 0x55aa);
		break;

	case FPC1020_MODE_BOARD_TEST_ONE:
		capture_count = 1;
		adjust_settings = false;
		error = fpc1020_write_test_setup(fpc1020, 0xffff);
		break;

	case FPC1020_MODE_BOARD_TEST_ZERO:
		capture_count = 1;
		adjust_settings = false;
		error = fpc1020_write_test_setup(fpc1020, 0x0000);
		break;

	case FPC1020_MODE_WAIT_FINGER_DOWN:
		wait_finger_down = true;
		capture_count = 0;
		adjust_settings = false;
		error = fpc1020_write_capture_setup(fpc1020);
		break;

	case FPC1020_MODE_WAIT_FINGER_UP:
		wait_finger_up = true;
		capture_count = 0;
		adjust_settings = false;
		error = fpc1020_write_capture_setup(fpc1020);
		break;

	case FPC1020_MODE_IDLE:
	default:
		capture_count = 0;
		adjust_settings = false;
		error = -EINVAL;
		break;
	}

	if (error < 0)
		goto out_error;

	if (mode == FPC1020_MODE_CHECKERBOARD_TEST_NORM ||
	    mode == FPC1020_MODE_CHECKERBOARD_TEST_INV ||
	    mode == FPC1020_MODE_BOARD_TEST_ONE ||
	    mode == FPC1020_MODE_BOARD_TEST_ZERO) {
		error = fpc1020_capture_set_crop_selftest(fpc1020);
		image_byte_size = fpc1020_calc_image_size_selftest(fpc1020);
	} else {
		error = fpc1020_capture_set_crop(fpc1020,
						 fpc1020->
						 setup.capture_col_start,
						 fpc1020->
						 setup.capture_col_groups,
						 fpc1020->
						 setup.capture_row_start,
						 fpc1020->
						 setup.capture_row_count);
		image_byte_size = fpc1020_calc_image_size(fpc1020);
	}
	if (error < 0)
		goto out_error;

	dev_dbg(&fpc1020->spi->dev,
		"Start capture, mode %d, (%d frames)\n", mode, capture_count);

	if (wait_finger_down) {
		fpc1020->capture.state =
		    FPC1020_CAPTURE_STATE_WAIT_FOR_FINGER_DOWN;

		error = fpc1020_capture_wait_finger_down(fpc1020);

		if (error < 0)
			goto out_error;

		dev_dbg(&fpc1020->spi->dev, "Finger down\n");

		if (mode == FPC1020_MODE_WAIT_FINGER_DOWN) {

			fpc1020->capture.available_bytes = 4;
			fpc1020->huge_buffer[0] = 'F';
			fpc1020->huge_buffer[1] = ':';
			fpc1020->huge_buffer[2] = 'D';
			fpc1020->huge_buffer[3] = 'N';
		}
	}

	current_capture = 0;
	image_offset = 0;
	if (mode != FPC1020_MODE_WAIT_FINGER_DOWN
	    && mode != FPC1020_MODE_WAIT_FINGER_UP) {
		while (capture_count && (error >= 0)) {
			fpc1020->capture.state = FPC1020_CAPTURE_STATE_ACQUIRE;

			dev_dbg(&fpc1020->spi->dev, "Capture, frame %d \n",
				current_capture + 1);

			error = (!adjust_settings) ? 0 :
			    fpc1020_capture_settings(fpc1020, current_capture);

			if (error < 0)
				goto out_error;

			error = fpc1020_cmd(fpc1020,
					    FPC1020_CMD_CAPTURE_IMAGE,
					    FPC_1020_IRQ_REG_BIT_FIFO_NEW_DATA);

			if (error < 0)
				goto out_error;

			fpc1020->capture.state = FPC1020_CAPTURE_STATE_FETCH;

			error = fpc1020_fetch_image(fpc1020,
						    fpc1020->huge_buffer,
						    image_offset,
						    image_byte_size,
						    (u32)
						    fpc1020->huge_buffer_size);
			if (error < 0)
				goto out_error;

			fpc1020->capture.available_bytes +=
			    (int)image_byte_size;
			fpc1020->capture.last_error = error;

			capture_count--;
			current_capture++;
			image_offset += (int)image_byte_size;
		}
	}
	if (mode != FPC1020_MODE_WAIT_FINGER_UP)
		wake_up_interruptible(&fpc1020->capture.wq_data_avail);

	if (wait_finger_up) {
		fpc1020->capture.state =
		    FPC1020_CAPTURE_STATE_WAIT_FOR_FINGER_UP;

		error = fpc1020_capture_wait_finger_up(fpc1020);
		if (error < 0)
			goto out_error;

		if (mode == FPC1020_MODE_WAIT_FINGER_UP) {
			fpc1020->capture.available_bytes = 4;
			fpc1020->huge_buffer[0] = 'F';
			fpc1020->huge_buffer[1] = ':';
			fpc1020->huge_buffer[2] = 'U';
			fpc1020->huge_buffer[3] = 'P';

			wake_up_interruptible(&fpc1020->capture.wq_data_avail);
		}

		dev_dbg(&fpc1020->spi->dev, "Finger up\n");
	}

out_error:
	fpc1020->capture.last_error = error;

	if (error < 0) {
		fpc1020->capture.state = FPC1020_CAPTURE_STATE_FAILED;
		dev_err(&fpc1020->spi->dev, "%s FAILED %d\n", __func__, error);
	} else {
		fpc1020->capture.state = FPC1020_CAPTURE_STATE_COMPLETED;
		dev_err(&fpc1020->spi->dev, "%s OK\n", __func__);
	}
	return error;
}

/* -------------------------------------------------------------------- */
int cal_thredhold(fpc1020_data_t * fpc1020)
{
	int error = 0;
	int i = 0;
	int sum = 0;
	error =
	    fpc1020_cmd(fpc1020, FPC1020_CMD_CAPTURE_IMAGE,
			FPC_1020_IRQ_REG_BIT_FIFO_NEW_DATA);
	if (error < 0)
		goto out_error;
	fpc1020->capture.state = FPC1020_CAPTURE_STATE_FETCH;
	memset(fpc1020->huge_buffer, 0, 64);
	error = fpc1020_fetch_image(fpc1020,
				    fpc1020->huge_buffer,
				    0,
				    (size_t) 64,
				    (size_t) fpc1020->huge_buffer_size);
	if (error < 0)
		goto out_error;
	for (i = 0; i < 64; i++) {
		sum += ((255 - *(fpc1020->huge_buffer + i)) >> 4);
	}
	sum = sum >> 1;
	printk("fingerprint sum =%d\n", sum);
out_error:
	return (error >= 0) ? sum : error;
}

int fpc1020_capture_wait_finger_down(fpc1020_data_t * fpc1020)
{
	int error = 0;
	int thold = 32;
	int addthred = 30;

	if (fpc1020->setup.finger_auto_threshold) {
		error = fpc1020_read_irq(fpc1020, true);
		if (error < 0)
			goto out_error;
		error = fpc1020_cmd(fpc1020,
				    FPC1020_CMD_FINGER_PRESENT_QUERY,
				    FPC_1020_IRQ_REG_BIT_COMMAND_DONE);
		if (error < 0)
			goto out_error;
		error =
		    fpc1020_capture_set_crop(fpc1020,
					     fpc1020->
					     setup.wakeup_detect_cols[0], 1,
					     fpc1020->
					     setup.wakeup_detect_rows[0], 8);
		if (error < 0)
			goto out_error;
		error = cal_thredhold(fpc1020);
		if (error < 0)
			goto out_error;
		thold = error;
		error =
		    fpc1020_capture_set_crop(fpc1020,
					     fpc1020->
					     setup.wakeup_detect_cols[1], 1,
					     fpc1020->
					     setup.wakeup_detect_rows[1], 8);
		if (error < 0)
			goto out_error;
		error = cal_thredhold(fpc1020);
		if (error < 0)
			goto out_error;
		if (error > thold)
			thold = error;
		thold += addthred;
		if (thold < fpc1020->fp_thredhold && thold >= addthred)
			fpc1020->fp_thredhold = thold;
		printk
		    ("fingerprint fp_thredhold =%d default_hold=%d det1=[%d,1,%d,8] det2=[%d,1,%d,8]\n",
		     fpc1020->fp_thredhold,
		     fpc1020->setup.finger_detect_threshold,
		     fpc1020->setup.wakeup_detect_cols[0],
		     fpc1020->setup.wakeup_detect_rows[0],
		     fpc1020->setup.wakeup_detect_cols[1],
		     fpc1020->setup.wakeup_detect_rows[1]);
		fpc1020_wake_up(fpc1020);
		fpc1020_write_sensor_setup(fpc1020);
	}

	error = fpc1020_wait_finger_present(fpc1020);

	fpc1020_read_irq(fpc1020, true);

out_error:
	return (error >= 0) ? 0 : error;
}

/* -------------------------------------------------------------------- */
int fpc1020_capture_wait_finger_up(fpc1020_data_t * fpc1020)
{
	int error = 0;
	bool finger_up = false;
	int i = 0;
	wake_lock(&fpc1020->fp_wake_lock);
	while (!finger_up && (error >= 0)) {

		// todo: awaiting SW-152

		if (fpc1020->worker.stop_request) {
			error = -EINTR;
			break;
		} else
			error = fpc1020_check_finger_present_sum(fpc1020);

		i++;
		if (i > 100) {
			i = 0;
			printk("%s:finger present sum = %d \n", __func__,
			       error);
		}

		if ((error >= 0) && (error <= FPC1020_FINGER_UP_THRESHOLD))
			finger_up = true;
		else
			msleep(FPC1020_CAPTURE_WAIT_FINGER_DELAY_MS);
	}

	fpc1020_read_irq(fpc1020, true);
	wake_unlock(&fpc1020->fp_wake_lock);
	wake_lock_timeout(&fpc1020->fp_wake_lock,
			  FPC1020_DEFAULT_WAKELOCK_TIME_S);
	return (finger_up) ? 0 : error;
}

/* -------------------------------------------------------------------- */
int fpc1020_capture_settings(fpc1020_data_t * fpc1020, int select)
{
	int error = 0;
	fpc1020_reg_access_t reg;

	u16 pxlCtrl;
	u16 adc_shift_gain;

	dev_dbg(&fpc1020->spi->dev, "%s #%d\n", __func__, select);

	if (select >= FPC1020_BUFFER_MAX_IMAGES) {
		error = -EINVAL;
		goto out_err;
	}

	pxlCtrl = fpc1020->setup.pxl_ctrl[select];

	adc_shift_gain = fpc1020->setup.adc_shift[select];
	adc_shift_gain <<= 8;
	adc_shift_gain |= fpc1020->setup.adc_gain[select];

	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_PXL_CTRL, &pxlCtrl);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out_err;

	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_ADC_SHIFT_GAIN, &adc_shift_gain);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out_err;

out_err:
	if (error)
		dev_err(&fpc1020->spi->dev, "%s Error %d\n", __func__, error);

	return error;
}

/* -------------------------------------------------------------------- */
int fpc1020_capture_set_crop(fpc1020_data_t * fpc1020,
			     int first_column,
			     int num_columns, int first_row, int num_rows)
{
	fpc1020_reg_access_t reg;
	u32 temp_u32;

	temp_u32 = first_row;
	temp_u32 <<= 8;
	temp_u32 |= num_rows;
	temp_u32 <<= 8;
	temp_u32 |= (first_column * fpc1020->chip.adc_group_size);
	temp_u32 <<= 8;
	temp_u32 |= (num_columns * fpc1020->chip.adc_group_size);

	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_IMG_CAPT_SIZE, &temp_u32);
	return fpc1020_reg_access(fpc1020, &reg);
}

/* -------------------------------------------------------------------- */
int fpc1020_capture_set_crop_selftest(fpc1020_data_t * fpc1020)
{
	int first_column;
	int num_columns;
	int first_row;
	int num_rows;
	int error;

	switch (fpc1020->chip.type) {
	case FPC1020_CHIP_1020A:
		first_column = 0;
		num_columns = 24;
		first_row = 0;
		num_rows = 192;
		break;
	case FPC1020_CHIP_1021A:
	case FPC1020_CHIP_1021B:
		first_column = 0;
		num_columns = 20;
		first_row = 0;
		num_rows = 160;
		break;
	case FPC1020_CHIP_1150A:
		first_column = 0;
		num_columns = 10;
		first_row = 0;
		num_rows = 208;
		break;
		/* get the value from fpc1020_setup_default_1140 */
	case FPC1020_CHIP_1140B:
		first_column = 0;
		num_columns = 7;
		first_row = 0;
		num_rows = 192;
		break;
	default:
		first_column = 0;
		num_columns = 20;
		first_row = 0;
		num_rows = 160;
		break;
	}

	error =
	    fpc1020_capture_set_crop(fpc1020, first_column, num_columns,
				     first_row, num_rows);
	return error;
}

/* -------------------------------------------------------------------- */
int fpc1020_capture_buffer(fpc1020_data_t * fpc1020,
			   u8 * data, u32 offset, u32 image_size_bytes)
{
	int error = 0;

	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);

	error = fpc1020_cmd(fpc1020,
			    FPC1020_CMD_CAPTURE_IMAGE,
			    FPC_1020_IRQ_REG_BIT_FIFO_NEW_DATA);

	if (error < 0)
		goto out_error;

	error = fpc1020_fetch_image(fpc1020,
				    data,
				    offset,
				    image_size_bytes,
				    (u32) fpc1020->huge_buffer_size);
	if (error < 0)
		goto out_error;

	return 0;

out_error:
	dev_dbg(&fpc1020->spi->dev, "%s FAILED %d\n", __func__, error);

	return error;
}

/* -------------------------------------------------------------------- */
