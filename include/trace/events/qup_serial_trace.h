/* SPDX-License-Identifier: GPL-2.0
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM qup_serial_trace

#if !defined(_TRACE_QUP_SERIAL_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_QUP_SERIAL_TRACE_H

#include "qup_trace_common.h"

DEFINE_QUP_LOG_EVENT(serial_log_info);

DECLARE_QUP_INFO_EVENT_CLASS(serial_info_event);
DEFINE_EVENT(serial_info_event, serial_info,
	     TP_PROTO(struct device *dev, const char *string1, char *string2),
	     TP_ARGS(dev, string1, string2)
);

DECLARE_QUP_DATA_EVENT_CLASS(serial_transmit_data);
DEFINE_EVENT(serial_transmit_data, serial_transmit_data_tx,
	     TP_PROTO(struct device *dev, char *string, int size),
	     TP_ARGS(dev, string, size)
);
DEFINE_EVENT(serial_transmit_data, serial_transmit_data_rx,
	     TP_PROTO(struct device *dev, char *string, int size),
	     TP_ARGS(dev, string, size)
);

#endif /* _TRACE_QUP_SERIAL_TRACE_H */
#include <trace/define_trace.h>
