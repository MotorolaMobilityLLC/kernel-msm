/* Copyright (c) 2014, Motorola Mobility LLC. All rights reserved.
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
#define pr_fmt(fmt) "mot_patch: " fmt

#include <linux/kernel.h>
#include <linux/of_fdt.h>
#include <linux/printk.h>
#include <linux/string.h>

#include <asm/cacheflush.h>
#include <asm/page.h>
#include <asm/sections.h>

#define MOT_PATCH_BUFFER_CANARY  0xABAD1DEA

#define MOT_PATCH_MEMORY_TYPE    0x80000001
#define MOT_PATCH_CODE_TYPE      0x80000002

#define MAX_SUPPORTED_BUFFER     1024

#define MOT_PATCH_ALIGN_SIZE     2048

#define MOT_PATCH_MAX_SIZE       0x4000  /* 16kb */

struct mp_header {
	unsigned int id;
	unsigned int size;
	unsigned int version;
	unsigned int banner_size;
};

struct mp_footer {
	unsigned int id;
};

struct mp_patch {
	unsigned int type;
	unsigned long address;
	unsigned int data_length;
};

/* Dummy reserved executable region to be used as scratch space to
 * hold code fixups needed that cannot be patched within the function
 */
__aligned(16)
void mot_patch_table(void)
{
	/* Just reserve with NOPs */
	asm(
		".rept 1023\n\t"
		"mov r0, r0\n\t"
		".endr\n\t"
	);
}

static inline unsigned char *get_data_start(unsigned char *base)
{
	struct mp_header *hdr = (struct mp_header *)base;
	unsigned int size = le32_to_cpu(hdr->banner_size);

	return base + sizeof(struct mp_header) + ALIGN(size, 4);
}

static inline unsigned char *get_data_end(unsigned char *base)
{
	struct mp_header *hdr = (struct mp_header *)base;
	unsigned int data_size;

	data_size = le32_to_cpu(hdr->size) - sizeof(struct mp_footer);

	return base + data_size;
}

static inline struct mp_footer *get_footer(unsigned char *base)
{
	return (struct mp_footer *)get_data_end(base);
}

static inline bool valid_canary(unsigned int id)
{
	return le32_to_cpu(id) == MOT_PATCH_BUFFER_CANARY;
}

static inline bool valid_banner(struct mp_header *hdr)
{
	int max_size = strlen(linux_banner);
	char *ptr = (char *)hdr + sizeof(struct mp_header);

	if (le32_to_cpu(hdr->banner_size) > max_size) {
		pr_err("banner size too large\n");
		return false;
	}

	if (strncmp(ptr, linux_banner, max_size)) {
		pr_err("banner mismatch\n");
		return false;
	}

	return true;
}

static int validate_buffer(unsigned char *base)
{
	struct mp_header *hdr = (struct mp_header *)base;
	unsigned char *ptr = base;
	struct mp_footer *ftr;
	int count = 0;

	if (!valid_canary(hdr->id)) {
		pr_err("Invalid starting marker, skipping\n");
		return 0;
	}

	if (le32_to_cpu(hdr->size) > MOT_PATCH_MAX_SIZE) {
		pr_err("Invalid size argument: 0x%X\n",
			le32_to_cpu(hdr->size));
		return 0;
	}

	if (!valid_banner(hdr))
		return 0;

	ptr = get_data_start(base);

	/* While there is still expected data */
	while (ptr < get_data_end(base)) {
		struct mp_patch *patch = (struct mp_patch *)ptr;
		unsigned int size = le32_to_cpu(patch->data_length);

		/* Validate the type */
		switch (le32_to_cpu(patch->type)) {
		case MOT_PATCH_MEMORY_TYPE:
		case MOT_PATCH_CODE_TYPE:
			break;
		default:
			pr_err("Invalid patch type: %X\n",
					le32_to_cpu(patch->type));
			return 0;
		}

		if (size > MAX_SUPPORTED_BUFFER) {
			pr_err("data_length exceeds support\n");
			return 0;
		}

		count++;

		/* Increment the pointer a patch structure and data size */
		ptr += size + sizeof(struct mp_patch);
	}

	ftr = get_footer(base);
	if (!valid_canary(ftr->id)) {
		pr_err("Missing end marker: %X\n", le32_to_cpu(ftr->id));
		return 0;
	}

	return count;
}

static unsigned char *locate_patch_buffer(unsigned int atags)
{
	struct boot_param_header *dt;
	unsigned int size;

	dt = phys_to_virt(atags);

	/* Check for a devtree */
	if (be32_to_cpu(dt->magic) != OF_DT_HEADER)
		return NULL;

	size = be32_to_cpu(dt->totalsize);
	if (!size)
		return NULL;

	size = ALIGN(size, MOT_PATCH_ALIGN_SIZE);

	return (unsigned char *)dt + size;
}

void mot_patch_kernel(unsigned int atags)
{
	unsigned char *ptr, *base;
	int count;

	/* Try to find the patch buffer attached to end of devtree */
	base = locate_patch_buffer(atags);
	if (!base)
		return;

	/* Do a sanity of all the contents before applying a single
	 * patch so we are not partially patching from the buffer
	 */
	count = validate_buffer(base);
	if (!count)
		return;

	pr_info("Applying %d patches\n", count);

	ptr = get_data_start(base);

	/* While there is still expected data */
	while (ptr < get_data_end(base)) {
		struct mp_patch *patch = (struct mp_patch *)ptr;
		unsigned int psize;

		psize = sizeof(*patch) + le32_to_cpu(patch->data_length);

		memcpy((void *)le32_to_cpu(patch->address),
			(void *)patch + sizeof(*patch),
			le32_to_cpu(patch->data_length));

		ptr += psize;
	}

	flush_cache_all();

	return;
}
