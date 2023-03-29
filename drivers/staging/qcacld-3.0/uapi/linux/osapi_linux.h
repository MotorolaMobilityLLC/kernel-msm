/*
 * Copyright (c) 2013-2018, 2021 The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* ------------------------------------------------------------------------------ */
/* This file contains the definitions of the basic atheros data types. */
/* It is used to map the data types in atheros files to a platform specific */
/* type. */
/* ------------------------------------------------------------------------------ */

#ifndef _OSAPI_LINUX_H_
#define _OSAPI_LINUX_H_

#ifdef __KERNEL__

#include <linux/version.h>
#include <generated/autoconf.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/jiffies.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/semaphore.h>

#include <linux/cache.h>
/* #include <linux/kthread.h> */
#include "a_types.h"

#ifdef __GNUC__
#define __ATTRIB_PACK           __attribute__ ((packed))
#define __ATTRIB_PRINTF         __attribute__ ((format (printf, 1, 2)))
#define __ATTRIB_NORETURN       __attribute__ ((noreturn))
#ifndef INLINE
#define INLINE                  __inline__
#endif
#else                           /* Not GCC */
#define __ATTRIB_PACK
#define __ATTRIB_PRINTF
#define __ATTRIB_NORETURN
#ifndef INLINE
#define INLINE                  __inline
#endif
#endif /* End __GNUC__ */

#define PREPACK
#define POSTPACK                __ATTRIB_PACK

#define A_MEMCPY(dst, src, len)         memcpy((A_UINT8 *)(dst), (src), (len))
#define A_MEMZERO(addr, len)            memset(addr, 0, len)
#define A_MEMSET(addr, value, size)     memset((addr), (value), (size))
#define A_MEMCMP(addr1, addr2, len)     memcmp((addr1), (addr2), (len))

#define A_LOGGER(mask, mod, args ...) \
	QDF_TRACE_ERROR(QDF_MODULE_ID_QDF, args)
#define A_PRINTF(args ...) \
	QDF_TRACE_ERROR(QDF_MODULE_ID_QDF, args)
#define A_SNPRINTF(buf, len, args ...)   snprintf(buf, len, args)
#define A_OFFSETOF(type, field)   offsetof(type, field)

/*
 * Timer Functions
 */
#define A_MSLEEP(msecs)							   \
	{									   \
		set_current_state(TASK_INTERRUPTIBLE);				       \
		schedule_timeout((HZ * (msecs)) / 1000);			       \
		set_current_state(TASK_RUNNING);				       \
	}

typedef struct timer_list A_TIMER;

/*
 * Wait Queue related functions
 */
#ifndef wait_event_interruptible_timeout
#define __wait_event_interruptible_timeout(wq, condition, ret)		\
	do {									\
		wait_queue_t __wait;						\
		init_waitqueue_entry(&__wait, current);				\
									\
		add_wait_queue(&wq, &__wait);					\
		for (;; ) {							 \
			set_current_state(TASK_INTERRUPTIBLE);			\
			if (condition)						\
				break;						\
			if (!signal_pending(current)) {				\
				ret = schedule_timeout(ret);			\
				if (!ret)					\
					break;					\
				continue;					\
			}							\
			ret = -ERESTARTSYS;					\
			break;							\
		}								\
		current->state = TASK_RUNNING;					\
		remove_wait_queue(&wq, &__wait);				\
	} while (0)

#define wait_event_interruptible_timeout(wq, condition, timeout)	\
	({									\
		long __ret = timeout;						 \
		if (!(condition))						 \
			__wait_event_interruptible_timeout(wq, condition, __ret); \
		__ret;								 \
	 })
#endif /* wait_event_interruptible_timeout */

#ifdef WLAN_DEBUG
#ifdef A_SIMOS_DEVHOST
extern unsigned int panic_on_assert;
#define A_ASSERT(expr)	\
	if (!(expr)) {	 \
		printk(KERN_ALERT "Debug Assert Caught, File %s, Line: %d, Test:%s\n", __FILE__, __LINE__, # expr); \
		if (panic_on_assert) panic(# expr);								  \
	}
#else
#define A_ASSERT(expr)	\
	if (!(expr)) {	 \
		printk(KERN_ALERT "Debug Assert Caught, File %s, Line: %d, Test:%s\n", __FILE__, __LINE__, # expr); \
	}
#endif
#else
#define A_ASSERT(expr)
#endif /* DEBUG */

#ifdef ANDROID_ENV
struct firmware;
int android_request_firmware(const struct firmware **firmware_p,
			     const char *filename, struct device *device);
void android_release_firmware(const struct firmware *firmware);
#define A_REQUEST_FIRMWARE(_ppf, _pfile, _dev) android_request_firmware(_ppf, _pfile, _dev)
#define A_RELEASE_FIRMWARE(_pf) android_release_firmware(_pf)
#else
#define A_REQUEST_FIRMWARE(_ppf, _pfile, _dev) request_firmware(_ppf, _pfile, _dev)
#define A_RELEASE_FIRMWARE(_pf) release_firmware(_pf)
#endif

/*
 * Network buffer queue support
 */
typedef struct sk_buff_head A_NETBUF_QUEUE_T;

#define A_NETBUF_FREE(bufPtr) \
	a_netbuf_free(bufPtr)
#define A_NETBUF_LEN(bufPtr) \
	a_netbuf_to_len(bufPtr)
#define A_NETBUF_PUSH(bufPtr, len) \
	a_netbuf_push(bufPtr, len)
#define A_NETBUF_PUT(bufPtr, len) \
	a_netbuf_put(bufPtr, len)
#define A_NETBUF_TRIM(bufPtr, len) \
	a_netbuf_trim(bufPtr, len)
#define A_NETBUF_PULL(bufPtr, len) \
	a_netbuf_pull(bufPtr, len)
#define A_NETBUF_HEADROOM(bufPtr) \
	a_netbuf_headroom(bufPtr)
#define A_NETBUF_SETLEN(bufPtr, len) \
	a_netbuf_setlen(bufPtr, len)

/* Add data to end of a buffer  */
#define A_NETBUF_PUT_DATA(bufPtr, srcPtr,  len)	\
	a_netbuf_put_data(bufPtr, srcPtr, len)

/* Add data to start of the  buffer */
#define A_NETBUF_PUSH_DATA(bufPtr, srcPtr,  len) \
	a_netbuf_push_data(bufPtr, srcPtr, len)

/* Remove data at start of the buffer */
#define A_NETBUF_PULL_DATA(bufPtr, dstPtr, len)	\
	a_netbuf_pull_data(bufPtr, dstPtr, len)

/* Remove data from the end of the buffer */
#define A_NETBUF_TRIM_DATA(bufPtr, dstPtr, len)	\
	a_netbuf_trim_data(bufPtr, dstPtr, len)

/* View data as "size" contiguous bytes of type "t" */
#define A_NETBUF_VIEW_DATA(bufPtr, t, size) \
	(t)(((struct skbuf *)(bufPtr))->data)

/* return the beginning of the headroom for the buffer */
#define A_NETBUF_HEAD(bufPtr) \
	((((struct sk_buff *)(bufPtr))->head))

/*
 * OS specific network buffer access routines
 */
void a_netbuf_free(void *bufPtr);
void *a_netbuf_to_data(void *bufPtr);
A_UINT32 a_netbuf_to_len(void *bufPtr);
A_STATUS a_netbuf_push(void *bufPtr, A_INT32 len);
A_STATUS a_netbuf_push_data(void *bufPtr, char *srcPtr, A_INT32 len);
A_STATUS a_netbuf_put(void *bufPtr, A_INT32 len);
A_STATUS a_netbuf_put_data(void *bufPtr, char *srcPtr, A_INT32 len);
A_STATUS a_netbuf_pull(void *bufPtr, A_INT32 len);
A_STATUS a_netbuf_pull_data(void *bufPtr, char *dstPtr, A_INT32 len);
A_STATUS a_netbuf_trim(void *bufPtr, A_INT32 len);
A_STATUS a_netbuf_trim_data(void *bufPtr, char *dstPtr, A_INT32 len);
A_STATUS a_netbuf_setlen(void *bufPtr, A_INT32 len);
A_INT32 a_netbuf_headroom(void *bufPtr);
void a_netbuf_enqueue(A_NETBUF_QUEUE_T *q, void *pkt);
void a_netbuf_prequeue(A_NETBUF_QUEUE_T *q, void *pkt);
void *a_netbuf_dequeue(A_NETBUF_QUEUE_T *q);
int a_netbuf_queue_size(A_NETBUF_QUEUE_T *q);
int a_netbuf_queue_empty(A_NETBUF_QUEUE_T *q);
int a_netbuf_queue_empty(A_NETBUF_QUEUE_T *q);
void a_netbuf_queue_init(A_NETBUF_QUEUE_T *q);

#ifdef QCA_PARTNER_PLATFORM
#include "ath_carr_pltfrm.h"
#endif /* QCA_PARTNER_PLATFORM */

#else                           /* __KERNEL__ */

#ifdef __GNUC__
#define __ATTRIB_PACK           __attribute__ ((packed))
#define __ATTRIB_PRINTF         __attribute__ ((format (printf, 1, 2)))
#define __ATTRIB_NORETURN       __attribute__ ((noreturn))
#ifndef inline
#define inline                  __inline__
#endif
#ifndef INLINE
#define INLINE                  __inline__
#endif
#else                           /* Not GCC */
#define __ATTRIB_PACK
#define __ATTRIB_PRINTF
#define __ATTRIB_NORETURN
#ifndef inline
#define inline                  __inline
#endif
#ifndef INLINE
#define INLINE                  __inline
#endif
#endif /* End __GNUC__ */

#define PREPACK
#define POSTPACK                __ATTRIB_PACK

#define A_MEMCPY(dst, src, len)         memcpy((dst), (src), (len))
#define A_MEMSET(addr, value, size)     memset((addr), (value), (size))
#define A_MEMZERO(addr, len)            memset((addr), 0, (len))
#define A_MEMCMP(addr1, addr2, len)     memcmp((addr1), (addr2), (len))

#ifdef ANDROID
#ifndef err
#include <linux/errno.h>
#define err(_s, args ...) do { \
		fprintf(stderr, "%s: line %d ", __FILE__, __LINE__); \
		fprintf(stderr, args); fprintf(stderr, ": %d\n", errno); \
		exit(_s); } while (0)
#endif
#else
#include <linux/err.h>
#endif

#endif /* __KERNEL__ */

#endif /* _OSAPI_LINUX_H_ */
