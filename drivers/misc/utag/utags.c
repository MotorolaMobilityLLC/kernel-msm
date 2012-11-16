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

#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/ioctl.h>
#include <linux/input.h>
#include <linux/inet.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/string.h>
#include <linux/stat.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/unistd.h>
#include "utags.h"

const char *utags_blkdev = UTAGS_PARTITION;
struct proc_dir_entry *dir_root;
static char payload[MAX_UTAG_SIZE];
static uint32_t csum;

static struct utag_node utag_tab[] = {
	{UTAG_DT_NAME, "dtname", NULL, NULL, NULL, NULL},
	{UTAG_SERIALNO, "barcode", NULL, NULL, NULL, NULL},
	{UTAG_FTI, "fti", NULL, NULL, NULL, NULL},
	{UTAG_MSN, "msn", NULL, NULL, NULL, NULL},
	{UTAG_MODELNO, "model", NULL, NULL, NULL, NULL},
	{UTAG_CARRIER, "carrier", NULL, NULL, NULL, NULL},
	{UTAG_KERNEL_LOG_SWITCH, "log_switch", NULL, NULL, NULL, NULL},
	{UTAG_BASEBAND, "baseband", NULL, NULL, NULL, NULL},
	{UTAG_DISPLAY, "display", NULL, NULL, NULL, NULL},
	{UTAG_BOOTMODE, "bootmode", NULL, NULL, NULL, NULL},
	{UTAG_END, "all", NULL, NULL, NULL, NULL},
	{0, "", NULL, NULL, NULL, NULL},
};

ssize_t find_first_utag(const struct utag *head, uint32_t type, void **payload,
			enum utag_error *pstatus)
{
	ssize_t size = -1;
	enum utag_error status = UTAG_ERR_NOT_FOUND;
	const struct utag *cur = head;

	if (!payload || !cur) {
		status = UTAG_ERR_INVALID_ARGS;
		goto out;
	}

	while (cur) {
		if (cur->type == type) {
			*payload = (void *)cur->payload;
			size = (ssize_t) cur->size;
			status = UTAG_NO_ERROR;
			goto out;
		}

		cur = cur->next;
	}

 out:
	if (pstatus)
		*pstatus = status;

	return size;
}

enum utag_error free_tags(struct utag *tags)
{
	struct utag *next;

	while (tags) {
		next = tags->next;

		kfree(tags->payload);

		kfree(tags);

		tags = next;
	}

	return UTAG_NO_ERROR;
}

void *freeze_tags(size_t block_size, const struct utag *tags,
		  size_t *tags_size, enum utag_error *status)
{
	size_t frozen_size = 0;
	char *buf = NULL, *ptr;
	const struct utag *cur = tags;
	enum utag_error err = UTAG_NO_ERROR;

	/* Make sure the tags start with the HEAD marker. */
	if (tags && (UTAG_HEAD != tags->type)) {
		err = UTAG_ERR_INVALID_HEAD;
		goto out;
	}

	/*
	 * Walk the list once to determine the amount of space to allocate
	 * for the frozen tags.
	 */
	while (cur) {
		frozen_size += FROM_TAG_SIZE(TO_TAG_SIZE(cur->size))
		    + UTAG_MIN_TAG_SIZE;

		if (UTAG_END == cur->type)
			break;

		cur = cur->next;
	}

	/* do some more sanity checking */
	if (((cur) && (cur->next)) || (!cur)) {
		err = UTAG_ERR_INVALID_TAIL;
		goto out;
	}

	if (block_size < frozen_size) {
		err = UTAG_ERR_TAGS_TOO_BIG;
		goto out;
	}

	ptr = buf = kmalloc(frozen_size, GFP_KERNEL);
	if (!buf) {
		err = UTAG_ERR_OUT_OF_MEMORY;
		goto out;
	}

	cur = tags;
	while (1) {
		size_t aligned_size, zeros;
		struct frozen_utag frozen;

		aligned_size = FROM_TAG_SIZE(TO_TAG_SIZE(cur->size));

		frozen.type = htonl(cur->type);
		frozen.size = htonl(TO_TAG_SIZE(cur->size));

		memcpy(ptr, &frozen, UTAG_MIN_TAG_SIZE);
		ptr += UTAG_MIN_TAG_SIZE;

		if (0 != cur->size) {
			memcpy(ptr, cur->payload, cur->size);

			ptr += cur->size;
		}

		/* pad with zeros if needed */
		zeros = aligned_size - cur->size;
		if (zeros) {
			memset(ptr, 0, zeros);
			ptr += zeros;
		}

		if (UTAG_END == cur->type)
			break;

		cur = cur->next;
	}

	if (tags_size)
		*tags_size = frozen_size;

 out:
	if (status)
		*status = err;

	return buf;
}

struct utag *generate_empty_tags(void)
{
	struct utag *head, *cur;

	cur = head = kmalloc(sizeof(struct utag), GFP_KERNEL);

	if (!cur)
		return NULL;

	cur->type = UTAG_HEAD;
	cur->size = 0;
	cur->payload = NULL;
	cur->next = kmalloc(sizeof(struct utag), GFP_KERNEL);

	cur = cur->next;

	if (!cur) {
		kfree(head);
		return NULL;
	}

	cur->type = UTAG_END;
	cur->size = 0;
	cur->payload = NULL;
	cur->next = NULL;

	return head;
}

struct utag *load_utags(enum utag_error *pstatus)
{
	int res;
	size_t block_size;
	ssize_t bytes;
	struct utag *head = NULL;
	void *data;
	enum utag_error status = UTAG_NO_ERROR;
	int file_errno;
	struct file *filp = NULL;
	struct inode *inode = NULL;

	filp = filp_open(utags_blkdev, O_RDONLY, 0600);
	if (IS_ERR_OR_NULL(filp)) {
		file_errno = -PTR_ERR(filp);
		pr_err("%s ERROR opening file (%s) errno=%d\n", __func__,
		       utags_blkdev, file_errno);
		status = UTAG_ERR_PARTITION_OPEN_FAILED;
		goto out;
	}

	if (filp->f_path.dentry)
		inode = filp->f_path.dentry->d_inode;
	if (!inode || !S_ISBLK(inode->i_mode)) {
		status = UTAG_ERR_PARTITION_NOT_BLKDEV;
		goto close_block;
	}

	res =
	    filp->f_op->unlocked_ioctl(filp, BLKGETSIZE64,
				       (unsigned long)&block_size);
	if (0 > res) {
		status = UTAG_ERR_PARTITION_NOT_BLKDEV;
		goto close_block;
	}

	data = kmalloc(block_size, GFP_KERNEL);
	if (!data) {
		status = UTAG_ERR_OUT_OF_MEMORY;
		goto close_block;
	}

	bytes = kernel_read(filp, filp->f_pos, data, block_size);
	if ((ssize_t) block_size > bytes) {
		status = UTAG_ERR_PARTITION_READ_ERR;
		goto free_data;
	}

	head = thaw_tags(block_size, data, &status);

 free_data:
	kfree(data);

 close_block:
	filp_close(filp, NULL);

 out:
	if (pstatus)
		*pstatus = status;

	return head;
}

enum utag_error replace_first_utag(struct utag *head, uint32_t type,
				   const void *payload, size_t size)
{
	struct utag *cur = head;

	/* search for the first occurrence of specified type of tag */
	while (cur) {
		if (cur->type == type) {
			void *oldpayload = cur->payload;

			cur->payload = kmalloc(size, GFP_KERNEL);
			if (!cur->payload) {
				cur->payload = oldpayload;
				return UTAG_ERR_OUT_OF_MEMORY;
			}

			memcpy(cur->payload, payload, size);

			cur->size = size;

			kfree(oldpayload);

			return UTAG_NO_ERROR;
		}

		cur = cur->next;
	}

	/*
	 * If specified type wasn't found, insert a new tag immediately after
	 * the head.
	 */
	cur = kmalloc(sizeof(struct utag), GFP_KERNEL);
	if (!cur)
		return UTAG_ERR_OUT_OF_MEMORY;

	cur->type = type;
	cur->size = size;
	cur->payload = kmalloc(size, GFP_KERNEL);
	if (!cur->payload) {
		kfree(cur);
		return UTAG_ERR_OUT_OF_MEMORY;
	}

	memcpy(cur->payload, payload, size);

	cur->next = head->next;
	head->next = cur;

	return UTAG_NO_ERROR;
}

enum utag_error store_utags(const struct utag *tags)
{
	int res;
	size_t written = 0;
	size_t block_size = 0, tags_size = 0;
	char *datap = NULL;
	enum utag_error status = UTAG_NO_ERROR;
	int file_errno;
	struct file *filep = NULL;
	struct inode *inode = NULL;
	mm_segment_t fs;

	fs = get_fs();
	set_fs(KERNEL_DS);

	filep = filp_open(utags_blkdev, O_RDWR, 0);
	if (IS_ERR_OR_NULL(filep)) {
		file_errno = -PTR_ERR(filep);
		pr_err("%s ERROR opening file (%s) errno=%d\n", __func__,
		       utags_blkdev, file_errno);
		status = UTAG_ERR_PARTITION_OPEN_FAILED;
		goto out;
	}

	if (filep->f_path.dentry)
		inode = filep->f_path.dentry->d_inode;
	if (!inode || !S_ISBLK(inode->i_mode)) {
		status = UTAG_ERR_PARTITION_NOT_BLKDEV;
		goto close_block;
	}

	res =
	    filep->f_op->unlocked_ioctl(filep, BLKGETSIZE64,
					(unsigned long)&block_size);
	if (0 > res) {
		status = UTAG_ERR_PARTITION_NOT_BLKDEV;
		goto close_block;
	}

	datap = freeze_tags(block_size, tags, &tags_size, &status);
	if (!datap)
		goto close_block;

	written = filep->f_op->write(filep, datap, tags_size, &filep->f_pos);
	if (written < tags_size) {
		pr_err("%s ERROR writing file (%s) ret %d\n", __func__,
		       utags_blkdev, written);
		status = UTAG_ERR_PARTITION_WRITE_ERR;
	}
	kfree(datap);

 close_block:
	filp_close(filep, NULL);

 out:
	set_fs(fs);
	return status;
}

struct utag *thaw_tags(size_t block_size, void *buf, enum utag_error *status)
{
	struct utag *head = NULL, *cur = NULL;
	uint8_t *ptr = buf;
	enum utag_error err = UTAG_NO_ERROR;

	/*
	 * make sure the block is at least big enough to hold header
	 * and footer
	 */

	if (UTAG_MIN_TAG_SIZE * 2 > block_size) {
		err = UTAG_ERR_NO_ROOM;
		goto out;
	}

	while (1) {
		struct frozen_utag *frozen;
		uint8_t *next_ptr;

		frozen = (struct frozen_utag *)ptr;

		if (!head) {
			cur = kcalloc(1, sizeof(struct utag), GFP_KERNEL);
			if (!cur) {
				err = UTAG_ERR_OUT_OF_MEMORY;
				goto out;
			}
		}

		cur->type = ntohl(frozen->type);
		cur->size = FROM_TAG_SIZE(ntohl(frozen->size));

		if (!head) {
			head = cur;

			if ((UTAG_HEAD != head->type) && (0 != head->size)) {
				err = UTAG_ERR_INVALID_HEAD;
				goto err_free;
			}
		}

		/* check if this is the end */
		if (UTAG_END == cur->type) {
			/* footer payload size should be zero */
			if (0 != cur->size) {
				err = UTAG_ERR_INVALID_TAIL;
				goto err_free;
			}

			/* finish up */
			cur->payload = NULL;
			cur->next = NULL;
			break;
		}

		next_ptr = ptr + UTAG_MIN_TAG_SIZE + cur->size;

		/*
		 * Ensure there is enough space in the buffer for both the
		 * payload and the tag header for the next tag.
		 */
		if ((next_ptr - (uint8_t *) buf) + UTAG_MIN_TAG_SIZE >
		    block_size) {
			err = UTAG_ERR_TAGS_TOO_BIG;
			goto err_free;
		}

		/*
		 * Copy over the payload.
		 */
		if (cur->size != 0) {
			cur->payload = kcalloc(1, cur->size, GFP_KERNEL);
			if (!cur->payload) {
				err = UTAG_ERR_OUT_OF_MEMORY;
				goto err_free;
			}
			memcpy(cur->payload, frozen->payload, cur->size);
		} else {
			cur->payload = NULL;
		}

		/* advance to beginning of next tag */
		ptr = next_ptr;

		/* get ready for the next tag */
		cur->next = kcalloc(1, sizeof(struct utag), GFP_KERNEL);
		cur = cur->next;
		if (!cur) {
			err = UTAG_ERR_OUT_OF_MEMORY;
			goto err_free;
		}
	}

	goto out;

 err_free:
	free_tags(head);
	head = NULL;

 out:
	if (status)
		*status = err;

	return head;
}

static char *find_name(uint32_t type)
{
	int i = 0;

	while (UTAG_END != utag_tab[i].type) {
		if (type == utag_tab[i].type)
			return utag_tab[i].name;
		i++;
	}
	return "";

}

static int proc_dump_tags(struct seq_file *file)
{
	int i;
	char *data = NULL;
	struct utag *tags = NULL;
	enum utag_error status;
	uint32_t loc_csum = 0;

	tags = load_utags(&status);
	if (NULL == tags)
		pr_err("%s Load config failed\n", __func__);
	else
		while (tags != NULL) {
			seq_printf(file,
				   "Tag: Type: [0x%08X] Name [%s] Size: [0x%08X]\nData:\n\t",
				   tags->type, find_name(tags->type),
				   tags->size);
			for (i = 0, data = tags->payload; i < tags->size; i++) {
				loc_csum += data[i];
				seq_printf(file, "[0x%02X] ", data[i]);
				if ((i + 1) % 10 == 0)
					seq_printf(file, "\n\t");
			}
			seq_printf(file, "\n\n");
			tags = tags->next;
		}

	csum = loc_csum;
	free_tags(tags);
	return 0;
}

static int read_tag(struct seq_file *file, void *v)
{
	int i;
	struct utag *tags = NULL;
	enum utag_error status;
	uint8_t *buf;
	ssize_t size;
	struct proc_node *proc = (struct proc_node *)file->private;
	struct utag_node *mytag = proc->tag;
	uint32_t type = mytag->type;

	if (UTAG_END == type) {
		proc_dump_tags(file);
		return 0;
	}

	tags = load_utags(&status);
	if (NULL == tags) {
		pr_err("%s Load config failed\n", __func__);
		return -EFAULT;
	} else {
		size = find_first_utag(tags, type, (void **)&buf, &status);
		if (0 > size) {
			if (UTAG_ERR_NOT_FOUND == status)
				pr_err("Tag 0x%08X not found.\n", type);
			else
				pr_err("Unexpected error looking for tag: %d\n",
				       status);
			free_tags(tags);
			return 11;
		}
		switch (proc->mode) {
		case OUT_ASCII:
			seq_printf(file, "%s\n", buf);
			break;
		case OUT_RAW:
			for (i = 0; i < size; i++)
				seq_printf(file, "%02X", buf[i]);
			seq_printf(file, "\n");
			break;
		case OUT_ID:
			seq_printf(file, "%#X\n", type);
			break;
		}
	}

	free_tags(tags);
	return 0;
}

static ssize_t write_tag(struct file *file, const char __user *buffer,
			 size_t count, loff_t *pos)
{
	struct utag *tags = NULL;
	enum utag_error status;
	struct inode *inode = file->f_dentry->d_inode;
	struct proc_node *proc = proc_get_parent_data(inode);
	struct utag_node *mytag = proc->tag;
	uint32_t type = mytag->type;

	if (UTAG_END == type)
		return count;

	if (MAX_UTAG_SIZE < count) {
		pr_err("%s error utag too big %d\n", __func__, count);
		return count;
	}

	if (copy_from_user(payload, buffer, count)) {
		pr_err("%s user copy error\n", __func__);
		return count;
	}

	tags = load_utags(&status);
	if (NULL == tags) {
		pr_err("%s load config error\n", __func__);
		return count;
	} else {
		status = replace_first_utag(tags, type, payload, (count - 1));
		if (UTAG_NO_ERROR != status)
			pr_err("%s error on update tag %#x status %d\n",
			       __func__, type, status);
		else {
			status = store_utags(tags);
			if (UTAG_NO_ERROR != status)
				pr_err("%s error on store tags %#x status %d\n",
				       __func__, type, status);
		}
	}

	free_tags(tags);
	return count;
}

static int config_read(struct inode *inode, struct file *file)
{
	return single_open(file, read_tag, proc_get_parent_data(inode));
}

static const struct file_operations config_fops = {
	.owner = THIS_MODULE,
	.open = config_read,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = write_tag,
};

static int __init config_init(void)
{
	int i = 0;
	struct utag_node *utag = &utag_tab[0];
	struct proc_node *proc_data = NULL;
	dir_root = proc_mkdir("config", NULL);
	if (!dir_root) {
		pr_info("%s Failed to create dir entry\n", __func__);
		return -EFAULT;
	}
	while (UTAG_END != utag->type) {
		utag->root = proc_mkdir(utag->name, dir_root);
		proc_data =
			kmalloc(sizeof(struct proc_node), GFP_KERNEL);
		if (proc_data) {
			proc_data->mode = OUT_ASCII;
			proc_data->tag = utag;
		}
		utag->ascii_file = proc_create_data("ascii", 0, utag->root, &config_fops, proc_data);

		proc_data =
			kmalloc(sizeof(struct proc_node), GFP_KERNEL);
		if (proc_data) {
			proc_data->mode = OUT_RAW;
			proc_data->tag = utag;
		}
		utag->raw_file = proc_create_data("raw", 0, utag->root, &config_fops, proc_data);

		proc_data =
			kmalloc(sizeof(struct proc_node), GFP_KERNEL);
		if (proc_data) {
			proc_data->mode = OUT_ID;
			proc_data->tag = utag;
		}
		utag->id_file = proc_create_data("id", 0, utag->root, &config_fops, proc_data);

		i++;
		utag = &utag_tab[i];
	}

	proc_data =
		kmalloc(sizeof(struct proc_node), GFP_KERNEL);
	if (proc_data) {
		proc_data->mode = OUT_RAW;
		proc_data->tag = utag;
	}
	utag->raw_file = proc_create_data(utag->name, 0, dir_root, &config_fops, proc_data);

	return 0;
}

static void __exit config_exit(void)
{
	int i = 0;
	struct utag_node *utag = &utag_tab[0];
	while (UTAG_END != utag->type) {
		if (NULL != utag->ascii_file)
			remove_proc_entry("ascii", utag->root);
		if (NULL != utag->raw_file)
			remove_proc_entry("raw", utag->root);
		if (NULL != utag->id_file)
			remove_proc_entry("id", utag->root);
		remove_proc_entry(utag->name, dir_root);
		i++;
		utag = &utag_tab[i];
	}
	remove_proc_entry(utag->name, dir_root);
	remove_proc_entry("config", NULL);
}

module_init(config_init);
module_exit(config_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Motorola Mobility LLC");
MODULE_DESCRIPTION("Configuration module");
