/* The kernel call implemented in this file:
 *   m_type:	SYS_VMCTL
 *
 * The parameters for this kernel call are:
 *   	SVMCTL_WHO	which process
 *    	SVMCTL_PARAM	set this setting (VMCTL_*)
 *    	SVMCTL_VALUE	to this value
 */

#include "kernel/system.h"
#include "kernel/vm.h"
// #include <assert.h> // Replaced

// Added kernel headers
#include <minix/kernel_types.h> // For k_errno_t
#include <klib/include/kprintf.h>
#include <klib/include/kstring.h>
#include <klib/include/kmemory.h>


/*===========================================================================*
 *				do_vmctl				     *
 *===========================================================================*/
int do_vmctl(struct proc * caller, message * m_ptr)
{
  int proc_nr;
  endpoint_t ep = m_ptr->SVMCTL_WHO;
  struct proc *p, *rp, **rpp, *target;

  if(ep == SELF) { ep = caller->p_endpoint; }

  if(!isokendpt(ep, &proc_nr)) {
	kprintf_stub("do_vmctl: unexpected endpoint %d from VM\n", ep); // MODIFIED
	return EINVAL; // EINVAL might be undefined
  }

  p = proc_addr(proc_nr);

  switch(m_ptr->SVMCTL_PARAM) {
	case VMCTL_CLEAR_PAGEFAULT:
		KASSERT_PLACEHOLDER(RTS_ISSET(p,RTS_PAGEFAULT)); // MODIFIED
		RTS_UNSET(p, RTS_PAGEFAULT);
		return OK;
	case VMCTL_MEMREQ_GET:
		/* Send VM the information about the memory request. We can
		 * not simply send the first request on the list, because IPC
		 * filters may forbid VM from getting requests for particular
		 * sources. However, IPC filters are used only in rare cases.
		 */
		for (rpp = &vmrequest; *rpp != NULL; // NULL might be undefined
		    rpp = &(*rpp)->p_vmrequest.nextrequestor) {
			rp = *rpp;

			KASSERT_PLACEHOLDER(RTS_ISSET(rp, RTS_VMREQUEST)); // MODIFIED

			okendpt(rp->p_vmrequest.target, &proc_nr);
			target = proc_addr(proc_nr);

			/* Check against IPC filters. */
			if (!allow_ipc_filtered_memreq(rp, target))
				continue;

			/* Reply with request fields. */
			if (rp->p_vmrequest.req_type != VMPTYPE_CHECK)
				panic("VMREQUEST wrong type");

			m_ptr->SVMCTL_MRG_TARGET	=
				rp->p_vmrequest.target;
			m_ptr->SVMCTL_MRG_ADDR		=
				rp->p_vmrequest.params.check.start;
			m_ptr->SVMCTL_MRG_LENGTH	=
				rp->p_vmrequest.params.check.length;
			m_ptr->SVMCTL_MRG_FLAG		=
				rp->p_vmrequest.params.check.writeflag;
			m_ptr->SVMCTL_MRG_REQUESTOR	=
				(void *) rp->p_endpoint;

			rp->p_vmrequest.vmresult = VMSUSPEND;

			/* Remove from request chain. */
			*rpp = rp->p_vmrequest.nextrequestor;

			return rp->p_vmrequest.req_type;
		}

		return ENOENT; // ENOENT might be undefined

	case VMCTL_MEMREQ_REPLY:
		KASSERT_PLACEHOLDER(RTS_ISSET(p, RTS_VMREQUEST)); // MODIFIED
		KASSERT_PLACEHOLDER(p->p_vmrequest.vmresult == VMSUSPEND); // MODIFIED
  		okendpt(p->p_vmrequest.target, &proc_nr);
		target = proc_addr(proc_nr);
		p->p_vmrequest.vmresult = m_ptr->SVMCTL_VALUE;
		KASSERT_PLACEHOLDER(p->p_vmrequest.vmresult != VMSUSPEND); // MODIFIED

		switch(p->p_vmrequest.type) {
		case VMSTYPE_KERNELCALL:
			/*
			 * we will have to resume execution of the kernel call
			 * as soon the scheduler picks up this process again
			 */
			p->p_misc_flags |= MF_KCALL_RESUME;
			break;
		case VMSTYPE_DELIVERMSG:
			KASSERT_PLACEHOLDER(p->p_misc_flags & MF_DELIVERMSG); // MODIFIED
			KASSERT_PLACEHOLDER(p == target); // MODIFIED
			KASSERT_PLACEHOLDER(RTS_ISSET(p, RTS_VMREQUEST)); // MODIFIED
			break;
		case VMSTYPE_MAP:
			KASSERT_PLACEHOLDER(RTS_ISSET(p, RTS_VMREQUEST)); // MODIFIED
			break;
		default:
			panic("strange request type: %d",p->p_vmrequest.type);
		}

		RTS_UNSET(p, RTS_VMREQUEST);
		return OK;

	case VMCTL_KERN_PHYSMAP:
	{
		int i = m_ptr->SVMCTL_VALUE;
		return arch_phys_map(i,
			(phys_bytes *) &m_ptr->SVMCTL_MAP_PHYS_ADDR,
			(phys_bytes *) &m_ptr->SVMCTL_MAP_PHYS_LEN,
			&m_ptr->SVMCTL_MAP_FLAGS);
	}
	case VMCTL_KERN_MAP_REPLY:
	{
		return arch_phys_map_reply(m_ptr->SVMCTL_VALUE,
			(vir_bytes) m_ptr->SVMCTL_MAP_VIR_ADDR);
	}
	case VMCTL_VMINHIBIT_SET:
		/* check if we must stop a process on a different CPU */
#if CONFIG_SMP
		if (p->p_cpu != cpuid) {
			smp_schedule_vminhibit(p);
		} else
#endif
			RTS_SET(p, RTS_VMINHIBIT);
#if CONFIG_SMP
		p->p_misc_flags |= MF_FLUSH_TLB;
#endif
		return OK;
	case VMCTL_VMINHIBIT_CLEAR:
		KASSERT_PLACEHOLDER(RTS_ISSET(p, RTS_VMINHIBIT)); // MODIFIED
		/*
		 * the processes is certainly not runnable, no need to tell its
		 * cpu
		 */
		RTS_UNSET(p, RTS_VMINHIBIT);
#ifdef CONFIG_SMP
		if (p->p_misc_flags & MF_SENDA_VM_MISS) {
			struct priv *privp;
			p->p_misc_flags &= ~MF_SENDA_VM_MISS;
			privp = priv(p);
			try_deliver_senda(p, (asynmsg_t *) privp->s_asyntab,
							privp->s_asynsize);
		}
		/*
		 * We don't know whether kernel has the changed mapping
		 * installed to access userspace memory. And if so, on what CPU.
		 * More over we don't know what mapping has changed and how and
		 * therefore we must invalidate all mappings we have anywhere.
		 * Next time we map memory, we map it fresh.
		 */
		bits_fill(p->p_stale_tlb, CONFIG_MAX_CPUS);
#endif
		return OK;
	case VMCTL_CLEARMAPCACHE:
		/* VM says: forget about old mappings we have cached. */
		mem_clear_mapcache();
		return OK;
	case VMCTL_BOOTINHIBIT_CLEAR:
		RTS_UNSET(p, RTS_BOOTINHIBIT);
		return OK;
  }

  /* Try architecture-specific vmctls. */
  return arch_do_vmctl(m_ptr, p);
}
