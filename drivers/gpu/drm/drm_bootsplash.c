#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/notifier.h>
#include <linux/keyboard.h>
#include <linux/completion.h>

#include <drm/drmP.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_modes.h>
#include <drm/drm_client.h>

#include "drm_internal.h"
#include "drm_splash.h"

static bool drm_bootsplash_enabled;
module_param_named(bootsplash_enabled, drm_bootsplash_enabled, bool, 0664);

static void drm_bootsplash_client_unregister(struct drm_client_dev *client);
static int drm_bootsplash_client_hotplug(struct drm_client_dev *client);

struct drm_bootsplash {
	struct drm_client_dev client;
	struct mutex lock;
	struct drm_client_display *display;
	struct drm_client_buffer *buffer;
	struct work_struct worker;
	struct completion xref;
	bool started;
	bool stop;
};

static void is_drm_bootsplash_enabled(struct device *dev)
{
	drm_bootsplash_enabled = of_property_read_bool(dev->of_node,
		"qcom,sde-drm-fb-splash-logo-enabled");
}

static void drm_bootsplash_buffer_delete(struct drm_bootsplash *splash)
{
	if (!IS_ERR_OR_NULL(splash->buffer))
		drm_client_framebuffer_delete(splash->buffer);
	splash->buffer = NULL;
}

static int drm_bootsplash_buffer_create(
			struct drm_bootsplash *splash, u32 width, u32 height)
{
	splash->buffer =
		drm_client_framebuffer_create(&splash->client,
				width, height, SPLASH_IMAGE_FORMAT);
	if (IS_ERR(splash->buffer)) {
		drm_bootsplash_buffer_delete(splash);
		return PTR_ERR(splash->buffer);
	}

	splash->buffer->vaddr =
		drm_client_buffer_vmap(splash->buffer);
	if (!splash->buffer->vaddr)
		DRM_ERROR("drm_client_buffer_vmap fail\n");

	return 0;
}

static int drm_bootsplash_display_probe(struct drm_bootsplash *splash)
{
	struct drm_client_dev *client = &splash->client;
	unsigned int width = 0, height = 0;
	unsigned int num_non_tiled = 0, i;
	unsigned int modeset_mask = 0;
	struct drm_mode_set *modeset;
	bool tiled = false;
	int ret;

	ret = drm_client_modeset_probe(client, 0, 0);
	if (ret)
		return ret;

	mutex_lock(&client->modeset_mutex);

	drm_client_for_each_modeset(modeset, client) {
		if (!modeset->mode)
			continue;

		if (modeset->connectors[0]->has_tile)
			tiled = true;
		else
			num_non_tiled++;
	}

	if (!tiled && !num_non_tiled) {
		drm_bootsplash_buffer_delete(splash);
		ret = -ENOENT;
		goto out;
	}

	/* Assume only one tiled monitor is possible */
	if (tiled) {
		int hdisplay = 0, vdisplay = 0;

		i = 0;
		drm_client_for_each_modeset(modeset, client) {
			i++;
			if (!modeset->connectors[0]->has_tile)
				continue;

			if (!modeset->y)
				hdisplay += modeset->mode->hdisplay;
			if (!modeset->x)
				vdisplay += modeset->mode->vdisplay;
			modeset_mask |= BIT(i - 1);
		}

		width = hdisplay;
		height = vdisplay;

		goto trim;
	}

	/* The rest have one display per modeset, pick the largest */
	i = 0;
	drm_client_for_each_modeset(modeset, client) {
		i++;
		if (!modeset->mode || modeset->connectors[0]->has_tile)
			continue;

		if (modeset->mode->hdisplay *
				modeset->mode->vdisplay > width * height) {
			width = modeset->mode->hdisplay;
			height = modeset->mode->vdisplay;
			modeset_mask = BIT(i - 1);
		}
	}

trim:
	i = 0;
	drm_client_for_each_modeset(modeset, client) {
		unsigned int j;

		if (modeset_mask & BIT(i++))
			continue;
		drm_mode_destroy(client->dev, modeset->mode);
		modeset->mode = NULL;

		for (j = 0; j < modeset->num_connectors; j++) {
			drm_connector_unreference(modeset->connectors[j]);
			modeset->connectors[j] = NULL;
		}
		modeset->num_connectors = 0;
	}

	if (!splash->buffer ||
	    splash->buffer->fb->width != width ||
	    splash->buffer->fb->height != height) {
		drm_bootsplash_buffer_delete(splash);
		ret = drm_bootsplash_buffer_create(splash, width, height);
	}

out:
	mutex_unlock(&client->modeset_mutex);

	return ret;
}

static int drm_bootsplash_display_commit_buffer(
			struct drm_bootsplash *splash)
{
	struct drm_client_dev *client = &splash->client;
	struct drm_mode_set *modeset;

	mutex_lock(&client->modeset_mutex);
	drm_client_for_each_modeset(modeset, client) {
		if (modeset->mode)
			modeset->fb = splash->buffer->fb;
	}
	mutex_unlock(&client->modeset_mutex);

	return drm_client_modeset_commit(client);
}

/* Draw a box for copying the image */
static void drm_bootsplash_draw_box(struct drm_client_buffer *buffer)
{
	unsigned int width = buffer->fb->width;
	unsigned int height = buffer->fb->height;
	unsigned int x, y, z;
	u32 *pix;

	pix = buffer->vaddr;
	pix += ((height / 2) - 50) * width;
	pix += (width / 2) - 50;

	z = 0;
	for (y = 0; y < SPLASH_IMAGE_HEIGHT; y++) {
		for (x = 0; x < SPLASH_IMAGE_WIDTH; x++)
			*pix++ = splash_bgr888_image[z++];
		pix += width - SPLASH_IMAGE_WIDTH;
	}
}

static int drm_bootsplash_draw(struct drm_bootsplash *splash)
{
	if (!splash->buffer)
		return -ENOENT;

	drm_bootsplash_draw_box(splash->buffer);

	return drm_bootsplash_display_commit_buffer(splash);
}

static void drm_bootsplash_worker(struct work_struct *work)
{
	struct drm_bootsplash *splash =
		container_of(work, struct drm_bootsplash, worker);
	struct drm_client_dev *client = &splash->client;
	struct drm_device *dev = client->dev;
	bool stop = false;
	int ret = 0;

	mutex_lock(&splash->lock);
	stop = splash->stop;
	ret = drm_bootsplash_draw(splash);
	mutex_unlock(&splash->lock);

	if (stop || ret == -ENOENT || ret == -EBUSY)
		goto skip;

	msleep(5000);
	splash->stop = true;
skip:
	drm_lastclose(dev);

	drm_bootsplash_buffer_delete(splash);

	DRM_DEBUG("Bootsplash has stopped (start=%u, stop=%u, ret=%d).\n",
			splash->started, splash->stop, ret);

	complete(&splash->xref);
}

static int drm_bootsplash_client_hotplug(struct drm_client_dev *client)
{
	struct drm_bootsplash *splash =
		container_of(client, struct drm_bootsplash, client);
	int ret = 0, retval;

	if (splash->stop)
		goto out_unlock;

	ret = drm_bootsplash_display_probe(splash);
	if (ret < 0) {
		if (splash->started && ret == -ENOENT)
			splash->stop = true;
		goto out_unlock;
	}

	if (!splash->started) {
		splash->started = true;
		reinit_completion(&splash->xref);
		schedule_work(&splash->worker);
		retval = wait_for_completion_interruptible(&splash->xref);
		if (retval < 0)
			DRM_ERROR("wait for bootsplash worker failed\n");
	}

out_unlock:
	return ret;
}

static const struct drm_client_funcs drm_bootsplash_client_funcs = {
	.owner          = THIS_MODULE,
	.unregister     = drm_bootsplash_client_unregister,
	.hotplug        = drm_bootsplash_client_hotplug,
};

static void drm_bootsplash_client_unregister(struct drm_client_dev *client)
{
	struct drm_bootsplash *splash =
		container_of(client, struct drm_bootsplash, client);

	mutex_lock(&splash->lock);
	splash->stop = true;
	mutex_unlock(&splash->lock);

	flush_work(&splash->worker);

	drm_client_release(client);
	kfree(splash);
}

void drm_bootsplash_client_register(struct drm_device *dev)
{
	struct drm_bootsplash *splash;
	int ret;

	is_drm_bootsplash_enabled(dev->dev);

	if (!drm_bootsplash_enabled)
		return;

	splash = kzalloc(sizeof(*splash), GFP_KERNEL);
	if (!splash)
		return;

	ret = drm_client_init(dev, &splash->client, "bootsplash",
			&drm_bootsplash_client_funcs);
	if (ret) {
		DRM_DEV_ERROR(dev->dev, "Fail to create client, ret=%d\n", ret);
		kfree(splash);
		return;
	}

	/* For this simple example only allow the first */
	drm_bootsplash_enabled = false;

	mutex_init(&splash->lock);

	INIT_WORK(&splash->worker, drm_bootsplash_worker);
	init_completion(&splash->xref);

	ret = drm_bootsplash_client_hotplug(&splash->client);
	if (ret)
		DRM_DEV_ERROR(dev->dev, "client hotplug ret=%d\n", ret);

	drm_client_register(&splash->client);
}

MODULE_DESCRIPTION("bootsplash");
