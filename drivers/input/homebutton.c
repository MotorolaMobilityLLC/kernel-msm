#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/fb.h>

#define KEY_FPS_DOWN	614
static DEFINE_MUTEX(hb_lock);

struct homebutton_data {
	struct input_dev *hb_dev;
	struct workqueue_struct *hb_input_wq;
	struct work_struct hb_input_work;
	struct notifier_block notif;
	struct kobject *homebutton_kobj;
	bool key_down;
	bool scr_suspended;
	bool enable;
	unsigned int key;
} hb_data = {
	.enable = true,
	.key = KEY_HOME
};

static void hb_input_callback(struct work_struct *unused) {
	if (!mutex_trylock(&hb_lock))
		return;

	input_report_key(hb_data.hb_dev, hb_data.key, hb_data.key_down);
	input_sync(hb_data.hb_dev);

	mutex_unlock(&hb_lock);

	return;
}

static int input_dev_filter(struct input_dev *dev) {
	if (strstr(dev->name, "fpc1020")) {
		return 0;
	} else {
		return 1;
	}
}

static int hb_input_connect(struct input_handler *handler,
				struct input_dev *dev, const struct input_device_id *id) {
	int rc;
	struct input_handle *handle;

	if (input_dev_filter(dev))
		return -ENODEV;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "hb";

	rc = input_register_handle(handle);
	if (rc)
		goto err_input_register_handle;
	
	rc = input_open_device(handle);
	if (rc)
		goto err_input_open_device;

	return 0;

err_input_open_device:
	input_unregister_handle(handle);
err_input_register_handle:
	kfree(handle);
	return rc;
}

static bool hb_input_filter(struct input_handle *handle, unsigned int type, 
						unsigned int code, int value)
{
	if (type != EV_KEY)
		return false;

	if (!hb_data.enable || hb_data.scr_suspended)
		return false;

	if (value == 1) {
		if (code == KEY_FPS_DOWN)
			hb_data.key_down = true;
		else
			hb_data.key_down = false;
	}

	schedule_work(&hb_data.hb_input_work);

	return true;
}

static void hb_input_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id hb_ids[] = {
	{ .driver_info = 1 },
	{ },
};

static struct input_handler hb_input_handler = {
	.filter		= hb_input_filter,
	.connect	= hb_input_connect,
	.disconnect	= hb_input_disconnect,
	.name		= "hb_inputreq",
	.id_table	= hb_ids,
};

static int fb_notifier_callback(struct notifier_block *this,
				unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;

	if (evdata && evdata->data && event == FB_EVENT_BLANK) {
		blank = evdata->data;
		switch (*blank) {
			case FB_BLANK_UNBLANK:
				//display on
				hb_data.scr_suspended = false;
				break;
			case FB_BLANK_POWERDOWN:
			case FB_BLANK_HSYNC_SUSPEND:
			case FB_BLANK_VSYNC_SUSPEND:
			case FB_BLANK_NORMAL:
				//display off
				hb_data.scr_suspended = true;
				break;
		}
	}

	return NOTIFY_OK;
}

static ssize_t hb_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", hb_data.enable);
}

static ssize_t hb_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int rc;
	unsigned long input;

	rc = kstrtoul(buf, 0, &input);
	if (rc < 0)
		return -EINVAL;

	if (input < 0 || input > 1)
		input = 0;

	hb_data.enable = input;

	return count;
}

static DEVICE_ATTR(enable, (S_IWUSR | S_IRUGO),
	hb_enable_show, hb_enable_store);

static ssize_t key_show(struct device *dev,
		 struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", hb_data.key);
}

static ssize_t key_store(struct device *dev,
		 struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned long input;

	ret = kstrtoul(buf, 0, &input);
	if (ret < 0)
		return -EINVAL;

	set_bit(input, hb_data.hb_dev->keybit);
	hb_data.key = input;

	return count;
}

static DEVICE_ATTR(key, (S_IWUSR | S_IRUGO),
	key_show, key_store);

static int __init hb_init(void)
{
	int rc = 0;

	hb_data.hb_dev = input_allocate_device();
	if (!hb_data.hb_dev) {
		pr_err("Failed to allocate hb_dev\n");
		goto err_alloc_dev;
	}

	input_set_capability(hb_data.hb_dev, EV_KEY, KEY_HOME);
	set_bit(EV_KEY, hb_data.hb_dev->evbit);
	set_bit(KEY_HOME, hb_data.hb_dev->keybit);
	hb_data.hb_dev->name = "qwerty";
	hb_data.hb_dev->phys = "qwerty/input0";

	rc = input_register_device(hb_data.hb_dev);
	if (rc) {
		pr_err("%s: input_register_device err=%d\n", __func__, rc);
		goto err_input_dev;
	}

	rc = input_register_handler(&hb_input_handler);
	if (rc)
		pr_err("%s: Failed to register hb_input_handler\n", __func__);

	hb_data.hb_input_wq = create_workqueue("hb_wq");
	if (!hb_data.hb_input_wq) {
		pr_err("%s: Failed to create workqueue\n", __func__);
		return -EFAULT;
	}
	INIT_WORK(&hb_data.hb_input_work, hb_input_callback);

	hb_data.notif.notifier_call = fb_notifier_callback;
	if (fb_register_client(&hb_data.notif)) {
		rc = -EINVAL;
		goto err_alloc_dev;
	}

	hb_data.homebutton_kobj = kobject_create_and_add("homebutton", NULL) ;
	if (hb_data.homebutton_kobj == NULL) {
		pr_warn("%s: homebutton_kobj failed\n", __func__);
	}

	rc = sysfs_create_file(hb_data.homebutton_kobj, &dev_attr_enable.attr);
	if (rc)
		pr_err("%s: sysfs_create_file failed for homebutton enable\n", __func__);

	rc = sysfs_create_file(hb_data.homebutton_kobj, &dev_attr_key.attr);
	if (rc)
		pr_err("%s: sysfs_create_file failed for homebutton key\n", __func__);

err_input_dev:
	input_free_device(hb_data.hb_dev);

err_alloc_dev:
	pr_info("%s hb done\n", __func__);

	return 0;
}

static void __exit hb_exit(void)
{
	kobject_del(hb_data.homebutton_kobj);
	destroy_workqueue(hb_data.hb_input_wq);
	input_unregister_handler(&hb_input_handler);
	input_unregister_device(hb_data.hb_dev);
	input_free_device(hb_data.hb_dev);

	return;
}

module_init(hb_init);
module_exit(hb_exit);
