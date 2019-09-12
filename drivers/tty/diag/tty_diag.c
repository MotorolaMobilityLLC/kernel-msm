#include <linux/module.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/major.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/usb/usbdiag.h>
#include <linux/usb/tty_diag.h>

#define NUM_MDM_PORTS 2
#define NUM_MSM_PORTS 1

#define DIAG_MDM_START_IDX 0
#define DIAG_MSM_START_IDX (DIAG_MDM_START_IDX + NUM_MDM_PORTS)

#define DIAG_MAJOR 185
#define DIAG_TTY_MINOR_COUNT (NUM_MDM_PORTS + NUM_MSM_PORTS)

static DEFINE_SPINLOCK(diag_tty_lock);

static struct tty_driver *diag_tty_driver;

struct diag_tty_data {
	struct tty_port port;
	struct tty_struct *tty;
	int open_count;
	struct usb_diag_ch *legacy_ch;
	struct diag_request *d_req_ptr;
	const char *ch_name;
	int idx;
	int diag_packet_incomplete;
};

static struct diag_tty_data diag_tty[DIAG_TTY_MINOR_COUNT];

static int diag_tty_open(struct tty_struct *tty, struct file *f)
{
	int n = tty->index;
	struct diag_tty_data *tty_data;
	unsigned long flags;

	if (n < 0 || n >= DIAG_TTY_MINOR_COUNT)
		return -ENODEV;

	tty_data = diag_tty + n;

	if (tty_data->open_count >= 1)
		return -EBUSY;

	spin_lock_irqsave(&diag_tty_lock, flags);

	tty_data->tty = tty;
	tty->driver_data = tty_data;
	tty_data->open_count++;

	spin_unlock_irqrestore(&diag_tty_lock, flags);

	tty_data->legacy_ch->notify(tty_data->legacy_ch->priv,
		USB_DIAG_CONNECT,
		NULL);

	return 0;
}

static void diag_tty_close(struct tty_struct *tty, struct file *f)
{
	struct diag_tty_data *tty_data = tty->driver_data;
	unsigned long flags;

	if (tty_data == NULL)
		return;

	spin_lock_irqsave(&diag_tty_lock, flags);
	tty_data->open_count--;

	if (tty_data->open_count == 0) {
		tty_data->tty = NULL;
		tty->driver_data = NULL;
	}

	spin_unlock_irqrestore(&diag_tty_lock, flags);
}

static int diag_tty_write(struct tty_struct *tty,
				const unsigned char *buf, int len)
{
	struct diag_tty_data *tty_data = tty->driver_data;
	struct diag_request *d_req_temp;
	unsigned long flags;

	spin_lock_irqsave(&diag_tty_lock, flags);

	if(!tty_data) {
		spin_unlock_irqrestore(&diag_tty_lock, flags);
		return -ENODEV;
	}

	d_req_temp = tty_data->d_req_ptr;

	/* Make sure diag char driver is ready and no outstanding request */
	if (tty_data->d_req_ptr == NULL) {
		spin_unlock_irqrestore(&diag_tty_lock, flags);
		pr_err("ttydiag: error writing diag message, channel not ready\n");
		return -EAGAIN;
	}

	if (tty_data->legacy_ch->priv_usb) {
		spin_unlock_irqrestore(&diag_tty_lock, flags);
		pr_err("ttydiag: error writing diag message, channel in use\n");
		return -EAGAIN;
	}

	/* Diag packet must fit in buff and be written all at once */
	if (len > tty_data->d_req_ptr->length) {
		spin_unlock_irqrestore(&diag_tty_lock, flags);
		pr_err("ttydiag: error writing diag message, bad len: %d/%d\n",
			len, tty_data->d_req_ptr->length);
		return -EMSGSIZE;
	}

	/* Check whether fresh packet */
	if (!tty_data->diag_packet_incomplete) {
		memcpy(tty_data->d_req_ptr->buf, buf, len);
		tty_data->d_req_ptr->actual = len;
	} else {
		if (tty_data->d_req_ptr->actual + len >
			tty_data->d_req_ptr->length) {
			tty_data->d_req_ptr->actual = 0;
			tty_data->diag_packet_incomplete = 0;
			spin_unlock_irqrestore(&diag_tty_lock, flags);
			return -EMSGSIZE;
		} else {
			memcpy(tty_data->d_req_ptr->buf +
				tty_data->d_req_ptr->actual,
				buf,
				len);
			tty_data->d_req_ptr->actual += len;
		}
	}

	/* Check if packet is now complete */
	if (tty_data->d_req_ptr->buf[tty_data->d_req_ptr->actual - 1]
		!= 0x7E) {
		tty_data->diag_packet_incomplete = 1;
		spin_unlock_irqrestore(&diag_tty_lock, flags);
		return len;
	} else
		tty_data->diag_packet_incomplete = 0;

	/* Set active tty for responding */
	tty_data->legacy_ch->priv_usb = tty_data;

	spin_unlock_irqrestore(&diag_tty_lock, flags);
	tty_data->legacy_ch->notify(tty_data->legacy_ch->priv,
		USB_DIAG_READ_DONE,
		d_req_temp);

	return len;
}

static int diag_tty_write_room(struct tty_struct *tty)
{
	struct diag_tty_data *tty_data = tty->driver_data;

	if (!tty_data)
		return -ENODEV;

	if (tty_data->d_req_ptr == NULL || tty_data->legacy_ch->priv_usb)
		return 0;
	else
		return tty_data->d_req_ptr->length;
}

static int diag_tty_chars_in_buffer(struct tty_struct *tty)
{
	/* Data is always written when available, this should be changed if
	   write to userspace is changed to delayed work */
	return 0;
}

static const struct tty_operations diag_tty_ops = {
	.open = diag_tty_open,
	.close = diag_tty_close,
	.write = diag_tty_write,
	.write_room = diag_tty_write_room,
	.chars_in_buffer = diag_tty_chars_in_buffer,
};

/* Diag char driver ready */
struct usb_diag_ch *tty_diag_channel_open(const char *name, void *priv,
		void (*notify)(void *, unsigned, struct diag_request *),
		struct usb_diag_ch *legacy_ch)
{
	unsigned long flags;
	struct diag_tty_data *tty_data;
	int i;
	int start_idx;
	int num_ports;

	pr_info("ttydiag, registering channel %s\n", name);

	if(strcmp(DIAG_LEGACY, name) == 0) {
		start_idx = DIAG_MSM_START_IDX;
		num_ports = NUM_MSM_PORTS;
	} else if(strcmp(DIAG_MDM, name) == 0) {
		start_idx = DIAG_MDM_START_IDX;
		num_ports = NUM_MDM_PORTS;
	} else
		return ERR_PTR(-ENODEV);

	if(!legacy_ch) {
		legacy_ch = kzalloc(sizeof(*legacy_ch), GFP_KERNEL);
		legacy_ch->name = name;
		legacy_ch->priv = priv;
		legacy_ch->notify = notify;
	}

	legacy_ch->priv_usb = NULL;

	for (i=0; i < num_ports; i++) {
		tty_data = &diag_tty[start_idx + i];
		tty_data->idx = start_idx + i;
		tty_data->ch_name = name;

		if (tty_data->legacy_ch)
			return ERR_PTR(-EBUSY);

		spin_lock_irqsave(&diag_tty_lock, flags);
		tty_data->legacy_ch = legacy_ch;
		spin_unlock_irqrestore(&diag_tty_lock, flags);

		tty_port_init(&tty_data->port);
		tty_port_register_device(&tty_data->port, diag_tty_driver,
				tty_data->idx, NULL);

		pr_info("ttydiag, registered channel %s as ttydiag%d OK\n",
			name, start_idx + i);
	}

	return legacy_ch;
}

/* Diag char driver no longer ready */
void tty_diag_channel_close(struct usb_diag_ch *diag_ch)
{
	struct diag_tty_data *tty_data = diag_ch->priv_usb;
	unsigned long flags;

	pr_info("ttydiag, unregistering channel %s\n", diag_ch->name);

	if (tty_data) {
		tty_insert_flip_char(&tty_data->port, 0x00, TTY_BREAK);
		tty_flip_buffer_push(&tty_data->port);
		tty_unregister_device(diag_tty_driver, tty_data->idx);
		tty_port_destroy(&tty_data->port);

		spin_lock_irqsave(&diag_tty_lock, flags);
		tty_data->open_count = 0;
		diag_ch->priv_usb = NULL;
		tty_data->legacy_ch = NULL;
		spin_unlock_irqrestore(&diag_tty_lock, flags);

		pr_info("ttydiag, unregistered channel %s OK\n", diag_ch->name);
	}
}
EXPORT_SYMBOL(tty_diag_channel_close);

/* Diag char driver prepares tty driver to receive a write from userspace */
int tty_diag_channel_read(struct usb_diag_ch *diag_ch,
				struct diag_request *d_req)
{
	int i;
	struct diag_tty_data *tty_data;

	/* If channel is MSM, assign d_req for all MSM ports,
	 * If channel is MDM, assign d_req for all MDM ports
	 */
	for (i=0; i < DIAG_TTY_MINOR_COUNT; i++) {
		tty_data = &diag_tty[i];
		/* NULL pointers in strcmp bad */
		if(tty_data->ch_name && diag_ch->name) {
			if(strcmp(tty_data->ch_name, diag_ch->name) == 0)
				tty_data->d_req_ptr = d_req;
		}
	}

	return 0;
}
EXPORT_SYMBOL(tty_diag_channel_read);

/* Diag char driver has diag packet ready for userspace */
int tty_diag_channel_write(struct usb_diag_ch *diag_ch,
				struct diag_request *d_req)
{
	struct diag_tty_data *tty_data = diag_ch->priv_usb;
	unsigned char *tty_buf;
	int tty_allocated;
	unsigned long flags;

	if (tty_data == NULL)
		return -ENODEV;

	spin_lock_irqsave(&diag_tty_lock, flags);

	if (tty_data->tty == NULL) {
		spin_unlock_irqrestore(&diag_tty_lock, flags);
		return -EIO;
	}

	tty_allocated = tty_prepare_flip_string(&tty_data->port,
						&tty_buf, d_req->length);

	if (tty_allocated < d_req->length) {
		spin_unlock_irqrestore(&diag_tty_lock, flags);
		return -ENOMEM;
	}

	/* Unset active tty for next request diag tool */
	diag_ch->priv_usb = NULL;

	memcpy(tty_buf, d_req->buf, d_req->length);
	tty_flip_buffer_push(&tty_data->port);

	d_req->actual = d_req->length;
	spin_unlock_irqrestore(&diag_tty_lock, flags);

	diag_ch->notify(diag_ch->priv, USB_DIAG_WRITE_DONE_SYNC, d_req);

	return 0;
}
EXPORT_SYMBOL(tty_diag_channel_write);

static int __init diag_tty_init(void)
{
	int result;

	diag_tty_driver = alloc_tty_driver(DIAG_TTY_MINOR_COUNT);
	if (diag_tty_driver == NULL)
		return -ENOMEM;

	diag_tty_driver->owner = THIS_MODULE;
	diag_tty_driver->driver_name = "tty_diag";
	diag_tty_driver->name = "ttydiag";
	diag_tty_driver->major = DIAG_MAJOR;
	diag_tty_driver->minor_start = 0;
	diag_tty_driver->type = TTY_DRIVER_TYPE_SYSTEM;
	diag_tty_driver->subtype = SYSTEM_TYPE_TTY;
	diag_tty_driver->init_termios = tty_std_termios;
	diag_tty_driver->init_termios.c_cflag = B115200 | CS8 | CREAD;
	diag_tty_driver->init_termios.c_iflag = IGNBRK;
	diag_tty_driver->init_termios.c_oflag = 0;
	diag_tty_driver->init_termios.c_lflag = 0;
	diag_tty_driver->flags = TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV;

	spin_lock_init(&diag_tty_lock);
	tty_set_operations(diag_tty_driver, &diag_tty_ops);

	result = tty_register_driver(diag_tty_driver);
	if (result) {
		printk(KERN_ERR "Failed to register diag_tty driver.");
		put_tty_driver(diag_tty_driver);
		return result;
	}

	return 0;
}

module_init(diag_tty_init);

MODULE_DESCRIPTION("tty diag driver");
