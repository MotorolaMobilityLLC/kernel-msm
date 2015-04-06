CC = gcc
CORE_DIR := $(shell pwd)/../../CORE
PKTLOG_INCLUDE := -I$(CORE_DIR)/UTILS/PKTLOG/include \
		  -I$(CORE_DIR)/SERVICES/COMMON

all: pktlogconf

pktlogconf:
	$(CC) pktlogconf.c $(PKTLOG_INCLUDE) -o pktlogconf

clean:
	rm -f pktlogconf
