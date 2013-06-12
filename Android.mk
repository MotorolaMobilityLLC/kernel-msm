# This Android.mk is experimental. Please use with caution.
#
# The purpose of this file is to allow users to run 'mm' to
# rebuild the kernel from this tree. 'mm' is a faster command
# because it instructs the Android build system to not read
# every Android.mk file (and hence not check all dependencies).
# The benefit is that the user is able to essentially directly
# compile the kernel.
#
# Make sure to use the '-j' option when running 'mm' to spawn
# multiple jobs and speed up your build. For example:
# mm -j8
#
# You can also run 'mm' with a target that you would normally
# use when building the kernel. Just prefix "kernel-" in front.
#
# For example:
# mm kernel-mrproper is the equivalent of running make mrproper
#
ifneq ($(ONE_SHOT_MAKEFILE),)
include build/target/board/Android.mk
include kernel/AndroidKernel.mk

ifeq ($(MAKECMDGOALS),all_modules)
#
# This is the default case when a user runs 'mm'
#
ALL_MODULES += bootimage
else
#
# This is the case where a user runs 'mm' with a special option
# For example "mm kernel-mrproper' or 'mm kernel-clean'
#
ANDROID_MAKE_GOALS=$(filter-out all_modules,$(MAKECMDGOALS))
KERNEL_BUILD_TARGETS=$(subst kernel-,,$(ANDROID_MAKE_GOALS))

$(ANDROID_MAKE_GOALS):
	$(MAKE) -C kernel KBUILD_RELSRC=$(KERNEL_SOURCE_RELATIVE_PATH) O=../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-eabi- $(KERNEL_BUILD_TARGETS)
endif
endif

