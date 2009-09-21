
#define _SYSTEM 1

#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/config.h>
#include <minix/const.h>
#include <minix/ds.h>
#include <minix/endpoint.h>
#include <minix/keymap.h>
#include <minix/minlib.h>
#include <minix/type.h>
#include <minix/ipc.h>
#include <minix/sysutil.h>
#include <minix/syslib.h>
#include <minix/bitmap.h>

#include <sys/mman.h>

#include <errno.h>
#include <env.h>

#include "../proto.h"
#include "../vm.h"
#include "../util.h"

#include "memory.h"

/*===========================================================================*
 *				arch_map2vir				     *
 *===========================================================================*/
PUBLIC vir_bytes arch_map2vir(struct vmproc *vmp, vir_bytes addr)
{
	vir_bytes textstart = CLICK2ABS(vmp->vm_arch.vm_seg[T].mem_phys);
	vir_bytes datastart = CLICK2ABS(vmp->vm_arch.vm_seg[D].mem_phys);

	/* Could be a text address. */
	vm_assert(datastart <= addr || textstart <= addr);

	return addr - datastart;
}

/*===========================================================================*
 *				arch_vir2map				     *
 *===========================================================================*/
PUBLIC vir_bytes arch_vir2map(struct vmproc *vmp, vir_bytes addr)
{
	vir_bytes bottom = CLICK2ABS(vmp->vm_arch.vm_seg[D].mem_phys);

	return addr + bottom;
}

/*===========================================================================*
 *				arch_vir2map_text			     *
 *===========================================================================*/
PUBLIC vir_bytes arch_vir2map_text(struct vmproc *vmp, vir_bytes addr)
{
	vir_bytes bottom = CLICK2ABS(vmp->vm_arch.vm_seg[T].mem_phys);

	return addr + bottom;
}
