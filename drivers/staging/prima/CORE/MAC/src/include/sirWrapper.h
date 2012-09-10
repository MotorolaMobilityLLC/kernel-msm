/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
 * Airgo Networks, Inc proprietary. All rights reserved.
 * This file sirWrapper.h contains the common definitions used by all
 * Firmware modules.
 *
 * Author:      Susan Tsao
 * Date:        09/24/2007
 * History:-
 * Date         Modified by    Modification Information
 * --------------------------------------------------------------------
 */

#ifndef __SIR_WRAPPER_H_
#define __SIR_WRAPPER_H_

#if defined VOSS_ENABLED
#include "VossWrapper.h"
#elif defined ANI_OS_TYPE_OSX
#include "OSXWrapper.h"
#else   // ANI_OS_TYPE undefined
#error Unknown or missing ANI_OS_TYPE macro
#endif  // ANI_OS_TYPE




#endif //__SIR_WRAPPER_H_

