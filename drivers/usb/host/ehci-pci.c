/*
 * EHCI HCD (Host Controller Driver) PCI Bus Glue.
 *
 * Copyright (c) 2000-2004 by David Brownell
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>

#include "ehci.h"
#include "pci-quirks.h"

#define DRIVER_DESC "EHCI PCI platform driver"

static const char hcd_name[] = "ehci-pci";

/* defined here to avoid adding to pci_ids.h for single instance use */
#define PCI_DEVICE_ID_INTEL_CE4100_USB	0x2e70

/*-------------------------------------------------------------------------*/
/* CloverTrail USB SPH and Modem USB Switch Control Flag */
static unsigned int use_sph;
module_param(use_sph, uint, S_IRUGO);
MODULE_PARM_DESC(use_sph, "sph and modem usb switch flag, default disable\n");
/*
* for external read access to <usb_sph>
*/
unsigned int sph_enabled(void)
{
	return use_sph;
}

/* enable SRAM if sram detected */
static void sram_init(struct usb_hcd *hcd)
{
	struct ehci_hcd		*ehci = hcd_to_ehci(hcd);
	struct pci_dev		*pdev = to_pci_dev(hcd->self.controller);
	void __iomem		*base = NULL;
	void __iomem		*addr = NULL;

	if (!hcd->has_sram)
		return;
	ehci->sram_addr = pci_resource_start(pdev, 1);
	ehci->sram_size = pci_resource_len(pdev, 1);
	ehci_info(ehci, "Found HCD SRAM at %x size:%x\n",
		ehci->sram_addr, ehci->sram_size);

	if (pci_request_region(pdev, 1, kobject_name(&pdev->dev.kobj))) {
		ehci_warn(ehci, "SRAM request failed\n");
		hcd->has_sram = 0;
		return;
	} else if (!dma_declare_coherent_memory(&pdev->dev, ehci->sram_addr,
			ehci->sram_addr, ehci->sram_size, DMA_MEMORY_MAP)) {
		ehci_warn(ehci, "SRAM DMA declare failed\n");
		pci_release_region(pdev, 1);
		hcd->has_sram = 0;
		return;
	}

	/* initialize SRAM to 0 to avoid ECC errors during entry into D0 */
	base = ioremap_nocache(ehci->sram_addr, ehci->sram_size);
	if (base == NULL) {
		ehci_warn(ehci, "SRAM init: ioremap failed\n");
		return;
	}

	addr = base;

	while (addr < base + ehci->sram_size) {
		writel(0x0, addr);
		addr = addr + 4;
	}

	iounmap(base);
}

static void sram_deinit(struct usb_hcd *hcd)
{
	struct ehci_hcd		*ehci = hcd_to_ehci(hcd);
	struct pci_dev *pdev = to_pci_dev(hcd->self.controller);

	if (!hcd->has_sram)
		return;
	dma_release_declared_memory(&pdev->dev);
	pci_release_region(pdev, 1);

	/* If host is suspended, SRAM backup memory should be freed */
	if (ehci->sram_swap) {
		vfree(ehci->sram_swap);
		ehci->sram_swap = NULL;
	}
}

static int sram_backup(struct usb_hcd *hcd)
{
	struct ehci_hcd		*ehci = hcd_to_ehci(hcd);
	void __iomem		*base;
	int			offset;

	ehci->sram_swap = vmalloc(ehci->sram_size);
	if (!ehci->sram_swap) {
		ehci_warn(ehci, "SRAM backup memory request failed\n");
		return -ENOMEM;
	}

	base = ioremap_nocache(ehci->sram_addr, ehci->sram_size);
	if (!base) {
		ehci_warn(ehci, "SRAM backeup ioremap fails\n");
		vfree(ehci->sram_swap);
		ehci->sram_swap = NULL;
		return -EFAULT;
	}

	for (offset = 0; offset < ehci->sram_size; offset += 4)
		*(u32 *)(ehci->sram_swap + offset) = readl(base + offset);

	iounmap(base);

	return 0;
}

static int sram_restore(struct usb_hcd *hcd)
{
	struct ehci_hcd		*ehci = hcd_to_ehci(hcd);
	void __iomem		*base;
	int			offset;

	if (!ehci->sram_swap)
		return -EFAULT;

	base = ioremap_nocache(ehci->sram_addr, ehci->sram_size);
	if (!base) {
		ehci_warn(ehci, "SRAM_restore ioremap fails\n");
		return -EFAULT;
	}

	for (offset = 0; offset < ehci->sram_size; offset += 4)
		writel(*(u32 *)(ehci->sram_swap + offset), base + offset);

	iounmap(base);
	vfree(ehci->sram_swap);
	ehci->sram_swap = NULL;

	return 0;
}

/* called after powerup, by probe or system-pm "wakeup" */
static int ehci_pci_reinit(struct ehci_hcd *ehci, struct pci_dev *pdev)
{
	int			retval;

	/* we expect static quirk code to handle the "extended capabilities"
	 * (currently just BIOS handoff) allowed starting with EHCI 0.96
	 */

	/* PCI Memory-Write-Invalidate cycle support is optional (uncommon) */
	retval = pci_set_mwi(pdev);
	if (!retval)
		ehci_dbg(ehci, "MWI active\n");

	return 0;
}

/* called during probe() after chip reset completes */
static int ehci_pci_setup(struct usb_hcd *hcd)
{
	struct ehci_hcd		*ehci = hcd_to_ehci(hcd);
	struct pci_dev		*pdev = to_pci_dev(hcd->self.controller);
	struct pci_dev		*p_smbus;
	u8			rev;
	u32			temp;
	int			retval;

	ehci->caps = hcd->regs;

	/*
	 * ehci_init() causes memory for DMA transfers to be
	 * allocated.  Thus, any vendor-specific workarounds based on
	 * limiting the type of memory used for DMA transfers must
	 * happen before ehci_setup() is called.
	 *
	 * Most other workarounds can be done either before or after
	 * init and reset; they are located here too.
	 */
	switch (pdev->vendor) {
	case PCI_VENDOR_ID_TOSHIBA_2:
		/* celleb's companion chip */
		if (pdev->device == 0x01b5) {
#ifdef CONFIG_USB_EHCI_BIG_ENDIAN_MMIO
			ehci->big_endian_mmio = 1;
#else
			ehci_warn(ehci,
				  "unsupported big endian Toshiba quirk\n");
#endif
		}
		break;
	case PCI_VENDOR_ID_NVIDIA:
		/* NVidia reports that certain chips don't handle
		 * QH, ITD, or SITD addresses above 2GB.  (But TD,
		 * data buffer, and periodic schedule are normal.)
		 */
		switch (pdev->device) {
		case 0x003c:	/* MCP04 */
		case 0x005b:	/* CK804 */
		case 0x00d8:	/* CK8 */
		case 0x00e8:	/* CK8S */
			if (pci_set_consistent_dma_mask(pdev,
						DMA_BIT_MASK(31)) < 0)
				ehci_warn(ehci, "can't enable NVidia "
					"workaround for >2GB RAM\n");
			break;

		/* Some NForce2 chips have problems with selective suspend;
		 * fixed in newer silicon.
		 */
		case 0x0068:
			if (pdev->revision < 0xa4)
				ehci->no_selective_suspend = 1;
			break;
		}
		break;
	case PCI_VENDOR_ID_INTEL:
		if (pdev->device == PCI_DEVICE_ID_INTEL_CE4100_USB)
			hcd->has_tt = 1;
		else if (pdev->device == 0x0811 || pdev->device == 0x0829 ||
				pdev->device == 0xE006) {
			ehci_info(ehci, "Detected Intel MID OTG HC\n");
			hcd->has_tt = 1;
			ehci->has_hostpc = 1;
#ifdef CONFIG_USB_OTG
			ehci->has_otg = 1;
#endif
			hcd->has_sram = 1;
			/*
			 * Disable SRAM for CLVP A0 due to the silicon issue.
			 */
			if (pdev->device == 0xE006 && pdev->revision < 0xC) {
				ehci_info(ehci, "Disable SRAM for CLVP A0\n");
				hcd->has_sram = 0;
			}

			hcd->sram_no_payload = 1;
			sram_init(hcd);
			} else if (pdev->device == 0x0806) {
				ehci_info(ehci, "Detected Langwell MPH\n");
				hcd->has_tt = 1;
				ehci->has_hostpc = 1;
				hcd->has_sram = 1;
				hcd->sram_no_payload = 1;
				sram_init(hcd);
			} else if (pdev->device == 0x0829) {
				ehci_info(ehci, "Detected Penwell OTG HC\n");
				hcd->has_tt = 1;
				ehci->has_hostpc = 1;
		} else if (pdev->device == 0x08F2) {
#ifdef CONFIG_USB_EHCI_HCD_SPH
			struct ehci_sph_pdata   *sph_pdata;
			sph_pdata = pdev->dev.platform_data;

			/* All need to bypass tll mode  */
			temp = ehci_readl(ehci, hcd->regs + CLV_SPHCFG);
			temp &= ~CLV_SPHCFG_ULPI1TYPE;
			ehci_writel(ehci, temp, hcd->regs + CLV_SPHCFG);

			/* Check SPH enabled or not */
			if (!sph_enabled() || !sph_pdata) {
				/* ULPI 1 ref-clock switch off */
				temp = ehci_readl(ehci, hcd->regs + CLV_SPHCFG);
				temp |= CLV_SPHCFG_REFCKDIS;
				ehci_writel(ehci, temp, hcd->regs + CLV_SPHCFG);

				/* Set Power state */
				retval = pci_set_power_state(pdev, PCI_D1);
				if (retval < 0)
					ehci_err(ehci,
						"Set SPH to D1 failed, retval = %d\n",
						retval);

				ehci_info(ehci, "USB SPH is disabled\n");
				return -ENODEV;
				}

			sph_pdata->enabled = sph_enabled();

			ehci_info(ehci, "Detected SPH HC\n");
			hcd->has_tt = 1;
			ehci->has_hostpc = 1;

			hcd->has_wakeup_irq = 1;

			temp = ehci_readl(ehci, hcd->regs + CLV_SPH_HOSTPC);
			temp |= CLV_SPH_HOSTPC_PTS;
			ehci_writel(ehci, temp, hcd->regs + CLV_SPH_HOSTPC);

			device_set_wakeup_enable(&pdev->dev, true);

			pm_runtime_set_active(&pdev->dev);
#endif
		} else if (pdev->device == 0x119D) {
			ehci_info(ehci, "Detected HSIC HC\n");
			hcd->has_tt = 1;
			ehci->has_hostpc = 1;

			hcd->has_wakeup_irq = 1;

			hcd->has_sram = 0;
			hcd->sram_no_payload = 1;
			sram_init(hcd);

			device_set_wakeup_enable(&pdev->dev, true);
			pm_runtime_set_active(&pdev->dev);
		}
		break;
	case PCI_VENDOR_ID_TDI:
		if (pdev->device == PCI_DEVICE_ID_TDI_EHCI)
			hcd->has_tt = 1;
		break;
	case PCI_VENDOR_ID_AMD:
		/* AMD PLL quirk */
		if (usb_amd_find_chipset_info())
			ehci->amd_pll_fix = 1;
		/* AMD8111 EHCI doesn't work, according to AMD errata */
		if (pdev->device == 0x7463) {
			ehci_info(ehci, "ignoring AMD8111 (errata)\n");
			retval = -EIO;
			goto done;
		}

		/*
		 * EHCI controller on AMD SB700/SB800/Hudson-2/3 platforms may
		 * read/write memory space which does not belong to it when
		 * there is NULL pointer with T-bit set to 1 in the frame list
		 * table. To avoid the issue, the frame list link pointer
		 * should always contain a valid pointer to a inactive qh.
		 */
		if (pdev->device == 0x7808) {
			ehci->use_dummy_qh = 1;
			ehci_info(ehci, "applying AMD SB700/SB800/Hudson-2/3 EHCI dummy qh workaround\n");
		}
		break;
	case PCI_VENDOR_ID_VIA:
		if (pdev->device == 0x3104 && (pdev->revision & 0xf0) == 0x60) {
			u8 tmp;

			/* The VT6212 defaults to a 1 usec EHCI sleep time which
			 * hogs the PCI bus *badly*. Setting bit 5 of 0x4B makes
			 * that sleep time use the conventional 10 usec.
			 */
			pci_read_config_byte(pdev, 0x4b, &tmp);
			if (tmp & 0x20)
				break;
			pci_write_config_byte(pdev, 0x4b, tmp | 0x20);
		}
		break;
	case PCI_VENDOR_ID_ATI:
		/* AMD PLL quirk */
		if (usb_amd_find_chipset_info())
			ehci->amd_pll_fix = 1;

		/*
		 * EHCI controller on AMD SB700/SB800/Hudson-2/3 platforms may
		 * read/write memory space which does not belong to it when
		 * there is NULL pointer with T-bit set to 1 in the frame list
		 * table. To avoid the issue, the frame list link pointer
		 * should always contain a valid pointer to a inactive qh.
		 */
		if (pdev->device == 0x4396) {
			ehci->use_dummy_qh = 1;
			ehci_info(ehci, "applying AMD SB700/SB800/Hudson-2/3 EHCI dummy qh workaround\n");
		}
		/* SB600 and old version of SB700 have a bug in EHCI controller,
		 * which causes usb devices lose response in some cases.
		 */
		if ((pdev->device == 0x4386) || (pdev->device == 0x4396)) {
			p_smbus = pci_get_device(PCI_VENDOR_ID_ATI,
						 PCI_DEVICE_ID_ATI_SBX00_SMBUS,
						 NULL);
			if (!p_smbus)
				break;
			rev = p_smbus->revision;
			if ((pdev->device == 0x4386) || (rev == 0x3a)
			    || (rev == 0x3b)) {
				u8 tmp;
				ehci_info(ehci, "applying AMD SB600/SB700 USB "
					"freeze workaround\n");
				pci_read_config_byte(pdev, 0x53, &tmp);
				pci_write_config_byte(pdev, 0x53, tmp | (1<<3));
			}
			pci_dev_put(p_smbus);
		}
		break;
	case PCI_VENDOR_ID_NETMOS:
		/* MosChip frame-index-register bug */
		ehci_info(ehci, "applying MosChip frame-index workaround\n");
		ehci->frame_index_bug = 1;
		break;
	}

	/* optional debug port, normally in the first BAR */
	temp = pci_find_capability(pdev, PCI_CAP_ID_DBG);
	if (temp) {
		pci_read_config_dword(pdev, temp, &temp);
		temp >>= 16;
		if (((temp >> 13) & 7) == 1) {
			u32 hcs_params = ehci_readl(ehci,
						    &ehci->caps->hcs_params);

			temp &= 0x1fff;
			ehci->debug = hcd->regs + temp;
			temp = ehci_readl(ehci, &ehci->debug->control);
			ehci_info(ehci, "debug port %d%s\n",
				  HCS_DEBUG_PORT(hcs_params),
				  (temp & DBGP_ENABLED) ? " IN USE" : "");
			if (!(temp & DBGP_ENABLED))
				ehci->debug = NULL;
		}
	}

	retval = ehci_setup(hcd);
	if (retval)
		return retval;

	/* These workarounds need to be applied after ehci_setup() */
	switch (pdev->vendor) {
	case PCI_VENDOR_ID_NEC:
		ehci->need_io_watchdog = 0;
		break;
	case PCI_VENDOR_ID_INTEL:
		ehci->need_io_watchdog = 0;
		break;
	case PCI_VENDOR_ID_NVIDIA:
		switch (pdev->device) {
		/* MCP89 chips on the MacBookAir3,1 give EPROTO when
		 * fetching device descriptors unless LPM is disabled.
		 * There are also intermittent problems enumerating
		 * devices with PPCD enabled.
		 */
		case 0x0d9d:
			ehci_info(ehci, "disable ppcd for nvidia mcp89\n");
			ehci->has_ppcd = 0;
			ehci->command &= ~CMD_PPCEE;
			break;
		}
		break;
	}

	/* at least the Genesys GL880S needs fixup here */
	temp = HCS_N_CC(ehci->hcs_params) * HCS_N_PCC(ehci->hcs_params);
	temp &= 0x0f;
	if (temp && HCS_N_PORTS(ehci->hcs_params) > temp) {
		ehci_dbg(ehci, "bogus port configuration: "
			"cc=%d x pcc=%d < ports=%d\n",
			HCS_N_CC(ehci->hcs_params),
			HCS_N_PCC(ehci->hcs_params),
			HCS_N_PORTS(ehci->hcs_params));

		switch (pdev->vendor) {
		case 0x17a0:		/* GENESYS */
			/* GL880S: should be PORTS=2 */
			temp |= (ehci->hcs_params & ~0xf);
			ehci->hcs_params = temp;
			break;
		case PCI_VENDOR_ID_NVIDIA:
			/* NF4: should be PCC=10 */
			break;
		}
	}

	/* Serial Bus Release Number is at PCI 0x60 offset */
	if (pdev->vendor == PCI_VENDOR_ID_STMICRO
	    && pdev->device == PCI_DEVICE_ID_STMICRO_USB_HOST)
		;	/* ConneXT has no sbrn register */
	else
		pci_read_config_byte(pdev, 0x60, &ehci->sbrn);

	/* Keep this around for a while just in case some EHCI
	 * implementation uses legacy PCI PM support.  This test
	 * can be removed on 17 Dec 2009 if the dev_warn() hasn't
	 * been triggered by then.
	 */
	if (!device_can_wakeup(&pdev->dev)) {
		u16	port_wake;

		pci_read_config_word(pdev, 0x62, &port_wake);
		if (port_wake & 0x0001) {
			dev_warn(&pdev->dev, "Enabling legacy PCI PM\n");
			device_set_wakeup_capable(&pdev->dev, 1);
		}
	}

#ifdef	CONFIG_PM_RUNTIME
	if (ehci->no_selective_suspend && device_can_wakeup(&pdev->dev))
		ehci_warn(ehci, "selective suspend/wakeup unavailable\n");
#endif

	retval = ehci_pci_reinit(ehci, pdev);
done:
	return retval;
}

/*-------------------------------------------------------------------------*/

#ifdef	CONFIG_PM

/* suspend/resume, section 4.3 */

/* These routines rely on the PCI bus glue
 * to handle powerdown and wakeup, and currently also on
 * transceivers that don't need any software attention to set up
 * the right sort of wakeup.
 * Also they depend on separate root hub suspend/resume.
 */

static bool usb_is_intel_switchable_ehci(struct pci_dev *pdev)
{
	return pdev->class == PCI_CLASS_SERIAL_USB_EHCI &&
		pdev->vendor == PCI_VENDOR_ID_INTEL &&
		(pdev->device == 0x1E26 ||
		 pdev->device == 0x8C2D ||
		 pdev->device == 0x8C26 ||
		 pdev->device == 0x9C26);
}

static void ehci_enable_xhci_companion(void)
{
	struct pci_dev		*companion = NULL;

	/* The xHCI and EHCI controllers are not on the same PCI slot */
	for_each_pci_dev(companion) {
		if (!usb_is_intel_switchable_xhci(companion))
			continue;
		usb_enable_xhci_ports(companion);
		return;
	}
}

static int ehci_pci_suspend(struct usb_hcd *hcd, bool do_wakeup)
{
	struct ehci_hcd		*ehci = hcd_to_ehci(hcd);
	unsigned long		flags;
	int			rc = 0;
	int			port;

	if (time_before(jiffies, ehci->next_statechange))
		usleep_range(10000, 12000);

	/* s0i3 may poweroff SRAM, backup the SRAM */
	if (hcd->has_sram && sram_backup(hcd)) {
		ehci_warn(ehci, "sram_backup failed\n");
		return -EPERM;
	}

	rc = ehci_suspend(hcd, do_wakeup);

	/* Set HOSTPC_PHCD if not set yet to let PHY enter low-power mode */
	if (ehci->has_hostpc) {
		usleep_range(5000, 6000);
		spin_lock_irqsave(&ehci->lock, flags);

		port = HCS_N_PORTS(ehci->hcs_params);
		while (port--) {
			u32 __iomem	*hostpc_reg;
			u32		temp;
			struct pci_dev  *pdev =
				to_pci_dev(hcd->self.controller);

			hostpc_reg = &ehci->regs->hostpc[port];
			temp = ehci_readl(ehci, hostpc_reg);

			if (!(temp & HOSTPC_PHCD))
				ehci_writel(ehci, temp | HOSTPC_PHCD,
						hostpc_reg);
			temp = ehci_readl(ehci, hostpc_reg);
			ehci_dbg(ehci, "Port %d PHY low-power mode %s\n",
				port, (temp & HOSTPC_PHCD) ?
					"succeeded" : "failed");
		}
		spin_unlock_irqrestore(&ehci->lock, flags);
	}

	return rc;
}

static int ehci_pci_resume(struct usb_hcd *hcd, bool hibernated)
{
	struct ehci_hcd		*ehci = hcd_to_ehci(hcd);
	struct pci_dev		*pdev = to_pci_dev(hcd->self.controller);

	/* s0i3 may have poweroff the SRAM, restore it here*/
	if (hcd->has_sram && sram_restore(hcd)) {
		ehci_warn(ehci, "sram_restore failed, stop resuming.\n");
		return -EPERM;
	}

	/* The BIOS on systems with the Intel Panther Point chipset may or may
	 * not support xHCI natively.  That means that during system resume, it
	 * may switch the ports back to EHCI so that users can use their
	 * keyboard to select a kernel from GRUB after resume from hibernate.
	 *
	 * The BIOS is supposed to remember whether the OS had xHCI ports
	 * enabled before resume, and switch the ports back to xHCI when the
	 * BIOS/OS semaphore is written, but we all know we can't trust BIOS
	 * writers.
	 *
	 * Unconditionally switch the ports back to xHCI after a system resume.
	 * We can't tell whether the EHCI or xHCI controller will be resumed
	 * first, so we have to do the port switchover in both drivers.  Writing
	 * a '1' to the port switchover registers should have no effect if the
	 * port was already switched over.
	 */
	if (usb_is_intel_switchable_ehci(pdev))
		ehci_enable_xhci_companion();

	if (ehci_resume(hcd, hibernated) != 0)
		(void) ehci_pci_reinit(ehci, pdev);
	return 0;
}

#else

#define ehci_pci_suspend	NULL
#define ehci_pci_resume		NULL
#endif	/* CONFIG_PM */

/*
 * Called when the ehci_pci module is removed.
 */
static void ehci_pci_stop(struct usb_hcd *hcd)
{
	ehci_stop(hcd);
	/* Release sram when removed */
	if (hcd->has_sram)
		sram_deinit(hcd);
}

static struct hc_driver __read_mostly ehci_pci_hc_driver;

static const struct ehci_driver_overrides pci_overrides __initconst = {
	.reset =		ehci_pci_setup,
};

/*-------------------------------------------------------------------------*/

/* PCI driver selection metadata; PCI hotplugging uses this */
static const struct pci_device_id pci_ids [] = { {
	/* handle any USB 2.0 EHCI controller */
	PCI_DEVICE_CLASS(PCI_CLASS_SERIAL_USB_EHCI, ~0),
	.driver_data =	(unsigned long) &ehci_pci_hc_driver,
	}, {
	PCI_VDEVICE(STMICRO, PCI_DEVICE_ID_STMICRO_USB_HOST),
	.driver_data = (unsigned long) &ehci_pci_hc_driver,
	},
	{ /* end: all zeroes */ }
};
MODULE_DEVICE_TABLE(pci, pci_ids);

/* pci driver glue; this is a "new style" PCI driver module */
static struct pci_driver ehci_pci_driver = {
	.name =		(char *) hcd_name,
	.id_table =	pci_ids,

	.probe =	usb_hcd_pci_probe,
	.remove =	usb_hcd_pci_remove,
	.shutdown = 	usb_hcd_pci_shutdown,

#ifdef CONFIG_PM
	.driver =	{
		.pm =	&usb_hcd_pci_pm_ops
	},
#endif
};

#ifdef CONFIG_USB_EHCI_HCD_SPH
#include "ehci-sph-pci.c"
#define INTEL_MID_SPH_HOST_DRIVER	ehci_sph_driver
#endif

#if defined(CONFIG_USB_LANGWELL_OTG) || defined(CONFIG_USB_PENWELL_OTG)
#include "ehci-langwell-pci.c"
#define INTEL_MID_OTG_HOST_DRIVER	ehci_otg_driver
#endif

#ifdef CONFIG_USB_HCD_HSIC
#include "ehci-tangier-hsic-pci.c"
#define INTEL_MID_HSIC_HOST_DRIVER	ehci_hsic_driver
#endif

static int __init ehci_pci_init(void)
{
	int		retval;

	if (usb_disabled())
		return -ENODEV;

	pr_info("%s: " DRIVER_DESC "\n", hcd_name);

	ehci_init_driver(&ehci_pci_hc_driver, &pci_overrides);

	/* Entries for the PCI suspend/resume callbacks are special */
	ehci_pci_hc_driver.pci_suspend = ehci_pci_suspend;
	ehci_pci_hc_driver.pci_resume = ehci_pci_resume;

	/* Need to use seprate ehci_stop since need to hanlde sram */
	ehci_pci_hc_driver.stop = ehci_pci_stop;

#ifdef CONFIG_USB_HCD_HSIC
	retval = pci_register_driver(&INTEL_MID_HSIC_HOST_DRIVER);
	if (retval < 0)
		goto clean0;
#endif

#ifdef INTEL_MID_OTG_HOST_DRIVER
	retval = intel_mid_ehci_driver_register(&INTEL_MID_OTG_HOST_DRIVER);
	if (retval < 0) {
		pr_err("%s  register otg host driver failed, retval = %d\n",
				__func__, retval);
		goto clean1;
	}
#endif

#ifdef CONFIG_USB_EHCI_HCD_SPH
	if (sph_enabled()) {
		retval = pci_register_driver(&INTEL_MID_SPH_HOST_DRIVER);
		if (retval < 0) {
			pr_err("%s  register sph driver failed, retval = %d\n",
					__func__, retval);
			goto clean2;
		}
	}
#endif

	retval = pci_register_driver(&ehci_pci_driver);
	if (retval < 0) {
		pr_err("%s  register ehci pci driver failed, retval = %d\n",
				__func__, retval);
		goto clean3;
	}
	return retval;

clean3:
	pci_unregister_driver(&ehci_pci_driver);

#ifdef CONFIG_USB_EHCI_HCD_SPH
clean2:
	pci_unregister_driver(&INTEL_MID_SPH_HOST_DRIVER);
#endif

#ifdef INTEL_MID_OTG_HOST_DRIVER
clean1:
	intel_mid_ehci_driver_unregister(&INTEL_MID_OTG_HOST_DRIVER);
#endif

#ifdef CONFIG_USB_HCD_HSIC
clean0:
	pci_unregister_driver(&INTEL_MID_HSIC_HOST_DRIVER);
#endif
	return retval;
}
module_init(ehci_pci_init);

static void __exit ehci_pci_cleanup(void)
{
#ifdef INTEL_MID_OTG_HOST_DRIVER
	intel_mid_ehci_driver_unregister(&INTEL_MID_OTG_HOST_DRIVER);
#endif

#ifdef CONFIG_USB_EHCI_HCD_SPH
	if (sph_enabled())
		pci_unregister_driver(&INTEL_MID_SPH_HOST_DRIVER);
#endif

#ifdef CONFIG_USB_HCD_HSIC
	pci_unregister_driver(&INTEL_MID_HSIC_HOST_DRIVER);
#endif

	pci_unregister_driver(&ehci_pci_driver);
}
module_exit(ehci_pci_cleanup);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR("David Brownell");
MODULE_AUTHOR("Alan Stern");
MODULE_LICENSE("GPL");
