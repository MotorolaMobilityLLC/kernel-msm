ccflags-y = -I$(src)

intel_fabric_logging-objs  := intel_fw_logging.o intel_fabricerr_status.o

obj-$(CONFIG_BCM_BT_LPM)        += bcm-lpm/
obj-$(CONFIG_SCU_LOGGING)	+= intel_fabric_logging.o
obj-$(CONFIG_UUID)		+= uuid.o
obj-$(CONFIG_EMMC_IPANIC)	+= emmc_ipanic.o
obj-$(CONFIG_RAWIO)             += rawio/
obj-$(CONFIG_KCT_DAEMON)	+= kct_daemon.o
obj-y				+= tp2e/
obj-$(CONFIG_FPS_THROTTLE)	+= fps_throttle.o
