ifeq ($(CONFIG_IR2E71Y),y) 
obj-y += ir2e71y_if.o
obj-y += ir2e71y_main.o
obj-y += ir2e71y_func.o
obj-y += ir2e71y_led.o
obj-y += ir2e71y_pm.o
obj-y += ir2e71y_io.o
EXTRA_CFLAGS    += -DSH_BUILD_ID=\"$(SH_BUILD_ID)\"
EXTRA_CFLAGS    += -DUSE_LINUX
endif