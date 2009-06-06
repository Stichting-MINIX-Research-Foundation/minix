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

extern int verifyrange;

/*===========================================================================*
 *				do_vmctl				     *
 *===========================================================================*/
PUBLIC int do_vmctl(m_ptr)
register message *m_ptr;	/* pointer to request message */
{
  int proc_nr, i;
  endpoint_t ep = m_ptr->SVMCTL_WHO;
  struct proc *p, *rp, *target;

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
		vmassert(RTS_ISSET(rp, VMREQUEST));

#if 0
		printf("kernel: vm request sent by: %s / %d about %d; 0x%lx-0x%lx, wr %d, stack: %s ",
			rp->p_name, rp->p_endpoint, rp->p_vmrequest.who,
			rp->p_vmrequest.start,
			rp->p_vmrequest.start + rp->p_vmrequest.length,
			rp->p_vmrequest.writeflag, rp->p_vmrequest.stacktrace);
		printf("type %d\n", rp->p_vmrequest.type);
#endif

#if DEBUG_VMASSERT
  		okendpt(rp->p_vmrequest.who, &proc_nr);
		target = proc_addr(proc_nr);
		if(!RTS_ISSET(target, VMREQTARGET)) {
			printf("set stack: %s\n", rp->p_vmrequest.stacktrace);
			minix_panic("VMREQTARGET not set for target",
				NO_NUM);
		}
#endif

		/* Reply with request fields. */
		m_ptr->SVMCTL_MRG_ADDR = (char *) rp->p_vmrequest.start;
		m_ptr->SVMCTL_MRG_LEN = rp->p_vmrequest.length;
		m_ptr->SVMCTL_MRG_WRITE = rp->p_vmrequest.writeflag;
		m_ptr->SVMCTL_MRG_EP = rp->p_vmrequest.who;
		m_ptr->SVMCTL_MRG_REQUESTOR = (void *) rp->p_endpoint;
		rp->p_vmrequest.vmresult = VMSUSPEND;

		/* Remove from request chain. */
		vmrequest = vmrequest->p_vmrequest.nextrequestor;

		return OK;
	case VMCTL_MEMREQ_REPLY:
		vmassert(RTS_ISSET(p, VMREQUEST));
		vmassert(p->p_vmrequest.vmresult == VMSUSPEND);
  		okendpt(p->p_vmrequest.who, &proc_nr);
		target = proc_addr(proc_nr);
		p->p_vmrequest.vmresult = m_ptr->SVMCTL_VALUE;
		vmassert(p->p_vmrequest.vmresult != VMSUSPEND);
		if(p->p_vmrequest.vmresult != OK)
			kprintf("SYSTEM: VM replied %d to mem request\n",
				p->p_vmrequest.vmresult);


#if 0
		printf("memreq reply: vm request sent by: %s / %d about %d; 0x%lx-0x%lx, wr %d, stack: %s ",
			p->p_name, p->p_endpoint, p->p_vmrequest.who,
			p->p_vmrequest.start,
			p->p_vmrequest.start + p->p_vmrequest.length,
			p->p_vmrequest.writeflag, p->p_vmrequest.stacktrace);
		printf("type %d\n", p->p_vmrequest.type);
#endif

#if DEBUG_VMASSERT
	{
		vmassert(target->p_rts_flags);

		/* Sanity check. */
		if(p->p_vmrequest.vmresult == OK) {
			int r;
			vmassert(!verifyrange);
			verifyrange = 1;
			r = CHECKRANGE(target,
				p->p_vmrequest.start,
				p->p_vmrequest.length,
				p->p_vmrequest.writeflag);
			vmassert(verifyrange);
			verifyrange = 0;

			if(r != OK) {

kprintf("SYSTEM: request by %d: on ep %d: 0x%lx-0x%lx, wrflag %d, stack %s, failed\n",
	p->p_endpoint, target->p_endpoint,
	p->p_vmrequest.start,  p->p_vmrequest.start + p->p_vmrequest.length,
	p->p_vmrequest.writeflag,
	p->p_vmrequest.stacktrace); 
	
				printf("printing pt of %d (0x%lx)\n",
					vm_print(target->p_endpoint),
					target->p_seg.p_cr3
					);
				vm_print(target->p_seg.p_cr3);
				minix_panic("SYSTEM: fail but VM said OK", NO_NUM);
			 }
		}  
	}
#endif

		vmassert(RTS_ISSET(target, VMREQTARGET));
		RTS_LOCK_UNSET(target, VMREQTARGET);

		if(p->p_vmrequest.type == VMSTYPE_KERNELCALL) {
			/* Put on restart chain. */
			p->p_vmrequest.nextrestart = vmrestart;
			vmrestart = p;
		} else if(p->p_vmrequest.type == VMSTYPE_DELIVERMSG) {
			vmassert(p->p_misc_flags & MF_DELIVERMSG);
			vmassert(p == target);
			vmassert(RTS_ISSET(p, VMREQUEST));
			vmassert(RTS_ISSET(p, VMREQTARGET));
			RTS_LOCK_UNSET(p, VMREQUEST);
			RTS_LOCK_UNSET(target, VMREQTARGET);
		} else {
#if DEBUG_VMASSERT
			printf("suspended with stack: %s\n",
				p->p_vmrequest.stacktrace);
#endif
			minix_panic("strange request type",
				p->p_vmrequest.type);
		}

		return OK;
	case VMCTL_ENABLE_PAGING:
		if(vm_running) 
			minix_panic("do_vmctl: paging already enabled", NO_NUM);
		vm_init(p);
		if(!vm_running)
			minix_panic("do_vmctl: paging enabling failed", NO_NUM);
		vmassert(p->p_delivermsg_lin ==
		  umap_local(p, D, p->p_delivermsg_vir, sizeof(message)));
		if(newmap(p, m_ptr->SVMCTL_VALUE) != OK)
			minix_panic("do_vmctl: newmap failed", NO_NUM);
		p->p_delivermsg_lin =
			umap_local(p, D, p->p_delivermsg_vir, sizeof(message));
		vmassert(p->p_delivermsg_lin);
		return OK;
  }

  /* Try architecture-specific vmctls. */
  return arch_do_vmctl(m_ptr, p);
}
