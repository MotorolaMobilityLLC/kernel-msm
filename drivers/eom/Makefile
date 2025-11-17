# SPDX-License-Identifier: GPL-2.0-only
# Makefile for drivers/eom

# Build eom_driver.ko if CONFIG_EOM_MSM is enabled
obj-$(CONFIG_EOM_MSM) += eom_driver.o

# Core source files (always included)
eom_driver-y := phy_core.o eom_main.o buffer_manager.o

# Conditional sources based on architecture
eom_driver-$(CONFIG_ARCH_CANOE) += pcie_eom_canoe_phy.o
eom_driver-$(CONFIG_ARCH_SERAPH) += pcie_eom_seraph_phy.o

# Also build pcie/pci_phy_access.ko if CONFIG_PCI_MSM_EOM is enabled
obj-$(CONFIG_PCI_MSM_EOM) += pcie/


# Header dependencies are automatically handled by the build system
