/*
 * Copyright (c) 2018-2020 The Linux Foundation. All rights reserved.
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

#if !defined(CONFIG_TDLS_H__)
#define CONFIG_TDLS_H__

#include "cfg_define.h"
#include "cfg_converged.h"
#include "qdf_types.h"

/*
 * <ini>
 * gTDLSUapsdMask - ACs to setup U-APSD for TDLS Sta.
 * @Min: 0
 * @Max: 0x0F
 * @Default: 0x0F
 *
 * This ini is used to configure the ACs for which mask needs to be enabled.
 * 0x1: Background	0x2: Best effort
 * 0x4: Video		0x8: Voice
 *
 * Related: gEnableTDLSSupport.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_QOS_WMM_UAPSD_MASK CFG_INI_UINT( \
	"gTDLSUapsdMask", \
	0, \
	0x0F, \
	0x0F, \
	CFG_VALUE_OR_DEFAULT, \
	"ACs to setup U-APSD for TDLS Sta")

/*
 * <ini>
 * gEnableTDLSBufferSta - Controls the TDLS buffer.
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to control the TDLS buffer.
 * Buffer STA is not enabled in CLD 2.0 yet.
 *
 * Related: gEnableTDLSSupport.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_BUF_STA_ENABLED CFG_INI_BOOL( \
	"gEnableTDLSBufferSta", \
	1, \
	"Controls the TDLS buffer")

/*
 * <ini>
 * gTDLSPuapsdInactivityTime - Peer UAPSD Inactivity time.
 * @Min: 0
 * @Max: 10
 * @Default: 0
 *
 * This ini is used to configure peer uapsd inactivity time(in ms).
 *
 * Related: gEnableTDLSSupport.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_PUAPSD_INACT_TIME CFG_INI_UINT( \
	"gTDLSPuapsdInactivityTime", \
	0, \
	10, \
	0, \
	CFG_VALUE_OR_DEFAULT, \
	"Peer UAPSD Inactivity time")

/*
 * <ini>
 * gTDLSPuapsdRxFrameThreshold - Peer UAPSD Rx frame threshold.
 * @Min: 10
 * @Max: 20
 * @Default: 10
 *
 * This ini is used to configure maximum Rx frame during SP.
 *
 * Related: gEnableTDLSSupport.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_RX_FRAME_THRESHOLD CFG_INI_UINT( \
	"gTDLSPuapsdRxFrameThreshold", \
	10, \
	20, \
	10, \
	CFG_VALUE_OR_DEFAULT, \
	"Peer UAPSD Rx frame threshold")

/*
 * <ini>
 * gEnableTDLSOffChannel - Enables off-channel support for TDLS link.
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable/disable off-channel support for TDLS link.
 *
 * Related: gEnableTDLSSupport.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_OFF_CHANNEL_ENABLED CFG_INI_BOOL( \
	"gEnableTDLSOffChannel", \
	0, \
	"Enables off-channel support for TDLS")

/*
 * <ini>
 * gEnableTDLSSupport - Enable support for TDLS.
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/disable TDLS support.
 *
 * Related: None.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_SUPPORT_ENABLE CFG_INI_BOOL( \
	"gEnableTDLSSupport", \
	1, \
	"enable/disable TDLS support")

/*
 * <ini>
 * gEnableTDLSImplicitTrigger - Enable Implicit TDLS.
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/disable implicit TDLS.
 * CLD driver initiates TDLS Discovery towards a peer whenever TDLS Setup
 * criteria (throughput and RSSI thresholds) is met and then it tears down
 * TDLS when teardown criteria (idle packet count and RSSI) is met.
 *
 * Related: gEnableTDLSSupport.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_IMPLICIT_TRIGGER CFG_INI_BOOL( \
	"gEnableTDLSImplicitTrigger", \
	1, \
	"enable/disable implicit TDLS")

/*
 * <ini>
 * gTDLSTxStatsPeriod - TDLS TX statistics time period.
 * @Min: 1000
 * @Max: 4294967295
 * @Default: 2000
 *
 * This ini is used to configure the time period (in ms) to evaluate whether
 * the number of Tx/Rx packets exceeds TDLSTxPacketThreshold and triggers a
 * TDLS Discovery request.
 *
 * Related: gEnableTDLSSupport.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_TX_STATS_PERIOD CFG_INI_UINT( \
	"gTDLSTxStatsPeriod", \
	1000, \
	4294967295UL, \
	2000, \
	CFG_VALUE_OR_DEFAULT, \
	"TDLS TX statistics time period")

/*
 * <ini>
 * gTDLSTxPacketThreshold - Tx/Rx Packet threshold for initiating TDLS.
 * @Min: 0
 * @Max: 4294967295
 * @Default: 40
 *
 * This ini is used to configure the number of Tx/Rx packets during the
 * period of gTDLSTxStatsPeriod when exceeded, a TDLS Discovery request
 * is triggered.
 * Related: gEnableTDLSSupport.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_TX_PACKET_THRESHOLD CFG_INI_UINT( \
	"gTDLSTxPacketThreshold", \
	0, \
	4294967295UL, \
	40, \
	CFG_VALUE_OR_DEFAULT, \
	"Tx/Rx Packet threshold for initiating TDLS")

/*
 * <ini>
 * gTDLSMaxDiscoveryAttempt - Attempts for sending TDLS discovery requests.
 * @Min: 1
 * @Max: 100
 * @Default: 5
 *
 * This ini is used to configure the number of failures of discover request,
 * when exceeded, the peer is assumed to be not TDLS capable and no further
 * TDLS Discovery request is made.
 *
 * Related: gEnableTDLSSupport.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_MAX_DISCOVERY_ATTEMPT CFG_INI_UINT( \
	"gTDLSMaxDiscoveryAttempt", \
	1, \
	100, \
	5, \
	CFG_VALUE_OR_DEFAULT, \
	"Attempts for sending TDLS discovery requests")

/*
 * gTDLSMaxPeerCount - Max TDLS connected peer count
 * @Min: 1
 * @Max: 8
 * @Default: 8
 *
 * This ini is used to configure the max connected TDLS peer count.
 *
 * Related: gEnableTDLSSupport.
 *
 * Supported Feature: TDLS
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_TDLS_MAX_PEER_COUNT CFG_INI_UINT( \
	"gTDLSMaxPeerCount", \
	1, \
	8, \
	8, \
	CFG_VALUE_OR_DEFAULT, \
	"Max TDLS peer count")

/*
 * <ini>
 * gTDLSIdleTimeout - Duration within which number of TX / RX frames meet the
 * criteria for TDLS teardown.
 * @Min: 500
 * @Max: 40000
 * @Default: 5000
 *
 * This ini is used to configure the time period (in ms) to evaluate whether
 * the number of Tx/Rx packets exceeds gTDLSIdlePacketThreshold and thus meets
 * criteria for TDLS teardown.
 * Teardown notification interval (gTDLSIdleTimeout) should be multiple of
 * setup notification (gTDLSTxStatsPeriod) interval.
 * e.g.
 *      if setup notification (gTDLSTxStatsPeriod) interval = 500, then
 *      teardown notification (gTDLSIdleTimeout) interval should be 1000,
 *      1500, 2000, 2500...
 *
 * Related: gEnableTDLSSupport.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 */
#define CFG_TDLS_IDLE_TIMEOUT CFG_INI_UINT( \
	"gTDLSIdleTimeout", \
	500, \
	40000, \
	5000, \
	CFG_VALUE_OR_DEFAULT, \
	"this is idle time period")

/*
 * <ini>
 * gTDLSIdlePacketThreshold - Number of idle packet.
 * @Min: 0
 * @Max: 40000
 * @Default: 3
 *
 * This ini is used to configure the number of Tx/Rx packet, below which
 * within last gTDLSTxStatsPeriod period is considered as idle condition.
 *
 * Related: gEnableTDLSSupport.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_IDLE_PACKET_THRESHOLD CFG_INI_UINT( \
	"gTDLSIdlePacketThreshold", \
	0, \
	40000, \
	3, \
	CFG_VALUE_OR_DEFAULT, \
	"Number of idle packet")

/*
 * <ini>
 * gTDLSRSSITriggerThreshold - RSSI threshold for TDLS connection.
 * @Min: -120
 * @Max: 0
 * @Default: -75
 *
 * This ini is used to configure the absolute value (in dB) of the peer RSSI,
 * below which a TDLS setup request is triggered.
 *
 * Related: gEnableTDLSSupport.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_RSSI_TRIGGER_THRESHOLD CFG_INI_INT( \
	"gTDLSRSSITriggerThreshold", \
	-120, \
	0, \
	-75, \
	CFG_VALUE_OR_DEFAULT, \
	"RSSI threshold for TDLS connection")

/*
 * <ini>
 * gTDLSRSSITeardownThreshold - RSSI threshold for TDLS teardown.
 * @Min: -120
 * @Max: 0
 * @Default: -75
 *
 * This ini is used to configure the absolute value (in dB) of the peer RSSI,
 * when exceed, a TDLS teardown is triggered.
 *
 * Related: gEnableTDLSSupport.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_RSSI_TEARDOWN_THRESHOLD CFG_INI_INT( \
	"gTDLSRSSITeardownThreshold", \
	-120, \
	0, \
	-75, \
	CFG_VALUE_OR_DEFAULT, \
	"RSSI threshold for TDLS teardown")

/*
 * <ini>
 * gTDLSRSSIDelta - Delta value for the peer RSSI that can trigger teardown.
 * @Min: -30
 * @Max: 0
 * @Default: -20
 *
 * This ini is used to configure delta for peer RSSI such that if Peer RSSI
 * is less than AP RSSI plus delta will trigger a teardown.
 *
 * Related: gEnableTDLSSupport.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_RSSI_DELTA CFG_INI_INT( \
	"gTDLSRSSIDelta", \
	-30, \
	0, \
	-20, \
	CFG_VALUE_OR_DEFAULT, \
	"Delta value for the peer RSSI that can trigger teardown")

/*
 * <ini>
 * gTDLSPrefOffChanNum - Preferred TDLS channel number when off-channel support
 * is enabled.
 * @Min: 1
 * @Max: 165
 * @Default: 36
 *
 * This ini is used to configure preferred TDLS channel number when off-channel
 * support is enabled.
 *
 * Related: gEnableTDLSSupport, gEnableTDLSOffChannel.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_PREFERRED_OFF_CHANNEL_NUM CFG_INI_UINT( \
	"gTDLSPrefOffChanNum", \
	1, \
	165, \
	36, \
	CFG_VALUE_OR_DEFAULT, \
	"Preferred TDLS channel number")

/*
 * <ini>
 * gTDLSPrefOffChanBandwidth - Preferred TDLS channel bandwidth when
 * off-channel support is enabled.
 * @Min: 0x01
 * @Max: 0x0F
 * @Default: 0x07
 *
 * This ini is used to configure preferred TDLS channel bandwidth when
 * off-channel support is enabled.
 * 0x1: 20 MHz	0x2: 40 MHz	0x4: 80 MHz	0x8: 160 MHz
 * When more than one bits are set then firmware starts from the highest and
 * selects one based on capability of peer.
 *
 * Related: gEnableTDLSSupport, gEnableTDLSOffChannel.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_PREFERRED_OFF_CHANNEL_BW CFG_INI_UINT( \
	"gTDLSPrefOffChanBandwidth", \
	1, \
	15, \
	7, \
	CFG_VALUE_OR_DEFAULT, \
	"Preferred TDLS channel bandwidth")

/*
 * <ini>
 * gTDLSPuapsdPTIWindow - This ini is used to configure peer traffic indication
 * window.
 * @Min: 1
 * @Max: 5
 * @Default: 2
 *
 * This ini is used to configure buffering time in number of beacon intervals.
 *
 * Related: gEnableTDLSSupport.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_PUAPSD_PEER_TRAFFIC_IND_WINDOW CFG_INI_UINT( \
	"gTDLSPuapsdPTIWindow", \
	1, \
	5, \
	2, \
	CFG_VALUE_OR_DEFAULT, \
	"This ini is used to configure peer traffic indication")

/*
 * <ini>
 * gTDLSPuapsdPTIWindow - This ini is used to configure peer traffic indication
 * window.
 * @Min: 1
 * @Max: 5
 * @Default: 2
 *
 * This ini is used to configure buffering time in number of beacon intervals.
 *
 * Related: gEnableTDLSSupport.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_PUAPSD_PEER_TRAFFIC_RSP_TIMEOUT CFG_INI_UINT( \
	"gTDLSPuapsdPTRTimeout", \
	0, \
	10000, \
	5000, \
	CFG_VALUE_OR_DEFAULT, \
	"Peer Traffic Response timer duration in ms")

/*
 * <ini>
 * gTDLSExternalControl - Enable external TDLS control.
 * @Min: 0
 * @Max: 2
 * @Default: 1
 *
 * This ini is used to enable/disable external TDLS control.
 * TDLS external control works with TDLS implicit trigger. TDLS external
 * control allows a user to add a MAC address of potential TDLS peers so
 * that the CLD driver can initiate implicit TDLS setup to only those peers
 * when criteria for TDLS setup (throughput and RSSI threshold) is met.
 * There are two flavors of external control supported. If control default
 * is set 1 it means strict external control where only for configured
 * tdls peer mac address tdls link will be established. If control default
 * is set 2 liberal tdls external control is needed which means
 * tdls link will be established with configured peer mac address as well
 * as any other peer which supports tdls.
 *
 * Related: gEnableTDLSSupport, gEnableTDLSImplicitTrigger.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_EXTERNAL_CONTROL CFG_INI_UINT( \
	"gTDLSExternalControl", \
	0, \
	2, \
	1, \
	CFG_VALUE_OR_DEFAULT, \
	"Enable external TDLS control")

/*
 * <ini>
 * gEnableTDLSWmmMode - Enables WMM support over TDLS link.
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/disable WMM support over TDLS link.
 * This is required to be set to 1 for any TDLS and uAPSD functionality.
 *
 * Related: gEnableTDLSSupport.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_WMM_MODE_ENABLE CFG_INI_BOOL( \
	"gEnableTDLSWmmMode", \
	1, \
	"Enables WMM support over TDLS link")

/*
 * <ini>
 * gEnableTDLSScan - Allow scan and maintain TDLS link.
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/disable TDLS scan.
 *  0: If peer is not buffer STA capable and device is not sleep STA
 *     capable, then teardown TDLS link when scan is initiated. If peer
 *     is buffer STA and we can be sleep STA then TDLS link is maintained
 *     during scan.
 *  1: Maintain TDLS link and allow scan even if peer is not buffer STA
 *     capable and device is not sleep STA capable. There will be loss of
 *     Rx pkts since peer would not know when device moves away from tdls
 *     channel. Tx on TDLS link would stop when device moves away from tdls
 *     channel.
 *
 * Related: gEnableTDLSSupport.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_SCAN_ENABLE CFG_INI_BOOL( \
	"gEnableTDLSScan", \
	1, \
	"Allow scan and maintain TDLS link")

/*
 * <ini>
 * gTDLSPeerKickoutThreshold - TDLS peer kick out threshold to firmware.
 * @Min: 10
 * @Max: 5000
 * @Default: 96
 *
 * This ini is used to configure TDLS peer kick out threshold to firmware.
 *     Firmware will use this value to determine, when to send TDLS
 *     peer kick out event to host.
 *     E.g.
 *        if peer kick out threshold is 10, then firmware will wait for 10
 *        consecutive packet failures and then send TDLS kick out
 *        notification to host driver
 *
 * Related: gEnableTDLSSupport.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_PEER_KICKOUT_THRESHOLD CFG_INI_UINT( \
	"gTDLSPeerKickoutThreshold", \
	10, \
	5000, \
	96, \
	CFG_VALUE_OR_DEFAULT, \
	"TDLS peer kick out threshold to firmware")
/*
 * <ini>
 * gTDLSDiscoveryWakeTimeout - TDLS discovery WAKE timeout in ms.
 * @Min: 10
 * @Max: 5000
 * @Default: 96
 *
 * DUT will wake until this timeout to receive TDLS discovery response
 * from peer. If tdls_discovery_wake_timeout is 0x0, the DUT will
 * choose autonomously what wake timeout value to use.
 *
 *
 * Related: gEnableTDLSSupport.
 *
 * Supported Feature: TDLS
 *
 * Usage: External
 *
 * </ini>
 */
 #define CFG_TDLS_DISCOVERY_WAKE_TIMEOUT CFG_INI_UINT( \
	"gTDLSDiscoveryWakeTimeout", \
	0, \
	2000, \
	200, \
	CFG_VALUE_OR_DEFAULT, \
	"TDLS peer kick out threshold to firmware")

/*
 * <ini>
 * gTDLSEnableDeferTime - Timer to defer for enabling TDLS on P2P listen.
 * @Min: 500
 * @Max: 6000
 * @Default: 2000
 *
 * This ini is used to set the timer to defer for enabling TDLS on P2P
 * listen (value in milliseconds).
 *
 * Related: gEnableTDLSSupport.
 *
 * Supported Feature: TDLS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TDLS_ENABLE_DEFER_TIMER CFG_INI_UINT( \
	"gTDLSEnableDeferTime", \
	500, \
	6000, \
	2000, \
	CFG_VALUE_OR_DEFAULT, \
	"Timer to defer for enabling TDLS on P2P listen")

#define CFG_TDLS_ALL \
	CFG(CFG_TDLS_QOS_WMM_UAPSD_MASK) \
	CFG(CFG_TDLS_BUF_STA_ENABLED) \
	CFG(CFG_TDLS_PUAPSD_INACT_TIME) \
	CFG(CFG_TDLS_RX_FRAME_THRESHOLD) \
	CFG(CFG_TDLS_OFF_CHANNEL_ENABLED) \
	CFG(CFG_TDLS_SUPPORT_ENABLE) \
	CFG(CFG_TDLS_IMPLICIT_TRIGGER) \
	CFG(CFG_TDLS_TX_STATS_PERIOD) \
	CFG(CFG_TDLS_TX_PACKET_THRESHOLD) \
	CFG(CFG_TDLS_MAX_DISCOVERY_ATTEMPT) \
	CFG(CFG_TDLS_MAX_PEER_COUNT) \
	CFG(CFG_TDLS_IDLE_TIMEOUT) \
	CFG(CFG_TDLS_IDLE_PACKET_THRESHOLD) \
	CFG(CFG_TDLS_RSSI_TRIGGER_THRESHOLD) \
	CFG(CFG_TDLS_RSSI_TEARDOWN_THRESHOLD) \
	CFG(CFG_TDLS_RSSI_DELTA) \
	CFG(CFG_TDLS_PREFERRED_OFF_CHANNEL_NUM) \
	CFG(CFG_TDLS_PREFERRED_OFF_CHANNEL_BW) \
	CFG(CFG_TDLS_PUAPSD_PEER_TRAFFIC_IND_WINDOW) \
	CFG(CFG_TDLS_PUAPSD_PEER_TRAFFIC_RSP_TIMEOUT) \
	CFG(CFG_TDLS_EXTERNAL_CONTROL) \
	CFG(CFG_TDLS_WMM_MODE_ENABLE) \
	CFG(CFG_TDLS_SCAN_ENABLE) \
	CFG(CFG_TDLS_PEER_KICKOUT_THRESHOLD) \
	CFG(CFG_TDLS_DISCOVERY_WAKE_TIMEOUT) \
	CFG(CFG_TDLS_ENABLE_DEFER_TIMER)

#endif
