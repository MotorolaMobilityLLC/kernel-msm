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

#define SYSMSK 0666
#define MAXMV 70
#define TAPSD 5
#define TAPSV 11
int vtapsv = TAPSV;  /* note: vtapsv <= TAPSV */

static void touchx(int *x, int *y, unsigned char fn, unsigned char nf);
static void set_curve(char);
static struct kobject *touchx_kobj;
static struct work_struct touch_work;
struct hrtimer touch_residual_timer;
static int wc;
static int xo;
static int yo;
static int iv_len(int x, int y);

struct profile_attr_t {
	int touchx_state;  /* 0 = touchx is off, anything else is on */
	int profile;	   /* selects response curve & profile elements */
	const char *curve_n;
	int *curve_l;
	int *curve_g;
	int scl;
	int recovery_is_enabled;
	int ofs;
	int tip;
	int vtapsv;
	int limit;	   /* vel_limit */
	int acc_limit;
};
static struct profile_attr_t attr;

static void set_touchx_profile(int profile);
static void tip_attr(void);

static ssize_t sysfs_show_touchx_state(struct kobject *kobj,
		struct kobj_attribute *kattr, char *buf)
{
	wc++;
	return snprintf(buf, PAGE_SIZE, "%d\n", attr.touchx_state);
}

static ssize_t sysfs_set_touchx_state(struct kobject *kobj,
		struct kobj_attribute *kattr,
		const char *buf, size_t count)
{
	sscanf(buf, "%du", &attr.touchx_state);
	if (attr.touchx_state)
		attr.touchx_state = 1;
	pr_info("New touchx state = %d\n", attr.touchx_state);
	return count;
}


static ssize_t sysfs_show_touchx_scale(struct kobject *kobj,
		struct kobj_attribute *kattr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", attr.scl);
}

static ssize_t sysfs_set_touchx_scale(struct kobject *kobj,
		struct kobj_attribute *kattr,
		const char *buf, size_t count)
{
	sscanf(buf, "%du", &attr.scl);
	pr_info("New touchx scale = %d\n", attr.scl);
	return count;
}


static ssize_t sysfs_show_touchx_profile(struct kobject *kobj,
		struct kobj_attribute *kattr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", attr.profile);
}

static ssize_t sysfs_set_touchx_profile(struct kobject *kobj,
		struct kobj_attribute *kattr,
		const char *buf, size_t count)
{
	sscanf(buf, "%du", &attr.profile);
	set_touchx_profile(attr.profile);
	pr_info("New touchx profile = %d\n", attr.profile);
	return count;
}


static ssize_t sysfs_show_touchx_recovery(struct kobject *kobj,
		struct kobj_attribute *kattr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", attr.recovery_is_enabled);
}

static ssize_t sysfs_set_touchx_recovery(struct kobject *kobj,
		struct kobj_attribute *kattr,
		const char *buf, size_t count)
{
	sscanf(buf, "%du", &attr.recovery_is_enabled);
	pr_info("New touchx recovery = %d\n", attr.recovery_is_enabled);
	return count;
}


static ssize_t sysfs_show_touchx_offset(struct kobject *kobj,
		struct kobj_attribute *kattr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", attr.ofs);
}

static ssize_t sysfs_set_touchx_offset(struct kobject *kobj,
		struct kobj_attribute *kattr,
		const char *buf, size_t count)
{
	sscanf(buf, "%du", &attr.ofs);
	pr_info("New touchx offset = %d\n", attr.ofs);
	return count;
}


static ssize_t sysfs_show_touchx_tip(struct kobject *kobj,
		struct kobj_attribute *kattr, char *buf)
{
	if (wc == -9) {
		tip_attr();
		wc++;
	} else
		wc = -18;

	return snprintf(buf, PAGE_SIZE, "%d\n", attr.tip);
}

static ssize_t sysfs_set_touchx_tip(struct kobject *kobj,
		struct kobj_attribute *kattr,
		const char *buf, size_t count)
{
	sscanf(buf, "%du", &attr.tip);
	pr_info("New touchx tip = %d\n", attr.tip);
	return count;
}


static ssize_t sysfs_show_touchx_depth(struct kobject *kobj,
		struct kobj_attribute *kattr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", attr.vtapsv);
}

static ssize_t sysfs_set_touchx_depth(struct kobject *kobj,
		struct kobj_attribute *kattr,
		const char *buf, size_t count)
{
	sscanf(buf, "%du", &attr.vtapsv);

	if (attr.vtapsv < 1)
		attr.vtapsv = 1;
	if (attr.vtapsv > TAPSV)
		attr.vtapsv = TAPSV;

	pr_info("New touchx depth = %d\n", attr.vtapsv);
	return count;
}


static ssize_t sysfs_show_touchx_curve(struct kobject *kobj,
		struct kobj_attribute *kattr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", attr.curve_n);
}

static ssize_t sysfs_set_touchx_curve(struct kobject *kobj,
		struct kobj_attribute *kattr,
		const char *buf, size_t count)
{
	char c;
	c = *buf;

	set_curve(c);

	return count;
}


static ssize_t sysfs_show_touchx_limit(struct kobject *kobj,
		struct kobj_attribute *kattr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", attr.limit);
}

static ssize_t sysfs_set_touchx_limit(struct kobject *kobj,
		struct kobj_attribute *kattr,
		const char *buf, size_t count)
{
	sscanf(buf, "%du", &attr.limit);

	pr_info("New touchx limit = %d\n", attr.limit);
	return count;
}


static ssize_t sysfs_show_touchx_acc_limit(struct kobject *kobj,
		struct kobj_attribute *kattr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", attr.acc_limit);
}

static ssize_t sysfs_set_touchx_acc_limit(struct kobject *kobj,
		struct kobj_attribute *kattr,
		const char *buf, size_t count)
{
	sscanf(buf, "%du", &attr.acc_limit);

	if (attr.acc_limit < 1)
		attr.acc_limit = 1;

	if (attr.acc_limit > 100)
		attr.acc_limit = 100;

	pr_info("New touchx acc_limit = %d\n", attr.acc_limit);
	return count;
}


static struct kobj_attribute touchx_state_attribute =
	__ATTR(touchx_state, 0644, sysfs_show_touchx_state,
			sysfs_set_touchx_state);

static struct kobj_attribute profile_attribute =
	__ATTR(profile, 0644, sysfs_show_touchx_profile,
			sysfs_set_touchx_profile);

static struct kobj_attribute scale_attribute =
	__ATTR(scale, 0644, sysfs_show_touchx_scale,
			sysfs_set_touchx_scale);

static struct kobj_attribute recovery_attribute =
	__ATTR(recovery, 0644, sysfs_show_touchx_recovery,
			sysfs_set_touchx_recovery);

static struct kobj_attribute offset_attribute =
	__ATTR(offset, 0644, sysfs_show_touchx_offset,
			sysfs_set_touchx_offset);

static struct kobj_attribute tip_attribute =
	__ATTR(tip, 0644, sysfs_show_touchx_tip,
			sysfs_set_touchx_tip);

static struct kobj_attribute depth_attribute =
	__ATTR(depth, 0644, sysfs_show_touchx_depth,
			sysfs_set_touchx_depth);

static struct kobj_attribute curve_attribute =
	__ATTR(curve, 0644, sysfs_show_touchx_curve,
			sysfs_set_touchx_curve);

static struct kobj_attribute limit_attribute =
	__ATTR(vel_limit, 0644, sysfs_show_touchx_limit,
			sysfs_set_touchx_limit);

static struct kobj_attribute acc_limit_attribute =
	__ATTR(acc_limit, 0644, sysfs_show_touchx_acc_limit,
			sysfs_set_touchx_acc_limit);

static const struct attribute *ifcs[] = {
	&touchx_state_attribute.attr,
	&profile_attribute.attr,
	&scale_attribute.attr,
	&recovery_attribute.attr,
	&offset_attribute.attr,
	&tip_attribute.attr,
	&depth_attribute.attr,
	&curve_attribute.attr,
	&limit_attribute.attr,
	&acc_limit_attribute.attr,
	0};

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

	if (iv_len(x - xo, y - yo) <= 12)
		hrtimer_cancel(&touch_residual_timer);
	else
	  hrtimer_start(&touch_residual_timer,
			ktime_set(0, 5000000), HRTIMER_MODE_REL);

	mutex_unlock(&touchxp.virtual_touch_mutex);
}

static enum hrtimer_restart
touch_residual_timer_callback(struct hrtimer *timer)
{
	schedule_work(&touch_work);
	return HRTIMER_NORESTART;
}

static void tip_attr(void)
{
	int i = 0;

	while (ifcs[i]) {
		if (-1 == sysfs_chmod_file(touchx_kobj, ifcs[i], SYSMSK))
			pr_err("touchx attr error\n");
		i++; }
}

static int iv_len(int x, int y)
{
	int r;
	int a;
	int i;

	a = x * x + y * y;
	r = a;

	if (a == 0)
		return 0;

	for (i = 0; i < 14; i++)
		r = (r + a/r) >> 1;

	return r;
}

int is_ld(int xv, int yv, int x, int y)
{
	static int xo;
	static int yo;
	int r = 0;

	if (!xo && !yo)
		goto done;

	if (iv_len(xv-x, yv-y) < iv_len(xv-xo, yv-yo))
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

static const char n_gradual[] = "a-gradual";

static int l_gradual[] = {
	1,  2,	3,  7, 10, 13, 16, 20, 23, 26, 33, 40, 70, 5000, 0 };

static int g_gradual[] = {
	38, 42, 44, 51, 54, 58, 61, 64, 66, 67, 70, 72, 75, 68 };


static const char n_less[] = "b-less";

static int l_less[] = {
	1,  2,	3,  4,	5,  6, 9, 12, 16, 30, 40,
	45, 50, 55, 60, 70, 80, 5000, 0 };

static int g_less[] = {
	30, 34, 35, 36, 36, 37, 38, 38, 39, 39, 38,
	38, 37, 36, 36, 35, 33, 31 };


static const char n_sbear[] = "c-sbear";

static int l_sbear[] = {
	1,  2,	3,  4,	5,  6,	9, 12, 16, 30, 40,
	45, 50, 55, 60, 70, 80, 5000, 0 };

static int g_sbear[] = {
	43, 48, 50, 51, 52, 53, 53, 54, 55, 55, 54,
	53, 53, 51, 50, 49, 43, 62 };


static const char n_mbear[] = "d-mbear";

static int l_mbear[] = {
	1,  2,	3,  4,	5,  6,	9, 12, 16, 30, 40,
	45, 50, 55, 60, 70, 80, 5000, 0 };

static int g_mbear[] = {
	49, 54, 57, 58, 59, 60, 61, 62, 62, 62, 61,
	60, 59, 58, 57, 56, 53, 49 };


static const char n_scroll[] = "e-scroll";

static int l_scroll[] = {
	1,  5,	6,  7,	8,  9, 10, 11, 13, 15, 20,
	26, 30, 40, 50, 60, 5000, 0 };

static int g_scroll[] = {
	10, 42, 45, 65, 65, 65, 55, 55, 55, 55, 55,
	55, 55, 55, 55, 70, 80 };


static const char n_scrollb[] = "f-scrollb";

static int l_scrollb[] = {
	1,  2,	3,  4,	5,  6,	7,  8,	9,  10,
	11, 12, 14, 18, 22, 25, 32, 36, 42, 53,
	60, 70, 75, 80, 84, 5000, 0 };

static int g_scrollb[] = {
	10, 12, 15, 43, 67, 69, 70, 70, 69, 67,
	65, 63, 61, 58, 57, 56, 56, 57, 58, 62,
	65, 70, 73, 77, 80, 80};


static const char n_bump[] = "g-bump";

static int l_bump[] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
	14, 18, 22, 25, 32, 5000, 0};

static int g_bump[] = {
	10, 25, 40, 53, 67, 69, 70, 70, 69, 67,
	65, 60, 40, 15, 10, 10, 10, 10};


static const char n_blip[] = "h-blip";

static int l_blip[] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
	14, 18, 5000, 0};

static int g_blip[] = {
	10, 25, 40, 53, 67, 69, 70, 70, 69, 60,
	40, 15, 10, 10};


static const char n_mesa[] = "i-mesa";

static int l_mesa[] = {1, 2, 3, 4, 5, 6, 7, 8, 5000, 0};

static int g_mesa[] = {10, 40, 60, 69, 69, 30, 10, 10, 10, 10};


static const char n_acri[] = "j-acri";

static int l_acri[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 5000, 0};

static int g_acri[] = {69, 69, 69, 60, 30, 10, 5, 2, 1, 1, 1};


void set_curve(char c)
{
	if (c == 'a') {
		attr.curve_n = n_gradual;
		attr.curve_l = l_gradual;
		attr.curve_g = g_gradual;
	}

	if (c == 'b') {
		attr.curve_n = n_less;
		attr.curve_l = l_less;
		attr.curve_g = g_less;
	}

	if (c == 'c') {
		attr.curve_n = n_sbear;
		attr.curve_l = l_sbear;
		attr.curve_g = g_sbear;
	}

	if (c == 'd') {
		attr.curve_n = n_mbear;
		attr.curve_l = l_mbear;
		attr.curve_g = g_mbear;
	}

	if (c == 'e') {
		attr.curve_n = n_scroll;
		attr.curve_l = l_scroll;
		attr.curve_g = g_scroll;
	}

	if (c == 'f') {
		attr.curve_n = n_scrollb;
		attr.curve_l = l_scrollb;
		attr.curve_g = g_scrollb;
	}

	if (c == 'g') {
		attr.curve_n = n_bump;
		attr.curve_l = l_bump;
		attr.curve_g = g_bump;
	}

	if (c == 'h') {
		attr.curve_n = n_blip;
		attr.curve_l = l_blip;
		attr.curve_g = g_blip;
	}

	if (c == 'i') {
		attr.curve_n = n_mesa;
		attr.curve_l = l_mesa;
		attr.curve_g = g_mesa;
	}

	if (c == 'j') {
		attr.curve_n = n_acri;
		attr.curve_l = l_acri;
		attr.curve_g = g_acri;
	}
}

void set_touchx_profile(int profile)
{
	if (profile)
		attr.touchx_state = 1;
	else {
		attr.touchx_state = 0;
		attr.profile = 0;
		return;
	}

	attr.curve_n = n_gradual;
	attr.curve_l = l_gradual;
	attr.curve_g = g_gradual;

	attr.scl = 10;
	attr.recovery_is_enabled = 1;

	attr.ofs = 0;
	attr.tip = 9;
	attr.vtapsv = TAPSV;

	attr.profile = profile;

	attr.limit = MAXMV;
	attr.acc_limit = 55;

	if (profile == 1) {
		attr.curve_n = n_less;
		attr.curve_l = l_less;
		attr.curve_g = g_less;

		attr.scl = 7;
		attr.ofs = 0;
		attr.tip = 6;
		attr.vtapsv = 11;
		attr.limit = 14;
		attr.acc_limit = 12;
		attr.recovery_is_enabled = 1;
	}

	if (profile == 2) {
		attr.curve_n = n_less;
		attr.curve_l = l_less;
		attr.curve_g = g_less;

		attr.scl = 16;
		attr.recovery_is_enabled = 1;
		attr.ofs = 0;
		attr.tip = 9;
		attr.vtapsv = 11;
		attr.limit = 10;
		attr.acc_limit = 12;
	}

	if (profile == 3) {
		attr.curve_n = n_mesa;
		attr.curve_l = l_mesa;
		attr.curve_g = g_mesa;

		attr.scl = 9;
		attr.ofs = -9;
		attr.tip = 9;
		attr.vtapsv = 9;
		attr.limit = 17;
		attr.acc_limit = 18;
		attr.recovery_is_enabled = 1;
	}

	if (profile == 4) {
		attr.curve_n = n_mesa;
		attr.curve_l = l_mesa;
		attr.curve_g = g_mesa;

		attr.scl = 10;
		attr.ofs = -9;
		attr.tip = 9;
		attr.vtapsv = 9;
		attr.limit = 20;
		attr.acc_limit = 21;
		attr.recovery_is_enabled = 1;
	}

	if (profile == 5) {
		attr.curve_n = n_acri;
		attr.curve_l = l_acri;
		attr.curve_g = g_acri;

		attr.scl = 10;
		attr.ofs = -6;
		attr.tip = 8;
		attr.vtapsv = 9;
		attr.limit = 20;
		attr.acc_limit = 21;
		attr.recovery_is_enabled = 1;
	}

	if (profile == 6) {
		attr.curve_n = n_mesa;
		attr.curve_l = l_mesa;
		attr.curve_g = g_mesa;

		attr.scl = 10;
		attr.ofs = -7;
		attr.tip = 7;
		attr.vtapsv = 9;
		attr.limit = 17;
		attr.acc_limit = 27;
		attr.recovery_is_enabled = 0;
	}

	if (profile == 7) {
		attr.curve_n = n_mesa;
		attr.curve_l = l_mesa;
		attr.curve_g = g_mesa;

		attr.scl = 9;
		attr.ofs = -5;
		attr.tip = 8;
		attr.vtapsv = 7;
		attr.limit = 20;
		attr.acc_limit = 21;
		attr.recovery_is_enabled = 0;
	}

	if (profile == 8) {
		attr.curve_n = n_mesa;
		attr.curve_l = l_mesa;
		attr.curve_g = g_mesa;

		attr.scl = 10;
		attr.ofs = -6;
		attr.tip = 7;
		attr.vtapsv = 8;
		attr.limit = 20;
		attr.acc_limit = 52;
		attr.recovery_is_enabled = 1;
	}

	if (profile == 9) {
		attr.curve_n = n_scrollb;
		attr.curve_l = l_scrollb;
		attr.curve_g = g_scrollb;
		attr.scl = 14;
		attr.ofs = 0;
		attr.tip = 9;
		attr.vtapsv = 11;
		attr.limit = 23;
		attr.recovery_is_enabled = 0;
		attr.acc_limit = 55;
	}
}

static void touchx(int *xp, int *yp, unsigned char finger,
		unsigned char number_of_fingers_actually_touching)
{
	static int xv;
	static int yv;

	int xc, yc, xvp, yvp;
	int dx, dy, pdx, pdy;
	int len, lenc, i;
	static int point;
	static unsigned int time_since_last_multi_finger_touch;
	unsigned long recovery_time = 20000000;
	int gos, scl, recovery_is_enabled, ofs, tip, limit, acc_limit, clen;

	int x = *xp;
	int y = *yp;

	int *l;
	int *g;

	if (!attr.touchx_state)
		return;

	l = attr.curve_l;
	g = attr.curve_g;
	scl = attr.scl;
	recovery_is_enabled = attr.recovery_is_enabled;
	ofs = attr.ofs;
	tip = attr.tip;
	vtapsv = attr.vtapsv;
	limit = attr.limit;
	acc_limit = attr.acc_limit;

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

		clen = iv_len(dx, dy);
		if (clen > limit) {
			int normp = (limit << 14) / clen;
			dx = (dx * normp) >> 14;
			dy = (dy * normp) >> 14;
		  }

		len = iv_len(dx, dy);
		if (len > 2)
			ftapd(&dx, &dy);

		lenc = iv_len(xo-x, yo-y);
		len  = iv_len(xv-x, yv-y);

		i = 0;
		while (l[i]) {
			gos = (g[i] + ofs) * scl / 10;
			if (lenc <= l[i]) {
				if ((len > acc_limit)
				    && is_ld(xvp, yvp, x, y)) {
					xv += (gos * dx /
					       (((len - acc_limit)/5) + 10));
					yv += (gos * dy /
					       (((len - acc_limit)/5) + 10));
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
	set_touchx_profile(4);
	attr.touchx_state = 0;
	pr_info("Initial touchx state = %d\n", attr.touchx_state);
	touchx_kobj = kobject_create_and_add("touchx", kernel_kobj);

	if (touchx_kobj) {
		if (-1 == sysfs_create_file(touchx_kobj,
					    &touchx_state_attribute.attr))
			pr_err("failed to create touchx_state sysfs entry.\n");
		if (-1 == sysfs_create_file(touchx_kobj,
					    &profile_attribute.attr))
			pr_err("failed to create touchx/profile sysfs entry.\n");
		if (-1 == sysfs_create_file(touchx_kobj,
					    &scale_attribute.attr))
			pr_err("failed to create touchx/scale sysfs entry.\n");
		if (-1 == sysfs_create_file(touchx_kobj,
					    &recovery_attribute.attr))
			pr_err("failed to create touchx/recovery sysfs entry.\n");
		if (-1 == sysfs_create_file(touchx_kobj,
					    &offset_attribute.attr))
			pr_err("failed to create touchx/offset sysfs entry.\n");
		if (-1 == sysfs_create_file(touchx_kobj,
					    &tip_attribute.attr))
			pr_err("failed to create touchx/tip sysfs entry.\n");
		if (-1 == sysfs_create_file(touchx_kobj,
					    &depth_attribute.attr))
			pr_err("failed to create touchx/depth sysfs entry.\n");
		if (-1 == sysfs_create_file(touchx_kobj,
					    &curve_attribute.attr))
			pr_err("failed to create touchx/curve sysfs entry.\n");
		if (-1 == sysfs_create_file(touchx_kobj,
					    &limit_attribute.attr))
			pr_err("failed to create touchx/vel_limit sysfs entry.\n");
		if (-1 == sysfs_create_file(touchx_kobj,
					    &acc_limit_attribute.attr))
			pr_err("failed to create touchx/acc_limit sysfs entry.\n");
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

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Motorola Mobility LLC");
MODULE_DESCRIPTION("touchx module");
