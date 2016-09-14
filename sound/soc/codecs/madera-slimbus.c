#include <linux/delay.h>
#include <linux/device.h>

#include <linux/mfd/madera/core.h>
#include <linux/mfd/madera/registers.h>
#include <linux/mfd/madera/pdata.h>

#include "madera-slimbus.h"
#include "madera.h"

/* As there is no sane way to recover an entry point to the slimbus core
 * from anything other than a slim_device probe, simply use this as a bridge
 * between ASoC and the slimbus core to get our slimbus generic device
 * associated with our ASoC specific slimbus code
 */
static struct slim_device *stashed_slim_dev;

static int madera_slim_get_la(struct slim_device *slim, u8 *la)
{
	int ret;
	const unsigned long timeout = jiffies + msecs_to_jiffies(100);

	do {
		ret = slim_get_logical_addr(slim, slim->e_addr, 6, la);
		if (!ret)
			break;
		/* Give SLIMBUS time to report present and be ready. */
		usleep_range(1000, 1100);
		dev_info(&slim->dev, "retrying get logical addr\n");
	} while time_before(jiffies, timeout);


	dev_info(&slim->dev, "LA %d\n", *la);

	return 0;
}

static void madera_slim_fixup_prop(struct madera_dai_priv *dai_priv,
				   struct slim_ch *prop)
{
	prop->prot = SLIM_AUTO_ISO;
	prop->baser = SLIM_RATE_4000HZ;
	prop->dataf = SLIM_CH_DATAF_NOT_DEFINED;
	prop->auxf = SLIM_CH_AUXF_NOT_APPLICABLE;
	prop->ratem = dai_priv->sample_rate / 4000;
	prop->sampleszbits = dai_priv->bit_width;
}

int madera_slim_tx_ev(struct snd_soc_dapm_widget *w,
		      struct snd_kcontrol *kcontrol,
		      int event, int dai_id)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct madera_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct madera_dai_priv *dai_priv = &priv->dai[dai_id - 1];
	struct madera *madera = priv->madera;
	struct slim_ch prop;
	int ret;

	madera_slim_fixup_prop(dai_priv, &prop);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
	case SND_SOC_DAPM_POST_PMU:
		dev_dbg(madera->dev, "Start slimbus TX\n");
		ret = slim_define_ch(stashed_slim_dev, &prop,
				     &priv->tx_channel_handle[w->shift],
				     1, false, NULL);
		if (ret != 0) {
			dev_err(madera->dev, "slim_define_ch() failed: %d\n",
				ret);
			return ret;
		}

		ret = slim_connect_src(stashed_slim_dev,
					priv->tx_port_handle[w->shift],
					priv->tx_channel_handle[w->shift]);
		if (ret != 0) {
			dev_err(madera->dev, "src connect fail %d\n", ret);
			return ret;
		}

		ret = slim_control_ch(stashed_slim_dev,
					priv->tx_channel_handle[w->shift],
					SLIM_CH_ACTIVATE, true);
		if (ret != 0) {
			dev_err(madera->dev, "Failed to activate: %d\n", ret);
			return ret;
		}
		break;

	case SND_SOC_DAPM_POST_PMD:
	case SND_SOC_DAPM_PRE_PMD:
		dev_dbg(madera->dev, "Stop slimbus Tx\n");
		ret = slim_control_ch(stashed_slim_dev,
					priv->tx_channel_handle[w->shift],
					SLIM_CH_REMOVE, true);
		if (ret != 0)
			dev_err(madera->dev, "Failed to remove tx: %d\n", ret);

		break;
	default:
		break;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(madera_slim_tx_ev);

int madera_slim_rx_ev(struct snd_soc_dapm_widget *w,
		      struct snd_kcontrol *kcontrol,
		      int event, int dai_id)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct madera_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct madera_dai_priv *dai_priv = &priv->dai[dai_id - 1];
	struct madera *madera = priv->madera;
	struct slim_ch prop;
	int ret;

	madera_slim_fixup_prop(dai_priv, &prop);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
	case SND_SOC_DAPM_POST_PMU:
		dev_dbg(madera->dev, "Start slimbus RX\n");
		ret = slim_define_ch(stashed_slim_dev, &prop,
				     &priv->rx_channel_handle[w->shift],
				     1, false, NULL);
		if (ret != 0) {
			dev_err(madera->dev, "slim_define_ch() failed: %d\n",
				ret);
			return ret;
		}

		ret = slim_connect_sink(stashed_slim_dev,
					&priv->rx_port_handle[w->shift],
					1, priv->rx_channel_handle[w->shift]);
		if (ret != 0) {
			dev_err(madera->dev, "sink connect fail %d\n", ret);
			return ret;
		}

		ret = slim_control_ch(stashed_slim_dev,
				      priv->rx_channel_handle[w->shift],
				      SLIM_CH_ACTIVATE, true);
		if (ret != 0) {
			dev_err(madera->dev, "Failed to activate: %d\n", ret);
			return ret;
		}
		break;

	case SND_SOC_DAPM_POST_PMD:
	case SND_SOC_DAPM_PRE_PMD:
		ret = slim_control_ch(stashed_slim_dev,
				      priv->rx_channel_handle[w->shift],
				      SLIM_CH_REMOVE, true);
		if (ret != 0)
			dev_err(madera->dev, "Failed to remove rx: %d\n", ret);

		break;
	default:
		break;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(madera_slim_rx_ev);

int madera_set_channel_map(struct snd_soc_dai *dai,
			   unsigned int tx_num, unsigned int *tx_slot,
			   unsigned int rx_num, unsigned int *rx_slot)
{
	struct madera_priv *priv = snd_soc_codec_get_drvdata(dai->codec);
	struct madera *madera = priv->madera;

	u8 laddr;
	int i, ret;

	madera_slim_get_la(stashed_slim_dev, &laddr);

	if (rx_num > MADERA_SLIMBUS_MAX_CHANNELS)
		rx_num = MADERA_SLIMBUS_MAX_CHANNELS;
	priv->rx_chan_map_num = rx_num;

	if (tx_num > MADERA_SLIMBUS_MAX_CHANNELS)
		tx_num = MADERA_SLIMBUS_MAX_CHANNELS;
	priv->tx_chan_map_num = tx_num;

	/* This actually allocates the channel or refcounts it if there... */
	for (i = 0; i < rx_num; i++) {
		slim_get_slaveport(laddr, i, &priv->rx_port_handle[i],
				   SLIM_SINK);

		ret = slim_query_ch(stashed_slim_dev, rx_slot[i],
				    &priv->rx_channel_handle[i]);
		if (ret != 0) {
			dev_err(madera->dev, "slim_alloc_ch() failed: %d\n",
				ret);
			return ret;
		}

		priv->rx_chan_map_slot[i] = rx_slot[i];
	}

	for (i = 0; i < tx_num; i++) {
		slim_get_slaveport(laddr, i + MADERA_SLIMBUS_MAX_CHANNELS,
				   &priv->tx_port_handle[i], SLIM_SRC);

		ret = slim_query_ch(stashed_slim_dev, tx_slot[i],
				    &priv->tx_channel_handle[i]);
		if (ret != 0) {
			dev_err(madera->dev, "slim_alloc_ch() failed: %d\n",
				ret);
			return ret;
		}

		priv->tx_chan_map_slot[i] = tx_slot[i];
	}

	return 0;
}
EXPORT_SYMBOL_GPL(madera_set_channel_map);

int madera_get_channel_map(struct snd_soc_dai *dai,
			   unsigned int *tx_num, unsigned int *tx_slot,
			   unsigned int *rx_num, unsigned int *rx_slot)
{
	struct madera_priv *priv = snd_soc_codec_get_drvdata(dai->codec);
	int i;

	if (!priv->rx_chan_map_num || !priv->tx_chan_map_num)
		return -EINVAL;

	*rx_num = priv->rx_chan_map_num;
	*tx_num = priv->tx_chan_map_num;

	for (i = 0; i < priv->rx_chan_map_num; i++)
		rx_slot[i] = priv->rx_chan_map_slot[i];

	for (i = 0; i < priv->tx_chan_map_num; i++)
		tx_slot[i] = priv->tx_chan_map_slot[i];

	return 0;
}
EXPORT_SYMBOL_GPL(madera_get_channel_map);

static const struct slim_device_id madera_slim_id[] = {
	{ "madera-slim-audio", 0 },
	{ },
};

static int madera_slim_audio_probe(struct slim_device *slim)
{
	stashed_slim_dev = slim;

	return 0;
}

static struct slim_driver madera_slim_audio = {
	.driver = {
		.name = "madera-slim-audio",
		.owner = THIS_MODULE,
	},
	.probe = madera_slim_audio_probe,
	.id_table = madera_slim_id,
};

static int __init madera_slim_audio_init(void)
{
	return slim_driver_register(&madera_slim_audio);
}
module_init(madera_slim_audio_init);

static void __exit madera_slim_audio_exit(void)
{
	slim_driver_unregister(&madera_slim_audio);
}
module_exit(madera_slim_audio_exit);
