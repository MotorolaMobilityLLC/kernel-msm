#ifndef _MFD_H
#define _MFD_H

#include <linux/serial_core.h>
#include <linux/serial_reg.h>
#include <linux/serial_mfd.h>
#include <linux/intel_mid_dma.h>
#include <linux/intel_mid_pm.h>
#include <linux/dma-direction.h>
#include <asm/intel_mid_hsu.h>

#define HSU_PORT_MAX		8
#define HSU_DMA_BUF_SIZE	2048
#define HSU_Q_MAX		4096
#define HSU_CL_BUF_LEN		(1 << CONFIG_LOG_BUF_SHIFT)
#define HSU_DMA_BSR		32
#define HSU_DMA_MOTSR		4
#define HSU_PIO_RX_ERR		0x06
#define HSU_PIO_RX_AVB		0x04
#define HSU_PIO_RX_TMO		0x0C
#define HSU_PIO_TX_REQ		0x02

enum {
	flag_console = 0,
	flag_reopen,
	flag_suspend,
	flag_active,
	flag_set_alt,
	flag_tx_on,
	flag_rx_on,
	flag_rx_pending,
	flag_startup,
	flag_cmd_on,
	flag_cmd_off,
};

enum {
	qcmd_overflow = 0,
	qcmd_get_msr,
	qcmd_set_mcr,
	qcmd_set_ier,
	qcmd_start_rx,
	qcmd_stop_rx,
	qcmd_start_tx,
	qcmd_stop_tx,
	qcmd_cl,
	qcmd_port_irq,
	qcmd_dma_irq,
	qcmd_enable_irq,
	qcmd_cmd_off,
	qcmd_max,
};

enum {
	context_save,
	context_load,
};

struct hsu_dma_buffer {
	u8		*buf;
	dma_addr_t	dma_addr;
	u32		dma_size;
	u32		ofs;
};

struct hsu_dma_chan {
	u32	id;
	enum dma_data_direction	dirt;
	struct uart_hsu_port	*uport;
	void __iomem		*reg;
	u32	cr;
	u32	dcr;
	u32	sar;
	u32	tsr;
};

struct dw_dma_priv {
	struct intel_mid_dma_slave	txs;
	struct intel_mid_dma_slave	rxs;

	struct uart_hsu_port	*up;

	struct dma_chan		*txchan;
	struct dma_chan		*rxchan;

	/* phy address of the Data register */
	dma_addr_t		dma_addr;
	struct pci_dev		*dmac;
};

struct intel_dma_priv {
	unsigned int		tx_addr;
	struct hsu_dma_chan	*txc;
	struct hsu_dma_chan	*rxc;
};

struct hsu_dma_ops {
	int (*init)(struct uart_hsu_port *up);
	int (*exit)(struct uart_hsu_port *up);
	int (*suspend)(struct uart_hsu_port *up);
	int (*resume)(struct uart_hsu_port *up);
	void (*start_tx)(struct uart_hsu_port *up);
	void (*stop_tx)(struct uart_hsu_port *up);
	void (*start_rx)(struct uart_hsu_port *up);
	void (*stop_rx)(struct uart_hsu_port *up);
	/* op will be context_save or context_load */
	void (*context_op)(struct uart_hsu_port *up, int op);
};

struct uart_hsu_port {
	struct uart_port        port;
	struct mutex		q_mutex;
	int			q_start;
	struct workqueue_struct *workqueue;
	struct work_struct	work;
	struct tasklet_struct	tasklet;
	struct circ_buf		qcirc;
	int			qbuf[HSU_Q_MAX];
	struct circ_buf		cl_circ;
	spinlock_t		cl_lock;

	/* Intel HSU or Designware */
	int			hw_type;

	unsigned char           msr;
	unsigned char           ier;
	unsigned char           lcr;
	unsigned char           mcr;
	unsigned char           lsr;
	unsigned char           dll;
	unsigned char           dlm;
	unsigned char		fcr;
	/* intel_hsu's clk param */
	unsigned int		mul;
	unsigned int		div;
	unsigned int		ps;

	/* Buffered value due to runtime PM and sharing IRQ */
	unsigned char		iir;

	/* intel_dw's clk param */
	unsigned int		m;
	unsigned int		n;

	unsigned int            lsr_break_flag;
	char			name[24];
	int			index;
	struct device		*dev;

	unsigned int		tx_addr;
	struct hsu_dma_chan	*txc;
	struct hsu_dma_chan	*rxc;
	struct hsu_dma_buffer	txbuf;
	struct hsu_dma_buffer	rxbuf;

	unsigned char		rxc_chcr_save;

	unsigned long		flags;

	unsigned int		qcmd_num;
	unsigned int		qcmd_done;
	unsigned int		port_irq_num;
	unsigned int		port_irq_cmddone;
	unsigned int		port_irq_no_alt;
	unsigned int		port_irq_no_startup;
	unsigned int		port_irq_pio_no_irq_pend;
	unsigned int		port_irq_pio_tx_req;
	unsigned int		port_irq_pio_rx_avb;
	unsigned int		port_irq_pio_rx_err;
	unsigned int		port_irq_pio_rx_timeout;
	unsigned int		cts_status;
	unsigned int		dma_irq_num;
	unsigned int		dma_invalid_irq_num;
	unsigned int		dma_irq_cmddone;
	unsigned int		dma_tx_irq_cmddone;
	unsigned int		dma_rx_irq_cmddone;
	unsigned int		dma_rx_tmt_irq_cmddone;
	unsigned int		tasklet_done;
	unsigned int		workq_done;
	unsigned int		in_workq;
	unsigned int		in_tasklet;

	unsigned int		byte_delay;

	int			use_dma;	/* flag for DMA/PIO */
	unsigned int		dma_irq;
	unsigned int		port_dma_sts;

	void			*dma_priv;
	struct hsu_dma_ops	*dma_ops;
	struct pm_qos_request   qos;
	int			dma_inited;
};

struct hsu_port {
	int dma_irq;
	int port_num;
	int irq_port_and_dma;
	struct hsu_port_cfg	*configs[HSU_PORT_MAX];
	void __iomem	*reg;
	struct uart_hsu_port	port[HSU_PORT_MAX];
	struct hsu_dma_chan	chans[HSU_PORT_MAX * 2];
	spinlock_t		dma_lock;
	struct dentry *debugfs;
};

#define chan_readl(chan, offset)	readl(chan->reg + offset)
#define chan_writel(chan, offset, val)	writel(val, chan->reg + offset)

#define mfd_readl(obj, offset)		readl(obj->reg + offset)
#define mfd_writel(obj, offset, val)	writel(val, obj->reg + offset)

static inline unsigned int serial_in(struct uart_hsu_port *up, int offset)
{
	unsigned int val;

	if (offset > UART_MSR || up->hw_type == hsu_dw) {
		offset <<= 2;
		val = readl(up->port.membase + offset);
	} else
		val = (unsigned int)readb(up->port.membase + offset);

	return val;
}

static inline void serial_out(struct uart_hsu_port *up, int offset, int value)
{
	if (offset > UART_MSR || up->hw_type == hsu_dw) {
		offset <<= 2;
		writel(value, up->port.membase + offset);
	} else {
		unsigned char val = value & 0xff;
		writeb(val, up->port.membase + offset);
	}
}
void serial_sched_cmd(struct uart_hsu_port *up, char cmd);
extern struct hsu_dma_ops *pdw_dma_ops;
extern struct hsu_dma_ops intel_dma_ops;

struct uart_hsu_port *serial_hsu_port_setup(struct device *pdev, int port,
	resource_size_t start, resource_size_t len, int irq);
void serial_hsu_port_free(struct uart_hsu_port *up);
void serial_hsu_port_shutdown(struct uart_hsu_port *up);
int serial_hsu_dma_setup(struct device *pdev,
	resource_size_t start, resource_size_t len, unsigned int irq, int share);
void serial_hsu_dma_free(void);
int serial_hsu_do_suspend(struct uart_hsu_port *up);
int serial_hsu_do_resume(struct uart_hsu_port *up);
int serial_hsu_do_runtime_idle(struct uart_hsu_port *up);

#include "mfd_trace.h"
#endif
