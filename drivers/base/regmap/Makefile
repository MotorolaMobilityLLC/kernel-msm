obj-$(CONFIG_REGMAP) += regmap.o regcache.o
obj-$(CONFIG_REGMAP) += regcache-rbtree.o regcache-lzo.o regcache-flat.o
obj-$(CONFIG_DEBUG_FS) += regmap-debugfs.o
obj-$(CONFIG_REGMAP_I2C) += regmap-i2c.o
obj-$(CONFIG_REGMAP_SPI) += regmap-spi.o
obj-$(CONFIG_REGMAP_MMIO) += regmap-mmio.o
obj-$(CONFIG_REGMAP_IRQ) += regmap-irq.o

ifeq (${TARGET_BUILD_VARIANT},$(filter ${TARGET_BUILD_VARIANT}, eng))
ccflags-y += -DCONFIG_REGMAP_ALLOW_WRITE_DEBUGFS
endif
