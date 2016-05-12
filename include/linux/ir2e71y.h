/* include/linux/ir2e71y.h  (Display Driver)
 *
 * Copyright (C) 2016 SHARP CORPORATION
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef IR2E71Y_H
#define IR2E71Y_H

#include <uapi/linux/ir2e71y.h>

enum {
    IR2E71Y_BDIC_I2C_M_W,
    IR2E71Y_BDIC_I2C_M_R,
    IR2E71Y_BDIC_I2C_M_R_MODE1,
    IR2E71Y_BDIC_I2C_M_R_MODE2,
    IR2E71Y_BDIC_I2C_M_R_MODE3,
    NUM_IR2E71Y_BDIC_I2C_M
};

enum {
    IR2E71Y_PROX_SENSOR_POWER_OFF,
    IR2E71Y_PROX_SENSOR_POWER_ON,
    IR2E71Y_PROX_SENSOR_BGMODE_ON,
    IR2E71Y_PROX_SENSOR_BGMODE_OFF,
    NUM_IR2E71Y_PROX_SENSOR_POWER
};

struct ir2e71y_bdic_i2c_msg {
    unsigned short addr;
    unsigned char mode;
    unsigned char wlen;
    unsigned char rlen;
    const unsigned char *wbuf;
    unsigned char *rbuf;
};

struct ir2e71y_prox_params {
    unsigned int threshold_low;
    unsigned int threshold_high;
};

#ifdef CONFIG_IR2E71Y
int ir2e71y_enable_analog_power(int enable);
int ir2e71y_prox_sensor_pow_ctl(int power_mode, struct ir2e71y_prox_params *prox_params);
int ir2e71y_write_bdic_i2c(struct ir2e71y_bdic_i2c_msg *i2c_msg);
int ir2e71y_read_bdic_i2c(struct ir2e71y_bdic_i2c_msg *i2c_msg);
#else
static inline int ir2e71y_enable_analog_power(int enable)
{
	return 0;
}
static inline int ir2e71y_prox_sensor_pow_ctl(int power_mode, struct ir2e71y_prox_params *prox_params)
{
	return IR2E71Y_RESULT_SUCCESS;
}
static inline int ir2e71y_write_bdic_i2c(struct ir2e71y_bdic_i2c_msg *i2c_msg)
{
	return IR2E71Y_RESULT_SUCCESS;
}
static inline int ir2e71y_read_bdic_i2c(struct ir2e71y_bdic_i2c_msg *i2c_msg)
{
	return IR2E71Y_RESULT_SUCCESS;
}
#endif /* CONFIG_IR2E71Y */

#endif /* IR2E71Y_H */

/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */
