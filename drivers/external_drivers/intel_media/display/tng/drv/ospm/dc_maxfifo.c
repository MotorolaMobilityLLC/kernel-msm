/**************************************************************************
 * Copyright (c) 2012, Intel Corporation.
 * All Rights Reserved.

 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *   Vinil Cheeramvelill <vinil.cheeramvelil@intel.com>
 */

#include "displayclass_interface.h"
#include "dc_maxfifo.h"
#include "psb_drv.h"
#include "psb_intel_reg.h"

#include <linux/device.h>
#include <linux/intel_mid_pm.h>

#define MAXFIFO_IDLE_FRAME_COUNT	10

/* When the Display controller buffer is above the high watermark
 * the display controller has enough data and does not need to
 * fetch any more data from memory till the low watermark point
 * is reached
 * Register layout
 *	Bits  9:0	- Low watermark
 *	Bits 19:10	- High watermark
 * The Watermark values represent 64 bytes of space. So a high
 * watermark value of 0x200 in bits 19:10 is
 * 0x200 X 64 = 512 X 64 = 32768 bytes
 */
#define MAXFIFO_TNG_SYSFS_GROUP_NAME	"dc_maxfifo"

#define MAXFIFO_HIGH_WATERMARK		(0x200<<10)
#define MAXFIFO_LOW_WATEMARK		(0x100<<0)

#define TNG_DSPSRCTRL_DEFAULT	(MAXFIFO_LOW_WATEMARK |\
				MAXFIFO_HIGH_WATERMARK |\
				DSPSRCTRL_MAXFIFO_MODE_ALWAYS_MAXFIFO |\
				DSPSRCTRL_MAXFIFO_ENABLE)

#define DC_MAXFIFO_REGSTOSET_DSPSRCTRL_ENABLE	0x1
#define DC_MAXFIO_REGSTOSET_DSPSSM_S0i1_DISP	0x2
#define DC_MAXFIFO_REGSTOSET_DSPSRCTRL_MAXFIFO	0x4

#define TNG_MAXFIFO_REGS_TO_SET_DEFAULT  (DC_MAXFIFO_REGSTOSET_DSPSRCTRL_ENABLE |\
		DC_MAXFIFO_REGSTOSET_DSPSRCTRL_MAXFIFO |\
		DC_MAXFIO_REGSTOSET_DSPSSM_S0i1_DISP )

typedef enum {
	S0i1_DISP_STATE_NOT_READY = 0,
	S0i1_DISP_STATE_READY,
	S0i1_DISP_STATE_ENTERED
} S0i1_DISP_STATE;

struct dc_maxfifo {
	struct mutex maxfifo_mtx;

	struct drm_device	*dev_drm;
	bool repeat_frame_interrupt_on;
	int  regs_to_set;
	S0i1_DISP_STATE s0i1_disp_state;
	struct work_struct repeat_frame_interrupt_work;
};

/* Sysfs Entries for Maxfifo mode
 */
static ssize_t _show_sysfs_enable (struct device *kdev,
					struct device_attribute *attr,
					char *buf);
static ssize_t _store_sysfs_enable (struct device *kdev,
					struct device_attribute *attr,
					const char *buf, size_t count);
static ssize_t _show_sysfs_state (struct device *kdev,
					struct device_attribute *attr, char *buf);
static inline bool _maxfifo_create_sysfs_entries(struct drm_device * dev);

#ifndef ENABLE_HW_REPEAT_FRAME
int maxfifo_entry_delay = 150;
EXPORT_SYMBOL(maxfifo_entry_delay);
module_param_named(maxfifo_delay, maxfifo_entry_delay, int, 0600);

static void maxfifo_timer_func(unsigned long data)
{
	maxfifo_report_repeat_frame_interrupt((struct drm_device *)data);
}

static int maxfifo_timer_init(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct timer_list *maxfifo_timer = &dev_priv->maxfifo_timer;

	init_timer(maxfifo_timer);

	maxfifo_timer->data = (unsigned long)dev;
	maxfifo_timer->function = maxfifo_timer_func;
	maxfifo_timer->expires = jiffies + maxfifo_entry_delay*HZ/1000;

	return 0;
}

void maxfifo_timer_start(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct timer_list *maxfifo_timer = &dev_priv->maxfifo_timer;
	struct dc_maxfifo * maxfifo_info =
		(struct dc_maxfifo *)
		((struct drm_psb_private *)dev->dev_private)->dc_maxfifo_info;

	if(maxfifo_info->repeat_frame_interrupt_on)
		mod_timer(maxfifo_timer, jiffies + maxfifo_entry_delay*HZ/1000);
}

void maxfifo_timer_stop(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;

	del_timer(&dev_priv->maxfifo_timer);
}
#endif


void enable_repeat_frame_intr(struct drm_device *dev)
{
#ifndef ENABLE_HW_REPEAT_FRAME
	struct dc_maxfifo * maxfifo_info =
		(struct dc_maxfifo *)
		((struct drm_psb_private *)dev->dev_private)->dc_maxfifo_info;

	maxfifo_info->repeat_frame_interrupt_on = true;
#else
	if (power_island_get(OSPM_DISPLAY_A)) {
		mrfl_enable_repeat_frame_intr(dev, MAXFIFO_IDLE_FRAME_COUNT);
		power_island_put(OSPM_DISPLAY_A);
	}
#endif
}

void disable_repeat_frame_intr(struct drm_device *dev)
{
#ifndef ENABLE_HW_REPEAT_FRAME
	struct dc_maxfifo * maxfifo_info =
		(struct dc_maxfifo *)
		((struct drm_psb_private *)dev->dev_private)->dc_maxfifo_info;

	maxfifo_info->repeat_frame_interrupt_on = false;
#else
	if (power_island_get(OSPM_DISPLAY_A)) {
		mrfl_disable_repeat_frame_intr(dev);
		power_island_put(OSPM_DISPLAY_A);
	}
#endif
}

static void _maxfifo_send_hwc_uevent(struct drm_device * dev)
{
	char *event_string = "REPEATED_FRAME";
	char *envp[] = { event_string, NULL };
	PSB_DEBUG_PM("Sending uevent to HWC\n");

	kobject_uevent_env(&dev->primary->kdev.kobj,
				KOBJ_CHANGE, envp);
}

static void _maxfifo_send_hwc_event_work(struct work_struct *work)
{
	struct dc_maxfifo * maxfifo_info =
		container_of(work, struct dc_maxfifo, repeat_frame_interrupt_work);
	 _maxfifo_send_hwc_uevent(maxfifo_info->dev_drm);

}

void maxfifo_report_repeat_frame_interrupt(struct drm_device * dev)
{
	struct dc_maxfifo * maxfifo_info =
		(struct dc_maxfifo *)
		((struct drm_psb_private *)dev->dev_private)->dc_maxfifo_info;
#ifdef ENABLE_HW_REPEAT_FRAME
	mrfl_disable_repeat_frame_intr(dev);
#else
	if (maxfifo_info)
		maxfifo_info->repeat_frame_interrupt_on = false;
#endif
	if (maxfifo_info)
		schedule_work(&maxfifo_info->repeat_frame_interrupt_work);
}

bool enter_s0i1_display_mode(struct drm_device *dev)
{
	struct drm_psb_private * dev_priv = dev->dev_private;
	struct dc_maxfifo * maxfifo_info =
		(struct dc_maxfifo *) dev_priv->dc_maxfifo_info;


	if (maxfifo_info &&
		(maxfifo_info->s0i1_disp_state == S0i1_DISP_STATE_READY)){

		pmu_set_s0i1_disp_vote(true);
/*
		u32 dsp_ss_pm_val;

		dsp_ss_pm_val = intel_mid_msgbus_read32(PUNIT_PORT, DSP_SS_PM);
		dsp_ss_pm_val |= PUNIT_DSPSSPM_ENABLE_S0i1_DISPLAY;
		intel_mid_msgbus_write32(PUNIT_PORT, DSP_SS_PM, dsp_ss_pm_val);
*/
		maxfifo_info->s0i1_disp_state = S0i1_DISP_STATE_ENTERED;
		PSB_DEBUG_PM("Enabled S0i1-Display Punit DSPSSMP Register\n");
	}

	return true;
}

bool exit_s0i1_display_mode(struct drm_device *dev)
{
	struct drm_psb_private * dev_priv = dev->dev_private;
	struct dc_maxfifo * maxfifo_info =
		(struct dc_maxfifo *) dev_priv->dc_maxfifo_info;
	if (maxfifo_info &&
		(maxfifo_info->s0i1_disp_state == S0i1_DISP_STATE_ENTERED)){

		pmu_set_s0i1_disp_vote(false);
/*
		u32 dsp_ss_pm_val;

		dsp_ss_pm_val = intel_mid_msgbus_read32(PUNIT_PORT, DSP_SS_PM);
		dsp_ss_pm_val &= ~PUNIT_DSPSSPM_ENABLE_S0i1_DISPLAY;
		intel_mid_msgbus_write32(PUNIT_PORT, DSP_SS_PM, dsp_ss_pm_val);
*/
		maxfifo_info->s0i1_disp_state = S0i1_DISP_STATE_NOT_READY;
		PSB_DEBUG_PM(" Disabled S0i1-Display in Punit DSPSSMP Register\n");
	}

	return true;
}

bool enter_maxfifo_mode(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	struct dc_maxfifo * maxfifo_info = dev_priv->dc_maxfifo_info;
	u32 dspsrctrl_val = MAXFIFO_LOW_WATEMARK | MAXFIFO_HIGH_WATERMARK;
	u32 regs_to_set;

	if (!maxfifo_info)
		return false;

#ifndef ENABLE_HW_REPEAT_FRAME
	maxfifo_timer_stop(dev);
#endif
	mutex_lock(&maxfifo_info->maxfifo_mtx);
	regs_to_set = maxfifo_info->regs_to_set;

	if (dev_priv->psb_dpst_state)
		psb_irq_disable_dpst(dev);

	if (power_island_get(OSPM_DISPLAY_A)) {
		if (regs_to_set & DC_MAXFIFO_REGSTOSET_DSPSRCTRL_ENABLE) {
			dspsrctrl_val |= DSPSRCTRL_MAXFIFO_ENABLE;
			if (regs_to_set & DC_MAXFIFO_REGSTOSET_DSPSRCTRL_MAXFIFO)
				dspsrctrl_val |= DSPSRCTRL_MAXFIFO_MODE_ALWAYS_MAXFIFO;
			PSB_WVDC32(dspsrctrl_val, DSPSRCTRL_REG);
		}

		if (regs_to_set & DC_MAXFIO_REGSTOSET_DSPSSM_S0i1_DISP) {
			unsigned long irqflags;
			spin_lock_irqsave(&dev_priv->irqmask_lock, irqflags);
			maxfifo_info->s0i1_disp_state = S0i1_DISP_STATE_READY;
			spin_unlock_irqrestore(&dev_priv->irqmask_lock, irqflags);

		}
		PSB_DEBUG_PM("S0i1-Display-ENABLE : Reg DSPSRCTRL = %08x, "
				"DSP_SS_PM = %08x\n", PSB_RVDC32(DSPSRCTRL_REG),
				intel_mid_msgbus_read32(PUNIT_PORT, DSP_SS_PM));
		power_island_put(OSPM_DISPLAY_A);
	}
	mutex_unlock(&maxfifo_info->maxfifo_mtx);
	return true;
}

bool exit_maxfifo_mode(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = psb_priv(dev);
	struct dc_maxfifo *maxfifo_info = dev_priv->dc_maxfifo_info;
	u32 dspsrctrl_val = MAXFIFO_LOW_WATEMARK | MAXFIFO_HIGH_WATERMARK;

	if (!maxfifo_info)
		return false;

	if (dev_priv->psb_dpst_state)
		psb_irq_enable_dpst(dev);

	mutex_lock(&maxfifo_info->maxfifo_mtx);
	if (power_island_get(OSPM_DISPLAY_A)) {
		unsigned long irqflags;
		PSB_WVDC32(dspsrctrl_val, DSPSRCTRL_REG);

		spin_lock_irqsave(&dev_priv->irqmask_lock, irqflags);
		exit_s0i1_display_mode(dev);
		// ? maxfifo_info->s0i1_disp_state = S0i1_DISP_STATE_NOT_READY;
		spin_unlock_irqrestore(&dev_priv->irqmask_lock, irqflags);
		PSB_DEBUG_PM("S0i1-Display-DISABLE : Reg DSPSRCTRL = %08x, "
				"DSP_SS_PM = %08x\n", PSB_RVDC32(DSPSRCTRL_REG),
				intel_mid_msgbus_read32(PUNIT_PORT, DSP_SS_PM));
		power_island_put(OSPM_DISPLAY_A);
	}
	mutex_unlock(&maxfifo_info->maxfifo_mtx);
	return true;
}

int dc_maxfifo_init(struct drm_device *dev)
{
	struct dc_maxfifo * maxfifo_info = NULL;
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *)dev->dev_private;

	if (dev_priv->dc_maxfifo_info)
		return true;

	dev_priv->dc_maxfifo_info =
		kzalloc(sizeof(struct dc_maxfifo), GFP_KERNEL);

	if (!dev_priv->dc_maxfifo_info) {
		DRM_ERROR("No memory\n");
		return -ENOMEM;
	}
	maxfifo_info = dev_priv->dc_maxfifo_info;

	mutex_init(&maxfifo_info->maxfifo_mtx);

	mutex_lock(&maxfifo_info->maxfifo_mtx);

	maxfifo_info->dev_drm = dev;
	maxfifo_info->regs_to_set = TNG_MAXFIFO_REGS_TO_SET_DEFAULT;
	maxfifo_info->s0i1_disp_state = S0i1_DISP_STATE_NOT_READY;

	INIT_WORK(&maxfifo_info->repeat_frame_interrupt_work,
			_maxfifo_send_hwc_event_work);

#ifndef ENABLE_HW_REPEAT_FRAME
	maxfifo_info->repeat_frame_interrupt_on = false;
	maxfifo_timer_init(dev);
#endif
	_maxfifo_create_sysfs_entries(dev);
	/*Initialize the sysfs entries*/

	mutex_unlock(&maxfifo_info->maxfifo_mtx);
	return 0;


}


static ssize_t _show_sysfs_enable (struct device *kdev,
					struct device_attribute *attr, char *buf)
{
	int enabled = 0;

	struct drm_minor *minor = container_of(kdev, struct drm_minor, kdev);
	struct drm_device *dev = minor->dev;
	struct drm_psb_private * dev_priv = dev->dev_private;
	struct dc_maxfifo * maxfifo_info = dev_priv->dc_maxfifo_info;

	if (maxfifo_info){
		mutex_lock(&maxfifo_info->maxfifo_mtx);
		enabled = maxfifo_info->regs_to_set;
		mutex_unlock(&maxfifo_info->maxfifo_mtx);
	}
	return sprintf(buf, "%d\n", enabled);

}

static ssize_t _store_sysfs_enable (struct device *kdev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int ret = -EINVAL;
	int enable = 0;

	struct drm_minor *minor = container_of(kdev, struct drm_minor, kdev);
	struct drm_device *dev = minor->dev;
	struct drm_psb_private * dev_priv = dev->dev_private;
	struct dc_maxfifo * maxfifo_info = dev_priv->dc_maxfifo_info;

	if (maxfifo_info) {
		ret = sscanf(buf, "%d", &enable);
		mutex_lock(&maxfifo_info->maxfifo_mtx);
		maxfifo_info->regs_to_set = enable;
		mutex_unlock(&maxfifo_info->maxfifo_mtx);
		if (enable & 0x8)
			enter_maxfifo_mode(dev);
		if (enable & 0x10)
			exit_maxfifo_mode(dev);
		if (enable & 0x20)
			maxfifo_report_repeat_frame_interrupt(dev);
		if (enable & 0x40)
			enable_repeat_frame_intr(dev);
		if (enable & 0x80)
			_maxfifo_send_hwc_uevent(dev);

	}

	return count;
}

static ssize_t _show_sysfs_state (struct device *kdev,
					struct device_attribute *attr, char *buf)
{
	int ret = 0;

	struct drm_minor *minor = container_of(kdev, struct drm_minor, kdev);
	struct drm_device *dev = minor->dev;
	struct drm_psb_private * dev_priv = dev->dev_private;
	struct dc_maxfifo * maxfifo_info = dev_priv->dc_maxfifo_info;

	if (maxfifo_info){
		mutex_lock(&maxfifo_info->maxfifo_mtx);
		ret = sprintf(buf, "S0i1-Display-Status : Reg DSPSRCTRL = %08x, "
				"DSP_SS_PM = %08x\n", PSB_RVDC32(DSPSRCTRL_REG),
				intel_mid_msgbus_read32(PUNIT_PORT, DSP_SS_PM));
		PSB_DEBUG_PM("S0i1-Display-Status : Reg DSPSRCTRL = %08x, "
				"DSP_SS_PM = %08x\n", PSB_RVDC32(DSPSRCTRL_REG),
				intel_mid_msgbus_read32(PUNIT_PORT, DSP_SS_PM));
		mutex_unlock(&maxfifo_info->maxfifo_mtx);
	}
	return ret;

}

static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR, _show_sysfs_enable, _store_sysfs_enable);
static DEVICE_ATTR(state, S_IRUGO, _show_sysfs_state, NULL);

static struct attribute *tng_maxfifo_attrs[] = {
	&dev_attr_enable.attr,
	&dev_attr_state.attr,
	NULL
};

static struct attribute_group tng_maxfifo_attr_group = {
	.name = MAXFIFO_TNG_SYSFS_GROUP_NAME,
	.attrs = tng_maxfifo_attrs
};


static inline bool _maxfifo_create_sysfs_entries(struct drm_device * dev) {
	int ret;

	ret = sysfs_create_group(&dev->primary->kdev.kobj,
				&tng_maxfifo_attr_group);
	if (ret)
		DRM_ERROR("Maxfifo sysfs setup failed\n");
	return ret;
}

int dc_maxfifo_uninit(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv =
		(struct drm_psb_private *)dev->dev_private;

	if (!dev_priv->dc_maxfifo_info)
		return true;

	mutex_destroy(&((struct dc_maxfifo *)
		(dev_priv->dc_maxfifo_info))->maxfifo_mtx);

	kfree(dev_priv->dc_maxfifo_info);
	return 0;
}
