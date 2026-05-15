/* SPDX-License-Identifier: GPL-2.0
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM qup_spi_trace

#if !defined(_TRACE_QUP_SPI_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_QUP_SPI_TRACE_H

#include "qup_trace_common.h"

DEFINE_QUP_LOG_EVENT(spi_log_info);

DECLARE_QUP_INFO_EVENT_CLASS(spi_info_event);
DEFINE_EVENT(spi_info_event, spi_info,
	     TP_PROTO(struct device *dev, const char *string1, char *string2),
	     TP_ARGS(dev, string1, string2)
);

#endif /* _TRACE_QUP_SPI_TRACE_H */
#include <trace/define_trace.h>
