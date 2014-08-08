/*
 * Copyright (C) 2014 Motorola Mobility LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <linux/stat.h>
#include <linux/err.h>
#include <linux/errno.h>


#include <linux/stml0xx.h>

int stml0xx_led_set(struct stml0xx_data *ps_stml0xx)
{
	unsigned char buf[SPI_MSG_SIZE];
	int err;

	buf[0] = ps_stml0xx->led_rgb >> 24;
	buf[1] = (ps_stml0xx->led_rgb >> 16) & 0xFF;
	buf[2] = (ps_stml0xx->led_rgb >> 8) & 0xFF;
	buf[3] = ps_stml0xx->led_rgb & 0xFF;

	buf[4] = ps_stml0xx->led_ms_on >> 24;
	buf[5] = (ps_stml0xx->led_ms_on >> 16) & 0xFF;
	buf[6] = (ps_stml0xx->led_ms_on >> 8) & 0xFF;
	buf[7] = ps_stml0xx->led_ms_on & 0xFF;

	buf[8] = ps_stml0xx->led_ms_off >> 24;
	buf[9] = (ps_stml0xx->led_ms_off >> 16) & 0xFF;
	buf[10] = (ps_stml0xx->led_ms_off >> 8) & 0xFF;
	buf[11] = ps_stml0xx->led_ms_off & 0xFF;

	err = stml0xx_spi_send_write_reg(LED_NOTIF_CONTROL, buf, 12);
	if (err < 0)
		return err;

	return 0;
}

static ssize_t notification_show_control(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct led_classdev *led = dev_get_drvdata(dev);
	struct stml0xx_data *ps_stml0xx =
		container_of(led, struct stml0xx_data, led_cdev);

	scnprintf(buf, PAGE_SIZE,
		"RGB=0x%08X, on/off=%d/%d ms, ramp=%d%%/%d%%\n",
		ps_stml0xx->led_rgb, ps_stml0xx->led_ms_on,
		ps_stml0xx->led_ms_off, ps_stml0xx->led_ramp_up,
		ps_stml0xx->led_ramp_down);

	return strlen(buf);
}

static ssize_t notification_store_control(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t len)
{
	struct led_classdev *led = dev_get_drvdata(dev);
	struct stml0xx_data *ps_stml0xx =
		container_of(led, struct stml0xx_data, led_cdev);
	unsigned rgb = 0, ms_on = 0, ms_off = 0, rup = 0, rdown = 0;

	if (len == 0)
		return 0;

	sscanf(buf, "%x %u %u %u %u", &rgb, &ms_on, &ms_off, &rup, &rdown);
	dev_dbg(&ps_stml0xx->spi->dev,
		"0x%X, on/off=%u/%u ms, ramp=%u%%/%u%%\n",
		rgb, ms_on, ms_off, rup, rdown);

	/* LED that is off is not blinking */
	if (rgb == 0) {
		ms_on = 0;
		ms_off = 0;
	}
	/* Total ramp time cannot be more than 100% */
	if (rup + rdown > 100) {
		rup = 50;
		rdown = 50;
	}
	/* If ms_on is not 0 but ms_off is 0 we won't blink */
	if (ms_off == 0)
		ms_on = 0;

	ps_stml0xx->led_rgb = rgb;
	ps_stml0xx->led_ms_on = ms_on;
	ps_stml0xx->led_ms_off = ms_off;
	ps_stml0xx->led_ramp_up = rup;
	ps_stml0xx->led_ramp_down = rdown;

	stml0xx_led_set(ps_stml0xx);

	return len;
}

static DEVICE_ATTR(control, S_IRUGO | S_IWUSR,
	notification_show_control, notification_store_control);

static struct attribute *notification_led_attributes[] = {
	&dev_attr_control.attr,
	NULL,
};

struct attribute_group stml0xx_notification_attribute_group = {
	.attrs = notification_led_attributes
};

