/* SPDX-License-Identifier: GPL-2.0 */
/*******************************************************************************
 *     Copyright (c) 2022    ASIX Electronic Corporation    All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <https://www.gnu.org/licenses/>.
 ******************************************************************************/
#ifndef __ASIX_AX88179A_772D_H
#define __ASIX_AX88179A_772D_H

#define AX88179A_BIN_TIMER_UINT		800		//ns

#define AX88179A_NAPI_WEIGHT		64
#define AX88179A_BUF_RX_SIZE		(48 * 1024)
#define AX88179A_PHY_ID			0x03
#define AX88179_GPHY_CTRL		0x0F
	#define AX_GPHY_EEE_CTRL		0x01
#define AX88179A_HIGH_QUEUE_POINT	0x60
#define AX88179A_AUTODETACH_DELAY	(5UL << 8)

#define AX88179A_PBUS_REG		0x10
#define AX88179A_ACCESS_BL		0x2A
#define AX88179A_PHY_CLAUSE45		0x27
#define AX88179A_PHY_POWER		0x31
	#define AX_PHY_POWER			0x02
#define AX88179A_BOOT_TO_ROM		0x9F
#define AX88179A_AUTODETACH		0xC0

#define AX88179A_BFM_DATA		0x0E
	#define AX_TX_QUEUE_CFG		0x02
	#define AX_TX_QUEUE_SET		0x08
	#define AX_TX_Q1_AHB_FC_EN		0x10
	#define AX_TX_Q2_AHB_FC_EN		0x20
	#define AX_XGMII_EN			0x80
#define AX88179A_ETH_TX_GAP		0x0D
#define AX88179A_FLASH_READ		0x21
#define AX88179A_FLASH_WEN		0x22
#define AX88179A_FLASH_WDIS		0x23
#define AX88179A_FLASH_WRITE		0x24
#define AX88179A_FLASH_EARSE_ALL	0x25
#define AX88179A_VLAN_ID_CONTROL	0x2B
	#define AX_VLAN_CONTROL_WE		0x0001
	#define AX_VLAN_CONTROL_RD		0x0002
	#define AX_VLAN_CONTROL_VSO		0x0010
	#define AX_VLAN_CONTROL_VFE		0x0020
#define AX88179A_MAC_BM_INT_MASK	0x41
#define AX88179A_MAC_BM_RX_DMA_CTL	0x43
#define AX88179A_MAC_BM_TX_DMA_CTL	0x46
#define AX88179A_CLK_EN_ARRAY_1		0x4B
	#define AX_MAC_MII_TX_25M_EN		0x02
	#define AX_MAC_RX_25M_EN		0x08
#define AX88179A_MAC_CLK_SELECT_1	0x4D
	#define AX_MAC_PCSCLK_MII_TX		0x01
#define AX88178A_MAC_RX_STATUS_CDC	0x6D
#define AX88179A_MAC_LSOFC_GMIIPF	0x6E
	#define AX_GMII_CRC_APPEND		0x10
	#define AX_LSOFC_WCNT_1_ACCESS		0x00
	#define AX_LSOFC_WCNT_2_ACCESS		0x01
	#define AX_LSOFC_WCNT_5_ACCESS		0x02
	#define AX_LSOFC_WCNT_7_ACCESS		0x03
#define AX88179A_MAC_RX_FILTER_CTRL	0x6F
	#define AX_MAC_RX_FILTER_OFT_EN	0x80
#define AX88179A_MAC_QUEUE_POINT	0x7F
#define AX88179A_MAC_ARC_CTRL		0x9E
#define AX88179A_CDC_ECM_CTRL		0xB0
#define AX88179A_MAC_SWP_CTRL		0xB1
#define AX88179A_MAC_TX_PAUSE_0		0xB2
#define AX88179A_MAC_TX_PAUSE_1		0xB3
#define AX88179A_MAC_TX_PAUSE_2		0xB4
#define AX88179A_MAC_CDC_DELAY_TX	0xB5
#define AX88179A_MAC_PATH		0xB7
	#define AX_MAC_RX_PATH_READY		0x01
	#define AX_MAC_TX_PATH_READY		0x02
#define AX88179A_NEW_PAUSE_CTRL		0xB8
	#define AX_NEW_PAUSE_EN			0x01
#define AX88179A_MAC_BULK_OUT_CTRL	0xB9
	#define AX_MAC_EFF_EN			0x02
#define AX88179A_MAC_RX_DATA_CDC_CNT	0xC0
#define AX88179A_MAC_BFM_CTRL		0xC1
	#define AX_MAC_STOP_EP5_ACCESS		0x01
	#define AX_MAC_STOP_EP3_ACCESS		0x02
	#define AX_MAC_LSO_ERR_EN		0x04
	#define AX_MAC_MIQFFCTRL_FORMAT		0x10
	#define AX_MAC_MIQFFCTRL_DROP_CRC	0x20
#define AX88179A_MAC_LSO_ENHANCE_CTRL	0xC3
	#define AX_LSO_ENHANCE_EN		0x01
#define AX88179A_MAC_TX_HDR_CKSUM	0xCC
	#define AX_TXHDR_CKSUM_EN		0x01
	#define AX_TXRX_INDV_RESET_EN		0x02
#define AX88179A_MAC_CPU_CTRL_MAC_1	0xCE
	#define AX_RX_INDV_RESET		0x01
	#define AX_TX_INDV_RESET		0x02
#define AX88179A_EP5_EHR		0xF9
	#define AX_EP5_DAT_ERROR_HANDLE		0x80
#define AX88179A_HW_EC_VERSION		0xFB
#define AX88179A_SW_REVERSION		0xFC
	#define AX88179A_FLASH_MODE		0x80

#define PHY_1000M_STS			0x11
	#define LINK_1000M_OK		0x1000
#define PHY_100M_STS			0x10
	#define LINK_100M_OK		0x1000
#define PHY_10M_STS			0x16
	#define LINK_10M_OK		0x40


struct _179a_rx_pkt_header {
	u16	L4_err		:1,
		L3_err		:1,
		L4_pkt_type	:3,
		L3_pkt_type	:2,
		CE		:1,
		TCO_match	:1,
		node_ID_match	:1,
		vlan_ind	:1,
		RxOk		:1,
		reserved2	:3,
		BMC		:1;
	u16	length		:15,
		drop		:1;
	u16	vlan_tag;
	u16	WUF_detect	:1,
		WUF_wake	:1,
		WUF_ind		:6,
		PTP_ind		:1,
		reserved	:7;
};

struct _179a_rx_header {
	u32	pkt_cnt	:13,
		hdr_off	:19;
	u32	rxThroughput;
};
#define AX88179A_RX_HEADER_SIZE	sizeof(struct _179a_rx_header)

struct _179a_tx_pkt_header {
	u32	length		:21,
		checksum	:7,
		padding		:1,
		vlan_tag	:1,
		CPHI		:1,
		DICF		:1;
	u32	max_seg_size	:15,
		reserved	:1,
		vlan_info	:16;
};
#define AX88179A_TX_HEADER_SIZE	sizeof(struct _179a_tx_pkt_header)

#define AX88179A_TX_HERDER_CHKSUM(len)  ((tx_hdr->length + \
					(tx_hdr->length >> 8) + \
					((tx_hdr->length >> 16) & 0x1F)) & 0x7F)

extern const struct net_device_ops ax88179a_netdev_ops;
extern const struct driver_info ax88179a_info;
extern const struct driver_info ax88772d_info;
#if KERNEL_VERSION(5, 15, 0) <= LINUX_VERSION_CODE
int ax88179a_siocdevprivate(struct net_device *netdev, struct ifreq *rq,
			    void __user *udata, int cmd);
#endif
int ax88179a_ioctl(struct net_device *net, struct ifreq *rq, int cmd);
void ax88179a_set_multicast(struct net_device *net);
#endif /* End of __ASIX_AX88179A_772D_H */
