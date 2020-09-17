DEFCONFIGSRC			:= $(TARGET_KERNEL_SOURCE)/arch/$(KERNEL_ARCH)/configs
LJAPDEFCONFIGSRC		:= ${DEFCONFIGSRC}/vendor/ext_config
DEFCONFIG_BASENAME		:= $(subst -perf,,$(subst _defconfig,,$(notdir $(KERNEL_DEFCONFIG))))
PRODUCT_SPECIFIC_DEFCONFIGS	:= $(DEFCONFIGSRC)/$(KERNEL_DEFCONFIG) $(LJAPDEFCONFIGSRC)/moto-$(DEFCONFIG_BASENAME).config
TARGET_DEFCONFIG		:= $(KERNEL_OUT)/mapphone_defconfig
KERNEL_DEBUG_DEFCONFIG          := $(LJAPDEFCONFIGSRC)/debug-$(DEFCONFIG_BASENAME).config
KERNEL_DEBUG_ARCH_DEFCONFIG          := $(LJAPDEFCONFIGSRC)/debug-$(DEFCONFIG_BASENAME)-$(KERNEL_ARCH).config
PRODUCT_KERNEL_DEBUG_DEFCONFIG  := $(LJAPDEFCONFIGSRC)/$(PRODUCT_DEBUG_DEFCONFIG)
FACTORY_DEFCONFIG		:= $(LJAPDEFCONFIGSRC)/factory-$(DEFCONFIG_BASENAME).config
KERNEL_DEBUG_FS_DEFCONFIG       := ${DEFCONFIGSRC}/vendor/debugfs.config

# add debug config file for non-user build
ifneq ($(TARGET_BUILD_VARIANT), user)
ifneq ($(TARGET_NO_KERNEL_DEBUG), true)

ifneq ($(wildcard $(KERNEL_DEBUG_DEFCONFIG)),)
PRODUCT_SPECIFIC_DEFCONFIGS += $(KERNEL_DEBUG_DEFCONFIG)

# add arch arm/arm64 debug config
ifneq ($(wildcard $(KERNEL_DEBUG_ARCH_DEFCONFIG)),)
PRODUCT_SPECIFIC_DEFCONFIGS += $(KERNEL_DEBUG_ARCH_DEFCONFIG)
endif

# Add a product-specific debug defconfig, too
ifneq ($(wildcard $(PRODUCT_KERNEL_DEBUG_DEFCONFIG)),)
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

ifeq ($(TARGET_BUILD_VARIANT), user)
ifeq (true,$(call math_gt_or_eq,$(SHIPPING_API_LEVEL),30))
# disable debug fs
PRODUCT_SPECIFIC_DEFCONFIGS += $(KERNEL_DEBUG_FS_DEFCONFIG)
endif
endif


define do-make-defconfig
	$(hide) mkdir -p $(dir $(1))
	( perl -le 'print "# This file was automatically generated from:\n#\t" . join("\n#\t", @ARGV) . "\n"' $(2) && cat $(2) ) > $(1) || ( rm -f $(1) && false )
endef

#When building MSI, moto config does not really take effect
ifneq ($(findstring msi, $(TARGET_PRODUCT)),)
PRODUCT_SPECIFIC_DEFCONFIGS := $(DEFCONFIGSRC)/$(KERNEL_DEFCONFIG)
endif
#
# make combined defconfig file
#---------------------------------------
$(TARGET_DEFCONFIG): $(PRODUCT_SPECIFIC_DEFCONFIGS)
	$(call do-make-defconfig,$@,$(PRODUCT_SPECIFIC_DEFCONFIGS))
