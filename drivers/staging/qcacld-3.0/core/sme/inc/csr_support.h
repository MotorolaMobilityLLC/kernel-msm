/*
 * Copyright (c) 2011-2020 The Linux Foundation. All rights reserved.
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
 * \file csr_support.h
 *
 * Exports and types for the Common Scan and Roaming supporting interfaces.
 */
#ifndef CSR_SUPPORT_H__
#define CSR_SUPPORT_H__

#include "csr_link_list.h"
#include "csr_api.h"
#include "cds_reg_service.h"

#ifdef FEATURE_WLAN_WAPI
#define CSR_WAPI_OUI_SIZE              (4)
#define CSR_WAPI_VERSION_SUPPORTED     (1)
#define CSR_WAPI_MAX_AUTH_SUITES       (2)
#define CSR_WAPI_MAX_CYPHERS           (5)
#define CSR_WAPI_MAX_UNICAST_CYPHERS   (5)
#define CSR_WAPI_MAX_MULTICAST_CYPHERS (1)
#endif /* FEATURE_WLAN_WAPI */

#define CSR_RSN_OUI_SIZE              (4)
#define CSR_RSN_VERSION_SUPPORTED     (1)
#define CSR_RSN_MAX_AUTH_SUITES       (5)
#define CSR_RSN_MAX_CYPHERS           (5)
#define CSR_RSN_MAX_UNICAST_CYPHERS   (5)
#define CSR_RSN_MAX_MULTICAST_CYPHERS (1)

#define CSR_WPA_OUI_SIZE              (4)
#define CSR_WPA_VERSION_SUPPORTED     (1)
#define CSR_WME_OUI_SIZE (4)
#define CSR_WPA_MAX_AUTH_SUITES       (2)
#define CSR_WPA_MAX_CYPHERS           (5)
#define CSR_WPA_MAX_UNICAST_CYPHERS   (5)
#define CSR_WPA_MAX_MULTICAST_CYPHERS (1)
/* minimum size of the IE->length is the size of the Oui + Version. */
#define CSR_WPA_IE_MIN_SIZE           (6)
#define CSR_WPA_IE_MIN_SIZE_W_MULTICAST (HDD_WPA_IE_MIN_SIZE + HDD_WPA_OUI_SIZE)
#define CSR_WPA_IE_MIN_SIZE_W_UNICAST   (HDD_WPA_IE_MIN_SIZE + \
		HDD_WPA_OUI_SIZE + sizeof(pWpaIe->cUnicastCyphers))

#define CSR_DOT11_SUPPORTED_RATES_MAX (12)
#define CSR_DOT11_EXTENDED_SUPPORTED_RATES_MAX (8)

#define CSR_DOT11_BASIC_RATE_MASK (0x80)

/* NOTE these index are use as array index for csr_rsn_oui */
#define CSR_OUI_USE_GROUP_CIPHER_INDEX 0x00
#define CSR_OUI_WEP40_OR_1X_INDEX      0x01
#define CSR_OUI_TKIP_OR_PSK_INDEX      0x02
#define CSR_OUI_RESERVED_INDEX         0x03
#define CSR_OUI_AES_INDEX              0x04
#define CSR_OUI_WEP104_INDEX           0x05
/* ENUM_FILS_SHA384 9 */
/* ENUM_FILS_SHA384 10 */
/* ENUM_FT_FILS_SHA256 11 */
/* ENUM_FT_FILS_SHA384 12 */
#define CSR_OUI_AES_GCMP_INDEX         0x0D
#define CSR_OUI_AES_GCMP_256_INDEX     0x0E

#ifdef FEATURE_WLAN_WAPI
#define CSR_OUI_WAPI_RESERVED_INDEX    0x00
#define CSR_OUI_WAPI_WAI_CERT_OR_SMS4_INDEX    0x01
#define CSR_OUI_WAPI_WAI_PSK_INDEX     0x02
/* max idx, should be last & highest */
#define CSR_OUI_WAPI_WAI_MAX_INDEX     0x03
#endif /* FEATURE_WLAN_WAPI */

typedef enum {
	/* 11b rates */
	eCsrSuppRate_1Mbps   = 1 * 2,
	eCsrSuppRate_2Mbps   = 2 * 2,
	eCsrSuppRate_5_5Mbps = 11,      /* 5.5 * 2 */
	eCsrSuppRate_11Mbps  = 11 * 2,

	/* 11a / 11g rates */
	eCsrSuppRate_6Mbps   = 6 * 2,
	eCsrSuppRate_9Mbps   = 9 * 2,
	eCsrSuppRate_12Mbps  = 12 * 2,
	eCsrSuppRate_18Mbps  = 18 * 2,
	eCsrSuppRate_24Mbps  = 24 * 2,
	eCsrSuppRate_36Mbps  = 36 * 2,
	eCsrSuppRate_48Mbps  = 48 * 2,
	eCsrSuppRate_54Mbps  = 54 * 2,

	/* airgo proprietary rates */
	eCsrSuppRate_10Mbps   = 10 * 2,
	eCsrSuppRate_10_5Mbps = 21,     /* 10.5 * 2 */
	eCsrSuppRate_20Mbps   = 20 * 2,
	eCsrSuppRate_21Mbps   = 21 * 2,
	eCsrSuppRate_40Mbps   = 40 * 2,
	eCsrSuppRate_42Mbps   = 42 * 2,
	eCsrSuppRate_60Mbps   = 60 * 2,
	eCsrSuppRate_63Mbps   = 63 * 2,
	eCsrSuppRate_72Mbps   = 72 * 2,
	eCsrSuppRate_80Mbps   = 80 * 2,
	eCsrSuppRate_84Mbps   = 84 * 2,
	eCsrSuppRate_96Mbps   = 96 * 2,
	eCsrSuppRate_108Mbps  = 108 * 2,
	eCsrSuppRate_120Mbps  = 120 * 2,
	eCsrSuppRate_126Mbps  = 126 * 2,
	eCsrSuppRate_144Mbps  = 144 * 2,
	eCsrSuppRate_160Mbps  = 160 * 2,
	eCsrSuppRate_168Mbps  = 168 * 2,
	eCsrSuppRate_192Mbps  = 192 * 2,
	eCsrSuppRate_216Mbps  = 216 * 2,
	eCsrSuppRate_240Mbps  = 240 * 2
} eCsrSupportedRates;

/* Generic Information Element Structure */
typedef struct sDot11IEHeader {
	uint8_t ElementID;
	uint8_t Length;
} qdf_packed tDot11IEHeader;

typedef struct tagCsrWpaIe {
	tDot11IEHeader IeHeader;
	uint8_t Oui[CSR_WPA_OUI_SIZE];
	uint16_t Version;
	uint8_t MulticastOui[CSR_WPA_OUI_SIZE];
	uint16_t cUnicastCyphers;
	struct {
		uint8_t Oui[CSR_WPA_OUI_SIZE];
	} qdf_packed UnicastOui[1];
} qdf_packed tCsrWpaIe;

typedef struct tagCsrWpaAuthIe {
	uint16_t cAuthenticationSuites;
	struct {
		uint8_t Oui[CSR_WPA_OUI_SIZE];
	} qdf_packed AuthOui[1];
} qdf_packed tCsrWpaAuthIe;

typedef struct tagCsrRSNIe {
	tDot11IEHeader IeHeader;
	uint16_t Version;
	uint8_t MulticastOui[CSR_RSN_OUI_SIZE];
	uint16_t cUnicastCyphers;
	struct {
		uint8_t Oui[CSR_RSN_OUI_SIZE];
	} qdf_packed UnicastOui[1];
} qdf_packed tCsrRSNIe;

typedef struct tagCsrRSNAuthIe {
	uint16_t cAuthenticationSuites;
	struct {
		uint8_t Oui[CSR_RSN_OUI_SIZE];
	} qdf_packed AuthOui[1];
} qdf_packed tCsrRSNAuthIe;

typedef struct tagCsrRSNPMKIe {
	uint16_t cPMKIDs;
	struct {
		uint8_t PMKID[PMKID_LEN];
	} qdf_packed PMKIDList[1];
} qdf_packed tCsrRSNPMKIe;

typedef struct tCsrIELenInfo {
	uint8_t min;
	uint8_t max;
} qdf_packed tCsrIELenInfo;

#ifdef FEATURE_WLAN_WAPI
typedef struct tagCsrWapiIe {
	tDot11IEHeader IeHeader;
	uint16_t Version;
	uint16_t cAuthenticationSuites;
	struct {
		uint8_t Oui[CSR_WAPI_OUI_SIZE];
	} qdf_packed AuthOui[1];
	uint16_t cUnicastCyphers;
	struct {
		uint8_t Oui[CSR_WAPI_OUI_SIZE];
	} qdf_packed UnicastOui[1];
	uint8_t MulticastOui[CSR_WAPI_OUI_SIZE];
	struct {
		uint16_t PreAuthSupported:1;
		uint16_t Reserved:15;
	} qdf_packed tCsrWapiCapabilities;
} qdf_packed tCsrWapiIe;
#endif /* FEATURE_WLAN_WAPI */

/**
 * struct csr_timer_info - CSR-specific timer context
 * @mac: Global MAC context associated with the timer
 * @vdev_id: Session associated with the timer
 */
struct csr_timer_info {
	struct mac_context *mac;
	uint8_t vdev_id;
};

#define CSR_IS_11A_BSS(bss_desc)    (eSIR_11A_NW_TYPE == (bss_desc)->nwType)
#define CSR_IS_BASIC_RATE(rate)     ((rate) & CSR_DOT11_BASIC_RATE_MASK)
#define CSR_IS_QOS_BSS(pIes)  \
		((pIes)->WMMParams.present || (pIes)->WMMInfoAp.present)
#define CSR_IS_UAPSD_BSS(pIes) \
	(((pIes)->WMMParams.present && \
	 ((pIes)->WMMParams.qosInfo & SME_QOS_AP_SUPPORTS_APSD)) || \
	 ((pIes)->WMMInfoAp.present && (pIes)->WMMInfoAp.uapsd))

bool csr_get_bss_id_bss_desc(struct bss_description *pSirBssDesc,
			     struct qdf_mac_addr *pBssId);
bool csr_is_bss_id_equal(struct bss_description *pSirBssDesc1,
			 struct bss_description *pSirBssDesc2);

eCsrMediaAccessType
csr_get_qos_from_bss_desc(struct mac_context *mac_ctx,
			  struct bss_description *pSirBssDesc,
			  tDot11fBeaconIEs *pIes);

bool csr_is_nullssid(uint8_t *pBssSsid, uint8_t len);
bool csr_is_infra_bss_desc(struct bss_description *pSirBssDesc);

tSirResultCodes csr_get_de_auth_rsp_status_code(struct deauth_rsp *pSmeRsp);
uint32_t csr_get_frag_thresh(struct mac_context *mac_ctx);
uint32_t csr_get_rts_thresh(struct mac_context *mac_ctx);
uint32_t csr_get11h_power_constraint(struct mac_context *mac_ctx,
				     tDot11fIEPowerConstraints *constraints);
uint8_t csr_construct_rsn_ie(struct mac_context *mac, uint32_t sessionId,
			     struct csr_roam_profile *pProfile,
			     struct bss_description *pSirBssDesc,
			     tDot11fBeaconIEs *pIes, tCsrRSNIe *pRSNIe);

uint8_t csr_construct_wpa_ie(struct mac_context *mac, uint8_t session_id,
			     struct csr_roam_profile *pProfile,
			     struct bss_description *pSirBssDesc,
			     tDot11fBeaconIEs *pIes, tCsrWpaIe *pWpaIe);

#ifdef FEATURE_WLAN_WAPI
bool csr_is_profile_wapi(struct csr_roam_profile *pProfile);
#endif /* FEATURE_WLAN_WAPI */
/*
 * If a WPAIE exists in the profile, just use it.
 * Or else construct one from the BSS Caller allocated memory for pWpaIe and
 * guarrantee it can contain a max length WPA IE
 */
uint8_t csr_retrieve_wpa_ie(struct mac_context *mac, uint8_t session_id,
			    struct csr_roam_profile *pProfile,
			    struct bss_description *pSirBssDesc,
			    tDot11fBeaconIEs *pIes, tCsrWpaIe *pWpaIe);

bool csr_is_ssid_equal(struct mac_context *mac,
		       struct bss_description *pSirBssDesc1,
		       struct bss_description *pSirBssDesc2,
		       tDot11fBeaconIEs *pIes2);

/* Null ssid means match */
bool csr_is_ssid_in_list(tSirMacSSid *pSsid, tCsrSSIDs *pSsidList);
bool csr_is_profile_wpa(struct csr_roam_profile *pProfile);
bool csr_is_profile_rsn(struct csr_roam_profile *pProfile);
/*
 * If a RSNIE exists in the profile, just use it. Or
 * else construct one from the BSS Caller allocated memory for pWpaIe and
 * guarantee it can contain a max length WPA IE
 */
uint8_t csr_retrieve_rsn_ie(struct mac_context *mac, uint32_t sessionId,
			    struct csr_roam_profile *pProfile,
			    struct bss_description *pSirBssDesc,
			    tDot11fBeaconIEs *pIes, tCsrRSNIe *pRsnIe);
#ifdef FEATURE_WLAN_WAPI
/*
 * If a WAPI IE exists in the profile, just use it.
 * Or else construct one from the BSS. Caller allocated memory for pWapiIe and
 * guarrantee it can contain a max length WAPI IE
 */
uint8_t csr_retrieve_wapi_ie(struct mac_context *mac, uint32_t sessionId,
			     struct csr_roam_profile *pProfile,
			     struct bss_description *pSirBssDesc,
			     tDot11fBeaconIEs *pIes, tCsrWapiIe *pWapiIe);
#endif /* FEATURE_WLAN_WAPI */
bool csr_rates_is_dot11_rate11b_supported_rate(uint8_t dot11Rate);
bool csr_rates_is_dot11_rate11a_supported_rate(uint8_t dot11Rate);
tAniEdType csr_translate_encrypt_type_to_ed_type(
		eCsrEncryptionType EncryptType);

bool csr_is_bssid_match(struct qdf_mac_addr *pProfBssid,
			struct qdf_mac_addr *BssBssid);
void csr_add_rate_bitmap(uint8_t rate, uint16_t *pRateBitmap);
bool csr_check_rate_bitmap(uint8_t rate, uint16_t RateBitmap);
bool csr_rates_is_dot11_rate_supported(struct mac_context *mac_ctx, uint8_t rate);
enum bss_type csr_translate_bsstype_to_mac_type(eCsrRoamBssType csrtype);
/* Caller allocates memory for pIEStruct */
QDF_STATUS csr_parse_bss_description_ies(struct mac_context *mac_ctx,
					 struct bss_description *bss_desc,
					 tDot11fBeaconIEs *pIEStruct);
/*
 * This function will allocate memory for the parsed IEs to the caller.
 * Caller must free the memory. after it is done with the data only if
 * this function succeeds
 */
QDF_STATUS csr_get_parsed_bss_description_ies(struct mac_context *mac_ctx,
					      struct bss_description *bss_desc,
					      tDot11fBeaconIEs **ppIEStruct);

tSirScanType csr_get_scan_type(struct mac_context *mac, uint8_t chnId);

QDF_STATUS csr_get_phy_mode_from_bss(struct mac_context *mac,
		struct bss_description *pBSSDescription,
		eCsrPhyMode *pPhyMode, tDot11fBeaconIEs *pIes);
/*
 * fForce -- force reassoc regardless of whether there is any change.
 * The reason is that for UAPSD-bypass, the code underneath this call determine
 * whether to allow UAPSD. The information in pModProfileFields reflects what
 * the user wants. There may be discrepency in it. UAPSD-bypass logic should
 * decide if it needs to reassoc
 */
QDF_STATUS csr_reassoc(struct mac_context *mac, uint32_t sessionId,
		tCsrRoamModifyProfileFields *pModProfileFields,
		uint32_t *pRoamId, bool fForce);

bool csr_is_profile11r(struct mac_context *mac, struct csr_roam_profile *pProfile);
bool csr_is_auth_type11r(struct mac_context *mac, enum csr_akm_type AuthType,
			 uint8_t mdiePresent);
#ifdef FEATURE_WLAN_ESE
bool csr_is_profile_ese(struct csr_roam_profile *pProfile);
#endif

/**
 * csr_is_auth_type_ese() - Checks whether Auth type is ESE or not
 * @AuthType: Authentication type
 *
 * Return: true, if auth type is ese, false otherwise
 */
bool csr_is_auth_type_ese(enum csr_akm_type AuthType);

#endif
