# Android makefile for the WLAN Module

# Assume no targets will be supported
WLAN_CHIPSET :=

# Build/Package options for 8960 target
ifeq ($(call is-board-platform,msm8960),true)
WLAN_CHIPSET := prima
WLAN_SELECT := CONFIG_PRIMA_WLAN=m
endif

# Build/Package options for 8974, 8226, 8610 targets
ifeq ($(call is-board-platform-in-list,msm8916 msm8974 msm8226 msm8610),true)
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
    WLAN_BLD_DIR := vendor/qcom/proprietary/wlan-noship
else
    WLAN_BLD_DIR := vendor/qcom/opensource/wlan
endif

# DLKM_DIR was moved for JELLY_BEAN (PLATFORM_SDK 16)
ifeq ($(call is-platform-sdk-version-at-least,16),true)
       DLKM_DIR := $(TOP)/device/qcom/common/dlkm
else
       DLKM_DIR := build/dlkm
endif

# Copy WCNSS_cfg.dat file from firmware_bin/ folder to target out directory.
ifeq ($(WLAN_PROPRIETARY),0)

$(shell rm -f $(TARGET_OUT_ETC)/firmware/wlan/prima/WCNSS_cfg.dat)
$(shell cp $(LOCAL_PATH)/firmware_bin/WCNSS_cfg.dat $(TARGET_OUT_ETC)/firmware/wlan/prima)

else

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

include $(CLEAR_VARS)
LOCAL_MODULE              := proprietary_$(WLAN_CHIPSET)_wlan.ko
LOCAL_MODULE_KBUILD_NAME  := wlan.ko
LOCAL_MODULE_TAGS         := optional
LOCAL_MODULE_DEBUG_ENABLE := true
LOCAL_MODULE_PATH         := $(TARGET_OUT)/lib/modules/$(WLAN_CHIPSET)
include $(DLKM_DIR)/AndroidKernelModule.mk
###########################################################

endif # DLKM check

endif # supported target check
