DEFCONFIGSRC			:= kernel/arch/arm/configs
LJAPDEFCONFIGSRC		:= ${DEFCONFIGSRC}/ext_config
PRODUCT_SPECIFIC_DEFCONFIGS	:= $(DEFCONFIGSRC)/$(KERNEL_DEFCONFIG)
TARGET_DEFCONFIG		:= $(KERNEL_OUT)/mapphone_defconfig

# add debug config file for non-user build
ifneq ($(TARGET_BUILD_VARIANT), user)
KERNEL_EXTRA_CONFIG +=debug-$(subst _defconfig,,$(KERNEL_DEFCONFIG))
endif

# append all additional configs
ifneq ($(KERNEL_EXTRA_CONFIG),)
PRODUCT_SPECIFIC_DEFCONFIGS += $(KERNEL_EXTRA_CONFIG:%=$(LJAPDEFCONFIGSRC)/%.config)
endif


define do-make-defconfig
	$(hide) mkdir -p $(dir $(1))
	( perl -le 'print "# This file was automatically generated from:\n#\t" . join("\n#\t", @ARGV) . "\n"' $(2) && cat $(2) ) > $(1) || ( rm -f $(1) && false )
endef

#
# make combined defconfig file
#---------------------------------------
$(TARGET_DEFCONFIG): FORCE $(PRODUCT_SPECIFIC_DEFCONFIGS)
	$(call do-make-defconfig,$@,$(PRODUCT_SPECIFIC_DEFCONFIGS))

.PHONY: FORCE
FORCE:
