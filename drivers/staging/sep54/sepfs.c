/*
 * Copyright (C) 2013  Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Special thanks to the author of smackfs, for keeping it simple!
 *
 *	Casey Schaufler <casey@schaufler-ca.com>
 */

#include <linux/seq_file.h>
#include <linux/capability.h>
#include <linux/rculist.h>
#include <linux/kernel.h>
#include <linux/vmalloc.h>
#include <linux/mutex.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/cred.h>
#include <linux/uidgid.h>
#include <linux/sched.h>

#include "sepfs.h"

enum sep_inode {
	SEP_ROOT_INODE	= 2,
	SEP_LOAD	= 3,	/* load policy */
	SEP_LOG         = 4,	/* log file */
};

/* A struct to contain a single known rule for an open or invoke command */
struct sep_rule {
	struct list_head		list;
	u32				cmd_id;   /* command to be invoked */
	uid_t                           uid;	  /* allowed user */
	gid_t                           gid;	  /* allowed group */
};

/* Rules that apply to an individual TA */
struct sep_ta_rules {
	struct list_head                list;
	u8				uuid[16]; /* uuid of the TA */
	struct sep_rule                 open;     /* control open session */
	struct sep_rule                 invoke;   /* control invoke command */
	struct mutex		        ta_rules_mutex;	/* lock for the rules */
};

/* All the rules that apply to the Security Engine */
LIST_HEAD(sep_acl);

/* Create a mutex to protect the policy */
static DEFINE_MUTEX(sep_acl_mutex);

#define UUID_LEN 16
#define TMP_SIZE 8

#define SEP_MAGIC 0x37504553 /* SEP7 */

/**
 * Find the rules for a given TA
 * @uuid The UUID of the TA
 * returns the rule list if it exists or NULL if not found
 */
static struct sep_ta_rules *get_ta_rules(const u8 *uuid)
{
	struct sep_ta_rules *rule;

	list_for_each_entry_rcu(rule, &sep_acl, list) {
		if (memcmp(rule->uuid, uuid, UUID_LEN) == 0)
			return rule;
	}

	return NULL;
}

/**
 * Try to find a policy list for a TA, if it does not exist then create it
 * @uuid the UUID of the TA
 * @rules [out] A pointer to the rules for the TA
 * return 0 on sucess, error otherwise
 */
static int get_create_ta_rules(u8 *uuid, struct sep_ta_rules **rules)
{
	int rc = 0;
	struct sep_ta_rules *tmp_rules;

	mutex_lock(&sep_acl_mutex);

	tmp_rules = get_ta_rules(uuid);
	if (tmp_rules == NULL) {
		/* this is the first rule for this TA */
		tmp_rules = kzalloc(sizeof(struct sep_ta_rules), GFP_KERNEL);
		if (tmp_rules == NULL) {
			rc = -ENOMEM;
			goto out;
		}

		memcpy(tmp_rules->uuid, uuid, UUID_LEN);

		INIT_LIST_HEAD(&tmp_rules->open.list);
		INIT_LIST_HEAD(&tmp_rules->invoke.list);

		mutex_init(&tmp_rules->ta_rules_mutex);

		/* Add to the policy */
		list_add_rcu(&tmp_rules->list, &sep_acl);
	}

	*rules = tmp_rules;

out:
	mutex_unlock(&sep_acl_mutex);
	return rc;
}

/**
 * @brief toByte Convert a Character to its hex int representation
 * @param c To be converted
 * @return the value of the char on success, -1 on error
 */
static inline char to_byte(const char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	else if (c >= 'A' && c <= 'F')
		return (c - 'A') + 10;
	else if (c >= 'a' && c <= 'f')
		return (c - 'a') + 10;
	else
		return -1;
}

/**
 * Parse a rule from the load file and create a rule in the policy for it
 * @data The raw line that has been written to the load file
 * returns 0 on success
 */
static int parse_raw_rule(const char *data)
{
	int rc = -EINVAL;
	struct sep_ta_rules *ta_rules = NULL;
	struct sep_rule *new_rule;
	u8 uuid[UUID_LEN];
	u8 tmp[UUID_LEN * 2 + 1]; /* Hex representation in the file */
	int i;
	char ct;

	new_rule = kzalloc(sizeof(*new_rule), GFP_KERNEL);
	if (new_rule == NULL)
		return -ENOMEM;

	/*
	 * overall rule format is
	 * UUID is in hex representation in the string
	 * UUID[32] command_id(u32) uid(u32) gid(u32)
	 */
	if (sscanf(data, "%32s %d %d %d", tmp, &new_rule->cmd_id,
		   &new_rule->uid, &new_rule->gid) != 4) {
		rc = -EINVAL;
		goto err_out;
	}

	if (strnlen(tmp, UUID_LEN * 2) < 32) {
		rc = -EINVAL;
		goto err_out;
	}

	/* convert the hex string to a byte array */
	for (i = 0; i < UUID_LEN; i++) {
		ct = to_byte(tmp[2 * i]);
		if (ct == -1) {
			rc = -EINVAL;
			goto err_out;
		}
		/* set the first nibble of the byte */
		uuid[i] = ct << 4;

		ct = to_byte(tmp[2 * i + 1]);
		if (ct == -1) {
			rc = -EINVAL;
			goto err_out;
		}

		/* complete the byte */
		uuid[i] |= ct & 0x0F;
	}

	pr_debug("Scanned the rule\n");
	pr_debug("%pUB %d %d %d\n", uuid,
		   new_rule->cmd_id, new_rule->uid, new_rule->gid);

	rc = get_create_ta_rules(uuid, &ta_rules);
	if (rc != 0 || ta_rules == NULL)
		goto err_out;

	mutex_lock(&ta_rules->ta_rules_mutex);

	/* TODO: we should probably check if there is a duplicate rule */
	/* append the rule to the appropriate policy config */
	if (new_rule->cmd_id == RESTRICT_OPEN)
		list_add_rcu(&new_rule->list, &ta_rules->open.list);
	else
		list_add_rcu(&new_rule->list, &ta_rules->invoke.list);

	mutex_unlock(&ta_rules->ta_rules_mutex);
	goto out;

err_out:
	kfree(new_rule);
	pr_err("Error Parsing the rule\n");
out:
	return rc;
}

bool is_permitted(const u8 *uuid, int cmd_id)
{
	struct list_head *pos;
	struct list_head *list;
	struct sep_rule *element;
	struct sep_ta_rules *ta_rule;
	struct group_info *groups_info;
	bool rule_exists = false;

	/* Allow a priviladged process to bypass the MAC checks */
	if (capable(CAP_MAC_OVERRIDE))
		return true;

	ta_rule = get_ta_rules(uuid);
	if (ta_rule == NULL) {
#ifdef DRACONIAN
		/* When enforced a rule must exist that allows access to a
		 * service.  The rules, therefore, are used to relax the default
		 * policy of no access.
		 */
		return false;
#else
		/* By default the policy is permissive, only if a rule exists
		   for a TA will it be enforced to block access */
		pr_debug("Allowed because no rule\n");
		return true;
#endif
	}

	/* determine the suplementary groups of the running process */
	groups_info = get_current_groups();

	if (cmd_id == RESTRICT_OPEN)
		list = &(&ta_rule->open)->list;
	else
		list = &(&ta_rule->invoke)->list;


	list_for_each(pos, list) {
		element = list_entry(pos, struct sep_rule, list);
		if (element->cmd_id == cmd_id) {
			rule_exists = true;
			/* if we have a rule for this command check the perms */
			if (uid_eq(current_euid(), element->uid) ||
			    gid_eq(current_egid(), element->gid) ||
			    groups_search(groups_info, element->gid))
				return true;
		}
	}

#ifdef DRACONIAN
	/* If no rule exists then there is no access allowed */
	return false;
#else
	/* If there is a matching command id in the rule list and we have gotten
	 * this far it means that we are not permitted to access the TA/cmd_id
	 * so return "false".  Otherwise if there is no rule for that command ID
	 * we default to permissive mode and return "true".
	 */
	return (rule_exists == true) ? false : true;
#endif
}

/**
 * Start at the first element of the sep_acl
 * @sf the file being worked on
 */
static void *load_seq_start(struct seq_file *sf, loff_t *pos)
{
	if (sf->index == 0)
		sf->private =  list_first_entry_or_null(&sep_acl,
							struct sep_ta_rules,
							list);

	return sf->private;
}

/**
 * Interate to the next element in the sep_acl list
 * @s the file
 * @v the current element in the sep_acl list
 */
static void *load_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	struct list_head *list = v;

	if (list_is_last(list, &sep_acl)) {
		s->private = NULL;
		return NULL;
	}

	s->private = list->next;
	return list->next;
}

/**
 * Display the entries for a TA's policy
 * @sf the sequence file
 * @entry an element on the sep_acl list
 */
static int load_seq_show(struct seq_file *sf, void *entry)
{
	struct list_head *ta_r_list = entry;
	struct list_head *pos;
	struct sep_rule *element;
	struct sep_ta_rules *ta_rules =
			list_entry(ta_r_list, struct sep_ta_rules, list);

	list_for_each(pos, &(&ta_rules->open)->list) {
		element = list_entry(pos, struct sep_rule, list);
		seq_printf(sf, "%pUB %d %d %d\n", ta_rules->uuid,
			   element->cmd_id, element->uid, element->gid);
	}

	list_for_each(pos, &(&ta_rules->invoke)->list) {
		element = list_entry(pos, struct sep_rule, list);
		seq_printf(sf, "%pUB %d %d %d\n", ta_rules->uuid,
			   element->cmd_id, element->uid, element->gid);
	}

	return 0;
}

/**
 * Stop the sequential access to the file, unused
 */
static void load_seq_stop(struct seq_file *sf, void *entry)
{
}

static const struct seq_operations load_seq_ops = {
	.start = load_seq_start,
	.next  = load_seq_next,
	.show  = load_seq_show,
	.stop  = load_seq_stop,
};

/**
 * The open handler for the load file, when reading use the sequentianl file ops
 * @inode of the file
 * @file file descriptor of the opened file
 */
static int sep_open_load(struct inode *inode, struct file *file)
{
	/* only privilaged processes can interact with the policy */
	/* TODO Android L does not support init with CAP_MAC_ADMIN */
	/*if (!capable(CAP_MAC_ADMIN))
		return -EPERM;
	*/

	return seq_open(file, &load_seq_ops);
}

/**
 * The write handler that is associated with the load file
 * @file file descriptor of the open file
 * @buf the userspace data that has been written to the file
 * @count the amount of data written to the file
 * @ppos the current offset in the file stream
 */
static ssize_t sep_write_load(struct file *file, const char __user *buf,
			      size_t count, loff_t *ppos)
{
	char *data;
	int rc = -EINVAL;
	char *rule;
	int orig_count = count;

	/* only privilaged processes can update the policy */
	/* TODO Android L does not support init with CAP_MAC_ADMIN */
	/* if (capable(CAP_MAC_ADMIN) == false)
		return -EPERM;
	*/

	if (*ppos != 0)
		return -EINVAL;

	/* allow for \0 to stringify the data */
	data = kzalloc(count + 1, GFP_KERNEL);
	if (data == NULL)
		return -ENOMEM;

	if (copy_from_user(data, buf, count) != 0) {
		rc = -EFAULT;
		goto out;
	}

	/* all rules should be line based so only take whole lines */
	while (count > 0 && data[count] != '\n')
		count--;

	data[count] = '\0';

	while (data != NULL) {
		rule = strsep(&data, "\n");
		if (rule == NULL)
			break;

		rc = parse_raw_rule(rule);
		if (rc != 0) /* if one rule is mangled continue */
			pr_err("Failed to read rule\n");
	}

	/* say that we read the whole file */
	rc = orig_count;

out:
	kfree(data);
	return rc;
}

static const struct file_operations sep_load_ops = {
	.open           = sep_open_load,
	.read		= seq_read,
	.llseek         = seq_lseek,
	.write		= sep_write_load,
	.release        = seq_release,
};

/**
 * create the sep file system superblock and add the known entries
 * @sb: the empty superblock
 * @data: unused
 * @silent: unused
 *
 * Returns 0 on success
 */
static int sep_fill_super(struct super_block *sb, void *data, int silent)
{
	int rc;
	struct inode *root_inode;

	static struct tree_descr sep_files[] = {
		[SEP_LOAD] = {
			"load", &sep_load_ops, S_IRUGO|S_IWUSR},
				/* [SEP_LOG] = {
				   "log", &sep_log_ops, S_IRUGO|S_IWUSR},*/
		{""}
	};

	rc = simple_fill_super(sb, SEP_MAGIC, sep_files);
	if (rc != 0) {
		pr_err("%s failed %d while creating inodes\n", __func__, rc);
		return rc;
	}

	/* access the inode to ensure it is created */
	root_inode = sb->s_root->d_inode;

	return 0;
}

/**
 * get the sepfs superblock for mounting
 */
static struct dentry *sep_mount(struct file_system_type *fs_type,
				int flags, const char *dev_name, void *data)
{
	return mount_single(fs_type, flags, data, sep_fill_super);
}

static struct file_system_type sep_fs_type = {
	.name		= "sepfs",
	.mount		= sep_mount,
	.kill_sb	= kill_litter_super,
};

static struct kset *sepfs_kset;

static int sep_init_sysfs(void)
{
	sepfs_kset = kset_create_and_add("sepfs", NULL, fs_kobj);
	if (!sepfs_kset)
		return -ENOMEM;
	return 0;
}

static struct vfsmount *sepfs_mount;

/**
 * initialize and register sepfs
 */
static int __init sep_init_fs(void)
{
	int rc;

	rc = sep_init_sysfs();
	if (rc)
		pr_err("sysfs mountpoint problem.\n");

	rc = register_filesystem(&sep_fs_type);
	if (!rc) {
		pr_err("Mounting\n");
		sepfs_mount = kern_mount(&sep_fs_type);
		if (IS_ERR(sepfs_mount)) {
			pr_err("Failed to mount\n");
			rc = PTR_ERR(sepfs_mount);
			sepfs_mount = NULL;
		}
	}

	return rc;
}

device_initcall(sep_init_fs);
