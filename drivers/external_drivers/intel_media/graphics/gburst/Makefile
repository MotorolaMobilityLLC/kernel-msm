# NOTE: This file is *not* invoked by the kernel build system,
# as the presence of file "Kbuild" takes precedence.  Rather, this
# file can be invoked by a "make" in this directory to build the
# module stand-alone.
#
# See commentary in file "Kbuild".

# For development and test, be more verbose for build of this module.
export KBUILD_VERBOSE := 1

ifeq ($(KERNELRELEASE),)

ifeq ($(wildcard $(ANDROID_BUILD_TOP)),)
$(error Error: required directory not present: ANDROID_BUILD_TOP = $(ANDROID_BUILD_TOP))
endif

ifeq ($(wildcard $(ANDROID_PRODUCT_OUT)),)
$(error Error: required directory not present: ANDROID_PRODUCT_OUT = $(ANDROID_PRODUCT_OUT))
endif

# Assume the source tree is where the running kernel was built
# You should set KERNELDIR in the environment if it's elsewhere
KERNELDIR ?= $(ANDROID_BUILD_TOP)/$(KERNEL_SRC_DIR)

MODULE_SRC  := $(ANDROID_PRODUCT_OUT)/kernel_modules
MODULE_DEST := $(ANDROID_PRODUCT_OUT)/root/lib/modules

INSTALL_MOD_PATH=$(MODULE_SRC)
export INSTALL_MOD_PATH

# The current directory is passed to sub-makes as argument
CURDIR := $(shell pwd)

# Note: This export of KBUILD_OUTPUT is equivalent to -O=the_same
export KBUILD_OUTPUT := $(ANDROID_PRODUCT_OUT)/linux/kernel

# In this makefile (used only for external module builds), force
# GBURST_EXT_MOD_BUILD=y to allow standalone module builds for development
# and testing.

default:	modules

make_and_install: modules
	$(MAKE) modules_install

modules:
	$(MAKE) GBURST_EXT_MOD_BUILD=y -C $(KERNELDIR) M=$(CURDIR) modules

modules_install:
	$(MAKE) GBURST_EXT_MOD_BUILD=y -C $(KERNELDIR) M=$(CURDIR) modules_install
	cp -vpf gburst.ko $(MODULE_DEST)

clean:
	$(MAKE) GBURST_EXT_MOD_BUILD=y -C $(KERNELDIR) M=$(CURDIR) $@
	rm -f m.log
	rm -f $(MODULE_DEST)/gburst.ko


cleanx:
	rm -f .*.o.d built-in.o .built-in.o.cmd
	rm -f *.o *~ core .depend .*.cmd *.ko *.mod.c
	rm -f Module.symvers Module.markers modules.order
	rm -f *.lst m.log
	rm -rf .tmp_versions

.PHONY: modules modules_install clean

# Required once per boot before pushing module.
adb_rw:
	adb shell mount -o rw,remount /

# Push the module to its home on the device-under-test.
adb_push:
	adb push gburst.ko /lib/modules/gburst.ko

# Notes about module installation location:
# Default directory is: /lib/modules/<kernel-version>/extra
#
# If specified, INSTALL_MOD_PATH is a prefix for the above (used only by default definition of MODLIB).
#   Definition not necessary if MODLIB also being defined.
# MODLIB - Initial part of directory specification.  Default is /lib/modules/<kernel-version>.
# INSTALL_MOD_DIR - default is "extra" for out-of-tree, "kernel" for in-tree.
# install-dir - Default definition is INSTALL_MOD_DIR if non-blank, else "extra".
#   Same as INSTALL_MOD_DIR, but only for directory creation.
#
# The command line assignment overrides all makefile assignments.
#
## make MODLIB=${ANDROID_PRODUCT_OUT}/root/lib/modules INSTALL_MOD_DIR= install-dir=. modules_install

endif
