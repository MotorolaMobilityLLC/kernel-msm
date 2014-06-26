/*
 * early_printk_intel_mid.c - early consoles for Intel MID platforms
 *
 * Copyright (c) 2008-2010, Intel Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

/*
 * Currently we have 3 types of early printk consoles: PTI, HSU and
 * MAX3110 SPI-UART.
 * PTI is available for mdfld, clv and mrfld.
 * HSU is available for mdfld, clv and mrfld. But it depends on board design.
 * Some boards don't have HSU UART pins routed to the connector so we can't
 * use it.
 * Max3110 SPI-UART is a stand-alone chip with SPI interface located in the
 * debug card. Drivers can access to this chip via Soc's SPI controller or SSP
 * controller(working in SPI mode).
 * Max3110 is available for mrst, mdfld, clv and mrfld. But for mrst, mdfld
 * and clv, MAX3110 is connected to SPI controller, for mrfld, MAX3110 is
 * connected to SSP controller.
 */

#include <linux/serial_reg.h>
#include <linux/serial_mfd.h>
#include <linux/kmsg_dump.h>
#include <linux/console.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/sched.h>
#include <linux/hardirq.h>
#include <linux/pti.h>

#include <asm/fixmap.h>
#include <asm/pgtable.h>
#include <asm/intel-mid.h>

#define MRST_SPI_TIMEOUT		0x200000
#define MRST_REGBASE_SPI0		0xff128000
#define MRST_REGBASE_SPI1		0xff128400
#define CLV_REGBASE_SPI1		0xff135000
#define MRST_CLK_SPI0_REG		0xff11d86c
#define MRFLD_SSP_TIMEOUT		0x200000
#define MRFLD_REGBASE_SSP5		0xff189000

/* Bit fields in CTRLR0 */
#define SPI_DFS_OFFSET			0

#define SPI_FRF_OFFSET			4
#define SPI_FRF_SPI			0x0
#define SPI_FRF_SSP			0x1
#define SPI_FRF_MICROWIRE		0x2
#define SPI_FRF_RESV			0x3

#define SPI_MODE_OFFSET			6
#define SPI_SCPH_OFFSET			6
#define SPI_SCOL_OFFSET			7
#define SPI_TMOD_OFFSET			8
#define	SPI_TMOD_TR			0x0		/* xmit & recv */
#define SPI_TMOD_TO			0x1		/* xmit only */
#define SPI_TMOD_RO			0x2		/* recv only */
#define SPI_TMOD_EPROMREAD		0x3		/* eeprom read mode */

#define SPI_SLVOE_OFFSET		10
#define SPI_SRL_OFFSET			11
#define SPI_CFS_OFFSET			12

/* Bit fields in SR, 7 bits */
#define SR_MASK				0x7f		/* cover 7 bits */
#define SR_BUSY				(1 << 0)
#define SR_TF_NOT_FULL			(1 << 1)
#define SR_TF_EMPT			(1 << 2)
#define SR_RF_NOT_EMPT			(1 << 3)
#define SR_RF_FULL			(1 << 4)
#define SR_TX_ERR			(1 << 5)
#define SR_DCOL				(1 << 6)

/* SR bit fields for SSP*/
#define SSP_SR_TF_NOT_FULL		(1 << 2)

static int ssp_timing_wr; /* Tangier A0 SSP timing workaround */

static unsigned int early_pti_console_channel;
static unsigned int early_pti_control_channel;

/* SPI controller registers */
struct dw_spi_reg {
	u32	ctrl0;
	u32	ctrl1;
	u32	ssienr;
	u32	mwcr;
	u32	ser;
	u32	baudr;
	u32	txfltr;
	u32	rxfltr;
	u32	txflr;
	u32	rxflr;
	u32	sr;
	u32	imr;
	u32	isr;
	u32	risr;
	u32	txoicr;
	u32	rxoicr;
	u32	rxuicr;
	u32	msticr;
	u32	icr;
	u32	dmacr;
	u32	dmatdlr;
	u32	dmardlr;
	u32	idr;
	u32	version;

	/* Currently operates as 32 bits, though only the low 16 bits matter */
	u32	dr;
} __packed;

/* SSP controler registers */
struct dw_ssp_reg {
	u32 ctrl0;
	u32 ctrl1;
	u32 sr;
	u32 ssitr;
	u32 dr;
} __packed;

#define dw_readl(dw, name)		__raw_readl(&(dw)->name)
#define dw_writel(dw, name, val)	__raw_writel((val), &(dw)->name)

/* Default use SPI0 register for mrst, we will detect Penwell and use SPI1 */
static unsigned long mrst_spi_paddr = MRST_REGBASE_SPI0;

static u32 *pclk_spi0;
/* Always contains an accessible address, start with 0 */
static struct dw_spi_reg *pspi;
static struct dw_ssp_reg *pssp;

static struct kmsg_dumper dw_dumper;
static int dumper_registered;

static void dw_kmsg_dump(struct kmsg_dumper *dumper,
			 enum kmsg_dump_reason reason)
{
	static char line[1024];
	size_t len;

	/* When run to this, we'd better re-init the HW */
	mrst_early_console_init();

	while (kmsg_dump_get_line(dumper, true, line, sizeof(line), &len))
		early_mrst_console.write(&early_mrst_console, line, len);
}

/* Set the ratio rate to 115200, 8n1, IRQ disabled */
static void max3110_spi_write_config(void)
{
	u16 config;

	config = 0xc001;
	dw_writel(pspi, dr, config);
}

/* Translate char to a eligible word and send to max3110 */
static void max3110_spi_write_data(char c)
{
	u16 data;

	data = 0x8000 | c;
	dw_writel(pspi, dr, data);
}

/* similar to max3110_spi_write_config, but via SSP controller */
static void max3110_ssp_write_config(void)
{
	u16 config;

	config = 0xc001;
	dw_writel(pssp, dr, config);
	dw_readl(pssp, dr);
	udelay(10);
	return;
}

/* similar to max3110_spi_write_data, but via SSP controller */
static void max3110_ssp_write_data(char c)
{
	u16 data;

	data = 0x8000 | c;
	dw_writel(pssp, dr, data);
	dw_readl(pssp, dr);
	udelay(10);
	return;
}

void mrst_early_console_init(void)
{
	u32 ctrlr0 = 0;
	u32 spi0_cdiv;
	u32 freq; /* Freqency info only need be searched once */

	/* Base clk is 100 MHz, the actual clk = 100M / (clk_divider + 1) */
	pclk_spi0 = (void *)set_fixmap_offset_nocache(FIX_EARLYCON_MEM_BASE,
							MRST_CLK_SPI0_REG);
	spi0_cdiv = ((*pclk_spi0) & 0xe00) >> 9;
	freq = 100000000 / (spi0_cdiv + 1);

	if (intel_mid_identify_cpu() == INTEL_MID_CPU_CHIP_PENWELL)
		mrst_spi_paddr = MRST_REGBASE_SPI1;
	else if (intel_mid_identify_cpu() == INTEL_MID_CPU_CHIP_CLOVERVIEW)
		mrst_spi_paddr = CLV_REGBASE_SPI1;

	pspi = (void *)set_fixmap_offset_nocache(FIX_EARLYCON_MEM_BASE,
						mrst_spi_paddr);

	/* Disable SPI controller */
	dw_writel(pspi, ssienr, 0);

	/* Set control param, 8 bits, transmit only mode */
	ctrlr0 = dw_readl(pspi, ctrl0);

	ctrlr0 &= 0xfcc0;
	ctrlr0 |= 0xf | (SPI_FRF_SPI << SPI_FRF_OFFSET)
		      | (SPI_TMOD_TO << SPI_TMOD_OFFSET);
	dw_writel(pspi, ctrl0, ctrlr0);

	/*
	 * Change the spi0 clk to comply with 115200 bps, use 100000 to
	 * calculate the clk dividor to make the clock a little slower
	 * than real baud rate.
	 */
	dw_writel(pspi, baudr, freq/100000);

	/* Disable all INT for early phase */
	dw_writel(pspi, imr, 0x0);

	/* Set the cs to spi-uart */
	dw_writel(pspi, ser, 0x2);

	/* Enable the HW, the last step for HW init */
	dw_writel(pspi, ssienr, 0x1);

	/* Set the default configuration */
	max3110_spi_write_config();

	/* Register the kmsg dumper */
	if (!dumper_registered) {
		dw_dumper.dump = dw_kmsg_dump;
		kmsg_dump_register(&dw_dumper);
		dumper_registered = 1;
	}
}

/* Slave select should be called in the read/write function */
static void early_mrst_spi_putc(char c)
{
	unsigned int timeout;
	u32 sr;

	timeout = MRST_SPI_TIMEOUT;
	/* Early putc needs to make sure the TX FIFO is not full */
	while (--timeout) {
		sr = dw_readl(pspi, sr);
		if (!(sr & SR_TF_NOT_FULL))
			cpu_relax();
		else
			break;
	}

	if (!timeout)
		pr_warn("MRST earlycon: timed out\n");
	else
		max3110_spi_write_data(c);
}

/* Early SPI only uses polling mode */
static void early_mrst_spi_write(struct console *con, const char *str,
				unsigned n)
{
	int i;

	for (i = 0; i < n && *str; i++) {
		if (*str == '\n')
			early_mrst_spi_putc('\r');
		early_mrst_spi_putc(*str);
		str++;
	}
}

struct console early_mrst_console = {
	.name =		"earlymrst",
	.write =	early_mrst_spi_write,
	.flags =	CON_PRINTBUFFER,
	.index =	-1,
};

void mrfld_early_console_init(void)
{
	u32 ctrlr0 = 0;

	set_fixmap_nocache(FIX_EARLYCON_MEM_BASE, MRFLD_REGBASE_SSP5);

	pssp = (void *)(__fix_to_virt(FIX_EARLYCON_MEM_BASE) +
			(MRFLD_REGBASE_SSP5 & (PAGE_SIZE - 1)));

	if (intel_mid_identify_sim() == INTEL_MID_CPU_SIMULATION_NONE)
		ssp_timing_wr = 1;

	/* mask interrupts, clear enable and set DSS config */
	/* SSPSCLK on active transfers only */
	if (intel_mid_identify_sim() != INTEL_MID_CPU_SIMULATION_SLE) {
		if (ssp_timing_wr) {
			dw_writel(pssp, ctrl0, 0xc12c0f);
			dw_writel(pssp, ctrl1, 0x0);
		} else {
			dw_writel(pssp, ctrl0, 0xc0000f);
			dw_writel(pssp, ctrl1, 0x10000000);
		}
	}

	dw_readl(pssp, sr);

	/* enable port */
	if (intel_mid_identify_sim() != INTEL_MID_CPU_SIMULATION_SLE) {
		ctrlr0 = dw_readl(pssp, ctrl0);
		ctrlr0 |= 0x80;
		dw_writel(pssp, ctrl0, ctrlr0);
	}

}

/* slave select should be called in the read/write function */
static int early_mrfld_putc(char c)
{
	unsigned int timeout;
	u32 sr;

	timeout = MRFLD_SSP_TIMEOUT;
	/* early putc need make sure the TX FIFO is not full*/
	while (timeout--) {
		sr = dw_readl(pssp, sr);
		if (ssp_timing_wr) {
			if (sr & 0xF00)
				cpu_relax();
			else
				break;
		} else {
			if (!(sr & SSP_SR_TF_NOT_FULL))
				cpu_relax();
			else
				break;
		}
	}

	if (timeout == 0xffffffff) {
		pr_info("SSP: waiting timeout\n");
		return -1;
	}

	max3110_ssp_write_data(c);
	return 0;
}

static void early_mrfld_write(struct console *con,
				const char *str, unsigned n)
{
	int  i;

	for (i = 0; i < n && *str; i++) {
		if (*str == '\n')
			early_mrfld_putc('\r');
		early_mrfld_putc(*str);

		str++;
	}
}

struct console early_mrfld_console = {
	.name =		"earlymrfld",
	.write =	early_mrfld_write,
	.flags =	CON_PRINTBUFFER,
	.index =	-1,
};

void mrfld_early_printk(const char *fmt, ...)
{
	char buf[512];
	int n;
	va_list ap;

	va_start(ap, fmt);
	n = vscnprintf(buf, 512, fmt, ap);
	va_end(ap);

	early_mrfld_console.write(&early_mrfld_console, buf, n);
}

/*
 * Following is the early console based on High Speed UART device.
 */
#define MERR_HSU_PORT_BASE	0xff010180
#define MERR_HSU_CLK_CTL	0xff00b830
#define MFLD_HSU_PORT_BASE	0xffa28080

static void __iomem *phsu;

void hsu_early_console_init(const char *s)
{
	unsigned long paddr, port = 0;
	u8 lcr;
	int *clkctl;

	if (intel_mid_identify_cpu() == INTEL_MID_CPU_CHIP_TANGIER) {
		paddr = MERR_HSU_PORT_BASE;
		clkctl = (int *)set_fixmap_offset_nocache(FIX_CLOCK_CTL,
							  MERR_HSU_CLK_CTL);
	} else {
		paddr = MFLD_HSU_PORT_BASE;
		clkctl = NULL;
	}

	/*
	 * Select the early HSU console port if specified by user in the
	 * kernel command line.
	 */
	if (*s && !kstrtoul(s, 10, &port))
		port = clamp_val(port, 0, 2);

	paddr += port * 0x80;
	phsu = (void *)set_fixmap_offset_nocache(FIX_EARLYCON_MEM_BASE, paddr);

	/* Disable FIFO */
	writeb(0x0, phsu + UART_FCR);

	/* Set to default 115200 bps, 8n1 */
	lcr = readb(phsu + UART_LCR);
	writeb((0x80 | lcr), phsu + UART_LCR);
	writeb(0x01, phsu + UART_DLL);
	writeb(0x00, phsu + UART_DLM);
	writeb(lcr,  phsu + UART_LCR);
	writel(0x0010, phsu + UART_ABR * 4);
	writel(0x0010, phsu + UART_PS * 4);

	if (intel_mid_identify_cpu() == INTEL_MID_CPU_CHIP_TANGIER) {
		/* detect HSU clock is 50M or 19.2M */
		if (clkctl && *clkctl & (1 << 16))
			writel(0x0120, phsu + UART_MUL * 4); /* for 50M */
		else
			writel(0x05DC, phsu + UART_MUL * 4);  /* for 19.2M */
	} else
		writel(0x0240, phsu + UART_MUL * 4);

	writel(0x3D09, phsu + UART_DIV * 4);

	writeb(0x8, phsu + UART_MCR);
	writeb(0x7, phsu + UART_FCR);
	writeb(0x3, phsu + UART_LCR);

	/* Clear IRQ status */
	readb(phsu + UART_LSR);
	readb(phsu + UART_RX);
	readb(phsu + UART_IIR);
	readb(phsu + UART_MSR);

	/* Enable FIFO */
	writeb(0x7, phsu + UART_FCR);
}

#define BOTH_EMPTY (UART_LSR_TEMT | UART_LSR_THRE)

static void early_hsu_putc(char ch)
{
	unsigned int timeout = 10000; /* 10ms */
	u8 status;

	while (--timeout) {
		status = readb(phsu + UART_LSR);
		if (status & BOTH_EMPTY)
			break;
		udelay(1);
	}

	/* Only write the char when there was no timeout */
	if (timeout)
		writeb(ch, phsu + UART_TX);
}

static void early_hsu_write(struct console *con, const char *str, unsigned n)
{
	int i;

	for (i = 0; i < n && *str; i++) {
		if (*str == '\n')
			early_hsu_putc('\r');
		early_hsu_putc(*str);
		str++;
	}
}

struct console early_hsu_console = {
	.name =		"earlyhsu",
	.write =	early_hsu_write,
	.flags =	CON_PRINTBUFFER,
	.index =	-1,
};

void hsu_early_printk(const char *fmt, ...)
{
	char buf[512];
	int n;
	va_list ap;

	va_start(ap, fmt);
	n = vscnprintf(buf, 512, fmt, ap);
	va_end(ap);

	early_hsu_console.write(&early_hsu_console, buf, n);
}

#define PTI_ADDRESS		0xfd800000
#define CONTROL_FRAME_LEN 32    /* PTI control frame maximum size */

static void early_pti_write_to_aperture(struct pti_masterchannel *mc,
					 u8 *buf, int len)
{
	int dwordcnt, final, i;
	u32 ptiword;
	u8 *p ;
	u32 pti_phys_address ;
	u32 __iomem *aperture;

	p = buf;

	/*
	   calculate the aperture offset from the base using the master and
	   channel id's.
	*/
	pti_phys_address = PTI_ADDRESS +
				(mc->master << 15) + (mc->channel << 8);

	set_fixmap_nocache(FIX_EARLYCON_MEM_BASE, pti_phys_address);
	aperture = (void *)(__fix_to_virt(FIX_EARLYCON_MEM_BASE) +
				(pti_phys_address & (PAGE_SIZE - 1)));

	dwordcnt = len >> 2;
	final = len - (dwordcnt << 2);		/* final = trailing bytes */
	if (final == 0 && dwordcnt != 0) {	/* always have a final dword */
		final += 4;
		dwordcnt--;
	}

	for (i = 0; i < dwordcnt; i++) {
		ptiword = be32_to_cpu(*(u32 *)p);
		p += 4;
		iowrite32(ptiword, aperture);
	}

	aperture += PTI_LASTDWORD_DTS;	/* adding DTS signals that is EOM */
	ptiword = 0;

	for (i = 0; i < final; i++)
		ptiword |= *p++ << (24-(8*i));

	iowrite32(ptiword, aperture);

	return;
}

static int pti_early_console_init(void)
{
	early_pti_console_channel = 0;
	early_pti_control_channel = 0;
	return 0;
}

static void early_pti_write(struct console *con,
			const char *str, unsigned n)
{
	static struct pti_masterchannel mccontrol = {.master = 72,
						     .channel = 0};
	static struct pti_masterchannel mcconsole = {.master = 73,
						     .channel = 0};
	const char *control_format = "%3d %3d %s";

	/*
	 * Since we access the comm member in current's task_struct,
	 * we only need to be as large as what 'comm' in that
	 * structure is.
	 */
	char comm[TASK_COMM_LEN];
	u8 control_frame[CONTROL_FRAME_LEN];

	/* task information */
	if (in_irq())
		strncpy(comm, "hardirq", sizeof(comm));
	else if (in_softirq())
		strncpy(comm, "softirq", sizeof(comm));
	else
		strncpy(comm, current->comm, sizeof(comm));

	/* Absolutely ensure our buffer is zero terminated */
	comm[TASK_COMM_LEN-1] = 0;

	mccontrol.channel = early_pti_control_channel;
	early_pti_control_channel = (early_pti_control_channel + 1) & 0x7f;

	mcconsole.channel = early_pti_console_channel;
	early_pti_console_channel = (early_pti_console_channel + 1) & 0x7f;

	snprintf(control_frame, CONTROL_FRAME_LEN, control_format,
		mcconsole.master, mcconsole.channel, comm);

	early_pti_write_to_aperture(&mccontrol, control_frame,
					strlen(control_frame));
	early_pti_write_to_aperture(&mcconsole, (u8 *)str, n);

}

struct console early_pti_console = {
	.name =		"earlypti",
	.early_setup =  pti_early_console_init,
	.write =	early_pti_write,
	.flags =	CON_PRINTBUFFER,
	.index =	-1,
};

void pti_early_printk(const char *fmt, ...)
{
	char buf[512];
	int n;
	va_list ap;

	va_start(ap, fmt);
	n = vscnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	early_pti_console.write(&early_pti_console, buf, n);
}
