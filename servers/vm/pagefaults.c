
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
#include <minix/bitmap.h>

#include <errno.h>
#include <string.h>
#include <env.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <assert.h>

#include "glo.h"
#include "proto.h"
#include "util.h"
#include "region.h"

/*===========================================================================*
 *				pf_errstr	     		     	*
 *===========================================================================*/
char *pf_errstr(u32_t err)
{
	static char buf[100];

	sprintf(buf, "err 0x%lx ", (long)err);
	if(PFERR_NOPAGE(err)) strcat(buf, "nopage ");
	if(PFERR_PROT(err)) strcat(buf, "protection ");
	if(PFERR_WRITE(err)) strcat(buf, "write");
	if(PFERR_READ(err)) strcat(buf, "read");

	return buf;
}

struct pf_state {
        endpoint_t ep;
        vir_bytes vaddr;
	u32_t err;
};

struct hm_state {
	endpoint_t requestor;
	struct vmproc *vmp;
	vir_bytes mem;
	vir_bytes len;
	int wrflag;
};

static void pf_cont(struct vmproc *vmp, message *m, void *arg, void *statearg);

static void hm_cont(struct vmproc *vmp, message *m, void *arg, void *statearg);

static void handle_pagefault(endpoint_t ep, vir_bytes addr, u32_t err, int retry)
{
	struct vmproc *vmp;
	int s, result;
	struct vir_region *region;
	vir_bytes offset;
	int p, wr = PFERR_WRITE(err);
	int io = 0;

	if(vm_isokendpt(ep, &p) != OK)
		panic("handle_pagefault: endpoint wrong: %d", ep);

	vmp = &vmproc[p];
	assert(vmp->vm_flags & VMF_INUSE);

	/* See if address is valid at all. */
	if(!(region = map_lookup(vmp, addr, NULL))) {
		if(PFERR_PROT(err))  {
			printf("VM: pagefault: SIGSEGV %d protected addr 0x%lx; %s\n",
				ep, addr, pf_errstr(err));
		} else {
			assert(PFERR_NOPAGE(err));
			printf("VM: pagefault: SIGSEGV %d bad addr 0x%lx; %s\n",
					ep, addr, pf_errstr(err));
			sys_sysctl_stacktrace(ep);
		}
		if((s=sys_kill(vmp->vm_endpoint, SIGSEGV)) != OK)
			panic("sys_kill failed: %d", s);
		if((s=sys_vmctl(ep, VMCTL_CLEAR_PAGEFAULT, 0 /*unused*/)) != OK)
			panic("do_pagefaults: sys_vmctl failed: %d", ep);
		return;
	}

	/* If process was writing, see if it's writable. */
	if(!(region->flags & VR_WRITABLE) && wr) {
		printf("VM: pagefault: SIGSEGV %d ro map 0x%lx %s\n",
				ep, addr, pf_errstr(err));
		if((s=sys_kill(vmp->vm_endpoint, SIGSEGV)) != OK)
			panic("sys_kill failed: %d", s);
		if((s=sys_vmctl(ep, VMCTL_CLEAR_PAGEFAULT, 0 /*unused*/)) != OK)
			panic("do_pagefaults: sys_vmctl failed: %d", ep);
		return;
	}

	assert(addr >= region->vaddr);
	offset = addr - region->vaddr;

	/* Access is allowed; handle it. */
	if(retry) {
		result = map_pf(vmp, region, offset, wr, NULL, NULL, 0, &io);
		assert(result != SUSPEND);
	} else {
		struct pf_state state;
		state.ep = ep;
		state.vaddr = addr;
		state.err = err;
		result = map_pf(vmp, region, offset, wr, pf_cont,
			&state, sizeof(state), &io);
	}
	if (io)
		vmp->vm_major_page_fault++;
	else
		vmp->vm_minor_page_fault++;

	if(result == SUSPEND) {
		return;
	}

	if(result != OK) {
		printf("VM: pagefault: SIGSEGV %d pagefault not handled\n", ep);
		if((s=sys_kill(ep, SIGSEGV)) != OK)
			panic("sys_kill failed: %d", s);
		if((s=sys_vmctl(ep, VMCTL_CLEAR_PAGEFAULT, 0 /*unused*/)) != OK)
			panic("do_pagefaults: sys_vmctl failed: %d", ep);
		return;
	}

        pt_clearmapcache();

	/* Pagefault is handled, so now reactivate the process. */
	if((s=sys_vmctl(ep, VMCTL_CLEAR_PAGEFAULT, 0 /*unused*/)) != OK)
		panic("do_pagefaults: sys_vmctl failed: %d", ep);
}


static void pf_cont(struct vmproc *vmp, message *m,
        void *arg, void *statearg)
{
	struct pf_state *state = statearg;
	int p;
	if(vm_isokendpt(state->ep, &p) != OK) return;	/* signal */
	handle_pagefault(state->ep, state->vaddr, state->err, 1);
}

static void hm_cont(struct vmproc *vmp, message *m,
        void *arg, void *statearg)
{
	int r;
	struct hm_state *state = statearg;
	printf("hm_cont: result %d\n", m->VMV_RESULT);
	r = handle_memory(vmp, state->mem, state->len, state->wrflag,
		hm_cont, &state, sizeof(state));
	if(r == SUSPEND) {
		printf("VM: hm_cont: damnit: hm_cont: more SUSPEND\n");
		return;
	}

	printf("VM: hm_cont: ok, result %d, requestor %d\n", r, state->requestor);

	if(sys_vmctl(state->requestor, VMCTL_MEMREQ_REPLY, r) != OK)
		panic("hm_cont: sys_vmctl failed: %d", r);

	printf("MEMREQ_REPLY sent\n");
}

/*===========================================================================*
 *				do_pagefaults	     		     *
 *===========================================================================*/
void do_pagefaults(message *m)
{
	handle_pagefault(m->m_source, m->VPF_ADDR, m->VPF_FLAGS, 0);
}

/*===========================================================================*
 *				   do_memory	     			     *
 *===========================================================================*/
void do_memory(void)
{
	endpoint_t who, who_s, requestor;
	vir_bytes mem, mem_s;
	vir_bytes len;
	int wrflag;

	while(1) {
		int p, r = OK;
		struct vmproc *vmp;

		r = sys_vmctl_get_memreq(&who, &mem, &len, &wrflag, &who_s,
			&mem_s, &requestor);

		switch(r) {
		case VMPTYPE_CHECK:
		{
			struct hm_state state;

			if(vm_isokendpt(who, &p) != OK)
				panic("do_memory: bad endpoint: %d", who);
			vmp = &vmproc[p];


			state.vmp = vmp;
			state.mem = mem;
			state.len = len;
			state.wrflag = wrflag;
			state.requestor = requestor;

			r = handle_memory(vmp, mem, len,
				wrflag, hm_cont, &state, sizeof(state));

			break;
		}

		default:
			return;
		}

		if(r != SUSPEND) {
		   if(sys_vmctl(requestor, VMCTL_MEMREQ_REPLY, r) != OK)
			panic("do_memory: sys_vmctl failed: %d", r);
		}
	}
}

int handle_memory(struct vmproc *vmp, vir_bytes mem, vir_bytes len, int wrflag,
	vfs_callback_t callback, void *state, int statelen)
{
	struct vir_region *region;
	vir_bytes o;
	struct hm_state *hmstate = (struct hm_state *) state;

	/* Page-align memory and length. */
	o = mem % VM_PAGE_SIZE;
	mem -= o;
	len += o;
	o = len % VM_PAGE_SIZE;
	if(o > 0) len += VM_PAGE_SIZE - o;

	while(len > 0) {
		int r;
		if(!(region = map_lookup(vmp, mem, NULL))) {
#if VERBOSE
			map_printmap(vmp);
			printf("VM: do_memory: memory doesn't exist\n");
#endif
			r = EFAULT;
		} else if(!(region->flags & VR_WRITABLE) && wrflag) {
#if VERBOSE
			printf("VM: do_memory: write to unwritable map\n");
#endif
			r = EFAULT;
		} else {
			vir_bytes offset, sublen;
			assert(region->vaddr <= mem);
			assert(!(region->vaddr % VM_PAGE_SIZE));
			offset = mem - region->vaddr;
			sublen = len;
			if(offset + sublen > region->length)
				sublen = region->length - offset;
	
			if(hmstate && hmstate->requestor == VFS_PROC_NR
			   && region->def_memtype == &mem_type_mappedfile) {
				r = map_handle_memory(vmp, region, offset,
				   sublen, wrflag, NULL, NULL, 0);
			} else {
				r = map_handle_memory(vmp, region, offset,
				   sublen, wrflag, callback, state, sizeof(*state));
			}

			len -= sublen;
			mem += sublen;
		}
	
		if(r != OK) {
#if VERBOSE
			printf("VM: memory range 0x%lx-0x%lx not available in %d\n",
				mem, mem+len, vmp->vm_endpoint);
#endif
			return r;
		}
	}

	return OK;
}

