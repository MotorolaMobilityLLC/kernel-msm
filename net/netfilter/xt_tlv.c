/*
 * TLV data matching match for iptables
 *
 * (C) 2017 liu hongjie <liuhj7@lenovo.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_tlv.h>
#include <net/netlink.h>
#include <net/genetlink.h>

#define TLV_NL_EVENT 113
#define TLV_MAX_SIZE 128

struct xt_tlv_entry {
	u32 tag;
	u32 len;
	u32 offset;
	u8 data[];
};

/* this cmd is sent to application. because generic net link not permit NULL
 * ops and must define one cmd handler. here we give one empty function.
 * just ignore. Application doesn't need to issue this cmd.
 */
static
int tlv_nl_notify_cmd_fun(struct sk_buff *skb, struct genl_info *info)
{
	(void)skb;
	(void)info;
	return 0;
}

static const
struct nla_policy tlv_notify_cmd_policy[TLV_CMD_ATTR_MAX + 1] = {
	[TLV_CMD_ATTR_TOKEN] = { .type = NLA_S32 },
};

enum tlv_multicast_groups {
	TLV_MCGRP_NOTIFY,
	TLV_MCGRP_NUM
};

const struct genl_multicast_group tlv_mcgrps[TLV_MCGRP_NUM] = {
	[TLV_MCGRP_NOTIFY] = { .name = "notify", },
};

static struct genl_family tlv_family = {
	.id = TLV_NL_EVENT,
	.name = TLV_NL_NAME,
	.version = TLV_NL_VERSION,
	.netnsok = true,
	.mcgrp_offset = 0,
	.maxattr = TLV_CMD_MAX,
};

static const struct genl_ops tlv_ops[TLV_CMD_MAX] = {
	{
		.cmd = TLV_CMD_NOTIFY,
		.doit = tlv_nl_notify_cmd_fun,
		.policy = tlv_notify_cmd_policy,
		.flags = GENL_CMD_CAP_DO,
	}
};

static int register_tlv_genl_family(void)
{
	int ret = 0;

	ret = genl_register_family_with_ops_groups(&tlv_family,
						   tlv_ops,
						   tlv_mcgrps);
	if (ret < 0)
		pr_err("tlv register tlv genl family failed\n");
	return ret;
}

static void tlv_notify(u32 token)
{
	int ret = 0;
	struct sk_buff *skb = NULL;
	void *msg_head = NULL;

	skb = genlmsg_new(GENLMSG_DEFAULT_SIZE, GFP_ATOMIC);
	if (!skb) {
		pr_err("tlv: genlmsg_new failed\n");
		goto exit_err;
	}

	msg_head = genlmsg_put(skb, 0, 0, &tlv_family,
			       0, TLV_CMD_NOTIFY);
	if (!msg_head) {
		pr_err("tlv: genlmsg_put failed\n");
		goto exit_err;
	}

	/* we report token to client */
	ret = nla_put_u32(skb, TLV_CMD_ATTR_TOKEN, token);
	if (ret < 0)
		goto exit_err;

	genlmsg_end(skb, msg_head);
	genlmsg_multicast_allns(&tlv_family, skb, 0,
				TLV_MCGRP_NOTIFY, GFP_ATOMIC);
	return;
exit_err:
	if (skb)
		nlmsg_free(skb);
}

static bool process_tlv_data(const struct sk_buff *skb, int offset,
			     const struct xt_action_param *par){
	bool ret = false;
	int i = 0;
	u32 min, max;
	u8 range_data[4] = { 0 };
	u32 match_data = 0;
	u8 tlv_data[TLV_MAX_SIZE] = { 0 };
	const u8 *p_tlv = NULL;
	const struct xt_tlv_mtinfo *info = par->matchinfo;
	struct xt_tlv_entry *entry = NULL;
	const u8 *p_data = info->entries_data;
	u16 data_left = info->data_len;

	if (!info ||
	    info->entries < 1 ||
	    info->data_len < 1 ||
	    skb->len == offset ||
	    skb->len < info->data_len ||
	    info->data_len > TLV_MAX_SIZE)
		return ret;

	p_tlv = skb_header_pointer(skb, offset,
				   info->data_len,
				   tlv_data);

	if (!p_tlv)
		return ret;

	for (i = 0; i < info->entries; i++) {
		entry = (struct xt_tlv_entry *)p_data;
		if (data_left < entry->len) {
			/* exit when client set a invalid entry data len */
			ret = false;
			break;
		}
		data_left -= entry->len;
		switch (entry->tag) {
		case RANGE_TAG:
			if (entry->len < 1 ||
			    entry->len > 4) {
				ret = false;
				break;
			}
			memcpy(range_data + 4 - entry->len,
			       p_tlv + entry->offset, entry->len);
			match_data = ntohl(*((u32 *)range_data));
			memcpy(&min, entry->data, sizeof(min));
			memcpy(&max, entry->data + sizeof(min), sizeof(max));
			if (match_data >= min && match_data <= max)
				ret = true;
			else
				ret = false;
			p_data += sizeof(struct xt_tlv_entry) +
				  sizeof(min) + sizeof(max);
			break;
		case DATA_TAG:
			if (memcmp(p_tlv + entry->offset,
				   entry->data, entry->len) == 0)
				ret = true;
			else
				ret = false;
			p_data += sizeof(struct xt_tlv_entry) + entry->len;
			break;
		default:
			ret = false;
			break;
		}
		if (!ret) {
			/* if one of tag can't match, break the loop */
			break;
		}
	}

	if (ret && info->notify)
		tlv_notify(info->token);

	return ret;
}

static bool parse_skb(const struct sk_buff *skb,
		      const struct xt_action_param *par)
{
	bool ret = false;
	int data_offset = 0;
	int trans_hdr_len = 0;

	if (skb->sk->sk_protocol == IPPROTO_TCP)
		trans_hdr_len = tcp_hdr(skb)->doff << 2;
	else if (skb->sk->sk_protocol == IPPROTO_UDP)
		trans_hdr_len = sizeof(struct udphdr);

	data_offset = skb_network_header_len(skb) + trans_hdr_len;
	ret = process_tlv_data(skb, data_offset, par);

	return ret;
}

static bool tlv_mt(const struct sk_buff *skb, struct xt_action_param *par)
{
	if (!par || !par->matchinfo)
		return false;

	if (!skb || !skb->sk || !skb->sk->sk_socket || skb->len <= 0)
		return false;

	if (!parse_skb(skb, par))
		return false;
	return true;
}

static struct xt_match tlv_mt_reg[] __read_mostly = {
	{
		.name      = "tlv",
		.revision  = 1,
		.family    = NFPROTO_IPV4,
		.match     = tlv_mt,
		.matchsize = sizeof(struct xt_tlv_mtinfo),
		.me        = THIS_MODULE,
	},
	{
		.name	   = "tlv",
		.revision  = 1,
		.family    = NFPROTO_IPV6,
		.match     = tlv_mt,
		.matchsize = sizeof(struct xt_tlv_mtinfo),
		.me        = THIS_MODULE,
	},
};

static int __init tlv_mt_init(void)
{
	int ret = 0;

	ret = register_tlv_genl_family();
	if (ret < 0) {
		pr_err("tlv_mt_init no memory\n");
		return -ENOMEM;
	}
	ret = xt_register_matches(tlv_mt_reg, ARRAY_SIZE(tlv_mt_reg));
	return ret;
}

static void __exit tlv_mt_exit(void)
{
	genl_unregister_family(&tlv_family);
	xt_unregister_matches(tlv_mt_reg, ARRAY_SIZE(tlv_mt_reg));
}

module_init(tlv_mt_init);
module_exit(tlv_mt_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("liu hongjie liuhj7@lenovo.com");
MODULE_DESCRIPTION("Xtables: arbitrary tlv matching");
MODULE_ALIAS("ipt_tlv");
MODULE_ALIAS("ip6t_tlv");
