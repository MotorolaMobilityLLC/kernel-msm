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

#ifndef __PLD_SDIO_H__
#define __PLD_SDIO_H__

#ifdef CONFIG_PLD_SDIO_CNSS
#include <net/cnss.h>
#endif
#include "pld_common.h"

#ifdef DYNAMIC_SINGLE_CHIP
#define PREFIX DYNAMIC_SINGLE_CHIP "/"
#else

#ifdef MULTI_IF_NAME
#define PREFIX MULTI_IF_NAME "/"
#else
#define PREFIX ""
#endif

#endif

#define PLD_QCA9377_REV1_1_VERSION          0x5020001
#define PLD_QCA9379_REV1_VERSION            0x5040000

#ifndef CONFIG_CNSS
#define PLD_AR6004_VERSION_REV1_3           0x31c8088a
#define PLD_AR9888_REV2_VERSION             0x4100016c
#define PLD_AR6320_REV1_VERSION             0x5000000
#define PLD_AR6320_REV1_1_VERSION           0x5000001
#define PLD_AR6320_REV1_3_VERSION           0x5000003
#define PLD_AR6320_REV2_1_VERSION           0x5010000
#define PLD_AR6320_REV3_VERSION             0x5020000
#define PLD_AR6320_REV3_2_VERSION           0x5030000
#define PLD_AR6320_DEV_VERSION              0x1000000


#endif

#ifndef CONFIG_SDIO
static inline int pld_sdio_register_driver(void)
{
	return 0;
}

static inline void pld_sdio_unregister_driver(void)
{
}

static inline
int pld_sdio_get_fw_files_for_target(struct pld_fw_files *pfw_files,
				     u32 target_type, u32 target_version)
{
	return 0;
}
static inline uint8_t *pld_sdio_get_wlan_mac_address(struct device *dev,
						     uint32_t *num)
{
	*num = 0;
	return NULL;
}
#else
int pld_sdio_register_driver(void);
void pld_sdio_unregister_driver(void);
int pld_sdio_get_fw_files_for_target(struct pld_fw_files *pfw_files,
				     u32 target_type, u32 target_version);
#ifdef CONFIG_CNSS
static inline uint8_t *pld_sdio_get_wlan_mac_address(struct device *dev,
						     uint32_t *num)
{
	return cnss_common_get_wlan_mac_address(dev, num);
}
#else
static inline uint8_t *pld_sdio_get_wlan_mac_address(struct device *dev,
						     uint32_t *num)
{
	*num = 0;
	return NULL;
}
#endif
#endif

#ifdef CONFIG_PLD_SDIO_CNSS
static inline void *pld_sdio_get_virt_ramdump_mem(struct device *dev,
		unsigned long *size)
{
	return cnss_common_get_virt_ramdump_mem(dev, size);
}

static inline void pld_sdio_release_virt_ramdump_mem(void *address)
{
}

static inline void pld_sdio_device_crashed(struct device *dev)
{
	cnss_common_device_crashed(dev);
}
static inline bool pld_sdio_is_fw_dump_skipped(void)
{
	return cnss_get_restart_level() == CNSS_RESET_SUBSYS_COUPLED;
}

static inline void pld_sdio_device_self_recovery(struct device *dev)
{
	cnss_common_device_self_recovery(dev);
}

static inline bool pld_sdio_platform_driver_support(void)
{
	return true;
}
#else
static inline void *pld_sdio_get_virt_ramdump_mem(struct device *dev,
		unsigned long *size)
{
	size_t length = 0;
	int flags = GFP_KERNEL;

	length = TOTAL_DUMP_SIZE;

	if (!size)
		return NULL;

	*size = (unsigned long)length;

	if (in_interrupt() || irqs_disabled() || in_atomic())
		flags = GFP_ATOMIC;

	return kzalloc(length, flags);
}

static inline void pld_sdio_release_virt_ramdump_mem(void *address)
{
	kfree(address);
}

static inline void pld_sdio_device_crashed(struct device *dev)
{
}
static inline bool pld_sdio_is_fw_dump_skipped(void)
{
	return false;
}

static inline void pld_sdio_device_self_recovery(struct device *dev)
{
}

static inline bool pld_sdio_platform_driver_support(void)
{
	return false;
}
#endif

#ifdef CONFIG_PLD_SDIO_CNSS
/**
 * pld_hif_sdio_get_virt_ramdump_mem() - Get virtual ramdump memory
 * @dev: device
 * @size: buffer to virtual memory size
 *
 * Return: virtual ramdump memory address
 */
static inline void *pld_hif_sdio_get_virt_ramdump_mem(struct device *dev,
						unsigned long *size)
{
	return cnss_common_get_virt_ramdump_mem(dev, size);
}

/**
 * pld_hif_sdio_release_ramdump_mem() - Release virtual ramdump memory
 * @address: virtual ramdump memory address
 *
 * Return: void
 */
static inline void pld_hif_sdio_release_ramdump_mem(unsigned long *address)
{
}
#else
#ifdef CONFIG_PLD_SDIO_CNSS2
#include <net/cnss2.h>

/**
 * pld_sdio_get_sdio_al_client_handle() - Get the sdio al client handle
 * @func: SDIO function pointer
 *
 * Return: On success return client handle from al via cnss, else NULL
 */
static inline struct sdio_al_client_handle *pld_sdio_get_sdio_al_client_handle
(
struct sdio_func *func
)
{
	if (!func)
		return NULL;

	return cnss_sdio_wlan_get_sdio_al_client_handle(func);
}

/**
 * pld_sdio_register_sdio_al_channel() - Register channel with sdio al
 * @al_client: SDIO al client handle
 * @ch_data: SDIO client channel data
 *
 * Return: Channel handle on success, else null
 */
static inline struct sdio_al_channel_handle *pld_sdio_register_sdio_al_channel
(
struct sdio_al_client_handle *al_client,
struct sdio_al_channel_data *ch_data
)
{
	if (!al_client || !ch_data)
		return NULL;

	return cnss_sdio_wlan_register_sdio_al_channel(ch_data);
}

/**
 * pld_sdio_unregister_sdio_al_channel() - Unregister the sdio al channel
 * @ch_handle: SDIO al channel handle
 *
 * Return: None
 */
static inline void pld_sdio_unregister_sdio_al_channel
(
struct sdio_al_channel_handle *ch_handle
)
{
	cnss_sdio_wlan_unregister_sdio_al_channel(ch_handle);
}

int pld_sdio_wlan_enable(struct device *dev, struct pld_wlan_enable_cfg *config,
			 enum pld_driver_mode mode, const char *host_version);
#else
static inline int pld_sdio_wlan_enable(struct device *dev,
				       struct pld_wlan_enable_cfg *config,
				       enum pld_driver_mode mode,
				       const char *host_version)
{
	return 0;
}
#endif /* CONFIG_PLD_SDIO_CNSS2 */

/**
 * pld_hif_sdio_get_virt_ramdump_mem() - Get virtual ramdump memory
 * @dev: device
 * @size: buffer to virtual memory size
 *
 * Return: virtual ramdump memory address
 */
static inline void *pld_hif_sdio_get_virt_ramdump_mem(struct device *dev,
						unsigned long *size)
{
	size_t length = 0;
	int flags = GFP_KERNEL;

	length = TOTAL_DUMP_SIZE;

	if (size)
		*size = (unsigned long)length;

	if (in_interrupt() || irqs_disabled() || in_atomic())
		flags = GFP_ATOMIC;

	return kzalloc(length, flags);
}

/**
 * pld_hif_sdio_release_ramdump_mem() - Release virtual ramdump memory
 * @address: virtual ramdump memory address
 *
 * Return: void
 */
static inline void pld_hif_sdio_release_ramdump_mem(unsigned long *address)
{
	if (address)
		kfree(address);
}
#endif
#endif
