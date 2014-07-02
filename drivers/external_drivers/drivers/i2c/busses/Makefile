#
# Makefile for the i2c bus drivers.
#

obj-$(CONFIG_I2C_DESIGNWARE_CORE_FORK)	+= i2c-designware-core.o
obj-$(CONFIG_I2C_DESIGNWARE_PCI_FORK)	+= i2c-designware-pci.o
i2c-designware-pci-objs := i2c-designware-pcidrv.o
i2c-designware-platform-objs := i2c-designware-platdrv.o

obj-$(CONFIG_I2C_PMIC)          += i2c-pmic.o

ccflags-$(CONFIG_I2C_DEBUG_BUS) := -DDEBUG
