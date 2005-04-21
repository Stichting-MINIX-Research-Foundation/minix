/* The system call implemented in this file:
 *   m_type:	SYS_FORK
 *
 * The parameters for this system call are:
 *    m1_i1:	PR_PROC_NR	(child's process table slot)	
 *    m1_i2:	PR_PPROC_NR	(parent, process that forked)	
 *    m1_i3:	PR_PID	 	(child pid received from MM)
 */

#include "../kernel.h"
#include "../system.h"
#include "../sendmask.h"
#include <signal.h>
#if (CHIP == INTEL)
#include "../protect.h"
#endif

INIT_ASSERT

/*===========================================================================*
 *				do_fork					     *
 *===========================================================================*/
PUBLIC int do_fork(m_ptr)
register message *m_ptr;	/* pointer to request message */
{
/* Handle sys_fork().  PR_PPROC_NR has forked.  The child is PR_PROC_NR. */

#if (CHIP == INTEL)
  reg_t old_ldt_sel;
#endif
  register struct proc *rpc;
  struct proc *rpp;

  rpp = proc_addr(m_ptr->PR_PPROC_NR);
  assert(isuserp(rpp));
  rpc = proc_addr(m_ptr->PR_PROC_NR);
  assert(isemptyp(rpc));

  /* Copy parent 'proc' struct to child. */
#if (CHIP == INTEL)
  old_ldt_sel = rpc->p_ldt_sel;	/* stop this being obliterated by copy */
#endif

  *rpc = *rpp;					/* copy 'proc' struct */

#if (CHIP == INTEL)
  rpc->p_ldt_sel = old_ldt_sel;
#endif
  rpc->p_nr = m_ptr->PR_PROC_NR;	/* this was obliterated by copy */

  rpc->p_flags |= NO_MAP;	/* inhibit the process from running */

  rpc->p_flags &= ~(PENDING | SIG_PENDING | P_STOP);

  /* Only 1 in group should have PENDING, child does not inherit trace status*/
  sigemptyset(&rpc->p_pending);
  rpc->p_pendcount = 0;
  rpc->p_reg.retreg = 0;	/* child sees pid = 0 to know it is child */

  rpc->user_time = 0;		/* set all the accounting times to 0 */
  rpc->sys_time = 0;
  rpc->child_utime = 0;
  rpc->child_stime = 0;

  return(OK);
}


/* The system call implemented in this file:
 *   m_type:	SYS_NEWMAP
 *
 * The parameters for this system call are:
 *    m1_i1:	PR_PROC_NR		(install new map for this process)
 *    m1_p1:	PR_MEM_PTR		(pointer to memory map)
 */


/*===========================================================================*
 *				do_newmap				     *
 *===========================================================================*/
PUBLIC int do_newmap(m_ptr)
message *m_ptr;			/* pointer to request message */
{
/* Handle sys_newmap().  Fetch the memory map from MM. */

  register struct proc *rp;
  phys_bytes src_phys;
  int caller;			/* whose space has the new map (usually MM) */
  int k;			/* process whose map is to be loaded */
  int old_flags;		/* value of flags before modification */
  struct mem_map *map_ptr;	/* virtual address of map inside caller (MM) */

  /* Extract message parameters and copy new memory map from MM. */
  caller = m_ptr->m_source;
  k = m_ptr->PR_PROC_NR;
  map_ptr = (struct mem_map *) m_ptr->PR_MEM_PTR;
  if (!isokprocn(k)) return(EINVAL);
  rp = proc_addr(k);		/* ptr to entry of user getting new map */

  /* Copy the map from MM. */
  src_phys = umap_local(proc_addr(caller), D, (vir_bytes) map_ptr, 
  	sizeof(rp->p_memmap));
  assert(src_phys != 0);
  phys_copy(src_phys,vir2phys(rp->p_memmap),(phys_bytes)sizeof(rp->p_memmap));

#if (CHIP != M68000)
  alloc_segments(rp);
#else
  pmmu_init_proc(rp);
#endif
  old_flags = rp->p_flags;	/* save the previous value of the flags */
  rp->p_flags &= ~NO_MAP;
  if (old_flags != 0 && rp->p_flags == 0) lock_ready(rp);

  return(OK);
}


/* The system call implemented in this file:
 *   m_type:	SYS_EXEC
 *
 * The parameters for this system call are:
 *    m1_i1:	PR_PROC_NR		(process that did exec call)
 *    m1_i3:	PR_TRACING		(flag to indicate tracing is on/ off)
 *    m1_p1:	PR_STACK_PTR		(new stack pointer)
 *    m1_p2:	PR_NAME_PTR		(pointer to program name)
 *    m1_p3:	PR_IP_PTR		(new instruction pointer)
 */

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
#define NLEN (sizeof(rp->p_name)-1)

  rp = proc_addr(m_ptr->PR_PROC_NR);
  assert(isuserp(rp));
  if (m_ptr->PR_TRACING) cause_sig(m_ptr->PR_PROC_NR, SIGTRAP);
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
  kmemset(&rp->p_ldt[EXTRA_LDT_INDEX], 0,
	(LDT_SIZE - EXTRA_LDT_INDEX) * sizeof(rp->p_ldt[0]));
#endif
  rp->p_reg.pc = (reg_t) m_ptr->PR_IP_PTR;	/* set pc */
  rp->p_flags &= ~RECEIVING;	/* MM does not reply to EXEC call */
  if (rp->p_flags == 0) lock_ready(rp);

  /* Save command name for debugging, ps(1) output, etc. */
  phys_name = numap_local(m_ptr->m_source, (vir_bytes) m_ptr->PR_NAME_PTR,
							(vir_bytes) NLEN);
  if (phys_name != 0) {
	phys_copy(phys_name, vir2phys(rp->p_name), (phys_bytes) NLEN);
	for (np = rp->p_name; (*np & BYTE) >= ' '; np++) {}
	*np = 0;
  }
  return(OK);
}


/* The system call implemented in this file:
 *   m_type:	SYS_XIT
 *
 * The parameters for this system call are:
 *    m1_i1:	PR_PROC_NR		(slot number of exiting process)
 *    m1_i2:	PR_PPROC_NR		(slot number of parent process)
 */



/*===========================================================================*
 *				do_xit					     *
 *===========================================================================*/
PUBLIC int do_xit(m_ptr)
message *m_ptr;			/* pointer to request message */
{
/* Handle sys_exit. A user process has exited (the MM sent the request).
 */
  register struct proc *rp, *rc;
  struct proc *np, *xp;
  int exit_proc_nr;				

  /* Get a pointer to the process that exited. */
  exit_proc_nr = m_ptr->PR_PROC_NR;	
  if (exit_proc_nr == SELF) exit_proc_nr = m_ptr->m_source;
  if (! isokprocn(exit_proc_nr)) return(EINVAL);
  rc = proc_addr(exit_proc_nr);

  /* If this is a user process and the MM passed in a valid parent process, 
   * accumulate the child times at the parent. 
   */
  if (isuserp(rc) && isokprocn(m_ptr->PR_PPROC_NR)) {
      rp = proc_addr(m_ptr->PR_PPROC_NR);
      lock();
      rp->child_utime += rc->user_time + rc->child_utime;
      rp->child_stime += rc->sys_time + rc->child_stime;
      unlock();
  }

  /* Now call the routine to clean up of the process table slot. This cancels
   * outstanding timers, possibly removes the process from the message queues,
   * and resets important process table fields.
   */
  clear_proc(exit_proc_nr);
  return(OK);				/* tell MM that cleanup succeeded */
}


