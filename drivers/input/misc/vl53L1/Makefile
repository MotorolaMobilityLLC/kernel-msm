#
# Makefile for the vl53L1 drivers.
#

# Each configuration option enables a list of files.
#FEATURE_USE_CCI := false

FEATURE_USE_CCI := true

ifeq ($(FEATURE_USE_CCI), true)
ccflags-y	+= -I$(TOP)/kernel/msm-4.14/drivers/input/misc/vl53L1/inc -I$(TOP)/kernel/msm-4.14/drivers/input/misc/vl53L1/ipp -I$(TOP)/kernel/msm-4.14/drivers/input/misc/vl53L1 -DCAMERA_CCI
else
ccflags-y	+= -I$(src)/inc -I$(src)/ipp -I$(src)
endif

ifdef OLD_NETLINK_API
ccflags-y	+= -DOLD_NETLINK_API
endif

ifdef VL53L1_LOG_ENABLE
ccflags-y	+= -DVL53L1_LOG_ENABLE
endif

ccflags-y += -Idrivers/media/platform/msm/camera/cam_utils
ccflags-y += -Idrivers/media/platform/msm/camera/cam_cpas/include
ccflags-y += -Idrivers/media/platform/msm/camera/cam_sensor_module/cam_sensor_utils
ccflags-y += -Idrivers/media/platform/msm/camera/cam_sensor_module/cam_sensor_io
ccflags-y += -Idrivers/media/platform/msm/camera/cam_sensor_module/cam_cci
ccflags-y += -Idrivers/media/platform/msm/camera/cam_req_mgr
ccflags-y += -Idrivers/media/platform/msm/camera/cam_smmu/

obj-$(CONFIG_INPUT_STMVL53L1)	+= stmvl53l1.o
stmvl53l1-objs		:= stmvl53l1_module.o stmvl53l1_module-i2c.o stmvl53l1_module-cci.o
stmvl53l1-objs		+= stmvl53l1_i2c.o stmvl53l1_ipp_nl.o stmvl53l1_log.o
stmvl53l1-objs		+= src/vl53l1_api.o src/vl53l1_api_core.o
stmvl53l1-objs		+= src/vl53l1_api_strings.o src/vl53l1_error_strings.o
stmvl53l1-objs		+= src/vl53l1_core.o src/vl53l1_register_funcs.o
stmvl53l1-objs		+= src/vl53l1_api_preset_modes.o
stmvl53l1-objs		+= src/vl53l1_api_calibration.o
stmvl53l1-objs		+= src/vl53l1_fpga_core.o src/vl53l1_silicon_core.o
stmvl53l1-objs		+= src/vl53l1_zone_presets.o src/vl53l1_nvm.o
stmvl53l1-objs		+= src/vl53l1_api_debug.o src/vl53l1_core_support.o
stmvl53l1-objs		+= src/vl53l1_wait.o ipp/ipp_linux.o
stmvl53l1-objs		+= src/vl53l1_nvm_debug.o
