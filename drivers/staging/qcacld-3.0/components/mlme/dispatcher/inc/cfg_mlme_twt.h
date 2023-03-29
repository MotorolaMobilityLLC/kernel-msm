/*
 * Copyright (c) 2011-2018, 2020 The Linux Foundation. All rights reserved.
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

/**
 * DOC: This file contains centralized definitions of converged configuration.
 */

#ifndef __CFG_MLME_TWT_H
#define __CFG_MLME_TWT_H

/*
 * <ini>
 * twt_requestor - twt requestor.
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This cfg is used to store twt requestor config.
 *
 * Related: NA
 *
 * Supported Feature: 11AX
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_TWT_REQUESTOR CFG_INI_BOOL( \
		"twt_requestor", \
		1, \
		"TWT requestor")
/*
 * <ini>
 * twt_responder - twt responder.
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This cfg is used to store twt responder config.
 *
 * Related: NA
 *
 * Supported Feature: 11AX
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_TWT_RESPONDER CFG_INI_BOOL( \
		"twt_responder", \
		1, \
		"TWT responder")

/*
 * <ini>
 * enable_twt - Enable Target Wake Time support.
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable or disable TWT support.
 *
 * Related: NA
 *
 * Supported Feature: 11AX
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_TWT CFG_INI_BOOL( \
		"enable_twt", \
		1, \
		"TWT support")

/*
 * <ini>
 * twt_congestion_timeout - Target wake time congestion timeout.
 * @Min: 0
 * @Max: 10000
 * @Default: 100
 *
 * STA uses this timer to continuously monitor channel congestion levels to
 * decide whether to start or stop TWT. This ini is used to configure the
 * target wake time congestion timeout value in the units of milliseconds.
 * A value of Zero indicates that this is a host triggered TWT and all the
 * necessary configuration for TWT will be directed from the host.
 *
 * Related: NA
 *
 * Supported Feature: 11AX
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_TWT_CONGESTION_TIMEOUT CFG_INI_UINT( \
		"twt_congestion_timeout", \
		0, \
		10000, \
		100, \
		CFG_VALUE_OR_DEFAULT, \
		"twt congestion timeout")
/*
 * <ini>
 * twt_bcast_req_resp_config - To enable broadcast twt requestor and responder.
 * @Min: 0 Disable the extended twt capability
 * @Max: 3
 * @Default: 1
 *
 * This cfg is used to configure the broadcast TWT requestor and responder.
 * Bitmap for enabling the broadcast twt requestor and responder.
 * BIT 0: Enable/Disable broadcast twt requestor.
 * BIT 1: Enable/Disable broadcast twt responder.
 * BIT 2-31: Reserved
 *
 * Related: CFG_ENABLE_TWT
 * Related: CFG_TWT_RESPONDER
 * Related: CFG_TWT_REQUESTOR
 *
 * Supported Feature: 11AX
 *
 * Usage: External
 *
 * </ini>
 */
/* defines to extract the requestor/responder capabilities from cfg */
#define TWT_BCAST_REQ_INDEX    0
#define TWT_BCAST_REQ_BITS     1
#define TWT_BCAST_RES_INDEX    1
#define TWT_BCAST_RES_BITS     1

#define CFG_BCAST_TWT_REQ_RESP CFG_INI_UINT( \
		"twt_bcast_req_resp_config", \
		0, \
		3, \
		1, \
		CFG_VALUE_OR_DEFAULT, \
		"BROADCAST TWT CAPABILITY")

#define CFG_TWT_GET_BCAST_REQ(_bcast_conf) \
	QDF_GET_BITS(_bcast_conf, \
		     TWT_BCAST_REQ_INDEX, \
		     TWT_BCAST_REQ_BITS)

#define CFG_TWT_GET_BCAST_RES(_bcast_conf) \
	QDF_GET_BITS(_bcast_conf, \
		     TWT_BCAST_RES_INDEX, \
		     TWT_BCAST_RES_BITS)

/*
 * <ini>
 * enable_twt_24ghz - Enable Target wake time when STA is connected on 2.4Ghz
 * band.
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/disable the host TWT when STA is connected to AP
 * in 2.4Ghz band.
 *
 * Related: NA
 *
 * Supported Feature: 11AX
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_TWT_24GHZ CFG_INI_BOOL( \
		"enable_twt_24ghz", \
		true, \
		"enable twt in 2.4Ghz band")

#define CFG_TWT_ALL \
	CFG(CFG_ENABLE_TWT) \
	CFG(CFG_TWT_REQUESTOR) \
	CFG(CFG_TWT_RESPONDER) \
	CFG(CFG_TWT_CONGESTION_TIMEOUT) \
	CFG(CFG_BCAST_TWT_REQ_RESP) \
	CFG(CFG_ENABLE_TWT_24GHZ)
#endif /* __CFG_MLME_TWT_H */
