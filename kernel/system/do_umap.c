/* The kernel call implemented in this file:
 *   m_type:	SYS_UMAP
 *
 * The parameters for this kernel call are:
 *    m5_i1:	CP_SRC_PROC_NR	(process number)	
 *    m5_s1:	CP_SRC_SPACE	(segment where address is: T, D, or S)
 *    m5_l1:	CP_SRC_ADDR	(virtual address)	
 *    m5_l2:	CP_DST_ADDR	(returns physical address)	
 *    m5_l3:	CP_NR_BYTES	(size of datastructure) 	
 */

#include "kernel/system.h"

#include <minix/endpoint.h>

#if USE_UMAP

/*==========================================================================*
 *				do_umap					    *
 *==========================================================================*/
PUBLIC int do_umap(struct proc * caller, message * m_ptr)
{
/* Map virtual address to physical, for non-kernel processes. */
  int seg_type = m_ptr->CP_SRC_SPACE & SEGMENT_TYPE;
  int seg_index = m_ptr->CP_SRC_SPACE & SEGMENT_INDEX;
  vir_bytes offset = m_ptr->CP_SRC_ADDR;
  int count = m_ptr->CP_NR_BYTES;
  int endpt = (int) m_ptr->CP_SRC_ENDPT;
  int proc_nr, r;
  int naughty = 0;
  phys_bytes phys_addr = 0, lin_addr = 0;
  struct proc *targetpr;

  /* Verify process number. */
  if (endpt == SELF)
	proc_nr = _ENDPOINT_P(caller->p_endpoint);
  else
	if (! isokendpt(endpt, &proc_nr))
		return(EINVAL);
  targetpr = proc_addr(proc_nr);

  /* See which mapping should be made. */
  switch(seg_type) {
  case LOCAL_SEG:
      phys_addr = lin_addr = umap_local(targetpr, seg_index, offset, count); 
      if(!lin_addr) return EFAULT;
      naughty = 1;
      break;
  case REMOTE_SEG:
      phys_addr = lin_addr = umap_remote(targetpr, seg_index, offset, count); 
      if(!lin_addr) return EFAULT;
      naughty = 1;
      break;
  case LOCAL_VM_SEG:
    if(seg_index == MEM_GRANT) {
	vir_bytes newoffset;
	endpoint_t newep;
	int new_proc_nr;
	cp_grant_id_t grant = (cp_grant_id_t) offset;

        if(verify_grant(targetpr->p_endpoint, ANY, grant, count, 0, 0,
                &newoffset, &newep) != OK) {
                printf("SYSTEM: do_umap: verify_grant in %s, grant %d, bytes 0x%lx, failed, caller %s\n", targetpr->p_name, offset, count, caller->p_name);
		proc_stacktrace(caller);
                return EFAULT;
        }

        if(!isokendpt(newep, &new_proc_nr)) {
                printf("SYSTEM: do_umap: isokendpt failed\n");
                return EFAULT;
        }

	/* New lookup. */
	offset = newoffset;
	targetpr = proc_addr(new_proc_nr);
	seg_index = D;
      }

      if(seg_index == T || seg_index == D || seg_index == S) {
        phys_addr = lin_addr = umap_local(targetpr, seg_index, offset, count); 
      } else {
	printf("SYSTEM: bogus seg type 0x%lx\n", seg_index);
	return EFAULT;
      }
      if(!lin_addr) {
	printf("SYSTEM:do_umap: umap_local failed\n");
	return EFAULT;
      }
      if(vm_lookup(targetpr, lin_addr, &phys_addr, NULL) != OK) {
	printf("SYSTEM:do_umap: vm_lookup failed\n");
	return EFAULT;
      }
      if(phys_addr == 0)
	panic("vm_lookup returned zero physical address");
      break;
  default:
      if((r=arch_umap(targetpr, offset, count, seg_type, &lin_addr))
	!= OK)
	return r;
      phys_addr = lin_addr;
  }

  if(vm_running && !vm_contiguous(targetpr, lin_addr, count)) {
	printf("SYSTEM:do_umap: not contiguous\n");
	return EFAULT;
  }

  m_ptr->CP_DST_ADDR = phys_addr;
  if(naughty || phys_addr == 0) {
	  printf("kernel: umap 0x%x done by %d / %s, pc 0x%lx, 0x%lx -> 0x%lx\n",
		seg_type, caller->p_endpoint, caller->p_name,
		caller->p_reg.pc, offset, phys_addr);
	printf("caller stack: ");
	proc_stacktrace(caller);
  }
  return (phys_addr == 0) ? EFAULT: OK;
}

#endif /* USE_UMAP */
