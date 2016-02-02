/* $NetBSD: machdep.h,v 1.18 2014/03/28 21:51:59 matt Exp $ */

#ifndef _ARM32_BOOT_MACHDEP_H_
#define _ARM32_BOOT_MACHDEP_H_

/* Define various stack sizes in pages */ 
#ifndef IRQ_STACK_SIZE
#define IRQ_STACK_SIZE	1
#endif
#ifndef ABT_STACK_SIZE
#define ABT_STACK_SIZE	1
#endif
#ifndef UND_STACK_SIZE
#ifdef IPKDB
#define UND_STACK_SIZE	2
#else
#define UND_STACK_SIZE	1
#endif
#endif
#ifndef FIQ_STACK_SIZE
#define FIQ_STACK_SIZE	1
#endif


extern void (*cpu_reset_address)(void);
extern paddr_t cpu_reset_address_paddr;

extern u_int data_abort_handler_address;
extern u_int prefetch_abort_handler_address;
// extern u_int undefined_handler_address;
#define	undefined_handler_address (curcpu()->ci_undefsave[2])

struct bootmem_info {
	paddr_t bmi_start;
	paddr_t bmi_kernelstart;
	paddr_t bmi_kernelend;
	paddr_t bmi_end;
	pv_addrqh_t bmi_freechunks;
	pv_addrqh_t bmi_chunks;		/* sorted list of memory to be mapped */
	pv_addr_t bmi_freeblocks[4];
	/*
	 * These need to be static for pmap's kernel_pt list.
	 */
	pv_addr_t bmi_vector_l2pt;
	pv_addr_t bmi_io_l2pt;
	pv_addr_t bmi_l2pts[32];	// for large memory disks.
	u_int bmi_freepages;
	u_int bmi_nfreeblocks;
};

extern struct bootmem_info bootmem_info;

extern char *booted_kernel;

extern volatile uint32_t arm_cpu_hatched;
extern volatile uint32_t arm_cpu_mbox;
extern u_int arm_cpu_max;

/* misc prototypes used by the many arm machdeps */
void cortex_pmc_ccnt_init(void);
void cpu_hatch(struct cpu_info *, cpuid_t, void (*)(struct cpu_info *));
void halt(void);
void parse_mi_bootargs(char *);
void data_abort_handler(trapframe_t *);
void prefetch_abort_handler(trapframe_t *);
void undefinedinstruction_bounce(trapframe_t *);
void dumpsys(void);

/* 
 * note that we use void *as all the platforms have different ideas on what
 * the structure is
 */
u_int initarm(void *);
struct pmap_devmap;
struct boot_physmem;
void arm32_bootmem_init(paddr_t memstart, psize_t memsize, paddr_t kernelstart);
void arm32_kernel_vm_init(vaddr_t kvm_base, vaddr_t vectors,
	vaddr_t iovbase /* (can be zero) */,
	const struct pmap_devmap *devmap, bool mapallmem_p);
vaddr_t initarm_common(vaddr_t kvm_base, vsize_t kvm_size,
        const struct boot_physmem *bp, size_t nbp);


/* from arm/arm32/intr.c */
void dosoftints(void);
void set_spl_masks(void);
#ifdef DIAGNOSTIC
void dump_spl_masks(void);
#endif
#endif
