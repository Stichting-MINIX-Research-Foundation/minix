/* The kernel calls that are implemented in this file:
 *   m_type:	SYS_SETMCONTEXT
 *   m_type:	SYS_GETMCONTEXT
 *
 * The parameters for these kernel calls are:
 *     m1_i1:	PR_ENDPT        # proc endpoint doing call
 *     m1_p1:	PR_MEM_PTR	# pointer to mcontext structure
 *
 */

#include "kernel/system.h"
#include <string.h>
#include <machine/mcontext.h>

#if USE_MCONTEXT 
/*===========================================================================*
 *			      do_getmcontext				     *
 *===========================================================================*/
int do_getmcontext(struct proc * caller, message * m_ptr)
{
/* Retrieve machine context of a process */

  register struct proc *rp;
  int proc_nr, r;
  mcontext_t mc;

  if (! isokendpt(m_ptr->PR_ENDPT, &proc_nr)) return(EINVAL);
  if (iskerneln(proc_nr)) return(EPERM);
  rp = proc_addr(proc_nr);

#if defined(__i386__)
  if (!proc_used_fpu(rp))
	return(OK);	/* No state to copy */
#endif

  /* Get the mcontext structure into our address space.  */
  if ((r = data_copy(m_ptr->PR_ENDPT, (vir_bytes) m_ptr->PR_CTX_PTR, KERNEL,
		(vir_bytes) &mc, (phys_bytes) sizeof(struct __mcontext))) != OK)
	return(r);

#if defined(__i386__)
  /* Copy FPU state */
  mc.mc_fpu_flags = 0;
  if (proc_used_fpu(rp)) {
	/* make sure that the FPU context is saved into proc structure first */
	save_fpu(rp);
	mc.mc_fpu_flags = rp->p_misc_flags & MF_FPU_INITIALIZED;
	memcpy(&(mc.mc_fpu_state), rp->p_seg.fpu_state, FPU_XFP_SIZE);
  } 
#endif


  /* Copy the mcontext structure to the user's address space. */
  if ((r = data_copy(KERNEL, (vir_bytes) &mc, m_ptr->PR_ENDPT, 
	(vir_bytes) m_ptr->PR_CTX_PTR,
	(phys_bytes) sizeof(struct __mcontext))) != OK)
	return(r);

  return(OK);
}


/*===========================================================================*
 *			      do_setmcontext				     *
 *===========================================================================*/
int do_setmcontext(struct proc * caller, message * m_ptr)
{
/* Set machine context of a process */

  register struct proc *rp;
  int proc_nr, r;
  mcontext_t mc;

  if (!isokendpt(m_ptr->PR_ENDPT, &proc_nr)) return(EINVAL);
  rp = proc_addr(proc_nr);

  /* Get the mcontext structure into our address space.  */
  if ((r = data_copy(m_ptr->PR_ENDPT, (vir_bytes) m_ptr->PR_CTX_PTR, KERNEL,
		(vir_bytes) &mc, (phys_bytes) sizeof(struct __mcontext))) != OK)
	return(r);

#if defined(__i386__)
  /* Copy FPU state */
  if (mc.mc_fpu_flags & MF_FPU_INITIALIZED) {
	rp->p_misc_flags |= MF_FPU_INITIALIZED;
	memcpy(rp->p_seg.fpu_state, &(mc.mc_fpu_state), FPU_XFP_SIZE);
  } else
	rp->p_misc_flags &= ~MF_FPU_INITIALIZED;
  /* force reloading FPU in either case */
  release_fpu(rp);
#endif

  return(OK);
}

#endif
