/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#ifdef WLAN_OPEN_SOURCE
#include <wlan_hdd_includes.h>
#include <wlan_hdd_wowl.h>

#define MAX_USER_COMMAND_SIZE_WOWL_PATTERN 512

static ssize_t wcnss_wowpattern_write(struct file *file,
               const char __user *buf, size_t count, loff_t *ppos)
{
    hdd_adapter_t *pAdapter = (hdd_adapter_t *)file->private_data;

    char cmd[MAX_USER_COMMAND_SIZE_WOWL_PATTERN];
    char *sptr, *token;
    v_U8_t pattern_idx = 0;
    v_U8_t pattern_offset = 0;
    char *pattern_buf;

    if ((NULL == pAdapter) || (WLAN_HDD_ADAPTER_MAGIC != pAdapter->magic))
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s: Invalid adapter or adapter has invalid magic.",
                   __func__);

        return -EINVAL;
    }

    if (count > MAX_USER_COMMAND_SIZE_WOWL_PATTERN)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: Command length is larger than %d bytes.",
                   __func__, MAX_USER_COMMAND_SIZE_WOWL_PATTERN);

        return -EINVAL;
    }

    /* Get command from user */
    if (copy_from_user(cmd, buf, count))
        return -EFAULT;
    cmd[count] = '\0';
    sptr = cmd;

    /* Get pattern idx */
    token = strsep(&sptr, " ");
    if (!token)
        return -EINVAL;

    if (kstrtou8(token, 0, &pattern_idx))
        return -EINVAL;

    /* Get pattern offset */
    token = strsep(&sptr, " ");

    /* Delete pattern if no further argument */
    if (!token) {
        hdd_del_wowl_ptrn_debugfs(pAdapter, pattern_idx);

        return count;
    }

    if (kstrtou8(token, 0, &pattern_offset))
        return -EINVAL;

    /* Get pattern */
    token = strsep(&sptr, " ");
    if (!token)
        return -EINVAL;

    pattern_buf = token;
    pattern_buf[strlen(pattern_buf) - 1] = '\0';

    hdd_add_wowl_ptrn_debugfs(pAdapter, pattern_idx, pattern_offset,
                              pattern_buf);

    return count;
}

static int wcnss_debugfs_open(struct inode *inode, struct file *file)
{
    if (inode->i_private)
    {
        file->private_data = inode->i_private;
    }

    return 0;
}

static const struct file_operations fops_wowpattern = {
    .write = wcnss_wowpattern_write,
    .open = wcnss_debugfs_open,
    .owner = THIS_MODULE,
    .llseek = default_llseek,
};

VOS_STATUS hdd_debugfs_init(hdd_adapter_t *pAdapter)
{
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    pHddCtx->debugfs_phy = debugfs_create_dir("wlan_wcnss", 0);

    if (NULL == pHddCtx->debugfs_phy)
        return VOS_STATUS_E_FAILURE;

    if (NULL == debugfs_create_file("wow_pattern", S_IRUSR | S_IWUSR,
        pHddCtx->debugfs_phy, pAdapter, &fops_wowpattern))
        return VOS_STATUS_E_FAILURE;

    return VOS_STATUS_SUCCESS;
}

void hdd_debugfs_exit(hdd_context_t *pHddCtx)
{
    debugfs_remove_recursive(pHddCtx->debugfs_phy);
}
#endif //#ifdef WLAN_OPEN_SOURCE

