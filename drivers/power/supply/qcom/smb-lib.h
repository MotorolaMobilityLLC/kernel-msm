/* Copyright (c) 2016-2017 The Linux Foundation. All rights reserved.
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

#ifndef __SMB2_CHARGER_H
#define __SMB2_CHARGER_H
#include <linux/gpio.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/irqreturn.h>
#include <linux/reboot.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/consumer.h>
#include <linux/extcon.h>
#include "storm-watch.h"
#include <linux/alarmtimer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/power_supply.h>
#include <linux/usb/usbpd.h>

enum print_reason {
	PR_INTERRUPT	= BIT(0),
	PR_REGISTER	= BIT(1),
	PR_MISC		= BIT(2),
	PR_PARALLEL	= BIT(3),
	PR_OTG		= BIT(4),
	PR_MOTO		= BIT(7),
};

#define DEFAULT_VOTER			"DEFAULT_VOTER"
#define USER_VOTER			"USER_VOTER"
#define PD_VOTER			"PD_VOTER"
#define DCP_VOTER			"DCP_VOTER"
#define QC_VOTER			"QC_VOTER"
#define PL_USBIN_USBIN_VOTER		"PL_USBIN_USBIN_VOTER"
#define USB_PSY_VOTER			"USB_PSY_VOTER"
#define PL_TAPER_WORK_RUNNING_VOTER	"PL_TAPER_WORK_RUNNING_VOTER"
#define PL_QNOVO_VOTER			"PL_QNOVO_VOTER"
#define USBIN_V_VOTER			"USBIN_V_VOTER"
#define CHG_STATE_VOTER			"CHG_STATE_VOTER"
#define TYPEC_SRC_VOTER			"TYPEC_SRC_VOTER"
#define TAPER_END_VOTER			"TAPER_END_VOTER"
#define THERMAL_DAEMON_VOTER		"THERMAL_DAEMON_VOTER"
#define CC_DETACHED_VOTER		"CC_DETACHED_VOTER"
#define HVDCP_TIMEOUT_VOTER		"HVDCP_TIMEOUT_VOTER"
#define PD_DISALLOWED_INDIRECT_VOTER	"PD_DISALLOWED_INDIRECT_VOTER"
#define PD_HARD_RESET_VOTER		"PD_HARD_RESET_VOTER"
#define VBUS_CC_SHORT_VOTER		"VBUS_CC_SHORT_VOTER"
#define PD_INACTIVE_VOTER		"PD_INACTIVE_VOTER"
#define BOOST_BACK_VOTER		"BOOST_BACK_VOTER"
#define USBIN_USBIN_BOOST_VOTER		"USBIN_USBIN_BOOST_VOTER"
#define HVDCP_INDIRECT_VOTER		"HVDCP_INDIRECT_VOTER"
#define MICRO_USB_VOTER			"MICRO_USB_VOTER"
#define DEBUG_BOARD_VOTER		"DEBUG_BOARD_VOTER"
#define PD_SUSPEND_SUPPORTED_VOTER	"PD_SUSPEND_SUPPORTED_VOTER"
#define PL_DELAY_VOTER			"PL_DELAY_VOTER"
#define CTM_VOTER			"CTM_VOTER"
#define SW_QC3_VOTER			"SW_QC3_VOTER"
#define AICL_RERUN_VOTER		"AICL_RERUN_VOTER"
#define LEGACY_UNKNOWN_VOTER		"LEGACY_UNKNOWN_VOTER"
#define ICL_LIMIT_VOTER		"ICL_LIMIT_VOTER"
#define CC2_WA_VOTER			"CC2_WA_VOTER"
#define QNOVO_VOTER			"QNOVO_VOTER"
#define BATT_PROFILE_VOTER		"BATT_PROFILE_VOTER"
#define OTG_DELAY_VOTER			"OTG_DELAY_VOTER"
#define USBIN_I_VOTER			"USBIN_I_VOTER"
#define WEAK_CHARGER_VOTER		"WEAK_CHARGER_VOTER"
#define WBC_VOTER			"WBC_VOTER"
#define OV_VOTER			"OV_VOTER"
#define FCC_STEPPER_VOTER		"FCC_STEPPER_VOTER"

#define VCONN_MAX_ATTEMPTS	3
#define OTG_MAX_ATTEMPTS	3
#define BOOST_BACK_STORM_COUNT	3
#define WEAK_CHG_STORM_COUNT	8
#define HEARTBEAT_VOTER			"HEARTBEAT_VOTER"
#define EB_VOTER			"EB_VOTER"
#define WIRELESS_VOTER			"WIRELESS_VOTER"
#define DEMO_VOTER			"DEMO_VOTER"
#define OTG_VOTER			"OTG_VOTER"

enum smb_mode {
	PARALLEL_MASTER = 0,
	PARALLEL_SLAVE,
	NUM_MODES,
};

enum {
	QC_CHARGER_DETECTION_WA_BIT	= BIT(0),
	BOOST_BACK_WA			= BIT(1),
	TYPEC_CC2_REMOVAL_WA_BIT	= BIT(2),
	QC_AUTH_INTERRUPT_WA_BIT	= BIT(3),
	OTG_WA				= BIT(4),
	OV_IRQ_WA_BIT			= BIT(5),
	TYPEC_PBS_WA_BIT		= BIT(6),
};

enum smb_irq_index {
	CHG_ERROR_IRQ = 0,
	CHG_STATE_CHANGE_IRQ,
	STEP_CHG_STATE_CHANGE_IRQ,
	STEP_CHG_SOC_UPDATE_FAIL_IRQ,
	STEP_CHG_SOC_UPDATE_REQ_IRQ,
	OTG_FAIL_IRQ,
	OTG_OVERCURRENT_IRQ,
	OTG_OC_DIS_SW_STS_IRQ,
	TESTMODE_CHANGE_DET_IRQ,
	BATT_TEMP_IRQ,
	BATT_OCP_IRQ,
	BATT_OV_IRQ,
	BATT_LOW_IRQ,
	BATT_THERM_ID_MISS_IRQ,
	BATT_TERM_MISS_IRQ,
	USBIN_COLLAPSE_IRQ,
	USBIN_LT_3P6V_IRQ,
	USBIN_UV_IRQ,
	USBIN_OV_IRQ,
	USBIN_PLUGIN_IRQ,
	USBIN_SRC_CHANGE_IRQ,
	USBIN_ICL_CHANGE_IRQ,
	TYPE_C_CHANGE_IRQ,
	DCIN_COLLAPSE_IRQ,
	DCIN_LT_3P6V_IRQ,
	DCIN_UV_IRQ,
	DCIN_OV_IRQ,
	DCIN_PLUGIN_IRQ,
	DIV2_EN_DG_IRQ,
	DCIN_ICL_CHANGE_IRQ,
	WDOG_SNARL_IRQ,
	WDOG_BARK_IRQ,
	AICL_FAIL_IRQ,
	AICL_DONE_IRQ,
	HIGH_DUTY_CYCLE_IRQ,
	INPUT_CURRENT_LIMIT_IRQ,
	TEMPERATURE_CHANGE_IRQ,
	SWITCH_POWER_OK_IRQ,
	SMB_IRQ_MAX,
};

enum try_sink_exit_mode {
	ATTACHED_SRC = 0,
	ATTACHED_SINK,
	UNATTACHED_SINK,
};

struct smb_irq_info {
	const char			*name;
	const irq_handler_t		handler;
	const bool			wake;
	const struct storm_watch	storm_data;
	struct smb_irq_data		*irq_data;
	int				irq;
};

static const unsigned int smblib_extcon_cable[] = {
	EXTCON_USB,
	EXTCON_USB_HOST,
	EXTCON_USB_CC,
	EXTCON_USB_SPEED,
	EXTCON_NONE,
};

/* EXTCON_USB and EXTCON_USB_HOST are mutually exclusive */
static const u32 smblib_extcon_exclusive[] = {0x3, 0};

struct smb_regulator {
	struct regulator_dev	*rdev;
	struct regulator_desc	rdesc;
};

struct smb_irq_data {
	void			*parent_data;
	const char		*name;
	struct storm_watch	storm_data;
};

struct smb_chg_param {
	const char	*name;
	u16		reg;
	int		min_u;
	int		max_u;
	int		step_u;
	int		(*get_proc)(struct smb_chg_param *param,
				    u8 val_raw);
	int		(*set_proc)(struct smb_chg_param *param,
				    int val_u,
				    u8 *val_raw);
};

struct smb_chg_freq {
	unsigned int		freq_5V;
	unsigned int		freq_6V_8V;
	unsigned int		freq_9V;
	unsigned int		freq_12V;
	unsigned int		freq_removal;
	unsigned int		freq_below_otg_threshold;
	unsigned int		freq_above_otg_threshold;
};

struct smb_params {
	struct smb_chg_param	fcc;
	struct smb_chg_param	fv;
	struct smb_chg_param	usb_icl;
	struct smb_chg_param	icl_stat;
	struct smb_chg_param	otg_cl;
	struct smb_chg_param	dc_icl;
	struct smb_chg_param	dc_icl_pt_lv;
	struct smb_chg_param	dc_icl_pt_hv;
	struct smb_chg_param	dc_icl_div2_lv;
	struct smb_chg_param	dc_icl_div2_mid_lv;
	struct smb_chg_param	dc_icl_div2_mid_hv;
	struct smb_chg_param	dc_icl_div2_hv;
	struct smb_chg_param	jeita_cc_comp;
	struct smb_chg_param	freq_buck;
	struct smb_chg_param	freq_boost;
};

struct parallel_params {
	struct power_supply	*psy;
};

struct smb_iio {
	struct iio_channel	*temp_chan;
	struct iio_channel	*temp_max_chan;
	struct iio_channel	*usbin_i_chan;
	struct iio_channel	*usbin_v_chan;
	struct iio_channel	*batt_i_chan;
	struct iio_channel	*connector_temp_chan;
	struct iio_channel	*connector_temp_thr1_chan;
	struct iio_channel	*connector_temp_thr2_chan;
	struct iio_channel	*connector_temp_thr3_chan;
	struct iio_channel	*dcin_v_chan;
};

struct reg_info {
	u16		reg;
	u8		mask;
	u8		val;
	u8		bak;
	const char	*desc;
};

struct mmi_temp_zone {
	int		temp_c;
	int		norm_mv;
	int		fcc_max_ma;
	int		fcc_norm_ma;
};

#define MAX_NUM_STEPS 10
enum mmi_temp_zones {
	ZONE_FIRST = 0,
	/* states 0-9 are reserved for zones */
	ZONE_LAST = MAX_NUM_STEPS + ZONE_FIRST - 1,
	ZONE_HOT,
	ZONE_COLD,
	ZONE_NONE = 0xFF,
};

enum mmi_chrg_step {
	STEP_MAX,
	STEP_NORM,
	STEP_EB,
	STEP_FULL,
	STEP_FLOAT,
	STEP_DEMO,
	STEP_STOP,
	STEP_NONE = 0xFF,
};

enum ebchg_state {
	EB_DISCONN = POWER_SUPPLY_EXTERN_STATE_DIS,
	EB_SINK = POWER_SUPPLY_EXTERN_STATE_SINK,
	EB_SRC = POWER_SUPPLY_EXTERN_STATE_SRC,
	EB_OFF = POWER_SUPPLY_EXTERN_STATE_OFF,
};

enum charging_limit_modes {
	CHARGING_LIMIT_OFF,
	CHARGING_LIMIT_RUN,
	CHARGING_LIMIT_UNKNOWN,
};

enum turbo_ebsrc {
	TURBO_EBSRC_UNKNOWN,
	TURBO_EBSRC_NOT_SUPPORTED,
	TURBO_EBSRC_VALID,
};

struct mmi_params {
	bool			factory_mode;
	int			demo_mode;
	struct gpio		warn_gpio;
	struct gpio		togl_rst_gpio;
	struct gpio		ebchg_gpio;
	struct delayed_work	warn_irq_work;
	int			warn_irq;
	struct notifier_block	smb_reboot;
	/* thermal mitigation */
	int			dc_system_temp_level;
	int			dc_thermal_levels;
	int			*dc_thermal_mitigation;
	int			usb_system_temp_level;
	int			usb_thermal_levels;
	int			*usb_thermal_mitigation;
	bool			factory_kill_armed;

	/* Charge Profile */
	int			num_temp_zones;
	struct mmi_temp_zone	*temp_zones;
	enum mmi_temp_zones	pres_temp_zone;
	enum mmi_chrg_step	pres_chrg_step;
	int			chrg_taper_cnt;
	int			dc_ebmax_current_ma;
	int			dc_eff_current_ma;
	/* external battery params */
	const char		*eb_batt_psy_name;
	const char		*eb_pwr_psy_name;
	enum ebchg_state	ebchg_state;
	bool			force_eb_chrg;
	int			update_eb_params;
	int			charger_debounce_cnt;
	int			cl_ebchg;
	int			cl_ebsrc;
	int			vl_ebsrc;
	int			vo_ebsrc;
	int			vi_ebsrc;
	bool			eb_rechrg;
	bool			usbeb_present;
	bool			wls_present;
	int			temp_state;
	int			chrg_iterm;
	atomic_t		hb_ready;
	struct alarm		heartbeat_alarm;
	struct delayed_work	heartbeat_work;
	struct power_supply	*wls_psy;
	struct power_supply	*usbeb_psy;
	struct pinctrl		*smb_pinctrl;
	struct wakeup_source	smblib_mmi_hb_wake_source;
	bool			apsd_done;
	int			charger_rate;
	bool			hvdcp3_con;
	struct notifier_block	mmi_psy_notifier;
	bool			init_done;
	int			vbus_inc_cnt;
	bool			enable_charging_limit;
	bool			is_factory_image;
	enum charging_limit_modes	charging_limit_modes;
	int			upper_limit_capacity;
	int			lower_limit_capacity;
	int			base_fv_mv;
	int			vfloat_comp_mv;
	enum turbo_ebsrc	turbo_pwr_ebsrc;
	bool			check_ebsrc_vl;
	int			batt_health;
	int			max_chrg_temp;
};

struct smb_charger {
	struct device		*dev;
	char			*name;
	struct regmap		*regmap;
	struct smb_irq_info	*irq_info;
	struct smb_params	param;
	struct smb_iio		iio;
	int			*debug_mask;
	int			*try_sink_enabled;
	enum smb_mode		mode;
	struct smb_chg_freq	chg_freq;
	int			smb_version;
	int			otg_delay_ms;
	int			*weak_chg_icl_ua;

	/* locks */
	struct mutex		lock;
	struct mutex		write_lock;
	struct mutex		ps_change_lock;
	struct mutex		otg_oc_lock;
	struct mutex		vconn_oc_lock;

	/* power supplies */
	struct power_supply		*batt_psy;
	struct power_supply		*usb_psy;
	struct power_supply		*dc_psy;
	struct power_supply		*bms_psy;
	struct power_supply_desc	usb_psy_desc;
	struct power_supply		*usb_main_psy;
	struct power_supply		*usb_port_psy;
	enum power_supply_type		real_charger_type;

	/* notifiers */
	struct notifier_block	nb;

	/* parallel charging */
	struct parallel_params	pl;

	/* regulators */
	struct smb_regulator	*vbus_vreg;
	struct smb_regulator	*vconn_vreg;
	struct regulator	*dpdm_reg;
	struct smb_regulator	*ext_vbus_vreg;

	/* votables */
	struct votable		*dc_suspend_votable;
	struct votable		*fcc_votable;
	struct votable		*fv_votable;
	struct votable		*usb_icl_votable;
	struct votable		*dc_icl_votable;
	struct votable		*pd_disallowed_votable_indirect;
	struct votable		*pd_allowed_votable;
	struct votable		*awake_votable;
	struct votable		*pl_disable_votable;
	struct votable		*chg_disable_votable;
	struct votable		*pl_enable_votable_indirect;
	struct votable		*hvdcp_disable_votable_indirect;
	struct votable		*hvdcp_enable_votable;
	struct votable		*apsd_disable_votable;
	struct votable		*hvdcp_hw_inov_dis_votable;
	struct votable		*usb_irq_enable_votable;
	struct votable		*typec_irq_disable_votable;

	/* work */
	struct work_struct	bms_update_work;
	struct work_struct	rdstd_cc2_detach_work;
	struct delayed_work	hvdcp_detect_work;
	struct delayed_work	ps_change_timeout_work;
	struct delayed_work	clear_hdc_work;
	struct work_struct	otg_oc_work;
	struct work_struct	vconn_oc_work;
	struct delayed_work	otg_ss_done_work;
	struct delayed_work	icl_change_work;
	struct delayed_work	pl_enable_work;
	struct work_struct	legacy_detection_work;
	struct delayed_work	uusb_otg_work;
	struct delayed_work	bb_removal_work;

	/* cached status */
	int			voltage_min_uv;
	int			voltage_max_uv;
	int			pd_active;
	bool			system_suspend_supported;
	int			boost_threshold_ua;
	int			system_temp_level;
	int			thermal_levels;
	int			*thermal_mitigation;
	int			dcp_icl_ua;
	int			fake_capacity;
	bool			step_chg_enabled;
	bool			sw_jeita_enabled;
	bool			is_hdc;
	bool			chg_done;
	bool			micro_usb_mode;
	bool			otg_en;
	bool			vconn_en;
	bool			suspend_input_on_debug_batt;
	int			otg_attempts;
	int			vconn_attempts;
	int			default_icl_ua;
	int			otg_cl_ua;
	bool			uusb_apsd_rerun_done;
	bool			pd_hard_reset;
	bool			typec_present;
	u8			typec_status[5];
	bool			typec_legacy_valid;
	int			fake_input_current_limited;
	bool			pr_swap_in_progress;
	int			typec_mode;
	int			usb_icl_change_irq_enabled;
	u32			jeita_status;
	u8			float_cfg;
	bool			use_extcon;
	bool			otg_present;
	bool			fcc_stepper_mode;

	/* workaround flag */
	u32			wa_flags;
	bool			cc2_detach_wa_active;
	bool			typec_en_dis_active;
	bool			try_sink_active;
	int			boost_current_ua;
	int			temp_speed_reading_count;

	/* extcon for VBUS / ID notification to USB for uUSB */
	struct extcon_dev	*extcon;

	/* battery profile */
	int			batt_profile_fcc_ua;
	int			batt_profile_fv_uv;

	/* qnovo */
	int			usb_icl_delta_ua;
	int			pulse_cnt;

	/* mmi based params */
	/* Place at end of struct smb_charger as it grows */
	struct mmi_params	mmi;
	void			*ipc_log;
	void			*ipc_log_reg;
	struct usbpd		*pd;
	int			pd_contract_uv;
	struct delayed_work	pd_contract_work;
	bool			external_vbus;
	bool			reverse_boost;
	bool			hvdcp_force_5v;
	bool			suspended;
};

int smblib_read(struct smb_charger *chg, u16 addr, u8 *val);
int smblib_masked_write(struct smb_charger *chg, u16 addr, u8 mask, u8 val);
int smblib_write(struct smb_charger *chg, u16 addr, u8 val);

int smblib_get_charge_param(struct smb_charger *chg,
			    struct smb_chg_param *param, int *val_u);
int smblib_get_usb_suspend(struct smb_charger *chg, int *suspend);

int smblib_enable_charging(struct smb_charger *chg, bool enable);
int smblib_set_charge_param(struct smb_charger *chg,
			    struct smb_chg_param *param, int val_u);
int smblib_set_usb_suspend(struct smb_charger *chg, bool suspend);
int smblib_set_dc_suspend(struct smb_charger *chg, bool suspend);

int smblib_mapping_soc_from_field_value(struct smb_chg_param *param,
					     int val_u, u8 *val_raw);
int smblib_mapping_cc_delta_to_field_value(struct smb_chg_param *param,
					   u8 val_raw);
int smblib_mapping_cc_delta_from_field_value(struct smb_chg_param *param,
					     int val_u, u8 *val_raw);
int smblib_set_chg_freq(struct smb_chg_param *param,
				int val_u, u8 *val_raw);

int smblib_vbus_regulator_enable(struct regulator_dev *rdev);
int smblib_vbus_regulator_disable(struct regulator_dev *rdev);
int smblib_vbus_regulator_is_enabled(struct regulator_dev *rdev);

int smblib_vconn_regulator_enable(struct regulator_dev *rdev);
int smblib_vconn_regulator_disable(struct regulator_dev *rdev);
int smblib_vconn_regulator_is_enabled(struct regulator_dev *rdev);

int smblib_ext_vbus_regulator_enable(struct regulator_dev *rdev);
int smblib_ext_vbus_regulator_disable(struct regulator_dev *rdev);
int smblib_ext_vbus_regulator_is_enabled(struct regulator_dev *rdev);

irqreturn_t smblib_handle_debug(int irq, void *data);
irqreturn_t smblib_handle_otg_overcurrent(int irq, void *data);
irqreturn_t smblib_handle_chg_state_change(int irq, void *data);
irqreturn_t smblib_handle_batt_temp_changed(int irq, void *data);
irqreturn_t smblib_handle_batt_psy_changed(int irq, void *data);
irqreturn_t smblib_handle_usb_psy_changed(int irq, void *data);
irqreturn_t smblib_handle_usbin_uv(int irq, void *data);
irqreturn_t smblib_handle_usb_plugin(int irq, void *data);
irqreturn_t smblib_handle_usb_source_change(int irq, void *data);
irqreturn_t smblib_handle_icl_change(int irq, void *data);
irqreturn_t smblib_handle_usb_typec_change(int irq, void *data);
irqreturn_t smblib_handle_dc_plugin(int irq, void *data);
irqreturn_t smblib_handle_high_duty_cycle(int irq, void *data);
irqreturn_t smblib_handle_switcher_power_ok(int irq, void *data);
irqreturn_t smblib_handle_wdog_bark(int irq, void *data);

int smblib_get_prop_input_suspend(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_batt_present(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_batt_capacity(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_batt_age(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_batt_status(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_batt_charge_type(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_batt_charge_done(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_batt_health(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_system_temp_level(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_input_current_limited(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_set_prop_input_suspend(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblib_set_prop_batt_capacity(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblib_set_prop_system_temp_level(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblib_set_prop_input_current_limited(struct smb_charger *chg,
				const union power_supply_propval *val);

int smblib_get_prop_dc_present(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_dc_online(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_dc_current_max(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_set_prop_dc_current_max(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblib_get_prop_dc_voltage_now(struct smb_charger *chg,
				union power_supply_propval *val);

int smblib_get_prop_usb_present(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_usb_online(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_usb_suspend(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_usb_voltage_max(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_usb_voltage_now(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_usb_current_now(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_typec_cc_orientation(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_typec_power_role(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_pd_allowed(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_input_current_settled(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_input_voltage_settled(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_pd_in_hard_reset(struct smb_charger *chg,
			       union power_supply_propval *val);
int smblib_get_pe_start(struct smb_charger *chg,
			       union power_supply_propval *val);
int smblib_get_prop_charger_temp(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_charger_temp_max(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_die_health(struct smb_charger *chg,
			       union power_supply_propval *val);
int smblib_get_prop_charge_qnovo_enable(struct smb_charger *chg,
			       union power_supply_propval *val);
int smblib_set_prop_pd_current_max(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblib_set_prop_sdp_current_max(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblib_set_prop_pd_voltage_max(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblib_set_prop_pd_voltage_min(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblib_set_prop_boost_current(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblib_set_prop_typec_power_role(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblib_set_prop_pd_active(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblib_set_prop_pd_in_hard_reset(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblib_get_prop_slave_current_now(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_set_prop_ship_mode(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblib_set_prop_charge_qnovo_enable(struct smb_charger *chg,
				const union power_supply_propval *val);
void smblib_suspend_on_debug_battery(struct smb_charger *chg);
int smblib_rerun_apsd_if_required(struct smb_charger *chg);
int smblib_get_prop_fcc_delta(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_icl_override(struct smb_charger *chg, bool override);
int smblib_dp_dm(struct smb_charger *chg, int val);
int smblib_disable_hw_jeita(struct smb_charger *chg, bool disable);
int smblib_rerun_aicl(struct smb_charger *chg);
int smblib_set_icl_current(struct smb_charger *chg, int icl_ua);
int smblib_get_icl_current(struct smb_charger *chg, int *icl_ua);
int smblib_get_charge_current(struct smb_charger *chg, int *total_current_ua);
int smblib_get_prop_pr_swap_in_progress(struct smb_charger *chg,
				union power_supply_propval *val);
int smblib_get_prop_from_bms(struct smb_charger *chg,
				enum power_supply_property psp,
				union power_supply_propval *val);
int smblib_set_prop_pr_swap_in_progress(struct smb_charger *chg,
				const union power_supply_propval *val);
void smblib_usb_typec_change(struct smb_charger *chg);

int smblib_init(struct smb_charger *chg);
int smblib_deinit(struct smb_charger *chg);

int smblib_get_prop_dc_system_temp_level(struct smb_charger *chg,
					 union power_supply_propval *val);
int smblib_set_prop_dc_system_temp_level(struct smb_charger *chg,
				const union power_supply_propval *val);
int smblib_get_prop_usb_system_temp_level(struct smb_charger *chg,
					  union power_supply_propval *val);
int smblib_set_prop_usb_system_temp_level(struct smb_charger *chg,
				const union power_supply_propval *val);
void mmi_init(struct smb_charger *chg);
void mmi_deinit(struct smb_charger *chg);
void mmi_chrg_rate_check(struct smb_charger *chip);

#define HEARTBEAT_EB_MS 1000
extern struct smb_charger *mmi_chip;
extern int eb_rechrg_start_soc;
extern int eb_rechrg_stop_soc;
extern int eb_attach_start_soc;
extern int eb_attach_stop_soc;
extern int eb_low_start_soc;
extern int eb_low_stop_soc;
extern int eb_on_sw;

#endif /* __SMB2_CHARGER_H */
