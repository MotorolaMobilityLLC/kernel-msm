# Makefile for the drm device driver. This driver provides support for the
# Direct Rendering Infrastructure (DRI) in XFree86 4.1.0 and higher.

INCDIR=drivers/staging/mrfl
MEDIA_INCDIR=drivers/staging/intel_media
ccflags-y += \
	-I$(INCDIR)/ \
	-I$(INCDIR)/rgx/include \
	-I$(INCDIR)/interface \
	-I$(INCDIR)/drv \
	-I$(INCDIR)/drv/ospm \
	-I$(INCDIR)/../intel_media/video/common \
	-I$(INCDIR)/../intel_media/video/vsp \
	-I$(INCDIR)/../../../include/linux \
	-I$(INCDIR)/../../../include/drm \
	-I$(INCDIR)/../../../include/drm/ttm

ccflags-y += -DANDROID -D_linux_ -DLINUX -D__KERNEL__ -DSUPPORT_VSP -DMERRIFIELD -DCONFIG_VIDEO_MRFLD

obj-y += \
	vsp.o \
	vsp_init.o

