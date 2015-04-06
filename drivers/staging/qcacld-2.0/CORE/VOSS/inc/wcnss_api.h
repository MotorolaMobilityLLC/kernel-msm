/*
 * Copyright (c) 2013-2014 The Linux Foundation. All rights reserved.
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

#ifndef _WCNSS_API_H_
#define _WCNSS_API_H_


/*
 * Do nothing for non ISOC
 */
#define wcnss_wlan_get_drvdata(dev) NULL

static inline void wcnss_wlan_register_pm_ops(void *dev, void *pm_ops)
{
}

static inline void wcnss_wlan_unregister_pm_ops(void *dev, void *pm_ops)
{
}

static inline void wcnss_register_thermal_mitigation(void *dev, void *tmnotify)
{
}

static inline void wcnss_unregister_thermal_mitigation(void *tm_notify)
{
}

static inline void wcnss_prevent_suspend(void)
{
}

static inline void wcnss_allow_suspend(void)
{
}

static inline unsigned int wcnss_get_serial_number(void)
{
        return 0;
}

#if !defined(CONFIG_CNSS) && !defined(HIF_USB) && !defined(HIF_SDIO)
static inline void *wcnss_wlan_crypto_alloc_ahash(const char *alg_name,
                                                  unsigned int type,
                                                  unsigned int mask)
{
        return NULL;
}

static inline int wcnss_wlan_crypto_ahash_digest(void *req)
{
        return 0;
}

static inline void wcnss_wlan_crypto_free_ahash(void *tfm)
{
}

static inline int wcnss_wlan_crypto_ahash_setkey(void *tfm,
                                                 const u8 *key,
                                                 unsigned int keylen)
{
        return 0;
}

static inline void *wcnss_wlan_crypto_alloc_ablkcipher(const char *alg_name,
                                                       u32 type, u32 mask)
{
        return NULL;
}

static inline void wcnss_wlan_ablkcipher_request_free(void *req)
{
}

static inline void wcnss_wlan_crypto_free_ablkcipher(void *tfm)
{
}
#endif /* !CONFIG_CNSS */

static inline int req_riva_power_on_lock(char *driver_name)
{
        return 0;
}

static inline int free_riva_power_on_lock(char *driver_name)
{
        return 0;
}


#endif	/* #ifndef _WCNSS_API_H_ */
