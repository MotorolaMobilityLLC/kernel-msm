/*
 * Copyright (c) 1998-2014 Erez Zadok
 * Copyright (c) 2009	   Shrikar Archak
 * Copyright (c) 2003-2014 Stony Brook University
 * Copyright (c) 2003-2014 The Research Foundation of SUNY
 * Copyright (C) 2013-2014 Motorola Mobility, LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "esdfs.h"
#include <linux/module.h>
#include <linux/parser.h>

/*
 * Derived from first generation "ANDROID_EMU" glue in modifed F2FS driver.
 */
enum {
	Opt_lower_perms,
	Opt_upper_perms,
	Opt_err
};

static match_table_t esdfs_tokens = {
	{Opt_lower_perms, "lower=%s"},
	{Opt_upper_perms, "upper=%s"},
	{Opt_err, NULL},
};

static int parse_perms(struct esdfs_perms *perms, char *args)
{
	char *sep = args;
	char *sepres;
	int ret;

	if (!sep)
		return -EINVAL;

	sepres = strsep(&sep, ":");
	if (!sep)
		return -EINVAL;
	ret = kstrtou32(sepres, 0, &perms->uid);
	if (ret)
		return ret;

	sepres = strsep(&sep, ":");
	if (!sep)
		return -EINVAL;
	ret = kstrtou32(sepres, 0, &perms->gid);
	if (ret)
		return ret;

	sepres = strsep(&sep, ":");
	if (!sep)
		return -EINVAL;
	ret = kstrtou16(sepres, 0, &perms->fmask);
	if (ret)
		return ret;

	sepres = strsep(&sep, ":");
	ret = kstrtou16(sepres, 8, &perms->dmask);
	if (ret)
		return ret;

	return 0;
}

static int parse_options(struct super_block *sb, char *options)
{
	struct esdfs_sb_info *sbi = ESDFS_SB(sb);
	substring_t args[MAX_OPT_ARGS];
	char *p;

	if (!options)
		return 0;

	while ((p = strsep(&options, ",")) != NULL) {
		int token;
		if (!*p)
			continue;
		/*
		 * Initialize args struct so we know whether arg was
		 * found; some options take optional arguments.
		 */
		args[0].to = args[0].from = NULL;
		token = match_token(p, esdfs_tokens, args);

		switch (token) {
		case Opt_lower_perms:
			if (args->from) {
				int ret;
				char *perms = match_strdup(args);

				ret = parse_perms(&sbi->lower_perms, perms);
				kfree(perms);

				if (ret)
					return -EINVAL;
			} else
				return -EINVAL;
			break;
		case Opt_upper_perms:
			if (args->from) {
				int ret;
				char *perms = match_strdup(args);

				ret = parse_perms(&sbi->upper_perms, perms);
				kfree(perms);

				if (ret)
					return -EINVAL;
			} else
				return -EINVAL;
			break;
		default:
			printk(KERN_ERR	"Unrecognized mount option \"%s\" or missing value",
				p);
			return -EINVAL;
		}
	}
	return 0;
}

/*
 * There is no need to lock the esdfs_super_info's rwsem as there is no
 * way anyone can have a reference to the superblock at this point in time.
 */
static int esdfs_read_super(struct super_block *sb, const char *dev_name,
		void *raw_data, int silent)
{
	int err = 0;
	struct super_block *lower_sb;
	struct path lower_path;
	struct inode *inode;

	if (!dev_name) {
		printk(KERN_ERR
		       "esdfs: read_super: missing dev_name argument\n");
		err = -EINVAL;
		goto out;
	}

	/* parse lower path */
	err = kern_path(dev_name, LOOKUP_FOLLOW | LOOKUP_DIRECTORY,
			&lower_path);
	if (err) {
		printk(KERN_ERR	"esdfs: error accessing "
		       "lower directory '%s'\n", dev_name);
		goto out;
	}

	/* allocate superblock private data */
	sb->s_fs_info = kzalloc(sizeof(struct esdfs_sb_info), GFP_KERNEL);
	if (!ESDFS_SB(sb)) {
		printk(KERN_CRIT "esdfs: read_super: out of memory\n");
		err = -ENOMEM;
		goto out_pput;
	}

	/* set defaults and then parse the mount options */
	ESDFS_SB(sb)->lower_perms.uid = ESDFS_DEFAULT_LOWER_UID;
	ESDFS_SB(sb)->lower_perms.gid = ESDFS_DEFAULT_LOWER_GID;
	ESDFS_SB(sb)->lower_perms.fmask = ESDFS_DEFAULT_LOWER_FMASK;
	ESDFS_SB(sb)->lower_perms.dmask = ESDFS_DEFAULT_LOWER_DMASK;
	ESDFS_SB(sb)->upper_perms.uid = ESDFS_DEFAULT_UPPER_UID;
	ESDFS_SB(sb)->upper_perms.gid = ESDFS_DEFAULT_UPPER_GID;
	ESDFS_SB(sb)->upper_perms.fmask = ESDFS_DEFAULT_UPPER_FMASK;
	ESDFS_SB(sb)->upper_perms.dmask = ESDFS_DEFAULT_UPPER_DMASK;
	err = parse_options(sb, (char *)raw_data);
	if (err)
		goto out_free;

	/* set the lower superblock field of upper superblock */
	lower_sb = lower_path.dentry->d_sb;
	atomic_inc(&lower_sb->s_active);
	esdfs_set_lower_super(sb, lower_sb);

	/* inherit maxbytes from lower file system */
	sb->s_maxbytes = lower_sb->s_maxbytes;

	/*
	 * Our c/m/atime granularity is 1 ns because we may stack on file
	 * systems whose granularity is as good.
	 */
	sb->s_time_gran = 1;

	sb->s_op = &esdfs_sops;

	/* get a new inode and allocate our root dentry */
	inode = esdfs_iget(sb, lower_path.dentry->d_inode);
	if (IS_ERR(inode)) {
		err = PTR_ERR(inode);
		goto out_sput;
	}
	sb->s_root = d_make_root(inode);
	if (!sb->s_root) {
		err = -ENOMEM;
		goto out_iput;
	}
	d_set_d_op(sb->s_root, &esdfs_dops);

	/* link the upper and lower dentries */
	sb->s_root->d_fsdata = NULL;
	err = new_dentry_private_data(sb->s_root);
	if (err)
		goto out_freeroot;

	/* if get here: cannot have error */

	/* set the lower dentries for s_root */
	esdfs_set_lower_path(sb->s_root, &lower_path);

	/*
	 * No need to call interpose because we already have a positive
	 * dentry, which was instantiated by d_make_root.  Just need to
	 * d_rehash it.
	 */
	d_rehash(sb->s_root);
	if (!silent)
		printk(KERN_INFO
		       "esdfs: mounted on top of %s type %s\n",
		       dev_name, lower_sb->s_type->name);
	goto out; /* all is well */

	/* no longer needed: free_dentry_private_data(sb->s_root); */
out_freeroot:
	dput(sb->s_root);
out_iput:
	iput(inode);
out_sput:
	/* drop refs we took earlier */
	atomic_dec(&lower_sb->s_active);
out_free:
	kfree(ESDFS_SB(sb));
	sb->s_fs_info = NULL;
out_pput:
	path_put(&lower_path);

out:
	return err;
}

/*
 * Based on mount_nodev() in fs/super.c (need to pass mount options).
 */
struct dentry *esdfs_mount(struct file_system_type *fs_type, int flags,
			    const char *dev_name, void *raw_data)
{
	int error;
	struct super_block *s = sget(fs_type, NULL, set_anon_super, flags, NULL);

	if (IS_ERR(s))
		return ERR_CAST(s);

	s->s_flags = flags;

	error = esdfs_read_super(s, dev_name, raw_data,
					flags & MS_SILENT ? 1 : 0);
	if (error) {
		deactivate_locked_super(s);
		return ERR_PTR(error);
	}
	s->s_flags |= MS_ACTIVE;
	return dget(s->s_root);
}

static struct file_system_type esdfs_fs_type = {
	.owner		= THIS_MODULE,
	.name		= ESDFS_NAME,
	.mount		= esdfs_mount,
	.kill_sb	= generic_shutdown_super,
	.fs_flags	= 0,
};
MODULE_ALIAS_FS(ESDFS_NAME);

static int __init init_esdfs_fs(void)
{
	int err;

	pr_info("Registering esdfs " ESDFS_VERSION "\n");

	err = esdfs_init_inode_cache();
	if (err)
		goto out;
	err = esdfs_init_dentry_cache();
	if (err)
		goto out;
	err = register_filesystem(&esdfs_fs_type);
out:
	if (err) {
		esdfs_destroy_inode_cache();
		esdfs_destroy_dentry_cache();
	}
	return err;
}

static void __exit exit_esdfs_fs(void)
{
	esdfs_destroy_inode_cache();
	esdfs_destroy_dentry_cache();
	unregister_filesystem(&esdfs_fs_type);
	pr_info("Completed esdfs module unload\n");
}

MODULE_AUTHOR("Erez Zadok, Filesystems and Storage Lab, Stony Brook University"
	      " (http://www.fsl.cs.sunysb.edu/), Motorola Mobility, LLC");
MODULE_DESCRIPTION("Emulated 'SD card' Filesystem");
MODULE_LICENSE("GPL");

module_init(init_esdfs_fs);
module_exit(exit_esdfs_fs);
