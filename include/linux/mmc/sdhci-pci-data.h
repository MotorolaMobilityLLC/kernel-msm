#ifndef LINUX_MMC_SDHCI_PCI_DATA_H
#define LINUX_MMC_SDHCI_PCI_DATA_H

struct pci_dev;

struct sdhci_pci_data {
	struct pci_dev	*pdev;
	void __iomem *flis_addr; /* eMMC0 FLIS registers start address */
	int		slotno;
	int		rst_n_gpio; /* Set to -EINVAL if unused */
	int		cd_gpio;    /* Set to -EINVAL if unused */
	int		quirks;
	int		quirks2;
	int		platform_quirks; /* Platform related quirks */
	int		(*setup)(struct sdhci_pci_data *data);
	void		(*cleanup)(struct sdhci_pci_data *data);
	int		(*power_up)(void *data);
	void		(*register_embedded_control)(void *dev_id,
			   void (*virtual_cd)(void *dev_id, int card_present));
	int		(*flis_check)(void *data, unsigned int host_clk,
				unsigned int clk);
	int		(*flis_dump)(void *data);
};

/* Some Pre-Silicon platform not support all SDHCI HCs of the SoC */
#define PLFM_QUIRK_NO_HOST_CTRL_HW	(1<<0)
/* Some Pre-Silicon platform do not support eMMC boot partition access */
#define PLFM_QUIRK_NO_EMMC_BOOT_PART	(1<<1)
/* Some Pre-Silicon platform do not support eMMC or SD High Speed */
#define PLFM_QUIRK_NO_HIGH_SPEED	(1<<2)
/* For the platform which don't have the SD card slot */
#define PLFM_QUIRK_NO_SDCARD_SLOT	(1<<3)

extern struct sdhci_pci_data *(*sdhci_pci_get_data)(struct pci_dev *pdev,
				int slotno);

#endif
