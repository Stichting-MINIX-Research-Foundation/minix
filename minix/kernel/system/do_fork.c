/* The kernel call implemented in this file:
 *   m_type:	SYS_FORK
 *
 * The parameters for this kernel call are:
 *   m_lsys_krn_sys_fork.endpt		(parent, process that forked)
 *   m_lsys_krn_sys_fork.slot		(child's process table slot)
 *   m_lsys_krn_sys_fork.flags		(fork flags)
 *   m_krn_lsys_sys_fork.endpt		(endpoint of the child)
 *   m_krn_lsys_sys_fork.msgaddr	(new memory map for the child)
 */

#include "kernel/system.h"
#include "kernel/vm.h"
// #include <signal.h> // Replaced
// #include <string.h> // Replaced
// #include <assert.h> // Replaced

#include <minix/endpoint.h> // Kept
#include <minix/u64.h>      // Kept

// Added kernel headers
#include <minix/kernel_types.h> // For k_errno_t, k_sigset_t
#include <klib/include/kprintf.h>
#include <klib/include/kstring.h>
#include <klib/include/kmemory.h>


#if USE_FORK

/*===========================================================================*
 *				do_fork					     *
 *===========================================================================*/
int do_fork(struct proc * caller, message * m_ptr)
{
/* Handle sys_fork().
 * m_lsys_krn_sys_fork.endpt has forked.
 * The child is m_lsys_krn_sys_fork.slot.
 */
#if defined(__i386__)
  char *old_fpu_save_area_p;
#endif
  register struct proc *rpc;		/* child process pointer */
  struct proc *rpp;			/* parent process pointer */
  int gen;
  int p_proc;
  k_size_t namelen; // MODIFIED int to k_size_t (though kstrlen returns k_size_t, used in comparison)

  if(!isokendpt(m_ptr->m_lsys_krn_sys_fork.endpt, &p_proc))
	return EINVAL; // EINVAL might be undefined

  rpp = proc_addr(p_proc);
  rpc = proc_addr(m_ptr->m_lsys_krn_sys_fork.slot);
  if (isemptyp(rpp) || ! isemptyp(rpc)) return(EINVAL); // EINVAL might be undefined

  KASSERT_PLACEHOLDER(!(rpp->p_misc_flags & MF_DELIVERMSG)); // MODIFIED

  /* needs to be receiving so we know where the message buffer is */
  if(!RTS_ISSET(rpp, RTS_RECEIVING)) {
	kprintf_stub("kernel: fork not done synchronously?\n"); // MODIFIED
	return EINVAL; // EINVAL might be undefined
  }

  /* make sure that the FPU context is saved in parent before copy */
  save_fpu(rpp);
  /* Copy parent 'proc' struct to child. And reinitialize some fields. */
  gen = _ENDPOINT_G(rpc->p_endpoint);
#if defined(__i386__)
  old_fpu_save_area_p = rpc->p_seg.fpu_state;
#endif
  *rpc = *rpp;				/* copy 'proc' struct */
#if defined(__i386__)
  rpc->p_seg.fpu_state = old_fpu_save_area_p;
  if(proc_used_fpu(rpp))
	kmemcpy(rpc->p_seg.fpu_state, rpp->p_seg.fpu_state, FPU_XFP_SIZE); // MODIFIED
#endif
  if(++gen >= _ENDPOINT_MAX_GENERATION)	/* increase generation */
	gen = 1;			/* generation number wraparound */
  rpc->p_nr = m_ptr->m_lsys_krn_sys_fork.slot;	/* this was obliterated by copy */
  rpc->p_endpoint = _ENDPOINT(gen, rpc->p_nr);	/* new endpoint of slot */

  rpc->p_reg.retreg = 0;	/* child sees pid = 0 to know it is child */
  rpc->p_user_time = 0;		/* set all the accounting times to 0 */
  rpc->p_sys_time = 0;

  rpc->p_misc_flags &=
	~(MF_VIRT_TIMER | MF_PROF_TIMER | MF_SC_TRACE | MF_SPROF_SEEN | MF_STEP);
  rpc->p_virt_left = 0;		/* disable, clear the process-virtual timers */
  rpc->p_prof_left = 0;

  /* Mark process name as being a forked copy */
  namelen = kstrlen(rpc->p_name); // MODIFIED
#define FORKSTR "*F"
  if(namelen+kstrlen(FORKSTR) < sizeof(rpc->p_name)) // MODIFIED
	/* FIXME: strcat was here */; // MODIFIED strcat(rpc->p_name, FORKSTR);

  /* the child process is not runnable until it's scheduled. */
  RTS_SET(rpc, RTS_NO_QUANTUM);
  reset_proc_accounting(rpc);

  rpc->p_cpu_time_left = 0;
  rpc->p_cycles = 0;
  rpc->p_kcall_cycles = 0;
  rpc->p_kipc_cycles = 0;

  rpc->p_tick_cycles = 0;
  cpuavg_init(&rpc->p_cpuavg);

  /* If the parent is a privileged process, take away the privileges from the 
   * child process and inhibit it from running by setting the NO_PRIV flag.
   * The caller should explicitly set the new privileges before executing.
   */
  if (priv(rpp)->s_flags & SYS_PROC) {
      rpc->p_priv = priv_addr(USER_PRIV_ID);
      rpc->p_rts_flags |= RTS_NO_PRIV;
  }

  /* Calculate endpoint identifier, so caller knows what it is. */
  m_ptr->m_krn_lsys_sys_fork.endpt = rpc->p_endpoint;
  m_ptr->m_krn_lsys_sys_fork.msgaddr = rpp->p_delivermsg_vir;

  /* Don't schedule process in VM mode until it has a new pagetable. */
  if(m_ptr->m_lsys_krn_sys_fork.flags & PFF_VMINHIBIT) {
  	RTS_SET(rpc, RTS_VMINHIBIT);
  }

  /* 
   * Only one in group should have RTS_SIGNALED, child doesn't inherit tracing.
   */
  RTS_UNSET(rpc, (RTS_SIGNALED | RTS_SIG_PENDING | RTS_P_STOP));
  /* FIXME: sigemptyset was here */ // (void) sigemptyset(&rpc->p_pending);

#if defined(__i386__)
  rpc->p_seg.p_cr3 = 0;
  rpc->p_seg.p_cr3_v = NULL; // MODIFIED (NULL)
#elif defined(__arm__)
  rpc->p_seg.p_ttbr = 0;
  rpc->p_seg.p_ttbr_v = NULL; // MODIFIED (NULL)
#endif

  return OK;
}

#endif /* USE_FORK */
