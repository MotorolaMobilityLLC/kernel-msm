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
#ifndef __SEP_POWER_H__
#define __SEP_POWER_H__

/**
 * dx_sep_power_init() - Init resources for this module
 */
void dx_sep_power_init(struct sep_drvdata *drvdata);

/**
 * dx_sep_power_exit() - Cleanup resources for this module
 */
void dx_sep_power_exit(void);

/**
 * dx_sep_state_change_handler() - Interrupt handler for SeP state changes
 * @drvdata:	Associated driver context
 */
void dx_sep_state_change_handler(struct sep_drvdata *drvdata);

/**
 * dx_sep_wait_for_state() - Wait for SeP to reach one of the states reflected
 *				with given state mask
 * @state_mask:		The OR of expected SeP states
 * @timeout_msec:	Timeout of waiting for the state (in millisec.)
 *
 * Returns the state reached. In case of wait timeout the returned state
 * may not be one of the expected states.
 */
u32 dx_sep_wait_for_state(u32 state_mask, int timeout_msec);

void dx_sep_pm_runtime_get(void);

void dx_sep_pm_runtime_put(void);

#endif				/* __SEP_POWER_H__ */
