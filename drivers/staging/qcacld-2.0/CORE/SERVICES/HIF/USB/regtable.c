/*
 * Copyright (c) 2014 The Linux Foundation. All rights reserved.
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

#include "bmi_msg.h"
#include "targaddrs.h"
#include "regtable.h"
#include "ar9888def.h"
#include "ar6320def.h"
#include "ar6320v2def.h"

void target_register_tbl_attach(struct hif_usb_softc *sc, u32 target_type)
{
    switch (target_type) {
    case TARGET_TYPE_AR9888:
        sc->targetdef = &ar9888_targetdef;
        break;
    case TARGET_TYPE_AR6320:
        sc->targetdef = &ar6320_targetdef;
        break;
    case TARGET_TYPE_AR6320V2:
        sc->targetdef = &ar6320v2_targetdef;
        break;
    default:
        break;
    }
}

void hif_register_tbl_attach(struct hif_usb_softc *sc, u32 hif_type)
{
    switch (hif_type) {
    case HIF_TYPE_AR9888:
        sc->hostdef = &ar9888_hostdef;
        break;
    case HIF_TYPE_AR6320:
        sc->hostdef = &ar6320_hostdef;
        break;
    case HIF_TYPE_AR6320V2:
        sc->hostdef = &ar6320v2_hostdef;
        break;
    default:
        break;
    }
}
