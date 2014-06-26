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
/**
 * Kernel services API of power state control (sleep, warm-boot)
 */
#ifndef __DX_SEP_POWER_KAPI_H__
#define __DX_SEP_POWER_KAPI_H__

#include <linux/types.h>

/******************************************/
/* Power state control (sleep, warm-boot) */
/******************************************/

/**
 * Power states of SeP
 */
enum dx_sep_power_state {
	DX_SEP_POWER_INVALID = -1,	/* SeP is in unexpected (error) state */
	DX_SEP_POWER_OFF = 0,	/* SeP is assumed to be off (unreachable) */
	DX_SEP_POWER_BOOT,	/* SeP is in (warm) boot process */
	DX_SEP_POWER_IDLE,	/* SeP is running but no request is pending */
	DX_SEP_POWER_ACTIVE,	/* SeP is running and processing */
	DX_SEP_POWER_HIBERNATED	/* SeP is in hibernated (sleep) state */
};

/* Prototype for callback on sep state change */
typedef void (*dx_sep_state_change_callback_t) (unsigned long cookie);

/**
 * dx_sep_power_state_set() - Change power state of SeP (CC)
 *
 * @req_state:	The requested power state (_HIBERNATED or _ACTIVE)
 *
 * Request changing of power state to given state and block until transition
 * is completed.
 * Requesting _HIBERNATED is allowed only from _ACTIVE state.
 * Requesting _ACTIVE is allowed only after CC was powered back on (warm boot).
 * Return codes:
 * 0 -	Power state change completed.
 * -EINVAL -	This request is not allowed in current SeP state or req_state
 *		value is invalid.
 * -EBUSY -	State change request ignored because SeP is busy (primarily,
 *		when requesting hibernation while SeP is processing something).
 * -ETIME -	Request timed out (primarily, when asking for _ACTIVE)
 */
int dx_sep_power_state_set(enum dx_sep_power_state req_state);

/**
 * dx_sep_power_state_get() - Get the current power state of SeP (CC)
 * @state_jiffies_p:	The "jiffies" value at which given state was detected.
 */
enum dx_sep_power_state dx_sep_power_state_get(unsigned long *state_jiffies_p);

#endif /*__DX_SEP_POWER_KAPI_H__*/
