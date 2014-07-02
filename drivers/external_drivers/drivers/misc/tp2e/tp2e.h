#ifndef _TP2E_H_
#define _TP2E_H_

#include <linux/kct.h>


#define DEFINE_PROBE(event, probe)

#endif /* _TP2E_H_ */

#ifdef DECLARE_TP2E_ELT
#undef DEFINE_PROBE
#define DEFINE_PROBE(event, probe)				\
	static struct tp2e_element tp2e_##event = {			\
		.system = __stringify(TRACE_SYSTEM),			\
		.name = __stringify(event),				\
		.probe_fn = (void *)probe,				\
	};
#endif /* DECLARE_TP2E_ELT */

#ifdef ADD_TP2E_ELT
#undef DEFINE_PROBE
#define DEFINE_PROBE(event, probe)				\
	do {								\
		INIT_LIST_HEAD(&tp2e_##event.list);			\
		list_add_tail(&tp2e_##event.list, &tp2e_list);		\
	} while (0)
#endif /* ADD_TP2E_ELT */

