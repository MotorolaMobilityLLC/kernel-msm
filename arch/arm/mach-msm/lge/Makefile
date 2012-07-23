subdir-ccflags-$(CONFIG_ARCH_MSM) += -Iarch/arm/mach-msm

-include $(src)/Makefile.board

obj-$(CONFIG_MACH_LGE) += devices_lge.o 
obj-$(CONFIG_LGE_QFPROM_INTERFACE) += lge_qfprom_access.o
obj-$(CONFIG_UPDATE_LCDC_LUT) += lge_qc_lcdc_luts.o
obj-$(CONFIG_LCD_KCAL) += lge_kcal_ctrl.o
obj-$(CONFIG_LGE_CRASH_HANDLER) += lge_panic_handler.o

CFLAGS_devices_lge.o += -Idrivers/staging/android
