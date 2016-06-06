# Required flag to configure the core to operate with the Linux kernel
ccflags-$(CONFIG_FUSB302) += -DFSC_PLATFORM_LINUX
# Optional flag to enable interrupt-driven operation
ccflags-$(CONFIG_FUSB302) += -DFSC_INTERRUPT_TRIGGERED
# Optional flag to enable debug/hostcomm interface and functionality
ccflags-$(CONFIG_FUSB302) += -DFSC_DEBUG

# The following flags are used to configure which features are compiled in,
# and the allowed combinations are:
#	FSC_HAVE_SRC - Source only
#	FSC_HAVE_SNK - Sink only
#	FSC_HAVE_SRC, FSC_HAVE_SNK - Source or sink configurable
#	FSC_HAVE_SRC, FSC_HAVE_SNK, FSC_HAVE_DRP - DRP capable source or sink
#	FSC_HAVE_ACCMODE - Accessory mode. Requires FSC_HAVE_SRC.
#	FSC_HAVE_VDM - Vendor Defined Message (VDM) support.
#	FSC_HAVE_DP - Display Port (DP) support. Requires FSC_HAVE_VDM.
ccflags-$(CONFIG_FUSB302) += -DFSC_HAVE_SRC
ccflags-$(CONFIG_FUSB302) += -DFSC_HAVE_SNK
ccflags-$(CONFIG_FUSB302) += -DFSC_HAVE_DRP
ccflags-$(CONFIG_FUSB302) += -DFSC_HAVE_ACCMODE
ccflags-$(CONFIG_FUSB302) += -DFSC_HAVE_VDM
ccflags-$(CONFIG_FUSB302) += -DFSC_HAVE_DP

obj-$(CONFIG_FUSB302)     += fusb302.o
fusb302-objs         := Platform_Linux/fusb30x_driver.o \
                     Platform_Linux/fusb30x_global.o \
                     Platform_Linux/platform.o \
                     Platform_Linux/platform_helpers.o \
                     Platform_Linux/hostcomm.o \
                     core/core.o \
                     core/fusb30X.o \
                     core/Log.o \
                     core/PDPolicy.o \
                     core/PDProtocol.o \
                     core/TypeC.o \
                     core/vdm/bitfield_translators.o \
                     core/vdm/vdm.o \
                     core/vdm/vdm_callbacks.o \
                     core/vdm/vdm_config.o \
                     core/vdm/DisplayPort/configure.o \
                     core/vdm/DisplayPort/dp.o \
                     core/vdm/DisplayPort/dp_system_stubs.o
