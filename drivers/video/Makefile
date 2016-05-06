obj-$(CONFIG_MSM_BA_V4L2) += msm/ba/
obj-$(CONFIG_VGASTATE)            += vgastate.o
obj-$(CONFIG_HDMI)                += hdmi.o

obj-$(CONFIG_ADF)		  += adf/
obj-$(CONFIG_VT)		  += console/
obj-$(CONFIG_LOGO)		  += logo/
obj-y				  += backlight/

obj-y				  += fbdev/
obj-y				  += slimport/

obj-$(CONFIG_VIDEOMODE_HELPERS) += display_timing.o videomode.o
ifeq ($(CONFIG_OF),y)
obj-$(CONFIG_VIDEOMODE_HELPERS) += of_display_timing.o of_videomode.o
endif
obj-$(CONFIG_PANEL_NOTIFICATIONS) += panel_notifier.o
