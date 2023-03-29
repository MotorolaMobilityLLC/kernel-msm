/*
 * Copyright (c) 2017 The Linux Foundation. All rights reserved.
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

#ifndef __WMA_FIPS_API_H
#define __WMA_FIPS_API_H

#include "wma_api.h"
#include "wmi_unified_api.h"
#include "wma_fips_public_structs.h"

#ifdef WLAN_FEATURE_FIPS
/**
 * wma_fips_request() - Perform a FIPS certification operation
 * @handle: WMA handle of the object being certified
 * @param: The FIPS certification parameters
 * @callback: Callback function to invoke with the results
 * @context: Opaque context to pass back to caller in the callback
 *
 * Return: QDF_STATUS_SUCCESS if the request is successfully sent
 * to firmware for processing, otherwise an error status.
 */
QDF_STATUS wma_fips_request(WMA_HANDLE handle,
			    struct fips_params *param,
			    wma_fips_cb callback,
			    void *context);

/**
 * wma_fips_register_event_handlers() - Register FIPS event handlers
 * @handle: WMA handle of the object being initialized
 *
 * This function registers all WMI event handlers required by the FIPS
 * feature.
 *
 * Return: QDF_STATUS_SUCCESS upon success, otherwise an error
 */
QDF_STATUS wma_fips_register_event_handlers(WMA_HANDLE handle);

#else /* WLAN_FEATURE_FIPS */

static inline
QDF_STATUS wma_fips_request(WMA_HANDLE handle,
			    const struct fips_params *param,
			    wma_fips_cb callback,
			    void *context)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline
QDF_STATUS wma_fips_register_event_handlers(WMA_HANDLE wma_handle)
{
	return QDF_STATUS_SUCCESS;
}

#endif /* WLAN_FEATURE_FIPS */

#endif /* __WMA_FIPS_API_H */
