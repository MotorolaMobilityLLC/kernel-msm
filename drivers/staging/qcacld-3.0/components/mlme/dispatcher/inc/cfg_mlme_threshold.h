/*
 * Copyright (c) 2012-2018 The Linux Foundation. All rights reserved.
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

#ifndef __CFG_MLME_THRESHOLD_H
#define __CFG_MLME_THRESHOLD_H

#include "wni_cfg.h"

/*
 * <ini>
 * RTSThreshold - Will provide RTSThreshold
 * @Min: 0
 * @Max: 1048576
 * @Default: 2347
 *
 * This ini is used to set default RTSThreshold
 * If minimum value 0 is selectd then it will use always RTS
 * max is the max frame size
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_RTS_THRESHOLD CFG_INI_UINT( \
		"RTSThreshold", \
		0, \
		1048576, \
		2347, \
		CFG_VALUE_OR_DEFAULT, \
		"Default RTS Threshold")

/*
 * <ini>
 * gFragmentationThreshold - It will set fragmentation threshold
 * @Min: 256
 * @Max: 8000
 * @Default: 8000
 *
 * This ini is used to indicate default fragmentation threshold
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_FRAG_THRESHOLD CFG_INI_UINT( \
		"gFragmentationThreshold", \
		256, \
		8000, \
		8000, \
		CFG_VALUE_OR_DEFAULT, \
		"Default Fragmentation Threshold")

#define CFG_THRESHOLD_ALL \
	CFG(CFG_RTS_THRESHOLD) \
	CFG(CFG_FRAG_THRESHOLD)

#endif /* __CFG_MLME_MAIN_H */
