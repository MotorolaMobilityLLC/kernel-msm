/*
 * arizona.h - Wolfson Arizona class device shared support
 *
 * Copyright 2012 Wolfson Microelectronics plc
 *
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ASOC_ARIZONA_H
#define _ASOC_ARIZONA_H

#include <linux/completion.h>

#include <sound/soc.h>

#include "wm_adsp.h"

#define ARIZONA_CLK_SYSCLK         1
#define ARIZONA_CLK_ASYNCCLK       2
#define ARIZONA_CLK_OPCLK          3
#define ARIZONA_CLK_ASYNC_OPCLK    4
#define ARIZONA_CLK_SYSCLK_2       5
#define ARIZONA_CLK_SYSCLK_3       6
#define ARIZONA_CLK_ASYNCCLK_2     7
#define ARIZONA_CLK_DSPCLK         8

#define ARIZONA_CLK_SRC_MCLK1    0x0
#define ARIZONA_CLK_SRC_MCLK2    0x1
#define ARIZONA_CLK_SRC_FLL1     0x4
#define ARIZONA_CLK_SRC_FLL2     0x5
#define ARIZONA_CLK_SRC_FLL3     0x6
#define ARIZONA_CLK_SRC_FLLAO_HI 0x7
#define ARIZONA_CLK_SRC_AIF1BCLK 0x8
#define ARIZONA_CLK_SRC_AIF2BCLK 0x9
#define ARIZONA_CLK_SRC_AIF3BCLK 0xa
#define ARIZONA_CLK_SRC_AIF4BCLK 0xb
#define ARIZONA_CLK_SRC_FLLAO    0xF

#define ARIZONA_FLL_SRC_NONE      -1
#define ARIZONA_FLL_SRC_MCLK1      0
#define ARIZONA_FLL_SRC_MCLK2      1
#define ARIZONA_FLL_SRC_SLIMCLK    3
#define ARIZONA_FLL_SRC_FLL1       4
#define ARIZONA_FLL_SRC_FLL2       5
#define ARIZONA_FLL_SRC_AIF1BCLK   8
#define ARIZONA_FLL_SRC_AIF2BCLK   9
#define ARIZONA_FLL_SRC_AIF3BCLK  10
#define ARIZONA_FLL_SRC_AIF4BCLK  11
#define ARIZONA_FLL_SRC_AIF1LRCLK 12
#define ARIZONA_FLL_SRC_AIF2LRCLK 13
#define ARIZONA_FLL_SRC_AIF3LRCLK 14
#define ARIZONA_FLL_SRC_AIF4LRCLK 15

#define ARIZONA_MIXER_VOL_MASK             0x00FE
#define ARIZONA_MIXER_VOL_SHIFT                 1
#define ARIZONA_MIXER_VOL_WIDTH                 7

#define ARIZONA_CLK_6MHZ   0
#define ARIZONA_CLK_12MHZ  1
#define ARIZONA_CLK_24MHZ  2
#define ARIZONA_CLK_49MHZ  3
#define ARIZONA_CLK_73MHZ  4
#define CLEARWATER_CLK_98MHZ   4
#define ARIZONA_CLK_98MHZ  5
#define ARIZONA_CLK_147MHZ 6

#define CLEARWATER_DSP_CLK_9MHZ   0
#define CLEARWATER_DSP_CLK_18MHZ  1
#define CLEARWATER_DSP_CLK_36MHZ  2
#define CLEARWATER_DSP_CLK_73MHZ  3
#define CLEARWATER_DSP_CLK_147MHZ 4

#define ARIZONA_MAX_DAI  15
#define ARIZONA_MAX_ADSP 7

#define ARIZONA_SLIM1 4
#define ARIZONA_SLIM2 5
#define ARIZONA_SLIM3 6

#define ARIZONA_SLIMCLK_384kHZ		384000
#define ARIZONA_SLIMCLK_768kHZ		768000
#define ARIZONA_SLIMCLK_1P536MHZ	1536000
#define ARIZONA_SLIMCLK_3P072MHZ	3072000
#define ARIZONA_SLIMCLK_6P144MHZ	6144000
#define ARIZONA_SLIMCLK_12P288MHZ	12288000
#define ARIZONA_SLIMCLK_GEAR_MASK	0xF

struct arizona;
struct wm_adsp;
struct arizona_jd_state;

struct arizona_dai_priv {
	int clk;
};

struct arizona_priv {
	struct wm_adsp adsp[ARIZONA_MAX_ADSP];
	struct arizona *arizona;
	int sysclk;
	int asyncclk;
	int dspclk;
	struct arizona_dai_priv dai[ARIZONA_MAX_DAI];

	int num_inputs;
	unsigned int in_pending;

	unsigned int out_up_pending;
	unsigned int out_up_delay;
	unsigned int out_down_pending;
	unsigned int out_down_delay;
};

#define ARIZONA_NUM_MIXER_INPUTS 134
#define ARIZONA_V2_NUM_MIXER_INPUTS 146

extern const unsigned int arizona_mixer_tlv[];
extern const char * const arizona_mixer_texts[ARIZONA_NUM_MIXER_INPUTS];
extern unsigned int arizona_mixer_values[ARIZONA_NUM_MIXER_INPUTS];
extern const char * const arizona_v2_mixer_texts[ARIZONA_V2_NUM_MIXER_INPUTS];
extern unsigned int arizona_v2_mixer_values[ARIZONA_V2_NUM_MIXER_INPUTS];

#define ARIZONA_GAINMUX_CONTROLS(name, base) \
	SOC_SINGLE_RANGE_TLV(name " Input Volume", base + 1,		\
			     ARIZONA_MIXER_VOL_SHIFT, 0x20, 0x50, 0,	\
			     arizona_mixer_tlv)

#define ARIZONA_MIXER_CONTROLS(name, base) \
	SOC_SINGLE_RANGE_TLV(name " Input 1 Volume", base + 1,		\
			     ARIZONA_MIXER_VOL_SHIFT, 0x20, 0x50, 0,	\
			     arizona_mixer_tlv),			\
	SOC_SINGLE_RANGE_TLV(name " Input 2 Volume", base + 3,		\
			     ARIZONA_MIXER_VOL_SHIFT, 0x20, 0x50, 0,	\
			     arizona_mixer_tlv),			\
	SOC_SINGLE_RANGE_TLV(name " Input 3 Volume", base + 5,		\
			     ARIZONA_MIXER_VOL_SHIFT, 0x20, 0x50, 0,	\
			     arizona_mixer_tlv),			\
	SOC_SINGLE_RANGE_TLV(name " Input 4 Volume", base + 7,		\
			     ARIZONA_MIXER_VOL_SHIFT, 0x20, 0x50, 0,	\
			     arizona_mixer_tlv)

struct arizona_enum {
	struct soc_enum mixer_enum;
	int val;
};

#define ARIZONA_MUX_ENUM_DECL(name, reg) \
	SOC_VALUE_ENUM_SINGLE_DECL(name, reg, 0, 0xff,			\
				   arizona_mixer_texts, arizona_mixer_values)

#define ARIZONA_MUX_CTL_DECL(name) \
	const struct snd_kcontrol_new name##_mux =	\
		SOC_DAPM_ENUM("Route", name##_enum)

#define ARIZONA_MUX_ENUMS(name, base_reg) \
	static ARIZONA_MUX_ENUM_DECL(name##_enum, base_reg);      \
	static ARIZONA_MUX_CTL_DECL(name)

#define ARIZONA_MIXER_ENUMS(name, base_reg) \
	ARIZONA_MUX_ENUMS(name##_in1, base_reg);     \
	ARIZONA_MUX_ENUMS(name##_in2, base_reg + 2); \
	ARIZONA_MUX_ENUMS(name##_in3, base_reg + 4); \
	ARIZONA_MUX_ENUMS(name##_in4, base_reg + 6)

#define ARIZONA_DSP_AUX_ENUMS(name, base_reg) \
	ARIZONA_MUX_ENUMS(name##_aux1, base_reg);	\
	ARIZONA_MUX_ENUMS(name##_aux2, base_reg + 8);	\
	ARIZONA_MUX_ENUMS(name##_aux3, base_reg + 16);	\
	ARIZONA_MUX_ENUMS(name##_aux4, base_reg + 24);	\
	ARIZONA_MUX_ENUMS(name##_aux5, base_reg + 32);	\
	ARIZONA_MUX_ENUMS(name##_aux6, base_reg + 40)

#define CLEARWATER_MUX_ENUM_DECL(name, reg) \
	SOC_VALUE_ENUM_SINGLE_DECL(name, reg, 0, 0xff,			\
				   arizona_v2_mixer_texts, arizona_v2_mixer_values)

#define CLEARWATER_MUX_ENUMS(name, base_reg) \
	static CLEARWATER_MUX_ENUM_DECL(name##_enum, base_reg);      \
	static ARIZONA_MUX_CTL_DECL(name)

#define CLEARWATER_MIXER_ENUMS(name, base_reg) \
	CLEARWATER_MUX_ENUMS(name##_in1, base_reg);     \
	CLEARWATER_MUX_ENUMS(name##_in2, base_reg + 2); \
	CLEARWATER_MUX_ENUMS(name##_in3, base_reg + 4); \
	CLEARWATER_MUX_ENUMS(name##_in4, base_reg + 6)

#define CLEARWATER_DSP_AUX_ENUMS(name, base_reg) \
	CLEARWATER_MUX_ENUMS(name##_aux1, base_reg);	\
	CLEARWATER_MUX_ENUMS(name##_aux2, base_reg + 8);	\
	CLEARWATER_MUX_ENUMS(name##_aux3, base_reg + 16);	\
	CLEARWATER_MUX_ENUMS(name##_aux4, base_reg + 24);	\
	CLEARWATER_MUX_ENUMS(name##_aux5, base_reg + 32);	\
	CLEARWATER_MUX_ENUMS(name##_aux6, base_reg + 40)

#define ARIZONA_MUX(name, ctrl) \
	SND_SOC_DAPM_MUX(name, SND_SOC_NOPM, 0, 0, ctrl)

#define ARIZONA_MUX_WIDGETS(name, name_str) \
	ARIZONA_MUX(name_str " Input", &name##_mux)

#define ARIZONA_MIXER_WIDGETS(name, name_str)	\
	ARIZONA_MUX(name_str " Input 1", &name##_in1_mux), \
	ARIZONA_MUX(name_str " Input 2", &name##_in2_mux), \
	ARIZONA_MUX(name_str " Input 3", &name##_in3_mux), \
	ARIZONA_MUX(name_str " Input 4", &name##_in4_mux), \
	SND_SOC_DAPM_MIXER(name_str " Mixer", SND_SOC_NOPM, 0, 0, NULL, 0)

#define ARIZONA_DSP_WIDGETS(name, name_str) \
	ARIZONA_MIXER_WIDGETS(name##L, name_str "L"), \
	ARIZONA_MIXER_WIDGETS(name##R, name_str "R"), \
	ARIZONA_MUX(name_str " Aux 1", &name##_aux1_mux), \
	ARIZONA_MUX(name_str " Aux 2", &name##_aux2_mux), \
	ARIZONA_MUX(name_str " Aux 3", &name##_aux3_mux), \
	ARIZONA_MUX(name_str " Aux 4", &name##_aux4_mux), \
	ARIZONA_MUX(name_str " Aux 5", &name##_aux5_mux), \
	ARIZONA_MUX(name_str " Aux 6", &name##_aux6_mux)

#define ARIZONA_MUX_ROUTES(widget, name) \
	{ widget, NULL, name " Input" }, \
	ARIZONA_MIXER_INPUT_ROUTES(name " Input")

#define ARIZONA_MIXER_ROUTES(widget, name) \
	{ widget, NULL, name " Mixer" },         \
	{ name " Mixer", NULL, name " Input 1" }, \
	{ name " Mixer", NULL, name " Input 2" }, \
	{ name " Mixer", NULL, name " Input 3" }, \
	{ name " Mixer", NULL, name " Input 4" }, \
	ARIZONA_MIXER_INPUT_ROUTES(name " Input 1"), \
	ARIZONA_MIXER_INPUT_ROUTES(name " Input 2"), \
	ARIZONA_MIXER_INPUT_ROUTES(name " Input 3"), \
	ARIZONA_MIXER_INPUT_ROUTES(name " Input 4")

#define ARIZONA_DSP_ROUTES(name) \
	{ name, NULL, name " Preloader"}, \
	{ name " Preloader", NULL, name " Aux 1" }, \
	{ name " Preloader", NULL, name " Aux 2" }, \
	{ name " Preloader", NULL, name " Aux 3" }, \
	{ name " Preloader", NULL, name " Aux 4" }, \
	{ name " Preloader", NULL, name " Aux 5" }, \
	{ name " Preloader", NULL, name " Aux 6" }, \
	ARIZONA_MIXER_INPUT_ROUTES(name " Aux 1"), \
	ARIZONA_MIXER_INPUT_ROUTES(name " Aux 2"), \
	ARIZONA_MIXER_INPUT_ROUTES(name " Aux 3"), \
	ARIZONA_MIXER_INPUT_ROUTES(name " Aux 4"), \
	ARIZONA_MIXER_INPUT_ROUTES(name " Aux 5"), \
	ARIZONA_MIXER_INPUT_ROUTES(name " Aux 6"), \
	ARIZONA_MIXER_ROUTES(name " Preloader", name "L"), \
	ARIZONA_MIXER_ROUTES(name " Preloader", name "R")

#define ARIZONA_SAMPLE_RATE_CONTROL(name, domain) \
	SOC_ENUM(name, arizona_sample_rate[(domain) - 2])

#define ARIZONA_SAMPLE_RATE_CONTROL_DVFS(name, domain)        \
	SOC_ENUM_EXT(name, arizona_sample_rate[(domain) - 2], \
			snd_soc_get_enum_double,              \
			arizona_put_sample_rate_enum)

#define ARIZONA_EQ_CONTROL(xname, xbase) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname,	   \
	.info = snd_soc_bytes_info, .get = snd_soc_bytes_get,      \
	.put = arizona_eq_coeff_put, .private_value =		   \
	((unsigned long)&(struct soc_bytes)			   \
		{.base = xbase, .num_regs = 20, \
		 .mask = ~ARIZONA_EQ1_B1_MODE }) }

#define ARIZONA_LHPF_CONTROL(xname, xbase)                    \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname,   \
	.info = snd_soc_bytes_info, .get = snd_soc_bytes_get, \
	.put = arizona_lhpf_coeff_put, .private_value =       \
	((unsigned long)&(struct soc_bytes) { .base = xbase,  \
	.num_regs = 1 }) }

#define CLEARWATER_OSR_ENUM_SIZE 5
#define ARIZONA_RATE_ENUM_SIZE 5
#define ARIZONA_SYNC_RATE_ENUM_SIZE 3
#define ARIZONA_ASYNC_RATE_ENUM_SIZE 2
#define ARIZONA_SAMPLE_RATE_ENUM_SIZE 14
#define ARIZONA_ANC_INPUT_ENUM_SIZE 19
#define WM8280_ANC_INPUT_ENUM_SIZE 13
#define CLEARWATER_ANC_INPUT_ENUM_SIZE 19
#define MOON_DFC_TYPE_ENUM_SIZE  5
#define MOON_DFC_WIDTH_ENUM_SIZE 5

extern const char * const arizona_rate_text[ARIZONA_RATE_ENUM_SIZE];
extern const unsigned int arizona_rate_val[ARIZONA_RATE_ENUM_SIZE];
extern const char * const arizona_sample_rate_text[ARIZONA_SAMPLE_RATE_ENUM_SIZE];
extern const unsigned int arizona_sample_rate_val[ARIZONA_SAMPLE_RATE_ENUM_SIZE];
extern const char * const moon_dfc_width_text[MOON_DFC_WIDTH_ENUM_SIZE];
extern const unsigned int moon_dfc_width_val[MOON_DFC_WIDTH_ENUM_SIZE];
extern const char * const moon_dfc_type_text[MOON_DFC_TYPE_ENUM_SIZE];
extern const unsigned int moon_dfc_type_val[MOON_DFC_TYPE_ENUM_SIZE];

extern const struct soc_enum arizona_sample_rate[];
extern const struct soc_enum arizona_isrc_fsl[];
extern const struct soc_enum arizona_isrc_fsh[];
extern const struct soc_enum arizona_asrc_rate1;
extern const struct soc_enum arizona_asrc_rate2;
extern const struct soc_enum clearwater_asrc1_rate[];
extern const struct soc_enum clearwater_asrc2_rate[];
extern const struct soc_enum arizona_input_rate;
extern const struct soc_enum arizona_output_rate;
extern const struct soc_enum arizona_fx_rate;
extern const struct soc_enum arizona_spdif_rate;
extern const struct soc_enum moon_input_rate[];
extern const struct soc_enum moon_dfc_width[];
extern const struct soc_enum moon_dfc_type[];

extern const struct soc_enum arizona_in_vi_ramp;
extern const struct soc_enum arizona_in_vd_ramp;

extern const struct soc_enum arizona_out_vi_ramp;
extern const struct soc_enum arizona_out_vd_ramp;

extern const struct soc_enum arizona_lhpf1_mode;
extern const struct soc_enum arizona_lhpf2_mode;
extern const struct soc_enum arizona_lhpf3_mode;
extern const struct soc_enum arizona_lhpf4_mode;

extern const struct soc_enum arizona_ng_hold;
extern const struct soc_enum arizona_in_hpf_cut_enum;
extern const struct soc_enum arizona_in_dmic_osr[];
extern const struct soc_enum clearwater_in_dmic_osr[];

extern const struct soc_enum arizona_anc_input_src[];
extern const struct soc_enum clearwater_anc_input_src[];
extern const struct soc_enum arizona_output_anc_src[];
extern const struct soc_enum clearwater_output_anc_src_defs[];
extern const struct soc_enum arizona_ip_mode[];

extern int arizona_ip_mode_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);

extern int moon_in_rate_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);

extern int moon_dfc_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);

extern int moon_osr_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);

extern int moon_lp_mode_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);

extern int arizona_put_anc_input(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol);
extern int arizona_get_anc_input(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol);

extern int arizona_in_ev(struct snd_soc_dapm_widget *w,
			 struct snd_kcontrol *kcontrol,
			 int event);
extern int arizona_out_ev(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol,
			  int event);
extern int arizona_hp_ev(struct snd_soc_dapm_widget *w,
			 struct snd_kcontrol *kcontrol,
			 int event);
extern int florida_hp_ev(struct snd_soc_dapm_widget *w,
			 struct snd_kcontrol *kcontrol,
			 int event);
extern int clearwater_hp_ev(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol,
			    int event);
extern int moon_hp_ev(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol,
			    int event);
extern int moon_analog_ev(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *kcontrol,
			    int event);
extern int arizona_anc_ev(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol,
			  int event);

extern int arizona_mux_info(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo);
extern int arizona_mux_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);
extern int arizona_mux_put(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol);
extern int arizona_mux_event(struct snd_soc_dapm_widget *w,
			     struct snd_kcontrol *kcontrol, int event);

extern int arizona_put_sample_rate_enum(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol);

extern int arizona_eq_coeff_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol);

extern int arizona_lhpf_coeff_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol);

extern int arizona_set_sysclk(struct snd_soc_codec *codec, int clk_id,
			      int source, unsigned int freq, int dir);

extern int arizona_cache_and_clear_sources(struct arizona *arizona,
					   const int *sources,
					   unsigned int *cache,
					   int lim);

extern int arizona_restore_sources(struct arizona *arizona,
				   const int *sources,
				   unsigned int *cache,
				   int lim);

/* Slimbus */
extern int arizona_slim_tx_ev(struct snd_soc_dapm_widget *w,
                             struct snd_kcontrol *kcontrol,
                             int event);
extern int arizona_slim_rx_ev(struct snd_soc_dapm_widget *w,
                             struct snd_kcontrol *kcontrol,
                             int event);

extern void clearwater_spin_sysclk(struct arizona *arizona);

extern const struct snd_soc_dai_ops arizona_dai_ops;
extern const struct snd_soc_dai_ops arizona_simple_dai_ops;
extern const struct snd_soc_dai_ops arizona_slim_dai_ops;

#define ARIZONA_FLL_NAME_LEN 20

struct arizona_fll {
	struct arizona *arizona;
	int id;
	unsigned int base;
	unsigned int sync_offset;
	unsigned int vco_mult;
	struct completion ok;

	unsigned int fvco;
	int min_outdiv;
	int max_outdiv;
	int outdiv;

	unsigned int fout;
	int sync_src;
	unsigned int sync_freq;
	int ref_src;
	unsigned int ref_freq;
	struct mutex lock;

	char lock_name[ARIZONA_FLL_NAME_LEN];
	char clock_ok_name[ARIZONA_FLL_NAME_LEN];
};

extern int arizona_init_fll(struct arizona *arizona, int id, int base,
			    int lock_irq, int ok_irq, struct arizona_fll *fll);
extern int arizona_set_fll_refclk(struct arizona_fll *fll, int source,
				  unsigned int Fref, unsigned int Fout);
extern int arizona_set_fll(struct arizona_fll *fll, int source,
			   unsigned int Fref, unsigned int Fout);
extern int arizona_set_fll_ao(struct arizona_fll *fll, int source,
		    unsigned int fin, unsigned int fout);
extern int arizona_get_fll(struct arizona_fll *fll, int *source,
			   unsigned int *Fref, unsigned int *Fout);

extern int arizona_init_spk(struct snd_soc_codec *codec);
extern int arizona_init_gpio(struct snd_soc_codec *codec);
extern int arizona_init_mono(struct snd_soc_codec *codec);
extern int arizona_init_input(struct snd_soc_codec *codec);

extern int arizona_adsp_power_ev(struct snd_soc_dapm_widget *w,
				 struct snd_kcontrol *kcontrol,
				 int event);

extern int arizona_init_dai(struct arizona_priv *priv, int dai);

int arizona_set_output_mode(struct snd_soc_codec *codec, int output,
			    bool diff);

extern int arizona_set_hpdet_cb(struct snd_soc_codec *codec,
				void (*hpdet_cb)(unsigned int measurement));
extern int arizona_set_micd_cb(struct snd_soc_codec *codec,
				void (*micd_cb)(bool mic));
extern int arizona_set_ez2ctrl_cb(struct snd_soc_codec *codec,
				  void (*ez2ctrl_trigger)(void));
extern int arizona_set_ez2panic_cb(struct snd_soc_codec *codec,
				  void (*ez2panic_trigger)(int dsp, u16 *msg));
extern int arizona_set_ez2text_cb(struct snd_soc_codec *codec,
				  void (*ez2text_trigger)(int dsp));
extern int arizona_set_custom_jd(struct snd_soc_codec *codec,
				 const struct arizona_jd_state *custom_jd);

extern int florida_put_dre(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol);
extern int clearwater_put_dre(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol);
extern int arizona_put_out4_edre(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol);

extern struct regmap *arizona_get_regmap_dsp(struct snd_soc_codec *codec);

extern struct arizona_extcon_info *
arizona_get_extcon_info(struct snd_soc_codec *codec);

extern int arizona_enable_force_bypass(struct snd_soc_codec *codec);
extern int arizona_disable_force_bypass(struct snd_soc_codec *codec);

#endif
