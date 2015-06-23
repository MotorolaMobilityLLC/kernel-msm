#
# Makefile for the vl6180 drivers.
#

# Each configuration option enables a list of files.

ccflags-$(CONFIG_LASER_STMVL6180)	+= -Idrivers/input/misc/vl6180/inc
obj-$(CONFIG_LASER_STMVL6180)		+= stmvl6180.o
stmvl6180-objs				:= stmvl6180_module.o src/vl6180x_api.o src/vl6180x_i2c.o src/vl6180x_port_i2c.o 
