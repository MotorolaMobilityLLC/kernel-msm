/*
 * Copyright (c) 2016-2021 The Linux Foundation. All rights reserved.
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

#if !defined __HDD_REGULATORY_H
#define __HDD_REGULATORY_H

/**
 * DOC: wlan_hdd_regulatory.h
 *
 * HDD Regulatory prototype implementation
 */

struct hdd_context;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)) || defined(WITH_BACKPORTS)
#define IEEE80211_CHAN_PASSIVE_SCAN IEEE80211_CHAN_NO_IR
#endif

#define CHANNEL_LIST_UPDATE_TIMEOUT 4500

/**
 * hdd_update_regulatory_config() - API to update regulatory config parameters
 * @hdd_ctx: HDD context
 *
 * Return: 0 on success, err on failure
 */
int hdd_update_regulatory_config(struct hdd_context *hdd_ctx);

/**
 * hdd_init_regulatory_update_event() - Initialize the regulatory update event
 * @hdd_ctx: HDD context
 *
 * Return: 0 on success, err on failure
 */
int hdd_init_regulatory_update_event(struct hdd_context *hdd_ctx);

/**
 * hdd_deinit_regulatory_update_event() - Cleanup the regulatory update event
 * @hdd_ctx: HDD context
 *
 * Return: none
 */
void hdd_deinit_regulatory_update_event(struct hdd_context *hdd_ctx);

int hdd_regulatory_init(struct hdd_context *hdd_ctx, struct wiphy *wiphy);

/**
 * hdd_regulatory_deinit() - cleanup all components of regulatory
 * @hdd_ctx: HDD context
 *
 * Return: None
 */
void hdd_regulatory_deinit(struct hdd_context *hdd_ctx);

void hdd_program_country_code(struct hdd_context *hdd_ctx);
void hdd_reset_global_reg_params(void);

/**
 * hdd_send_wiphy_regd_sync_event() - sends the regulatory sync event
 * @hdd_ctx: HDD context
 *
 * Return: None
 */
void hdd_send_wiphy_regd_sync_event(struct hdd_context *hdd_ctx);

/**
 * hdd_reg_set_country() - helper function for setting the regulatory country
 * @hdd_ctx: the HDD context to set the country for
 * @country_code: the two character country code to configure
 *
 * Return: zero for success, non-zero error code for failure
 */
int hdd_reg_set_country(struct hdd_context *hdd_ctx, char *country_code);

/**
 * hdd_reg_legacy_setband_to_reg_wifi_band_bitmap() - Convert the user space
 *	band input to a bitmap of band capabilities, with reg_wifi_band as the
 *	bit value
 * @qca_setband: user space/setband value band input, can be 0, 1, or 2
 *
 * Return: bitmap on top of reg_wifi_band of bands enabled
 */
uint32_t hdd_reg_legacy_setband_to_reg_wifi_band_bitmap(uint8_t qca_setband);

/**
 * hdd_reg_set_band() - helper function for setting the regulatory band
 * @hdd_ctx: the HDD context to set the band for
 * @band_bitmap: the band bitmap to configure
 *
 * Return: zero for success, non-zero error code for failure
 */
int hdd_reg_set_band(struct net_device *dev, uint32_t band_bitmap);

/**
 * hdd_update_indoor_channel() - enable/disable indoor channel
 * @hdd_ctx: hdd context
 * @disable: whether to enable / disable indoor channel
 *
 * enable/disable indoor channel in wiphy/cds
 *
 * Return: void
 */
void hdd_update_indoor_channel(struct  hdd_context *hdd_ctx,
					bool disable);
/**
 * hdd_modify_indoor_channel_state_flags() - modify wiphy flags and cds state
 * @wiphy_chan: wiphy channel number
 * @cds_chan: cds channel structure
 * @chan_enum: channel enum maintain in reg db
 * @chan_num: channel index
 * @disable: Disable/enable the flags
 *
 * Modify wiphy flags and cds state if channel is indoor.
 *
 * Return: void
 */
void hdd_modify_indoor_channel_state_flags(
	struct hdd_context *hdd_ctx,
	struct ieee80211_channel *wiphy_chan,
	struct regulatory_channel *cds_chan,
	enum channel_enum chan_enum, int chan_num, bool disable);

/**
 * hdd_update_regdb_offload_config() - Update regdb offload disable ini
 * for regulatory component.
 *
 * @psoc: psoc ptr
 *
 * Return: None
 */
void hdd_update_regdb_offload_config(struct hdd_context *hdd_ctx);

#endif
