/* The kernel call implemented in this file:
 *   m_type:	SYS_VMCTL
 *
 * The parameters for this kernel call are:
 *   	SVMCTL_WHO	which process
 *    	SVMCTL_PARAM	set this setting (VMCTL_*)
 *    	SVMCTL_VALUE	to this value
 */

#include "../../system.h"
#include <minix/type.h>

extern u32_t kernel_cr3;

/*===========================================================================*
 *				arch_do_vmctl				     *
 *===========================================================================*/
PUBLIC int arch_do_vmctl(m_ptr, p)
register message *m_ptr;	/* pointer to request message */
struct proc *p;
{
  switch(m_ptr->SVMCTL_PARAM) {
	case VMCTL_I386_GETCR3:
		/* Get process CR3. */
		m_ptr->SVMCTL_VALUE = p->p_seg.p_cr3;
		return OK;
	case VMCTL_I386_SETCR3:
		/* Set process CR3. */
		if(m_ptr->SVMCTL_VALUE) {
			p->p_seg.p_cr3 = m_ptr->SVMCTL_VALUE;
			p->p_misc_flags |= MF_FULLVM;
		} else {
			p->p_seg.p_cr3 = kernel_cr3;
			p->p_misc_flags &= ~MF_FULLVM;
		}
		RTS_LOCK_UNSET(p, VMINHIBIT);
		return OK;
        case VMCTL_GET_PAGEFAULT:
	{
  		struct proc *rp;
		if(!(rp=pagefaults))
			return ESRCH;
		pagefaults = rp->p_nextpagefault;
		if(!RTS_ISSET(rp, PAGEFAULT))
			minix_panic("non-PAGEFAULT process on pagefault chain",
				rp->p_endpoint);
                m_ptr->SVMCTL_PF_WHO = rp->p_endpoint;
                m_ptr->SVMCTL_PF_I386_CR2 = rp->p_pagefault.pf_virtual;
		m_ptr->SVMCTL_PF_I386_ERR = rp->p_pagefault.pf_flags;
		return OK;
	}
  }

  kprintf("arch_do_vmctl: strange param %d\n", m_ptr->SVMCTL_PARAM);
  return EINVAL;
}
