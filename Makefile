#Copyright (C) 2014 NXP Semiconductors, All Rights Reserved.
#This program is free software; you can redistribute it and/or modify
#it under the terms of the GNU General Public License version 2 as
#published by the Free Software Foundation.

# make sure that the environment variables ARCH and CROSS_COMPILE
# are set for your architecture and cross compiler.
# And that KDIR points to the kernel to build against.
#
# e.g.:
# export ARCH=arm
# export CROSS_COMPILE=arm-linux-gnueabihf-
# export KDIR=../linux


# In case of out of tree build, build as a module
# (when build inside kernel we will not enter this directory and this will have no effect)
ifeq ($(CONFIG_SND_SOC_TFA98XX),)
CONFIG_SND_SOC_TFA98XX := m
endif

ifneq ($(KERNELRELEASE),)

# add version number derived from Git
ifeq ($(KDIR),)
PLMA_TFA_AUDIO_DRV_DIR=$(realpath -f $(srctree)/$(src))
else
PLMA_TFA_AUDIO_DRV_DIR=$(realpath -f $(src))
endif
GIT_VERSION=$(shell cd $(PLMA_TFA_AUDIO_DRV_DIR); git describe --tags --dirty --match "v[0-9]*.[0-9]*.[0-9]*")
EXTRA_CFLAGS += -DTFA98XX_GIT_VERSIONS=\"$(GIT_VERSION)\"

EXTRA_CFLAGS += -I$(src)/inc
EXTRA_CFLAGS += -Werror

obj-$(CONFIG_SND_SOC_TFA98XX) := snd-soc-tfa98xx.o

snd-soc-tfa98xx-objs += src/tfa98xx.o
snd-soc-tfa98xx-objs += src/tfa_container.o
snd-soc-tfa98xx-objs += src/tfa_dsp.o
snd-soc-tfa98xx-objs += src/tfa_init.o


else

MAKEARCH := $(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE)

all:
	$(MAKEARCH) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKEARCH) -C $(KDIR) M=$(PWD) clean
	rm -f $(snd-soc-tfa98xx-objs)

endif
