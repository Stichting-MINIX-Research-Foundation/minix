/* This file contains a simple exception handler.  Exceptions in user
 * processes are converted to signals. Exceptions in a kernel task cause
 * a panic.
 */

#include "kernel/kernel.h"
#include "arch_proto.h"
// #include <signal.h> // Replaced
// #include <string.h> // Removed (memcpy handled by klib)
// #include <assert.h> // Replaced
#include <machine/vm.h> // Kept

// Added kernel headers
#include <minix/kernel_types.h> // For k_sigset_t (not directly used but signal.h was), signal constants
#include <klib/include/kprintf.h>
#include <klib/include/kstring.h>
#include <klib/include/kmemory.h>


struct ex_s {
	char *msg;
	int signum;
	int minprocessor;
};

static struct ex_s ex_data[] = {
	{ "Divide error", SIGFPE, 86 },             // SIGFPE might be undefined
	{ "Debug exception", SIGTRAP, 86 },         // SIGTRAP might be undefined
	{ "Nonmaskable interrupt", SIGBUS, 86 },    // SIGBUS might be undefined
	{ "Breakpoint", SIGEMT, 86 },             // SIGEMT might be undefined
	{ "Overflow", SIGFPE, 86 },             // SIGFPE might be undefined
	{ "Bounds check", SIGFPE, 186 },            // SIGFPE might be undefined
	{ "Invalid opcode", SIGILL, 186 },          // SIGILL might be undefined
	{ "Coprocessor not available", SIGFPE, 186 },// SIGFPE might be undefined
	{ "Double fault", SIGBUS, 286 },            // SIGBUS might be undefined
	{ "Coprocessor segment overrun", SIGSEGV, 286 },// SIGSEGV might be undefined
	{ "Invalid TSS", SIGSEGV, 286 },            // SIGSEGV might be undefined
	{ "Segment not present", SIGSEGV, 286 },    // SIGSEGV might be undefined
	{ "Stack exception", SIGSEGV, 286 },	/* STACK_FAULT already used */ // SIGSEGV
	{ "General protection", SIGSEGV, 286 },     // SIGSEGV might be undefined
	{ "Page fault", SIGSEGV, 386 },		/* not close */ // SIGSEGV
	{ NULL, SIGILL, 0 },			/* probably software trap */ // NULL, SIGILL
	{ "Coprocessor error", SIGFPE, 386 },       // SIGFPE might be undefined
	{ "Alignment check", SIGBUS, 386 },         // SIGBUS might be undefined
	{ "Machine check", SIGBUS, 386 },           // SIGBUS might be undefined
	{ "SIMD exception", SIGFPE, 386 },          // SIGFPE might be undefined
};

static void inkernel_disaster(struct proc *saved_proc,
	struct exception_frame *frame, struct ex_s *ep, int is_nested);

extern int catch_pagefaults;

static void proc_stacktrace_execute(struct proc *whichproc, reg_t v_bp, reg_t pc);

static void pagefault( struct proc *pr,
			struct exception_frame * frame,
			int is_nested)
{
	int in_physcopy = 0, in_memset = 0;

	reg_t pagefaultcr2;
	message m_pagefault;
	int err;

	pagefaultcr2 = read_cr2();

#if 0
	kprintf_stub("kernel: pagefault in pr %d, addr 0x%lx, his cr3 0x%lx, actual cr3 0x%lx\n", // MODIFIED
		pr->p_endpoint, pagefaultcr2, pr->p_seg.p_cr3, read_cr3());
#endif

	in_physcopy = (frame->eip > (vir_bytes) phys_copy) &&
	   (frame->eip < (vir_bytes) phys_copy_fault);

	in_memset = (frame->eip > (vir_bytes) phys_memset) &&
	   (frame->eip < (vir_bytes) memset_fault);

	if((is_nested || iskernelp(pr)) &&
		catch_pagefaults && (in_physcopy || in_memset)) {
#if 0
		kprintf_stub("pf caught! addr 0x%lx\n", pagefaultcr2); // MODIFIED
#endif
		if (is_nested) {
			if(in_physcopy) {
				KASSERT_PLACEHOLDER(!in_memset); // MODIFIED
				frame->eip = (reg_t) phys_copy_fault_in_kernel;
			} else {
				frame->eip = (reg_t) memset_fault_in_kernel;
			}
		}
		else {
			pr->p_reg.pc = (reg_t) phys_copy_fault;
			pr->p_reg.retreg = pagefaultcr2;
		}
	
		return;
	}

	if(is_nested) {
		kprintf_stub("pagefault in kernel at pc 0x%lx address 0x%lx\n", // MODIFIED
			frame->eip, pagefaultcr2);
		inkernel_disaster(pr, frame, NULL, is_nested); // NULL might be undefined
	}

	/* VM can't handle page faults. */
	if(pr->p_endpoint == VM_PROC_NR) {
		/* Page fault we can't / don't want to
		 * handle.
		 */
		kprintf_stub("pagefault for VM on CPU %d, " // MODIFIED
			"pc = 0x%x, addr = 0x%x, flags = 0x%x, is_nested %d\n",
			cpuid, pr->p_reg.pc, pagefaultcr2, frame->errcode,
			is_nested);
		proc_stacktrace(pr);
		kprintf_stub("pc of pagefault: 0x%lx\n", frame->eip); // MODIFIED
		panic("pagefault in VM");

		return;
	}

	/* Don't schedule this process until pagefault is handled. */
	RTS_SET(pr, RTS_PAGEFAULT);

	/* tell Vm about the pagefault */
	m_pagefault.m_source = pr->p_endpoint;
	m_pagefault.m_type   = VM_PAGEFAULT;
	m_pagefault.VPF_ADDR = pagefaultcr2;
	m_pagefault.VPF_FLAGS = frame->errcode;

	if ((err = mini_send(pr, VM_PROC_NR,
					&m_pagefault, FROM_KERNEL))) {
		panic("WARNING: pagefault: mini_send returned %d\n", err);
	}

	return;
}

static void inkernel_disaster(struct proc *saved_proc,
	struct exception_frame * frame, struct ex_s *ep,
	int is_nested)
{
#if USE_SYSDEBUG
  if(ep) {
	if (ep->msg == NULL) // NULL might be undefined
		kprintf_stub("\nIntel-reserved exception %d\n", frame->vector); // MODIFIED
	  else
		kprintf_stub("\n%s\n", ep->msg); // MODIFIED
  }

  kprintf_stub("cpu %d is_nested = %d ", cpuid, is_nested); // MODIFIED

  kprintf_stub("vec_nr= %d, trap_errno= 0x%x, eip= 0x%x, " // MODIFIED
	"cs= 0x%x, eflags= 0x%x trap_esp 0x%08x\n",
	frame->vector, frame->errcode, frame->eip,
	frame->cs, frame->eflags, frame);
  kprintf_stub("KERNEL registers :\n"); // MODIFIED
#define REG(n) (((u32_t *)frame)[-n])
  kprintf_stub( // MODIFIED
		  "\t%%eax 0x%08x %%ebx 0x%08x %%ecx 0x%08x %%edx 0x%08x\n"
		  "\t%%esp 0x%08x %%ebp 0x%08x %%esi 0x%08x %%edi 0x%08x\n",
		  REG(1), REG(2), REG(3), REG(4),
		  REG(5), REG(6), REG(7), REG(8));

  { 
  	reg_t k_ebp = REG(6);
	kprintf_stub("KERNEL stacktrace, starting with ebp = 0x%lx:\n", k_ebp); // MODIFIED
  	proc_stacktrace_execute(proc_addr(SYSTEM), k_ebp, frame->eip);
  }

  if (saved_proc) {
	  kprintf_stub("scheduled was: process %d (%s), ", saved_proc->p_endpoint, saved_proc->p_name); // MODIFIED
	  kprintf_stub("pc = 0x%x\n", (unsigned) saved_proc->p_reg.pc); // MODIFIED
	  proc_stacktrace(saved_proc);

	  panic("Unhandled kernel exception");
  }

  /* in an early stage of boot process we don't have processes yet */
  panic("exception in kernel while booting, no saved_proc yet");
#endif /* USE_SYSDEBUG */
}

/*===========================================================================*
 *				exception				     *
 *===========================================================================*/
void exception_handler(int is_nested, struct exception_frame * frame)
{
/* An exception or unexpected interrupt has occurred. */
  register struct ex_s *ep;
  struct proc *saved_proc;

  /* Save proc_ptr, because it may be changed by debug statements. */
  saved_proc = get_cpulocal_var(proc_ptr);
  
  ep = &ex_data[frame->vector];

  if (frame->vector == 2) {		/* spurious NMI on some machines */
	kprintf_stub("got spurious NMI\n"); // MODIFIED
	return;
  }

  /*
   * handle special cases for nested problems as they might be tricky or filter
   * them out quickly if the traps are not nested
   */
  if (is_nested) {
	/*
	 * if a problem occured while copying a message from userspace because
	 * of a wrong pointer supplied by userland, handle it the only way we
	 * can handle it ...
	 */
	if (((void*)*frame->eip >= (void*)copy_msg_to_user && // MODIFIED eip to frame->eip based on context
			(void*)*frame->eip <= (void*)__copy_msg_to_user_end) ||
			((void*)*frame->eip >= (void*)copy_msg_from_user &&
			(void*)*frame->eip <= (void*)__copy_msg_from_user_end)) {
		switch(frame->vector) {
		/* these error are expected */
		case PAGE_FAULT_VECTOR:
		case PROTECTION_VECTOR:
			frame->eip = (reg_t) __user_copy_msg_pointer_failure;
			return;
		default:
			panic("Copy involving a user pointer failed unexpectedly!");
		}
	}

	/* Pass any error resulting from restoring FPU state, as a FPU
	 * exception to the process.
	 */
	if (((void*)*frame->eip >= (void*)fxrstor && // MODIFIED eip to frame->eip
			(void *)frame->eip <= (void*)__fxrstor_end) ||
			((void*)*frame->eip >= (void*)frstor && // MODIFIED eip to frame->eip
			(void *)frame->eip <= (void*)__frstor_end)) {
		frame->eip = (reg_t) __frstor_failure;
		return;
	}

  	if(frame->vector == DEBUG_VECTOR
		&& (saved_proc->p_reg.psw & TRACEBIT)
		&& (saved_proc->p_seg.p_kern_trap_style == KTS_NONE)) {
		/* Getting a debug trap in the kernel is legitimate
		 * if a traced process entered the kernel using sysenter
		 * or syscall; the trap flag is not cleared then.
		 *
		 * It triggers on the first kernel entry so the trap
		 * style is still KTS_NONE.
		 */

		frame->eflags &= ~TRACEBIT;

		return;

		/* If control passes, this case is not recognized as legitimate
		 * and we panic later on after all.
		 */
	}
  }

  if(frame->vector == PAGE_FAULT_VECTOR) {
	pagefault(saved_proc, frame, is_nested);
	return;
  }

  /* If an exception occurs while running a process, the is_nested variable
   * will be zero. Exceptions in interrupt handlers or system traps will make
   * is_nested non-zero.
   */
  if (is_nested == 0 && ! iskernelp(saved_proc)) {
#if 0
	{

		kprintf_stub( // MODIFIED
  "vec_nr= %d, trap_errno= 0x%lx, eip= 0x%lx, cs= 0x%x, eflags= 0x%lx\n",
			frame->vector, (unsigned long)frame->errcode,
			(unsigned long)frame->eip, frame->cs,
			(unsigned long)frame->eflags);
		proc_stacktrace(saved_proc);
	}

#endif
	cause_sig(proc_nr(saved_proc), ep->signum);
	return;
  }

  /* Exception in system code. This is not supposed to happen. */
  inkernel_disaster(saved_proc, frame, ep, is_nested);

  panic("return from inkernel_disaster");
}

#if USE_SYSDEBUG
/*===========================================================================*
 *				proc_stacktrace_execute			     *
 *===========================================================================*/
static void proc_stacktrace_execute(struct proc *whichproc, reg_t v_bp, reg_t pc)
{
	reg_t v_hbp;
	int iskernel;
	int n = 0;

	iskernel = iskernelp(whichproc);

	kprintf_stub("%-8.8s %6d 0x%lx ", // MODIFIED
		whichproc->p_name, whichproc->p_endpoint, pc);

	while(v_bp) {
		reg_t v_pc;

#define PRCOPY(pr, pv, v, n) \
  (iskernel ? (kmemcpy((char *) v, (char *) pv, n), OK) : \
     data_copy(pr->p_endpoint, pv, KERNEL, (vir_bytes) (v), n))

	        if(PRCOPY(whichproc, v_bp, &v_hbp, sizeof(v_hbp)) != OK) {
			kprintf_stub("(v_bp 0x%lx ?)", v_bp); // MODIFIED
			break;
		}
		if(PRCOPY(whichproc, v_bp + sizeof(v_pc), &v_pc, sizeof(v_pc)) != OK) {
			kprintf_stub("(v_pc 0x%lx ?)", v_bp + sizeof(v_pc)); // MODIFIED
			break;
		}
		kprintf_stub("0x%lx ", (unsigned long) v_pc); // MODIFIED
		if(v_hbp != 0 && v_hbp <= v_bp) {
			kprintf_stub("(hbp 0x%lx ?)", v_hbp); // MODIFIED
			break;
		}
		v_bp = v_hbp;
		if(n++ > 50) {
			kprintf_stub("(truncated after %d steps) ", n); // MODIFIED
			break;
		}
	}
	kprintf_stub("\n"); // MODIFIED
}
#endif /* USE_SYSDEBUG */

/*===========================================================================*
 *				proc_stacktrace			     *
 *===========================================================================*/
void proc_stacktrace(struct proc *whichproc)
{
	u32_t use_bp;

	if(whichproc->p_seg.p_kern_trap_style == KTS_NONE) {
		kprintf_stub("WARNING: stacktrace of running process\n"); // MODIFIED
	}

	switch(whichproc->p_seg.p_kern_trap_style) {
		case KTS_SYSENTER:
		case KTS_SYSCALL:
		{
			u32_t sp = whichproc->p_reg.sp;

			/* Full context is not available in the p_reg
			 * struct. Obtain it from the user's stack.
			 * The use stack pointer is always available.
			 * The fact that it's there, and the 16 byte offset,
			 * is a dependency on the trap code in
			 * kernel/arch/i386/usermapped_glo_ipc.S.
			 */

			if(data_copy(whichproc->p_endpoint, sp+16,
			  KERNEL, (vir_bytes) &use_bp,
				sizeof(use_bp)) != OK) {
				kprintf_stub("stacktrace: aborting, copy failed\n"); // MODIFIED
				return;
			}

			break;
		}
		default:
			/* Full context is available; use the stored ebp */
			use_bp = whichproc->p_reg.fp;
			break;
	}

#if USE_SYSDEBUG
	proc_stacktrace_execute(whichproc, use_bp, whichproc->p_reg.pc);
#endif /* USE_SYSDEBUG */
}

void enable_fpu_exception(void)
{
	u32_t cr0 = read_cr0();
	if(!(cr0 & I386_CR0_TS))
		write_cr0(cr0 | I386_CR0_TS);
}

void disable_fpu_exception(void)
{
	clts();
}
