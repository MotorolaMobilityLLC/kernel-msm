subdir-ccflags-$(CONFIG_ARCH_MSM) += -Iarch/arm/mach-msm

-include $(src)/Makefile.board

obj-$(CONFIG_MACH_ASUSTEK) += devices_asustek.o
obj-$(CONFIG_ASUSTEK_PCBID) += asustek-pcbid.o
obj-$(CONFIG_ASUSTEK_KEYPAD) += asustek-keypad.o

CFLAGS_devices_asustek.o += -Idrivers/staging/android
