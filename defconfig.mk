DEFCONFIGSRC			:= kernel/arch/arm/configs
LJAPDEFCONFIGSRC		:= ${DEFCONFIGSRC}/ext_config
PRODUCT_SPECIFIC_DEFCONFIG	:= $(DEFCONFIGSRC)/$(KERNEL_DEFCONFIG)
TARGET_DEFCONFIG		:= $(KERNEL_OUT)/mapphone_defconfig
USERDEBUG_DEFCONFIG		:= ${LJAPDEFCONFIGSRC}/userdebug_bld.config
ENG_DEFCONFIG			:= ${LJAPDEFCONFIGSRC}/eng_bld.config


BUILD_DEFCONFIGS := ${PRODUCT_SPECIFIC_DEFCONFIG}

ifeq ($(KERNEL_EXTRA_DEBUG), true)
BUILD_DEFCONFIGS += ${USERDEBUG_DEFCONFIG} ${ENG_DEFCONFIG}
else ifeq ($(TARGET_BUILD_VARIANT), eng)
BUILD_DEFCONFIGS += ${USERDEBUG_DEFCONFIG} ${ENG_DEFCONFIG}
else ifeq ($(TARGET_BUILD_VARIANT), userdebug)
BUILD_DEFCONFIGS += ${USERDEBUG_DEFCONFIG}
endif

define do-make-defconfig
	$(hide) mkdir -p $(dir $(1))
	( perl -le 'print "# This file was automatically generated from:\n#\t" . join("\n#\t", @ARGV) . "\n"' $(2) && cat $(2) ) > $(1) || ( rm -f $(1) && false )
endef

#
# make combined defconfig file
#---------------------------------------
$(TARGET_DEFCONFIG): FORCE $(BUILD_DEFCONFIGS)
	$(call do-make-defconfig,$@,$(BUILD_DEFCONFIGS))

.PHONY: FORCE
FORCE:
