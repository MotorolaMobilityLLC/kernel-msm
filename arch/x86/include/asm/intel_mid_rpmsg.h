#ifndef _INTEL_MID_RPMSG_H_
#define _INTEL_MID_RPMSG_H_

#include <asm/scu_ipc_rpmsg.h>
#include <linux/wakelock.h>
#include <linux/rpmsg.h>

#define RPMSG_TX_TIMEOUT   (5 * HZ)

struct rpmsg_instance {
	struct rpmsg_channel *rpdev;
	struct mutex instance_lock;
	struct tx_ipc_msg *tx_msg;
	struct rx_ipc_msg *rx_msg;
	struct mutex rx_lock;
	struct completion reply_arrived;
	struct rpmsg_endpoint *endpoint;
};

struct rpmsg_lock {
	struct mutex lock;
	int locked_prev; /* locked prev flag */
	atomic_t pending;
};

extern int rpmsg_send_command(struct rpmsg_instance *instance, u32 cmd,
						u32 sub, u8 *in,
						u32 *out, u32 inlen,
						u32 outlen);

extern int rpmsg_send_raw_command(struct rpmsg_instance *instance, u32 cmd,
						u32 sub, u8 *in,
						u32 *out, u32 inlen,
						u32 outlen, u32 sptr,
						u32 dptr);

extern int rpmsg_send_simple_command(struct rpmsg_instance *instance, u32 cmd,
						u32 sub);

extern int alloc_rpmsg_instance(struct rpmsg_channel *rpdev,
				struct rpmsg_instance **pInstance);

extern void free_rpmsg_instance(struct rpmsg_channel *rpdev,
				struct rpmsg_instance **pInstance);

extern void init_rpmsg_instance(struct rpmsg_instance *instance);

extern int rpmsg_send_generic_command(u32 cmd, u32 sub, u8 *in, u32 inlen,
				u32 *out, u32 outlen);

extern int rpmsg_send_generic_simple_command(u32 cmd, u32 sub);

extern int rpmsg_send_generic_raw_command(u32 cmd, u32 sub,
				   u8 *in, u32 inlen,
				   u32 *out, u32 outlen,
				   u32 dptr, u32 sptr);

struct rpmsg_device_data {
	char name[RPMSG_NAME_SIZE];
	struct rpmsg_channel *rpdev;
	struct rpmsg_instance *rpmsg_instance;
};

enum rpmsg_ipc_command_type {
	RPMSG_IPC_COMMAND = 0,
	RPMSG_IPC_SIMPLE_COMMAND,
	RPMSG_IPC_RAW_COMMAND,
	RPMSG_IPC_COMMAND_TYPE_NUM,
};

extern void rpmsg_global_lock(void);
extern void rpmsg_global_unlock(void);

#endif
