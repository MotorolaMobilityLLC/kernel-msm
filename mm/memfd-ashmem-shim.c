// SPDX-License-Identifier: GPL-2.0

/*
 * Ashmem compatability for memfd
 *
 * Copyright (c) 2025, Google LLC.
 * Author: Isaac J. Manjarres <isaacmanjarres@google.com>
 */

#include <asm-generic/mman-common.h>
#include <linux/capability.h>
#include <linux/fs.h>
#include <linux/memfd.h>
#include <linux/uaccess.h>

#include "memfd-ashmem-shim.h"
#include "memfd-ashmem-shim-internal.h"

/* file_path() returns the path of the file including the root, hence the additional "/". */
#define MEMFD_PATH_PREFIX "/memfd:"
#define MEMFD_PATH_PREFIX_LEN (sizeof(MEMFD_PATH_PREFIX) - 1)

/* All memfd files are unlinked, and are therefore suffixed with the " (deleted)" string. */
#define UNLINKED_FILE_SUFFIX " (deleted)"
#define UNLINKED_FILE_SUFFIX_LEN (sizeof(UNLINKED_FILE_SUFFIX) - 1)

/*
 * 1 character for the start of the path (/), NAME_MAX for the maximum length of a full memfd file
 * name, UNLINKED_FILE_SUFFIX_LEN for the " (deleted)" suffix, and 1 for the NUL terminating
 * character.
 */
#define MAX_FILE_PATH_SIZE (1 + NAME_MAX + UNLINKED_FILE_SUFFIX_LEN + 1)

static char *get_memfd_file_name(struct file *file, char *buf, size_t size)
{
	char *name_end;
	char *path = file_path(file, buf, size);

	if (IS_ERR(path))
		return path;

	/* Only handle memfds; we cannot make assumptions about other file names. */
	name_end = strstr(path, UNLINKED_FILE_SUFFIX);
	if ((strstr(path, MEMFD_PATH_PREFIX) != path) || !name_end)
		return ERR_PTR(-EINVAL);

	/*
	 * Since file_path() returns the full path of the file, including the root, the format will
	 * be:
	 *
	 * "/memfd:testbuf (deleted)"
	 *
	 * But the ASHMEM_GET_NAME ioctl only returns the name of the buffer without any prefixes
	 * or suffixes. So, terminate the string at the start of the " (deleted)" suffix so that
	 * strlen() can be used on it from the start of the name.
	 */
	*name_end = '\0';

	/* return a pointer to the start of the name */
	return &path[MEMFD_PATH_PREFIX_LEN];
}

static long get_name(struct file *file, void __user *name)
{
	char buf[MAX_FILE_PATH_SIZE];
	char *file_name = get_memfd_file_name(file, buf, sizeof(buf));
	size_t len;

	if (IS_ERR(file_name))
		return PTR_ERR(file_name);

	/*
	 * The expectation is that the user provided buffer is ASHMEM_NAME_LEN in size, which is
	 * larger than the maximum size of a name for a memfd buffer, so the name should always fit
	 * within the given buffer.
	 *
	 * However, we should ensure that the string will indeed fit in the user provided buffer.
	 *
	 * Add 1 to the copy size to account for the NUL terminator
	 */
	len = strlen(file_name) + 1;
	if (len > ASHMEM_NAME_LEN)
		return -EINVAL;

	return copy_to_user(name, file_name, len) ? -EFAULT : 0;
}

static long get_prot_mask(struct file *file)
{
	long prot_mask = PROT_READ | PROT_EXEC;
	long seals = memfd_fcntl(file, F_GET_SEALS, 0);

	if (seals < 0)
		return seals;

	/* memfds are readable and executable by default. Only writability can be changed. */
	if (!(seals & (F_SEAL_WRITE | F_SEAL_FUTURE_WRITE)))
		prot_mask |= PROT_WRITE;

	return prot_mask;
}

static long set_prot_mask(struct file *file, unsigned long prot)
{
	long curr_prot = get_prot_mask(file);
	long ret = 0;

	if (curr_prot < 0)
		return curr_prot;

	/*
	 * memfds are always readable and executable; there is no way to remove either mapping
	 * permission, nor is there a known usecase that requires it.
	 *
	 * Attempting to remove either of these mapping permissions will return successfully, but
	 * will be a nop, as the buffer will still be mappable with these permissions.
	 */
	prot |= PROT_READ | PROT_EXEC;

	/* Only allow permissions to be removed. */
	if ((curr_prot & prot) != prot)
		return -EINVAL;

	/*
	 * Removing PROT_WRITE:
	 *
	 * We could prevent any other mappings from having write permissions by adding the
	 * F_SEAL_WRITE mapping. However, that would conflict with known usecases where it is
	 * desirable to maintain an existing writable mapping, but forbid future writable mappings.
	 *
	 * To support those usecases, we use F_SEAL_FUTURE_WRITE.
	 */
	if (!(prot & PROT_WRITE))
		ret = memfd_fcntl(file, F_ADD_SEALS, F_SEAL_FUTURE_WRITE);

	return ret;
}

/*
 * memfd_ashmem_shim_ioctl - ioctl handler for ashmem commands
 * @file: The shmem file.
 * @cmd: The ioctl command.
 * @arg: The argument for the ioctl command.
 *
 * The purpose of this handler is to allow old applications to continue working
 * on newer kernels by allowing them to invoke ashmem ioctl commands on memfds.
 *
 * The ioctl handler attempts to retain as much compatibility with the ashmem
 * driver as possible.
 */
long memfd_ashmem_shim_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = -ENOTTY;
	unsigned long inode_nr;

	switch (cmd) {
	/*
	 * Older applications won't create memfds and try to use ASHMEM_SET_NAME/ASHMEM_SET_SIZE on
	 * them intentionally.
	 *
	 * Instead, we can end up in this scenario if an old application receives a memfd that was
	 * created by another process.
	 *
	 * However, the current process shouldn't expect to be able to reliably [re]name/size a
	 * buffer that was shared with it, since the process that shared that buffer with it, or
	 * any other process that references the buffer could have already mapped it.
	 *
	 * Additionally in the case of ASHMEM_SET_SIZE, when processes create memfds that are going
	 * to be shared with other processes in Android, they also specify the size of the memory
	 * region and seal the file against any size changes. Therefore, ASHMEM_SET_SIZE should not
	 * be supported anyway.
	 *
	 * Therefore, it is reasonable to return -EINVAL here, as if the buffer was already mapped.
	 */
	case ASHMEM_SET_NAME:
	case ASHMEM_SET_SIZE:
		ret = -EINVAL;
		break;
	case ASHMEM_GET_NAME:
		ret = get_name(file, (void __user *)arg);
		break;
	case ASHMEM_GET_SIZE:
		ret = i_size_read(file_inode(file));
		break;
	case ASHMEM_SET_PROT_MASK:
		ret = set_prot_mask(file, arg);
		break;
	case ASHMEM_GET_PROT_MASK:
		ret = get_prot_mask(file);
		break;
	/*
	 * Unpinning ashmem buffers was deprecated with the release of Android 10,
	 * as it did not yield any remarkable benefits. Therefore, ignore pinning
	 * related requests.
	 *
	 * This makes it so that memory is always "pinned" or never entirely freed
	 * until all references to the ashmem buffer are dropped. The memory occupied
	 * by the buffer is still subject to being reclaimed (swapped out) under memory
	 * pressure, but that is not the same as being freed.
	 *
	 * This makes it so that:
	 *
	 * 1. Memory is always pinned and therefore never purged.
	 * 2. Requests to unpin memory (make it a candidate for being freed) are ignored.
	 */
	case ASHMEM_PIN:
		ret = ASHMEM_NOT_PURGED;
		break;
	case ASHMEM_UNPIN:
		ret = 0;
		break;
	case ASHMEM_GET_PIN_STATUS:
		ret = ASHMEM_IS_PINNED;
		break;
	case ASHMEM_PURGE_ALL_CACHES:
		ret = capable(CAP_SYS_ADMIN) ? 0 : -EPERM;
		break;
	case ASHMEM_GET_FILE_ID:
		inode_nr = file_inode(file)->i_ino;
		if (copy_to_user((void __user *)arg, &inode_nr, sizeof(inode_nr)))
			ret = -EFAULT;
		else
			ret = 0;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
long memfd_ashmem_shim_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	if (cmd == COMPAT_ASHMEM_SET_SIZE)
		cmd = ASHMEM_SET_SIZE;
	else if (cmd == COMPAT_ASHMEM_SET_PROT_MASK)
		cmd = ASHMEM_SET_PROT_MASK;

	return memfd_ashmem_shim_ioctl(file, cmd, arg);
}
#endif
