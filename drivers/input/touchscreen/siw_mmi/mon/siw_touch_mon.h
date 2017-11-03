/*
 * SiW touch monitor
 *
 * Copyright (C) 2016 Silicon Works - http://www.siliconworks.co.kr
 * Author: Hyunho Kim <kimhh@siliconworks.co.kr>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

#ifndef __SIW_TOUCH_MON_H
#define __SIW_TOUCH_MON_H

#include <linux/list.h>
#include <linux/slab.h>
#include <linux/kref.h>

/*
 * Do not use __SIW_MON_USE_BUF_SLAB
 * memory leakage problem found
 */
#if defined(__SIW_MON_USE_BUF_SLAB)
#define __SIW_MON_USE_BUF_SLAB_BUS
#define __SIW_MON_USE_BUF_SLAB_OPS
#endif

#ifndef KERN_WARN
#define KERN_WARN	KERN_WARNING
#endif

#define SIW_MON_NAME	"siwmon"
#define SIW_MON_TAG	"siwmon"

#define mon_pr_info(fmt, args...)	pr_info(SIW_MON_TAG ": " fmt, ##args)
#define mon_pr_noti(fmt, args...)	pr_notice(SIW_MON_TAG ": " fmt, ##args)
#define mon_pr_warn(fmt, args...)	pr_warn(SIW_MON_TAG ": " fmt, ##args)
#define mon_pr_err(fmt, args...)	pr_err(SIW_MON_TAG ": " fmt, ##args)
#define mon_pr_dbg(fmt, args...)	pr_debug(SIW_MON_TAG ": " fmt, ##args)

#define mon_pr_test		mon_pr_dbg

enum {
	BIT_OFF16	= 16,
	BIT_OFF8	= 8,
	MASK_BIT16	= ((1L<<BIT_OFF16)-1),
	MASK_BIT8	= ((1L<<BIT_OFF8)-1),
};

/*
 * [31:16] : original total size
 * [15:00] : adjusted total size by print limit
 * See '__siw_mon_txt_read_get_len'
 */
static inline int siw_mon_buf_size_tot(int size)
{
	return (size>>BIT_OFF16);
}

static inline int siw_mon_buf_size_cur(int size)
{
	return (size & MASK_BIT16);
}

static inline int siw_mon_set_buf_size(int cur, int tot)
{
	return ((tot<<BIT_OFF16) | cur);
}

/*
 *
 */
static inline int siw_mon_priv_sub(int priv)
{
	return (priv>>BIT_OFF16);
}

static inline int siw_mon_priv_main(int priv)
{
	return (priv & MASK_BIT16);
}

/*
 * [15:08] : curr idx
 * [07:00] : total count
 */
static inline int siw_mon_priv_cur(int priv)
{
	return (priv>>BIT_OFF8);
}

static inline int siw_mon_priv_tot(int priv)
{
	return (priv & MASK_BIT8);
}

static inline int siw_mon_set_priv_sub(int priv, int cur, int tot)
{
	int priv_sub = (cur<<BIT_OFF8) | tot;

	priv &= MASK_BIT16;
	priv |= priv_sub<<BIT_OFF16;
	return priv;
}

enum {
	BUS_NAME_SZ = 32,
};

struct siw_mon_data_bus {
	char dir[BUS_NAME_SZ];
	u8 *tx_buf;
	int tx_size;
	u8 *rx_buf;
	int rx_size;
	dma_addr_t tx_dma;
	dma_addr_t rx_dma;
	int priv;
};

enum {
	EVT_NAME_SZ = 32,
};

struct siw_mon_data_evt {
	char type[EVT_NAME_SZ];
	int type_v;
	char code[EVT_NAME_SZ];
	int code_v;
	int value;
	int priv;
};

enum {
	OPS_NAME_SZ = 128,
	OPS_DATA_SZ = 8,
};

struct siw_mon_data_ops {
	char ops[OPS_NAME_SZ];
	size_t data[OPS_DATA_SZ];
	int len;
	int priv;
};

struct siw_mon_data {
	int type;
	int cnt_events;
	void *dev;
	int ret;
	union {
		struct siw_mon_data_bus bus;
		struct siw_mon_data_evt evt;
		struct siw_mon_data_ops ops;
	} d;
	int inc_cnt;
};

enum _SIW_MON_TYPE {
	SIW_MON_NONE = 0,
	SIW_MON_BUS,
	SIW_MON_EVT,
	SIW_MON_OPS,
	/* */
	SIW_MON_ALL = 0x80,
};

#define SIW_MON_ALL_NAME	"all"
#define SIW_MON_BUS_NAME	"bus"
#define SIW_MON_EVT_NAME	"evt"
#define SIW_MON_OPS_NAME	"ops"


#define SIW_MON_SOCK_RSTR	"@ABTR"
#define SIW_MON_SOCK_SSTR	"@ABTS"

enum {
	SIW_MON_SOCK_NONE = 0,
	SIW_MON_SOCK_R,
	SIW_MON_SOCK_S,
	SIW_MON_SOCK_ERR = -1,
};

static inline int siw_mon_txt_sock_str_cmp(char *ops)
{
	if (!strncmp(SIW_MON_SOCK_RSTR, ops, sizeof(SIW_MON_SOCK_RSTR)))
		return SIW_MON_SOCK_R;

	if (!strncmp(SIW_MON_SOCK_SSTR, ops, sizeof(SIW_MON_SOCK_SSTR)))
		return SIW_MON_SOCK_S;

	return SIW_MON_SOCK_ERR;
}


/*
 * Mon terminal
 */
struct siw_mon_term {
	int type;
	char name[16];

	struct list_head term_link;
	spinlock_t lock;

	int prt_inited;

	struct dentry *mfile;		/* Text interface file */

	/* Ref */
	int nreaders;
	struct list_head r_list;
	struct kref ref;

	unsigned int cnt_events;
	unsigned int cnt_text_lost;

	int monitored;
};

/*
 * An instance of a process which opened a file (but can fork later)
 */
struct siw_mon_reader {
	struct list_head r_link;
	struct siw_mon_term *mterm;
	void *r_data;		/* Use container_of instead? */

	void (*submit)(void *r_data, struct siw_mon_data *data,
			unsigned long *flags);
};

extern struct mutex __siw_mon_lock;

extern void
siw_mon_reader_add(struct siw_mon_term *mterm, struct siw_mon_reader *r);
extern void
siw_mon_reader_del(struct siw_mon_term *mterm, struct siw_mon_reader *r);
extern int siw_mon_buf_max(void);
extern int siw_mon_prt_max(void);
extern int siw_mon_prt_init(void);
extern void siw_mon_prt_exit(void);
extern int siw_mon_prt_add(struct siw_mon_term *mterm, char *name);
extern void siw_mon_prt_del(struct siw_mon_term *mterm);
extern void *siw_mon_get_buf_cache(unsigned char *src, int len);
extern void siw_mon_put_buf_cache(void *buf);

#endif	/* __SIW_TOUCH_MON_H */


