#
# Makefile for the sensor drivers.
#

# Each configuration option enables a list of files.
obj-y += factory/
obj-$(CONFIG_SENSORS_SSP)		+= ssp_dev.o ssp_spi.o ssp_data.o ssp_sysfs.o\
						ssp_input.o ssp_firmware.o ssp_debug.o
obj-$(CONFIG_SENSORS_SYSFS)		+= sensors_core.o
obj-$(CONFIG_IIO)			+= ssp_iio_ring.o ssp_iio_trigger.o

obj-$(CONFIG_SENSORS_SSP_SENSORHUB)	+= ssp_sensorhub.o
