/*
 * Copyright (c) 2012-2021 The Linux Foundation. All rights reserved.
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

/*===========================================================================

			s a p C h S e l e c t . C
   OVERVIEW:

   This software unit holds the implementation of the WLAN SAP modules
   functions for channel selection.

   DEPENDENCIES:

   Are listed for each API below.
   ===========================================================================*/

/*--------------------------------------------------------------------------
   Include Files
   ------------------------------------------------------------------------*/
#include "qdf_trace.h"
#include "csr_api.h"
#include "sme_api.h"
#include "sap_ch_select.h"
#include "sap_internal.h"
#ifdef ANI_OS_TYPE_QNX
#include "stdio.h"
#endif
#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
#include "lim_utils.h"
#include "parser_api.h"
#include <wlan_utility.h>
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */
#include "cds_utils.h"
#include "pld_common.h"
#include "wlan_reg_services_api.h"
#include <wlan_scan_utils_api.h>

/*--------------------------------------------------------------------------
   Function definitions
   --------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------
   Defines
   --------------------------------------------------------------------------*/
#define SAP_DEBUG

#define IS_RSSI_VALID(extRssi, rssi) \
	( \
		((extRssi < rssi) ? true : false) \
	)

#define SET_ACS_BAND(acs_band, sap_ctx) \
{ \
	if (sap_ctx->acs_cfg->start_ch_freq <= \
	    WLAN_REG_CH_TO_FREQ(CHAN_ENUM_2484) && \
	    sap_ctx->acs_cfg->end_ch_freq <= \
			WLAN_REG_CH_TO_FREQ(CHAN_ENUM_2484)) \
		acs_band = eCSR_DOT11_MODE_11g; \
	else if (sap_ctx->acs_cfg->start_ch_freq >= \
		 WLAN_REG_CH_TO_FREQ(CHAN_ENUM_2484))\
		acs_band = eCSR_DOT11_MODE_11a; \
	else \
		acs_band = eCSR_DOT11_MODE_abg; \
}

#define ACS_WEIGHT_AMOUNT_LOCAL    240

#define ACS_WEIGHT_AMOUNT_CONFIG(weights) \
	(((weights) & 0xf) + \
	(((weights) & 0xf0) >> 4) + \
	(((weights) & 0xf00) >> 8) + \
	(((weights) & 0xf000) >> 12) + \
	(((weights) & 0xf0000) >> 16) + \
	(((weights) & 0xf00000) >> 20))

/*
 * LSH/RSH 4 to enhance the accurate since
 * need to do modulation to ACS_WEIGHT_AMOUNT_LOCAL.
 */
#define ACS_WEIGHT_COMPUTE(weights, weight, factor, base) \
	(((((((((weight) << 4) * ACS_WEIGHT_AMOUNT_LOCAL * (factor)) + \
	(ACS_WEIGHT_AMOUNT_CONFIG((weights)) >> 1)) / \
	ACS_WEIGHT_AMOUNT_CONFIG((weights))) + \
	((base) >> 1)) / (base)) + 8) >> 4)

#define ACS_WEIGHT_CFG_TO_LOCAL(weights, weight) \
	(((((((weight) << 4) * ACS_WEIGHT_AMOUNT_LOCAL) + \
	(ACS_WEIGHT_AMOUNT_CONFIG((weights)) >> 1)) / \
	ACS_WEIGHT_AMOUNT_CONFIG((weights))) + 8) >> 4)

#define ACS_WEIGHT_SOFTAP_RSSI_CFG(weights) \
	((weights) & 0xf)

#define ACS_WEIGHT_SOFTAP_COUNT_CFG(weights) \
	(((weights) & 0xf0) >> 4)

#define ACS_WEIGHT_SOFTAP_NOISE_FLOOR_CFG(weights) \
	(((weights) & 0xf00) >> 8)

#define ACS_WEIGHT_SOFTAP_CHANNEL_FREE_CFG(weights) \
	(((weights) & 0xf000) >> 12)

#define ACS_WEIGHT_SOFTAP_TX_POWER_RANGE_CFG(weights) \
	(((weights) & 0xf0000) >> 16)

#define ACS_WEIGHT_SOFTAP_TX_POWER_THROUGHPUT_CFG(weights) \
	(((weights) & 0xf00000) >> 20)

typedef struct {
	uint16_t chStartNum;
	uint32_t weight;
} sapAcsChannelInfo;

sapAcsChannelInfo acs_ht40_channels5_g[] = {
	{36, SAP_ACS_WEIGHT_MAX},
	{44, SAP_ACS_WEIGHT_MAX},
	{52, SAP_ACS_WEIGHT_MAX},
	{60, SAP_ACS_WEIGHT_MAX},
	{100, SAP_ACS_WEIGHT_MAX},
	{108, SAP_ACS_WEIGHT_MAX},
	{116, SAP_ACS_WEIGHT_MAX},
	{124, SAP_ACS_WEIGHT_MAX},
	{132, SAP_ACS_WEIGHT_MAX},
	{140, SAP_ACS_WEIGHT_MAX},
	{149, SAP_ACS_WEIGHT_MAX},
	{157, SAP_ACS_WEIGHT_MAX},
};

sapAcsChannelInfo acs_ht80_channels[] = {
	{36, SAP_ACS_WEIGHT_MAX},
	{52, SAP_ACS_WEIGHT_MAX},
	{100, SAP_ACS_WEIGHT_MAX},
	{116, SAP_ACS_WEIGHT_MAX},
	{132, SAP_ACS_WEIGHT_MAX},
	{149, SAP_ACS_WEIGHT_MAX},
};

sapAcsChannelInfo acs_vht160_channels[] = {
	{36, SAP_ACS_WEIGHT_MAX},
	{100, SAP_ACS_WEIGHT_MAX},
};

sapAcsChannelInfo acs_ht40_channels24_g[] = {
	{1, SAP_ACS_WEIGHT_MAX},
	{2, SAP_ACS_WEIGHT_MAX},
	{3, SAP_ACS_WEIGHT_MAX},
	{4, SAP_ACS_WEIGHT_MAX},
	{9, SAP_ACS_WEIGHT_MAX},
};

#define CHANNEL_165  165

/* rssi discount for channels in PCL */
#define PCL_RSSI_DISCOUNT 10

#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
/**
 * sap_check_n_add_channel() - checks and add given channel in sap context's
 * avoid_channels_info struct
 * @sap_ctx:           sap context.
 * @new_channel:       channel to be added to sap_ctx's avoid ch info
 *
 * sap_ctx contains sap_avoid_ch_info strcut containing the list of channels on
 * which MDM device's AP with MCC was detected. This function will add channels
 * to that list after checking for duplicates.
 *
 * Return: true: if channel was added or already present
 *   else false: if channel list was already full.
 */
static bool
sap_check_n_add_channel(struct sap_context *sap_ctx,
			uint8_t new_channel)
{
	uint8_t i = 0;
	struct sap_avoid_channels_info *ie_info =
		&sap_ctx->sap_detected_avoid_ch_ie;

	for (i = 0; i < sizeof(ie_info->channels); i++) {
		if (ie_info->channels[i] == new_channel)
			break;

		if (ie_info->channels[i] == 0) {
			ie_info->channels[i] = new_channel;
			break;
		}
	}
	if (i == sizeof(ie_info->channels))
		return false;
	else
		return true;
}
/**
 * sap_check_n_add_overlapped_chnls() - checks & add overlapped channels
 *                                      to primary channel in 2.4Ghz band.
 * @sap_ctx:           sap context.
 * @primary_chnl:      primary channel to be avoided.
 *
 * sap_ctx contains sap_avoid_ch_info struct containing the list of channels on
 * which MDM device's AP with MCC was detected. This function will add channels
 * to that list after checking for duplicates.
 *
 * Return: true: if channel was added or already present
 *   else false: if channel list was already full.
 */
static bool
sap_check_n_add_overlapped_chnls(struct sap_context *sap_ctx,
				 uint8_t primary_channel)
{
	uint8_t i = 0, j = 0, upper_chnl = 0, lower_chnl = 0;
	struct sap_avoid_channels_info *ie_info =
		&sap_ctx->sap_detected_avoid_ch_ie;
	/*
	 * if primary channel less than channel 1 or out of 2g band then
	 * no further process is required. return true in this case.
	 */
	if (primary_channel < CHANNEL_1 || primary_channel > CHANNEL_14)
		return true;

	/* lower channel is one channel right before primary channel */
	lower_chnl = primary_channel - 1;
	/* upper channel is one channel right after primary channel */
	upper_chnl = primary_channel + 1;

	/* lower channel needs to be non-zero, zero is not valid channel */
	if (lower_chnl > (CHANNEL_1 - 1)) {
		for (i = 0; i < sizeof(ie_info->channels); i++) {
			if (ie_info->channels[i] == lower_chnl)
				break;
			if (ie_info->channels[i] == 0) {
				ie_info->channels[i] = lower_chnl;
				break;
			}
		}
	}
	/* upper channel needs to be atleast last channel in 2.4Ghz band */
	if (upper_chnl < (CHANNEL_14 + 1)) {
		for (j = 0; j < sizeof(ie_info->channels); j++) {
			if (ie_info->channels[j] == upper_chnl)
				break;
			if (ie_info->channels[j] == 0) {
				ie_info->channels[j] = upper_chnl;
				break;
			}
		}
	}
	if (i == sizeof(ie_info->channels) || j == sizeof(ie_info->channels))
		return false;
	else
		return true;
}

/**
 * sap_process_avoid_ie() - processes the detected Q2Q IE
 * context's avoid_channels_info struct
 * @mac_handle:         opaque handle to the MAC context
 * @sap_ctx:            sap context.
 * @scan_result:        scan results for ACS scan.
 * @spect_info:         spectrum weights array to update
 *
 * Detection of Q2Q IE indicates presence of another MDM device with its AP
 * operating in MCC mode. This function parses the scan results and processes
 * the Q2Q IE if found. It then extracts the channels and populates them in
 * sap_ctx struct. It also increases the weights of those channels so that
 * ACS logic will avoid those channels in its selection algorithm.
 *
 * Return: void
 */
static void
sap_process_avoid_ie(mac_handle_t mac_handle, struct sap_context *sap_ctx,
		     qdf_list_t *scan_list, tSapChSelSpectInfo *spect_info)
{
	const uint8_t *temp_ptr = NULL;
	uint8_t i = 0;
	struct sAvoidChannelIE *avoid_ch_ie;
	struct mac_context *mac_ctx = NULL;
	tSapSpectChInfo *spect_ch = NULL;
	qdf_list_node_t *cur_lst = NULL, *next_lst = NULL;
	struct scan_cache_node *cur_node = NULL;
	uint32_t chan_freq;

	mac_ctx = MAC_CONTEXT(mac_handle);
	spect_ch = spect_info->pSpectCh;

	if (scan_list)
		qdf_list_peek_front(scan_list, &cur_lst);

	while (cur_lst) {
		cur_node = qdf_container_of(cur_lst, struct scan_cache_node,
					    node);

		temp_ptr = wlan_get_vendor_ie_ptr_from_oui(
				SIR_MAC_QCOM_VENDOR_OUI,
				SIR_MAC_QCOM_VENDOR_SIZE,
				util_scan_entry_ie_data(cur_node->entry),
				util_scan_entry_ie_len(cur_node->entry));

		if (temp_ptr) {
			avoid_ch_ie = (struct sAvoidChannelIE *)temp_ptr;
			if (avoid_ch_ie->type !=
					QCOM_VENDOR_IE_MCC_AVOID_CH) {
				qdf_list_peek_next(scan_list,
						   cur_lst, &next_lst);
				cur_lst = next_lst;
				next_lst = NULL;
				continue;
			}

			sap_ctx->sap_detected_avoid_ch_ie.present = 1;

			chan_freq =
			    wlan_reg_legacy_chan_to_freq(mac_ctx->pdev,
							 avoid_ch_ie->channel);

			sap_debug("Q2Q-IE avoid freq = %d", chan_freq);
			/* add this channel to to_avoid channel list */
			sap_check_n_add_channel(sap_ctx, avoid_ch_ie->channel);
			sap_check_n_add_overlapped_chnls(sap_ctx,
							 avoid_ch_ie->channel);
			/*
			 * Mark weight of these channel present in IE to MAX
			 * so that ACS logic will to avoid thse channels
			 */
			for (i = 0; i < spect_info->numSpectChans; i++) {
				if (spect_ch[i].chan_freq != chan_freq)
					continue;
				/*
				 * weight is set more than max so that,
				 * in the case of other channels being
				 * assigned max weight due to noise,
				 * they may be preferred over channels
				 * with Q2Q IE.
				 */
				spect_ch[i].weight = SAP_ACS_WEIGHT_MAX + 1;
				spect_ch[i].weight_copy =
							SAP_ACS_WEIGHT_MAX + 1;
				break;
			}
		}

		qdf_list_peek_next(scan_list, cur_lst, &next_lst);
		cur_lst = next_lst;
		next_lst = NULL;
	}
}
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */

/**
 * sap_select_preferred_channel_from_channel_list() - to calc best cahnnel
 * @best_ch_freq: best chan freq already calculated among all the chanels
 * @sap_ctx: sap context
 * @spectinfo_param: Pointer to tSapChSelSpectInfo structure
 *
 * This function calculates the best channel among the configured channel list.
 * If channel list not configured then returns the best channel calculated
 * among all the channel list.
 *
 * Return: uint32_t best channel frequency
 */
static
uint32_t sap_select_preferred_channel_from_channel_list(uint32_t best_ch_freq,
				struct sap_context *sap_ctx,
				tSapChSelSpectInfo *spectinfo_param)
{
	/*
	 * If Channel List is not Configured don't do anything
	 * Else return the Best Channel from the Channel List
	 */
	if ((!sap_ctx->acs_cfg->freq_list) ||
	    (!spectinfo_param) ||
	    (!sap_ctx->acs_cfg->ch_list_count))
		return best_ch_freq;

	if (wlansap_is_channel_present_in_acs_list(best_ch_freq,
					sap_ctx->acs_cfg->freq_list,
					sap_ctx->acs_cfg->ch_list_count))
		return best_ch_freq;

	return SAP_CHANNEL_NOT_SELECTED;
}

/**
 * sap_chan_sel_init() - Initialize channel select
 * @mac_handle: Opaque handle to the global MAC context
 * @pSpectInfoParams: Pointer to tSapChSelSpectInfo structure
 * @sap_ctx: Pointer to SAP Context
 *
 * Function sap_chan_sel_init allocates the memory, initializes the
 * structures used by the channel selection algorithm
 *
 * Return: bool Success or FAIL
 */
static bool sap_chan_sel_init(mac_handle_t mac_handle,
			      tSapChSelSpectInfo *pSpectInfoParams,
			      struct sap_context *sap_ctx)
{
	tSapSpectChInfo *pSpectCh = NULL;
	uint32_t *pChans = NULL;
	uint16_t channelnum = 0;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	bool include_dfs_ch = true;
	uint8_t sta_sap_scc_on_dfs_chnl_config_value;

	pSpectInfoParams->numSpectChans =
		mac->scan.base_channels.numChannels;

	/* Allocate memory for weight computation of 2.4GHz */
	pSpectCh = qdf_mem_malloc((pSpectInfoParams->numSpectChans) *
			sizeof(*pSpectCh));
	if (!pSpectCh)
		return false;

	/* Initialize the pointers in the DfsParams to the allocated memory */
	pSpectInfoParams->pSpectCh = pSpectCh;

	pChans = mac->scan.base_channels.channel_freq_list;

	policy_mgr_get_sta_sap_scc_on_dfs_chnl(mac->psoc,
			&sta_sap_scc_on_dfs_chnl_config_value);
#if defined(FEATURE_WLAN_STA_AP_MODE_DFS_DISABLE)
	if (sap_ctx->dfs_ch_disable == true)
		include_dfs_ch = false;
#endif
	if (!mac->mlme_cfg->dfs_cfg.dfs_master_capable ||
	    ACS_DFS_MODE_DISABLE == sap_ctx->dfs_mode)
		include_dfs_ch = false;

	/* Fill the channel number in the spectrum in the operating freq band */
	for (channelnum = 0;
	     channelnum < pSpectInfoParams->numSpectChans;
	     channelnum++, pChans++, pSpectCh++) {
		uint8_t channel;

		channel = wlan_reg_freq_to_chan(mac->pdev, *pChans);

		pSpectCh->chan_freq = *pChans;
		/* Initialise for all channels */
		pSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
		/* Initialise max ACS weight for all channels */
		pSpectCh->weight = SAP_ACS_WEIGHT_MAX;

		/* check if the channel is in NOL blacklist */
		if (sap_dfs_is_channel_in_nol_list(
					sap_ctx, channel,
					PHY_SINGLE_CHANNEL_CENTERED)) {
			sap_debug_rl("Ch %d is in NOL list", channel);
			continue;
		}

		if (!include_dfs_ch ||
		    sta_sap_scc_on_dfs_chnl_config_value == 1) {
			if (wlan_reg_is_dfs_for_freq(mac->pdev,
						     pSpectCh->chan_freq)) {
				sap_debug("DFS Ch %d not considered for ACS. include_dfs_ch %u, sta_sap_scc_on_dfs_chnl_config_value %d",
					channel, include_dfs_ch,
					sta_sap_scc_on_dfs_chnl_config_value);
				continue;
			}
		}

		if (!policy_mgr_is_sap_freq_allowed(mac->psoc, *pChans)) {
			QDF_TRACE(QDF_MODULE_ID_SAP, QDF_TRACE_LEVEL_INFO_HIGH,
				  "%s: Skip freq %d", __func__, *pChans);
			continue;
		}

		/* OFDM rates are not supported on channel 14 */
		if (channel == 14 &&
		    eCSR_DOT11_MODE_11b != sap_ctx->csr_roamProfile.phyMode)
			continue;

		/* Skip DSRC channels */
		if (wlan_reg_is_dsrc_chan(mac->pdev, channel))
			continue;

		/*
		 * Skip the channels which are not in ACS config from user
		 * space
		 */
		if (!wlansap_is_channel_present_in_acs_list(*pChans,
					sap_ctx->acs_cfg->freq_list,
					sap_ctx->acs_cfg->ch_list_count))
			continue;

		pSpectCh->valid = true;
		pSpectCh->weight = 0;
	}

	return true;
}

/**
 * sapweight_rssi_count() - calculates the channel weight due to rssi
    and data count(here number of BSS observed)
 * @sap_ctx     : Softap context
 * @rssi        : Max signal strength receieved from a BSS for the channel
 * @count       : Number of BSS observed in the channel
 *
 * Return: uint32_t Calculated channel weight based on above two
 */
static
uint32_t sapweight_rssi_count(struct sap_context *sap_ctx, int8_t rssi,
			      uint16_t count)
{
	int32_t rssiWeight = 0;
	int32_t countWeight = 0;
	uint32_t rssicountWeight = 0;
	uint8_t softap_rssi_weight_cfg, softap_count_weight_cfg;
	uint8_t softap_rssi_weight_local, softap_count_weight_local;

	softap_rssi_weight_cfg =
	    ACS_WEIGHT_SOFTAP_RSSI_CFG(sap_ctx->auto_channel_select_weight);

	softap_count_weight_cfg =
	    ACS_WEIGHT_SOFTAP_COUNT_CFG(sap_ctx->auto_channel_select_weight);

	softap_rssi_weight_local =
	    ACS_WEIGHT_CFG_TO_LOCAL(sap_ctx->auto_channel_select_weight,
				    softap_rssi_weight_cfg);

	softap_count_weight_local =
	    ACS_WEIGHT_CFG_TO_LOCAL(sap_ctx->auto_channel_select_weight,
				    softap_count_weight_cfg);

	/* Weight from RSSI */
	rssiWeight = ACS_WEIGHT_COMPUTE(sap_ctx->auto_channel_select_weight,
					softap_rssi_weight_cfg,
					rssi - SOFTAP_MIN_RSSI,
					SOFTAP_MAX_RSSI - SOFTAP_MIN_RSSI);

	if (rssiWeight > softap_rssi_weight_local)
		rssiWeight = softap_rssi_weight_local;

	else if (rssiWeight < 0)
		rssiWeight = 0;

	/* Weight from data count */
	countWeight = ACS_WEIGHT_COMPUTE(sap_ctx->auto_channel_select_weight,
					 softap_count_weight_cfg,
					 count - SOFTAP_MIN_COUNT,
					 SOFTAP_MAX_COUNT - SOFTAP_MIN_COUNT);

	if (countWeight > softap_count_weight_local)
		countWeight = softap_count_weight_local;

	rssicountWeight = rssiWeight + countWeight;

	sap_debug("rssiWeight=%d, countWeight=%d, rssicountWeight=%d",
		  rssiWeight, countWeight, rssicountWeight);

	return rssicountWeight;
}

/**
 * sap_get_channel_status() - get channel info via channel number
 * @p_mac: Pointer to Global MAC structure
 * @channel_id: channel id
 *
 * Return: chan status info
 */
static struct lim_channel_status *sap_get_channel_status
	(struct mac_context *p_mac, uint32_t chan_freq)
{
	return csr_get_channel_status(p_mac, chan_freq);
}

/**
 * sap_clear_channel_status() - clear chan info
 * @p_mac: Pointer to Global MAC structure
 *
 * Return: none
 */
static void sap_clear_channel_status(struct mac_context *p_mac)
{
	csr_clear_channel_status(p_mac);
}

/**
 * sap_weight_channel_noise_floor() - compute noise floor weight
 * @sap_ctx:  sap context
 * @chn_stat: Pointer to chan status info
 *
 * Return: channel noise floor weight
 */
static uint32_t sap_weight_channel_noise_floor(struct sap_context *sap_ctx,
					       struct lim_channel_status
						*channel_stat)
{
	uint32_t    noise_floor_weight;
	uint8_t     softap_nf_weight_cfg;
	uint8_t     softap_nf_weight_local;

	softap_nf_weight_cfg =
	    ACS_WEIGHT_SOFTAP_NOISE_FLOOR_CFG
	    (sap_ctx->auto_channel_select_weight);

	softap_nf_weight_local =
	    ACS_WEIGHT_CFG_TO_LOCAL(sap_ctx->auto_channel_select_weight,
				    softap_nf_weight_cfg);

	if (!channel_stat || channel_stat->channelfreq == 0)
		return softap_nf_weight_local;

	noise_floor_weight = (channel_stat->noise_floor == 0) ? 0 :
			    (ACS_WEIGHT_COMPUTE(
			     sap_ctx->auto_channel_select_weight,
			     softap_nf_weight_cfg,
			     channel_stat->noise_floor -
			     SOFTAP_MIN_NF,
			     SOFTAP_MAX_NF - SOFTAP_MIN_NF));

	if (noise_floor_weight > softap_nf_weight_local)
		noise_floor_weight = softap_nf_weight_local;

	sap_debug("nf=%d, nfwc=%d, nfwl=%d, nfw=%d",
		  channel_stat->noise_floor,
		  softap_nf_weight_cfg, softap_nf_weight_local,
		  noise_floor_weight);

	return noise_floor_weight;
}

/**
 * sap_weight_channel_free() - compute channel free weight
 * @sap_ctx:  sap context
 * @chn_stat: Pointer to chan status info
 *
 * Return: channel free weight
 */
static uint32_t sap_weight_channel_free(struct sap_context *sap_ctx,
					struct lim_channel_status
					*channel_stat)
{
	uint32_t     channel_free_weight;
	uint8_t      softap_channel_free_weight_cfg;
	uint8_t      softap_channel_free_weight_local;
	uint32_t     rx_clear_count = 0;
	uint32_t     cycle_count = 0;

	softap_channel_free_weight_cfg =
	    ACS_WEIGHT_SOFTAP_CHANNEL_FREE_CFG
	    (sap_ctx->auto_channel_select_weight);

	softap_channel_free_weight_local =
	    ACS_WEIGHT_CFG_TO_LOCAL(sap_ctx->auto_channel_select_weight,
				    softap_channel_free_weight_cfg);

	if (!channel_stat || channel_stat->channelfreq == 0)
		return softap_channel_free_weight_local;

	rx_clear_count = channel_stat->rx_clear_count -
			channel_stat->tx_frame_count -
			channel_stat->rx_frame_count;
	cycle_count = channel_stat->cycle_count;

	/* LSH 4, otherwise it is always 0. */
	channel_free_weight = (cycle_count == 0) ? 0 :
			 (ACS_WEIGHT_COMPUTE(
			  sap_ctx->auto_channel_select_weight,
			  softap_channel_free_weight_cfg,
			 ((rx_clear_count << 8) +
			 (cycle_count >> 1))/cycle_count -
			 (SOFTAP_MIN_CHNFREE << 8),
			 (SOFTAP_MAX_CHNFREE -
			 SOFTAP_MIN_CHNFREE) << 8));

	if (channel_free_weight > softap_channel_free_weight_local)
		channel_free_weight = softap_channel_free_weight_local;

	sap_debug("rcc=%d, cc=%d, tc=%d, rc=%d, cfwc=%d, cfwl=%d, cfw=%d",
		  rx_clear_count, cycle_count,
		  channel_stat->tx_frame_count,
		  channel_stat->rx_frame_count,
		  softap_channel_free_weight_cfg,
		  softap_channel_free_weight_local,
		  channel_free_weight);

	return channel_free_weight;
}

/**
 * sap_weight_channel_txpwr_range() - compute channel tx power range weight
 * @sap_ctx:  sap context
 * @chn_stat: Pointer to chan status info
 *
 * Return: tx power range weight
 */
static uint32_t sap_weight_channel_txpwr_range(struct sap_context *sap_ctx,
					       struct lim_channel_status
					       *channel_stat)
{
	uint32_t     txpwr_weight_low_speed;
	uint8_t      softap_txpwr_range_weight_cfg;
	uint8_t      softap_txpwr_range_weight_local;

	softap_txpwr_range_weight_cfg =
	    ACS_WEIGHT_SOFTAP_TX_POWER_RANGE_CFG
	    (sap_ctx->auto_channel_select_weight);

	softap_txpwr_range_weight_local =
	    ACS_WEIGHT_CFG_TO_LOCAL(sap_ctx->auto_channel_select_weight,
				    softap_txpwr_range_weight_cfg);

	if (!channel_stat || channel_stat->channelfreq == 0)
		return softap_txpwr_range_weight_local;


	txpwr_weight_low_speed = (channel_stat->chan_tx_pwr_range == 0) ? 0 :
				(ACS_WEIGHT_COMPUTE(
				 sap_ctx->auto_channel_select_weight,
				 softap_txpwr_range_weight_cfg,
				 SOFTAP_MAX_TXPWR -
				 channel_stat->chan_tx_pwr_range,
				 SOFTAP_MAX_TXPWR - SOFTAP_MIN_TXPWR));

	if (txpwr_weight_low_speed > softap_txpwr_range_weight_local)
		txpwr_weight_low_speed = softap_txpwr_range_weight_local;

	sap_debug("tpr=%d, tprwc=%d, tprwl=%d, tprw=%d",
		  channel_stat->chan_tx_pwr_range,
		  softap_txpwr_range_weight_cfg,
		  softap_txpwr_range_weight_local,
		  txpwr_weight_low_speed);

	return txpwr_weight_low_speed;
}

/**
 * sap_weight_channel_txpwr_tput() - compute channel tx power
 * throughput weight
 * @sap_ctx:  sap context
 * @chn_stat: Pointer to chan status info
 *
 * Return: tx power throughput weight
 */
static uint32_t sap_weight_channel_txpwr_tput(struct sap_context *sap_ctx,
					      struct lim_channel_status
					      *channel_stat)
{
	uint32_t     txpwr_weight_high_speed;
	uint8_t      softap_txpwr_tput_weight_cfg;
	uint8_t      softap_txpwr_tput_weight_local;

	softap_txpwr_tput_weight_cfg =
	    ACS_WEIGHT_SOFTAP_TX_POWER_THROUGHPUT_CFG
	    (sap_ctx->auto_channel_select_weight);

	softap_txpwr_tput_weight_local =
	    ACS_WEIGHT_CFG_TO_LOCAL(sap_ctx->auto_channel_select_weight,
				    softap_txpwr_tput_weight_cfg);

	if (!channel_stat || channel_stat->channelfreq == 0)
		return softap_txpwr_tput_weight_local;

	txpwr_weight_high_speed = (channel_stat->chan_tx_pwr_throughput == 0)
				  ? 0 : (ACS_WEIGHT_COMPUTE(
				  sap_ctx->auto_channel_select_weight,
				  softap_txpwr_tput_weight_cfg,
				  SOFTAP_MAX_TXPWR -
				  channel_stat->chan_tx_pwr_throughput,
				  SOFTAP_MAX_TXPWR - SOFTAP_MIN_TXPWR));

	if (txpwr_weight_high_speed > softap_txpwr_tput_weight_local)
		txpwr_weight_high_speed = softap_txpwr_tput_weight_local;

	sap_debug("tpt=%d, tptwc=%d, tptwl=%d, tptw=%d",
		  channel_stat->chan_tx_pwr_throughput,
		  softap_txpwr_tput_weight_cfg,
		  softap_txpwr_tput_weight_local,
		  txpwr_weight_high_speed);

	return txpwr_weight_high_speed;
}

/**
 * sap_weight_channel_status() - compute chan status weight
 * @sap_ctx:  sap context
 * @chn_stat: Pointer to chan status info
 *
 * Return: chan status weight
 */
static
uint32_t sap_weight_channel_status(struct sap_context *sap_ctx,
				   struct lim_channel_status *channel_stat)
{
	return sap_weight_channel_noise_floor(sap_ctx, channel_stat) +
	       sap_weight_channel_free(sap_ctx, channel_stat) +
	       sap_weight_channel_txpwr_range(sap_ctx, channel_stat) +
	       sap_weight_channel_txpwr_tput(sap_ctx, channel_stat);
}

/**
 * sap_update_rssi_bsscount() - updates bss count and rssi effect.
 *
 * @pSpectCh:     Channel Information
 * @offset:       Channel Offset
 * @sap_24g:      Channel is in 2.4G or 5G
 * @spectch_start: the start of spect ch array
 * @spectch_end: the end of spect ch array
 *
 * sap_update_rssi_bsscount updates bss count and rssi effect based
 * on the channel offset.
 *
 * Return: None.
 */

static void sap_update_rssi_bsscount(tSapSpectChInfo *pSpectCh, int32_t offset,
	bool sap_24g, tSapSpectChInfo *spectch_start,
	tSapSpectChInfo *spectch_end)
{
	tSapSpectChInfo *pExtSpectCh = NULL;
	int32_t rssi, rsssi_effect;

	pExtSpectCh = (pSpectCh + offset);
	if (pExtSpectCh && pExtSpectCh >= spectch_start &&
	    pExtSpectCh < spectch_end) {
		if (!WLAN_REG_IS_SAME_BAND_FREQS(pSpectCh->chan_freq,
						 pExtSpectCh->chan_freq))
			return;
		++pExtSpectCh->bssCount;
		switch (offset) {
		case -1:
		case 1:
			rsssi_effect = sap_24g ?
			    SAP_24GHZ_FIRST_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY :
			    SAP_SUBBAND1_RSSI_EFFECT_PRIMARY;
			break;
		case -2:
		case 2:
			rsssi_effect = sap_24g ?
			    SAP_24GHZ_SEC_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY :
			    SAP_SUBBAND2_RSSI_EFFECT_PRIMARY;
			break;
		case -3:
		case 3:
			rsssi_effect = sap_24g ?
			    SAP_24GHZ_THIRD_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY :
			    SAP_SUBBAND3_RSSI_EFFECT_PRIMARY;
			break;
		case -4:
		case 4:
			rsssi_effect = sap_24g ?
			    SAP_24GHZ_FOURTH_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY :
			    SAP_SUBBAND4_RSSI_EFFECT_PRIMARY;
			break;
		case -5:
		case 5:
			rsssi_effect = SAP_SUBBAND5_RSSI_EFFECT_PRIMARY;
			break;
		case -6:
		case 6:
			rsssi_effect = SAP_SUBBAND6_RSSI_EFFECT_PRIMARY;
			break;
		case -7:
		case 7:
			rsssi_effect = SAP_SUBBAND7_RSSI_EFFECT_PRIMARY;
			break;
		default:
			rsssi_effect = 0;
			break;
		}

		rssi = pSpectCh->rssiAgr + rsssi_effect;
		if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
			pExtSpectCh->rssiAgr = rssi;
		if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
			pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
	}
}

/**
 * sap_update_rssi_bsscount_vht_5G() - updates bss count and rssi effect.
 *
 * @spect_ch:     Channel Information
 * @offset:       Channel Offset
 * @num_ch:       no.of channels
 * @spectch_start: the start of spect ch array
 * @spectch_end: the end of spect ch array
 *
 * sap_update_rssi_bsscount_vht_5G updates bss count and rssi effect based
 * on the channel offset.
 *
 * Return: None.
 */

static void sap_update_rssi_bsscount_vht_5G(tSapSpectChInfo *spect_ch,
					    int32_t offset,
					    uint16_t num_ch,
					    tSapSpectChInfo *spectch_start,
					    tSapSpectChInfo *spectch_end)
{
	int32_t ch_offset;
	uint16_t i, cnt;

	if (!offset)
		return;
	if (offset > 0)
		cnt = num_ch;
	else
		cnt = num_ch + 1;
	for (i = 0; i < cnt; i++) {
		ch_offset = offset + i;
		if (ch_offset == 0)
			continue;
		sap_update_rssi_bsscount(spect_ch, ch_offset, false,
			spectch_start, spectch_end);
	}
}
/**
 * sap_interference_rssi_count_5G() - sap_interference_rssi_count
 *                                    considers the Adjacent channel rssi and
 *                                    data count(here number of BSS observed)
 * @spect_ch:        Channel Information
 * @chan_width:      Channel width parsed from beacon IE
 * @sec_chan_offset: Secondary Channel Offset
 * @center_freq:     Central frequency for the given channel.
 * @channel_id:      channel_id
 * @spectch_start: the start of spect ch array
 * @spectch_end: the end of spect ch array
 *
 * sap_interference_rssi_count_5G considers the Adjacent channel rssi
 * and data count(here number of BSS observed)
 *
 * Return: NA.
 */

static void sap_interference_rssi_count_5G(tSapSpectChInfo *spect_ch,
					   uint16_t chan_width,
					   uint16_t sec_chan_offset,
					   uint32_t ch_freq0,
					   uint32_t ch_freq1,
					   uint32_t op_chan_freq,
					   tSapSpectChInfo *spectch_start,
					   tSapSpectChInfo *spectch_end)
{
	uint16_t num_ch;
	int32_t offset = 0;

	sap_debug("freq = %d, ch width = %d, ch_freq0 = %d ch_freq1 = %d",
		  op_chan_freq, chan_width, ch_freq0, ch_freq1);

	switch (chan_width) {
	case eHT_CHANNEL_WIDTH_40MHZ:
		switch (sec_chan_offset) {
		/* Above the Primary Channel */
		case PHY_DOUBLE_CHANNEL_LOW_PRIMARY:
			sap_update_rssi_bsscount(spect_ch, 1, false,
						 spectch_start, spectch_end);
			return;

		/* Below the Primary channel */
		case PHY_DOUBLE_CHANNEL_HIGH_PRIMARY:
			sap_update_rssi_bsscount(spect_ch, -1, false,
						 spectch_start, spectch_end);
			return;
		}
		return;
	case eHT_CHANNEL_WIDTH_80MHZ:
	case eHT_CHANNEL_WIDTH_80P80MHZ:
		num_ch = 3;
		if ((ch_freq0 - op_chan_freq) == 30) {
			offset = 1;
		} else if ((ch_freq0 - op_chan_freq) == 10) {
			offset = -1;
		} else if ((ch_freq0 - op_chan_freq) == -10) {
			offset = -2;
		} else if ((ch_freq0 - op_chan_freq) == -30) {
			offset = -3;
		}
		break;
	case eHT_CHANNEL_WIDTH_160MHZ:
		num_ch = 7;
		if ((ch_freq0 - op_chan_freq) == 70)
			offset = 1;
		else if ((ch_freq0 - op_chan_freq) == 50)
			offset = -1;
		else if ((ch_freq0 - op_chan_freq) == 30)
			offset = -2;
		else if ((ch_freq0 - op_chan_freq) == 10)
			offset = -3;
		else if ((ch_freq0 - op_chan_freq) == -10)
			offset = -4;
		else if ((ch_freq0 - op_chan_freq) == -30)
			offset = -5;
		else if ((ch_freq0 - op_chan_freq) == -50)
			offset = -6;
		else if ((ch_freq0 - op_chan_freq) == -70)
			offset = -7;
		break;
	default:
		return;
	}

	sap_update_rssi_bsscount_vht_5G(spect_ch, offset, num_ch, spectch_start,
					spectch_end);
}

/**
 * sap_interference_rssi_count() - sap_interference_rssi_count
 *                                 considers the Adjacent channel rssi
 *                                 and data count(here number of BSS observed)
 * @spect_ch    Channel Information
 * @spectch_start: the start of spect ch array
 * @spectch_end: the end of spect ch array
 *
 * sap_interference_rssi_count considers the Adjacent channel rssi
 * and data count(here number of BSS observed)
 *
 * Return: None.
 */

static void sap_interference_rssi_count(tSapSpectChInfo *spect_ch,
	tSapSpectChInfo *spectch_start,
	tSapSpectChInfo *spectch_end)
{
	if (!spect_ch) {
		sap_err("spect_ch is NULL");
		return;
	}

	switch (wlan_freq_to_chan(spect_ch->chan_freq)) {
	case CHANNEL_1:
		sap_update_rssi_bsscount(spect_ch, 1, true,
			spectch_start, spectch_end);
		sap_update_rssi_bsscount(spect_ch, 2, true,
			spectch_start, spectch_end);
		sap_update_rssi_bsscount(spect_ch, 3, true,
			spectch_start, spectch_end);
		sap_update_rssi_bsscount(spect_ch, 4, true,
			spectch_start, spectch_end);
		break;

	case CHANNEL_2:
		sap_update_rssi_bsscount(spect_ch, -1, true,
			spectch_start, spectch_end);
		sap_update_rssi_bsscount(spect_ch, 1, true,
			spectch_start, spectch_end);
		sap_update_rssi_bsscount(spect_ch, 2, true,
			spectch_start, spectch_end);
		sap_update_rssi_bsscount(spect_ch, 3, true,
			spectch_start, spectch_end);
		sap_update_rssi_bsscount(spect_ch, 4, true,
			spectch_start, spectch_end);
		break;
	case CHANNEL_3:
		sap_update_rssi_bsscount(spect_ch, -2, true,
			spectch_start, spectch_end);
		sap_update_rssi_bsscount(spect_ch, -1, true,
			spectch_start, spectch_end);
		sap_update_rssi_bsscount(spect_ch, 1, true,
			spectch_start, spectch_end);
		sap_update_rssi_bsscount(spect_ch, 2, true,
			spectch_start, spectch_end);
		sap_update_rssi_bsscount(spect_ch, 3, true,
			spectch_start, spectch_end);
		sap_update_rssi_bsscount(spect_ch, 4, true,
			spectch_start, spectch_end);
		break;
	case CHANNEL_4:
		sap_update_rssi_bsscount(spect_ch, -3, true,
			spectch_start, spectch_end);
		sap_update_rssi_bsscount(spect_ch, -2, true,
			spectch_start, spectch_end);
		sap_update_rssi_bsscount(spect_ch, -1, true,
			spectch_start, spectch_end);
		sap_update_rssi_bsscount(spect_ch, 1, true,
			spectch_start, spectch_end);
		sap_update_rssi_bsscount(spect_ch, 2, true,
			spectch_start, spectch_end);
		sap_update_rssi_bsscount(spect_ch, 3, true,
			spectch_start, spectch_end);
		sap_update_rssi_bsscount(spect_ch, 4, true,
			spectch_start, spectch_end);
		break;

	case CHANNEL_5:
	case CHANNEL_6:
	case CHANNEL_7:
	case CHANNEL_8:
	case CHANNEL_9:
	case CHANNEL_10:
		sap_update_rssi_bsscount(spect_ch, -4, true,
			spectch_start, spectch_end);
		sap_update_rssi_bsscount(spect_ch, -3, true,
			spectch_start, spectch_end);
		sap_update_rssi_bsscount(spect_ch, -2, true,
			spectch_start, spectch_end);
		sap_update_rssi_bsscount(spect_ch, -1, true,
			spectch_start, spectch_end);
		sap_update_rssi_bsscount(spect_ch, 1, true,
			spectch_start, spectch_end);
		sap_update_rssi_bsscount(spect_ch, 2, true,
			spectch_start, spectch_end);
		sap_update_rssi_bsscount(spect_ch, 3, true,
			spectch_start, spectch_end);
		sap_update_rssi_bsscount(spect_ch, 4, true,
			spectch_start, spectch_end);
		break;

	case CHANNEL_11:
		sap_update_rssi_bsscount(spect_ch, -4, true,
			spectch_start, spectch_end);
		sap_update_rssi_bsscount(spect_ch, -3, true,
			spectch_start, spectch_end);
		sap_update_rssi_bsscount(spect_ch, -2, true,
			spectch_start, spectch_end);
		sap_update_rssi_bsscount(spect_ch, -1, true,
			spectch_start, spectch_end);
		sap_update_rssi_bsscount(spect_ch, 1, true,
			spectch_start, spectch_end);
		sap_update_rssi_bsscount(spect_ch, 2, true,
			spectch_start, spectch_end);
		sap_update_rssi_bsscount(spect_ch, 3, true,
			spectch_start, spectch_end);
		break;

	case CHANNEL_12:
		sap_update_rssi_bsscount(spect_ch, -4, true,
			spectch_start, spectch_end);
		sap_update_rssi_bsscount(spect_ch, -3, true,
			spectch_start, spectch_end);
		sap_update_rssi_bsscount(spect_ch, -2, true,
			spectch_start, spectch_end);
		sap_update_rssi_bsscount(spect_ch, -1, true,
			spectch_start, spectch_end);
		sap_update_rssi_bsscount(spect_ch, 1, true,
			spectch_start, spectch_end);
		sap_update_rssi_bsscount(spect_ch, 2, true,
			spectch_start, spectch_end);
		break;

	case CHANNEL_13:
		sap_update_rssi_bsscount(spect_ch, -4, true,
			spectch_start, spectch_end);
		sap_update_rssi_bsscount(spect_ch, -3, true,
			spectch_start, spectch_end);
		sap_update_rssi_bsscount(spect_ch, -2, true,
			spectch_start, spectch_end);
		sap_update_rssi_bsscount(spect_ch, -1, true,
			spectch_start, spectch_end);
		sap_update_rssi_bsscount(spect_ch, 1, true,
			spectch_start, spectch_end);
		break;

	case CHANNEL_14:
		sap_update_rssi_bsscount(spect_ch, -4, true,
			spectch_start, spectch_end);
		sap_update_rssi_bsscount(spect_ch, -3, true,
			spectch_start, spectch_end);
		sap_update_rssi_bsscount(spect_ch, -2, true,
			spectch_start, spectch_end);
		sap_update_rssi_bsscount(spect_ch, -1, true,
			spectch_start, spectch_end);
		break;

	default:
		break;
	}
}

/**
 * ch_in_pcl() - Is channel in the Preferred Channel List (PCL)
 * @sap_ctx: SAP context which contains the current PCL
 * @channel: Input channel number to be checked
 *
 * Check if a channel is in the preferred channel list
 *
 * Return:
 *   true:    channel is in PCL,
 *   false:   channel is not in PCL
 */
static bool ch_in_pcl(struct sap_context *sap_ctx, uint32_t ch_freq)
{
	uint32_t i;

	for (i = 0; i < sap_ctx->acs_cfg->pcl_ch_count; i++) {
		if (ch_freq == sap_ctx->acs_cfg->pcl_chan_freq[i])
			return true;
	}

	return false;
}

/**
 * sap_upd_chan_spec_params() - sap_upd_chan_spec_params
 *  updates channel parameters obtained from Beacon
 * @scan_entry: Beacon strucutre populated by scan
 * @ch_width: Channel width
 * @sec_ch_offset: Secondary Channel Offset
 * @center_freq0: Central frequency 0 for the given channel
 * @center_freq1: Central frequency 1 for the given channel
 *
 * sap_upd_chan_spec_params updates the spectrum channels based on the
 * scan_entry
 *
 * Return: NA.
 */
static void
sap_upd_chan_spec_params(struct scan_cache_node *scan_entry,
			 tSirMacHTChannelWidth *ch_width,
			 uint16_t *sec_ch_offset,
			 uint32_t *center_freq0,
			 uint32_t *center_freq1)
{
	enum wlan_phymode phy_mode;
	struct channel_info *chan;

	phy_mode = util_scan_entry_phymode(scan_entry->entry);
	chan = util_scan_entry_channel(scan_entry->entry);

	if (IS_WLAN_PHYMODE_160MHZ(phy_mode)) {
		if (phy_mode == WLAN_PHYMODE_11AC_VHT80_80 ||
		    phy_mode == WLAN_PHYMODE_11AXA_HE80_80) {
			*ch_width = eHT_CHANNEL_WIDTH_80P80MHZ;
			*center_freq0 = chan->cfreq0;
			*center_freq1 = chan->cfreq1;
		} else {
			*ch_width = eHT_CHANNEL_WIDTH_160MHZ;
			if (chan->cfreq1)
				*center_freq0 = chan->cfreq1;
			else
				*center_freq0 = chan->cfreq0;
		}

	} else if (IS_WLAN_PHYMODE_80MHZ(phy_mode)) {
		*ch_width = eHT_CHANNEL_WIDTH_80MHZ;
		*center_freq0 = chan->cfreq0;
	} else if (IS_WLAN_PHYMODE_40MHZ(phy_mode)) {
		if (chan->cfreq0 > chan->chan_freq)
			*sec_ch_offset = PHY_DOUBLE_CHANNEL_LOW_PRIMARY;
		else
			*sec_ch_offset = PHY_DOUBLE_CHANNEL_HIGH_PRIMARY;
		*ch_width = eHT_CHANNEL_WIDTH_40MHZ;
		*center_freq0 = chan->cfreq0;
	} else {
		*ch_width = eHT_CHANNEL_WIDTH_20MHZ;
	}
}

/**
 * sap_compute_spect_weight() - Compute spectrum weight
 * @pSpectInfoParams: Pointer to the tSpectInfoParams structure
 * @mac_handle: Opaque handle to the global MAC context
 * @pResult: Pointer to tScanResultHandle
 * @sap_ctx: Context of the SAP
 *
 * Main function for computing the weight of each channel in the
 * spectrum based on the RSSI value of the BSSes on the channel
 * and number of BSS
 */
static void sap_compute_spect_weight(tSapChSelSpectInfo *pSpectInfoParams,
				     mac_handle_t mac_handle,
				     qdf_list_t *scan_list,
				     struct sap_context *sap_ctx)
{
	int8_t rssi = 0;
	uint8_t chn_num = 0;
	tSapSpectChInfo *pSpectCh = pSpectInfoParams->pSpectCh;
	tSirMacHTChannelWidth ch_width = 0;
	uint16_t secondaryChannelOffset;
	uint32_t center_freq0, center_freq1;
	uint8_t i;
	bool found;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);
	tSapSpectChInfo *spectch_start = pSpectInfoParams->pSpectCh;
	tSapSpectChInfo *spectch_end = pSpectInfoParams->pSpectCh +
		pSpectInfoParams->numSpectChans;
	qdf_list_node_t *cur_lst = NULL, *next_lst = NULL;
	struct scan_cache_node *cur_node = NULL;
	uint32_t normalized_weight;
	uint8_t normalize_factor;
	uint32_t chan_freq;
	struct acs_weight *weight_list =
				mac->mlme_cfg->acs.normalize_weight_chan;
	struct acs_weight_range *range_list =
				mac->mlme_cfg->acs.normalize_weight_range;
	bool freq_present_in_list = false;

	sap_debug("Computing spectral weight");

	if (scan_list)
		qdf_list_peek_front(scan_list, &cur_lst);
	while (cur_lst) {
		cur_node = qdf_container_of(cur_lst, struct scan_cache_node,
					    node);
		pSpectCh = pSpectInfoParams->pSpectCh;
		/* Defining the default values, so that any value will hold the default values */

		secondaryChannelOffset = PHY_SINGLE_CHANNEL_CENTERED;
		center_freq0 = 0;
		center_freq1 = 0;

		chan_freq =
		    util_scan_entry_channel_frequency(cur_node->entry);

		sap_upd_chan_spec_params(cur_node, &ch_width,
					 &secondaryChannelOffset,
					 &center_freq0, &center_freq1);

		/* Processing for each tCsrScanResultInfo in the tCsrScanResult DLink list */
		for (chn_num = 0; chn_num < pSpectInfoParams->numSpectChans;
		     chn_num++) {

			if (chan_freq != pSpectCh->chan_freq) {
				pSpectCh++;
				continue;
			}

			if (pSpectCh->rssiAgr < cur_node->entry->rssi_raw)
				pSpectCh->rssiAgr = cur_node->entry->rssi_raw;

			++pSpectCh->bssCount;

			if (WLAN_REG_IS_24GHZ_CH_FREQ(chan_freq))
				sap_interference_rssi_count(pSpectCh,
					spectch_start, spectch_end);
			else
				sap_interference_rssi_count_5G(
				    pSpectCh, ch_width, secondaryChannelOffset,
				    center_freq0, center_freq1, chan_freq,
				    spectch_start, spectch_end);

			pSpectCh++;
			break;

		}

		qdf_list_peek_next(scan_list, cur_lst, &next_lst);
		cur_lst = next_lst;
		next_lst = NULL;
	}

	/* Calculate the weights for all channels in the spectrum pSpectCh */
	pSpectCh = pSpectInfoParams->pSpectCh;

	for (chn_num = 0; chn_num < (pSpectInfoParams->numSpectChans);
	     chn_num++) {

		/*
		   rssi : Maximum received signal strength among all BSS on that channel
		   bssCount : Number of BSS on that channel
		 */

		rssi = (int8_t) pSpectCh->rssiAgr;
		if (ch_in_pcl(sap_ctx, pSpectCh->chan_freq))
			rssi -= PCL_RSSI_DISCOUNT;

		if (rssi < SOFTAP_MIN_RSSI)
			rssi = SOFTAP_MIN_RSSI;

		if (pSpectCh->weight == SAP_ACS_WEIGHT_MAX) {
			pSpectCh->weight_copy = pSpectCh->weight;
			goto debug_info;
		}

		/* There may be channels in scanlist, which were not sent to
		 * FW for scanning as part of ACS scan list, but they do have an
		 * effect on the neighbouring channels, so they help to find a
		 * suitable channel, but there weight should be max as they were
		 * and not meant to be included in the ACS scan results.
		 * So just assign RSSI as -100, bsscount as 0, and weight as max
		 * to them, so that they always stay low in sorting of best
		 * channles which were included in ACS scan list
		 */
		found = false;
		for (i = 0; i < sap_ctx->num_of_channel; i++) {
			if (pSpectCh->chan_freq == sap_ctx->freq_list[i]) {
			/* Scan channel was included in ACS scan list */
				found = true;
				break;
			}
		}

		if (found)
			pSpectCh->weight =
				SAPDFS_NORMALISE_1000 *
				(sapweight_rssi_count(sap_ctx, rssi,
				pSpectCh->bssCount) + sap_weight_channel_status(
				sap_ctx, sap_get_channel_status(mac,
							 pSpectCh->chan_freq)));
		else {
			pSpectCh->weight = SAP_ACS_WEIGHT_MAX;
			pSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
			rssi = SOFTAP_MIN_RSSI;
			pSpectCh->bssCount = SOFTAP_MIN_COUNT;
		}

		chan_freq = pSpectCh->chan_freq;

		if (wlan_reg_is_dfs_for_freq(mac->pdev, chan_freq)) {
			normalize_factor =
				MLME_GET_DFS_CHAN_WEIGHT(
				mac->mlme_cfg->acs.np_chan_weightage);
			freq_present_in_list = true;
			sap_debug_rl("DFS channel weightage %d",
				     normalize_factor);
		}

		/* Check if the freq is present in range list */
		for (i = 0; i < mac->mlme_cfg->acs.num_weight_range; i++) {
			if (chan_freq >= range_list[i].start_freq &&
			    chan_freq <= range_list[i].end_freq) {
				normalize_factor =
					range_list[i].normalize_weight;
				sap_debug("Range list, freq %d normalize weight factor %d",
					  chan_freq, normalize_factor);
				freq_present_in_list = true;
			}
		}

		/* Check if user wants a special factor for this freq */

		for (i = 0; i < mac->mlme_cfg->acs.normalize_weight_num_chan;
		     i++) {
			if (chan_freq == weight_list[i].chan_freq) {
				normalize_factor =
					weight_list[i].normalize_weight;
				sap_debug("freq %d normalize weight factor %d",
					  chan_freq, normalize_factor);
				freq_present_in_list = true;
			}
		}

		if (freq_present_in_list) {
			normalized_weight =
				((SAP_ACS_WEIGHT_MAX - pSpectCh->weight) *
				(100 - normalize_factor)) / 100;
			sap_debug_rl("freq %d old weight %d new weight %d",
				     chan_freq, pSpectCh->weight,
				     pSpectCh->weight + normalized_weight);
			pSpectCh->weight += normalized_weight;
			freq_present_in_list = false;
		}

		if (pSpectCh->weight > SAP_ACS_WEIGHT_MAX)
			pSpectCh->weight = SAP_ACS_WEIGHT_MAX;
		pSpectCh->weight_copy = pSpectCh->weight;

debug_info:
		sap_debug_rl("freq = %d, weight = %d rssi = %d bss count = %d",
			     pSpectCh->chan_freq, pSpectCh->weight,
			     pSpectCh->rssiAgr, pSpectCh->bssCount);

		pSpectCh++;
	}
	sap_clear_channel_status(mac);
}

/*==========================================================================
   FUNCTION    sap_chan_sel_exit

   DESCRIPTION
    Exit function for free out the allocated memory, to be called
    at the end of the dfsSelectChannel function

   DEPENDENCIES
    NA.

   PARAMETERS

    IN
    pSpectInfoParams       : Pointer to the tSapChSelSpectInfo structure

   RETURN VALUE
    void     : NULL

   SIDE EFFECTS
   ============================================================================*/
static void sap_chan_sel_exit(tSapChSelSpectInfo *pSpectInfoParams)
{
	/* Free all the allocated memory */
	qdf_mem_free(pSpectInfoParams->pSpectCh);
}

/*==========================================================================
   FUNCTION    sap_sort_chl_weight

   DESCRIPTION
    Function to sort the channels with the least weight first for 20MHz channels

   DEPENDENCIES
    NA.

   PARAMETERS

    IN
    pSpectInfoParams       : Pointer to the tSapChSelSpectInfo structure

   RETURN VALUE
    void     : NULL

   SIDE EFFECTS
   ============================================================================*/
static void sap_sort_chl_weight(tSapChSelSpectInfo *pSpectInfoParams)
{
	tSapSpectChInfo temp;

	tSapSpectChInfo *pSpectCh = NULL;
	uint32_t i = 0, j = 0, minWeightIndex = 0;

	pSpectCh = pSpectInfoParams->pSpectCh;
	for (i = 0; i < pSpectInfoParams->numSpectChans; i++) {
		minWeightIndex = i;
		for (j = i + 1; j < pSpectInfoParams->numSpectChans; j++) {
			if (pSpectCh[j].weight <
			    pSpectCh[minWeightIndex].weight) {
				minWeightIndex = j;
			} else if (pSpectCh[j].weight ==
				   pSpectCh[minWeightIndex].weight) {
				if (pSpectCh[j].bssCount <
				    pSpectCh[minWeightIndex].bssCount)
					minWeightIndex = j;
			}
		}
		if (minWeightIndex != i) {
			qdf_mem_copy(&temp, &pSpectCh[minWeightIndex],
				     sizeof(*pSpectCh));
			qdf_mem_copy(&pSpectCh[minWeightIndex], &pSpectCh[i],
				     sizeof(*pSpectCh));
			qdf_mem_copy(&pSpectCh[i], &temp, sizeof(*pSpectCh));
		}
	}
}

/**
 * sap_sort_chl_weight_80_mhz() - to sort the channels with the least weight
 * @pSpectInfoParams: Pointer to the tSapChSelSpectInfo structure
 * Function to sort the channels with the least weight first for HT80 channels
 *
 * Return: none
 */
static void sap_sort_chl_weight_80_mhz(struct mac_context *mac_ctx,
				       tSapChSelSpectInfo *pSpectInfoParams)
{
	uint8_t i, j;
	tSapSpectChInfo *pSpectInfo;
	uint8_t minIdx;
	struct ch_params acs_ch_params;
	int8_t center_freq_diff;
	uint32_t combined_weight;
	uint32_t min_ch_weight;

	pSpectInfo = pSpectInfoParams->pSpectCh;

	for (j = 0; j < pSpectInfoParams->numSpectChans; j++) {

		if (pSpectInfo[j].weight_calc_done)
			continue;

		acs_ch_params.ch_width = CH_WIDTH_80MHZ;

		wlan_reg_set_channel_params_for_freq(mac_ctx->pdev,
						     pSpectInfo[j].chan_freq,
						     0, &acs_ch_params);

		/* Check if the freq supports 80 Mhz */
		if (acs_ch_params.ch_width != CH_WIDTH_80MHZ) {
			pSpectInfo[j].weight = SAP_ACS_WEIGHT_MAX * 4;
			pSpectInfo[j].weight_calc_done = true;
			continue;
		}

		center_freq_diff = acs_ch_params.mhz_freq_seg0 -
				   pSpectInfo[j].chan_freq;

		/* This channel frequency does not have all channels */
		if (center_freq_diff != 30) {
			pSpectInfo[j].weight = SAP_ACS_WEIGHT_MAX * 4;
			pSpectInfo[j].weight_calc_done = true;
			continue;
		}

		/* no other freq left for 80 Mhz operation in spectrum */
		if (j + 3 > pSpectInfoParams->numSpectChans)
			continue;

		/* Check whether all frequencies are present for 80 Mhz */

		if (!(((pSpectInfo[j].chan_freq + 20) ==
			pSpectInfo[j + 1].chan_freq) &&
			((pSpectInfo[j].chan_freq + 40) ==
				 pSpectInfo[j + 2].chan_freq) &&
			((pSpectInfo[j].chan_freq + 60) ==
				 pSpectInfo[j + 3].chan_freq))) {
			/*
			 * some channels does not exist in pSectInfo array,
			 * skip this channel and those in the same HT80 width
			 */
			pSpectInfo[j].weight = SAP_ACS_WEIGHT_MAX * 4;
			pSpectInfo[j].weight_calc_done = true;
			if ((pSpectInfo[j].chan_freq + 20) ==
					pSpectInfo[j + 1].chan_freq) {
				pSpectInfo[j + 1].weight =
					SAP_ACS_WEIGHT_MAX * 4;
				pSpectInfo[j +1].weight_calc_done = true;
			}
			if ((pSpectInfo[j].chan_freq + 40) ==
					pSpectInfo[j + 2].chan_freq) {
				pSpectInfo[j + 2].weight =
					SAP_ACS_WEIGHT_MAX * 4;
				pSpectInfo[j +2].weight_calc_done = true;
			}
			if ((pSpectInfo[j].chan_freq + 60) ==
					pSpectInfo[j + 3].chan_freq) {
				pSpectInfo[j + 3].weight =
					SAP_ACS_WEIGHT_MAX * 4;
				pSpectInfo[j +3].weight_calc_done = true;
			}

			continue;
		}

		/* We have 4 channels to calculate cumulative weight */

		combined_weight = pSpectInfo[j].weight +
				  pSpectInfo[j + 1].weight +
				  pSpectInfo[j + 2].weight +
				  pSpectInfo[j + 3].weight;

		min_ch_weight = pSpectInfo[j].weight;
		minIdx = 0;

		for (i = 0; i < 4; i++) {
			if (min_ch_weight > pSpectInfo[j + i].weight) {
				min_ch_weight = pSpectInfo[j + i].weight;
				minIdx = i;
			}
			pSpectInfo[j + i].weight = SAP_ACS_WEIGHT_MAX * 4;
			pSpectInfo[j + i].weight_calc_done = true;
		}

		pSpectInfo[j + minIdx].weight = combined_weight;

		sap_debug("best freq = %d for 80mhz center freq %d combined weight = %d",
			  pSpectInfo[j + minIdx].chan_freq,
			  acs_ch_params.mhz_freq_seg0,
			  combined_weight);
	}

	sap_sort_chl_weight(pSpectInfoParams);

	pSpectInfo = pSpectInfoParams->pSpectCh;

	for (j = 0; j < (pSpectInfoParams->numSpectChans); j++) {
		sap_debug_rl("freq = %d weight = %d rssi = %d bss count = %d",
			     pSpectInfo->chan_freq, pSpectInfo->weight,
			     pSpectInfo->rssiAgr, pSpectInfo->bssCount);

		pSpectInfo++;
	}
}

/**
 * sap_sort_chl_weight_vht160() - to sort the channels with the least weight
 * @pSpectInfoParams: Pointer to the tSapChSelSpectInfo structure
 *
 * Function to sort the channels with the least weight first for VHT160 channels
 *
 * Return: none
 */
static void sap_sort_chl_weight_160_mhz(struct mac_context *mac_ctx,
					tSapChSelSpectInfo *pSpectInfoParams)
{
	uint8_t i, j;
	tSapSpectChInfo *pSpectInfo;
	uint8_t minIdx;
	struct ch_params acs_ch_params;
	int8_t center_freq_diff;
	uint32_t combined_weight;
	uint32_t min_ch_weight;

	pSpectInfo = pSpectInfoParams->pSpectCh;

	for (j = 0; j < pSpectInfoParams->numSpectChans; j++) {

		if (pSpectInfo[j].weight_calc_done)
			continue;

		acs_ch_params.ch_width = CH_WIDTH_160MHZ;

		wlan_reg_set_channel_params_for_freq(mac_ctx->pdev,
						     pSpectInfo[j].chan_freq,
						     0, &acs_ch_params);

		/* Check if the freq supports 160 Mhz */
		if (acs_ch_params.ch_width != CH_WIDTH_160MHZ) {
			pSpectInfo[j].weight = SAP_ACS_WEIGHT_MAX * 8;
			pSpectInfo[j].weight_calc_done = true;
			continue;
		}

		center_freq_diff = acs_ch_params.mhz_freq_seg1 -
				   pSpectInfo[j].chan_freq;

		/* This channel frequency does not have all channels */
		if (center_freq_diff != 70) {
			pSpectInfo[j].weight = SAP_ACS_WEIGHT_MAX * 8;
			pSpectInfo[j].weight_calc_done = true;
			continue;
		}

		/* no other freq left for 160 Mhz operation in spectrum */
		if (j + 7 > pSpectInfoParams->numSpectChans)
			continue;

		/* Check whether all frequencies are present for 160 Mhz */

		if (!(((pSpectInfo[j].chan_freq + 20) ==
			pSpectInfo[j + 1].chan_freq) &&
			((pSpectInfo[j].chan_freq + 40) ==
				 pSpectInfo[j + 2].chan_freq) &&
			((pSpectInfo[j].chan_freq + 60) ==
				 pSpectInfo[j + 3].chan_freq) &&
			((pSpectInfo[j].chan_freq + 80) ==
				 pSpectInfo[j + 4].chan_freq) &&
			((pSpectInfo[j].chan_freq + 100) ==
				 pSpectInfo[j + 5].chan_freq) &&
			((pSpectInfo[j].chan_freq + 120) ==
				 pSpectInfo[j + 6].chan_freq) &&
			((pSpectInfo[j].chan_freq + 140) ==
				 pSpectInfo[j + 7].chan_freq))) {
			/*
			 * some channels does not exist in pSectInfo array,
			 * skip this channel and those in the same VHT160 width
			 */
			pSpectInfo[j].weight = SAP_ACS_WEIGHT_MAX * 8;
			pSpectInfo[j].weight_calc_done = true;
			if ((pSpectInfo[j].chan_freq + 20) ==
					pSpectInfo[j + 1].chan_freq) {
				pSpectInfo[j + 1].weight =
					SAP_ACS_WEIGHT_MAX * 8;
				pSpectInfo[j + 1].weight_calc_done = true;
			}
			if ((pSpectInfo[j].chan_freq + 40) ==
					pSpectInfo[j + 2].chan_freq) {
				pSpectInfo[j + 2].weight =
					SAP_ACS_WEIGHT_MAX * 8;
				pSpectInfo[j + 2].weight_calc_done = true;
			}
			if ((pSpectInfo[j].chan_freq + 60) ==
					pSpectInfo[j + 3].chan_freq) {
				pSpectInfo[j + 3].weight =
					SAP_ACS_WEIGHT_MAX * 8;
				pSpectInfo[j + 3].weight_calc_done = true;
			}
			if ((pSpectInfo[j].chan_freq + 80) ==
					pSpectInfo[j + 4].chan_freq) {
				pSpectInfo[j + 4].weight =
					SAP_ACS_WEIGHT_MAX * 8;
				pSpectInfo[j + 4].weight_calc_done = true;
			}
			if ((pSpectInfo[j].chan_freq + 100) ==
					pSpectInfo[j + 5].chan_freq) {
				pSpectInfo[j + 5].weight =
					SAP_ACS_WEIGHT_MAX * 8;
				pSpectInfo[j + 5].weight_calc_done = true;
			}
			if ((pSpectInfo[j].chan_freq + 120) ==
					pSpectInfo[j + 6].chan_freq) {
				pSpectInfo[j + 6].weight =
					SAP_ACS_WEIGHT_MAX * 8;
				pSpectInfo[j + 6].weight_calc_done = true;
			}
			if ((pSpectInfo[j].chan_freq + 140) ==
					pSpectInfo[j + 7].chan_freq) {
				pSpectInfo[j + 7].weight =
					SAP_ACS_WEIGHT_MAX * 8;
				pSpectInfo[j + 7].weight_calc_done = true;
			}

			continue;
		}


		/* We have 8 channels to calculate cumulative weight */

		combined_weight = pSpectInfo[j].weight +
				  pSpectInfo[j + 1].weight +
				  pSpectInfo[j + 2].weight +
				  pSpectInfo[j + 3].weight +
				  pSpectInfo[j + 4].weight +
				  pSpectInfo[j + 5].weight +
				  pSpectInfo[j + 6].weight +
				  pSpectInfo[j + 7].weight;

		min_ch_weight = pSpectInfo[j].weight;
		minIdx = 0;

		for (i = 0; i < 8; i++) {
			if (min_ch_weight > pSpectInfo[j + i].weight) {
				min_ch_weight = pSpectInfo[j + i].weight;
				minIdx = i;
			}
			pSpectInfo[j + i].weight = SAP_ACS_WEIGHT_MAX * 8;
			pSpectInfo[j + i].weight_calc_done = true;
		}

		pSpectInfo[j + minIdx].weight = combined_weight;

		sap_debug("best freq = %d for 160mhz center freq %d combined weight = %d",
			  pSpectInfo[j + minIdx].chan_freq,
			  acs_ch_params.mhz_freq_seg1,
			  combined_weight);
	}

	sap_sort_chl_weight(pSpectInfoParams);

	pSpectInfo = pSpectInfoParams->pSpectCh;
	for (j = 0; j < (pSpectInfoParams->numSpectChans); j++) {
		sap_debug_rl("freq = %d weight = %d rssi = %d bss count = %d",
			     pSpectInfo->chan_freq, pSpectInfo->weight,
			     pSpectInfo->rssiAgr, pSpectInfo->bssCount);

		pSpectInfo++;
	}
}

/**
 * sap_allocate_max_weight_ht40_24_g() - allocate max weight for 40Mhz
 *                                       to all 2.4Ghz channels
 * @spect_info_params: Pointer to the tSapChSelSpectInfo structure
 *
 * Return: none
 */
static void
sap_allocate_max_weight_40_mhz_24_g(tSapChSelSpectInfo *spect_info_params)
{
	tSapSpectChInfo *spect_info;
	uint8_t j;

	/*
	 * Assign max weight for 40Mhz (SAP_ACS_WEIGHT_MAX * 2) to all
	 * 2.4 Ghz channels
	 */
	spect_info = spect_info_params->pSpectCh;
	for (j = 0; j < spect_info_params->numSpectChans; j++) {
		if (WLAN_REG_IS_24GHZ_CH_FREQ(spect_info[j].chan_freq))
			spect_info[j].weight = SAP_ACS_WEIGHT_MAX * 2;
	}
}

/**
 * sap_allocate_max_weight_ht40_5_g() - allocate max weight for 40Mhz
 *                                      to all 5Ghz channels
 * @spect_info_params: Pointer to the tSapChSelSpectInfo structure
 *
 * Return: none
 */
static void
sap_allocate_max_weight_40_mhz(tSapChSelSpectInfo *spect_info_params)
{
	tSapSpectChInfo *spect_info;
	uint8_t j;

	/*
	 * Assign max weight for 40Mhz (SAP_ACS_WEIGHT_MAX * 2) to all
	 * 5 Ghz channels
	 */
	spect_info = spect_info_params->pSpectCh;
	for (j = 0; j < spect_info_params->numSpectChans; j++) {
		if (WLAN_REG_IS_5GHZ_CH_FREQ(spect_info[j].chan_freq) ||
		    WLAN_REG_IS_6GHZ_CHAN_FREQ(spect_info[j].chan_freq))
			spect_info[j].weight = SAP_ACS_WEIGHT_MAX * 2;
	}
}

/**
 * sap_sort_chl_weight_ht40_24_g() - to sort channel with the least weight
 * @pSpectInfoParams: Pointer to the tSapChSelSpectInfo structure
 *
 * Function to sort the channels with the least weight first for HT40 channels
 *
 * Return: none
 */
static void sap_sort_chl_weight_ht40_24_g(struct mac_context *mac_ctx,
					  tSapChSelSpectInfo *pSpectInfoParams,
					  v_REGDOMAIN_t domain)
{
	uint8_t i, j;
	tSapSpectChInfo *pSpectInfo;
	uint32_t tmpWeight1, tmpWeight2;
	uint32_t ht40plus2gendch = 0;
	uint32_t channel;
	uint32_t chan_freq;

	pSpectInfo = pSpectInfoParams->pSpectCh;
	/*
	 * for each HT40 channel, calculate the combined weight of the
	 * two 20MHz weight
	 */
	for (i = 0; i < ARRAY_SIZE(acs_ht40_channels24_g); i++) {
		for (j = 0; j < pSpectInfoParams->numSpectChans; j++) {
			channel = wlan_freq_to_chan(pSpectInfo[j].chan_freq);
			if (channel == acs_ht40_channels24_g[i].chStartNum)
				break;
		}
		if (j == pSpectInfoParams->numSpectChans)
			continue;

		if (!((pSpectInfo[j].chan_freq + 20) ==
		       pSpectInfo[j + 4].chan_freq)) {
			pSpectInfo[j].weight = SAP_ACS_WEIGHT_MAX * 2;
			continue;
		}
		/*
		 * check if there is another channel combination possiblity
		 * e.g., {1, 5} & {5, 9}
		 */
		if ((pSpectInfo[j + 4].chan_freq + 20) ==
		     pSpectInfo[j + 8].chan_freq) {
			/* need to compare two channel pairs */
			tmpWeight1 = pSpectInfo[j].weight +
						pSpectInfo[j + 4].weight;
			tmpWeight2 = pSpectInfo[j + 4].weight +
						pSpectInfo[j + 8].weight;
			if (tmpWeight1 <= tmpWeight2) {
				if (pSpectInfo[j].weight <=
						pSpectInfo[j + 4].weight) {
					pSpectInfo[j].weight =
						tmpWeight1;
					pSpectInfo[j + 4].weight =
						SAP_ACS_WEIGHT_MAX * 2;
					pSpectInfo[j + 8].weight =
						SAP_ACS_WEIGHT_MAX * 2;
				} else {
					pSpectInfo[j + 4].weight =
						tmpWeight1;
					/* for secondary channel selection */
					pSpectInfo[j].weight =
						SAP_ACS_WEIGHT_MAX * 2
						- 1;
					pSpectInfo[j + 8].weight =
						SAP_ACS_WEIGHT_MAX * 2;
				}
			} else {
				if (pSpectInfo[j + 4].weight <=
						pSpectInfo[j + 8].weight) {
					pSpectInfo[j + 4].weight =
						tmpWeight2;
					pSpectInfo[j].weight =
						SAP_ACS_WEIGHT_MAX * 2;
					/* for secondary channel selection */
					pSpectInfo[j + 8].weight =
						SAP_ACS_WEIGHT_MAX * 2
						- 1;
				} else {
					pSpectInfo[j + 8].weight =
						tmpWeight2;
					pSpectInfo[j].weight =
						SAP_ACS_WEIGHT_MAX * 2;
					pSpectInfo[j + 4].weight =
						SAP_ACS_WEIGHT_MAX * 2;
				}
			}
		} else {
			tmpWeight1 = pSpectInfo[j].weight_copy +
						pSpectInfo[j + 4].weight_copy;
			if (pSpectInfo[j].weight_copy <=
					pSpectInfo[j + 4].weight_copy) {
				pSpectInfo[j].weight = tmpWeight1;
				pSpectInfo[j + 4].weight =
					SAP_ACS_WEIGHT_MAX * 2;
			} else {
				pSpectInfo[j + 4].weight = tmpWeight1;
				pSpectInfo[j].weight =
					SAP_ACS_WEIGHT_MAX * 2;
			}
		}
	}
	/*
	 * Every channel should be checked. Add the check for the omissive
	 * channel. Mark the channel whose combination can't satisfy 40MHZ
	 * as max value, so that it will be sorted to the bottom.
	 */
	if (REGDOMAIN_FCC == domain)
		ht40plus2gendch = HT40PLUS_2G_FCC_CH_END;
	else
		ht40plus2gendch = HT40PLUS_2G_EURJAP_CH_END;
	for (i = HT40MINUS_2G_CH_START; i <= ht40plus2gendch; i++) {
		chan_freq = wlan_reg_legacy_chan_to_freq(mac_ctx->pdev, i);
		for (j = 0; j < pSpectInfoParams->numSpectChans; j++) {
			if (pSpectInfo[j].chan_freq == chan_freq &&
				((pSpectInfo[j].chan_freq + 20) !=
					pSpectInfo[j + 4].chan_freq) &&
				((pSpectInfo[j].chan_freq - 20) !=
					pSpectInfo[j - 4].chan_freq))
				pSpectInfo[j].weight = SAP_ACS_WEIGHT_MAX * 2;
		}
	}
	for (i = ht40plus2gendch + 1; i <= HT40MINUS_2G_CH_END; i++) {
		chan_freq = wlan_reg_legacy_chan_to_freq(mac_ctx->pdev, i);
		for (j = 0; j < pSpectInfoParams->numSpectChans; j++) {
			if (pSpectInfo[j].chan_freq == chan_freq &&
				(pSpectInfo[j].chan_freq - 20) !=
					pSpectInfo[j - 4].chan_freq)
				pSpectInfo[j].weight = SAP_ACS_WEIGHT_MAX * 2;
		}
	}

	pSpectInfo = pSpectInfoParams->pSpectCh;
	for (j = 0; j < (pSpectInfoParams->numSpectChans); j++) {
		sap_debug_rl("freq = %d weight = %d rssi = %d bss count = %d",
			     pSpectInfo->chan_freq, pSpectInfo->weight,
			     pSpectInfo->rssiAgr, pSpectInfo->bssCount);

		pSpectInfo++;
	}

	sap_sort_chl_weight(pSpectInfoParams);
}

static void sap_sort_chl_weight_40_mhz(struct mac_context *mac_ctx,
				       tSapChSelSpectInfo *pSpectInfoParams)
{
	uint8_t i, j;
	tSapSpectChInfo *pSpectInfo;
	uint8_t minIdx;
	struct ch_params acs_ch_params;
	int8_t center_freq_diff;
	uint32_t combined_weight;
	uint32_t min_ch_weight;

	pSpectInfo = pSpectInfoParams->pSpectCh;

	for (j = 0; j < pSpectInfoParams->numSpectChans; j++) {

		if (WLAN_REG_IS_24GHZ_CH_FREQ(pSpectInfo[j].chan_freq))
			continue;

		if (pSpectInfo[j].weight_calc_done)
			continue;

		acs_ch_params.ch_width = CH_WIDTH_40MHZ;

		wlan_reg_set_channel_params_for_freq(mac_ctx->pdev,
						     pSpectInfo[j].chan_freq,
						     0, &acs_ch_params);

		/* Check if the freq supports 40 Mhz */
		if (acs_ch_params.ch_width != CH_WIDTH_40MHZ) {
			pSpectInfo[j].weight = SAP_ACS_WEIGHT_MAX * 2;
			pSpectInfo[j].weight_calc_done = true;
			continue;
		}

		center_freq_diff = acs_ch_params.mhz_freq_seg0 -
				   pSpectInfo[j].chan_freq;

		/* This channel frequency does not have all channels */
		if (center_freq_diff != 10) {
			pSpectInfo[j].weight = SAP_ACS_WEIGHT_MAX * 2;
			pSpectInfo[j].weight_calc_done = true;
			continue;
		}

		/* no other freq left for 40 Mhz operation in spectrum */
		if (j + 1 > pSpectInfoParams->numSpectChans)
			continue;

		/* Check whether all frequencies are present for 40 Mhz */

		if (!((pSpectInfo[j].chan_freq + 20) ==
		       pSpectInfo[j + 1].chan_freq)) {
			/*
			 * some channels does not exist in pSectInfo array,
			 * skip this channel and those in the same 40 width
			 */
			pSpectInfo[j].weight = SAP_ACS_WEIGHT_MAX * 2;
			pSpectInfo[j].weight_calc_done = true;

			if ((pSpectInfo[j].chan_freq + 20) ==
					pSpectInfo[j + 1].chan_freq) {
				pSpectInfo[j + 1].weight =
					SAP_ACS_WEIGHT_MAX * 2;
				pSpectInfo[j +1].weight_calc_done = true;
			}

			continue;
		}

		/* We have 2 channels to calculate cumulative weight */

		combined_weight = pSpectInfo[j].weight +
				  pSpectInfo[j + 1].weight;

		min_ch_weight = pSpectInfo[j].weight;
		minIdx = 0;

		for (i = 0; i < 2; i++) {
			if (min_ch_weight > pSpectInfo[j + i].weight) {
				min_ch_weight = pSpectInfo[j + i].weight;
				minIdx = i;
			}
			pSpectInfo[j + i].weight = SAP_ACS_WEIGHT_MAX * 2;
			pSpectInfo[j + i].weight_calc_done = true;
		}

		pSpectInfo[j + minIdx].weight = combined_weight;

		sap_debug("best freq = %d for 40mhz center freq %d combined weight = %d",
			  pSpectInfo[j + minIdx].chan_freq,
			  acs_ch_params.mhz_freq_seg0,
			  combined_weight);
	}

	sap_sort_chl_weight(pSpectInfoParams);

	pSpectInfo = pSpectInfoParams->pSpectCh;
	for (j = 0; j < (pSpectInfoParams->numSpectChans); j++) {
		sap_debug_rl("freq = %d weight = %d rssi = %d bss count = %d",
			     pSpectInfo->chan_freq, pSpectInfo->weight,
			     pSpectInfo->rssiAgr, pSpectInfo->bssCount);

		pSpectInfo++;
	}
}

/*==========================================================================
   FUNCTION    sap_sort_chl_weight_all

   DESCRIPTION
    Function to sort the channels with the least weight first

   DEPENDENCIES
    NA.

   PARAMETERS

    IN
    sap_ctx                : Pointer to the struct sap_context *structure
    pSpectInfoParams       : Pointer to the tSapChSelSpectInfo structure

   RETURN VALUE
    void     : NULL

   SIDE EFFECTS
   ============================================================================*/
static void sap_sort_chl_weight_all(struct mac_context *mac_ctx,
				    struct sap_context *sap_ctx,
				    tSapChSelSpectInfo *pSpectInfoParams,
				    uint32_t operatingBand,
				    v_REGDOMAIN_t domain)
{
	tSapSpectChInfo *pSpectCh = NULL;
	uint32_t j = 0;

	pSpectCh = pSpectInfoParams->pSpectCh;

	switch (sap_ctx->acs_cfg->ch_width) {
	case CH_WIDTH_40MHZ:
		/*
		 * Assign max weight to all 5Ghz channels when operating band
		 * is 11g and to all 2.4Ghz channels when operating band is 11a
		 * or 11abg to avoid selection in ACS algorithm for starting SAP
		 */
		if (eCSR_DOT11_MODE_11g == operatingBand) {
			sap_allocate_max_weight_40_mhz(pSpectInfoParams);
			sap_sort_chl_weight_ht40_24_g(mac_ctx,
						      pSpectInfoParams,
						      domain);
		} else {
			sap_allocate_max_weight_40_mhz_24_g(pSpectInfoParams);
			sap_sort_chl_weight_40_mhz(mac_ctx, pSpectInfoParams);
		}
		break;
	case CH_WIDTH_80MHZ:
	case CH_WIDTH_80P80MHZ:
		sap_sort_chl_weight_80_mhz(mac_ctx, pSpectInfoParams);
		break;
	case CH_WIDTH_160MHZ:
		sap_sort_chl_weight_160_mhz(mac_ctx, pSpectInfoParams);
		break;
	case CH_WIDTH_20MHZ:
	default:
		/* Sorting the channels as per weights as 20MHz channels */
		sap_sort_chl_weight(pSpectInfoParams);
	}

	pSpectCh = pSpectInfoParams->pSpectCh;
	for (j = 0; j < (pSpectInfoParams->numSpectChans); j++) {
		sap_debug_rl("Freq = %d weight = %d rssi aggr = %d bss count = %d",
			     pSpectCh->chan_freq, pSpectCh->weight,
			     pSpectCh->rssiAgr, pSpectCh->bssCount);
		pSpectCh++;
	}

}

/**
 * sap_is_ch_non_overlap() - returns true if non-overlapping channel
 * @sap_ctx: Sap context
 * @ch: channel number
 *
 * Returns: true if non-overlapping (1, 6, 11) channel, false otherwise
 */
static bool sap_is_ch_non_overlap(struct sap_context *sap_ctx, uint16_t ch)
{
	if (sap_ctx->enableOverLapCh)
		return true;

	if ((ch == CHANNEL_1) || (ch == CHANNEL_6) || (ch == CHANNEL_11))
		return true;

	return false;
}

uint32_t sap_select_channel(mac_handle_t mac_handle,
			   struct sap_context *sap_ctx,
			   qdf_list_t *scan_list)
{
	/* DFS param object holding all the data req by the algo */
	tSapChSelSpectInfo spect_info_obj = { NULL, 0 };
	tSapChSelSpectInfo *spect_info = &spect_info_obj;
	uint8_t best_ch_num = SAP_CHANNEL_NOT_SELECTED;
	uint32_t best_ch_weight = SAP_ACS_WEIGHT_MAX;
	uint32_t ht40plus2gendch = 0;
	v_REGDOMAIN_t domain;
	uint8_t country[CDS_COUNTRY_CODE_LEN + 1];
	uint8_t count;
	uint32_t operating_band = 0;
	struct mac_context *mac_ctx;
	uint32_t best_chan_freq = 0;

	mac_ctx = MAC_CONTEXT(mac_handle);

	/* Initialize the structure pointed by spect_info */
	if (sap_chan_sel_init(mac_handle, spect_info, sap_ctx) != true) {
		sap_err("Ch Select initialization failed");
		return SAP_CHANNEL_NOT_SELECTED;
	}

	/* Compute the weight of the entire spectrum in the operating band */
	sap_compute_spect_weight(spect_info, mac_handle, scan_list, sap_ctx);

#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
	/* process avoid channel IE to collect all channels to avoid */
	sap_process_avoid_ie(mac_handle, sap_ctx, scan_list, spect_info);
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */

	wlan_reg_read_current_country(mac_ctx->psoc, country);
	wlan_reg_get_domain_from_country_code(&domain, country, SOURCE_DRIVER);

	SET_ACS_BAND(operating_band, sap_ctx);

	/* Sort the ch lst as per the computed weights, lesser weight first. */
	sap_sort_chl_weight_all(mac_ctx, sap_ctx, spect_info, operating_band,
				domain);

	/*Loop till get the best channel in the given range */
	for (count = 0; count < spect_info->numSpectChans; count++) {
		if (!spect_info->pSpectCh[count].valid)
			continue;

		best_chan_freq = spect_info->pSpectCh[count].chan_freq;
		/* check if best_ch_num is in preferred channel list */
		best_chan_freq =
			sap_select_preferred_channel_from_channel_list(
				best_chan_freq, sap_ctx, spect_info);
		/* if not in preferred ch lst, go to nxt best ch */
		if (best_chan_freq == SAP_CHANNEL_NOT_SELECTED)
			continue;

#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
		/*
		 * Weight of the channels(device's AP is operating)
		 * increased to MAX+1 so that they will be chosen only
		 * when there is no other best channel to choose
		 */
		if (!WLAN_REG_IS_6GHZ_CHAN_FREQ(best_chan_freq) &&
		    sap_check_in_avoid_ch_list(sap_ctx,
		    wlan_reg_freq_to_chan(mac_ctx->pdev, best_chan_freq))) {
			best_chan_freq = SAP_CHANNEL_NOT_SELECTED;
			continue;
		}
#endif

		/* Give preference to Non-overlap channels */
		if (WLAN_REG_IS_24GHZ_CH_FREQ(best_chan_freq) &&
		    !sap_is_ch_non_overlap(sap_ctx,
		    wlan_reg_freq_to_chan(mac_ctx->pdev, best_chan_freq))) {
			sap_debug("freq %d is overlapping, skipped",
				  best_chan_freq);
			continue;
		}

		best_ch_weight = spect_info->pSpectCh[count].weight;
		sap_debug("Freq = %d selected as best frequency weight = %d",
			  best_chan_freq, best_ch_weight);

		break;
	}

	/*
	 * in case the best channel seleted is not in PCL and there is another
	 * channel which has same weightage and is in PCL, choose the one in
	 * PCL
	 */
	if (!ch_in_pcl(sap_ctx, best_chan_freq)) {
		uint32_t cal_chan_freq, cal_chan_weight;
		for (count = 0; count < spect_info->numSpectChans; count++) {
			if (!spect_info->pSpectCh[count].valid)
				continue;

			cal_chan_freq = spect_info->pSpectCh[count].chan_freq;
			cal_chan_weight = spect_info->pSpectCh[count].weight;
			/* skip pcl channel whose weight is bigger than best */
			if (!ch_in_pcl(sap_ctx, cal_chan_freq) ||
			    (cal_chan_weight > best_ch_weight))
				continue;

			if (best_chan_freq == cal_chan_freq)
				continue;

			if (sap_select_preferred_channel_from_channel_list(
				cal_chan_freq, sap_ctx,
				spect_info)
				== SAP_CHANNEL_NOT_SELECTED)
				continue;

			best_chan_freq = cal_chan_freq;
			sap_debug("Changed best freq to %d Preferred freq",
				  best_chan_freq);

			break;
		}
	}

	sap_ctx->acs_cfg->pri_ch_freq = best_chan_freq;

	/* Below code is for 2.4Ghz freq, so freq to channel is safe here */

	/* determine secondary channel for 2.4G channel 5, 6, 7 in HT40 */
	if ((operating_band != eCSR_DOT11_MODE_11g) ||
	    (sap_ctx->acs_cfg->ch_width != CH_WIDTH_40MHZ))
		goto sap_ch_sel_end;

	best_ch_num = wlan_reg_freq_to_chan(mac_ctx->pdev, best_chan_freq);

	if (REGDOMAIN_FCC == domain)
		ht40plus2gendch = HT40PLUS_2G_FCC_CH_END;
	else
		ht40plus2gendch = HT40PLUS_2G_EURJAP_CH_END;
	if ((best_ch_num >= HT40MINUS_2G_CH_START) &&
			(best_ch_num <= ht40plus2gendch)) {
		int weight_below, weight_above, i;
		tSapSpectChInfo *pspect_info;

		weight_below = weight_above = SAP_ACS_WEIGHT_MAX;
		pspect_info = spect_info->pSpectCh;
		for (i = 0; i < spect_info->numSpectChans; i++) {
			if (pspect_info[i].chan_freq == (best_chan_freq - 20))
				weight_below = pspect_info[i].weight;
			if (pspect_info[i].chan_freq == (best_ch_num + 20))
				weight_above = pspect_info[i].weight;
		}

		if (weight_below < weight_above)
			sap_ctx->acs_cfg->ht_sec_ch_freq =
					sap_ctx->acs_cfg->pri_ch_freq - 20;
		else
			sap_ctx->acs_cfg->ht_sec_ch_freq =
					sap_ctx->acs_cfg->pri_ch_freq + 20;
	} else if (best_ch_num >= 1 && best_ch_num <= 4) {
		sap_ctx->acs_cfg->ht_sec_ch_freq =
					sap_ctx->acs_cfg->pri_ch_freq + 20;
	} else if (best_ch_num >= ht40plus2gendch && best_ch_num <=
			HT40MINUS_2G_CH_END) {
		sap_ctx->acs_cfg->ht_sec_ch_freq =
					sap_ctx->acs_cfg->pri_ch_freq - 20;
	} else if (best_ch_num == 14) {
		sap_ctx->acs_cfg->ht_sec_ch_freq = 0;
	}
	sap_ctx->sec_ch_freq = sap_ctx->acs_cfg->ht_sec_ch_freq;

sap_ch_sel_end:
	/* Free all the allocated memory */
	sap_chan_sel_exit(spect_info);

	return best_chan_freq;
}
