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

#ifndef _SEP_REQUEST_KERNEL_API_H_
#define _SEP_REQUEST_KERNEL_API_H_

#include <linux/types.h>
#include "dx_driver.h"

/**
 * dx_sep_req_handler() - SeP request interrupt handler
 * @drvdata: The driver private info
 */
void dx_sep_req_handler(struct sep_drvdata *drvdata);

/**
 * dx_sep_req_get_sep_init_params() - Setup sep init params
 * @sep_request_params: The sep init parameters array
 */
void dx_sep_req_get_sep_init_params(u32 *sep_request_params);

/**
 * dx_sep_req_enable() - Enable the sep request interrupt handling
 * @drvdata: Driver private data
 */
void dx_sep_req_enable(struct sep_drvdata *drvdata);

/**
 * dx_sep_req_init() - Initialize the sep request state
 * @drvdata: Driver private data
 */
int dx_sep_req_init(struct sep_drvdata *drvdata);

/**
 * dx_sep_req_fini() - Finalize the sep request state
 * @drvdata: Driver private data
 */
void dx_sep_req_fini(struct sep_drvdata *drvdata);

#endif /*_SEP_REQUEST_KERNEL_API_H_*/
