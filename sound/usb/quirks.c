// SPDX-License-Identifier: GPL-2.0
/*
 * xhci quirk driver
 *
 * Copyright (c) 2025 Motorola Inc.
 */
#include <linux/platform_device.h>
#include <linux/usb/audio.h>
#include <linux/usb/quirks.h>
#include <linux/spinlock.h>
#include <linux/stringhash.h>
#include "quirks.h"

#include <sound/asound.h>
#include <sound/core.h>
#include "card.h"

unsigned int uac_ctrl_timeout_ms = 3000; /* 3 sec */
module_param(uac_ctrl_timeout_ms, uint, 0644);

struct usb_audio_quirk_flags_table {
	u32 id;
	u32 flags;
};

static struct snd_usb_audio *usb_chip[SNDRV_CARDS];

#define DEVICE_FLG(vid, pid, _flags) \
	{ .id = USB_ID(vid, pid), .flags = (_flags) }
#define VENDOR_FLG(vid, _flags) DEVICE_FLG(vid, 0, _flags)


/* quirk list in usbcore */
static const struct usb_device_id usb_quirk_list[] = {
	/* AM33/CM33 HeadSet */
	{USB_DEVICE(0x12d1, 0x3a07), .driver_info = USB_QUIRK_IGNORE_REMOTE_WAKEUP|USB_QUIRK_RESET},

	{ }  /* terminating entry must be last */
};

/* quirk list in /sound/usb */
static const struct usb_audio_quirk_flags_table snd_quirk_flags_table[] = {
		/* Device matches */
		DEVICE_FLG(0x12d1, 0x3a07,	/* AM33/CM33 HeadSet */
		   QUIRK_FLAG_CTL_MSG_DELAY),
		DEVICE_FLG(0x1532, 0x0504,
		   QUIRK_FLAG_CTL_MSG_DELAY),
		{} /* terminator */
};


static int usb_match_device(struct usb_device *dev, const struct usb_device_id *id)
{
	if ((id->match_flags & USB_DEVICE_ID_MATCH_VENDOR) &&
		id->idVendor != le16_to_cpu(dev->descriptor.idVendor))
		return 0;

	if ((id->match_flags & USB_DEVICE_ID_MATCH_PRODUCT) &&
		id->idProduct != le16_to_cpu(dev->descriptor.idProduct))
		return 0;

	/* No need to test id->bcdDevice_lo != 0, since 0 is never */
	/*   greater than any unsigned number. */
	if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_LO) &&
		(id->bcdDevice_lo > le16_to_cpu(dev->descriptor.bcdDevice)))
		return 0;

	if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_HI) &&
		(id->bcdDevice_hi < le16_to_cpu(dev->descriptor.bcdDevice)))
		return 0;

	if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_CLASS) &&
		(id->bDeviceClass != dev->descriptor.bDeviceClass))
		return 0;

	if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_SUBCLASS) &&
		(id->bDeviceSubClass != dev->descriptor.bDeviceSubClass))
		return 0;

	if ((id->match_flags & USB_DEVICE_ID_MATCH_DEV_PROTOCOL) &&
		(id->bDeviceProtocol != dev->descriptor.bDeviceProtocol))
		return 0;

	return 1;
}

static u32 usb_detect_static_quirks(struct usb_device *udev,
					const struct usb_device_id *id)
{
	u32 quirks = 0;

	for (; id->match_flags; id++) {
		if (!usb_match_device(udev, id))
			continue;

		quirks |= (u32)(id->driver_info);
		dev_info(&udev->dev,
			  "Set usbcore quirk_flags 0x%x for device %04x:%04x\n",
			  (u32)id->driver_info, id->idVendor,
			  id->idProduct);
	}

	return quirks;
}

static void xhci_usb_free_format(struct audioformat *fp)
{
	list_del(&fp->list);
	kfree(fp->rate_table);
	kfree(fp->chmap);
	kfree(fp);
}

static void xhci_usb_format_quirk(struct snd_usb_audio *chip)
{
	struct snd_usb_stream *as;
	struct snd_usb_substream *subs;
	struct audioformat *fp, *n;

	/* Restrict the playback format for bestechnic audio device */
	if (chip->usb_id == USB_ID(0xbe57, 0x0238)) {
		dev_info(&chip->dev->dev, "Restrict the playback format to 16 bits\n");
		/* list all streams */
		list_for_each_entry(as, &chip->pcm_list, list) {
			subs = &as->substream[SNDRV_PCM_STREAM_PLAYBACK];
			/* check if the stream is initialized */
			if (subs->num_formats) {
				/* check and remove the unsupported format from the list */
				list_for_each_entry_safe(fp, n, &subs->fmt_list, list) {
					if (fp->fmt_bits > 16) {
						subs->num_formats--;
						subs->formats &= ~(fp->formats);
						xhci_usb_free_format(fp);
					}
				}
			}
		}
	}
}

void xhci_init_snd_quirk(struct snd_usb_audio *chip)
{
	const struct usb_audio_quirk_flags_table *p;

	if (chip->index >= 0 && chip->index <SNDRV_CARDS)
		usb_chip[chip->index] = chip;

	for (p = snd_quirk_flags_table; p->id; p++) {
		if (chip->usb_id == p->id ||
			(!USB_ID_PRODUCT(p->id) &&
			 USB_ID_VENDOR(chip->usb_id) == USB_ID_VENDOR(p->id))) {
			dev_info(&chip->dev->dev,
					  "Set audio quirk_flags 0x%x for device %04x:%04x\n",
					  p->flags, USB_ID_VENDOR(chip->usb_id),
					  USB_ID_PRODUCT(chip->usb_id));
			chip->quirk_flags |= p->flags;
			return;
		}
	}

	xhci_usb_format_quirk(chip);
}

void xhci_deinit_snd_quirk(struct snd_usb_audio *chip)
{
	if (chip->index >= 0 && chip->index <SNDRV_CARDS)
		usb_chip[chip->index] = NULL;
}

/* update usbcore quirk */
void xhci_apply_quirk(struct usb_device *udev)
{
	if (!udev)
		return;

	udev->quirks = usb_detect_static_quirks(udev, usb_quirk_list);
}
