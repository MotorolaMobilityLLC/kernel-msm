/*
 * intel_soc_dump.c - This driver provides a debugfs interface to read or
 * write any registers inside the SoC. Supported access methods are:
 * mmio, msg_bus, pci and i2c.
 *
 * Copyright (c) 2012, Intel Corporation.
 * Author: Bin Gao <bin.gao@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

/*
 * Two files are created in debugfs root folder: dump_cmd and dump_output.
 * Echo a dump command to the file dump_cmd, and then cat the file dump_output.
 * Even for write command, you still have to run "cat dump_output", otherwise
 * the data will not be really written.
 *
 * It works like this:
 * $ echo "dump command" > dump_cmd
 * $ cat dump_output
 *
 * I/O memory read: echo "r[1|2|4] mmio <addr> [<len>]" > dump_cmd
 *     e.g.  echo "r mmio 0xff180000" > dump_cmd
 *
 * I/O memory write: echo "w[1|2|4] <addr> <val>" > dump_cmd
 *     e.g.  echo "w mmio 0xff190000 0xf0107a08" > dump_cmd
 *
 * I/O port read: echo "r[1|2|4] port <port>" > dump_cmd
 *     e.g.  echo "r port 0xcf8" > dump_cmd
 *
 * I/O port write: echo "w[1|2|4] <port> <val>" > dump_cmd
 *     e.g.  echo "w4 port 0xcfc 0x80002188" > dump_cmd
 *
 * message bus read: echo "r msg_bus <port> <addr> [<len>]" > dump_cmd
 *     e.g.  echo "r msg_bus 0x02 0x30" > dump_cmd
 *
 * message bus write: echo "w msg_bus <port> <addr> <val>" > dump_cmd
 *     e.g.  echo "w msg_bus 0x02 0x30 0x1020003f" > dump_cmd
 *
 * pci config read: echo "r[1|2|4] pci <bus> <dev> <func> <reg> [<len>]" >
 * dump_cmd
 *     e.g.  echo "r1 pci 0 2 0 0x20" > dump_cmd
 *
 * pci config write: echo "w[1|2|4] pci <bus> <dev> <func> <reg> <value>" >
 * dump_cmd
 *     e.g.  echo "w pci 0 2 0 0x20 0x380020f3" > dump_cmd
 *
 * msr read: echo "r[4|8]  msr [<cpu>|all] <reg>" > dump_cmd
 * read cab be 32bit(r4) or 64bit(r8), default is r8 (=r)
 * cpu can be 0, 1, 2, 3, ... or all, default is all
 *     e.g.  echo "r msr 0 0xcd" > dump_cmd
 *     (read all cpu's msr reg 0xcd in 64bit mode)
 *
 * msr write: echo "w[4|8] msr [<cpu>|all] <reg> <val>" > dump_cmd
 * write cab be 32bit(w4) or 64bit(w8), default is w8 (=w)
 * cpu can be 0, 1, 2, 3, ... or all, default is all
 *     e.g.  echo "w msr 1 289 0xf03090a0cc73be64" > dump_cmd
 *     (write value 0xf03090a0cc73be64 to cpu 1's msr reg 289 in 64bit mode)
 *
 * i2c read:  echo "r i2c <bus> <addr>" > dump_cmd
 *     e.g.  echo "r i2c 1 0x3e" > dump_cmd
 *
 * i2c write: echo "w i2c <bus> <addr> <val>" > dump_cmd
 *      e.g.  echo "w i2c 2 0x70 0x0f" > dump_cmd
 *
 * SCU indirect memory read: echo "r[4] scu <addr>" > dump_cmd
 *     e.g.  echo "r scu 0xff108194" > dump_cmd
 *
 * SCU indirect memory write: echo "w[4] scu <addr> <val>" > dump_cmd
 *     e.g.  echo "w scu 0xff108194 0x03000001" > dump_cmd
 *
 *  SCU indirect read/write is limited to those addresses in
 *  IndRdWrValidAddrRange array in SCU FW.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/seq_file.h>
#include <linux/i2c.h>
#include <linux/pm_runtime.h>
#include <asm/uaccess.h>
#include <asm/intel-mid.h>
#include <asm/processor.h>
#include <asm/msr.h>
#include <asm/intel_mid_rpmsg.h>

#define MAX_CMDLEN		96
#define MAX_ERRLEN		255
#define MIN_ARGS_NUM		3
#define MAX_ARGS_NUM		8
#define MAX_MMIO_PCI_LEN	4096
#define MAX_MSG_BUS_LEN		64

#define ACCESS_WIDTH_DEFAULT	0
#define ACCESS_WIDTH_8BIT	1
#define ACCESS_WIDTH_16BIT	2
#define ACCESS_WIDTH_32BIT	4
#define ACCESS_WIDTH_64BIT	8

#define ACCESS_BUS_MMIO		1 /* I/O memory */
#define ACCESS_BUS_PORT		2 /* I/O port */
#define ACCESS_BUS_MSG_BUS	3 /* message bus */
#define ACCESS_BUS_PCI		4 /* PCI bus */
#define ACCESS_BUS_MSR		5 /* MSR registers */
#define ACCESS_BUS_I2C		6 /* I2C bus */
#define ACCESS_BUS_SCU_INDRW	7 /* SCU indirect read/write */

#define ACCESS_DIR_READ		1
#define ACCESS_DIR_WRITE	2

#define RP_INDIRECT_READ	0x02 /* MSG_ID for indirect read via SCU */
#define RP_INDIRECT_WRITE	0x05 /* MSG_ID for indirect write via SCU */

#define SHOW_NUM_PER_LINE	(32 / access_width)
#define LINE_WIDTH		(access_width * SHOW_NUM_PER_LINE)
#define IS_WHITESPACE(c)	((c) == ' ' || (c) == '\t' || (c) == '\n')
#define ADDR_RANGE(start, size, addr) \
	((addr >= start) && (addr < (start + size)))

/* mmio <--> device map */
struct mmio_pci_map {
	u32 start;
	size_t size;
	u32 pci_bus:8;
	u32 pci_dev:8;
	u32 pci_func:8;
	char name[24];
};

static struct dentry *dump_cmd_dentry, *dump_output_dentry;
static int dump_cmd_was_set;
static char dump_cmd_buf[MAX_CMDLEN], err_buf[MAX_ERRLEN + 1];

static int access_dir, access_width, access_bus, access_len;
static u32 access_value;
static u64 access_value_64;

/* I/O memory */
static u32 mmio_addr;

/* I/O port */
static unsigned port_addr;

/* msg_bus */
static u8 msg_bus_port;
static u32 msg_bus_addr;

/* pci */
static u8 pci_bus, pci_dev, pci_func;
static u16 pci_reg;

/* msr */
static int msr_cpu;
static u32 msr_reg;

/* i2c */
static u8 i2c_bus;
static u32 i2c_addr;

/* scu */
static u32 scu_addr;

static const struct mmio_pci_map soc_pnw_map[] = {
	{ 0xff128000, 0x400, 0, 0, 1, "SPI0" },
	{ 0xff128400, 0x400, 0, 0, 2, "SPI1" },
	{ 0xff128800, 0x400, 0, 2, 4, "SPI2" },

	{ 0xff12a000, 0x400, 0, 0, 3, "I2C0" },
	{ 0xff12a400, 0x400, 0, 0, 4, "I2C1" },
	{ 0xff12a800, 0x400, 0, 0, 5, "I2C2" },
	{ 0xff12ac00, 0x400, 0, 3, 2, "I2C3" },
	{ 0xff12b000, 0x400, 0, 3, 3, "I2C4" },
	{ 0xff12b400, 0x400, 0, 3, 4, "I2C5" },

	{ 0xffae5800, 0x400, 0, 2, 7, "SSP0" },
	{ 0xffae6000, 0x400, 0, 1, 4, "SSP1" },
	{ 0xffae6400, 0x400, 0, 1, 3, "SSP2" },
	{ 0xffaf0000, 0x800, 0, 2, 6, "LPE DMA1" },

	{ 0xff0d0000, 0x10000, 0, 1, 5, "SEP SECURITY" },
	{ 0xff11c000, 0x400, 0, 1, 7, "SCU IPC1" },

	{ 0xdff00000, 0x100000, 0, 2, 0, "GVD BAR0" },
	{ 0x40000000, 0x10000000, 0, 2, 0, "GVD BAR2" },
	{ 0xdfec0000, 0x40000, 0, 2, 0, "GVD BAR3" },

	{ 0xff11d000, 0x1000, 0, 2, 2, "PMU" },
	{ 0xffa60000, 0x20000, 0, 2, 3, "USB OTG" },

	{ 0xdf800000, 0x400000, 0, 3, 0, "ISP" },

	{ 0xff12c000, 0x800, 0, 2, 1, "GPIO0" },
	{ 0xff12c800, 0x800, 0, 3, 5, "GPIO1" },
	{ 0xff12b800, 0x800, 0, 2, 5, "GP DMA" },

	{ 0xffa58000, 0x100, 0, 4, 0, "SDIO0(HC2)" },
	{ 0xffa5c000, 0x100, 0, 4, 1, "SDIO1(HC1a)" },
	{ 0xffa2a000, 0x100, 0, 4, 2, "SDIO3(HC1b)" },
	{ 0xffa50000, 0x100, 0, 1, 0, "SDIO3/eMMC0(HC0a)" },
	{ 0xffa54000, 0x100, 0, 1, 1, "SDIO4/eMMC1(HC0b)" },

	{ 0xffa28080, 0x80, 0, 5, 0, "UART0" },
	{ 0xffa28100, 0x80, 0, 5, 1, "UART1" },
	{ 0xffa28180, 0x80, 0, 5, 2, "UART2" },
	{ 0xffa28400, 0x400, 0, 5, 3, "UART DMA" },

	{ 0xffa2e000, 0x400, 0, 6, 0, "PTI" },

	/* no address assigned:	{ 0x0, 0, 0, 6, 1, "xx" }, */

	{ 0xffa29000, 0x800, 0, 6, 3, "HSI" },
	{ 0xffa29800, 0x800, 0, 6, 4, "HSI DMA" },
};

static const struct mmio_pci_map soc_clv_map[] = {
	{ 0xff138000, 0x400, 0, 0, 3, "I2C0" },
	{ 0xff139000, 0x400, 0, 0, 4, "I2C1" },
	{ 0xff13a000, 0x400, 0, 0, 5, "I2C2" },
	{ 0xff13b000, 0x400, 0, 3, 2, "I2C3" },
	{ 0xff13c000, 0x400, 0, 3, 3, "I2C4" },
	{ 0xff13d000, 0x400, 0, 3, 4, "I2C5" },

	{ 0xff128000, 0x400, 0, 0, 1, "SPI0/MSIC" },
	{ 0xff135000, 0x400, 0, 0, 2, "SPI1" },
	{ 0xff136000, 0x400, 0, 2, 4, "SPI2" },
	/* invisible to IA: { 0xff137000, 0, -1, -1, -1, "SPI3" }, */

	{ 0xffa58000, 0x100, 0, 4, 0, "SDIO0 (HC2)" },
	{ 0xffa48000, 0x100, 0, 4, 1, "SDIO1 (HC1a)" },
	{ 0xffa4c000, 0x100, 0, 4, 2, "SDIO2 (HC1b)" },
	{ 0xffa50000, 0x100, 0, 1, 0, "SDIO3/eMMC0 (HC0a)" },
	{ 0xffa54000, 0x100, 0, 1, 1, "SDIO4/eMMC1 (HC0b)" },

	{ 0xff119000, 0x800, 0, 2, 1, "GPIO0" },
	{ 0xff13f000, 0x800, 0, 3, 5, "GPIO1" },
	{ 0xff13e000, 0x800, 0, 2, 5, "GP DMA" },

	{ 0xffa20000, 0x400, 0, 2, 7, "SSP0" },
	{ 0xffa21000, 0x400, 0, 1, 4, "SSP1" },
	{ 0xffa22000, 0x400, 0, 1, 3, "SSP2" },
	/* invisible to IA: { 0xffa23000, 0, -1, -1, -1, "SSP3" }, */

	/* invisible to IA: { 0xffaf8000, 0, -1, -1, -1, "LPE DMA0" }, */
	{ 0xffaf0000, 0x800, 0, 2, 6, "LPE DMA1" },
	{ 0xffae8000, 0x1000, 0, 1, 3, "LPE SHIM" },
	/* { 0xffae9000, 0, 0, 6, 5, "VIBRA" }, LPE SHIM BASE + 0x1000 */

	{ 0xffa28080, 0x80, 0, 5, 0, "UART0" },
	{ 0xffa28100, 0x80, 0, 5, 1, "UART1" },
	{ 0xffa28180, 0x80, 0, 5, 2, "UART2" },
	{ 0xffa28400, 0x400, 0, 5, 3, "UART DMA" },

	{ 0xffa29000, 0x800, 0, 6, 3, "HSI" },
	{ 0xffa2a000, 0x800, 0, 6, 4, "HSI DMA" },

	{ 0xffa60000, 0x20000, 0, 2, 3, "USB OTG" },
	{ 0xffa80000, 0x60000, 0, 6, 5, "USB SPH" },

	{ 0xff0d0000, 0x10000, 0, 1, 5, "SEP SECURITY" },

	{ 0xdff00000, 0x100000, 0, 2, 0, "GVD BAR0" },
	{ 0x40000000, 0x10000000, 0, 2, 0, "GVD BAR2" },
	{ 0xdfec0000, 0x40000, 0, 2, 0, "GVD BAR3" },
	/* No address assigned: { 0x0, 0, 0, 6, 1, "HDMI HOTPLUG" }, */

	{ 0xdf800000, 0x400000, 0, 3, 0, "ISP" },

	{ 0xffa2e000, 0x400, 0, 6, 0, "PTI" },
	{ 0xff11c000, 0x400, 0, 1, 7, "SCU IPC1" },
	{ 0xff11d000, 0x1000, 0, 2, 2, "PMU" },
};

static const struct mmio_pci_map soc_tng_map[] = {
	/* I2C0 is reserved for SCU<-->PMIC communication */
	{ 0xff18b000, 0x400, 0, 8, 0, "I2C1" },
	{ 0xff18c000, 0x400, 0, 8, 1, "I2C2" },
	{ 0xff18d000, 0x400, 0, 8, 2, "I2C3" },
	{ 0xff18e000, 0x400, 0, 8, 3, "I2C4" },
	{ 0xff18f000, 0x400, 0, 9, 0, "I2C5" },
	{ 0xff190000, 0x400, 0, 9, 1, "I2C6" },
	{ 0xff191000, 0x400, 0, 9, 2, "I2C7" },

	/* SDIO controllers number: 4 (compared to 5 of PNW/CLV) */
	{ 0xff3fa000, 0x100, 0, 1, 2, "SDIO0 (HC2)" },
	{ 0xff3fb000, 0x100, 0, 1, 3, "SDIO1 (HC1a)" },
	{ 0xff3fc000, 0x100, 0, 1, 0, "SDIO3/eMMC0 (HC0a)" },
	{ 0xff3fd000, 0x100, 0, 1, 1, "SDIO4/eMMC1 (HC0b)" },

	/* GPIO0 and GPIO1 are merged to one GPIO controller in TNG */
	{ 0xff008000, 0x1000, 0, 12, 0, "GPIO" },
	{ 0xff192000, 0x1000, 0, 21, 0, "GP DMA" },

	/* SSP Audio: SSP0: Modem, SSP1: Audio Codec, SSP2: Bluetooth */

	/* LPE */
	{ 0xff340000, 0x4000, 0, 13, 0, "LPE SHIM" },
	{ 0xff344000, 0x1000, 0, 13, 0, "MAILBOX RAM" },
	{ 0xff2c0000, 0x14000, 0, 13, 0, "ICCM" },
	{ 0xff300000, 0x28000, 0, 13, 0, "DCCM" },
	{ 0xff298000, 0x4000, 0, 14, 0, "LPE DMA0" },
	/* invisible to IA: { 0xff29c000, 0x4000, -1, -1, -1, "LPE DMA1" }, */


	/* SSP SC: SSP4: used by SCU for SPI Debug Card */
	/* invisible to IA: { 0xff00e000, 0x1000, -1, -1, -1, "SSP SC" }, */

	/* SSP General Purpose */
	{ 0xff188000, 0x1000, 0, 7, 0, "SSP3" },
	{ 0xff189000, 0x1000, 0, 7, 1, "SSP5" },
	{ 0xff18a000, 0x1000, 0, 7, 2, "SSP6" },

	/* UART */
	{ 0xff010080, 0x80, 0, 4, 1, "UART0" },
	{ 0xff011000, 0x80, 0, 4, 2, "UART1" },
	{ 0xff011080, 0x80, 0, 4, 3, "UART2" },
	{ 0xff011400, 0x400, 0, 5, 0, "UART DMA" },

	/* HSI */
	{ 0xff3f8000, 0x1000, 0, 10, 0, "HSI" },

	/* USB */
	{ 0xf9040000, 0x20000, 0, 15, 0, "USB2 OTG" },
	{ 0xf9060000, 0x20000, 0, 16, 0, "USB2 MPH/HSIC" },
	{ 0xf9100000, 0x100000, 0, 17, 0, "USB3 OTG" },
	/* { 0xf90f0000, 0x1000, -1, -1, -1, "USB3 PHY" }, */
	/* { 0xf90a0000, 0x10000, -1, -1, -1, "USB3 DMA FETCH" }, */

	/* Security/Chaabi */
	{ 0xf9030000, 0x1000, 0, 11, 0, "SEP SECURITY" },

	/* Graphics/Display */
	{ 0xc0000000, 0x2000000, 0, 2, 0, "GVD BAR0" },
	{ 0x80000000, 0x10000000, 0, 2, 0, "GVD BAR2" },

	/* ISP */
	{ 0xc2000000, 0x400000, 0, 3, 0, "ISP" },

	/* PTI */
	{ 0xf9009000, 0x1000, 0, 18, 0, "PTI STM" },
	{ 0xf90a0000, 0x10000, 0, 18, 0, "PTI USB3 DMA FETCH" },
	{ 0xfa000000, 0x1000000, 0, 18, 0, "PTI APERTURE A" },

	{ 0xff009000, 0x1000, 0, 19, 0, "SCU-IA IPC" },
	{ 0xff00b000, 0x1000, 0, 20, 0, "PMU" },
};

static struct pci_dev *mmio_to_pci(u32 addr, char **name)
{
	int i, count;
	struct mmio_pci_map *map;

	if (intel_mid_identify_cpu() == INTEL_MID_CPU_CHIP_PENWELL) {
		count = ARRAY_SIZE(soc_pnw_map);
		map = (struct mmio_pci_map *) &soc_pnw_map[0];
	} else if (intel_mid_identify_cpu() == INTEL_MID_CPU_CHIP_CLOVERVIEW) {
		count = ARRAY_SIZE(soc_clv_map);
		map = (struct mmio_pci_map *) &soc_clv_map[0];
	} else if (intel_mid_identify_cpu() == INTEL_MID_CPU_CHIP_TANGIER) {
		count = ARRAY_SIZE(soc_tng_map);
		map = (struct mmio_pci_map *) &soc_tng_map[0];
	} else {
		return NULL;
	}

	for (i = 0; i < count; i++) {
		if (ADDR_RANGE(map[i].start, map[i].size, addr))
			break;
	}

	if (i >= count)
		return NULL;

	*name = &map[i].name[0];
	return pci_get_bus_and_slot(map[i].pci_bus,
		PCI_DEVFN(map[i].pci_dev, map[i].pci_func));
}

static int parse_argument(char *input, char **args)
{
	int count, located;
	char *p = input;
	int input_len = strlen(input);

	count = 0;
	located = 0;
	while (*p != 0) {
		if (p - input >= input_len)
			break;

		/* Locate the first character of a argument */
		if (!IS_WHITESPACE(*p)) {
			if (!located) {
				located = 1;
				args[count++] = p;
				if (count > MAX_ARGS_NUM)
					break;
			}
		} else {
			if (located) {
				*p = 0;
				located = 0;
			}
		}
		p++;
	}

	return count;
}

static int dump_cmd_show(struct seq_file *s, void *unused)
{
	seq_printf(s, dump_cmd_buf);
	return 0;
}

static int dump_cmd_open(struct inode *inode, struct file *file)
{
	return single_open(file, dump_cmd_show, NULL);
}

static int parse_mmio_args(char **arg_list, int arg_num)
{
	int ret;

	if (arg_num < 3) {
		snprintf(err_buf, MAX_ERRLEN, "too few arguments\n"
			"usage: r[1|2|4] <mmio> <addr> [<len>]\n"
			"       w[1|2|4] <mmio> <addr> <val>\n");
		goto failed;
	}

	if (access_width == ACCESS_WIDTH_DEFAULT)
		access_width = ACCESS_WIDTH_32BIT;

	ret = kstrtou32(arg_list[2], 0, &mmio_addr);
	if (ret) {
		snprintf(err_buf, MAX_ERRLEN, "invalid mmio address %s\n",
							 arg_list[2]);
		goto failed;
	}

	if ((access_width == ACCESS_WIDTH_32BIT) &&
		(mmio_addr % 4)) {
		snprintf(err_buf, MAX_ERRLEN,
			"addr %x is not 4 bytes aligned!\n",
						mmio_addr);
		goto failed;
	}

	if ((access_width == ACCESS_WIDTH_16BIT) &&
		(mmio_addr % 2)) {
		snprintf(err_buf, MAX_ERRLEN,
			"addr %x is not 2 bytes aligned!\n",
						mmio_addr);
		goto failed;
	}

	if (access_dir == ACCESS_DIR_READ) {
		if (arg_num == 4) {
			ret = kstrtou32(arg_list[3], 0, &access_len);
			if (ret) {
				snprintf(err_buf, MAX_ERRLEN,
					"invalid mmio read length %s\n",
							arg_list[3]);
				goto failed;
			}
		} else if (arg_num > 4) {
			snprintf(err_buf, MAX_ERRLEN,
				"usage: r[1|2|4] mmio <addr> "
						"[<len>]\n");
			goto failed;
		}
	}

	if (access_dir == ACCESS_DIR_WRITE) {
		if (arg_num != 4) {
			snprintf(err_buf, MAX_ERRLEN,
				"need exact 4 arguments for "
					"mmio write.\n");
			goto failed;
		}
		ret = kstrtou32(arg_list[3], 0, &access_value);
		if (ret) {
			snprintf(err_buf, MAX_ERRLEN,
				"invalid mmio address %s\n",
						arg_list[3]);
			goto failed;
		}
	}

	return 0;

failed:
	return -EINVAL;
}

static int parse_port_args(char **arg_list, int arg_num)
{
	int ret;

	if (arg_num < 2) {
		snprintf(err_buf, MAX_ERRLEN, "too few arguments\n"
			"usage: r[1|2|4] port <port>\n"
			"       w[1|2|4] port <port> <val>\n");
		goto failed;
	}

	if (access_width == ACCESS_WIDTH_DEFAULT)
		access_width = ACCESS_WIDTH_8BIT;

	ret = kstrtou16(arg_list[2], 0, (u16 *)&port_addr);
	if (ret) {
		snprintf(err_buf, MAX_ERRLEN, "invalid port address %s\n",
							 arg_list[2]);
		goto failed;
	}

	if ((access_width == ACCESS_WIDTH_32BIT) &&
		(port_addr % ACCESS_WIDTH_32BIT)) {
		snprintf(err_buf, MAX_ERRLEN,
			"port %x is not 4 bytes aligned!\n", port_addr);
		goto failed;
	}

	if ((access_width == ACCESS_WIDTH_16BIT) &&
		(port_addr % ACCESS_WIDTH_16BIT)) {
		snprintf(err_buf, MAX_ERRLEN,
			"port %x is not 2 bytes aligned!\n", port_addr);
		goto failed;
	}

	if (access_dir == ACCESS_DIR_READ) {
		if (arg_num != 3) {
			snprintf(err_buf, MAX_ERRLEN,
				"usage: r[1|2|4] port <port>\n");
			goto failed;
		}
	}

	if (access_dir == ACCESS_DIR_WRITE) {
		if (arg_num != 4) {
			snprintf(err_buf, MAX_ERRLEN,
				"need exact 4 arguments for port write.\n");
			goto failed;
		}
		ret = kstrtou32(arg_list[3], 0, &access_value);
		if (ret) {
			snprintf(err_buf, MAX_ERRLEN,
				"invalid value %s\n", arg_list[3]);
			goto failed;
		}
	}

	return 0;

failed:
	return -EINVAL;
}

static int parse_msg_bus_args(char **arg_list, int arg_num)
{
	int ret;

	if (arg_num < 4) {
		snprintf(err_buf, MAX_ERRLEN, "too few arguments\n"
			"usage: r msg_bus <port> <addr> [<len>]\n"
			"       w msg_bus <port> <addr> <val>\n");
		goto failed;
	}

	if (access_width == ACCESS_WIDTH_DEFAULT)
		access_width = ACCESS_WIDTH_32BIT;

	if (access_width != ACCESS_WIDTH_32BIT) {
		snprintf(err_buf, MAX_ERRLEN,
			"only 32bit read/write are supported.\n");
		goto failed;
	}

	ret = kstrtou8(arg_list[2], 0, &msg_bus_port);
	if (ret || msg_bus_port > 255) {
		snprintf(err_buf, MAX_ERRLEN, "invalid msg_bus port %s\n",
								arg_list[2]);
		goto failed;
	}

	ret = kstrtou32(arg_list[3], 0, &msg_bus_addr);
	if (ret) {
		snprintf(err_buf, MAX_ERRLEN, "invalid msg_bus address %s\n",
								arg_list[3]);
		goto failed;
	}

	if (access_dir == ACCESS_DIR_READ) {
		if (arg_num == 5) {
			ret = kstrtou32(arg_list[4], 0, &access_len);
			if (ret) {
				snprintf(err_buf, MAX_ERRLEN,
					"invalid msg_bus read length %s\n",
								arg_list[4]);
				goto failed;
			}
		} else if (arg_num > 5) {
			snprintf(err_buf, MAX_ERRLEN, "too many arguments\n"
						"usage: r[1|2|4] msg_bus "
						"<port> <addr> [<len>]\n");
			goto failed;
		}
	}

	if (access_dir == ACCESS_DIR_WRITE) {
		if (arg_num != 5) {
			snprintf(err_buf, MAX_ERRLEN, "too few arguments\n"
				"usage: w msg_bus <port> <addr> <val>]\n");
			goto failed;
		}
		ret = kstrtou32(arg_list[4], 0, &access_value);
		if (ret) {
			snprintf(err_buf, MAX_ERRLEN,
				"invalid value for msg_bus write %s\n",
							 arg_list[4]);
			goto failed;
		}
	}

	return 0;

failed:
	return -EINVAL;
}

static int parse_pci_args(char **arg_list, int arg_num)
{
	int ret;

	if (arg_num < 6) {
		snprintf(err_buf, MAX_ERRLEN, "too few arguments\n"
			"usage: r[1|2|4] pci <bus> <dev> <func> <reg> [<len>]\n"
			"       w[1|2|4] pci <bus> <dev> <func> <reg> <val>\n");
		goto failed;
	}

	if (access_width == ACCESS_WIDTH_DEFAULT)
		access_width = ACCESS_WIDTH_32BIT;

	ret = kstrtou8(arg_list[2], 0, &pci_bus);
	if (ret || pci_bus > 255) {
		snprintf(err_buf, MAX_ERRLEN, "invalid pci bus %s\n",
							arg_list[2]);
		goto failed;
	}

	ret = kstrtou8(arg_list[3], 0, &pci_dev);
	if (ret || pci_dev > 255) {
		snprintf(err_buf, MAX_ERRLEN, "invalid pci device %s\n",
							arg_list[3]);
		goto failed;
	}

	ret = kstrtou8(arg_list[4], 0, &pci_func);
	if (ret || pci_func > 255) {
		snprintf(err_buf, MAX_ERRLEN, "invalid pci function %s\n",
							arg_list[4]);
		goto failed;
	}

	ret = kstrtou16(arg_list[5], 0, &pci_reg);
	if (ret || pci_reg > 4 * 1024) {
		snprintf(err_buf, MAX_ERRLEN, "invalid pci register %s\n",
							arg_list[5]);
		goto failed;
	}

	if ((access_width == ACCESS_WIDTH_32BIT) && (pci_reg % 4)) {
		snprintf(err_buf, MAX_ERRLEN, "reg %x is not 4 bytes aligned!\n"
							 , (u32) pci_reg);
		goto failed;
	}

	if ((access_width == ACCESS_WIDTH_16BIT) && (pci_reg % 2)) {
		snprintf(err_buf, MAX_ERRLEN, "reg %x is not 2 bytes aligned\n",
								pci_reg);
		goto failed;
	}

	if (access_dir == ACCESS_DIR_READ) {
		if (arg_num == 7) {
			ret = kstrtou32(arg_list[6], 0, &access_len);
			if (ret || access_len > 4 * 1024) {
				snprintf(err_buf, MAX_ERRLEN,
					"invalid pci read length %s\n",
							arg_list[6]);
				return ret;
			}
		} else if (arg_num > 7) {
			snprintf(err_buf, MAX_ERRLEN,
				"max 7 args are allowed for pci read\n"
				"usage: r[1|2|4] pci <bus> <dev> <func> "
							"<reg> [<len>]\n");
			goto failed;
		}
	}

	if (access_dir == ACCESS_DIR_WRITE) {
		if (arg_num != 7) {
			snprintf(err_buf, MAX_ERRLEN,
				"need exact 7 args for pci write.\n");
			goto failed;
		}
		ret = kstrtou32(arg_list[6], 0, &access_value);
		if (ret) {
			snprintf(err_buf, MAX_ERRLEN,
				"invalid value for pci write %s\n",
							 arg_list[6]);
			goto failed;
		}
	}

	return 0;

failed:
	return -EINVAL;
}

static int parse_msr_args(char **arg_list, int arg_num)
{
	int ret, arg_reg, arg_val;

	if (((access_dir == ACCESS_DIR_READ) && (arg_num < 3)) ||
		((access_dir == ACCESS_DIR_WRITE) && (arg_num < 4))) {
		snprintf(err_buf, MAX_ERRLEN, "too few arguments\n"
			"usage: r[4|8] msr [<cpu> | all] <reg>]\n"
			"       w[4|8] msr [<cpu> | all] <reg> <val>]\n");
		goto failed;
	}

	if (((access_dir == ACCESS_DIR_READ) && (arg_num > 4)) ||
		((access_dir == ACCESS_DIR_WRITE) && (arg_num > 5))) {
		snprintf(err_buf, MAX_ERRLEN, "too many arguments\n"
			"usage: r[4|8] msr [<cpu> | all] <reg>]\n"
			"       w[4|8] msr [<cpu> | all] <reg> <val>]\n");
		goto failed;
	}

	if (access_width == ACCESS_WIDTH_DEFAULT)
		access_width = ACCESS_WIDTH_64BIT;

	if (!strncmp(arg_list[2], "all", 3)) {
		msr_cpu = -1;
		arg_reg = 3;
		arg_val = 4;
	} else if ((access_dir == ACCESS_DIR_READ && arg_num == 4) ||
		(access_dir == ACCESS_DIR_WRITE && arg_num == 5)) {
		ret = kstrtou32(arg_list[2], 0, &msr_cpu);
		if (ret) {
			snprintf(err_buf, MAX_ERRLEN, "invalid cpu: %s\n",
							arg_list[2]);
			goto failed;
		}
		arg_reg = 3;
		arg_val = 4;
	} else {
		/* Default cpu for msr read is all, for msr write is 0 */
		if (access_dir == ACCESS_DIR_READ)
			msr_cpu = -1;
		else
			msr_cpu = 0;
		arg_reg = 2;
		arg_val = 3;
	}


	ret = kstrtou32(arg_list[arg_reg], 0, &msr_reg);
	if (ret) {
		snprintf(err_buf, MAX_ERRLEN, "invalid msr reg: %s\n",
							arg_list[2]);
		goto failed;
	}
	if (access_dir == ACCESS_DIR_WRITE) {
		if (access_width == ACCESS_WIDTH_32BIT)
			ret = kstrtou32(arg_list[arg_val], 0, &access_value);
		else
			ret = kstrtou64(arg_list[arg_val], 0, &access_value_64);
		if (ret) {
			snprintf(err_buf, MAX_ERRLEN, "invalid value: %s\n",
							arg_list[arg_val]);
			goto failed;
		}
	}

	return 0;

failed:
	return -EINVAL;
}

static int parse_i2c_args(char **arg_list, int arg_num)
{
	int ret;

	if ((access_dir == ACCESS_DIR_READ && arg_num != 4) ||
		(access_dir == ACCESS_DIR_WRITE && arg_num != 5)) {
		snprintf(err_buf, MAX_ERRLEN, "usage: r i2c <bus> <addr>\n"
			"       w i2c <bus> <addr> <val>\n");
		goto failed;
	}

	if (access_width == ACCESS_WIDTH_DEFAULT)
		access_width = ACCESS_WIDTH_8BIT;

	if (access_width != ACCESS_WIDTH_8BIT) {
		snprintf(err_buf, MAX_ERRLEN, "only 8bit access is allowed\n");
		goto failed;
	}

	ret = kstrtou8(arg_list[2], 0, &i2c_bus);
	if (ret || i2c_bus > 9) {
		snprintf(err_buf, MAX_ERRLEN, "invalid i2c bus %s\n",
							arg_list[2]);
		goto failed;
	}

	ret = kstrtou32(arg_list[3], 0, &i2c_addr);

	pr_err("ret = %d, i2c_addr is 0x%x\n", ret, i2c_addr);
	if (ret || (i2c_addr > 1024)) {
		snprintf(err_buf, MAX_ERRLEN, "invalid i2c address %s\n",
							arg_list[3]);
		goto failed;
	}

	if (access_dir == ACCESS_DIR_WRITE) {
		ret = kstrtou32(arg_list[4], 0, &access_value);
		if (ret) {
			snprintf(err_buf, MAX_ERRLEN,
				"invalid value for i2c write %s\n",
							 arg_list[4]);
			goto failed;
		}
	}
	return 0;

failed:
	return -EINVAL;
}

static int parse_scu_args(char **arg_list, int arg_num)
{
	int ret;

	if (access_width != ACCESS_WIDTH_32BIT)
		access_width = ACCESS_WIDTH_32BIT;

	ret = kstrtou32(arg_list[2], 0, &scu_addr);
	if (ret) {
		snprintf(err_buf, MAX_ERRLEN, "invalid scu address %s\n",
							arg_list[2]);
		goto failed;
	}

	if (scu_addr % 4) {
		snprintf(err_buf, MAX_ERRLEN,
			"addr %x is not 4 bytes aligned!\n",
						scu_addr);
		goto failed;
	}

	if (access_dir == ACCESS_DIR_READ) {
		if (arg_num != 3) {
			snprintf(err_buf, MAX_ERRLEN,
				"usage: r[4] scu <addr>\n");
			goto failed;
		}
	}

	if (access_dir == ACCESS_DIR_WRITE) {
		if (arg_num != 4) {
			snprintf(err_buf, MAX_ERRLEN,
				"usage: w[4] scu <addr> <val>\n");
			goto failed;
		}
		ret = kstrtou32(arg_list[3], 0, &access_value);
		if (ret) {
			snprintf(err_buf, MAX_ERRLEN,
				"invalid scu write value %s\n",
						arg_list[3]);
			goto failed;
		}
	}

	return 0;

failed:
	return -EINVAL;
}

static ssize_t dump_cmd_write(struct file *file, const char __user *buf,
				size_t len, loff_t *offset)
{
	char cmd[MAX_CMDLEN];
	char *arg_list[MAX_ARGS_NUM];
	int arg_num, ret = -EINVAL;

	err_buf[0] = 0;

	if (len >= MAX_CMDLEN) {
		snprintf(err_buf, MAX_ERRLEN, "input command is too long.\n"
					"max allowed input length is %d\n",
							MAX_CMDLEN);
		goto done;
	}

	if (copy_from_user(cmd, buf, len)) {
		snprintf(err_buf, MAX_ERRLEN, "copy_from_user() failed.\n");
		goto done;
	}
	cmd[len] = 0;

	dump_cmd_buf[0] = 0;
	strncpy(dump_cmd_buf, cmd, len);
	dump_cmd_buf[len] = 0;

	arg_num = parse_argument(cmd, arg_list);
	if (arg_num < MIN_ARGS_NUM) {
		snprintf(err_buf, MAX_ERRLEN,
			"invalid command(too few arguments): "
					"%s\n", dump_cmd_buf);
		goto done;
	}
	if (arg_num > MAX_ARGS_NUM) {
		snprintf(err_buf, MAX_ERRLEN,
			"invalid command(too many arguments): "
					"%s\n", dump_cmd_buf);
		goto done;
	}

	/* arg 1: direction(read/write) and mode (8/16/32/64 bit) */
	if (!strncmp(arg_list[0], "r8", 2)) {
		access_dir = ACCESS_DIR_READ;
		access_width = ACCESS_WIDTH_64BIT;
	} else if (!strncmp(arg_list[0], "r4", 2)) {
		access_dir = ACCESS_DIR_READ;
		access_width = ACCESS_WIDTH_32BIT;
	} else if (!strncmp(arg_list[0], "r2", 2)) {
		access_dir = ACCESS_DIR_READ;
		access_width = ACCESS_WIDTH_16BIT;
	} else if (!strncmp(arg_list[0], "r1", 2)) {
		access_dir = ACCESS_DIR_READ;
		access_width = ACCESS_WIDTH_8BIT;
	} else if (!strncmp(arg_list[0], "r", 1)) {
		access_dir = ACCESS_DIR_READ;
		access_width = ACCESS_WIDTH_DEFAULT;
	} else if (!strncmp(arg_list[0], "w8", 2)) {
		access_dir = ACCESS_DIR_WRITE;
		access_width = ACCESS_WIDTH_64BIT;
	} else if (!strncmp(arg_list[0], "w4", 2)) {
		access_dir = ACCESS_DIR_WRITE;
		access_width = ACCESS_WIDTH_32BIT;
	} else if (!strncmp(arg_list[0], "w2", 2)) {
		access_dir = ACCESS_DIR_WRITE;
		access_width = ACCESS_WIDTH_16BIT;
	} else if (!strncmp(arg_list[0], "w1", 2)) {
		access_dir = ACCESS_DIR_WRITE;
		access_width = ACCESS_WIDTH_8BIT;
	} else if (!strncmp(arg_list[0], "w", 1)) {
		access_dir = ACCESS_DIR_WRITE;
		access_width = ACCESS_WIDTH_DEFAULT;
	} else {
		snprintf(err_buf, MAX_ERRLEN, "unknown argument: %s\n",
							arg_list[0]);
		goto done;
	}

	/* arg2: bus type(mmio, msg_bus, pci or i2c) */
	access_len = 1;
	if (!strncmp(arg_list[1], "mmio", 4)) {
		access_bus = ACCESS_BUS_MMIO;
		ret = parse_mmio_args(arg_list, arg_num);
	} else if (!strncmp(arg_list[1], "port", 4)) {
		access_bus = ACCESS_BUS_PORT;
		ret = parse_port_args(arg_list, arg_num);
	} else if (!strncmp(arg_list[1], "msg_bus", 7)) {
		access_bus = ACCESS_BUS_MSG_BUS;
		ret = parse_msg_bus_args(arg_list, arg_num);
	} else if (!strncmp(arg_list[1], "pci", 3)) {
		access_bus = ACCESS_BUS_PCI;
		ret = parse_pci_args(arg_list, arg_num);
	} else if (!strncmp(arg_list[1], "msr", 3)) {
		access_bus = ACCESS_BUS_MSR;
		ret = parse_msr_args(arg_list, arg_num);
	} else if (!strncmp(arg_list[1], "i2c", 3)) {
		access_bus = ACCESS_BUS_I2C;
		ret = parse_i2c_args(arg_list, arg_num);
	} else if (!strncmp(arg_list[1], "scu", 3)) {
		access_bus = ACCESS_BUS_SCU_INDRW;
		ret = parse_scu_args(arg_list, arg_num);
	} else {
		snprintf(err_buf, MAX_ERRLEN, "unknown argument: %s\n",
							arg_list[1]);
	}

	if (access_len == 0) {
		snprintf(err_buf, MAX_ERRLEN,
			"access length must be larger than 0\n");
		ret = -EINVAL;
		goto done;
	}

	if ((access_bus == ACCESS_BUS_MMIO || access_bus == ACCESS_BUS_PCI) &&
					 (access_len > MAX_MMIO_PCI_LEN)) {
		snprintf(err_buf, MAX_ERRLEN,
			"%d exceeds max mmio/pci read length(%d)\n",
					access_len, MAX_MMIO_PCI_LEN);
		ret = -EINVAL;
		goto done;
	}

	if ((access_bus == ACCESS_BUS_MSG_BUS) &&
		(access_len > MAX_MSG_BUS_LEN)) {
		snprintf(err_buf, MAX_ERRLEN,
			"%d exceeds max msg_bus read length(%d)\n",
					access_len, MAX_MSG_BUS_LEN);
		ret = -EINVAL;
	}

	if (access_bus == ACCESS_BUS_MSR) {
		if ((access_width != ACCESS_WIDTH_32BIT) &&
			(access_width != ACCESS_WIDTH_64BIT) &&
			(access_width != ACCESS_WIDTH_DEFAULT)) {
			snprintf(err_buf, MAX_ERRLEN,
				"only 32bit or 64bit is allowed for msr\n");
			ret = -EINVAL;
		}
	}

done:
	dump_cmd_was_set = ret ? 0 : 1;
	return ret ? ret : len;
}

static int dump_output_show_mmio(struct seq_file *s)
{
	void __iomem *base;
	int i, comp1, comp2;
	u32 start, end, end_natural;
	struct pci_dev *pdev;
	char *name;

	pdev = mmio_to_pci(mmio_addr, &name);
	if (pdev && pm_runtime_get_sync(&pdev->dev) < 0) {
		seq_printf(s, "can't put device %s into D0i0 state\n", name);
		return 0;
	}

	if (access_dir == ACCESS_DIR_WRITE) {
		base = ioremap_nocache(mmio_addr, access_width);
		if (!base) {
			seq_printf(s, "can't map physical address: %x\n",
				mmio_addr);
			if (pdev)
				pm_runtime_put_sync(&pdev->dev);
			return 0;
		}
		switch (access_width) {
		case ACCESS_WIDTH_8BIT:
			iowrite8((u8) access_value, base);
			break;
		case ACCESS_WIDTH_16BIT:
			iowrite16((u16) access_value, base);
			break;
		case ACCESS_WIDTH_32BIT:
		case ACCESS_WIDTH_DEFAULT:
			iowrite32(access_value, base);
			break;
		default:
			break; /* never happen */
		}
		seq_printf(s, "write succeeded\n");
	} else {
		start = (mmio_addr / LINE_WIDTH) * LINE_WIDTH;
		end_natural = mmio_addr + (access_len - 1) * access_width;
		end = (end_natural / LINE_WIDTH + 1) * LINE_WIDTH -
						access_width;
		comp1 = (mmio_addr - start) / access_width;
		comp2 = (end - end_natural) / access_width;

		base = ioremap_nocache(start, (comp1 + comp2 +
			access_len) * access_width);
		if (!base) {
			seq_printf(s, "can't map physical address: %x\n",
				mmio_addr);
			if (pdev)
				pm_runtime_put_sync(&pdev->dev);
			return 0;
		}

		for (i = 0; i < comp1 + comp2 + access_len; i++) {
			if ((i % SHOW_NUM_PER_LINE) == 0)
					seq_printf(s, "[%08x]", start + i * 4);

			if (i < comp1 || i >= access_len + comp1) {
				switch (access_width) {
				case ACCESS_WIDTH_32BIT:
					seq_printf(s, "         ");
					break;
				case ACCESS_WIDTH_16BIT:
					seq_printf(s, "     ");
					break;
				case ACCESS_WIDTH_8BIT:
					seq_printf(s, "   ");
					break;
				}

			} else {
				switch (access_width) {
				case ACCESS_WIDTH_32BIT:
					seq_printf(s, " %08x",
						ioread32(base + i * 4));
					break;
				case ACCESS_WIDTH_16BIT:
					seq_printf(s, " %04x",
						(u16) ioread16(base + i * 2));
					break;
				case ACCESS_WIDTH_8BIT:
					seq_printf(s, " %02x",
						(u8) ioread8(base + i));
					break;
				}
			}

			if ((i + 1) % SHOW_NUM_PER_LINE == 0)
				seq_printf(s, "\n");
		}
	}

	iounmap(base);
	if (pdev)
		pm_runtime_put_sync(&pdev->dev);
	return 0;
}

static int dump_output_show_port(struct seq_file *s)
{
	if (access_dir == ACCESS_DIR_WRITE) {
		switch (access_width) {
		case ACCESS_WIDTH_8BIT:
		case ACCESS_WIDTH_DEFAULT:
			outb((u8) access_value, port_addr);
			break;
		case ACCESS_WIDTH_16BIT:
			outw((u16) access_value, port_addr);
			break;
		case ACCESS_WIDTH_32BIT:
			outl(access_value, port_addr);
			break;
		default:
			break; /* never happen */
		}
		seq_printf(s, "write succeeded\n");
	} else {
		switch (access_width) {
		case ACCESS_WIDTH_32BIT:
			seq_printf(s, " %08x\n", inl(port_addr));
			break;
		case ACCESS_WIDTH_16BIT:
			seq_printf(s, " %04x\n", (u16) inw(port_addr));
			break;
		case ACCESS_WIDTH_8BIT:
			seq_printf(s, " %02x\n", (u8) inb(port_addr));
			break;
		default:
			break;
		}
	}

	return 0;
}

static int dump_output_show_msg_bus(struct seq_file *s)
{
	int i, comp1, comp2;
	u32 start, end, end_natural;

	if (access_dir == ACCESS_DIR_WRITE) {
		intel_mid_msgbus_write32(msg_bus_port,
			msg_bus_addr, access_value);
		seq_printf(s, "write succeeded\n");
	} else {
		start = (msg_bus_addr / LINE_WIDTH) * LINE_WIDTH;
		end_natural = msg_bus_addr + (access_len - 1) * access_width;
		end = (end_natural / LINE_WIDTH + 1) * LINE_WIDTH -
						access_width;
		comp1 = (msg_bus_addr - start) / access_width;
		comp2 = (end - end_natural) / access_width;

	for (i = 0; i < comp1 + comp2 + access_len; i++) {
			if ((i % SHOW_NUM_PER_LINE) == 0)
					seq_printf(s, "[%08x]", start + i * 4);

			if (i < comp1 || i >= access_len + comp1)
				seq_printf(s, "         ");

			else
				seq_printf(s, " %08x", intel_mid_msgbus_read32(
					msg_bus_port, msg_bus_addr + i));

			if ((i + 1) % SHOW_NUM_PER_LINE == 0)
				seq_printf(s, "\n");
		}
	}

	return 0;
}

static int dump_output_show_pci(struct seq_file *s)
{
	int i, comp1, comp2;
	u32 start, end, end_natural, val;
	struct pci_dev *pdev;

	pdev = pci_get_bus_and_slot(pci_bus, PCI_DEVFN(pci_dev, pci_func));
	if (!pdev) {
		seq_printf(s, "pci bus %d:%d:%d doesn't exist\n",
			pci_bus, pci_dev, pci_func);
		return 0;
	}

	if (pm_runtime_get_sync(&pdev->dev) < 0) {
		seq_printf(s, "can't put pci device %d:%d:%d into D0i0 state\n",
			pci_bus, pci_dev, pci_func);
		return 0;
	}

	if (access_dir == ACCESS_DIR_WRITE) {
		switch (access_width) {
		case ACCESS_WIDTH_8BIT:
			pci_write_config_byte(pdev, (int)pci_reg,
					(u8)access_value);
			break;
		case ACCESS_WIDTH_16BIT:
			pci_write_config_word(pdev, (int)pci_reg,
				(u16)access_value);
			break;
		case ACCESS_WIDTH_32BIT:
		case ACCESS_WIDTH_DEFAULT:
			pci_write_config_dword(pdev, (int)pci_reg,
				access_value);
			break;
		default:
			break; /* never happen */
		}
		seq_printf(s, "write succeeded\n");
	} else {
		start = (pci_reg / LINE_WIDTH) * LINE_WIDTH;
		end_natural = pci_reg + (access_len - 1) * access_width;
		end = (end_natural / LINE_WIDTH + 1) * LINE_WIDTH -
						access_width;
		comp1 = (pci_reg - start) / access_width;
		comp2 = (end - end_natural) / access_width;

		for (i = 0; i < comp1 + comp2 + access_len; i++) {
			if ((i % SHOW_NUM_PER_LINE) == 0)
					seq_printf(s, "[%08x]", start + i * 4);

			if (i < comp1 || i >= access_len + comp1) {
				switch (access_width) {
				case ACCESS_WIDTH_32BIT:
					seq_printf(s, "         ");
					break;
				case ACCESS_WIDTH_16BIT:
					seq_printf(s, "     ");
					break;
				case ACCESS_WIDTH_8BIT:
					seq_printf(s, "   ");
					break;
				}

			} else {
				switch (access_width) {
				case ACCESS_WIDTH_32BIT:
					pci_read_config_dword(pdev,
						start + i * 4, &val);
					seq_printf(s, " %08x", val);
					break;
				case ACCESS_WIDTH_16BIT:
					pci_read_config_word(pdev,
						start + i * 2, (u16 *) &val);
					seq_printf(s, " %04x", (u16)val);
					break;
				case ACCESS_WIDTH_8BIT:
					pci_read_config_byte(pdev,
						start + i, (u8 *) &val);
					seq_printf(s, " %04x", (u8)val);
					break;
				}
			}

			if ((i + 1) % SHOW_NUM_PER_LINE == 0)
				seq_printf(s, "\n");
		}
	}

	return 0;
}

static int dump_output_show_msr(struct seq_file *s)
{
	int ret, i, count;
	u32 data[2];

	if (access_dir == ACCESS_DIR_READ) {
		if (msr_cpu < 0) {
			/* loop for all cpus */
			i = 0;
			count = nr_cpu_ids;
		} else if (msr_cpu >= nr_cpu_ids || msr_cpu < 0) {
			seq_printf(s, "cpu should be between 0 - %d\n",
							nr_cpu_ids - 1);
			return 0;
		} else {
			/* loop for one cpu */
			i = msr_cpu;
			count = msr_cpu + 1;
		}
		for (; i < count; i++) {
			ret = rdmsr_safe_on_cpu(i, msr_reg, &data[0], &data[1]);
			if (ret) {
				seq_printf(s, "msr read error: %d\n", ret);
				return 0;
			} else {
				if (access_width == ACCESS_WIDTH_32BIT)
					seq_printf(s, "[cpu %1d] %08x\n",
							i, data[0]);
				else
					seq_printf(s, "[cpu %1d] %08x%08x\n",
						 i, data[1], data[0]);
			}
		}
	} else {
		if (access_width == ACCESS_WIDTH_32BIT) {
			ret = rdmsr_safe_on_cpu(msr_cpu, msr_reg,
					&data[0], &data[1]);
			if (ret) {
				seq_printf(s, "msr write error: %d\n", ret);
				return 0;
			}
			data[0] = access_value;
		} else {
			data[0] = (u32)access_value_64;
			data[1] = (u32)(access_value_64 >> 32);
		}
		if (msr_cpu < 0) {
			/* loop for all cpus */
			i = 0;
			count = nr_cpu_ids;
		} else {
			if (msr_cpu >= nr_cpu_ids || msr_cpu < 0) {
				seq_printf(s, "cpu should be between 0 - %d\n",
						nr_cpu_ids - 1);
				return 0;
			}
			/* loop for one cpu */
			i = msr_cpu;
			count = msr_cpu + 1;
		}
		for (; i < count; i++) {
			ret = wrmsr_safe_on_cpu(i, msr_reg, data[0], data[1]);
			if (ret) {
				seq_printf(s, "msr write error: %d\n", ret);
				return 0;
			} else {
				seq_printf(s, "write succeeded.\n");
			}
		}
	}

	return 0;
}

static int dump_output_show_i2c(struct seq_file *s)
{
	int ret;
	struct i2c_adapter *adap;
	struct i2c_msg msg;
	u8 val;

	adap = i2c_get_adapter(i2c_bus);
	if (!adap) {
		seq_printf(s, "can't find bus adapter for i2c bus %d\n",
							i2c_bus);
		return 0;
	}

	if (access_dir == ACCESS_DIR_WRITE) {
		msg.addr = i2c_addr;
		msg.len = 1;
		msg.buf = (u8 *) &access_value;
		ret = i2c_transfer(adap, &msg, 1);
		if (ret != 1)
			seq_printf(s, "i2c write error: %d\n", ret);
		else
			seq_printf(s, "write succeeded.\n");
	} else {
		msg.flags |= I2C_M_RD;
		msg.addr = i2c_addr;
		msg.len = 1;
		msg.buf = &val;
		ret = i2c_transfer(adap, &msg, 1);
		if (ret != 1)
			seq_printf(s, "i2c read error: %d\n", ret);
		else
			seq_printf(s, "%02x\n", val);
	}

	return 0;
}

static int dump_output_show_scu(struct seq_file *s)
{
	struct pci_dev *pdev;
	char *name;
	int ret;
	u32 cmd, sub = 0, dptr = 0, sptr = 0;
	u8 wbuflen = 4, rbuflen = 4;
	u8 wbuf[16];
	u8 rbuf[16];

	memset(wbuf, 0, 16);
	memset(rbuf, 0, 16);

	pdev = mmio_to_pci(scu_addr, &name);
	if (pdev && pm_runtime_get_sync(&pdev->dev) < 0) {
		seq_printf(s, "can't put device %s into D0i0 state\n", name);
		return 0;
	}

	if (access_dir == ACCESS_DIR_WRITE) {
		cmd = RP_INDIRECT_WRITE;
		dptr = scu_addr;
		wbuf[0] = (u8) (access_value & 0xff);
		wbuf[1] = (u8) ((access_value >> 8) & 0xff);
		wbuf[2] = (u8) ((access_value >> 16) & 0xff);
		wbuf[3] = (u8) ((access_value >> 24) & 0xff);

		ret = rpmsg_send_generic_raw_command(cmd, sub, wbuf, wbuflen,
			(u32 *)rbuf, rbuflen, dptr, sptr);

		if (ret) {
			seq_printf(s,
				"Indirect write failed (check dmesg): "
						"[%08x]\n", scu_addr);
		} else {
			seq_printf(s, "write succeeded\n");
		}
	} else if (access_dir == ACCESS_DIR_READ) {
		cmd = RP_INDIRECT_READ;
		sptr = scu_addr;

		ret = rpmsg_send_generic_raw_command(cmd, sub, wbuf, wbuflen,
			(u32 *)rbuf, rbuflen, dptr, sptr);

		if (ret) {
			seq_printf(s,
				"Indirect read failed (check dmesg): "
						"[%08x]\n", scu_addr);
		} else {
			access_value = (rbuf[3] << 24) | (rbuf[2] << 16) |
				(rbuf[1] << 8) | (rbuf[0]);
			seq_printf(s, "[%08x] %08x\n", scu_addr, access_value);
		}
	}

	if (pdev)
		pm_runtime_put_sync(&pdev->dev);

	return 0;
}

static int dump_output_show(struct seq_file *s, void *unused)
{
	int ret = 0;

	if (!dump_cmd_was_set) {
		seq_printf(s, "%s", err_buf);
		return 0;
	}

	switch (access_bus) {
	case ACCESS_BUS_MMIO:
		ret = dump_output_show_mmio(s);
		break;
	case ACCESS_BUS_PORT:
		ret = dump_output_show_port(s);
		break;
	case ACCESS_BUS_MSG_BUS:
		ret = dump_output_show_msg_bus(s);
		break;
	case ACCESS_BUS_PCI:
		ret = dump_output_show_pci(s);
		break;
	case ACCESS_BUS_MSR:
		ret = dump_output_show_msr(s);
		break;
	case ACCESS_BUS_I2C:
		ret = dump_output_show_i2c(s);
		break;
	case ACCESS_BUS_SCU_INDRW:
		ret = dump_output_show_scu(s);
		break;
	default:
		seq_printf(s, "unknow bus type: %d\n", access_bus);
		break;

	}

	return ret;
}

static const struct file_operations dump_cmd_fops = {
	.owner		= THIS_MODULE,
	.open		= dump_cmd_open,
	.read		= seq_read,
	.write		= dump_cmd_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int dump_output_open(struct inode *inode, struct file *file)
{
	return single_open(file, dump_output_show, NULL);
}

static const struct file_operations dump_output_fops = {
	.owner		= THIS_MODULE,
	.open		= dump_output_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init intel_mid_dump_init(void)
{
	dump_cmd_dentry = debugfs_create_file("dump_cmd",
		S_IFREG | S_IRUGO | S_IWUSR, NULL, NULL, &dump_cmd_fops);
	dump_output_dentry = debugfs_create_file("dump_output",
		S_IFREG | S_IRUGO, NULL, NULL, &dump_output_fops);
	if (!dump_cmd_dentry || !dump_output_dentry) {
		pr_err("intel_mid_dump: can't create debugfs node\n");
		return -EFAULT;
	}
	return 0;
}
module_init(intel_mid_dump_init);

static void __exit intel_mid_dump_exit(void)
{
	if (dump_cmd_dentry)
		debugfs_remove(dump_cmd_dentry);
	if (dump_output_dentry)
		debugfs_remove(dump_output_dentry);
}
module_exit(intel_mid_dump_exit);

MODULE_DESCRIPTION("Intel Atom SoC register dump driver");
MODULE_VERSION("1.0");
MODULE_AUTHOR("Bin Gao <bin.gao@intel.com>");
MODULE_LICENSE("GPL v2");
