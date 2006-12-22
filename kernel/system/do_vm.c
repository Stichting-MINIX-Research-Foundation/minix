/* The system call implemented in this file:
 *   m_type:	SYS_VM_MAP
 *
 * The parameters for this system call are:
 *    m4_l1:	Process that requests map (VM_MAP_ENDPT)
 *    m4_l2:	Map (TRUE) or unmap (FALSE) (VM_MAP_MAPUNMAP)
 *    m4_l3:	Base address (VM_MAP_BASE)
 *    m4_l4:	Size  (VM_MAP_SIZE)
 *    m4_l5:	address  (VM_MAP_ADDR)
 */
#include "../system.h"

PRIVATE int vm_needs_init= 1;

#include <sys/vm.h>

/*===========================================================================*
 *				do_vm_map				     *
 *===========================================================================*/
PUBLIC int do_vm_map(m_ptr)
message *m_ptr;			/* pointer to request message */
{
	int proc_nr, do_map;
	phys_bytes base, size, offset, p_phys;
	struct proc *pp;

	/* do_serial_debug= 1; */
	if (vm_needs_init)
	{
	        vm_needs_init= 0;
	        vm_init();
	}

	if (m_ptr->VM_MAP_ENDPT == SELF) {
		proc_nr = who_p;
	} else {
		if(!isokendpt(m_ptr->VM_MAP_ENDPT, &proc_nr))
			return EINVAL;
	}

	do_map= m_ptr->VM_MAP_MAPUNMAP;
	base= m_ptr->VM_MAP_BASE;
	size= m_ptr->VM_MAP_SIZE;
	offset= m_ptr->VM_MAP_ADDR;

	pp= proc_addr(proc_nr);
	p_phys= umap_local(pp, D, base, size);
	if (p_phys == 0)
		return EFAULT;

	if (do_map)
	{
		pp->p_misc_flags |= MF_VM;

		vm_map_range(p_phys, size, offset);
	}
	else
	{
		vm_map_range(p_phys, size, p_phys);
	}

	return OK;
}


/*===========================================================================*
 *				vm_map_default				     *
 *===========================================================================*/
PUBLIC void vm_map_default(pp)
struct proc *pp;
{
	phys_bytes base_clicks, size_clicks;

	if (vm_needs_init)
		panic("vm_map_default: VM not initialized?", NO_NUM);
	pp->p_misc_flags &= ~MF_VM;
	base_clicks= pp->p_memmap[D].mem_phys;
	size_clicks= pp->p_memmap[S].mem_phys+pp->p_memmap[S].mem_len -
		base_clicks;
	vm_map_range(base_clicks << CLICK_SHIFT,
		size_clicks << CLICK_SHIFT, base_clicks << CLICK_SHIFT);
}

