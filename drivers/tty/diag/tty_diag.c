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
#include <linux/usb/usbdiag.h>

#include <linux/usb/tty_diag.h>

#define DIAG_MAJOR 185
#define DIAG_TTY_MINOR_COUNT 3

static DEFINE_SPINLOCK(diag_tty_lock);

static struct tty_driver *diag_tty_driver;
static struct usb_diag_ch legacy_ch;
static struct diag_request *d_req_ptr;

struct diag_tty_data {
	struct tty_port port;
	struct tty_struct *tty;
	int open_count;
};

static struct diag_tty_data diag_tty[DIAG_TTY_MINOR_COUNT];
static int diag_packet_incomplete;

static int ttydiag_dbg_cmd_code, ttydiag_dbg_subsys_id, dbg_tty_minor;

/* 1 - would add channel dispatch checking logic
 *
 *	by default, this is off (0)
 *
 *
 *	try `echo "1">/sys/class/diag/diag/dbg_ftm' to turn it on.
 */
static int dbg_ftm_flag;

static int diag_tty_open(struct tty_struct *tty, struct file *f)
{
	int n = tty->index;
	struct diag_tty_data *tty_data;
	unsigned long flags;

	tty_data = diag_tty + n;

	if (n < 0 || n >= DIAG_TTY_MINOR_COUNT)
		return -ENODEV;

	if (tty_data->open_count >= 1)
		return -EBUSY;

	spin_lock_irqsave(&diag_tty_lock, flags);

	tty_data->tty = tty;
	tty->driver_data = tty_data;
	tty_data->open_count++;

	spin_unlock_irqrestore(&diag_tty_lock, flags);

	legacy_ch.notify(legacy_ch.priv, USB_DIAG_CONNECT, NULL);

	return 0;
}

static void diag_tty_close(struct tty_struct *tty, struct file *f)
{
	struct diag_tty_data *tty_data = tty->driver_data;
	int disconnect_channel = 1;
	int i;
	unsigned long flags;

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

	if (disconnect_channel && legacy_ch.notify && legacy_ch.priv)
		legacy_ch.priv_usb = NULL;

	if (tty_data->open_count == 0)
		tty->driver_data = NULL;

	spin_unlock_irqrestore(&diag_tty_lock, flags);
}

static int diag_tty_write(struct tty_struct *tty,
				const unsigned char *buf, int len)
{
	struct diag_tty_data *tty_data = tty->driver_data;
	struct diag_request *d_req_temp = d_req_ptr;
	unsigned long flags;

	spin_lock_irqsave(&diag_tty_lock, flags);

	/* Make sure diag char driver is ready and no outstanding request */
	if ((d_req_ptr == NULL) || legacy_ch.priv_usb) {
		spin_unlock_irqrestore(&diag_tty_lock, flags);
		return -EAGAIN;
	}

	/* Diag packet must fit in buff and be written all at once */
	if (len > d_req_ptr->length) {
		spin_unlock_irqrestore(&diag_tty_lock, flags);
		return -EMSGSIZE;
	}

	/* Check whether fresh packet */
	if (!diag_packet_incomplete) {
		memcpy(d_req_ptr->buf, buf, len);
		d_req_ptr->actual = len;
	} else {
		if (d_req_ptr->actual + len > d_req_ptr->length) {
			d_req_ptr->actual = 0;
			diag_packet_incomplete = 0;
			spin_unlock_irqrestore(&diag_tty_lock, flags);
			return -EMSGSIZE;
		} else {
			memcpy(d_req_ptr->buf + d_req_ptr->actual, buf, len);
			d_req_ptr->actual += len;
		}
	}

	/* Check if packet is now complete */
	if (d_req_ptr->buf[d_req_ptr->actual - 1] != 0x7E) {
		diag_packet_incomplete = 1;
		spin_unlock_irqrestore(&diag_tty_lock, flags);
		return len;
	} else
		diag_packet_incomplete = 0;

	if (dbg_ftm_flag == 1) {

		if (tty_data != NULL) {
			if (!strncmp(tty_data->tty->name, "ttydiag0",
						strlen("ttydiag0"))) {
				dbg_tty_minor = 0;
				ttydiag_dbg_cmd_code =
					(int)(*(char *)d_req_ptr->buf);
				ttydiag_dbg_subsys_id =
					(int)(*(char *)(d_req_ptr->buf+1));
			}
			if (!strncmp(tty_data->tty->name, "ttydiag1",
						strlen("ttydiag1"))) {
				dbg_tty_minor = 1;
				ttydiag_dbg_cmd_code =
					(int)(*(char *)d_req_ptr->buf);
				ttydiag_dbg_subsys_id =
					(int)(*(char *)(d_req_ptr->buf+1));
			}
		}
	}

	/* Set active tty for responding */
	legacy_ch.priv_usb = tty_data;

	spin_unlock_irqrestore(&diag_tty_lock, flags);
	legacy_ch.notify(legacy_ch.priv, USB_DIAG_READ_DONE, d_req_temp);

	return len;
}

static int diag_tty_write_room(struct tty_struct *tty)
{
	if ((d_req_ptr == NULL) || legacy_ch.priv_usb)
		return 0;
	else
		return d_req_ptr->length;
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

	if(strcmp(DIAG_LEGACY, name) != 0)
		return ERR_PTR(-EINVAL);

	if (legacy_ch.priv != NULL)
		return ERR_PTR(-EBUSY);

	spin_lock_init(&diag_tty_lock);
	spin_lock_irqsave(&diag_tty_lock, flags);
	legacy_ch.priv = priv;
	legacy_ch.notify = notify;
	spin_unlock_irqrestore(&diag_tty_lock, flags);

	for (i = 0; i < DIAG_TTY_MINOR_COUNT; i++) {
		tty_port_init(&diag_tty[i].port);
		tty_port_register_device(&diag_tty[i].port, diag_tty_driver,
				i, NULL);
	}

	return &legacy_ch;
}
EXPORT_SYMBOL(tty_diag_channel_open);

/* Diag char driver no longer ready */
void tty_diag_channel_close(struct usb_diag_ch *diag_ch)
{
	struct diag_tty_data *priv_usb = diag_ch->priv_usb;
	unsigned long flags;
	int i;

	if (diag_ch->priv_usb) {
		tty_insert_flip_char(&priv_usb->port, 0x00, TTY_BREAK);
		tty_flip_buffer_push(&priv_usb->port);
	}
	spin_lock_irqsave(&diag_tty_lock, flags);
	diag_ch->priv = NULL;
	diag_ch->notify = NULL;

	for (i = DIAG_TTY_MINOR_COUNT - 1; i >= 0; i--)
		tty_unregister_device(diag_tty_driver, i);

	spin_unlock_irqrestore(&diag_tty_lock, flags);
}
EXPORT_SYMBOL(tty_diag_channel_close);

/* Diag char driver prepares tty driver to receive a write from userspace */
int tty_diag_channel_read(struct usb_diag_ch *diag_ch,
				struct diag_request *d_req)
{
	d_req_ptr = d_req;

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
	int cmd_code, subsys_id;

	/* If diag packet is not 1:1 response (perhaps logging packet?),
	   try primary channel */
	if (tty_data == NULL)
		tty_data = &(diag_tty[0]);

	spin_lock_irqsave(&diag_tty_lock, flags);

	if (dbg_ftm_flag == 1) {
		cmd_code = (int)(*(char *)d_req->buf);
		subsys_id = (int)(*(char *)(d_req->buf+1));

		if (cmd_code == ttydiag_dbg_cmd_code &&
				subsys_id == ttydiag_dbg_subsys_id &&
				dbg_tty_minor != -1) {
			/* respond to last tty */
			ttydiag_dbg_cmd_code = 0;
			ttydiag_dbg_subsys_id = 0;
			tty_data = &(diag_tty[dbg_tty_minor]);
			dbg_tty_minor = -1;
		} else {
			tty_data = &(diag_tty[2]);
		}
	}

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

void tty_diag_channel_abandon_request()
{
	unsigned long flags;

	spin_lock_irqsave(&diag_tty_lock, flags);
	legacy_ch.priv_usb = NULL;
	spin_unlock_irqrestore(&diag_tty_lock, flags);
}
EXPORT_SYMBOL(tty_diag_channel_abandon_request);

int tty_diag_get_dbg_ftm_flag_value()
{
	return dbg_ftm_flag;
}
EXPORT_SYMBOL(tty_diag_get_dbg_ftm_flag_value);

int tty_diag_set_dbg_ftm_flag_value(int val)
{
	dbg_ftm_flag = val;
	return 0;
}
EXPORT_SYMBOL(tty_diag_set_dbg_ftm_flag_value);

static int __init diag_tty_init(void)
{
	int result;

	legacy_ch.notify = NULL;
	legacy_ch.priv = NULL;
	legacy_ch.priv_usb = NULL;
	diag_packet_incomplete = 0;
	dbg_tty_minor = -1;

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
