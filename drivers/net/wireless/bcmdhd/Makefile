# bcmdhd
#####################
# SDIO Basic feature
#####################

DHDCFLAGS = -Wall -Wstrict-prototypes -Dlinux -DLINUX -DBCMDRIVER             \
        -DBCMDONGLEHOST -DUNRELEASEDCHIP -DBCMDMA32 -DBCMFILEIMAGE            \
        -DDHDTHREAD -DBDC                                                     \
        -DDHD_BCMEVENTS -DSHOW_EVENTS -DBCMDBG                                \
        -DMMC_SDIO_ABORT -DBCMSDIO -DBCMLXSDMMC -DBCMPLATFORM_BUS -DWLP2P     \
        -DWIFI_ACT_FRAME -DARP_OFFLOAD_SUPPORT          \
        -DKEEP_ALIVE -DCSCAN -DPKT_FILTER_SUPPORT                             \
        -DEMBEDDED_PLATFORM -DPNO_SUPPORT -DSDIO_CRC_ERROR_FIX

#################
# Common feature
#################

ifeq ($(CONFIG_BCMDHD_HOSTWAKEUP_USE_GPIO),y)
ifeq ($(CONFIG_BCMDHD_HW_OOB),y)
DHDCFLAGS += -DHW_OOB
endif
DHDCFLAGS += -DOOB_INTR_ONLY
DHDCFLAGS += -DCUSTOM_OOB_GPIO_NUM=$(CONFIG_BCMDHD_GPIO_WL_HOSTWAKEUP)
else
DHDCFLAGS += -DSDIO_ISR_THREAD
endif

ifeq ($(CONFIG_WIFI_CONTROL_FUNC),y)
DHDCFLAGS += -DCUSTOMER_HW2
DHDCFLAGS += -DCONFIG_WIFI_CONTROL_FUNC
endif

DHDCFLAGS += -DCUSTOMER_HW4
DHDCFLAGS += -DCUSTOMER_HW10
DHDCFLAGS += -DDEBUGFS_CFG80211
#DHDCFLAGS += -DBLOCK_IPV6_PACKET -DPASS_IPV4_SUSPEND
#DHDCFLAGS += -DSUPPORT_DEEP_SLEEP
#DHDCFLAGS += -DSIMPLE_MAC_PRINT

# Print out kernel panic point of file and line info when assertion happened
DHDCFLAGS += -DBCMASSERT_LOG

# For p2p connection issue
DHDCFLAGS += -DWL_CFG80211_GON_COLLISION
DHDCFLAGS += -DWL_SCB_TIMEOUT=10

# For Passing all multicast packets to host when not in suspend mode.
DHDCFLAGS += -DPASS_ALL_MCAST_PKTS

# Early suspend
#DHDCFLAGS += -DDHD_USE_EARLYSUSPEND

DHDCFLAGS += -DSUPPORT_PM2_ONLY

# For Scan result patch
DHDCFLAGS += -DESCAN_RESULT_PATCH
DHDCFLAGS += -DDUAL_ESCAN_RESULT_BUFFER

#DHDCFLAGS += -DROAM_ENABLE -DROAM_CHANNEL_CACHE -DROAM_API
#DHDCFLAGS += -DENABLE_FW_ROAM_SUSPEND

# For Static Buffer
ifeq ($(CONFIG_BROADCOM_WIFI_RESERVED_MEM),y)
  DHDCFLAGS += -DCONFIG_DHD_USE_STATIC_BUF
  DHDCFLAGS += -DENHANCED_STATIC_BUF
  DHDCFLAGS += -DSTATIC_WL_PRIV_STRUCT
endif

# For CCX
ifeq ($(CONFIG_BRCM_CCX),y)
  DHDCFLAGS += -DBCMCCX
endif

DHDCFLAGS += -DWL_CFG80211

# SoftAP
DHDCFLAGS += -DSUPPORT_AUTO_CHANNEL -DSUPPORT_HIDDEN_AP
DHDCFLAGS += -DSUPPORT_SOFTAP_SINGL_DISASSOC
#DHDCFLAGS += -DUSE_STAMAC_4SOFTAP

# DPC priority
DHDCFLAGS += -DCUSTOM_DPC_PRIO_SETTING=98

# WiFi turn off delay
DHDCFLAGS += -DWIFI_TURNOFF_DELAY=100

# DTIM listen interval in suspend mode(0 means follow AP's DTIM period)
DHDCFLAGS += -DCUSTOM_SUSPEND_BCN_LI_DTIM=0

# Priority mismatch fix with kernel stack
DHDCFLAGS += -DPKTPRIO_OVERRIDE

# Ioctl timeout 5000ms
DHDCFLAGS += -DIOCTL_RESP_TIMEOUT=5000

# WiFi Kernel thread type
DHDCFLAGS += -DUSE_KTHREAD_API

# for only AP Certification
#DHDCFLAGS += -DBCMWPA_MIXED

# B/G/BGN mode support in HOSTAPD
#DHDCFLAGS += -DSUPPORT_HOSTAPD_BGN_MODE

############
# JellyBean 
############
DHDCFLAGS += -DWL_ENABLE_P2P_IF
#DHDCFLAGS += -DMULTIPLE_SUPPLICANT
DHDCFLAGS += -DWL_CFG80211_STA_EVENT

#########################
# Chip dependent feature
#########################

ifneq ($(CONFIG_BCM4335),)
  DHDCFLAGS += -DBCM4335_CHIP
  DHDCFLAGS += -DDHD_USE_IDLECOUNT
  DHDCFLAGS += -DSUPPORT_AMPDU_MPDU_CMD
  DHDCFLAGS += -DUSE_DYNAMIC_F2_BLKSIZE -DDYNAMIC_F2_BLKSIZE_FOR_NONLEGACY=128
#  DHDCFLAGS += -DUSE_CID_CHECK -DCONFIG_CONTROL_PM
  DHDCFLAGS += -DCONFIG_PM_LOCK
  DHDCFLAGS += -DPROP_TXSTATUS -DPROP_TXSTATUS_VSDB
  DHDCFLAGS += -DVSDB
  DHDCFLAGS += -DWL_CFG80211_VSDB_PRIORITIZE_SCAN_REQUEST
  DHDCFLAGS += -DCUSTOM_GLOM_SETTING=5 -DCUSTOM_RXCHAIN=1
# DHDCFLAGS += -DENABLE_BCN_LI_BCN_WAKEUP
  DHDCFLAGS += -DROAM_AP_ENV_DETECTION
  DHDCFLAGS += -DBCMSDIOH_TXGLOM -DCUSTOM_TXGLOM=1 -DBCMSDIOH_TXGLOM_HIGHSPEED
  DHDCFLAGS += -DDHDTCPACK_SUPPRESS
# DHDCFLAGS += -DDHD_SET_FW_HIGHSPEED
  DHDCFLAGS += -DUSE_WL_TXBF
  DHDCFLAGS += -DUSE_WL_FRAMEBURST
  DHDCFLAGS += -DRXFRAME_THREAD
  DHDCFLAGS += -DREPEAT_READFRAME
  DHDCFLAGS += -DCUSTOM_DPC_CPUCORE=0
  DHDCFLAGS += -DCUSTOM_AMPDU_BA_WSIZE=40
  DHDCFLAGS += -DWL11U
  DHDCFLAGS += -DBCMCCX
#  DHDCFLAGS += -DLPC_SUPPORT
#  DHDCFLAGS += -DDISABLE_11AC
#  DHDCFLAGS += -DWES_SUPPORT
#  DHDCFLAGS += -DOKC_SUPPORT
#  DHDCFLAGS += -DWLTDLS
# DHDCFLAGS += -DTPUT_DEBUG
#  DHDCFLAGS += -DWLFBT
# For BT LOCK
ifeq ($(CONFIG_BCM4335BT),y)
#  DHDCFLAGS += -DENABLE_4335BT_WAR
endif
  # Remove define for BCM4335
  DHDCFLAGS :=$(filter-out -DWL_CFG80211_GON_COLLISION,$(DHDCFLAGS))
endif

ifneq ($(CONFIG_BCM4334),)
  DHDCFLAGS += -DBCM4334_CHIP
#  DHDCFLAGS += -DUSE_CID_CHECK -DCONFIG_CONTROL_PM
  DHDCFLAGS += -DCONFIG_PM_LOCK
  DHDCFLAGS += -DPROP_TXSTATUS -DPROP_TXSTATUS_VSDB
  DHDCFLAGS += -DVSDB
  DHDCFLAGS += -DWL_CFG80211_VSDB_PRIORITIZE_SCAN_REQUEST
  DHDCFLAGS += -DDHD_USE_IDLECOUNT
  DHDCFLAGS += -DSUPPORT_AMPDU_MPDU_CMD
  DHDCFLAGS += -DUSE_DYNAMIC_F2_BLKSIZE -DDYNAMIC_F2_BLKSIZE_FOR_NONLEGACY=64
  DHDCFLAGS += -DCUSTOM_GLOM_SETTING=5 -DENABLE_BCN_LI_BCN_WAKEUP
#  DHDCFLAGS += -DROAM_AP_ENV_DETECTION
endif

ifneq ($(CONFIG_BCM4330),)
  DHDCFLAGS += -DBCM4330_CHIP
  DHDCFLAGS += -DMCAST_LIST_ACCUMULATION
  DHDCFLAGS += -DCONFIG_PM_LOCK
  DHDCFLAGS += -DCUSTOM_GLOM_SETTING=0
  DHDCFLAGS += -DPASS_ARP_PACKET
  DHDCFLAGS += -DWL_CFG80211_VSDB_PRIORITIZE_SCAN_REQUEST
endif


#############################
# Platform dependent feature
#############################

DHDCFLAGS += -DGET_CUSTOM_MAC_ENABLE
ifeq ($(CONFIG_BCM4335),y)
DHDCFLAGS += -DENABLE_INSMOD_NO_FW_LOAD
endif

#########
# Others
#########

#EXTRA_LDFLAGS += --strip-debug

EXTRA_CFLAGS += $(DHDCFLAGS) -DDHD_DEBUG
EXTRA_CFLAGS += -DSRCBASE=\"$(src)/src\"
EXTRA_CFLAGS += -I$(src)/src/include/
EXTRA_CFLAGS += -I$(src)/src/dhd/sys/
EXTRA_CFLAGS += -I$(src)/src/dongle/
EXTRA_CFLAGS += -I$(src)/src/bcmsdio/sys/
EXTRA_CFLAGS += -I$(src)/src/wl/sys/
EXTRA_CFLAGS += -I$(src)/src/shared/
EXTRA_CFLAGS += -I$(src)/src/shared/bcmwifi/include/
EXTRA_CFLAGS += -I$(src)/src/shared/bcmwifi/src/

KBUILD_CFLAGS += -I$(LINUXDIR)/include -I$(shell pwd)

DHDOFILES := src/bcmsdio/sys/bcmsdh.o	src/bcmsdio/sys/bcmsdh_linux.o \
	 src/bcmsdio/sys/bcmsdh_sdmmc.o	src/bcmsdio/sys/bcmsdh_sdmmc_linux.o \
	 src/dhd/sys/dhd_cdc.o \
	 src/dhd/sys/dhd_common.o	src/dhd/sys/dhd_custom_gpio.o \
	 src/dhd/sys/dhd_linux.o	src/dhd/sys/dhd_linux_sched.o \
	 src/dhd/sys/dhd_cfg80211.o	src/dhd/sys/dhd_sdio.o \
	 src/shared/aiutils.o		src/shared/bcmevent.o \
	 src/shared/bcmutils.o		src/shared/bcmwifi/src/bcmwifi_channels.o \
	 src/shared/hndpmu.o		src/shared/linux_osl.o \
	 src/shared/sbutils.o		src/shared/siutils.o \
	 src/wl/sys/wl_android.o	src/wl/sys/wl_cfg80211.o \
	 src/wl/sys/wl_cfgp2p.o		src/wl/sys/wldev_common.o \
	 src/wl/sys/wl_linux_mon.o	src/wl/sys/wl_roam.o

obj-$(CONFIG_BCMDHD) += bcmdhd.o

bcmdhd-objs := $(DHDOFILES)

all:
	@echo "$(MAKE) --no-print-directory -C $(KDIR) SUBDIRS=$(CURDIR) modules"
	@$(MAKE) --no-print-directory -C $(KDIR) SUBDIRS=$(CURDIR) modules

clean:
	rm -rf *.o *.ko *.mod.c *~ .*.cmd \
	Module.symvers modules.order .tmp_versions modules.builtin \
	src/bcmsdio/sys/*.o \
	src/bcmsdio/sys/*.o.cmd \
	src/bcmsdio/sys/.*.o.cmd \
	src/dhd/sys/*.o \
	src/dhd/sys/*.o.cmd \
	src/dhd/sys/.*.o.cmd \
	src/shared/*.o \
	src/shared/*.o.cmd \
	src/shared/.*.o.cmd \
	src/wl/sys/*.o \
	src/wl/sys/*.o.cmd \
	src/wl/sys/.*.o.cmd \
	src/shared/bcmwifi/src/bcmwifi_channels.o \
	src/shared/bcmwifi/src/.*.o.cmd

install:
	@$(MAKE) --no-print-directory -C $(KDIR) \
		SUBDIRS=$(CURDIR) modules_install
