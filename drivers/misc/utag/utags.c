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
#include <linux/vmalloc.h>
#include <linux/of.h>

#define MAX_UTAG_SIZE 1024
#define MAX_UTAG_NAME 32
#define UTAG_HEAD  "__UTAG_HEAD__"
#define UTAG_TAIL  "__UTAG_TAIL__"
#define ROUNDUP(a, b) (((a) + ((b)-1)) & ~((b)-1))
#define TO_SECT_SIZE(n)     (((n) + 511) & ~511)
#define UTAGS_MAX_DEFERRALS 5
#define DRVNAME "utags"
#define DEFAULT_ROOT "config"

static const struct file_operations utag_fops;
struct ctrl;

enum utag_flag {
	UTAG_FLAG_PROTECTED = 1 << 0,
};

#define UTAG_STATUS_LOADED '0'
#define UTAG_STATUS_RELOAD '1'
#define UTAG_STATUS_NOT_READY '2'

struct utag {
	char name[MAX_UTAG_NAME];
	uint32_t size;
	uint32_t flags;
	uint32_t crc32;		/* reserved for futher implementation */
	void *payload;
	struct utag *next;
	struct utag *prev;
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
	struct ctrl *ctrl;
};

struct dir_node {
	struct list_head entry;
	struct proc_dir_entry *root;
	char name[MAX_UTAG_NAME];
	struct ctrl *ctrl;
	struct proc_dir_entry *dir;
};

struct blkdev {
	const char *name;
	struct file *filep;
	size_t size;
};

struct ctrl {
	struct blkdev main;
	struct blkdev backup;
	struct platform_device *pdev;
	struct proc_dir_entry *root;
	char reload;
	uint32_t csum;
	struct list_head dir_list;
	struct list_head node_list;
	const char *dir_name;
	uint32_t lock;
};

static void build_utags_directory(struct ctrl *ctrl);
static void clear_utags_directory(struct ctrl *ctrl);

/*
 * Open and store file handle for a utag partition
 *
 * Not thread safe, call from a safe context only
 */
static int open_utags(struct blkdev *cb)
{
	struct inode *inode;

	if (cb->filep)
		return 0;

	if (!cb->name)
		return -EIO;

	cb->filep = filp_open(cb->name, O_RDWR|O_SYNC, 0600);
	if (IS_ERR_OR_NULL(cb->filep)) {
		int rc = PTR_ERR(cb->filep);

		pr_err("%s opening (%s) errno=%d\n", __func__, cb->name, rc);
		cb->filep = NULL;
		return rc;
	}

	if (cb->filep->f_path.dentry)
		inode = cb->filep->f_path.dentry->d_inode;
	if (!inode || !S_ISBLK(inode->i_mode)) {
		pr_err("%s (%s) not a block device\n", __func__, cb->name);
		filp_close(cb->filep, NULL);
		cb->filep = NULL;
		return -EIO;
	}

	cb->size = i_size_read(inode->i_bdev->bd_inode);
	return 0;
}


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
	if (MAX_UTAG_NAME > type_length)
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
	  enum utag_output mode, struct dir_node *dnode,
	  const struct file_operations *fops)
{
	struct proc_node *node;
	struct ctrl *ctrl = dnode->ctrl;

	if (sizeof(files) < mode)
		return -EINVAL;

	node = kzalloc(sizeof(struct proc_node), GFP_KERNEL);
	if (node) {
		list_add(&node->entry, &ctrl->node_list);
		strlcpy(node->file_name, files[mode], MAX_UTAG_NAME);
		strlcpy(node->name, utag_name, MAX_UTAG_NAME);
		strlcpy(node->type, utag_type, MAX_UTAG_NAME);
		node->mode = mode;
		node->dir = dnode->dir;
		node->ctrl = ctrl;
		node->file = proc_create_data(node->file_name, 0,
			dnode->dir, fops, node);
	}

	return 0;
}

/*
 * Convert a block of tags, presumably loaded from seconday storage, into a
 * format that can be manipulated.
 */
static struct utag *thaw_tags(size_t block_size, void *buf)
{
	struct utag *head = NULL, *cur = NULL;
	uint8_t *ptr = buf;

	/*
	 * make sure the block is at least big enough to hold header
	 * and footer
	 */
	if (UTAG_MIN_TAG_SIZE * 2 > block_size) {
		pr_err("%s invalid tags size\n", __func__);
		return NULL;
	}

	while (1) {
		struct frozen_utag *frozen;
		uint8_t *next_ptr;

		frozen = (struct frozen_utag *)ptr;

		if (!head) {
			/* This is allocation of the head */
			cur = kzalloc(sizeof(struct utag), GFP_KERNEL);
			if (!cur)
				return NULL;
		}

		strlcpy(cur->name, frozen->name, MAX_UTAG_NAME - 1);
		cur->flags = ntohl(frozen->flags);
		cur->size = ntohl(frozen->size);

		if (!head) {
			head = cur;

			if (strcmp(head->name, UTAG_HEAD) ||
				(0 != head->size)) {
					pr_err("%s bad utags head\n", __func__);
					goto err_free;
			}
		}

		/* check if this is the end */
		if (!strcmp(cur->name, UTAG_TAIL)) {
			/* footer payload size should be zero */
			if (0 != cur->size) {
				pr_err("%s invalid utags tail\n", __func__);
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
			pr_err("%s invalid tags size\n", __func__);
			goto err_free;
		}

		if (cur->size != 0) {
			cur->payload = kzalloc(cur->size, GFP_KERNEL);
			if (!cur->payload)
				goto err_free;
			memcpy(cur->payload, frozen->payload, cur->size);
		}

		/* advance to beginning of next tag */
		ptr = next_ptr;

		/* get ready for the next tag */
		cur->next = kzalloc(sizeof(struct utag), GFP_KERNEL);
		cur->next->prev = cur;
		cur = cur->next;
		if (!cur)
			goto err_free;
	}			/* while (1) */

	goto out;

 err_free:
	free_tags(head);
	head = NULL;
 out:
	return head;
}

static void *freeze_tags(size_t block_size, const struct utag *tags,
	size_t *tags_size)
{
	size_t frozen_size = 0;
	char *buf = NULL, *ptr;
	const struct utag *cur = tags;
	size_t zeros;
	struct frozen_utag frozen;

	/* Make sure the tags start with the HEAD marker. */
	if (!tags || strncmp(tags->name, UTAG_HEAD, MAX_UTAG_NAME)) {
		pr_err("%s invalid utags head\n", __func__);
		return NULL;
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
		pr_err("%s utags corrupted\n", __func__);
		return NULL;
	}

	if (block_size < frozen_size) {
		pr_err("%s utag size %zu too big\n", __func__, frozen_size);
		return NULL;
	}

	ptr = buf = vmalloc(frozen_size);
	if (!buf)
		return NULL;

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

	return buf;
}

/*
 * Try to load utags into memory from a partition on secondary storage.
 *
 * Not thread safe, call from a safe context only
 */
static struct utag *load_utags(struct blkdev *cb)
{
	ssize_t bytes;
	struct utag *head = NULL;
	void *data;

	data = vmalloc(cb->size);
	if (!data)
		goto out;

	bytes = kernel_read(cb->filep, 0, data, cb->size);
	if (cb->size > bytes) {
		pr_err("%s ERR file (%s) read failed\n", __func__, cb->name);
		goto free_data;
	}

	head = thaw_tags(cb->size, data);

 free_data:
	vfree(data);
 out:
	return head;
}

static int replace_first_utag(struct utag *head, const char *name,
		void *payload, size_t size)
{
	struct utag *utag;

	/* search for the first occurrence of specified type of tag */
	utag = find_first_utag(head, name);
	if (utag) {
		void *oldpayload = utag->payload;
		if (utag->flags & UTAG_FLAG_PROTECTED) {
			pr_err("%s protected utag\n", __func__);
			return -EIO;
		}

		utag->payload = kzalloc(size, GFP_KERNEL);
		if (!utag->payload) {
			utag->payload = oldpayload;
			return -EIO;
		}

		memcpy(utag->payload, payload, size);
		utag->size = size;
		kfree(oldpayload);
		return 0;
	}

	return 0;
}

static int store_utags(struct ctrl *ctrl, const struct utag *tags)
{
	size_t written;
	size_t tags_size;
	char *datap = NULL;
	int rc = 0;
	mm_segment_t fs;
	loff_t pos = 0;
	struct file *fp = ctrl->main.filep;
	struct blkdev *cb = &ctrl->main;

	fs = get_fs();
	set_fs(KERNEL_DS);

	datap = freeze_tags(cb->size, tags, &tags_size);
	if (!datap) {
		rc = -EIO;
		goto out;
	}

	written = fp->f_op->write(fp, datap, tags_size, &pos);
	if (written < tags_size) {
		pr_err("%s ERR writing file (%s) ret %zu\n", __func__,
			cb->name, written);
		rc = -EIO;
	}

	fp = ctrl->backup.filep;
	cb = &ctrl->backup;
	pos = 0;

	if (fp) {
		written = fp->f_op->write(fp, datap, tags_size, &pos);
		if (written < tags_size) {
			pr_err("%s ERR writing file (%s) ret %zu\n", __func__,
				cb->name, written);
			rc = -EIO;
		}
	}
	vfree(datap);

 out:
	set_fs(fs);
	return rc;
}

static int read_tag(struct seq_file *file, void *v)
{
	int i;
	uint8_t *tmp;
	struct utag *tags = NULL;
	struct utag *tag = NULL;
	struct proc_node *proc = (struct proc_node *)file->private;
	struct ctrl *ctrl = proc->ctrl;

	tags = load_utags(&ctrl->main);
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
	struct inode *inode = file->f_dentry->d_inode;
	struct proc_node *proc = PDE_DATA(inode);
	struct ctrl *ctrl = proc->ctrl;
	char *tmp;

	if (OUT_TYPE == proc->mode) {
		return count;
	}

	if (MAX_UTAG_SIZE < count) {
		pr_err("%s error utag too big %zu\n", __func__, count);
		return count;
	}

	tmp = kzalloc(count, GFP_KERNEL);
	if (!tmp)
		return -ENOMEM;

	if (copy_from_user(tmp, buffer, count)) {
		pr_err("%s user copy error\n", __func__);
		kfree(tmp);
		return count;
	}

	tags = load_utags(&ctrl->main);
	if (NULL == tags) {
		pr_err("%s load config error\n", __func__);
		kfree(tmp);
		return count;
	} else {
		if (replace_first_utag(tags, proc->name, tmp, (count - 1)))
			pr_err("%s error replace [%s]\n", __func__, proc->name);
		else if (store_utags(ctrl, tags))
			pr_err("%s error store [%s]\n", __func__, proc->name);
	}

	kfree(tmp);
	free_tags(tags);
	return count;
}

/*
 * Process delete file request. Check for exiisting utag,
 * delete it, save utags, update proc fs
*/

static ssize_t delete_store(struct device *dev, struct device_attribute *attr,
	const char *buffer, size_t count)
{
	struct utag *tags, *cur;
	struct ctrl *ctrl = dev_get_drvdata(dev);
	char *tmp;

	if ((MAX_UTAG_NAME < count) || (0 == count)) {
		pr_err("%s invalid utag name %zu\n", __func__, count);
		return -EIO;
	}

	tmp = kzalloc(count, GFP_KERNEL);
	if (!tmp)
		return -ENOMEM;

	memcpy(tmp, buffer, count);

	/* payload has input string plus \n. Replace \n with 00 */
	tmp[count-1] = 0;
	if (!validate_name(tmp, (count-1))) {
		pr_err("%s invalid format %s\n", __func__, tmp);
		count = -1;
		goto out_noload;
	}

	tags = load_utags(&ctrl->main);
	if (NULL == tags) {
		pr_err("%s load config error\n", __func__);
		count = -1;
		goto out_noload;
	} else {
		/* Ignore request if utag name already in use */
		cur = find_first_utag(tags, tmp);
		if (NULL != cur) {
			/* update pointers */
			cur->prev->next = cur->next;
			cur->next->prev = cur->prev;
			store_utags(ctrl, tags);
			clear_utags_directory(ctrl);
			build_utags_directory(ctrl);
			kfree(cur);
			goto out;
		} else {
			pr_err("%s error can not find %s", __func__, tmp);
			goto out;
		}
	}

out:	free_tags(tags);
out_noload:
	kfree(tmp);
	return count;
}
DEVICE_ATTR(delete, S_IWUSR, NULL, delete_store);

/*
 * Process new file request. Check for exiisting utag,
 * add empty new utag, save utags and add file interface
*/

static ssize_t
new_utag(struct file *file, const char __user *buffer,
	   size_t count, loff_t *pos)
{
	struct utag *tags, *cur;
	struct inode *inode = file->f_dentry->d_inode;
	struct proc_node *proc = PDE_DATA(inode);
	char uname[MAX_UTAG_NAME];
	char utype[MAX_UTAG_NAME];
	struct dir_node *dnode;
	struct proc_dir_entry *dir;
	struct ctrl *ctrl = proc->ctrl;
	char *tmp;

	if ((MAX_UTAG_NAME < count) || (0 == count)) {
		pr_err("%s invalid utag name %zu\n", __func__, count);
		return count;
	}

	tmp = kzalloc(count, GFP_KERNEL);
	if (!tmp)
		return -ENOMEM;

	if (copy_from_user(tmp, buffer, count)) {
		pr_err("%s user copy error\n", __func__);
		goto out_noload;
	}
	/* payload has input string plus \n. Replace \n with 00 */
	tmp[count-1] = 0;
	if (!validate_name(tmp, (count-1))) {
		pr_err("%s invalid format %s\n", __func__, tmp);
		goto out_noload;
	}

	tags = load_utags(&ctrl->main);
	if (NULL == tags) {
		pr_err("%s load config error\n", __func__);
		goto out_noload;
	} else {
		/* Ignore request if utag name already in use */
		cur = find_first_utag(tags, tmp);
		if (NULL != cur) {
			pr_err("%s error can not create [%s]. Already in use\n",
			       __func__, tmp);
			goto out;
		} else {
		/* Add new utag after head, store changed partition */
			cur = kzalloc(sizeof(struct utag), GFP_KERNEL);
			if (!cur)
				goto out;
			strlcpy(cur->name, tmp, MAX_UTAG_NAME);
			split_name(tmp, uname, utype);
			cur->next = tags->next;
			tags->next->prev = cur;
			tags->next = cur;
			cur->prev = tags;

			if (store_utags(ctrl, tags)) {
				pr_err("%s error tag [%s]\n",
					__func__, proc->name);
				goto out;
			}
		/* Add procfs elements for utag access */
			dir = proc_mkdir(uname, ctrl->root);
			if (!dir) {
				pr_err("%s Failed to create dir\n", __func__);
				goto out;
			}
			dnode = kzalloc(sizeof(struct dir_node), GFP_KERNEL);
			if (dnode) {
				dnode->root = ctrl->root;
				dnode->ctrl = ctrl;
				dnode->dir = dir;
				list_add(&dnode->entry, &ctrl->dir_list);
				strlcpy(dnode->name, uname, MAX_UTAG_NAME);
			}

			utag_file(uname, utype, OUT_ASCII, dnode, &utag_fops);
			utag_file(uname, utype, OUT_RAW, dnode, &utag_fops);
			utag_file(uname, utype, OUT_TYPE, dnode, &utag_fops);
		}
	}

out:	free_tags(tags);
out_noload:
	kfree(tmp);
	return count;
}

static ssize_t lock_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct ctrl *ctrl = dev_get_drvdata(dev);
	size_t max = PAGE_SIZE;

	if (!ctrl) {
		dev_err(dev, "%s device data not set\n", __func__);
		return 0;
	}

	dev_info(dev, "%s lock is %d\n", ctrl->main.name, ctrl->lock);
	return snprintf(buf, max, "%d\n", ctrl->lock);
}

static ssize_t lock_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct ctrl *ctrl = dev_get_drvdata(dev);

	if (!ctrl) {
		dev_err(dev, "%s device data not set\n", __func__);
		return 0;
	}

	if (!strncasecmp(buf, "lock", 4)) {
		ctrl->lock = 1;
		goto lock_done;
	}

	if (!strncasecmp(buf, "unlock", 6)) {
		ctrl->lock = 0;
		goto lock_done;
	}

lock_done:
	dev_info(dev, "lock %s lock %d\n", ctrl->main.name, ctrl->lock);
	return count;
}
DEVICE_ATTR(lock, (S_IWUSR | S_IRUGO), lock_show, lock_store);

static int reload_show(struct seq_file *file, void *v)
{
	struct ctrl *ctrl = (struct ctrl *)file->private;

	seq_printf(file, "%c\n", ctrl->reload);
	return 0;
}

static ssize_t reload_write(struct file *file, const char __user *buffer,
	   size_t count, loff_t *pos)
{
	struct inode *inode = file->f_dentry->d_inode;
	struct ctrl *ctrl = PDE_DATA(inode);

	/* only single character input plus new line */
	if (2 < count) {
		pr_err("%s invalid command length\n", __func__);
		goto out;
	}

	if (copy_from_user(&ctrl->reload, buffer, 1)) {
		pr_err("%s user copy error\n", __func__);
		return -EFAULT;
	}

	if (UTAG_STATUS_RELOAD == ctrl->reload) {
		if (open_utags(&ctrl->main))
			return count;
		open_utags(&ctrl->backup);
		clear_utags_directory(ctrl);
		build_utags_directory(ctrl);
	}

out:
	return count;
}

static int config_read(struct inode *inode, struct file *file)
{
	return single_open(file, read_tag, PDE_DATA(inode));
}

static int reload_open(struct inode *inode, struct file *file)
{
	return single_open(file, reload_show, PDE_DATA(inode));
}

static const struct file_operations utag_fops = {
	.owner = THIS_MODULE,
	.open = config_read,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = write_utag,
};

static const struct file_operations new_fops = {
	.read = NULL,
	.write = new_utag,
};

static const struct file_operations reload_fops = {
	.owner = THIS_MODULE,
	.open = reload_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = reload_write,
};

static void build_utags_directory(struct ctrl *ctrl)
{
	struct utag *tags, *cur;
	struct dir_node *dnode;
	struct proc_dir_entry *dir = NULL;
	char utag_name[MAX_UTAG_NAME];
	char utag_type[MAX_UTAG_NAME];

	/* try to load utags from primary partition */
	tags = load_utags(&ctrl->main);
	if (NULL == tags) {
		pr_warn("%s Can not open utags\n", __func__);
		return;
	}
	/* skip utags head */
	cur = tags->next;
	while (1) {
		/* skip utags tail */
		if (cur->next == NULL)
			break;
		split_name(cur->name, utag_name, utag_type);
		dir = proc_mkdir(utag_name, ctrl->root);
		if (!dir) {
			pr_err("%s Failed to create dir\n", __func__);
			break;
		}
		dnode = kzalloc(sizeof(struct dir_node), GFP_KERNEL);
		if (dnode) {
			dnode->root = ctrl->root;
			dnode->ctrl = ctrl;
			dnode->dir = dir;
			list_add(&dnode->entry, &ctrl->dir_list);
			strlcpy(dnode->name, utag_name, MAX_UTAG_NAME);
		}

		utag_file(utag_name, utag_type, OUT_ASCII, dnode, &utag_fops);
		utag_file(utag_name, utag_type, OUT_RAW, dnode, &utag_fops);
		utag_file(utag_name, utag_type, OUT_TYPE, dnode, &utag_fops);

		cur = cur->next;
	}

	/* add "all" directory for debug purposes */
	dir = proc_mkdir("all", ctrl->root);
	dnode = kzalloc(sizeof(struct dir_node), GFP_KERNEL);
	if (dnode) {
		dnode->root = ctrl->root;
		dnode->ctrl = ctrl;
		dnode->dir = dir;
		list_add(&dnode->entry, &ctrl->dir_list);
		strlcpy(dnode->name, "all", MAX_UTAG_NAME);
	}

	utag_file("all", "new", OUT_NEW, dnode, &new_fops);

	free_tags(tags);
	ctrl->reload = UTAG_STATUS_LOADED;
}

#ifdef CONFIG_OF
static int utags_dt_init(struct platform_device *pdev)
{
	int rc;
	struct device_node *node = pdev->dev.of_node;
	struct ctrl *ctrl;

	ctrl = dev_get_drvdata(&pdev->dev);
	rc = of_property_read_string(node, "mmi,main-utags",
		&ctrl->main.name);
	if (rc) {
		pr_err("%s storage path not provided\n", __func__);
		return -EIO;
	}

	rc = of_property_read_string(node, "mmi,backup-utags",
		&ctrl->backup.name);
	if (rc)
		pr_err("%s backup storage path not provided\n", __func__);

	ctrl->dir_name = DEFAULT_ROOT;
	rc = of_property_read_string(node, "mmi,dir-name", &ctrl->dir_name);
	if (!rc)
		pr_info("%s utag dir override %s\n", __func__, ctrl->dir_name);

	return 0;
}
#else
static int utags_dt_init(struct platform_device *pdev) { return -EINVAL; }
#endif

static void clear_utags_directory(struct ctrl *ctrl)
{
	struct proc_node *node, *s = NULL;
	struct dir_node *dir_node, *c = NULL;

	list_for_each_entry_safe(node, s, &ctrl->node_list, entry) {
		remove_proc_entry(node->file_name, node->dir);
		list_del(&node->entry);
		kfree(node);
	}
	list_for_each_entry_safe(dir_node, c, &ctrl->dir_list, entry) {
		remove_proc_entry(dir_node->name, ctrl->root);
		list_del(&dir_node->entry);
		kfree(dir_node);
	}
}

static struct attribute *utag_device_attrs[] = {
	&dev_attr_lock.attr,
	&dev_attr_delete.attr,
	NULL,
};

static const struct attribute_group utag_device_group = {
	.attrs = utag_device_attrs,
};

static int utags_probe(struct platform_device *pdev)
{
	int rc;
	static int retry;
	struct ctrl *ctrl;

	ctrl = devm_kzalloc(&pdev->dev, sizeof(struct ctrl), GFP_KERNEL);
	if (!ctrl)
		return -ENOMEM;

	INIT_LIST_HEAD(&ctrl->dir_list);
	INIT_LIST_HEAD(&ctrl->node_list);
	ctrl->pdev = pdev;
	ctrl->reload = UTAG_STATUS_NOT_READY;
	dev_set_drvdata(&pdev->dev, ctrl);

	rc = utags_dt_init(pdev);
	if (rc)
		return -EIO;

	rc = open_utags(&ctrl->main);
	if ((rc == -ENOENT) && (++retry < UTAGS_MAX_DEFERRALS))
		return -EPROBE_DEFER;
	else if (rc)
		pr_err("%s failed to open, try reload later\n", __func__);

	if (!ctrl->backup.name)
		pr_err("%s backup storage path not provided\n", __func__);
	else
		open_utags(&ctrl->backup);

	ctrl->root = proc_mkdir(ctrl->dir_name, NULL);
	if (!ctrl->root) {
		pr_err("%s Failed to create dir entry\n", __func__);
		return -EIO;
	}

	if (!proc_create_data("reload", 0600, ctrl->root, &reload_fops, ctrl)) {
		pr_err("%s Failed to create reload entry\n", __func__);
		remove_proc_entry(ctrl->dir_name, NULL);
		return -EIO;
	}

	rc = sysfs_create_group(&pdev->dev.kobj, &utag_device_group);
	if (rc) {
		pr_err("%s sysfs create group %d\n", __func__, rc);
		remove_proc_entry("reload", ctrl->root);
		remove_proc_entry(ctrl->dir_name, NULL);
		return -EIO;
	}

	pr_info("%s Success\n", __func__);

	return 0;
}

static int utags_remove(struct platform_device *pdev)
{
	struct ctrl *ctrl;

	ctrl = dev_get_drvdata(&pdev->dev);
	clear_utags_directory(ctrl);
	remove_proc_entry("reload", ctrl->root);
	sysfs_remove_group(&pdev->dev.kobj, &utag_device_group);
	remove_proc_entry(ctrl->dir_name, NULL);
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id utags_match_table[] = {
	{	.compatible = "mmi,utags",
	},
	{}
};
#endif

static struct platform_driver utags_driver = {
	.probe = utags_probe,
	.remove = utags_remove,
	.driver = {
		.name = DRVNAME,
		.bus = &platform_bus_type,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(utags_match_table),
	},
};

static int __init utags_init(void)
{
	return platform_driver_register(&utags_driver);
}

static void __exit utags_exit(void)
{
	platform_driver_unregister(&utags_driver);
}

late_initcall(utags_init);
module_exit(utags_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Motorola Mobility LLC");
MODULE_DESCRIPTION("Configuration module");
