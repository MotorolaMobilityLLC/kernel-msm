/* Lis3dsh_acc.c - LIS3DSH accelerometer sensor driver
 *
 * Copyright (C) 2012 ASUSTek Inc.
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

#include	<linux/kernel.h>
#include	<linux/init.h>
#include	<linux/module.h>
#include	<linux/err.h>
#include	<linux/errno.h>
#include	<linux/delay.h>
#include	<linux/fs.h>
#include	<linux/i2c.h>
#include	<linux/input.h>
#include	<linux/uaccess.h>
#include	<linux/sysfs.h>
#include	<linux/syscalls.h>
#include	<linux/workqueue.h>
#include	<linux/regulator/consumer.h>
#include	<linux/irq.h>
#include	<linux/gpio.h>
#include	<linux/interrupt.h>
#include	<linux/slab.h>
#include	<linux/wakelock.h>
#include	<linux/sensors.h>

#include	<linux/input/lis3dsh.h>

//Debug Masks +++
#define NO_DEBUG       0
#define DEBUG_POWER     1
#define DEBUG_INFO  2
#define DEBUG_VERBOSE 5
#define DEBUG_RAW      8
#define DEBUG_TRACE   10

static int debug = DEBUG_INFO;

module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "Activate debugging output");

#define sensor_debug(level, ...) \
		if (debug >= (level)) \
			pr_info(__VA_ARGS__);
//Debug Masks ---

/* set to 1 to enable SM program and parameters loading */
/* set to 0 to leave unused */
#define LOAD_SM1_PROGRAM	1
#define LOAD_SP1_PARAMETERS	1
#define LOAD_SM2_PROGRAM	1
#define LOAD_SP2_PARAMETERS	1

#define LIS3DSH_TILT_TO_WAKE		1

#define G_MAX			23920640	/* ug */
#define I2C_RETRY_DELAY		5		/* Waiting for signals [ms] */
#define I2C_RETRIES		5		/* Number of retries */
#define I2C_AUTO_INCREMENT	0x00		/* Autoincrement i2c address */

#define SENSITIVITY_2G		60		/* ug/LSB	*/
#define SENSITIVITY_4G		120		/* ug/LSB	*/
#define SENSITIVITY_6G		180		/* ug/LSB	*/
#define SENSITIVITY_8G		240		/* ug/LSB	*/
#define SENSITIVITY_16G		730		/* ug/LSB	*/

#define LIS3DSH_FS_MASK	(0x38)

/* Output Data Rates ODR */
#define LIS3DSH_ODR_MASK	(0XF0)
#define LIS3DSH_PM_OFF		(0x00)		/* OFF */
#define LIS3DSH_ODR3_125	(0x10)		/*    3.125 Hz */
#define LIS3DSH_ODR6_25		(0x20)		/*    6.25  Hz */
#define LIS3DSH_ODR12_5		(0x30)		/*   12.5   Hz */
#define LIS3DSH_ODR25		(0x40)		/*   25     Hz */
#define LIS3DSH_ODR50		(0x50)		/*   50     Hz */
#define LIS3DSH_ODR100		(0x60)		/*  100     Hz */
#define LIS3DSH_ODR400		(0x70)		/*  400     Hz */
#define LIS3DSH_ODR800		(0x80)		/*  800     Hz */
#define LIS3DSH_ODR1600		(0x90)		/* 1600     Hz */

/* Registers configuration Mask and settings */
/* ----CTRLREGx */
#define LIS3DSH_INTEN_MASK		(0x01)
#define LIS3DSH_INTEN_OFF		(0x00)
#define LIS3DSH_INTEN_ON		(0x01)

/* CTRLREG1 */
#define LIS3DSH_HIST1_MASK		(0xE0)
#define LIS3DSH_SM1INT_PIN_MASK		(0x08)
#define LIS3DSH_SM1INT_PININT2		(0x08)
#define LIS3DSH_SM1INT_PININT1		(0x00)
#define LIS3DSH_SM1_EN_MASK		(0x01)
#define LIS3DSH_SM1_EN_ON		(0x01)
#define LIS3DSH_SM1_EN_OFF		(0x00)
/* */

/* CTRLREG2 */
#define LIS3DSH_HIST2_MASK		(0xE0)
#define LIS3DSH_SM2INT_PIN_MASK		(0x08)
#define LIS3DSH_SM2INT_PININT2		(0x08)
#define LIS3DSH_SM2INT_PININT1		(0x00)
#define LIS3DSH_SM2_EN_MASK		(0x09)
#define LIS3DSH_SM2_EN_ON		(0x09)
#define LIS3DSH_SM2_EN_OFF		(0x00)
/* */

/* CTRLREG3 */
#define LIS3DSH_INT_ACT_MASK		(0x01 << 6)
#define LIS3DSH_INT_ACT_H		(0x01 << 6)
#define LIS3DSH_INT_ACT_L		(0x00)

#define LIS3DSH_INT2_EN_MASK		(0x01 << 4)
#define LIS3DSH_INT2_EN_ON		(0x01 << 4)
#define LIS3DSH_INT2_EN_OFF		(0x00)

#define LIS3DSH_INT1_EN_MASK		(0x01 << 3)
#define LIS3DSH_INT1_EN_ON		(0x01 << 3)
#define LIS3DSH_INT1_EN_OFF		(0x00)
/* */

/* CTRLREG4 */
#define LIS3DSH_BDU_EN			(0x08)
#define LIS3DSH_ALL_AXES		(0x07)
/* */

/* STATUS REG BITS */
#define LIS3DSH_STAT_INTSM1_BIT		(0x01 << 3)
#define LIS3DSH_STAT_INTSM2_BIT		(0x01 << 2)

#define OUT_AXISDATA_REG		LIS3DSH_OUTX_L
#define WHOAMI_LIS3DSH_ACC		(0x3F)	/* Expected content for WAI */

/*	CONTROL REGISTERS	*/
#define LIS3DSH_WHO_AM_I		(0x0F)	/* WhoAmI register Address */

#define LIS3DSH_OUTX_L			(0x28)	/* Output X LSByte */
#define LIS3DSH_OUTX_H			(0x29)	/* Output X MSByte */
#define LIS3DSH_OUTY_L			(0x2A)	/* Output Y LSByte */
#define LIS3DSH_OUTY_H			(0x2B)	/* Output Y MSByte */
#define LIS3DSH_OUTZ_L			(0x2C)	/* Output Z LSByte */
#define LIS3DSH_OUTZ_H			(0x2D)	/* Output Z MSByte */
#define LIS3DSH_LC_L			(0x16)	/* LSByte Long Counter Status */
#define LIS3DSH_LC_H			(0x17)	/* MSByte Long Counter Status */

#define LIS3DSH_INTERR_STAT		(0x18)	/* Interrupt Status */

#define LIS3DSH_STATUS_REG		(0x27)	/* Status */

#define LIS3DSH_CTRL_REG1		(0x21)	/* control reg 1 */
#define LIS3DSH_CTRL_REG2		(0x22)	/* control reg 2 */
#define LIS3DSH_CTRL_REG3		(0x23)	/* control reg 3 */
#define LIS3DSH_CTRL_REG4		(0x20)	/* control reg 4 */
#define LIS3DSH_CTRL_REG5		(0x24)	/* control reg 5 */
#define LIS3DSH_CTRL_REG6		(0x25)	/* control reg 6 */

#define LIS3DSH_OFF_X			(0x10)	/* Offset X Corr */
#define LIS3DSH_OFF_Y			(0x11)	/* Offset Y Corr */
#define LIS3DSH_OFF_Z			(0x12)	/* Offset Z Corr */

#define LIS3DSH_CS_X			(0x13)	/* Const Shift X */
#define LIS3DSH_CS_Y			(0x14)	/* Const Shift Y */
#define LIS3DSH_CS_Z			(0x15)	/* Const Shift Z */

#define LIS3DSH_VFC_1			(0x1B)	/* Vect Filter Coeff 1 */
#define LIS3DSH_VFC_2			(0x1C)	/* Vect Filter Coeff 2 */
#define LIS3DSH_VFC_3			(0x1D)	/* Vect Filter Coeff 3 */
#define LIS3DSH_VFC_4			(0x1E)	/* Vect Filter Coeff 4 */


	/* state program 1 */
#define LIS3DSH_STATEPR1	(0X40)	/*	State Program 1 16 bytes */

#define LIS3DSH_TIM4_1		(0X50)	/*	SPr1 Timer4		*/
#define LIS3DSH_TIM3_1		(0X51)	/*	SPr1 Timer3		*/
#define LIS3DSH_TIM2_1		(0X52)	/*	SPr1 Timer2	2bytes	*/
#define LIS3DSH_TIM1_1		(0X54)	/*	SPr1 Timer1	2bytes	*/

#define LIS3DSH_THRS2_1		(0X56)	/*	SPr1 Threshold1		*/
#define LIS3DSH_THRS1_1		(0X57)	/*	SPr1 Threshold2		*/
#define LIS3DSH_SA_1		(0X59)	/*	SPr1 Swap Axis Sign Msk	*/
#define LIS3DSH_MA_1		(0X5A)	/*	SPr1 Axis Sign Msk	*/
#define LIS3DSH_SETT_1		(0X5B)	/*	SPr1 			*/
#define LIS3DSH_PPRP_1		(0X5C)	/*	SPr1 ProgPointer ResetPointer */
#define LIS3DSH_TC_1		(0X5D)	/*	SPr1 		2bytes	*/
#define LIS3DSH_OUTS_1		(0X5F)	/*	SPr1 			*/

	/* state program 2 */
#define LIS3DSH_STATEPR2	(0X60)	/*	State Program 2 16 bytes */

#define LIS3DSH_TIM4_2		(0X70)	/*	SPr2 Timer4		*/
#define LIS3DSH_TIM3_2		(0X71)	/*	SPr2 Timer3		*/
#define LIS3DSH_TIM2_2		(0X72)	/*	SPr2 Timer2	2bytes	*/
#define LIS3DSH_TIM1_2		(0X74)	/*	SPr2 Timer1	2bytes	*/

#define LIS3DSH_THRS2_2		(0X76)	/*	SPr2 Threshold1		*/
#define LIS3DSH_THRS1_2		(0X77)	/*	SPr2 Threshold2		*/
#define LIS3DSH_DES_2		(0X78)	/*	SPr2 Decimation		*/
#define LIS3DSH_SA_2		(0X79)	/*	SPr2 Swap Axis Sign Msk	*/
#define LIS3DSH_MA_2		(0X7A)	/*	SPr2 Axis Sign Msk	*/
#define LIS3DSH_SETT_2		(0X7B)	/*	SPr2 			*/
#define LIS3DSH_PPRP_2		(0X7C)	/*	SPr2 ProgPointer ResetPointer */
#define LIS3DSH_TC_2		(0X7D)	/*	SPr2 		2bytes	*/
#define LIS3DSH_OUTS_2		(0X7F)	/*	SPr2 			*/
/*	end CONTROL REGISTRES	*/


/* RESUME STATE INDICES */
#define RES_LIS3DSH_LC_L			0
#define RES_LIS3DSH_LC_H			1

#define RES_LIS3DSH_CTRL_REG4			2
#define RES_LIS3DSH_CTRL_REG1			3
#define RES_LIS3DSH_CTRL_REG2			4
#define RES_LIS3DSH_CTRL_REG3			5
#define RES_LIS3DSH_CTRL_REG5			6
#define RES_LIS3DSH_CTRL_REG6			7

#define RES_LIS3DSH_OFF_X			8
#define RES_LIS3DSH_OFF_Y			9
#define RES_LIS3DSH_OFF_Z			10

#define RES_LIS3DSH_CS_X			11
#define RES_LIS3DSH_CS_Y			12
#define RES_LIS3DSH_CS_Z			13

#define RES_LIS3DSH_VFC_1			14
#define RES_LIS3DSH_VFC_2			15
#define RES_LIS3DSH_VFC_3			16
#define RES_LIS3DSH_VFC_4			17

#define RES_LIS3DSH_THRS3			18

#define RES_LIS3DSH_TIM4_1			20
#define RES_LIS3DSH_TIM3_1			21
#define RES_LIS3DSH_TIM2_1_L			22
#define RES_LIS3DSH_TIM2_1_H			23
#define RES_LIS3DSH_TIM1_1_L			24
#define RES_LIS3DSH_TIM1_1_H			25

#define RES_LIS3DSH_THRS2_1			26
#define RES_LIS3DSH_THRS1_1			27
#define RES_LIS3DSH_SA_1			28
#define RES_LIS3DSH_MA_1			29
#define RES_LIS3DSH_SETT_1			30

#define RES_LIS3DSH_TIM4_2			31
#define RES_LIS3DSH_TIM3_2			32
#define RES_LIS3DSH_TIM2_2_L			33
#define RES_LIS3DSH_TIM2_2_H			34
#define RES_LIS3DSH_TIM1_2_L			35
#define RES_LIS3DSH_TIM1_2_H			36

#define RES_LIS3DSH_THRS2_2			37
#define RES_LIS3DSH_THRS1_2			38
#define RES_LIS3DSH_DES_2			39
#define RES_LIS3DSH_SA_2			40
#define RES_LIS3DSH_MA_2			41
#define RES_LIS3DSH_SETT_2			42

#define LIS3DSH_RESUME_ENTRIES			43



#define LIS3DSH_STATE_PR_SIZE			16
/* end RESUME STATE INDICES */

/* STATE PROGRAMS ENABLE CONTROLS */
#define LIS3DSH_SM1_DIS_SM2_DIS			(0x00)
#define LIS3DSH_SM1_EN_SM2_DIS			(0x01)
#define LIS3DSH_SM1_DIS_SM2_EN			(0x02)
#define LIS3DSH_SM1_EN_SM2_EN			(0x03)

/* INTERRUPTS ENABLE CONTROLS */
#define LIS3DSH_INT1_DIS_INT2_DIS		(0x00)
#define LIS3DSH_INT1_EN_INT2_DIS		(0x01)
#define LIS3DSH_INT1_DIS_INT2_EN		(0x02)
#define LIS3DSH_INT1_EN_INT2_EN			(0x03)

struct {
	unsigned int cutoff_ms;
	unsigned int mask;
} lis3dsh_acc_odr_table[] = {
		{    1, LIS3DSH_ODR1600 },
		{    3, LIS3DSH_ODR400  },
		{   10, LIS3DSH_ODR100  },
		{   20, LIS3DSH_ODR50   },
		{   40, LIS3DSH_ODR25   },
		{   80, LIS3DSH_ODR12_5 },
		{  160, LIS3DSH_ODR6_25 },
		{  320, LIS3DSH_ODR3_125},
};

static struct lis3dsh_acc_platform_data default_lis3dsh_acc_pdata = {
	.fs_range = LIS3DSH_ACC_G_2G,
	.axis_map_x = 0,
	.axis_map_y = 1,
	.axis_map_z = 2,
	.negate_x = 0,
	.negate_y = 0,
	.negate_z = 0,
	.poll_interval = 3,
	.min_interval = LIS3DSH_ACC_MIN_POLL_PERIOD_MS,
	.gpio_int1 = LIS3DSH_ACC_DEFAULT_INT1_GPIO,
	.gpio_int2 = LIS3DSH_ACC_DEFAULT_INT2_GPIO,
};

static int int1_gpio = LIS3DSH_ACC_DEFAULT_INT1_GPIO;
static int int2_gpio = LIS3DSH_ACC_DEFAULT_INT2_GPIO;
module_param(int1_gpio, int, S_IRUGO);
module_param(int2_gpio, int, S_IRUGO);
MODULE_PARM_DESC(int1_gpio, "integer: gpio number being assined to interrupt PIN1");
MODULE_PARM_DESC(int2_gpio, "integer: gpio number being assined to interrupt PIN2");

static struct sensors_classdev lis3dsh_cdev = {
	.name = "lis3dsh",
	.vendor = "STMicroelectronics",
	.version = 2,
	.enabled = 0,
	.sensors_enable = NULL,
};

struct lis3dsh_acc_data {
	struct sensors_classdev cdev;
	struct i2c_client *client;
	struct lis3dsh_acc_platform_data *pdata;

	struct mutex lock;

	struct input_dev *input_dev;

	int hw_initialized;
	/* hw_working=-1 means not tested yet */
	int hw_working;
	atomic_t enabled;
	int use_smbus;

	u16 sensitivity;
	u8 stateprogs_enable_setting;

	u8 resume_state[LIS3DSH_RESUME_ENTRIES];
	u8 resume_stmach_program1[LIS3DSH_STATE_PR_SIZE];
	u8 resume_stmach_program2[LIS3DSH_STATE_PR_SIZE];

	int irq1;
	struct work_struct irq1_work;
	struct workqueue_struct *irq1_work_queue;
	int irq2;
	struct work_struct irq2_work;
	struct workqueue_struct *irq2_work_queue;
	
	struct wake_lock tilt_wakelock;
};
static struct lis3dsh_acc_data *sensor_data;

static int chip_status=0;			//ASUS_BSP +++ Maggie_Lee "Support ATD BMMI"

static void lis3dsh_acc_report_tilt(struct lis3dsh_acc_data *acc, int val);
static int lis3dsh_acc_get_acceleration_data(struct lis3dsh_acc_data *acc, int *xyz);
static int lis3dsh_acc_state_progrs_enable_control(struct lis3dsh_acc_data *acc, u8 settings);

/* sets default init values to be written in registers at probe stage */
static void lis3dsh_acc_set_init_register_values(struct lis3dsh_acc_data *acc)
{
	acc->resume_state[RES_LIS3DSH_LC_L] = 0x00;
	acc->resume_state[RES_LIS3DSH_LC_H] = 0x00;

	acc->resume_state[RES_LIS3DSH_CTRL_REG1] = (0x00 | LIS3DSH_SM1INT_PININT1);
	acc->resume_state[RES_LIS3DSH_CTRL_REG2] = (0x00 | LIS3DSH_SM2INT_PININT1);
	acc->resume_state[RES_LIS3DSH_CTRL_REG3] = (0x04 | LIS3DSH_INT_ACT_H);
	if(acc->pdata->gpio_int1 >= 0)
		acc->resume_state[RES_LIS3DSH_CTRL_REG3] =
				acc->resume_state[RES_LIS3DSH_CTRL_REG3] | \
					LIS3DSH_INT1_EN_ON;
	if(acc->pdata->gpio_int2 >= 0)
		acc->resume_state[RES_LIS3DSH_CTRL_REG3] =
				acc->resume_state[RES_LIS3DSH_CTRL_REG3] | \
					LIS3DSH_INT2_EN_ON;

	acc->resume_state[RES_LIS3DSH_CTRL_REG4] = (LIS3DSH_ALL_AXES | LIS3DSH_ODR400);
	acc->resume_state[RES_LIS3DSH_CTRL_REG5] = 0xC0;
	acc->resume_state[RES_LIS3DSH_CTRL_REG6] = 0x10;

	acc->resume_state[RES_LIS3DSH_THRS3] = 0x00;
	acc->resume_state[RES_LIS3DSH_OFF_X] = 0x00;
	acc->resume_state[RES_LIS3DSH_OFF_Y] = 0x00;
	acc->resume_state[RES_LIS3DSH_OFF_Z] = 0x00;

	acc->resume_state[RES_LIS3DSH_CS_X] = 0x00;
	acc->resume_state[RES_LIS3DSH_CS_Y] = 0x00;
	acc->resume_state[RES_LIS3DSH_CS_Z] = 0x00;

	acc->resume_state[RES_LIS3DSH_VFC_1] = 0x7D;
	acc->resume_state[RES_LIS3DSH_VFC_2] = 0x40;
	acc->resume_state[RES_LIS3DSH_VFC_3] = 0x20;
	acc->resume_state[RES_LIS3DSH_VFC_4] = 0x10;
}

static void lis3dsh_acc_set_init_statepr1_inst(struct lis3dsh_acc_data *acc, int sel)
{
	/* Place here state machine 1 program */
	/* Tilt to wake function Part 1(When watch face is flat)*/
	acc->resume_stmach_program1[0] = 0x09;
	acc->resume_stmach_program1[1] = 0x71;
	acc->resume_stmach_program1[2] = 0x88;
	acc->resume_stmach_program1[3] = 0xFF;
	acc->resume_stmach_program1[4] = 0x11;
	acc->resume_stmach_program1[5] = 0x00;
	acc->resume_stmach_program1[6] = 0x00;
	acc->resume_stmach_program1[7] = 0x00;
	acc->resume_stmach_program1[8] = 0x00;
	acc->resume_stmach_program1[9] = 0x00;
	acc->resume_stmach_program1[10] = 0x00;
	acc->resume_stmach_program1[11] = 0x00;
	acc->resume_stmach_program1[12] = 0x00;
	acc->resume_stmach_program1[13] = 0x00;
	acc->resume_stmach_program1[14] = 0x00;
	acc->resume_stmach_program1[15] = 0x00;
}

static void lis3dsh_acc_set_init_statepr2_inst(struct lis3dsh_acc_data *acc)
{
	/* Place here state machine 2 program */
	/* Tilt to wake function Part 2(When watch face is towards user)*/
	acc->resume_stmach_program2[0] = 0xFF;
	acc->resume_stmach_program2[1] = 0x33;
	acc->resume_stmach_program2[2] = 0x08;
	acc->resume_stmach_program2[3] = 0x62;
	acc->resume_stmach_program2[4] = 0x88;
	acc->resume_stmach_program2[5] = 0x44;
	acc->resume_stmach_program2[6] = 0x11;
	acc->resume_stmach_program2[7] = 0x00;
	acc->resume_stmach_program2[8] = 0x00;
	acc->resume_stmach_program2[9] = 0x00;
	acc->resume_stmach_program2[10] = 0x00;
	acc->resume_stmach_program2[11] = 0x00;
	acc->resume_stmach_program2[12] = 0x00;
	acc->resume_stmach_program2[13] = 0x00;
	acc->resume_stmach_program2[14] = 0x00;
	acc->resume_stmach_program2[15] = 0x00;
}

static void lis3dsh_acc_set_init_statepr1_param(struct lis3dsh_acc_data *acc, int sel)
{
	/* Place here state machine 1 parameters */
	/* Tilt to wake function Part 1(When watch face is flat)*/
	acc->resume_state[RES_LIS3DSH_TIM4_1] = 0x00;
	acc->resume_state[RES_LIS3DSH_TIM3_1] = 0x00;
	acc->resume_state[RES_LIS3DSH_TIM2_1_L] = 0x32;
	acc->resume_state[RES_LIS3DSH_TIM2_1_H] = 0x00;
	acc->resume_state[RES_LIS3DSH_TIM1_1_L] = 0x78;
	acc->resume_state[RES_LIS3DSH_TIM1_1_H] = 0x00;
	acc->resume_state[RES_LIS3DSH_THRS2_1] = 0xEA;		//tilt angle
	acc->resume_state[RES_LIS3DSH_THRS1_1] = 0xFF;		//flat (return) angle
	/* DES1 not available*/
	acc->resume_state[RES_LIS3DSH_SA_1] = 0x80;
	acc->resume_state[RES_LIS3DSH_MA_1] = 0x80;
	acc->resume_state[RES_LIS3DSH_SETT_1] = 0x20;
}

static void lis3dsh_acc_set_init_statepr2_param(struct lis3dsh_acc_data *acc)
{
	/* Place here state machine 2 parameters */
	/* Tilt to wake function Part 2(When watch face is towards user)*/
	acc->resume_state[RES_LIS3DSH_TIM4_2] = 0x00;
	acc->resume_state[RES_LIS3DSH_TIM3_2] = 0x00;
	acc->resume_state[RES_LIS3DSH_TIM2_2_L] = 0x32;
	acc->resume_state[RES_LIS3DSH_TIM2_2_H] = 0x00;
	acc->resume_state[RES_LIS3DSH_TIM1_2_L] = 0x78;
	acc->resume_state[RES_LIS3DSH_TIM1_2_H] = 0x00;
	acc->resume_state[RES_LIS3DSH_THRS2_2] = 0xEA;
	acc->resume_state[RES_LIS3DSH_THRS1_2] = 0xFF;
	acc->resume_state[RES_LIS3DSH_DES_2] = 0x00;
	acc->resume_state[RES_LIS3DSH_SA_2] = 0x80;
	acc->resume_state[RES_LIS3DSH_MA_2] = 0x80;
	acc->resume_state[RES_LIS3DSH_SETT_2] = 0x20;
}

static int lis3dsh_acc_i2c_read(struct lis3dsh_acc_data *acc,
				u8 * buf, int len)
{
	int err;
	int tries = 0;

	struct i2c_msg	msgs[] = {
		{
			.addr = acc->client->addr,
			.flags = acc->client->flags & I2C_M_TEN,
			.len = 1,
			.buf = buf,
		},
		{
			.addr = acc->client->addr,
			.flags = (acc->client->flags & I2C_M_TEN) | I2C_M_RD,
			.len = len,
			.buf = buf,
		},
	};

	do {
		err = i2c_transfer(acc->client->adapter, msgs, 2);
		if (err != 2)
			msleep_interruptible(I2C_RETRY_DELAY);
	} while ((err != 2) && (++tries < I2C_RETRIES));

	if (err != 2) {
		dev_err(&acc->client->dev, "read transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

static int lis3dsh_acc_i2c_write(struct lis3dsh_acc_data *acc, u8 * buf,
								int len)
{
	int err;
	int tries = 0;

	struct i2c_msg msgs[] = {
		{
		 .addr = acc->client->addr,
			.flags = acc->client->flags & I2C_M_TEN,
		 .len = len + 1,
		 .buf = buf,
		 },
	};

	do {
		err = i2c_transfer(acc->client->adapter, msgs, 1);
		if (err != 1)
			msleep_interruptible(I2C_RETRY_DELAY);
	} while ((err != 1) && (++tries < I2C_RETRIES));

	if (err != 1) {
		dev_err(&acc->client->dev, "write transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

static int lis3dsh_acc_i2c_update(struct lis3dsh_acc_data *acc,
				u8 reg_address, u8 mask, u8 new_bit_values)
{
	int err = -1;
	u8 rdbuf[1] = { reg_address };
	u8 wrbuf[2] = { reg_address , 0x00 };

	u8 init_val;
	u8 updated_val;
	err = lis3dsh_acc_i2c_read(acc, rdbuf, 1);
	if (!(err < 0)) {
		init_val = rdbuf[0];
		updated_val = ((mask & new_bit_values) | ((~mask) & init_val));
		wrbuf[1] = updated_val;
		err = lis3dsh_acc_i2c_write(acc, wrbuf, 1);
	}
	return err;
}

static int lis3dsh_acc_hw_init(struct lis3dsh_acc_data *acc)
{
	int i;
	int err = -1;
	u8 buf[17];

	sensor_debug(DEBUG_VERBOSE, "[lis3dsh] %s: +++\n", __func__);

	buf[0] = LIS3DSH_WHO_AM_I;
	err = lis3dsh_acc_i2c_read(acc, buf, 1);
	if (err < 0) {
	dev_warn(&acc->client->dev, "Error reading WHO_AM_I: is device "
		"available/working?\n");
		goto err_firstread;
	} else
		acc->hw_working = 1;

	if (buf[0] != WHOAMI_LIS3DSH_ACC) {
	dev_err(&acc->client->dev,
		"device unknown. Expected: 0x%02x,"
		" Replies: 0x%02x\n", WHOAMI_LIS3DSH_ACC, buf[0]);
		err = -1; /* choose the right coded error */
		goto err_unknown_device;
	}


	buf[0] = (I2C_AUTO_INCREMENT | LIS3DSH_LC_L);
	buf[1] = acc->resume_state[RES_LIS3DSH_LC_L];
	buf[2] = acc->resume_state[RES_LIS3DSH_LC_H];
	err = lis3dsh_acc_i2c_write(acc, buf, 2);
	if (err < 0)
		goto err_resume_state;

	buf[0] = (I2C_AUTO_INCREMENT | LIS3DSH_VFC_1);
	buf[1] = acc->resume_state[RES_LIS3DSH_VFC_1];
	buf[2] = acc->resume_state[RES_LIS3DSH_VFC_2];
	buf[3] = acc->resume_state[RES_LIS3DSH_VFC_3];
	buf[4] = acc->resume_state[RES_LIS3DSH_VFC_4];
	err = lis3dsh_acc_i2c_write(acc, buf, 4);
	if (err < 0)
		goto err_resume_state;

	buf[0] = (I2C_AUTO_INCREMENT | LIS3DSH_TIM4_1);
	buf[1] = acc->resume_state[RES_LIS3DSH_TIM4_1];
	buf[2] = acc->resume_state[RES_LIS3DSH_TIM3_1];
	buf[3] = acc->resume_state[RES_LIS3DSH_TIM2_1_L];
	buf[4] = acc->resume_state[RES_LIS3DSH_TIM2_1_H];
	buf[5] = acc->resume_state[RES_LIS3DSH_TIM1_1_L];
	buf[6] = acc->resume_state[RES_LIS3DSH_TIM1_1_H];
	buf[7] = acc->resume_state[RES_LIS3DSH_THRS2_1];
	buf[8] = acc->resume_state[RES_LIS3DSH_THRS1_1];
	err = lis3dsh_acc_i2c_write(acc, buf, 8);
	if (err < 0)
		goto err_resume_state;

	buf[0] = (I2C_AUTO_INCREMENT | LIS3DSH_SA_1);
	buf[1] = acc->resume_state[RES_LIS3DSH_SA_1];
	buf[2] = acc->resume_state[RES_LIS3DSH_MA_1];
	buf[3] = acc->resume_state[RES_LIS3DSH_SETT_1];
	err = lis3dsh_acc_i2c_write(acc, buf, 3);
	if (err < 0)
		goto err_resume_state;

	buf[0] = (I2C_AUTO_INCREMENT | LIS3DSH_TIM4_2);
	buf[1] = acc->resume_state[RES_LIS3DSH_TIM4_2];
	buf[2] = acc->resume_state[RES_LIS3DSH_TIM3_2];
	buf[3] = acc->resume_state[RES_LIS3DSH_TIM2_2_L];
	buf[4] = acc->resume_state[RES_LIS3DSH_TIM2_2_H];
	buf[5] = acc->resume_state[RES_LIS3DSH_TIM1_2_L];
	buf[6] = acc->resume_state[RES_LIS3DSH_TIM1_2_H];
	buf[7] = acc->resume_state[RES_LIS3DSH_THRS2_2];
	buf[8] = acc->resume_state[RES_LIS3DSH_THRS1_2];
	buf[9] = acc->resume_state[RES_LIS3DSH_DES_2];
	buf[10] = acc->resume_state[RES_LIS3DSH_SA_2];
	buf[11] = acc->resume_state[RES_LIS3DSH_MA_2];
	buf[12] = acc->resume_state[RES_LIS3DSH_SETT_2];
	err = lis3dsh_acc_i2c_write(acc, buf, 12);
	if (err < 0)
		goto err_resume_state;

	/*	state program 1 */
	buf[0] = (I2C_AUTO_INCREMENT | LIS3DSH_STATEPR1);
	for (i = 1; i <= LIS3DSH_STATE_PR_SIZE; i++) {
		buf[i] = acc->resume_stmach_program1[i-1];
		pr_debug("[lis3dsh] i=%d,sm pr1 buf[%d]=0x%02x\n", i, i, buf[i]);
	};
	err = lis3dsh_acc_i2c_write(acc, buf, LIS3DSH_STATE_PR_SIZE);
	if (err < 0)
		goto err_resume_state;

	/*	state program 2 */
	buf[0] = (I2C_AUTO_INCREMENT | LIS3DSH_STATEPR2);
	for(i = 1; i <= LIS3DSH_STATE_PR_SIZE; i++){
		buf[i] = acc->resume_stmach_program2[i-1];
		pr_debug("[lis3dsh] i=%d,sm pr2 buf[%d]=0x%02x\n", i, i, buf[i]);
	};
	err = lis3dsh_acc_i2c_write(acc, buf, LIS3DSH_STATE_PR_SIZE);
	if (err < 0)
		goto err_resume_state;

	buf[0] = (I2C_AUTO_INCREMENT | LIS3DSH_CTRL_REG5);
	buf[1] = acc->resume_state[RES_LIS3DSH_CTRL_REG5];
	buf[2] = acc->resume_state[RES_LIS3DSH_CTRL_REG6];
	err = lis3dsh_acc_i2c_write(acc, buf, 2);
	if (err < 0)
		goto err_resume_state;

	buf[0] = (I2C_AUTO_INCREMENT | LIS3DSH_CTRL_REG1);
	buf[1] = acc->resume_state[RES_LIS3DSH_CTRL_REG1];
	buf[2] = acc->resume_state[RES_LIS3DSH_CTRL_REG2];
	buf[3] = acc->resume_state[RES_LIS3DSH_CTRL_REG3];
	err = lis3dsh_acc_i2c_write(acc, buf, 3);
	if (err < 0)
		goto err_resume_state;

	buf[0] = (LIS3DSH_CTRL_REG4);
	buf[1] = acc->resume_state[RES_LIS3DSH_CTRL_REG4];
	err = lis3dsh_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		goto err_resume_state;

	acc->hw_initialized = 1;
	sensor_debug(DEBUG_VERBOSE, "[lis3dsh] %s: ---\n", __func__);
	return 0;

err_firstread:
	acc->hw_working = 0;
err_unknown_device:
err_resume_state:
	acc->hw_initialized = 0;
	dev_err(&acc->client->dev, "hw init error 0x%02x,0x%02x: %d\n", buf[0],
			buf[1], err);
	return err;
}

static void lis3dsh_acc_device_power_off(struct lis3dsh_acc_data *acc)
{
	int err;

	if (acc == NULL)
			return;

	if (acc->pdata == NULL)
			return;

	err = lis3dsh_acc_i2c_update(acc, LIS3DSH_CTRL_REG4,
					LIS3DSH_ODR_MASK, LIS3DSH_PM_OFF);
	if (err < 0)
		dev_err(&acc->client->dev, "soft power off failed: %d\n", err);

	if (acc->pdata->power_off) {
		if(acc->pdata->gpio_int1)
			disable_irq_nosync(acc->irq1);
		if(acc->pdata->gpio_int2)
			disable_irq_nosync(acc->irq2);
		acc->pdata->power_off();
		acc->hw_initialized = 0;
		sensor_debug(DEBUG_INFO, "[Sensors] %s : powered off\n", __func__);
	}
	if (acc->hw_initialized) {
		if(acc->pdata->gpio_int1 >= 0)
			disable_irq_nosync(acc->irq1);
		if(acc->pdata->gpio_int2 >= 0)
			disable_irq_nosync(acc->irq2);
		acc->hw_initialized = 0;
	}
}

static int lis3dsh_acc_device_power_on(struct lis3dsh_acc_data *acc)
{
	int err = -1;

	if (acc == NULL)
			return err;

	if (acc->pdata == NULL)
			return err;

	if (acc->pdata->power_on) {
		err = acc->pdata->power_on();
		if (err < 0) {
			dev_err(&acc->client->dev, "power_on failed: %d\n", err);
			return err;
		}
		if(acc->pdata->gpio_int1 >= 0)
			enable_irq(acc->irq1);
		if(acc->pdata->gpio_int2 >= 0)
			enable_irq(acc->irq2);
		sensor_debug(DEBUG_INFO, "[Sensors] %s : powered on\n", __func__);
	}

	if (!acc->hw_initialized) {
		err = lis3dsh_acc_hw_init(acc);
		if (acc->hw_working == 1 && err < 0) {
			lis3dsh_acc_device_power_off(acc);
			return err;
		}
	}

	if (acc->hw_initialized) {
		if(acc->pdata->gpio_int1 >= 0)
			enable_irq(acc->irq1);
		if(acc->pdata->gpio_int2 >= 0)
			enable_irq(acc->irq2);
	}
	return 0;
}

static irqreturn_t lis3dsh_acc_isr1(int irq, void *dev)
{
	struct lis3dsh_acc_data *acc = dev;

	disable_irq_nosync(irq);
	queue_work(acc->irq1_work_queue, &acc->irq1_work);

	return IRQ_HANDLED;
}

static irqreturn_t lis3dsh_acc_isr2(int irq, void *dev)
{
	struct lis3dsh_acc_data *acc = dev;

	disable_irq_nosync(irq);
	queue_work(acc->irq2_work_queue, &acc->irq2_work);

	return IRQ_HANDLED;
}

static void lis3dsh_acc_irq1_work_func(struct work_struct *work)
{
	int err = -1;
	u8 rbuf[2], status;
	int xyz[3] = { 0 };
	struct lis3dsh_acc_data *acc;

	acc = container_of(work, struct lis3dsh_acc_data, irq1_work);
	sensor_debug(DEBUG_INFO, "[lis3dsh] %s: IRQ1 triggered\n", __func__);
	
    err = lis3dsh_acc_get_acceleration_data(acc, xyz);
	rbuf[0] = LIS3DSH_INTERR_STAT;
	err = lis3dsh_acc_i2c_read(acc, rbuf, 1);
	status = rbuf[0];
	if(status & LIS3DSH_STAT_INTSM1_BIT) {
		rbuf[0] = LIS3DSH_OUTS_1;
		err = lis3dsh_acc_i2c_read(acc, rbuf, 1);
		sensor_debug(DEBUG_INFO, "[lis3dsh] %s: interrupt (0x%02x)\n", __func__, rbuf[0]);
		if((rbuf[0] == 0x80) && (xyz[2] < 0)) {       //only trigger wake when watch face is facing upwards
			sensor_debug(DEBUG_INFO, "[lis3dsh] %s: tilt down\n", __func__);
			//lis3dsh_acc_report_tilt(acc, 0);
		}
	}
	enable_irq(acc->irq1);
	sensor_debug(DEBUG_VERBOSE, "[lis3dsh] %s: IRQ1 re-enabled\n", __func__);
}

static void lis3dsh_acc_irq2_work_func(struct work_struct *work)
{
	int err = -1;
	u8 rbuf[2], status;
	int xyz[3] = { 0 };
	struct lis3dsh_acc_data *acc;

	acc = container_of(work, struct lis3dsh_acc_data, irq2_work);
	sensor_debug(DEBUG_INFO, "[lis3dsh] %s: IRQ2 triggered\n", __func__);
	/*  */
    err = lis3dsh_acc_get_acceleration_data(acc, xyz);
	rbuf[0] = LIS3DSH_INTERR_STAT;
	err = lis3dsh_acc_i2c_read(acc, rbuf, 1);
	status = rbuf[0];
	if(status & LIS3DSH_STAT_INTSM2_BIT) {
		rbuf[0] = LIS3DSH_OUTS_2;
		err = lis3dsh_acc_i2c_read(acc, rbuf, 1);
		sensor_debug(DEBUG_INFO, "[lis3dsh] %s: interrupt (0x%02x)\n", __func__, rbuf[0]);
		if((rbuf[0] == 0x80) && (xyz[2] < 0)) {       //only trigger wake when watch face is facing upwards
			wake_lock_timeout(&acc->tilt_wakelock, (HZ/2));		//keep system awake to ensure tilt event receiver can obtain sensor event
			sensor_debug(DEBUG_INFO, "[lis3dsh] %s: tilt up\n", __func__);
			lis3dsh_acc_report_tilt(acc, 1);
		}
	}
	enable_irq(acc->irq2);
	sensor_debug(DEBUG_VERBOSE, "[lis3dsh] %s: IRQ2 re-enabled\n", __func__);
}

static int lis3dsh_acc_register_masked_update(struct lis3dsh_acc_data *acc,
		u8 reg_address, u8 mask, u8 new_bit_values, int resume_index)
{
	u8 config[2] = {0};
	u8 init_val, updated_val;
	int err;
	int step = 0;

	config[0] = reg_address;
	err = lis3dsh_acc_i2c_read(acc, config, 1);
	if (err < 0)
		goto error;
	init_val = config[0];
	acc->resume_state[resume_index] = init_val;
	step = 1;
	updated_val = ((mask & new_bit_values) | ((~mask) & init_val));
	config[0] = reg_address;
	config[1] = updated_val;
	err = lis3dsh_acc_i2c_write(acc, config, 1);
	if (err < 0)
		goto error;
	acc->resume_state[resume_index] = updated_val;

	return err;
	error:
		dev_err(&acc->client->dev,
			"register 0x%02x update failed at step %d, error: %d\n",
				config[0], step, err);
	return err;
}

static int lis3dsh_acc_update_fs_range(struct lis3dsh_acc_data *acc,
								u8 new_fs_range)
{
	int err=-1;
	u16 sensitivity;

	switch (new_fs_range) {
	case LIS3DSH_ACC_G_2G:
		sensitivity = SENSITIVITY_2G;
		break;
	case LIS3DSH_ACC_G_4G:
		sensitivity = SENSITIVITY_4G;
		break;
	case LIS3DSH_ACC_G_6G:
		sensitivity = SENSITIVITY_6G;
		break;
	case LIS3DSH_ACC_G_8G:
		sensitivity = SENSITIVITY_8G;
		break;
	case LIS3DSH_ACC_G_16G:
		sensitivity = SENSITIVITY_16G;
		break;
	default:
		dev_err(&acc->client->dev, "invalid g range requested: %u\n",
				new_fs_range);
		return -EINVAL;
	}

	if (atomic_read(&acc->enabled)) {
		/* Updates configuration register 1,
		* which contains g range setting */
		err = lis3dsh_acc_register_masked_update(acc, LIS3DSH_CTRL_REG5,
		LIS3DSH_FS_MASK, new_fs_range, RES_LIS3DSH_CTRL_REG5);
		if(err < 0) {
			dev_err(&acc->client->dev, "update g range failed\n");
			return err;
		}
		else
			acc->sensitivity = sensitivity;
	}

	if(err < 0)
		dev_err(&acc->client->dev, "update g range not executed "
						"because the device is off\n");
	return err;
}


static int lis3dsh_acc_update_odr(struct lis3dsh_acc_data *acc,
							int poll_interval_ms)
{
	int err = -1;
	int i;
	u8 new_odr;

	sensor_debug(DEBUG_VERBOSE, "[Sensors] %s : ODR=%d\n", __func__, poll_interval_ms);

	/* Following, looks for the longest possible odr interval scrolling the
	 * odr_table vector from the end (shortest interval) backward (longest
	 * interval), to support the poll_interval requested by the system.
	 * It must be the longest interval lower then the poll interval.*/
	for (i = ARRAY_SIZE(lis3dsh_acc_odr_table) - 1; i >= 0; i--) {
		if (lis3dsh_acc_odr_table[i].cutoff_ms <= poll_interval_ms)
			break;
	}
	new_odr = lis3dsh_acc_odr_table[i].mask;

	/* If device is currently enabled, we need to write new
	 *  configuration out to it */
	if (atomic_read(&acc->enabled)) {
		err = lis3dsh_acc_register_masked_update(acc,
			LIS3DSH_CTRL_REG4, LIS3DSH_ODR_MASK, new_odr,
							RES_LIS3DSH_CTRL_REG4);
	}

	if(err < 0)
		dev_err(&acc->client->dev, "update odr failed\n");
	return err;
}


#ifdef DEBUG
static int lis3dsh_acc_register_write(struct lis3dsh_acc_data *acc, u8 *buf,
		u8 reg_address, u8 new_value)
{
	int err = -1;

	/* Sets configuration register at reg_address
	 *  NOTE: this is a straight overwrite  */
		buf[0] = reg_address;
		buf[1] = new_value;
		err = lis3dsh_acc_i2c_write(acc, buf, 1);
		if (err < 0)
			return err;
	return err;
}

static int lis3dsh_acc_register_read(struct lis3dsh_acc_data *acc, u8 *buf,
		u8 reg_address)
{

	int err = -1;
	buf[0] = (reg_address);
	err = lis3dsh_acc_i2c_read(acc, buf, 1);
	return err;
}

static int lis3dsh_acc_register_update(struct lis3dsh_acc_data *acc, u8 *buf,
		u8 reg_address, u8 mask, u8 new_bit_values)
{
	int err = -1;
	u8 init_val;
	u8 updated_val;
	err = lis3dsh_acc_register_read(acc, buf, reg_address);
	if (!(err < 0)) {
		init_val = buf[0];
		updated_val = ((mask & new_bit_values) | ((~mask) & init_val));
		err = lis3dsh_acc_register_write(acc, buf, reg_address,
				updated_val);
	}
	return err;
}
#endif


static int lis3dsh_acc_get_acceleration_data(struct lis3dsh_acc_data *acc,
		int *xyz)
{
	int err = -1;
	/* Data bytes from hardware xL, xH, yL, yH, zL, zH */
	u8 acc_data[6];
	/* x,y,z hardware data */
	s32 hw_d[3] = { 0 };

	acc_data[0] = (I2C_AUTO_INCREMENT | OUT_AXISDATA_REG);
	err = lis3dsh_acc_i2c_read(acc, acc_data, 6);
	if (err < 0)
		return err;

	hw_d[0] = ((s16) ((acc_data[1] << 8) | acc_data[0]));
	hw_d[1] = ((s16) ((acc_data[3] << 8) | acc_data[2]));
	hw_d[2] = ((s16) ((acc_data[5] << 8) | acc_data[4]));

	hw_d[0] = hw_d[0] * acc->sensitivity / 1000;
	hw_d[1] = hw_d[1] * acc->sensitivity / 1000;
	hw_d[2] = hw_d[2] * acc->sensitivity / 1000;


	xyz[0] = ((acc->pdata->negate_x) ? (-hw_d[acc->pdata->axis_map_x])
		   : (hw_d[acc->pdata->axis_map_x]));
	xyz[1] = ((acc->pdata->negate_y) ? (-hw_d[acc->pdata->axis_map_y])
		   : (hw_d[acc->pdata->axis_map_y]));
	xyz[2] = ((acc->pdata->negate_z) ? (-hw_d[acc->pdata->axis_map_z])
		   : (hw_d[acc->pdata->axis_map_z]));

	sensor_debug(DEBUG_RAW, "[lis3dsh] %s read x=%d, y=%d, z=%d\n", __func__, xyz[0], xyz[1], xyz[2]);

	return err;
}

static void lis3dsh_acc_report_tilt(struct lis3dsh_acc_data *acc, int val)
{
	input_event(acc->input_dev, EV_MSC, MSC_GESTURE, val);
	input_sync(acc->input_dev);
	sensor_debug(DEBUG_INFO, "[lis3dsh] %s : (%d)\n", __func__, val);
}

static int lis3dsh_acc_enable(struct sensors_classdev *sensors_cdev, unsigned int enable)
{
	int err;

	sensor_debug(DEBUG_INFO, "[Sensors] %s +++ %s\n", __func__, enable?"ON":"OFF");
	if(enable && !atomic_cmpxchg(&sensor_data->enabled, 0, 1)) {
		err = lis3dsh_acc_device_power_on(sensor_data);
		if (err < 0) {
			atomic_set(&sensor_data->enabled, 0);
			return err;
		}
		lis3dsh_acc_state_progrs_enable_control(sensor_data, LIS3DSH_SM1_EN_SM2_EN);
		sensor_data->cdev.enabled = 1;
	}
	else if (!enable && atomic_cmpxchg(&sensor_data->enabled, 1, 0)) {
		lis3dsh_acc_device_power_off(sensor_data);
		sensor_data->cdev.enabled = 0;
	}
	sensor_debug(DEBUG_INFO, "[Sensors] %s --- %s\n", __func__, enable?"ON":"OFF");

	return 0;
}

static void lis3dsh_acc_reg_dump(struct lis3dsh_acc_data *acc)
{
	int rc;
	u8 dbuf[0];
	int i;

	for(i=15;i<128; i++) {
		if (i==38) i++;
		if (i==48) i+=32;
		dbuf[0] = (u8)i;
		printk("Reg = 0x%02x\t", dbuf[0]);
		rc = lis3dsh_acc_i2c_read(acc, dbuf, 1);
		if (rc < 0)
			goto error;
		printk("Value = 0x%02x\n", dbuf[0]);
	}

	return;

	error:
		dev_err(&acc->client->dev, "Error reading ECG register\n");
	
}

static ssize_t attr_get_enable(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct lis3dsh_acc_data *acc = dev_get_drvdata(dev);
	int val = atomic_read(&acc->enabled);
	return sprintf(buf, "%d\n", val);
}

static ssize_t attr_set_enable(struct device *dev,
				struct device_attribute *attr,
						const char *buf, size_t size)
{
	unsigned long val;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	lis3dsh_acc_enable(&sensor_data->cdev, val);

	return size;
}
static DEVICE_ATTR(enable, S_IWUSR|S_IWGRP|S_IRUGO, attr_get_enable, attr_set_enable);

static ssize_t attr_get_acc_data(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct lis3dsh_acc_data *acc = dev_get_drvdata(dev);
	int xyz[3] = { 0 };
	int err;
	
	err =lis3dsh_acc_device_power_on(acc);
	if (err < 0)
		dev_err(&acc->client->dev, "power_on_sensor failed\n");
	
	err = lis3dsh_acc_get_acceleration_data(acc, xyz);
	if (err < 0)
		dev_err(&acc->client->dev, "get_acceleration_data failed\n");
	
	lis3dsh_acc_device_power_off(acc);

	return err;
}
static DEVICE_ATTR(acc_data, S_IRUGO, attr_get_acc_data, NULL);

static ssize_t attr_reg_dump(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct lis3dsh_acc_data *acc = dev_get_drvdata(dev);
	lis3dsh_acc_reg_dump(acc);
	return sprintf(buf, "DONE\n");
}
static DEVICE_ATTR(dump, S_IRUGO, attr_reg_dump, NULL);

#ifndef ASUS_USER_BUILD
//ASUS_BSP +++ Maggie_Lee "Support ATD BMMI"
static ssize_t attr_get_chip_status(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	sensor_debug(DEBUG_INFO, "[lis3dsh] %s: read_accel_status = %d", __func__, chip_status);
	return sprintf(buf, "%d\n", chip_status);
}
static DEVICE_ATTR(chip_status, S_IRUGO, attr_get_chip_status, NULL);
//ASUS_BSP --- Maggie_Lee "Support ATD BMMI"
#endif

static struct attribute *lis3dsh_attributes[] = {
	&dev_attr_enable.attr,
	&dev_attr_acc_data.attr,
	&dev_attr_dump.attr,
#ifndef ASUS_USER_BUILD
	&dev_attr_chip_status.attr,
#endif
	NULL
};

static const struct attribute_group lis3dsh_attr_group = {
	.attrs = lis3dsh_attributes,
};

static int lis3dsh_acc_state_progrs_enable_control(struct lis3dsh_acc_data *acc, u8 settings)
{
	u8 val1, val2;
	int err = -1;

	sensor_debug(DEBUG_INFO, "[Sensors] %s : set machine state=%d\n", __func__, settings);

	switch ( settings ) {
	case LIS3DSH_SM1_DIS_SM2_DIS:
		val1 = LIS3DSH_SM1_EN_OFF;
		val2 = LIS3DSH_SM2_EN_OFF;
		break;
	case LIS3DSH_SM1_DIS_SM2_EN:
		val1 = LIS3DSH_SM1_EN_OFF;
		val2 = LIS3DSH_SM2_EN_ON;
		break;
	case LIS3DSH_SM1_EN_SM2_DIS:
		val1 = LIS3DSH_SM1_EN_ON;
		val2 = LIS3DSH_SM2_EN_OFF;
		break;
	case LIS3DSH_SM1_EN_SM2_EN:
		val1 = LIS3DSH_SM1_EN_ON;
		val2 = LIS3DSH_SM2_EN_ON;
		break;
	default :
		pr_err("[lis3dsh] invalid state program setting : 0x%02x\n",settings);
		return err;
	}

	err = lis3dsh_acc_register_masked_update(acc,
		LIS3DSH_CTRL_REG1, LIS3DSH_SM1_EN_MASK, val1,
							RES_LIS3DSH_CTRL_REG1);
	if (err < 0 )
		return err;
	err = lis3dsh_acc_register_masked_update(acc,
		LIS3DSH_CTRL_REG2, LIS3DSH_SM2_EN_MASK, val2,
							RES_LIS3DSH_CTRL_REG2);
	if (err < 0 )
			return err;
	acc->stateprogs_enable_setting = settings;

	pr_debug("[lis3dsh] state program setting : 0x%02x\n",
						acc->stateprogs_enable_setting);


	return err;
}

static int lis3dsh_acc_validate_pdata(struct lis3dsh_acc_data *acc)
{
	acc->pdata->poll_interval = max(acc->pdata->poll_interval,
			acc->pdata->min_interval);

	if (acc->pdata->axis_map_x > 2 ||
		acc->pdata->axis_map_y > 2 ||
		 acc->pdata->axis_map_z > 2) {
		dev_err(&acc->client->dev, "invalid axis_map value "
			"x:%u y:%u z%u\n", acc->pdata->axis_map_x,
				acc->pdata->axis_map_y, acc->pdata->axis_map_z);
		return -EINVAL;
	}

	/* Only allow 0 and 1 for negation boolean flag */
	if (acc->pdata->negate_x > 1 || acc->pdata->negate_y > 1
			|| acc->pdata->negate_z > 1) {
		dev_err(&acc->client->dev, "invalid negate value "
			"x:%u y:%u z:%u\n", acc->pdata->negate_x,
				acc->pdata->negate_y, acc->pdata->negate_z);
		return -EINVAL;
	}

	/* Enforce minimum polling interval */
	if (acc->pdata->poll_interval < acc->pdata->min_interval) {
		dev_err(&acc->client->dev, "minimum poll interval violated\n");
		return -EINVAL;
	}

	return 0;
}

static int lis3dsh_acc_input_init(struct lis3dsh_acc_data *acc)
{
	int ret = 0;

	acc->input_dev = input_allocate_device();
	if (!acc->input_dev) {
		printk("[lis3dsh]: Failed to allocate input_data device\n");
		return -ENOMEM;
	}

	acc->input_dev->name = LIS3DSH_ACC_DEV_NAME;
	acc->input_dev->id.bustype = BUS_HOST;
	acc->input_dev->dev.parent = &sensor_data->client->dev;
	input_set_capability(acc->input_dev, EV_MSC, MSC_GESTURE);
	input_set_drvdata(acc->input_dev, sensor_data);
	ret = input_register_device(acc->input_dev);
	if (ret < 0) {
		input_free_device(acc->input_dev);
		return ret;
	}
	sensor_data->input_dev = acc->input_dev;

	return 0;
}

static void lis3dsh_acc_input_cleanup(struct lis3dsh_acc_data *acc)
{
	input_unregister_device(acc->input_dev);
	input_free_device(acc->input_dev);
}

static int lis3dsh_suspend(struct i2c_client *client, pm_message_t mesg)
{
	enable_irq_wake(sensor_data->irq1);
	enable_irq_wake(sensor_data->irq2);
	return 0;
}

static int lis3dsh_resume(struct i2c_client *client)
{
	disable_irq_wake(sensor_data->irq1);
	disable_irq_wake(sensor_data->irq2);
	return 0;
}

static int lis3dsh_pwr_ctrl(struct device *dev, int en)
{
	int ret;
	static  bool pwr_ready = false;
	static struct regulator *pm8921_l15;

	sensor_debug(DEBUG_INFO, "[lis3dsh] %s +++\n", __func__);

	if(!pwr_ready) {
		pm8921_l15 = regulator_get(dev, "8226_l15");
		if (IS_ERR(pm8921_l15)) {
			pr_err("[lis3dsh] %s: regulator get of pm8921_l15 failed (%ld)\n", __func__, PTR_ERR(pm8921_l15));
			ret = PTR_ERR(pm8921_l15);
			goto regulator_get_fail;
		}

		ret = regulator_set_voltage(pm8921_l15, 2800000, 2800000);
		if (ret) {
			pr_err("[lis3dsh] %s: regulator_set_voltage of pm8921_l15 failed(%d)\n", __func__, ret);
			goto reg_put_LDO15;
		}
		pwr_ready = true;
	}

	if(en) {
		ret = regulator_enable(pm8921_l15);
		if (ret) {
			pr_err("[lis3dsh] %s: regulator_enable of pm8921_l15 failed(%d)\n", __func__, ret);
			goto reg_put_LDO15;
    		}
		msleep(10);			//allow the sensor enough time to do its boot up sequence
	} else {
		ret = regulator_disable(pm8921_l15);
		if (ret) {
			pr_err("[lis3dsh] %s: regulator_disable of pm8921_l15 failed(%d)\n", __func__, ret);
			goto reg_put_LDO15;
    		}
	}

	return 0;

	reg_put_LDO15:
		regulator_put(pm8921_l15);
	regulator_get_fail:
		return -ret;
}

static int lis3dsh_acc_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct lis3dsh_acc_data *acc;
	u32 smbus_func = I2C_FUNC_SMBUS_BYTE_DATA |I2C_FUNC_SMBUS_WORD_DATA | I2C_FUNC_SMBUS_I2C_BLOCK ;
	int err = -1;

	dev_info(&client->dev, "[lis3dsh] probe start.\n");

	acc = kzalloc(sizeof(struct lis3dsh_acc_data), GFP_KERNEL);
	if (acc == NULL) {
		err = -ENOMEM;
		dev_err(&client->dev,"failed to allocate memory for module data: %d\n", err);
		goto exit_check_functionality_failed;
	}

	acc->cdev = lis3dsh_cdev;
	acc->cdev.enabled = 0;
	acc->cdev.sensors_enable = lis3dsh_acc_enable;

	err = sensors_classdev_register(&client->dev, &acc->cdev);
	if (err) {
		pr_err("[lis3dsh] class device create failed: %d\n", err);
		goto classdev_register_fail;
	}

	err = sysfs_create_group(&client->dev.kobj, &lis3dsh_attr_group);
	if (err)
		goto classdev_register_fail;

	err = lis3dsh_pwr_ctrl(&client->dev, 1);
	if(err)
		goto exit_check_functionality_failed;

	/* Support for both I2C and SMBUS adapter interfaces. */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_warn(&client->dev, "client not i2c capable\n");
		if (i2c_check_functionality(client->adapter, smbus_func)){
			acc->use_smbus = 1;
			dev_warn(&client->dev, "client using SMBUS\n");
		} else {
			err = -ENODEV;
			dev_err(&client->dev, "client nor SMBUS capable\n");
			acc->use_smbus = 0;
			goto exit_check_functionality_failed;
		}
	} else {
		acc->use_smbus = 0;
	}

	mutex_init(&acc->lock);
	mutex_lock(&acc->lock);

	acc->client = client;
	i2c_set_clientdata(client, acc);
	
	wake_lock_init(&acc->tilt_wakelock, WAKE_LOCK_SUSPEND, "tilt detect");

	acc->pdata = kmalloc(sizeof(*acc->pdata), GFP_KERNEL);
	if (acc->pdata == NULL) {
		err = -ENOMEM;
		dev_err(&client->dev,
				"failed to allocate memory for pdata: %d\n",
				err);
		goto err_mutexunlock;
	}

	if(client->dev.platform_data == NULL) {
		default_lis3dsh_acc_pdata.gpio_int1 = int1_gpio;
		default_lis3dsh_acc_pdata.gpio_int2 = int2_gpio;
		memcpy(acc->pdata, &default_lis3dsh_acc_pdata,
							sizeof(*acc->pdata));
		dev_info(&client->dev, "using default platform_data\n");
	} else {
		memcpy(acc->pdata, client->dev.platform_data,
							sizeof(*acc->pdata));
	}

	err = lis3dsh_acc_validate_pdata(acc);
	if (err < 0) {
		dev_err(&client->dev, "failed to validate platform data\n");
		goto exit_kfree_pdata;
	}

	sensor_data = acc;

	if (acc->pdata->init) {
		err = acc->pdata->init();
		if (err < 0) {
			dev_err(&client->dev, "init failed: %d\n", err);
			goto err_pdata_init;
		}
	}

	if(acc->pdata->gpio_int1 >= 0){
		acc->irq1 = gpio_to_irq(acc->pdata->gpio_int1);
		sensor_debug(DEBUG_INFO, "[lis3dsh] %s: has set irq1 to irq: %d "
							"mapped on gpio:%d\n", __func__, acc->irq1, acc->pdata->gpio_int1);
		gpio_request(acc->pdata->gpio_int1, "lis3dsh-irq1");
		gpio_direction_input(acc->pdata->gpio_int1);
	}

	if(acc->pdata->gpio_int2 >= 0){
		acc->irq2 = gpio_to_irq(acc->pdata->gpio_int2);
		sensor_debug(DEBUG_INFO, "[lis3dsh] %s: has set irq2 to irq: %d "
							"mapped on gpio:%d\n", __func__, acc->irq2, acc->pdata->gpio_int2);
		gpio_request(acc->pdata->gpio_int2, "lis3dsh-irq2");
		gpio_direction_input(acc->pdata->gpio_int2);
	}

	/* resume state init config */
	memset(acc->resume_state, 0, ARRAY_SIZE(acc->resume_state));
	lis3dsh_acc_set_init_register_values(acc);
	//init state program1 and params
	lis3dsh_acc_set_init_statepr1_param(acc, LIS3DSH_TILT_TO_WAKE);
	lis3dsh_acc_set_init_statepr1_inst(acc, LIS3DSH_TILT_TO_WAKE);
	//init state program2  and params
	lis3dsh_acc_set_init_statepr2_param(acc);
	lis3dsh_acc_set_init_statepr2_inst(acc);

	err = lis3dsh_acc_device_power_on(acc);
	if (err < 0) {
		dev_err(&client->dev, "power on failed: %d\n", err);
		goto err_pdata_init;
	}

	atomic_set(&acc->enabled, 1);

	err = lis3dsh_acc_update_fs_range(acc, acc->pdata->fs_range);
	if (err < 0) {
		dev_err(&client->dev, "update_fs_range failed\n");
		goto  err_power_off;
	}

	err = lis3dsh_acc_update_odr(acc, acc->pdata->poll_interval);
	if (err < 0) {
		dev_err(&client->dev, "update_odr failed\n");
		goto  err_power_off;
	}

	err = lis3dsh_acc_input_init(acc);
	if (err < 0) {
		dev_err(&client->dev, "input init failed\n");
		goto err_power_off;
	}

	lis3dsh_acc_device_power_off(acc);	

	/* As default, do not report information */
	atomic_set(&acc->enabled, 0);

	if(acc->pdata->gpio_int1 >= 0){
		INIT_WORK(&acc->irq1_work, lis3dsh_acc_irq1_work_func);
		acc->irq1_work_queue =
			create_singlethread_workqueue("lis3dsh_acc_wq1");
		if (!acc->irq1_work_queue) {
			err = -ENOMEM;
			dev_err(&client->dev,
					"[lis3dsh] cannot create work queue1: %d\n", err);
			goto err_remove_sysfs_int;
		}
		err = request_irq(acc->irq1, lis3dsh_acc_isr1,
				IRQF_TRIGGER_RISING, "lis3dsh_acc_irq1", acc);
		if (err < 0) {
			dev_err(&client->dev, "[lis3dsh] request irq1 failed: %d\n", err);
			goto err_destoyworkqueue1;
		}
		disable_irq_nosync(acc->irq1);
	}

	if(acc->pdata->gpio_int2 >= 0){
		INIT_WORK(&acc->irq2_work, lis3dsh_acc_irq2_work_func);
		acc->irq2_work_queue =
			create_singlethread_workqueue("lis3dsh_acc_wq2");
		if (!acc->irq2_work_queue) {
			err = -ENOMEM;
			dev_err(&client->dev,
					"[lis3dsh] cannot create work queue2: %d\n", err);
			goto err_free_irq1;
		}
		err = request_irq(acc->irq2, lis3dsh_acc_isr2,
				IRQF_TRIGGER_RISING, "lis3dsh_acc_irq2", acc);
		if (err < 0) {
			dev_err(&client->dev, "[lis3dsh] request irq2 failed: %d\n", err);
			goto err_destoyworkqueue2;
		}
		disable_irq_nosync(acc->irq2);
	}

	mutex_unlock(&acc->lock);
	chip_status=1;			//ASUS_BSP +++ Maggie_Lee "Support ATD BMMI"

	dev_info(&client->dev, "[lis3dsh] %s ---\n", __func__);

	return 0;

err_destoyworkqueue2:
	if(acc->pdata->gpio_int2 >= 0)
		destroy_workqueue(acc->irq2_work_queue);
err_free_irq1:
	free_irq(acc->irq1, acc);
err_destoyworkqueue1:
	if(acc->pdata->gpio_int1 >= 0)
		destroy_workqueue(acc->irq1_work_queue);
err_remove_sysfs_int:
err_power_off:
	lis3dsh_acc_device_power_off(acc);
err_pdata_init:
	if (acc->pdata->exit)
		acc->pdata->exit();
exit_kfree_pdata:
	kfree(acc->pdata);
err_mutexunlock:
	wake_lock_destroy(&acc->tilt_wakelock);
	mutex_unlock(&acc->lock);
exit_check_functionality_failed:
	pr_err("[lis3dsh] %s: Driver Init failed\n", __func__);
classdev_register_fail:
	kfree(acc);
	return err;
}

static int lis3dsh_acc_remove(struct i2c_client *client)
{
	struct lis3dsh_acc_data *acc = i2c_get_clientdata(client);

	if(acc->pdata->gpio_int1 >= 0){
		free_irq(acc->irq1, acc);
		gpio_free(acc->pdata->gpio_int1);
		destroy_workqueue(acc->irq1_work_queue);
	}

	if(acc->pdata->gpio_int2 >= 0){
		free_irq(acc->irq2, acc);
		gpio_free(acc->pdata->gpio_int2);
		destroy_workqueue(acc->irq2_work_queue);
	}

	lis3dsh_acc_device_power_off(acc);
	lis3dsh_acc_input_cleanup(acc);

	if (acc->pdata->exit)
		acc->pdata->exit();

	kfree(acc->pdata);
	wake_lock_destroy(&acc->tilt_wakelock);
	kfree(acc);

	chip_status=0;			//ASUS_BSP +++ Maggie_Lee "Support ATD BMMI"
	return 0;
}

static const struct i2c_device_id lis3dsh_acc_id[]
		= { { LIS3DSH_ACC_DEV_NAME, 0 }, { }, };

MODULE_DEVICE_TABLE(i2c, lis3dsh_acc_id);

static struct of_device_id lis3dsh_match_table[] = {
	{ .compatible = "stm,lis3dsh_acc",},
	{},
};

static struct i2c_driver lis3dsh_acc_driver = {
	.driver = {
			.owner = THIS_MODULE,
			.name = LIS3DSH_ACC_DEV_NAME,
			.of_match_table = lis3dsh_match_table,
		  },
	.probe = lis3dsh_acc_probe,
	.suspend = lis3dsh_suspend,
	.resume = lis3dsh_resume,
	.remove = lis3dsh_acc_remove,
	.id_table = lis3dsh_acc_id,
};

static int __init lis3dsh_acc_init(void)
{
	return i2c_add_driver(&lis3dsh_acc_driver);
}

static void __exit lis3dsh_acc_exit(void)
{
	return i2c_del_driver(&lis3dsh_acc_driver);
}

module_init(lis3dsh_acc_init);
module_exit(lis3dsh_acc_exit);

MODULE_DESCRIPTION("lis3dsh accelerometer driver");
MODULE_AUTHOR("ASUS");
MODULE_LICENSE("GPL");
