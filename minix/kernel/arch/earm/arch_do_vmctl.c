/* The kernel call implemented in this file:
 *   m_type:	SYS_VMCTL
 *
 * The parameters for this kernel call are:
 *   	SVMCTL_WHO	which process
 *    	SVMCTL_PARAM	set this setting (VMCTL_*)
 *    	SVMCTL_VALUE	to this value
 */

#include "kernel/system.h"
// #include <assert.h> // Replaced
#include <minix/type.h> // Kept for now, may need future review

#include "arch_proto.h"

// Added kernel headers
#include <klib/include/kprintf.h> // For KASSERT_PLACEHOLDER and kprintf_stub
#include <minix/kernel_types.h>
#include <klib/include/kstring.h> // Precautionary
#include <klib/include/kmemory.h> // Precautionary


static void set_ttbr(struct proc *p, u32_t ttbr, u32_t *v)
{
	/* Set process TTBR. */
	p->p_seg.p_ttbr = ttbr;
	KASSERT_PLACEHOLDER(p->p_seg.p_ttbr); // MODIFIED
	p->p_seg.p_ttbr_v = v;
	if(p == get_cpulocal_var(ptproc)) {
		write_ttbr0(p->p_seg.p_ttbr);
	}
	if(p->p_nr == VM_PROC_NR) {
		if (arch_enable_paging(p) != OK)
			panic("arch_enable_paging failed");
	}
	RTS_UNSET(p, RTS_VMINHIBIT);
}

/*===========================================================================*
 *				arch_do_vmctl				     *
 *===========================================================================*/
int arch_do_vmctl(
  register message *m_ptr,	/* pointer to request message */
  struct proc *p
)
{
  switch(m_ptr->SVMCTL_PARAM) {
	case VMCTL_GET_PDBR:
		/* Get process page directory base reg (TTBR). */
		m_ptr->SVMCTL_VALUE = p->p_seg.p_ttbr;
		return OK;
	case VMCTL_SETADDRSPACE:
		set_ttbr(p, m_ptr->SVMCTL_PTROOT, (u32_t *) m_ptr->SVMCTL_PTROOT_V);
		return OK;
	case VMCTL_FLUSHTLB:
	{
		reload_ttbr0();
		return OK;
	}
  }

  kprintf_stub("arch_do_vmctl: strange param %d\n", m_ptr->SVMCTL_PARAM); // MODIFIED
  return EINVAL;
}
