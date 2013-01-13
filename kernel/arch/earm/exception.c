/* This file contains a simple exception handler.  Exceptions in user
 * processes are converted to signals. Exceptions in a kernel task cause
 * a panic.
 */

#include "kernel/kernel.h"
#include "arch_proto.h"
#include <signal.h>
#include <string.h>
#include <assert.h>
#include "kernel/proc.h"
#include "kernel/proto.h"
#include <machine/vm.h>

struct ex_s {
	char *msg;
	int signum;
};

static struct ex_s ex_data[] = {
	{ "Reset", 0},
	{ "Undefined instruction", SIGILL},
	{ "Supervisor call", 0},
	{ "Prefetch Abort", SIGILL},
	{ "Data Abort", SIGSEGV},
	{ "Hypervisor call", 0},
	{ "Interrupt", 0},
	{ "Fast Interrupt", 0},
};

static void inkernel_disaster(struct proc *saved_proc,
	reg_t *saved_lr, struct ex_s *ep, int is_nested);

extern int catch_pagefaults;

static void proc_stacktrace_execute(struct proc *whichproc, reg_t v_bp, reg_t pc);

static void pagefault( struct proc *pr,
			reg_t *saved_lr,
			int is_nested)
{
	int in_physcopy = 0, in_memset = 0;

	reg_t pagefault_addr, pagefault_status;
	message m_pagefault;
	int err;

	pagefault_addr = read_dfar();
	pagefault_status = read_dfsr();

#if 0
	printf("kernel: pagefault in pr %d, addr 0x%lx, his ttbr 0x%lx, actual ttbr 0x%lx\n",
		pr->p_endpoint, pagefault_addr, pr->p_seg.p_ttbr, read_ttbr0());
#endif

	in_physcopy = (*saved_lr > (vir_bytes) phys_copy) &&
	   (*saved_lr < (vir_bytes) phys_copy_fault);

	in_memset = (*saved_lr > (vir_bytes) phys_memset) &&
	   (*saved_lr < (vir_bytes) memset_fault);

	if((is_nested || iskernelp(pr)) &&
		catch_pagefaults && (in_physcopy || in_memset)) {
#if 0
		printf("pf caught! addr 0x%lx\n", pagefault_addr);
#endif
		if (is_nested) {
			if(in_physcopy) {
				assert(!in_memset);
				*saved_lr = (reg_t) phys_copy_fault_in_kernel;
			} else {
				*saved_lr = (reg_t) memset_fault_in_kernel;
			}
		}
		else {
			pr->p_reg.pc = (reg_t) phys_copy_fault;
			pr->p_reg.retreg = pagefault_addr;
		}

		return;
	}

	if(is_nested) {
		printf("pagefault in kernel at pc 0x%lx address 0x%lx\n",
			*saved_lr, pagefault_addr);
		inkernel_disaster(pr, saved_lr, NULL, is_nested);
	}

	/* VM can't handle page faults. */
	if(pr->p_endpoint == VM_PROC_NR) {
		/* Page fault we can't / don't want to
		 * handle.
		 */
		printf("pagefault for VM on CPU %d, "
			"pc = 0x%x, addr = 0x%x, flags = 0x%x, is_nested %d\n",
			cpuid, pr->p_reg.pc, pagefault_addr, pagefault_status,
			is_nested);
		proc_stacktrace(pr);
		printf("pc of pagefault: 0x%lx\n", pr->p_reg.pc);
		panic("pagefault in VM");

		return;
	}

	/* Don't schedule this process until pagefault is handled. */
	RTS_SET(pr, RTS_PAGEFAULT);

	/* tell Vm about the pagefault */
	m_pagefault.m_source = pr->p_endpoint;
	m_pagefault.m_type   = VM_PAGEFAULT;
	m_pagefault.VPF_ADDR = pagefault_addr;
	m_pagefault.VPF_FLAGS = pagefault_status;

	if ((err = mini_send(pr, VM_PROC_NR,
					&m_pagefault, FROM_KERNEL))) {
		panic("WARNING: pagefault: mini_send returned %d\n", err);
	}

	return;
}

static void inkernel_disaster(struct proc *saved_proc,
	reg_t *saved_lr, struct ex_s *ep,
	int is_nested)
{
#if USE_SYSDEBUG
  if(ep)
	printf("\n%s\n", ep->msg);

  printf("cpu %d is_nested = %d ", cpuid, is_nested);

  if (saved_proc) {
	  printf("scheduled was: process %d (%s), ", saved_proc->p_endpoint, saved_proc->p_name);
	  printf("pc = 0x%x\n", (unsigned) saved_proc->p_reg.pc);
	  proc_stacktrace(saved_proc);

	  panic("Unhandled kernel exception");
  }

  /* in an early stage of boot process we don't have processes yet */
  panic("exception in kernel while booting, no saved_proc yet");

#endif /* USE_SYSDEBUG */
}

void exception_handler(int is_nested, reg_t *saved_lr, int vector)
{
/* An exception or unexpected interrupt has occurred. */
  struct ex_s *ep;
  struct proc *saved_proc;

  saved_proc = get_cpulocal_var(proc_ptr);

  ep = &ex_data[vector];

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
	if (((void*)*saved_lr >= (void*)copy_msg_to_user &&
			(void*)*saved_lr <= (void*)__copy_msg_to_user_end) ||
			((void*)*saved_lr >= (void*)copy_msg_from_user &&
			(void*)*saved_lr <= (void*)__copy_msg_from_user_end)) {
		switch(vector) {
		/* these error are expected */
		case DATA_ABORT_VECTOR:
			*saved_lr = (reg_t) __user_copy_msg_pointer_failure;
			return;
		default:
			panic("Copy involving a user pointer failed unexpectedly!");
		}
	}
  }

  if (vector == DATA_ABORT_VECTOR) {
	pagefault(saved_proc, saved_lr, is_nested);
	return;
  }

  /* If an exception occurs while running a process, the is_nested variable
   * will be zero. Exceptions in interrupt handlers or system traps will make
   * is_nested non-zero.
   */
  if (is_nested == 0 && ! iskernelp(saved_proc)) {
	cause_sig(proc_nr(saved_proc), ep->signum);
	return;
  }

  /* Exception in system code. This is not supposed to happen. */
  inkernel_disaster(saved_proc, saved_lr, ep, is_nested);

  panic("return from inkernel_disaster");
}

#if USE_SYSDEBUG
/*===========================================================================*
 *				proc_stacktrace_execute			     *
 *===========================================================================*/
static void proc_stacktrace_execute(struct proc *whichproc, reg_t v_bp, reg_t pc)
{
	printf("%-8.8s %6d 0x%lx \n",
		whichproc->p_name, whichproc->p_endpoint, pc);
}
#endif

void proc_stacktrace(struct proc *whichproc)
{
#if USE_SYSDEBUG
	proc_stacktrace_execute(whichproc, whichproc->p_reg.fp, whichproc->p_reg.pc);
#endif /* USE_SYSDEBUG */
}

void enable_fpu_exception(void)
{
}

void disable_fpu_exception(void)
{
}
