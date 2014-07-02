/*
 * drivers/misc/intel_fabricid_def.h
 *
 * Copyright (C) 2011 Intel Corp
 * Author: winson.w.yung@intel.com
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

#ifndef __INTEL_FABRICID_DEF_H
#define __INTEL_FABRICID_DEF_H

#define FAB_ID_FULLCHIP					0
#define FAB_ID_AUDIO					1
#define FAB_ID_SECONDARY				2
#define FAB_ID_GP					3
#define FAB_ID_SC					4
#define FAB_ID_SC1					5
#define FAB_ID_UNKNOWN					6
#define FAB_ID_PM_FULLCHIP				7
#define FAB_ID_PM_AUDIO					8
#define FAB_ID_PM_SECONDARY				9
#define FAB_ID_PM_GP					10
#define FAB_ID_PM_SC					11

enum scu_cold_boot_err {
	ERR_MEM_ERR		= 0xE101,
	ERR_INST_ERR,
	ERR_FABRIC_ERR,
	ERR_SRAM_ECC_ERR	= 0xE104,
	ERR_NORTH_FUSES_ERR	= 0xE106,
	ERR_KERNEL_HANG_ERR	= 0xE10A,
	ERR_KERNEL_WDT_ERR,
	ERR_SCUWDT_CHAABIWDT,
	ERR_FABRIC_XML_ERR
};

enum scu_runtime_err {
	ERR_PLL_LOCKSLIP_ERR	= 0xE601,
	ERR_UNDEFINED_L1_ERR	= 0xE603,
	ERR_PUINT_INT_MBB_TMOUT_ERR,
	ERR_FUSE_VOLTATK_ERR,
	ERR_FUSE_VOLT_SAIATK_ERR,
	ERR_LPE_INT_ERR,
	ERR_PSH_INT_ERR,
	ERR_FUSE_INT_ERR,
	ERR_UNSUPPORTED_IPC2_ERR,
	ERR_KWDT_IPC_ERR
};

enum scu_recoverable_fab_err {
	ERR_UNSUPPORTED_CMD_ERR	= 0xF00A,
	ERR_ADDR_HOLE_ERR,
	ERR_PROTECTION_ERR,
	ERR_MEM_ASSERTION_ERR
};

enum scu_fatal_fab_err {
	ERR_REQ_TIMEOUT_NOT_ACCEPTED	= 0xF00E,
	ERR_REQ_TIMEOUT_NO_RESPONSE,
	ERR_REQ_TIMEOUT_DATA_NOT_ACCEPTED
};

char *fabric_error_lookup(u32 fab_id, u32 error_index, int use_hidword);
char *get_errortype_str(u16 error_type);
int errorlog_element_type(u8 id_type);
char *get_element_errorlog_detail(u8 id, u32 *fabric_type);
char *get_element_flagsts_detail(u8 id);
char *get_initiator_id_str(int init_id, u32 fabric_id);

#endif /* __INTEL_FABRICID_DEF_H */
