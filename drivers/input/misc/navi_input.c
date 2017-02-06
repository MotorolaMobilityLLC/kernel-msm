#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/input/mt.h>
#include <linux/printk.h>

#include "et320.h"
#include "navi_input.h"


enum {
	KEY_RELEASE,
	KEY_PRESS,
	KEY_PRESS_RELEASE
};

#define	DISABLE		0
#define	ENABLE		1

static struct task_struct *nav_kthread;
static DECLARE_WAIT_QUEUE_HEAD(nav_input_wait);
static unsigned int nav_input_sig;
static DEFINE_MUTEX(driver_mode_lock);
struct navi_cmd_struct {
	char cmd;
	struct list_head list;
};

struct navi_cmd_struct cmd_list;

/*****************************************************************
*                                                                *
*                         Configuration                          *
*                                                                *
*****************************************************************/


/*
 * @ ENABLE_SWIPE_UP_DOWN
 *     ENABLE : Listening to swipe-up & swipe-down navigation events.
 *              Configure ENABLE_SWIPE_UP_DOWN properties below.
 *
 *     DISABLE : Ignore swipe-up & swipe-down navigation events.
 *               Don't care properties.
 *
 * @ ENABLE_SWIPE_LEFT_RIGHT
 *     ENABLE : Listening to swipe-left & swipe-right navigation events.
 *              configure ENABLE_SWIPE_LEFT_RIGHT properties below.
 *
 *     DISABLE : Ignore swipe-left & swipe-right navigation events.
 *               Don't care properties.
 */
#define ENABLE_SWIPE_UP_DOWN	DISABLE
#define ENABLE_SWIPE_LEFT_RIGHT	ENABLE
#define ENABLE_FINGER_DOWN_UP	ENABLE
#define KEY_FPS_DOWN   614
#define KEY_FPS_UP     615
#define KEY_FPS_TAP    616
#define KEY_FPS_HOLD   617
#define KEY_FPS_YPLUS  618
#define KEY_FPS_YMINUS 619
#define KEY_FPS_XPLUS  620
#define KEY_FPS_XMINUS 621


/*
 * ENABLE_SWIPE_UP_DOWN properties
 *
 * If ENABLE_SWIPE_UP_DOWN set to DISABLE, these should neglected
 *
 *
 * @ KEYEVENT_UP : The key-event should be sent when swipe-up.
 * @ KEYEVENT_UP_ACTION : Action of KEYEVENT_UP.
 *
 * @ KEYEVENT_DOWN : The key-event should be sent when swipe-down.
 * @ KEYEVENT_DOWN_ACTION : Action of KEYEVENT_UP.
 *
 * @ ACTION:
 *   KEY_PRESS : Press key button
 *   KEY_RELEASE : Release key button
 *   KEY_PRESS_RELEASE : Combined action of press-then-release
 */
#define	KEYEVENT_UP				KEY_FPS_XMINUS /*KEY_UP*/
#define	KEYEVENT_UP_ACTION		KEY_PRESS_RELEASE
#define	KEYEVENT_DOWN			KEY_FPS_XPLUS /*KEY_DOWN*/
#define	KEYEVENT_DOWN_ACTION	KEY_PRESS_RELEASE


/*
 *ENABLE_SWIPE_LEFT_RIGHT properties.
 *
 * If ENABLE_SWIPE_LEFT_RIGHT set to DISABLE, these should neglected
 *
 *
 * @ KEYEVENT_RIGHT : The key-event should be sent when swipe-right.
 * @ KEYEVENT_RIGHT_ACTION : Action of KEYEVENT_RIGHT.
 *
 * @ KEYEVENT_LEFT : The key-event should be sent when swipe-left.
 * @ KEYEVENT_LEFT_ACTION : Action of KEYEVENT_LEFT.
 *
 * @ ACTION:
 *   KEY_PRESS : Press key button
 *   KEY_RELEASE : Release key button
 *   KEY_PRESS_RELEASE : Combined action of press-then-release
 */
#define	KEYEVENT_RIGHT			KEY_FPS_YMINUS /* KEY_RIGHT */
#define	KEYEVENT_RIGHT_ACTION	KEY_PRESS_RELEASE
#define	KEYEVENT_LEFT			KEY_FPS_YPLUS  /* KEY_LEFT */
#define	KEYEVENT_LEFT_ACTION	KEY_PRESS_RELEASE
#if ENABLE_FINGER_DOWN_UP
unsigned int prev_keycode = 0;
#endif
/*
 * @ TRANSLATED_COMMAND
 *     ENABLE : TRANSLATED command. Navigation events will be translated to
 *              logical user-events. e.g. click, double-click, long-click
 *              Configure TRANSLATED properties.
 *
 *     DISABLE : STRAIGHT command. Navigation events will be sent one-by-one
 *               directly.
 *               Configure STRAIGHT properties.
 */
#define	TRANSLATED_COMMAND		ENABLE




#if TRANSLATED_COMMAND

/*-------------------TRANSLATED properties---------------------*/

/*
 * @ ENABLE_TRANSLATED_SINGLE_CLICK
 *     ENABLE/DISABLE : enable/disable single-click event.
 *
 * @ ENABLE_TRANSLATED_DOUBLE_CLICK
 *     ENABLE/DISABLE : enable/disable double-click event.
 *
 * @ ENABLE_TRANSLATED_LONG_TOUCH
 *     ENABLE/DISABLE : enable/disable long-touch event.
 */
#define ENABLE_TRANSLATED_SINGLE_CLICK	ENABLE
#define ENABLE_TRANSLATED_DOUBLE_CLICK	DISABLE
#define ENABLE_TRANSLATED_LONG_TOUCH	ENABLE


/*
 * @ LONGTOUCH_INTERVAL : Minimum time finger stay-on that counted to long-touch.
 *     Only concerned while ENABLE_TRANSLATED_LONG_TOUCH set to ENABLE.
 *     In millisecond (ms)
 *
 * @ DOUBLECLICK_INTERVAL : Maximum time between two click that counted to double-click.
 *     Only concerned while ENABLE_TRANSLATED_DOUBLE_CLICK set to ENABLE.
 *     In millisecond (ms)
 *
 * @ KEYEVENT_CLICK : The key-event should be sent when single-click.
 * @ KEYEVENT_CLICK_ACTION : Action of KEYEVENT_CLICK.
 *     Only concerned while ENABLE_TRANSLATED_SINGLE_CLICK set to ENABLE.
 *
 * @ KEYEVENT_DOUBLECLICK : The key-event should be sent when double-click.
 * @ KEYEVENT_DOUBLECLICK_ACTION : Action of KEYEVENT_DOUBLECLICK.
 *     Only concerned while ENABLE_TRANSLATED_DOUBLE_CLICK set to ENABLE.
 *
 * @ KEYEVENT_LONGTOUCH : The key-event should be sent when long-touch.
 * @ KEYEVENT_LONGTOUCH_ACTION : Action of KEYEVENT_LONGTOUCH.
 *     Only concerned while ENABLE_TRANSLATED_LONG_TOUCH set to ENABLE.
 *
 * @ ACTION:
 *   KEY_PRESS : Press key button
 *   KEY_RELEASE : Release key button
 *   KEY_PRESS_RELEASE : Combined action of press-then-release
 */
#define LONGTOUCH_INTERVAL          400
#define DOUBLECLICK_INTERVAL        500
#define	KEYEVENT_CLICK              KEY_FPS_TAP /* 0x232 */
#define	KEYEVENT_CLICK_ACTION       KEY_PRESS_RELEASE
#define	KEYEVENT_DOUBLECLICK        KEY_DELETE
#define	KEYEVENT_DOUBLECLICK_ACTION KEY_PRESS_RELEASE
#define	KEYEVENT_LONGTOUCH          KEY_FPS_HOLD /* 0x233 */
#define	KEYEVENT_LONGTOUCH_ACTION   KEY_PRESS_RELEASE

#define	KEYEVENT_ON                 KEY_FPS_DOWN
#define	KEYEVENT_ON_ACTION          KEY_PRESS_RELEASE
#define	KEYEVENT_OFF                KEY_FPS_UP
#define	KEYEVENT_OFF_ACTION         KEY_PRESS_RELEASE


/*---------------End of TRANSLATED properties-----------------*/


#else	/* STRAIGHT COMMAND */


/*-------------------STRAIGHT properties----------------------*/

/*
 * @ ENABLE_STRAIGHT_CANCEL
 *     ENABLE/DISABLE : enable/disable cancel event.
 *
 * @ ENABLE_STRAIGHT_ON
 *     ENABLE/DISABLE : enable/disable finger-on event.
 *
 * @ ENABLE_STRAIGHT_OFF
 *     ENABLE/DISABLE : enable/disable finger-off event.
 */
#define ENABLE_STRAIGHT_CANCEL	ENABLE
#define ENABLE_STRAIGHT_ON		ENABLE
#define ENABLE_STRAIGHT_OFF		ENABLE


/*
 * @ KEYEVENT_CANCEL : The key-event should be sent when cancel.
 * @ KEYEVENT_CANCEL_ACTION : Action of KEYEVENT_CANCEL.
 *     Only concerned while ENABLE_STRAIGHT_CANCEL set to ENABLE.
 *
 * @ KEYEVENT_ON : The key-event should be sent when finger-on.
 * @ KEYEVENT_ON_ACTION : Action of KEYEVENT_ON.
 *     Only concerned while ENABLE_STRAIGHT_ON set to ENABLE.
 *
 * @ KEYEVENT_OFF : The key-event should be sent when long-touch.
 * @ KEYEVENT_OFF_ACTION : Action of KEYEVENT_OFF.
 *     Only concerned while ENABLE_STRAIGHT_OFF set to ENABLE.
 *
 * @ ACTION:
 *   KEY_PRESS : Press key button
 *   KEY_RELEASE : Release key button
 *   KEY_PRESS_RELEASE : Combined action of press-then-release
 */
#define	KEYEVENT_CANCEL			KEY_0
#define	KEYEVENT_CANCEL_ACTION	KEY_PRESS_RELEASE
#define	KEYEVENT_ON				KEY_EXIT
#define	KEYEVENT_ON_ACTION		KEY_PRESS
#define	KEYEVENT_OFF			KEY_EXIT
#define	KEYEVENT_OFF_ACTION		KEY_RELEASE

/*-----------------End of STRAIGHT properties-------------------*/


#endif


/****************************************************************
*                                                               *
*                      End of Configuration                     *
*                                                               *
****************************************************************/










#define PROPERTY_NAVIGATION_ENABLE_DEFAULT  true

struct navi_struct {
    char cmd;
    struct etspi_data *etspi;
    struct work_struct workq;
};

enum navi_event {
	NAVI_EVENT_CANCEL,
	NAVI_EVENT_ON,
	NAVI_EVENT_OFF,
	NAVI_EVENT_SWIPE,
	NAVI_EVENT_UP,
	NAVI_EVENT_DOWN,
	NAVI_EVENT_RIGHT,
	NAVI_EVENT_LEFT
};

static struct timer_list long_touch_timer;

static bool g_KeyEventRaised = true;
static unsigned long g_DoubleClickJiffies;


/* Set event bits according to what events we would generate */
void init_event_enable(struct etspi_data *etspi)
{
	set_bit(EV_KEY, etspi->input_dev->evbit);
	set_bit(EV_SYN, etspi->input_dev->evbit);
#if TRANSLATED_COMMAND
	set_bit(KEYEVENT_ON, etspi->input_dev->keybit);
	set_bit(KEYEVENT_OFF, etspi->input_dev->keybit);
	set_bit(KEYEVENT_CLICK, etspi->input_dev->keybit);
	set_bit(KEYEVENT_DOUBLECLICK, etspi->input_dev->keybit);
	set_bit(KEYEVENT_LONGTOUCH, etspi->input_dev->keybit);
	set_bit(KEYEVENT_UP, etspi->input_dev->keybit);
	set_bit(KEYEVENT_DOWN, etspi->input_dev->keybit);
	set_bit(KEYEVENT_RIGHT, etspi->input_dev->keybit);
	set_bit(KEYEVENT_LEFT, etspi->input_dev->keybit);
#else
	set_bit(KEYEVENT_CANCEL, etspi->input_dev->keybit);
	set_bit(KEYEVENT_ON, etspi->input_dev->keybit);
	set_bit(KEYEVENT_OFF, etspi->input_dev->keybit);
	set_bit(KEYEVENT_UP, etspi->input_dev->keybit);
	set_bit(KEYEVENT_DOWN, etspi->input_dev->keybit);
	set_bit(KEYEVENT_RIGHT, etspi->input_dev->keybit);
	set_bit(KEYEVENT_LEFT, etspi->input_dev->keybit);
#endif
}


static void send_key_event(struct etspi_data *etspi, unsigned int code, int value)
{
	struct etspi_data *obj = etspi;

	if (value == KEY_PRESS_RELEASE) {
		input_report_key(obj->input_dev, code, 1);	/* 1 is press */
		input_sync(obj->input_dev);
		input_report_key(obj->input_dev, code, 0);	/* 0 is release */
		input_sync(obj->input_dev);
#if ENABLE_FINGER_DOWN_UP
		prev_keycode = code;
#endif
	} else {
		input_report_key(obj->input_dev, code, value);
		input_sync(obj->input_dev);
	}

	DEBUG_PRINT("Egis navigation driver, send key event: %d, action: %d\n", code, value);
}

#if ENABLE_TRANSLATED_LONG_TOUCH
static void long_touch_handler(unsigned long arg)
{
	struct etspi_data *etspi = (struct etspi_data *)arg;

	if (g_KeyEventRaised == false) {
		g_KeyEventRaised = true;
		/* Long touch event */
		send_key_event(etspi, KEYEVENT_LONGTOUCH, KEYEVENT_LONGTOUCH_ACTION);
	}
}
#endif


#if TRANSLATED_COMMAND
void translated_command_converter(char cmd, struct etspi_data *etspi)
{
	DEBUG_PRINT("Egis navigation driver, translated cmd: %d\n", cmd);

	switch (cmd) {
	case NAVI_EVENT_CANCEL:
		g_KeyEventRaised = true;
		g_DoubleClickJiffies = 0;
#if ENABLE_TRANSLATED_LONG_TOUCH
		del_timer(&long_touch_timer);
#endif
		break;

	case NAVI_EVENT_ON:
		g_KeyEventRaised = false;
#if ENABLE_FINGER_DOWN_UP
		send_key_event(etspi, KEYEVENT_ON, KEYEVENT_ON_ACTION);
#endif
#if ENABLE_TRANSLATED_LONG_TOUCH
		long_touch_timer.data = (unsigned long)etspi;
		mod_timer(&long_touch_timer, jiffies + (HZ * LONGTOUCH_INTERVAL / 1000));
#endif
		break;

	case NAVI_EVENT_OFF:
		if (g_KeyEventRaised == false) {
			g_KeyEventRaised = true;
#if ENABLE_TRANSLATED_DOUBLE_CLICK
			if ((jiffies - g_DoubleClickJiffies) < (HZ * DOUBLECLICK_INTERVAL / 1000)) {
				/* Double click event */
				send_key_event(etspi, KEYEVENT_DOUBLECLICK, KEYEVENT_DOUBLECLICK_ACTION);
				g_DoubleClickJiffies = 0;
			} else {
#if ENABLE_TRANSLATED_SINGLE_CLICK
				/* Click event */
				send_key_event(etspi, KEYEVENT_CLICK, KEYEVENT_CLICK_ACTION);
#endif
				g_DoubleClickJiffies = jiffies;
			}
#else

#if ENABLE_TRANSLATED_SINGLE_CLICK
			/* Click event */
			send_key_event(etspi, KEYEVENT_CLICK, KEYEVENT_CLICK_ACTION);
#endif

#endif	/* end of ENABLE_DOUBLE_CLICK */
		}
#if ENABLE_FINGER_DOWN_UP
		else	{
			if (prev_keycode == KEYEVENT_LONGTOUCH)
				send_key_event(etspi, KEYEVENT_OFF, KEYEVENT_OFF_ACTION);
		}
#endif
#if ENABLE_TRANSLATED_LONG_TOUCH
		del_timer(&long_touch_timer);
#endif
		break;

	case NAVI_EVENT_UP:
		if (g_KeyEventRaised == false) {
			g_KeyEventRaised = true;
#if ENABLE_SWIPE_UP_DOWN
			send_key_event(etspi, KEYEVENT_UP, KEYEVENT_UP_ACTION);
#endif
		}
		break;

	case NAVI_EVENT_DOWN:

		if (g_KeyEventRaised == false) {
			g_KeyEventRaised = true;
#if ENABLE_SWIPE_UP_DOWN
			send_key_event(etspi, KEYEVENT_DOWN, KEYEVENT_DOWN_ACTION);
#endif
		}

	break;

	case NAVI_EVENT_RIGHT:

		#if ENABLE_SWIPE_LEFT_RIGHT
		if (g_KeyEventRaised == false) {
			g_KeyEventRaised = true;
			send_key_event(etspi, KEYEVENT_RIGHT, KEYEVENT_RIGHT_ACTION);
		}
		#endif

		break;

	case NAVI_EVENT_LEFT:

		#if ENABLE_SWIPE_LEFT_RIGHT
		if (g_KeyEventRaised == false) {
			g_KeyEventRaised = true;
			send_key_event(etspi, KEYEVENT_LEFT, KEYEVENT_LEFT_ACTION);
		}
		#endif

		break;

	default:
		pr_err("Egis navigation driver, cmd not match\n");
	}
}

#else	/* straight command (not define TRANSLATED_COMMAND) */

void straight_command_converter(char cmd, struct etspi_data *etspi)
{
	DEBUG_PRINT("Egis navigation driver, straight cmd: %d\n", cmd);

	switch (cmd) {
	case NAVI_EVENT_CANCEL:

#if ENABLE_STRAIGHT_CANCEL
		send_key_event(etspi, KEYEVENT_CANCEL, KEYEVENT_CANCEL_ACTION);
#endif

		break;

	case NAVI_EVENT_ON:

#if ENABLE_STRAIGHT_ON
		send_key_event(etspi, KEYEVENT_ON, KEYEVENT_ON_ACTION);
#endif

		break;

	case NAVI_EVENT_OFF:

#if ENABLE_STRAIGHT_OFF
		send_key_event(etspi, KEYEVENT_OFF, KEYEVENT_OFF_ACTION);
#endif

		break;

	case NAVI_EVENT_UP:

#if ENABLE_SWIPE_UP_DOWN
		send_key_event(etspi, KEYEVENT_UP, KEYEVENT_UP_ACTION);
#endif

		break;

	case NAVI_EVENT_DOWN:

#if ENABLE_SWIPE_UP_DOWN
		send_key_event(etspi, KEYEVENT_DOWN, KEYEVENT_DOWN_ACTION);
#endif

		break;

	case NAVI_EVENT_RIGHT:

#if ENABLE_SWIPE_LEFT_RIGHT
		send_key_event(etspi, KEYEVENT_RIGHT, KEYEVENT_RIGHT_ACTION);
#endif

		break;

	case NAVI_EVENT_LEFT:

#if ENABLE_SWIPE_LEFT_RIGHT
		send_key_event(etspi, KEYEVENT_LEFT, KEYEVENT_LEFT_ACTION);
#endif

		break;

	default:
		pr_err("Egis navigation driver, cmd not match\n");
	}
}

#endif  /* end of TRANSLATED_COMMAND */

void navi_operator(struct work_struct *work)
{
    struct navi_struct *command = container_of(work, struct navi_struct, workq);

#if TRANSLATED_COMMAND
	translated_command_converter(command->cmd, command->etspi);
#else
	straight_command_converter(command->cmd, command->etspi);

#endif
}

static ssize_t navigation_event_func(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct etspi_data *etspi = dev_get_drvdata(dev);
	struct navi_cmd_struct *tempcmd;
	DEBUG_PRINT("Egis navigation driver, %s echo :'%d'\n", __func__, *buf);

	if (etspi) {
		dev_dbg(&etspi->spi->dev, "%s spi_show\n", __func__);
		if (etspi->spi) {
			dev_dbg(&etspi->spi->dev, "%s spi show\n", __func__);
		}
	} else
		pr_err("Egis navigation driver, etspi is NULL\n");

	if (etspi->input_dev == NULL)
		pr_err("Egis navigation driver, etspi->input_dev is NULL\n");
	tempcmd = kmalloc(sizeof(*tempcmd), GFP_KERNEL);
	if (tempcmd != NULL) {
		mutex_lock(&driver_mode_lock);
		tempcmd->cmd = *buf;
		list_add_tail(&tempcmd->list, &cmd_list.list);
		nav_input_sig = 1;
		mutex_unlock(&driver_mode_lock);
		wake_up_interruptible(&nav_input_wait);
	} else {
		pr_err("navigation_event_func kmalloc failed\n");

	}
	return count;
}
static DEVICE_ATTR(navigation_event, S_IWUSR, NULL, navigation_event_func);

static ssize_t property_navigation_enable_set(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct etspi_data *etspi = dev_get_drvdata(dev);
	pr_err("Egis navigation driver, %s echo :'%d'\n", __func__, *buf);

	if (!strncmp(buf, "enable", strlen("enable")))	{
		etspi->property_navigation_enable = 1;
	} else if (!strncmp(buf, "disable", strlen("disable")))	 {
		etspi->property_navigation_enable = 0;
	} else {
		pr_err("strcmp not match\n");
	}
	return count;
}

static ssize_t property_navigation_enable_get(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct etspi_data *etspi = dev_get_drvdata(dev);
	DEBUG_PRINT("Egis navigation driver, %s echo :'%d'\n", __func__, *buf);

	return scnprintf(buf, PAGE_SIZE, "%s",
			etspi->property_navigation_enable ? "enable":"disable");
}
static DEVICE_ATTR(navigation_enable, S_IRUSR|S_IWUSR,
				property_navigation_enable_get, property_navigation_enable_set);


/*-------------------------------------------------------------------------*/
/*
 *	Sysfs node creation
 */
static struct attribute *attributes[] = {
	&dev_attr_navigation_event.attr,
	&dev_attr_navigation_enable.attr,
	NULL
};
static const struct attribute_group attribute_group = {
	.attrs = attributes,
};


/*-------------------------------------------------------------------------*/
static int nav_input_thread(void *et_spi)
{
	struct etspi_data *etspi = et_spi;
	struct navi_cmd_struct *tempcmd, *acmd;

	set_user_nice(current, -20);
	DEBUG_PRINT("nav_input_thread enter\n");

	while (1) {
		wait_event_interruptible(nav_input_wait,
			nav_input_sig || kthread_should_stop());
		mutex_lock(&driver_mode_lock);
		list_for_each_entry_safe(acmd, tempcmd, &cmd_list.list, list) {
			translated_command_converter(acmd->cmd, etspi);
			list_del(&acmd->list);
			kfree(acmd);
		}
		nav_input_sig = 0;
		mutex_unlock(&driver_mode_lock);
		if (kthread_should_stop())
			break;
	}


	DEBUG_PRINT("nav_input_thread exit\n");

	return 0;
}


void uinput_egis_init(struct etspi_data *etspi)
{
	int error = 0;

	DEBUG_PRINT("Egis navigation driver, %s\n", __func__);

	etspi->property_navigation_enable = PROPERTY_NAVIGATION_ENABLE_DEFAULT ;
	etspi->input_dev = input_allocate_device();

	if (!etspi->input_dev) {
		pr_err("Egis navigation driver, Input_allocate_device failed.\n");
		return;
	}


	INIT_LIST_HEAD(&cmd_list.list);
	nav_input_sig = 0;
	if (!nav_kthread) {
		nav_kthread = kthread_run(nav_input_thread,
			(void *)etspi, "nav_thread");
	}
#if ENABLE_TRANSLATED_LONG_TOUCH
	init_timer(&long_touch_timer);
	long_touch_timer.function = long_touch_handler;
#endif


	etspi->input_dev->name = "uinput-egis";

	init_event_enable(etspi);

	/* Register the input device */
	error = input_register_device(etspi->input_dev);
	if (error) {
		pr_err("Egis navigation driver, Input_register_device failed.\n");
		input_free_device(etspi->input_dev);
		etspi->input_dev = NULL;
	}
}

void uinput_egis_destroy(struct etspi_data *etspi)
{
	DEBUG_PRINT("Egis navigation driver, %s\n", __func__);


#if ENABLE_TRANSLATED_LONG_TOUCH
	del_timer(&long_touch_timer);
#endif

	if (etspi->input_dev != NULL)
		input_free_device(etspi->input_dev);
	if (nav_kthread)
		kthread_stop(nav_kthread);

	nav_kthread = NULL;
}


void sysfs_egis_init(struct etspi_data *etspi)
{
	int status;

	DEBUG_PRINT("Egis navigation driver, egis_input device init\n");
	etspi->spi = platform_device_alloc("egis_input", -1);
	if (!etspi->spi) {
		pr_err("Egis navigation driver, platform_device_alloc fail\n");
		return;
	}

	status = platform_device_add(etspi->spi);
	if (status != 0) {
		pr_err("Egis navigation driver, platform_device_add fail\n");
		platform_device_put(etspi->spi);
		return;
	}

	dev_set_drvdata(&etspi->spi->dev, etspi);
	status = sysfs_create_group(&etspi->spi->dev.kobj, &attribute_group);
	if (status) {
		pr_err("Egis navigation driver, could not create sysfs\n");
		platform_device_del(etspi->spi);
		platform_device_put(etspi->spi);
		return;
	}
}

void sysfs_egis_destroy(struct etspi_data *etspi)
{
	DEBUG_PRINT("Egis navigation driver, %s\n", __func__);

	if (etspi->spi) {
		platform_device_del(etspi->spi);
		platform_device_put(etspi->spi);
	}
}
