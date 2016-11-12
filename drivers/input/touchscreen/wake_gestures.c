/*
 * drivers/input/touchscreen/wake_gestures.c
 *
 *
 * Copyright (c) 2013, Dennis Rassmann <showp1984@gmail.com>
 * Copyright (c) 2013-16 Aaron Segaert <asegaert@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/wake_gestures.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/input.h>
#include <linux/hrtimer.h>
#include <asm-generic/cputime.h>
#include <linux/wakelock.h>

/* Tunables */
#define WG_DEBUG		0
#define WG_DEFAULT		0
#define S2W_DEFAULT		4
#define S2S_DEFAULT		2
#define WG_PWRKEY_DUR           180

/* marlin */
#define SWEEP_Y_MAX             2560
#define SWEEP_X_MAX             1440
#define SWEEP_EDGE		130
#define SWEEP_Y_LIMIT           SWEEP_Y_MAX-SWEEP_EDGE
#define SWEEP_X_LIMIT           SWEEP_X_MAX-SWEEP_EDGE
#define SWEEP_X_B1              532
#define SWEEP_X_B2              960
#define SWEEP_Y_START		1066
#define SWEEP_X_START		720
#define SWEEP_X_FINAL           360
#define SWEEP_Y_NEXT            180

/* Wake Gestures */
#define SWEEP_TIMEOUT		90
#define TRIGGER_TIMEOUT		150
#define DT2W_FEATHER		150
#define DT2W_TIME 		150
#define WAKE_GESTURE		0x0b
#define SWEEP_RIGHT		0x01
#define SWEEP_LEFT		0x02
#define SWEEP_UP		0x04
#define SWEEP_DOWN		0x08
#define VIB_STRENGTH 		20
#define WAKE_GESTURES_ENABLED	1

#define LOGTAG			"WG"

#if (WAKE_GESTURES_ENABLED)
int gestures_switch = WG_DEFAULT;
static struct input_dev *gesture_dev;
#endif

/* Resources */
int s2w_switch = S2W_DEFAULT;
int s2w_switch_temp; 
bool dt2w_switch;
static int s2s_switch = S2S_DEFAULT;
static int touch_x = 0, touch_y = 0;
static bool touch_x_called = false, touch_y_called = false;
static bool exec_countx = true, exec_county = true, exec_count = true;
static bool barrierx[2] = {false, false}, barriery[2] = {false, false};
static int firstx = 0, firsty = 0;
static unsigned long firstx_time = 0, firsty_time = 0;
static unsigned long pwrtrigger_time[2] = {0, 0};
static unsigned long long tap_time_pre = 0;
static int touch_nr = 0, x_pre = 0, y_pre = 0;
static bool touch_cnt = true;
int vib_strength = VIB_STRENGTH;

static unsigned int sweep_y_limit = SWEEP_Y_LIMIT;
static unsigned int sweep_x_limit = SWEEP_X_LIMIT;
static unsigned int sweep_x_b1 = SWEEP_X_B1;
static unsigned int sweep_x_b2 = SWEEP_X_B2;
static unsigned int sweep_y_start = SWEEP_Y_START;
static unsigned int sweep_x_start = SWEEP_X_START;
static unsigned int sweep_x_final = SWEEP_X_FINAL;
static unsigned int sweep_y_next = SWEEP_Y_NEXT;
static unsigned int sweep_x_max = SWEEP_X_MAX;
static unsigned int sweep_edge = SWEEP_EDGE;

static struct input_dev * wake_dev;
static DEFINE_MUTEX(pwrkeyworklock);
static struct workqueue_struct *s2w_input_wq;
static struct workqueue_struct *dt2w_input_wq;
static struct work_struct s2w_input_work;
static struct work_struct dt2w_input_work;
static struct wake_lock dt2w_wakelock;

void wg_setdev(struct input_dev * input_device) {
	wake_dev = input_device;
	return;
}

//get hardware type
static int __init get_model(char *cmdline_model)
{
	

	return 0;
}
__setup("androidboot.hardware=", get_model);

/* Wake Gestures */
#if (WAKE_GESTURES_ENABLED)
static void report_gesture(int gest)
{
	pwrtrigger_time[1] = pwrtrigger_time[0];
	pwrtrigger_time[0] = jiffies;	

	if (pwrtrigger_time[0] - pwrtrigger_time[1] < TRIGGER_TIMEOUT)
		return;

	wake_lock_timeout(&dt2w_wakelock, 150);
	input_report_rel(gesture_dev, WAKE_GESTURE, gest);
	input_sync(gesture_dev);
}
#endif

/* PowerKey work func */
static void wake_presspwr(struct work_struct * wake_presspwr_work) {
	if (!mutex_trylock(&pwrkeyworklock))
        	return;

	input_event(wake_dev, EV_KEY, KEY_POWER, 1);
	input_event(wake_dev, EV_SYN, 0, 0);
	msleep(WG_PWRKEY_DUR);
	input_event(wake_dev, EV_KEY, KEY_POWER, 0);
	input_event(wake_dev, EV_SYN, 0, 0);
	msleep(WG_PWRKEY_DUR);
    	mutex_unlock(&pwrkeyworklock);

	return;
}
static DECLARE_WORK(wake_presspwr_work, wake_presspwr);

/* PowerKey trigger */
static void wake_pwrtrigger(void) {

	pwrtrigger_time[1] = pwrtrigger_time[0];
	pwrtrigger_time[0] = jiffies;
	
	if (pwrtrigger_time[0] - pwrtrigger_time[1] < TRIGGER_TIMEOUT)
		return;

	set_vibrate(vib_strength);
	schedule_work(&wake_presspwr_work);
        return;
}


/* Doubletap2wake */

static void doubletap2wake_reset(void) {
	if (wake_lock_active(&dt2w_wakelock))
		wake_unlock(&dt2w_wakelock);
	exec_count = true;
	touch_nr = 0;
	tap_time_pre = 0;
	x_pre = 0;
	y_pre = 0;
}

static unsigned int calc_feather(int coord, int prev_coord) {
	int calc_coord = 0;
	calc_coord = coord-prev_coord;
	if (calc_coord < 0)
		calc_coord = calc_coord * (-1);
	return calc_coord;
}

/* init a new touch */
static void new_touch(int x, int y) {
	tap_time_pre = jiffies;
	x_pre = x;
	y_pre = y;
	touch_nr++;
	wake_lock_timeout(&dt2w_wakelock, 150);
}

/* Doubletap2wake main function */
static void detect_doubletap2wake(int x, int y, bool st)
{
        bool single_touch = st;
#if WG_DEBUG
        pr_info(LOGTAG"x,y(%4d,%4d) tap_time_pre:%llu\n",
                x, y, tap_time_pre);
#endif
	if (x < sweep_edge || x > sweep_x_limit)
       		return;
	if (y < sweep_edge || y > sweep_y_limit)
       		return;

	if ((single_touch) && (dt2w_switch) && (exec_count) && (touch_cnt)) {
		touch_cnt = false;
		if (touch_nr == 0) {
			new_touch(x, y);
		} else if (touch_nr == 1) {
			if ((calc_feather(x, x_pre) < DT2W_FEATHER) &&
			    (calc_feather(y, y_pre) < DT2W_FEATHER) &&
			    ((jiffies-tap_time_pre) < DT2W_TIME))
				touch_nr++;
			else {
				doubletap2wake_reset();
				new_touch(x, y);
			}
		} else {
			doubletap2wake_reset();
			new_touch(x, y);
		}
		if ((touch_nr > 1)) {
			exec_count = false;
#if (WAKE_GESTURES_ENABLED)
			if (gestures_switch) {
				report_gesture(5);
			} else {
#endif
				wake_pwrtrigger();
#if (WAKE_GESTURES_ENABLED)
			}
#endif
			doubletap2wake_reset();
		}
	}
}


/* Sweep2wake/Sweep2sleep */

static void sweep2wake_reset(void) {

	exec_countx = true;
	barrierx[0] = false;
	barrierx[1] = false;
	firstx = 0;
	firstx_time = 0;

	exec_county = true;
	barriery[0] = false;
	barriery[1] = false;
	firsty = 0;
	firsty_time = 0;
}

/* Sweep2wake main functions*/

static void detect_sweep2wake_v(int x, int y, bool st)
{
	int prevy = 0, nexty = 0;
        bool single_touch = st;

	if (firsty == 0) {
		firsty = y;
		firsty_time = jiffies;
	}

#if WG_DEBUG
        pr_info(LOGTAG"s2w vert  x,y(%4d,%4d) single:%s\n",
                x, y, (single_touch) ? "true" : "false");
#endif

	//sweep up
	if (firsty > sweep_y_start && single_touch && s2w_switch & SWEEP_UP) {
		prevy = firsty;
		nexty = prevy - sweep_y_next;
		if (barriery[0] == true || (y < prevy && y > nexty)) {
			prevy = nexty;
			nexty -= sweep_y_next;
			barriery[0] = true;
			if (barriery[1] == true || (y < prevy && y > nexty)) {
				prevy = nexty;
				barriery[1] = true;
				if (y < prevy) {
					if (y < (nexty - sweep_y_next)) {
						if (exec_county && (jiffies - firsty_time < SWEEP_TIMEOUT)) {
#if (WAKE_GESTURES_ENABLED)
							if (gestures_switch) {
								report_gesture(3);
							} else {
#endif
								wake_pwrtrigger();
#if (WAKE_GESTURES_ENABLED)
							}		
#endif								
							exec_county = false;
						}
					}
				}
			}
		}
	//sweep down
	} else if (firsty <= sweep_y_start && single_touch && s2w_switch & SWEEP_DOWN) {
		prevy = firsty;
		nexty = prevy + sweep_y_next;
		if (barriery[0] == true || (y > prevy && y < nexty)) {
			prevy = nexty;
			nexty += sweep_y_next;
			barriery[0] = true;
			if (barriery[1] == true || (y > prevy && y < nexty)) {
				prevy = nexty;
				barriery[1] = true;
				if (y > prevy) {
					if (y > (nexty + sweep_y_next)) {
						if (exec_county && (jiffies - firsty_time < SWEEP_TIMEOUT)) {
#if (WAKE_GESTURES_ENABLED)
							if (gestures_switch) {
								report_gesture(4);
							} else {
#endif
								wake_pwrtrigger();
#if (WAKE_GESTURES_ENABLED)
							}								
#endif
							exec_county = false;
						}
					}
				}
			}
		}
	}
	
}

static void detect_sweep2wake_h(int x, int y, bool st, bool scr_suspended)
{
        int prevx = 0, nextx = 0;
        bool single_touch = st;

	if (!scr_suspended && y < sweep_y_limit) {
		sweep2wake_reset();
		return;
	}

	if (firstx == 0) {
		firstx = x;
		firstx_time = jiffies;
	}

#if WG_DEBUG
        pr_info(LOGTAG"s2w Horz x,y(%4d,%4d) wake:%s\n",
                x, y, (scr_suspended) ? "true" : "false");
#endif
	//left->right
	if (firstx < sweep_x_start && single_touch &&
			((scr_suspended && (s2w_switch & SWEEP_RIGHT)) ||
			(!scr_suspended && (s2s_switch & SWEEP_RIGHT)))) {
		prevx = 0;
		nextx = sweep_x_b1;
		if ((barrierx[0] == true) ||
		   ((x > prevx) && (x < nextx))) {
			prevx = nextx;
			nextx = sweep_x_b2;
			barrierx[0] = true;
			if ((barrierx[1] == true) ||
			   ((x > prevx) && (x < nextx))) {
				prevx = nextx;
				barrierx[1] = true;
				if (x > prevx) {
					if (x > (sweep_x_max - sweep_x_final)) {
						if (exec_countx && (jiffies - firstx_time < SWEEP_TIMEOUT)) {
#if (WAKE_GESTURES_ENABLED)
							if (gestures_switch && scr_suspended) {
								report_gesture(1);
							} else {
#endif
								wake_pwrtrigger();
#if (WAKE_GESTURES_ENABLED)
							}
#endif							
							exec_countx = false;
						}
					}
				}
			}
		}
	//right->left
	} else if (firstx >= sweep_x_start && single_touch &&
			((scr_suspended && (s2w_switch & SWEEP_LEFT)) ||
			(!scr_suspended && (s2s_switch & SWEEP_LEFT)))) {
		prevx = (sweep_x_max - sweep_x_final);
		nextx = sweep_x_b2;
		if ((barrierx[0] == true) ||
		   ((x < prevx) && (x > nextx))) {
			prevx = nextx;
			nextx = sweep_x_b1;
			barrierx[0] = true;
			if ((barrierx[1] == true) ||
			   ((x < prevx) && (x > nextx))) {
				prevx = nextx;
				barrierx[1] = true;
				if (x < prevx) {
					if (x < sweep_x_final) {
						if (exec_countx) {
#if (WAKE_GESTURES_ENABLED)
							if (gestures_switch && scr_suspended) {
								report_gesture(2);
							} else {
#endif
								wake_pwrtrigger();
#if (WAKE_GESTURES_ENABLED)
							}		
#endif							
							exec_countx = false;
						}
					}
				}
			}
		}
	}
}

static void s2w_input_callback(struct work_struct *unused)
{
	detect_sweep2wake_h(touch_x, touch_y, true, scr_suspended());
	if (scr_suspended())
		detect_sweep2wake_v(touch_x, touch_y, true);

	return;
}

static void dt2w_input_callback(struct work_struct *unused)
{

	if (scr_suspended() && s2w_switch > 0 && dt2w_switch)
		detect_doubletap2wake(touch_x, touch_y, true);
	return;
}

static void wg_input_event(struct input_handle *handle, unsigned int type,
				unsigned int code, int value)
{

	if (scr_suspended() && code == ABS_MT_POSITION_X) {
		value -= 5000;
	}

#if WG_DEBUG
	pr_info("wg: code: %s|%u, val: %i\n",
		((code==ABS_MT_POSITION_X) ? "X" :
		(code==ABS_MT_POSITION_Y) ? "Y" :
		(code==ABS_MT_TRACKING_ID) ? "ID" :
		"undef"), code, value);
#endif
	if (code == ABS_MT_SLOT) {
		sweep2wake_reset();
		doubletap2wake_reset();
		return;
	}

	if (code == ABS_MT_TRACKING_ID && value == -1) {
		sweep2wake_reset();
		touch_cnt = true;
		queue_work_on(0, dt2w_input_wq, &dt2w_input_work);
		return;
	}

	if (code == ABS_MT_POSITION_X) {
		touch_x = value;
		touch_x_called = true;
	}

	if (code == ABS_MT_POSITION_Y) {
		touch_y = value;
		touch_y_called = true;
	}

	if (touch_x_called && touch_y_called) {
		touch_x_called = false;
		touch_y_called = false;
		queue_work_on(0, s2w_input_wq, &s2w_input_work);
	} else if (!scr_suspended() && touch_x_called && !touch_y_called) {
		touch_x_called = false;
		touch_y_called = false;
		queue_work_on(0, s2w_input_wq, &s2w_input_work);
	}
}

static int input_dev_filter(struct input_dev *dev) {
	if (strstr(dev->name, "synaptics_dsx")) {
		return 0;
	} else {
		return 1;
	}
	return 0;
}

static int wg_input_connect(struct input_handler *handler,
				struct input_dev *dev, const struct input_device_id *id) {
	struct input_handle *handle;
	int error;

	if (input_dev_filter(dev))
		return -ENODEV;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "wg";

	error = input_register_handle(handle);
	if (error)
		goto err2;

	error = input_open_device(handle);
	if (error)
		goto err1;

	return 0;
err1:
	input_unregister_handle(handle);
err2:
	kfree(handle);
	return error;
}

static void wg_input_disconnect(struct input_handle *handle) {
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id wg_ids[] = {
	{ .driver_info = 1 },
	{ },
};

static struct input_handler wg_input_handler = {
	.event		= wg_input_event,
	.connect	= wg_input_connect,
	.disconnect	= wg_input_disconnect,
	.name		= "wg_inputreq",
	.id_table	= wg_ids,
};


/*
 * SYSFS stuff below here
 */
static ssize_t sweep2wake_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	size_t count = 0;

	count += sprintf(buf, "%d\n", s2w_switch);

	return count;
}

static ssize_t sweep2wake_dump(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	sscanf(buf, "%d ", &s2w_switch_temp);
	if (s2w_switch_temp < 0 || s2w_switch_temp > 15)
		s2w_switch_temp = 0;
		
	if (s2w_switch_temp == 0)
		set_internal_dt(dt2w_switch);
	else {
		set_internal_dt(false);
	}

	if (!scr_suspended())
		s2w_switch = s2w_switch_temp;

	return count;
}

static DEVICE_ATTR(sweep2wake, (S_IWUSR|S_IRUGO),
	sweep2wake_show, sweep2wake_dump);

static ssize_t sweep2sleep_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	size_t count = 0;
	count += sprintf(buf, "%d\n", s2s_switch);
	return count;
}

static ssize_t sweep2sleep_dump(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	sscanf(buf, "%d ", &s2s_switch);
	if (s2s_switch < 0 || s2s_switch > 3)
		s2s_switch = 0;				
				
	return count;
}

static DEVICE_ATTR(sweep2sleep, (S_IWUSR|S_IRUGO),
	sweep2sleep_show, sweep2sleep_dump);

static ssize_t doubletap2wake_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	size_t count = 0;

	count += sprintf(buf, "%d\n", dt2w_switch);

	return count;
}

static ssize_t doubletap2wake_dump(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{

	int input;
	sscanf(buf, "%d ", &input);
	if (input < 0 || input > 1)
		input = 0;	

	dt2w_switch = (input) ? true : false;		
	
	if (s2w_switch == 0 || s2w_switch_temp == 0)
		set_internal_dt(dt2w_switch);

	return count;
}

static DEVICE_ATTR(doubletap2wake, (S_IWUSR|S_IRUGO),
	doubletap2wake_show, doubletap2wake_dump);
	
#if (WAKE_GESTURES_ENABLED)
static ssize_t wake_gestures_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	size_t count = 0;
	count += sprintf(buf, "%d\n", gestures_switch);
	return count;
}
static ssize_t wake_gestures_dump(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	sscanf(buf, "%d ", &gestures_switch);
	if (gestures_switch < 0 || gestures_switch > 1)
		gestures_switch = 0;	
	return count;
}

static DEVICE_ATTR(wake_gestures, (S_IWUSR|S_IRUGO),
	wake_gestures_show, wake_gestures_dump);
#endif	

static ssize_t vib_strength_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	size_t count = 0;
	count += sprintf(buf, "%d\n", vib_strength);
	return count;
}

static ssize_t vib_strength_dump(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	sscanf(buf, "%d ",&vib_strength);
	if (vib_strength < 0 || vib_strength > 90)
		vib_strength = 20;

	return count;
}

static DEVICE_ATTR(vib_strength, (S_IWUSR|S_IRUGO),
	vib_strength_show, vib_strength_dump);


/*
 * INIT / EXIT stuff below here
 */

extern struct kobject *android_touch_kobj;

static int __init wake_gestures_init(void)
{
	int rc = 0;

	rc = input_register_handler(&wg_input_handler);
	if (rc)
		pr_err("%s: Failed to register wg_input_handler\n", __func__);

	s2w_input_wq = create_workqueue("s2wiwq");
	if (!s2w_input_wq) {
		pr_err("%s: Failed to create s2wiwq workqueue\n", __func__);
		return -EFAULT;
	}
	INIT_WORK(&s2w_input_work, s2w_input_callback);
		
	dt2w_input_wq = create_workqueue("dt2wiwq");
	if (!dt2w_input_wq) {
		pr_err("%s: Failed to create dt2wiwq workqueue\n", __func__);
		return -EFAULT;
	}
	INIT_WORK(&dt2w_input_work, dt2w_input_callback);
		
	wake_lock_init(&dt2w_wakelock, WAKE_LOCK_SUSPEND, "dt2w_wakelock");
		
	//dt2w_switch = get_internal_dt();
		
#if (WAKE_GESTURES_ENABLED)
	gesture_dev = input_allocate_device();
	if (!gesture_dev) {
		pr_err("Can't allocate gesture device\n");
		goto err_alloc_dev;
	}
	
	gesture_dev->name = "wake_gesture";
	gesture_dev->phys = "wake_gesture/input0";
	input_set_capability(gesture_dev, EV_REL, WAKE_GESTURE);

	rc = input_register_device(gesture_dev);
	if (rc) {
		pr_err("%s: input_register_device err=%d\n", __func__, rc);
		goto err_gesture_dev;
	}
#endif

	rc = sysfs_create_file(android_touch_kobj, &dev_attr_sweep2wake.attr);
	if (rc) {
		pr_warn("%s: sysfs_create_file failed for sweep2wake\n", __func__);
	}
	rc = sysfs_create_file(android_touch_kobj, &dev_attr_sweep2sleep.attr);
	if (rc) {
		pr_warn("%s: sysfs_create_file failed for sweep2sleep\n", __func__);
	}
		rc = sysfs_create_file(android_touch_kobj, &dev_attr_doubletap2wake.attr);
	if (rc) {
		pr_warn("%s: sysfs_create_file failed for doubletap2wake\n", __func__);
	}
	rc = sysfs_create_file(android_touch_kobj, &dev_attr_vib_strength.attr);
	if (rc) {
		pr_warn("%s: sysfs_create_file failed for vib_strength\n", __func__);
	}
#if (WAKE_GESTURES_ENABLED)
	rc = sysfs_create_file(android_touch_kobj, &dev_attr_wake_gestures.attr);
	if (rc) {
		pr_warn("%s: sysfs_create_file failed for wake_gestures\n", __func__);
	}

	return 0;

err_gesture_dev:
	input_free_device(gesture_dev);
err_alloc_dev:
#endif

	return 0;
}

static void __exit wake_gestures_exit(void)
{
	input_unregister_handler(&wg_input_handler);
	destroy_workqueue(s2w_input_wq);
	destroy_workqueue(dt2w_input_wq);
	input_free_device(wake_dev);
	wake_lock_destroy(&dt2w_wakelock);
#if (WAKE_GESTURES_ENABLED)	
	input_unregister_device(gesture_dev);
	input_free_device(gesture_dev);
#endif

	return;
}

module_init(wake_gestures_init);
module_exit(wake_gestures_exit);
