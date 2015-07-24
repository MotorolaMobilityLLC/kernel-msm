#
# Makefile for the vl6180 drivers.
#

# Each configuration option enables a list of files.
FEATURE_USE_CCI := true

ifeq ($(FEATURE_USE_CCI), true)
ccflags-y	+= -Idrivers/input/misc/vl6180/inc -DCAMERA_CCI
else
ccflags-y	+= -Idrivers/input/misc/vl6180/inc -DSTM_TEST
endif

ccflags-y += -Idrivers/media/platform/msm/camera_v2/sensor/io
ccflags-y += -Idrivers/media/platform/msm/camera_v2
ccflags-y += -Idrivers/media/platform/msm/camera_v2/sensor/cci
ccflags-y += -Idrivers/input/misc/vl6180/
obj-$(CONFIG_STMVL6180)			+= stmvl6180.o
stmvl6180-objs				:= stmvl6180_module.o stmvl6180_module-i2c.o stmvl6180_module-cci.o src/vl6180x_api.o src/vl6180x_i2c.o src/vl6180x_port_i2c.o
