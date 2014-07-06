#
# Makefile for the RMI4 touchscreen driver.
#
rmi4-objs = synaptics_i2c_rmi4.o rmi4_fw_reflash.o
obj-$(CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI4_FORK) += rmi4.o

ccflags-y += -Wno-packed-bitfield-compat