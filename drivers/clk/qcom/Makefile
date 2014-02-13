obj-y				+= clock.o
obj-y				+= clock-dummy.o
obj-y				+= clock-generic.o
obj-y				+= clock-local2.o
obj-y				+= clock-pll.o
ifndef CONFIG_ARM64
obj-y				+= clock-rpm.o
endif
obj-y				+= clock-voter.o

obj-$(CONFIG_DEBUG_FS)		+= clock-debug.o

# MDM9630
obj-$(CONFIG_ARCH_MDM9630)	+= clock-alpha-pll.o

# ACPU clock
obj-$(CONFIG_ARCH_MSM8610)	+= clock-a7.o
obj-$(CONFIG_ARCH_MSM8226)	+= clock-a7.o
obj-$(CONFIG_ARCH_MDM9630)	+= clock-a7.o
obj-$(CONFIG_ARCH_MSM8916)	+= clock-a7.o

obj-$(CONFIG_ARCH_MSM_KRAIT)	+= clock-krait.o
obj-$(CONFIG_ARCH_FSM9900)	+= clock-krait-8974.o
obj-$(CONFIG_ARCH_APQ8084)	+= clock-krait-8974.o
obj-$(CONFIG_ARCH_MSM8974)	+= clock-krait-8974.o

obj-y				+= gdsc.o
obj-y				+= mdss/
