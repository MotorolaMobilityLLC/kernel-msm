/*
 *  Copyright (C) 2013 Motorola, Inc.
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

#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/m4sensorhub.h>
#include <linux/m4sensorhub_client_ioctl.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/sound.h>
#include <linux/soundcard.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/m4sensorhub/MemMapAudio.h>
#include <linux/spi/spi.h>


#define AUDIO_CLIENT_DRIVER_NAME "m4sensorhub_audio"
/* This is the number of total kernel buffers */
#define AUDIO_NBFRAGS_READ 20
#define AUDIO_SAMPLE_RATE 16000
#define AUDIO_TIMEOUT HZ
#define MIC_ENABLE 0x01
#define MIC_DISABLE 0x00

/* Mutex used to prevent mutiple calls to audio functions at same time */
DEFINE_MUTEX(audio_lock);

struct audio_client {
	struct m4sensorhub_data *m4sensorhub;
	struct spi_device *spi;
	int dev_dsp;
	int dev_dsp_open_count;
	char *buffers[AUDIO_NBFRAGS_READ];
	unsigned int usr_head;	/* user index where app is reading from */
	unsigned int buf_head;	/* SPI index where SPI writing to */
	unsigned int usr_offset;	/* offset in usr_head buffer to read */
	int read_buf_full;	/* num buffers available for app */
	u32 total_buf_cnt;  /* total num of bufs read from since audio enable*/
	wait_queue_head_t wq;	/* wait till read buffer is available */
	int active;	/* Indicates if audio transfer is active */
};

static struct audio_client *audio_data;

static void audio_client_spidma_read(struct audio_client *audio_client_data,
	int len)
{
	int ret = 0;
	struct spi_message msg;
	struct spi_transfer rx;
	unsigned char txbuff[AUDIO_BUFFER_SIZE];

	memset(&rx, 0x00, sizeof(struct spi_transfer));
	memset(&msg, 0x00, sizeof(struct spi_message));

	rx.rx_buf = audio_client_data->buffers[
		audio_client_data->buf_head];
	rx.tx_buf = txbuff;
	rx.len = len;

	spi_message_init(&msg);
	spi_message_add_tail(&rx, &msg);

	ret = spi_sync(audio_client_data->spi, &msg);
	if (ret < 0)
		KDEBUG(M4SH_ERROR, "%s failed to read %d bytes, ret = %d\n",
			__func__, len, ret);
	else {
		audio_data->read_buf_full++;
		audio_data->total_buf_cnt++;
		wake_up_interruptible(&audio_data->wq);

		if (++audio_client_data->buf_head >= AUDIO_NBFRAGS_READ)
			audio_client_data->buf_head = 0;
	}

}

static void m4_handle_audio_irq(enum m4sensorhub_irqs int_event,
	void *data)
{
	u32 m4_buf_cnt = 0;
	u32 bufs_to_read = 0;
	struct audio_client *audio_client_data = (struct audio_client *)data;
	int ret = 0;

	mutex_lock(&audio_lock);

	/* Read the total buf count from M4 */
	ret = m4sensorhub_reg_read(audio_client_data->m4sensorhub,
		M4SH_REG_AUDIO_TOTALPACKETS,
		(char *)&m4_buf_cnt);

	if (ret != m4sensorhub_reg_getsize(audio_client_data->m4sensorhub,
		M4SH_REG_AUDIO_TOTALPACKETS)) {
		KDEBUG(M4SH_ERROR, "M4 packet count read failed %d\n", ret);
		goto EXIT;
	}

	bufs_to_read = m4_buf_cnt - audio_data->total_buf_cnt;
	KDEBUG(M4SH_DEBUG, "R = %u, m4_cnt = %u, omap_cnt = %u\n",
			bufs_to_read, m4_buf_cnt, audio_data->total_buf_cnt);

	/* If no free buffers, then skip reads from SPI */
	while ((bufs_to_read) &&
			(audio_data->read_buf_full < AUDIO_NBFRAGS_READ)) {
		audio_client_spidma_read(audio_client_data, AUDIO_BUFFER_SIZE);
		bufs_to_read--;
	}

EXIT:
	mutex_unlock(&audio_lock);
}

static int audio_client_open(struct inode *inode, struct file *file)
{
	int ret = 0, i = 0;
	mutex_lock(&audio_lock);

	if (audio_data->dev_dsp_open_count == 1) {
		KDEBUG(M4SH_ERROR, "Mic already opened, can't open again\n");
		ret = -EBUSY;
		goto out;
	}

	for (i = 0; i < AUDIO_NBFRAGS_READ; i++) {
		audio_data->buffers[i] = kmalloc(AUDIO_BUFFER_SIZE,
			GFP_KERNEL | GFP_DMA);
		if (!audio_data->buffers[i]) {
			KDEBUG(M4SH_ERROR, "Can't allocate memory for mic\n");
			ret = -ENOMEM;
			goto free_buffers;
		}
	}

	audio_data->active = 0;
	audio_data->usr_head = 0;
	audio_data->usr_offset = 0;
	audio_data->buf_head = 0;
	audio_data->read_buf_full = 0;

	ret = m4sensorhub_irq_enable(audio_data->m4sensorhub,
		M4SH_IRQ_MIC_DATA_READY);
	if (ret < 0) {
		KDEBUG(M4SH_ERROR, "Unable to enable mic irq, ret = %d\n",
			ret);
	  goto free_buffers;
	}

	init_waitqueue_head(&audio_data->wq);

	audio_data->dev_dsp_open_count = 1;
	KDEBUG(M4SH_INFO, "M4 mic driver opened\n");
	goto out;

free_buffers:
	for (i = 0; i < AUDIO_NBFRAGS_READ; i++) {
		kfree((void *) audio_data->buffers[i]);
		audio_data->buffers[i] = NULL;
	}
out:
	mutex_unlock(&audio_lock);
	return ret;
}

static int audio_client_release(struct inode *inode, struct file *file)
{
	int i = 0, ret;
	mutex_lock(&audio_lock);

	audio_data->active = 0;
	ret = m4sensorhub_irq_disable(audio_data->m4sensorhub,
		M4SH_IRQ_MIC_DATA_READY);
	if (ret < 0)
		KDEBUG(M4SH_ERROR, "Unable to disable mic, ret = %d\n", ret);
	ret = m4sensorhub_reg_write_1byte(audio_data->m4sensorhub,
		M4SH_REG_AUDIO_ENABLE, MIC_DISABLE, 0xFF);
	/* Check that we wrote 1 byte */
	if (ret != 1)
		KDEBUG(M4SH_ERROR, "Unable to disable mic, size = %d\n", ret);

	audio_data->dev_dsp_open_count = 0;

	for (i = 0; i < AUDIO_NBFRAGS_READ; i++) {
		kfree((void *) audio_data->buffers[i]);
		audio_data->buffers[i] = NULL;
	}

	mutex_unlock(&audio_lock);
	KDEBUG(M4SH_INFO, "M4 mic driver closed\n");
	return 0;
}

static long audio_client_ioctl(struct file *filp,
				 unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	unsigned int samp_rate;

	mutex_lock(&audio_lock);

	switch (cmd) {
	case OSS_GETVERSION:
		ret = put_user(SOUND_VERSION, (int *)arg);
		break;

	case SNDCTL_DSP_SPEED:
		if (copy_from_user(&samp_rate, (unsigned int *)arg,
			     sizeof(unsigned int)))
			ret = -EFAULT;
		else if (samp_rate != AUDIO_SAMPLE_RATE)
			ret = -EINVAL;

		break;

	case SNDCTL_DSP_GETBLKSIZE:
		put_user(AUDIO_BUFFER_SIZE, (int *)arg);
		break;

	default:
		break;
	}
	mutex_unlock(&audio_lock);
	return ret;
}

static ssize_t audio_client_read(struct file *file, char *buffer, size_t size,
								loff_t *nouse)
{
	int ret = 0;
	int local_size = size;
	int local_offset = 0; /* offset into output buffer */
	int remainder_buff = 0; /* Indicates bytes remaining in input buffer */

	mutex_lock(&audio_lock);

	if (!audio_data->active) {
		ret = m4sensorhub_reg_write_1byte(audio_data->m4sensorhub,
			M4SH_REG_AUDIO_ENABLE, MIC_ENABLE, 0xFF);
		/* Check that we wrote 1 byte */
		if (ret != 1) {
			KDEBUG(M4SH_ERROR, "Unable to enable mic, size = %d\n",
				ret);
		  goto out;
		}
		audio_data->active = 1;
		audio_data->total_buf_cnt = 0;
	}

	while (local_size > 0) {
		mutex_unlock(&audio_lock);
		ret = wait_event_interruptible_timeout(audio_data->wq,
			audio_data->read_buf_full > 0, AUDIO_TIMEOUT);
		mutex_lock(&audio_lock);
		if (!ret) {
			KDEBUG(M4SH_ERROR,
				"Timed out waiting for mic buffer\n");
			goto out;
		}

		remainder_buff = AUDIO_BUFFER_SIZE - audio_data->usr_offset;
		if (local_size > remainder_buff) {

			if (copy_to_user(buffer + local_offset,
					audio_data->buffers
						[audio_data->usr_head] +
					audio_data->usr_offset,
					remainder_buff)) {
				KDEBUG(M4SH_ERROR,
					"Mic driver: copy_to_user failed \n");
				ret = -EFAULT;
				goto out;
			}

			if (++audio_data->usr_head >= AUDIO_NBFRAGS_READ)
				audio_data->usr_head = 0;

			if (--audio_data->read_buf_full < 0)
				audio_data->read_buf_full = 0;

			local_size -= remainder_buff;
			local_offset += remainder_buff;
			audio_data->usr_offset = 0;
		} else {

			if (copy_to_user(buffer + local_offset,
					audio_data->buffers
						[audio_data->usr_head] +
					audio_data->usr_offset, local_size)) {
				KDEBUG(M4SH_ERROR,
					"Mic driver: copy_to_user failed \n");
				ret = -EFAULT;
				goto out;
			}

			if (local_size == remainder_buff) {
				if (++audio_data->usr_head >=
						AUDIO_NBFRAGS_READ)
					audio_data->usr_head = 0;

				if (--audio_data->read_buf_full < 0)
					audio_data->read_buf_full = 0;

				audio_data->usr_offset = 0;

			} else {
				audio_data->usr_offset += local_size;
			}

			local_size = 0;
		}
	}
	ret = size;

out:
	mutex_unlock(&audio_lock);
	return ret;
}

/* File Ops structure */
static const struct file_operations audio_client_fops = {
	.owner = THIS_MODULE,
	.open = audio_client_open,
	.release = audio_client_release,
	.unlocked_ioctl = audio_client_ioctl,
	.read = audio_client_read,
};

static int audio_driver_init(struct init_calldata *p_arg)
{
	int ret;
	struct m4sensorhub_data *m4sensorhub = p_arg->p_m4sensorhub_data;

	ret = m4sensorhub_irq_register(m4sensorhub, M4SH_IRQ_MIC_DATA_READY,
		m4_handle_audio_irq, audio_data, 0);
	if (ret < 0) {
		KDEBUG(M4SH_ERROR, "Error registering int %d (%d)\n",
			M4SH_IRQ_MIC_DATA_READY, ret);
	}
	return ret;
}

static int audio_client_probe(struct spi_device *spi)
{
	int ret = -1;
	struct audio_client *audio_client_data;
	struct m4sensorhub_data *m4sensorhub = m4sensorhub_client_get_drvdata();

	if (!m4sensorhub)
		return -EFAULT;

	audio_client_data = kzalloc(sizeof(*audio_client_data), GFP_KERNEL);
	if (!audio_client_data)
		return -ENOMEM;
	audio_client_data->m4sensorhub = m4sensorhub;
	spi_set_drvdata(spi, audio_client_data);
	audio_client_data->spi = spi;
	audio_data = audio_client_data;

	ret = register_sound_dsp(&audio_client_fops, -1);
	if (ret < 0) {
		KDEBUG(M4SH_ERROR, "Error registering %s driver\n",
				 AUDIO_CLIENT_DRIVER_NAME);
		goto free_client_data;
	}
	audio_client_data->dev_dsp = ret;
	audio_client_data->dev_dsp_open_count = 0;
	ret = m4sensorhub_register_initcall(audio_driver_init,
						audio_client_data);
	if (ret < 0) {
		KDEBUG(M4SH_ERROR, "Unable to register init function "
			"for audio client = %d\n", ret);
		goto unregister_sound_device;
	}

	KDEBUG(M4SH_ERROR, "Initialized %s driver\n", AUDIO_CLIENT_DRIVER_NAME);
	return 0;

unregister_sound_device:
	unregister_sound_dsp(audio_client_data->dev_dsp);
free_client_data:
	spi_set_drvdata(spi, NULL);
	kfree(audio_client_data);
	return ret;
}

static int __exit audio_client_remove(struct spi_device *spi)
{
	struct audio_client *audio_client_data = spi_get_drvdata(spi);

	m4sensorhub_irq_disable(audio_client_data->m4sensorhub,
		M4SH_IRQ_MIC_DATA_READY);
	m4sensorhub_irq_unregister(audio_client_data->m4sensorhub,
		M4SH_IRQ_MIC_DATA_READY);
	m4sensorhub_unregister_initcall(audio_driver_init);
	unregister_sound_dsp(audio_client_data->dev_dsp);
	spi_set_drvdata(spi, NULL);
	kfree(audio_client_data);
	return 0;
}


static struct of_device_id m4audio_match_tbl[] = {
	{.compatible = "mot,m4audio"},
	{},
};

static struct spi_driver audio_client_spi_driver = {
	.driver = {
		.name = AUDIO_CLIENT_DRIVER_NAME,
		.bus = &spi_bus_type,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(m4audio_match_tbl),
	},
	.suspend = NULL,
	.resume = NULL,
	.probe = audio_client_probe,
	.remove = __exit_p(audio_client_remove),
};

static int __init audio_client_init(void)
{
	int ret = 0;

	ret = spi_register_driver(&audio_client_spi_driver);

	return ret;
}

static void __exit audio_client_exit(void)
{
	spi_unregister_driver(&audio_client_spi_driver);
}

module_init(audio_client_init);
module_exit(audio_client_exit);

MODULE_ALIAS("platform:audio_client");
MODULE_DESCRIPTION("M4 Sensor Hub audio client driver");
MODULE_AUTHOR("Motorola");
MODULE_LICENSE("GPL");

