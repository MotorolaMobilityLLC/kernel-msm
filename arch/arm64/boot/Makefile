#
# arch/arm64/boot/Makefile
#
# This file is included by the global makefile so that you can add your own
# architecture-specific flags and dependencies.
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 2012, ARM Ltd.
# Author: Will Deacon <will.deacon@arm.com>
#
# Based on the ia64 boot/Makefile.
#

include $(srctree)/arch/arm64/boot/dts/qcom/Makefile

targets := Image Image.$(KBUILD_COMPRESS_SUFFIX)

DTB_NAMES := $(subst $\",,$(CONFIG_BUILD_ARM64_APPENDED_DTB_IMAGE_NAMES))
ifneq ($(DTB_NAMES),)
DTB_LIST := $(addsuffix .dtb,$(DTB_NAMES))
DTB_OBJS := $(addprefix $(obj)/dts/,$(DTB_LIST))
else
DTB_OBJS := $(shell find $(obj)/dts/ -name \*.dtb)
endif

$(obj)/Image: vmlinux FORCE
	$(call if_changed,objcopy)

$(obj)/Image.$(KBUILD_COMPRESS_SUFFIX): $(obj)/Image FORCE
	$(call if_changed,$(KBUILD_COMPRESS_CMD))

$(obj)/Image.$(KBUILD_COMPRESS_SUFFIX)-dtb: $(obj)/Image.$(KBUILD_COMPRESS_SUFFIX) $(DTB_OBJS) FORCE
	$(call if_changed,cat)

install: $(obj)/Image
	$(CONFIG_SHELL) $(srctree)/$(src)/install.sh $(KERNELRELEASE) \
	$(obj)/Image System.map "$(INSTALL_PATH)"

zinstall: $(obj)/Image.$(KBUILD_COMPRESS_SUFFIX)
	$(CONFIG_SHELL) $(srctree)/$(src)/install.sh $(KERNELRELEASE) \
	$(obj)/Image.$(KBUILD_COMPRESS_SUFFIX) System.map "$(INSTALL_PATH)"
