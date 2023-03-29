/*
 * Copyright (c) 2011-2012, 2014-2020 The Linux Foundation. All rights reserved.
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

#if !defined(__RRMGLOBAL_H)
#define __RRMGLOBAL_H

/**=========================================================================

   \file  rrm_global.h

   \brief Definitions for SME APIs

   ========================================================================*/

#define MAX_MEASUREMENT_REQUEST      5
#define MAX_NUM_CHANNELS             255

#define DEFAULT_RRM_IDX 0

typedef enum eRrmRetStatus {
	eRRM_SUCCESS,
	eRRM_INCAPABLE,
	eRRM_REFUSED,
	eRRM_FAILURE
} tRrmRetStatus;

typedef enum eRrmMsgReqSource {
	eRRM_MSG_SOURCE_LEGACY_ESE = 1, /* legacy ese */
	eRRM_MSG_SOURCE_11K = 2,        /* 11k */
	eRRM_MSG_SOURCE_ESE_UPLOAD = 3, /* ese upload approach */
} tRrmMsgReqSource;

struct sir_channel_info {
	uint8_t reg_class;
	uint8_t chan_num;
	uint32_t chan_freq;
};

typedef struct sSirBeaconReportReqInd {
	uint16_t messageType;   /* eWNI_SME_BEACON_REPORT_REQ_IND */
	uint16_t length;
	uint8_t measurement_idx;
	tSirMacAddr bssId;
	uint16_t measurementDuration[SIR_ESE_MAX_MEAS_IE_REQS]; /* ms */
	uint16_t randomizationInterval; /* ms */
	struct sir_channel_info channel_info;
	/* 0: wildcard */
	tSirMacAddr macaddrBssid;
	/* 0:Passive, 1: Active, 2: table mode */
	uint8_t fMeasurementtype[SIR_ESE_MAX_MEAS_IE_REQS];
	tAniSSID ssId;          /* May be wilcard. */
	uint16_t uDialogToken;
	struct report_channel_list channel_list; /* From AP channel report. */
	tRrmMsgReqSource msgSource;
} tSirBeaconReportReqInd, *tpSirBeaconReportReqInd;

typedef struct sSirBeaconReportXmitInd {
	uint16_t messageType;   /* eWNI_SME_BEACON_REPORT_RESP_XMIT_IND */
	uint16_t length;
	uint8_t measurement_idx;
	tSirMacAddr bssId;
	uint16_t uDialogToken;
	uint8_t fMeasureDone;
	uint16_t duration;
	uint8_t regClass;
	uint8_t numBssDesc;
	struct bss_description *pBssDescription[SIR_BCN_REPORT_MAX_BSS_DESC];
} tSirBeaconReportXmitInd, *tpSirBeaconReportXmitInd;

typedef struct sSirNeighborReportReqInd {
	/* eWNI_SME_NEIGHBOR_REPORT_REQ_IND */
	uint16_t messageType;
	uint16_t length;
	/* For the session. */
	tSirMacAddr bssId;
	/* true - dont include SSID in the request. */
	uint16_t noSSID;
	/* false  include the SSID. It may be null (wildcard) */
	tSirMacSSid ucSSID;
} tSirNeighborReportReqInd, *tpSirNeighborReportReqInd;

typedef struct sSirNeighborBssDescription {
	uint16_t length;
	tSirMacAddr bssId;
	uint8_t regClass;
	uint8_t channel;
	uint8_t phyType;
	union sSirNeighborBssidInfo {
		struct _rrmInfo {
			 /* see IEEE 802.11k Table 7-43a */
			uint32_t fApPreauthReachable:2;
			uint32_t fSameSecurityMode:1;
			uint32_t fSameAuthenticator:1;
			/* see IEEE 802.11k Table 7-95d */
			uint32_t fCapSpectrumMeasurement:1;
			uint32_t fCapQos:1;
			uint32_t fCapApsd:1;
			uint32_t fCapRadioMeasurement:1;
			uint32_t fCapDelayedBlockAck:1;
			uint32_t fCapImmediateBlockAck:1;
			uint32_t fMobilityDomain:1;
			uint32_t reserved:21;
		} rrmInfo;
		struct _eseInfo {
			uint32_t channelBand:8;
			uint32_t minRecvSigPower:8;
			uint32_t apTxPower:8;
			uint32_t roamHysteresis:8;
			uint32_t adaptScanThres:8;

			uint32_t transitionTime:8;
			uint32_t tsfOffset:16;

			uint32_t beaconInterval:16;
			uint32_t reserved:16;
		} eseInfo;
	} bssidInfo;

	/* Optional sub IEs....ignoring for now. */
} tSirNeighborBssDescription, *tpSirNeighborBssDescripton;

typedef struct sSirNeighborReportInd {
	uint16_t messageType;   /* eWNI_SME_NEIGHBOR_REPORT_IND */
	uint16_t length;
	uint8_t sessionId;
	uint8_t measurement_idx;
	uint16_t numNeighborReports;
	tSirMacAddr bssId;      /* For the session. */
	tSirNeighborBssDescription sNeighborBssDescription[1];
} tSirNeighborReportInd, *tpSirNeighborReportInd;

typedef struct sRRMBeaconReportRequestedIes {
	uint8_t num;
	uint8_t *pElementIds;
} tRRMBeaconReportRequestedIes, *tpRRMBeaconReportRequestedIes;

/* Reporting detail defines. */
/* Reference - IEEE Std 802.11k-2008 section 7.3.2.21.6 Table 7-29h */
#define BEACON_REPORTING_DETAIL_NO_FF_IE 0
#define BEACON_REPORTING_DETAIL_ALL_FF_REQ_IE 1
#define BEACON_REPORTING_DETAIL_ALL_FF_IE 2

typedef struct sRRMReq {
	uint8_t measurement_idx; /* Index of the measurement report in frame */
	uint8_t dialog_token;   /* In action frame; */
	uint8_t token;          /* Within individual request; */
	uint8_t type;
	union {
		struct {
			uint8_t reportingDetail;
			uint8_t last_beacon_report_indication;
			tRRMBeaconReportRequestedIes reqIes;
		} Beacon;
	} request;
	uint8_t sendEmptyBcnRpt;
} tRRMReq, *tpRRMReq;

typedef struct sRRMCaps {
	uint8_t LinkMeasurement:1;
	uint8_t NeighborRpt:1;
	uint8_t parallel:1;
	uint8_t repeated:1;
	uint8_t BeaconPassive:1;
	uint8_t BeaconActive:1;
	uint8_t BeaconTable:1;
	uint8_t BeaconRepCond:1;
	uint8_t FrameMeasurement:1;
	uint8_t ChannelLoad:1;
	uint8_t NoiseHistogram:1;
	uint8_t statistics:1;
	uint8_t LCIMeasurement:1;
	uint8_t LCIAzimuth:1;
	uint8_t TCMCapability:1;
	uint8_t triggeredTCM:1;
	uint8_t APChanReport:1;
	uint8_t RRMMIBEnabled:1;
	uint8_t operatingChanMax:3;
	uint8_t nonOperatingChanMax:3;
	uint8_t MeasurementPilot:3;
	uint8_t MeasurementPilotEnabled:1;
	uint8_t NeighborTSFOffset:1;
	uint8_t RCPIMeasurement:1;
	uint8_t RSNIMeasurement:1;
	uint8_t BssAvgAccessDelay:1;
	uint8_t BSSAvailAdmission:1;
	uint8_t AntennaInformation:1;
	uint8_t fine_time_meas_rpt:1;
	uint8_t lci_capability:1;
	uint8_t reserved:4;
} tRRMCaps, *tpRRMCaps;

typedef struct sRrmPEContext {
	uint8_t rrmEnable;
	/*
	 * Used during scan/measurement to store the start TSF.
	 * this is not used directly in beacon reports.
	 */
	uint32_t startTSF[2];
	/*
	 * This value is stored into bssdescription and beacon report
	 * gets it from bss decsription.
	 */
	tRRMCaps rrmEnabledCaps;
	int8_t txMgmtPower;
	/* Dialog token for the request initiated from station. */
	uint8_t DialogToken;
	uint16_t prev_rrm_report_seq_num;
	uint8_t num_active_request;
	tpRRMReq pCurrentReq[MAX_MEASUREMENT_REQUEST];
	uint32_t beacon_rpt_chan_list[MAX_NUM_CHANNELS];
	uint8_t beacon_rpt_chan_num;
} tRrmPEContext, *tpRrmPEContext;

/* 2008 11k spec reference: 18.4.8.5 RCPI Measurement */
#define RCPI_LOW_RSSI_VALUE   (-110)
#define RCPI_MAX_VALUE        (220)
#define CALCULATE_RCPI(rssi)  (((rssi) + 110) * 2)

/* Bit mask are defined as per Draft P802.11REVmc_D4.2 */

/**
 * enum mask_rm_capability_byte1 - mask for supported capability
 * @RM_CAP_LINK_MEASUREMENT: Link Measurement capability
 * @RM_CAP_NEIGHBOR_REPORT: Neighbor report capability
 * @RM_CAP_PARALLEL_MEASUREMENT: Parallel Measurement capability
 * @RM_CAP_REPEATED_MEASUREMENT: Repeated Measurement capability
 * @RM_CAP_BCN_PASSIVE_MEASUREMENT: Beacon passive measurement capability
 * @RM_CAP_BCN_ACTIVE_MEASUREMENT: Beacon active measurement capability
 * @RM_CAP_BCN_TABLE_MEASUREMENT: Beacon table measurement capability
 * @RM_CAP_BCN_MEAS_REPORTING_COND: Beacon measurement reporting conditions
 */
enum mask_rm_capability_byte1 {
	RM_CAP_LINK_MEASUREMENT = (1 << (0)),
	RM_CAP_NEIGHBOR_REPORT = (1 << (1)),
	RM_CAP_PARALLEL_MEASUREMENT = (1 << (2)),
	RM_CAP_REPEATED_MEASUREMENT = (1 << (3)),
	RM_CAP_BCN_PASSIVE_MEASUREMENT = (1 << (4)),
	RM_CAP_BCN_ACTIVE_MEASUREMENT = (1 << (5)),
	RM_CAP_BCN_TABLE_MEASUREMENT = (1 << (6)),
	RM_CAP_BCN_MEAS_REPORTING_COND = (1 << (7)),
};

/**
 * enum mask_rm_capability_byte2 - mask for supported capability
 * @RM_CAP_FRAME_MEASUREMENT: Frame Measurement capability
 * @RM_CAP_CHAN_LOAD_MEASUREMENT: Channel load measurement capability
 * @RM_CAP_NOISE_HIST_MEASUREMENT: Noise Histogram Measurement capability
 * @RM_CAP_STATISTICS_MEASUREMENT: Statistics Measurement capability
 * @RM_CAP_LCI_MEASUREMENT: LCI measurement capability
 * @RM_CAP_LCI_AZIMUTH: LCI Azimuth capability
 * @RM_CAP_TX_CATEGORY_MEASUREMENT: Transmit category measurement capability
 * @RM_CAP_TRIG_TX_CATEGORY_MEASUREMENT:
 *		    Triggered Transmit category measurement capability
 */
enum mask_rm_capability_byte2 {
	RM_CAP_FRAME_MEASUREMENT = (1 << (0)),
	RM_CAP_CHAN_LOAD_MEASUREMENT = (1 << (1)),
	RM_CAP_NOISE_HIST_MEASUREMENT = (1 << (2)),
	RM_CAP_STATISTICS_MEASUREMENT = (1 << (3)),
	RM_CAP_LCI_MEASUREMENT = (1 << (4)),
	RM_CAP_LCI_AZIMUTH = (1 << (5)),
	RM_CAP_TX_CATEGORY_MEASUREMENT = (1 << (6)),
	RM_CAP_TRIG_TX_CATEGORY_MEASUREMENT = (1 << (7)),
};

/**
 * enum mask_rm_capability_byte3 - mask for supported capability
 * @RM_CAP_AP_CHAN_REPORT: AP channel report capability
 * @RM_CAP_RM_MIB: RM MIB capability
 * @RM_CAP_OPER_CHAN_MAX_DURATION_1: OPER_CHAN_MAX_DURATION bit1
 * @RM_CAP_OPER_CHAN_MAX_DURATION_2: OPER_CHAN_MAX_DURATION bit2
 * @RM_CAP_OPER_CHAN_MAX_DURATION_3: OPER_CHAN_MAX_DURATION bit3
 * @RM_CAP_NONOPER_CHAN_MAX_DURATION_1: NONOPER_CHAN_MAX bit1
 * @RM_CAP_NONOPER_CHAN_MAX_DURATION_2: NONOPER_CHAN_MAX bit2
 * @RM_CAP_NONOPER_CHAN_MAX_DURATION_3: NONOPER_CHAN_MAX bit3
 * @RM_CAP_OPER_CHAN_MAX_DURATION: Operating Channel Max Measurement Duration
 * @RM_CAP_NONOPER_CHAN_MAX_DURATION:
 *		    Nonoperating Channel Max Measurement Duration
 */

enum mask_rm_capability_byte3 {
	RM_CAP_AP_CHAN_REPORT = (1 << (0)),
	RM_CAP_RM_MIB = (1 << (1)),
	RM_CAP_OPER_CHAN_MAX_DURATION_1 = (1 << (2)),
	RM_CAP_OPER_CHAN_MAX_DURATION_2 = (1 << (3)),
	RM_CAP_OPER_CHAN_MAX_DURATION_3 = (1 << (4)),
	RM_CAP_NONOPER_CHAN_MAX_DURATION_1 = (1 << (5)),
	RM_CAP_NONOPER_CHAN_MAX_DURATION_2 = (1 << (6)),
	RM_CAP_NONOPER_CHAN_MAX_DURATION_3 = (1 << (7)),
	RM_CAP_OPER_CHAN_MAX_DURATION = (RM_CAP_OPER_CHAN_MAX_DURATION_1 |
					 RM_CAP_OPER_CHAN_MAX_DURATION_2 |
					 RM_CAP_OPER_CHAN_MAX_DURATION_3),
	RM_CAP_NONOPER_CHAN_MAX_DURATION =
				(RM_CAP_NONOPER_CHAN_MAX_DURATION_1 |
				 RM_CAP_NONOPER_CHAN_MAX_DURATION_2 |
				 RM_CAP_NONOPER_CHAN_MAX_DURATION_3),
};

/**
 * enum mask_rm_capability_byte4 - mask for supported capability
 * @RM_CAP_MEASUREMENT_PILOT_1: MEASUREMENT_PILOT bit1
 * @RM_CAP_MEASUREMENT_PILOT_2: MEASUREMENT_PILOT bit2
 * @RM_CAP_MEASUREMENT_PILOT_3: MEASUREMENT_PILOT bit3
 * @RM_CAP_MEAS_PILOT_TX_INFO: Measurement Pilot Transmission Capability
 * @RM_CAP_NEIGHBOR_RPT_TSF_OFFSET: Neighbor Report TSF Offset Capability
 * @RM_CAP_RCPI_MEASUREMENT: RCPI Measurement Capability
 * @RM_CAP_RSNI_MEASUREMENT: RSNI Measurement Capability
 * @RM_CAP_BSS_AVG_ACCESS_DELAY: BSS Average Access Delay Capability
 * @RM_CAP_MEASUREMENT_PILOT: Measurement pilot capability
 */

enum mask_rm_capability_byte4 {
	RM_CAP_MEASUREMENT_PILOT_1 = (1 << (0)),
	RM_CAP_MEASUREMENT_PILOT_2 = (1 << (1)),
	RM_CAP_MEASUREMENT_PILOT_3 = (1 << (2)),
	RM_CAP_MEAS_PILOT_TX_INFO = (1 << (3)),
	RM_CAP_NEIGHBOR_RPT_TSF_OFFSET = (1 << (4)),
	RM_CAP_RCPI_MEASUREMENT1 = (1 << (5)),
	RM_CAP_RSNI_MEASUREMENT = (1 << (6)),
	RM_CAP_BSS_AVG_ACCESS_DELAY = (1 << (7)),
	RM_CAP_MEASUREMENT_PILOT = (RM_CAP_MEASUREMENT_PILOT_1 |
				    RM_CAP_MEASUREMENT_PILOT_2 |
				    RM_CAP_MEASUREMENT_PILOT_3),
};

/**
 * enum mask_rm_capability_byte5 - mask for supported capability
 * @RM_CAP_BSS_AVAIL_ADMISSION: BSS Available Admission Capacity Capability
 * @RM_CAP_ANTENNA: Antenna Capability
 * @RM_CAP_FTM_RANGE_REPORT: FTM Range Report Capability
 * @RM_CAP_CIVIC_LOC_MEASUREMENT: Civic Location Measurement capability
 *
 * 4 bits are reserved
 */
enum mask_rm_capability_byte5 {
	RM_CAP_BSS_AVAIL_ADMISSION  = (1 << (0)),
	RM_CAP_ANTENNA = (1 << (1)),
	RM_CAP_FTM_RANGE_REPORT = (1 << (2)),
	RM_CAP_CIVIC_LOC_MEASUREMENT = (1 << (3)),
};

#endif /* #if defined __RRMGLOBAL_H */
