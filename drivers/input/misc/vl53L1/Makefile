#
# Makefile for the vl53L1 drivers.
#

# Each configuration option enables a list of files.
#FEATURE_USE_CCI := false
#FEATURE_USE_CCI := true
#extra symbol /D that can be used see doxygen doc for full detail 
#define STMVL53L1_LOG_POLL_TIMING
#define STMVL53L1_LOG_CCI_TIMING


ccflags-y	+= -Idrivers/input/misc/vl53L1/inc -Idrivers/input/misc/vl53L1/ipp
ifeq ($(FEATURE_USE_CCI), true)
ccflags-y	+= -DCAMERA_CCI
else
#ccflags-y	+= -DSTM_TEST
endif

#ccflags-y	+= -DSTMVL53L1_LOG_POLL_TIMING=1
#ccflags-y	+= -DSTMVL53L1_LOG_CCI_TIMING=1
 
#ccflags-y += -Idrivers/media/platform/msm/camera_v2/sensor/io
#ccflags-y += -Idrivers/media/platform/msm/camera_v2
#ccflags-y += -Idrivers/media/platform/msm/camera_v2/common
#ccflags-y += -Idrivers/media/platform/msm/camera_v2/sensor/cci
#ccflags-y += -Idrivers/media/platform/msm/camera_v2/sensor/cci
obj-$(CONFIG_STMVL53L1)	+= stmvl53l1.o
stmvl53l1-objs		:= stmvl53l1_module.o stmvl53l1_module-i2c.o stmvl53l1_i2c.o stmvl53l1_ipp_nl.o 
stmvl53l1-objs		+= src/vl53l1_api.o src/vl53l1_api_core.o
stmvl53l1-objs		+= src/vl53l1_api_strings.o src/vl53l1_error_strings.o  src/vl53l1_core.o src/vl53l1_register_funcs.o
stmvl53l1-objs		+= src/vl53l1_api_preset_modes.o  src/vl53l1_api_calibration.o src/vl53l1_fpga_core.o
#localy patched file from src 
#stmvl53l1-objs		+= local/vl53l1_wait.o
stmvl53l1-objs		+= src/vl53l1_wait.o 
stmvl53l1-objs		+= ipp/ipp_linux.o
