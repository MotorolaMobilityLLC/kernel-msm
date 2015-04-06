CC := $(ATH_CROSS_COMPILE_TYPE)gcc
TARGET_TYPE ?= AR9888
TARGET_VERS ?= v2

all:
	$(CC) -g3 -Wall  \
		-I ../../CORE/SERVICES/COMMON/ \
		-I ../../CORE/SERVICES/HIF/ \
		cld-fwlog-record.c -o cld-fwlog-record
	$(CC) -g3 -Wall  \
		-I ../../CORE/SERVICES/COMMON/ \
		-I ../../CORE/SERVICES/HIF/ \
		cld-fwlog-netlink.c parser.c nan-parser.c -o cld-fwlog-netlink
	$(CC) -g3 -Wall  \
		-I ../../CORE/SERVICES/COMMON/ \
		-I ../../CORE/SERVICES/HIF/ \
		cld-fwlog-parser.c -o cld-fwlog-parser
clean:
	rm -f cld-fwlog-record
	rm -f cld-fwlog-parser
	rm -f cld-fwlog-netlink
