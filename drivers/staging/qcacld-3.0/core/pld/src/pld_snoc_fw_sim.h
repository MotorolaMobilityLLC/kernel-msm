/*
 * Copyright (c) 2016-2019 The Linux Foundation. All rights reserved.
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

#ifndef __PLD_SNOC_FW_SIM_H__
#define __PLD_SNOC_FW_SIM_H__

#include "pld_internal.h"

#ifndef CONFIG_PLD_SNOC_FW_SIM
static inline int pld_snoc_fw_sim_register_driver(void)
{
	return 0;
}

static inline void pld_snoc_fw_sim_unregister_driver(void)
{
}

static inline int pld_snoc_fw_sim_wlan_enable(struct device *dev,
					      struct pld_wlan_enable_cfg *cfg,
					      enum pld_driver_mode mode,
					      const char *host_version)
{
	return 0;
}

static inline int pld_snoc_fw_sim_wlan_disable(struct device *dev,
					       enum pld_driver_mode mode)
{
	return 0;
}

static inline int pld_snoc_fw_sim_ce_request_irq(struct device *dev,
						 unsigned int ce_id,
					  irqreturn_t (*handler)(int, void *),
					  unsigned long flags,
					  const char *name, void *ctx)
{
	return 0;
}

static inline int pld_snoc_fw_sim_ce_free_irq(struct device *dev,
					      unsigned int ce_id, void *ctx)
{
	return 0;
}

static inline void pld_snoc_fw_sim_enable_irq(struct device *dev,
					      unsigned int ce_id)
{
}

static inline void pld_snoc_fw_sim_disable_irq(struct device *dev,
					       unsigned int ce_id)
{
}

static inline int pld_snoc_fw_sim_get_soc_info(struct device *dev,
					       struct pld_soc_info *info)
{
	return 0;
}

static inline int pld_snoc_fw_sim_get_ce_id(struct device *dev, int irq)
{
	return 0;
}

static inline int pld_snoc_fw_sim_get_irq(struct device *dev, int ce_id)
{
	return 0;
}

static inline int pld_snoc_fw_sim_is_fw_down(struct device *dev)
{
	return 0;
}

static inline int pld_snoc_fw_sim_idle_shutdown(struct device *dev)
{
	return 0;
}

static inline int pld_snoc_fw_sim_idle_restart(struct device *dev)
{
	return 0;
}

#else
#include <soc/icnss.h>

int pld_snoc_fw_sim_register_driver(void);
void pld_snoc_fw_sim_unregister_driver(void);
int pld_snoc_fw_sim_wlan_enable(struct device *dev,
				struct pld_wlan_enable_cfg *config,
				enum pld_driver_mode mode,
				const char *host_version);
int pld_snoc_fw_sim_wlan_disable(struct device *dev, enum pld_driver_mode mode);
int pld_snoc_fw_sim_get_soc_info(struct device *dev, struct pld_soc_info *info);

static inline int pld_snoc_fw_sim_ce_request_irq(struct device *dev,
						 unsigned int ce_id,
					irqreturn_t (*handler)(int, void *),
					unsigned long flags,
					const char *name, void *ctx)
{
	if (!dev)
		return -ENODEV;

	return icnss_ce_request_irq(dev, ce_id, handler, flags, name, ctx);
}

static inline int pld_snoc_fw_sim_ce_free_irq(struct device *dev,
					      unsigned int ce_id, void *ctx)
{
	if (!dev)
		return -ENODEV;

	return icnss_ce_free_irq(dev, ce_id, ctx);
}

static inline void pld_snoc_fw_sim_enable_irq(struct device *dev,
					      unsigned int ce_id)
{
	if (dev)
		icnss_enable_irq(dev, ce_id);
}

static inline void pld_snoc_fw_sim_disable_irq(struct device *dev,
					       unsigned int ce_id)
{
	if (dev)
		icnss_disable_irq(dev, ce_id);
}

static inline int pld_snoc_fw_sim_get_ce_id(struct device *dev, int irq)
{
	if (!dev)
		return -ENODEV;

	return icnss_get_ce_id(dev, irq);
}

static inline int pld_snoc_fw_sim_get_irq(struct device *dev, int ce_id)
{
	if (!dev)
		return -ENODEV;

	return icnss_get_irq(dev, ce_id);
}

static inline int pld_snoc_fw_sim_is_fw_down(struct device *dev)
{
	return icnss_is_fw_down();
}

static inline int pld_snoc_fw_sim_idle_shutdown(struct device *dev)
{
	return icnss_idle_shutdown(dev);
}

static inline int pld_snoc_fw_sim_idle_restart(struct device *dev)
{
	return icnss_idle_restart(dev);
}

#endif
#endif
