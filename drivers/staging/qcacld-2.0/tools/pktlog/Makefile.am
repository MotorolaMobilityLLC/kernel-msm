AM_CFLAGS = -Wall

TARGET_TYPE ?= AR9888
TARGET_VERS ?= v2

if DEBUG
AM_CFLAGS += -g
else
AM_CFLAGS += -O2
endif

AM_CFLAGS += -I $(top_srcdir)/../CORE/SERVICES/COMMON/ \
             -I $(top_srcdir)/../CORE/UTILS/PKTLOG/include

pktlogconf_SOURCES = pktlogconf.c
bin_PROGRAMS = pktlogconf
