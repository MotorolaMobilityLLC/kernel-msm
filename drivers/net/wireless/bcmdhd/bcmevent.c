/*
 * bcmevent read-only data shared by kernel or app layers
 *
 * Copyright (C) 1999-2013, Broadcom Corporation
 *
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 *
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 *
 *      Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a license
 * other than the GPL, without Broadcom's express prior written consent.
 * $Id: bcmevent.c 389384 2013-03-06 12:20:17Z $
 */

#include <typedefs.h>
#include <bcmutils.h>
#include <bcmendian.h>
#include <proto/ethernet.h>
#include <proto/bcmeth.h>
#include <proto/bcmevent.h>
#include <proto/dnglevent.h>
#include <proto/802.11.h>

#if WLC_E_LAST != 144
#error "You need to add an entry to bcmevent_names[] for the new event"
#endif

const bcmevent_name_t bcmevent_names[] = {
	{ WLC_E_SET_SSID, "SET_SSID" },
	{ WLC_E_JOIN, "JOIN" },
	{ WLC_E_START, "START" },
	{ WLC_E_AUTH, "AUTH" },
	{ WLC_E_AUTH_IND, "AUTH_IND" },
	{ WLC_E_DEAUTH, "DEAUTH" },
	{ WLC_E_DEAUTH_IND, "DEAUTH_IND" },
	{ WLC_E_ASSOC, "ASSOC" },
	{ WLC_E_ASSOC_IND, "ASSOC_IND" },
	{ WLC_E_REASSOC, "REASSOC" },
	{ WLC_E_REASSOC_IND, "REASSOC_IND" },
	{ WLC_E_DISASSOC, "DISASSOC" },
	{ WLC_E_DISASSOC_IND, "DISASSOC_IND" },
	{ WLC_E_QUIET_START, "START_QUIET" },
	{ WLC_E_QUIET_END, "END_QUIET" },
	{ WLC_E_BEACON_RX, "BEACON_RX" },
	{ WLC_E_LINK, "LINK" },
	{ WLC_E_MIC_ERROR, "MIC_ERROR" },
	{ WLC_E_NDIS_LINK, "NDIS_LINK" },
	{ WLC_E_ROAM, "ROAM" },
	{ WLC_E_TXFAIL, "TXFAIL" },
	{ WLC_E_PMKID_CACHE, "PMKID_CACHE" },
	{ WLC_E_RETROGRADE_TSF, "RETROGRADE_TSF" },
	{ WLC_E_PRUNE, "PRUNE" },
	{ WLC_E_AUTOAUTH, "AUTOAUTH" },
	{ WLC_E_EAPOL_MSG, "EAPOL_MSG" },
	{ WLC_E_SCAN_COMPLETE, "SCAN_COMPLETE" },
	{ WLC_E_ADDTS_IND, "ADDTS_IND" },
	{ WLC_E_DELTS_IND, "DELTS_IND" },
	{ WLC_E_BCNSENT_IND, "BCNSENT_IND" },
	{ WLC_E_BCNRX_MSG, "BCNRX_MSG" },
	{ WLC_E_BCNLOST_MSG, "BCNLOST_IND" },
	{ WLC_E_ROAM_PREP, "ROAM_PREP" },
	{ WLC_E_PFN_NET_FOUND, "PFNFOUND_IND" },
	{ WLC_E_PFN_NET_LOST, "PFNLOST_IND" },
#if defined(IBSS_PEER_DISCOVERY_EVENT)
	{ WLC_E_IBSS_ASSOC, "IBSS_ASSOC" },
#endif /* defined(IBSS_PEER_DISCOVERY_EVENT) */
	{ WLC_E_RADIO, "RADIO" },
	{ WLC_E_PSM_WATCHDOG, "PSM_WATCHDOG" },
	{ WLC_E_PROBREQ_MSG, "PROBE_REQ_MSG" },
	{ WLC_E_SCAN_CONFIRM_IND, "SCAN_CONFIRM_IND" },
	{ WLC_E_PSK_SUP, "PSK_SUP" },
	{ WLC_E_COUNTRY_CODE_CHANGED, "CNTRYCODE_IND" },
	{ WLC_E_EXCEEDED_MEDIUM_TIME, "EXCEEDED_MEDIUM_TIME" },
	{ WLC_E_ICV_ERROR, "ICV_ERROR" },
	{ WLC_E_UNICAST_DECODE_ERROR, "UNICAST_DECODE_ERROR" },
	{ WLC_E_MULTICAST_DECODE_ERROR, "MULTICAST_DECODE_ERROR" },
	{ WLC_E_TRACE, "TRACE" },
	{ WLC_E_IF, "IF" },
#ifdef WLP2P
	{ WLC_E_P2P_DISC_LISTEN_COMPLETE, "WLC_E_P2P_DISC_LISTEN_COMPLETE" },
#endif
	{ WLC_E_RSSI, "RSSI" },
	{ WLC_E_PFN_SCAN_COMPLETE, "SCAN_COMPLETE" },
	{ WLC_E_EXTLOG_MSG, "EXTERNAL LOG MESSAGE" },
#ifdef WIFI_ACT_FRAME
	{ WLC_E_ACTION_FRAME, "ACTION_FRAME" },
	{ WLC_E_ACTION_FRAME_RX, "ACTION_FRAME_RX" },
	{ WLC_E_ACTION_FRAME_COMPLETE, "ACTION_FRAME_COMPLETE" },
#endif
#if 0 && (0>= 0x0620)
	{ WLC_E_PRE_ASSOC_IND, "ASSOC_RECV" },
	{ WLC_E_PRE_REASSOC_IND, "REASSOC_RECV" },
	{ WLC_E_CHANNEL_ADOPTED, "CHANNEL_ADOPTED" },
	{ WLC_E_AP_STARTED, "AP_STARTED" },
	{ WLC_E_DFS_AP_STOP, "DFS_AP_STOP" },
	{ WLC_E_DFS_AP_RESUME, "DFS_AP_RESUME" },
	{ WLC_E_ASSOC_IND_NDIS, "ASSOC_IND_NDIS"},
	{ WLC_E_REASSOC_IND_NDIS, "REASSOC_IND_NDIS"},
	{ WLC_E_ACTION_FRAME_RX_NDIS, "WLC_E_ACTION_FRAME_RX_NDIS" },
	{ WLC_E_AUTH_REQ, "WLC_E_AUTH_REQ" },
	{ WLC_E_IBSS_COALESCE, "IBSS COALESCE" },
#endif 
	{ WLC_E_ESCAN_RESULT, "WLC_E_ESCAN_RESULT" },
	{ WLC_E_ACTION_FRAME_OFF_CHAN_COMPLETE, "WLC_E_AF_OFF_CHAN_COMPLETE" },
#ifdef WLP2P
	{ WLC_E_PROBRESP_MSG, "PROBE_RESP_MSG" },
	{ WLC_E_P2P_PROBREQ_MSG, "P2P PROBE_REQ_MSG" },
#endif
#ifdef PROP_TXSTATUS
	{ WLC_E_FIFO_CREDIT_MAP, "FIFO_CREDIT_MAP" },
#endif
	{ WLC_E_WAKE_EVENT, "WAKE_EVENT" },
	{ WLC_E_DCS_REQUEST, "DCS_REQUEST" },
	{ WLC_E_RM_COMPLETE, "RM_COMPLETE" },
#ifdef WLMEDIA_HTSF
	{ WLC_E_HTSFSYNC, "HTSF_SYNC_EVENT" },
#endif
	{ WLC_E_OVERLAY_REQ, "OVERLAY_REQ_EVENT" },
	{ WLC_E_CSA_COMPLETE_IND, "WLC_E_CSA_COMPLETE_IND"},
	{ WLC_E_EXCESS_PM_WAKE_EVENT, "EXCESS_PM_WAKE_EVENT" },
	{ WLC_E_PFN_SCAN_NONE, "PFN_SCAN_NONE" },
	{ WLC_E_PFN_SCAN_ALLGONE, "PFN_SCAN_ALLGONE" },
#ifdef SOFTAP
	{ WLC_E_GTK_PLUMBED, "GTK_PLUMBED" },
#endif
	{ WLC_E_ASSOC_REQ_IE, "ASSOC_REQ_IE" },
	{ WLC_E_ASSOC_RESP_IE, "ASSOC_RESP_IE" },
	{ WLC_E_ACTION_FRAME_RX_NDIS, "WLC_E_ACTION_FRAME_RX_NDIS" },
#ifdef WLTDLS
	{ WLC_E_TDLS_PEER_EVENT, "TDLS_PEER_EVENT" },
#endif /* WLTDLS */
	{ WLC_E_NATIVE, "NATIVE" },
#ifdef WLPKTDLYSTAT
	{ WLC_E_PKTDELAY_IND, "PKTDELAY_IND" },
#endif /* WLPKTDLYSTAT */
	{ WLC_E_SERVICE_FOUND, "SERVICE_FOUND" },
	{ WLC_E_GAS_FRAGMENT_RX, "GAS_FRAGMENT_RX" },
	{ WLC_E_GAS_COMPLETE, "GAS_COMPLETE" },
	{ WLC_E_P2PO_ADD_DEVICE, "P2PO_DEV_FOUND" },
	{ WLC_E_P2PO_DEL_DEVICE, "P2PO_DEV_LOST" },
#ifdef WLWNM
	{ WLC_E_WNM_STA_SLEEP, "WMM_STA_SLEEP" },
#endif /* WLWNM */
#if defined(WL_PROXDETECT)
	{ WLC_E_PROXD, "WLC_E_PROXD" },
#endif
	{ WLC_E_CCA_CHAN_QUAL, "CCA_BASED_CHANNEL_QUALITY" },
#ifdef GSCAN_SUPPORT
	{ WLC_E_PFN_GSCAN_FULL_RESULT, "PFN_GSCAN_FULL_RESULT"},
	{ WLC_E_PFN_SWC, "PFN_SIGNIFICANT_WIFI_CHANGE"},
	{ WLC_E_PFN_SSID_EXT, "PFN_SSID_EXT"},
	{ WLC_E_ROAM_EXP_EVENT, "ROAM_EXP_EVENT"},
#endif /* GSCAN_SUPPORT */
};

const int bcmevent_names_size = ARRAYSIZE(bcmevent_names);

const char *bcmevent_get_name(uint event_type)
{
	const char *event_name = NULL;

	uint idx;
	for (idx = 0; idx < bcmevent_names_size; idx++) {

		if (bcmevent_names[idx].event == event_type) {
			event_name = bcmevent_names[idx].name;
			break;
		}
	}

	/* if we find an event name in the array, return it.
	 * otherwise return unknown string.
	 */
	return ((event_name) ? event_name : "Unknown Event");
}

/*
 * Validate if the event is proper and if valid copy event header to event.
 * If proper event pointer is passed, to just validate, pass NULL to event.
 *
 * Return values are
 *	BCME_OK - It is a BRCM event or BRCM dongle event
 *	BCME_NOTFOUND - Not BRCM, not an event, may be okay
 *	BCME_BADLEN - Bad length, should not process, just drop
 */
int
is_wlc_event_frame(void *pktdata, uint pktlen, uint16 exp_usr_subtype,
	bcm_event_msg_u_t *out_event)
{
	uint16 len;
	uint16 subtype;
	uint16 usr_subtype;
	bcm_event_t *bcm_event;
	uint8 *pktend;
	int err = BCME_OK;

	pktend = (uint8 *)pktdata + pktlen;
	bcm_event = (bcm_event_t *)pktdata;

	/* only care about 16-bit subtype / length versions */
	if ((uint8 *)&bcm_event->bcm_hdr < pktend) {
		uint8 short_subtype = *(uint8 *)&bcm_event->bcm_hdr;
		if (!(short_subtype & 0x80)) {
			err = BCME_NOTFOUND;
			goto done;
		}
	}

	/* must have both ether_header and bcmeth_hdr */
	if (pktlen < OFFSETOF(bcm_event_t, event)) {
		err = BCME_BADLEN;
		goto done;
	}

	/* check length in bcmeth_hdr */
	len = ntoh16_ua((void *)&bcm_event->bcm_hdr.length);
	if (((uint8 *)&bcm_event->bcm_hdr.version + len) > pktend) {
		err = BCME_BADLEN;
		goto done;
	}

	/* match on subtype, oui and usr subtype for BRCM events */
	subtype = ntoh16_ua((void *)&bcm_event->bcm_hdr.subtype);
	if (subtype != BCMILCP_SUBTYPE_VENDOR_LONG) {
		err = BCME_NOTFOUND;
		goto done;
	}

	if (bcmp(BRCM_OUI, &bcm_event->bcm_hdr.oui[0], DOT11_OUI_LEN)) {
		err = BCME_NOTFOUND;
		goto done;
	}

	/* if it is a bcm_event or bcm_dngl_event_t, validate it */
	usr_subtype = ntoh16_ua((void *)&bcm_event->bcm_hdr.usr_subtype);
	switch (usr_subtype) {
	case BCMILCP_BCM_SUBTYPE_EVENT:
		if (pktlen < sizeof(bcm_event_t)) {
			err = BCME_BADLEN;
			goto done;
		}

		len = sizeof(bcm_event_t) + ntoh32_ua((void *)&bcm_event->event.datalen);
		if ((uint8 *)pktdata + len > pktend) {
			err = BCME_BADLEN;
			goto done;
		}

		if (exp_usr_subtype && (exp_usr_subtype != usr_subtype)) {
			err = BCME_NOTFOUND;
			goto done;
		}

		if (out_event) {
			/* ensure BRCM event pkt aligned */
			memcpy(&out_event->event, &bcm_event->event, sizeof(wl_event_msg_t));
		}

		break;
	case BCMILCP_BCM_SUBTYPE_DNGLEVENT:
		if (pktlen < sizeof(bcm_dngl_event_t)) {
			err = BCME_BADLEN;
			goto done;
		}

		len = sizeof(bcm_dngl_event_t) +
			ntoh16_ua((void *)&((bcm_dngl_event_t *)pktdata)->dngl_event.datalen);
		if ((uint8 *)pktdata + len > pktend) {
			err = BCME_BADLEN;
			goto done;
		}

		if (exp_usr_subtype && (exp_usr_subtype != usr_subtype)) {
			err = BCME_NOTFOUND;
			goto done;
		}

		if (out_event) {
			/* ensure BRCM dngl event pkt aligned */
			memcpy(&out_event->dngl_event, &((bcm_dngl_event_t *)pktdata)->dngl_event,
				sizeof(bcm_dngl_event_msg_t));
		}

		break;
	default:
		err = BCME_NOTFOUND;
		goto done;
	}

done:
	return err;
}
