#EXTRA CFLAGS
ccflags-y += -Werror

# Medfield board
snd-soc-mfld-machine-objs := mfld_common.o mfld_machine.o
obj-$(CONFIG_SND_MFLD_MACHINE) += snd-soc-mfld-machine.o

# Gilligan Island/Lexington board
snd-soc-mfld-machine-gi-objs := mfld_common.o mfld_machine_gi.o
obj-$(CONFIG_SND_MFLD_MACHINE_GI) += snd-soc-mfld-machine-gi.o

# Yukkabeach board
snd-soc-yukkabeach-machine-objs := mfld_common.o yukkabeach_machine.o
obj-$(CONFIG_SND_YUKKABEACH_MACHINE) += snd-soc-yukkabeach-machine.o

# Cloverview/Clovertrail+/Bodegabay board
snd-soc-ctp-machine-objs := ctp_common.o ctp_rhb_cs42l73.o ctp_vb_cs42l73.o ctp_comms_common.o merr_bb_cs42l73.o ctp_ht_wm5102.o ctp_lt_wm8994.o
obj-$(CONFIG_SND_CTP_MACHINE) += snd-soc-ctp-machine.o

# Merrifield board
snd-merr-saltbay-lm49453-objs := merr_saltbay_lm49453.o
snd-merr-saltbay-wm8958-objs := merr_saltbay_wm8958.o
snd-merr-dpcm-wm8958-objs := merr_dpcm_wm8958.o

# Cherrytrail board
snd-cht-bl-rt5672-objs := cht_bl_rt5672.o
snd-cht-dpcm-rt5672-objs := cht_bl_dpcm_rt5672.o

ifdef CONFIG_SST_DPCM
	obj-$(CONFIG_SND_CHT_MACHINE) += snd-cht-dpcm-rt5672.o
	obj-$(CONFIG_SND_MRFLD_MACHINE) += snd-merr-dpcm-wm8958.o
else
	obj-$(CONFIG_SND_CHT_MACHINE) += snd-cht-bl-rt5672.o
	obj-$(CONFIG_SND_MRFLD_MACHINE) += snd-merr-saltbay-lm49453.o snd-merr-saltbay-wm8958.o
endif

# Moorefield board
obj-$(CONFIG_SND_MOOR_MACHINE) += snd-merr-dpcm-wm8958.o
obj-$(CONFIG_SND_MOOR_MACHINE) += snd-merr-saltbay-wm8958.o

# Baytrail board
snd-byt-bl-rt5642-objs := byt_bl_rt5642.o
snd-byt-cr-aic3100-objs := byt_cr_aic3100.o
obj-$(CONFIG_SND_BYT_MACHINE) += snd-byt-bl-rt5642.o snd-byt-cr-aic3100.o

