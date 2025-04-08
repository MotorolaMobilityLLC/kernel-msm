// SPDX-License-Identifier: GPL-2.0
/*
 * arch/arm64/kvm/fpsimd.c: Guest/host FPSIMD context coordination helpers
 *
 * Copyright 2018 Arm Limited
 * Author: Dave Martin <Dave.Martin@arm.com>
 */
#include <linux/irqflags.h>
#include <linux/sched.h>
#include <linux/thread_info.h>
#include <linux/kvm_host.h>
#include <asm/fpsimd.h>
#include <asm/kvm_asm.h>
#include <asm/kvm_hyp.h>
#include <asm/kvm_mmu.h>
#include <asm/sysreg.h>

/*
 * Called on entry to KVM_RUN unless this vcpu previously ran at least
 * once and the most recent prior KVM_RUN for this vcpu was called from
 * the same task as current (highly likely).
 *
 * This is guaranteed to execute before kvm_arch_vcpu_load_fp(vcpu),
 * such that on entering hyp the relevant parts of current are already
 * mapped.
 */
int kvm_arch_vcpu_run_map_fp(struct kvm_vcpu *vcpu)
{
	int ret;

	struct thread_info *ti = &current->thread_info;
	struct user_fpsimd_state *fpsimd = &current->thread.uw.fpsimd_state;

	/*
	 * Make sure the host task thread flags and fpsimd state are
	 * visible to hyp:
	 */
	ret = create_hyp_mappings(ti, ti + 1, PAGE_HYP);
	if (ret)
		goto error;

	ret = create_hyp_mappings(fpsimd, fpsimd + 1, PAGE_HYP);
	if (ret)
		goto error;

	if (vcpu->arch.sve_state) {
		void *sve_end;

		sve_end = vcpu->arch.sve_state + vcpu_sve_state_size(vcpu);

		ret = create_hyp_mappings(vcpu->arch.sve_state, sve_end,
					  PAGE_HYP);
		if (ret)
			goto error;
	}

	vcpu->arch.host_thread_info = kern_hyp_va(ti);
error:
	return ret;
}

/*
 * Prepare vcpu for saving the host's FPSIMD state and loading the guest's.
 * The actual loading is done by the FPSIMD access trap taken to hyp.
 *
 * Here, we just set the correct metadata to indicate that the FPSIMD
 * state in the cpu regs (if any) belongs to current on the host.
 */
void kvm_arch_vcpu_load_fp(struct kvm_vcpu *vcpu)
{
	BUG_ON(!current->mm);

	vcpu->arch.flags &= ~KVM_ARM64_FP_ENABLED;
	vcpu->arch.flags |= KVM_ARM64_FP_HOST;

	/*
	 * Ensure that any host FPSIMD/SVE/SME state is saved and unbound such
	 * that the host kernel is responsible for restoring this state upon
	 * return to userspace, and the hyp code doesn't need to save anything.
	 *
	 * When the host may use SME, fpsimd_save_and_flush_cpu_state() ensures
	 * that PSTATE.{SM,ZA} == {0,0}.
	 */
	fpsimd_save_and_flush_cpu_state();
	vcpu->arch.flags &= ~KVM_ARM64_FP_HOST;
}

/*
 * If the guest FPSIMD state was loaded, update the host's context
 * tracking data mark the CPU FPSIMD regs as dirty and belonging to vcpu
 * so that they will be written back if the kernel clobbers them due to
 * kernel-mode NEON before re-entry into the guest.
 */
void kvm_arch_vcpu_ctxsync_fp(struct kvm_vcpu *vcpu)
{
	enum fp_type fp_type;

	WARN_ON_ONCE(!irqs_disabled());

	if (vcpu->arch.flags & KVM_ARM64_FP_ENABLED) {
		if (vcpu_has_sve(vcpu))
			fp_type = FP_STATE_SVE;
		else
			fp_type = FP_STATE_FPSIMD;

		/*
		 * Currently we do not support SME guests so SVCR is
		 * always 0 and we just need a variable to point to.
		 */
		fpsimd_bind_state_to_cpu(&vcpu->arch.ctxt.fp_regs,
					 vcpu->arch.sve_state,
					 vcpu->arch.sve_max_vl,
					 &vcpu->arch.fp_type, fp_type);

		clear_thread_flag(TIF_FOREIGN_FPSTATE);
	}
}

/*
 * Write back the vcpu FPSIMD regs if they are dirty, and invalidate the
 * cpu FPSIMD regs so that they can't be spuriously reused if this vcpu
 * disappears and another task or vcpu appears that recycles the same
 * struct fpsimd_state.
 */
void kvm_arch_vcpu_put_fp(struct kvm_vcpu *vcpu)
{
	unsigned long flags;

	local_irq_save(flags);

	if (vcpu->arch.flags & KVM_ARM64_FP_ENABLED) {
		/*
		 * Flush (save and invalidate) the fpsimd/sve state so that if
		 * the host tries to use fpsimd/sve, it's not using stale data
		 * from the guest.
		 *
		 * Flushing the state sets the TIF_FOREIGN_FPSTATE bit for the
		 * context unconditionally, in both nVHE and VHE. This allows
		 * the kernel to restore the fpsimd/sve state, including ZCR_EL1
		 * when needed.
		 */
		fpsimd_save_and_flush_cpu_state();
	}

	local_irq_restore(flags);
}
