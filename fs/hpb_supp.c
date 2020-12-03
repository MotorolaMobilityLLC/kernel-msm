/*
 *  Copyright (C) 2019 Samsung Electronics Co., Ltd.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/hashtable.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/ctype.h>
#include <linux/fs_hpb.h>
#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/rwsem.h>

struct kobject *fs_hpb_kobj;
EXPORT_SYMBOL(fs_hpb_kobj);

struct hpb_ext {
	struct hlist_node hlist;
	const char *ext;
	bool def_ext;
};
DECLARE_HASHTABLE(hpb_ext_hash_tbl, 5);
struct rw_semaphore hpb_ext_hash_tbl_lock;

static char* DEF_HPB_extension[] = {
	"so",
	"jar",
	"apk",
	"obb",
	"apex",
	"dex",
	"odex",
	"vdex",
	"art",
	"oat",
	"ttf",
	"ttc",
	"otf",
	"relro",
	"realm",
	"db",
	"db-wal",
	"db-shm",
	"db-journal",
};
#define NR_DEF_HPB_EXT (sizeof(DEF_HPB_extension) / sizeof(*DEF_HPB_extension))
#define __full_name_hash(name)	full_name_hash((void *)0x0, name, strlen(name))

struct fs_hpb_attr {
	struct kobj_attribute attr;
};

#define FS_HPB_RO_ATTR(name, func) \
static struct fs_hpb_attr fs_hpb_attr_##name = { \
	.attr = __ATTR(name, 0440, func, NULL), \
}

#define FS_HPB_WO_ATTR(name, func) \
static struct fs_hpb_attr fs_hpb_attr_##name = { \
	.attr = __ATTR(name, 0640, NULL, func), \
}

#define FS_HPB_GENERAL_RW_ATTR(name) \
static struct fs_hpb_attr fs_hpb_attr_##name = { \
	.attr = __ATTR(name, 0640, fs_hpb_generic_show, fs_hpb_generic_store), \
}

static struct hpb_ext* is_ext_in_list(const char *ext)
{
	struct hpb_ext *entry, *ret = NULL;
	unsigned hash;

	hash = __full_name_hash(ext);
	down_read(&hpb_ext_hash_tbl_lock);
	hash_for_each_possible(hpb_ext_hash_tbl, entry, hlist, hash) {
		if (!strcmp(ext, entry->ext)) {
			ret = entry;
			break;
		}
	}
	up_read(&hpb_ext_hash_tbl_lock);

	return ret;
}

bool hpb_ext_in_list(const char* filename, const char token) {
	char *ext = strrchr(filename, token);
	struct hpb_ext *entry;
	unsigned hash;
	int i;
	bool ret = false;

	if (!ext || !ext[1])
		goto final;

	ext = kstrdup(&ext[1], GFP_KERNEL);
	if (!ext)
		goto final;

	for (i = 0; ext[i]; i++)
		ext[i] = tolower(ext[i]);

	hash = __full_name_hash(ext);

	hash_for_each_possible(hpb_ext_hash_tbl, entry, hlist, hash) {
		if (!strcmp(ext, entry->ext)) {
			ret = true;
			break;
		}
	}
	kfree(ext);

final:
	return ret;
}
EXPORT_SYMBOL(hpb_ext_in_list);

static int add_to_hash(const char *ext, bool def_ext) {
	struct hpb_ext *entry;
	entry = kzalloc(sizeof(struct hpb_ext), GFP_KERNEL);
	if (!entry)
		return -ENOMEM;

	INIT_HLIST_NODE(&entry->hlist);
	if (def_ext)
		entry->ext = ext;
	else
		entry->ext = kstrdup(ext, GFP_KERNEL);

	entry->def_ext = def_ext;
	down_write(&hpb_ext_hash_tbl_lock);
	hash_add(hpb_ext_hash_tbl, &entry->hlist,
		 __full_name_hash(entry->ext));
	up_write(&hpb_ext_hash_tbl_lock);

	return 0;
}


static inline void del_from_hash(struct hpb_ext *entry, bool need_lock) {

	if (need_lock)
		down_write(&hpb_ext_hash_tbl_lock);

	hash_del(&entry->hlist);

	if (need_lock)
		up_write(&hpb_ext_hash_tbl_lock);

	if (!entry->def_ext)
		kfree(entry->ext);

	kfree(entry);
}

static ssize_t fs_hpb_generic_show(struct kobject *kobj,
				   struct kobj_attribute *attr, char *buf)
{
	//struct fs_hpb_attr *entry = \
	//			container_of(attr, struct fs_hpb_attr, attr);
	struct hpb_ext *ext_entry;
	int offs = 0;
	int i;

	if (!strcmp(attr->attr.name, "hpb_extension")) {
		down_read(&hpb_ext_hash_tbl_lock);
		hash_for_each(hpb_ext_hash_tbl, i, ext_entry, hlist) {
			offs += scnprintf(buf + offs, PAGE_SIZE, "%s) %s\n",
					  ext_entry->def_ext? "Def":"Usr", ext_entry->ext);
		}
		up_read(&hpb_ext_hash_tbl_lock);
	}

	return offs;
}

static ssize_t fs_hpb_generic_store(struct kobject *kobj,
				    struct kobj_attribute *attr, const char *buf, size_t len)
{
	//struct fs_hpb_attr *entry = \
	//			    container_of(attr, struct fs_hpb_attr, attr);
	struct hpb_ext *ext_entry;

	if (!strcmp(attr->attr.name, "hpb_extension")) {
		ext_entry = is_ext_in_list(buf);
		if (ext_entry) {
			del_from_hash(ext_entry, true);
		} else {
			add_to_hash(buf, false);
		}
	}

	return len;
}

FS_HPB_GENERAL_RW_ATTR(hpb_extension);

#define ATTR_LIST(name) (&fs_hpb_attr_##name.attr.attr)
static struct attribute *fs_hpb_attrs[] = {
	ATTR_LIST(hpb_extension),
	NULL,
};
static struct attribute_group fs_hpb_group = {
	.attrs = fs_hpb_attrs,
};

int __init fs_hpb_support_init(void)
{
	int i;
	int ret;

	if(fs_kobj) {
		fs_hpb_kobj = kobject_create_and_add("hpb", fs_kobj);
		if(!fs_hpb_kobj) {
			printk(KERN_WARNING "%s: error on creating hpb kobj\n",
			       __func__);
			return -EAGAIN;
		}
	}

	// Create node to add/delete extension
	ret = sysfs_create_group(fs_hpb_kobj, &fs_hpb_group);
	if (ret) {
		printk("%s: sysfs_create_group() failed. ret=%d\n",
		       __func__, ret);
		return ret;
	}

	hash_init(hpb_ext_hash_tbl);
	init_rwsem(&hpb_ext_hash_tbl_lock);

	for (i = 0; i < NR_DEF_HPB_EXT; i++) {
		ret = add_to_hash(DEF_HPB_extension[i], true);
		if (ret)
			return ret;
	}

	return 0;
}
module_init(fs_hpb_support_init);

void __exit fs_hpb_support_exit(void)
{
	int i;
	struct hlist_node *tmp;
	struct hpb_ext *entry;

	down_write(&hpb_ext_hash_tbl_lock);
	hash_for_each_safe(hpb_ext_hash_tbl, i, tmp, entry, hlist) {
		del_from_hash(entry, false);
	}
	up_write(&hpb_ext_hash_tbl_lock);

	if(fs_hpb_kobj) {
		sysfs_remove_group(fs_hpb_kobj, &fs_hpb_group);
		kobject_del(fs_hpb_kobj);
	}
}
module_exit(fs_hpb_support_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("HPB Support for Filesystem");
MODULE_AUTHOR("Samsung Electronics Co., Ltd.");
