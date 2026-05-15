/* SPDX-License-Identifier: GPL-2.0
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM qup_i2c_trace

#include <linux/tracepoint.h>

#if !defined(_TRACE_QUP_I2C_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_QUP_I2C_TRACE_H

#include "qup_trace_common.h"

DEFINE_QUP_LOG_EVENT(i2c_log_info);

#endif /* _TRACE_QUP_I2C_TRACE_H */
#include <trace/define_trace.h>
