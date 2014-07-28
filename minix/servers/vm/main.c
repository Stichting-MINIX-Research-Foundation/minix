
#define _SYSTEM		1

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
#include <minix/const.h>
#include <minix/bitmap.h>
#include <minix/rs.h>
#include <minix/vfsif.h>

#include <sys/exec.h>

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
#include <lib.h>

/* Table of calls and a macro to test for being in range. */
struct {
	int (*vmc_func)(message *);	/* Call handles message. */
	const char *vmc_name;			/* Human-readable string. */
} vm_calls[NR_VM_CALLS];

/* Macro to verify call range and map 'high' range to 'base' range
 * (starting at 0) in one. Evaluates to zero-based call number if call
 * number is valid, returns -1 otherwise.
 */
#define CALLNUMBER(c) (((c) >= VM_RQ_BASE && 				\
			(c) < VM_RQ_BASE + ELEMENTS(vm_calls)) ?	\
			((c) - VM_RQ_BASE) : -1)

static int map_service(struct rprocpub *rpub);
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
	int type;
	int transid = 0;	/* VFS transid if any */

	SANITYCHECK(SCL_TOP);
	if(missing_spares > 0) {
		alloc_cycle();	/* mem alloc code wants to be called */
	}

  	if ((r=sef_receive_status(ANY, &msg, &rcv_sts)) != OK)
		panic("sef_receive_status() error: %d", r);

	if (is_ipc_notify(rcv_sts)) {
		/* Unexpected ipc_notify(). */
		printf("VM: ignoring ipc_notify() from %d\n", msg.m_source);
		continue;
	}
	who_e = msg.m_source;
	if(vm_isokendpt(who_e, &caller_slot) != OK)
		panic("invalid caller %d", who_e);

	/* We depend on this being false for the initialized value. */
	assert(!IS_VFS_FS_TRANSID(transid));

	type = msg.m_type;
	c = CALLNUMBER(type);
	result = ENOSYS; /* Out of range or restricted calls return this. */

	transid = TRNS_GET_ID(msg.m_type);

	if((msg.m_source == VFS_PROC_NR) && IS_VFS_FS_TRANSID(transid)) {
		/* If it's a request from VFS, it might have a transaction id. */
		msg.m_type = TRNS_DEL_ID(msg.m_type);

		/* Calls that use the transid */
		result = do_procctl(&msg, transid);
	} else if(msg.m_type == RS_INIT && msg.m_source == RS_PROC_NR) {
		result = do_rs_init(&msg);
	} else if (msg.m_type == VM_PAGEFAULT) {
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
		if (acl_check(&vmproc[caller_slot], c) != OK) {
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

		assert(!IS_VFS_FS_TRANSID(transid));

		if((r=ipc_send(who_e, &msg)) != OK) {
			printf("VM: couldn't send %d to %d (err %d)\n",
				msg.m_type, who_e, r);
			panic("ipc_send() error");
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
	if((s = sys_safecopyfrom(RS_PROC_NR, m->m_rs_init.rproctab_gid, 0,
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
	m->m_rs_init.result = OK;
	ipc_sendrec(RS_PROC_NR, m);

	return(SUSPEND);
}

static struct vmproc *init_proc(endpoint_t ep_nr)
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
	off_t off, vir_bytes vaddr, size_t len)
{
	vir_bytes end;
	struct vm_exec_info *ei = execi->opaque;
	end = ei->ip->start_addr + ei->ip->len;
	assert(ei->ip->start_addr + off + len <= end);
	return sys_physcopy(NONE, ei->ip->start_addr + off,
		execi->proc_e, vaddr, len, 0);
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
	vir_bytes vaddr, size_t len)
{
	boot_alloc(execi, vaddr, len, MF_PREALLOC);
	return OK;
}

static int libexec_alloc_vm_ondemand(struct exec_info *execi,
	vir_bytes vaddr, size_t len)
{
	boot_alloc(execi, vaddr, len, 0);
	return OK;
}

static void exec_bootproc(struct vmproc *vmp, struct boot_image *ip)
{
	struct vm_exec_info vmexeci;
	struct exec_info *execi = &vmexeci.execi;
	char hdr[VM_PAGE_SIZE];

	size_t frame_size = 0;	/* Size of the new initial stack. */
	int argc = 0;		/* Argument count. */
	int envc = 0;		/* Environment count */
	char overflow = 0;	/* No overflow yet. */
	struct ps_strings *psp;

	int vsp = 0;	/* (virtual) Stack pointer in new address space. */
	char *argv[] = { ip->proc_name, NULL };
	char *envp[] = { NULL };
	char *path = ip->proc_name;
	char frame[VM_PAGE_SIZE];

	memset(&vmexeci, 0, sizeof(vmexeci));

	if(pt_new(&vmp->vm_pt) != OK)
		panic("VM: no new pagetable");

	if(pt_bind(&vmp->vm_pt, vmp) != OK)
		panic("VM: pt_bind failed");

	if(sys_physcopy(NONE, ip->start_addr, SELF,
		(vir_bytes) hdr, sizeof(hdr), 0) != OK)
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
	execi->allocmem_prealloc_junk = libexec_alloc_vm_prealloc;
	execi->allocmem_prealloc_cleared = libexec_alloc_vm_prealloc;
	execi->allocmem_ondemand = libexec_alloc_vm_ondemand;

	if (libexec_load_elf(execi) != OK)
		panic("vm: boot process load of process %s (ep=%d) failed\n", 
			execi->progname, vmp->vm_endpoint);

	/* Setup a minimal stack. */
	minix_stack_params(path, argv, envp, &frame_size, &overflow, &argc,
		&envc);

	/* The party is off if there is an overflow, or it is too big for our
	 * pre-allocated space. */
	if(overflow || frame_size > sizeof(frame))
		panic("vm: could not alloc stack for boot process %s (ep=%d)\n",
			execi->progname, vmp->vm_endpoint);

	minix_stack_fill(path, argc, argv, envc, envp, frame_size, frame, &vsp,
		&psp);

	if(handle_memory_once(vmp, vsp, frame_size, 1) != OK)
		panic("vm: could not map stack for boot process %s (ep=%d)\n",
			execi->progname, vmp->vm_endpoint);

	if(sys_datacopy(SELF, (vir_bytes)frame, vmp->vm_endpoint, vsp, frame_size) != OK)
		panic("vm: could not copy stack for boot process %s (ep=%d)\n",
			execi->progname, vmp->vm_endpoint);

	if(sys_exec(vmp->vm_endpoint, (vir_bytes)vsp,
		   (vir_bytes)execi->progname, execi->pc,
		   vsp + ((int)psp - (int)frame)) != OK)
		panic("vm: boot process exec of process %s (ep=%d) failed\n",
			execi->progname,vmp->vm_endpoint);

	/* make it runnable */
	if(sys_vmctl(vmp->vm_endpoint, VMCTL_BOOTINHIBIT_CLEAR, 0) != OK)
		panic("VMCTL_BOOTINHIBIT_CLEAR failed");
}

static int do_procctl_notrans(message *msg)
{
	int transid = 0;

	assert(!IS_VFS_FS_TRANSID(transid));

	return do_procctl(msg, transid);
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

	/* Turn file mmap on? */
	enable_filemap=1;	/* yes by default */
	env_parse("filemap", "d", 0, &enable_filemap, 0, 1);

	/* Sanity check */
	assert(kernel_boot_info.mmap_size > 0);
	assert(kernel_boot_info.mods_with_kernel > 0);

	/* Get chunks of available memory. */
	get_mem_chunks(mem_chunks);

	/* Set table to 0. This invalidates all slots (clear VMF_INUSE). */
	memset(vmproc, 0, sizeof(vmproc));

	for(i = 0; i < ELEMENTS(vmproc); i++) {
		vmproc[i].vm_slot = i;
	}

	/* Initialize ACL data structures. */
	acl_init();

	/* region management initialization. */
	map_region_init();

	/* Initialize tables to all physical memory. */
	mem_init(mem_chunks);

	/* Architecture-dependent initialization. */
	init_proc(VM_PROC_NR);
	pt_init();

	/* Acquire kernel ipc vectors that weren't available
	 * before VM had determined kernel mappings
	 */
	__minix_init();

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
#define CALLMAP(code, func) { int _cmi;		      \
	_cmi=CALLNUMBER(code);				\
	assert(_cmi >= 0);					\
	assert(_cmi < NR_VM_CALLS);		\
	vm_calls[_cmi].vmc_func = (func); 	      \
	vm_calls[_cmi].vmc_name = #code;	      \
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

	CALLMAP(VM_PROCCTL, do_procctl_notrans);

	/* Calls from VFS. */
	CALLMAP(VM_VFS_REPLY, do_vfs_reply);
	CALLMAP(VM_VFS_MMAP, do_vfs_mmap);

	/* Calls from RS */
	CALLMAP(VM_RS_SET_PRIV, do_rs_set_priv);
	CALLMAP(VM_RS_UPDATE, do_rs_update);
	CALLMAP(VM_RS_MEMCTL, do_rs_memctl);

	/* Generic calls. */
	CALLMAP(VM_REMAP, do_remap);
	CALLMAP(VM_REMAP_RO, do_remap);
	CALLMAP(VM_GETPHYS, do_get_phys);
	CALLMAP(VM_SHM_UNMAP, do_munmap);
	CALLMAP(VM_GETREF, do_get_refcount);
	CALLMAP(VM_INFO, do_info);
	CALLMAP(VM_QUERY_EXIT, do_query_exit);
	CALLMAP(VM_WATCH_EXIT, do_watch_exit);

	/* Cache blocks. */
	CALLMAP(VM_MAPCACHEPAGE, do_mapcache);
	CALLMAP(VM_SETCACHEPAGE, do_setcache);
	CALLMAP(VM_CLEARCACHE, do_clearcache);

	/* getrusage */
	CALLMAP(VM_GETRUSAGE, do_getrusage);

	/* Initialize the structures for queryexit */
	init_query_exit();
}

/*===========================================================================*
 *                         sef_cb_signal_handler                             *
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
		alloc_cycle();	/* pagetable code wants to be called */
	}

	pt_clearmapcache();
}

/*===========================================================================*
 *                             map_service                                   *
 *===========================================================================*/
static int map_service(struct rprocpub *rpub)
{
/* Map a new service by initializing its call mask. */
	int r, proc_nr;

	if ((r = vm_isokendpt(rpub->endpoint, &proc_nr)) != OK) {
		return r;
	}

	/* Copy the call mask. */
	acl_set(&vmproc[proc_nr], rpub->vm_call_mask, !IS_RPUB_BOOT_USR(rpub));

	return(OK);
}
