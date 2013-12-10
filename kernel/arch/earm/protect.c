/* This file contains code for initialization of protected mode, to initialize
 * code and data segment descriptors, and to initialize global descriptors
 * for local descriptors in the process table.
 */

#include <assert.h>
#include <string.h>

#include <machine/multiboot.h>

#include "kernel/kernel.h"

#include "archconst.h"
#include "arch_proto.h"

#include <sys/exec.h>
#include <libexec.h>

struct tss_s tss[CONFIG_MAX_CPUS];
extern int exc_vector_table;

int prot_init_done = 0;

phys_bytes vir2phys(void *vir)
{
	/* defined in kernel.lds */
	extern char _kern_vir_base, _kern_phys_base;
	u32_t offset = (vir_bytes) &_kern_vir_base -
	    (vir_bytes) &_kern_phys_base;
	return (phys_bytes)vir - offset;
}

int tss_init(unsigned cpu, void * kernel_stack)
{

	struct tss_s * t = &tss[cpu];

	/*
	 * make space for process pointer and cpu id and point to the first
	 * usable word
	 */
	t->sp0 = ((unsigned) kernel_stack) - ARM_STACK_TOP_RESERVED;
	/*
	 * set the cpu id at the top of the stack so we know on which cpu is
	 * this stak in use when we trap to kernel
	 */
	*((reg_t *)(t->sp0 + 1 * sizeof(reg_t))) = cpu;

	return 0;
}

multiboot_module_t *bootmod(int pnr)
{
	int i;

	assert(pnr >= 0);

	/* Search for desired process in boot process
	 * list. The first NR_TASKS ones do not correspond
	 * to a module, however, so we don't search those.
	 */
	for(i = NR_TASKS; i < NR_BOOT_PROCS; i++) {
		int p;
		p = i - NR_TASKS;
		if(image[i].proc_nr == pnr) {
			assert(p < MULTIBOOT_MAX_MODS);
			assert(p < kinfo.mbi.mi_mods_count);
			return &kinfo.module_list[p];
		}
	}

	panic("boot module %d not found", pnr);
}

int booting_cpu = 0;

void prot_init()
{
	/* tell the HW where we stored our vector table */
	write_vbar((reg_t)&exc_vector_table);

	/* Set up a new post-relocate bootstrap pagetable so that
	 * we can map in VM, and we no longer rely on pre-relocated
	 * data.
	 */

	pg_clear();
	pg_identity(&kinfo); /* Still need 1:1 for device memory . */
	pg_mapkernel();
	pg_load();

	prot_init_done = 1;
}

static int alloc_for_vm = 0;

void arch_post_init(void)
{
	/* Let memory mapping code know what's going on at bootstrap time */
	struct proc *vm;
	vm = proc_addr(VM_PROC_NR);
	get_cpulocal_var(ptproc) = vm;
	pg_info(&vm->p_seg.p_ttbr, &vm->p_seg.p_ttbr_v);
}

static int libexec_pg_alloc(struct exec_info *execi, vir_bytes vaddr, size_t len)
{
	pg_map(PG_ALLOCATEME, vaddr, vaddr+len, &kinfo);
	pg_load();
	memset((char *) vaddr, 0, len);
	alloc_for_vm += len;
	return OK;
}

void arch_boot_proc(struct boot_image *ip, struct proc *rp)
{
	multiboot_module_t *mod;
	struct ps_strings *psp;
	char *sp;

	if(rp->p_nr < 0) return;

	mod = bootmod(rp->p_nr);

	/* Important special case: we put VM in the bootstrap pagetable
	 * so it can run.
	 */

	if(rp->p_nr == VM_PROC_NR) {
		struct exec_info execi;

		memset(&execi, 0, sizeof(execi));

		/* exec parameters */
		execi.stack_high = kinfo.user_sp;
		execi.stack_size = 64 * 1024;	/* not too crazy as it must be preallocated */
		execi.proc_e = ip->endpoint;
		execi.hdr = (char *) mod->mod_start; /* phys mem direct */
		execi.filesize = execi.hdr_len = mod->mod_end - mod->mod_start;
		strlcpy(execi.progname, ip->proc_name, sizeof(execi.progname));
		execi.frame_len = 0;

		/* callbacks for use in the kernel */
		execi.copymem = libexec_copy_memcpy;
		execi.clearmem = libexec_clear_memset;
		execi.allocmem_prealloc_junk = libexec_pg_alloc;
		execi.allocmem_prealloc_cleared = libexec_pg_alloc;
		execi.allocmem_ondemand = libexec_pg_alloc;
		execi.clearproc = NULL;

		/* parse VM ELF binary and alloc/map it into bootstrap pagetable */
		if(libexec_load_elf(&execi) != OK)
			panic("VM loading failed");

		/* Setup a ps_strings struct on the stack, pointing to the
		 * following argv, envp. */
		sp = (char *)execi.stack_high;
		sp -= sizeof(struct ps_strings);
		psp = (struct ps_strings *) sp;

		/* Take the stack pointer down three words to give startup code
		 * something to use as "argc", "argv" and "envp".
		 */
		sp -= (sizeof(void *) + sizeof(void *) + sizeof(int));

		// linear address space, so it is available.
		psp->ps_argvstr = (char **)(sp + sizeof(int));
		psp->ps_nargvstr = 0;
		psp->ps_envstr = psp->ps_argvstr + sizeof(void *);
		psp->ps_nenvstr = 0;

		arch_proc_init(rp, execi.pc, (vir_bytes)sp,
			execi.stack_high - sizeof(struct ps_strings),
			ip->proc_name);

		/* Free VM blob that was just copied into existence. */
		add_memmap(&kinfo, mod->mod_start, mod->mod_end-mod->mod_start);
		mod->mod_end = mod->mod_start = 0;

		/* Remember them */
		kinfo.vm_allocated_bytes = alloc_for_vm;
	}
}
