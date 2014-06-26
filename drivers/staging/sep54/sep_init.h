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

#ifndef __SEP_INIT_H__
#define __SEP_INIT_H__

#include "dx_driver.h"

/**
 * sepinit_do_cc_init() - Initiate SeP cold boot sequence and wait for
 *	its completion.
 *
 * @drvdata:
 *
 * This function loads the CC firmware and dispatches an CC_INIT request message
 * Returns int 0 for success
 */
int sepinit_do_cc_init(struct sep_drvdata *drvdata);

/**
 * sepinit_get_fw_props() - Get the FW properties (version, cache size, etc.)
 *	after completing cold boot
 * @drvdata:	 Context where to fill retrieved data
 *
 * This function should be called only after sepinit_do_cc_init completes
 * successfully.
 */
void sepinit_get_fw_props(struct sep_drvdata *drvdata);

/**
 * sepinit_do_fw_init() - Initialize SeP FW
 * @drvdata:
 *
 * Provide SeP FW with initialization parameters and wait for DONE_FW_INIT.
 *
 * Returns int 0 on success
 */
int sepinit_do_fw_init(struct sep_drvdata *drvdata);

#endif /*__SEP_INIT_H__*/
