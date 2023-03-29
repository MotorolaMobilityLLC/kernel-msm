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

/**
 * DOC: This file contains configuration definitions for MLME LFR.
 */

#ifndef CFG_MLME_LFR_H__
#define CFG_MLME_LFR_H__

/*
 * <ini>
 * mawc_roam_enabled - Enable/Disable MAWC during roaming
 * @Min: 0 - Disabled
 * @Max: 1 - Enabled
 * @Default: 0
 *
 * This ini is used to control MAWC during roaming.
 *
 * Related: MAWCEnabled.
 *
 * Supported Feature: MAWC Roaming
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_LFR_MAWC_ROAM_ENABLED CFG_INI_BOOL( \
	"mawc_roam_enabled", \
	0, \
	"Enable/Disable MAWC during roaming")

/*
 * <ini>
 * mawc_roam_traffic_threshold - Configure traffic threshold
 * @Min: 0
 * @Max: 0xFFFFFFFF
 * @Default: 300
 *
 * This ini is used to configure the data traffic load in kbps to
 * register CMC.
 *
 * Related: mawc_roam_enabled.
 *
 * Supported Feature: MAWC Roaming
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_LFR_MAWC_ROAM_TRAFFIC_THRESHOLD CFG_INI_UINT( \
	"mawc_roam_traffic_threshold", \
	0, \
	0xFFFFFFFF, \
	300, \
	CFG_VALUE_OR_DEFAULT, \
	"Configure traffic threshold")

/*
 * <ini>
 * mawc_roam_ap_rssi_threshold - Best AP RSSI threshold
 * @Min: -120
 * @Max: 0
 * @Default: -66
 *
 * This ini is used to specify the RSSI threshold to scan for the AP.
 *
 * Related: mawc_roam_enabled.
 *
 * Supported Feature: MAWC Roaming
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_LFR_MAWC_ROAM_AP_RSSI_THRESHOLD CFG_INI_INT( \
	"mawc_roam_ap_rssi_threshold", \
	-120, \
	0, \
	-66, \
	CFG_VALUE_OR_DEFAULT, \
	"Best AP RSSI threshold")

/*
 * <ini>
 * mawc_roam_rssi_high_adjust - Adjust MAWC roam high RSSI
 * @Min: 3
 * @Max: 5
 * @Default: 5
 *
 * This ini is used for high RSSI threshold adjustment in stationary state
 * to suppress the scan.
 *
 * Related: mawc_roam_enabled.
 *
 * Supported Feature: MAWC Roaming
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_LFR_MAWC_ROAM_RSSI_HIGH_ADJUST CFG_INI_UINT( \
	"mawc_roam_rssi_high_adjust", \
	3, \
	5, \
	5, \
	CFG_VALUE_OR_DEFAULT, \
	"Adjust MAWC roam high RSSI")

/*
 * <ini>
 * mawc_roam_rssi_low_adjust - Adjust MAWC roam low RSSI
 * @Min: 3
 * @Max: 5
 * @Default: 5
 *
 * This ini is used for low RSSI threshold adjustment in stationary state
 * to suppress the scan.
 *
 * Related: mawc_roam_enabled.
 *
 * Supported Feature: MAWC Roaming
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_LFR_MAWC_ROAM_RSSI_LOW_ADJUST CFG_INI_UINT( \
	"mawc_roam_rssi_low_adjust", \
	3, \
	5, \
	5, \
	CFG_VALUE_OR_DEFAULT, \
	"Adjust MAWC roam low RSSI")

/*
 * <ini>
 * rssi_abs_thresh - The min RSSI of the candidate AP to consider roam
 * @Min: -96
 * @Max: 0
 * @Default: 0
 *
 * The RSSI value of the candidate AP should be higher than rssi_abs_thresh
 * to roam to the AP. 0 means no absolute minimum RSSI is required.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_ROAM_RSSI_ABS_THRESHOLD CFG_INI_INT( \
	"rssi_abs_thresh", \
	-96, \
	0, \
	0, \
	CFG_VALUE_OR_DEFAULT, \
	"The min RSSI of the candidate AP to consider roam")

/*
 * <ini>
 * lookup_threshold_5g_offset - Lookup threshold offset for 5G band
 * @Min: -120
 * @Max: 120
 * @Default: 0
 *
 * This ini is used to set the 5G band lookup threshold for roaming.
 * It depends on another INI which is gNeighborLookupThreshold.
 * gNeighborLookupThreshold is a legacy INI item which will be used to
 * set the RSSI lookup threshold for both 2G and 5G bands. If the
 * user wants to setup a different threshold for a 5G band, then user
 * can use this offset value which will be summed up to the value of
 * gNeighborLookupThreshold and used for 5G
 * e.g: gNeighborLookupThreshold = -76dBm
 *      lookup_threshold_5g_offset = 6dBm
 *      Then the 5G band will be configured to -76+6 = -70dBm
 * A default value of Zero to lookup_threshold_5g_offset will keep the
 * thresholds same for both 2G and 5G bands
 *
 * Related: gNeighborLookupThreshold
 *
 * Supported Feature: Roaming
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_LFR_5G_RSSI_THRESHOLD_OFFSET CFG_INI_INT( \
	"lookup_threshold_5g_offset", \
	-120, \
	120, \
	0, \
	CFG_VALUE_OR_DEFAULT, \
	"Lookup threshold offset for 5G band")

/*
 * <ini>
 * gEnableFastRoamInConcurrency - Enable LFR roaming on STA during concurrency
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This INI is used to enable Legacy fast roaming(LFR) on STA link during
 * concurrent sessions.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_ENABLE_FAST_ROAM_IN_CONCURRENCY CFG_INI_BOOL( \
	"gEnableFastRoamInConcurrency", \
	1, \
	"Enable LFR roaming on STA during concurrency")

/*
 * <ini>
 * gEnableEarlyStopScan - Set early stop scan
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to set early stop scan. Early stop
 * scan is a feature for roaming to stop the scans at
 * an early stage as soon as we find a better AP to roam.
 * This would make the roaming happen quickly.
 *
 * Related: None
 *
 * Supported Feature: LFR Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_EARLY_STOP_SCAN_ENABLE CFG_INI_BOOL( \
	"gEnableEarlyStopScan", \
	0, \
	"Set early stop scan")

/*
 * <ini>
 * gEarlyStopScanMinThreshold - Set early stop scan min
 * threshold
 * @Min: -80
 * @Max: -70
 * @Default: -73
 *
 * This ini is used to set the early stop scan minimum
 * threshold. Early stop scan minimum threshold is the
 * minimum threshold to be considered for stopping the
 * scan. The algorithm starts with a scan on the greedy
 * channel list with the maximum threshold and steps down
 * the threshold by 20% for each further channel. It can
 * step down on each channel but cannot go lower than the
 * minimum threshold.
 *
 * Related: None
 *
 * Supported Feature: Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_EARLY_STOP_SCAN_MIN_THRESHOLD CFG_INI_INT( \
	"gEarlyStopScanMinThreshold", \
	-80, \
	-70, \
	-73, \
	CFG_VALUE_OR_DEFAULT, \
	"Set early stop scan min")

/*
 * <ini>
 * gEarlyStopScanMaxThreshold - Set early stop scan max
 * threshold
 * @Min: -60
 * @Max: -40
 * @Default: -43
 *
 * This ini is used to set the the early stop scan maximum
 * threshold at which the candidate AP should be to be
 * qualified as a potential roam candidate and good enough
 * to stop the roaming scan.
 *
 * Related: None
 *
 * Supported Feature: Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_EARLY_STOP_SCAN_MAX_THRESHOLD CFG_INI_INT( \
	"gEarlyStopScanMaxThreshold", \
	-60, \
	-40, \
	-43, \
	CFG_VALUE_OR_DEFAULT, \
	"Set early stop scan max")

/*
 * <ini>
 * gtraffic_threshold - Dense traffic threshold
 * @Min: 0
 * @Max: 0xffffffff
 * @Default: 400
 *
 * Dense traffic threshold
 * traffic threshold required for dense roam scan
 * Measured in kbps
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_ROAM_DENSE_TRAFFIC_THRESHOLD CFG_INI_UINT( \
	"gtraffic_threshold", \
	0, \
	0xffffffff, \
	400, \
	CFG_VALUE_OR_DEFAULT, \
	"Dense traffic threshold")

/*
 * <ini>
 * groam_dense_rssi_thresh_offset - Sets dense roam RSSI threshold diff
 * @Min: 0
 * @Max: 20
 * @Default: 10
 *
 * This INI is used to set offset value from normal RSSI threshold to dense
 * RSSI threshold FW will optimize roaming based on new RSSI threshold once
 * it detects dense environment.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_ROAM_DENSE_RSSI_THRE_OFFSET CFG_INI_UINT( \
	"groam_dense_rssi_thresh_offset", \
	0, \
	20, \
	10, \
	CFG_VALUE_OR_DEFAULT, \
	"Dense traffic threshold")

/*
 * <ini>
 * groam_dense_min_aps - Sets minimum number of AP for dense roam
 * @Min: 1
 * @Max: 5
 * @Default: 3
 *
 * Minimum number of APs required for dense roam. FW will consider
 * environment as dense once it detects #APs operating is more than
 * groam_dense_min_aps.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_ROAM_DENSE_MIN_APS CFG_INI_UINT( \
	"groam_dense_min_aps", \
	1, \
	5, \
	3, \
	CFG_VALUE_OR_DEFAULT, \
	"Sets minimum number of AP for dense roam")

/*
 * <ini>
 * roam_bg_scan_bad_rssi_thresh - RSSI threshold for background roam
 * @Min: -96
 * @Max: 0
 * @Default: -76
 *
 * If the DUT is connected to an AP with weak signal, then the bad RSSI
 * threshold will be used as an opportunity to use the scan results
 * from other scan clients and try to roam if there is a better AP
 * available in the environment.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_ROAM_BG_SCAN_BAD_RSSI_THRESHOLD CFG_INI_INT( \
	"roam_bg_scan_bad_rssi_thresh", \
	-96, \
	0, \
	-76, \
	CFG_VALUE_OR_DEFAULT, \
	"RSSI threshold for background roam")

/*
 * <ini>
 * roam_bg_scan_client_bitmap - Bitmap used to identify the scan clients
 * @Min: 0
 * @Max: 0x7FF
 * @Default: 0x424
 *
 * This bitmap is used to define the client scans that need to be used
 * by the roaming module to perform a background roaming.
 * Currently supported bit positions are as follows:
 * Bit 0 is reserved in the firmware.
 * WMI_SCAN_CLIENT_NLO - 1
 * WMI_SCAN_CLIENT_EXTSCAN - 2
 * WMI_SCAN_CLIENT_ROAM - 3
 * WMI_SCAN_CLIENT_P2P - 4
 * WMI_SCAN_CLIENT_LPI - 5
 * WMI_SCAN_CLIENT_NAN - 6
 * WMI_SCAN_CLIENT_ANQP - 7
 * WMI_SCAN_CLIENT_OBSS - 8
 * WMI_SCAN_CLIENT_PLM - 9
 * WMI_SCAN_CLIENT_HOST - 10
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_ROAM_BG_SCAN_CLIENT_BITMAP CFG_INI_UINT( \
	"roam_bg_scan_client_bitmap", \
	0, \
	0x7FF, \
	0x424, \
	CFG_VALUE_OR_DEFAULT, \
	"Bitmap used to identify the scan clients")

/*
 * <ini>
 * roam_bad_rssi_thresh_offset_2g - RSSI threshold offset for 2G to 5G roam
 * @Min: 0
 * @Max: 86
 * @Default: 40
 *
 * If the DUT is connected to an AP with weak signal in 2G band, then the
 * bad RSSI offset for 2g would be used as offset from the bad RSSI
 * threshold configured and then use the resulting rssi for an opportunity
 * to use the scan results from other scan clients and try to roam to
 * 5G Band ONLY if there is a better AP available in the environment.
 *
 * For example if the roam_bg_scan_bad_rssi_thresh is -76 and
 * roam_bad_rssi_thresh_offset_2g is 40 then the difference of -36 would be
 * used as a trigger to roam to a 5G AP if DUT initially connected to a 2G AP
 *
 * Related: roam_bg_scan_bad_rssi_thresh
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_ROAM_BG_SCAN_BAD_RSSI_OFFSET_2G CFG_INI_UINT( \
	"roam_bad_rssi_thresh_offset_2g", \
	0, \
	86, \
	40, \
	CFG_VALUE_OR_DEFAULT, \
	"RSSI threshold offset for 2G to 5G roam")

/*
 * <ini>
 * roam_data_rssi_threshold_triggers - triggers of data rssi threshold for roam
 * @Min: 0
 * @Max: 0xffff
 * @Default: 0x3
 *
 * If the DUT is connected to an AP with weak signal, during latest
 * rx_data_inactivity_time, if there is no activity or avg of data_rssi is
 * better than roam_data_rssi_threshold(-70dbM), then suppress roaming
 * triggered by roam_data_rssi_threshold_triggers: low RSSI or bg scan.
 * Triggers bitmap definition:
 * ROAM_DATA_RSSI_FLAG_LOW_RSSI   1<<0
 * ROAM_DATA_RSSI_FLAG_BACKGROUND 1<<1
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ROAM_DATA_RSSI_THRESHOLD_TRIGGERS CFG_INI_UINT( \
	"roam_data_rssi_threshold_triggers", \
	0, \
	0xffff, \
	0x3, \
	CFG_VALUE_OR_DEFAULT, \
	"Triggers of DATA RSSI threshold for roam")

/*
 * <ini>
 * roam_data_rssi_threshold - Data RSSI threshold for background roam
 * @Min: -96
 * @Max: 0
 * @Default: -70
 *
 * If the DUT is connected to an AP with weak signal, during latest
 * rx_data_inactivity_time, if there is no activity or avg of data_rssi is
 * better than roam_data_rssi_threshold(-70dbM), then suppress roaming
 * triggered by roam_data_rssi_threshold_triggers: low RSSI or bg scan.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ROAM_DATA_RSSI_THRESHOLD CFG_INI_INT( \
	"roam_data_rssi_threshold", \
	-96, \
	0, \
	-70, \
	CFG_VALUE_OR_DEFAULT, \
	"DATA RSSI threshold for roam")

/*
 * <ini>
 * rx_data_inactivity_time - Duration to check data rssi
 * @Min: 0
 * @Max: 100000 ms
 * @Default: 2000
 *
 * If the DUT is connected to an AP with weak signal, during latest
 * rx_data_inactivity_time, if there is no activity or avg of data_rssi is
 * better than roam_data_rssi_threshold(-70dbM), then suppress roaming
 * triggered by roam_data_rssi_threshold_triggers: low RSSI or bg scan.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_RX_DATA_INACTIVITY_TIME CFG_INI_UINT( \
	"rx_data_inactivity_time", \
	0, \
	100000, \
	2000, \
	CFG_VALUE_OR_DEFAULT, \
	"Rx inactivity time to check data rssi")

/*
 * <ini>
 * roamscan_adaptive_dwell_mode - Sets dwell time adaptive mode
 * @Min: 0
 * @Max: 4
 * @Default: 4
 *
 * This parameter will set the algo used in dwell time optimization during
 * roam scan. see enum scan_dwelltime_adaptive_mode.
 * Acceptable values for this:
 * 0: Default (Use firmware default mode)
 * 1: Conservative optimization
 * 2: Moderate optimization
 * 3: Aggressive optimization
 * 4: Static
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_ADAPTIVE_ROAMSCAN_DWELL_MODE CFG_INI_UINT( \
	"roamscan_adaptive_dwell_mode", \
	0, \
	4, \
	4, \
	CFG_VALUE_OR_DEFAULT, \
	"Sets dwell time adaptive mode")

/*
 * <ini>
 * gper_roam_enabled - To enabled/disable PER based roaming in FW
 * @Min: 0
 * @Max: 3
 * @Default: 3
 *
 * This ini is used to enable/disable Packet error based roaming, enabling this
 * will cause DUT to monitor Tx and Rx traffic and roam to a better candidate
 * if current is not good enough.
 *
 * Values supported:
 * 0: disabled
 * 1: enabled for Rx traffic
 * 2: enabled for Tx traffic
 * 3: enabled for Tx and Rx traffic
 *
 * Related: gper_roam_high_rate_th, gper_roam_low_rate_th,
 *          gper_roam_th_percent, gper_roam_rest_time
 *
 * Supported Feature: LFR-3.0
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_LFR_PER_ROAM_ENABLE CFG_INI_UINT( \
	"gper_roam_enabled", \
	0, \
	3, \
	3, \
	CFG_VALUE_OR_DEFAULT, \
	"To enabled/disable PER based roaming in FW")

/*
 * <ini>
 * gper_roam_high_rate_th - Rate at which PER based roam will stop
 * @Min: 1 Mbps
 * @Max: 0xffffffff
 * @Default: 40 Mbps
 *
 * This ini is used to define the data rate in mbps*10 at which FW will stop
 * monitoring the traffic for PER based roam.
 *
 * Related: gper_roam_enabled, gper_roam_low_rate_th,
 *          gper_roam_th_percent, gper_roam_rest_time
 *
 * Supported Feature: LFR-3.0
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_LFR_PER_ROAM_CONFIG_HIGH_RATE_TH CFG_INI_UINT( \
	"gper_roam_high_rate_th", \
	10, \
	0xffffffff, \
	400, \
	CFG_VALUE_OR_DEFAULT, \
	"Rate at which PER based roam will stop")

/*
 * <ini>
 * gper_roam_low_rate_th - Rate at which FW starts considering traffic for PER
 * based roam.
 *
 * @Min: 1 Mbps
 * @Max: 0xffffffff
 * @Default: 20 Mbps
 *
 * This ini is used to define the rate in mbps*10 at which FW starts considering
 * traffic for PER based roam, if gper_roam_th_percent of data is below this
 * rate, FW will issue a roam scan.
 *
 * Related: gper_roam_enabled, gper_roam_high_rate_th,
 *          gper_roam_th_percent, gper_roam_rest_time
 *
 * Supported Feature: LFR-3.0
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_LFR_PER_ROAM_CONFIG_LOW_RATE_TH CFG_INI_UINT( \
	"gper_roam_low_rate_th", \
	10, \
	0xffffffff, \
	200, \
	CFG_VALUE_OR_DEFAULT, \
	"Rate at which FW starts considering traffic for PER")

/*
 * <ini>
 * gper_roam_th_percent - Percentage at which FW will issue a roam scan if
 * traffic is below gper_roam_low_rate_th rate.
 *
 * @Min: 10%
 * @Max: 100%
 * @Default: 60%
 *
 * This ini is used to define the percentage at which FW will issue a roam scan
 * if traffic is below gper_roam_low_rate_th rate.
 *
 * Related: gper_roam_enabled, gper_roam_high_rate_th,
 *          gper_roam_high_rate_th, gper_roam_rest_time
 *
 * Supported Feature: LFR-3.0
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_LFR_PER_ROAM_CONFIG_RATE_TH_PERCENT CFG_INI_UINT( \
	"gper_roam_th_percent", \
	10, \
	100, \
	60, \
	CFG_VALUE_OR_DEFAULT, \
	"Percentage at which FW will issue a roam scan")

/*
 * <ini>
 * gper_roam_rest_time - Time for which FW will wait once it issues a
 * roam scan.
 *
 * @Min: 10 seconds
 * @Max: 3600 seconds
 * @Default: 300 seconds
 *
 * This ini is used to define the time for which FW will wait once it issues a
 * PER based roam scan.
 *
 * Related: gper_roam_enabled, gper_roam_high_rate_th,
 *          gper_roam_high_rate_th, gper_roam_th_percent
 *
 * Supported Feature: LFR-3.0
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_LFR_PER_ROAM_REST_TIME CFG_INI_UINT( \
	"gper_roam_rest_time", \
	10, \
	3600, \
	300, \
	CFG_VALUE_OR_DEFAULT, \
	"Time for which FW will wait once it issues a roam scan")

/*
 * <ini>
 * gper_roam_mon_time - Minimum time required in seconds to
 * be considered as valid scenario for PER based roam
 * @Min: 5
 * @Max: 25
 * @Default: 25
 *
 * This ini is used to define minimum time in seconds for which DUT has
 * collected the PER stats before it can consider the stats hysteresis to be
 * valid for PER based scan.
 * DUT collects following information during this period:
 *     1. % of packets below gper_roam_low_rate_th
 *     2. # packets above gper_roam_high_rate_th
 * if DUT gets (1) greater than gper_roam_th_percent and (2) is zero during
 * this period, it triggers PER based roam scan.
 *
 * Related: gper_roam_enabled, gper_roam_high_rate_th, gper_roam_low_rate_th,
 *          gper_roam_th_percent, gper_roam_rest_time
 *
 * Supported Feature: LFR-3.0
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_LFR_PER_ROAM_MONITOR_TIME CFG_INI_UINT( \
	"gper_roam_mon_time", \
	5, \
	25, \
	25, \
	CFG_VALUE_OR_DEFAULT, \
	"Minimum time to be considered as valid scenario for PER based roam")

/*
 * <ini>
 * gper_min_rssi_threshold_for_roam -  Minimum roamable AP RSSI for
 * candidate selection for PER based roam
 * @Min: 0
 * @Max: 96
 * @Default: 83
 *
 * Minimum roamable AP RSSI for candidate selection for PER based roam
 *
 * Related: gper_roam_enabled, gper_roam_high_rate_th, gper_roam_low_rate_th,
 *          gper_roam_th_percent, gper_roam_rest_time
 *
 * Supported Feature: LFR-3.0
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_LFR_PER_ROAM_MIN_CANDIDATE_RSSI CFG_INI_UINT( \
	"gper_min_rssi_threshold_for_roam", \
	10, \
	96, \
	83, \
	CFG_VALUE_OR_DEFAULT, \
	"Minimum roamable AP RSSI for candidate selection for PER based roam")

/*
 * <ini>
 * groam_disallow_duration - disallow duration before roaming
 * @Min: 0
 * @Max: 3600
 * @Default: 30
 *
 * This ini is used to configure how long LCA[Last Connected AP] AP will
 * be disallowed before it can be a roaming candidate again, in units of
 * seconds.
 *
 * Related: LFR
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_LFR3_ROAM_DISALLOW_DURATION CFG_INI_UINT( \
	"groam_disallow_duration", \
	0, \
	3600, \
	30, \
	CFG_VALUE_OR_DEFAULT, \
	"disallow duration before roaming")

/*
 * <ini>
 * grssi_channel_penalization - RSSI penalization
 * @Min: 0
 * @Max: 15
 * @Default: 5
 *
 * This ini is used to configure RSSI that will be penalized if candidate(s)
 * are found to be in the same channel as disallowed AP's, in units of db.
 *
 * Related: LFR
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_LFR3_ROAM_RSSI_CHANNEL_PENALIZATION CFG_INI_UINT( \
	"grssi_channel_penalization", \
	0, \
	15, \
	5, \
	CFG_VALUE_OR_DEFAULT, \
	"RSSI penalization")

/*
 * <ini>
 * groam_num_disallowed_aps - Max number of AP's to maintain in LCA list
 * @Min: 0
 * @Max: 8
 * @Default: 3
 *
 * This ini is used to set the maximum number of AP's to be maintained
 * in LCA [Last Connected AP] list.
 *
 * Related: LFR
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_LFR3_ROAM_NUM_DISALLOWED_APS CFG_INI_UINT( \
	"groam_num_disallowed_aps", \
	0, \
	8, \
	3, \
	CFG_VALUE_OR_DEFAULT, \
	"Max number of AP's to maintain in LCA list")

/*
 * <ini>
 * enable_5g_band_pref - Enable preference for 5G from INI.
 * @Min: 0
 * @Max: 1
 * @Default: 0
 * This ini is used to enable 5G preference parameters.
 *
 * Related: 5g_rssi_boost_threshold, 5g_rssi_boost_factor, 5g_max_rssi_boost
 * 5g_rssi_penalize_threshold, 5g_rssi_penalize_factor, 5g_max_rssi_penalize
 *
 * Supported Feature: 5G band preference
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_ENABLE_5G_BAND_PREF CFG_INI_BOOL( \
	"enable_5g_band_pref", \
	0, \
	"Enable preference for 5G from INI")

/*
 * <ini>
 * 5g_rssi_boost_threshold - A_band_boost_threshold above which 5G is favored.
 * @Min: -70
 * @Max: -55
 * @Default: -60
 * This ini is used to set threshold for 5GHz band preference.
 *
 * Related: 5g_rssi_boost_factor, 5g_max_rssi_boost
 * 5g_rssi_penalize_threshold, 5g_rssi_penalize_factor, 5g_max_rssi_penalize
 *
 * Supported Feature: 5G band preference
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_5G_RSSI_BOOST_THRESHOLD CFG_INI_INT( \
	"5g_rssi_boost_threshold", \
	-70, \
	-55, \
	-60, \
	CFG_VALUE_OR_DEFAULT, \
	"A_band_boost_threshold above which 5 GHz is favored")

/*
 * <ini>
 * 5g_rssi_boost_factor - Factor by which 5GHz RSSI is boosted.
 * @Min: 0
 * @Max: 2
 * @Default: 1
 * This ini is used to set the 5Ghz boost factor.
 *
 * Related: 5g_rssi_boost_threshold, 5g_max_rssi_boost
 * 5g_rssi_penalize_threshold, 5g_rssi_penalize_factor, 5g_max_rssi_penalize
 *
 * Supported Feature: 5G band preference
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_5G_RSSI_BOOST_FACTOR CFG_INI_UINT( \
	"5g_rssi_boost_factor", \
	0, \
	2, \
	1, \
	CFG_VALUE_OR_DEFAULT, \
	"Factor by which 5GHz RSSI is boosted")

/*
 * <ini>
 * 5g_max_rssi_boost - Maximum boost that can be applied to 5GHz RSSI.
 * @Min: 0
 * @Max: 20
 * @Default: 10
 * This ini is used to set maximum boost which can be given to a 5Ghz network.
 *
 * Related: 5g_rssi_boost_threshold, 5g_rssi_boost_factor
 * 5g_rssi_penalize_threshold, 5g_rssi_penalize_factor, 5g_max_rssi_penalize
 *
 * Supported Feature: 5G band preference
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_5G_MAX_RSSI_BOOST CFG_INI_UINT( \
	"5g_max_rssi_boost", \
	0, \
	20, \
	10, \
	CFG_VALUE_OR_DEFAULT, \
	"Maximum boost that can be applied to 5GHz RSSI")

/*
 * <ini>
 * 5g_rssi_penalize_threshold - A_band_penalize_threshold above which
 * 5 GHz is not favored.
 * @Min: -80
 * @Max: -65
 * @Default: -70
 * This ini is used to set threshold for 5GHz band preference.
 *
 * Related: 5g_rssi_penalize_factor, 5g_max_rssi_penalize
 * 5g_rssi_boost_threshold, 5g_rssi_boost_factor, 5g_max_rssi_boost
 *
 * Supported Feature: 5G band preference
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_5G_RSSI_PENALIZE_THRESHOLD CFG_INI_INT( \
	"5g_rssi_penalize_threshold", \
	-80, \
	-65, \
	-70, \
	CFG_VALUE_OR_DEFAULT, \
	"A_band_penalize_threshold above which 5 GHz is not favored")

/*
 * <ini>
 * 5g_rssi_penalize_factor - Factor by which 5GHz RSSI is penalizeed.
 * @Min: 0
 * @Max: 2
 * @Default: 1
 * This ini is used to set the 5Ghz penalize factor.
 *
 * Related: 5g_rssi_penalize_threshold, 5g_max_rssi_penalize
 * 5g_rssi_boost_threshold, 5g_rssi_boost_factor, 5g_max_rssi_boost
 *
 * Supported Feature: 5G band preference
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_5G_RSSI_PENALIZE_FACTOR CFG_INI_UINT( \
	"5g_rssi_penalize_factor", \
	0, \
	2, \
	1, \
	CFG_VALUE_OR_DEFAULT, \
	"Factor by which 5GHz RSSI is penalizeed")

/*
 * <ini>
 * 5g_max_rssi_penalize - Maximum penalty that can be applied to 5GHz RSSI.
 * @Min: 0
 * @Max: 20
 * @Default: 10
 * This ini is used to set maximum penalty which can be given to a 5Ghz network.
 *
 * Related: 5g_rssi_penalize_threshold, 5g_rssi_penalize_factor
 * 5g_rssi_boost_threshold, 5g_rssi_boost_factor, 5g_max_rssi_boost
 *
 * Supported Feature: 5G band preference
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_5G_MAX_RSSI_PENALIZE CFG_INI_UINT( \
	"5g_max_rssi_penalize", \
	0, \
	20, \
	10, \
	CFG_VALUE_OR_DEFAULT, \
	"Maximum penalty that can be applied to 5GHz RSSI")

/*
 * <ini>
 * max_num_pre_auth - Configure max number of pre-auth
 * @Min: 0
 * @Max: 256
 * @Default: 64
 *
 * This ini is used to configure the data max number of pre-auth
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_LFR_MAX_NUM_PRE_AUTH CFG_UINT( \
	"max_num_pre_auth", \
	0, \
	256, \
	64, \
	CFG_VALUE_OR_DEFAULT, \
	"")

/*
 * <ini>
 * roam_preauth_retry_count
 *
 * @Min: 1
 * @Max: 10
 * @Default: 5
 *
 * The maximum number of software retries for preauth or
 * reassoc made before picking up the next candidate for
 * connection during roaming.
 *
 * Related: N/A
 *
 * Supported Features: Roaming
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_LFR3_ROAM_PREAUTH_RETRY_COUNT CFG_INI_INT( \
			"roam_preauth_retry_count", \
			1, \
			10, \
			5, \
			CFG_VALUE_OR_DEFAULT, \
			"The maximum number of software retries for preauth")

/*
 * <ini>
 * roam_preauth_no_ack_timeout
 *
 * @Min: 5
 * @Max: 50
 * @Default: 5
 *
 * Time to wait (in ms) after sending an preauth or reassoc
 * request which didn't have an ack, before considering
 * it as a failure and making another software retry.
 *
 * Related: N/A
 *
 * Supported Features: Roaming
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_LFR3_ROAM_PREAUTH_NO_ACK_TIMEOUT CFG_INI_INT( \
			"roam_preauth_no_ack_timeout", \
			5, \
			50, \
			5, \
			CFG_VALUE_OR_DEFAULT, \
			"Time to wait after sending an preauth or reassoc")

/*
 * <ini>
 * FastRoamEnabled - Enable fast roaming
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to inform FW to enable fast roaming
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_FEATURE_ENABLED CFG_INI_BOOL( \
	"FastRoamEnabled", \
	0, \
	"Enable fast roaming")

/*
 * <ini>
 * MAWCEnabled - Enable/Disable Motion Aided Wireless Connectivity Global
 * @Min: 0 - Disabled
 * @Max: 1 - Enabled
 * @Default: 0
 *
 * This ini is used to controls the MAWC feature globally.
 * MAWC is Motion Aided Wireless Connectivity.
 *
 * Related: mawc_roam_enabled.
 *
 * Supported Feature: Roaming and PNO/NLO
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_LFR_MAWC_FEATURE_ENABLED CFG_INI_BOOL( \
	"MAWCEnabled", \
	0, \
	"Enable MAWC")

/*
 * <ini>
 * FastTransitionEnabled - Enable fast transition in case of 11r and ese.
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to turn ON/OFF the whole neighbor roam, pre-auth, reassoc.
 * With this turned OFF 11r will completely not work. For 11r this flag has to
 * be ON. For ESE fastroam will not work.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_FAST_TRANSITION_ENABLED CFG_INI_BOOL( \
	"FastTransitionEnabled", \
	1, \
	"Enable fast transition")

/*
 * <ini>
 * RoamRssiDiff - Enable roam based on rssi
 * @Min: 0
 * @Max: 100
 * @Default: 5
 *
 * This INI is used to decide whether to Roam or not based on RSSI. AP1 is the
 * currently associated AP and AP2 is chosen for roaming. The Roaming will
 * happen only if AP2 has better Signal Quality and it has a RSSI better than
 * AP2. RoamRssiDiff is the number of units (typically measured in dB) AP2
 * is better than AP1.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_ROAM_RSSI_DIFF CFG_INI_UINT( \
	"RoamRssiDiff", \
	0, \
	100, \
	5, \
	CFG_VALUE_OR_DEFAULT, \
	"Enable roam based on rssi")

/*
 * <ini>
 * bg_rssi_threshold - To set RSSI Threshold for BG scan roaming
 * @Min: 0
 * @Max: 100
 * @Default: 5
 *
 * This INI is used to set the value of rssi threshold to trigger roaming
 * after background scan. To trigger roam after bg scan, value of rssi of
 * candidate AP should be higher by this threshold than the rssi of the
 * currrently associated AP.
 *
 * Related: RoamRssiDiff
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_ROAM_BG_RSSI_TH CFG_INI_UINT( \
	"bg_rssi_threshold", \
	0, \
	100, \
	5, \
	CFG_VALUE_OR_DEFAULT, \
	"Enable roam based on rssi after BG scan")

/*
 * <ini>
 * gWESModeEnabled - Enable WES mode
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable/disable Wireless Extended Security mode.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_ENABLE_WES_MODE CFG_INI_BOOL( \
	"gWESModeEnabled", \
	0, \
	"Enable WES mode")

/*
 * <ini>
 * gRoamScanOffloadEnabled - Enable Roam Scan Offload
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This INI is used to enable Roam Scan Offload in firmware
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_ROAM_SCAN_OFFLOAD_ENABLED CFG_INI_BOOL( \
	"gRoamScanOffloadEnabled", \
	1, \
	"Enable Roam Scan Offload")

/*
 * <ini>
 * gNeighborScanChannelList - Set channels to be scanned
 * by firmware for LFR scan
 * @Default: ""
 *
 * This ini is used to set the channels to be scanned
 * by firmware for LFR scan.
 *
 * Related: None
 *
 * Supported Feature: LFR Scan
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_LFR_NEIGHBOR_SCAN_CHANNEL_LIST CFG_INI_STRING( \
		"gNeighborScanChanList", \
		0, \
		CFG_VALID_CHANNEL_LIST_STRING_LEN, \
		"", \
		"Set channels to be scanned")

/*
 * <ini>
 * gNeighborScanTimerPeriod - Set neighbor scan timer period
 * @Min: 3
 * @Max: 300
 * @Default: 100
 *
 * This ini is used to set the timer period in secs after
 * which neighbor scan is trigerred.
 *
 * Related: None
 *
 * Supported Feature: LFR Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_NEIGHBOR_SCAN_TIMER_PERIOD CFG_INI_UINT( \
	"gNeighborScanTimerPeriod", \
	3, \
	300, \
	100, \
	CFG_VALUE_OR_DEFAULT, \
	"Neighbor scan timer period")

/*
 * <ini>
 * gRoamRestTimeMin - Set min neighbor scan timer period
 * @Min: 3
 * @Max: 300
 * @Default: 50
 *
 * This is the min rest time after which firmware will check for traffic
 * and if there no traffic it will move to a new channel to scan
 * else it will stay on the home channel till gNeighborScanTimerPeriod time
 * and then will move to a new channel to scan.
 *
 * Related: None
 *
 * Supported Feature: LFR Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_NEIGHBOR_SCAN_MIN_TIMER_PERIOD CFG_INI_UINT( \
	"gRoamRestTimeMin", \
	3, \
	300, \
	50, \
	CFG_VALUE_OR_DEFAULT, \
	"Min neighbor scan timer period")

/*
 * <ini>
 * gNeighborLookupThreshold - Set neighbor lookup rssi threshold
 * @Min: 10
 * @Max: 120
 * @Default: 78
 *
 * This is used to control the RSSI threshold for neighbor lookup.
 *
 * Related: None
 *
 * Supported Feature: LFR Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_NEIGHBOR_LOOKUP_RSSI_THRESHOLD CFG_INI_UINT( \
	"gNeighborLookupThreshold", \
	10, \
	120, \
	78, \
	CFG_VALUE_OR_DEFAULT, \
	"Neighbor lookup rssi threshold")

/*
 * <ini>
 * gOpportunisticThresholdDiff - Set oppurtunistic threshold diff
 * @Min: 0
 * @Max: 127
 * @Default: 0
 *
 * This ini is used to set opportunistic threshold diff.
 * This parameter is the RSSI diff above neighbor lookup
 * threshold, when opportunistic scan should be triggered.
 * MAX value is chosen so that this type of scan can be
 * always enabled by user.
 * MIN value will cause opportunistic scan to be triggered
 * in neighbor lookup RSSI range.
 *
 * Related: None
 *
 * Supported Feature: LFR Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_OPPORTUNISTIC_SCAN_THRESHOLD_DIFF CFG_INI_UINT( \
	"gOpportunisticThresholdDiff", \
	0, \
	127, \
	0, \
	CFG_VALUE_OR_DEFAULT, \
	"Set oppurtunistic threshold diff")

/*
 * <ini>
 * gRoamRescanRssiDiff - Sets RSSI for Scan trigger in firmware
 * @Min: 0
 * @Max: 100
 * @Default: 5
 *
 * This INI is the drop in RSSI value that will trigger a precautionary
 * scan by firmware. Max value is chosen in such a way that this type
 * of scan can be disabled by user.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_ROAM_RESCAN_RSSI_DIFF CFG_INI_UINT( \
	"gRoamRescanRssiDiff", \
	0, \
	100, \
	5, \
	CFG_VALUE_OR_DEFAULT, \
	"Sets RSSI for Scan trigger in firmware")

/*
 * <ini>
 * gNeighborScanChannelMinTime - Set neighbor scan channel min time
 * @Min: 10
 * @Max: 40
 * @Default: 20
 *
 * This ini is used to set the minimum time in secs spent on each
 * channel in LFR scan inside firmware.
 *
 * Related: None
 *
 * Supported Feature: LFR Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_NEIGHBOR_SCAN_MIN_CHAN_TIME CFG_INI_UINT( \
	"gNeighborScanChannelMinTime", \
	10, \
	40, \
	20, \
	CFG_VALUE_OR_DEFAULT, \
	"Neighbor scan channel min time")

/*
 * <ini>
 * gNeighborScanChannelMaxTime - Set neighbor scan channel max time
 * @Min: 3
 * @Max: 300
 * @Default: 40
 *
 * This ini is used to set the maximum time in secs spent on each
 * channel in LFR scan inside firmware.
 *
 * Related: None
 *
 * Supported Feature: LFR Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_NEIGHBOR_SCAN_MAX_CHAN_TIME CFG_INI_UINT( \
	"gNeighborScanChannelMaxTime", \
	3, \
	300, \
	40, \
	CFG_VALUE_OR_DEFAULT, \
	"Neighbor scan channel max time")

/*
 * <ini>
 * gNeighborScanRefreshPeriod - Set neighbor scan refresh period
 * @Min: 1000
 * @Max: 60000
 * @Default: 20000
 *
 * This ini is used by firmware to set scan refresh period
 * in msecs for lfr scan.
 *
 * Related: None
 *
 * Supported Feature: LFR Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_NEIGHBOR_SCAN_RESULTS_REFRESH_PERIOD CFG_INI_UINT( \
	"gNeighborScanRefreshPeriod", \
	1000, \
	60000, \
	20000, \
	CFG_VALUE_OR_DEFAULT, \
	"Neighbor scan refresh period")

/*
 * <ini>
 * gFullRoamScanPeriod - Set full roam scan refresh period
 * @Min: 0
 * @Max: 600
 * @Default: 0
 *
 * This ini is used by firmware to set full roam scan period in secs.
 * Full roam scan period is the minimum idle period in seconds between two
 * successive full channel roam scans. If this is configured as a non-zero,
 * full roam scan will be triggered for every configured interval.
 * If this configured as 0, full roam scan will not be triggered at all.
 *
 * Related: None
 *
 * Supported Feature: LFR Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_FULL_ROAM_SCAN_REFRESH_PERIOD CFG_INI_UINT( \
	"gFullRoamScanPeriod", \
	0, \
	600, \
	0, \
	CFG_VALUE_OR_DEFAULT, \
	"Full roam scan refresh period")

/*
 * <ini>
 * gEmptyScanRefreshPeriod - Set empty scan refresh period
 * @Min: 0
 * @Max: 60000
 * @Default: 0
 *
 * This ini is used by firmware to set scan period in msecs
 * following empty scan results.
 *
 * Related: None
 *
 * Supported Feature: LFR Scan
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_EMPTY_SCAN_REFRESH_PERIOD CFG_INI_UINT( \
	"gEmptyScanRefreshPeriod", \
	0, \
	60000, \
	0, \
	CFG_VALUE_OR_DEFAULT, \
	"Empty scan refresh period")

/*
 * <ini>
 * gRoamBmissFirstBcnt - Beacon miss count to trigger 1st bmiss event
 * @Min: 5
 * @Max: 100
 * @Default: 10
 *
 * This ini used to control how many beacon miss will trigger first bmiss
 * event. First bmiss event will result in roaming scan.
 *
 * Related: None
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_ROAM_BMISS_FIRST_BCNT CFG_INI_UINT( \
	"gRoamBmissFirstBcnt", \
	5, \
	100, \
	10, \
	CFG_VALUE_OR_DEFAULT, \
	"First beacon miss count")

/*
 * <ini>
 * gRoamBmissFinalBcnt - Beacon miss count to trigger final bmiss event
 * @Min: 5
 * @Max: 100
 * @Default: 20
 *
 * This ini used to control how many beacon miss will trigger final bmiss
 * event. Final bmiss event will make roaming take place or cause the
 * indication of final bmiss event.
 *
 * Related: None
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_ROAM_BMISS_FINAL_BCNT CFG_INI_UINT( \
	"gRoamBmissFinalBcnt", \
	5, \
	100, \
	20, \
	CFG_VALUE_OR_DEFAULT, \
	"Final beacon miss count")

/*
 * <ini>
 * gAllowDFSChannelRoam - Allow dfs channel in roam
 * @Min: 0
 * @Max: 2
 * @Default: 0
 *
 * This ini is used to set default dfs channel
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_LFR_ROAMING_DFS_CHANNEL CFG_INI_UINT( \
	"gAllowDFSChannelRoam", \
	0, \
	2, \
	0, \
	CFG_VALUE_OR_DEFAULT, \
	"Allow dfs channel in roam")

/*
 * <ini>
 * gRoamScanHiRssiMaxCount - Sets 5GHz maximum scan count
 * @Min: 0
 * @Max: 10
 * @Default: 3
 *
 * This INI is used to set maximum scan count in 5GHz
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_ROAM_SCAN_HI_RSSI_MAXCOUNT CFG_INI_UINT( \
	"gRoamScanHiRssiMaxCount", \
	0, \
	10, \
	3, \
	CFG_VALUE_OR_DEFAULT, \
	"5GHz maximum scan count")

/*
 * <ini>
 * gRoamScanHiRssiDelta - Sets RSSI Delta for scan trigger
 * @Min: 0
 * @Max: 16
 * @Default: 10
 *
 * This INI is used to set change in RSSI at which scan is triggered
 * in 5GHz.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_ROAM_SCAN_HI_RSSI_DELTA CFG_INI_UINT( \
	"gRoamScanHiRssiDelta", \
	0, \
	16, \
	10, \
	CFG_VALUE_OR_DEFAULT, \
	"RSSI Delta for scan trigger")

/*
 * <ini>
 * gRoamScanHiRssiDelay - Sets minimum delay between 5GHz scans
 * @Min: 5000
 * @Max: 0x7fffffff
 * @Default: 15000
 *
 * This INI is used to set the minimum delay between 5GHz scans.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_ROAM_SCAN_HI_RSSI_DELAY CFG_INI_UINT( \
	"gRoamScanHiRssiDelay", \
	5000, \
	0x7fffffff, \
	15000, \
	CFG_VALUE_OR_DEFAULT, \
	"Minimum delay between 5GHz scans")

/*
 * <ini>
 * gRoamScanHiRssiUpperBound - Sets upper bound after which 5GHz scan
 * @Min: -66
 * @Max: 0
 * @Default: -30
 *
 * This INI is used to set the RSSI upper bound above which the 5GHz scan
 * will not be performed.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_ROAM_SCAN_HI_RSSI_UB CFG_INI_INT( \
	"gRoamScanHiRssiUpperBound", \
	-66, \
	0, \
	-30, \
	CFG_VALUE_OR_DEFAULT, \
	"Upper bound after which 5GHz scan")

/*
 * <ini>
 * gRoamPrefer5GHz - Prefer roaming to 5GHz Bss
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to inform FW to prefer roaming to 5GHz BSS
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_ROAM_PREFER_5GHZ CFG_INI_BOOL( \
	"gRoamPrefer5GHz", \
	1, \
	"Prefer roaming to 5GHz Bss")

/*
 * <ini>
 * gRoamIntraBand - Prefer roaming within Band
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to inform FW to prefer roaming within band
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_ROAM_INTRA_BAND CFG_INI_BOOL( \
	"gRoamIntraBand", \
	0, \
	"Prefer roaming within Band")

/*
 * <ini>
 * gRoamScanNProbes - Sets the number of probes to be sent for firmware roaming
 * @Min: 1
 * @Max: 10
 * @Default: 2
 *
 * This INI is used to set the maximum number of probes the firmware can send
 * for firmware internal roaming cases.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_ROAM_SCAN_N_PROBES CFG_INI_UINT( \
	"gRoamScanNProbes", \
	1, \
	10, \
	2, \
	CFG_VALUE_OR_DEFAULT, \
	"The number of probes to be sent for firmware roaming")

/*
 * <ini>
 * gRoamScanHomeAwayTime - Sets the Home Away Time to firmware
 * @Min: 0
 * @Max: 300
 * @Default: 0
 *
 * Home Away Time should be at least equal to (gNeighborScanChannelMaxTime
 * + (2*RFS)), where RFS is the RF Switching time(3). It is twice RFS
 * to consider the time to go off channel and return to the home channel.
 *
 * Related: gNeighborScanChannelMaxTime
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_ROAM_SCAN_HOME_AWAY_TIME CFG_INI_UINT( \
	"gRoamScanHomeAwayTime", \
	0, \
	300, \
	0, \
	CFG_VALUE_OR_DEFAULT, \
	"the home away time to firmware")

/*
 * <ini>
 * gDelayBeforeVdevStop - wait time for tx complete before vdev stop
 * @Min: 2
 * @Max: 200
 * @Default: 20
 *
 * This INI is used to set wait time for tx complete before vdev stop.
 *
 * Related: None
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_DELAY_BEFORE_VDEV_STOP CFG_INI_UINT( \
	"gDelayBeforeVdevStop", \
	2, \
	200, \
	20, \
	CFG_VALUE_OR_DEFAULT, \
	"wait time for tx complete before vdev stop")
/*
 * <ini>
 * enable_bss_load_roam_trigger - enable/disable bss load based roam trigger
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini when enabled, allows the firmware to roam when bss load outpaces
 * the configured bss load threshold. When this ini is disabled, firmware
 * doesn't consider bss load values to trigger roam.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_BSS_LOAD_TRIGGERED_ROAM CFG_INI_BOOL( \
			"enable_bss_load_roam_trigger", \
			0, \
			"enable bss load triggered roaming")

/*
 * <ini>
 * bss_load_threshold - bss load above which the STA should trigger roaming
 * @Min: 0
 * @Max: 100
 * @Default: 70
 *
 * When the bss laod value that is sampled exceeds this threshold, firmware
 * will trigger roaming if bss load trigger is enabled.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BSS_LOAD_THRESHOLD CFG_INI_UINT( \
		"bss_load_threshold", \
		0, \
		100, \
		70, \
		CFG_VALUE_OR_DEFAULT, \
		"bss load threshold")

/*
 * <ini>
 * bss_load_sample_time - Time in milliseconds for which the bss load values
 * obtained from the beacons is sampled.
 * @Min: 0
 * @Max: 0xffffffff
 * @Default: 10000
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BSS_LOAD_SAMPLE_TIME CFG_INI_UINT( \
			"bss_load_sample_time", \
			0, \
			0xffffffff, \
			10000, \
			CFG_VALUE_OR_DEFAULT, \
			"bss load sampling time")

/*
 * <ini>
 * bss_load_trigger_5g_rssi_threshold - Current AP minimum RSSI in dBm below
 * which roaming can be triggered if BSS load exceeds bss_load_threshold.
 * @Min: -120
 * @Max: 0
 * @Default: -70
 *
 * If connected AP is in 5Ghz, then consider bss load roam triggered only if
 * load % > bss_load_threshold && connected AP rssi is worse than
 * bss_load_trigger_5g_rssi_threshold
 *
 * Related: "bss_load_threshold"
 *
 * Supported Feature: Roaming
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_BSS_LOAD_TRIG_5G_RSSI_THRES CFG_INI_INT( \
	"bss_load_trigger_5g_rssi_threshold", \
	-120, \
	0, \
	-70, \
	CFG_VALUE_OR_DEFAULT, \
	"Minimum RSSI of current AP in 5GHz band for BSS load roam trigger")

/*
 * <ini>
 * bss_load_trigger_2g_rssi_threshold - Current AP minimum RSSI in dBm below
 * which roaming can be triggered if BSS load exceeds bss_load_threshold.
 * @Min: -120
 * @Max: 0
 * @Default: -60
 *
 * If connected AP is in 2Ghz, then consider bss load roam triggered only if
 * load % > bss_load_threshold && connected AP rssi is worse than
 * bss_load_trigger_2g_rssi_threshold.
 *
 * Related: "bss_load_threshold"
 *
 * Supported Feature: Roaming
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_BSS_LOAD_TRIG_2G_RSSI_THRES CFG_INI_INT( \
	"bss_load_trigger_2g_rssi_threshold", \
	-120, \
	0, \
	-60, \
	CFG_VALUE_OR_DEFAULT, \
	"Minimum RSSI of current AP in 2.4GHz band for BSS load roam trigger")

/*
 * <ini>
 * ho_delay_for_rx - Delay hand-off (in msec) by this duration to receive
 * pending rx frames from current BSS
 * @Min: 0
 * @Max: 200
 * @Default: 0
 *
 * For LFR 3.0 roaming scenario, once roam candidate is found, firmware
 * waits for minimum this much duration to receive pending rx frames from
 * current BSS before switching to new channel for handoff to new AP.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR3_ROAM_HO_DELAY_FOR_RX CFG_INI_UINT( \
	"ho_delay_for_rx", \
	0, \
	200, \
	0, \
	CFG_VALUE_OR_DEFAULT, \
	"Delay Hand-off by this duration to receive")

/*
 * <ini>
 * min_delay_btw_roam_scans - Min duration (in sec) allowed btw two
 * consecutive roam scans
 * @Min: 0
 * @Max: 60
 * @Default: 10
 *
 * Roam scan is not allowed if duration between two consecutive
 * roam scans is less than this time.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_MIN_DELAY_BTW_ROAM_SCAN CFG_INI_UINT( \
	"min_delay_btw_roam_scans", \
	0, \
	60, \
	10, \
	CFG_VALUE_OR_DEFAULT, \
	"Min duration")

/*
 * <ini>
 * roam_trigger_reason_bitmask - Contains roam_trigger_reasons
 * @Min: 0
 * @Max: 0xFFFFFFFF
 * @Default: 0x10DA
 *
 * Bitmask containing roam_trigger_reasons for which
 * min_delay_btw_roam_scans constraint should be applied.
 * Currently supported bit positions are as follows:
 * Bit 0 is reserved in the firmware.
 * WMI_ROAM_TRIGGER_REASON_PER - 1
 * WMI_ROAM_TRIGGER_REASON_BMISS - 2
 * WMI_ROAM_TRIGGER_REASON_LOW_RSSI - 3
 * WMI_ROAM_TRIGGER_REASON_HIGH_RSSI - 4
 * WMI_ROAM_TRIGGER_REASON_PERIODIC - 5
 * WMI_ROAM_TRIGGER_REASON_MAWC - 6
 * WMI_ROAM_TRIGGER_REASON_DENSE - 7
 * WMI_ROAM_TRIGGER_REASON_BACKGROUND - 8
 * WMI_ROAM_TRIGGER_REASON_FORCED - 9
 * WMI_ROAM_TRIGGER_REASON_BTM - 10
 * WMI_ROAM_TRIGGER_REASON_UNIT_TEST - 11
 * WMI_ROAM_TRIGGER_REASON_BSS_LOAD - 12
 * WMI_ROAM_TRIGGER_REASON_DEAUTH - 13
 * WMI_ROAM_TRIGGER_REASON_IDLE - 14
 * WMI_ROAM_TRIGGER_REASON_MAX - 15
 *
 * For Ex: 0xDA (PER, LOW_RSSI, HIGH_RSSI, MAWC, DENSE)
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_ROAM_SCAN_TRIGGER_REASON_BITMASK CFG_INI_UINT( \
	"roam_trigger_reason_bitmask", \
	0, \
	0xFFFFFFFF, \
	0x10DA, \
	CFG_VALUE_OR_DEFAULT, \
	"Contains roam_trigger_reasons")

/*
 * <ini>
 * enable_ftopen - enable/disable FT open feature
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This INI is used to enable/disable FT open feature
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_ROAM_FT_OPEN_ENABLE CFG_INI_BOOL( \
	"enable_ftopen", \
	1, \
	"enable/disable FT open feature")

/*
 * <ini>
 * roam_force_rssi_trigger - To force RSSI trigger
 * irrespective of channel list type
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to set roam scan mode
 * WMI_ROAM_SCAN_MODE_RSSI_CHANGE, irrespective of whether
 * channel list type is CHANNEL_LIST_STATIC or not
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_ROAM_FORCE_RSSI_TRIGGER CFG_INI_BOOL( \
	"roam_force_rssi_trigger", \
	1, \
	"To force RSSI trigger")

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/*
 * <ini>
 * gRoamOffloadEnabled - enable/disable roam offload feature
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This INI is used to enable/disable roam offload feature
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR3_ROAMING_OFFLOAD CFG_INI_BOOL( \
	"gRoamOffloadEnabled", \
	1, \
	"enable roam offload")

/*
 * <ini>
 * enable_self_bss_roam - enable/disable roaming to self bss
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This INI is used to enable/disable roaming to already connected BSSID
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_LFR3_ENABLE_SELF_BSS_ROAM CFG_INI_BOOL( \
	"enable_self_bss_roam", \
	0, \
	"enable self bss roam")

/*
 * <ini>
 * enable_disconnect_roam_offload - Enable/Disable emergency roaming during
 * deauth/disassoc
 * @Min: 0 - Disabled
 * @Max: 1 - Enabled
 * @Default: 1
 *
 * When this ini is enabled firmware will trigger roam scan and roam to a new ap
 * if candidate is found and it will not send the deauth/disassoc frame to
 * the host driver.
 * If roaming fails after this deauth, then firmware will send
 * WMI_ROAM_REASON_DEAUTH event to the host. If roaming is successful, driver
 * follows the normal roam synch event path.
 *
 * Supported Feature: Roaming
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_LFR_ENABLE_DISCONNECT_ROAM CFG_INI_BOOL( \
	"enable_disconnect_roam_offload", \
	true, \
	"Enable/Disable roaming on deauth/disassoc from AP")

/*
 * <ini>
 * enable_idle_roam - Enable/Disable idle roaming
 * @Min: 0 - Disabled
 * @Max: 1 - Enabled
 * @Default: 0
 *
 * When this ini is enabled firmware will trigger roam scan and roam to a new
 * ap if current connected AP rssi falls below the threshold. To consider the
 * connection as idle, the following conditions should be met if this ini
 * "enable_idle_roam" is enabled:
 * 1. User space sends "SET SUSPENDMODE" command with value 0.
 * 2. No TX/RX data for idle time configured via ini "idle_roam_inactive_time".
 * 3. Connected AP rssi change doesn't exceed a specific delta value.
 * (configured via ini idle_roam_rssi_delta)
 * 4. Connected AP rssi falls below minimum rssi (configured via ini
 * "idle_roam_min_rssi").
 *
 * Supported Feature: Roaming
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_LFR_ENABLE_IDLE_ROAM CFG_INI_BOOL( \
	"enable_idle_roam", \
	false, \
	"Enable/Disable idle roam")

/*
 * <ini>
 * idle_roam_rssi_delta - This threshold is the criteria to decide whether DUT
 * is idle or moving. If rssi delta is more than configured thresold then its
 * considered as not idle. RSSI delta is entered in dBm. Idle roaming can be
 * triggered if the connected AP rssi change exceeds or falls below the
 * rssi delta and if other criteria of ini "enable_idle_roam" is met
 * @Min: 0
 * @Max: 50
 * @Default: 3
 *
 * Related: enable_idle_roam
 *
 * Supported Feature: Roaming
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_LFR_IDLE_ROAM_RSSI_DELTA CFG_INI_UINT( \
	"idle_roam_rssi_delta", \
	0, \
	50, \
	3, \
	CFG_VALUE_OR_DEFAULT, \
	"Configure RSSI delta to start idle roam")

/*
 * <ini>
 * idle_roam_inactive_time - Time duration in millseconds for which the
 * connection is idle.
 * @Min: 0
 * @Max: 0xFFFFFFFF
 * @Default: 10000
 *
 * This ini is used to configure the time in seconds for which the connection
 * candidate is idle and after which idle roam scan can be triggered if
 * other criteria of ini "enable_idle_roam" is met.
 *
 * Related: enable_idle_roam
 *
 * Supported Feature: Roaming
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_LFR_IDLE_ROAM_INACTIVE_TIME CFG_INI_UINT( \
	"idle_roam_inactive_time", \
	0, \
	0xFFFFFFFF, \
	10000, \
	CFG_VALUE_OR_DEFAULT, \
	"Configure RSSI delta to start idle roam")

/*
 * <ini>
 * idle_data_packet_count - No of tx/rx packets above which the connection is
 * not idle.
 * @Min: 0
 * @Max: 0xFFFFFFFF
 * @Default: 10
 *
 * This ini is used to configure the acceptable number of tx/rx packets below
 * which the connection is idle. Ex: If idle_data_packet_count is 10
 * and if the tx/rx packet count is less than 10, the connection is
 * idle. If there are more than 10 packets, the connection is active one.
 *
 * Related: enable_idle_roam
 *
 * Supported Feature: Roaming
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_LFR_IDLE_ROAM_PACKET_COUNT CFG_INI_UINT( \
	"idle_data_packet_count", \
	0, \
	0xFFFFFFFF, \
	10, \
	CFG_VALUE_OR_DEFAULT, \
	"Configure idle packet count")

/*
 * <ini>
 * idle_roam_min_rssi - Minimum RSSI of connected AP, below which
 * idle roam scan can be triggered if other criteria of ini "enable_idle_roam"
 * is met.
 * @Min: -96
 * @Max: 0
 * @Default: -65
 *
 * Related: enable_idle_roam
 *
 * Supported Feature: Roaming
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_LFR_IDLE_ROAM_MIN_RSSI CFG_INI_INT( \
	"idle_roam_min_rssi", \
	-96, \
	0, \
	-65, \
	CFG_VALUE_OR_DEFAULT, \
	"Configure idle roam minimum RSSI")

/*
 * <ini>
 * idle_roam_band - Band on which idle roam scan will be
 * enabled
 * @Min: 0
 * @Max: 2
 * @Default: 0
 *
 * Value 0 - Allow idle roam on both bands
 * Value 1 - Allow idle roam only on 2G band
 * Value 2 - Allow idle roam only on 5G band
 *
 * Related: enable_idle_roam
 *
 * Supported Feature: Roaming
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_LFR_IDLE_ROAM_BAND CFG_INI_UINT( \
	"idle_roam_band", \
	0, \
	2, \
	0, \
	CFG_VALUE_OR_DEFAULT, \
	"Band on which idle roam needs to be enabled")

/*
 * <ini>
 * roam_triggers - Bitmap of roaming triggers. Setting this to
 * zero will disable roaming altogether for the STA interface.
 * ESS report element of beacon explores BSS information, for roaming station
 * uses it to consider next AP to roam. ROAM_TRIGGER_REASON_ESS_RSSI bit is
 * to enable/disable roam trigger for ESS RSSI reason. This bit of ini is also
 * used for WFA certification.
 * @Min: 0
 * @Max: 0xFFFFFFFF
 * @Default: 0x3FFFF
 *
 * ROAM_TRIGGER_REASON_PER         BIT 1
 * ROAM_TRIGGER_REASON_BMISS       BIT 2
 * ROAM_TRIGGER_REASON_LOW_RSSI    BIT 3
 * ROAM_TRIGGER_REASON_HIGH_RSSI   BIT 4
 * ROAM_TRIGGER_REASON_PERIODIC    BIT 5
 * ROAM_TRIGGER_REASON_MAWC        BIT 6
 * ROAM_TRIGGER_REASON_DENSE       BIT 7
 * ROAM_TRIGGER_REASON_BACKGROUND  BIT 8
 * ROAM_TRIGGER_REASON_FORCED      BIT 9
 * ROAM_TRIGGER_REASON_BTM         BIT 10
 * ROAM_TRIGGER_REASON_UNIT_TEST   BIT 11
 * ROAM_TRIGGER_REASON_BSS_LOAD    BIT 12
 * ROAM_TRIGGER_REASON_DEAUTH      BIT 13
 * ROAM_TRIGGER_REASON_IDLE        BIT 14
 * ROAM_TRIGGER_REASON_STA_KICKOUT BIT 15
 * ROAM_TRIGGER_REASON_ESS_RSSI    BIT 16
 * ROAM_TRIGGER_REASON_WTC_BTM     BIT 17
 * ROAM_TRIGGER_REASON_PMK_TIMEOUT BIT 18
 * ROAM_TRIGGER_REASON_MAX         BIT 19
 *
 * Related: none
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ROAM_TRIGGER_BITMAP CFG_INI_UINT( \
			"roam_triggers", \
			0, \
			0xFFFFFFFF, \
			0x7FFFF, \
			CFG_VALUE_OR_DEFAULT, \
			"Bitmap of roaming triggers")

/*
 * <ini>
 * sta_disable_roam - Disable Roam on sta interface
 * @Min: 0 - Roam Enabled on sta interface
 * @Max: 0xffffffff - Roam Disabled on sta interface irrespective
 * of other interface connections
 * @Default: 0x00
 *
 * Disable roaming on STA iface to avoid audio glitches on p2p and ndp if
 * those are in connected state. Each bit for "sta_disable_roam" INI represents
 * an interface for which sta roaming can be disabled.
 *
 * LFR3_STA_ROAM_DISABLE_BY_P2P BIT(0)
 * LFR3_STA_ROAM_DISABLE_BY_NAN BIT(1)
 *
 * Related: None.
 *
 * Supported Feature: ROAM
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_STA_DISABLE_ROAM CFG_INI_UINT( \
		"sta_disable_roam", \
		0, \
		0xffffffff, \
		0x00, \
		CFG_VALUE_OR_DEFAULT, \
		"disable roam on STA iface if one of the iface mentioned in default is in connected state")

/*
 * <ini>
 * enable_dual_sta_roam_offload - Enable roaming offload on both interfaces
 * for STA + STA
 * @Min: 0 - Dual STA Roam offload Disabled
 * @Max: 1 - Dual STA Roam offload Enabled
 * @Default: 1
 *
 * Enabling this ini will:
 *  a) Enforce the STA + STA connection be DBS if the hw is capable.
 *  b) Enable Roam Scan Offload on both the STA vdev.
 *  c) Enable firmware to support sequential roaming on both STA vdev
 *     if the firmware is capable of dual sta roaming.
 *
 * Related: None.
 *
 * Supported Feature: ROAM
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_DUAL_STA_ROAM_OFFLOAD CFG_INI_UINT( \
		"enable_dual_sta_roam_offload", \
		false, \
		true, \
		true, \
		CFG_VALUE_OR_DEFAULT, \
		"Enable roam on both STA vdev")

#define ROAM_OFFLOAD_ALL \
	CFG(CFG_LFR3_ROAMING_OFFLOAD) \
	CFG(CFG_LFR3_ENABLE_SELF_BSS_ROAM) \
	CFG(CFG_LFR_ENABLE_DISCONNECT_ROAM) \
	CFG(CFG_LFR_ENABLE_IDLE_ROAM) \
	CFG(CFG_LFR_IDLE_ROAM_RSSI_DELTA) \
	CFG(CFG_LFR_IDLE_ROAM_INACTIVE_TIME) \
	CFG(CFG_LFR_IDLE_ROAM_PACKET_COUNT) \
	CFG(CFG_LFR_IDLE_ROAM_MIN_RSSI) \
	CFG(CFG_LFR_IDLE_ROAM_BAND) \
	CFG(CFG_ROAM_TRIGGER_BITMAP) \
	CFG(CFG_STA_DISABLE_ROAM) \
	CFG(CFG_ENABLE_DUAL_STA_ROAM_OFFLOAD) \

#else
#define ROAM_OFFLOAD_ALL
#endif

#ifdef FEATURE_WLAN_ESE
/*
 * <ini>
 * EseEnabled - Enable ESE feature
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable/disable ESE feature
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR_ESE_FEATURE_ENABLED CFG_INI_BOOL( \
	"EseEnabled", \
	0, \
	"Enable ESE")
#define LFR_ESE_ALL CFG(CFG_LFR_ESE_FEATURE_ENABLED)
#else
#define LFR_ESE_ALL
#endif

#ifdef FEATURE_LFR_SUBNET_DETECTION
/*
 * <ini>
 * gLFRSubnetDetectionEnable - Enable LFR3 subnet detection
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * Enable IP subnet detection during legacy fast roming version 3. Legacy fast
 * roaming could roam across IP subnets without host processors' knowledge.
 * This feature enables firmware to wake up the host processor if it
 * successfully determines change in the IP subnet. Change in IP subnet could
 * potentially cause disruption in IP connnectivity if IP address is not
 * refreshed.
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_LFR3_ENABLE_SUBNET_DETECTION CFG_INI_BOOL( \
	"gLFRSubnetDetectionEnable", \
	1, \
	"Enable LFR3 subnet detection")

#define LFR_SUBNET_DETECTION_ALL CFG(CFG_LFR3_ENABLE_SUBNET_DETECTION)
#else
#define LFR_SUBNET_DETECTION_ALL
#endif

#if defined(WLAN_SAE_SINGLE_PMK) && defined(WLAN_FEATURE_ROAM_OFFLOAD)
/*
 * <ini>
 * sae_single_pmk_feature_enabled - Enable/disable sae single pmk feature.
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This INI is to enable/disable SAE Roaming with same PMK/PMKID feature support
 *
 * Related: None.
 *
 * Supported Feature: Roaming
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_SAE_SINGLE_PMK CFG_INI_BOOL( \
		"sae_single_pmk_feature_enabled", \
		true, \
		"Enable/disable SAE Roaming with single PMK/PMKID")

#define SAE_SINGLE_PMK_ALL CFG(CFG_SAE_SINGLE_PMK)
#else
#define SAE_SINGLE_PMK_ALL
#endif

#ifdef WLAN_ADAPTIVE_11R
/*
 * <ini>
 * adaptive_11r - Enable/disable adaptive 11r feature.
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * Adaptive 11r feature enables the AP to support FT-AKM without
 * configuring the FT-AKM in the network. The AP will advertise non-FT akm
 * with a vendor specific IE having Adaptive 11r bit set to 1 in the IE data.
 * The AP also advertises the MDE in beacon/probe response.
 *
 * STA should check the adaptive 11r capability if the AP advertises MDE in
 * beacon/probe and adaptive 11r capability in vendor specific IE.  If adaptive
 * 11r capability is found, STA can advertise the FT equivalent of the non-FT
 * AKM and connect with 11r protocol.
 *
 * Related: None.
 *
 * Supported Feature: Fast BSS Transition
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ADAPTIVE_11R CFG_INI_BOOL( \
		"enable_adaptive_11r", \
		false, \
		"Enable/disable adaptive 11r support")

#define ADAPTIVE_11R_ALL CFG(CFG_ADAPTIVE_11R)
#else
#define ADAPTIVE_11R_ALL
#endif

/*
 * <ini>
 * roaming_scan_policy - To config roaming scan policy
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to configure roaming scan behavior from HOST
 * 0 : DBS scan
 * 1 : Non-DBS scan
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ROAM_SCAN_SCAN_POLICY CFG_INI_BOOL( \
		"roaming_scan_policy", \
		0, \
		"Config roam scan policy")

/*
 * <ini>
 * enable_ft_im_roaming - FW needs to perform FT initial moiblity association
 * instead of FT roaming for deauth roam trigger
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to FT roaming for deauth roam trigger behavior from HOST
 * 0 - To disable FT-IM
 * 1 - To enable FT-IM
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_FT_IM_ROAMING CFG_INI_BOOL( \
		"enable_ft_im_roaming", \
		1, \
		"FT roaming for deauth roam trigger")

/*
 * <ini>
 * roam_scan_inactivity_time - Device inactivity monitoring time in
 * milliseconds for which the device is considered to be inactive with data
 * packets count is less than configured roam_inactive_data_count.
 *
 * @Min: 0
 * @Max: 0xFFFFFFFF
 * @Default: 0
 *
 * The below three ini values are used to control the roam scan after the
 * firmware gets empty roam scan results during periodic roam scans.
 * 1. roam_scan_inactivity_time
 * 2. roam_inactive_data_count
 * 3. roam_scan_period_after_inactivity
 * The first two ini "roam_scan_inactivity_time" and "roam_inactive_data_count"
 * is frames the criteria to detect if the DUT is inactive. If the device is
 * identified to be inactive based on the above two ini, then the value,
 * "roam_scan_period_after_inactivity" will be used as periodic roam scan
 * duration.
 *
 * Related: roam_inactive_data_count
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ROAM_SCAN_INACTIVITY_TIME CFG_INI_UINT( \
	"roam_scan_inactivity_time", \
	0, \
	0xFFFFFFFF, \
	0, \
	CFG_VALUE_OR_DEFAULT, \
	"Device inactivity monitoring time")

/*
 * <ini>
 * roam_inactive_data_count - Maximum allowed data packets count during
 * roam_scan_inactivity_time.
 *
 * @Min: 0
 * @Max: 0xFFFFFFFF
 * @Default: 10
 *
 * The DUT is said to be inactive only if the data packets count
 * during this  roam_scan_inactivity_time is less than the configured
 * roam_inactive_data_count.
 *
 * Related: roam_scan_inactivity_time
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ROAM_INACTIVE_COUNT CFG_INI_UINT( \
	"roam_inactive_data_count", \
	0, \
	0xFFFFFFFF, \
	10, \
	CFG_VALUE_OR_DEFAULT, \
	"Roam scan inactivity period data pkt count")

/*
 * <ini>
 * roam_scan_period_after_inactivity - Roam scan duration in ms after device is
 * out of inactivity state.
 *
 * @Min: 0
 * @Max: 0xFFFFFFFF
 * @Default: 120000
 *
 * If there is empty scan results during roam scan, firmware will move to
 * roam scan inactive state if roam_scan_inactivity and
 * roam_inactive_data_count criteria are met.
 * This ini is used to configure the roam scan duration in ms once the
 * inactivity is finished and roam scan can be started.
 *
 * Related: roam_scan_inactivity_time, roam_inactive_data_count
 *
 * Supported Feature: Roaming
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_POST_INACTIVITY_ROAM_SCAN_PERIOD CFG_INI_UINT( \
	"roam_scan_period_after_inactivity", \
	0, \
	0xFFFFFFFF, \
	120000, \
	CFG_VALUE_OR_DEFAULT, \
	"Roam scan period post inactivity")

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/*
 * <ini>
 * enable_roam_reason_vsie - Enable/Disable inclusion of Roam Reason
 * in Re(association) frame
 *
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable fw to include/exclude roam reason vsie in
 * Re(association)
 *
 * Related: None
 *
 * Supported Feature: Roaming
 *
 * Usage: internal
 *
 * </ini>
 */
#define CFG_ENABLE_ROAM_REASON_VSIE CFG_INI_BOOL( \
		"enable_roam_reason_vsie", \
		0, \
		"To Enable enable_roam_reason_vsie")
#define ROAM_REASON_VSIE_ALL CFG(CFG_ENABLE_ROAM_REASON_VSIE)
#else
#define ROAM_REASON_VSIE_ALL
#endif

#define CFG_LFR_ALL \
	CFG(CFG_LFR_MAWC_ROAM_ENABLED) \
	CFG(CFG_LFR_MAWC_ROAM_TRAFFIC_THRESHOLD) \
	CFG(CFG_LFR_MAWC_ROAM_AP_RSSI_THRESHOLD) \
	CFG(CFG_LFR_MAWC_ROAM_RSSI_HIGH_ADJUST) \
	CFG(CFG_LFR_MAWC_ROAM_RSSI_LOW_ADJUST) \
	CFG(CFG_LFR_ROAM_RSSI_ABS_THRESHOLD) \
	CFG(CFG_LFR_5G_RSSI_THRESHOLD_OFFSET) \
	CFG(CFG_LFR_ENABLE_FAST_ROAM_IN_CONCURRENCY) \
	CFG(CFG_LFR_EARLY_STOP_SCAN_ENABLE) \
	CFG(CFG_LFR_EARLY_STOP_SCAN_MIN_THRESHOLD) \
	CFG(CFG_LFR_EARLY_STOP_SCAN_MAX_THRESHOLD) \
	CFG(CFG_LFR_ROAM_DENSE_TRAFFIC_THRESHOLD) \
	CFG(CFG_LFR_ROAM_DENSE_RSSI_THRE_OFFSET) \
	CFG(CFG_LFR_ROAM_DENSE_MIN_APS) \
	CFG(CFG_LFR_ROAM_BG_SCAN_BAD_RSSI_THRESHOLD) \
	CFG(CFG_LFR_ROAM_BG_SCAN_CLIENT_BITMAP) \
	CFG(CFG_LFR_ROAM_BG_SCAN_BAD_RSSI_OFFSET_2G) \
	CFG(CFG_ROAM_DATA_RSSI_THRESHOLD_TRIGGERS) \
	CFG(CFG_ROAM_DATA_RSSI_THRESHOLD) \
	CFG(CFG_RX_DATA_INACTIVITY_TIME) \
	CFG(CFG_LFR_ADAPTIVE_ROAMSCAN_DWELL_MODE) \
	CFG(CFG_LFR_PER_ROAM_ENABLE) \
	CFG(CFG_LFR_PER_ROAM_CONFIG_HIGH_RATE_TH) \
	CFG(CFG_LFR_PER_ROAM_CONFIG_LOW_RATE_TH) \
	CFG(CFG_LFR_PER_ROAM_CONFIG_RATE_TH_PERCENT) \
	CFG(CFG_LFR_PER_ROAM_REST_TIME) \
	CFG(CFG_LFR_PER_ROAM_MONITOR_TIME) \
	CFG(CFG_LFR_PER_ROAM_MIN_CANDIDATE_RSSI) \
	CFG(CFG_LFR3_ROAM_DISALLOW_DURATION) \
	CFG(CFG_LFR3_ROAM_RSSI_CHANNEL_PENALIZATION) \
	CFG(CFG_LFR3_ROAM_NUM_DISALLOWED_APS) \
	CFG(CFG_LFR_ENABLE_5G_BAND_PREF) \
	CFG(CFG_LFR_5G_RSSI_BOOST_THRESHOLD) \
	CFG(CFG_LFR_5G_RSSI_BOOST_FACTOR) \
	CFG(CFG_LFR_5G_MAX_RSSI_BOOST) \
	CFG(CFG_LFR_5G_RSSI_PENALIZE_THRESHOLD) \
	CFG(CFG_LFR_5G_RSSI_PENALIZE_FACTOR) \
	CFG(CFG_LFR_5G_MAX_RSSI_PENALIZE) \
	CFG(CFG_LFR_MAX_NUM_PRE_AUTH) \
	CFG(CFG_LFR3_ROAM_PREAUTH_RETRY_COUNT) \
	CFG(CFG_LFR3_ROAM_PREAUTH_NO_ACK_TIMEOUT) \
	CFG(CFG_LFR_FEATURE_ENABLED) \
	CFG(CFG_LFR_MAWC_FEATURE_ENABLED) \
	CFG(CFG_LFR_FAST_TRANSITION_ENABLED) \
	CFG(CFG_LFR_ROAM_RSSI_DIFF) \
	CFG(CFG_LFR_ROAM_BG_RSSI_TH) \
	CFG(CFG_LFR_ENABLE_WES_MODE) \
	CFG(CFG_LFR_ROAM_SCAN_OFFLOAD_ENABLED) \
	CFG(CFG_LFR_NEIGHBOR_SCAN_CHANNEL_LIST) \
	CFG(CFG_LFR_NEIGHBOR_SCAN_TIMER_PERIOD) \
	CFG(CFG_LFR_NEIGHBOR_SCAN_MIN_TIMER_PERIOD) \
	CFG(CFG_LFR_NEIGHBOR_LOOKUP_RSSI_THRESHOLD) \
	CFG(CFG_LFR_OPPORTUNISTIC_SCAN_THRESHOLD_DIFF) \
	CFG(CFG_LFR_ROAM_RESCAN_RSSI_DIFF) \
	CFG(CFG_LFR_NEIGHBOR_SCAN_MIN_CHAN_TIME) \
	CFG(CFG_LFR_NEIGHBOR_SCAN_MAX_CHAN_TIME) \
	CFG(CFG_LFR_NEIGHBOR_SCAN_RESULTS_REFRESH_PERIOD) \
	CFG(CFG_LFR_EMPTY_SCAN_REFRESH_PERIOD) \
	CFG(CFG_LFR_ROAM_BMISS_FIRST_BCNT) \
	CFG(CFG_LFR_ROAM_BMISS_FINAL_BCNT) \
	CFG(CFG_LFR_ROAMING_DFS_CHANNEL) \
	CFG(CFG_LFR_ROAM_SCAN_HI_RSSI_MAXCOUNT) \
	CFG(CFG_LFR_ROAM_SCAN_HI_RSSI_DELTA) \
	CFG(CFG_LFR_ROAM_SCAN_HI_RSSI_DELAY) \
	CFG(CFG_LFR_ROAM_SCAN_HI_RSSI_UB) \
	CFG(CFG_LFR_ROAM_PREFER_5GHZ) \
	CFG(CFG_LFR_ROAM_INTRA_BAND) \
	CFG(CFG_LFR_ROAM_SCAN_N_PROBES) \
	CFG(CFG_LFR_ROAM_SCAN_HOME_AWAY_TIME) \
	CFG(CFG_LFR_DELAY_BEFORE_VDEV_STOP) \
	CFG(CFG_ENABLE_BSS_LOAD_TRIGGERED_ROAM) \
	CFG(CFG_BSS_LOAD_THRESHOLD) \
	CFG(CFG_BSS_LOAD_SAMPLE_TIME) \
	CFG(CFG_LFR3_ROAM_HO_DELAY_FOR_RX) \
	CFG(CFG_LFR_MIN_DELAY_BTW_ROAM_SCAN) \
	CFG(CFG_LFR_ROAM_SCAN_TRIGGER_REASON_BITMASK) \
	CFG(CFG_LFR_ROAM_FT_OPEN_ENABLE) \
	CFG(CFG_LFR_ROAM_FORCE_RSSI_TRIGGER) \
	CFG(CFG_ROAM_SCAN_SCAN_POLICY) \
	CFG(CFG_ROAM_SCAN_INACTIVITY_TIME) \
	CFG(CFG_FT_IM_ROAMING) \
	CFG(CFG_ROAM_INACTIVE_COUNT) \
	CFG(CFG_POST_INACTIVITY_ROAM_SCAN_PERIOD) \
	CFG(CFG_BSS_LOAD_TRIG_5G_RSSI_THRES) \
	CFG(CFG_BSS_LOAD_TRIG_2G_RSSI_THRES) \
	CFG(CFG_LFR_FULL_ROAM_SCAN_REFRESH_PERIOD) \
	ADAPTIVE_11R_ALL \
	ROAM_OFFLOAD_ALL \
	LFR_ESE_ALL \
	LFR_SUBNET_DETECTION_ALL \
	SAE_SINGLE_PMK_ALL \
	ROAM_REASON_VSIE_ALL

#endif /* CFG_MLME_LFR_H__ */
