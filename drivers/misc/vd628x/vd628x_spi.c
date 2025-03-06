/******************** (C) COPYRIGHT 2025 STMicroelectronics ********************
*
* File Name          : vd628x_spi.c
*
********************************************************************************
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* THE PRESENT SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES
* OR CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, FOR THE SOLE
* PURPOSE TO SUPPORT YOUR APPLICATION DEVELOPMENT.
* AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY DIRECT,
* INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM THE
* CONTENT OF SUCH SOFTWARE AND/OR THE USE MADE BY CUSTOMERS OF THE CODING
* INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
*
*******************************************************************************/

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/miscdevice.h>

#include "cam_sensor_dev.h"
#include "vd628x_spi_ioctl.h"

#include <linux/of.h>
#include <linux/uaccess.h>

#define VD628x_ADAPTER_DEV_NAME "vd628x_adapter"

struct vd628x_spidev_data {
	struct spi_device *pdev;
	struct miscdevice misc;
	struct cam_hw_soc_info soc_info;
	u8 *pbuffer;
	int16_t *psamples;
	u32 spi_max_frequency;
	u32 spi_buffer_size;
	u32 spi_speed_hz;
	u16 samples_nb_per_chunk;
	u16 pdm_data_sample_width_in_bytes;
};

// legacy chunk transfer function. To be used by user part as a backup
static ssize_t vd628x_spi_read(struct file *file, char __user *buf, size_t count, loff_t *f_pos)
{
	int status = 0;
	unsigned long missing;
	struct vd628x_spidev_data *pdata = container_of(file->private_data,
		struct vd628x_spidev_data, misc);

	struct spi_transfer t = {
			.rx_buf = pdata->pbuffer,
			.len		= count,
		};
	struct spi_message m;

	if (count > pdata->spi_buffer_size)
		return -EMSGSIZE;

	// set the speed set by user, or the max_frequency one if not set by user
	t.speed_hz = pdata->spi_speed_hz;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	status = spi_sync(pdata->pdev, &m);
	if (status != 0) {
		pr_err("[%d] spi read failed\n", status);
		return status;
	}

	status = m.actual_length;
	missing = copy_to_user(buf, pdata->pbuffer, status);
	if (missing == status)
		status = -EFAULT;
	else
		status = status - missing;

	return status;
}


static int vd628x_spi_open(struct inode *inode, struct file *file)
{
	struct vd628x_spidev_data *pdata = container_of(file->private_data,
		struct vd628x_spidev_data, misc);

	if (!pdata->pbuffer) {

		if (pdata->spi_buffer_size != 0) {
			pdata->pbuffer = kmalloc(pdata->spi_buffer_size, GFP_KERNEL);
			if (!pdata->pbuffer) {
				pr_err("[%s] vd628x open spi failed => ENOMEM", __func__);
				return -ENOMEM;
			}
		}
		else
			return -EFAULT;
	}

	return 0;
}

static int vd628x_spi_chunk_transfer_and_get_samples(struct vd628x_spidev_data *pdata)
{
	int i,s;
	uint16_t index;
	uint32_t d;

	int status = 0;

	struct spi_transfer t = {
			.rx_buf = pdata->pbuffer,
			.len	= pdata->spi_buffer_size,
		};
	struct spi_message m;


	// set the speed set by user, or the max_frequency one if not set by user
	t.speed_hz = pdata->spi_speed_hz;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	status = spi_sync(pdata->pdev, &m);
	if (status != 0) {
		pr_err("[%d] spi read failed\n", status);
		return status;
	}

	if (m.actual_length != pdata->spi_buffer_size) {
		printk("vd628x. spi tranfser error");
		return -EFAULT;
	}

	for(s = 0; s < pdata->samples_nb_per_chunk ; s++) {
		// example : SPI frequency = 5*1024*1204 Hz. sampling rate = 2048.
		// ==> pdm_sample_width in bytes = 320 = 8*40
		// i = 0,8,16, ..... 312. ==> each __builtin_popcountll is applied on 8 bytes = 64 bits
		pdata->psamples[s] = 0;
		index = s*pdata->pdm_data_sample_width_in_bytes;
		for(i = 0; i < pdata->pdm_data_sample_width_in_bytes; i += 4) {
			d = 0;
			d += (((uint32_t)pdata->pbuffer[index+i])&0xFF);
			d += (((uint32_t)pdata->pbuffer[index+i+1])&0xFF)<<8;
			d += (((uint32_t)pdata->pbuffer[index+i+2])&0xFF)<<16;
			d += (((uint32_t)pdata->pbuffer[index+i+3])&0xFF)<<24;
			pdata->psamples[s] +=  __builtin_popcountl(d);
		}
	}

	return 0;
}


static int vd628x_spi_ioctl_handler(struct vd628x_spidev_data *pdata, unsigned int cmd, unsigned long arg)
{
	int ret;
	struct vd628x_spi_info spi_info;
	struct vd628x_spi_params spi_params;

	if (!pdata)
		return -EINVAL;

	switch (cmd) {
		case VD628x_IOCTL_GET_SPI_INFO:
			spi_info.chunk_size = pdata->spi_buffer_size;
			spi_info.spi_max_frequency = pdata->spi_max_frequency;
			ret = copy_to_user((void __user *) arg, &spi_info, sizeof(struct vd628x_spi_info));
			break;

		case VD628x_IOCTL_SET_SPI_PARAMS:
			ret = copy_from_user(&spi_params, (void __user *)arg, sizeof(struct vd628x_spi_params));
			if (ret != 0)
				return ret;
			if ((!spi_params.speed_hz) || (!spi_params.samples_nb_per_chunk) || (!spi_params.pdm_data_sample_width_in_bytes))
				return -EINVAL;
			pdata->spi_speed_hz = spi_params.speed_hz;
			pdata->samples_nb_per_chunk = spi_params.samples_nb_per_chunk;
			pdata->pdm_data_sample_width_in_bytes = spi_params.pdm_data_sample_width_in_bytes;
			if (pdata->psamples)
				kfree(pdata->psamples);
			pdata->psamples = (int16_t *)kmalloc(pdata->samples_nb_per_chunk*sizeof(int16_t), GFP_KERNEL);
			printk("vd628x : spi speed set : %d", pdata->spi_speed_hz);
			printk("vd628x : nb of samples per chunk  : %d", pdata->samples_nb_per_chunk);
			printk("vd628x : sample width in bytes: %d", pdata->pdm_data_sample_width_in_bytes);
			break;

		case VD628x_IOCTL_GET_CHUNK_SAMPLES:
			ret = vd628x_spi_chunk_transfer_and_get_samples(pdata);
			if (ret != 0)
				return ret;
			ret = copy_to_user((void __user *) arg, pdata->psamples, pdata->samples_nb_per_chunk * sizeof(int16_t));
			break;

		default:
			ret = -EINVAL;
	}

	return ret;
}

static long vd628x_spi_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{

	struct vd628x_spidev_data *pdata = container_of(file->private_data,
		struct vd628x_spidev_data, misc);

return vd628x_spi_ioctl_handler(pdata, cmd, arg);
}

static int vd628x_spi_release(struct inode *inode, struct file *file)
{
	struct vd628x_spidev_data *pdata = container_of(file->private_data,
		struct vd628x_spidev_data, misc);

	if (pdata->pbuffer) {
		kfree(pdata->pbuffer);
		pdata->pbuffer = NULL;
	}

	if (pdata->psamples) {
		kfree(pdata->psamples);
		pdata->psamples = NULL;
	}

	return 0;
}

static int vd628x_spi_parse_dt(struct vd628x_spidev_data *pdata)
{
	int ret = 0;
	struct device_node *of_node = pdata->pdev->dev.of_node;

	ret = of_property_read_u32(of_node, "spi-max-frequency",&pdata->spi_max_frequency);
	if (ret) {
		pr_err("[%s] failed to read spi-frequency", __func__);
		return ret;
	}

	// set the spi speed to max frequency by default
	pdata->spi_speed_hz = pdata->spi_max_frequency;

	printk("vd628x : [%s] spi (max) frequency=%d", __func__, pdata->spi_max_frequency);

	ret = of_property_read_u32(of_node, "chunk-size",&pdata->spi_buffer_size);
	if (ret) {
		pr_err("[%s] failed to read spi-frequency", __func__);
		return ret;
	}

	// set the spi speed to max frequency by default
	printk("vd628x : [%s] spi chunk size=%d", __func__, pdata->spi_buffer_size);

	return 0;
}

static const struct file_operations vd628x_spi_fops = {
	.owner		= THIS_MODULE,
	.open		= vd628x_spi_open,
	.release	= vd628x_spi_release,
	.read		= vd628x_spi_read,
	.unlocked_ioctl	= vd628x_spi_ioctl
};



int vd628x_spi_driver_probe(struct spi_device *pdev)
{
	int ret = 0;
	struct vd628x_spidev_data *pdata;

	pr_info("vd628x: enter probe spi\n");


	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	pdata->pdev = pdev;
	spi_set_drvdata(pdev, pdata);

	vd628x_spi_parse_dt(pdata);

	//pr_info("[%s] spi mode=%d, cs=%d, bits_per_word=%d, speed=%d, csgpio=%d, modalias=%s",
	//	___func__, pdev->mode, pdev->chip_select, pdev->bits_per_word,
	//	pdev->max_speed_hz, pdev->cs_gpio, pdev->modalias);

	pdata->misc.minor = MISC_DYNAMIC_MINOR;
	pdata->misc.name = "vd628x_spi";
	pdata->misc.fops = &vd628x_spi_fops;
	ret = misc_register(&pdata->misc);

	if (ret)
		pr_info("vd628x_spi_probe failed");
	else
		pr_info("vd628x_spi_probe successfully");

	return ret;
}


int vd628x_spi_driver_remove(struct spi_device *pdev)
{
	struct vd628x_spidev_data *pdata;
	pdata = spi_get_drvdata(pdev);
	if (!pdata) {
		pr_err("[%s] can't remove %p", __func__, pdev);
		return 0;
	}


	misc_deregister(&pdata->misc);

	return 0;
}



static const struct of_device_id vd628x_adapter_dt_ids[] = {
	{ .compatible = "st,vd628x_adapter" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, vd628x_adapter_dt_ids);

static const struct of_device_id vd628x_spi_dt_ids[] = {
	{ .compatible = "st,vd628x_spi" },
	{ /* sentinel */ }
};

static struct spi_driver vd628x_spi_driver = {
 	.driver = {
		.name = "vd628x_spi",
		.owner = THIS_MODULE,
		.of_match_table = vd628x_spi_dt_ids,
 	},
	.probe = vd628x_spi_driver_probe,
	.remove = vd628x_spi_driver_remove,
};

MODULE_DEVICE_TABLE(of, vd628x_spi_dt_ids);

static int __init vd628x_spi_module_init(void)
{
	int ret = 0;

	pr_info("vd628x: module init\n");

	ret = spi_register_driver(&vd628x_spi_driver);
	if (ret < 0) {
		pr_err("spi_register_driver failed => %d", ret);
		return ret;
	}

	return ret;
}

static void __exit vd628x_spi_module_exit(void)
{

	pr_debug("vd628x : module exit\n");

	spi_unregister_driver(&vd628x_spi_driver);
}

module_init(vd628x_spi_module_init);
module_exit(vd628x_spi_module_exit);

MODULE_DESCRIPTION("vd628x spi adapter driver");
MODULE_LICENSE("GPL v2");
