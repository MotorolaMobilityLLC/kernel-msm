/******************************************************************************
 *
 * Copyright (c) 2011, Intel Corporation.
 * Portions (c), Imagination Technology, Ltd.
 * All rights reserved.
 *
 * Redistribution and Use.  Redistribution and use in binary form, without
 * modification, of the software code provided with this license ("Software"),
 * are permitted provided that the following conditions are met:
 *
 *  1. Redistributions must reproduce the above copyright notice and this
 *     license in the documentation and/or other materials provided with the
 *     Software.
 *  2. Neither the name of Intel Corporation nor the name of Imagination
 *     Technology, Ltd may be used to endorse or promote products derived from
 *     the Software without specific prior written permission.
 *  3. The Software can only be used in connection with the Intel hardware
 *     designed to use the Software as outlined in the documentation. No other
 *     use is authorized.
 *  4. No reverse engineering, decompilation, or disassembly of the Software
 *     is permitted.
 *  5. The Software may not be distributed under terms different than this
 *     license.
 *
 * Limited Patent License.  Intel Corporation grants a world-wide, royalty-free
 * , non-exclusive license under patents it now or hereafter owns or controls
 * to make, have made, use, import, offer to sell and sell ("Utilize") the
 * Software, but solely to the extent that any such patent is necessary to
 * Utilize the Software alone.  The patent license shall not apply to any
 * combinations which include the Software.  No hardware per se is licensed
 * hereunder.
 *
 * Ownership of Software and Copyrights. Title to all copies of the Software
 * remains with the copyright holders. The Software is copyrighted and
 * protected by the laws of the United States and other countries, and
 * international treaty provisions.
 *
 * DISCLAIMER.  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
******************************************************************************/

#include <linux/version.h>
#include <linux/fs.h>
#include <net/genetlink.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include "dispmgrnl.h"
#include "psb_dpst_func.h"
#include "psb_powermgmt.h"

#define NETLINK_DISPMGR		20

static unsigned int g_pid = 0;

static struct drm_device *g_dev = NULL;
struct sock *nl_sk = NULL;

static void execute_recv_command(struct dispmgr_command_hdr *cmd_hdr)
{
	switch (cmd_hdr->module) {
	case DISPMGR_MOD_NETLINK:
		{
			switch (cmd_hdr->cmd) {
			case DISPMGR_TEST:
				{
					struct dispmgr_command_hdr send_cmd_hdr;
					unsigned int data = 0xdeadbeef;

					if (cmd_hdr->data_size) {
						unsigned int value =
						    *((unsigned int *)
						      cmd_hdr->data);
						printk
						    ("kdispmgr: received DISPMGR_TEST cmd data = 0x%x.\n",
						     value);
					} else
						printk
						    ("kdispmgr: received DISPMGR_TEST cmd NO data.\n");

					send_cmd_hdr.data_size = sizeof(data);
					send_cmd_hdr.data = &data;
					send_cmd_hdr.module =
					    DISPMGR_MOD_NETLINK;
					send_cmd_hdr.cmd = DISPMGR_TEST;
					dispmgr_nl_send_msg(&send_cmd_hdr);
				}
				break;
			case DISPMGR_TEST_TEXT:
				{
					struct dispmgr_command_hdr send_cmd_hdr;
					char *data = "can you hear me?";

					if (cmd_hdr->data_size) {
						printk
						    ("kdispmgr: received DISPMGR_TEST_TEXT cmd text = 0x%s.\n",
						     (char *)cmd_hdr->data);
					} else
						printk
						    ("kdispmgr: received DISPMGR_TEST_TEXT cmd NO text.\n");

					send_cmd_hdr.module =
					    DISPMGR_MOD_NETLINK;
					send_cmd_hdr.cmd = DISPMGR_TEST_TEXT;
					send_cmd_hdr.data_size =
					    strlen(data) + 1;
					send_cmd_hdr.data = (void *)data;
					dispmgr_nl_send_msg(&send_cmd_hdr);
				}
				break;
			default:
				{
					printk
					    ("kdispmgr: received unknown command = %d.\n",
					     cmd_hdr->cmd);
				};
			};	/* switch */
		}
		break;
	case DISPMGR_MOD_DPST:
		{
			dpst_execute_recv_command(cmd_hdr);
		}
		break;
	default:
		{
			printk("kdispmgr: received unknown module = %d.\n",
			       cmd_hdr->module);
		};
	}			/* switch */
}

/* Send Message to user mode */
void dispmgr_nl_send_msg(struct dispmgr_command_hdr *cmd_hdr)
{
	struct nlmsghdr *nlh;
	struct sk_buff *skb_out;
	unsigned int msg_size = 0;
	unsigned int data_size = 0;
	unsigned int hdr_size = 0;
	int ret = 0;

	/* if no user mode process active */
	if (!g_pid)
		return;

	hdr_size = sizeof(struct dispmgr_command_hdr);
	data_size = hdr_size + cmd_hdr->data_size;
	msg_size = data_size + sizeof(struct nlmsghdr);

	skb_out = nlmsg_new(msg_size, 0);
	if (!skb_out) {
		printk("kdispmgr: Failed to allocated skb\n");
		return;
	}

	nlh = nlmsg_put(skb_out, 0, 0, NLMSG_DONE, msg_size, 0);
	NETLINK_CB(skb_out).dst_group = 0;	/* not in mcast group */

	memcpy(nlmsg_data(nlh), cmd_hdr, hdr_size);
	if (cmd_hdr->data_size) {
		memcpy(nlmsg_data(nlh) + hdr_size, cmd_hdr->data,
		       cmd_hdr->data_size);
	}
	ret = netlink_unicast(nl_sk, skb_out, g_pid, MSG_DONTWAIT);
}

/* Receive Message from Kernel */
static void nl_recv_msg(struct sk_buff *skb)
{
	struct nlmsghdr *nlh = NULL;
	struct dispmgr_command_hdr cmd_hdr;
	unsigned int hdr_size = sizeof(struct dispmgr_command_hdr);

	if (skb == NULL) {
		printk("kdispmgr: received null command.\n");
		return;
	}

	nlh = (struct nlmsghdr *)skb->data;
	g_pid = nlh->nlmsg_pid;

	memcpy((void *)(&cmd_hdr), NLMSG_DATA(nlh), hdr_size);
	if (cmd_hdr.data_size) {
		cmd_hdr.data = NLMSG_DATA(nlh) + hdr_size;
	}

	execute_recv_command(&cmd_hdr);
}

#define KEEP_UNUSED_CODE 0

#if KEEP_UNUSED_CODE
static void dispmgr_nl_exit(void)
{
	printk(KERN_INFO "kdispmgr: exiting hello module\n");
	netlink_kernel_release(nl_sk);
	g_pid = 0;
}
#endif /* if KEEP_UNUSED_CODE */

static int dispmgr_nl_init(void)
{
	int ret = 0;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0))
	nl_sk = netlink_kernel_create(&init_net,
				      NETLINK_DISPMGR,
				      0, nl_recv_msg, NULL, THIS_MODULE);
#else
        struct netlink_kernel_cfg cfg = {
                 .groups         = 0,
                 .input          = nl_recv_msg,
                 .cb_mutex       = NULL,
                 .flags          = THIS_MODULE,
	};

	nl_sk = netlink_kernel_create(&init_net, NETLINK_DISPMGR, &cfg);
#endif

	if (!nl_sk) {
		printk(KERN_ALERT "kdispmgr: error creating netlink socket.\n");
		ret = -10;
	} else {
		printk(KERN_ALERT
		       "kdispmgr: netlink socket created successfully.\n");
		ret = 0;
	}

	return ret;
}

void dispmgr_start(struct drm_device *dev)
{
	g_dev = dev;
	printk("kdispmgr: display manager start.\n");
	dispmgr_nl_init();
	return;
}

/* this function is only called by dpms on or late resume function */
void dpstmgr_reg_restore_locked(struct drm_device *dev, struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_hw_context *ctx = NULL;
	struct drm_psb_private *dev_priv = NULL;
	struct mdfld_dsi_hw_registers *regs = NULL;

	if (!dsi_config || !dsi_config->dev)
		return;

	ctx = &dsi_config->dsi_hw_context;
	regs = &dsi_config->regs;
	dev_priv = dsi_config->dev->dev_private;

	if (power_island_get(OSPM_DISPLAY_A))
	{

		PSB_WVDC32(ctx->histogram_intr_ctrl, regs->histogram_intr_ctrl_reg);
		PSB_WVDC32(ctx->histogram_logic_ctrl, regs->histogram_logic_ctrl_reg);
		PSB_WVDC32(ctx->aimg_enhance_bin, regs->aimg_enhance_bin_reg);
		PSB_WVDC32(ctx->lvds_port_ctrl, regs->lvds_port_ctrl_reg);

		power_island_put(OSPM_DISPLAY_A);
	}
}
