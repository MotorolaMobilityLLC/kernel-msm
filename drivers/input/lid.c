/*
 * ASUS Lid driver.
 */
#include <linux/module.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/gpio_event.h>
#include <linux/gpio.h>

#define LID_DEBUG		0
#define CONVERSION_TIME_MS	50

#if LID_DEBUG
#define LID_INFO(format, arg...) \
	pr_info("hall_sensor: [%s] " format , __func__ , ##arg)
#else
#define LID_INFO(format, arg...) do { } while (0)
#endif

#define LID_NOTICE(format, arg...)	\
	pr_notice("hall_sensor: [%s] " format , __func__ , ##arg)

#define LID_ERR(format, arg...)	\
	pr_err("hall_sensor: [%s] " format , __func__ , ##arg)

struct delayed_work lid_hall_sensor_work;

/*
 * functions declaration
 */
static void lid_report_function(struct work_struct *dat);
static int lid_input_device_create(void);
static ssize_t show_lid_status(struct device *class,
		struct device_attribute *attr, char *buf);
/*
 * global variable
 */
static unsigned int hall_sensor_gpio = 36;
static int hall_sensor_irq;
static struct workqueue_struct *lid_wq;
static struct input_dev	 *lid_indev;
static DEVICE_ATTR(lid_status, S_IWUSR | S_IRUGO, show_lid_status, NULL);

/* Attribute Descriptor */
static struct attribute *lid_attrs[] = {
	&dev_attr_lid_status.attr,
	NULL
};

/* Attribute group */
static struct attribute_group lid_attr_group = {
	.attrs = lid_attrs,
};

static ssize_t show_lid_status(struct device *class,
		struct device_attribute *attr, char *buf)
{
	char *s = buf;

	s += sprintf(buf, "%u\n",
		gpio_get_value(hall_sensor_gpio) ? 1 : 0);

	return s - buf;
}

static irqreturn_t lid_interrupt_handler(int irq, void *dev_id)
{
	if (irq == hall_sensor_irq) {
		LID_NOTICE("LID interrupt handler...gpio: %d..\n",
			gpio_get_value(hall_sensor_gpio));
		queue_delayed_work(lid_wq, &lid_hall_sensor_work, 0);
	}
	return IRQ_HANDLED;
}

static void lid_report_function(struct work_struct *dat)
{
	int value = 0;

	if (!lid_indev) {
		LID_ERR("LID input device doesn't exist\n");
		return;
	}

	msleep(CONVERSION_TIME_MS);
	value = gpio_get_value(hall_sensor_gpio) ? 1 : 0;
	input_report_switch(lid_indev, SW_LID, !value);
	input_sync(lid_indev);

	LID_NOTICE("SW_LID report value = %d\n", value);
}

static int lid_input_device_create(void){
	int err = 0;

	lid_indev = input_allocate_device();
	if (!lid_indev) {
		LID_ERR("lid_indev allocation fails\n");
		err = -ENOMEM;
		goto exit;
	}

	lid_indev->name = "lid_input";
	lid_indev->phys = "/dev/input/lid_indev";

	set_bit(EV_SW, lid_indev->evbit);
	set_bit(SW_LID, lid_indev->swbit);

	err = input_register_device(lid_indev);
	if (err) {
		LID_ERR("lid_indev registration fails\n");
		goto exit_input_free;
	}
	return 0;

exit_input_free:
	input_free_device(lid_indev);
	lid_indev = NULL;
exit:
	return err;

}

static int __init lid_driver_probe(struct platform_device *pdev)
{
	int ret = 0, irq = 0;
	unsigned long irqflags;

	if (!pdev)
		return -EINVAL;

	pr_info("ASUSTek: %s", __func__);

	ret = sysfs_create_group(&pdev->dev.kobj, &lid_attr_group);

	if (ret) {
		LID_ERR("Unable to create sysfs, error: %d\n",
				ret);
		goto fail_sysfs;
	}

	ret = lid_input_device_create();
	if (ret) {
		LID_ERR(
		"Unable to register input device, error: %d\n",
			ret);
		goto fail_create;
	}

	lid_wq = create_singlethread_workqueue("lid_wq");
	if(!lid_wq){
		LID_ERR("Unable to create workqueue\n");
		goto fail_create;
	}

	if (!gpio_is_valid(hall_sensor_gpio)) {
		LID_ERR("Invalid GPIO %d\n", hall_sensor_gpio);
		goto fail_create;
	}

	ret = gpio_request(hall_sensor_gpio, "LID");
	if (ret < 0) {
		LID_ERR("Failed to request GPIO %d\n",
				hall_sensor_gpio);
		goto fail_create;
	}

	ret = gpio_direction_input(hall_sensor_gpio);
	if (ret < 0) {
		LID_ERR(
		"Failed to configure direction for GPIO %d\n",
			hall_sensor_gpio);
		goto fail_free;
	}

	irq = gpio_to_irq(hall_sensor_gpio);
	hall_sensor_irq = irq;
	if (irq < 0) {
		LID_ERR("Unable to get irq number for GPIO %d\n",
				hall_sensor_gpio);
		goto fail_free;
	}

	irqflags = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING;
	ret = request_any_context_irq(irq, lid_interrupt_handler,
					irqflags, "hall_sensor",
					lid_indev);

	if (ret < 0) {
		LID_ERR("Unable to claim irq %d\n", irq);
		goto fail_free;
	}
	device_init_wakeup(&pdev->dev, 1);
	enable_irq_wake(irq);

	INIT_DELAYED_WORK_DEFERRABLE(&lid_hall_sensor_work,
					lid_report_function);

	return ret;

fail_free:
	gpio_free(hall_sensor_gpio);

fail_create:
	sysfs_remove_group(&pdev->dev.kobj, &lid_attr_group);

fail_sysfs:
	return ret;
}

static int __devexit lid_driver_remove(struct platform_device *pdev)
{
	sysfs_remove_group(&pdev->dev.kobj, &lid_attr_group);
	free_irq(hall_sensor_irq, NULL);
	cancel_delayed_work_sync(&lid_hall_sensor_work);
	if (gpio_is_valid(hall_sensor_gpio))
		gpio_free(hall_sensor_gpio);

	input_unregister_device(lid_indev);
	device_init_wakeup(&pdev->dev, 0);

	return 0;
}

static struct platform_driver asustek_lid_driver __refdata = {
	.probe = lid_driver_probe,
	.remove = __devexit_p(lid_driver_remove),
	.driver = {
		.name = "asustek_lid",
		.owner = THIS_MODULE,
	},
};

module_platform_driver(asustek_lid_driver);
MODULE_DESCRIPTION("Hall Sensor Driver");
MODULE_LICENSE("GPL");
