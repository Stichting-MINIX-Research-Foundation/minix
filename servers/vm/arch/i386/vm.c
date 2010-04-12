
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
#include <assert.h>
#include <env.h>

#include "proto.h"
#include "vm.h"
#include "util.h"

#include "memory.h"

/*===========================================================================*
 *				arch_map2vir				     *
 *===========================================================================*/
PUBLIC vir_bytes arch_map2vir(struct vmproc *vmp, vir_bytes addr)
{
	vir_bytes textstart = CLICK2ABS(vmp->vm_arch.vm_seg[T].mem_phys);
	vir_bytes datastart = CLICK2ABS(vmp->vm_arch.vm_seg[D].mem_phys);

	/* Could be a text address. */
	assert(datastart <= addr || textstart <= addr);

	return addr - datastart;
}

/*===========================================================================*
 *				arch_map2str				     *
 *===========================================================================*/
PUBLIC char *arch_map2str(struct vmproc *vmp, vir_bytes addr)
{
	static char bufstr[100];
	vir_bytes textstart = CLICK2ABS(vmp->vm_arch.vm_seg[T].mem_phys);
	vir_bytes textend = textstart + CLICK2ABS(vmp->vm_arch.vm_seg[T].mem_len);
	vir_bytes datastart = CLICK2ABS(vmp->vm_arch.vm_seg[D].mem_phys);

	if(addr < textstart) {
		sprintf(bufstr, "<lin:0x%lx>", addr);
	} else if(addr < datastart) {
		sprintf(bufstr, "0x%lx (codeseg)", addr - textstart);
	} else {
		sprintf(bufstr, "0x%lx (dataseg)", addr - datastart);
	}

	return bufstr;
}

/*===========================================================================*
 *				arch_map2info				     *
 *===========================================================================*/
PUBLIC vir_bytes arch_map2info(struct vmproc *vmp, vir_bytes addr, int *seg,
	int *prot)
{
	vir_bytes textstart = CLICK2ABS(vmp->vm_arch.vm_seg[T].mem_phys);
	vir_bytes textend = textstart +
		CLICK2ABS(vmp->vm_arch.vm_seg[T].mem_len);
	vir_bytes datastart = CLICK2ABS(vmp->vm_arch.vm_seg[D].mem_phys);

	/* The protection to be returned here is that of the segment. */
	if(addr < textstart) {
		*seg = D;
		*prot = PROT_READ | PROT_WRITE | PROT_EXEC;
		return addr;
	} else if(addr < datastart) {
		*seg = T;
		*prot = PROT_READ | PROT_EXEC;
		return addr - textstart;
	} else {
		*seg = D;
		if (textstart == textend)	/* common I&D? */
			*prot = PROT_READ | PROT_WRITE | PROT_EXEC;
		else
			*prot = PROT_READ | PROT_WRITE;
		return addr - datastart;
	}
}

/*===========================================================================*
 *				arch_addrok				     *
 *===========================================================================*/
PUBLIC vir_bytes arch_addrok(struct vmproc *vmp, vir_bytes addr)
{
	vir_bytes textstart = CLICK2ABS(vmp->vm_arch.vm_seg[T].mem_phys);
	vir_bytes textend = CLICK2ABS(vmp->vm_arch.vm_seg[T].mem_phys +
		vmp->vm_arch.vm_seg[T].mem_phys);
	vir_bytes datastart = CLICK2ABS(vmp->vm_arch.vm_seg[D].mem_phys);

	if(addr >= textstart && addr < textstart+textend)
		return 1;

	if(addr >= datastart && addr < VM_DATATOP)
		return 1;

	return 0;
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
