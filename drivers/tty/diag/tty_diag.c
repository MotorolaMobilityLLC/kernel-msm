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

#include <mach/socinfo.h>
#include <mach/tty_diag.h>

#define DIAG_MAJOR 185
#define DIAG_MDM_MINOR_COUNT 3
#define DIAG_LEGACY_MINOR_COUNT 2
#define DIAG_LEGACY_MINOR_START DIAG_MDM_MINOR_COUNT
#define DIAG_MINOR_COUNT (DIAG_MDM_MINOR_COUNT + DIAG_LEGACY_MINOR_COUNT)

static DEFINE_MUTEX(diag_tty_lock);

static struct tty_driver *diag_tty_driver;
static struct legacy_diag_ch legacy_ch, mdm_ch;

struct diag_tty_data {
	struct tty_struct *tty;
	int open_count;
	struct legacy_diag_ch *ch_ptr; /* owner */
	struct diag_request *d_req_ptr;
};

static struct diag_tty_data diag_tty[DIAG_MINOR_COUNT];
static struct diag_tty_data *dbg_tty_target, *last_tty;

static int ttydiag_dbg_cmd_code, ttydiag_dbg_subsys_id, diag_packet_incomplete;

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

	tty_data = diag_tty + n;

	if (n < 0 || n >= DIAG_MINOR_COUNT)
		return -ENODEV;

	/* Diag kernel driver not ready */
	if (!(tty_data->ch_ptr->priv))
		return -EAGAIN;

	if (tty_data->open_count >= 1)
		return -EBUSY;

	mutex_lock(&diag_tty_lock);

	tty_data->tty = tty;
	tty->driver_data = tty_data;
	tty_data->open_count++;

	mutex_unlock(&diag_tty_lock);

	tty_data->ch_ptr->notify(tty_data->ch_ptr->priv,
				CHANNEL_DIAG_CONNECT, NULL);

	return 0;
}

static void diag_tty_close(struct tty_struct *tty, struct file *f)
{
	struct diag_tty_data *tty_data = tty->driver_data;
	int disconnect_channel = 1;
	int i;

	if (tty_data == NULL)
		return;

	mutex_lock(&diag_tty_lock);
	tty_data->open_count--;

	if (tty_data->ch_ptr == &mdm_ch) {
		for (i = 0; i < DIAG_MDM_MINOR_COUNT; i++) {
			if (diag_tty[i].open_count) {
				disconnect_channel = 0;
				break;
			}
		}
	} else if (tty_data->ch_ptr == &legacy_ch) {
		for (i = DIAG_LEGACY_MINOR_START; i < DIAG_MINOR_COUNT; i++) {
			if (diag_tty[i].open_count) {
				disconnect_channel = 0;
				break;
			}
		}
	}

	if (disconnect_channel && tty_data->ch_ptr->notify &&
			tty_data->ch_ptr->priv)
		tty_data->ch_ptr->priv_channel = NULL;
		/* Could send disconnect notification here but that doesn't
		   buy us anything */

	if (tty_data->open_count == 0)
		tty->driver_data = NULL;

	mutex_unlock(&diag_tty_lock);
}

static int diag_tty_write(struct tty_struct *tty,
				const unsigned char *buf, int len)
{
	struct diag_tty_data *tty_data = tty->driver_data;
	struct diag_request *d_req = tty_data->d_req_ptr;

	mutex_lock(&diag_tty_lock);

	/* Make sure diag char driver is ready and no outstanding request */
	if ((d_req == NULL) || tty_data->ch_ptr->priv_channel) {
		mutex_unlock(&diag_tty_lock);
		return -EAGAIN;
	}

	/* Diag packet must fit in buff and be written all at once */
	if (len > d_req->length) {
		mutex_unlock(&diag_tty_lock);
		return -EMSGSIZE;
	}

	/* Check whether fresh packet */
	if (!diag_packet_incomplete) {
		memcpy(d_req->buf, buf, len);
		d_req->actual = len;
	} else {
		if (d_req->actual + len > d_req->length) {
			d_req->actual = 0;
			diag_packet_incomplete = 0;
			return -EMSGSIZE;
		} else {
			memcpy(d_req->buf + d_req->actual, buf, len);
			d_req->actual += len;
		}
	}

	/* Check if packet is now complete */
	if (d_req->buf[d_req->actual - 1] != 0x7E) {
		diag_packet_incomplete = 1;
		mutex_unlock(&diag_tty_lock);
		return len;
	} else
		diag_packet_incomplete = 0;

	if (dbg_ftm_flag == 1 && tty_data->ch_ptr == &mdm_ch) {
		if (tty_data != NULL) {
			dbg_tty_target = tty_data;
			ttydiag_dbg_cmd_code =
				(int)(*(unsigned char *)d_req->buf);
			ttydiag_dbg_subsys_id =
				(int)(*(unsigned char *)(d_req->buf+1));
		}
	}

	/* Set active tty for responding */
	tty_data->ch_ptr->priv_channel = tty_data;
	last_tty = tty_data;

	mutex_unlock(&diag_tty_lock);

	tty_data->ch_ptr->notify(tty_data->ch_ptr->priv,
				CHANNEL_DIAG_READ_DONE, d_req);

	return len;
}

static int diag_tty_write_room(struct tty_struct *tty)
{
	struct diag_tty_data *tty_data = tty->driver_data;

	if (tty_data->d_req_ptr == NULL || tty_data->ch_ptr->priv_channel)
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

/* Diag master is ready */
struct legacy_diag_ch *tty_diag_channel_open(const char *name, void *priv,
		void (*notify)(void *, unsigned, struct diag_request *))
{
	int i;

	mutex_lock(&diag_tty_lock);

	if (!strncmp(name, DIAG_LEGACY, 9) && cpu_is_apq8064()) {
		legacy_ch.priv = priv;
		legacy_ch.notify = notify;

		for (i = DIAG_LEGACY_MINOR_START; i < DIAG_MINOR_COUNT; i++) {
			tty_register_device(diag_tty_driver, i, NULL);
			diag_tty[i].ch_ptr = &legacy_ch;
		}

		mutex_unlock(&diag_tty_lock);

		return &legacy_ch;
	} else if ((!strncmp(name, DIAG_MDM, 9) && cpu_is_apq8064()) ||
			(!strcmp(name, DIAG_LEGACY) && (cpu_is_msm8960() ||
			cpu_is_msm8960ab()))) {
		mdm_ch.priv = priv;
		mdm_ch.notify = notify;

		for (i = 0; i < DIAG_MDM_MINOR_COUNT; i++) {
			tty_register_device(diag_tty_driver, i, NULL);
			diag_tty[i].ch_ptr = &mdm_ch;
		}

		mutex_unlock(&diag_tty_lock);

		return &mdm_ch;
	}

	mutex_unlock(&diag_tty_lock);

	return ERR_PTR(-EINVAL);
}
EXPORT_SYMBOL(tty_diag_channel_open);

/* Diag master no longer ready */
void tty_diag_channel_close(struct legacy_diag_ch *diag_ch)
{
	struct diag_tty_data *priv_channel = diag_ch->priv_channel;
	int i;

	if (diag_ch->priv_channel) {
		tty_insert_flip_char(priv_channel->tty, 0x00, TTY_BREAK);
		tty_flip_buffer_push(priv_channel->tty);
	}
	mutex_lock(&diag_tty_lock);
	diag_ch->priv = NULL;
	diag_ch->notify = NULL;

	if (diag_ch == &mdm_ch) {
		for (i = DIAG_MDM_MINOR_COUNT - 1; i >= 0; i--)
			tty_unregister_device(diag_tty_driver, i);
	} else if (diag_ch == &legacy_ch) {
		for (i = DIAG_MINOR_COUNT - 1;
			i >= DIAG_LEGACY_MINOR_START; i--)
			tty_unregister_device(diag_tty_driver, i);
	}

	mutex_unlock(&diag_tty_lock);
}
EXPORT_SYMBOL(tty_diag_channel_close);

/* Diag char driver prepares tty driver to receive a write from userspace */
int tty_diag_channel_read(struct legacy_diag_ch *diag_ch,
				struct diag_request *d_req)
{
	int i;

	if (diag_ch == &mdm_ch) {
		for (i = 0; i < DIAG_MDM_MINOR_COUNT; i++)
			diag_tty[i].d_req_ptr = d_req;
	} else if (diag_ch == &legacy_ch) {
		for (i = DIAG_LEGACY_MINOR_START; i < DIAG_MINOR_COUNT; i++)
			diag_tty[i].d_req_ptr = d_req;
	}

	return 0;
}
EXPORT_SYMBOL(tty_diag_channel_read);

/* Diag char driver has diag packet ready for userspace */
int tty_diag_channel_write(struct legacy_diag_ch *diag_ch,
				struct diag_request *d_req)
{
	struct diag_tty_data *tty_data = diag_ch->priv_channel;
	unsigned char *tty_buf;
	int tty_allocated;
	int cmd_code, subsys_id;

	/* If diag packet is not 1:1 response (perhaps logging packet?),
	   try last tty */
	if (tty_data == NULL)
		tty_data = last_tty;

	mutex_lock(&diag_tty_lock);

	if (dbg_ftm_flag == 1 && diag_ch == &mdm_ch) {
		cmd_code = (int)(*(unsigned char *)d_req->buf);
		subsys_id = (int)(*(unsigned char *)(d_req->buf+1));

		if (cmd_code == ttydiag_dbg_cmd_code &&
				subsys_id == ttydiag_dbg_subsys_id &&
				dbg_tty_target != NULL) {
			/* respond to last tty */
			ttydiag_dbg_cmd_code = -1;
			ttydiag_dbg_subsys_id = -1;
			tty_data = dbg_tty_target;
			dbg_tty_target = NULL;
		} else {
			tty_data = &(diag_tty[2]);
		}
	}

	if (tty_data->tty == NULL) {
		mutex_unlock(&diag_tty_lock);
		return -EIO;
	}

	tty_allocated = tty_prepare_flip_string(tty_data->tty,
						&tty_buf, d_req->length);

	if (tty_allocated < d_req->length) {
		mutex_unlock(&diag_tty_lock);
		return -ENOMEM;
	}

	/* Unset active tty for next request diag tool */
	diag_ch->priv_channel = NULL;

	memcpy(tty_buf, d_req->buf, d_req->length);
	tty_flip_buffer_push(tty_data->tty);

	mutex_unlock(&diag_tty_lock);

	diag_ch->notify(diag_ch->priv, CHANNEL_DIAG_WRITE_DONE, d_req);

	return 0;
}
EXPORT_SYMBOL(tty_diag_channel_write);

void tty_diag_channel_abandon_request()
{
	mutex_lock(&diag_tty_lock);
	legacy_ch.priv_channel = NULL;
	mdm_ch.priv_channel = NULL;
	mutex_unlock(&diag_tty_lock);
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
	legacy_ch.priv_channel = NULL;
	mdm_ch.notify = NULL;
	mdm_ch.priv = NULL;
	mdm_ch.priv_channel = NULL;
	diag_packet_incomplete = 0;
	dbg_tty_target = NULL;
	last_tty = &(diag_tty[0]);

	diag_tty_driver = alloc_tty_driver(DIAG_MINOR_COUNT);
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
