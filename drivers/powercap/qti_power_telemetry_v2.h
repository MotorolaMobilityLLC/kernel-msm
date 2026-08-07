/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __QCOM_QPT_H__
#define __QCOM_QPT_H__

#include <linux/interrupt.h>
#include <linux/ipc_logging.h>
#include <linux/soc/qcom/qptf.h>

struct qpt_priv;
struct qpt_device;

#define IPC_LOGPAGES 10
#define QPT_DBG_DATA(qpt, msg, args...) do {			\
		dev_dbg(qpt->dev, "%s:" msg, __func__, args);	\
		if ((qpt) && (qpt)->ipc_log_data) {		\
			ipc_log_string((qpt)->ipc_log_data,	\
			"[%s] "msg"\n",				\
			 current->comm, args);			\
		}						\
	} while (0)

#define QPT_DBG_EVENT(qpt, msg, args...) do {			\
		dev_dbg(qpt->dev, "%s:" msg, __func__, args);	\
		if ((qpt) && (qpt)->ipc_log_event) {		\
			ipc_log_string((qpt)->ipc_log_event,	\
			"[%s] "msg"\n",				\
			 current->comm, args);			\
		}						\
	} while (0)

#define QPT_REG_NAME_LENGTH 32
#define QPT_POWER_CH_MAX  23

/* Different qpt channel types */
enum qpt_channel_type {
	QPT_DEFEAULT = 0,
	QPT_BOB,
};

/* Different qpt sdam IDs to use as an index into an array */
enum qpt_sdam_id {
	CONFIG_SDAM,
	DATA_FRAC_SDAM,
	DATA_AVG_SDAM,
	MAX_QPT_SDAM
};

/* Data sdam field IDs to use as an index into an array */
enum data_sdam_field_ids {
	DATA_SDAM_TELEMETRY_ENABLE,
	DATA_SDAM_BUFFER_FULL_STATUS,
	DATA_SDAM_DATA_READY_COUNT,
	DATA_SDAM_BOB_COUNT,
	DATA_SDAM_TSTAMP0,
	DATA_SDAM_TSTAMP1,
	DATA_SDAM_TSTAMP2,
	DATA_SDAM_TSTAMP3,
	DATA_SDAM_AVGPWR0_CH1,
	DATA_SDAM_AVGPWR1_CH1,
	DATA_SDAM_AVGPWR2_CH1,
	DATA_SDAM_AVGPWR3_CH1,
	DATA_SDAM_AVGPWR4_CH1,
	DATA_SDAM_AVGPWR0_CH23 = DATA_SDAM_AVGPWR0_CH1 +  5 * (QPT_POWER_CH_MAX - 1),
	DATA_SDAM_AVGPWR1_CH23,
	DATA_SDAM_AVGPWR2_CH23,
	DATA_SDAM_AVGPWR3_CH23,
	DATA_SDAM_AVGPWR4_CH23,
	DATA_SDAM_TRIG_SET = 0xe5,
	MAX_SDAM_DATA
};

/* config sdam field IDs to use as an index into an array */
enum config_sdam_field_ids {
	CONFIG_SDAM_DATA_READY_MAX_COUNT,
	CONFIG_SDAM_BOB_MAX_COUNT,
	CONFIG_SDAM_HSR_VER,
	CONFIG_SDAM_SPARE,
	CONFIG_SDAM_TELEMETRY_TIMER_LB,
	CONFIG_SDAM_TELEMETRY_TIMER_UB,
	CONFIG_SDAM_TELEMETRY_CONFIG0,
	CONFIG_SDAM_TELEMETRY_CONFIG1,
	CONFIG_SDAM_CH1_CONFIG0,
	CONFIG_SDAM_CH1_PID,
	CONFIG_SDAM_CH1_CONFIG1,
	CONFIG_SDAM_CH23_CONFIG0 = CONFIG_SDAM_CH1_CONFIG0 + 3 * (QPT_POWER_CH_MAX - 1),
	MAX_CONFIG_SDAM_DATA
};

/**
 * struct qpt_sdam - QPT sdam data structure
 * @id:		QPT sdam id type
 * @nvmem:	Pointer to nvmem device
 * @lock:	lock to protect multiple read concurrently
 * @last_data:	last full read data copy for current sdam
 */
struct qpt_sdam {
	enum qpt_sdam_id	id;
	struct nvmem_device	*nvmem;
	struct mutex		lock;
	uint8_t			last_data[MAX_CONFIG_SDAM_DATA];
};

/**
 * struct qpt_device -  Each regulator channel device data
 * @qpt_node:	qpt device list head member to traverse all devices
 * @priv:	qpt hardware instance that this channel is connected to
 * @pz:	        array of power zone data types for different data retrieval
 * @type:	type of the channel. regular channel or BoB channel
 * @enabled:	qpt channel is enabled or not
 * @sid:	qpt channel SID
 * @pid:	qpt channel PID
 * @ch_id:	qpt channel id
 * @data_offset:	qpt channel power data offset from DATA sdam base
 * @last_data:	qpt channel last adc data from SDAM
 * @last_data_uw:	qpt channel last cumulative data without time
 * @total_energey_uj:	qpt channel cumulative energy data after read
 * @pavg:	qpt channel average power data over reporting interval
 * @prev_buffer_energy:	 Previously accumulated energy before overflow
 * @prev_buffer_data_uw:  Previously accumulated power value before overflow
 * @prev_buffer_data_adc: Previously accumulated adc value before overflow
 * @lock:	lock to protect multiple client read concurrently
 */
struct qpt_device {
	struct list_head		qpt_node;
	struct qpt_priv			*priv;
	struct powerzone		*pz;
	enum qpt_channel_type		type;
	bool				enabled;
	uint8_t				sid;
	uint8_t				pid;
	uint16_t			ch_id;
	uint8_t				data_offset;
	u64				last_data;
	u64				last_data_uw;
	u64				total_energey_uj;
	u64				pavg;
	u64				prev_buffer_energy;
	u64				prev_buffer_data_uw;
	u64				prev_buffer_data_adc;
	struct mutex			lock;
};

/**
 * struct qpt_priv - Structure for QPT hardware private data
 * @dev:		Pointer for QPT device
 * @sdam:		Pointer for array of QPT sdams
 * @data_ready_irq:	QPT data update irq number
 * @buf_overflow_irq:	QPT channel overflow irq number
 * @num_sdams:		Number of SDAMs used for QPT from DT
 * @num_reg:		Number of regulator based on config sdam
 * @bob_reg_ppids:	array of BoB channel id from devicetree
 * @bot_reg_cnt:	Number of regulator count in devicetree
 * @last_ch_offset:	Last enabled data channel offset
 * @first_bob_offset:	First enabled BoB channel_offset
 * @last_bob_offset:	Last enabled BoB channel_offset
 * @ready_max_count:	channel data sample count
 * @bob_max_count:	BoB channel sample count
 * @hsr_ver:		QPT hardware revision
 * @enabled:		QPT hardware initialization is done if it is true
 * @data_irq_enabled:	The qpt data irq enable/disable status
 * @in_suspend:		The QPT driver suspend status
 * @config_sdam_data:	Config sdam data dump collected at init
 * @ipc_log_data:	Handle to ipc_logging for data update buffer
 * @ipc_log_event:	Handle to ipc_logging for qpt events
 * @adc_scaling_factor: ADC to power unit scaling factor
 * @tperiod:		channel pmic hardware time period
 * @bob_tperiod:	BoB channel pmic hardware time period
 * @data_update_sampling: Hardware data reporting period in milliseconds
 * @overflow_counter:   Counter to capture number of channel overflow trigger
 * @rtc_ts:		RTC Timestamp collected just after qpt irq data update
 * @hw_read_ts:		Timestamp collected just after qpt irq data update
 * @last_overflow_ts:   Kernel Timestamp collected during last channel overflow
 * @last_overflow_rtc_ts: RTC Timestamp collected during last channel overflow
 * @last_enabled_ts:    Kernel Timestamp collected during last enable request
 * @last_disbled_ts:    Kernel Timestamp collected during last disable request
 * @qpt_dev_head:	List head for all qpt channel devices
 * @hw_read_lock:	lock to protect avg data update and client request
 * @genpd_nb:		Genpd notifier for apps idle notification
 */
struct qpt_priv {
	struct device		*dev;
	struct qpt_sdam		*sdam;
	int			data_ready_irq;
	int			buf_overflow_irq;
	u32			num_sdams;
	u32			num_reg;
	u16			bob_reg_ppids[QPT_POWER_CH_MAX];
	u8			bob_reg_cnt;
	u8			last_ch_offset;
	u8			first_bob_offset;
	u8			last_bob_offset;
	u8			ready_max_count;
	u8			bob_max_count;
	u8			hsr_ver;
	bool			enabled;
	bool			data_irq_enabled;
	atomic_t		in_suspend;
	uint8_t			*config_sdam_data;
	void			*ipc_log_data;
	void			*ipc_log_event;
	u32			adc_scaling_factor;
	u32			tperiod;
	u32			bob_tperiod;
	u32			data_update_sampling;
	u32			overflow_counter;
	u32			rtc_ts;
	u64			hw_read_ts;
	u64			last_overflow_ts;
	u64			last_overflow_rtc_ts;
	u64			last_enabled_ts;
	u64			last_disbled_ts;
	struct list_head	qpt_dev_head;
	struct mutex		hw_read_lock;
	struct notifier_block	genpd_nb;
};

#endif /* __QCOM_QPT_H__ */
