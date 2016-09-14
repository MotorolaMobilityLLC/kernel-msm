#ifndef MADERA_SLIMBUS_H
#define MADERA_SLIMBUS_H

#include <sound/soc.h>

#define MADERA_SLIMBUS_MAX_CHANNELS 8

#ifdef CONFIG_SND_SOC_MADERA_SLIMBUS
#include <linux/slimbus/slimbus.h>

int madera_slim_tx_ev(struct snd_soc_dapm_widget *w,
		      struct snd_kcontrol *kcontrol,
		      int event, int dai_id);
int madera_slim_rx_ev(struct snd_soc_dapm_widget *w,
		      struct snd_kcontrol *kcontrol,
		      int event, int dai_id);
int madera_set_channel_map(struct snd_soc_dai *dai,
			   unsigned int tx_num, unsigned int *tx_slot,
			   unsigned int rx_num, unsigned int *rx_slot);
int madera_get_channel_map(struct snd_soc_dai *dai,
			   unsigned int *tx_num, unsigned int *tx_slot,
			   unsigned int *rx_num, unsigned int *rx_slot);
#else
static inline int madera_slim_tx_ev(struct snd_soc_dapm_widget *w,
				    struct snd_kcontrol *kcontrol,
				    int event, int dai_id)
{
	return 0;
}

static inline int madera_slim_rx_ev(struct snd_soc_dapm_widget *w,
				    struct snd_kcontrol *kcontrol,
				    int event, int dai_id)
{
	return 0;
}

static inline int madera_set_channel_map(struct snd_soc_dai *dai,
					 unsigned int tx_num,
					 unsigned int *tx_slot,
					 unsigned int rx_num,
					 unsigned int *rx_slot)
{
	return 0;
}

static inline int madera_get_channel_map(struct snd_soc_dai *dai,
					 unsigned int *tx_num,
					 unsigned int *tx_slot,
					 unsigned int *rx_num,
					 unsigned int *rx_slot)
{
	return 0;
}
#endif

#endif
