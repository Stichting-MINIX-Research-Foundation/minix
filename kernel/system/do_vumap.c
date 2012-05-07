/* The kernel call implemented in this file:
 *   m_type:	SYS_VUMAP
 *
 * The parameters for this kernel call are:
 *   m10_i1:	VUMAP_ENDPT	(grant owner, or SELF for local addresses)
 *   m10_l1:	VUMAP_VADDR	(address of virtual (input) vector)
 *   m10_i2:	VUMAP_VCOUNT	(number of elements in virtual vector)
 *   m10_l2:	VUMAP_OFFSET	(offset into first entry of input vector)
 *   m10_i3:	VUMAP_ACCESS	(safecopy access requested for input)
 *   m10_l3:	VUMAP_PADDR	(address of physical (output) vector)
 *   m10_i4:	VUMAP_PMAX	(maximum number of physical vector elements)
 *   m10_i1:	VUMAP_PCOUNT	(upon return: number of elements filled)
 */

#include "kernel/system.h"

#include <assert.h>

/*===========================================================================*
 *				do_vumap				     *
 *===========================================================================*/
int do_vumap(struct proc *caller, message *m_ptr)
{
/* Map a vector of grants or local virtual addresses to physical addresses.
 * Designed to be used by drivers to perform an efficient lookup of physical
 * addresses for the purpose of direct DMA from/to a remote process.
 */
  endpoint_t endpt, source, granter;
  struct proc *procp;
  struct vumap_vir vvec[MAPVEC_NR];
  struct vumap_phys pvec[MAPVEC_NR];
  vir_bytes vaddr, paddr, vir_addr;
  phys_bytes phys_addr;
  int i, r, proc_nr, vcount, pcount, pmax, access;
  size_t size, chunk, offset;

  endpt = caller->p_endpoint;

  /* Retrieve and check input parameters. */
  source = m_ptr->VUMAP_ENDPT;
  vaddr = (vir_bytes) m_ptr->VUMAP_VADDR;
  vcount = m_ptr->VUMAP_VCOUNT;
  offset = m_ptr->VUMAP_OFFSET;
  access = m_ptr->VUMAP_ACCESS;
  paddr = (vir_bytes) m_ptr->VUMAP_PADDR;
  pmax = m_ptr->VUMAP_PMAX;

  if (vcount <= 0 || pmax <= 0)
	return EINVAL;

  if (vcount > MAPVEC_NR) vcount = MAPVEC_NR;
  if (pmax > MAPVEC_NR) pmax = MAPVEC_NR;

  /* Convert access to safecopy access flags. */
  switch (access) {
  case VUA_READ:		access = CPF_READ; break;
  case VUA_WRITE:		access = CPF_WRITE; break;
  case VUA_READ|VUA_WRITE:	access = CPF_READ|CPF_WRITE; break;
  default:			return EINVAL;
  }

  /* Copy in the vector of virtual addresses. */
  size = vcount * sizeof(vvec[0]);

  if (data_copy(endpt, vaddr, KERNEL, (vir_bytes) vvec, size) != OK)
	return EFAULT;

  pcount = 0;

  /* Go through the input entries, one at a time. Stop early in case the output
   * vector has filled up.
   */
  for (i = 0; i < vcount && pcount < pmax; i++) {
	size = vvec[i].vv_size;
	if (size <= offset)
		return EINVAL;
	size -= offset;

	if (source != SELF) {
		r = verify_grant(source, endpt, vvec[i].vv_grant, size, access,
			offset, &vir_addr, &granter);
		if (r != OK)
			return r;
	} else {
		vir_addr = vvec[i].vv_addr + offset;
		granter = endpt;
	}

	okendpt(granter, &proc_nr);
	procp = proc_addr(proc_nr);

	/* Each virtual range is made up of one or more physical ranges. */
	while (size > 0 && pcount < pmax) {
		chunk = vm_lookup_range(procp, vir_addr, &phys_addr, size);

		if (!chunk) {
			/* Try to get the memory allocated, unless the memory
			 * is supposed to be there to be read from.
			 */
			if (access & CPF_READ)
				return EFAULT;

			/* This call may suspend the current call, or return an
			 * error for a previous invocation.
			 */
			return vm_check_range(caller, procp, vir_addr, size);
		}

		pvec[pcount].vp_addr = phys_addr;
		pvec[pcount].vp_size = chunk;
		pcount++;

		vir_addr += chunk;
		size -= chunk;
	}

	offset = 0;
  }

  /* Copy out the resulting vector of physical addresses. */
  assert(pcount > 0);

  size = pcount * sizeof(pvec[0]);

  r = data_copy_vmcheck(caller, KERNEL, (vir_bytes) pvec, endpt, paddr, size);

  if (r == OK)
	m_ptr->VUMAP_PCOUNT = pcount;

  return r;
}
