/*
 * Copyright (c) 2013 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

#if defined(CONFIG_ATH_PROCFS_DIAG_SUPPORT)
#include <linux/module.h>	/* Specifically, a module */
#include <linux/kernel.h>	/* We're doing kernel work */
#include <linux/version.h>	/* We're doing kernel work */
#include <linux/proc_fs.h>	/* Necessary because we use the proc fs */
#include <asm/uaccess.h>	/* for copy_from_user */
#if defined(HIF_PCI)
#include "if_pci.h"
#elif defined(HIF_USB)
#include "if_usb.h"
#elif defined(HIF_SDIO)
#include "if_ath_sdio.h"
#endif
#include "vos_api.h"
#include <adf_os_atomic.h>

#define PROCFS_NAME		"athdiagpfs"
#ifdef MULTI_IF_NAME
#define PROCFS_DIR		"cld" MULTI_IF_NAME
#else
#define PROCFS_DIR		"cld"
#endif

/**
 * This structure hold information about the /proc file
 *
 */
static struct proc_dir_entry *proc_file, *proc_dir;

static void *get_hif_hdl_from_file(struct file *file)
{
#if defined(HIF_PCI)
	struct hif_pci_softc *scn;
#elif defined(HIF_USB)
	struct hif_usb_softc *scn;
#elif defined(HIF_SDIO)
	struct ath_hif_sdio_softc *scn;
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
#if defined(HIF_PCI)
	scn = (struct hif_pci_softc *)PDE_DATA(file_inode(file));
#elif defined(HIF_USB)
	scn = (struct hif_usb_softc *)PDE_DATA(file_inode(file));
#elif defined(HIF_SDIO)
	scn = (struct ath_hif_sdio_softc *)PDE_DATA(file_inode(file));
#endif
#else
#if defined(HIF_PCI)
	scn = (struct hif_pci_softc *)(PDE(file->f_path.dentry->d_inode)->data);
#elif defined(HIF_USB)
	scn = (struct hif_usb_softc *)(PDE(file->f_path.dentry->d_inode)->data);
#elif defined(HIF_SDIO)
	scn = (struct ath_hif_sdio_softc *)(PDE(file->f_path.dentry->d_inode)->data);
#endif
#endif
	return (void*)scn->ol_sc->hif_hdl;
}

static ssize_t ath_procfs_diag_read(struct file *file, char __user *buf,
					size_t count, loff_t *pos)
{
	hif_handle_t            hif_hdl;
	int rv;
	A_UINT8 *read_buffer = NULL;

	read_buffer = (A_UINT8 *)vos_mem_malloc(count);
	if (NULL == read_buffer) {
		pr_debug("%s: vos_mem_alloc failed\n", __func__);
		return -EINVAL;
	}

	hif_hdl = get_hif_hdl_from_file(file);
	pr_debug("rd buff 0x%p cnt %zu offset 0x%x buf 0x%p\n",
			read_buffer,count,
			(int)*pos, buf);

	if ((count == 4) && ((((A_UINT32)(*pos)) & 3) == 0)) {
		/* reading a word? */
		rv = HIFDiagReadAccess(hif_hdl, (A_UINT32)(*pos),
					(A_UINT32 *)read_buffer);
	} else {
		rv = HIFDiagReadMem(hif_hdl, (A_UINT32)(*pos),
					(A_UINT8 *)read_buffer, count);
	}

	if(copy_to_user(buf, read_buffer, count)) {
		vos_mem_free(read_buffer);
		return -EFAULT;
	} else
		vos_mem_free(read_buffer);

	if (rv == 0) {
		return count;
	} else {
		return -EIO;
	}
}

static ssize_t ath_procfs_diag_write(struct file *file, const char __user *buf,
					size_t count, loff_t *pos)
{
	hif_handle_t            hif_hdl;
	int rv;
	A_UINT8 *write_buffer = NULL;

	write_buffer = (A_UINT8 *)vos_mem_malloc(count);
	if (NULL == write_buffer) {
		pr_debug("%s: vos_mem_alloc failed\n", __func__);
		return -EINVAL;
	}
	if(copy_from_user(write_buffer, buf, count)) {
		vos_mem_free(write_buffer);
		return -EFAULT;
	}

	hif_hdl = get_hif_hdl_from_file(file);
	pr_debug("wr buff 0x%p buf 0x%p cnt %zu offset 0x%x value 0x%x\n",
			write_buffer, buf, count,
			(int)*pos, *((A_UINT32 *)write_buffer));

	if ((count == 4) && ((((A_UINT32)(*pos)) & 3) == 0)) {
		/* reading a word? */
		A_UINT32 value = *((A_UINT32 *)write_buffer);
		rv = HIFDiagWriteAccess(hif_hdl,
					(A_UINT32)(*pos), value);
	} else {
		rv = HIFDiagWriteMem(hif_hdl, (A_UINT32)(*pos),
					(A_UINT8 *)write_buffer, count);
	}

	vos_mem_free(write_buffer);
	if (rv == 0) {
		return count;
	} else {
		return -EIO;
	}
}

static const struct file_operations athdiag_fops = {
	.read = ath_procfs_diag_read,
	.write = ath_procfs_diag_write,
};

/**
 *This function is called when the module is loaded
 *
 */
int athdiag_procfs_init(void *scn)
{
	proc_dir = proc_mkdir(PROCFS_DIR, NULL);
	if (proc_dir == NULL) {
		remove_proc_entry(PROCFS_DIR, NULL);
		pr_debug("Error: Could not initialize /proc/%s\n",
				PROCFS_DIR);
		return -ENOMEM;
	}

	proc_file = proc_create_data(PROCFS_NAME,
					S_IRUSR | S_IWUSR, proc_dir,
					&athdiag_fops, (void *)scn);
	if (proc_file == NULL) {
		remove_proc_entry(PROCFS_NAME, proc_dir);
		pr_debug("Error: Could not initialize /proc/%s\n",
				PROCFS_NAME);
		return -ENOMEM;
	}

	pr_debug("/proc/%s/%s created\n", PROCFS_DIR, PROCFS_NAME);
	return 0;	/* everything is ok */
}

/**
 *This function is called when the module is unloaded
 *
 */
void athdiag_procfs_remove(void)
{
	if (proc_dir  != NULL) {
	    remove_proc_entry(PROCFS_NAME, proc_dir);
	    pr_debug("/proc/%s/%s removed\n", PROCFS_DIR, PROCFS_NAME);
	    remove_proc_entry(PROCFS_DIR, NULL);
	    pr_debug("/proc/%s removed\n", PROCFS_DIR);
	    proc_dir  = NULL;
        }
}
#endif
