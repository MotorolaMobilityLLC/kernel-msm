/*
 * Copyright (c) 2013-2016 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
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

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

#ifndef _WMI_TLV_DEFS_H_
#define _WMI_TLV_DEFS_H_

#ifdef __cplusplus
extern "C" {
#endif

#define WMITLV_FIELD_BUF_IS_ALLOCATED(elem_name) \
       is_allocated_##elem_name

#define WMITLV_FIELD_NUM_OF(elem_name) \
       num_##elem_name

/* Define the structure typedef for the TLV parameters of each cmd/event */
#define WMITLV_TYPEDEF_STRUCT_PARAMS_TLVS(wmi_cmd_event_id) \
       wmi_cmd_event_id##_param_tlvs

/*
 * The following macro WMITLV_OP_* are created by the macro WMITLV_ELEM().
 */
/* macro to define the TLV name in the correct order. When (op==TAG_ORDER) */
#define WMITLV_OP_TAG_ORDER_macro(param_ptr, param_len, wmi_cmd_event_id, elem_tlv_tag, elem_struc_type, elem_name, var_len, arr_size)  \
      wmi_cmd_event_id##_tlv_order_##elem_name,

/* macro to define the TLV name with the TLV Tag value. When (op==TAG_ID) */
#define WMITLV_OP_TAG_ID_macro(param_ptr, param_len, wmi_cmd_event_id, elem_tlv_tag, elem_struc_type, elem_name, var_len, arr_size)  \
      wmi_cmd_event_id##_tlv_tag_##elem_name = elem_tlv_tag,

/* macro to define the TLV name with the TLV structure size. May not be accurate when variable length. When (op==TAG_SIZEOF) */
#define WMITLV_OP_TAG_SIZEOF_macro(param_ptr, param_len, wmi_cmd_event_id, elem_tlv_tag, elem_struc_type, elem_name, var_len, arr_size)  \
      wmi_cmd_event_id##_sizeof_##elem_name = sizeof(elem_struc_type),

/* macro to define the TLV name with value indicating whether the TLV is variable length. When (op==TAG_VAR_SIZED) */
#define WMITLV_OP_TAG_VAR_SIZED_macro(param_ptr, param_len, wmi_cmd_event_id, elem_tlv_tag, elem_struc_type, elem_name, var_len, arr_size)  \
      wmi_cmd_event_id##_var_sized_##elem_name = var_len,

/* macro to define the TLV name with value indicating the fixed array size. When (op==TAG_ARR_SIZE) */
#define WMITLV_OP_TAG_ARR_SIZE_macro(param_ptr, param_len, wmi_cmd_event_id, elem_tlv_tag, elem_struc_type, elem_name, var_len, arr_size)  \
      wmi_cmd_event_id##_arr_size_##elem_name = arr_size,

/*
 * macro to define afew fields associated to a TLV. For example, a structure pointer with the TLV name.
 * This macro is expand from WMITLV_ELEM(op) when (op==STRUCT_FIELD).
 * NOTE: If this macro is changed, then "mirror" structure wmitlv_cmd_param_info
 * should be updated too.
 */
#define WMITLV_OP_STRUCT_FIELD_macro(param_ptr, param_len, wmi_cmd_event_id, elem_tlv_tag, elem_struc_type, elem_name, var_len, arr_size)  \
      elem_struc_type *elem_name; \
      A_UINT32 WMITLV_FIELD_NUM_OF(elem_name); \
      A_UINT32 WMITLV_FIELD_BUF_IS_ALLOCATED(elem_name);

/*
 * A "mirror" structure that contains the fields that is created by the
 * macro WMITLV_OP_STRUCT_FIELD_macro.
 * NOTE: you should modify this structure and WMITLV_OP_STRUCT_FIELD_macro
 * so that they both has the same kind of fields.
 */
typedef struct {
    void *tlv_ptr;            /* Pointer to the TLV Buffer. But the "real" one will have the right type instead of void. */
    A_UINT32 num_elements;    /* Number of elements. For non-array, this is one. For array, this is the number of elements. */
    A_UINT32 buf_is_allocated;/* Boolean flag to indicate that a new buffer is allocated for this TLV. */
} wmitlv_cmd_param_info;

/*
 * NOTE TRICKY MACRO:
 *  WMITLV_ELEM is re-defined to a "op" specific macro.
 *  Eg. WMITLV_OP_TAG_ORDER_macro is created for the op_type=TAG_ORDER.
 */
#define WMITLV_ELEM(wmi_cmd_event_id, op_type, param_ptr, param_len, elem_tlv_tag, elem_struc_type, elem_name, var_len) \
    WMITLV_OP_##op_type##_macro(param_ptr, param_len, wmi_cmd_event_id, elem_tlv_tag, elem_struc_type, elem_name, var_len, WMITLV_ARR_SIZE_INVALID)
/*
 *  WMITLV_FXAR (FiX ARray) is similar to WMITLV_ELEM except it has an extra parameter for the fixed number of elements.
 *  It is re-defined to a "op" specific macro.
 *  Eg. WMITLV_OP_TAG_ORDER_macro is created for the op_type=TAG_ORDER.
 */
#define WMITLV_FXAR(wmi_cmd_event_id, op_type, param_ptr, param_len, elem_tlv_tag, elem_struc_type, elem_name, var_len, arr_size) \
    WMITLV_OP_##op_type##_macro(param_ptr, param_len, wmi_cmd_event_id, elem_tlv_tag, elem_struc_type, elem_name, var_len, arr_size)

#define WMITLV_TABLE(id,op,buf,len) WMITLV_TABLE_##id(id,op,buf,len)

/*
 * This macro will create various enumerations and structures to describe the TLVs for
 * the given Command/Event ID.
 *
 * For example, the following is for WMI_SERVICE_READY_EVENTID:
 *    #define WMITLV_TABLE_WMI_SERVICE_READY_EVENTID(id,op,buf,len)                                                                                                 \
 *       WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_service_ready_event_fixed_param, wmi_service_ready_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)     \
 *       WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_HAL_REG_CAPABILITIES, HAL_REG_CAPABILITIES, hal_reg_capabilities, WMITLV_SIZE_FIX)  \
 *       WMITLV_FXAR(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, wmi_service_bitmap, WMITLV_SIZE_FIX, WMI_SERVICE_BM_SIZE) \
 *       WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wlan_host_mem_req, mem_reqs, WMITLV_SIZE_VAR)
 *    WMITLV_CREATE_PARAM_STRUC(WMI_SERVICE_READY_EVENTID);
 * This macro will create the following text:
 *
 * typedef enum {
 *    WMI_SERVICE_READY_EVENTID_tlv_order_wmi_service_ready_event_fixed_param,
 *    WMI_SERVICE_READY_EVENTID_tlv_order_hal_reg_capabilities,
 *    WMI_SERVICE_READY_EVENTID_tlv_order_wmi_service_bitmap,
 *    WMI_SERVICE_READY_EVENTID_tlv_order_mem_reqs,
 *    WMI_TLV_HLPR_NUM_TLVS_FOR_WMI_SERVICE_READY_EVENTID
 * } WMI_SERVICE_READY_EVENTID_TAG_ID_enum_type;
 * //NOTE: WMI_TLV_HLPR_NUM_TLVS_FOR_WMI_SERVICE_READY_EVENTID is the number of TLVs.
 *
 * typedef enum {
 *    WMI_SERVICE_READY_EVENTID_tlv_tag_wmi_service_ready_event_fixed_param = WMITLV_TAG_STRUC_wmi_service_ready_event_fixed_param,
 *    WMI_SERVICE_READY_EVENTID_tlv_tag_hal_reg_capabilities = WMITLV_TAG_STRUC_HAL_REG_CAPABILITIES,
 *    WMI_SERVICE_READY_EVENTID_tlv_tag_wmi_service_bitmap = WMITLV_TAG_ARRAY_UINT32,
 *    WMI_SERVICE_READY_EVENTID_tlv_tag_mem_reqs = WMITLV_TAG_ARRAY_STRUC,
 * } WMI_SERVICE_READY_EVENTID_TAG_ORDER_enum_type;
 *
 * typedef enum {
 *    WMI_SERVICE_READY_EVENTID_sizeof_wmi_service_ready_event_fixed_param = sizeof(wmi_service_ready_event_fixed_param),
 *    WMI_SERVICE_READY_EVENTID_sizeof_hal_reg_capabilities = sizeof(HAL_REG_CAPABILITIES),
 *    WMI_SERVICE_READY_EVENTID_sizeof_wmi_service_bitmap = sizeof(A_UINT32),
 *    WMI_SERVICE_READY_EVENTID_sizeof_mem_reqs = sizeof(wlan_host_mem_req),
 * } WMI_SERVICE_READY_EVENTID_TAG_SIZEOF_enum_type;
 *
 * typedef enum {
 *    WMI_SERVICE_READY_EVENTID_var_sized_wmi_service_ready_event_fixed_param = WMITLV_SIZE_FIX,
 *    WMI_SERVICE_READY_EVENTID_var_sized_hal_reg_capabilities = WMITLV_SIZE_FIX,
 *    WMI_SERVICE_READY_EVENTID_var_sized_wmi_service_bitmap = WMITLV_SIZE_VAR,
 *    WMI_SERVICE_READY_EVENTID_var_sized_mem_reqs = WMITLV_SIZE_VAR,
 * } WMI_SERVICE_READY_EVENTID_TAG_VAR_SIZED_enum_type;
 *
 * typedef enum {
 *    WMI_SERVICE_READY_EVENTID_arr_size_wmi_service_ready_event_fixed_param = WMITLV_ARR_SIZE_INVALID,
 *    WMI_SERVICE_READY_EVENTID_arr_size_hal_reg_capabilities = WMITLV_ARR_SIZE_INVALID,
 *    WMI_SERVICE_READY_EVENTID_arr_size_wmi_service_bitmap = WMI_SERVICE_BM_SIZE,
 *    WMI_SERVICE_READY_EVENTID_arr_size_mem_reqs = WMITLV_ARR_SIZE_INVALID,
 * } WMI_SERVICE_READY_EVENTID_TAG_ARR_SIZE_enum_type;
 *
 * typedef struct {
 *    wmi_service_ready_event_fixed_param *fixed_param;
 *    A_UINT32 num_fixed_param;
 *    A_UINT32 is_allocated_fixed_param;
 *    HAL_REG_CAPABILITIES *hal_reg_capabilities;
 *    A_UINT32 num_hal_reg_capabilities;
 *    A_UINT32 is_allocated_hal_reg_capabilities;
 *    A_UINT32 *wmi_service_bitmap;
 *    A_UINT32 num_wmi_service_bitmap;
 *    A_UINT32 is_allocated_wmi_service_bitmap;
 *    wlan_host_mem_req *mem_reqs;
 *    A_UINT32 num_mem_reqs;
 *    A_UINT32 is_allocated_mem_reqs;
 *
 * } WMI_SERVICE_READY_EVENTID_param_tlvs;
 *
 */

#define WMITLV_CREATE_PARAM_STRUC(wmi_cmd_event_id)            \
    typedef enum {                                             \
        WMITLV_TABLE(wmi_cmd_event_id, TAG_ORDER, NULL, 0)     \
        WMI_TLV_HLPR_NUM_TLVS_FOR_##wmi_cmd_event_id           \
    } wmi_cmd_event_id##_TAG_ORDER_enum_type;                  \
                                                               \
    typedef struct {                                           \
        WMITLV_TABLE(wmi_cmd_event_id, STRUCT_FIELD, NULL, 0)  \
    } WMITLV_TYPEDEF_STRUCT_PARAMS_TLVS(wmi_cmd_event_id);     \

/** Enum list of TLV Tags for each parameter structure type. */
typedef enum {
    /* 0 to 15 is reserved */
    WMITLV_TAG_LAST_RESERVED = 15,
    WMITLV_TAG_FIRST_ARRAY_ENUM, /* First entry of ARRAY type tags */
    WMITLV_TAG_ARRAY_UINT32 = WMITLV_TAG_FIRST_ARRAY_ENUM,
    WMITLV_TAG_ARRAY_BYTE,
    WMITLV_TAG_ARRAY_STRUC,
    WMITLV_TAG_ARRAY_FIXED_STRUC,
    WMITLV_TAG_LAST_ARRAY_ENUM = 31,   /* Last entry of ARRAY type tags */
    WMITLV_TAG_STRUC_wmi_service_ready_event_fixed_param,
    WMITLV_TAG_STRUC_HAL_REG_CAPABILITIES,
    WMITLV_TAG_STRUC_wlan_host_mem_req,
    WMITLV_TAG_STRUC_wmi_ready_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_scan_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_tpc_config_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_chan_info_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_comb_phyerr_rx_hdr,
    WMITLV_TAG_STRUC_wmi_vdev_start_response_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_stopped_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_install_key_complete_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_sta_kickout_event_fixed_param,
	WMITLV_TAG_STRUC_wmi_mgmt_rx_hdr,
    WMITLV_TAG_STRUC_wmi_tbtt_offset_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_tx_delba_complete_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_tx_addba_complete_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_event_fixed_param,
	WMITLV_TAG_STRUC_WOW_EVENT_INFO_fixed_param,
	WMITLV_TAG_STRUC_WOW_EVENT_INFO_SECTION_BITMAP,
    WMITLV_TAG_STRUC_wmi_rtt_event_header,
    WMITLV_TAG_STRUC_wmi_rtt_error_report_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_rtt_meas_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_echo_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_ftm_intg_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_get_keepalive_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_gpio_input_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_csa_event_fixed_param,
    WMITLV_TAG_STRUC_WMI_GTK_OFFLOAD_STATUS_EVENT_fixed_param,
    WMITLV_TAG_STRUC_wmi_igtk_info,
    WMITLV_TAG_STRUC_wmi_dcs_interference_event_fixed_param,
    WMITLV_TAG_STRUC_ath_dcs_cw_int,
    WMITLV_TAG_STRUC_ath_dcs_wlan_int_stat,
    WMITLV_TAG_STRUC_wmi_wlan_profile_ctx_t,
    WMITLV_TAG_STRUC_wmi_wlan_profile_t,
    WMITLV_TAG_STRUC_wmi_pdev_qvit_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_host_swba_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_tim_info,
    WMITLV_TAG_STRUC_wmi_p2p_noa_info,
    WMITLV_TAG_STRUC_wmi_stats_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_avoid_freq_ranges_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_avoid_freq_range_desc,
    WMITLV_TAG_STRUC_wmi_gtk_rekey_fail_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_init_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_resource_config,
    WMITLV_TAG_STRUC_wlan_host_memory_chunk,
    WMITLV_TAG_STRUC_wmi_start_scan_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_stop_scan_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_scan_chan_list_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_channel,
    WMITLV_TAG_STRUC_wmi_pdev_set_regdomain_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_set_param_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_set_wmm_params_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_wmm_params,
    WMITLV_TAG_STRUC_wmi_pdev_set_quiet_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_create_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_delete_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_start_request_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_p2p_noa_descriptor,
    WMITLV_TAG_STRUC_wmi_p2p_go_set_beacon_ie_fixed_param,
    WMITLV_TAG_STRUC_WMI_GTK_OFFLOAD_CMD_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_up_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_stop_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_down_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_set_param_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_install_key_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_create_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_delete_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_flush_tids_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_set_param_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_assoc_complete_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vht_rate_set,
    WMITLV_TAG_STRUC_wmi_bcn_tmpl_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_prb_tmpl_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_bcn_prb_info,
    WMITLV_TAG_STRUC_wmi_peer_tid_addba_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_tid_delba_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_sta_powersave_mode_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_sta_powersave_param_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_sta_dtim_ps_method_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_scan_mode_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_scan_rssi_threshold_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_scan_period_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_scan_rssi_change_threshold_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_suspend_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_resume_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_add_bcn_filter_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_rmv_bcn_filter_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_wow_enable_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_wow_hostwakeup_from_sleep_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_sta_uapsd_auto_trig_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_sta_uapsd_auto_trig_param,
    WMITLV_TAG_STRUC_WMI_SET_ARP_NS_OFFLOAD_CMD_fixed_param,
    WMITLV_TAG_STRUC_WMI_ARP_OFFLOAD_TUPLE,
    WMITLV_TAG_STRUC_WMI_NS_OFFLOAD_TUPLE,
    WMITLV_TAG_STRUC_wmi_ftm_intg_cmd_fixed_param,
    WMITLV_TAG_STRUC_WMI_STA_KEEPALIVE_CMD_fixed_param,
    WMITLV_TAG_STRUC_WMI_STA_KEEPALVE_ARP_RESPONSE,
    WMITLV_TAG_STRUC_wmi_p2p_set_vendor_ie_data_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_ap_ps_peer_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_rate_retry_sched_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_wlan_profile_trigger_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_wlan_profile_set_hist_intvl_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_wlan_profile_get_prof_data_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_wlan_profile_enable_profile_id_cmd_fixed_param,
    WMITLV_TAG_STRUC_WMI_WOW_DEL_PATTERN_CMD_fixed_param,
    WMITLV_TAG_STRUC_WMI_WOW_ADD_DEL_EVT_CMD_fixed_param,
    WMITLV_TAG_STRUC_wmi_rtt_measreq_head,
    WMITLV_TAG_STRUC_wmi_rtt_measreq_body,
    WMITLV_TAG_STRUC_wmi_rtt_tsf_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_spectral_configure_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_spectral_enable_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_request_stats_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_nlo_config_cmd_fixed_param,
    WMITLV_TAG_STRUC_nlo_configured_parameters,
    WMITLV_TAG_STRUC_wmi_csa_offload_enable_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_csa_offload_chanswitch_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_chatter_set_mode_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_echo_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_set_keepalive_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_get_keepalive_cmd_fixed_param,
    WMITLV_TAG_STRUC_WMI_FORCE_FW_HANG_CMD_fixed_param,
    WMITLV_TAG_STRUC_wmi_gpio_config_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_gpio_output_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_add_wds_entry_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_remove_wds_entry_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_bcn_tx_hdr,
    WMITLV_TAG_STRUC_wmi_bcn_send_from_host_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_mgmt_tx_hdr,
    WMITLV_TAG_STRUC_wmi_addba_clear_resp_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_addba_send_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_delba_send_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_addba_setresponse_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_send_singleamsdu_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_pktlog_enable_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_pktlog_disable_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_set_ht_ie_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_set_vht_ie_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_set_dscp_tid_map_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_green_ap_ps_enable_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_get_tpc_config_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_set_base_macaddr_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_mcast_group_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_ap_profile_fixed_param,
    WMITLV_TAG_STRUC_wmi_ap_profile,
    WMITLV_TAG_STRUC_wmi_scan_sch_priority_table_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_dfs_enable_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_dfs_disable_cmd_fixed_param,
    WMITLV_TAG_STRUC_WMI_WOW_ADD_PATTERN_CMD_fixed_param,
    WMITLV_TAG_STRUC_WOW_BITMAP_PATTERN_T,
    WMITLV_TAG_STRUC_WOW_IPV4_SYNC_PATTERN_T,
    WMITLV_TAG_STRUC_WOW_IPV6_SYNC_PATTERN_T,
    WMITLV_TAG_STRUC_WOW_MAGIC_PATTERN_CMD,
    WMITLV_TAG_STRUC_WMI_scan_update_request_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_chatter_pkt_coalescing_filter,
    WMITLV_TAG_STRUC_wmi_chatter_coalescing_add_filter_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_chatter_coalescing_delete_filter_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_chatter_coalescing_query_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_txbf_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_debug_log_config_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_nlo_event,
	WMITLV_TAG_STRUC_wmi_chatter_query_reply_event_fixed_param,
	WMITLV_TAG_STRUC_wmi_upload_h_hdr,
    WMITLV_TAG_STRUC_wmi_capture_h_event_hdr,
    WMITLV_TAG_STRUC_WMI_VDEV_WNM_SLEEPMODE_CMD_fixed_param,
    WMITLV_TAG_STRUC_WMI_VDEV_IPSEC_NATKEEPALIVE_FILTER_CMD_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_wmm_addts_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_wmm_delts_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_set_wmm_params_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_tdls_set_state_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_tdls_peer_update_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_tdls_peer_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_tdls_peer_capabilities,
    WMITLV_TAG_STRUC_wmi_vdev_mcc_set_tbtt_mode_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_chan_list_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_mcc_bcn_intvl_change_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_resmgr_adaptive_ocs_enable_disable_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_resmgr_set_chan_time_quota_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_resmgr_set_chan_latency_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_ba_req_ssn_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_ba_rsp_ssn_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_sta_smps_force_mode_cmd_fixed_param,
    WMITLV_TAG_STRUC_WMI_SET_MCASTBCAST_FILTER_CMD_fixed_param,
    WMITLV_TAG_STRUC_wmi_p2p_set_oppps_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_p2p_set_noa_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_ba_req_ssn_cmd_sub_struct_param,
    WMITLV_TAG_STRUC_wmi_ba_req_ssn_event_sub_struct_param,
    WMITLV_TAG_STRUC_wmi_sta_smps_param_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_set_gtx_params_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_mcc_sched_traffic_stats_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_mcc_sched_sta_traffic_stats,
    WMITLV_TAG_STRUC_wmi_offload_bcn_tx_status_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_p2p_noa_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_hb_set_enable_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_hb_set_tcp_params_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_hb_set_tcp_pkt_filter_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_hb_set_udp_params_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_hb_set_udp_pkt_filter_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_hb_ind_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_tx_pause_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_rfkill_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_dfs_radar_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_dfs_phyerr_filter_ena_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_dfs_phyerr_filter_dis_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_batch_scan_result_scan_list,
    WMITLV_TAG_STRUC_wmi_batch_scan_result_network_info,
    WMITLV_TAG_STRUC_wmi_batch_scan_enable_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_batch_scan_disable_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_batch_scan_trigger_result_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_batch_scan_enabled_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_batch_scan_result_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_plmreq_start_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_plmreq_stop_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_thermal_mgmt_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_thermal_mgmt_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_info_req_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_info_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_info,
    WMITLV_TAG_STRUC_wmi_peer_tx_fail_cnt_thr_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_rmc_set_mode_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_rmc_set_action_period_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_rmc_config_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_mhf_offload_set_mode_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_mhf_offload_plumb_routing_table_cmd_fixed_param,
    WMITLV_TAG_STRUC_WMI_ADD_PROACTIVE_ARP_RSP_PATTERN_CMD_fixed_param,
    WMITLV_TAG_STRUC_WMI_DEL_PROACTIVE_ARP_RSP_PATTERN_CMD_fixed_param,
    WMITLV_TAG_STRUC_wmi_nan_cmd_param,
    WMITLV_TAG_STRUC_wmi_nan_event_hdr,
    WMITLV_TAG_STRUC_wmi_pdev_l1ss_track_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_diag_data_container_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_modem_power_state_cmd_param,
    WMITLV_TAG_STRUC_wmi_peer_get_estimated_linkspeed_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_estimated_linkspeed_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_aggr_state_trig_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_mhf_offload_routing_table_entry,
    WMITLV_TAG_STRUC_wmi_roam_scan_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_req_stats_ext_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_stats_ext_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_obss_scan_enable_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_obss_scan_disable_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_offload_prb_rsp_tx_status_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_set_led_config_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_host_auto_shutdown_cfg_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_host_auto_shutdown_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_update_whal_mib_stats_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_chan_avoid_update_cmd_param,
    WMITLV_TAG_STRUC_WOW_IOAC_PKT_PATTERN_T,
    WMITLV_TAG_STRUC_WOW_IOAC_TMR_PATTERN_T,
    WMITLV_TAG_STRUC_WMI_WOW_IOAC_ADD_KEEPALIVE_CMD_fixed_param,
    WMITLV_TAG_STRUC_WMI_WOW_IOAC_DEL_KEEPALIVE_CMD_fixed_param,
    WMITLV_TAG_STRUC_WMI_WOW_IOAC_KEEPALIVE_T,
    WMITLV_TAG_STRUC_WMI_WOW_IOAC_ADD_PATTERN_CMD_fixed_param,
    WMITLV_TAG_STRUC_WMI_WOW_IOAC_DEL_PATTERN_CMD_fixed_param,
    WMITLV_TAG_STRUC_wmi_start_link_stats_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_clear_link_stats_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_request_link_stats_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_iface_link_stats_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_radio_link_stats_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_stats_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_channel_stats,
    WMITLV_TAG_STRUC_wmi_radio_link_stats,
    WMITLV_TAG_STRUC_wmi_rate_stats,
    WMITLV_TAG_STRUC_wmi_peer_link_stats,
    WMITLV_TAG_STRUC_wmi_wmm_ac_stats,
    WMITLV_TAG_STRUC_wmi_iface_link_stats,
    WMITLV_TAG_STRUC_wmi_lpi_mgmt_snooping_config_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_lpi_start_scan_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_lpi_stop_scan_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_lpi_result_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_state_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_bucket_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_bucket_channel_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_start_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_stop_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_configure_wlan_change_monitor_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_wlan_change_bssid_param_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_configure_hotlist_monitor_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_get_cached_results_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_get_wlan_change_results_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_set_capabilities_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_get_capabilities_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_operation_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_start_stop_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_table_usage_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_wlan_descriptor_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_rssi_info_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_cached_results_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_wlan_change_results_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_wlan_change_result_bssid_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_hotlist_match_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_capabilities_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_cache_capabilities_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_wlan_change_monitor_capabilities_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_hotlist_monitor_capabilities_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_d0_wow_enable_disable_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_d0_wow_disable_ack_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_unit_test_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_offload_tlv_param,
    WMITLV_TAG_STRUC_wmi_roam_11i_offload_tlv_param,
    WMITLV_TAG_STRUC_wmi_roam_11r_offload_tlv_param,
    WMITLV_TAG_STRUC_wmi_roam_ese_offload_tlv_param,
    WMITLV_TAG_STRUC_wmi_roam_synch_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_synch_complete_fixed_param,
    WMITLV_TAG_STRUC_wmi_extwow_enable_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_extwow_set_app_type1_params_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_extwow_set_app_type2_params_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_lpi_status_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_lpi_handoff_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_rate_stats_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_rate_ht_info,
    WMITLV_TAG_STRUC_wmi_ric_request_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_get_temperature_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_temperature_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_set_dhcp_server_offload_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_tpc_chainmask_config_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_ric_tspec,
    WMITLV_TAG_STRUC_wmi_tpc_chainmask_config,
    WMITLV_TAG_STRUCT_wmi_ipa_offload_enable_disable_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_scan_prob_req_oui_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_key_material,
    WMITLV_TAG_STRUC_wmi_tdls_set_offchan_mode_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_set_led_flashing_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_mdns_offload_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_mdns_set_fqdn_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_mdns_set_resp_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_mdns_get_stats_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_mdns_stats_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_invoke_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_resume_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_pdev_set_antenna_diversity_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_sap_ofl_enable_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_sap_ofl_add_sta_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_sap_ofl_del_sta_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_apfind_cmd_param,
    WMITLV_TAG_STRUC_wmi_apfind_event_hdr,
    WMITLV_TAG_STRUC_wmi_ocb_set_sched_cmd_fixed_param, //DEPRECATED
    WMITLV_TAG_STRUC_wmi_ocb_set_sched_event_fixed_param, // DEPRECATED
    WMITLV_TAG_STRUC_wmi_ocb_set_config_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_ocb_set_config_resp_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_ocb_set_utc_time_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_ocb_start_timing_advert_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_ocb_stop_timing_advert_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_ocb_get_tsf_timer_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_ocb_get_tsf_timer_resp_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_dcc_get_stats_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_dcc_channel_stats_request,
    WMITLV_TAG_STRUC_wmi_dcc_get_stats_resp_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_dcc_clear_stats_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_dcc_update_ndl_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_dcc_update_ndl_resp_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_dcc_stats_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_ocb_channel,
    WMITLV_TAG_STRUC_wmi_ocb_schedule_element,
    WMITLV_TAG_STRUC_wmi_dcc_ndl_stats_per_channel,
    WMITLV_TAG_STRUC_wmi_dcc_ndl_chan,
    WMITLV_TAG_STRUC_wmi_qos_parameter,
    WMITLV_TAG_STRUC_wmi_dcc_ndl_active_state_config,
    WMITLV_TAG_STRUC_wmi_roam_scan_extended_threshold_param,
    WMITLV_TAG_STRUC_wmi_roam_filter_fixed_param,
    WMITLV_TAG_STRUC_wmi_passpoint_config_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_passpoint_event_hdr,
    WMITLV_TAG_STRUC_wmi_extscan_configure_hotlist_ssid_monitor_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_hotlist_ssid_match_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_tsf_tstamp_action_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_tsf_report_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_get_fw_mem_dump_fixed_param,
    WMITLV_TAG_STRUC_wmi_update_fw_mem_dump_fixed_param,
    WMITLV_TAG_STRUC_wmi_fw_mem_dump_params,
    WMITLV_TAG_STRUC_wmi_debug_mesg_flush_fixed_param,
    WMITLV_TAG_STRUC_wmi_debug_mesg_flush_complete_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_set_rate_report_condition_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_subnet_change_config_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_set_ie_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_rssi_breach_monitor_config_fixed_param,
    WMITLV_TAG_STRUC_wmi_rssi_breach_event_fixed_param,
    WMITLV_TAG_STRUC_WOW_EVENT_INITIAL_WAKEUP_fixed_param,
    WMITLV_TAG_STRUC_wmi_soc_set_pcl_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_soc_set_hw_mode_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_soc_set_hw_mode_response_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_soc_hw_mode_transition_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_txrx_streams,
    WMITLV_TAG_STRUC_wmi_soc_set_hw_mode_response_vdev_mac_entry,
    WMITLV_TAG_STRUC_wmi_soc_set_dual_mac_config_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_soc_set_dual_mac_config_response_event_fixed_param,
    WMITLV_TAG_STRUC_WOW_IOAC_SOCK_PATTERN_T,
    WMITLV_TAG_STRUC_wmi_wow_enable_icmpv6_na_flt_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_diag_event_log_config_fixed_param,
    WMITLV_TAG_STRUC_wmi_diag_event_log_supported_event_fixed_params,
    WMITLV_TAG_STRUC_wmi_packet_filter_config_fixed_param,
    WMITLV_TAG_STRUC_wmi_packet_filter_enable_fixed_param,
    WMITLV_TAG_STRUC_wmi_sap_set_blacklist_param_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_mgmt_tx_send_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_mgmt_tx_compl_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_soc_set_antenna_mode_cmd_fixed_param,
    WMITLV_TAG_STRUC_WMI_WOW_UDP_SVC_OFLD_CMD_fixed_param,
    WMITLV_TAG_STRUC_wmi_lro_info_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_earlystop_rssi_thres_param,
    WMITLV_TAG_STRUC_wmi_service_ready_ext_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_mawc_sensor_report_ind_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_mawc_enable_sensor_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_configure_mawc_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_nlo_configure_mawc_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_extscan_configure_mawc_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_assoc_conf_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_wow_hostwakeup_gpio_pin_pattern_config_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_ap_ps_egap_param_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_ap_ps_egap_info_event_fixed_param,
    WMITLV_TAG_STRUC_WMI_PMF_OFFLOAD_SET_SA_QUERY_CMD_fixed_param,
    WMITLV_TAG_STRUC_wmi_transfer_data_to_flash_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_transfer_data_to_flash_complete_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_scpc_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_ap_ps_egap_info_chainmask_list,
    WMITLV_TAG_STRUC_wmi_sta_smps_force_mode_complete_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_bpf_get_capability_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_bpf_capability_info_evt_fixed_param,
    WMITLV_TAG_STRUC_wmi_bpf_get_vdev_stats_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_bpf_vdev_stats_info_evt_fixed_param,
    WMITLV_TAG_STRUC_wmi_bpf_set_vdev_instructions_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_bpf_del_vdev_instructions_cmd_fixed_param,
    WMITLV_TAG_STRUC_wmi_vdev_delete_resp_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_peer_delete_resp_event_fixed_param,
    WMITLV_TAG_STRUC_wmi_roam_dense_thres_param,
    WMITLV_TAG_STRUC_enlo_candidate_score_param,
} WMITLV_TAG_ID;

/*
 * IMPORTANT: Please add _ALL_ WMI Commands Here.
 * Otherwise, these WMI TLV Functions will be process them.
 */
#define WMITLV_ALL_CMD_LIST(OP) \
    OP(WMI_INIT_CMDID) \
    OP(WMI_PEER_CREATE_CMDID) \
    OP(WMI_PEER_DELETE_CMDID) \
    OP(WMI_PEER_FLUSH_TIDS_CMDID) \
    OP(WMI_PEER_SET_PARAM_CMDID) \
    OP(WMI_STA_POWERSAVE_MODE_CMDID) \
    OP(WMI_STA_POWERSAVE_PARAM_CMDID) \
    OP(WMI_STA_DTIM_PS_METHOD_CMDID) \
    OP(WMI_PDEV_SET_REGDOMAIN_CMDID) \
    OP(WMI_PEER_TID_ADDBA_CMDID) \
    OP(WMI_PEER_TID_DELBA_CMDID) \
    OP(WMI_PDEV_FTM_INTG_CMDID) \
    OP(WMI_WOW_HOSTWAKEUP_FROM_SLEEP_CMDID) \
    OP(WMI_WOW_ENABLE_CMDID) \
    OP(WMI_RMV_BCN_FILTER_CMDID) \
    OP(WMI_ROAM_SCAN_MODE) \
    OP(WMI_ROAM_SCAN_RSSI_THRESHOLD) \
    OP(WMI_ROAM_SCAN_PERIOD) \
    OP(WMI_ROAM_SCAN_RSSI_CHANGE_THRESHOLD) \
    OP(WMI_START_SCAN_CMDID) \
    OP(WMI_VDEV_PLMREQ_START_CMDID) \
    OP(WMI_VDEV_PLMREQ_STOP_CMDID) \
    OP(WMI_PDEV_SET_CHANNEL_CMDID) \
    OP(WMI_PDEV_SET_WMM_PARAMS_CMDID) \
    OP(WMI_VDEV_START_REQUEST_CMDID) \
    OP(WMI_VDEV_RESTART_REQUEST_CMDID) \
    OP(WMI_P2P_GO_SET_BEACON_IE) \
    OP(WMI_GTK_OFFLOAD_CMDID) \
    OP(WMI_SCAN_CHAN_LIST_CMDID) \
    OP(WMI_STA_UAPSD_AUTO_TRIG_CMDID) \
    OP(WMI_PRB_TMPL_CMDID) \
    OP(WMI_BCN_TMPL_CMDID) \
    OP(WMI_VDEV_INSTALL_KEY_CMDID) \
    OP(WMI_PEER_ASSOC_CMDID) \
    OP(WMI_ADD_BCN_FILTER_CMDID) \
    OP(WMI_STA_KEEPALIVE_CMDID) \
    OP(WMI_SET_ARP_NS_OFFLOAD_CMDID) \
    OP(WMI_P2P_SET_VENDOR_IE_DATA_CMDID) \
    OP(WMI_AP_PS_PEER_PARAM_CMDID) \
    OP(WMI_WLAN_PROFILE_TRIGGER_CMDID) \
    OP(WMI_WLAN_PROFILE_SET_HIST_INTVL_CMDID) \
    OP(WMI_WLAN_PROFILE_GET_PROFILE_DATA_CMDID) \
    OP(WMI_WLAN_PROFILE_ENABLE_PROFILE_ID_CMDID) \
    OP(WMI_WOW_DEL_WAKE_PATTERN_CMDID) \
    OP(WMI_WOW_ENABLE_DISABLE_WAKE_EVENT_CMDID) \
    OP(WMI_RTT_MEASREQ_CMDID) \
    OP(WMI_RTT_TSF_CMDID) \
    OP(WMI_OEM_REQ_CMDID) \
    OP(WMI_VDEV_SPECTRAL_SCAN_CONFIGURE_CMDID) \
    OP(WMI_VDEV_SPECTRAL_SCAN_ENABLE_CMDID) \
    OP(WMI_REQUEST_STATS_CMDID) \
    OP(WMI_NETWORK_LIST_OFFLOAD_CONFIG_CMDID) \
    OP(WMI_CSA_OFFLOAD_ENABLE_CMDID) \
    OP(WMI_CSA_OFFLOAD_CHANSWITCH_CMDID) \
    OP(WMI_CHATTER_SET_MODE_CMDID) \
    OP(WMI_ECHO_CMDID) \
    OP(WMI_PDEV_UTF_CMDID) \
    OP(WMI_PDEV_QVIT_CMDID) \
    OP(WMI_VDEV_SET_KEEPALIVE_CMDID) \
    OP(WMI_VDEV_GET_KEEPALIVE_CMDID) \
    OP(WMI_FORCE_FW_HANG_CMDID) \
    OP(WMI_GPIO_CONFIG_CMDID) \
    OP(WMI_GPIO_OUTPUT_CMDID) \
    OP(WMI_PEER_ADD_WDS_ENTRY_CMDID) \
    OP(WMI_PEER_REMOVE_WDS_ENTRY_CMDID) \
    OP(WMI_BCN_TX_CMDID) \
    OP(WMI_PDEV_SEND_BCN_CMDID) \
    OP(WMI_MGMT_TX_CMDID) \
    OP(WMI_ADDBA_CLEAR_RESP_CMDID) \
    OP(WMI_ADDBA_SEND_CMDID) \
    OP(WMI_DELBA_SEND_CMDID) \
    OP(WMI_ADDBA_SET_RESP_CMDID) \
    OP(WMI_SEND_SINGLEAMSDU_CMDID) \
    OP(WMI_PDEV_PKTLOG_ENABLE_CMDID) \
    OP(WMI_PDEV_PKTLOG_DISABLE_CMDID) \
    OP(WMI_PDEV_SET_HT_CAP_IE_CMDID) \
    OP(WMI_PDEV_SET_VHT_CAP_IE_CMDID) \
    OP(WMI_PDEV_SET_DSCP_TID_MAP_CMDID) \
    OP(WMI_PDEV_GREEN_AP_PS_ENABLE_CMDID) \
    OP(WMI_PDEV_GET_TPC_CONFIG_CMDID) \
    OP(WMI_PDEV_SET_BASE_MACADDR_CMDID) \
    OP(WMI_PEER_MCAST_GROUP_CMDID) \
    OP(WMI_ROAM_AP_PROFILE) \
    OP(WMI_SCAN_SCH_PRIO_TBL_CMDID) \
    OP(WMI_PDEV_DFS_ENABLE_CMDID) \
    OP(WMI_PDEV_DFS_DISABLE_CMDID) \
    OP(WMI_WOW_ADD_WAKE_PATTERN_CMDID) \
    OP(WMI_PDEV_SUSPEND_CMDID) \
    OP(WMI_PDEV_RESUME_CMDID) \
    OP(WMI_STOP_SCAN_CMDID) \
    OP(WMI_PDEV_SET_PARAM_CMDID) \
    OP(WMI_PDEV_SET_QUIET_MODE_CMDID) \
    OP(WMI_VDEV_CREATE_CMDID) \
    OP(WMI_VDEV_DELETE_CMDID) \
    OP(WMI_VDEV_UP_CMDID) \
    OP(WMI_VDEV_STOP_CMDID) \
    OP(WMI_VDEV_DOWN_CMDID) \
    OP(WMI_VDEV_SET_PARAM_CMDID) \
    OP(WMI_SCAN_UPDATE_REQUEST_CMDID) \
    OP(WMI_CHATTER_ADD_COALESCING_FILTER_CMDID) \
    OP(WMI_CHATTER_DELETE_COALESCING_FILTER_CMDID) \
    OP(WMI_CHATTER_COALESCING_QUERY_CMDID) \
    OP(WMI_TXBF_CMDID) \
    OP(WMI_DBGLOG_CFG_CMDID) \
    OP(WMI_VDEV_WNM_SLEEPMODE_CMDID) \
    OP(WMI_VDEV_IPSEC_NATKEEPALIVE_FILTER_CMDID) \
    OP(WMI_VDEV_WMM_ADDTS_CMDID) \
    OP(WMI_VDEV_WMM_DELTS_CMDID) \
    OP(WMI_VDEV_SET_WMM_PARAMS_CMDID) \
    OP(WMI_VDEV_SET_GTX_PARAMS_CMDID) \
    OP(WMI_TDLS_SET_STATE_CMDID) \
    OP(WMI_TDLS_PEER_UPDATE_CMDID) \
    OP(WMI_FWTEST_VDEV_MCC_SET_TBTT_MODE_CMDID) \
    OP(WMI_ROAM_CHAN_LIST)  \
    OP(WMI_RESMGR_ADAPTIVE_OCS_ENABLE_DISABLE_CMDID)\
    OP(WMI_RESMGR_SET_CHAN_TIME_QUOTA_CMDID)    \
    OP(WMI_RESMGR_SET_CHAN_LATENCY_CMDID) \
    OP(WMI_BA_REQ_SSN_CMDID) \
    OP(WMI_STA_SMPS_FORCE_MODE_CMDID) \
    OP(WMI_SET_MCASTBCAST_FILTER_CMDID) \
    OP(WMI_P2P_SET_OPPPS_PARAM_CMDID) \
    OP(WMI_FWTEST_P2P_SET_NOA_PARAM_CMDID) \
    OP(WMI_STA_SMPS_PARAM_CMDID) \
    OP(WMI_MCC_SCHED_TRAFFIC_STATS_CMDID) \
    OP(WMI_HB_SET_ENABLE_CMDID) \
    OP(WMI_HB_SET_TCP_PARAMS_CMDID) \
    OP(WMI_HB_SET_TCP_PKT_FILTER_CMDID) \
    OP(WMI_HB_SET_UDP_PARAMS_CMDID) \
    OP(WMI_HB_SET_UDP_PKT_FILTER_CMDID) \
    OP(WMI_PEER_INFO_REQ_CMDID) \
    OP(WMI_RMC_SET_MODE_CMDID) \
    OP(WMI_RMC_SET_ACTION_PERIOD_CMDID) \
    OP(WMI_RMC_CONFIG_CMDID) \
    OP(WMI_MHF_OFFLOAD_SET_MODE_CMDID) \
    OP(WMI_MHF_OFFLOAD_PLUMB_ROUTING_TBL_CMDID) \
    OP(WMI_DFS_PHYERR_FILTER_ENA_CMDID) \
    OP(WMI_DFS_PHYERR_FILTER_DIS_CMDID) \
    OP(WMI_BATCH_SCAN_ENABLE_CMDID) \
    OP(WMI_BATCH_SCAN_DISABLE_CMDID) \
    OP(WMI_BATCH_SCAN_TRIGGER_RESULT_CMDID) \
    OP(WMI_THERMAL_MGMT_CMDID) \
    OP(WMI_ADD_PROACTIVE_ARP_RSP_PATTERN_CMDID) \
    OP(WMI_DEL_PROACTIVE_ARP_RSP_PATTERN_CMDID) \
    OP(WMI_NAN_CMDID) \
    OP(WMI_MODEM_POWER_STATE_CMDID) \
    OP(WMI_PEER_GET_ESTIMATED_LINKSPEED_CMDID) \
    OP(WMI_ROAM_SCAN_CMD)\
    OP(WMI_REQUEST_STATS_EXT_CMDID) \
    OP(WMI_OBSS_SCAN_ENABLE_CMDID) \
    OP(WMI_OBSS_SCAN_DISABLE_CMDID)\
    OP(WMI_PDEV_SET_LED_CONFIG_CMDID)\
    OP(WMI_HOST_AUTO_SHUTDOWN_CFG_CMDID) \
    OP(WMI_TPC_CHAINMASK_CONFIG_CMDID) \
    OP(WMI_CHAN_AVOID_UPDATE_CMDID) \
    OP(WMI_WOW_IOAC_ADD_KEEPALIVE_CMDID) \
    OP(WMI_WOW_IOAC_DEL_KEEPALIVE_CMDID) \
    OP(WMI_WOW_IOAC_ADD_WAKE_PATTERN_CMDID) \
    OP(WMI_WOW_IOAC_DEL_WAKE_PATTERN_CMDID) \
    OP(WMI_REQUEST_LINK_STATS_CMDID) \
    OP(WMI_START_LINK_STATS_CMDID) \
    OP(WMI_CLEAR_LINK_STATS_CMDID) \
    OP(WMI_LPI_MGMT_SNOOPING_CONFIG_CMDID) \
    OP(WMI_LPI_START_SCAN_CMDID) \
    OP(WMI_LPI_STOP_SCAN_CMDID) \
    OP(WMI_EXTSCAN_START_CMDID) \
    OP(WMI_EXTSCAN_STOP_CMDID) \
    OP(WMI_EXTSCAN_CONFIGURE_WLAN_CHANGE_MONITOR_CMDID) \
    OP(WMI_EXTSCAN_CONFIGURE_HOTLIST_MONITOR_CMDID) \
    OP(WMI_EXTSCAN_GET_CACHED_RESULTS_CMDID) \
    OP(WMI_EXTSCAN_GET_WLAN_CHANGE_RESULTS_CMDID) \
    OP(WMI_EXTSCAN_SET_CAPABILITIES_CMDID) \
    OP(WMI_EXTSCAN_GET_CAPABILITIES_CMDID) \
    OP(WMI_EXTSCAN_CONFIGURE_HOTLIST_SSID_MONITOR_CMDID) \
    OP(WMI_D0_WOW_ENABLE_DISABLE_CMDID) \
    OP(WMI_UNIT_TEST_CMDID) \
    OP(WMI_ROAM_SYNCH_COMPLETE) \
    OP(WMI_EXTWOW_ENABLE_CMDID) \
    OP(WMI_EXTWOW_SET_APP_TYPE1_PARAMS_CMDID) \
    OP(WMI_EXTWOW_SET_APP_TYPE2_PARAMS_CMDID) \
    OP(WMI_ROAM_SET_RIC_REQUEST_CMDID) \
    OP(WMI_PDEV_GET_TEMPERATURE_CMDID) \
    OP(WMI_SET_DHCP_SERVER_OFFLOAD_CMDID) \
    OP(WMI_IPA_OFFLOAD_ENABLE_DISABLE_CMDID)\
    OP(WMI_SCAN_PROB_REQ_OUI_CMDID) \
    OP(WMI_TDLS_SET_OFFCHAN_MODE_CMDID)\
    OP(WMI_PDEV_SET_LED_FLASHING_CMDID) \
    OP(WMI_ROAM_INVOKE_CMDID) \
    OP(WMI_MDNS_OFFLOAD_ENABLE_CMDID) \
    OP(WMI_MDNS_SET_FQDN_CMDID) \
    OP(WMI_MDNS_SET_RESPONSE_CMDID) \
    OP(WMI_MDNS_GET_STATS_CMDID) \
    OP(WMI_SET_ANTENNA_DIVERSITY_CMDID) \
    OP(WMI_SAP_OFL_ENABLE_CMDID) \
    OP(WMI_APFIND_CMDID) \
    OP(WMI_OCB_SET_SCHED_CMDID) \
    OP(WMI_OCB_SET_CONFIG_CMDID) \
    OP(WMI_OCB_SET_UTC_TIME_CMDID) \
    OP(WMI_OCB_START_TIMING_ADVERT_CMDID) \
    OP(WMI_OCB_STOP_TIMING_ADVERT_CMDID) \
    OP(WMI_OCB_GET_TSF_TIMER_CMDID) \
    OP(WMI_DCC_GET_STATS_CMDID) \
    OP(WMI_DCC_CLEAR_STATS_CMDID) \
    OP(WMI_DCC_UPDATE_NDL_CMDID) \
    OP(WMI_ROAM_FILTER_CMDID) \
    OP(WMI_PASSPOINT_LIST_CONFIG_CMDID) \
    OP(WMI_VDEV_TSF_TSTAMP_ACTION_CMDID) \
    OP(WMI_GET_FW_MEM_DUMP_CMDID) \
    OP(WMI_DEBUG_MESG_FLUSH_CMDID) \
    OP(WMI_PEER_SET_RATE_REPORT_CONDITION_CMDID) \
    OP(WMI_ROAM_SUBNET_CHANGE_CONFIG_CMDID) \
    OP(WMI_VDEV_SET_IE_CMDID) \
    OP(WMI_RSSI_BREACH_MONITOR_CONFIG_CMDID) \
    OP(WMI_SOC_SET_PCL_CMDID) \
    OP(WMI_SOC_SET_HW_MODE_CMDID) \
    OP(WMI_SOC_SET_DUAL_MAC_CONFIG_CMDID) \
    OP(WMI_WOW_ENABLE_ICMPV6_NA_FLT_CMDID) \
    OP(WMI_DIAG_EVENT_LOG_CONFIG_CMDID) \
    OP(WMI_PACKET_FILTER_CONFIG_CMDID) \
    OP(WMI_PACKET_FILTER_ENABLE_CMDID) \
    OP(WMI_SAP_SET_BLACKLIST_PARAM_CMDID) \
    OP(WMI_MGMT_TX_SEND_CMDID) \
    OP(WMI_SOC_SET_ANTENNA_MODE_CMDID) \
    OP(WMI_WOW_UDP_SVC_OFLD_CMDID) \
    OP(WMI_LRO_CONFIG_CMDID) \
    OP(WMI_MAWC_SENSOR_REPORT_IND_CMDID) \
    OP(WMI_ROAM_CONFIGURE_MAWC_CMDID) \
    OP(WMI_NLO_CONFIGURE_MAWC_CMDID) \
    OP(WMI_EXTSCAN_CONFIGURE_MAWC_CMDID) \
    OP(WMI_WOW_HOSTWAKEUP_GPIO_PIN_PATTERN_CONFIG_CMDID) \
    OP(WMI_AP_PS_EGAP_PARAM_CMDID) \
    OP(WMI_PMF_OFFLOAD_SET_SA_QUERY_CMDID) \
    OP(WMI_TRANSFER_DATA_TO_FLASH_CMDID) \
    OP(WMI_OEM_REQUEST_CMDID) \
    OP(WMI_BPF_GET_CAPABILITY_CMDID) \
    OP(WMI_BPF_GET_VDEV_STATS_CMDID) \
    OP(WMI_BPF_SET_VDEV_INSTRUCTIONS_CMDID) \
    OP(WMI_BPF_DEL_VDEV_INSTRUCTIONS_CMDID)
/*
 * IMPORTANT: Please add _ALL_ WMI Events Here.
 * Otherwise, these WMI TLV Functions will be process them.
 */
#define WMITLV_ALL_EVT_LIST(OP) \
    OP(WMI_SERVICE_READY_EVENTID) \
    OP(WMI_SERVICE_READY_EXT_EVENTID) \
    OP(WMI_READY_EVENTID) \
    OP(WMI_SCAN_EVENTID) \
    OP(WMI_PDEV_TPC_CONFIG_EVENTID) \
    OP(WMI_CHAN_INFO_EVENTID) \
    OP(WMI_PHYERR_EVENTID) \
    OP(WMI_VDEV_START_RESP_EVENTID) \
    OP(WMI_VDEV_STOPPED_EVENTID) \
    OP(WMI_VDEV_INSTALL_KEY_COMPLETE_EVENTID) \
    OP(WMI_PEER_STA_KICKOUT_EVENTID) \
    OP(WMI_MGMT_RX_EVENTID) \
    OP(WMI_TBTTOFFSET_UPDATE_EVENTID) \
    OP(WMI_TX_DELBA_COMPLETE_EVENTID) \
    OP(WMI_TX_ADDBA_COMPLETE_EVENTID) \
    OP(WMI_ROAM_EVENTID) \
    OP(WMI_WOW_WAKEUP_HOST_EVENTID) \
    OP(WMI_RTT_ERROR_REPORT_EVENTID) \
    OP(WMI_OEM_MEASUREMENT_REPORT_EVENTID) \
    OP(WMI_OEM_ERROR_REPORT_EVENTID) \
    OP(WMI_OEM_CAPABILITY_EVENTID) \
    OP(WMI_ECHO_EVENTID) \
    OP(WMI_PDEV_FTM_INTG_EVENTID) \
    OP(WMI_VDEV_GET_KEEPALIVE_EVENTID) \
    OP(WMI_GPIO_INPUT_EVENTID) \
    OP(WMI_CSA_HANDLING_EVENTID) \
    OP(WMI_DEBUG_MESG_EVENTID) \
    OP(WMI_GTK_OFFLOAD_STATUS_EVENTID) \
    OP(WMI_DCS_INTERFERENCE_EVENTID) \
    OP(WMI_WLAN_PROFILE_DATA_EVENTID) \
    OP(WMI_PDEV_UTF_EVENTID) \
    OP(WMI_DEBUG_PRINT_EVENTID) \
    OP(WMI_RTT_MEASUREMENT_REPORT_EVENTID) \
    OP(WMI_HOST_SWBA_EVENTID) \
    OP(WMI_UPDATE_STATS_EVENTID) \
    OP(WMI_PDEV_QVIT_EVENTID) \
    OP(WMI_WLAN_FREQ_AVOID_EVENTID) \
    OP(WMI_GTK_REKEY_FAIL_EVENTID) \
    OP(WMI_NLO_MATCH_EVENTID) \
    OP(WMI_NLO_SCAN_COMPLETE_EVENTID) \
    OP(WMI_APFIND_EVENTID) \
    OP(WMI_CHATTER_PC_QUERY_EVENTID) \
    OP(WMI_UPLOADH_EVENTID) \
    OP(WMI_CAPTUREH_EVENTID) \
    OP(WMI_TDLS_PEER_EVENTID) \
    OP(WMI_VDEV_MCC_BCN_INTERVAL_CHANGE_REQ_EVENTID) \
    OP(WMI_BA_RSP_SSN_EVENTID) \
    OP(WMI_OFFLOAD_BCN_TX_STATUS_EVENTID) \
    OP(WMI_P2P_NOA_EVENTID) \
    OP(WMI_TX_PAUSE_EVENTID) \
    OP(WMI_RFKILL_STATE_CHANGE_EVENTID) \
    OP(WMI_PEER_INFO_EVENTID) \
    OP(WMI_PEER_TX_FAIL_CNT_THR_EVENTID) \
    OP(WMI_DFS_RADAR_EVENTID) \
    OP(WMI_BATCH_SCAN_ENABLED_EVENTID) \
    OP(WMI_BATCH_SCAN_RESULT_EVENTID) \
    OP(WMI_THERMAL_MGMT_EVENTID) \
    OP(WMI_NAN_EVENTID) \
    OP(WMI_PDEV_L1SS_TRACK_EVENTID) \
    OP(WMI_DIAG_DATA_CONTAINER_EVENTID) \
    OP(WMI_PEER_ESTIMATED_LINKSPEED_EVENTID) \
    OP(WMI_AGGR_STATE_TRIG_EVENTID)\
    OP(WMI_STATS_EXT_EVENTID) \
    OP(WMI_OFFLOAD_PROB_RESP_TX_STATUS_EVENTID) \
    OP(WMI_HOST_AUTO_SHUTDOWN_EVENTID) \
    OP(WMI_UPDATE_WHAL_MIB_STATS_EVENTID) \
    OP(WMI_IFACE_LINK_STATS_EVENTID) \
    OP(WMI_PEER_LINK_STATS_EVENTID) \
    OP(WMI_RADIO_LINK_STATS_EVENTID) \
    OP(WMI_LPI_RESULT_EVENTID) \
    OP(WMI_PEER_STATE_EVENTID) \
    OP(WMI_EXTSCAN_START_STOP_EVENTID) \
    OP(WMI_EXTSCAN_OPERATION_EVENTID) \
    OP(WMI_EXTSCAN_TABLE_USAGE_EVENTID) \
    OP(WMI_EXTSCAN_CACHED_RESULTS_EVENTID) \
    OP(WMI_EXTSCAN_WLAN_CHANGE_RESULTS_EVENTID) \
    OP(WMI_EXTSCAN_HOTLIST_MATCH_EVENTID) \
    OP(WMI_EXTSCAN_CAPABILITIES_EVENTID) \
    OP(WMI_EXTSCAN_HOTLIST_SSID_MATCH_EVENTID) \
    OP(WMI_D0_WOW_DISABLE_ACK_EVENTID) \
    OP(WMI_ROAM_SYNCH_EVENTID) \
    OP(WMI_LPI_STATUS_EVENTID) \
    OP(WMI_LPI_HANDOFF_EVENTID) \
    OP(WMI_UPDATE_VDEV_RATE_STATS_EVENTID) \
    OP(WMI_PDEV_TEMPERATURE_EVENTID) \
    OP(WMI_DIAG_EVENTID) \
    OP(WMI_MDNS_STATS_EVENTID) \
    OP(WMI_PDEV_RESUME_EVENTID) \
    OP(WMI_SAP_OFL_ADD_STA_EVENTID) \
    OP(WMI_SAP_OFL_DEL_STA_EVENTID) \
    OP(WMI_OCB_SET_SCHED_EVENTID) \
    OP(WMI_OCB_SET_CONFIG_RESP_EVENTID) \
    OP(WMI_OCB_GET_TSF_TIMER_RESP_EVENTID) \
    OP(WMI_DCC_GET_STATS_RESP_EVENTID) \
    OP(WMI_DCC_UPDATE_NDL_RESP_EVENTID) \
    OP(WMI_DCC_STATS_EVENTID) \
    OP(WMI_PASSPOINT_MATCH_EVENTID) \
    OP(WMI_VDEV_TSF_REPORT_EVENTID) \
    OP(WMI_UPDATE_FW_MEM_DUMP_EVENTID) \
    OP(WMI_DEBUG_MESG_FLUSH_COMPLETE_EVENTID) \
    OP(WMI_RSSI_BREACH_EVENTID)\
    OP(WMI_WOW_INITIAL_WAKEUP_EVENTID) \
    OP(WMI_SOC_SET_HW_MODE_RESP_EVENTID) \
    OP(WMI_SOC_HW_MODE_TRANSITION_EVENTID) \
    OP(WMI_SOC_SET_DUAL_MAC_CONFIG_RESP_EVENTID) \
    OP(WMI_DIAG_EVENT_LOG_SUPPORTED_EVENTID) \
    OP(WMI_MGMT_TX_COMPLETION_EVENTID) \
    OP(WMI_MAWC_ENABLE_SENSOR_EVENTID) \
    OP(WMI_PEER_ASSOC_CONF_EVENTID) \
    OP(WMI_AP_PS_EGAP_INFO_EVENTID) \
    OP(WMI_TRANSFER_DATA_TO_FLASH_COMPLETE_EVENTID) \
    OP(WMI_OEM_RESPONSE_EVENTID) \
    OP(WMI_PDEV_UTF_SCPC_EVENTID) \
    OP(WMI_STA_SMPS_FORCE_MODE_COMPLETE_EVENTID) \
    OP(WMI_BPF_CAPABILIY_INFO_EVENTID) \
    OP(WMI_BPF_VDEV_STATS_INFO_EVENTID) \
    OP(WMI_VDEV_DELETE_RESP_EVENTID) \
    OP(WMI_PEER_DELETE_RESP_EVENTID)

/* TLV definitions of WMI commands */

/* Init Cmd */
#define WMITLV_TABLE_WMI_INIT_CMDID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_init_cmd_fixed_param, wmi_init_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_resource_config, wmi_resource_config, resource_config, WMITLV_SIZE_FIX)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wlan_host_memory_chunk, host_mem_chunks, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_INIT_CMDID);

/* Peer create Cmd */
#define WMITLV_TABLE_WMI_PEER_CREATE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_create_cmd_fixed_param, wmi_peer_create_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PEER_CREATE_CMDID);

/* Peer delete Cmd */
#define WMITLV_TABLE_WMI_PEER_DELETE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_delete_cmd_fixed_param, wmi_peer_delete_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PEER_DELETE_CMDID);

/* Peer flush Cmd*/
#define WMITLV_TABLE_WMI_PEER_FLUSH_TIDS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_flush_tids_cmd_fixed_param, wmi_peer_flush_tids_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PEER_FLUSH_TIDS_CMDID);

/* Peer Set Param Cmd */
#define WMITLV_TABLE_WMI_PEER_SET_PARAM_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_set_param_cmd_fixed_param, wmi_peer_set_param_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PEER_SET_PARAM_CMDID);

/* STA Powersave Mode Cmd */
#define WMITLV_TABLE_WMI_STA_POWERSAVE_MODE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_sta_powersave_mode_cmd_fixed_param, wmi_sta_powersave_mode_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_STA_POWERSAVE_MODE_CMDID);

/* STA Powersave Param Cmd */
#define WMITLV_TABLE_WMI_STA_POWERSAVE_PARAM_CMDID(id,op,buf,len) \
            WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_sta_powersave_param_cmd_fixed_param, wmi_sta_powersave_param_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_STA_POWERSAVE_PARAM_CMDID);

/* STA DTIM PS METHOD Cmd */
#define WMITLV_TABLE_WMI_STA_DTIM_PS_METHOD_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_sta_dtim_ps_method_cmd_fixed_param, wmi_sta_dtim_ps_method_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_STA_DTIM_PS_METHOD_CMDID);

/* Pdev Set Reg Domain Cmd */
#define WMITLV_TABLE_WMI_PDEV_SET_REGDOMAIN_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_set_regdomain_cmd_fixed_param, wmi_pdev_set_regdomain_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SET_REGDOMAIN_CMDID);


/* Peer TID ADD BA Cmd */
#define WMITLV_TABLE_WMI_PEER_TID_ADDBA_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_tid_addba_cmd_fixed_param, wmi_peer_tid_addba_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PEER_TID_ADDBA_CMDID);

/* Peer TID DEL BA Cmd */
#define WMITLV_TABLE_WMI_PEER_TID_DELBA_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_tid_delba_cmd_fixed_param, wmi_peer_tid_delba_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PEER_TID_DELBA_CMDID);
/* Peer Req Add BA Ssn for staId/tid pair Cmd */
#define WMITLV_TABLE_WMI_BA_REQ_SSN_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ba_req_ssn_cmd_fixed_param, wmi_ba_req_ssn_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len,WMITLV_TAG_ARRAY_STRUC, wmi_ba_req_ssn, ba_req_ssn_list, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_BA_REQ_SSN_CMDID);

/* PDEV FTM integration Cmd */
#define WMITLV_TABLE_WMI_PDEV_FTM_INTG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ftm_intg_cmd_fixed_param, wmi_ftm_intg_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_FTM_INTG_CMDID);

/* WOW Wakeup from sleep Cmd */
#define WMITLV_TABLE_WMI_WOW_HOSTWAKEUP_FROM_SLEEP_CMDID(id,op,buf,len)                                                         \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_wow_hostwakeup_from_sleep_cmd_fixed_param, wmi_wow_hostwakeup_from_sleep_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_WOW_HOSTWAKEUP_FROM_SLEEP_CMDID);

/* WOW Enable Cmd */
#define WMITLV_TABLE_WMI_WOW_ENABLE_CMDID(id,op,buf,len)                                                         \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_wow_enable_cmd_fixed_param, wmi_wow_enable_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_WOW_ENABLE_CMDID);

/* WOW ICMPv6 NA filtering command */
#define WMITLV_TABLE_WMI_WOW_ENABLE_ICMPV6_NA_FLT_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_wow_enable_icmpv6_na_flt_cmd_fixed_param, wmi_wow_enable_icmpv6_na_flt_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_WOW_ENABLE_ICMPV6_NA_FLT_CMDID);

/* Remove Bcn Filter Cmd */
#define WMITLV_TABLE_WMI_RMV_BCN_FILTER_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_rmv_bcn_filter_cmd_fixed_param, wmi_rmv_bcn_filter_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_RMV_BCN_FILTER_CMDID);

/** Service bit WMI_SERVICE_ROAM_OFFLOAD for Roaming feature */
/* Roam scan mode Cmd */
#define WMITLV_TABLE_WMI_ROAM_SCAN_MODE(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_roam_scan_mode_fixed_param, wmi_roam_scan_mode_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_start_scan_cmd_fixed_param, wmi_start_scan_cmd_fixed_param, scan_params, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_roam_offload_tlv_param, offload_param, WMITLV_SIZE_VAR)  \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_roam_11i_offload_tlv_param, offload_11i_param, WMITLV_SIZE_VAR)  \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_roam_11r_offload_tlv_param, offload_11r_param, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_roam_ese_offload_tlv_param, offload_ese_param, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_SCAN_MODE);

/* Roam scan Rssi Threshold Cmd */
#define WMITLV_TABLE_WMI_ROAM_SCAN_RSSI_THRESHOLD(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_roam_scan_rssi_threshold_fixed_param, wmi_roam_scan_rssi_threshold_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_roam_scan_extended_threshold_param, extended_param, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_roam_earlystop_rssi_thres_param, earlystop_param, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_roam_dense_thres_param, dense_param, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_SCAN_RSSI_THRESHOLD);

/* Roam Scan Period Cmd */
#define WMITLV_TABLE_WMI_ROAM_SCAN_PERIOD(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_roam_scan_period_fixed_param, wmi_roam_scan_period_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_SCAN_PERIOD);

/* Roam scan change Rssi Threshold Cmd */
#define WMITLV_TABLE_WMI_ROAM_SCAN_RSSI_CHANGE_THRESHOLD(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_roam_scan_rssi_change_threshold_fixed_param, wmi_roam_scan_rssi_change_threshold_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_SCAN_RSSI_CHANGE_THRESHOLD);
/* Roam Scan Channel list Cmd */
#define WMITLV_TABLE_WMI_ROAM_CHAN_LIST(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_roam_chan_list_fixed_param, wmi_roam_chan_list_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, channel_list, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_CHAN_LIST);

/* Roam scan mode Cmd */
#define WMITLV_TABLE_WMI_ROAM_SCAN_CMD(id,op,buf,len) \
 WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_roam_scan_cmd_fixed_param, wmi_roam_scan_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_SCAN_CMD);


#define WMITLV_TABLE_WMI_VDEV_PLMREQ_START_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_plmreq_start_cmd_fixed_param, wmi_vdev_plmreq_start_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, channel_list, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_PLMREQ_START_CMDID);

#define WMITLV_TABLE_WMI_VDEV_PLMREQ_STOP_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_plmreq_stop_cmd_fixed_param, wmi_vdev_plmreq_stop_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_PLMREQ_STOP_CMDID);

/* Start scan Cmd */
#define WMITLV_TABLE_WMI_START_SCAN_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_start_scan_cmd_fixed_param, wmi_start_scan_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, channel_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_FIXED_STRUC, wmi_ssid, ssid_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_FIXED_STRUC, wmi_mac_addr, bssid_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, ie_data, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_START_SCAN_CMDID);

/* Start ExtScan Cmd */
#define WMITLV_TABLE_WMI_EXTSCAN_START_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_extscan_start_cmd_fixed_param, wmi_extscan_start_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_FIXED_STRUC, wmi_ssid, ssid_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_FIXED_STRUC, wmi_mac_addr, bssid_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, ie_data, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_extscan_bucket, bucket_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_extscan_bucket_channel, channel_list, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_EXTSCAN_START_CMDID);

/* Stop ExtScan Cmd */
#define WMITLV_TABLE_WMI_EXTSCAN_STOP_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_extscan_stop_cmd_fixed_param, wmi_extscan_stop_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_EXTSCAN_STOP_CMDID);

/* Start ExtScan BSSID Monitoring Cmd */
#define WMITLV_TABLE_WMI_EXTSCAN_CONFIGURE_WLAN_CHANGE_MONITOR_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_extscan_configure_wlan_change_monitor_cmd_fixed_param, wmi_extscan_configure_wlan_change_monitor_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_extscan_wlan_change_bssid_param, wlan_change_descriptor_list, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_EXTSCAN_CONFIGURE_WLAN_CHANGE_MONITOR_CMDID);

/* Start Hot List Monitoring Cmd */
#define WMITLV_TABLE_WMI_EXTSCAN_CONFIGURE_HOTLIST_MONITOR_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_extscan_configure_hotlist_monitor_cmd_fixed_param, wmi_extscan_configure_hotlist_monitor_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_extscan_hotlist_entry, hotlist, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_EXTSCAN_CONFIGURE_HOTLIST_MONITOR_CMDID);

/* Get ExtScan BSSID/RSSI list Cmd */
#define WMITLV_TABLE_WMI_EXTSCAN_GET_CACHED_RESULTS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_extscan_get_cached_results_cmd_fixed_param, wmi_extscan_get_cached_results_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_EXTSCAN_GET_CACHED_RESULTS_CMDID);

/* Get ExtScan BSSID monitor results Cmd */
#define WMITLV_TABLE_WMI_EXTSCAN_GET_WLAN_CHANGE_RESULTS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_extscan_get_wlan_change_results_cmd_fixed_param, wmi_extscan_get_wlan_change_results_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_EXTSCAN_GET_WLAN_CHANGE_RESULTS_CMDID);

/* Set ExtScan Capabilities Cmd */
#define WMITLV_TABLE_WMI_EXTSCAN_SET_CAPABILITIES_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_extscan_set_capabilities_cmd_fixed_param, wmi_extscan_set_capabilities_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_extscan_cache_capabilities, extscan_cache_capabilities, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_extscan_wlan_change_monitor_capabilities, wlan_change_capabilities, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_extscan_hotlist_monitor_capabilities, hotlist_capabilities, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_EXTSCAN_SET_CAPABILITIES_CMDID);

/* Get ExtScan Capabilities Cmd */
#define WMITLV_TABLE_WMI_EXTSCAN_GET_CAPABILITIES_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_extscan_get_capabilities_cmd_fixed_param, wmi_extscan_get_capabilities_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_EXTSCAN_GET_CAPABILITIES_CMDID);

/* Start SSID Hot List Monitoring Cmd */
#define WMITLV_TABLE_WMI_EXTSCAN_CONFIGURE_HOTLIST_SSID_MONITOR_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_extscan_configure_hotlist_ssid_monitor_cmd_fixed_param, wmi_extscan_configure_hotlist_ssid_monitor_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_extscan_hotlist_ssid_entry, hotlist_ssid, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_EXTSCAN_CONFIGURE_HOTLIST_SSID_MONITOR_CMDID);

/* P2P set vendor ID data Cmd */
#define WMITLV_TABLE_WMI_P2P_SET_VENDOR_IE_DATA_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_p2p_set_vendor_ie_data_cmd_fixed_param, wmi_p2p_set_vendor_ie_data_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_P2P_SET_VENDOR_IE_DATA_CMDID);
/* P2P set OppPS parameters Cmd */
#define WMITLV_TABLE_WMI_P2P_SET_OPPPS_PARAM_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_p2p_set_oppps_cmd_fixed_param, wmi_p2p_set_oppps_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_P2P_SET_OPPPS_PARAM_CMDID);

/* Pdev set channel Cmd */
#define WMITLV_TABLE_WMI_PDEV_SET_CHANNEL_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_channel, wmi_channel, chan, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SET_CHANNEL_CMDID);

/* Echo Cmd */
#define WMITLV_TABLE_WMI_ECHO_CMDID(id,op,buf,len)                                                         \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_echo_cmd_fixed_param, wmi_echo_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_ECHO_CMDID);

/* Pdev set wmm params */
#define WMITLV_TABLE_WMI_PDEV_SET_WMM_PARAMS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_set_wmm_params_cmd_fixed_param, wmi_pdev_set_wmm_params_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_wmm_params, wmi_wmm_params, wmm_params_ac_be, WMITLV_SIZE_FIX)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_wmm_params, wmi_wmm_params, wmm_params_ac_bk, WMITLV_SIZE_FIX)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_wmm_params, wmi_wmm_params, wmm_params_ac_vi, WMITLV_SIZE_FIX)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_wmm_params, wmi_wmm_params, wmm_params_ac_vo, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SET_WMM_PARAMS_CMDID);

/* Vdev start request Cmd */
#define WMITLV_TABLE_WMI_VDEV_START_REQUEST_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_start_request_cmd_fixed_param, wmi_vdev_start_request_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_channel, wmi_channel, chan, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_p2p_noa_descriptor, noa_descriptors, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_START_REQUEST_CMDID);

/* Vdev restart request cmd */
#define WMITLV_TABLE_WMI_VDEV_RESTART_REQUEST_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_start_request_cmd_fixed_param, wmi_vdev_start_request_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_channel, wmi_channel, chan, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_p2p_noa_descriptor, noa_descriptors, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_RESTART_REQUEST_CMDID);

/* P2P Go set beacon IE cmd */
#define WMITLV_TABLE_WMI_P2P_GO_SET_BEACON_IE(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_p2p_go_set_beacon_ie_fixed_param, wmi_p2p_go_set_beacon_ie_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, ie_data, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_P2P_GO_SET_BEACON_IE);

/* GTK offload Cmd */
#define WMITLV_TABLE_WMI_GTK_OFFLOAD_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_GTK_OFFLOAD_CMD_fixed_param, WMI_GTK_OFFLOAD_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_GTK_OFFLOAD_CMDID);

/* PMF 11w offload Set SA query cmd */
#define WMITLV_TABLE_WMI_PMF_OFFLOAD_SET_SA_QUERY_CMDID(id,op,buf,len) \
   WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_PMF_OFFLOAD_SET_SA_QUERY_CMD_fixed_param, WMI_PMF_OFFLOAD_SET_SA_QUERY_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PMF_OFFLOAD_SET_SA_QUERY_CMDID);

/* Scan channel list Cmd */
#define WMITLV_TABLE_WMI_SCAN_CHAN_LIST_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_scan_chan_list_cmd_fixed_param, wmi_scan_chan_list_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_channel, chan_info, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_SCAN_CHAN_LIST_CMDID);

/* STA UAPSD Auto trigger Cmd */
#define WMITLV_TABLE_WMI_STA_UAPSD_AUTO_TRIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_sta_uapsd_auto_trig_cmd_fixed_param, wmi_sta_uapsd_auto_trig_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_sta_uapsd_auto_trig_param, ac_param, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_STA_UAPSD_AUTO_TRIG_CMDID);

/* Probe template Cmd */
#define WMITLV_TABLE_WMI_PRB_TMPL_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_prb_tmpl_cmd_fixed_param, wmi_prb_tmpl_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_bcn_prb_info, wmi_bcn_prb_info, bcn_prb_info, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, bufp, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_PRB_TMPL_CMDID);

/* Beacon template Cmd */
#define WMITLV_TABLE_WMI_BCN_TMPL_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_bcn_tmpl_cmd_fixed_param, wmi_bcn_tmpl_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_bcn_prb_info, wmi_bcn_prb_info, bcn_prb_info, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, bufp, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_BCN_TMPL_CMDID);

/* VDEV install key complete Cmd */
#define WMITLV_TABLE_WMI_VDEV_INSTALL_KEY_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_install_key_cmd_fixed_param, wmi_vdev_install_key_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, key_data, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_INSTALL_KEY_CMDID);
/* VDEV WNM SLEEP MODE Cmd */
#define WMITLV_TABLE_WMI_VDEV_WNM_SLEEPMODE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_VDEV_WNM_SLEEPMODE_CMD_fixed_param, WMI_VDEV_WNM_SLEEPMODE_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_WNM_SLEEPMODE_CMDID);

#define WMITLV_TABLE_WMI_VDEV_IPSEC_NATKEEPALIVE_FILTER_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_VDEV_IPSEC_NATKEEPALIVE_FILTER_CMD_fixed_param, WMI_VDEV_IPSEC_NATKEEPALIVE_FILTER_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_IPSEC_NATKEEPALIVE_FILTER_CMDID);

/* Peer Assoc Cmd */
#define WMITLV_TABLE_WMI_PEER_ASSOC_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_assoc_complete_cmd_fixed_param, wmi_peer_assoc_complete_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, peer_legacy_rates, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, peer_ht_rates, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vht_rate_set, wmi_vht_rate_set, peer_vht_rates, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PEER_ASSOC_CMDID);

/* Peer Set Rate Report Condition Cmd */
#define WMITLV_TABLE_WMI_PEER_SET_RATE_REPORT_CONDITION_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_set_rate_report_condition_fixed_param, wmi_peer_set_rate_report_condition_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PEER_SET_RATE_REPORT_CONDITION_CMDID);


/* Add Beacon filter Cmd */
#define WMITLV_TABLE_WMI_ADD_BCN_FILTER_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_add_bcn_filter_cmd_fixed_param, wmi_add_bcn_filter_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_FXAR(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, ie_map, WMITLV_SIZE_FIX, BCN_FLT_MAX_ELEMS_IE_LIST)

WMITLV_CREATE_PARAM_STRUC(WMI_ADD_BCN_FILTER_CMDID);

/* Sta keepalive cmd */
#define WMITLV_TABLE_WMI_STA_KEEPALIVE_CMDID(id,op,buf,len)                                           \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_STA_KEEPALIVE_CMD_fixed_param, WMI_STA_KEEPALIVE_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_STA_KEEPALVE_ARP_RESPONSE, WMI_STA_KEEPALVE_ARP_RESPONSE, arp_resp, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_STA_KEEPALIVE_CMDID);

/* ARP NS offload Cmd */
#define WMITLV_TABLE_WMI_SET_ARP_NS_OFFLOAD_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_SET_ARP_NS_OFFLOAD_CMD_fixed_param, WMI_SET_ARP_NS_OFFLOAD_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_FXAR(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, WMI_NS_OFFLOAD_TUPLE, ns_tuples, WMITLV_SIZE_FIX, WMI_MAX_NS_OFFLOADS) \
    WMITLV_FXAR(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, WMI_ARP_OFFLOAD_TUPLE, arp_tuples, WMITLV_SIZE_FIX, WMI_MAX_ARP_OFFLOADS) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, WMI_NS_OFFLOAD_TUPLE, ns_ext_tuples, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_SET_ARP_NS_OFFLOAD_CMDID);

/* AP PS peer param Cmd */
#define WMITLV_TABLE_WMI_AP_PS_PEER_PARAM_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ap_ps_peer_cmd_fixed_param, wmi_ap_ps_peer_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_AP_PS_PEER_PARAM_CMDID);

/* AP PS enhanced green ap param Cmd */
#define WMITLV_TABLE_WMI_AP_PS_EGAP_PARAM_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id, op, buf, len,\
        WMITLV_TAG_STRUC_wmi_ap_ps_egap_param_cmd_fixed_param,\
        wmi_ap_ps_egap_param_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_AP_PS_EGAP_PARAM_CMDID);

/* Profile Trigger Cmd */
#define WMITLV_TABLE_WMI_WLAN_PROFILE_TRIGGER_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_wlan_profile_trigger_cmd_fixed_param, wmi_wlan_profile_trigger_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_WLAN_PROFILE_TRIGGER_CMDID);

/* WLAN Profile set hist interval Cmd */
#define WMITLV_TABLE_WMI_WLAN_PROFILE_SET_HIST_INTVL_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_wlan_profile_set_hist_intvl_cmd_fixed_param, wmi_wlan_profile_set_hist_intvl_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_WLAN_PROFILE_SET_HIST_INTVL_CMDID);

/* WLAN Profile get profile data Cmd */
#define WMITLV_TABLE_WMI_WLAN_PROFILE_GET_PROFILE_DATA_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_wlan_profile_get_prof_data_cmd_fixed_param, wmi_wlan_profile_get_prof_data_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_WLAN_PROFILE_GET_PROFILE_DATA_CMDID);

/* WLAN Profile enable profile ID Cmd */
#define WMITLV_TABLE_WMI_WLAN_PROFILE_ENABLE_PROFILE_ID_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_wlan_profile_enable_profile_id_cmd_fixed_param, wmi_wlan_profile_enable_profile_id_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_WLAN_PROFILE_ENABLE_PROFILE_ID_CMDID);

/* WOW Delete Wake Pattern Cmd */
#define WMITLV_TABLE_WMI_WOW_DEL_WAKE_PATTERN_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_WOW_DEL_PATTERN_CMD_fixed_param, WMI_WOW_DEL_PATTERN_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_WOW_DEL_WAKE_PATTERN_CMDID);

#define WMITLV_TABLE_WMI_WOW_UDP_SVC_OFLD_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_WOW_UDP_SVC_OFLD_CMD_fixed_param, WMI_WOW_UDP_SVC_OFLD_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, pattern, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, response, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_WOW_UDP_SVC_OFLD_CMDID);

#define WMITLV_TABLE_WMI_WOW_HOSTWAKEUP_GPIO_PIN_PATTERN_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id, op, buf, len,\
        WMITLV_TAG_STRUC_wmi_wow_hostwakeup_gpio_pin_pattern_config_cmd_fixed_param,\
        WMI_WOW_HOSTWAKEUP_GPIO_PIN_PATTERN_CONFIG_CMD_fixed_param, fixed_param,\
        WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_WOW_HOSTWAKEUP_GPIO_PIN_PATTERN_CONFIG_CMDID);

/* Wow enable/disable wake up Cmd */
#define WMITLV_TABLE_WMI_WOW_ENABLE_DISABLE_WAKE_EVENT_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_WOW_ADD_DEL_EVT_CMD_fixed_param, WMI_WOW_ADD_DEL_EVT_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_WOW_ENABLE_DISABLE_WAKE_EVENT_CMDID);

/* RTT measurement request Cmd */
#define WMITLV_TABLE_WMI_RTT_MEASREQ_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_RTT_MEASREQ_CMDID);

/* RTT TSF Cmd */
#define WMITLV_TABLE_WMI_RTT_TSF_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_RTT_TSF_CMDID);

/*RTT OEM req Cmd - DEPRECATED */
#define WMITLV_TABLE_WMI_OEM_REQ_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_OEM_REQ_CMDID);

/* RTT OEM request Cmd */
#define WMITLV_TABLE_WMI_OEM_REQUEST_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_OEM_REQUEST_CMDID);

/* Spectral scan configure Cmd */
#define WMITLV_TABLE_WMI_VDEV_SPECTRAL_SCAN_CONFIGURE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_spectral_configure_cmd_fixed_param, wmi_vdev_spectral_configure_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_SPECTRAL_SCAN_CONFIGURE_CMDID);

/* Spectral scan enable Cmd */
#define WMITLV_TABLE_WMI_VDEV_SPECTRAL_SCAN_ENABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_spectral_enable_cmd_fixed_param, wmi_vdev_spectral_enable_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_SPECTRAL_SCAN_ENABLE_CMDID);

/* Request stats Cmd */
#define WMITLV_TABLE_WMI_REQUEST_STATS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_request_stats_cmd_fixed_param, wmi_request_stats_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_REQUEST_STATS_CMDID);


/* Request for memory dump stats Cmd */
#define WMITLV_TABLE_WMI_GET_FW_MEM_DUMP_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_get_fw_mem_dump_fixed_param, wmi_get_fw_mem_dump_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_fw_mem_dump, fw_mem_dump_params, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_GET_FW_MEM_DUMP_CMDID);

/* flush debug messages */
#define WMITLV_TABLE_WMI_DEBUG_MESG_FLUSH_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_debug_mesg_flush_fixed_param, wmi_debug_mesg_flush_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_DEBUG_MESG_FLUSH_CMDID);

/* Request to config the DIAG Events and LOGs*/
#define WMITLV_TABLE_WMI_DIAG_EVENT_LOG_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_diag_event_log_config_fixed_param, wmi_diag_event_log_config_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, diag_events_logs_list, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_DIAG_EVENT_LOG_CONFIG_CMDID);

/* Set config params */
#define WMITLV_TABLE_WMI_START_LINK_STATS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_start_link_stats_cmd_fixed_param, wmi_start_link_stats_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_START_LINK_STATS_CMDID);

/* Request to clear link stats */
#define WMITLV_TABLE_WMI_CLEAR_LINK_STATS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_clear_link_stats_cmd_fixed_param, wmi_clear_link_stats_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_CLEAR_LINK_STATS_CMDID);

/* Request Link stats Cmd */
#define WMITLV_TABLE_WMI_REQUEST_LINK_STATS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_request_link_stats_cmd_fixed_param, wmi_request_link_stats_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_REQUEST_LINK_STATS_CMDID);

/* Network list offload config Cmd */
#define WMITLV_TABLE_WMI_NETWORK_LIST_OFFLOAD_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_nlo_config_cmd_fixed_param, wmi_nlo_config_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, nlo_configured_parameters, nlo_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, channel_list, WMITLV_SIZE_VAR)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, nlo_channel_prediction_cfg, channel_prediction_param, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_enlo_candidate_score_param, enlo_candidate_score_params, candidate_score_params, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_NETWORK_LIST_OFFLOAD_CONFIG_CMDID);

/* Passpoint list offload config Cmd */
#define WMITLV_TABLE_WMI_PASSPOINT_LIST_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_passpoint_config_cmd_fixed_param, wmi_passpoint_config_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PASSPOINT_LIST_CONFIG_CMDID);

/* CSA offload enable Cmd */
#define WMITLV_TABLE_WMI_CSA_OFFLOAD_ENABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_csa_offload_enable_cmd_fixed_param, wmi_csa_offload_enable_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_CSA_OFFLOAD_ENABLE_CMDID);

/* CSA offload channel switch Cmd */
#define WMITLV_TABLE_WMI_CSA_OFFLOAD_CHANSWITCH_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_csa_offload_chanswitch_cmd_fixed_param, wmi_csa_offload_chanswitch_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_channel, wmi_channel, chan, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_CSA_OFFLOAD_CHANSWITCH_CMDID);

/* Chatter set mode Cmd */
#define WMITLV_TABLE_WMI_CHATTER_SET_MODE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_chatter_set_mode_cmd_fixed_param, wmi_chatter_set_mode_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_CHATTER_SET_MODE_CMDID);


/* PDEV UTF Cmd */
#define WMITLV_TABLE_WMI_PDEV_UTF_CMDID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_UTF_CMDID);

/* PDEV QVIT Cmd */
#define WMITLV_TABLE_WMI_PDEV_QVIT_CMDID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_QVIT_CMDID);

/* Vdev Set keep alive Cmd  */
#define WMITLV_TABLE_WMI_VDEV_SET_KEEPALIVE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_set_keepalive_cmd_fixed_param, wmi_vdev_set_keepalive_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_SET_KEEPALIVE_CMDID);

/* Vdev Get keep alive Cmd  */
#define WMITLV_TABLE_WMI_VDEV_GET_KEEPALIVE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_get_keepalive_cmd_fixed_param, wmi_vdev_get_keepalive_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_GET_KEEPALIVE_CMDID);
/*FWTEST Set TBTT mode Cmd*/
#define WMITLV_TABLE_WMI_FWTEST_VDEV_MCC_SET_TBTT_MODE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_mcc_set_tbtt_mode_cmd_fixed_param, wmi_vdev_mcc_set_tbtt_mode_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_FWTEST_VDEV_MCC_SET_TBTT_MODE_CMDID);

/* FWTEST set NoA parameters Cmd */
#define WMITLV_TABLE_WMI_FWTEST_P2P_SET_NOA_PARAM_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_p2p_set_noa_cmd_fixed_param, wmi_p2p_set_noa_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_p2p_noa_descriptor, noa_descriptor, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_FWTEST_P2P_SET_NOA_PARAM_CMDID);

/* Unit test FW */
#define WMITLV_TABLE_WMI_UNIT_TEST_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_unit_test_cmd_fixed_param, wmi_unit_test_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, args, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_UNIT_TEST_CMDID);

/* Force Fw Hang Cmd */
#define WMITLV_TABLE_WMI_FORCE_FW_HANG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_FORCE_FW_HANG_CMD_fixed_param, WMI_FORCE_FW_HANG_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_FORCE_FW_HANG_CMDID);
/* Set Mcast address Cmd */
#define WMITLV_TABLE_WMI_SET_MCASTBCAST_FILTER_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_SET_MCASTBCAST_FILTER_CMD_fixed_param, WMI_SET_MCASTBCAST_FILTER_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_SET_MCASTBCAST_FILTER_CMDID);

/* GPIO config Cmd */
#define WMITLV_TABLE_WMI_GPIO_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_gpio_config_cmd_fixed_param, wmi_gpio_config_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_GPIO_CONFIG_CMDID);

/* GPIO output Cmd */
#define WMITLV_TABLE_WMI_GPIO_OUTPUT_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_gpio_output_cmd_fixed_param, wmi_gpio_output_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_GPIO_OUTPUT_CMDID);

/* Peer add WDA entry Cmd */
#define WMITLV_TABLE_WMI_PEER_ADD_WDS_ENTRY_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_add_wds_entry_cmd_fixed_param, wmi_peer_add_wds_entry_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PEER_ADD_WDS_ENTRY_CMDID);

/*Peer remove WDS entry Cmd */
#define WMITLV_TABLE_WMI_PEER_REMOVE_WDS_ENTRY_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_remove_wds_entry_cmd_fixed_param, wmi_peer_remove_wds_entry_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PEER_REMOVE_WDS_ENTRY_CMDID);

/* Beacon tx Cmd */
#define WMITLV_TABLE_WMI_BCN_TX_CMDID(id,op,buf,len)                                                                                                 \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_bcn_tx_hdr, wmi_bcn_tx_hdr, hdr, WMITLV_SIZE_FIX)   \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, bufp, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_BCN_TX_CMDID);

/* PDEV send Beacon Cmd */
#define WMITLV_TABLE_WMI_PDEV_SEND_BCN_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_bcn_send_from_host_cmd_fixed_param, wmi_bcn_send_from_host_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SEND_BCN_CMDID);

/* Management tx Cmd */
#define WMITLV_TABLE_WMI_MGMT_TX_CMDID(id,op,buf,len)                                                                                                 \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_mgmt_tx_hdr, wmi_mgmt_tx_hdr, hdr, WMITLV_SIZE_FIX)   \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, bufp, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_MGMT_TX_CMDID);

/* Management tx send cmd */
#define WMITLV_TABLE_WMI_MGMT_TX_SEND_CMDID(id,op,buf,len)  \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_mgmt_tx_send_cmd_fixed_param, wmi_mgmt_tx_send_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, bufp, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_MGMT_TX_SEND_CMDID);

/* ADD clear response Cmd */
#define WMITLV_TABLE_WMI_ADDBA_CLEAR_RESP_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_addba_clear_resp_cmd_fixed_param, wmi_addba_clear_resp_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_ADDBA_CLEAR_RESP_CMDID);

/* ADD BA send Cmd */
#define WMITLV_TABLE_WMI_ADDBA_SEND_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_addba_send_cmd_fixed_param, wmi_addba_send_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_ADDBA_SEND_CMDID);

/* DEL BA send Cmd */
#define WMITLV_TABLE_WMI_DELBA_SEND_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_delba_send_cmd_fixed_param, wmi_delba_send_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_DELBA_SEND_CMDID);

/* ADD BA set response Cmd */
#define WMITLV_TABLE_WMI_ADDBA_SET_RESP_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_addba_setresponse_cmd_fixed_param, wmi_addba_setresponse_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_ADDBA_SET_RESP_CMDID);

/* Send single AMSDU Cmd */
#define WMITLV_TABLE_WMI_SEND_SINGLEAMSDU_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_send_singleamsdu_cmd_fixed_param, wmi_send_singleamsdu_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_SEND_SINGLEAMSDU_CMDID);

/* PDev Packet Log enable Cmd */
#define WMITLV_TABLE_WMI_PDEV_PKTLOG_ENABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_pktlog_enable_cmd_fixed_param, wmi_pdev_pktlog_enable_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_PKTLOG_ENABLE_CMDID);

/* PDev Packet Log disable Cmd */
#define WMITLV_TABLE_WMI_PDEV_PKTLOG_DISABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_pktlog_disable_cmd_fixed_param, wmi_pdev_pktlog_disable_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_PKTLOG_DISABLE_CMDID);

/* PDev set HT Cap IE Cmd */
#define WMITLV_TABLE_WMI_PDEV_SET_HT_CAP_IE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_set_ht_ie_cmd_fixed_param, wmi_pdev_set_ht_ie_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, ie_data, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SET_HT_CAP_IE_CMDID);

/* PDev set VHT Cap IE Cmd */
#define WMITLV_TABLE_WMI_PDEV_SET_VHT_CAP_IE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_set_vht_ie_cmd_fixed_param, wmi_pdev_set_vht_ie_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, ie_data, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SET_VHT_CAP_IE_CMDID);

/* PDev Set DSCP to TID map Cmd */
#define WMITLV_TABLE_WMI_PDEV_SET_DSCP_TID_MAP_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_set_dscp_tid_map_cmd_fixed_param, wmi_pdev_set_dscp_tid_map_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SET_DSCP_TID_MAP_CMDID);

/* PDev Green AP PS enable Cmd */
#define WMITLV_TABLE_WMI_PDEV_GREEN_AP_PS_ENABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_green_ap_ps_enable_cmd_fixed_param, wmi_pdev_green_ap_ps_enable_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_GREEN_AP_PS_ENABLE_CMDID);

/* PDEV Get TPC Config Cmd */
#define WMITLV_TABLE_WMI_PDEV_GET_TPC_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_get_tpc_config_cmd_fixed_param, wmi_pdev_get_tpc_config_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_GET_TPC_CONFIG_CMDID);

/* PDEV Set Base Mac Address Cmd */
#define WMITLV_TABLE_WMI_PDEV_SET_BASE_MACADDR_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_set_base_macaddr_cmd_fixed_param, wmi_pdev_set_base_macaddr_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SET_BASE_MACADDR_CMDID);

/* Peer multicast group Cmd */
#define WMITLV_TABLE_WMI_PEER_MCAST_GROUP_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_mcast_group_cmd_fixed_param, wmi_peer_mcast_group_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PEER_MCAST_GROUP_CMDID);

/* Roam AP profile Cmd */
#define WMITLV_TABLE_WMI_ROAM_AP_PROFILE(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_roam_ap_profile_fixed_param, wmi_roam_ap_profile_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ap_profile, wmi_ap_profile, ap_profile, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_AP_PROFILE);

/* Roam sync complete Cmd */
#define WMITLV_TABLE_WMI_ROAM_SYNCH_COMPLETE(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_roam_synch_complete_fixed_param, wmi_roam_synch_complete_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_SYNCH_COMPLETE);

#define WMITLV_TABLE_WMI_ROAM_SET_RIC_REQUEST_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ric_request_fixed_param, wmi_ric_request_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_ric_tspec, ric_tspec_list, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_SET_RIC_REQUEST_CMDID);

/* Scan scheduler priority Table Cmd */
#define WMITLV_TABLE_WMI_SCAN_SCH_PRIO_TBL_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_scan_sch_priority_table_cmd_fixed_param, wmi_scan_sch_priority_table_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, mapping_table, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_SCAN_SCH_PRIO_TBL_CMDID);

/* PDEV DFS enable Cmd */
#define WMITLV_TABLE_WMI_PDEV_DFS_ENABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_dfs_enable_cmd_fixed_param, wmi_pdev_dfs_enable_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_DFS_ENABLE_CMDID);

/* PDEV DFS disable Cmd */
#define WMITLV_TABLE_WMI_PDEV_DFS_DISABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_dfs_disable_cmd_fixed_param, wmi_pdev_dfs_disable_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_DFS_DISABLE_CMDID);

/* DFS phyerr parse/filter offload enable Cmd */
#define WMITLV_TABLE_WMI_DFS_PHYERR_FILTER_ENA_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_dfs_phyerr_filter_ena_cmd_fixed_param, wmi_dfs_phyerr_filter_ena_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_DFS_PHYERR_FILTER_ENA_CMDID);

/* DFS phyerr parse/filter offload disable Cmd */
#define WMITLV_TABLE_WMI_DFS_PHYERR_FILTER_DIS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_dfs_phyerr_filter_dis_cmd_fixed_param, wmi_dfs_phyerr_filter_dis_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_DFS_PHYERR_FILTER_DIS_CMDID);

/* WOW Add Wake Pattern Cmd */
#define WMITLV_TABLE_WMI_WOW_ADD_WAKE_PATTERN_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_WOW_ADD_PATTERN_CMD_fixed_param, WMI_WOW_ADD_PATTERN_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, WOW_BITMAP_PATTERN_T, pattern_info_bitmap, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, WOW_IPV4_SYNC_PATTERN_T, pattern_info_ipv4, WMITLV_SIZE_VAR)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, WOW_IPV6_SYNC_PATTERN_T, pattern_info_ipv6, WMITLV_SIZE_VAR)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, WOW_MAGIC_PATTERN_CMD, pattern_info_magic_pattern, WMITLV_SIZE_VAR)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, pattern_info_timeout, WMITLV_SIZE_VAR) \
    WMITLV_FXAR(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, ra_ratelimit_interval, WMITLV_SIZE_FIX, 1)

WMITLV_CREATE_PARAM_STRUC(WMI_WOW_ADD_WAKE_PATTERN_CMDID);

/* IOAC add keep alive cmd. */
#define WMITLV_TABLE_WMI_WOW_IOAC_ADD_KEEPALIVE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_WOW_IOAC_ADD_KEEPALIVE_CMD_fixed_param, WMI_WOW_IOAC_ADD_KEEPALIVE_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, WMI_WOW_IOAC_KEEPALIVE_T, keepalive_set, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_WOW_IOAC_ADD_KEEPALIVE_CMDID);

/* IOAC del keep alive cmd. */
#define WMITLV_TABLE_WMI_WOW_IOAC_DEL_KEEPALIVE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_WOW_IOAC_DEL_KEEPALIVE_CMD_fixed_param, WMI_WOW_IOAC_DEL_KEEPALIVE_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_WOW_IOAC_DEL_KEEPALIVE_CMDID);

/* WOW IOAC Add Wake Pattern Cmd */
#define WMITLV_TABLE_WMI_WOW_IOAC_ADD_WAKE_PATTERN_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_WOW_IOAC_ADD_PATTERN_CMD_fixed_param, WMI_WOW_IOAC_ADD_PATTERN_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, WOW_IOAC_PKT_PATTERN_T, pattern_info_pkt, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, WOW_IOAC_TMR_PATTERN_T, pattern_info_tmr, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, WOW_IOAC_SOCK_PATTERN_T, pattern_info_sock, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_WOW_IOAC_ADD_WAKE_PATTERN_CMDID);

/* WOW IOAC Delete Wake Pattern Cmd */
#define WMITLV_TABLE_WMI_WOW_IOAC_DEL_WAKE_PATTERN_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_WOW_IOAC_DEL_PATTERN_CMD_fixed_param, WMI_WOW_IOAC_DEL_PATTERN_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_WOW_IOAC_DEL_WAKE_PATTERN_CMDID);

/* extwow enable Cmd */
#define WMITLV_TABLE_WMI_EXTWOW_ENABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_extwow_enable_cmd_fixed_param, wmi_extwow_enable_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_EXTWOW_ENABLE_CMDID);

/* extwow set wakeup params cmd for app type1 */
#define WMITLV_TABLE_WMI_EXTWOW_SET_APP_TYPE1_PARAMS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_extwow_set_app_type1_params_cmd_fixed_param, wmi_extwow_set_app_type1_params_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_EXTWOW_SET_APP_TYPE1_PARAMS_CMDID);

/* extwow set wakeup params cmd for app type2 */
#define WMITLV_TABLE_WMI_EXTWOW_SET_APP_TYPE2_PARAMS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_extwow_set_app_type2_params_cmd_fixed_param, wmi_extwow_set_app_type2_params_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_EXTWOW_SET_APP_TYPE2_PARAMS_CMDID);

/* Stop scan Cmd */
#define WMITLV_TABLE_WMI_STOP_SCAN_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_stop_scan_cmd_fixed_param, wmi_stop_scan_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_STOP_SCAN_CMDID);

#define WMITLV_TABLE_WMI_PDEV_SET_PARAM_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_set_param_cmd_fixed_param, wmi_pdev_set_param_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SET_PARAM_CMDID);

/* PDev set quiet Cmd */
#define WMITLV_TABLE_WMI_PDEV_SET_QUIET_MODE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_set_quiet_cmd_fixed_param, wmi_pdev_set_quiet_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SET_QUIET_MODE_CMDID);

/* Vdev create Cmd */
#define WMITLV_TABLE_WMI_VDEV_CREATE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_create_cmd_fixed_param, wmi_vdev_create_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_vdev_txrx_streams, cfg_txrx_streams, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_CREATE_CMDID);

/* Vdev delete Cmd */
#define WMITLV_TABLE_WMI_VDEV_DELETE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_delete_cmd_fixed_param, wmi_vdev_delete_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)


WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_DELETE_CMDID);

/* Vdev up Cmd */
#define WMITLV_TABLE_WMI_VDEV_UP_CMDID(id,op,buf,len)   \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_up_cmd_fixed_param, wmi_vdev_up_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_UP_CMDID);

/* Vdev stop cmd */
#define WMITLV_TABLE_WMI_VDEV_STOP_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_stop_cmd_fixed_param, wmi_vdev_stop_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_STOP_CMDID);

/* Vdev down Cmd */
#define WMITLV_TABLE_WMI_VDEV_DOWN_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_down_cmd_fixed_param, wmi_vdev_down_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_DOWN_CMDID);

/* Vdev set param Cmd */
#define WMITLV_TABLE_WMI_VDEV_SET_PARAM_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_set_param_cmd_fixed_param, wmi_vdev_set_param_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_SET_PARAM_CMDID);

/* Pdev suspend Cmd */
#define WMITLV_TABLE_WMI_PDEV_SUSPEND_CMDID(id,op,buf,len)                                                         \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_suspend_cmd_fixed_param, wmi_pdev_suspend_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SUSPEND_CMDID);

/* Pdev Resume Cmd */
#define WMITLV_TABLE_WMI_PDEV_RESUME_CMDID(id,op,buf,len)                                                         \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_resume_cmd_fixed_param, wmi_pdev_resume_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_RESUME_CMDID);

#define WMITLV_TABLE_WMI_SCAN_UPDATE_REQUEST_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_scan_update_request_cmd_fixed_param, wmi_scan_update_request_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_SCAN_UPDATE_REQUEST_CMDID);

#define WMITLV_TABLE_WMI_SCAN_PROB_REQ_OUI_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_scan_prob_req_oui_cmd_fixed_param, wmi_scan_prob_req_oui_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_SCAN_PROB_REQ_OUI_CMDID);

#define WMITLV_TABLE_WMI_CHATTER_ADD_COALESCING_FILTER_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len,WMITLV_TAG_STRUC_wmi_chatter_coalescing_add_filter_cmd_fixed_param, wmi_chatter_coalescing_add_filter_cmd_fixed_param, fixed_param,WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, chatter_pkt_coalescing_filter, coalescing_filter, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_CHATTER_ADD_COALESCING_FILTER_CMDID);

#define WMITLV_TABLE_WMI_CHATTER_DELETE_COALESCING_FILTER_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_chatter_coalescing_delete_filter_cmd_fixed_param,wmi_chatter_coalescing_delete_filter_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \

WMITLV_CREATE_PARAM_STRUC(WMI_CHATTER_DELETE_COALESCING_FILTER_CMDID);

#define WMITLV_TABLE_WMI_CHATTER_COALESCING_QUERY_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_chatter_coalescing_query_cmd_fixed_param, wmi_chatter_coalescing_query_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \

WMITLV_CREATE_PARAM_STRUC(WMI_CHATTER_COALESCING_QUERY_CMDID);

#define WMITLV_TABLE_WMI_TXBF_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len,WMITLV_TAG_STRUC_wmi_txbf_cmd_fixed_param, wmi_txbf_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \

WMITLV_CREATE_PARAM_STRUC(WMI_TXBF_CMDID);

#define WMITLV_TABLE_WMI_DBGLOG_CFG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_debug_log_config_cmd_fixed_param, wmi_debug_log_config_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_FXAR(id,op,buf,len,WMITLV_TAG_ARRAY_UINT32, A_UINT32, module_id_bitmap, WMITLV_SIZE_FIX, MAX_MODULE_ID_BITMAP_WORDS) \

WMITLV_CREATE_PARAM_STRUC(WMI_DBGLOG_CFG_CMDID);

#define WMITLV_TABLE_WMI_VDEV_WMM_ADDTS_CMDID(id,op,buf,len)                                           \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_wmm_addts_cmd_fixed_param, wmi_vdev_wmm_addts_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_WMM_ADDTS_CMDID);

#define WMITLV_TABLE_WMI_VDEV_WMM_DELTS_CMDID(id,op,buf,len)                                           \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_wmm_delts_cmd_fixed_param, wmi_vdev_wmm_delts_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_WMM_DELTS_CMDID);

#define WMITLV_TABLE_WMI_VDEV_SET_WMM_PARAMS_CMDID(id,op,buf,len)                                           \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_set_wmm_params_cmd_fixed_param, wmi_vdev_set_wmm_params_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_SET_WMM_PARAMS_CMDID);

#define WMITLV_TABLE_WMI_VDEV_SET_GTX_PARAMS_CMDID(id,op,buf,len)                                           \
WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_set_gtx_params_cmd_fixed_param, wmi_vdev_set_gtx_params_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_SET_GTX_PARAMS_CMDID);

/* TDLS Enable/Disable Cmd */
#define WMITLV_TABLE_WMI_TDLS_SET_STATE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_tdls_set_state_cmd_fixed_param, \
            wmi_tdls_set_state_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_TDLS_SET_STATE_CMDID);

/* TDLS Peer Update Cmd */
#define WMITLV_TABLE_WMI_TDLS_PEER_UPDATE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_tdls_peer_update_cmd_fixed_param, wmi_tdls_peer_update_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_tdls_peer_capabilities, wmi_tdls_peer_capabilities, peer_caps, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_channel, peer_chan_list, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_TDLS_PEER_UPDATE_CMDID);

/* Enable/Disable TDLS Offchannel Cmd */
#define WMITLV_TABLE_WMI_TDLS_SET_OFFCHAN_MODE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_tdls_set_offchan_mode_cmd_fixed_param, \
            wmi_tdls_set_offchan_mode_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_TDLS_SET_OFFCHAN_MODE_CMDID);

/* Resmgr Enable/Disable Adaptive OCS CMD */
#define WMITLV_TABLE_WMI_RESMGR_ADAPTIVE_OCS_ENABLE_DISABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_resmgr_adaptive_ocs_enable_disable_cmd_fixed_param, \
            wmi_resmgr_adaptive_ocs_enable_disable_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_RESMGR_ADAPTIVE_OCS_ENABLE_DISABLE_CMDID);

/* Resmgr Set Channel Time Quota CMD */
#define WMITLV_TABLE_WMI_RESMGR_SET_CHAN_TIME_QUOTA_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_resmgr_set_chan_time_quota_cmd_fixed_param, \
            wmi_resmgr_set_chan_time_quota_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)   \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_RESMGR_SET_CHAN_TIME_QUOTA_CMDID);

/* Resmgr Set Channel Latency CMD */
#define WMITLV_TABLE_WMI_RESMGR_SET_CHAN_LATENCY_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_resmgr_set_chan_latency_cmd_fixed_param, \
            wmi_resmgr_set_chan_latency_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)  \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_RESMGR_SET_CHAN_LATENCY_CMDID);

/* STA SMPS Force Mode CMD */
#define WMITLV_TABLE_WMI_STA_SMPS_FORCE_MODE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_sta_smps_force_mode_cmd_fixed_param, \
            wmi_sta_smps_force_mode_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_STA_SMPS_FORCE_MODE_CMDID);

/* wlan hb enable/disable CMD */
#define WMITLV_TABLE_WMI_HB_SET_ENABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_hb_set_enable_cmd_fixed_param, \
            wmi_hb_set_enable_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_HB_SET_ENABLE_CMDID);

/* wlan hb set tcp params CMD */
#define WMITLV_TABLE_WMI_HB_SET_TCP_PARAMS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_hb_set_tcp_params_cmd_fixed_param, \
            wmi_hb_set_tcp_params_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_HB_SET_TCP_PARAMS_CMDID);

/* wlan hb set tcp pkt filter CMD */
#define WMITLV_TABLE_WMI_HB_SET_TCP_PKT_FILTER_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_hb_set_tcp_pkt_filter_cmd_fixed_param, \
            wmi_hb_set_tcp_pkt_filter_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_HB_SET_TCP_PKT_FILTER_CMDID);

/* wlan set udp params CMD */
#define WMITLV_TABLE_WMI_HB_SET_UDP_PARAMS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_hb_set_udp_params_cmd_fixed_param, \
            wmi_hb_set_udp_params_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_HB_SET_UDP_PARAMS_CMDID);

/* wlan hb set udp pkt filter CMD */
#define WMITLV_TABLE_WMI_HB_SET_UDP_PKT_FILTER_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_hb_set_udp_pkt_filter_cmd_fixed_param, \
            wmi_hb_set_udp_pkt_filter_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_HB_SET_UDP_PKT_FILTER_CMDID);

/* STA SMPS Param CMD */
#define WMITLV_TABLE_WMI_STA_SMPS_PARAM_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_sta_smps_param_cmd_fixed_param, \
            wmi_sta_smps_param_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_STA_SMPS_PARAM_CMDID);

/* MCC Adaptive Scheduler Traffic Stats */
#define WMITLV_TABLE_WMI_MCC_SCHED_TRAFFIC_STATS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_mcc_sched_traffic_stats_cmd_fixed_param, wmi_mcc_sched_traffic_stats_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
				    WMITLV_ELEM(id, op, buf, len, WMITLV_TAG_ARRAY_STRUC, wmi_mcc_sched_sta_traffic_stats, mcc_sched_sta_traffic_stats_list, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_MCC_SCHED_TRAFFIC_STATS_CMDID);

#define WMITLV_TABLE_WMI_BATCH_SCAN_ENABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_batch_scan_enable_cmd_fixed_param, wmi_batch_scan_enable_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_BATCH_SCAN_ENABLE_CMDID);

#define WMITLV_TABLE_WMI_PEER_INFO_REQ_CMDID(id,op,buf,len)   \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_info_req_cmd_fixed_param, \
    wmi_peer_info_req_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_PEER_INFO_REQ_CMDID);

#define WMITLV_TABLE_WMI_RMC_SET_MODE_CMDID(id,op,buf,len)   \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_rmc_set_mode_cmd_fixed_param, \
    wmi_rmc_set_mode_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_RMC_SET_MODE_CMDID);

#define WMITLV_TABLE_WMI_RMC_SET_ACTION_PERIOD_CMDID(id,op,buf,len)   \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_rmc_set_action_period_cmd_fixed_param, \
    wmi_rmc_set_action_period_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_RMC_SET_ACTION_PERIOD_CMDID);

#define WMITLV_TABLE_WMI_RMC_CONFIG_CMDID(id,op,buf,len)   \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_rmc_config_cmd_fixed_param, \
    wmi_rmc_config_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_RMC_CONFIG_CMDID);

#define WMITLV_TABLE_WMI_MHF_OFFLOAD_SET_MODE_CMDID(id,op,buf,len)   \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_mhf_offload_set_mode_cmd_fixed_param, \
    wmi_mhf_offload_set_mode_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_MHF_OFFLOAD_SET_MODE_CMDID);

#define WMITLV_TABLE_WMI_MHF_OFFLOAD_PLUMB_ROUTING_TBL_CMDID(id,op,buf,len)   \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_mhf_offload_plumb_routing_table_cmd_fixed_param, \
                wmi_mhf_offload_plumb_routing_table_cmd, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_mhf_offload_routing_table_entry, \
                routing_tbl_entries, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_MHF_OFFLOAD_PLUMB_ROUTING_TBL_CMDID)

#define WMITLV_TABLE_WMI_BATCH_SCAN_DISABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_batch_scan_disable_cmd_fixed_param, wmi_batch_scan_disable_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_BATCH_SCAN_DISABLE_CMDID);

#define WMITLV_TABLE_WMI_BATCH_SCAN_TRIGGER_RESULT_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_batch_scan_trigger_result_cmd_fixed_param, wmi_batch_scan_trigger_result_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_BATCH_SCAN_TRIGGER_RESULT_CMDID);

/* LPI mgmt snooping config Cmd */
#define WMITLV_TABLE_WMI_LPI_MGMT_SNOOPING_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_lpi_mgmt_snooping_config_cmd_fixed_param, wmi_lpi_mgmt_snooping_config_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_LPI_MGMT_SNOOPING_CONFIG_CMDID);

/* LPI start scan Cmd */
#define WMITLV_TABLE_WMI_LPI_START_SCAN_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_lpi_start_scan_cmd_fixed_param, wmi_lpi_start_scan_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, channel_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_FIXED_STRUC, wmi_ssid, ssid_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_FIXED_STRUC, wmi_mac_addr, bssid_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, ie_data, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_LPI_START_SCAN_CMDID);

/* LPI stop scan Cmd */
#define WMITLV_TABLE_WMI_LPI_STOP_SCAN_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_lpi_stop_scan_cmd_fixed_param, wmi_lpi_stop_scan_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_LPI_STOP_SCAN_CMDID);

#define WMITLV_TABLE_WMI_LPI_RESULT_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_lpi_result_event_fixed_param, wmi_lpi_result_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_LPI_RESULT_EVENTID);

/* LPI Status Event */
#define WMITLV_TABLE_WMI_LPI_STATUS_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_lpi_status_event_fixed_param, wmi_lpi_status_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_LPI_STATUS_EVENTID);

/* LPI Handoff Event */
#define WMITLV_TABLE_WMI_LPI_HANDOFF_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_lpi_handoff_event_fixed_param, wmi_lpi_handoff_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_LPI_HANDOFF_EVENTID);

/* Thermal Manager Params*/
#define WMITLV_TABLE_WMI_THERMAL_MGMT_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_thermal_mgmt_cmd_fixed_param, wmi_thermal_mgmt_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_THERMAL_MGMT_CMDID);

#define WMITLV_TABLE_WMI_ADD_PROACTIVE_ARP_RSP_PATTERN_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_ADD_PROACTIVE_ARP_RSP_PATTERN_CMD_fixed_param, WMI_ADD_PROACTIVE_ARP_RSP_PATTERN_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, pattern, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_ADD_PROACTIVE_ARP_RSP_PATTERN_CMDID);

#define WMITLV_TABLE_WMI_DEL_PROACTIVE_ARP_RSP_PATTERN_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_DEL_PROACTIVE_ARP_RSP_PATTERN_CMD_fixed_param, WMI_DEL_PROACTIVE_ARP_RSP_PATTERN_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_DEL_PROACTIVE_ARP_RSP_PATTERN_CMDID);

/* NaN Request */
#define WMITLV_TABLE_WMI_NAN_CMDID(id,op,buf,len) \
   WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_nan_cmd_param, wmi_nan_cmd_param, fixed_param, WMITLV_SIZE_FIX) \
   WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_NAN_CMDID);

/* Modem power state cmd */
#define WMITLV_TABLE_WMI_MODEM_POWER_STATE_CMDID(id,op,buf,len) \
   WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_modem_power_state_cmd_param, wmi_modem_power_state_cmd_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_MODEM_POWER_STATE_CMDID);

/* get estimated link speed cmd */
#define WMITLV_TABLE_WMI_PEER_GET_ESTIMATED_LINKSPEED_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_get_estimated_linkspeed_cmd_fixed_param, wmi_peer_get_estimated_linkspeed_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PEER_GET_ESTIMATED_LINKSPEED_CMDID);

/* ext stats Request */
#define WMITLV_TABLE_WMI_REQUEST_STATS_EXT_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_req_stats_ext_cmd_fixed_param, wmi_req_stats_ext_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_REQUEST_STATS_EXT_CMDID);

/* 2.4Ghz HT40 OBSS scan enable */
#define WMITLV_TABLE_WMI_OBSS_SCAN_ENABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id, op, buf, len, WMITLV_TAG_STRUC_wmi_obss_scan_enable_cmd_fixed_param, wmi_obss_scan_enable_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id, op, buf, len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, channels, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id, op, buf, len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, ie_field, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_OBSS_SCAN_ENABLE_CMDID);

/* 2.4Ghz HT40 OBSS scan disable */
#define WMITLV_TABLE_WMI_OBSS_SCAN_DISABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id, op, buf, len, WMITLV_TAG_STRUC_wmi_obss_scan_disable_cmd_fixed_param, wmi_obss_scan_disable_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_OBSS_SCAN_DISABLE_CMDID);

/* Pdev Set LED Config Cmd */
#define WMITLV_TABLE_WMI_PDEV_SET_LED_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_set_led_config_cmd_fixed_param, wmi_pdev_set_led_config_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SET_LED_CONFIG_CMDID);

/* host auto shut down config cmd */
#define WMITLV_TABLE_WMI_HOST_AUTO_SHUTDOWN_CFG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_host_auto_shutdown_cfg_cmd_fixed_param, wmi_host_auto_shutdown_cfg_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_HOST_AUTO_SHUTDOWN_CFG_CMDID);

/* tpc chainmask config cmd */
#define WMITLV_TABLE_WMI_TPC_CHAINMASK_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_tpc_chainmask_config_cmd_fixed_param, wmi_tpc_chainmask_config_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)  \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_tpc_chainmask_config, config_list, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_TPC_CHAINMASK_CONFIG_CMDID);

/* Ch avoidance update cmd */
#define WMITLV_TABLE_WMI_CHAN_AVOID_UPDATE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_chan_avoid_update_cmd_param, wmi_chan_avoid_update_cmd_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_CHAN_AVOID_UPDATE_CMDID);

/* D0-WOW Enable Disable Cmd */
#define WMITLV_TABLE_WMI_D0_WOW_ENABLE_DISABLE_CMDID(id,op,buf,len)                                                         \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_d0_wow_enable_disable_cmd_fixed_param, wmi_d0_wow_enable_disable_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_D0_WOW_ENABLE_DISABLE_CMDID);

/* Pdev get chip temperature Cmd */
#define WMITLV_TABLE_WMI_PDEV_GET_TEMPERATURE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_get_temperature_cmd_fixed_param, wmi_pdev_get_temperature_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_GET_TEMPERATURE_CMDID);

/* Set antenna diversity Cmd */
#define WMITLV_TABLE_WMI_SET_ANTENNA_DIVERSITY_CMDID(id,op,buf,len) \
WMITLV_ELEM(id, op, buf, len, WMITLV_TAG_STRUC_wmi_pdev_set_antenna_diversity_cmd_fixed_param, wmi_pdev_set_antenna_diversity_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_SET_ANTENNA_DIVERSITY_CMDID);

/* Set rssi monitoring config Cmd */
#define WMITLV_TABLE_WMI_RSSI_BREACH_MONITOR_CONFIG_CMDID(id,op,buf,len) \
WMITLV_ELEM(id, op, buf, len, WMITLV_TAG_STRUC_wmi_rssi_breach_monitor_config_fixed_param, wmi_rssi_breach_monitor_config_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_RSSI_BREACH_MONITOR_CONFIG_CMDID);

/* DHCP server offload param Cmd */
#define WMITLV_TABLE_WMI_SET_DHCP_SERVER_OFFLOAD_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_set_dhcp_server_offload_cmd_fixed_param, wmi_set_dhcp_server_offload_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_SET_DHCP_SERVER_OFFLOAD_CMDID);

/* IPA Offload Enable Disable Cmd */
#define WMITLV_TABLE_WMI_IPA_OFFLOAD_ENABLE_DISABLE_CMDID(id,op,buf,len)                                                         \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUCT_wmi_ipa_offload_enable_disable_cmd_fixed_param, wmi_ipa_offload_enable_disable_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_IPA_OFFLOAD_ENABLE_DISABLE_CMDID);

/* Set LED flashing parameter Cmd */
#define WMITLV_TABLE_WMI_PDEV_SET_LED_FLASHING_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_set_led_flashing_cmd_fixed_param, wmi_set_led_flashing_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_SET_LED_FLASHING_CMDID);

/* mDNS responder offload param Cmd */
#define WMITLV_TABLE_WMI_MDNS_OFFLOAD_ENABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_mdns_offload_cmd_fixed_param, wmi_mdns_offload_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_MDNS_OFFLOAD_ENABLE_CMDID);

#define WMITLV_TABLE_WMI_MDNS_SET_FQDN_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_mdns_set_fqdn_cmd_fixed_param, wmi_mdns_set_fqdn_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, fqdn_data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_MDNS_SET_FQDN_CMDID);

#define WMITLV_TABLE_WMI_MDNS_SET_RESPONSE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_mdns_set_resp_cmd_fixed_param, wmi_mdns_set_resp_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, resp_data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_MDNS_SET_RESPONSE_CMDID);

#define WMITLV_TABLE_WMI_MDNS_GET_STATS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_mdns_get_stats_cmd_fixed_param, wmi_mdns_get_stats_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_MDNS_GET_STATS_CMDID);

/* roam invoke Cmd */
#define WMITLV_TABLE_WMI_ROAM_INVOKE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_roam_invoke_cmd_fixed_param, wmi_roam_invoke_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, channel_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_FIXED_STRUC, wmi_mac_addr, bssid_list, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_INVOKE_CMDID);

/* SAP Authentication offload param Cmd */
#define WMITLV_TABLE_WMI_SAP_OFL_ENABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_sap_ofl_enable_cmd_fixed_param, wmi_sap_ofl_enable_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, psk, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_SAP_OFL_ENABLE_CMDID);

/* SAP set blacklist param cmd */
#define WMITLV_TABLE_WMI_SAP_SET_BLACKLIST_PARAM_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_sap_set_blacklist_param_cmd_fixed_param, wmi_sap_set_blacklist_param_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_SAP_SET_BLACKLIST_PARAM_CMDID);

/* APFIND Request */
#define WMITLV_TABLE_WMI_APFIND_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_apfind_cmd_param, wmi_apfind_cmd_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_APFIND_CMDID);

/* Set OCB schedule cmd, DEPRECATED */
#define WMITLV_TABLE_WMI_OCB_SET_SCHED_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id, op, buf, len, WMITLV_TAG_STRUC_wmi_ocb_set_sched_cmd_fixed_param, wmi_ocb_set_sched_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_OCB_SET_SCHED_CMDID);

/* Set OCB configuration cmd */
#define WMITLV_TABLE_WMI_OCB_SET_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ocb_set_config_cmd_fixed_param, wmi_ocb_set_config_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_channel, chan_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_ocb_channel, ocb_chan_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_qos_parameter, qos_parameter_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_dcc_ndl_chan, chan_cfg, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_dcc_ndl_active_state_config, ndl_active_state_config_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_ocb_schedule_element, schedule_list, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_OCB_SET_CONFIG_CMDID);

/* Set UTC time cmd */
#define WMITLV_TABLE_WMI_OCB_SET_UTC_TIME_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ocb_set_utc_time_cmd_fixed_param, wmi_ocb_set_utc_time_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_OCB_SET_UTC_TIME_CMDID);

/* Start timing advertisement cmd */
#define WMITLV_TABLE_WMI_OCB_START_TIMING_ADVERT_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ocb_start_timing_advert_cmd_fixed_param, wmi_ocb_start_timing_advert_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, bufp, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_OCB_START_TIMING_ADVERT_CMDID);

/* Stop timing advertisement cmd */
#define WMITLV_TABLE_WMI_OCB_STOP_TIMING_ADVERT_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ocb_stop_timing_advert_cmd_fixed_param, wmi_ocb_stop_timing_advert_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_OCB_STOP_TIMING_ADVERT_CMDID);

/* Get TSF timer cmd */
#define WMITLV_TABLE_WMI_OCB_GET_TSF_TIMER_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ocb_get_tsf_timer_cmd_fixed_param, wmi_ocb_get_tsf_timer_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_OCB_GET_TSF_TIMER_CMDID);

/* Get DCC stats cmd */
#define WMITLV_TABLE_WMI_DCC_GET_STATS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_dcc_get_stats_cmd_fixed_param, wmi_dcc_get_stats_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_dcc_channel_stats_request, channel_stats_request, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_DCC_GET_STATS_CMDID);

/* Clear DCC stats cmd */
#define WMITLV_TABLE_WMI_DCC_CLEAR_STATS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_dcc_clear_stats_cmd_fixed_param, wmi_dcc_clear_stats_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_DCC_CLEAR_STATS_CMDID);

/* Update DCC NDL cmd */
#define WMITLV_TABLE_WMI_DCC_UPDATE_NDL_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_dcc_update_ndl_cmd_fixed_param, wmi_dcc_update_ndl_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_dcc_ndl_chan, chan_ndl_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_dcc_ndl_active_state_config, ndl_active_state_config_list, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_DCC_UPDATE_NDL_CMDID);

/* Roam filter cmd */
#define WMITLV_TABLE_WMI_ROAM_FILTER_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_roam_filter_fixed_param, wmi_roam_filter_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_FIXED_STRUC, wmi_mac_addr, bssid_black_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_FIXED_STRUC, wmi_ssid, ssid_white_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_FIXED_STRUC, wmi_mac_addr, bssid_preferred_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, bssid_preferred_factor, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_FILTER_CMDID);

/* TSF timestamp action cmd */
#define WMITLV_TABLE_WMI_VDEV_TSF_TSTAMP_ACTION_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_tsf_tstamp_action_cmd_fixed_param, wmi_vdev_tsf_tstamp_action_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_TSF_TSTAMP_ACTION_CMDID);

/* LFR subnet change config Cmd */
#define WMITLV_TABLE_WMI_ROAM_SUBNET_CHANGE_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_roam_subnet_change_config_fixed_param, wmi_roam_subnet_change_config_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_FIXED_STRUC, wmi_mac_addr, skip_subnet_change_detection_bssid_list, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_SUBNET_CHANGE_CONFIG_CMDID);

/* Set the SOC Preferred Channel List (PCL) Cmd */
#define WMITLV_TABLE_WMI_SOC_SET_PCL_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_soc_set_pcl_cmd_fixed_param, wmi_soc_set_pcl_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, channel_list, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_SOC_SET_PCL_CMDID);

/* Set the SOC Hardware Mode Cmd */
#define WMITLV_TABLE_WMI_SOC_SET_HW_MODE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_soc_set_hw_mode_cmd_fixed_param, wmi_soc_set_hw_mode_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_SOC_SET_HW_MODE_CMDID);

/* Set the SOC Dual MAC Config Cmd */
#define WMITLV_TABLE_WMI_SOC_SET_DUAL_MAC_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_soc_set_dual_mac_config_cmd_fixed_param, wmi_soc_set_dual_mac_config_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_SOC_SET_DUAL_MAC_CONFIG_CMDID);

/* Set the SOC Antenna Mode Cmd */
#define WMITLV_TABLE_WMI_SOC_SET_ANTENNA_MODE_CMDID(id, op, buf, len) \
    WMITLV_ELEM(id, op, buf, len, WMITLV_TAG_STRUC_wmi_soc_set_antenna_mode_cmd_fixed_param, wmi_soc_set_antenna_mode_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_SOC_SET_ANTENNA_MODE_CMDID);

#define WMITLV_TABLE_WMI_LRO_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_lro_info_cmd_fixed_param, wmi_lro_info_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_LRO_CONFIG_CMDID);

#define WMITLV_TABLE_WMI_TRANSFER_DATA_TO_FLASH_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_transfer_data_to_flash_cmd_fixed_param, wmi_transfer_data_to_flash_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_TRANSFER_DATA_TO_FLASH_CMDID);

/* MAWC sensor report indication cmd */
#define WMITLV_TABLE_WMI_MAWC_SENSOR_REPORT_IND_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_mawc_sensor_report_ind_cmd_fixed_param, wmi_mawc_sensor_report_ind_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_MAWC_SENSOR_REPORT_IND_CMDID);

/* Roam configure MAWC cmd */
#define WMITLV_TABLE_WMI_ROAM_CONFIGURE_MAWC_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_roam_configure_mawc_cmd_fixed_param, wmi_roam_configure_mawc_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_CONFIGURE_MAWC_CMDID);

/* NLO configure MAWC cmd */
#define WMITLV_TABLE_WMI_NLO_CONFIGURE_MAWC_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_nlo_configure_mawc_cmd_fixed_param, wmi_nlo_configure_mawc_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_NLO_CONFIGURE_MAWC_CMDID);

/* Extscan configure MAWC cmd */
#define WMITLV_TABLE_WMI_EXTSCAN_CONFIGURE_MAWC_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_extscan_configure_mawc_cmd_fixed_param, wmi_extscan_configure_mawc_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_EXTSCAN_CONFIGURE_MAWC_CMDID);

/* bpf offload capability get cmd */
#define WMITLV_TABLE_WMI_BPF_GET_CAPABILITY_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_bpf_get_capability_cmd_fixed_param, wmi_bpf_get_capability_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_BPF_GET_CAPABILITY_CMDID);

/* bpf offload get vdev status cmd */
#define WMITLV_TABLE_WMI_BPF_GET_VDEV_STATS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_bpf_get_vdev_stats_cmd_fixed_param, wmi_bpf_get_vdev_stats_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_BPF_GET_VDEV_STATS_CMDID);

/* bpf offload set vdev instructions cmd */
#define WMITLV_TABLE_WMI_BPF_SET_VDEV_INSTRUCTIONS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_bpf_set_vdev_instructions_cmd_fixed_param, wmi_bpf_set_vdev_instructions_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, buf_inst, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_BPF_SET_VDEV_INSTRUCTIONS_CMDID);

/* bpf offload delete vdev instructions cmd */
#define WMITLV_TABLE_WMI_BPF_DEL_VDEV_INSTRUCTIONS_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_bpf_del_vdev_instructions_cmd_fixed_param, wmi_bpf_del_vdev_instructions_cmd_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_BPF_DEL_VDEV_INSTRUCTIONS_CMDID);

/************************** TLV definitions of WMI events *******************************/

/* Service Ready event */
#define WMITLV_TABLE_WMI_SERVICE_READY_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_service_ready_event_fixed_param, wmi_service_ready_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)     \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_HAL_REG_CAPABILITIES, HAL_REG_CAPABILITIES, hal_reg_capabilities, WMITLV_SIZE_FIX) \
    WMITLV_FXAR(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, wmi_service_bitmap, WMITLV_SIZE_FIX, WMI_SERVICE_BM_SIZE) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wlan_host_mem_req, mem_reqs, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, wlan_dbs_hw_mode_list, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_SERVICE_READY_EVENTID);

/* Service Ready Extension event */
#define WMITLV_TABLE_WMI_SERVICE_READY_EXT_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_service_ready_ext_event_fixed_param, wmi_service_ready_ext_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_SERVICE_READY_EXT_EVENTID);

/* Ready event */
#define WMITLV_TABLE_WMI_READY_EVENTID(id,op,buf,len)                                                                                                 \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ready_event_fixed_param, wmi_ready_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_READY_EVENTID);

/* Scan Event */
#define WMITLV_TABLE_WMI_SCAN_EVENTID(id,op,buf,len)                                                                                                 \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_scan_event_fixed_param, wmi_scan_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_SCAN_EVENTID);

/* ExtScan Start/Stop Event */
#define WMITLV_TABLE_WMI_EXTSCAN_START_STOP_EVENTID(id,op,buf,len)                                                                                                 \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_extscan_start_stop_event_fixed_param, wmi_extscan_start_stop_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_EXTSCAN_START_STOP_EVENTID);

/* ExtScan Event */
#define WMITLV_TABLE_WMI_EXTSCAN_OPERATION_EVENTID(id,op,buf,len)                                                                                                 \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_extscan_operation_event_fixed_param, wmi_extscan_operation_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, bucket_id, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_EXTSCAN_OPERATION_EVENTID);

/* ExtScan Table Usage Event */
#define WMITLV_TABLE_WMI_EXTSCAN_TABLE_USAGE_EVENTID(id,op,buf,len)                                                                                                 \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_extscan_table_usage_event_fixed_param, wmi_extscan_table_usage_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_EXTSCAN_TABLE_USAGE_EVENTID);

/* ExtScan Result Event */
#define WMITLV_TABLE_WMI_EXTSCAN_CACHED_RESULTS_EVENTID(id,op,buf,len)                                                                                                 \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_extscan_cached_results_event_fixed_param, wmi_extscan_cached_results_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_extscan_wlan_descriptor, bssid_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_extscan_rssi_info, rssi_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, ie_list, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_EXTSCAN_CACHED_RESULTS_EVENTID);

/* ExtScan Monitor RSSI List Event */
#define WMITLV_TABLE_WMI_EXTSCAN_WLAN_CHANGE_RESULTS_EVENTID(id,op,buf,len)                                                                                                 \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_extscan_wlan_change_results_event_fixed_param, wmi_extscan_wlan_change_results_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_extscan_wlan_change_result_bssid, bssid_signal_descriptor_list, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, rssi_list, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_EXTSCAN_WLAN_CHANGE_RESULTS_EVENTID);

/* ExtScan Hot List Match Event */
#define WMITLV_TABLE_WMI_EXTSCAN_HOTLIST_MATCH_EVENTID(id,op,buf,len)                                                                                                 \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_extscan_hotlist_match_event_fixed_param, wmi_extscan_hotlist_match_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_extscan_wlan_descriptor, hotlist_match, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_EXTSCAN_HOTLIST_MATCH_EVENTID);

/* ExtScan Hot List Match Event */
#define WMITLV_TABLE_WMI_EXTSCAN_CAPABILITIES_EVENTID(id,op,buf,len)                                                                                                 \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_extscan_capabilities_event_fixed_param, wmi_extscan_capabilities_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_extscan_cache_capabilities, extscan_cache_capabilities, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_extscan_wlan_change_monitor_capabilities, wlan_change_capabilities, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_extscan_hotlist_monitor_capabilities, hotlist_capabilities, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_EXTSCAN_CAPABILITIES_EVENTID);

/* ExtScan Hot List Match Event */
#define WMITLV_TABLE_WMI_EXTSCAN_HOTLIST_SSID_MATCH_EVENTID(id,op,buf,len)                                                                                                 \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_extscan_hotlist_ssid_match_event_fixed_param, wmi_extscan_hotlist_ssid_match_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_extscan_wlan_descriptor, hotlist_ssid_match, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_EXTSCAN_HOTLIST_SSID_MATCH_EVENTID);

/* Update_whal_mib_stats Event */
#define WMITLV_TABLE_WMI_UPDATE_WHAL_MIB_STATS_EVENTID(id,op,buf,len)                                                                                                 \
    WMITLV_ELEM(id, op, buf, len, WMITLV_TAG_STRUC_wmi_update_whal_mib_stats_event_fixed_param, wmi_update_whal_mib_stats_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_UPDATE_WHAL_MIB_STATS_EVENTID);

/* PDEV TPC Config Event */
#define WMITLV_TABLE_WMI_PDEV_TPC_CONFIG_EVENTID(id,op,buf,len)                                                                                                 \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_tpc_config_event_fixed_param, wmi_pdev_tpc_config_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, ratesArray, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_TPC_CONFIG_EVENTID);

/* Channel Info Event */
#define WMITLV_TABLE_WMI_CHAN_INFO_EVENTID(id,op,buf,len)                                                                                                 \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_chan_info_event_fixed_param, wmi_chan_info_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_CHAN_INFO_EVENTID);

/* Phy Error Event */
#define WMITLV_TABLE_WMI_PHYERR_EVENTID(id,op,buf,len)                                                                                                 \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_comb_phyerr_rx_hdr, wmi_comb_phyerr_rx_hdr, hdr, WMITLV_SIZE_FIX)   \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, bufp, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PHYERR_EVENTID);

/* TX Pause/Unpause event */
#define WMITLV_TABLE_WMI_TX_PAUSE_EVENTID(id,op,buf,len)                                                                                                 \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_tx_pause_event_fixed_param, wmi_tx_pause_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_TX_PAUSE_EVENTID);

/* Mgmt TX completion event */
#define WMITLV_TABLE_WMI_MGMT_TX_COMPLETION_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_mgmt_tx_compl_event_fixed_param, wmi_mgmt_tx_compl_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_MGMT_TX_COMPLETION_EVENTID);

/* VDEV Start response Event */
#define WMITLV_TABLE_WMI_VDEV_START_RESP_EVENTID(id,op,buf,len)                                                         \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_start_response_event_fixed_param, wmi_vdev_start_response_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_START_RESP_EVENTID);

/* VDEV Stopped Event */
#define WMITLV_TABLE_WMI_VDEV_STOPPED_EVENTID(id,op,buf,len)                                                         \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_stopped_event_fixed_param, wmi_vdev_stopped_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_STOPPED_EVENTID);

/* VDEV delete response Event */
#define WMITLV_TABLE_WMI_VDEV_DELETE_RESP_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_delete_resp_event_fixed_param, wmi_vdev_delete_resp_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_DELETE_RESP_EVENTID);

/* VDEV Install Key Complete Event */
#define WMITLV_TABLE_WMI_VDEV_INSTALL_KEY_COMPLETE_EVENTID(id,op,buf,len)                                           \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_install_key_complete_event_fixed_param, wmi_vdev_install_key_complete_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_INSTALL_KEY_COMPLETE_EVENTID);

/* Peer STA Kickout Event */
#define WMITLV_TABLE_WMI_PEER_STA_KICKOUT_EVENTID(id,op,buf,len)                                                         \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_sta_kickout_event_fixed_param, wmi_peer_sta_kickout_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PEER_STA_KICKOUT_EVENTID);

/* Management Rx Event */
#define WMITLV_TABLE_WMI_MGMT_RX_EVENTID(id,op,buf,len)                                                                                                 \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_mgmt_rx_hdr, wmi_mgmt_rx_hdr, hdr, WMITLV_SIZE_FIX)   \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, bufp, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_MGMT_RX_EVENTID);

/* TBTT offset Event */
#define WMITLV_TABLE_WMI_TBTTOFFSET_UPDATE_EVENTID(id,op,buf,len)                                                         \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_tbtt_offset_event_fixed_param, wmi_tbtt_offset_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_FXAR(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, tbttoffset_list, WMITLV_SIZE_FIX, WMI_MAX_AP_VDEV)
WMITLV_CREATE_PARAM_STRUC(WMI_TBTTOFFSET_UPDATE_EVENTID);

/* TX DELBA Complete Event */
#define WMITLV_TABLE_WMI_TX_DELBA_COMPLETE_EVENTID(id,op,buf,len)                                           \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_tx_delba_complete_event_fixed_param, wmi_tx_delba_complete_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_TX_DELBA_COMPLETE_EVENTID);

/* Tx ADDBA Complete Event */
#define WMITLV_TABLE_WMI_TX_ADDBA_COMPLETE_EVENTID(id,op,buf,len)                                       \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_tx_addba_complete_event_fixed_param, wmi_tx_addba_complete_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_TX_ADDBA_COMPLETE_EVENTID);

/* ADD BA Req ssn Event */
#define WMITLV_TABLE_WMI_BA_RSP_SSN_EVENTID(id,op,buf,len)                                       \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ba_rsp_ssn_event_fixed_param, wmi_ba_rsp_ssn_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id, op, buf, len, WMITLV_TAG_ARRAY_STRUC, wmi_ba_event_ssn, ba_event_ssn_list, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_BA_RSP_SSN_EVENTID);

/* Aggregation Request event */
#define WMITLV_TABLE_WMI_AGGR_STATE_TRIG_EVENTID(id,op,buf,len)                                       \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_aggr_state_trig_event_fixed_param, wmi_aggr_state_trig_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_AGGR_STATE_TRIG_EVENTID);

/* Roam Event */
#define WMITLV_TABLE_WMI_ROAM_EVENTID(id,op,buf,len)                                                         \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_roam_event_fixed_param, wmi_roam_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_EVENTID);

/* Roam Synch Event */
#define WMITLV_TABLE_WMI_ROAM_SYNCH_EVENTID(id,op,buf,len)                                                      \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_roam_synch_event_fixed_param, wmi_roam_synch_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, bcn_probe_rsp_frame, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, reassoc_rsp_frame, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_channel, wmi_channel, chan, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_key_material, key, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, status, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, reassoc_req_frame, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_ROAM_SYNCH_EVENTID);

/* WOW Wakeup Host Event */
/* NOTE: Make sure wow_bitmap_info can be zero or one elements only */
#define WMITLV_TABLE_WMI_WOW_WAKEUP_HOST_EVENTID(id,op,buf,len)                                                         \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WOW_EVENT_INFO_fixed_param, WOW_EVENT_INFO_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, WOW_EVENT_INFO_SECTION_BITMAP, wow_bitmap_info, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, wow_packet_buffer, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_hb_ind_event_fixed_param, hb_indevt, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, WMI_GTK_OFFLOAD_STATUS_EVENT_fixed_param, wow_gtkigtk, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_WOW_WAKEUP_HOST_EVENTID);

#define WMITLV_TABLE_WMI_WOW_INITIAL_WAKEUP_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WOW_EVENT_INITIAL_WAKEUP_fixed_param, WOW_INITIAL_WAKEUP_EVENT_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_WOW_INITIAL_WAKEUP_EVENTID);

/* RTT error report Event */
#define WMITLV_TABLE_WMI_RTT_ERROR_REPORT_EVENTID(id,op,buf,len)    \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_RTT_ERROR_REPORT_EVENTID);

/* Echo Event */
#define WMITLV_TABLE_WMI_ECHO_EVENTID(id,op,buf,len)                                                         \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_echo_event_fixed_param, wmi_echo_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_ECHO_EVENTID);

/* FTM Integration Event */
#define WMITLV_TABLE_WMI_PDEV_FTM_INTG_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ftm_intg_event_fixed_param, wmi_ftm_intg_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_FTM_INTG_EVENTID);

/* VDEV get Keepalive Event */
#define WMITLV_TABLE_WMI_VDEV_GET_KEEPALIVE_EVENTID(id,op,buf,len)                                                         \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_get_keepalive_event_fixed_param, wmi_vdev_get_keepalive_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_GET_KEEPALIVE_EVENTID);

/* GPIO Input Event */
#define WMITLV_TABLE_WMI_GPIO_INPUT_EVENTID(id,op,buf,len)  \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_gpio_input_event_fixed_param, wmi_gpio_input_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_GPIO_INPUT_EVENTID);

/* CSA Handling Event */
#define WMITLV_TABLE_WMI_CSA_HANDLING_EVENTID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_csa_event_fixed_param, wmi_csa_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_CSA_HANDLING_EVENTID);

/* Rfkill state change Event */
#define WMITLV_TABLE_WMI_RFKILL_STATE_CHANGE_EVENTID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_rfkill_event_fixed_param, wmi_rfkill_mode_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_RFKILL_STATE_CHANGE_EVENTID);

/* Debug Message Event */
#define WMITLV_TABLE_WMI_DEBUG_MESG_EVENTID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, bufp, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_DEBUG_MESG_EVENTID);

#define WMITLV_TABLE_WMI_DEBUG_MESG_FLUSH_COMPLETE_EVENTID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_debug_mesg_flush_complete_fixed_param, wmi_debug_mesg_flush_complete_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_DEBUG_MESG_FLUSH_COMPLETE_EVENTID);

#define WMITLV_TABLE_WMI_RSSI_BREACH_EVENTID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_rssi_breach_event_fixed_param, wmi_rssi_breach_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_RSSI_BREACH_EVENTID);

#define WMITLV_TABLE_WMI_TRANSFER_DATA_TO_FLASH_COMPLETE_EVENTID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_transfer_data_to_flash_complete_event_fixed_param, wmi_transfer_data_to_flash_complete_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_TRANSFER_DATA_TO_FLASH_COMPLETE_EVENTID);

/* Diagnostics Event */
#define WMITLV_TABLE_WMI_DIAG_EVENTID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, bufp, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_DIAG_EVENTID);

/* IGTK Offload Event */
#define WMITLV_TABLE_WMI_GTK_OFFLOAD_STATUS_EVENTID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_WMI_GTK_OFFLOAD_STATUS_EVENT_fixed_param, WMI_GTK_OFFLOAD_STATUS_EVENT_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_GTK_OFFLOAD_STATUS_EVENTID);

/* DCA interferance Event */
#define WMITLV_TABLE_WMI_DCS_INTERFERENCE_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_dcs_interference_event_fixed_param, wmi_dcs_interference_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, ath_dcs_cw_int, cw_int, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wlan_dcs_im_tgt_stats_t, wlan_stat, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_DCS_INTERFERENCE_EVENTID);

/* Profile data Event */
#define WMITLV_TABLE_WMI_WLAN_PROFILE_DATA_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_wlan_profile_ctx_t, wmi_wlan_profile_ctx_t, profile_ctx, WMITLV_SIZE_FIX)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_wlan_profile_t, profile_data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_WLAN_PROFILE_DATA_EVENTID);

/* PDEV UTF Event */
#define WMITLV_TABLE_WMI_PDEV_UTF_EVENTID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_UTF_EVENTID);

/* Update SCPC calibrated data Event */
#define WMITLV_TABLE_WMI_PDEV_UTF_SCPC_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_scpc_event_fixed_param, wmi_scpc_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_UTF_SCPC_EVENTID);

/* Debug print Event */
#define WMITLV_TABLE_WMI_DEBUG_PRINT_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_DEBUG_PRINT_EVENTID);

/* RTT measurement report Event - DEPRECATED */
#define WMITLV_TABLE_WMI_RTT_MEASUREMENT_REPORT_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_RTT_MEASUREMENT_REPORT_EVENTID);

/*oem measurement report Event - DEPRECATED */
#define WMITLV_TABLE_WMI_OEM_MEASUREMENT_REPORT_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_OEM_MEASUREMENT_REPORT_EVENTID);

/*oem error report event - DEPRECATED */
#define WMITLV_TABLE_WMI_OEM_ERROR_REPORT_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_OEM_ERROR_REPORT_EVENTID);

/*oem capability report event - DEPRECATED */
#define WMITLV_TABLE_WMI_OEM_CAPABILITY_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_OEM_CAPABILITY_EVENTID);

/*oem response event*/
#define WMITLV_TABLE_WMI_OEM_RESPONSE_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_OEM_RESPONSE_EVENTID);

/* HOST SWBA Event */
#define WMITLV_TABLE_WMI_HOST_SWBA_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_host_swba_event_fixed_param, wmi_host_swba_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_tim_info, tim_info, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_p2p_noa_info, p2p_noa_info, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_HOST_SWBA_EVENTID);


/* Update stats Event */
#define WMITLV_TABLE_WMI_UPDATE_STATS_EVENTID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_stats_event_fixed_param, wmi_stats_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_UPDATE_STATS_EVENTID);

/* For vdev based ht/vht info upload*/
#define WMITLV_TABLE_WMI_UPDATE_VDEV_RATE_STATS_EVENTID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_rate_stats_event_fixed_param, wmi_vdev_rate_stats_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_vdev_rate_ht_info, ht_info, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_UPDATE_VDEV_RATE_STATS_EVENTID);


/* Update memory dump complete Event */
#define  WMITLV_TABLE_WMI_UPDATE_FW_MEM_DUMP_EVENTID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_update_fw_mem_dump_fixed_param, wmi_update_fw_mem_dump_fixed_param, fixed_param, WMITLV_SIZE_FIX)

WMITLV_CREATE_PARAM_STRUC(WMI_UPDATE_FW_MEM_DUMP_EVENTID);

/* Event indicating the DIAG LOGs/Events supported by FW */
#define WMITLV_TABLE_WMI_DIAG_EVENT_LOG_SUPPORTED_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_diag_event_log_supported_event_fixed_params, wmi_diag_event_log_supported_event_fixed_params, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_UINT32, A_UINT32, diag_events_logs_list, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_DIAG_EVENT_LOG_SUPPORTED_EVENTID);

/* Update iface link stats Event */
#define WMITLV_TABLE_WMI_IFACE_LINK_STATS_EVENTID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_iface_link_stats_event_fixed_param, wmi_iface_link_stats_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_iface_link_stats, iface_link_stats, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_wmm_ac_stats, ac, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_IFACE_LINK_STATS_EVENTID);

/* Update Peer link stats Event */
#define WMITLV_TABLE_WMI_PEER_LINK_STATS_EVENTID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_stats_event_fixed_param, wmi_peer_stats_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_peer_link_stats, peer_stats, WMITLV_SIZE_VAR) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_rate_stats, peer_rate_stats, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PEER_LINK_STATS_EVENTID);

/* Update radio stats Event */
#define WMITLV_TABLE_WMI_RADIO_LINK_STATS_EVENTID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_radio_link_stats_event_fixed_param, wmi_radio_link_stats_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_radio_link_stats, radio_stats, WMITLV_SIZE_VAR)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_channel_stats, channel_stats, WMITLV_SIZE_VAR)

WMITLV_CREATE_PARAM_STRUC(WMI_RADIO_LINK_STATS_EVENTID);

/* PDEV QVIT Event */
#define WMITLV_TABLE_WMI_PDEV_QVIT_EVENTID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_QVIT_EVENTID);

/* WLAN Frequency avoid Event */
#define WMITLV_TABLE_WMI_WLAN_FREQ_AVOID_EVENTID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_avoid_freq_ranges_event_fixed_param, wmi_avoid_freq_ranges_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_avoid_freq_range_desc, avd_freq_range, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_WLAN_FREQ_AVOID_EVENTID);

/* GTK rekey fail Event */
#define WMITLV_TABLE_WMI_GTK_REKEY_FAIL_EVENTID(id,op,buf,len)                                                                                                 \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_gtk_rekey_fail_event_fixed_param, wmi_gtk_rekey_fail_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_GTK_REKEY_FAIL_EVENTID);

/* NLO match event */
#define WMITLV_TABLE_WMI_NLO_MATCH_EVENTID(id,op,buf,len)                                                                                                 \
WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_nlo_event, wmi_nlo_event, fixed_param, WMITLV_SIZE_FIX)
    WMITLV_CREATE_PARAM_STRUC(WMI_NLO_MATCH_EVENTID);

/* NLO scan complete event */
#define WMITLV_TABLE_WMI_NLO_SCAN_COMPLETE_EVENTID(id,op,buf,len)                                                                                                 \
WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_nlo_event, wmi_nlo_event, fixed_param, WMITLV_SIZE_FIX)
    WMITLV_CREATE_PARAM_STRUC(WMI_NLO_SCAN_COMPLETE_EVENTID);

/* APFIND event */
#define WMITLV_TABLE_WMI_APFIND_EVENTID(id,op,buf,len)                                                                                                 \
WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_apfind_event_hdr, wmi_apfind_event_hdr, hdr, WMITLV_SIZE_FIX) \
WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
    WMITLV_CREATE_PARAM_STRUC(WMI_APFIND_EVENTID);

/* WMI_PASSPOINT_MATCH_EVENTID */
#define WMITLV_TABLE_WMI_PASSPOINT_MATCH_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_passpoint_event_hdr, wmi_passpoint_event_hdr, fixed_param, WMITLV_SIZE_FIX)   \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, bufp, WMITLV_SIZE_VAR)
    WMITLV_CREATE_PARAM_STRUC(WMI_PASSPOINT_MATCH_EVENTID);

/* Chatter query reply event */
#define WMITLV_TABLE_WMI_CHATTER_PC_QUERY_EVENTID(id,op,buf,len) \
WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_chatter_query_reply_event_fixed_param, wmi_chatter_query_reply_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
    WMITLV_CREATE_PARAM_STRUC(WMI_CHATTER_PC_QUERY_EVENTID);

/* Upload H_CV info event */
#define WMITLV_TABLE_WMI_UPLOADH_EVENTID(id,op,buf,len) \
WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_upload_h_hdr, wmi_upload_h_hdr, hdr, WMITLV_SIZE_FIX)   \
WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, bufp, WMITLV_SIZE_VAR)
    WMITLV_CREATE_PARAM_STRUC(WMI_UPLOADH_EVENTID);

/* Capture H info event */
#define WMITLV_TABLE_WMI_CAPTUREH_EVENTID(id,op,buf,len) \
WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_capture_h_event_hdr, wmi_capture_h_event_hdr, fixed_param, WMITLV_SIZE_FIX)
    WMITLV_CREATE_PARAM_STRUC(WMI_CAPTUREH_EVENTID);

/* TDLS Peer Update event */
#define WMITLV_TABLE_WMI_TDLS_PEER_EVENTID(id,op,buf,len) \
WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_tdls_peer_event_fixed_param, wmi_tdls_peer_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
    WMITLV_CREATE_PARAM_STRUC(WMI_TDLS_PEER_EVENTID);

/* VDEV MCC Beacon Interval Change Request Event */
#define WMITLV_TABLE_WMI_VDEV_MCC_BCN_INTERVAL_CHANGE_REQ_EVENTID(id,op,buf,len)                                                         \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_mcc_bcn_intvl_change_event_fixed_param, wmi_vdev_mcc_bcn_intvl_change_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_MCC_BCN_INTERVAL_CHANGE_REQ_EVENTID);

#define WMITLV_TABLE_WMI_BATCH_SCAN_ENABLED_EVENTID(id,op,buf,len)                                                         \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_batch_scan_enabled_event_fixed_param, wmi_batch_scan_enabled_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_BATCH_SCAN_ENABLED_EVENTID);

#define WMITLV_TABLE_WMI_BATCH_SCAN_RESULT_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_batch_scan_result_event_fixed_param, wmi_batch_scan_result_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len,WMITLV_TAG_ARRAY_STRUC, wmi_batch_scan_result_scan_list, scan_list, WMITLV_SIZE_VAR)    \
    WMITLV_ELEM(id,op,buf,len,WMITLV_TAG_ARRAY_STRUC, wmi_batch_scan_result_network_info, network_list, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_BATCH_SCAN_RESULT_EVENTID);

#define WMITLV_TABLE_WMI_OFFLOAD_BCN_TX_STATUS_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_offload_bcn_tx_status_event_fixed_param, wmi_offload_bcn_tx_status_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_OFFLOAD_BCN_TX_STATUS_EVENTID);

/* NOA Event */
#define WMITLV_TABLE_WMI_P2P_NOA_EVENTID(id,op,buf,len) \
WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_p2p_noa_event_fixed_param, wmi_p2p_noa_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_p2p_noa_info, wmi_p2p_noa_info, p2p_noa_info, WMITLV_SIZE_FIX)
    WMITLV_CREATE_PARAM_STRUC(WMI_P2P_NOA_EVENTID);

    /* AP PS enhanced green ap Event */
#define WMITLV_TABLE_WMI_AP_PS_EGAP_INFO_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id, op, buf, len,\
        WMITLV_TAG_STRUC_wmi_ap_ps_egap_info_event_fixed_param,\
        wmi_ap_ps_egap_info_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id, op, buf, len, WMITLV_TAG_ARRAY_STRUC,\
        wmi_ap_ps_egap_info_chainmask_list, chainmask_list, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_AP_PS_EGAP_INFO_EVENTID);

#define WMITLV_TABLE_WMI_PEER_INFO_EVENTID(id,op,buf,len)                                                                                                 \
WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_info_event_fixed_param, wmi_peer_info_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)               \
WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_peer_info, peer_info, WMITLV_SIZE_VAR)
    WMITLV_CREATE_PARAM_STRUC(WMI_PEER_INFO_EVENTID);

#define WMITLV_TABLE_WMI_PEER_TX_FAIL_CNT_THR_EVENTID(id,op,buf,len)                                                                                      \
WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_tx_fail_cnt_thr_event_fixed_param, wmi_peer_tx_fail_cnt_thr_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
    WMITLV_CREATE_PARAM_STRUC(WMI_PEER_TX_FAIL_CNT_THR_EVENTID);

/* DFS radar Event */
#define WMITLV_TABLE_WMI_DFS_RADAR_EVENTID(id,op,buf,len) \
WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_dfs_radar_event_fixed_param, wmi_dfs_radar_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
    WMITLV_CREATE_PARAM_STRUC(WMI_DFS_RADAR_EVENTID);

/* Thermal Event */
#define WMITLV_TABLE_WMI_THERMAL_MGMT_EVENTID(id,op,buf,len) \
WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_thermal_mgmt_event_fixed_param, wmi_thermal_mgmt_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
    WMITLV_CREATE_PARAM_STRUC(WMI_THERMAL_MGMT_EVENTID);

/* NAN Response/Indication Event */
#define WMITLV_TABLE_WMI_NAN_EVENTID(id,op,buf,len)                                     \
WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_nan_event_hdr, wmi_nan_event_hdr, fixed_param, WMITLV_SIZE_FIX) \
WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
    WMITLV_CREATE_PARAM_STRUC(WMI_NAN_EVENTID);

/* L1SS track Event */
#define WMITLV_TABLE_WMI_PDEV_L1SS_TRACK_EVENTID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_l1ss_track_event_fixed_param, wmi_pdev_l1ss_track_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_L1SS_TRACK_EVENTID);

#define WMITLV_TABLE_WMI_DIAG_DATA_CONTAINER_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_diag_data_container_event_fixed_param, wmi_diag_data_container_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)   \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, bufp, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_DIAG_DATA_CONTAINER_EVENTID);

/* Estimated Link Speed Indication*/
#define WMITLV_TABLE_WMI_PEER_ESTIMATED_LINKSPEED_EVENTID(id,op,buf,len)\
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_estimated_linkspeed_event_fixed_param, wmi_peer_estimated_linkspeed_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PEER_ESTIMATED_LINKSPEED_EVENTID);

#define WMITLV_TABLE_WMI_STATS_EXT_EVENTID(id,op,buf,len)                                     \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_stats_ext_event_fixed_param, wmi_stats_ext_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, data, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_STATS_EXT_EVENTID);

#define WMITLV_TABLE_WMI_OFFLOAD_PROB_RESP_TX_STATUS_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_offload_prb_rsp_tx_status_event_fixed_param, wmi_offload_prb_rsp_tx_status_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_OFFLOAD_PROB_RESP_TX_STATUS_EVENTID);

/* host auto shut down event */
#define WMITLV_TABLE_WMI_HOST_AUTO_SHUTDOWN_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_host_auto_shutdown_event_fixed_param, wmi_host_auto_shutdown_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_HOST_AUTO_SHUTDOWN_EVENTID);

/* peer state Event */
#define WMITLV_TABLE_WMI_PEER_STATE_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_state_event_fixed_param, wmi_peer_state_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PEER_STATE_EVENTID);

/* peer delete response Event */
#define WMITLV_TABLE_WMI_PEER_DELETE_RESP_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_delete_resp_event_fixed_param, wmi_peer_delete_resp_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PEER_DELETE_RESP_EVENTID);

/* peer assoc conf Event */
#define WMITLV_TABLE_WMI_PEER_ASSOC_CONF_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_peer_assoc_conf_event_fixed_param, wmi_peer_assoc_conf_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PEER_ASSOC_CONF_EVENTID);

/* D0-WOW Disable Ack event */
#define WMITLV_TABLE_WMI_D0_WOW_DISABLE_ACK_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_d0_wow_disable_ack_event_fixed_param, wmi_d0_wow_disable_ack_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_D0_WOW_DISABLE_ACK_EVENTID);

/* Pdev get chip temperature event */
#define WMITLV_TABLE_WMI_PDEV_TEMPERATURE_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_temperature_event_fixed_param, wmi_pdev_temperature_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_TEMPERATURE_EVENTID);

/* mDNS offload stats event */
#define WMITLV_TABLE_WMI_MDNS_STATS_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_mdns_stats_event_fixed_param, wmi_mdns_stats_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_MDNS_STATS_EVENTID);

/* pdev resume event */
#define WMITLV_TABLE_WMI_PDEV_RESUME_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_pdev_resume_event_fixed_param, wmi_pdev_resume_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PDEV_RESUME_EVENTID);

/* SAP Authentication offload event */
#define WMITLV_TABLE_WMI_SAP_OFL_ADD_STA_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_sap_ofl_add_sta_event_fixed_param, wmi_sap_ofl_add_sta_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)   \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, bufp, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_SAP_OFL_ADD_STA_EVENTID);

#define WMITLV_TABLE_WMI_SAP_OFL_DEL_STA_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_sap_ofl_del_sta_event_fixed_param, wmi_sap_ofl_del_sta_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_SAP_OFL_DEL_STA_EVENTID);

/* Set OCB schedule cmd, DEPRECATED */
#define WMITLV_TABLE_WMI_OCB_SET_SCHED_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id, op, buf, len, WMITLV_TAG_STRUC_wmi_ocb_set_sched_event_fixed_param, wmi_ocb_set_sched_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_OCB_SET_SCHED_EVENTID);

/* Set OCB configuration response event */
#define WMITLV_TABLE_WMI_OCB_SET_CONFIG_RESP_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ocb_set_config_resp_event_fixed_param, wmi_ocb_set_config_resp_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_OCB_SET_CONFIG_RESP_EVENTID);

/* Get TSF timer response event */
#define WMITLV_TABLE_WMI_OCB_GET_TSF_TIMER_RESP_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_ocb_get_tsf_timer_resp_event_fixed_param, wmi_ocb_get_tsf_timer_resp_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_OCB_GET_TSF_TIMER_RESP_EVENTID);

/* Get DCC stats response event */
#define WMITLV_TABLE_WMI_DCC_GET_STATS_RESP_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_dcc_get_stats_resp_event_fixed_param, wmi_dcc_get_stats_resp_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_dcc_ndl_stats_per_channel, stats_per_channel_list, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_DCC_GET_STATS_RESP_EVENTID);

/* Update DCC NDL response event */
#define WMITLV_TABLE_WMI_DCC_UPDATE_NDL_RESP_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_dcc_update_ndl_resp_event_fixed_param, wmi_dcc_update_ndl_resp_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_DCC_UPDATE_NDL_RESP_EVENTID);

/* DCC stats event */
#define WMITLV_TABLE_WMI_DCC_STATS_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_dcc_stats_event_fixed_param, wmi_dcc_stats_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_dcc_ndl_stats_per_channel, stats_per_channel_list, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_DCC_STATS_EVENTID);

/* Read TSF timer response event */
#define WMITLV_TABLE_WMI_VDEV_TSF_REPORT_EVENTID(id,op,buf,len) \
WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_tsf_report_event_fixed_param, wmi_vdev_tsf_report_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_TSF_REPORT_EVENTID);

/* Vdev capabilities IE to be transmitted in mgmt frames */
#define WMITLV_TABLE_WMI_VDEV_SET_IE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_vdev_set_ie_cmd_fixed_param, wmi_vdev_set_ie_cmd_fixed_param, vdev_ie, WMITLV_SIZE_FIX) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_BYTE, A_UINT8, bufp, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_VDEV_SET_IE_CMDID);

/* SOC Set Hardware Mode Response event */
#define WMITLV_TABLE_WMI_SOC_SET_HW_MODE_RESP_EVENTID(id,op,buf,len) \
WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_soc_set_hw_mode_response_event_fixed_param, wmi_soc_set_hw_mode_response_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_soc_set_hw_mode_response_vdev_mac_entry, wmi_soc_set_hw_mode_response_vdev_mac_mapping, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_SOC_SET_HW_MODE_RESP_EVENTID);

/* SOC Hardware Mode Transition event */
#define WMITLV_TABLE_WMI_SOC_HW_MODE_TRANSITION_EVENTID(id,op,buf,len) \
WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_soc_hw_mode_transition_event_fixed_param, wmi_soc_hw_mode_transition_event_fixed_param, fixed_param, WMITLV_SIZE_FIX) \
WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_ARRAY_STRUC, wmi_soc_set_hw_mode_response_vdev_mac_entry, wmi_soc_set_hw_mode_response_vdev_mac_mapping, WMITLV_SIZE_VAR)
WMITLV_CREATE_PARAM_STRUC(WMI_SOC_HW_MODE_TRANSITION_EVENTID);

/* SOC Set Dual MAC Config Response event */
#define WMITLV_TABLE_WMI_SOC_SET_DUAL_MAC_CONFIG_RESP_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_soc_set_dual_mac_config_response_event_fixed_param, wmi_soc_set_dual_mac_config_response_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_SOC_SET_DUAL_MAC_CONFIG_RESP_EVENTID);

/* Packet Filter configure command*/
#define WMITLV_TABLE_WMI_PACKET_FILTER_CONFIG_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_packet_filter_config_fixed_param, WMI_PACKET_FILTER_CONFIG_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PACKET_FILTER_CONFIG_CMDID);

/* Packet Filter enable command*/
#define WMITLV_TABLE_WMI_PACKET_FILTER_ENABLE_CMDID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_packet_filter_enable_fixed_param, WMI_PACKET_FILTER_ENABLE_CMD_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_PACKET_FILTER_ENABLE_CMDID);

/* MAWC enable/disable sensor event */
#define WMITLV_TABLE_WMI_MAWC_ENABLE_SENSOR_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_mawc_enable_sensor_event_fixed_param, wmi_mawc_enable_sensor_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_MAWC_ENABLE_SENSOR_EVENTID);

/* SMPS force mode complete Event */
#define WMITLV_TABLE_WMI_STA_SMPS_FORCE_MODE_COMPLETE_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_sta_smps_force_mode_complete_event_fixed_param, wmi_sta_smps_force_mode_complete_event_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_STA_SMPS_FORCE_MODE_COMPLETE_EVENTID);

/* bpf offload capability info event */
#define WMITLV_TABLE_WMI_BPF_CAPABILIY_INFO_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_bpf_capability_info_evt_fixed_param,  wmi_bpf_capability_info_evt_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_BPF_CAPABILIY_INFO_EVENTID);

/* bpf offload vdev status info event */
#define WMITLV_TABLE_WMI_BPF_VDEV_STATS_INFO_EVENTID(id,op,buf,len) \
    WMITLV_ELEM(id,op,buf,len, WMITLV_TAG_STRUC_wmi_bpf_vdev_stats_info_evt_fixed_param, wmi_bpf_vdev_stats_info_evt_fixed_param, fixed_param, WMITLV_SIZE_FIX)
WMITLV_CREATE_PARAM_STRUC(WMI_BPF_VDEV_STATS_INFO_EVENTID);

#ifdef __cplusplus
}
#endif

#endif /*_WMI_TLV_DEFS_H_*/
