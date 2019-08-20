/*
 * Copyright (C) 2019 Motorola Mobility Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM mmi_sigprint

#if !defined(_TRACE_MMI_SIGPRINT_EVENTS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MMI_SIGPRINT_EVENTS_H

#include <linux/tracepoint.h>
/*
DECLARE_TRACE(mmi_sigprint_signal_generate,
	TP_PROTO(int firstarg, struct task_struct *p),
	TP_ARGS(firstarg, p));

DECLARE_TRACE(mmi_sigprintsignal_deliver,
	TP_PROTO(int firstarg, struct task_struct *p),
	TP_ARGS(firstarg, p));
*/

#endif /* _TRACE_MMI_SIGPRINT_EVENTS_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
