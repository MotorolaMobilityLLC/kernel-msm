/*
 * wm_adsp.c  --  Wolfson ADSP support
 *
 * Copyright 2012 Wolfson Microelectronics plc
 *
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/list.h>
#include <linux/of.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>
#include <linux/debugfs.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include <linux/mfd/arizona/registers.h>

#include "arizona.h"
#include "wm_adsp.h"

#define adsp_crit(_dsp, fmt, ...) \
	dev_crit(_dsp->dev, "DSP%d: " fmt, _dsp->num, ##__VA_ARGS__)
#define adsp_err(_dsp, fmt, ...) \
	dev_err(_dsp->dev, "DSP%d: " fmt, _dsp->num, ##__VA_ARGS__)
#define adsp_warn(_dsp, fmt, ...) \
	dev_warn(_dsp->dev, "DSP%d: " fmt, _dsp->num, ##__VA_ARGS__)
#define adsp_info(_dsp, fmt, ...) \
	dev_info(_dsp->dev, "DSP%d: " fmt, _dsp->num, ##__VA_ARGS__)
#define adsp_dbg(_dsp, fmt, ...) \
	dev_dbg(_dsp->dev, "DSP%d: " fmt, _dsp->num, ##__VA_ARGS__)

#define ADSP1_CONTROL_1                   0x00
#define ADSP1_CONTROL_2                   0x02
#define ADSP1_CONTROL_3                   0x03
#define ADSP1_CONTROL_4                   0x04
#define ADSP1_CONTROL_5                   0x06
#define ADSP1_CONTROL_6                   0x07
#define ADSP1_CONTROL_7                   0x08
#define ADSP1_CONTROL_8                   0x09
#define ADSP1_CONTROL_9                   0x0A
#define ADSP1_CONTROL_10                  0x0B
#define ADSP1_CONTROL_11                  0x0C
#define ADSP1_CONTROL_12                  0x0D
#define ADSP1_CONTROL_13                  0x0F
#define ADSP1_CONTROL_14                  0x10
#define ADSP1_CONTROL_15                  0x11
#define ADSP1_CONTROL_16                  0x12
#define ADSP1_CONTROL_17                  0x13
#define ADSP1_CONTROL_18                  0x14
#define ADSP1_CONTROL_19                  0x16
#define ADSP1_CONTROL_20                  0x17
#define ADSP1_CONTROL_21                  0x18
#define ADSP1_CONTROL_22                  0x1A
#define ADSP1_CONTROL_23                  0x1B
#define ADSP1_CONTROL_24                  0x1C
#define ADSP1_CONTROL_25                  0x1E
#define ADSP1_CONTROL_26                  0x20
#define ADSP1_CONTROL_27                  0x21
#define ADSP1_CONTROL_28                  0x22
#define ADSP1_CONTROL_29                  0x23
#define ADSP1_CONTROL_30                  0x24
#define ADSP1_CONTROL_31                  0x26

/*
 * ADSP1 Control 19
 */
#define ADSP1_WDMA_BUFFER_LENGTH_MASK     0x00FF  /* DSP1_WDMA_BUFFER_LENGTH - [7:0] */
#define ADSP1_WDMA_BUFFER_LENGTH_SHIFT         0  /* DSP1_WDMA_BUFFER_LENGTH - [7:0] */
#define ADSP1_WDMA_BUFFER_LENGTH_WIDTH         8  /* DSP1_WDMA_BUFFER_LENGTH - [7:0] */


/*
 * ADSP1 Control 30
 */
#define ADSP1_DBG_CLK_ENA                 0x0008  /* DSP1_DBG_CLK_ENA */
#define ADSP1_DBG_CLK_ENA_MASK            0x0008  /* DSP1_DBG_CLK_ENA */
#define ADSP1_DBG_CLK_ENA_SHIFT                3  /* DSP1_DBG_CLK_ENA */
#define ADSP1_DBG_CLK_ENA_WIDTH                1  /* DSP1_DBG_CLK_ENA */
#define ADSP1_SYS_ENA                     0x0004  /* DSP1_SYS_ENA */
#define ADSP1_SYS_ENA_MASK                0x0004  /* DSP1_SYS_ENA */
#define ADSP1_SYS_ENA_SHIFT                    2  /* DSP1_SYS_ENA */
#define ADSP1_SYS_ENA_WIDTH                    1  /* DSP1_SYS_ENA */
#define ADSP1_CORE_ENA                    0x0002  /* DSP1_CORE_ENA */
#define ADSP1_CORE_ENA_MASK               0x0002  /* DSP1_CORE_ENA */
#define ADSP1_CORE_ENA_SHIFT                   1  /* DSP1_CORE_ENA */
#define ADSP1_CORE_ENA_WIDTH                   1  /* DSP1_CORE_ENA */
#define ADSP1_START                       0x0001  /* DSP1_START */
#define ADSP1_START_MASK                  0x0001  /* DSP1_START */
#define ADSP1_START_SHIFT                      0  /* DSP1_START */
#define ADSP1_START_WIDTH                      1  /* DSP1_START */

/*
 * ADSP1 Control 31
 */
#define ADSP1_CLK_SEL_MASK                0x0007  /* CLK_SEL_ENA */
#define ADSP1_CLK_SEL_SHIFT                    0  /* CLK_SEL_ENA */
#define ADSP1_CLK_SEL_WIDTH                    3  /* CLK_SEL_ENA */

#define ADSP2_CONTROL        0x0
#define ADSP2_CLOCKING       0x1
#define ADSP2V2_CLOCKING     0x2
#define ADSP2_STATUS1        0x4
#define ADSP2_WDMA_CONFIG_1   0x30
#define ADSP2_WDMA_CONFIG_2   0x31
#define ADSP2V2_WDMA_CONFIG_2 0x32
#define ADSP2_RDMA_CONFIG_1   0x34

#define ADSP2_SCRATCH0        0x40
#define ADSP2_SCRATCH1        0x41
#define ADSP2_SCRATCH2        0x42
#define ADSP2_SCRATCH3        0x43

#define ADSP2V2_SCRATCH0_1        0x40
#define ADSP2V2_SCRATCH2_3        0x42

/*
 * ADSP2 Control
 */

#define ADSP2_MEM_ENA                     0x0010  /* DSP1_MEM_ENA */
#define ADSP2_MEM_ENA_MASK                0x0010  /* DSP1_MEM_ENA */
#define ADSP2_MEM_ENA_SHIFT                    4  /* DSP1_MEM_ENA */
#define ADSP2_MEM_ENA_WIDTH                    1  /* DSP1_MEM_ENA */
#define ADSP2_SYS_ENA                     0x0004  /* DSP1_SYS_ENA */
#define ADSP2_SYS_ENA_MASK                0x0004  /* DSP1_SYS_ENA */
#define ADSP2_SYS_ENA_SHIFT                    2  /* DSP1_SYS_ENA */
#define ADSP2_SYS_ENA_WIDTH                    1  /* DSP1_SYS_ENA */
#define ADSP2_CORE_ENA                    0x0002  /* DSP1_CORE_ENA */
#define ADSP2_CORE_ENA_MASK               0x0002  /* DSP1_CORE_ENA */
#define ADSP2_CORE_ENA_SHIFT                   1  /* DSP1_CORE_ENA */
#define ADSP2_CORE_ENA_WIDTH                   1  /* DSP1_CORE_ENA */
#define ADSP2_START                       0x0001  /* DSP1_START */
#define ADSP2_START_MASK                  0x0001  /* DSP1_START */
#define ADSP2_START_SHIFT                      0  /* DSP1_START */
#define ADSP2_START_WIDTH                      1  /* DSP1_START */

/*
 * ADSP2 clocking
 */
#define ADSP2_CLK_SEL_MASK                0x0007  /* CLK_SEL_ENA */
#define ADSP2_CLK_SEL_SHIFT                    0  /* CLK_SEL_ENA */
#define ADSP2_CLK_SEL_WIDTH                    3  /* CLK_SEL_ENA */

/*
 * ADSP2V2 clocking
 */
#define ADSP2V2_CLK_SEL_MASK               0x70000 /* CLK_SEL_ENA */
#define ADSP2V2_CLK_SEL_SHIFT                   16  /* CLK_SEL_ENA */
#define ADSP2V2_CLK_SEL_WIDTH                    3  /* CLK_SEL_ENA */

#define ADSP2V2_RATE_MASK                   0x7800  /* DSP_RATE */
#define ADSP2V2_RATE_SHIFT                      11  /* DSP_RATE */
#define ADSP2V2_RATE_WIDTH                       4  /* DSP_RATE */

/*
 * ADSP2 Status 1
 */
#define ADSP2_RAM_RDY                     0x0001
#define ADSP2_RAM_RDY_MASK                0x0001
#define ADSP2_RAM_RDY_SHIFT                    0
#define ADSP2_RAM_RDY_WIDTH                    1

/*
 * ADSP2 Lock support
 */

#define ADSP2_LOCK_CODE_0                    0x5555
#define ADSP2_LOCK_CODE_1                    0xAAAA

#define ADSP2_WATCHDOG                       0x0A
#define ADSP2_BUS_ERR_ADDR                   0x52
#define ADSP2_REGION_LOCK_STATUS             0x64
#define ADSP2_LOCK_REGION_1_LOCK_REGION_0    0x66
#define ADSP2_LOCK_REGION_3_LOCK_REGION_2    0x68
#define ADSP2_LOCK_REGION_5_LOCK_REGION_4    0x6A
#define ADSP2_LOCK_REGION_7_LOCK_REGION_6    0x6C
#define ADSP2_LOCK_REGION_9_LOCK_REGION_8    0x6E
#define ADSP2_LOCK_REGION_CTRL               0x7A
#define ADSP2_PMEM_ERR_ADDR_XMEM_ERR_ADDR    0x7C

#define ADSP2_REGION_LOCK_ERR_MASK           0x8000
#define ADSP2_SLAVE_ERR_MASK                 0x4000
#define ADSP2_WDT_TIMEOUT_STS_MASK           0x2000
#define ADSP2_CTRL_ERR_PAUSE_ENA             0x0002
#define ADSP2_CTRL_ERR_EINT                  0x0001

#define ADSP2_BUS_ERR_ADDR_MASK              0x00FFFFFF
#define ADSP2_XMEM_ERR_ADDR_MASK             0x0000FFFF
#define ADSP2_PMEM_ERR_ADDR_MASK             0x7FFF0000
#define ADSP2_PMEM_ERR_ADDR_SHIFT            16
#define ADSP2_WDT_ENA_MASK                   0xFFFFFFFD

#define ADSP2_LOCK_REGION_SHIFT              16

struct wm_adsp_buf {
	struct list_head list;
	void *buf;
};

static struct wm_adsp_buf *wm_adsp_buf_alloc(const void *src, size_t len,
					     struct list_head *list)
{
	struct wm_adsp_buf *buf = kzalloc(sizeof(*buf), GFP_KERNEL);

	if (buf == NULL)
		return NULL;

	buf->buf = kmemdup(src, len, GFP_KERNEL | GFP_DMA);
	if (!buf->buf) {
		kfree(buf);
		return NULL;
	}

	if (list)
		list_add_tail(&buf->list, list);

	return buf;
}

static void wm_adsp_buf_free(struct list_head *list)
{
	while (!list_empty(list)) {
		struct wm_adsp_buf *buf = list_first_entry(list,
							   struct wm_adsp_buf,
							   list);
		list_del(&buf->list);
		kfree(buf->buf);
		kfree(buf);
	}
}

/* Must remain a power of two */
#define WM_ADSP_CAPTURE_BUFFER_SIZE      1048576

#define WM_ADSP_FW_MBC_VSS        0
#define WM_ADSP_FW_TX             1
#define WM_ADSP_FW_TX_SPK         2
#define WM_ADSP_FW_RX_ANC         3
#define WM_ADSP_FW_EZ2CONTROL     4
#define WM_ADSP_FW_TRACE          5
#define WM_ADSP_FW_EDAC           6
#define WM_ADSP_FW_EZ2LISTEN_SP   7
#define WM_ADSP_FW_EZ2LISTEN_HP   8
#define WM_ADSP_FW_EZ2HEAR_SP_TX  9
#define WM_ADSP_FW_EZ2HEAR_HS_TX  10
#define WM_ADSP_FW_EZ2HEAR_RX     11
#define WM_ADSP_FW_EZ2FACETALK_TX 12
#define WM_ADSP_FW_EZ2FACETALK_RX 13
#define WM_ADSP_FW_EZ2GROUPTALK_TX 14
#define WM_ADSP_FW_EZ2GROUPTALK_RX 15
#define WM_ADSP_FW_EZ2RECORD       16
#define WM_ADSP_FW_ASR_ASSIST     17
#define WM_ADSP_FW_MASTERHIFI     18
#define WM_ADSP_FW_SPEAKERPROTECT 19

#define WM_ADSP_NUM_FW            20

static const char *wm_adsp_fw_text[WM_ADSP_NUM_FW] = {
	[WM_ADSP_FW_MBC_VSS] =    "MBC/VSS",
	[WM_ADSP_FW_TX] =         "Tx",
	[WM_ADSP_FW_TX_SPK] =     "Tx Speaker",
	[WM_ADSP_FW_RX_ANC] =     "Rx ANC",
	[WM_ADSP_FW_EZ2CONTROL] = "Ez2Control",
	[WM_ADSP_FW_TRACE] =      "Trace",
	[WM_ADSP_FW_EDAC] =       "EDAC",
	[WM_ADSP_FW_EZ2LISTEN_SP] = "Ez2Listen SP",
	[WM_ADSP_FW_EZ2LISTEN_HP] = "Ez2Listen HP",
	[WM_ADSP_FW_EZ2HEAR_SP_TX] = "Ez2HearSP Tx",
	[WM_ADSP_FW_EZ2HEAR_HS_TX] = "Ez2HearHS Tx",
	[WM_ADSP_FW_EZ2HEAR_RX] = "Ez2Hear Rx",
	[WM_ADSP_FW_EZ2FACETALK_TX] = "Ez2FaceTalk Tx",
	[WM_ADSP_FW_EZ2FACETALK_RX] = "Ez2FaceTalk Rx",
	[WM_ADSP_FW_EZ2GROUPTALK_TX] = "Ez2GroupTalk Tx",
	[WM_ADSP_FW_EZ2GROUPTALK_RX] = "Ez2GroupTalk Rx",
	[WM_ADSP_FW_EZ2RECORD] = "Ez2Record",
	[WM_ADSP_FW_ASR_ASSIST] = "ASR Assist",
	[WM_ADSP_FW_MASTERHIFI] = "MasterHiFi",
	[WM_ADSP_FW_SPEAKERPROTECT] = "Speaker Protect",
};

struct wm_adsp_system_config_xm_hdr {
	__be32 sys_enable;
	__be32 fw_id;
	__be32 fw_rev;
	__be32 boot_status;
	__be32 watchdog;
	__be32 dma_buffer_size;
	__be32 rdma[6];
	__be32 wdma[8];
	__be32 build_job_name[3];
	__be32 build_job_number;
};

struct wm_adsp_alg_xm_struct {
	__be32 magic;
	__be32 smoothing;
	__be32 threshold;
	__be32 host_buf_ptr;
	__be32 start_seq;
	__be32 high_water_mark;
	__be32 low_water_mark;
	__be64 smoothed_power;
};

struct wm_adsp_host_buffer {
	__be32 X_buf_base;		/* XM base addr of first X area */
	__be32 X_buf_size;		/* Size of 1st X area in words */
	__be32 X_buf_base2;		/* XM base addr of 2nd X area */
	__be32 X_buf_brk;		/* Total X size in words */
	__be32 Y_buf_base;		/* YM base addr of Y area */
	__be32 wrap;			/* Total size X and Y in words */
	__be32 high_water_mark;		/* Point at which IRQ is asserted */
	__be32 irq_count;		/* bits 1-31 count IRQ assertions */
	__be32 irq_ack;			/* acked IRQ count, bit 0 enables IRQ */
	__be32 next_write_index;	/* word index of next write */
	__be32 next_read_index;		/* word index of next read */
	__be32 error;			/* error if any */
	__be32 oldest_block_index;	/* word index of oldest surviving */
	__be32 requested_rewind;	/* how many blocks rewind was done */
	__be32 reserved_space;		/* internal */
	__be32 min_free;		/* min free space since stream start */
	__be32 blocks_written[2];	/* total blocks written (64 bit) */
	__be32 words_written[2];	/* total words written (64 bit) */
};

#define WM_ADSP_DATA_WORD_SIZE         3
#define WM_ADSP_MAX_READ_SIZE          1024
#define WM_ADSP_ALG_XM_STRUCT_MAGIC    0x49aec7
#define WM_ADSP_ALG_XM2_STRUCT_MAGIC   0x2e07b0

#define WM_ADSP_DEFAULT_WATERMARK      DIV_ROUND_UP(2048, WM_ADSP_DATA_WORD_SIZE)

#define ADSP2_SYSTEM_CONFIG_XM_PTR \
	(offsetof(struct wmfw_adsp2_id_hdr, xm) / sizeof(__be32))

#define WM_ADSP_ALG_XM_PTR \
	(sizeof(struct wm_adsp_system_config_xm_hdr) / sizeof(__be32))

#define HOST_BUFFER_FIELD(field) \
	(offsetof(struct wm_adsp_host_buffer, field) / sizeof(__be32))

#define ALG_XM_FIELD(field) \
	(offsetof(struct wm_adsp_alg_xm_struct, field) / sizeof(__be32))

static struct wm_adsp_buffer_region_def ez2control_regions[] = {
	{
		.mem_type = WMFW_ADSP2_XM,
		.base_offset = HOST_BUFFER_FIELD(X_buf_base),
		.size_offset = HOST_BUFFER_FIELD(X_buf_size),
	},
	{
		.mem_type = WMFW_ADSP2_XM,
		.base_offset = HOST_BUFFER_FIELD(X_buf_base2),
		.size_offset = HOST_BUFFER_FIELD(X_buf_brk),
	},
	{
		.mem_type = WMFW_ADSP2_YM,
		.base_offset = HOST_BUFFER_FIELD(Y_buf_base),
		.size_offset = HOST_BUFFER_FIELD(wrap),
	},
};

static struct wm_adsp_fw_caps ez2control_caps[] = {
	{
		.id = SND_AUDIOCODEC_PCM,
		.desc = {
			.max_ch = 1,
			.sample_rates = { 16000 },
			.num_sample_rates = 1,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.num_host_regions = ARRAY_SIZE(ez2control_regions),
		.host_region_defs = ez2control_regions,
	},
};

static struct wm_adsp_buffer_region_def trace_regions[] = {
	{
		.mem_type = WMFW_ADSP2_XM,
		.base_offset = HOST_BUFFER_FIELD(X_buf_base),
		.size_offset = HOST_BUFFER_FIELD(X_buf_size),
	},
	{
		.mem_type = WMFW_ADSP2_XM,
		.base_offset = HOST_BUFFER_FIELD(X_buf_base2),
		.size_offset = HOST_BUFFER_FIELD(X_buf_brk),
	},
	{
		.mem_type = WMFW_ADSP2_YM,
		.base_offset = HOST_BUFFER_FIELD(Y_buf_base),
		.size_offset = HOST_BUFFER_FIELD(wrap),
	},
};

static struct wm_adsp_fw_caps trace_caps[] = {
	{
		.id = SND_AUDIOCODEC_PCM,
		.desc = {
			.max_ch = 8,
			.sample_rates = {
				4000,8000,11025,12000,16000,22050,
				24000,32000,44100,48000,64000,88200,
				96000,176400,192000
			},
			.num_sample_rates = 15,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.num_host_regions = ARRAY_SIZE(trace_regions),
		.host_region_defs = trace_regions,
	},
};

static struct wm_adsp_fw_defs wm_adsp_fw[WM_ADSP_NUM_FW] = {
	[WM_ADSP_FW_MBC_VSS] =    { .file = "mbc-vss" },
	[WM_ADSP_FW_TX] =         { .file = "tx" },
	[WM_ADSP_FW_TX_SPK] =     { .file = "tx-spk" },
	[WM_ADSP_FW_RX_ANC] =     { .file = "rx-anc" },
	[WM_ADSP_FW_EZ2CONTROL] = {
		.file = "ez2-control",
		.compr_direction = SND_COMPRESS_CAPTURE,
		.num_caps = ARRAY_SIZE(ez2control_caps),
		.caps = ez2control_caps,
	},
	[WM_ADSP_FW_TRACE] = {
		.file = "trace",
		.compr_direction = SND_COMPRESS_CAPTURE,
		.num_caps = ARRAY_SIZE(trace_caps),
		.caps = trace_caps,
	},
	[WM_ADSP_FW_EDAC] =     { .file = "edac" },
	[WM_ADSP_FW_EZ2LISTEN_SP] = { .file = "ez2listen-sp" },
	[WM_ADSP_FW_EZ2LISTEN_HP] = { .file = "ez2listen-hp" },
	[WM_ADSP_FW_EZ2HEAR_SP_TX] = { .file = "ez2hear-sp-tx" },
	[WM_ADSP_FW_EZ2HEAR_HS_TX] = { .file = "ez2hear-hs-tx" },
	[WM_ADSP_FW_EZ2HEAR_RX] = { .file = "ez2hear-rx" },
	[WM_ADSP_FW_EZ2FACETALK_TX] = { .file = "ez2facetalk-tx" },
	[WM_ADSP_FW_EZ2FACETALK_RX] = { .file = "ez2facetalk-rx" },
	[WM_ADSP_FW_EZ2GROUPTALK_TX] = { .file = "ez2grouptalk-tx" },
	[WM_ADSP_FW_EZ2GROUPTALK_RX] = { .file = "ez2grouptalk-rx" },
	[WM_ADSP_FW_EZ2RECORD] = { .file = "ez2record" },
	[WM_ADSP_FW_ASR_ASSIST] =     { .file = "asr-assist" },
	[WM_ADSP_FW_MASTERHIFI] =     { .file = "masterhifi" },
	[WM_ADSP_FW_SPEAKERPROTECT] = { .file = "speaker-protect" },
};

struct wm_coeff_ctl_ops {
	int (*xget)(struct snd_kcontrol *kcontrol,
		    struct snd_ctl_elem_value *ucontrol);
	int (*xput)(struct snd_kcontrol *kcontrol,
		    struct snd_ctl_elem_value *ucontrol);
	int (*xinfo)(struct snd_kcontrol *kcontrol,
		     struct snd_ctl_elem_info *uinfo);
};

struct wm_coeff_ctl {
	const char *name;
	const char *fw_name;
	struct wm_adsp_alg_region alg_region;
	struct wm_coeff_ctl_ops ops;
	struct wm_adsp *dsp;
	unsigned int enabled:1;
	struct list_head list;
	void *cache;
	unsigned int offset;
	size_t len;
	unsigned int set:1;
	struct snd_kcontrol *kcontrol;
	unsigned int flags;
	struct mutex lock;
};

#ifdef CONFIG_DEBUG_FS
static void wm_adsp_debugfs_save_wmfwname(struct wm_adsp *dsp, const char *s);
static void wm_adsp_debugfs_save_binname(struct wm_adsp *dsp, const char *s);
#else
static inline void wm_adsp_debugfs_save_wmfwname(struct wm_adsp *dsp,
						 const char *s)
{
}

static inline void wm_adsp_debugfs_save_binname(struct wm_adsp *dsp,
						const char *s)
{
}
#endif

static int wm_adsp_fw_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	struct wm_adsp *dsp = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = dsp[e->shift_l].fw;

	return 0;
}

static int wm_adsp_fw_put(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	struct wm_adsp *dsp = snd_soc_codec_get_drvdata(codec);

	if (ucontrol->value.enumerated.item[0] == dsp[e->shift_l].fw)
		return 0;

	if (ucontrol->value.enumerated.item[0] >= dsp[e->shift_l].num_firmwares)
		return -EINVAL;

	if (dsp[e->shift_l].running)
		return -EBUSY;

	dsp[e->shift_l].fw = ucontrol->value.enumerated.item[0];

	return 0;
}

static int wm_adsp2v2_rate_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	struct wm_adsp *dsps = snd_soc_codec_get_drvdata(codec);
	struct wm_adsp *dsp = &dsps[e->shift_l];
	unsigned int item;

	mutex_lock(&dsp->rate_lock);

	for (item = 0; item < e->items; item++) {
		if (e->values[item] == dsp->rate_cache) {
			ucontrol->value.enumerated.item[0] = item;
			mutex_unlock(&dsp->rate_lock);
			return 0;
		}
	}

	mutex_unlock(&dsp->rate_lock);

	return -EINVAL;
}

static int wm_adsp2v2_rate_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	struct wm_adsp *dsps = snd_soc_codec_get_drvdata(codec);
	struct wm_adsp *dsp = &dsps[e->shift_l];
	unsigned int item = ucontrol->value.enumerated.item[0];
	unsigned int val;
	int ret = 0;

	if (item >= e->items)
		return -EINVAL;

	mutex_lock(&dsp->rate_lock);

	if (e->values[item] != dsp->rate_cache) {
		val = e->values[item];
		dsp->rate_cache = val;

		if (dsp->running) {
			ret = dsp->rate_put_cb(dsp, ADSP2V2_RATE_MASK,
						val << ADSP2V2_RATE_SHIFT);
		}
	}

	mutex_unlock(&dsp->rate_lock);

	return ret;
}

static struct soc_enum wm_adsp_fw_enum[] = {
	SOC_ENUM_SINGLE(0, 0, ARRAY_SIZE(wm_adsp_fw_text), wm_adsp_fw_text),
	SOC_ENUM_SINGLE(0, 1, ARRAY_SIZE(wm_adsp_fw_text), wm_adsp_fw_text),
	SOC_ENUM_SINGLE(0, 2, ARRAY_SIZE(wm_adsp_fw_text), wm_adsp_fw_text),
	SOC_ENUM_SINGLE(0, 3, ARRAY_SIZE(wm_adsp_fw_text), wm_adsp_fw_text),
	SOC_ENUM_SINGLE(0, 4, ARRAY_SIZE(wm_adsp_fw_text), wm_adsp_fw_text),
	SOC_ENUM_SINGLE(0, 5, ARRAY_SIZE(wm_adsp_fw_text), wm_adsp_fw_text),
	SOC_ENUM_SINGLE(0, 6, ARRAY_SIZE(wm_adsp_fw_text), wm_adsp_fw_text),
};

const struct snd_kcontrol_new wm_adsp1_fw_controls[] = {
	SOC_ENUM_EXT("DSP1 Firmware", wm_adsp_fw_enum[0],
		     wm_adsp_fw_get, wm_adsp_fw_put),
	SOC_ENUM_EXT("DSP2 Firmware", wm_adsp_fw_enum[1],
		     wm_adsp_fw_get, wm_adsp_fw_put),
	SOC_ENUM_EXT("DSP3 Firmware", wm_adsp_fw_enum[2],
		     wm_adsp_fw_get, wm_adsp_fw_put),
};
EXPORT_SYMBOL_GPL(wm_adsp1_fw_controls);

#if IS_ENABLED(CONFIG_SND_SOC_ARIZONA)
static const struct soc_enum wm_adsp2_rate_enum[] = {
	SOC_VALUE_ENUM_SINGLE(ARIZONA_DSP1_CONTROL_1,
			      ARIZONA_DSP1_RATE_SHIFT, 0xf,
			      ARIZONA_RATE_ENUM_SIZE,
			      arizona_rate_text, arizona_rate_val),
	SOC_VALUE_ENUM_SINGLE(ARIZONA_DSP2_CONTROL_1,
			      ARIZONA_DSP1_RATE_SHIFT, 0xf,
			      ARIZONA_RATE_ENUM_SIZE,
			      arizona_rate_text, arizona_rate_val),
	SOC_VALUE_ENUM_SINGLE(ARIZONA_DSP3_CONTROL_1,
			      ARIZONA_DSP1_RATE_SHIFT, 0xf,
			      ARIZONA_RATE_ENUM_SIZE,
			      arizona_rate_text, arizona_rate_val),
	SOC_VALUE_ENUM_SINGLE(ARIZONA_DSP4_CONTROL_1,
			      ARIZONA_DSP1_RATE_SHIFT, 0xf,
			      ARIZONA_RATE_ENUM_SIZE,
			      arizona_rate_text, arizona_rate_val),
};

const struct snd_kcontrol_new wm_adsp2_fw_controls[] = {
	SOC_ENUM_EXT("DSP1 Firmware", wm_adsp_fw_enum[0],
		     wm_adsp_fw_get, wm_adsp_fw_put),
	SOC_ENUM("DSP1 Rate", wm_adsp2_rate_enum[0]),
	SOC_ENUM_EXT("DSP2 Firmware", wm_adsp_fw_enum[1],
		     wm_adsp_fw_get, wm_adsp_fw_put),
	SOC_ENUM("DSP2 Rate", wm_adsp2_rate_enum[1]),
	SOC_ENUM_EXT("DSP3 Firmware", wm_adsp_fw_enum[2],
		     wm_adsp_fw_get, wm_adsp_fw_put),
	SOC_ENUM("DSP3 Rate", wm_adsp2_rate_enum[2]),
	SOC_ENUM_EXT("DSP4 Firmware", wm_adsp_fw_enum[3],
		     wm_adsp_fw_get, wm_adsp_fw_put),
	SOC_ENUM("DSP4 Rate", wm_adsp2_rate_enum[3]),
};
EXPORT_SYMBOL_GPL(wm_adsp2_fw_controls);

static const struct soc_enum wm_adsp2v2_rate_enum[] = {
	SOC_VALUE_ENUM_SINGLE(0, 0, 0xf, ARIZONA_RATE_ENUM_SIZE,
			      arizona_rate_text, arizona_rate_val),
	SOC_VALUE_ENUM_SINGLE(0, 1, 0xf, ARIZONA_RATE_ENUM_SIZE,
			      arizona_rate_text, arizona_rate_val),
	SOC_VALUE_ENUM_SINGLE(0, 2, 0xf, ARIZONA_RATE_ENUM_SIZE,
			      arizona_rate_text, arizona_rate_val),
	SOC_VALUE_ENUM_SINGLE(0, 3, 0xf, ARIZONA_RATE_ENUM_SIZE,
			      arizona_rate_text, arizona_rate_val),
	SOC_VALUE_ENUM_SINGLE(0, 4, 0xf, ARIZONA_RATE_ENUM_SIZE,
			      arizona_rate_text, arizona_rate_val),
	SOC_VALUE_ENUM_SINGLE(0, 5, 0xf, ARIZONA_RATE_ENUM_SIZE,
			      arizona_rate_text, arizona_rate_val),
	SOC_VALUE_ENUM_SINGLE(0, 6, 0xf, ARIZONA_RATE_ENUM_SIZE,
			      arizona_rate_text, arizona_rate_val),
};

const struct snd_kcontrol_new wm_adsp2v2_fw_controls[] = {
	SOC_ENUM_EXT("DSP1 Firmware", wm_adsp_fw_enum[0],
		     wm_adsp_fw_get, wm_adsp_fw_put),
	SOC_ENUM_EXT("DSP1 Rate", wm_adsp2v2_rate_enum[0],
		     wm_adsp2v2_rate_get, wm_adsp2v2_rate_put),
	SOC_ENUM_EXT("DSP2 Firmware", wm_adsp_fw_enum[1],
		     wm_adsp_fw_get, wm_adsp_fw_put),
	SOC_ENUM_EXT("DSP2 Rate", wm_adsp2v2_rate_enum[1],
		     wm_adsp2v2_rate_get, wm_adsp2v2_rate_put),
	SOC_ENUM_EXT("DSP3 Firmware", wm_adsp_fw_enum[2],
		     wm_adsp_fw_get, wm_adsp_fw_put),
	SOC_ENUM_EXT("DSP3 Rate", wm_adsp2v2_rate_enum[2],
		     wm_adsp2v2_rate_get, wm_adsp2v2_rate_put),
	SOC_ENUM_EXT("DSP4 Firmware", wm_adsp_fw_enum[3],
		     wm_adsp_fw_get, wm_adsp_fw_put),
	SOC_ENUM_EXT("DSP4 Rate", wm_adsp2v2_rate_enum[3],
		     wm_adsp2v2_rate_get, wm_adsp2v2_rate_put),
	SOC_ENUM_EXT("DSP5 Firmware", wm_adsp_fw_enum[4],
		     wm_adsp_fw_get, wm_adsp_fw_put),
	SOC_ENUM_EXT("DSP5 Rate", wm_adsp2v2_rate_enum[4],
		     wm_adsp2v2_rate_get, wm_adsp2v2_rate_put),
	SOC_ENUM_EXT("DSP6 Firmware", wm_adsp_fw_enum[5],
		     wm_adsp_fw_get, wm_adsp_fw_put),
	SOC_ENUM_EXT("DSP6 Rate", wm_adsp2v2_rate_enum[5],
		     wm_adsp2v2_rate_get, wm_adsp2v2_rate_put),
	SOC_ENUM_EXT("DSP7 Firmware", wm_adsp_fw_enum[6],
		     wm_adsp_fw_get, wm_adsp_fw_put),
	SOC_ENUM_EXT("DSP7 Rate", wm_adsp2v2_rate_enum[6],
		     wm_adsp2v2_rate_get, wm_adsp2v2_rate_put),
};
EXPORT_SYMBOL_GPL(wm_adsp2v2_fw_controls);
#endif

static struct wm_adsp_region const *wm_adsp_find_region(struct wm_adsp *dsp,
							int type)
{
	int i;

	for (i = 0; i < dsp->num_mems; i++)
		if (dsp->mem[i].type == type)
			return &dsp->mem[i];

	return NULL;
}

static unsigned int wm_adsp_region_to_reg(struct wm_adsp_region const *mem,
					  unsigned int offset)
{
	if (WARN_ON(!mem))
		return offset;
	switch (mem->type) {
	case WMFW_ADSP1_PM:
		return mem->base + (offset * 3);
	case WMFW_ADSP1_DM:
		return mem->base + (offset * 2);
	case WMFW_ADSP2_XM:
		return mem->base + (offset * 2);
	case WMFW_ADSP2_YM:
		return mem->base + (offset * 2);
	case WMFW_ADSP1_ZM:
		return mem->base + (offset * 2);
	default:
		WARN(1, "Unknown memory region type");
		return offset;
	}
}

static int wm_coeff_info(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_info *uinfo)
{
	struct wm_coeff_ctl *ctl = (struct wm_coeff_ctl *)kcontrol->private_value;

	uinfo->type = SNDRV_CTL_ELEM_TYPE_BYTES;
	uinfo->count = ctl->len;
	return 0;
}

static int wm_coeff_write_control(struct wm_coeff_ctl *ctl,
				  const void *buf, size_t len)
{
	struct wm_adsp_alg_region *alg_region = &ctl->alg_region;
	const struct wm_adsp_region *mem;
	struct wm_adsp *dsp = ctl->dsp;
	void *scratch;
	int ret;
	unsigned int reg;

	mem = wm_adsp_find_region(dsp, alg_region->type);
	if (!mem) {
		adsp_err(dsp, "No base for region %x\n",
			 alg_region->type);
		return -EINVAL;
	}

	reg = ctl->alg_region.base + ctl->offset;
	reg = wm_adsp_region_to_reg(mem, reg);

	scratch = kmemdup(buf, ctl->len, GFP_KERNEL | GFP_DMA);
	if (!scratch)
		return -ENOMEM;

	ret = regmap_raw_write(dsp->regmap, reg, scratch,
			       ctl->len);
	if (ret) {
		adsp_err(dsp, "Failed to write %zu bytes to %x: %d\n",
			 ctl->len, reg, ret);
		kfree(scratch);
		return ret;
	}
	adsp_dbg(dsp, "Wrote %zu bytes to %x\n", ctl->len, reg);

	kfree(scratch);

	return 0;
}

static int wm_coeff_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct wm_coeff_ctl *ctl = (struct wm_coeff_ctl *)kcontrol->private_value;
	char *p = ucontrol->value.bytes.data;
	int ret = 0;

	if (ctl->flags && !(ctl->flags & WMFW_CTL_FLAG_WRITEABLE))
		return -EPERM;

	mutex_lock(&ctl->lock);

	memcpy(ctl->cache, p, ctl->len);

	ctl->set = 1;
	if (!ctl->enabled)
		goto out;

	ret = wm_coeff_write_control(ctl, p, ctl->len);

out:
	mutex_unlock(&ctl->lock);
	return ret;
}

static int wm_coeff_read_control(struct wm_coeff_ctl *ctl,
				 void *buf, size_t len)
{
	struct wm_adsp_alg_region *alg_region = &ctl->alg_region;
	const struct wm_adsp_region *mem;
	struct wm_adsp *dsp = ctl->dsp;
	void *scratch;
	int ret;
	unsigned int reg;

	mem = wm_adsp_find_region(dsp, alg_region->type);
	if (!mem) {
		adsp_err(dsp, "No base for region %x\n",
			 alg_region->type);
		return -EINVAL;
	}

	reg = ctl->alg_region.base + ctl->offset;
	reg = wm_adsp_region_to_reg(mem, reg);

	scratch = kmalloc(ctl->len, GFP_KERNEL | GFP_DMA);
	if (!scratch)
		return -ENOMEM;

	ret = regmap_raw_read(dsp->regmap, reg, scratch, ctl->len);
	if (ret) {
		adsp_err(dsp, "Failed to read %zu bytes from %x: %d\n",
			 ctl->len, reg, ret);
		kfree(scratch);
		return ret;
	}
	adsp_dbg(dsp, "Read %zu bytes from %x\n", ctl->len, reg);

	memcpy(buf, scratch, ctl->len);
	kfree(scratch);

	return 0;
}

static int wm_coeff_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct wm_coeff_ctl *ctl = (struct wm_coeff_ctl *)kcontrol->private_value;
	char *p = ucontrol->value.bytes.data;
	int ret = 0;

	if (ctl->flags && !(ctl->flags & WMFW_CTL_FLAG_READABLE))
		return -EPERM;

	mutex_lock(&ctl->lock);

	if (ctl->flags & WMFW_CTL_FLAG_VOLATILE) {
		if (ctl->enabled)
			ret = wm_coeff_read_control(ctl, p, ctl->len);
		else
			ret = -EPERM;
		goto out;
	}

	memcpy(p, ctl->cache, ctl->len);

out:
	mutex_unlock(&ctl->lock);
	return ret;
}

struct wmfw_ctl_work {
	struct wm_adsp *dsp;
	struct wm_coeff_ctl *ctl;
	struct work_struct work;
};

static int wmfw_add_ctl(struct wm_adsp *dsp, struct wm_coeff_ctl *ctl)
{
	struct snd_kcontrol_new *kcontrol;
	int ret;

	if (!ctl || !ctl->name)
		return -EINVAL;

	kcontrol = kzalloc(sizeof(*kcontrol), GFP_KERNEL);
	if (!kcontrol)
		return -ENOMEM;
	kcontrol->iface = SNDRV_CTL_ELEM_IFACE_MIXER;

	kcontrol->name = ctl->name;
	kcontrol->info = wm_coeff_info;
	kcontrol->get = wm_coeff_get;
	kcontrol->put = wm_coeff_put;
	kcontrol->private_value = (unsigned long)ctl;

	ret = snd_soc_add_card_controls(dsp->card,
					kcontrol, 1);
	if (ret < 0)
		goto err_kcontrol;

	kfree(kcontrol);

	ctl->kcontrol = snd_soc_card_get_kcontrol(dsp->card,
						  ctl->name);

	return 0;

err_kcontrol:
	kfree(kcontrol);
	return ret;
}

static int wm_coeff_init_control_caches(struct wm_adsp *dsp)
{
	struct wm_coeff_ctl *ctl;
	int ret = 0;

	list_for_each_entry(ctl, &dsp->ctl_list, list) {
		if (!ctl->enabled || (ctl->flags & WMFW_CTL_FLAG_VOLATILE))
			continue;

		mutex_lock(&ctl->lock);

		if (!ctl->set)
			ret = wm_coeff_read_control(ctl, ctl->cache, ctl->len);

		mutex_unlock(&ctl->lock);

		if (ret < 0)
			return ret;
	}

	return 0;
}

static int wm_coeff_sync_controls(struct wm_adsp *dsp)
{
	struct wm_coeff_ctl *ctl;
	int ret = 0;

	list_for_each_entry(ctl, &dsp->ctl_list, list) {
		if (!ctl->enabled || (ctl->flags & WMFW_CTL_FLAG_VOLATILE))
			continue;

		mutex_lock(&ctl->lock);

		if (ctl->set)
			ret = wm_coeff_write_control(ctl, ctl->cache, ctl->len);

		mutex_unlock(&ctl->lock);

		if (ret < 0)
			return ret;
	}

	return 0;
}

static void wm_adsp_ctl_work(struct work_struct *work)
{
	struct wmfw_ctl_work *ctl_work = container_of(work,
						      struct wmfw_ctl_work,
						      work);

	wmfw_add_ctl(ctl_work->dsp, ctl_work->ctl);
	kfree(ctl_work);
}

static int wm_adsp_create_ctl_blk(struct wm_adsp *dsp,
				  const struct wm_adsp_alg_region *alg_region,
				  unsigned int offset, unsigned int len,
				  const char *subname, unsigned int subname_len,
				  unsigned int flags, int block)
{
	struct wm_coeff_ctl *ctl;
	struct wmfw_ctl_work *ctl_work;
	char name[SNDRV_CTL_ELEM_ID_NAME_MAXLEN];
	char *region_name;
	int ret;

	if (flags & WMFW_CTL_FLAG_SYS)
		return 0;

	switch (alg_region->type) {
	case WMFW_ADSP1_PM:
		region_name = "PM";
		break;
	case WMFW_ADSP1_DM:
		region_name = "DM";
		break;
	case WMFW_ADSP2_XM:
		region_name = "XM";
		break;
	case WMFW_ADSP2_YM:
		region_name = "YM";
		break;
	case WMFW_ADSP1_ZM:
		region_name = "ZM";
		break;
	default:
		adsp_err(dsp, "Unknown region type: %d\n", alg_region->type);
		return -EINVAL;
	}

	switch (dsp->fw_ver) {
	case 0:
	case 1:
		snprintf(name, SNDRV_CTL_ELEM_ID_NAME_MAXLEN, "DSP%d %s %x:%d",
			 dsp->num, region_name, alg_region->alg, block);
		break;
	default:
		ret = snprintf(name, SNDRV_CTL_ELEM_ID_NAME_MAXLEN,
			       "DSP%d%c %.10s %x:%d",
			       dsp->num, *region_name, wm_adsp_fw_text[dsp->fw],
			       alg_region->alg, block);

		/* Truncate the subname from the start if it is too long */
		if (subname) {
			int avail = SNDRV_CTL_ELEM_ID_NAME_MAXLEN - ret - 2;
			int skip = 0;

			if (subname_len > avail)
				skip = subname_len - avail;

			snprintf(name + ret,
				 SNDRV_CTL_ELEM_ID_NAME_MAXLEN - ret, " %.*s",
				 subname_len - skip, subname + skip);
		}
		break;
	}

	list_for_each_entry(ctl, &dsp->ctl_list,
			    list) {
		if (!strcmp(ctl->name, name)) {
			if (!ctl->enabled)
				ctl->enabled = 1;
			return 0;
		}
	}

	ctl = kzalloc(sizeof(*ctl), GFP_KERNEL);
	if (!ctl)
		return -ENOMEM;
	ctl->fw_name = dsp->firmwares[dsp->fw].name;
	ctl->alg_region = *alg_region;
	ctl->name = kmemdup(name, strlen(name) + 1, GFP_KERNEL);
	if (!ctl->name) {
		ret = -ENOMEM;
		goto err_ctl;
	}
	ctl->enabled = 1;
	ctl->set = 0;
	ctl->ops.xget = wm_coeff_get;
	ctl->ops.xput = wm_coeff_put;
	ctl->dsp = dsp;

	ctl->flags = flags;
	ctl->offset = offset;
	if (len > 512) {
		adsp_warn(dsp, "Truncating control %s from %d\n",
			  ctl->name, len);
		len = 512;
	}
	ctl->len = len;
	ctl->cache = kzalloc(ctl->len, GFP_KERNEL);
	if (!ctl->cache) {
		ret = -ENOMEM;
		goto err_ctl_name;
	}
	mutex_init(&ctl->lock);

	mutex_lock(&dsp->ctl_lock);
	list_add(&ctl->list, &dsp->ctl_list);
	mutex_unlock(&dsp->ctl_lock);

	ctl_work = kzalloc(sizeof(*ctl_work), GFP_KERNEL);
	if (!ctl_work) {
		ret = -ENOMEM;
		goto err_ctl_cache;
	}

	ctl_work->dsp = dsp;
	ctl_work->ctl = ctl;
	INIT_WORK(&ctl_work->work, wm_adsp_ctl_work);
	schedule_work(&ctl_work->work);

	return 0;

err_ctl_cache:
	kfree(ctl->cache);
err_ctl_name:
	kfree(ctl->name);
err_ctl:
	kfree(ctl);

	return ret;
}

static int wm_adsp_create_control(struct wm_adsp *dsp,
				  const struct wm_adsp_alg_region *alg_region,
				  unsigned int offset, unsigned int len,
				  const char *subname, unsigned int subname_len,
				  unsigned int flags)
{
	unsigned int ctl_len;
	int block = 0;
	int ret;

	while (len) {
		ctl_len = len;
		if (ctl_len > 512)
			ctl_len = 512;

		ret = wm_adsp_create_ctl_blk(dsp, alg_region, offset,
					     ctl_len, subname, subname_len,
					     flags, block);
		if (ret < 0)
			return ret;

		offset += ctl_len / 4;
		len -= ctl_len;

		block++;
	}

	return 0;
}

struct wm_coeff_parsed_alg {
	int id;
	const u8 *name;
	int name_len;
	int ncoeff;
};

struct wm_coeff_parsed_coeff {
	int offset;
	int mem_type;
	const u8 *name;
	int name_len;
	int ctl_type;
	int flags;
	int len;
};

static int wm_coeff_parse_string(int bytes, const u8 **pos, const u8 **str)
{
	int length;

	switch (bytes) {
	case 1:
		length = **pos;
		break;
	case 2:
		length = le16_to_cpu(*((u16 *)*pos));
		break;
	default:
		return 0;
	}

	if (str)
		*str = *pos + bytes;

	*pos += ((length + bytes) + 3) & ~0x03;

	return length;
}

static int wm_coeff_parse_int(int bytes, const u8 **pos)
{
	int val = 0;

	switch (bytes) {
	case 2:
		val = le16_to_cpu(*((u16 *)*pos));
		break;
	case 4:
		val = le32_to_cpu(*((u32 *)*pos));
		break;
	default:
		break;
	}

	*pos += bytes;

	return val;
}

static inline void wm_coeff_parse_alg(struct wm_adsp *dsp, const u8 **data,
				      struct wm_coeff_parsed_alg *blk)
{
	const struct wmfw_adsp_alg_data *raw;

	switch (dsp->fw_ver) {
	case 0:
	case 1:
		raw = (const struct wmfw_adsp_alg_data *)*data;
		*data = raw->data;

		blk->id = le32_to_cpu(raw->id);
		blk->name = raw->name;
		blk->name_len = strlen(raw->name);
		blk->ncoeff = le32_to_cpu(raw->ncoeff);
		break;
	default:
		blk->id = wm_coeff_parse_int(sizeof(raw->id), data);
		blk->name_len = wm_coeff_parse_string(sizeof(u8), data,
						      &blk->name);
		/* Discard description we have no use for it in the driver */
		wm_coeff_parse_string(sizeof(u16), data, NULL);
		blk->ncoeff = wm_coeff_parse_int(sizeof(raw->ncoeff), data);
		break;
	}

	adsp_dbg(dsp, "Algorithm ID: %#x\n", blk->id);
	adsp_dbg(dsp, "Algorithm name: %.*s\n", blk->name_len, blk->name);
	adsp_dbg(dsp, "# of coefficient descriptors: %#x\n", blk->ncoeff);
}

static inline void wm_coeff_parse_coeff(struct wm_adsp *dsp, const u8 **data,
					struct wm_coeff_parsed_coeff *blk)
{
	const struct wmfw_adsp_coeff_data *raw;
	const u8 *tmp;
	int length;

	switch (dsp->fw_ver) {
	case 0:
	case 1:
		raw = (const struct wmfw_adsp_coeff_data *)*data;
		*data = *data + sizeof(raw->hdr) + le32_to_cpu(raw->hdr.size);

		blk->offset = le16_to_cpu(raw->hdr.offset);
		blk->mem_type = le16_to_cpu(raw->hdr.type);
		blk->name = raw->name;
		blk->name_len = strlen(raw->name);
		blk->ctl_type = le16_to_cpu(raw->ctl_type);
		blk->flags = le16_to_cpu(raw->flags);
		blk->len = le32_to_cpu(raw->len);
		break;
	default:
		tmp = *data;
		blk->offset = wm_coeff_parse_int(sizeof(raw->hdr.offset), &tmp);
		blk->mem_type = wm_coeff_parse_int(sizeof(raw->hdr.type), &tmp);
		length = wm_coeff_parse_int(sizeof(raw->hdr.size), &tmp);
		blk->name_len = wm_coeff_parse_string(sizeof(u8), &tmp,
						      &blk->name);
		/* Discard extended name we have no use for it in the driver */
		wm_coeff_parse_string(sizeof(u8), &tmp, NULL);
		/* Discard description we have no use for it in the driver */
		wm_coeff_parse_string(sizeof(u16), &tmp, NULL);
		blk->ctl_type = wm_coeff_parse_int(sizeof(raw->ctl_type), &tmp);
		blk->flags = wm_coeff_parse_int(sizeof(raw->flags), &tmp);
		blk->len = wm_coeff_parse_int(sizeof(raw->len), &tmp);

		*data = *data + sizeof(raw->hdr) + length;
		break;
	}

	adsp_dbg(dsp, "\tCoefficient type: %#x\n", blk->mem_type);
	adsp_dbg(dsp, "\tCoefficient offset: %#x\n", blk->offset);
	adsp_dbg(dsp, "\tCoefficient name: %.*s\n", blk->name_len, blk->name);
	adsp_dbg(dsp, "\tCoefficient flags: %#x\n", blk->flags);
	adsp_dbg(dsp, "\tALSA control type: %#x\n", blk->ctl_type);
	adsp_dbg(dsp, "\tALSA control len: %#x\n", blk->len);
}

static int wm_adsp_parse_coeff(struct wm_adsp *dsp,
			       const struct wmfw_region *region)
{
	struct wm_adsp_alg_region alg_region = {};
	struct wm_coeff_parsed_alg alg_blk;
	struct wm_coeff_parsed_coeff coeff_blk;
	const u8 *data = region->data;
	int i, ret;

	wm_coeff_parse_alg(dsp, &data, &alg_blk);
	for (i = 0; i < alg_blk.ncoeff; i++) {
		wm_coeff_parse_coeff(dsp, &data, &coeff_blk);

		switch (coeff_blk.ctl_type) {
		case SNDRV_CTL_ELEM_TYPE_BYTES:
			break;
		default:
			adsp_err(dsp, "Unknown control type: %d\n",
				 coeff_blk.ctl_type);
			return -EINVAL;
		}

		alg_region.type = coeff_blk.mem_type;
		alg_region.alg = alg_blk.id;

		ret = wm_adsp_create_control(dsp, &alg_region,
					     coeff_blk.offset,
					     coeff_blk.len,
					     coeff_blk.name,
					     coeff_blk.name_len,
					     coeff_blk.flags);
		if (ret < 0)
			adsp_err(dsp, "Failed to create control: %.*s, %d\n",
				 coeff_blk.name_len, coeff_blk.name, ret);
	}

	return 0;
}

static int wm_adsp_load(struct wm_adsp *dsp)
{
	LIST_HEAD(buf_list);
	const struct firmware *firmware;
	struct regmap *regmap = dsp->regmap;
	unsigned int pos = 0;
	const struct wmfw_header *header;
	const struct wmfw_adsp1_sizes *adsp1_sizes;
	const struct wmfw_adsp2_sizes *adsp2_sizes;
	const struct wmfw_footer *footer;
	const struct wmfw_region *region;
	const struct wm_adsp_region *mem;
	const char *region_name;
	char *file, *text;
	struct wm_adsp_buf *buf;
	unsigned int reg;
	int regions = 0;
	int ret, offset, type, sizes;

	file = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (file == NULL)
		return -ENOMEM;

	if (dsp->part_rev) {
		snprintf(file, PAGE_SIZE, "%s%c-dsp%d-%s.wmfw",
			 dsp->part, dsp->part_rev, dsp->num,
			 dsp->firmwares[dsp->fw].file);
		file[PAGE_SIZE - 1] = '\0';

		mutex_lock(dsp->fw_lock);
		ret = request_firmware(&firmware, file, dsp->dev);
		mutex_unlock(dsp->fw_lock);
	} else {
		snprintf(file, PAGE_SIZE, "%s-dsp%d-%s.wmfw", dsp->part,
			 dsp->num, dsp->firmwares[dsp->fw].file);
		file[PAGE_SIZE - 1] = '\0';

		mutex_lock(dsp->fw_lock);
		ret = request_firmware(&firmware, file, dsp->dev);
		mutex_unlock(dsp->fw_lock);
	}
	if (ret != 0) {
		adsp_err(dsp, "Failed to request: '%s'\n", file);
		goto out;
	}
	ret = -EINVAL;

	pos = sizeof(*header) + sizeof(*adsp1_sizes) + sizeof(*footer);
	if (pos >= firmware->size) {
		adsp_err(dsp, "%s: file too short, %zu bytes\n",
			 file, firmware->size);
		goto out_fw;
	}

	header = (void*)&firmware->data[0];

	if (memcmp(&header->magic[0], "WMFW", 4) != 0) {
		adsp_err(dsp, "%s: invalid magic\n", file);
		goto out_fw;
	}

	switch (header->ver) {
	case 0:
		adsp_warn(dsp, "%s: Depreciated file format %d\n",
			  file, header->ver);
		break;
	case 1:
	case 2:
		break;
	default:
		adsp_err(dsp, "%s: unknown file format %d\n",
			 file, header->ver);
		goto out_fw;
	}
	adsp_info(dsp, "Firmware version: %d\n", header->ver);
	dsp->fw_ver = header->ver;

	if (header->core != dsp->type) {
		adsp_err(dsp, "%s: invalid core %d != %d\n",
			 file, header->core, dsp->type);
		goto out_fw;
	}

	switch (dsp->type) {
	case WMFW_ADSP1:
		pos = sizeof(*header) + sizeof(*adsp1_sizes) + sizeof(*footer);
		adsp1_sizes = (void *)&(header[1]);
		footer = (void *)&(adsp1_sizes[1]);
		sizes = sizeof(*adsp1_sizes);

		adsp_dbg(dsp, "%s: %d DM, %d PM, %d ZM\n",
			 file, le32_to_cpu(adsp1_sizes->dm),
			 le32_to_cpu(adsp1_sizes->pm),
			 le32_to_cpu(adsp1_sizes->zm));
		break;

	case WMFW_ADSP2:
		pos = sizeof(*header) + sizeof(*adsp2_sizes) + sizeof(*footer);
		adsp2_sizes = (void *)&(header[1]);
		footer = (void *)&(adsp2_sizes[1]);
		sizes = sizeof(*adsp2_sizes);

		adsp_dbg(dsp, "%s: %d XM, %d YM %d PM, %d ZM\n",
			 file, le32_to_cpu(adsp2_sizes->xm),
			 le32_to_cpu(adsp2_sizes->ym),
			 le32_to_cpu(adsp2_sizes->pm),
			 le32_to_cpu(adsp2_sizes->zm));
		break;

	default:
		WARN(1, "Unknown DSP type");
		goto out_fw;
	}

	if (le32_to_cpu(header->len) != sizeof(*header) +
	    sizes + sizeof(*footer)) {
		adsp_err(dsp, "%s: unexpected header length %d\n",
			 file, le32_to_cpu(header->len));
		goto out_fw;
	}

	adsp_dbg(dsp, "%s: timestamp %llu\n", file,
		 le64_to_cpu(footer->timestamp));

	while (pos < firmware->size &&
	       pos - firmware->size > sizeof(*region)) {
		region = (void *)&(firmware->data[pos]);
		region_name = "Unknown";
		reg = 0;
		text = NULL;
		offset = le32_to_cpu(region->offset) & 0xffffff;
		type = be32_to_cpu(region->type) & 0xff;
		mem = wm_adsp_find_region(dsp, type);

		switch (type) {
		case WMFW_NAME_TEXT:
			region_name = "Firmware name";
			text = kzalloc(le32_to_cpu(region->len) + 1,
				       GFP_KERNEL);
			break;
		case WMFW_ALGORITHM_DATA:
			region_name = "Algorithm";
			ret = wm_adsp_parse_coeff(dsp, region);
			if (ret != 0)
				goto out_fw;
			break;
		case WMFW_INFO_TEXT:
			region_name = "Information";
			text = kzalloc(le32_to_cpu(region->len) + 1,
				       GFP_KERNEL);
			break;
		case WMFW_ABSOLUTE:
			region_name = "Absolute";
			reg = offset;
			break;
		case WMFW_ADSP1_PM:
			region_name = "PM";
			reg = wm_adsp_region_to_reg(mem, offset);
			break;
		case WMFW_ADSP1_DM:
			region_name = "DM";
			reg = wm_adsp_region_to_reg(mem, offset);
			break;
		case WMFW_ADSP2_XM:
			region_name = "XM";
			reg = wm_adsp_region_to_reg(mem, offset);
			break;
		case WMFW_ADSP2_YM:
			region_name = "YM";
			reg = wm_adsp_region_to_reg(mem, offset);
			break;
		case WMFW_ADSP1_ZM:
			region_name = "ZM";
			reg = wm_adsp_region_to_reg(mem, offset);
			break;
		default:
			adsp_warn(dsp,
				  "%s.%d: Unknown region type %x at %d(%x)\n",
				  file, regions, type, pos, pos);
			break;
		}

		adsp_dbg(dsp, "%s.%d: %d bytes at %d in %s\n", file,
			 regions, le32_to_cpu(region->len), offset,
			 region_name);

		if (text) {
			memcpy(text, region->data, le32_to_cpu(region->len));
			adsp_info(dsp, "%s: %s\n", file, text);
			kfree(text);
		}

		if (reg) {
			size_t to_write = PAGE_SIZE;
			size_t remain = le32_to_cpu(region->len);
			const u8 *data = region->data;

			while (remain > 0) {
				if (remain < PAGE_SIZE)
					to_write = remain;

				buf = wm_adsp_buf_alloc(data,
							to_write,
							&buf_list);
				if (!buf) {
					adsp_err(dsp, "Out of memory\n");
					ret = -ENOMEM;
					goto out_buf;
				}

				ret = regmap_raw_write_async(regmap, reg,
							     buf->buf,
							     to_write);
				if (ret != 0) {
					adsp_err(dsp,
						"%s.%d: Failed to write %zd bytes at %d in %s: %d\n",
						file, regions,
						to_write, offset,
						region_name, ret);
					goto out_buf;
				}

				data += to_write;
				reg += to_write / 2;
				remain -= to_write;
			}
		}

		pos += le32_to_cpu(region->len) + sizeof(*region);
		regions++;
	}

	ret = regmap_async_complete(regmap);
	if (ret != 0) {
		adsp_err(dsp, "Failed to complete async write: %d\n", ret);
		goto out_buf;
	}

	if (pos > firmware->size)
		adsp_warn(dsp, "%s.%d: %zu bytes at end of file\n",
			  file, regions, pos - firmware->size);

	wm_adsp_debugfs_save_wmfwname(dsp, file);

out_buf:
	wm_adsp_buf_free(&buf_list);
out_fw:
	release_firmware(firmware);
out:
	kfree(file);

	return ret;
}

static void wm_adsp_ctl_fixup_base(struct wm_adsp *dsp,
				  const struct wm_adsp_alg_region *alg_region)
{
	struct wm_coeff_ctl *ctl;

	list_for_each_entry(ctl, &dsp->ctl_list, list) {
		if (ctl->fw_name == dsp->firmwares[dsp->fw].name &&
		    alg_region->alg == ctl->alg_region.alg &&
		    alg_region->type == ctl->alg_region.type) {
			ctl->alg_region.base = alg_region->base;
		}
	}
}

static void *wm_adsp_read_algs(struct wm_adsp *dsp, size_t n_algs,
			       unsigned int pos, unsigned int len)
{
	void *alg;
	int ret;
	__be32 val;

	if (n_algs == 0) {
		adsp_err(dsp, "No algorithms\n");
		return ERR_PTR(-EINVAL);
	}

	if (n_algs > 1024) {
		adsp_err(dsp, "Algorithm count %zx excessive\n", n_algs);
		return ERR_PTR(-EINVAL);
	}

	/* Read the terminator first to validate the length */
	ret = regmap_raw_read(dsp->regmap, pos + len, &val, sizeof(val));
	if (ret != 0) {
		adsp_err(dsp, "Failed to read algorithm list end: %d\n",
			ret);
		return ERR_PTR(ret);
	}

	if (be32_to_cpu(val) != 0xbedead)
		adsp_warn(dsp, "Algorithm list end %x 0x%x != 0xbeadead\n",
			  pos + len, be32_to_cpu(val));

	alg = kzalloc(len * 2, GFP_KERNEL | GFP_DMA);
	if (!alg)
		return ERR_PTR(-ENOMEM);

	ret = regmap_raw_read(dsp->regmap, pos, alg, len * 2);
	if (ret != 0) {
		adsp_err(dsp, "Failed to read algorithm list: %d\n",
			 ret);
		kfree(alg);
		return ERR_PTR(ret);
	}

	return alg;
}

static struct wm_adsp_alg_region *wm_adsp_create_region(struct wm_adsp *dsp,
							int type, __be32 id,
							__be32 base)
{
	struct wm_adsp_alg_region *alg_region;

	alg_region = kzalloc(sizeof(*alg_region), GFP_KERNEL);
	if (!alg_region)
		return ERR_PTR(-ENOMEM);

	alg_region->type = type;
	alg_region->alg = be32_to_cpu(id);
	alg_region->base = be32_to_cpu(base);

	list_add_tail(&alg_region->list, &dsp->alg_regions);

	if (dsp->fw_ver > 0)
		wm_adsp_ctl_fixup_base(dsp, alg_region);

	return alg_region;
}

static int wm_adsp1_setup_algs(struct wm_adsp *dsp)
{
	struct wmfw_adsp1_id_hdr adsp1_id;
	struct wmfw_adsp1_alg_hdr *adsp1_alg;
	struct wm_adsp_alg_region *alg_region;
	const struct wm_adsp_region *mem;
	unsigned int pos, len;
	size_t n_algs;
	int i, ret;

	mem = wm_adsp_find_region(dsp, WMFW_ADSP1_DM);
	if (WARN_ON(!mem))
		return -EINVAL;

	ret = regmap_raw_read(dsp->regmap, mem->base, &adsp1_id,
			      sizeof(adsp1_id));
	if (ret != 0) {
		adsp_err(dsp, "Failed to read algorithm info: %d\n",
			 ret);
		return ret;
	}

	n_algs = be32_to_cpu(adsp1_id.n_algs);
	dsp->fw_id = be32_to_cpu(adsp1_id.fw.id);
	dsp->fw_id_version = be32_to_cpu(adsp1_id.fw.ver);
	adsp_info(dsp, "Firmware: %x v%d.%d.%d, %zu algorithms\n",
		  dsp->fw_id,
		  (dsp->fw_id_version & 0xff0000) >> 16,
		  (dsp->fw_id_version & 0xff00) >> 8,
		  dsp->fw_id_version & 0xff,
		  n_algs);

	alg_region = wm_adsp_create_region(dsp, WMFW_ADSP1_ZM,
					   adsp1_id.fw.id, adsp1_id.zm);
	if (IS_ERR(alg_region))
		return PTR_ERR(alg_region);

	alg_region = wm_adsp_create_region(dsp, WMFW_ADSP1_DM,
					   adsp1_id.fw.id, adsp1_id.dm);
	if (IS_ERR(alg_region))
		return PTR_ERR(alg_region);

	pos = sizeof(adsp1_id) / 2;
	len = (sizeof(*adsp1_alg) * n_algs) / 2;

	adsp1_alg = wm_adsp_read_algs(dsp, n_algs, mem->base + pos, len);
	if (IS_ERR(adsp1_alg))
		return PTR_ERR(adsp1_alg);

	for (i = 0; i < n_algs; i++) {
		adsp_info(dsp, "%d: ID %x v%d.%d.%d DM@%x ZM@%x\n",
			  i, be32_to_cpu(adsp1_alg[i].alg.id),
			  (be32_to_cpu(adsp1_alg[i].alg.ver) & 0xff0000) >> 16,
			  (be32_to_cpu(adsp1_alg[i].alg.ver) & 0xff00) >> 8,
			  be32_to_cpu(adsp1_alg[i].alg.ver) & 0xff,
			  be32_to_cpu(adsp1_alg[i].dm),
			  be32_to_cpu(adsp1_alg[i].zm));

		alg_region = wm_adsp_create_region(dsp, WMFW_ADSP1_DM,
						   adsp1_alg[i].alg.id,
						   adsp1_alg[i].dm);
		if (IS_ERR(alg_region)) {
			ret = PTR_ERR(alg_region);
			goto out;
		}

		if (dsp->fw_ver == 0) {
			if (i + 1 < n_algs) {
				len = be32_to_cpu(adsp1_alg[i + 1].dm);
				len -= be32_to_cpu(adsp1_alg[i].dm);
				len *= 4;
				wm_adsp_create_control(dsp, alg_region, 0,
						       len, NULL, 0, 0);
			} else {
				adsp_warn(dsp,
					  "Missing length info for region DM with ID %x\n",
					  be32_to_cpu(adsp1_alg[i].alg.id));
			}
		}

		alg_region = wm_adsp_create_region(dsp, WMFW_ADSP1_ZM,
						   adsp1_alg[i].alg.id,
						   adsp1_alg[i].zm);
		if (IS_ERR(alg_region)) {
			ret = PTR_ERR(alg_region);
			goto out;
		}

		if (dsp->fw_ver == 0) {
			if (i + 1 < n_algs) {
				len = be32_to_cpu(adsp1_alg[i + 1].zm);
				len -= be32_to_cpu(adsp1_alg[i].zm);
				len *= 4;
				wm_adsp_create_control(dsp, alg_region, 0,
						       len, NULL, 0, 0);
			} else {
				adsp_warn(dsp,
					  "Missing length info for region ZM with ID %x\n",
					  be32_to_cpu(adsp1_alg[i].alg.id));
			}
		}
	}

out:
	kfree(adsp1_alg);
	return ret;
}

static int wm_adsp2_setup_algs(struct wm_adsp *dsp)
{
	struct wmfw_adsp2_id_hdr adsp2_id;
	struct wmfw_adsp2_alg_hdr *adsp2_alg;
	struct wm_adsp_alg_region *alg_region;
	const struct wm_adsp_region *mem;
	unsigned int pos, len;
	size_t n_algs;
	int i, ret;

	mem = wm_adsp_find_region(dsp, WMFW_ADSP2_XM);
	if (WARN_ON(!mem))
		return -EINVAL;

	ret = regmap_raw_read(dsp->regmap, mem->base, &adsp2_id,
			      sizeof(adsp2_id));
	if (ret != 0) {
		adsp_err(dsp, "Failed to read algorithm info: %d\n",
			 ret);
		return ret;
	}

	n_algs = be32_to_cpu(adsp2_id.n_algs);
	dsp->fw_id = be32_to_cpu(adsp2_id.fw.id);
	dsp->fw_id_version = be32_to_cpu(adsp2_id.fw.ver);
	adsp_info(dsp, "Firmware: %x v%d.%d.%d, %zu algorithms\n",
		  dsp->fw_id,
		  (dsp->fw_id_version & 0xff0000) >> 16,
		  (dsp->fw_id_version & 0xff00) >> 8,
		  dsp->fw_id_version & 0xff,
		  n_algs);

	alg_region = wm_adsp_create_region(dsp, WMFW_ADSP2_XM,
					   adsp2_id.fw.id, adsp2_id.xm);
	if (IS_ERR(alg_region))
		return PTR_ERR(alg_region);

	alg_region = wm_adsp_create_region(dsp, WMFW_ADSP2_YM,
					   adsp2_id.fw.id, adsp2_id.ym);
	if (IS_ERR(alg_region))
		return PTR_ERR(alg_region);

	alg_region = wm_adsp_create_region(dsp, WMFW_ADSP2_ZM,
					   adsp2_id.fw.id, adsp2_id.zm);
	if (IS_ERR(alg_region))
		return PTR_ERR(alg_region);

	pos = sizeof(adsp2_id) / 2;
	len = (sizeof(*adsp2_alg) * n_algs) / 2;

	adsp2_alg = wm_adsp_read_algs(dsp, n_algs, mem->base + pos, len);
	if (IS_ERR(adsp2_alg))
		return PTR_ERR(adsp2_alg);

	for (i = 0; i < n_algs; i++) {
		adsp_info(dsp,
			  "%d: ID %x v%d.%d.%d XM@%x YM@%x ZM@%x\n",
			  i, be32_to_cpu(adsp2_alg[i].alg.id),
			  (be32_to_cpu(adsp2_alg[i].alg.ver) & 0xff0000) >> 16,
			  (be32_to_cpu(adsp2_alg[i].alg.ver) & 0xff00) >> 8,
			  be32_to_cpu(adsp2_alg[i].alg.ver) & 0xff,
			  be32_to_cpu(adsp2_alg[i].xm),
			  be32_to_cpu(adsp2_alg[i].ym),
			  be32_to_cpu(adsp2_alg[i].zm));

		alg_region = wm_adsp_create_region(dsp, WMFW_ADSP2_XM,
						   adsp2_alg[i].alg.id,
						   adsp2_alg[i].xm);
		if (IS_ERR(alg_region)) {
			ret = PTR_ERR(alg_region);
			goto out;
		}

		if (dsp->fw_ver == 0) {
			if (i + 1 < n_algs) {
				len = be32_to_cpu(adsp2_alg[i + 1].xm);
				len -= be32_to_cpu(adsp2_alg[i].xm);
				len *= 4;
				wm_adsp_create_control(dsp, alg_region, 0,
						       len, NULL, 0, 0);
			} else {
				adsp_warn(dsp,
					  "Missing length info for region XM with ID %x\n",
					  be32_to_cpu(adsp2_alg[i].alg.id));
			}
		}

		alg_region = wm_adsp_create_region(dsp, WMFW_ADSP2_YM,
						   adsp2_alg[i].alg.id,
						   adsp2_alg[i].ym);
		if (IS_ERR(alg_region)) {
			ret = PTR_ERR(alg_region);
			goto out;
		}

		if (dsp->fw_ver == 0) {
			if (i + 1 < n_algs) {
				len = be32_to_cpu(adsp2_alg[i + 1].ym);
				len -= be32_to_cpu(adsp2_alg[i].ym);
				len *= 4;
				wm_adsp_create_control(dsp, alg_region, 0,
						       len, NULL, 0, 0);
			} else {
				adsp_warn(dsp,
					  "Missing length info for region YM with ID %x\n",
					  be32_to_cpu(adsp2_alg[i].alg.id));
			}
		}

		alg_region = wm_adsp_create_region(dsp, WMFW_ADSP2_ZM,
						   adsp2_alg[i].alg.id,
						   adsp2_alg[i].zm);
		if (IS_ERR(alg_region)) {
			ret = PTR_ERR(alg_region);
			goto out;
		}

		if (dsp->fw_ver == 0) {
			if (i + 1 < n_algs) {
				len = be32_to_cpu(adsp2_alg[i + 1].zm);
				len -= be32_to_cpu(adsp2_alg[i].zm);
				len *= 4;
				wm_adsp_create_control(dsp, alg_region, 0,
						       len, NULL, 0, 0);
			} else {
				adsp_warn(dsp,
					  "Missing length info for region ZM with ID %x\n",
					  be32_to_cpu(adsp2_alg[i].alg.id));
			}
		}
	}

out:
	kfree(adsp2_alg);
	return ret;
}

static int wm_adsp_load_coeff(struct wm_adsp *dsp)
{
	LIST_HEAD(buf_list);
	struct regmap *regmap = dsp->regmap;
	struct wmfw_coeff_hdr *hdr;
	struct wmfw_coeff_item *blk;
	const struct firmware *firmware;
	const struct wm_adsp_region *mem;
	struct wm_adsp_alg_region *alg_region;
	const char *region_name;
	int ret = 0;
	int err, pos, blocks, type, offset, reg;
	char *file;
	struct wm_adsp_buf *buf;

	if (dsp->firmwares[dsp->fw].binfile &&
	    !(strcmp(dsp->firmwares[dsp->fw].binfile, "None")))
		return 0;

	file = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (file == NULL)
		return -ENOMEM;

	if (dsp->firmwares[dsp->fw].binfile)
		snprintf(file, PAGE_SIZE, "%s-dsp%d-%s.bin", dsp->part,
			 dsp->num, dsp->firmwares[dsp->fw].binfile);
	else
		snprintf(file, PAGE_SIZE, "%s-dsp%d-%s.bin", dsp->part,
			 dsp->num, dsp->firmwares[dsp->fw].file);
	file[PAGE_SIZE - 1] = '\0';

	mutex_lock(dsp->fw_lock);
	ret = request_firmware(&firmware, file, dsp->dev);
	mutex_unlock(dsp->fw_lock);
	if (ret != 0) {
		adsp_warn(dsp, "Failed to request '%s'\n", file);
		ret = 0;
		goto out;
	}
	ret = -EINVAL;

	if (sizeof(*hdr) >= firmware->size) {
		adsp_err(dsp, "%s: file too short, %zu bytes\n",
			file, firmware->size);
		goto out_fw;
	}

	hdr = (void*)&firmware->data[0];
	if (memcmp(hdr->magic, "WMDR", 4) != 0) {
		adsp_err(dsp, "%s: invalid magic\n", file);
		goto out_fw;
	}

	switch (be32_to_cpu(hdr->rev) & 0xff) {
	case 1:
		break;
	default:
		adsp_err(dsp, "%s: Unsupported coefficient file format %d\n",
			 file, be32_to_cpu(hdr->rev) & 0xff);
		ret = -EINVAL;
		goto out_fw;
	}

	adsp_dbg(dsp, "%s: v%d.%d.%d\n", file,
		(le32_to_cpu(hdr->ver) >> 16) & 0xff,
		(le32_to_cpu(hdr->ver) >>  8) & 0xff,
		le32_to_cpu(hdr->ver) & 0xff);

	pos = le32_to_cpu(hdr->len);

	blocks = 0;
	while (pos < firmware->size &&
	       pos - firmware->size > sizeof(*blk)) {
		blk = (void*)(&firmware->data[pos]);

		type = le16_to_cpu(blk->type);
		offset = le16_to_cpu(blk->offset);

		adsp_dbg(dsp, "%s.%d: %x v%d.%d.%d\n",
			 file, blocks, le32_to_cpu(blk->id),
			 (le32_to_cpu(blk->ver) >> 16) & 0xff,
			 (le32_to_cpu(blk->ver) >>  8) & 0xff,
			 le32_to_cpu(blk->ver) & 0xff);
		adsp_dbg(dsp, "%s.%d: %d bytes at 0x%x in %x\n",
			 file, blocks, le32_to_cpu(blk->len), offset, type);

		reg = 0;
		region_name = "Unknown";
		switch (type) {
		case (WMFW_NAME_TEXT << 8):
		case (WMFW_INFO_TEXT << 8):
			break;
		case (WMFW_ABSOLUTE << 8):
			/*
			 * Old files may use this for global
			 * coefficients.
			 */
			if (le32_to_cpu(blk->id) == dsp->fw_id &&
			    offset == 0) {
				region_name = "global coefficients";
				mem = wm_adsp_find_region(dsp, type);
				if (!mem) {
					adsp_err(dsp, "No ZM\n");
					break;
				}
				reg = wm_adsp_region_to_reg(mem, 0);

			} else {
				region_name = "register";
				reg = offset;
			}
			break;

		case WMFW_ADSP1_DM:
		case WMFW_ADSP1_ZM:
		case WMFW_ADSP2_XM:
		case WMFW_ADSP2_YM:
			adsp_dbg(dsp, "%s.%d: %d bytes in %x for %x\n",
				 file, blocks, le32_to_cpu(blk->len),
				 type, le32_to_cpu(blk->id));

			mem = wm_adsp_find_region(dsp, type);
			if (!mem) {
				adsp_err(dsp, "No base for region %x\n", type);
				break;
			}

			reg = 0;
			list_for_each_entry(alg_region,
					    &dsp->alg_regions, list) {
				if (le32_to_cpu(blk->id) == alg_region->alg &&
				    type == alg_region->type) {
					reg = alg_region->base;
					reg = wm_adsp_region_to_reg(mem,
								    reg);
					reg += offset;
					break;
				}
			}

			if (reg == 0)
				adsp_err(dsp, "No %x for algorithm %x\n",
					 type, le32_to_cpu(blk->id));
			break;

		default:
			adsp_err(dsp, "%s.%d: Unknown region type %x at %d\n",
				 file, blocks, type, pos);
			break;
		}

		if (reg) {
			buf = wm_adsp_buf_alloc(blk->data,
						le32_to_cpu(blk->len),
						&buf_list);
			if (!buf) {
				adsp_err(dsp, "Out of memory\n");
				ret = -ENOMEM;
				goto out_async;
			}

			adsp_dbg(dsp, "%s.%d: Writing %d bytes at %x\n",
				 file, blocks, le32_to_cpu(blk->len),
				 reg);
			ret = regmap_raw_write_async(regmap, reg, buf->buf,
						     le32_to_cpu(blk->len));
			if (ret != 0) {
				adsp_err(dsp,
					"%s.%d: Failed to write to %x in %s: %d\n",
					file, blocks, reg, region_name, ret);
			}
		}

		pos += (le32_to_cpu(blk->len) + sizeof(*blk) + 3) & ~0x03;
		blocks++;
	}

	if (pos > firmware->size)
		adsp_warn(dsp, "%s.%d: %zu bytes at end of file\n",
			  file, blocks, pos - firmware->size);

	wm_adsp_debugfs_save_binname(dsp, file);

out_async:
	err = regmap_async_complete(regmap);
	if (err != 0) {
		adsp_err(dsp, "Failed to complete async write: %d\n", err);
		if (!ret)
			ret = err;
	}
out_fw:
	regmap_async_complete(regmap);
	release_firmware(firmware);
	wm_adsp_buf_free(&buf_list);
out:
	kfree(file);
	return ret;
}

int wm_adsp1_init(struct wm_adsp *dsp)
{
	INIT_LIST_HEAD(&dsp->alg_regions);

	return 0;
}
EXPORT_SYMBOL_GPL(wm_adsp1_init);

static int wm_adsp_get_features(struct wm_adsp *dsp)
{
	memset(&dsp->fw_features, 0, sizeof(dsp->fw_features));

	switch (dsp->fw_id) {
	case 0x4000d:
	case 0x40036:
	case 0x5f003:
	case 0x6000d:
	case 0x60037:
	case 0x7000d:
	case 0x70036:
	case 0x8000d:
	case 0x80053:
		dsp->fw_features.ez2control_trigger = true;
		break;
	case 0x40019:
	case 0x4001f:
	case 0x5001f:
	case 0x7001f:
		dsp->fw_features.shutdown = true;
	default:
		break;
	}

	return 0;
}


int wm_adsp1_event(struct snd_soc_dapm_widget *w,
		   struct snd_kcontrol *kcontrol,
		   int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct wm_adsp *dsps = snd_soc_codec_get_drvdata(codec);
	struct wm_adsp *dsp = &dsps[w->shift];
	struct wm_adsp_alg_region *alg_region;
	struct wm_coeff_ctl *ctl;
	int ret;
	unsigned int val;

	dsp->card = codec->component.card;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		regmap_update_bits(dsp->regmap, dsp->base + ADSP1_CONTROL_30,
				   ADSP1_SYS_ENA, ADSP1_SYS_ENA);

		/*
		 * For simplicity set the DSP clock rate to be the
		 * SYSCLK rate rather than making it configurable.
		 */
		if(dsp->sysclk_reg) {
			ret = regmap_read(dsp->regmap, dsp->sysclk_reg, &val);
			if (ret != 0) {
				adsp_err(dsp, "Failed to read SYSCLK state: %d\n",
				ret);
				return ret;
			}

			val = (val & dsp->sysclk_mask)
				>> dsp->sysclk_shift;

			ret = regmap_update_bits(dsp->regmap,
						 dsp->base + ADSP1_CONTROL_31,
						 ADSP1_CLK_SEL_MASK, val);
			if (ret != 0) {
				adsp_err(dsp, "Failed to set clock rate: %d\n",
					 ret);
				return ret;
			}
		}

		ret = wm_adsp_load(dsp);
		if (ret != 0)
			goto err;

		ret = wm_adsp1_setup_algs(dsp);
		if (ret != 0)
			goto err;

		ret = wm_adsp_load_coeff(dsp);
		if (ret != 0)
			goto err;

		/* Initialize caches for enabled and unset controls */
		ret = wm_coeff_init_control_caches(dsp);
		if (ret != 0)
			goto err;

		/* Sync set controls */
		ret = wm_coeff_sync_controls(dsp);
		if (ret != 0)
			goto err;

		/* Start the core running */
		regmap_update_bits(dsp->regmap, dsp->base + ADSP1_CONTROL_30,
				   ADSP1_CORE_ENA | ADSP1_START,
				   ADSP1_CORE_ENA | ADSP1_START);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		/* Halt the core */
		regmap_update_bits(dsp->regmap, dsp->base + ADSP1_CONTROL_30,
				   ADSP1_CORE_ENA | ADSP1_START, 0);

		regmap_update_bits(dsp->regmap, dsp->base + ADSP1_CONTROL_19,
				   ADSP1_WDMA_BUFFER_LENGTH_MASK, 0);

		regmap_update_bits(dsp->regmap, dsp->base + ADSP1_CONTROL_30,
				   ADSP1_SYS_ENA, 0);

		list_for_each_entry(ctl, &dsp->ctl_list, list)
			ctl->enabled = 0;

		while (!list_empty(&dsp->alg_regions)) {
			alg_region = list_first_entry(&dsp->alg_regions,
						      struct wm_adsp_alg_region,
						      list);
			list_del(&alg_region->list);
			kfree(alg_region);
		}
		break;

	default:
		break;
	}

	return 0;

err:
	regmap_update_bits(dsp->regmap, dsp->base + ADSP1_CONTROL_30,
			   ADSP1_SYS_ENA, 0);
	return ret;
}
EXPORT_SYMBOL_GPL(wm_adsp1_event);

static int wm_adsp2_ena(struct wm_adsp *dsp)
{
	unsigned int val;
	int ret, count;

	switch (dsp->rev) {
	case 0:
		ret = regmap_update_bits_async(dsp->regmap,
						dsp->base + ADSP2_CONTROL,
						ADSP2_SYS_ENA, ADSP2_SYS_ENA);
		if (ret != 0)
			return ret;

		/* Wait for the RAM to start, should be near instantaneous */
		for (count = 0; count < 10; ++count) {
			ret = regmap_read(dsp->regmap, dsp->base + ADSP2_STATUS1,
					  &val);
			if (ret != 0)
				return ret;

			if (val & ADSP2_RAM_RDY)
				break;

			msleep(1);
		}

		if (!(val & ADSP2_RAM_RDY)) {
			adsp_err(dsp, "Failed to start DSP RAM\n");
			return -EBUSY;
		}
		adsp_dbg(dsp, "RAM ready after %d polls\n", count);
		break;
	default:
		ret = regmap_update_bits(dsp->regmap, dsp->base + ADSP2_CONTROL,
				 ADSP2_MEM_ENA, ADSP2_MEM_ENA);

		if (ret != 0)
			return ret;

		break;
	}

	return 0;
}

static void wm_adsp2_boot_work(struct work_struct *work)
{
	struct wm_adsp *dsp = container_of(work,
					   struct wm_adsp,
					   boot_work);
	int ret;

	ret = wm_adsp2_ena(dsp);
	if (ret != 0)
		return;

	ret = wm_adsp_load(dsp);
	if (ret != 0)
		goto err;

	ret = wm_adsp2_setup_algs(dsp);
	if (ret != 0)
		goto err;

	ret = wm_adsp_load_coeff(dsp);
	if (ret != 0)
		goto err;

	/* Initialize caches for enabled and unset controls */
	ret = wm_coeff_init_control_caches(dsp);
	if (ret != 0)
		goto err;

	/* Sync set controls */
	ret = wm_coeff_sync_controls(dsp);
	if (ret != 0)
		goto err;

	/* Check firmware features */
	ret = wm_adsp_get_features(dsp);
	if (ret != 0)
		goto err;

	dsp->running = true;

	return;

err:
	regmap_update_bits(dsp->regmap, dsp->base + ADSP2_CONTROL,
			   ADSP2_SYS_ENA | ADSP2_CORE_ENA | ADSP2_START, 0);
}

static void wm_adsp2_set_dspclk(struct wm_adsp *dsp, unsigned int freq)
{
	int ret;
	unsigned int mask, val;

	switch (dsp->rev) {
	case 0:
		ret = regmap_update_bits(dsp->regmap,
					 dsp->base + ADSP2_CLOCKING,
					 ADSP2_CLK_SEL_MASK,
					 freq << ADSP2_CLK_SEL_SHIFT);
		if (ret != 0) {
			adsp_err(dsp, "Failed to set clock rate: %d\n", ret);
			return;
		}
		break;
	default:
		mutex_lock(&dsp->rate_lock);

		mask = ADSP2V2_RATE_MASK;
		val = dsp->rate_cache << ADSP2V2_RATE_SHIFT;

		switch (dsp->rev) {
		case 0:
		case 1:
			/* use legacy frequency registers */
			mask |= ADSP2V2_CLK_SEL_MASK;
			val |= (freq << ADSP2V2_CLK_SEL_SHIFT);
			break;
		default:
			/* Configure exact dsp frequency */
			ret = regmap_write(dsp->regmap,
					dsp->base + ADSP2V2_CLOCKING,
					freq);
			if (ret != 0) {
				adsp_err(dsp,
					 "Failed to set DSP freq: %d\n",
					 ret);
				mutex_unlock(&dsp->rate_lock);
				return;
			}
			break;
		}

		ret = dsp->rate_put_cb(dsp, mask, val);
		if (ret != 0) {
			adsp_err(dsp, "Failed to set DSP_CLK rate: %d\n", ret);
			mutex_unlock(&dsp->rate_lock);
			return;
		}

		mutex_unlock(&dsp->rate_lock);
		break;
	}
}

int wm_adsp2_early_event(struct snd_soc_dapm_widget *w,
		   struct snd_kcontrol *kcontrol, int event,
		   unsigned int freq)
{
	struct snd_soc_codec *codec = w->codec;
	struct wm_adsp *dsps = snd_soc_codec_get_drvdata(codec);
	struct wm_adsp *dsp = &dsps[w->shift];

	dsp->card = codec->component.card;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		wm_adsp2_set_dspclk(dsp, freq);
		queue_work(system_unbound_wq, &dsp->boot_work);
		break;
	default:
		break;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(wm_adsp2_early_event);

static void wm_adsp_edac_shutdown(struct wm_adsp *dsp)
{
	int i, ret;
	unsigned int val = 1;
	const struct wm_adsp_region *mem;

	mem = wm_adsp_find_region(dsp, WMFW_ADSP2_YM);
	if (!mem) {
		adsp_err(dsp, "Failed to locate YM\n");
		return;
	}

	ret = regmap_write(dsp->regmap, mem->base + 0x1, val);
	if (ret != 0) {
		adsp_err(dsp,
			 "Failed to inform eDAC to shutdown: %d\n",
			 ret);
		return;
	}

	for (i = 0; i < 5; ++i) {
		ret = regmap_read(dsp->regmap, mem->base + 0x1, &val);
		if (ret != 0) {
			adsp_err(dsp,
				 "Failed to check for eDAC shutdown: %d\n",
				 ret);
			return;
		}

		if (!val)
			break;

		msleep(1);
	}

	if (val)
		adsp_err(dsp, "Failed to shutdown eDAC firmware\n");
}

static inline void wm_adsp_stop_watchdog(struct wm_adsp *adsp)
{
	switch (adsp->rev) {
	case 0:
	case 1:
		return;
	default:
		regmap_update_bits(adsp->regmap,
			adsp->base + ADSP2_WATCHDOG,
			ADSP2_WDT_ENA_MASK, 0);
	}
}

int wm_adsp2_event(struct snd_soc_dapm_widget *w,
		   struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct wm_adsp *dsps = snd_soc_codec_get_drvdata(codec);
	struct wm_adsp *dsp = &dsps[w->shift];
	struct wm_adsp_alg_region *alg_region;
	struct wm_coeff_ctl *ctl;
	unsigned int scratch1 = 0xFFFFFFFF;
	int ret;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		flush_work(&dsp->boot_work);

		if (!dsp->running)
			return -EIO;

		wm_adsp2_lock(dsp, dsp->lock_regions);

		ret = regmap_update_bits(dsp->regmap,
			 dsp->base + ADSP2_CONTROL,
			 ADSP2_CORE_ENA | ADSP2_START,
			 ADSP2_CORE_ENA | ADSP2_START);

		if (ret != 0)
			goto err;
		break;

	case SND_SOC_DAPM_PRE_PMD:
		/* Capture DSP_SCRATCH1, it can be useful for analysis */
		switch (dsp->rev) {
		case 0:
			ret = regmap_read(dsp->regmap,
					  dsp->base + ADSP2_SCRATCH1,
					  &scratch1);
			break;
		default:
			ret = regmap_read(dsp->regmap,
					  dsp->base + ADSP2V2_SCRATCH0_1,
					  &scratch1);
			break;
		}

		if (ret < 0)
			adsp_err(dsp, "Failed to read SCRATCH1 %d\n", ret);

		if (dsp->fw_features.shutdown)
			wm_adsp_edac_shutdown(dsp);

		wm_adsp_stop_watchdog(dsp);

		dsp->running = false;

		wm_adsp_debugfs_save_wmfwname(dsp, NULL);
		wm_adsp_debugfs_save_binname(dsp, NULL);

		switch (dsp->rev) {
		case 0:
			regmap_update_bits(dsp->regmap,
					   dsp->base + ADSP2_CONTROL,
					   ADSP2_SYS_ENA | ADSP2_CORE_ENA |
					   ADSP2_START, 0);

			/* Make sure DMAs are quiesced */
			regmap_write(dsp->regmap,
				     dsp->base + ADSP2_WDMA_CONFIG_1, 0);
			regmap_write(dsp->regmap,
				     dsp->base + ADSP2_WDMA_CONFIG_2, 0);
			regmap_write(dsp->regmap,
				     dsp->base + ADSP2_RDMA_CONFIG_1, 0);
			break;
		default:
			regmap_update_bits(dsp->regmap,
					   dsp->base + ADSP2_CONTROL,
					   ADSP2_MEM_ENA | ADSP2_SYS_ENA |
					   ADSP2_CORE_ENA | ADSP2_START, 0);

			/* Make sure DMAs are quiesced */
			regmap_write(dsp->regmap,
				     dsp->base + ADSP2_WDMA_CONFIG_1, 0);
			regmap_write(dsp->regmap,
				     dsp->base + ADSP2V2_WDMA_CONFIG_2, 0);
			regmap_write(dsp->regmap,
				     dsp->base + ADSP2_RDMA_CONFIG_1, 0);
			break;
		}

		list_for_each_entry(ctl, &dsp->ctl_list, list)
			ctl->enabled = 0;

		while (!list_empty(&dsp->alg_regions)) {
			alg_region = list_first_entry(&dsp->alg_regions,
						      struct wm_adsp_alg_region,
						      list);
			list_del(&alg_region->list);
			kfree(alg_region);
		}

		adsp_info(dsp, "Shutdown complete (SCRATCH1:0x%x)\n", scratch1);
		break;

	default:
		break;
	}

	return 0;
err:
	regmap_update_bits(dsp->regmap, dsp->base + ADSP2_CONTROL,
			   ADSP2_SYS_ENA | ADSP2_CORE_ENA | ADSP2_START, 0);
	return ret;
}
EXPORT_SYMBOL_GPL(wm_adsp2_event);

#ifdef CONFIG_OF
static int wm_adsp_of_parse_caps(struct wm_adsp *dsp,
				 struct device_node *np,
				 struct wm_adsp_fw_defs *fw)
{
	const char *prop = "wlf,compr-caps";
	int i;
	int len_prop;
	u32 of_cap;

	if (!of_get_property(np, prop, &len_prop))
		return -EINVAL;

	len_prop /= sizeof(u32);

	if (len_prop < 5 || len_prop > 5 + MAX_NUM_SAMPLE_RATES)
		return -EOVERFLOW;

	fw->num_caps = 1;
	fw->caps = devm_kzalloc(dsp->dev,
				sizeof(struct wm_adsp_fw_caps),
				GFP_KERNEL);
	if (!fw->caps)
		return -ENOMEM;

	fw->caps->num_host_regions = ARRAY_SIZE(ez2control_regions);
	fw->caps->host_region_defs =
		devm_kzalloc(dsp->dev,
			     sizeof(ez2control_regions),
			     GFP_KERNEL);
	if (!fw->caps->host_region_defs)
		return -ENOMEM;

	memcpy(fw->caps->host_region_defs,
	       ez2control_regions,
	       sizeof(ez2control_regions));

	of_property_read_u32_index(np, prop, 0, &of_cap);
	fw->caps->id = of_cap;
	of_property_read_u32_index(np, prop, 1, &of_cap);
	fw->caps->desc.max_ch = of_cap;
	of_property_read_u32_index(np, prop, 2, &of_cap);
	fw->caps->desc.formats = of_cap;
	of_property_read_u32_index(np, prop, 3, &of_cap);
	fw->compr_direction = of_cap;

	for (i = 4; i < len_prop; ++i) {
		of_property_read_u32_index(np, prop, i, &of_cap);
		fw->caps->desc.sample_rates[i - 4] = of_cap;
	}
	fw->caps->desc.num_sample_rates = i - 4;

	return 0;
}

static int wm_adsp_of_parse_firmware(struct wm_adsp *dsp,
				     struct device_node *np)
{
	struct device_node *fws = of_get_child_by_name(np, "firmware");
	struct device_node *fw = NULL;
	const char **ctl_names;
	int ret;
	int i;

	if (!fws)
		return 0;

	i = 0;
	while ((fw = of_get_next_child(fws, fw)) != NULL)
		i++;

	if (i == 0)
		return 0;

	dsp->num_firmwares = i;

	dsp->firmwares = devm_kzalloc(dsp->dev,
				       i * sizeof(struct wm_adsp_fw_defs),
				       GFP_KERNEL);
	if (!dsp->firmwares)
		return -ENOMEM;

	ctl_names = devm_kzalloc(dsp->dev,
				 i * sizeof(const char *),
				 GFP_KERNEL);
	if (!ctl_names)
		return -ENOMEM;

	i = 0;
	while ((fw = of_get_next_child(fws, fw)) != NULL) {
		ctl_names[i] = fw->name;
		dsp->firmwares[i].name = fw->name;

		ret = of_property_read_string(fw, "wlf,wmfw-file",
					      &dsp->firmwares[i].file);
		if (ret < 0) {
			adsp_err(dsp,
				 "Firmware filename missing/malformed: %d\n",
				 ret);
			return ret;
		}

		ret = of_property_read_string(fw, "wlf,bin-file",
					      &dsp->firmwares[i].binfile);
		if (ret < 0)
			dsp->firmwares[i].binfile = NULL;

		wm_adsp_of_parse_caps(dsp, fw, &dsp->firmwares[i]);

		i++;
	}

	wm_adsp_fw_enum[dsp->num - 1].items = dsp->num_firmwares;
	wm_adsp_fw_enum[dsp->num - 1].texts = ctl_names;

	return dsp->num_firmwares;
}

static int wm_adsp_of_parse_adsp(struct wm_adsp *dsp)
{
	struct device_node *np = of_get_child_by_name(dsp->dev->of_node,
						      "adsps");
	struct device_node *core = NULL;
	unsigned int addr;
	int ret;

	if (!np)
		return 0;

	while ((core = of_get_next_child(np, core)) != NULL) {
		ret = of_property_read_u32(core, "reg", &addr);
		if (ret < 0) {
			adsp_err(dsp,
				 "Failed to get ADSP base address: %d\n",
				 ret);
			return ret;
		}

		if (addr == dsp->base)
			break;
	}

	if (!core)
		return 0;

	return wm_adsp_of_parse_firmware(dsp, core);
}
#else
static inline int wm_adsp_of_parse_adsp(struct wm_adsp *dsp)
{
	return 0;
}
#endif

int wm_adsp2_init(struct wm_adsp *dsp, struct mutex *fw_lock)
{
	int ret, i;
	const char **ctl_names;

	/*
	 * Disable the DSP memory by default when in reset for a small
	 * power saving.
	 */
	ret = regmap_update_bits(dsp->regmap, dsp->base + ADSP2_CONTROL,
				 ADSP2_MEM_ENA, 0);
	if (ret != 0) {
		adsp_err(dsp, "Failed to clear memory retention: %d\n", ret);
		return ret;
	}

	INIT_LIST_HEAD(&dsp->alg_regions);
	INIT_LIST_HEAD(&dsp->ctl_list);
	INIT_WORK(&dsp->boot_work, wm_adsp2_boot_work);
	mutex_init(&dsp->ctl_lock);
	mutex_init(&dsp->rate_lock);

	dsp->fw_lock = fw_lock;

	if (!dsp->num_firmwares) {
		if (!dsp->dev->of_node || wm_adsp_of_parse_adsp(dsp) <= 0) {
			dsp->num_firmwares = WM_ADSP_NUM_FW;
			dsp->firmwares = wm_adsp_fw;
		}

		for (i = 0; i < dsp->num_firmwares; i++)
			dsp->firmwares[i].name = wm_adsp_fw_text[i];
	} else {
		ctl_names = devm_kzalloc(dsp->dev,
				dsp->num_firmwares * sizeof(const char *),
				GFP_KERNEL);

		for (i = 0; i < dsp->num_firmwares; i++)
			ctl_names[i] = dsp->firmwares[i].name;

		wm_adsp_fw_enum[dsp->num - 1].items = dsp->num_firmwares;
		wm_adsp_fw_enum[dsp->num - 1].texts = ctl_names;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(wm_adsp2_init);

bool wm_adsp_compress_supported(const struct wm_adsp *dsp,
				const struct snd_compr_stream *stream)
{
	if (dsp->fw >= 0 && dsp->fw < dsp->num_firmwares) {
		const struct wm_adsp_fw_defs *fw_defs =
				&dsp->firmwares[dsp->fw];

		if (fw_defs->num_caps == 0)
			return false;

		if (fw_defs->compr_direction == stream->direction)
			return true;
	}

	return false;
}
EXPORT_SYMBOL_GPL(wm_adsp_compress_supported);

bool wm_adsp_format_supported(const struct wm_adsp *dsp,
			      const struct snd_compr_stream *stream,
			      const struct snd_compr_params *params)
{
	const struct wm_adsp_fw_caps *caps;
	int i, j;

	for (i = 0; i < dsp->firmwares[dsp->fw].num_caps; i++) {
		caps = &dsp->firmwares[dsp->fw].caps[i];

		if (caps->id != params->codec.id)
			continue;

		if (stream->direction == SND_COMPRESS_PLAYBACK) {
			if (caps->desc.max_ch < params->codec.ch_out)
				continue;
		} else {
			if (caps->desc.max_ch < params->codec.ch_in)
				continue;
		}

		if (!(caps->desc.formats & (1 << params->codec.format)))
			continue;

		for (j = 0; j < caps->desc.num_sample_rates; ++j)
			if (caps->desc.sample_rates[j] ==
			    params->codec.sample_rate)
				return true;
	}

	return false;
}
EXPORT_SYMBOL_GPL(wm_adsp_format_supported);

void wm_adsp_get_caps(const struct wm_adsp *dsp,
		      const struct snd_compr_stream *stream,
		      struct snd_compr_caps *caps)
{
	int i;

	if (dsp->firmwares[dsp->fw].caps) {
		for (i = 0; i < dsp->firmwares[dsp->fw].num_caps; i++)
			caps->codecs[i] = dsp->firmwares[dsp->fw].caps[i].id;

		caps->num_codecs = i;
		caps->direction = dsp->firmwares[dsp->fw].compr_direction;
	}
}
EXPORT_SYMBOL_GPL(wm_adsp_get_caps);

static int wm_adsp_read_data_block(struct wm_adsp *dsp, int mem_type,
				   unsigned int mem_addr,
				   unsigned int num_words,
				   u32 *data)
{
	struct wm_adsp_region const *mem = wm_adsp_find_region(dsp,
							       mem_type);
	unsigned int i, reg;
	int ret;

	if (!mem)
		return -EINVAL;

	reg = wm_adsp_region_to_reg(mem, mem_addr);

	ret = regmap_raw_read(dsp->regmap, reg, data,
			      sizeof(*data) * num_words);
	if (ret < 0)
		return ret;

	for (i = 0; i < num_words; ++i)
		data[i] = be32_to_cpu(data[i]) & 0x00ffffffu;

	return 0;
}

static int wm_adsp_read_data_word(struct wm_adsp *dsp, int mem_type,
				  unsigned int mem_addr, u32 *data)
{
	return wm_adsp_read_data_block(dsp, mem_type, mem_addr, 1, data);
}

static int wm_adsp_write_data_word(struct wm_adsp *dsp, int mem_type,
				   unsigned int mem_addr, u32 data)
{
	struct wm_adsp_region const *mem = wm_adsp_find_region(dsp,
							       mem_type);
	unsigned int reg;

	if (!mem)
		return -EINVAL;

	reg = wm_adsp_region_to_reg(mem, mem_addr);

	data = cpu_to_be32(data & 0x00ffffffu);

	return regmap_raw_write(dsp->regmap, reg, &data, sizeof(data));
}

static inline int wm_adsp_host_buffer_read(struct wm_adsp *dsp,
					   unsigned int field_offset, u32 *data)
{
	return wm_adsp_read_data_word(dsp, WMFW_ADSP2_XM,
				      dsp->host_buf_ptr + field_offset, data);
}

static inline int wm_adsp_host_buffer_write(struct wm_adsp *dsp,
					    unsigned int field_offset, u32 data)
{
	return wm_adsp_write_data_word(dsp, WMFW_ADSP2_XM,
				       dsp->host_buf_ptr + field_offset,
				       data);
}

static inline int wm_adsp_host_buffer2_read(struct wm_adsp *adsp,
					   unsigned int field_offset, u32 *data)
{
	return wm_adsp_read_data_word(adsp, WMFW_ADSP2_XM,
				      adsp->host_buf_ptr2 + field_offset, data);
}

static inline int wm_adsp_host_buffer2_write(struct wm_adsp *adsp,
					    unsigned int field_offset, u32 data)
{
	return wm_adsp_write_data_word(adsp, WMFW_ADSP2_XM,
				       adsp->host_buf_ptr2 + field_offset,
				       data);
}

static int wm_adsp_populate_buffer_regions(struct wm_adsp *dsp)
{
	int i, ret;
	u32 offset = 0;
	struct wm_adsp_buffer_region_def *host_region_defs =
		dsp->firmwares[dsp->fw].caps->host_region_defs;
	struct wm_adsp_buffer_region *region;

	for (i = 0; i < dsp->firmwares[dsp->fw].caps->num_host_regions; ++i) {
		region = &dsp->host_regions[i];

		region->offset = offset;
		region->mem_type = host_region_defs[i].mem_type;

		ret = wm_adsp_host_buffer_read(dsp,
					       host_region_defs[i].base_offset,
					       &region->base_addr);
		if (ret < 0)
			return ret;

		ret = wm_adsp_host_buffer_read(dsp,
					       host_region_defs[i].size_offset,
					       &offset);
		if (ret < 0)
			return ret;

		region->cumulative_size = offset;

		adsp_dbg(dsp,
			 "Region %d type %d base %04x off %04x size %04x\n",
			 i, region->mem_type, region->base_addr,
			 region->offset, region->cumulative_size);
	}

	return 0;
}

static int wm_adsp_populate_buffer_regions2(struct wm_adsp *adsp)
{
	int i, ret;
	u32 offset2 = 0;

	struct wm_adsp_buffer_region_def *host_region_defs =
		adsp->firmwares[adsp->fw].caps->host_region_defs;
	struct wm_adsp_buffer_region *region2;

	for (i = 0; i < adsp->firmwares[adsp->fw].caps->num_host_regions; ++i) {
		region2 = &adsp->host_regions2[i];
		region2->offset = offset2;
		region2->mem_type = host_region_defs[i].mem_type;

		ret = wm_adsp_host_buffer2_read(adsp,
					       host_region_defs[i].base_offset,
					       &region2->base_addr);

		if (ret < 0)
			return ret;

		ret = wm_adsp_host_buffer2_read(adsp,
				       host_region_defs[i].size_offset,
				       &offset2);

		region2->cumulative_size = offset2;

		adsp_dbg(adsp,
			 "Region2 %d type %d base %04x off %04x size %04x\n",
			 i, region2->mem_type, region2->base_addr,
			 region2->offset, region2->cumulative_size);

	}

	return 0;
}

static int wm_adsp_read_buffer(struct wm_adsp *dsp, int32_t read_index,
			       int avail)
{
	int circ_space_words = CIRC_SPACE(dsp->capt_buf.head,
					  dsp->capt_buf.tail,
					  dsp->capt_buf_size) /
			       WM_ADSP_DATA_WORD_SIZE;
	u8 *capt_buf = (u8 *)dsp->capt_buf.buf;
	int capt_buf_h = dsp->capt_buf.head;
	int capt_buf_mask = dsp->capt_buf_size - 1;
	int mem_type;
	unsigned int adsp_addr;
	int num_words;
	int i, ret;

	/* Calculate read parameters */
	for (i = 0; i < dsp->firmwares[dsp->fw].caps->num_host_regions; ++i) {
		if (read_index < dsp->host_regions[i].cumulative_size)
			break;
	}

	if (i == dsp->firmwares[dsp->fw].caps->num_host_regions)
		return -EINVAL;

	num_words = dsp->host_regions[i].cumulative_size - read_index;
	mem_type = dsp->host_regions[i].mem_type;
	adsp_addr = dsp->host_regions[i].base_addr +
		    (read_index - dsp->host_regions[i].offset);

	if (circ_space_words < num_words)
		num_words = circ_space_words;
	if (avail < num_words)
		num_words = avail;
	if (num_words >= WM_ADSP_MAX_READ_SIZE) {
		num_words = WM_ADSP_MAX_READ_SIZE;
	}
	if (!num_words)
		return 0;

	/* Read data from DSP */
	ret = wm_adsp_read_data_block(dsp, mem_type, adsp_addr,
				      num_words, dsp->raw_capt_buf);
	if (ret != 0)
		return ret;

	/* Copy to circular buffer */
	for (i = 0; i < num_words; ++i) {
		u32 x = dsp->raw_capt_buf[i];

		capt_buf[capt_buf_h++] = (u8)((x >> 0) & 0xff);
		capt_buf_h &= capt_buf_mask;
		capt_buf[capt_buf_h++] = (u8)((x >> 8) & 0xff);
		capt_buf_h &= capt_buf_mask;
		capt_buf[capt_buf_h++] = (u8)((x >> 16) & 0xff);
		capt_buf_h &= capt_buf_mask;
	}

	dsp->capt_buf.head = capt_buf_h;

	return num_words;
}

static int wm_adsp_read_buffer2(struct wm_adsp *adsp, int32_t read_index,
			       int avail)
{
	int circ_space_words = CIRC_SPACE(adsp->capt_buf2.head,
					  adsp->capt_buf2.tail,
					  adsp->capt_buf_size) /
			       WM_ADSP_DATA_WORD_SIZE;
	u8 *capt_buf;
	int capt_buf_h = adsp->capt_buf2.head;
	int capt_buf_mask = adsp->capt_buf_size - 1;
	int mem_type;
	unsigned int adsp_addr;
	int num_words;
	int i, ret;

	if (!adsp->capt_buf2.buf) {
		adsp_err(adsp, "Invalid argument\n");
		return -EINVAL;
	}
	capt_buf = (u8 *)adsp->capt_buf2.buf;
	/* Calculate read parameters */
	for (i = 0; i < adsp->firmwares[adsp->fw].caps->num_host_regions; ++i) {
		if (read_index < adsp->host_regions2[i].cumulative_size)
			break;
	}

	if (i == adsp->firmwares[adsp->fw].caps->num_host_regions)
		return -EINVAL;

	num_words = adsp->host_regions2[i].cumulative_size - read_index;
	mem_type = adsp->host_regions2[i].mem_type;
	adsp_addr = adsp->host_regions2[i].base_addr +
		    (read_index - adsp->host_regions2[i].offset);

	if (circ_space_words < num_words)
		num_words = circ_space_words;
	if (avail < num_words)
		num_words = avail;
	if (num_words >= WM_ADSP_MAX_READ_SIZE)
		num_words = WM_ADSP_MAX_READ_SIZE;

	if (!num_words)
		return 0;

	/* Read data from DSP */
	if (!adsp->raw_capt_buf2) {
		adsp_err(adsp, "Raw capture buffer is NULL\n");
		return -EINVAL;
	}
	ret = wm_adsp_read_data_block(adsp, mem_type, adsp_addr,
				      num_words, adsp->raw_capt_buf2);
	if (ret != 0)
		return ret;

	/* Copy to circular buffer */
	for (i = 0; i < num_words; ++i) {
		u32 x = adsp->raw_capt_buf2[i];

		capt_buf[capt_buf_h++] = (u8)((x >> 0) & 0xff);
		capt_buf_h &= capt_buf_mask;
		capt_buf[capt_buf_h++] = (u8)((x >> 8) & 0xff);
		capt_buf_h &= capt_buf_mask;
		capt_buf[capt_buf_h++] = (u8)((x >> 16) & 0xff);
		capt_buf_h &= capt_buf_mask;
	}

	adsp->capt_buf2.head = capt_buf_h;

	return num_words;
}

static int wm_adsp_capture_block(struct wm_adsp *dsp, int *avail)
{
	int last_region = dsp->firmwares[dsp->fw].caps->num_host_regions - 1;
	int host_size = dsp->host_regions[last_region].cumulative_size;
	int num_words;
	u32 next_read_index, next_write_index;
	int32_t write_index, read_index;
	int ret;

	/* Get current host buffer status */
	ret = wm_adsp_host_buffer_read(dsp,
				       HOST_BUFFER_FIELD(next_read_index),
				       &next_read_index);
	if (ret < 0)
		return ret;
	ret = wm_adsp_host_buffer_read(dsp,
				       HOST_BUFFER_FIELD(next_write_index),
				       &next_write_index);
	if (ret < 0)
		return ret;

	read_index = sign_extend32(next_read_index, 23);
	write_index = sign_extend32(next_write_index, 23);

	/* Don't empty the buffer as it kills the firmware */
	write_index--;

	if (read_index < 0) {
		*avail = 0;
		return 0;	/* stream has not yet started */
	}

	*avail = write_index - read_index;
	if (*avail < 0)
		*avail += host_size;

	/* Read data from DSP */
	num_words = wm_adsp_read_buffer(dsp, read_index, *avail);
	if (num_words <= 0)
		return num_words;

	/* update read index to account for words read */
	next_read_index += num_words;
	if (next_read_index == host_size)
		next_read_index = 0;

	ret = wm_adsp_host_buffer_write(dsp,
					HOST_BUFFER_FIELD(next_read_index),
					next_read_index);
	if (ret < 0)
		return ret;

	return num_words;
}

static int wm_adsp_capture_block2(struct wm_adsp *adsp, int *avail)
{
	int last_region = adsp->firmwares[adsp->fw].caps->num_host_regions - 1;
	int host_size;
	int num_words;
	u32 next_read_index, next_write_index;
	int32_t write_index, read_index;
	int ret;

	if (!adsp->host_regions2 || last_region < 0 || !avail) {
		adsp_err(adsp, "Invalid argument\n");
		return -EINVAL;
	}
	host_size = adsp->host_regions2[last_region].cumulative_size;
	/* Get current host buffer status */
	ret = wm_adsp_host_buffer2_read(adsp,
				       HOST_BUFFER_FIELD(next_read_index),
				       &next_read_index);
	if (ret < 0)
		return ret;

	ret = wm_adsp_host_buffer2_read(adsp,
				       HOST_BUFFER_FIELD(next_write_index),
				       &next_write_index);
	if (ret < 0)
		return ret;

	read_index = sign_extend32(next_read_index, 23);
	write_index = sign_extend32(next_write_index, 23);

	/* Don't empty the buffer as it kills the firmware */
	write_index--;

	if (read_index < 0) {
		*avail = 0;
		return 0;
	}

	*avail = write_index - read_index;
	if (*avail < 0)
		*avail += host_size;

	/* Read data from DSP */
	num_words = wm_adsp_read_buffer2(adsp, read_index, *avail);
	if (num_words <= 0)
		return num_words;

	/* update read index to account for words read */
	next_read_index += num_words;
	if (next_read_index == host_size)
		next_read_index = 0;

	ret = wm_adsp_host_buffer2_write(adsp,
					HOST_BUFFER_FIELD(next_read_index),
					next_read_index);
	if (ret < 0)
		return ret;

	return num_words;
}

int wm_adsp_stream_alloc(struct wm_adsp *dsp,
			 const struct snd_compr_params *params)
{
	int ret;
	unsigned int size;

	dsp->dsp_error = 0;

	if (!dsp->capt_buf.buf) {
		dsp->capt_buf_size = WM_ADSP_CAPTURE_BUFFER_SIZE;
		dsp->capt_buf.buf = vmalloc(dsp->capt_buf_size);

		if (!dsp->capt_buf.buf)
			return -ENOMEM;
	}

	dsp->capt_buf.head = 0;
	dsp->capt_buf.tail = 0;

	if (!dsp->raw_capt_buf) {
		size = WM_ADSP_MAX_READ_SIZE * sizeof(*dsp->raw_capt_buf);
		dsp->raw_capt_buf = kzalloc(size, GFP_DMA | GFP_KERNEL);

		if (!dsp->raw_capt_buf) {
			ret = -ENOMEM;
			goto err_capt_buf;
		}
	}

	if (!dsp->host_regions) {
		size = dsp->firmwares[dsp->fw].caps->num_host_regions *
		       sizeof(*dsp->host_regions);
		dsp->host_regions = kzalloc(size, GFP_KERNEL);

		if (!dsp->host_regions) {
			ret = -ENOMEM;
			goto err_raw_capt_buf;
		}
	}

	size = params->buffer.fragment_size;
	if (size == 0) {
		dsp->capt_watermark = WM_ADSP_DEFAULT_WATERMARK;
		adsp_warn(dsp, "No fragment size, assuming %u",
				dsp->capt_watermark * WM_ADSP_DATA_WORD_SIZE);
	} else {
		dsp->capt_watermark =
				DIV_ROUND_UP(size, WM_ADSP_DATA_WORD_SIZE);
	}

	return 0;

err_raw_capt_buf:
	kfree(dsp->raw_capt_buf);
	dsp->raw_capt_buf = NULL;
err_capt_buf:
	vfree(dsp->capt_buf.buf);
	dsp->capt_buf.buf = NULL;
	return ret;
}
EXPORT_SYMBOL_GPL(wm_adsp_stream_alloc);

int wm_adsp_stream_alloc2(struct wm_adsp *adsp,
			 const struct snd_compr_params *params)
{
	int ret;
	unsigned int size;

	adsp->dsp_error = 0;

	adsp_dbg(adsp, "%s\n", __func__);
	if (!adsp->capt_buf2.buf) {
		adsp->capt_buf_size = WM_ADSP_CAPTURE_BUFFER_SIZE;
		adsp->capt_buf2.buf = vmalloc(adsp->capt_buf_size);

		if (!adsp->capt_buf2.buf) {
			ret = -ENOMEM;
			goto err_capt_buf;
		}

		adsp->capt_buf2.head = 0;
		adsp->capt_buf2.tail = 0;
	}

	if (!adsp->raw_capt_buf2) {
		size = WM_ADSP_MAX_READ_SIZE * sizeof(*adsp->raw_capt_buf2);
		adsp->raw_capt_buf2 = kzalloc(size, GFP_KERNEL);

		if (!adsp->raw_capt_buf2) {
			ret = -ENOMEM;
			goto err_raw_capt_buf;
		}
	}

	if (!adsp->host_regions2) {
		size = adsp->firmwares[adsp->fw].caps->num_host_regions *
		       sizeof(*adsp->host_regions);
		adsp->host_regions2 = kzalloc(size, GFP_KERNEL);

		if (!adsp->host_regions2) {
			ret = -ENOMEM;
			goto err_raw_capt_buf;
		}
	}

	size = params->buffer.fragment_size;
	if (size == 0) {
		adsp->capt_watermark2 = WM_ADSP_DEFAULT_WATERMARK;
		adsp_warn(adsp, "No fragment size, assuming %u",
				adsp->capt_watermark2 * WM_ADSP_DATA_WORD_SIZE);
	} else {
		adsp->capt_watermark2 =
				DIV_ROUND_UP(size, WM_ADSP_DATA_WORD_SIZE);
	}
	return 0;

err_raw_capt_buf:
	kfree(adsp->raw_capt_buf2);
	adsp->raw_capt_buf2 = NULL;
err_capt_buf:
	vfree(adsp->capt_buf2.buf);
	adsp->capt_buf2.buf = NULL;
	return ret;
}
EXPORT_SYMBOL_GPL(wm_adsp_stream_alloc2);

int wm_adsp_stream_free(struct wm_adsp *adsp, int buffer)
{
	switch (buffer) {
	case ADSP2_BUFFER_1:
		kfree(adsp->host_regions);
		adsp->host_regions = NULL;
		kfree(adsp->raw_capt_buf);
		adsp->raw_capt_buf = NULL;
		vfree(adsp->capt_buf.buf);
		adsp->capt_buf.buf = NULL;
		break;
	case ADSP2_BUFFER_2:
		kfree(adsp->host_regions2);
		adsp->host_regions2 = NULL;
		kfree(adsp->raw_capt_buf2);
		adsp->raw_capt_buf2 = NULL;
		vfree(adsp->capt_buf2.buf);
		adsp->capt_buf2.buf = NULL;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(wm_adsp_stream_free);

int wm_adsp_stream_start(struct wm_adsp *dsp)
{
	u32 xm_base, magic;
	int i, ret;
	u32 stream_offset;

	ret = wm_adsp_read_data_word(dsp, WMFW_ADSP2_XM,
			ADSP2_SYSTEM_CONFIG_XM_PTR, &xm_base);
	if (ret < 0)
		return ret;

	stream_offset = xm_base + WM_ADSP_ALG_XM_PTR;
	ret = wm_adsp_read_data_word(dsp, WMFW_ADSP2_XM,
			xm_base + WM_ADSP_ALG_XM_PTR +
			0,  /* Read first 32 bit word from header */
			&magic);
	if (ret < 0)
		return ret;

	if ((magic != WM_ADSP_ALG_XM_STRUCT_MAGIC) &&
		(magic != WM_ADSP_ALG_XM2_STRUCT_MAGIC))
		return -EINVAL;

	if (magic == WM_ADSP_ALG_XM2_STRUCT_MAGIC)
		stream_offset++;

	for (i = 0; i < 5; ++i) {
		ret = wm_adsp_read_data_word(dsp, WMFW_ADSP2_XM,
					stream_offset +
					ALG_XM_FIELD(host_buf_ptr),
					&dsp->host_buf_ptr);
		if (ret < 0)
			return ret;

		if (dsp->host_buf_ptr)
			break;

		msleep(1);
	}

	if (!dsp->host_buf_ptr)
		return -EIO;

	dsp->max_dsp_read_bytes = WM_ADSP_MAX_READ_SIZE * sizeof(u32);
	ret = wm_adsp_populate_buffer_regions(dsp);
	if (ret < 0)
		return ret;

	ret = wm_adsp_host_buffer_write(dsp,
					HOST_BUFFER_FIELD(high_water_mark),
					dsp->capt_watermark);
	if (ret < 0)
		return ret;

	adsp_dbg(dsp, "Set watermark to %u\n", dsp->capt_watermark);

	return 0;
}
EXPORT_SYMBOL_GPL(wm_adsp_stream_start);

int wm_adsp_stream_start2(struct wm_adsp *adsp)
{
	u32 xm_base, magic;
	int i, ret;
	u32 stream_offset;

	adsp_dbg(adsp, "%s\n", __func__);
	ret = wm_adsp_read_data_word(adsp, WMFW_ADSP2_XM,
			ADSP2_SYSTEM_CONFIG_XM_PTR, &xm_base);
	if (ret < 0)
		return ret;

	stream_offset = xm_base + WM_ADSP_ALG_XM_PTR;
	ret = wm_adsp_read_data_word(adsp, WMFW_ADSP2_XM,
			xm_base + WM_ADSP_ALG_XM_PTR +
			0,  /* Read first 32 bit word from header */
			&magic);
	if (ret < 0)
		return ret;

	if ((magic != WM_ADSP_ALG_XM_STRUCT_MAGIC) &&
		(magic != WM_ADSP_ALG_XM2_STRUCT_MAGIC))
		return -EINVAL;

	if (magic == WM_ADSP_ALG_XM2_STRUCT_MAGIC)
		stream_offset++;

	if (magic == WM_ADSP_ALG_XM2_STRUCT_MAGIC) {
		stream_offset += 7;
		for (i = 0; i < 5; ++i) {
			ret = wm_adsp_read_data_word(adsp, WMFW_ADSP2_XM,
					stream_offset +
					ALG_XM_FIELD(host_buf_ptr),
					&adsp->host_buf_ptr2);
			if (ret < 0)
				return ret;

			if (adsp->host_buf_ptr2)
				break;

		}
		if (!adsp->host_buf_ptr2)
			return -EIO;
	}


	adsp->max_dsp_read_bytes = WM_ADSP_MAX_READ_SIZE * sizeof(u32);
	ret = wm_adsp_populate_buffer_regions2(adsp);
	if (ret < 0)
		return ret;

	ret = wm_adsp_host_buffer2_write(adsp,
					HOST_BUFFER_FIELD(high_water_mark),
					adsp->capt_watermark2);
	if (ret < 0)
		return ret;

	adsp_dbg(adsp, "Set watermark to %u\n", adsp->capt_watermark2);

	return 0;
}
EXPORT_SYMBOL_GPL(wm_adsp_stream_start2);

static int wm_adsp_stream_capture(struct wm_adsp *dsp)
{
	int avail = 0;
	int amount_read;
	int total_read = 0;
	int ret = 0;

	dsp->buffer_drain_pending = false;

	do {
		amount_read = 0;
		do {
			ret = wm_adsp_capture_block(dsp, &avail);
			if (ret < 0)
				return ret;

			amount_read += ret;
		} while (ret > 0 && avail >= WM_ADSP_MAX_READ_SIZE);

		total_read += amount_read;
	} while (amount_read > 0 && avail >= WM_ADSP_MAX_READ_SIZE);

	if (avail >= WM_ADSP_MAX_READ_SIZE)
		dsp->buffer_drain_pending = true;

	return total_read * WM_ADSP_DATA_WORD_SIZE;
}

static int wm_adsp_stream_capture2(struct wm_adsp *adsp)
{
	int avail = 0;
	int amount_read;
	int total_read = 0;
	int ret = 0;

	if (!adsp) {
		pr_err("%s Invalid argument\n", __func__);
		return -EINVAL;
	}

	adsp->buffer2_drain_pending = false;

	do {
		amount_read = 0;
		do {
			ret = wm_adsp_capture_block2(adsp, &avail);
			if (ret < 0)
				return ret;

			amount_read += ret;
		} while (ret > 0 && avail >= WM_ADSP_MAX_READ_SIZE);

		total_read += amount_read;
	} while (amount_read > 0 && avail >= WM_ADSP_MAX_READ_SIZE);

	if (avail >= WM_ADSP_MAX_READ_SIZE)
		adsp->buffer2_drain_pending = true;

	return total_read * WM_ADSP_DATA_WORD_SIZE;
}

static int wm_adsp_ack_buffer_interrupt(struct wm_adsp *dsp)
{
	u32 irq_ack;
	int ret;

	ret = wm_adsp_host_buffer_read(dsp,
				       HOST_BUFFER_FIELD(irq_count),
				       &irq_ack);
	if (ret < 0)
		return ret;

	if (!dsp->buffer_drain_pending)
		irq_ack |= 1;		/* enable further IRQs */

	ret = wm_adsp_host_buffer_write(dsp,
					HOST_BUFFER_FIELD(irq_ack),
					irq_ack);
	return ret;
}

static int wm_adsp_ack_buffer2_interrupt(struct wm_adsp *adsp)
{
	u32 irq_ack;
	int ret;

	ret = wm_adsp_host_buffer2_read(adsp,
				       HOST_BUFFER_FIELD(irq_count),
				       &irq_ack);
	if (ret < 0)
		return ret;

	if (!adsp->buffer2_drain_pending)
		irq_ack |= 1;		/* enable further IRQs */

	ret = wm_adsp_host_buffer2_write(adsp,
					HOST_BUFFER_FIELD(irq_ack),
					irq_ack);
	return ret;
}


int wm_adsp_stream_handle_irq(struct wm_adsp *adsp, bool two_buf)
{
	int ret = 0, bytes_captured = 0;

	adsp->dsp_error = 0;

	if (!two_buf)
		ret = wm_adsp_host_buffer_read(adsp,
			       HOST_BUFFER_FIELD(error),
			       &adsp->dsp_error);
	else
		ret = wm_adsp_host_buffer2_read(adsp,
				HOST_BUFFER_FIELD(error),
				&adsp->dsp_error);
	if (ret < 0)
		return ret;
	if (adsp->dsp_error != 0) {
		adsp_err(adsp, "DSP error occurred: %d\n", adsp->dsp_error);
		return -EIO;
	}
	if (!two_buf) {
		bytes_captured = wm_adsp_stream_capture(adsp);
		if (bytes_captured < 0)
			return bytes_captured;

		ret = wm_adsp_ack_buffer_interrupt(adsp);
		if (ret < 0)
			return ret;
	} else {
		bytes_captured = wm_adsp_stream_capture2(adsp);
		if (bytes_captured <= 0)
			return bytes_captured;

		ret = wm_adsp_ack_buffer2_interrupt(adsp);
		if (ret < 0)
			return ret;
	}

	return bytes_captured;
}
EXPORT_SYMBOL_GPL(wm_adsp_stream_handle_irq);

int wm_adsp_stream_read(struct wm_adsp *dsp, char __user *buf, size_t count)
{
	int avail, to_end;
	int ret;

	if (!dsp->running)
		return -EIO;

	avail = CIRC_CNT(dsp->capt_buf.head,
			 dsp->capt_buf.tail,
			 dsp->capt_buf_size);
	to_end = CIRC_CNT_TO_END(dsp->capt_buf.head,
				 dsp->capt_buf.tail,
				 dsp->capt_buf_size);

	if (avail < count)
		count = avail;

	adsp_dbg(dsp, "%s: avail=%d toend=%d count=%zo\n",
		 __func__, avail, to_end, count);

	if (count > to_end) {
		if (copy_to_user(buf,
				 dsp->capt_buf.buf +
				 dsp->capt_buf.tail,
				 to_end))
			return -EFAULT;
		if (copy_to_user(buf + to_end, dsp->capt_buf.buf,
				 count - to_end))
			return -EFAULT;
	} else {
		if (copy_to_user(buf,
				 dsp->capt_buf.buf +
				 dsp->capt_buf.tail,
				 count))
			return -EFAULT;
	}

	dsp->capt_buf.tail += count;
	dsp->capt_buf.tail &= dsp->capt_buf_size - 1;

	if (dsp->buffer_drain_pending) {
		wm_adsp_stream_capture(dsp);

		ret = wm_adsp_ack_buffer_interrupt(dsp);
		if (ret < 0)
			return ret;
	}

	return count;
}
EXPORT_SYMBOL_GPL(wm_adsp_stream_read);

int wm_adsp_stream_read2(struct wm_adsp *adsp, char __user *buf, size_t count)
{
	int avail, to_end;
	int ret;

	if (!adsp->running)
		return -EIO;

	avail = CIRC_CNT(adsp->capt_buf2.head,
			 adsp->capt_buf2.tail,
			 adsp->capt_buf_size);
	to_end = CIRC_CNT_TO_END(adsp->capt_buf2.head,
				 adsp->capt_buf2.tail,
				 adsp->capt_buf_size);

	if (avail < count)
		count = avail;

	adsp_dbg(adsp, "%s: avail=%d toend=%d count=%zo\n",
		 __func__, avail, to_end, count);

	if (count > to_end) {
		if (copy_to_user(buf,
				 adsp->capt_buf2.buf +
				 adsp->capt_buf2.tail,
				 to_end))
			return -EFAULT;
		if (copy_to_user(buf + to_end, adsp->capt_buf2.buf,
				 count - to_end))
			return -EFAULT;
	} else {
		if (copy_to_user(buf,
				 adsp->capt_buf2.buf +
				 adsp->capt_buf2.tail,
				 count))
			return -EFAULT;
	}

	adsp->capt_buf2.tail += count;
	adsp->capt_buf2.tail &= adsp->capt_buf_size - 1;

	if (adsp->buffer2_drain_pending) {
		wm_adsp_stream_capture2(adsp);

		ret = wm_adsp_ack_buffer2_interrupt(adsp);
		if (ret < 0)
			return ret;
	}

	return count;
}
EXPORT_SYMBOL_GPL(wm_adsp_stream_read2);

int wm_adsp_stream_avail(const struct wm_adsp *dsp)
{
	return CIRC_CNT(dsp->capt_buf.head,
			dsp->capt_buf.tail,
			dsp->capt_buf_size);
}
EXPORT_SYMBOL_GPL(wm_adsp_stream_avail);

/* DSP lock region support */
int wm_adsp2_lock(struct wm_adsp *adsp, unsigned int lock_regions)
{
	struct regmap *regmap_32bit = adsp->regmap;
	unsigned int lockcode0, lockcode1, lock_reg;

	if (!(lock_regions & WM_ADSP2_REGION_ALL))
		return 0;

	lock_regions &= WM_ADSP2_REGION_ALL;
	lock_reg = adsp->base +
		ADSP2_LOCK_REGION_1_LOCK_REGION_0;

	while (lock_regions) {
		lockcode0 = lockcode1 = 0;
		if (lock_regions & BIT(0)) {
			lockcode0 = ADSP2_LOCK_CODE_0;
			lockcode1 = ADSP2_LOCK_CODE_1;
		}
		if (lock_regions & BIT(1)) {
			lockcode0 |= ADSP2_LOCK_CODE_0 <<
				ADSP2_LOCK_REGION_SHIFT;
			lockcode1 |= ADSP2_LOCK_CODE_1 <<
				ADSP2_LOCK_REGION_SHIFT;
		}
		regmap_write(regmap_32bit,
			lock_reg, lockcode0);
		regmap_write(regmap_32bit,
			lock_reg, lockcode1);
		lock_regions >>= 2;
		lock_reg += 2;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(wm_adsp2_lock);

irqreturn_t wm_adsp2_bus_error(struct wm_adsp *adsp)
{
	unsigned int reg_val;
	int ret = 0;
	struct regmap *regmap = adsp->regmap;

	ret = regmap_read(regmap, adsp->base +
			ADSP2_LOCK_REGION_CTRL, &reg_val);
	if (ret != 0) {
		adsp_err(adsp,
			"Failed to read Region Lock Ctrl register: %d\n",
			ret);
		goto exit;
	}

	if (reg_val & ADSP2_WDT_TIMEOUT_STS_MASK) {
		adsp_err(adsp, "watchdog timeout error\n");
		wm_adsp_stop_watchdog(adsp);
	}

	if (reg_val & (ADSP2_SLAVE_ERR_MASK | ADSP2_REGION_LOCK_ERR_MASK)) {
		if (reg_val & ADSP2_SLAVE_ERR_MASK)
			adsp_err(adsp, "bus error: slave error\n");
		else
			adsp_err(adsp, "bus error: region lock error\n");

		ret = regmap_read(regmap, adsp->base +
				ADSP2_BUS_ERR_ADDR, &reg_val);
		if (ret != 0) {
			adsp_err(adsp,
				"Failed to read Bus Err Addr register: %d\n",
				ret);
			goto exit;
		}
		adsp_err(adsp, "bus error address = 0x%x\n",
			(reg_val & ADSP2_BUS_ERR_ADDR_MASK));

		ret = regmap_read(regmap, adsp->base +
				ADSP2_PMEM_ERR_ADDR_XMEM_ERR_ADDR,
				&reg_val);
		if (ret != 0) {
			adsp_err(adsp,
				"Failed to read Pmem Xmem Err Addr register: %d\n",
				ret);
			goto exit;
		}
		adsp_err(adsp, "xmem error address = 0x%x\n",
			(reg_val & ADSP2_XMEM_ERR_ADDR_MASK));
		adsp_err(adsp, "pmem error address = 0x%x\n",
			(reg_val & ADSP2_PMEM_ERR_ADDR_MASK)
			>> ADSP2_PMEM_ERR_ADDR_SHIFT);
	}

	regmap_write(regmap, adsp->base + ADSP2_LOCK_REGION_CTRL,
		     ADSP2_CTRL_ERR_EINT);
exit:
	return IRQ_HANDLED;
}
EXPORT_SYMBOL_GPL(wm_adsp2_bus_error);

#ifdef CONFIG_DEBUG_FS
static void wm_adsp_debugfs_save_wmfwname(struct wm_adsp *dsp, const char *s)
{
	kfree(dsp->wmfw_file_loaded);
	/* kstrdup returns NULL if (s == NULL) */
	dsp->wmfw_file_loaded = kstrdup(s, GFP_KERNEL);
}

static void wm_adsp_debugfs_save_binname(struct wm_adsp *dsp, const char *s)
{
	kfree(dsp->bin_file_loaded);
	dsp->bin_file_loaded = kstrdup(s, GFP_KERNEL);
}

static ssize_t wm_adsp_debugfs_string_read(struct wm_adsp *dsp,
					  char __user *user_buf,
					  size_t count, loff_t *ppos,
					  const char *string)
{
	char *temp;
	int len;
	ssize_t ret;

	if (!string || !dsp->running)
		return 0;

	temp = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!temp)
		return -ENOMEM;

	len = snprintf(temp, PAGE_SIZE, "%s\n", string);
	ret = simple_read_from_buffer(user_buf, count, ppos, temp, len);
	kfree(temp);
	return ret;
}

static ssize_t wm_adsp_debugfs_x32_read(struct wm_adsp *dsp,
					char __user *user_buf,
					size_t count, loff_t *ppos,
					int value)
{
	char temp[12];
	int len;

	if (!dsp->running)
		return 0;

	len = snprintf(temp, sizeof(temp), "0x%06x\n", value);
	return simple_read_from_buffer(user_buf, count, ppos, temp, len);
}

static ssize_t wm_adsp_debugfs_running_read(struct file *file,
					    char __user *user_buf,
					    size_t count, loff_t *ppos)
{
	struct wm_adsp *dsp = file->private_data;
	char temp[2];

	temp[0] = dsp->running ? 'Y' : 'N';
	temp[1] = '\n';

	return simple_read_from_buffer(user_buf, count, ppos, temp, 2);
}

static ssize_t wm_adsp_debugfs_wmfw_read(struct file *file,
					 char __user *user_buf,
					 size_t count, loff_t *ppos)
{
	struct wm_adsp *dsp = file->private_data;

	return wm_adsp_debugfs_string_read(dsp, user_buf, count, ppos,
					   dsp->wmfw_file_loaded);
}

static ssize_t wm_adsp_debugfs_bin_read(struct file *file,
					 char __user *user_buf,
					 size_t count, loff_t *ppos)
{
	struct wm_adsp *dsp = file->private_data;

	return wm_adsp_debugfs_string_read(dsp, user_buf, count, ppos,
					   dsp->bin_file_loaded);
}

static ssize_t wm_adsp_debugfs_fwid_read(struct file *file,
					 char __user *user_buf,
					 size_t count, loff_t *ppos)
{
	struct wm_adsp *dsp = file->private_data;

	return wm_adsp_debugfs_x32_read(dsp, user_buf, count, ppos,
					dsp->fw_id);
}

static ssize_t wm_adsp_debugfs_fwver_read(struct file *file,
					 char __user *user_buf,
					 size_t count, loff_t *ppos)
{
	struct wm_adsp *dsp = file->private_data;

	return wm_adsp_debugfs_x32_read(dsp, user_buf, count, ppos,
					dsp->fw_id_version);
}

static const struct {
	const char *name;
	const struct file_operations fops;
} wm_adsp_debugfs_fops[] = {
	{
		.name = "running",
		.fops = {
			.open = simple_open,
			.read = wm_adsp_debugfs_running_read,
		},
	},
	{
		.name = "wmfw_file",
		.fops = {
			.open = simple_open,
			.read = wm_adsp_debugfs_wmfw_read,
		},
	},
	{
		.name = "bin_file",
		.fops = {
			.open = simple_open,
			.read = wm_adsp_debugfs_bin_read,
		},
	},
	{
		.name = "fw_id",
		.fops = {
			.open = simple_open,
			.read = wm_adsp_debugfs_fwid_read,
		},
	},
	{
		.name = "fw_version",
		.fops = {
			.open = simple_open,
			.read = wm_adsp_debugfs_fwver_read,
		},
	},
};

void wm_adsp_init_debugfs(struct wm_adsp *dsp, struct snd_soc_codec *codec)
{
	struct dentry *root = NULL;
	char *root_name;
	int i;

	if (!codec->component.debugfs_root) {
		adsp_err(dsp, "No codec debugfs root\n");
		goto err;
	}

	root_name = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!root_name)
		goto err;

	snprintf(root_name, PAGE_SIZE, "dsp%d", dsp->num);
	root = debugfs_create_dir(root_name, codec->component.debugfs_root);
	kfree(root_name);

	if (!root)
		goto err;

	for (i = 0; i < ARRAY_SIZE(wm_adsp_debugfs_fops); ++i) {
		if (!debugfs_create_file(wm_adsp_debugfs_fops[i].name,
					 S_IRUGO, root, dsp,
					 &wm_adsp_debugfs_fops[i].fops))
			goto err;
	}

	dsp->debugfs_root = root;
	return;

err:
	debugfs_remove_recursive(root);
	adsp_err(dsp, "Failed to create debugfs\n");
}
EXPORT_SYMBOL_GPL(wm_adsp_init_debugfs);


void wm_adsp_cleanup_debugfs(struct wm_adsp *dsp)
{
	debugfs_remove_recursive(dsp->debugfs_root);
}
EXPORT_SYMBOL_GPL(wm_adsp_cleanup_debugfs);
#endif

MODULE_LICENSE("GPL v2");
