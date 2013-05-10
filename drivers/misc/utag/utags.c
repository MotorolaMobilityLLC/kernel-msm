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
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/string.h>
#include <linux/stat.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/unistd.h>
#include <linux/slab.h>

#define MAX_UTAG_SIZE 1024
#define MAX_UTAG_NAME 32
#define UTAG_HEAD  "__UTAG_HEAD__"
#define UTAG_TAIL  "__UTAG_TAIL__"
#define ROUNDUP(a, b) (((a) + ((b)-1)) & ~((b)-1))
#define TO_SECT_SIZE(n)     (((n) + 511) & ~511)

static const struct file_operations utag_fops;

enum utag_flag {
	UTAG_FLAG_PROTECTED = 1 << 0,
};

static uint32_t csum;
static char payload[MAX_UTAG_SIZE];
static struct proc_dir_entry *dir_root;

static char utags_blkdev[255];
static char utags_backup[255];

module_param_string(blkdev, utags_blkdev, sizeof(utags_blkdev), 0664);
MODULE_PARM_DESC(blkdev, "Full path for utags partition");
module_param_string(backup, utags_backup, sizeof(utags_backup), 0664);
MODULE_PARM_DESC(blkdev, "Full path for utags backup partition");

struct utag;

struct utag {
	char name[MAX_UTAG_NAME];
	uint32_t size;
	uint32_t flags;
	uint32_t crc32;		/* reserved for futher implementation */
	void *payload;
	struct utag *next;
};

struct frozen_utag {
	char name[MAX_UTAG_NAME];
	uint32_t size;
	uint32_t flags;
	uint32_t crc32;
	uint8_t payload[];
};

#define UTAG_MIN_TAG_SIZE   (sizeof(struct frozen_utag))

enum utag_output {
	OUT_ASCII = 0,
	OUT_RAW,
	OUT_TYPE,
	OUT_NEW
};

char *files[] = {
	"ascii",
	"raw",
	"type",
	"new"
};

struct proc_node {
	struct list_head entry;
	char full_name[MAX_UTAG_NAME];
	char name[MAX_UTAG_NAME];
	char type[MAX_UTAG_NAME];
	char file_name[MAX_UTAG_NAME];
	struct proc_dir_entry *file;
	struct proc_dir_entry *dir;
	uint32_t mode;
};

LIST_HEAD(node_list);

struct dir_node {
	struct list_head entry;
	struct proc_dir_entry *root;
	char name[MAX_UTAG_NAME];
};

LIST_HEAD(dir_list);

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
	UTAG_ERR_PROTECTED,
};

/*
 * Free the memory associated with a list of tags.
 *
 */

static inline void free_tags(struct utag *tags)
{
	struct utag *next;

	while (tags) {
		next = tags->next;
		kfree(tags->payload);
		kfree(tags);
		tags = next;
	}
}

/*
 * compare only the name part of a <name[:type]> formatted full
 * utag name
 *
 * returns true if match, false otherwise
 */

static inline bool names_match(const char *s1, const char *s2)
{
	register size_t count = MAX_UTAG_NAME;
	register int r, c1, c2;

	while (count--) {
		c1 = *s1++;
		c2 = *s2++;
		if (c1 == ':')
			c1 = 0;
		if (c2 == ':')
			c2 = 0;
		r = c1 - c2;
		if (r || !c1)
			return (r) ? false : true;
	}
	return true;
}

/*
 *
 * Check for name to have single ':' char
 * not in the first or last position
 *
 * returns true if name is OK, false otherwise
 */

static inline bool validate_name(const char *s1, int count)
{
	register int c1 = *s1, sep = 0;

	if (c1 == ':')
		return false;
	while (count--) {
		if (c1 == ':')
			sep++;
		if (sep > 1)
			return false;
		c1 = *s1++;
	}
	if (c1 == ':')
		return false;
	return true;
}

/*
 * split long name <name[:type]> of a utag in two
 * name and type
 *
 * if there is no : separator type will be empty string
 */

static inline void split_name(char *full, char *name, char *type)
{
	size_t pos, type_length;

	memset(name, 0, MAX_UTAG_NAME);
	memset(type, 0, MAX_UTAG_NAME);

	for (pos = 0; pos < MAX_UTAG_NAME; pos++) {
		if ((full[pos] == ':') || (full[pos] == 0))
			break;
	}
	memcpy(name, full, pos);
	type_length = strnlen(full, MAX_UTAG_NAME) - 1 - pos;
	if (0 < type_length)
		memcpy(type, (full + pos + 1), type_length);
}

/*
 * Find first instance of utag by specified name
 */

static struct utag *find_first_utag(const struct utag *head, const char *name)
{
	struct utag *cur;

	cur = head->next;	/* skip HEAD */
	while (cur) {
		/* skip TAIL */
		if (cur->next == NULL)
			break;
		if (names_match(name, cur->name))
			return cur;
		cur = cur->next;
	}
	return NULL;
}

/*
 * Create, initialize add to the list procfs utag file node
 */

static int
utag_file(char *utag_name, char *utag_type,
	  enum utag_output mode, struct proc_dir_entry *dir,
	  const struct file_operations *fops)
{
	struct proc_node *node;

	if (sizeof(files) < mode)
		return -EINVAL;

	node = kmalloc(sizeof(struct proc_node), GFP_KERNEL);
	if (node) {
		list_add(&node->entry, &node_list);
		strlcpy(node->file_name, files[mode], MAX_UTAG_NAME);
		strlcpy(node->name, utag_name, MAX_UTAG_NAME);
		strlcpy(node->type, utag_type, MAX_UTAG_NAME);
		node->mode = mode;
		node->dir = dir;
		node->file = proc_create_data(node->file_name, 0, dir, fops, node);
	}

	return 0;
}

/*
 * Convert a block of tags, presumably loaded from seconday storage, into a
 * format that can be manipulated.
 */
static struct utag *thaw_tags(size_t block_size, void *buf,
			      enum utag_error *status)
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
			/* calloc zeros allocated memory */
			cur = kcalloc(1, sizeof(struct utag), GFP_KERNEL);
			if (!cur) {
				err = UTAG_ERR_OUT_OF_MEMORY;
				goto out;
			}
		}

		strlcpy(cur->name, frozen->name, MAX_UTAG_NAME - 1);
		cur->flags = ntohl(frozen->flags);
		cur->size = ntohl(frozen->size);

		if (!head) {
			head = cur;

			if (strcmp(head->name, UTAG_HEAD) ||
				(0 != head->size)) {
					err = UTAG_ERR_INVALID_HEAD;
					goto err_free;
			}
		}

		/* check if this is the end */
		if (!strcmp(cur->name, UTAG_TAIL)) {
			/* footer payload size should be zero */
			if (0 != cur->size) {
				err = UTAG_ERR_INVALID_TAIL;
				goto err_free;
			}

			/* all done */
			break;
		}

		next_ptr = ptr + UTAG_MIN_TAG_SIZE + ROUNDUP(cur->size, 4);

		/*
		 * Ensure there is enough space in the buffer for both the
		 * payload and the tag header for the next tag.
		 */
		if ((next_ptr - (uint8_t *) buf) + UTAG_MIN_TAG_SIZE >
		    block_size) {
			err = UTAG_ERR_TAGS_TOO_BIG;
			goto err_free;
		}

		if (cur->size != 0) {
			cur->payload = kcalloc(1, cur->size, GFP_KERNEL);
			if (!cur->payload) {
				err = UTAG_ERR_OUT_OF_MEMORY;
				goto err_free;
			}
			memcpy(cur->payload, frozen->payload, cur->size);
		}

		/* advance to beginning of next tag */
		ptr = next_ptr;

		/* get ready for the next tag, calloc() sets memory to zero */
		cur->next = kcalloc(1, sizeof(struct utag), GFP_KERNEL);
		cur = cur->next;
		if (!cur) {
			err = UTAG_ERR_OUT_OF_MEMORY;
			goto err_free;
		}
	}			/* while (1) */

	goto out;

 err_free:
	free_tags(head);
	head = NULL;

 out:
	if (status)
		*status = err;

	return head;
}

static void *freeze_tags(size_t block_size, const struct utag *tags,
			 size_t *tags_size, enum utag_error *status)
{
	size_t frozen_size = 0;
	char *buf = NULL, *ptr;
	const struct utag *cur = tags;
	enum utag_error err = UTAG_NO_ERROR;
	size_t zeros;
	struct frozen_utag frozen;

	/* Make sure the tags start with the HEAD marker. */
	if (!tags || strncmp(tags->name, UTAG_HEAD, MAX_UTAG_NAME)) {
		err = UTAG_ERR_INVALID_HEAD;
		goto out;
	}

	/*
	 * Walk the list once to determine the amount of space to allocate
	 * for the frozen tags.
	 */
	while (cur) {
		frozen_size += ROUNDUP(cur->size, 4) + UTAG_MIN_TAG_SIZE;
		if (!strncmp(cur->name, UTAG_TAIL, MAX_UTAG_NAME))
			break;
		cur = cur->next;
	}

	/* round up frozen_size to eMMC sector size */
	frozen_size = TO_SECT_SIZE(frozen_size);

	/* do some more sanity checking */
	if (!cur || cur->next) {
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
		memcpy(frozen.name, cur->name, MAX_UTAG_NAME);
		frozen.flags = htonl(cur->flags);
		frozen.size = htonl(cur->size);

		memcpy(ptr, &frozen, UTAG_MIN_TAG_SIZE);
		ptr += UTAG_MIN_TAG_SIZE;

		if (cur->size) {
			memcpy(ptr, cur->payload, cur->size);
			ptr += cur->size;
		}

		/* pad with zeros if needed */
		zeros = ROUNDUP(cur->size, 4) - cur->size;
		if (zeros) {
			memset(ptr, 0, zeros);
			ptr += zeros;
		}

		if (!strncmp(cur->name, UTAG_TAIL, MAX_UTAG_NAME))
			break;

		cur = cur->next;
	}

	memset(ptr, 0, buf + frozen_size - ptr);

	if (tags_size)
		*tags_size = frozen_size;

 out:
	if (status)
		*status = err;

	return buf;
}

/*
 * Try to load utags into memory from a partition on secondary storage.
 *
 * Not thread safe, call from a safe context only
 */
static struct utag *load_utags(const char *partition_name)
{
	int res;
	size_t block_size;
	ssize_t bytes;
	struct utag *head = NULL;
	void *data;
	int file_errno;
	struct file *filp = NULL;
	struct inode *inode = NULL;
	enum utag_error *status;

	if (0 == partition_name[0]) {
		pr_err("%s utag partition not set\n", __func__);
		goto out;
	}

	filp = filp_open(partition_name, O_RDONLY, 0600);
	if (IS_ERR_OR_NULL(filp)) {
		file_errno = -PTR_ERR(filp);
		pr_err("%s ERR opening (%s) errno=%d\n", __func__,
		       partition_name, file_errno);
		goto out;
	}

	if (filp->f_path.dentry)
		inode = filp->f_path.dentry->d_inode;
	if (!inode || !S_ISBLK(inode->i_mode)) {
		pr_err("%s ERR (%s) not a block device\n", __func__,
		       partition_name);
		goto close_block;
	}

	res =
	    filp->f_op->unlocked_ioctl(filp, BLKGETSIZE64,
				       (unsigned long)&block_size);
	if (0 > res) {
		pr_err("%s ERR file (%s) ioctl failed\n", __func__,
		       partition_name);
		goto close_block;
	}

	data = kmalloc(block_size, GFP_KERNEL);
	if (!data) {
		pr_err("%s ERR file (%s) out of memory size %d\n", __func__,
		       partition_name, block_size);
		goto close_block;
	}

	bytes = kernel_read(filp, filp->f_pos, data, block_size);
	if ((ssize_t) block_size > bytes) {
		pr_err("%s ERR file (%s) read failed\n", __func__,
		       partition_name);
		goto free_data;
	}

	head = thaw_tags(block_size, data, status);

 free_data:
	kfree(data);

 close_block:
	filp_close(filp, NULL);

 out:

	return head;
}

static enum utag_error
replace_first_utag(struct utag *head, const char *name, void *payload,
		   size_t size)
{
	struct utag *utag;

	/* search for the first occurrence of specified type of tag */
	utag = find_first_utag(head, name);
	if (utag) {
		void *oldpayload = utag->payload;
		if (utag->flags & UTAG_FLAG_PROTECTED)
			return UTAG_ERR_PROTECTED;

		utag->payload = kmalloc(size, GFP_KERNEL);
		if (!utag->payload) {
			utag->payload = oldpayload;
			return UTAG_ERR_OUT_OF_MEMORY;
		}

		memcpy(utag->payload, payload, size);
		utag->size = size;
		kfree(oldpayload);
		return UTAG_NO_ERROR;
	}

	return UTAG_NO_ERROR;
}

static enum utag_error
flash_partition(const char *partition_name, const struct utag *tags)
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

	filep = filp_open(partition_name, O_RDWR, 0);
	if (IS_ERR_OR_NULL(filep)) {
		file_errno = -PTR_ERR(filep);
		pr_err("%s ERROR opening file (%s) errno=%d\n", __func__,
		       partition_name, file_errno);
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

static enum utag_error store_utags(const struct utag *tags)
{
	enum utag_error status = UTAG_NO_ERROR;

	status = flash_partition(utags_blkdev, tags);
	if (UTAG_NO_ERROR != status)
		pr_err("%s flash partition failed status %d\n", __func__,
		       status);

	if (0 != utags_backup[0]) {
		status = flash_partition(utags_backup, tags);
		if (UTAG_NO_ERROR != status)
			pr_err("%s flash backup partition failed status %d\n",
			__func__, status);
	}

	return status;
}

static int read_tag(struct seq_file *file, void *v)
{
	int i;
	uint8_t *tmp;
	struct utag *tags = NULL;
	struct utag *tag = NULL;
	struct proc_node *proc = (struct proc_node *)file->private;

	tags = load_utags(utags_blkdev);
	if (NULL == tags) {
		pr_err("%s Load config failed\n", __func__);
		return -EFAULT;
	} else {
		tag = find_first_utag(tags, proc->name);
		if (NULL == tag) {
			pr_err("Tag [%s] not found.\n", proc->name);
			free_tags(tags);
			return -EIO;
		}
		switch (proc->mode) {
		case OUT_ASCII:
			seq_printf(file, "%s\n", (char *)tag->payload);
			break;
		case OUT_RAW:
			tmp = (uint8_t *) tag->payload;
			for (i = 0; i < tag->size; i++)
				seq_printf(file, "%02X", tmp[i]);
			seq_printf(file, "\n");
			break;
		case OUT_TYPE:
			seq_printf(file, "%s\n", (char *)proc->type);
			break;
		}
	}

	free_tags(tags);
	return 0;
}

static ssize_t
write_utag(struct file *file, const char __user *buffer,
	   size_t count, loff_t *pos)
{
	struct utag *tags = NULL;
	enum utag_error status;
	struct inode *inode = file->f_dentry->d_inode;
	struct proc_node *proc = proc_get_parent_data(inode);

	if (OUT_TYPE == proc->mode) {
		return count;
	}

	if (MAX_UTAG_SIZE < count) {
		pr_err("%s error utag too big %d\n", __func__, count);
		return count;
	}

	if (copy_from_user(payload, buffer, count)) {
		pr_err("%s user copy error\n", __func__);
		return count;
	}

	tags = load_utags(utags_blkdev);
	if (NULL == tags) {
		pr_err("%s load config error\n", __func__);
		return count;
	} else {
		status =
		    replace_first_utag(tags, proc->name, payload, (count - 1));
		if (UTAG_NO_ERROR != status)
			pr_err("%s error on update tag [%s] status %d\n",
			       __func__, proc->name, status);
		else {
			status = store_utags(tags);
			if (UTAG_NO_ERROR != status)
				pr_err
				    ("%s error on store tags [%s] status %d\n",
				     __func__, proc->name, status);
		}
	}

	free_tags(tags);
	return count;
}

/*
 * Process new file request. Check for exiisting utag,
 * add empty new utag, save utags and add file interface
*/

static ssize_t
new_utag(struct file *file, const char __user *buffer,
	   size_t count, loff_t *pos)
{
	struct utag *tags, *cur;
	enum utag_error status;
	struct inode *inode = file->f_dentry->d_inode;
	struct proc_node *proc = proc_get_parent_data(inode);
	char uname[MAX_UTAG_NAME];
	char utype[MAX_UTAG_NAME];
	struct dir_node *dnode;
	struct proc_dir_entry *dir;

	if ((MAX_UTAG_NAME < count) || (0 == count)) {
		pr_err("%s invalid utag name %d\n", __func__, count);
		return count;
	}

	if (copy_from_user(payload, buffer, count)) {
		pr_err("%s user copy error\n", __func__);
		return count;
	}
	/* payload has input string plus \n. Replace \n with 00 */
	payload[count-1] = 0;
	if (!validate_name(payload, (count-1))) {
		pr_err("%s invalid format %s\n", __func__, payload);
		return count;
	}

	tags = load_utags(utags_blkdev);
	if (NULL == tags) {
		pr_err("%s load config error\n", __func__);
		return count;
	} else {
		/* Ignore request if utag name already in use */
		cur = find_first_utag(tags, payload);
		if (NULL != cur) {
			pr_err("%s error can not create [%s]. Already in use\n",
			       __func__, payload);
			goto out;
		} else {
		/* Add new utag after head, store changed partition */
			cur = kcalloc(1, sizeof(struct utag), GFP_KERNEL);
			if (!cur)
				goto out;
			strlcpy(cur->name, payload, MAX_UTAG_NAME);
			split_name(payload, uname, utype);
			cur->next = tags->next;
			tags->next = cur;

			status = store_utags(tags);
			if (UTAG_NO_ERROR != status) {
				pr_err
				    ("%s error on store tags [%s] status %d\n",
				     __func__, proc->name, status);
				goto out;
			}
		/* Add procfs elements for utag access */
			dir = proc_mkdir(uname, dir_root);
			if (!dir) {
				pr_err("%s Failed to create dir\n", __func__);
				goto out;
			}
			dnode = kmalloc(sizeof(struct dir_node), GFP_KERNEL);
			if (dnode) {
				dnode->root = dir_root;
				list_add(&dnode->entry, &dir_list);
				strlcpy(dnode->name, uname, MAX_UTAG_NAME);
			}

			utag_file(uname, utype, OUT_ASCII, dir, &utag_fops);
			utag_file(uname, utype, OUT_RAW, dir, &utag_fops);
			utag_file(uname, utype, OUT_TYPE, dir, &utag_fops);
		}
	}

out:	free_tags(tags);
	return count;
}


static int dump_all(struct seq_file *file, void *v)
{
	int i;
	char *data = NULL;
	struct utag *tags = NULL;
	uint32_t loc_csum = 0;

	tags = load_utags(utags_blkdev);
	if (NULL == tags)
		pr_err("%s Load config failed\n", __func__);
	else
		while (tags != NULL) {
			seq_printf(file,
				   "Tag: Name [%s] Size: [%d]\nData:\n\t",
				   tags->name, tags->size);
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

static int config_read(struct inode *inode, struct file *file)
{
	return single_open(file, read_tag, proc_get_parent_data(inode));
}

static int config_dump(struct inode *inode, struct file *file)
{
	return single_open(file, dump_all, proc_get_parent_data(inode));
}

static const struct file_operations utag_fops = {
	.owner = THIS_MODULE,
	.open = config_read,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = write_utag,
};

static const struct file_operations dump_fops = {
	.owner = THIS_MODULE,
	.open = config_dump,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations new_fops = {
	.owner = THIS_MODULE,
	.open = config_dump,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = new_utag,
};


static int __init config_init(void)
{
	struct utag *tags, *cur;
	struct dir_node *dnode;
	struct proc_dir_entry *dir = NULL;
	char utag_name[MAX_UTAG_NAME];
	char utag_type[MAX_UTAG_NAME];

	/* try to load utags from primary partition */
	tags = load_utags(utags_blkdev);
	if (NULL == tags) {
		pr_err("%s Can not open utags\n", __func__);
		return -EIO;
	}
	dir_root = proc_mkdir("config", NULL);
	if (!dir_root) {
		pr_err("%s Failed to create dir entry\n", __func__);
		free_tags(tags);
		return -EIO;
	}
	/* skip utags head */
	cur = tags->next;
	while (1) {
		/* skip utags tail */
		if (cur->next == NULL)
			break;
		split_name(cur->name, utag_name, utag_type);
		dir = proc_mkdir(utag_name, dir_root);
		if (!dir) {
			pr_err("%s Failed to create dir\n", __func__);
			goto out;
		}
		dnode = kmalloc(sizeof(struct dir_node), GFP_KERNEL);
		if (dnode) {
			dnode->root = dir_root;
			list_add(&dnode->entry, &dir_list);
			strlcpy(dnode->name, utag_name, MAX_UTAG_NAME);
		}

		utag_file(utag_name, utag_type, OUT_ASCII, dir, &utag_fops);
		utag_file(utag_name, utag_type, OUT_RAW, dir, &utag_fops);
		utag_file(utag_name, utag_type, OUT_TYPE, dir, &utag_fops);

		cur = cur->next;
	}

	/* add "all" directory for debug purposes */
	dir = proc_mkdir("all", dir_root);
	dnode = kmalloc(sizeof(struct dir_node), GFP_KERNEL);
	if (dnode) {
		dnode->root = dir_root;
		list_add(&dnode->entry, &dir_list);
		strlcpy(dnode->name, "all", MAX_UTAG_NAME);
	}

	utag_file("all", "raw", OUT_RAW, dir, &dump_fops);
	utag_file("all", "new", OUT_NEW, dir, &new_fops);

out:	free_tags(tags);

	return 0;
}

static void __exit config_exit(void)
{

	struct proc_node *node, *s = NULL;
	struct dir_node *dir_node, *c = NULL;

	list_for_each_entry_safe(node, s, &node_list, entry) {
		remove_proc_entry(node->file_name, node->dir);
		list_del(&node->entry);
		kfree(node);
	}
	list_for_each_entry_safe(dir_node, c, &dir_list, entry) {
		remove_proc_entry(dir_node->name, dir_root);
		list_del(&dir_node->entry);
		kfree(dir_node);
	}
	remove_proc_entry("config", NULL);
}

module_init(config_init);
module_exit(config_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Motorola Mobility LLC");
MODULE_DESCRIPTION("Configuration module");
