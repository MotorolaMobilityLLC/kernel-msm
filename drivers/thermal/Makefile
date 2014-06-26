#
# Makefile for sensor chip drivers.
#

CFLAGS_intel_mrfl_thermal.o := -Werror
CFLAGS_intel_soc_thermal.o := -Werror
CFLAGS_thermal_core.o := -Werror
CFLAGS_intel_moor_thermal.o := -Werror

obj-$(CONFIG_THERMAL)		+= thermal_sys.o
thermal_sys-y			+= thermal_core.o

obj-$(CONFIG_INTEL_MFLD_THERMAL) += intel_mid_thermal.o
obj-$(CONFIG_INTEL_BYT_THERMAL)  += intel_byt_thermal.o
obj-$(CONFIG_SENSORS_THERMAL_MRFLD)     += intel_mrfl_thermal.o
obj-$(CONFIG_INTEL_BYT_EC_THERMAL)      += intel_byt_ec_thermal.o
obj-$(CONFIG_SOC_THERMAL)               += intel_soc_thermal.o
obj-$(CONFIG_INTEL_MOOR_THERMAL) += intel_moor_thermal.o

# governors
thermal_sys-$(CONFIG_THERMAL_GOV_FAIR_SHARE)	+= fair_share.o
thermal_sys-$(CONFIG_THERMAL_GOV_STEP_WISE)	+= step_wise.o
thermal_sys-$(CONFIG_THERMAL_GOV_USER_SPACE)	+= user_space.o

# cpufreq cooling
thermal_sys-$(CONFIG_CPU_THERMAL)	+= cpu_cooling.o

# platform thermal drivers
obj-$(CONFIG_SPEAR_THERMAL)	+= spear_thermal.o
obj-$(CONFIG_RCAR_THERMAL)	+= rcar_thermal.o
obj-$(CONFIG_KIRKWOOD_THERMAL)  += kirkwood_thermal.o
obj-$(CONFIG_EXYNOS_THERMAL)	+= exynos_thermal.o
obj-$(CONFIG_DOVE_THERMAL)  	+= dove_thermal.o
obj-$(CONFIG_DB8500_THERMAL)	+= db8500_thermal.o
obj-$(CONFIG_ARMADA_THERMAL)	+= armada_thermal.o
obj-$(CONFIG_DB8500_CPUFREQ_COOLING)	+= db8500_cpufreq_cooling.o
obj-$(CONFIG_INTEL_POWERCLAMP)	+= intel_powerclamp.o

