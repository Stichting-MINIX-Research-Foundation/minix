/*
 * MXV Phase-1 VM-entry trampoline.
 *
 * The only guest body used by this phase is mxv_vmx_probe_guest below.  It is
 * kernel-owned and contains no memory accesses or stack operations.
 */
	.text
	.p2align 4
	.globl	mxv_vmx_enter
	.type	mxv_vmx_enter,@function
mxv_vmx_enter:
	/* Preserve every host GPR: VMX does not save general-purpose registers. */
	pushq	%rax
	pushq	%rcx
	pushq	%rdx
	pushq	%rbx
	pushq	%rbp
	pushq	%rsi
	pushq	%rdi
	pushq	%r8
	pushq	%r9
	pushq	%r10
	pushq	%r11
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15

	/*
	 * After the pushes, the saved vCPU pointer is at 64(%rsp), and rsp is
	 * 16-byte aligned for the C ABI.  VM exit restores this exact host stack
	 * and resumes at .Lmxv_vmexit.
	 */
	movq	64(%rsp), %rdi
	movq	%rsp, %rsi
	leaq	.Lmxv_vmexit(%rip), %rdx
	call	mxv_vmx_prepare_entry
	testl	%eax, %eax
	jnz	.Lmxv_store_result

	vmlaunch
	/* A successful VMLAUNCH never falls through; VMfail does. */
	movq	64(%rsp), %rdi
	call	mxv_vmx_entry_failed
	jmp	.Lmxv_store_result

.Lmxv_vmexit:
	movq	64(%rsp), %rdi
	call	mxv_vmx_exit_complete

.Lmxv_store_result:
	/* Replace the saved RAX slot with the C result before restoring GPRs. */
	movl	%eax, 112(%rsp)
	popq	%r15
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%r11
	popq	%r10
	popq	%r9
	popq	%r8
	popq	%rdi
	popq	%rsi
	popq	%rbp
	popq	%rbx
	popq	%rdx
	popq	%rcx
	popq	%rax
	ret
	.size	mxv_vmx_enter, .-mxv_vmx_enter

	.p2align 4
	.globl	mxv_vmx_probe_guest
	.type	mxv_vmx_probe_guest,@function
mxv_vmx_probe_guest:
	movl	$0x4d585631, %eax	/* "MXV1" marker for a debugger */
	vmcall
	ud2				/* VMCALL must have exited to the host */
	.size	mxv_vmx_probe_guest, .-mxv_vmx_probe_guest

	.section .note.GNU-stack,"",@progbits
