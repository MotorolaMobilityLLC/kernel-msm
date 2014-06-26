/*******************************************************************
* (c) Copyright 2011-2012 Discretix Technologies Ltd.              *
* This software is protected by copyright, international           *
* treaties and patents, and distributed under multiple licenses.   *
* Any use of this Software as part of the Discretix CryptoCell or  *
* Packet Engine products requires a commercial license.            *
* Copies of this Software that are distributed with the Discretix  *
* CryptoCell or Packet Engine product drivers, may be used in      *
* accordance with a commercial license, or at the user's option,   *
* used and redistributed under the terms and conditions of the GNU *
* General Public License ("GPL") version 2, as published by the    *
* Free Software Foundation.                                        *
* This program is distributed in the hope that it will be useful,  *
* but WITHOUT ANY LIABILITY AND WARRANTY; without even the implied *
* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. *
* See the GNU General Public License version 2 for more details.   *
* You should have received a copy of the GNU General Public        *
* License version 2 along with this Software; if not, please write *
* to the Free Software Foundation, Inc., 59 Temple Place - Suite   *
* 330, Boston, MA 02111-1307, USA.                                 *
* Any copy or reproduction of this Software, as permitted under    *
* the GNU General Public License version 2, must include this      *
* Copyright Notice as well as any other notices provided under     *
* the said license.                                                *
********************************************************************/
/** SeP Applets support module */
#ifndef _SEPAPP_H_
#define _SEPAPP_H_

#include "dx_driver_abi.h"
#include "dx_driver.h"

/* Global drvdata to be used by kernel clients via dx_sepapp_ API */
int sep_ioctl_sepapp_session_open(struct sep_client_ctx *client_ctx,
				  unsigned long arg);

int sep_ioctl_sepapp_session_close(struct sep_client_ctx *client_ctx,
				   unsigned long arg);

int sep_ioctl_sepapp_command_invoke(struct sep_client_ctx *client_ctx,
				    unsigned long arg);

void dx_sepapp_init(struct sep_drvdata *drvdata);

int sepapp_session_close(struct sep_op_ctx *op_ctx, int session_id);

int sepapp_image_verify(u8 *addr, ssize_t size, u32 key_index, u32 magic_num);

int sepapp_hdmi_status(u8 status, u8 bksv[5]);
#endif /*_SEPAPP_H_*/
