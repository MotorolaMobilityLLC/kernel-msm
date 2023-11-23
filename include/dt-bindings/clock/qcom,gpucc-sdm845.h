/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _DT_BINDINGS_CLK_SDM_GPU_CC_SDM845_H
#define _DT_BINDINGS_CLK_SDM_GPU_CC_SDM845_H

/* GPUCC clock registers */
#define GPU_CC_ACD_AHB_CLK					0
#define GPU_CC_ACD_CXO_CLK					1
#define GPU_CC_CRC_AHB_CLK					2
#define GPU_CC_CX_APB_CLK					3
#define GPU_CC_CX_GMU_CLK					4
#define GPU_CC_CX_QDSS_AT_CLK					5
#define GPU_CC_CX_QDSS_TRIG_CLK					6
#define GPU_CC_CX_QDSS_TSCTR_CLK				7
#define GPU_CC_CX_SNOC_DVM_CLK					8
#define GPU_CC_CXO_AON_CLK					9
#define GPU_CC_CXO_CLK						10
#define GPU_CC_GX_GMU_CLK					11
#define GPU_CC_GX_QDSS_TSCTR_CLK				12
#define GPU_CC_GX_VSENSE_CLK					13
#define GPU_CC_PLL0						14
#define GPU_CC_PLL0_OUT_EVEN					15
#define GPU_CC_PLL0_OUT_MAIN					16
#define GPU_CC_PLL0_OUT_ODD					17
#define GPU_CC_PLL1						18
#define GPU_CC_PLL1_OUT_EVEN					19
#define GPU_CC_PLL1_OUT_MAIN					20
#define GPU_CC_PLL1_OUT_ODD					21
#define GPU_CC_SLEEP_CLK					22
#define GPU_CC_GMU_CLK_SRC					23
#define GPU_CC_CX_GFX3D_CLK					24
#define GPU_CC_CX_GFX3D_SLV_CLK					25
#define GPU_CC_GX_GFX3D_CLK_SRC					26
#define GPU_CC_GX_GFX3D_CLK					27

/* GPU_CC GDSCRs */
#define GPU_CX_GDSC                            0
#define GPU_GX_GDSC                            1

/* GPUCC reset clock registers */
#define GPUCC_GPU_CC_ACD_BCR					0
#define GPUCC_GPU_CC_CX_BCR					1
#define GPUCC_GPU_CC_GFX3D_AON_BCR				2
#define GPUCC_GPU_CC_GMU_BCR					3
#define GPUCC_GPU_CC_GX_BCR					4
#define GPUCC_GPU_CC_SPDM_BCR					5
#define GPUCC_GPU_CC_XO_BCR					6

#endif
