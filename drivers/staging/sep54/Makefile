obj-$(CONFIG_DX_SEP54) += sep54.o
sep54-objs := dx_driver.o sep_init.o crypto_ctx_mgr.o sep_sysfs.o \
                   desc_mgr.o lli_mgr.o crypto_api.o sep_request_mgr.o \
                   sep_power.o sepapp.o crypto_hwk.o sepfs.o

ifeq ($(CONFIG_COMPAT),y)
	sep54-objs += sep_compat_ioctl.o
endif

ccflags-y += -DSEP_SUPPORT_SHA=256 -DCONFIG_NOT_COHERENT_CACHE -DSEP_HWK_UNIT_TEST

ifeq ($(CONFIG_PM_RUNTIME),y)
	ccflags-y += -DSEP_RUNTIME_PM
endif
