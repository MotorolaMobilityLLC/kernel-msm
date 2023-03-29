# We can build either as part of a standalone Kernel build or as
# an external module.  Determine which mechanism is being used
ifeq ($(MODNAME),)
	KERNEL_BUILD := y
else
	KERNEL_BUILD := n
endif

ifeq ($(KERNEL_BUILD), y)
	# These are provided in external module based builds
	# Need to explicitly define for Kernel-based builds
	MODNAME := wlan
	WLAN_ROOT := drivers/staging/qcacld-3.0
	WLAN_COMMON_ROOT := ../qca-wifi-host-cmn
	WLAN_COMMON_INC := $(WLAN_ROOT)/$(WLAN_COMMON_ROOT)
	WLAN_FW_API := $(WLAN_ROOT)/../fw-api/
	WLAN_PROFILE := default
endif

WLAN_COMMON_ROOT ?= ../qca-wifi-host-cmn
WLAN_COMMON_INC ?= $(WLAN_ROOT)/$(WLAN_COMMON_ROOT)
WLAN_FW_API ?= $(WLAN_ROOT)/../fw-api/
WLAN_PROFILE ?= default
CONFIG_QCA_CLD_WLAN_PROFILE ?= $(WLAN_PROFILE)
DEVNAME ?= wlan

ifeq ($(KERNEL_BUILD), n)
ifneq ($(ANDROID_BUILD_TOP),)
      ANDROID_BUILD_TOP_REL := $(shell python -c "import os.path; print(os.path.relpath('$(ANDROID_BUILD_TOP)'))")
      override WLAN_ROOT := $(ANDROID_BUILD_TOP_REL)/$(WLAN_ROOT)
      override WLAN_COMMON_INC := $(ANDROID_BUILD_TOP_REL)/$(WLAN_COMMON_INC)
      override WLAN_FW_API := $(ANDROID_BUILD_TOP_REL)/$(WLAN_FW_API)
endif
endif

include $(WLAN_ROOT)/configs/$(CONFIG_QCA_CLD_WLAN_PROFILE)_defconfig

# add configurations in WLAN_CFG_OVERRIDE
ifneq ($(WLAN_CFG_OVERRIDE),)
WLAN_CFG_OVERRIDE_FILE := $(WLAN_ROOT)/.wlan_cfg_override
$(shell echo > $(WLAN_CFG_OVERRIDE_FILE))

$(foreach cfg, $(WLAN_CFG_OVERRIDE), \
	$(shell echo $(cfg) >> $(WLAN_CFG_OVERRIDE_FILE)))

include $(WLAN_CFG_OVERRIDE_FILE)
$(warning "Overriding WLAN config with: $(shell cat $(WLAN_CFG_OVERRIDE_FILE))")
endif

OBJS :=
OBJS_DIRS :=

define add-wlan-objs
$(eval $(_add-wlan-objs))
endef

define _add-wlan-objs
  ifneq ($(2),)
    ifeq ($(KERNEL_SUPPORTS_NESTED_COMPOSITES),y)
      OBJS_DIRS += $(dir $(2))
      OBJS += $(1).o
      $(1)-y := $(2)
    else
      OBJS += $(2)
    endif
  endif
endef

############ UAPI ############
UAPI_DIR :=	uapi
UAPI_INC :=	-I$(WLAN_ROOT)/$(UAPI_DIR)/linux

############ COMMON ############
COMMON_DIR :=	core/common
COMMON_INC :=	-I$(WLAN_ROOT)/$(COMMON_DIR)

############ HDD ############
HDD_DIR :=	core/hdd
HDD_INC_DIR :=	$(HDD_DIR)/inc
HDD_SRC_DIR :=	$(HDD_DIR)/src

HDD_INC := 	-I$(WLAN_ROOT)/$(HDD_INC_DIR) \
		-I$(WLAN_ROOT)/$(HDD_SRC_DIR)

HDD_OBJS := 	$(HDD_SRC_DIR)/wlan_hdd_assoc.o \
		$(HDD_SRC_DIR)/wlan_hdd_cfg.o \
		$(HDD_SRC_DIR)/wlan_hdd_cfg80211.o \
		$(HDD_SRC_DIR)/wlan_hdd_data_stall_detection.o \
		$(HDD_SRC_DIR)/wlan_hdd_driver_ops.o \
		$(HDD_SRC_DIR)/wlan_hdd_ftm.o \
		$(HDD_SRC_DIR)/wlan_hdd_hostapd.o \
		$(HDD_SRC_DIR)/wlan_hdd_ioctl.o \
		$(HDD_SRC_DIR)/wlan_hdd_main.o \
		$(HDD_SRC_DIR)/wlan_hdd_object_manager.o \
		$(HDD_SRC_DIR)/wlan_hdd_oemdata.o \
		$(HDD_SRC_DIR)/wlan_hdd_p2p.o \
		$(HDD_SRC_DIR)/wlan_hdd_power.o \
		$(HDD_SRC_DIR)/wlan_hdd_regulatory.o \
		$(HDD_SRC_DIR)/wlan_hdd_scan.o \
		$(HDD_SRC_DIR)/wlan_hdd_softap_tx_rx.o \
		$(HDD_SRC_DIR)/wlan_hdd_sta_info.o \
		$(HDD_SRC_DIR)/wlan_hdd_stats.o \
		$(HDD_SRC_DIR)/wlan_hdd_trace.o \
		$(HDD_SRC_DIR)/wlan_hdd_tx_rx.o \
		$(HDD_SRC_DIR)/wlan_hdd_wmm.o \
		$(HDD_SRC_DIR)/wlan_hdd_wowl.o

ifeq ($(CONFIG_WLAN_FEATURE_PERIODIC_STA_STATS), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_periodic_sta_stats.o
endif

ifeq ($(CONFIG_UNIT_TEST), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_unit_test.o
endif

ifeq ($(CONFIG_WLAN_WEXT_SUPPORT_ENABLE), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_wext.o \
	    $(HDD_SRC_DIR)/wlan_hdd_hostapd_wext.o
endif

ifeq ($(CONFIG_DCS), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_dcs.o
endif

ifeq ($(CONFIG_FEATURE_WLAN_EXTSCAN), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_ext_scan.o
endif

ifeq ($(CONFIG_WLAN_DEBUGFS), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_debugfs.o
ifeq ($(CONFIG_WLAN_FEATURE_LINK_LAYER_STATS), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_debugfs_llstat.o
endif
ifeq ($(CONFIG_WLAN_FEATURE_MIB_STATS), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_debugfs_mibstat.o
endif
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_debugfs_csr.o
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_debugfs_offload.o
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_debugfs_roam.o
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_debugfs_config.o
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_debugfs_unit_test.o
ifeq ($(CONFIG_WLAN_MWS_INFO_DEBUGFS), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_debugfs_coex.o
endif
endif

ifeq ($(CONFIG_WLAN_CONV_SPECTRAL_ENABLE),y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_spectralscan.o
endif

ifeq ($(CONFIG_WLAN_FEATURE_DSRC), y)
HDD_OBJS+=	$(HDD_SRC_DIR)/wlan_hdd_ocb.o
endif

ifeq ($(CONFIG_FEATURE_MEMDUMP_ENABLE), y)
HDD_OBJS+=	$(HDD_SRC_DIR)/wlan_hdd_memdump.o
endif

ifeq ($(CONFIG_WLAN_FEATURE_FIPS), y)
HDD_OBJS+=	$(HDD_SRC_DIR)/wlan_hdd_fips.o
endif

ifeq ($(CONFIG_QCACLD_FEATURE_GREEN_AP), y)
HDD_OBJS+=	$(HDD_SRC_DIR)/wlan_hdd_green_ap.o
endif

ifeq ($(CONFIG_QCACLD_FEATURE_APF), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_apf.o
endif

ifeq ($(CONFIG_WLAN_FEATURE_LPSS), y)
HDD_OBJS +=	$(HDD_SRC_DIR)/wlan_hdd_lpass.o
endif

ifeq ($(CONFIG_WLAN_LRO), y)
HDD_OBJS +=     $(HDD_SRC_DIR)/wlan_hdd_lro.o
endif

ifeq ($(CONFIG_WLAN_NAPI), y)
HDD_OBJS +=     $(HDD_SRC_DIR)/wlan_hdd_napi.o
endif

ifeq ($(CONFIG_IPA_OFFLOAD), y)
HDD_OBJS +=	$(HDD_SRC_DIR)/wlan_hdd_ipa.o
endif

ifeq ($(CONFIG_QCACLD_FEATURE_NAN), y)
HDD_OBJS +=	$(HDD_SRC_DIR)/wlan_hdd_nan.o
HDD_OBJS +=	$(HDD_SRC_DIR)/wlan_hdd_nan_datapath.o
endif

ifeq ($(CONFIG_QCOM_TDLS), y)
HDD_OBJS +=	$(HDD_SRC_DIR)/wlan_hdd_tdls.o
endif

ifeq ($(CONFIG_WLAN_SYNC_TSF_PLUS), y)
CONFIG_WLAN_SYNC_TSF := y
endif

ifeq ($(CONFIG_WLAN_SYNC_TSF), y)
HDD_OBJS +=	$(HDD_SRC_DIR)/wlan_hdd_tsf.o
endif

ifeq ($(CONFIG_MPC_UT_FRAMEWORK), y)
HDD_OBJS +=	$(HDD_SRC_DIR)/wlan_hdd_conc_ut.o
endif

ifeq ($(CONFIG_WLAN_FEATURE_DISA), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_disa.o
endif

ifeq ($(CONFIG_LFR_SUBNET_DETECTION), y)
HDD_OBJS +=	$(HDD_SRC_DIR)/wlan_hdd_subnet_detect.o
endif

ifeq ($(CONFIG_WLAN_FEATURE_11AX), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_he.o
endif

ifeq ($(CONFIG_WLAN_FEATURE_TWT), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_twt.o
endif

ifeq ($(CONFIG_LITHIUM), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_rx_monitor.o
endif

ifeq ($(CONFIG_LITHIUM), y)
CONFIG_WLAN_FEATURE_DP_RX_THREADS := y
CONFIG_WLAN_FEATURE_RX_SOFTIRQ_TIME_LIMIT := y
endif

ifeq ($(CONFIG_WLAN_NUD_TRACKING), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_nud_tracking.o
endif

ifeq ($(CONFIG_WLAN_FEATURE_PACKET_FILTERING), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_packet_filter.o
endif

ifeq ($(CONFIG_FEATURE_RSSI_MONITOR), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_rssi_monitor.o
endif

ifeq ($(CONFIG_FEATURE_BSS_TRANSITION), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_bss_transition.o
endif

ifeq ($(CONFIG_FEATURE_STATION_INFO), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_station_info.o
endif

ifeq ($(CONFIG_FEATURE_TX_POWER), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_tx_power.o
endif

ifeq ($(CONFIG_FEATURE_OTA_TEST), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_ota_test.o
endif

ifeq ($(CONFIG_FEATURE_ACTIVE_TOS), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_active_tos.o
endif

ifeq ($(CONFIG_FEATURE_SAR_LIMITS), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sar_limits.o
endif

ifeq ($(CONFIG_FEATURE_CONCURRENCY_MATRIX), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_concurrency_matrix.o
endif

ifeq ($(CONFIG_FEATURE_SAP_COND_CHAN_SWITCH), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sap_cond_chan_switch.o
endif

ifeq ($(CONFIG_FEATURE_P2P_LISTEN_OFFLOAD), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_p2p_listen_offload.o
endif

ifeq ($(CONFIG_WLAN_SYSFS), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sysfs.o
ifeq ($(CONFIG_WLAN_SYSFS_CHANNEL), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sysfs_channel.o
endif
ifeq ($(CONFIG_WLAN_SYSFS_FW_MODE_CFG), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sysfs_fw_mode_config.o
endif
ifeq ($(CONFIG_WLAN_REASSOC), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sysfs_reassoc.o
endif
ifeq ($(CONFIG_WLAN_SYSFS_STA_INFO), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sysfs_sta_info.o
endif
ifeq ($(CONFIG_WLAN_DEBUG_CRASH_INJECT), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sysfs_crash_inject.o
endif
ifeq ($(CONFIG_FEATURE_UNIT_TEST_SUSPEND), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sysfs_suspend_resume.o
endif
ifeq ($(CONFIG_WLAN_SYSFS_MEM_STATS), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sysfs_mem_stats.o
endif
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sysfs_unit_test.o
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sysfs_modify_acl.o
ifeq ($(CONFIG_WLAN_SYSFS_CONNECT_INFO), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sysfs_connect_info.o
endif
ifeq ($(CONFIG_WLAN_SCAN_DISABLE), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sysfs_scan_disable.o
endif
ifeq ($(CONFIG_WLAN_SYSFS_DCM), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sysfs_dcm.o
endif
ifeq ($(CONFIG_WLAN_WOW_ITO), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sysfs_wow_ito.o
endif
ifeq ($(CONFIG_WLAN_WOWL_ADD_PTRN), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sysfs_wowl_add_ptrn.o
endif
ifeq ($(CONFIG_WLAN_WOWL_DEL_PTRN), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sysfs_wowl_del_ptrn.o
endif
ifeq ($(CONFIG_WLAN_SYSFS_TX_STBC), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sysfs_tx_stbc.o
endif
ifeq ($(CONFIG_WLAN_SYSFS_SCAN_CFG), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sysfs_scan_config.o
endif
ifeq ($(CONFIG_WLAN_SYSFS_MONITOR_MODE_CHANNEL), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sysfs_monitor_mode_channel.o
endif
ifeq ($(CONFIG_WLAN_SYSFS_RANGE_EXT), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sysfs_range_ext.o
endif
ifeq ($(CONFIG_WLAN_SYSFS_RADAR), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sysfs_radar.o
endif
ifeq ($(CONFIG_WLAN_SYSFS_RTS_CTS), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sysfs_rts_cts.o
endif
ifeq ($(CONFIG_WLAN_SYSFS_HE_BSS_COLOR), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sysfs_he_bss_color.o
endif
ifeq ($(CONFIG_WLAN_TXRX_FW_STATS), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sysfs_txrx_fw_stats.o
endif
ifeq ($(CONFIG_WLAN_TXRX_STATS), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sysfs_txrx_stats.o
endif
ifeq ($(CONFIG_WLAN_SYSFS_DP_TRACE), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sysfs_dp_trace.o
endif
ifeq ($(CONFIG_WLAN_SYSFS_STATS), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sysfs_stats.o
endif
ifeq ($(CONFIG_WLAN_SYSFS_TDLS_PEERS), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sysfs_tdls_peers.o
endif
ifeq ($(CONFIG_WLAN_SYSFS_TEMPERATURE), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sysfs_temperature.o
endif
ifeq ($(CONFIG_WLAN_THERMAL_CFG), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sysfs_thermal_cfg.o
endif
ifeq ($(CONFIG_FEATURE_MOTION_DETECTION), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sysfs_motion_detection.o
endif
ifeq ($(CONFIG_WLAN_SYSFS_WLAN_DBG), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sysfs_wlan_dbg.o
endif
ifeq ($(CONFIG_WLAN_TXRX_FW_ST_RST), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sysfs_txrx_fw_st_rst.o
endif
ifeq ($(CONFIG_WLAN_GTX_BW_MASK), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sysfs_gtx_bw_mask.o
endif
ifeq ($(CONFIG_IPA_OFFLOAD), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sysfs_ipa.o
endif
ifeq ($(CONFIG_REMOVE_PKT_LOG), n)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sysfs_pktlog.o
endif
ifeq ($(CONFIG_WLAN_DL_MODES), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sysfs_dl_modes.o
endif
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sysfs_policy_mgr.o
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sysfs_dp_aggregation.o
ifeq ($(CONFIG_DP_SWLM), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sysfs_swlm.o
endif
ifeq ($(CONFIG_WLAN_DUMP_IN_PROGRESS), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_sysfs_dump_in_progress.o
endif
endif

ifeq ($(CONFIG_QCACLD_FEATURE_FW_STATE), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_fw_state.o
endif

ifeq ($(CONFIG_QCACLD_FEATURE_COEX_CONFIG), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_coex_config.o
endif

ifeq ($(CONFIG_QCACLD_FEATURE_MPTA_HELPER), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_mpta_helper.o
endif

ifeq ($(CONFIG_WLAN_BCN_RECV_FEATURE), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_bcn_recv.o
endif

ifeq ($(CONFIG_QCACLD_FEATURE_HW_CAPABILITY), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_hw_capability.o
endif

ifeq ($(CONFIG_FW_THERMAL_THROTTLE), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_thermal.o
endif

ifeq ($(CONFIG_QCACLD_FEATURE_BTC_CHAIN_MODE), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_btc_chain_mode.o
endif

ifeq ($(CONFIG_FEATURE_WLAN_TIME_SYNC_FTM), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_ftm_time_sync.o
endif

ifeq ($(CONFIG_WLAN_HANG_EVENT), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_hang_event.o
endif

ifeq ($(CONFIG_WLAN_CFR_ENABLE), y)
HDD_OBJS += $(HDD_SRC_DIR)/wlan_hdd_cfr.o
endif

$(call add-wlan-objs,hdd,$(HDD_OBJS))

###### OSIF_SYNC ########
SYNC_DIR := os_if/sync
SYNC_INC_DIR := $(SYNC_DIR)/inc
SYNC_SRC_DIR := $(SYNC_DIR)/src

SYNC_INC := \
	-I$(WLAN_ROOT)/$(SYNC_INC_DIR) \
	-I$(WLAN_ROOT)/$(SYNC_SRC_DIR) \

SYNC_OBJS := \
	$(SYNC_SRC_DIR)/osif_sync.o \
	$(SYNC_SRC_DIR)/osif_driver_sync.o \
	$(SYNC_SRC_DIR)/osif_psoc_sync.o \
	$(SYNC_SRC_DIR)/osif_vdev_sync.o \

$(call add-wlan-objs,sync,$(SYNC_OBJS))

########### Driver Synchronization Core (DSC) ###########
DSC_DIR := components/dsc
DSC_INC_DIR := $(DSC_DIR)/inc
DSC_SRC_DIR := $(DSC_DIR)/src
DSC_TEST_DIR := $(DSC_DIR)/test

DSC_INC := \
	-I$(WLAN_ROOT)/$(DSC_INC_DIR) \
	-I$(WLAN_ROOT)/$(DSC_SRC_DIR) \
	-I$(WLAN_ROOT)/$(DSC_TEST_DIR)

DSC_OBJS := \
	$(DSC_SRC_DIR)/__wlan_dsc.o \
	$(DSC_SRC_DIR)/wlan_dsc_driver.o \
	$(DSC_SRC_DIR)/wlan_dsc_psoc.o \
	$(DSC_SRC_DIR)/wlan_dsc_vdev.o

ifeq ($(CONFIG_DSC_TEST), y)
	DSC_OBJS += $(DSC_TEST_DIR)/wlan_dsc_test.o
endif

$(call add-wlan-objs,dsc,$(DSC_OBJS))

cppflags-$(CONFIG_DSC_DEBUG) += -DWLAN_DSC_DEBUG
cppflags-$(CONFIG_DSC_TEST) += -DWLAN_DSC_TEST

########### HOST DIAG LOG ###########
HOST_DIAG_LOG_DIR :=	$(WLAN_COMMON_ROOT)/utils/host_diag_log

HOST_DIAG_LOG_INC_DIR :=	$(HOST_DIAG_LOG_DIR)/inc
HOST_DIAG_LOG_SRC_DIR :=	$(HOST_DIAG_LOG_DIR)/src

HOST_DIAG_LOG_INC :=	-I$(WLAN_ROOT)/$(HOST_DIAG_LOG_INC_DIR) \
			-I$(WLAN_ROOT)/$(HOST_DIAG_LOG_SRC_DIR)

ifeq ($(BUILD_DIAG_VERSION), y)
HOST_DIAG_LOG_OBJS +=	$(HOST_DIAG_LOG_SRC_DIR)/host_diag_log.o
endif

$(call add-wlan-objs,host_diag_log,$(HOST_DIAG_LOG_OBJS))

############ EPPING ############
EPPING_DIR :=	$(WLAN_COMMON_ROOT)/utils/epping
EPPING_INC_DIR :=	$(EPPING_DIR)/inc
EPPING_SRC_DIR :=	$(EPPING_DIR)/src

EPPING_INC := 	-I$(WLAN_ROOT)/$(EPPING_INC_DIR)

ifeq ($(CONFIG_FEATURE_EPPING), y)
EPPING_OBJS := $(EPPING_SRC_DIR)/epping_main.o \
		$(EPPING_SRC_DIR)/epping_txrx.o \
		$(EPPING_SRC_DIR)/epping_tx.o \
		$(EPPING_SRC_DIR)/epping_rx.o \
		$(EPPING_SRC_DIR)/epping_helper.o
endif

$(call add-wlan-objs,epping,$(EPPING_OBJS))

############ SYS ############
CMN_SYS_DIR :=	$(WLAN_COMMON_ROOT)/utils/sys
CMN_SYS_INC_DIR := 	$(CMN_SYS_DIR)
CMN_SYS_INC :=	-I$(WLAN_ROOT)/$(CMN_SYS_INC_DIR)

############ MAC ############
MAC_DIR :=	core/mac
MAC_INC_DIR :=	$(MAC_DIR)/inc
MAC_SRC_DIR :=	$(MAC_DIR)/src

MAC_INC := 	-I$(WLAN_ROOT)/$(MAC_INC_DIR) \
		-I$(WLAN_ROOT)/$(MAC_SRC_DIR)/dph \
		-I$(WLAN_ROOT)/$(MAC_SRC_DIR)/include \
		-I$(WLAN_ROOT)/$(MAC_SRC_DIR)/pe/include \
		-I$(WLAN_ROOT)/$(MAC_SRC_DIR)/pe/lim \
		-I$(WLAN_ROOT)/$(MAC_SRC_DIR)/pe/nan

MAC_DPH_OBJS :=	$(MAC_SRC_DIR)/dph/dph_hash_table.o

MAC_LIM_OBJS := $(MAC_SRC_DIR)/pe/lim/lim_aid_mgmt.o \
		$(MAC_SRC_DIR)/pe/lim/lim_admit_control.o \
		$(MAC_SRC_DIR)/pe/lim/lim_api.o \
		$(MAC_SRC_DIR)/pe/lim/lim_assoc_utils.o \
		$(MAC_SRC_DIR)/pe/lim/lim_ft.o \
		$(MAC_SRC_DIR)/pe/lim/lim_link_monitoring_algo.o \
		$(MAC_SRC_DIR)/pe/lim/lim_process_action_frame.o \
		$(MAC_SRC_DIR)/pe/lim/lim_process_assoc_req_frame.o \
		$(MAC_SRC_DIR)/pe/lim/lim_process_assoc_rsp_frame.o \
		$(MAC_SRC_DIR)/pe/lim/lim_process_auth_frame.o \
		$(MAC_SRC_DIR)/pe/lim/lim_process_beacon_frame.o \
		$(MAC_SRC_DIR)/pe/lim/lim_process_cfg_updates.o \
		$(MAC_SRC_DIR)/pe/lim/lim_process_deauth_frame.o \
		$(MAC_SRC_DIR)/pe/lim/lim_process_disassoc_frame.o \
		$(MAC_SRC_DIR)/pe/lim/lim_process_message_queue.o \
		$(MAC_SRC_DIR)/pe/lim/lim_process_mlm_req_messages.o \
		$(MAC_SRC_DIR)/pe/lim/lim_process_mlm_rsp_messages.o \
		$(MAC_SRC_DIR)/pe/lim/lim_process_probe_req_frame.o \
		$(MAC_SRC_DIR)/pe/lim/lim_process_probe_rsp_frame.o \
		$(MAC_SRC_DIR)/pe/lim/lim_process_sme_req_messages.o \
		$(MAC_SRC_DIR)/pe/lim/lim_prop_exts_utils.o \
		$(MAC_SRC_DIR)/pe/lim/lim_scan_result_utils.o \
		$(MAC_SRC_DIR)/pe/lim/lim_security_utils.o \
		$(MAC_SRC_DIR)/pe/lim/lim_send_management_frames.o \
		$(MAC_SRC_DIR)/pe/lim/lim_send_messages.o \
		$(MAC_SRC_DIR)/pe/lim/lim_send_sme_rsp_messages.o \
		$(MAC_SRC_DIR)/pe/lim/lim_session.o \
		$(MAC_SRC_DIR)/pe/lim/lim_session_utils.o \
		$(MAC_SRC_DIR)/pe/lim/lim_sme_req_utils.o \
		$(MAC_SRC_DIR)/pe/lim/lim_timer_utils.o \
		$(MAC_SRC_DIR)/pe/lim/lim_trace.o \
		$(MAC_SRC_DIR)/pe/lim/lim_utils.o

ifeq ($(CONFIG_QCOM_TDLS), y)
MAC_LIM_OBJS += $(MAC_SRC_DIR)/pe/lim/lim_process_tdls.o
endif

ifeq ($(CONFIG_WLAN_FEATURE_FILS), y)
MAC_LIM_OBJS += $(MAC_SRC_DIR)/pe/lim/lim_process_fils.o
endif

ifeq ($(CONFIG_QCACLD_FEATURE_NAN), y)
MAC_NDP_OBJS += $(MAC_SRC_DIR)/pe/nan/nan_datapath.o
endif

ifeq ($(CONFIG_QCACLD_WLAN_LFR2), y)
	MAC_LIM_OBJS += $(MAC_SRC_DIR)/pe/lim/lim_process_mlm_host_roam.o \
		$(MAC_SRC_DIR)/pe/lim/lim_send_frames_host_roam.o \
		$(MAC_SRC_DIR)/pe/lim/lim_roam_timer_utils.o \
		$(MAC_SRC_DIR)/pe/lim/lim_ft_preauth.o \
		$(MAC_SRC_DIR)/pe/lim/lim_reassoc_utils.o
endif

MAC_SCH_OBJS := $(MAC_SRC_DIR)/pe/sch/sch_api.o \
		$(MAC_SRC_DIR)/pe/sch/sch_beacon_gen.o \
		$(MAC_SRC_DIR)/pe/sch/sch_beacon_process.o \
		$(MAC_SRC_DIR)/pe/sch/sch_message.o

MAC_RRM_OBJS :=	$(MAC_SRC_DIR)/pe/rrm/rrm_api.o

MAC_OBJS := 	$(MAC_CFG_OBJS) \
		$(MAC_DPH_OBJS) \
		$(MAC_LIM_OBJS) \
		$(MAC_SCH_OBJS) \
		$(MAC_RRM_OBJS) \
		$(MAC_NDP_OBJS)

$(call add-wlan-objs,mac,$(MAC_OBJS))

############ SAP ############
SAP_DIR :=	core/sap
SAP_INC_DIR :=	$(SAP_DIR)/inc
SAP_SRC_DIR :=	$(SAP_DIR)/src

SAP_INC := 	-I$(WLAN_ROOT)/$(SAP_INC_DIR) \
		-I$(WLAN_ROOT)/$(SAP_SRC_DIR)

SAP_OBJS :=	$(SAP_SRC_DIR)/sap_api_link_cntl.o \
		$(SAP_SRC_DIR)/sap_ch_select.o \
		$(SAP_SRC_DIR)/sap_fsm.o \
		$(SAP_SRC_DIR)/sap_module.o

$(call add-wlan-objs,sap,$(SAP_OBJS))

############ CFG ############
CFG_REL_DIR := $(WLAN_COMMON_ROOT)/cfg
CFG_DIR := $(WLAN_ROOT)/$(CFG_REL_DIR)
CFG_INC := \
	-I$(CFG_DIR)/inc \
	-I$(CFG_DIR)/dispatcher/inc \
	-I$(WLAN_ROOT)/components/cfg
CFG_OBJS := \
	$(CFG_REL_DIR)/src/cfg.o

$(call add-wlan-objs,cfg,$(CFG_OBJS))

############ DFS ############
DFS_DIR :=     $(WLAN_COMMON_ROOT)/umac/dfs
DFS_CORE_INC_DIR := $(DFS_DIR)/core/inc
DFS_CORE_SRC_DIR := $(DFS_DIR)/core/src

DFS_DISP_INC_DIR := $(DFS_DIR)/dispatcher/inc
DFS_DISP_SRC_DIR := $(DFS_DIR)/dispatcher/src
DFS_TARGET_INC_DIR := $(WLAN_COMMON_ROOT)/target_if/dfs/inc
DFS_CMN_SERVICES_INC_DIR := $(WLAN_COMMON_ROOT)/umac/cmn_services/dfs/inc

DFS_INC :=	-I$(WLAN_ROOT)/$(DFS_DISP_INC_DIR) \
		-I$(WLAN_ROOT)/$(DFS_TARGET_INC_DIR) \
		-I$(WLAN_ROOT)/$(DFS_CMN_SERVICES_INC_DIR)

ifeq ($(CONFIG_WLAN_DFS_MASTER_ENABLE), y)

DFS_OBJS :=	$(DFS_CORE_SRC_DIR)/misc/dfs.o \
		$(DFS_CORE_SRC_DIR)/misc/dfs_cac.o \
		$(DFS_CORE_SRC_DIR)/misc/dfs_nol.o \
		$(DFS_CORE_SRC_DIR)/misc/dfs_random_chan_sel.o \
		$(DFS_CORE_SRC_DIR)/misc/dfs_process_radar_found_ind.o \
		$(DFS_DISP_SRC_DIR)/wlan_dfs_init_deinit_api.o \
		$(DFS_DISP_SRC_DIR)/wlan_dfs_lmac_api.o \
		$(DFS_DISP_SRC_DIR)/wlan_dfs_mlme_api.o \
		$(DFS_DISP_SRC_DIR)/wlan_dfs_tgt_api.o \
		$(DFS_DISP_SRC_DIR)/wlan_dfs_ucfg_api.o \
		$(DFS_DISP_SRC_DIR)/wlan_dfs_tgt_api.o \
		$(DFS_DISP_SRC_DIR)/wlan_dfs_utils_api.o \
		$(WLAN_COMMON_ROOT)/target_if/dfs/src/target_if_dfs.o

ifeq ($(CONFIG_WLAN_FEATURE_DFS_OFFLOAD), y)
DFS_OBJS +=	$(WLAN_COMMON_ROOT)/target_if/dfs/src/target_if_dfs_full_offload.o \
		$(DFS_CORE_SRC_DIR)/misc/dfs_full_offload.o
else
DFS_OBJS +=	$(WLAN_COMMON_ROOT)/target_if/dfs/src/target_if_dfs_partial_offload.o \
		$(DFS_CORE_SRC_DIR)/filtering/dfs_fcc_bin5.o \
		$(DFS_CORE_SRC_DIR)/filtering/dfs_bindetects.o \
		$(DFS_CORE_SRC_DIR)/filtering/dfs_debug.o \
		$(DFS_CORE_SRC_DIR)/filtering/dfs_init.o \
		$(DFS_CORE_SRC_DIR)/filtering/dfs_misc.o \
		$(DFS_CORE_SRC_DIR)/filtering/dfs_phyerr_tlv.o \
		$(DFS_CORE_SRC_DIR)/filtering/dfs_process_phyerr.o \
		$(DFS_CORE_SRC_DIR)/filtering/dfs_process_radarevent.o \
		$(DFS_CORE_SRC_DIR)/filtering/dfs_staggered.o \
		$(DFS_CORE_SRC_DIR)/filtering/dfs_radar.o \
		$(DFS_CORE_SRC_DIR)/filtering/dfs_partial_offload_radar.o \
		$(DFS_CORE_SRC_DIR)/misc/dfs_filter_init.o
endif
endif

$(call add-wlan-objs,dfs,$(DFS_OBJS))

############ SME ############
SME_DIR :=	core/sme
SME_INC_DIR :=	$(SME_DIR)/inc
SME_SRC_DIR :=	$(SME_DIR)/src

SME_INC := 	-I$(WLAN_ROOT)/$(SME_INC_DIR) \
		-I$(WLAN_ROOT)/$(SME_SRC_DIR)/csr

SME_CSR_OBJS := $(SME_SRC_DIR)/csr/csr_api_roam.o \
		$(SME_SRC_DIR)/csr/csr_api_scan.o \
		$(SME_SRC_DIR)/csr/csr_cmd_process.o \
		$(SME_SRC_DIR)/csr/csr_link_list.o \
		$(SME_SRC_DIR)/csr/csr_neighbor_roam.o \
		$(SME_SRC_DIR)/csr/csr_util.o \


ifeq ($(CONFIG_QCACLD_WLAN_LFR2), y)
SME_CSR_OBJS += $(SME_SRC_DIR)/csr/csr_roam_preauth.o \
		$(SME_SRC_DIR)/csr/csr_host_scan_roam.o
endif

SME_QOS_OBJS := $(SME_SRC_DIR)/qos/sme_qos.o

SME_CMN_OBJS := $(SME_SRC_DIR)/common/sme_api.o \
		$(SME_SRC_DIR)/common/sme_ft_api.o \
		$(SME_SRC_DIR)/common/sme_power_save.o \
		$(SME_SRC_DIR)/common/sme_trace.o

SME_RRM_OBJS := $(SME_SRC_DIR)/rrm/sme_rrm.o

ifeq ($(CONFIG_QCACLD_FEATURE_NAN), y)
SME_NDP_OBJS += $(SME_SRC_DIR)/nan/nan_datapath_api.o
endif

SME_OBJS :=	$(SME_CMN_OBJS) \
		$(SME_CSR_OBJS) \
		$(SME_QOS_OBJS) \
		$(SME_RRM_OBJS) \
		$(SME_NAN_OBJS) \
		$(SME_NDP_OBJS)

$(call add-wlan-objs,sme,$(SME_OBJS))

############ NLINK ############
NLINK_DIR     :=	$(WLAN_COMMON_ROOT)/utils/nlink
NLINK_INC_DIR :=	$(NLINK_DIR)/inc
NLINK_SRC_DIR :=	$(NLINK_DIR)/src

NLINK_INC     := 	-I$(WLAN_ROOT)/$(NLINK_INC_DIR)
NLINK_OBJS    :=	$(NLINK_SRC_DIR)/wlan_nlink_srv.o

$(call add-wlan-objs,nlink,$(NLINK_OBJS))

############ PTT ############
PTT_DIR     :=	$(WLAN_COMMON_ROOT)/utils/ptt
PTT_INC_DIR :=	$(PTT_DIR)/inc
PTT_SRC_DIR :=	$(PTT_DIR)/src

PTT_INC     := 	-I$(WLAN_ROOT)/$(PTT_INC_DIR)
PTT_OBJS    :=	$(PTT_SRC_DIR)/wlan_ptt_sock_svc.o

$(call add-wlan-objs,ptt,$(PTT_OBJS))

############ WLAN_LOGGING ############
WLAN_LOGGING_DIR     :=	$(WLAN_COMMON_ROOT)/utils/logging
WLAN_LOGGING_INC_DIR :=	$(WLAN_LOGGING_DIR)/inc
WLAN_LOGGING_SRC_DIR :=	$(WLAN_LOGGING_DIR)/src

WLAN_LOGGING_INC     := -I$(WLAN_ROOT)/$(WLAN_LOGGING_INC_DIR)
WLAN_LOGGING_OBJS    := $(WLAN_LOGGING_SRC_DIR)/wlan_logging_sock_svc.o \
		$(WLAN_LOGGING_SRC_DIR)/wlan_roam_debug.o

$(call add-wlan-objs,wlan_logging,$(WLAN_LOGGING_OBJS))

############ SYS ############
SYS_DIR :=	core/mac/src/sys

SYS_INC := 	-I$(WLAN_ROOT)/$(SYS_DIR)/common/inc \
		-I$(WLAN_ROOT)/$(SYS_DIR)/legacy/src/platform/inc \
		-I$(WLAN_ROOT)/$(SYS_DIR)/legacy/src/system/inc \
		-I$(WLAN_ROOT)/$(SYS_DIR)/legacy/src/utils/inc

SYS_COMMON_SRC_DIR := $(SYS_DIR)/common/src
SYS_LEGACY_SRC_DIR := $(SYS_DIR)/legacy/src
SYS_OBJS :=	$(SYS_COMMON_SRC_DIR)/wlan_qct_sys.o \
		$(SYS_LEGACY_SRC_DIR)/platform/src/sys_wrapper.o \
		$(SYS_LEGACY_SRC_DIR)/system/src/mac_init_api.o \
		$(SYS_LEGACY_SRC_DIR)/system/src/sys_entry_func.o \
		$(SYS_LEGACY_SRC_DIR)/utils/src/dot11f.o \
		$(SYS_LEGACY_SRC_DIR)/utils/src/mac_trace.o \
		$(SYS_LEGACY_SRC_DIR)/utils/src/parser_api.o \
		$(SYS_LEGACY_SRC_DIR)/utils/src/utils_parser.o

$(call add-wlan-objs,sys,$(SYS_OBJS))

############ Qcacld WMI ###################
WMI_DIR := components/wmi

CLD_WMI_INC  :=	-I$(WLAN_ROOT)/$(WMI_DIR)/inc

ifeq ($(CONFIG_WMI_ROAM_SUPPORT), y)
CLD_WMI_ROAM_OBJS +=	$(WMI_DIR)/src/wmi_unified_roam_tlv.o \
			$(WMI_DIR)/src/wmi_unified_roam_api.o
endif

ifeq ($(CONFIG_CP_STATS), y)
CLD_WMI_MC_CP_STATS_OBJS :=	$(WMI_DIR)/src/wmi_unified_mc_cp_stats_tlv.o \
				$(WMI_DIR)/src/wmi_unified_mc_cp_stats_api.o
endif

CLD_WMI_OBJS :=	$(CLD_WMI_ROAM_OBJS) \
		$(CLD_WMI_MC_CP_STATS_OBJS)

$(call add-wlan-objs,cld_wmi,$(CLD_WMI_OBJS))

############ Qca-wifi-host-cmn ############
QDF_OS_DIR :=	qdf
QDF_OS_INC_DIR := $(QDF_OS_DIR)/inc
QDF_OS_SRC_DIR := $(QDF_OS_DIR)/src
QDF_OS_LINUX_SRC_DIR := $(QDF_OS_DIR)/linux/src
QDF_OBJ_DIR := $(WLAN_COMMON_ROOT)/$(QDF_OS_SRC_DIR)
QDF_LINUX_OBJ_DIR := $(WLAN_COMMON_ROOT)/$(QDF_OS_LINUX_SRC_DIR)
QDF_TEST_DIR := $(QDF_OS_DIR)/test
QDF_TEST_OBJ_DIR := $(WLAN_COMMON_ROOT)/$(QDF_TEST_DIR)

QDF_INC := \
	-I$(WLAN_COMMON_INC)/$(QDF_OS_INC_DIR) \
	-I$(WLAN_COMMON_INC)/$(QDF_OS_LINUX_SRC_DIR) \
	-I$(WLAN_COMMON_INC)/$(QDF_TEST_DIR)

QDF_OBJS := \
	$(QDF_LINUX_OBJ_DIR)/qdf_crypto.o \
	$(QDF_LINUX_OBJ_DIR)/qdf_defer.o \
	$(QDF_LINUX_OBJ_DIR)/qdf_delayed_work.o \
	$(QDF_LINUX_OBJ_DIR)/qdf_event.o \
	$(QDF_LINUX_OBJ_DIR)/qdf_file.o \
	$(QDF_LINUX_OBJ_DIR)/qdf_func_tracker.o \
	$(QDF_LINUX_OBJ_DIR)/qdf_idr.o \
	$(QDF_LINUX_OBJ_DIR)/qdf_list.o \
	$(QDF_LINUX_OBJ_DIR)/qdf_lock.o \
	$(QDF_LINUX_OBJ_DIR)/qdf_mc_timer.o \
	$(QDF_LINUX_OBJ_DIR)/qdf_mem.o \
	$(QDF_LINUX_OBJ_DIR)/qdf_nbuf.o \
	$(QDF_LINUX_OBJ_DIR)/qdf_periodic_work.o \
	$(QDF_LINUX_OBJ_DIR)/qdf_status.o \
	$(QDF_LINUX_OBJ_DIR)/qdf_threads.o \
	$(QDF_LINUX_OBJ_DIR)/qdf_trace.o \
	$(QDF_LINUX_OBJ_DIR)/qdf_nbuf_frag.o \
	$(QDF_OBJ_DIR)/qdf_flex_mem.o \
	$(QDF_OBJ_DIR)/qdf_parse.o \
	$(QDF_OBJ_DIR)/qdf_platform.o \
	$(QDF_OBJ_DIR)/qdf_str.o \
	$(QDF_OBJ_DIR)/qdf_talloc.o \
	$(QDF_OBJ_DIR)/qdf_types.o \

ifeq ($(CONFIG_WLAN_DEBUGFS), y)
QDF_OBJS += $(QDF_LINUX_OBJ_DIR)/qdf_debugfs.o
endif

ifeq ($(CONFIG_WLAN_STREAMFS), y)
QDF_OBJS += $(QDF_LINUX_OBJ_DIR)/qdf_streamfs.o
endif

ifeq ($(CONFIG_IPA_OFFLOAD), y)
QDF_OBJS += $(QDF_LINUX_OBJ_DIR)/qdf_ipa.o
endif

# enable CPU hotplug support if SMP is enabled
ifeq ($(CONFIG_SMP), y)
	QDF_OBJS += $(QDF_OBJ_DIR)/qdf_cpuhp.o
	QDF_OBJS += $(QDF_LINUX_OBJ_DIR)/qdf_cpuhp.o
endif

ifeq ($(CONFIG_LEAK_DETECTION), y)
	QDF_OBJS += $(QDF_OBJ_DIR)/qdf_debug_domain.o
	QDF_OBJS += $(QDF_OBJ_DIR)/qdf_tracker.o
endif

ifeq ($(CONFIG_WLAN_HANG_EVENT), y)
	QDF_OBJS += $(QDF_OBJ_DIR)/qdf_notifier.o
endif

ifeq ($(CONFIG_QDF_TEST), y)
	QDF_OBJS += $(QDF_TEST_OBJ_DIR)/qdf_delayed_work_test.o
	QDF_OBJS += $(QDF_TEST_OBJ_DIR)/qdf_hashtable_test.o
	QDF_OBJS += $(QDF_TEST_OBJ_DIR)/qdf_periodic_work_test.o
	QDF_OBJS += $(QDF_TEST_OBJ_DIR)/qdf_ptr_hash_test.o
	QDF_OBJS += $(QDF_TEST_OBJ_DIR)/qdf_slist_test.o
	QDF_OBJS += $(QDF_TEST_OBJ_DIR)/qdf_talloc_test.o
	QDF_OBJS += $(QDF_TEST_OBJ_DIR)/qdf_tracker_test.o
	QDF_OBJS += $(QDF_TEST_OBJ_DIR)/qdf_types_test.o
endif

ifeq ($(CONFIG_WLAN_HANG_EVENT), y)
	QDF_OBJS += $(QDF_OBJ_DIR)/qdf_hang_event_notifier.o
endif

ifeq ($(CONFIG_WLAN_LRO), y)
QDF_OBJS +=     $(QDF_LINUX_OBJ_DIR)/qdf_lro.o
endif

$(call add-wlan-objs,qdf,$(QDF_OBJS))

cppflags-$(CONFIG_TALLOC_DEBUG) += -DWLAN_TALLOC_DEBUG
cppflags-$(CONFIG_QDF_TEST) += -DWLAN_DELAYED_WORK_TEST
cppflags-$(CONFIG_QDF_TEST) += -DWLAN_HASHTABLE_TEST
cppflags-$(CONFIG_QDF_TEST) += -DWLAN_PERIODIC_WORK_TEST
cppflags-$(CONFIG_QDF_TEST) += -DWLAN_PTR_HASH_TEST
cppflags-$(CONFIG_QDF_TEST) += -DWLAN_SLIST_TEST
cppflags-$(CONFIG_QDF_TEST) += -DWLAN_TALLOC_TEST
cppflags-$(CONFIG_QDF_TEST) += -DWLAN_TRACKER_TEST
cppflags-$(CONFIG_QDF_TEST) += -DWLAN_TYPES_TEST
cppflags-$(CONFIG_WLAN_HANG_EVENT) += -DWLAN_HANG_EVENT

############ WBUFF ############
WBUFF_OS_DIR :=	wbuff
WBUFF_OS_INC_DIR := $(WBUFF_OS_DIR)/inc
WBUFF_OS_SRC_DIR := $(WBUFF_OS_DIR)/src
WBUFF_OBJ_DIR := $(WLAN_COMMON_ROOT)/$(WBUFF_OS_SRC_DIR)

WBUFF_INC :=	-I$(WLAN_COMMON_INC)/$(WBUFF_OS_INC_DIR) \

ifeq ($(CONFIG_WLAN_WBUFF), y)
WBUFF_OBJS += 	$(WBUFF_OBJ_DIR)/wbuff.o
endif

$(call add-wlan-objs,wbuff,$(WBUFF_OBJS))

##########QAL #######
QAL_OS_DIR :=	qal
QAL_OS_INC_DIR := $(QAL_OS_DIR)/inc
QAL_OS_LINUX_SRC_DIR := $(QAL_OS_DIR)/linux/src

QAL_INC :=	-I$(WLAN_COMMON_INC)/$(QAL_OS_INC_DIR) \
		-I$(WLAN_COMMON_INC)/$(QAL_OS_LINUX_SRC_DIR)


##########OS_IF #######
OS_IF_DIR := $(WLAN_COMMON_ROOT)/os_if

OS_IF_INC += -I$(WLAN_COMMON_INC)/os_if/linux \
            -I$(WLAN_COMMON_INC)/os_if/linux/scan/inc \
            -I$(WLAN_COMMON_INC)/os_if/linux/spectral/inc \
            -I$(WLAN_COMMON_INC)/os_if/linux/crypto/inc \
            -I$(WLAN_COMMON_INC)/os_if/linux/mlme/inc

OS_IF_OBJ += $(OS_IF_DIR)/linux/wlan_osif_request_manager.o \
	     $(OS_IF_DIR)/linux/crypto/src/wlan_nl_to_crypto_params.o

CONFIG_CRYPTO_COMPONENT := y

ifeq ($(CONFIG_CRYPTO_COMPONENT), y)
OS_IF_OBJ += $(OS_IF_DIR)/linux/crypto/src/wlan_cfg80211_crypto.o
endif

$(call add-wlan-objs,os_if,$(OS_IF_OBJ))

############ UMAC_DISP ############
UMAC_DISP_DIR := umac/global_umac_dispatcher/lmac_if
UMAC_DISP_INC_DIR := $(UMAC_DISP_DIR)/inc
UMAC_DISP_SRC_DIR := $(UMAC_DISP_DIR)/src
UMAC_DISP_OBJ_DIR := $(WLAN_COMMON_ROOT)/$(UMAC_DISP_SRC_DIR)

UMAC_DISP_INC := -I$(WLAN_COMMON_INC)/$(UMAC_DISP_INC_DIR)

UMAC_DISP_OBJS := $(UMAC_DISP_OBJ_DIR)/wlan_lmac_if.o

$(call add-wlan-objs,umac_disp,$(UMAC_DISP_OBJS))

############# UMAC_SCAN ############
UMAC_SCAN_DIR := umac/scan
UMAC_SCAN_DISP_INC_DIR := $(UMAC_SCAN_DIR)/dispatcher/inc
UMAC_SCAN_CORE_DIR := $(WLAN_COMMON_ROOT)/$(UMAC_SCAN_DIR)/core/src
UMAC_SCAN_DISP_DIR := $(WLAN_COMMON_ROOT)/$(UMAC_SCAN_DIR)/dispatcher/src
UMAC_TARGET_SCAN_INC := -I$(WLAN_COMMON_INC)/target_if/scan/inc

UMAC_SCAN_INC := -I$(WLAN_COMMON_INC)/$(UMAC_SCAN_DISP_INC_DIR)
UMAC_SCAN_OBJS := $(UMAC_SCAN_CORE_DIR)/wlan_scan_cache_db.o \
		$(UMAC_SCAN_CORE_DIR)/wlan_scan_11d.o \
		$(UMAC_SCAN_CORE_DIR)/wlan_scan_filter.o \
		$(UMAC_SCAN_CORE_DIR)/wlan_scan_main.o \
		$(UMAC_SCAN_CORE_DIR)/wlan_scan_manager.o \
		$(UMAC_SCAN_DISP_DIR)/wlan_scan_tgt_api.o \
		$(UMAC_SCAN_DISP_DIR)/wlan_scan_ucfg_api.o \
		$(UMAC_SCAN_DISP_DIR)/wlan_scan_api.o \
		$(UMAC_SCAN_DISP_DIR)/wlan_scan_utils_api.o \
		$(WLAN_COMMON_ROOT)/os_if/linux/scan/src/wlan_cfg80211_scan.o \
		$(WLAN_COMMON_ROOT)/os_if/linux/wlan_cfg80211.o \
		$(WLAN_COMMON_ROOT)/target_if/scan/src/target_if_scan.o

ifeq ($(CONFIG_FEATURE_WLAN_EXTSCAN), y)
UMAC_SCAN_OBJS += $(UMAC_SCAN_DISP_DIR)/wlan_extscan_api.o
endif

ifeq ($(CONFIG_BAND_6GHZ), y)
UMAC_SCAN_OBJS += $(UMAC_SCAN_CORE_DIR)/wlan_scan_manager_6ghz.o
endif

$(call add-wlan-objs,umac_scan,$(UMAC_SCAN_OBJS))

############# UMAC_SPECTRAL_SCAN ############
UMAC_SPECTRAL_DIR := spectral
UMAC_SPECTRAL_DISP_INC_DIR := $(UMAC_SPECTRAL_DIR)/dispatcher/inc
UMAC_SPECTRAL_CORE_INC_DIR := $(UMAC_SPECTRAL_DIR)/core
UMAC_SPECTRAL_CORE_DIR := $(WLAN_COMMON_ROOT)/$(UMAC_SPECTRAL_DIR)/core
UMAC_SPECTRAL_DISP_DIR := $(WLAN_COMMON_ROOT)/$(UMAC_SPECTRAL_DIR)/dispatcher/src
UMAC_TARGET_SPECTRAL_INC := -I$(WLAN_COMMON_INC)/target_if/spectral

UMAC_SPECTRAL_INC := -I$(WLAN_COMMON_INC)/$(UMAC_SPECTRAL_DISP_INC_DIR) \
			-I$(WLAN_COMMON_INC)/$(UMAC_SPECTRAL_CORE_INC_DIR) \
			-I$(WLAN_COMMON_INC)/target_if/direct_buf_rx/inc
ifeq ($(CONFIG_WLAN_CONV_SPECTRAL_ENABLE),y)
UMAC_SPECTRAL_OBJS := $(UMAC_SPECTRAL_CORE_DIR)/spectral_offload.o \
		$(UMAC_SPECTRAL_CORE_DIR)/spectral_common.o \
		$(UMAC_SPECTRAL_DISP_DIR)/wlan_spectral_ucfg_api.o \
		$(UMAC_SPECTRAL_DISP_DIR)/wlan_spectral_utils_api.o \
		$(UMAC_SPECTRAL_DISP_DIR)/wlan_spectral_tgt_api.o \
		$(WLAN_COMMON_ROOT)/os_if/linux/spectral/src/wlan_cfg80211_spectral.o \
		$(WLAN_COMMON_ROOT)/os_if/linux/spectral/src/os_if_spectral_netlink.o \
		$(WLAN_COMMON_ROOT)/target_if/spectral/target_if_spectral_netlink.o \
		$(WLAN_COMMON_ROOT)/target_if/spectral/target_if_spectral_phyerr.o \
		$(WLAN_COMMON_ROOT)/target_if/spectral/target_if_spectral.o \
		$(WLAN_COMMON_ROOT)/target_if/spectral/target_if_spectral_sim.o
endif

$(call add-wlan-objs,umac_spectral,$(UMAC_SPECTRAL_OBJS))

############# WLAN_CFR ############
WLAN_CFR_DIR := umac/cfr
WLAN_CFR_DISP_INC_DIR := $(WLAN_CFR_DIR)/dispatcher/inc
WLAN_CFR_CORE_INC_DIR := $(WLAN_CFR_DIR)/core/inc
WLAN_CFR_CORE_DIR := $(WLAN_COMMON_ROOT)/$(WLAN_CFR_DIR)/core/src
WLAN_CFR_DISP_DIR := $(WLAN_COMMON_ROOT)/$(WLAN_CFR_DIR)/dispatcher/src
WLAN_CFR_TARGET_INC_DIR := target_if/cfr/inc

WLAN_CFR_INC := -I$(WLAN_COMMON_INC)/$(WLAN_CFR_DISP_INC_DIR) \
			-I$(WLAN_COMMON_INC)/$(WLAN_CFR_CORE_INC_DIR) \
			-I$(WLAN_COMMON_INC)/$(WLAN_CFR_TARGET_INC_DIR)
ifeq ($(CONFIG_WLAN_CFR_ENABLE),y)
WLAN_CFR_OBJS := $(WLAN_CFR_CORE_DIR)/cfr_common.o \
                $(WLAN_CFR_DISP_DIR)/wlan_cfr_tgt_api.o \
                $(WLAN_CFR_DISP_DIR)/wlan_cfr_ucfg_api.o \
                $(WLAN_CFR_DISP_DIR)/wlan_cfr_utils_api.o \
		$(WLAN_COMMON_ROOT)/target_if/cfr/src/target_if_cfr.o \
		$(WLAN_COMMON_ROOT)/target_if/cfr/src/target_if_cfr_enh.o \
		$(WLAN_COMMON_ROOT)/target_if/cfr/src/target_if_cfr_6490.o
endif

$(call add-wlan-objs,wlan_cfr,$(WLAN_CFR_OBJS))

############# UMAC_GREEN_AP ############
UMAC_GREEN_AP_DIR := umac/green_ap
UMAC_GREEN_AP_DISP_INC_DIR := $(UMAC_GREEN_AP_DIR)/dispatcher/inc
UMAC_GREEN_AP_CORE_DIR := $(WLAN_COMMON_ROOT)/$(UMAC_GREEN_AP_DIR)/core/src
UMAC_GREEN_AP_DISP_DIR := $(WLAN_COMMON_ROOT)/$(UMAC_GREEN_AP_DIR)/dispatcher/src
UMAC_TARGET_GREEN_AP_INC := -I$(WLAN_COMMON_INC)/target_if/green_ap/inc

UMAC_GREEN_AP_INC := -I$(WLAN_COMMON_INC)/$(UMAC_GREEN_AP_DISP_INC_DIR)

ifeq ($(CONFIG_QCACLD_FEATURE_GREEN_AP), y)
UMAC_GREEN_AP_OBJS := $(UMAC_GREEN_AP_CORE_DIR)/wlan_green_ap_main.o \
		$(UMAC_GREEN_AP_DISP_DIR)/wlan_green_ap_api.o \
                $(UMAC_GREEN_AP_DISP_DIR)/wlan_green_ap_ucfg_api.o \
                $(WLAN_COMMON_ROOT)/target_if/green_ap/src/target_if_green_ap.o
endif

$(call add-wlan-objs,umac_green_ap,$(UMAC_GREEN_AP_OBJS))

############# WLAN_CONV_CRYPTO_SUPPORTED ############
UMAC_CRYPTO_DIR := umac/cmn_services/crypto
UMAC_CRYPTO_CORE_DIR := $(WLAN_COMMON_ROOT)/$(UMAC_CRYPTO_DIR)/src
UMAC_CRYPTO_INC := -I$(WLAN_COMMON_INC)/$(UMAC_CRYPTO_DIR)/inc \
		-I$(WLAN_COMMON_INC)/$(UMAC_CRYPTO_DIR)/src
ifeq ($(CONFIG_CRYPTO_COMPONENT), y)
UMAC_CRYPTO_OBJS := $(UMAC_CRYPTO_CORE_DIR)/wlan_crypto_global_api.o \
		$(UMAC_CRYPTO_CORE_DIR)/wlan_crypto_ucfg_api.o \
		$(UMAC_CRYPTO_CORE_DIR)/wlan_crypto_main.o \
		$(UMAC_CRYPTO_CORE_DIR)/wlan_crypto_obj_mgr.o \
		$(UMAC_CRYPTO_CORE_DIR)/wlan_crypto_param_handling.o
endif

$(call add-wlan-objs,umac_crypto,$(UMAC_CRYPTO_OBJS))

############# FTM CORE ############
FTM_CORE_DIR := ftm
TARGET_IF_FTM_DIR := target_if/ftm
OS_IF_LINUX_FTM_DIR := os_if/linux/ftm

FTM_CORE_SRC := $(WLAN_COMMON_ROOT)/$(FTM_CORE_DIR)/core/src
FTM_DISP_SRC := $(WLAN_COMMON_ROOT)/$(FTM_CORE_DIR)/dispatcher/src
TARGET_IF_FTM_SRC := $(WLAN_COMMON_ROOT)/$(TARGET_IF_FTM_DIR)/src
OS_IF_FTM_SRC := $(WLAN_COMMON_ROOT)/$(OS_IF_LINUX_FTM_DIR)/src

FTM_CORE_INC := $(WLAN_COMMON_INC)/$(FTM_CORE_DIR)/core/src
FTM_DISP_INC := $(WLAN_COMMON_INC)/$(FTM_CORE_DIR)/dispatcher/inc
TARGET_IF_FTM_INC := $(WLAN_COMMON_INC)/$(TARGET_IF_FTM_DIR)/inc
OS_IF_FTM_INC := $(WLAN_COMMON_INC)/$(OS_IF_LINUX_FTM_DIR)/inc

FTM_INC := -I$(FTM_DISP_INC)	\
	   -I$(FTM_CORE_INC)	\
	   -I$(OS_IF_FTM_INC)	\
	   -I$(TARGET_IF_FTM_INC)

ifeq ($(CONFIG_QCA_WIFI_FTM), y)
FTM_OBJS := $(FTM_DISP_SRC)/wlan_ftm_init_deinit.o \
	    $(FTM_DISP_SRC)/wlan_ftm_ucfg_api.o \
	    $(FTM_CORE_SRC)/wlan_ftm_svc.o \
	    $(TARGET_IF_FTM_SRC)/target_if_ftm.o

ifeq ($(QCA_WIFI_FTM_NL80211), y)
FTM_OBJS += $(OS_IF_FTM_SRC)/wlan_cfg80211_ftm.o
endif

ifeq ($(CONFIG_LINUX_QCMBR), y)
FTM_OBJS += $(OS_IF_FTM_SRC)/wlan_ioctl_ftm.o
endif

endif

$(call add-wlan-objs,ftm,$(FTM_OBJS))

############# UMAC_CMN_SERVICES ############
UMAC_COMMON_INC := -I$(WLAN_COMMON_INC)/umac/cmn_services/cmn_defs/inc \
		-I$(WLAN_COMMON_INC)/umac/cmn_services/utils/inc
UMAC_COMMON_OBJS := $(WLAN_COMMON_ROOT)/umac/cmn_services/utils/src/wlan_utility.o

$(call add-wlan-objs,umac_common,$(UMAC_COMMON_OBJS))

############ CDS (Connectivity driver services) ############
CDS_DIR :=	core/cds
CDS_INC_DIR :=	$(CDS_DIR)/inc
CDS_SRC_DIR :=	$(CDS_DIR)/src

CDS_INC := 	-I$(WLAN_ROOT)/$(CDS_INC_DIR) \
		-I$(WLAN_ROOT)/$(CDS_SRC_DIR)

CDS_OBJS :=	$(CDS_SRC_DIR)/cds_api.o \
		$(CDS_SRC_DIR)/cds_reg_service.o \
		$(CDS_SRC_DIR)/cds_packet.o \
		$(CDS_SRC_DIR)/cds_regdomain.o \
		$(CDS_SRC_DIR)/cds_sched.o \
		$(CDS_SRC_DIR)/cds_utils.o

$(call add-wlan-objs,cds,$(CDS_OBJS))

###### UMAC OBJMGR ########
UMAC_OBJMGR_DIR := $(WLAN_COMMON_ROOT)/umac/cmn_services/obj_mgr

UMAC_OBJMGR_INC := -I$(WLAN_COMMON_INC)/umac/cmn_services/obj_mgr/inc \
		-I$(WLAN_COMMON_INC)/umac/cmn_services/obj_mgr/src \
		-I$(WLAN_COMMON_INC)/umac/cmn_services/inc \
		-I$(WLAN_COMMON_INC)/umac/global_umac_dispatcher/lmac_if/inc

UMAC_OBJMGR_OBJS := $(UMAC_OBJMGR_DIR)/src/wlan_objmgr_global_obj.o \
		$(UMAC_OBJMGR_DIR)/src/wlan_objmgr_pdev_obj.o \
		$(UMAC_OBJMGR_DIR)/src/wlan_objmgr_peer_obj.o \
		$(UMAC_OBJMGR_DIR)/src/wlan_objmgr_psoc_obj.o \
		$(UMAC_OBJMGR_DIR)/src/wlan_objmgr_vdev_obj.o

ifeq ($(CONFIG_WLAN_OBJMGR_DEBUG), y)
UMAC_OBJMGR_OBJS += $(UMAC_OBJMGR_DIR)/src/wlan_objmgr_debug.o
endif

$(call add-wlan-objs,umac_objmgr,$(UMAC_OBJMGR_OBJS))

###########  UMAC MGMT TXRX ##########
UMAC_MGMT_TXRX_DIR := $(WLAN_COMMON_ROOT)/umac/cmn_services/mgmt_txrx

UMAC_MGMT_TXRX_INC := -I$(WLAN_COMMON_INC)/umac/cmn_services/mgmt_txrx/dispatcher/inc \

UMAC_MGMT_TXRX_OBJS := $(UMAC_MGMT_TXRX_DIR)/core/src/wlan_mgmt_txrx_main.o \
	$(UMAC_MGMT_TXRX_DIR)/dispatcher/src/wlan_mgmt_txrx_utils_api.o \
	$(UMAC_MGMT_TXRX_DIR)/dispatcher/src/wlan_mgmt_txrx_tgt_api.o

$(call add-wlan-objs,umac_mgmt_txrx,$(UMAC_MGMT_TXRX_OBJS))

###### UMAC INTERFACE_MGR ########
UMAC_INTERFACE_MGR_COMP_DIR :=	components/cmn_services/interface_mgr
UMAC_INTERFACE_MGR_CMN_DIR := $(WLAN_COMMON_ROOT)/umac/cmn_services/interface_mgr

UMAC_INTERFACE_MGR_INC := -I$(WLAN_COMMON_INC)/umac/cmn_services/interface_mgr/inc \
			 -I$(WLAN_ROOT)/components/cmn_services/interface_mgr/inc

ifeq ($(CONFIG_INTERFACE_MGR), y)
UMAC_INTERFACE_MGR_OBJS := $(UMAC_INTERFACE_MGR_CMN_DIR)/src/wlan_if_mgr_main.o \
			  $(UMAC_INTERFACE_MGR_CMN_DIR)/src/wlan_if_mgr_core.o \
			  $(UMAC_INTERFACE_MGR_COMP_DIR)/src/wlan_if_mgr_sta.o \
			  $(UMAC_INTERFACE_MGR_COMP_DIR)/src/wlan_if_mgr_sap.o \
			  $(UMAC_INTERFACE_MGR_COMP_DIR)/src/wlan_if_mgr_roam.o
endif

$(call add-wlan-objs,umac_ifmgr,$(UMAC_INTERFACE_MGR_OBJS))

########## POWER MANAGEMENT OFFLOADS (PMO) ##########
PMO_DIR :=	components/pmo
PMO_INC :=	-I$(WLAN_ROOT)/$(PMO_DIR)/core/inc \
			-I$(WLAN_ROOT)/$(PMO_DIR)/core/src \
			-I$(WLAN_ROOT)/$(PMO_DIR)/dispatcher/inc \
			-I$(WLAN_ROOT)/$(PMO_DIR)/dispatcher/src \

ifeq ($(CONFIG_POWER_MANAGEMENT_OFFLOAD), y)
PMO_OBJS :=     $(PMO_DIR)/core/src/wlan_pmo_main.o \
		$(PMO_DIR)/core/src/wlan_pmo_apf.o \
		$(PMO_DIR)/core/src/wlan_pmo_arp.o \
		$(PMO_DIR)/core/src/wlan_pmo_gtk.o \
		$(PMO_DIR)/core/src/wlan_pmo_mc_addr_filtering.o \
		$(PMO_DIR)/core/src/wlan_pmo_static_config.o \
		$(PMO_DIR)/core/src/wlan_pmo_wow.o \
		$(PMO_DIR)/core/src/wlan_pmo_lphb.o \
		$(PMO_DIR)/core/src/wlan_pmo_suspend_resume.o \
		$(PMO_DIR)/core/src/wlan_pmo_hw_filter.o \
		$(PMO_DIR)/dispatcher/src/wlan_pmo_obj_mgmt_api.o \
		$(PMO_DIR)/dispatcher/src/wlan_pmo_ucfg_api.o \
		$(PMO_DIR)/dispatcher/src/wlan_pmo_tgt_arp.o \
		$(PMO_DIR)/dispatcher/src/wlan_pmo_tgt_gtk.o \
		$(PMO_DIR)/dispatcher/src/wlan_pmo_tgt_wow.o \
		$(PMO_DIR)/dispatcher/src/wlan_pmo_tgt_static_config.o \
		$(PMO_DIR)/dispatcher/src/wlan_pmo_tgt_mc_addr_filtering.o \
		$(PMO_DIR)/dispatcher/src/wlan_pmo_tgt_lphb.o \
		$(PMO_DIR)/dispatcher/src/wlan_pmo_tgt_suspend_resume.o \
		$(PMO_DIR)/dispatcher/src/wlan_pmo_tgt_hw_filter.o \

ifeq ($(CONFIG_WLAN_FEATURE_PACKET_FILTERING), y)
PMO_OBJS +=	$(PMO_DIR)/core/src/wlan_pmo_pkt_filter.o \
		$(PMO_DIR)/dispatcher/src/wlan_pmo_tgt_pkt_filter.o
endif
endif

ifeq ($(CONFIG_WLAN_NS_OFFLOAD), y)
PMO_OBJS +=     $(PMO_DIR)/core/src/wlan_pmo_ns.o \
		$(PMO_DIR)/dispatcher/src/wlan_pmo_tgt_ns.o
endif

$(call add-wlan-objs,pmo,$(PMO_OBJS))

########## DISA (ENCRYPTION TEST) ##########

DISA_DIR :=	components/disa
DISA_INC :=	-I$(WLAN_ROOT)/$(DISA_DIR)/core/inc \
		-I$(WLAN_ROOT)/$(DISA_DIR)/dispatcher/inc

ifeq ($(CONFIG_WLAN_FEATURE_DISA), y)
DISA_OBJS :=	$(DISA_DIR)/core/src/wlan_disa_main.o \
		$(DISA_DIR)/dispatcher/src/wlan_disa_obj_mgmt_api.o \
		$(DISA_DIR)/dispatcher/src/wlan_disa_tgt_api.o \
		$(DISA_DIR)/dispatcher/src/wlan_disa_ucfg_api.o
endif

$(call add-wlan-objs,disa,$(DISA_OBJS))

######## OCB ##############
OCB_DIR := components/ocb
OCB_INC := -I$(WLAN_ROOT)/$(OCB_DIR)/core/inc \
		-I$(WLAN_ROOT)/$(OCB_DIR)/dispatcher/inc

ifeq ($(CONFIG_WLAN_FEATURE_DSRC), y)
OCB_OBJS :=	$(OCB_DIR)/dispatcher/src/wlan_ocb_ucfg_api.o \
		$(OCB_DIR)/dispatcher/src/wlan_ocb_tgt_api.o \
		$(OCB_DIR)/core/src/wlan_ocb_main.o
endif

$(call add-wlan-objs,ocb,$(OCB_OBJS))

######## IPA ##############
IPA_DIR := components/ipa
IPA_INC := -I$(WLAN_ROOT)/$(IPA_DIR)/core/inc \
		-I$(WLAN_ROOT)/$(IPA_DIR)/dispatcher/inc

ifeq ($(CONFIG_IPA_OFFLOAD), y)
IPA_OBJS :=	$(IPA_DIR)/dispatcher/src/wlan_ipa_ucfg_api.o \
		$(IPA_DIR)/dispatcher/src/wlan_ipa_obj_mgmt_api.o \
		$(IPA_DIR)/dispatcher/src/wlan_ipa_tgt_api.o \
		$(IPA_DIR)/core/src/wlan_ipa_main.o \
		$(IPA_DIR)/core/src/wlan_ipa_core.o \
		$(IPA_DIR)/core/src/wlan_ipa_stats.o \
		$(IPA_DIR)/core/src/wlan_ipa_rm.o
endif

$(call add-wlan-objs,ipa,$(IPA_OBJS))

######## FWOL ##########
FWOL_CORE_INC := components/fw_offload/core/inc
FWOL_CORE_SRC := components/fw_offload/core/src
FWOL_DISPATCHER_INC := components/fw_offload/dispatcher/inc
FWOL_DISPATCHER_SRC := components/fw_offload/dispatcher/src
FWOL_TARGET_IF_INC := components/target_if/fw_offload/inc
FWOL_TARGET_IF_SRC := components/target_if/fw_offload/src
FWOL_OS_IF_INC := os_if/fw_offload/inc
FWOL_OS_IF_SRC := os_if/fw_offload/src

FWOL_INC := -I$(WLAN_ROOT)/$(FWOL_CORE_INC) \
	    -I$(WLAN_ROOT)/$(FWOL_DISPATCHER_INC) \
	    -I$(WLAN_ROOT)/$(FWOL_TARGET_IF_INC) \
	    -I$(WLAN_ROOT)/$(FWOL_OS_IF_INC) \
	    -I$(WLAN_COMMON_INC)/umac/thermal/dispatcher/inc

ifeq ($(CONFIG_WLAN_FW_OFFLOAD), y)
FWOL_OBJS :=	$(FWOL_CORE_SRC)/wlan_fw_offload_main.o \
		$(FWOL_DISPATCHER_SRC)/wlan_fwol_ucfg_api.o \
		$(FWOL_DISPATCHER_SRC)/wlan_fwol_tgt_api.o \
		$(FWOL_TARGET_IF_SRC)/target_if_fwol.o \
		$(FWOL_OS_IF_SRC)/os_if_fwol.o
endif

$(call add-wlan-objs,fwol,$(FWOL_OBJS))

######## SM FRAMEWORK  ##############
UMAC_SM_DIR := umac/cmn_services/sm_engine
UMAC_SM_INC := -I$(WLAN_COMMON_INC)/$(UMAC_SM_DIR)/inc

UMAC_SM_OBJS := $(WLAN_COMMON_ROOT)/$(UMAC_SM_DIR)/src/wlan_sm_engine.o

ifeq ($(CONFIG_SM_ENG_HIST), y)
UMAC_SM_OBJS +=	$(WLAN_COMMON_ROOT)/$(UMAC_SM_DIR)/src/wlan_sm_engine_dbg.o
endif

$(call add-wlan-objs,umac_sm,$(UMAC_SM_OBJS))

######## COMMON MLME ##############
UMAC_MLME_INC := -I$(WLAN_COMMON_INC)/umac/mlme \
		-I$(WLAN_COMMON_INC)/umac/mlme/mlme_objmgr/dispatcher/inc \
		-I$(WLAN_COMMON_INC)/umac/mlme/vdev_mgr/dispatcher/inc \
		-I$(WLAN_COMMON_INC)/umac/mlme/pdev_mgr/dispatcher/inc \
		-I$(WLAN_COMMON_INC)/umac/mlme/psoc_mgr/dispatcher/inc \
		-I$(WLAN_COMMON_INC)/umac/mlme/connection_mgr/dispatcher/inc

UMAC_MLME_OBJS := $(WLAN_COMMON_ROOT)/umac/mlme/mlme_objmgr/dispatcher/src/wlan_vdev_mlme_main.o \
		$(WLAN_COMMON_ROOT)/umac/mlme/vdev_mgr/core/src/vdev_mlme_sm.o \
		$(WLAN_COMMON_ROOT)/umac/mlme/vdev_mgr/dispatcher/src/wlan_vdev_mlme_api.o \
		$(WLAN_COMMON_ROOT)/umac/mlme/vdev_mgr/core/src/vdev_mgr_ops.o \
		$(WLAN_COMMON_ROOT)/umac/mlme/vdev_mgr/dispatcher/src/wlan_vdev_mgr_tgt_if_rx_api.o \
		$(WLAN_COMMON_ROOT)/umac/mlme/vdev_mgr/dispatcher/src/wlan_vdev_mgr_tgt_if_tx_api.o \
		$(WLAN_COMMON_ROOT)/umac/mlme/vdev_mgr/dispatcher/src/wlan_vdev_mgr_ucfg_api.o \
		$(WLAN_COMMON_ROOT)/umac/mlme/vdev_mgr/dispatcher/src/wlan_vdev_mgr_utils_api.o \
		$(WLAN_COMMON_ROOT)/umac/mlme/mlme_objmgr/dispatcher/src/wlan_cmn_mlme_main.o \
		$(WLAN_COMMON_ROOT)/umac/mlme/mlme_objmgr/dispatcher/src/wlan_pdev_mlme_main.o \
		$(WLAN_COMMON_ROOT)/umac/mlme/pdev_mgr/dispatcher/src/wlan_pdev_mlme_api.o \
		$(WLAN_COMMON_ROOT)/umac/mlme/mlme_objmgr/dispatcher/src/wlan_psoc_mlme_main.o \
		$(WLAN_COMMON_ROOT)/umac/mlme/psoc_mgr/dispatcher/src/wlan_psoc_mlme_api.o \
		$(WLAN_COMMON_ROOT)/umac/mlme/connection_mgr/core/src/wlan_cm_bss_scoring.o

$(call add-wlan-objs,umac_mlme,$(UMAC_MLME_OBJS))

######## MLME ##############
MLME_DIR := components/mlme
MLME_INC := -I$(WLAN_ROOT)/$(MLME_DIR)/core/inc \
		-I$(WLAN_ROOT)/$(MLME_DIR)/dispatcher/inc

MLME_OBJS :=	$(MLME_DIR)/core/src/wlan_mlme_main.o \
		$(MLME_DIR)/dispatcher/src/wlan_mlme_api.o \
		$(MLME_DIR)/dispatcher/src/wlan_mlme_ucfg_api.o

MLME_OBJS += $(MLME_DIR)/core/src/wlan_mlme_vdev_mgr_interface.o

ifeq ($(CONFIG_WLAN_FEATURE_TWT), y)
MLME_OBJS += $(MLME_DIR)/core/src/wlan_mlme_twt_api.o
MLME_OBJS += $(MLME_DIR)/dispatcher/src/wlan_mlme_twt_ucfg_api.o
endif

CM_DIR := components/umac/mlme/connection_mgr
CM_TGT_IF_DIR := components/target_if/connection_mgr

CM_INC := -I$(WLAN_ROOT)/$(CM_DIR)/dispatcher/inc \
		-I$(WLAN_ROOT)/$(CM_TGT_IF_DIR)/inc

MLME_INC += $(CM_INC)

CM_ROAM_OBJS :=    $(CM_DIR)/dispatcher/src/wlan_cm_tgt_if_tx_api.o \
			$(CM_DIR)/dispatcher/src/wlan_cm_roam_api.o \
			$(CM_DIR)/dispatcher/src/wlan_cm_roam_ucfg_api.o \
			$(CM_TGT_IF_DIR)/src/target_if_cm_roam_offload.o \
			$(CM_DIR)/core/src/wlan_cm_roam_offload.o

MLME_OBJS += $(CM_ROAM_OBJS)

####### WFA_CONFIG ########

WFA_DIR := components/umac/mlme/wfa_config
WFA_TGT_IF_DIR := components/target_if/wfa_config

WFA_INC := -I$(WLAN_ROOT)/$(WFA_DIR)/dispatcher/inc \
		-I$(WLAN_ROOT)/$(WFA_TGT_IF_DIR)/inc

MLME_INC += $(WFA_INC)

MLME_OBJS += $(WFA_TGT_IF_DIR)/src/target_if_wfa_testcmd.o \
		$(WFA_DIR)/dispatcher/src/wlan_wfa_tgt_if_tx_api.o

$(call add-wlan-objs,mlme,$(MLME_OBJS))

####### BLACKLIST_MGR ########

BLM_DIR := components/blacklist_mgr
BLM_INC := -I$(WLAN_ROOT)/$(BLM_DIR)/core/inc \
                -I$(WLAN_ROOT)/$(BLM_DIR)/dispatcher/inc
ifeq ($(CONFIG_FEATURE_BLACKLIST_MGR), y)
BLM_OBJS :=    $(BLM_DIR)/core/src/wlan_blm_main.o \
                $(BLM_DIR)/core/src/wlan_blm_core.o \
                $(BLM_DIR)/dispatcher/src/wlan_blm_ucfg_api.o \
                $(BLM_DIR)/dispatcher/src/wlan_blm_tgt_api.o
endif

$(call add-wlan-objs,blm,$(BLM_OBJS))

########## ACTION OUI ##########

ACTION_OUI_DIR := components/action_oui
ACTION_OUI_INC := -I$(WLAN_ROOT)/$(ACTION_OUI_DIR)/core/inc \
		  -I$(WLAN_ROOT)/$(ACTION_OUI_DIR)/dispatcher/inc

ifeq ($(CONFIG_WLAN_FEATURE_ACTION_OUI), y)
ACTION_OUI_OBJS := $(ACTION_OUI_DIR)/core/src/wlan_action_oui_main.o \
		$(ACTION_OUI_DIR)/core/src/wlan_action_oui_parse.o \
		$(ACTION_OUI_DIR)/dispatcher/src/wlan_action_oui_tgt_api.o \
		$(ACTION_OUI_DIR)/dispatcher/src/wlan_action_oui_ucfg_api.o
endif

$(call add-wlan-objs,action_oui,$(ACTION_OUI_OBJS))

######## PACKET CAPTURE ########

PKT_CAPTURE_DIR := components/pkt_capture
PKT_CAPTURE_OS_IF_DIR := os_if/pkt_capture
PKT_CAPTURE_TARGET_IF_DIR := components/target_if/pkt_capture/
PKT_CAPTURE_INC := -I$(WLAN_ROOT)/$(PKT_CAPTURE_DIR)/core/inc \
		  -I$(WLAN_ROOT)/$(PKT_CAPTURE_DIR)/dispatcher/inc \
		  -I$(WLAN_ROOT)/$(PKT_CAPTURE_TARGET_IF_DIR)/inc \
		  -I$(WLAN_ROOT)/$(PKT_CAPTURE_OS_IF_DIR)/inc

ifeq ($(CONFIG_WLAN_FEATURE_PKT_CAPTURE), y)
PKT_CAPTURE_OBJS := $(PKT_CAPTURE_DIR)/core/src/wlan_pkt_capture_main.o \
		$(PKT_CAPTURE_DIR)/core/src/wlan_pkt_capture_mon_thread.o \
		$(PKT_CAPTURE_DIR)/core/src/wlan_pkt_capture_mgmt_txrx.o \
		$(PKT_CAPTURE_DIR)/core/src/wlan_pkt_capture_data_txrx.o \
		$(PKT_CAPTURE_DIR)/dispatcher/src/wlan_pkt_capture_ucfg_api.o \
		$(PKT_CAPTURE_DIR)/dispatcher/src/wlan_pkt_capture_tgt_api.o \
		$(PKT_CAPTURE_DIR)/dispatcher/src/wlan_pkt_capture_api.o \
		$(PKT_CAPTURE_TARGET_IF_DIR)/src/target_if_pkt_capture.o \
		$(PKT_CAPTURE_OS_IF_DIR)/src/os_if_pkt_capture.o
endif

$(call add-wlan-objs,pkt_capture,$(PKT_CAPTURE_OBJS))

########## FTM TIME SYNC ##########

FTM_TIME_SYNC_DIR := components/ftm_time_sync
FTM_TIME_SYNC_INC := -I$(WLAN_ROOT)/$(FTM_TIME_SYNC_DIR)/core/inc \
		  -I$(WLAN_ROOT)/$(FTM_TIME_SYNC_DIR)/dispatcher/inc

ifeq ($(CONFIG_FEATURE_WLAN_TIME_SYNC_FTM), y)
FTM_TIME_SYNC_OBJS := $(FTM_TIME_SYNC_DIR)/core/src/ftm_time_sync_main.o \
		$(FTM_TIME_SYNC_DIR)/dispatcher/src/ftm_time_sync_ucfg_api.o \
		$(FTM_TIME_SYNC_DIR)/dispatcher/src/wlan_ftm_time_sync_tgt_api.o
endif

$(call add-wlan-objs,ftm_time_sync,$(FTM_TIME_SYNC_OBJS))

########## CLD TARGET_IF #######
CLD_TARGET_IF_DIR := components/target_if

CLD_TARGET_IF_INC := -I$(WLAN_ROOT)/$(CLD_TARGET_IF_DIR)/pmo/inc \
	 -I$(WLAN_ROOT)/$(CLD_TARGET_IF_DIR)/pmo/src

ifeq ($(CONFIG_POWER_MANAGEMENT_OFFLOAD), y)
CLD_TARGET_IF_OBJ := $(CLD_TARGET_IF_DIR)/pmo/src/target_if_pmo_arp.o \
		$(CLD_TARGET_IF_DIR)/pmo/src/target_if_pmo_gtk.o \
		$(CLD_TARGET_IF_DIR)/pmo/src/target_if_pmo_hw_filter.o \
		$(CLD_TARGET_IF_DIR)/pmo/src/target_if_pmo_lphb.o \
		$(CLD_TARGET_IF_DIR)/pmo/src/target_if_pmo_main.o \
		$(CLD_TARGET_IF_DIR)/pmo/src/target_if_pmo_mc_addr_filtering.o \
		$(CLD_TARGET_IF_DIR)/pmo/src/target_if_pmo_static_config.o \
		$(CLD_TARGET_IF_DIR)/pmo/src/target_if_pmo_suspend_resume.o \
		$(CLD_TARGET_IF_DIR)/pmo/src/target_if_pmo_wow.o
ifeq ($(CONFIG_WLAN_NS_OFFLOAD), y)
CLD_TARGET_IF_OBJ += $(CLD_TARGET_IF_DIR)/pmo/src/target_if_pmo_ns.o
endif
ifeq ($(CONFIG_WLAN_FEATURE_PACKET_FILTERING), y)
CLD_TARGET_IF_OBJ += $(CLD_TARGET_IF_DIR)/pmo/src/target_if_pmo_pkt_filter.o
endif
endif

ifeq ($(CONFIG_WLAN_FEATURE_DSRC), y)
CLD_TARGET_IF_INC += -I$(WLAN_ROOT)/$(CLD_TARGET_IF_DIR)/ocb/inc
CLD_TARGET_IF_OBJ += $(CLD_TARGET_IF_DIR)/ocb/src/target_if_ocb.o
endif

ifeq ($(CONFIG_WLAN_FEATURE_DISA), y)
CLD_TARGET_IF_INC += -I$(WLAN_ROOT)/$(CLD_TARGET_IF_DIR)/disa/inc
CLD_TARGET_IF_OBJ += $(CLD_TARGET_IF_DIR)/disa/src/target_if_disa.o
endif

ifeq ($(CONFIG_FEATURE_BLACKLIST_MGR), y)
CLD_TARGET_IF_INC += -I$(WLAN_ROOT)/$(CLD_TARGET_IF_DIR)/blacklist_mgr/inc
CLD_TARGET_IF_OBJ += $(CLD_TARGET_IF_DIR)/blacklist_mgr/src/target_if_blm.o
endif

ifeq ($(CONFIG_IPA_OFFLOAD), y)
CLD_TARGET_IF_INC += -I$(WLAN_ROOT)/$(CLD_TARGET_IF_DIR)/ipa/inc
CLD_TARGET_IF_OBJ += $(CLD_TARGET_IF_DIR)/ipa/src/target_if_ipa.o
endif

ifeq ($(CONFIG_WLAN_FEATURE_ACTION_OUI), y)
CLD_TARGET_IF_INC += -I$(WLAN_ROOT)/$(CLD_TARGET_IF_DIR)/action_oui/inc
CLD_TARGET_IF_OBJ += $(CLD_TARGET_IF_DIR)/action_oui/src/target_if_action_oui.o
endif

ifeq ($(CONFIG_FEATURE_WLAN_TIME_SYNC_FTM), y)
CLD_TARGET_IF_INC += -I$(WLAN_ROOT)/$(CLD_TARGET_IF_DIR)/ftm_time_sync/inc
CLD_TARGET_IF_OBJ += $(CLD_TARGET_IF_DIR)/ftm_time_sync/src/target_if_ftm_time_sync.o
endif

$(call add-wlan-objs,cld_target_if,$(CLD_TARGET_IF_OBJ))

############## UMAC P2P ###########
P2P_DIR := components/p2p
P2P_CORE_OBJ_DIR := $(P2P_DIR)/core/src
P2P_DISPATCHER_DIR := $(P2P_DIR)/dispatcher
P2P_DISPATCHER_INC_DIR := $(P2P_DISPATCHER_DIR)/inc
P2P_DISPATCHER_OBJ_DIR := $(P2P_DISPATCHER_DIR)/src
P2P_OS_IF_INC := os_if/p2p/inc
P2P_OS_IF_SRC := os_if/p2p/src
P2P_TARGET_IF_INC := components/target_if/p2p/inc
P2P_TARGET_IF_SRC := components/target_if/p2p/src
P2P_INC := -I$(WLAN_ROOT)/$(P2P_DISPATCHER_INC_DIR) \
	   -I$(WLAN_ROOT)/$(P2P_OS_IF_INC) \
	   -I$(WLAN_ROOT)/$(P2P_TARGET_IF_INC)
P2P_OBJS := $(P2P_DISPATCHER_OBJ_DIR)/wlan_p2p_ucfg_api.o \
	    $(P2P_DISPATCHER_OBJ_DIR)/wlan_p2p_tgt_api.o \
	    $(P2P_DISPATCHER_OBJ_DIR)/wlan_p2p_cfg.o \
	    $(P2P_DISPATCHER_OBJ_DIR)/wlan_p2p_api.o \
	    $(P2P_CORE_OBJ_DIR)/wlan_p2p_main.o \
	    $(P2P_CORE_OBJ_DIR)/wlan_p2p_roc.o \
	    $(P2P_CORE_OBJ_DIR)/wlan_p2p_off_chan_tx.o \
	    $(P2P_OS_IF_SRC)/wlan_cfg80211_p2p.o \
	    $(P2P_TARGET_IF_SRC)/target_if_p2p.o

$(call add-wlan-objs,p2p,$(P2P_OBJS))

###### UMAC POLICY MGR ########
POLICY_MGR_DIR := components/cmn_services/policy_mgr

POLICY_MGR_INC := -I$(WLAN_ROOT)/$(POLICY_MGR_DIR)/inc \
		  -I$(WLAN_ROOT)/$(POLICY_MGR_DIR)/src

POLICY_MGR_OBJS := $(POLICY_MGR_DIR)/src/wlan_policy_mgr_action.o \
	$(POLICY_MGR_DIR)/src/wlan_policy_mgr_core.o \
	$(POLICY_MGR_DIR)/src/wlan_policy_mgr_get_set_utils.o \
	$(POLICY_MGR_DIR)/src/wlan_policy_mgr_init_deinit.o \
	$(POLICY_MGR_DIR)/src/wlan_policy_mgr_ucfg.o \
	$(POLICY_MGR_DIR)/src/wlan_policy_mgr_pcl.o \

$(call add-wlan-objs,policy_mgr,$(POLICY_MGR_OBJS))

###### UMAC TDLS ########
TDLS_DIR := components/tdls

TDLS_OS_IF_INC := os_if/tdls/inc
TDLS_OS_IF_SRC := os_if/tdls/src
TDLS_TARGET_IF_INC := components/target_if/tdls/inc
TDLS_TARGET_IF_SRC := components/target_if/tdls/src
TDLS_INC := -I$(WLAN_ROOT)/$(TDLS_DIR)/dispatcher/inc \
	    -I$(WLAN_ROOT)/$(TDLS_OS_IF_INC) \
	    -I$(WLAN_ROOT)/$(TDLS_TARGET_IF_INC)

ifeq ($(CONFIG_QCOM_TDLS), y)
TDLS_OBJS := $(TDLS_DIR)/core/src/wlan_tdls_main.o \
       $(TDLS_DIR)/core/src/wlan_tdls_cmds_process.o \
       $(TDLS_DIR)/core/src/wlan_tdls_peer.o \
       $(TDLS_DIR)/core/src/wlan_tdls_mgmt.o \
       $(TDLS_DIR)/core/src/wlan_tdls_ct.o \
       $(TDLS_DIR)/dispatcher/src/wlan_tdls_tgt_api.o \
       $(TDLS_DIR)/dispatcher/src/wlan_tdls_ucfg_api.o \
       $(TDLS_DIR)/dispatcher/src/wlan_tdls_utils_api.o \
       $(TDLS_DIR)/dispatcher/src/wlan_tdls_cfg.o \
       $(TDLS_DIR)/dispatcher/src/wlan_tdls_api.o \
       $(TDLS_OS_IF_SRC)/wlan_cfg80211_tdls.o \
       $(TDLS_TARGET_IF_SRC)/target_if_tdls.o
endif

$(call add-wlan-objs,tdls,$(TDLS_OBJS))

########### BMI ###########
BMI_DIR := core/bmi

BMI_INC := -I$(WLAN_ROOT)/$(BMI_DIR)/inc

ifeq ($(CONFIG_WLAN_FEATURE_BMI), y)
BMI_OBJS := $(BMI_DIR)/src/bmi.o \
            $(BMI_DIR)/src/bmi_1.o \
            $(BMI_DIR)/src/ol_fw.o \
            $(BMI_DIR)/src/ol_fw_common.o
endif

$(call add-wlan-objs,bmi,$(BMI_OBJS))

##########  TARGET_IF #######
TARGET_IF_DIR := $(WLAN_COMMON_ROOT)/target_if

TARGET_IF_INC := -I$(WLAN_COMMON_INC)/target_if/core/inc \
		 -I$(WLAN_COMMON_INC)/target_if/core/src \
		 -I$(WLAN_COMMON_INC)/target_if/init_deinit/inc \
		 -I$(WLAN_COMMON_INC)/target_if/crypto/inc \
		 -I$(WLAN_COMMON_INC)/target_if/regulatory/inc \
		 -I$(WLAN_COMMON_INC)/target_if/mlme/vdev_mgr/inc \
		 -I$(WLAN_COMMON_INC)/target_if/dispatcher/inc \
		 -I$(WLAN_COMMON_INC)/target_if/mlme/psoc/inc

TARGET_IF_OBJ := $(TARGET_IF_DIR)/core/src/target_if_main.o \
		$(TARGET_IF_DIR)/regulatory/src/target_if_reg.o \
		$(TARGET_IF_DIR)/regulatory/src/target_if_reg_lte.o \
		$(TARGET_IF_DIR)/regulatory/src/target_if_reg_11d.o \
		$(TARGET_IF_DIR)/init_deinit/src/init_cmd_api.o \
		$(TARGET_IF_DIR)/init_deinit/src/init_deinit_lmac.o \
		$(TARGET_IF_DIR)/init_deinit/src/init_event_handler.o \
		$(TARGET_IF_DIR)/init_deinit/src/service_ready_util.o \
		$(TARGET_IF_DIR)/mlme/vdev_mgr/src/target_if_vdev_mgr_tx_ops.o \
		$(TARGET_IF_DIR)/mlme/vdev_mgr/src/target_if_vdev_mgr_rx_ops.o \
		$(TARGET_IF_DIR)/mlme/psoc/src/target_if_psoc_timer_tx_ops.o

ifeq ($(CONFIG_FEATURE_VDEV_OPS_WAKELOCK), y)
TARGET_IF_OBJ += $(TARGET_IF_DIR)/mlme/psoc/src/target_if_psoc_wake_lock.o
endif

ifeq ($(CONFIG_CRYPTO_COMPONENT), y)
TARGET_IF_OBJ += $(TARGET_IF_DIR)/crypto/src/target_if_crypto.o
endif

$(call add-wlan-objs,target_if,$(TARGET_IF_OBJ))

########### GLOBAL_LMAC_IF ##########
GLOBAL_LMAC_IF_DIR := $(WLAN_COMMON_ROOT)/global_lmac_if

GLOBAL_LMAC_IF_INC := -I$(WLAN_COMMON_INC)/global_lmac_if/inc \
                      -I$(WLAN_COMMON_INC)/global_lmac_if/src

GLOBAL_LMAC_IF_OBJ := $(GLOBAL_LMAC_IF_DIR)/src/wlan_global_lmac_if.o

$(call add-wlan-objs,global_lmac_if,$(GLOBAL_LMAC_IF_OBJ))

########### WMI ###########
WMI_ROOT_DIR := wmi

WMI_SRC_DIR := $(WMI_ROOT_DIR)/src
WMI_INC_DIR := $(WMI_ROOT_DIR)/inc
WMI_OBJ_DIR := $(WLAN_COMMON_ROOT)/$(WMI_SRC_DIR)

WMI_INC := -I$(WLAN_COMMON_INC)/$(WMI_INC_DIR)

WMI_OBJS := $(WMI_OBJ_DIR)/wmi_unified.o \
	    $(WMI_OBJ_DIR)/wmi_tlv_helper.o \
	    $(WMI_OBJ_DIR)/wmi_unified_tlv.o \
	    $(WMI_OBJ_DIR)/wmi_unified_api.o \
	    $(WMI_OBJ_DIR)/wmi_unified_reg_api.o \
	    $(WMI_OBJ_DIR)/wmi_unified_vdev_api.o \
	    $(WMI_OBJ_DIR)/wmi_unified_vdev_tlv.o

ifeq ($(CONFIG_POWER_MANAGEMENT_OFFLOAD), y)
WMI_OBJS += $(WMI_OBJ_DIR)/wmi_unified_pmo_api.o
WMI_OBJS += $(WMI_OBJ_DIR)/wmi_unified_pmo_tlv.o
endif

ifeq ($(CONFIG_QCACLD_FEATURE_APF), y)
WMI_OBJS += $(WMI_OBJ_DIR)/wmi_unified_apf_tlv.o
endif

ifeq ($(CONFIG_WLAN_FEATURE_ACTION_OUI), y)
WMI_OBJS += $(WMI_OBJ_DIR)/wmi_unified_action_oui_tlv.o
endif

ifeq ($(CONFIG_WLAN_FEATURE_DSRC), y)
ifeq ($(CONFIG_OCB_UT_FRAMEWORK), y)
WMI_OBJS += $(WMI_OBJ_DIR)/wmi_unified_ocb_ut.o
endif
endif

ifeq ($(CONFIG_WLAN_DFS_MASTER_ENABLE), y)
WMI_OBJS += $(WMI_OBJ_DIR)/wmi_unified_dfs_api.o
endif

ifeq ($(CONFIG_WLAN_FEATURE_TWT), y)
WMI_OBJS += $(WMI_OBJ_DIR)/wmi_unified_twt_api.o
WMI_OBJS += $(WMI_OBJ_DIR)/wmi_unified_twt_tlv.o
endif

ifeq ($(CONFIG_WLAN_FEATURE_DSRC), y)
WMI_OBJS += $(WMI_OBJ_DIR)/wmi_unified_ocb_api.o
WMI_OBJS += $(WMI_OBJ_DIR)/wmi_unified_ocb_tlv.o
endif

ifeq ($(CONFIG_FEATURE_WLAN_EXTSCAN), y)
WMI_OBJS += $(WMI_OBJ_DIR)/wmi_unified_extscan_api.o
WMI_OBJS += $(WMI_OBJ_DIR)/wmi_unified_extscan_tlv.o
endif

ifeq ($(CONFIG_FEATURE_INTEROP_ISSUES_AP), y)
WMI_OBJS += $(WMI_OBJ_DIR)/wmi_unified_interop_issues_ap_api.o
WMI_OBJS += $(WMI_OBJ_DIR)/wmi_unified_interop_issues_ap_tlv.o
endif

ifeq ($(CONFIG_DCS), y)
WMI_OBJS += $(WMI_OBJ_DIR)/wmi_unified_dcs_api.o
WMI_OBJS += $(WMI_OBJ_DIR)/wmi_unified_dcs_tlv.o
endif

ifeq ($(CONFIG_QCACLD_FEATURE_NAN), y)
WMI_OBJS += $(WMI_OBJ_DIR)/wmi_unified_nan_api.o
WMI_OBJS += $(WMI_OBJ_DIR)/wmi_unified_nan_tlv.o
endif

ifeq ($(CONFIG_CONVERGED_P2P_ENABLE), y)
WMI_OBJS += $(WMI_OBJ_DIR)/wmi_unified_p2p_api.o
WMI_OBJS += $(WMI_OBJ_DIR)/wmi_unified_p2p_tlv.o
endif

ifeq ($(CONFIG_WMI_CONCURRENCY_SUPPORT), y)
WMI_OBJS += $(WMI_OBJ_DIR)/wmi_unified_concurrency_api.o
WMI_OBJS += $(WMI_OBJ_DIR)/wmi_unified_concurrency_tlv.o
endif

ifeq ($(CONFIG_WMI_STA_SUPPORT), y)
WMI_OBJS += $(WMI_OBJ_DIR)/wmi_unified_sta_api.o
WMI_OBJS += $(WMI_OBJ_DIR)/wmi_unified_sta_tlv.o
endif

ifeq ($(CONFIG_WMI_BCN_OFFLOAD), y)
WMI_OBJS += $(WMI_OBJ_DIR)/wmi_unified_bcn_api.o
WMI_OBJS += $(WMI_OBJ_DIR)/wmi_unified_bcn_tlv.o
endif

ifeq ($(CONFIG_WLAN_FW_OFFLOAD), y)
WMI_OBJS += $(WMI_OBJ_DIR)/wmi_unified_fwol_api.o
WMI_OBJS += $(WMI_OBJ_DIR)/wmi_unified_fwol_tlv.o
endif

ifeq ($(CONFIG_WLAN_HANG_EVENT), y)
WMI_OBJS += $(WMI_OBJ_DIR)/wmi_hang_event.o
endif

ifeq ($(CONFIG_WLAN_CFR_ENABLE), y)
WMI_OBJS += $(WMI_OBJ_DIR)/wmi_unified_cfr_tlv.o
WMI_OBJS += $(WMI_OBJ_DIR)/wmi_unified_cfr_api.o
endif

ifeq ($(CONFIG_CP_STATS), y)
WMI_OBJS += $(WMI_OBJ_DIR)/wmi_unified_cp_stats_api.o
WMI_OBJS += $(WMI_OBJ_DIR)/wmi_unified_cp_stats_tlv.o
endif

$(call add-wlan-objs,wmi,$(WMI_OBJS))

########### FWLOG ###########
FWLOG_DIR := $(WLAN_COMMON_ROOT)/utils/fwlog

FWLOG_INC := -I$(WLAN_ROOT)/$(FWLOG_DIR)

ifeq ($(CONFIG_FEATURE_FW_LOG_PARSING), y)
FWLOG_OBJS := $(FWLOG_DIR)/dbglog_host.o
endif

$(call add-wlan-objs,fwlog,$(FWLOG_OBJS))

############ TXRX ############
TXRX_DIR :=     core/dp/txrx
TXRX_INC :=     -I$(WLAN_ROOT)/$(TXRX_DIR)

TXRX_OBJS :=
ifeq ($(CONFIG_WDI_EVENT_ENABLE), y)
TXRX_OBJS += 	$(TXRX_DIR)/ol_txrx_event.o
endif

ifneq ($(CONFIG_LITHIUM), y)
TXRX_OBJS += $(TXRX_DIR)/ol_txrx.o \
		$(TXRX_DIR)/ol_cfg.o \
                $(TXRX_DIR)/ol_rx.o \
                $(TXRX_DIR)/ol_rx_fwd.o \
                $(TXRX_DIR)/ol_txrx.o \
                $(TXRX_DIR)/ol_rx_defrag.o \
                $(TXRX_DIR)/ol_tx_desc.o \
                $(TXRX_DIR)/ol_tx.o \
                $(TXRX_DIR)/ol_rx_reorder_timeout.o \
                $(TXRX_DIR)/ol_rx_reorder.o \
                $(TXRX_DIR)/ol_rx_pn.o \
                $(TXRX_DIR)/ol_txrx_peer_find.o \
                $(TXRX_DIR)/ol_txrx_encap.o \
                $(TXRX_DIR)/ol_tx_send.o

ifeq ($(CONFIG_LL_DP_SUPPORT), y)

TXRX_OBJS +=     $(TXRX_DIR)/ol_tx_ll.o

ifeq ($(CONFIG_WLAN_FASTPATH), y)
TXRX_OBJS +=     $(TXRX_DIR)/ol_tx_ll_fastpath.o
else
TXRX_OBJS +=     $(TXRX_DIR)/ol_tx_ll_legacy.o
endif

ifeq ($(CONFIG_WLAN_TX_FLOW_CONTROL_V2), y)
TXRX_OBJS +=     $(TXRX_DIR)/ol_txrx_flow_control.o
endif

endif #CONFIG_LL_DP_SUPPORT

ifeq ($(CONFIG_HL_DP_SUPPORT), y)
TXRX_OBJS +=     $(TXRX_DIR)/ol_tx_hl.o
TXRX_OBJS +=     $(TXRX_DIR)/ol_tx_classify.o
TXRX_OBJS +=     $(TXRX_DIR)/ol_tx_sched.o
TXRX_OBJS +=     $(TXRX_DIR)/ol_tx_queue.o
endif #CONFIG_HL_DP_SUPPORT

ifeq ($(CONFIG_WLAN_TX_FLOW_CONTROL_LEGACY), y)
TXRX_OBJS +=     $(TXRX_DIR)/ol_txrx_legacy_flow_control.o
endif

ifeq ($(CONFIG_IPA_OFFLOAD), y)
TXRX_OBJS +=     $(TXRX_DIR)/ol_txrx_ipa.o
endif

ifeq ($(CONFIG_QCA_SUPPORT_TX_THROTTLE), y)
TXRX_OBJS +=     $(TXRX_DIR)/ol_tx_throttle.o
endif
endif #LITHIUM

$(call add-wlan-objs,txrx,$(TXRX_OBJS))

############ TXRX 3.0 ############
TXRX3.0_DIR :=     core/dp/txrx3.0
TXRX3.0_INC :=     -I$(WLAN_ROOT)/$(TXRX3.0_DIR)

ifeq ($(CONFIG_LITHIUM), y)
TXRX3.0_OBJS := $(TXRX3.0_DIR)/dp_txrx.o

ifeq ($(CONFIG_WLAN_FEATURE_DP_RX_THREADS), y)
TXRX3.0_OBJS += $(TXRX3.0_DIR)/dp_rx_thread.o
endif

ifeq ($(CONFIG_RX_FISA), y)
TXRX3.0_OBJS += $(TXRX3.0_DIR)/dp_fisa_rx.o
TXRX3.0_OBJS += $(TXRX3.0_DIR)/dp_rx_fst.o
endif

ifeq ($(CONFIG_DP_SWLM), y)
TXRX3.0_OBJS += $(TXRX3.0_DIR)/dp_swlm.o
endif

endif #LITHIUM

$(call add-wlan-objs,txrx30,$(TXRX3.0_OBJS))

ifeq ($(CONFIG_LITHIUM), y)
############ DP 3.0 ############
DP_INC := -I$(WLAN_COMMON_INC)/dp/inc \
	-I$(WLAN_COMMON_INC)/dp/wifi3.0 \
	-I$(WLAN_COMMON_INC)/target_if/dp/inc

DP_SRC := $(WLAN_COMMON_ROOT)/dp/wifi3.0
DP_OBJS := $(DP_SRC)/dp_main.o \
		$(DP_SRC)/dp_tx.o \
		$(DP_SRC)/dp_tx_desc.o \
		$(DP_SRC)/dp_rx.o \
		$(DP_SRC)/dp_rx_err.o \
		$(DP_SRC)/dp_htt.o \
		$(DP_SRC)/dp_peer.o \
		$(DP_SRC)/dp_rx_desc.o \
		$(DP_SRC)/dp_reo.o \
		$(DP_SRC)/dp_rx_mon_dest.o \
		$(DP_SRC)/dp_rx_mon_status.o \
		$(DP_SRC)/dp_rx_defrag.o \
		$(DP_SRC)/dp_mon_filter.o \
		$(DP_SRC)/dp_stats.o \
		$(WLAN_COMMON_ROOT)/target_if/dp/src/target_if_dp.o
ifeq ($(CONFIG_WLAN_TX_FLOW_CONTROL_V2), y)
DP_OBJS += $(DP_SRC)/dp_tx_flow_control.o
endif

ifeq ($(CONFIG_WLAN_FEATURE_RX_BUFFER_POOL), y)
DP_OBJS += $(DP_SRC)/dp_rx_buffer_pool.o
endif

ifeq ($(CONFIG_IPA_OFFLOAD), y)
DP_OBJS +=     $(DP_SRC)/dp_ipa.o
endif

ifeq ($(CONFIG_WDI_EVENT_ENABLE), y)
DP_OBJS +=     $(DP_SRC)/dp_wdi_event.o
endif

endif #LITHIUM

$(call add-wlan-objs,dp,$(DP_OBJS))

############ CFG ############
WCFG_DIR := wlan_cfg
WCFG_INC := -I$(WLAN_COMMON_INC)/$(WCFG_DIR)
WCFG_SRC := $(WLAN_COMMON_ROOT)/$(WCFG_DIR)

ifeq ($(CONFIG_LITHIUM), y)
WCFG_OBJS := $(WCFG_SRC)/wlan_cfg.o
endif

$(call add-wlan-objs,wcfg,$(WCFG_OBJS))

############ OL ############
OL_DIR :=     core/dp/ol
OL_INC :=     -I$(WLAN_ROOT)/$(OL_DIR)/inc

############ CDP ############
CDP_ROOT_DIR := dp
CDP_INC_DIR := $(CDP_ROOT_DIR)/inc
CDP_INC := -I$(WLAN_COMMON_INC)/$(CDP_INC_DIR)

############ PKTLOG ############
PKTLOG_DIR :=      $(WLAN_COMMON_ROOT)/utils/pktlog
PKTLOG_INC :=      -I$(WLAN_ROOT)/$(PKTLOG_DIR)/include

ifeq ($(CONFIG_REMOVE_PKT_LOG), n)
PKTLOG_OBJS :=	$(PKTLOG_DIR)/pktlog_ac.o \
		$(PKTLOG_DIR)/pktlog_internal.o \
		$(PKTLOG_DIR)/linux_ac.o

ifeq ($(CONFIG_PKTLOG_LEGACY), y)
	PKTLOG_OBJS  += $(PKTLOG_DIR)/pktlog_wifi2.o
else
	PKTLOG_OBJS  += $(PKTLOG_DIR)/pktlog_wifi3.o
endif
endif

$(call add-wlan-objs,pktlog,$(PKTLOG_OBJS))

############ HTT ############
HTT_DIR :=      core/dp/htt
HTT_INC :=      -I$(WLAN_ROOT)/$(HTT_DIR)

ifneq ($(CONFIG_LITHIUM), y)
HTT_OBJS := $(HTT_DIR)/htt_tx.o \
            $(HTT_DIR)/htt.o \
            $(HTT_DIR)/htt_t2h.o \
            $(HTT_DIR)/htt_h2t.o \
            $(HTT_DIR)/htt_fw_stats.o \
            $(HTT_DIR)/htt_rx.o

ifeq ($(CONFIG_FEATURE_MONITOR_MODE_SUPPORT), y)
HTT_OBJS += $(HTT_DIR)/htt_monitor_rx.o
endif

ifeq ($(CONFIG_LL_DP_SUPPORT), y)
HTT_OBJS += $(HTT_DIR)/htt_rx_ll.o
endif

ifeq ($(CONFIG_HL_DP_SUPPORT), y)
HTT_OBJS += $(HTT_DIR)/htt_rx_hl.o
endif
endif

$(call add-wlan-objs,htt,$(HTT_OBJS))

############## INIT-DEINIT ###########
INIT_DEINIT_DIR := init_deinit/dispatcher
INIT_DEINIT_INC_DIR := $(INIT_DEINIT_DIR)/inc
INIT_DEINIT_SRC_DIR := $(INIT_DEINIT_DIR)/src
INIT_DEINIT_OBJ_DIR := $(WLAN_COMMON_ROOT)/$(INIT_DEINIT_SRC_DIR)
INIT_DEINIT_INC := -I$(WLAN_COMMON_INC)/$(INIT_DEINIT_INC_DIR)
INIT_DEINIT_OBJS := $(INIT_DEINIT_OBJ_DIR)/dispatcher_init_deinit.o

$(call add-wlan-objs,init_deinit,$(INIT_DEINIT_OBJS))

############## REGULATORY ###########
REGULATORY_DIR := umac/regulatory
REGULATORY_CORE_INC_DIR := $(REGULATORY_DIR)/core/inc
REGULATORY_CORE_SRC_DIR := $(REGULATORY_DIR)/core/src
REG_DISPATCHER_INC_DIR := $(REGULATORY_DIR)/dispatcher/inc
REG_DISPATCHER_SRC_DIR := $(REGULATORY_DIR)/dispatcher/src
REG_CORE_OBJ_DIR := $(WLAN_COMMON_ROOT)/$(REGULATORY_CORE_SRC_DIR)
REG_DISPATCHER_OBJ_DIR := $(WLAN_COMMON_ROOT)/$(REG_DISPATCHER_SRC_DIR)
REGULATORY_INC := -I$(WLAN_COMMON_INC)/$(REGULATORY_CORE_INC_DIR)
REGULATORY_INC += -I$(WLAN_COMMON_INC)/$(REG_DISPATCHER_INC_DIR)
REGULATORY_OBJS := $(REG_CORE_OBJ_DIR)/reg_build_chan_list.o \
		    $(REG_CORE_OBJ_DIR)/reg_callbacks.o \
		    $(REG_CORE_OBJ_DIR)/reg_db.o \
		    $(REG_CORE_OBJ_DIR)/reg_db_parser.o \
		    $(REG_CORE_OBJ_DIR)/reg_utils.o \
		    $(REG_CORE_OBJ_DIR)/reg_lte.o \
		    $(REG_CORE_OBJ_DIR)/reg_offload_11d_scan.o \
		    $(REG_CORE_OBJ_DIR)/reg_opclass.o \
		    $(REG_CORE_OBJ_DIR)/reg_priv_objs.o \
		    $(REG_DISPATCHER_OBJ_DIR)/wlan_reg_services_api.o \
		    $(REG_CORE_OBJ_DIR)/reg_services_common.o \
		    $(REG_DISPATCHER_OBJ_DIR)/wlan_reg_tgt_api.o \
		    $(REG_DISPATCHER_OBJ_DIR)/wlan_reg_ucfg_api.o
ifeq ($(CONFIG_HOST_11D_SCAN), y)
REGULATORY_OBJS += $(REG_CORE_OBJ_DIR)/reg_host_11d.o
endif

$(call add-wlan-objs,regulatory,$(REGULATORY_OBJS))

############## Control path common scheduler ##########
SCHEDULER_DIR := scheduler
SCHEDULER_INC_DIR := $(SCHEDULER_DIR)/inc
SCHEDULER_SRC_DIR := $(SCHEDULER_DIR)/src
SCHEDULER_OBJ_DIR := $(WLAN_COMMON_ROOT)/$(SCHEDULER_SRC_DIR)
SCHEDULER_INC := -I$(WLAN_COMMON_INC)/$(SCHEDULER_INC_DIR)
SCHEDULER_OBJS := $(SCHEDULER_OBJ_DIR)/scheduler_api.o \
                  $(SCHEDULER_OBJ_DIR)/scheduler_core.o

$(call add-wlan-objs,scheduler,$(SCHEDULER_OBJS))

###### UMAC SERIALIZATION ########
UMAC_SER_DIR := umac/cmn_services/serialization
UMAC_SER_INC_DIR := $(UMAC_SER_DIR)/inc
UMAC_SER_SRC_DIR := $(UMAC_SER_DIR)/src
UMAC_SER_OBJ_DIR := $(WLAN_COMMON_ROOT)/$(UMAC_SER_SRC_DIR)

UMAC_SER_INC := -I$(WLAN_COMMON_INC)/$(UMAC_SER_INC_DIR)
UMAC_SER_OBJS := $(UMAC_SER_OBJ_DIR)/wlan_serialization_main.o \
		 $(UMAC_SER_OBJ_DIR)/wlan_serialization_api.o \
		 $(UMAC_SER_OBJ_DIR)/wlan_serialization_utils.o \
		 $(UMAC_SER_OBJ_DIR)/wlan_serialization_legacy_api.o \
		 $(UMAC_SER_OBJ_DIR)/wlan_serialization_rules.o \
		 $(UMAC_SER_OBJ_DIR)/wlan_serialization_internal.o \
		 $(UMAC_SER_OBJ_DIR)/wlan_serialization_non_scan.o \
		 $(UMAC_SER_OBJ_DIR)/wlan_serialization_queue.o \
		 $(UMAC_SER_OBJ_DIR)/wlan_serialization_scan.o

$(call add-wlan-objs,umac_ser,$(UMAC_SER_OBJS))

###### WIFI POS ########
WIFI_POS_OS_IF_DIR := $(WLAN_COMMON_ROOT)/os_if/linux/wifi_pos/src
WIFI_POS_OS_IF_INC := -I$(WLAN_COMMON_INC)/os_if/linux/wifi_pos/inc
WIFI_POS_TGT_DIR := $(WLAN_COMMON_ROOT)/target_if/wifi_pos/src
WIFI_POS_TGT_INC := -I$(WLAN_COMMON_INC)/target_if/wifi_pos/inc
WIFI_POS_CORE_DIR := $(WLAN_COMMON_ROOT)/umac/wifi_pos/src
WIFI_POS_API_INC := -I$(WLAN_COMMON_INC)/umac/wifi_pos/inc


ifeq ($(CONFIG_WIFI_POS_CONVERGED), y)
WIFI_POS_OBJS := $(WIFI_POS_CORE_DIR)/wifi_pos_api.o \
		 $(WIFI_POS_CORE_DIR)/wifi_pos_main.o \
		 $(WIFI_POS_CORE_DIR)/wifi_pos_ucfg.o \
		 $(WIFI_POS_CORE_DIR)/wifi_pos_utils.o \
		 $(WIFI_POS_OS_IF_DIR)/os_if_wifi_pos.o \
		 $(WIFI_POS_TGT_DIR)/target_if_wifi_pos.o
endif

$(call add-wlan-objs,wifi_pos,$(WIFI_POS_OBJS))

###### CP STATS ########
CP_MC_STATS_OS_IF_SRC           := os_if/cp_stats/src
CP_STATS_TGT_SRC                := $(WLAN_COMMON_ROOT)/target_if/cp_stats/src
CP_STATS_CORE_SRC               := $(WLAN_COMMON_ROOT)/umac/cp_stats/core/src
CP_STATS_DISPATCHER_SRC         := $(WLAN_COMMON_ROOT)/umac/cp_stats/dispatcher/src
CP_MC_STATS_COMPONENT_SRC       := components/cp_stats/dispatcher/src
CP_MC_STATS_COMPONENT_TGT_SRC   := $(CLD_TARGET_IF_DIR)/cp_stats/src

CP_STATS_OS_IF_INC              := -I$(WLAN_COMMON_INC)/os_if/linux/cp_stats/inc
CP_STATS_TGT_INC                := -I$(WLAN_COMMON_INC)/target_if/cp_stats/inc
CP_STATS_DISPATCHER_INC         := -I$(WLAN_COMMON_INC)/umac/cp_stats/dispatcher/inc
CP_MC_STATS_COMPONENT_INC       := -I$(WLAN_ROOT)/components/cp_stats/dispatcher/inc

ifeq ($(CONFIG_CP_STATS), y)
CP_STATS_OBJS := $(CP_MC_STATS_COMPONENT_SRC)/wlan_cp_stats_mc_tgt_api.o	\
		 $(CP_MC_STATS_COMPONENT_SRC)/wlan_cp_stats_mc_ucfg_api.o	\
		 $(CP_MC_STATS_COMPONENT_TGT_SRC)/target_if_mc_cp_stats.o	\
		 $(CP_STATS_CORE_SRC)/wlan_cp_stats_comp_handler.o		\
		 $(CP_STATS_CORE_SRC)/wlan_cp_stats_obj_mgr_handler.o		\
		 $(CP_STATS_CORE_SRC)/wlan_cp_stats_ol_api.o			\
		 $(CP_MC_STATS_OS_IF_SRC)/wlan_cfg80211_mc_cp_stats.o		\
		 $(CP_STATS_DISPATCHER_SRC)/wlan_cp_stats_utils_api.o		\
		 $(WLAN_COMMON_ROOT)/target_if/cp_stats/src/target_if_cp_stats.o	\
		 $(CP_STATS_DISPATCHER_SRC)/wlan_cp_stats_ucfg_api.o
endif

$(call add-wlan-objs,cp_stats,$(CP_STATS_OBJS))

###### DCS ######
DCS_TGT_IF_SRC := $(WLAN_COMMON_ROOT)/target_if/dcs/src
DCS_CORE_SRC   := $(WLAN_COMMON_ROOT)/umac/dcs/core/src
DCS_DISP_SRC   := $(WLAN_COMMON_ROOT)/umac/dcs/dispatcher/src

DCS_TGT_IF_INC := -I$(WLAN_COMMON_INC)/target_if/dcs/inc
DCS_DISP_INC   := -I$(WLAN_COMMON_INC)/umac/dcs/dispatcher/inc

ifeq ($(CONFIG_DCS), y)
DCS_OBJS := $(DCS_TGT_IF_SRC)/target_if_dcs.o \
	$(DCS_CORE_SRC)/wlan_dcs.o \
	$(DCS_DISP_SRC)/wlan_dcs_init_deinit_api.o \
	$(DCS_DISP_SRC)/wlan_dcs_ucfg_api.o \
	$(DCS_DISP_SRC)/wlan_dcs_tgt_api.o
endif

$(call add-wlan-objs,dcs,$(DCS_OBJS))

###### INTEROP ISSUES AP ########
INTEROP_ISSUES_AP_OS_IF_SRC      := os_if/interop_issues_ap/src
INTEROP_ISSUES_AP_TGT_SRC        := components/target_if/interop_issues_ap/src
INTEROP_ISSUES_AP_CORE_SRC       := components/interop_issues_ap/core/src
INTEROP_ISSUES_AP_DISPATCHER_SRC := components/interop_issues_ap/dispatcher/src

INTEROP_ISSUES_AP_OS_IF_INC      := -I$(WLAN_ROOT)/os_if/interop_issues_ap/inc
INTEROP_ISSUES_AP_TGT_INC        := -I$(WLAN_ROOT)/components/target_if/interop_issues_ap/inc
INTEROP_ISSUES_AP_DISPATCHER_INC := -I$(WLAN_ROOT)/components/interop_issues_ap/dispatcher/inc
INTEROP_ISSUES_AP_CORE_INC       := -I$(WLAN_ROOT)/components/interop_issues_ap/core/inc

ifeq ($(CONFIG_FEATURE_INTEROP_ISSUES_AP), y)
INTEROP_ISSUES_AP_OBJS := $(INTEROP_ISSUES_AP_TGT_SRC)/target_if_interop_issues_ap.o \
		$(INTEROP_ISSUES_AP_CORE_SRC)/wlan_interop_issues_ap_api.o \
		$(INTEROP_ISSUES_AP_OS_IF_SRC)/wlan_cfg80211_interop_issues_ap.o \
		$(INTEROP_ISSUES_AP_DISPATCHER_SRC)/wlan_interop_issues_ap_tgt_api.o \
		$(INTEROP_ISSUES_AP_DISPATCHER_SRC)/wlan_interop_issues_ap_ucfg_api.o
endif

$(call add-wlan-objs,interop_issues_ap,$(INTEROP_ISSUES_AP_OBJS))

######################### NAN #########################
NAN_CORE_DIR := components/nan/core/src
NAN_CORE_INC := -I$(WLAN_ROOT)/components/nan/core/inc
NAN_UCFG_DIR := components/nan/dispatcher/src
NAN_UCFG_INC := -I$(WLAN_ROOT)/components/nan/dispatcher/inc
NAN_TGT_DIR  := components/target_if/nan/src
NAN_TGT_INC  := -I$(WLAN_ROOT)/components/target_if/nan/inc

NAN_OS_IF_DIR  := os_if/nan/src
NAN_OS_IF_INC  := -I$(WLAN_ROOT)/os_if/nan/inc

ifeq ($(CONFIG_QCACLD_FEATURE_NAN), y)
WLAN_NAN_OBJS := $(NAN_CORE_DIR)/nan_main.o \
		 $(NAN_CORE_DIR)/nan_api.o \
		 $(NAN_UCFG_DIR)/nan_ucfg_api.o \
		 $(NAN_UCFG_DIR)/cfg_nan.o \
		 $(NAN_TGT_DIR)/target_if_nan.o \
		 $(NAN_OS_IF_DIR)/os_if_nan.o
endif

$(call add-wlan-objs,nan,$(WLAN_NAN_OBJS))

#######################################################

###### COEX ########
COEX_OS_IF_SRC      := $(WLAN_COMMON_ROOT)/os_if/linux/coex/src
COEX_TGT_SRC        := $(WLAN_COMMON_ROOT)/target_if/coex/src
COEX_CORE_SRC       := $(WLAN_COMMON_ROOT)/umac/coex/core/src
COEX_DISPATCHER_SRC := $(WLAN_COMMON_ROOT)/umac/coex/dispatcher/src

COEX_OS_IF_INC      := -I$(WLAN_COMMON_INC)/os_if/linux/coex/inc
COEX_TGT_INC        := -I$(WLAN_COMMON_INC)/target_if/coex/inc
COEX_DISPATCHER_INC := -I$(WLAN_COMMON_INC)/umac/coex/dispatcher/inc
COEX_CORE_INC       := -I$(WLAN_COMMON_INC)/umac/coex/core/inc

ifeq ($(CONFIG_FEATURE_COEX), y)
COEX_OBJS := $(COEX_TGT_SRC)/target_if_coex.o                 \
		 $(COEX_CORE_SRC)/wlan_coex_main.o                 \
		 $(COEX_OS_IF_SRC)/wlan_cfg80211_coex.o           \
		 $(COEX_DISPATCHER_SRC)/wlan_coex_tgt_api.o       \
		 $(COEX_DISPATCHER_SRC)/wlan_coex_utils_api.o       \
		 $(COEX_DISPATCHER_SRC)/wlan_coex_ucfg_api.o
endif

$(call add-wlan-objs,coex,$(COEX_OBJS))

############## HTC ##########
HTC_DIR := htc
HTC_INC := -I$(WLAN_COMMON_INC)/$(HTC_DIR)

HTC_OBJS := $(WLAN_COMMON_ROOT)/$(HTC_DIR)/htc.o \
            $(WLAN_COMMON_ROOT)/$(HTC_DIR)/htc_send.o \
            $(WLAN_COMMON_ROOT)/$(HTC_DIR)/htc_recv.o \
            $(WLAN_COMMON_ROOT)/$(HTC_DIR)/htc_services.o

ifeq ($(CONFIG_FEATURE_HTC_CREDIT_HISTORY), y)
HTC_OBJS += $(WLAN_COMMON_ROOT)/$(HTC_DIR)/htc_credit_history.o
endif

ifeq ($(CONFIG_WLAN_HANG_EVENT), y)
HTC_OBJS += $(WLAN_COMMON_ROOT)/$(HTC_DIR)/htc_hang_event.o
endif

$(call add-wlan-objs,htc,$(HTC_OBJS))

########### HIF ###########
HIF_DIR := hif
HIF_CE_DIR := $(HIF_DIR)/src/ce

HIF_DISPATCHER_DIR := $(HIF_DIR)/src/dispatcher

HIF_PCIE_DIR := $(HIF_DIR)/src/pcie
HIF_IPCIE_DIR := $(HIF_DIR)/src/ipcie
HIF_SNOC_DIR := $(HIF_DIR)/src/snoc
HIF_USB_DIR := $(HIF_DIR)/src/usb
HIF_SDIO_DIR := $(HIF_DIR)/src/sdio

HIF_SDIO_NATIVE_DIR := $(HIF_SDIO_DIR)/native_sdio
HIF_SDIO_NATIVE_INC_DIR := $(HIF_SDIO_NATIVE_DIR)/include
HIF_SDIO_NATIVE_SRC_DIR := $(HIF_SDIO_NATIVE_DIR)/src

HIF_INC := -I$(WLAN_COMMON_INC)/$(HIF_DIR)/inc \
	   -I$(WLAN_COMMON_INC)/$(HIF_DIR)/src

ifeq ($(CONFIG_HIF_PCI), y)
HIF_INC += -I$(WLAN_COMMON_INC)/$(HIF_DISPATCHER_DIR)
HIF_INC += -I$(WLAN_COMMON_INC)/$(HIF_PCIE_DIR)
HIF_INC += -I$(WLAN_COMMON_INC)/$(HIF_CE_DIR)
endif

ifeq ($(CONFIG_HIF_IPCI), y)
HIF_INC += -I$(WLAN_COMMON_INC)/$(HIF_DISPATCHER_DIR)
HIF_INC += -I$(WLAN_COMMON_INC)/$(HIF_IPCIE_DIR)
HIF_INC += -I$(WLAN_COMMON_INC)/$(HIF_CE_DIR)
endif

ifeq ($(CONFIG_HIF_SNOC), y)
HIF_INC += -I$(WLAN_COMMON_INC)/$(HIF_DISPATCHER_DIR)
HIF_INC += -I$(WLAN_COMMON_INC)/$(HIF_SNOC_DIR)
HIF_INC += -I$(WLAN_COMMON_INC)/$(HIF_CE_DIR)
endif

ifeq ($(CONFIG_HIF_USB), y)
HIF_INC += -I$(WLAN_COMMON_INC)/$(HIF_DISPATCHER_DIR)
HIF_INC += -I$(WLAN_COMMON_INC)/$(HIF_USB_DIR)
HIF_INC += -I$(WLAN_COMMON_INC)/$(HIF_CE_DIR)
endif

ifeq ($(CONFIG_HIF_SDIO), y)
HIF_INC += -I$(WLAN_COMMON_INC)/$(HIF_DISPATCHER_DIR)
HIF_INC += -I$(WLAN_COMMON_INC)/$(HIF_SDIO_DIR)
HIF_INC += -I$(WLAN_COMMON_INC)/$(HIF_SDIO_NATIVE_INC_DIR)
HIF_INC += -I$(WLAN_COMMON_INC)/$(HIF_CE_DIR)
endif

HIF_COMMON_OBJS := $(WLAN_COMMON_ROOT)/$(HIF_DIR)/src/ath_procfs.o \
		   $(WLAN_COMMON_ROOT)/$(HIF_DIR)/src/hif_main.o \
		   $(WLAN_COMMON_ROOT)/$(HIF_DIR)/src/hif_runtime_pm.o \
		   $(WLAN_COMMON_ROOT)/$(HIF_DIR)/src/hif_exec.o

ifneq ($(CONFIG_LITHIUM), y)
HIF_COMMON_OBJS += $(WLAN_COMMON_ROOT)/$(HIF_DIR)/src/hif_main_legacy.o
endif

ifeq ($(CONFIG_WLAN_NAPI), y)
HIF_COMMON_OBJS += $(WLAN_COMMON_ROOT)/$(HIF_DIR)/src/hif_irq_affinity.o
endif

HIF_CE_OBJS :=  $(WLAN_COMMON_ROOT)/$(HIF_CE_DIR)/ce_diag.o \
                $(WLAN_COMMON_ROOT)/$(HIF_CE_DIR)/ce_main.o \
                $(WLAN_COMMON_ROOT)/$(HIF_CE_DIR)/ce_service.o \
                $(WLAN_COMMON_ROOT)/$(HIF_CE_DIR)/ce_tasklet.o \
                $(WLAN_COMMON_ROOT)/$(HIF_DIR)/src/mp_dev.o \
                $(WLAN_COMMON_ROOT)/$(HIF_DIR)/src/regtable.o

ifeq ($(CONFIG_WLAN_FEATURE_BMI), y)
HIF_CE_OBJS +=  $(WLAN_COMMON_ROOT)/$(HIF_CE_DIR)/ce_bmi.o
endif

ifeq ($(CONFIG_LITHIUM), y)
ifeq ($(CONFIG_CNSS_QCA6290), y)
HIF_CE_OBJS +=  $(WLAN_COMMON_ROOT)/$(HIF_DIR)/src/qca6290def.o
endif

ifeq ($(CONFIG_CNSS_QCA6390), y)
HIF_CE_OBJS +=  $(WLAN_COMMON_ROOT)/$(HIF_DIR)/src/qca6390def.o
endif

ifeq ($(CONFIG_CNSS_QCA6490), y)
HIF_CE_OBJS +=  $(WLAN_COMMON_ROOT)/$(HIF_DIR)/src/qca6490def.o
endif

ifeq ($(CONFIG_CNSS_QCA6750), y)
HIF_CE_OBJS +=  $(WLAN_COMMON_ROOT)/$(HIF_DIR)/src/qca6750def.o
endif

HIF_CE_OBJS +=  $(WLAN_COMMON_ROOT)/$(HIF_CE_DIR)/ce_service_srng.o
else
HIF_CE_OBJS +=  $(WLAN_COMMON_ROOT)/$(HIF_CE_DIR)/ce_service_legacy.o
endif

HIF_USB_OBJS := $(WLAN_COMMON_ROOT)/$(HIF_USB_DIR)/usbdrv.o \
                $(WLAN_COMMON_ROOT)/$(HIF_USB_DIR)/hif_usb.o \
                $(WLAN_COMMON_ROOT)/$(HIF_USB_DIR)/if_usb.o \
                $(WLAN_COMMON_ROOT)/$(HIF_USB_DIR)/regtable_usb.o

HIF_SDIO_OBJS := $(WLAN_COMMON_ROOT)/$(HIF_SDIO_DIR)/hif_diag_reg_access.o \
                 $(WLAN_COMMON_ROOT)/$(HIF_SDIO_DIR)/hif_sdio_dev.o \
                 $(WLAN_COMMON_ROOT)/$(HIF_SDIO_DIR)/hif_sdio.o \
                 $(WLAN_COMMON_ROOT)/$(HIF_SDIO_DIR)/regtable_sdio.o \
                 $(WLAN_COMMON_ROOT)/$(HIF_SDIO_DIR)/transfer/transfer.o

ifeq ($(CONFIG_WLAN_FEATURE_BMI), y)
HIF_SDIO_OBJS += $(WLAN_COMMON_ROOT)/$(HIF_SDIO_DIR)/hif_bmi_reg_access.o
endif

ifeq ($(CONFIG_SDIO_TRANSFER), adma)
HIF_SDIO_OBJS += $(WLAN_COMMON_ROOT)/$(HIF_SDIO_DIR)/transfer/adma.o
else
HIF_SDIO_OBJS += $(WLAN_COMMON_ROOT)/$(HIF_SDIO_DIR)/transfer/mailbox.o
endif

HIF_SDIO_NATIVE_OBJS := $(WLAN_COMMON_ROOT)/$(HIF_SDIO_NATIVE_SRC_DIR)/hif.o \
                        $(WLAN_COMMON_ROOT)/$(HIF_SDIO_NATIVE_SRC_DIR)/hif_scatter.o \
                        $(WLAN_COMMON_ROOT)/$(HIF_SDIO_NATIVE_SRC_DIR)/dev_quirks.o

ifeq ($(CONFIG_WLAN_NAPI), y)
HIF_OBJS += $(WLAN_COMMON_ROOT)/$(HIF_DIR)/src/hif_napi.o
endif

ifeq ($(CONFIG_FEATURE_UNIT_TEST_SUSPEND), y)
	HIF_OBJS += $(WLAN_COMMON_ROOT)/$(HIF_DIR)/src/hif_unit_test_suspend.o
endif

HIF_PCIE_OBJS := $(WLAN_COMMON_ROOT)/$(HIF_PCIE_DIR)/if_pci.o
HIF_IPCIE_OBJS := $(WLAN_COMMON_ROOT)/$(HIF_IPCIE_DIR)/if_ipci.o
HIF_SNOC_OBJS := $(WLAN_COMMON_ROOT)/$(HIF_SNOC_DIR)/if_snoc.o
HIF_SDIO_OBJS += $(WLAN_COMMON_ROOT)/$(HIF_SDIO_DIR)/if_sdio.o

HIF_OBJS += $(WLAN_COMMON_ROOT)/$(HIF_DISPATCHER_DIR)/multibus.o
HIF_OBJS += $(WLAN_COMMON_ROOT)/$(HIF_DISPATCHER_DIR)/dummy.o
HIF_OBJS += $(HIF_COMMON_OBJS)

ifeq ($(CONFIG_HIF_PCI), y)
HIF_OBJS += $(HIF_PCIE_OBJS)
HIF_OBJS += $(HIF_CE_OBJS)
HIF_OBJS += $(WLAN_COMMON_ROOT)/$(HIF_DISPATCHER_DIR)/multibus_pci.o
endif

ifeq ($(CONFIG_HIF_IPCI), y)
HIF_OBJS += $(HIF_IPCIE_OBJS)
HIF_OBJS += $(HIF_CE_OBJS)
HIF_OBJS += $(WLAN_COMMON_ROOT)/$(HIF_DISPATCHER_DIR)/multibus_ipci.o
endif

ifeq ($(CONFIG_HIF_SNOC), y)
HIF_OBJS += $(HIF_SNOC_OBJS)
HIF_OBJS += $(HIF_CE_OBJS)
HIF_OBJS += $(WLAN_COMMON_ROOT)/$(HIF_DISPATCHER_DIR)/multibus_snoc.o
endif

ifeq ($(CONFIG_HIF_SDIO), y)
HIF_OBJS += $(HIF_SDIO_OBJS)
HIF_OBJS += $(HIF_SDIO_NATIVE_OBJS)
HIF_OBJS += $(WLAN_COMMON_ROOT)/$(HIF_DISPATCHER_DIR)/multibus_sdio.o
endif

ifeq ($(CONFIG_HIF_USB), y)
HIF_OBJS += $(HIF_USB_OBJS)
HIF_OBJS += $(WLAN_COMMON_ROOT)/$(HIF_DISPATCHER_DIR)/multibus_usb.o
endif

$(call add-wlan-objs,hif,$(HIF_OBJS))

ifeq ($(CONFIG_LITHIUM), y)
############ HAL ############
HAL_DIR :=	hal
HAL_INC :=	-I$(WLAN_COMMON_INC)/$(HAL_DIR)/inc \
		-I$(WLAN_COMMON_INC)/$(HAL_DIR)/wifi3.0

HAL_OBJS :=	$(WLAN_COMMON_ROOT)/$(HAL_DIR)/wifi3.0/hal_srng.o \
		$(WLAN_COMMON_ROOT)/$(HAL_DIR)/wifi3.0/hal_reo.o

ifeq ($(CONFIG_RX_FISA), y)
HAL_OBJS += $(WLAN_COMMON_ROOT)/$(HAL_DIR)/wifi3.0/hal_rx_flow.o
endif

ifeq ($(CONFIG_CNSS_QCA6290), y)
HAL_INC += -I$(WLAN_COMMON_INC)/$(HAL_DIR)/wifi3.0/qca6290
HAL_OBJS += $(WLAN_COMMON_ROOT)/$(HAL_DIR)/wifi3.0/qca6290/hal_6290.o
else ifeq ($(CONFIG_CNSS_QCA6390), y)
HAL_INC += -I$(WLAN_COMMON_INC)/$(HAL_DIR)/wifi3.0/qca6390
HAL_OBJS += $(WLAN_COMMON_ROOT)/$(HAL_DIR)/wifi3.0/qca6390/hal_6390.o
else ifeq ($(CONFIG_CNSS_QCA6490), y)
HAL_INC += -I$(WLAN_COMMON_INC)/$(HAL_DIR)/wifi3.0/qca6490
HAL_OBJS += $(WLAN_COMMON_ROOT)/$(HAL_DIR)/wifi3.0/qca6490/hal_6490.o
else ifeq ($(CONFIG_CNSS_QCA6750), y)
HAL_INC += -I$(WLAN_COMMON_INC)/$(HAL_DIR)/wifi3.0/qca6750
HAL_OBJS += $(WLAN_COMMON_ROOT)/$(HAL_DIR)/wifi3.0/qca6750/hal_6750.o
else
#error "Not 11ax"
endif

endif #####CONFIG_LITHIUM####

$(call add-wlan-objs,hal,$(HAL_OBJS))

############ WMA ############
WMA_DIR :=	core/wma

WMA_INC_DIR :=  $(WMA_DIR)/inc
WMA_SRC_DIR :=  $(WMA_DIR)/src

WMA_INC :=	-I$(WLAN_ROOT)/$(WMA_INC_DIR) \
		-I$(WLAN_ROOT)/$(WMA_SRC_DIR)

ifeq ($(CONFIG_QCACLD_FEATURE_NAN), y)
WMA_NDP_OBJS += $(WMA_SRC_DIR)/wma_nan_datapath.o
endif

WMA_OBJS :=	$(WMA_SRC_DIR)/wma_main.o \
		$(WMA_SRC_DIR)/wma_scan_roam.o \
		$(WMA_SRC_DIR)/wma_dev_if.o \
		$(WMA_SRC_DIR)/wma_mgmt.o \
		$(WMA_SRC_DIR)/wma_power.o \
		$(WMA_SRC_DIR)/wma_data.o \
		$(WMA_SRC_DIR)/wma_utils.o \
		$(WMA_SRC_DIR)/wma_features.o \
		$(WMA_SRC_DIR)/wlan_qct_wma_legacy.o\
		$(WMA_NDP_OBJS)

ifeq ($(CONFIG_WLAN_FEATURE_DSRC), y)
WMA_OBJS+=	$(WMA_SRC_DIR)/wma_ocb.o
endif
ifeq ($(CONFIG_WLAN_FEATURE_FIPS), y)
WMA_OBJS+=	$(WMA_SRC_DIR)/wma_fips_api.o
endif
ifeq ($(CONFIG_MPC_UT_FRAMEWORK), y)
WMA_OBJS +=	$(WMA_SRC_DIR)/wma_utils_ut.o
endif
ifeq ($(CONFIG_WLAN_FEATURE_11AX), y)
WMA_OBJS+=	$(WMA_SRC_DIR)/wma_he.o
endif
ifeq ($(CONFIG_WLAN_FEATURE_TWT), y)
WMA_OBJS +=	$(WMA_SRC_DIR)/wma_twt.o
endif
ifeq ($(CONFIG_QCACLD_FEATURE_FW_STATE), y)
WMA_OBJS +=	$(WMA_SRC_DIR)/wma_fw_state.o
endif
ifeq ($(CONFIG_WLAN_MWS_INFO_DEBUGFS), y)
WMA_OBJS +=	$(WMA_SRC_DIR)/wma_coex.o
endif

$(call add-wlan-objs,wma,$(WMA_OBJS))

#######DIRECT_BUFFER_RX#########
ifeq ($(CONFIG_DIRECT_BUF_RX_ENABLE), y)
DBR_DIR = $(WLAN_COMMON_ROOT)/target_if/direct_buf_rx
UMAC_DBR_INC := -I$(WLAN_COMMON_INC)/target_if/direct_buf_tx/inc
UMAC_DBR_OBJS := $(DBR_DIR)/src/target_if_direct_buf_rx_api.o \
		 $(DBR_DIR)/src/target_if_direct_buf_rx_main.o \
		 $(WLAN_COMMON_ROOT)/wmi/src/wmi_unified_dbr_api.o \
		 $(WLAN_COMMON_ROOT)/wmi/src/wmi_unified_dbr_tlv.o
endif

$(call add-wlan-objs,umac_dbr,$(UMAC_DBR_OBJS))

############## PLD ##########
PLD_DIR := core/pld
PLD_INC_DIR := $(PLD_DIR)/inc
PLD_SRC_DIR := $(PLD_DIR)/src

PLD_INC :=	-I$(WLAN_ROOT)/$(PLD_INC_DIR) \
		-I$(WLAN_ROOT)/$(PLD_SRC_DIR)

PLD_OBJS :=	$(PLD_SRC_DIR)/pld_common.o

ifeq ($(CONFIG_IPCIE_FW_SIM), y)
PLD_OBJS +=     $(PLD_SRC_DIR)/pld_pcie_fw_sim.o
endif
ifeq ($(CONFIG_PCIE_FW_SIM), y)
PLD_OBJS +=     $(PLD_SRC_DIR)/pld_pcie_fw_sim.o
else ifeq ($(CONFIG_HIF_PCI), y)
PLD_OBJS +=     $(PLD_SRC_DIR)/pld_pcie.o
endif
ifeq ($(CONFIG_SNOC_FW_SIM),y)
PLD_OBJS +=     $(PLD_SRC_DIR)/pld_snoc_fw_sim.o
else ifeq (y,$(findstring y, $(CONFIG_ICNSS) $(CONFIG_PLD_SNOC_ICNSS_FLAG)))
PLD_OBJS +=     $(PLD_SRC_DIR)/pld_snoc.o
else ifeq (y,$(findstring y, $(CONFIG_PLD_IPCI_ICNSS_FLAG)))
PLD_OBJS +=     $(PLD_SRC_DIR)/pld_ipci.o
endif

ifeq ($(CONFIG_QCA_WIFI_SDIO), y)
PLD_OBJS +=	$(PLD_SRC_DIR)/pld_sdio.o
endif
ifeq ($(CONFIG_HIF_USB), y)
PLD_OBJS +=	$(PLD_SRC_DIR)/pld_usb.o
endif

$(call add-wlan-objs,pld,$(PLD_OBJS))


TARGET_INC := 	-I$(WLAN_FW_API)/fw

ifeq ($(CONFIG_CNSS_QCA6290), y)
ifeq ($(CONFIG_QCA6290_11AX), y)
TARGET_INC +=	-I$(WLAN_FW_API)/hw/qca6290/11ax/v2
else
TARGET_INC +=	-I$(WLAN_FW_API)/hw/qca6290/v2
endif
endif

ifeq ($(CONFIG_CNSS_QCA6390), y)
TARGET_INC +=	-I$(WLAN_FW_API)/hw/qca6390/v1
endif

ifeq ($(CONFIG_CNSS_QCA6490), y)
TARGET_INC +=	-I$(WLAN_FW_API)/hw/qca6490/v1
endif

ifeq ($(CONFIG_CNSS_QCA6750), y)
TARGET_INC +=	-I$(WLAN_FW_API)/hw/qca6750/v1
endif

LINUX_INC :=	-Iinclude

INCS :=		$(HDD_INC) \
		$(SYNC_INC) \
		$(DSC_INC) \
		$(EPPING_INC) \
		$(LINUX_INC) \
		$(MAC_INC) \
		$(SAP_INC) \
		$(SME_INC) \
		$(SYS_INC) \
		$(CLD_WMI_INC) \
		$(QAL_INC) \
		$(QDF_INC) \
		$(WBUFF_INC) \
		$(CDS_INC) \
		$(CFG_INC) \
		$(DFS_INC) \
		$(TARGET_IF_INC) \
		$(CLD_TARGET_IF_INC) \
		$(OS_IF_INC) \
		$(GLOBAL_LMAC_IF_INC) \
		$(FTM_INC)

INCS +=		$(WMA_INC) \
		$(UAPI_INC) \
		$(COMMON_INC) \
		$(WMI_INC) \
		$(FWLOG_INC) \
		$(TXRX_INC) \
		$(OL_INC) \
		$(CDP_INC) \
		$(PKTLOG_INC) \
		$(HTT_INC) \
		$(INIT_DEINIT_INC) \
		$(SCHEDULER_INC) \
		$(REGULATORY_INC) \
		$(HTC_INC) \
		$(DFS_INC) \
		$(WCFG_INC) \
		$(TXRX3.0_INC)

INCS +=		$(HIF_INC) \
		$(BMI_INC) \
		$(CMN_SYS_INC)

ifeq ($(CONFIG_LITHIUM), y)
INCS += 	$(HAL_INC) \
		$(DP_INC)
endif

################ WIFI POS ################
INCS +=		$(WIFI_POS_API_INC)
INCS +=		$(WIFI_POS_TGT_INC)
INCS +=		$(WIFI_POS_OS_IF_INC)
################ CP STATS ################
INCS +=		$(CP_STATS_OS_IF_INC)
INCS +=		$(CP_STATS_TGT_INC)
INCS +=		$(CP_STATS_DISPATCHER_INC)
INCS +=		$(CP_MC_STATS_COMPONENT_INC)
################ Dynamic ACS ####################
INCS +=		$(DCS_TGT_IF_INC)
INCS +=		$(DCS_DISP_INC)
################ INTEROP ISSUES AP ################
INCS +=		$(INTEROP_ISSUES_AP_OS_IF_INC)
INCS +=		$(INTEROP_ISSUES_AP_TGT_INC)
INCS +=		$(INTEROP_ISSUES_AP_DISPATCHER_INC)
INCS +=		$(INTEROP_ISSUES_AP_CORE_INC)
################ NAN POS ################
INCS +=		$(NAN_CORE_INC)
INCS +=		$(NAN_UCFG_INC)
INCS +=		$(NAN_TGT_INC)
INCS +=		$(NAN_OS_IF_INC)
##########################################
INCS +=		$(UMAC_OBJMGR_INC)
INCS +=		$(UMAC_MGMT_TXRX_INC)
INCS +=		$(PMO_INC)
INCS +=		$(P2P_INC)
INCS +=		$(POLICY_MGR_INC)
INCS +=		$(TARGET_INC)
INCS +=		$(TDLS_INC)
INCS +=		$(UMAC_SER_INC)
INCS +=		$(NLINK_INC) \
		$(PTT_INC) \
		$(WLAN_LOGGING_INC)

INCS +=		$(PLD_INC)
INCS +=		$(OCB_INC)

INCS +=		$(IPA_INC)
INCS +=		$(UMAC_SM_INC)
INCS +=		$(UMAC_MLME_INC)
INCS +=		$(MLME_INC)
INCS +=		$(FWOL_INC)
INCS +=		$(BLM_INC)

ifeq ($(CONFIG_REMOVE_PKT_LOG), n)
INCS +=		$(PKTLOG_INC)
endif

INCS +=		$(HOST_DIAG_LOG_INC)

INCS +=		$(DISA_INC)
INCS +=		$(ACTION_OUI_INC)
INCS +=		$(PKT_CAPTURE_INC)
INCS +=		$(FTM_TIME_SYNC_INC)

INCS +=		$(UMAC_DISP_INC)
INCS +=		$(UMAC_SCAN_INC)
INCS +=		$(UMAC_TARGET_SCAN_INC)
INCS +=		$(UMAC_GREEN_AP_INC)
INCS +=		$(UMAC_TARGET_GREEN_AP_INC)
INCS +=		$(UMAC_COMMON_INC)
INCS +=		$(UMAC_SPECTRAL_INC)
INCS +=		$(WLAN_CFR_INC)
INCS +=		$(UMAC_TARGET_SPECTRAL_INC)
INCS +=		$(UMAC_DBR_INC)
INCS +=		$(UMAC_CRYPTO_INC)
ifeq ($(CONFIG_INTERFACE_MGR), y)
INCS +=		$(UMAC_INTERFACE_MGR_INC)
endif
INCS +=		$(COEX_OS_IF_INC)
INCS +=		$(COEX_TGT_INC)
INCS +=		$(COEX_DISPATCHER_INC)
INCS +=		$(COEX_CORE_INC)

ccflags-y += $(INCS)

cppflags-y +=	-DANI_OS_TYPE_ANDROID=6 \
		-Wall\
		-Werror\
		-D__linux__

cppflags-$(CONFIG_THERMAL_STATS_SUPPORT) += -DTHERMAL_STATS_SUPPORT
cppflags-$(CONFIG_PTT_SOCK_SVC_ENABLE) += -DPTT_SOCK_SVC_ENABLE
cppflags-$(CONFIG_FEATURE_WLAN_WAPI) += -DFEATURE_WLAN_WAPI
cppflags-$(CONFIG_FEATURE_WLAN_WAPI) += -DATH_SUPPORT_WAPI
cppflags-$(CONFIG_AGEIE_ON_SCAN_RESULTS) += -DWLAN_ENABLE_AGEIE_ON_SCAN_RESULTS
cppflags-$(CONFIG_SOFTAP_CHANNEL_RANGE) += -DSOFTAP_CHANNEL_RANGE
cppflags-$(CONFIG_FEATURE_WLAN_SCAN_PNO) += -DFEATURE_WLAN_SCAN_PNO
cppflags-$(CONFIG_WLAN_FEATURE_PACKET_FILTERING) += -DWLAN_FEATURE_PACKET_FILTERING
cppflags-$(CONFIG_DHCP_SERVER_OFFLOAD) += -DDHCP_SERVER_OFFLOAD
cppflags-$(CONFIG_WLAN_NS_OFFLOAD) += -DWLAN_NS_OFFLOAD
cppflags-$(CONFIG_FEATURE_WLAN_RA_FILTERING) += -DFEATURE_WLAN_RA_FILTERING
cppflags-$(CONFIG_FEATURE_WLAN_LPHB) += -DFEATURE_WLAN_LPHB
cppflags-$(CONFIG_QCA_SUPPORT_TX_THROTTLE) += -DQCA_SUPPORT_TX_THROTTLE
cppflags-$(CONFIG_WMI_INTERFACE_EVENT_LOGGING) += -DWMI_INTERFACE_EVENT_LOGGING
cppflags-$(CONFIG_WLAN_FEATURE_LINK_LAYER_STATS) += -DWLAN_FEATURE_LINK_LAYER_STATS
cppflags-$(CONFIG_FEATURE_CLUB_LL_STATS_AND_GET_STATION) += -DFEATURE_CLUB_LL_STATS_AND_GET_STATION
cppflags-$(CONFIG_WLAN_FEATURE_MIB_STATS) += -DWLAN_FEATURE_MIB_STATS
cppflags-$(CONFIG_FEATURE_WLAN_EXTSCAN) += -DFEATURE_WLAN_EXTSCAN
cppflags-$(CONFIG_160MHZ_SUPPORT) += -DCONFIG_160MHZ_SUPPORT
cppflags-$(CONFIG_MCL) += -DCONFIG_MCL
cppflags-$(CONFIG_REG_CLIENT) += -DCONFIG_REG_CLIENT
cppflags-$(CONFIG_WLAN_PMO_ENABLE) += -DWLAN_PMO_ENABLE
cppflags-$(CONFIG_CONVERGED_P2P_ENABLE) += -DCONVERGED_P2P_ENABLE
cppflags-$(CONFIG_WLAN_POLICY_MGR_ENABLE) += -DWLAN_POLICY_MGR_ENABLE
cppflags-$(CONFIG_FEATURE_BLACKLIST_MGR) += -DFEATURE_BLACKLIST_MGR
cppflags-$(CONFIG_WAPI_BIG_ENDIAN) += -DFEATURE_WAPI_BIG_ENDIAN
cppflags-$(CONFIG_SUPPORT_11AX) += -DSUPPORT_11AX
cppflags-$(CONFIG_HDD_INIT_WITH_RTNL_LOCK) += -DCONFIG_HDD_INIT_WITH_RTNL_LOCK
cppflags-$(CONFIG_WLAN_CONV_SPECTRAL_ENABLE) += -DWLAN_CONV_SPECTRAL_ENABLE
cppflags-$(CONFIG_WLAN_CFR_ENABLE) += -DWLAN_CFR_ENABLE
cppflags-$(CONFIG_WLAN_ENH_CFR_ENABLE) += -DWLAN_ENH_CFR_ENABLE
cppflags-$(CONFIG_WLAN_ENH_CFR_ENABLE) += -DWLAN_CFR_PM
cppflags-$(CONFIG_WLAN_CFR_ENABLE) += -DCFR_USE_FIXED_FOLDER
cppflags-$(CONFIG_DIRECT_BUF_RX_ENABLE) += -DDIRECT_BUF_RX_ENABLE
cppflags-$(CONFIG_WMI_DBR_SUPPORT) += -DWMI_DBR_SUPPORT
ifneq ($(CONFIG_CNSS_QCA6750), y)
cppflags-$(CONFIG_DIRECT_BUF_RX_ENABLE) += -DDBR_MULTI_SRNG_ENABLE
endif
cppflags-$(CONFIG_WMI_CMD_STRINGS) += -DWMI_CMD_STRINGS
cppflags-$(CONFIG_WLAN_FEATURE_TWT) += -DWLAN_SUPPORT_TWT

ifdef CONFIG_WLAN_TWT_SAP_STA_COUNT
WLAN_TWT_SAP_STA_COUNT ?= 32
ccflags-y += -DWLAN_TWT_SAP_STA_COUNT=$(WLAN_TWT_SAP_STA_COUNT)
endif

cppflags-$(CONFIG_WLAN_TWT_SAP_PDEV_COUNT) += -DWLAN_TWT_AP_PDEV_COUNT_NUM_PHY
cppflags-$(CONFIG_WLAN_DISABLE_EXPORT_SYMBOL) += -DWLAN_DISABLE_EXPORT_SYMBOL
cppflags-$(CONFIG_WIFI_POS_CONVERGED) += -DWIFI_POS_CONVERGED
cppflags-$(CONFIG_WIFI_POS_LEGACY) += -DFEATURE_OEM_DATA_SUPPORT
cppflags-$(CONFIG_FEATURE_HTC_CREDIT_HISTORY) += -DFEATURE_HTC_CREDIT_HISTORY
cppflags-$(CONFIG_WLAN_FEATURE_P2P_DEBUG) += -DWLAN_FEATURE_P2P_DEBUG
cppflags-$(CONFIG_WLAN_WEXT_SUPPORT_ENABLE) += -DWLAN_WEXT_SUPPORT_ENABLE
cppflags-$(CONFIG_WLAN_LOGGING_SOCK_SVC) += -DWLAN_LOGGING_SOCK_SVC_ENABLE
cppflags-$(CONFIG_WLAN_LOGGING_BUFFERS_DYNAMICALLY) += -DWLAN_LOGGING_BUFFERS_DYNAMICALLY
cppflags-$(CONFIG_WLAN_FEATURE_FILS) += -DWLAN_FEATURE_FILS_SK
cppflags-$(CONFIG_CP_STATS) += -DWLAN_SUPPORT_INFRA_CTRL_PATH_STATS
cppflags-$(CONFIG_CP_STATS) += -DQCA_SUPPORT_CP_STATS
cppflags-$(CONFIG_CP_STATS) += -DQCA_SUPPORT_MC_CP_STATS
cppflags-$(CONFIG_CP_STATS) += -DWLAN_SUPPORT_LEGACY_CP_STATS_HANDLERS
cppflags-$(CONFIG_DCS) += -DDCS_INTERFERENCE_DETECTION
cppflags-$(CONFIG_FEATURE_INTEROP_ISSUES_AP) += -DWLAN_FEATURE_INTEROP_ISSUES_AP
cppflags-$(CONFIG_FEATURE_MEMDUMP_ENABLE) += -DWLAN_FEATURE_MEMDUMP_ENABLE
cppflags-$(CONFIG_FEATURE_FW_LOG_PARSING) += -DFEATURE_FW_LOG_PARSING
cppflags-$(CONFIG_FEATURE_OEM_DATA) += -DFEATURE_OEM_DATA
cppflags-$(CONFIG_FEATURE_MOTION_DETECTION) += -DWLAN_FEATURE_MOTION_DETECTION
cppflags-$(CONFIG_WLAN_FW_OFFLOAD) += -DWLAN_FW_OFFLOAD
cppflags-$(CONFIG_WLAN_FEATURE_ELNA) += -DWLAN_FEATURE_ELNA
cppflags-$(CONFIG_FEATURE_COEX) += -DFEATURE_COEX
cppflags-$(CONFIG_INTERFACE_MGR) += -DWLAN_FEATURE_INTERFACE_MGR
cppflags-$(CONFIG_HOST_WAKEUP_OVER_QMI) += -DHOST_WAKEUP_OVER_QMI

cppflags-$(CONFIG_PLD_IPCI_ICNSS_FLAG) += -DCONFIG_PLD_IPCI_ICNSS
cppflags-$(CONFIG_PLD_SDIO_CNSS_FLAG) += -DCONFIG_PLD_SDIO_CNSS
cppflags-$(CONFIG_WLAN_RESIDENT_DRIVER) += -DFEATURE_WLAN_RESIDENT_DRIVER

ifeq ($(CONFIG_IPCIE_FW_SIM), y)
cppflags-y += -DCONFIG_PLD_IPCIE_FW_SIM
endif
ifeq ($(CONFIG_PLD_PCIE_CNSS_FLAG), y)
ifeq ($(CONFIG_PCIE_FW_SIM), y)
cppflags-y += -DCONFIG_PLD_PCIE_FW_SIM
else
cppflags-y += -DCONFIG_PLD_PCIE_CNSS
endif
endif

cppflags-$(CONFIG_PLD_PCIE_INIT_FLAG) += -DCONFIG_PLD_PCIE_INIT
cppflags-$(CONFIG_WLAN_FEATURE_DP_RX_THREADS) += -DFEATURE_WLAN_DP_RX_THREADS
cppflags-$(CONFIG_WLAN_FEATURE_RX_SOFTIRQ_TIME_LIMIT) += -DWLAN_FEATURE_RX_SOFTIRQ_TIME_LIMIT
cppflags-$(CONFIG_FEATURE_HAL_DELAYED_REG_WRITE) += -DFEATURE_HAL_DELAYED_REG_WRITE
cppflags-$(CONFIG_QCA_OL_DP_SRNG_LOCK_LESS_ACCESS) += -DQCA_OL_DP_SRNG_LOCK_LESS_ACCESS
cppflags-$(CONFIG_SHADOW_WRITE_DELAY) += -DSHADOW_WRITE_DELAY

cppflags-$(CONFIG_PLD_USB_CNSS) += -DCONFIG_PLD_USB_CNSS
cppflags-$(CONFIG_PLD_SDIO_CNSS2) += -DCONFIG_PLD_SDIO_CNSS2
cppflags-$(CONFIG_WLAN_RECORD_RX_PADDR) += -DHIF_RECORD_RX_PADDR
cppflags-$(CONFIG_FEATURE_WLAN_TIME_SYNC_FTM) += -DFEATURE_WLAN_TIME_SYNC_FTM

#For both legacy and lithium chip's monitor mode config
ifeq ($(CONFIG_FEATURE_MONITOR_MODE_SUPPORT), y)
cppflags-y += -DFEATURE_MONITOR_MODE_SUPPORT
else
cppflags-y += -DDISABLE_MON_CONFIG
endif

#Enable NL80211 test mode
cppflags-$(CONFIG_NL80211_TESTMODE) += -DWLAN_NL80211_TESTMODE

# Flag to enable bus auto suspend
ifeq ($(CONFIG_BUS_AUTO_SUSPEND), y)
cppflags-y += -DFEATURE_RUNTIME_PM
endif

ifeq (y,$(findstring y, $(CONFIG_ICNSS) $(CONFIG_ICNSS_MODULE)))
ifeq ($(CONFIG_SNOC_FW_SIM), y)
cppflags-y += -DCONFIG_PLD_SNOC_FW_SIM
else
cppflags-y += -DCONFIG_PLD_SNOC_ICNSS
endif
endif

cppflags-$(CONFIG_PLD_SNOC_ICNSS_FLAG) += -DCONFIG_PLD_SNOC_ICNSS
cppflags-$(CONFIG_ICNSS2_HELIUM) += -DCONFIG_PLD_SNOC_ICNSS2

cppflags-$(CONFIG_WIFI_3_0_ADRASTEA) += -DQCA_WIFI_3_0_ADRASTEA
cppflags-$(CONFIG_WIFI_3_0_ADRASTEA) += -DQCA_WIFI_3_0
cppflags-$(CONFIG_ADRASTEA_SHADOW_REGISTERS) += -DADRASTEA_SHADOW_REGISTERS
cppflags-$(CONFIG_ADRASTEA_RRI_ON_DDR) += -DADRASTEA_RRI_ON_DDR

ifeq ($(CONFIG_QMI_SUPPORT), n)
cppflags-y += -DCONFIG_BYPASS_QMI
endif

cppflags-$(CONFIG_WLAN_FASTPATH) +=	-DWLAN_FEATURE_FASTPATH

cppflags-$(CONFIG_FEATURE_PKTLOG) +=     -DFEATURE_PKTLOG

ifeq ($(CONFIG_WLAN_NAPI), y)
cppflags-y += -DFEATURE_NAPI
cppflags-y += -DHIF_IRQ_AFFINITY
ifeq ($(CONFIG_WLAN_NAPI_DEBUG), y)
cppflags-y += -DFEATURE_NAPI_DEBUG
endif
endif

ifeq (y,$(findstring y,$(CONFIG_ARCH_MSM) $(CONFIG_ARCH_QCOM)))
cppflags-y += -DMSM_PLATFORM
endif

cppflags-$(CONFIG_WLAN_FEATURE_DP_BUS_BANDWIDTH) += -DWLAN_FEATURE_DP_BUS_BANDWIDTH
cppflags-$(CONFIG_WLAN_FEATURE_PERIODIC_STA_STATS) += -DWLAN_FEATURE_PERIODIC_STA_STATS

cppflags-y +=	-DQCA_SUPPORT_TXRX_LOCAL_PEER_ID

cppflags-$(CONFIG_WLAN_TX_FLOW_CONTROL_V2) += -DQCA_LL_TX_FLOW_CONTROL_V2
cppflags-$(CONFIG_WLAN_TX_FLOW_CONTROL_V2) += -DQCA_LL_TX_FLOW_GLOBAL_MGMT_POOL
cppflags-$(CONFIG_WLAN_TX_FLOW_CONTROL_LEGACY) += -DQCA_LL_LEGACY_TX_FLOW_CONTROL
cppflags-$(CONFIG_WLAN_PDEV_TX_FLOW_CONTROL) += -DQCA_LL_PDEV_TX_FLOW_CONTROL

ifeq ($(BUILD_DEBUG_VERSION), y)
cppflags-y +=	-DWLAN_DEBUG
ifeq ($(CONFIG_TRACE_RECORD_FEATURE), y)
cppflags-y +=	-DTRACE_RECORD \
		-DLIM_TRACE_RECORD \
		-DSME_TRACE_RECORD \
		-DHDD_TRACE_RECORD
endif
endif
cppflags-$(CONFIG_UNIT_TEST) += -DWLAN_UNIT_TEST
cppflags-$(CONFIG_WLAN_DEBUG_CRASH_INJECT) += -DCONFIG_WLAN_DEBUG_CRASH_INJECT
cppflags-$(CONFIG_WLAN_SYSFS_FW_MODE_CFG) += -DCONFIG_WLAN_SYSFS_FW_MODE_CFG
cppflags-$(CONFIG_WLAN_REASSOC) += -DCONFIG_WLAN_REASSOC
cppflags-$(CONFIG_WLAN_SCAN_DISABLE) += -DCONFIG_WLAN_SCAN_DISABLE
cppflags-$(CONFIG_WLAN_WOW_ITO) += -DCONFIG_WLAN_WOW_ITO
cppflags-$(CONFIG_WLAN_WOWL_ADD_PTRN) += -DCONFIG_WLAN_WOWL_ADD_PTRN
cppflags-$(CONFIG_WLAN_WOWL_DEL_PTRN) += -DCONFIG_WLAN_WOWL_DEL_PTRN
cppflags-$(CONFIG_WLAN_SYSFS_TX_STBC) += -DCONFIG_WLAN_SYSFS_TX_STBC
cppflags-$(CONFIG_WLAN_GET_STATS) += -DCONFIG_WLAN_GET_STATS
cppflags-$(CONFIG_WLAN_SYSFS_WLAN_DBG) += -DCONFIG_WLAN_SYSFS_WLAN_DBG
cppflags-$(CONFIG_WLAN_TXRX_FW_ST_RST) += -DCONFIG_WLAN_TXRX_FW_ST_RST
cppflags-$(CONFIG_WLAN_GTX_BW_MASK) += -DCONFIG_WLAN_GTX_BW_MASK
cppflags-$(CONFIG_WLAN_SYSFS_SCAN_CFG) += -DCONFIG_WLAN_SYSFS_SCAN_CFG
cppflags-$(CONFIG_WLAN_SYSFS_MONITOR_MODE_CHANNEL) += -DCONFIG_WLAN_SYSFS_MONITOR_MODE_CHANNEL
cppflags-$(CONFIG_WLAN_SYSFS_RADAR) += -DCONFIG_WLAN_SYSFS_RADAR
cppflags-$(CONFIG_WLAN_SYSFS_RTS_CTS) += -DWLAN_SYSFS_RTS_CTS
cppflags-$(CONFIG_WLAN_TXRX_FW_STATS) += -DCONFIG_WLAN_TXRX_FW_STATS
cppflags-$(CONFIG_WLAN_TXRX_STATS) += -DCONFIG_WLAN_TXRX_STATS
cppflags-$(CONFIG_WLAN_SYSFS_DP_TRACE) += -DWLAN_SYSFS_DP_TRACE
cppflags-$(CONFIG_WLAN_SYSFS_STATS) += -DWLAN_SYSFS_STATS
cppflags-$(CONFIG_WLAN_SYSFS_TEMPERATURE) += -DCONFIG_WLAN_SYSFS_TEMPERATURE
cppflags-$(CONFIG_WLAN_THERMAL_CFG) += -DCONFIG_WLAN_THERMAL_CFG
cppflags-$(CONFIG_FEATURE_UNIT_TEST_SUSPEND) += -DWLAN_SUSPEND_RESUME_TEST
cppflags-$(CONFIG_FEATURE_WLM_STATS) += -DFEATURE_WLM_STATS
cppflags-$(CONFIG_WLAN_SYSFS_MEM_STATS) += -DCONFIG_WLAN_SYSFS_MEM_STATS
cppflags-$(CONFIG_WLAN_SYSFS_DCM) += -DWLAN_SYSFS_DCM
cppflags-$(CONFIG_WLAN_SYSFS_HE_BSS_COLOR) += -DWLAN_SYSFS_HE_BSS_COLOR
cppflags-$(CONFIG_WLAN_SYSFS_STA_INFO) += -DWLAN_SYSFS_STA_INFO
cppflags-$(CONFIG_WLAN_DL_MODES) += -DCONFIG_WLAN_DL_MODES
cppflags-$(CONFIG_WLAN_THERMAL_MULTI_CLIENT_SUPPORT) += -DFEATURE_WPSS_THERMAL_MITIGATION
cppflags-$(CONFIG_WLAN_DUMP_IN_PROGRESS) += -DCONFIG_WLAN_DUMP_IN_PROGRESS

ifeq ($(CONFIG_LEAK_DETECTION), y)
cppflags-y += \
	-DCONFIG_HALT_KMEMLEAK \
	-DCONFIG_LEAK_DETECTION \
	-DMEMORY_DEBUG \
	-DNBUF_MEMORY_DEBUG \
	-DNBUF_MAP_UNMAP_DEBUG \
	-DTIMER_MANAGER \
	-DWLAN_DELAYED_WORK_DEBUG \
	-DWLAN_WAKE_LOCK_DEBUG \
	-DWLAN_PERIODIC_WORK_DEBUG
endif

cppflags-y += -DWLAN_FEATURE_P2P
cppflags-y += -DWLAN_FEATURE_WFD

ifeq ($(CONFIG_QCOM_VOWIFI_11R), y)
cppflags-y += -DKERNEL_SUPPORT_11R_CFG80211
cppflags-y += -DUSE_80211_WMMTSPEC_FOR_RIC
endif

ifeq ($(CONFIG_QCOM_ESE), y)
cppflags-y += -DFEATURE_WLAN_ESE
endif

#normally, TDLS negative behavior is not needed
cppflags-$(CONFIG_QCOM_TDLS) += -DFEATURE_WLAN_TDLS
ifeq ($(CONFIG_LITHIUM), y)
cppflags-$(CONFIG_QCOM_TDLS) += -DTDLS_WOW_ENABLED
endif

cppflags-$(CONFIG_WLAN_SYSFS_TDLS_PEERS) += -DWLAN_SYSFS_TDLS_PEERS
cppflags-$(CONFIG_WLAN_SYSFS_RANGE_EXT) += -DWLAN_SYSFS_RANGE_EXT

cppflags-y += -DCONN_MGR_ADV_FEATURE

cppflags-$(CONFIG_QCACLD_WLAN_LFR3) += -DWLAN_FEATURE_ROAM_OFFLOAD

cppflags-$(CONFIG_WLAN_FEATURE_MBSSID) += -DWLAN_FEATURE_MBSSID

ifeq (y,$(findstring y, $(CONFIG_CNSS_GENL) $(CONFIG_CNSS_GENL_MODULE)))
cppflags-y += -DCNSS_GENL
endif

cppflags-$(CONFIG_QCACLD_WLAN_LFR2) += -DWLAN_FEATURE_HOST_ROAM

cppflags-$(CONFIG_FEATURE_ROAM_DEBUG) += -DFEATURE_ROAM_DEBUG

cppflags-$(CONFIG_WLAN_POWER_DEBUG) += -DWLAN_POWER_DEBUG

cppflags-$(CONFIG_WLAN_MWS_INFO_DEBUGFS) += -DWLAN_MWS_INFO_DEBUGFS

# Enable object manager reference count debug infrastructure
cppflags-$(CONFIG_WLAN_OBJMGR_DEBUG) += -DWLAN_OBJMGR_DEBUG
cppflags-$(CONFIG_WLAN_OBJMGR_DEBUG) += -DWLAN_OBJMGR_REF_ID_DEBUG

cppflags-$(CONFIG_WLAN_FEATURE_SAE) += -DWLAN_FEATURE_SAE

ifeq ($(BUILD_DIAG_VERSION), y)
cppflags-y += -DFEATURE_WLAN_DIAG_SUPPORT
cppflags-y += -DFEATURE_WLAN_DIAG_SUPPORT_CSR
cppflags-y += -DFEATURE_WLAN_DIAG_SUPPORT_LIM
ifeq ($(CONFIG_HIF_PCI), y)
cppflags-y += -DCONFIG_ATH_PROCFS_DIAG_SUPPORT
endif
ifeq ($(CONFIG_HIF_IPCI), y)
cppflags-y += -DCONFIG_ATH_PROCFS_DIAG_SUPPORT
endif
endif

ifeq ($(CONFIG_HIF_USB), y)
cppflags-y += -DCONFIG_ATH_PROCFS_DIAG_SUPPORT
cppflags-y += -DQCA_SUPPORT_OL_RX_REORDER_TIMEOUT
cppflags-y += -DCONFIG_ATH_PCIE_MAX_PERF=0 -DCONFIG_ATH_PCIE_AWAKE_WHILE_DRIVER_LOAD=0 -DCONFIG_DISABLE_CDC_MAX_PERF_WAR=0
endif

cppflags-$(CONFIG_QCA_SUPPORT_TXRX_DRIVER_TCP_DEL_ACK) += -DQCA_SUPPORT_TXRX_DRIVER_TCP_DEL_ACK

cppflags-$(CONFIG_WLAN_FEATURE_11W) += -DWLAN_FEATURE_11W

cppflags-$(CONFIG_QCA_TXDESC_SANITY_CHECKS) += -DQCA_SUPPORT_TXDESC_SANITY_CHECKS

cppflags-$(CONFIG_QCOM_LTE_COEX) += -DFEATURE_WLAN_CH_AVOID

cppflags-$(CONFIG_WLAN_FEATURE_LPSS) += -DWLAN_FEATURE_LPSS

cppflags-$(CONFIG_DESC_DUP_DETECT_DEBUG) += -DDESC_DUP_DETECT_DEBUG
cppflags-$(CONFIG_DEBUG_RX_RING_BUFFER) += -DDEBUG_RX_RING_BUFFER

cppflags-$(CONFIG_DESC_TIMESTAMP_DEBUG_INFO) += -DDESC_TIMESTAMP_DEBUG_INFO

cppflags-$(PANIC_ON_BUG) += -DPANIC_ON_BUG

cppflags-$(WLAN_WARN_ON_ASSERT) += -DWLAN_WARN_ON_ASSERT

ccflags-$(CONFIG_POWER_MANAGEMENT_OFFLOAD) += -DWLAN_POWER_MANAGEMENT_OFFLOAD

cppflags-$(CONFIG_WLAN_LOG_FATAL) += -DWLAN_LOG_FATAL
cppflags-$(CONFIG_WLAN_LOG_ERROR) += -DWLAN_LOG_ERROR
cppflags-$(CONFIG_WLAN_LOG_WARN) += -DWLAN_LOG_WARN
cppflags-$(CONFIG_WLAN_LOG_INFO) += -DWLAN_LOG_INFO
cppflags-$(CONFIG_WLAN_LOG_DEBUG) += -DWLAN_LOG_DEBUG
cppflags-$(CONFIG_WLAN_LOG_ENTER) += -DWLAN_LOG_ENTER
cppflags-$(CONFIG_WLAN_LOG_EXIT) += -DWLAN_LOG_EXIT
cppflags-$(WLAN_OPEN_SOURCE) += -DWLAN_OPEN_SOURCE
cppflags-$(CONFIG_FEATURE_STATS_EXT) += -DWLAN_FEATURE_STATS_EXT
cppflags-$(CONFIG_QCACLD_FEATURE_NAN) += -DWLAN_FEATURE_NAN
cppflags-$(CONFIG_NDP_SAP_CONCURRENCY_ENABLE) += -DNDP_SAP_CONCURRENCY_ENABLE

ifeq ($(CONFIG_DFS_FCC_TYPE4_DURATION_CHECK), y)
cppflags-$(CONFIG_DFS_FCC_TYPE4_DURATION_CHECK) += -DDFS_FCC_TYPE4_DURATION_CHECK
endif

cppflags-$(CONFIG_WLAN_SYSFS) += -DWLAN_SYSFS
cppflags-$(CONFIG_WLAN_SYSFS_CHANNEL) += -DWLAN_SYSFS_CHANNEL
cppflags-$(CONFIG_FEATURE_BECN_STATS) += -DWLAN_FEATURE_BEACON_RECEPTION_STATS

cppflags-$(CONFIG_WLAN_SYSFS_CONNECT_INFO) += -DWLAN_SYSFS_CONNECT_INFO

#Set RX_PERFORMANCE
cppflags-$(CONFIG_RX_PERFORMANCE) += -DRX_PERFORMANCE

#Set MULTI_IF_LOG
cppflags-$(CONFIG_MULTI_IF_LOG) += -DMULTI_IF_LOG

#Set SLUB_MEM_OPTIMIZE
cppflags-$(CONFIG_SLUB_MEM_OPTIMIZE) += -DSLUB_MEM_OPTIMIZE

#Set DFS_PRI_MULTIPLIER
cppflags-$(CONFIG_DFS_PRI_MULTIPLIER) += -DDFS_PRI_MULTIPLIER

#Set DFS_OVERRIDE_RF_THRESHOLD
cppflags-$(CONFIG_DFS_OVERRIDE_RF_THRESHOLD) += -DDFS_OVERRIDE_RF_THRESHOLD

#Enable OL debug and wmi unified functions
cppflags-$(CONFIG_ATH_PERF_PWR_OFFLOAD) += -DATH_PERF_PWR_OFFLOAD

#Disable packet log
cppflags-$(CONFIG_REMOVE_PKT_LOG) += -DREMOVE_PKT_LOG

#Enable 11AC TX
cppflags-$(CONFIG_ATH_11AC_TXCOMPACT) += -DATH_11AC_TXCOMPACT

#Enable PCI specific APIS (dma, etc)
cppflags-$(CONFIG_HIF_PCI) += -DHIF_PCI

cppflags-$(CONFIG_HIF_IPCI) += -DHIF_IPCI

cppflags-$(CONFIG_HIF_SNOC) += -DHIF_SNOC

cppflags-$(CONFIG_HL_DP_SUPPORT) += -DCONFIG_HL_SUPPORT
cppflags-$(CONFIG_HL_DP_SUPPORT) += -DWLAN_PARTIAL_REORDER_OFFLOAD
cppflags-$(CONFIG_HL_DP_SUPPORT) += -DQCA_COMPUTE_TX_DELAY
cppflags-$(CONFIG_HL_DP_SUPPORT) += -DQCA_COMPUTE_TX_DELAY_PER_TID
cppflags-$(CONFIG_LL_DP_SUPPORT) += -DCONFIG_LL_DP_SUPPORT
cppflags-$(CONFIG_LL_DP_SUPPORT) += -DWLAN_FULL_REORDER_OFFLOAD
cppflags-$(CONFIG_WLAN_FEATURE_BIG_DATA_STATS) += -DWLAN_FEATURE_BIG_DATA_STATS
cppflags-$(CONFIG_WLAN_FEATURE_IGMP_OFFLOAD) += -DWLAN_FEATURE_IGMP_OFFLOAD
cppflags-$(CONFIG_WLAN_FEATURE_GET_USABLE_CHAN_LIST) += -DWLAN_FEATURE_GET_USABLE_CHAN_LIST

# For PCIe GEN switch
cppflags-$(CONFIG_PCIE_GEN_SWITCH) += -DPCIE_GEN_SWITCH

# For OOB testing
cppflags-$(CONFIG_WLAN_FEATURE_WOW_PULSE) += -DWLAN_FEATURE_WOW_PULSE

#Enable High Latency related Flags
ifeq ($(CONFIG_QCA_WIFI_SDIO), y)
cppflags-y += -DCONFIG_AR6320_SUPPORT \
            -DSDIO_3_0 \
            -DHIF_SDIO \
            -DCONFIG_DISABLE_CDC_MAX_PERF_WAR=0 \
            -DCONFIG_ATH_PROCFS_DIAG_SUPPORT \
            -DHIF_MBOX_SLEEP_WAR \
            -DDEBUG_HL_LOGGING \
            -DQCA_BAD_PEER_TX_FLOW_CL \
            -DCONFIG_SDIO \
            -DFEATURE_WLAN_FORCE_SAP_SCC

ifeq ($(CONFIG_SDIO_TRANSFER), adma)
cppflags-y += -DCONFIG_SDIO_TRANSFER_ADMA
else
cppflags-y += -DCONFIG_SDIO_TRANSFER_MAILBOX
endif
endif

ifeq ($(CONFIG_WLAN_FEATURE_DSRC), y)
cppflags-y += -DWLAN_FEATURE_DSRC

ifeq ($(CONFIG_OCB_UT_FRAMEWORK), y)
cppflags-y += -DWLAN_OCB_UT
endif

endif

cppflags-$(CONFIG_FEATURE_SKB_PRE_ALLOC) += -DFEATURE_SKB_PRE_ALLOC

#Enable USB specific APIS
ifeq ($(CONFIG_HIF_USB), y)
cppflags-y += -DHIF_USB \
            -DDEBUG_HL_LOGGING
endif

#Enable Genoa specific features.
cppflags-$(CONFIG_QCA_HL_NETDEV_FLOW_CONTROL) += -DQCA_HL_NETDEV_FLOW_CONTROL
cppflags-$(CONFIG_FEATURE_HL_GROUP_CREDIT_FLOW_CONTROL) += -DFEATURE_HL_GROUP_CREDIT_FLOW_CONTROL
cppflags-$(CONFIG_FEATURE_HL_DBS_GROUP_CREDIT_SHARING) += -DFEATURE_HL_DBS_GROUP_CREDIT_SHARING
cppflags-$(CONFIG_CREDIT_REP_THROUGH_CREDIT_UPDATE) += -DCONFIG_CREDIT_REP_THROUGH_CREDIT_UPDATE
cppflags-$(CONFIG_RX_PN_CHECK_OFFLOAD) += -DCONFIG_RX_PN_CHECK_OFFLOAD

cppflags-$(CONFIG_WLAN_SYNC_TSF_TIMER) += -DWLAN_FEATURE_TSF_TIMER_SYNC
cppflags-$(CONFIG_WLAN_SYNC_TSF_PTP) += -DWLAN_FEATURE_TSF_PTP
cppflags-$(CONFIG_WLAN_SYNC_TSF_PLUS_EXT_GPIO_IRQ) += -DWLAN_FEATURE_TSF_PLUS_EXT_GPIO_IRQ
cppflags-$(CONFIG_WLAN_SYNC_TSF_PLUS_EXT_GPIO_SYNC) += -DWLAN_FEATURE_TSF_PLUS_EXT_GPIO_SYNC
cppflags-$(CONFIG_TX_DESC_HI_PRIO_RESERVE) += -DCONFIG_TX_DESC_HI_PRIO_RESERVE

#Enable FW logs through ini
cppflags-y += -DCONFIG_FW_LOGS_BASED_ON_INI

#Enable power management suspend/resume functionality
cppflags-$(CONFIG_ATH_BUS_PM) += -DATH_BUS_PM

#Enable FLOWMAC module support
cppflags-$(CONFIG_ATH_SUPPORT_FLOWMAC_MODULE) += -DATH_SUPPORT_FLOWMAC_MODULE

#Enable spectral support
cppflags-$(CONFIG_ATH_SUPPORT_SPECTRAL) += -DATH_SUPPORT_SPECTRAL

#Enable legacy pktlog
cppflags-$(CONFIG_PKTLOG_LEGACY) += -DPKTLOG_LEGACY

#Enable WDI Event support
cppflags-$(CONFIG_WDI_EVENT_ENABLE) += -DWDI_EVENT_ENABLE

#Enable the type_specific_data in the struct ath_pktlog_arg
cppflags-$(CONFIG_PKTLOG_HAS_SPECIFIC_DATA) += -DPKTLOG_HAS_SPECIFIC_DATA

#Endianness selection
ifeq ($(CONFIG_LITTLE_ENDIAN), y)
cppflags-y += -DANI_LITTLE_BYTE_ENDIAN
cppflags-y += -DANI_LITTLE_BIT_ENDIAN
cppflags-y += -DDOT11F_LITTLE_ENDIAN_HOST
else
cppflags-y += -DANI_BIG_BYTE_ENDIAN
cppflags-y += -DBIG_ENDIAN_HOST
endif

#Enable MWS COEX support for 4G quick TDM and 5G NR pwr limit
cppflags-y += -DMWS_COEX

#Enable TX reclaim support
cppflags-$(CONFIG_TX_CREDIT_RECLAIM_SUPPORT) += -DTX_CREDIT_RECLAIM_SUPPORT

#Enable FTM support
cppflags-$(CONFIG_QCA_WIFI_FTM) += -DQCA_WIFI_FTM
cppflags-$(CONFIG_NL80211_TESTMODE) += -DQCA_WIFI_FTM_NL80211
cppflags-$(CONFIG_LINUX_QCMBR) += -DLINUX_QCMBR -DQCA_WIFI_FTM_IOCTL

#Enable Checksum Offload support
cppflags-$(CONFIG_CHECKSUM_OFFLOAD) += -DCHECKSUM_OFFLOAD

#Enable IPA Offload support
cppflags-$(CONFIG_IPA_OFFLOAD) += -DIPA_OFFLOAD

cppflags-$(CONFIG_WDI3_IPA_OVER_GSI) += -DIPA_WDI3_GSI
cppflags-$(CONFIG_WDI2_IPA_OVER_GSI) += -DIPA_WDI2_GSI

#Enable WMI DIAG log over CE7
cppflags-$(CONFIG_WLAN_FEATURE_WMI_DIAG_OVER_CE7) += -DWLAN_FEATURE_WMI_DIAG_OVER_CE7

ifdef CONFIG_WLAN_DP_FEATURE_DEFERRED_REO_QDESC_DESTROY
cppflags-y += -DWLAN_DP_FEATURE_DEFERRED_REO_QDESC_DESTROY
endif

ifeq ($(CONFIG_ARCH_SDX20), y)
cppflags-y += -DSYNC_IPA_READY
endif

ifeq ($(CONFIG_ARCH_SDXPOORWILLS), y)
CONFIG_FEATURE_SG := y
endif

ifeq ($(CONFIG_ARCH_MSM8996), y)
ifneq ($(CONFIG_QCN7605_SUPPORT), y)
CONFIG_FEATURE_SG := y
CONFIG_RX_THREAD_PRIORITY := y
endif
endif

ifeq ($(CONFIG_FEATURE_SG), y)
cppflags-y += -DFEATURE_SG
endif

ifeq ($(CONFIG_RX_THREAD_PRIORITY), y)
cppflags-y += -DRX_THREAD_PRIORITY
endif

ifeq ($(CONFIG_SUPPORT_P2P_BY_ONE_INTF_WLAN), y)
#sta support to tx P2P action frames
cppflags-y += -DSUPPORT_P2P_BY_ONE_INTF_WLAN
else
#Open P2P device interface only for non-Mobile router use cases
cppflags-$(CONFIG_WLAN_OPEN_P2P_INTERFACE) += -DWLAN_OPEN_P2P_INTERFACE
endif

cppflags-$(CONFIG_WMI_BCN_OFFLOAD) += -DWLAN_WMI_BCN

#Enable wbuff
cppflags-$(CONFIG_WLAN_WBUFF) += -DWLAN_FEATURE_WBUFF

#Enable GTK Offload
cppflags-$(CONFIG_GTK_OFFLOAD) += -DWLAN_FEATURE_GTK_OFFLOAD

#Enable External WoW
cppflags-$(CONFIG_EXT_WOW) += -DWLAN_FEATURE_EXTWOW_SUPPORT

#Mark it as SMP Kernel
cppflags-$(CONFIG_SMP) += -DQCA_CONFIG_SMP

cppflags-$(CONFIG_CHNL_MATRIX_RESTRICTION) += -DWLAN_ENABLE_CHNL_MATRIX_RESTRICTION

#Enable ICMP packet disable powersave feature
cppflags-$(CONFIG_ICMP_DISABLE_PS) += -DWLAN_ICMP_DISABLE_PS

#Enable OBSS feature
cppflags-y += -DQCA_HT_2040_COEX

#enable MCC TO SCC switch
cppflags-$(CONFIG_FEATURE_WLAN_MCC_TO_SCC_SWITCH) += -DFEATURE_WLAN_MCC_TO_SCC_SWITCH

#enable wlan auto shutdown feature
cppflags-$(CONFIG_FEATURE_WLAN_AUTO_SHUTDOWN) += -DFEATURE_WLAN_AUTO_SHUTDOWN

#enable AP-AP ACS Optimization
cppflags-$(CONFIG_FEATURE_WLAN_AP_AP_ACS_OPTIMIZE) += -DFEATURE_WLAN_AP_AP_ACS_OPTIMIZE

#Enable 4address scheme
cppflags-$(CONFIG_FEATURE_WLAN_STA_4ADDR_SCHEME) += -DFEATURE_WLAN_STA_4ADDR_SCHEME

#enable MDM/SDX special config
cppflags-$(CONFIG_MDM_PLATFORM) += -DMDM_PLATFORM

#Disable STA-AP Mode DFS support
cppflags-$(CONFIG_FEATURE_WLAN_STA_AP_MODE_DFS_DISABLE) += -DFEATURE_WLAN_STA_AP_MODE_DFS_DISABLE

#Enable 2.4 GHz social channels in 5 GHz only mode for p2p usage
cppflags-$(CONFIG_WLAN_ENABLE_SOCIAL_CHANNELS_5G_ONLY) += -DWLAN_ENABLE_SOCIAL_CHANNELS_5G_ONLY

#Green AP feature
cppflags-$(CONFIG_QCACLD_FEATURE_GREEN_AP) += -DWLAN_SUPPORT_GREEN_AP

cppflags-$(CONFIG_QCACLD_FEATURE_APF) += -DFEATURE_WLAN_APF

cppflags-$(CONFIG_WLAN_FEATURE_SARV1_TO_SARV2) += -DWLAN_FEATURE_SARV1_TO_SARV2
#CRYPTO Coverged Component
cppflags-$(CONFIG_CRYPTO_COMPONENT) += -DWLAN_CONV_CRYPTO_SUPPORTED \
                                       -DCRYPTO_SET_KEY_CONVERGED \
                                       -DWLAN_CRYPTO_WEP_OS_DERIVATIVE \
                                       -DWLAN_CRYPTO_TKIP_OS_DERIVATIVE \
                                       -DWLAN_CRYPTO_CCMP_OS_DERIVATIVE \
                                       -DWLAN_CRYPTO_GCMP_OS_DERIVATIVE \
                                       -DWLAN_CRYPTO_WAPI_OS_DERIVATIVE \
                                       -DWLAN_CRYPTO_GCM_OS_DERIVATIVE \
                                       -DWLAN_CRYPTO_FILS_OS_DERIVATIVE \
                                       -DWLAN_CRYPTO_OMAC1_OS_DERIVATIVE

cppflags-$(CONFIG_FEATURE_WLAN_FT_IEEE8021X) += -DFEATURE_WLAN_FT_IEEE8021X
cppflags-$(CONFIG_FEATURE_WLAN_FT_PSK) += -DFEATURE_WLAN_FT_PSK

#Enable host 11d scan
cppflags-$(CONFIG_HOST_11D_SCAN) += -DHOST_11D_SCAN

#Stats & Quota Metering feature
ifeq ($(CONFIG_IPA_OFFLOAD), y)
ifeq ($(CONFIG_QCACLD_FEATURE_METERING), y)
cppflags-y += -DFEATURE_METERING
endif
endif

#Define Max IPA interface
ifeq ($(CONFIG_IPA_OFFLOAD), y)
ifdef CONFIG_NUM_IPA_IFACE
cppflags-y += -DMAX_IPA_IFACE=$(CONFIG_NUM_IPA_IFACE)
else
NUM_IPA_IFACE ?= 3
cppflags-y += -DMAX_IPA_IFACE=$(NUM_IPA_IFACE)
endif
endif


cppflags-$(CONFIG_TUFELLO_DUAL_FW_SUPPORT) += -DCONFIG_TUFELLO_DUAL_FW_SUPPORT
cppflags-$(CONFIG_CHANNEL_HOPPING_ALL_BANDS) += -DCHANNEL_HOPPING_ALL_BANDS

#Enable Signed firmware support for split binary format
cppflags-$(CONFIG_QCA_SIGNED_SPLIT_BINARY_SUPPORT) += -DQCA_SIGNED_SPLIT_BINARY_SUPPORT

#Enable single firmware binary format
cppflags-$(CONFIG_QCA_SINGLE_BINARY_SUPPORT) += -DQCA_SINGLE_BINARY_SUPPORT

#Enable collecting target RAM dump after kernel panic
cppflags-$(CONFIG_TARGET_RAMDUMP_AFTER_KERNEL_PANIC) += -DTARGET_RAMDUMP_AFTER_KERNEL_PANIC

#Enable/disable secure firmware feature
cppflags-$(CONFIG_FEATURE_SECURE_FIRMWARE) += -DFEATURE_SECURE_FIRMWARE

cppflags-$(CONFIG_ATH_PCIE_ACCESS_DEBUG) += -DCONFIG_ATH_PCIE_ACCESS_DEBUG

# Enable feature support for Linux version QCMBR
cppflags-$(CONFIG_LINUX_QCMBR) += -DLINUX_QCMBR

# Enable feature sync tsf between multi devices
cppflags-$(CONFIG_WLAN_SYNC_TSF) += -DWLAN_FEATURE_TSF
cppflags-$(CONFIG_WLAN_SYNC_TSF_PLUS) += -DWLAN_FEATURE_TSF_PLUS
# Enable feature sync tsf for chips based on Adrastea arch
cppflags-$(CONFIG_WLAN_SYNC_TSF_PLUS_NOIRQ) += -DWLAN_FEATURE_TSF_PLUS_NOIRQ

cppflags-$(CONFIG_ATH_PROCFS_DIAG_SUPPORT) += -DCONFIG_ATH_PROCFS_DIAG_SUPPORT

cppflags-$(CONFIG_HELIUMPLUS) += -DHELIUMPLUS
cppflags-$(CONFIG_RX_OL) += -DRECEIVE_OFFLOAD
cppflags-$(CONFIG_TX_TID_OVERRIDE) += -DATH_TX_PRI_OVERRIDE
cppflags-$(CONFIG_AR900B) += -DAR900B
cppflags-$(CONFIG_HTT_PADDR64) += -DHTT_PADDR64
cppflags-$(CONFIG_OL_RX_INDICATION_RECORD) += -DOL_RX_INDICATION_RECORD
cppflags-$(CONFIG_TSOSEG_DEBUG) += -DTSOSEG_DEBUG
cppflags-$(CONFIG_ALLOW_PKT_DROPPING) += -DFEATURE_ALLOW_PKT_DROPPING

cppflags-$(CONFIG_ENABLE_DEBUG_ADDRESS_MARKING) += -DENABLE_DEBUG_ADDRESS_MARKING
cppflags-$(CONFIG_FEATURE_TSO) += -DFEATURE_TSO
cppflags-$(CONFIG_FEATURE_TSO_DEBUG) += -DFEATURE_TSO_DEBUG
cppflags-$(CONFIG_FEATURE_TSO_STATS) += -DFEATURE_TSO_STATS
cppflags-$(CONFIG_FEATURE_FORCE_WAKE) += -DFORCE_WAKE
cppflags-$(CONFIG_WLAN_LRO) += -DFEATURE_LRO

cppflags-$(CONFIG_FEATURE_AP_MCC_CH_AVOIDANCE) += -DFEATURE_AP_MCC_CH_AVOIDANCE

cppflags-$(CONFIG_MPC_UT_FRAMEWORK) += -DMPC_UT_FRAMEWORK

cppflags-$(CONFIG_FEATURE_EPPING) += -DWLAN_FEATURE_EPPING

cppflags-$(CONFIG_WLAN_OFFLOAD_PACKETS) += -DWLAN_FEATURE_OFFLOAD_PACKETS

cppflags-$(CONFIG_WLAN_FEATURE_DISA) += -DWLAN_FEATURE_DISA

cppflags-$(CONFIG_WLAN_FEATURE_ACTION_OUI) += -DWLAN_FEATURE_ACTION_OUI

cppflags-$(CONFIG_WLAN_FEATURE_FIPS) += -DWLAN_FEATURE_FIPS

cppflags-$(CONFIG_LFR_SUBNET_DETECTION) += -DFEATURE_LFR_SUBNET_DETECTION

cppflags-$(CONFIG_MCC_TO_SCC_SWITCH) += -DFEATURE_WLAN_MCC_TO_SCC_SWITCH

cppflags-$(CONFIG_FEATURE_WLAN_D0WOW) += -DFEATURE_WLAN_D0WOW

cppflags-$(CONFIG_WLAN_FEATURE_PKT_CAPTURE) += -DWLAN_FEATURE_PKT_CAPTURE

cppflags-$(CONFIG_WLAN_FEATURE_PKT_CAPTURE_V2) += -DWLAN_FEATURE_PKT_CAPTURE_V2

cppflags-$(CONFIG_QCA_WIFI_NAPIER_EMULATION) += -DQCA_WIFI_NAPIER_EMULATION
cppflags-$(CONFIG_SHADOW_V2) += -DCONFIG_SHADOW_V2
cppflags-$(CONFIG_QCA6290_HEADERS_DEF) += -DQCA6290_HEADERS_DEF
cppflags-$(CONFIG_QCA_WIFI_QCA6290) += -DQCA_WIFI_QCA6290
cppflags-$(CONFIG_QCA6390_HEADERS_DEF) += -DQCA6390_HEADERS_DEF
cppflags-$(CONFIG_QCA6750_HEADERS_DEF) += -DQCA6750_HEADERS_DEF
cppflags-$(CONFIG_QCA_WIFI_QCA6390) += -DQCA_WIFI_QCA6390
cppflags-$(CONFIG_QCA6490_HEADERS_DEF) += -DQCA6490_HEADERS_DEF
cppflags-$(CONFIG_QCA_WIFI_QCA6490) += -DQCA_WIFI_QCA6490
cppflags-$(CONFIG_QCA_WIFI_QCA6750) += -DQCA_WIFI_QCA6750
cppflags-$(CONFIG_QCA_WIFI_QCA8074) += -DQCA_WIFI_QCA8074
cppflags-$(CONFIG_SCALE_INCLUDES) += -DSCALE_INCLUDES
cppflags-$(CONFIG_QCA_WIFI_QCA8074_VP) += -DQCA_WIFI_QCA8074_VP
cppflags-$(CONFIG_DP_INTR_POLL_BASED) += -DDP_INTR_POLL_BASED
cppflags-$(CONFIG_TX_PER_PDEV_DESC_POOL) += -DTX_PER_PDEV_DESC_POOL
cppflags-$(CONFIG_DP_TRACE) += -DCONFIG_DP_TRACE
cppflags-$(CONFIG_FEATURE_TSO) += -DFEATURE_TSO
cppflags-$(CONFIG_TSO_DEBUG_LOG_ENABLE) += -DTSO_DEBUG_LOG_ENABLE
cppflags-$(CONFIG_DP_LFR) += -DDP_LFR
cppflags-$(CONFIG_DUP_RX_DESC_WAR) += -DDUP_RX_DESC_WAR
cppflags-$(CONFIG_DP_MEM_PRE_ALLOC) += -DDP_MEM_PRE_ALLOC
cppflags-$(CONFIG_DP_TXRX_SOC_ATTACH) += -DDP_TXRX_SOC_ATTACH
cppflags-$(CONFIG_HTT_PADDR64) += -DHTT_PADDR64
cppflags-$(CONFIG_WLAN_FEATURE_BMI) += -DWLAN_FEATURE_BMI
cppflags-$(CONFIG_QCA_TX_PADDING_CREDIT_SUPPORT) += -DQCA_TX_PADDING_CREDIT_SUPPORT
cppflags-$(CONFIG_QCN7605_SUPPORT) += -DQCN7605_SUPPORT -DPLATFORM_GENOA
cppflags-$(CONFIG_HIF_REG_WINDOW_SUPPORT) += -DHIF_REG_WINDOW_SUPPORT
cppflags-$(CONFIG_WLAN_ALLOCATE_GLOBAL_BUFFERS_DYNAMICALLY) += -DWLAN_ALLOCATE_GLOBAL_BUFFERS_DYNAMICALLY
cppflags-$(CONFIG_HIF_CE_DEBUG_DATA_BUF) += -DHIF_CE_DEBUG_DATA_BUF
cppflags-$(CONFIG_IPA_DISABLE_OVERRIDE) += -DIPA_DISABLE_OVERRIDE
ccflags-$(CONFIG_QCA_LL_TX_FLOW_CONTROL_RESIZE) += -DQCA_LL_TX_FLOW_CONTROL_RESIZE
ccflags-$(CONFIG_HIF_PCI) += -DCE_SVC_CMN_INIT
ccflags-$(CONFIG_HIF_IPCI) += -DCE_SVC_CMN_INIT
ccflags-$(CONFIG_HIF_SNOC) += -DCE_SVC_CMN_INIT
cppflags-$(CONFIG_RX_DESC_SANITY_WAR) += -DRX_DESC_SANITY_WAR
cppflags-$(CONFIG_WBM_IDLE_LSB_WR_CNF_WAR) += -DWBM_IDLE_LSB_WRITE_CONFIRM_WAR
cppflags-$(CONFIG_DYNAMIC_RX_AGGREGATION) += -DWLAN_FEATURE_DYNAMIC_RX_AGGREGATION

cppflags-$(CONFIG_RX_HASH_DEBUG) += -DRX_HASH_DEBUG

ifeq ($(CONFIG_QCA6290_11AX), y)
cppflags-y += -DQCA_WIFI_QCA6290_11AX -DQCA_WIFI_QCA6290_11AX_MU_UL
endif

ifeq ($(CONFIG_LITHIUM), y)
cppflags-$(CONFIG_WLAN_TX_FLOW_CONTROL_V2) += -DQCA_AC_BASED_FLOW_CONTROL
cppflags-y += -DFEATURE_NO_DBS_INTRABAND_MCC_SUPPORT
cppflags-y += -DHAL_DISABLE_NON_BA_2K_JUMP_ERROR
cppflags-y += -DENABLE_HAL_SOC_STATS
cppflags-y += -DENABLE_HAL_REG_WR_HISTORY
cppflags-y += -DDP_RX_DESC_COOKIE_INVALIDATE
cppflags-y += -DMON_ENABLE_DROP_FOR_MAC
cppflags-y += -DPCI_LINK_STATUS_SANITY
cppflags-y += -DDP_MON_RSSI_IN_DBM
cppflags-y += -DSYSTEM_PM_CHECK
cppflags-y += -DDISABLE_EAPOL_INTRABSS_FWD
cppflags-y += -DDISABLE_MON_RING_MSI_CFG
endif

# Enable Low latency optimisation mode
cppflags-$(CONFIG_WLAN_FEATURE_LL_MODE) += -DWLAN_FEATURE_LL_MODE

cppflags-$(CONFIG_WLAN_CLD_PM_QOS) += -DCLD_PM_QOS
cppflags-$(CONFIG_WLAN_CLD_DEV_PM_QOS) += -DCLD_DEV_PM_QOS
cppflags-$(CONFIG_REO_DESC_DEFER_FREE) += -DREO_DESC_DEFER_FREE
cppflags-$(CONFIG_WLAN_FEATURE_11AX) += -DWLAN_FEATURE_11AX
cppflags-$(CONFIG_WLAN_FEATURE_11AX) += -DWLAN_FEATURE_11AX_BSS_COLOR
cppflags-$(CONFIG_WLAN_FEATURE_11AX) += -DSUPPORT_11AX_D3
cppflags-$(CONFIG_RXDMA_ERR_PKT_DROP) += -DRXDMA_ERR_PKT_DROP
cppflags-$(CONFIG_MAX_ALLOC_PAGE_SIZE) += -DMAX_ALLOC_PAGE_SIZE
cppflags-$(CONFIG_DELIVERY_TO_STACK_STATUS_CHECK) += -DDELIVERY_TO_STACK_STATUS_CHECK
cppflags-$(CONFIG_WLAN_TRACE_HIDE_MAC_ADDRESS) += -DWLAN_TRACE_HIDE_MAC_ADDRESS
cppflags-$(CONFIG_WLAN_FEATURE_CAL_FAILURE_TRIGGER) += -DWLAN_FEATURE_CAL_FAILURE_TRIGGER

cppflags-$(CONFIG_LITHIUM) += -DFIX_TXDMA_LIMITATION
cppflags-$(CONFIG_LITHIUM) += -DFEATURE_AST
cppflags-$(CONFIG_LITHIUM) += -DPEER_PROTECTED_ACCESS
cppflags-$(CONFIG_LITHIUM) += -DSERIALIZE_QUEUE_SETUP
cppflags-$(CONFIG_LITHIUM) += -DDP_RX_PKT_NO_PEER_DELIVER
cppflags-$(CONFIG_LITHIUM) += -DDP_RX_DROP_RAW_FRM
cppflags-$(CONFIG_LITHIUM) += -DFEATURE_ALIGN_STATS_FROM_DP
cppflags-$(CONFIG_LITHIUM) += -DDP_RX_SPECIAL_FRAME_NEED
cppflags-$(CONFIG_LITHIUM) += -DFEATURE_STATS_EXT_V2
cppflags-$(CONFIG_VERBOSE_DEBUG) += -DENABLE_VERBOSE_DEBUG
cppflags-$(CONFIG_RX_DESC_DEBUG_CHECK) += -DRX_DESC_DEBUG_CHECK
cppflags-$(CONFIG_REGISTER_OP_DEBUG) += -DHAL_REGISTER_WRITE_DEBUG
cppflags-$(CONFIG_ENABLE_QDF_PTR_HASH_DEBUG) += -DENABLE_QDF_PTR_HASH_DEBUG
#Enable STATE MACHINE HISTORY
cppflags-$(CONFIG_SM_ENG_HIST) += -DSM_ENG_HIST_ENABLE
cppflags-$(CONFIG_FEATURE_VDEV_OPS_WAKELOCK) += -DFEATURE_VDEV_OPS_WAKELOCK

# Vendor Commands
cppflags-$(CONFIG_FEATURE_RSSI_MONITOR) += -DFEATURE_RSSI_MONITOR
cppflags-$(CONFIG_FEATURE_BSS_TRANSITION) += -DFEATURE_BSS_TRANSITION
cppflags-$(CONFIG_FEATURE_STATION_INFO) += -DFEATURE_STATION_INFO
cppflags-$(CONFIG_FEATURE_TX_POWER) += -DFEATURE_TX_POWER
cppflags-$(CONFIG_FEATURE_OTA_TEST) += -DFEATURE_OTA_TEST
cppflags-$(CONFIG_FEATURE_ACTIVE_TOS) += -DFEATURE_ACTIVE_TOS
cppflags-$(CONFIG_FEATURE_SAR_LIMITS) += -DFEATURE_SAR_LIMITS
cppflags-$(CONFIG_FEATURE_CONCURRENCY_MATRIX) += -DFEATURE_CONCURRENCY_MATRIX
cppflags-$(CONFIG_FEATURE_SAP_COND_CHAN_SWITCH) += -DFEATURE_SAP_COND_CHAN_SWITCH

#if converged p2p is enabled
ifeq ($(CONFIG_CONVERGED_P2P_ENABLE), y)
cppflags-$(CONFIG_FEATURE_P2P_LISTEN_OFFLOAD) += -DFEATURE_P2P_LISTEN_OFFLOAD
endif

#Enable support to get ANI value
ifeq ($(CONFIG_ANI_LEVEL_REQUEST), y)
cppflags-y += -DFEATURE_ANI_LEVEL_REQUEST
endif

#Flags to enable/disable WMI APIs
cppflags-$(CONFIG_WMI_ROAM_SUPPORT) += -DWMI_ROAM_SUPPORT
cppflags-$(CONFIG_WMI_CONCURRENCY_SUPPORT) += -DWMI_CONCURRENCY_SUPPORT
cppflags-$(CONFIG_WMI_STA_SUPPORT) += -DWMI_STA_SUPPORT

cppflags-y += -DWMI_MULTI_MAC_SVC

# Dummy flag for WIN/MCL converged data path compilation
cppflags-y += -DDP_PRINT_ENABLE=0
cppflags-y += -DATH_SUPPORT_WRAP=0
cppflags-y += -DQCA_HOST2FW_RXBUF_RING
cppflags-y += -DDP_FLOW_CTL
cppflags-y += -DDP_PEER_EXTENDED_API
cppflags-y += -DDP_POWER_SAVE
cppflags-y += -DDP_CON_MON
cppflags-y += -DDP_MOB_DEFS
cppflags-y += -DDP_PRINT_NO_CONSOLE
cppflags-y += -DDP_INTR_POLL_BOTH
cppflags-y += -DDP_INVALID_PEER_ASSERT

ifdef CONFIG_HIF_LARGE_CE_RING_HISTORY
ccflags-y += -DHIF_CE_HISTORY_MAX=$(CONFIG_HIF_LARGE_CE_RING_HISTORY)
endif

cppflags-$(CONFIG_WLAN_HANG_EVENT) += -DHIF_CE_LOG_INFO
cppflags-$(CONFIG_WLAN_HANG_EVENT) += -DHIF_BUS_LOG_INFO
cppflags-$(CONFIG_WLAN_HANG_EVENT) += -DDP_SUPPORT_RECOVERY_NOTIFY
#endof dummy flags

cppflags-y += -DMOTO_UTAGS_MAC

ccflags-$(CONFIG_ENABLE_SIZE_OPTIMIZE) += -Os

# DFS component
cppflags-$(CONFIG_WLAN_DFS_STATIC_MEM_ALLOC) += -DWLAN_DFS_STATIC_MEM_ALLOC
cppflags-$(CONFIG_WLAN_DFS_MASTER_ENABLE) += -DQCA_MCL_DFS_SUPPORT
ifeq ($(CONFIG_WLAN_FEATURE_DFS_OFFLOAD), y)
cppflags-$(CONFIG_WLAN_DFS_MASTER_ENABLE) += -DWLAN_DFS_FULL_OFFLOAD
else
cppflags-$(CONFIG_WLAN_DFS_MASTER_ENABLE) += -DWLAN_DFS_PARTIAL_OFFLOAD
endif
cppflags-$(CONFIG_WLAN_DFS_MASTER_ENABLE) += -DDFS_COMPONENT_ENABLE
cppflags-$(CONFIG_WLAN_DFS_MASTER_ENABLE) += -DQCA_DFS_USE_POLICY_MANAGER
cppflags-$(CONFIG_WLAN_DFS_MASTER_ENABLE) += -DQCA_DFS_NOL_PLATFORM_DRV_SUPPORT

cppflags-$(CONFIG_WLAN_DEBUGFS) += -DWLAN_DEBUGFS
cppflags-$(CONFIG_WLAN_STREAMFS) += -DWLAN_STREAMFS

cppflags-$(CONFIG_DYNAMIC_DEBUG) += -DFEATURE_MULTICAST_HOST_FW_MSGS

cppflags-$(CONFIG_ENABLE_SMMU_S1_TRANSLATION) += -DENABLE_SMMU_S1_TRANSLATION

#Flag to enable/disable Line number logging
cppflags-$(CONFIG_LOG_LINE_NUMBER) += -DLOG_LINE_NUMBER

#Flag to enable/disable MTRACE feature
cppflags-$(CONFIG_ENABLE_MTRACE_LOG) += -DENABLE_MTRACE_LOG

cppflags-$(CONFIG_FUNC_CALL_MAP) += -DFUNC_CALL_MAP

#Flag to enable/disable Adaptive 11r feature
cppflags-$(CONFIG_ADAPTIVE_11R) += -DWLAN_ADAPTIVE_11R

#Flag to enable/disable sae single pmk feature feature
cppflags-$(CONFIG_SAE_SINGLE_PMK) += -DWLAN_SAE_SINGLE_PMK

#Flag to enable/disable mscs feature
cppflags-$(CONFIG_FEATURE_MSCS) += -DWLAN_FEATURE_MSCS

#Flag to enable NUD tracking
cppflags-$(CONFIG_WLAN_NUD_TRACKING) += -DWLAN_NUD_TRACKING

#Flag to enable set and get disable channel list feature
cppflags-$(CONFIG_DISABLE_CHANNEL_LIST) += -DDISABLE_CHANNEL_LIST

#Flag to enable/disable WIPS feature
cppflags-$(CONFIG_WLAN_BCN_RECV_FEATURE) += -DWLAN_BCN_RECV_FEATURE

#Flag to enable/disable thermal mitigation
cppflags-$(CONFIG_FW_THERMAL_THROTTLE) += -DFW_THERMAL_THROTTLE

#Flag to enable/disable LTE COEX support
cppflags-$(CONFIG_LTE_COEX) += -DLTE_COEX

#Flag to enable/disable HOST_OPCLASS
cppflags-$(CONFIG_HOST_OPCLASS) += -DHOST_OPCLASS
cppflags-$(CONFIG_HOST_OPCLASS) += -DHOST_OPCLASS_EXT

#Flag to enable/disable TARGET_11D_SCAN
cppflags-$(CONFIG_TARGET_11D_SCAN) += -DTARGET_11D_SCAN

#Flag to enable/disable avoid acs frequency list feature
cppflags-$(CONFIG_SAP_AVOID_ACS_FREQ_LIST) += -DSAP_AVOID_ACS_FREQ_LIST

#Flag to enable Dynamic Voltage WDCVS (Config Voltage Mode)
cppflags-$(CONFIG_WLAN_DYNAMIC_CVM) += -DFEATURE_WLAN_DYNAMIC_CVM

#Flag to enable get firmware state feature
cppflags-$(CONFIG_QCACLD_FEATURE_FW_STATE) += -DFEATURE_FW_STATE

#Flag to enable set coex configuration feature
cppflags-$(CONFIG_QCACLD_FEATURE_COEX_CONFIG) += -DFEATURE_COEX_CONFIG

#Flag to enable MPTA helper feature
cppflags-$(CONFIG_QCACLD_FEATURE_MPTA_HELPER) += -DFEATURE_MPTA_HELPER

#Flag to enable get hw capability
cppflags-$(CONFIG_QCACLD_FEATURE_HW_CAPABILITY) += -DFEATURE_HW_CAPABILITY

#Flag to enable set btc chain mode feature
cppflags-$(CONFIG_QCACLD_FEATURE_BTC_CHAIN_MODE) += -DFEATURE_BTC_CHAIN_MODE

cppflags-$(CONFIG_DATA_CE_SW_INDEX_NO_INLINE_UPDATE) += -DDATA_CE_SW_INDEX_NO_INLINE_UPDATE

#Flag to enable Multi page memory allocation for RX descriptor pool
cppflags-$(CONFIG_QCACLD_RX_DESC_MULTI_PAGE_ALLOC) += -DRX_DESC_MULTI_PAGE_ALLOC

#Flag to enable SAR Safety Feature
cppflags-$(CONFIG_SAR_SAFETY_FEATURE) += -DSAR_SAFETY_FEATURE

cppflags-$(CONFIG_WLAN_FEATURE_DP_EVENT_HISTORY) += -DWLAN_FEATURE_DP_EVENT_HISTORY
cppflags-$(CONFIG_WLAN_FEATURE_DP_RX_RING_HISTORY) += -DWLAN_FEATURE_DP_RX_RING_HISTORY
cppflags-$(CONFIG_WLAN_FEATURE_DP_TX_DESC_HISTORY) += -DWLAN_FEATURE_DP_TX_DESC_HISTORY
cppflags-$(CONFIG_REO_QDESC_HISTORY) += -DREO_QDESC_HISTORY
cppflags-$(CONFIG_DP_TX_HW_DESC_HISTORY) += -DDP_TX_HW_DESC_HISTORY
ifdef CONFIG_QDF_NBUF_HISTORY_SIZE
ccflags-y += -DQDF_NBUF_HISTORY_SIZE=$(CONFIG_QDF_NBUF_HISTORY_SIZE)
endif
cppflags-$(CONFIG_WLAN_DP_PER_RING_TYPE_CONFIG) += -DWLAN_DP_PER_RING_TYPE_CONFIG
cppflags-$(CONFIG_WLAN_CE_INTERRUPT_THRESHOLD_CONFIG) += -DWLAN_CE_INTERRUPT_THRESHOLD_CONFIG
cppflags-$(CONFIG_SAP_DHCP_FW_IND) += -DSAP_DHCP_FW_IND
cppflags-$(CONFIG_WLAN_DP_PENDING_MEM_FLUSH) += -DWLAN_DP_PENDING_MEM_FLUSH
cppflags-$(CONFIG_WLAN_SUPPORT_DATA_STALL) += -DWLAN_SUPPORT_DATA_STALL
cppflags-$(CONFIG_WLAN_SUPPORT_TXRX_HL_BUNDLE) += -DWLAN_SUPPORT_TXRX_HL_BUNDLE
cppflags-$(CONFIG_QCN7605_PCIE_SHADOW_REG_SUPPORT) += -DQCN7605_PCIE_SHADOW_REG_SUPPORT
cppflags-$(CONFIG_MARK_ICMP_REQ_TO_FW) += -DWLAN_DP_FEATURE_MARK_ICMP_REQ_TO_FW
cppflags-$(CONFIG_WLAN_SKIP_BAR_UPDATE) += -DWLAN_SKIP_BAR_UPDATE

ifdef CONFIG_MAX_LOGS_PER_SEC
ccflags-y += -DWLAN_MAX_LOGS_PER_SEC=$(CONFIG_MAX_LOGS_PER_SEC)
endif

ifdef CONFIG_SCHED_HISTORY_SIZE
ccflags-y += -DWLAN_SCHED_HISTORY_SIZE=$(CONFIG_SCHED_HISTORY_SIZE)
endif

ifdef CONFIG_HANDLE_RX_REROUTE_ERR
cppflags-y += -DHANDLE_RX_REROUTE_ERR
endif

# configure log buffer size
ifdef CONFIG_CFG_NUM_DP_TRACE_RECORD
ccflags-y += -DMAX_QDF_DP_TRACE_RECORDS=$(CONFIG_CFG_NUM_DP_TRACE_RECORD)
endif

ifdef CONFIG_CFG_NUM_HTC_CREDIT_HISTORY
ccflags-y += -DHTC_CREDIT_HISTORY_MAX=$(CONFIG_CFG_NUM_HTC_CREDIT_HISTORY)
endif

ifdef CONFIG_CFG_NUM_WMI_EVENT_HISTORY
ccflags-y += -DWMI_EVENT_DEBUG_MAX_ENTRY=$(CONFIG_CFG_NUM_WMI_EVENT_HISTORY)
endif

ifdef CONFIG_CFG_NUM_WMI_MGMT_EVENT_HISTORY
ccflags-y += -DWMI_MGMT_EVENT_DEBUG_MAX_ENTRY=$(CONFIG_CFG_NUM_WMI_MGMT_EVENT_HISTORY)
endif

ifdef CONFIG_CFG_NUM_TX_RX_HISTOGRAM
ccflags-y += -DNUM_TX_RX_HISTOGRAM=$(CONFIG_CFG_NUM_TX_RX_HISTOGRAM)
endif

ifdef CONFIG_CFG_NUM_RX_IND_RECORD
ccflags-y += -DOL_RX_INDICATION_MAX_RECORDS=$(CONFIG_CFG_NUM_RX_IND_RECORD)
endif

ifdef CONFIG_CFG_NUM_ROAM_DEBUG_RECORD
ccflags-y += -DWLAN_ROAM_DEBUG_MAX_REC=$(CONFIG_CFG_NUM_ROAM_DEBUG_RECORD)
endif

ifdef CONFIG_CFG_PMO_WOW_FILTERS_MAX
ccflags-y += -DPMO_WOW_FILTERS_MAX=$(CONFIG_CFG_PMO_WOW_FILTERS_MAX)
endif

ifdef CONFIG_CFG_GTK_OFFLOAD_MAX_VDEV
ccflags-y += -DCFG_TGT_DEFAULT_GTK_OFFLOAD_MAX_VDEV=$(CONFIG_CFG_GTK_OFFLOAD_MAX_VDEV)
endif

ifdef CONFIG_TGT_NUM_MSDU_DESC
ccflags-y += -DCFG_TGT_NUM_MSDU_DESC=$(CONFIG_TGT_NUM_MSDU_DESC)
endif

ifdef CONFIG_HTC_MAX_MSG_PER_BUNDLE_TX
ccflags-y += -DCFG_HTC_MAX_MSG_PER_BUNDLE_TX=$(CONFIG_HTC_MAX_MSG_PER_BUNDLE_TX)
endif

ifdef CONFIG_CFG_ROAM_OFFLOAD_MAX_VDEV
ccflags-y += -DCFG_TGT_DEFAULT_ROAM_OFFLOAD_MAX_VDEV=$(CONFIG_CFG_ROAM_OFFLOAD_MAX_VDEV)
endif

ifdef CONFIG_CFG_MAX_PERIODIC_TX_PTRNS
ccflags-y += -DMAXNUM_PERIODIC_TX_PTRNS=$(CONFIG_CFG_MAX_PERIODIC_TX_PTRNS)
endif

ifdef CONFIG_CFG_MAX_STA_VDEVS
ccflags-y += -DCFG_TGT_DEFAULT_MAX_STA_VDEVS=$(CONFIG_CFG_MAX_STA_VDEVS)
endif

ifdef CONFIG_CFG_NUM_OF_ADDITIONAL_FW_PEERS
ccflags-y += -DNUM_OF_ADDITIONAL_FW_PEERS=$(CONFIG_CFG_NUM_OF_ADDITIONAL_FW_PEERS)
endif

ifdef CONFIG_CFG_NUM_OF_TDLS_CONN_TABLE_ENTRIES
ccflags-y += -DCFG_TGT_NUM_TDLS_CONN_TABLE_ENTRIES=$(CONFIG_CFG_NUM_OF_TDLS_CONN_TABLE_ENTRIES)
endif

ifdef CONFIG_CFG_TGT_AST_SKID_LIMIT
ccflags-y += -DCFG_TGT_AST_SKID_LIMIT=$(CONFIG_CFG_TGT_AST_SKID_LIMIT)
endif

ifdef CONFIG_TX_RESOURCE_HIGH_TH_IN_PER
ccflags-y += -DTX_RESOURCE_HIGH_TH_IN_PER=$(CONFIG_TX_RESOURCE_HIGH_TH_IN_PER)
endif

ifdef CONFIG_TX_RESOURCE_LOW_TH_IN_PER
ccflags-y += -DTX_RESOURCE_LOW_TH_IN_PER=$(CONFIG_TX_RESOURCE_LOW_TH_IN_PER)
endif

CONFIG_WLAN_MAX_PSOCS ?= 1
ccflags-y += -DWLAN_MAX_PSOCS=$(CONFIG_WLAN_MAX_PSOCS)

CONFIG_WLAN_MAX_PDEVS ?= 1
ccflags-y += -DWLAN_MAX_PDEVS=$(CONFIG_WLAN_MAX_PDEVS)

CONFIG_WLAN_MAX_VDEVS ?= 6
ccflags-y += -DWLAN_MAX_VDEVS=$(CONFIG_WLAN_MAX_VDEVS)

#Maximum pending commands for a vdev is calculated in vdev create handler
#by WLAN_SER_MAX_PENDING_CMDS/WLAN_SER_MAX_VDEVS. For SAP case, we will need
#to accommodate 32 Pending commands to handle multiple STA sending
#deauth/disassoc at the same time and for STA vdev,4 non scan pending commands
#are supported. So calculate WLAN_SER_MAX_PENDING_COMMANDS based on the SAP
#modes supported and no of STA vdev total non scan pending queue. Reserve
#additional 3 pending commands for WLAN_SER_MAX_PENDING_CMDS_AP to account for
#other commands like hardware mode change.

ifdef CONFIG_SIR_SAP_MAX_NUM_PEERS
CONFIG_WLAN_SER_MAX_PENDING_CMDS_AP ?=$(CONFIG_SIR_SAP_MAX_NUM_PEERS)
else
CONFIG_WLAN_SER_MAX_PENDING_CMDS_AP ?=32
endif
ccflags-y += -DWLAN_SER_MAX_PENDING_CMDS_AP=$(CONFIG_WLAN_SER_MAX_PENDING_CMDS_AP)+3

CONFIG_WLAN_SER_MAX_PENDING_CMDS_STA ?= 4
ccflags-y += -DWLAN_SER_MAX_PENDING_CMDS_STA=$(CONFIG_WLAN_SER_MAX_PENDING_CMDS_STA)

CONFIG_WLAN_MAX_PENDING_CMDS ?= $(CONFIG_WLAN_SER_MAX_PENDING_CMDS_AP)*3+$(CONFIG_WLAN_SER_MAX_PENDING_CMDS_STA)*2

ccflags-y += -DWLAN_SER_MAX_PENDING_CMDS=$(CONFIG_WLAN_MAX_PENDING_CMDS)

CONFIG_WLAN_PDEV_MAX_VDEVS ?= $(CONFIG_WLAN_MAX_VDEVS)
ccflags-y += -DWLAN_PDEV_MAX_VDEVS=$(CONFIG_WLAN_PDEV_MAX_VDEVS)

CONFIG_WLAN_PSOC_MAX_VDEVS ?= $(CONFIG_WLAN_MAX_VDEVS)
ccflags-y += -DWLAN_PSOC_MAX_VDEVS=$(CONFIG_WLAN_PSOC_MAX_VDEVS)

CONFIG_MAX_SCAN_CACHE_SIZE ?= 300
ccflags-y += -DMAX_SCAN_CACHE_SIZE=$(CONFIG_MAX_SCAN_CACHE_SIZE)
CONFIG_SCAN_MAX_REST_TIME ?= 0
ccflags-y += -DSCAN_MAX_REST_TIME=$(CONFIG_SCAN_MAX_REST_TIME)
CONFIG_SCAN_MIN_REST_TIME ?= 0
ccflags-y += -DSCAN_MIN_REST_TIME=$(CONFIG_SCAN_MIN_REST_TIME)
CONFIG_SCAN_BURST_DURATION ?= 0
ccflags-y += -DSCAN_BURST_DURATION=$(CONFIG_SCAN_BURST_DURATION)
CONFIG_SCAN_PROBE_SPACING_TIME ?= 0
ccflags-y += -DSCAN_PROBE_SPACING_TIME=$(CONFIG_SCAN_PROBE_SPACING_TIME)
CONFIG_SCAN_PROBE_DELAY ?= 0
ccflags-y += -DSCAN_PROBE_DELAY=$(CONFIG_SCAN_PROBE_DELAY)
CONFIG_SCAN_MAX_SCAN_TIME ?= 30000
ccflags-y += -DSCAN_MAX_SCAN_TIME=$(CONFIG_SCAN_MAX_SCAN_TIME)
CONFIG_SCAN_NETWORK_IDLE_TIMEOUT ?= 0
ccflags-y += -DSCAN_NETWORK_IDLE_TIMEOUT=$(CONFIG_SCAN_NETWORK_IDLE_TIMEOUT)
CONFIG_HIDDEN_SSID_TIME ?= 0xFFFFFFFF
ccflags-y += -DHIDDEN_SSID_TIME=$(CONFIG_HIDDEN_SSID_TIME)
CONFIG_SCAN_CHAN_STATS_EVENT_ENAB ?= false
ccflags-y += -DSCAN_CHAN_STATS_EVENT_ENAB=$(CONFIG_SCAN_CHAN_STATS_EVENT_ENAB)
CONFIG_MAX_BCN_PROBE_IN_SCAN_QUEUE ?= 150
ccflags-y += -DMAX_BCN_PROBE_IN_SCAN_QUEUE=$(CONFIG_MAX_BCN_PROBE_IN_SCAN_QUEUE)

#CONFIG_RX_DIAG_WQ_MAX_SIZE maximum number FW diag events that can be queued in
#FW diag events work queue. Host driver will discard the all diag events after
#this limit is reached.
#
# Value 0 represents no limit and any non zero value represents the maximum
# size of the work queue.
CONFIG_RX_DIAG_WQ_MAX_SIZE ?= 1000
ccflags-y += -DRX_DIAG_WQ_MAX_SIZE=$(CONFIG_RX_DIAG_WQ_MAX_SIZE)

CONFIG_MGMT_DESC_POOL_MAX ?= 64
ccflags-y += -DMGMT_DESC_POOL_MAX=$(CONFIG_MGMT_DESC_POOL_MAX)

ifdef CONFIG_SIR_SAP_MAX_NUM_PEERS
ccflags-y += -DSIR_SAP_MAX_NUM_PEERS=$(CONFIG_SIR_SAP_MAX_NUM_PEERS)
endif

ifdef CONFIG_BEACON_TX_OFFLOAD_MAX_VDEV
ccflags-y += -DCFG_TGT_DEFAULT_BEACON_TX_OFFLOAD_MAX_VDEV=$(CONFIG_BEACON_TX_OFFLOAD_MAX_VDEV)
endif

ifdef CONFIG_LIMIT_IPA_TX_BUFFER
ccflags-y += -DLIMIT_IPA_TX_BUFFER=$(CONFIG_LIMIT_IPA_TX_BUFFER)
endif

ifdef CONFIG_LOCK_STATS_ON
ccflags-y += -DQDF_LOCK_STATS=1
ccflags-y += -DQDF_LOCK_STATS_DESTROY_PRINT=0
ifneq ($(CONFIG_ARCH_SDXPRAIRIE), y)
ccflags-y += -DQDF_LOCK_STATS_BUG_ON=1
endif
ccflags-y += -DQDF_LOCK_STATS_LIST=1
ccflags-y += -DQDF_LOCK_STATS_LIST_SIZE=256
endif

ifdef CONFIG_FW_THERMAL_THROTTLE
ccflags-y += -DFW_THERMAL_THROTTLE_SUPPORT
endif

cppflags-y += -DFEATURE_NBUFF_REPLENISH_TIMER
cppflags-y += -DPEER_CACHE_RX_PKTS
cppflags-y += -DPCIE_REG_WINDOW_LOCAL_NO_CACHE

cppflags-y += -DSERIALIZE_VDEV_RESP
cppflags-y += -DTGT_IF_VDEV_MGR_CONV

cppflags-y += -DCONFIG_CHAN_NUM_API
cppflags-y += -DCONFIG_CHAN_FREQ_API

#Flag to enable/disable MCC specific feature regarding unallowed phymodes
cppflags-y += -DCHECK_REG_PHYMODE

cppflags-$(CONFIG_BAND_6GHZ) += -DCONFIG_BAND_6GHZ
cppflags-$(CONFIG_6G_SCAN_CHAN_SORT_ALGO) += -DFEATURE_6G_SCAN_CHAN_SORT_ALGO

cppflags-$(CONFIG_RX_FISA) += -DWLAN_SUPPORT_RX_FISA
cppflags-$(CONFIG_RX_FISA_HISTORY) += -DWLAN_SUPPORT_RX_FISA_HIST

cppflags-$(CONFIG_DP_SWLM) += -DWLAN_DP_FEATURE_SW_LATENCY_MGR

cppflags-$(CONFIG_RX_DEFRAG_DO_NOT_REINJECT) += -DRX_DEFRAG_DO_NOT_REINJECT

cppflags-$(CONFIG_HANDLE_BC_EAP_TX_FRM) += -DHANDLE_BROADCAST_EAPOL_TX_FRAME

cppflags-$(CONFIG_MORE_TX_DESC) += -DTX_TO_NPEERS_INC_TX_DESCS

ccflags-$(CONFIG_HASTINGS_BT_WAR) += -DHASTINGS_BT_WAR

cppflags-$(CONFIG_SLUB_DEBUG_ON) += -DHIF_CONFIG_SLUB_DEBUG_ON
cppflags-$(CONFIG_SLUB_DEBUG_ON) += -DHAL_CONFIG_SLUB_DEBUG_ON

ccflags-$(CONFIG_FOURTH_CONNECTION) += -DFEATURE_FOURTH_CONNECTION
ccflags-$(CONFIG_FOURTH_CONNECTION_AUTO) += -DFOURTH_CONNECTION_AUTO
ccflags-$(CONFIG_WMI_SEND_RECV_QMI) += -DWLAN_FEATURE_WMI_SEND_RECV_QMI

cppflags-$(CONFIG_WDI3_STATS_UPDATE) += -DWDI3_STATS_UPDATE
cppflags-$(CONFIG_WDI3_STATS_BW_MONITOR) += -DWDI3_STATS_BW_MONITOR

cppflags-$(CONFIG_WLAN_CUSTOM_DSCP_UP_MAP) += -DWLAN_CUSTOM_DSCP_UP_MAP
cppflags-$(CONFIG_WLAN_SEND_DSCP_UP_MAP_TO_FW) += -DWLAN_SEND_DSCP_UP_MAP_TO_FW

cppflags-$(CONFIG_SMMU_S1_UNMAP) += -DCONFIG_SMMU_S1_UNMAP
cppflags-$(CONFIG_HIF_CPU_PERF_AFFINE_MASK) += -DHIF_CPU_PERF_AFFINE_MASK

cppflags-$(CONFIG_GENERIC_SHADOW_REGISTER_ACCESS_ENABLE) += -DGENERIC_SHADOW_REGISTER_ACCESS_ENABLE

cppflags-$(CONFIG_DUMP_REO_QUEUE_INFO_IN_DDR) += -DDUMP_REO_QUEUE_INFO_IN_DDR

ifdef CONFIG_MAX_CLIENTS_ALLOWED
ccflags-y += -DWLAN_MAX_CLIENTS_ALLOWED=$(CONFIG_MAX_CLIENTS_ALLOWED)
endif

ifeq ($(CONFIG_WLAN_FEATURE_RX_BUFFER_POOL), y)
cppflags-y += -DWLAN_FEATURE_RX_PREALLOC_BUFFER_POOL
ifdef CONFIG_DP_RX_BUFFER_POOL_SIZE
ccflags-y += -DDP_RX_BUFFER_POOL_SIZE=$(CONFIG_DP_RX_BUFFER_POOL_SIZE)
endif
ifdef CONFIG_DP_RX_BUFFER_POOL_ALLOC_THRES
ccflags-y += -DDP_RX_BUFFER_POOL_ALLOC_THRES=$(CONFIG_DP_RX_BUFFER_POOL_ALLOC_THRES)
endif
endif

cppflags-$(CONFIG_DP_FT_LOCK_HISTORY) += -DDP_FT_LOCK_HISTORY

ccflags-$(CONFIG_GET_DRIVER_MODE) += -DFEATURE_GET_DRIVER_MODE

ifeq ($(CONFIG_SMP), y)
ifeq ($(CONFIG_HIF_DETECTION_LATENCY_ENABLE), y)
cppflags-y += -DHIF_DETECTION_LATENCY_ENABLE
cppflags-y += -DDETECTION_TIMER_TIMEOUT=4000
cppflags-y += -DDETECTION_LATENCY_THRESHOLD=3900
endif
endif

KBUILD_CPPFLAGS += $(cppflags-y)

# Currently, for versions of gcc which support it, the kernel Makefile
# is disabling the maybe-uninitialized warning.  Re-enable it for the
# WLAN driver.  Note that we must use ccflags-y here so that it
# will override the kernel settings.
ifeq ($(call cc-option-yn, -Wmaybe-uninitialized), y)
ccflags-y += -Wmaybe-uninitialized
ifneq (y,$(CONFIG_ARCH_MSM))
ccflags-y += -Wframe-larger-than=4096
endif
endif
ccflags-y += -Wmissing-prototypes

ifeq ($(call cc-option-yn, -Wheader-guard), y)
ccflags-y += -Wheader-guard
endif
# If the module name is not "wlan", then the define MULTI_IF_NAME to be the
# same a the QCA CHIP name. The host driver will then append MULTI_IF_NAME to
# any string that must be unique for all instances of the driver on the system.
# This allows multiple instances of the driver with different module names.
# If the module name is wlan, leave MULTI_IF_NAME undefined and the code will
# treat the driver as the primary driver.
#
# If DYNAMIC_SINGLE_CHIP is defined and MULTI_IF_NAME is undefined, which means
# there are multiple possible drivers, but only 1 driver will be loaded at
# a time(WLAN dynamic detect), no matter what the module name is, then
# host driver will only append DYNAMIC_SINGLE_CHIP to the path of
# firmware/mac/ini file.
#
# If DYNAMIC_SINGLE_CHIP & MULTI_IF_NAME defined, which means there are
# multiple possible drivers, we also can load multiple drivers together.
# And we can use DYNAMIC_SINGLE_CHIP to distingush the ko name, and use
# MULTI_IF_NAME to make cnss2 platform driver to figure out which wlanhost
# driver attached. Moreover, as the first priority, host driver will only
# append DYNAMIC_SINGLE_CHIP to the path of firmware/mac/ini file.

ifneq ($(DYNAMIC_SINGLE_CHIP),)
ccflags-y += -DDYNAMIC_SINGLE_CHIP=\"$(DYNAMIC_SINGLE_CHIP)\"
ifneq ($(MULTI_IF_NAME),)
ccflags-y += -DMULTI_IF_NAME=\"$(MULTI_IF_NAME)\"
endif
#
else

ifneq ($(MODNAME), wlan)
CHIP_NAME ?= $(MODNAME)
ccflags-y += -DMULTI_IF_NAME=\"$(CHIP_NAME)\"
endif

endif #DYNAMIC_SINGLE_CHIP

# WLAN_HDD_ADAPTER_MAGIC must be unique for all instances of the driver on the
# system. If it is not defined, then the host driver will use the first 4
# characters (including NULL) of MULTI_IF_NAME to construct
# WLAN_HDD_ADAPTER_MAGIC.
ifdef WLAN_HDD_ADAPTER_MAGIC
ccflags-y += -DWLAN_HDD_ADAPTER_MAGIC=$(WLAN_HDD_ADAPTER_MAGIC)
endif

# Max no of Serialization msg queue depth for MCL. If it is not
# defined, then SCHEDULER_CORE_MAX_MESSAGES will be 4000 for
# WIN.

ccflags-y += -DSCHEDULER_CORE_MAX_MESSAGES=1000

# Defining Reduction Limit 0 for MCL. If it is not defined,
#then WLAN_SCHED_REDUCTION_LIMIT will be 32 for
# WIN.
ccflags-y += -DWLAN_SCHED_REDUCTION_LIMIT=0

# Determine if we are building against an arm architecture host
ifeq ($(findstring arm, $(ARCH)),)
	ccflags-y += -DWLAN_HOST_ARCH_ARM=0
else
	ccflags-y += -DWLAN_HOST_ARCH_ARM=1
endif

# inject some build related information
ifeq ($(CONFIG_BUILD_TAG), y)
CLD_CHECKOUT = $(shell cd "$(WLAN_ROOT)" && \
	git reflog | grep -vm1 "}: cherry-pick: " | grep -oE ^[0-f]+)
CLD_IDS = $(shell cd "$(WLAN_ROOT)" && \
	git log -50 $(CLD_CHECKOUT)~..HEAD | \
		sed -nE 's/^\s*Change-Id: (I[0-f]{10})[0-f]{30}\s*$$/\1/p' | \
		paste -sd "," -)

CMN_CHECKOUT = $(shell cd "$(WLAN_COMMON_INC)" && \
	git reflog | grep -vm1 "}: cherry-pick: " | grep -oE ^[0-f]+)
CMN_IDS = $(shell cd "$(WLAN_COMMON_INC)" && \
	git log -50 $(CMN_CHECKOUT)~..HEAD | \
		sed -nE 's/^\s*Change-Id: (I[0-f]{10})[0-f]{30}\s*$$/\1/p' | \
		paste -sd "," -)
BUILD_TAG = "cld:$(CLD_IDS); cmn:$(CMN_IDS); dev:$(DEVNAME)"
ccflags-y += -DBUILD_TAG=\"$(BUILD_TAG)\"
endif

# Module information used by KBuild framework
obj-$(CONFIG_QCA_CLD_WLAN) += $(MODNAME).o
ifeq ($(CONFIG_WLAN_RESIDENT_DRIVER), y)
$(MODNAME)-y := $(HDD_SRC_DIR)/wlan_hdd_main_module.o
obj-$(CONFIG_QCA_CLD_WLAN) += wlan_resident.o
wlan_resident-y := $(OBJS)
else
$(MODNAME)-y := $(OBJS)
endif
OBJS_DIRS += $(dir $(OBJS)) \
	     $(WLAN_COMMON_ROOT)/$(HIF_CE_DIR)/ \
	     $(QDF_OBJ_DIR)/ \
	     $(WLAN_COMMON_ROOT)/$(HIF_PCIE_DIR)/ \
	     $(WLAN_COMMON_ROOT)/$(HIF_SNOC_DIR)/ \
	     $(WLAN_COMMON_ROOT)/$(HIF_SDIO_DIR)/
CLEAN_DIRS := $(addsuffix *.o,$(sort $(OBJS_DIRS))) \
	      $(addsuffix .*.o.cmd,$(sort $(OBJS_DIRS)))
clean-files := $(CLEAN_DIRS)
