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
#include	<linux/workqueue.h>
#include	<linux/regulator/consumer.h>
#include	<linux/irq.h>
#include	<linux/gpio.h>
#include	<linux/interrupt.h>
#include	<linux/slab.h>

#include	<linux/input/lis3dsh.h>

#if defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#elif defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
struct notifier_block lis3dsh_fb_notif;
#endif

#ifdef CONFIG_PM_8226_CHARGER
extern int pm8226_is_ac_usb_in(void);
#endif

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

struct lis3dsh_acc_data *g_acc;

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

static int lis3dsh_acc_state_progrs_enable_control(struct lis3dsh_acc_data *acc, u8 settings);

struct lis3dsh_acc_data {
	struct i2c_client *client;
	struct lis3dsh_acc_platform_data *pdata;

	struct mutex lock;
	struct delayed_work input_work;

	struct input_dev *input_dev;

	int hw_initialized;
	/* hw_working=-1 means not tested yet */
	int hw_working;
	atomic_t enabled;
	int on_before_suspend;
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

	int report_event_en;
	int tilt_status;

#ifdef DEBUG
	u8 reg_addr;
#endif
};

static int chip_status=0;			//ASUS_BSP +++ Maggie_Lee "Support ATD BMMI"
static atomic_t is_suspend;
// ASUS_BSP +++ Maggie_Lee "Detect uevent from gsensor for double tap"
static struct class *lis3dsh_class = NULL;
static int lis3dsh_major = 0;
static int lis3dsh_minor = 0;
static dev_t lis3dsh_dev;
struct device *lis3dsh_class_dev = NULL;
static char event_status[10];
// ASUS_BSP --- Maggie_Lee "Detect uevent from gsensor for double tap"

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
	#if (LOAD_SM1_PROGRAM == 0)
	sel = 0;
	#endif

	switch (sel) {
	case LIS3DSH_TILT_TO_WAKE:				//detect tilt
		acc->resume_stmach_program1[0] = 0x09;
		acc->resume_stmach_program1[1] = 0x71;
		acc->resume_stmach_program1[2] = 0x88;
		acc->resume_stmach_program1[3] = 0x33;
		acc->resume_stmach_program1[4] = 0x08;
		acc->resume_stmach_program1[5] = 0x62;
		acc->resume_stmach_program1[6] = 0x88;
		acc->resume_stmach_program1[7] = 0x44;
		acc->resume_stmach_program1[8] = 0x11;
		acc->resume_stmach_program1[9] = 0x00;
		acc->resume_stmach_program1[10] = 0x00;
		acc->resume_stmach_program1[11] = 0x00;
		acc->resume_stmach_program1[12] = 0x00;
		acc->resume_stmach_program1[13] = 0x00;
		acc->resume_stmach_program1[14] = 0x00;
		acc->resume_stmach_program1[15] = 0x00;
		break;
	default:
		acc->resume_stmach_program1[0] = 0x00;
		acc->resume_stmach_program1[1] = 0x00;
		acc->resume_stmach_program1[2] = 0x00;
		acc->resume_stmach_program1[3] = 0x00;
		acc->resume_stmach_program1[4] = 0x00;
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
		break;
	}
}

static void lis3dsh_acc_set_init_statepr2_inst(struct lis3dsh_acc_data *acc)
{
#if (LOAD_SM2_PROGRAM == 1)
	/* Place here state machine 2 program */
	/* Knock Knock Function */
	acc->resume_stmach_program2[0] = 0x08;
	acc->resume_stmach_program2[1] = 0x03;
	acc->resume_stmach_program2[2] = 0x61;
	acc->resume_stmach_program2[3] = 0x61;
	acc->resume_stmach_program2[4] = 0x05;
	acc->resume_stmach_program2[5] = 0x47;
	acc->resume_stmach_program2[6] = 0x03;
	acc->resume_stmach_program2[7] = 0x52;
	acc->resume_stmach_program2[8] = 0x15;
	acc->resume_stmach_program2[9] = 0x47;
	acc->resume_stmach_program2[10] = 0x03;
	acc->resume_stmach_program2[11] = 0x52;
	acc->resume_stmach_program2[12] = 0x11;
	acc->resume_stmach_program2[13] = 0x00;
	acc->resume_stmach_program2[14] = 0x00;
	acc->resume_stmach_program2[15] = 0x00;
#else
	acc->resume_stmach_program2[0] = 0x00;
	acc->resume_stmach_program2[1] = 0x00;
	acc->resume_stmach_program2[2] = 0x00;
	acc->resume_stmach_program2[3] = 0x00;
	acc->resume_stmach_program2[4] = 0x00;
	acc->resume_stmach_program2[5] = 0x00;
	acc->resume_stmach_program2[6] = 0x00;
	acc->resume_stmach_program2[7] = 0x00;
	acc->resume_stmach_program2[8] = 0x00;
	acc->resume_stmach_program2[9] = 0x00;
	acc->resume_stmach_program2[10] = 0x00;
	acc->resume_stmach_program2[11] = 0x00;
	acc->resume_stmach_program2[12] = 0x00;
	acc->resume_stmach_program2[13] = 0x00;
	acc->resume_stmach_program2[14] = 0x00;
	acc->resume_stmach_program2[15] = 0x00;
#endif /* LOAD_SM2_PROGRAM */
}

static void lis3dsh_acc_set_init_statepr1_param(struct lis3dsh_acc_data *acc, int sel)
{
#if (LOAD_SP1_PARAMETERS == 1)
	switch (sel) {
	case LIS3DSH_TILT_TO_WAKE:				//detect tilt
		/* Place here state machine 1 parameters */
		/* Tilt to wake function */
		acc->resume_state[RES_LIS3DSH_TIM4_1] = 0x00;
		acc->resume_state[RES_LIS3DSH_TIM3_1] = 0x00;
		acc->resume_state[RES_LIS3DSH_TIM2_1_L] = 0xC8;
		acc->resume_state[RES_LIS3DSH_TIM2_1_H] = 0x00;
		acc->resume_state[RES_LIS3DSH_TIM1_1_L] = 0x78;
		acc->resume_state[RES_LIS3DSH_TIM1_1_H] = 0x00;
		acc->resume_state[RES_LIS3DSH_THRS2_1] = 0xE0;		//up angle
		acc->resume_state[RES_LIS3DSH_THRS1_1] = 0x01;		//down angle
		/* DES1 not available*/
		acc->resume_state[RES_LIS3DSH_SA_1] = 0x80;
		acc->resume_state[RES_LIS3DSH_MA_1] = 0x80;
		acc->resume_state[RES_LIS3DSH_SETT_1] = 0x20;
		break;
	default:
		acc->resume_state[RES_LIS3DSH_TIM4_1] = 0x00;
		acc->resume_state[RES_LIS3DSH_TIM3_1] = 0x00;
		acc->resume_state[RES_LIS3DSH_TIM2_1_L] = 0x00;
		acc->resume_state[RES_LIS3DSH_TIM2_1_H] = 0x00;
		acc->resume_state[RES_LIS3DSH_TIM1_1_L] = 0x00;
		acc->resume_state[RES_LIS3DSH_TIM1_1_H] = 0x00;
		acc->resume_state[RES_LIS3DSH_THRS2_1] = 0x00;
		acc->resume_state[RES_LIS3DSH_THRS1_1] = 0x00;
		/* DES1 not available*/
		acc->resume_state[RES_LIS3DSH_SA_1] = 0x00;
		acc->resume_state[RES_LIS3DSH_MA_1] = 0x00;
		acc->resume_state[RES_LIS3DSH_SETT_1] = 0x00;
		break;
	}
#else
	acc->resume_state[RES_LIS3DSH_TIM4_1] = 0x00;
	acc->resume_state[RES_LIS3DSH_TIM3_1] = 0x00;
	acc->resume_state[RES_LIS3DSH_TIM2_1_L] = 0x00;
	acc->resume_state[RES_LIS3DSH_TIM2_1_H] = 0x00;
	acc->resume_state[RES_LIS3DSH_TIM1_1_L] = 0x00;
	acc->resume_state[RES_LIS3DSH_TIM1_1_H] = 0x00;
	acc->resume_state[RES_LIS3DSH_THRS2_1] = 0x00;
	acc->resume_state[RES_LIS3DSH_THRS1_1] = 0x00;
	/* DES1 not available*/
	acc->resume_state[RES_LIS3DSH_SA_1] = 0x00;
	acc->resume_state[RES_LIS3DSH_MA_1] = 0x00;
	acc->resume_state[RES_LIS3DSH_SETT_1] = 0x00;
#endif
}

static void lis3dsh_acc_set_init_statepr2_param(struct lis3dsh_acc_data *acc)
{
#if (LOAD_SP2_PARAMETERS == 1)
	/* Place here state machine 2 parameters */
	/* Knock knock function */
	acc->resume_state[RES_LIS3DSH_TIM4_2] = 0x05;
	acc->resume_state[RES_LIS3DSH_TIM3_2] = 0x14;
	acc->resume_state[RES_LIS3DSH_TIM2_2_L] = 0x24;
	acc->resume_state[RES_LIS3DSH_TIM2_2_H] = 0x00;
	acc->resume_state[RES_LIS3DSH_TIM1_2_L] = 0x86;
	acc->resume_state[RES_LIS3DSH_TIM1_2_H] = 0x00;
	acc->resume_state[RES_LIS3DSH_THRS2_2] = 0x01;
	acc->resume_state[RES_LIS3DSH_THRS1_2] = 0x03;
	acc->resume_state[RES_LIS3DSH_DES_2] = 0x00;
	acc->resume_state[RES_LIS3DSH_SA_2] = 0x00;
	acc->resume_state[RES_LIS3DSH_MA_2] = 0x03;
	acc->resume_state[RES_LIS3DSH_SETT_2] = 0x21;
#else
	acc->resume_state[RES_LIS3DSH_TIM4_2] = 0x00;
	acc->resume_state[RES_LIS3DSH_TIM3_2] = 0x00;
	acc->resume_state[RES_LIS3DSH_TIM2_2_L] = 0x00;
	acc->resume_state[RES_LIS3DSH_TIM2_2_H] = 0x00;
	acc->resume_state[RES_LIS3DSH_TIM1_2_L] = 0x00;
	acc->resume_state[RES_LIS3DSH_TIM1_2_H] = 0x00;
	acc->resume_state[RES_LIS3DSH_THRS2_2] = 0x00;
	acc->resume_state[RES_LIS3DSH_THRS1_2] = 0x00;
	acc->resume_state[RES_LIS3DSH_DES_2] = 0x00;
	acc->resume_state[RES_LIS3DSH_SA_2] = 0x00;
	acc->resume_state[RES_LIS3DSH_MA_2] = 0x00;
	acc->resume_state[RES_LIS3DSH_SETT_2] = 0x00;
#endif
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
	struct lis3dsh_acc_data *acc;

	acc = container_of(work, struct lis3dsh_acc_data, irq1_work);
	/* TODO  add interrupt service procedure.
		 ie:lis3dsh_acc_get_int_source(acc); */
	sensor_debug(DEBUG_INFO, "[lis3dsh] %s: IRQ1 triggered\n", __func__);
	/*  */
	rbuf[0] = LIS3DSH_INTERR_STAT;
	err = lis3dsh_acc_i2c_read(acc, rbuf, 1);
	status = rbuf[0];
	if(status & LIS3DSH_STAT_INTSM1_BIT) {
		rbuf[0] = LIS3DSH_OUTS_1;
		err = lis3dsh_acc_i2c_read(acc, rbuf, 1);
		sensor_debug(DEBUG_INFO, "[lis3dsh] %s: interrupt (0x%02x)\n", __func__, rbuf[0]);
		if((rbuf[0] == 0x80)) {
			acc->tilt_status = !acc->tilt_status;
			printk("***********************Tilt to wake event [%s]\n", acc->tilt_status?"TILT":"FLAT");
			if (acc->tilt_status) {
				if (atomic_read(&is_suspend)) {
					input_report_key(acc->input_dev, KEY_POWER,1);
					input_sync(acc->input_dev);
					msleep(5);
					input_report_key(acc->input_dev, KEY_POWER,0);
					input_sync(acc->input_dev);
				}
			}
		}
	}
	if(status & LIS3DSH_STAT_INTSM2_BIT) {
		rbuf[0] = LIS3DSH_OUTS_2;
		err = lis3dsh_acc_i2c_read(acc, rbuf, 1);
		sensor_debug(DEBUG_INFO, "[lis3dsh] %s: interrupt (0x%02x)\n", __func__, rbuf[0]);
	}
	enable_irq(acc->irq1);
	sensor_debug(DEBUG_VERBOSE, "[lis3dsh] %s: IRQ1 re-enabled\n", __func__);
}

static void lis3dsh_acc_irq2_work_func(struct work_struct *work)
{
	int err = -1;
	u8 rbuf[2], status;
	struct lis3dsh_acc_data *acc;

	acc = container_of(work, struct lis3dsh_acc_data, irq2_work);
	sensor_debug(DEBUG_INFO, "[lis3dsh] %s: IRQ2 triggered\n", __func__);
	/*  */
	rbuf[0] = LIS3DSH_INTERR_STAT;
	err = lis3dsh_acc_i2c_read(acc, rbuf, 1);
	status = rbuf[0];
	if(status & LIS3DSH_STAT_INTSM1_BIT) {
		rbuf[0] = LIS3DSH_OUTS_1;
		err = lis3dsh_acc_i2c_read(acc, rbuf, 1);
		sensor_debug(DEBUG_INFO, "[lis3dsh] %s: interrupt (0x%02x)\n", __func__, rbuf[0]);
	}
	if(status & LIS3DSH_STAT_INTSM2_BIT) {
		rbuf[0] = LIS3DSH_OUTS_2;
		err = lis3dsh_acc_i2c_read(acc, rbuf, 1);
		sensor_debug(DEBUG_INFO, "[lis3dsh] %s: interrupt (0x%02x)\n", __func__, rbuf[0]);
		if((rbuf[0] == 0x01) || (rbuf[0] == 0x02) & !pm8226_is_ac_usb_in()) {		//do not report knock event if device is inserted into pogo
			printk("***********************knock-knock event\n");
			strcpy(event_status,"KNOCK");
			kobject_uevent(&lis3dsh_class_dev->kobj, KOBJ_CHANGE);
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

	hw_d[0] = hw_d[0] * acc->sensitivity;
	hw_d[1] = hw_d[1] * acc->sensitivity;
	hw_d[2] = hw_d[2] * acc->sensitivity;


	xyz[0] = ((acc->pdata->negate_x) ? (-hw_d[acc->pdata->axis_map_x])
		   : (hw_d[acc->pdata->axis_map_x]));
	xyz[1] = ((acc->pdata->negate_y) ? (-hw_d[acc->pdata->axis_map_y])
		   : (hw_d[acc->pdata->axis_map_y]));
	xyz[2] = ((acc->pdata->negate_z) ? (-hw_d[acc->pdata->axis_map_z])
		   : (hw_d[acc->pdata->axis_map_z]));

	sensor_debug(DEBUG_RAW, "[lis3dsh] %s read x=%d, y=%d, z=%d\n", __func__, xyz[0], xyz[1], xyz[2]);

	return err;
}

static void lis3dsh_acc_report_values(struct lis3dsh_acc_data *acc,
					int *xyz)
{
	input_report_abs(acc->input_dev, ABS_X, xyz[0]);
	input_report_abs(acc->input_dev, ABS_Y, xyz[1]);
	input_report_abs(acc->input_dev, ABS_Z, xyz[2]);
	input_sync(acc->input_dev);
}

static int lis3dsh_acc_enable(struct lis3dsh_acc_data *acc)
{
	int err;

	if (!atomic_cmpxchg(&acc->enabled, 0, 1)) {
		sensor_debug(DEBUG_INFO, "[Sensors] %s +++ \n", __func__);
		err = lis3dsh_acc_device_power_on(acc);
		if (err < 0) {
			atomic_set(&acc->enabled, 0);
			return err;
		}
		if (acc->report_event_en)
			schedule_delayed_work(&acc->input_work, msecs_to_jiffies(acc->pdata->poll_interval));
		sensor_debug(DEBUG_INFO, "[Sensors] %s ---\n", __func__);
	}
	return 0;
}

static int lis3dsh_acc_disable(struct lis3dsh_acc_data *acc)
{
	if (atomic_cmpxchg(&acc->enabled, 1, 0)) {
		sensor_debug(DEBUG_INFO, "[Sensors] %s +++\n", __func__);
		if (acc->report_event_en)
			cancel_delayed_work_sync(&acc->input_work);
		lis3dsh_acc_device_power_off(acc);
	}
	sensor_debug(DEBUG_INFO, "[Sensors] %s ---\n", __func__);
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

static ssize_t attr_get_polling_rate(struct device *dev,
					struct device_attribute *attr,
								char *buf)
{
	int val;
	struct lis3dsh_acc_data *acc = dev_get_drvdata(dev);
	mutex_lock(&acc->lock);
	val = acc->pdata->poll_interval;
	mutex_unlock(&acc->lock);
	return sprintf(buf, "%d\n", val);
}

static ssize_t attr_set_polling_rate(struct device *dev,
					struct device_attribute *attr,
						const char *buf, size_t size)
{
	int err;
	struct lis3dsh_acc_data *acc = dev_get_drvdata(dev);
	unsigned long interval_ms;

	if (strict_strtoul(buf, 10, &interval_ms))
		return -EINVAL;
	if (!interval_ms)
		return -EINVAL;
	mutex_lock(&acc->lock);
	err = lis3dsh_acc_update_odr(acc, interval_ms);
	if(err >= 0)
	{
		acc->pdata->poll_interval = interval_ms;
	}
	mutex_unlock(&acc->lock);
	return size;
}

static ssize_t attr_get_range(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	u8 val;
	struct lis3dsh_acc_data *acc = dev_get_drvdata(dev);
	int range = 2;
	mutex_lock(&acc->lock);
	val = acc->pdata->fs_range ;

	switch(val) {
	case LIS3DSH_ACC_G_2G:
		range = 2;
		break;
	case LIS3DSH_ACC_G_4G:
		range = 4;
		break;
	case LIS3DSH_ACC_G_6G:
		range = 6;
		break;
	case LIS3DSH_ACC_G_8G:
		range = 8;
		break;
	case LIS3DSH_ACC_G_16G:
		range = 16;
		break;
	}
	mutex_unlock(&acc->lock);
	return sprintf(buf, "%d\n", range);
}

static ssize_t attr_set_range(struct device *dev,
				struct device_attribute *attr,
						const char *buf, size_t size)
{
	int err;
	struct lis3dsh_acc_data *acc = dev_get_drvdata(dev);
	unsigned long val;
	u8 range;
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	switch(val) {
		case 2:
			range = LIS3DSH_ACC_G_2G;
			break;
		case 4:
			range = LIS3DSH_ACC_G_4G;
			break;
		case 6:
			range = LIS3DSH_ACC_G_6G;
			break;
		case 8:
			range = LIS3DSH_ACC_G_8G;
			break;
		case 16:
			range = LIS3DSH_ACC_G_16G;
			break;
		default:
			return -1;
	}

	mutex_lock(&acc->lock);
	err = lis3dsh_acc_update_fs_range(acc, range);
	if(err >= 0)
	{
		acc->pdata->fs_range = range;
	}
	mutex_unlock(&acc->lock);
	return size;
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
	struct lis3dsh_acc_data *acc = dev_get_drvdata(dev);
	unsigned long val;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	if (val)
		lis3dsh_acc_enable(acc);
	else
		lis3dsh_acc_disable(acc);
	return size;
}

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

#ifndef ASUS_USER_BUILD
//ASUS_BSP +++ Maggie_Lee "Support ATD BMMI"
static ssize_t attr_get_chip_status(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	printk("[lis3dsh] read_accel_status = %d",chip_status);
	return sprintf(buf, "%d\n", chip_status);
}
//ASUS_BSP --- Maggie_Lee "Support ATD BMMI"

static ssize_t attr_reg_dump(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct lis3dsh_acc_data *acc = dev_get_drvdata(dev);
	lis3dsh_acc_reg_dump(acc);
	return sprintf(buf, "DONE\n");
}
#endif

#ifdef ASUS_FACTORY_BUILD
static int factory_test_get_output(struct device *dev, short *rOUT)
{
	struct lis3dsh_acc_data *acc = dev_get_drvdata(dev);
	u16 OUT[3];
	u8 rbuf[3];
	int err;
	int i;

	//Read output registers to clear ZYXDA
	rbuf[0] = LIS3DSH_OUTX_L;
	err = lis3dsh_acc_i2c_read(acc, rbuf, 1);
	rbuf[0] = LIS3DSH_OUTX_H;
	err = lis3dsh_acc_i2c_read(acc, rbuf, 1);
	rbuf[0] = LIS3DSH_OUTY_L;
	err = lis3dsh_acc_i2c_read(acc, rbuf, 1);
	rbuf[0] = LIS3DSH_OUTY_H;
	err = lis3dsh_acc_i2c_read(acc, rbuf, 1);
	rbuf[0] = LIS3DSH_OUTZ_L;
	err = lis3dsh_acc_i2c_read(acc, rbuf, 1);
	rbuf[0] = LIS3DSH_OUTZ_H;
	err = lis3dsh_acc_i2c_read(acc, rbuf, 1);

	//Check ZYXDA
	for(i=0 ; i<5 ; i++) {
		rbuf[0] = LIS3DSH_STATUS_REG;
		err = lis3dsh_acc_i2c_read(acc, rbuf, 1);
		printk("[lis3dsh] %d: LIS3DSH_STATUS_REG=%d XYZDA=%d\n", i, rbuf[0], (rbuf[0]&0x08)>0?1:0);
		if ((rbuf[0]&0x08) > 0) {
			rbuf[0] = LIS3DSH_OUTX_L;
			err = lis3dsh_acc_i2c_read(acc, rbuf, 1);
			if (err < 0)
				goto err_resume_state;
			OUT[0] = rbuf[0];

			rbuf[0] = LIS3DSH_OUTX_H;
			err = lis3dsh_acc_i2c_read(acc, rbuf, 1);
			if (err < 0)
				goto err_resume_state;
			OUT[0] = (rbuf[0] << 8 | OUT[0]);

			rbuf[0] = LIS3DSH_OUTY_L;
			err = lis3dsh_acc_i2c_read(acc, rbuf, 1);
			if (err < 0)
				goto err_resume_state;
			OUT[1] = rbuf[0];
			
			rbuf[0] = LIS3DSH_OUTY_H;
			err = lis3dsh_acc_i2c_read(acc, rbuf, 1);
			if (err < 0)
				goto err_resume_state;
			OUT[1] = (rbuf[0] << 8 | OUT[1]);

			rbuf[0] = LIS3DSH_OUTZ_L;
			err = lis3dsh_acc_i2c_read(acc, rbuf, 1);
			if (err < 0)
				goto err_resume_state;
			OUT[2] = rbuf[0];

			rbuf[0] = LIS3DSH_OUTZ_H;
			err = lis3dsh_acc_i2c_read(acc, rbuf, 1);
			if (err < 0)
				goto err_resume_state;
			OUT[2] = (rbuf[0] << 8 | OUT[2]);

			//convert the output to mg (2's complement then multiply by 0.06)
			printk("[lis3dsh] OUT[X]=%d OUT[Y]=%d OUT[Z]=%d\n", OUT[0], OUT[1], OUT[2]);
			for(i=0;i<3;i++) {
				rOUT[i] = (short)OUT[i];
				rOUT[i] = rOUT[i]*6/100;
			}
			return 0;
			
		}
		if (i==5)
			goto err_no_zyxda;

		msleep(10);
	}

	err_resume_state:
	dev_err(&acc->client->dev, "i2c read err 0x%02x,0x%02x: %d\n", rbuf[0], rbuf[1], err);
	return err;

	err_no_zyxda:
	dev_err(&acc->client->dev, "ZYXDA can not be obtained\n");
	return -1;	
}

static ssize_t attr_factory_test(struct device *dev,	struct device_attribute *attr, char *buf)
{
	struct lis3dsh_acc_data *acc = dev_get_drvdata(dev);
	short OUT_NOST[3], OUT_ST[3], OUT_ABS[3];
	int err, i, j;
	u8 rbuf[2], temp[4];

	printk("[lis3dsh] factory_test +++ \n");

	//Store current register settings then disable irq
	for(i=32, j=0;i < 37;i++, j++) {
		if(i==33)
			i=35;
		rbuf[0] = (u8)i;
		lis3dsh_acc_i2c_read(acc, rbuf, 1);
		temp[j] = rbuf[0];
	}
	if(acc->pdata->gpio_int1)
		disable_irq_nosync(acc->irq1);
	if(acc->pdata->gpio_int2)
		disable_irq_nosync(acc->irq2);

	//Initialize sensor: turn on sensors, enable X/Y/Z, set BDU=1, ODR=100HZ, Cut-off freq=50Hz, FS=2g
	buf[0] = LIS3DSH_CTRL_REG4;
	buf[1] = 0x6F;
	err = lis3dsh_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		goto err_resume_state;

	buf[0] = LIS3DSH_CTRL_REG5;
	buf[1] = 0xC0;
	err = lis3dsh_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		goto err_resume_state;

	buf[0] = LIS3DSH_CTRL_REG3;
	buf[1] = 0x00;
	err = lis3dsh_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		goto err_resume_state;

	buf[0] = LIS3DSH_CTRL_REG6;
	buf[1] = 0x00;
	err = lis3dsh_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		goto err_resume_state;

	//wait 80 ms for stable output
	msleep(80);
	err=factory_test_get_output(dev, OUT_NOST);
	if (err < 0) 
		goto err_out_of_range;

	printk("[lis3dsh] OUT_NOST[X]=%d OUT_NOST[Y]=%d OUT_NOST[Z]=%d\n", OUT_NOST[0], OUT_NOST[1], OUT_NOST[2]);

	//Enable positive sign self test and wait 80ms for stable output
	buf[0] = LIS3DSH_CTRL_REG5;
	buf[1] = 0xC2;
	err = lis3dsh_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		goto err_resume_state;

	msleep(80);
	err=factory_test_get_output(dev, OUT_ST);
	if (err < 0)
		goto err_out_of_range;

	printk("[lis3dsh] OUT_ST[X]=%d OUT_ST[Y]=%d OUT_ST[Z]=%d\n", OUT_ST[0], OUT_ST[1], OUT_ST[2]);

	//the absolute different between obtained data (OUT_NOST) and self test data (OUT_ST) should be between 70 and 1400
	OUT_ABS[0] = abs(OUT_ST[0] - OUT_NOST[0]);
	OUT_ABS[1] = abs(OUT_ST[1] - OUT_NOST[1]);
	OUT_ABS[2] = abs(OUT_ST[2] - OUT_NOST[2]);
	printk("[lis3dsh] OUT_ABS[X]=%d OUT_ABS[Y]=%d OUT_ABS[Z]=%d\n", OUT_ABS[0], OUT_ABS[1], OUT_ABS[2]);

	for( i=0 ; i < 3 ; i++) {
		if (70 > OUT_ABS[i] || OUT_ABS[i] > 1400)
			goto err_out_of_range;
	}

	printk("[lis3dsh] PASS\n");
	//Restore reg value prior to self test and enable irq
	for(i=32, j=0;i < 37;i++, j++) {
		if(i==33)
			i=35;
		rbuf[0] = (u8)i;
		rbuf[1] = temp[j];
		lis3dsh_acc_i2c_write(acc, rbuf, 1);
	}
	if(acc->pdata->gpio_int1 >= 0)
		enable_irq(acc->irq1);
	if(acc->pdata->gpio_int2 >= 0)
		enable_irq(acc->irq2);

	return sprintf(buf, "PASS\n");

	err_resume_state:
	dev_err(&acc->client->dev, "i2c error 0x%02x,0x%02x: %d\n", buf[0], buf[1], err);
	printk("[lis3dsh] FAIL\n");
	for(i=32, j=0;i < 37;i++, j++) {
		if(i==33)
			i=35;
		rbuf[0] = (u8)i;
		rbuf[1] = temp[j];
		lis3dsh_acc_i2c_write(acc, rbuf, 1);
	}
	if(acc->pdata->gpio_int1 >= 0)
		enable_irq(acc->irq1);
	if(acc->pdata->gpio_int2 >= 0)
		enable_irq(acc->irq2);
	return sprintf(buf, "FAIL\n");

	err_out_of_range:
	dev_err(&acc->client->dev, "out of range error %d %d %d\n", OUT_ABS[0], OUT_ABS[1], OUT_ABS[2]);
	printk("[lis3dsh] FAIL\n");
	for(i=32, j=0;i < 37;i++, j++) {
		if(i==33)
			i=35;
		rbuf[0] = (u8)i;
		rbuf[1] = temp[j];
		lis3dsh_acc_i2c_write(acc, rbuf, 1);
	}
	if(acc->pdata->gpio_int1 >= 0)
		enable_irq(acc->irq1);
	if(acc->pdata->gpio_int2 >= 0)
		enable_irq(acc->irq2);
	return sprintf(buf, "FAIL\n");
}
#endif

static int lis3dsh_acc_state_progrs_enable_control(struct lis3dsh_acc_data *acc, u8 settings)
{
	u8 val1, val2;
	int err = -1;
	//settings = settings & 0x03;

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

static ssize_t attr_set_enable_state_prog(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	int err = -1;
	struct lis3dsh_acc_data *acc = dev_get_drvdata(dev);
	long val=0;

	if (strict_strtoul(buf, 16, &val))
		return -EINVAL;


	if ( val < 0x00 || val > LIS3DSH_SM1_EN_SM2_EN){
		pr_warn("[lis3dsh] invalid state program setting, val: %ld\n",val);
		return -EINVAL;
	}

	mutex_lock(&acc->lock);
	err = lis3dsh_acc_state_progrs_enable_control(acc, val);
	mutex_unlock(&acc->lock);
	if (err < 0)
		return err;
	return size;
}

static ssize_t attr_get_enable_state_prog(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	u8 val;
	struct lis3dsh_acc_data *acc = dev_get_drvdata(dev);
	mutex_lock(&acc->lock);
	val = acc->stateprogs_enable_setting;
	mutex_unlock(&acc->lock);
	return sprintf(buf, "0x%02x\n", val);
}

#ifdef DEBUG
/* PAY ATTENTION: These DEBUG funtions don't manage resume_state */
static ssize_t attr_reg_set(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	int rc;
	struct lis3dsh_acc_data *acc = dev_get_drvdata(dev);
	u8 x[2];
	unsigned long val;

	if (strict_strtoul(buf, 16, &val))
		return -EINVAL;
	mutex_lock(&acc->lock);
	x[0] = acc->reg_addr;
	mutex_unlock(&acc->lock);
	x[1] = val;
	rc = lis3dsh_acc_i2c_write(acc, x, 1);
	/*TODO: error need to be managed */
	return size;
}

static ssize_t attr_reg_get(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	ssize_t ret;
	struct lis3dsh_acc_data *acc = dev_get_drvdata(dev);
	int rc;
	u8 data;

	mutex_lock(&acc->lock);
	data = acc->reg_addr;
	mutex_unlock(&acc->lock);
	rc = lis3dsh_acc_i2c_read(acc, &data, 1);
	/*TODO: error need to be managed */
	ret = sprintf(buf, "0x%02x\n", data);
	return ret;
}

static ssize_t attr_addr_set(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct lis3dsh_acc_data *acc = dev_get_drvdata(dev);
	unsigned long val;
	if (strict_strtoul(buf, 16, &val))
		return -EINVAL;
	mutex_lock(&acc->lock);
	acc->reg_addr = val;
	mutex_unlock(&acc->lock);
	return size;
}
#endif

static struct device_attribute attributes[] = {

	__ATTR(poll_period_ms, 0664, attr_get_polling_rate,
							attr_set_polling_rate),
	__ATTR(range, 0664, attr_get_range, attr_set_range),
	__ATTR(enable, 0664, attr_get_enable, attr_set_enable),
	__ATTR(enable_state_prog, 0664, attr_get_enable_state_prog,
						attr_set_enable_state_prog),
	__ATTR(acc_data, 0444, attr_get_acc_data, NULL),
#ifdef DEBUG
	__ATTR(reg_value, 0600, attr_reg_get, attr_reg_set),
	__ATTR(reg_addr, 0200, NULL, attr_addr_set),
#endif
#ifndef ASUS_USER_BUILD
	__ATTR(chip_status, 0444, attr_get_chip_status, NULL),			//ASUS_BSP +++ Maggie_Lee "Support ATD BMMI"
	__ATTR(dump, 0444, attr_reg_dump, NULL),
#endif
#ifdef ASUS_FACTORY_BUILD
	__ATTR(factory_test, 0444, attr_factory_test, NULL),	
#endif
};

static int create_sysfs_interfaces(struct device *dev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(attributes); i++)
		if (device_create_file(dev, attributes + i))
			goto error;
	return 0;

error:
	for ( ; i >= 0; i--)
		device_remove_file(dev, attributes + i);
	dev_err(dev, "%s:Unable to create interface\n", __func__);
	return -1;
}

static int remove_sysfs_interfaces(struct device *dev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(attributes); i++)
		device_remove_file(dev, attributes + i);
	return 0;
}

static void lis3dsh_acc_input_work_func(struct work_struct *work)
{
	struct lis3dsh_acc_data *acc;

	int xyz[3] = { 0 };
	int err;

	acc = container_of((struct delayed_work *)work,
			struct lis3dsh_acc_data, input_work);

	mutex_lock(&acc->lock);
	err = lis3dsh_acc_get_acceleration_data(acc, xyz);
	if (err < 0)
		dev_err(&acc->client->dev, "get_acceleration_data failed\n");
	else
		lis3dsh_acc_report_values(acc, xyz);

	schedule_delayed_work(&acc->input_work, msecs_to_jiffies(acc->pdata->poll_interval));
	mutex_unlock(&acc->lock);
}

int lis3dsh_acc_input_open(struct input_dev *input)
{
	struct lis3dsh_acc_data *acc = input_get_drvdata(input);

	return lis3dsh_acc_enable(acc);
}

void lis3dsh_acc_input_close(struct input_dev *dev)
{
	struct lis3dsh_acc_data *acc = input_get_drvdata(dev);

	lis3dsh_acc_disable(acc);
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
	int err;

	acc->report_event_en = 0; 			//default report ABS event OFF
	INIT_DELAYED_WORK(&acc->input_work, lis3dsh_acc_input_work_func);
	acc->input_dev = input_allocate_device();
	if (!acc->input_dev) {
		err = -ENOMEM;
		dev_err(&acc->client->dev, "input device allocation failed\n");
		goto err0;
	}

	acc->input_dev->open = lis3dsh_acc_input_open;
	acc->input_dev->close = lis3dsh_acc_input_close;
	acc->input_dev->name = LIS3DSH_ACC_DEV_NAME;

	acc->input_dev->id.bustype = BUS_I2C;
	acc->input_dev->dev.parent = &acc->client->dev;

	input_set_drvdata(acc->input_dev, acc);

	set_bit(EV_ABS, acc->input_dev->evbit);
	/*	next is used for interruptA sources data if the case */
	set_bit(ABS_MISC, acc->input_dev->absbit);
	/*	next is used for interruptB sources data if the case */
	set_bit(ABS_WHEEL, acc->input_dev->absbit);
	set_bit(EV_KEY, acc->input_dev->evbit);
	set_bit(KEY_POWER, acc->input_dev->keybit);

	input_set_abs_params(acc->input_dev, ABS_X, -G_MAX, G_MAX, 0, 0);
	input_set_abs_params(acc->input_dev, ABS_Y, -G_MAX, G_MAX, 0, 0);
	input_set_abs_params(acc->input_dev, ABS_Z, -G_MAX, G_MAX, 0, 0);
	/*	next is used for interruptA sources data if the case */
//input_set_abs_params(acc->input_dev, ABS_MISC, INT_MIN, INT_MAX, 0, 0);
	/*	next is used for interruptB sources data if the case */
//input_set_abs_params(acc->input_dev, ABS_WHEEL, INT_MIN, INT_MAX, 0, 0);


	err = input_register_device(acc->input_dev);
	if (err) {
		dev_err(&acc->client->dev,
				"unable to register input device %s\n",
				acc->input_dev->name);
		goto err1;
	}

	return 0;

err1:
	input_free_device(acc->input_dev);
err0:
	return err;
}

static void lis3dsh_acc_input_cleanup(struct lis3dsh_acc_data *acc)
{
	input_unregister_device(acc->input_dev);
	input_free_device(acc->input_dev);
}

#ifdef CONFIG_I2C_STRESS_TEST
#include <linux/i2c_testcase.h>
#define I2C_TEST_ST_SENSOR_FAIL (-1)

static int TestLIS3DSHSensorI2C (struct i2c_client *apClient)
{
	u8 buf[17];
	int err = 0;

	i2c_log_in_test_case("%s ++\n", __func__);

	err = lis3dsh_acc_enable(g_acc);
	if (err < 0) {
		i2c_log_in_test_case("Fail to turn on sensor\n");
		goto error;
	}

	buf[0] = LIS3DSH_WHO_AM_I;
	err = lis3dsh_acc_i2c_read(g_acc, buf, 1);
	if (err < 0) {
		i2c_log_in_test_case("Fail to read sensor ID\n");
		goto error;
	}

	err = lis3dsh_acc_disable(g_acc);
	if (err < 0) {
		i2c_log_in_test_case("Fail to turn off sensor\n");
		goto error;
	}
	
	i2c_log_in_test_case("%s --\n", __func__);

	return I2C_TEST_PASS;

error:
	return I2C_TEST_ST_SENSOR_FAIL;
}

static struct i2c_test_case_info gLIS3DSHTestCaseInfo[] =
{
     __I2C_STRESS_TEST_CASE_ATTR(TestLIS3DSHSensorI2C),
};
#endif

void notify_st_sensor_lowpowermode(int low)
{
	sensor_debug(DEBUG_INFO, "[lis3dsh] %s: +++: (%s)\n", __func__, low?"enter":"exit");
	if(low) {
		atomic_set(&is_suspend,1);
		enable_irq_wake(g_acc->irq1);
		enable_irq_wake(g_acc->irq2);
		lis3dsh_acc_state_progrs_enable_control(g_acc, LIS3DSH_SM1_EN_SM2_DIS);
	}
	else {
		atomic_set(&is_suspend,0);
		disable_irq_wake(g_acc->irq1);
		disable_irq_wake(g_acc->irq2);
		lis3dsh_acc_state_progrs_enable_control(g_acc, LIS3DSH_SM1_EN_SM2_EN);
	}
	sensor_debug(DEBUG_INFO, "[lis3dsh] %s: --- : (%s)\n", __func__, low?"enter":"exit");
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

static int lis3dsh_acc_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct lis3dsh_acc_data *acc;

	u32 smbus_func = I2C_FUNC_SMBUS_BYTE_DATA |
			I2C_FUNC_SMBUS_WORD_DATA | I2C_FUNC_SMBUS_I2C_BLOCK ;

	int err = -1;

	dev_info(&client->dev, "probe start.\n");

	err = lis3dsh_pwr_ctrl(&client->dev, 1);
	if(err)
		goto exit_check_functionality_failed;

	acc = kzalloc(sizeof(struct lis3dsh_acc_data), GFP_KERNEL);
	if (acc == NULL) {
		err = -ENOMEM;
		dev_err(&client->dev,
				"failed to allocate memory for module data: "
					"%d\n", err);
		goto exit_check_functionality_failed;
	}

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

	acc->tilt_status = 1;		//init tilt status to "non-flat"
	acc->client = client;
	i2c_set_clientdata(client, acc);

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

	g_acc = acc;

	err = lis3dsh_acc_validate_pdata(acc);
	if (err < 0) {
		dev_err(&client->dev, "failed to validate platform data\n");
		goto exit_kfree_pdata;
	}

	if (acc->pdata->init) {
		err = acc->pdata->init();
		if (err < 0) {
			dev_err(&client->dev, "init failed: %d\n", err);
			goto err_pdata_init;
		}
	}

	//lis3dsh_class = class_create(THIS_MODULE, LIS3DSH_ACC_DEV_NAME);
	//err = kobject_set_name(&client->dev->kobj, "%s", &client->dev->name);

	#if 0//defined(CONFIG_HAS_EARLYSUSPEND)
	register_early_suspend(&lis3dsh_early_suspend_handler);
	//#elif defined(CONFIG_FB)
	lis3dsh_fb_notif.notifier_call = lis3dsh_fb_notifier_callback;
	err= fb_register_client(&lis3dsh_fb_notif);
	if (err)
		dev_err(&client->dev, "Unable to register fb_notifier: %d\n", err);
	#endif

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
	
	err = create_sysfs_interfaces(&client->dev);
	if (err < 0) {
		dev_err(&client->dev,
		   "[lis3dsh] device LIS3DSH_ACC_DEV_NAME sysfs register failed\n");
		goto err_input_cleanup;
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

	atomic_set(&is_suspend,0);
	mutex_unlock(&acc->lock);
	chip_status=1;			//ASUS_BSP +++ Maggie_Lee "Support ATD BMMI"

	lis3dsh_acc_enable(acc);			//default on

	lis3dsh_acc_state_progrs_enable_control(acc, LIS3DSH_SM1_EN_SM2_EN);

	#ifdef CONFIG_I2C_STRESS_TEST
	i2c_add_test_case(client, "STSensorTest", ARRAY_AND_SIZE(gLIS3DSHTestCaseInfo));
	#endif

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
	remove_sysfs_interfaces(&client->dev);
err_input_cleanup:
	lis3dsh_acc_input_cleanup(acc);
err_power_off:
	lis3dsh_acc_device_power_off(acc);
err_pdata_init:
	if (acc->pdata->exit)
		acc->pdata->exit();
exit_kfree_pdata:
	kfree(acc->pdata);
err_mutexunlock:
	mutex_unlock(&acc->lock);
//err_freedata:
	kfree(acc);
exit_check_functionality_failed:
	pr_err("[lis3dsh] %s: Driver Init failed\n", __func__);
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

	if (atomic_cmpxchg(&acc->enabled, 1, 0) && acc->report_event_en)
		cancel_delayed_work_sync(&acc->input_work);

	lis3dsh_acc_device_power_off(acc);
	lis3dsh_acc_input_cleanup(acc);
	remove_sysfs_interfaces(&client->dev);

	if (acc->pdata->exit)
		acc->pdata->exit();

	kfree(acc->pdata);
	kfree(acc);

	chip_status=0;			//ASUS_BSP +++ Maggie_Lee "Support ATD BMMI"
	return 0;
}

#ifdef CONFIG_PM
static int lis3dsh_acc_resume(struct i2c_client *client)
{
	#if 0
	struct lis3dsh_acc_data *acc = i2c_get_clientdata(client);

	if (acc->on_before_suspend)
		return lis3dsh_acc_enable(acc);

	return 0;
	#endif
	
	return 0;
}

static int lis3dsh_acc_suspend(struct i2c_client *client, pm_message_t mesg)
{
	#if 0
	struct lis3dsh_acc_data *acc = i2c_get_clientdata(client);

	acc->on_before_suspend = atomic_read(&acc->enabled);
	return lis3dsh_acc_disable(acc);
	#endif
	
	return 0;
}
#else
#define lis3dsh_acc_suspend	NULL
#define lis3dsh_acc_resume	NULL
#endif /* CONFIG_PM */

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
	.remove = lis3dsh_acc_remove,
	.suspend = lis3dsh_acc_suspend,
	.resume = lis3dsh_acc_resume,
	.id_table = lis3dsh_acc_id,
};

// ASUS_BSP +++ Maggie_Lee "Detect uevent from gsensor for double tap"
static ssize_t show_event_status(struct device *device, struct device_attribute *attr, char *buf)
{
	ssize_t ret = snprintf(buf, PAGE_SIZE, "%s\n", event_status);
	memset(&event_status[0], 0, sizeof(event_status));
	return ret;
}
// ASUS_BSP --- Maggie_Lee "Detect uevent from gsensor for double tap"

static struct device_attribute lis3dsh_attrs[] = {
	__ATTR(event_status, S_IRUGO, show_event_status, NULL),
};

static int __init lis3dsh_acc_init(void)
{
	dev_t dev = MKDEV(lis3dsh_major, 0);

	sensor_debug(DEBUG_INFO, "%s accelerometer driver: init\n", LIS3DSH_ACC_DEV_NAME);

	lis3dsh_major = MAJOR(dev);

	lis3dsh_class = class_create(THIS_MODULE, LIS3DSH_ACC_DEV_NAME);
	if(IS_ERR(lis3dsh_class)) {
		sensor_debug(DEBUG_INFO, "Err: failed in creating class.\n");
		goto error;
	}

	lis3dsh_dev = MKDEV(lis3dsh_major, lis3dsh_minor);
	lis3dsh_class_dev = device_create(lis3dsh_class, NULL, lis3dsh_dev, NULL, LIS3DSH_ACC_DEV_NAME);
	if(lis3dsh_class_dev == NULL)
	{
		sensor_debug(DEBUG_INFO, "Err: failed in creating device.\n");
		goto error;
	}
	memset(&event_status[0], 0, sizeof(event_status));
	device_create_file(lis3dsh_class_dev, &lis3dsh_attrs[0]);

	return i2c_add_driver(&lis3dsh_acc_driver);

	error:
	return -1;
}

static void __exit lis3dsh_acc_exit(void)
{
	sensor_debug(DEBUG_INFO, "[maggie] %s accelerometer driver: exit\n", LIS3DSH_ACC_DEV_NAME);

	// unregister class
	device_destroy(lis3dsh_class, lis3dsh_dev);
	class_destroy(lis3dsh_class);
	i2c_del_driver(&lis3dsh_acc_driver);
	return;
}

module_init(lis3dsh_acc_init);
module_exit(lis3dsh_acc_exit);

MODULE_DESCRIPTION("lis3dsh accelerometer driver");
MODULE_AUTHOR("ASUS");
MODULE_LICENSE("GPL");

