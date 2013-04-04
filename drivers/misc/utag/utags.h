/* Copyright (c) 2012, Motorola Mobility LLC. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>

#ifndef __UTAGS_H_HEADER__
#define __UTAGS_H_HEADER__

struct utag;

struct utag {
	uint32_t type;
	uint32_t size;
	struct utag *next;
	void *payload;
};

struct utag_node {
	uint32_t type;
	char name[32];
	struct proc_dir_entry *ascii_file;
	struct proc_dir_entry *raw_file;
	struct proc_dir_entry *id_file;
	struct proc_dir_entry *root;
};

enum utag_output {
	OUT_ASCII = 0,
	OUT_RAW,
	OUT_ID,
};

struct proc_node {
	struct utag_node *tag;
	uint32_t mode;
};

enum utag_error {
	UTAG_NO_ERROR = 0,
	UTAG_ERR_NO_ROOM,
	UTAG_ERR_OUT_OF_MEMORY,
	UTAG_ERR_INVALID_HEAD,
	UTAG_ERR_INVALID_TAIL,
	UTAG_ERR_TAGS_TOO_BIG,
	UTAG_ERR_PARTITION_OPEN_FAILED,
	UTAG_ERR_PARTITION_NOT_BLKDEV,
	UTAG_ERR_PARTITION_READ_ERR,
	UTAG_ERR_PARTITION_WRITE_ERR,
	UTAG_ERR_NOT_FOUND,
	UTAG_ERR_INVALID_ARGS,
};

struct frozen_utag {
	uint32_t type;
	uint32_t size;
	uint8_t payload[];
};

/* comes out to 8 since payload is empty array */
#define UTAG_MIN_TAG_SIZE   (sizeof(struct frozen_utag))
#define MAX_UTAG_SIZE 1024
#define UTAGS_PARTITION "/dev/block/platform/msm_sdcc.1/by-name/utags"

#define TO_TAG_SIZE(n)      (((n)+3)>>2)
#define FROM_TAG_SIZE(n)    ((n)<<2)

#define UTAG_HEAD     0xCAFE0001
#define UTAG_END      0xCAFE0002

/*
 * device tree name: variable length ASCII string with the name of the device
 * tree to be used by a particular device
 */
#define UTAG_DT_NAME  0xCAFE0003

/*
 * serial number: the serial number to be reported by the bootloader
 *
 * This was originally added to be set to the factory barcode of the device
 */
#define UTAG_SERIALNO 0xCAFE0004

/*
 * factory tracking information: 128 character ASCII string
 */
#define UTAG_FTI        0xCAFE0005

/*
 * mobile serial number: yet another unique identifier of a device
 */
#define UTAG_MSN        0xCAFE0006

/*
 * model number: manufacturer model number (i.e. MZ617)
 */
#define UTAG_MODELNO    0xCAFE0007

/*
 * carrier: this is originally meant only to indicate to the kernel that this
 *          device is wifi-only for tablets
 */
#define UTAG_CARRIER    0xCAFE0008

/*
 * kernel log console: store the kernel uart console. If it is not used,
 * the kernel uart console will be "/dev/null". Anything defined will be passed
 * into kernel as console in command line.
 */
#define UTAG_CONSOLE_NAME  0xCAFE0009

/*
 * baseband: indicates baseband type as an ASCII string ("cdma", "umts", etc)
 */
#define UTAG_BASEBAND   0xCAFE000A

/*
 * display: basic display type as an ASCII string, intended for hardware
 * that can support wildly-different panels (such as mortables).  This tag is
 * not indended for differentiating between panel sub-versions or 2nd source
 * types because the factory cannot be expected to program this correctly over
 * the lifespan of a product.
 */
#define UTAG_DISPLAY    0xCAFE000B

/*
 * bootmode: this value is passed to the kernel command line via
 * androidboot.mode.
 */
#define UTAG_BOOTMODE   0xCAFE000C

/*
 * hwrev: this string is used as an override for the board revision field in
 * the FTI.
 */
#define UTAG_HWREV          0xCAFE000D

/*
 * flags: general purpose bit flags
 */
#define UTAG_FLAGS          0xCAFE000E

/*
 * baseband version
 */
#define UTAG_VERSION_BASEBAND   0xCAFE000F

/*
 * system version
 */
#define UTAG_VERSION_SYSTEM     0xCAFE0010

/*
 * kernel version
 */
#define UTAG_VERSION_KERNEL     0xCAFE0011

/*
 * webtop version
 */
#define UTAG_VERSION_WEBTOP     0xCAFE0012

/*
 * locked status
 */
#define UTAG_UNLOCKED       0xCAFE0013

/*
 * data for charging/battery
 */
#define UTAG_BATTERY      0xCAFE0014

/*
 * QCOM DLOAD mode enable/disable
 */
#define UTAG_DLOAD_MODE  0xCAFE0015

/*
 * IMEI: 14 decimal digits plus a check digit
 */
#define UTAG_IMEI  0xCAFE0016

/*
 * CMDLINE OVERLAY: kernel command line parameters which
 * are appended to the command line
 * or substitute existing parameters
 */
#define UTAG_CMDLINE_OVERLAY  0xCAFE0017

/*
 * TOD offset: offset in seconds of current time of day
 * against PMIC RTC
 * (offset + rtc) == seconds since Epoch (Jan. 1 1970 UTC 00:00AM)
 */
#define UTAG_TOD_OFFSET  0xCAFE0018

/*
 * SKU: Stock Keeping Unit code
 */
#define UTAG_SKU  0xCAFE0019

/*
 * Date Code: mm-dd-yyyy
 */
#define UTAG_DATE  0xCAFE001A

/*
 * Device Name
 */
#define UTAG_DEVICE 0xCAFE001B

/*
 * Customization MD5 checksum, e.g. 64ce3caf02cd4c9881e43b613be2354a
 */
#define UTAG_CUST_MD5  0xCAFE001C

/*
 * The ICCID (Integrated Circuit Card Identifier)
 */
#define UTAG_ICCID 0xCAFE001D

/*
 * tags to store android properties
 */
#define UTAG_BUILD_DATE           0xCAFE1000
#define UTAG_BUILD_ID             0xCAFE1001
#define UTAG_BUILD_TAGS           0xCAFE1002
#define UTAG_BUILD_TYPE           0xCAFE1003
#define UTAG_BUILD_USER           0xCAFE1004
#define UTAG_BUILD_VER_CODENAME   0xCAFE1005
#define UTAG_BUILD_VER_INC        0xCAFE1006
#define UTAG_BUILD_VER_REL        0xCAFE1007
#define UTAG_MOT_BUILD_CID        0xCAFE1008
#define UTAG_MOT_BUILD_PRODUCT    0xCAFE1009
#define UTAG_BUILD_FINGERPRINT    0xCAFE100A
#define UTAG_KERNEL_VERSION       0xCAFE1020

extern struct utag *thaw_tags(size_t block_size, void *buf,
			      enum utag_error *status);
extern void *freeze_tags(size_t block_size, const struct utag *tags,
			 size_t *tags_size, enum utag_error *status);
extern enum utag_error free_tags(struct utag *tags);

extern struct utag *load_utags(enum utag_error *pstatus);
extern enum utag_error store_utags(const struct utag *tags);

extern enum utag_error replace_first_utag(struct utag *head, uint32_t type,
					  const void *payload, size_t size);
extern ssize_t find_first_utag(const struct utag *head, uint32_t type,
			       void **payload, enum utag_error *pstatus);

extern struct utag *generate_empty_tags(void);

#endif
