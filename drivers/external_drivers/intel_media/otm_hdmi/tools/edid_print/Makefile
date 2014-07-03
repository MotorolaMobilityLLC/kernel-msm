obj-m = edidprint.o
edidprint-objs := edid_print_func.o edid_print.o

OTM_HDMI_INCDIR =hardware/intel/linux/drivers/staging/mrst/drv

KDIR := $(ANDROID_PRODUCT_OUT)/obj/kernel
ccflags-y += \
	-I$(ANDROID_BUILD_TOP)/$(OTM_HDMI_INCDIR)/cosai/linux_kernel/include \
	-I$(ANDROID_BUILD_TOP)/$(OTM_HDMI_INCDIR)/otm_hdmi/pil/include \
	-I$(ANDROID_BUILD_TOP)/$(OTM_HDMI_INCDIR)/otm_hdmi/pil/common \
	-I$(ANDROID_BUILD_TOP)/$(OTM_HDMI_INCDIR)/otm_hdmi/tools/edid_print \

PWD := $(shell pwd)
all:
	make  -C $(KDIR) M=$(PWD) modules
clean:
	make -C $(KDIR) M=$(PWD) clean
