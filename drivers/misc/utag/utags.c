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
#define pr_fmt(fmt) "utags (%s): " fmt, __func__

#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/ioctl.h>
#include <linux/input.h>
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
#define UTAG_DEPTH 5
#define UTAG_HEAD  "__UTAG_HEAD__"
#define UTAG_TAIL  "__UTAG_TAIL__"
#define ROUNDUP(a, b) (((a) + ((b)-1)) & ~((b)-1))
#define TO_SECT_SIZE(n)     (((n) + 511) & ~511)
#define UTAGS_MAX_DEFERRALS 5
#define DRVNAME "utags"
#define DEFAULT_ROOT "config"
#define HW_ROOT "hw"

static const struct file_operations utag_fops;
struct ctrl;

enum utag_flag {
	UTAG_FLAG_PROTECTED = 1 << 0,
};

#define UTAG_STATUS_LOADED '0'
#define UTAG_STATUS_RELOAD '1'
#define UTAG_STATUS_NOT_READY '2'

struct utag {
	char name[MAX_UTAG_NAME]; /* UTAG name and type combined */
	char name_only[MAX_UTAG_NAME]; /* UTAG name with type removed */
	uint32_t size;
	uint32_t flags;
	uint32_t util;
	void *payload;
	struct utag *next;
	struct utag *prev;
};

struct frozen_utag {
	char name[MAX_UTAG_NAME];
	uint32_t size;
	uint32_t flags;
	uint32_t util;
	uint8_t payload[];
};

#define UTAG_MIN_TAG_SIZE   (sizeof(struct frozen_utag))

enum utag_output {
	OUT_ASCII = 0,
	OUT_RAW,
	OUT_TYPE
};

static char *files[] = {
	"ascii",
	"raw",
	"type"
};

struct proc_node {
	struct list_head entry;
	char name[MAX_UTAG_NAME]; /* UTAG name string */
	char type[MAX_UTAG_NAME]; /* UTAG type string */
	char file_name[MAX_UTAG_NAME];
	struct proc_dir_entry *file;
	struct proc_dir_entry *dir;
	uint32_t mode;
	struct ctrl *ctrl;
};

struct dir_node {
	struct list_head entry;
	char name[MAX_UTAG_NAME];
	char path[MAX_UTAG_NAME];
	struct proc_dir_entry *dir;
	struct proc_dir_entry *parent;
	struct ctrl *ctrl;
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
	uint32_t key;
	size_t rsize;
	struct list_head dir_list;
	struct list_head node_list;
	const char *dir_name;
	uint32_t lock;
	struct work_struct delete_work;
};

static void build_utags_directory(struct ctrl *ctrl);
static void clear_utags_directory(struct ctrl *ctrl);

static int store_utags(struct ctrl *ctrl, struct utag *tags);

static ssize_t write_utag(struct file *file, const char __user *buffer,
	   size_t count, loff_t *pos);
static ssize_t new_utag(struct file *file, const char __user *buffer,
	   size_t count, loff_t *pos);
static ssize_t delete_utag(struct file *file, const char __user *buffer,
	   size_t count, loff_t *pos);
static ssize_t lock_store(struct file *file, const char __user *buffer,
	   size_t count, loff_t *pos);

static int lock_open(struct inode *inode, struct file *file);
static int partition_open(struct inode *inode, struct file *file);

static const struct file_operations utag_fops = {
	.owner = THIS_MODULE,
	.open = partition_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = write_utag,
};

static const struct file_operations new_fops = {
	.read = NULL,
	.write = new_utag,
};

static const struct file_operations lock_fops = {
	.owner = THIS_MODULE,
	.open = lock_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = lock_store,
};

static const struct file_operations delete_fops = {
	.read = NULL,
	.write = delete_utag,
};

static char *get_dir_name(struct ctrl *ctrl, struct proc_dir_entry *dir)
{
	struct dir_node *c, *dir_node = NULL;

	list_for_each_entry(c, &ctrl->dir_list, entry) {
		if (c->dir == dir) {
			dir_node = c;
			break;
		}
	}
	return dir_node ? dir_node->name : "na";
}

/*
 * Check util field of head utag for actual data size
 *
 */
static size_t data_size(struct blkdev *cb)
{
	size_t bytes;
	struct frozen_utag buf;
	struct ctrl *ctrl = container_of(cb, struct ctrl, main);

	bytes = kernel_read(cb->filep, 0, (void *) &buf, UTAG_MIN_TAG_SIZE);
	if (UTAG_MIN_TAG_SIZE > bytes) {
		pr_err("ERR file (%s) read failed\n", cb->name);
		return 0;
	}

	bytes = ntohl(buf.flags);
	ctrl->key = ntohl(buf.util);
	pr_debug("utag file (%s) saved size %zu block size %zu key %u\n",
				cb->name, bytes, cb->size, ctrl->key);

	/* Until in sync with BL use saved size only for HW block */
	if (!strncmp(ctrl->dir_name, HW_ROOT, sizeof(HW_ROOT))) {
		pr_err("hw partition block size logic utag\n");
		if (!bytes || bytes > cb->size)
			bytes = cb->size;
	} else
			bytes = cb->size;

	ctrl->rsize = bytes;

	pr_debug("utag reading %zu bytes\n", bytes);
	return bytes;
}

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

		pr_err("opening (%s) errno=%d\n", cb->name, rc);
		cb->filep = NULL;
		return rc;
	}

	if (cb->filep->f_path.dentry)
		inode = cb->filep->f_path.dentry->d_inode;
	if (!inode || !S_ISBLK(inode->i_mode)) {
		pr_err("(%s) not a block device\n", cb->name);
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

static void walk_tags(struct utag *tags)
{
	struct utag *next;

	while (tags) {
		next = tags->next;
		pr_debug("utag [%s], payload size %u\n",
			tags->name, tags->size);
		tags = next;
	}
}

static inline void walk_proc_nodes(struct ctrl *ctrl)
{
	struct proc_node *p;

	list_for_each_entry(p, &ctrl->node_list, entry) {
		pr_debug("proc-node [%s:%s:%s]\n",
			p->name, p->type, p->file_name);
	}
}

static inline void walk_dir_nodes(struct ctrl *ctrl)
{
	struct dir_node *d;

	list_for_each_entry(d, &ctrl->dir_list, entry) {
		pr_debug("dir-node [%s] path [%s]\n", d->name, d->path);
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

	pr_debug("cmp (%s) <=> (%s)\n)", s1, s2);
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
 * validate path to have :type as a last element
 */

static int check_path(char *fullpath, int count)
{
	char *ptr, *bptr;

		pr_err("path %s\n", fullpath);
	ptr = strnchr(fullpath, count, ':');
	bptr = strnchr(fullpath, count, '/');
	if (bptr && ptr && bptr > ptr) {
		pr_err("Invalid path %s\n", fullpath);
		return -EINVAL;
	}

	return 0;
}


/*
 * Extract hierarchical names from the fullpath
 * Return: number of pointers in name array
 */

static int full_split(char *fullpath, char **name, char **type)
{
	int i;
	char *ptr;

	ptr = strnchr(fullpath, MAX_UTAG_NAME, ':');
	if (ptr) {
		*ptr++ = 0;
		*type = ptr;
		pr_debug("type=%s\n", *type);
	}

	for (i = 0, ptr = fullpath; ptr; i++) {
		if (*ptr == '/')
			*ptr++ = 0;
		name[i] = ptr;
		ptr = strnchr(ptr, MAX_UTAG_NAME, '/');
		pr_debug("name[%d]=%s\n", i, name[i]);
	}

	return i;
}

static int add_utag_tail(struct utag *head, char *utag_name, char *utag_type)
{
	char utag[MAX_UTAG_NAME];
	struct utag *new, *tail;

	scnprintf(utag, MAX_UTAG_NAME, "%s%s%s", utag_name,
		utag_type ? ":" : "", utag_type ? utag_type : "");

	/* find list tail and check for duplicate utag */
	for (tail = head; tail->next; tail = tail->next) {
		if (!strncmp(tail->name_only, utag, MAX_UTAG_NAME)) {
			pr_debug("utag [%s] already exists\n", utag);
			return -EEXIST;
		}
	}
	pr_debug("tail utag [%s]\n", tail->name);

	new = kzalloc(sizeof(struct utag), GFP_KERNEL);
	if (!new) {
		pr_err("cannot allocate utag %s\n", utag_name);
		return -ENOMEM;
	}

	strlcpy(new->name, utag, MAX_UTAG_NAME);
	new->size = new->flags = new->util = 0;

	if (!tail->prev) { /* tail is in fact the head */
		tail->next = new;
		new->prev = tail;
	} else {
		/* insert new utag before tail */
		new->prev = tail->prev;
		new->next = tail;
		tail->prev->next = new;
		tail->prev = new;
	}
	walk_tags(head);

	return 0;
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

static int proc_utag_file(char *utag_name, char *utag_type,
	  enum utag_output mode, struct dir_node *dnode,
	  const struct file_operations *fops)
{
	struct proc_node *node;
	struct ctrl *ctrl = dnode->ctrl;

	if (sizeof(files) < mode)
		return -EINVAL;

	node = kzalloc(sizeof(struct proc_node), GFP_KERNEL);
	if (node) {
		list_add_tail(&node->entry, &ctrl->node_list);
		strlcpy(node->file_name, files[mode], MAX_UTAG_NAME);
		strlcpy(node->name, utag_name, MAX_UTAG_NAME);
		if (utag_type)
			strlcpy(node->type, utag_type, MAX_UTAG_NAME);
		else
			node->type[0] = 0;
		node->mode = mode;
		node->dir = dnode->dir;
		node->ctrl = ctrl;
		node->file = proc_create_data(node->file_name, 0,
			dnode->dir, fops, node);

		pr_debug("created file [%s/%s]\n",
			get_dir_name(ctrl, dnode->dir), node->file_name);
	}
	return 0;
}

static struct dir_node *find_dir_node(struct ctrl *ctrl, char *path)
{
	struct dir_node *c, *dir_node = NULL;

	list_for_each_entry(c, &ctrl->dir_list, entry) {
		if (!strncmp(c->path, path, MAX_UTAG_NAME)) {
			dir_node = c;
			break;
		}
	}
	return dir_node;
}

static int proc_utag_util(struct ctrl *ctrl)
{
	struct proc_dir_entry *dir;

	dir = proc_mkdir("all", ctrl->root);
	if (!dir) {
		pr_err("failed to create util\n");
		return -EIO;
	}

	if (!proc_create_data("new", 0600, dir, &new_fops, ctrl)) {
		pr_err("Failed to create utag new entry\n");
		return -EIO;
	}

	if (!proc_create_data("lock", 0600, dir, &lock_fops, ctrl)) {
		pr_err("Failed to create lock entry\n");
		return -EIO;
	}

	if (!proc_create_data("delete", 0600, dir, &delete_fops, ctrl)) {
		pr_err("Failed to create delete entry\n");
		return -EIO;
	}

	return 0;
}

static struct proc_dir_entry *proc_utag_dir(struct ctrl *ctrl,
	char *tname, char *path, char *ttype, bool populate,
	struct proc_dir_entry *parent)
{
	struct dir_node *dnode;
	struct proc_dir_entry *dir;

	if (!parent)
		parent = ctrl->root;

	dnode = find_dir_node(ctrl, path);
	if (dnode) {
		if (populate)
			goto populate_utag_dir;

		pr_info("procfs dir %s exists; skip\n", tname);
		return dnode->dir;
	}

	dir = proc_mkdir(tname, parent);
	if (!dir) {
		pr_err("failed to create dir %s\n", tname);
		return ERR_PTR(-ENOMEM);
	}

	dnode = kzalloc(sizeof(struct dir_node), GFP_KERNEL);
	if (!dnode) {
		kfree(dir);
		pr_err("failed to create node structure\n");
		return ERR_PTR(-ENOMEM);
	}

	dnode->parent = parent;
	dnode->ctrl = ctrl;
	dnode->dir = dir;
	list_add_tail(&dnode->entry, &ctrl->dir_list);
	strlcpy(dnode->name, tname, MAX_UTAG_NAME);
	strlcpy(dnode->path, path, MAX_UTAG_NAME);

	if (!populate)
		return dir;

populate_utag_dir:

	proc_utag_file(tname, ttype, OUT_ASCII, dnode, &utag_fops);
	proc_utag_file(tname, ttype, OUT_RAW, dnode, &utag_fops);
	proc_utag_file(tname, ttype, OUT_TYPE, dnode, &utag_fops);

	return dir;
}

/*
 * Convert a block of tags, presumably loaded from seconday storage, into a
 * format that can be manipulated.
 */
static struct utag *thaw_tags(size_t block_size, void *buf)
{
	struct utag *head = NULL, *cur = NULL;
	uint8_t *ptr = buf;

	while (1) {
		struct frozen_utag *frozen;
		uint8_t *next_ptr;
		char *sep;

		frozen = (struct frozen_utag *)ptr;

		if (!head) {
			/* This is allocation of the head */
			cur = kzalloc(sizeof(struct utag), GFP_KERNEL);
			if (!cur)
				return NULL;
		}

		strlcpy(cur->name, frozen->name, MAX_UTAG_NAME - 1);
		strlcpy(cur->name_only, frozen->name, MAX_UTAG_NAME-1);
		sep = strnchr(cur->name_only, MAX_UTAG_NAME, ':');
		if (sep)
			*sep = 0;
		cur->flags = ntohl(frozen->flags);
		cur->util = ntohl(frozen->util);
		cur->size = ntohl(frozen->size);

		if (!head) {
			head = cur;

			if (strcmp(head->name, UTAG_HEAD)) {
				pr_err("invalid or empty utags partition\n");
				goto err_free;
			}
		}

		/* moved here to print statistics for tail as well */
		next_ptr = ptr + UTAG_MIN_TAG_SIZE + ROUNDUP(cur->size, 4);
		pr_debug("utag [%s] size %zu\n", cur->name, next_ptr - ptr);

		/* check if this is the end */
		if (!strcmp(cur->name, UTAG_TAIL)) {
			/* footer payload size should be zero */
			if (0 != cur->size) {
				pr_err("invalid utags tail\n");
				goto err_free;
			}

			/* all done */
			break;
		}

		/*
		 * Ensure there is enough space in the buffer for both the
		 * payload and the tag header for the next tag.
		 */
		if ((next_ptr - (uint8_t *) buf) + UTAG_MIN_TAG_SIZE >
		    block_size) {
			pr_err("invalid tags size\n");
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
		/* FIXME if kzalloc fails, kernel will panic in the next line */
		cur->next->prev = cur;
		cur = cur->next;
		if (!cur)
			goto err_free;
	} /* while (1) */

	walk_tags(head);
	goto out;

 err_free:
	free_tags(head);
	head = NULL;
 out:
	return head;
}

static void *freeze_tags(size_t block_size, struct utag *tags,
	size_t *tags_size)
{
	size_t written, frozen_size = 0;
	char *buf = NULL, *ptr;
	struct utag *cur = tags;
	size_t zeros;
	struct frozen_utag frozen = { {0} };

	/* Make sure the tags start with the HEAD marker. */
	if (!tags || strncmp(tags->name, UTAG_HEAD, MAX_UTAG_NAME)) {
		pr_err("invalid utags head\n");
		return NULL;
	}

	/*
	 * Walk the list once to determine the amount of space to allocate
	 * for the frozen tags.
	 */
	while (cur) {
		pr_debug("utag [%s], payload size %u\n", cur->name, cur->size);
		frozen_size += ROUNDUP(cur->size, 4) + UTAG_MIN_TAG_SIZE;
		pr_debug("calculated size %zu\n", frozen_size);
		if (!strncmp(cur->name, UTAG_TAIL, MAX_UTAG_NAME))
			break;
		cur = cur->next;
	}

	/* round up frozen_size to eMMC sector size */
	frozen_size = TO_SECT_SIZE(frozen_size);
	pr_debug("frozen size aligned to sector size %zu\n", frozen_size);

	/* do some more sanity checking */
	if (!cur || cur->next) {
		pr_err("utags corrupted\n");
		return NULL;
	}

	if (block_size < frozen_size) {
		pr_err("utag size %zu too big\n", frozen_size);
		return NULL;
	}

	ptr = buf = vmalloc(frozen_size);
	if (!buf)
		return NULL;

	cur = tags;
	/* root utag stores size of entire image in flags word */
	cur->flags = frozen_size;
	while (1) {
		written = 0;
		memcpy(frozen.name, cur->name, MAX_UTAG_NAME);
		frozen.flags = htonl(cur->flags);
		frozen.size = htonl(cur->size);
		frozen.util = htonl(cur->util);

		memcpy(ptr, &frozen, UTAG_MIN_TAG_SIZE);
		ptr += UTAG_MIN_TAG_SIZE;
		written += UTAG_MIN_TAG_SIZE;

		if (cur->size) {
			memcpy(ptr, cur->payload, cur->size);
			ptr += cur->size;
			written += cur->size;
		}

		/* pad with zeros if needed */
		zeros = ROUNDUP(cur->size, 4) - cur->size;
		if (zeros) {
			memset(ptr, 0, zeros);
			ptr += zeros;
			written += zeros;
		}

		pr_debug("written %zu bytes for utag [%s]\n",
			written, cur->name);

		if (!strncmp(cur->name, UTAG_TAIL, MAX_UTAG_NAME))
			break;

		cur = cur->next;
	}

	memset(ptr, 0, buf + frozen_size - ptr);
	if ((buf + frozen_size - ptr))
		pr_debug("padded %zu bytes\n", buf + frozen_size - ptr);
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
	size_t bytes;
	void *data;
	struct utag *head = NULL;

	bytes = data_size(cb);

	/*
	 * make sure the block is at least big enough to hold header
	 * and footer
	 */
	if (UTAG_MIN_TAG_SIZE * 2 > bytes) {
		pr_err("invalid tags size %zu\n", bytes);
		return NULL;
	}

	data = vmalloc(bytes);
	if (!data)
		return NULL;

	if (bytes != kernel_read(cb->filep, 0, data, bytes)) {
		pr_err("ERR file (%s) read failed\n", cb->name);
		goto free_data;
	}

	head = thaw_tags(bytes, data);
	if (!head) { /* initialize empty utags partition */
		int error;
		struct ctrl *ctrl = container_of(cb, struct ctrl, main);

		head = kzalloc(sizeof(struct utag), GFP_KERNEL);
		if (!head)
			goto free_data;
		strlcpy(head->name, UTAG_HEAD, MAX_UTAG_NAME);
		add_utag_tail(head, UTAG_TAIL, NULL);

		error = store_utags(ctrl, head);
		if (error)
			pr_err("error storing utags partition\n");
	}

 free_data:
	vfree(data);
	return head;
}

static int full_utag_name(struct proc_node *pnode, char *tag)
{
	int i, subdir, blen;
	char *subdir_names[UTAG_DEPTH];
	struct ctrl *ctrl = pnode->ctrl;
	struct proc_dir_entry *parent;
	struct dir_node *c, *dir_node;

	*tag = 0;
	for (subdir = 0, parent = pnode->dir; parent;) {
		dir_node = NULL;
		list_for_each_entry(c, &ctrl->dir_list, entry) {
			if (c->dir == parent) {
				dir_node = c;
				break;
			}
		}

		if (!dir_node)
			break;

		pr_debug("dir [%s] has %sparent\n", dir_node->name,
			dir_node->parent == ctrl->root ? "no " : "");

		subdir_names[subdir++] = dir_node->name;
		parent = dir_node->parent;
	}

	pr_debug("utag consists of %d subdirs\n", subdir);

	/* apply subdirs in opposite order */
	for (blen = 0, i = subdir - 1; i >= 0; i--)
		blen += scnprintf(tag + blen, MAX_UTAG_NAME - blen,
			"%s%s", subdir_names[i], i ? "/" : "");

	if (pnode->type[0] != 0)
		/* top off with type strings */
		blen += scnprintf(tag + blen, MAX_UTAG_NAME - blen,
			":%s", pnode->type);

	pr_debug("full name [%s](%d) has %d subdirs\n", tag, blen, subdir);

	return blen;
}


static int replace_first_utag(struct utag *head, char *name,
		void *payload, size_t size)
{
	struct utag *utag;
	void *oldpayload;

	/* search for the first occurrence of specified type of tag */
	utag = find_first_utag(head, name);
	if (!utag)
		return 0;

	oldpayload = utag->payload;
	if (utag->flags & UTAG_FLAG_PROTECTED) {
		pr_err("protected utag %s\n", name);
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

static int store_utags(struct ctrl *ctrl, struct utag *tags)
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

	pr_debug("[%s] utags partition blk_sz=%zu\n", ctrl->dir_name, cb->size);

	datap = freeze_tags(cb->size, tags, &tags_size);
	if (!datap) {
		rc = -EIO;
		goto out;
	}

	written = fp->f_op->write(fp, datap, tags_size, &pos);
	if (written < tags_size) {
		pr_err("failed to write file (%s), rc=%zu\n",
			cb->name, written);
		rc = -EIO;
	}

	fp = ctrl->backup.filep;
	cb = &ctrl->backup;
	pos = 0;

	if (fp) {
		written = fp->f_op->write(fp, datap, tags_size, &pos);
		if (written < tags_size) {
			pr_err("failed to write file (%s), rc=%zu\n",
				cb->name, written);
			rc = -EIO;
		}
	}
	vfree(datap);

 out:
	set_fs(fs);
	return rc;
}

static int read_utag(struct seq_file *file, void *v)
{
	int i, error;
	char utag_name[MAX_UTAG_NAME];
	uint8_t *ptr;
	struct utag *tags = NULL;
	struct utag *tag = NULL;
	struct proc_node *proc = (struct proc_node *)file->private;
	struct ctrl *ctrl = proc->ctrl;

	tags = load_utags(&ctrl->main);
	if (NULL == tags) {
		pr_err("load config error\n");
		return -EFAULT;
	}

	/* traverse back all parent directories up to root */
	error = full_utag_name(proc, utag_name);
	if (!error) {
		seq_puts(file, "cannot find utag associated with this file\n");
		goto free_tags_exit;
	}

	tag = find_first_utag(tags, utag_name);
	if (NULL == tag) {
		seq_printf(file, "utag [%s] not found\n", utag_name);
		goto free_tags_exit;
	}

	switch (proc->mode) {
	case OUT_ASCII:
		seq_printf(file, "%s", (char *)tag->payload);
		break;
	case OUT_RAW:
		ptr = (uint8_t *) tag->payload;
		for (i = 0; i < tag->size; i++)
			seq_printf(file, "%02X", *(ptr + i));
		break;
	case OUT_TYPE:
		if (*(char *)proc->type != 0)
			seq_printf(file, "%s", (char *)proc->type);
		break;
	}
	seq_puts(file, "\n");

free_tags_exit:
	free_tags(tags);
	return 0;
}

static ssize_t write_utag(struct file *file, const char __user *buffer,
	   size_t count, loff_t *pos)
{
	int error;
	char *payload, utag[MAX_UTAG_NAME];
	struct utag *tags = NULL;
	struct inode *inode = file->f_dentry->d_inode;
	struct proc_node *proc = PDE_DATA(inode);
	struct ctrl *ctrl = proc->ctrl;

	pr_debug("%zu bytes write attempt to [%s](%d)\n",
					count, proc->name, proc->mode);
	if (OUT_TYPE == proc->mode) {
		return count;
	}

	if (MAX_UTAG_SIZE < count) {
		pr_err("error utag too big %zu\n", count);
		return count;
	}

	payload = kzalloc(count, GFP_KERNEL);
	if (!payload)
		return -ENOMEM;

	if (copy_from_user(payload, buffer, count)) {
		pr_err("user copy error\n");
		goto free_temp_exit;
	}

	tags = load_utags(&ctrl->main);
	if (NULL == tags) {
		pr_err("load config error\n");
		goto free_temp_exit;
	}

	/* traverse back all parent directories up to root */
	error = full_utag_name(proc, utag);
	if (!error) {
		pr_err("cannot find utag associated with this file\n");
		goto free_tags_exit;
	}

	error = replace_first_utag(tags, utag, payload, (count - 1));
	if (error) {
		pr_err("error storing [%s] new payload\n", utag);
		goto free_tags_exit;
	}

	error = store_utags(ctrl, tags);
	if (error)
		pr_err("error storing utags partition\n");
free_tags_exit:
	free_tags(tags);
free_temp_exit:
	kfree(payload);
	return count;
}

static void rebuild_utags_directory(struct ctrl *ctrl)
{
	clear_utags_directory(ctrl);
	build_utags_directory(ctrl);
}

/*
 * Process delete file request. Check for existing utag,
 * delete it, save utags, update proc fs
*/

static ssize_t delete_utag(struct file *file, const char __user *buffer,
	   size_t count, loff_t *pos)
{
	int error;
	char *pattern, expendable[MAX_UTAG_NAME];
	struct utag *tags, *cur, *next;
	struct inode *inode = file->f_dentry->d_inode;
	struct ctrl *ctrl = PDE_DATA(inode);

	if ((MAX_UTAG_NAME < count) || (0 == count)) {
		pr_err("invalid utag name %zu\n", count);
		return -EIO;
	}

	if (copy_from_user(expendable, buffer, count)) {
		pr_err("user copy error\n");
		return -EFAULT;
	}

	/* payload has input string plus \n. Replace \n with \0 */
	expendable[count-1] = 0;
	if (!validate_name(expendable, count-1)) {
		pr_err("invalid format %s\n", expendable);
		return count;
	}

	if (check_path(expendable, count-1))
		return count;

	tags = load_utags(&ctrl->main);
	if (NULL == tags) {
		pr_err("load config error\n");
		return count;
	}

	cur = find_first_utag(tags, expendable);
	if (!cur) {
		pr_err("cannot find utag %s\n", expendable);
		goto just_leave;
	}

	/* update pointers */
	cur->prev->next = cur->next;
	cur->next->prev = cur->prev;
	kfree(cur);
	pr_debug("deleted utag [%s]\n", expendable);

	/* remove all utags beneath */
	for (cur = tags->next; cur->next;) {
		pattern = strnstr(cur->name, expendable, MAX_UTAG_NAME);
		if (pattern == cur->name) {
			pr_debug("deleting utag [%s]\n", cur->name);
			next = cur->next;
			cur->prev->next = cur->next;
			cur->next->prev = cur->prev;
			kfree(cur);
			cur = next;
			continue;
		}
		cur = cur->next;
	}

	/* Store changed partition */
	error = store_utags(ctrl, tags);
	if (error)
		pr_err("error storing utags partition\n");

	queue_work(system_wq, &ctrl->delete_work);

just_leave:
	free_tags(tags);
	return count;
}

static int lock_show(struct seq_file *file, void *v)
{
	struct ctrl *ctrl = (struct ctrl *)file->private;

	if (!ctrl)
		pr_err("no control data set\n");
	else
		seq_printf(file, "%u\n", ctrl->lock);
	return 0;
}

static ssize_t lock_store(struct file *file, const char __user *buffer,
	   size_t count, loff_t *pos)
{
	struct inode *inode = file->f_dentry->d_inode;
	struct ctrl *ctrl = PDE_DATA(inode);
	char tag[MAX_UTAG_NAME];

	if (MAX_UTAG_NAME < count) {
		pr_err("invalid parameter length %zu\n", count);
		return -EIO;
	}

	if (!ctrl || copy_from_user(tag, buffer, count)) {
		pr_err("no control data set or user copy error\n");
		return -EFAULT;
	}

	if (!strncasecmp(tag, "lock", 4))
		ctrl->lock = 1;
	else if (!strncasecmp(tag, "unlock", 6))
		ctrl->lock = 0;

	pr_info("partition %s lock %d\n", ctrl->main.name, ctrl->lock);
	return count;
}

/*
 * Process new file request. Check for existing utag,
 * add empty new utag, save utags and add file interface
*/

static ssize_t new_utag(struct file *file, const char __user *buffer,
	   size_t count, loff_t *pos)
{
	struct inode *inode = file->f_dentry->d_inode;
	struct ctrl *ctrl = PDE_DATA(inode);
	struct utag *tags, *cur;
	struct proc_dir_entry *parent = NULL;
	char tag[MAX_UTAG_NAME];
	char expendable[MAX_UTAG_NAME], *names[UTAG_DEPTH], *type = NULL;
	int error, i, num_names;

	if ((MAX_UTAG_NAME < count) || (0 == count)) {
		pr_err("invalid utag name %zu\n", count);
		return -EIO;
	}

	if (copy_from_user(expendable, buffer, count)) {
		pr_err("user copy error\n");
		return -EFAULT;
	}
	/* payload has input string plus \n. Replace \n with \0 */
	expendable[count-1] = 0;
	if (!validate_name(expendable, count-1)) {
		pr_err("invalid format %s\n", expendable);
		return -EFAULT;
	}

	if (check_path(expendable, count-1))
		return count;

	tags = load_utags(&ctrl->main);
	if (NULL == tags) {
		pr_err("load config error\n");
		return -EFAULT;
	}

	/* Ignore request if utag name already in use */
	cur = find_first_utag(tags, expendable);
	if (NULL != cur) {
		pr_err("cannot create [%s]; already in use\n", expendable);
		goto just_leave;
	}

	num_names = full_split(expendable, names, &type);
	for (i = 0, tag[0] = 0; i < num_names; i++) {
		/* build name for each sublevel and add */
		/* slash to all sublevels except first */
		scnprintf(tag + strlen(tag), MAX_UTAG_NAME - strlen(tag),
			"%s%s", i ? "/" : "", names[i]);
		pr_debug("creating subdir %s as %s\n", names[i], tag);

		error = add_utag_tail(tags, tag, type);
		if (error == -EEXIST) {
			struct dir_node *dnode;
			/* need to update parent to ensure proper hierarchy */
			dnode = find_dir_node(ctrl, tag);
			parent = dnode->dir;
			continue;
		}

		/* create utag dir for every part of utag name */
		parent = proc_utag_dir(ctrl, names[i], tag, type, true, parent);
		if (IS_ERR(parent))
			break;
	}

	walk_dir_nodes(ctrl);
	walk_proc_nodes(ctrl);

	/* Store changed partition */
	error = store_utags(ctrl, tags);
	if (error)
		pr_err("error storing utags partition\n");
just_leave:
	free_tags(tags);
	return count;
}

static int reload_show(struct seq_file *file, void *v)
{
	struct ctrl *ctrl = (struct ctrl *)file->private;

	if (!ctrl)
		pr_err("no control data set\n");
	else
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
		pr_err("invalid command length\n");
		return -EIO;
	}

	if (copy_from_user(&ctrl->reload, buffer, 1)) {
		pr_err("user copy error\n");
		return -EFAULT;
	}

	if (UTAG_STATUS_RELOAD == ctrl->reload) {
		if (open_utags(&ctrl->main))
			return count;
		open_utags(&ctrl->backup);
		rebuild_utags_directory(ctrl);
	}

	return count;
}

static int lock_open(struct inode *inode, struct file *file)
{
	return single_open(file, lock_show, PDE_DATA(inode));
}

static int reload_open(struct inode *inode, struct file *file)
{
	return single_open(file, reload_show, PDE_DATA(inode));
}

static int partition_open(struct inode *inode, struct file *file)
{
	return single_open(file, read_utag, PDE_DATA(inode));
}

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
	struct proc_dir_entry *parent;
	struct utag *tags, *cur;

	/* try to load utags from primary partition */
	tags = load_utags(&ctrl->main);
	if (NULL == tags) {
		pr_err("load config error\n");
		return;
	}
	/* skip utags head */
	cur = tags->next;
	while (1) {
		int i, num_names;
		char expendable[MAX_UTAG_NAME];
		char *type, *names[UTAG_DEPTH];
		char path[MAX_UTAG_NAME];

		memset(path, 0, MAX_UTAG_NAME);

		/* skip utags tail */
		if (cur->next == NULL)
			break;

		memcpy(expendable, cur->name, MAX_UTAG_NAME);
		parent = NULL, type = NULL;
		num_names = full_split(expendable, names, &type);

		for (i = 0; i < num_names; i++) {
			scnprintf(path + strlen(path),
				MAX_UTAG_NAME - strlen(path),
				"%s%s", i ? "/" : "", names[i]);
			pr_debug("creating subdir %s as %s\n", names[i], path);
			parent = proc_utag_dir(ctrl, names[i], path, type,
					/* populate only utag dir */
					(i == (num_names - 1)) ? true : false,
					parent);
			if (IS_ERR(parent))
				goto stop_building_utags;
		}

		cur = cur->next;
	}

stop_building_utags:

	walk_dir_nodes(ctrl);
	walk_proc_nodes(ctrl);

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
		pr_err("storage path not provided\n");
		return -EIO;
	}

	rc = of_property_read_string(node, "mmi,backup-utags",
		&ctrl->backup.name);
	if (rc)
		pr_err("backup storage path not provided\n");

	ctrl->dir_name = DEFAULT_ROOT;
	rc = of_property_read_string(node, "mmi,dir-name", &ctrl->dir_name);
	if (!rc)
		pr_info("utag dir override %s\n", ctrl->dir_name);

	return 0;
}
#else
static int utags_dt_init(struct platform_device *pdev) { return -EINVAL; }
#endif

static void clear_utags_directory(struct ctrl *ctrl)
{
	struct proc_node *node, *s = NULL;
	struct dir_node *dir_node, *c = NULL;

	list_for_each_entry_safe(dir_node, c, &ctrl->dir_list, entry) {
		if (dir_node->parent != ctrl->root)
			continue;
		/* remove whole subtree of first level subdir */
		remove_proc_subtree(dir_node->name, ctrl->root);

		pr_debug("removing subtree [%s]\n", dir_node->name);

		list_del(&dir_node->entry);
		kfree(dir_node);
	}

	/* all subtrees removed; just free memory */
	list_for_each_entry_safe(dir_node, c, &ctrl->dir_list, entry) {
		list_del(&dir_node->entry);
		kfree(dir_node);
	}

	/* nothing left in procfs except reload file; free memory */
	list_for_each_entry_safe(node, s, &ctrl->node_list, entry) {
		list_del(&node->entry);
		kfree(node);
	}
}

static void rebuild_utags_work(struct work_struct *w)
{
	struct ctrl *ctrl = container_of(w, struct ctrl, delete_work);

	rebuild_utags_directory(ctrl);
}

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

	INIT_WORK(&ctrl->delete_work, rebuild_utags_work);

	rc = utags_dt_init(pdev);
	if (rc)
		return -EIO;

	rc = open_utags(&ctrl->main);
	if ((rc == -ENOENT) && (++retry < UTAGS_MAX_DEFERRALS))
		return -EPROBE_DEFER;
	else if (rc)
		pr_err("failed to open, try reload later\n");

	if (!ctrl->backup.name)
		pr_err("backup storage path not provided\n");
	else
		open_utags(&ctrl->backup);

	ctrl->root = proc_mkdir(ctrl->dir_name, NULL);
	if (!ctrl->root) {
		pr_err("Failed to create dir entry\n");
		return -EIO;
	}

	if (!proc_create_data("reload", 0600, ctrl->root, &reload_fops, ctrl)) {
		pr_err("Failed to create reload entry\n");
		remove_proc_subtree(ctrl->dir_name, NULL);
		return -EIO;
	}

	if (proc_utag_util(ctrl)) {
		pr_err("Failed to create util dir\n");
		remove_proc_subtree(ctrl->dir_name, NULL);
		return -EFAULT;
	}

	pr_info("Success\n");
	return 0;
}

static int utags_remove(struct platform_device *pdev)
{
	struct ctrl *ctrl = dev_get_drvdata(&pdev->dev);

	clear_utags_directory(ctrl);
	remove_proc_subtree(ctrl->dir_name, NULL);
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
