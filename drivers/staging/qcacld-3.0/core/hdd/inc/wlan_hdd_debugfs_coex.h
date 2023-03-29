/*
 * Copyright (c) 2019 The Linux Foundation. All rights reserved.
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

#ifndef _WLAN_HDD_DEBUGFS_COEX_H
#define _WLAN_HDD_DEBUGFS_COEX_H

#ifdef WLAN_MWS_INFO_DEBUGFS
/**
 * hdd_debugfs_mws_coex_info_init() - MWS coex initialization
 * @hdd_ctx: Pointer to the hdd_ctx
 *
 * This function is called to initialize the coex debugfs.
 * Return: None
 */
void hdd_debugfs_mws_coex_info_init(struct hdd_context *hdd_ctx);

/**
 * hdd_debugfs_mws_coex_info_deinit() - MWS coex deintialization
 * @hdd_ctx: Pointer to the hdd_ctx
 *
 * This function is called to deinitialize the coex debugfs.
 * Return: None
 */
void hdd_debugfs_mws_coex_info_deinit(struct hdd_context *hdd_ctx);
#else
static inline void hdd_debugfs_mws_coex_info_init(struct hdd_context *hdd_ctx)
{
}

static inline void hdd_debugfs_mws_coex_info_deinit(struct hdd_context *hdd_ctx)
{
}
#endif
#endif
