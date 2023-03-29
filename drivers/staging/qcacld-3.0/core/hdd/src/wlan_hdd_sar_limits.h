/*
 * Copyright (c) 2012-2020 The Linux Foundation. All rights reserved.
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

#ifndef __WLAN_HDD_SAR_LIMITS_H
#define __WLAN_HDD_SAR_LIMITS_H

/**
 * DOC: wlan_hdd_sar_limits_h
 *
 * WLAN Host Device Driver SAR limits API specification
 */

#if defined(FEATURE_SAR_LIMITS) && defined(SAR_SAFETY_FEATURE)
/**
 * wlan_hdd_sar_unsolicited_timer_start() - Start SAR unsolicited timer
 * @hdd_ctx: Pointer to HDD context
 *
 * This function checks the state of the sar unsolicited timer, if the
 * sar_unsolicited_timer is not runnig, it starts the timer.
 *
 * Return: None
 */
void wlan_hdd_sar_unsolicited_timer_start(struct hdd_context *hdd_ctx);

/**
 * wlan_hdd_sar_safety_timer_reset() - Reset SAR sefety timer
 * @hdd_ctx: Pointer to HDD context
 *
 * This function checks the state of the sar safety timer, if the
 * sar_safety_timer is not runnig, it starts the timer else it stops
 * the timer and start the timer again.
 *
 * Return: None
 */
void wlan_hdd_sar_timers_reset(struct hdd_context *hdd_ctx);

/**
 * wlan_hdd_sar_timers_init() - Initialize SAR timers
 * @hdd_ctx: Pointer to HDD context
 *
 * This function initializes sar timers.
 * Return: None
 */
void wlan_hdd_sar_timers_init(struct hdd_context *hdd_ctx);

/**
 * wlan_hdd_sar_timers_deinit() - De-initialize SAR timers
 * @hdd_ctx: Pointer to HDD context
 *
 * This function de-initializes sar timers.
 * Return: None
 */
void wlan_hdd_sar_timers_deinit(struct hdd_context *hdd_ctx);

/**
 * hdd_configure_sar_index() - configures SAR index to the FW
 * @hdd_ctx: Pointer to HDD context
 * @sar_index: sar index which needs to be configured to FW
 *
 * This function configures SAR power index on both the chains
 * for SAR version2
 *
 * Return: None
 */
void hdd_configure_sar_index(struct hdd_context *hdd_ctx, uint32_t sar_index);

/**
 * hdd_disable_sar() - Disable SAR feature to FW
 * @hdd_ctx: Pointer to HDD context
 *
 * This function Disables SAR power index on both the chains
 *
 * Return: None
 */
void hdd_disable_sar(struct hdd_context *hdd_ctx);

/**
 * hdd_configure_sar_sleep_index() - Configure SAR sleep index to FW
 * @hdd_ctx: Pointer to HDD context
 *
 * This function configures SAR sleep index on both the chains
 *
 * Return: None
 */
void hdd_configure_sar_sleep_index(struct hdd_context *hdd_ctx);

/**
 * hdd_configure_sar_resume_index() - Configure SAR resume index to FW
 * @hdd_ctx: Pointer to HDD context
 *
 * This function configures SAR resume index on both the chains
 *
 * Return: None
 */
void hdd_configure_sar_resume_index(struct hdd_context *hdd_ctx);

#else
static inline void wlan_hdd_sar_unsolicited_timer_start(
						struct hdd_context *hdd_ctx)
{
}

static inline void wlan_hdd_sar_timers_reset(struct hdd_context *hdd_ctx)
{
}

static inline void wlan_hdd_sar_timers_init(struct hdd_context *hdd_ctx)
{
}

static inline void wlan_hdd_sar_timers_deinit(struct hdd_context *hdd_ctx)
{
}

static inline void hdd_configure_sar_index(struct hdd_context *hdd_ctx,
					   uint32_t sar_index)
{
}

static inline void hdd_disable_sar(struct hdd_context *hdd_ctx)
{
}

static inline void hdd_configure_sar_sleep_index(struct hdd_context *hdd_ctx)
{
}

static inline void hdd_configure_sar_resume_index(struct hdd_context *hdd_ctx)
{
}

#endif

#ifdef FEATURE_SAR_LIMITS
/**
 * wlan_hdd_cfg80211_get_sar_power_limits() - Get SAR power limits
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Length of @data
 *
 * Wrapper function of __wlan_hdd_cfg80211_get_sar_power_limits()
 *
 * Return: 0 on success, negative errno on failure
 */
int wlan_hdd_cfg80211_get_sar_power_limits(struct wiphy *wiphy,
					   struct wireless_dev *wdev,
					   const void *data,
					   int data_len);

/**
 * wlan_hdd_cfg80211_set_sar_power_limits() - Set SAR power limits
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Length of @data
 *
 * Wrapper function of __wlan_hdd_cfg80211_set_sar_power_limits()
 *
 * Return: 0 on success, negative errno on failure
 */
int wlan_hdd_cfg80211_set_sar_power_limits(struct wiphy *wiphy,
					   struct wireless_dev *wdev,
					   const void *data,
					   int data_len);

/**
 * hdd_store_sar_config() - Store SAR config in HDD context
 * @hdd_ctx: The HDD context
 * @sar_limit_cmd: The sar_limit_cmd_params struct to save
 *
 * After SSR, the SAR configuration is lost. As SSR is hidden from
 * userland, this command will not come from userspace after a SSR. To
 * restore this configuration, save this in hdd context and restore
 * after re-init.
 *
 * Return: None
 */
void hdd_store_sar_config(struct hdd_context *hdd_ctx,
			  struct sar_limit_cmd_params *sar_limit_cmd);

/**
 * hdd_free_sar_config() - Free the resources allocated while storing SAR config
 * @hdd_ctx: HDD context
 *
 * The driver stores the SAR config values in HDD context so that it can be
 * restored in the case SSR is invoked. Free those resources.
 *
 * Return: None
 */
void wlan_hdd_free_sar_config(struct hdd_context *hdd_ctx);

extern const struct nla_policy
wlan_hdd_sar_limits_policy[QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_MAX + 1];

#define FEATURE_SAR_LIMITS_VENDOR_COMMANDS                              \
{                                                                       \
	.info.vendor_id = QCA_NL80211_VENDOR_ID,                        \
	.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_GET_SAR_LIMITS,        \
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV |                           \
		 WIPHY_VENDOR_CMD_NEED_RUNNING,                         \
	.doit = wlan_hdd_cfg80211_get_sar_power_limits,                 \
	vendor_command_policy(VENDOR_CMD_RAW_DATA, 0)                   \
},                                                                      \
{                                                                       \
	.info.vendor_id = QCA_NL80211_VENDOR_ID,                        \
	.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SET_SAR_LIMITS,        \
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV |                           \
		 WIPHY_VENDOR_CMD_NEED_RUNNING,                         \
	.doit = wlan_hdd_cfg80211_set_sar_power_limits,                 \
	vendor_command_policy(wlan_hdd_sar_limits_policy,               \
			      QCA_WLAN_VENDOR_ATTR_SAR_LIMITS_MAX)      \
},
#else /* FEATURE_SAR_LIMITS */
#define FEATURE_SAR_LIMITS_VENDOR_COMMANDS
static inline
void hdd_store_sar_config(struct hdd_context *hdd_ctx,
			  struct sar_limit_cmd_params *sar_limit_cmd)
{
}

static inline void wlan_hdd_free_sar_config(struct hdd_context *hdd_ctx)
{
}

#endif /* FEATURE_SAR_LIMITS */

#endif /* __WLAN_HDD_SAR_LIMITS_H */

