/******************** (C) COPYRIGHT 2012 STMicroelectronics ********************
 *
 * File Name		: lis3dsh_acc.c
 * Authors		: MSH - Motion Mems BU - Application Team
 *			: Matteo Dameno (matteo.dameno@st.com)
 *			: Denis Ciocca (denis.ciocca@st.com)
 *			: Author is willing to be considered the contact
 *			: and update point for the driver.
 * Version		: V.1.0.0
 * Date			: 2012/08/01
 * Description		: LIS3DSH accelerometer driver
 *
 *******************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THE PRESENT SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES
 * OR CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, FOR THE SOLE
 * PURPOSE TO SUPPORT YOUR APPLICATION DEVELOPMENT.
 * AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY DIRECT,
 * INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM THE
 * CONTENT OF SUCH SOFTWARE AND/OR THE USE MADE BY CUSTOMERS OF THE CODING
 * INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
 *
 * THIS SOFTWARE IS SPECIFICALLY DESIGNED FOR EXCLUSIVE USE WITH ST PARTS.
 *
 ******************************************************************************
Version History.
	V 1.0.0		First Release
 ******************************************************************************/

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
#include	<linux/irq.h>
#include	<linux/gpio.h>
#include	<linux/interrupt.h>
#include	<linux/slab.h>
#include	<linux/of.h>
#include	<linux/of_gpio.h>
#include	<linux/regulator/consumer.h>
#include	<linux/input/lis3dsh.h>
#include	<linux/mutex.h>
#include	<linux/wakelock.h>

/* TILT_WAKELOCK_HOLD_MS defines time to hold wakelock to allow receiver of
 * tilt event to grab their own wakelock
 */
#define TILT_WAKELOCK_HOLD_MS	(HZ / 2)
#define TILT_LOCK_NAME		"tilt_detect"

#define LOAD_STATE_PROGRAM1
#define LOAD_SP1_PARAMETERS
/*
#define LOAD_STATE_PROGRAM2
#define LOAD_SP2_PARAMETERS
*/

#define G_MAX			23920640	/* ug */
#define	I2C_RETRY_DELAY		5		/* Waiting for signals [ms] */
#define	I2C_RETRIES		5		/* Number of retries */
#define	I2C_AUTO_INCREMENT	0x80		/* Autoincrement i2c address */

#define SENSITIVITY_2G		60		/* ug/LSB	*/
#define SENSITIVITY_4G		120		/* ug/LSB	*/
#define SENSITIVITY_6G		180		/* ug/LSB	*/
#define SENSITIVITY_8G		240		/* ug/LSB	*/
#define SENSITIVITY_16G		730		/* ug/LSB	*/

#define	LIS3DSH_ACC_FS_MASK	(0x38)

/* Registers configuration Mask and settings */
/* CTRLREG1 */
#define LIS3DSH_INTEN_MASK		0x01
#define LIS3DSH_INTEN_OFF		0x00
#define LIS3DSH_INTEN_ON		0x01

/* CTRLREG2 */
#define LIS3DSH_HIST1_MASK		0xE0
#define LIS3DSH_SM1INT_PIN_MASK		0x08
#define LIS3DSH_SM1INT_PINB		0x08
#define LIS3DSH_SM1INT_PINA		0x00
#define LIS3DSH_SM1_EN_MASK		0x01
#define LIS3DSH_SM1_EN_ON		0x01
#define LIS3DSH_SM1_EN_OFF		0x00
/* */

/* CTRLREG3 */
#define LIS3DSH_HIST2_MASK		0xE0
#define LIS3DSH_SM2INT_PIN_MASK		0x08
#define LIS3DSH_SM2INT_PINB		0x08
#define LIS3DSH_SM2INT_PINA		0x00
#define LIS3DSH_SM2_EN_MASK		0x01
#define LIS3DSH_SM2_EN_ON		0x01
#define LIS3DSH_SM2_EN_OFF		0x00
/* */

/* CTRLREG4 */
#define LIS3DSH_INT_ACT_MASK		(0x01 << 6)
#define LIS3DSH_INT_ACT_H		(0x01 << 6)
#define LIS3DSH_INT_ACT_L		0x00

#define LIS3DSH_INT_LATCH_MASK		(0x01 << 5)
#define LIS3DSH_INT_PULSED		(0x01 << 5)
#define LIS3DSH_INT_LATCHED		0x00

#define LIS3DSH_INT2_EN_MASK		(0x01 << 4)
#define LIS3DSH_INT2_EN_ON		(0x01 << 4)
#define LIS3DSH_INT2_EN_OFF		0x00

#define LIS3DSH_INT1_EN_MASK		(0x01 << 3)
#define LIS3DSH_INT1_EN_ON		(0x01 << 3)
#define LIS3DSH_INT1_EN_OFF		0x00
/* */

#define	OUT_AXISDATA_REG		LIS3DSH_OUTX_L
#define WHOAMI_LIS3DSH_ACC		0x3F	/* Expected content for WAI */

/*	CONTROL REGISTERS	*/
#define	LIS3DSH_WHO_AM_I		0x0F	/* WhoAmI register Address */

#define	LIS3DSH_OUTX_L			0x28	/* Output X LSByte */
#define	LIS3DSH_OUTX_H			0x29	/* Output X MSByte */
#define	LIS3DSH_OUTY_L			0x2A	/* Output Y LSByte */
#define	LIS3DSH_OUTY_H			0x2B	/* Output Y MSByte */
#define	LIS3DSH_OUTZ_L			0x2C	/* Output Z LSByte */
#define	LIS3DSH_OUTZ_H			0x2D	/* Output Z MSByte */
#define	LIS3DSH_LC_L			0x16	/* LSByte Long Counter Status */
#define	LIS3DSH_LC_H			0x17	/* MSByte Long Counter Status */

#define	LIS3DSH_INT_STATUS_REG		0x18	/* Interrupt Status */

#define	LIS3DSH_STATUS_REG		0x27	/* Status */
#define	LIS3DSH_STAT_ZYXOR	(0x01 << 7)	/* X,Y,Z-axis Data Overrun */
#define	LIS3DSH_STAT_ZOR	(0x01 << 6)	/* Z-axis Data Overrun */
#define	LIS3DSH_STAT_YOR	(0x01 << 5)	/* Y-axis Data Overrun */
#define	LIS3DSH_STAT_XOR	(0x01 << 4)	/* X-axis Data Overrun */
#define	LIS3DSH_STAT_ZYXDA	(0x01 << 3)	/* X,Y,Z-axis Data Avail */
#define	LIS3DSH_STAT_ZDA	(0x01 << 2)	/* Z-axis Data Avail */
#define	LIS3DSH_STAT_YDA	(0x01 << 1)	/* Y-axis Data Avail */
#define	LIS3DSH_STAT_XDA	(0x01 << 0)	/* X-axis Data Avail */

#define	LIS3DSH_CTRL_REG1		0x21	/* control reg 1 */
#define	LIS3DSH_CTRL_REG2		0x22	/* control reg 2 */
#define	LIS3DSH_CTRL_REG3		0x23	/* control reg 3 */
#define	LIS3DSH_CTRL_REG4		0x20	/* control reg 4 */
#define	LIS3DSH_CTRL_REG5		0x24	/* control reg 5 */
#define	LIS3DSH_CTRL_REG6		0x25	/* control reg 6 */

	/* CTRL_REG4 Fields */
#define	LIS3DSH_BDU_CONTINUOUS		(1 << 3)/* continuous update */
#define	LIS3DSH_X_AXIS_ENABLE		(1 << 0)/* X-Axis enable */
#define	LIS3DSH_Y_AXIS_ENABLE		(1 << 1)/* Y-Axis enable */
#define	LIS3DSH_Z_AXIS_ENABLE		(1 << 2)/* Z-Axis enable */
#define	LIS3DSH_ALL_AXES_ENABLE		(LIS3DSH_X_AXIS_ENABLE | \
					LIS3DSH_Y_AXIS_ENABLE | \
					LIS3DSH_Z_AXIS_ENABLE)

	/* Output Data Rates ODR */
#define	LIS3DSH_ODR_MASK	0XF0
#define	LIS3DSH_PM_OFF		0x00		/* OFF */
#define	LIS3DSH_ODR3_125	0x10		/*    3.125 Hz */
#define	LIS3DSH_ODR6_25		0x20		/*    6.25  Hz */
#define	LIS3DSH_ODR12_5		0x30		/*   12.5   Hz */
#define	LIS3DSH_ODR25		0x40		/*   25     Hz */
#define	LIS3DSH_ODR50		0x50		/*   50     Hz */
#define	LIS3DSH_ODR100		0x60		/*  100     Hz */
#define	LIS3DSH_ODR400		0x70		/*  400     Hz */
#define	LIS3DSH_ODR800		0x80		/*  800     Hz */
#define	LIS3DSH_ODR1600		0x90		/* 1600     Hz */

	/* MASK1_A Fields */
#define	LIS3DSH_P_X		(1 << 7)	/* 0=X+ disabled, 1 = enabled */
#define	LIS3DSH_N_X		(1 << 6)	/* 0=X- disabled, 1 = enabled */
#define	LIS3DSH_P_Y		(1 << 5)	/* 0=Y+ disabled, 1 = enabled */
#define	LIS3DSH_N_Y		(1 << 4)	/* 0=Y- disabled, 1 = enabled */
#define	LIS3DSH_P_Z		(1 << 3)	/* 0=Z+ disabled, 1 = enabled */
#define	LIS3DSH_N_Z		(1 << 2)	/* 0=Z- disabled, 1 = enabled */
#define	LIS3DSH_P_V		(1 << 1)	/* 0=V+ disabled, 1 = enabled */
#define	LIS3DSH_N_V		(1 << 0)	/* 0=V- disabled, 1 = enabled */

	/* SETT Fields */
#define	LIS3DSH_SM1_P_DET	(1 << 7)	/* SM1 peak detection */
#define	LIS3DSH_THR3_SA		(1 << 6)	/* Threshold 3 enable MASKB_1 */
#define	LIS3DSH_ABS		(1 << 5)	/* 0 = unsigned thresholds */
#define	LIS3DSH_THR3_MA		(1 << 2)	/* Threshold 3 enable MASKA_1 */
#define	LIS3DSH_R_TAM		(1 << 1)	/* R_TAM		*/
#define	LIS3DSH_SITR		(1 << 0)	/* STOP & CONT generate ints */

	/* default init values */
#define	LIS3DSH_DFLT_DATA_RATE		LIS3DSH_ODR100
#define	LIS3DSH_DFLT_SA_1		LIS3DSH_P_Z
#define	LIS3DSH_DFLT_MA_1		LIS3DSH_P_Z
#define	LIS3DSH_DFLT_SETT_1		(LIS3DSH_ABS | LIS3DSH_R_TAM | \
					LIS3DSH_SITR)
#define	LIS3DSH_DFLT_THRS2_1		0xD0
#define	LIS3DSH_DFLT_THRS1_1		0x30

#define	LIS3DSH_OFF_X			0x10	/* Offset X Corr */
#define	LIS3DSH_OFF_Y			0x11	/* Offset Y Corr */
#define	LIS3DSH_OFF_Z			0x12	/* Offset Z Corr */

#define	LIS3DSH_CS_X			0x13	/* Const Shift X */
#define	LIS3DSH_CS_Y			0x14	/* Const Shift Y */
#define	LIS3DSH_CS_Z			0x15	/* Const Shift Z */

#define	LIS3DSH_VFC_1			0x1B	/* Vect Filter Coeff 1 */
#define	LIS3DSH_VFC_2			0x1C	/* Vect Filter Coeff 2 */
#define	LIS3DSH_VFC_3			0x1D	/* Vect Filter Coeff 3 */
#define	LIS3DSH_VFC_4			0x1E	/* Vect Filter Coeff 4 */

/* LIS3DSH State Machine Opcodes */
#define	NOP		0x00	/* No operation */
#define	TI1_OPCODE	0x01	/* Timer 1 (8-bit value) valid */
#define	TI2_OPCODE	0x02	/* Timer 2 (8-bit value) valid */
#define	TI3_OPCODE	0x03	/* Timer 3 (8-bit value) valid */
#define	TI4_OPCODE	0x04	/* Timer 4 (8-bit value) valid */
#define	GNTH1_OPCODE	0x05	/* Any/triggered axis > THRS1 */
#define	GNTH2_OPCODE	0x06	/* Any/triggered axis > THRS2 */
#define	LNTH1_OPCODE	0x07	/* Any/triggered axis <= THRS1 */
#define	LNTH2_OPCODE	0x08	/* Any/triggered axis <= THRS2 */
#define	GTTH1_OPCODE	0x09	/* Any/triggered axis > THRS1 but using MASK1 */
#define	LLTH2_OPCODE	0x0A	/* All axis <= to THRS2 */
#define	GRTH1_OPCODE	0x0B	/* Any/triggered axis > reversed THRS1 */
#define	LRTH1_OPCODE	0x0C	/* Any/triggered axis <= reversed THRS1 */
#define	GRTH2_OPCODE	0x0D	/* Any/triggered axis > reversed THRS2 */
#define	LRTH2_OPCODE	0x0E	/* Any/triggered axis <= reversed THRS2 */
#define	NZERO_OPCODE	0x0F	/* Any axis zero crossed */

/* LIS3DSH State Machine Commands */
#define	STOP_CMD	0x00	/* Stops execution, and resets to start */
#define	CONT_CMD	0x11	/* Continues execution from RESET- POINT */
#define	JMP_CMD		0x22
#define	SRP_CMD		0x33
#define	CRP_CMD		0x44
#define	SETP_CMD	0x55
#define	SETS1_CMD	0x66
#define	STHR1_CMD	0x77
#define	OUTC_CMD	0x88
#define	OUTW_CMD	0x99
#define	STHR2_CMD	0xAA
#define	DEC_CMD		0xBB
#define	SISW_CMD	0xCC
#define	REL_CMD		0xDD
#define	STHR3_CMD	0xEE
#define	SSYNC_CMD	0xFF

#define	SABS0_CMD	0x12	/* Set SETTy(ABS=0) (unsigned threshold) */
#define	SABS1_CMD	0x13	/* Set SETTy(ABS=1) (signed threshold) */
#define	SELMA_CMD	0x14	/* Set mask to MASK_A */
#define	SELSA_CMD	0x24	/* Set mask to MASK_B */

	/* state program 1 */
#define	LIS3DSH_STATEPR1	0X40	/*	State Program 1 16 bytes */

#define	LIS3DSH_TIM4_1		0X50	/*	SPr1 Timer4		*/
#define	LIS3DSH_TIM3_1		0X51	/*	SPr1 Timer3		*/
#define	LIS3DSH_TIM2_1		0X52	/*	SPr1 Timer2	2bytes	*/
#define	LIS3DSH_TIM1_1		0X54	/*	SPr1 Timer1	2bytes	*/

#define	LIS3DSH_THRS2_1		0X56	/*	SPr1 Threshold1		*/
#define	LIS3DSH_THRS1_1		0X57	/*	SPr1 Threshold2		*/
#define	LIS3DSH_SA_1		0X59	/*	SPr1 Swap Axis Sign Msk	*/
#define	LIS3DSH_MA_1		0X5A	/*	SPr1 Axis Sign Msk	*/
#define	LIS3DSH_SETT_1		0X5B	/*	SPr1			*/
#define	LIS3DSH_PPRP_1		0X5C	/*	SPr1 ProgPointer ResetPointer */
#define	LIS3DSH_TC_1		0X5D	/*	SPr1		2bytes	*/
#define	LIS3DSH_OUTS_1		0X5F	/*	SPr1			*/

	/* state program 2 */
#define	LIS3DSH_STATEPR2	0X60	/*	State Program 2 16 bytes */

#define	LIS3DSH_TIM4_2		0X70	/*	SPr2 Timer4		*/
#define	LIS3DSH_TIM3_2		0X71	/*	SPr2 Timer3		*/
#define	LIS3DSH_TIM2_2		0X72	/*	SPr2 Timer2	2bytes	*/
#define	LIS3DSH_TIM1_2		0X74	/*	SPr2 Timer1	2bytes	*/

#define	LIS3DSH_THRS2_2		0X76	/*	SPr2 Threshold1		*/
#define	LIS3DSH_THRS1_2		0X77	/*	SPr2 Threshold2		*/
#define	LIS3DSH_DES_2		0X78	/*	SPr2 Decimation		*/
#define	LIS3DSH_SA_2		0X79	/*	SPr2 Swap Axis Sign Msk	*/
#define	LIS3DSH_MA_2		0X7A	/*	SPr2 Axis Sign Msk	*/
#define	LIS3DSH_SETT_2		0X7B	/*	SPr2			*/
#define	LIS3DSH_PPRP_2		0X7C	/*	SPr2 ProgPointer ResetPointer */
#define	LIS3DSH_TC_2		0X7D	/*	SPr2		2bytes	*/
#define	LIS3DSH_OUTS_2		0X7F	/*	SPr2			*/
/*	end CONTROL REGISTRES	*/


/* RESUME STATE INDICES */
#define	LIS3DSH_RES_LC_L			0
#define	LIS3DSH_RES_LC_H			1

#define	LIS3DSH_RES_CTRL_REG1			2
#define	LIS3DSH_RES_CTRL_REG2			3
#define	LIS3DSH_RES_CTRL_REG3			4
#define	LIS3DSH_RES_CTRL_REG4			5
#define	LIS3DSH_RES_CTRL_REG5			6

#define	LIS3DSH_RES_TIM4_1			20
#define	LIS3DSH_RES_TIM3_1			21
#define	LIS3DSH_RES_TIM2_1_L			22
#define	LIS3DSH_RES_TIM2_1_H			23
#define	LIS3DSH_RES_TIM1_1_L			24
#define	LIS3DSH_RES_TIM1_1_H			25

#define	LIS3DSH_RES_THRS2_1			26
#define	LIS3DSH_RES_THRS1_1			27
#define	LIS3DSH_RES_SA_1			28
#define	LIS3DSH_RES_MA_1			29
#define	LIS3DSH_RES_SETT_1			30

#define	LIS3DSH_RES_TIM4_2			31
#define	LIS3DSH_RES_TIM3_2			32
#define	LIS3DSH_RES_TIM2_2_L			33
#define	LIS3DSH_RES_TIM2_2_H			34
#define	LIS3DSH_RES_TIM1_2_L			35
#define	LIS3DSH_RES_TIM1_2_H			36

#define	LIS3DSH_RES_THRS2_2			37
#define	LIS3DSH_RES_THRS1_2			38
#define	LIS3DSH_RES_DES_2			39
#define	LIS3DSH_RES_SA_2			40
#define	LIS3DSH_RES_MA_2			41
#define	LIS3DSH_RES_SETT_2			42

#define	LIS3DSH_RESUME_ENTRIES			43



#define	LIS3DSH_STATE_PR_SIZE			16
/* end RESUME STATE INDICES */

/* STATE PROGRAMS ENABLE CONTROLS */
#define	LIS3DSH_SM1_DIS_SM2_DIS			0x00
#define	LIS3DSH_SM1_DIS_SM2_EN			0x01
#define	LIS3DSH_SM1_EN_SM2_DIS			0x02
#define	LIS3DSH_SM1_EN_SM2_EN			0x03

/* INTERRUPTS ENABLE CONTROLS */
#define	LIS3DSH_INT1_DIS_INT2_DIS		0x00
#define	LIS3DSH_INT1_DIS_INT2_EN		0x01
#define	LIS3DSH_INT1_EN_INT2_DIS		0x02
#define	LIS3DSH_INT1_EN_INT2_EN			0x03

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

struct lis3dsh_acc_private_data {
	u32 poll_interval;
	u32 min_interval;
	u8 fs_range;
	u8 axis_map_x;
	u8 axis_map_y;
	u8 axis_map_z;
	u8 negate_x;
	u8 negate_y;
	u8 negate_z;
	int gpio_int1;
	int gpio_int2;
	int gpio_dsp_int1;
	struct regulator *vdd_io_regulator;
	struct regulator *vcc_i2c_regulator;
	struct regulator *vdd_regulator;
};

struct lis3dsh_acc_data {
	struct i2c_client *client;
	struct lis3dsh_acc_private_data *pdata;

	struct mutex lock;
	struct delayed_work input_work;

	struct input_dev *input_dev;

	int hw_initialized;
	/* hw_working=-1 means not tested yet */
	int hw_working;
	int enabled;
	int powered;
	int on_before_suspend;

	u16 sensitivity;
	u8 stateprogs_enable_setting;
	u8 interrupts_enable_setting;

	u8 resume_state[LIS3DSH_RESUME_ENTRIES];
	u8 resume_stmach_program1[LIS3DSH_STATE_PR_SIZE];
	u8 resume_stmach_program2[LIS3DSH_STATE_PR_SIZE];

	int irq1;
	int irq2;

	struct wake_lock tilt_wakelock;

	/* counts number of times device has gone into TILT */
	u32 tilt_count;

	atomic_t int1_enabled;
	atomic_t int2_enabled;
	u8 reg_addr;
	bool report_events;
};

/* Local Prototypes */
static int lis3dsh_regulator_enable(struct lis3dsh_acc_data *acc,
		bool enable);
static int lis3dsh_acc_get_acceleration_data(struct lis3dsh_acc_data *acc,
		int *xyz);
static void report_motion(struct lis3dsh_acc_data *acc);
static int lis3dsh_acc_disable_l(struct lis3dsh_acc_data *acc);

/* sets default init values to be written in registers at probe stage */
static void lis3dsh_acc_set_init_register_values(struct lis3dsh_acc_data *acc)
{
	acc->resume_state[LIS3DSH_RES_LC_L] = 0x00;
	acc->resume_state[LIS3DSH_RES_LC_H] = 0x00;

	acc->resume_state[LIS3DSH_RES_CTRL_REG1] = LIS3DSH_INT_ACT_H;
	acc->resume_state[LIS3DSH_RES_CTRL_REG4] = 0x00;
	acc->resume_state[LIS3DSH_RES_CTRL_REG3] = 0x00;
	acc->resume_state[LIS3DSH_RES_CTRL_REG4] = (LIS3DSH_DFLT_DATA_RATE |
						LIS3DSH_ALL_AXES_ENABLE);
	acc->resume_state[LIS3DSH_RES_CTRL_REG5] = 0x00;
}

static void lis3dsh_acc_set_init_statepr1_inst(struct lis3dsh_acc_data *acc)
{
#ifdef LOAD_STATE_PROGRAM1
	acc->resume_stmach_program1[0] = LNTH1_OPCODE;
	acc->resume_stmach_program1[1] = LNTH2_OPCODE;
	acc->resume_stmach_program1[2] = GNTH2_OPCODE;
	acc->resume_stmach_program1[3] = GNTH1_OPCODE;
	acc->resume_stmach_program1[4] = CONT_CMD;
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
#else
	acc->resume_stmach_program1[0] = 0x00;
	acc->resume_stmach_program1[1] = 0x00;
	acc->resume_stmach_program1[2] = 0X00;
	acc->resume_stmach_program1[3] = 0X00;
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
#endif
}

static void lis3dsh_acc_set_init_statepr2_inst(struct lis3dsh_acc_data *acc)
{
	acc->resume_stmach_program2[0] = 0x00;
	acc->resume_stmach_program2[1] = 0x00;
	acc->resume_stmach_program2[2] = 0X00;
	acc->resume_stmach_program2[3] = 0X00;
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
}

static void lis3dsh_acc_set_init_statepr1_param(struct lis3dsh_acc_data *acc)
{
#ifdef	LOAD_SP1_PARAMETERS
	acc->resume_state[LIS3DSH_RES_TIM4_1] = 0x00;
	acc->resume_state[LIS3DSH_RES_TIM3_1] = 0x00;
	acc->resume_state[LIS3DSH_RES_TIM2_1_L] = 0x00;
	acc->resume_state[LIS3DSH_RES_TIM2_1_H] = 0x00;
	acc->resume_state[LIS3DSH_RES_TIM1_1_L] = 0x00;
	acc->resume_state[LIS3DSH_RES_TIM1_1_H] = 0x00;
	acc->resume_state[LIS3DSH_RES_THRS2_1] = LIS3DSH_DFLT_THRS2_1;
	acc->resume_state[LIS3DSH_RES_THRS1_1] = LIS3DSH_DFLT_THRS1_1;
	/* DES1 not available*/
	acc->resume_state[LIS3DSH_RES_SA_1] = LIS3DSH_DFLT_SA_1;
	acc->resume_state[LIS3DSH_RES_MA_1] = LIS3DSH_DFLT_MA_1;
	acc->resume_state[LIS3DSH_RES_SETT_1] = LIS3DSH_DFLT_SETT_1;
#else
	acc->resume_state[LIS3DSH_RES_TIM4_1] = 0x00;
	acc->resume_state[LIS3DSH_RES_TIM3_1] = 0x00;
	acc->resume_state[LIS3DSH_RES_TIM2_1_L] = 0x00;
	acc->resume_state[LIS3DSH_RES_TIM2_1_H] = 0x00;
	acc->resume_state[LIS3DSH_RES_TIM1_1_L] = 0x00;
	acc->resume_state[LIS3DSH_RES_TIM1_1_H] = 0x00;
	acc->resume_state[LIS3DSH_RES_THRS2_1] = 0x00;
	acc->resume_state[LIS3DSH_RES_THRS1_1] = 0x00;
	/* DES1 not available*/
	acc->resume_state[LIS3DSH_RES_SA_1] = 0x00;
	acc->resume_state[LIS3DSH_RES_MA_1] = 0x00;
	acc->resume_state[LIS3DSH_RES_SETT_1] = 0x00;
#endif
}

static void lis3dsh_acc_set_init_statepr2_param(struct lis3dsh_acc_data *acc)
{
	acc->resume_state[LIS3DSH_RES_TIM4_2] = 0x00;
	acc->resume_state[LIS3DSH_RES_TIM3_2] = 0x00;
	acc->resume_state[LIS3DSH_RES_TIM2_2_L] = 0x00;
	acc->resume_state[LIS3DSH_RES_TIM2_2_H] = 0x00;
	acc->resume_state[LIS3DSH_RES_TIM1_2_L] = 0x00;
	acc->resume_state[LIS3DSH_RES_TIM1_2_H] = 0x00;
	acc->resume_state[LIS3DSH_RES_THRS2_2] = 0x00;
	acc->resume_state[LIS3DSH_RES_THRS1_2] = 0x00;
	acc->resume_state[LIS3DSH_RES_DES_2] = 0x00;
	acc->resume_state[LIS3DSH_RES_SA_2] = 0x00;
	acc->resume_state[LIS3DSH_RES_MA_2] = 0x00;
	acc->resume_state[LIS3DSH_RES_SETT_2] = 0x00;
}

static int lis3dsh_acc_i2c_read(struct lis3dsh_acc_data *acc,
				u8 *buf, int len)
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

static int lis3dsh_acc_i2c_write(struct lis3dsh_acc_data *acc, u8 *buf, int len)
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

	pr_info("%s: hw init start\n", LIS3DSH_ACC_DEV_NAME);

	buf[0] = LIS3DSH_WHO_AM_I;
	err = lis3dsh_acc_i2c_read(acc, buf, 1);
	if (err < 0) {
		dev_warn(&acc->client->dev, "Error reading WHO_AM_I: is device "
			"available/working?\n");
		goto err_firstread;
	} else
		acc->hw_working = 1;

	if (buf[0] != WHOAMI_LIS3DSH_ACC) {
		dev_err(&acc->client->dev, "device unknown. Expected: 0x%x,"
			" Replies: 0x%x\n", WHOAMI_LIS3DSH_ACC, buf[0]);
		err = -1; /* choose the right coded error */
		goto err_unknown_device;
	}


	buf[0] = (I2C_AUTO_INCREMENT | LIS3DSH_LC_L);
	buf[1] = acc->resume_state[LIS3DSH_RES_LC_L];
	buf[2] = acc->resume_state[LIS3DSH_RES_LC_H];
	err = lis3dsh_acc_i2c_write(acc, buf, 2);
	if (err < 0)
		goto err_resume_state;

	buf[0] = (I2C_AUTO_INCREMENT | LIS3DSH_TIM4_1);
	buf[1] = acc->resume_state[LIS3DSH_RES_TIM4_1];
	buf[2] = acc->resume_state[LIS3DSH_RES_TIM3_1];
	buf[3] = acc->resume_state[LIS3DSH_RES_TIM2_1_L];
	buf[4] = acc->resume_state[LIS3DSH_RES_TIM2_1_H];
	buf[5] = acc->resume_state[LIS3DSH_RES_TIM1_1_L];
	buf[6] = acc->resume_state[LIS3DSH_RES_TIM1_1_H];
	buf[7] = acc->resume_state[LIS3DSH_RES_THRS2_1];
	buf[8] = acc->resume_state[LIS3DSH_RES_THRS1_1];
	err = lis3dsh_acc_i2c_write(acc, buf, 8);
	if (err < 0)
		goto err_resume_state;

	buf[0] = (I2C_AUTO_INCREMENT | LIS3DSH_SA_1);
	buf[1] = acc->resume_state[LIS3DSH_RES_SA_1];
	buf[2] = acc->resume_state[LIS3DSH_RES_MA_1];
	buf[3] = acc->resume_state[LIS3DSH_RES_SETT_1];
	err = lis3dsh_acc_i2c_write(acc, buf, 3);
	if (err < 0)
		goto err_resume_state;

	buf[0] = (I2C_AUTO_INCREMENT | LIS3DSH_TIM4_2);
	buf[1] = acc->resume_state[LIS3DSH_RES_TIM4_2];
	buf[2] = acc->resume_state[LIS3DSH_RES_TIM3_2];
	buf[3] = acc->resume_state[LIS3DSH_RES_TIM2_2_L];
	buf[4] = acc->resume_state[LIS3DSH_RES_TIM2_2_H];
	buf[5] = acc->resume_state[LIS3DSH_RES_TIM1_2_L];
	buf[6] = acc->resume_state[LIS3DSH_RES_TIM1_2_H];
	buf[7] = acc->resume_state[LIS3DSH_RES_THRS2_2];
	buf[8] = acc->resume_state[LIS3DSH_RES_THRS1_2];
	buf[9] = acc->resume_state[LIS3DSH_RES_DES_2];
	buf[10] = acc->resume_state[LIS3DSH_RES_SA_2];
	buf[11] = acc->resume_state[LIS3DSH_RES_MA_2];
	buf[12] = acc->resume_state[LIS3DSH_RES_SETT_2];
	err = lis3dsh_acc_i2c_write(acc, buf, 12);
	if (err < 0)
		goto err_resume_state;

	/*	state program 1 */
	buf[0] = (I2C_AUTO_INCREMENT | LIS3DSH_STATEPR1);
	for (i = 1; i <= LIS3DSH_STATE_PR_SIZE; i++) {
		buf[i] = acc->resume_stmach_program1[i-1];
#ifdef DEBUG
		pr_info("i=%d,st pr1 buf[i]=0x%x\n", i, buf[i]);
#endif
	};
	err = lis3dsh_acc_i2c_write(acc, buf, LIS3DSH_STATE_PR_SIZE);
	if (err < 0)
		goto err_resume_state;

	/*	state program 2 */
	buf[0] = (I2C_AUTO_INCREMENT | LIS3DSH_STATEPR2);
	for (i = 1; i <= LIS3DSH_STATE_PR_SIZE; i++) {
		buf[i] = acc->resume_stmach_program2[i-1];
#ifdef DEBUG
		pr_info("i=%d,st pr2 buf[i]=0x%x\n", i, buf[i]);
#endif
	};
	err = lis3dsh_acc_i2c_write(acc, buf, LIS3DSH_STATE_PR_SIZE);
	if (err < 0)
		goto err_resume_state;

	buf[0] = (I2C_AUTO_INCREMENT | LIS3DSH_CTRL_REG2);
	buf[1] = acc->resume_state[LIS3DSH_RES_CTRL_REG2];
	buf[2] = acc->resume_state[LIS3DSH_RES_CTRL_REG3];
	err = lis3dsh_acc_i2c_write(acc, buf, 2);
	if (err < 0)
		goto err_resume_state;

	buf[0] = (I2C_AUTO_INCREMENT | LIS3DSH_CTRL_REG4);
	buf[1] = acc->resume_state[LIS3DSH_RES_CTRL_REG4];
	buf[2] = acc->resume_state[LIS3DSH_RES_CTRL_REG1];
	err = lis3dsh_acc_i2c_write(acc, buf, 2);
	if (err < 0)
		goto err_resume_state;

	acc->hw_initialized = 1;
	pr_info("%s: hw init done\n", LIS3DSH_ACC_DEV_NAME);
	return 0;

err_firstread:
	acc->hw_working = 0;
err_unknown_device:
err_resume_state:
	acc->hw_initialized = 0;
	dev_err(&acc->client->dev, "hw init error 0x%x,0x%x: %d\n", buf[0],
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

	if (acc->pdata->gpio_int1 >= 0) {
		if (atomic_cmpxchg(&acc->int1_enabled, 1, 0))
			disable_irq_nosync(acc->irq1);
	}
	if (acc->pdata->gpio_int2 >= 0) {
		if (atomic_cmpxchg(&acc->int2_enabled, 1, 0))
			disable_irq_nosync(acc->irq2);
	}
	lis3dsh_regulator_enable(acc, false);
	acc->hw_initialized = 0;
}

static int lis3dsh_acc_device_power_on(struct lis3dsh_acc_data *acc)
{
	int err = -1;

	err = lis3dsh_regulator_enable(acc, true);
	if (err < 0) {
		dev_err(&acc->client->dev,
			"power_on failed: %d\n", err);
		return err;
	}

	if (!acc->hw_initialized) {
		err = lis3dsh_acc_hw_init(acc);
		if (acc->hw_working == 1 && err < 0) {
			lis3dsh_acc_device_power_off(acc);
			return err;
		}

		/* lis3dsh_acc_hw_init() sets hw_initialized on success */
		if (!acc->hw_initialized)
			return -EIO;
	}

	return 0;
}

static int lis3dsh_clear_irq1(struct lis3dsh_acc_data *acc)
{
	u8 val = LIS3DSH_OUTS_1;
	return lis3dsh_acc_i2c_read(acc, &val, 1);
}

static int lis3dsh_clear_irq2(struct lis3dsh_acc_data *acc)
{
	u8 val = LIS3DSH_OUTS_2;
	return lis3dsh_acc_i2c_read(acc, &val, 1);
}

static int is_tilt_detect_enabled(struct lis3dsh_acc_data *acc)
{
	/* If external INT1 is enabled, we're setup for TILT detect */
	if (atomic_read(&acc->int1_enabled))
		return 1;
	return 0;
}

static irqreturn_t lis3dsh_acc_isr_threaded(int irq, void *dev)
{
	struct lis3dsh_acc_data *acc = dev;
	int err = 0;
	u8 val = 0;

	mutex_lock(&acc->lock);
	if (is_tilt_detect_enabled(acc))
		dev_info(&acc->client->dev, "DEVICE IN TILT\n");
	val = LIS3DSH_INT_STATUS_REG;
	err = lis3dsh_acc_i2c_read(acc, &val, 1);
	if (err) {
		dev_err(&acc->client->dev, "lis3dsh_acc_i2c_read failed\n");
		goto exit;
	}

	val = LIS3DSH_STATUS_REG;
	err = lis3dsh_acc_i2c_read(acc, &val, 1);
	if (err) {
		dev_err(&acc->client->dev, "get_acceleration_data failed\n");
		goto exit;
	}

	if (val & LIS3DSH_STAT_ZYXDA) {	/* Do we have data available? */
		acc->tilt_count++;
		report_motion(acc);
	}

	/* reading OUTSx register resets SMx interrupt (IRQx) */
	if (irq == acc->irq1)
		val = LIS3DSH_OUTS_1;
	else
		val = LIS3DSH_OUTS_2;
	err = lis3dsh_acc_i2c_read(acc, &val, 1);
	if (err) {
		dev_err(&acc->client->dev,
			"unrecoverable error, shutting down lis3dsh driver.\n");
		lis3dsh_acc_disable_l(acc);
		mutex_unlock(&acc->lock);
		return IRQ_HANDLED;
	}

exit:
	mutex_unlock(&acc->lock);
	return IRQ_HANDLED;
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
	init_val = acc->resume_state[resume_index];
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
		"register 0x%x update failed at step %d, error: %d\n",
			config[0], step, err);
	return err;
}

/* update the sensitivity range, must be called with lock held */
static int lis3dsh_acc_update_fs_range_l(struct lis3dsh_acc_data *acc,
								u8 new_fs_range)
{
	int err = -1;
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

	if (acc->enabled) {
		/* Updates configuration register 1,
		* which contains g range setting */
		err = lis3dsh_acc_register_masked_update(acc, LIS3DSH_CTRL_REG5,
		LIS3DSH_ACC_FS_MASK, new_fs_range, LIS3DSH_RES_CTRL_REG5);
		if (err < 0) {
			dev_err(&acc->client->dev, "update g range failed\n");
			return err;
		} else {
			acc->sensitivity = sensitivity;
		}
	}

	if (err < 0)
		dev_err(&acc->client->dev, "update g range not executed "
						"because the device is off\n");
	return err;
}


/* Update the odr table, must be called with lock held */
static int lis3dsh_acc_update_odr_l(struct lis3dsh_acc_data *acc,
							int poll_interval_ms)
{
	int err = -1;
	int i;
	u8 new_odr;

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
	if (acc->enabled) {
		err = lis3dsh_acc_register_masked_update(acc,
			LIS3DSH_CTRL_REG4, LIS3DSH_ODR_MASK, new_odr,
							LIS3DSH_RES_CTRL_REG4);
	}

	if (err < 0)
		dev_err(&acc->client->dev, "update odr failed\n");
	return err;
}

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

#ifdef DEBUG
/*
	pr_info("%s read x=%d, y=%d, z=%d\n",
			LIS3DSH_ACC_DEV_NAME, xyz[0], xyz[1], xyz[2]);
*/
#endif
	return err;
}

static void lis3dsh_acc_report_values(struct lis3dsh_acc_data *acc,
					int *xyz)
{
	if (acc->report_events) {
		input_report_abs(acc->input_dev, ABS_X, xyz[0]);
		input_report_abs(acc->input_dev, ABS_Y, xyz[1]);
		input_report_abs(acc->input_dev, ABS_Z, xyz[2]);
		input_sync(acc->input_dev);
	} else {
		/* not reporting events, so print values for debug purposes */
		printk(KERN_INFO "x,y,z: %d, %d, %d\n",
					xyz[0], xyz[1], xyz[2]);
	}
}

static void report_motion(struct lis3dsh_acc_data *acc)
{
        if (acc->report_events) {
		/*
		 * Grab or reprime a temporary wakelock to keep system awake
		 * long enough for receiver of tilt event to grab it's own
		 * wakelock
		 */
		wake_lock_timeout(&acc->tilt_wakelock, TILT_WAKELOCK_HOLD_MS);

		/*
		 * Tell the HAL a wrist tilt gesture was recognized
		 */
		input_event(acc->input_dev, EV_MSC, MSC_GESTURE, 1);
		input_sync(acc->input_dev);
	}
}

/* Enable the accelerometer, must be called with lock held */
static int lis3dsh_acc_enable_l(struct lis3dsh_acc_data *acc)
{
	int err;

	if (!acc->enabled) {
		err = lis3dsh_acc_device_power_on(acc);
		if (err < 0)
			return err;

		if (acc->pdata->gpio_int1 >= 0) {
			if (!atomic_cmpxchg(&acc->int1_enabled, 0, 1))
				enable_irq(acc->irq1);
		}

		acc->enabled = 1;
		schedule_delayed_work(&acc->input_work,
			msecs_to_jiffies(acc->pdata->poll_interval));
	}

	return 0;
}

/* Disable the accelerometer, must be called with lock held */
static int lis3dsh_acc_disable_l(struct lis3dsh_acc_data *acc)
{
	if (acc->enabled == 1) {
		cancel_delayed_work_sync(&acc->input_work);
		lis3dsh_acc_device_power_off(acc);
		acc->enabled = 0;
	}

	return 0;
}

static ssize_t attr_get_sensitivity(struct device *dev,
					struct device_attribute *attr,
								char *buf)
{
	int val;
	struct lis3dsh_acc_data *acc = dev_get_drvdata(dev);
	mutex_lock(&acc->lock);
	val = acc->sensitivity;
	mutex_unlock(&acc->lock);
	return snprintf(buf, 16, "%d\n", val);
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
	err = lis3dsh_acc_update_odr_l(acc, interval_ms);
	if (err >= 0)
		acc->pdata->poll_interval = interval_ms;
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
	switch (val) {
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

	switch (val) {
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
		return -EINVAL;
	}

	mutex_lock(&acc->lock);
	err = lis3dsh_acc_update_fs_range_l(acc, range);
	if (err >= 0)
		acc->pdata->fs_range = range;
	mutex_unlock(&acc->lock);
	return size;
}

static ssize_t attr_get_tilt_count(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct lis3dsh_acc_data *acc = dev_get_drvdata(dev);
	return snprintf(buf, 16, "%d\n", acc->tilt_count);
}

static ssize_t attr_set_tilt_count(struct device *dev,
				struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct lis3dsh_acc_data *acc = dev_get_drvdata(dev);
	unsigned long val;

	if (kstrtoul(buf, 10, &val))
		return -EINVAL;
	acc->tilt_count = val;
	return size;
}

static ssize_t attr_get_power_device(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct lis3dsh_acc_data *acc = dev_get_drvdata(dev);
	return snprintf(buf, 16, "%d\n", acc->powered);
}

static ssize_t attr_set_power_device(struct device *dev,
				struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct lis3dsh_acc_data *acc = dev_get_drvdata(dev);
	unsigned long val;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;

	mutex_lock(&acc->lock);
	if (val)
		lis3dsh_acc_device_power_on(acc);
	else
		lis3dsh_acc_device_power_off(acc);
	mutex_unlock(&acc->lock);

	return size;
}


static ssize_t attr_get_enable(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct lis3dsh_acc_data *acc = dev_get_drvdata(dev);
	return snprintf(buf, 16, "%d\n", acc->enabled);
}

static ssize_t attr_set_enable(struct device *dev,
				struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct lis3dsh_acc_data *acc = dev_get_drvdata(dev);
	unsigned long val;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	mutex_lock(&acc->lock);
	if (val)
		lis3dsh_acc_enable_l(acc);
	else
		lis3dsh_acc_disable_l(acc);
	mutex_unlock(&acc->lock);

	return size;
}


static int lis3dsh_acc_interrupt_enable_control(struct lis3dsh_acc_data *acc,
								u8 settings)
{
	/* All cases have IRQ latched and active high */
	u8 val1 = (LIS3DSH_INT_LATCHED | LIS3DSH_INT_ACT_H);
	u8 val2 = LIS3DSH_INTEN_ON;
	u8 mask1 = (LIS3DSH_INT_LATCH_MASK | LIS3DSH_INT_ACT_MASK |
			LIS3DSH_INT1_EN_MASK | LIS3DSH_INT2_EN_MASK);
	int err = -EINVAL;
	settings = settings & 0x03;

	switch (settings) {
	case LIS3DSH_INT1_DIS_INT2_DIS:
		val1 |= (LIS3DSH_INT1_EN_OFF | LIS3DSH_INT2_EN_OFF);
		val2 = LIS3DSH_INTEN_OFF;
		break;
	case LIS3DSH_INT1_DIS_INT2_EN:
		val1 |= (LIS3DSH_INT1_EN_OFF | LIS3DSH_INT2_EN_ON);
		break;
	case LIS3DSH_INT1_EN_INT2_DIS:
		val1 |= (LIS3DSH_INT1_EN_ON | LIS3DSH_INT2_EN_OFF);
		break;
	case LIS3DSH_INT1_EN_INT2_EN:
		val1 |= (LIS3DSH_INT1_EN_ON | LIS3DSH_INT2_EN_ON);
		break;
	default:
		pr_err("invalid interrupt setting : 0x%02x\n", settings);
		return err;
	}
	err = lis3dsh_acc_register_masked_update(acc,
		LIS3DSH_CTRL_REG3, mask1, val1, LIS3DSH_RES_CTRL_REG3);
	if (err < 0)
		return err;

	err = lis3dsh_acc_register_masked_update(acc, LIS3DSH_CTRL_REG1,
			LIS3DSH_INTEN_MASK, val2, LIS3DSH_RES_CTRL_REG1);
	if (err < 0)
		return err;
	acc->interrupts_enable_setting = settings;
	return err;
}

static int lis3dsh_acc_state_progrs_enable_control(
				struct lis3dsh_acc_data *acc, u8 settings)
{
	u8 val1, val2;
	int err = -1;

	switch (settings) {
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
	default:
		pr_err("invalid state program setting : 0x%02x\n", settings);
		return err;
	}
	err = lis3dsh_acc_register_masked_update(acc, LIS3DSH_CTRL_REG1,
		LIS3DSH_SM1_EN_MASK, val1, LIS3DSH_RES_CTRL_REG1);
	if (err < 0)
		return err;

	err = lis3dsh_acc_register_masked_update(acc, LIS3DSH_CTRL_REG2,
		LIS3DSH_SM2_EN_MASK, val2, LIS3DSH_RES_CTRL_REG2);
	if (err < 0)
		return err;
	acc->stateprogs_enable_setting = settings;
#ifdef DEBUG
	pr_err("state program setting : 0x%02x\n",
		acc->stateprogs_enable_setting);
#endif

	return err;
}

static ssize_t attr_set_enable_state_prog(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	int err = -1;
	struct lis3dsh_acc_data *acc = dev_get_drvdata(dev);
	long val = 0;

	if (strict_strtoul(buf, 16, &val))
		return -EINVAL;

	if (val < 0x00 || val > LIS3DSH_SM1_EN_SM2_EN) {
#ifdef DEBUG
		pr_err("invalid state program setting, val: %ld\n", val);
#endif
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

static ssize_t attr_get_x_negate(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct lis3dsh_acc_data *acc = dev_get_drvdata(dev);
	ssize_t ret;
	mutex_lock(&acc->lock);
	ret = snprintf(buf, 16, "%d\n", acc->pdata->negate_x ? 1 : 0);
	mutex_unlock(&acc->lock);
	return ret;
}

static ssize_t attr_get_y_negate(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct lis3dsh_acc_data *acc = dev_get_drvdata(dev);
	ssize_t ret;
	mutex_lock(&acc->lock);
	ret = snprintf(buf, 16, "%d\n", acc->pdata->negate_y ? 1 : 0);
	mutex_unlock(&acc->lock);
	return ret;
}

static ssize_t attr_get_z_negate(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct lis3dsh_acc_data *acc = dev_get_drvdata(dev);
	ssize_t ret;
	mutex_lock(&acc->lock);
	ret = snprintf(buf, 16, "%d\n", acc->pdata->negate_z ? 1 : 0);
	mutex_unlock(&acc->lock);
	return ret;
}

static ssize_t attr_get_x_index(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct lis3dsh_acc_data *acc = dev_get_drvdata(dev);
	ssize_t ret;
	mutex_lock(&acc->lock);
	ret = snprintf(buf, 16, "%d\n", acc->pdata->axis_map_x);
	mutex_unlock(&acc->lock);
	return ret;
}

static ssize_t attr_get_y_index(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct lis3dsh_acc_data *acc = dev_get_drvdata(dev);
	ssize_t ret;
	mutex_lock(&acc->lock);
	ret = snprintf(buf, 16, "%d\n", acc->pdata->axis_map_y);
	mutex_unlock(&acc->lock);
	return ret;
}

static ssize_t attr_get_z_index(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct lis3dsh_acc_data *acc = dev_get_drvdata(dev);
	ssize_t ret;
	mutex_lock(&acc->lock);
	ret = snprintf(buf, 16, "%d\n", acc->pdata->axis_map_z);
	mutex_unlock(&acc->lock);
	return ret;
}

static ssize_t attr_get_int1_enable(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct lis3dsh_acc_data *acc = dev_get_drvdata(dev);
	ssize_t ret;
	ret = snprintf(buf, 16, "%d\n", atomic_read(&acc->int1_enabled));
	return ret;
}

static ssize_t attr_set_int1_enable(struct device *dev,
				struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct lis3dsh_acc_data *acc = dev_get_drvdata(dev);
	unsigned long val = 0;

	if (acc->pdata->gpio_int1 < 0)
		return -EINVAL;
	if (kstrtoul(buf, 10, &val))
		return -EINVAL;

	mutex_lock(&acc->lock);
	if (val == 0) {
		if (atomic_cmpxchg(&acc->int1_enabled, 1, 0)) {
			disable_irq(acc->irq1);
			disable_irq_wake(acc->irq1);
		}
	} else if (val == 1) {
		if (!atomic_cmpxchg(&acc->int1_enabled, 0, 1)) {
			enable_irq_wake(acc->irq1);
			enable_irq(acc->irq1);
			lis3dsh_clear_irq1(acc);
		}
	} else {
		pr_err("enable setting not 0 or 1 (found %d)\n", (int) val);
		mutex_unlock(&acc->lock);
		return -EINVAL;
	}
	mutex_unlock(&acc->lock);
	return size;
}

static ssize_t attr_get_int2_enable(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct lis3dsh_acc_data *acc = dev_get_drvdata(dev);
	ssize_t ret;
	ret = snprintf(buf, 16, "%d\n", atomic_read(&acc->int2_enabled));
	return ret;
}

static ssize_t attr_set_int2_enable(struct device *dev,
				struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct lis3dsh_acc_data *acc = dev_get_drvdata(dev);
	unsigned long val = 0;

	if (acc->pdata->gpio_int2 < 0)
		return -EINVAL;
	if (kstrtoul(buf, 10, &val))
		return -EINVAL;
	mutex_lock(&acc->lock);
	if (val == 0) {
		if (atomic_cmpxchg(&acc->int2_enabled, 1, 0)) {
			disable_irq(acc->irq2);
			disable_irq_wake(acc->irq2);
		}
	} else if (val == 1) {
		if (!atomic_cmpxchg(&acc->int2_enabled, 0, 1)) {
			enable_irq_wake(acc->irq2);
			enable_irq(acc->irq2);
			lis3dsh_clear_irq2(acc);
		}
	} else {
		pr_err("enable setting not 0 or 1 (found %d)\n", (int) val);
		mutex_unlock(&acc->lock);
		return -EINVAL;
	}
	mutex_unlock(&acc->lock);
	return size;
}

static ssize_t attr_get_report_events(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct lis3dsh_acc_data *acc = dev_get_drvdata(dev);
	ssize_t ret;
	ret = snprintf(buf, 16, "%d\n", acc->report_events ? 1 : 0);
	return ret;
}

static ssize_t attr_set_report_events(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct lis3dsh_acc_data *acc = dev_get_drvdata(dev);
	unsigned long val = 0;

	if (kstrtoul(buf, 10, &val))
		return -EINVAL;
	mutex_lock(&acc->lock);
	acc->report_events = (val ? true : false);
	mutex_unlock(&acc->lock);
	return size;
}

static ssize_t attr_get_both_ints_enable(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct lis3dsh_acc_data *acc = dev_get_drvdata(dev);
	ssize_t ret;
	bool both_enabled = false;

	mutex_lock(&acc->lock);
	both_enabled = (atomic_read(&acc->int1_enabled)
			&& atomic_read(&acc->int2_enabled));
	ret = snprintf(buf, 16, "%d\n", both_enabled ? 1 : 0);
	mutex_unlock(&acc->lock);
	return ret;
}

static ssize_t attr_set_both_ints_enable(struct device *dev,
				struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct lis3dsh_acc_data *acc = dev_get_drvdata(dev);
	unsigned long val = 0;

	if ((acc->pdata->gpio_int1 < 0) || (acc->pdata->gpio_int2 < 0))
		return -EINVAL;
	if (kstrtoul(buf, 10, &val))
		return -EINVAL;
	mutex_lock(&acc->lock);
	if (val == 0) {
		if (atomic_cmpxchg(&acc->int1_enabled, 1, 0)) {
			disable_irq(acc->irq1);
			disable_irq_wake(acc->irq1);
		}
		if (atomic_cmpxchg(&acc->int2_enabled, 1, 0)) {
			disable_irq(acc->irq2);
			disable_irq_wake(acc->irq2);
		}
	} else if (val == 1) {
		if (!atomic_cmpxchg(&acc->int1_enabled, 0, 1)) {
			enable_irq_wake(acc->irq1);
			enable_irq(acc->irq1);
			lis3dsh_clear_irq1(acc);
		}
		if (!atomic_cmpxchg(&acc->int2_enabled, 0, 1)) {
			enable_irq_wake(acc->irq2);
			enable_irq(acc->irq2);
			lis3dsh_clear_irq2(acc);
		}
	} else {
		pr_err("enable setting not 0 or 1 (found %d)\n", (int) val);
		mutex_unlock(&acc->lock);
		return -EINVAL;
	}
	mutex_unlock(&acc->lock);
	return size;
}

static struct device_attribute attributes[] = {

	__ATTR(poll_period_ms, 0664, attr_get_polling_rate,
							attr_set_polling_rate),
	__ATTR(sensitivity, 0444, attr_get_sensitivity, NULL),
	__ATTR(range, 0664, attr_get_range, attr_set_range),
	__ATTR(enable_device, 0664, attr_get_enable, attr_set_enable),
	__ATTR(power_device, 0664, attr_get_power_device,
							attr_set_power_device),

	__ATTR(tilt_count, 0600, attr_get_tilt_count, attr_set_tilt_count),

	__ATTR(enable_state_prog, 0664, attr_get_enable_state_prog,
						attr_set_enable_state_prog),
	__ATTR(reg_value, 0600, attr_reg_get, attr_reg_set),
	__ATTR(reg_addr, 0200, NULL, attr_addr_set),

	__ATTR(enable_int1, 0600, attr_get_int1_enable, attr_set_int1_enable),
	__ATTR(enable_int2, 0600, attr_get_int2_enable, attr_set_int2_enable),
	__ATTR(enable_both_ints, 0600, attr_get_both_ints_enable,
						attr_set_both_ints_enable),

	__ATTR(report_events, 0600, attr_get_report_events,
						attr_set_report_events),

	__ATTR(x_index, 0440, attr_get_x_index, NULL),
	__ATTR(y_index, 0440, attr_get_y_index, NULL),
	__ATTR(z_index, 0440, attr_get_z_index, NULL),
	__ATTR(negate_x, 0440, attr_get_x_negate, NULL),
	__ATTR(negate_y, 0440, attr_get_y_negate, NULL),
	__ATTR(negate_z, 0440, attr_get_z_negate, NULL),
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
	return -EPERM;
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
	int xyz[3] = { 0 };
	int err;
	struct lis3dsh_acc_data *acc;
	acc = container_of(work, struct lis3dsh_acc_data, input_work.work);

	mutex_lock(&acc->lock);
	err = lis3dsh_acc_get_acceleration_data(acc, xyz);
	if (err < 0)
		dev_err(&acc->client->dev, "get_acceleration_data failed\n");
	else
		lis3dsh_acc_report_values(acc, xyz);

	schedule_delayed_work(&acc->input_work, msecs_to_jiffies(
			acc->pdata->poll_interval));
	mutex_unlock(&acc->lock);
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

	INIT_DELAYED_WORK(&acc->input_work, lis3dsh_acc_input_work_func);
	acc->input_dev = input_allocate_device();
	if (!acc->input_dev) {
		err = -ENOMEM;
		dev_err(&acc->client->dev, "input device allocation failed\n");
		goto err0;
	}

	acc->input_dev->open = NULL;
	acc->input_dev->close = NULL;
	acc->input_dev->name = LIS3DSH_ACC_DEV_NAME;

	acc->input_dev->id.bustype = BUS_I2C;
	acc->input_dev->dev.parent = &acc->client->dev;

	input_set_drvdata(acc->input_dev, acc);

	set_bit(EV_ABS, acc->input_dev->evbit);
	/*	next is used for interruptA sources data if the case */
	set_bit(ABS_MISC, acc->input_dev->absbit);
	/*	next is used for interruptB sources data if the case */
	set_bit(ABS_WHEEL, acc->input_dev->absbit);

	input_set_abs_params(acc->input_dev, ABS_X, -G_MAX, G_MAX, 0, 0);
	input_set_abs_params(acc->input_dev, ABS_Y, -G_MAX, G_MAX, 0, 0);
	input_set_abs_params(acc->input_dev, ABS_Z, -G_MAX, G_MAX, 0, 0);

#if 0
	/* next is used for interruptA sources data if the case */
	input_set_abs_params(acc->input_dev, ABS_MISC, INT_MIN, INT_MAX, 0, 0);
	/* next is used for interruptB sources data if the case */
	input_set_abs_params(acc->input_dev, ABS_WHEEL, INT_MIN, INT_MAX, 0, 0);
#endif
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

static int lis3dsh_regulator_enable(struct lis3dsh_acc_data *acc,
					bool enable)
{
	struct lis3dsh_acc_private_data *pdata = acc->pdata;
	int err = 0;
	int reg_err = 0;

	if (enable && !acc->powered) {
		if (pdata->vdd_regulator) {
			err = regulator_enable(pdata->vdd_regulator);
			if (err) {
				pr_err("enable vdd_regulator failed\n");
				goto err_exit_2;
			}
		}
		if (pdata->vdd_io_regulator) {
			err = regulator_enable(pdata->vdd_io_regulator);
			if (err) {
				pr_err("enable vdd_io_regulator failed\n");
				goto err_exit_1;
			}
		}
		if (pdata->vcc_i2c_regulator) {
			err = regulator_enable(pdata->vcc_i2c_regulator);
			if (err) {
				pr_err("enable vcc_i2c_regulator failed\n");
				goto err_exit;
			}
		}
		acc->powered = true;

		/* LIS3DSH takes 5mS for internal power on boot sequence */
		msleep(10);

	} else if (!enable && acc->powered) {
		if (pdata->vcc_i2c_regulator) {
			err = regulator_disable(pdata->vcc_i2c_regulator);
			if (err) {
				pr_err("disable vcc_i2c_regulator failed\n");
				reg_err = err;
			}
		}
		if (pdata->vdd_io_regulator) {
			err = regulator_disable(pdata->vdd_io_regulator);
			if (err) {
				pr_err("disable vdd_io_regulator failed\n");
				reg_err = err;
			}
		}
		if (pdata->vdd_regulator) {
			err = regulator_disable(pdata->vdd_regulator);
			if (err) {
				pr_err("disable vdd_regulator failed\n");
				reg_err = err;
			}
		}
		acc->powered = false;
		err = reg_err;
	}
	return err;

err_exit:
	if (pdata->vdd_io_regulator)
		regulator_disable(pdata->vdd_io_regulator);
err_exit_1:
	if (pdata->vdd_regulator)
		regulator_disable(pdata->vdd_regulator);
err_exit_2:
	return err;
}

static int my_of_property_read_u8(struct device_node *np, const char *str,
							u8 *outValue)
{
	u32 lame;
	int err;

	err = of_property_read_u32(np, str, &lame);
	if (err)
		return err;
	if (lame > 0xff) {
		pr_err("value of %d for %s too large for 8 bits\n", lame, str);
		return -EOVERFLOW;
	}
	*outValue = lame;

	return 0;
}

/* This function collects device tree data that gets range-checked later */
static int lis3dsh_parse_dt(struct lis3dsh_acc_data *acc)
{
	struct i2c_client *client = acc->client;
	struct device_node *np = client->dev.of_node;
	struct lis3dsh_acc_private_data *pdata = acc->pdata;
	int err = 0;

	pdata->gpio_int1 = of_get_named_gpio(np, "lis,int1_gpio", 0);
	if (pdata->gpio_int1 < 0) {
		pr_err("FAILED to get gpio_int1\n");
		err = pdata->gpio_int1;
		goto exit;
	}

	pdata->gpio_int2 = of_get_named_gpio(np, "lis,int2_gpio", 0);
	if (pdata->gpio_int2 < 0) {
		pr_err("FAILED to get gpio_int2\n");
		err = pdata->gpio_int2;
		goto exit;
	}

	pdata->gpio_dsp_int1 = of_get_named_gpio(np, "google,dsp_int1_gpio", 0);
	if (pdata->gpio_dsp_int1 < 0)
		pr_info("lis3dsh_acc: dsp_int1_gpio not specified\n");

	pdata->vdd_io_regulator = regulator_get(&client->dev, "vdd_io");
	if (IS_ERR_OR_NULL(pdata->vdd_io_regulator)) {
		pdata->vdd_io_regulator = NULL;
		pr_err("FAILED to get vdd_io_regulator\n");
		err = -EIO;
		goto exit;
	}

	pdata->vdd_regulator = regulator_get(&client->dev, "vdd");
	if (IS_ERR_OR_NULL(pdata->vdd_regulator)) {
		pdata->vdd_regulator = NULL;
		pr_err("FAILED to get vdd_regulator\n");
		err = -EIO;
		goto exit;
	}

	pdata->vcc_i2c_regulator = regulator_get(&client->dev, "vcc_i2c");
	if (IS_ERR_OR_NULL(pdata->vcc_i2c_regulator)) {
		pdata->vcc_i2c_regulator = NULL;
		pr_err("FAILED to get vcc_i2c_regulator\n");
		err = -EIO;
		goto exit;
	}

	err = my_of_property_read_u8(np, "lis,negate_x", &pdata->negate_x);
	if (err) {
		pr_err("FAILED to get negate_x\n");
		goto exit;
	}
	err = my_of_property_read_u8(np, "lis,negate_y", &pdata->negate_y);
	if (err) {
		pr_err("FAILED to get negate_y\n");
		goto exit;
	}
	err = my_of_property_read_u8(np, "lis,negate_z", &pdata->negate_z);
	if (err) {
		pr_err("FAILED to get negate_z\n");
		goto exit;
	}

	err = of_property_read_u32(np, "lis,min_interval",
							&pdata->min_interval);
	if (err) {
		pr_err("FAILED to get min_interval\n");
		goto exit;
	}
	err = of_property_read_u32(np, "lis,poll_interval",
							&pdata->poll_interval);
	if (err) {
		pr_err("FAILED to get poll_interval\n");
		goto exit;
	}

	err = my_of_property_read_u8(np, "lis,axis_map_x", &pdata->axis_map_x);
	if (err)
		goto exit;
	err = my_of_property_read_u8(np, "lis,axis_map_y", &pdata->axis_map_y);
	if (err)
		goto exit;
	err = my_of_property_read_u8(np, "lis,axis_map_z", &pdata->axis_map_z);
	if (err)
		goto exit;
	err = my_of_property_read_u8(np, "lis,fs_range", &pdata->fs_range);
	if (err)
		goto exit;
exit:
	return err;
}

static int lis3dsh_acc_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct lis3dsh_acc_data *acc;
	int err = 0;

	pr_info("%s: probe start.\n", LIS3DSH_ACC_DEV_NAME);

	if (!client->dev.of_node) {
		dev_err(&client->dev, "only device tree supported, exiting.\n");
		err = -ENODEV;
		goto exit_check_functionality_failed;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "client not i2c capable\n");
		err = -ENODEV;
		goto exit_check_functionality_failed;
	}


	acc = kzalloc(sizeof(struct lis3dsh_acc_data), GFP_KERNEL);
	if (acc == NULL) {
		err = -ENOMEM;
		dev_err(&client->dev,
				"failed to allocate memory for module data: "
					"%d\n", err);
		goto exit_check_functionality_failed;
	}

	acc->pdata = kmalloc(sizeof(*acc->pdata), GFP_KERNEL);
	if (acc->pdata == NULL) {
		err = -ENOMEM;
		dev_err(&client->dev,
			"failed to allocate memory for pdata: %d\n", err);
		goto exit_check_functionality_failed;
	}

	mutex_init(&acc->lock);
	mutex_lock(&acc->lock);

	acc->powered = 0;
	acc->tilt_count = 0;
	acc->report_events = true;
	atomic_set(&acc->int1_enabled, 0);
	atomic_set(&acc->int2_enabled, 0);
	acc->enabled = 0;
	acc->client = client;
	i2c_set_clientdata(client, acc);

	wake_lock_init(&acc->tilt_wakelock, WAKE_LOCK_SUSPEND, TILT_LOCK_NAME);

	err = lis3dsh_parse_dt(acc);
	if (err < 0) {
		printk("%s: failed getting board data\n", __func__);
		dev_err(&client->dev, "failed getting board data\n");
		goto exit_mutex_unlock;
	}

	err = lis3dsh_acc_validate_pdata(acc);
	if (err < 0) {
		printk("%s: failed to validate platform data\n", __func__);
		dev_err(&client->dev, "failed to validate platform data\n");
		goto exit_mutex_unlock;
	}

	if (acc->pdata->gpio_int1 >= 0) {
		acc->irq1 = gpio_to_irq(acc->pdata->gpio_int1);
		pr_info("%s: %s has set irq1 to irq: %d "
							"mapped on gpio:%d\n",
			LIS3DSH_ACC_DEV_NAME, __func__, acc->irq1,
							acc->pdata->gpio_int1);
	}

	if (acc->pdata->gpio_int2 >= 0) {
		acc->irq2 = gpio_to_irq(acc->pdata->gpio_int2);
		pr_info("%s: %s has set irq2 to irq: %d mapped on gpio:%d\n",
			LIS3DSH_ACC_DEV_NAME, __func__, acc->irq2,
							acc->pdata->gpio_int2);
	}

	/* resume state init config */
	memset(acc->resume_state, 0, ARRAY_SIZE(acc->resume_state));
	lis3dsh_acc_set_init_register_values(acc);

	/* init state program1 and params */
	lis3dsh_acc_set_init_statepr1_param(acc);
	lis3dsh_acc_set_init_statepr1_inst(acc);

	/* init state program2  and params */
	lis3dsh_acc_set_init_statepr2_param(acc);
	lis3dsh_acc_set_init_statepr2_inst(acc);

	err = lis3dsh_acc_device_power_on(acc);
	if (err < 0) {
		dev_err(&client->dev, "power on failed: %d\n", err);
		goto exit_mutex_unlock;
	}

	acc->enabled = 1;

	err = lis3dsh_acc_interrupt_enable_control(acc,
						acc->interrupts_enable_setting);
	if (err < 0) {
		dev_err(&client->dev, "interrupt settings failed\n");
		goto  err_power_off;
	}

	err = lis3dsh_acc_update_fs_range_l(acc, acc->pdata->fs_range);
	if (err < 0) {
		dev_err(&client->dev, "update_fs_range failed\n");
		goto  err_power_off;
	}

	err = lis3dsh_acc_update_odr_l(acc, acc->pdata->poll_interval);
	if (err < 0) {
		dev_err(&client->dev, "update_odr failed\n");
		goto  err_power_off;
	}

	err = lis3dsh_acc_input_init(acc);
	if (err < 0) {
		dev_err(&client->dev, "input init failed\n");
		goto err_power_off;
	}

        input_set_capability(acc->input_dev, EV_MSC, MSC_GESTURE);

	err = create_sysfs_interfaces(&client->dev);
	if (err < 0) {
		dev_err(&client->dev,
		   "device LIS3DSH_ACC_DEV_NAME sysfs register failed\n");
		goto err_input_cleanup;
	}

	lis3dsh_acc_device_power_off(acc);
	acc->enabled = 0;

	/* As default, do not report information */
	if (acc->pdata->gpio_int1 >= 0) {
		atomic_set(&acc->int1_enabled, 1);
		err = request_threaded_irq(acc->irq1, NULL,
				lis3dsh_acc_isr_threaded,
				IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
				"lis3dsh_acc_irq1", acc);
		if (err < 0) {
			dev_err(&client->dev, "request irq1 failed: %d\n", err);
			atomic_set(&acc->int1_enabled, 0);
			goto err_remove_sysfs_int;
		}
		if (atomic_cmpxchg(&acc->int1_enabled, 1, 0))
			disable_irq_nosync(acc->irq1);
	}

	if (acc->pdata->gpio_int2 >= 0) {
		atomic_set(&acc->int2_enabled, 1);
		err = request_threaded_irq(acc->irq2, NULL,
				lis3dsh_acc_isr_threaded,
				IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
				"lis3dsh_acc_irq2", acc);
		if (err < 0) {
			dev_err(&client->dev, "request irq2 failed: %d\n", err);
			atomic_set(&acc->int2_enabled, 0);
			goto err_free_irq1;
		}
		if (atomic_cmpxchg(&acc->int2_enabled, 1, 0))
			disable_irq_nosync(acc->irq2);
	}

	mutex_unlock(&acc->lock);

	dev_info(&client->dev, "%s: probed\n", LIS3DSH_ACC_DEV_NAME);
	return 0;

err_free_irq1:
	free_irq(acc->irq1, acc);
err_remove_sysfs_int:
	remove_sysfs_interfaces(&client->dev);
err_input_cleanup:
	lis3dsh_acc_input_cleanup(acc);
err_power_off:
	lis3dsh_acc_device_power_off(acc);
exit_mutex_unlock:
	wake_lock_destroy(&acc->tilt_wakelock);
	mutex_unlock(&acc->lock);
	kfree(acc);
exit_check_functionality_failed:
	pr_err("%s: Driver Init failed\n", LIS3DSH_ACC_DEV_NAME);
	return err;
}

static int lis3dsh_acc_remove(struct i2c_client *client)
{
	struct lis3dsh_acc_data *acc = i2c_get_clientdata(client);

	mutex_lock(&acc->lock);
	if (acc->pdata->gpio_int1 >= 0) {
		free_irq(acc->irq1, acc);
		gpio_free(acc->pdata->gpio_int1);
		/* TODO - cancel pending threaded irq handlers */
	}

	if (acc->pdata->gpio_int2 >= 0) {
		free_irq(acc->irq2, acc);
		gpio_free(acc->pdata->gpio_int2);
		/* TODO - cancel pending threaded irq handlers */
	}

	if (acc->enabled)
		cancel_delayed_work_sync(&acc->input_work);

	lis3dsh_acc_device_power_off(acc);
	lis3dsh_acc_input_cleanup(acc);
	remove_sysfs_interfaces(&client->dev);

	kfree(acc->pdata);
	wake_lock_destroy(&acc->tilt_wakelock);
	mutex_unlock(&acc->lock);
	kfree(acc);

	return 0;
}

#ifdef CONFIG_PM
static int lis3dsh_acc_resume(struct i2c_client *client)
{
	struct lis3dsh_acc_data *acc = i2c_get_clientdata(client);
	int err = 0;

	/* Don't do anything if IRQ1 interrupt is enabled */
	if (atomic_read(&acc->int1_enabled)) {
		enable_irq(acc->irq1);
		return 0;
	}

	mutex_lock(&acc->lock);
	if (acc->on_before_suspend)
		err = lis3dsh_acc_enable_l(acc);
	mutex_unlock(&acc->lock);
	return err;
}

static int lis3dsh_acc_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct lis3dsh_acc_data *acc = i2c_get_clientdata(client);
	int err = 0;

	/* Don't do anything if IRQ1 interrupt is enabled */
	if (atomic_read(&acc->int1_enabled)) {
		disable_irq(acc->irq1);
		return 0;
	}

	mutex_lock(&acc->lock);
	acc->on_before_suspend = acc->enabled;
	err = lis3dsh_acc_disable_l(acc);
	mutex_unlock(&acc->lock);
	return err;
}
#else
#define lis3dsh_acc_suspend	NULL
#define lis3dsh_acc_resume	NULL
#endif /* CONFIG_PM */
#ifdef CONFIG_OF
static const struct of_device_id lis3dsh_dt_match[] = {
	{ .compatible = "st,lis3dsh" },
	{ },
};
MODULE_DEVICE_TABLE(of, lis3dsh_dt_match);
#endif
static const struct i2c_device_id lis3dsh_acc_id[]
		= { { LIS3DSH_ACC_DEV_NAME, 0 }, { }, };

MODULE_DEVICE_TABLE(i2c, lis3dsh_acc_id);

static struct i2c_driver lis3dsh_acc_driver = {
	.driver = {
			.owner = THIS_MODULE,
			.name = LIS3DSH_ACC_DEV_NAME,
			.of_match_table = of_match_ptr(lis3dsh_dt_match),
		  },
	.probe = lis3dsh_acc_probe,
	.remove = lis3dsh_acc_remove,
	.suspend = lis3dsh_acc_suspend,
	.resume = lis3dsh_acc_resume,
	.id_table = lis3dsh_acc_id,
};

static int __init lis3dsh_acc_init(void)
{
	pr_info("%s accelerometer driver: init\n", LIS3DSH_ACC_DEV_NAME);
	return i2c_add_driver(&lis3dsh_acc_driver);
}

static void __exit lis3dsh_acc_exit(void)
{
#ifdef DEBUG
	pr_info("%s accelerometer driver exit\n", LIS3DSH_ACC_DEV_NAME);
#endif /* DEBUG */
	i2c_del_driver(&lis3dsh_acc_driver);
	return;
}

module_init(lis3dsh_acc_init);
module_exit(lis3dsh_acc_exit);

MODULE_DESCRIPTION("lis3dsh accelerometer driver");
MODULE_AUTHOR("Matteo Dameno, Denis Ciocca, STMicroelectronics");
MODULE_LICENSE("GPL");

