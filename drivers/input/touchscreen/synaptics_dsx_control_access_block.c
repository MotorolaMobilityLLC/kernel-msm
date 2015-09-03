/*
 * Synaptics DSX touchscreen driver extension
 *
 * Copyright (C) 2015 Motorola Mobility, Inc.
 *
 * Copyright (C) 2015 Konstantin Makariev <kmakariev@motorola.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#define pr_fmt(fmt) "synaptics-CAB: %s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include "synaptics_dsx_i2c.h"
#include "synaptics_dsx_control_access_block.h"

#define cab_debug pr_debug

static struct touch_control_access_block *control_access_block;

struct touch_control_access_block *control_access_block_get(void) {
	return control_access_block;
}
EXPORT_SYMBOL_GPL(control_access_block_get);

#define GROUP_NAME(grp) (grp == SYN_DSX_STATUS ? "STATUS" : \
			(grp == SYN_DSX_DATA ? "DATA" : \
			(grp == SYN_DSX_MODE ? "MODE" : \
			(grp == SYN_DSX_WAKE ? "WAKE" : \
			(grp == SYN_DSX_SLEEP ? "SLEEP" : \
			(grp == SYN_DSX_RESET ? "RESET" : "CONFIG"))))))

#define DIR_NAME(grp) (grp & MODE_WO ? "WO" : "RO")

#define SPRINTF_PAGE(fmt, args...) {\
		blen += scnprintf(buf + blen, PAGE_SIZE - blen, fmt, ##args);\
	}

static ssize_t control_access_block_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int i, j, k, m;
	ssize_t blen = 0;
	struct touch_control_access_block *cab = control_access_block_get();

	if (cab->do_sync)
		SPRINTF_PAGE("has been changed, ");
	SPRINTF_PAGE("has %d groups\n", cab->grp_num);

	for (i = 0; i < cab->grp_num; i++) {
		struct grp_info *ginfo = cab->grp_ptr[i];
		/* skip empty slots */
		if (!ginfo)
			continue;
		SPRINTF_PAGE("group [%s] access %s has %d data blocks\n",
			GROUP_NAME(ginfo->group),
			DIR_NAME(ginfo->group),
			ginfo->data_blk_cnt);
			for (m = 0; m < ginfo->data_blk_cnt; m++) {
				SPRINTF_PAGE("\tdata block %d:\n", m);
				if (ginfo->group & MODE_WO) {
					struct op_write *wo = ginfo->wo_data[m];
					SPRINTF_PAGE("\t\tpage:%d, addr:0x%X, blk_sz:%d, blk_cnt:%d, mask:0x%x\n",
						wo->paddr >> 8, wo->paddr & 0xff,
						wo->blk_sz, wo->blk_cnt, wo->mask);
					SPRINTF_PAGE("\t\tdata: ");
					for (j = 0; j < wo->blk_cnt; j++) {
						unsigned char *data = &wo->data[j];
						SPRINTF_PAGE("[%d] ", j);
						for (k = 0; k < wo->blk_sz; k++)
							SPRINTF_PAGE("0x%X ", *(data + k));
						SPRINTF_PAGE("\n");
					}
				} else {
					struct op_read *ro = ginfo->ro_data[m];
					SPRINTF_PAGE("\t\tpage:%d, addr:0x%X, blk_sz:%d, blk_cnt:%d\n",
						ro->paddr >> 8, ro->paddr & 0xff,
						ro->blk_sz, ro->blk_cnt);
				}
			}
	}

	cab->do_sync = false;
	cab_debug("clear do_sync flag\n");

	return blen;
}

static struct op_write *alloc_op_write(unsigned short paddr,
	unsigned char blk_sz, unsigned char blk_cnt,
	unsigned char mask, unsigned char *data)
{
	int i;
	struct op_write *wo;
	wo = kzalloc(sizeof(struct op_write) + blk_sz, GFP_KERNEL);
	if (!wo)
		return NULL;
	wo->paddr = paddr;
	wo->blk_sz = blk_sz;
	wo->blk_cnt = blk_cnt;
	wo->mask = mask;
	for (i = 0; i < blk_sz; i++)
		wo->data[i] = *data++;
	cab_debug("added WO block: addr={%X}, sz=%u, cnt=%u\n", paddr, blk_sz, blk_cnt);
	return wo;
}

static struct op_read *alloc_op_read(unsigned short paddr,
	unsigned char blk_sz, unsigned char blk_cnt)
{
	struct op_read *ro;
	ro = kzalloc(sizeof(struct op_read), GFP_KERNEL);
	if (!ro)
		return NULL;
	ro->paddr = paddr;
	ro->blk_sz = blk_sz;
	ro->blk_cnt = blk_cnt;
	cab_debug("added RO block: addr={%X}, sz=%u, cnt=%u\n", paddr, blk_sz, blk_cnt);
	return ro;
}

/* create a new group and add one or more entities */
static struct grp_info *alloc_grp_info(unsigned char group,
	unsigned char data_blk_cnt, ...)
{
	int i;
	va_list ap;
	struct grp_info *gi;
	gi = kzalloc(sizeof(struct grp_info), GFP_KERNEL);
	if (!gi)
		return NULL;
	gi->group = group;
	gi->data_blk_cnt = data_blk_cnt;
	va_start(ap, data_blk_cnt);
	if (group & MODE_WO) {
		struct op_write *data;
		gi->wo_data = (struct op_write **)kzalloc(
					sizeof(struct op_write *)*data_blk_cnt,
					GFP_KERNEL);
		if (!gi->wo_data)
			goto alloc_failure;
		for (i = 0; i < data_blk_cnt; i++) {
			data = va_arg(ap, struct op_write *);
			gi->wo_data[i] = data;
		}
	} else {
		struct op_read *data;
		gi->ro_data = (struct op_read **)kzalloc(
					sizeof(struct op_read *)*data_blk_cnt,
					GFP_KERNEL);
		if (!gi->ro_data)
			goto alloc_failure;
		for (i = 0; i < data_blk_cnt; i++) {
			data = va_arg(ap, struct op_read *);
			gi->ro_data[i] = data;
		}
	}
	goto success;
alloc_failure:
	gi->data_blk_cnt = 0;
success:
	cab_debug("created group [%s] access %s has %d data block(s)\n",
		GROUP_NAME(gi->group), DIR_NAME(gi->group), gi->data_blk_cnt);
	va_end(ap);
	return gi;
}

/* add one or more entities to existing group */
static int modify_grp_info(struct grp_info *gi,
	unsigned char data_blk_cnt, ...)
{
	int i, retval = 0;
	int new_max = gi->data_blk_cnt + data_blk_cnt;
	va_list ap;

	va_start(ap, data_blk_cnt);
	if (gi->group & MODE_WO) {
		struct op_write **realloc, *append;
		realloc = (struct op_write **)kzalloc(sizeof(struct op_write *)*
					(gi->data_blk_cnt + data_blk_cnt),
					GFP_KERNEL);
		if (!realloc) {
			retval = -ENOMEM;
			goto alloc_failure;
		}
		for (i = 0; i < new_max; i++) {
			if (i < gi->data_blk_cnt) {
				realloc[i] = gi->wo_data[i];
			}else {
				append = va_arg(ap, struct op_write *);
				realloc[i] = append;
			}
		}
		kfree(gi->wo_data);
		gi->wo_data = realloc;
	} else {
		struct op_read **realloc, *append;
		realloc = (struct op_read **)kzalloc(sizeof(struct op_read *)*
					(gi->data_blk_cnt + data_blk_cnt),
					GFP_KERNEL);
		if (!realloc) {
			retval = -ENOMEM;
			goto alloc_failure;
		}
		for (i = 0; i < new_max; i++) {
			if (i < gi->data_blk_cnt) {
				realloc[i] = gi->ro_data[i];
			}else {
				append = va_arg(ap, struct op_read *);
				realloc[i] = append;
			}
		}
		kfree(gi->ro_data);
		gi->ro_data = realloc;
	}
	gi->data_blk_cnt += data_blk_cnt;
	cab_debug("modified group:[%s] access:%s has %d data blocks\n",
		GROUP_NAME(gi->group), DIR_NAME(gi->group), gi->data_blk_cnt);
alloc_failure:
	va_end(ap);
	return retval;
}

void control_access_block_zap(unsigned char gid)
{
	struct touch_control_access_block *cab = control_access_block_get();
	struct grp_info **grp_ptr = NULL;
	int i, idx = 0;

	for (i = 0; i < cab->grp_num; i++) {
		if (cab->grp_ptr[i] && cab->grp_ptr[i]->group == gid) {
			idx = i;
			grp_ptr = &cab->grp_ptr[idx];
			break;
		}
	}

	if (!grp_ptr)
		return;

	/* clean up data and make slot available */
	if ((*grp_ptr)->group & MODE_WO) {
		struct op_write *data;
		for (i = 0; i < (*grp_ptr)->data_blk_cnt; i++) {
			data = (*grp_ptr)->wo_data[i];
			kfree(data);
		}
		kfree((*grp_ptr)->wo_data);
	} else {
		struct op_read *data;
		for (i = 0; i < (*grp_ptr)->data_blk_cnt; i++) {
			data = (*grp_ptr)->ro_data[i];
			kfree(data);
		}
		kfree((*grp_ptr)->ro_data);
	}

	kfree(*grp_ptr);
	*grp_ptr = NULL;

	cab_debug("group [%s] idx %d removed %d record(s)\n", GROUP_NAME(gid), idx, i);
}

static struct grp_info **find_group(unsigned char gid)
{
	struct touch_control_access_block *cab = control_access_block_get();
	struct grp_info **grp_ptr = NULL;
	int i;

	for (i = 0; i < cab->grp_num; i++) {
		if (cab->grp_ptr[i] && cab->grp_ptr[i]->group &&
			cab->grp_ptr[i]->group != gid)
			continue;
		/* match or empty slot */
		grp_ptr = &cab->grp_ptr[i];
		break;
	}
	cab_debug("idx %d %s\n", i, *grp_ptr ? "existing" : "new");
	return grp_ptr;
}

static int add_tail(struct grp_info **grp, unsigned char gid, void *io_data)
{
	int retval = 0;

	if (!*grp) {
		struct grp_info *new;

		new = alloc_grp_info(gid, 1, (void *)io_data);
		if (!new) {
			retval = -ENOMEM;
			goto alloc_failure;
		}
		*grp = new;
		cab_debug("created new %s group\n", DIR_NAME((*grp)->group));
	} else {
		modify_grp_info(*grp, 1, (void *)io_data);
		cab_debug("added %s block\n", DIR_NAME((*grp)->group));
	}
alloc_failure:
	return retval;
}

int control_access_block_update_ro(
	unsigned char gid,
	unsigned short paddr,
	unsigned char sz,
	unsigned char cnt)
{
	struct op_read *ro = alloc_op_read(paddr, sz, cnt);
	struct grp_info **grp = find_group(gid);
	return !ro ? -1  : add_tail(grp, gid, (void *)ro);
}

int control_access_block_update_wo(
	unsigned char gid,
	unsigned short paddr,
	unsigned char sz,
	unsigned char mask,
	unsigned char *data)
{
	struct op_write *wo = alloc_op_write(paddr, sz, 1, mask, data);
	struct grp_info **grp = find_group(gid);
	return !wo ? -1 : add_tail(grp, gid, (void *)wo);
}

static struct device_attribute attrs[] = {
	__ATTR(control_access_block, S_IRUSR | S_IRGRP,
			control_access_block_show, NULL),
};

static int synaptics_control_access_block_init(
		struct synaptics_rmi4_data *rmi4_data)
{
	int retval, attr_count;

	control_access_block = kzalloc(sizeof(*control_access_block), GFP_KERNEL);
	if (!control_access_block) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to alloc controll access block\n",
				__func__);
		goto exit;
	}
	control_access_block->grp_num = GROUP_MAX;

	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++) {
		retval = sysfs_create_file(&rmi4_data->i2c_client->dev.kobj,
				&attrs[attr_count].attr);
		if (retval < 0) {
			dev_err(&rmi4_data->i2c_client->dev,
					"%s: Failed to create sysfs attributes\n",
					__func__);
			retval = -ENODEV;
			goto exit_remove_attrs;
		}
	}

	return 0;

exit_remove_attrs:
	for (attr_count--; attr_count >= 0; attr_count--) {
		sysfs_remove_file(&rmi4_data->i2c_client->dev.kobj,
				&attrs[attr_count].attr);
	}
exit:
	return 0;
}

static struct completion remove_complete;

static void synaptics_control_access_block_remove(
		struct synaptics_rmi4_data *rmi4_data)
{
	int i, m, attr_count;

	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++) {
		sysfs_remove_file(&rmi4_data->i2c_client->dev.kobj,
				&attrs[attr_count].attr);
	}

	for (i = 0; i < control_access_block->grp_num; i++) {
		struct grp_info *ginfo = control_access_block->grp_ptr[i];
		void **ptr_to_free;

		if (ginfo->group & MODE_WO)
			ptr_to_free = (void **)ginfo->wo_data;
		else
			ptr_to_free = (void **)ginfo->ro_data;

		for (m = 0; m < ginfo->data_blk_cnt; m++) {
			if (ginfo->group & MODE_WO)
				kfree(ginfo->wo_data[m]);
			else
				kfree(ginfo->ro_data[m]);
		}
		kfree(ptr_to_free);
		kfree(ginfo);
	}

	kfree(control_access_block->grp_ptr);
	kfree(control_access_block);
	control_access_block = NULL;

	complete(&remove_complete);

	return;
}

static int __init synaptics_control_access_block_module_init(void)
{
	synaptics_rmi4_new_function(RMI_CTRL_ACCESS_BLK, true,
			synaptics_control_access_block_init,
			synaptics_control_access_block_remove,
			NULL, IC_MODE_ANY);
	return 0;
}

static void __exit synaptics_control_access_block_module_exit(void)
{
	init_completion(&remove_complete);
	synaptics_rmi4_new_function(RMI_CTRL_ACCESS_BLK, false,
			synaptics_control_access_block_init,
			synaptics_control_access_block_remove,
			NULL, IC_MODE_ANY);
	wait_for_completion(&remove_complete);
	return;
}

module_init(synaptics_control_access_block_module_init);
module_exit(synaptics_control_access_block_module_exit);

MODULE_AUTHOR("Motorola Mobility, Inc.");
MODULE_DESCRIPTION("Synaptics DSX Touch Access Block Module");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(SYNAPTICS_DSX_DRIVER_VERSION);
