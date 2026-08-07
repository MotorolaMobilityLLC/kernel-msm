/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2024 Broadcom Corporation
 * DW25GMAC definitions.
 */
#ifndef __STMMAC_DW25GMAC_H__
#define __STMMAC_DW25GMAC_H__

/* Hardware features */
#define XXVGMAC_HWFEAT_VDMA_RXCNT	GENMASK(16, 12)
#define XXVGMAC_HWFEAT_VDMA_TXCNT	GENMASK(22, 18)

/* DMA Indirect Registers*/
#define XXVGMAC_DMA_CH_IND_CONTROL	0x00003080
#define XXVGMAC_MODE_SELECT		GENMASK(27, 24)
enum dma_ch_ind_modes {
	MODE_TXEXTCFG	 = 0x0,	  /* Tx Extended Config */
	MODE_RXEXTCFG	 = 0x1,	  /* Rx Extended Config */
	MODE_TXDBGSTS	 = 0x2,	  /* Tx Debug Status */
	MODE_RXDBGSTS	 = 0x3,	  /* Rx Debug Status */
	MODE_TXDESCCTRL	 = 0x4,	  /* Tx Descriptor control */
	MODE_RXDESCCTRL	 = 0x5,	  /* Rx Descriptor control */
};

#define XXVGMAC_ADDR_OFFSET		GENMASK(14, 8)
#define XXVGMAC_AUTO_INCR		GENMASK(5, 4)
#define XXVGMAC_CMD_TYPE		BIT(1)
#define XXVGMAC_OB			BIT(0)
#define XXVGMAC_DMA_CH_IND_DATA		0x00003084

/* TX Config definitions */
#define XXVGMAC_TXPBL			GENMASK(29, 24)
#define XXVGMAC_TPBLX8_MODE		BIT(19)
#define XXVGMAC_TP2TCMP			GENMASK(18, 16)
#define XXVGMAC_ORRQ			GENMASK(13, 8)

/* RX Config definitions */
#define XXVGMAC_RXPEN			BIT(31)
#define XXVGMAC_RXPBL			GENMASK(29, 24)
#define XXVGMAC_RPBLX8_MODE		BIT(19)
#define XXVGMAC_RP2TCMP			GENMASK(18, 16)
#define XXVGMAC_OWRQ			GENMASK(13, 8)

/* Tx Descriptor control */
#define XXVGMAC_TXDCSZ			GENMASK(2, 0)
#define XXVGMAC_TDPS			GENMASK(5, 3)

/* Rx Descriptor control */
#define XXVGMAC_RXDCSZ			GENMASK(2, 0)
#define XXVGMAC_RDPS			GENMASK(5, 3)

/* DWCXG_DMA_CH(#i) Registers*/
#define XXVGMAC_DSL			GENMASK(20, 18)
#define XXVGMAC_MSS			GENMASK(13, 0)
#define XXVGMAC_TFSEL			GENMASK(30, 29)
#define XXVGMAC_TQOS			GENMASK(27, 24)
#define XXVGMAC_IPBL			BIT(15)
#define XXVGMAC_TVDMA2TCMP		GENMASK(6, 4)
#define XXVGMAC_RPF			BIT(31)
#define XXVGMAC_RVDMA2TCMP		GENMASK(30, 28)
#define XXVGMAC_RQOS			GENMASK(27, 24)

u32 dw25gmac_decode_vdma_count(u32 regval);

void dw25gmac_dma_init(struct stmmac_priv *priv, void __iomem *ioaddr,
		       struct stmmac_dma_cfg *dma_cfg, int atds);

void dw25gmac_dma_init_tx_chan(struct stmmac_priv *priv,
			       void __iomem *ioaddr,
			       struct stmmac_dma_cfg *dma_cfg,
			       dma_addr_t dma_addr, u32 chan);
void dw25gmac_dma_init_rx_chan(struct stmmac_priv *priv,
			       void __iomem *ioaddr,
			       struct stmmac_dma_cfg *dma_cfg,
			       dma_addr_t dma_addr, u32 chan);
void dw25gmac_dma_map_tx_offline_chan(struct stmmac_priv *priv,
				      void __iomem *ioaddr,
				      struct stmmac_dma_cfg *dma_cfg,
				      u32 chan);
void dw25gmac_dma_map_rx_offline_chan(struct stmmac_priv *priv,
				      void __iomem *ioaddr,
				      struct stmmac_dma_cfg *dma_cfg,
				      u32 chan);
void dw25gmac_desc_cache_compute(void __iomem *ioaddr);
#endif /* __STMMAC_DW25GMAC_H__ */
