#
# Makefile for SiW touch driver
#

siwtch-y := siw_touch.o
siwtch-y += siw_touch_hal.o siw_touch_hal_sysfs.o
siwtch-y += siw_touch_bus.o
siwtch-y += siw_touch_bus_i2c.o siw_touch_bus_spi.o
siwtch-y += siw_touch_of.o
siwtch-y += siw_touch_irq.o siw_touch_gpio.o
siwtch-y += siw_touch_event.o siw_touch_notify.o
siwtch-y += siw_touch_sys.o siw_touch_sysfs.o
siwtch-y += siw_touch_misc.o

siwtch-$(CONFIG_TOUCHSCREEN_SIW_MMI_SW49106) += touch_sw49106.o

obj-$(CONFIG_TOUCHSCREEN_SIW_MMI) += siwtch.o

obj-$(CONFIG_TOUCHSCREEN_SIW_MMI_MON) += mon/

