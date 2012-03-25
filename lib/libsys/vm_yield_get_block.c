
#include "syslib.h"

#include <minix/vm.h>
#include <minix/u64.h>

/*===========================================================================*
 *                                vm_forgetblocks		     	*
 *===========================================================================*/
void vm_forgetblocks(void)
{
	message m;
	_taskcall(VM_PROC_NR, VM_FORGETBLOCKS, &m);
	return;
}

/*===========================================================================*
 *                                vm_forgetblock		     	*
 *===========================================================================*/
int vm_forgetblock(u64_t id)
{
	message m;

	m.VMFB_IDHI = ex64hi(id);
	m.VMFB_IDLO = ex64lo(id);

	return _taskcall(VM_PROC_NR, VM_FORGETBLOCK, &m);
}

/*===========================================================================*
 *                                vm_yield_block_get_block	     	*
 *===========================================================================*/
int vm_yield_block_get_block(u64_t yieldid, u64_t getid,
	void *mem, vir_bytes len)
{
	message m;

	m.VMYBGB_VADDR = mem;
	m.VMYBGB_GETIDHI = ex64hi(getid);
	m.VMYBGB_GETIDLO = ex64lo(getid);
	m.VMYBGB_LEN = len;
	m.VMYBGB_YIELDIDHI = ex64hi(yieldid);
	m.VMYBGB_YIELDIDLO = ex64lo(yieldid);

	return _taskcall(VM_PROC_NR, VM_YIELDBLOCKGETBLOCK, &m);
}

