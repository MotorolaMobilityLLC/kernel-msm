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

#if !defined(WLAN_HDD_LPASS_H)
#define WLAN_HDD_LPASS_H

struct cds_config_info;
struct wma_tgt_cfg;
struct hdd_context;
struct hdd_adapter;

#ifdef WLAN_FEATURE_LPSS
/**
 * hdd_lpass_target_config() - Handle LPASS target configuration
 * @hdd_ctx: HDD global context where lpass information is stored
 * @target_config: Target configuration containing lpass info
 *
 * This function updates the HDD context with lpass-specific
 * information provided by the target.
 *
 * Return: none
 */
void hdd_lpass_target_config(struct hdd_context *hdd_ctx,
			     struct wma_tgt_cfg *target_config);

/**
 * hdd_lpass_populate_cds_config() - Populate LPASS configuration
 * @cds_config: CDS configuration to populate with lpass info
 * @hdd_ctx: HDD global context which contains lpass information
 *
 * This function seeds the CDS configuration structure with
 * lpass-specific information gleaned from the HDD context.
 *
 * Return: none
 */
void hdd_lpass_populate_cds_config(struct cds_config_info *cds_config,
				   struct hdd_context *hdd_ctx);

/**
 * hdd_lpass_populate_pmo_config() - Populate LPASS configuration
 * @pmo_config: PMO configuration to populate with lpass info
 * @hdd_ctx: HDD global context which contains lpass information
 *
 * This function seeds the PMO configuration structure with
 * lpass-specific information gleaned from the HDD context.
 *
 * Return: none
 */
void hdd_lpass_populate_pmo_config(struct pmo_psoc_cfg *pmo_config,
				   struct hdd_context *hdd_ctx);

/**
 * hdd_lpass_notify_connect() - Notify LPASS of interface connect
 * @adapter: The adapter that connected
 *
 * This function is used to notify the LPASS feature that an adapter
 * has connected.
 *
 * Return: none
 */
void hdd_lpass_notify_connect(struct hdd_adapter *adapter);

/**
 * hdd_lpass_notify_disconnect() - Notify LPASS of interface disconnect
 * @adapter: The adapter that connected
 *
 * This function is used to notify the LPASS feature that an adapter
 * has disconnected.
 *
 * Return: none
 */
void hdd_lpass_notify_disconnect(struct hdd_adapter *adapter);

/**
 * hdd_lpass_notify_mode_change() - Notify LPASS of interface mode change
 * @adapter: The adapter whose mode was changed
 *
 * This function is used to notify the LPASS feature that an adapter
 * had its mode changed.
 *
 * Return: none
 */
void hdd_lpass_notify_mode_change(struct hdd_adapter *adapter);

/**
 * hdd_lpass_notify_start() - Notify LPASS of driver start
 * @hdd_ctx: The global HDD context
 * @adapter: adapter for which notification is send
 *
 * This function is used to notify the LPASS feature that the wlan
 * driver has (re-)started.
 *
 * Return: none
 */
void hdd_lpass_notify_start(struct hdd_context *hdd_ctx,
			    struct hdd_adapter *adapter);

/**
 * hdd_lpass_notify_stop() - Notify LPASS of driver stop
 * @hdd_ctx: The global HDD context
 *
 * This function is used to notify the LPASS feature that the wlan
 * driver has stopped.
 *
 * Return: none
 */
void hdd_lpass_notify_stop(struct hdd_context *hdd_ctx);

/**
 * hdd_lpass_is_supported() - Is lpass feature supported?
 * @hdd_ctx: The global HDD context
 *
 * Return: true if feature is enabled and supported by firmware, false
 * if the feature is not enabled or not supported by firmware.
 */
bool hdd_lpass_is_supported(struct hdd_context *hdd_ctx);

/*
 * hdd_lpass_notify_wlan_version() - Notify LPASS WLAN Host/FW version
 * @hdd_ctx: The global HDD context
 *
 * Notify LPASS for the WLAN host/firmware and hardware version.
 *
 * Return: none
 */
void hdd_lpass_notify_wlan_version(struct hdd_context *hdd_ctx);
#else
static inline void hdd_lpass_target_config(struct hdd_context *hdd_ctx,
					   struct wma_tgt_cfg *target_config)
{
}
static inline
void hdd_lpass_populate_cds_config(struct cds_config_info *cds_config,
				   struct hdd_context *hdd_ctx)
{
}

static inline
void hdd_lpass_populate_pmo_config(struct pmo_psoc_cfg *pmo_config,
				   struct hdd_context *hdd_ctx)
{
}

static inline void hdd_lpass_notify_connect(struct hdd_adapter *adapter)
{
}
static inline void hdd_lpass_notify_disconnect(struct hdd_adapter *adapter)
{
}
static inline void hdd_lpass_notify_mode_change(struct hdd_adapter *adapter)
{
}

static inline void hdd_lpass_notify_start(struct hdd_context *hdd_ctx,
					  struct hdd_adapter *adapter)
{
}

static inline void hdd_lpass_notify_stop(struct hdd_context *hdd_ctx) { }
static inline bool hdd_lpass_is_supported(struct hdd_context *hdd_ctx)
{
	return false;
}

static inline void hdd_lpass_notify_wlan_version(struct hdd_context *hdd_ctx)
{
}

#endif

#endif /* WLAN_HDD_LPASS_H */
