#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/jiffies.h>
#include <linux/hrtimer.h>
#include <linux/semaphore.h>
#include "touchx.h"

#define TYPE_B_PROTOCOL

static void touchx(int *x, int *y, unsigned char fn, unsigned char nf);
static struct kobject *touchx_kobj;
static struct work_struct touch_work;
struct hrtimer touch_residual_timer;
static int xo;
static int yo;
static int l_abs(int);
static int iv_len(int x, int y, unsigned int maxlen);
static int touchx_state;

static ssize_t sysfs_show_touchx_state(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", touchx_state);
}

static ssize_t sysfs_set_touchx_state(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	sscanf(buf, "%du", &touchx_state);
	pr_info("New touchx state = %d\n", touchx_state);
	return count;
}

static struct kobj_attribute touchx_state_attribute =
	__ATTR(touchx_state, 0666, sysfs_show_touchx_state,
			sysfs_set_touchx_state);

static void touch_notify(struct work_struct *work)
{
	int x, y;

	if (!touchxp.finger_down) {
		hrtimer_cancel(&touch_residual_timer);
		return;
	}

	mutex_lock(&touchxp.virtual_touch_mutex);
	x = xo;
	y = yo;
	touchx(&x, &y, 0, 1);

#ifdef TYPE_B_PROTOCOL
	input_mt_slot(touchxp.touch_magic_dev, 0);
	input_mt_report_slot_state(touchxp.touch_magic_dev,
				   MT_TOOL_FINGER, 1);
#endif
	input_report_abs(touchxp.touch_magic_dev, ABS_MT_POSITION_X, x);
	input_report_abs(touchxp.touch_magic_dev, ABS_MT_POSITION_Y, y);

	input_report_abs(touchxp.touch_magic_dev, ABS_MT_PRESSURE, 83);
	input_report_abs(touchxp.touch_magic_dev, ABS_MT_TOUCH_MAJOR, 83);

#ifndef TYPE_B_PROTOCOL
	input_report_abs(touchxp.touch_magic_dev, ABS_MT_TRACKING_ID, 0);
	input_mt_sync(touchxp.touch_magic_dev);
#endif
	input_sync(touchxp.touch_magic_dev);

	if (iv_len(x - xo, y - yo, 15) <= 12)
		hrtimer_cancel(&touch_residual_timer);

	mutex_unlock(&touchxp.virtual_touch_mutex);
}

static enum hrtimer_restart
touch_residual_timer_callback(struct hrtimer *timer)
{
	schedule_work(&touch_work);
	return HRTIMER_NORESTART;
}

#define MAXMV 70
#define TAPSD 5
#define TAPSV 11
int vtapsv = TAPSV;  /* note: vtapsv <= TAPSV */

static int l_abs(int i)
{
	return i * ((i > 0) - (i < 0));
}

static int iv_len(int x, int y, unsigned int maxlen)
{
	int r;
	if (maxlen > 700)
		maxlen = 700;

	for (r = 0; r < maxlen; r++)
		if ((x * x + y * y - r * r) <= 0)
			return r;

	return r;
}

int is_ld(int xv, int yv, int x, int y)
{
	static int xo;
	static int yo;
	int r = 0;

	if (!xo && !yo)
		goto done;

	if (iv_len(xv-x, yv-y, 700) < iv_len(xv-xo, yv-yo, 700))
		r = 1;

done:
	if (xo != x)
		xo = x;
	if (yo != y)
		yo = y;

	return r;
}

int ftapd(int *xp, int *yp)
{
	static int x[TAPSD];
	static int y[TAPSD];

	int xs = 0;
	int ys = 0;
	int i;
	int ws = 0;
	int w;
	static int g;

	if (!*xp && !*yp)
		return 0;

	if (*xp == 0xffff && *yp == 0xffff) {
		for (i = 0; i < TAPSD; i++) {
			x[i] = 0xffff;
			y[i] = 0xffff;
		}
		g = 100;
		return 0;
	}

	for (i = TAPSD-1; i > 0; i--) {
		x[i] = x[i-1];
		y[i] = y[i-1];
	}

	x[0] = *xp; y[0] = *yp;

	for (i = 0; i < TAPSD; i++) {
		if (x[i] == 0xffff)
			break;
		else {
			w = TAPSD-i;
			ws += w;
			xs += w*x[i];
			ys += w*y[i];
		}
	}
	*xp = xs / ws;
	*yp = ys / ws;

	g += 10;
	if (g > 200)
		g = 200;

	return g;
}

int ftapv(int *xp, int *yp)
{
	static int x[TAPSV];
	static int y[TAPSV];

	int xs = 0;
	int ys = 0;
	int i;
	int ws = 0;
	int w;
	static int g;

	if (!*xp && !*yp)
		return 0;

	if (*xp == 0xffff && *yp == 0xffff) {
		for (i = 0; i < vtapsv; i++) {
			x[i] = 0xffff;
			y[i] = 0xffff;
		}
		g = 100;
		return 0;
	}

	for (i = vtapsv-1; i > 0; i--) {
		x[i] = x[i-1];
		y[i] = y[i-1];
	}

	x[0] = *xp;
	y[0] = *yp;

	for (i = 0; i < vtapsv; i++) {
		if (x[i] == 0xffff)
			break;
		else {
			w = vtapsv-i;
			ws += w;
			xs += w*x[i];
			ys += w*y[i];
		}
	}
	*xp = xs / ws;
	*yp = ys / ws;

	g += 10;
	if (g > 200)
		g = 200;

	return g;
}

static void touchx(int *xp, int *yp, unsigned char finger,
		unsigned char number_of_fingers_actually_touching)
{
	static int l_gradual[] = {
		1,  2,  3,  7, 10, 13, 16, 20, 23, 26, 33, 40, 70, 5000, 0 };

	static int g_gradual[] = {
		38, 42, 44, 51, 54, 58, 61, 64, 66, 67, 70, 72, 75, 68 };

	static int l_aggressive[] = {
		1,  2,  3,  4,  5,  6,  9, 12, 16, 30,
		40, 45, 50, 55, 60, 70, 80, 5000, 0 };

	static int g_aggressive[] = {
		61, 68, 71, 73, 74, 75, 76, 77, 78, 78,
		77, 76, 75, 73, 72, 70, 66, 62 };

	static int l_less[] = {
		1,  2,  3,  4,  5,  6, 9, 12, 16, 30, 40,
		45, 50, 55, 60, 70, 80, 5000, 0 };

	static int g_less[] = {
		30, 34, 35, 36, 36, 37, 38, 38, 39, 39, 38,
		38, 37, 36, 36, 35, 33, 31 };

	static int l_sbear[] = {
		1,  2,  3,  4,  5,  6,  9, 12, 16, 30, 40,
		45, 50, 55, 60, 70, 80, 5000, 0 };

	static int g_sbear[] = {
		43, 48, 50, 51, 52, 53, 53, 54, 55, 55, 54,
		53, 53, 51, 50, 49, 43, 62 };

	static int l_mbear[] = {
		1,  2,  3,  4,  5,  6,  9, 12, 16, 30, 40,
		45, 50, 55, 60, 70, 80, 5000, 0 };

	static int g_mbear[] = {
		49, 54, 57, 58, 59, 60, 61, 62, 62, 62, 61,
		60, 59, 58, 57, 56, 53, 49 };

	static int l_scroll[] = {
		1,  5,  6,  7,  8,  9, 10, 11, 13, 15, 20,
		26, 30, 40, 50, 60, 5000, 0 };

	static int g_scroll[] = {
		10, 42, 45, 65, 65, 65, 55, 55, 55, 55, 55,
		55, 55, 55, 55, 70, 80 };

	static int l_scrollb[] = {
		1,  2,  3,  4,  5,  6,  7,  8,  9,  10,
		11, 12, 14, 18, 22, 25, 32, 36, 42, 53,
		60, 70, 75, 80, 84, 5000, 0 };

	static int g_scrollb[] = {
		10, 12, 15, 43, 67, 69, 70, 70, 69, 67,
		65, 63, 61, 58, 57, 56, 56, 57, 58, 62,
		65, 70, 73, 77, 80, 80};

	static int xv;
	static int yv;

	int xc, yc, xvp, yvp;
	int dx, dy, pdx, pdy;
	int len, lenc, i;
	static int point;
	static unsigned int time_since_last_multi_finger_touch;
	int recovery_is_enabled = 1;
	unsigned long recovery_time = 11000000;
	int tip = 9;
	int gos;
	int scl = 10;
	int ofs = 0;

	int x = *xp;
	int y = *yp;

	int *l;
	int *g;

	if (!touchx_state)
		return;

	if (number_of_fingers_actually_touching > 1) {
		time_since_last_multi_finger_touch = jiffies_to_msecs(jiffies);
		return;
	}

	if (finger != 0)
		return;

	if (jiffies_to_msecs(jiffies) -
			time_since_last_multi_finger_touch < 300)
		return;

	touchxp.finger_down++;

	hrtimer_cancel(&touch_residual_timer);

	l = 0;

	if (touchx_state == 1) {
		l = l_gradual;
		g = g_gradual;
	}

	if (touchx_state == 2) {
		l = l_aggressive;
		g = g_aggressive;
	}

	if (touchx_state == 3) {
		l = l_less;
		g = g_less;
	}

	if (touchx_state == 4) {
		l = l_sbear;
		g = g_sbear;
	}

	if (touchx_state == 5) {
		l = l_mbear;
		g = g_mbear;
	}

	if (touchx_state == 6) {
		l = l_scroll;
		g = g_scroll;
		scl = 17;
		recovery_is_enabled = 0;
		tip = 5;
		vtapsv = 6;
	}

	if (touchx_state == 7) {
		l = l_scroll;
		g = g_scroll;
		scl = 15;
		ofs = 0;
		recovery_is_enabled = 1;
		tip = 2;
		vtapsv = 6;
	}

	if (touchx_state == 8) {
		l = l_scrollb;
		g = g_scrollb;
		scl = 10;
		ofs = -3;
		recovery_is_enabled = 0;
		tip = 2;
		vtapsv = 6;
	}

	/* just for testing */
	if (touchx_state == 9) {
		*xp -= 50;
		*yp -= 50;
		return;
	}

	if (!l) {
		l = l_gradual;
		g = g_gradual;
	}

	xc = x; yc = y;

	point++;

	if (touchxp.finger_down == 1) {
		int z = 0xffff;
#ifdef TESTRIG
		cls();
		line_first(x, y);
#endif
		ftapd(&z, &z);
		ftapv(&z, &z);
		point = 0;
		return;
	}

	if (point < tip) {
		xo = x; yo = y;
		xv = x; yv = y;
		return;
	}

	if (xo || yo) {
		if (!xo && !yo) {
			xo = x;
			yo = y;
			xv = x;
			yv = y;
		}

		dx = x - xo;
		pdx = dx;
		xvp = xv;

		dy = y - yo;
		pdy = dy;
		yvp = yv;

		if (l_abs(dx) > MAXMV) {
			if (x < xo)
				dx = -MAXMV;
			else
				dx =  MAXMV;
		}
		if (l_abs(dy) > MAXMV) {
			if (y < yo)
				dy = -MAXMV;
			else
				dy =  MAXMV;
		}

		len = iv_len(dx, dy, 5);
		if (len > 2)
			ftapd(&dx, &dy);

		lenc = iv_len(xo-x, yo-y, 150);
		len  = iv_len(xv-x, yv-y, 200);

		i = 0;
		while (l[i]) {
			gos = (g[i] + ofs) * scl / 10;
			if (lenc <= l[i]) {
				if ((len > 55) && is_ld(xvp, yvp, x, y)) {
					xv += (gos * dx /
							(((len - 55)/5) + 10));
					yv += (gos * dy /
							(((len - 55)/5) + 10));
				} else {
					xv += (gos * dx / 10) + (x - xv)/6;
					yv += (gos * dy / 10) + (y - yv)/6;
				}
				break;
			}
			i++;
		}

		ftapv(&xv, &yv);
	}

	xo = xc;
	yo = yc;

	*xp = xv;
	*yp = yv;

	if (recovery_is_enabled)
		hrtimer_start(&touch_residual_timer,
				ktime_set(0, recovery_time), HRTIMER_MODE_REL);
}

static int __init touchx_init(void)
{
	printk(KERN_INFO "touchx init\n");

	INIT_WORK(&touch_work, touch_notify);
	mutex_init(&touchxp.virtual_touch_mutex);
	touchx_state = 6;
	pr_info("Initial touchx state = %d\n", touchx_state);
	touchx_kobj = kobject_create_and_add("touchx", kernel_kobj);

	if (touchx_kobj) {
		if (-1 == sysfs_create_file(touchx_kobj,
					    &touchx_state_attribute.attr))
			pr_err("failed to create touchx sysfs entry.\n");
	}

	hrtimer_init(&(touch_residual_timer), CLOCK_MONOTONIC,
			HRTIMER_MODE_REL);

	(touch_residual_timer).function = touch_residual_timer_callback;

	touchxp.touchx = touchx;

	return 0;
}

static void __exit touchx_exit(void)
{
	touchxp.touchx = 0;
	mutex_destroy(&touchxp.virtual_touch_mutex);
	kobject_put(touchx_kobj);
	printk(KERN_INFO "touchx exit\n");
}

module_init(touchx_init);
module_exit(touchx_exit);

/* FIXME: change this: */
MODULE_LICENSE("GPL");
/* MODULE_LICENSE("Proprietary. (C) 2013 - Motorola Mobility LLC"); */
MODULE_AUTHOR("Motorola Mobility LLC");
MODULE_DESCRIPTION("touchx module");
