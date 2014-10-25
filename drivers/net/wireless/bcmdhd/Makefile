# bcmdhd
SRCROOT = drivers/net/wireless/bcmdhd
DHDCFLAGS = -Wall -Wstrict-prototypes -Werror -Dlinux -DBCMDRIVER             \
	-DBCMDONGLEHOST -DUNRELEASEDCHIP -DBCMDMA32 -DWLBTAMP -DBCMFILEIMAGE  \
	-DDHDTHREAD -DDHD_GPL -DDHD_SCHED -DDHD_DEBUG -DSDTEST -DBDC -DTOE    \
	-DDHD_BCMEVENTS -DSHOW_EVENTS -DDONGLEOVERLAYS -DBCMDBG               \
	-DCUSTOM_OOB_GPIO_NUM=$(CONFIG_BCMDHD_OOB_GPIO)       \
	-DMMC_SDIO_ABORT -DBCMSDIO -DBCMLXSDMMC -DBCMPLATFORM_BUS -DWLP2P     \
	-DNEW_COMPAT_WIRELESS -DWIFI_ACT_FRAME -DARP_OFFLOAD_SUPPORT          \
	-DKEEP_ALIVE -DCSCAN -DGET_CUSTOM_MAC_ENABLE -DPKT_FILTER_SUPPORT     \
	-DEMBEDDED_PLATFORM -DPNO_SUPPORT          \
	-DHW_OOB -DOOB_INTR_ONLY                                              \
	-I$(SRCROOT) \
	-I$(SRCROOT)/include \
	-I$(SRCROOT)/shared \
	-I$(SRCROOT)/dhd/sys \
	-I$(SRCROOT)/dongle \
	-I$(SRCROOT)/wl/sys \
	-I$(SRCROOT)/wl/bcmwifi/src \
	-I$(SRCROOT)/wl/bcmwifi/include

# for P2P
DHDCFLAGS += -DP2P_PATCH

DHDCFLAGS += -DHW_OOB -DOOB_INTR_ONLY

# For CCX
ifeq ($(CONFIG_BRCM_CCX),y)
DHDCFLAGS += -DBCMCCX
endif

DHDOFILES = \
	bcmsdio/sys/bcmsdh.o \
	bcmsdio/sys/bcmsdh_linux.o \
	bcmsdio/sys/bcmsdh_sdmmc.o \
	bcmsdio/sys/bcmsdh_sdmmc_linux.o \
	dhd/sys/dhd_bta.o \
	dhd/sys/dhd_cdc.o \
	dhd/sys/dhd_common.o \
	dhd/sys/dhd_custom_gpio.o \
	dhd/sys/dhd_linux.o \
	dhd/sys/dhd_linux_sched.o \
	dhd/sys/dhd_sdio.o \
	shared/aiutils.o \
	shared/bcmutils.o   \
	shared/bcmevent.o \
	shared/hndpmu.o  \
	shared/linux_osl.o	\
	shared/sbutils.o \
	shared/siutils.o \
	wl/sys/wl_android.o	\
	wl/sys/wldev_common.o \
	wl/sys/wl_linux_mon.o \
	wl/bcmwifi/src/bcmwifi_channels.o 

obj-$(CONFIG_BCMDHD) += bcmdhd.o
bcmdhd-objs += $(DHDOFILES)
ifneq ($(CONFIG_WIRELESS_EXT),)
bcmdhd-objs += wl/sys/wl_iw.o
DHDCFLAGS += -DSOFTAP
endif
ifneq ($(CONFIG_CFG80211),)
bcmdhd-objs += \
	wl/sys/wl_cfg80211.o \
	wl/sys/wl_cfgp2p.o \
	dhd/sys/dhd_cfg80211.o
DHDCFLAGS += -DWL_CFG80211
endif
EXTRA_CFLAGS = $(DHDCFLAGS)
ifeq ($(CONFIG_BCMDHD),m)
EXTRA_LDFLAGS += --strip-debug
endif

ifeq ($(CONFIG_BCM4330_MEM_PREALLOC),y)
WLANCFLAGS += -DCONFIG_DHD_USE_STATIC_BUF
endif
