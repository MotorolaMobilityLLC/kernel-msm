/*
 * Copyright (c) 2017, 2019-2020 The Linux Foundation. All rights reserved.
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

#include "wlan_crypto_global_def.h"

#define FILS_EAP_TLV_MAX_DATA_LEN 255
#define FILS_SHA256_128_AUTH_TAG 16
#define FILS_SHA256_256_AUTH_TAG 32

/* RFC 6696 */
#define RMSK_LABEL "Re-authentication Master Session Key@ietf.org"

/* 12.12.2.5.3 80211-ai draft */
#define PTK_KEY_LABEL "FILS PTK Derivation"
#define FT_PMK_R0_KEY_LABEL "FT-R0"
#define FT_PMK_R0_NAME_KEY_LABEL "FT-R0N"
#define FT_PMK_R1_NAME_KEY_LABEL "FT-R1N"

#define PMKR0_SCATTER_LIST_ELEM 2
#define PMKR1_SCATTER_LIST_ELEM 4

#define SCTR_LST_ELEM0 0
#define SCTR_LST_ELEM1 1
#define SCTR_LST_ELEM2 2
#define SCTR_LST_ELEM3 3

/* Length of "FT-R1N" */
#define SCTR_LST_R0_LABEL_LEN 6
#define SCTR_LST_R1_LABEL_LEN 6

#define MAX_ICK_LEN 48
#define MAX_KEK_LEN 64
#define MAX_TK_LEN 32
#define MAX_KEY_AUTH_DATA_LEN 48
#define MAX_GTK_LEN 255
#define MAX_IGTK_LEN 255
#define SIR_FILS_SESSION_IE_LEN 11
#define FILS_KEY_RSC_LEN 8
#define FILS_MAX_KEY_AUTH_LEN (MAX_ICK_LEN + MAX_KEK_LEN + MAX_TK_LEN)

#define IPN_LEN 6
#define FILS_SESSION_LENGTH 8
#define FILS_MAX_KDE_LIST_LEN 255
#define FILS_MAX_HLP_DATA_LEN 2048

/* 12.12.2.5.3 80211-ai draft */
#define FILS_SHA384_KEK_LEN 64
#define FILS_SHA256_KEK_LEN 32

/* 12.12.2.5.3 80211-ai draft */
#define FILS_SHA256_ICK_LEN 32
#define FILS_SHA384_ICK_LEN 48

#define TK_LEN_TKIP 32
#define TK_LEN_CCMP 16
#define TK_LEN_AES_128_CMAC 32

#define FILS_SHA256_PMK_LEN 32
#define FILS_SHA384_PMK_LEN 48

#define FILS_FT_SHA256_LEN 32
#define FILS_FT_SHA384_LEN 48

#define FILS_FT_MAX_R0_KEY_DATA_LEN 64

/* 12.7.1.7.3 802.11ai */
#define FILS_SHA256_Q_LEN 32
#define FILS_SHA384_Q_LEN 48

/* 9.4.2.180 FILS Session element */
#define SIR_FILS_SESSION_LENGTH    8
#define SIR_FILS_SESSION_EXT_EID   4

/* 9.4.2.184 FILS HLP Container Element */
#define SIR_FILS_HLP_EXT_EID 5

/* 9.4.2.190 FILS Nonce element */
#define SIR_FILS_NONCE_LENGTH      16
#define SIR_FILS_NONCE_EXT_EID   13

/*9.4.2.188 FILS Wrapped Data element */
#define SIR_FILS_WRAPPED_DATA_MAX_SIZE 255
#define SIR_FILS_WRAPPED_DATA_EXT_EID   8

/* RFC 6696 5.3.1: EAP-Initiate/Re-auth-Start Packet */
#define SIR_FILS_EAP_REAUTH_PACKET_TYPE 1
#define SIR_FILS_EAP_INIT_PACKET_TYPE 2

#define FILS_AUTH_TAG_MAX_LENGTH 32

#define SIR_FILS_OPTIONAL_DATA_LEN 3
/* RFC 6696 4.3: RiK deriavtion */
#define SIR_FILS_RIK_LABEL "Re-authentication Integrity Key@ietf.org"

/* RFC 6696 5.3.1: EAP-Initiate/Re-auth-Start Packet */
#define SIR_FILS_EAP_TLV_KEYNAME_NAI 1
#define SIR_FILS_EAP_TLV_R_RK_LIFETIME 2
#define SIR_FILS_EAP_TLV_R_MSK_LIFETIME 3
#define SIR_FILS_EAP_TLV_DOMAIN_NAME 4
#define SIR_FILS_EAP_TLV_CRYPTO_LIST 5
#define SIR_FILS_EAP_TLV_AUTH_INDICATION 6

#define DATA_TYPE_GTK 1
#define DATA_TYPE_IGTK 9
#define KEY_RSC_LEN 8
#define KDE_IE_DATA_OFFSET 4
#define KDE_DATA_TYPE_OFFSET 3
#define GTK_OFFSET 2
#define IPN_OFFSET 2
#define IGTK_OFFSET 8

#define KDE_OUI_TYPE   "\x00\x0F\xAC"
#define KDE_OUI_TYPE_SIZE  3

#define SINGLE_ELEMENT_HASH_CNT 1

/*
 * struct eap_auth_reserved: this structure defines flags format in eap packets
 * as defined in RFC 6696 5.3.1
 * flag_r:
 * flag_b:
 * flag_l:
 */
struct eap_auth_reserved {
	uint8_t flag_r:1;
	uint8_t flag_b:1;
	uint8_t flag_l:1;
	uint8_t reverved:5;
};

/*
 * struct fils_eap_tlv: this structure defines the eap header
 * for eap packet present in warpped data element IE
 * @type: type of packet
 * @length: length of packet
 * @data: pointer to eap data
 */
struct fils_eap_tlv {
	uint8_t type;
	uint8_t length;
	uint8_t data[FILS_EAP_TLV_MAX_DATA_LEN];
};

/* struct fils_auth_rsp_info: this structure saves the info from
 * fils auth response.
 * @keyname: pointer to keyname nai
 * @keylength: keyname nai length
 * @domain_name: pointer to domain name
 * @domain_len: domain length
 * @r_rk_lifetime: rRk lifetime
 * @r_msk_lifetime: RMSK lifetime
 * @sequence: sequence number to be validated
 * @fils_nonce: anonce
 * @assoc_delay: time in ms, DUT needs to wait after association req
 */
struct fils_auth_rsp_info {
	uint8_t *keyname;
	uint8_t keylength;
	uint8_t *domain_name;
	uint8_t domain_len;
	uint32_t r_rk_lifetime;
	uint32_t r_msk_lifetime;
	uint16_t sequence;
	uint8_t fils_nonce[SIR_FILS_NONCE_LENGTH];
	uint8_t assoc_delay;
};

#define FT_R0KH_ID_MAX_LEN 48
#define FT_R1KH_ID_LEN     6
#define FT_NONCE_LEN       32

/* MIC Length Specified in Table 12-8- 802.11-2016 Spec */
#define FT_MIC_LEN         16
#define FT_GTK_RSC_LEN     8
#define FT_GTK_KEY_LEN     32
#define FT_IGTK_KEY_ID_LEN 2
#define FT_IGTK_IPN_LEN    6
#define FT_IGTK_KEY_LEN    24

/**
 * struct mac_ft_gtk_ie - structure to parse the gtk ie
 * @present: flag to indicate ie is present
 * @key_id: Key-Id
 * @reserved: reserved bits
 * @key_length: gtk key length
 * @rsc: denotes the last TSC or PN sent using the GTK
 * @num_key: number of keys
 * @key: actual keys
 */
struct mac_ft_gtk_ie {
	uint8_t present;
	uint16_t key_id:2;
	uint16_t reserved:14;
	uint8_t key_len;
	uint8_t rsc[FT_GTK_RSC_LEN];
	uint8_t num_key;
	uint8_t key[FT_GTK_KEY_LEN];
};

/**
 * struct mac_ft_gtk_ie - structure to parse the gtk ie
 * @present: IE present or not present
 * @key_id: 2Byte Key-ID
 * @ipn: icorresponds to the last packet number used by broadcaster/multicaster
 * @key_len: IGTK key length
 * @key: IGTK Key
 */
struct mac_ft_igtk_ie {
	uint8_t present;
	uint8_t key_id[FT_IGTK_KEY_ID_LEN];
	uint8_t ipn[FT_IGTK_IPN_LEN];
	uint8_t key_len;
	uint8_t key[FT_IGTK_KEY_LEN];
};

/**
 * struct mac_ft_ie - structure to parse the FT ie from auth frame
 * @present: true if IE is present in Auth Frame
 * @element_count: number of elements
 * @mic: MIC. Will be zero in auth frame sent from AP. (Refer 13.2.4 802.11ai)
 * @anonce: Authenticator NONCE. Will be zero in auth frame sent from AP.
 * @snonce: Supplicant NONCE. Will be zero in auth frame
 * @r1kh_id: R1KH ID. Length of R1KH ID is fixed(6 bytes).
 * @r0kh_id_len: Length of R0KH ID
 * @r0kh_id: R0KH id
 * @gtk_ie: GTK subelement in FTIE
 * @igtk_ie: IGTK subelement in FTIE
 */
struct mac_ft_ie {
	bool present;
	uint8_t element_count;
	uint8_t mic[FT_MIC_LEN];
	uint8_t anonce[FT_NONCE_LEN];
	uint8_t snonce[FT_NONCE_LEN];
	uint8_t r1kh_id[FT_R1KH_ID_LEN];
	uint8_t r0kh_id_len;
	uint8_t r0kh_id[FT_R0KH_ID_MAX_LEN];
	struct mac_ft_gtk_ie gtk_ie;
	struct mac_ft_igtk_ie igtk_ie;
};

#define FILS_PMK_LEN 48
#define FILS_PMK_NAME_LEN 16
#define FILS_FT_MAX_LEN 48
#define FILS_FT_PMK_R0_SALT_LEN 16
#define FILS_MAX_KEY_DATA_LEN \
	(MAX_ICK_LEN + MAX_KEK_LEN + MAX_TK_LEN + FILS_FT_MAX_LEN)

/*
 * struct pe_fils_session: fils session info used in PE session
 * @is_fils_connection: whether connection is fils or not
 * @keyname_nai_data: keyname nai data
 * @keyname_nai_length: keyname nai length
 * @akm: akm type will be used
 * @auth: authentication type
 * @cipher: cipher type
 * @fils_erp_reauth_pkt: pointer to fils reauth packet data
 * @fils_erp_reauth_pkt_len: reauth packet length
 * @fils_rrk: pointer to fils rRk
 * @fils_rrk_len: fils rRk length
 * @fils_rik: pointer to fils rIk
 * @fils_rik_len: fils rIk length
 * @sequence_number: sequence number needs to be used in eap packet
 * @fils_session: fils session IE element
 * @fils_nonce: fils snonce
 * @rsn_ie: rsn ie used in auth request
 * @rsn_ie_len: rsn ie length
 * @group_mgmt_cipher_suite_present: Check if group management cipher suite
 * is present in the FILS RSN IE
 * @ft_ie: structure to store the parsed FTIE from auth response frame
 * @pmkr0: PMKR0
 * @pmkr0_len: length of PMKR0 key
 * @pmkr0_name: PMK_R0 name derived
 * @pmkr1_name: PMKR1 Name derived
 * @fils_eap_finish_pkt: pointer to eap finish packet
 * @fils_eap_finish_pkt_len: eap finish packet length
 * @fils_rmsk: rmsk data pointer
 * @fils_rmsk_len: rmsk data length
 * @fils_pmk: pointer to pmk data
 * @fils_pmk_len: pmk length
 * @fils_pmkid: pointer to pmkid derived
 * @auth_info: data obtained from auth response
 * @ick: pointer to ick
 * @ick_len: ick length
 * @kek: pointer to kek
 * @kek_len: kek length
 * @tk: pointer to tk
 * @tk_len: tk length
 * @key_auth: data needs to be sent in assoc req, will be validated by AP
 * @key_auth_len: key auth data length
 * @ap_key_auth_data: data needs to be validated in assoc rsp
 * @ap_key_auth_len:  ap key data length
 * @gtk_len: gtk key length
 * @gtk: pointer to gtk data
 * @fils_ft: xx_key data
 * @fils_ft_len: xx_key length
 * @rsc: rsc value
 * @igtk_len: igtk length
 * @igtk: igtk data pointer
 * @ipn: pointer to ipn data
 * @dst_mac: HLP destination mac address
 * @src_mac: HLP source mac address
 * @hlp_data_len: HLP data length
 * @hlp_data: pointer to HLP data
 */
struct pe_fils_session {
	bool is_fils_connection;
	uint8_t *keyname_nai_data;
	uint8_t keyname_nai_length;
	uint8_t akm;
	uint8_t auth;
	uint8_t cipher;
	uint8_t *fils_erp_reauth_pkt;
	uint32_t fils_erp_reauth_pkt_len;
	uint8_t *fils_rrk;
	uint8_t fils_rrk_len;
	uint8_t *fils_rik;
	uint32_t fils_rik_len;
	uint16_t sequence_number;
	uint8_t fils_session[SIR_FILS_SESSION_LENGTH];
	uint8_t fils_nonce[SIR_FILS_NONCE_LENGTH];
	uint8_t rsn_ie[WLAN_MAX_IE_LEN];
	uint8_t rsn_ie_len;
	bool group_mgmt_cipher_present;
	struct mac_ft_ie ft_ie;
	uint8_t pmkr0[FILS_PMK_LEN];
	uint8_t pmkr0_len;
	uint8_t pmkr0_name[FILS_PMK_NAME_LEN];
	uint8_t pmkr1_name[FILS_PMK_NAME_LEN];
	uint8_t *fils_eap_finish_pkt;
	uint8_t fils_eap_finish_pkt_len;
	uint8_t *fils_rmsk;
	uint8_t fils_rmsk_len;
	uint8_t *fils_pmk;
	uint8_t fils_pmk_len;
	uint8_t fils_pmkid[PMKID_LEN];
	struct fils_auth_rsp_info auth_info;
	uint8_t ick[MAX_ICK_LEN];
	uint8_t ick_len;
	uint8_t kek[MAX_KEK_LEN];
	uint8_t kek_len;
	uint8_t tk[MAX_TK_LEN];
	uint8_t tk_len;
	uint8_t fils_ft[FILS_FT_MAX_LEN];
	uint8_t fils_ft_len;
	uint8_t key_auth[MAX_KEY_AUTH_DATA_LEN];
	uint8_t key_auth_len;
	uint8_t ap_key_auth_data[MAX_KEY_AUTH_DATA_LEN];
	uint8_t ap_key_auth_len;
	uint8_t gtk_len;
	uint8_t gtk[MAX_GTK_LEN];
	uint8_t rsc;
	uint8_t igtk_len;
	uint8_t igtk[MAX_IGTK_LEN];
	uint8_t ipn[IPN_LEN];
	struct qdf_mac_addr dst_mac;
	struct qdf_mac_addr src_mac;
	uint16_t hlp_data_len;
	uint8_t *hlp_data;
};
