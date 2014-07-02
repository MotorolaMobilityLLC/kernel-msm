#include "tp2e.h"

#ifndef _TP2E_PROBE_GENERIC_EVENT_
#define _TP2E_PROBE_GENERIC_EVENT_

/* Below are the names of the system and event as they
 * are defined in include/trace/events/tp2e.h
 */
#define TRACE_SYSTEM tp2e
#define TP2E_GENERIC_EVENT_NAME tp2e_generic_event
#define TP2E_SCU_RECOV_EVENT_NAME tp2e_scu_recov_event

static void tp2e_probe_generic_event(void *cb_data,
				     enum tp2e_ev_type tp2e_ev_type,
				     char *submitter_name, char *ev_name,
				     char *data0, char *data1, char *data2,
				     char *data3, char *data4, char *data5,
				     char *filelist)
{
	enum ct_ev_type ev_type;

	switch (tp2e_ev_type) {
	case TP2E_EV_STAT:
		ev_type = CT_EV_STAT;
		break;
	case TP2E_EV_INFO:
		ev_type = CT_EV_INFO;
		break;
	case TP2E_EV_ERROR:
		ev_type = CT_EV_ERROR;
		break;
	case TP2E_EV_CRASH:
		ev_type = CT_EV_CRASH;
		break;
	default:
		ev_type = CT_EV_INFO;
		break;
	}
	kct_log(ev_type, submitter_name, ev_name, 0, data0, data1, data2,
		data3, data4, data5, filelist);
}

#endif /* _TP2E_PROBE_GENERIC_EVENT_H_ */

DEFINE_PROBE(TP2E_GENERIC_EVENT_NAME, tp2e_probe_generic_event);

DEFINE_PROBE(TP2E_SCU_RECOV_EVENT_NAME, tp2e_probe_generic_event);
