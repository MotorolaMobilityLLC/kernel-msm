#ifndef KCT_H_
#  define KCT_H_

#  include <linux/netlink.h>

/*
 * warning: structures and constants in this header must match the
 * ones in libc/kernel/common/linux/kct.h, so that information can
 * be exchange between kernel and userspace throught netlink socket.
 */
/* flags to optionally filter events on android property activation */
#define	EV_FLAGS_PRIORITY_LOW	(1<<0)

#  ifndef MAX_SB_N
#    define MAX_SB_N 32
#  endif

#  ifndef MAX_EV_N
#    define MAX_EV_N 32
#  endif

#  define NETLINK_CRASHTOOL 27
#  define ATTCHMT_ALIGN 4U

/* Type of events supported by crashtool */
enum ct_ev_type {
	CT_EV_STAT,
	CT_EV_INFO,
	CT_EV_ERROR,
	CT_EV_CRASH,
	CT_EV_LAST
};

enum ct_attchmt_type {
	CT_ATTCHMT_DATA0,
	CT_ATTCHMT_DATA1,
	CT_ATTCHMT_DATA2,
	CT_ATTCHMT_DATA3,
	CT_ATTCHMT_DATA4,
	CT_ATTCHMT_DATA5,
	/* Always add new types after DATA5 */
	CT_ATTCHMT_BINARY,
	CT_ATTCHMT_FILELIST
};

struct ct_attchmt {
	__u32 size; /* sizeof(data) */
	enum ct_attchmt_type type;
	char data[];
} __aligned(4);

struct ct_event {
	__u64 timestamp;
	char submitter_name[MAX_SB_N];
	char ev_name[MAX_EV_N];
	enum ct_ev_type type;
	__u32 attchmt_size; /* sizeof(all_attachments inc. padding) */
	__u32 flags;
	struct ct_attchmt attachments[];
} __aligned(4);

enum kct_nlmsg_type {
	/* kernel -> userland */
	KCT_EVENT,
	/* userland -> kernel */
	KCT_SET_PID = 4200,
};

struct kct_packet {
	struct nlmsghdr nlh;
	struct ct_event event;
};

#  define ATTCHMT_ALIGNMENT	4

#  ifndef KCT_ALIGN
#    define __KCT_ALIGN_MASK(x, mask)    (((x) + (mask)) & ~(mask))
#    define __KCT_ALIGN(x, a)            __KCT_ALIGN_MASK(x, (typeof(x))(a) - 1)
#    define KCT_ALIGN(x, a)		     __KCT_ALIGN((x), (a))
#  endif /* !KCT_ALIGN */

#  define foreach_attchmt(Event, Attchmt)				\
	if ((Event)->attchmt_size)					\
		for ((Attchmt) = (Event)->attachments;			\
		     (Attchmt) < (typeof(Attchmt))(((char *)		\
				  (Event)->attachments) +               \
			(Event)->attchmt_size);                         \
	(Attchmt) = (typeof(Attchmt))KCT_ALIGN(((size_t)(Attchmt)) \
						     + sizeof(*(Attchmt)) + \
			      (Attchmt)->size, ATTCHMT_ALIGNMENT))

/*
 * User should use the macros below rather than those extern functions
 * directly. Laters' declaration are only to set them __weak so
 * that the macros works fine.
 */
/* Raw API (deprecated) */
extern struct ct_event *kct_alloc_event(const char *submitter_name,
					const char *ev_name,
					enum ct_ev_type ev_type,
					gfp_t flags, uint eflags) __weak;
extern int kct_add_attchmt(struct ct_event **ev,
			   enum ct_attchmt_type at_type,
			   unsigned int size,
			   char *data, gfp_t flags)  __weak;
extern void kct_free_event(struct ct_event *ev) __weak;
extern int kct_log_event(struct ct_event *ev, gfp_t flags) __weak;

/* API */
#define MKFN(fn, ...) MKFN_N(fn, ##__VA_ARGS__, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)(__VA_ARGS__)
#define MKFN_N(fn, n0, n1, n2, n3, n4, n5, n6, n7, n8, n9, n10, n11, n, ...) fn##n
#define kct_log(...) MKFN(__kct_log_, ##__VA_ARGS__)

#define __kct_log_4(Type, Submitter_name, Ev_name, flags) \
	do {  if (kct_alloc_event) {	\
		struct ct_event *__ev =	\
			kct_alloc_event(Submitter_name, Ev_name, Type, \
				GFP_ATOMIC, flags); \
		if (__ev) { \
			kct_log_event(__ev, GFP_ATOMIC); \
		} \
	} } while (0)

#define __kct_log_5(Type, Submitter_name, Ev_name, flags, Data0) \
	do {  if (kct_alloc_event) {	\
		struct ct_event *__ev =	\
			kct_alloc_event(Submitter_name, Ev_name, Type, \
				GFP_ATOMIC, flags); \
		if (__ev) { \
			if (Data0) \
				kct_add_attchmt(&__ev, CT_ATTCHMT_DATA0, \
					strlen(Data0) + 1, Data0, GFP_ATOMIC); \
			kct_log_event(__ev, GFP_ATOMIC); \
		} \
	} } while (0)

#define __kct_log_6(Type, Submitter_name, Ev_name, flags, Data0, Data1) \
	do {  if (kct_alloc_event) {	\
		struct ct_event *__ev =	\
			kct_alloc_event(Submitter_name, Ev_name, Type, \
				GFP_ATOMIC, flags); \
		if (__ev) { \
			if (Data0) \
				kct_add_attchmt(&__ev, CT_ATTCHMT_DATA0, \
					strlen(Data0) + 1, Data0, GFP_ATOMIC); \
			if (Data1) \
				kct_add_attchmt(&__ev, CT_ATTCHMT_DATA1, \
					strlen(Data1) + 1, Data1, GFP_ATOMIC); \
			kct_log_event(__ev, GFP_ATOMIC); \
		} \
	} } while (0)

#define __kct_log_7(Type, Submitter_name, Ev_name, flags, Data0, Data1, Data2) \
	do {  if (kct_alloc_event) {	\
		struct ct_event *__ev =	\
			kct_alloc_event(Submitter_name, Ev_name, Type, \
				GFP_ATOMIC, flags); \
		if (__ev) { \
			if (Data0) \
				kct_add_attchmt(&__ev, CT_ATTCHMT_DATA0, \
					strlen(Data0) + 1, Data0, GFP_ATOMIC); \
			if (Data1) \
				kct_add_attchmt(&__ev, CT_ATTCHMT_DATA1, \
					strlen(Data1) + 1, Data1, GFP_ATOMIC); \
			if (Data2) \
				kct_add_attchmt(&__ev, CT_ATTCHMT_DATA2, \
					strlen(Data2) + 1, Data2, GFP_ATOMIC); \
			kct_log_event(__ev, GFP_ATOMIC); \
		} \
	} } while (0)

#define __kct_log_8(Type, Submitter_name, Ev_name, flags, Data0, Data1, Data2, \
					Data3) \
	do {  if (kct_alloc_event) {	\
		struct ct_event *__ev =	\
			kct_alloc_event(Submitter_name, Ev_name, Type, \
				GFP_ATOMIC, flags); \
		if (__ev) { \
			if (Data0) \
				kct_add_attchmt(&__ev, CT_ATTCHMT_DATA0, \
					strlen(Data0) + 1, Data0, GFP_ATOMIC); \
			if (Data1) \
				kct_add_attchmt(&__ev, CT_ATTCHMT_DATA1, \
					strlen(Data1) + 1, Data1, GFP_ATOMIC); \
			if (Data2) \
				kct_add_attchmt(&__ev, CT_ATTCHMT_DATA2, \
					strlen(Data2) + 1, Data2, GFP_ATOMIC); \
			if (Data3) \
				kct_add_attchmt(&__ev, CT_ATTCHMT_DATA3, \
					strlen(Data3) + 1, Data3, GFP_ATOMIC); \
			kct_log_event(__ev, GFP_ATOMIC); \
		} \
	} } while (0)

	#define __kct_log_9(Type, Submitter_name, Ev_name, flags, Data0, Data1, Data2, \
					 Data3, Data4) \
	do {  if (kct_alloc_event) {	\
		struct ct_event *__ev =	\
			kct_alloc_event(Submitter_name, Ev_name, Type, \
				GFP_ATOMIC, flags); \
		if (__ev) { \
			if (Data0) \
				kct_add_attchmt(&__ev, CT_ATTCHMT_DATA0, \
					strlen(Data0) + 1, Data0, GFP_ATOMIC); \
			if (Data1) \
				kct_add_attchmt(&__ev, CT_ATTCHMT_DATA1, \
					strlen(Data1) + 1, Data1, GFP_ATOMIC); \
			if (Data2) \
				kct_add_attchmt(&__ev, CT_ATTCHMT_DATA2, \
					strlen(Data2) + 1, Data2, GFP_ATOMIC); \
			if (Data3) \
				kct_add_attchmt(&__ev, CT_ATTCHMT_DATA3, \
					strlen(Data3) + 1, Data3, GFP_ATOMIC); \
			if (Data4) \
				kct_add_attchmt(&__ev, CT_ATTCHMT_DATA4, \
					strlen(Data4) + 1, Data4, GFP_ATOMIC); \
			kct_log_event(__ev, GFP_ATOMIC); \
		} \
	} } while (0)

	#define __kct_log_10(Type, Submitter_name, Ev_name, flags, Data0, Data1, Data2, \
					 Data3, Data4, Data5) \
	do {  if (kct_alloc_event) {	\
		struct ct_event *__ev =	\
			kct_alloc_event(Submitter_name, Ev_name, Type, \
				GFP_ATOMIC, flags); \
		if (__ev) { \
			if (Data0) \
				kct_add_attchmt(&__ev, CT_ATTCHMT_DATA0, \
					strlen(Data0) + 1, Data0, GFP_ATOMIC); \
			if (Data1) \
				kct_add_attchmt(&__ev, CT_ATTCHMT_DATA1, \
					strlen(Data1) + 1, Data1, GFP_ATOMIC); \
			if (Data2) \
				kct_add_attchmt(&__ev, CT_ATTCHMT_DATA2, \
					strlen(Data2) + 1, Data2, GFP_ATOMIC); \
			if (Data3) \
				kct_add_attchmt(&__ev, CT_ATTCHMT_DATA3, \
					strlen(Data3) + 1, Data3, GFP_ATOMIC); \
			if (Data4) \
				kct_add_attchmt(&__ev, CT_ATTCHMT_DATA4, \
					strlen(Data4) + 1, Data4, GFP_ATOMIC); \
			if (Data5) \
				kct_add_attchmt(&__ev, CT_ATTCHMT_DATA5, \
					strlen(Data5) + 1, Data5, GFP_ATOMIC); \
			kct_log_event(__ev, GFP_ATOMIC); \
		} \
	} } while (0)

	#define __kct_log_11(Type, Submitter_name, Ev_name, flags, Data0, Data1, Data2, \
					 Data3, Data4, Data5, filelist) \
	do {  if (kct_alloc_event) {	\
		struct ct_event *__ev =	\
			kct_alloc_event(Submitter_name, Ev_name, Type, \
				GFP_ATOMIC, flags); \
		if (__ev) { \
			if (Data0) \
			kct_add_attchmt(&__ev, CT_ATTCHMT_DATA0, \
					strlen(Data0) + 1, Data0, GFP_ATOMIC); \
			if (Data1) \
			kct_add_attchmt(&__ev, CT_ATTCHMT_DATA1, \
					strlen(Data1) + 1, Data1, GFP_ATOMIC); \
			if (Data2) \
			kct_add_attchmt(&__ev, CT_ATTCHMT_DATA2, \
					strlen(Data2) + 1, Data2, GFP_ATOMIC); \
			if (Data3) \
			kct_add_attchmt(&__ev, CT_ATTCHMT_DATA3, \
					strlen(Data3) + 1, Data3, GFP_ATOMIC); \
			if (Data4) \
			kct_add_attchmt(&__ev, CT_ATTCHMT_DATA4, \
					strlen(Data4) + 1, Data4, GFP_ATOMIC); \
			if (Data5) \
			kct_add_attchmt(&__ev, CT_ATTCHMT_DATA5, \
					strlen(Data5) + 1, Data5, GFP_ATOMIC); \
			if (filelist) \
			kct_add_attchmt(&__ev, CT_ATTCHMT_FILELIST, \
					strlen(filelist) + 1, filelist, GFP_ATOMIC); \
			kct_log_event(__ev, GFP_ATOMIC); \
		} \
	} } while (0)

#endif /* !KCT_H_ */
