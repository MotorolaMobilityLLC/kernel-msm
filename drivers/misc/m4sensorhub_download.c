/*
 *  Copyright (C) 2012 Motorola, Inc.
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
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/input.h>
#include <linux/uaccess.h>
#include <linux/m4sensorhub.h>
#include <linux/m4sensorhub_client_ioctl.h>
#include <linux/m4sensorhub/MemMapDownload.h>
#include <linux/wait.h>
#include <linux/slab.h>

#define DOWNLOAD_CLIENT_DRIVER_NAME	"m4sensorhub_download"
#define M4_SENSOR_DL_MAX_RETRY_CNT	3
#define M4_SENSOR_DL_MAX_RET_SIZE	8
#define M4_SENSOR_DL_MIN_INPUT_SIZE	\
	(sizeof(struct m4sh_download_packet) - M4_SENSOR_DL_MAX_PACKET_SIZE)

enum {
	i2c_reg_end,
	i2c_reg_read,
	i2c_reg_write,
	i2c_reg_wait,
};

struct i2c_reg_sequence {
	int direct;
	int reg;
};

struct download_client {
	struct m4sensorhub_data *m4sensorhub;
};

static struct download_client *misc_download_data;
static wait_queue_head_t download_wq;
static atomic_t m4_dlcmd_resp_ready;
static atomic_t download_client_entry;

static struct i2c_reg_sequence seq_m4dlm_get_checksum[] = {
	{i2c_reg_write, M4SH_REG_DOWNLOAD_FILENAME},
	{i2c_reg_write, M4SH_REG_DOWNLOAD_COMMAND},
	{i2c_reg_read, M4SH_REG_DOWNLOAD_CHECKSUM},
	{i2c_reg_read, M4SH_REG_DOWNLOAD_STATUS},
	{i2c_reg_end, i2c_reg_end}
};

static struct i2c_reg_sequence seq_m4dlm_open_file[] = {
	{i2c_reg_write, M4SH_REG_DOWNLOAD_FILENAME},
	{i2c_reg_write, M4SH_REG_DOWNLOAD_COMMAND},
	{i2c_reg_wait, M4SH_REG_DOWNLOAD_STATUS},
	{i2c_reg_end, i2c_reg_end}
};

static struct i2c_reg_sequence seq_m4dlm_close_file[] = {
	{i2c_reg_write, M4SH_REG_DOWNLOAD_COMMAND},
	{i2c_reg_wait, M4SH_REG_DOWNLOAD_STATUS},
	{i2c_reg_read, M4SH_REG_DOWNLOAD_CHECKSUM},
	{i2c_reg_end, i2c_reg_end}
};

static struct i2c_reg_sequence seq_m4dlm_delete_file[] = {
	{i2c_reg_write, M4SH_REG_DOWNLOAD_FILENAME},
	{i2c_reg_write, M4SH_REG_DOWNLOAD_COMMAND},
	{i2c_reg_wait, M4SH_REG_DOWNLOAD_STATUS},
	{i2c_reg_end, i2c_reg_end}
};

static struct i2c_reg_sequence seq_m4dlm_write_file[] = {
	{i2c_reg_write, M4SH_REG_DOWNLOAD_PACKET},
	{i2c_reg_wait, M4SH_REG_DOWNLOAD_STATUS},
	{i2c_reg_end, i2c_reg_end}
};

static struct i2c_reg_sequence seq_m4dlm_write_size_file[] = {
	{i2c_reg_write, M4SH_REG_DOWNLOAD_SIZE},
	{i2c_reg_write, M4SH_REG_DOWNLOAD_PACKET},
	{i2c_reg_wait, M4SH_REG_DOWNLOAD_STATUS},
	{i2c_reg_end, i2c_reg_end}
};

static int download_client_open(struct inode *inode, struct file *file)
{
	int err = atomic_inc_return(&download_client_entry);
	if (err == 1) {
		err = nonseekable_open(inode, file);
		if (err >= 0) {
			file->private_data = misc_download_data;
			return 0;
		}
	} else
		err = -EBUSY;

	atomic_dec_return(&download_client_entry);
	KDEBUG(M4SH_ERROR, "%s: failed, err=%d\n", __func__, -err);
	return err;
}

static int download_client_close(struct inode *inode, struct file *file)
{
	int entry = atomic_dec_return(&download_client_entry);
	file->private_data = NULL;
	KDEBUG(M4SH_DEBUG, "%s: entry = %d\n", __func__, entry);
	return 0;
}

static void m4_handle_download_irq(enum m4sensorhub_irqs int_event,
					void *download_data)
{
	atomic_set(&m4_dlcmd_resp_ready, true);
	wake_up_interruptible(&download_wq);
}

static inline void wait_m4_cmd_executed(void)
{
	wait_event_interruptible(download_wq, \
			(atomic_read(&m4_dlcmd_resp_ready)));
	atomic_set(&m4_dlcmd_resp_ready, false);
}

static char *m4dlm_i2c_reg_seq_getptr(
	int reg, struct m4sh_download_packet *dl_packet)
{
	switch (reg) {
	case M4SH_REG_DOWNLOAD_COMMAND:
		return (char *)(&(dl_packet->command));
	case M4SH_REG_DOWNLOAD_STATUS:
		return (char *)(&(dl_packet->status));
	case M4SH_REG_DOWNLOAD_SIZE:
		return (char *)(&(dl_packet->size));
	case M4SH_REG_DOWNLOAD_CHECKSUM:
		return (char *)(&(dl_packet->checksum));
	case M4SH_REG_DOWNLOAD_FILENAME:
		return (char *)(dl_packet->filename);
	case M4SH_REG_DOWNLOAD_PACKET:
		return (char *)(dl_packet->buffer);
	}
	KDEBUG(M4SH_ERROR, "%s Invaild i2c reg %d\n", __func__, reg);
	return NULL;
}

static int m4dlm_i2c_reg_seq_process(
	struct m4sensorhub_data *m4sensorhub,
	struct m4sh_download_packet *dl_packet,
	struct i2c_reg_sequence *i2c_reg_seq)
{
	int ret;
	for (; i2c_reg_seq->direct != i2c_reg_end; i2c_reg_seq++) {
		/*we don't need retry for I2C read/write as
		 *m4sensorhub_reg_write/read already had retry mechanism
		 */
		switch (i2c_reg_seq->direct) {
		case i2c_reg_write:
			ret = m4sensorhub_reg_write(m4sensorhub,
				i2c_reg_seq->reg,
				m4dlm_i2c_reg_seq_getptr(\
					i2c_reg_seq->reg, dl_packet),
				m4sh_no_mask);
			break;
		case i2c_reg_wait:
			/*Wait for IRQ answered*/
			wait_m4_cmd_executed();
			/*fallback to read status*/
		case i2c_reg_read:
			ret = m4sensorhub_reg_read(m4sensorhub,
				i2c_reg_seq->reg,
				m4dlm_i2c_reg_seq_getptr(\
					i2c_reg_seq->reg, dl_packet)
				);
			break;
		default:
			/*should be fault*/
			KDEBUG(M4SH_ERROR, "%s: Invaild I2C direct %d\n", \
						__func__, i2c_reg_seq->direct);
			return -ENOEXEC;
		}
		if (ret != m4sensorhub_reg_getsize(\
				m4sensorhub, i2c_reg_seq->reg)) {
			KDEBUG(M4SH_ERROR, "%s: Process I2C [%d-%d] failed!\n",
					__func__, i2c_reg_seq->direct,
					 i2c_reg_seq->reg);
			return -EIO;
		}
	}
	return 0;
}

static long download_client_ioctl(
	struct file *filp,
	unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;

	struct download_client *download_data = filp->private_data;
	struct i2c_reg_sequence *i2c_req_seq = NULL;
	int ret = -EINVAL, retry;
	static struct m4sh_download_packet packet;
	static unsigned short packet_size;

	switch (cmd) {
	case M4_SENSOR_IOCTL_DL_SEND_PACKET:
		if (copy_from_user(&packet, argp, M4_SENSOR_DL_MIN_INPUT_SIZE))
			return -EFAULT;

		KDEBUG(M4SH_INFO, "%s cmd = %d\n", __func__, packet.command);

		switch (packet.command) {
		case M4_SENSOR_DL_CMD_GET_CHECKSUM:
			i2c_req_seq = seq_m4dlm_get_checksum;
			break;
		case M4_SENSOR_DL_CMD_OPEN_FILE:
			packet_size = 0;
			i2c_req_seq = seq_m4dlm_open_file;
			break;
		case M4_SENSOR_DL_CMD_DELETE_FILE:
			i2c_req_seq = seq_m4dlm_delete_file;
			break;
		case M4_SENSOR_DL_CMD_CLOSE_FILE:
			i2c_req_seq = seq_m4dlm_close_file;
			break;
		case M4_SENSOR_DL_CMD_WRITE_FILE:
			if (!packet.size || \
				(packet.size > M4_SENSOR_DL_MAX_PACKET_SIZE)) {
				packet.status = M4_SENSOR_DL_ERROR_INVALID_SIZE;
				/*we only copy packet data before filename*/
				if (copy_to_user(argp, &packet, \
					M4_SENSOR_DL_MAX_RET_SIZE))
					return -EFAULT;
				return 0;
			}
			if (copy_from_user(&packet, argp, sizeof(packet)))
				return -EFAULT;
			if (packet.size != packet_size) {
				i2c_req_seq = seq_m4dlm_write_size_file;
				packet_size = packet.size;
			} else
				i2c_req_seq = seq_m4dlm_write_file;
			break;
		default:
			/*should be wrong command received*/
			KDEBUG(M4SH_ERROR, "%s Invaild packet cmd %d\n", \
						__func__, packet.command);
			return  -EINVAL;
		}
		for (retry = 0; retry++ < M4_SENSOR_DL_MAX_RETRY_CNT; ) {
			ret = m4dlm_i2c_reg_seq_process(\
					download_data->m4sensorhub,
					&packet, i2c_req_seq);
			/*only retry if M4 has internal error*/
			if (!ret) {
				switch (packet.status) {
				case M4_SENSOR_DL_ERROR_SEND_CMD:
				case M4_SENSOR_DL_ERROR_DATA_CHECKSUM:
				/*something wrong and we need retry*/
				KDEBUG(M4SH_ERROR, \
					"Tried %d times for packet cmd %d\n", \
					retry, packet.command);
					continue;
				}
			}
			break; /*exit retry loop*/
		}
		if (!ret) {
			/*we only copy packet data before filename for return*/
			if (copy_to_user(argp, &packet, \
					M4_SENSOR_DL_MAX_RET_SIZE))
				return -EFAULT;
		}
		break;
	default:
		KDEBUG(M4SH_ERROR, "%s Invaild ioctl cmd %d\n", __func__, cmd);
		break;
	}
	return ret;
}

static const struct file_operations download_client_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = download_client_ioctl,
	.open  = download_client_open,
	.release = download_client_close,
};

static struct miscdevice download_client_miscdrv = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = DOWNLOAD_CLIENT_DRIVER_NAME,
	.fops = &download_client_fops,
};

static int download_driver_init(struct init_calldata *p_arg)
{
	int ret;
	struct m4sensorhub_data *m4sensorhub = p_arg->p_m4sensorhub_data;
	ret = m4sensorhub_irq_register(m4sensorhub, M4SH_IRQ_DLCMD_RESP_READY,
					m4_handle_download_irq,
					misc_download_data, 0);
	if (ret < 0) {
		KDEBUG(M4SH_ERROR, "Error registering int %d (%d)\n",
			M4SH_IRQ_DLCMD_RESP_READY, ret);
		return ret;
	}
	ret = m4sensorhub_irq_enable(m4sensorhub, M4SH_IRQ_DLCMD_RESP_READY);
	if (ret < 0) {
		KDEBUG(M4SH_ERROR, "Error enable irq %d (%d)\n",
			M4SH_IRQ_DLCMD_RESP_READY, ret);
		goto exit;
	}
	return ret;
exit:
	m4sensorhub_irq_unregister(m4sensorhub, M4SH_IRQ_DLCMD_RESP_READY);
	return ret;
}

static int download_client_probe(struct platform_device *pdev)
{
	int ret = -1;
	struct download_client *download_client_data;
	struct m4sensorhub_data *m4sensorhub = m4sensorhub_client_get_drvdata();

	if (!m4sensorhub) {
		printk(KERN_WARNING "m4sensorhub is null\n");
		return -EFAULT;
	}

	download_client_data =
		kzalloc(sizeof(*download_client_data), GFP_KERNEL);
	if (!download_client_data)
		return -ENOMEM;

	download_client_data->m4sensorhub = m4sensorhub;
	platform_set_drvdata(pdev, download_client_data);

	ret = misc_register(&download_client_miscdrv);
	if (ret < 0) {
		KDEBUG(M4SH_ERROR, "Error registering %s driver\n",
				 DOWNLOAD_CLIENT_DRIVER_NAME);
		goto free_memory;
	}
	misc_download_data = download_client_data;
	ret = m4sensorhub_register_initcall(download_driver_init,
						download_client_data);
	if (ret < 0) {
		KDEBUG(M4SH_ERROR, "Unable to register init function "
			"for download client = %d\n", ret);
		goto unregister_misc_device;
	}
	init_waitqueue_head(&download_wq);
	atomic_set(&m4_dlcmd_resp_ready, false);
	atomic_set(&download_client_entry, 0);

	KDEBUG(M4SH_INFO, "Initialized %s driver\n",
		DOWNLOAD_CLIENT_DRIVER_NAME);
	return 0;

unregister_misc_device:
	misc_download_data = NULL;
	misc_deregister(&download_client_miscdrv);
free_memory:
	platform_set_drvdata(pdev, NULL);
	download_client_data->m4sensorhub = NULL;
	kfree(download_client_data);
	download_client_data = NULL;
	return ret;
}

static int __exit download_client_remove(struct platform_device *pdev)
{
	struct download_client *download_client_data =
		platform_get_drvdata(pdev);

	m4sensorhub_irq_disable(download_client_data->m4sensorhub,
				M4SH_IRQ_DLCMD_RESP_READY);
	m4sensorhub_irq_unregister(download_client_data->m4sensorhub,
				M4SH_IRQ_DLCMD_RESP_READY);
	m4sensorhub_unregister_initcall(download_driver_init);
	misc_download_data = NULL;
	misc_deregister(&download_client_miscdrv);
	platform_set_drvdata(pdev, NULL);
	download_client_data->m4sensorhub = NULL;
	kfree(download_client_data);
	download_client_data = NULL;
	return 0;
}

static void download_client_shutdown(struct platform_device *pdev)
{
}

#ifdef CONFIG_PM

static int download_client_suspend(struct platform_device *pdev,
				pm_message_t message)
{
	return 0;
}

static int download_client_resume(struct platform_device *pdev)
{
	return 0;
}

#else
#define download_client_suspend NULL
#define download_client_resume  NULL
#endif

static struct of_device_id m4download_match_tbl[] = {
	{ .compatible = "mot,m4download" },
	{},
};

static struct platform_driver download_client_driver = {
	.probe		= download_client_probe,
	.remove		= __exit_p(download_client_remove),
	.shutdown	= download_client_shutdown,
	.suspend	= download_client_suspend,
	.resume		= download_client_resume,
		.driver		= {
		.name	= DOWNLOAD_CLIENT_DRIVER_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(m4download_match_tbl),
	},
};

static int __init download_client_init(void)
{
	return platform_driver_register(&download_client_driver);
}

static void __exit download_client_exit(void)
{
	platform_driver_unregister(&download_client_driver);
}

module_init(download_client_init);
module_exit(download_client_exit);

MODULE_ALIAS("platform:download_client");
MODULE_DESCRIPTION("M4 Sensor Hub driver");
MODULE_AUTHOR("Motorola");
MODULE_LICENSE("GPL");

