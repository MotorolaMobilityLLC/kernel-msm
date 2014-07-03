obj-m = hdcp.o

PWD := $(shell pwd)

ifeq ($(PF), DV1)
        KDIR := ../../../../../../../../out/target/product/mfld_dv10/obj/kernel
endif
ifeq ($(PF), PR2)
        KDIR := ../../../../../../../../out/target/product/mfld_pr2/linux/kernel/
endif

all:
	make -C $(KDIR) M=$(PWD) modules
clean:
	make -C $(KDIR) M=$(PWD) clean
