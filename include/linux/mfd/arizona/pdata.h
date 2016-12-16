/*
 * Platform data for Arizona devices
 *
 * Copyright 2012 Wolfson Microelectronics. PLC.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>

#ifndef _ARIZONA_PDATA_H
#define _ARIZONA_PDATA_H

#include <dt-bindings/mfd/arizona.h>

#define ARIZONA_GPN_DIR_MASK                     0x8000  /* GPN_DIR */
#define ARIZONA_GPN_DIR_SHIFT                        15  /* GPN_DIR */
#define ARIZONA_GPN_DIR_WIDTH                         1  /* GPN_DIR */
#define ARIZONA_GPN_PU_MASK                      0x4000  /* GPN_PU */
#define ARIZONA_GPN_PU_SHIFT                         14  /* GPN_PU */
#define ARIZONA_GPN_PU_WIDTH                          1  /* GPN_PU */
#define ARIZONA_GPN_PD_MASK                      0x2000  /* GPN_PD */
#define ARIZONA_GPN_PD_SHIFT                         13  /* GPN_PD */
#define ARIZONA_GPN_PD_WIDTH                          1  /* GPN_PD */
#define ARIZONA_GPN_LVL_MASK                     0x0800  /* GPN_LVL */
#define ARIZONA_GPN_LVL_SHIFT                        11  /* GPN_LVL */
#define ARIZONA_GPN_LVL_WIDTH                         1  /* GPN_LVL */
#define ARIZONA_GPN_POL_MASK                     0x0400  /* GPN_POL */
#define ARIZONA_GPN_POL_SHIFT                        10  /* GPN_POL */
#define ARIZONA_GPN_POL_WIDTH                         1  /* GPN_POL */
#define ARIZONA_GPN_OP_CFG_MASK                  0x0200  /* GPN_OP_CFG */
#define ARIZONA_GPN_OP_CFG_SHIFT                      9  /* GPN_OP_CFG */
#define ARIZONA_GPN_OP_CFG_WIDTH                      1  /* GPN_OP_CFG */
#define ARIZONA_GPN_DB_MASK                      0x0100  /* GPN_DB */
#define ARIZONA_GPN_DB_SHIFT                          8  /* GPN_DB */
#define ARIZONA_GPN_DB_WIDTH                          1  /* GPN_DB */
#define ARIZONA_GPN_FN_MASK                      0x007F  /* GPN_FN - [6:0] */
#define ARIZONA_GPN_FN_SHIFT                          0  /* GPN_FN - [6:0] */
#define ARIZONA_GPN_FN_WIDTH                          7  /* GPN_FN - [6:0] */

#define CLEARWATER_GPN_LVL                           0x8000  /* GPN_LVL */
#define CLEARWATER_GPN_LVL_MASK                      0x8000  /* GPN_LVL */
#define CLEARWATER_GPN_LVL_SHIFT                         15  /* GPN_LVL */
#define CLEARWATER_GPN_LVL_WIDTH                          1  /* GPN_LVL */

#define ARIZONA_MAX_GPIO_REGS 5
#define CLEARWATER_MAX_GPIO_REGS 80

#define CLEARWATER_NUM_GPIOS	40
#define MARLEY_NUM_GPIOS	16
#define MOON_NUM_GPIOS		38

#define ARIZONA_MAX_INPUT 12

#define ARIZONA_MAX_MICBIAS 4
#define ARIZONA_MAX_CHILD_MICBIAS 4

#define WM5102_NUM_MICBIAS       3
#define CLEARWATER_NUM_MICBIAS   4
#define LARGO_NUM_MICBIAS        2
#define MARLEY_NUM_MICBIAS       2
#define MARLEY_NUM_CHILD_MICBIAS 2
#define MOON_NUM_MICBIAS         2
#define MOON_NUM_CHILD_MICBIAS   4

#define ARIZONA_MAX_OUTPUT 6

#define ARIZONA_MAX_AIF 4

#define ARIZONA_HAP_ACT_ERM 0
#define ARIZONA_HAP_ACT_LRA 2

#define ARIZONA_MAX_PDM_SPK 2

/* Treat INT_MAX impedance as open circuit */
#define ARIZONA_HP_Z_OPEN INT_MAX

#define ARIZONA_MAX_DSP	7

struct regulator_init_data;

struct arizona_jd_state;

struct arizona_micbias {
	int mV;                    /** Regulated voltage */
	unsigned int ext_cap:1;    /** External capacitor fitted */
	/** Actively discharge */
	unsigned int discharge[ARIZONA_MAX_CHILD_MICBIAS];
	unsigned int soft_start:1; /** Disable aggressive startup ramp rate */
	unsigned int bypass:1;     /** Use bypass mode */
};

struct arizona_micd_config {
	unsigned int src;
	unsigned int gnd;
	unsigned int bias;
	bool gpio;
};

struct arizona_micd_range {
	int max;  /** Ohms */
	int key;  /** Key to report to input layer */
};

struct arizona_hpd_pins {
	unsigned int clamp_pin;
	unsigned int impd_pin;
};

struct arizona_pdata {
	int reset;      /** GPIO controlling /RESET, if any */
	int ldoena;     /** GPIO controlling LODENA, if any */

	/** Regulator configuration for MICVDD */
	struct regulator_init_data *micvdd;

	/** Regulator configuration for LDO1 */
	struct regulator_init_data *ldo1;

	/** If a direct 32kHz clock is provided on an MCLK specify it here */
	int clk32k_src;

	/** Mode for primary IRQ (defaults to active low) */
	unsigned int irq_flags;

	/* Base GPIO */
	int gpio_base;

	/** Pin state for GPIO pins
	 * Defines default pin function and state for each GPIO
	 *
	 * 0 = leave at chip default
	 * values 0x1..0xffff = set to this value
	 * >0xffff = set to 0
	 */
	unsigned int gpio_defaults[CLEARWATER_MAX_GPIO_REGS];

	/**
	 * Maximum number of channels clocks will be generated for,
	 * useful for systems where and I2S bus with multiple data
	 * lines is mastered.
	 */
	int max_channels_clocked[ARIZONA_MAX_AIF];

	/** Time in milliseconds to keep wake lock during jack detection */
	int jd_wake_time;

	/** GPIO5 is used for jack detection */
	bool jd_gpio5;

	/** Internal pull on GPIO5 is disabled when used for jack detection */
	bool jd_gpio5_nopull;

	/** JD2 IRQ is used for jack detection */
	bool jd2_irq;

	/** set to true if jackdet contact opens on insert */
	bool jd_invert;

	/** If non-zero don't run headphone detection, report this value */
	int fixed_hpdet_imp;

	/** Use the headphone detect circuit to identify the accessory */
	bool hpdet_acc_id;

	/** Check for line output with HPDET method */
	bool hpdet_acc_id_line;

	/** GPIO used for mic isolation with HPDET */
	int hpdet_id_gpio;

	/** Callback notifying HPDET result */
	void (*hpdet_cb)(unsigned int measurement);

	/** Callback notifying mic presence */
	void (*micd_cb)(bool mic);

	/** If non-zero, specifies the maximum impedance in ohms
	 * that will be considered as a short circuit.
	 */
	int hpdet_short_circuit_imp;

	/**
	 * Channel to use for headphone detection, valid values are 0 for
	 * left and 1 for right
	 */
	int hpdet_channel;

	/** Use software comparison to determine mic presence */
	bool micd_software_compare;

	/** Extra debounce timeout used during initial mic detection (ms) */
	int micd_detect_debounce;

	/** Extra software debounces during button detection */
	int micd_manual_debounce;

	/** GPIO for mic detection polarity */
	int micd_pol_gpio;

	/** Mic detect ramp rate */
	int micd_bias_start_time;

	/** Mic detect sample rate */
	int micd_rate;

	/** Mic detect debounce level */
	int micd_dbtime;

	/** Mic detect timeout (ms) */
	int micd_timeout;

	/** Force MICBIAS on for mic detect */
	bool micd_force_micbias;

	/** Force MICBIAS on for initial mic detect only, not button detect */
	bool micd_force_micbias_initial;

	/** Declare an open circuit as a 4 pole jack */
	bool micd_open_circuit_declare;

	/** Delay between jack detection and MICBIAS ramp */
	int init_mic_delay;

	/** Mic detect level parameters */
	const struct arizona_micd_range *micd_ranges;
	int num_micd_ranges;

	/** Mic detect clamp function */
	unsigned int micd_clamp_mode;

	/** Headset polarity configurations */
	struct arizona_micd_config *micd_configs;
	int num_micd_configs;

	/**
	* [clamp_pin, impedance_measurement_pin] for HPL
	* of 3.5mm Jack
	*/
	struct arizona_hpd_pins hpd_l_pins;

	/**
	* [clamp_pin, impedance_measurement_pin] for HPR
	* of 3.5mm Jack
	*/
	struct arizona_hpd_pins hpd_r_pins;

	/** Reference voltage for DMIC inputs */
	int dmic_ref[ARIZONA_MAX_INPUT];

	/** Clock Source for DMIC's */
	int dmic_clksrc[ARIZONA_MAX_INPUT];

	/** MICBIAS configurations */
	struct arizona_micbias micbias[ARIZONA_MAX_MICBIAS];

	/**
	 * Mode of input structures
	 * One of the ARIZONA_INMODE_xxx values
	 * For most codecs the entries are [0]=IN1 [1]=IN2 [2]=IN3 [3]=IN4
	 * wm8998: [0]=IN1A [1]=IN2A [2]=IN1B [3]=IN2B
	 * cs47l85, wm8285: [0]=IN1L [1]=IN1R [2]=IN2L [3]=IN2R [4]=IN3L [5]=IN3R
	 */
	int inmode[ARIZONA_MAX_INPUT];

	/** Mode for outputs */
	bool out_mono[ARIZONA_MAX_OUTPUT];

	/** Provide improved ultrasonic frequency response */
	bool ultrasonic_response;

	/** PDM speaker mute setting */
	unsigned int spk_mute[ARIZONA_MAX_PDM_SPK];

	/** PDM speaker format */
	unsigned int spk_fmt[ARIZONA_MAX_PDM_SPK];

	/** Haptic actuator type */
	unsigned int hap_act;

	/** GPIO for primary IRQ (used for edge triggered emulation) */
	int irq_gpio;

	/** General purpose switch control */
	unsigned int gpsw;

	/** Callback which is called when the trigger phrase is detected */
	void (*ez2ctrl_trigger)(void);

	/** Callback which is called when a DSP panic is detected */
	void (*ez2panic_trigger)(int dsp, u16 *msg);

	/** Callback which is called when text data from a DSP is detected */
	void (*ez2text_trigger)(int dsp);


	/** wm5102t output power */
	unsigned int wm5102t_output_pwr;

	/** Override the normal jack detection */
	const struct arizona_jd_state *custom_jd;

	struct wm_adsp_fw_defs *fw_defs[ARIZONA_MAX_DSP];
	int num_fw_defs[ARIZONA_MAX_DSP];

	/** Some platforms add a series resistor for hpdet to suppress pops */
	int hpdet_ext_res;

	/** Load firmwares for specific chip revisions */
	bool rev_specific_fw;

	/**
	 * Specify an input to mute during headset button presses and jack
	 * removal: 1 - IN1L, 2 - IN1R, ..., n - IN[n]R
	 */
	unsigned int hs_mic;

	/* If lrclk_adv is set then in dsp-a mode,
	fsync is shifted left by half bclk */
	int lrclk_adv[ARIZONA_MAX_AIF];

	/* Report headset state as an input switch event */
	bool report_to_input;
};

#endif
