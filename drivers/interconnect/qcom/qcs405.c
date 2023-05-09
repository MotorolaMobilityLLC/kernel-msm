// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 */

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/interconnect-provider.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <dt-bindings/interconnect/qcom,qcs405.h>

#include <linux/soc/qcom/smd-rpm.h>
#include <soc/qcom/rpm-smd.h>

#include "icc-rpm.h"
#include "qnoc-qos-rpm.h"
#include "rpm-ids.h"

static LIST_HEAD(qnoc_probe_list);
static DEFINE_MUTEX(probe_list_lock);

static int probe_count;

static const struct clk_bulk_data bus_clocks[] = {
	{ .id = "bus" },
	{ .id = "bus_a" },
};

static struct qcom_icc_qosbox mas_apps_proc_qos = {
	.regs = icc_bimc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0x8300 },
	.config = &(struct qos_config) {
		.prio = 0,
		.bke_enable = 1,
	 },
};

static struct qcom_icc_node mas_apps_proc = {
	.name = "mas_apps_proc",
	.id = MASTER_AMPSS_M0,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_bimc_ops,
	.qosbox = &mas_apps_proc_qos,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 2,
	.links = { SLAVE_EBI_CH0, BIMC_SNOC_SLV },
};

static struct qcom_icc_qosbox mas_oxili_qos = {
	.regs = icc_bimc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0x10300 },
	.config = &(struct qos_config) {
		.prio = 0,
		.bke_enable = 1,
	 },
};

static struct qcom_icc_node mas_oxili = {
	.name = "mas_oxili",
	.id = MASTER_GRAPHICS_3D,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_bimc_ops,
	.qosbox = &mas_oxili_qos,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 2,
	.links = { SLAVE_EBI_CH0, BIMC_SNOC_SLV },
};

static struct qcom_icc_qosbox mas_mdp_qos = {
	.regs = icc_bimc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0x18300 },
	.config = &(struct qos_config) {
		.prio = 1,
		.bke_enable = 1,
	 },
};

static struct qcom_icc_node mas_mdp = {
	.name = "mas_mdp",
	.id = MASTER_MDP_PORT0,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_bimc_ops,
	.qosbox = &mas_mdp_qos,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 2,
	.links = { SLAVE_EBI_CH0, BIMC_SNOC_SLV },
};

static struct qcom_icc_qosbox mas_snoc_bimc_1_qos = {
	.regs = icc_bimc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0x1C300 },
	.config = &(struct qos_config) {
		.prio = 0,
		.bke_enable = 0,
	 },
};

static struct qcom_icc_node mas_snoc_bimc_1 = {
	.name = "mas_snoc_bimc_1",
	.id = SNOC_BIMC_1_MAS,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_bimc_ops,
	.qosbox = &mas_snoc_bimc_1_qos,
	.mas_rpm_id = ICBID_MASTER_SNOC_BIMC_1,
	.slv_rpm_id = -1,
	.num_links = 1,
	.links = { SLAVE_EBI_CH0 },
};

static struct qcom_icc_qosbox mas_tcu_0_qos = {
	.regs = icc_bimc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0x20300 },
	.config = &(struct qos_config) {
		.prio = 2,
		.bke_enable = 1,
	 },
};

static struct qcom_icc_node mas_tcu_0 = {
	.name = "mas_tcu_0",
	.id = MASTER_TCU_0,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_bimc_ops,
	.qosbox = &mas_tcu_0_qos,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 2,
	.links = { SLAVE_EBI_CH0, BIMC_SNOC_SLV },
};

static struct qcom_icc_node mas_spdm = {
	.name = "mas_spdm",
	.id = MASTER_SPDM,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 1,
	.links = { PCNOC_INT_3 },
};

static struct qcom_icc_node mas_blsp_1 = {
	.name = "mas_blsp_1",
	.id = MASTER_BLSP_1,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = ICBID_MASTER_BLSP_1,
	.slv_rpm_id = -1,
	.num_links = 1,
	.links = { PCNOC_INT_3 },
};

static struct qcom_icc_node mas_blsp_2 = {
	.name = "mas_blsp_2",
	.id = MASTER_BLSP_2,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = ICBID_MASTER_BLSP_2,
	.slv_rpm_id = -1,
	.num_links = 1,
	.links = { PCNOC_INT_3 },
};

static struct qcom_icc_node mas_xi_usb_hs1 = {
	.name = "mas_xi_usb_hs1",
	.id = MASTER_XM_USB_HS1,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = ICBID_MASTER_XI_USB_HS1,
	.slv_rpm_id = -1,
	.num_links = 1,
	.links = { PCNOC_INT_0 },
};

static struct qcom_icc_qosbox mas_crypto_c0_qos = {
	.regs = icc_qnoc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0x7000 },
	.config = &(struct qos_config) {
		.prio = 1,
		.urg_fwd = 0,
	},
};

static struct qcom_icc_node mas_crypto_c0 = {
	.name = "mas_crypto_c0",
	.id = MASTER_CRYPTO_CORE0,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.qosbox = &mas_crypto_c0_qos,
	.mas_rpm_id = ICBID_MASTER_CRYPTO_CORE0,
	.slv_rpm_id = -1,
	.num_links = 2,
	.links = { PCNOC_SNOC_SLV, PCNOC_INT_2 },
};

static struct qcom_icc_node mas_sdcc_1 = {
	.name = "mas_sdcc_1",
	.id = MASTER_SDCC_1,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = ICBID_MASTER_SDCC_1,
	.slv_rpm_id = -1,
	.num_links = 1,
	.links = { PCNOC_INT_0 },
};

static struct qcom_icc_node mas_sdcc_2 = {
	.name = "mas_sdcc_2",
	.id = MASTER_SDCC_2,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = ICBID_MASTER_SDCC_2,
	.slv_rpm_id = -1,
	.num_links = 1,
	.links = { PCNOC_INT_0 },
};

static struct qcom_icc_node mas_snoc_pcnoc = {
	.name = "mas_snoc_pcnoc",
	.id = SNOC_PCNOC_MAS,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 1,
	.links = { PCNOC_INT_2 },
};

static struct qcom_icc_qosbox mas_qpic_qos = {
	.regs = icc_qnoc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0x15000 },
	.config = &(struct qos_config) {
		.prio = 1,
		.urg_fwd = 0,
	},
};

static struct qcom_icc_node mas_qpic = {
	.name = "mas_qpic",
	.id = MASTER_QPIC,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.qosbox = &mas_qpic_qos,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 1,
	.links = { PCNOC_INT_0 },
};

static struct qcom_icc_node mas_pcnoc_int_0 = {
	.name = "mas_pcnoc_int_0",
	.id = PCNOC_INT_0,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = ICBID_MASTER_PCNOC_INT_0,
	.slv_rpm_id = ICBID_SLAVE_PCNOC_INT_0,
	.num_links = 2,
	.links = { PCNOC_INT_2, PCNOC_SNOC_SLV },
};

static struct qcom_icc_node mas_pcnoc_int_2 = {
	.name = "mas_pcnoc_int_2",
	.id = PCNOC_INT_2,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = ICBID_MASTER_PCNOC_INT_2,
	.slv_rpm_id = ICBID_SLAVE_PCNOC_INT_2,
	.num_links = 12,
	.links = { PCNOC_SLV_9, PCNOC_SLV_6,
		   PCNOC_SLV_8, PCNOC_SLV_2,
		   PCNOC_SLV_3, PCNOC_SLV_0,
		   PCNOC_SLV_1, PCNOC_SLV_4,
		   SLAVE_TCU, PCNOC_SLV_7,
		   MASTER_PCNOC_S_10, MASTER_PCNOC_S_11 },
};

static struct qcom_icc_node mas_pcnoc_int_3 = {
	.name = "mas_pcnoc_int_3",
	.id = PCNOC_INT_3,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = ICBID_MASTER_PCNOC_INT_3,
	.slv_rpm_id = ICBID_SLAVE_PCNOC_INT_3,
	.num_links = 1,
	.links = { PCNOC_SNOC_SLV },
};

static struct qcom_icc_node mas_pcnoc_s_0 = {
	.name = "mas_pcnoc_s_0",
	.id = PCNOC_SLV_0,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = ICBID_MASTER_PCNOC_S_0,
	.slv_rpm_id = ICBID_SLAVE_PCNOC_S_0,
	.num_links = 3,
	.links = { SLAVE_SPDM_WRAPPER, SLAVE_PDM,
		   SLAVE_PRNG },
};

static struct qcom_icc_node mas_pcnoc_s_1 = {
	.name = "mas_pcnoc_s_1",
	.id = PCNOC_SLV_1,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = ICBID_MASTER_PCNOC_S_1,
	.slv_rpm_id = ICBID_SLAVE_PCNOC_S_1,
	.num_links = 1,
	.links = { SLAVE_TCSR },
};

static struct qcom_icc_node mas_pcnoc_s_2 = {
	.name = "mas_pcnoc_s_2",
	.id = PCNOC_SLV_2,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 1,
	.links = { SLAVE_GRAPHICS_3D_CFG },
};

static struct qcom_icc_node mas_pcnoc_s_3 = {
	.name = "mas_pcnoc_s_3",
	.id = PCNOC_SLV_3,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = ICBID_MASTER_PCNOC_S_3,
	.slv_rpm_id = ICBID_SLAVE_PCNOC_S_3,
	.num_links = 1,
	.links = { SLAVE_MESSAGE_RAM },
};

static struct qcom_icc_node mas_pcnoc_s_4 = {
	.name = "mas_pcnoc_s_4",
	.id = PCNOC_SLV_4,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = ICBID_MASTER_PCNOC_S_4,
	.slv_rpm_id = ICBID_SLAVE_PCNOC_S_4,
	.num_links = 1,
	.links = { SLAVE_SNOC_CFG },
};

static struct qcom_icc_node mas_pcnoc_s_6 = {
	.name = "mas_pcnoc_s_6",
	.id = PCNOC_SLV_6,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = ICBID_MASTER_PCNOC_S_6,
	.slv_rpm_id = ICBID_SLAVE_PCNOC_S_6,
	.num_links = 3,
	.links = { SLAVE_BLSP_1, SLAVE_TLMM_NORTH,
		   SLAVE_EMAC },
};

static struct qcom_icc_node mas_pcnoc_s_7 = {
	.name = "mas_pcnoc_s_7",
	.id = PCNOC_SLV_7,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = ICBID_MASTER_PCNOC_S_7,
	.slv_rpm_id = ICBID_SLAVE_PCNOC_S_7,
	.num_links = 5,
	.links = { SLAVE_DISPLAY_CFG, SLAVE_PCIE_1,
		   SLAVE_SDCC_1, SLAVE_SDCC_2,
		   SLAVE_TLMM_SOUTH },
};

static struct qcom_icc_node mas_pcnoc_s_8 = {
	.name = "mas_pcnoc_s_8",
	.id = PCNOC_SLV_8,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = ICBID_MASTER_PCNOC_S_8,
	.slv_rpm_id = ICBID_SLAVE_PCNOC_S_8,
	.num_links = 1,
	.links = { SLAVE_CRYPTO_0_CFG },
};

static struct qcom_icc_node mas_pcnoc_s_9 = {
	.name = "mas_pcnoc_s_9",
	.id = PCNOC_SLV_9,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = ICBID_MASTER_PCNOC_S_9,
	.slv_rpm_id = ICBID_SLAVE_PCNOC_S_9,
	.num_links = 3,
	.links = { SLAVE_PMIC_ARB, SLAVE_BLSP_2,
		   SLAVE_TLMM_EAST },
};

static struct qcom_icc_node mas_pcnoc_s_10 = {
	.name = "mas_pcnoc_s_10",
	.id = MASTER_PCNOC_S_10,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = ICBID_MASTER_PCNOC_S_10,
	.slv_rpm_id = -1,
	.num_links = 1,
	.links = { SLAVE_USB_HS },
};

static struct qcom_icc_node mas_pcnoc_s_11 = {
	.name = "mas_pcnoc_s_11",
	.id = MASTER_PCNOC_S_11,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = ICBID_MASTER_PCNOC_S_11,
	.slv_rpm_id = ICBID_SLAVE_PCNOC_S_11,
	.num_links = 1,
	.links = { SLAVE_USB3 },
};

static struct qcom_icc_qosbox mas_qdss_bam_qos = {
	.regs = icc_qnoc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0x12000 },
	.config = &(struct qos_config) {
		.prio = 1,
		.urg_fwd = 0,
	},
};

static struct qcom_icc_node mas_qdss_bam = {
	.name = "mas_qdss_bam",
	.id = MASTER_QDSS_BAM,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.qosbox = &mas_qdss_bam_qos,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 1,
	.links = { SNOC_QDSS_INT },
};

static struct qcom_icc_node mas_bimc_snoc = {
	.name = "mas_bimc_snoc",
	.id = BIMC_SNOC_MAS,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = ICBID_MASTER_BIMC_SNOC,
	.slv_rpm_id = -1,
	.num_links = 4,
	.links = { SLAVE_CATS_128, SLAVE_OCMEM_64,
		   SNOC_INT_0, SNOC_INT_1 },
};

static struct qcom_icc_node mas_pcnoc_snoc = {
	.name = "mas_pcnoc_snoc",
	.id = PNOC_SNOC_MAS,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = ICBID_MASTER_PNOC_SNOC,
	.slv_rpm_id = -1,
	.num_links = 3,
	.links = { SNOC_INT_0, SNOC_INT_2,
		   SNOC_BIMC_1_SLV },
};

static struct qcom_icc_qosbox mas_qdss_etr_qos = {
	.regs = icc_qnoc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0x11000 },
	.config = &(struct qos_config) {
		.prio = 1,
		.urg_fwd = 0,
	},
};

static struct qcom_icc_node mas_qdss_etr = {
	.name = "mas_qdss_etr",
	.id = MASTER_QDSS_ETR,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.qosbox = &mas_qdss_etr_qos,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 1,
	.links = { SNOC_QDSS_INT },
};

static struct qcom_icc_qosbox mas_emac_qos = {
	.regs = icc_qnoc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0x22000 },
	.config = &(struct qos_config) {
		.prio = 1,
		.urg_fwd = 0,
	},
};

static struct qcom_icc_node mas_emac = {
	.name = "mas_emac",
	.id = MASTER_EMAC,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.qosbox = &mas_emac_qos,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 2,
	.links = { SNOC_INT_1, SNOC_BIMC_1_SLV },
};

static struct qcom_icc_qosbox mas_pcie_qos = {
	.regs = icc_qnoc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0x19000 },
	.config = &(struct qos_config) {
		.prio = 1,
		.urg_fwd = 0,
	},
};

static struct qcom_icc_node mas_pcie = {
	.name = "mas_pcie",
	.id = MASTER_PCIE,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.qosbox = &mas_pcie_qos,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 2,
	.links = { SNOC_INT_1, SNOC_BIMC_1_SLV },
};

static struct qcom_icc_qosbox mas_usb3_qos = {
	.regs = icc_qnoc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0x21000 },
	.config = &(struct qos_config) {
		.prio = 1,
		.urg_fwd = 0,
	},
};

static struct qcom_icc_node mas_usb3 = {
	.name = "mas_usb3",
	.id = MASTER_USB3,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.qosbox = &mas_usb3_qos,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 2,
	.links = { SNOC_INT_1, SNOC_BIMC_1_SLV },
};

static struct qcom_icc_node mas_qdss_int = {
	.name = "mas_qdss_int",
	.id = SNOC_QDSS_INT,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 2,
	.links = { SNOC_INT_1, SNOC_BIMC_1_SLV },
};

static struct qcom_icc_node mas_snoc_int_0 = {
	.name = "mas_snoc_int_0",
	.id = SNOC_INT_0,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = ICBID_MASTER_SNOC_INT_0,
	.slv_rpm_id = ICBID_SLAVE_SNOC_INT_0,
	.num_links = 3,
	.links = { SLAVE_APPSS, SLAVE_WCSS,
		   SLAVE_LPASS },
};

static struct qcom_icc_node mas_snoc_int_1 = {
	.name = "mas_snoc_int_1",
	.id = SNOC_INT_1,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = ICBID_MASTER_SNOC_INT_1,
	.slv_rpm_id = ICBID_SLAVE_SNOC_INT_1,
	.num_links = 2,
	.links = { SNOC_INT_2, SNOC_PCNOC_SLV },
};

static struct qcom_icc_node mas_snoc_int_2 = {
	.name = "mas_snoc_int_2",
	.id = SNOC_INT_2,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = ICBID_MASTER_SNOC_INT_2,
	.slv_rpm_id = ICBID_SLAVE_SNOC_INT_2,
	.num_links = 2,
	.links = { SLAVE_OCIMEM, SLAVE_QDSS_STM },
};

static struct qcom_icc_node slv_ebi = {
	.name = "slv_ebi",
	.id = SLAVE_EBI_CH0,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = -1,
	.slv_rpm_id = ICBID_SLAVE_EBI1,
	.num_links = 0,
};

static struct qcom_icc_node slv_bimc_snoc = {
	.name = "slv_bimc_snoc",
	.id = BIMC_SNOC_SLV,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = -1,
	.slv_rpm_id = ICBID_SLAVE_BIMC_SNOC,
	.num_links = 1,
	.links = { BIMC_SNOC_MAS },
};

static struct qcom_icc_node slv_spdm = {
	.name = "slv_spdm",
	.id = SLAVE_SPDM_WRAPPER,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 0,
};

static struct qcom_icc_node slv_pdm = {
	.name = "slv_pdm",
	.id = SLAVE_PDM,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 0,
};

static struct qcom_icc_node slv_prng = {
	.name = "slv_prng",
	.id = SLAVE_PRNG,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 0,
};

static struct qcom_icc_node slv_tcsr = {
	.name = "slv_tcsr",
	.id = SLAVE_TCSR,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 0,
};

static struct qcom_icc_node slv_snoc_cfg = {
	.name = "slv_snoc_cfg",
	.id = SLAVE_SNOC_CFG,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 0,
};

static struct qcom_icc_node slv_message_ram = {
	.name = "slv_message_ram",
	.id = SLAVE_MESSAGE_RAM,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 0,
};

static struct qcom_icc_node slv_disp_ss_cfg = {
	.name = "slv_disp_ss_cfg",
	.id = SLAVE_DISPLAY_CFG,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 0,
};

static struct qcom_icc_node slv_gpu_cfg = {
	.name = "slv_gpu_cfg",
	.id = SLAVE_GRAPHICS_3D_CFG,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 0,
};

static struct qcom_icc_node slv_blsp_1 = {
	.name = "slv_blsp_1",
	.id = SLAVE_BLSP_1,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 0,
};

static struct qcom_icc_node slv_tlmm_north = {
	.name = "slv_tlmm_north",
	.id = SLAVE_TLMM_NORTH,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 0,
};

static struct qcom_icc_node slv_pcie = {
	.name = "slv_pcie",
	.id = SLAVE_PCIE_1,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 0,
};

static struct qcom_icc_node slv_ethernet = {
	.name = "slv_ethernet",
	.id = SLAVE_EMAC,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 0,
};

static struct qcom_icc_node slv_blsp_2 = {
	.name = "slv_blsp_2",
	.id = SLAVE_BLSP_2,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 0,
};

static struct qcom_icc_node slv_tlmm_east = {
	.name = "slv_tlmm_east",
	.id = SLAVE_TLMM_EAST,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 0,
};

static struct qcom_icc_node slv_tcu = {
	.name = "slv_tcu",
	.id = SLAVE_TCU,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 0,
};

static struct qcom_icc_node slv_pmic_arb = {
	.name = "slv_pmic_arb",
	.id = SLAVE_PMIC_ARB,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 0,
};

static struct qcom_icc_node slv_sdcc_1 = {
	.name = "slv_sdcc_1",
	.id = SLAVE_SDCC_1,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 0,
};

static struct qcom_icc_node slv_sdcc_2 = {
	.name = "slv_sdcc_2",
	.id = SLAVE_SDCC_2,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 0,
};

static struct qcom_icc_node slv_tlmm_south = {
	.name = "slv_tlmm_south",
	.id = SLAVE_TLMM_SOUTH,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 0,
};

static struct qcom_icc_node slv_usb_hs = {
	.name = "slv_usb_hs",
	.id = SLAVE_USB_HS,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 0,
};

static struct qcom_icc_node slv_usb3 = {
	.name = "slv_usb3",
	.id = SLAVE_USB3,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 0,
};

static struct qcom_icc_node slv_crypto_0_cfg = {
	.name = "slv_crypto_0_cfg",
	.id = SLAVE_CRYPTO_0_CFG,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 0,
};

static struct qcom_icc_node slv_pcnoc_snoc = {
	.name = "slv_pcnoc_snoc",
	.id = PCNOC_SNOC_SLV,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = -1,
	.slv_rpm_id = ICBID_SLAVE_PCNOC_SNOC,
	.num_links = 1,
	.links = { PNOC_SNOC_MAS },
};

static struct qcom_icc_node slv_kpss_ahb = {
	.name = "slv_kpss_ahb",
	.id = SLAVE_APPSS,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 0,
};

static struct qcom_icc_node slv_wcss = {
	.name = "slv_wcss",
	.id = SLAVE_WCSS,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 0,
};

static struct qcom_icc_node slv_snoc_bimc_1 = {
	.name = "slv_snoc_bimc_1",
	.id = SNOC_BIMC_1_SLV,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = -1,
	.slv_rpm_id = ICBID_SLAVE_SNOC_BIMC_1,
	.num_links = 1,
	.links = { SNOC_BIMC_1_MAS },
};

static struct qcom_icc_node slv_imem = {
	.name = "slv_imem",
	.id = SLAVE_OCIMEM,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = -1,
	.slv_rpm_id = ICBID_SLAVE_IMEM,
	.num_links = 0,
};

static struct qcom_icc_node slv_snoc_pcnoc = {
	.name = "slv_snoc_pcnoc",
	.id = SNOC_PCNOC_SLV,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = -1,
	.slv_rpm_id = ICBID_SLAVE_SNOC_PCNOC,
	.num_links = 1,
	.links = { SNOC_PCNOC_MAS },
};

static struct qcom_icc_node slv_qdss_stm = {
	.name = "slv_qdss_stm",
	.id = SLAVE_QDSS_STM,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 0,
};

static struct qcom_icc_node slv_cats_0 = {
	.name = "slv_cats_0",
	.id = SLAVE_CATS_128,
	.channels = 1,
	.buswidth = 16,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 0,
};

static struct qcom_icc_node slv_cats_1 = {
	.name = "slv_cats_1",
	.id = SLAVE_OCMEM_64,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 0,
};

static struct qcom_icc_node slv_lpass = {
	.name = "slv_lpass",
	.id = SLAVE_LPASS,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 0,
};

static struct qcom_icc_node *bimc_nodes[] = {
	[MASTER_AMPSS_M0] = &mas_apps_proc,
	[MASTER_GRAPHICS_3D] = &mas_oxili,
	[MASTER_MDP_PORT0] = &mas_mdp,
	[SNOC_BIMC_1_MAS] = &mas_snoc_bimc_1,
	[MASTER_TCU_0] = &mas_tcu_0,
	[SLAVE_EBI_CH0] = &slv_ebi,
	[BIMC_SNOC_SLV] = &slv_bimc_snoc,
};

static struct qcom_icc_desc qcs405_bimc = {
	.nodes = bimc_nodes,
	.num_nodes = ARRAY_SIZE(bimc_nodes),
};

static struct qcom_icc_node *pcnoc_nodes[] = {
	[MASTER_SPDM] = &mas_spdm,
	[MASTER_BLSP_1] = &mas_blsp_1,
	[MASTER_BLSP_2] = &mas_blsp_2,
	[MASTER_XM_USB_HS1] = &mas_xi_usb_hs1,
	[MASTER_CRYPTO_CORE0] = &mas_crypto_c0,
	[MASTER_SDCC_1] = &mas_sdcc_1,
	[MASTER_SDCC_2] = &mas_sdcc_2,
	[SNOC_PCNOC_MAS] = &mas_snoc_pcnoc,
	[MASTER_QPIC] = &mas_qpic,
	[PCNOC_INT_0] = &mas_pcnoc_int_0,
	[PCNOC_INT_2] = &mas_pcnoc_int_2,
	[PCNOC_INT_3] = &mas_pcnoc_int_3,
	[PCNOC_SLV_0] = &mas_pcnoc_s_0,
	[PCNOC_SLV_1] = &mas_pcnoc_s_1,
	[PCNOC_SLV_2] = &mas_pcnoc_s_2,
	[PCNOC_SLV_3] = &mas_pcnoc_s_3,
	[PCNOC_SLV_4] = &mas_pcnoc_s_4,
	[PCNOC_SLV_6] = &mas_pcnoc_s_6,
	[PCNOC_SLV_7] = &mas_pcnoc_s_7,
	[PCNOC_SLV_8] = &mas_pcnoc_s_8,
	[PCNOC_SLV_9] = &mas_pcnoc_s_9,
	[MASTER_PCNOC_S_10] = &mas_pcnoc_s_10,
	[MASTER_PCNOC_S_11] = &mas_pcnoc_s_11,
	[SLAVE_SPDM_WRAPPER] = &slv_spdm,
	[SLAVE_PDM] = &slv_pdm,
	[SLAVE_PRNG] = &slv_prng,
	[SLAVE_TCSR] = &slv_tcsr,
	[SLAVE_SNOC_CFG] = &slv_snoc_cfg,
	[SLAVE_MESSAGE_RAM] = &slv_message_ram,
	[SLAVE_DISPLAY_CFG] = &slv_disp_ss_cfg,
	[SLAVE_GRAPHICS_3D_CFG] = &slv_gpu_cfg,
	[SLAVE_BLSP_1] = &slv_blsp_1,
	[SLAVE_TLMM_NORTH] = &slv_tlmm_north,
	[SLAVE_PCIE_1] = &slv_pcie,
	[SLAVE_EMAC] = &slv_ethernet,
	[SLAVE_BLSP_2] = &slv_blsp_2,
	[SLAVE_TLMM_EAST] = &slv_tlmm_east,
	[SLAVE_TCU] = &slv_tcu,
	[SLAVE_PMIC_ARB] = &slv_pmic_arb,
	[SLAVE_SDCC_1] = &slv_sdcc_1,
	[SLAVE_SDCC_2] = &slv_sdcc_2,
	[SLAVE_TLMM_SOUTH] = &slv_tlmm_south,
	[SLAVE_USB_HS] = &slv_usb_hs,
	[SLAVE_USB3] = &slv_usb3,
	[SLAVE_CRYPTO_0_CFG] = &slv_crypto_0_cfg,
	[PCNOC_SNOC_SLV] = &slv_pcnoc_snoc,
};

static struct qcom_icc_desc qcs405_pcnoc = {
	.nodes = pcnoc_nodes,
	.num_nodes = ARRAY_SIZE(pcnoc_nodes),
};

static struct qcom_icc_node *snoc_nodes[] = {
	[MASTER_QDSS_BAM] = &mas_qdss_bam,
	[BIMC_SNOC_MAS] = &mas_bimc_snoc,
	[PNOC_SNOC_MAS] = &mas_pcnoc_snoc,
	[MASTER_QDSS_ETR] = &mas_qdss_etr,
	[MASTER_EMAC] = &mas_emac,
	[MASTER_PCIE] = &mas_pcie,
	[MASTER_USB3] = &mas_usb3,
	[SNOC_QDSS_INT] = &mas_qdss_int,
	[SNOC_INT_0] = &mas_snoc_int_0,
	[SNOC_INT_1] = &mas_snoc_int_1,
	[SNOC_INT_2] = &mas_snoc_int_2,
	[SLAVE_APPSS] = &slv_kpss_ahb,
	[SLAVE_WCSS] = &slv_wcss,
	[SNOC_BIMC_1_SLV] = &slv_snoc_bimc_1,
	[SLAVE_OCIMEM] = &slv_imem,
	[SNOC_PCNOC_SLV] = &slv_snoc_pcnoc,
	[SLAVE_QDSS_STM] = &slv_qdss_stm,
	[SLAVE_CATS_128] = &slv_cats_0,
	[SLAVE_OCMEM_64] = &slv_cats_1,
	[SLAVE_LPASS] = &slv_lpass,
};

static struct qcom_icc_desc qcs405_snoc = {
	.nodes = snoc_nodes,
	.num_nodes = ARRAY_SIZE(snoc_nodes),
};

static const struct regmap_config icc_regmap_config = {
	.reg_bits       = 32,
	.reg_stride     = 4,
	.val_bits       = 32,
};

static struct regmap *
qcom_icc_map(struct platform_device *pdev, const struct qcom_icc_desc *desc)
{
	void __iomem *base;
	struct resource *res;
	struct device *dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return NULL;

	base = devm_ioremap_resource(dev, res);
	if (IS_ERR(base))
		return ERR_CAST(base);

	return devm_regmap_init_mmio(dev, base, &icc_regmap_config);
}

static int qnoc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct qcom_icc_desc *desc;
	struct icc_onecell_data *data;
	struct icc_provider *provider;
	struct qcom_icc_node **qnodes;
	struct qcom_icc_provider *qp;
	struct icc_node *node, *tmp;
	size_t num_nodes, i;
	int ret;

	desc = of_device_get_match_data(dev);
	if (!desc)
		return -EINVAL;

	qnodes = desc->nodes;
	num_nodes = desc->num_nodes;

	qp = devm_kzalloc(dev, sizeof(*qp), GFP_KERNEL);
	if (!qp)
		return -ENOMEM;

	data = devm_kzalloc(dev, struct_size(data, nodes, num_nodes),
			    GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	qp->bus_clks = devm_kmemdup(dev, bus_clocks, sizeof(bus_clocks),
				    GFP_KERNEL);
	if (!qp->bus_clks)
		return -ENOMEM;

	qp->num_clks = ARRAY_SIZE(bus_clocks);
	ret = devm_clk_bulk_get(dev, qp->num_clks, qp->bus_clks);
	if (ret)
		return ret;

	ret = clk_bulk_prepare_enable(qp->num_clks, qp->bus_clks);
	if (ret)
		return ret;

	provider = &qp->provider;
	provider->dev = dev;
	provider->set = qcom_icc_rpm_set;
	provider->pre_aggregate = qcom_icc_rpm_pre_aggregate;
	provider->aggregate = qcom_icc_rpm_aggregate;
	provider->get_bw = qcom_icc_get_bw_stub;
	provider->xlate = of_icc_xlate_onecell;
	INIT_LIST_HEAD(&provider->nodes);
	provider->data = data;
	qp->dev = &pdev->dev;

	qp->init = true;
	qp->keepalive = of_property_read_bool(dev->of_node, "qcom,keepalive");

	if (of_property_read_u32(dev->of_node, "qcom,util-factor",
				 &qp->util_factor))
		qp->util_factor = DEFAULT_UTIL_FACTOR;

	qp->regmap = qcom_icc_map(pdev, desc);
	if (IS_ERR(qp->regmap))
		return PTR_ERR(qp->regmap);

	ret = icc_provider_add(provider);
	if (ret) {
		dev_err(dev, "error adding interconnect provider: %d\n", ret);
		clk_bulk_disable_unprepare(qp->num_clks, qp->bus_clks);
		return ret;
	}

	qp->num_qos_clks = devm_clk_bulk_get_all(dev, &qp->qos_clks);
	if (qp->num_qos_clks < 0)
		return qp->num_qos_clks;

	ret = clk_bulk_prepare_enable(qp->num_qos_clks, qp->qos_clks);
	if (ret) {
		clk_bulk_disable_unprepare(qp->num_clks, qp->bus_clks);
		dev_err(&pdev->dev, "failed to enable QoS clocks\n");
		return ret;
	}

	for (i = 0; i < num_nodes; i++) {
		size_t j;

		if (!qnodes[i])
			continue;

		qnodes[i]->regmap = dev_get_regmap(qp->dev, NULL);

		node = icc_node_create(qnodes[i]->id);
		if (IS_ERR(node)) {
			ret = PTR_ERR(node);
			goto err;
		}

		if (qnodes[i]->qosbox) {
			qnodes[i]->noc_ops->set_qos(qnodes[i]);
			qnodes[i]->qosbox->initialized = true;
		}

		node->name = qnodes[i]->name;
		node->data = qnodes[i];
		icc_node_add(node, provider);

		for (j = 0; j < qnodes[i]->num_links; j++)
			icc_link_create(node, qnodes[i]->links[j]);

		data->nodes[i] = node;
	}
	data->num_nodes = num_nodes;

	clk_bulk_disable_unprepare(qp->num_qos_clks, qp->qos_clks);

	platform_set_drvdata(pdev, qp);

	dev_info(dev, "Registered QCS405 ICC\n");

	mutex_lock(&probe_list_lock);
	list_add_tail(&qp->probe_list, &qnoc_probe_list);
	mutex_unlock(&probe_list_lock);

	return 0;
err:
	list_for_each_entry_safe(node, tmp, &provider->nodes, node_list) {
		icc_node_del(node);
		icc_node_destroy(node->id);
	}

	clk_bulk_disable_unprepare(qp->num_qos_clks, qp->qos_clks);
	clk_bulk_disable_unprepare(qp->num_clks, qp->bus_clks);
	icc_provider_del(provider);
	return ret;
}

static int qnoc_remove(struct platform_device *pdev)
{
	struct qcom_icc_provider *qp = platform_get_drvdata(pdev);
	struct icc_provider *provider = &qp->provider;
	struct icc_node *n, *tmp;

	list_for_each_entry_safe(n, tmp, &provider->nodes, node_list) {
		icc_node_del(n);
		icc_node_destroy(n->id);
	}

	clk_bulk_disable_unprepare(qp->num_clks, qp->bus_clks);

	return icc_provider_del(provider);
}

static const struct of_device_id qnoc_of_match[] = {
	{ .compatible = "qcom,qcs405-bimc",
	  .data = &qcs405_bimc},
	{ .compatible = "qcom,qcs405-pcnoc",
	  .data = &qcs405_pcnoc},
	{ .compatible = "qcom,qcs405-snoc",
	  .data = &qcs405_snoc},
	{ }
};
MODULE_DEVICE_TABLE(of, qnoc_of_match);

static void qnoc_sync_state(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct qcom_icc_provider *qp = platform_get_drvdata(pdev);
	int ret = 0, i;

	mutex_lock(&probe_list_lock);
	probe_count++;

	if (probe_count < ARRAY_SIZE(qnoc_of_match) - 1) {
		mutex_unlock(&probe_list_lock);
		return;
	}

	list_for_each_entry(qp, &qnoc_probe_list, probe_list) {
		qp->init = false;

		if (!qp->keepalive)
			continue;

		for (i = 0; i < RPM_NUM_CXT; i++) {
			if (i == RPM_ACTIVE_CXT) {
				if (qp->bus_clk_cur_rate[i] == 0)
					ret = clk_set_rate(qp->bus_clks[i].clk,
						RPM_CLK_MIN_LEVEL);
				else
					ret = clk_set_rate(qp->bus_clks[i].clk,
						qp->bus_clk_cur_rate[i]);
			} else {
				ret = clk_set_rate(qp->bus_clks[i].clk,
						qp->bus_clk_cur_rate[i]);
			}

			if (ret)
				pr_err("%s clk_set_rate error: %d\n",
						qp->bus_clks[i].id, ret);
		}
	}

	mutex_unlock(&probe_list_lock);

	pr_err("QCS405 ICC Sync State done\n");
}

static struct platform_driver qnoc_driver = {
	.probe = qnoc_probe,
	.remove = qnoc_remove,
	.driver = {
		.name = "qnoc-qcs405",
		.of_match_table = qnoc_of_match,
		.sync_state = qnoc_sync_state,
	},
};

static int __init qnoc_driver_init(void)
{
	return platform_driver_register(&qnoc_driver);
}
core_initcall(qnoc_driver_init);

MODULE_DESCRIPTION("QCS405 NoC driver");
MODULE_LICENSE("GPL v2");
