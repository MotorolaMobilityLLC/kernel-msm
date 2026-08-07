/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM bus_prof

#if !defined(_TRACE_BUS_PROF_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_BUS_PROF_H
#include "bus_prof.h"
#include <linux/tracepoint.h>


TRACE_EVENT(memory_miss_last_sample,

	TP_PROTO(u64 qtime, struct llcc_miss_buf *master_buf0, struct llcc_miss_buf *master_buf1),

	TP_ARGS(qtime, master_buf0, master_buf1),

	TP_STRUCT__entry(
		__field(u64, qtime)
		__field(u8, master1)
		__field(u16, miss1)
		__field(u8, master2)
		__field(u16, miss2)
	),

	TP_fast_assign(
		__entry->qtime = qtime;
		__entry->master1 = master_buf0->master_id;
		__entry->miss1 = master_buf0->miss_info;
		__entry->master2 = master_buf1->master_id;
		__entry->miss2 = master_buf1->miss_info;
	),

	TP_printk("qtime=%llu master1=%u miss1=%u master2=%u miss2=%u",
		__entry->qtime,
		__entry->master1,
		__entry->miss1,
		__entry->master2,
		__entry->miss2)
);

TRACE_EVENT(llcc_occupancy_last_sample,

	TP_PROTO(u64 qtime, int master, u32 curr_cap, u32 max_cap),

	TP_ARGS(qtime, master, curr_cap, max_cap),

	TP_STRUCT__entry(
		__field(u64, qtime)
		__field(int, master)
		__field(u32, curr_cap)
		__field(u32, max_cap)
	),

	TP_fast_assign(
		__entry->qtime = qtime;
		__entry->master = master;
		__entry->curr_cap = curr_cap;
		__entry->max_cap = max_cap;
	),

	TP_printk("qtime=%llu master=%u curr_cap=%u max_cap=%u",
		__entry->qtime,
		__entry->master,
		__entry->curr_cap,
		__entry->max_cap)
);

TRACE_EVENT(memory_lat_last_sample,

	TP_PROTO(u64 qtime, int master, u32 *bin),

	TP_ARGS(qtime, master, bin),

	TP_STRUCT__entry(
		__field(u64, qtime)
		__field(int, master)
		__field(u32, bin0)
		__field(u32, bin1)
		__field(u32, bin2)
		__field(u32, bin3)
		__field(u32, bin4)
		__field(u32, bin5)
		__field(u32, bin6)
		__field(u32, bin7)
	),

	TP_fast_assign(
		__entry->qtime = qtime;
		__entry->master = master;
		__entry->bin0 = bin[0];
		__entry->bin1 = bin[1];
		__entry->bin2 = bin[2];
		__entry->bin3 = bin[3];
		__entry->bin4 = bin[4];
		__entry->bin5 = bin[5];
		__entry->bin6 = bin[6];
		__entry->bin7 = bin[7];
	),

	TP_printk("qtime=%llu master = %d bin0=%u bin1=%u bin2=%u bin3=%u bin4=%u bin5=%u bin6=%u bin7=%u",
		__entry->qtime,
		__entry->master,
		__entry->bin0,
		__entry->bin1,
		__entry->bin2,
		__entry->bin3,
		__entry->bin4,
		__entry->bin5,
		__entry->bin6,
		__entry->bin7)
);
#endif /* _TRACE_BUS_PROF_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ../../drivers/soc/qcom/dcvs

#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE trace-bus-prof

#include <trace/define_trace.h>
