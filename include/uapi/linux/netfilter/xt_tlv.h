#ifndef _LINUX_NETFILTER_XT_TLV_H
#define _LINUX_NETFILTER_XT_TLV_H

#include <linux/types.h>
#include <linux/netfilter.h>

#define TLV_NL_NAME "tlv"
#define TLV_NL_VERSION 1
#define MAX_MATCH_DATA 128

enum {
	RANGE_TAG = 0,
	DATA_TAG
};

struct xt_tlv_mtinfo {
	u32 token;
	/* it's the all key data length of the red packet */
	u16 data_len;
	/* the number of the entry */
	u16 entries;
	/* the data of the entries */
	u8 entries_data[MAX_MATCH_DATA];
};

enum {
	TLV_CMD_UNSPEC = 0,
	TLV_CMD_NOTIFY,
	TLV_CMD_MAX = TLV_CMD_NOTIFY
};

enum {
	TLV_CMD_ATTR_UNSPEC = 0,
	TLV_CMD_ATTR_TOKEN,
	TLV_CMD_ATTR_MAX = TLV_CMD_ATTR_TOKEN
};

#endif /* _LINUX_NETFILTER_XT_TLV_H */
