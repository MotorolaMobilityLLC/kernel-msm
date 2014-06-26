#ifndef _SCU_IPC_RPMSG_H_
#define _SCU_IPC_RPMSG_H_

struct tx_ipc_msg {
	u32 cmd;
	u32 sub;
	u8 *in;
	u32 *out;
	u32 inlen;	/* number of bytes to be written */
	u32 outlen;	/* number of dwords to be read */
	u32 sptr;	/* needed for raw ipc command */
	u32 dptr;	/* needed for raw ipc command */
};

struct rx_ipc_msg {
	u32 status;	/* Indicate IPC status, 0-success, 1-fail */
};

#endif
