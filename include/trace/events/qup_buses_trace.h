/* SPDX-License-Identifier: GPL-2.0
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM qup_buses_trace

#if !defined(_TRACE_QUP_BUSES_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_QUP_BUSES_TRACE_H

#include <linux/ktime.h>
#include <linux/tracepoint.h>

#define MAX_MSG_LEN 256

TRACE_EVENT(buses_log_info,

	TP_PROTO(const char *name, struct va_format *vaf),

	TP_ARGS(name, vaf),

	TP_STRUCT__entry(
		__string(name, name)
		__dynamic_array(char, msg, MAX_MSG_LEN)
	),

	TP_fast_assign(
		__assign_str(name, name);
		WARN_ON_ONCE(vsnprintf(__get_dynamic_array(msg),
				       MAX_MSG_LEN, vaf->fmt,
				       *vaf->va) >= MAX_MSG_LEN);
	),

	TP_printk("%s: %s", __get_str(name), __get_str(msg))
);

DECLARE_EVENT_CLASS(buses_info,

	TP_PROTO(struct device *dev, const char *string1, char *string2),

	TP_ARGS(dev, string1, string2),

	TP_STRUCT__entry(
		__string(name, dev_name(dev))
		__string(string1, string1)
		__string(string2, string2)
	),

	TP_fast_assign(
		__assign_str(name, dev_name(dev));
		__assign_str(string1, string1);
		__assign_str(string2, string2);
	),

	TP_printk("%s: %s: %s", __get_str(name), __get_str(string1), __get_str(string2))
);

DECLARE_EVENT_CLASS(serial_transmit_data,

	TP_PROTO(struct device *dev, char *string, int size),

	TP_ARGS(dev, string, size),

	TP_STRUCT__entry(
		__string(name, dev_name(dev))
		__array(char, buf, 64)
		__field(unsigned int, size)
		__field(int, len)
	),

	TP_fast_assign(
		__assign_str(name, dev_name(dev));
		__entry->len = min(32, size);
		hex_dump_to_buffer(string, __entry->len, 32, 1, __entry->buf,
				   sizeof(__entry->buf), false);
	),

	TP_printk("%s: %s\n", __get_str(name),  __entry->buf)
);


DEFINE_EVENT(buses_info, serial_info,

	TP_PROTO(struct device *dev, const char *string1, char *string2),

	TP_ARGS(dev, string1, string2)
);

DEFINE_EVENT(serial_transmit_data, serial_transmit_data_tx,

	TP_PROTO(struct device *dev, char *string, int size),

	TP_ARGS(dev, string, size)
);

DEFINE_EVENT(serial_transmit_data, serial_transmit_data_rx,

	TP_PROTO(struct device *dev, char *string, int size),

	TP_ARGS(dev, string, size)
);

#endif /* _TRACE_QUP_BUSES_TRACE_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
