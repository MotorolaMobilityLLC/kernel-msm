// SPDX-License-Identifier: GPL-2.0
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
#include "ax_main.h"
#include "ax88179a_772d.h"

struct _ax_buikin_setting AX88179A_BULKIN_SIZE[] = {
	{5, 0x7B, 0x00,	0x18, 0x0F},	//1G, SS
	{5, 0xC0, 0x02,	0x06, 0x0F},	//1G, HS
	{7, 0xF0, 0x00,	0x0C, 0x0F},	//100M, Full, SS
	{6, 0x00, 0x00,	0x06, 0x0F},	//100M, Half, SS
	{5, 0xC0, 0x04,	0x06, 0x0F},	//100M, Full, HS
	{7, 0xC0, 0x04,	0x06, 0x0F},	//100M, Half, HS
	{7, 0x00, 0,	0x03, 0x3F},	//FS
};
static int ax88179a_chk_eee(struct ax_device *axdev)
{
	struct ethtool_cmd ecmd = { .cmd = ETHTOOL_GSET };

	mii_ethtool_gset(&axdev->mii, &ecmd);

	if (ecmd.speed == SPEED_1000) {
		int eee_lp, eee_cap, eee_adv;
		u32 lp, cap, adv;

		eee_cap = ax_mmd_read(axdev->netdev,
				      MDIO_MMD_PCS, MDIO_PCS_EEE_ABLE);
		if (eee_cap < 0)
			goto exit;
		eee_cap &= ~MDIO_EEE_100TX;

		cap = mmd_eee_cap_to_ethtool_sup_t(eee_cap);
		if (!cap)
			goto exit;

		eee_lp = ax_mmd_read(axdev->netdev,
				     MDIO_MMD_AN, MDIO_AN_EEE_LPABLE);
		if (eee_lp < 0)
			goto exit;

		eee_adv = ax_mmd_read(axdev->netdev,
				      MDIO_MMD_AN, MDIO_AN_EEE_ADV);
		if (eee_adv < 0)
			goto exit;

		adv = mmd_eee_adv_to_ethtool_adv_t(eee_adv);
		lp = mmd_eee_adv_to_ethtool_adv_t(eee_lp);
		if (!(lp & adv & SUPPORTED_1000baseT_Full))
			goto exit;

		axdev->eee_active = 1;
		return true;
	}
exit:
	axdev->eee_active = 0;
	return false;
}

static int ax88179a_ethtool_get_eee(struct ax_device *axdev,
				    struct ethtool_eee *data)
{
	int val;

	val = ax_mmd_read(axdev->netdev, MDIO_MMD_PCS, MDIO_PCS_EEE_ABLE);
	if (val < 0)
		return val;
	val &= ~MDIO_EEE_100TX;
	data->supported = mmd_eee_cap_to_ethtool_sup_t(val);

	val = ax_mmd_read(axdev->netdev, MDIO_MMD_AN, MDIO_AN_EEE_ADV);
	if (val < 0)
		return val;
	data->advertised = mmd_eee_adv_to_ethtool_adv_t(val);

	val = ax_mmd_read(axdev->netdev, MDIO_MMD_AN, MDIO_AN_EEE_LPABLE);
	if (val < 0)
		return val;
	data->lp_advertised = mmd_eee_adv_to_ethtool_adv_t(val);

	return 0;
}

static int ax88179a_get_eee(struct net_device *net, struct ethtool_eee *edata)
{
	struct ax_device *axdev = netdev_priv(net);

	edata->eee_enabled = axdev->eee_enabled;
	edata->eee_active = axdev->eee_active;

	return ax88179a_ethtool_get_eee(axdev, edata);
}

static void ax88179a_eee_setting(struct ax_device *axdev, bool enable)
{
	ax_write_cmd(axdev, AX88179_GPHY_CTRL, AX_GPHY_EEE_CTRL,
		     enable, 0, NULL);
}

static int ax88179a_set_eee(struct net_device *net, struct ethtool_eee *edata)
{
	struct ax_device *axdev = netdev_priv(net);

	if (edata->advertised & MDIO_EEE_100TX)
		return -EOPNOTSUPP;

	axdev->eee_enabled = edata->eee_enabled;
	ax88179a_eee_setting(axdev, axdev->eee_enabled);
	if (axdev->eee_enabled) {
		axdev->eee_enabled = ax88179a_chk_eee(axdev);
		if (!axdev->eee_enabled) {
			ax88179a_eee_setting(axdev, false);
			return -EOPNOTSUPP;
		}
	}

	mii_nway_restart(&axdev->mii);

	return 0;
}
#if KERNEL_VERSION(5, 15, 0) <= LINUX_VERSION_CODE
static int ax88179a_get_coalesce(struct net_device *netdev,
				 struct ethtool_coalesce *coalesce,
				 struct kernel_ethtool_coalesce *kernel_coal,
				 struct netlink_ext_ack *extack)
#else
static int ax88179a_get_coalesce(struct net_device *netdev,
				 struct ethtool_coalesce *coalesce)
#endif
{
	struct ax_device *axdev = netdev_priv(netdev);

	coalesce->rx_coalesce_usecs = axdev->coalesce;

	return 0;
}

static u16 ax88179a_usec_to_bin_timer(struct ax_device *axdev)
{
	u16 speed_multiple;

	switch (axdev->link_info.eth_speed) {
	case ETHER_LINK_10:
		speed_multiple = 100;
		break;
	case ETHER_LINK_100:
		speed_multiple = 10;
		break;
	case ETHER_LINK_1000:
	default:
		speed_multiple = 1;
		break;
	};

	return (axdev->coalesce * US_TO_NS * speed_multiple) /
		AX88179A_BIN_TIMER_UINT;
}

static u32 ax88179a_bin_timer_to_usec(struct ax_device *axdev, u16 timer)
{
	u16 speed_multiple;

	switch (axdev->link_info.eth_speed) {
	case ETHER_LINK_10:
		speed_multiple = 100;
		break;
	case ETHER_LINK_100:
		speed_multiple = 10;
		break;
	case ETHER_LINK_1000:
	default:
		speed_multiple = 1;
		break;
	};

	return (AX88179A_BIN_TIMER_UINT * timer) / (US_TO_NS * speed_multiple);
}
#if KERNEL_VERSION(5, 15, 0) <= LINUX_VERSION_CODE
static int ax88179a_set_coalesce(struct net_device *netdev,
				 struct ethtool_coalesce *coalesce,
				 struct kernel_ethtool_coalesce *kernel_coal,
				 struct netlink_ext_ack *extack)
#else
static int ax88179a_set_coalesce(struct net_device *netdev,
				 struct ethtool_coalesce *coalesce)
#endif
{
	struct ax_device *axdev = netdev_priv(netdev);
	u16 timer;
	int ret = 0;

	ret = usb_autopm_get_interface(axdev->intf);
	if (ret < 0)
		return ret;

	axdev->coalesce = coalesce->rx_coalesce_usecs;

	timer = ax88179a_usec_to_bin_timer(axdev);
	if (timer > 0) {
		timer &= 0x7FFF;
		ax_write_cmd(axdev, AX_ACCESS_MAC, AX_RX_BULKIN_QTIMR_LOW,
			     2, 2, &timer);
	}

	usb_autopm_put_interface(axdev->intf);

	return ret;
}


const struct ethtool_ops ax88179a_ethtool_ops = {
#if KERNEL_VERSION(5, 7, 0) < LINUX_VERSION_CODE
	.supported_coalesce_params = ETHTOOL_COALESCE_RX_USECS,
#endif
	.get_drvinfo	= ax_get_drvinfo,
#if KERNEL_VERSION(4, 10, 0) > LINUX_VERSION_CODE
	.get_settings	= ax_get_settings,
	.set_settings	= ax_set_settings,
#else
	.get_link_ksettings = ax_get_link_ksettings,
	.set_link_ksettings = ax_set_link_ksettings,
#endif
	.get_link	= ethtool_op_get_link,
	.get_msglevel	= ax_get_msglevel,
	.set_msglevel	= ax_set_msglevel,
	.get_wol	= ax_get_wol,
	.set_wol	= ax_set_wol,
	.get_eee	= ax88179a_get_eee,
	.set_eee	= ax88179a_set_eee,
	.get_coalesce	= ax88179a_get_coalesce,
	.set_coalesce	= ax88179a_set_coalesce,
	.get_strings	= ax_get_strings,
	.get_sset_count = ax_get_sset_count,
	.get_ethtool_stats = ax_get_ethtool_stats,
	.get_pauseparam = ax_get_pauseparam,
	.set_pauseparam = ax_set_pauseparam,
	.get_regs_len	= ax_get_regs_len,
	.get_regs	= ax_get_regs,
	.get_ts_info	= ethtool_op_get_ts_info,
};

void ax88179a_get_fw_version(struct ax_device *axdev)
{
	int i;

	for (i = 0; i < 3; i++) {
		if (ax_read_cmd(axdev, AX88179A_ACCESS_BL, (0xFD + i),
				1, 1, &axdev->fw_version[i], 1) < 0) {
			axdev->fw_version[i] = ~0;
		}
	}

	if (ax_read_cmd(axdev, AX88179A_ACCESS_BL, AX88179A_SW_REVERSION, 1, 1,
			&axdev->fw_version[3], 0) < 0)
		axdev->fw_version[3] = ~0;
	else
		axdev->fw_version[3] &= 0xF;
}

int ax88179a_signature(struct ax_device *axdev, struct _ax_ioctl_command *info)
{
	strlcpy(info->sig, AX88179A_SIGNATURE, sizeof(info->sig));
	return 0;
}

int ax88179a_read_version(struct ax_device *axdev,
			  struct _ax_ioctl_command *info)
{
	unsigned char temp[16] = {0};

	snprintf(temp, sizeof(temp), "v%d.%d.%d.%d",
		axdev->fw_version[0], axdev->fw_version[1],
		axdev->fw_version[2], axdev->fw_version[3]);

	memcpy(&info->version.version, temp, 16);

	return 0;
}

int ax88179a_write_flash(struct ax_device *axdev,
			 struct _ax_ioctl_command *info)
{
	int i, ret;
	u8 *buf = NULL;
	u8 *block;

	buf = kzalloc(256, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	block = buf;

	ret = ax_write_cmd(axdev, AX88179A_FLASH_WEN, 0, 0, 0, NULL);
	if (ret < 0) {
		netdev_err(axdev->netdev, "Flash write enable failed");
		info->flash.status = -ERR_FALSH_WRITE_EN;
		goto out;
	}

	for (i = info->flash.offset;
	     i < (info->flash.length + info->flash.offset);
	     i += 256) {
		if (copy_from_user(block,
				   (void __user *)&info->flash.buf[i], 256)) {
			ret = -EFAULT;
			goto out;
		}

		ret = ax_write_cmd(axdev, AX88179A_FLASH_WRITE,
					(u16)((i >> 16) & 0xFFFF),
					(u16)(i & 0xFFFF), 256, block);
		if (ret < 0) {
			info->flash.status = -ERR_FALSH_WRITE;
			goto out;
		}
	}

	ret = ax_write_cmd(axdev, AX88179A_FLASH_WDIS, 0, 0, 0, NULL);
	if (ret < 0) {
		netdev_err(axdev->netdev, "Flash write disable failed");
		info->flash.status = -ERR_FALSH_WRITE_DIS;
		return ret;
	}
out:
	kfree(buf);

	return ret;
}

int ax88179a_read_flash(struct ax_device *axdev, struct _ax_ioctl_command *info)
{
	int i, ret = 0;
	void *buf = NULL;
	u8 *block;

	buf = kzalloc(256, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	block = buf;

	for (i = info->flash.offset;
	     i < (info->flash.length + info->flash.offset);
	     i += 256) {
		ret = ax_read_cmd(axdev, AX88179A_FLASH_READ,
				  (u16)((i >> 16) & 0xFFFF),
				  (u16)(i & 0xFFFF), 256, block, 0);
		if (ret < 0) {
			info->flash.status = -ERR_FALSH_READ;
			break;
		}

		if (copy_to_user((void __user *)&info->flash.buf[i],
				 block, 256)) {
			ret = -EFAULT;
			break;
		}
	}

	kfree(buf);

	return ret;
}

int ax88179a_program_efuse(struct ax_device *axdev,
			   struct _ax_ioctl_command *info)
{
	int ret = 0;
	u16 offset = (u16)(info->flash.offset * 16);
	u8 buf[20] = {0};

	ret = copy_from_user(buf, (void __user *)info->flash.buf, 20);
	if (ret)
		return ret;

	ret = ax_write_cmd(axdev, AX_ACCESS_EFUSE, offset, 0, 20, buf);
	if (ret < 0) {
		info->flash.status = -ERR_EFUSE_WRITE;
		return ret;
	}

	return ret;
}

int ax88179a_dump_efuse(struct ax_device *axdev, struct _ax_ioctl_command *info)
{
	int ret = 0;
	u16 offset = (u16)(info->flash.offset * 16);
	u8 buf[20] = {0};

	ret = ax_read_cmd(axdev, AX_ACCESS_EFUSE, offset, 0, 20, buf, 0);
	if (ret < 0) {
		info->flash.status = -ERR_EFUSE_READ;
		return ret;
	}

	ret = copy_to_user((void __user *)info->flash.buf, buf, 20);

	return ret;
}

int ax88179a_boot_to_rom(struct ax_device *axdev,
			 struct _ax_ioctl_command *info)
{
	usb_control_msg(axdev->udev, usb_sndctrlpipe(axdev->udev, 0),
			AX88179A_BOOT_TO_ROM,
			USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			0x5A5A, 0xA5A5, NULL, 0, 1);

	return 0;
}

int ax88179a_erase_flash(struct ax_device *axdev,
			 struct _ax_ioctl_command *info)
{
	int ret = 0;

	ret = ax_write_cmd(axdev, AX88179A_FLASH_WEN, 0, 0, 0, NULL);
	if (ret < 0) {
		netdev_err(axdev->netdev, "Flash write enable failed");
		info->flash.status = -ERR_FALSH_WRITE_EN;
		return ret;
	}

	ret = usb_control_msg(axdev->udev, usb_sndctrlpipe(axdev->udev, 0),
			      AX88179A_FLASH_EARSE_ALL,
			      USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			      0, 0, NULL, 0, 300000);
	if (ret < 0) {
		netdev_err(axdev->netdev, "Flash erase all failed");
		info->flash.status = -ERR_FALSH_ERASE_ALL;
		return ret;
	}

	ret = ax_write_cmd(axdev, AX88179A_FLASH_WDIS, 0, 0, 0, NULL);
	if (ret < 0) {
		netdev_err(axdev->netdev, "Flash write disable failed");
		info->flash.status = -ERR_FALSH_WRITE_DIS;
		return ret;
	}

	return 0;
}

int ax88179a_sw_reset(struct ax_device *axdev, struct _ax_ioctl_command *info)
{
	void *buf = NULL;

	buf = kzalloc(sizeof(u32), GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	*((u32 *)buf) = 1;

	usb_control_msg(axdev->udev, usb_sndctrlpipe(axdev->udev, 0), 0x10,
			USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			0x18E8, 0x000F, buf, 4, 10);

	kfree(buf);

	return 0;
}

IOCTRL_TABLE ax88179a_tbl[] = {
	ax88179a_signature,
	ax_usb_command,
	ax88179a_read_version,
	ax88179a_write_flash,
	ax88179a_boot_to_rom,
	ax88179a_erase_flash,
	ax88179a_sw_reset,
	ax88179a_read_flash,
	ax88179a_program_efuse,
	ax88179a_dump_efuse,
};


#if KERNEL_VERSION(5, 15, 0) <= LINUX_VERSION_CODE
int ax88179a_siocdevprivate(struct net_device *netdev, struct ifreq *rq,
			    void __user *udata, int cmd)
{
	struct ax_device *axdev = netdev_priv(netdev);
	struct _ax_ioctl_command info;
	void __user *uptr = (void __user *) rq->ifr_data;
	int ret = 0;

	switch (cmd) {
	case AX_PRIVATE:
		if (copy_from_user(&info, uptr,
				   sizeof(struct _ax_ioctl_command))) {
			netdev_err(netdev, "copy_from_user, return -EFAULT");
			return -EFAULT;
		}

		ret = (*ax88179a_tbl[info.ioctl_cmd])(axdev, &info);
		if (ret < 0) {
			netdev_err(netdev, "ax88179a_tbl, return %d", ret);
			return ret;
		}

		if (copy_to_user(uptr, &info,
				 sizeof(struct _ax_ioctl_command))) {
			netdev_err(netdev, "copy_to_user, return -EFAULT");
			return -EFAULT;
		}

		break;
	default:
		ret = -EOPNOTSUPP;
	}

	return ret;
}

int ax88179a_ioctl(struct net_device *netdev, struct ifreq *rq, int cmd)
{
	struct ax_device *axdev = netdev_priv(netdev);

	switch (cmd) {
	}
	return generic_mii_ioctl(&axdev->mii, if_mii(rq), cmd, NULL);
}
#else
int ax88179a_ioctl(struct net_device *netdev, struct ifreq *rq, int cmd)
{
	struct ax_device *axdev = netdev_priv(netdev);
	struct _ax_ioctl_command info;
	void __user *uptr = (void __user *) rq->ifr_data;
	int ret;

	switch (cmd) {
	case AX_PRIVATE:
		if (copy_from_user(&info, uptr,
				   sizeof(struct _ax_ioctl_command))) {
			netdev_err(netdev, "copy_from_user, return -EFAULT");
			return -EFAULT;
		}

		ret = (*ax88179a_tbl[info.ioctl_cmd])(axdev, &info);
		if (ret < 0) {
			netdev_err(netdev, "ax88179a_tbl, return %d", ret);
			return ret;
		}

		if (copy_to_user(uptr, &info,
				 sizeof(struct _ax_ioctl_command))) {
			netdev_err(netdev, "copy_to_user, return -EFAULT");
			return -EFAULT;
		}

		break;
	default:
		return generic_mii_ioctl(&axdev->mii, if_mii(rq), cmd, NULL);
	}
	return 0;
}
#endif

static int ax88179a_autodetach(struct ax_device *axdev)
{
	u16 value = ((axdev->autodetach) ? 1 : 0) | AX88179A_AUTODETACH_DELAY;

	return ax_write_cmd(axdev, AX88179A_AUTODETACH, value, 0, 0, NULL);
}

static bool ax88179a_check_phy_power(struct ax_device *axdev)
{
	u8 reg8 = 0;
	int ret = 0;

	ret = ax_read_cmd_nopm(axdev, AX88179A_PHY_POWER, 0, 0, 1, &reg8, 1);
	if (ret < 0)
		return false;

	return (reg8 & AX_PHY_POWER);
}

static int ax88179a_set_phy_power(struct ax_device *axdev, bool on)
{
	u8 reg8;
	int ret;

	reg8 = (on)?AX_PHY_POWER:0;
	ret = ax_write_cmd_nopm(axdev, AX88179A_PHY_POWER, 0, 0, 1, &reg8);
	if (ret < 0)
		return ret;
	msleep(250);

	return 0;
}

static int ax88179a_bind(struct ax_device *axdev)
{
	struct net_device *netdev = axdev->netdev;
	u16 wvalue = 0;
	int ret;

	ax88179a_get_fw_version(axdev);
	PRINT_VERSION(axdev, AX_DRIVER_STRING_179A_772D);

	ret = ax_write_cmd(axdev, AX_FW_MODE, AX_FW_MODE_179A, wvalue, 0, NULL);
	if (ret < 0)
		return ret;

	ret = ax_write_cmd(axdev, AX_RELOAD_FLASH_EFUSE, 0, 0, 0, NULL);
	if (ret < 0)
		return ret;

	netdev->features    |= NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM |
			       NETIF_F_SG | NETIF_F_TSO | NETIF_F_FRAGLIST |
			       NETIF_F_HW_VLAN_CTAG_RX |
			       NETIF_F_HW_VLAN_CTAG_TX;
	netdev->hw_features |= NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM |
			       NETIF_F_SG | NETIF_F_TSO | NETIF_F_FRAGLIST |
			       NETIF_F_HW_VLAN_CTAG_RX |
			       NETIF_F_HW_VLAN_CTAG_TX | NETIF_F_RXALL;
	netdev->vlan_features = NETIF_F_SG | NETIF_F_IP_CSUM | NETIF_F_TSO |
				NETIF_F_HIGHDMA | NETIF_F_FRAGLIST |
				NETIF_F_IPV6_CSUM;

	axdev->tx_casecade_size = TX_CASECADES_SIZE;
	axdev->gso_max_size = AX_GSO_DEFAULT_SIZE;
	axdev->mii.supports_gmii = true;
	axdev->mii.dev = netdev;
	axdev->mii.mdio_read = ax_mdio_read;
	axdev->mii.mdio_write = ax_mdio_write;
	axdev->mii.phy_id_mask = 0x3F;
	axdev->mii.reg_num_mask = 0x1F;
	axdev->mii.phy_id = AX88179A_PHY_ID;

	netif_set_gso_max_size(netdev, axdev->gso_max_size);

	axdev->bin_setting.custom = false;
	axdev->tx_align_len = 8;
	axdev->coalesce = 0;

	netdev->ethtool_ops = &ax88179a_ethtool_ops;
	axdev->netdev->netdev_ops = &ax88179a_netdev_ops;

	return ret;
}

static void ax88179a_unbind(struct ax_device *axdev)
{
}

static int ax88179a_stop(struct ax_device *axdev)
{
	u16 reg16;

	reg16 = AX_RX_CTL_STOP;
	ax_write_cmd(axdev, AX_ACCESS_MAC, AX_MEDIUM_STATUS_MODE, 2, 2, &reg16);

	ax88179a_set_phy_power(axdev, false);

	return 0;
}

void ax88179a_set_multicast(struct net_device *netdev)
{
	struct ax_device *axdev = netdev_priv(netdev);
	u8 *m_filter = axdev->m_filter;
	int mc_count = 0;

	if (!test_bit(AX_ENABLE, &axdev->flags))
		return;

#if KERNEL_VERSION(2, 6, 35) > LINUX_VERSION_CODE
	mc_count = net->mc_count;
#else
	mc_count = netdev_mc_count(netdev);
#endif

	axdev->rxctl = (AX_RX_CTL_START | AX_RX_CTL_AB);

	if (netdev->flags & IFF_PROMISC) {
		axdev->rxctl |= AX_RX_CTL_PRO;
	} else if (netdev->flags & IFF_ALLMULTI || mc_count > AX_MAX_MCAST) {
		axdev->rxctl |= AX_RX_CTL_AMALL;
	} else if (netdev_mc_empty(netdev)) {
	} else {
		u32 crc_bits;
#if KERNEL_VERSION(2, 6, 35) > LINUX_VERSION_CODE
		struct dev_mc_list *mc_list = netdev->mc_list;
		int i = 0;

		memset(m_filter, 0, AX_MCAST_FILTER_SIZE);
		for (i = 0; i < netdev->mc_count; i++) {
			crc_bits =
			    ether_crc(ETH_ALEN,
				      mc_list->dmi_addr) >> 26;
			*(m_filter + (crc_bits >> 3)) |=
				1 << (crc_bits & 7);
			mc_list = mc_list->next;
		}
#else
		struct netdev_hw_addr *ha = NULL;

		memset(m_filter, 0, AX_MCAST_FILTER_SIZE);
		netdev_for_each_mc_addr(ha, netdev) {
			crc_bits = ether_crc(ETH_ALEN, ha->addr) >> 26;
			*(m_filter + (crc_bits >> 3)) |= 1 << (crc_bits & 7);
		}
#endif
		ax_write_cmd_async(axdev, AX_ACCESS_MAC, AX_MULTI_FILTER_ARRY,
				   AX_MCAST_FILTER_SIZE, AX_MCAST_FILTER_SIZE,
				   m_filter);

		axdev->rxctl |= AX_RX_CTL_AM;
	}

	ax_write_cmd_async(axdev, AX_ACCESS_MAC, AX_RX_CTL,
			   2, 2, &axdev->rxctl);
}

static int ax88179a_hw_init(struct ax_device *axdev)
{
	u16 reg16;
	u8 reg8;
	int ret;

	ret = ax88179a_set_phy_power(axdev, true);
	if (ret < 0)
		return ret;
	msleep(250);

	ret = ax88179a_autodetach(axdev);
	if (ret < 0)
		return ret;

	reg8 = AX_TXCOE_IP | AX_TXCOE_TCP | AX_TXCOE_UDP |
	       AX_TXCOE_TCPV6 | AX_TXCOE_UDPV6;
	ret = ax_write_cmd(axdev, AX_ACCESS_MAC, AX_TXCOE_CTL, 1, 1, &reg8);
	if (ret < 0)
		return ret;

	reg8 = AX_RXCOE_IP | AX_RXCOE_TCP | AX_RXCOE_UDP |
	       AX_RXCOE_TCPV6 | AX_RXCOE_UDPV6;
	ret = ax_write_cmd(axdev, AX_ACCESS_MAC, AX_RXCOE_CTL, 1, 1, &reg8);
	if (ret < 0)
		return ret;

	reg8 = AX_MAC_EFF_EN;
	ret = ax_write_cmd(axdev, AX_ACCESS_MAC, AX88179A_MAC_BULK_OUT_CTRL,
			    1, 1, &reg8);
	if (ret < 0)
		return ret;

	reg16 = 0;
	ret = ax_write_cmd_async(axdev, AX_ACCESS_MAC, AX_RX_CTL, 2, 2, &reg16);
	if (ret < 0)
		return ret;

	reg8 = 0x04;
	ret = ax_write_cmd(axdev, AX_ACCESS_MAC, AX_PAUSE_WATERLVL_LOW,
			   1, 1, &reg8);
	if (ret < 0)
		return ret;

	reg8 = 0x10;
	ret = ax_write_cmd(axdev, AX_ACCESS_MAC, AX_PAUSE_WATERLVL_HIGH,
			   1, 1, &reg8);
	if (ret < 0)
		return ret;

	reg8 = 0;
	if (axdev->netdev->features & NETIF_F_HW_VLAN_CTAG_FILTER)
		reg8 |= AX_VLAN_CONTROL_VFE;
	if (axdev->netdev->features & NETIF_F_HW_VLAN_CTAG_RX)
		reg8 |= AX_VLAN_CONTROL_VSO;
	ret = ax_write_cmd(axdev, AX_ACCESS_MAC, AX88179A_VLAN_ID_CONTROL,
			   1, 1, &reg8);
	if (ret < 0)
		return ret;

	reg8 = 0xff;
	ret = ax_write_cmd(axdev, AX_ACCESS_MAC, AX88179A_MAC_BM_INT_MASK,
			    1, 1, &reg8);
	if (ret < 0)
		return ret;

	reg8 = 0;
	ret = ax_write_cmd(axdev, AX_ACCESS_MAC, AX88179A_MAC_BM_RX_DMA_CTL,
			    1, 1, &reg8);
	if (ret < 0)
		return ret;


	ret = ax_write_cmd(axdev, AX_ACCESS_MAC, AX88179A_MAC_BM_TX_DMA_CTL,
			    1, 1, &reg8);
	if (ret < 0)
		return ret;


	ret = ax_write_cmd(axdev, AX_ACCESS_MAC, AX88179A_MAC_ARC_CTRL,
			    1, 1, &reg8);
	if (ret < 0)
		return ret;


	ret = ax_write_cmd(axdev, AX_ACCESS_MAC, AX88179A_MAC_SWP_CTRL,
			    1, 1, &reg8);
	if (ret < 0)
		return ret;


	reg8 = AX_TXHDR_CKSUM_EN;
	ret = ax_write_cmd_nopm(axdev, AX_ACCESS_MAC, AX88179A_MAC_TX_HDR_CKSUM,
				1, 1, &reg8);
	if (ret < 0)
		return ret;

	reg16 = AX_RX_CTL_START | AX_RX_CTL_AP | AX_RX_CTL_AMALL |
		AX_RX_CTL_AB | AX_RX_CTL_DROPCRCERR;
	ret = ax_write_cmd(axdev, AX_ACCESS_MAC, AX_RX_CTL, 1, 1, &reg16);
	if (ret < 0)
		return ret;

	reg8 = 0;
	ret = ax_write_cmd(axdev, AX_ACCESS_MAC, AX88179A_MAC_PATH, 1, 1, &reg8);
	if (ret < 0)
		return ret;

	ret = ax_read_cmd(axdev, AX_ACCESS_MAC, AX_MONITOR_MODE, 1, 1, &reg8, 1);
	if (ret < 0)
		return ret;
	reg8 &= 0xE0;
	ret = ax_write_cmd(axdev, AX_ACCESS_MAC, AX_MONITOR_MODE, 1, 1, &reg8);
	if (ret < 0)
		return ret;


	ret = ax_read_cmd(axdev, AX_ACCESS_MAC, AX_MEDIUM_STATUS_MODE,
				 2, 2, &reg16, 2);
	if (ret < 0)
		return ret;

	reg16 &= ~AX_MEDIUM_GIGAMODE;
	ret = ax_write_cmd(axdev, AX_ACCESS_MAC, AX_MEDIUM_STATUS_MODE,
			    2, 2, &reg16);
	if (ret < 0)
		return ret;

	reg16 |= AX_MEDIUM_GIGAMODE;
	ret = ax_write_cmd(axdev, AX_ACCESS_MAC, AX_MEDIUM_STATUS_MODE,
			    2, 2, &reg16);
	if (ret < 0)
		return ret;

	reg8 = 0;
	ret = ax_write_cmd(axdev, AX_ACCESS_MAC, AX88179A_BFM_DATA, 1, 1, &reg8);
	if (ret < 0)
		return ret;

	ret = ax_write_cmd(axdev, AX_ACCESS_MAC, AX88179A_CDC_ECM_CTRL,
			   1, 1, &reg8);
	if (ret < 0)
		return ret;

	ax_set_tx_qlen(axdev);


	return ret;
}

static int ax88179a_get_ether_link(struct ax_device *axdev)
{
	struct ax_link_info *link_info = &axdev->link_info;
#if KERNEL_VERSION(4, 10, 0) > LINUX_VERSION_CODE
	struct ethtool_cmd cmd;

	mii_ethtool_gset(&axdev->mii, &cmd);
	switch (ethtool_cmd_speed(&cmd)) {
	case SPEED_1000:
		link_info->eth_speed = ETHER_LINK_1000;
		break;
	case SPEED_100:
		link_info->eth_speed = ETHER_LINK_100;
		break;
	case SPEED_10:
	default:
		link_info->eth_speed = ETHER_LINK_10;
		break;
	};

	link_info->full_duplex = cmd.duplex;
#else
	struct ethtool_link_ksettings cmd;

	mii_ethtool_get_link_ksettings(&axdev->mii, &cmd);
	switch (cmd.base.speed) {
	case SPEED_1000:
		link_info->eth_speed = ETHER_LINK_1000;
		break;
	case SPEED_100:
		link_info->eth_speed = ETHER_LINK_100;
		break;
	case SPEED_10:
	default:
		link_info->eth_speed = ETHER_LINK_10;
		break;
	};

	link_info->full_duplex = cmd.base.duplex;
#endif
	return 0;
}

static int ax88179a_set_bulkin_setting(struct ax_device *axdev)
{
	struct ax_link_info *link_info = &axdev->link_info;
	u8 link_sts;
	int index = 0, ret;

	ret = ax_read_cmd_nopm(axdev, AX_ACCESS_MAC, PHYSICAL_LINK_STATUS,
			       1, 1, &link_sts, 0);
	if (ret < 0)
		return ret;

	switch (link_info->eth_speed) {
	case ETHER_LINK_1000:
		if (link_sts & AX_USB_SS)
			index = 0;
		else if (link_sts & AX_USB_HS)
			index = 1;
		break;
	case ETHER_LINK_100:
		if (link_sts & AX_USB_SS)
			index = 2;
		else if (link_sts & AX_USB_HS)
			index = 4;

		if (!link_info->full_duplex)
			index++;
		break;
	case ETHER_LINK_10:
		index = 6;
		break;
	case ETHER_LINK_NONE:
	default:
		index = 0;
		break;
	};

	if (axdev->coalesce == 0) {
		u16 timer = *((u16 *)&AX88179A_BULKIN_SIZE[index].timer_l);

		axdev->coalesce = ax88179a_bin_timer_to_usec(axdev, timer);
	} else {
		u16 timer = ax88179a_usec_to_bin_timer(axdev);

		memcpy(&AX88179A_BULKIN_SIZE[index].timer_l, &timer, 2);
	}

	ret = ax_write_cmd_nopm(axdev, AX_ACCESS_MAC, AX_RX_BULKIN_QCTRL,
				5, 5, &AX88179A_BULKIN_SIZE[index]);
	if (ret < 0)
		return ret;

	return 0;
}

static int ax88179a_link_setting(struct ax_device *axdev)
{
	struct ax_link_info *link_info = &axdev->link_info;
	struct ethtool_pauseparam pause;
	u16 medium_mode, reg16;
	u8 reg8[3];
	int ret;

	reg16 = AX_RX_CTL_STOP;
	ret = ax_write_cmd_nopm(axdev, AX_ACCESS_MAC, AX_RX_CTL, 2, 2, &reg16);
	if (ret < 0)
		return ret;

	reg8[0] = 0;
	ret = ax_write_cmd_nopm(axdev, AX_ACCESS_MAC, AX88179A_MAC_PATH,
				1, 1, reg8);
	if (ret < 0)
		return ret;

	reg8[0] = 0xA5;
	ret = ax_write_cmd_nopm(axdev, AX_ACCESS_MAC, AX88179A_MAC_CDC_DELAY_TX,
				 1, 1, reg8);
	if (ret < 0)
		return ret;

	reg8[0] = 0x10;
	reg8[1] = 0x04;
	ret = ax_write_cmd_nopm(axdev, AX_ACCESS_MAC, AX_PAUSE_WATERLVL_HIGH,
				2, 2, reg8);
	if (ret < 0)
		return ret;

	memset(&pause, 0, sizeof(pause));
	ax_get_pauseparam(axdev->netdev, &pause);
	medium_mode = AX_MEDIUM_RECEIVE_EN;
	if (pause.rx_pause)
		medium_mode |= AX_MEDIUM_RXFLOW_CTRLEN;
	
	if (pause.tx_pause) {
		medium_mode |= AX_MEDIUM_TXFLOW_CTRLEN;
		reg8[0] = 0x28 | AX_NEW_PAUSE_EN;
	} else {
		reg8[0] = 0;	
	}
	ret = ax_write_cmd(axdev, AX_ACCESS_MAC, AX88179A_NEW_PAUSE_CTRL,
			   1, 1, reg8);
	if (ret < 0)
		return ret;

	switch (link_info->eth_speed) {
	case ETHER_LINK_1000:
	case ETHER_LINK_100:
		reg8[0] = 0x78;
		reg8[1] = (AX_LSOFC_WCNT_7_ACCESS << 5) | AX_GMII_CRC_APPEND;
		reg8[2] = 0;
		ret = ax_write_cmd_nopm(axdev, AX_ACCESS_MAC,
					AX88178A_MAC_RX_STATUS_CDC, 3, 3, reg8);
		if (ret < 0)
			return ret;

		reg8[0] = 0x40;
		ret = ax_write_cmd_nopm(axdev, AX_ACCESS_MAC,
					AX88179A_MAC_RX_DATA_CDC_CNT,
					1, 1, reg8);
		if (ret < 0)
			return ret;
		if (link_info->eth_speed == ETHER_LINK_1000)
			medium_mode |= AX_MEDIUM_GIGAMODE;
		break;
	case ETHER_LINK_10:
		reg8[0] = 0xFA;
		reg8[1] = (AX_LSOFC_WCNT_7_ACCESS << 5) | AX_GMII_CRC_APPEND;
		reg8[2] = 0xFF;
		ret = ax_write_cmd_nopm(axdev, AX_ACCESS_MAC,
					AX88178A_MAC_RX_STATUS_CDC, 3, 3, reg8);
		if (ret < 0)
			return ret;

		reg8[0] = 0xFA;
		ret = ax_write_cmd_nopm(axdev, AX_ACCESS_MAC,
					AX88179A_MAC_RX_DATA_CDC_CNT,
					1, 1, reg8);
		if (ret < 0)
			return ret;
		break;
	default:
		break;
	};

	reg8[0] = 0;
	ret = ax_write_cmd_nopm(axdev, AX_ACCESS_MAC, AX88179A_BFM_DATA,
				1, 1, reg8);
	if (ret < 0)
		return ret;

	ret = ax88179a_set_bulkin_setting(axdev);
	if (ret < 0)
		return ret;

	if (link_info->full_duplex)
		medium_mode |= AX_MEDIUM_FULL_DUPLEX;
	ret = ax_write_cmd_nopm(axdev, AX_ACCESS_MAC, AX_MEDIUM_STATUS_MODE,
				2, 2, &medium_mode);
	if (ret < 0)
		return ret;

	axdev->rxctl |= AX_RX_CTL_START | AX_RX_CTL_AB;
	ret = ax_write_cmd_nopm(axdev, AX_ACCESS_MAC, AX_RX_CTL,
				2, 2, &axdev->rxctl);
	if (ret < 0)
		return ret;

	reg8[0] = AX_MAC_RX_PATH_READY | AX_MAC_TX_PATH_READY;
	ret = ax_write_cmd_nopm(axdev, AX_ACCESS_MAC, AX88179A_MAC_PATH,
				1, 1, reg8);
	if (ret < 0)
		return ret;

	return 0;
}


static int ax88179a_link_reset(struct ax_device *axdev)
{
	int ret;

	ret = ax88179a_get_ether_link(axdev);
	if (ret < 0)
		return ret;

	ret = ax88179a_link_setting(axdev);
	if (ret < 0)
		return ret;

	axdev->eee_enabled = ax88179a_chk_eee(axdev);
	ax88179a_eee_setting(axdev, axdev->eee_enabled);



	return 0;
}
inline void ax88179a_rx_checksum(struct sk_buff *skb, void *pkt_hdr)
{
	struct _179a_rx_pkt_header *hdr = (struct _179a_rx_pkt_header *)pkt_hdr;

	skb->ip_summed = CHECKSUM_NONE;

	if ((hdr->L4_err) ||
	    (hdr->L3_err))
		return;

	if ((hdr->L4_pkt_type == AX_RXHDR_L4_TYPE_TCP) ||
	    (hdr->L4_pkt_type == AX_RXHDR_L4_TYPE_UDP))
		skb->ip_summed = CHECKSUM_UNNECESSARY;
}

static void ax88179a_rx_fixup(struct ax_device *axdev, struct rx_desc *desc,
			      int *work_done, int budget)
{
	struct napi_struct *napi = &axdev->napi;
	struct net_device *netdev = axdev->netdev;
	struct net_device_stats *stats = ax_get_stats(netdev);
	struct _179a_rx_pkt_header *pkt_hdr;
	struct _179a_rx_header *rx_header;
	const u32 actual_length = desc->urb->actual_length;
	u8 *rx_data;
	u32 aa = 0, rx_hdroffset = 0;
	u16 pkt_count = 0;

	rx_header = (struct _179a_rx_header *)
		(((u8 *)desc->head) + actual_length - AX88179A_RX_HEADER_SIZE);
	le64_to_cpus(rx_header);

	rx_hdroffset = rx_header->hdr_off;
	pkt_count = rx_header->pkt_cnt;
	aa = (actual_length - ((pkt_count + 1) * 8));
	if (((aa != rx_hdroffset) && ((aa - rx_hdroffset) % 16) != 0) ||
	    (rx_hdroffset >= desc->urb->actual_length) ||
	    (pkt_count == 0)) {
		desc->urb->actual_length = 0;
		stats->rx_length_errors++;
		return;
	}

	pkt_hdr = (struct _179a_rx_pkt_header *)
			(((u8 *)desc->head) + rx_header->hdr_off);

	rx_data = desc->head;
	while (pkt_count--) {
		struct sk_buff *skb;
		u32 pkt_len = 0;

		le64_to_cpus(pkt_hdr);

		if (!pkt_hdr->RxOk) {
			stats->rx_crc_errors++;
			if (!(netdev->features & NETIF_F_RXALL))
				goto find_next_rx;
		}

		if (pkt_hdr->drop) {
			stats->rx_dropped++;
			if (!(netdev->features & NETIF_F_RXALL))
				goto find_next_rx;
		}

		pkt_len = (u32)(pkt_hdr->length & 0x7FFF);

		skb = napi_alloc_skb(napi, pkt_len);
		if (!skb) {
			stats->rx_dropped++;
			goto find_next_rx;
		}

		skb_put(skb, pkt_len);
		memcpy(skb->data, rx_data, pkt_len);
		ax88179a_rx_checksum(skb, pkt_hdr);

		skb->truesize = skb->len + sizeof(struct sk_buff);

		if (pkt_hdr->vlan_ind) {
			__vlan_hwaccel_put_tag(skb, htons(ETH_P_8021Q),
					     pkt_hdr->vlan_tag & VLAN_VID_MASK);
		}

		skb->protocol = eth_type_trans(skb, netdev);
		if (*work_done < budget) {
			napi_gro_receive(napi, skb);
			*work_done += 1;
			stats->rx_packets++;
			stats->rx_bytes += pkt_len;
		} else {
			__skb_queue_tail(&axdev->rx_queue, skb);
		}
find_next_rx:
		rx_data += (pkt_len + 7) & 0x7FFF8;
		pkt_hdr++;
	}
}

static int ax88179a_tx_fixup(struct ax_device *axdev, struct tx_desc *desc)
{
	struct sk_buff_head skb_head, *tx_queue;
	struct net_device_stats *stats = &axdev->netdev->stats;
	int remain, ret;
	int endpoint = 3;
	u8 *tx_data;

	tx_queue = axdev->tx_queue;

	__skb_queue_head_init(&skb_head);
	spin_lock(&tx_queue->lock);
	skb_queue_splice_init(tx_queue, &skb_head);
	spin_unlock(&tx_queue->lock);

	tx_data = desc->head;
	desc->skb_num = 0;
	desc->skb_len = 0;
	remain = axdev->tx_casecade_size;

	while (remain >= (ETH_ZLEN + AX88179A_TX_HEADER_SIZE)) {
		struct sk_buff *skb;
		struct _179a_tx_pkt_header *tx_hdr;
		u16 tci = 0;

		skb = __skb_dequeue(&skb_head);
		if (!skb)
			break;

		if ((skb->len + AX88179A_TX_HEADER_SIZE) > remain &&
		    (skb_shinfo(skb)->gso_size == 0)) {
			__skb_queue_head(&skb_head, skb);
			break;
		}

		tx_hdr = (struct _179a_tx_pkt_header *)tx_data;
		memset(tx_hdr, 0, AX88179A_TX_HEADER_SIZE);
		tx_hdr->length = (skb->len & 0x1FFFFF);
		tx_hdr->checksum = AX88179A_TX_HERDER_CHKSUM(tx_hdr->length);
		tx_hdr->max_seg_size = skb_shinfo(skb)->gso_size;
		if ((axdev->netdev->features & NETIF_F_HW_VLAN_CTAG_TX) &&
		    (vlan_get_tag(skb, &tci) >= 0)) {
			tx_hdr->vlan_tag = 1;
			tx_hdr->vlan_info = tci;
		}

		cpu_to_le64s(tx_hdr);
		tx_data += AX88179A_TX_HEADER_SIZE;

		if (skb_copy_bits(skb, 0, tx_data, skb->len) < 0) {
			stats->tx_dropped += skb_shinfo(skb)->gso_segs ?: 1;
			dev_kfree_skb_any(skb);
			continue;
		}

		tx_data = __tx_buf_align((void *)(tx_data + skb->len),
					 axdev->tx_align_len);
		desc->skb_len += skb->len;
		desc->skb_num += skb_shinfo(skb)->gso_segs ?: 1;
		dev_kfree_skb_any(skb);

		if (tx_hdr->max_seg_size)
			break;

		remain = axdev->tx_casecade_size -
			 (int)((void *)tx_data - desc->head);
	}

	if (!skb_queue_empty(&skb_head)) {
		spin_lock(&tx_queue->lock);
		skb_queue_splice(&skb_head, tx_queue);
		spin_unlock(&tx_queue->lock);
	}

	netif_tx_lock(axdev->netdev);
	if (netif_queue_stopped(axdev->netdev) &&
	    skb_queue_len(tx_queue) < axdev->tx_qlen) {
		netif_wake_queue(axdev->netdev);
	}
	netif_tx_unlock(axdev->netdev);

	ret = usb_autopm_get_interface_async(axdev->intf);
	if (ret < 0)
		return ret;

	usb_fill_bulk_urb(desc->urb, axdev->udev,
			  usb_sndbulkpipe(axdev->udev, endpoint),
			  desc->head, (int)(tx_data - (u8 *)desc->head),
			  (usb_complete_t)ax_write_bulk_callback, desc);

	ret = usb_submit_urb(desc->urb, GFP_ATOMIC);
	if (ret < 0)
		usb_autopm_put_interface_async(axdev->intf);


	return 0;
}

static int ax88179a_system_suspend(struct ax_device *axdev)
{

	return 0;
}

static int ax88179a_system_resume(struct ax_device *axdev)
{
	int ret;

	if (!ax88179a_check_phy_power(axdev))
		ax88179a_set_phy_power(axdev, true);

	ret = ax_write_cmd_nopm(axdev, AX_FW_MODE, AX_FW_MODE_179A, 0, 0, NULL);
	if (ret < 0)
		return ret;

	axdev->driver_info->hw_init(axdev);

	return 0;
}
const struct driver_info ax88179a_info = {
	.bind		= ax88179a_bind,
	.unbind		= ax88179a_unbind,
	.hw_init	= ax88179a_hw_init,
	.stop		= ax88179a_stop,
	.link_reset	= ax88179a_link_reset,
	.rx_fixup	= ax88179a_rx_fixup,
	.tx_fixup	= ax88179a_tx_fixup,
	.system_suspend = ax88179a_system_suspend,
	.system_resume	= ax88179a_system_resume,
	.napi_weight	= AX88179A_NAPI_WEIGHT,
	.buf_rx_size	= AX88179A_BUF_RX_SIZE,
};

const struct driver_info ax88772d_info = {
	.bind		= ax88179a_bind,
	.unbind		= ax88179a_unbind,
	.hw_init	= ax88179a_hw_init,
	.stop		= ax88179a_stop,
	.link_reset	= ax88179a_link_reset,
	.rx_fixup	= ax88179a_rx_fixup,
	.tx_fixup	= ax88179a_tx_fixup,
	.system_suspend = ax88179a_system_suspend,
	.system_resume	= ax88179a_system_resume,
	.napi_weight	= AX88179A_NAPI_WEIGHT,
	.buf_rx_size	= AX88179A_BUF_RX_SIZE,
};
