#undef TRACE_SYSTEM
#define TRACE_SYSTEM tp2e


#if !defined(_TRACE_TP2E_H)
enum tp2e_ev_type {
	TP2E_EV_STAT,
	TP2E_EV_INFO,
	TP2E_EV_ERROR,
	TP2E_EV_CRASH,
	TP2E_EV_LAST
};

#endif /* _TRACE_TP2E_H */


#if !defined(_TRACE_TP2E_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_TP2E_H

#include <linux/tracepoint.h>

#define NAME_MAX_LEN 16
#define DATA_MAX_LEN 128
#define FILELIST_MAX_LEN 256

#define show_tp2e_ev_type(type)			\
	__print_symbolic(type,				\
			 { TP2E_EV_STAT, "STAT" },	\
			 { TP2E_EV_INFO, "INFO" },	\
			 { TP2E_EV_ERROR, "ERROR" },	\
			 { TP2E_EV_CRASH, "CRASH" })

DECLARE_EVENT_CLASS(tp2e_generic_class,

	    TP_PROTO(
		    enum tp2e_ev_type ev_type,
		    char *submitter_name,
		    char *ev_name,
		    char *data0,
		    char *data1,
		    char *data2,
		    char *data3,
		    char *data4,
		    char *data5,
		    char *filelist
		    ),

	    TP_ARGS(
		    ev_type, submitter_name, ev_name,
		    data0, data1, data2, data3, data4, data5, filelist
		    ),

	    TP_STRUCT__entry(
		    __field(enum tp2e_ev_type, ev_type)
		    __array(char, submitter_name, NAME_MAX_LEN)
		    __array(char, ev_name, NAME_MAX_LEN)
		    __array(char, data0, DATA_MAX_LEN)
		    __array(char, data1, DATA_MAX_LEN)
		    __array(char, data2, DATA_MAX_LEN)
		    __array(char, data3, DATA_MAX_LEN)
		    __array(char, data4, DATA_MAX_LEN)
		    __array(char, data5, DATA_MAX_LEN)
		    __array(char, filelist, FILELIST_MAX_LEN)
		    ),

	    TP_fast_assign(
		    __entry->ev_type = ev_type;
		    strncpy(__entry->submitter_name, submitter_name, NAME_MAX_LEN);
		    strncpy(__entry->ev_name, ev_name, NAME_MAX_LEN);
		    strncpy(__entry->data0, data0, DATA_MAX_LEN);
		    strncpy(__entry->data1, data1, DATA_MAX_LEN);
		    strncpy(__entry->data2, data2, DATA_MAX_LEN);
		    strncpy(__entry->data3, data3, DATA_MAX_LEN);
		    strncpy(__entry->data4, data4, DATA_MAX_LEN);
		    strncpy(__entry->data5, data5, DATA_MAX_LEN);
		    strncpy(__entry->filelist, filelist, FILELIST_MAX_LEN);
		    ),

	    TP_printk("type=%s submitter_name=%s name=%s data0=%s data1=%s data2=%s data3=%s data4=%s data5=%s",
		      show_tp2e_ev_type(__entry->ev_type),
		      __entry->submitter_name, __entry->ev_name,
		      __entry->data0, __entry->data1, __entry->data2,
		      __entry->data3, __entry->data4, __entry->data5
		    )
	);

DEFINE_EVENT(tp2e_generic_class, tp2e_generic_event,
	TP_PROTO(
		enum tp2e_ev_type ev_type,
		char *submitter_name,
		char *ev_name,
		char *data0,
		char *data1,
		char *data2,
		char *data3,
		char *data4,
		char *data5,
		char *filelist
		),

	TP_ARGS(
		ev_type, submitter_name, ev_name,
		data0, data1, data2, data3, data4, data5, filelist
		)
	);

DEFINE_EVENT(tp2e_generic_class, tp2e_scu_recov_event,
	TP_PROTO(
		enum tp2e_ev_type ev_type,
		char *submitter_name,
		char *ev_name,
		char *data0,
		char *data1,
		char *data2,
		char *data3,
		char *data4,
		char *data5,
		char *filelist
		),

	TP_ARGS(
		ev_type, submitter_name, ev_name,
		data0, data1, data2, data3, data4, data5, filelist
		)
	);

#endif /* _TRACE_TP2E_H || TRACE_HEADER_MULTI_READ */

/* This part must be outside protection */
#include <trace/define_trace.h>
