/* This file contains a simple exception handler.  Exceptions in user
 * processes are converted to signals. Exceptions in a kernel task cause
 * a panic.
 */

#include "kernel/kernel.h"
#include "proto.h"
#include <signal.h>
#include <string.h>
#include <assert.h>
#include "kernel/proc.h"
#include "kernel/proto.h"
#include <machine/vm.h>

extern int catch_pagefaults;

PRIVATE void pagefault( struct proc *pr,
			struct exception_frame * frame,
			int is_nested)
{
	int in_physcopy = 0;

	reg_t pagefaultcr2;
	message m_pagefault;
	int err;

	assert(frame);

	pagefaultcr2 = read_cr2();

#if 0
	printf("kernel: pagefault in pr %d, addr 0x%lx, his cr3 0x%lx, actual cr3 0x%lx\n",
		pr->p_endpoint, pagefaultcr2, pr->p_seg.p_cr3, read_cr3());
#endif

	if(pr->p_seg.p_cr3) {
		assert(pr->p_seg.p_cr3 == read_cr3());
	}

	in_physcopy = (frame->eip > (vir_bytes) phys_copy) &&
	   (frame->eip < (vir_bytes) phys_copy_fault);

	if((is_nested || iskernelp(pr)) &&
		catch_pagefaults && in_physcopy) {
#if 0
		printf("pf caught! addr 0x%lx\n", pagefaultcr2);
#endif
		if (is_nested) {
			frame->eip = (reg_t) phys_copy_fault_in_kernel;
		}
		else {
			pr->p_reg.pc = (reg_t) phys_copy_fault;
			pr->p_reg.retreg = pagefaultcr2;
		}
	
		return;
	}

	if(is_nested) {
		panic("pagefault in kernel at pc 0x%lx address 0x%lx", frame->eip, pagefaultcr2);
	}

	/* System processes that don't have their own page table can't
	 * have page faults. VM does have its own page table but also
	 * can't have page faults (because VM has to handle them).
	 */
	if((pr->p_endpoint <= INIT_PROC_NR &&
	 !(pr->p_misc_flags & MF_FULLVM)) || pr->p_endpoint == VM_PROC_NR) {
		/* Page fault we can't / don't want to
		 * handle.
		 */
		printf("pagefault for process %d ('%s'), pc = 0x%x, addr = 0x%x, flags = 0x%x, is_nested %d\n",
			pr->p_endpoint, pr->p_name, pr->p_reg.pc,
			pagefaultcr2, frame->errcode, is_nested);
		proc_stacktrace(pr);
		printf("pc of pagefault: 0x%lx\n", frame->eip);
		cause_sig(proc_nr(pr), SIGSEGV);

		return;
	}

	/* Don't schedule this process until pagefault is handled. */
	assert(pr->p_seg.p_cr3 == read_cr3());
	assert(!RTS_ISSET(pr, RTS_PAGEFAULT));
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

/*===========================================================================*
 *				exception				     *
 *===========================================================================*/
PUBLIC void exception_handler(int is_nested, struct exception_frame * frame)
{
/* An exception or unexpected interrupt has occurred. */

  struct ex_s {
	char *msg;
	int signum;
	int minprocessor;
  };
  static struct ex_s ex_data[] = {
	{ "Divide error", SIGFPE, 86 },
	{ "Debug exception", SIGTRAP, 86 },
	{ "Nonmaskable interrupt", SIGBUS, 86 },
	{ "Breakpoint", SIGEMT, 86 },
	{ "Overflow", SIGFPE, 86 },
	{ "Bounds check", SIGFPE, 186 },
	{ "Invalid opcode", SIGILL, 186 },
	{ "Coprocessor not available", SIGFPE, 186 },
	{ "Double fault", SIGBUS, 286 },
	{ "Coprocessor segment overrun", SIGSEGV, 286 },
	{ "Invalid TSS", SIGSEGV, 286 },
	{ "Segment not present", SIGSEGV, 286 },
	{ "Stack exception", SIGSEGV, 286 },	/* STACK_FAULT already used */
	{ "General protection", SIGSEGV, 286 },
	{ "Page fault", SIGSEGV, 386 },		/* not close */
	{ NULL, SIGILL, 0 },			/* probably software trap */
	{ "Coprocessor error", SIGFPE, 386 },
	{ "Alignment check", SIGBUS, 386 },
	{ "Machine check", SIGBUS, 386 },
	{ "SIMD exception", SIGFPE, 386 },
  };
  register struct ex_s *ep;
  struct proc *saved_proc;

  /* Save proc_ptr, because it may be changed by debug statements. */
  saved_proc = proc_ptr;	
  
  ep = &ex_data[frame->vector];

  if (frame->vector == 2) {		/* spurious NMI on some machines */
	printf("got spurious NMI\n");
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
	if (((void*)frame->eip >= (void*)copy_msg_to_user &&
			(void*)frame->eip <= (void*)__copy_msg_to_user_end) ||
			((void*)frame->eip >= (void*)copy_msg_from_user &&
			(void*)frame->eip <= (void*)__copy_msg_from_user_end)) {
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

  		printf(
  "vec_nr= %d, trap_errno= 0x%lx, eip= 0x%lx, cs= 0x%x, eflags= 0x%lx\n",
			frame->vector, (unsigned long)frame->errcode,
			(unsigned long)frame->eip, frame->cs,
			(unsigned long)frame->eflags);
		printseg("cs: ", 1, saved_proc, frame->cs);
		printseg("ds: ", 0, saved_proc, saved_proc->p_reg.ds);
		if(saved_proc->p_reg.ds != saved_proc->p_reg.ss) {
			printseg("ss: ", 0, saved_proc, saved_proc->p_reg.ss);
		}
		proc_stacktrace(saved_proc);
	}

#endif
	cause_sig(proc_nr(saved_proc), ep->signum);
	return;
  }

  /* Exception in system code. This is not supposed to happen. */
  if (ep->msg == NULL || machine.processor < ep->minprocessor)
	printf("\nIntel-reserved exception %d\n", frame->vector);
  else
	printf("\n%s\n", ep->msg);
  printf("is_nested = %d ", is_nested);

  printf("vec_nr= %d, trap_errno= 0x%x, eip= 0x%x, "
	"cs= 0x%x, eflags= 0x%x trap_esp 0x%08x\n",
	frame->vector, frame->errcode, frame->eip,
	frame->cs, frame->eflags, frame);
  printf("KERNEL registers :\n");
  printf(
		  "\t%%eax 0x%08x %%ebx 0x%08x %%ecx 0x%08x %%edx 0x%08x\n"
		  "\t%%esp 0x%08x %%ebp 0x%08x %%esi 0x%08x %%edi 0x%08x\n",
		  ((u32_t *)frame)[-1],
		  ((u32_t *)frame)[-2],
		  ((u32_t *)frame)[-3],
		  ((u32_t *)frame)[-4],
		  ((u32_t *)frame)[-5],
		  ((u32_t *)frame)[-6],
		  ((u32_t *)frame)[-7],
		  ((u32_t *)frame)[-8]
	 );
  printseg("ker cs: ", 1, NULL, frame->cs);
  printseg("ker ds: ", 0, NULL, DS_SELECTOR);
  /* TODO should we enable this only when compiled for some debug mode? */
  if (saved_proc) {
	  printf("scheduled was: process %d (%s), ", proc_nr(saved_proc), saved_proc->p_name);
	  printf("pc = %u:0x%x\n", (unsigned) saved_proc->p_reg.cs,
			  (unsigned) saved_proc->p_reg.pc);
	  proc_stacktrace(saved_proc);

	  panic("Unhandled kernel exception");
  }
  else {
	  /* in an early stage of boot process we don't have processes yet */
	  panic("exception in kernel while booting");
  }
}

/*===========================================================================*
 *				stacktrace				     *
 *===========================================================================*/
PUBLIC void proc_stacktrace(struct proc *whichproc)
{
	reg_t v_bp, v_pc, v_hbp;
	int iskernel;

	v_bp = whichproc->p_reg.fp;

	iskernel = iskernelp(whichproc);

	printf("%-8.8s %6d 0x%lx ",
		whichproc->p_name, whichproc->p_endpoint, whichproc->p_reg.pc);

	while(v_bp) {

#define PRCOPY(pr, pv, v, n) \
  (iskernel ? (memcpy((char *) v, (char *) pv, n), OK) : \
     data_copy(pr->p_endpoint, pv, KERNEL, (vir_bytes) (v), n))

	        if(PRCOPY(whichproc, v_bp, &v_hbp, sizeof(v_hbp)) != OK) {
			printf("(v_bp 0x%lx ?)", v_bp);
			break;
		}
		if(PRCOPY(whichproc, v_bp + sizeof(v_pc), &v_pc, sizeof(v_pc)) != OK) {
			printf("(v_pc 0x%lx ?)", v_bp + sizeof(v_pc));
			break;
		}
		printf("0x%lx ", (unsigned long) v_pc);
		if(v_hbp != 0 && v_hbp <= v_bp) {
			printf("(hbp %lx ?)", v_hbp);
			break;
		}
		v_bp = v_hbp;
	}
	printf("\n");
}

PUBLIC void enable_fpu_exception(void)
{
	write_cr0(read_cr0() | I386_CR0_TS);
}

PUBLIC void disable_fpu_exception(void)
{
	clts();
}

