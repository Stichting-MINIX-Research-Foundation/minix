
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
#include <minix/const.h>
#include <minix/bitmap.h>
#include <minix/crtso.h>
#include <minix/rs.h>

#include <errno.h>
#include <string.h>
#include <env.h>
#include <stdio.h>

#include <memory.h>

#define _MAIN 1
#include "glo.h"
#include "proto.h"
#include "util.h"
#include "vm.h"
#include "sanitycheck.h"

extern int missing_spares;

#include <archtypes.h>
#include "../../kernel/const.h"
#include "../../kernel/config.h" 
#include "../../kernel/proc.h"

/* Table of calls and a macro to test for being in range. */
struct {
	int (*vmc_func)(message *);	/* Call handles message. */
	char *vmc_name;			/* Human-readable string. */
} vm_calls[NR_VM_CALLS];

/* Macro to verify call range and map 'high' range to 'base' range
 * (starting at 0) in one. Evaluates to zero-based call number if call
 * number is valid, returns -1 otherwise.
 */
#define CALLNUMBER(c) (((c) >= VM_RQ_BASE && 				\
			(c) < VM_RQ_BASE + ELEMENTS(vm_calls)) ?	\
			((c) - VM_RQ_BASE) : -1)

FORWARD _PROTOTYPE(int map_service, (struct rprocpub *rpub));
FORWARD _PROTOTYPE(int vm_acl_ok, (endpoint_t caller, int call));

extern int unmap_ok;

/* SEF functions and variables. */
FORWARD _PROTOTYPE( void sef_local_startup, (void) );
FORWARD _PROTOTYPE( int sef_cb_init_fresh, (int type, sef_init_info_t *info) );

/*===========================================================================*
 *				main					     *
 *===========================================================================*/
PUBLIC int main(void)
{
  message msg;
  int result, who_e;

  /* SEF local startup. */
  sef_local_startup();

  /* This is VM's main loop. */
  while (TRUE) {
	int r, c;

	SANITYCHECK(SCL_TOP);
	if(missing_spares > 0) {
		pt_cycle();	/* pagetable code wants to be called */
	}
	SANITYCHECK(SCL_DETAIL);

  	if ((r=sef_receive(ANY, &msg)) != OK)
		vm_panic("sef_receive() error", r);

	SANITYCHECK(SCL_DETAIL);

	if(msg.m_type & NOTIFY_MESSAGE) {
		switch(msg.m_source) {
			case SYSTEM:
				/* Kernel wants to have memory ranges
				 * verified, and/or pagefaults handled.
				 */
				do_memory();
				break;
			case HARDWARE:
				do_pagefaults();
				break;
			case PM_PROC_NR:
				/* PM sends a notify() on shutdown, which
				 * is OK and we ignore.
				 */
				break;
			default:
				/* No-one else should send us notifies. */
				printf("VM: ignoring notify() from %d\n",
					msg.m_source);
				break;
		}
		continue;
	}
	who_e = msg.m_source;
	c = CALLNUMBER(msg.m_type);
	result = ENOSYS; /* Out of range or restricted calls return this. */
	if(c < 0 || !vm_calls[c].vmc_func) {
		printf("VM: out of range or missing callnr %d from %d\n",
			msg.m_type, who_e);
	} else if (vm_acl_ok(who_e, c) != OK) {
		printf("VM: unauthorized %s by %d\n",
			vm_calls[c].vmc_name, who_e);
	} else {
	SANITYCHECK(SCL_FUNCTIONS);
		result = vm_calls[c].vmc_func(&msg);
	SANITYCHECK(SCL_FUNCTIONS);
	}

	/* Send reply message, unless the return code is SUSPEND,
	 * which is a pseudo-result suppressing the reply message.
	 */
	if(result != SUSPEND) {
	SANITYCHECK(SCL_DETAIL);
		msg.m_type = result;
		if((r=send(who_e, &msg)) != OK) {
			printf("VM: couldn't send %d to %d (err %d)\n",
				msg.m_type, who_e, r);
			vm_panic("send() error", NO_NUM);
		}
	SANITYCHECK(SCL_DETAIL);
	}
	SANITYCHECK(SCL_DETAIL);
  }
  return(OK);
}

/*===========================================================================*
 *			       sef_local_startup			     *
 *===========================================================================*/
PRIVATE void sef_local_startup()
{
  /* Register init callbacks. */
  sef_setcb_init_fresh(sef_cb_init_fresh);
  sef_setcb_init_restart(sef_cb_init_restart_fail);

  /* No live update support for now. */

  /* Let SEF perform startup. */
  sef_startup();
}

/*===========================================================================*
 *				sef_cb_init_fresh			     *
 *===========================================================================*/
PRIVATE int sef_cb_init_fresh(int type, sef_init_info_t *info)
{
/* Initialize the vm server. */
	int s, i;
	int click, clicksforgotten = 0;
	struct memory mem_chunks[NR_MEMS];
	struct boot_image image[NR_BOOT_PROCS];
	struct boot_image *ip;
	struct rprocpub rprocpub[NR_BOOT_PROCS];
	phys_bytes limit = 0;

#if SANITYCHECKS
	incheck = nocheck = 0;
	FIXME("VM SANITYCHECKS are on");
#endif

	vm_paged = 1;
	env_parse("vm_paged", "d", 0, &vm_paged, 0, 1);
#if SANITYCHECKS
	env_parse("vm_sanitychecklevel", "d", 0, &vm_sanitychecklevel, 0, SCL_MAX);
#endif

	/* Get chunks of available memory. */
	get_mem_chunks(mem_chunks);

	/* Initialize VM's process table. Request a copy of the system
	 * image table that is defined at the kernel level to see which
	 * slots to fill in.
	 */
	if (OK != (s=sys_getimage(image)))
		vm_panic("couldn't get image table: %d\n", s);

	/* Set table to 0. This invalidates all slots (clear VMF_INUSE). */
	memset(vmproc, 0, sizeof(vmproc));

	for(i = 0; i < ELEMENTS(vmproc); i++) {
		vmproc[i].vm_slot = i;
	}

	/* Walk through boot-time system processes that are alive
	 * now and make valid slot entries for them.
	 */
	for (ip = &image[0]; ip < &image[NR_BOOT_PROCS]; ip++) {
		phys_bytes proclimit;
		struct vmproc *vmp;

		if(ip->proc_nr >= _NR_PROCS) { vm_panic("proc", ip->proc_nr); }
		if(ip->proc_nr < 0 && ip->proc_nr != SYSTEM) continue;

#define GETVMP(v, nr)						\
		if(nr >= 0) {					\
			vmp = &vmproc[ip->proc_nr];		\
		} else if(nr == SYSTEM) {			\
			vmp = &vmproc[VMP_SYSTEM];		\
		} else {					\
			vm_panic("init: crazy proc_nr", nr);	\
		}

		/* Initialize normal process table slot or special SYSTEM
		 * table slot. Kernel memory is already reserved.
		 */
		GETVMP(vmp, ip->proc_nr);

		/* reset fields as if exited */
		clear_proc(vmp);

		/* Get memory map for this process from the kernel. */
		if ((s=get_mem_map(ip->proc_nr, vmp->vm_arch.vm_seg)) != OK)
			vm_panic("couldn't get process mem_map",s);

		/* Remove this memory from the free list. */
		reserve_proc_mem(mem_chunks, vmp->vm_arch.vm_seg);

		/* Set memory limit. */
		proclimit = CLICK2ABS(vmp->vm_arch.vm_seg[S].mem_phys +
			vmp->vm_arch.vm_seg[S].mem_len) - 1;

		if(proclimit > limit)
			limit = proclimit;

		vmp->vm_flags = VMF_INUSE;
		vmp->vm_endpoint = ip->endpoint;
		vmp->vm_stacktop =
			CLICK2ABS(vmp->vm_arch.vm_seg[S].mem_vir +
				vmp->vm_arch.vm_seg[S].mem_len);

		if (vmp->vm_arch.vm_seg[T].mem_len != 0)
			vmp->vm_flags |= VMF_SEPARATE;
	}

	/* Architecture-dependent initialization. */
	pt_init(limit);

	/* Initialize tables to all physical memory. */
	mem_init(mem_chunks);
	meminit_done = 1;

	/* Give these processes their own page table. */
	for (ip = &image[0]; ip < &image[NR_BOOT_PROCS]; ip++) {
		int s;
		struct vmproc *vmp;
		vir_bytes old_stacktop, old_stack;

		if(ip->proc_nr < 0) continue;

		GETVMP(vmp, ip->proc_nr);

               if(!(ip->flags & PROC_FULLVM))
                       continue;

		old_stack = 
			vmp->vm_arch.vm_seg[S].mem_vir +
			vmp->vm_arch.vm_seg[S].mem_len - 
			vmp->vm_arch.vm_seg[D].mem_len;

        	if(pt_new(&vmp->vm_pt) != OK)
			vm_panic("VM: no new pagetable", NO_NUM);
#define BASICSTACK VM_PAGE_SIZE
		old_stacktop = CLICK2ABS(vmp->vm_arch.vm_seg[S].mem_vir +
				vmp->vm_arch.vm_seg[S].mem_len);
		if(sys_vmctl(vmp->vm_endpoint, VMCTL_INCSP,
			VM_STACKTOP - old_stacktop) != OK) {
			vm_panic("VM: vmctl for new stack failed", NO_NUM);
		}

		FREE_MEM(vmp->vm_arch.vm_seg[D].mem_phys +
			vmp->vm_arch.vm_seg[D].mem_len,
			old_stack);

		if(proc_new(vmp,
			VM_PROCSTART,
			CLICK2ABS(vmp->vm_arch.vm_seg[T].mem_len),
			CLICK2ABS(vmp->vm_arch.vm_seg[D].mem_len),
			BASICSTACK,
			CLICK2ABS(vmp->vm_arch.vm_seg[S].mem_vir +
				vmp->vm_arch.vm_seg[S].mem_len -
				vmp->vm_arch.vm_seg[D].mem_len) - BASICSTACK,
			CLICK2ABS(vmp->vm_arch.vm_seg[T].mem_phys),
			CLICK2ABS(vmp->vm_arch.vm_seg[D].mem_phys),
				VM_STACKTOP) != OK) {
			vm_panic("failed proc_new for boot process", NO_NUM);
		}
	}

	/* Set up table of calls. */
#define CALLMAP(code, func) { int i;			      \
	if((i=CALLNUMBER(code)) < 0) { vm_panic(#code " invalid", (code)); } \
	if(i >= NR_VM_CALLS) { vm_panic(#code " invalid", (code)); } \
	vm_calls[i].vmc_func = (func); 				      \
	vm_calls[i].vmc_name = #code; 				      \
}

	/* Set call table to 0. This invalidates all calls (clear
	 * vmc_func).
	 */
	memset(vm_calls, 0, sizeof(vm_calls));

	/* Basic VM calls. */
	CALLMAP(VM_MMAP, do_mmap);
	CALLMAP(VM_MUNMAP, do_munmap);
	CALLMAP(VM_MUNMAP_TEXT, do_munmap);
	CALLMAP(VM_MAP_PHYS, do_map_phys);
	CALLMAP(VM_UNMAP_PHYS, do_unmap_phys);

	/* Calls from PM. */
	CALLMAP(VM_EXIT, do_exit);
	CALLMAP(VM_FORK, do_fork);
	CALLMAP(VM_BRK, do_brk);
	CALLMAP(VM_EXEC_NEWMEM, do_exec_newmem);
	CALLMAP(VM_PUSH_SIG, do_push_sig);
	CALLMAP(VM_WILLEXIT, do_willexit);
	CALLMAP(VM_ADDDMA, do_adddma);
	CALLMAP(VM_DELDMA, do_deldma);
	CALLMAP(VM_GETDMA, do_getdma);
	CALLMAP(VM_NOTIFY_SIG, do_notify_sig);

	/* Calls from RS */
	CALLMAP(VM_RS_SET_PRIV, do_rs_set_priv);

	/* Generic calls. */
	CALLMAP(VM_REMAP, do_remap);
	CALLMAP(VM_GETPHYS, do_get_phys);
	CALLMAP(VM_SHM_UNMAP, do_shared_unmap);
	CALLMAP(VM_GETREF, do_get_refcount);
	CALLMAP(VM_INFO, do_info);
	CALLMAP(VM_QUERY_EXIT, do_query_exit);

	/* Sanity checks */
	if(find_kernel_top() >= VM_PROCSTART)
		vm_panic("kernel loaded too high", NO_NUM);

	/* Initialize the structures for queryexit */
	init_query_exit();

	/* Unmap our own low pages. */
	unmap_ok = 1;
	_minix_unmapzero();

	/* Map all the services in the boot image. */
	if((s = sys_safecopyfrom(RS_PROC_NR, info->rproctab_gid, 0,
		(vir_bytes) rprocpub, sizeof(rprocpub), S)) != OK) {
		panic("VM", "sys_safecopyfrom failed", s);
	}
	for(i=0;i < NR_BOOT_PROCS;i++) {
		if(rprocpub[i].in_use) {
			if((s = map_service(&rprocpub[i])) != OK) {
				vm_panic("unable to map service", s);
			}
		}
	}

	return(OK);
}

/*===========================================================================*
 *		               map_service                                   *
 *===========================================================================*/
PRIVATE int map_service(rpub)
struct rprocpub *rpub;
{
/* Map a new service by initializing its call mask. */
	int r, proc_nr;

	if ((r = vm_isokendpt(rpub->endpoint, &proc_nr)) != OK) {
		return r;
	}

	/* Copy the call mask. */
	memcpy(&vmproc[proc_nr].vm_call_mask, &rpub->vm_call_mask,
		sizeof(vmproc[proc_nr].vm_call_mask));

	return(OK);
}

/*===========================================================================*
 *				vm_acl_ok				     *
 *===========================================================================*/
PRIVATE int vm_acl_ok(endpoint_t caller, int call)
{
	int n, r;

	if ((r = vm_isokendpt(caller, &n)) != OK)
		vm_panic("VM: from strange source.", caller);

	/* See if the call is allowed. */
	if (!GET_BIT(vmproc[n].vm_call_mask, call)) {
		return EPERM;
	}

	return OK;
}

