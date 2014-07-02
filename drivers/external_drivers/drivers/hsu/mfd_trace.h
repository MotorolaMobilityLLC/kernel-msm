#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE mfd_trace

#define TRACE_SYSTEM hsu

#if !defined(_TRACE_HSU_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HSU_H

#include <linux/tracepoint.h>

#define hsucmd_name(cmd) { cmd, #cmd }
#define show_hsucmd_name(val)			\
	__print_symbolic(val,			\
		hsucmd_name(qcmd_overflow),	\
		hsucmd_name(qcmd_get_msr),	\
		hsucmd_name(qcmd_set_mcr),	\
		hsucmd_name(qcmd_set_ier),	\
		hsucmd_name(qcmd_start_rx),	\
		hsucmd_name(qcmd_stop_rx),	\
		hsucmd_name(qcmd_start_tx),	\
		hsucmd_name(qcmd_stop_tx),	\
		hsucmd_name(qcmd_cl),		\
		hsucmd_name(qcmd_port_irq),	\
		hsucmd_name(qcmd_dma_irq),	\
		hsucmd_name(qcmd_enable_irq),   \
		hsucmd_name(qcmd_cmd_off))


TRACE_EVENT(hsu_cmd_insert,

	TP_PROTO(unsigned port, char cmd),

	TP_ARGS(port, cmd),

	TP_STRUCT__entry(
		__field(unsigned, port)
		__field(char, cmd)
	),

	TP_fast_assign(
		__entry->port = port;
		__entry->cmd = cmd;
	),

	TP_printk("port=%u cmd=%s", __entry->port,
		show_hsucmd_name(__entry->cmd))
);

TRACE_EVENT(hsu_cmd_add,

	TP_PROTO(unsigned port, char cmd),

	TP_ARGS(port, cmd),

	TP_STRUCT__entry(
		__field(unsigned, port)
		__field(char, cmd)
	),

	TP_fast_assign(
		__entry->port = port;
		__entry->cmd = cmd;
	),

	TP_printk("port=%u cmd=%s", __entry->port,
		show_hsucmd_name(__entry->cmd))
);

TRACE_EVENT(hsu_cmd_start,

	TP_PROTO(unsigned port, char cmd),

	TP_ARGS(port, cmd),

	TP_STRUCT__entry(
		__field(unsigned, port)
		__field(char, cmd)
	),

	TP_fast_assign(
		__entry->port = port;
		__entry->cmd = cmd;
	),

	TP_printk("port=%u cmd=%s", __entry->port,
		show_hsucmd_name(__entry->cmd))
);

TRACE_EVENT(hsu_cmd_end,

	TP_PROTO(unsigned port, char cmd),

	TP_ARGS(port, cmd),

	TP_STRUCT__entry(
		__field(unsigned, port)
		__field(char, cmd)
	),

	TP_fast_assign(
		__entry->port = port;
		__entry->cmd = cmd;
	),

	TP_printk("port=%u cmd=%s", __entry->port,
		show_hsucmd_name(__entry->cmd))
);

TRACE_EVENT(hsu_func_start,

	TP_PROTO(unsigned port, const char *func),

	TP_ARGS(port, func),

	TP_STRUCT__entry(
		__field(unsigned, port)
		__string(name, func)
	),

	TP_fast_assign(
		__entry->port = port;
		__assign_str(name, func);
	),

	TP_printk("port=%u func=%s", __entry->port,
		__get_str(name))
);

TRACE_EVENT(hsu_func_end,

	TP_PROTO(unsigned port, const char *func, char *err),

	TP_ARGS(port, func, err),

	TP_STRUCT__entry(
		__field(unsigned, port)
		__string(name, func)
		__string(ret, err)
	),

	TP_fast_assign(
		__entry->port = port;
		__assign_str(name, func);
		__assign_str(ret, err);
	),

	TP_printk("port=%u func=%s err=%s", __entry->port,
		__get_str(name), __get_str(ret))
);

TRACE_EVENT(hsu_mctrl,

	TP_PROTO(unsigned port, unsigned mctrl),

	TP_ARGS(port, mctrl),

	TP_STRUCT__entry(
		__field(unsigned, port)
		__field(unsigned, mctrl)
	),

	TP_fast_assign(
		__entry->port = port;
		__entry->mctrl = mctrl;
	),

	TP_printk("port=%u mctrl=%d", __entry->port, __entry->mctrl)
);

TRACE_EVENT(hsu_set_termios,

	TP_PROTO(unsigned port, unsigned int baud, int ctsrts),

	TP_ARGS(port, baud, ctsrts),

	TP_STRUCT__entry(
		__field(unsigned, port)
		__field(unsigned int, baud)
		__field(int, ctsrts)
	),

	TP_fast_assign(
		__entry->port = port;
		__entry->baud = baud;
		__entry->ctsrts = ctsrts;
	),

	TP_printk("port=%u baud=%d ctsrts=%d", __entry->port,
		__entry->baud, __entry->ctsrts)
);

#endif /* if !defined(_TRACE_HSU_H) || defined(TRACE_HEADER_MULTI_READ) */

/* This part must be outside protection */
#include <trace/define_trace.h>
