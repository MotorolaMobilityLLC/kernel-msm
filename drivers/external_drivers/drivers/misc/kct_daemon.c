#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netlink.h>
#include <linux/kthread.h>
#include <linux/version.h>

#include <net/sock.h>
#include <net/netlink.h>

#include <linux/kct.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 7, 1))
#define PORTID(skb) (NETLINK_CB(skb).pid)
#else
#define PORTID(skb) (NETLINK_CB(skb).portid)
#endif

/**
 * FIFO of events
 *
 * The kct_daemon pops() events from this queue and sends them to the ct_agent
 * through the Netlink socket ct_nl_sk. We push in this queue each time we
 * trace an event and we cannot directly send it at that time (if in atomic
 * context for example).
 */
static struct sk_buff_head kct_skb_queue;

/** The Netlink socket */
static struct sock *kct_nl_sk;

/** The kct_daemon kthread */
static struct task_struct *kctd;

/** Waitqueue used to wake kct_daemon */
static DECLARE_WAIT_QUEUE_HEAD(kct_wq);

/* monitor port ID, used to identify the netlink socket to send events to */
int monitor_pid;

/** Sequence ID of packet sent to userland */
atomic_t kct_seq = ATOMIC_INIT(0);

/** Protocol number for netlink socket */
static int kctunit = NETLINK_CRASHTOOL;
module_param(kctunit, int, 0);

/* keep ONE return to ensure inline */
inline struct ct_event *kct_alloc_event(const char *submitter_name,
					const char *ev_name,
					enum ct_ev_type ev_type,
					gfp_t flags,
					uint ev_flags)
{
	struct ct_event *ev = NULL;
	struct timespec t;

	if (submitter_name && ev_name) {
		ev = kzalloc(sizeof(*ev), flags);
		if (ev) {
			strlcpy(ev->submitter_name,
				submitter_name,
				sizeof(ev->submitter_name));
			strlcpy(ev->ev_name, ev_name, sizeof(ev->ev_name));

			getnstimeofday(&t);
			ev->timestamp = (time_t)t.tv_sec;
			ev->type = ev_type;
			ev->flags = ev_flags;
		}
	}

	return ev;
}
EXPORT_SYMBOL(kct_alloc_event);

inline int kct_add_attchmt(struct ct_event **ev,
			   enum ct_attchmt_type at_type,
			   unsigned int size,
			   char *data, gfp_t flags)
{
	struct ct_attchmt *new_attchmt = NULL;
	struct ct_event *new_ev = NULL;
	u32 new_size = sizeof(*new_ev) + (*ev)->attchmt_size +
		ALIGN(size + sizeof(*new_attchmt), ATTCHMT_ALIGNMENT);

	pr_debug("%s: size %u\n", __func__, new_size);

	new_ev = krealloc(*ev, new_size, flags);
	if (!new_ev) {
		pr_warn("%s: krealloc() failed.\n", __func__);
		return -ENOMEM;
	}

	new_attchmt = (struct ct_attchmt *)
		(((char *) new_ev->attachments) + new_ev->attchmt_size);

	WARN_ON(!IS_ALIGNED((size_t)new_attchmt, 4));

	new_attchmt->size = size;
	new_attchmt->type = at_type;
	memcpy(new_attchmt->data, data, size);

	new_ev->attchmt_size = new_size - sizeof(*new_ev);

	*ev = new_ev;

	return 0;
}
EXPORT_SYMBOL(kct_add_attchmt);

void kct_free_event(struct ct_event *ev)
{
	kfree(ev);
}
EXPORT_SYMBOL(kct_free_event);

int kct_log_event(struct ct_event *ev, gfp_t flags)
{
	struct nlmsghdr *nlh = NULL;
	struct sk_buff *skb = NULL;
	u32 seq;

	skb = nlmsg_new(sizeof(*ev) + ev->attchmt_size, flags);
	if (!skb)
		return -ENOMEM;

	seq = atomic_inc_return(&kct_seq);
	/** TODO: atomic monitor_pid or spinlock */
	nlh = nlmsg_put(skb, monitor_pid, seq, KCT_EVENT,
			sizeof(*ev) + ev->attchmt_size, 0);
	if (nlh == NULL) {
		nlmsg_free(skb);
		return -EMSGSIZE;
	}

	memcpy(nlmsg_data(nlh), ev, sizeof(*ev) + ev->attchmt_size);

	kct_free_event(ev);

	skb_queue_tail(&kct_skb_queue, skb);
	wake_up(&kct_wq);

	return 0;
}
EXPORT_SYMBOL(kct_log_event);

/**
 * Daemon responsible to empty the FIFO of events inside the Netlink
 */
static int kct_daemon(void *unused)
{
	struct sk_buff *skb = NULL;

	pr_debug("%s: started!\n", __func__);

	while (!kthread_should_stop()) {

		pr_debug("%s: loop.\n", __func__);

		if (skb_queue_len(&kct_skb_queue) && monitor_pid) {
			skb = skb_dequeue(&kct_skb_queue);
			if (skb) {
				/* pid might not have been set in kct_log_event;
				 * ensure it's ok now
				 **/
				PORTID(skb) = monitor_pid;
				netlink_unicast(kct_nl_sk, skb, monitor_pid, 1);
			}
		} else {
			wait_event_interruptible(kct_wq,
						 (skb_queue_len(&kct_skb_queue)
						  && monitor_pid) ||
						 kthread_should_stop());
		}
	}

	pr_debug("%s: daemon terminated.\n", __func__);

	skb_queue_purge(&kct_skb_queue);

	return 0;
}

int kct_receive_msg(struct sk_buff *skb, struct nlmsghdr *nlh)
{
	if (nlh->nlmsg_type != KCT_SET_PID) {
		pr_warn("%s: Wrong command received.\n", __func__);
		return -EINVAL;
	}

	pr_debug("%s: KCT_SET_PID received: %d\n", __func__, PORTID(skb));
	monitor_pid = PORTID(skb);

	wake_up(&kct_wq);

	return 0;
}

/**
 * Netlink handler, called when a user space process writes into the netlink.
 *
 * Note:
 * - The socket buffer skb given as argument might contain multiple Netlink
 *   packet. We ack each of them.
 */
static void kct_receive_skb(struct sk_buff *skb)
{
	struct nlmsghdr *nlh = NULL;
	int len = 0;
	int err = 0;

	pr_debug("%s: message received on the socket.\n", __func__);

	nlh = nlmsg_hdr(skb);
	len = skb->len;

	while (nlmsg_ok(nlh, len)) {
		err = kct_receive_msg(skb, nlh);
		 if (err || (nlh->nlmsg_flags & NLM_F_ACK))
			netlink_ack(skb, nlh, err);

		nlh = nlmsg_next(nlh, &len);
	}
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0))
#define CREATENL(init, unit, rcv)					\
	(netlink_kernel_create(&init, unit,				\
			       &((struct netlink_kernel_cfg){ .input = rcv })))
#else
#define CREATENL(init, unit, rcv)					\
	(netlink_kernel_create(&init, unit, 0, &rcv, NULL, THIS_MODULE))
#endif

/**
 * Initializes the module by:
 * - creating a netlink socket with our protocol number (NETLINK_CRASHTOOL)
 * - initializing the FIFO of events to pass on to userspace
 * - starting the kthread that will consume those events from the internal FIFO
 *    to the netlink.
 */
static int __init kct_init(void)
{
	kct_nl_sk = CREATENL(init_net, kctunit, kct_receive_skb);

	if (!kct_nl_sk) {
		pr_err("%s: Can't create netlink socket.\n", __func__);
		return -1;
	}

	skb_queue_head_init(&kct_skb_queue);

	kctd = kthread_run(kct_daemon, NULL, "kct_daemon");

	return 0;
}

static void __exit kct_exit(void)
{
	netlink_kernel_release(kct_nl_sk);

	kthread_stop(kctd);

	/* Wake up kct_daemon kthread, so it can terminate */
	wake_up(&kct_wq);
}

fs_initcall(kct_init);
module_exit(kct_exit);

MODULE_LICENSE("GPL");
