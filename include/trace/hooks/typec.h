/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM typec
#define TRACE_INCLUDE_PATH trace/hooks
#if !defined(_TRACE_HOOK_TYPEC_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_TYPEC_H
#include <trace/hooks/vendor_hooks.h>
struct tcpm_port;

DECLARE_HOOK(android_vh_typec_store_partner_src_caps,
	     TP_PROTO(unsigned int *nr_source_caps, u32 (*source_caps)[]),
	     TP_ARGS(nr_source_caps, source_caps));

DECLARE_HOOK(android_vh_typec_tcpm_modify_src_caps,
	     TP_PROTO(unsigned int *nr_src_pdo, u32 (*src_pdo)[], bool *modified),
	     TP_ARGS(nr_src_pdo, src_pdo, modified));

DECLARE_HOOK(android_vh_typec_tcpm_log,
	     TP_PROTO(const char *log, bool *bypass),
	     TP_ARGS(log, bypass));

#endif /* _TRACE_HOOK_TYPEC_H */
/* This part must be outside protection */
#include <trace/define_trace.h>
