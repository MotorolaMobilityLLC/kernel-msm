/*
 * Copyright (c) 2011-2012, 2014-2015, 2017-2019, 2021 The Linux Foundation. All rights reserved.
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

/*
 * Author:      Sandesh Goel
 * Date:        02/25/02
 */

#ifndef __POL_DEBUG_H__
#define __POL_DEBUG_H__

#define LOGOFF  0
#define LOGP    1
#define LOGE    2
#define LOGW    3
#define LOG1    4
#define LOG2    5
#define LOG3    6
#define LOG4    7
#define LOGD    8

#define pe_alert_rl(params...) QDF_TRACE_FATAL_RL(QDF_MODULE_ID_PE, params)
#define pe_err_rl(params...) QDF_TRACE_ERROR_RL(QDF_MODULE_ID_PE, params)
#define pe_warn_rl(params...) QDF_TRACE_WARN_RL(QDF_MODULE_ID_PE, params)
#define pe_info_rl(params...) QDF_TRACE_INFO_RL(QDF_MODULE_ID_PE, params)
#define pe_debug_rl(params...) QDF_TRACE_DEBUG_RL(QDF_MODULE_ID_PE, params)

#define pe_alert(params...) QDF_TRACE_FATAL(QDF_MODULE_ID_PE, params)
#define pe_err(params...) QDF_TRACE_ERROR(QDF_MODULE_ID_PE, params)
#define pe_warn(params...) QDF_TRACE_WARN(QDF_MODULE_ID_PE, params)
#define pe_info(params...) QDF_TRACE_INFO(QDF_MODULE_ID_PE, params)
#define pe_debug(params...) QDF_TRACE_DEBUG(QDF_MODULE_ID_PE, params)

#define pe_nofl_alert(params...) \
	QDF_TRACE_FATAL_NO_FL(QDF_MODULE_ID_PE, params)
#define pe_nofl_err(params...) \
	QDF_TRACE_ERROR_NO_FL(QDF_MODULE_ID_PE, params)
#define pe_nofl_warn(params...) \
	QDF_TRACE_WARN_NO_FL(QDF_MODULE_ID_PE, params)
#define pe_nofl_info(params...) \
	QDF_TRACE_INFO_NO_FL(QDF_MODULE_ID_PE, params)
#define pe_nofl_debug(params...) \
	QDF_TRACE_DEBUG_NO_FL(QDF_MODULE_ID_PE, params)
#define pe_nofl_rl_debug(params...) \
	QDF_TRACE_DEBUG_RL_NO_FL(QDF_MODULE_ID_PE, params)
#define pe_nofl_rl_info(params...) \
	QDF_TRACE_INFO_RL_NO_FL(QDF_MODULE_ID_PE, params)

#define PE_ENTER() QDF_TRACE_ENTER(QDF_MODULE_ID_PE, "enter")
#define PE_EXIT() QDF_TRACE_EXIT(QDF_MODULE_ID_PE, "exit")
#endif
