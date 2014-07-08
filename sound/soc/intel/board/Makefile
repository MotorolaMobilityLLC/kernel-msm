#EXTRA CFLAGS
ccflags-y += -Werror

# Merrifield board
snd-merr-saltbay-lm49453-objs := merr_saltbay_lm49453.o
snd-merr-saltbay-wm8958-objs := merr_saltbay_wm8958.o
snd-merr-dpcm-wm8958-objs := merr_dpcm_wm8958.o

# Moorefield board
obj-$(CONFIG_SND_MOOR_DPCM) += snd-merr-dpcm-wm8958.o
obj-$(CONFIG_SND_MOOR_MACHINE) += snd-merr-saltbay-wm8958.o

