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


/* \file cryto_api.h
   Definitions of Linux Crypto API shared with the main driver code
 */

#ifndef __CRYPTO_API_H__
#define __CRYPTO_API_H__

#include <linux/crypto.h>
#include <crypto/algapi.h>
#include <crypto/hash.h>
#include <crypto/internal/hash.h>
#include <crypto/sha.h>
#include <linux/workqueue.h>

/**
 * struct async_req_ctx - Context for async. request (__ctx of request)
 * @op_ctx:		SeP operation context
 * @initiating_req:	The initiating crypto request
 * @comp_work:		Completion work handler
 */
struct async_req_ctx {
	struct sep_op_ctx op_ctx;
	struct crypto_async_request *initiating_req;
	struct work_struct comp_work;
};

/* Crypto-API init. entry point (to be used by sep_setup) */
int dx_crypto_api_init(struct sep_drvdata *drvdata);
void dx_crypto_api_fini(void);

int hwk_init(void);
void hwk_fini(void);

#endif /*__CRYPTO_API_H__*/



