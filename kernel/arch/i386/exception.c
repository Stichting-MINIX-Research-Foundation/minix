/* This file contains a simple exception handler.  Exceptions in user
 * processes are converted to signals. Exceptions in a kernel task cause
 * a panic.
 */

#include "../../kernel.h"
#include <signal.h>
#include "../../proc.h"

/*===========================================================================*
 *				exception				     *
 *===========================================================================*/
PUBLIC void exception(unsigned vec_nr)
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
#if 0
	{
		kprintf(
		"exception for process %d, pc = 0x%x:0x%x, sp = 0x%x:0x%x\n",
			proc_nr(saved_proc),
			saved_proc->p_reg.cs, saved_proc->p_reg.pc,
			saved_proc->p_reg.ss, saved_proc->p_reg.sp);
		kprintf("edi = 0x%x\n", saved_proc->p_reg.di);

#if DEBUG_STACKTRACE
		stacktrace(saved_proc);
#endif
	}
#endif

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
  kprintf("pc = %u:0x%x", (unsigned) saved_proc->p_reg.cs,
  (unsigned) saved_proc->p_reg.pc);

  panic("exception in a kernel task", NO_NUM);
}

#if DEBUG_STACKTRACE
/*===========================================================================*
 *				stacktrace				     *
 *===========================================================================*/
PUBLIC void stacktrace(struct proc *proc)
{
	reg_t bp, v_bp, v_pc, v_hbp;

	v_bp = proc->p_reg.fp;

	kprintf("stacktrace: ");
	while(v_bp) {
		phys_bytes p;
		if(!(p = umap_local(proc, D, v_bp, sizeof(v_bp)))) {
			kprintf("(bad bp %lx)", v_bp);
			break;
		}
		phys_copy(p+sizeof(v_pc), vir2phys(&v_pc), sizeof(v_pc));
		phys_copy(p, vir2phys(&v_hbp), sizeof(v_hbp));
		kprintf("0x%lx ", (unsigned long) v_pc);
		if(v_hbp != 0 && v_hbp <= v_bp) {
			kprintf("(bad hbp %lx)", v_hbp);
			break;
		}
		v_bp = v_hbp;
	}
	kprintf("\n");
}
#endif

