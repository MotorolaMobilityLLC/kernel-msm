/*copyright (c) 2015 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
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
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

#ifndef __WMA_OCB_H
#define __WMA_OCB_H

#include "wma.h"
#include "sirApi.h"

int wma_ocb_set_config_resp(tp_wma_handle wma_handle, uint8_t status);

int wma_ocb_set_config_req(tp_wma_handle handle,
			   struct sir_ocb_config *config_req);

int wma_ocb_set_config_event_handler(void *handle, uint8_t *event_buf,
				     uint32_t len);

int wma_ocb_start_resp_ind_cont(tp_wma_handle wma_handle);

int wma_ocb_set_config(tp_wma_handle wma_handle, struct sir_ocb_config *config);

int wma_ocb_set_utc_time(tp_wma_handle wma_handle, struct sir_ocb_utc *utc);

int wma_ocb_start_timing_advert(tp_wma_handle wma_handle,
				struct sir_ocb_timing_advert *timing_advert);

int wma_ocb_stop_timing_advert(tp_wma_handle wma_handle,
			       struct sir_ocb_timing_advert *timing_advert);

int wma_ocb_get_tsf_timer(tp_wma_handle wma_handle,
			  struct sir_ocb_get_tsf_timer *request);

int wma_dcc_get_stats(tp_wma_handle wma_handle,
		      struct sir_dcc_get_stats *get_stats_param);

int wma_dcc_clear_stats(tp_wma_handle wma_handle,
			struct sir_dcc_clear_stats *clear_stats_param);

int wma_dcc_update_ndl(tp_wma_handle wma_handle,
		       struct sir_dcc_update_ndl *update_ndl_param);

int wma_ocb_register_event_handlers(tp_wma_handle wma_handle);

#endif /* __WMA_OCB_H */
