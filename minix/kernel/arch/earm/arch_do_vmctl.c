/* The kernel call implemented in this file:
 *   m_type:	SYS_VMCTL
 *
 * The parameters for this kernel call are:
 *   	SVMCTL_WHO	which process
 *    	SVMCTL_PARAM	set this setting (VMCTL_*)
 *    	SVMCTL_VALUE	to this value
 */

#include "kernel/system.h"
#include <assert.h>
#include <minix/type.h>

#include "asid.h"
#include "arch_proto.h"

static void set_ttbr(struct proc *p, u32_t ttbr, u32_t *v)
{
	/* Set process TTBR. */
	p->p_seg.p_ttbr = ttbr;
	assert(p->p_seg.p_ttbr);
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
int arch_do_vmctl(m_ptr, p)
register message *m_ptr;	/* pointer to request message */
struct proc *p;
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
	case VMCTL_FLUSH_ASID:
	{
		if(p->p_asid != 0)
			tlb_flush_asid(p->p_asid);
		return OK;
	}
	case VMCTL_RUN_ASID_TEST:
	{
		int i;
		struct proc *test_proc = p;
		u8_t original_asid = test_proc->p_asid;
		u32_t original_gen = test_proc->p_asid_generation;

		/* 1. Test alloc/free from free list */
		for (i = 0; i < 10; i++) {
			asid_alloc(test_proc);
			asid_free(test_proc);
		}

		/* 2. Test exhaustion and recycling */
		for (i = 0; i < 300; i++) {
			asid_alloc(test_proc);
			test_proc->p_asid = 0; /* Hack to force new allocation */
		}

		/* Restore original asid info */
		test_proc->p_asid = original_asid;
		test_proc->p_asid_generation = original_gen;
		asid_alloc(test_proc); /* to make sure it's valid again */

		printf("ASID allocator test completed successfully.\n");

		return OK;
	}
  }

  printf("arch_do_vmctl: strange param %d\n", m_ptr->SVMCTL_PARAM);
  return EINVAL;
}
