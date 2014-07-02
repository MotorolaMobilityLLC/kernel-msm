/*
 * mfd_core.c: driver core for High Speed UART device of Intel Medfield platform
 *
 * Refer pxa.c, 8250.c and some other drivers in drivers/serial/
 *
 * (C) Copyright 2010 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

/* Notes:
 * 1. DMA channel allocation: 0/1 channel are assigned to port 0,
 *    2/3 chan to port 1, 4/5 chan to port 3. Even number chans
 *    are used for RX, odd chans for TX
 *
 * 2. The RI/DSR/DCD/DTR are not pinned out, DCD & DSR are always
 *    asserted, only when the HW is reset the DDCD and DDSR will
 *    be triggered
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/console.h>
#include <linux/sysrq.h>
#include <linux/slab.h>
#include <linux/circ_buf.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/pm_runtime.h>
#include <linux/irq.h>
#include <linux/intel_mid_pm.h>
#include <linux/pm_qos.h>

#define CREATE_TRACE_POINTS
#include "mfd.h"

static int hsu_dma_enable = 0xff;
module_param(hsu_dma_enable, int, 0);
MODULE_PARM_DESC(hsu_dma_enable,
		 "It is a bitmap to set working mode, if bit[x] is 1, then port[x] will work in DMA mode, otherwise in PIO mode.");

static struct hsu_port hsu;
static struct hsu_port *phsu = &hsu;
static struct uart_driver serial_hsu_reg;
static struct hsu_port_cfg *hsu_port_func_cfg;

static void serial_hsu_command(struct uart_hsu_port *up);

int hsu_register_board_info(void *inf)
{
	hsu_port_func_cfg = inf;
	return 0;
}

static inline int check_qcmd(struct uart_hsu_port *up, char *cmd)
{
	struct circ_buf *circ = &up->qcirc;
	char *buf;

	buf = circ->buf + circ->tail;
	*cmd = *buf;
	return CIRC_CNT(circ->head, circ->tail, HSU_Q_MAX);
}

static inline void insert_qcmd(struct uart_hsu_port *up, char cmd)
{
	struct circ_buf *circ = &up->qcirc;
	char *buf;
	char last_cmd;

	trace_hsu_cmd_insert(up->index, cmd);
	if (check_qcmd(up, &last_cmd) && last_cmd == cmd &&
		cmd != qcmd_enable_irq && cmd != qcmd_port_irq &&
                cmd != qcmd_dma_irq)
		return;
	trace_hsu_cmd_add(up->index, cmd);
	up->qcmd_num++;
	buf = circ->buf + circ->head;
	if (CIRC_SPACE(circ->head, circ->tail, HSU_Q_MAX) < 1)
		*buf = qcmd_overflow;
	else {
		*buf = cmd;
		circ->head++;
		if (circ->head == HSU_Q_MAX)
			circ->head = 0;
	}
}

static inline int get_qcmd(struct uart_hsu_port *up, char *cmd)
{
	struct circ_buf *circ = &up->qcirc;
	char *buf;

	if (!CIRC_CNT(circ->head, circ->tail, HSU_Q_MAX))
		return 0;
	buf = circ->buf + circ->tail;
	*cmd = *buf;
	circ->tail++;
	if (circ->tail == HSU_Q_MAX)
		circ->tail = 0;
	up->qcmd_done++;
	return 1;
}

static inline void cl_put_char(struct uart_hsu_port *up, char c)
{
	struct circ_buf *circ = &up->cl_circ;
	char *buf;
	unsigned long flags;

	spin_lock_irqsave(&up->cl_lock, flags);
	buf = circ->buf + circ->head;
	if (CIRC_SPACE(circ->head, circ->tail, HSU_CL_BUF_LEN) > 1) {
		*buf = c;
		circ->head++;
		if (circ->head == HSU_CL_BUF_LEN)
			circ->head = 0;
	}
	spin_unlock_irqrestore(&up->cl_lock, flags);
}

static inline int cl_get_char(struct uart_hsu_port *up, char *c)
{
	struct circ_buf *circ = &up->cl_circ;
	char *buf;
	unsigned long flags;

	spin_lock_irqsave(&up->cl_lock, flags);
	if (!CIRC_CNT(circ->head, circ->tail, HSU_CL_BUF_LEN)) {
		spin_unlock_irqrestore(&up->cl_lock, flags);
		return 0;
	}
	buf = circ->buf + circ->tail;
	*c = *buf;
	circ->tail++;
	if (circ->tail == HSU_CL_BUF_LEN)
		circ->tail = 0;
	spin_unlock_irqrestore(&up->cl_lock, flags);
	return 1;
}



void serial_sched_cmd(struct uart_hsu_port *up, char cmd)
{
	pm_runtime_get(up->dev);
	insert_qcmd(up, cmd);
	if (test_bit(flag_cmd_on, &up->flags)) {
		if (up->use_dma)
			tasklet_schedule(&up->tasklet);
		else
			queue_work(up->workqueue, &up->work);
	}
	pm_runtime_put(up->dev);
}

static inline void serial_sched_sync(struct uart_hsu_port *up)
{
	mutex_lock(&up->q_mutex);
	if (up->q_start > 0) {
		if (up->use_dma) {
			tasklet_disable(&up->tasklet);
			serial_hsu_command(up);
			tasklet_enable(&up->tasklet);
		} else {
			flush_workqueue(up->workqueue);
		}
	}
	mutex_unlock(&up->q_mutex);
}

static inline void serial_sched_start(struct uart_hsu_port *up)
{
	unsigned long flags;

	mutex_lock(&up->q_mutex);
	up->q_start++;
	if (up->q_start == 1) {
		clear_bit(flag_cmd_off, &up->flags);
		spin_lock_irqsave(&up->port.lock, flags);
		set_bit(flag_cmd_on, &up->flags);
		spin_unlock_irqrestore(&up->port.lock, flags);
		if (up->use_dma)
			tasklet_schedule(&up->tasklet);
		else
			queue_work(up->workqueue, &up->work);
	}
	mutex_unlock(&up->q_mutex);
}

static inline void serial_sched_stop(struct uart_hsu_port *up)
{
	unsigned long flags;

	mutex_lock(&up->q_mutex);
	up->q_start--;
	if (up->q_start == 0) {
		spin_lock_irqsave(&up->port.lock, flags);
		clear_bit(flag_cmd_on, &up->flags);
		insert_qcmd(up, qcmd_cmd_off);
		spin_unlock_irqrestore(&up->port.lock, flags);
		if (up->use_dma) {
			tasklet_schedule(&up->tasklet);
			while (!test_bit(flag_cmd_off, &up->flags))
				cpu_relax();
		} else {
			queue_work(up->workqueue, &up->work);
			flush_workqueue(up->workqueue);
		}
	}
	mutex_unlock(&up->q_mutex);
}

static void serial_set_alt(int index)
{
	struct uart_hsu_port *up = phsu->port + index;
	struct hsu_dma_chan *txc = up->txc;
	struct hsu_dma_chan *rxc = up->rxc;
	struct hsu_port_cfg *cfg = phsu->configs[index];

	if (test_bit(flag_set_alt, &up->flags))
		return;

	trace_hsu_func_start(up->index, __func__);
	pm_runtime_get_sync(up->dev);
	disable_irq(up->port.irq);
	disable_irq(up->dma_irq);
	serial_sched_stop(up);
	if (up->use_dma && up->hw_type == hsu_intel) {
		txc->uport = up;
		rxc->uport = up;
	}
	dev_set_drvdata(up->dev, up);
	if (cfg->hw_set_alt)
		cfg->hw_set_alt(index);
	if (cfg->hw_set_rts)
		cfg->hw_set_rts(up->index, 0);
	set_bit(flag_set_alt, &up->flags);
	serial_sched_start(up);
	enable_irq(up->dma_irq);
	enable_irq(up->port.irq);
	pm_runtime_put(up->dev);
	trace_hsu_func_end(up->index, __func__, "");
}

static void serial_clear_alt(int index)
{
	struct uart_hsu_port *up = phsu->port + index;
	struct hsu_port_cfg *cfg = phsu->configs[index];

	if (!test_bit(flag_set_alt, &up->flags))
		return;

	pm_runtime_get_sync(up->dev);
	disable_irq(up->port.irq);
	disable_irq(up->dma_irq);
	serial_sched_stop(up);
	if (cfg->hw_set_rts)
		cfg->hw_set_rts(up->index, 1);
	clear_bit(flag_set_alt, &up->flags);
	serial_sched_start(up);
	enable_irq(up->dma_irq);
	enable_irq(up->port.irq);
	pm_runtime_put(up->dev);
}

#ifdef CONFIG_DEBUG_FS

#define HSU_DBGFS_BUFSIZE	8192

static int hsu_show_regs_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t port_show_regs(struct file *file, char __user *user_buf,
				size_t count, loff_t *ppos)
{
	struct uart_hsu_port *up = file->private_data;
	char *buf;
	u32 len = 0;
	ssize_t ret;

	buf = kzalloc(HSU_DBGFS_BUFSIZE, GFP_KERNEL);
	if (!buf)
		return 0;

	pm_runtime_get_sync(up->dev);
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"MFD HSU port[%d] regs:\n", up->index);

	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"=================================\n");
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"IER: \t\t0x%08x\n", serial_in(up, UART_IER));
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"IIR: \t\t0x%08x\n", serial_in(up, UART_IIR));
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"LCR: \t\t0x%08x\n", serial_in(up, UART_LCR));
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"MCR: \t\t0x%08x\n", serial_in(up, UART_MCR));
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"LSR: \t\t0x%08x\n", serial_in(up, UART_LSR));
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"MSR: \t\t0x%08x\n", serial_in(up, UART_MSR));
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"FOR: \t\t0x%08x\n", serial_in(up, UART_FOR));
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"PS: \t\t0x%08x\n", serial_in(up, UART_PS));
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"MUL: \t\t0x%08x\n", serial_in(up, UART_MUL));
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"DIV: \t\t0x%08x\n", serial_in(up, UART_DIV));
	pm_runtime_put(up->dev);

	if (len > HSU_DBGFS_BUFSIZE)
		len = HSU_DBGFS_BUFSIZE;

	ret =  simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);
	return ret;
}

static ssize_t dma_show_regs(struct file *file, char __user *user_buf,
				size_t count, loff_t *ppos)
{
	struct hsu_dma_chan *chan = file->private_data;
	char *buf;
	u32 len = 0;
	ssize_t ret;

	buf = kzalloc(HSU_DBGFS_BUFSIZE, GFP_KERNEL);
	if (!buf)
		return 0;

	pm_runtime_get_sync(chan->uport->dev);
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"MFD HSU DMA channel [%d] regs:\n", chan->id);

	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"=================================\n");
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"CR: \t\t0x%08x\n", chan_readl(chan, HSU_CH_CR));
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"DCR: \t\t0x%08x\n", chan_readl(chan, HSU_CH_DCR));
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"BSR: \t\t0x%08x\n", chan_readl(chan, HSU_CH_BSR));
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"MOTSR: \t\t0x%08x\n", chan_readl(chan, HSU_CH_MOTSR));
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"D0SAR: \t\t0x%08x\n", chan_readl(chan, HSU_CH_D0SAR));
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"D0TSR: \t\t0x%08x\n", chan_readl(chan, HSU_CH_D0TSR));
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"D0SAR: \t\t0x%08x\n", chan_readl(chan, HSU_CH_D1SAR));
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"D0TSR: \t\t0x%08x\n", chan_readl(chan, HSU_CH_D1TSR));
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"D0SAR: \t\t0x%08x\n", chan_readl(chan, HSU_CH_D2SAR));
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"D0TSR: \t\t0x%08x\n", chan_readl(chan, HSU_CH_D2TSR));
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"D0SAR: \t\t0x%08x\n", chan_readl(chan, HSU_CH_D3SAR));
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"D0TSR: \t\t0x%08x\n", chan_readl(chan, HSU_CH_D3TSR));
	pm_runtime_put(chan->uport->dev);

	if (len > HSU_DBGFS_BUFSIZE)
		len = HSU_DBGFS_BUFSIZE;

	ret =  simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);
	return ret;
}

static ssize_t hsu_dump_show(struct file *file, char __user *user_buf,
				size_t count, loff_t *ppos)
{
	struct uart_hsu_port *up;
	struct hsu_port_cfg *cfg;
	char *buf;
	char cmd;
	int i;
	u32 len = 0;
	ssize_t ret;
	struct irq_desc *dma_irqdesc = irq_to_desc(phsu->dma_irq);
	struct irq_desc *port_irqdesc;
	struct circ_buf *xmit;

	buf = kzalloc(HSU_DBGFS_BUFSIZE, GFP_KERNEL);
	if (!buf)
		return 0;

	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
		"HSU status dump:\n");
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
		"\tdma irq (>0: disable): %d\n",
		dma_irqdesc ? dma_irqdesc->depth : 0);
	for (i = 0; i < phsu->port_num; i++) {
		up = phsu->port + i;
		cfg = hsu_port_func_cfg + i;
		port_irqdesc = irq_to_desc(up->port.irq);
		xmit = &up->port.state->xmit;

		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"HSU port[%d] %s:\n", up->index, cfg->name);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"xmit empty[%d] xmit pending[%d]\n",
			uart_circ_empty(xmit),
			(int)uart_circ_chars_pending(xmit));
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tsuspend idle: %d\n", cfg->idle);
		if (cfg->has_alt)
			len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
				"\talt port: %d\n", cfg->alt);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
				"\tforce_suspend: %d\n", cfg->force_suspend);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
				"\tcts status: %d\n", up->cts_status);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tuse_dma: %s\n",
			up->use_dma ? "yes" : "no");
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tflag_console: %s\n",
			test_bit(flag_console, &up->flags) ? "yes" : "no");
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tflag_suspend: %s\n",
			test_bit(flag_suspend, &up->flags) ? "yes" : "no");
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tflag_active: %s\n",
			test_bit(flag_active, &up->flags) ? "yes" : "no");
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tflag_set_alt: %s\n",
			test_bit(flag_set_alt, &up->flags) ? "yes" : "no");
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tflag_startup: %s\n",
			test_bit(flag_startup, &up->flags) ? "yes" : "no");
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tqcmd q_start: %d\n", up->q_start);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tqcmd total count: %d\n", up->qcmd_num);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tqcmd done count: %d\n", up->qcmd_done);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tport irq count: %d\n", up->port_irq_num);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tport irq cmddone: %d\n", up->port_irq_cmddone);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tport irq cts: %d\n", up->port.icount.cts);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tport irq rng: %d\n", up->port.icount.rng);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tport irq dsr: %d\n", up->port.icount.dsr);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tport irq no irq pending: %d\n",
			up->port_irq_pio_no_irq_pend);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tport irq no alt: %d\n",
			up->port_irq_no_alt);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tport irq no startup: %d\n",
			up->port_irq_no_startup);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tport irq pio rx error: %d\n",
			up->port_irq_pio_rx_err);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tport irq pio rx available: %d\n",
			up->port_irq_pio_rx_avb);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tport irq pio rx fifo timeout: %d\n",
			up->port_irq_pio_rx_timeout);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tport irq pio tx request: %d\n",
			up->port_irq_pio_tx_req);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tdma invalid irq count: %d\n",
			up->dma_invalid_irq_num);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tdma irq count: %d\n", up->dma_irq_num);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tdma irq cmddone: %d\n", up->dma_irq_cmddone);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tdma tx irq cmddone: %d\n",
			up->dma_tx_irq_cmddone);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tport&dma rx irq cmddone: %d\n",
			up->dma_rx_irq_cmddone);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tport&dma rx timeout irq cmddone: %d\n",
			up->dma_rx_tmt_irq_cmddone);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\ttasklet done: %d\n", up->tasklet_done);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tworkq done: %d\n", up->workq_done);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tqcmd pending count: %d\n", check_qcmd(up, &cmd));
		if (check_qcmd(up, &cmd))
			len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
				"\tqcmd pending next: %d\n", cmd);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tin tasklet: %d\n", up->in_tasklet);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tin workq: %d\n", up->in_workq);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tport irq (>0: disable): %d\n",
			port_irqdesc ? port_irqdesc->depth : 0);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tbyte delay: %d\n", up->byte_delay);
	}
	if (len > HSU_DBGFS_BUFSIZE)
		len = HSU_DBGFS_BUFSIZE;

	ret =  simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);
	return ret;
}


static const struct file_operations port_regs_ops = {
	.owner		= THIS_MODULE,
	.open		= hsu_show_regs_open,
	.read		= port_show_regs,
	.llseek		= default_llseek,
};

static const struct file_operations dma_regs_ops = {
	.owner		= THIS_MODULE,
	.open		= hsu_show_regs_open,
	.read		= dma_show_regs,
	.llseek		= default_llseek,
};

static const struct file_operations hsu_dump_ops = {
	.owner		= THIS_MODULE,
	.read		= hsu_dump_show,
	.llseek		= default_llseek,
};

static int hsu_debugfs_init(struct hsu_port *hsu)
{
	int i;
	char name[32];

	hsu->debugfs = debugfs_create_dir("hsu", NULL);
	if (!hsu->debugfs)
		return -ENOMEM;

	for (i = 0; i < 3; i++) {
		snprintf(name, sizeof(name), "port_%d_regs", i);
		debugfs_create_file(name, S_IRUSR,
			hsu->debugfs, (void *)(&hsu->port[i]), &port_regs_ops);
	}

	for (i = 0; i < 6; i++) {
		snprintf(name, sizeof(name), "dma_chan_%d_regs", i);
		debugfs_create_file(name, S_IRUSR,
			hsu->debugfs, (void *)&hsu->chans[i], &dma_regs_ops);
	}

	snprintf(name, sizeof(name), "dump_status");
	debugfs_create_file(name, S_IRUSR,
		hsu->debugfs, NULL, &hsu_dump_ops);

	return 0;
}

static void hsu_debugfs_remove(struct hsu_port *hsu)
{
	if (hsu->debugfs)
		debugfs_remove_recursive(hsu->debugfs);
}

#else
static inline int hsu_debugfs_init(struct hsu_port *hsu)
{
	return 0;
}

static inline void hsu_debugfs_remove(struct hsu_port *hsu)
{
}
#endif /* CONFIG_DEBUG_FS */

static void serial_hsu_enable_ms(struct uart_port *port)
{
	struct uart_hsu_port *up =
		container_of(port, struct uart_hsu_port, port);

	trace_hsu_func_start(up->index, __func__);
	up->ier |= UART_IER_MSI;
	serial_sched_cmd(up, qcmd_set_ier);
	trace_hsu_func_end(up->index, __func__, "");
}

/* Protected by spin_lock_irqsave(port->lock) */
static void serial_hsu_start_tx(struct uart_port *port)
{
	struct uart_hsu_port *up =
		container_of(port, struct uart_hsu_port, port);

	trace_hsu_func_start(up->index, __func__);
	serial_sched_cmd(up, qcmd_start_tx);
	trace_hsu_func_end(up->index, __func__, "");
}

static void serial_hsu_stop_tx(struct uart_port *port)
{
	struct uart_hsu_port *up =
		container_of(port, struct uart_hsu_port, port);

	trace_hsu_func_start(up->index, __func__);
	serial_sched_cmd(up, qcmd_stop_tx);
	trace_hsu_func_end(up->index, __func__, "");
}

static void hsu_stop_tx(struct uart_port *port)
{
	struct uart_hsu_port *up =
		container_of(port, struct uart_hsu_port, port);

	trace_hsu_func_start(up->index, __func__);
	serial_sched_cmd(up, qcmd_stop_tx);
	trace_hsu_func_end(up->index, __func__, "");
}

/* This is always called in spinlock protected mode, so
 * modify timeout timer is safe here */
void intel_dma_do_rx(struct uart_hsu_port *up, u32 int_sts)
{
	struct hsu_dma_buffer *dbuf = &up->rxbuf;
	struct hsu_dma_chan *chan = up->rxc;
	struct uart_port *port = &up->port;
	struct tty_struct *tty;
	struct tty_port *tport = &port->state->port;
	int count;

	trace_hsu_func_start(up->index, __func__);
	tty = tty_port_tty_get(&up->port.state->port);
	if (!tty) {
		trace_hsu_func_end(up->index, __func__, "notty");
		return;
	}

	/*
	 * First need to know how many is already transferred,
	 * then check if its a timeout DMA irq, and return
	 * the trail bytes out, push them up and reenable the
	 * channel
	 */

	/* Timeout IRQ, need wait some time, see Errata 2 */
	if (int_sts & 0xf00) {
		up->dma_rx_tmt_irq_cmddone++;
		udelay(2);
	} else
		up->dma_rx_irq_cmddone++;

	/* Stop the channel */
	chan_writel(chan, HSU_CH_CR, 0x0);

	count = chan_readl(chan, HSU_CH_D0SAR) - dbuf->dma_addr;
	if (!count) {
		/* Restart the channel before we leave */
		chan_writel(chan, HSU_CH_CR, 0x3);
		tty_kref_put(tty);
		trace_hsu_func_end(up->index, __func__, "nodata");
		return;
	}

	dma_sync_single_for_cpu(port->dev, dbuf->dma_addr,
			dbuf->dma_size, DMA_FROM_DEVICE);

	/*
	 * Head will only wrap around when we recycle
	 * the DMA buffer, and when that happens, we
	 * explicitly set tail to 0. So head will
	 * always be greater than tail.
	 */
	tty_insert_flip_string(tport, dbuf->buf, count);
	port->icount.rx += count;

	dma_sync_single_for_device(up->port.dev, dbuf->dma_addr,
			dbuf->dma_size, DMA_FROM_DEVICE);

	/* Reprogram the channel */
	chan_writel(chan, HSU_CH_D0SAR, dbuf->dma_addr);
	chan_writel(chan, HSU_CH_D0TSR, dbuf->dma_size);
	chan_writel(chan, HSU_CH_DCR, 0x1
					 | (0x1 << 8)
					 | (0x1 << 16)
					 | (0x1 << 24)	/* timeout bit, see HSU Errata 1 */
					 );
	tty_flip_buffer_push(tport);

	chan_writel(chan, HSU_CH_CR, 0x3);
	tty_kref_put(tty);
	trace_hsu_func_end(up->index, __func__, "");

}

static void serial_hsu_stop_rx(struct uart_port *port)
{
	struct uart_hsu_port *up =
		container_of(port, struct uart_hsu_port, port);

	trace_hsu_func_start(up->index, __func__);
	serial_sched_cmd(up, qcmd_stop_rx);
	trace_hsu_func_end(up->index, __func__, "");
}

static inline void receive_chars(struct uart_hsu_port *up, int *status)
{
	struct tty_struct *tty = up->port.state->port.tty;
	struct tty_port *tport = &up->port.state->port;
	unsigned int ch, flag;
	unsigned int max_count = 256;

	if (!tty)
		return;

	trace_hsu_func_start(up->index, __func__);
	do {
		ch = serial_in(up, UART_RX);
		flag = TTY_NORMAL;
		up->port.icount.rx++;

		if (unlikely(*status & (UART_LSR_BI | UART_LSR_PE |
				       UART_LSR_FE | UART_LSR_OE))) {

			dev_warn(up->dev,
				"We really rush into ERR/BI case"
				"status = 0x%02x\n", *status);
			/* For statistics only */
			if (*status & UART_LSR_BI) {
				*status &= ~(UART_LSR_FE | UART_LSR_PE);
				up->port.icount.brk++;
				/*
				 * We do the SysRQ and SAK checking
				 * here because otherwise the break
				 * may get masked by ignore_status_mask
				 * or read_status_mask.
				 */
				if (uart_handle_break(&up->port))
					goto ignore_char;
			} else if (*status & UART_LSR_PE)
				up->port.icount.parity++;
			else if (*status & UART_LSR_FE)
				up->port.icount.frame++;
			if (*status & UART_LSR_OE)
				up->port.icount.overrun++;

			/* Mask off conditions which should be ignored. */
			*status &= up->port.read_status_mask;

#ifdef CONFIG_SERIAL_MFD_HSU_EXT_CONSOLE
			if (up->port.cons &&
				up->port.cons->index == up->port.line) {
				/* Recover the break flag from console xmit */
				*status |= up->lsr_break_flag;
				up->lsr_break_flag = 0;
			}
#endif
			if (*status & UART_LSR_BI) {
				flag = TTY_BREAK;
			} else if (*status & UART_LSR_PE)
				flag = TTY_PARITY;
			else if (*status & UART_LSR_FE)
				flag = TTY_FRAME;
		}

		if (uart_handle_sysrq_char(&up->port, ch))
			goto ignore_char;

		uart_insert_char(&up->port, *status, UART_LSR_OE, ch, flag);
	ignore_char:
		*status = serial_in(up, UART_LSR);
	} while ((*status & UART_LSR_DR) && max_count--);

	tty_flip_buffer_push(tport);
	trace_hsu_func_end(up->index, __func__, "");
}

static void transmit_chars(struct uart_hsu_port *up)
{
	struct circ_buf *xmit = &up->port.state->xmit;
	unsigned long flags;
	int count;

	spin_lock_irqsave(&up->port.lock, flags);
	trace_hsu_func_start(up->index, __func__);
	if (up->port.x_char) {
		serial_out(up, UART_TX, up->port.x_char);
		up->port.icount.tx++;
		up->port.x_char = 0;
		trace_hsu_func_end(up->index, __func__, "x_char");
		goto out;
	}
	if (uart_circ_empty(xmit) || uart_tx_stopped(&up->port)) {
		hsu_stop_tx(&up->port);
		if (uart_circ_empty(xmit))
			trace_hsu_func_end(up->index, __func__, "empty");
		else
			trace_hsu_func_end(up->index, __func__, "stop");
		goto out;
	}

	/* The IRQ is for TX FIFO half-empty */
	count = up->port.fifosize / 2;

	do {
		if (uart_tx_stopped(&up->port)) {
			hsu_stop_tx(&up->port);
			break;
		}
		serial_out(up, UART_TX, xmit->buf[xmit->tail]);
		xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);

		up->port.icount.tx++;
		if (uart_circ_empty(xmit))
			break;
	} while (--count > 0);

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(&up->port);

	if (uart_circ_empty(xmit)) {
		hsu_stop_tx(&up->port);
		trace_hsu_func_end(up->index, __func__, "tx_complete");
	}
	else
		trace_hsu_func_end(up->index, __func__, "");

out:
	spin_unlock_irqrestore(&up->port.lock, flags);
}

static void check_modem_status(struct uart_hsu_port *up)
{
	struct uart_port *uport = &up->port;
	struct tty_port *port = &uport->state->port;
	struct tty_struct *tty = port->tty;
	struct hsu_port_cfg *cfg = phsu->configs[up->index];
	int status;
	int delta_msr = 0;

	trace_hsu_func_start(up->index, __func__);
	status = serial_in(up, UART_MSR);
	trace_hsu_mctrl(up->index, status);
	if (port->flags & ASYNC_CTS_FLOW && !cfg->hw_ctrl_cts) {
		if (tty->hw_stopped) {
			if (status & UART_MSR_CTS) {
				serial_sched_cmd(up, qcmd_start_tx);
				tty->hw_stopped = 0;
				up->cts_status = 0;
				uport->icount.cts++;
				delta_msr = 1;
				uart_write_wakeup(uport);
			}
		} else {
			if (!(status & UART_MSR_CTS)) {
				/* Is this automitically controlled */
				if (up->use_dma)
					up->dma_ops->stop_tx(up);
				clear_bit(flag_tx_on, &up->flags);
				tty->hw_stopped = 1;
				up->cts_status = 1;
				delta_msr = 1;
				uport->icount.cts++;
			}
		}
	}

	if ((status & UART_MSR_ANY_DELTA)) {
		if (status & UART_MSR_TERI)
			up->port.icount.rng++;
		if (status & UART_MSR_DDSR)
			up->port.icount.dsr++;
		/* We may only get DDCD when HW init and reset */
		if (status & UART_MSR_DDCD)
			uart_handle_dcd_change(&up->port,
					status & UART_MSR_DCD);
		delta_msr = 1;
	}

	if (delta_msr)
		wake_up_interruptible(&up->port.state->port.delta_msr_wait);
	trace_hsu_func_end(up->index, __func__, "");
}

static void hsu_dma_chan_handler(struct hsu_port *hsu, int index)
{
	unsigned long flags;
	struct uart_hsu_port *up = hsu->chans[index * 2].uport;
	struct hsu_dma_chan *txc = up->txc;
	struct hsu_dma_chan *rxc = up->rxc;

	up->dma_irq_num++;
	if (unlikely(!up->use_dma
		|| !test_bit(flag_startup, &up->flags))) {
		up->dma_invalid_irq_num++;
		chan_readl(txc, HSU_CH_SR);
		chan_readl(rxc, HSU_CH_SR);
		return;
	}
	disable_irq_nosync(up->dma_irq);
	spin_lock_irqsave(&up->port.lock, flags);
	serial_sched_cmd(up, qcmd_dma_irq);
	spin_unlock_irqrestore(&up->port.lock, flags);
}

/*
 * This handles the interrupt from one port.
 */
static irqreturn_t hsu_port_irq(int irq, void *dev_id)
{
	struct uart_hsu_port *up = dev_id;
	unsigned long flags;
	u8 lsr;

	trace_hsu_func_start(up->index, __func__);
	up->port_irq_num++;

	if (up->hw_type == hsu_intel) {
		if (unlikely(!test_bit(flag_set_alt, &up->flags))) {
			up->port_irq_no_alt++;
			trace_hsu_func_end(up->index, __func__, "noalt");
			return IRQ_NONE;
		}
	} else {
		if (unlikely(test_bit(flag_suspend, &up->flags))) {
			trace_hsu_func_end(up->index, __func__, "suspend");
			return IRQ_NONE;
		}

		/* On BYT, this IRQ may be shared with other HW */
		up->iir = serial_in(up, UART_IIR);
		if (unlikely(up->iir & 0x1)) {
			/*
			 * Read  UART_BYTE_COUNT and UART_OVERFLOW
			 * registers to clear the overrun error on
			 * Tx. This is a HW issue on VLV2 B0.
			 * more information on HSD 4683358.
			 */
			serial_in(up, 0x818 / 4);
			serial_in(up, 0x820 / 4);
			trace_hsu_func_end(up->index, __func__, "workaround");
			return IRQ_NONE;
		}
	}

	if (unlikely(!test_bit(flag_startup, &up->flags))) {
		pr_err("recv IRQ when we are not startup yet\n");
		/*SCU might forward it too late when it is closed already*/
		serial_in(up, UART_LSR);
		up->port_irq_no_startup++;
		trace_hsu_func_end(up->index, __func__, "nostart");
		return IRQ_HANDLED;
	}

	/* DesignWare HW's DMA mode still needs the port irq */
	if (up->use_dma && up->hw_type == hsu_intel) {
		lsr = serial_in(up, UART_LSR);
		spin_lock_irqsave(&up->port.lock, flags);
		check_modem_status(up);
		spin_unlock_irqrestore(&up->port.lock, flags);
		if (unlikely(lsr & (UART_LSR_BI | UART_LSR_PE |
				UART_LSR_FE | UART_LSR_OE)))
			dev_warn(up->dev,
				"Got LSR irq(0x%02x) while using DMA", lsr);
		trace_hsu_func_end(up->index, __func__, "lsr");
		return IRQ_HANDLED;
	}

	disable_irq_nosync(up->port.irq);
	spin_lock_irqsave(&up->port.lock, flags);
	serial_sched_cmd(up, qcmd_port_irq);
	spin_unlock_irqrestore(&up->port.lock, flags);

	trace_hsu_func_end(up->index, __func__, "");
	return IRQ_HANDLED;
}

static irqreturn_t hsu_dma_irq(int irq, void *dev_id)
{
	struct uart_hsu_port *up;
	unsigned long flags;
	unsigned int dmairq;
	int i;

	spin_lock_irqsave(&phsu->dma_lock, flags);
	dmairq = mfd_readl(phsu, HSU_GBL_DMAISR);
	if (phsu->irq_port_and_dma) {
		up = dev_id;
		up->port_dma_sts = dmairq;
		if (up->port_dma_sts & (3 << (up->index * 2)))
			hsu_dma_chan_handler(phsu, up->index);
	} else {
		for (i = 0; i < 3; i++)
			if (dmairq & (3 << (i * 2))) {
				up = phsu->chans[i * 2].uport;
				up->port_dma_sts = dmairq;
				hsu_dma_chan_handler(phsu, i);
			}
	}
	spin_unlock_irqrestore(&phsu->dma_lock, flags);

	return IRQ_HANDLED;
}

static unsigned int serial_hsu_tx_empty(struct uart_port *port)
{
	struct uart_hsu_port *up =
		container_of(port, struct uart_hsu_port, port);
	int ret = 1;

	trace_hsu_func_start(up->index, __func__);
	pm_runtime_get_sync(up->dev);
	serial_sched_stop(up);

	if (up->use_dma && test_bit(flag_tx_on, &up->flags))
		ret = 0;
	ret = ret &&
		(serial_in(up, UART_LSR) & UART_LSR_TEMT ? TIOCSER_TEMT : 0);
	serial_sched_start(up);
	pm_runtime_put(up->dev);
	trace_hsu_func_end(up->index, __func__, "");
	return ret;
}

static unsigned int serial_hsu_get_mctrl(struct uart_port *port)
{
	struct uart_hsu_port *up =
		container_of(port, struct uart_hsu_port, port);
	unsigned char status = up->msr;
	unsigned int ret = 0;

	trace_hsu_func_start(up->index, __func__);
	if (status & UART_MSR_DCD)
		ret |= TIOCM_CAR;
	if (status & UART_MSR_RI)
		ret |= TIOCM_RNG;
	if (status & UART_MSR_DSR)
		ret |= TIOCM_DSR;
	if (status & UART_MSR_CTS)
		ret |= TIOCM_CTS;
	trace_hsu_func_end(up->index, __func__, "");
	return ret;
}

static void set_mctrl(struct uart_hsu_port *up, unsigned int mctrl)
{
	trace_hsu_func_start(up->index, __func__);
	up->mcr &= ~(UART_MCR_RTS | UART_MCR_DTR | UART_MCR_OUT1 |
		     UART_MCR_OUT2 | UART_MCR_LOOP);
	if (mctrl & TIOCM_RTS)
		up->mcr |= UART_MCR_RTS;
	if (mctrl & TIOCM_DTR)
		up->mcr |= UART_MCR_DTR;
	if (mctrl & TIOCM_OUT1)
		up->mcr |= UART_MCR_OUT1;
	if (mctrl & TIOCM_OUT2)
		up->mcr |= UART_MCR_OUT2;
	if (mctrl & TIOCM_LOOP)
		up->mcr |= UART_MCR_LOOP;
	trace_hsu_mctrl(up->index, mctrl);
	serial_out(up, UART_MCR, up->mcr);
	udelay(100);
	trace_hsu_func_end(up->index, __func__, "");
}

static void serial_hsu_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
	struct uart_hsu_port *up =
		container_of(port, struct uart_hsu_port, port);

	trace_hsu_func_start(up->index, __func__);
	up->mcr &= ~(UART_MCR_RTS | UART_MCR_DTR | UART_MCR_OUT1 |
		     UART_MCR_OUT2 | UART_MCR_LOOP);
	if (mctrl & TIOCM_RTS)
		up->mcr |= UART_MCR_RTS;
	if (mctrl & TIOCM_DTR)
		up->mcr |= UART_MCR_DTR;
	if (mctrl & TIOCM_OUT1)
		up->mcr |= UART_MCR_OUT1;
	if (mctrl & TIOCM_OUT2)
		up->mcr |= UART_MCR_OUT2;
	if (mctrl & TIOCM_LOOP)
		up->mcr |= UART_MCR_LOOP;
	serial_sched_cmd(up, qcmd_set_mcr);
	trace_hsu_func_end(up->index, __func__, "");
}

static void serial_hsu_break_ctl(struct uart_port *port, int break_state)
{
	struct uart_hsu_port *up =
		container_of(port, struct uart_hsu_port, port);

	trace_hsu_func_start(up->index, __func__);
	pm_runtime_get_sync(up->dev);
	serial_sched_stop(up);
	if (break_state == -1)
		up->lcr |= UART_LCR_SBC;
	else
		up->lcr &= ~UART_LCR_SBC;
	serial_out(up, UART_LCR, up->lcr);
	serial_sched_start(up);
	pm_runtime_put(up->dev);
	trace_hsu_func_end(up->index, __func__, "");
}

/*
 * What special to do:
 * 1. chose the 64B fifo mode
 * 2. start dma or pio depends on configuration
 * 3. we only allocate dma memory when needed
 */
static int serial_hsu_startup(struct uart_port *port)
{
	static int console_first_init = 1;
	int ret = 0;
	unsigned long flags;
	static DEFINE_MUTEX(lock);
	struct uart_hsu_port *up =
		container_of(port, struct uart_hsu_port, port);
	struct hsu_port_cfg *cfg = phsu->configs[up->index];

	trace_hsu_func_start(up->index, __func__);
	mutex_lock(&lock);

	pm_runtime_get_sync(up->dev);

	/* HW start it */
	if (cfg->hw_reset)
		cfg->hw_reset(up->port.membase);

	if (console_first_init && test_bit(flag_console, &up->flags)) {
		serial_sched_stop(up);
		console_first_init = 0;
	}
	clear_bit(flag_reopen, &up->flags);
	if (cfg->has_alt) {
		struct hsu_port_cfg *alt_cfg = hsu_port_func_cfg + cfg->alt;
		struct uart_hsu_port *alt_up = phsu->port + alt_cfg->index;

		if (test_bit(flag_startup, &alt_up->flags) &&
			alt_up->port.state->port.tty) {
			if (alt_cfg->force_suspend) {
				uart_suspend_port(&serial_hsu_reg,
							&alt_up->port);
				serial_clear_alt(alt_up->index);
				set_bit(flag_reopen, &alt_up->flags);
			} else {
				int loop = 50;

				while (test_bit(flag_startup,
						&alt_up->flags) && --loop)
					msleep(20);
				if (test_bit(flag_startup, &alt_up->flags)) {
					WARN(1, "Share port open timeout\n");
					ret = -EBUSY;
					goto out;
				}
			}
		}
	}
	serial_set_alt(up->index);
	serial_sched_start(up);
	serial_sched_stop(up);

	/*
	 * Clear the FIFO buffers and disable them.
	 * (they will be reenabled in set_termios())
	 */
	serial_out(up, UART_FCR, UART_FCR_ENABLE_FIFO);
	serial_out(up, UART_FCR, UART_FCR_ENABLE_FIFO |
			UART_FCR_CLEAR_RCVR |
			UART_FCR_CLEAR_XMIT);
	serial_out(up, UART_FCR, 0);

	/* Clear the interrupt registers. */
	(void) serial_in(up, UART_LSR);
	(void) serial_in(up, UART_RX);
	(void) serial_in(up, UART_IIR);
	(void) serial_in(up, UART_MSR);

	/* Now, initialize the UART, default is 8n1 */
	serial_out(up, UART_LCR, UART_LCR_WLEN8);
	up->port.mctrl |= TIOCM_OUT2;
	set_mctrl(up, up->port.mctrl);

	/* DMA init */
	if (up->use_dma) {
		ret = up->dma_ops->init ? up->dma_ops->init(up) : -ENODEV;
		if (ret) {
			dev_warn(up->dev, "Fail to init DMA, will use PIO\n");
			up->use_dma = 0;
		}
	}

	/*
	 * Finally, enable interrupts.  Note: Modem status
	 * interrupts are set via set_termios(), which will
	 *  be occurring imminently
	 * anyway, so we don't enable them here.
	 */
	/* bit 4 for DW is reserved, but SEG need it to be set */
	if (!up->use_dma || up->hw_type == hsu_dw)
		up->ier = UART_IER_RLSI | UART_IER_RDI | UART_IER_RTOIE;
	else
		up->ier = 0;
	serial_out(up, UART_IER, up->ier);

	/* And clear the interrupt registers again for luck. */
	(void) serial_in(up, UART_LSR);
	(void) serial_in(up, UART_RX);
	(void) serial_in(up, UART_IIR);
	(void) serial_in(up, UART_MSR);

	set_bit(flag_startup, &up->flags);
	serial_sched_start(up);
	spin_lock_irqsave(&up->port.lock, flags);
	serial_sched_cmd(up, qcmd_get_msr);
	spin_unlock_irqrestore(&up->port.lock, flags);
	serial_sched_sync(up);

out:
	pm_runtime_put(up->dev);
	mutex_unlock(&lock);
	trace_hsu_func_end(up->index, __func__, "");
	return ret;
}

static void serial_hsu_shutdown(struct uart_port *port)
{
	static DEFINE_MUTEX(lock);
	struct uart_hsu_port *up =
		container_of(port, struct uart_hsu_port, port);
	struct hsu_port_cfg *cfg = phsu->configs[up->index];

	trace_hsu_func_start(up->index, __func__);
	mutex_lock(&lock);
	pm_runtime_get_sync(up->dev);
	serial_sched_stop(up);
	clear_bit(flag_startup, &up->flags);

	/* Disable interrupts from this port */
	up->ier = 0;
	serial_out(up, UART_IER, 0);

	clear_bit(flag_tx_on, &up->flags);

	up->port.mctrl &= ~TIOCM_OUT2;
	set_mctrl(up, up->port.mctrl);

	/* Disable break condition and FIFOs */
	serial_out(up, UART_LCR,
			serial_in(up, UART_LCR) & ~UART_LCR_SBC);
	serial_out(up, UART_FCR, UART_FCR_ENABLE_FIFO |
			UART_FCR_CLEAR_RCVR |
			UART_FCR_CLEAR_XMIT);
	serial_out(up, UART_FCR, 0);

	/* Free allocated dma buffer */
	if (up->use_dma)
		up->dma_ops->exit(up);

	if (cfg->has_alt) {
		struct hsu_port_cfg *alt_cfg = hsu_port_func_cfg + cfg->alt;
		struct uart_hsu_port *alt_up = phsu->port + alt_cfg->index;

		if (test_bit(flag_reopen, &alt_up->flags)) {
			serial_clear_alt(up->index);
			uart_resume_port(&serial_hsu_reg, &alt_up->port);
		}
	}

	pm_runtime_put_sync(up->dev);
	mutex_unlock(&lock);
	trace_hsu_func_end(up->index, __func__, "");
}

/* calculate mul,div for low fref e.g. TNG B0 38.4M
 * finally the fref will swith to high fref e.g. 100M
*/
static bool calc_for_low_fref(u32 clock, u32 baud, u32 *mul, u32 *div)
{
	if (clock == 38400) {
		switch (baud) {
		case 3500000:
			/* ps: 10 */
			*mul = 350;
			*div = 384;
			break;
		case 3000000:
			/* ps: 12 */
			*mul = 360;
			*div = 384;
			break;
		case 2500000:
			/* ps: 12 */
			*mul = 300;
			*div = 384;
			break;
		case 2000000:
			/* ps: 16 */
			*mul = 320;
			*div = 384;
			break;
		case 1843200:
			/* ps: 16 */
			*mul = 294912;
			*div = 384000;
			break;
		case 1500000:
			/* ps: 16 */
			*mul = 240;
			*div = 384;
			break;
		case 1000000:
			/* ps: 16 */
			*mul = 160;
			*div = 384;
			break;
		case 500000:
			/* ps: 16 */
			*mul = 80;
			*div = 384;
			break;
		}
		return true;
	} else
		return false;
}

static void
serial_hsu_set_termios(struct uart_port *port, struct ktermios *termios,
		       struct ktermios *old)
{
	struct uart_hsu_port *up =
			container_of(port, struct uart_hsu_port, port);
	struct hsu_port_cfg *cfg = phsu->configs[up->index];
	unsigned char cval, fcr = 0;
	unsigned long flags;
	unsigned int baud, quot, clock, bits;
	/* 0x3d09 is default dividor value refer for deatils
	 * please refer high speed UART HAS documents.
	 */
	u32 ps = 0, mul = 0, div = 0x3D09, m = 0, n = 0;

	trace_hsu_func_start(up->index, __func__);
	switch (termios->c_cflag & CSIZE) {
	case CS5:
		cval = UART_LCR_WLEN5;
		bits = 7;
		break;
	case CS6:
		cval = UART_LCR_WLEN6;
		bits = 8;
		break;
	case CS7:
		cval = UART_LCR_WLEN7;
		bits = 9;
		break;
	default:
	case CS8:
		cval = UART_LCR_WLEN8;
		bits = 10;
		break;
	}

	/* CMSPAR isn't supported by this driver */
	termios->c_cflag &= ~CMSPAR;

	if (termios->c_cflag & CSTOPB) {
		cval |= UART_LCR_STOP;
		bits++;
	}
	if (termios->c_cflag & PARENB) {
		cval |= UART_LCR_PARITY;
		bits++;
	}
	if (!(termios->c_cflag & PARODD))
		cval |= UART_LCR_EPAR;

	baud = uart_get_baud_rate(port, termios, old, 0, 4000000);
	trace_hsu_set_termios(up->index, baud, termios->c_cflag & CRTSCTS ? 1 : 0);

	if (up->hw_type == hsu_intel) {
		/*
		 * If base clk is 50Mhz, and the baud rate come from:
		 *	baud = 50M * MUL / (DIV * PS * DLAB)
		 *
		 * For those basic low baud rate we can get the direct
		 * scalar from 2746800, like 115200 = 2746800/24. For those
		 * higher baud rate, we handle them case by case, mainly by
		 * adjusting the MUL/PS registers, and DIV register is kept
		 * as default value 0x3d09 to make things simple
		 */

		if (cfg->hw_get_clk)
			clock = cfg->hw_get_clk();
		else
			clock = 50000;
		/* ps = 16 is prefered, if not have to use 12, l0 */
		if (baud * 16 <= clock * 1000)
			ps = 16;
		else if (baud * 12 <= clock * 1000)
			ps = 12;
		else if (baud * 10 <= clock * 1000)
			ps = 10;
		else
			pr_err("port:%d baud:%d is too high for clock:%u M\n",
				up->index, baud, clock / 1000);

		switch (baud) {
		case 3500000:
		case 3000000:
		case 2500000:
		case 2000000:
		case 1843200:
		case 1500000:
		case 1000000:
		case 500000:
			quot = 1;
			if (!calc_for_low_fref(clock, baud, &mul, &div))
				/*
				 * mul = baud * 0x3d09 * ps / 1000 / clock
				 * change the formula order to avoid overflow
				 */
				mul = (0x3d09 * ps / 100) * (baud / 100)
					* 10 / clock;
			break;
		default:
			/* Use uart_get_divisor to get quot for other baud rates
			 * avoid overflow: mul = uartclk * 0x3d09 / clock / 1000
			 * uartclk is multiply of 115200 * n * 16 */
			mul = (up->port.uartclk / 1600) * 0x3d09 /
				clock * 16 / 10;
			quot = 0;
		}

		if (!quot)
			quot = uart_get_divisor(port, baud);

		if ((up->port.uartclk / quot) < (2400 * 16))
			fcr = UART_FCR_ENABLE_FIFO | UART_FCR_HSU_64_1B;
		else if ((up->port.uartclk / quot) < (230400 * 16))
			fcr = UART_FCR_ENABLE_FIFO | UART_FCR_HSU_64_16B;
		else
			fcr = UART_FCR_ENABLE_FIFO | UART_FCR_HSU_64_32B;

		fcr |= UART_FCR_HSU_64B_FIFO;
	} else {
		/* need calc quot here */
		switch (baud) {
		case 3000000:
		case 1500000:
		case 1000000:
		case 500000:
			m = 48;
			n = 100;
			quot = 3000000 / baud;
			break;
		default:
			m = 9216;
			n = 15625;
			quot = 0;
		}
		if (!quot)
			quot = uart_get_divisor(port, baud);

		fcr = UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_10 |
			UART_FCR_T_TRIG_11;
		if (baud < 2400) {
			fcr &= ~UART_FCR_TRIGGER_MASK;
			fcr |= UART_FCR_TRIGGER_1;
		}
	}

	/* one byte transfer duration unit microsecond */
	up->byte_delay = (bits * 1000000 + baud - 1) / baud;

	pm_runtime_get_sync(up->dev);
	serial_sched_stop(up);
	/*
	 * Ok, we're now changing the port state.  Do it with
	 * interrupts disabled.
	 */
	spin_lock_irqsave(&up->port.lock, flags);

	/* Update the per-port timeout */
	uart_update_timeout(port, termios->c_cflag, baud);

	up->port.read_status_mask = UART_LSR_OE | UART_LSR_THRE | UART_LSR_DR;
	if (termios->c_iflag & INPCK)
		up->port.read_status_mask |= UART_LSR_FE | UART_LSR_PE;
	if (termios->c_iflag & (BRKINT | PARMRK))
		up->port.read_status_mask |= UART_LSR_BI;

	/* Characters to ignore */
	up->port.ignore_status_mask = 0;
	if (termios->c_iflag & IGNPAR)
		up->port.ignore_status_mask |= UART_LSR_PE | UART_LSR_FE;
	if (termios->c_iflag & IGNBRK) {
		up->port.ignore_status_mask |= UART_LSR_BI;
		/*
		 * If we're ignoring parity and break indicators,
		 * ignore overruns too (for real raw support).
		 */
		if (termios->c_iflag & IGNPAR)
			up->port.ignore_status_mask |= UART_LSR_OE;
	}

	/* Ignore all characters if CREAD is not set */
	if ((termios->c_cflag & CREAD) == 0)
		up->port.ignore_status_mask |= UART_LSR_DR;

	/*
	 * CTS flow control flag and modem status interrupts, disable
	 * MSI by default
	 */
	up->ier &= ~UART_IER_MSI;
	if (!cfg->hw_ctrl_cts && UART_ENABLE_MS(&up->port, termios->c_cflag))
		up->ier |= UART_IER_MSI;

	serial_out(up, UART_IER, up->ier);

	if (termios->c_cflag & CRTSCTS)
		up->mcr |= UART_MCR_AFE | UART_MCR_RTS;
	else
		up->mcr &= ~UART_MCR_AFE;

	up->dll	= quot & 0xff;
	up->dlm	= quot >> 8;
	up->fcr	= fcr;
	up->lcr = cval;					/* Save LCR */

	serial_out(up, UART_LCR, cval | UART_LCR_DLAB);	/* set DLAB */
	serial_out(up, UART_DLL, up->dll);		/* LS of divisor */
	serial_out(up, UART_DLM, up->dlm);		/* MS of divisor */
	serial_out(up, UART_LCR, cval);			/* reset DLAB */

	if (up->hw_type == hsu_intel) {
		up->mul	= mul;
		up->div = div;
		up->ps	= ps;
		serial_out(up, UART_MUL, up->mul);	/* set MUL */
		serial_out(up, UART_DIV, up->div);	/* set DIV */
		serial_out(up, UART_PS, up->ps);	/* set PS */
	} else {
		if (m != up->m || n != up->n) {
			if (cfg->set_clk)
				cfg->set_clk(m, n, up->port.membase);
			up->m = m;
			up->n = n;
		}
	}

	serial_out(up, UART_FCR, fcr);
	set_mctrl(up, up->port.mctrl);
	serial_sched_cmd(up, qcmd_get_msr);
	spin_unlock_irqrestore(&up->port.lock, flags);
	serial_sched_start(up);
	serial_sched_sync(up);
	pm_runtime_put(up->dev);
	trace_hsu_func_end(up->index, __func__, "");
}

static void
serial_hsu_pm(struct uart_port *port, unsigned int state,
	      unsigned int oldstate)
{
}

static void serial_hsu_release_port(struct uart_port *port)
{
}

static int serial_hsu_request_port(struct uart_port *port)
{
	return 0;
}

static void serial_hsu_config_port(struct uart_port *port, int flags)
{
	struct uart_hsu_port *up =
		container_of(port, struct uart_hsu_port, port);
	up->port.type = PORT_MFD;
}

static int
serial_hsu_verify_port(struct uart_port *port, struct serial_struct *ser)
{
	/* We don't want the core code to modify any port params */
	return -EINVAL;
}

static const char *
serial_hsu_type(struct uart_port *port)
{
	struct uart_hsu_port *up =
		container_of(port, struct uart_hsu_port, port);
	return up->name;
}

struct device *intel_mid_hsu_set_wake_peer(int port,
	void (*wake_peer)(struct device *))
{
	struct hsu_port_cfg *cfg = phsu->configs[port];

	cfg->wake_peer = wake_peer;
	return cfg->dev;
}
EXPORT_SYMBOL(intel_mid_hsu_set_wake_peer);

static void serial_hsu_wake_peer(struct uart_port *port)
{
	struct uart_hsu_port *up =
			container_of(port, struct uart_hsu_port, port);
	struct hsu_port_cfg *cfg = phsu->configs[up->index];

	trace_hsu_func_start(up->index, __func__);
	if (cfg->wake_peer)
		cfg->wake_peer(cfg->dev);
	trace_hsu_func_end(up->index, __func__, "");
}

#define BOTH_EMPTY (UART_LSR_TEMT | UART_LSR_THRE)
/* Wait for transmitter & holding register to empty */
static inline int wait_for_xmitr(struct uart_hsu_port *up)
{
	unsigned int status, tmout = 10000;

	while (--tmout) {
		status = serial_in(up, UART_LSR);
		if (status & UART_LSR_BI)
			up->lsr_break_flag = UART_LSR_BI;
		udelay(1);
		if (status & BOTH_EMPTY)
			break;
	}
	if (tmout == 0)
		return 0;

	if (up->port.flags & UPF_CONS_FLOW) {
		tmout = 10000;
		while (--tmout &&
		       ((serial_in(up, UART_MSR) & UART_MSR_CTS) == 0))
			udelay(1);
		if (tmout == 0)
			return 0;
	}
	return 1;
}

#ifdef CONFIG_CONSOLE_POLL
static int serial_hsu_get_poll_char(struct uart_port *port)
{
	struct uart_hsu_port *up =
		container_of(port, struct uart_hsu_port, port);
	u8 lsr;

	lsr = serial_in(up, UART_LSR);
	if (!(lsr & UART_LSR_DR))
		return NO_POLL_CHAR;
	return serial_in(up, UART_RX);
}

static void serial_hsu_put_poll_char(struct uart_port *port,
			unsigned char c)
{
	struct uart_hsu_port *up =
		container_of(port, struct uart_hsu_port, port);

	serial_out(up, UART_IER, 0);
	while (!wait_for_xmitr(up))
		cpu_relax();
	serial_out(up, UART_TX, c);
	while (!wait_for_xmitr(up))
		cpu_relax();
	serial_out(up, UART_IER, up->ier);
}
#endif

#ifdef CONFIG_SERIAL_MFD_HSU_EXT_CONSOLE
static void serial_hsu_console_putchar(struct uart_port *port, int ch)
{
	struct uart_hsu_port *up =
		container_of(port, struct uart_hsu_port, port);
	cl_put_char(up, ch);
}

/*
 * Print a string to the serial port trying not to disturb
 * any possible real use of the port...
 *
 *	The console_lock must be held when we get here.
 */
static void
serial_hsu_console_write(struct console *co, const char *s, unsigned int count)
{
	struct uart_hsu_port *up = phsu->port + co->index;
	unsigned long flags;

	uart_console_write(&up->port, s, count, serial_hsu_console_putchar);
	spin_lock_irqsave(&up->cl_lock, flags);
	serial_sched_cmd(up, qcmd_cl);
	spin_unlock_irqrestore(&up->cl_lock, flags);
}

static struct console serial_hsu_console;

static int __init
serial_hsu_console_setup(struct console *co, char *options)
{
	struct uart_hsu_port *up = phsu->port + co->index;
	int baud = 115200;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';
	unsigned long flags;

	if (co->index < 0 || co->index >= hsu_port_max)
		return -ENODEV;

	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);

	pm_runtime_get_sync(up->dev);
	set_bit(flag_console, &up->flags);
	set_bit(flag_startup, &up->flags);
	serial_set_alt(up->index);
	serial_sched_start(up);
	spin_lock_irqsave(&up->port.lock, flags);
	serial_sched_cmd(up, qcmd_get_msr);
	spin_unlock_irqrestore(&up->port.lock, flags);
	serial_sched_sync(up);
	pm_runtime_put(up->dev);
	up->cl_circ.buf = kzalloc(HSU_CL_BUF_LEN, GFP_KERNEL);
	if (up->cl_circ.buf == NULL)
		return -ENOMEM;
	return uart_set_options(&up->port, co, baud, parity, bits, flow);
}

static struct console serial_hsu_console = {
	.name		= "ttyMFD",
	.write		= serial_hsu_console_write,
	.device		= uart_console_device,
	.setup		= serial_hsu_console_setup,
	.flags		= CON_PRINTBUFFER,
	.index		= -1,
	.data		= &serial_hsu_reg,
};

#define SERIAL_HSU_CONSOLE	(&serial_hsu_console)
#else
#define SERIAL_HSU_CONSOLE	NULL
#endif

struct uart_ops serial_hsu_pops = {
	.tx_empty	= serial_hsu_tx_empty,
	.set_mctrl	= serial_hsu_set_mctrl,
	.get_mctrl	= serial_hsu_get_mctrl,
	.stop_tx	= serial_hsu_stop_tx,
	.start_tx	= serial_hsu_start_tx,
	.stop_rx	= serial_hsu_stop_rx,
	.enable_ms	= serial_hsu_enable_ms,
	.break_ctl	= serial_hsu_break_ctl,
	.startup	= serial_hsu_startup,
	.shutdown	= serial_hsu_shutdown,
	.set_termios	= serial_hsu_set_termios,
	.pm		= serial_hsu_pm,
	.type		= serial_hsu_type,
	.release_port	= serial_hsu_release_port,
	.request_port	= serial_hsu_request_port,
	.config_port	= serial_hsu_config_port,
	.verify_port	= serial_hsu_verify_port,
	.wake_peer	= serial_hsu_wake_peer,
#ifdef CONFIG_CONSOLE_POLL
	.poll_get_char = serial_hsu_get_poll_char,
	.poll_put_char = serial_hsu_put_poll_char,
#endif
};

static struct uart_driver serial_hsu_reg = {
	.owner		= THIS_MODULE,
	.driver_name	= "MFD serial",
	.dev_name	= "ttyMFD",
	.major		= TTY_MAJOR,
	.minor		= 128,
	.nr		= HSU_PORT_MAX,
};

static irqreturn_t wakeup_irq(int irq, void *dev)
{
	struct uart_hsu_port *up = dev_get_drvdata(dev);
	struct hsu_port_cfg *cfg = phsu->configs[up->index];

	trace_hsu_func_start(up->index, __func__);
	set_bit(flag_active, &up->flags);
	if (cfg->preamble && cfg->hw_set_rts)
		cfg->hw_set_rts(up->index, 1);
	pm_runtime_get(dev);
	pm_runtime_put(dev);
	trace_hsu_func_end(up->index, __func__, "");
	return IRQ_HANDLED;
}

#if defined(CONFIG_PM) || defined(CONFIG_PM_RUNTIME)
static void hsu_flush_rxfifo(struct uart_hsu_port *up)
{
	unsigned int lsr, cnt;

	if (up->hw_type == hsu_intel) {
		cnt = serial_in(up, UART_FOR) & 0x7F;
		if (cnt)
			dev_dbg(up->dev,
				"Warning: %d bytes are received"
				" in RX fifo after RTS active for %d us\n",
				cnt, up->byte_delay);
		lsr = serial_in(up, UART_LSR);
		if (lsr & UART_LSR_DR && cnt)
			dev_dbg(up->dev,
				"flush abnormal data in rx fifo\n");
			while (cnt) {
				serial_in(up, UART_RX);
				cnt--;
			}
	}
}

static void hsu_regs_context(struct uart_hsu_port *up, int op)
{
	struct hsu_port_cfg *cfg = phsu->configs[up->index];

	if (op == context_load) {
		/*
		 * Delay a while before HW get stable. Without this the
		 * resume will just fail, as the value you write to the
		 * HW register will not be really written.
		 *
		 * This is only needed for Tangier, which really powers gate
		 * the HSU HW in runtime suspend. While in Penwell/CLV it is
		 * only clock gated.
		*/
		usleep_range(500, 510);

		if (cfg->hw_reset)
			cfg->hw_reset(up->port.membase);

		serial_out(up, UART_LCR, up->lcr);
		serial_out(up, UART_LCR, up->lcr | UART_LCR_DLAB);
		serial_out(up, UART_DLL, up->dll);
		serial_out(up, UART_DLM, up->dlm);
		serial_out(up, UART_LCR, up->lcr);

		if (up->hw_type == hsu_intel) {
			serial_out(up, UART_MUL, up->mul);
			serial_out(up, UART_DIV, up->div);
			serial_out(up, UART_PS, up->ps);
		} else {
			if (cfg->set_clk)
				cfg->set_clk(up->m, up->n, up->port.membase);
		}

		serial_out(up, UART_MCR, up->mcr);
		serial_out(up, UART_FCR, up->fcr);
		serial_out(up, UART_IER, up->ier);
	}

	if (up->use_dma && up->dma_ops->context_op)
		up->dma_ops->context_op(up, op);
}

int serial_hsu_do_suspend(struct uart_hsu_port *up)
{
	struct hsu_port_cfg *cfg = phsu->configs[up->index];
	struct uart_port *uport = &up->port;
	struct tty_port *tport = &uport->state->port;
	struct tty_struct *tty = tport->tty;
	struct circ_buf *xmit = &up->port.state->xmit;
	char cmd;
	unsigned long flags;

	trace_hsu_func_start(up->index, __func__);

	if (test_bit(flag_startup, &up->flags)) {
		if (up->hw_type == hsu_intel &&
			serial_in(up, UART_FOR) & 0x7F)
			goto busy;
		else if (up->hw_type == hsu_dw &&
			serial_in(up, 0x7c / 4) & BIT(3))
			goto busy;
	}

	if (up->use_dma) {
		if (up->hw_type == hsu_intel) {
			if (chan_readl(up->rxc, HSU_CH_D0SAR) >
					up->rxbuf.dma_addr)
				goto busy;
		}
	}

	if (cfg->hw_set_rts)
		cfg->hw_set_rts(up->index, 1);

	disable_irq(up->port.irq);
	disable_irq(up->dma_irq);

	if (cfg->hw_set_rts)
		usleep_range(up->byte_delay, up->byte_delay + 1);

	serial_sched_stop(up);
	set_bit(flag_suspend, &up->flags);

	if (test_bit(flag_startup, &up->flags) && check_qcmd(up, &cmd)) {
		dev_info(up->dev, "ignore suspend cmd: %d\n", cmd);
		goto err;
	}

	if (test_bit(flag_tx_on, &up->flags)) {
		dev_info(up->dev, "ignore suspend for tx on\n");
		dev_info(up->dev,
			"xmit pending:%d, stopped:%d, hw_stopped:%d, MSR:%x\n",
			(int)uart_circ_chars_pending(xmit), tty->stopped,
			tty->hw_stopped, serial_in(up, UART_MSR));
		goto err;
	}

	if (test_bit(flag_startup, &up->flags) && !uart_circ_empty(xmit) &&
		!uart_tx_stopped(&up->port)) {
		dev_info(up->dev, "ignore suspend for xmit\n");
		dev_info(up->dev,
			"xmit pending:%d, stopped:%d, hw_stopped:%d, MSR:%x\n",
			(int)uart_circ_chars_pending(xmit),
			tty->stopped,
			tty->hw_stopped,
			serial_in(up, UART_MSR));
		goto err;
	}

	if (up->use_dma) {
		if (up->dma_ops->suspend(up))
			goto err;
	} else if (test_bit(flag_startup, &up->flags)) {
		if (up->hw_type == hsu_intel &&
			serial_in(up, UART_FOR) & 0x7F)
			goto err;
		else if (up->hw_type == hsu_dw &&
			serial_in(up, 0x7c / 4) & BIT(3))
			goto err;
	}

	if (cfg->hw_suspend)
		cfg->hw_suspend(up->index, up->dev, wakeup_irq);
	if (cfg->hw_context_save)
		hsu_regs_context(up, context_save);
	if (cfg->preamble && cfg->hw_suspend_post)
		cfg->hw_suspend_post(up->index);
	enable_irq(up->dma_irq);
	if (up->hw_type == hsu_dw)
		enable_irq(up->port.irq);

	trace_hsu_func_end(up->index, __func__, "");
	return 0;
err:
	if (cfg->hw_set_rts)
		cfg->hw_set_rts(up->index, 0);
	clear_bit(flag_suspend, &up->flags);
	enable_irq(up->port.irq);
	if (up->use_dma && up->hw_type == hsu_intel)
		intel_dma_do_rx(up, 0);
	enable_irq(up->dma_irq);
	serial_sched_start(up);
	spin_lock_irqsave(&up->port.lock, flags);
	serial_sched_cmd(up, qcmd_get_msr);
	spin_unlock_irqrestore(&up->port.lock, flags);
	serial_sched_sync(up);
busy:
	pm_schedule_suspend(up->dev, cfg->idle);
	trace_hsu_func_end(up->index, __func__, "busy");
	return -EBUSY;
}
EXPORT_SYMBOL(serial_hsu_do_suspend);

int serial_hsu_do_resume(struct uart_hsu_port *up)
{
	struct hsu_port_cfg *cfg = phsu->configs[up->index];
	unsigned long flags;

	trace_hsu_func_start(up->index, __func__);
	if (!test_and_clear_bit(flag_suspend, &up->flags)) {
		trace_hsu_func_end(up->index, __func__, "ignore");
		return 0;
	}
	if (up->hw_type == hsu_dw)
		disable_irq(up->port.irq);
	if (cfg->hw_context_save)
		hsu_regs_context(up, context_load);
	if (cfg->hw_resume)
		cfg->hw_resume(up->index, up->dev);
	if (test_bit(flag_startup, &up->flags))
		hsu_flush_rxfifo(up);
	if (up->use_dma)
		up->dma_ops->resume(up);
	if (cfg->hw_set_rts)
		cfg->hw_set_rts(up->index, 0);
	enable_irq(up->port.irq);

	serial_sched_start(up);
	spin_lock_irqsave(&up->port.lock, flags);
	serial_sched_cmd(up, qcmd_get_msr);
	spin_unlock_irqrestore(&up->port.lock, flags);
	serial_sched_sync(up);
	trace_hsu_func_end(up->index, __func__, "");
	return 0;
}
EXPORT_SYMBOL(serial_hsu_do_resume);
#endif

#ifdef CONFIG_PM_RUNTIME
int serial_hsu_do_runtime_idle(struct uart_hsu_port *up)
{
	struct hsu_port_cfg *cfg = phsu->configs[up->index];

	trace_hsu_func_start(up->index, __func__);
	if (cfg->type == debug_port
			&& system_state == SYSTEM_BOOTING)
		/* if HSU is set as default console, but earlyprintk is not hsu,
		 * then it will enter suspend and can not get back since system
		 * is on boot up, no contex switch to let it resume, here just
		 * postpone the suspend retry 30 seconds, then system should
		 * have finished booting
		 */
		pm_schedule_suspend(up->dev, 30000);
	else if (!test_and_clear_bit(flag_active, &up->flags))
		pm_schedule_suspend(up->dev, 20);
	else
		pm_schedule_suspend(up->dev, cfg->idle);
	trace_hsu_func_end(up->index, __func__, "");
	return -EBUSY;
}
EXPORT_SYMBOL(serial_hsu_do_runtime_idle);
#endif

static void serial_hsu_command(struct uart_hsu_port *up)
{
	char cmd, c;
	unsigned long flags;
	unsigned int iir, lsr;
	int status;
	struct hsu_dma_chan *txc = up->txc;
	struct hsu_dma_chan *rxc = up->rxc;
	struct hsu_port_cfg *cfg = phsu->configs[up->index];

	trace_hsu_func_start(up->index, __func__);
	if (unlikely(test_bit(flag_cmd_off, &up->flags))) {
		trace_hsu_func_end(up->index, __func__, "cmd_off");
		return;
	}
	if (unlikely(test_bit(flag_suspend, &up->flags))) {
		dev_err(up->dev,
			"Error to handle cmd while port is suspended\n");
		if (check_qcmd(up, &cmd))
			dev_err(up->dev, "Command pending: %d\n", cmd);
		trace_hsu_func_end(up->index, __func__, "suspend");
		return;
	}
	set_bit(flag_active, &up->flags);
	spin_lock_irqsave(&up->port.lock, flags);
	while (get_qcmd(up, &cmd)) {
		spin_unlock_irqrestore(&up->port.lock, flags);
		trace_hsu_cmd_start(up->index, cmd);
		switch (cmd) {
		case qcmd_overflow:
			dev_err(up->dev, "queue overflow!!\n");
			break;
		case qcmd_set_mcr:
			serial_out(up, UART_MCR, up->mcr);
			break;
		case qcmd_set_ier:
			serial_out(up, UART_IER, up->ier);
			break;
		case qcmd_start_rx:
			/* use for DW DMA RX only */
			if (test_and_clear_bit(flag_rx_pending, &up->flags)) {
				if (up->use_dma)
					up->dma_ops->start_rx(up);
			}
			break;
		case qcmd_stop_rx:
			if (!up->use_dma || up->hw_type == hsu_dw) {
				up->ier &= ~UART_IER_RLSI;
				up->port.read_status_mask &= ~UART_LSR_DR;
				serial_out(up, UART_IER, up->ier);
			}

			if (up->use_dma)
				up->dma_ops->stop_rx(up);
			break;
		case qcmd_start_tx:
			if (up->use_dma) {
				if (!test_bit(flag_tx_on, &up->flags))
					up->dma_ops->start_tx(up);
			} else if (!(up->ier & UART_IER_THRI)) {
				up->ier |= UART_IER_THRI;
				serial_out(up, UART_IER, up->ier);
			}
			break;
		case qcmd_stop_tx:
			if (up->use_dma) {
				spin_lock_irqsave(&up->port.lock, flags);
				up->dma_ops->stop_tx(up);
				clear_bit(flag_tx_on, &up->flags);
				spin_unlock_irqrestore(&up->port.lock, flags);
			} else if (up->ier & UART_IER_THRI) {
				up->ier &= ~UART_IER_THRI;
				serial_out(up, UART_IER, up->ier);
			}
			break;
		case qcmd_cl:
			serial_out(up, UART_IER, 0);
			while (cl_get_char(up, &c)) {
				while (!wait_for_xmitr(up))
					schedule();
				serial_out(up, UART_TX, c);
			}
			serial_out(up, UART_IER, up->ier);
			break;
		case qcmd_port_irq:
			up->port_irq_cmddone++;

			/* Baytrail patform use shared IRQ and need more care */
			if (up->hw_type == hsu_intel) {
				iir = serial_in(up, UART_IIR);
			} else {
				if (up->iir & 0x1)
					up->iir = serial_in(up, UART_IIR);
				iir = up->iir;
				up->iir = 1;
			}

			if (iir & UART_IIR_NO_INT) {
				enable_irq(up->port.irq);
				up->port_irq_pio_no_irq_pend++;
				break;
			}

			if (iir & HSU_PIO_RX_ERR)
				up->port_irq_pio_rx_err++;
			if (iir & HSU_PIO_RX_AVB)
				up->port_irq_pio_rx_avb++;
			if (iir & HSU_PIO_RX_TMO)
				up->port_irq_pio_rx_timeout++;
			if (iir & HSU_PIO_TX_REQ)
				up->port_irq_pio_tx_req++;

			lsr = serial_in(up, UART_LSR);

			/* We need to judge it's timeout or data available */
			if (lsr & UART_LSR_DR) {
				if (!up->use_dma) {
					receive_chars(up, &lsr);
				} else if (up->hw_type == hsu_dw) {
					if ((iir & 0xf) == 0xc) {
						/*
						 * RX timeout IRQ, the DMA
						 * channel may be stalled
						 */
						up->dma_ops->stop_rx(up);
						receive_chars(up, &lsr);
					} else
						up->dma_ops->start_rx(up);
				}
			}

			/* lsr will be renewed during the receive_chars */
			if (!up->use_dma && (lsr & UART_LSR_THRE))
				transmit_chars(up);

			spin_lock_irqsave(&up->port.lock, flags);
			serial_sched_cmd(up, qcmd_enable_irq);
			spin_unlock_irqrestore(&up->port.lock, flags);
			break;
		case qcmd_enable_irq:
			enable_irq(up->port.irq);
			break;
		case qcmd_dma_irq:
			/* Only hsu_intel has this irq */
			up->dma_irq_cmddone++;
			if (up->port_dma_sts & (1 << txc->id)) {
				up->dma_tx_irq_cmddone++;
				status = chan_readl(txc, HSU_CH_SR);
				up->dma_ops->start_tx(up);
			}

			if (up->port_dma_sts & (1 << rxc->id)) {
				status = chan_readl(rxc, HSU_CH_SR);
				intel_dma_do_rx(up, status);
			}
			enable_irq(up->dma_irq);
			break;
		case qcmd_cmd_off:
			set_bit(flag_cmd_off, &up->flags);
			break;
		case qcmd_get_msr:
			break;
		default:
			dev_err(up->dev, "invalid command!!\n");
			break;
		}
		trace_hsu_cmd_end(up->index, cmd);
		spin_lock_irqsave(&up->port.lock, flags);
		if (unlikely(test_bit(flag_cmd_off, &up->flags)))
			break;
	}
	up->msr = serial_in(up, UART_MSR);
	if (cfg->hw_ctrl_cts)
		up->msr |= UART_MSR_CTS;
	check_modem_status(up);
	spin_unlock_irqrestore(&up->port.lock, flags);
	trace_hsu_func_end(up->index, __func__, "");
}

static void serial_hsu_tasklet(unsigned long data)
{
	struct uart_hsu_port *up = (struct uart_hsu_port *)data;

	up->in_tasklet = 1;
	serial_hsu_command(up);
	up->tasklet_done++;
	up->in_tasklet = 0;
}

static void serial_hsu_work(struct work_struct *work)
{
	struct uart_hsu_port *uport =
		container_of(work, struct uart_hsu_port, work);

	uport->in_workq = 1;
	serial_hsu_command(uport);
	uport->workq_done++;
	uport->in_workq = 0;
}

static int serial_port_setup(struct uart_hsu_port *up,
		struct hsu_port_cfg *cfg)
{
	int ret;
	int index = cfg->index;

	phsu->configs[index] = cfg;
	up->port.line = index;
	snprintf(up->name, sizeof(up->name) - 1, "%s_p", cfg->name);
	up->index = index;

	if ((hsu_dma_enable & (1 << index)) && up->dma_ops)
		up->use_dma = 1;
	else
		up->use_dma = 0;

	if (cfg->hw_init)
		cfg->hw_init(up->dev, index);
	mutex_init(&up->q_mutex);
	tasklet_init(&up->tasklet, serial_hsu_tasklet,
				(unsigned long)up);
	up->workqueue =
		create_singlethread_workqueue(up->name);
	INIT_WORK(&up->work, serial_hsu_work);
	up->qcirc.buf = (char *)up->qbuf;
	spin_lock_init(&up->cl_lock);
	set_bit(flag_cmd_off, &up->flags);

	if (phsu->irq_port_and_dma) {
		up->dma_irq = up->port.irq;
		ret = request_irq(up->dma_irq, hsu_dma_irq, IRQF_SHARED,
				"hsu dma", up);
		if (ret) {
			dev_err(up->dev, "can not get dma IRQ\n");
			return ret;
		}
		ret = request_irq(up->port.irq, hsu_port_irq, IRQF_SHARED,
				up->name, up);
		if (ret) {
			dev_err(up->dev, "can not get port IRQ\n");
			return ret;
		}
	} else {
		up->dma_irq = phsu->dma_irq;
		ret = request_irq(up->port.irq, hsu_port_irq, IRQF_SHARED,
				up->name, up);
		if (ret) {
			dev_err(up->dev, "can not get port IRQ\n");
			return ret;
		}
	}

	if (cfg->type == debug_port) {
		serial_hsu_reg.cons = SERIAL_HSU_CONSOLE;
		if (serial_hsu_reg.cons)
			serial_hsu_reg.cons->index = index;
		up->use_dma = 0;
	} else
		serial_hsu_reg.cons = NULL;
	uart_add_one_port(&serial_hsu_reg, &up->port);
	return 0;
}

struct uart_hsu_port *serial_hsu_port_setup(struct device *pdev, int port,
	resource_size_t start, resource_size_t len, int irq)
{
	struct uart_hsu_port *up;
	int index;
	unsigned int uclk, clock;
	struct hsu_port_cfg *cfg;

	cfg = hsu_port_func_cfg + port;
	if (!cfg)
		return ERR_PTR(-EINVAL);

	pr_info("Found a %s HSU\n", cfg->hw_ip ? "Designware" : "Intel");

	index = cfg->index;
	up = phsu->port + index;

	up->dev = pdev;
	up->port.type = PORT_MFD;
	up->port.iotype = UPIO_MEM;
	up->port.mapbase = start;
	up->port.membase = ioremap_nocache(up->port.mapbase, len);
	up->port.fifosize = 64;
	up->port.ops = &serial_hsu_pops;
	up->port.flags = UPF_IOREMAP;
	up->hw_type = cfg->hw_ip;
	/* calculate if DLAB=1, the ideal uartclk */
	if (cfg->hw_get_clk)
		clock = cfg->hw_get_clk();
	else
		clock = 50000;
	uclk = clock * 1000 / (115200 * 16); /* 16 is default ps */
	if (uclk >= 24)
		uclk = 24;
	else if (uclk >= 16)
		uclk = 16;
	else if (uclk >= 8)
		uclk = 8;
	else
		uclk = 1;

	if (up->hw_type == hsu_intel)
		up->port.uartclk = 115200 * uclk * 16;
	else
		up->port.uartclk = 115200 * 32 * 16;

	up->port.irq = irq;
	up->port.dev = pdev;

	if (up->hw_type == hsu_intel) {
		up->txc = &phsu->chans[index * 2];
		up->rxc = &phsu->chans[index * 2 + 1];
		up->dma_ops = &intel_dma_ops;
	} else {
		up->dma_ops = pdw_dma_ops;
	}

	if (cfg->has_alt) {
		struct hsu_port_cfg *alt_cfg =
			hsu_port_func_cfg + cfg->alt;
		struct uart_hsu_port *alt_up =
			phsu->port + alt_cfg->index;

		memcpy(alt_up, up, sizeof(*up));
		serial_port_setup(alt_up, alt_cfg);
		phsu->port_num++;
	}

	serial_port_setup(up, cfg);
	phsu->port_num++;

	return up;
}
EXPORT_SYMBOL(serial_hsu_port_setup);

void serial_hsu_port_free(struct uart_hsu_port *up)
{
	struct hsu_port_cfg *cfg = phsu->configs[up->index];

	uart_remove_one_port(&serial_hsu_reg, &up->port);
	free_irq(up->port.irq, up);
	if (cfg->has_alt) {
		struct hsu_port_cfg *alt_cfg = phsu->configs[cfg->alt];
		struct uart_hsu_port *alt_up =
			phsu->port + alt_cfg->index;
		uart_remove_one_port(&serial_hsu_reg, &alt_up->port);
		free_irq(up->port.irq, alt_up);
	}
}
EXPORT_SYMBOL(serial_hsu_port_free);

void serial_hsu_port_shutdown(struct uart_hsu_port *up)
{
	uart_suspend_port(&serial_hsu_reg, &up->port);
}
EXPORT_SYMBOL(serial_hsu_port_shutdown);

int serial_hsu_dma_setup(struct device *pdev,
	resource_size_t start, resource_size_t len, unsigned int irq, int share)
{
	struct hsu_dma_chan *dchan;
	int i, ret;

	phsu->reg = ioremap_nocache(start, len);
	dchan = phsu->chans;
	for (i = 0; i < 6; i++) {
		dchan->id = i;
		dchan->dirt = (i & 0x1) ? DMA_FROM_DEVICE :
			DMA_TO_DEVICE;
		dchan->uport = &phsu->port[i/2];
		dchan->reg = phsu->reg + HSU_DMA_CHANS_REG_OFFSET +
			i * HSU_DMA_CHANS_REG_LENGTH;

		dchan++;
	}

	/* will share irq with port if irq < 0 */
	if (share)
		phsu->irq_port_and_dma = 1;
	else {
		phsu->dma_irq = irq;
		ret = request_irq(irq, hsu_dma_irq, 0, "hsu dma", phsu);
		if (ret) {
			dev_err(pdev, "can not get dma IRQ\n");
			goto err;
		}
	}

	dev_set_drvdata(pdev, phsu);

	return 0;
err:
	iounmap(phsu->reg);
	return ret;
}
EXPORT_SYMBOL(serial_hsu_dma_setup);

void serial_hsu_dma_free(void)
{
	free_irq(phsu->dma_irq, phsu);
}
EXPORT_SYMBOL(serial_hsu_dma_free);

static int __init hsu_init(void)
{
	int ret;

	ret = uart_register_driver(&serial_hsu_reg);
	if (ret)
		return ret;

	spin_lock_init(&phsu->dma_lock);
	return hsu_debugfs_init(phsu);
}

static void __exit hsu_exit(void)
{
	uart_unregister_driver(&serial_hsu_reg);
	hsu_debugfs_remove(phsu);
}

module_init(hsu_init);
module_exit(hsu_exit);

MODULE_AUTHOR("Yang Bin <bin.yang@intel.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:medfield-hsu");
