/* SPDX-License-Identifier: GPL-2.0-only */
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.

#ifndef	_DWMAC_QCOM_ETHQOS_H
#define	_DWMAC_QCOM_ETHQOS_H

#include <linux/ipc_logging.h>

extern void *ipc_emac_log_ctxt;

#define IPCLOG_STATE_PAGES 50
#define __FILENAME__ (strrchr(__FILE__, '/') ? \
				strrchr(__FILE__, '/') + 1 : __FILE__)
#include <linux/inetdevice.h>
#include <linux/inet.h>

#include <net/addrconf.h>
#include <net/ipv6.h>
#include <net/inet_common.h>

#include <linux/uaccess.h>

#define QCOM_ETH_QOS_MAC_ADDR_LEN 6
#define QCOM_ETH_QOS_MAC_ADDR_STR_LEN 18

#define DRV_NAME "qcom-ethqos"
#define ETHQOSDBG(fmt, args...) \
	pr_debug(DRV_NAME " %s:%d " fmt, __func__, __LINE__, ## args)
#define ETHQOSERR(fmt, args...) \
do {\
	pr_err(DRV_NAME " %s:%d " fmt, __func__, __LINE__, ## args);\
	if (ipc_emac_log_ctxt) { \
		ipc_log_string(ipc_emac_log_ctxt, \
		"%s: %s[%u]:[emac] ERROR:" fmt, __FILENAME__,\
		__func__, __LINE__, ## args); \
	} \
} while (0)
#define ETHQOSINFO(fmt, args...) \
	pr_info(DRV_NAME " %s:%d " fmt, __func__, __LINE__, ## args)

#define PM_WAKEUP_MS			5000

#define RGMII_IO_MACRO_CONFIG		0x0
#define SDCC_HC_REG_DLL_CONFIG		0x4
#define SDCC_TEST_CTL			0x8
#define SDCC_HC_REG_DDR_CONFIG		0xC
#define SDCC_HC_REG_DLL_CONFIG2		0x10
#define SDC4_STATUS			0x14
#define SDCC_USR_CTL			0x18
#define RGMII_IO_MACRO_CONFIG2		0x1C

#define EMAC_WRAPPER_SGMII_PHY_CNTRL0_v3	0xF0
#define EMAC_WRAPPER_SGMII_PHY_CNTRL1_v3	0xF4
#define EMAC_WRAPPER_SGMII_PHY_CNTRL0	0x170
#define EMAC_WRAPPER_SGMII_PHY_CNTRL1	0x174
#define EMAC_WRAPPER_USXGMII_MUX_SEL	0x1D0
#define RGMII_IO_MACRO_SCRATCH_2	0x44
#define RGMII_IO_MACRO_BYPASS		0x16C

#define EMAC_HW_NONE 0
#define EMAC_HW_v2_1_1 0x20010001
#define EMAC_HW_v2_1_2 0x20010002
#define EMAC_HW_v2_3_0 0x20030000
#define EMAC_HW_v2_3_1 0x20030001
#define EMAC_HW_v3_0_0_RG 0x30000000
#define EMAC_HW_v3_1_0 0x30010000
#define EMAC_HW_v4_0_0 0x40000000
#define EMAC_HW_vMAX 9

#define EMAC_GDSC_EMAC_NAME "gdsc_emac"

struct ethqos_emac_por {
	unsigned int offset;
	unsigned int value;
};

struct ethqos_emac_driver_data {
	const struct ethqos_emac_por *por;
	unsigned int num_por;
	struct dwxgmac_addrs dwxgmac_addrs;
	u32 dma_addr_width;
	bool has_hdma;
};

struct qcom_ethqos {
	struct platform_device *pdev;
	void __iomem *rgmii_base;
	void __iomem *sgmii_base;
	void __iomem *ioaddr;
	unsigned int rgmii_clk_rate;
	struct clk *rgmii_clk;
	struct clk *phyaux_clk;
	struct clk *sgmiref_clk;

	unsigned int speed;
	int interface;

	int gpio_phy_intr_redirect;
	u32 phy_intr;
	/* Work struct for handling phy interrupt */
	struct work_struct emac_phy_work;

	const struct ethqos_emac_por *por;
	unsigned int num_por;
	unsigned int emac_ver;

	struct regulator *gdsc_emac;
	struct regulator *reg_rgmii;
	struct regulator *reg_emac_phy;
	struct regulator *reg_rgmii_io_pads;

	bool use_domains;
	struct dev_pm_domain_list *pd_list;

	/* state of enabled wol options in PHY*/
	u32 phy_wol_wolopts;
	/* state of supported wol options in PHY*/
	u32 phy_wol_supported;

	int curr_serdes_speed;

	/* Boolean to check if clock is suspended*/
	int clks_suspended;
	struct completion clk_enable_done;
	/* Boolean flag for turning off GDSC during suspend */
	bool gdsc_off_on_suspend;

	/* early ethernet parameters */
	struct work_struct early_eth;
	struct delayed_work ipv4_addr_assign_wq;
	struct delayed_work ipv6_addr_assign_wq;
	bool early_eth_enabled;
	bool driver_load_fail;
	/* Key Performance Indicators */
	bool print_kpi;

	struct dentry *debugfs_dir;
};

struct ip_params {
	unsigned char mac_addr[QCOM_ETH_QOS_MAC_ADDR_LEN];
	bool is_valid_mac_addr;
	char link_speed[32];
	bool is_valid_link_speed;
	char ipv4_addr_str[32];
	struct in_addr ipv4_addr;
	bool is_valid_ipv4_addr;
	char ipv6_addr_str[48];
	struct in6_ifreq ipv6_addr;
	bool is_valid_ipv6_addr;
};

int ethqos_init_regulators(struct qcom_ethqos *ethqos);
void ethqos_disable_regulators(struct qcom_ethqos *ethqos);
int ethqos_init_gpio(struct qcom_ethqos *ethqos);
void ethqos_free_gpios(struct qcom_ethqos *ethqos);
void *qcom_ethqos_get_priv(struct qcom_ethqos *ethqos);

#define QTAG_VLAN_ETH_TYPE_OFFSET 16
#define QTAG_UCP_FIELD_OFFSET 14
#define QTAG_ETH_TYPE_OFFSET 12
#define PTP_UDP_EV_PORT 0x013F
#define PTP_UDP_GEN_PORT 0x0140

#define IPA_DMA_TX_CH 0
#define IPA_DMA_RX_CH 0

#define VLAN_TAG_UCP_SHIFT 13
#define CLASS_A_TRAFFIC_UCP 3
#define CLASS_A_TRAFFIC_TX_CHANNEL 3

#define CLASS_B_TRAFFIC_UCP 2
#define CLASS_B_TRAFFIC_TX_CHANNEL 2

#define NON_TAGGED_IP_TRAFFIC_TX_CHANNEL 1
#define ALL_OTHER_TRAFFIC_TX_CHANNEL 1
#define ALL_OTHER_TX_TRAFFIC_IPA_DISABLED 0

#define DEFAULT_INT_MOD 1
#define AVB_INT_MOD 8
#define IP_PKT_INT_MOD 32
#define PTP_INT_MOD 1

#endif
