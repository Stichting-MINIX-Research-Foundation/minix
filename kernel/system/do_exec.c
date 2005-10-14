/* The kernel call implemented in this file:
 *   m_type:	SYS_EXEC
 *
 * The parameters for this kernel call are:
 *    m1_i1:	PR_PROC_NR		(process that did exec call)
 *    m1_p1:	PR_STACK_PTR		(new stack pointer)
 *    m1_p2:	PR_NAME_PTR		(pointer to program name)
 *    m1_p3:	PR_IP_PTR		(new instruction pointer)
 */
#include "../system.h"
#include <string.h>
#include <signal.h>

#if USE_EXEC

/*===========================================================================*
 *				do_exec					     *
 *===========================================================================*/
PUBLIC int do_exec(m_ptr)
register message *m_ptr;	/* pointer to request message */
{
/* Handle sys_exec().  A process has done a successful EXEC. Patch it up. */
  register struct proc *rp;
  reg_t sp;			/* new sp */
  phys_bytes phys_name;
  char *np;

  rp = proc_addr(m_ptr->PR_PROC_NR);
  sp = (reg_t) m_ptr->PR_STACK_PTR;
  rp->p_reg.sp = sp;		/* set the stack pointer */
#if (CHIP == M68000)
  rp->p_splow = sp;		/* set the stack pointer low water */
#ifdef FPP
  /* Initialize fpp for this process */
  fpp_new_state(rp);
#endif
#endif
#if (CHIP == INTEL)		/* wipe extra LDT entries */
  phys_memset(vir2phys(&rp->p_ldt[EXTRA_LDT_INDEX]), 0,
	(LDT_SIZE - EXTRA_LDT_INDEX) * sizeof(rp->p_ldt[0]));
#endif
  rp->p_reg.pc = (reg_t) m_ptr->PR_IP_PTR;	/* set pc */
  rp->p_rts_flags &= ~RECEIVING;	/* PM does not reply to EXEC call */
  if (rp->p_rts_flags == 0) lock_enqueue(rp);

  /* Save command name for debugging, ps(1) output, etc. */
  phys_name = numap_local(m_ptr->m_source, (vir_bytes) m_ptr->PR_NAME_PTR,
					(vir_bytes) P_NAME_LEN - 1);
  if (phys_name != 0) {
	phys_copy(phys_name, vir2phys(rp->p_name), (phys_bytes) P_NAME_LEN - 1);
	for (np = rp->p_name; (*np & BYTE) >= ' '; np++) {}
	*np = 0;					/* mark end */
  } else {
  	strncpy(rp->p_name, "<unset>", P_NAME_LEN);
  }
  return(OK);
}
#endif /* USE_EXEC */

