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

#ifndef __PLD_SNOC_H__
#define __PLD_SNOC_H__

#ifdef CONFIG_PLD_SNOC_ICNSS
#ifdef CONFIG_PLD_SNOC_ICNSS2
#include <soc/qcom/icnss2.h>
#else
#include <soc/qcom/icnss.h>
#endif
#endif
#include "pld_internal.h"

#ifndef CONFIG_PLD_SNOC_ICNSS
static inline int pld_snoc_register_driver(void)
{
	return 0;
}

static inline void pld_snoc_unregister_driver(void)
{
}
static inline int pld_snoc_wlan_enable(struct device *dev,
			struct pld_wlan_enable_cfg *config,
			enum pld_driver_mode mode, const char *host_version)
{
	return 0;
}
static inline int pld_snoc_wlan_disable(struct device *dev,
					enum pld_driver_mode mode)
{
	return 0;
}
static inline int pld_snoc_ce_request_irq(struct device *dev,
					  unsigned int ce_id,
					  irqreturn_t (*handler)(int, void *),
					  unsigned long flags,
					  const char *name, void *ctx)
{
	return 0;
}
static inline int pld_snoc_ce_free_irq(struct device *dev,
				       unsigned int ce_id, void *ctx)
{
	return 0;
}
static inline void pld_snoc_enable_irq(struct device *dev, unsigned int ce_id)
{
}
static inline void pld_snoc_disable_irq(struct device *dev, unsigned int ce_id)
{
}
static inline int pld_snoc_get_soc_info(struct device *dev, struct pld_soc_info *info)
{
	return 0;
}
static inline int pld_snoc_get_ce_id(struct device *dev, int irq)
{
	return 0;
}
static inline int pld_snoc_power_on(struct device *dev)
{
	return 0;
}
static inline int pld_snoc_power_off(struct device *dev)
{
	return 0;
}
static inline int pld_snoc_get_irq(struct device *dev, int ce_id)
{
	return 0;
}
static inline int pld_snoc_athdiag_read(struct device *dev, uint32_t offset,
					uint32_t memtype, uint32_t datalen,
					uint8_t *output)
{
	return 0;
}
static inline int pld_snoc_athdiag_write(struct device *dev, uint32_t offset,
					 uint32_t memtype, uint32_t datalen,
					 uint8_t *input)
{
	return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0))
static inline void *pld_snoc_smmu_get_domain(struct device *dev)
{
	return NULL;
}

#else
static inline void *pld_snoc_smmu_get_mapping(struct device *dev)
{
	return NULL;
}
#endif

static inline int pld_snoc_idle_restart(struct device *dev)
{
	return 0;
}

static inline int pld_snoc_idle_shutdown(struct device *dev)
{
	return 0;
}

static inline int pld_snoc_smmu_map(struct device *dev, phys_addr_t paddr,
				    uint32_t *iova_addr, size_t size)
{
	return 0;
}

static inline int pld_snoc_smmu_unmap(struct device *dev,
				      uint32_t iova_addr, size_t size)
{
	return 0;
}

static inline
unsigned int pld_snoc_socinfo_get_serial_number(struct device *dev)
{
	return 0;
}
static inline int pld_snoc_is_qmi_disable(struct device *dev)
{
	return 0;
}

static inline int pld_snoc_is_fw_down(struct device *dev)
{
	return 0;
}
static inline int pld_snoc_set_fw_log_mode(struct device *dev, u8 fw_log_mode)
{
	return 0;
}
static inline int pld_snoc_force_assert_target(struct device *dev)
{
	return 0;
}

static inline int pld_snoc_is_pdr(void)
{
	return 0;
}

static inline int pld_snoc_is_fw_rejuvenate(void)
{
	return 0;
}

static inline void pld_snoc_block_shutdown(bool status)
{
}

#ifdef FEATURE_WLAN_TIME_SYNC_FTM
static inline int
pld_snoc_get_audio_wlan_timestamp(struct device *dev,
				  enum pld_wlan_time_sync_trigger_type type,
				  uint64_t *ts)
{
	return 0;
}
#endif /* FEATURE_WLAN_TIME_SYNC_FTM */

#else
int pld_snoc_register_driver(void);
void pld_snoc_unregister_driver(void);
int pld_snoc_wlan_enable(struct device *dev,
			 struct pld_wlan_enable_cfg *config,
			 enum pld_driver_mode mode, const char *host_version);
int pld_snoc_wlan_disable(struct device *dev, enum pld_driver_mode mode);
int pld_snoc_get_soc_info(struct device *dev, struct pld_soc_info *info);

#ifdef FEATURE_WLAN_TIME_SYNC_FTM
/**
 * pld_snoc_get_audio_wlan_timestamp() - Get audio timestamp
 * @dev: device
 * @type: trigger type
 * @ts: timestamp
 *
 * Return audio timestamp to the ts.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
static inline int
pld_snoc_get_audio_wlan_timestamp(struct device *dev,
				  enum pld_wlan_time_sync_trigger_type type,
				  uint64_t *ts)
{
	if (!dev)
		return -ENODEV;

	return 0;
}
#endif /* FEATURE_WLAN_TIME_SYNC_FTM */
static inline int pld_snoc_ce_request_irq(struct device *dev,
					  unsigned int ce_id,
					  irqreturn_t (*handler)(int, void *),
					  unsigned long flags,
					  const char *name, void *ctx)
{
	if (!dev)
		return -ENODEV;

	return icnss_ce_request_irq(dev, ce_id, handler, flags, name, ctx);
}

static inline int pld_snoc_ce_free_irq(struct device *dev,
				       unsigned int ce_id, void *ctx)
{
	if (!dev)
		return -ENODEV;

	return icnss_ce_free_irq(dev, ce_id, ctx);
}

static inline void pld_snoc_enable_irq(struct device *dev, unsigned int ce_id)
{
	if (dev)
		icnss_enable_irq(dev, ce_id);
}

static inline void pld_snoc_disable_irq(struct device *dev, unsigned int ce_id)
{
	if (dev)
		icnss_disable_irq(dev, ce_id);
}

static inline int pld_snoc_get_ce_id(struct device *dev, int irq)
{
	if (!dev)
		return -ENODEV;

	return icnss_get_ce_id(dev, irq);
}

static inline int pld_snoc_get_irq(struct device *dev, int ce_id)
{
	if (!dev)
		return -ENODEV;

	return icnss_get_irq(dev, ce_id);
}

static inline int pld_snoc_power_on(struct device *dev)
{
	return icnss_power_on(dev);
}
static inline int pld_snoc_power_off(struct device *dev)
{
	return icnss_power_off(dev);
}
static inline int pld_snoc_athdiag_read(struct device *dev, uint32_t offset,
					uint32_t memtype, uint32_t datalen,
					uint8_t *output)
{
	return icnss_athdiag_read(dev, offset, memtype, datalen, output);
}
static inline int pld_snoc_athdiag_write(struct device *dev, uint32_t offset,
					 uint32_t memtype, uint32_t datalen,
					 uint8_t *input)
{
	return icnss_athdiag_write(dev, offset, memtype, datalen, input);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0))
static inline void *pld_snoc_smmu_get_domain(struct device *dev)
{
	return icnss_smmu_get_domain(dev);
}

#else
static inline void *pld_snoc_smmu_get_mapping(struct device *dev)
{
	return icnss_smmu_get_mapping(dev);
}
#endif

static inline int pld_snoc_smmu_map(struct device *dev, phys_addr_t paddr,
				    uint32_t *iova_addr, size_t size)
{
	return icnss_smmu_map(dev, paddr, iova_addr, size);
}

#ifdef CONFIG_SMMU_S1_UNMAP
static inline int pld_snoc_smmu_unmap(struct device *dev,
				      uint32_t iova_addr, size_t size)
{
	return icnss_smmu_unmap(dev, iova_addr, size);
}

#else
static inline int pld_snoc_smmu_unmap(struct device *dev,
				      uint32_t iova_addr, size_t size)
{
	return 0;
}
#endif

static inline
unsigned int pld_snoc_socinfo_get_serial_number(struct device *dev)
{
	return icnss_socinfo_get_serial_number(dev);
}

static inline int pld_snoc_is_fw_down(struct device *dev)
{
	return icnss_is_fw_down();
}

static inline int pld_snoc_is_qmi_disable(struct device *dev)
{
	if (!dev)
		return -ENODEV;

	return icnss_is_qmi_disable(dev);
}

static inline int pld_snoc_set_fw_log_mode(struct device *dev, u8 fw_log_mode)
{
	if (!dev)
		return -ENODEV;

	return icnss_set_fw_log_mode(dev, fw_log_mode);
}

static inline int pld_snoc_force_assert_target(struct device *dev)
{
	return icnss_trigger_recovery(dev);
}

static inline int pld_snoc_is_pdr(void)
{
	return icnss_is_pdr();
}

static inline int pld_snoc_is_fw_rejuvenate(void)
{
	return icnss_is_rejuvenate();
}

static inline void pld_snoc_block_shutdown(bool status)
{
	icnss_block_shutdown(status);
}

static inline int pld_snoc_idle_restart(struct device *dev)
{
	return icnss_idle_restart(dev);
}

static inline int pld_snoc_idle_shutdown(struct device *dev)
{
	return icnss_idle_shutdown(dev);
}
#endif
#endif
