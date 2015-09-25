/*
 *  Copyright (C) 2015 Motorola, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  Adds ability to program periodic interrupts from user space that
 *  can wake the phone out of low power modes.
 *
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/m4sensorhub.h>
#include <linux/m4sensorhub/m4sensorhub_registers.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#define MAX_SIMPLEFILE_NAME_LEN		16
#define M4SH_FS_PACKET_SIZE		228

struct _fscmd {
	u8 cmd;
	u8 name[MAX_SIMPLEFILE_NAME_LEN+1];
	u16 size;
};

struct _fsrw {
	u16 offset;
	u16 size;
};
#define MAX_ONCE_DATA_SIZE		(M4SH_FS_PACKET_SIZE\
					 - sizeof(struct _fsrw))
#define MAX_PACKET_SIZE_MASK		0x7FFF
#define END_OF_PACKET_MASK		0x8000

enum simplefs_response_codes {
	SIMPLEFS_COMPLETED,
	SIMPLEFS_PROCESSING,
	SIMPLEFS_ERROR,
};

enum simplefs_commands {
	SIMPLEFS_CMD_RESET,
	SIMPLEFS_CMD_READ_DIR,
	SIMPLEFS_CMD_READ_FILE,
	SIMPLEFS_CMD_WRITE_FILE,
	SIMPLEFS_CMD_DELETE_FILE,
};

struct m4sensorhub_simplefs_drvdata {
	struct m4sensorhub_data *m4sensorhub;
	struct platform_device  *pdev;
	struct mutex		mutex; /* controls driver entry points */
	u8			*buffer;
	struct device_attribute dev_attr;
	u8			filename[MAX_SIMPLEFILE_NAME_LEN+1];
	struct completion	cmd_comp;
	u32			response;
	struct _fscmd		command;
	u8			packet[M4SH_FS_PACKET_SIZE];
};

static int m4sensorhub_simplefs_send_locked(
	struct m4sensorhub_simplefs_drvdata *dd, bool ispkt)
{
	enum m4sensorhub_reg reg = ispkt ? M4SH_REG_SIMPLEFS_PACKET
					 : M4SH_REG_SIMPLEFS_COMMAND;
	u8 *data = ispkt ? dd->packet : (u8 *)&(dd->command);
	int size = ispkt ? M4SH_FS_PACKET_SIZE : sizeof(dd->command);
	int ret;
	INIT_COMPLETION(dd->cmd_comp);
	m4sensorhub_irq_enable(dd->m4sensorhub, M4SH_NOWAKEIRQ_SIMPLEFS);
	ret = m4sensorhub_reg_write(dd->m4sensorhub, reg, data, m4sh_no_mask);
	WARN(ret != size, "failed write reg:0x%x, size:%d ret:%d\n",
	     reg, size, ret);
	ret = wait_for_completion_timeout(&dd->cmd_comp,
					  msecs_to_jiffies(3000));
	m4sensorhub_irq_disable(dd->m4sensorhub, M4SH_NOWAKEIRQ_SIMPLEFS);
	if (ret <= 0) {
		WARN(1, "write reg(0x%x) time out\n", reg);
		return -EFAULT;
	}
	dd->response = SIMPLEFS_PROCESSING;
	ret = m4sensorhub_reg_read(dd->m4sensorhub, M4SH_REG_SIMPLEFS_RESPONSE,
				   (unsigned char *)&(dd->response));
	WARN(ret != sizeof(dd->response), "failed to read, ret = %d\n", ret);
	if (dd->response != SIMPLEFS_COMPLETED) {
		pr_err("%s: response is %d\n", __func__, dd->response);
		return -EFAULT;
	}
	return 0;
}

static int m4sensorhub_simplefs_reset_locked(
	struct m4sensorhub_simplefs_drvdata *dd)
{
	dd->command.cmd = SIMPLEFS_CMD_RESET;
	return m4sensorhub_simplefs_send_locked(dd, false);
}

static int m4sensorhub_simplefs_read_dir_locked(
	struct m4sensorhub_simplefs_drvdata *dd)
{
	int ret;
	ret = m4sensorhub_simplefs_reset_locked(dd);
	if (ret)
		return ret;
	dd->command.cmd = SIMPLEFS_CMD_READ_DIR;
	ret = m4sensorhub_simplefs_send_locked(dd, false);
	if (ret)
		return ret;
	ret = m4sensorhub_reg_read(dd->m4sensorhub, M4SH_REG_SIMPLEFS_PACKET,
				   dd->packet);
	if (ret != sizeof(dd->packet))
		return -EFAULT;
	return 0;
}

static int m4sensorhub_simplefs_read_file_locked(
	struct m4sensorhub_simplefs_drvdata *dd, char *buf)
{
	struct _fsrw *read = (struct _fsrw *)dd->packet;
	int ret = m4sensorhub_simplefs_reset_locked(dd);
	if (ret)
		return ret;
	strlcpy(dd->command.name, dd->filename, sizeof(dd->command.name));
	dd->command.cmd = SIMPLEFS_CMD_READ_FILE;
	dd->command.size = 0;
	for (;;) {
		ret = m4sensorhub_simplefs_send_locked(dd, false);
		if (ret)
			return ret;
		ret = m4sensorhub_reg_read(dd->m4sensorhub,
					   M4SH_REG_SIMPLEFS_PACKET,
					   dd->packet);
		if (ret != sizeof(dd->packet))
			return -EFAULT;
		if (read->offset != dd->command.size)
			return -EFAULT;
		memcpy(buf + read->offset, read+1,
		       read->size & MAX_PACKET_SIZE_MASK);
		dd->command.size += read->size & MAX_PACKET_SIZE_MASK;
		if (read->size & END_OF_PACKET_MASK)
			break;
	}
	return 0;
}

static int m4sensorhub_simplefs_write_file_locked(
	struct m4sensorhub_simplefs_drvdata *dd, const char *buf, int len)
{
	struct _fsrw *write = (struct _fsrw *)dd->packet;
	int ret = m4sensorhub_simplefs_reset_locked(dd);
	if (ret)
		return ret;
	strlcpy(dd->command.name, dd->filename, sizeof(dd->command.name));
	dd->command.cmd = SIMPLEFS_CMD_WRITE_FILE;
	dd->command.size = len;
	ret = m4sensorhub_simplefs_send_locked(dd, false);
	if (ret)
		return ret;
	for (write->offset = 0; write->offset < len; ) {
		write->size = len - write->offset;
		if (write->size > MAX_ONCE_DATA_SIZE)
			write->size = MAX_ONCE_DATA_SIZE;
		else
			write->size |= END_OF_PACKET_MASK;
		memcpy(write + 1, buf + write->offset,
		       write->size & MAX_PACKET_SIZE_MASK);
		ret = m4sensorhub_simplefs_send_locked(dd, true);
		if (ret)
			return ret;
		write->offset += write->size & MAX_PACKET_SIZE_MASK;
	}
	return 0;
}

static int m4sensorhub_simplefs_open_locked(
	struct m4sensorhub_simplefs_drvdata *dd, char *fname)
{
	int r = 0;
	if (strcmp(fname, dd->filename)) {
		if (dd->filename[0]) {
			device_remove_file(&dd->pdev->dev, &dd->dev_attr);
			dd->filename[0] = 0;
		}
		strlcpy(dd->filename, fname, sizeof(dd->filename));
		r = device_create_file(&dd->pdev->dev, &dd->dev_attr);
		if (r)
			dd->filename[0] = 0;
	}
	return r;
}

static int m4sensorhub_simplefs_close_locked(
	struct m4sensorhub_simplefs_drvdata  *dd, char *fname)
{
	m4sensorhub_simplefs_reset_locked(dd);
	if (!strcmp(fname, dd->filename)) {
		device_remove_file(&dd->pdev->dev, &dd->dev_attr);
		dd->filename[0] = 0;
		return 0;
	}
	return -EINVAL;
}

static int m4sensorhub_simplefs_delete_locked(
	struct m4sensorhub_simplefs_drvdata  *dd, char *fname)
{
	int ret = m4sensorhub_simplefs_reset_locked(dd);
	if (ret)
		return ret;
	dd->command.cmd = SIMPLEFS_CMD_DELETE_FILE;
	strlcpy(dd->command.name, fname, sizeof(dd->command.name));
	return m4sensorhub_simplefs_send_locked(dd, false);
}

static ssize_t m4sensorhub_simplefs_file_read(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct m4sensorhub_simplefs_drvdata *dd = platform_get_drvdata(pdev);
	int ret;
	mutex_lock(&(dd->mutex));
	ret = m4sensorhub_simplefs_read_file_locked(dd, buf);
	if (ret)
		pr_err("%s: failed read file from M4\n", __func__);
	else
		ret = dd->command.size;
	mutex_unlock(&(dd->mutex));
	return ret;
}

static ssize_t __ref m4sensorhub_simplefs_file_write(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct m4sensorhub_simplefs_drvdata *dd = platform_get_drvdata(pdev);
	int ret;
	mutex_lock(&(dd->mutex));
	ret = m4sensorhub_simplefs_write_file_locked(dd, buf, count);
	mutex_unlock(&(dd->mutex));
	return ret ? ret : count;
}

static ssize_t m4sensorhub_simplefs_list_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct m4sensorhub_simplefs_drvdata *dd = platform_get_drvdata(pdev);
	ssize_t count = 0;
	int ret;
	mutex_lock(&(dd->mutex));
	ret = m4sensorhub_simplefs_read_dir_locked(dd);
	if (ret) {
_failed_:
		count += snprintf(buf+count, PAGE_SIZE-count,
				  "Failed read directory from M4\n");
	} else {
		struct _fsrw *read = (struct _fsrw *)dd->packet;
		struct _fscmd *list = (struct _fscmd *)(read+1);
		if ((read->size - sizeof(*read)) !=
		    (read->offset * sizeof(*list)))
			goto _failed_;
		count += snprintf(buf+count, PAGE_SIZE-count,
				  "Files in M4:\n");
		for (ret = read->offset; ret--; list++) {
			count += snprintf(buf+count, PAGE_SIZE-count,
					  "%d:% 6d\t%s\n", list->cmd,
					  list->size, list->name);
		}
		count += snprintf(buf+count, PAGE_SIZE-count,
				  "\nFile in process:\n");
		if (dd->filename[0])
			count += snprintf(buf+count, PAGE_SIZE-count,
					 "\t[%s]\n", dd->filename);
	}
	mutex_unlock(&(dd->mutex));
	return count;
}

static
int check_copy_filename(char *out, int out_size, const char *in, int in_size)
{
	int len = 0;
	while ((in_size > 0) && (*in <= ' ')) {
		in++;
		in_size--;
	}
	while ((in_size > 0) && (out_size > 1)) {
		if (*in <= ' ')
			break;
		*out++ = *in++;
		in_size--;
		out_size--;
		len++;
	}
	*out = '\0';
	return len;
}

static ssize_t __ref m4sensorhub_simplefs_list_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct m4sensorhub_simplefs_drvdata *dd = platform_get_drvdata(pdev);
	enum {
		cmd_open,
		cmd_close,
		cmd_delete,
		cmd_max
	};
	const struct {
		const char *cmd;
		const int off;
	} cmds[cmd_max] = {
		[cmd_open] = {"open ", 5},
		[cmd_close] = {"close ", 6},
		[cmd_delete] = {"delete ", 7},
	};
	int ret, cmd, len;
	const char *fname;
	u8 filename[MAX_SIMPLEFILE_NAME_LEN+1];
	mutex_lock(&(dd->mutex));
	for (cmd = 0; cmd < cmd_max; cmd++) {
		fname = strnstr(buf, cmds[cmd].cmd, count);
		if (fname == NULL)
			continue;
		fname += cmds[cmd].off;
		len = check_copy_filename(filename, sizeof(filename), fname,
					  count-cmds[cmd].off);
		if (!len) {
			pr_err("%s: invalid file name [%s]\n", __func__, fname);
			ret = -EINVAL;
			goto _ret_;
		}
		break;
	}
	switch (cmd) {
	case cmd_open:
		ret = m4sensorhub_simplefs_open_locked(dd, filename);
		break;
	case cmd_close:
		ret = m4sensorhub_simplefs_close_locked(dd, filename);
		break;
	case cmd_delete:
		ret = m4sensorhub_simplefs_delete_locked(dd, filename);
		break;
	default:
		pr_err("%s: invalid command [%s]\n", __func__, buf);
		ret = -EINVAL;
		goto _ret_;
	}
	pr_info("%s cmd = %d, fname = [%s], ret = %d\n",
		__func__, cmd, filename, ret);
_ret_:
	mutex_unlock(&(dd->mutex));
	return ret ? ret : count;
}
static DEVICE_ATTR(list, 0644, m4sensorhub_simplefs_list_show,
				m4sensorhub_simplefs_list_store);

static void m4_handle_simplefs_irq(enum m4sensorhub_irqs int_event,
							void *data)
{
	struct m4sensorhub_simplefs_drvdata *dd = data;
	complete_all(&dd->cmd_comp);
}

static int m4sensorhub_simplefs_driver_initcallback(struct init_calldata *arg)
{
	struct m4sensorhub_simplefs_drvdata *dd = arg->p_data;
	int ret;

	ret = m4sensorhub_irq_register(dd->m4sensorhub,
			M4SH_NOWAKEIRQ_SIMPLEFS, m4_handle_simplefs_irq,
			dd, 0);
	if (ret < 0)
		pr_err("%s: failed irq register(%d)\n", __func__, ret);

	return 0;
}

static int m4sensorhub_simplefs_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct m4sensorhub_simplefs_drvdata *dd;

	dd = devm_kzalloc(&pdev->dev,
			  sizeof(struct m4sensorhub_simplefs_drvdata),
			  GFP_KERNEL);
	if (!dd) {
		pr_err("%s: err: OOM\n", __func__);
		ret = -ENOMEM;
		return ret;
	}

	dd->pdev = pdev;
	dd->m4sensorhub = NULL;
	dd->dev_attr.attr.name = dd->filename;
	dd->dev_attr.attr.mode = 0644;
	dd->dev_attr.show = m4sensorhub_simplefs_file_read;
	dd->dev_attr.store = m4sensorhub_simplefs_file_write;
	mutex_init(&(dd->mutex));
	init_completion(&(dd->cmd_comp));

	dd->m4sensorhub = m4sensorhub_client_get_drvdata();
	if (dd->m4sensorhub == NULL) {
		pr_err("%s: M4 sensor data is NULL.\n", __func__);
		ret = -ENODATA;
		return ret;
	}

	ret = m4sensorhub_register_initcall(
			m4sensorhub_simplefs_driver_initcallback,
			dd);
	if (ret < 0) {
		pr_err("%s:Register init failed, ret = %d\n", __func__, ret);
		ret = -EINVAL;
		return ret;
	}

	platform_set_drvdata(pdev, dd);

	ret = device_create_file(&pdev->dev, &dev_attr_list);
	return ret;
}

static int __exit m4sensorhub_simplefs_remove(struct platform_device *pdev)
{
	struct m4sensorhub_simplefs_drvdata *dd = platform_get_drvdata(pdev);

	device_remove_file(&pdev->dev, &dev_attr_list);
	m4sensorhub_irq_disable(dd->m4sensorhub, M4SH_NOWAKEIRQ_SIMPLEFS);
	m4sensorhub_irq_unregister(dd->m4sensorhub, M4SH_NOWAKEIRQ_SIMPLEFS);
	m4sensorhub_unregister_initcall(
				m4sensorhub_simplefs_driver_initcallback);
	mutex_destroy(&(dd->mutex));
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static void m4sensorhub_simplefs_shutdown(struct platform_device *pdev)
{
	return;
}

#ifdef CONFIG_PM
static int m4sensorhub_simplefs_suspend(struct platform_device *pdev,
				pm_message_t message)
{
	return 0;
}

static int m4sensorhub_simplefs_resume(struct platform_device *pdev)
{
	return 0;
}
#else
#define m4sensorhub_simplefs_suspend NULL
#define m4sensorhub_simplefs_resume  NULL
#endif

static struct of_device_id m4sensorhub_simplefs_match_tbl[] = {
	{ .compatible = "mot,m4simplefs" },
	{},
};

static struct platform_driver m4sensorhub_simplefs_driver = {
	.probe		= m4sensorhub_simplefs_probe,
	.remove		= __exit_p(m4sensorhub_simplefs_remove),
	.shutdown	= m4sensorhub_simplefs_shutdown,
	.suspend	= m4sensorhub_simplefs_suspend,
	.resume		= m4sensorhub_simplefs_resume,
	.driver		= {
		.name	= "m4sensorhub_simplefs",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(m4sensorhub_simplefs_match_tbl),
	},
};

module_platform_driver(m4sensorhub_simplefs_driver);

MODULE_ALIAS("platform:m4sensorhub_simplefs");
MODULE_DESCRIPTION("M4 Sensor Hub simple file system driver");
MODULE_AUTHOR("Motorola");
MODULE_LICENSE("GPL");
