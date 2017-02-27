/*
* Copyright (c) 2016, STMicroelectronics - All Rights Reserved
*
* License terms: BSD 3-clause "New" or "Revised" License.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice, this
* list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright notice,
* this list of conditions and the following disclaimer in the documentation
* and/or other materials provided with the distribution.
*
* 3. Neither the name of the copyright holder nor the names of its contributors
* may be used to endorse or promote products derived from this software
* without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/**  @file stmvl53l1-i2c.h
 * Linux kernel i2c/cci  wrapper for  ST VL53L1 sensor i2c interface
 **/

#ifndef STMVL53L1_I2C_H
#define STMVL53L1_I2C_H
#include <linux/types.h>
#include "stmvl53l1.h"

struct i2c_data {
	struct i2c_client *client;
	/** back link to driver for interrupt and clean-up */
	struct stmvl53l1_data *vl53l1_data;

	/* reference counter */
	struct kref ref;

	/*!< if null no regulator use for power ctrl */
	struct regulator *vdd;

	/*!< power enable gpio number
	 *
	 * if -1 no gpio if vdd not avl pwr is not controllable
	*/
	int pwren_gpio;

	/*!< xsdn reset (low active) gpio number to device
	 *
	 *  -1  mean none assume no "resetable"
	*/
	int xsdn_gpio;

	/*!< intr gpio number to device
	 *
	 *  intr is active/low negative edge by default
	 *
	 *  -1  mean none assume use polling
	 *  @warning if the dev tree and intr gpio is require please adapt code
	*/
	int intr_gpio;

	/*!< device boot i2c register address
	 *
	 * boot_reg is the value of device i2c address after it is bring out
	 * of reset.
	 */
	int boot_reg;

	/*!< is set if above irq gpio got acquired */
	struct i2d_data_flags_t {
		unsigned pwr_owned:1; /*!< set if pwren gpio is owned*/
		unsigned xsdn_owned:1; /*!< set if sxdn  gpio is owned*/
		unsigned intr_owned:1; /*!< set if intr  gpio is owned*/
		unsigned intr_started:1; /*!< set if irq is hanlde  */
	} io_flag;

	/** the irq vectore assigned to gpio
	 * -1 if no irq hanled
	*/
	int irq;

	struct msgtctrl_t {
		unsigned unhandled_irq_vec:1;
	} msg_flag;
};

int stmvl53l1_init_i2c(void);
void __exit stmvl53l1_exit_i2c(void *);
int stmvl53l1_power_up_i2c(void *);
int stmvl53l1_power_down_i2c(void *);
int stmvl53l1_reset_release_i2c(void *);
int stmvl53l1_reset_hold_i2c(void *);
void stmvl53l1_clean_up_i2c(void);
int stmvl53l1_start_intr(void *object, int *poll_mode);
void *stmvl53l1_get(void *);
void stmvl53l1_put(void *);

#endif /* STMVL53L1_I2C_H */
