/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)     "motUtil:[%s]" fmt, __func__

#include <linux/debugfs.h>
#include <linux/string.h>
#include <linux/slab.h>

#include "sde_connector.h"
#include "sde_encoder.h"
#include "sde_motUtil.h"
#include "dsi_display.h"


#define MAIN_DSI_CONN_NAME "DSI-1"
#define CLI_DSI_CONN_NAME "DSI-2"

struct motUtil motUtil_data;

static ssize_t _sde_debugfs_motUtil_string2num(const char __user *buf,
					size_t count, char buffer[])
{
	char *input, *input_copy, *token, *input_dup = NULL;
	const char *delim =" ";
	u32 buf_size = 0;
	int rc=0, strtoint;

	/*copy the data from user space*/
	input = kmalloc(count + 1, GFP_KERNEL);
	if (!input)
		return -ENOMEM;

	if (copy_from_user(input, buf, count)) {
		SDE_ERROR("copy from user failed\n");
		rc = -EFAULT;
		goto end;
	}
	input[count]='\0';
	pr_info("input_cmd = %s\n", input);

	input_copy = kstrdup(input, GFP_KERNEL);
	if (!input_copy) {
		rc = -ENOMEM;
		goto end;
	}

	input_dup = input_copy,
	token = strsep(&input_copy, delim);
	while (token) {
		rc = kstrtoint(token, 0, &strtoint);
		if (rc) {
			SDE_ERROR("input buffer conversion failed\n");
			goto end;
		}

		if (buf_size >= MAX_CMD_PAYLOAD_SIZE) {
			SDE_ERROR("buffer size exceeding the limit %d\n",
					MAX_CMD_PAYLOAD_SIZE);
			goto end;
		}

		buffer[buf_size++] = (strtoint & 0xff);
		token = strsep(&input_copy, delim);
	}

	SDE_DEBUG("command packet size in bytes: %u\n", buf_size);
	rc = buf_size;
end:
	kfree(input_dup);
	kfree(input);

	return rc;
}

static struct sde_connector *_sde_debugfs_motUtil_get_sde_conn(
					struct sde_kms *kms,
					size_t count, char input[])
{
	struct drm_device *dev = kms->dev;
	struct drm_connector *conn = NULL;
	struct drm_connector_list_iter conn_iter;
	struct sde_connector *sde_conn = NULL;
	int found_conn = 0;
	enum sde_motUtil_disp_cmd panel_type = input[DISPUTIL_PANEL_TYPE];

        mutex_lock(&dev->mode_config.mutex);
	drm_connector_list_iter_begin(dev, &conn_iter);
	drm_for_each_connector_iter(conn, &conn_iter) {
                if (panel_type == MOTUTIL_MAIN_DISP) {
                        if (!strncmp(conn->name, MAIN_DSI_CONN_NAME, 5)) {
                                DRM_DEBUG("found MAIN disp conn\n");
                                found_conn = 1;
                                break;
                        }
                } else if (panel_type == MOTUTIL_CLI_DISP) {
                         if (!strncmp(conn->name, CLI_DSI_CONN_NAME, 5)) {
                                SDE_DEBUG("found CLI disp conn\n");
                                found_conn = 1;
                                break;
                        }
                }
        }

        if (found_conn) {
		DRM_DEBUG("found SDE Connector for %s\n",
				(panel_type == MOTUTIL_MAIN_DISP) ?
				"Main Display" : "CLI Display");
                sde_conn = to_sde_connector(conn);
	} else {
		DRM_ERROR("Can not find SDE Connector for %s\n",
				(panel_type == MOTUTIL_MAIN_DISP) ?
				"Main Display" : "CLI Display");
	}

        mutex_unlock(&dev->mode_config.mutex);

	return sde_conn;
}

static int _sde_debugfs_motUtil_transfer(struct sde_kms *kms,
					size_t count, char input[])
{
	struct sde_connector *sde_conn = NULL;
	int ret = 0;
	enum sde_motUtil_disp_cmd panel_type = input[DISPUTIL_PANEL_TYPE];

	if (panel_type > MOTUTIL_CLI_DISP) {
		DRM_ERROR("Invalid panel number = %d\n", panel_type);
		return -EINVAL;
	}

	sde_conn = _sde_debugfs_motUtil_get_sde_conn(kms, count, input);
	if (!sde_conn) {
		ret = -EINVAL;
		goto end;
	}

	if (sde_conn && !sde_conn->ops.motUtil_transfer) {
		DRM_ERROR("no motUtil_transfer support for connector name %s\n",
						sde_conn->name);
		ret = -ENOENT;
		goto end;
	}

	if (motUtil_data.rd_buf) {
		mutex_lock(&sde_conn->lock);
		ret = sde_conn->ops.motUtil_transfer(sde_conn->display,
						input,	count, &motUtil_data);
		motUtil_data.last_cmd_tx_status = !ret ? true : false;
		motUtil_data.read_len = ret;
		mutex_unlock(&sde_conn->lock);

		ret = count;
	} else
		ret = -ENOMEM;
end:
	return ret;
}

static int _sde_debugfs_motUtil_set_tearing(struct sde_kms *kms,
					size_t count, char input[])
{
	struct sde_connector *sde_conn = NULL;
	int ret = 0;
	bool te_enable = input[TETEST_TE_ENABLE];
	enum sde_motUtil_disp_cmd panel_type = input[DISPUTIL_PANEL_TYPE];

	if (panel_type > MOTUTIL_CLI_DISP) {
		DRM_ERROR("Invalid panel number = %d\n", panel_type);
		return -EINVAL;
	}

	if (te_enable == motUtil_data.te_enable) {
		SDE_DEBUG("TE doesn't change. new_te_enable=%d old_te_enable=%d\n",
				te_enable, motUtil_data.te_enable);
		goto end;
	}

	sde_conn = _sde_debugfs_motUtil_get_sde_conn(kms, count, input);
	if(!sde_conn) {
		ret = -EINVAL;
		goto end;
	}

	if (sde_conn && !sde_conn->ops.set_tearing) {
		DRM_ERROR("no set_tearing for connector name %s\n",
					sde_conn->name);
		ret = -ENOENT;
		goto end;
	}

	mutex_lock(&sde_conn->lock);
	ret = sde_conn->ops.set_tearing(sde_conn->display,
					input[TETEST_TE_ENABLE]);
	if (ret)
		motUtil_data.te_enable = te_enable;
	mutex_unlock(&sde_conn->lock);
end:
	return 0;
}

static int _sde_debugfs_motUtil_read_frame_cnt(struct drm_encoder *drm_enc)
{
	int ret = 0;

	motUtil_data.val = sde_encoder_poll_rd_frame_counts(drm_enc);
	if (motUtil_data.val >= 0) {
		pr_info("Encoder transfer frame_count= %d\n", motUtil_data.val);
		motUtil_data.last_cmd_tx_status = 1;
	} else {
		SDE_ERROR("failed to get_frame_count ret =%d\n", ret);
		ret = -EAGAIN;
		motUtil_data.last_cmd_tx_status = 0;
	}

        return ret;

}

static int _sde_debugfs_motUtil_te_test(struct sde_kms *kms,
					size_t count, char input[])
{
	struct sde_connector *sde_conn = NULL;
	int ret = 0;

	motUtil_data.read_cmd = true;
	sde_conn = _sde_debugfs_motUtil_get_sde_conn(kms, count, input);
	if (!sde_conn) {
		ret = -EINVAL;
		goto end;
	}

	if (count < TETEST_TE_ENABLE) {
		DRM_ERROR(" Number of Input data is invalid (%zu).\n", count);
		ret = -EINVAL;
		goto end;
	}

	ret = _sde_debugfs_motUtil_set_tearing(kms, count, input);
	if (ret)
		goto end;

	ret = _sde_debugfs_motUtil_read_frame_cnt(sde_conn->encoder);
	if (!ret) {
		ret = -EAGAIN;
		goto end;
	}

end:
	return ret;
}

static ssize_t _sde_debugfs_motUtil_exe_command(struct sde_kms *kms,
					size_t count, char input[])
{
	int ret = 0;
	enum sde_motUtil_type motUtil_type = input[0];

	motUtil_data.motUtil_type = motUtil_type;
	switch (motUtil_type) {
	case MOTUTIL_DISP_UTIL:
		ret = _sde_debugfs_motUtil_transfer(kms, count, input);
		break;
	case MOTUTIL_TE_TEST:
		ret = _sde_debugfs_motUtil_te_test(kms, count, input);
		break;
	default:
		SDE_ERROR("Un-support motUtil type = %d\n", motUtil_type);
		ret = -EINVAL;
	}

	return ret;
}

static int _sde_debugfs_motUtil_open(struct inode *inode,
				struct file *file)
{
	if (!inode || !file)
		return -EINVAL;

	/* non-seekable */
	file->private_data = inode->i_private;
	return nonseekable_open(inode, file);
}

static ssize_t _sde_debugfs_motUtil_write(struct file *file,
			const char __user *buf, size_t count, loff_t *ppos)
{
	u32 buf_size = 0;
	char buffer[MAX_CMD_PAYLOAD_SIZE];
	struct sde_kms *kms = (struct sde_kms *)file->private_data;

	if (*ppos || !kms) {
		SDE_ERROR("invalid argument(s)\n");
		return 0;
	}

	buf_size = _sde_debugfs_motUtil_string2num(buf, count, buffer);
	if (!buf_size){
		SDE_ERROR("invalid input\n");
		return count;
	}

	mutex_lock(&motUtil_data.lock);
	_sde_debugfs_motUtil_exe_command(kms, count, buffer);
	mutex_unlock(&motUtil_data.lock);

	return count;
}

static ssize_t _sde_debugfs_motUtil_dispUtil_read(char *buffer)
{
	int blen = 0, i;

	if (motUtil_data.read_len) {
		blen = snprintf(buffer, 16, "motUtil_read: ");
		for (i = 0; i < motUtil_data.read_len; i ++)
			blen += snprintf((buffer + blen) , 6, "0x%02x ",
					*motUtil_data.rd_buf++);

			blen += snprintf((buffer + blen), 2, "\n");
	} else
		SDE_ERROR("motUtil failed to read from panel\n");

	return blen;
}

static ssize_t _sde_debugfs_motUtil_read(struct file *file,
			char __user *buf, size_t count, loff_t *ppos)
{
	int blen = 0;
	char buffer[MAX_CMD_PAYLOAD_SIZE];

	if (*ppos)
		return 0;

	mutex_lock(&motUtil_data.lock);
	if (!motUtil_data.read_cmd) {
		blen = snprintf(buffer, MAX_CMD_PAYLOAD_SIZE,
			"motUtil_status: 0x%x\n",
			motUtil_data.last_cmd_tx_status);
	} else if (motUtil_data.motUtil_type == MOTUTIL_DISP_UTIL) {
		blen = _sde_debugfs_motUtil_dispUtil_read(buffer);
	} else if (motUtil_data.motUtil_type == MOTUTIL_TE_TEST) {
		blen = snprintf(buffer, 16, "motUtil_read: ");
		blen += snprintf((buffer + blen), 12, "0x%08x ",
							motUtil_data.val);
		blen += snprintf((buffer + blen), 2, "\n");
	}

        pr_info("%s\n", buffer);

        if (blen <= 0) {
                SDE_ERROR("snprintf failed, blen %d\n", blen);
                return 0;
        }

        if (copy_to_user(buf, buffer, min(count, (size_t)blen+1))) {
                SDE_ERROR("copy to user buffer failed\n");
                return -EFAULT;
        }

        *ppos += blen;
	mutex_unlock(&motUtil_data.lock);
        return blen;
}

static const struct file_operations sde_debugfs_motUtil_fops = {
	.open =         _sde_debugfs_motUtil_open,
	.read =         _sde_debugfs_motUtil_read,
	.write =        _sde_debugfs_motUtil_write,
};

int sde_debugfs_mot_util_init(struct sde_kms *sde_kms,
			struct dentry *parent)
{
	debugfs_create_file("motUtil", 0606, parent,
			sde_kms, &sde_debugfs_motUtil_fops);

	motUtil_data.rd_buf = kzalloc(SZ_4K, GFP_KERNEL);
	if (!motUtil_data.rd_buf)
		return -ENOMEM;

	motUtil_data.te_enable = true;
	mutex_init(&motUtil_data.lock);
	return 0;
}
