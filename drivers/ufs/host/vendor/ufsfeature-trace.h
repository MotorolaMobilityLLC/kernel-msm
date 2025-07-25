/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2024 Samsung Electronics Co., Ltd.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM ufsfeature

#if !defined(_TRACE_UFSFEATURE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_UFSFEATURE_H

#include <linux/tracepoint.h>

#define str_opcode(opcode)						\
	__print_symbolic(opcode,					\
		{ WRITE_16,		"WRITE_16" },			\
		{ WRITE_10,		"WRITE_10" },			\
		{ READ_16,		"READ_16" },			\
		{ READ_10,		"READ_10" },			\
		{ SYNCHRONIZE_CACHE,	"SYNC" },			\
		{ UNMAP,		"UNMAP" })

#define UFS_CMD_TRACE_STRINGS					\
	EM(UFS_CMD_SEND,	"send_req")			\
	EM(UFS_CMD_COMP,	"complete_rsp")			\
	EM(UFS_DEV_COMP,	"dev_complete")			\
	EM(UFS_QUERY_SEND,	"query_send")			\
	EM(UFS_QUERY_COMP,	"query_complete")		\
	EM(UFS_QUERY_ERR,	"query_complete_err")		\
	EM(UFS_TM_SEND,		"tm_send")			\
	EM(UFS_TM_COMP,		"tm_complete")			\
	EMe(UFS_TM_ERR,		"tm_complete_err")

#define UFS_CMD_TRACE_TSF_TYPES					\
	EM(UFS_TSF_CDB,		"CDB")		                \
	EM(UFS_TSF_OSF,		"OSF")		                \
	EM(UFS_TSF_TM_INPUT,	"TM_INPUT")                     \
	EMe(UFS_TSF_TM_OUTPUT,	"TM_OUTPUT")

/* Enums require being exported to userspace, for user tool parsing */
#undef EM
#undef EMe
#define EM(a, b)	TRACE_DEFINE_ENUM(a);
#define EMe(a, b)	TRACE_DEFINE_ENUM(a);

UFS_CMD_TRACE_STRINGS
UFS_CMD_TRACE_TSF_TYPES

/*
 * Now redefine the EM() and EMe() macros to map the enums to the strings
 * that will be printed in the output.
 */
#undef EM
#undef EMe
#define EM(a, b)	{a, b},
#define EMe(a, b)	{a, b}

#define show_ufs_cmd_trace_str(str_t)	\
				__print_symbolic(str_t, UFS_CMD_TRACE_STRINGS)
#define show_ufs_cmd_trace_tsf(tsf)	\
				__print_symbolic(tsf, UFS_CMD_TRACE_TSF_TYPES)

TRACE_EVENT(ufsfeature_command,
	TP_PROTO(struct scsi_device *sdev, struct ufs_hba *hba,
		 enum ufs_trace_str_t str_t,
		 unsigned int tag, u32 doorbell, u32 hwq_id, int transfer_len,
		 u32 intr, u64 lba, u8 opcode, u8 group_id),

	TP_ARGS(sdev, hba, str_t, tag, doorbell, hwq_id, transfer_len, intr, lba,
		opcode, group_id),

	TP_STRUCT__entry(
		__field(struct scsi_device *, sdev)
		__field(struct ufs_hba *, hba)
		__field(enum ufs_trace_str_t, str_t)
		__field(unsigned int, tag)
		__field(u32, doorbell)
		__field(u32, hwq_id)
		__field(u32, intr)
		__field(u64, lba)
		__field(int, transfer_len)
		__field(u8, opcode)
		__field(u8, group_id)
	),

	TP_fast_assign(
		__entry->sdev = sdev;
		__entry->hba = hba;
		__entry->str_t = str_t;
		__entry->tag = tag;
		__entry->doorbell = doorbell;
		__entry->hwq_id = hwq_id;
		__entry->intr = intr;
		__entry->lba = lba;
		__entry->transfer_len = transfer_len;
		__entry->opcode = opcode;
		__entry->group_id = group_id;
	),

	TP_printk(
		"%s: %s: tag: %u, DB: 0x%x, size: %d, IS: %u, LBA: %llu, opcode: 0x%x (%s), group_id: 0x%x, hwq_id: %d",
		show_ufs_cmd_trace_str(__entry->str_t),
		dev_name(&__entry->sdev->sdev_dev), __entry->tag,
		__entry->doorbell, __entry->transfer_len, __entry->intr,
		__entry->lba, (u32)__entry->opcode, str_opcode(__entry->opcode),
		(u32)__entry->group_id, __entry->hwq_id
	)
);

TRACE_EVENT(ufsfeature_upiu,
	TP_PROTO(struct ufs_hba *hba, enum ufs_trace_str_t str_t, void *hdr,
		 void *tsf, enum ufs_trace_tsf_t tsf_t),

	TP_ARGS(hba, str_t, hdr, tsf, tsf_t),

	TP_STRUCT__entry(
		__field(struct ufs_hba *, hba)
		__field(enum ufs_trace_str_t, str_t)
		__array(unsigned char, hdr, 12)
		__array(unsigned char, tsf, 16)
		__field(enum ufs_trace_tsf_t, tsf_t)
	),

	TP_fast_assign(
		__entry->hba = hba;
		__entry->str_t = str_t;
		memcpy(__entry->hdr, hdr, sizeof(__entry->hdr));
		memcpy(__entry->tsf, tsf, sizeof(__entry->tsf));
		__entry->tsf_t = tsf_t;
	),

	TP_printk(
		"%s: %s: HDR:%s, %s:%s",
		show_ufs_cmd_trace_str(__entry->str_t), dev_name(__entry->hba->dev),
		__print_hex(__entry->hdr, sizeof(__entry->hdr)),
		show_ufs_cmd_trace_tsf(__entry->tsf_t),
		__print_hex(__entry->tsf, sizeof(__entry->tsf))
	)
);

#endif /* if !defined(_TRACE_UFSFEATURE_H) || defined(TRACE_HEADER_MULTI_READ) */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ../../../drivers/ufs/host/vendor
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE ufsfeature-trace

/* This part must be outside protection */
#include <trace/define_trace.h>
