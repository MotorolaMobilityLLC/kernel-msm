
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/err.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/export.h>
#include <linux/mutex.h>
#include <linux/debugfs.h>
#include <linux/scatterlist.h>
#include <linux/uaccess.h>

#include "siw_touch_mon.h"

struct siw_mon_txt_ptr {
	int cnt, limit;
	char *pbuf;
};

struct siw_mon_event_txt {
	struct list_head e_link;
	ktime_t time;
	int type;
	struct siw_mon_data data;
};

enum {
	SIW_MON_EVENT_MAX = ((PAGE_SIZE<<2) / sizeof(struct siw_mon_data)),
};

enum {
	SIW_MON_PRINT_SIZE = 250   /* with 5 ISOs segs */
};

enum {
	SLAB_NAME_SZ = 30,
};

struct siw_mon_reader_txt {
	struct kmem_cache *e_slab;
	int nevents;
	struct list_head e_list;
	/* In C, parent class can be placed anywhere */
	struct siw_mon_reader r;

	wait_queue_head_t wait;
	int printf_size;
	char *printf_buf;
	struct mutex printf_lock;

	char slab_name[SLAB_NAME_SZ];

	atomic_t read_running;
};


#define __siw_mon_prt_snprintf(_p, _margin, _fmt, _args...) \
({	\
	int _n_size = 0;	\
	if (_p->limit >= (_p->cnt + _margin))	\
		_n_size = snprintf(_p->pbuf + _p->cnt, _p->limit - _p->cnt,\
					(const char *)_fmt, ##_args);	\
	_n_size;	\
})

#define siw_mon_prt_snprintf(_p, _fmt, _args...) \
	__siw_mon_prt_snprintf(_p, __siw_mon_prt_margin, _fmt, ##_args)

#define siw_mon_prt_snprintf_finish(_p) \
	__siw_mon_prt_snprintf(_p, __siw_mon_prt_margin_fin, "\n")


#define SIW_MON_TIME_FORMAT			"%5ld.%06ld"
#define SIW_MON_TIME_FORMAT_NOP			"     .      "


#define SIW_MON_IDX_MAIN_FORMAT_4		"[%4d, %d]"
#define SIW_MON_IDX_MAIN_FORMAT_3		"[%3d, %d]"
#define SIW_MON_IDX_MAIN_FORMAT_2		"[%2d, %d]"
#define SIW_MON_IDX_MAIN_FORMAT_1		"[%d, %d]"

#define siw_mon_prt_idx_main_snprintf(_p, _cur, _tot) \
({	\
	int _n_size = 0;	\
	if (_tot > 999) {	\
		_n_size = siw_mon_prt_snprintf(_p,	\
					SIW_MON_IDX_MAIN_FORMAT_4,	\
					_cur, _tot);	\
	} else if (_tot > 99) {	\
		_n_size = siw_mon_prt_snprintf(_p,	\
					SIW_MON_IDX_MAIN_FORMAT_3,	\
					_cur, _tot);	\
	} else if (_tot > 9) {	\
		_n_size = siw_mon_prt_snprintf(_p,	\
					SIW_MON_IDX_MAIN_FORMAT_2,	\
					_cur, _tot);	\
	} else {	\
		_n_size = siw_mon_prt_snprintf(_p,	\
					SIW_MON_IDX_MAIN_FORMAT_1,	\
					_cur, _tot);	\
	}	\
	_n_size;	\
})

#define SIW_MON_IDX_SUB_FORMAT_4		"(%4d, %d)"
#define SIW_MON_IDX_SUB_FORMAT_3		"(%3d, %d)"
#define SIW_MON_IDX_SUB_FORMAT_2		"(%2d, %d)"
#define SIW_MON_IDX_SUB_FORMAT_1		"(%d, %d)"

#define siw_mon_prt_idx_sub_snprintf(_p, _cur, _tot) \
({	\
	int _n_size = 0;	\
	if (_tot > 999) {	\
		_n_size = siw_mon_prt_snprintf(_p,	\
					SIW_MON_IDX_SUB_FORMAT_4,	\
					_cur, _tot);	\
	} else if (_tot > 99) {	\
		_n_size = siw_mon_prt_snprintf(_p,	\
					SIW_MON_IDX_SUB_FORMAT_3,	\
					_cur, _tot);	\
	} else if (_tot > 9) {	\
		_n_size = siw_mon_prt_snprintf(_p,	\
					SIW_MON_IDX_SUB_FORMAT_2,	\
					_cur, _tot);	\
	} else {	\
		_n_size = siw_mon_prt_snprintf(_p,	\
					SIW_MON_IDX_SUB_FORMAT_1,	\
					_cur, _tot);	\
	}	\
	_n_size;	\
})


#define SIW_MON_UEVENT_STR				"@UEVENT"


enum {
	SIW_MON_PRT_MARGIN		= 128,
	SIW_MON_PRT_MARGIN_FIN		= 4,
	SIW_MON_BUF_MAX			= 256,
	SIW_MON_PRT_MAX			= 16,
};

static int __siw_mon_prt_margin = SIW_MON_PRT_MARGIN;
static int __siw_mon_prt_margin_fin = SIW_MON_PRT_MARGIN_FIN;

#if defined(__SIW_MON_EVENT_NO_LIMIT)
/* 0 = limit off, else limit on */
static int __siw_mon_buf_max;
#else	/* __SIW_MON_EVENT_NO_LIMIT */
static int __siw_mon_buf_max = SIW_MON_BUF_MAX;
#endif	/* __SIW_MON_EVENT_NO_LIMIT */

static int __siw_mon_prt_max = SIW_MON_PRT_MAX;

int __used siw_mon_buf_max(void)
{
	return __siw_mon_buf_max;
}

int __used siw_mon_prt_max(void)
{
	return __siw_mon_prt_max;
}

enum {
	NO_OF_DEBUF_FILES = 32,
};

struct siw_mon_debugfs_ctrl {
	struct dentry *root;
	struct dentry *file[NO_OF_DEBUF_FILES];
	unsigned long fbit;
};

static struct siw_mon_debugfs_ctrl *__siw_mon_debugfs_ctrl;

#define siw_mon_prt_init_files(_ctrl) { }


#if defined(__SIW_MON_USE_BUF_SLAB)
static void siw_mon_prt_submit_memmove(struct siw_mon_data *data)
{
	switch (data->type) {
	case SIW_MON_BUS:
	#if defined(__SIW_MON_USE_BUF_SLAB_BUS)
		{
			struct siw_mon_data_bus *bus = &data->d.bus;

			if (bus->tx_buf) {
				bus->tx_buf = siw_mon_get_buf_cache(bus->tx_buf,
					siw_mon_buf_size_cur(bus->tx_size));
			}

			if (bus->rx_buf) {
				bus->rx_buf = siw_mon_get_buf_cache(bus->rx_buf,
					siw_mon_buf_size_cur(bus->rx_size));
			}
		}
	#endif	/* __SIW_MON_USE_BUF_SLAB_BUS */
		break;
	case SIW_MON_EVT:
		break;
	case SIW_MON_OPS:
	#if defined(__SIW_MON_USE_BUF_SLAB_BUS)
		{
			struct siw_mon_data_ops *ops = &data->d.ops;
			unsigned char *buf = (unsigned char *)ops->data[0];

			if (siw_mon_txt_sock_str_cmp(ops->ops) > 0) {
				buf = siw_mon_get_buf_cache(buf,
					siw_mon_buf_size_cur(ops->data[1]));
				ops->data[0] = (size_t)buf;
			}
		}
	#endif
		break;
	default:
		break;
	}
}
#else	/* __SIW_MON_USE_BUF_SLAB */
#define siw_mon_prt_submit_memmove(_data)	{ }
#endif	/* __SIW_MON_USE_BUF_SLAB */

#define SIW_MON_PRT_SUBMIT_TAG	"prt_submit: "

static void siw_mon_prt_submit(void *r_data, struct siw_mon_data *data,
					unsigned long *flags)
{
	struct siw_mon_reader_txt *rp = r_data;
	struct siw_mon_event_txt *etxt;
	ktime_t kt = ktime_get();

	mon_pr_test(SIW_MON_PRT_SUBMIT_TAG "start\n");

#if defined(__SIW_MON_EVENT_NO_LIMIT)
	/*
	 * I'm not sure this is safe way...
	 */
	if (rp->nevents >= SIW_MON_EVENT_MAX) {
		unsigned long p_flags;

		if (flags) {
			p_flags = *flags;
			spin_unlock_irqrestore(&rp->r.mterm->lock, p_flags);
		}
		while (rp->nevents >= SIW_MON_EVENT_MAX) {
			wake_up(&rp->wait);
			usleep_range(1000, 1500);
		}
		if (flags) {
			spin_lock_irqsave(&rp->r.mterm->lock, p_flags);
			*flags = p_flags;
		}
	}
#else	/* __SIW_MON_EVENT_NO_LIMIT */
	if (rp->nevents >= SIW_MON_EVENT_MAX) {
		mon_pr_test(SIW_MON_PRT_SUBMIT_TAG "max events\n");
		rp->r.mterm->cnt_text_lost++;
		return;
	}
#endif	/* __SIW_MON_EVENT_NO_LIMIT */

	if (!atomic_read(&rp->read_running)) {
		mon_pr_test(SIW_MON_PRT_SUBMIT_TAG "read_running off\n");
		rp->r.mterm->cnt_text_lost++;
		return;
	}

	etxt = kmem_cache_zalloc(rp->e_slab, GFP_ATOMIC);
	if (etxt == NULL) {
		mon_pr_err(SIW_MON_PRT_SUBMIT_TAG
				"failed to get memory cache from etxt slab\n");
		rp->r.mterm->cnt_text_lost++;
		return;
	}

	memcpy(&etxt->time, &kt, sizeof(ktime_t));
	etxt->type = data->type;
	siw_mon_prt_submit_memmove(data);
	memcpy(&etxt->data, data, sizeof(struct siw_mon_data));

	rp->nevents++;
	list_add_tail(&etxt->e_link, &rp->e_list);
	wake_up(&rp->wait);

	mon_pr_test(SIW_MON_PRT_SUBMIT_TAG "done\n");
}

/*
 * Slab interface: constructor.
 */
static void siw_mon_txt_ctor(void *mem)
{
	/*
	 * Nothing to initialize. No, really!
	 * So, we fill it with garbage to emulate a reused object.
	 */
	memset(mem, 0xe5, sizeof(struct siw_mon_event_txt));
}

#define SIW_MON_PRT_OPEN_TAG	"prt_open: "

/*
 */
static int siw_mon_prt_open(struct inode *inode, struct file *file)
{
	struct siw_mon_term *mterm;
	struct siw_mon_reader_txt *rp;
	int ret = 0;

	mutex_lock(&__siw_mon_lock);

	if (inode == NULL) {
		mon_pr_err(SIW_MON_PRT_OPEN_TAG
			"NULL inode\n");
		ret = -EINVAL;
		goto out_alloc;
	}

	mterm = inode->i_private;
	if (mterm == NULL) {
		mon_pr_err(SIW_MON_PRT_OPEN_TAG
			"NULL mterm\n");
		ret = -EINVAL;
		goto out_alloc;
	}

	rp = kzalloc(sizeof(struct siw_mon_reader_txt), GFP_KERNEL);
	if (rp == NULL) {
		ret = -ENOMEM;
		goto out_alloc;
	}
	INIT_LIST_HEAD(&rp->e_list);
	init_waitqueue_head(&rp->wait);
	mutex_init(&rp->printf_lock);

	rp->printf_size = SIW_MON_PRINT_SIZE;
	rp->printf_buf = kmalloc(rp->printf_size, GFP_KERNEL);
	if (rp->printf_buf == NULL) {
		ret = -ENOMEM;
		goto out_alloc_pr;
	}

	rp->r.mterm = mterm;
	rp->r.r_data = rp;
	rp->r.submit = siw_mon_prt_submit;

	snprintf(rp->slab_name, SLAB_NAME_SZ, "siwmon_text_%p", rp);
	rp->e_slab = kmem_cache_create(rp->slab_name,
					sizeof(struct siw_mon_event_txt),
					sizeof(long),
					0,
					siw_mon_txt_ctor);
	if (rp->e_slab == NULL) {
		mon_pr_err(SIW_MON_PRT_OPEN_TAG
			"failed to create memory cache for rp slab\n");
		ret = -ENOMEM;
		goto out_slab;
	}

	siw_mon_reader_add(mterm, &rp->r);

	atomic_set(&rp->read_running, 1);

	file->private_data = rp;

	mon_pr_dbg(SIW_MON_PRT_OPEN_TAG
		"open file - mterm[%s]\n", mterm->name);

	mutex_unlock(&__siw_mon_lock);
	return 0;

out_slab:
	kfree(rp->printf_buf);
out_alloc_pr:
	kfree(rp);
out_alloc:
	mutex_unlock(&__siw_mon_lock);
	return ret;
}

/*
 * [31:16] : original total size
 * [15:00] : adjusted total size by print limit
 * See 'siw_mon_submit_bus', 'siw_mon_submit_ops'
 */
static int __siw_mon_txt_read_get_len(int size, int *total)
{
	int tot;
	int len;

	/* original total size */
	len = siw_mon_buf_size_tot(size);
	tot = (len) ? len : size;

	if (total)
		*total = tot;

	/* adjusted total size by print limit */
	return siw_mon_buf_size_cur(size);
}

static int __siw_mon_txt_read_priv_idx(struct siw_mon_txt_ptr *p,
			int priv, int total)
{
	int priv_main;
	int priv_sub;
	int cur, tot;
	int last = 0;

	if (!priv)
		goto out;

	priv_main = siw_mon_priv_main(priv);
	priv_sub = siw_mon_priv_sub(priv);

	if (priv_main) {
		cur = siw_mon_priv_cur(priv_main);
		tot = siw_mon_priv_tot(priv_main);

		p->cnt += siw_mon_prt_idx_main_snprintf(p, cur, tot);
	}

	if (priv_sub) {
		cur = siw_mon_priv_cur(priv_sub);
		tot = siw_mon_priv_tot(priv_sub);

		p->cnt += siw_mon_prt_idx_sub_snprintf(p, cur, tot);

		if (total && ((cur + 1) == tot))
			if ((tot * siw_mon_prt_max()) < total)
				last = 1;
	}

out:
	return last;
}

static int __siw_mon_txt_read_prt_buf(struct siw_mon_txt_ptr *p,
			unsigned char *buf, int size,
			char *dir, int total)
{
	int i;
	int prt = 0;

	if (!buf)
		goto out;

	if (!size)
		goto out;

	if (dir)
		p->cnt += siw_mon_prt_snprintf(p, " %s(%3d)", dir, total);

	for (i = 0; i < size; i++)
		p->cnt += siw_mon_prt_snprintf(p, " %02X", buf[i]);

	prt = 1;

out:
	return prt;
}

static void __siw_mon_txt_read_bus(struct siw_mon_reader_txt *rp,
		struct siw_mon_txt_ptr *p, struct siw_mon_event_txt *etxt)
{
	struct siw_mon_data_bus *bus = &etxt->data.d.bus;
	int total;
	int len;
	int last = 0;
	int prt = 0;

	p->cnt += siw_mon_prt_snprintf(p, "%s", bus->dir);

	if (bus->tx_buf) {
		len = __siw_mon_txt_read_get_len(bus->tx_size, &total);
		last += __siw_mon_txt_read_priv_idx(p, bus->priv, total);
		prt += __siw_mon_txt_read_prt_buf(p, bus->tx_buf, len, "T",
						total);
		siw_mon_put_buf_cache(bus->tx_buf);
	}

	if (bus->rx_buf) {
		len = __siw_mon_txt_read_get_len(bus->rx_size, &total);
		last += __siw_mon_txt_read_priv_idx(p, bus->priv, total);
		prt += __siw_mon_txt_read_prt_buf(p, bus->rx_buf, len, "R",
						total);
		siw_mon_put_buf_cache(bus->rx_buf);
	}

	if (prt && last)
		p->cnt += siw_mon_prt_snprintf(p, " ...");
}

static void __siw_mon_txt_read_evt(struct siw_mon_reader_txt *rp,
		struct siw_mon_txt_ptr *p, struct siw_mon_event_txt *etxt)

{
	struct siw_mon_data_evt *evt = &etxt->data.d.evt;
	char *type = evt->type;

	if (!strncmp(SIW_MON_UEVENT_STR, evt->type,
		sizeof(SIW_MON_UEVENT_STR)))
		type = &evt->type[1];

	p->cnt += siw_mon_prt_snprintf(p, "%s(%02X) %20s(%02X) %d",
					type, evt->type_v,
					evt->code, evt->code_v,
					evt->value);
}

static int __siw_mon_txt_read_ops_sock(struct siw_mon_reader_txt *rp,
		struct siw_mon_txt_ptr *p, struct siw_mon_event_txt *etxt)
{
	struct siw_mon_data_ops *ops = &etxt->data.d.ops;
	char *title;
	unsigned char *buf;
	int size, total, len;
	int last = 0;
	int idx = 0;
	int prt = 0;

	idx = siw_mon_txt_sock_str_cmp(ops->ops);
	if (idx < 0)
		return idx;

	title = (idx == SIW_MON_SOCK_S) ? "SOCK_S" : "SOCK_R";
	p->cnt += siw_mon_prt_snprintf(p, "%s", title);

	buf = (unsigned char *)ops->data[0];
	size = ops->data[1];

	len = __siw_mon_txt_read_get_len(size, &total);
	last += __siw_mon_txt_read_priv_idx(p, ops->priv, total);
	prt += __siw_mon_txt_read_prt_buf(p, buf, len, NULL, 0);

	if (prt && last)
		p->cnt += siw_mon_prt_snprintf(p, " ...");

	siw_mon_put_buf_cache(buf);

	return 0;
}

static void __siw_mon_txt_read_ops(struct siw_mon_reader_txt *rp,
		struct siw_mon_txt_ptr *p, struct siw_mon_event_txt *etxt)
{
	struct siw_mon_data_ops *ops = &etxt->data.d.ops;
	int len = min(siw_mon_prt_max(), ops->len);
	int i;

	if (!__siw_mon_txt_read_ops_sock(rp, p, etxt))
		return;

	p->cnt += siw_mon_prt_snprintf(p, "%s ", ops->ops);

	if (!len)
		return;

	for (i = 0; i < len; i++)
		p->cnt += siw_mon_prt_snprintf(p, " %08zX",
						(size_t)ops->data[i]);
}

static void __siw_mon_txt_read_x(struct siw_mon_reader_txt *rp,
		struct siw_mon_txt_ptr *p, struct siw_mon_event_txt *etxt)
{
	p->cnt += siw_mon_prt_snprintf(p, "(can't do analysis about this)");
}

static void siw_mon_txt_read_data(struct siw_mon_reader_txt *rp,
		struct siw_mon_txt_ptr *p, struct siw_mon_event_txt *etxt)
{
	switch (etxt->type) {
	case SIW_MON_BUS:
		__siw_mon_txt_read_bus(rp, p, etxt);
		break;
	case SIW_MON_EVT:
		__siw_mon_txt_read_evt(rp, p, etxt);
		break;
	case SIW_MON_OPS:
		__siw_mon_txt_read_ops(rp, p, etxt);
		break;
	default:
		__siw_mon_txt_read_x(rp, p, etxt);
		break;
	}

	p->cnt += siw_mon_prt_snprintf_finish(p);
}

static void siw_mon_txt_read_head(struct siw_mon_reader_txt *rp,
		struct siw_mon_txt_ptr *p, const struct siw_mon_event_txt *etxt)
{
	struct device *dev = etxt->data.dev;
	struct timeval tval;
	char *type_str;

	switch (etxt->type) {
	case SIW_MON_BUS:
		type_str = "B";
		break;
	case SIW_MON_EVT:
		type_str = "E";
		break;
	case SIW_MON_OPS:
		type_str = "O";
		break;
	default:
		type_str = "x";
		break;
	}

	if (etxt->data.inc_cnt) {
		tval = ktime_to_timeval(etxt->time);

		p->cnt += siw_mon_prt_snprintf(p,
			"[" SIW_MON_TIME_FORMAT "] %s[%8s](%08X)(%4d) ",
				tval.tv_sec, tval.tv_usec,
				type_str, dev_name(dev),
				etxt->data.cnt_events,
				etxt->data.ret);
	} else {
		p->cnt += siw_mon_prt_snprintf(p,
			"[" SIW_MON_TIME_FORMAT_NOP "] %s[%8s](%08X)(%4d) ",
				type_str, dev_name(dev),
				etxt->data.cnt_events,
				etxt->data.ret);
	}
}

/*
 * Fetch next event from the circular buffer.
 */
static struct siw_mon_event_txt *siw_mon_prt_fetch(
					struct siw_mon_reader_txt *rp,
				    struct siw_mon_term *mterm)
{
	struct list_head *p;
	unsigned long flags;

	spin_lock_irqsave(&mterm->lock, flags);
	if (list_empty(&rp->e_list)) {
		spin_unlock_irqrestore(&mterm->lock, flags);
		return NULL;
	}
	p = rp->e_list.next;
	list_del(p);
	--rp->nevents;
	spin_unlock_irqrestore(&mterm->lock, flags);
	return list_entry(p, struct siw_mon_event_txt, e_link);
}


static struct siw_mon_event_txt *siw_mon_prt_read_wait(
				struct siw_mon_reader_txt *rp,
			    struct file *file)
{
	struct siw_mon_term *mterm = rp->r.mterm;
	DECLARE_WAITQUEUE(waita, current);
	struct siw_mon_event_txt *etxt;

	add_wait_queue(&rp->wait, &waita);
	set_current_state(TASK_INTERRUPTIBLE);
	while ((etxt = siw_mon_prt_fetch(rp, mterm)) == NULL) {
		if (file->f_flags & O_NONBLOCK) {
			set_current_state(TASK_RUNNING);
			remove_wait_queue(&rp->wait, &waita);
			return ERR_PTR(-EWOULDBLOCK);
		}
		/*
		 * We do not count nwaiters, because ->release is supposed
		 * to be called when all openers are gone only.
		 */
		schedule();
		if (signal_pending(current)) {
			remove_wait_queue(&rp->wait, &waita);
			return ERR_PTR(-EINTR);
		}
		set_current_state(TASK_INTERRUPTIBLE);
	}
	set_current_state(TASK_RUNNING);
	remove_wait_queue(&rp->wait, &waita);
	return etxt;
}

#define SIW_MON_PRT_READ_TAG	"prt_read: "

/*
 * For simplicity, we read one record in one system call and throw out
 * what does not fit. This means that the following does not work:
 *   dd if=/dbg/usbmon/0t bs=10
 * Also, we do not allow seeks and do not bother advancing the offset.
 */
static ssize_t siw_mon_prt_read(struct file *file, char __user *buf,
				size_t nbytes, loff_t *ppos)
{
	struct siw_mon_reader_txt *rp = file->private_data;
	struct siw_mon_event_txt *etxt;
	struct siw_mon_txt_ptr ptr;

	mon_pr_test(SIW_MON_PRT_READ_TAG "start\n");

	if (!rp) {
		mon_pr_err(SIW_MON_PRT_READ_TAG "NULL rp\n");
		return -EINVAL;
	}

	etxt = siw_mon_prt_read_wait(rp, file);
	if (IS_ERR(etxt)) {
		mon_pr_test(SIW_MON_PRT_READ_TAG
				"read_wait stop, %d\n", (int)PTR_ERR(etxt));
		atomic_set(&rp->read_running, 0);
		return PTR_ERR(etxt);
	}

	mutex_lock(&rp->printf_lock);
	ptr.cnt = 0;
	ptr.pbuf = rp->printf_buf;
	ptr.limit = rp->printf_size;

	siw_mon_txt_read_head(rp, &ptr, etxt);
	siw_mon_txt_read_data(rp, &ptr, etxt);

	if (copy_to_user(buf, rp->printf_buf, ptr.cnt))
		ptr.cnt = -EFAULT;

	mutex_unlock(&rp->printf_lock);
	kmem_cache_free(rp->e_slab, etxt);

	mon_pr_test(SIW_MON_PRT_READ_TAG "done, %d\n", ptr.cnt);

	return ptr.cnt;
}

#define SIW_MON_PRT_RELEASE_TAG	"prt_release: "

static int siw_mon_prt_release(struct inode *inode, struct file *file)
{
	struct siw_mon_reader_txt *rp = file->private_data;
	struct siw_mon_term *mterm;
	/* unsigned long flags; */
	struct list_head *p;
	struct siw_mon_event_txt *etxt;
	int ret = 0;

	mutex_lock(&__siw_mon_lock);

	if (!rp) {
		mon_pr_err(SIW_MON_PRT_RELEASE_TAG
			"NULL rp\n");
		ret = -EINVAL;
		goto out;
	}

	atomic_set(&rp->read_running, 0);

	if (inode == NULL) {
		mon_pr_err(SIW_MON_PRT_RELEASE_TAG
			"NULL inode\n");
		ret = -EINVAL;
		goto out;
	}

	mterm = inode->i_private;
	if (mterm == NULL) {
		mon_pr_err(SIW_MON_PRT_RELEASE_TAG
			"NULL mterm\n");
		ret = -EINVAL;
		goto out;
	}

	if (mterm->nreaders <= 0) {
		mon_pr_err(SIW_MON_PRT_RELEASE_TAG
			"consistency error on close\n");
		goto out;
	}
	siw_mon_reader_del(mterm, &rp->r);

	/*
	 * In theory, e_list is protected by mbus->lock. However,
	 * after mon_reader_del has finished, the following is the case:
	 *  - we are not on reader list anymore, so new events won't be added;
	 *  - whole mbus may be dropped if it was orphaned.
	 * So, we better not touch mbus.
	 */
	/* spin_lock_irqsave(&mterm->lock, flags); */
	while (!list_empty(&rp->e_list)) {
		p = rp->e_list.next;
		etxt = list_entry(p, struct siw_mon_event_txt, e_link);
		list_del(p);
		--rp->nevents;
		kmem_cache_free(rp->e_slab, etxt);
	}
	/* spin_unlock_irqrestore(&mterm->lock, flags); */

	kmem_cache_destroy(rp->e_slab);
	kfree(rp->printf_buf);
	kfree(rp);

	file->private_data = NULL;

	mon_pr_dbg(SIW_MON_PRT_RELEASE_TAG
		"release file - mterm[%s]\n", mterm->name);

	mon_pr_test(SIW_MON_PRT_RELEASE_TAG
		"release file - mterm[%s]\n", mterm->name);

out:
	mutex_unlock(&__siw_mon_lock);
	return ret;
}

static const struct file_operations mon_fops_prt = {
	.owner		= THIS_MODULE,
	.open		= siw_mon_prt_open,
	.llseek		= no_llseek,
	.read		= siw_mon_prt_read,
	.release	= siw_mon_prt_release,
};

#define SIW_MON_PRT_ADD_TAG		"prt_add: "

int siw_mon_prt_add(struct siw_mon_term *mterm, char *name)
{
	struct dentry *mroot;
	struct dentry *mfile;
	enum { NAMESZ = 10 };
	char fname[NAMESZ];
	int ret = 0;

	if (__siw_mon_debugfs_ctrl == NULL) {
		mon_pr_warn(SIW_MON_PRT_ADD_TAG
			"unable to create file : NULL ctrl\n");
		return 0;
	}
	mroot = __siw_mon_debugfs_ctrl->root;

	if (mroot == NULL) {
		mon_pr_warn(SIW_MON_PRT_ADD_TAG
			"unable to create file : NULL root\n");
		return 0;
	}
	if (mterm == NULL) {
		mon_pr_warn(SIW_MON_PRT_ADD_TAG
			"unable to create file : NULL mterm\n");
		return 0;
	}

	ret = snprintf(fname, NAMESZ, "%s", name);
	if (ret <= 0 || ret >= NAMESZ) {
		mon_pr_err(SIW_MON_PRT_ADD_TAG
			"failed to arrange file name[%s]\n", name);
		goto out;
	}

	mfile = debugfs_create_file(fname, 0444,
						mroot, mterm,
						&mon_fops_prt);
	if (mfile == NULL) {
		mon_pr_err(SIW_MON_PRT_ADD_TAG
			"failed to create SiW debug file : %s\n", fname);
		goto out;
	}

	mterm->mfile = mfile;

	return 1;

out:
	return 0;
}

void siw_mon_prt_del(struct siw_mon_term *mterm)
{
	if (mterm->mfile != NULL) {
		debugfs_remove(mterm->mfile);
		mterm->mfile = NULL;
	}
}

int siw_mon_prt_init(void)
{
	struct siw_mon_debugfs_ctrl *ctrl;
	struct dentry *mroot = NULL;
	char *name = SIW_MON_NAME;

	ctrl = kzalloc(sizeof(struct siw_mon_debugfs_ctrl), GFP_KERNEL);
	if (!ctrl)
		return -ENOMEM;

	mroot = debugfs_create_dir((const char *)name, NULL);
	if (!mroot)
		return -ENOMEM;

	mon_pr_dbg("siw mon dir created : %s\n", mroot->d_iname);

	ctrl->root = mroot;

	__siw_mon_debugfs_ctrl = ctrl;

	siw_mon_prt_init_files(ctrl);

	return 0;
}

void siw_mon_prt_exit(void)
{
	struct siw_mon_debugfs_ctrl *ctrl = __siw_mon_debugfs_ctrl;
	struct dentry *mroot;

	if (!ctrl)
		return;

	mroot = ctrl->root;
	if (!mroot)
		return;

	mon_pr_dbg("siw mon dir removed : %s\n", mroot->d_iname);

	debugfs_remove_recursive(mroot);

	kfree(ctrl);
	__siw_mon_debugfs_ctrl = NULL;
}

