/*
 * Copyright (c) 2011 The Linux Foundation. All rights reserved.
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



/**
 * @ingroup adf_os_public
 * @file adf_os_io.h
 * This file abstracts I/O operations.
 */

#ifndef _ADF_OS_IO_H
#define _ADF_OS_IO_H

#include <adf_os_io_pvt.h>


/**
 * @brief Read an 8-bit register value
 *
 * @param[in] osdev   platform device object
 * @param[in] addr    register addr
 *
 * @return An 8-bit register value.
 */
#define adf_os_reg_read8(osdev, addr)         __adf_os_reg_read8(osdev, addr)

/**
 * @brief Read a 16-bit register value
 *
 * @param[in] osdev   platform device object
 * @param[in] addr    register addr
 *
 * @return A 16-bit register value.
 */
#define adf_os_reg_read16(osdev, addr)        __adf_os_reg_read16(osdev, addr)

/**
 * @brief Read a 32-bit register value
 *
 * @param[in] osdev   platform device object
 * @param[in] addr    register addr
 *
 * @return A 32-bit register value.
 */
#define adf_os_reg_read32(osdev, addr)        __adf_os_reg_read32(osdev, addr)

/**
 * @brief Read a 64-bit register value
 *
 * @param[in] osdev   platform device object
 * @param[in] addr    register addr
 *
 * @return A 64-bit register value.
 */
#define adf_os_reg_read64(osdev, addr)        __adf_os_reg_read64(osdev, addr)

/**
 * @brief Write an 8-bit value into register
 *
 * @param[in] osdev   platform device object
 * @param[in] addr    register addr
 * @param[in] b       the 8-bit value to be written
 */
#define adf_os_reg_write8(osdev, addr, b)     __adf_os_reg_write8(osdev, addr, b)

/**
 * @brief Write a 16-bit value into register
 *
 * @param[in] osdev   platform device object
 * @param[in] addr    register addr
 * @param[in] w       the 16-bit value to be written
 */
#define adf_os_reg_write16(osdev, addr, w)    __adf_os_reg_write16(osdev, addr, w)

/**
 * @brief Write a 32-bit value into register
 *
 * @param[in] osdev   platform device object
 * @param[in] addr    register addr
 * @param[in] l       the 32-bit value to be written
 */
#define adf_os_reg_write32(osdev, addr, l)    __adf_os_reg_write32(osdev, addr, l)

/**
 * @brief Write a 64-bit value into register
 *
 * @param[in] osdev   platform device object
 * @param[in] addr    register addr
 * @param[in] q       the 64-bit value to be written
 */
#define adf_os_reg_write64(osdev, addr, q)    __adf_os_reg_write64(osdev, addr, q)


/**
 * @brief io remaps a physical address to a i/o address
 *
 * @param[in] addr   physical address
 * @param[in] size   size of memeory to be remaped
 */
#define adf_os_ioremap(addr, size) __adf_os_ioremap(addr, size)

/**
 * @brief Convert a 16-bit value from network byte order to host byte order
 */
#define adf_os_ntohs(x)                         __adf_os_ntohs(x)

/**
 * @brief Convert a 32-bit value from network byte order to host byte order
 */
#define adf_os_ntohl(x)                         __adf_os_ntohl(x)

/**
 * @brief Convert a 16-bit value from host byte order to network byte order
 */
#define adf_os_htons(x)                         __adf_os_htons(x)

/**
 * @brief Convert a 32-bit value from host byte order to network byte order
 */
#define adf_os_htonl(x)                         __adf_os_htonl(x)

/**
 * @brief Convert a 16-bit value from CPU byte order to big-endian byte order
 */
#define adf_os_cpu_to_be16(x)                   __adf_os_cpu_to_be16(x)

/**
 * @brief Convert a 32-bit value from CPU byte order to big-endian byte order
 */
#define adf_os_cpu_to_be32(x)                   __adf_os_cpu_to_be32(x)

/**
 * @brief Convert a 64-bit value from CPU byte order to big-endian byte order
 */
#define adf_os_cpu_to_be64(x)                   __adf_os_cpu_to_be64(x)

/**
 * @brief Convert a 16-bit value from CPU byte order to little-endian byte order
 */
#define adf_os_cpu_to_le16(x)                   __adf_os_cpu_to_le16(x)

/**
 * @brief Convert a 32-bit value from CPU byte order to little-endian byte order
 */
#define adf_os_cpu_to_le32(x)                   __adf_os_cpu_to_le32(x)

/**
 * @brief Convert a 64-bit value from CPU byte order to little-endian byte order
 */
#define adf_os_cpu_to_le64(x)                   __adf_os_cpu_to_le64(x)

/**
 * @brief Convert a 16-bit value from big-endian byte order to CPU byte order
 */
#define adf_os_be16_to_cpu(x)                   __adf_os_be16_to_cpu(x)

/**
 * @brief Convert a 32-bit value from big-endian byte order to CPU byte order
 */
#define adf_os_be32_to_cpu(x)                   __adf_os_be32_to_cpu(x)

/**
 * @brief Convert a 64-bit value from big-endian byte order to CPU byte order
 */
#define adf_os_be64_to_cpu(x)                   __adf_os_be64_to_cpu(x)

/**
 * @brief Convert a 16-bit value from little-endian byte order to CPU byte order
 */
#define adf_os_le16_to_cpu(x)                   __adf_os_le16_to_cpu(x)

/**
 * @brief Convert a 32-bit value from little-endian byte order to CPU byte order
 */
#define adf_os_le32_to_cpu(x)                   __adf_os_le32_to_cpu(x)

/**
 * @brief Convert a 64-bit value from little-endian byte order to CPU byte order
 */
#define adf_os_le64_to_cpu(x)                   __adf_os_le64_to_cpu(x)

#endif
