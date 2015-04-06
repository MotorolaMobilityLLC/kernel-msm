
CC := $(ATH_CROSS_COMPILE_TYPE)gcc
TARGET_TYPE ?= AR9888
TARGET_VERS ?= v2

all:
	$(CC) -g3 -Wall  \
		-I ../../CORE/SERVICES/COMMON/ \
		athdiag.c -o athdiag

clean:
	rm -f athdiag
