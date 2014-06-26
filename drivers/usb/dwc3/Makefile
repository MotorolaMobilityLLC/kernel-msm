ccflags-$(CONFIG_USB_DWC3_DEBUG)	:= -DDEBUG
ccflags-$(CONFIG_USB_DWC3_VERBOSE)	+= -DVERBOSE_DEBUG


obj-$(CONFIG_USB_DWC3_DEVICE_INTEL)		+= dwc3-device-intel.o
obj-$(CONFIG_USB_DWC3_INTEL_MRFL)		+= dwc3-intel-mrfl.o
obj-$(CONFIG_USB_DWC3_INTEL_BYT)		+= dwc3-intel-byt.o
obj-$(CONFIG_USB_DWC3_INTEL_CHT)		+= dwc3-intel-cht.o
ifneq ($(CONFIG_DEBUG_FS),)
	obj-$(CONFIG_USB_DWC3_DEVICE_INTEL)	+= debugfs.o
endif

ifeq ($(CONFIG_USB_DWC3_DEVICE_INTEL),)
obj-$(CONFIG_USB_DWC3)			+= dwc3.o

dwc3-y					:= core.o

ifneq ($(filter y,$(CONFIG_USB_DWC3_HOST) $(CONFIG_USB_DWC3_DUAL_ROLE)),)
	dwc3-y				+= host.o
endif

ifneq ($(filter y,$(CONFIG_USB_DWC3_GADGET) $(CONFIG_USB_DWC3_DUAL_ROLE)),)
	dwc3-y				+= gadget.o ep0.o
endif

ifneq ($(CONFIG_DEBUG_FS),)
	dwc3-y				+= debugfs.o
endif
endif

##
# Platform-specific glue layers go here
#
# NOTICE: Make sure your glue layer doesn't depend on anything
# which is arch-specific and that it compiles on all situations.
#
# We want to keep this requirement in order to be able to compile
# the entire driver (with all its glue layers) on several architectures
# and make sure it compiles fine. This will also help with allmodconfig
# and allyesconfig builds.
##


ifneq ($(CONFIG_PCI),)
	obj-$(CONFIG_USB_DWC3_OTG)	+= otg.o
endif
obj-$(CONFIG_USB_DWC3_OMAP)		+= dwc3-omap.o
obj-$(CONFIG_USB_DWC3_EXYNOS)		+= dwc3-exynos.o
obj-$(CONFIG_USB_DWC3_PCI)		+= dwc3-pci.o

