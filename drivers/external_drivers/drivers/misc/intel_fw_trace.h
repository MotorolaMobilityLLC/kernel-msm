/*
 * drivers/misc/intel_fw_trace.h
 *
 * Copyright (C) 2013 Intel Corp
 * Author: jouni.hogander@intel.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 */

#ifndef __INTEL_FW_TRACE_H
#define __INTEL_FW_TRACE_H

struct scu_trace_hdr_t {
	u32 magic;
	u8 majorrev;
	u8 minorrev;
	u16 cmd;
	u32 offset;
	u32 size;
};

struct ia_trace_t {
	u32 apm_cmd[2];
	u32 ospm_pm_ssc[2];
};

#define TRACE_MAGIC 0x53435554

#define TRACE_ID_INFO    0x0100
#define TRACE_ID_ERROR   0x0200
#define TRACE_ID_MASK    (0x3 << 8)

#define TRACE_IS_ASCII   0x0001

void apic_scu_panic_dump(void);
#endif /* __INTEL_FABRICID_DEF_H */
