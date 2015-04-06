# Android makefile for the WLAN Module

# Assume no targets will be supported

BOARD_HAS_QCOM_WLAN := true

TARGET_KERNEL_ARCH := arm64
TARGET_KERNEL_CROSS_COMPILE_PREFIX := aarch64-linux-android-
TARGET_USES_UNCOMPRESSED_KERNEL := true
TARGET_KERNEL_APPEND_DTB := true


ifeq ($(BOARD_HAS_QCOM_WLAN), true)
# Build/Package options for 8084/8092/8960/8992/8994 target
WLAN_CHIPSET     := qca_cld
WLAN_SELECT      := CONFIG_QCA_CLD_WLAN=m

# Build/Package only in case of supported target
ifneq ($(WLAN_CHIPSET),)

LOCAL_PATH := $(call my-dir)

# This makefile is only for DLKM
ifneq ($(findstring vendor,$(LOCAL_PATH)),)

# Determine if we are Proprietary or Open Source
ifneq ($(findstring opensource,$(LOCAL_PATH)),)
    WLAN_PROPRIETARY := 0
    WLAN_OPEN_SOURCE := 1
else
    WLAN_PROPRIETARY := 1
    WLAN_OPEN_SOURCE := 0
endif

ifeq ($(WLAN_PROPRIETARY),1)
    WLAN_BLD_DIR := vendor/qcom/proprietary/wlan-noship
else
    WLAN_BLD_DIR := vendor/qcom/opensource/wlan
endif

       DLKM_DIR := device/lge/bullhead/dlkm

# Copy WCNSS_cfg.dat and WCNSS_qcom_cfg.ini file from firmware_bin/ folder to target out directory.
ifeq ($(call is-board-platform-in-list, msm8992),true)
$(shell rm -f $(TARGET_OUT_ETC)/firmware/wlan/qca_cld/WCNSS_cfg.dat)
$(shell rm -f $(TARGET_OUT_ETC)/firmware/wlan/qca_cld/WCNSS_qcom_cfg.ini)
$(shell mkdir -p $(TARGET_OUT_ETC)/firmware/wlan/qca_cld)
$(shell cp $(LOCAL_PATH)/firmware_bin/WCNSS_cfg.dat $(TARGET_OUT_ETC)/firmware/wlan/qca_cld)
$(shell cp $(LOCAL_PATH)/firmware_bin/WCNSS_qcom_cfg.ini $(TARGET_OUT_ETC)/firmware/wlan/qca_cld)
endif

###########################################################
# This is set once per LOCAL_PATH, not per (kernel) module
KBUILD_OPTIONS := WLAN_ROOT=../$(WLAN_BLD_DIR)/qcacld-2.0
# We are actually building wlan.ko here, as per the
# requirement we are specifying <chipset>_wlan.ko as LOCAL_MODULE.
# This means we need to rename the module to <chipset>_wlan.ko
# after wlan.ko is built.
KBUILD_OPTIONS += MODNAME=wlan
KBUILD_OPTIONS += BOARD_PLATFORM=$(TARGET_BOARD_PLATFORM)
KBUILD_OPTIONS += $(WLAN_SELECT)
KBUILD_OPTIONS += WLAN_OPEN_SOURCE=$(WLAN_OPEN_SOURCE)

include $(CLEAR_VARS)
LOCAL_MODULE              := $(WLAN_CHIPSET)_wlan.ko
LOCAL_MODULE_KBUILD_NAME  := wlan.ko
LOCAL_MODULE_TAGS         := debug
LOCAL_MODULE_DEBUG_ENABLE := true
LOCAL_MODULE_PATH         := $(TARGET_OUT)/lib/modules/$(WLAN_CHIPSET)
include $(DLKM_DIR)/AndroidKernelModule.mk
###########################################################

# Create Symbolic link for built <WLAN_CHIPSET>_wlan.ko driver from
# standard module location.
# TO-DO: This step needs to be moved to a post-build make target instead
# TO-DO: as this may run multiple times
$(shell mkdir -p $(TARGET_OUT)/lib/modules; \
    ln -sf /system/lib/modules/$(WLAN_CHIPSET)/$(WLAN_CHIPSET)_wlan.ko \
           $(TARGET_OUT)/lib/modules/wlan.ko)
$(shell ln -sf /persist/wlan_mac.bin $(TARGET_OUT_ETC)/firmware/wlan/qca_cld/wlan_mac.bin)

ifeq ($(call is-board-platform-in-list, msm8960),true)
$(shell ln -sf /firmware/image/bdwlan20.bin $(TARGET_OUT_ETC)/firmware/fakeboar.bin)
$(shell ln -sf /firmware/image/otp20.bin $(TARGET_OUT_ETC)/firmware/otp.bin)
$(shell ln -sf /firmware/image/utf20.bin $(TARGET_OUT_ETC)/firmware/utf.bin)
$(shell ln -sf /firmware/image/qwlan20.bin $(TARGET_OUT_ETC)/firmware/athwlan.bin)

$(shell ln -sf /firmware/image/bdwlan20.bin $(TARGET_OUT_ETC)/firmware/bdwlan20.bin)
$(shell ln -sf /firmware/image/otp20.bin $(TARGET_OUT_ETC)/firmware/otp20.bin)
$(shell ln -sf /firmware/image/utf20.bin $(TARGET_OUT_ETC)/firmware/utf20.bin)
$(shell ln -sf /firmware/image/qwlan20.bin $(TARGET_OUT_ETC)/firmware/qwlan20.bin)

$(shell ln -sf /firmware/image/bdwlan30.bin $(TARGET_OUT_ETC)/firmware/bdwlan30.bin)
$(shell ln -sf /firmware/image/otp30.bin $(TARGET_OUT_ETC)/firmware/otp30.bin)
$(shell ln -sf /firmware/image/utf30.bin $(TARGET_OUT_ETC)/firmware/utf30.bin)
$(shell ln -sf /firmware/image/qwlan30.bin $(TARGET_OUT_ETC)/firmware/qwlan30.bin)
endif

# Copy config ini files to target
ifeq ($(call is-board-platform-in-list, msm8994),false)
ifeq ($(WLAN_PROPRIETARY),1)
$(shell mkdir -p $(TARGET_OUT)/etc/firmware/wlan/$(WLAN_CHIPSET))
$(shell mkdir -p $(TARGET_OUT)/etc/wifi)
$(shell rm -f $(TARGET_OUT)/etc/wifi/WCNSS_qcom_cfg.ini)
$(shell rm -f $(TARGET_OUT)/etc/firmware/wlan/$(WLAN_SHIPSET)/WCNSS_cfg.dat)
$(shell cp $(LOCAL_PATH)/firmware_bin/WCNSS_qcom_cfg.ini $(TARGET_OUT)/etc/wifi)
$(shell cp $(LOCAL_PATH)/firmware_bin/WCNSS_cfg.dat $(TARGET_OUT)/etc/firmware/wlan/$(WLAN_CHIPSET))
endif
endif

endif # DLKM check

endif # supported target check
endif # WLAN enabled check
