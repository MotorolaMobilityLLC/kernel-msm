/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM qptf

#if !defined(_TRACE_QPT_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_QPT

#include <linux/tracepoint.h>

DECLARE_EVENT_CLASS(qpt_update_channel_data,

	TP_PROTO(unsigned int channel_id,
	unsigned long long adc, unsigned long long energy, unsigned long long power),

	TP_ARGS(channel_id, adc, energy, power),

	TP_STRUCT__entry(
		__field(unsigned int,	channel_id)
		__field(unsigned long long,	adc)
		__field(unsigned long long,	energy)
		__field(unsigned long long,	power)
	),

	TP_fast_assign(
		__entry->channel_id = channel_id;
		__entry->adc = adc;
		__entry->energy = energy;
		__entry->power = power;
	),

	TP_printk("Channel:0x%x adc_value:0x%llx energy:%lluuj avg_power:%lluuw",
		__entry->channel_id, __entry->adc, __entry->energy, __entry->power)
);

DEFINE_EVENT(qpt_update_channel_data, qpt_data_update,

	TP_PROTO(unsigned int channel_id,
	unsigned long long adc, unsigned long long energy, unsigned long long power),

	TP_ARGS(channel_id, adc, energy, power)
);

#endif /* _TRACE_QPT */

/* This part must be outside protection */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .

#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE trace
#include <trace/define_trace.h>
