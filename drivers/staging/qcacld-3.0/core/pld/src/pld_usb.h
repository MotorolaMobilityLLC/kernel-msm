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
#ifndef __PLD_USB_H__
#define __PLD_USB_H__

#include "pld_common.h"

#ifdef HIF_USB
int pld_usb_register_driver(void);
void pld_usb_unregister_driver(void);
int pld_usb_get_ce_id(int irq);
int pld_usb_wlan_enable(struct device *dev, struct pld_wlan_enable_cfg *config,
			enum pld_driver_mode mode, const char *host_version);
int pld_usb_is_fw_down(struct device *dev);
/**
 * pld_usb_athdiag_read() - Read data from WLAN FW through USB interface
 * @dev: pointer of device
 * @offset: address offset
 * @memtype: memory type
 * @datalen: data length
 * @output: pointer of output buffer
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_usb_athdiag_read(struct device *dev, uint32_t offset,
			 uint32_t memtype, uint32_t datalen,
			 uint8_t *output);
/**
 * pld_usb_athdiag_write() - Write data to WLAN FW through USB interface
 * @dev: pointer of device
 * @offset: address offset
 * @memtype: memory type
 * @datalen: data length
 * @output: pointer of input buffer
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_usb_athdiag_write(struct device *dev, uint32_t offset,
			  uint32_t memtype, uint32_t datalen,
			  uint8_t *input);

#else
static inline int pld_usb_register_driver(void)
{
	return 0;
}

static inline void pld_usb_unregister_driver(void)
{}

static inline int pld_usb_wlan_enable(struct device *dev,
				      struct pld_wlan_enable_cfg *config,
				      enum pld_driver_mode mode,
				      const char *host_version)
{
	return 0;
}

static inline int pld_usb_is_fw_down(struct device *dev)
{
	return  0;
}

static inline int pld_usb_athdiag_read(struct device *dev, uint32_t offset,
				       uint32_t memtype, uint32_t datalen,
				       uint8_t *output)
{
	return 0;
}

static inline int pld_usb_athdiag_write(struct device *dev, uint32_t offset,
					uint32_t memtype, uint32_t datalen,
					uint8_t *input)
{
	return 0;
}
#endif

static inline int
pld_usb_get_fw_files_for_target(struct pld_fw_files *pfw_files,
				 u32 target_type, u32 target_version)
{
	pld_get_default_fw_files(pfw_files);
	return 0;
}

#endif /*__PLD_USB_H__*/
