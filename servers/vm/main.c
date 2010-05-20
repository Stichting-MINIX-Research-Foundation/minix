
#define _POSIX_SOURCE      1
#define _MINIX             1
#define _SYSTEM            1

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
#include <assert.h>

#include <memory.h>

#define _MAIN 1
#include "glo.h"
#include "proto.h"
#include "util.h"
#include "vm.h"
#include "sanitycheck.h"

extern int missing_spares;

#include <machine/archtypes.h>
#include "kernel/const.h"
#include "kernel/config.h"
#include "kernel/proc.h"

#include <signal.h>

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
FORWARD _PROTOTYPE( void sef_cb_signal_handler, (int signo) );

/*===========================================================================*
 *				main					     *
 *===========================================================================*/
PUBLIC int main(void)
{
  message msg;
  int result, who_e, rcv_sts;
  sigset_t sigset;
  int caller_slot;
  struct vmproc *vmp_caller;

/*XXX*/vmmcall(0x1234560c, 0, 0);
	
  /* SEF local startup. */
  sef_local_startup();

/*XXX*/vmmcall(0x1234560c, 0, 1);
  SANITYCHECK(SCL_TOP);

  /* This is VM's main loop. */
/*XXX*/vmmcall(0x1234560c, 0, 2);
  while (TRUE) {
	int r, c;

/*XXX*/vmmcall(0x1234560c, 0, 3);
	SANITYCHECK(SCL_TOP);
	if(missing_spares > 0) {
		pt_cycle();	/* pagetable code wants to be called */
	}

/*XXX*/vmmcall(0x1234560c, 0, 4);
  	if ((r=sef_receive_status(ANY, &msg, &rcv_sts)) != OK)
		panic("sef_receive_status() error: %d", r);

/*XXX*/vmmcall(0x1234560c, msg.m_type, 5);
	if (is_ipc_notify(rcv_sts)) {
		/* Unexpected notify(). */
		printf("VM: ignoring notify() from %d\n", msg.m_source);
		continue;
	}
	who_e = msg.m_source;
	if(vm_isokendpt(who_e, &caller_slot) != OK)
		panic("invalid caller", who_e);
	vmp_caller = &vmproc[caller_slot];
	c = CALLNUMBER(msg.m_type);
/*XXX*/vmmcall(0x1234560c, c, 6);
	result = ENOSYS; /* Out of range or restricted calls return this. */
	if (msg.m_type == VM_PAGEFAULT) {
		if (!IPC_STATUS_FLAGS_TEST(rcv_sts, IPC_FLG_MSG_FROM_KERNEL)) {
			printf("VM: process %d faked VM_PAGEFAULT "
					"message!\n", msg.m_source);
		}
		do_pagefaults(&msg);
		/*
		 * do not reply to this call, the caller is unblocked by
		 * a sys_vmctl() call in do_pagefaults if success. VM panics
		 * otherwise
		 */
		continue;
	} else if(c < 0 || !vm_calls[c].vmc_func) {
		/* out of range or missing callnr */
	} else {
		if (vm_acl_ok(who_e, c) != OK) {
			printf("VM: unauthorized %s by %d\n",
					vm_calls[c].vmc_name, who_e);
		} else {
			SANITYCHECK(SCL_FUNCTIONS);
			result = vm_calls[c].vmc_func(&msg);
			SANITYCHECK(SCL_FUNCTIONS);
		}
	}
/*XXX*/vmmcall(0x1234560c, result, 7);

	/* Send reply message, unless the return code is SUSPEND,
	 * which is a pseudo-result suppressing the reply message.
	 */
	if(result != SUSPEND) {
		msg.m_type = result;
		if((r=send(who_e, &msg)) != OK) {
			printf("VM: couldn't send %d to %d (err %d)\n",
				msg.m_type, who_e, r);
			panic("send() error");
		}
	}
/*XXX*/vmmcall(0x1234560c, 0, 8);
  }
/*XXX*/vmmcall(0x1234560c, 0, 9);
  return(OK);
}

/*===========================================================================*
 *			       sef_local_startup			     *
 *===========================================================================*/
PRIVATE void sef_local_startup()
{
  /* Register init callbacks. */
/*XXX*/vmmcall(0x1234560c, 10, 10);
  sef_setcb_init_fresh(sef_cb_init_fresh);
/*XXX*/vmmcall(0x1234560c, 11, 11);
  sef_setcb_init_restart(sef_cb_init_fail);

  /* No live update support for now. */

  /* Register signal callbacks. */
/*XXX*/vmmcall(0x1234560c, 12, 12);
  sef_setcb_signal_handler(sef_cb_signal_handler);

  /* Let SEF perform startup. */
/*XXX*/vmmcall(0x1234560c, 13, 13);
  sef_startup();
/*XXX*/vmmcall(0x1234560c, 14, 14);
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
#endif
	
/*XXX*/vmmcall(0x1234560c, 0, 27);
	vm_paged = 1;
	env_parse("vm_paged", "d", 0, &vm_paged, 0, 1);
#if SANITYCHECKS
	env_parse("vm_sanitychecklevel", "d", 0, &vm_sanitychecklevel, 0, SCL_MAX);
#endif

	/* Get chunks of available memory. */
/*XXX*/vmmcall(0x1234560c, 0, 28);
	get_mem_chunks(mem_chunks);

	/* Initialize VM's process table. Request a copy of the system
	 * image table that is defined at the kernel level to see which
	 * slots to fill in.
	 */
/*XXX*/vmmcall(0x1234560c, 0, 28);
	if (OK != (s=sys_getimage(image)))
		panic("couldn't get image table: %d", s);

	/* Set table to 0. This invalidates all slots (clear VMF_INUSE). */
/*XXX*/vmmcall(0x1234560c, 0, 29);
	memset(vmproc, 0, sizeof(vmproc));

/*XXX*/vmmcall(0x1234560c, 0, 30);
	for(i = 0; i < ELEMENTS(vmproc); i++) {
		vmproc[i].vm_slot = i;
	}

	/* Walk through boot-time system processes that are alive
	 * now and make valid slot entries for them.
	 */
/*XXX*/vmmcall(0x1234560c, 0, 31);
	for (ip = &image[0]; ip < &image[NR_BOOT_PROCS]; ip++) {
		phys_bytes proclimit;
		struct vmproc *vmp;

/*XXX*/vmmcall(0x1234560c, 0, 32);
		if(ip->proc_nr >= _NR_PROCS) { panic("proc: %d", ip->proc_nr); }
		if(ip->proc_nr < 0 && ip->proc_nr != SYSTEM) continue;

#define GETVMP(v, nr)						\
		if(nr >= 0) {					\
			vmp = &vmproc[ip->proc_nr];		\
		} else if(nr == SYSTEM) {			\
			vmp = &vmproc[VMP_SYSTEM];		\
		} else {					\
			panic("init: crazy proc_nr: %d", nr);	\
		}

		/* Initialize normal process table slot or special SYSTEM
		 * table slot. Kernel memory is already reserved.
		 */
/*XXX*/vmmcall(0x1234560c, 0, 33);
		GETVMP(vmp, ip->proc_nr);

		/* reset fields as if exited */
/*XXX*/vmmcall(0x1234560c, 0, 34);
		clear_proc(vmp);

		/* Get memory map for this process from the kernel. */
/*XXX*/vmmcall(0x1234560c, 0, 35);
		if ((s=get_mem_map(ip->proc_nr, vmp->vm_arch.vm_seg)) != OK)
			panic("couldn't get process mem_map: %d", s);

		/* Remove this memory from the free list. */
/*XXX*/vmmcall(0x1234560c, 0, 36);
		reserve_proc_mem(mem_chunks, vmp->vm_arch.vm_seg);

		/* Set memory limit. */
/*XXX*/vmmcall(0x1234560c, 0, 37);
		proclimit = CLICK2ABS(vmp->vm_arch.vm_seg[S].mem_phys +
			vmp->vm_arch.vm_seg[S].mem_len) - 1;

/*XXX*/vmmcall(0x1234560c, 0, 38);
		if(proclimit > limit)
			limit = proclimit;

		vmp->vm_flags = VMF_INUSE;
		vmp->vm_endpoint = ip->endpoint;
		vmp->vm_stacktop =
			CLICK2ABS(vmp->vm_arch.vm_seg[S].mem_vir +
				vmp->vm_arch.vm_seg[S].mem_len);

		if (vmp->vm_arch.vm_seg[T].mem_len != 0)
			vmp->vm_flags |= VMF_SEPARATE;
/*XXX*/vmmcall(0x1234560c, 0, 39);
	}

	/* Architecture-dependent initialization. */
/*XXX*/vmmcall(0x1234560c, 0, 40);
	pt_init(limit);

	/* Initialize tables to all physical memory. */
/*XXX*/vmmcall(0x1234560c, 0, 41);
	mem_init(mem_chunks);
	meminit_done = 1;

	/* Give these processes their own page table. */
/*XXX*/vmmcall(0x1234560c, 0, 42);
	for (ip = &image[0]; ip < &image[NR_BOOT_PROCS]; ip++) {
		int s;
		struct vmproc *vmp;
		vir_bytes old_stacktop, old_stack;

/*XXX*/vmmcall(0x1234560c, 0, 43);
		if(ip->proc_nr < 0) continue;

		GETVMP(vmp, ip->proc_nr);

               if(!(ip->flags & PROC_FULLVM))
                       continue;

		old_stack = 
			vmp->vm_arch.vm_seg[S].mem_vir +
			vmp->vm_arch.vm_seg[S].mem_len - 
			vmp->vm_arch.vm_seg[D].mem_len;

/*XXX*/vmmcall(0x1234560c, 0, 44);
        	if(pt_new(&vmp->vm_pt) != OK)
			panic("VM: no new pagetable");
#define BASICSTACK VM_PAGE_SIZE
/*XXX*/vmmcall(0x1234560c, 0, 77);
		old_stacktop = CLICK2ABS(vmp->vm_arch.vm_seg[S].mem_vir +
				vmp->vm_arch.vm_seg[S].mem_len);
/*XXX*/vmmcall(0x1234560c, old_stacktop, 78);
		if(sys_vmctl(vmp->vm_endpoint, VMCTL_INCSP,
			VM_STACKTOP - old_stacktop) != OK) {
/*XXX*/vmmcall(0x1234560c, 0, 79);
			panic("VM: vmctl for new stack failed");
		}

/*XXX*/vmmcall(0x1234560c, 0, 45);
		free_mem(vmp->vm_arch.vm_seg[D].mem_phys +
			vmp->vm_arch.vm_seg[D].mem_len,
			old_stack);

/*XXX*/vmmcall(0x1234560c, 0, 46);
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
				VM_STACKTOP, 0) != OK) {
			panic("failed proc_new for boot process");
		}
/*XXX*/vmmcall(0x1234560c, 0, 47);
	}

	/* Set up table of calls. */
#define CALLMAP(code, func) { int i;			      \
	if((i=CALLNUMBER(code)) < 0) { panic(#code " invalid: %d", (code)); } \
	if(i >= NR_VM_CALLS) { panic(#code " invalid: %d", (code)); } \
	vm_calls[i].vmc_func = (func); 				      \
	vm_calls[i].vmc_name = #code; 				      \
}

	/* Set call table to 0. This invalidates all calls (clear
	 * vmc_func).
	 */
/*XXX*/vmmcall(0x1234560c, 0, 48);
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
	CALLMAP(VM_RS_UPDATE, do_rs_update);

	/* Generic calls. */
	CALLMAP(VM_REMAP, do_remap);
	CALLMAP(VM_GETPHYS, do_get_phys);
	CALLMAP(VM_SHM_UNMAP, do_shared_unmap);
	CALLMAP(VM_GETREF, do_get_refcount);
	CALLMAP(VM_INFO, do_info);
	CALLMAP(VM_QUERY_EXIT, do_query_exit);
	CALLMAP(VM_FORGETBLOCKS, do_forgetblocks);
	CALLMAP(VM_FORGETBLOCK, do_forgetblock);
	CALLMAP(VM_YIELDBLOCKGETBLOCK, do_yieldblockgetblock);

	/* Sanity checks */
/*XXX*/vmmcall(0x1234560c, 0, 49);
	if(find_kernel_top() >= VM_PROCSTART)
		panic("kernel loaded too high");

	/* Initialize the structures for queryexit */
/*XXX*/vmmcall(0x1234560c, 0, 50);
	init_query_exit();

	/* Unmap our own low pages. */
/*XXX*/vmmcall(0x1234560c, 0, 51);
	unmap_ok = 1;
	_minix_unmapzero();

	/* Map all the services in the boot image. */
/*XXX*/vmmcall(0x1234560c, 0, 52);
	if((s = sys_safecopyfrom(RS_PROC_NR, info->rproctab_gid, 0,
		(vir_bytes) rprocpub, sizeof(rprocpub), S)) != OK) {
		panic("sys_safecopyfrom failed: %d", s);
	}
	for(i=0;i < NR_BOOT_PROCS;i++) {
		if(rprocpub[i].in_use) {
			if((s = map_service(&rprocpub[i])) != OK) {
				panic("unable to map service: %d", s);
			}
		}
	}
/*XXX*/vmmcall(0x1234560c, 0, 53);

	return(OK);
}

/*===========================================================================*
 *		            sef_cb_signal_handler                            *
 *===========================================================================*/
PRIVATE void sef_cb_signal_handler(int signo)
{
	/* Check for known kernel signals, ignore anything else. */
	switch(signo) {
		/* There is a pending memory request from the kernel. */
		case SIGKMEM:
			do_memory();
		break;
	}

	/* It can happen that we get stuck receiving signals
	 * without sef_receive() returning. We could need more memory
	 * though.
	 */
	if(missing_spares > 0) {
		pt_cycle();	/* pagetable code wants to be called */
	}
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
		panic("VM: from strange source: %d", caller);

	/* See if the call is allowed. */
	if (!GET_BIT(vmproc[n].vm_call_mask, call)) {
		return EPERM;
	}

	return OK;
}

