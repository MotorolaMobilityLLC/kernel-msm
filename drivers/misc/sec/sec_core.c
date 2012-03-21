/*
 * Copyright (C) 2009 Motorola, Inc.
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* Necessary includes for device drivers */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h>
#include <linux/uaccess.h>	/* copy_from/to_user */
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/io.h>
#include <mach/scm.h>
#include "sec_core.h"

static ssize_t sec_read(struct file *filp, char *buf,
		size_t count, loff_t *f_pos);
static int sec_open(struct inode *inode, struct file *filp);
static int sec_release(struct inode *inode, struct file *filp);
static long sec_ioctl(struct file *file, unsigned int ioctl_num,
		unsigned long ioctl_param);
static int sec_init(void);
static void sec_exit(void);

#define TZBSP_SVC_OEM 254

/* Structure that declares the usual file */
/* access functions */
const struct file_operations sec_fops = {
	.read = sec_read,
	.open = sec_open,
	.release = sec_release,
	.unlocked_ioctl = sec_ioctl
};

/* Mapping of the module init and exit functions */
module_init(sec_init);
module_exit(sec_exit);

static struct miscdevice sec_dev = {
	MISC_DYNAMIC_MINOR,
	"sec",
	&sec_fops
};

/****************************************************************************/
/*   KERNEL DRIVER APIs, ONLY IOCTL RESPONDS BACK TO USER SPACE REQUESTS    */
/****************************************************************************/
int sec_init(void)
{
	int result;

	result = misc_register(&sec_dev);

	if (result) {
		printk(KERN_ERR "sec: cannot obtain major number\n");
		return result;
	}

	printk(KERN_INFO "Inserting sec module\n");
	return 0;
}

void sec_exit(void)
{
	/* Freeing the major number */
	misc_deregister(&sec_dev);
}

int sec_open(struct inode *inode, struct file *filp)
{
	/* Not supported, return Success */
	return 0;
}

int sec_release(struct inode *inode, struct file *filp)
{

	/* Not supported, return Success */
	return 0;
}

ssize_t sec_read(struct file *filp, char *buf,
		 size_t count, loff_t *f_pos)
{
	/* Not supported, return Success */
	return 0;
}

long sec_ioctl(struct file *file, unsigned int ioctl_num,
		unsigned long ioctl_param)
{
	struct {
		int mot_cmd;
		int parm1;
		int parm2;
		int parm3;
	} my_cmd;

	struct SEC_EFUSE_PARM_T efuse_data;

	u32 *shared_mem;
	long ret_val = SEC_KM_FAIL;

	switch (ioctl_num) {

	case SEC_IOCTL_READ_PROC_ID:

		shared_mem = kmalloc(SEC_PROC_ID_SIZE, GFP_KERNEL);
		my_cmd.mot_cmd = 10;
		my_cmd.parm1 = virt_to_phys(shared_mem);
		my_cmd.parm2 = SEC_PROC_ID_SIZE;

		if (scm_call(TZBSP_SVC_OEM, 1, &my_cmd,
			sizeof(my_cmd), NULL, 0) == 0) {

			if (copy_to_user((void __user *)ioctl_param,
			(const void *) shared_mem, SEC_PROC_ID_SIZE) == 0) {

				ret_val = SEC_KM_SUCCESS;
			}
		}

		kfree(shared_mem);
		break;

	case SEC_IOCTL_WRITE_FUSE:

		if (copy_from_user(&efuse_data, (void __user *)
			ioctl_param, sizeof(efuse_data)) != 0) {

			break;
		}

		my_cmd.mot_cmd = 2;
		my_cmd.parm1 = efuse_data.which_bank;
		my_cmd.parm2 = efuse_data.efuse_value;

		if (scm_call(TZBSP_SVC_OEM, 1, &my_cmd,
			sizeof(my_cmd), NULL, 0) == 0) {

			ret_val = SEC_KM_SUCCESS;
		}

		break;

	case SEC_IOCTL_READ_FUSE:

		if (copy_from_user(&efuse_data,
			(void *)ioctl_param, sizeof(efuse_data)) != 0) {

			break;
		}

		shared_mem = kmalloc(4, GFP_KERNEL);
		my_cmd.mot_cmd = 1;
		my_cmd.parm1 = efuse_data.which_bank;
		my_cmd.parm2 = virt_to_phys(shared_mem);

		if (scm_call(TZBSP_SVC_OEM, 1, &my_cmd,
			sizeof(my_cmd), NULL, 0) == 0) {

			efuse_data.efuse_value = *shared_mem;

			if (copy_to_user((void *)ioctl_param,
				&efuse_data, sizeof(efuse_data)) == 0) {

				ret_val = SEC_KM_SUCCESS;
			}
		}

		kfree(shared_mem);
		break;

	case SEC_IOCTL_GET_TZ_VERSION:

		shared_mem = kmalloc(4, GFP_KERNEL);
		my_cmd.mot_cmd = 11;
		my_cmd.parm1 = virt_to_phys(shared_mem);

		if (scm_call(TZBSP_SVC_OEM, 1, &my_cmd,
			sizeof(my_cmd), NULL, 0) == 0) {

			if (copy_to_user((void *)ioctl_param,
				shared_mem, 4) == 0) {

				ret_val = SEC_KM_SUCCESS;
			}
		}

		kfree(shared_mem);

	default:
		break;
	}

	return ret_val;
}

/****************************************************************************/
/*Kernel Module License Information                                         */
/****************************************************************************/
MODULE_LICENSE("GPL");
MODULE_AUTHOR("MOTOROLA");
