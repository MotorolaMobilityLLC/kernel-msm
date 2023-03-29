/*
 * Copyright (c) 2016-2021 The Linux Foundation. All rights reserved.
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

#ifndef __PLD_IPCI_H__
#define __PLD_IPCI_H__

#ifdef CONFIG_PLD_IPCI_ICNSS
#include <soc/qcom/icnss2.h>
#endif
#include "pld_internal.h"

#ifndef CONFIG_PLD_IPCI_ICNSS
static inline int pld_ipci_register_driver(void)
{
	return 0;
}

static inline void pld_ipci_unregister_driver(void)
{
}

static inline int pld_ipci_wlan_enable(struct device *dev,
				       struct pld_wlan_enable_cfg *config,
				       enum pld_driver_mode mode,
				       const char *host_version)
{
	return 0;
}

static inline int pld_ipci_wlan_disable(struct device *dev,
					enum pld_driver_mode mode)
{
	return 0;
}

static inline int pld_ipci_get_soc_info(struct device *dev,
					struct pld_soc_info *info)
{
	return 0;
}

static inline int pld_ipci_power_on(struct device *dev)
{
	return 0;
}

static inline int pld_ipci_power_off(struct device *dev)
{
	return 0;
}

static inline int pld_ipci_idle_restart(struct device *dev)
{
	return 0;
}

static inline int pld_ipci_idle_shutdown(struct device *dev)
{
	return 0;
}

static inline int pld_ipci_force_assert_target(struct device *dev)
{
	return -EINVAL;
}

static inline int pld_ipci_get_user_msi_assignment(struct device *dev,
						   char *user_name,
						   int *num_vectors,
						   uint32_t *user_base_data,
						   uint32_t *base_vector)
{
	return 0;
}

static inline int pld_ipci_get_msi_irq(struct device *dev, unsigned int vector)
{
	return 0;
}

static inline void pld_ipci_get_msi_address(struct device *dev,
					    uint32_t *msi_addr_low,
					    uint32_t *msi_addr_high)
{
}

static inline int pld_ipci_is_fw_down(struct device *dev)
{
	return 0;
}

static inline int pld_ipci_set_fw_log_mode(struct device *dev, u8 fw_log_mode)
{
	return 0;
}

static inline void *pld_ipci_smmu_get_domain(struct device *dev)
{
	return NULL;
}

static inline int pld_ipci_smmu_map(struct device *dev, phys_addr_t paddr,
				    uint32_t *iova_addr, size_t size)
{
	return 0;
}

static inline int pld_ipci_smmu_unmap(struct device *dev,
				      uint32_t iova_addr, size_t size)
{
	return 0;
}

static inline int pld_ipci_force_wake_request(struct device *dev)
{
	return 0;
}

static inline int pld_ipci_force_wake_release(struct device *dev)
{
	return 0;
}

static inline int pld_ipci_is_device_awake(struct device *dev)
{
	return 0;
}

static inline int pld_ipci_athdiag_read(struct device *dev, uint32_t offset,
					uint32_t memtype, uint32_t datalen,
					uint8_t *output)
{
	return 0;
}

static inline int pld_ipci_athdiag_write(struct device *dev, uint32_t offset,
					 uint32_t memtype, uint32_t datalen,
					 uint8_t *input)
{
	return 0;
}

static inline int
pld_ipci_qmi_send(struct device *dev, int type, void *cmd,
		  int cmd_len, void *cb_ctx,
		  int (*cb)(void *ctx, void *event, int event_len))
{
	return 0;
}

static inline int pld_ipci_thermal_register(struct device *dev,
					    unsigned long max_state,
					    int mon_id)
{
	return 0;
}

static inline void pld_ipci_thermal_unregister(struct device *dev,
					       int mon_id)
{
}

static inline int pld_ipci_get_thermal_state(struct device *dev,
					     unsigned long *thermal_state,
					     int mon_id)
{
	return 0;
}

static inline int pld_ipci_exit_power_save(struct device *dev)
{
	return 0;
}

static inline int pld_ipci_prevent_l1(struct device *dev)
{
	return 0;
}

static inline void pld_ipci_allow_l1(struct device *dev)
{
}

static inline int pld_ipci_mhi_state(struct device *dev)
{
	return 0;
}

static inline int pld_ipci_is_pci_ep_awake(struct device *dev)
{
	return 0;
}
#else
int pld_ipci_register_driver(void);
void pld_ipci_unregister_driver(void);
int pld_ipci_wlan_enable(struct device *dev,
			 struct pld_wlan_enable_cfg *config,
			 enum pld_driver_mode mode, const char *host_version);
int pld_ipci_wlan_disable(struct device *dev, enum pld_driver_mode mode);
int pld_ipci_get_soc_info(struct device *dev, struct pld_soc_info *info);

static inline int pld_ipci_power_on(struct device *dev)
{
	return icnss_power_on(dev);
}

static inline int pld_ipci_power_off(struct device *dev)
{
	return icnss_power_off(dev);
}

static inline int pld_ipci_idle_restart(struct device *dev)
{
	return icnss_idle_restart(dev);
}

static inline int pld_ipci_idle_shutdown(struct device *dev)
{
	return icnss_idle_shutdown(dev);
}

static inline int pld_ipci_force_assert_target(struct device *dev)
{
	return icnss_trigger_recovery(dev);
}

static inline int pld_ipci_get_user_msi_assignment(struct device *dev,
						   char *user_name,
						   int *num_vectors,
						   uint32_t *user_base_data,
						   uint32_t *base_vector)
{
	return icnss_get_user_msi_assignment(dev, user_name, num_vectors,
					    user_base_data, base_vector);
}

static inline int pld_ipci_get_msi_irq(struct device *dev, unsigned int vector)
{
	return icnss_get_msi_irq(dev, vector);
}

static inline void pld_ipci_get_msi_address(struct device *dev,
					    uint32_t *msi_addr_low,
					    uint32_t *msi_addr_high)
{
	icnss_get_msi_address(dev, msi_addr_low, msi_addr_high);
}

static inline int pld_ipci_is_fw_down(struct device *dev)
{
	return icnss_is_fw_down();
}

static inline int pld_ipci_set_fw_log_mode(struct device *dev, u8 fw_log_mode)
{
	return icnss_set_fw_log_mode(dev, fw_log_mode);
}

static inline void *pld_ipci_smmu_get_domain(struct device *dev)
{
	return icnss_smmu_get_domain(dev);
}

static inline int pld_ipci_smmu_map(struct device *dev, phys_addr_t paddr,
				    uint32_t *iova_addr, size_t size)
{
	return icnss_smmu_map(dev, paddr, iova_addr, size);
}

#ifdef CONFIG_SMMU_S1_UNMAP
static inline int pld_ipci_smmu_unmap(struct device *dev,
				      uint32_t iova_addr, size_t size)
{
	return icnss_smmu_unmap(dev, iova_addr, size);
}

#else
static inline int pld_ipci_smmu_unmap(struct device *dev,
				      uint32_t iova_addr, size_t size)
{
	return 0;
}
#endif

static inline int pld_ipci_force_wake_request(struct device *dev)
{
	return icnss_force_wake_request(dev);
}

static inline int pld_ipci_force_wake_release(struct device *dev)
{
	return icnss_force_wake_release(dev);
}

static inline int pld_ipci_is_device_awake(struct device *dev)
{
	return icnss_is_device_awake(dev);
}

static inline int pld_ipci_athdiag_read(struct device *dev, uint32_t offset,
					uint32_t memtype, uint32_t datalen,
					uint8_t *output)
{
	return icnss_athdiag_read(dev, offset, memtype, datalen, output);
}

static inline int pld_ipci_athdiag_write(struct device *dev, uint32_t offset,
					 uint32_t memtype, uint32_t datalen,
					 uint8_t *input)
{
	return icnss_athdiag_write(dev, offset, memtype, datalen, input);
}

static inline int
pld_ipci_qmi_send(struct device *dev, int type, void *cmd,
		  int cmd_len, void *cb_ctx,
		  int (*cb)(void *ctx, void *event, int event_len))
{
	return icnss_qmi_send(dev, type, cmd, cmd_len, cb_ctx, cb);
}

static inline int pld_ipci_thermal_register(struct device *dev,
					    unsigned long max_state,
					    int mon_id)
{
	return icnss_thermal_cdev_register(dev, max_state, mon_id);
}

static inline void pld_ipci_thermal_unregister(struct device *dev,
					       int mon_id)
{
	icnss_thermal_cdev_unregister(dev, mon_id);
}

static inline int pld_ipci_get_thermal_state(struct device *dev,
					     unsigned long *thermal_state,
					     int mon_id)
{
	return icnss_get_curr_therm_cdev_state(dev, thermal_state, mon_id);
}

static inline int pld_ipci_exit_power_save(struct device *dev)
{
	return icnss_exit_power_save(dev);
}

static inline int pld_ipci_prevent_l1(struct device *dev)
{
	return icnss_prevent_l1(dev);
}

static inline void pld_ipci_allow_l1(struct device *dev)
{
	icnss_allow_l1(dev);
}

static inline int pld_ipci_is_pci_ep_awake(struct device *dev)
{
	return icnss_is_pci_ep_awake(dev);
}

static inline int pld_ipci_mhi_state(struct device *dev)
{
	return icnss_get_mhi_state(dev);
}
#endif
#endif
