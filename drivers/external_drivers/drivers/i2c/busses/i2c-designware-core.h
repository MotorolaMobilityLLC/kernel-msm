/*
 * Synopsys DesignWare I2C adapter driver (master only).
 *
 * Based on the TI DAVINCI I2C adapter driver.
 *
 * Copyright (C) 2006 Texas Instruments.
 * Copyright (C) 2007 MontaVista Software Inc.
 * Copyright (C) 2009 Provigent Ltd.
 *
 * ----------------------------------------------------------------------------
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * ----------------------------------------------------------------------------
 *
 */

/*
 * Registers offset
 */
#define DW_IC_CON		0x0
#define DW_IC_TAR		0x4
#define DW_IC_DATA_CMD		0x10
#define DW_IC_SS_SCL_HCNT	0x14
#define DW_IC_SS_SCL_LCNT	0x18
#define DW_IC_FS_SCL_HCNT	0x1c
#define DW_IC_FS_SCL_LCNT	0x20
#define DW_IC_HS_SCL_HCNT	0x24
#define DW_IC_HS_SCL_LCNT	0x28
#define DW_IC_INTR_STAT		0x2c
#define DW_IC_INTR_MASK		0x30
#define DW_IC_RAW_INTR_STAT	0x34
#define DW_IC_RX_TL		0x38
#define DW_IC_TX_TL		0x3c
#define DW_IC_CLR_INTR		0x40
#define DW_IC_CLR_RX_UNDER	0x44
#define DW_IC_CLR_RX_OVER	0x48
#define DW_IC_CLR_TX_OVER	0x4c
#define DW_IC_CLR_RD_REQ	0x50
#define DW_IC_CLR_TX_ABRT	0x54
#define DW_IC_CLR_RX_DONE	0x58
#define DW_IC_CLR_ACTIVITY	0x5c
#define DW_IC_CLR_STOP_DET	0x60
#define DW_IC_CLR_START_DET	0x64
#define DW_IC_CLR_GEN_CALL	0x68
#define DW_IC_ENABLE		0x6c
#define DW_IC_STATUS		0x70
#define DW_IC_TXFLR		0x74
#define DW_IC_RXFLR		0x78
#define DW_IC_TX_ABRT_SOURCE	0x80
#define DW_IC_ENABLE_STATUS	0x9c
#define DW_IC_COMP_PARAM_1	0xf4
#define DW_IC_COMP_TYPE		0xfc
#define DW_IC_COMP_TYPE_VALUE	0x44570140

#define DW_IC_CON_MASTER		0x1
#define DW_IC_CON_SPEED_STD		0x2
#define DW_IC_CON_SPEED_FAST		0x4
#define DW_IC_CON_SPEED_HIGH		0x6
#define DW_IC_CON_10BITADDR_MASTER	0x10
#define DW_IC_CON_RESTART_EN		0x20
#define DW_IC_CON_SLAVE_DISABLE		0x40

#define INTEL_MID_STD_CFG  (DW_IC_CON_MASTER |			\
				DW_IC_CON_SLAVE_DISABLE |	\
				DW_IC_CON_RESTART_EN)

#define DW_IC_INTR_RX_UNDER	0x001
#define DW_IC_INTR_RX_OVER	0x002
#define DW_IC_INTR_RX_FULL	0x004
#define DW_IC_INTR_TX_OVER	0x008
#define DW_IC_INTR_TX_EMPTY	0x010
#define DW_IC_INTR_RD_REQ	0x020
#define DW_IC_INTR_TX_ABRT	0x040
#define DW_IC_INTR_RX_DONE	0x080
#define DW_IC_INTR_ACTIVITY	0x100
#define DW_IC_INTR_STOP_DET	0x200
#define DW_IC_INTR_START_DET	0x400
#define DW_IC_INTR_GEN_CALL	0x800

#define DW_IC_INTR_DEFAULT_MASK		(DW_IC_INTR_RX_FULL | \
					 DW_IC_INTR_TX_EMPTY | \
					 DW_IC_INTR_TX_ABRT | \
					 DW_IC_INTR_STOP_DET | \
					 DW_IC_INTR_RX_OVER)

#define DW_IC_STATUS_ACTIVITY	0x1

#define DW_IC_ERR_TX_ABRT	0x1

#define DW_IC_CMD_STOP		0x200

#define DW_IC_SPEED_MASK	0x6

/*
 * status codes
 */
#define STATUS_POWERON			0x0
#define STATUS_IDLE			STATUS_POWERON
#define STATUS_WRITE_IN_PROGRESS	0x1
#define STATUS_READ_IN_PROGRESS		0x2

#define TIMEOUT			20 /* ms */

/*
 * hardware abort codes from the DW_IC_TX_ABRT_SOURCE register
 *
 * only expected abort codes are listed here
 * refer to the datasheet for the full list
 */
#define ABRT_7B_ADDR_NOACK	0
#define ABRT_10ADDR1_NOACK	1
#define ABRT_10ADDR2_NOACK	2
#define ABRT_TXDATA_NOACK	3
#define ABRT_GCALL_NOACK	4
#define ABRT_GCALL_READ		5
#define ABRT_SBYTE_ACKDET	7
#define ABRT_SBYTE_NORSTRT	9
#define ABRT_10B_RD_NORSTRT	10
#define ABRT_MASTER_DIS		11
#define ARB_LOST		12

#define DW_IC_TX_ABRT_7B_ADDR_NOACK	(1UL << ABRT_7B_ADDR_NOACK)
#define DW_IC_TX_ABRT_10ADDR1_NOACK	(1UL << ABRT_10ADDR1_NOACK)
#define DW_IC_TX_ABRT_10ADDR2_NOACK	(1UL << ABRT_10ADDR2_NOACK)
#define DW_IC_TX_ABRT_TXDATA_NOACK	(1UL << ABRT_TXDATA_NOACK)
#define DW_IC_TX_ABRT_GCALL_NOACK	(1UL << ABRT_GCALL_NOACK)
#define DW_IC_TX_ABRT_GCALL_READ	(1UL << ABRT_GCALL_READ)
#define DW_IC_TX_ABRT_SBYTE_ACKDET	(1UL << ABRT_SBYTE_ACKDET)
#define DW_IC_TX_ABRT_SBYTE_NORSTRT	(1UL << ABRT_SBYTE_NORSTRT)
#define DW_IC_TX_ABRT_10B_RD_NORSTRT	(1UL << ABRT_10B_RD_NORSTRT)
#define DW_IC_TX_ABRT_MASTER_DIS	(1UL << ABRT_MASTER_DIS)
#define DW_IC_TX_ARB_LOST		(1UL << ARB_LOST)

#define DW_IC_TX_ABRT_NOACK		(DW_IC_TX_ABRT_7B_ADDR_NOACK | \
					 DW_IC_TX_ABRT_10ADDR1_NOACK | \
					 DW_IC_TX_ABRT_10ADDR2_NOACK | \
					 DW_IC_TX_ABRT_TXDATA_NOACK | \
					 DW_IC_TX_ABRT_GCALL_NOACK)

/*
 * i2c scl hcnt/lcnt setting
 */
#define PNW_SS_SCLK_HCNT		0x1EC
#define PNW_SS_SCLK_LCNT		0x1F3
#define PNW_FS_SCLK_HCNT		0x66
#define PNW_FS_SCLK_LCNT		0x8B
#define PNW_HS_SCLK_HCNT		0x9
#define PNW_HS_SCLK_LCNT		0x17

#define CLV_SS_SCLK_HCNT		0x1EC
#define CLV_SS_SCLK_LCNT		0x1F3
#define CLV_FS_SCLK_HCNT		0x59
#define CLV_FS_SCLK_LCNT		0x98
#define CLV_HS_SCLK_HCNT		0x8
#define CLV_HS_SCLK_LCNT		0x17

/* inofficial configuration
#define MERR_SS_SCLK_HCNT 0x2c8
#define MERR_SS_SCLK_LCNT 0x380
#define MERR_FS_SCLK_HCNT 0x084
#define MERR_FS_SCLK_LCNT 0x100
*/
#define MERR_SS_SCLK_HCNT 0x2f8
#define MERR_SS_SCLK_LCNT 0x37b
#define MERR_FS_SCLK_HCNT 0x087
#define MERR_FS_SCLK_LCNT 0x10a
#define MERR_HS_SCLK_HCNT 0x8
#define MERR_HS_SCLK_LCNT 0x20

#define VLV2_SS_SCLK_HCNT 0x214
#define VLV2_SS_SCLK_LCNT 0x272
#define VLV2_FS_SCLK_HCNT 0x50
#define VLV2_FS_SCLK_LCNT 0xad
#define VLV2_HS_SCLK_HCNT 0x6
#define VLV2_HS_SCLK_LCNT 0x16
#define VLV2_FS_P_SCLK_HCNT 0x1b
#define VLV2_FS_P_SCLK_LCNT 0x3a

#define CHV_SS_SCLK_HCNT 0x28f
#define CHV_SS_SCLK_LCNT 0x2ba
#define CHV_FS_SCLK_HCNT 0x71
#define CHV_FS_SCLK_LCNT 0xce
#define CHV_FS_P_SCLK_HCNT 0x24
#define CHV_FS_P_SCLK_LCNT 0x53

#define DW_STD_SPEED	100000
#define DW_FAST_SPEED	400000
#define DW_HIGH_SPEED	3400000

struct dw_controller;
/**
 * struct dw_i2c_dev - private i2c-designware data
 * @dev: driver model device node
 * @base: IO registers pointer
 * @cmd_complete: tx completion indicator
 * @lock: protect this struct and IO registers
 * @clk: input reference clock
 * @cmd_err: run time hadware error code
 * @msgs: points to an array of messages currently being transfered
 * @msgs_num: the number of elements in msgs
 * @msg_write_idx: the element index of the current tx message in the msgs
 *	array
 * @tx_buf_len: the length of the current tx buffer
 * @tx_buf: the current tx buffer
 * @msg_read_idx: the element index of the current rx message in the msgs
 *	array
 * @rx_buf_len: the length of the current rx buffer
 * @rx_buf: the current rx buffer
 * @msg_err: error status of the current transfer
 * @status: i2c master status, one of STATUS_*
 * @abort_source: copy of the TX_ABRT_SOURCE register
 * @irq: interrupt number for the i2c master
 * @adapter: i2c subsystem adapter node
 * @tx_fifo_depth: depth of the hardware tx fifo
 * @rx_fifo_depth: depth of the hardware rx fifo
 * @shared_host: if this host is shared by other units on the SoC
 */
struct dw_i2c_dev {
	struct device		*dev;
	void __iomem		*base;
	struct completion	cmd_complete;
	struct semaphore	lock;
	struct clk		*clk;
	u32			(*get_clk_rate_khz) (struct dw_i2c_dev *dev);
	int			(*get_scl_cfg) (struct dw_i2c_dev *dev);
	void			(*reset)(struct dw_i2c_dev *dev);
	int			(*abort)(int busnum);
	struct dw_controller 	*controller;
	int			enable_stop;
	int			share_irq;
	int			shared_host;
	int			(*acquire_ownership) (void);
	int			(*release_ownership) (void);
	int			cmd_err;
	struct i2c_msg		*msgs;
	int			msgs_num;
	int			msg_write_idx;
	u32			tx_buf_len;
	u8			*tx_buf;
	int			msg_read_idx;
	u32			rx_buf_len;
	u8			*rx_buf;
	int			msg_err;
	unsigned int		status;
	u32			abort_source;
	int			irq;
	int			swab;
	struct i2c_adapter	adapter;
	u32			functionality;
	u32			master_cfg;
	unsigned int		tx_fifo_depth;
	unsigned int		rx_fifo_depth;
	int			use_dyn_clk;	/* use dynamic clk setting */
	u32			clk_khz;	/* input clock */
	u32			speed_cfg;
	u32			lock_flag;
	u32			freq;
	u32			fast_plus;
};

struct dw_controller {
	u32 bus_num;
	u32 bus_cfg;
	u32 tx_fifo_depth;
	u32 rx_fifo_depth;
	u32 clk_khz;
	u32 fast_plus;
	int enable_stop;
	int share_irq;
	char *acpi_name;
	int (*scl_cfg) (struct dw_i2c_dev *dev);
	void (*reset)(struct dw_i2c_dev *dev);
};

enum dw_ctl_id_t {
	moorestown_0,
	moorestown_1,
	moorestown_2,

	medfield_0,
	medfield_1,
	medfield_2,
	medfield_3,
	medfield_4,
	medfield_5,

	cloverview_0,
	cloverview_1,
	cloverview_2,
	cloverview_3,
	cloverview_4,
	cloverview_5,

	merrifield_0,
	merrifield_1,
	merrifield_2,
	merrifield_3,
	merrifield_4,
	merrifield_5,
	merrifield_6,

	valleyview_0,
	valleyview_1,
	valleyview_2,
	valleyview_3,
	valleyview_4,
	valleyview_5,
	valleyview_6,
	valleyview_7,

	cherryview_0,
	cherryview_1,
	cherryview_2,
	cherryview_3,
	cherryview_4,
	cherryview_5,
	cherryview_6,
	cherryview_7,
};

extern int intel_mid_dw_i2c_abort(int busnum);
int i2c_dw_init(struct dw_i2c_dev *dev);
struct dw_i2c_dev *i2c_dw_setup(struct device *pdev, int bus_idx,
	unsigned long start, unsigned long len, int irq);
void i2c_dw_free(struct device *pdev, struct dw_i2c_dev *dev);
int i2c_dw_suspend(struct dw_i2c_dev *dev, bool runtime);
int i2c_dw_resume(struct dw_i2c_dev *dev, bool runtime);
extern int intel_mid_dw_i2c_acquire_ownership(void);
extern int intel_mid_dw_i2c_release_ownership(void);
