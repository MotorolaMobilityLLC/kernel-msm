/*
 * Copyright (c) 2013 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
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

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */


#ifndef _ADF_CMN_OS_IO_PVT_H
#define _ADF_CMN_OS_IO_PVT_H

#include <asm/io.h>
#include <asm/byteorder.h>

#ifdef QCA_PARTNER_PLATFORM
#include "ath_carr_pltfrm.h"
#else
#include <linux/byteorder/generic.h>
#endif


#define __adf_os_reg_read8(_dev, _addr)     \
  readb((volatile void __iomem *)((_dev)->res.vaddr + (_addr)))

#define __adf_os_reg_read16(_dev, _addr)    \
  readw((volatile void __iomem *)((_dev)->res.vaddr + (_addr)))

#define __adf_os_reg_read32(_dev, _addr)    \
  readl((volatile void __iomem *)((_dev)->res.vaddr + (_addr)))

#define __adf_os_reg_read64(_dev, _addr)    \
  readq((volatile void __iomem *)((_dev)->res.vaddr + (_addr)))


#define __adf_os_reg_write8(_dev, _addr, _val)      \
  writeb(_val, (volatile void __iomem *)((_dev)->res.vaddr + (_addr)))

#define __adf_os_reg_write16(_dev, _addr, _val)     \
  writew(_val, (volatile void __iomem *)((_dev)->res.vaddr + (_addr)))

#define __adf_os_reg_write32(_dev, _addr, _val)     \
  writel(_val, (volatile void __iomem *)((_dev)->res.vaddr + (_addr)))

#define __adf_os_reg_write64(_dev, _addr, _val)     \
  writeq(_val, (volatile void __iomem *)((_dev)->res.vaddr + (_addr)))

#define __adf_os_ioremap(_addr, _len) \
  ioremap(_addr, _len)


#define __adf_os_ntohs                      ntohs
#define __adf_os_ntohl                      ntohl

#define __adf_os_htons                      htons
#define __adf_os_htonl                      htonl

#define __adf_os_cpu_to_le16                cpu_to_le16
#define __adf_os_cpu_to_le32                cpu_to_le32
#define __adf_os_cpu_to_le64                cpu_to_le64

#define __adf_os_cpu_to_be16                cpu_to_be16
#define __adf_os_cpu_to_be32                cpu_to_be32
#define __adf_os_cpu_to_be64                cpu_to_be64

#define __adf_os_le16_to_cpu                le16_to_cpu
#define __adf_os_le32_to_cpu                le32_to_cpu
#define __adf_os_le64_to_cpu                le64_to_cpu

#define __adf_os_be16_to_cpu                be16_to_cpu
#define __adf_os_be32_to_cpu                be32_to_cpu
#define __adf_os_be64_to_cpu                be64_to_cpu

#endif /*_ADF_CMN_OS_IO_PVT_H*/
