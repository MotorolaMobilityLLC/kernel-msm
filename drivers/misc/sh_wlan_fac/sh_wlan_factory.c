/* drivers/misc/sh_wlan_factory.c
 *
 * Copyright (C) 2016 Sharp Corporation
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <soc/qcom/sh_smem.h>

static char *wlanmac_from_smem = NULL;
static char wlanmac_from_smem_buf[18];
module_param(wlanmac_from_smem, charp, S_IRUGO);
MODULE_PARM_DESC(wlanmac_from_smem, "store the wlan mac");

static char *nv_tx_level = NULL;
static char nv_tx_level_buf[4];
module_param(nv_tx_level, charp, S_IRUGO);
MODULE_PARM_DESC(nv_tx_level, "NV TX level (NV switching info)");

static int __init sh_wlan_fac_init(void){
	sharp_smem_common_type *SMemCommAdrP;

	printk(KERN_INFO "%s: enter\n", __FUNCTION__);
	memset(wlanmac_from_smem_buf, 0, 18);
	wlanmac_from_smem = wlanmac_from_smem_buf;
	SMemCommAdrP = sh_smem_get_common_address();
	if(SMemCommAdrP != NULL){
		snprintf(wlanmac_from_smem_buf, 18, "%02x:%02x:%02x:%02x:%02x:%02x",
			(unsigned char)SMemCommAdrP->shdarea_WlanMacAddress[0],
			(unsigned char)SMemCommAdrP->shdarea_WlanMacAddress[1],
			(unsigned char)SMemCommAdrP->shdarea_WlanMacAddress[2],
			(unsigned char)SMemCommAdrP->shdarea_WlanMacAddress[3],
			(unsigned char)SMemCommAdrP->shdarea_WlanMacAddress[4],
			(unsigned char)SMemCommAdrP->shdarea_WlanMacAddress[5]);
		printk(KERN_INFO "%s: MAC address is %s\n", __FUNCTION__, wlanmac_from_smem_buf);
	}else{
		snprintf(wlanmac_from_smem_buf, 18, "00:00:00:00:00:00");
		printk(KERN_ERR "%s: SMemCommAdrP is NULL, MAC address is set to all zero\n", __FUNCTION__);
	}

	memset(nv_tx_level_buf, 0, 4);
	nv_tx_level = nv_tx_level_buf;
	/* Tx level default to M */
	nv_tx_level_buf[0] = 'M';
	if(SMemCommAdrP != NULL){
		typedef struct{
			char			version[4];
			signed char	nv_switch_2_4G;
			signed char	nv_switch_5G;
			char			padding;
			char			nv_data_count;
		}TS_SHDIAG_WIFI_INFO;

		const char pt_suffix_table[] = "HML";
		TS_SHDIAG_WIFI_INFO *WifiInfo;
		int nv_switch_2_4G;

		WifiInfo = (TS_SHDIAG_WIFI_INFO *)(sh_smem_get_common_address()->shdarea_WlanNVSwitch);

		nv_switch_2_4G = (int)WifiInfo->nv_switch_2_4G;
		if(nv_switch_2_4G < -1 || 1 < nv_switch_2_4G){
			printk(KERN_ERR "%s: NV file switching flag for 2.4GHz has invalid value(%d)\n", __FUNCTION__, nv_switch_2_4G);
			nv_switch_2_4G = 0;
		}
		nv_tx_level_buf[0] = pt_suffix_table[nv_switch_2_4G + 1];
		printk(KERN_INFO "%s: NV TX level is %c\n", __FUNCTION__, nv_tx_level_buf[0]);
	}

	printk(KERN_INFO "%s: leave\n", __FUNCTION__);
	return 0;
}

static void __exit sh_wlan_fac_exit(void){
}

module_init(sh_wlan_fac_init);
module_exit(sh_wlan_fac_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("SHARP CORPORATION");
MODULE_DESCRIPTION("SHARP WLAN FACTORY MODULE");
