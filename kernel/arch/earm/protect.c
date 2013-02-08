/* This file contains code for initialization of protected mode, to initialize
 * code and data segment descriptors, and to initialize global descriptors
 * for local descriptors in the process table.
 */

#include <string.h>
#include <assert.h>
#include <machine/multiboot.h>

#include "kernel/kernel.h"
#include "kernel/proc.h"
#include "archconst.h"

#include "arch_proto.h"

#include <libexec.h>

struct tss_s tss[CONFIG_MAX_CPUS];
extern int exc_vector_table;

int prot_init_done = 0;

phys_bytes vir2phys(void *vir)
{
	extern char _kern_vir_base, _kern_phys_base;	/* in kernel.lds */
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
			assert(p < kinfo.mbi.mods_count);
			return &kinfo.module_list[p];
		}
	}

	panic("boot module %d not found", pnr);
}

int booting_cpu = 0;

void prot_init()
{
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

int libexec_pg_alloc(struct exec_info *execi, off_t vaddr, size_t len)
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
		execi.stack_size = 32 * 1024;	/* not too crazy as it must be preallocated */
		execi.proc_e = ip->endpoint;
		execi.hdr = (char *) mod->mod_start; /* phys mem direct */
		execi.filesize = execi.hdr_len = mod->mod_end - mod->mod_start;
		strcpy(execi.progname, ip->proc_name);
		execi.frame_len = 0;

		/* callbacks for use in the kernel */
		execi.copymem = libexec_copy_memcpy;
		execi.clearmem = libexec_clear_memset;
		execi.allocmem_prealloc = libexec_pg_alloc;
		execi.allocmem_ondemand = libexec_pg_alloc;
		execi.clearproc = NULL;

		/* parse VM ELF binary and alloc/map it into bootstrap pagetable */
		libexec_load_elf(&execi);

	        /* Initialize the server stack pointer. Take it down three words
		 * to give startup code something to use as "argc", "argv" and "envp".
		 */
		arch_proc_init(rp, execi.pc, kinfo.user_sp - 3*4, ip->proc_name);

		/* Free VM blob that was just copied into existence. */
		add_memmap(&kinfo, mod->mod_start, mod->mod_end-mod->mod_start);
		mod->mod_end = mod->mod_start = 0;

		/* Remember them */
		kinfo.vm_allocated_bytes = alloc_for_vm;
	}
}
