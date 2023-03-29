/*
 * Copyright (c) 2014-2020 The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifdef HDD_TRACE_RECORD

/**
 * DOC: wlan_hdd_trace.c
 *
 * WLAN Host Device Driver trace implementation
 *
 */

#include "qdf_trace.h"
#include "qdf_types.h"
#include "wlan_hdd_trace.h"
#include "wlan_hdd_main.h"

/**
 * hdd_trace_dump() - Dump an HDD-specific trace record
 * @mac: (unused) global MAC handle
 * @record: trace record that was previously recorded
 * @index: index of the trace record
 *
 * Return: none
 */
static void
hdd_trace_dump(void *mac, tp_qdf_trace_record record, uint16_t index)
{
	if (TRACE_CODE_HDD_RX_SME_MSG == record->code)
		hdd_nofl_debug("%04d %012llu %s S%d %-14s %-30s(0x%x)",
			       index, record->qtime, record->time,
			       record->session, "RX SME MSG:",
			       get_e_roam_cmd_status_str(record->data),
							 record->data);
	else
		hdd_nofl_debug("%04d %012llu %s S%d %-14s %-30s(0x%x)",
			       index, record->qtime, record->time,
			       record->session, "HDD Event:",
			       hdd_trace_event_string(record->code),
						      record->data);
}

/**
 * hdd_trace_init() - HDD trace subsystem initialization
 *
 * Registers HDD with the debug trace subsystem
 *
 * Return: none
 */
void hdd_trace_init(void)
{
	qdf_trace_register(QDF_MODULE_ID_HDD, hdd_trace_dump);
}

#endif /* ifdef HDD_TRACE_RECORD */
