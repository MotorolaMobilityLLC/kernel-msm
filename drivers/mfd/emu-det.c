/*
 * Copyright (C) 2007 - 2012 Motorola, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 *
 * Brief overview about the switches used
 * ======================================
 *
 * wsdev (Whisper switch)
 *	DOCKED      : PPD(UART) dock is detected.
 *	UNDOCKED    : SMART dock is detected.
 *                    No dock is connected.
 *
 * dsdev (Standard Dock switch)
 *      NO_DOCK	    : No dock is connected.
 *	DESK_DOCK   : DESK (PPD) dock is detected.
 *	CAR_DOCK    : CAR (PPD) dock is detected.
 *	LE_DOCK     : Low end dock is detected.
 *	HE_DOCK     : High end dock detected.
 *
 * edsdev (Motorola Dock switch)
 *      NO_DOCK	    : No dock is connected.
 *	DESK_DOCK   : DESK (PPD) dock is detected.
 *	CAR_DOCK    : CAR (PPD) dock is detected.
 *	LE_DOCK     : Low end dock is detected.
 *	HE_DOCK     : High end dock detected.
 *	MOBILE_DOCK : LAP/MOBILE(SMART) dock is detected.
 *
 * asdev (Audio switch)
 *	NO_DEVICE   : Audio cable not present.
 *	EMU_OUT     : Audio cable detected on a PPD dock.
 *	SPDIF_AUDIO_OUT : Audio cable detected on a SMART dock.
 *			: not supported on MSM8960, but left here
 *			: to depict SMART dock having it's own audio
 *
 * sdsdev (Smart Dock switch) - Used only by Dock Status App
 *	DOCKED      : SMART dock is detected.
 *	UNDOCKED    : No dock is connected.
 *
 * csdev (charge capability switch)
 *      CHARGE_NONE          : No charger_msg available
 *      CHARGE_VLCL_REQ      : Voltage level / Current limit request
 *
 * noauthdev (No authentication switch) - Used to indicate if
 *                                        authentication is needed
 *      AUTH_REQUIRED     : Need to authenticate
 *      AUTH_NOT_REQUIRED : Do not need to authenticate, already done
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/switch.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/emu-accy.h>
#include <linux/usb/otg.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/mfd/pm8xxx/pm8921-charger.h>

#include <mach/irqs.h>
#include <mach/mmi_emu_det.h>
#include <mach/gpiomux.h>
#include <mach/msm_iomap.h>

#define PRINT_STATUS	(1U << 2)
#define PRINT_TRANSITION (1U << 1)
#define PRINT_DEBUG	(1U << 3)
#define PRINT_ERROR	(1U << 0)
#define PRINT_PARANOIC	(1U << 4)

#define hasHoneyBadger() (the_emud->emu_gpio[DMB_PPD_DET_GPIO] > 0)

#undef DEF
#define LIMITS { \
	DEF(FACTORY,	2300, 2680), \
	DEF(FLOAT_UP,	2200, 2480), \
	DEF(FLOAT_P,	10, 60), \
	DEF(GROUND,	60, 90), \
	DEF(STD_100K,   650, 1450), \
	DEF(ALT_100K,   1350, 1699), \
	DEF(ALT_200K,   1700, 2100), \
	DEF(VBUS_PRESENT, 700, 7000), \
	DEF(MAX_NUM,	-1, -1) }

#define DEF(_idx, _min, _max)		REF_##_idx
enum voltage_limits LIMITS;
#undef DEF

struct v_limits {
	int min, max;
};

#define DEF(_idx, _min, _max) \
	[REF_##_idx] = { \
		.min = _min, \
		.max = _max, \
	}
static struct v_limits voltage_refs[REF_MAX_NUM+1] = LIMITS;
#undef DEF

static bool in_range(int voltage, int idx)
{
	bool result = false;
	if (idx >= 0 && idx < REF_MAX_NUM) {
		if ((voltage >= voltage_refs[idx].min) &&
		    (voltage <= voltage_refs[idx].max))
			result = true;
	}
	return result;
}

#define MODE_NORMAL	0
#define MODE_F_USB	1
#define MODE_F_UART	2
#define MODE_F_AUDIO	3

static unsigned long debug_mask = PRINT_ERROR | PRINT_DEBUG |
			PRINT_STATUS | PRINT_TRANSITION;

#define pr_emu_det(debug_level_mask, fmt, args...) \
	do { \
		if (debug_mask & \
		    PRINT_##debug_level_mask) { \
			pr_info("EMU-det(%s): " fmt, __func__, ##args); \
		} \
	} while (0)


static BLOCKING_NOTIFIER_HEAD(emu_det_notifier_list);

void emu_det_register_notify(struct notifier_block *nb)
{
	blocking_notifier_chain_register(&emu_det_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(emu_det_register_notify);

static BLOCKING_NOTIFIER_HEAD(semu_audio_notifier_list);

void semu_audio_register_notify(struct notifier_block *nb)
{
	blocking_notifier_chain_register(&semu_audio_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(semu_audio_register_notify);

static BLOCKING_NOTIFIER_HEAD(charger_vlcl_notifier_list);

void emu_det_vlcl_register_notify(struct notifier_block *nb)
{
	blocking_notifier_chain_register(&charger_vlcl_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(emu_det_vlcl_register_notify);

#undef DEF
#define STATES	{ \
	DEF(CONFIG), \
	DEF(SAMPLE), \
	DEF(SAMPLE_1), \
	DEF(IDENTIFY), \
	DEF(IDENTIFY_WHISPER_SMART), \
	DEF(USB), \
	DEF(FACTORY), \
	DEF(CHARGER), \
	DEF(WHISPER_SMART), \
	DEF(CHARGER_INDUCTIVE), \
	DEF(USB_ADAPTER), \
	DEF(WHISPER_SMART_LD2_OPEN), \
	DEF(WHISPER_SMART_LD2_CLOSE), \
	DEF(STATE_MAX) }


#define DEF(a)	a
enum emu_det_state STATES;
#undef DEF

#define DEF(a)	#a
static char *state_names[] = STATES;
#undef DEF

#undef DEF
#define ACCYS { \
	DEF(USB), \
	DEF(FACTORY), \
	DEF(CHARGER), \
	DEF(USB_DEVICE), \
	DEF(WHISPER_PPD), \
	DEF(WHISPER_SPD), \
	DEF(WHISPER_SMART), \
	DEF(NONE), \
	DEF(UNKNOWN), \
	DEF(MAX) }

#define DEF(a)	ACCY_##a
enum emu_det_accy ACCYS;
#undef DEF

#define DEF(a)	#a
static char *accy_names[] = ACCYS;
#undef DEF

enum emu_det_interrupts {
	SEMU_PPD_DET_IRQ = 0,
	EMU_SCI_OUT_IRQ,
	EMU_DET_IRQ_MAX
};

#undef DEF
#define BITS { \
	DEF(SESS_END), \
	DEF(SESS_VLD), \
	DEF(DP), \
	DEF(DM), \
	DEF(SE1), \
	DEF(FACTORY), \
	DEF(FLOAT), \
	DEF(GRND), \
	DEF(PD_100K), \
	DEF(PD_200K), \
	DEF(PD_440K), \
	DEF(NO_DCD), \
	DEF(USB_DEV), \
	DEF(SENSE_MAX) }

#define DEF(a)	a##_BIT
enum emu_det_sense_bits BITS;
#undef DEF

#define DEF(a)	#a
static char *bit_names[] = BITS;
#undef DEF

#define SENSE_USB           (test_bit(FLOAT_BIT, &data->sense) && \
			     test_bit(SESS_VLD_BIT, &data->sense) && \
			     test_bit(USB_DEV_BIT, &data->sense))

#define SENSE_FACTORY       (test_bit(FACTORY_BIT, &data->sense) && \
			     test_bit(FLOAT_BIT, &data->sense) && \
			     test_bit(SESS_VLD_BIT, &data->sense))

#define SENSE_CHARGER_FLOAT (test_bit(FLOAT_BIT, &data->sense) && \
			     test_bit(SESS_VLD_BIT, &data->sense) && \
			     test_bit(SE1_BIT, &data->sense))

#define SENSE_CHARGER       (test_bit(SESS_VLD_BIT, &data->sense) && \
			     test_bit(SE1_BIT, &data->sense))

#define SENSE_WHISPER_PPD   (test_bit(SESS_END_BIT, &data->sense) && \
			     (test_bit(PD_100K_BIT, &data->sense) || \
			      test_bit(PD_200K_BIT, &data->sense)) &&  \
			     !(test_bit(SE1_BIT, &data->sense)))

#define SENSE_WHISPER_SMART (test_bit(GRND_BIT, &data->sense) && \
			     test_bit(DP_BIT, &data->sense) && \
			     test_bit(SESS_VLD_BIT, &data->sense))

#define SENSE_WHISPER_SMART_SE1 (test_bit(GRND_BIT, &data->sense) && \
				 test_bit(SE1_BIT, &data->sense) && \
				 test_bit(SESS_VLD_BIT, &data->sense) && \
				 test_bit(DM_BIT, &data->sense) && \
				 test_bit(DP_BIT, &data->sense))

#define SENSE_WHISPER_LD2   (test_bit(GRND_BIT, &data->sense) && \
			     test_bit(SE1_BIT, &data->sense) && \
			     test_bit(SESS_VLD_BIT, &data->sense) && \
			     test_bit(DM_BIT, &data->sense) && \
			     test_bit(DP_BIT, &data->sense))

#define SENSE_USB_ADAPTER   (test_bit(GRND_BIT, &data->sense))

#define SENSE_DISCONNECT    (test_bit(SESS_END_BIT, &data->sense) && \
			     test_bit(FLOAT_BIT, &data->sense))

static DEFINE_MUTEX(whisper_ioctl);

struct emu_det_data {
	struct device *dev;
	struct delayed_work work;
	struct workqueue_struct *wq;
	unsigned long sense;
	unsigned long prev_sense;
	u32 otgsc_mask;
	enum emu_det_state state;
	enum emu_det_accy accy;
	struct platform_device *usb_dev;
	struct platform_device *usb_connected_dev;
	struct platform_device *charger_connected_dev;
	struct regulator *regulator;
	struct wake_lock wake_lock;
	bool suspend_disabled;
	struct switch_dev wsdev; /* Whisper switch */
	struct switch_dev dsdev; /* Standard Dock switch */
	struct switch_dev asdev; /* Audio switch */
	struct switch_dev edsdev; /* Motorola Dock switch */
	struct switch_dev sdsdev; /* Smart Dock Switch */
	struct switch_dev csdev;  /* charge capablity switch */
	struct switch_dev noauthdev; /* If authentication is needed */
	DECLARE_BITMAP(enabled_irqs, EMU_DET_IRQ_MAX);
	char dock_id[128];
	char dock_prop[128];
	short whisper_auth;
	char charger_vlcl[CHARGER_CAPABILITY_SIZE];
	char charger_msg[CHARGER_MSG_SIZE];
	struct emu_det_vlcl_request *vlcl_req;
	u8 retries;
	bool irq_setup;
	signed emu_gpio[EMU_DET_GPIO_MAX];
	unsigned emu_irq[EMU_DET_IRQ_MAX];
	atomic_t in_lpm, last_irq, semu_alt_mode;
	int async_int;
	int usb_present;
	bool protection_forced_off;
	int se1_mode;
	bool ext_5v_switch_enabled;
	struct  usb_phy *trans;
	int requested_muxmode;
	bool vbus_adc_delay;
	int driver_mode;
	int reverse_mode_enable;
	bool whisper_ioctl_context;
	bool is_chg_plugged_in;
};

#define VBUS_ADC_READ_SETTLE_TIME 20	/* ms */
#define SCI_OUT_DEBOUNCE_TMOUT	400	/* ms */
#define VBUS_5V_LIMIT		3999	/* mV */
#define VBUS_1DOT8V_LIMIT	1800	/* mV */
#define DELAY_100MS             100     /* ms */
#define DELAY_1S                1000    /* ms */

#define external_5V_enable()	external_5V_setup(1)
#define external_5V_disable()	external_5V_setup(0)
#define alternate_mode_enable()	alt_mode_setup(1)
#define standard_mode_enable()	alt_mode_setup(0)
#define emu_id_protection_on()	emu_id_protection_setup(true)
#define emu_id_protection_off()	emu_id_protection_setup(false)
#define gsbi_ctrl_reg_store()	gsbi_ctrl_register(0)
#define gsbi_ctrl_reg_restore()	gsbi_ctrl_register(1)
#define enable_se1_detection()	dp_dm_pm_gpio_mode(GPIO_MODE_ALTERNATE)
#define enable_dpdm_detection()	dp_dm_pm_gpio_mode(GPIO_MODE_ALTERNATE_2)
#define disable_se1_detection()	dp_dm_pm_gpio_mode(GPIO_MODE_STANDARD)

static struct mmi_emu_det_platform_data *emu_pdata;
static struct emu_det_data *the_emud;
static DEFINE_MUTEX(switch_access);
static DEFINE_MUTEX(mux_mode_switch);
static char buffer[512];

static ssize_t dock_print_name(struct switch_dev *switch_dev, char *buf)
{
	switch (switch_get_state(switch_dev)) {
	case NO_DOCK:
		return sprintf(buf, "None\n");
	case DESK_DOCK:
		return sprintf(buf, "DESK\n");
	case CAR_DOCK:
		return sprintf(buf, "CAR\n");
	case LE_DOCK:
		return sprintf(buf, "LE\n");
	case HE_DOCK:
		return sprintf(buf, "HE\n");
	case MOBILE_DOCK:
		return sprintf(buf, "MOBILE\n");
	case CHARGER_DOCK:
		return sprintf(buf, "CHARGER\n");
	}

	return -EINVAL;
}

static ssize_t emu_audio_print_name(struct switch_dev *sdev, char *buf)
{
	switch (switch_get_state(sdev)) {
	case NO_DEVICE:
		return sprintf(buf, "No Device\n");
	case EMU_OUT:
		return sprintf(buf, "Stereo out\n");
	}
	return -EINVAL;
}

static ssize_t charger_msg_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct emu_det_data *emud = the_emud;
	return snprintf(buf, PAGE_SIZE, "%s\n", emud->charger_msg);
}
static DEVICE_ATTR(charger_msg, S_IRUGO, charger_msg_show, NULL);

static char *print_sense_bits(unsigned long *sense)
{
	char *start = &buffer[0];
	int i, total = sizeof(buffer);
	buffer[0] = 0;
	for (i = 0; i < SENSE_MAX_BIT; i++)
		if (test_bit(i, sense)) {
			total -= snprintf(start, total, "%s, ", bit_names[i]);
			start = buffer + strlen(buffer);
		}
	return buffer;
}

static const char *print_accy_name(enum emu_det_accy accy)
{
	return (accy < ACCY_MAX ? accy_names[accy] : "NA");
}

static const char *print_state_name(enum emu_det_state st)
{
	return (st < STATE_MAX ? state_names[st] : "NA");
}

static void set_whisper_ioctl_context_flag(void)
{
	mutex_lock(&whisper_ioctl);
	the_emud->whisper_ioctl_context = true;
	mutex_unlock(&whisper_ioctl);
	pr_emu_det(DEBUG, "Called from whisper ioctl context\n");
}

static bool clear_whisper_ioctl_context_flag(void)
{
	bool status = false;
	mutex_lock(&whisper_ioctl);
	if (the_emud->whisper_ioctl_context) {
		the_emud->whisper_ioctl_context = false;
		status = true;
	}
	mutex_unlock(&whisper_ioctl);
	if (status)
		pr_emu_det(DEBUG, "Cleared the whisper ioctl context flag\n");
	return status;
}

static void whisper_gpio_mode(int mode)
{
	if (emu_pdata->gpio_mode)
		emu_pdata->gpio_mode(mode);
}

static void gsbi_ctrl_register(int restore)
{
	if (emu_pdata->gsbi_ctrl)
		emu_pdata->gsbi_ctrl(restore);
}

void switch_mux_mode(int mode, int check_valid_req)
{
	struct emu_det_data *emud = the_emud;
	int emu_audio = switch_get_state(&emud->asdev);

	mutex_lock(&mux_mode_switch);
	if (check_valid_req) {
		if (((!emu_audio) && (emud->requested_muxmode != MUXMODE_USB))
		    || ((emu_audio) && (emud->requested_muxmode !=
				      MUXMODE_AUDIO)))
			mode = MUXMODE_UART;
	}
	emud->requested_muxmode = MUXMODE_UNDEFINED;

	switch (mode) {
	case MUXMODE_USB:
		gpio_set_value_cansleep(emud->emu_gpio[EMU_MUX_CTRL1_GPIO], 0);
		gpio_set_value_cansleep(emud->emu_gpio[EMU_MUX_CTRL0_GPIO], 1);
		break;
	case MUXMODE_AUDIO:
		gpio_set_value_cansleep(emud->emu_gpio[EMU_MUX_CTRL1_GPIO], 1);
		gpio_set_value_cansleep(emud->emu_gpio[EMU_MUX_CTRL0_GPIO], 1);
		break;
	case MUXMODE_UART:
		gpio_set_value_cansleep(emud->emu_gpio[EMU_MUX_CTRL0_GPIO], 0);
		gpio_set_value_cansleep(emud->emu_gpio[EMU_MUX_CTRL1_GPIO], 1);
		break;
	default:
		break;
	}

	mutex_unlock(&mux_mode_switch);
}

void set_mux_ctrl_mode_for_audio(int mode)
{
	struct emu_det_data *emud = the_emud;
	int emu_audio = switch_get_state(&emud->asdev);

	if (emud->driver_mode == MODE_F_AUDIO) {
		switch_mux_mode(MUXMODE_AUDIO, 0);
		return;
	}

	if ((emud->whisper_auth == AUTH_FAILED) ||
	    (emud->whisper_auth == AUTH_NOT_STARTED)) {
		pr_err("Invalid Call to emu_det driver by Audio driver\n");
		return;
	}

	if (emud->whisper_auth == AUTH_IN_PROGRESS) {
		if ((mode < MUXMODE_UNDEFINED) &&
		    (emud->state == CHARGER))
			emud->requested_muxmode = mode;
		return;
	}

	if ((emud->whisper_auth == AUTH_PASSED) &&
	    (emud->state == CHARGER)) {
		if ((!emu_audio) ||
		    (mode != MUXMODE_AUDIO))
			mode = MUXMODE_USB;

		emud->requested_muxmode = mode;
		switch_mux_mode(mode, 0);
	}
}

static void mux_ctrl_mode(int mode)
{
	struct emu_det_data *emud = the_emud;
	int check_validity = 0;

	if ((emud->requested_muxmode != MUXMODE_UNDEFINED) &&
	    (emud->state == CHARGER))
		check_validity = 1;

	switch_mux_mode(mode, check_validity);
}

/* Returns voltage in mV */
static int adc_emu_id_get(void)
{
	int ret = 0;

	if (emu_pdata->adc_id)
		ret = emu_pdata->adc_id();

	pr_emu_det(DEBUG, "emu_id=%d mV\n", ret);
	return ret;
}

/* Returns voltage in mV */
static int adc_vbus_get(void)
{
	struct pm8xxx_adc_chan_result res;
	int ret = 0;

	pm8921_charger_usb_suspend_clear();
	memset(&res, 0, sizeof(res));
	if (pm8xxx_adc_read(CHANNEL_USBIN, &res))
		pr_err("VBUS ADC read error\n");
	else {
		ret = (int)res.physical/1000;
		pr_emu_det(DEBUG, "vbus=%d mV\n", ret);
	}
	return ret;
}


static void emu_det_enable_irq(int interrupt)
{
	struct emu_det_data *emud = the_emud;
	if ((emud->emu_irq[interrupt] > 0) &&
	    !test_and_set_bit(interrupt, emud->enabled_irqs)) {
		pr_emu_det(PARANOIC, "enabling irq %d\n",
					emud->emu_irq[interrupt]);
		enable_irq(emud->emu_irq[interrupt]);
	}
}

static void emu_det_disable_irq(int interrupt)
{
	struct emu_det_data *emud = the_emud;
	if ((emud->emu_irq[interrupt] > 0) &&
	    test_and_clear_bit(interrupt, emud->enabled_irqs)) {
		pr_emu_det(PARANOIC, "disabling irq %d\n",
					emud->emu_irq[interrupt]);
		disable_irq_nosync(emud->emu_irq[interrupt]);
	}
}

static void emu_id_protection_setup(bool on)
{
	struct emu_det_data *emud = the_emud;

	if (hasHoneyBadger()) {
		pr_emu_det(DEBUG, "No neccessary action\n");
		return;
	}

	if (on == emud->protection_forced_off) {
		if (emu_pdata->id_protect)
			emu_pdata->id_protect(on);
		if (on) {
			gpio_direction_input(
				emud->emu_gpio[EMU_ID_EN_GPIO]);
			emud->protection_forced_off = false;
			pr_emu_det(DEBUG, "EMU_ID protection is On\n");
		} else {
			gpio_direction_output(
				emud->emu_gpio[EMU_ID_EN_GPIO], 0);
			emud->protection_forced_off = true;
			pr_emu_det(DEBUG, "EMU_ID protection is Off\n");
		}
	} else
		pr_emu_det(DEBUG, "state is the same as requested\n");
}

static void alt_mode_setup(bool on)
{
	struct emu_det_data *emud = the_emud;
	if (emu_pdata->alt_mode)
		emu_pdata->alt_mode(on);
	atomic_set(&emud->semu_alt_mode, on);
	pr_emu_det(DEBUG, "SEMU_ALT_MODE_EN is %s\n",
		on ? "alternate" : "standard");
}

static void external_5V_setup(bool disable)
{
	if (emu_pdata->enable_5v) {
		struct emu_det_data *emud = the_emud;
		if (!disable) {
			if (emud->ext_5v_switch_enabled) {
				emu_pdata->enable_5v(0);
				emud->ext_5v_switch_enabled = false;
				pr_emu_det(DEBUG, "5VS_OTG disabled\n");
			}
		} else {
			if (!emud->ext_5v_switch_enabled) {
				emu_pdata->enable_5v(1);
				emu_id_protection_off();
				emud->ext_5v_switch_enabled = true;
				pr_emu_det(DEBUG, "5VS_OTG enabled\n");
			}
		}
	}
}

static void dp_dm_pm_gpio_mode(int mode)
{
	struct emu_det_data *emud = the_emud;

	if (emu_pdata->dp_dm_mode) {
		emu_pdata->dp_dm_mode(mode);

		emud->se1_mode = mode;

		clear_bit(NO_DCD_BIT, &emud->sense);
		clear_bit(USB_DEV_BIT, &emud->sense);
		clear_bit(SE1_BIT, &emud->sense);
		clear_bit(DP_BIT, &emud->sense);
		clear_bit(DM_BIT, &emud->sense);

		pr_emu_det(PARANOIC, "D+/D- detection: %s\n",
					mode ? "enabled" : "disabled");
	}
}

static void emu_det_vbus_state(int online)
{
	struct emu_det_data *emud = the_emud;
	int delay_ms = 50;

	if (emud->vbus_adc_delay) {
		if (online)
			delay_ms = 500;
		emud->vbus_adc_delay = false;
	}

	pr_emu_det(DEBUG, "PM8921 USBIN callback: %s\n",
					online ? "in" : "out");
	emud->usb_present = online;
	if (emud->driver_mode == MODE_NORMAL)
		queue_delayed_work(emud->wq, &emud->work,
					msecs_to_jiffies(delay_ms));
}

static void sense_emu_id(unsigned long *lsense, bool sci_out_debounce)
{
	struct emu_det_data *emud = the_emud;
	bool auto_protected = false;
	int id, vbus_voltage = adc_vbus_get();

	clear_bit(PD_100K_BIT, lsense);
	clear_bit(PD_200K_BIT, lsense);

	/* determine whether id protection is on */
	if (in_range(vbus_voltage, REF_VBUS_PRESENT)) {
		auto_protected = true;
		set_bit(SESS_VLD_BIT, lsense);
		clear_bit(SESS_END_BIT, lsense);
	} else {
		set_bit(SESS_END_BIT, lsense);
		clear_bit(SESS_VLD_BIT, lsense);
		clear_bit(FACTORY_BIT, lsense);
	}

	if (hasHoneyBadger())
		return;
	if (sci_out_debounce)
		msleep(SCI_OUT_DEBOUNCE_TMOUT);
	id = adc_emu_id_get();

	if (auto_protected && !emud->protection_forced_off) {
		if (in_range(id, REF_FACTORY)) {
			set_bit(FACTORY_BIT, lsense);
			set_bit(FLOAT_BIT, lsense);
			return;
		} else {
			emu_det_disable_irq(SEMU_PPD_DET_IRQ);
			emu_det_disable_irq(EMU_SCI_OUT_IRQ);

			clear_bit(FACTORY_BIT, lsense);
			emu_id_protection_off();
			msleep(20);
			id = adc_emu_id_get();

			emu_det_enable_irq(SEMU_PPD_DET_IRQ);
			emu_det_enable_irq(EMU_SCI_OUT_IRQ);
		}
	}

	if (atomic_read(&emud->semu_alt_mode)) {
		if (in_range(id, REF_ALT_100K)) {
			set_bit(PD_100K_BIT, lsense);
			clear_bit(GRND_BIT, lsense);
			clear_bit(FLOAT_BIT, lsense);
		} else if (in_range(id, REF_ALT_200K)) {
			set_bit(PD_200K_BIT, lsense);
			clear_bit(GRND_BIT, lsense);
			clear_bit(FLOAT_BIT, lsense);
		}
	} else {
		if (in_range(id, REF_STD_100K)) {
			set_bit(PD_100K_BIT, lsense);
			clear_bit(GRND_BIT, lsense);
			clear_bit(FLOAT_BIT, lsense);
		}
	}
}

static void sense_dp_dm(unsigned long *lsense)
{
	struct emu_det_data *emud = the_emud;
	char mask;

	mask = gpio_get_value_cansleep(emud->emu_gpio[DPLUS_GPIO]) << 4;
	mask |= gpio_get_value_cansleep(emud->emu_gpio[DMINUS_GPIO]);

	pr_emu_det(DEBUG, "D+/D- mask: 0x%x\n", mask);

	if (emud->se1_mode == GPIO_MODE_ALTERNATE) {
		clear_bit(NO_DCD_BIT, lsense);
		clear_bit(USB_DEV_BIT, lsense);
		clear_bit(SE1_BIT, lsense);
		switch (mask) {
		case 0x00: /* USB host */
			set_bit(USB_DEV_BIT, lsense);
			break;
		case 0x10: /* partial insertion */
			set_bit(NO_DCD_BIT, lsense);
			break;
		case 0x11: /* SE1 */
			set_bit(SE1_BIT, lsense);
			break;
		default: /* nonsense */
			pr_emu_det(PARANOIC, "D+/D- mask is weird!!!\n");
			break;
		}
	} else if (emud->se1_mode == GPIO_MODE_ALTERNATE_2) {
		clear_bit(DP_BIT, lsense);
		clear_bit(DM_BIT, lsense);
		switch (mask) {
		case 0x10: /* D+ */
			set_bit(DP_BIT, lsense);
			break;
		case 0x01: /* D- */
			set_bit(DM_BIT, lsense);
			break;
		case 0x11: /* D+ & D- */
			set_bit(DP_BIT, lsense);
			set_bit(DM_BIT, lsense);
			break;
		default: /* nonsense */
			pr_emu_det(PARANOIC, "D+/D- mask is weird!!!\n");
			break;
		}
	}
}

static int get_id_resistance(bool sci_out_debounce)
{
	struct emu_det_data *emud = the_emud;
	int value, first_read = -1;
	if (sci_out_debounce) {
		pr_emu_det(DEBUG, "Debouncing DMB_PPD_DET...\n");
		msleep(SCI_OUT_DEBOUNCE_TMOUT);
	}
once_more:
	if (atomic_read(&emud->semu_alt_mode)) {
		standard_mode_enable();
		msleep(20);
		value = gpio_get_value_cansleep(
				emud->emu_gpio[DMB_PPD_DET_GPIO]);
		alternate_mode_enable();
	} else {
		msleep(20);
		value = gpio_get_value_cansleep(
				emud->emu_gpio[DMB_PPD_DET_GPIO]);
	}
	if (value != first_read) {
		first_read = value;
		goto once_more;
	}
	return value;
}

static int get_id_extreme(int gpio)
{
	int value, first_read = -1;

once_more:
	msleep(20);
	value = gpio_get_value_cansleep(gpio);

	if (value != first_read) {
		first_read = value;
		goto once_more;
	}
	return value;
}

static int get_sense(void)
{
	struct emu_det_data *emud = the_emud;
	unsigned long lsense = emud->sense;
	bool sci_out = false;
	int irq, value;

	irq = atomic_read(&emud->last_irq);
	atomic_set(&emud->last_irq, 0);
	if (irq == emud->emu_irq[EMU_SCI_OUT_IRQ] &&
		emud->whisper_auth == AUTH_PASSED)
		sci_out = true;

	sense_emu_id(&lsense, sci_out);

	value = get_id_extreme(emud->emu_gpio[SEMU_PPD_DET_GPIO]);

	if (!hasHoneyBadger()) {
		/* Discrete logic has inverse polarity */
		value = !value;
	}
	if (value) {
		set_bit(FLOAT_BIT, &lsense);
		pr_emu_det(STATUS, "SEMU_PPD_DET: ID floating\n");
	} else
		clear_bit(FLOAT_BIT, &lsense);

	value = get_id_extreme(emud->emu_gpio[EMU_SCI_OUT_GPIO]);
	if (value) {
		set_bit(GRND_BIT, &lsense);
		pr_emu_det(STATUS, "EMU_SCI_OUT: ID grounded\n");
	} else
		clear_bit(GRND_BIT, &lsense);

	if (hasHoneyBadger()) {
		clear_bit(FACTORY_BIT, &lsense);
		if (test_bit(GRND_BIT, &lsense) &&
		    test_bit(FLOAT_BIT, &lsense)) {
			set_bit(FACTORY_BIT, &lsense);
		}

		clear_bit(PD_100K_BIT, &lsense);
		clear_bit(PD_200K_BIT, &lsense);
		if (!test_bit(GRND_BIT, &lsense) &&
		    !test_bit(FLOAT_BIT, &lsense)) {
			value = get_id_resistance(sci_out);
			pr_emu_det(DEBUG, "ID %s present\n",
				   value ? "200K" : "100K");
			if (value)
				set_bit(PD_200K_BIT, &lsense);
			else
				set_bit(PD_100K_BIT, &lsense);
		} else
			pr_emu_det(DEBUG, "No need to check DMB_PPD_DET_N\n");
	}

	if (emud->se1_mode)
		sense_dp_dm(&lsense);

	emud->sense = lsense;
	pr_emu_det(STATUS, "irq=%d, %s\n", irq, print_sense_bits(&lsense));

	return irq;
}

static int configure_hardware(enum emu_det_accy accy)
{
	struct emu_det_data *emud = the_emud;
	int retval = 0;

	switch (accy) {
	case ACCY_UNKNOWN:
		mux_ctrl_mode(MUXMODE_UART);
		enable_se1_detection();
		gsbi_ctrl_reg_restore();
		standard_mode_enable();
		external_5V_disable();
		emud->is_chg_plugged_in = false;
		break;

	case ACCY_WHISPER_SMART:
		emu_det_enable_irq(EMU_SCI_OUT_IRQ);
	case ACCY_USB:
	case ACCY_FACTORY:
		mux_ctrl_mode(MUXMODE_USB);
		disable_se1_detection();
		gsbi_ctrl_reg_restore();
		standard_mode_enable();
		external_5V_disable();
		break;

	case ACCY_CHARGER:
		disable_se1_detection();
		gsbi_ctrl_reg_store();
		external_5V_disable();
		emu_det_enable_irq(EMU_SCI_OUT_IRQ);
		emu_det_disable_irq(SEMU_PPD_DET_IRQ);
		emud->is_chg_plugged_in = true;
		break;

	case ACCY_WHISPER_PPD:
		disable_se1_detection();
		gsbi_ctrl_reg_store();
		external_5V_disable();
		emu_det_enable_irq(EMU_SCI_OUT_IRQ);
		emu_det_enable_irq(SEMU_PPD_DET_IRQ);
		emud->is_chg_plugged_in = false;
		break;

	case ACCY_USB_DEVICE:
		mux_ctrl_mode(MUXMODE_USB);
		disable_se1_detection();
		gsbi_ctrl_reg_restore();
		standard_mode_enable();
		external_5V_enable();
		emu_det_enable_irq(EMU_SCI_OUT_IRQ);
		break;

	case ACCY_NONE:
		mux_ctrl_mode(MUXMODE_USB);
		disable_se1_detection();
		gsbi_ctrl_reg_restore();
		standard_mode_enable();
		emud->whisper_auth = AUTH_NOT_STARTED;
		external_5V_disable();
		emu_id_protection_on();
		emu_det_disable_irq(EMU_SCI_OUT_IRQ);
		emu_det_enable_irq(SEMU_PPD_DET_IRQ);
		break;
	default:
		break;
	}

	if (retval != 0)
		retval = -EFAULT;

	return retval;
}

static void notify_otg(enum emu_det_accy accy)
{
	struct emu_det_data *data = the_emud;

	switch (accy) {
	case ACCY_NONE:
	case ACCY_WHISPER_PPD:
		atomic_notifier_call_chain(&data->trans->notifier,
					USB_EVENT_NONE, NULL);
		break;

	case ACCY_USB:
	case ACCY_FACTORY:
		atomic_notifier_call_chain(&data->trans->notifier,
					USB_EVENT_VBUS, NULL);
		break;

	case ACCY_CHARGER:
		atomic_notifier_call_chain(&data->trans->notifier,
					USB_EVENT_CHARGER, NULL);
		break;

	case ACCY_USB_DEVICE:
	case ACCY_WHISPER_SMART:
		atomic_notifier_call_chain(&data->trans->notifier,
					USB_EVENT_ID, NULL);
		break;
	default:
		break;
	}
}

static void notify_accy(enum emu_det_accy accy)
{
	struct emu_det_data *data = the_emud;

	data->accy = accy;
	pr_emu_det(STATUS, "EMU detection Notify: %s\n", print_accy_name(accy));

	/* configure hardware must occur before notifying OTG */
	configure_hardware(accy);
	notify_otg(accy);

	/* Always go through None State */
	if (data->accy != ACCY_NONE)
		blocking_notifier_call_chain(&emu_det_notifier_list,
					     (unsigned long)ACCY_NONE,
					     (void *)NULL);
	else
		blocking_notifier_call_chain(&semu_audio_notifier_list,
					     (unsigned long)NO_DEVICE,
					     (void *)NULL);

	blocking_notifier_call_chain(&emu_det_notifier_list,
				     (unsigned long)accy,
				     (void *)NULL);
}

static void notify_whisper_switch(enum emu_det_accy accy)
{
	struct emu_det_data *data = the_emud;

	if ((accy == ACCY_CHARGER) || (accy == ACCY_WHISPER_PPD)) {
		switch_set_state(&data->wsdev, DOCKED);
		pr_emu_det(PARANOIC, "whisper switch set to DOCKED\n");
	/* LD2 open->close */
	} else if ((accy == ACCY_WHISPER_SMART) &&
		   (data->state == WHISPER_SMART_LD2_CLOSE)) {

		if (data->whisper_auth == AUTH_PASSED)
			switch_set_state(&data->noauthdev, AUTH_NOT_REQUIRED);
		else
			switch_set_state(&data->noauthdev, AUTH_REQUIRED);

		switch_set_state(&data->dsdev, NO_DOCK);
		switch_set_state(&data->edsdev, NO_DOCK);
		switch_set_state(&data->wsdev, DOCKED);
		switch_set_state(&data->sdsdev, UNDOCKED);
		switch_set_state(&data->csdev, CHARGE_NONE);

	} else {
		switch_set_state(&data->wsdev, UNDOCKED);
		pr_emu_det(PARANOIC, "whisper switch set to UNDOCKED\n");
		switch_set_state(&data->dsdev, NO_DOCK);
		switch_set_state(&data->edsdev, NO_DOCK);
		switch_set_state(&data->asdev, NO_DEVICE);
		switch_set_state(&data->csdev, CHARGE_NONE);
		if (accy == ACCY_WHISPER_SMART)
			switch_set_state(&data->sdsdev, DOCKED);
		else {
			switch_set_state(&data->noauthdev, AUTH_REQUIRED);
			switch_set_state(&data->sdsdev, UNDOCKED);
			memset(data->dock_id, 0, CPCAP_WHISPER_ID_SIZE);
			memset(data->dock_prop, 0, CPCAP_WHISPER_PROP_SIZE);
			memset(data->charger_vlcl, 0, CHARGER_CAPABILITY_SIZE);
		}
	}
}

static void whisper_audio_check(void)
{
	struct emu_det_data *data = the_emud;
	int audio = NO_DEVICE;

	if (!switch_get_state(&data->dsdev))
		return;

	if (data->accy == ACCY_WHISPER_PPD)
		goto exit;

	if (test_bit(PD_200K_BIT, &data->sense)) {
		audio = EMU_OUT;
		pr_emu_det(DEBUG, "HEADSET attached\n");
		alternate_mode_enable();
	} else if (test_bit(PD_100K_BIT, &data->sense)) {
		pr_emu_det(DEBUG, "HEADSET detached\n");
		standard_mode_enable();
	}

 exit:
	switch_set_state(&data->asdev, audio);
	blocking_notifier_call_chain(&semu_audio_notifier_list,
				     (unsigned long)audio,
				     (void *)NULL);
}

#define POLL_TIME		(100 * HZ/1000) /* 100 msec */
#define WASTE_CYCLE_TIME	(50 * HZ/1000)

static void detection_work(void)
{
	struct emu_det_data *data = the_emud;
	int last_irq;
	unsigned long delay = 0;
	bool whisper_ioctl_status = clear_whisper_ioctl_context_flag();

	static unsigned long last_run;

	if (data->driver_mode != MODE_NORMAL)
		return;

	pr_emu_det(DEBUG, "state %s, time since last run %d ms\n",
				print_state_name(data->state),
				jiffies_to_msecs(jiffies-last_run));
	/* store last run info */
	last_run = jiffies;

	switch (data->state) {
	case CONFIG:
		if (!data->suspend_disabled) {
			data->suspend_disabled = true;
			wake_lock(&data->wake_lock);
		}
		memset(&data->sense, 0, sizeof(data->sense));

		configure_hardware(ACCY_UNKNOWN);
		switch_set_state(&data->noauthdev, AUTH_REQUIRED);
		data->retries = 0;
		data->state = SAMPLE;

		queue_delayed_work(data->wq, &data->work,
					msecs_to_jiffies(WASTE_CYCLE_TIME));
		break;

	case SAMPLE:
		data->prev_sense = data->sense;
		last_irq = get_sense();

		if (test_bit(GRND_BIT, &data->sense) &&
		    !test_bit(FACTORY_BIT, &data->sense)) {
			pr_emu_det(DEBUG, "DP/DM ...\n");
			data->state = SAMPLE_1;
			enable_dpdm_detection();

		} else if ((test_bit(SESS_VLD_BIT, &data->sense) &&
			   !test_bit(FACTORY_BIT, &data->sense) &&
			    test_bit(NO_DCD_BIT, &data->sense)) ||
			   (data->sense != data->prev_sense)) {
			pr_emu_det(DEBUG, "SAMPLE keep waiting ...\n");
			data->state = SAMPLE;
			delay = msecs_to_jiffies(WASTE_CYCLE_TIME);
		} else
			data->state = IDENTIFY;

		queue_delayed_work(data->wq, &data->work, delay);
		break;

	case SAMPLE_1:
		data->prev_sense = data->sense;
		last_irq = get_sense();

		if (!test_bit(GRND_BIT, &data->sense)) {
			pr_emu_det(DEBUG, "Start over ...\n");
			data->state = CONFIG;

		} else
			data->state = IDENTIFY;

		queue_delayed_work(data->wq, &data->work, delay);
		break;

	case IDENTIFY:
		data->state = CONFIG;
		last_irq = get_sense();

		if (SENSE_FACTORY) {
			data->state = FACTORY;
			notify_accy(ACCY_FACTORY);

		} else if (SENSE_USB) {
			data->state = USB;
			notify_accy(ACCY_USB);

		} else if (SENSE_WHISPER_SMART ||
			   SENSE_WHISPER_LD2 ||
			   SENSE_USB_ADAPTER) {
			pr_emu_det(STATUS,
					"detection_work: IW Identified\n");
			data->state = IDENTIFY_WHISPER_SMART;
			queue_delayed_work(data->wq, &data->work, 0);

		} else if (SENSE_CHARGER_FLOAT ||
			   SENSE_CHARGER) {
			pr_emu_det(STATUS,
				"detection_work: CHARGER Identified\n");
			notify_accy(ACCY_CHARGER);
			notify_whisper_switch(ACCY_CHARGER);
			data->state = CHARGER;
		} else if (SENSE_WHISPER_PPD) {
			pr_emu_det(STATUS,
				"detection_work: PPD Identified\n");
			emu_id_protection_off();
			notify_accy(ACCY_WHISPER_PPD);
			notify_whisper_switch(ACCY_WHISPER_PPD);
			data->state = CHARGER;
		} else {
			pr_emu_det(DEBUG, "no accessory\n");
			notify_accy(ACCY_NONE);
			notify_whisper_switch(ACCY_NONE);
			if (data->suspend_disabled) {
				data->suspend_disabled = false;
				wake_unlock(&data->wake_lock);
			}
		}
		break;

	case IDENTIFY_WHISPER_SMART:
		data->state = CONFIG;

		if (SENSE_WHISPER_SMART ||
		    SENSE_WHISPER_LD2) {
			pr_emu_det(STATUS,
				"detection_work: SMART Identified\n");
			if (SENSE_WHISPER_SMART)
				data->state = WHISPER_SMART;
			else {
				switch_set_state(&data->noauthdev,
						AUTH_REQUIRED);
				data->state = WHISPER_SMART_LD2_OPEN;
			}
			notify_accy(ACCY_WHISPER_SMART);
			notify_whisper_switch(ACCY_WHISPER_SMART);
		} else if (SENSE_USB_ADAPTER) {
			pr_emu_det(STATUS,
				"detection_work: USB Adapter Identified\n");
			notify_accy(ACCY_USB_DEVICE);
			data->state = USB_ADAPTER;
		} else
			queue_delayed_work(data->wq, &data->work, 0);
		break;

	case USB:
		last_irq = get_sense();

		if (!test_bit(SESS_VLD_BIT, &data->sense)) {
			data->state = CONFIG;
			queue_delayed_work(data->wq, &data->work, 0);
		}
		break;

	case FACTORY:
		last_irq = get_sense();

		if (!test_bit(SESS_VLD_BIT, &data->sense)) {
			data->state = CONFIG;
			queue_delayed_work(data->wq, &data->work, 0);
		}
		break;

	case CHARGER:
		pr_emu_det(STATUS, "detection_work: CHARGER\n");
		last_irq = get_sense();

		if (!test_bit(SESS_VLD_BIT, &data->sense) &&
		    !test_bit(FLOAT_BIT, &data->sense) &&
		    (data->whisper_auth ==  AUTH_PASSED)) {
			/* SPD->PPD transition */
			pr_emu_det(STATUS, "SPD->PPD transition\n");
			emu_id_protection_off();
			notify_accy(ACCY_WHISPER_PPD);
			notify_whisper_switch(ACCY_WHISPER_PPD);
			whisper_audio_check();
		} else if (!test_bit(SESS_VLD_BIT, &data->sense) &&
			    test_bit(SESS_END_BIT, &data->sense)) {
			/* charger disconnect */
			data->state = CONFIG;
			queue_delayed_work(data->wq, &data->work, 0);
		} else if (((last_irq == data->emu_irq[EMU_SCI_OUT_IRQ]) &&
			     (data->whisper_auth == AUTH_PASSED)) ||
			    whisper_ioctl_status) {
			/* insertion and removal of audio cable */
			whisper_audio_check();
		}

		if ((!data->is_chg_plugged_in) &&
		    (test_bit(SESS_VLD_BIT, &data->sense))) {
			pr_emu_det(STATUS, "PPD->SPD transition\n");
			notify_accy(ACCY_CHARGER);
			notify_whisper_switch(ACCY_CHARGER);
			whisper_audio_check();
		}
		break;

	case WHISPER_SMART:
		pr_emu_det(STATUS, "detection_work: SMART\n");
		last_irq = get_sense();

		if (data->whisper_auth == AUTH_FAILED &&
			data->accy == ACCY_WHISPER_SMART &&
			test_bit(SESS_VLD_BIT, &data->sense)) {
			notify_accy(ACCY_CHARGER);
			pr_emu_det(STATUS, "SMART: failed authentication\n");

		} else if (data->whisper_auth == AUTH_NOT_STARTED) {
			/* wait for authentication results */
			queue_delayed_work(data->wq, &data->work,
								POLL_TIME);
		}

		if (!test_bit(SESS_VLD_BIT, &data->sense)) {
			data->state = CONFIG;
			queue_delayed_work(data->wq, &data->work, 0);
		} else
			data->state = WHISPER_SMART;
		break;

	case USB_ADAPTER:
		last_irq = get_sense();

		if (!test_bit(GRND_BIT, &data->sense)) {
			data->state = CONFIG;
			queue_delayed_work(data->wq, &data->work, 0);
		} else {
			data->state = USB_ADAPTER;
		}
		break;

	case WHISPER_SMART_LD2_OPEN:
		pr_emu_det(STATUS, "detection_work: SMART_LD2_OPEN\n");
		last_irq = get_sense();

		if (data->whisper_auth == AUTH_FAILED &&
			data->accy == ACCY_WHISPER_SMART &&
			!test_bit(SESS_VLD_BIT, &data->sense)) {
			notify_accy(ACCY_CHARGER);
		}

		if ((switch_get_state(&data->edsdev) != MOBILE_DOCK) &&
		    (switch_get_state(&data->sdsdev) == DOCKED) &&
		    test_bit(SESS_VLD_BIT, &data->sense) &&
		    (data->whisper_auth == AUTH_PASSED)) {
			pr_emu_det(STATUS, "SMART_LD2: " \
					"Its a SMART dock with SE1\n");
			data->state = WHISPER_SMART;
			queue_delayed_work(data->wq, &data->work, 0);
			break;
		}

		if (!test_bit(SESS_VLD_BIT, &data->sense)) {
			data->state = CONFIG;
			queue_delayed_work(data->wq, &data->work, 0);
		} else if ((test_bit(SESS_VLD_BIT, &data->sense) &&
				(last_irq == data->emu_irq[EMU_SCI_OUT_IRQ]) &&
				test_bit(GRND_BIT, &data->sense)) ||
			(test_bit(SESS_VLD_BIT, &data->sense) &&
				(last_irq == data->emu_irq[EMU_SCI_OUT_IRQ]) &&
				!test_bit(GRND_BIT, &data->sense) &&
				test_bit(SE1_BIT, &data->sense))) {
			pr_emu_det(STATUS,
				"detection_work: LD2 lid closed\n");
			notify_accy(ACCY_CHARGER);
			data->state = WHISPER_SMART_LD2_CLOSE;
			notify_whisper_switch(ACCY_WHISPER_SMART);
		} else
			data->state = WHISPER_SMART_LD2_OPEN;
		break;

	case WHISPER_SMART_LD2_CLOSE:
		pr_emu_det(STATUS, "detection_work: SMART_LD2_CLOSE\n");
		last_irq = get_sense();

		if (!test_bit(SESS_VLD_BIT, &data->sense)) {
			data->state = CONFIG;
			queue_delayed_work(data->wq, &data->work, 0);
		} else if (test_bit(SESS_VLD_BIT, &data->sense) &&
			(last_irq == data->emu_irq[EMU_SCI_OUT_IRQ]) &&
			test_bit(GRND_BIT, &data->sense)) {
			pr_emu_det(STATUS,
					"detection_work: LD2 lid opened\n");
			data->state = WHISPER_SMART_LD2_OPEN;
			notify_whisper_switch(ACCY_WHISPER_SMART);
			if (data->whisper_auth == AUTH_PASSED)
				switch_set_state(&data->edsdev, MOBILE_DOCK);
			notify_accy(ACCY_WHISPER_SMART);
		} else
			data->state = WHISPER_SMART_LD2_CLOSE;
		break;

	default:
		return;
	}
}

#define DEBOUNCE_TIME	(500*HZ/1000) /* 500ms */

static irqreturn_t emu_det_irq_handler(int irq, void *data)
{
	struct emu_det_data *emud = data;
	static int prev_irq;
	static unsigned long prev_jiffies;

	if (!emud->irq_setup)
		return IRQ_HANDLED;
	/* debounce sporadic IRQs */
	if (prev_jiffies && prev_irq) {
		if ((irq == prev_irq) &&
		     time_before_eq(jiffies, prev_jiffies+DEBOUNCE_TIME)) {
			pr_emu_det(PARANOIC, "debounce sporadic IRQ (%dms)!\n",
				jiffies_to_msecs(jiffies - prev_jiffies));
			goto do_not_schedule;
		}
	}

	if (emud->driver_mode == MODE_NORMAL)
		queue_delayed_work(emud->wq, &emud->work, 0);

	prev_jiffies = jiffies;

do_not_schedule:

	atomic_set(&emud->last_irq, irq);
	prev_irq = irq;

	return IRQ_HANDLED;
}

struct emu_det_irq_init_data {
	unsigned int	idx;
	char		*name;
	unsigned long	flags;
	int		required;
	bool		wakeup;
	irqreturn_t	(*handler)(int, void *);
};

#define OPTIONAL	0
#define REQUIRED	1

#define SCI_OUT_IDX	0

#define	TO_STRING(x)	#x
#define EMU_DET_IRQ(_id, _flags, _req, _wakeup, _handler) \
{ \
	.idx		= _id##_IRQ, \
	.name		= TO_STRING(_id##_GPIO), \
	.flags		= _flags, \
	.required	= _req, \
	.wakeup		= _wakeup, \
	.handler	= _handler, \
}

static struct emu_det_irq_init_data emu_det_irq_data[] = {
	EMU_DET_IRQ(EMU_SCI_OUT, IRQF_TRIGGER_FALLING,
				REQUIRED, false, emu_det_irq_handler),
	EMU_DET_IRQ(SEMU_PPD_DET, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
				REQUIRED, true, emu_det_irq_handler),
};

static void free_irqs(void)
{
	struct emu_det_data *emud = the_emud;
	int i;

	for (i = 0; i < EMU_DET_IRQ_MAX; i++)
		if (emud->emu_irq[i]) {
			free_irq(emud->emu_irq[i], emud);
			pr_emu_det(DEBUG, "free-ed irq %d\n", emud->emu_irq[i]);
			emud->emu_irq[i] = 0;
		}
}

static int __devinit request_irqs(struct platform_device *pdev)
{
	struct emu_det_data *emud = the_emud;
	struct resource *res;
	int ret, i;

	ret = 0;
	bitmap_fill(emud->enabled_irqs, EMU_DET_IRQ_MAX);

	for (i = 0; i < ARRAY_SIZE(emu_det_irq_data); i++) {
		if (emu_det_irq_data[i].required == OPTIONAL) {
			pr_emu_det(DEBUG, "no IRQ for gpio %s\n",
					emu_det_irq_data[i].name);
			clear_bit(emu_det_irq_data[i].idx,
					emud->enabled_irqs);
			continue;
		}

		res = platform_get_resource_byname(pdev,
			IORESOURCE_IO, emu_det_irq_data[i].name);
		if (!res) {
			pr_emu_det(ERROR, "cannot find resource %s;\n",
					emu_det_irq_data[i].name);
			goto err_out;
		}

		emud->emu_irq[emu_det_irq_data[i].idx] =
				gpio_to_irq(res->start);
		if (!emud->emu_irq[emu_det_irq_data[i].idx]) {
			pr_err("failed to get %s\n", emu_det_irq_data[i].name);
			goto err_out;
		}

		pr_emu_det(DEBUG, "requesting irq[%d] %s-%d\n",
				emu_det_irq_data[i].idx,
				emu_det_irq_data[i].name,
				emud->emu_irq[emu_det_irq_data[i].idx]);
		ret = request_irq(emud->emu_irq[emu_det_irq_data[i].idx],
			emu_det_irq_data[i].handler,
			emu_det_irq_data[i].flags,
			emu_det_irq_data[i].name, emud);
		if (ret < 0) {
			pr_err("couldn't request %d (%s) %d\n",
				emud->emu_irq[emu_det_irq_data[i].idx],
				emu_det_irq_data[i].name, ret);
			goto err_out;
		}

		if (emu_det_irq_data[i].wakeup) {
			if (unlikely(irq_set_irq_wake(
				emud->emu_irq[emu_det_irq_data[i].idx], 1)))
				pr_err("unable to make irq %d wakeup\n",
					emud->emu_irq[emu_det_irq_data[i].idx]);
		}
		emu_det_disable_irq(emu_det_irq_data[i].idx);
	}
	return 0;

err_out:
	free_irqs();
	return -EINVAL;
}

struct emu_det_gpio_init_data {
	int	idx;
	char	*name;
	int	direction;
	int	value;
	int	required;
	bool	exprt;
};

#define SKIP	0
#define INPUT	1
#define OUTPUT	2

#define DECLARE_GPIO(_id, _dir, _val, _req, _expt)	\
{ \
	.idx		= _id, \
	.name		= #_id, \
	.direction	= _dir, \
	.value		= _val, \
	.required	= _req, \
	.exprt		= _expt, \
}

struct emu_det_gpio_init_data emu_det_gpio_data[] = {
	DECLARE_GPIO(EMU_SCI_OUT_GPIO,      INPUT, -1, REQUIRED, true),
	DECLARE_GPIO(EMU_ID_EN_GPIO,        INPUT, -1, OPTIONAL, false),
	DECLARE_GPIO(EMU_MUX_CTRL0_GPIO,    OUTPUT, 1, REQUIRED, false),
	DECLARE_GPIO(EMU_MUX_CTRL1_GPIO,    OUTPUT, 0, REQUIRED, false),
	DECLARE_GPIO(SEMU_PPD_DET_GPIO,     INPUT, -1, REQUIRED, true),
	DECLARE_GPIO(SEMU_ALT_MODE_EN_GPIO, SKIP,  -1, REQUIRED, true),
	DECLARE_GPIO(EMU_ID_GPIO,           SKIP,  -1, OPTIONAL, false),
	DECLARE_GPIO(DMB_PPD_DET_GPIO,      INPUT, -1, OPTIONAL, true),
	DECLARE_GPIO(DPLUS_GPIO,            SKIP,  -1, REQUIRED, false),
	DECLARE_GPIO(DMINUS_GPIO,           SKIP,  -1, REQUIRED, false),
	DECLARE_GPIO(WHISPER_UART_TX_GPIO,  OUTPUT, 1, REQUIRED, true),
	DECLARE_GPIO(WHISPER_UART_RX_GPIO,  INPUT, -1, REQUIRED, true),
	DECLARE_GPIO(TX_PAIR_GPIO,          SKIP,  -1, REQUIRED, false),
	DECLARE_GPIO(RX_PAIR_GPIO,          SKIP,  -1, REQUIRED, false),
};

static void free_gpios(void)
{
	struct emu_det_data *emud = the_emud;
	int i;

	for (i = 0; i < EMU_DET_GPIO_MAX; i++)
		if (emud->emu_gpio[i] > 0) {
			gpio_free(emud->emu_gpio[i]);
			pr_emu_det(DEBUG, "free-ed gpio %d\n",
						emud->emu_gpio[i]);
			emud->emu_gpio[i] = 0;
		}
}

static int __devinit request_gpios(struct platform_device *pdev)
{
	struct emu_det_data *emud = the_emud;
	struct resource *res;
	int ret, i;

	for (i = 0; i < ARRAY_SIZE(emu_det_gpio_data); i++) {
		res = platform_get_resource_byname(pdev, IORESOURCE_IO,
				emu_det_gpio_data[i].name);
		if (res == NULL) {
			if (emu_det_gpio_data[i].required) {
				pr_err("couldn't find REQUIRED %s\n",
				       emu_det_gpio_data[i].name);
				goto err_out;
			} else {
				pr_emu_det(DEBUG, "couldn't find OPTIONAL %s\n",
				       emu_det_gpio_data[i].name);
				emud->emu_gpio[emu_det_gpio_data[i].idx] = -1;
				continue;
			}
		}

		emud->emu_gpio[emu_det_gpio_data[i].idx] = res->start;

		pr_emu_det(DEBUG, "requesting gpio[%d] %s-%d\n",
				emu_det_gpio_data[i].idx,
				emu_det_gpio_data[i].name,
				emud->emu_gpio[emu_det_gpio_data[i].idx]);

		ret = gpio_request(emud->emu_gpio[emu_det_gpio_data[i].idx],
				emu_det_gpio_data[i].name);
		if (ret) {
			pr_err("failed to request gpio %s\n",
				emu_det_gpio_data[i].name);
			goto err_out;
		}

		if (emu_det_gpio_data[i].direction == OUTPUT) {
			ret = gpio_direction_output(
				emud->emu_gpio[emu_det_gpio_data[i].idx],
				emu_det_gpio_data[i].value);
			pr_emu_det(DEBUG, "gpio %d is OUT\n",
				emud->emu_gpio[emu_det_gpio_data[i].idx]);
		} else if (emu_det_gpio_data[i].direction == INPUT) {
			ret = gpio_direction_input(
				emud->emu_gpio[emu_det_gpio_data[i].idx]);
			pr_emu_det(DEBUG, "gpio %d is IN\n",
				emud->emu_gpio[emu_det_gpio_data[i].idx]);
		}

		if (emu_det_gpio_data[i].exprt == true) {
			gpio_export(res->start, true);
			gpio_export_link(emud->dev, emu_det_gpio_data[i].name,
					 res->start);
			pr_emu_det(DEBUG, "exporting gpio %s-%d\n",
				   emu_det_gpio_data[i].name, res->start);
		}
	}
	return 0;

err_out:
	free_gpios();
	return -EINVAL;
}

int emu_det_get_accy(void)
{
	struct emu_det_data *emud = the_emud;

	if (emud)
		return (int)emud->accy;
	else
		return EMU_ACCY_UNKNOWN;
}
EXPORT_SYMBOL_GPL(emu_det_get_accy);

#ifdef CONFIG_DEBUG_FS
static int accy_read(void *data, u64 *val)
{
	struct emu_det_data *emud = the_emud;
	*val = emud->accy;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(accy_debugfs_fops, accy_read, NULL, "%lld\n");

static int emu_id_adc_read(void *data, u64 *val)
{
	*val = adc_emu_id_get();
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(emu_id_debugfs_fops, emu_id_adc_read, NULL, "%lld\n");

static int vbus_adc_read(void *data, u64 *val)
{
	*val = adc_vbus_get();
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(vbus_debugfs_fops, vbus_adc_read, NULL, "%lld\n");

static int detection_set(void *data, u64 val)
{
	struct emu_det_data *emud = the_emud;
	queue_work(emud->wq, &(emud->work.work));
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(detection_debugfs_fops, NULL, detection_set, "%lld\n");

static int semu_ppd_det_get(void *data, u64 *val)
{
	struct emu_det_data *emud = the_emud;
	*val = gpio_get_value_cansleep(emud->emu_gpio[SEMU_PPD_DET_GPIO]);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(semu_ppd_det_debugfs_fops, semu_ppd_det_get, \
						NULL, "%lld\n");

static int emu_id_en_get(void *data, u64 *val)
{
	struct emu_det_data *emud = the_emud;
	*val = gpio_get_value_cansleep(emud->emu_gpio[EMU_ID_EN_GPIO]);
	return 0;
}

static int emu_id_en_set(void *data, u64 val)
{
	val = !!val;
	if (val)
		emu_id_protection_on();
	else
		emu_id_protection_off();
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(emu_id_en_debugfs_fops, emu_id_en_get, \
						emu_id_en_set, "%lld\n");

static void create_debugfs_entries(void)
{
	struct dentry *dent = debugfs_create_dir("emu_det", NULL);

	if (IS_ERR(dent)) {
		pr_err("EMU det debugfs dir not created\n");
		return;
	}

	debugfs_create_file("vbus", 0444, dent,
			    (void *)NULL, &vbus_debugfs_fops);
	debugfs_create_file("detection", 0222, dent,
			    (void *)NULL, &detection_debugfs_fops);
	debugfs_create_file("accy", 0444, dent,
			    (void *)NULL, &accy_debugfs_fops);
	debugfs_create_file("semu_ppd_det", 0444, dent,
			    (void *)NULL, &semu_ppd_det_debugfs_fops);

	if (!hasHoneyBadger()) {
		debugfs_create_file("emu_id", 0444, dent,
				    (void *)NULL, &emu_id_debugfs_fops);
		debugfs_create_file("emu_id_en", 0644, dent,
				    (void *)NULL, &emu_id_en_debugfs_fops);
	}
}
#else
static inline void create_debugfs_entries(void)
{
}
#endif

static ssize_t reverse_mode_voltage_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct emu_det_data *emud = the_emud;
	unsigned long reverse_mode = 0;
	int ret;

	if (emud->driver_mode != MODE_NORMAL) {
		ret = kstrtoul(buf, 0, &reverse_mode);
		if (ret) {
			pr_err("%s:  kstrtoul failed ret[%x]\n", __func__, ret);
			return ret;
		}

		pr_emu_det(DEBUG, "new reverse_mode is = %ld\n",
					reverse_mode);
		if (reverse_mode == 1 &&
				 !in_range(adc_vbus_get(), REF_VBUS_PRESENT))
			external_5V_enable();
		else if (reverse_mode == 0)
			external_5V_disable();
		else {
			pr_err("Invalid reverse mode value = %ld\n",
					reverse_mode);
			return -EINVAL;
		}
		emud->reverse_mode_enable = (int) reverse_mode;
	}
	return strnlen(buf, count);
}

static ssize_t reverse_mode_voltage_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_emu_det(DEBUG, "reverse_mode_enable = %d\n",
				the_emud->reverse_mode_enable);
	return snprintf(buf, PAGE_SIZE, "%d\n",
					the_emud->reverse_mode_enable);
}
static DEVICE_ATTR(reverse_mode_enable, 0664,
	reverse_mode_voltage_show, reverse_mode_voltage_store);

static ssize_t debug_mask_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;

	ret = kstrtoul(buf, 0, &debug_mask);
	if (ret) {
		pr_err("%s: kstrtoul Failed. ret[%x]\n", __func__, ret);
		return ret;
	}

	pr_emu_det(DEBUG, "new debug_mask=%ld\n", debug_mask);
	return strnlen(buf, count);
}

static ssize_t debug_mask_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_emu_det(DEBUG, "debug_mask=%ld\n", debug_mask);
	return snprintf(buf, PAGE_SIZE, "%ld\n", debug_mask);
}
static DEVICE_ATTR(debug_mask, 0644, debug_mask_show, debug_mask_store);

static ssize_t mode_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_emu_det(DEBUG, "mode=%d\n", the_emud->driver_mode);
	return snprintf(buf, PAGE_SIZE, "%d\n", the_emud->driver_mode);
}

static ssize_t mode_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct emu_det_data *emud = the_emud;
	int ret;
	unsigned long value;

	ret = kstrtoul(buf, 0, &value);
	if (ret) {
		pr_err("%s: kstrtoul Failed. ret[%x]\n", __func__, ret);
		return ret;
	}

	switch (value) {
	case MODE_F_USB:
		pr_emu_det(TRANSITION, "FACTORY (MUX-USB) mode\n");
		emud->driver_mode = MODE_F_USB;
		whisper_gpio_mode(GPIO_MODE_ALTERNATE);
		alternate_mode_enable();
		gpio_direction_output(emud->emu_gpio[SEMU_ALT_MODE_EN_GPIO], 0);
		mux_ctrl_mode(MUXMODE_USB);
		break;
	case MODE_F_UART:
		pr_emu_det(TRANSITION, "FACTORY (MUX-UART) mode\n");
		emud->driver_mode = MODE_F_UART;
		whisper_gpio_mode(GPIO_MODE_ALTERNATE);
		gpio_direction_output(emud->emu_gpio[WHISPER_UART_TX_GPIO], 0);
		gpio_direction_input(emud->emu_gpio[WHISPER_UART_RX_GPIO]);
		mux_ctrl_mode(MUXMODE_UART);
		break;
	case MODE_F_AUDIO:
		pr_emu_det(TRANSITION, "FACTORY (MUX-AUDIO) mode\n");
		emud->driver_mode = MODE_F_AUDIO;
		whisper_gpio_mode(GPIO_MODE_ALTERNATE);
		alternate_mode_enable();
		blocking_notifier_call_chain(&semu_audio_notifier_list,
				     (unsigned long)EMU_OUT,
				     (void *)NULL);
		break;
	case MODE_NORMAL:
		pr_emu_det(TRANSITION, "NORMAL (MUX-AUTO) mode\n");
		emud->driver_mode = MODE_NORMAL;
		whisper_gpio_mode(GPIO_MODE_STANDARD);
		standard_mode_enable();
		mux_ctrl_mode(MUXMODE_USB);
		detection_work();
		break;
	default:
		pr_emu_det(ERROR, "unsupported mode\n");
		break;
	}
	return strnlen(buf, count);
}
static DEVICE_ATTR(mode, 0664, mode_show, mode_store);

static struct attribute *emu_dev_attrs[] = {
	&dev_attr_debug_mask.attr,
	&dev_attr_mode.attr,
	&dev_attr_reverse_mode_enable.attr,
	NULL,
};

static struct attribute_group emu_dev_attr_groups = {
	.attrs = emu_dev_attrs,
};

static void work_runner(struct work_struct *work)
{
	detection_work();
}

int emu_det_vlcl_request(struct emu_det_vlcl_request *req)
{
	struct emu_det_data *emud = the_emud;

	if (!emud || (emud->charger_vlcl[0] == 0) || !req)
		return -EINVAL;

	if (emud->vlcl_req)
		return -EBUSY;

	emud->vlcl_req = req;
	snprintf(emud->charger_msg, sizeof(emud->charger_msg),
		 "%04x%04x", req->mV, req->mA);

	pr_emu_det(DEBUG, "Requesting %dmA at %dmV\n", req->mA, req->mV);
	switch_set_state(&emud->csdev, CHARGE_VLCL_REQ);

	return 0;
}

static long emu_det_ioctl_charger(struct cpcap_charger_request *req)
{
	struct emu_det_data *emud = the_emud;
	int ret = 0;
	struct emu_det_vlcl_request *vlcl_req = emud->vlcl_req;

	switch (req->cmd) {
	case CMD_CHARGER_CAPABILITY:
		strlcpy(emud->charger_vlcl, req->charger_capability,
			sizeof(emud->charger_vlcl));
		pr_emu_det(DEBUG, "charger vlcl = %s\n", emud->charger_vlcl);

		blocking_notifier_call_chain(&charger_vlcl_notifier_list, 0,
					     emud->charger_vlcl);
		break;

	case CMD_CHARGER_VLCL_RESP:
		/* Cleanup before calling callback */
		switch_set_state(&emud->csdev, CHARGE_NONE);
		emud->vlcl_req = NULL;

		if (strncmp(req->charger_msg, emud->charger_msg,
			    sizeof(emud->charger_msg)) == 0) {
			pr_emu_det(DEBUG, "Switch voltage/current succeeded\n");
			vlcl_req->status = 0;
		} else {
			pr_emu_det(DEBUG, "Switch voltage/current failed\n");
			vlcl_req->status = -EIO;
		}

		if (vlcl_req->callback)
			vlcl_req->callback(vlcl_req->callback_param);
		break;

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static long emu_det_ioctl(struct file *file,
				unsigned int cmd, unsigned long arg)
{
	struct emu_det_data *data = the_emud;
	int retval = -EINVAL;
	int dock = NO_DOCK;
	struct cpcap_whisper_request request;
	struct cpcap_charger_request chrg_req;

	switch (cmd) {
	case CPCAP_IOCTL_ACCY_WHISPER:
		if (copy_from_user((void *) &request, (void *) arg,
				   sizeof(request)))
			return -EFAULT;
		request.dock_id[CPCAP_WHISPER_ID_SIZE - 1] = '\0';
		request.dock_prop[CPCAP_WHISPER_PROP_SIZE - 1] = '\0';

		pr_emu_det(STATUS, "ioctl cmd = 0x%04x\n", request.cmd);

		if (data->state == CHARGER) {
			if (request.cmd & CPCAP_WHISPER_ENABLE_UART) {
				if (emu_pdata->cfg_l2_err)
					emu_pdata->cfg_l2_err(0);
				data->whisper_auth = AUTH_IN_PROGRESS;
				mux_ctrl_mode(MUXMODE_UART);
				if (data->accy == ACCY_WHISPER_PPD) {
					emu_det_disable_irq(EMU_SCI_OUT_IRQ);
					emu_det_disable_irq(SEMU_PPD_DET_IRQ);
					pm8921_charger_unregister_vbus_sn(0);
					external_5V_enable();
				}
			}

			if (request.cmd & CPCAP_WHISPER_MODE_PU)
				data->whisper_auth = AUTH_PASSED;
			else if (!(request.cmd & CPCAP_WHISPER_ENABLE_UART))
				data->whisper_auth = AUTH_FAILED;

			/* Report dock type to system */
			dock = (request.cmd & CPCAP_WHISPER_ACCY_MASK) >>
				CPCAP_WHISPER_ACCY_SHFT;
			switch_set_state(&data->dsdev, dock);
			switch_set_state(&data->edsdev, dock);

			pr_emu_det(STATUS, "Whisper_auth =%d\n",
							data->whisper_auth);
			if (!(request.cmd & CPCAP_WHISPER_ENABLE_UART)) {
				if (data->accy == ACCY_WHISPER_PPD) {
					external_5V_disable();
					emu_det_enable_irq(EMU_SCI_OUT_IRQ);
					emu_det_enable_irq(SEMU_PPD_DET_IRQ);
					pm8921_charger_register_vbus_sn
						(&emu_det_vbus_state);
				}

				if (emu_pdata->cfg_l2_err)
					emu_pdata->cfg_l2_err(1);
				if (dock != CHARGER_DOCK)
					mux_ctrl_mode(MUXMODE_USB);
				if (dock && (strlen(request.dock_id) <
					CPCAP_WHISPER_ID_SIZE))
					strlcpy(data->dock_id,
						request.dock_id,
						CPCAP_WHISPER_ID_SIZE);
				if (dock && (strlen(request.dock_prop) <
					CPCAP_WHISPER_PROP_SIZE))
					strlcpy(data->dock_prop,
						request.dock_prop,
						CPCAP_WHISPER_PROP_SIZE);
				set_whisper_ioctl_context_flag();
				queue_delayed_work(data->wq, &data->work, 0);
			}
			retval = 0;

		} else if ((data->state == WHISPER_SMART) ||
			(data->state == WHISPER_SMART_LD2_OPEN)) {
			/* Report dock type to system */
			dock = (request.cmd & CPCAP_WHISPER_ACCY_MASK) >>
						CPCAP_WHISPER_ACCY_SHFT;
			if (dock &&
			   (strlen(request.dock_id) < CPCAP_WHISPER_ID_SIZE))
				strlcpy(data->dock_id, request.dock_id,
						CPCAP_WHISPER_ID_SIZE);
			if (dock &&
			   (strlen(request.dock_prop) <
				CPCAP_WHISPER_PROP_SIZE))
				strlcpy(data->dock_prop, request.dock_prop,
						CPCAP_WHISPER_PROP_SIZE);
			switch (dock) {
			case HE_DOCK:
				switch_set_state(&data->dsdev, HE_DOCK);
				break;
			case MOBILE_DOCK:
			default:
				switch_set_state(&data->dsdev, NO_DOCK);
				break;
			}
			switch_set_state(&data->edsdev, dock);

			if (request.cmd & 0x01)
				data->whisper_auth = AUTH_PASSED;
			else
				data->whisper_auth = AUTH_FAILED;

			pr_emu_det(STATUS, "Whisper_auth =%d\n",
							data->whisper_auth);
			retval = 0;
		}
		break;

	case CPCAP_IOCTL_ACCY_CHARGER:
		if (copy_from_user((void *) &chrg_req, (void *) arg,
				   sizeof(chrg_req)))
			return -EFAULT;
		retval = emu_det_ioctl_charger(&chrg_req);
		break;

	default:
		retval = -ENOTTY;
		break;
	}

	return retval;
}

static const struct file_operations emu_det_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = emu_det_ioctl,
};

static struct miscdevice emu_det_dev = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= CPCAP_DEV_NAME,
	.fops	= &emu_det_fops,
};

static int emu_det_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct emu_det_data *data;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	platform_set_drvdata(pdev, data);
	the_emud = data;
	emu_pdata = pdev->dev.platform_data;

	if (!emu_pdata) {
		pr_err("missing platform data\n");
		ret = -ENODEV;
		goto free_data;
	}

	if (emu_pdata->accy_support == ACCY_SUPPORT_BASIC)
		emu_det_irq_data[SCI_OUT_IDX].required = OPTIONAL;
	data->dev = &pdev->dev;

	if (emu_pdata->core_power)
		emu_pdata->core_power(1);

	ret = request_gpios(pdev);
	if (ret) {
		pr_err("couldn't register gpios rc=%d\n", ret);
		goto free_data;
	}

	data->protection_forced_off = true;
	emu_id_protection_on();

	pr_emu_det(DEBUG, "done with gpios\n");

	ret = misc_register(&emu_det_dev);
	if (ret) {
		pr_err("%s: couldn't register device %s ret=%d]\n",
			__func__, emu_det_dev.name, ret);
		goto free_gpios;
	}

	INIT_DELAYED_WORK(&data->work, work_runner);

	wake_lock_init(&data->wake_lock, WAKE_LOCK_SUSPEND, "emu_det");

	data->wq = create_singlethread_workqueue("emu_det");

	data->state = CONFIG;
	atomic_set(&data->semu_alt_mode, 0);
	data->accy = ACCY_NONE;
	data->whisper_auth = AUTH_NOT_STARTED;
	data->requested_muxmode = MUXMODE_UNDEFINED;
	data->vbus_adc_delay = true;
	data->suspend_disabled = false;
	data->driver_mode = MODE_NORMAL;
	data->whisper_ioctl_context = false;
	data->is_chg_plugged_in = false;
	dev_set_drvdata(&pdev->dev, data);

	data->trans = usb_get_transceiver();
	if (!data->trans || !data->trans->init) {
		pr_err("failed to obtain OTG transiver\n");
		ret = -ENODEV;
		goto free_works;
	} else
		data->trans->init(data->trans);

	ret = request_irqs(pdev);
	if (ret) {
		pr_err("couldn't register interrupts rc=%d\n", ret);
		goto free_gpios;
	}

	emu_det_enable_irq(SEMU_PPD_DET_IRQ);
	pr_emu_det(DEBUG, "done with irqs\n");
	data->irq_setup = true;

	pm8921_charger_register_vbus_sn(&emu_det_vbus_state);
	pr_emu_det(DEBUG, "registered callback with PM8921\n");

	data->wsdev.name = "whisper";
	ret = switch_dev_register(&data->wsdev);
	if (ret)
		pr_err("couldn't register switch (%s) rc=%d\n",
					data->wsdev.name, ret);
	data->dsdev.name = "dock";
	data->dsdev.print_name = dock_print_name;
	ret = switch_dev_register(&data->dsdev);
	if (ret)
		pr_err("couldn't register switch (%s) rc=%d\n",
					data->dsdev.name, ret);
	data->asdev.name = "semu_audio";
	data->asdev.print_name = emu_audio_print_name;
	ret = switch_dev_register(&data->asdev);
	if (ret)
		pr_err("couldn't register switch (%s) rc=%d\n",
					data->asdev.name, ret);
	data->edsdev.name = "extdock";
	data->edsdev.print_name = dock_print_name;
	ret = switch_dev_register(&data->edsdev);
	if (ret)
		pr_err("couldn't register switch (%s) rc=%d\n",
					data->edsdev.name, ret);
	data->sdsdev.name = "smartdock";
	ret = switch_dev_register(&data->sdsdev);
	if (ret)
		pr_err("couldn't register switch (%s) rc=%d\n",
					data->sdsdev.name, ret);
	data->csdev.name = "charge_capability";
	ret = switch_dev_register(&data->csdev);
	if (ret)
		pr_err("couldn't register switch (%s) rc=%d\n",
					data->csdev.name, ret);
	data->noauthdev.name = "noauth";
	ret = switch_dev_register(&data->noauthdev);
	if (ret)
		pr_err("couldn't register switch (%s) rc=%d\n",
					data->noauthdev.name, ret);

	ret = device_create_file(data->csdev.dev, &dev_attr_charger_msg);
	if (ret)
		pr_err("Failed to create charger_msg file\n");

	ret = sysfs_create_group(&(pdev->dev.kobj), &emu_dev_attr_groups);
	if (ret)
		pr_err("failed to create sysfs group: %d\n", ret);

	create_debugfs_entries();
	pr_emu_det(DEBUG, "EMU detection driver started\n");

	/*
	 * PMIC always kicks off detection work regardless whether
	 * anything is connected to USB or not, thus no need to schedule
	 */

	return 0;

free_works:
	cancel_delayed_work_sync(&data->work);
	destroy_workqueue(data->wq);
/*free_misc_dev:*/
	misc_deregister(&emu_det_dev);
free_gpios:
	free_gpios();
	wake_lock_destroy(&data->wake_lock);
free_data:
	kfree(data);

	return ret;
}

static int __exit emu_det_remove(struct platform_device *pdev)
{
	struct emu_det_data *data = the_emud;

	wake_lock_destroy(&data->wake_lock);
	configure_hardware(ACCY_NONE);
	cancel_delayed_work_sync(&data->work);
	destroy_workqueue(data->wq);
	sysfs_remove_group(&(pdev->dev.kobj), &emu_dev_attr_groups);
	switch_dev_unregister(&data->wsdev);
	switch_dev_unregister(&data->dsdev);
	switch_dev_unregister(&data->asdev);
	switch_dev_unregister(&data->edsdev);
	switch_dev_unregister(&data->sdsdev);
	switch_dev_unregister(&data->csdev);
	switch_dev_unregister(&data->noauthdev);

	pm8921_charger_unregister_vbus_sn(0);
	free_irqs();
	free_gpios();

	misc_deregister(&emu_det_dev);
	if (emu_pdata->core_power)
		emu_pdata->core_power(0);
	kfree(data);

	return 0;
}

static struct platform_driver emu_det_driver = {
	.probe		= emu_det_probe,
	.remove		= __exit_p(emu_det_remove),
	.suspend	= NULL,
	.driver		= {
		.name	= "emu_det",
		.owner	= THIS_MODULE,
	},
};

static int __init emu_det_init(void)
{
	return  platform_driver_register(&emu_det_driver);
}

static void __exit emu_det_exit(void)
{
	platform_driver_unregister(&emu_det_driver);
}

late_initcall(emu_det_init);
module_exit(emu_det_exit);

MODULE_ALIAS("platform:emu_det");
MODULE_DESCRIPTION("Motorola MSM8960 EMU detection driver");
MODULE_LICENSE("GPL");
