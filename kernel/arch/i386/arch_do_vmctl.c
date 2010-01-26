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

#include "proto.h"

extern u8_t *vm_pagedirs;

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
			p->p_seg.p_cr3 = 0;
			p->p_misc_flags &= ~MF_FULLVM;
		}
		RTS_LOCK_UNSET(p, RTS_VMINHIBIT);
		return OK;
	case VMCTL_INCSP:
		/* Increase process SP. */
		p->p_reg.sp += m_ptr->SVMCTL_VALUE;
		return OK;
        case VMCTL_GET_PAGEFAULT:
	{
  		struct proc *rp;
		if(!(rp=pagefaults))
			return ESRCH;
		pagefaults = rp->p_nextpagefault;
		if(!RTS_ISSET(rp, RTS_PAGEFAULT))
			minix_panic("non-PAGEFAULT process on pagefault chain",
				rp->p_endpoint);
                m_ptr->SVMCTL_PF_WHO = rp->p_endpoint;
                m_ptr->SVMCTL_PF_I386_CR2 = rp->p_pagefault.pf_virtual;
		m_ptr->SVMCTL_PF_I386_ERR = rp->p_pagefault.pf_flags;
		return OK;
	}
	case VMCTL_I386_KERNELLIMIT:
	{
		int r;
		/* VM wants kernel to increase its segment. */
		r = prot_set_kern_seg_limit(m_ptr->SVMCTL_VALUE);
		return r;
	}
	case VMCTL_I386_PAGEDIRS:
	{
		vm_pagedirs = (u8_t *) m_ptr->SVMCTL_VALUE;
		return OK;
	}
	case VMCTL_I386_FREEPDE:
	{
		i386_freepde(m_ptr->SVMCTL_VALUE);
		return OK;
	}
	case VMCTL_FLUSHTLB:
	{
		level0(reload_cr3);
		return OK;
	}
  }



  kprintf("arch_do_vmctl: strange param %d\n", m_ptr->SVMCTL_PARAM);
  return EINVAL;
}
