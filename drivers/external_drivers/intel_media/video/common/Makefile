# Makefile for the drm device driver. This driver provides support for the
# Direct Rendering Infrastructure (DRI) in XFree86 4.1.0 and higher.

INCDIR=drivers/staging/mrfl
MEDIA_INCDIR=drivers/staging/intel_media
ccflags-$(CONFIG_DRM_MRFLD) += \
	-I$(INCDIR)/ \
	-I$(INCDIR)/rgx/include \
	-I$(INCDIR)/interface \
	-I$(INCDIR)/drv \
	-I$(INCDIR)/drv/ospm \
	-I$(INCDIR)/rgx/services/server/env/linux \
	-I$(INCDIR)/rgx/services/server/include \
	-I$(INCDIR)/../intel_media/video/common \
	-I$(INCDIR)/../intel_media/video/decode \
	-I$(INCDIR)/../intel_media/video/encode \
	-I$(INCDIR)/../intel_media/video/vsp \
	-I$(INCDIR)/../../../include/linux \
	-I$(INCDIR)/../../../include/drm \
	-I$(INCDIR)/../../../include/drm/ttm

ccflags-$(CONFIG_DRM_MRFLD) += -DANDROID -D_linux_ -DLINUX -D__KERNEL__  -DMERRIFIELD
ccflags-$(CONFIG_SUPPORT_VSP) += -DSUPPORT_VSP
ccflags-$(CONFIG_DRM_VXD_BYT) := -Iinclude/drm -Iinclude/drm/ttm -Idrivers/gpu/drm/i915 -Idrivers/staging/intel_media/common/baytrail -Idrivers/staging/intel_media/video/common -Idrivers/staging/intel_media/video/decode
#VIDEO_COMMON_DIR = $(TOP_DIR)/driver/staging/intel_media/video/common
#DECODE_DIR = $(TOP_DIR)/driver/staging/intel_media/video/decode

obj-y += \
	psb_ttm_glue.o \
	psb_cmdbuf.o \
	tng_securefw.o \
	psb_buffer.o \
	psb_fence.o \
	psb_mmu.o \
	psb_ttm_fence.o \
	psb_ttm_fence_user.o \
	psb_ttm_placement_user.o
