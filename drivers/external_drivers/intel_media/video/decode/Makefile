# Makefile for the drm device driver. This driver provides support for the
# Direct Rendering Infrastructure (DRI) in XFree86 4.1.0 and higher.

INCDIR=drivers/staging/mrfl
MEDIA_INCDIR=drivers/staging/intel_media
ccflags-$(CONFIG_DRM_MRFLD) += \
	-I$(INCDIR)/ \
	-I$(INCDIR)/rgx/include \
	-I$(INCDIR)/interface \
	-I$(INCDIR)/drv \
	-I$(INCDIR)/../intel_media/video/common \
	-I$(INCDIR)/../intel_media/video/decode \
	-I$(INCDIR)/../../../include/linux \
	-I$(INCDIR)/../../../include/drm \
	-I$(INCDIR)/../../../include/drm/ttm \
	-I$(INCDIR)/../tng/drv/ospm

ccflags-$(CONFIG_DRM_MRFLD) += -DANDROID -D_linux_ -DLINUX -D__KERNEL__ -DMERRIFIELD -DCONFIG_VIDEO_MRFLD
ccflags-$(CONFIG_DRM_VXD_BYT) := -Iinclude/drm -Iinclude/drm/ttm -Idrivers/gpu/drm/i915 -Idrivers/staging/intel_media/common/baytrail -Idrivers/staging/intel_media/video/common -Idrivers/staging/intel_media/video/decode
#VIDEO_COMMON_DIR = $(TOP_DIR)/driver/staging/intel_media/video/common
#DECODE_DIR = $(TOP_DIR)/driver/staging/intel_media/video/decode

obj-y += \
	psb_msvdx.o \
	psb_msvdxinit.o \
	psb_msvdx_fw.o

