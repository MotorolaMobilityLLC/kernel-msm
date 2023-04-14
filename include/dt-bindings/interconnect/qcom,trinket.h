/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __DT_BINDINGS_INTERCONNECT_QCOM_TRINKET_H
#define __DT_BINDINGS_INTERCONNECT_QCOM_TRINKET_H

#define MASTER_AMPSS_M0				0
#define MASTER_SNOC_BIMC_RT				1
#define MASTER_SNOC_BIMC_NRT				2
#define SNOC_BIMC_MAS				3
#define MASTER_GPU_CDSP_PROC				4
#define MASTER_TCU_0				5
#define SNOC_CNOC_MAS				6
#define MASTER_CPP				7
#define MASTER_JPEG				8
#define MASTER_VIDEO_P0				9
#define MASTER_VIDEO_PROC				10
#define MASTER_MDP_PORT0				11
#define MASTER_VFE0				12
#define MASTER_QUP_CORE_0				13
#define MASTER_QUP_CORE_1				14
#define MASTER_SNOC_CFG				15
#define MASTER_TIC				16
#define MASTER_ANOC_SNOC				17
#define BIMC_SNOC_MAS				18
#define MASTER_PIMEM				19
#define MASTER_QDSS_BAM				20
#define MASTER_QUP_0				21
#define MASTER_QUP_1				22
#define MASTER_SPDM				23
#define MASTER_CRYPTO_CORE0				24
#define MASTER_IPA				25
#define MASTER_QDSS_DAP				26
#define MASTER_QDSS_ETR				27
#define MASTER_SDCC_1				28
#define MASTER_SDCC_2				29
#define MASTER_UFS_MEM				30
#define MASTER_USB3				31
#define MASTER_GRAPHICS_3D				32
#define SLAVE_EBI_CH0				512
#define BIMC_SNOC_SLV				513
#define SLAVE_BIMC_CFG				514
#define SLAVE_CAMERA_NRT_THROTTLE_CFG				515
#define SLAVE_CAMERA_RT_THROTTLE_CFG				516
#define SLAVE_CAMERA_CFG				517
#define SLAVE_CDSP_THROTTLE_CFG				518
#define SLAVE_CLK_CTL				519
#define SLAVE_CRYPTO_0_CFG				520
#define SLAVE_DISPLAY_THROTTLE_CFG				521
#define SLAVE_GPU_CFG				522
#define SLAVE_IMEM_CFG				523
#define SLAVE_IPA_CFG				524
#define SLAVE_LPASS				525
#define SLAVE_MESSAGE_RAM				526
#define SLAVE_PDM				527
#define SLAVE_PIMEM_CFG				528
#define SLAVE_PMIC_ARB				529
#define SLAVE_PRNG				530
#define SLAVE_QDSS_CFG				531
#define SLAVE_QM_CFG				532
#define SLAVE_QM_MPU_CFG				533
#define SLAVE_QUP_0				534
#define SLAVE_QUP_1				535
#define SLAVE_SDCC_1				536
#define SLAVE_SDCC_2				537
#define SLAVE_SNOC_CFG				538
#define SLAVE_TCSR				539
#define SLAVE_TLMM_EAST				540
#define SLAVE_TLMM_SOUTH				541
#define SLAVE_TLMM_WEST				542
#define SLAVE_UFS_MEM_CFG				543
#define SLAVE_USB3				544
#define SLAVE_VENUS_CFG				545
#define SLAVE_VENUS_THROTTLE_CFG				546
#define SLAVE_VSENSE_CTRL_CFG				547
#define SLAVE_SERVICE_CNOC				548
#define SLAVE_SNOC_BIMC_NRT				549
#define SLAVE_SNOC_BIMC_RT				550
#define SLAVE_QUP_CORE_0				551
#define SLAVE_QUP_CORE_1				552
#define SLAVE_APPSS				553
#define SNOC_CNOC_SLV				554
#define SLAVE_OCIMEM				555
#define SLAVE_PIMEM				556
#define SNOC_BIMC_SLV				557
#define SLAVE_SERVICE_SNOC				558
#define SLAVE_QDSS_STM				559
#define SLAVE_TCU				560
#define SLAVE_ANOC_SNOC				561
#define SLAVE_GPU_CDSP_BIMC				562

#endif
