/*
 * Copyright (c) 2012-2015 The Linux Foundation. All rights reserved.
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

/*
 * IMPORTANT NOTE: For all change to WMI Interface, the ABI version number _must_ be updated.
 */
/** Major version number is incremented when there are significant changes to WMI Interface that break compatibility. */
#define __WMI_VER_MAJOR_    1
/** Minor version number is incremented when there are changes
 *  (however minor) to WMI Interface that break
 *  compatibility. */
#define __WMI_VER_MINOR_    0
/** WMI revision number has to be incremented when there is a
 *  change that may or may not break compatibility. */
#define __WMI_REVISION_ 192

/** The Version Namespace should not be normally changed. Only
 *  host and firmware of the same WMI namespace will work
 *  together.
 *  For example, "QCA_ML" converts to 0x4C, 0x4D5F414351.
 *  where 'Q'=0x51, 'C'=0x43, 'A'=0x41, '_'=0x5F. 'M'=4D, 'L'=4C
 */
#define __NAMESPACE_0_    0x5F414351
#define __NAMESPACE_1_    0x00004C4D
#define __NAMESPACE_2_    0x00000000
#define __NAMESPACE_3_    0x00000000

/* Format of the version number. */
#define WMI_VER_MAJOR_BIT_OFFSET        24
#define WMI_VER_MINOR_BIT_OFFSET        0

#define WMI_VER_MAJOR_BIT_MASK          0xFF000000
#define WMI_VER_MINOR_BIT_MASK          0x00FFFFFF

/* Macros to extract the sw_version components.
 */
#define WMI_VER_GET_MAJOR(x) (((x) & WMI_VER_MAJOR_BIT_MASK)>>WMI_VER_MAJOR_BIT_OFFSET)
#define WMI_VER_GET_MINOR(x) (((x) & WMI_VER_MINOR_BIT_MASK)>>WMI_VER_MINOR_BIT_OFFSET)

#define WMI_VER_GET_VERSION_0(major, minor) ( (( major << WMI_VER_MAJOR_BIT_OFFSET ) & WMI_VER_MAJOR_BIT_MASK) + (( minor << WMI_VER_MINOR_BIT_OFFSET ) & WMI_VER_MINOR_BIT_MASK) )
/*
 * The version has the following format:
 * Bits 24-31: Major version
 * Bits 0-23: Minor version
 * Bits 0-31:  Build number
 * E.g. Build 1.1.7 would be represented as 0x01000001 for Major/Minor & 0x00000007 for buildnum.
 *
 * DO NOT split the following macro into multiple lines as this may confuse the build scripts.
 */
/* ABI Version. Reflects the version of binary interface exposed by Target firmware. */
#define WMI_ABI_VERSION_0  WMI_VER_GET_VERSION_0(__WMI_VER_MAJOR_, __WMI_VER_MINOR_)
#define WMI_ABI_VERSION_1  __WMI_REVISION_
#define WMI_ABI_VERSION_NS_0 __NAMESPACE_0_
#define WMI_ABI_VERSION_NS_1 __NAMESPACE_1_
#define WMI_ABI_VERSION_NS_2 __NAMESPACE_2_
#define WMI_ABI_VERSION_NS_3 __NAMESPACE_3_
