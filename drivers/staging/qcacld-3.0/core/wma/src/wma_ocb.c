/*
 * Copyright (c) 2013-2019 The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: wma_ocb.c
 *
 * WLAN Host Device Driver 802.11p OCB implementation
 */

#include "wma_ocb.h"
#include "cds_utils.h"
#include "cds_api.h"
#include "wlan_ocb_ucfg_api.h"
#include "lim_utils.h"
#include "../../core/src/vdev_mgr_ops.h"

/**
 * wma_start_ocb_vdev() - start OCB vdev
 * @config: ocb channel config
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
static QDF_STATUS wma_start_ocb_vdev(struct ocb_config *config)
{
	QDF_STATUS status;
	tp_wma_handle wma = cds_get_context(QDF_MODULE_ID_WMA);
	struct wlan_objmgr_vdev *vdev;
	struct vdev_mlme_obj *mlme_obj;
	struct wlan_channel *des_chan;
	uint8_t dot11_mode;

	vdev = wma->interfaces[config->vdev_id].vdev;
	if (!vdev) {
		wma_err("vdev is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mlme_obj = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!mlme_obj) {
		wma_err("vdev component object is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	des_chan = vdev->vdev_mlme.des_chan;

	des_chan->ch_freq = config->channels[0].chan_freq;
	if (wlan_reg_is_24ghz_ch_freq(des_chan->ch_freq))
		dot11_mode = MLME_DOT11_MODE_11G;
	else
		dot11_mode = MLME_DOT11_MODE_11A;
	des_chan->ch_ieee =
		wlan_reg_freq_to_chan(wma->pdev, des_chan->ch_freq);

	status = lim_set_ch_phy_mode(vdev, dot11_mode);
	if (QDF_IS_STATUS_ERROR(status))
		return QDF_STATUS_E_FAILURE;

	mlme_obj->mgmt.chainmask_info.num_rx_chain = 2;
	mlme_obj->mgmt.chainmask_info.num_tx_chain = 2;

	status = wma_vdev_pre_start(config->vdev_id, false);
	if (status != QDF_STATUS_SUCCESS)
		return status;

	status = vdev_mgr_start_send(mlme_obj, false);

	return status;
}

QDF_STATUS wma_ocb_register_callbacks(tp_wma_handle wma_handle)
{
	ucfg_ocb_register_vdev_start(wma_handle->pdev, wma_start_ocb_vdev);

	return QDF_STATUS_SUCCESS;
}
