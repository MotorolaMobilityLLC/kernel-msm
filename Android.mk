# Android makefile for the WLAN Module

# Assume no targets will be supported
WLAN_CHIPSET :=

# Build/Package options for 8960 target
ifeq ($(call is-board-platform,msm8960),true)
WLAN_CHIPSET := prima
WLAN_SELECT := CONFIG_PRIMA_WLAN=m
endif

# Build/Package options for 8974, 8226, 8610 targets
ifeq ($(call is-board-platform-in-list,msm8974 msm8226 msm8610),true)
WLAN_CHIPSET := pronto
WLAN_SELECT := CONFIG_PRONTO_WLAN=m
endif

# Build/Package only in case of supported target
ifneq ($(WLAN_CHIPSET),)

LOCAL_PATH := $(call my-dir)

# This makefile is only for DLKM
ifneq ($(findstring vendor,$(LOCAL_PATH)),)

# Determine if we are Proprietary or Open Source
ifneq ($(findstring opensource,$(LOCAL_PATH)),)
    WLAN_PROPRIETARY := 0
else
    WLAN_PROPRIETARY := 1
endif

ifeq ($(WLAN_PROPRIETARY),1)
    WLAN_BLD_DIR := vendor/qcom/proprietary/wlan
else
    WLAN_BLD_DIR := vendor/qcom/opensource/wlan
endif

# DLKM_DIR was moved for JELLY_BEAN (PLATFORM_SDK 16)
ifeq ($(call is-platform-sdk-version-at-least,16),true)
       DLKM_DIR := $(TOP)/device/qcom/common/dlkm
WLAN_BLD_DIR := $(call my-dir)

ifeq ($(BOARD_HAS_QCOM_WLAN), true)
#Build/Package Libra Mono only in case of 7627 target
#ifeq ($(call is-chipset-in-board-platform,msm7627),true)
#        include $(WLAN_BLD_DIR)/libra/CORE/HDD/src/Android.mk
#        include $(WLAN_BLD_DIR)/utils/ptt/Android.mk
#        include $(WLAN_BLD_DIR)/utils/asf/src/Android.mk
#endif

#Build/Package Libra and Volans Module in case of msm7x30_surf target
ifeq ($(call is-board-platform,msm7630_surf),true)
        include $(WLAN_BLD_DIR)/volans/CORE/HDD/src/Android.mk
#       include $(WLAN_BLD_DIR)/libra/CORE/HDD/src/Android.mk
#        include $(WLAN_BLD_DIR)/utils/ptt/Android.mk
#        include $(WLAN_BLD_DIR)/utils/asf/src/Android.mk
endif

#Build/Package Volans Module only in case of 8660 target variants
ifeq ($(call is-board-platform,msm8660),true)
        include $(WLAN_BLD_DIR)/volans/CORE/HDD/src/Android.mk
#        include $(WLAN_BLD_DIR)/utils/ptt/Android.mk
#        include $(WLAN_BLD_DIR)/utils/asf/src/Android.mk
endif

#Build/Package Volans Module only in case of 7627a target
ifeq ($(call is-board-platform,msm7627a),true)
        include $(WLAN_BLD_DIR)/volans/CORE/HDD/src/Android.mk
#        include $(WLAN_BLD_DIR)/utils/ptt/Android.mk
#        include $(WLAN_BLD_DIR)/utils/asf/src/Android.mk
endif

#Build/Package CLD Module only in case of apq8084 target
ifeq ($(call is-board-platform,apq8084),true)
        include $(WLAN_BLD_DIR)/qcacld-new/Android.mk
        include $(WLAN_BLD_DIR)/qcacld-new/tools/athdiag/Android.mk
        include $(WLAN_BLD_DIR)/qcacld-new/tools/fwdebuglog/Android.mk
        include $(WLAN_BLD_DIR)/qcacld-new/tools/pktlog/Android.mk
else
       DLKM_DIR := build/dlkm
endif

# Some kernel include files are being moved.  Check to see if
# the old version of the files are present
INCLUDE_SELECT :=
ifneq ($(wildcard $(TOP)/kernel//arch/arm/mach-msm/include/mach/msm_smd.h),)
        INCLUDE_SELECT += EXISTS_MSM_SMD=1
endif

ifneq ($(wildcard $(TOP)/kernel//arch/arm/mach-msm/include/mach/msm_smsm.h),)
        INCLUDE_SELECT += EXISTS_MSM_SMSM=1
endif

ifeq ($(WLAN_PROPRIETARY),1)
# For the proprietary driver the firmware files are handled here
include $(CLEAR_VARS)
LOCAL_MODULE       := WCNSS_qcom_wlan_nv.bin
LOCAL_MODULE_TAGS  := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH  := $(PRODUCT_OUT)/persist
LOCAL_SRC_FILES    := firmware_bin/$(LOCAL_MODULE)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE       := WCNSS_cfg.dat
LOCAL_MODULE_TAGS  := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH  := $(TARGET_OUT_ETC)/firmware/wlan/prima
LOCAL_SRC_FILES    := firmware_bin/$(LOCAL_MODULE)
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE       := WCNSS_qcom_cfg.ini
LOCAL_MODULE_TAGS  := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH  := $(PRODUCT_OUT)/persist
LOCAL_SRC_FILES    := firmware_bin/$(LOCAL_MODULE)
include $(BUILD_PREBUILT)

endif

# Build wlan.ko as either prima_wlan.ko or pronto_wlan.ko
###########################################################

# This is set once per LOCAL_PATH, not per (kernel) module
KBUILD_OPTIONS := WLAN_ROOT=../$(WLAN_BLD_DIR)/prima
# We are actually building wlan.ko here, as per the
# requirement we are specifying <chipset>_wlan.ko as LOCAL_MODULE.
# This means we need to rename the module to <chipset>_wlan.ko
# after wlan.ko is built.
KBUILD_OPTIONS += MODNAME=wlan
KBUILD_OPTIONS += BOARD_PLATFORM=$(TARGET_BOARD_PLATFORM)
KBUILD_OPTIONS += $(WLAN_SELECT)
KBUILD_OPTIONS += $(INCLUDE_SELECT)

include $(CLEAR_VARS)
LOCAL_MODULE              := $(WLAN_CHIPSET)_wlan.ko
LOCAL_MODULE_KBUILD_NAME  := wlan.ko
LOCAL_MODULE_TAGS         := debug
LOCAL_MODULE_DEBUG_ENABLE := true
LOCAL_MODULE_PATH         := $(TARGET_OUT)/lib/modules/$(WLAN_CHIPSET)
include $(DLKM_DIR)/AndroidKernelModule.mk
###########################################################

#Create symbolic link
$(shell mkdir -p $(TARGET_OUT)/lib/modules; \
        ln -sf /system/lib/modules/$(WLAN_CHIPSET)/$(WLAN_CHIPSET)_wlan.ko \
               $(TARGET_OUT)/lib/modules/wlan.ko)

ifeq ($(WLAN_PROPRIETARY),1)
$(shell mkdir -p $(TARGET_OUT_ETC)/firmware/wlan/prima; \
        ln -sf /persist/WCNSS_qcom_wlan_nv.bin \
        $(TARGET_OUT_ETC)/firmware/wlan/prima/WCNSS_qcom_wlan_nv.bin; \
        ln -sf /data/misc/wifi/WCNSS_qcom_cfg.ini \
        $(TARGET_OUT_ETC)/firmware/wlan/prima/WCNSS_qcom_cfg.ini)
endif

endif # DLKM check

endif # supported target check
