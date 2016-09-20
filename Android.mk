# This Android.mk is experimental. And possibly the worst
# makefile on the planet.
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
#
# cd $ANDROID_BUILD_TOP
# mmm -j32 kernel
#

ifneq ($(ONE_SHOT_MAKEFILE),)

include build/target/board/Android.mk
include kernel/AndroidKernel.mk

ALL_MODULES += bootimage

endif

