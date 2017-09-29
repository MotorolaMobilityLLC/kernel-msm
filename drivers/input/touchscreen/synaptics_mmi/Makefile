#
# Makefile for the Synaptics DSX touchscreen driver.
#

# Each configuration option enables a list of files.

obj-$(CONFIG_TOUCHSCREEN_SYNAPTICS_DSX_I2C_MMI) := synaptics_mmi.o

synaptics_mmi-objs := synaptics_dsx_i2c.o
ifeq ($(CONFIG_DROPBOX),y)
	synaptics_mmi-objs += synaptics_dsx_dropbox.o
endif

obj-$(CONFIG_TOUCHSCREEN_SYNAPTICS_DSX_RMI_DEV_MMI) += synaptics_dsx_rmi_dev.o
obj-$(CONFIG_TOUCHSCREEN_SYNAPTICS_DSX_FW_UPDATE_MMI) += synaptics_dsx_fw_update.o
obj-$(CONFIG_TOUCHSCREEN_SYNAPTICS_DSX_TEST_REPORTING_MMI) += synaptics_dsx_test_reporting.o
obj-$(CONFIG_TOUCHSCREEN_SYNAPTICS_DSX_CTRL_ACCESS_BLK_MMI) += synaptics_dsx_control_access_block.o

