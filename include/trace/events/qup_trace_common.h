/* SPDX-License-Identifier: GPL-2.0
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#ifndef _TRACE_QUP_COMMON_H
#define _TRACE_QUP_COMMON_H

#include <linux/ktime.h>
#include <linux/tracepoint.h>

#define MAX_MSG_LEN 256

#define DEFINE_QUP_LOG_EVENT(event_name) \
TRACE_EVENT(event_name, \
	TP_PROTO(const char *name, struct va_format *vaf), \
	TP_ARGS(name, vaf), \
	TP_STRUCT__entry( \
		__string(name, name) \
		__dynamic_array(char, msg, MAX_MSG_LEN) \
	), \
	TP_fast_assign( \
		__assign_str_len(name, name, strlen(name)); \
		WARN_ON_ONCE(vsnprintf(__get_dynamic_array(msg), \
			     MAX_MSG_LEN, vaf->fmt, \
			     *vaf->va) >= MAX_MSG_LEN); \
	), \
	TP_printk("%s: %s", __get_str(name), __get_str(msg)) \
)

#define DECLARE_QUP_INFO_EVENT_CLASS(class_name) \
DECLARE_EVENT_CLASS(class_name, \
	TP_PROTO(struct device *dev, const char *string1, char *string2), \
	TP_ARGS(dev, string1, string2), \
	TP_STRUCT__entry( \
		__string(name, dev_name(dev)) \
		__string(string1, string1) \
		__string(string2, string2) \
	), \
	TP_fast_assign( \
		__assign_str_len(name, dev_name(dev), strlen(dev_name(dev))); \
		__assign_str_len(string1, string1, strlen(string1)); \
		__assign_str_len(string2, string2, strlen(string2)); \
	), \
	TP_printk("%s: %s: %s", __get_str(name), __get_str(string1), __get_str(string2)) \
)

#define DECLARE_QUP_DATA_EVENT_CLASS(class_name) \
DECLARE_EVENT_CLASS(class_name, \
	TP_PROTO(struct device *dev, char *string, int size), \
	TP_ARGS(dev, string, size), \
	TP_STRUCT__entry( \
		__string(name, dev_name(dev)) \
		__array(char, buf, 64) \
		__field(unsigned int, size) \
		__field(int, len) \
	), \
	TP_fast_assign( \
		__assign_str_len(name, dev_name(dev), strlen(dev_name(dev))); \
		__entry->len = min(32, size); \
		hex_dump_to_buffer(string, __entry->len, 32, 1, __entry->buf, \
				   sizeof(__entry->buf), false); \
	), \
	TP_printk("%s: %s", __get_str(name), __entry->buf) \
)

#endif /* _TRACE_QUP_COMMON_H */
