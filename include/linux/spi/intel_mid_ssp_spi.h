/*
 *  Copyright (C) Intel 2009
 *  Ken Mills <ken.k.mills@intel.com>
 *  Sylvain Centelles <sylvain.centelles@intel.com>
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 */
#ifndef INTEL_MID_SSP_SPI_H_
#define INTEL_MID_SSP_SPI_H_

#include <linux/intel_mid_dma.h>
#include <linux/pm_qos.h>
#include <linux/spi/spi.h>
#include <linux/interrupt.h>
#include <linux/wakelock.h>
#include <linux/completion.h>

#define PCI_MRST_DMAC1_ID	0x0814
#define PCI_MDFL_DMAC1_ID	0x0827
#define PCI_BYT_DMAC1_ID	0x0f06
#define PCI_MRFL_DMAC_ID	0x11A2

#define SSP_NOT_SYNC 0x400000
#define MAX_SPI_TRANSFER_SIZE 8192
#define MAX_BITBANGING_LOOP   10000
#define SPI_FIFO_SIZE 16

/* PM QoS define */
#define MIN_EXIT_LATENCY 20

/* SSP assignement configuration from PCI config */

#define SSP_CFG_SPI_MODE_ID		1
/* adid field offset is 6 inside the vendor specific capability */
#define VNDR_CAPABILITY_ADID_OFFSET	6

/* Driver's quirk flags */
/* This workarround bufferizes data in the audio fabric SDRAM from  */
/* where the DMA transfers will operate. Should be enabled only for */
/* SPI slave mode.                                                  */
#define QUIRKS_SRAM_ADDITIONAL_CPY	1
/* If set the trailing bytes won't be handled by the DMA.           */
/* Trailing byte feature not fully available.                       */
#define QUIRKS_DMA_USE_NO_TRAIL		2
/* If set, the driver will use PM_QOS to reduce the latency         */
/* introduced by the deeper C-states which may produce over/under   */
/* run issues. Must be used in slave mode. In master mode, the      */
/* latency is not critical, but setting this workarround  may       */
/* improve the SPI throughput.                                      */
#define QUIRKS_USE_PM_QOS		4
/* This quirks is set on Moorestown                                 */
#define QUIRKS_PLATFORM_MRST		8
/* This quirks is set on Medfield                                   */
#define QUIRKS_PLATFORM_MDFL		16
/* If set, the driver will apply the bitbanging workarround needed  */
/* to enable defective Langwell stepping A SSP. The defective SSP   */
/* can be enabled only once, and should never be disabled.          */
#define QUIRKS_BIT_BANGING		32
/* If set, SPI is in slave clock mode                               */
#define QUIRKS_SPI_SLAVE_CLOCK_MODE	64
/* Add more platform here. */
/* This quirks is set on Baytrail. */
#define QUIRKS_PLATFORM_BYT		128
#define QUIRKS_PLATFORM_MRFL		256

/* Uncomment to get RX and TX short dumps after each transfer */
/* #define DUMP_RX 1 */
#define MAX_TRAILING_BYTE_RETRY 16
#define MAX_TRAILING_BYTE_LOOP 100
#define DELAY_TO_GET_A_WORD 3
#define DFLT_TIMEOUT_VAL 500

#define DEFINE_SSP_REG(reg, off) \
static inline u32 read_##reg(void *p) { return __raw_readl(p + (off)); } \
static inline void write_##reg(u32 v, void *p) { __raw_writel(v, p + (off)); }

#define RX_DIRECTION 0
#define TX_DIRECTION 1

#define I2C_ACCESS_USDELAY 10

#define DFLT_BITS_PER_WORD 16
#define MIN_BITS_PER_WORD     4
#define MAX_BITS_PER_WORD     32
#define DFLT_FIFO_BURST_SIZE	IMSS_FIFO_BURST_8

#define TRUNCATE(x, a) ((x) & ~((a)-1))

DEFINE_SSP_REG(SSCR0, 0x00)
DEFINE_SSP_REG(SSCR1, 0x04)
DEFINE_SSP_REG(SSSR, 0x08)
DEFINE_SSP_REG(SSITR, 0x0c)
DEFINE_SSP_REG(SSDR, 0x10)
DEFINE_SSP_REG(SSTO, 0x28)
DEFINE_SSP_REG(SSPSP, 0x2c)
DEFINE_SSP_REG(SSFS, 0x44)
DEFINE_SSP_REG(SFIFOL, 0x68)

DEFINE_SSP_REG(I2CCTRL, 0x00);
DEFINE_SSP_REG(I2CDATA, 0x04);

DEFINE_SSP_REG(GPLR1, 0x04);
DEFINE_SSP_REG(GPDR1, 0x0c);
DEFINE_SSP_REG(GPSR1, 0x14);
DEFINE_SSP_REG(GPCR1, 0x1C);
DEFINE_SSP_REG(GAFR1_U, 0x44);

#define SYSCFG  0x20bc0

#define SRAM_BASE_ADDR 0xfffdc000
#define SRAM_RX_ADDR   SRAM_BASE_ADDR
#define SRAM_TX_ADDR  (SRAM_BASE_ADDR + MAX_SPI_TRANSFER_SIZE)

#define SSCR0_DSS   (0x0000000f)     /* Data Size Select (mask) */
#define SSCR0_DataSize(x)  ((x) - 1)    /* Data Size Select [4..16] */
#define SSCR0_FRF   (0x00000030)     /* FRame Format (mask) */
#define SSCR0_Motorola        (0x0 << 4)         /* Motorola's SPI mode */
#define SSCR0_ECS   (1 << 6) /* External clock select */
#define SSCR0_SSE   (1 << 7) /* Synchronous Serial Port Enable */

#define SSCR0_SCR   (0x000fff00)      /* Serial Clock Rate (mask) */
#define SSCR0_SerClkDiv(x) (((x) - 1) << 8) /* Divisor [1..4096] */
#define SSCR0_EDSS  (1 << 20)           /* Extended data size select */
#define SSCR0_NCS   (1 << 21)           /* Network clock select */
#define SSCR0_RIM    (1 << 22)           /* Receive FIFO overrrun int mask */
#define SSCR0_TUM   (1 << 23)           /* Transmit FIFO underrun int mask */
#define SSCR0_FRDC (0x07000000)     /* Frame rate divider control (mask) */
#define SSCR0_SlotsPerFrm(x) (((x) - 1) << 24) /* Time slots per frame */
#define SSCR0_ADC   (1 << 30)           /* Audio clock select */
#define SSCR0_MOD  (1 << 31)           /* Mode (normal or network) */

#define SSCR1_RIE    (1 << 0) /* Receive FIFO Interrupt Enable */
#define SSCR1_TIE     (1 << 1) /* Transmit FIFO Interrupt Enable */
#define SSCR1_LBM   (1 << 2) /* Loop-Back Mode */
#define SSCR1_SPO   (1 << 3) /* SSPSCLK polarity setting */
#define SSCR1_SPH   (1 << 4) /* Motorola SPI SSPSCLK phase setting */
#define SSCR1_MWDS           (1 << 5) /* Microwire Transmit Data Size */
#define SSCR1_TFT    (0x000003c0)     /* Transmit FIFO Threshold (mask) */
#define SSCR1_TxTresh(x) (((x) - 1) << 6) /* level [1..16] */
#define SSCR1_RFT    (0x00003c00)     /* Receive FIFO Threshold (mask) */
#define SSCR1_RxTresh(x) (((x) - 1) << 10) /* level [1..16] */

#define SSSR_TNF		(1 << 2)	/* Tx FIFO Not Full */
#define SSSR_RNE		(1 << 3)	/* Rx FIFO Not Empty */
#define SSSR_BSY		(1 << 4)	/* SSP Busy */
#define SSSR_TFS		(1 << 5)	/* Tx FIFO Service Request */
#define SSSR_RFS		(1 << 6)	/* Rx FIFO Service Request */
#define SSSR_ROR		(1 << 7)	/* Rx FIFO Overrun */
#define SSSR_TFL_MASK           (0x0F << 8)     /* Tx FIFO level field mask */
#define SSSR_RFL_SHIFT		12		/* Rx FIFO MASK shift */
#define SSSR_RFL_MASK		(0x0F << SSSR_RFL_SHIFT)/* RxFIFOlevel mask */

#define SSCR0_TIM    (1 << 23)          /* Transmit FIFO Under Run Int Mask */
#define SSCR0_RIM    (1 << 22)          /* Receive FIFO Over Run int Mask */
#define SSCR0_NCS    (1 << 21)          /* Network Clock Select */
#define SSCR0_EDSS   (1 << 20)          /* Extended Data Size Select */

#define SSCR0_TISSP      (1 << 4)  /* TI Sync Serial Protocol */
#define SSCR0_PSP        (3 << 4)  /* PSP - Programmable Serial Protocol */
#define SSCR1_TTELP      (1 << 31) /* TXD Tristate Enable Last Phase */
#define SSCR1_TTE        (1 << 30) /* TXD Tristate Enable */
#define SSCR1_EBCEI      (1 << 29) /* Enable Bit Count Error interrupt */
#define SSCR1_SCFR       (1 << 28) /* Slave Clock free Running */
#define SSCR1_ECRA       (1 << 27) /* Enable Clock Request A */
#define SSCR1_ECRB       (1 << 26) /* Enable Clock request B */
#define SSCR1_SCLKDIR    (1 << 25) /* Serial Bit Rate Clock Direction */
#define SSCR1_SFRMDIR    (1 << 24) /* Frame Direction */
#define SSCR1_RWOT       (1 << 23) /* Receive Without Transmit */
#define SSCR1_TRAIL      (1 << 22) /* Trailing Byte */
#define SSCR1_TSRE       (1 << 21) /* Transmit Service Request Enable */
#define SSCR1_RSRE       (1 << 20) /* Receive Service Request Enable */
#define SSCR1_TINTE      (1 << 19) /* Receiver Time-out Interrupt enable */
#define SSCR1_PINTE      (1 << 18) /* Trailing Byte Interupt Enable */
#define SSCR1_STRF       (1 << 15) /* Select FIFO or EFWR */
#define SSCR1_EFWR       (1 << 14) /* Enable FIFO Write/Read */
#define SSCR1_IFS        (1 << 16) /* Invert Frame Signal */

#define SSSR_BCE         (1 << 23) /* Bit Count Error */
#define SSSR_CSS         (1 << 22) /* Clock Synchronisation Status */
#define SSSR_TUR         (1 << 21) /* Transmit FIFO Under Run */
#define SSSR_EOC         (1 << 20) /* End Of Chain */
#define SSSR_TINT        (1 << 19) /* Receiver Time-out Interrupt */
#define SSSR_PINT        (1 << 18) /* Peripheral Trailing Byte Interrupt */

#define SSPSP_FSRT       (1 << 25)   /* Frame Sync Relative Timing */
#define SSPSP_DMYSTOP(x) ((x) << 23) /* Dummy Stop */
#define SSPSP_SFRMWDTH(x)((x) << 16) /* Serial Frame Width */
#define SSPSP_SFRMDLY(x) ((x) << 9)  /* Serial Frame Delay */
#define SSPSP_DMYSTRT(x) ((x) << 7)  /* Dummy Start */
#define SSPSP_STRTDLY(x) ((x) << 4)  /* Start Delay */
#define SSPSP_ETDS       (1 << 3)    /* End of Transfer data State */
#define SSPSP_SFRMP      (1 << 2)    /* Serial Frame Polarity */
#define SSPSP_SCMODE(x)  ((x) << 0)  /* Serial Bit Rate Clock Mode */

/*
 * For testing SSCR1 changes that require SSP restart, basically
 * everything except the service and interrupt enables
 */

#define SSCR1_CHANGE_MASK (SSCR1_TTELP | SSCR1_TTE | SSCR1_SCFR \
				| SSCR1_ECRA | SSCR1_ECRB | SSCR1_SCLKDIR \
				| SSCR1_SFRMDIR | SSCR1_RWOT | SSCR1_TRAIL \
				| SSCR1_IFS | SSCR1_STRF | SSCR1_EFWR \
				| SSCR1_RFT | SSCR1_TFT | SSCR1_MWDS \
				| SSCR1_SPH | SSCR1_SPO | SSCR1_LBM)

/* add CS control call back feature to give user capability
to control CS signal by themselves*/
#define CS_DEASSERT	0
#define CS_ASSERT		1

struct callback_param {
	void *drv_context;
	u32 direction;
};

struct ssp_drv_context {
	/* Driver model hookup */
	struct pci_dev *pdev;

	/* SPI framework hookup */
	struct spi_master *master;

	/* SSP register addresses */
	unsigned long paddr;
	void *ioaddr;
	int irq;

	/* I2C registers */
	dma_addr_t I2C_paddr;
	void *I2C_ioaddr;

	/* SSP masks*/
	u32 cr1_sig;
	u32 cr1;
	u32 clear_sr;
	u32 mask_sr;

	/* PM_QOS request */
	struct pm_qos_request pm_qos_req;

	struct tasklet_struct poll_transfer;

	spinlock_t lock;
	struct wake_lock	 stay_awake;
	struct workqueue_struct *workqueue;
	struct work_struct pump_messages;
	struct list_head queue;
	struct completion msg_done;

	int suspended;

	/* Current message transfer state info */
	struct spi_message *cur_msg;
	size_t len;
	size_t len_dma_rx;
	size_t len_dma_tx;
	void *tx;
	void *tx_end;
	void *rx;
	void *rx_end;
	bool dma_initialized;
	int dma_mapped;
	dma_addr_t rx_dma;
	dma_addr_t tx_dma;
	u8 n_bytes;
	int (*write)(struct ssp_drv_context *sspc);
	int (*read)(struct ssp_drv_context *sspc);

	struct intel_mid_dma_slave    dmas_tx;
	struct intel_mid_dma_slave    dmas_rx;
	struct dma_chan    *txchan;
	struct dma_chan    *rxchan;
	struct workqueue_struct *dma_wq;
	struct work_struct complete_work;

	u8 __iomem *virt_addr_sram_tx;
	u8 __iomem *virt_addr_sram_rx;

	int txdma_done;
	int rxdma_done;
	struct callback_param tx_param;
	struct callback_param rx_param;
	struct pci_dev *dmac1;

	unsigned long quirks;
	u32 rx_fifo_threshold;

	int cs_change;
	void (*cs_control)(u32 command);
};

struct chip_data {
	u32 cr0;
	u32 cr1;
	u32 timeout;
	u8 chip_select;
	u8 n_bytes;
	u8 dma_enabled;
	u8 bits_per_word;
	u32 speed_hz;
	int (*write)(struct ssp_drv_context *sspc);
	int (*read)(struct ssp_drv_context *sspc);
	void (*cs_control)(u32 command);
};


enum intel_mid_ssp_spi_fifo_burst {
	IMSS_FIFO_BURST_1,
	IMSS_FIFO_BURST_4,
	IMSS_FIFO_BURST_8
};

/* spi_board_info.controller_data for SPI slave devices,
 * copied to spi_device.platform_data ... mostly for dma tuning
 */
struct intel_mid_ssp_spi_chip {
	enum intel_mid_ssp_spi_fifo_burst burst_size;
	u32 timeout;
	u8 enable_loopback;
	u8 dma_enabled;
	void (*cs_control)(u32 command);
};

#define SPI_DIB_NAME_LEN  16
#define SPI_DIB_SPEC_INFO_LEN      10

struct spi_dib_header {
	u32       signature;
	u32       length;
	u8         rev;
	u8         checksum;
	u8         dib[0];
} __packed;

#endif /*INTEL_MID_SSP_SPI_H_*/
