/*
 * File: ImmVibeSPI.c
 *
 * Description:
 *     Device-dependent functions called by Immersion TSP API
 *     to control PWM duty cycle, amp enable/disable, save IVT file, etc...
 *
 * Portions Copyright (c) 2008-2010 Immersion Corporation. All Rights Reserved.
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the GNU Public License v2 -
 * (the 'License'). You may not use this file except in compliance with the
 * License. You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or contact
 * TouchSenseSales@immersion.com.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND IMMERSION HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see
 * the License for the specific language governing rights and limitations
 * under the License.
 */

#include <linux/io.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/regulator/msm-gpio-regulator.h>

#include <mach/irqs.h>
#include <mach/gpiomux.h>
#include <mach/msm_iomap.h>
#include <mach/msm_xo.h>

static bool g_bAmpEnabled = false;

/* gpio and clock control for vibrator */

#define PM8921_GPIO_BASE                NR_GPIO_IRQS
#define PM8921_GPIO_PM_TO_SYS(pm_gpio)  (pm_gpio - 1 + PM8921_GPIO_BASE)

#define REG_WRITEL(value, reg)          writel(value, (MSM_CLK_CTL_BASE + reg))
#define REG_READL(reg)                  readl((MSM_CLK_CTL_BASE + reg))

#define GPn_MD_REG(n)                   (0x2D00+32*(n))
#define GPn_NS_REG(n)                   (0x2D24+32*(n))

/*
** This SPI supports only one actuator.
*/
#define NUM_ACTUATORS                   1

#define PWM_DUTY_MAX                    579 /* 13MHz / (579 + 1) = 22.4kHz */

#define GPIO_LIN_MOTOR_EN               33
#define GPIO_LIN_MOTOR_PWR              47
#define GPIO_LIN_MOTOR_PWM              3

#define GP_CLK_ID                       0 /* gp clk 0 */
#define GP_CLK_M_DEFAULT                1
#define GP_CLK_N_DEFAULT                166
#define GP_CLK_D_MAX                    GP_CLK_N_DEFAULT
#define GP_CLK_D_HALF                   (GP_CLK_N_DEFAULT >> 1)


static struct gpiomux_setting vibrator_suspend_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting vibrator_active_cfg_gpio3 = {
	.func = GPIOMUX_FUNC_2, /*gp_mn:2 */
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct msm_gpiomux_config gpio2_vibrator_configs[] = {
	{
		.gpio = 3,
		.settings = {
			[GPIOMUX_ACTIVE] = &vibrator_active_cfg_gpio3,
			[GPIOMUX_SUSPENDED] = &vibrator_suspend_cfg,
		},
	},
};

static struct msm_xo_voter *vib_clock;
static int vibrator_clock_init(void)
{
	int rc;
	/*Vote for XO clock*/
	vib_clock = msm_xo_get(MSM_XO_TCXO_D0, "vib_clock");

	if (IS_ERR(vib_clock)) {
		rc = PTR_ERR(vib_clock);
		printk(KERN_ERR "%s: Couldn't get TCXO_D0 vote for Vib(%d)\n",
							__func__, rc);
	}
	return rc;
}

static int vibrator_clock_on(void)
{
	int rc;
	rc = msm_xo_mode_vote(vib_clock, MSM_XO_MODE_ON);
	if (rc < 0) {
		printk(KERN_ERR "%s: Failed to vote for TCX0_D0 ON (%d)\n",
					__func__, rc);
	}
	return rc;
}

static int vibrator_clock_off(void)
{
	int rc;
	rc = msm_xo_mode_vote(vib_clock, MSM_XO_MODE_OFF);
	if (rc < 0) {
		printk(KERN_ERR "%s: Failed to vote for TCX0_D0 OFF (%d)\n",
					__func__, rc);
	}
	return rc;
}

static int vibrator_power_set(int enable)
{
	int rc = 0;
	static struct regulator *vreg_l16 = NULL;
	int enabled = 0;

	if (unlikely(!vreg_l16)) {
		vreg_l16 = regulator_get(NULL, "8921_l16"); /* 2.6 ~ 3V */

		if (IS_ERR(vreg_l16)) {
			pr_err("%s: regulator get of 8921_lvs6 failed (%ld)\n",
						__func__, PTR_ERR(vreg_l16));
			rc = PTR_ERR(vreg_l16);
			return rc;
		}
	}

	/* fix the unbalanced disables */
	enabled = regulator_is_enabled(vreg_l16);
	if (enabled > 0) {
		if (enable) { /* already enabled */
			printk("vibrator already enabled\n");
			return 0;
		}
	} else if (enabled == 0) {
		if (enable == 0) { /* already disabled */
			printk("vibrator already disabled\n");
			return 0;
		}
	} else { /*  (enabled < 0) */
		pr_warn("%s: regulator_is_enabled failed\n", __func__);
	}

	rc = regulator_set_voltage(vreg_l16, 2800000, 2800000);

	if(enable) {
		printk("vibrator_power_set() : vibrator enable\n");
		rc = regulator_enable(vreg_l16);
	}
	else {
		printk("vibrator_power_set() : vibrator disable\n");
		rc = regulator_disable(vreg_l16);
	}

	return rc;
}

static void vibrator_pwm_set(int enable, int amp, int n_value)
{
	uint M_VAL	= GP_CLK_M_DEFAULT;
	uint D_VAL	= GP_CLK_D_MAX;
	uint D_INV	= 0; /* QCT support invert bit for msm8960 */

	if (enable) {
		vibrator_clock_on();

		D_VAL = (((GP_CLK_D_MAX -1) * amp) >> 8) + GP_CLK_D_HALF;

		if (D_VAL > GP_CLK_D_HALF) {
			if (D_VAL == GP_CLK_D_MAX)      /* Max duty is 99% */
				D_VAL = 2;
			else
				D_VAL = GP_CLK_D_MAX - D_VAL;

			D_INV = 1;
		}

		REG_WRITEL(
			(((M_VAL & 0xffU) << 16U) +     /* M_VAL[23:16] */
			((~(D_VAL << 1)) & 0xffU)),     /* D_VAL[7:0] */
			GPn_MD_REG(GP_CLK_ID));

		REG_WRITEL(
			((((~(n_value-M_VAL)) & 0xffU) << 16U) + /* N_VAL[23:16] */
			(1U << 11U) +  	/* CLK_ROOT_ENA[11]      : Enable(1) */
			((D_INV & 0x01U) << 10U) +/* CLK_INV[10] : Disable(0) */
			(1U << 9U) +    /* CLK_BRANCH_ENA[9]     : Enable(1) */
			(1U << 8U) +    /* NMCNTR_EN[8]          : Enable(1) */
			(0U << 7U) +    /* MNCNTR_RST[7]         : Not Active(0) */
			(2U << 5U) +    /* MNCNTR_MODE[6:5]      : Dual-edge mode(2) */
			(3U << 3U) +    /* PRE_DIV_SEL[4:3]      : Div-4 (3) */
			(5U << 0U)),    /* SRC_SEL[2:0]          : CXO (5) */
			GPn_NS_REG(GP_CLK_ID));
	}
	else {
		vibrator_clock_off();

		REG_WRITEL(
			((((~(n_value-M_VAL)) & 0xffU) << 16U) + /* N_VAL[23:16] */
			(0U << 11U) +	/* CLK_ROOT_ENA[11]	: Disable(0) */
			(0U << 10U) +	/* CLK_INV[10]		: Disable(0) */
			(0U << 9U) +	/* CLK_BRANCH_ENA[9]	: Disable(0) */
			(0U << 8U) +	/* NMCNTR_EN[8]		: Disable(0) */
			(0U << 7U) +	/* MNCNTR_RST[7]	: Not Active(0) */
			(2U << 5U) +	/* MNCNTR_MODE[6:5]	: Dual-edge mode(2) */
			(3U << 3U) +	/* PRE_DIV_SEL[4:3]	: Div-4 (3) */
			(5U << 0U)),	/* SRC_SEL[2:0]		: CXO (5) */
			GPn_NS_REG(GP_CLK_ID));
	}
}

static void vibrator_ic_enable_set(int enable)
{
	int gpio_lin_motor_en = 0;
	gpio_lin_motor_en = PM8921_GPIO_PM_TO_SYS(GPIO_LIN_MOTOR_EN);

	if (enable)
		gpio_direction_output(gpio_lin_motor_en, 1);
	else
		gpio_direction_output(gpio_lin_motor_en, 0);
}

/*
** Called to disable amp (disable output force)
*/
static VibeStatus ImmVibeSPI_ForceOut_AmpDisable(VibeUInt8 nActuatorIndex)
{

	if (g_bAmpEnabled) {
		DbgOut((KERN_DEBUG "ImmVibeSPI_ForceOut_AmpDisable.\n"));

		g_bAmpEnabled = false;

		vibrator_ic_enable_set(0);
		vibrator_pwm_set(0, 0, GP_CLK_N_DEFAULT);
		vibrator_power_set(0);
	}

	return VIBE_S_SUCCESS;
}

/*
** Called to enable amp (enable output force)
*/
static VibeStatus ImmVibeSPI_ForceOut_AmpEnable(VibeUInt8 nActuatorIndex)
{

	if (!g_bAmpEnabled) {
		DbgOut((KERN_DEBUG "ImmVibeSPI_ForceOut_AmpEnable.\n"));

		g_bAmpEnabled = true;

		vibrator_power_set(1);
		vibrator_pwm_set(1, 0, GP_CLK_N_DEFAULT);
		vibrator_ic_enable_set(1);
	}
	else {
		DbgOut((KERN_DEBUG "[ImmVibeSPI] : ImmVibeSPI_ForceOut_AmpEnable [%d]\n", g_bAmpEnabled ));
	}

	return VIBE_S_SUCCESS;
}

/*
** Called at initialization time to set PWM freq, disable amp, etc...
*/
static VibeStatus ImmVibeSPI_ForceOut_Initialize(void)
{
	int rc;
	int gpio_motor_en = 0;
	int gpio_motor_pwm = 0;

	DbgOut((KERN_DEBUG "ImmVibeSPI_ForceOut_Initialize.\n"));

	/* to force ImmVibeSPI_ForceOut_AmpDisable disabling the amp */
	g_bAmpEnabled = true;

	/*
	** Disable amp.
	** If multiple actuators are supported, please make sure to call
	** ImmVibeSPI_ForceOut_AmpDisable for each actuator (provide the actuator index as
	** input argument).
	*/

	gpio_motor_en = GPIO_LIN_MOTOR_EN;
	gpio_motor_pwm = GPIO_LIN_MOTOR_PWM;

	/* GPIO function setting */
	msm_gpiomux_install(gpio2_vibrator_configs, ARRAY_SIZE(gpio2_vibrator_configs));

	/* GPIO setting for Motor EN in pmic8921 */
	gpio_motor_en = PM8921_GPIO_PM_TO_SYS(GPIO_LIN_MOTOR_EN);
	rc = gpio_request(gpio_motor_en, "lin_motor_en");

	if (rc) {
		DbgOut(("GPIO_LIN_MOTOR_EN %d request failed\n", gpio_motor_en));
		return -1;
	}

	/* gpio init */
	rc = gpio_request(gpio_motor_pwm, "lin_motor_pwm");

	if (unlikely(rc < 0))
		DbgOut(("not able to get gpio\n"));

	vibrator_clock_init();

	ImmVibeSPI_ForceOut_AmpDisable(0);

	return VIBE_S_SUCCESS;
}

/*
** Called at termination time to set PWM freq, disable amp, etc...
*/
static VibeStatus ImmVibeSPI_ForceOut_Terminate(void)
{

	DbgOut((KERN_DEBUG "ImmVibeSPI_ForceOut_Terminate.\n"));

	/*
	** Disable amp.
	** If multiple actuators are supported, please make sure to call
	** ImmVibeSPI_ForceOut_AmpDisable for each actuator (provide the actuator index as
	** input argument).
	*/
	ImmVibeSPI_ForceOut_AmpDisable(0);

	return VIBE_S_SUCCESS;
}

/*
** Called by the real-time loop to set PWM duty cycle
*/
static VibeStatus ImmVibeSPI_ForceOut_SetSamples(VibeUInt8 nActuatorIndex, VibeUInt16 nOutputSignalBitDepth, VibeUInt16 nBufferSizeInBytes, VibeInt8* pForceOutputBuffer)
{
	VibeInt8 nForce;

	switch (nOutputSignalBitDepth) {
	case 8:
		/* pForceOutputBuffer is expected to contain 1 byte */
		if (nBufferSizeInBytes != 1)
			return VIBE_E_FAIL;

		nForce = pForceOutputBuffer[0];
		break;
	case 16:
		/* pForceOutputBuffer is expected to contain 2 byte */
		if (nBufferSizeInBytes != 2)
			return VIBE_E_FAIL;

		/* Map 16-bit value to 8-bit */
		nForce = ((VibeInt16*)pForceOutputBuffer)[0] >> 8;
		break;
	default:
		/* Unexpected bit depth */
		return VIBE_E_FAIL;
	}
	/* Check the Force value with Max and Min force value */

	if (nForce > 127)
		nForce = 127;
	if (nForce < -127)
		nForce = -127;

	vibrator_pwm_set(1, nForce, GP_CLK_N_DEFAULT);

	return VIBE_S_SUCCESS;
}

/*
** Called to get the device name (device name must be returned as ANSI char)
*/
static VibeStatus ImmVibeSPI_Device_GetName(VibeUInt8 nActuatorIndex, char *szDevName, int nSize)
{
	return VIBE_S_SUCCESS;
}
