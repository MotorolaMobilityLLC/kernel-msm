DRIVER_NAME := tngdisp

include drivers/external_drivers/intel_media/bld/merrifield/rgx/Makefile

obj-$(CONFIG_DRM_INTEL_HANDSET) += $(DRIVER_NAME).o

MEDIADIR=../../..
DISPLAYDIR=$(MEDIADIR)/display/tng/drv
OTM_HDMI_SRC = $(MEDIADIR)/otm_hdmi
VIDEO_COMMON_DIR=$(MEDIADIR)/video/common
DECODE_DIR=$(MEDIADIR)/video/decode
ENCODE_DIR=$(MEDIADIR)/video/encode
VSP_DIR=$(MEDIADIR)/video/vsp

MEDIAINC=drivers/external_drivers/intel_media
GFXINC=$(MEDIAINC)/graphics/rgx
VIDEOINC=$(MEDIAINC)/video
DISPLAYINC=$(MEDIAINC)/display/tng
OTM_HDMI_INC=$(MEDIAINC)/otm_hdmi

ccflags-y += \
	-I$(MEDIAINC)/interface \
	-I$(DISPLAYINC)/bc_video \
	-I$(DISPLAYINC)/interface \
	-I$(DISPLAYINC)/drv \
	-I$(DISPLAYINC)/drv/ospm \
	-I$(GFXINC)/include \
	-I$(OTM_HDMI_INC)/os/android/include \
	-I$(OTM_HDMI_INC)/pil/include \
	-Iinclude/linux \
	-Iinclude/drm

ccflags-y += -DANDROID -DLINUX -D_linux_ -D__KERNEL__
ccflags-y += -DMERRIFIELD -DCONFIG_PCI_MSI
ccflags-y += -DCONFIG_SLICE_HEADER_PARSING
ccflags-y += -DCONFIG_VIDEO_MRFLD_EC

#
# HDMI
#

ccflags-$(CONFIG_SUPPORT_HDMI) += -DCONFIG_SUPPORT_HDMI

#
# HDMI PRIMARY devices
#
ccflags-$(CONFIG_SUPPORT_MIPI) += -DCONFIG_SUPPORT_MIPI

#only for internal testing
ccflags-$(CONFIG_SUPPORT_HDMI) += -DOTM_HDMI_UNIT_TEST

#enable HDCP
ccflags-$(CONFIG_SUPPORT_HDMI) += -DOTM_HDMI_HDCP_ENABLE
#ccflags-$(CONFIG_SUPPORT_HDMI) += -DOTM_HDMI_HDCP_ALWAYS_ENC

ccflags-$(CONFIG_SUPPORT_HDMI) += \
	-I$(OTM_HDMI_INC)/os/android/include \
	-I$(OTM_HDMI_INC)/pil/include \
	-I$(OTM_HDMI_INC)/pil/common \
	-I$(OTM_HDMI_INC)/pil/specific/include \
	-I$(OTM_HDMI_INC)/ipil/include \
	-I$(OTM_HDMI_INC)/ipil/common \
	-I$(OTM_HDMI_INC)/ipil/specific/include \
	-I$(OTM_HDMI_INC)/pil/specific/mrfld \
	-I$(OTM_HDMI_INC)/ipil/specific/mfld

#
# Video ccflags
#
ccflags-y += -DCONFIG_VIDEO_MRFLD -DSUPPORT_VSP
ccflags-y += \
 	-I$(VIDEOINC)/common \
	-I$(VIDEOINC)/decode \
	-I$(VIDEOINC)/encode \
	-I$(VIDEOINC)/vsp \
	-I$(GFXINC)/services/server/env/linux \
	-I$(GFXINC)/services/server/include \
	-Iinclude/drm/ttm

#
# Display
#
$(DRIVER_NAME)-objs += \
	$(DISPLAYDIR)/psb_bl.o \
	$(DISPLAYDIR)/psb_drv.o \
	$(DISPLAYDIR)/psb_fb.o \
	$(DISPLAYDIR)/psb_gtt.o \
	$(DISPLAYDIR)/psb_hotplug.o \
	$(DISPLAYDIR)/psb_intel_display.o \
	$(DISPLAYDIR)/mdfld_hdmi_audio.o \
	$(DISPLAYDIR)/mdfld_msic.o \
	$(DISPLAYDIR)/mdfld_debugfs.o \
	$(DISPLAYDIR)/mdfld_csc.o \
	$(DISPLAYDIR)/mdfld_intel_hdcp.o \
	$(DISPLAYDIR)/psb_intel_hdmi.o \
	$(DISPLAYDIR)/psb_socket.o \
	$(DISPLAYDIR)/psb_umevents.o \
	$(DISPLAYDIR)/mdfld_output.o \
	$(DISPLAYDIR)/mrfld_clock.o \
	$(DISPLAYDIR)/psb_irq.o \
	$(DISPLAYDIR)/tng_wa.o \
	$(DISPLAYDIR)/pmu_tng.o \
	$(DISPLAYDIR)/ospm/pwr_mgmt.o \
	$(DISPLAYDIR)/ospm/gfx_rtpm.o \
	$(DISPLAYDIR)/ospm/dc_ospm.o \
	$(DISPLAYDIR)/ospm/dc_maxfifo.o \
	$(DISPLAYDIR)/ospm/video_ospm.o \
	$(DISPLAYDIR)/ospm/early_suspend.o \
	$(DISPLAYDIR)/ospm/early_suspend_sysfs.o

ifeq ($(CONFIG_SUPPORT_MIPI),y)
$(DRIVER_NAME)-objs += \
	$(DISPLAYDIR)/psb_dpst.o \
	$(DISPLAYDIR)/psb_dpst_func.o \
	$(DISPLAYDIR)/mdfld_dsi_dbi.o \
	$(DISPLAYDIR)/mdfld_dsi_dpi.o \
	$(DISPLAYDIR)/mdfld_dsi_output.o \
	$(DISPLAYDIR)/mdfld_dsi_esd.o \
	$(DISPLAYDIR)/mdfld_dsi_dbi_dsr.o \
	$(DISPLAYDIR)/dispmgrnl.o \
	$(DISPLAYDIR)/mdfld_dsi_pkg_sender.o \
	$(DISPLAYDIR)/jdi_vid.o \
	$(DISPLAYDIR)/jdi_cmd.o \
	$(DISPLAYDIR)/cmi_vid.o \
	$(DISPLAYDIR)/cmi_cmd.o \
	$(DISPLAYDIR)/sharp10x19_cmd.o \
	$(DISPLAYDIR)/sharp25x16_vid.o \
	$(DISPLAYDIR)/sharp25x16_cmd.o \
	$(DISPLAYDIR)/sdc16x25_8_cmd.o \
	$(DISPLAYDIR)/sdc25x16_cmd.o \
	$(DISPLAYDIR)/jdi25x16_vid.o \
	$(DISPLAYDIR)/jdi25x16_cmd.o
endif

ifeq ($(CONFIG_MOOREFIELD),y)
$(DRIVER_NAME)-objs += \
	$(DISPLAYDIR)/ospm/gfx_ospm_ann.o \
	$(DISPLAYDIR)/mofd_dc_callbacks.o
else
$(DRIVER_NAME)-objs += \
	$(DISPLAYDIR)/ospm/gfx_ospm.o \
	$(DISPLAYDIR)/dc_callbacks.o
endif

ifeq ($(CONFIG_SUPPORT_HDMI),y)
# Platform independent library
$(DRIVER_NAME)-objs += \
	$(OTM_HDMI_SRC)/pil/common/otm_hdmi.o \
	$(OTM_HDMI_SRC)/pil/common/mode_info.o \
	$(OTM_HDMI_SRC)/pil/common/hdcp.o \
	$(OTM_HDMI_SRC)/pil/common/edid.o \
	$(OTM_HDMI_SRC)/pil/common/edid_print.o \
	$(OTM_HDMI_SRC)/pil/common/infoframes.o

# IP independent library
$(DRIVER_NAME)-objs += \
	$(OTM_HDMI_SRC)/ipil/common/otm_ipil_main.o \
	$(OTM_HDMI_SRC)/ipil/common/ipil_hdcp.o

# OS specific library
$(DRIVER_NAME)-objs += \
	$(OTM_HDMI_SRC)/os/android/android_hdmi.o
	
$(DRIVER_NAME)-objs += \
	$(OTM_HDMI_SRC)/ipil/specific/mfld/ips_hdmi.o \
	$(OTM_HDMI_SRC)/ipil/specific/mrfld/ips_hdcp.o \
	$(OTM_HDMI_SRC)/ipil/specific/mrfld/ips_hdmi_priv.o

ifeq ($(CONFIG_MOOREFIELD),y)
$(DRIVER_NAME)-objs += \
	$(OTM_HDMI_SRC)/pil/specific/mofd/ps_hdmi.o
else
$(DRIVER_NAME)-objs += \
	$(OTM_HDMI_SRC)/pil/specific/mrfld/ps_hdmi.o
endif

endif

$(DRIVER_NAME)-objs += \
	$(VIDEO_COMMON_DIR)/psb_ttm_glue.o \
	$(VIDEO_COMMON_DIR)/psb_cmdbuf.o \
	$(VIDEO_COMMON_DIR)/tng_securefw.o \
	$(VIDEO_COMMON_DIR)/psb_buffer.o \
	$(VIDEO_COMMON_DIR)/psb_fence.o \
	$(VIDEO_COMMON_DIR)/psb_mmu.o \
	$(VIDEO_COMMON_DIR)/psb_ttm_fence.o \
	$(VIDEO_COMMON_DIR)/psb_ttm_fence_user.o \
	$(VIDEO_COMMON_DIR)/psb_ttm_placement_user.o \
	$(DECODE_DIR)/psb_msvdx.o \
	$(DECODE_DIR)/psb_msvdx_ec.o \
	$(DECODE_DIR)/psb_msvdxinit.o \
	$(DECODE_DIR)/psb_msvdx_fw.o \
	$(ENCODE_DIR)/tng_topaz.o \
	$(ENCODE_DIR)/tng_topazinit.o \
	$(VSP_DIR)/vsp.o \
	$(VSP_DIR)/vsp_init.o
