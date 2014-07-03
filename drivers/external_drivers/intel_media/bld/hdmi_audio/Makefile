DRIVER_NAME := hdmi_audio

MEDIADIR=../..
HDMI_AUDIO_SRC=$(MEDIADIR)/hdmi_audio

MEDIAINC=drivers/external_drivers/intel_media
PNWDISPLAYINC=$(MEDIAINC)/display/pnw/drv
TNGDISPLAYINC=$(MEDIAINC)/display/tng/drv
OTM_HDMI_INC=$(MEDIAINC)/otm_hdmi

include_dirs := \
	-I$(MEDIAINC)/common \
	-I$(MEDIAINC)/interface \
	-I$(OTM_HDMI_INC)/os/android/include \
	-I$(OTM_HDMI_INC)/pil/include \
	-I$(TNGDISPLAYINC)/ospm

ccflags-y += $(include_dirs)

ifdef CONFIG_DRM_MRFLD
ccflags-y += -I$(TNGDISPLAYINC)
else
ifdef CONFIG_DRM_I915
ccflags-y += -Idrivers/gpu/drm/i915
else
ccflags-y += -I$(PNWDISPLAYINC)
endif
endif

$(DRIVER_NAME)-objs += \
	$(HDMI_AUDIO_SRC)/intel_mid_hdmi_audio.o \
	$(HDMI_AUDIO_SRC)/intel_mid_hdmi_audio_if.o \
	$(HDMI_AUDIO_SRC)/intel_mid_hdmi_audio_debug.o

obj-m += $(DRIVER_NAME).o
