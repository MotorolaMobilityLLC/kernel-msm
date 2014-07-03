/******************************************************************************
 *
 * Copyright (c) 2011, Intel Corporation.
 * Portions (c), Imagination Technology, Ltd.
 * All rights reserved.
 *
 * Redistribution and Use.  Redistribution and use in binary form, without
 * modification, of the software code provided with this license ("Software"),
 * are permitted provided that the following conditions are met:
 *
 *  1. Redistributions must reproduce the above copyright notice and this
 *     license in the documentation and/or other materials provided with the
 *     Software.
 *  2. Neither the name of Intel Corporation nor the name of Imagination
 *     Technology, Ltd may be used to endorse or promote products derived from
 *     the Software without specific prior written permission.
 *  3. The Software can only be used in connection with the Intel hardware
 *     designed to use the Software as outlined in the documentation. No other
 *     use is authorized.
 *  4. No reverse engineering, decompilation, or disassembly of the Software
 *     is permitted.
 *  5. The Software may not be distributed under terms different than this
 *     license.
 *
 * Limited Patent License.  Intel Corporation grants a world-wide, royalty-free
 * , non-exclusive license under patents it now or hereafter owns or controls
 * to make, have made, use, import, offer to sell and sell ("Utilize") the
 * Software, but solely to the extent that any such patent is necessary to
 * Utilize the Software alone.  The patent license shall not apply to any
 * combinations which include the Software.  No hardware per se is licensed
 * hereunder.
 *
 * Ownership of Software and Copyrights. Title to all copies of the Software
 * remains with the copyright holders. The Software is copyrighted and
 * protected by the laws of the United States and other countries, and
 * international treaty provisions.
 *
 * DISCLAIMER.  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
******************************************************************************/

#ifndef _DISPLAY_MANAGER_NETLINK_H_
#define _DISPLAY_MANAGER_NETLINK_H_

#include <drm/drmP.h>
#include "mdfld_dsi_output.h"

#define SUCCESS		1
#define FAILED		0

enum dispmgr_module_enum {
	DISPMGR_MOD_UNKNOWN,
	DISPMGR_MOD_NETLINK,
	DISPMGR_MOD_DPST,
};

enum dispmgr_event_enum {
	DISPMGR_UNKNOWN,
	DISPMGR_TEST,
	DISPMGR_TEST_TEXT,
};

enum dispmgr_dpst_event_enum {
	DISPMGR_DPST_UNKNOWN,
	DISPMGR_DPST_INIT_COMM,
	DISPMGR_DPST_UPDATE_GUARD,
	DISPMGR_DPST_HIST_ENABLE,
	DISPMGR_DPST_HIST_DATA,
	DISPMGR_DPST_BL_CMD,
	DISPMGR_DPST_GAMMA_SET_CMD,
	DISPMGR_DPST_DIET_ENABLE,
	DISPMGR_DPST_DIET_DISABLE,
	DISPMGR_DPST_GET_MODE,
};

/* Display Manager Command Header */
struct dispmgr_command_hdr {
	unsigned int module;	/* module to receive the command */
	unsigned int cmd;	/* command from Userspace */
	unsigned int data_size;	/* data size of command_data in number of bytes */
	uint64_t  data  __attribute__ ((__packed__));             /* command data */
};

void dispmgr_start(struct drm_device *dev);
void dispmgr_nl_send_msg(struct dispmgr_command_hdr *cmd_hdr);
void dpstmgr_reg_restore_locked(struct drm_device *dev, struct mdfld_dsi_config *dsi_config);

#endif				/* _DISPLAY_MANAGER_NETLINK_H_ */
