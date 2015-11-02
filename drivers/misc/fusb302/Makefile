ccflags-y += -DPLATFORM_LINUX
ccflags-y += -DFSC_INTERRUPT_TRIGGERED
obj-$(CONFIG_FUSB302) += fusb30x_whole.o
fusb30x_whole-objs := Platform_Linux/fusb30x_driver.o \
					Platform_Linux/fusb30x_global.o \
					Platform_Linux/platform.o \
					Platform_Linux/platform_helpers.o \
					Platform_Linux/hostcomm.o \
					core/AlternateModes.o \
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
