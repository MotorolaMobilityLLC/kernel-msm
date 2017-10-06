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

static struct mutex slim_tx_lock;
static struct mutex slim_rx_lock;

static int madera_slim_get_la(struct slim_device *slim, u8 *la)
{
	int ret;
	const unsigned long timeout = jiffies + msecs_to_jiffies(100);

	if (slim == NULL)
		return -EINVAL;

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

static void madera_slim_fixup_prop(struct slim_ch *prop,
				   u32 samplerate, u32 sampleszbits)
{
	prop->prot = SLIM_AUTO_ISO;
	prop->baser = SLIM_RATE_4000HZ;
	prop->dataf = SLIM_CH_DATAF_NOT_DEFINED;
	prop->auxf = SLIM_CH_AUXF_NOT_APPLICABLE;
	prop->ratem = samplerate / 4000;
	prop->sampleszbits = sampleszbits;
}

#define TX_STREAM_1 128
#define TX_STREAM_2 132
#define TX_STREAM_3 131

#define RX_STREAM_1 144
#define RX_STREAM_2 146
#define RX_STREAM_3 148

static u32 rx_porth1[2], rx_porth2[2], rx_porth3[2];
static u32 tx_porth1[3], tx_porth2[2], tx_porth3[1];
static u16 rx_handles1[] = { RX_STREAM_1, RX_STREAM_1 + 1 };
static u16 rx_handles2[] = { RX_STREAM_2, RX_STREAM_2 + 1 };
static u16 rx_handles3[] = { RX_STREAM_3, RX_STREAM_3 + 1 };
static u16 tx_handles1[] = { TX_STREAM_1, TX_STREAM_1 + 1, TX_STREAM_1 + 2};
static u16 tx_handles2[] = { TX_STREAM_2, TX_STREAM_2 + 1 };
static u16 tx_handles3[] = { TX_STREAM_3 };
static u16 rx_group1, rx_group2, rx_group3;
static u16 tx_group1, tx_group2, tx_group3;

int madera_slim_tx_ev(struct snd_soc_dapm_widget *w,
		      struct snd_kcontrol *kcontrol,
		      int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct madera_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct madera *madera = priv->madera;
	struct slim_ch prop;
	int ret = 0, i;
	u32 *porth;
	u16 *handles, *group;
	int chcnt;

	if (stashed_slim_dev == NULL) {
		dev_err(madera->dev, "slim device undefined.\n");
		return -EINVAL;
	}
	mutex_lock(&slim_tx_lock);
	switch (w->shift) {
	case MADERA_SLIMTX1_ENA_SHIFT:
		dev_dbg(madera->dev, "TX1\n");
		chcnt = priv->tx_chan_map_num[0];
		if (chcnt > ARRAY_SIZE(tx_porth1)) {
			dev_err(madera->dev, "ERROR: Too many TX channels\n");
			ret = -EINVAL;
			goto exit;
		}
		porth = tx_porth1;
		handles = tx_handles1;
		group = &tx_group1;
		break;
	case MADERA_SLIMTX5_ENA_SHIFT:
		dev_dbg(madera->dev, "TX2\n");
		chcnt = priv->tx_chan_map_num[1];
		if (chcnt > ARRAY_SIZE(tx_porth2)) {
			dev_err(madera->dev, "ERROR: Too many TX channels\n");
			ret = -EINVAL;
			goto exit;
		}
		porth = tx_porth2;
		handles = tx_handles2;
		group = &tx_group2;
		break;
	case MADERA_SLIMTX4_ENA_SHIFT:
		dev_dbg(codec->dev, "TX3\n");
		chcnt = priv->tx_chan_map_num[2];
		if (chcnt > ARRAY_SIZE(tx_porth3)) {
			dev_err(madera->dev, "ERROR: Too many TX channels\n");
			ret = -EINVAL;
			goto exit;
		}
		porth = tx_porth3;
		handles = tx_handles3;
		group = &tx_group3;
		break;
	default:
		goto exit;
	}
	madera_slim_fixup_prop(&prop, 48000, 16);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
	case SND_SOC_DAPM_POST_PMU:
		dev_dbg(madera->dev, "Start slimbus TX\n");
		ret = slim_define_ch(stashed_slim_dev, &prop,
				     handles, chcnt, true, group);
		if (ret != 0) {
			dev_err(madera->dev, "slim_define_ch() failed: %d\n",
				ret);
			goto exit;
		}

		for (i = 0; i < chcnt; i++) {
			ret = slim_connect_src(stashed_slim_dev,
						porth[i], handles[i]);
			if (ret != 0) {
				dev_err(madera->dev, "src connect fail %d:%d\n",
					i, ret);
				goto exit;
			}
		}

		ret = slim_control_ch(stashed_slim_dev, *group,
					SLIM_CH_ACTIVATE, true);
		if (ret != 0) {
			dev_err(madera->dev, "Failed to activate: %d\n", ret);
			goto exit;
		}
		break;

	case SND_SOC_DAPM_POST_PMD:
	case SND_SOC_DAPM_PRE_PMD:
		dev_dbg(madera->dev, "Stop slimbus Tx\n");
		ret = slim_control_ch(stashed_slim_dev, *group,
					SLIM_CH_REMOVE, true);
		if (ret != 0)
			dev_err(madera->dev, "Failed to remove tx: %d\n", ret);

		break;
	default:
		break;
	}

exit:
	mutex_unlock(&slim_tx_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(madera_slim_tx_ev);

int madera_slim_rx_ev(struct snd_soc_dapm_widget *w,
		      struct snd_kcontrol *kcontrol,
		      int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct madera_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct madera *madera = priv->madera;
	struct slim_ch prop;
	int ret = 0, i;
	u32 *porth;
	u16 *handles, *group;
	int chcnt;
	u32 rx_sampleszbits = 16, rx_samplerate = 48000;

	if (stashed_slim_dev == NULL) {
		dev_err(madera->dev, "slim device undefined.\n");
		return -EINVAL;
	}
	mutex_lock(&slim_rx_lock);
	switch (w->shift) {
	case MADERA_SLIMRX1_ENA_SHIFT:
		dev_dbg(madera->dev, "RX1\n");
		chcnt = priv->rx_chan_map_num[0];
		if (chcnt > ARRAY_SIZE(rx_porth1)) {
			dev_err(madera->dev, "ERROR: Too many RX channels\n");
			ret = -EINVAL;
			goto exit;
		}
		porth = rx_porth1;
		handles = rx_handles1;
		group = &rx_group1;
		rx_sampleszbits = priv->rx1_sampleszbits;
		rx_samplerate = priv->rx1_samplerate;
		break;
	case MADERA_SLIMRX5_ENA_SHIFT:
		dev_dbg(madera->dev, "RX2\n");
		chcnt = priv->rx_chan_map_num[1];
		if (chcnt > ARRAY_SIZE(rx_porth2)) {
			dev_err(madera->dev, "ERROR: Too many RX channels\n");
			ret = -EINVAL;
			goto exit;
		}
		porth = rx_porth2;
		handles = rx_handles2;
		group = &rx_group2;
		rx_sampleszbits = priv->rx2_sampleszbits;
		rx_samplerate = priv->rx2_samplerate;
		break;
	case MADERA_SLIMRX3_ENA_SHIFT:
		dev_dbg(codec->dev, "RX3\n");
		chcnt = priv->rx_chan_map_num[2];
		if (chcnt > ARRAY_SIZE(rx_porth3)) {
			dev_err(madera->dev, "ERROR: Too many RX channels\n");
			ret = -EINVAL;
			goto exit;
		}
		porth = rx_porth3;
		handles = rx_handles3;
		group = &rx_group3;
		break;
	default:
		goto exit;
	}
	madera_slim_fixup_prop(&prop, rx_samplerate, rx_sampleszbits);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
	case SND_SOC_DAPM_POST_PMU:
		dev_dbg(madera->dev, "Start slimbus RX, rate=%d, bit=%d\n",
			rx_samplerate, rx_sampleszbits);
		ret = slim_define_ch(stashed_slim_dev, &prop,
				     handles, chcnt, true, group);
		if (ret != 0) {
			dev_err(madera->dev, "slim_define_ch() failed: %d\n",
				ret);
			return ret;
		}

		for (i = 0; i < chcnt; i++) {
			ret = slim_connect_sink(stashed_slim_dev, &porth[i], 1,
						handles[i]);
			if (ret != 0) {
				dev_err(madera->dev, "snk connect fail %d:%d\n",
					i, ret);
				goto exit;
			}
		}

		ret = slim_control_ch(stashed_slim_dev, *group,
					SLIM_CH_ACTIVATE, true);
		if (ret != 0) {
			dev_err(madera->dev, "Failed to activate: %d\n", ret);
			goto exit;
		}
		break;

	case SND_SOC_DAPM_POST_PMD:
	case SND_SOC_DAPM_PRE_PMD:
		dev_dbg(madera->dev, "Stop slimbus Rx\n");
		ret = slim_control_ch(stashed_slim_dev, *group,
					SLIM_CH_REMOVE, true);
		if (ret != 0)
			dev_err(madera->dev, "Failed to remove rx: %d\n", ret);

		break;
	default:
		break;
	}

exit:
	mutex_unlock(&slim_rx_lock);
	return ret;
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
	u32 *tx_porth, *rx_porth;
	u16 *tx_handles, *rx_handles;
	int tx_chcnt, rx_chcnt, tx_idx_step, rx_idx_step;
	int tx_stream_idx, rx_stream_idx;
	int *tx_priv_counter, *rx_priv_counter;
	u16 *tx_chan_map_slot, *rx_chan_map_slot;

	if (stashed_slim_dev == NULL) {
		dev_err(madera->dev, "%s No slim device available\n",
			__func__);
		return -EINVAL;
	}

	if (!priv->slim_logic_addr) {
		madera_slim_get_la(stashed_slim_dev, &laddr);
		priv->slim_logic_addr = laddr;
	} else {
		laddr = priv->slim_logic_addr;
	}

	switch (dai->id) {
	case 4: /* cs47l35-slim1 */
		tx_porth = tx_porth1;
		tx_handles = tx_handles1;
		tx_chcnt = ARRAY_SIZE(tx_porth1);
		tx_idx_step = 8;
		tx_stream_idx = TX_STREAM_1;
		tx_priv_counter = &priv->tx_chan_map_num[0];
		tx_chan_map_slot = priv->tx_chan_map_slot[0];

		rx_porth = rx_porth1;
		rx_handles = rx_handles1;
		rx_chcnt = ARRAY_SIZE(rx_porth1);
		rx_idx_step = 0;
		rx_stream_idx = RX_STREAM_1;
		rx_priv_counter = &priv->rx_chan_map_num[0];
		rx_chan_map_slot = priv->rx_chan_map_slot[0];
		break;
	case 5: /* cs47l35-slim2 */
		tx_porth = tx_porth2;
		tx_handles = tx_handles2;
		tx_chcnt = ARRAY_SIZE(tx_porth2);
		tx_idx_step = 12;
		tx_stream_idx = TX_STREAM_2;
		tx_priv_counter = &priv->tx_chan_map_num[1];
		tx_chan_map_slot = priv->tx_chan_map_slot[1];

		rx_porth = rx_porth2;
		rx_handles = rx_handles2;
		rx_chcnt = ARRAY_SIZE(rx_porth2);
		rx_idx_step = 4;
		rx_stream_idx = RX_STREAM_2;
		rx_priv_counter = &priv->rx_chan_map_num[1];
		rx_chan_map_slot = priv->rx_chan_map_slot[1];
		break;
	default:
		dev_err(madera->dev, "set_channel_map unknown dai->id %d\n",
			dai->id);
		return -EINVAL;
	}

	if (rx_num > rx_chcnt)
		rx_num = rx_chcnt;
	if (rx_num > 0)
		*rx_priv_counter = rx_num;

	if (tx_num > tx_chcnt)
		tx_num = tx_chcnt;
	if (tx_num > 0)
		*tx_priv_counter = tx_num;

	/* This actually allocates the channel or refcounts it if there... */
	for (i = 0; i < rx_num; i++) {
		slim_get_slaveport(laddr, i + rx_idx_step, &rx_porth[i],
				   SLIM_SINK);

		ret = slim_query_ch(stashed_slim_dev, rx_stream_idx + i,
				    &rx_handles[i]);
		if (ret != 0) {
			dev_err(madera->dev, "slim_alloc_ch() failed: %d\n",
				ret);
			return ret;
		}

		rx_chan_map_slot[i] = rx_slot[i] = rx_stream_idx + i;
	}

	for (i = 0; i < tx_num; i++) {
		slim_get_slaveport(laddr, i + tx_idx_step, &tx_porth[i],
				   SLIM_SRC);

		ret = slim_query_ch(stashed_slim_dev, tx_stream_idx + i,
				    &tx_handles[i]);
		if (ret != 0) {
			dev_err(madera->dev, "slim_alloc_ch() failed: %d\n",
				ret);
			return ret;
		}

		tx_chan_map_slot[i] = tx_slot[i] = tx_stream_idx + i;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(madera_set_channel_map);

int madera_get_channel_map(struct snd_soc_dai *dai,
			   unsigned int *tx_num, unsigned int *tx_slot,
			   unsigned int *rx_num, unsigned int *rx_slot)
{
	struct madera_priv *priv = snd_soc_codec_get_drvdata(dai->codec);
	struct madera *madera = priv->madera;
	int i;
	int tx_chan_map_num, rx_chan_map_num;
	u16 *tx_chan_map_slot, *rx_chan_map_slot;

	switch (dai->id) {
	case 4: /* cs47l35-slim1 */
		tx_chan_map_num = priv->tx_chan_map_num[0];
		rx_chan_map_num = priv->rx_chan_map_num[0];
		tx_chan_map_slot = priv->tx_chan_map_slot[0];
		rx_chan_map_slot = priv->rx_chan_map_slot[0];
		break;
	case 5: /* cs47l35-slim2 */
		tx_chan_map_num = priv->tx_chan_map_num[1];
		rx_chan_map_num = priv->rx_chan_map_num[1];
		tx_chan_map_slot = priv->tx_chan_map_slot[1];
		rx_chan_map_slot = priv->rx_chan_map_slot[1];
		break;
	default:
		dev_err(madera->dev, "get_channel_map unknown dai->id %d\n",
			dai->id);
		return -EINVAL;
	}

	if (!rx_chan_map_num || !tx_chan_map_num)
		return -EINVAL;

	*rx_num = rx_chan_map_num;
	*tx_num = tx_chan_map_num;

	for (i = 0; i < rx_chan_map_num; i++)
		rx_slot[i] = rx_chan_map_slot[i];

	for (i = 0; i < tx_chan_map_num; i++)
		tx_slot[i] = tx_chan_map_slot[i];

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
	dev_info(&slim->dev, "%s SLIM PROBE\n", __func__);
	mutex_init(&slim_tx_lock);
	mutex_init(&slim_rx_lock);

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
