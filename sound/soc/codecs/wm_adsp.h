/*
 * wm_adsp.h  --  Wolfson ADSP support
 *
 * Copyright 2012 Wolfson Microelectronics plc
 *
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __WM_ADSP_H
#define __WM_ADSP_H

#include <linux/circ_buf.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/compress_driver.h>

#include "wmfw.h"

#define WM_ADSP2_REGION_0 BIT(0)
#define WM_ADSP2_REGION_1 BIT(1)
#define WM_ADSP2_REGION_2 BIT(2)
#define WM_ADSP2_REGION_3 BIT(3)
#define WM_ADSP2_REGION_4 BIT(4)
#define WM_ADSP2_REGION_5 BIT(5)
#define WM_ADSP2_REGION_6 BIT(6)
#define WM_ADSP2_REGION_7 BIT(7)
#define WM_ADSP2_REGION_8 BIT(8)
#define WM_ADSP2_REGION_9 BIT(9)
#define WM_ADSP2_REGION_1_9 (WM_ADSP2_REGION_1 | \
		WM_ADSP2_REGION_2 | WM_ADSP2_REGION_3 | \
		WM_ADSP2_REGION_4 | WM_ADSP2_REGION_5 | \
		WM_ADSP2_REGION_6 | WM_ADSP2_REGION_7 | \
		WM_ADSP2_REGION_8 | WM_ADSP2_REGION_9)
#define WM_ADSP2_REGION_ALL (WM_ADSP2_REGION_0 | WM_ADSP2_REGION_1_9)

struct wm_adsp_region {
	int type;
	unsigned int base;
};

struct wm_adsp_alg_region {
	struct list_head list;
	unsigned int alg;
	int type;
	unsigned int base;
};

struct wm_adsp_buffer_region {
	unsigned int offset;
	unsigned int cumulative_size;
	unsigned int mem_type;
	unsigned int base_addr;
};

struct wm_adsp_buffer_region_def {
	unsigned int mem_type;
	unsigned int base_offset;
	unsigned int size_offset;
};

struct wm_adsp_fw_caps {
	u32 id;
	struct snd_codec_desc desc;
	int num_host_regions;
	struct wm_adsp_buffer_region_def *host_region_defs;
};

struct wm_adsp_fw_defs {
	const char *name;
	const char *file;
	const char *binfile;
	int compr_direction;
	int num_caps;
	struct wm_adsp_fw_caps *caps;
};

struct wm_adsp_fw_features {
	bool shutdown:1;
	bool ez2control_trigger:1;
};

struct wm_adsp {
	const char *part;
	char part_rev;
	int num;
	int type;
	int rev;
	struct device *dev;
	struct regmap *regmap;
	struct snd_soc_card *card;

	int base;
	int sysclk_reg;
	int sysclk_mask;
	int sysclk_shift;

	unsigned int rate_cache;
	struct mutex rate_lock;
	int (*rate_put_cb) (struct wm_adsp *adsp, unsigned int mask,
			    unsigned int val);

	struct list_head alg_regions;

	int fw_id;
	int fw_id_version;

	const struct wm_adsp_region *mem;
	int num_mems;

	int fw;
	int fw_ver;
	u32 running;

	struct mutex ctl_lock;

	u32 host_buf_ptr;
	u32 host_buf_ptr2;

	int max_dsp_read_bytes;
	u32 dsp_error;
	u32 *raw_capt_buf;
	struct circ_buf capt_buf;
	int capt_buf_size;

	u32 *raw_capt_buf2;
	struct circ_buf capt_buf2;

	u32 capt_watermark;
	u32 capt_watermark2;
	struct wm_adsp_buffer_region *host_regions;
	struct wm_adsp_buffer_region *host_regions2;
	bool buffer_drain_pending;
	bool buffer2_drain_pending;

	int num_firmwares;
	struct wm_adsp_fw_defs *firmwares;

	struct list_head ctl_list;

	struct wm_adsp_fw_features fw_features;

	struct mutex *fw_lock;
	struct work_struct boot_work;

#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs_root;
	char *wmfw_file_loaded;
	char *bin_file_loaded;
#endif

	unsigned int lock_regions;
};

#define ADSP2_BUFFER_1			       1
#define ADSP2_BUFFER_2			       2

#define WM_ADSP1(wname, num) \
	SND_SOC_DAPM_PGA_E(wname, SND_SOC_NOPM, num, 0, NULL, 0, \
		wm_adsp1_event, SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD)

#define WM_ADSP2(wname, num, event_fn) \
{	.id = snd_soc_dapm_dai_link, .name = wname " Preloader", \
	.reg = SND_SOC_NOPM, .shift = num, .event = event_fn, \
	.event_flags = SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD }, \
{	.id = snd_soc_dapm_out_drv, .name = wname, \
	.reg = SND_SOC_NOPM, .shift = num, .event = wm_adsp2_event, \
	.event_flags = SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD }

extern const struct snd_kcontrol_new wm_adsp1_fw_controls[];
extern const struct snd_kcontrol_new wm_adsp2_fw_controls[];
extern const struct snd_kcontrol_new wm_adsp2v2_fw_controls[];

int wm_adsp1_init(struct wm_adsp *dsp);
int wm_adsp2_init(struct wm_adsp *dsp, struct mutex *fw_lock);

#ifdef CONFIG_DEBUG_FS
void wm_adsp_init_debugfs(struct wm_adsp *dsp, struct snd_soc_codec *codec);
void wm_adsp_cleanup_debugfs(struct wm_adsp *dsp);
#else
static inline void wm_adsp_init_debugfs(struct wm_adsp *dsp,
					struct snd_soc_codec *codec)
{
}

void wm_adsp_cleanup_debugfs(struct wm_adsp *dsp)
{
}
#endif

int wm_adsp1_event(struct snd_soc_dapm_widget *w,
		   struct snd_kcontrol *kcontrol, int event);

#if defined(CONFIG_SND_SOC_WM_ADSP)
int wm_adsp2_early_event(struct snd_soc_dapm_widget *w,
			 struct snd_kcontrol *kcontrol, int event,
			 unsigned int freq);
#else
static inline int wm_adsp2_early_event(struct snd_soc_dapm_widget *w,
				       struct snd_kcontrol *kcontrol, int event,
				       unsigned int freq)
{
	return 0;
}
#endif

int wm_adsp2_lock(struct wm_adsp *adsp, unsigned int regions);
irqreturn_t wm_adsp2_bus_error(struct wm_adsp *adsp);

int wm_adsp2_event(struct snd_soc_dapm_widget *w,
		   struct snd_kcontrol *kcontrol, int event);

extern bool wm_adsp_compress_supported(const struct wm_adsp *adsp,
				       const struct snd_compr_stream *stream);
extern bool wm_adsp_format_supported(const struct wm_adsp *adsp,
				     const struct snd_compr_stream *stream,
				     const struct snd_compr_params *params);
extern void wm_adsp_get_caps(const struct wm_adsp *adsp,
			     const struct snd_compr_stream *stream,
			     struct snd_compr_caps *caps);

extern int wm_adsp_stream_alloc(struct wm_adsp *adsp,
				const struct snd_compr_params *params);
extern int wm_adsp_stream_alloc2(struct wm_adsp *adsp,
				const struct snd_compr_params *params);
extern int wm_adsp_stream_free(struct wm_adsp *adsp, int buffer);
extern int wm_adsp_stream_start(struct wm_adsp *adsp);
extern int wm_adsp_stream_start2(struct wm_adsp *adsp);

extern int wm_adsp_stream_handle_irq(struct wm_adsp *adsp, bool two_buf);
extern int wm_adsp_stream_read(struct wm_adsp *adsp, char __user *buf,
			       size_t count);
extern int wm_adsp_stream_read2(struct wm_adsp *adsp, char __user *buf,
			       size_t count);
extern int wm_adsp_stream_avail(const struct wm_adsp *adsp);

#endif
