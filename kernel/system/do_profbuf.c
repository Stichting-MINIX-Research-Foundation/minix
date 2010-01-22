/* The kernel call that is implemented in this file:
 *   m_type:    SYS_PROFBUF
 *
 * The parameters for this kernel call are:
 *    m7_p1:    PROF_CTL_PTR      (location of control struct)
 *    m7_p2:    PROF_MEM_PTR      (location of profiling table)
 *
 * Changes:
 *   14 Aug, 2006   Created (Rogier Meurs)
 */

#include "../system.h"

/*===========================================================================*
 *				do_profbuf				     *
 *===========================================================================*/
PUBLIC int do_profbuf(m_ptr)
register message *m_ptr;    /* pointer to request message */
{
/* This kernel call is used by profiled system processes when Call
 * Profiling is enabled. It is called on the first execution of procentry.
 * By means of this kernel call, the profiled processes inform the kernel
 * about the location of their profiling table and the control structure
 * which is used to enable the kernel to have the tables cleared.
 */ 
  int proc_nr;
  struct proc *rp;                          

  /* Store process name, control struct, table locations. */
  if(!isokendpt(m_ptr->m_source, &proc_nr))
	return EDEADSRCDST;

  if(cprof_procs_no >= NR_SYS_PROCS)
	return ENOSPC;

  rp = proc_addr(proc_nr);

  cprof_proc_info[cprof_procs_no].endpt = who_e;
  cprof_proc_info[cprof_procs_no].name = rp->p_name;

  cprof_proc_info[cprof_procs_no].ctl_v = (vir_bytes) m_ptr->PROF_CTL_PTR;
  cprof_proc_info[cprof_procs_no].buf_v = (vir_bytes) m_ptr->PROF_MEM_PTR;

  cprof_procs_no++;

  return OK;
}


