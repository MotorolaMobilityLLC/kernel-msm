obj-m = hdmicmd.o
# TODO: Enable the below logic to config KDIR once the OTM MAKEFILE is updated.
KDIR := ../../../../../../../../out/target/product/mfld_pr2/linux/kernel
PWD := $(shell pwd)
all:
	make -C $(KDIR) M=$(PWD) modules
clean:
	make -C $(KDIR) M=$(PWD) clean
