
#define _SYSTEM 1

#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/config.h>
#include <minix/const.h>
#include <minix/ds.h>
#include <minix/endpoint.h>
#include <minix/minlib.h>
#include <minix/type.h>
#include <minix/ipc.h>
#include <minix/sysutil.h>
#include <minix/syslib.h>
#include <minix/safecopies.h>
#include <minix/bitmap.h>
#include <minix/vfsif.h>

#include <machine/vmparam.h>

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <assert.h>

#include "glo.h"
#include "proto.h"
#include "util.h"
#include "region.h"

struct pf_state {
        endpoint_t ep;
        vir_bytes vaddr;
	u32_t err;
};

struct hm_state {
	endpoint_t caller;	/* KERNEL or process? if NONE, no callback */
	endpoint_t requestor;	/* on behalf of whom? */
	int transid;		/* VFS transaction id if valid */
	struct vmproc *vmp;	/* target address space */
	vir_bytes mem, len;	/* memory range */
	int wrflag;		/* must it be writable or not */
	int valid;		/* sanity check */
	int vfs_avail;		/* may vfs be called to satisfy this range? */
#define VALID	0xc0ff1
};

static void handle_memory_continue(struct vmproc *vmp, message *m,
        void *arg, void *statearg);
static int handle_memory_step(struct hm_state *hmstate, int retry);
static void handle_memory_final(struct hm_state *state, int result);

/*===========================================================================*
 *				pf_errstr	     		     	*
 *===========================================================================*/
char *pf_errstr(u32_t err)
{
	static char buf[100];

	snprintf(buf, sizeof(buf), "err 0x%lx ", (long)err);
	if(PFERR_NOPAGE(err)) strcat(buf, "nopage ");
	if(PFERR_PROT(err)) strcat(buf, "protection ");
	if(PFERR_WRITE(err)) strcat(buf, "write");
	if(PFERR_READ(err)) strcat(buf, "read");

	return buf;
}

static void pf_cont(struct vmproc *vmp, message *m, void *arg, void *statearg);

static void handle_memory_continue(struct vmproc *vmp, message *m, void *arg, void *statearg);

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
			sys_diagctl_stacktrace(ep);
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

static void handle_memory_continue(struct vmproc *vmp, message *m,
        void *arg, void *statearg)
{
	int r;
	struct hm_state *state = statearg;
	assert(state);
	assert(state->caller != NONE);
	assert(state->valid == VALID);

	if(m->VMV_RESULT != OK) {
		printf("VM: handle_memory_continue: vfs request failed\n");
		handle_memory_final(state, m->VMV_RESULT);
		return;
	}

	r = handle_memory_step(state, TRUE /*retry*/);

	assert(state->valid == VALID);

	if(r == SUSPEND) {
		return;
	}

	assert(state->valid == VALID);

	handle_memory_final(state, r);
}

static void handle_memory_final(struct hm_state *state, int result)
{
	int r, flag;

	assert(state);
	assert(state->valid == VALID);

	if(state->caller == KERNEL) {
		if((r=sys_vmctl(state->requestor, VMCTL_MEMREQ_REPLY, result)) != OK)
			panic("handle_memory_continue: sys_vmctl failed: %d", r);
	} else if(state->caller != NONE) {
		/* Send a reply msg */
		message msg;
		memset(&msg, 0, sizeof(msg));
		msg.m_type = result;

		if(IS_VFS_FS_TRANSID(state->transid)) {
			assert(state->caller == VFS_PROC_NR);
			/* If a transaction ID was set, reset it */
			msg.m_type = TRNS_ADD_ID(msg.m_type, state->transid);
			flag = AMF_NOREPLY;
		} else
			flag = 0;

		/*
		 * Use AMF_NOREPLY only if there was a transaction ID, which
		 * signifies that VFS issued the request asynchronously.
		 */
		if(asynsend3(state->caller, &msg, flag) != OK) {
			panic("handle_memory_final: asynsend3 failed");
		}

		assert(state->valid == VALID);

		/* fail fast if anyone tries to access this state again */
		memset(state, 0, sizeof(*state));
	}
}

/*===========================================================================*
 *				do_pagefaults	     		     *
 *===========================================================================*/
void do_pagefaults(message *m)
{
	handle_pagefault(m->m_source, m->VPF_ADDR, m->VPF_FLAGS, 0);
}

int handle_memory_once(struct vmproc *vmp, vir_bytes mem, vir_bytes len,
	int wrflag)
{
	int r;
	r = handle_memory_start(vmp, mem, len, wrflag, NONE, NONE, 0, 0);
	assert(r != SUSPEND);
	return r;
}

int handle_memory_start(struct vmproc *vmp, vir_bytes mem, vir_bytes len,
	int wrflag, endpoint_t caller, endpoint_t requestor, int transid,
	int vfs_avail)
{
	int r;
	struct hm_state state;
	vir_bytes o;

	if((o = mem % PAGE_SIZE)) {
		mem -= o;
		len += o;
	}

	len = roundup(len, PAGE_SIZE);

	state.vmp = vmp;
	state.mem = mem;
	state.len = len;
	state.wrflag = wrflag;
	state.requestor = requestor;
	state.caller = caller;
	state.transid = transid;
	state.valid = VALID;
	state.vfs_avail = vfs_avail;

	r = handle_memory_step(&state, FALSE /*retry*/);

	if(r == SUSPEND) {
		assert(caller != NONE);
		assert(vfs_avail);
	} else {
		handle_memory_final(&state, r);
	}

	return r;
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
			int transid = 0;
			int vfs_avail;

			if(vm_isokendpt(who, &p) != OK)
				panic("do_memory: bad endpoint: %d", who);
			vmp = &vmproc[p];

			assert(!IS_VFS_FS_TRANSID(transid));

			/* is VFS blocked? */
			if(requestor == VFS_PROC_NR) vfs_avail = 0;
			else vfs_avail = 1;

			handle_memory_start(vmp, mem, len, wrflag,
				KERNEL, requestor, transid, vfs_avail);

			break;
		}

		default:
			return;
		}
	}
}

static int handle_memory_step(struct hm_state *hmstate, int retry)
{
	struct vir_region *region;
	vir_bytes offset, length, sublen;
	int r;

	/* Page-align memory and length. */
	assert(hmstate);
	assert(hmstate->valid == VALID);
	assert(!(hmstate->mem % VM_PAGE_SIZE));
	assert(!(hmstate->len % VM_PAGE_SIZE));

	while(hmstate->len > 0) {
		if(!(region = map_lookup(hmstate->vmp, hmstate->mem, NULL))) {
#if VERBOSE
			map_printmap(hmstate->vmp);
			printf("VM: do_memory: memory doesn't exist\n");
#endif
			return EFAULT;
		} else if(!(region->flags & VR_WRITABLE) && hmstate->wrflag) {
#if VERBOSE
			printf("VM: do_memory: write to unwritable map\n");
#endif
			return EFAULT;
		}

		assert(region->vaddr <= hmstate->mem);
		assert(!(region->vaddr % VM_PAGE_SIZE));
		offset = hmstate->mem - region->vaddr;
		length = hmstate->len;
		if (offset + length > region->length)
			length = region->length - offset;

		/*
		 * Handle one page at a time.  While it seems beneficial to
		 * handle multiple pages in one go, the opposite is true:
		 * map_handle_memory will handle one page at a time anyway, and
		 * if we give it the whole range multiple times, it will have
		 * to recheck pages it already handled.  In addition, in order
		 * to handle one-shot pages, we need to know whether we are
		 * retrying a single page, and that is not possible if this is
		 * hidden in map_handle_memory.
		 */
		while (length > 0) {
			sublen = VM_PAGE_SIZE;

			assert(sublen <= length);
			assert(offset + sublen <= region->length);

			/*
			 * Upon the second try for this range, do not allow
			 * calling into VFS again.  This prevents eternal loops
			 * in case the FS messes up, and allows one-shot pages
			 * to be mapped in on the second call.
			 */
			if((region->def_memtype == &mem_type_mappedfile &&
			    (!hmstate->vfs_avail || retry)) ||
			    hmstate->caller == NONE) {
				r = map_handle_memory(hmstate->vmp, region,
				    offset, sublen, hmstate->wrflag, NULL,
				    NULL, 0);
				assert(r != SUSPEND);
			} else {
				r = map_handle_memory(hmstate->vmp, region,
				    offset, sublen, hmstate->wrflag,
				    handle_memory_continue, hmstate,
				    sizeof(*hmstate));
			}

			if(r != OK) return r;

			hmstate->len -= sublen;
			hmstate->mem += sublen;

			offset += sublen;
			length -= sublen;
			retry = FALSE;
		}
	}

	return OK;
}

