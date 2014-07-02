#include <linux/module.h>
#include <linux/init.h>
#include <linux/console.h>
#include <linux/sysrq.h>
#include <linux/slab.h>
#include <linux/serial_reg.h>
#include <linux/circ_buf.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial_core.h>
#include <linux/serial_mfd.h>
#include <linux/dma-mapping.h>
#include <linux/pci.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/pm_runtime.h>
#include <linux/intel_mid_pm.h>
#include <linux/intel_mid_dma.h>
#include <linux/irq.h>
#include <linux/acpi.h>
#include <asm/intel_mid_hsu.h>
#include <linux/intel_mid_pm.h>
#include <linux/pm_qos.h>

#include "mfd.h"

static int dma_init_common(struct uart_hsu_port *up)
{
	struct hsu_dma_buffer *dbuf;
	struct circ_buf *xmit = &up->port.state->xmit;

	/* 1. Allocate the RX buffer */
	dbuf = &up->rxbuf;
	dbuf->buf = kzalloc(HSU_DMA_BUF_SIZE, GFP_KERNEL);
	if (!dbuf->buf) {
		up->use_dma = 0;
		dev_err(up->dev, "allocate DMA buffer failed!!\n");
		return -ENOMEM;
	}

	dbuf->dma_addr = dma_map_single(up->dev,
			dbuf->buf,
			HSU_DMA_BUF_SIZE,
			DMA_FROM_DEVICE);
	dbuf->dma_size = HSU_DMA_BUF_SIZE;

	/* 2. prepare teh TX buffer */
	dbuf = &up->txbuf;
	dbuf->buf = xmit->buf;
	dbuf->dma_addr = dma_map_single(up->dev,
			dbuf->buf,
			UART_XMIT_SIZE,
			DMA_TO_DEVICE);
	dbuf->dma_size = UART_XMIT_SIZE;
	dbuf->ofs = 0;
	return 0;
}

static void dma_exit_common(struct uart_hsu_port *up)
{
	struct hsu_dma_buffer *dbuf;
	struct uart_port *port = &up->port;

	/* Free and unmap rx dma buffer */
	dbuf = &up->rxbuf;
	dma_unmap_single(port->dev,
			dbuf->dma_addr,
			dbuf->dma_size,
			DMA_FROM_DEVICE);
	kfree(dbuf->buf);

	/* Next unmap tx dma buffer*/
	dbuf = &up->txbuf;
	dma_unmap_single(port->dev,
			dbuf->dma_addr,
			dbuf->dma_size,
			DMA_TO_DEVICE);
}

#ifdef CONFIG_INTEL_MID_DMAC
static bool dw_dma_chan_filter(struct dma_chan *chan, void *param)
{
	struct dw_dma_priv *dw_dma = param;

	if (dw_dma->dmac && (&dw_dma->dmac->dev == chan->device->dev))
		return true;
	else {
#ifdef CONFIG_ACPI
		acpi_handle handle = ACPI_HANDLE(chan->device->dev);
		struct acpi_device *device;
		int ret;
		const char *hid;
		ret = acpi_bus_get_device(handle, &device);
		if (ret) {
			pr_warn("DW HSU: no acpi entry\n");
			return false;
		}
		hid = acpi_device_hid(device);
		if (!strncmp(hid, "INTL9C60", strlen(hid))) {
			acpi_status status;
			unsigned long long tmp;
			status = acpi_evaluate_integer(handle,
					"_UID", NULL, &tmp);
			if (!ACPI_FAILURE(status) && (tmp == 1))
				return true;
		}
		if (!strncmp(hid, "80862286", strlen(hid))) {
			return true;
		}

#endif
		return false;
	}
}

/* the RX/TX buffer init should be a common stuff */
static int dw_dma_init(struct uart_hsu_port *up)
{
	struct dw_dma_priv *dw_dma;
	struct intel_mid_dma_slave *rxs, *txs;
	dma_cap_mask_t mask;
	int ret = 0;

	dw_dma = kzalloc(sizeof(*dw_dma), GFP_KERNEL);
	if (!dw_dma) {
		pr_warn("DW HSU: Can't alloc memory for dw_dm_priv\n");
		return -1;
	}

	up->dma_priv = dw_dma;

	/*
	 * Get pci device for DMA controller, currently it could only
	 * be the DMA controller of baytrail
	 */
	dw_dma->dmac = pci_get_device(PCI_VENDOR_ID_INTEL, 0x0f06, NULL);
	if (!dw_dma->dmac) {
		/* still have chance to get from ACPI dev */
		pr_warn("DW HSU: Can't find LPIO1 DMA controller by PCI, try ACPI\n");
	}

	ret = dma_init_common(up);
	if (ret)
		return ret;

	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);

	/* 1. Init rx channel */
	dw_dma->rxchan = dma_request_channel(mask, dw_dma_chan_filter, dw_dma);
	if (!dw_dma->rxchan)
		goto err_exit;
	rxs = &dw_dma->rxs;
	rxs->dma_slave.direction = DMA_FROM_DEVICE;
	rxs->hs_mode = LNW_DMA_HW_HS;
	rxs->cfg_mode = LNW_DMA_PER_TO_MEM;
	rxs->dma_slave.dst_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;

	/* These are fixed HW info from Baytrail datasheet */
	if (up->index == 0)
		rxs->device_instance = 3;
	else
		rxs->device_instance = 5;
	dw_dma->rxchan->private = rxs;

	/* 2. Init tx channel */
	dw_dma->txchan = dma_request_channel(mask, dw_dma_chan_filter, dw_dma);
	if (!dw_dma->txchan)
		goto free_rxchan;

	txs = &dw_dma->txs;
	txs->dma_slave.direction = DMA_TO_DEVICE;
	txs->hs_mode = LNW_DMA_HW_HS;
	txs->cfg_mode = LNW_DMA_MEM_TO_PER;
	txs->dma_slave.src_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
	if (up->index == 0)
		txs->device_instance = 2;
	else
		txs->device_instance = 4;
	dw_dma->txchan->private = txs;

	/* TX/RX reg share the same addr */
	dw_dma->dma_addr = up->port.mapbase + UART_RX;

	pm_qos_add_request(&up->qos, PM_QOS_CPU_DMA_LATENCY,
			PM_QOS_DEFAULT_VALUE);

	dw_dma->up = up;
	up->dma_inited = 1;
	return 0;

free_rxchan:
	dma_release_channel(dw_dma->rxchan);
err_exit:
	return -1;

}

static int dw_dma_suspend(struct uart_hsu_port *up)
{
	struct dw_dma_priv *dw_dma = up->dma_priv;
	struct dma_chan *txchan;
	struct dma_chan *rxchan;

	if (!up->dma_inited)
		return 0;

	txchan = dw_dma->txchan;
	rxchan = dw_dma->rxchan;

	if (test_bit(flag_rx_on, &up->flags) ||
		test_bit(flag_rx_pending, &up->flags)) {
		dev_warn(up->dev, "ignore suspend for rx dma is running\n");
		return -1;
	}

	txchan->device->device_control(txchan, DMA_TERMINATE_ALL, 0);
	rxchan->device->device_control(rxchan, DMA_TERMINATE_ALL, 0);

	txchan->device->device_control(txchan, DMA_PAUSE, 0);
	rxchan->device->device_control(rxchan, DMA_PAUSE, 0);
	pm_qos_update_request(&up->qos, PM_QOS_DEFAULT_VALUE);
	return 0;
}

static int dw_dma_resume(struct uart_hsu_port *up)
{
	struct dw_dma_priv *dw_dma = up->dma_priv;
	struct dma_chan *txchan;
	struct dma_chan *rxchan;

	if (!up->dma_inited)
		return 0;

	txchan = dw_dma->txchan;
	rxchan = dw_dma->rxchan;

	rxchan->device->device_control(rxchan, DMA_RESUME, 0);
	txchan->device->device_control(txchan, DMA_RESUME, 0);
	pm_qos_update_request(&up->qos, CSTATE_EXIT_LATENCY_C2);
	return 0;
}


static int dw_dma_exit(struct uart_hsu_port *up)
{
	struct dw_dma_priv *dw_dma = up->dma_priv;
	struct dma_chan *txchan = dw_dma->txchan;
	struct dma_chan *rxchan = dw_dma->rxchan;

	pm_qos_remove_request(&up->qos);
	txchan->device->device_control(txchan, DMA_TERMINATE_ALL, 0);
	rxchan->device->device_control(rxchan, DMA_TERMINATE_ALL, 0);
	dma_release_channel(dw_dma->txchan);
	dma_release_channel(dw_dma->rxchan);

	dma_exit_common(up);

	kfree(dw_dma);

	up->dma_inited = 0;
	up->dma_priv = NULL;
	return 0;
}

static void dw_dma_tx_done(void *arg)
{
	struct dw_dma_priv *dw_dma = arg;
	struct uart_hsu_port *up = dw_dma->up;
	struct circ_buf *xmit = &up->port.state->xmit;
	struct hsu_dma_buffer *dbuf = &up->txbuf;
	unsigned long flags;
	int count = 0;

	count = intel_dma_get_src_addr(dw_dma->txchan) - dbuf->dma_addr
			- xmit->tail;

	/* Update the circ buf info */
	xmit->tail += dbuf->ofs;
	xmit->tail &= UART_XMIT_SIZE - 1;
	up->port.icount.tx += dbuf->ofs;

	dbuf->ofs = 0;

	clear_bit(flag_tx_on, &up->flags);

	if (!uart_circ_empty(xmit) && !uart_tx_stopped(&up->port)) {
		spin_lock_irqsave(&up->port.lock, flags);
		serial_sched_cmd(up, qcmd_start_tx);
		spin_unlock_irqrestore(&up->port.lock, flags);
	}

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(&up->port);
}

static void dw_dma_start_tx(struct uart_hsu_port *up)
{
	struct dw_dma_priv *dw_dma = up->dma_priv;
	struct dma_async_tx_descriptor *txdesc = NULL;
	struct dma_chan *txchan;
	struct dma_slave_config *txconf;
	struct hsu_dma_buffer *dbuf = &up->txbuf;
	struct circ_buf *xmit = &up->port.state->xmit;
	int count;
	enum dma_ctrl_flags		flag;

	txchan = dw_dma->txchan;
	txconf = &dw_dma->txs.dma_slave;

	if (uart_circ_empty(xmit) || uart_tx_stopped(&up->port)) {
		if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
			uart_write_wakeup(&up->port);
		return;
	}

	/*
	 * Need to check if FCR is set, better to be set only once when
	 * use_dma == 1
	 */

	set_bit(flag_tx_on, &up->flags);
	count = CIRC_CNT_TO_END(xmit->head, xmit->tail, UART_XMIT_SIZE);

	if (count >= 2000)
		count = 2000;

	dbuf->ofs = count;


	if (!count) {
		pr_err("we see a case of TX Len == 0!!!\n\n");
		dump_stack();
		clear_bit(flag_tx_on, &up->flags);
		return;
	}

	/* 2. Prepare the TX dma transfer */
	txconf->direction = DMA_TO_DEVICE;
	txconf->dst_addr = dw_dma->dma_addr;
	txconf->src_maxburst = LNW_DMA_MSIZE_8;
	txconf->dst_maxburst = LNW_DMA_MSIZE_8;
	txconf->src_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
	txconf->dst_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;

	txchan->device->device_control(txchan, DMA_SLAVE_CONFIG,
				       (unsigned long) txconf);

	dma_sync_single_for_device(up->port.dev,
					   dbuf->dma_addr,
					   dbuf->dma_size,
					   DMA_TO_DEVICE);

	flag = DMA_PREP_INTERRUPT | DMA_COMPL_SKIP_DEST_UNMAP | DMA_CTRL_ACK;

	txdesc = txchan->device->device_prep_dma_memcpy(
					txchan,		/* DMA Channel */
					dw_dma->dma_addr,	/* DAR */
					dbuf->dma_addr + xmit->tail, /* SAR */
					count,		/* Data len */
					flag);		/* Flag */
	if (!txdesc) {
		pr_warn("DW HSU: fail to prepare TX DMA operation\n");
		return;
	}

	txdesc->callback = dw_dma_tx_done;
	txdesc->callback_param = dw_dma;
	txdesc->tx_submit(txdesc);
}

static void dw_dma_stop_tx(struct uart_hsu_port *up)
{
	struct dw_dma_priv *dw_dma = up->dma_priv;
	struct dma_chan *txchan = dw_dma->txchan;
	struct hsu_dma_buffer *dbuf = &up->txbuf;
	int ret;
	int count;

	if (!test_bit(flag_tx_on, &up->flags))
		return;

	count = intel_dma_get_src_addr(dw_dma->txchan) - dbuf->dma_addr;

	/* ? this may be sleepable */
	ret = txchan->device->device_control(txchan, DMA_TERMINATE_ALL, 0);
	if (ret)
		dev_warn(up->dev, "Fail to stop DMA RX channel!\n");
}

static void dw_dma_rx_done(void *arg)
{
	struct dw_dma_priv *dw_dma = arg;
	struct uart_hsu_port *up = dw_dma->up;
	struct hsu_dma_buffer *dbuf = &up->rxbuf;
	struct uart_port *port = &up->port;
	struct tty_struct *tty;
	struct tty_port *tport = &port->state->port;
	int count;
	unsigned long flags;

	tty = tty_port_tty_get(&up->port.state->port);
	if (!tty)
		return;

	dma_sync_single_for_cpu(port->dev, dbuf->dma_addr,
			dbuf->dma_size, DMA_FROM_DEVICE);

	count = dbuf->ofs;
	tty_insert_flip_string(tport, dbuf->buf, count);
	port->icount.rx += count;

	/* Do we really need it for x86? */
	dma_sync_single_for_device(up->port.dev, dbuf->dma_addr,
			dbuf->dma_size, DMA_FROM_DEVICE);

	tty_flip_buffer_push(tport);
	tty_kref_put(tty);

	clear_bit(flag_rx_on, &up->flags);

	spin_lock_irqsave(&up->port.lock, flags);
	if (test_bit(flag_rx_pending, &up->flags))
		serial_sched_cmd(up, qcmd_start_rx);
	spin_unlock_irqrestore(&up->port.lock, flags);

}


static void dw_dma_start_rx(struct uart_hsu_port *up)
{
	struct dma_async_tx_descriptor *rxdesc = NULL;
	struct dw_dma_priv *dw_dma = up->dma_priv;
	struct hsu_dma_buffer *dbuf = &up->rxbuf;
	struct dma_chan *rxchan = dw_dma->rxchan;
	struct dma_slave_config *rxconf = &dw_dma->rxs.dma_slave;
	enum dma_ctrl_flags flag;

	if (test_and_set_bit(flag_rx_on, &up->flags)) {
		set_bit(flag_rx_pending, &up->flags);
		return;
	}

	dbuf->ofs = 2048 - 64;

	/* Prepare the RX dma transfer */
	rxconf->direction = DMA_FROM_DEVICE;
	rxconf->src_addr = dw_dma->dma_addr;
	rxconf->dst_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
	rxconf->src_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;

	/* feng: better to calculate a best size */
	rxconf->src_maxburst = LNW_DMA_MSIZE_8;
	rxconf->dst_maxburst = LNW_DMA_MSIZE_8;

	rxchan->device->device_control(rxchan, DMA_SLAVE_CONFIG,
				       (unsigned long) rxconf);
	flag = DMA_PREP_INTERRUPT | DMA_COMPL_SKIP_DEST_UNMAP | DMA_CTRL_ACK;
	rxdesc = rxchan->device->device_prep_dma_memcpy(
					rxchan,			/* DMA chan */
					dbuf->dma_addr,		/* DAR */
					dw_dma->dma_addr,	/* SAR */
					dbuf->ofs,		/* data len */
					flag);
	if (!rxdesc) {
		pr_warn("DW HSU: fail to prepare TX DMA operation\n");
		return;
	}

	rxdesc->callback = dw_dma_rx_done;
	rxdesc->callback_param = dw_dma;
	rxdesc->tx_submit(rxdesc);
}

static void dw_dma_stop_rx(struct uart_hsu_port *up)
{
	struct dw_dma_priv *dw_dma = up->dma_priv;
	struct hsu_dma_buffer *dbuf = &up->rxbuf;
	struct dma_chan *rxchan = dw_dma->rxchan;
	int count, ret;
	struct uart_port *port = &up->port;
	struct tty_struct *tty;
	struct tty_port *tport = &port->state->port;

	if (!test_bit(flag_rx_on, &up->flags)) {
		clear_bit(flag_rx_pending, &up->flags);
		return;
	}

	ret = rxchan->device->device_control(rxchan, DMA_TERMINATE_ALL, 0);
	if (ret) {
		WARN(1, "DMA TERMINATE of TX returns error\n");
		return;
	}

	tty = tty_port_tty_get(&up->port.state->port);
	if (!tty)
		return;

	count = intel_dma_get_dst_addr(rxchan) - dbuf->dma_addr;
	if (!count)
		goto exit;

	dma_sync_single_for_cpu(port->dev, dbuf->dma_addr,
		dbuf->dma_size, DMA_FROM_DEVICE);

	tty_insert_flip_string(tport, dbuf->buf, count);
	port->icount.rx += count;

	/* Do we really need it for x86? */
	dma_sync_single_for_device(up->port.dev, dbuf->dma_addr,
			dbuf->dma_size, DMA_FROM_DEVICE);

	tty_flip_buffer_push(tport);

exit:
	tty_kref_put(tty);
	clear_bit(flag_rx_on, &up->flags);
	clear_bit(flag_rx_pending, &up->flags);
}

struct hsu_dma_ops dw_dma_ops = {
	.init =		dw_dma_init,
	.exit =		dw_dma_exit,
	.suspend =	dw_dma_suspend,
	.resume	=	dw_dma_resume,
	.start_tx =	dw_dma_start_tx,
	.stop_tx =	dw_dma_stop_tx,
	.start_rx =	dw_dma_start_rx,
	.stop_rx =	dw_dma_stop_rx,
};

struct hsu_dma_ops *pdw_dma_ops = &dw_dma_ops;

#else
struct hsu_dma_ops *pdw_dma_ops = NULL;
#endif

/* Intel DMA ops */

/* The buffer is already cache coherent */
void hsu_dma_start_rx_chan(struct hsu_dma_chan *rxc,
			struct hsu_dma_buffer *dbuf)
{
	dbuf->ofs = 0;

	chan_writel(rxc, HSU_CH_BSR, HSU_DMA_BSR);
	chan_writel(rxc, HSU_CH_MOTSR, HSU_DMA_MOTSR);

	chan_writel(rxc, HSU_CH_D0SAR, dbuf->dma_addr);
	chan_writel(rxc, HSU_CH_D0TSR, dbuf->dma_size);
	chan_writel(rxc, HSU_CH_DCR, 0x1 | (0x1 << 8)
					 | (0x1 << 16)
					 | (0x1 << 24)	/* timeout, Errata 1 */
					 );
	chan_writel(rxc, HSU_CH_CR, 0x3);
}

static int intel_dma_init(struct uart_hsu_port *up)
{
	int ret;

	clear_bit(flag_tx_on, &up->flags);

	ret = dma_init_common(up);
	if (ret)
		return ret;

	/* This should not be changed all around */
	chan_writel(up->txc, HSU_CH_BSR, HSU_DMA_BSR);
	chan_writel(up->txc, HSU_CH_MOTSR, HSU_DMA_MOTSR);

	/* Start the RX channel right now */
	hsu_dma_start_rx_chan(up->rxc, &up->rxbuf);

	up->dma_inited = 1;
	return 0;
}

static int intel_dma_exit(struct uart_hsu_port *up)
{
	chan_writel(up->txc, HSU_CH_CR, 0x0);
	clear_bit(flag_tx_on, &up->flags);
	chan_writel(up->rxc, HSU_CH_CR, 0x2);
	dma_exit_common(up);

	up->dma_inited = 0;
	return 0;
}


static void intel_dma_start_tx(struct uart_hsu_port *up)
{
	struct circ_buf *xmit = &up->port.state->xmit;
	struct hsu_dma_buffer *dbuf = &up->txbuf;
	unsigned long flags;
	int count;

	spin_lock_irqsave(&up->port.lock, flags);
	chan_writel(up->txc, HSU_CH_CR, 0x0);
	while (chan_readl(up->txc, HSU_CH_CR))
		cpu_relax();
	clear_bit(flag_tx_on, &up->flags);
	if (dbuf->ofs) {
		u32 real = chan_readl(up->txc, HSU_CH_D0SAR) - up->tx_addr;

		/* we found in flow control case, TX irq came without sending
		 * all TX buffer
		 */
		if (real < dbuf->ofs)
			dbuf->ofs = real; /* adjust to real chars sent */

		/* Update the circ buf info */
		xmit->tail += dbuf->ofs;
		xmit->tail &= UART_XMIT_SIZE - 1;

		up->port.icount.tx += dbuf->ofs;
		dbuf->ofs = 0;
	}

	if (!uart_circ_empty(xmit) && !uart_tx_stopped(&up->port)) {
		set_bit(flag_tx_on, &up->flags);
		dma_sync_single_for_device(up->port.dev,
					   dbuf->dma_addr,
					   dbuf->dma_size,
					   DMA_TO_DEVICE);

		count = CIRC_CNT_TO_END(xmit->head, xmit->tail, UART_XMIT_SIZE);
		dbuf->ofs = count;

		/* Reprogram the channel */
		up->tx_addr = dbuf->dma_addr + xmit->tail;
		chan_writel(up->txc, HSU_CH_D0SAR, up->tx_addr);
		chan_writel(up->txc, HSU_CH_D0TSR, count);

		/* Reenable the channel */
		chan_writel(up->txc, HSU_CH_DCR, 0x1
						 | (0x1 << 8)
						 | (0x1 << 16));
		chan_writel(up->txc, HSU_CH_CR, 0x1);
	}

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(&up->port);

	spin_unlock_irqrestore(&up->port.lock, flags);
	return;
}

static void intel_dma_stop_tx(struct uart_hsu_port *up)
{
	chan_writel(up->txc, HSU_CH_CR, 0x0);
	return;
}

static void intel_dma_start_rx(struct uart_hsu_port *up)
{
	return;
}

static void intel_dma_stop_rx(struct uart_hsu_port *up)
{
	chan_writel(up->rxc, HSU_CH_CR, 0x2);
	return;
}

static void intel_dma_context_op(struct uart_hsu_port *up, int op)
{
	if (op == context_save) {
		up->txc->cr  = chan_readl(up->txc, HSU_CH_CR);
		up->txc->dcr = chan_readl(up->txc, HSU_CH_DCR);
		up->txc->sar = chan_readl(up->txc, HSU_CH_D0SAR);
		up->txc->tsr = chan_readl(up->txc, HSU_CH_D0TSR);

		up->rxc->cr  = chan_readl(up->rxc, HSU_CH_CR);
		up->rxc->dcr = chan_readl(up->rxc, HSU_CH_DCR);
		up->rxc->sar = chan_readl(up->rxc, HSU_CH_D0SAR);
		up->rxc->tsr = chan_readl(up->rxc, HSU_CH_D0TSR);
	} else {
		chan_writel(up->txc, HSU_CH_DCR, up->txc->dcr);
		chan_writel(up->txc, HSU_CH_D0SAR, up->txc->sar);
		chan_writel(up->txc, HSU_CH_D0TSR, up->txc->tsr);
		chan_writel(up->txc, HSU_CH_BSR, HSU_DMA_BSR);
		chan_writel(up->txc, HSU_CH_MOTSR, HSU_DMA_MOTSR);

		chan_writel(up->rxc, HSU_CH_DCR, up->rxc->dcr);
		chan_writel(up->rxc, HSU_CH_D0SAR, up->rxc->sar);
		chan_writel(up->rxc, HSU_CH_D0TSR, up->rxc->tsr);
		chan_writel(up->rxc, HSU_CH_BSR, HSU_DMA_BSR);
		chan_writel(up->rxc, HSU_CH_MOTSR, HSU_DMA_MOTSR);
	}
}


static int intel_dma_resume(struct uart_hsu_port *up)
{
	chan_writel(up->rxc, HSU_CH_CR, up->rxc_chcr_save);
	return 0;
}

static int intel_dma_suspend(struct uart_hsu_port *up)
{
	int loop = 100000;
	struct hsu_dma_chan *chan = up->rxc;

	up->rxc_chcr_save = chan_readl(up->rxc, HSU_CH_CR);

	if (test_bit(flag_startup, &up->flags)
			&& serial_in(up, UART_FOR) & 0x7F) {
		dev_err(up->dev, "ignore suspend for rx fifo\n");
		return -1;
	}

	if (chan_readl(up->txc, HSU_CH_CR)) {
		dev_info(up->dev, "ignore suspend for tx dma\n");
		return -1;
	}

	chan_writel(up->rxc, HSU_CH_CR, 0x2);
	while (--loop) {
		if (chan_readl(up->rxc, HSU_CH_CR) == 0x2)
			break;
		cpu_relax();
	}

	if (!loop) {
		dev_err(up->dev, "Can't stop rx dma\n");
		return -1;
	}

	if (chan_readl(chan, HSU_CH_D0SAR) - up->rxbuf.dma_addr) {
		dev_err(up->dev, "ignore suspend for dma pointer\n");
		return -1;
	}

	return 0;
}

struct hsu_dma_ops intel_dma_ops = {
	.init =		intel_dma_init,
	.exit =		intel_dma_exit,
	.suspend =	intel_dma_suspend,
	.resume	=	intel_dma_resume,
	.start_tx =	intel_dma_start_tx,
	.stop_tx =	intel_dma_stop_tx,
	.start_rx =	intel_dma_start_rx,
	.stop_rx =	intel_dma_stop_rx,
	.context_op =	intel_dma_context_op,
};


