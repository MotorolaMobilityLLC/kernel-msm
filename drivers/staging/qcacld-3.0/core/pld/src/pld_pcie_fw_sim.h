/*
 * Copyright (c) 2016-2020 The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef __PLD_PCIE_FW_SIM_H__
#define __PLD_PCIE_FW_SIM_H__

#include "pld_internal.h"

#if !defined(CONFIG_PLD_PCIE_FW_SIM) && !defined(CONFIG_PLD_IPCIE_FW_SIM)

static inline int pld_pcie_fw_sim_register_driver(void)
{
	return 0;
}

static inline void pld_pcie_fw_sim_unregister_driver(void)
{
}

static inline int pld_pcie_fw_sim_wlan_enable(struct device *dev,
					      struct pld_wlan_enable_cfg *cfg,
					      enum pld_driver_mode mode,
					      const char *host_version)
{
	return 0;
}

static inline int pld_pcie_fw_sim_wlan_disable(struct device *dev,
					       enum pld_driver_mode mode)
{
	return 0;
}

static inline void pld_pcie_fw_sim_link_down(struct device *dev)
{
}

static inline int pld_pcie_fw_sim_is_fw_down(struct device *dev)
{
	return 0;
}

static inline int pld_pcie_fw_sim_get_platform_cap(struct device *dev,
						   struct pld_platform_cap *cap)
{
	return 0;
}

static inline int pld_pcie_fw_sim_get_soc_info(struct device *dev,
					       struct pld_soc_info *info)
{
	return 0;
}

static inline int pld_pcie_fw_sim_get_user_msi_assignment(struct device *dev,
							  char *user_name,
							  int *num_vectors,
							  uint32_t *base_data,
							  uint32_t *base_vector)
{
	return -EINVAL;
}

static inline int pld_pcie_fw_sim_get_msi_irq(struct device *dev,
					      unsigned int vector)
{
	return 0;
}

static inline void pld_pcie_fw_sim_get_msi_address(struct device *dev,
						   uint32_t *msi_addr_low,
						   uint32_t *msi_addr_high)
{
}

static inline int pld_pcie_fw_sim_request_irq(struct device *dev, int irq,
					      irq_handler_t handler,
					      unsigned long irqflags,
					      const char *devname,
					      void *dev_data)
{
	return 0;
}

static inline int pld_pcie_fw_sim_read_config_word(struct device *dev,
						   int offset, uint16_t *val)
{
	return 0;
}

static inline int pld_pcie_fw_sim_free_irq(struct device *dev,
					   unsigned int ce_id, void *ctx)
{
	return 0;
}

static inline void pld_pcie_fw_sim_enable_irq(struct device *dev,
					      unsigned int irq)
{
}

static inline void pld_pcie_fw_sim_disable_irq(struct device *dev,
					       unsigned int irq)
{
}

static inline int pld_pcie_fw_sim_idle_shutdown(struct device *dev)
{
	return 0;
}

static inline int pld_pcie_fw_sim_idle_restart(struct device *dev)
{
	return 0;
}

static inline int pld_pcie_fw_sim_thermal_register(struct device *dev,
						   unsigned long max_state,
						   int mon_id)
{
	return 0;
}

static inline void pld_pcie_fw_sim_thermal_unregister(struct device *dev,
						      int mon_id)
{
}

static inline int pld_pcie_fw_sim_get_thermal_state(struct device *dev,
						    unsigned long *therm_state,
						    int mon_id)
{
	return 0;
}
#else
#include <net/cnss2.h>

int pld_pcie_fw_sim_wlan_enable(struct device *dev,
				struct pld_wlan_enable_cfg *config,
				enum pld_driver_mode mode,
				const char *host_version);
int pld_pcie_fw_sim_wlan_disable(struct device *dev, enum pld_driver_mode mode);
int pld_pcie_fw_sim_register_driver(void);
void pld_pcie_fw_sim_unregister_driver(void);
int pld_pcie_fw_sim_get_platform_cap(struct device *dev,
				     struct pld_platform_cap *cap);
int pld_pcie_fw_sim_get_soc_info(struct device *dev, struct pld_soc_info *info);

static inline int pld_pcie_fw_sim_get_user_msi_assignment(struct device *dev,
							  char *user_name,
							  int *num_vectors,
							  uint32_t *base_data,
							  uint32_t *base_vector)
{
	return cnss_fw_sim_get_user_msi_assignment(dev, user_name, num_vectors,
					    base_data, base_vector);
}

static inline int pld_pcie_fw_sim_get_msi_irq(struct device *dev,
					      unsigned int vector)
{
	return cnss_fw_sim_get_msi_irq(dev, vector);
}

static inline void pld_pcie_fw_sim_get_msi_address(struct device *dev,
						   uint32_t *msi_addr_low,
						   uint32_t *msi_addr_high)
{
	cnss_fw_sim_get_msi_address(dev, msi_addr_low, msi_addr_high);
}

static inline int pld_pcie_fw_sim_request_irq(struct device *dev, int irq,
					      irq_handler_t handler,
					      unsigned long irqflags,
					      const char *devname,
					      void *dev_data)
{
	return cnss_fw_sim_request_irq(dev, irq, handler,
				       irqflags, devname, dev_data);
}

static inline int pld_pcie_fw_sim_read_config_word(struct device *dev,
						   int offset, uint16_t *val)
{
	return cnss_fw_sim_read_config_word(dev, offset, val);
}

static inline int pld_pcie_fw_sim_free_irq(struct device *dev,
					   int irq, void *dev_data)
{
	return cnss_fw_sim_free_irq(dev, irq, dev_data);
}

static inline void pld_pcie_fw_sim_enable_irq(struct device *dev, int irq)
{
	cnss_fw_sim_enable_irq(dev, irq);
}

static inline void pld_pcie_fw_sim_disable_irq(struct device *dev, int irq)
{
	cnss_fw_sim_disable_irq(dev, irq);
}

static inline int pld_pcie_fw_sim_idle_shutdown(struct device *dev)
{
	return cnss_fw_sim_idle_shutdown(dev);
}

static inline int pld_pcie_fw_sim_idle_restart(struct device *dev)
{
	return cnss_fw_sim_idle_restart(dev);
}

static inline int pld_pcie_fw_sim_thermal_register(struct device *dev,
						   unsigned long max_state,
						   int mon_id)
{
	return cnss_fw_sim_thermal_cdev_register(dev, max_state, mon_id);
}

static inline void pld_pcie_fw_sim_thermal_unregister(struct device *dev,
						      int mon_id)
{
	cnss_fw_sim_thermal_cdev_unregister(dev, mon_id);
}

static inline int pld_pcie_fw_sim_get_thermal_state(struct device *dev,
						    unsigned long *therm_state,
						    int mon_id)
{
	return cnss_fw_sim_get_curr_therm_cdev_state(dev, therm_state,
						     mon_id);
}

#endif
#endif
