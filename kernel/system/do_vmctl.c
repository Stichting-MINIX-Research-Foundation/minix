/* The kernel call implemented in this file:
 *   m_type:	SYS_VMCTL
 *
 * The parameters for this kernel call are:
 *   	SVMCTL_WHO	which process
 *    	SVMCTL_PARAM	set this setting (VMCTL_*)
 *    	SVMCTL_VALUE	to this value
 */

#include "../system.h"
#include "../vm.h"
#include "../debug.h"
#include <minix/type.h>
#include <minix/config.h>

/*===========================================================================*
 *				do_vmctl				     *
 *===========================================================================*/
PUBLIC int do_vmctl(m_ptr)
register message *m_ptr;	/* pointer to request message */
{
  int proc_nr, i;
  endpoint_t ep = m_ptr->SVMCTL_WHO;
  struct proc *p, *rp;

  if(ep == SELF) { ep = m_ptr->m_source; }

  if(!isokendpt(ep, &proc_nr)) {
	kprintf("do_vmctl: unexpected endpoint %d from VM\n", ep);
	return EINVAL;
  }

  p = proc_addr(proc_nr);

  switch(m_ptr->SVMCTL_PARAM) {
	case VMCTL_CLEAR_PAGEFAULT:
		RTS_LOCK_UNSET(p, PAGEFAULT);
		return OK;
	case VMCTL_MEMREQ_GET:
		/* Send VM the information about the memory request.  */
		if(!(rp = vmrequest))
			return ESRCH;
		if(!RTS_ISSET(rp, VMREQUEST))
			minix_panic("do_vmctl: no VMREQUEST set", NO_NUM);

		printf("kernel: vm request sent by: %s / %d about %d; 0x%lx-0x%lx, wr %d\n",
			rp->p_name, rp->p_endpoint, rp->p_vmrequest.who,
			rp->p_vmrequest.start,
			rp->p_vmrequest.start + rp->p_vmrequest.length,
			rp->p_vmrequest.writeflag);

		/* Reply with request fields. */
		m_ptr->SVMCTL_MRG_ADDR = (char *) rp->p_vmrequest.start;
		m_ptr->SVMCTL_MRG_LEN = rp->p_vmrequest.length;
		m_ptr->SVMCTL_MRG_WRITE = rp->p_vmrequest.writeflag;
		m_ptr->SVMCTL_MRG_EP = rp->p_vmrequest.who;
		rp->p_vmrequest.vmresult = VMSUSPEND;

		/* Remove from request chain. */
		vmrequest = vmrequest->p_vmrequest.nextrequestor;

		return OK;
	case VMCTL_MEMREQ_REPLY:
		if(!(rp = p->p_vmrequest.requestor))
			minix_panic("do_vmctl: no requestor set", ep);
		p->p_vmrequest.requestor = NULL;
		if(!RTS_ISSET(rp, VMREQUEST))
			minix_panic("do_vmctl: no VMREQUEST set", ep);
		if(rp->p_vmrequest.vmresult != VMSUSPEND)
			minix_panic("do_vmctl: result not VMSUSPEND set",
				rp->p_vmrequest.vmresult);
		rp->p_vmrequest.vmresult = m_ptr->SVMCTL_VALUE;
		if(rp->p_vmrequest.vmresult == VMSUSPEND)
			minix_panic("VM returned VMSUSPEND?", NO_NUM);
		if(rp->p_vmrequest.vmresult != OK)
			kprintf("SYSTEM: VM replied %d to mem request\n",
				rp->p_vmrequest.vmresult);

		/* Put on restart chain. */
		printf("kernel: vm reply received for %d\n", rp->p_endpoint);
		rp->p_vmrequest.nextrestart = vmrestart;
		vmrestart = rp;

#if DEBUG_VMASSERT
		/* Sanity check. */
		if(rp->p_vmrequest.vmresult == OK) {
			if(CHECKRANGE(p,
				rp->p_vmrequest.start,
				rp->p_vmrequest.length,
				rp->p_vmrequest.writeflag) != OK) {
kprintf("SYSTEM: request %d:0x%lx-0x%lx, wrflag %d, failed\n",
	rp->p_endpoint,
	rp->p_vmrequest.start,  rp->p_vmrequest.start + rp->p_vmrequest.length,
	rp->p_vmrequest.writeflag); 
	
				minix_panic("SYSTEM: fail but VM said OK", NO_NUM);
			}
		}
#endif
		return OK;
	case VMCTL_ENABLE_PAGING:
		if(vm_running) 
			minix_panic("do_vmctl: paging already enabled", NO_NUM);
		vm_init(p);
		if(!vm_running)
			minix_panic("do_vmctl: paging enabling failed", NO_NUM);
		if(newmap(p, m_ptr->SVMCTL_VALUE) != OK)
			minix_panic("do_vmctl: newmap failed", NO_NUM);
		return OK;
  }

  /* Try architecture-specific vmctls. */
  return arch_do_vmctl(m_ptr, p);
}
