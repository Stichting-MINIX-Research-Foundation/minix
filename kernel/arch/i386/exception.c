/* This file contains a simple exception handler.  Exceptions in user
 * processes are converted to signals. Exceptions in a kernel task cause
 * a panic.
 */

#include "../../kernel.h"
#include "proto.h"
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <minix/sysutil.h>
#include "../../proc.h"

extern int vm_copy_in_progress;
extern struct proc *vm_copy_from, *vm_copy_to;
extern u32_t vm_copy_from_v, vm_copy_to_v;
extern u32_t vm_copy_from_p, vm_copy_to_p, vm_copy_cr3;

u32_t pagefault_cr2, pagefault_count = 0;

void pagefault(struct proc *pr, int trap_errno)
{
	int s;
	vir_bytes ph;
	u32_t pte;

	if(pagefault_count != 1)
		minix_panic("recursive pagefault", pagefault_count);

	/* Don't schedule this process until pagefault is handled. */
	if(RTS_ISSET(pr, PAGEFAULT))
		minix_panic("PAGEFAULT set", pr->p_endpoint);
	RTS_LOCK_SET(pr, PAGEFAULT);

	if(pr->p_endpoint <= INIT_PROC_NR) {
		/* Page fault we can't / don't want to
		 * handle.
		 */
		kprintf("pagefault for process %d ('%s'), pc = 0x%x\n",
			pr->p_endpoint, pr->p_name, pr->p_reg.pc);
		proc_stacktrace(pr);
  		minix_panic("page fault in system process", pr->p_endpoint);

		return;
	}

	/* Save pagefault details, suspend process,
	 * add process to pagefault chain,
	 * and tell VM there is a pagefault to be
	 * handled.
	 */
	pr->p_pagefault.pf_virtual = pagefault_cr2;
	pr->p_pagefault.pf_flags = trap_errno;
	pr->p_nextpagefault = pagefaults;
	pagefaults = pr;
	lock_notify(HARDWARE, VM_PROC_NR);

	pagefault_count = 0;

#if 0
	kprintf("pagefault for process %d ('%s'), pc = 0x%x\n",
			pr->p_endpoint, pr->p_name, pr->p_reg.pc);
	proc_stacktrace(pr);
#endif

	return;
}

/*===========================================================================*
 *				exception				     *
 *===========================================================================*/
PUBLIC void exception(vec_nr, trap_errno, old_eip, old_cs, old_eflags)
unsigned vec_nr;
u32_t trap_errno;
u32_t old_eip;
U16_t old_cs;
u32_t old_eflags;
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
	{ "Copressor segment overrun", SIGSEGV, 286 },
	{ "Invalid TSS", SIGSEGV, 286 },
	{ "Segment not present", SIGSEGV, 286 },
	{ "Stack exception", SIGSEGV, 286 },	/* STACK_FAULT already used */
	{ "General protection", SIGSEGV, 286 },
	{ "Page fault", SIGSEGV, 386 },		/* not close */
	{ NIL_PTR, SIGILL, 0 },			/* probably software trap */
	{ "Coprocessor error", SIGFPE, 386 },
  };
  register struct ex_s *ep;
  struct proc *saved_proc;

  /* Save proc_ptr, because it may be changed by debug statements. */
  saved_proc = proc_ptr;	

  ep = &ex_data[vec_nr];

  if (vec_nr == 2) {		/* spurious NMI on some machines */
	kprintf("got spurious NMI\n");
	return;
  }

  /* If an exception occurs while running a process, the k_reenter variable 
   * will be zero. Exceptions in interrupt handlers or system traps will make 
   * k_reenter larger than zero.
   */
  if (k_reenter == 0 && ! iskernelp(saved_proc)) {
	{
		switch(vec_nr) {
			case PAGE_FAULT_VECTOR:
				pagefault(saved_proc, trap_errno);
				return;
		}

		kprintf(
"exception for process %d, endpoint %d ('%s'), pc = 0x%x:0x%x, sp = 0x%x:0x%x\n",
			proc_nr(saved_proc), saved_proc->p_endpoint,
			saved_proc->p_name,
			saved_proc->p_reg.cs, saved_proc->p_reg.pc,
			saved_proc->p_reg.ss, saved_proc->p_reg.sp);
  		kprintf(
  "vec_nr= %d, trap_errno= 0x%lx, eip= 0x%lx, cs= 0x%x, eflags= 0x%lx\n",
			vec_nr, (unsigned long)trap_errno,
			(unsigned long)old_eip, old_cs,
			(unsigned long)old_eflags);
		proc_stacktrace(saved_proc);
	}

	kprintf("kernel: cause_sig %d for %d\n",
		ep->signum, saved_proc->p_endpoint);
	cause_sig(proc_nr(saved_proc), ep->signum);
	return;
  }

  /* Exception in system code. This is not supposed to happen. */
  if (ep->msg == NIL_PTR || machine.processor < ep->minprocessor)
	kprintf("\nIntel-reserved exception %d\n", vec_nr);
  else
	kprintf("\n%s\n", ep->msg);
  kprintf("k_reenter = %d ", k_reenter);
  kprintf("process %d (%s), ", proc_nr(saved_proc), saved_proc->p_name);
  kprintf("pc = %u:0x%x\n", (unsigned) saved_proc->p_reg.cs,
	  (unsigned) saved_proc->p_reg.pc);
  kprintf(
  "vec_nr= %d, trap_errno= 0x%lx, eip= 0x%lx, cs= 0x%x, eflags= 0x%lx\n",
	vec_nr, (unsigned long)trap_errno,
	(unsigned long)old_eip, old_cs, (unsigned long)old_eflags);
  proc_stacktrace(saved_proc);

  minix_panic("exception in a kernel task", saved_proc->p_endpoint);
}

/*===========================================================================*
 *				stacktrace				     *
 *===========================================================================*/
PUBLIC void proc_stacktrace(struct proc *proc)
{
	reg_t bp, v_bp, v_pc, v_hbp;

	v_bp = proc->p_reg.fp;

	kprintf("ep %d pc 0x%lx stack ", proc->p_endpoint, proc->p_reg.pc);

	while(v_bp) {
	        if(data_copy(proc->p_endpoint, v_bp,
			SYSTEM, (vir_bytes) &v_hbp, sizeof(v_hbp)) != OK) {
			kprintf("(v_bp 0x%lx ?)", v_bp);
			break;
		}
		if(data_copy(proc->p_endpoint, v_bp + sizeof(v_pc),
			SYSTEM, (vir_bytes) &v_pc, sizeof(v_pc)) != OK) {
			kprintf("(v_pc 0x%lx ?)", v_pc);
			break;
		}
		kprintf("0x%lx ", (unsigned long) v_pc);
		if(v_hbp != 0 && v_hbp <= v_bp) {
			kprintf("(hbp %lx ?)", v_hbp);
			break;
		}
		v_bp = v_hbp;
	}
	kprintf("\n");
}
