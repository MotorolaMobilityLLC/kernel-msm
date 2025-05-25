// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#define pr_fmt(fmt)	"qpt_hw: %s: " fmt, __func__

#include <linux/device.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/sched/clock.h>
#include <asm/unaligned.h>

#define CREATE_TRACE_POINTS
#include "trace.h"

#include "qti_power_telemetry_v2.h"

#define QPT_CONFIG_SDAM_BASE_OFF	0x45
#define QPT_DATA_SDAM_BASE_OFF		0x45
#define QPT_CH_ENABLE_MASK_0		BIT(7)
#define QPT_CH_ENABLE_MASK_1		BIT(3)
#define QPT_SID_MASK			GENMASK(4, 0)
#define QPT_DATA_BYTE_SIZE		5
#define QPT_ADC_SF_MULTIPLIER		GENMASK(2, 0)
#define QPT_DATA_SF_BASE		400L

#define QPT_GET_CMLTV_POWER_UW_FROM_ADC(qpt, adc)	(adc * qpt->adc_scaling_factor)
#define QPT_HW "qpt-hw"

static int qpt_sdam_nvmem_write(struct qpt_priv *qpt, struct qpt_sdam *sdam,
		uint16_t offset, size_t bytes, void *data)
{
	int rc = 0;

	mutex_lock(&sdam->lock);
	rc = nvmem_device_write(sdam->nvmem, offset, bytes, data);
	mutex_unlock(&sdam->lock);
	if (rc < 0)
		dev_err(qpt->dev,
			"Failed to write sdam[%d] off:%#x,size:%ld rc=%d\n",
			sdam->id, offset, bytes, rc);
	return rc;
}

static int qpt_sdam_nvmem_read(struct qpt_priv *qpt, struct qpt_sdam *sdam,
		uint16_t offset, size_t bytes, void *data)
{
	int rc = 0;

	mutex_lock(&sdam->lock);
	rc = nvmem_device_read(sdam->nvmem, offset, bytes, data);
	mutex_unlock(&sdam->lock);
	if (rc < 0)
		dev_err(qpt->dev,
			"Failed to read sdam[%d] off:%#x,size:%ld rc=%d\n",
			sdam->id, offset, bytes, rc);
	return rc;
}

static void qti_qpt_clear_all_channel_data(struct qpt_priv *qpt)
{
	struct qpt_device *qpt_dev;

	qpt->hw_read_ts = 0;
	qpt->rtc_ts = 0;
	list_for_each_entry(qpt_dev, &qpt->qpt_dev_head, qpt_node) {
		mutex_lock(&qpt_dev->lock);
		qpt_dev->last_data = 0;
		qpt_dev->last_data_uw = 0;
		qpt_dev->total_energey_uj = 0;
		qpt_dev->pavg = 0;
		qpt_dev->prev_buffer_energy = 0;
		qpt_dev->prev_buffer_data_uw = 0;
		qpt_dev->prev_buffer_data_adc = 0;
		mutex_unlock(&qpt_dev->lock);
	}
}

static bool qti_qpt_telemetry_status(struct qpt_priv *qpt)
{
	return qpt->enabled ? true : false;
}

static u32 get_data_update_rate_from_config(uint8_t timer_lb, uint8_t timer_ub,
		uint8_t config0, uint8_t max_count)
{
	u32 timer = timer_ub << 8 | timer_lb;
	u32 hz = config0 & BIT(3) ? 10 : 32000;

	return (timer * max_count  * 1000 / hz);
}

static u32 get_scaling_factor_from_config(uint8_t config)
{
	u32 mltplr = config & QPT_ADC_SF_MULTIPLIER;

	return (QPT_DATA_SF_BASE * (1 << mltplr));
}

static u32 get_tperiod_from_config(uint8_t timer_lb, uint8_t timer_ub,
		uint8_t config0, uint8_t config1)
{
	u32 timer = timer_ub << 8 | timer_lb;
	u32 hz = config0 & BIT(3) ? 10 : 32000;
	u32 shift = 1 << config1;

	return (timer * shift * 1000 / hz);
}

static int qti_qpt_sync_common_telemetry_config(struct qpt_priv *qpt)
{
	uint8_t *config_sdam = NULL;
	int rc = 0;

	if (!qpt->sdam[DATA_AVG_SDAM].nvmem || !qpt->sdam[CONFIG_SDAM].nvmem) {
		dev_err(qpt->dev, "Invalid sdam nvmem\n");
		return -EINVAL;
	}

	config_sdam = qpt->config_sdam_data;
	if (!config_sdam) {
		config_sdam = devm_kcalloc(qpt->dev, MAX_CONFIG_SDAM_DATA,
				sizeof(*config_sdam), GFP_KERNEL);
		if (!config_sdam)
			return -ENOMEM;
	} else {
		memset(config_sdam, 0, MAX_CONFIG_SDAM_DATA);
	}

	mutex_lock(&qpt->hw_read_lock);
	rc = qpt_sdam_nvmem_read(qpt, &qpt->sdam[CONFIG_SDAM],
			QPT_CONFIG_SDAM_BASE_OFF,
			MAX_CONFIG_SDAM_DATA, config_sdam);
	if (rc < 0)
		goto unlock_exit;

	qpt->ready_max_count = config_sdam[CONFIG_SDAM_DATA_READY_MAX_COUNT];
	qpt->bob_max_count = config_sdam[CONFIG_SDAM_BOB_MAX_COUNT];
	qpt->hsr_ver = config_sdam[CONFIG_SDAM_HSR_VER];
	qpt->data_update_sampling = get_data_update_rate_from_config(
				config_sdam[CONFIG_SDAM_TELEMETRY_TIMER_LB],
				config_sdam[CONFIG_SDAM_TELEMETRY_TIMER_UB],
				config_sdam[CONFIG_SDAM_TELEMETRY_CONFIG0],
					qpt->ready_max_count);
	qpt->tperiod =  get_tperiod_from_config(
				config_sdam[CONFIG_SDAM_TELEMETRY_TIMER_LB],
				config_sdam[CONFIG_SDAM_TELEMETRY_TIMER_UB],
				config_sdam[CONFIG_SDAM_TELEMETRY_CONFIG0],
				config_sdam[CONFIG_SDAM_TELEMETRY_CONFIG1]);
	qpt->bob_tperiod = qpt->data_update_sampling / qpt->bob_max_count;
	qpt->adc_scaling_factor = get_scaling_factor_from_config(
					config_sdam[CONFIG_SDAM_TELEMETRY_CONFIG0]);
	qpt->config_sdam_data = config_sdam;
	QPT_DBG_EVENT(qpt, "ready_cnt:%d bob count:%d b_tperiod:%d tperiod:%d",
			qpt->ready_max_count, qpt->bob_max_count, qpt->bob_tperiod,
			qpt->tperiod);
	QPT_DBG_EVENT(qpt, "scaling_factor:%d reporting sampling:%d",
			qpt->adc_scaling_factor, qpt->data_update_sampling);

unlock_exit:
	mutex_unlock(&qpt->hw_read_lock);

	return rc >= 0 ? 0 : rc;
}

static int qti_qpt_start_stop_telemetry(struct qpt_priv *qpt, bool enable)
{
	int rc = 0;
	uint8_t status = 0;
	uint8_t tel_en = enable ? 0x80 : 0;
	uint8_t trig_set = 1;

	if (qpt->enabled == enable)
		return 0;

	mutex_lock(&qpt->hw_read_lock);

	rc = qpt_sdam_nvmem_read(qpt, &qpt->sdam[DATA_AVG_SDAM],
			QPT_DATA_SDAM_BASE_OFF + DATA_SDAM_TELEMETRY_ENABLE,
			1, (void *)&status);
	if (rc < 0)
		goto err_exit;

	rc = qpt_sdam_nvmem_write(qpt, &qpt->sdam[DATA_AVG_SDAM],
			QPT_DATA_SDAM_BASE_OFF + DATA_SDAM_TELEMETRY_ENABLE,
			1, (void *)&tel_en);
	if (rc < 0)
		goto err_exit;

	rc = qpt_sdam_nvmem_write(qpt, &qpt->sdam[DATA_AVG_SDAM],
			DATA_SDAM_TRIG_SET,
			1, (void *)&trig_set);
	if (rc < 0)
		goto err_exit;

	qpt->enabled = enable;
	if (enable)
		qpt->last_enabled_ts = ktime_get();
	else
		qpt->last_disbled_ts = ktime_get();

	QPT_DBG_EVENT(qpt, "qpt is %s with last rtc_ts:%d",
			enable ? "enabled" : "disabled", qpt->rtc_ts);
	/* Better to clear all buffer */
	if (!enable)
		qti_qpt_clear_all_channel_data(qpt);
err_exit:
	mutex_unlock(&qpt->hw_read_lock);

	return rc < 0 ? rc : 0;
}

static int qti_qpt_read_rtc_time(struct qpt_priv *qpt, u32 *rtc_ts)
{
	int rc = 0;
	u8 val[4];

	rc = qpt_sdam_nvmem_read(qpt, &qpt->sdam[DATA_AVG_SDAM],
			QPT_DATA_SDAM_BASE_OFF + DATA_SDAM_TSTAMP0, sizeof(val), val);
	if (rc < 0)
		return rc;

	*rtc_ts = get_unaligned_le32(val);

	return 0;
}

static int qti_qpt_cache_data_to_overflow_buffer(struct qpt_priv *qpt)
{
	struct qpt_device *qpt_dev;

	qpt->last_overflow_ts = qpt->hw_read_ts;
	qpt->last_overflow_rtc_ts = qpt->rtc_ts;
	qpt->overflow_counter++;

	list_for_each_entry(qpt_dev, &qpt->qpt_dev_head, qpt_node) {
		if (!qpt_dev->enabled)
			continue;
		qpt_dev->prev_buffer_energy += qpt_dev->total_energey_uj;
		qpt_dev->prev_buffer_data_uw += qpt_dev->last_data_uw;
		qpt_dev->prev_buffer_data_adc += qpt_dev->last_data;

		QPT_DBG_EVENT(qpt_dev->priv,
			"overflow: qpt[0x%x]:last data uw:%llu", qpt_dev->ch_id,
				qpt_dev->last_data_uw);
	}

	return 0;
}

static u64 get_unaligned_le40(const u8 *p)
{
	return (u64)p[0] | (u64)p[1] << 8 | (u64)p[2] << 16 |
			(u64)p[3] << 24 | (u64)p[4] << 32;
}

static void qpt_channel_avg_data_update(struct qpt_device *qpt_dev,
		uint8_t *base, enum qpt_channel_type type, u32 cur_rtc_ts)
{
	u64 curr_total_energy = 0, cur_last_data = 0, cur_last_data_uw = 0;
	u32 rtcdiff;
	u32 tot_np;
	struct qpt_priv *qpt = qpt_dev->priv;

	mutex_lock(&qpt_dev->lock);

	cur_last_data = get_unaligned_le40(base);
	cur_last_data_uw = QPT_GET_CMLTV_POWER_UW_FROM_ADC(qpt, cur_last_data);

	/*
	 * Calculate cumulative energy
	 * cumulative energy = cumulative power * Tperiod in second
	 * total cumulative energy = cumulative energy + prev_buffer_energy;
	 * Calculate RTCdiff = RTCnew – RTCold (Elapsed time since last reading)
	 * Calculate # of Tperiods (Nperiod) within the elapsed time
	 * Pavg = (Pnew – Pold)/Nperiod
	 **/
	rtcdiff = cur_rtc_ts - qpt->rtc_ts;

	if (type == QPT_BOB) {
		curr_total_energy = cur_last_data_uw * qpt->bob_tperiod / 1000;
		tot_np = rtcdiff / qpt->bob_tperiod;
	} else {
		curr_total_energy = cur_last_data_uw * qpt->tperiod / 1000;
		tot_np = rtcdiff / qpt->tperiod;
	}

	qpt_dev->total_energey_uj = curr_total_energy + qpt_dev->prev_buffer_energy;
	qpt_dev->pavg = (cur_last_data_uw - qpt_dev->last_data_uw) / tot_np;

	qpt_dev->last_data = cur_last_data;
	qpt_dev->last_data_uw = cur_last_data_uw;

	mutex_unlock(&qpt_dev->lock);

	trace_qpt_data_update(qpt_dev->ch_id, qpt_dev->last_data,
				qpt_dev->total_energey_uj, qpt_dev->pavg);
	QPT_DBG_DATA(qpt_dev->priv,
		"qpt[0x%x]: tot_power:%lluuw ADC:0x%llx tot_energy:%lluuj powavg %lluuw",
			qpt_dev->ch_id, qpt_dev->last_data_uw, qpt_dev->last_data,
			qpt_dev->total_energey_uj, qpt_dev->pavg);
}

static int qti_qpt_read_seq_count(struct qpt_priv *qpt, int *seq_count,
		enum qpt_channel_type type)
{
	int rc = -1;

	rc = qpt_sdam_nvmem_read(qpt, &qpt->sdam[DATA_AVG_SDAM],
		QPT_DATA_SDAM_BASE_OFF +
		(type == QPT_BOB ? DATA_SDAM_BOB_COUNT : DATA_SDAM_DATA_READY_COUNT),
		1, &seq_count);
	if (rc < 0)
		return rc;

	return 0;
}

static int qti_qpt_read_all_data(struct qpt_priv *qpt, uint16_t offset, size_t size,
		enum qpt_channel_type type)
{
	uint8_t data_sdam_avg[DATA_SDAM_AVGPWR4_CH23 + 1] = {0};
	int seq_count = 0;
	int rc = 0;
	struct qpt_device *qpt_dev;
	int seq_count_start = -1;
	u32 rtc_ts = 0;

	qpt->hw_read_ts = ktime_get();
	rc = qti_qpt_read_seq_count(qpt, &seq_count, type);
	if (rc < 0)
		return rc;

	do {
		seq_count_start = seq_count;
		rc = qpt_sdam_nvmem_read(qpt, &qpt->sdam[DATA_AVG_SDAM], offset,
				size, data_sdam_avg);
		if (rc < 0)
			return rc;

		rc = qti_qpt_read_seq_count(qpt, &seq_count, type);
		if (rc < 0)
			return rc;
	} while (seq_count < seq_count_start);

	qti_qpt_read_rtc_time(qpt, &rtc_ts);
	list_for_each_entry(qpt_dev, &qpt->qpt_dev_head, qpt_node) {
		if (!qpt_dev->enabled || qpt_dev->type != type)
			continue;
		if (qpt_dev->data_offset >= (offset + size))
			continue;
		qpt_channel_avg_data_update(qpt_dev,
			&data_sdam_avg[qpt_dev->data_offset], type, rtc_ts);
	}
	qpt->rtc_ts = rtc_ts;

	if (type == QPT_BOB)
		return 0;

	QPT_DBG_DATA(qpt, "Time(us) to read all channel:%lldus & RTC Time:%u",
		ktime_to_us(ktime_sub(ktime_get(), qpt->hw_read_ts)),
		qpt->rtc_ts);

	return 0;
}

static u64 qti_qpt_get_energy(struct powerzone *pz)
{
	u64 energy;
	struct qpt_device *qpt_dev = (struct qpt_device *)pz->devdata;

	mutex_lock(&qpt_dev->lock);
	energy = qpt_dev->total_energey_uj;
	mutex_unlock(&qpt_dev->lock);

	return energy;
}

static u64 qti_qpt_get_power(struct powerzone *pz)
{
	u64 power;
	struct qpt_device *qpt_dev = (struct qpt_device *)pz->devdata;

	mutex_lock(&qpt_dev->lock);
	power = qpt_dev->pavg;
	mutex_unlock(&qpt_dev->lock);

	return power;
}

static int qti_qpt_set_enable(struct powerzone *pz, bool enable)
{
	struct qpt_device *qpt_dev = (struct qpt_device *)pz->devdata;
	struct qpt_priv *qpt = qpt_dev->priv;
	int ret = 0;

	if (qti_qpt_telemetry_status(qpt) == enable)
		return 0;

	if (enable) {
		ret = qti_qpt_sync_common_telemetry_config(qpt);
		if (ret)
			return ret;
	}

	return qti_qpt_start_stop_telemetry(qpt, enable);
}

static bool qti_qpt_get_enable(struct powerzone *pz)
{
	struct qpt_device *qpt_dev = (struct qpt_device *)pz->devdata;
	struct qpt_priv *qpt = qpt_dev->priv;

	return qti_qpt_telemetry_status(qpt);
}

static struct powerzone_ops qpt_ops = {
	.get_energy = qti_qpt_get_energy,
	.get_power = qti_qpt_get_power,
	.set_enable = qti_qpt_set_enable,
	.get_enable = qti_qpt_get_enable,
};

static int qti_qpt_read_data_update(struct qpt_priv *qpt)
{
	int rc = 0;

	mutex_lock(&qpt->hw_read_lock);
	rc = qti_qpt_read_all_data(qpt,
		QPT_DATA_SDAM_BASE_OFF + DATA_SDAM_AVGPWR0_CH1,
		qpt->last_ch_offset + QPT_DATA_BYTE_SIZE, QPT_DEFEAULT);

	/* TO DO: Need optimization for bob channel */
	if (qpt->bob_reg_cnt) {
		rc = qti_qpt_read_all_data(qpt,
			QPT_DATA_SDAM_BASE_OFF + qpt->first_bob_offset,
			qpt->last_bob_offset + QPT_DATA_BYTE_SIZE, QPT_BOB);
	}
	mutex_unlock(&qpt->hw_read_lock);

	if (rc < 0)
		return rc;

	return 0;
}

static int qti_qpt_overflow_ack_back_sdam(struct qpt_priv *qpt)
{
	int trig_set = 1;

	return qpt_sdam_nvmem_write(qpt, &qpt->sdam[DATA_FRAC_SDAM],
		DATA_SDAM_TRIG_SET, 1, (void *)&trig_set);
}

static irqreturn_t qpt_sdam_irq_handler(int irq, void *data)
{
	struct qpt_priv *qpt = data;

	qti_qpt_read_data_update(qpt);

	return IRQ_HANDLED;
}

static irqreturn_t qpt_sdam_overflow_irq_handler(int irq, void *data)
{
	int ret;
	struct qpt_priv *qpt = data;

	ret = qti_qpt_read_data_update(qpt);
	if (ret < 0)
		dev_err(qpt->dev, "qpt channel read error\n");

	QPT_DBG_EVENT(qpt, "Channel overflow: rtc timestamp:%u", qpt->rtc_ts);
	ret = qti_qpt_cache_data_to_overflow_buffer(qpt);

	ret = qti_qpt_overflow_ack_back_sdam(qpt);
	if (ret < 0)
		dev_err(qpt->dev, "qpt channel overflow ack error\n");

	return IRQ_HANDLED;
}

static enum qpt_channel_type get_reg_channel_type(struct qpt_priv *qpt, u16 ppid)
{
	int i;

	for (i = 0; i < qpt->bob_reg_cnt; i++) {
		if (qpt->bob_reg_ppids[i] == ppid)
			return QPT_BOB;
	}

	return QPT_DEFEAULT;
}

static int qti_qpt_config_sdam_initialize(struct qpt_priv *qpt)
{
	uint8_t *config_sdam = NULL;
	struct qpt_device *qpt_dev = NULL;
	int rc = 0;
	uint8_t conf_idx, data_idx;

	rc = qti_qpt_sync_common_telemetry_config(qpt);
	if (rc < 0)
		return rc;

	qpt->first_bob_offset = 0xff;
	config_sdam = qpt->config_sdam_data;
	/* logic to read number of channels and die_temps */
	for (conf_idx = CONFIG_SDAM_CH1_CONFIG0, data_idx = 0;
	 conf_idx <= CONFIG_SDAM_CH23_CONFIG0;
	 conf_idx += 3, data_idx += QPT_DATA_BYTE_SIZE) {

		if ((!(config_sdam[conf_idx] & QPT_CH_ENABLE_MASK_0)) ||
			(!(config_sdam[conf_idx + 2] & QPT_CH_ENABLE_MASK_1)))
			continue;

		qpt->num_reg++;
		qpt_dev = devm_kzalloc(qpt->dev, sizeof(*qpt_dev), GFP_KERNEL);
		if (!qpt_dev)
			return -ENOMEM;
		qpt_dev->enabled = true;
		qpt_dev->sid = config_sdam[conf_idx] & QPT_SID_MASK;
		qpt_dev->pid = config_sdam[conf_idx + 1];
		qpt_dev->ch_id = qpt_dev->sid << 8 | qpt_dev->pid;
		qpt_dev->priv = qpt;
		qpt_dev->data_offset = data_idx;
		qpt_dev->type = get_reg_channel_type(qpt, qpt_dev->ch_id);
		if (qpt_dev->type == QPT_BOB) {
			if (data_idx > qpt->last_bob_offset)
				qpt->last_bob_offset = data_idx;

			if (data_idx < qpt->first_bob_offset)
				qpt->first_bob_offset = data_idx;
		} else {
			if (data_idx > qpt->last_ch_offset)
				qpt->last_ch_offset = data_idx;
		}
		mutex_init(&qpt_dev->lock);

		dev_dbg(qpt->dev, "%s: qpt channel id:0x%x off:0x%x type:%d\n", __func__,
				qpt_dev->ch_id, data_idx, qpt_dev->type);

		list_add(&qpt_dev->qpt_node, &qpt->qpt_dev_head);
		qpt_dev->pz = qptm_channel_register(qpt->dev, qpt_dev->ch_id,
					&qpt_ops, qpt_dev);
	}

	return 0;
}

static int qpt_get_sdam_nvmem(struct device *dev, struct qpt_sdam *sdam,
			char *sdam_name)
{
	int rc = 0;

	sdam->nvmem = devm_nvmem_device_get(dev, sdam_name);
	if (IS_ERR(sdam->nvmem)) {
		rc = PTR_ERR(sdam->nvmem);
		if (rc != -EPROBE_DEFER)
			dev_err(dev, "Failed to get nvmem device, rc=%d\n",
				rc);
		sdam->nvmem = NULL;
		return rc;
	}

	return rc;
}

static int qpt_parse_sdam_data(struct qpt_priv *qpt)
{
	int rc = 0;
	char buf[20];

	rc = of_property_count_strings(qpt->dev->of_node, "nvmem-names");
	if (rc < 0) {
		dev_err(qpt->dev, "Could not find nvmem device\n");
		return rc;
	}
	if (rc != MAX_QPT_SDAM) {
		dev_err(qpt->dev, "Invalid num of SDAMs:%d\n", rc);
		return -EINVAL;
	}

	qpt->num_sdams = rc;
	qpt->sdam = devm_kcalloc(qpt->dev, qpt->num_sdams,
				sizeof(*qpt->sdam), GFP_KERNEL);
	if (!qpt->sdam)
		return -ENOMEM;

	/* Check for config sdam */
	qpt->sdam[0].id = CONFIG_SDAM;
	scnprintf(buf, sizeof(buf), "qpt-config-sdam");
	mutex_init(&qpt->sdam[0].lock);
	rc = qpt_get_sdam_nvmem(qpt->dev, &qpt->sdam[0], buf);
	if (rc < 0)
		return rc;

	/* Check frac sdam */
	qpt->sdam[1].id = DATA_FRAC_SDAM;
	mutex_init(&qpt->sdam[1].lock);
	scnprintf(buf, sizeof(buf), "qpt-frac-sdam");
	rc = qpt_get_sdam_nvmem(qpt->dev, &qpt->sdam[1], buf);
	if (rc < 0)
		return rc;

	/* Check data sdam */
	qpt->sdam[2].id = DATA_AVG_SDAM;
	mutex_init(&qpt->sdam[2].lock);
	scnprintf(buf, sizeof(buf), "qpt-data-sdam");
	rc = qpt_get_sdam_nvmem(qpt->dev, &qpt->sdam[2], buf);
	if (rc < 0)
		return rc;

	return 0;
}

static int qpt_pd_callback(struct notifier_block *nfb,
				unsigned long action, void *v)
{
	struct qpt_priv *qpt = container_of(nfb, struct qpt_priv, genpd_nb);
	struct qpt_device *qpt_dev = NULL;
	ktime_t now;
	s64 diff;

	if (atomic_read(&qpt->in_suspend))
		goto cb_exit;

	switch (action) {
	case GENPD_NOTIFY_OFF:
		if (qpt->data_irq_enabled) {
			disable_irq_nosync(qpt->data_ready_irq);
			qpt->data_irq_enabled = false;
		}
		break;
	case GENPD_NOTIFY_ON:
		if (qpt->data_irq_enabled)
			break;
		now = ktime_get();
		diff = ktime_to_ms(ktime_sub(now, qpt->hw_read_ts));
		if (diff > qpt->data_update_sampling) {
			list_for_each_entry(qpt_dev, &qpt->qpt_dev_head,
					qpt_node)
				qpt_dev->pavg = 0;
		}
		enable_irq(qpt->data_ready_irq);
		qpt->data_irq_enabled = true;
		break;
	default:
		break;
	}
cb_exit:
	return NOTIFY_OK;
}

static int qti_qpt_pd_notifier_register(struct qpt_priv *qpt, struct device *dev)
{
	int ret;

	pm_runtime_enable(dev);
	qpt->genpd_nb.notifier_call = qpt_pd_callback;
	qpt->genpd_nb.priority = INT_MIN;
	ret = dev_pm_genpd_add_notifier(dev, &qpt->genpd_nb);
	if (ret)
		pm_runtime_disable(dev);
	return ret;
}

static int qpt_parse_dt(struct qpt_priv *qpt)
{
	struct platform_device *pdev;
	int rc = 0;
	struct device_node *np = qpt->dev->of_node;

	pdev = of_find_device_by_node(np);
	if (!pdev) {
		dev_err(qpt->dev, "Invalid pdev\n");
		return -ENODEV;
	}

	rc = qpt_parse_sdam_data(qpt);
	if (rc < 0)
		return rc;

	rc = of_property_count_elems_of_size(np, "bob-channels",
						sizeof(u16));
	if (rc > 0 && rc < QPT_POWER_CH_MAX) {
		qpt->bob_reg_cnt = rc;
		rc = of_property_read_u16_array(np, "bob-channels",
			qpt->bob_reg_ppids, rc);
		if (rc < 0) {
			dev_err(qpt->dev,
				"Failed to read ppid mapping array, rc = %d\n", rc);
			return rc;
		}
	}

	rc = platform_get_irq(pdev, 0);
	if (rc <= 0) {
		dev_err(qpt->dev, "Failed to get qpt irq, rc=%d\n", rc);
		return -EINVAL;
	}
	qpt->data_ready_irq = rc;

	rc = platform_get_irq(pdev, 1);
	if (rc <= 0) {
		dev_err(qpt->dev, "Failed to get qpt irq, rc=%d\n", rc);
		return -EINVAL;
	}
	qpt->buf_overflow_irq = rc;

	if (of_find_property(np, "power-domains", NULL) && pdev->dev.pm_domain) {
		rc = qti_qpt_pd_notifier_register(qpt, &pdev->dev);
		if (rc) {
			dev_err(qpt->dev, "Failed to register for pd notifier\n");
			return rc;
		}
	}

	return 0;
}

static int qti_qpt_hw_init(struct qpt_priv *qpt)
{
	int rc;

	if (qpt->enabled)
		return 0;

	mutex_init(&qpt->hw_read_lock);
	INIT_LIST_HEAD(&qpt->qpt_dev_head);

	rc = qpt_parse_dt(qpt);
	if (rc < 0) {
		dev_err(qpt->dev, "Failed to parse qpt rc=%d\n", rc);
		return rc;
	}

	rc = qti_qpt_config_sdam_initialize(qpt);
	if (rc < 0) {
		dev_err(qpt->dev, "Failed to parse config sdam rc=%d\n", rc);
		return rc;
	}
	atomic_set(&qpt->in_suspend, 0);

	rc = devm_request_threaded_irq(qpt->dev, qpt->data_ready_irq,
			NULL, qpt_sdam_irq_handler,
			IRQF_ONESHOT, "qpt_data_irq", qpt);
	if (rc < 0) {
		dev_err(qpt->dev,
			"Failed to request IRQ for qpt, rc=%d\n", rc);
		return rc;
	}
	irq_set_status_flags(qpt->data_ready_irq, IRQ_DISABLE_UNLAZY);
	qpt->data_irq_enabled = true;

	rc = devm_request_threaded_irq(qpt->dev, qpt->buf_overflow_irq,
			NULL, qpt_sdam_overflow_irq_handler,
			IRQF_ONESHOT, "qpt_overflow_irq", qpt);
	if (rc < 0) {
		dev_err(qpt->dev,
			"Failed to request IRQ for qpt, rc=%d\n", rc);
		return rc;
	}
	/* overflow irq needs to be wake up capable */
	enable_irq_wake(qpt->buf_overflow_irq);

	/* Update first reading for all channels */
	qti_qpt_read_data_update(qpt);

	return 0;
}

static int qti_qpt_suspend(struct device *dev)
{
	struct qpt_priv *qpt = dev_get_drvdata(dev);

	atomic_set(&qpt->in_suspend, 1);

	if (qpt->data_irq_enabled) {
		disable_irq_nosync(qpt->data_ready_irq);
		qpt->data_irq_enabled = false;
	}

	return 0;
}

static int qti_qpt_resume(struct device *dev)
{
	struct qpt_priv *qpt = dev_get_drvdata(dev);
	struct qpt_device *qpt_dev = NULL;
	ktime_t now;
	s64 diff;

	now = ktime_get();
	diff = ktime_to_ms(ktime_sub(now, qpt->hw_read_ts));
	if (diff > qpt->data_update_sampling) {
		list_for_each_entry(qpt_dev, &qpt->qpt_dev_head,
					qpt_node)
			qpt_dev->pavg = 0;
	}

	if (!qpt->data_irq_enabled) {
		enable_irq(qpt->data_ready_irq);
		qpt->data_irq_enabled = true;
	}
	atomic_set(&qpt->in_suspend, 0);

	return 0;
}

static void qti_qpt_hw_release(struct qpt_priv *qpt)
{
	pm_runtime_disable(qpt->dev);
	dev_pm_genpd_remove_notifier(qpt->dev);
	struct qpt_device *qpt_dev, *aux;

	qti_qpt_start_stop_telemetry(qpt, false);
	list_for_each_entry_safe(qpt_dev, aux, &qpt->qpt_dev_head, qpt_node) {
		if (qpt_dev->pz)
			qptm_channel_unregister(qpt_dev->pz);
		list_del(&qpt_dev->qpt_node);
	}
}

static int qpt_hw_device_probe(struct platform_device *pdev)
{
	int ret;
	struct qpt_priv *qpt;

	qpt = devm_kzalloc(&pdev->dev, sizeof(*qpt), GFP_KERNEL);
	if (!qpt)
		return -ENOMEM;

	qpt->dev = &pdev->dev;
	qpt->ipc_log_data = ipc_log_context_create(IPC_LOGPAGES, "qpt_data", 0);
	if (!qpt->ipc_log_data)
		dev_err(qpt->dev, "%s: unable to create IPC Logging for %s\n",
					__func__, "qpt_data");

	qpt->ipc_log_event = ipc_log_context_create(IPC_LOGPAGES, "qpt_event", 0);
	if (!qpt->ipc_log_event)
		dev_err(qpt->dev, "%s: unable to create IPC Logging for %s\n",
					__func__, "qpt_event");

	ret = qti_qpt_hw_init(qpt);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: init failed\n", __func__);
		return ret;
	}
	platform_set_drvdata(pdev, qpt);
	dev_set_drvdata(qpt->dev, qpt);

	return 0;
}

static int qpt_hw_device_remove(struct platform_device *pdev)
{
	struct qpt_priv *qpt = platform_get_drvdata(pdev);

	qti_qpt_hw_release(qpt);
	return 0;
}

static const struct dev_pm_ops qpt_pm_ops = {
	.suspend = qti_qpt_suspend,
	.resume = qti_qpt_resume,
};

static const struct of_device_id qpt_hw_device_match[] = {
	{.compatible = "qcom,power-telemetry-v2"},
	{}
};

static struct platform_driver qpt_hw_device_driver = {
	.probe          = qpt_hw_device_probe,
	.remove         = qpt_hw_device_remove,
	.driver         = {
		.name   = QPT_HW,
		.pm = &qpt_pm_ops,
		.of_match_table = qpt_hw_device_match,
	},
};

module_platform_driver(qpt_hw_device_driver);

MODULE_DESCRIPTION("Qualcomm Technologies, Inc. Power Telemetry v2 driver");
MODULE_LICENSE("GPL");
