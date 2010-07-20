/* The kernel call implemented in this file:
 *   m_type:	SYS_VMCTL
 *
 * The parameters for this kernel call are:
 *   	SVMCTL_WHO	which process
 *    	SVMCTL_PARAM	set this setting (VMCTL_*)
 *    	SVMCTL_VALUE	to this value
 */

#include "kernel/system.h"
#include <minix/type.h>

#include "proto.h"

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
	case VMCTL_SETADDRSPACE:
		/* Set process CR3. */
		if(m_ptr->SVMCTL_PTROOT) {
			p->p_seg.p_cr3 = m_ptr->SVMCTL_PTROOT;
			p->p_seg.p_cr3_v = (u32_t *) m_ptr->SVMCTL_PTROOT_V;
			p->p_misc_flags |= MF_FULLVM;
			if(p == ptproc) {
				write_cr3(p->p_seg.p_cr3);
			}
		} else {
			p->p_seg.p_cr3 = 0;
			p->p_seg.p_cr3_v = NULL;
			p->p_misc_flags &= ~MF_FULLVM;
		}
		RTS_UNSET(p, RTS_VMINHIBIT);
		return OK;
	case VMCTL_INCSP:
		/* Increase process SP. */
		p->p_reg.sp += m_ptr->SVMCTL_VALUE;
		return OK;
	case VMCTL_I386_KERNELLIMIT:
	{
		int r;
		/* VM wants kernel to increase its segment. */
		r = prot_set_kern_seg_limit(m_ptr->SVMCTL_VALUE);
		return r;
	}
	case VMCTL_I386_FREEPDE:
	{
		i386_freepde(m_ptr->SVMCTL_VALUE);
		return OK;
	}
	case VMCTL_FLUSHTLB:
	{
		reload_cr3();
		return OK;
	}
  }



  printf("arch_do_vmctl: strange param %d\n", m_ptr->SVMCTL_PARAM);
  return EINVAL;
}
