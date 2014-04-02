#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/major.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/slab.h>

#include <mach/tty_diag.h>
#include <mach/usbdiag.h>

#define DIAG_MAJOR 185
#define DIAG_TTY_MINOR_COUNT 3

static DEFINE_SPINLOCK(diag_tty_lock);

static struct tty_driver *diag_tty_driver;

static LIST_HEAD(tty_diag_ch_list);

struct diag_tty_data {
	struct tty_port port;
	struct tty_struct *tty;
	struct usb_diag_ch *ch;
	int diag_packet_incomplete;
	int open_count;
};

static struct diag_tty_data diag_tty[DIAG_TTY_MINOR_COUNT];

static int port_registered;

struct usb_diag_ch *get_tty_ch(struct tty_struct *tty)
{
	int found = 0;
	unsigned long flags;
	struct usb_diag_ch *ch;

	spin_lock_irqsave(&diag_tty_lock, flags);
	if (!strncmp(tty->name, "ttydiag0",
						strlen("ttydiag0"))) {

		list_for_each_entry(ch, &tty_diag_ch_list, list) {
			if (!strcmp(DIAG_MDM, ch->name)) {
				found = 1;
				break;
			}
		}
	}

	if (!strncmp(tty->name, "ttydiag1",
						strlen("ttydiag1"))) {
		list_for_each_entry(ch, &tty_diag_ch_list, list) {
			if (!strcmp(DIAG_LEGACY, ch->name)) {
				found = 1;
				break;
			}
		}
	}

	if (!strncmp(tty->name, "ttydiag2",
						strlen("ttydiag2"))) {

		list_for_each_entry(ch, &tty_diag_ch_list, list) {
			if (!strcmp(DIAG_MDM, ch->name)) {
				found = 1;
				break;
			}
		}
	}

	spin_unlock_irqrestore(&diag_tty_lock, flags);

	if (found == 0) {
			pr_err("ttydiag Driver is not found!\n");
			return  ERR_PTR(-ENOENT);
	}

	return ch;

}
static int diag_tty_open(struct tty_struct *tty, struct file *f)
{
	int n = tty->index;
	struct diag_tty_data *tty_data;
	unsigned long flags;
	struct usb_diag_ch *ch;

	tty_data = diag_tty + n;

	if (n < 0 || n >= DIAG_TTY_MINOR_COUNT)
		return -ENODEV;

	ch = get_tty_ch(tty);
	if (ch < 0)
		return  (int)ch;

	/* Diag kernel driver not ready */
	if (!(ch->priv))
		return -EAGAIN;

	if (tty_data->open_count >= 1)
		return -EBUSY;

	spin_lock_irqsave(&diag_tty_lock, flags);

	tty_data->tty = tty;
	tty_data->ch = ch;
	tty_data->diag_packet_incomplete = 0;

	tty->driver_data = tty_data;
	tty_data->open_count++;

	spin_unlock_irqrestore(&diag_tty_lock, flags);

	ch->notify(ch->priv, USB_DIAG_CONNECT, NULL);

	return 0;
}

static void diag_tty_close(struct tty_struct *tty, struct file *f)
{
	struct diag_tty_data *tty_data = tty->driver_data;
	int disconnect_channel = 1;
	int i;
	unsigned long flags;
	struct usb_diag_ch *ch;

	if (tty_data == NULL)
		return;

	spin_lock_irqsave(&diag_tty_lock, flags);
	tty_data->open_count--;

	for (i = 0; i < DIAG_TTY_MINOR_COUNT; i++) {
		if (diag_tty[i].open_count) {
			disconnect_channel = 0;
			break;
		}
	}

	ch = tty_data->ch;

	if (disconnect_channel && ch->notify && ch->priv)
		ch->priv_usb = NULL;

	if (tty_data->open_count == 0)
		tty->driver_data = NULL;

	spin_unlock_irqrestore(&diag_tty_lock, flags);
}

static int diag_tty_write(struct tty_struct *tty,
				const unsigned char *buf, int len)
{
	struct diag_tty_data *tty_data;
	struct diag_request *d_req_temp;
	unsigned long flags;
	struct usb_diag_ch *ch;

	spin_lock_irqsave(&diag_tty_lock, flags);

	tty_data = tty->driver_data;

	ch = tty_data->ch;

	d_req_temp = ch->d_req_ptr;

	/* Make sure diag char driver is ready and no outstanding request */
	if ((ch->d_req_ptr == NULL) || ch->priv_usb) {
		spin_unlock_irqrestore(&diag_tty_lock, flags);
		return -EAGAIN;
	}

	/* Diag packet must fit in buff and be written all at once */
	if (len > ch->d_req_ptr->length) {
		spin_unlock_irqrestore(&diag_tty_lock, flags);
		return -EMSGSIZE;
	}

	/* Check whether fresh packet */
	if (!tty_data->diag_packet_incomplete) {
		memcpy(ch->d_req_ptr->buf, buf, len);
		ch->d_req_ptr->actual = len;
	} else {
		if (ch->d_req_ptr->actual + len
				> ch->d_req_ptr->length) {
			ch->d_req_ptr->actual = 0;
			tty_data->diag_packet_incomplete = 0;
			spin_unlock_irqrestore(&diag_tty_lock, flags);
			return -EMSGSIZE;
		} else {
			memcpy(ch->d_req_ptr->buf +
				 ch->d_req_ptr->actual, buf, len);
			ch->d_req_ptr->actual += len;
		}
	}

	/* Check if packet is now complete */
	if (ch->d_req_ptr->buf[ch->d_req_ptr->actual - 1] != 0x7E) {
		tty_data->diag_packet_incomplete = 1;
		spin_unlock_irqrestore(&diag_tty_lock, flags);
		return len;
	} else
		tty_data->diag_packet_incomplete = 0;

	/* Set active tty for responding */
	ch->priv_usb = tty_data;

	spin_unlock_irqrestore(&diag_tty_lock, flags);
	ch->notify(ch->priv, USB_DIAG_READ_DONE, d_req_temp);

	return len;
}

static int diag_tty_write_room(struct tty_struct *tty)
{
	struct usb_diag_ch *ch;
	struct diag_tty_data *tty_data = tty->driver_data;

	ch = tty_data->ch;

	if (ch	< 0)
		return  (int)ch;

	if ((ch->d_req_ptr == NULL) || ch->priv_usb)
		return 0;
	else
		return ch->d_req_ptr->length;
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
		void (*notify)(void *, unsigned, struct diag_request *))
{
	int i;
	unsigned long flags;
	struct usb_diag_ch *ch;
	int found = 0;

	spin_lock_irqsave(&diag_tty_lock, flags);
	/* Check if we already have a channel with this name */
	list_for_each_entry(ch, &tty_diag_ch_list, list) {
		if (!strcmp(name, ch->name)) {
			found = 1;
			break;
		}
	}
	spin_unlock_irqrestore(&diag_tty_lock, flags);

	if (!found) {
		ch = kzalloc(sizeof(*ch), GFP_KERNEL);
		if (!ch)
			return ERR_PTR(-ENOMEM);
	}

	ch->name = name;
	ch->priv = priv;
	ch->notify = notify;
	ch->d_req_ptr = NULL;

	if (port_registered == 0) {
		for (i = 0; i < DIAG_TTY_MINOR_COUNT; i++) {
			tty_port_init(&diag_tty[i].port);
			tty_port_register_device(&diag_tty[i].port,
				diag_tty_driver, i, NULL);
		}
	}
	spin_lock_irqsave(&diag_tty_lock, flags);
	port_registered++;
	list_add_tail(&ch->list, &tty_diag_ch_list);
	spin_unlock_irqrestore(&diag_tty_lock, flags);


	return ch;
}
EXPORT_SYMBOL(tty_diag_channel_open);

/* Diag char driver no longer ready */
void tty_diag_channel_close(struct usb_diag_ch *diag_ch)
{
	struct diag_tty_data *priv_usb = diag_ch->priv_usb;
	unsigned long flags;
	int i;
	int close_port = 0;

	if (diag_ch->priv_usb) {
		tty_insert_flip_char(&priv_usb->port, 0x00, TTY_BREAK);
		tty_flip_buffer_push(&priv_usb->port);
	}
	spin_lock_irqsave(&diag_tty_lock, flags);
	diag_ch->priv = NULL;
	diag_ch->notify = NULL;
	diag_ch->name = NULL;
	diag_ch->priv_usb = NULL;

	list_del(&diag_ch->list);
	kfree(diag_ch);

	port_registered--;
	if (port_registered == 0)
		close_port = 1;
	spin_unlock_irqrestore(&diag_tty_lock, flags);

	if (close_port == 1) {
		for (i = DIAG_TTY_MINOR_COUNT - 1; i >= 0; i--)
			tty_unregister_device(diag_tty_driver, i);
	}

}
EXPORT_SYMBOL(tty_diag_channel_close);

/* Diag char driver prepares tty driver to receive a write from userspace */
int tty_diag_channel_read(struct usb_diag_ch *diag_ch,
				struct diag_request *d_req)
{
	unsigned long flags;

	spin_lock_irqsave(&diag_tty_lock, flags);

	diag_ch->d_req_ptr = d_req;

	spin_unlock_irqrestore(&diag_tty_lock, flags);

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

	/* If diag packet is not 1:1 response (perhaps logging packet?),
	   try primary channel */
	if (tty_data == NULL)
		tty_data = &(diag_tty[0]);

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

	spin_unlock_irqrestore(&diag_tty_lock, flags);

	diag_ch->notify(diag_ch->priv, USB_DIAG_WRITE_DONE, d_req);

	return 0;
}
EXPORT_SYMBOL(tty_diag_channel_write);

void tty_diag_channel_abandon_request(struct usb_diag_ch *diag_ch)
{
	unsigned long flags;

	spin_lock_irqsave(&diag_tty_lock, flags);
	diag_ch->priv_usb = NULL;
	spin_unlock_irqrestore(&diag_tty_lock, flags);
}
EXPORT_SYMBOL(tty_diag_channel_abandon_request);

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
