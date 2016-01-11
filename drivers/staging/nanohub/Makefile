#
# Makefile for nanohub
#

obj-$(CONFIG_NANOHUB) += nanohub.o
nanohub-y := main.o bl.o comms.o
nanohub-$(CONFIG_NANOHUB_SPI) += spi.o
nanohub-$(CONFIG_NANOHUB_I2C) += i2c.o
