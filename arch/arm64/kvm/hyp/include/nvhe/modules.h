#include <asm/kvm_pgtable.h>
#include <linux/kvm_host.h>
#include <linux/arm-smccc.h>

#define HCALL_HANDLED 0
#define HCALL_UNHANDLED -1

int __pkvm_register_host_smc_handler(bool (*cb)(struct user_pt_regs *));
int __pkvm_register_guest_smc_handler(bool (*cb)(struct arm_smccc_1_2_regs *,
						 struct arm_smccc_res *res,
						 pkvm_handle_t handle),
				      pkvm_handle_t handle);
int __pkvm_register_default_trap_handler(bool (*cb)(struct user_pt_regs *));
int __pkvm_register_illegal_abt_notifier(void (*cb)(struct user_pt_regs *));
int __pkvm_register_hyp_panic_notifier(void (*cb)(struct user_pt_regs *));
int __pkvm_register_unmask_serror(bool (*unmask)(void), void (*mask)(void));

enum pkvm_psci_notification;
int __pkvm_register_psci_notifier(void (*cb)(enum pkvm_psci_notification, struct user_pt_regs *));

#ifdef CONFIG_MODULES
int __pkvm_init_module(void *module_init);
int __pkvm_register_hcall(unsigned long hfn_hyp_va);
int handle_host_dynamic_hcall(struct user_pt_regs *regs, int id);
void __pkvm_close_module_registration(void);
#else
static inline int __pkvm_init_module(void *module_init) { return -EOPNOTSUPP; }
static inline int
__pkvm_register_hcall(unsigned long hfn_hyp_va) { return -EOPNOTSUPP; }
static inline int
handle_host_dynamic_hcall(struct kvm_cpu_context *host_ctxt, int id)
{
	return HCALL_UNHANDLED;
}
static inline void __pkvm_close_module_registration(void) { }
#endif
