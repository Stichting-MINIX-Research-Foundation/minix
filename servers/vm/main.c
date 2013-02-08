
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

#include <libexec.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <env.h>
#include <stdio.h>
#include <assert.h>

#define _MAIN 1
#include "glo.h"
#include "proto.h"
#include "util.h"
#include "vm.h"
#include "sanitycheck.h"

extern int missing_spares;

#include <machine/archtypes.h>
#include <sys/param.h>
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

static int map_service(struct rprocpub *rpub);
static int vm_acl_ok(endpoint_t caller, int call);
static int do_rs_init(message *m);

/* SEF functions and variables. */
static void sef_cb_signal_handler(int signo);

void init_vm(void);

/*===========================================================================*
 *				main					     *
 *===========================================================================*/
int main(void)
{
  message msg;
  int result, who_e, rcv_sts;
  int caller_slot;

  /* Initialize system so that all processes are runnable */
  init_vm();

  /* Register init callbacks. */
  sef_setcb_init_restart(sef_cb_init_fail);
  sef_setcb_signal_handler(sef_cb_signal_handler);

  /* Let SEF perform startup. */
  sef_startup();

  SANITYCHECK(SCL_TOP);

  /* This is VM's main loop. */
  while (TRUE) {
	int r, c;

	SANITYCHECK(SCL_TOP);
	if(missing_spares > 0) {
		pt_cycle();	/* pagetable code wants to be called */
	}

  	if ((r=sef_receive_status(ANY, &msg, &rcv_sts)) != OK)
		panic("sef_receive_status() error: %d", r);

	if (is_ipc_notify(rcv_sts)) {
		/* Unexpected notify(). */
		printf("VM: ignoring notify() from %d\n", msg.m_source);
		continue;
	}
	who_e = msg.m_source;
	if(vm_isokendpt(who_e, &caller_slot) != OK)
		panic("invalid caller %d", who_e);
	c = CALLNUMBER(msg.m_type);
	result = ENOSYS; /* Out of range or restricted calls return this. */
	
	if(msg.m_type == RS_INIT && msg.m_source == RS_PROC_NR) {
		result = do_rs_init(&msg);
	} else if (msg.m_type == VM_PAGEFAULT) {
		if (!IPC_STATUS_FLAGS_TEST(rcv_sts, IPC_FLG_MSG_FROM_KERNEL)) {
			printf("VM: process %d faked VM_PAGEFAULT "
					"message!\n", msg.m_source);
		}
		do_pagefaults(&msg);
		pt_clearmapcache();
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
  }
  return(OK);
}

static int do_rs_init(message *m)
{
	int s, i;
	static struct rprocpub rprocpub[NR_BOOT_PROCS];

	/* Map all the services in the boot image. */
	if((s = sys_safecopyfrom(RS_PROC_NR, m->RS_INIT_RPROCTAB_GID, 0,
		(vir_bytes) rprocpub, sizeof(rprocpub))) != OK) {
		panic("vm: sys_safecopyfrom (rs) failed: %d", s);
	}

	for(i=0;i < NR_BOOT_PROCS;i++) {
		if(rprocpub[i].in_use) {
			if((s = map_service(&rprocpub[i])) != OK) {
				panic("unable to map service: %d", s);
			}
		}
	}

	/* RS expects this response that it then again wants to reply to: */
	m->RS_INIT_RESULT = OK;
	sendrec(RS_PROC_NR, m);

	return(SUSPEND);
}

struct vmproc *init_proc(endpoint_t ep_nr)
{
	static struct boot_image *ip;

	for (ip = &kernel_boot_info.boot_procs[0];
		ip < &kernel_boot_info.boot_procs[NR_BOOT_PROCS]; ip++) {
		struct vmproc *vmp;

		if(ip->proc_nr != ep_nr) continue;

		if(ip->proc_nr >= _NR_PROCS || ip->proc_nr < 0)
			panic("proc: %d", ip->proc_nr);

		vmp = &vmproc[ip->proc_nr];
		assert(!(vmp->vm_flags & VMF_INUSE));	/* no double procs */
		clear_proc(vmp);
		vmp->vm_flags = VMF_INUSE;
		vmp->vm_endpoint = ip->endpoint;
		vmp->vm_boot = ip;

		return vmp;
	}

	panic("no init_proc");
}

struct vm_exec_info {
	struct exec_info execi;
	struct boot_image *ip;
	struct vmproc *vmp;
};

static int libexec_copy_physcopy(struct exec_info *execi,
        off_t off, off_t vaddr, size_t len)
{
	vir_bytes end;
	struct vm_exec_info *ei = execi->opaque;
	end = ei->ip->start_addr + ei->ip->len;
        assert(ei->ip->start_addr + off + len <= end);
        return sys_physcopy(NONE, ei->ip->start_addr + off,
		execi->proc_e, vaddr, len);
}

static void boot_alloc(struct exec_info *execi, off_t vaddr,
	size_t len, int flags)
{
	struct vmproc *vmp = ((struct vm_exec_info *) execi->opaque)->vmp;

	if(!(map_page_region(vmp, vaddr, 0, len,
		VR_ANON | VR_WRITABLE | VR_UNINITIALIZED, flags,
		&mem_type_anon))) {
		panic("VM: exec: map_page_region for boot process failed");
	}
}

static int libexec_alloc_vm_prealloc(struct exec_info *execi,
	off_t vaddr, size_t len)
{
	boot_alloc(execi, vaddr, len, MF_PREALLOC);
	return OK;
}

static int libexec_alloc_vm_ondemand(struct exec_info *execi,
	off_t vaddr, size_t len)
{
	boot_alloc(execi, vaddr, len, 0);
	return OK;
}

void exec_bootproc(struct vmproc *vmp, struct boot_image *ip)
{
	struct vm_exec_info vmexeci;
	struct exec_info *execi = &vmexeci.execi;
	char hdr[VM_PAGE_SIZE];

	memset(&vmexeci, 0, sizeof(vmexeci));

	if(pt_new(&vmp->vm_pt) != OK)
		panic("VM: no new pagetable");

	if(pt_bind(&vmp->vm_pt, vmp) != OK)
		panic("VM: pt_bind failed");

	if(sys_physcopy(NONE, ip->start_addr, SELF,
		(vir_bytes) hdr, sizeof(hdr)) != OK)
		panic("can't look at boot proc header");

        execi->stack_high = kernel_boot_info.user_sp;
        execi->stack_size = DEFAULT_STACK_LIMIT;
        execi->proc_e = vmp->vm_endpoint;
        execi->hdr = hdr;
        execi->hdr_len = sizeof(hdr);
        strlcpy(execi->progname, ip->proc_name, sizeof(execi->progname));
        execi->frame_len = 0;
	execi->opaque = &vmexeci;
	execi->filesize = ip->len;

	vmexeci.ip = ip;
	vmexeci.vmp = vmp;

        /* callback functions and data */
        execi->copymem = libexec_copy_physcopy;
        execi->clearproc = NULL;
        execi->clearmem = libexec_clear_sys_memset;
        execi->allocmem_prealloc = libexec_alloc_vm_prealloc;
        execi->allocmem_ondemand = libexec_alloc_vm_ondemand;

	if(libexec_load_elf(execi) != OK)
		panic("vm: boot process load of %d failed\n", vmp->vm_endpoint);

        if(sys_exec(vmp->vm_endpoint, (char *) execi->stack_high - 12,
		(char *) ip->proc_name, execi->pc) != OK)
		panic("vm: boot process exec of %d failed\n", vmp->vm_endpoint);

	/* make it runnable */
	if(sys_vmctl(vmp->vm_endpoint, VMCTL_BOOTINHIBIT_CLEAR, 0) != OK)
		panic("VMCTL_BOOTINHIBIT_CLEAR failed");
}

void init_vm(void)
{
	int s, i;
	static struct memory mem_chunks[NR_MEMS];
	static struct boot_image *ip;
	extern void __minix_init(void);
	multiboot_module_t *mod;
	vir_bytes kern_dyn, kern_static;

#if SANITYCHECKS
	incheck = nocheck = 0;
#endif

	/* Retrieve various crucial boot parameters */
	if(OK != (s=sys_getkinfo(&kernel_boot_info))) {
		panic("couldn't get bootinfo: %d", s);
	}

	/* Sanity check */
	assert(kernel_boot_info.mmap_size > 0);
	assert(kernel_boot_info.mods_with_kernel > 0);

#if SANITYCHECKS
	env_parse("vm_sanitychecklevel", "d", 0, &vm_sanitychecklevel, 0, SCL_MAX);
#endif

	/* Get chunks of available memory. */
	get_mem_chunks(mem_chunks);

	/* Set table to 0. This invalidates all slots (clear VMF_INUSE). */
	memset(vmproc, 0, sizeof(vmproc));

	for(i = 0; i < ELEMENTS(vmproc); i++) {
		vmproc[i].vm_slot = i;
	}

	/* region management initialization. */
	map_region_init();

	/* Initialize tables to all physical memory. */
	mem_init(mem_chunks);

	/* Architecture-dependent initialization. */
	init_proc(VM_PROC_NR);
	pt_init();

	/* The kernel's freelist does not include boot-time modules; let
	 * the allocator know that the total memory is bigger.
	 */
	for (mod = &kernel_boot_info.module_list[0];
		mod < &kernel_boot_info.module_list[kernel_boot_info.mods_with_kernel-1]; mod++) {
		phys_bytes len = mod->mod_end-mod->mod_start+1;
		len = roundup(len, VM_PAGE_SIZE);
		mem_add_total_pages(len/VM_PAGE_SIZE);
	}

	kern_dyn = kernel_boot_info.kernel_allocated_bytes_dynamic;
	kern_static = kernel_boot_info.kernel_allocated_bytes;
	kern_static = roundup(kern_static, VM_PAGE_SIZE);
	mem_add_total_pages((kern_dyn + kern_static)/VM_PAGE_SIZE);

	/* Give these processes their own page table. */
	for (ip = &kernel_boot_info.boot_procs[0];
		ip < &kernel_boot_info.boot_procs[NR_BOOT_PROCS]; ip++) {
		struct vmproc *vmp;

		if(ip->proc_nr < 0) continue;

		assert(ip->start_addr);

		/* VM has already been set up by the kernel and pt_init().
		 * Any other boot process is already in memory and is set up
		 * here.
		 */
		if(ip->proc_nr == VM_PROC_NR) continue;

		vmp = init_proc(ip->proc_nr);

		exec_bootproc(vmp, ip);

		/* Free the file blob */
		assert(!(ip->start_addr % VM_PAGE_SIZE));
		ip->len = roundup(ip->len, VM_PAGE_SIZE);
		free_mem(ABS2CLICK(ip->start_addr), ABS2CLICK(ip->len));
	}

	/* Set up table of calls. */
#define CALLMAP(code, func) { int i;		      \
	i=CALLNUMBER(code);				\
	assert(i >= 0);					\
	assert(i < NR_VM_CALLS);			\
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
	CALLMAP(VM_MAP_PHYS, do_map_phys);
	CALLMAP(VM_UNMAP_PHYS, do_munmap);

	/* Calls from PM. */
	CALLMAP(VM_EXIT, do_exit);
	CALLMAP(VM_FORK, do_fork);
	CALLMAP(VM_BRK, do_brk);
	CALLMAP(VM_WILLEXIT, do_willexit);
	CALLMAP(VM_NOTIFY_SIG, do_notify_sig);

	/* Calls from RS */
	CALLMAP(VM_RS_SET_PRIV, do_rs_set_priv);
	CALLMAP(VM_RS_UPDATE, do_rs_update);
	CALLMAP(VM_RS_MEMCTL, do_rs_memctl);

	/* Calls from RS/VFS */
	CALLMAP(VM_PROCCTL, do_procctl);

	/* Generic calls. */
	CALLMAP(VM_REMAP, do_remap);
	CALLMAP(VM_REMAP_RO, do_remap);
	CALLMAP(VM_GETPHYS, do_get_phys);
	CALLMAP(VM_SHM_UNMAP, do_munmap);
	CALLMAP(VM_GETREF, do_get_refcount);
	CALLMAP(VM_INFO, do_info);
	CALLMAP(VM_QUERY_EXIT, do_query_exit);
	CALLMAP(VM_WATCH_EXIT, do_watch_exit);
	CALLMAP(VM_FORGETBLOCKS, do_forgetblocks);
	CALLMAP(VM_FORGETBLOCK, do_forgetblock);
	CALLMAP(VM_YIELDBLOCKGETBLOCK, do_yieldblockgetblock);

	/* Initialize the structures for queryexit */
	init_query_exit();

	/* Acquire kernel ipc vectors that weren't available
	 * before VM had determined kernel mappings
	 */
	__minix_init();
}

/*===========================================================================*
 *		            sef_cb_signal_handler                            *
 *===========================================================================*/
static void sef_cb_signal_handler(int signo)
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

	pt_clearmapcache();
}

/*===========================================================================*
 *		               map_service                                   *
 *===========================================================================*/
static int map_service(rpub)
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
static int vm_acl_ok(endpoint_t caller, int call)
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

