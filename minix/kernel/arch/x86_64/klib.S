/* sections */

#include <minix/config.h>
#include <minix/const.h>
#include <machine/asm.h>
#include <machine/interrupt.h>
#include <machine/vm.h>
#include "archconst.h"
#include "kernel/const.h"
#include "sconst.h"
/*
 * x86_64 (AMD64 SysV ABI) kernel library routines.
 * Integer arguments: rdi, rsi, rdx, rcx, r8, r9.  Return value: rax.
 * Callee-saves: rbx, rbp, r12–r15.
 */

ENTRY(__main)
	ret

/*===========================================================================*/
/*				phys_insw / phys_insb			     */
/*===========================================================================*/
/* void phys_insw(Port_t port, phys_bytes buf, size_t count)
 * rdi=port(u16), rsi=buf, rdx=count */
ENTRY(phys_insw)
	movq	%rdx, %rcx		/* byte count → rcx */
	movzwl	%di,  %edx		/* port → dx */
	movq	%rsi, %rdi		/* destination buffer */
	shrq	$1,   %rcx		/* word count */
	cld
	rep insw
	ret

/* void phys_insb(Port_t port, phys_bytes buf, size_t count)
 * rdi=port(u16), rsi=buf, rdx=count */
ENTRY(phys_insb)
	movq	%rdx, %rcx
	movzwl	%di,  %edx
	movq	%rsi, %rdi
	cld
	rep insb
	ret

/*===========================================================================*/
/*				phys_outsw / phys_outsb			     */
/*===========================================================================*/
/* void phys_outsw(Port_t port, phys_bytes buf, size_t count)
 * rdi=port(u16), rsi=buf, rdx=count */
ENTRY(phys_outsw)
	movq	%rdx, %rcx		/* byte count → rcx */
	movzwl	%di,  %edx		/* port → dx */
	shrq	$1,   %rcx		/* word count */
	cld
	rep outsw			/* dx=port, rsi=source */
	ret

/* void phys_outsb(Port_t port, phys_bytes buf, size_t count)
 * rdi=port(u16), rsi=buf, rdx=count */
ENTRY(phys_outsb)
	movq	%rdx, %rcx
	movzwl	%di,  %edx
	cld
	rep outsb
	ret

/*===========================================================================*/
/*				phys_copy				     */
/*===========================================================================*/
/* phys_bytes phys_copy(phys_bytes source, phys_bytes destination,
 *                      phys_bytes bytecount);
 * rdi=source, rsi=destination, rdx=bytecount
 * rep movsb uses rsi=source, rdi=destination — swap the first two args. */
ENTRY(phys_copy)
	movq	%rdx, %rcx
	xchgq	%rdi, %rsi		/* rdi=destination, rsi=source */
	cld
	rep movsb
	xorq	%rax, %rax		/* 0 = no fault */
LABEL(phys_copy_fault)
	ret

LABEL(phys_copy_fault_in_kernel)
	movq	%cr2, %rax
	ret

/*===========================================================================*/
/*			copy_msg_from_user / copy_msg_to_user		     */
/*===========================================================================*/
/* int copy_msg_from_user(message *user_mbuf, message *dst)
 * rdi=user_mbuf, rsi=dst — skip m_source at offset 0 */
ENTRY(copy_msg_from_user)
	movl	 4(%rdi), %eax;	movl	%eax,  4(%rsi)
	movq	 8(%rdi), %rax;	movq	%rax,  8(%rsi)
	movq	16(%rdi), %rax;	movq	%rax, 16(%rsi)
	movq	24(%rdi), %rax;	movq	%rax, 24(%rsi)
	movq	32(%rdi), %rax;	movq	%rax, 32(%rsi)
	movq	40(%rdi), %rax;	movq	%rax, 40(%rsi)
	movq	48(%rdi), %rax;	movq	%rax, 48(%rsi)
	movq	56(%rdi), %rax;	movq	%rax, 56(%rsi)
	movq	64(%rdi), %rax;	movq	%rax, 64(%rsi)
	movq	72(%rdi), %rax;	movq	%rax, 72(%rsi)
	movq	80(%rdi), %rax;	movq	%rax, 80(%rsi)
	movq	88(%rdi), %rax;	movq	%rax, 88(%rsi)
LABEL(__copy_msg_from_user_end)
	xorl	%eax, %eax
	ret

/* void copy_msg_to_user(message *src, message *user_mbuf)
 * rdi=src, rsi=user_mbuf — copies all 96 bytes */
ENTRY(copy_msg_to_user)
	movq	 0(%rdi), %rax;	movq	%rax,  0(%rsi)
	movq	 8(%rdi), %rax;	movq	%rax,  8(%rsi)
	movq	16(%rdi), %rax;	movq	%rax, 16(%rsi)
	movq	24(%rdi), %rax;	movq	%rax, 24(%rsi)
	movq	32(%rdi), %rax;	movq	%rax, 32(%rsi)
	movq	40(%rdi), %rax;	movq	%rax, 40(%rsi)
	movq	48(%rdi), %rax;	movq	%rax, 48(%rsi)
	movq	56(%rdi), %rax;	movq	%rax, 56(%rsi)
	movq	64(%rdi), %rax;	movq	%rax, 64(%rsi)
	movq	72(%rdi), %rax;	movq	%rax, 72(%rsi)
	movq	80(%rdi), %rax;	movq	%rax, 80(%rsi)
	movq	88(%rdi), %rax;	movq	%rax, 88(%rsi)
LABEL(__copy_msg_to_user_end)
	xorl	%eax, %eax
	ret

ENTRY(__user_copy_msg_pointer_failure)
	movq	$-1, %rax
	ret

/*===========================================================================*/
/*				phys_memset				     */
/*===========================================================================*/
/* phys_bytes phys_memset(phys_bytes dst, u32_t pattern, phys_bytes count)
 * rdi=dst, esi=pattern, rdx=count */
ENTRY(phys_memset)
	movl	%esi, %eax		/* low 32 bits of pattern */
	movq	%rax, %rcx
	shlq	$32, %rcx
	orq	%rcx, %rax		/* rax = pattern replicated in both halves */
	movq	%rdx, %rcx		/* byte count */
	cld
	shrq	$3, %rcx		/* qword count */
	rep stosq			/* rdi=dst, rax=val, rcx=count */
	movq	%rdx, %rcx
	andq	$7, %rcx		/* remaining bytes */
	rep stosb
	xorq	%rax, %rax		/* 0 = no fault */
LABEL(memset_fault)
	ret

LABEL(memset_fault_in_kernel)
	movq	%cr2, %rax
	ret

/*===========================================================================*/
/*				x86_triplefault				     */
/*===========================================================================*/
ENTRY(x86_triplefault)
	lidtq	idt_zero(%rip)
	int	$3
.data
idt_zero:
	.quad	0
	.quad	0
.text

/*===========================================================================*/
/*				halt_cpu				     */
/*===========================================================================*/
ENTRY(halt_cpu)
	sti
	hlt
	ret

/*===========================================================================*/
/*			poweroff_vmware_clihlt				     */
/*===========================================================================*/
ENTRY(poweroff_vmware_clihlt)
	pushq	%rbx
#ifndef NO_VMWARE_DETECTION
	movl	$1, %eax
	cpuid
	testl	$(1<<31), %ecx
	jz	1f
	movl	$0x40000000, %eax
	cpuid
	cmpl	$0x61774D56, %ebx
	jne	1f
	cmpl	$0x4D566572, %ecx
	jne	1f
	cmpl	$0x65726177, %edx
	jne	1f
#endif
	cli
	hlt
1:	popq	%rbx
	ret

/*===========================================================================*/
/*				read_cpu_flags				     */
/*===========================================================================*/
ENTRY(read_cpu_flags)
	pushfq
	popq	%rax
	ret

ENTRY(read_ds)
	xorq	%rax, %rax
	movw	%ds, %ax
	ret

ENTRY(read_cs)
	xorq	%rax, %rax
	movw	%cs, %ax
	ret

ENTRY(read_ss)
	xorq	%rax, %rax
	movw	%ss, %ax
	ret

/*===========================================================================*/
/*				FPU routines				     */
/*===========================================================================*/
ENTRY(fninit)
	fninit
	ret

ENTRY(clts)
	clts
	ret

ENTRY(fnstsw)
	xorq	%rax, %rax
	fnstsw	%ax
	ret

/* int fxrstor(void *state)  rdi=state */
ENTRY(fxrstor)
	fxrstor	(%rdi)
ENTRY(__fxrstor_end)
	xorl	%eax, %eax
	ret

/* int frstor(void *state)   rdi=state */
ENTRY(frstor)
	frstor	(%rdi)
ENTRY(__frstor_end)
	xorl	%eax, %eax
	ret

ENTRY(__frstor_failure)
	movl	$1, %eax
	ret

/* void fxsave(void *state)  rdi=state */
ENTRY(fxsave)
	fxsave	(%rdi)
	ret

/* void fnsave(void *state)  rdi=state */
ENTRY(fnsave)
	fnsave	(%rdi)
	fwait
	ret

/* void fnstcw(unsigned short *cw)  rdi=cw */
ENTRY(fnstcw)
	fnstcw	(%rdi)
	ret

/*===========================================================================*/
/*			Control register read/write			     */
/*===========================================================================*/
ENTRY(read_cr0)
	movq	%cr0, %rax
	ret

ENTRY(read_cr2)
	movq	%cr2, %rax
	ret

ENTRY(read_cr3)
	movq	%cr3, %rax
	ret

ENTRY(read_cr4)
	movq	%cr4, %rax
	ret

/* write_cr0(unsigned long value)  rdi=value */
ENTRY(write_cr0)
	movq	%rdi, %cr0
	ret

/* write_cr3(unsigned long value)  rdi=value */
ENTRY(write_cr3)
	movq	%rdi, %cr3
	ret

/* write_cr4(unsigned long value)  rdi=value */
ENTRY(write_cr4)
	movq	%rdi, %cr4
	ret

/* void i386_invlpg(phys_bytes linaddr)  rdi=linaddr */
ENTRY(i386_invlpg)
	invlpg	(%rdi)
	ret

/*===========================================================================*/
/*			Descriptor table loading			     */
/*===========================================================================*/
/* void x86_lgdt(void *p)   rdi=p */
ENTRY(x86_lgdt)
	lgdtq	(%rdi)
	ret

/* void x86_lidt(void *p)   rdi=p */
ENTRY(x86_lidt)
	lidtq	(%rdi)
	ret

/* void x86_lldt(u32_t sel) rdi=sel */
ENTRY(x86_lldt)
	lldt	%di
	ret

/* void x86_ltr(u32_t sel)  rdi=sel */
ENTRY(x86_ltr)
	ltr	%di
	ret

/* void x86_sgdt(void *p)   rdi=p */
ENTRY(x86_sgdt)
	sgdt	(%rdi)
	ret

/* void x86_sidt(void *p)   rdi=p */
ENTRY(x86_sidt)
	sidt	(%rdi)
	ret

/*===========================================================================*/
/*			Segment register loads				     */
/*===========================================================================*/
ENTRY(x86_load_ds)
	movw	%di, %ds
	ret

ENTRY(x86_load_es)
	movw	%di, %es
	ret

ENTRY(x86_load_fs)
	movw	%di, %fs
	ret

ENTRY(x86_load_gs)
	movw	%di, %gs
	ret

ENTRY(x86_load_ss)
	movw	%di, %ss
	ret

/*===========================================================================*/
/*			x86_load_kerncs				     */
/*===========================================================================*/
/*
 * In 64-bit mode there is no direct far-jump encoding for a segment reload.
 * Use the far-return trick: push CS selector and return address, then lretq.
 */
ENTRY(x86_load_kerncs)
	leaq	.Lkerncs_back(%rip), %rax
	pushq	$KERN_CS_SELECTOR
	pushq	%rax
	lretq
.Lkerncs_back:
	ret

/*===========================================================================*/
/*			ia32_msr_read / ia32_msr_write			     */
/*===========================================================================*/
/* void ia32_msr_read(u32_t reg, u32_t *hi, u32_t *lo)
 * rdi=reg, rsi=hi, rdx=lo */
ENTRY(ia32_msr_read)
	movq	%rdx, %r8		/* save lo ptr (rdx will be clobbered by rdmsr) */
	movl	%edi, %ecx		/* MSR number */
	rdmsr				/* edx:eax = MSR value */
	movl	%edx, (%rsi)		/* *hi = edx */
	movl	%eax, (%r8)		/* *lo = eax */
	ret

/* void ia32_msr_write(u32_t reg, u32_t hi, u32_t lo)
 * rdi=reg, rsi=hi, rdx=lo */
ENTRY(ia32_msr_write)
	movl	%edx, %eax		/* lo → eax (before overwriting rdx) */
	movl	%esi, %edx		/* hi → edx */
	movl	%edi, %ecx		/* reg → ecx */
	wrmsr
	ret

/*===========================================================================*/
/*			__switch_address_space				     */
/*===========================================================================*/
/* void __switch_address_space(struct proc *p, struct proc **ptproc)
 * rdi=p, rsi=ptproc */
ENTRY(__switch_address_space)
	movq	P_CR3(%rdi), %rax	/* new CR3 value */
	testq	%rax, %rax
	jz	0f
	movq	%cr3, %rcx
	cmpq	%rcx, %rax
	je	0f
	movq	%rax, %cr3
	movq	%rdi, (%rsi)		/* *ptproc = p */
0:
	ret

/*===========================================================================*/
/*				eoi_8259_master / slave			     */
/*===========================================================================*/
ENTRY(eoi_8259_master)
	movb	$END_OF_INT, %al
	outb	$INT_CTL
	ret

ENTRY(eoi_8259_slave)
	movb	$END_OF_INT, %al
	outb	$INT_CTL
	outb	$INT2_CTL
	ret

/*===========================================================================*/
/*				refresh_tlb				     */
/*===========================================================================*/
ENTRY(refresh_tlb)
	movq	%cr3, %rax
	movq	%rax, %cr3
	ret

/*===========================================================================*/
/*				mfence / arch_pause			     */
/*===========================================================================*/
ENTRY(mfence)
	mfence
	ret

ENTRY(arch_pause)
	pause
	ret

/*===========================================================================*/
/*				read_ebp (read_rbp)			     */
/*===========================================================================*/
ENTRY(read_ebp)
	movq	%rbp, %rax
	ret

ENTRY(interrupts_enable)
	sti
	ret

ENTRY(interrupts_disable)
	cli
	ret

ENTRY(reload_ds)
	movw	$KERN_DS_SELECTOR, %ax
	movw	%ax, %ds
	ret

/*===========================================================================*/
/*				switch_k_stack				     */
/*===========================================================================*/
/* void switch_k_stack(void *new_sp, void (*continuation)(void))
 * rdi=new_sp, rsi=continuation */
ENTRY(switch_k_stack)
	movq	%rdi, %rsp		/* switch to new stack */
	xorq	%rbp, %rbp		/* reset frame pointer */
	jmpq	*%rsi			/* jump to continuation */
	/* NOT_REACHABLE */

#ifdef CONFIG_SMP

/*===========================================================================*/
/*			smp_get_htt / smp_get_num_htt / smp_get_cores	     */
/*===========================================================================*/
/* On x86_64 CPUID is always supported; no EFLAGS.ID check needed. */

/* u32_t smp_get_htt(void) — returns EDX from CPUID(1) (HTT bit = bit 28) */
ENTRY(smp_get_htt)
	pushq	%rbx			/* cpuid clobbers rbx; rbx is callee-saved */
	movl	$0x1, %eax
	cpuid
	movl	%edx, %eax
	popq	%rbx
	ret

/* u32_t smp_get_num_htt(void) — returns EBX[23:16] from CPUID(1) */
ENTRY(smp_get_num_htt)
	pushq	%rbx
	movl	$0x1, %eax
	cpuid
	movl	%ebx, %eax
	popq	%rbx
	ret

/* u32_t smp_get_cores(void) — returns EAX from CPUID(4, ecx=0) */
ENTRY(smp_get_cores)
	pushq	%rbx
	xorl	%ecx, %ecx
	movl	$0x4, %eax
	cpuid
	popq	%rbx
	ret

/*===========================================================================*/
/*			arch_spinlock_lock / arch_spinlock_unlock	     */
/*===========================================================================*/
/* void arch_spinlock_lock(u32_t *lock_data)   rdi=lock_data */
ENTRY(arch_spinlock_lock)
	movq	$1, %rdx
.Lspin_retry:
	movl	$1, %ecx
	xchgl	%ecx, (%rdi)
	testl	%ecx, %ecx
	je	.Lspin_done

	cmpq	$(1 << 16), %rdx
	je	.Lspin_backoff
	shlq	$1, %rdx
.Lspin_backoff:
	movq	%rdx, %rcx
.Lspin_pause:
	pause
	subq	$1, %rcx
	testq	%rcx, %rcx
	jz	.Lspin_retry
	jmp	.Lspin_pause
.Lspin_done:
	mfence
	ret

/* void arch_spinlock_unlock(u32_t *lock_data)  rdi=lock_data */
ENTRY(arch_spinlock_unlock)
	xorl	%ecx, %ecx
	xchgl	%ecx, (%rdi)
	mfence
	ret

#endif /* CONFIG_SMP */
