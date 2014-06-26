#ifndef _ASM_X86_INTEL_PSH_IPC_H_
#define _ASM_X86_INTEL_PSH_IPC_H_

#define CHANNEL_BUSY		(1 << 31)
#define PSH_IPC_CONTINUE	(1 << 30)

struct psh_msg {
	u32 msg;
	u32 param;
};

enum psh_channel {
	PSH_SEND_CH0 = 0,
	PSH_SEND_CH1,
	PSH_SEND_CH2,
	PSH_SEND_CH3,
	NUM_IA2PSH_IPC,
	PSH_RECV_CH0 = NUM_IA2PSH_IPC,
	PSH_RECV_CH1,
	PSH_RECV_CH2,
	PSH_RECV_CH3,
	PSH_RECV_END,
	NUM_PSH2IA_IPC = PSH_RECV_END - PSH_RECV_CH0,
	NUM_ALL_CH = NUM_IA2PSH_IPC + NUM_PSH2IA_IPC,
};

typedef void(*psh_channel_handle_t)(u32 msg, u32 param, void *data);
int intel_ia2psh_command(struct psh_msg *in, struct psh_msg *out,
			 int ch, int timeout);
int intel_psh_ipc_bind(int ch, psh_channel_handle_t handle, void *data);
void intel_psh_ipc_unbind(int ch);

void intel_psh_ipc_disable_irq(void);
void intel_psh_ipc_enable_irq(void);
#endif
