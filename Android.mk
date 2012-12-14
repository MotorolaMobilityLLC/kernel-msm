# Android makefile for the WLAN WCNSS/Prima Module

# Build/Package only in case of 8960 target
ifeq ($(call is-board-platform,msm8960),true)

LOCAL_PATH := $(call my-dir)

# This makefile is only for DLKM
ifneq ($(findstring vendor,$(LOCAL_PATH)),)

WLAN_BLD_DIR := vendor/qcom/opensource/wlan

ifeq ($(call is-android-codename,JELLY_BEAN),true)
       DLKM_DIR := $(TOP)/device/qcom/common/dlkm
else
       DLKM_DIR := build/dlkm
endif


# Build prima_wlan.ko
###########################################################

# This is set once per LOCAL_PATH, not per (kernel) module
KBUILD_OPTIONS := WLAN_PRIMA=../$(WLAN_BLD_DIR)/prima
# We are actually building wlan.ko here, as per the
# requirement we are specifying prima_wlan.ko as LOCAL_MODULE.
# This means we need to rename the module to prima_wlan.ko
# after wlan.ko is built.
KBUILD_OPTIONS += MODNAME=wlan
KBUILD_OPTIONS += BOARD_PLATFORM=$(TARGET_BOARD_PLATFORM)

include $(CLEAR_VARS)
LOCAL_MODULE              := prima_wlan.ko
LOCAL_MODULE_KBUILD_NAME  := wlan.ko
LOCAL_MODULE_TAGS         := debug
LOCAL_MODULE_DEBUG_ENABLE := true
LOCAL_MODULE_PATH         := $(TARGET_OUT)/lib/modules/prima
include $(DLKM_DIR)/AndroidKernelModule.mk
###########################################################

#Create symbolic link
$(shell mkdir -p $(TARGET_OUT)/lib/modules; \
        ln -sf /system/lib/modules/prima/prima_wlan.ko \
               $(TARGET_OUT)/lib/modules/wlan.ko)

endif

endif # 8960 target check
