// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#define pr_fmt(fmt) "%s: " fmt, KBUILD_MODNAME

#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/soc/qcom/smem.h>
#include <linux/seq_buf.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/soc/qcom/sysmon_subsystem_stats.h>
#include <uapi/misc/sysmon_subsystem_stats_ioctl.h>

struct subsystem_stats {
	dev_t devno;
	struct device *dev;
	struct cdev sysmon_cdev;
	struct class *dev_class;
};
static struct subsystem_stats g_subsystem_stats;
#define SUBSYSTEMSTATS_CLASS_NAME_LOCAL                          "class_subsystem_stats"
#define SUBSYSTEMSTATS_DEVICE_NAME_LOCAL                         "sysmon_subsystem_stats"
#define SYSMON_SMEM_ID					634
#define SLEEPSTATS_SMEM_ID_ADSP			606
#define SLEEPSTATS_SMEM_ID_CDSP			607
#define SLEEPSTATS_SMEM_ID_SLPI			608
#define SLEEPSTATS_LPI_SMEM_ID			613
#define DSPPMSTATS_SMEM_ID				624
#define SYS_CLK_TICKS_PER_MS			19200
#define SYS_CLK_TICKS_PER_SEC			19200000
#define DSPPMSTATS_NUMPD				5
#define HMX_HVX_PMU_EVENTS_NA			0xFFFF

#define SYSMON_POWER_STATS_MAX_CLK_LEVELS_16 16

struct pd_clients {
	int pid;
	u32 num_active;
};

struct dsppm_stats {
	u32 version;
	u32 latency_us;
	u32 timestamp;
	struct pd_clients pd[DSPPMSTATS_NUMPD];
};

struct sysmon_smem_power_stats_local {
	u32 version;
	/**< Version */

	u32 clk_arr[SYSMON_POWER_STATS_MAX_CLK_LEVELS_16];
	/**< Core clock frequency(KHz) array */

	u32 active_time[SYSMON_POWER_STATS_MAX_CLK_LEVELS_16];
	/**< Active time(seconds) array corresponding to core clock array */

	u32 pc_time;
	/**< DSP LPM(Low Power Mode) time(seconds) */

	u32 lpi_time;
	/**< DSP LPI(Low Power Island Mode) time(seconds) */

	u32 island_time[SYSMON_POWER_STATS_MAX_CLK_LEVELS_16];
	/**< DSP LPI(Low Power Island Mode) time(seconds)
	 * array corresponding to core clock array
	 */

	u32 current_clk;
	/**< DSP current clock in KHz*/
};

struct sysmon_smem_power_stats_ext {
	u64 active_time_us[SYSMON_POWER_STATS_MAX_CLK_LEVELS_16];
	/**< Active time(micro seconds) array corresponding to core clock array */

	u64 pc_time_us;
	/**< DSP LPM(Low Power Mode) time(micro seconds) */

	u64 check_sum;
	/**< Checksum */

	u32 clk_arr[SYSMON_POWER_STATS_MAX_CLK_LEVELS_16];
	/**< Additional Core clock frequency(KHz) array (if clock levels more than
	 * SYSMON_POWER_STATS_MAX_CLK_LEVELS_16)
	 */

	u32 active_time[SYSMON_POWER_STATS_MAX_CLK_LEVELS_16];
	/**< Active time(seconds) array corresponding to additional core clock array */

	u32 island_time[SYSMON_POWER_STATS_MAX_CLK_LEVELS_16];
	/**< DSP LPI(Low Power Island Mode) time(seconds)
	 * array corresponding to additional core clock array
	 */

	u64 active_time_us2[SYSMON_POWER_STATS_MAX_CLK_LEVELS_16];
	/**< Active time(micro seconds) array corresponding to
	 * additional core clock array
	 */

	u64 island_time_us[SYSMON_POWER_STATS_MAX_CLK_LEVELS];
	/**< DSP LPI(Low Power Island Mode) time(micor seconds)
	 * array corresponding to core clock array
	 */

	u64 lpi_time_us;
	/**< DSP LPI(Low Power Island Mode) time(micro seconds) */

	u64 check_sum2;
	/**< Check sum */
};

struct sysmon_smem_power_stats_extended {
	struct sysmon_smem_power_stats_local powerstats;
	/**< Powerstats data */

	u32 last_update_time_powerstats_lsb;
	/**< Powerstats updated timestamp (lower significant 32 bits) */

	u32 last_update_time_powerstats_msb;
	/**< Powerstats updated timestamp (Most significant 32 bits) */

	struct sysmon_smem_power_stats_ext powerstats_ext;
	/**< Extended powerstats params */
};

struct sysmon_smem_q6_event_stats_local {
	u32 QDSP6_clk;
	/**< Q6 core clock in KHz */

	u32 Ab_vote_lsb;
	/**< Lower 32bits of the average bandwidth vote in bytes */

	u32 Ab_vote_msb;
	/**< Upper 32bits of the average bandwidth vote in bytes */

	u32 Ib_vote_lsb;
	/**< Lower 32bits of instantaneous bandwidth vote in bytes */

	u32 Ib_vote_msb;
	/**< Upper 32bits of instantaneous bandwidth vote in bytes */

	u32 Sleep_latency;
	/**< Sleep latency vote in micro-seconds */

};

struct sysmon_smem_q6_event_stats_ext {

	int HMX_Power_state;
	/**< HMX Power state 1 : HMX ON
	 *                   0 : HMX OFF
	 *                  -1 : not supported
	 */

	int HMX_clk;
	/**< HMX clock in KHz, -1 When not supported */
};

struct sysmon_smem_q6_event_stats_extended {
	u32 featureId_q6Event;
	/**< Feature [31:28] Size [27:16] version[15:0] */

	struct sysmon_smem_q6_event_stats_local event_stats;
	/*Structure type to hold DSP Q6 event based statistics*/

	uint32_t Last_update_time_event_lsb;
	/**< LSB of Smem update lsb timestamp for event based stat updates*/

	uint32_t Last_update_time_event_msb;
	/**< MSB of Smem updatemsb timestamp for event based stat updates*/

	struct sysmon_smem_q6_event_stats_ext event_stats_ext;
	/**< Extended DSP Q6 event stats */
};

struct sysmon_smem_ddr_stats_local {
	uint32_t clk_arr[SYSMON_DDR_STATS_MAX_CLK_LEVELS];
	/**< DDR clock frequency(kHz) array */

	uint32_t active_time[SYSMON_DDR_STATS_MAX_CLK_LEVELS];
	/**< Active time(seconds) array correspons to DDR clock array */

	uint32_t num_ddrclk_levels;
	/**< Number of levels present in DDR clock table */

	uint32_t pc_time;
	/**< DSP Power collapse time(seconds) */

	uint32_t curr_clk;
	/**< Current DDR clock frequency(kHz) */

	uint32_t reserved;
	/**< Reserved field for future use */

	uint64_t checksum;
	/**< Check sum */
};
struct sysmon_smem_ddr_stats_extended {
	u32 featureId_ddrStats;
	/**< FeatureID[31:28] Size[27:16] version[15:0] */

	u32 reserved;
	/**< For structure alignment */

	struct sysmon_smem_ddr_stats_local stats;
	/**< Structure to hold DDR stats */

	uint32_t Last_update_time_event_lsb;
	/**< LSB of Smem update lsb timestamp for event based stat updates*/

	uint32_t Last_update_time_event_msb;
	/**< MSB of Smem updatemsb timestamp for event based stat updates*/
};

struct sysmon_smem_stats {
	bool smem_init_adsp;
	bool smem_init_cdsp;
	bool smem_init_slpi;
	bool smem_init_cdsp_v2;
	struct sysmon_smem_power_stats_extended *sysmon_power_stats_adsp;
	struct sysmon_smem_power_stats_extended *sysmon_power_stats_cdsp;
	struct sysmon_smem_power_stats_extended *sysmon_power_stats_slpi;
	struct sysmon_smem_q6_event_stats_extended *sysmon_event_stats_adsp;
	struct sysmon_smem_q6_event_stats_extended *sysmon_event_stats_cdsp;
	struct sysmon_smem_q6_event_stats_extended *sysmon_event_stats_slpi;
	struct sysmon_smem_ddr_stats_extended *sysmon_ddr_stats;
	struct sleep_stats *sleep_stats_adsp;
	struct sleep_stats *sleep_stats_cdsp;
	struct sleep_stats *sleep_stats_slpi;
	struct sleep_stats_island *sleep_lpi_adsp;
	struct sleep_stats_island *sleep_lpi_slpi;
	u32 *q6_avg_load_adsp;
	u32 *q6_avg_load_cdsp;
	u32 *q6_avg_load_slpi;
	struct dsppm_stats *dsppm_stats_adsp;
	struct dsppm_stats *dsppm_stats_cdsp;
	struct dentry *debugfs_dir;
	struct dentry *debugfs_master_adsp_stats;
	struct dentry *debugfs_master_cdsp_stats;
	u32 *hmx_util;
	u32 *hvx_util;
};

enum feature_id {
	SYSMONSTATS_Q6_LOAD_FEATUREID = 1,
	SYSMONSTATS_Q6_EVENT_FEATUREID = 2,
	SYSMON_POWER_STATS_FEATUREID = 3,
	SYSMON_HMX_UTIL_FEATUREID = 4,
	SYSMON_HVX_UTIL_FEATUREID = 5,
	SYSMON_RUNNING_STATS_FEATUREID = 6,
	SYSMON_HMX_POWER_STATS_FEATUREID = 7,
	SYSMON_DDR_POWER_STATS_FEATUREID = 8
};

struct sysmon_smem_q6_load_stats {
	u32 q6load_avg;
	u32 Last_update_time_load_lsb;
	u32 Last_update_time_load_msb;
};

struct sysmon_smem_hvx_stats {
	u32 hvx_util_avg;
	u32 Last_update_time_load_lsb;
	u32 Last_update_time_load_msb;
};

struct sysmon_smem_hmx_stats {
	u32 hmx_util_avg;
	u32 Last_update_time_load_lsb;
	u32 Last_update_time_load_msb;
};

enum sub_id {
	HMX_CDSP,
	DDR_CDSP
};
struct delta_time {
	uint32_t active_time;
	uint32_t pc_time;
	uint32_t lpi_time;
};
static struct sysmon_smem_stats g_sysmon_stats;

static int get_delta_time(enum sub_id sub_id, struct delta_time *delta_time,
	u64 last_updated_time, u32 pc_time_updated, u32 lpi_time_updated)
{
	u64 lpm_acc = 0, lpi_acc = 0;
	u64 updated_ticks = 0, curr_timestamp = 0, diff_ticks = 0;
	u64 pc_time = 0, lpi_time = 0, active_time = 0, diff_time = 0;

	if (delta_time == NULL)
		return -EINVAL;

	if (sub_id == DDR_CDSP) {
		if (g_sysmon_stats.sleep_stats_cdsp) {
			lpm_acc = g_sysmon_stats.sleep_stats_cdsp->accumulated;
			if (g_sysmon_stats.sleep_stats_cdsp->last_entered_at >
					g_sysmon_stats.sleep_stats_cdsp->last_exited_at)
				lpm_acc += arch_timer_read_counter() -
					g_sysmon_stats.sleep_stats_cdsp->last_entered_at;
		} else
			return -EINVAL;
	}
	updated_ticks = (last_updated_time);
	curr_timestamp = __arch_counter_get_cntvct();

	if (curr_timestamp < updated_ticks)
		diff_ticks = curr_timestamp + (ULLONG_MAX - updated_ticks);
	else
		diff_ticks = (curr_timestamp - updated_ticks);

	diff_time = (diff_ticks / SYS_CLK_TICKS_PER_SEC);
	if (diff_time > 0) {
		pc_time = (lpm_acc / SYS_CLK_TICKS_PER_SEC);
		lpi_time = (lpi_acc / SYS_CLK_TICKS_PER_SEC);

		if (pc_time >= pc_time_updated)
			pc_time -= pc_time_updated;

		if (diff_time >= pc_time)
			active_time = diff_time - pc_time;

		if ((lpi_time) && (lpi_time >= lpi_time_updated))
			lpi_time -= lpi_time_updated;

		if ((lpi_time) && (pc_time >= lpi_time))
			pc_time -= lpi_time;
	}
	delta_time->active_time = active_time;
	delta_time->pc_time = pc_time;
	delta_time->lpi_time = lpi_time;
	return 0;
}

/* Adds delta between SMEM powerstats updated time to current time */
static int add_delta_time(
	u8 ver,
	u64 lpi_acc, u64 lpm_acc,
	struct sysmon_smem_power_stats *stats, enum dsp_id_t dsp_id
)
{
	u64 powerstats_ticks = 0, curr_timestamp = 0, diff_ticks = 0;
	u64 pc_time = 0, lpi_time = 0, active_time = 0, diff_time = 0;
	u8 j = 0;
	struct sysmon_smem_power_stats_extended *ptr = NULL;

	if (dsp_id == ADSP) {
		ptr = g_sysmon_stats.sysmon_power_stats_adsp;
	} else if (dsp_id == CDSP) {
		ptr = g_sysmon_stats.sysmon_power_stats_cdsp;
	} else if (dsp_id == SLPI) {
		ptr = g_sysmon_stats.sysmon_power_stats_slpi;
	} else {
		return -EINVAL;
	}

	if (ptr == NULL)
		return -EINVAL;

	if (ver >= 2) {
		powerstats_ticks = (u64)(((u64)ptr->last_update_time_powerstats_msb << 32) |
							ptr->last_update_time_powerstats_lsb);
		curr_timestamp = __arch_counter_get_cntvct();

		if (curr_timestamp < powerstats_ticks)
			diff_ticks = curr_timestamp + (ULLONG_MAX - powerstats_ticks);
		else
			diff_ticks = (curr_timestamp - powerstats_ticks);

		diff_time = (diff_ticks / SYS_CLK_TICKS_PER_SEC);

		if (diff_time > 0) {
			pc_time = (lpm_acc / SYS_CLK_TICKS_PER_SEC);
			lpi_time = (lpi_acc / SYS_CLK_TICKS_PER_SEC);

			if (pc_time >= stats->pc_time)
				pc_time -= stats->pc_time;

			if (diff_time >= pc_time)
				active_time = diff_time - pc_time;

			if ((lpi_time) && (lpi_time >= stats->lpi_time))
				lpi_time -= stats->lpi_time;

			if ((lpi_time) && (pc_time >= lpi_time))
				pc_time -= lpi_time;

			for (j = 0; j < SYSMON_POWER_STATS_MAX_CLK_LEVELS; j++) {
				if (stats->clk_arr[j] == stats->current_clk) {
					stats->active_time[j] += active_time;
					stats->island_time[j] += lpi_time;
					break;
				}
			}

			if (j >= SYSMON_POWER_STATS_MAX_CLK_LEVELS) {
				pr_err("%s: Current clock level %u KHz didn't match with any clock level\n",
					__func__, stats->current_clk);
				return -EINVAL;
			}
			stats->pc_time += pc_time;
			stats->lpi_time += lpi_time;
		}
	}

	return 0;
}

/*Updates SMEM pointers for all the sysmon Master stats*/
static void update_sysmon_smem_pointers(void *smem_pointer, enum dsp_id_t dsp_id, size_t size)
{
	u32 featureId;
	int feature, size_rcvd;
	int size_of_u32 = sizeof(u32);

	featureId = *(unsigned int *)smem_pointer;
	feature = featureId >> 28;
	size_rcvd = (featureId >> 16) & 0xFFF;

	while ((size > 0) && (size >= size_rcvd)) {
		switch (feature) {
		case SYSMON_POWER_STATS_FEATUREID:
			if (!IS_ERR_OR_NULL(smem_pointer + size_of_u32)) {
				if (dsp_id == ADSP)
					g_sysmon_stats.sysmon_power_stats_adsp =
					(struct sysmon_smem_power_stats_extended *)
					(smem_pointer);
				else if (dsp_id == CDSP)
					g_sysmon_stats.sysmon_power_stats_cdsp =
					(struct sysmon_smem_power_stats_extended *)
					(smem_pointer);
				else if (dsp_id == SLPI)
					g_sysmon_stats.sysmon_power_stats_slpi =
					(struct sysmon_smem_power_stats_extended *)
					(smem_pointer);
				else
					pr_err("%s: subsystem not found %d\n",
						__func__, SYSMON_POWER_STATS_FEATUREID);
			} else {
				pr_err("%s: Failed to fetch %d feature pointer\n",
					__func__, SYSMON_POWER_STATS_FEATUREID);
				size = 0;
			}
		break;
		case SYSMONSTATS_Q6_EVENT_FEATUREID:
			if (!IS_ERR_OR_NULL(smem_pointer + size_of_u32)) {
				if (dsp_id == ADSP)
					g_sysmon_stats.sysmon_event_stats_adsp =
					(struct sysmon_smem_q6_event_stats_extended *)
					(smem_pointer);
				else if (dsp_id == CDSP)
					g_sysmon_stats.sysmon_event_stats_cdsp =
					(struct sysmon_smem_q6_event_stats_extended *)
					(smem_pointer);
				else if (dsp_id == SLPI)
					g_sysmon_stats.sysmon_event_stats_slpi =
					(struct sysmon_smem_q6_event_stats_extended *)
					(smem_pointer);
				else
					pr_err("%s:subsystem not found %d\n",
					__func__, SYSMONSTATS_Q6_EVENT_FEATUREID);
			} else {
				pr_err("%s: Failed to fetch %d feature pointer\n",
					__func__, SYSMONSTATS_Q6_EVENT_FEATUREID);
				size = 0;
			}
		break;
		case SYSMONSTATS_Q6_LOAD_FEATUREID:
			if (!IS_ERR_OR_NULL(smem_pointer + size_of_u32)) {
				if (dsp_id == ADSP)
					g_sysmon_stats.q6_avg_load_adsp =
					(u32 *)(smem_pointer + size_of_u32);
				else if (dsp_id == CDSP)
					g_sysmon_stats.q6_avg_load_cdsp =
					(u32 *)(smem_pointer + size_of_u32);
				else if (dsp_id == SLPI)
					g_sysmon_stats.q6_avg_load_slpi =
					(u32 *)(smem_pointer + size_of_u32);
				else
					pr_err("%s:subsystem not found %d\n",
						__func__, SYSMONSTATS_Q6_LOAD_FEATUREID);
			} else {
				pr_err("%s: Failed to fetch %d feature pointer\n",
					__func__, SYSMONSTATS_Q6_LOAD_FEATUREID);
				size = 0;
			}
		break;
		case SYSMON_HMX_UTIL_FEATUREID:
			if (!IS_ERR_OR_NULL(smem_pointer + size_of_u32)) {
				if (dsp_id == CDSP) {
					g_sysmon_stats.hmx_util =
							(u32 *)(smem_pointer + size_of_u32);
				} else
					pr_err("%s:subsystem not supported %d\n",
						__func__, SYSMON_HMX_UTIL_FEATUREID);
			} else {
				pr_err("%s: Failed to fetch %d feature pointer\n",
					__func__, SYSMON_HMX_UTIL_FEATUREID);
				size = 0;
			}
		break;
		case SYSMON_HVX_UTIL_FEATUREID:
			if (!IS_ERR_OR_NULL(smem_pointer + size_of_u32)) {
				if (dsp_id == CDSP) {
					g_sysmon_stats.hvx_util =
							(u32 *)(smem_pointer + size_of_u32);
				} else
					pr_err("%s:subsystem not supported %d\n",
						__func__, SYSMON_HVX_UTIL_FEATUREID);
			} else {
				pr_err("%s: Failed to fetch %d feature pointer\n",
					__func__, SYSMON_HVX_UTIL_FEATUREID);
				size = 0;
			}
		break;
		case SYSMON_DDR_POWER_STATS_FEATUREID:
			if (!IS_ERR_OR_NULL(smem_pointer)) {
				if (dsp_id == CDSP) {
					g_sysmon_stats.sysmon_ddr_stats = smem_pointer;
				} else
					pr_err("%s:subsystem not supported %d stats\n",
						__func__, SYSMON_DDR_POWER_STATS_FEATUREID);
			} else {
				pr_err("%s: Failed to fetch %d feature pointer\n",
					__func__, SYSMON_DDR_POWER_STATS_FEATUREID);
				size = 0;
			}
		break;
		default:
			pr_err("%s: Requested feature not found, Feature:%u\n",
					__func__, feature);
		break;
		}
		if (!IS_ERR_OR_NULL(smem_pointer + size_rcvd)
					&& (size > size_rcvd)) {
			featureId = *(unsigned int *)(smem_pointer + size_rcvd);
			smem_pointer += size_rcvd;
			size = size - size_rcvd;
			feature = featureId >> 28;
			size_rcvd = (featureId >> 16) & 0xFFF;
		} else {
			size = 0;
		}
	}
}

static void sysmon_smem_init_adsp(void)
{
	size_t size;
	void *smem_pointer_adsp = NULL;

	g_sysmon_stats.smem_init_adsp = true;

	g_sysmon_stats.dsppm_stats_adsp = qcom_smem_get(ADSP,
						DSPPMSTATS_SMEM_ID,
						&size);

	if (IS_ERR_OR_NULL(g_sysmon_stats.dsppm_stats_adsp) ||
		(sizeof(struct dsppm_stats) > size)) {
		pr_err("%s:Failed to get fetch dsppm stats from SMEM for ADSP: %ld, size: %lu\n",
				__func__, PTR_ERR(g_sysmon_stats.dsppm_stats_adsp), size);
		g_sysmon_stats.smem_init_adsp = false;
	}

	g_sysmon_stats.sleep_stats_adsp = qcom_smem_get(ADSP,
						SLEEPSTATS_SMEM_ID_ADSP,
						&size);

	if (IS_ERR_OR_NULL(g_sysmon_stats.sleep_stats_adsp) ||
		(sizeof(struct sleep_stats) > size)) {
		pr_err("%s:Failed to get fetch sleep data from SMEM for ADSP: %ld, size: %lu\n",
				__func__, PTR_ERR(g_sysmon_stats.sleep_stats_adsp), size);
		g_sysmon_stats.smem_init_adsp = false;
	}

	g_sysmon_stats.sleep_lpi_adsp = qcom_smem_get(ADSP,
						SLEEPSTATS_LPI_SMEM_ID,
						&size);
	if (IS_ERR_OR_NULL(g_sysmon_stats.sleep_lpi_adsp) ||
		(sizeof(struct sleep_stats_island) > size)) {
		pr_err("%s:Failed to get fetch LPI sleep data from SMEM for ADSP: %ld, size: %lu\n",
				__func__, PTR_ERR(g_sysmon_stats.sleep_lpi_adsp), size);
		g_sysmon_stats.smem_init_adsp = false;
	}

	smem_pointer_adsp = qcom_smem_get(ADSP,
						SYSMON_SMEM_ID,
						&size);

	if (IS_ERR_OR_NULL(smem_pointer_adsp) || !size) {
		pr_err("%s:Failed to get fetch sysmon data from SMEM for ADSP: %ld, size: %lu\n",
				__func__, PTR_ERR(smem_pointer_adsp), size);
		g_sysmon_stats.smem_init_adsp = false;
	}

	update_sysmon_smem_pointers(smem_pointer_adsp, ADSP, size);

	if (IS_ERR_OR_NULL(g_sysmon_stats.sysmon_event_stats_adsp)) {

		pr_err("%s:Failed to get stats from SMEM for ADSP:\n"
				"event stats:%lx\n",
				__func__, PTR_ERR(g_sysmon_stats.sysmon_event_stats_adsp));
		g_sysmon_stats.smem_init_adsp = false;
	}

	if (IS_ERR_OR_NULL(g_sysmon_stats.sysmon_power_stats_adsp)) {

		pr_err("%s:Failed to get stats from SMEM for ADSP:\n"
				"power stats: %lx\n",
				__func__, PTR_ERR(g_sysmon_stats.sysmon_power_stats_adsp));
		g_sysmon_stats.smem_init_adsp = false;
	}

	if (IS_ERR_OR_NULL(g_sysmon_stats.q6_avg_load_adsp)) {

		pr_err("%s:Failed to get stats from SMEM for ADSP:\n"
				"q6_avg_load: %lx\n",
				__func__, PTR_ERR(g_sysmon_stats.q6_avg_load_adsp));
		g_sysmon_stats.smem_init_adsp = false;
	}
}

static void sysmon_smem_init_cdsp(void)
{
	size_t size;
	void *smem_pointer_cdsp = NULL;

	g_sysmon_stats.smem_init_cdsp = true;
	g_sysmon_stats.smem_init_cdsp_v2 = true;

	g_sysmon_stats.dsppm_stats_cdsp = qcom_smem_get(CDSP,
						DSPPMSTATS_SMEM_ID,
						&size);

	if (IS_ERR_OR_NULL(g_sysmon_stats.dsppm_stats_cdsp) ||
		(sizeof(struct dsppm_stats) > size)) {
		pr_err("%s:Failed to get fetch dsppm stats from SMEM for CDSP: %ld, size: %lu\n",
				__func__, PTR_ERR(g_sysmon_stats.dsppm_stats_cdsp), size);
		g_sysmon_stats.smem_init_cdsp = false;
	}

	g_sysmon_stats.sleep_stats_cdsp = qcom_smem_get(CDSP,
						SLEEPSTATS_SMEM_ID_CDSP,
						&size);

	if (IS_ERR_OR_NULL(g_sysmon_stats.sleep_stats_cdsp) ||
		(sizeof(struct sleep_stats) > size)) {
		pr_err("%s:Failed to get fetch sleep data from SMEM for CDSP: %ld, size: %lu\n",
				__func__, PTR_ERR(g_sysmon_stats.sleep_stats_cdsp), size);
		g_sysmon_stats.smem_init_cdsp = false;
	}

	smem_pointer_cdsp = qcom_smem_get(CDSP,
							SYSMON_SMEM_ID,
							&size);
	if (IS_ERR_OR_NULL(smem_pointer_cdsp) || !size) {
		pr_err("%s:Failed to get fetch data from SMEM for CDSP: %ld, size: %lu\n",
				__func__, PTR_ERR(smem_pointer_cdsp), size);
		g_sysmon_stats.smem_init_cdsp = false;
	}

	update_sysmon_smem_pointers(smem_pointer_cdsp, CDSP, size);

	if (IS_ERR_OR_NULL(g_sysmon_stats.sysmon_event_stats_cdsp)) {

		pr_err("%s:Failed to get stats from SMEM for CDSP:\n"
				"event stats:%lx\n",
				__func__, PTR_ERR(g_sysmon_stats.sysmon_event_stats_cdsp));
		g_sysmon_stats.smem_init_cdsp = false;
	}

	if (IS_ERR_OR_NULL(g_sysmon_stats.sysmon_power_stats_cdsp)) {

		pr_err("%s:Failed to get stats from SMEM for CDSP:\n"
				" power stats: %lx\n",
				__func__, PTR_ERR(g_sysmon_stats.sysmon_power_stats_cdsp));
		g_sysmon_stats.smem_init_cdsp = false;
	}

	if (IS_ERR_OR_NULL(g_sysmon_stats.q6_avg_load_cdsp)) {

		pr_err("%s:Failed to get stats from SMEM for CDSP:\n"
				"q6_avg_load: %lx\n",
				__func__, PTR_ERR(g_sysmon_stats.q6_avg_load_cdsp));
		g_sysmon_stats.smem_init_cdsp = false;
	}

	if (IS_ERR_OR_NULL(g_sysmon_stats.hmx_util)) {

		pr_err("%s:Failed to get stats from SMEM for CDSP:\n"
				"hmx_util: %lx\n",
				__func__, PTR_ERR(g_sysmon_stats.hmx_util));
		g_sysmon_stats.smem_init_cdsp_v2 = false;
	}

	if (IS_ERR_OR_NULL(g_sysmon_stats.hvx_util)) {

		pr_err("%s:Failed to get stats from SMEM for CDSP:\n"
				"hmx_util: %lx\n",
				__func__, PTR_ERR(g_sysmon_stats.hvx_util));
		g_sysmon_stats.smem_init_cdsp_v2 = false;
	}

}
static void sysmon_smem_init_slpi(void)
{
	size_t size;
	void *smem_pointer_slpi = NULL;

	g_sysmon_stats.smem_init_slpi = true;

	g_sysmon_stats.sleep_stats_slpi = qcom_smem_get(SLPI,
						SLEEPSTATS_SMEM_ID_SLPI,
						&size);

	if (IS_ERR_OR_NULL(g_sysmon_stats.sleep_stats_slpi) ||
		(sizeof(struct sleep_stats) > size)) {
		pr_err("%s:Failed to get fetch sleep data from SMEM for SLPI: %ld, size: %lu\n",
				__func__, PTR_ERR(g_sysmon_stats.sleep_stats_slpi), size);
		g_sysmon_stats.smem_init_slpi = false;
	}

	g_sysmon_stats.sleep_lpi_slpi = qcom_smem_get(SLPI,
						SLEEPSTATS_LPI_SMEM_ID,
						&size);
	if (IS_ERR_OR_NULL(g_sysmon_stats.sleep_lpi_slpi) ||
		(sizeof(struct sleep_stats_island) > size)) {
		pr_err("%s:Failed to get fetch LPI sleep data from SMEM for SLPI: %ld, size: %lu\n",
				__func__, PTR_ERR(g_sysmon_stats.sleep_lpi_slpi), size);
		g_sysmon_stats.smem_init_slpi = false;
	}

	smem_pointer_slpi = qcom_smem_get(SLPI,
							SYSMON_SMEM_ID,
							&size);

	if (IS_ERR_OR_NULL(smem_pointer_slpi) || !size) {
		pr_err("%s:Failed to get fetch data from SMEM for SLPI: %ld, size: %lu\n",
				__func__, PTR_ERR(smem_pointer_slpi), size);
	}

	update_sysmon_smem_pointers(smem_pointer_slpi, SLPI, size);

	if (IS_ERR_OR_NULL(g_sysmon_stats.sysmon_event_stats_slpi)) {

		pr_err("%s:Failed to get stats from SMEM for SLPI:\n"
				"event stats:%lx\n",
				__func__, PTR_ERR(g_sysmon_stats.sysmon_event_stats_slpi));
		g_sysmon_stats.smem_init_slpi = false;
	}

	if (IS_ERR_OR_NULL(g_sysmon_stats.sysmon_power_stats_slpi)) {

		pr_err("%s:Failed to get stats from SMEM for SLPI:\n"
				"power stats: %lx\n",
				__func__, PTR_ERR(g_sysmon_stats.sysmon_power_stats_slpi));
		g_sysmon_stats.smem_init_slpi = false;
	}

	if (IS_ERR_OR_NULL(g_sysmon_stats.q6_avg_load_slpi)) {

		pr_err("%s:Failed to get stats from SMEM for SLPI:\n"
				"q6_avg_load: %lx\n",
				__func__, PTR_ERR(g_sysmon_stats.q6_avg_load_slpi));
		g_sysmon_stats.smem_init_slpi = false;
	}
}
/**
 * sysmon_read_hmx_util() - Checks for CDSP power collapse
 * and resets the HMX utilization to zero if dsp is power collapsed.
 */
static u32 sysmon_read_hmx_util(void)
{
	struct sysmon_smem_hmx_stats sysmon_hmx_util;
	u64 curr_timestamp = __arch_counter_get_cntvct();
	u64 last_hmx_update_at = 0;

	memcpy(&sysmon_hmx_util,
		g_sysmon_stats.hmx_util,
		sizeof(struct sysmon_smem_hmx_stats));

	last_hmx_update_at =
		(u64) (((u64)sysmon_hmx_util.Last_update_time_load_msb << 32)|
				sysmon_hmx_util.Last_update_time_load_lsb);

	if ((curr_timestamp > last_hmx_update_at) &&
		((curr_timestamp - last_hmx_update_at) / SYS_CLK_TICKS_PER_MS) > 100) {
		sysmon_hmx_util.hmx_util_avg = 0;
	}

	if (sysmon_hmx_util.hmx_util_avg == HMX_HVX_PMU_EVENTS_NA)
		pr_err("HMX PMU not registered, user ovveride pmu counter\n");

	return sysmon_hmx_util.hmx_util_avg;
}

/**
 * sysmon_stats_query_hmx_utlization() - * API to query
 * CDSP hmx utlization.On success, returns HMX utilization
 * in the hmx_util parameter.
 * @arg1: u32 pointer to HMX utilization in percentage.
 * @return: SUCCESS (0) if Query is successful
 *        FAILURE (Non-zero) if Query could not be processed, refer error codes.
 */
int sysmon_stats_query_hmx_utlization(u32 *hmx_util)
{
	u32 hmx_utilization = 0;
	int ret = 0;

	if (!g_sysmon_stats.smem_init_cdsp_v2)
		sysmon_smem_init_cdsp();

	if (!IS_ERR_OR_NULL(g_sysmon_stats.hmx_util)) {
		hmx_utilization = sysmon_read_hmx_util();

		if (hmx_utilization == HMX_HVX_PMU_EVENTS_NA) {
			hmx_utilization = 0;
			ret = DSP_PMU_COUNTER_NA;
		}

		memcpy(hmx_util, &hmx_utilization, sizeof(u32));
	} else
		return -ENOKEY;

	return ret;
}
EXPORT_SYMBOL_GPL(sysmon_stats_query_hmx_utlization);
/**
 * sysmon_read_hvx_util() - Checks for CDSP power collapse
 * and resets the HVX utilization to zero if dsp is power collapsed.
 */
static u32 sysmon_read_hvx_util(void)
{
	struct sysmon_smem_hvx_stats sysmon_hvx_util;
	u64 curr_timestamp = __arch_counter_get_cntvct();
	u64 last_hvx_update_at = 0;

	memcpy(&sysmon_hvx_util,
		g_sysmon_stats.hvx_util,
		sizeof(struct sysmon_smem_hvx_stats));

	last_hvx_update_at =
		(u64) (((u64)sysmon_hvx_util.Last_update_time_load_msb << 32)|
				sysmon_hvx_util.Last_update_time_load_lsb);

	if ((curr_timestamp > last_hvx_update_at) &&
		((curr_timestamp - last_hvx_update_at) / SYS_CLK_TICKS_PER_MS) > 100) {
		sysmon_hvx_util.hvx_util_avg = 0;
	}

	if (sysmon_hvx_util.hvx_util_avg == HMX_HVX_PMU_EVENTS_NA)
		pr_err("HVX PMU not registered, user ovveride pmu counter\n");

	return sysmon_hvx_util.hvx_util_avg;
}

/**
 * sysmon_stats_query_hvx_utlization() - * API to query
 * CDSP subsystem hvx utlization.On success, returns HVX utilization
 * in the hvx_util parameter.
 * @arg1: u32 pointer to HVX utilization in percentage.
 * @return: SUCCESS (0) if Query is successful
 *        FAILURE (Non-zero) if Query could not be processed, refer error codes.
 */
int sysmon_stats_query_hvx_utlization(u32 *hvx_util)
{
	u32 hvx_utilization = 0;
	int ret = 0;

	if (!g_sysmon_stats.smem_init_cdsp_v2)
		sysmon_smem_init_cdsp();

	if (!IS_ERR_OR_NULL(g_sysmon_stats.hvx_util)) {
		hvx_utilization = sysmon_read_hvx_util();

		if (hvx_utilization == HMX_HVX_PMU_EVENTS_NA) {
			hvx_utilization = 0;
			ret = DSP_PMU_COUNTER_NA;
		}

		memcpy(hvx_util, &hvx_utilization, sizeof(u32));
	} else
		return -ENOKEY;

	return ret;
}
EXPORT_SYMBOL_GPL(sysmon_stats_query_hvx_utlization);

int copy_powerstats(struct sysmon_smem_power_stats *out_ptr, enum dsp_id_t dsp_id)
{
	const struct sysmon_smem_power_stats_extended *in_ptr = NULL;
	int ver = 0;
	int i = 0, j = 0;

	if (dsp_id == ADSP)
		in_ptr = g_sysmon_stats.sysmon_power_stats_adsp;
	else if (dsp_id == CDSP)
		in_ptr = g_sysmon_stats.sysmon_power_stats_cdsp;
	else if (dsp_id == SLPI)
		in_ptr = g_sysmon_stats.sysmon_power_stats_slpi;
	else {
		pr_err("%s: Stats not found for dsp(%d)\n", __func__, dsp_id);
		return 1;
	}
	out_ptr->version = in_ptr->powerstats.version & 0xFF;
	ver = in_ptr->powerstats.version & 0xFF;
	out_ptr->pc_time = in_ptr->powerstats.pc_time;
	out_ptr->lpi_time = in_ptr->powerstats.lpi_time;
	if (ver >= 2)
		out_ptr->current_clk = in_ptr->powerstats.current_clk;

	for (i = 0; i < SYSMON_POWER_STATS_MAX_CLK_LEVELS_16; i++) {
		out_ptr->clk_arr[i] = in_ptr->powerstats.clk_arr[i];
		out_ptr->active_time[i] = in_ptr->powerstats.active_time[i];
		if ((ver >= 2) && ((dsp_id == ADSP) || (dsp_id == SLPI)))
			out_ptr->island_time[i] = in_ptr->powerstats.island_time[i];
	}
	if (ver >= 4) {
		for (i = SYSMON_POWER_STATS_MAX_CLK_LEVELS_16;
			i < SYSMON_POWER_STATS_MAX_CLK_LEVELS; i++) {
			j = i - SYSMON_POWER_STATS_MAX_CLK_LEVELS_16;
			out_ptr->clk_arr[i] = in_ptr->powerstats_ext.clk_arr[j];
			out_ptr->active_time[i] = in_ptr->powerstats_ext.active_time[j];
			if (dsp_id == ADSP)
				out_ptr->island_time[i] = in_ptr->powerstats_ext.island_time[j];
		}
	}
	return 0;
}

/**
 * sysmon_stats_query_ddr_residency() - * API to query requested
 * DDR power residency.
 * On success, returns 0 and copies power residency
 *      statistics in the given sysmon_smem_ddr_stats structure.
 */
int sysmon_stats_query_ddr_residency(struct sysmon_smem_ddr_stats *sysmon_ddr_stats)
{
	struct delta_time delta = {0};
	uint32_t dtime = 0, j = 0;
	struct sysmon_smem_ddr_stats_extended *stats = g_sysmon_stats.sysmon_ddr_stats;

	if (!sysmon_ddr_stats) {
		pr_err("%s: Null pointer received\n", __func__);
		return -EINVAL;
	}

	if (g_sysmon_stats.sysmon_ddr_stats) {
		if (get_delta_time(DDR_CDSP, &delta,
				(u64)(((u64)stats->Last_update_time_event_msb << 32)
				| stats->Last_update_time_event_lsb),
				stats->stats.pc_time, 0))
			return -ENOKEY;

		if (stats->stats.num_ddrclk_levels > SYSMON_DDR_STATS_MAX_CLK_LEVELS) {
			pr_err("%s: Number of DDR clock levels (%d) are more than expected %d\n",
				__func__, stats->stats.num_ddrclk_levels,
				SYSMON_DDR_STATS_MAX_CLK_LEVELS);
			return -EINVAL;
		}

		for (j = 0; j < stats->stats.num_ddrclk_levels; j++) {
			if (stats->stats.clk_arr[j]) {
				if (stats->stats.clk_arr[j] == stats->stats.curr_clk)
					dtime = delta.active_time;
				else
					dtime = 0;

				sysmon_ddr_stats->clk_arr[j] = stats->stats.clk_arr[j];
				sysmon_ddr_stats->active_time[j] =
						stats->stats.active_time[j] + dtime;
			}
		}
		sysmon_ddr_stats->pc_time = stats->stats.pc_time + delta.pc_time;
		sysmon_ddr_stats->curr_clk = stats->stats.curr_clk;
		sysmon_ddr_stats->num_ddrclk_levels = stats->stats.num_ddrclk_levels;
	} else
		return -ENOKEY;
	return 0;
}
EXPORT_SYMBOL_GPL(sysmon_stats_query_ddr_residency);

/**
 * sysmon_stats_query_power_residency() - * API to query requested
 * DSP subsystem power residency.On success, returns power residency
 * statistics in the given sysmon_smem_power_stats structure.
 */
int sysmon_stats_query_power_residency(enum dsp_id_t dsp_id,
					struct sysmon_smem_power_stats *sysmon_power_stats)
{
	int ret = 0, size = 0;
	u64 lpi_accumulated = 0;
	u64 lpm_accumulated = 0;
	struct sysmon_smem_power_stats_extended *ptr = NULL;
	struct sysmon_smem_power_stats smem_sysmon_power_stats_l = { 0 };

	if (!sysmon_power_stats) {
		pr_err("%s: Null pointer received\n", __func__);
		return -EINVAL;
	}

	switch (dsp_id) {
	case ADSP:
		if (!g_sysmon_stats.smem_init_adsp)
			sysmon_smem_init_adsp();

		if (!IS_ERR_OR_NULL(g_sysmon_stats.sysmon_power_stats_adsp)) {
			ptr = g_sysmon_stats.sysmon_power_stats_adsp;
			size = (((ptr->powerstats.version) >> 16) & 0xFFF) + sizeof(u32);
			if (copy_powerstats(&smem_sysmon_power_stats_l, dsp_id))
				return -ENOKEY;

			if (g_sysmon_stats.sleep_stats_adsp) {
				lpm_accumulated = g_sysmon_stats.sleep_stats_adsp->accumulated;
				if (g_sysmon_stats.sleep_stats_adsp->last_entered_at >
						g_sysmon_stats.sleep_stats_adsp->last_exited_at)
					lpm_accumulated += arch_timer_read_counter() -
						g_sysmon_stats.sleep_stats_adsp->last_entered_at;
			}

			if (g_sysmon_stats.sleep_lpi_adsp) {
				lpi_accumulated = g_sysmon_stats.sleep_lpi_adsp->accumulated;
				if (g_sysmon_stats.sleep_lpi_adsp->last_entered_at >
					g_sysmon_stats.sleep_lpi_adsp->last_exited_at)
					lpi_accumulated += arch_timer_read_counter() -
						g_sysmon_stats.sleep_lpi_adsp->last_entered_at;
			}
		} else
			ret = -ENOKEY;
	break;
	case CDSP:
		if (!g_sysmon_stats.smem_init_cdsp)
			sysmon_smem_init_cdsp();

		if (!IS_ERR_OR_NULL(g_sysmon_stats.sysmon_power_stats_cdsp)) {
			ptr = g_sysmon_stats.sysmon_power_stats_cdsp;
			size = (((ptr->powerstats.version) >> 16) & 0xFFF) + sizeof(u32);
			if (copy_powerstats(&smem_sysmon_power_stats_l, dsp_id))
				return -ENOKEY;

			if (g_sysmon_stats.sleep_stats_cdsp) {
				lpm_accumulated = g_sysmon_stats.sleep_stats_cdsp->accumulated;
				if (g_sysmon_stats.sleep_stats_cdsp->last_entered_at >
						g_sysmon_stats.sleep_stats_cdsp->last_exited_at)
					lpm_accumulated += arch_timer_read_counter() -
						g_sysmon_stats.sleep_stats_cdsp->last_entered_at;
			}
		} else
			ret = -ENOKEY;
	break;
	case SLPI:
		if (!g_sysmon_stats.smem_init_slpi)
			sysmon_smem_init_slpi();

		if (!IS_ERR_OR_NULL(g_sysmon_stats.sysmon_power_stats_slpi)) {
			ptr = g_sysmon_stats.sysmon_power_stats_slpi;
			size = (((ptr->powerstats.version) >> 16) & 0xFFF) + sizeof(u32);
			if (copy_powerstats(&smem_sysmon_power_stats_l, dsp_id))
				return -ENOKEY;
			if (g_sysmon_stats.sleep_stats_slpi) {
				lpm_accumulated = g_sysmon_stats.sleep_stats_slpi->accumulated;
				if (g_sysmon_stats.sleep_stats_slpi->last_entered_at >
					g_sysmon_stats.sleep_stats_slpi->last_exited_at)
					lpm_accumulated += arch_timer_read_counter() -
						g_sysmon_stats.sleep_stats_slpi->last_entered_at;
			}
			if (g_sysmon_stats.sleep_lpi_slpi) {
				lpi_accumulated = g_sysmon_stats.sleep_lpi_slpi->accumulated;
				if (g_sysmon_stats.sleep_lpi_slpi->last_entered_at >
					g_sysmon_stats.sleep_lpi_slpi->last_exited_at)
					lpi_accumulated += arch_timer_read_counter() -
						g_sysmon_stats.sleep_lpi_slpi->last_entered_at;
			}
		} else
			ret = -ENOKEY;
	break;
	default:
		pr_err("%s:Provided subsystem %d is not supported\n", __func__, dsp_id);
		ret = -EINVAL;
	break;
	}

	if (ret == 0) {
		ret = add_delta_time((ptr->powerstats.version) & 0xFF, lpi_accumulated,
				lpm_accumulated, &smem_sysmon_power_stats_l, dsp_id);
		if (ret == 0) {
			memcpy(sysmon_power_stats, &smem_sysmon_power_stats_l,
				sizeof(struct sysmon_smem_power_stats));
			sysmon_power_stats->version = (ptr->powerstats.version) & 0xFF;
		}
	}

	return ret;
}
EXPORT_SYMBOL_GPL(sysmon_stats_query_power_residency);
/**
 * API to query requested DSP subsystem's Q6 clock and bandwidth.
 * On success, returns Q6 clock and bandwidth statistics in the given
 * sysmon_smem_q6_event_stats structure.
 */
int sysmon_stats_query_q6_votes(enum dsp_id_t dsp_id,
		struct sysmon_smem_q6_event_stats *sysmon_q6_event_stats)
{
	int ret = 0;
	int ver = 0;
	struct sysmon_smem_q6_event_stats_extended *events_ptr = NULL;

	if (!sysmon_q6_event_stats) {
		pr_err("%s: Null pointer received\n", __func__);
		return -EINVAL;
	}

	switch (dsp_id) {
	case ADSP:
		if (!g_sysmon_stats.smem_init_adsp)
			sysmon_smem_init_adsp();

		if (!IS_ERR_OR_NULL(g_sysmon_stats.sysmon_event_stats_adsp)) {
			events_ptr = g_sysmon_stats.sysmon_event_stats_adsp;
			sysmon_q6_event_stats->QDSP6_clk =
				events_ptr->event_stats.QDSP6_clk;
			sysmon_q6_event_stats->Ab_vote_lsb =
				events_ptr->event_stats.Ab_vote_lsb;
			sysmon_q6_event_stats->Ab_vote_msb =
				events_ptr->event_stats.Ab_vote_msb;
			sysmon_q6_event_stats->Ib_vote_lsb =
				events_ptr->event_stats.Ib_vote_lsb;
			sysmon_q6_event_stats->Ib_vote_msb =
				events_ptr->event_stats.Ib_vote_msb;
			sysmon_q6_event_stats->Sleep_latency =
				events_ptr->event_stats.Sleep_latency;
			sysmon_q6_event_stats->HMX_Power_state = -1;
			sysmon_q6_event_stats->HMX_clk = -1;
		}
		else
			ret = -ENOKEY;
	break;
	case CDSP:
		if (!g_sysmon_stats.smem_init_cdsp)
			sysmon_smem_init_cdsp();

		if (!IS_ERR_OR_NULL(g_sysmon_stats.sysmon_event_stats_cdsp)) {
			events_ptr = g_sysmon_stats.sysmon_event_stats_cdsp;
			ver = events_ptr->featureId_q6Event & 0xFFFF;
			sysmon_q6_event_stats->QDSP6_clk =
				events_ptr->event_stats.QDSP6_clk;
			sysmon_q6_event_stats->Ab_vote_lsb =
				events_ptr->event_stats.Ab_vote_lsb;
			sysmon_q6_event_stats->Ab_vote_msb =
				events_ptr->event_stats.Ab_vote_msb;
			sysmon_q6_event_stats->Ib_vote_lsb =
				events_ptr->event_stats.Ib_vote_lsb;
			sysmon_q6_event_stats->Ib_vote_msb =
				events_ptr->event_stats.Ib_vote_msb;
			sysmon_q6_event_stats->Sleep_latency =
				events_ptr->event_stats.Sleep_latency;
			if (ver > 1) {
				sysmon_q6_event_stats->HMX_Power_state =
					events_ptr->event_stats_ext.HMX_Power_state;
				sysmon_q6_event_stats->HMX_clk =
					events_ptr->event_stats_ext.HMX_clk;
			} else {
				sysmon_q6_event_stats->HMX_Power_state = -1;
				sysmon_q6_event_stats->HMX_clk = -1;
			}
		}
		else
			ret = -ENOKEY;
	break;
	case SLPI:
		if (!g_sysmon_stats.smem_init_slpi)
			sysmon_smem_init_slpi();

		if (!IS_ERR_OR_NULL(g_sysmon_stats.sysmon_event_stats_slpi)) {
			events_ptr = g_sysmon_stats.sysmon_event_stats_slpi;
			sysmon_q6_event_stats->QDSP6_clk =
				events_ptr->event_stats.QDSP6_clk;
			sysmon_q6_event_stats->Ab_vote_lsb =
				events_ptr->event_stats.Ab_vote_lsb;
			sysmon_q6_event_stats->Ab_vote_msb =
				events_ptr->event_stats.Ab_vote_msb;
			sysmon_q6_event_stats->Ib_vote_lsb =
				events_ptr->event_stats.Ib_vote_lsb;
			sysmon_q6_event_stats->Ib_vote_msb =
				events_ptr->event_stats.Ib_vote_msb;
			sysmon_q6_event_stats->Sleep_latency =
				events_ptr->event_stats.Sleep_latency;
			sysmon_q6_event_stats->HMX_Power_state = -1;
			sysmon_q6_event_stats->HMX_clk = -1;
		}
		else
			ret = -ENOKEY;
	break;
	default:
		pr_err("%s:Provided subsystem %d is not supported\n", __func__, dsp_id);
		ret = -EINVAL;
	break;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(sysmon_stats_query_q6_votes);

/*Checks for DSP power collapse and resets the q6 average
 *load to zero if dsp is power collapsed.
 */
u32 sysmon_read_q6_load(enum dsp_id_t dsp_id)
{
	struct sysmon_smem_q6_load_stats sysmon_q6_load;
	u64 last_q6load_update_at = 0;
	u64 curr_timestamp = __arch_counter_get_cntvct();

	switch (dsp_id) {
	case ADSP:
		memcpy(&sysmon_q6_load,
			g_sysmon_stats.q6_avg_load_adsp,
			sizeof(struct sysmon_smem_q6_load_stats));

		last_q6load_update_at =
			(u64) (((u64)sysmon_q6_load.Last_update_time_load_msb<<32)|
					sysmon_q6_load.Last_update_time_load_lsb);

		if ((curr_timestamp > last_q6load_update_at) &&
			((curr_timestamp - last_q6load_update_at) / SYS_CLK_TICKS_PER_MS) > 100) {
			sysmon_q6_load.q6load_avg = 0;
		}
	break;
	case CDSP:
		memcpy(&sysmon_q6_load,
			g_sysmon_stats.q6_avg_load_cdsp,
			sizeof(struct sysmon_smem_q6_load_stats));

		last_q6load_update_at =
			(u64) (((u64)sysmon_q6_load.Last_update_time_load_msb << 32)|
					sysmon_q6_load.Last_update_time_load_lsb);

		if ((curr_timestamp > last_q6load_update_at) &&
			((curr_timestamp - last_q6load_update_at) / SYS_CLK_TICKS_PER_MS) > 100) {
			sysmon_q6_load.q6load_avg = 0;
		}
	break;
	case SLPI:
		memcpy(&sysmon_q6_load,
			g_sysmon_stats.q6_avg_load_slpi,
			sizeof(struct sysmon_smem_q6_load_stats));

		last_q6load_update_at =
			(u64) (((u64)sysmon_q6_load.Last_update_time_load_msb << 32)|
						sysmon_q6_load.Last_update_time_load_lsb);

		if ((curr_timestamp > last_q6load_update_at) &&
			((curr_timestamp - last_q6load_update_at) / SYS_CLK_TICKS_PER_MS) > 100) {
			sysmon_q6_load.q6load_avg = 0;
		}
	break;
	default:
		pr_err("%s:Provided subsystem %d is not supported\n", __func__, dsp_id);
		return -EINVAL;
	break;
	}

	return sysmon_q6_load.q6load_avg;
}

/**
 * API to query requested DSP subsystem's Q6 load.
 * On success, returns average Q6 load in KCPS for the given
 * q6load_avg parameter.
 */
int sysmon_stats_query_q6_load(enum dsp_id_t dsp_id, u32 *q6load_avg)
{
	int ret = 0;
	u32 q6_average_load = 0;

	if (!q6load_avg) {
		pr_err("%s: Null pointer received\n", __func__);
		return -EINVAL;
	}

	switch (dsp_id) {
	case ADSP:
		if (!g_sysmon_stats.smem_init_adsp)
			sysmon_smem_init_adsp();

		if (!IS_ERR_OR_NULL(g_sysmon_stats.q6_avg_load_adsp)) {
			q6_average_load = sysmon_read_q6_load(ADSP);
			memcpy(q6load_avg, &q6_average_load, sizeof(u32));
		} else
			ret = -ENOKEY;
	break;
	case CDSP:
		if (!g_sysmon_stats.smem_init_cdsp)
			sysmon_smem_init_cdsp();

		if (!IS_ERR_OR_NULL(g_sysmon_stats.q6_avg_load_cdsp)) {
			q6_average_load = sysmon_read_q6_load(CDSP);
			memcpy(q6load_avg, &q6_average_load, sizeof(u32));
		} else
			ret = -ENOKEY;
	break;
	case SLPI:
		if (!g_sysmon_stats.smem_init_slpi)
			sysmon_smem_init_slpi();

		if (!IS_ERR_OR_NULL(g_sysmon_stats.q6_avg_load_slpi)) {
			q6_average_load = sysmon_read_q6_load(SLPI);
			memcpy(q6load_avg, &q6_average_load, sizeof(u32));
		} else
			ret = -ENOKEY;
	break;
	default:
		pr_err("%s:Provided subsystem %d is not supported\n", __func__, dsp_id);
		ret = -EINVAL;
	break;
	}

	return ret;

}
EXPORT_SYMBOL_GPL(sysmon_stats_query_q6_load);
/*
 * API to query requested DSP subsystem sleep stats for
 * LPM and LPI.On success, returns sleep
 * statistics in the given sleep_stats structure for LPM and
 * LPI(on supported subsystems).
 */
int sysmon_stats_query_sleep(enum dsp_id_t dsp_id,
					struct sleep_stats *sleep_stats_lpm,
					struct sleep_stats_island *sleep_stats_lpi)
{
	int ret = 0;

	if (!sleep_stats_lpm && !sleep_stats_lpi) {
		pr_err("%s: Null pointer received\n", __func__);
		return -EINVAL;
	}
	switch (dsp_id) {
	case ADSP:
		if (!g_sysmon_stats.smem_init_adsp)
			sysmon_smem_init_adsp();
		/*
		 * If a subsystem is in sleep when reading the sleep stats adjust
		 * the accumulated sleep duration to show actual sleep time.
		 */
		if (sleep_stats_lpm) {
			if (!IS_ERR_OR_NULL(g_sysmon_stats.sleep_stats_adsp)) {
				if (g_sysmon_stats.sleep_stats_adsp->last_entered_at >
					g_sysmon_stats.sleep_stats_adsp->last_exited_at)
					g_sysmon_stats.sleep_stats_adsp->accumulated
						+= arch_timer_read_counter() -
						g_sysmon_stats.sleep_stats_adsp->last_entered_at;

				memcpy(sleep_stats_lpm, g_sysmon_stats.sleep_stats_adsp,
							sizeof(struct sleep_stats));
			} else
				ret = -ENOKEY;
		}

		if (sleep_stats_lpi) {
			if (!IS_ERR_OR_NULL(g_sysmon_stats.sleep_lpi_adsp)) {
				if (g_sysmon_stats.sleep_lpi_adsp->last_entered_at >
					g_sysmon_stats.sleep_lpi_adsp->last_exited_at)
					g_sysmon_stats.sleep_lpi_adsp->accumulated
						+= arch_timer_read_counter() -
						g_sysmon_stats.sleep_lpi_adsp->last_entered_at;

				memcpy(sleep_stats_lpi, g_sysmon_stats.sleep_lpi_adsp,
							sizeof(struct sleep_stats_island));
			} else
				ret = -ENOKEY;
		}
	break;
	case CDSP:
		if (!g_sysmon_stats.smem_init_cdsp)
			sysmon_smem_init_cdsp();

		if (sleep_stats_lpm) {
			if (!IS_ERR_OR_NULL(g_sysmon_stats.sleep_stats_cdsp)) {
				if (g_sysmon_stats.sleep_stats_cdsp->last_entered_at >
					g_sysmon_stats.sleep_stats_cdsp->last_exited_at)
					g_sysmon_stats.sleep_stats_cdsp->accumulated
						+= arch_timer_read_counter() -
						g_sysmon_stats.sleep_stats_cdsp->last_entered_at;

				memcpy(sleep_stats_lpm, g_sysmon_stats.sleep_stats_cdsp,
						sizeof(struct sleep_stats));
			} else
				ret = -ENOKEY;
		}
	break;
	case SLPI:
		if (!g_sysmon_stats.smem_init_slpi)
			sysmon_smem_init_slpi();

		if (sleep_stats_lpm) {
			if (!IS_ERR_OR_NULL(g_sysmon_stats.sleep_stats_slpi)) {
				if (g_sysmon_stats.sleep_stats_slpi->last_entered_at >
					g_sysmon_stats.sleep_stats_slpi->last_exited_at)
					g_sysmon_stats.sleep_stats_slpi->accumulated
						+= arch_timer_read_counter() -
						g_sysmon_stats.sleep_stats_slpi->last_entered_at;

				memcpy(sleep_stats_lpm, g_sysmon_stats.sleep_stats_slpi,
							sizeof(struct sleep_stats));
			} else
				ret = -ENOKEY;
		}

		if (sleep_stats_lpi) {
			if (!IS_ERR_OR_NULL(g_sysmon_stats.sleep_lpi_slpi)) {
				if (g_sysmon_stats.sleep_lpi_slpi->last_entered_at >
					g_sysmon_stats.sleep_lpi_slpi->last_exited_at)
					g_sysmon_stats.sleep_lpi_slpi->accumulated
						+= arch_timer_read_counter() -
						g_sysmon_stats.sleep_lpi_slpi->last_entered_at;

				memcpy(sleep_stats_lpi, g_sysmon_stats.sleep_lpi_slpi,
							sizeof(struct sleep_stats_island));
			} else
				ret = -ENOKEY;
		}

	break;
	default:
		pr_err("%s:Provided subsystem %d is not supported\n", __func__, dsp_id);
		ret = -EINVAL;
	break;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(sysmon_stats_query_sleep);

static int master_adsp_stats_show(struct seq_file *s, void *d)
{
	int i = 0, j = 0;
	u64 lpi_accumulated = 0;
	u64 lpm_accumulated = 0;
	u32 q6_load;
	u8 ver = 0;
	struct sysmon_smem_power_stats sysmon_power_stats = { 0 };
	struct sysmon_smem_q6_event_stats_extended *events_ptr = NULL;

	if (!g_sysmon_stats.smem_init_adsp)
		sysmon_smem_init_adsp();

	if (g_sysmon_stats.sysmon_event_stats_adsp) {
		events_ptr = g_sysmon_stats.sysmon_event_stats_adsp;
		seq_puts(s, "\nsysMon stats:\n\n");
		seq_printf(s, "Core clock(KHz): %d\n",
				events_ptr->event_stats.QDSP6_clk);
		seq_printf(s, "Ab vote(Bytes): %llu\n",
				(((u64)events_ptr->event_stats.Ab_vote_msb << 32) |
					   events_ptr->event_stats.Ab_vote_lsb));
		seq_printf(s, "Ib vote(Bytes): %llu\n",
				(((u64)events_ptr->event_stats.Ib_vote_msb << 32) |
					   events_ptr->event_stats.Ib_vote_lsb));
		seq_printf(s, "Sleep latency(usec): %u\n",
				events_ptr->event_stats.Sleep_latency > 0 ?
				events_ptr->event_stats.Sleep_latency : U32_MAX);
	}

	if (g_sysmon_stats.dsppm_stats_adsp) {
		seq_puts(s, "\nDSPPM stats:\n\n");
		seq_printf(s, "Version: %u\n", g_sysmon_stats.dsppm_stats_adsp->version);
		seq_printf(s, "Sleep latency(usec): %u\n",
					g_sysmon_stats.dsppm_stats_adsp->latency_us ?
					g_sysmon_stats.dsppm_stats_adsp->latency_us : U32_MAX);
		seq_printf(s, "Timestamp: %u\n", g_sysmon_stats.dsppm_stats_adsp->timestamp);

		for (; i < DSPPMSTATS_NUMPD; i++) {
			seq_printf(s, "Pid: %d, Num active clients: %d\n",
						g_sysmon_stats.dsppm_stats_adsp->pd[i].pid,
						g_sysmon_stats.dsppm_stats_adsp->pd[i].num_active);
		}
	}

	if (g_sysmon_stats.sleep_stats_adsp) {
		lpm_accumulated = g_sysmon_stats.sleep_stats_adsp->accumulated;

		if (g_sysmon_stats.sleep_stats_adsp->last_entered_at >
					g_sysmon_stats.sleep_stats_adsp->last_exited_at)
			lpm_accumulated += arch_timer_read_counter() -
						g_sysmon_stats.sleep_stats_adsp->last_entered_at;

		seq_puts(s, "\nLPM stats:\n\n");
		seq_printf(s, "Count = %u\n", g_sysmon_stats.sleep_stats_adsp->count);
		seq_printf(s, "Last Entered At = %llu\n",
			g_sysmon_stats.sleep_stats_adsp->last_entered_at);
		seq_printf(s, "Last Exited At = %llu\n",
			g_sysmon_stats.sleep_stats_adsp->last_exited_at);
		seq_printf(s, "Accumulated Duration = %llu\n", lpm_accumulated);
	}

	if (g_sysmon_stats.sleep_lpi_adsp) {
		lpi_accumulated = g_sysmon_stats.sleep_lpi_adsp->accumulated;

		if (g_sysmon_stats.sleep_lpi_adsp->last_entered_at >
					g_sysmon_stats.sleep_lpi_adsp->last_exited_at)
			lpi_accumulated += arch_timer_read_counter() -
					g_sysmon_stats.sleep_lpi_adsp->last_entered_at;

		seq_puts(s, "\nLPI stats:\n\n");
		seq_printf(s, "Count = %u\n", g_sysmon_stats.sleep_lpi_adsp->count);
		seq_printf(s, "Last Entered At = %llu\n",
			g_sysmon_stats.sleep_lpi_adsp->last_entered_at);
		seq_printf(s, "Last Exited At = %llu\n",
			g_sysmon_stats.sleep_lpi_adsp->last_exited_at);
		seq_printf(s, "Accumulated Duration = %llu\n",
			lpi_accumulated);
	}

	if (g_sysmon_stats.sysmon_power_stats_adsp) {

		if (sysmon_stats_query_power_residency(ADSP, &sysmon_power_stats))
			return -ENOKEY;

		seq_puts(s, "\nPower Stats:\n\n");
		for (j = 0; j < SYSMON_POWER_STATS_MAX_CLK_LEVELS; j++) {
			if (sysmon_power_stats.clk_arr[j]) {
				if (ver >= 2) {
					seq_printf(s, "%u : Core Clock(KHz) : %u \tActive Time(sec) : %u \tLPI time(sec) : %u\n",
						j,
						sysmon_power_stats.clk_arr[j],
						sysmon_power_stats.active_time[j],
						sysmon_power_stats.island_time[j]);
				} else {
					seq_printf(s, "%u : Core Clock(KHz): %u \tActive Time(sec): %u\n",
						j,
						sysmon_power_stats.clk_arr[j],
						sysmon_power_stats.active_time[j]);
				}
			}
		}

		seq_printf(s, "Power collapse time(sec) = %u\n",
			sysmon_power_stats.pc_time);
		seq_printf(s, "Total LPI time(sec) = %u\n",
			sysmon_power_stats.lpi_time);
		if (ver >= 2)
			seq_printf(s, "Current core clock(KHz) = %u\n",
				sysmon_power_stats.current_clk);
	}

	if (g_sysmon_stats.q6_avg_load_adsp) {
		seq_puts(s, "\nQ6 load:\n\n");
		q6_load = sysmon_read_q6_load(ADSP);
		seq_printf(s, "Average Q6 load in KCPS = %u\n", q6_load);
	}

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(master_adsp_stats);

static int master_cdsp_stats_show(struct seq_file *s, void *d)
{
	int i = 0, j = 0, ret = 0;
	u32 hmx_util, hvx_util;
	u32 q6_load;
	u64 lpm_accumulated = 0;
	u8 ver = 0;
	struct sysmon_smem_power_stats sysmon_power_stats = { 0 };
	struct sysmon_smem_ddr_stats sysmon_ddr_stats = {0};
	struct sysmon_smem_q6_event_stats_extended *events_ptr = NULL;

	if (!g_sysmon_stats.smem_init_cdsp)
		sysmon_smem_init_cdsp();

	if (g_sysmon_stats.sysmon_event_stats_cdsp) {
		events_ptr = g_sysmon_stats.sysmon_event_stats_cdsp;
		ver = events_ptr->featureId_q6Event & 0xFFFF;
		seq_puts(s, "\nsysMon stats:\n\n");
		seq_printf(s, "Core clock(KHz): %d\n",
				events_ptr->event_stats.QDSP6_clk);
		seq_printf(s, "Ab vote(Bytes): %llu\n",
			(((u64)events_ptr->event_stats.Ab_vote_msb << 32) |
					   events_ptr->event_stats.Ab_vote_lsb));
		seq_printf(s, "Ib vote(Bytes): %llu\n",
			(((u64)events_ptr->event_stats.Ib_vote_msb << 32) |
					   events_ptr->event_stats.Ib_vote_lsb));
		seq_printf(s, "Sleep latency(usec): %u\n",
			events_ptr->event_stats.Sleep_latency > 0 ?
			events_ptr->event_stats.Sleep_latency : U32_MAX);
		if ((ver > 1) &&
			(events_ptr->event_stats_ext.HMX_Power_state >= 0)) {
			seq_printf(s, "HMX Power state: %s\n",
				events_ptr->event_stats_ext.HMX_Power_state ? "ON" : "OFF");
			seq_printf(s, "HMX Clock(KHz): %d\n",
				events_ptr->event_stats_ext.HMX_clk);
		}
	}

	if (g_sysmon_stats.dsppm_stats_cdsp) {
		seq_puts(s, "\nDSPPM stats:\n\n");
		seq_printf(s, "Version: %u\n",
			g_sysmon_stats.dsppm_stats_cdsp->version);
		seq_printf(s, "Sleep latency(usec): %u\n",
			g_sysmon_stats.dsppm_stats_cdsp->latency_us ?
			g_sysmon_stats.dsppm_stats_cdsp->latency_us : U32_MAX);
		seq_printf(s, "Timestamp: %u\n",
			g_sysmon_stats.dsppm_stats_cdsp->timestamp);

		for (; i < DSPPMSTATS_NUMPD; i++) {
			seq_printf(s, "Pid: %d, Num active clients: %d\n",
						g_sysmon_stats.dsppm_stats_cdsp->pd[i].pid,
						g_sysmon_stats.dsppm_stats_cdsp->pd[i].num_active);
		}
	}

	if (g_sysmon_stats.sleep_stats_cdsp) {
		lpm_accumulated = g_sysmon_stats.sleep_stats_cdsp->accumulated;

		if (g_sysmon_stats.sleep_stats_cdsp->last_entered_at >
					g_sysmon_stats.sleep_stats_cdsp->last_exited_at)
			lpm_accumulated += arch_timer_read_counter() -
						g_sysmon_stats.sleep_stats_cdsp->last_entered_at;

		seq_puts(s, "\nLPM stats:\n\n");
		seq_printf(s, "Count = %u\n",
			g_sysmon_stats.sleep_stats_cdsp->count);
		seq_printf(s, "Last Entered At = %llu\n",
			g_sysmon_stats.sleep_stats_cdsp->last_entered_at);
		seq_printf(s, "Last Exited At = %llu\n",
			g_sysmon_stats.sleep_stats_cdsp->last_exited_at);
		seq_printf(s, "Accumulated Duration = %llu\n", lpm_accumulated);
	}

	if (g_sysmon_stats.sysmon_power_stats_cdsp) {

		if (sysmon_stats_query_power_residency(CDSP, &sysmon_power_stats))
			return -ENOKEY;

		seq_puts(s, "\nPower Stats:\n\n");
		for (j = 0; j < SYSMON_POWER_STATS_MAX_CLK_LEVELS; j++) {
			if (sysmon_power_stats.clk_arr[j])
				seq_printf(s, "%u : Core Clock(KHz) : %u \tActive Time(sec) : %u\n",
					j,
					sysmon_power_stats.clk_arr[j],
					sysmon_power_stats.active_time[j]);
		}

		seq_printf(s, "Power collapse time(sec) = %u\n",
			sysmon_power_stats.pc_time);
		seq_printf(s, "Total LPI time(sec) = %u\n",
			sysmon_power_stats.lpi_time);

		if (ver >= 2)
			seq_printf(s, "Current core clock(KHz) = %u\n",
				sysmon_power_stats.current_clk);

		if (ret)
			return ret;

	}

	if (g_sysmon_stats.sysmon_ddr_stats) {

		seq_puts(s, "\nDDR stats :\n");

		if (sysmon_stats_query_ddr_residency(&sysmon_ddr_stats)) {
			seq_puts(s, "DDR Stats Query API failed\n");
			return -ENOKEY;
		}

		for (j = 0; j < sysmon_ddr_stats.num_ddrclk_levels; j++) {
			seq_printf(s, "%u : DDR Clock(KHz) : %u \tActive Time(sec) : %u\n",
				j,
				sysmon_ddr_stats.clk_arr[j],
				sysmon_ddr_stats.active_time[j]);
		}

		seq_printf(s, "Number of DDR clock levels = %u\n",
			sysmon_ddr_stats.num_ddrclk_levels);

		seq_printf(s, "Power collapse time(sec) = %u\n",
			sysmon_ddr_stats.pc_time);

		seq_printf(s, "Current DDR clock(KHz) = %u\n",
				sysmon_ddr_stats.curr_clk);

	}
	if (g_sysmon_stats.q6_avg_load_cdsp) {
		seq_puts(s, "\nQ6 load:\n\n");
		q6_load = sysmon_read_q6_load(CDSP);
		seq_printf(s, "Average Q6 load in KCPS = %u\n", q6_load);
	}

	ret = sysmon_stats_query_hmx_utlization(&hmx_util);

	if (ret) {
		seq_printf(s, "\nHMX stats not available, error code: %d\n", ret);
	} else {
		seq_puts(s, "\nHMX stats:\n\n");
		seq_printf(s, "HMX utilization in percentage = %u\n", hmx_util);
	}

	ret = sysmon_stats_query_hvx_utlization(&hvx_util);

	if (ret) {
		seq_printf(s, "\nHVX stats not available, error code: %d\n", ret);
	} else {
		seq_puts(s, "\nHVX Stats:\n\n");
		seq_printf(s, "HVX utilization in percentage = %u\n", hvx_util);
	}

	return 0;
}

DEFINE_SHOW_ATTRIBUTE(master_cdsp_stats);

static long dsp_utilization_stats_ioctl(struct file *file, unsigned int ioctl_num,
			unsigned long ioctl_params)
{
	u32 util;
	u32 q6_avg_load;
	u32 retVal;

	switch (ioctl_num) {
	case HVX_UTILIZATION_QUERY:
		retVal = sysmon_stats_query_hvx_utlization(&util);
		if (retVal) {
			pr_err("sysmon_stats_query_hvx_utlization failed with return value: %d\n",
				retVal);
			return retVal;
		}

		if (!access_ok((u32 __user *)ioctl_params, sizeof(u32))) {
			pr_err("User-space pointer is not accessible\n");
			return -EFAULT;
		}

		if (copy_to_user((u32 __user *)ioctl_params, &util, sizeof(u32))) {
			pr_err("copy_to_user failed in HVX Utilization data read\n");
			return -EFAULT;
		}

		break;
	case HMX_UTILIZATION_QUERY:
		retVal = sysmon_stats_query_hmx_utlization(&util);
		if (retVal) {
			pr_err("sysmon_stats_query_hmx_utlization failed with return value: %d\n",
					retVal);
			return retVal;
		}

		if (!access_ok((u32 __user *)ioctl_params, sizeof(u32))) {
			pr_err("User-space pointer is not accessible\n");
			return -EFAULT;
		}

		if (copy_to_user((u32 __user *)ioctl_params, &util, sizeof(u32))) {
			pr_err("copy_to_user failed in HMX Utilization data read\n");
			return -EFAULT;
		}

		break;
	case Q6_CDSP_UTILIZATION:
		retVal = sysmon_stats_query_q6_load(CDSP, &q6_avg_load);
		if (retVal) {
			pr_err("sysmon_stats_query_q6_load failed with return value: %d\n",
				retVal);
			return retVal;
		}

		if (!access_ok((u32 __user *)ioctl_params, sizeof(u32))) {
			pr_err("User-space pointer is not accessible\n");
			return -EFAULT;
		}

		if (copy_to_user((u32 __user *)ioctl_params, &q6_avg_load, sizeof(u32))) {
			pr_err("copy_to_user failed in Q6 load data read\n");
			return -EFAULT;
		}

		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static const struct file_operations sysmon_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = dsp_utilization_stats_ioctl,
};


/*
    Dump adsp/cdsp stats by /sys/kernel/dsp_stats.
    NOTE: the node size of sysfs PAGESIZE.
*/
static struct kset *moto_dsp_stats;

static int sysfs_adsp_stats_show(struct seq_buf *s)
{
	int i = 0, j = 0;
	u64 lpi_accumulated = 0;
	u64 lpm_accumulated = 0;
	u32 q6_load;
	struct sysmon_smem_power_stats sysmon_power_stats = { 0 };
	struct sysmon_smem_q6_event_stats_extended *events_ptr = NULL;

	if (!g_sysmon_stats.smem_init_adsp)
		sysmon_smem_init_adsp();

	if (g_sysmon_stats.sysmon_event_stats_adsp) {
		events_ptr = g_sysmon_stats.sysmon_event_stats_adsp;
		seq_buf_printf(s, "\nsysMon stats:\n\n");
		seq_buf_printf(s, "Core clock(KHz): %d\n",
				events_ptr->event_stats.QDSP6_clk);
		seq_buf_printf(s, "Ab vote(Bytes): %llu\n",
				(((u64)events_ptr->event_stats.Ab_vote_msb << 32) |
					   events_ptr->event_stats.Ab_vote_lsb));
		seq_buf_printf(s, "Ib vote(Bytes): %llu\n",
				(((u64)events_ptr->event_stats.Ib_vote_msb << 32) |
					   events_ptr->event_stats.Ib_vote_lsb));
		seq_buf_printf(s, "Sleep latency(usec): %u\n",
				events_ptr->event_stats.Sleep_latency > 0 ?
				events_ptr->event_stats.Sleep_latency : U32_MAX);
	}

	if (g_sysmon_stats.dsppm_stats_adsp) {
		seq_buf_printf(s, "\nDSPPM stats:\n\n");
		seq_buf_printf(s, "Version: %u\n", g_sysmon_stats.dsppm_stats_adsp->version);
		seq_buf_printf(s, "Sleep latency(usec): %u\n",
					g_sysmon_stats.dsppm_stats_adsp->latency_us ?
					g_sysmon_stats.dsppm_stats_adsp->latency_us : U32_MAX);
		seq_buf_printf(s, "Timestamp: %u\n", g_sysmon_stats.dsppm_stats_adsp->timestamp);

		for (; i < DSPPMSTATS_NUMPD; i++) {
			seq_buf_printf(s, "Pid: %d, Num active clients: %d\n",
						g_sysmon_stats.dsppm_stats_adsp->pd[i].pid,
						g_sysmon_stats.dsppm_stats_adsp->pd[i].num_active);
		}
	}

	if (g_sysmon_stats.sleep_stats_adsp) {
		lpm_accumulated = g_sysmon_stats.sleep_stats_adsp->accumulated;

		if (g_sysmon_stats.sleep_stats_adsp->last_entered_at >
					g_sysmon_stats.sleep_stats_adsp->last_exited_at)
			lpm_accumulated += arch_timer_read_counter() -
						g_sysmon_stats.sleep_stats_adsp->last_entered_at;

		seq_buf_printf(s, "\nLPM stats:\n\n");
		seq_buf_printf(s, "Count = %u\n", g_sysmon_stats.sleep_stats_adsp->count);
		seq_buf_printf(s, "Last Entered At = %llu\n",
			g_sysmon_stats.sleep_stats_adsp->last_entered_at);
		seq_buf_printf(s, "Last Exited At = %llu\n",
			g_sysmon_stats.sleep_stats_adsp->last_exited_at);
		seq_buf_printf(s, "Accumulated Duration = %llu\n", lpm_accumulated);
	}

	if (g_sysmon_stats.sleep_lpi_adsp) {
		lpi_accumulated = g_sysmon_stats.sleep_lpi_adsp->accumulated;

		if (g_sysmon_stats.sleep_lpi_adsp->last_entered_at >
					g_sysmon_stats.sleep_lpi_adsp->last_exited_at)
			lpi_accumulated += arch_timer_read_counter() -
					g_sysmon_stats.sleep_lpi_adsp->last_entered_at;

		seq_buf_printf(s, "\nLPI stats:\n\n");
		seq_buf_printf(s, "Count = %u\n", g_sysmon_stats.sleep_lpi_adsp->count);
		seq_buf_printf(s, "Last Entered At = %llu\n",
			g_sysmon_stats.sleep_lpi_adsp->last_entered_at);
		seq_buf_printf(s, "Last Exited At = %llu\n",
			g_sysmon_stats.sleep_lpi_adsp->last_exited_at);
		seq_buf_printf(s, "Accumulated Duration = %llu\n",
			lpi_accumulated);
	}

	if (g_sysmon_stats.sysmon_power_stats_adsp) {

		if (sysmon_stats_query_power_residency(ADSP, &sysmon_power_stats))
			return -ENOKEY;

		seq_buf_printf(s, "\nPower Stats:\n\n");
		for (j = 0; j < SYSMON_POWER_STATS_MAX_CLK_LEVELS; j++) {
			if (sysmon_power_stats.clk_arr[j]) {
				seq_buf_printf(s, "%u : Core Clock(KHz): %u \tActive Time(sec): %u\n",
					j,
					sysmon_power_stats.clk_arr[j],
					sysmon_power_stats.active_time[j]);
			}
		}

		seq_buf_printf(s, "Power collapse time(sec) = %u\n",
			sysmon_power_stats.pc_time);
		seq_buf_printf(s, "Total LPI time(sec) = %u\n",
			sysmon_power_stats.lpi_time);
	}

	if (g_sysmon_stats.q6_avg_load_adsp) {
		seq_buf_printf(s, "\nQ6 load:\n\n");
		q6_load = sysmon_read_q6_load(ADSP);
		seq_buf_printf(s, "Average Q6 load in KCPS = %u\n", q6_load);
	}

	return 0;
}

static int sysfs_cdsp_stats_show(struct seq_buf *s)
{
	int i = 0, j = 0, ret = 0;
	u32 hmx_util, hvx_util;
	u32 q6_load;
	u64 lpm_accumulated = 0;
	u8 ver = 0;
	struct sysmon_smem_power_stats sysmon_power_stats = { 0 };
	struct sysmon_smem_ddr_stats sysmon_ddr_stats = {0};
	struct sysmon_smem_q6_event_stats_extended *events_ptr = NULL;

	if (!g_sysmon_stats.smem_init_cdsp)
		sysmon_smem_init_cdsp();

	if (g_sysmon_stats.sysmon_event_stats_cdsp) {
		events_ptr = g_sysmon_stats.sysmon_event_stats_cdsp;
		ver = events_ptr->featureId_q6Event & 0xFFFF;
		seq_buf_printf(s, "\nsysMon stats:\n\n");
		seq_buf_printf(s, "Core clock(KHz): %d\n",
				events_ptr->event_stats.QDSP6_clk);
		seq_buf_printf(s, "Ab vote(Bytes): %llu\n",
			(((u64)events_ptr->event_stats.Ab_vote_msb << 32) |
					   events_ptr->event_stats.Ab_vote_lsb));
		seq_buf_printf(s, "Ib vote(Bytes): %llu\n",
			(((u64)events_ptr->event_stats.Ib_vote_msb << 32) |
					   events_ptr->event_stats.Ib_vote_lsb));
		seq_buf_printf(s, "Sleep latency(usec): %u\n",
			events_ptr->event_stats.Sleep_latency > 0 ?
			events_ptr->event_stats.Sleep_latency : U32_MAX);
		if ((ver > 1) &&
			(events_ptr->event_stats_ext.HMX_Power_state >= 0)) {
			seq_buf_printf(s, "HMX Power state: %s\n",
				events_ptr->event_stats_ext.HMX_Power_state ? "ON" : "OFF");
			seq_buf_printf(s, "HMX Clock(KHz): %d\n",
				events_ptr->event_stats_ext.HMX_clk);
		}
	}

	if (g_sysmon_stats.dsppm_stats_cdsp) {
		seq_buf_printf(s, "\nDSPPM stats:\n\n");
		seq_buf_printf(s, "Version: %u\n",
			g_sysmon_stats.dsppm_stats_cdsp->version);
		seq_buf_printf(s, "Sleep latency(usec): %u\n",
			g_sysmon_stats.dsppm_stats_cdsp->latency_us ?
			g_sysmon_stats.dsppm_stats_cdsp->latency_us : U32_MAX);
		seq_buf_printf(s, "Timestamp: %u\n",
			g_sysmon_stats.dsppm_stats_cdsp->timestamp);

		for (; i < DSPPMSTATS_NUMPD; i++) {
			seq_buf_printf(s, "Pid: %d, Num active clients: %d\n",
						g_sysmon_stats.dsppm_stats_cdsp->pd[i].pid,
						g_sysmon_stats.dsppm_stats_cdsp->pd[i].num_active);
		}
	}

	if (g_sysmon_stats.sleep_stats_cdsp) {
		lpm_accumulated = g_sysmon_stats.sleep_stats_cdsp->accumulated;

		if (g_sysmon_stats.sleep_stats_cdsp->last_entered_at >
					g_sysmon_stats.sleep_stats_cdsp->last_exited_at)
			lpm_accumulated += arch_timer_read_counter() -
						g_sysmon_stats.sleep_stats_cdsp->last_entered_at;

		seq_buf_printf(s, "\nLPM stats:\n\n");
		seq_buf_printf(s, "Count = %u\n",
			g_sysmon_stats.sleep_stats_cdsp->count);
		seq_buf_printf(s, "Last Entered At = %llu\n",
			g_sysmon_stats.sleep_stats_cdsp->last_entered_at);
		seq_buf_printf(s, "Last Exited At = %llu\n",
			g_sysmon_stats.sleep_stats_cdsp->last_exited_at);
		seq_buf_printf(s, "Accumulated Duration = %llu\n", lpm_accumulated);
	}

	if (g_sysmon_stats.sysmon_power_stats_cdsp) {

		if (sysmon_stats_query_power_residency(CDSP, &sysmon_power_stats))
			return -ENOKEY;

		seq_buf_printf(s, "\nPower Stats:\n\n");
		for (j = 0; j < SYSMON_POWER_STATS_MAX_CLK_LEVELS; j++) {
			if (sysmon_power_stats.clk_arr[j])
				seq_buf_printf(s, "%u : Core Clock(KHz) : %u \tActive Time(sec) : %u\n",
					j,
					sysmon_power_stats.clk_arr[j],
					sysmon_power_stats.active_time[j]);
		}

		seq_buf_printf(s, "Power collapse time(sec) = %u\n",
			sysmon_power_stats.pc_time);
		seq_buf_printf(s, "Total LPI time(sec) = %u\n",
			sysmon_power_stats.lpi_time);

		if (ver >= 2)
			seq_buf_printf(s, "Current core clock(KHz) = %u\n",
				sysmon_power_stats.current_clk);
	}

	if (g_sysmon_stats.sysmon_ddr_stats) {

		seq_buf_printf(s, "\nDDR stats :\n");

		if (sysmon_stats_query_ddr_residency(&sysmon_ddr_stats)) {
			seq_buf_printf(s, "DDR Stats Query API failed\n");
			return -ENOKEY;
		}

		for (j = 0; j < sysmon_ddr_stats.num_ddrclk_levels; j++) {
			seq_buf_printf(s, "%u : DDR Clock(KHz) : %u \tActive Time(sec) : %u\n",
				j,
				sysmon_ddr_stats.clk_arr[j],
				sysmon_ddr_stats.active_time[j]);
		}

		seq_buf_printf(s, "Number of DDR clock levels = %u\n",
			sysmon_ddr_stats.num_ddrclk_levels);

		seq_buf_printf(s, "Power collapse time(sec) = %u\n",
			sysmon_ddr_stats.pc_time);

		seq_buf_printf(s, "Current DDR clock(KHz) = %u\n",
				sysmon_ddr_stats.curr_clk);

	}
	if (g_sysmon_stats.q6_avg_load_cdsp) {
		seq_buf_printf(s, "\nQ6 load:\n\n");
		q6_load = sysmon_read_q6_load(CDSP);
		seq_buf_printf(s, "Average Q6 load in KCPS = %u\n", q6_load);
	}

	ret = sysmon_stats_query_hmx_utlization(&hmx_util);

	if (ret) {
		seq_buf_printf(s, "\nHMX stats not available, error code: %d\n", ret);
	} else {
		seq_buf_printf(s, "\nHMX stats:\n\n");
		seq_buf_printf(s, "HMX utilization in percentage = %u\n", hmx_util);
	}

	ret = sysmon_stats_query_hvx_utlization(&hvx_util);

	if (ret) {
		seq_buf_printf(s, "\nHVX stats not available, error code: %d\n", ret);
	} else {
		seq_buf_printf(s, "\nHVX Stats:\n\n");
		seq_buf_printf(s, "HVX utilization in percentage = %u\n", hvx_util);
	}

	return 0;
}

static ssize_t show_adsp_stats(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf)
{
	struct seq_buf sbuf;

	seq_buf_init(&sbuf, buf, PAGE_SIZE);
	sysfs_adsp_stats_show(&sbuf);

	return seq_buf_used(&sbuf);
}

static ssize_t show_adsp_clk(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf)
{
	int clk = 0;
	struct sysmon_smem_q6_event_stats events;

	if (!sysmon_stats_query_q6_votes(ADSP, &events)) {
		clk = events.QDSP6_clk;
	}

	return scnprintf(buf, PAGE_SIZE, "%d\n", clk);
}

//Output format: "[{ActiveTime, LPI Time}, ...], current_clk, sleep_latency"
static ssize_t show_clk_stats(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf)
{
	int j, first_clk = 0;
	struct seq_buf s;
	struct sysmon_smem_power_stats sysmon_power_stats = { 0 };
	struct sysmon_smem_q6_event_stats_extended *events_ptr = NULL;

	seq_buf_init(&s, buf, PAGE_SIZE);

	if (!g_sysmon_stats.smem_init_adsp)
		sysmon_smem_init_adsp();

	if (g_sysmon_stats.sysmon_power_stats_adsp) {

		if (sysmon_stats_query_power_residency(ADSP, &sysmon_power_stats)) {
			seq_buf_printf(&s, "NA");
			return seq_buf_used(&s);
		}

		for (j = 0; j < SYSMON_POWER_STATS_MAX_CLK_LEVELS; j++) {
			if (sysmon_power_stats.clk_arr[j]) {
				first_clk = j;
				break;
			}
		}

		seq_buf_printf(&s, "[");
		for (j = first_clk; j < SYSMON_POWER_STATS_MAX_CLK_LEVELS; j++) {
			if (sysmon_power_stats.clk_arr[j]) {
				if (first_clk == j) {
					seq_buf_printf(&s, "%u",
						sysmon_power_stats.active_time[j]);
				} else {
					seq_buf_printf(&s, ", %u",
						sysmon_power_stats.active_time[j]);
				}
			}
		}
		seq_buf_printf(&s, "]");
	}

	if (g_sysmon_stats.sysmon_event_stats_adsp) {
		events_ptr = g_sysmon_stats.sysmon_event_stats_adsp;

		seq_buf_printf(&s, ", %10d", events_ptr->event_stats.QDSP6_clk);
		seq_buf_printf(&s, ", %10u", events_ptr->event_stats.Sleep_latency > 0 ?
				events_ptr->event_stats.Sleep_latency : U32_MAX);
	}
	seq_buf_printf(&s, "\n");

	return seq_buf_used(&s);
}

static struct kobj_attribute adsp_stats_attr =
	__ATTR(adsp_stats, 0444, show_adsp_stats, NULL);
static struct kobj_attribute adsp_clk_attr =
	__ATTR(clk, 0444, show_adsp_clk, NULL);
static struct kobj_attribute clk_stats_attr =
	__ATTR(clk_stats, 0444, show_clk_stats, NULL);

static struct attribute *adsp_stats_attrs[] = {
	&adsp_stats_attr.attr,
	&adsp_clk_attr.attr,
	&clk_stats_attr.attr,
	NULL,
};

static struct attribute_group adsp_stats_attr_group = {
	.attrs = adsp_stats_attrs,
};

static ssize_t show_cdsp_stats(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf)
{
	struct seq_buf sbuf;

	seq_buf_init(&sbuf, buf, PAGE_SIZE);
	sysfs_cdsp_stats_show(&sbuf);

	return seq_buf_used(&sbuf);
}

static ssize_t show_hvx_clk(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf)
{
    int clk = 0;
    struct sysmon_smem_q6_event_stats events;

    if (!sysmon_stats_query_q6_votes(CDSP, &events)) {
        clk = events.QDSP6_clk;
    }

	return scnprintf(buf, PAGE_SIZE, "%d\n", clk);
}

static ssize_t show_hvx_util(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf)
{
    u32 util = 0;

    sysmon_stats_query_hvx_utlization(&util);

	return scnprintf(buf, PAGE_SIZE, "%d\n", util);
}

//Format: "[ActiveTime, ...], current_clk, sleep_latency, hmx power, hmx clk"
static ssize_t show_hvxclk_in_time(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf)
{
	int j = 0, first_clk = 0;
	u8 ver = 0;
	struct seq_buf s;
	struct sysmon_smem_power_stats sysmon_power_stats = { 0 };
	struct sysmon_smem_q6_event_stats_extended *events_ptr = NULL;

	seq_buf_init(&s, buf, PAGE_SIZE);

	if (!g_sysmon_stats.smem_init_cdsp)
		sysmon_smem_init_cdsp();

	if (g_sysmon_stats.sysmon_power_stats_cdsp) {

		if (sysmon_stats_query_power_residency(CDSP, &sysmon_power_stats)) {
			seq_buf_printf(&s, "NA");
			return seq_buf_used(&s);
		}

		for (j = 0; j < SYSMON_POWER_STATS_MAX_CLK_LEVELS; j++) {
			if (sysmon_power_stats.clk_arr[j]) {
				first_clk = j;
				break;
			}
		}

		seq_buf_printf(&s, "[");
		for (j = first_clk; j < SYSMON_POWER_STATS_MAX_CLK_LEVELS; j++) {
			if (sysmon_power_stats.clk_arr[j]) {
				if (j == first_clk) {
					seq_buf_printf(&s, "%u",
					sysmon_power_stats.active_time[j]);
				} else {
					seq_buf_printf(&s, ", %u",
					sysmon_power_stats.active_time[j]);
				}
			}
		}
		seq_buf_printf(&s, "]");
	}

	if (g_sysmon_stats.sysmon_event_stats_cdsp) {
		events_ptr = g_sysmon_stats.sysmon_event_stats_cdsp;
		ver = events_ptr->featureId_q6Event & 0xFFFF;

		seq_buf_printf(&s, ", %10d",
				events_ptr->event_stats.QDSP6_clk);

		seq_buf_printf(&s, ", %10u",
			events_ptr->event_stats.Sleep_latency > 0 ?
			events_ptr->event_stats.Sleep_latency : U32_MAX);

		if ((ver > 1)
			 && (events_ptr->event_stats_ext.HMX_Power_state >= 0)) {
			seq_buf_printf(&s, ", %2d",
				events_ptr->event_stats_ext.HMX_Power_state);
			seq_buf_printf(&s, ", %10d",
				events_ptr->event_stats_ext.HMX_clk);
		}
	}

	seq_buf_printf(&s, "\n");

	return seq_buf_used(&s);
}

static ssize_t show_hmx_clk(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf)
{
    int clk = 0;
    struct sysmon_smem_q6_event_stats events;

    if (!sysmon_stats_query_q6_votes(CDSP, &events)) {
        clk = events.HMX_clk > 0 ? events.HMX_clk : 0;
    }

	return scnprintf(buf, PAGE_SIZE, "%d\n", clk);
}

static ssize_t show_hmx_util(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf)
{
    u32 util = 0;

    sysmon_stats_query_hmx_utlization(&util);

	return scnprintf(buf, PAGE_SIZE, "%d\n", util);
}

static ssize_t show_hmx_power(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf)
{
    int power = 0;
    struct sysmon_smem_q6_event_stats events;

    if (!sysmon_stats_query_q6_votes(CDSP, &events)) {
        power = events.HMX_Power_state > 0 ? events.HMX_Power_state : 0;
    }

	return scnprintf(buf, PAGE_SIZE, "%d\n", power);
}

static struct kobj_attribute cdsp_stats_attr =
	__ATTR(cdsp_stats, 0444, show_cdsp_stats, NULL);
static struct kobj_attribute cdsp_hvx_clk_attr =
	__ATTR(hvx_clk, 0444, show_hvx_clk, NULL);
static struct kobj_attribute cdsp_hvx_util_attr =
	__ATTR(hvx_util, 0444, show_hvx_util, NULL);
static struct kobj_attribute cdsp_hvxclk_in_time_attr =
	__ATTR(clk_stats, 0444, show_hvxclk_in_time, NULL);
static struct kobj_attribute cdsp_hmx_clk_attr =
	__ATTR(hmx_clk, 0444, show_hmx_clk, NULL);
static struct kobj_attribute cdsp_hmx_util_attr =
	__ATTR(hmx_util, 0444, show_hmx_util, NULL);
static struct kobj_attribute cdsp_hmx_power_attr =
	__ATTR(hmx_power, 0444, show_hmx_power, NULL);

static struct attribute *cdsp_stats_attrs[] = {
	&cdsp_stats_attr.attr,
	&cdsp_hvx_clk_attr.attr,
	&cdsp_hvx_util_attr.attr,
	&cdsp_hvxclk_in_time_attr.attr,
	&cdsp_hmx_clk_attr.attr,
	&cdsp_hmx_util_attr.attr,
	&cdsp_hmx_power_attr.attr,
	NULL,
};

static struct attribute_group cdsp_stats_attr_group = {
	.attrs = cdsp_stats_attrs,
};

static int sysfs_kernel_dsp_stats_init(void)
{
	int ret = 0;
    struct kset *adsp_kset, *cdsp_kset;
	struct kobject *module_kobj, *adsp_kobj, *cdsp_kobj;

	moto_dsp_stats = kset_create_and_add("dsp_stats", NULL, kernel_kobj);
	if (!moto_dsp_stats) {
		pr_err("moto_dsp_stats: memory is not enough\n");
		ret = -ENOMEM;
        goto failed;
	}

	module_kobj = &moto_dsp_stats->kobj;

	adsp_kset = kset_create_and_add("adsp", NULL, module_kobj);
	if (!adsp_kset) {
		pr_err("adsp_stats: memory is not enough\n");
		ret = -ENOMEM;
        goto adsp_failed;
	}
    adsp_kobj = &adsp_kset->kobj;

	cdsp_kset = kset_create_and_add("cdsp", NULL, module_kobj);
	if (!cdsp_kset) {
		pr_err("cdsp_stats: memory is not enough\n");
		ret = -ENOMEM;
        goto cdsp_failed;
	}
    cdsp_kobj = &cdsp_kset->kobj;

	ret = sysfs_create_group(adsp_kobj, &adsp_stats_attr_group);
	if (ret) {
		pr_err("adsp_stats: Failed to create adsp sysfs\n");
		goto dsp_failed;
	}

	ret = sysfs_create_group(cdsp_kobj, &cdsp_stats_attr_group);
	if (ret) {
		pr_err("cdsp_stats: Failed to create cdsp sysfs\n");
		goto adsp_grp_failed;
	}

    return 0;

adsp_grp_failed:
    sysfs_remove_group(adsp_kobj, &adsp_stats_attr_group);
dsp_failed:
    kset_unregister(cdsp_kset);
cdsp_failed:
    kset_unregister(adsp_kset);
adsp_failed:
    kset_unregister(moto_dsp_stats);
failed:
    return ret;
}


static int  __init sysmon_stats_init(void)
{

	u32 ret;
	struct subsystem_stats *me = &g_subsystem_stats;

	sysfs_kernel_dsp_stats_init();
	g_sysmon_stats.debugfs_dir = debugfs_create_dir("sysmon_subsystem_stats", NULL);

	if (!g_sysmon_stats.debugfs_dir) {
		pr_err("Failed to create debugfs directory for sysmon_subsystem_stats\n");
		goto debugfs_bail;
	}

	g_sysmon_stats.debugfs_master_adsp_stats =
			debugfs_create_file("master_adsp_stats",
			 0444, g_sysmon_stats.debugfs_dir, NULL, &master_adsp_stats_fops);

	if (!g_sysmon_stats.debugfs_master_adsp_stats)
		pr_err("Failed to create debugfs file for ADSP master stats\n");

	g_sysmon_stats.debugfs_master_cdsp_stats =
			debugfs_create_file("master_cdsp_stats",
			 0444, g_sysmon_stats.debugfs_dir, NULL, &master_cdsp_stats_fops);

	if (!g_sysmon_stats.debugfs_master_cdsp_stats)
		pr_err("Failed to create debugfs file for CDSP master stats\n");

	ret = alloc_chrdev_region(&me->devno, 0, 1, SUBSYSTEMSTATS_DEVICE_NAME_LOCAL);

	if (ret != 0) {
		pr_err("Cannot create sysmon_subsystem_stats char device\n");
		goto chardev_region_fail;
	}

	cdev_init(&me->sysmon_cdev, &sysmon_fops);
	me->sysmon_cdev.owner = THIS_MODULE;

	if ((cdev_add(&me->sysmon_cdev, MKDEV(MAJOR(me->devno), 0), 1)) != 0) {
		pr_err("Cannot add the subsystem stats device to the system\n");
		goto cdev_add_fail;
	}

	me->dev_class = class_create(SUBSYSTEMSTATS_CLASS_NAME_LOCAL);

	if (IS_ERR(me->dev_class)) {
		pr_err("Cannot create the subsystem stats class\n");
		goto class_bail;
	}

	if (IS_ERR(device_create(me->dev_class, NULL,
						MKDEV(MAJOR(me->devno), 0),
						NULL,
						SUBSYSTEMSTATS_DEVICE_NAME_LOCAL))) {
		pr_err("Cannot create subsystem stats Device\n");
		goto device_bail;
	}

	pr_debug("Subsystem stats device inserted\n");
	return 0;
device_bail:
	class_destroy(me->dev_class);
class_bail:
	cdev_del(&g_subsystem_stats.sysmon_cdev);
cdev_add_fail:
	unregister_chrdev_region(me->devno, 1);
chardev_region_fail:
	debugfs_remove_recursive(g_sysmon_stats.debugfs_dir);
debugfs_bail:
	return 0;
}

static void __exit sysmon_stats_exit(void)
{
	debugfs_remove_recursive(g_sysmon_stats.debugfs_dir);
	device_destroy(g_subsystem_stats.dev_class, g_subsystem_stats.sysmon_cdev.dev);
	class_destroy(g_subsystem_stats.dev_class);
	cdev_del(&g_subsystem_stats.sysmon_cdev);
	unregister_chrdev_region(g_subsystem_stats.devno, 1);
	pr_debug("Removed subsystem stats device\n");
}

module_init(sysmon_stats_init);
module_exit(sysmon_stats_exit);


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Qualcomm Technologies, Inc. Sysmon subsystem Stats driver");
