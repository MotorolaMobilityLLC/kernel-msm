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

int stml0xx_led_set(struct led_classdev *led_cdev)
{
	return stml0xx_led_set_reset(led_cdev, RESET_ALLOWED);
}

int stml0xx_led_set_reset(struct led_classdev *led_cdev, uint8_t allow_reset)
{
	struct stml0xx_data *ps_stml0xx = stml0xx_misc_data;
	unsigned char buf[SPI_MSG_SIZE];
	int err;

	buf[0] = led_cdev->brightness >> 24;
	buf[1] = (led_cdev->brightness >> 16) & 0xFF;
	buf[2] = (led_cdev->brightness >> 8) & 0xFF;
	buf[3] = led_cdev->brightness & 0xFF;

	buf[4] = led_cdev->blink_delay_on >> 24;
	buf[5] = (led_cdev->blink_delay_on >> 16) & 0xFF;
	buf[6] = (led_cdev->blink_delay_on >> 8) & 0xFF;
	buf[7] = led_cdev->blink_delay_on & 0xFF;

	buf[8] = led_cdev->blink_delay_off >> 24;
	buf[9] = (led_cdev->blink_delay_off >> 16) & 0xFF;
	buf[10] = (led_cdev->blink_delay_off >> 8) & 0xFF;
	buf[11] = led_cdev->blink_delay_off & 0xFF;

	stml0xx_wake(ps_stml0xx);

	err = stml0xx_spi_send_write_reg_reset(LED_NOTIF_CONTROL, buf, 12,
		allow_reset);
	if (err < 0)
		return err;

	stml0xx_sleep(ps_stml0xx);

	return 0;
}

static ssize_t notification_show_control(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);

	scnprintf(buf, PAGE_SIZE,
		"RGB=0x%08X, on/off=%ld/%ld ms, ramp=%d%%/%d%%\n",
		led_cdev->brightness, led_cdev->blink_delay_on,
		led_cdev->blink_delay_off, 0, 0);

	return strlen(buf);
}

static ssize_t notification_store_control(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t len)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct stml0xx_data *ps_stml0xx =
		container_of(led_cdev, struct stml0xx_data, led_cdev);
	unsigned rgb = 0;
	unsigned ms_on = 1000;
	unsigned ms_off = 0;
	unsigned rup = 0;
	unsigned rdown = 0;

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

	led_cdev->brightness = rgb;
	led_cdev->blink_delay_on = ms_on;
	led_cdev->blink_delay_off = ms_off;

	stml0xx_led_set(led_cdev);

	return len;
}

void stml0xx_brightness_set(struct led_classdev *led_cdev,
	enum led_brightness value)
{
	switch (value) {
	case LED_OFF:
		led_cdev->brightness = STML0XX_LED_OFF;
		break;
	case LED_HALF:
		led_cdev->brightness = STML0XX_LED_HALF_BRIGHTNESS;
		break;
	case LED_FULL:
		led_cdev->brightness = STML0XX_LED_MAX_BRIGHTNESS;
		break;
	}

	stml0xx_led_set(led_cdev);
}

enum led_brightness stml0xx_brightness_get(struct led_classdev *led_cdev)
{
	if (led_cdev->brightness == STML0XX_LED_OFF)
		return LED_OFF;
	else if (led_cdev->brightness == STML0XX_LED_MAX_BRIGHTNESS)
		return LED_FULL;
	else
		return LED_HALF;

}

int stml0xx_blink_set(struct led_classdev *led_cdev,
	unsigned long *delay_on, unsigned long *delay_off)
{
	if (*delay_on > STML0XX_LED_MAX_DELAY)
		*delay_on = STML0XX_LED_MAX_DELAY;
	if (*delay_off > STML0XX_LED_MAX_DELAY)
		*delay_off = STML0XX_LED_MAX_DELAY;

	led_cdev->blink_delay_on = *delay_on;
	led_cdev->blink_delay_off = *delay_off;

	stml0xx_led_set(led_cdev);

	return 0;
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
