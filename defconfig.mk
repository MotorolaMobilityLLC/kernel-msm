DEFCONFIGSRC			:= $(TARGET_KERNEL_SOURCE)/arch/$(KERNEL_ARCH)/configs
LJAPDEFCONFIGSRC		:= ${DEFCONFIGSRC}/vendor/ext_config
DEFCONFIG_BASENAME		:= $(subst -perf,,$(subst _defconfig,,$(notdir $(KERNEL_DEFCONFIG))))
PRODUCT_SPECIFIC_DEFCONFIGS	:= $(DEFCONFIGSRC)/$(KERNEL_DEFCONFIG) $(LJAPDEFCONFIGSRC)/moto-$(DEFCONFIG_BASENAME).config
TARGET_DEFCONFIG		:= $(KERNEL_OUT)/mapphone_defconfig
KERNEL_DEBUG_DEFCONFIG          := $(LJAPDEFCONFIGSRC)/debug-$(DEFCONFIG_BASENAME).config
PRODUCT_KERNEL_DEBUG_DEFCONFIG  := $(LJAPDEFCONFIGSRC)/$(PRODUCT_DEBUG_DEFCONFIG)
FACTORY_DEFCONFIG		:= $(LJAPDEFCONFIGSRC)/factory-$(DEFCONFIG_BASENAME).config

# add debug config file for non-user build
ifneq ($(TARGET_BUILD_VARIANT), user)
ifneq ($(TARGET_NO_KERNEL_DEBUG), true)
ifneq ($(wildcard $(KERNEL_DEBUG_DEFCONFIG)),)
PRODUCT_SPECIFIC_DEFCONFIGS += $(KERNEL_DEBUG_DEFCONFIG)
# Add a product-specific debug defconfig, too
ifneq ($(PRODUCT_DEBUG_DEFCONFIG),)
PRODUCT_SPECIFIC_DEFCONFIGS += $(PRODUCT_KERNEL_DEBUG_DEFCONFIG)
endif
endif
endif
endif

ifeq ($(TARGET_FACTORY_DEFCONFIG), true)
PRODUCT_SPECIFIC_DEFCONFIGS += $(FACTORY_DEFCONFIG)
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
$(TARGET_DEFCONFIG): $(PRODUCT_SPECIFIC_DEFCONFIGS)
	$(call do-make-defconfig,$@,$(PRODUCT_SPECIFIC_DEFCONFIGS))
