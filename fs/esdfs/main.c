/*
 * Copyright (c) 1998-2013 Erez Zadok
 * Copyright (c) 2009	   Shrikar Archak
 * Copyright (c) 2003-2013 Stony Brook University
 * Copyright (c) 2003-2013 The Research Foundation of SUNY
 * Copyright (C) 2013-2014 Motorola Mobility, LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "esdfs.h"
#include <linux/module.h>
#include <linux/parser.h>
#include <linux/security.h>

/*
 * Derived from first generation "ANDROID_EMU" glue in modifed F2FS driver.
 */
enum {
	Opt_lower_perms,
	Opt_upper_perms,
	Opt_derive_none,
	Opt_derive_legacy,
	Opt_derive_unified,
	Opt_split,
	Opt_nosplit,
	Opt_err
};

static match_table_t esdfs_tokens = {
	{Opt_lower_perms, "lower=%s"},
	{Opt_upper_perms, "upper=%s"},
	{Opt_derive_none, "derive=none"},
	{Opt_derive_legacy, "derive=legacy"},
	{Opt_derive_unified, "derive=unified"},
	{Opt_split, "split"},
	{Opt_nosplit, "nosplit"},
	{Opt_err, NULL},
};

struct esdfs_perms esdfs_perms_table[ESDFS_PERMS_TABLE_SIZE] = {
	/* ESDFS_PERMS_LOWER_DEFAULT */
	{ .uid   = AID_MEDIA_RW,
	  .gid   = AID_MEDIA_RW,
	  .fmask = 0664,
	  .dmask = 0775 },
	/* ESDFS_PERMS_UPPER_LEGACY */
	{ .uid   = AID_ROOT,
	  .gid   = AID_SDCARD_RW,
	  .fmask = 0664,
	  .dmask = 0775 },
	/* ESDFS_PERMS_UPPER_DERIVED */
	{ .uid   = AID_ROOT,
	  .gid   = AID_SDCARD_R,
	  .fmask = 0660,
	  .dmask = 0771 },
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
		case Opt_derive_none:
			clear_opt(sbi, DERIVE_LEGACY);
			clear_opt(sbi, DERIVE_UNIFIED);
			break;
		case Opt_derive_legacy:
			set_opt(sbi, DERIVE_LEGACY);
			clear_opt(sbi, DERIVE_UNIFIED);
			break;
		case Opt_derive_unified:
			clear_opt(sbi, DERIVE_LEGACY);
			set_opt(sbi, DERIVE_UNIFIED);
			break;
		case Opt_split:
			set_opt(sbi, DERIVE_SPLIT);
			break;
		case Opt_nosplit:
			clear_opt(sbi, DERIVE_SPLIT);
			break;
		default:
			esdfs_msg(sb, KERN_ERR, "unrecognized mount option \"%s\" or missing value\n",
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
	struct esdfs_sb_info *sbi;
	struct inode *inode;

	if (!dev_name) {
		esdfs_msg(sb, KERN_ERR, "missing dev_name argument\n");
		err = -EINVAL;
		goto out;
	}

	/* parse lower path */
	err = kern_path(dev_name, LOOKUP_FOLLOW | LOOKUP_DIRECTORY,
			&lower_path);
	if (err) {
		esdfs_msg(sb, KERN_ERR, "error accessing lower directory '%s'\n",
			dev_name);
		goto out;
	}

	/* allocate superblock private data */
	sb->s_fs_info = kzalloc(sizeof(struct esdfs_sb_info), GFP_KERNEL);
	sbi = ESDFS_SB(sb);
	if (!sbi) {
		esdfs_msg(sb, KERN_CRIT, "read_super: out of memory\n");
		err = -ENOMEM;
		goto out_pput;
	}

	/* set defaults and then parse the mount options */
	memcpy(&sbi->lower_perms,
	       &esdfs_perms_table[ESDFS_PERMS_LOWER_DEFAULT],
	       sizeof(struct esdfs_perms));
	memcpy(&sbi->upper_perms,
	       &esdfs_perms_table[ESDFS_PERMS_UPPER_LEGACY],
	       sizeof(struct esdfs_perms));
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
#ifdef CONFIG_SECURITY_SELINUX
	security_secctx_to_secid(ESDFS_LOWER_SECCTX,
				 strlen(ESDFS_LOWER_SECCTX),
				 &sbi->lower_secid);
#endif
	/*
	 * No need to call interpose because we already have a positive
	 * dentry, which was instantiated by d_make_root.  Just need to
	 * d_rehash it.
	 */
	d_rehash(sb->s_root);
	if (!silent)
		esdfs_msg(sb, KERN_INFO, "mounted on top of %s type %s\n",
			dev_name, lower_sb->s_type->name);

	if (!ESDFS_DERIVE_PERMS(sbi))
		goto out;

	/* let user know that we ignore this option in derived mode */
	if (memcmp(&sbi->upper_perms,
		   &esdfs_perms_table[ESDFS_PERMS_UPPER_LEGACY],
		   sizeof(struct esdfs_perms)))
		esdfs_msg(sb, KERN_WARNING, "'upper' mount option ignored in derived mode\n");

	/* all derived modes start with the same, basic root */
	memcpy(&sbi->upper_perms,
	       &esdfs_perms_table[ESDFS_PERMS_UPPER_DERIVED],
	       sizeof(struct esdfs_perms));

	/*
	 * In Android 3.0 all user conent in the emulated storage tree was
	 * stored in /data/media.  Android 4.2 introduced multi-user support,
	 * which required that the primary user's content be migrated from
	 * /data/media to /data/media/0.  The framework then uses bind mounts
	 * to create per-process namespaces to isolate each user's tree at
	 * /data/media/N.  This approach of having each user in a common root
	 * is now considered "legacy" by the sdcard service.
	 */
	if (test_opt(sbi, DERIVE_LEGACY)) {
		ESDFS_I(inode)->tree = ESDFS_TREE_ROOT_LEGACY;
		sbi->obb_parent = dget(sb->s_root);
	/*
	 * Android 4.4 reorganized this sturcture yet again, so that the
	 * primary user's content was again at the root.  Secondary users'
	 * content is found in Android/user/N.  Emulated internal storage still
	 * seems to use the legacy tree, but secondary external storage uses
	 * this method.
	 */
	} else if (test_opt(sbi, DERIVE_UNIFIED))
		ESDFS_I(inode)->tree = ESDFS_TREE_ROOT;
	/*
	 * Later versions of Android organize user content using quantum
	 * entanglement, which has a low probability of being supported by
	 * this driver.
	 */
	else
		esdfs_msg(sb, KERN_WARNING, "unsupported derived permissions mode\n");

	/* initialize root inode */
	esdfs_derive_perms(sb->s_root);

	goto out;

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
	struct super_block *s = sget(fs_type, NULL, set_anon_super, NULL);

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

static void esdfs_kill_sb(struct super_block *sb)
{
	if (ESDFS_SB(sb)->obb_parent)
		dput(ESDFS_SB(sb)->obb_parent);

	generic_shutdown_super(sb);
}

static struct file_system_type esdfs_fs_type = {
	.owner		= THIS_MODULE,
	.name		= ESDFS_NAME,
	.mount		= esdfs_mount,
	.kill_sb	= esdfs_kill_sb,
	.fs_flags	= FS_REVAL_DOT,
};

static int __init init_esdfs_fs(void)
{
	int err;

	pr_info("Registering esdfs " ESDFS_VERSION "\n");

	esdfs_init_package_list();

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
		esdfs_destroy_package_list();
	}
	return err;
}

static void __exit exit_esdfs_fs(void)
{
	esdfs_destroy_inode_cache();
	esdfs_destroy_dentry_cache();
	esdfs_destroy_package_list();
	unregister_filesystem(&esdfs_fs_type);
	pr_info("Completed esdfs module unload\n");
}

MODULE_AUTHOR("Erez Zadok, Filesystems and Storage Lab, Stony Brook University"
	      " (http://www.fsl.cs.sunysb.edu/)");
MODULE_DESCRIPTION("esdfs " ESDFS_VERSION);
MODULE_LICENSE("GPL");

module_init(init_esdfs_fs);
module_exit(exit_esdfs_fs);
