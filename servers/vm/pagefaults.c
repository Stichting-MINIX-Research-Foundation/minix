
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
#include <minix/safecopies.h>

#include <errno.h>
#include <string.h>
#include <env.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>

#include <pagefaults.h>

#include "glo.h"
#include "proto.h"
#include "memory.h"
#include "util.h"
#include "region.h"

static char *pferr(int err)
{
	static char buf[100];

	sprintf(buf, "err 0x%lx ", err);
	if(PFERR_NOPAGE(err)) strcat(buf, "nopage ");
	if(PFERR_PROT(err)) strcat(buf, "protection ");
	if(PFERR_WRITE(err)) strcat(buf, "write");
	if(PFERR_READ(err)) strcat(buf, "read");

	return buf;
}

/*===========================================================================*
 *				handle_pagefaults	     		     *
 *===========================================================================*/
PUBLIC void handle_pagefaults(void)
{
	endpoint_t ep;
	u32_t addr, err;
	struct vmproc *vmp;
	int r, s;

	while((r=arch_get_pagefault(&ep, &addr, &err)) == OK) {
		struct vir_region *region;
		vir_bytes offset;
		int p, wr = PFERR_WRITE(err);

		if(vm_isokendpt(ep, &p) != OK)
			vm_panic("handle_pagefaults: endpoint wrong", ep);

		vmp = &vmproc[p];
		vm_assert(vmp->vm_flags & VMF_INUSE);

		/* See if address is valid at all. */
		if(!(region = map_lookup(vmp, addr))) {
			vm_assert(PFERR_NOPAGE(err));
			printf("VM: SIGSEGV %d bad addr 0x%lx error 0x%lx\n", 
				ep, addr, err);
			if((s=sys_kill(vmp->vm_endpoint, SIGSEGV)) != OK)
				vm_panic("sys_kill failed", s);
			continue;
		}

		/* Make sure this isn't a region that isn't supposed
		 * to cause pagefaults.
		 */
		vm_assert(!(region->flags & VR_NOPF));

		/* If process was writing, see if it's writable. */
		if(!(region->flags & VR_WRITABLE) && wr) {
			printf("VM: SIGSEGV %d ro map 0x%lx error 0x%lx\n", 
				ep, addr, err);
			if((s=sys_kill(vmp->vm_endpoint, SIGSEGV)) != OK)
				vm_panic("sys_kill failed", s);
			continue;
		}

		vm_assert(addr > region->vaddr);
		offset = addr - region->vaddr;

		/* Access is allowed; handle it. */
		if((r=map_pagefault(vmp, region, offset, wr)) != OK) {
			printf("VM: SIGSEGV %d pagefault not handled\n", ep);
			if((s=sys_kill(vmp->vm_endpoint, SIGSEGV)) != OK)
				vm_panic("sys_kill failed", s);
			continue;
		}


		/* Pagefault is handled, so now reactivate the process. */
		if((s=sys_vmctl(ep, VMCTL_CLEAR_PAGEFAULT, r)) != OK)
			vm_panic("handle_pagefaults: sys_vmctl failed", ep);
	}

	return;
}

/*===========================================================================*
 *				handle_memory	     		     *
 *===========================================================================*/
PUBLIC void handle_memory(void)
{
	int r, s;
	endpoint_t who;
	vir_bytes mem;
	vir_bytes len;
	int wrflag;

	while((r=sys_vmctl_get_memreq(&who, &mem, &len, &wrflag)) == OK) {
		int p, r = OK;
		struct vir_region *region;
		struct vmproc *vmp;
		vir_bytes o;

		if(vm_isokendpt(who, &p) != OK)
			vm_panic("handle_memory: endpoint wrong", who);
		vmp = &vmproc[p];

		/* Page-align memory and length. */
		o = mem % VM_PAGE_SIZE;
		mem -= o;
		len += o;
		o = len % VM_PAGE_SIZE;
		if(o > 0) len += VM_PAGE_SIZE - o;

		if(!(region = map_lookup(vmp, mem))) {
			printf("VM: handle_memory: memory doesn't exist\n");
			r = EFAULT;
		} else if(mem + len > region->vaddr + region->length) {
			vm_assert(region->vaddr <= mem);
			vm_panic("handle_memory: not contained", NO_NUM);
		} else if(!(region->flags & VR_WRITABLE) && wrflag) {
			printf("VM: handle_memory: write to unwritable map\n");
			r = EFAULT;
		} else {
			vir_bytes offset;
			vm_assert(region->vaddr <= mem);
			vm_assert(!(region->flags & VR_NOPF));
			vm_assert(!(region->vaddr % VM_PAGE_SIZE));
			offset = mem - region->vaddr;

			r = map_handle_memory(vmp, region, offset, len, wrflag);
		}

		if(r != OK) {
			printf("VM: SIGSEGV %d, memory range not available\n",
				vmp->vm_endpoint);
			if((s=sys_kill(vmp->vm_endpoint, SIGSEGV)) != OK)
				vm_panic("sys_kill failed", s);
		}

		if(sys_vmctl(who, VMCTL_MEMREQ_REPLY, r) != OK)
			vm_panic("handle_memory: sys_vmctl failed", r);

		if(r != OK) {
			printf("VM: killing %d\n", vmp->vm_endpoint);
			if((s=sys_kill(vmp->vm_endpoint, SIGSEGV)) != OK)
				vm_panic("sys_kill failed", s);
		}
	}
}

