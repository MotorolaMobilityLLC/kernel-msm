/*
 * Copyright (c) 2012-2015 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
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

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

/******************************************************************************
* wlan_nlink_srv.c
*
* This file contains the definitions specific to the wlan_nlink_srv
*
******************************************************************************/

/*
 * If MULTI_IF_NAME is not defined, then this is the primary instance of the
 * driver and the diagnostics netlink socket will be available. If
 * MULTI_IF_NAME is defined then this is not the primary instance of the driver
 * and the diagnotics netlink socket will not be available since this
 * diagnostics netlink socket can only be exposed by one instance of the driver.
 */
#ifndef MULTI_IF_NAME

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <wlan_nlink_srv.h>
#include <vos_trace.h>

/* Global variables */
static DEFINE_MUTEX(nl_srv_sem);
static struct sock *nl_srv_sock;
static nl_srv_msg_callback nl_srv_msg_handler[NLINK_MAX_CALLBACKS];

/* Forward declaration */
static void nl_srv_rcv (struct sk_buff *sk);
static void nl_srv_rcv_skb (struct sk_buff *skb);
static void nl_srv_rcv_msg (struct sk_buff *skb, struct nlmsghdr *nlh);

/*
 * Initialize the netlink service.
 * Netlink service is usable after this.
 */
int nl_srv_init(void)
{
   int retcode = 0;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0))
   struct netlink_kernel_cfg cfg = {
      .groups = WLAN_NLINK_MCAST_GRP_ID,
      .input = nl_srv_rcv
   };
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0))
   nl_srv_sock = netlink_kernel_create(&init_net, WLAN_NLINK_PROTO_FAMILY,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,7,0))
                                       THIS_MODULE,
#endif
                                       &cfg);
#else
   nl_srv_sock = netlink_kernel_create(&init_net, WLAN_NLINK_PROTO_FAMILY,
      WLAN_NLINK_MCAST_GRP_ID, nl_srv_rcv, NULL, THIS_MODULE);
#endif

   if (nl_srv_sock != NULL) {
      memset(nl_srv_msg_handler, 0, sizeof(nl_srv_msg_handler));
   } else {
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
         "NLINK: netlink_kernel_create failed");
      retcode = -ECONNREFUSED;
   }
   return retcode;
}

/*
 * Deinit the netlink service.
 * Netlink service is unusable after this.
 */
#ifdef WLAN_KD_READY_NOTIFIER
void nl_srv_exit(int dst_pid)
#else
void nl_srv_exit(void)
#endif /* WLAN_KD_READY_NOTIFIER */
{
   netlink_kernel_release(nl_srv_sock);
   nl_srv_sock = NULL;
}

/*
 * Register a message handler for a specified module.
 * Each module (e.g. WLAN_NL_MSG_BTC )will register a
 * handler to handle messages addressed to it.
 */
int nl_srv_register(tWlanNlModTypes msg_type, nl_srv_msg_callback msg_handler)
{
   int retcode = 0;

   if ((msg_type >= WLAN_NL_MSG_BASE) && (msg_type < WLAN_NL_MSG_MAX) &&
        msg_handler != NULL)
   {
      nl_srv_msg_handler[msg_type - WLAN_NL_MSG_BASE] = msg_handler;
   }
   else {
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
         "NLINK: nl_srv_register failed for msg_type %d", msg_type);
      retcode = -EINVAL;
   }

   return retcode;
}
/*
 * Unregister the message handler for a specified module.
 */
int nl_srv_unregister(tWlanNlModTypes msg_type, nl_srv_msg_callback msg_handler)
{
   int retcode = 0;

   if ((msg_type >= WLAN_NL_MSG_BASE) && (msg_type < WLAN_NL_MSG_MAX) &&
       (nl_srv_msg_handler[msg_type - WLAN_NL_MSG_BASE] == msg_handler))
   {
      nl_srv_msg_handler[msg_type - WLAN_NL_MSG_BASE] = NULL;
   }
   else
   {
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
         "NLINK: nl_srv_unregister failed for msg_type %d", msg_type);
      retcode = -EINVAL;
   }

   return retcode;
}

/*
 * Unicast the message to the process in user space identfied
 * by the dst-pid
 */
int nl_srv_ucast(struct sk_buff *skb, int dst_pid, int flag)
{
   int err = 0;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,7,0))
   NETLINK_CB(skb).pid = 0; //sender's pid
#else
   NETLINK_CB(skb).portid = 0; //sender's pid
#endif
   NETLINK_CB(skb).dst_group = 0; //not multicast

   if (nl_srv_sock != NULL) {
       err = netlink_unicast(nl_srv_sock, skb, dst_pid, flag);
   }
   if (err < 0)
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
      "NLINK: netlink_unicast to pid[%d] failed, ret[%d]", dst_pid, err);

   return err;
}

/*
 *  Broadcast the message. Broadcast will return an error if
 *  there are no listeners
 */
int nl_srv_bcast(struct sk_buff *skb)
{
   int err = 0;
   int flags = GFP_KERNEL;

   if (in_interrupt() || irqs_disabled() || in_atomic())
       flags = GFP_ATOMIC;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,7,0))
   NETLINK_CB(skb).pid = 0; //sender's pid
#else
   NETLINK_CB(skb).portid = 0; //sender's pid
#endif
   NETLINK_CB(skb).dst_group = WLAN_NLINK_MCAST_GRP_ID; //destination group

   if (nl_srv_sock != NULL) {
       err = netlink_broadcast(nl_srv_sock, skb, 0, WLAN_NLINK_MCAST_GRP_ID, flags);
   }
   if (err < 0)
   {
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
         "NLINK: netlink_broadcast failed err = %d", err);
   }
   return err;
}

/*
 *  Processes the Netlink socket input queue.
 *  Dequeue skb's from the socket input queue and process
 *  all the netlink messages in that skb, before moving
 *  to the next skb.
 */
static void nl_srv_rcv (struct sk_buff *sk)
{
   mutex_lock(&nl_srv_sem);
   nl_srv_rcv_skb(sk);
   mutex_unlock(&nl_srv_sem);
}

/*
 * Each skb could contain multiple Netlink messages. Process all the
 * messages in one skb and discard malformed skb's silently.
 */
static void nl_srv_rcv_skb (struct sk_buff *skb)
{
   struct nlmsghdr * nlh;

   while (skb->len >= NLMSG_SPACE(0)) {
      u32 rlen;

      nlh = (struct nlmsghdr *)skb->data;

      if (nlh->nlmsg_len < sizeof(*nlh) || skb->len < nlh->nlmsg_len) {
         VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN, "NLINK: Invalid "
            "Netlink message: skb[%p], len[%d], nlhdr[%p], nlmsg_len[%d]",
            skb, skb->len, nlh, nlh->nlmsg_len);
         return;
      }

      rlen = NLMSG_ALIGN(nlh->nlmsg_len);
      if (rlen > skb->len)
         rlen = skb->len;
      nl_srv_rcv_msg(skb, nlh);
      skb_pull(skb, rlen);
   }
}

/*
 * Process a netlink message.
 * Each netlink message will have a message of type tAniMsgHdr inside.
 */
static void nl_srv_rcv_msg (struct sk_buff *skb, struct nlmsghdr *nlh)
{
   int type;

   /* Only requests are handled by kernel now */
   if (!(nlh->nlmsg_flags & NLM_F_REQUEST)) {
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
         "NLINK: Received Invalid NL Req type [%x]", nlh->nlmsg_flags);
      return;
   }

   type = nlh->nlmsg_type;

   /* Unknown message */
   if (type < WLAN_NL_MSG_BASE || type >= WLAN_NL_MSG_MAX) {
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
         "NLINK: Received Invalid NL Msg type [%x]", type);
      return;
   }

   /*
   * All the messages must at least carry the tAniMsgHdr
   * Drop any message with invalid length
   */
   if (nlh->nlmsg_len < NLMSG_LENGTH(sizeof(tAniMsgHdr))) {
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
         "NLINK: Received NL Msg with invalid len[%x]", nlh->nlmsg_len);
      return;
   }

   // turn type into dispatch table offset
   type -= WLAN_NL_MSG_BASE;

   // dispatch to handler
   if (nl_srv_msg_handler[type] != NULL) {
      (nl_srv_msg_handler[type])(skb);
   } else {
      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
         "NLINK: No handler for Netlink Msg [0x%X]", type);
   }
}

/**
 * nl_srv_is_initialized() - This function is used check if the netlink
 * service is initialized
 *
 * This function is used check if the netlink service is initialized
 *
 * Return: Return -EPERM if the service is not initialized
 *
 */
int nl_srv_is_initialized()
{
	if (nl_srv_sock)
		return 0;
	else
		return -EPERM;
}

#else /* ifndef MULTI_IF_NAME */

#include <wlan_nlink_srv.h>

int nl_srv_init(void)
{
	return 0;
}

void nl_srv_exit(int dst_pid)
{
}

int nl_srv_register(tWlanNlModTypes msg_type, nl_srv_msg_callback msg_handler)
{
	return 0;
}

int nl_srv_unregister(tWlanNlModTypes msg_type, nl_srv_msg_callback msg_handler)
{
	return 0;
}

int nl_srv_ucast(struct sk_buff *skb, int dst_pid, int flag)
{
	return 0;
}

int nl_srv_bcast(struct sk_buff *skb)
{
	return 0;
}

int nl_srv_is_initialized()
{
	return 0;
}

#endif
