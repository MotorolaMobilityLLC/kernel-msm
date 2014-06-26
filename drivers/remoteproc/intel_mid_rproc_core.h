/*
 * INTEL MID Remote Processor Core Head File
 *
 * Copyright (C) 2012 Intel, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

typedef int (*rpmsg_handle_t)(void *rx_buf, void *tx_buf,
				u32 *r_len, u32 *s_len);

int scu_ipc_rpmsg_handle(void *rx_buf, void *tx_buf, u32 *r_len, u32 *s_len);
int psh_ipc_rpmsg_handle(void *rx_buf, void *tx_buf, u32 *len);

#define RPROC_FW_LOADING_TIMEOUT	(3 * HZ)
#define IPROC_NAME_SIZE 20

/**
 * struct intel_mid_rproc - intel mid remote processor
 * @ns_enabled: name service enabled flag
 * @name: rproc name
 * @type: rproc type
 * @r_vring_last_used: last used index of rx vring
 * @s_vring_last_used: last used index of tx vring
 * @rproc: rproc handle
 * @rx_vring: rproc rx vring
 * @tx_vring: rproc tx vring
 * @ns_info: loop cursor when creating ns channels
 * @rproc_rpmsg_handle: rproc private rpmsg handle
 */
struct intel_mid_rproc {
	bool ns_enabled;
	char name[IPROC_NAME_SIZE];
	u32 type;
	u32 r_vring_last_used;
	u32 s_vring_last_used;
	struct rproc *rproc;
	struct vring rx_vring;
	struct vring tx_vring;
	struct rpmsg_ns_info *ns_info;
	rpmsg_handle_t rproc_rpmsg_handle;
};

enum local_vring_idx {
	RX_VRING,
	TX_VRING,
};

extern void intel_mid_rproc_vq_interrupt(struct rproc *rproc, int msg);
extern int intel_mid_rproc_msg_handle(struct intel_mid_rproc *iproc);
extern int intel_mid_rproc_ns_handle(struct intel_mid_rproc *iproc,
					struct rpmsg_ns_info *ns_info);

extern struct rproc_vdev *find_rvdev(struct rproc *rproc, int id);
extern int find_vring_index(struct rproc *rproc, int vqid, int id);
extern void intel_mid_rproc_vring_init(struct rproc *rproc,
			struct vring *vring, enum local_vring_idx id);

extern void rpmsg_ns_del_list(struct rpmsg_ns_list *nslist);

/* Please do NOT use these APIs to send ipc commands,
 * use rpmsg commands defined in <asm/intel_mid_rpmsg.h>
 */
extern void intel_scu_ipc_send_command(u32 cmd);

/* Issue commands to the SCU with or without data */
extern int intel_scu_ipc_simple_command(int cmd, int sub);
extern int intel_scu_ipc_command(u32 cmd, u32 sub, u8 *in, u32 inlen,
		u32 *out, u32 outlen);
extern int intel_scu_ipc_raw_cmd(u32 cmd, u32 sub, u8 *in, u32 inlen,
		u32 *out, u32 outlen, u32 dptr, u32 sptr);

/* IPC locking */
extern void intel_scu_ipc_lock(void);
extern void intel_scu_ipc_unlock(void);
