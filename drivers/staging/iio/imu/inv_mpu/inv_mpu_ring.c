/*
* Copyright (C) 2012 Invensense, Inc.
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/jiffies.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kfifo.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>

#include "iio.h"
#include "kfifo_buf.h"
#include "trigger_consumer.h"
#include "sysfs.h"

#include "inv_mpu_iio.h"

static int inv_push_8bytes_buffer(struct inv_mpu_state *st, u16 hdr,
							u64 t, s16 *d)
{
	struct iio_dev *indio_dev = iio_priv_to_dev(st);
	u8 buf[8];
	int i;

	memcpy(buf, &hdr, sizeof(hdr));
	for (i = 0; i < 3; i++)
		memcpy(&buf[2 + i * 2], &d[i], sizeof(d[i]));
	iio_push_to_buffer(indio_dev->buffer, buf, 0);
	memcpy(buf, &t, sizeof(t));
	iio_push_to_buffer(indio_dev->buffer, buf, 0);

	return 0;
}

static int inv_send_pressure_data(struct inv_mpu_state *st)
{
	int pressure_divider;
	short sen[3];
	struct inv_chip_config_s *conf;

	conf = &st->chip_config;
	if (!conf->pressure_enable)
		return 0;
	if (conf->dmp_on && conf->dmp_event_int_on)
		return 0;
	if (st->chip_config.dmp_on)
		pressure_divider = st->pressure_dmp_divider;
	else
		pressure_divider = st->pressure_divider;
	/* if batch mode is on, pressure divider needs to be decreased */
	if (st->chip_config.batchmode_on) {
			pressure_divider /=
				st->chip_config.batchmode_counter;
	}
	if (pressure_divider <= st->pressure_counter) {
		st->slave_pressure->read_data(st, sen);
		st->pressure_counter = 0;
		inv_push_8bytes_buffer(st, PRESSURE_HDR, st->last_ts, sen);
	} else if (pressure_divider != 0) {
		st->pressure_counter++;
	}

	return 0;
}

static int inv_send_compass_data(struct inv_mpu_state *st)
{
	int compass_divider;
	short sen[3];
	struct inv_chip_config_s *conf;

	conf = &st->chip_config;
	if (!conf->compass_enable)
		return 0;
	if (conf->dmp_on && conf->dmp_event_int_on)
		return 0;
	if (!conf->normal_compass_measure) {
		conf->normal_compass_measure = 1;
		return 0;
	}

	if (st->chip_config.dmp_on)
		compass_divider = st->compass_dmp_divider;
	else
		compass_divider = st->compass_divider;
	/* if batch mode is on, compass divider needs to be decreased */
	if (st->chip_config.batchmode_on) {
			compass_divider /=
				st->chip_config.batchmode_counter;
	}
	if (compass_divider <= st->compass_counter) {
		st->slave_compass->read_data(st, sen);
		st->compass_counter = 0;
		inv_push_8bytes_buffer(st, COMPASS_HDR, st->last_ts, sen);
	} else if (compass_divider != 0) {
		st->compass_counter++;
	}

	return 0;
}

int inv_batchmode_setup(struct inv_mpu_state *st)
{
	int a, b, c, r;

	if (st->chip_config.dmp_on &&
			(st->chip_config.batchmode_timeout > 0) &&
			(st->chip_config.dmp_event_int_on == 0)) {
		b = st->chip_config.bytes_per_datum;
		a = __roundup_pow_of_two(b);
		c = ONE_K_HZ / st->chip_config.dmp_output_rate;
		c = st->chip_config.batchmode_timeout / c;

		if ((a * c > FIFO_SIZE) &&
				(!st->chip_config.batchmode_overflow_on))
			c = FIFO_SIZE / a;
		if (!c)
			return 0;
		r = inv_write_2bytes(st, KEY_BM_NUMWORD_TOFILL, (a - b) >> 1);
		if (r)
			return r;
		r = inv_write_2bytes(st, KEY_BM_BATCH_THLD, c);
		if (r)
			return r;
		r = inv_write_2bytes(st, KEY_BM_BATCH_CNTR, 0);
		if (r)
			return r;
		st->chip_config.batchmode_counter = c;
		st->chip_config.batchmode_bpm = a;
		st->chip_config.batchmode_on = true;
	} else {
		st->chip_config.batchmode_on = false;
		r = inv_write_2bytes(st, KEY_BM_BATCH_THLD, 0);
		if (r)
			return r;
	}
	r = inv_write_2bytes(st, KEY_BM_ENABLE, st->chip_config.batchmode_on);

	return r;
}

/**
 *  reset_fifo_mpu3050() - Reset FIFO related registers
 *  @st:	Device driver instance.
 */
static int reset_fifo_mpu3050(struct iio_dev *indio_dev)
{
	struct inv_reg_map_s *reg;
	int result;
	u8 val, user_ctrl;
	struct inv_mpu_state  *st = iio_priv(indio_dev);
	reg = &st->reg;

	/* disable interrupt */
	result = inv_i2c_single_write(st, reg->int_enable,
				st->plat_data.int_config);
	if (result)
		return result;
	/* disable the sensor output to FIFO */
	result = inv_i2c_single_write(st, reg->fifo_en, 0);
	if (result)
		goto reset_fifo_fail;
	result = inv_i2c_read(st, reg->user_ctrl, 1, &user_ctrl);
	if (result)
		goto reset_fifo_fail;
	/* disable fifo reading */
	user_ctrl &= ~BIT_FIFO_EN;
	st->chip_config.has_footer = 0;
	/* reset fifo */
	val = (BIT_3050_FIFO_RST | user_ctrl);
	result = inv_i2c_single_write(st, reg->user_ctrl, val);
	if (result)
		goto reset_fifo_fail;
	if (st->chip_config.dmp_on) {
		/* enable interrupt when DMP is done */
		result = inv_i2c_single_write(st, reg->int_enable,
				st->plat_data.int_config | BIT_DMP_INT_EN);
		if (result)
			return result;

		result = inv_i2c_single_write(st, reg->user_ctrl,
			BIT_FIFO_EN|user_ctrl);
		if (result)
			return result;
	} else {
		/* enable interrupt */
		if (st->chip_config.accel_fifo_enable ||
		    st->chip_config.gyro_fifo_enable) {
			result = inv_i2c_single_write(st, reg->int_enable,
				st->plat_data.int_config | BIT_DATA_RDY_EN);
			if (result)
				return result;
		}
		/* enable FIFO reading and I2C master interface*/
		result = inv_i2c_single_write(st, reg->user_ctrl,
			BIT_FIFO_EN | user_ctrl);
		if (result)
			return result;
		/* enable sensor output to FIFO and FIFO footer*/
		val = 1;
		if (st->chip_config.accel_fifo_enable)
			val |= BITS_3050_ACCEL_OUT;
		if (st->chip_config.gyro_fifo_enable)
			val |= BITS_GYRO_OUT;
		result = inv_i2c_single_write(st, reg->fifo_en, val);
		if (result)
			return result;
	}

	return 0;
reset_fifo_fail:
	if (st->chip_config.dmp_on)
		val = BIT_DMP_INT_EN;
	else
		val = BIT_DATA_RDY_EN;
	inv_i2c_single_write(st, reg->int_enable,
			     st->plat_data.int_config | val);
	pr_err("reset fifo failed\n");

	return result;
}

/*
 *  inv_set_lpf() - set low pass filer based on fifo rate.
 */
static int inv_set_lpf(struct inv_mpu_state *st, int rate)
{
	const short hz[] = {188, 98, 42, 20, 10, 5};
	const int   d[] = {INV_FILTER_188HZ, INV_FILTER_98HZ,
			INV_FILTER_42HZ, INV_FILTER_20HZ,
			INV_FILTER_10HZ, INV_FILTER_5HZ};
	int i, h, data, result;
	struct inv_reg_map_s *reg;
	reg = &st->reg;
	h = (rate >> 1);
	i = 0;
	while ((h < hz[i]) && (i < ARRAY_SIZE(d) - 1))
		i++;
	data = d[i];
	if (INV_MPU3050 == st->chip_type) {
		if (st->slave_accel != NULL) {
			result = st->slave_accel->set_lpf(st, rate);
			if (result)
				return result;
		}
		result = inv_i2c_single_write(st, reg->lpf, data |
			(st->chip_config.fsr << GYRO_CONFIG_FSR_SHIFT));
	} else {
		result = inv_i2c_single_write(st, reg->lpf, data);
	}
	if (result)
		return result;
	st->chip_config.lpf = data;

	return 0;
}

/**
 *  set_fifo_rate_reg() - Set fifo rate in hardware register
 */
static int set_fifo_rate_reg(struct inv_mpu_state *st)
{
	u8 data;
	u16 fifo_rate;
	int result;
	struct inv_reg_map_s *reg;

	reg = &st->reg;
	fifo_rate = st->chip_config.new_fifo_rate;
	data = ONE_K_HZ / fifo_rate - 1;
	result = inv_i2c_single_write(st, reg->sample_rate_div, data);
	if (result)
		return result;
	result = inv_set_lpf(st, fifo_rate);
	if (result)
		return result;
	/* wait for the sampling rate change to stabilize */
	mdelay(INV_MPU_SAMPLE_RATE_CHANGE_STABLE);
	st->chip_config.fifo_rate = fifo_rate;

	return 0;
}

/**
 *  inv_lpa_mode() - store current low power mode settings
 */
static int inv_lpa_mode(struct inv_mpu_state *st, int lpa_mode)
{
	unsigned long result;
	u8 d;
	struct inv_reg_map_s *reg;

	reg = &st->reg;
	result = inv_i2c_read(st, reg->pwr_mgmt_1, 1, &d);
	if (result)
		return result;
	if (lpa_mode)
		d |= BIT_CYCLE;
	else
		d &= ~BIT_CYCLE;

	result = inv_i2c_single_write(st, reg->pwr_mgmt_1, d);
	if (result)
		return result;
	if (INV_MPU6500 == st->chip_type) {
		d = BIT_FIFO_SIZE_1K;
		if (lpa_mode)
			d |= BIT_ACCEL_FCHOCIE_B;
		result = inv_i2c_single_write(st, REG_6500_ACCEL_CONFIG2, d);
		if (result)
			return result;
	}

	return 0;
}

static int inv_set_master_delay(struct inv_mpu_state *st)
{
	int d, result;
	u8 delay;

	if ((!st->chip_config.compass_enable) &&
		(!st->chip_config.pressure_enable))
		return 0;

	delay = 0;
	d = 0;
	if (st->chip_config.compass_enable) {
		switch (st->plat_data.sec_slave_id) {
		case COMPASS_ID_AK8975:
		case COMPASS_ID_AK8972:
		case COMPASS_ID_AK8963:
			delay = (BIT_SLV0_DLY_EN | BIT_SLV1_DLY_EN);
			break;
		case COMPASS_ID_MLX90399:
			delay = (BIT_SLV0_DLY_EN |
				BIT_SLV1_DLY_EN |
				BIT_SLV2_DLY_EN |
				BIT_SLV3_DLY_EN);
			break;
		default:
			return -EINVAL;
		}
		d = max(d, st->slave_compass->rate_scale);
	}
	if (st->chip_config.pressure_enable) {
		switch (st->plat_data.aux_slave_id) {
		case PRESSURE_ID_BMP280:
			delay |= (BIT_SLV2_DLY_EN | BIT_SLV3_DLY_EN);
			break;
		default:
			return -EINVAL;
		}
		d = max(d, st->slave_pressure->rate_scale);
	}
	result = inv_i2c_single_write(st, REG_I2C_MST_DELAY_CTRL, delay);
	if (result)
		return result;

	d = d * st->chip_config.fifo_rate / ONE_K_HZ;
	if (st->chip_config.dmp_on)
		d = max(d, st->chip_config.fifo_rate /
					st->chip_config.dmp_output_rate);
	if (d > 0)
		d -= 1;
	/* I2C_MST_DLY is set to slow down secondary I2C */
	return inv_i2c_single_write(st, REG_I2C_SLV4_CTRL, d);
}

/*
 *  reset_fifo_itg() - Reset FIFO related registers.
 */
static int reset_fifo_itg(struct iio_dev *indio_dev)
{
	struct inv_reg_map_s *reg;
	int result;
	u8 val, int_word;
	struct inv_mpu_state  *st = iio_priv(indio_dev);
	reg = &st->reg;

	if (st->chip_config.lpa_mode) {
		result = inv_lpa_mode(st, 0);
		if (result) {
			pr_err("reset lpa mode failed\n");
			return result;
		}
	}
	/* disable interrupt */
	result = inv_i2c_single_write(st, reg->int_enable, 0);
	if (result) {
		pr_err("int_enable write failed\n");
		return result;
	}
	/* disable the sensor output to FIFO */
	result = inv_i2c_single_write(st, reg->fifo_en, 0);
	if (result)
		goto reset_fifo_fail;
	/* disable fifo reading */
	result = inv_i2c_single_write(st, reg->user_ctrl, 0);
	if (result)
		goto reset_fifo_fail;
	int_word = 0;

	/* MPU6500's BIT_6500_WOM_EN is the same as BIT_MOT_EN */
	if (st->mot_int.mot_on)
		int_word |= BIT_MOT_EN;

	if (st->chip_config.dmp_on) {
		val = (BIT_FIFO_RST | BIT_DMP_RST);
		result = inv_i2c_single_write(st, reg->user_ctrl, val);
		if (result)
			goto reset_fifo_fail;
		if (st->chip_config.dmp_int_on) {
			int_word |= BIT_DMP_INT_EN;
			result = inv_i2c_single_write(st, reg->int_enable,
							int_word);
			if (result)
				return result;
		}
		val = (BIT_DMP_EN | BIT_FIFO_EN);
		if ((st->chip_config.compass_enable ||
			st->chip_config.pressure_enable) &&
			(!st->chip_config.dmp_event_int_on))
			val |= BIT_I2C_MST_EN;
		result = inv_i2c_single_write(st, reg->user_ctrl, val);
		if (result)
			goto reset_fifo_fail;
	} else {
		/* reset FIFO and possibly reset I2C*/
		val = BIT_FIFO_RST;
		result = inv_i2c_single_write(st, reg->user_ctrl, val);
		if (result)
			goto reset_fifo_fail;
		/* enable interrupt */
		if (st->chip_config.accel_fifo_enable ||
		    st->chip_config.gyro_fifo_enable ||
		    st->chip_config.compass_enable ||
		    st->chip_config.pressure_enable) {
			int_word |= BIT_DATA_RDY_EN;
		}
		result = inv_i2c_single_write(st, reg->int_enable, int_word);
		if (result)
			return result;
		/* enable FIFO reading and I2C master interface*/
		val = BIT_FIFO_EN;
		if (st->chip_config.compass_enable ||
		    st->chip_config.pressure_enable)
			val |= BIT_I2C_MST_EN;
		result = inv_i2c_single_write(st, reg->user_ctrl, val);
		if (result)
			goto reset_fifo_fail;
		/* enable sensor output to FIFO */
		val = 0;
		if (st->chip_config.gyro_fifo_enable)
			val |= BITS_GYRO_OUT;
		if (st->chip_config.accel_fifo_enable)
			val |= BIT_ACCEL_OUT;
		result = inv_i2c_single_write(st, reg->fifo_en, val);
		if (result)
			goto reset_fifo_fail;
	}
	result = inv_set_master_delay(st);
	if (result)
		goto reset_fifo_fail;
	st->last_ts = get_time_ns();
	st->ts_counter = 0;
	st->diff_accumulater = 0;
	st->chip_config.normal_compass_measure = 0;
	result = inv_lpa_mode(st, st->chip_config.lpa_mode);
	if (result)
		goto reset_fifo_fail;

	return 0;

reset_fifo_fail:
	if (st->chip_config.dmp_on)
		val = BIT_DMP_INT_EN;
	else
		val = BIT_DATA_RDY_EN;
	inv_i2c_single_write(st, reg->int_enable, val);
	pr_err("reset fifo failed\n");

	return result;
}

/**
 *  inv_clear_kfifo() - clear time stamp fifo
 *  @st:	Device driver instance.
 */
static void inv_clear_kfifo(struct inv_mpu_state *st)
{
	unsigned long flags;

	spin_lock_irqsave(&st->time_stamp_lock, flags);
	kfifo_reset(&st->timestamps);
	spin_unlock_irqrestore(&st->time_stamp_lock, flags);
}

/*
 *  inv_reset_fifo() - Reset FIFO related registers.
 */
int inv_reset_fifo(struct iio_dev *indio_dev)
{
	struct inv_mpu_state *st = iio_priv(indio_dev);

	inv_clear_kfifo(st);
	if (INV_MPU3050 == st->chip_type)
		return reset_fifo_mpu3050(indio_dev);
	else
		return reset_fifo_itg(indio_dev);
}

static int inv_send_accel_data(struct inv_mpu_state *st, bool on)
{
	u8 rn[] = {0xa3, 0xa3};
	u8 rf[] = {0xf4, 0x12};
	int result;

	if (on)
		result = mem_w_key(KEY_CFG_OUT_ACCL, ARRAY_SIZE(rn), rn);
	else
		result = mem_w_key(KEY_CFG_OUT_ACCL, ARRAY_SIZE(rf), rf);

	return result;
}

static int inv_send_gyro_data(struct inv_mpu_state *st, bool on)
{
	u8 rn[]  = {0xa3, 0xa3};
	u8 rf[] = {0xf4, 0x0a};
	int result;

	if (on)
		result = mem_w_key(KEY_CFG_OUT_GYRO, ARRAY_SIZE(rn), rn);
	else
		result = mem_w_key(KEY_CFG_OUT_GYRO, ARRAY_SIZE(rf), rf);

	return result;
}

static int inv_send_three_q_data(struct inv_mpu_state *st, bool on)
{
	u8 rn[]  = {0xa3, 0xa3};
	u8 rf[] = {0xf4, 0x0c};
	int result;

	if (on)
		result = mem_w_key(KEY_CFG_OUT_3QUAT, ARRAY_SIZE(rn), rn);
	else
		result = mem_w_key(KEY_CFG_OUT_3QUAT, ARRAY_SIZE(rf), rf);

	return result;
}

static int inv_send_six_q_data(struct inv_mpu_state *st, bool on)
{
	u8 rn[]  = {0xa3, 0xa3};
	u8 rf[] = {0xf4, 0x0c};
	int result;

	if (on)
		result = mem_w_key(KEY_CFG_OUT_6QUAT, ARRAY_SIZE(rn), rn);
	else
		result = mem_w_key(KEY_CFG_OUT_6QUAT, ARRAY_SIZE(rf), rf);

	return result;
}

static int inv_send_ped_q_data(struct inv_mpu_state *st, bool on)
{
	u8 rn1[]  = {0xa3, 0xa3};
	u8 rn2[]  = {0xf3, 0xf3};
	u8 rf1[] = {0xf4, 0x0c};
	u8 rf2[] = {0xf4, 0x03};
	int result;

	if (on) {
		result = mem_w_key(KEY_CFG_OUT_PQUAT, ARRAY_SIZE(rn1), rn1);
		if (result)
			return result;
		result = mem_w_key(KEY_CFG_PEDSTEP_DET, ARRAY_SIZE(rn2), rn2);
	} else {
		result = mem_w_key(KEY_CFG_OUT_PQUAT, ARRAY_SIZE(rf1), rf1);
		if (result)
			return result;
		result = mem_w_key(KEY_CFG_PEDSTEP_DET, ARRAY_SIZE(rf2), rf2);
	}

	return result;
}

static int inv_set_dmp_sysfs(struct inv_mpu_state *st)
{
	int result;

	result = inv_set_fifo_rate(st, st->chip_config.dmp_output_rate);
	if (result)
		return result;
	result = inv_set_interrupt_on_gesture_event(st,
				st->chip_config.dmp_event_int_on);

	if (st->chip_config.accel_fifo_enable &&
				(!st->chip_config.dmp_event_int_on))
		result = inv_send_accel_data(st, true);
	else
		result = inv_send_accel_data(st, false);

	if (st->chip_config.gyro_fifo_enable &&
				(!st->chip_config.dmp_event_int_on))
		result = inv_send_gyro_data(st, true);
	else
		result = inv_send_gyro_data(st, false);

	if (st->chip_config.three_q_on &&
				(!st->chip_config.dmp_event_int_on))
		result = inv_send_three_q_data(st, true);
	else
		result = inv_send_three_q_data(st, false);

	if (st->chip_config.six_q_on &&
				(!st->chip_config.dmp_event_int_on))
		result = inv_send_six_q_data(st, true);
	else
		result = inv_send_six_q_data(st, false);

	if (st->chip_config.ped_q_on &&
				(!st->chip_config.dmp_event_int_on))
		result = inv_send_ped_q_data(st, true);
	else
		result = inv_send_ped_q_data(st, false);

	result = inv_batchmode_setup(st);
	if (result)
		return result;

	return result;
}

static void inv_get_data_count(struct inv_mpu_state *st)
{
	struct inv_chip_config_s *c;
	int b;

	c = &st->chip_config;
	b = 0;
	if (st->chip_config.dmp_on) {
		if (c->gyro_fifo_enable)
			b += HEADERED_NORMAL_BYTES;
		if (c->accel_fifo_enable)
			b += HEADERED_NORMAL_BYTES;
		if (c->ped_q_on)
			b += HEADERED_NORMAL_BYTES;
		if (c->six_q_on)
			b += HEADERED_Q_BYTES;
		if (c->three_q_on)
			b += HEADERED_Q_BYTES;
	} else {
		if (st->chip_config.accel_fifo_enable)
			b += BYTES_PER_SENSOR;
		if (st->chip_config.gyro_fifo_enable)
			b += BYTES_PER_SENSOR;
	}
	c->bytes_per_datum = b;

	return;
}
/*
 *  set_inv_enable() - main enable/disable function.
 */
static int set_inv_enable(struct iio_dev *indio_dev,
			bool enable) {
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct inv_reg_map_s *reg;
	int result;

	reg = &st->reg;
	if (enable) {
		inv_get_data_count(st);
		if (st->chip_config.new_fifo_rate !=
				st->chip_config.fifo_rate) {
			result = set_fifo_rate_reg(st);
			if (result)
				return result;
		}
		if (st->chip_config.dmp_on) {
			result = inv_set_dmp_sysfs(st);
			if (result)
				return result;
		} else {
			st->chip_config.batchmode_on = false;
		}

		if (st->chip_config.gyro_enable) {
			result = st->switch_gyro_engine(st, true);
			if (result)
				return result;
		}
		if (st->chip_config.accel_enable) {
			result = st->switch_accel_engine(st, true);
			if (result)
				return result;
		}
		if (st->chip_config.compass_enable) {
			result = st->slave_compass->resume(st);
			if (result)
				return result;
		}
		if (st->chip_config.pressure_enable) {
			result = st->slave_pressure->resume(st);
			if (result)
				return result;
		}
		result = inv_reset_fifo(indio_dev);
		if (result)
			return result;
	} else {
		if ((INV_MPU3050 != st->chip_type)
			&& st->chip_config.lpa_mode) {
			/* if the chip is in low power mode,
				register write/read could fail */
			result = inv_lpa_mode(st, 0);
			if (result)
				return result;
		}
		result = inv_i2c_single_write(st, reg->fifo_en, 0);
		if (result)
			return result;
		/* disable fifo reading */
		if (INV_MPU3050 != st->chip_type) {
			result = inv_i2c_single_write(st, reg->int_enable, 0);
			if (result)
				return result;
			result = inv_i2c_single_write(st, reg->user_ctrl, 0);
		} else {
			result = inv_i2c_single_write(st, reg->int_enable,
				st->plat_data.int_config);
		}
		if (result)
			return result;
		/* turn off the gyro/accel engine during disable phase */
		result = st->switch_gyro_engine(st, false);
		if (result)
			return result;
		result = st->switch_accel_engine(st, false);
		if (result)
			return result;
		if (st->chip_config.compass_enable) {
			result = st->slave_compass->suspend(st);
			if (result)
				return result;
		}
		if (st->chip_config.pressure_enable) {
			result = st->slave_pressure->suspend(st);
			if (result)
				return result;
		}

		result = st->set_power_state(st, false);
		if (result)
			return result;
	}
	st->chip_config.enable = enable;

	return 0;
}

/**
 *  inv_irq_handler() - Cache a timestamp at each data ready interrupt.
 */
static irqreturn_t inv_irq_handler(int irq, void *dev_id)
{
	struct inv_mpu_state *st = (struct inv_mpu_state *)dev_id;
	u64 ts;

	ts = get_time_ns();
	kfifo_in_spinlocked(&st->timestamps, &ts, 1, &st->time_stamp_lock);

	return IRQ_WAKE_THREAD;
}

static void inv_report_data_3050(struct iio_dev *indio_dev, s64 t,
			int has_footer, u8 *data)
{
	struct inv_mpu_state *st = iio_priv(indio_dev);
	int ind, i;
	struct inv_chip_config_s *conf;
	short s[THREE_AXIS];

	conf = &st->chip_config;
	ind = 0;
	if (has_footer)
		ind += 2;

	if (conf->gyro_fifo_enable) {
		for (i = 0; i < 3; i++) {
			s[i] = be16_to_cpup((__be16 *)(&data[ind + i * 2]));
			st->raw_gyro[i] = s[i];
		}
		inv_push_8bytes_buffer(st, GYRO_HDR, t, s);
		ind += BYTES_PER_SENSOR;
	}
	if (conf->accel_fifo_enable) {
		st->slave_accel->combine_data(&data[ind], s);
		for (i = 0; i < 3; i++)
			st->raw_accel[i] = s[i];
		inv_push_8bytes_buffer(st, ACCEL_HDR, t, s);
	}
}

/**
 *  inv_read_fifo_mpu3050() - Transfer data from FIFO to ring buffer for
 *                            mpu3050.
 */
irqreturn_t inv_read_fifo_mpu3050(int irq, void *dev_id)
{

	struct inv_mpu_state *st = (struct inv_mpu_state *)dev_id;
	struct iio_dev *indio_dev = iio_priv_to_dev(st);
	int bytes_per_datum;
	u8 data[64];
	int result;
	short fifo_count, byte_read;
	s64 timestamp;
	struct inv_reg_map_s *reg;

	reg = &st->reg;
	/* It is impossible that chip is asleep or enable is
	zero when interrupt is on
	*  because interrupt is now connected with enable */
	mutex_lock(&indio_dev->mlock);
	if (st->chip_config.dmp_on)
		bytes_per_datum = HEADERED_NORMAL_BYTES;
	else
		bytes_per_datum = (st->chip_config.accel_fifo_enable +
			st->chip_config.gyro_fifo_enable) * BYTES_PER_SENSOR;
	if (st->chip_config.has_footer)
		byte_read = bytes_per_datum + MPU3050_FOOTER_SIZE;
	else
		byte_read = bytes_per_datum;

	fifo_count = 0;
	if (byte_read != 0) {
		result = inv_i2c_read(st, reg->fifo_count_h,
				FIFO_COUNT_BYTE, data);
		if (result)
			goto end_session;
		fifo_count = be16_to_cpup((__be16 *)(&data[0]));
		if (fifo_count < byte_read)
			goto end_session;
		if (fifo_count & 1)
			goto flush_fifo;
		if (fifo_count > FIFO_THRESHOLD)
			goto flush_fifo;
		/* Timestamp mismatch. */
		if (kfifo_len(&st->timestamps) <
			fifo_count / byte_read)
			goto flush_fifo;
		if (kfifo_len(&st->timestamps) >
			fifo_count / byte_read + TIME_STAMP_TOR) {
			if (st->chip_config.dmp_on) {
				result = kfifo_out(&st->timestamps,
				&timestamp, 1);
				if (result != 1)
					goto flush_fifo;
			} else {
				goto flush_fifo;
			}
		}
	}
	while ((bytes_per_datum != 0) && (fifo_count >= byte_read)) {
		result = inv_i2c_read(st, reg->fifo_r_w, byte_read, data);
		if (result)
			goto flush_fifo;

		result = kfifo_out(&st->timestamps, &timestamp, 1);
		if (result != 1)
			goto flush_fifo;
		inv_report_data_3050(indio_dev, timestamp,
				     st->chip_config.has_footer, data);
		fifo_count -= byte_read;
		if (st->chip_config.has_footer == 0) {
			st->chip_config.has_footer = 1;
			byte_read = bytes_per_datum + MPU3050_FOOTER_SIZE;
		}
	}

end_session:
	mutex_unlock(&indio_dev->mlock);

	return IRQ_HANDLED;

flush_fifo:
	/* Flush HW and SW FIFOs. */
	inv_reset_fifo(indio_dev);
	inv_clear_kfifo(st);
	mutex_unlock(&indio_dev->mlock);

	return IRQ_HANDLED;
}

static int inv_report_gyro_accel(struct iio_dev *indio_dev,
					u8 *data, s64 t)
{
	struct inv_mpu_state *st = iio_priv(indio_dev);
	short s[THREE_AXIS];
	int ind;
	int i;
	struct inv_chip_config_s *conf;

	conf = &st->chip_config;
	ind = 0;
	if (conf->accel_fifo_enable) {
		for (i = 0; i < 3; i++)
			s[i] = be16_to_cpup((__be16 *)(&data[ind + i * 2]));
		inv_push_8bytes_buffer(st, ACCEL_HDR, t, s);
		ind += BYTES_PER_SENSOR;
	}

	if (conf->gyro_fifo_enable) {
		for (i = 0; i < 3; i++)
			s[i] = be16_to_cpup((__be16 *)(&data[ind + i * 2]));
		inv_push_8bytes_buffer(st, GYRO_HDR, t, s);
		ind += BYTES_PER_SENSOR;
	}

	return 0;
}

static void inv_process_motion(struct inv_mpu_state *st)
{
	struct iio_dev *indio_dev = iio_priv_to_dev(st);
	s32 diff, true_motion;
	s64 timestamp;
	int result;
	u8 data[1];

	/* motion interrupt */
	result = inv_i2c_read(st, REG_INT_STATUS, 1, data);
	if (result)
		return;

	if (data[0] & BIT_MOT_INT) {
		timestamp = get_time_ns();
		diff = (int)(((timestamp - st->mpu6500_last_motion_time) >>
			NS_PER_MS_SHIFT));
		if (diff > st->mot_int.mot_dur) {
			st->mpu6500_last_motion_time = timestamp;
			true_motion = 1;
		} else {
			true_motion = 0;
		}
		if (true_motion)
			sysfs_notify(&indio_dev->dev.kobj, NULL,
					"event_accel_motion");
	}
}

static int inv_get_timestamp(struct inv_mpu_state *st, int count)
{
	u32 *dur;
	u32 thresh;
	s32 diff, result, counter;
	u64 ts;

	/* goal of algorithm is to estimate the true frequency of the chip */
	if (st->chip_config.dmp_on && st->chip_config.dmp_event_int_on)
		return 0;
	if (st->chip_config.dmp_on)
		dur = &st->irq_dmp_dur_ns;
	else
		dur = &st->irq_dur_ns;
	if (st->chip_config.batchmode_on)
		counter = st->chip_config.batchmode_counter;
	else
		counter = 1;
	thresh = min((u32)((*dur) >> 2), (u32)(10 * NSEC_PER_MSEC));
	while (kfifo_len(&st->timestamps) >= count) {
		result = kfifo_out(&st->timestamps, &ts, 1);
		if (result != 1)
			return -EINVAL;
		/* first time since reset fifo, just take it */
		if (!st->ts_counter) {
			st->last_ts = ts;
			st->prev_kfifo_ts = ts;
			st->ts_counter++;
			return 0;
		}
		/* special handling. If it is too big, let go */
		if (st->chip_config.batchmode_on) {
			if (ts - st->prev_kfifo_ts >= INT_MAX) {
				st->last_ts = ts;
				st->prev_kfifo_ts = ts;
				return 0;
			} else {
				diff = (s32)(ts - st->prev_kfifo_ts);
				diff /= counter;
			}
		} else {
			diff = (s32)(ts - st->prev_kfifo_ts);
		}
		st->prev_kfifo_ts = ts;
		if (abs(diff - (*dur)) < thresh) {
			st->diff_accumulater >>= 1;
			if (*dur > diff)
				st->diff_accumulater -= (((*dur) - diff) >> 7);
			else
				st->diff_accumulater += ((diff - (*dur)) >> 7);
			*dur += st->diff_accumulater;
		}
	}
	ts = *dur;
	ts *= counter;
	st->last_ts += ts;

	return 0;
}

static void inv_process_dmp_interrupt(struct inv_mpu_state *st)
{
	int r;
	u8 d[2];
	struct iio_dev *indio_dev = iio_priv_to_dev(st);

#define DMP_INT_SMD             0x04
#define DMP_INT_PED             0x08
#define DMP_INT_DISPLAY_ORIENTATION 0x02
	//printk("dmp irq detected\n");

	if (st->chip_config.smd_enable | st->chip_config.ped_int_on |
		st->chip_config.display_orient_on) {
		r = inv_i2c_read(st, REG_DMP_INT_STATUS, 1, d);
		if (r) {
			printk("i2c_read failed %d\n",r);
			return;
		}
		if (d[0] & DMP_INT_SMD) {
				sysfs_notify(&indio_dev->dev.kobj, NULL,
						"event_smd");
				st->chip_config.smd_enable = 0;
		}
		if (d[0] & DMP_INT_PED)
			sysfs_notify(&indio_dev->dev.kobj, NULL,
						"event_pedometer");
		if (d[0] & DMP_INT_DISPLAY_ORIENTATION) {
			r = mpu_memory_read(st, st->i2c_addr,
				inv_dmp_get_address(KEY_SO_DATA), 2, d);

			st->display_orient_data =
				(u8)be16_to_cpup((__be16 *)(d));
			sysfs_notify(&indio_dev->dev.kobj, NULL,
				     "event_display_orientation");
		}
		//printk("read sucess 0x%x\n", d[0]);
	}
}

static int inv_read_send_data_to_buffer(struct inv_mpu_state *st,
							int bpm, u64 t)
{

	u8  d[MAX_BYTES_PER_SAMPLE], buf[IIO_BUFFER_BYTES], *dptr;
	struct iio_dev *indio_dev = iio_priv_to_dev(st);
	struct inv_reg_map_s *r;
	int res, i, q[3];
	s16 sen[3];
	u16 hdr;

	r = &st->reg;
	res = inv_i2c_read(st, r->fifo_r_w, bpm, d);
	if (res)
		return res;
	dptr = d;
	while (dptr - d < bpm) {
		hdr = (u16)be16_to_cpup((__be16 *)(dptr));
		switch (hdr) {
		case ACCEL_HDR:
		case ACCEL_STEP_HDR:
		case GYRO_HDR:
		case PEDQUAT_HDR:
		case PEDQUAT_STEP_HDR:
			for (i = 0; i < 3; i++) {
				sen[i] =
				(short)be16_to_cpup((__be16 *)(dptr + 2
				+ i * 2));
			}
			dptr += HEADERED_NORMAL_BYTES;
            //printk("Inv sensor data [A/G/Ped] = %d, [0]%d, [1]%d, [2]%d. ---->\n", hdr, sen[0], sen[1], sen[2]);
			inv_push_8bytes_buffer(st, hdr, t, sen);
			break;
		case LPQUAT_HDR:
		case SIXQUAT_HDR:
			for (i = 0; i < 3; i++) {
				q[i] =
				(int)be32_to_cpup((__be32 *)(dptr + 4
				+ i * 4));
			}
            //printk("Inv sensor data [Q] = [0]%d, [1]%d, [2]%d. ---->\n", sen[0], sen[1], sen[2]);
			memcpy(buf, &hdr, sizeof(hdr));
			memcpy(buf + 4, &q[0], sizeof(q[0]));
			iio_push_to_buffer(indio_dev->buffer, buf, 0);
			for (i = 0; i < 2; i++)
				memcpy(buf + 4 * i, &q[i + 1],
						sizeof(q[i]));
			iio_push_to_buffer(indio_dev->buffer, buf, 0);
			memcpy(buf, &t, sizeof(t));
			iio_push_to_buffer(indio_dev->buffer, buf, 0);

			dptr += HEADERED_Q_BYTES;
			break;
		default:
			dptr += HEADERED_NORMAL_BYTES;
			break;
		}
	}

	return 0;
}

static int inv_get_headered_data(struct inv_mpu_state *st, int fc)
{
	struct inv_reg_map_s *r;
	int res, bpm;

	r = &st->reg;
	bpm = st->chip_config.bytes_per_datum;
	if (fc < bpm)
		return -EINVAL;
	while (fc > bpm) {
		res = inv_get_timestamp(st, fc / bpm);
		if (res)
			return res;
		inv_read_send_data_to_buffer(st, bpm, st->last_ts);
		fc -= bpm;
	}

	return 0;
}

static int inv_process_batchmode(struct inv_mpu_state *st, int fifo_count)
{
	int bpm, result, counter, i, size, loop;
	struct inv_reg_map_s *reg;

	reg = &st->reg;
	bpm = st->chip_config.batchmode_bpm;
	counter = st->chip_config.batchmode_counter;
	size = bpm * counter;
	loop = counter;
	if (st->chip_config.batchmode_overflow_on) {
		size = min(size, FIFO_SIZE);
		loop = size / bpm;
	}
	if (fifo_count < size)
		return -EINVAL;

	while (fifo_count >= bpm * loop) {
		result = inv_get_timestamp(st, fifo_count / (bpm * loop));
		if (result)
			return 0;
		i = loop;
		while (i > 0) {
			i--;
			inv_read_send_data_to_buffer(st, bpm, st->last_ts -
						(s64)(st->irq_dmp_dur_ns) * i);
		}
		fifo_count -= (bpm * loop);
	}

	return 0;
}

/*
 *  inv_read_fifo() - Transfer data from FIFO to ring buffer.
 */
irqreturn_t inv_read_fifo(int irq, void *dev_id)
{

	struct inv_mpu_state *st = (struct inv_mpu_state *)dev_id;
	struct iio_dev *indio_dev = iio_priv_to_dev(st);
	int result, bpm;
	u8 data[MAX_HW_FIFO_BYTES];
	u16 fifo_count;
	struct inv_reg_map_s *reg;

	mutex_lock(&indio_dev->mlock);
	if (!(iio_buffer_enabled(indio_dev)))
		goto end_session;

	reg = &st->reg;
	if (!(st->chip_config.accel_fifo_enable |
		st->chip_config.gyro_fifo_enable |
		st->chip_config.dmp_on |
		st->chip_config.compass_enable |
		st->chip_config.pressure_enable |
		st->mot_int.mot_on))
		goto end_session;
	if (st->chip_config.lpa_mode) {
		result = inv_i2c_read(st, reg->raw_accel,
				      BYTES_PER_SENSOR, data);
		if (result)
			goto end_session;
		inv_report_gyro_accel(indio_dev, data,
					     get_time_ns());
		goto end_session;
	}
	if (st->mot_int.mot_on)
		inv_process_motion(st);
	if (st->chip_config.dmp_on)
		inv_process_dmp_interrupt(st);

	if (st->chip_config.dmp_on && st->chip_config.dmp_event_int_on)
		goto end_session;
	bpm = st->chip_config.bytes_per_datum;
	fifo_count = 0;
	if (bpm) {
		result = inv_i2c_read(st, reg->fifo_count_h,
			FIFO_COUNT_BYTE, data);
		if (result)
			goto end_session;
		fifo_count = be16_to_cpup((__be16 *)(data));
        //printk("FIFO count = %d, Bytes per Datum = %d. ------>\n", fifo_count, bpm);
		if (fifo_count < bpm)
			goto end_session;
		/* fifo count can't be odd number */
		if (fifo_count & 1)
			goto flush_fifo;
	} else {
		result = inv_get_timestamp(st, 1);
		if (result)
			goto end_session;
		goto send_slave;
	}

	if (st->chip_config.dmp_on) {
		if (st->chip_config.batchmode_on)
			result = inv_process_batchmode(st, fifo_count);
		else
			result = inv_get_headered_data(st, fifo_count);
		if (result)
			goto end_session;
		goto send_slave;
	} else {
		if (fifo_count >  FIFO_THRESHOLD)
			goto flush_fifo;
		while (fifo_count >= bpm) {
			result = inv_i2c_read(st, reg->fifo_r_w, bpm, data);
			if (result)
				goto flush_fifo;
			result = inv_get_timestamp(st, fifo_count / bpm);
			if (result)
				goto flush_fifo;
			inv_report_gyro_accel(indio_dev, data, st->last_ts);
			fifo_count -= bpm;
		}
	}
    //printk("Inv IRQ post process complete. ------>\n");

send_slave:
	inv_send_compass_data(st);
	inv_send_pressure_data(st);
end_session:
	mutex_unlock(&indio_dev->mlock);

	return IRQ_HANDLED;
flush_fifo:
	/* Flush HW and SW FIFOs. */
	inv_reset_fifo(indio_dev);
	inv_clear_kfifo(st);
	mutex_unlock(&indio_dev->mlock);

	return IRQ_HANDLED;
}

void inv_mpu_unconfigure_ring(struct iio_dev *indio_dev)
{
	struct inv_mpu_state *st = iio_priv(indio_dev);
	free_irq(st->client->irq, st);
	iio_kfifo_free(indio_dev->buffer);
};

static int inv_postenable(struct iio_dev *indio_dev)
{
	return set_inv_enable(indio_dev, true);
}

static int inv_predisable(struct iio_dev *indio_dev)
{
	return set_inv_enable(indio_dev, false);
}

static int inv_check_conflict_sysfs(struct iio_dev *indio_dev)
{
	struct inv_mpu_state  *st = iio_priv(indio_dev);

	if (st->chip_config.lpa_mode) {
		/* dmp cannot run with low power mode on */
		st->chip_config.dmp_on = 0;
		st->chip_config.gyro_enable = false;
		st->chip_config.gyro_fifo_enable = false;
		st->chip_config.compass_enable = false;
	}
	if (st->chip_config.gyro_fifo_enable &&
		(!st->chip_config.gyro_enable)) {
		st->chip_config.gyro_enable = true;
	}
	if (st->chip_config.accel_fifo_enable &&
		(!st->chip_config.accel_enable)) {
		st->chip_config.accel_enable = true;
	}

	return 0;
}

static int inv_preenable(struct iio_dev *indio_dev)
{
	int result;
	struct inv_mpu_state *st = iio_priv(indio_dev);

	result = st->set_power_state(st, true);
	if (result)
		return result;

	result = inv_check_conflict_sysfs(indio_dev);
	if (result)
		return result;
	result = iio_sw_buffer_preenable(indio_dev);

	return result;
}

static const struct iio_buffer_setup_ops inv_mpu_ring_setup_ops = {
	.preenable = &inv_preenable,
	.postenable = &inv_postenable,
	.predisable = &inv_predisable,
};

int inv_mpu_configure_ring(struct iio_dev *indio_dev)
{
	int ret;
	struct inv_mpu_state *st = iio_priv(indio_dev);
	struct iio_buffer *ring;

	ring = iio_kfifo_allocate(indio_dev);
	if (!ring)
		return -ENOMEM;
	indio_dev->buffer = ring;
	/* setup ring buffer */
	ring->scan_timestamp = true;
	indio_dev->setup_ops = &inv_mpu_ring_setup_ops;
	/*scan count double count timestamp. should subtract 1. but
	number of channels still includes timestamp*/
	if (INV_MPU3050 == st->chip_type)
		ret = request_threaded_irq(st->client->irq, inv_irq_handler,
			inv_read_fifo_mpu3050,
			IRQF_TRIGGER_RISING | IRQF_SHARED, "inv_irq", st);
	else
		ret = request_threaded_irq(st->client->irq, inv_irq_handler,
			inv_read_fifo,
			IRQF_TRIGGER_RISING | IRQF_SHARED, "inv_irq", st);
	if (ret)
		goto error_iio_sw_rb_free;

	indio_dev->modes |= INDIO_BUFFER_TRIGGERED;
	return 0;
error_iio_sw_rb_free:
	iio_kfifo_free(indio_dev->buffer);
	return ret;
}

