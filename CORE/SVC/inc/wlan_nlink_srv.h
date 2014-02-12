/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/******************************************************************************
 * wlan_nlink_srv.h
 *
 * wlan_nlink_srv is used to RX/TX Netlink messages from user space to kernel
 * modules and vice versa. Kernel modules must register a message handler for a
 * message type so that the wlan_nlink_srv can invoke the corresponding msg handler
 * whenever a Netlink message of a particular type has been received from an
 * application.  In the opposite direction, wlan_nlink_srv provides a mechanism  
 * which kernel modules can use to send Netlink messages to applications.
 *
 ******************************************************************************/

#ifndef WLAN_NLINK_SRV_H
#define WLAN_NLINK_SRV_H 

#include <linux/skbuff.h>
#include <net/netlink.h>
#include <wlan_nlink_common.h>

#define NLINK_MAX_CALLBACKS (WLAN_NL_MSG_MAX - WLAN_NL_MSG_BASE)

typedef int (* nl_srv_msg_callback)(struct sk_buff * skb);

int nl_srv_init(void);
#ifdef WLAN_KD_READY_NOTIFIER
void nl_srv_exit(int dst_pid);
#else
void nl_srv_exit(void);
#endif /* WLAN_KD_READY_NOTIFIER */
int nl_srv_register(tWlanNlModTypes msg_type, nl_srv_msg_callback msg_handler);
int nl_srv_unregister(tWlanNlModTypes msg_type, nl_srv_msg_callback msg_handler);
int nl_srv_ucast(struct sk_buff * skb, int dst_pid);
int nl_srv_bcast(struct sk_buff * skb);
#ifdef WLAN_KD_READY_NOTIFIER
void nl_srv_nl_ready_indication(void);
void nl_srv_nl_close_indication(int pid);
#endif /* WLAN_KD_READY_NOTIFIER */
#endif
