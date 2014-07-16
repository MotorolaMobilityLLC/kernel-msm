default: all

KBUILD_OPTIONS := WLAN_ROOT=$(PWD)
KBUILD_OPTIONS += MODNAME=wlan

all::
	$(MAKE) -C $(KERNEL_SOURCE) $(KBUILD_OPTIONS) $(EXTRA_CFLAGS) ARCH=$(ARCH) \
		SUBDIRS=$(CURDIR) CC=${CC} modules

clean::
	rm -f *.o *.ko *.mod.c *.mod.o *~ .*.cmd Module.symvers
	rm -rf .tmp_versions


