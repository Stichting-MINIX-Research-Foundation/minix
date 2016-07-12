#ifndef __SYS_VM_386_H__
#define __SYS_VM_386_H__
/*
i386/vm.h
*/

#define I386_PAGE_SIZE		4096
#define I386_BIG_PAGE_SIZE	(I386_PAGE_SIZE*I386_VM_PT_ENTRIES)

/* i386 paging constants */
#define I386_VM_PRESENT	0x001	/* Page is present */
#define I386_VM_WRITE	0x002	/* Read/write access allowed */
#define I386_VM_READ	0x000	/* Read access only */
#define I386_VM_USER	0x004	/* User access allowed */
#define I386_VM_PWT	0x008	/* Write through */
#define I386_VM_PCD	0x010	/* Cache disable */
#define I386_VM_ACC	0x020	/* Accessed */
#define I386_VM_ADDR_MASK 0xFFFFF000 /* physical address */
#define I386_VM_ADDR_MASK_4MB 0xFFC00000 /* physical address */
#define I386_VM_OFFSET_MASK_4MB 0x003FFFFF /* physical address */

/* Page directory specific flags. */
#define I386_VM_BIGPAGE	0x080	/* 4MB page */

/* Page table specific flags. */
#define I386_VM_DIRTY    (1L<< 6)	/* Dirty */
#define I386_VM_PS  	 (1L<< 7)	/* Page size. */
#define I386_VM_GLOBAL   (1L<< 8)	/* Global. */
#define I386_VM_PTAVAIL1 (1L<< 9)	/* Available for use. */
#define I386_VM_PTAVAIL2 (1L<<10)	/* Available for use. */
#define I386_VM_PTAVAIL3 (1L<<11)	/* Available for use. */

#define I386_VM_PT_ENT_SIZE	4	/* Size of a page table entry */
#define I386_VM_DIR_ENTRIES	1024	/* Number of entries in a page dir */
#define I386_VM_DIR_ENT_SHIFT	22	/* Shift to get entry in page dir. */
#define I386_VM_PT_ENT_SHIFT	12	/* Shift to get entry in page table */
#define I386_VM_PT_ENT_MASK	0x3FF	/* Mask to get entry in page table */
#define I386_VM_PT_ENTRIES	1024	/* Number of entries in a page table */
#define I386_VM_PFA_SHIFT	22	/* Page frame address shift */

/* CR0 bits */
#define I386_CR0_PE		0x00000001	/* Protected mode  */
#define I386_CR0_MP		0x00000002	/* Monitor Coprocessor  */
#define I386_CR0_EM		0x00000004	/* Emulate  */
#define I386_CR0_TS		0x00000008	/* Task Switched  */
#define I386_CR0_ET		0x00000010	/* Extension Type  */
#define I386_CR0_WP		0x00010000	/* Enable paging */
#define I386_CR0_PG		0x80000000	/* Enable paging */

/* some CR4 bits */
#define I386_CR4_VME		0x00000001	/* Virtual 8086 */
#define I386_CR4_PVI		0x00000002	/* Virtual ints */
#define I386_CR4_TSD		0x00000004	/* RDTSC privileged */
#define I386_CR4_DE		0x00000008	/* Debugging extensions */
#define I386_CR4_PSE		0x00000010	/* Page size extensions */
#define I386_CR4_PAE		0x00000020	/* Physical addr extens. */
#define I386_CR4_MCE		0x00000040	/* Machine check enable */
#define I386_CR4_PGE		0x00000080	/* Global page flag enable */

/* i386 paging 'functions' */
#define I386_VM_PTE(v)	(((v) >> I386_VM_PT_ENT_SHIFT) & I386_VM_PT_ENT_MASK)
#define I386_VM_PDE(v)	( (v) >> I386_VM_DIR_ENT_SHIFT)
#define I386_VM_PFA(e)	( (e) & I386_VM_ADDR_MASK)
#define I386_VM_PAGE(v)	( (v) >> I386_VM_PFA_SHIFT)

/* i386 pagefault error code bits */
#define I386_VM_PFE_P	0x01	/* Pagefault caused by non-present page.
				 * (otherwise protection violation.)
				 */
#define I386_VM_PFE_W	0x02	/* Caused by write (otherwise read) */
#define I386_VM_PFE_U	0x04	/* CPU in user mode (otherwise supervisor) */

/* CPUID flags */
#define CPUID1_EDX_FPU		(1L)		/* FPU presence */
#define CPUID1_EDX_PSE 		(1L <<  3)	/* Page Size Extension */
#define CPUID1_EDX_SYSENTER	(1L << 11)	/* Intel SYSENTER */
#define CPUID1_EDX_PAE 		(1L << 6)	/* Physical Address Extension */
#define CPUID1_EDX_PGE 		(1L << 13)	/* Page Global (bit) Enable */
#define CPUID1_EDX_APIC_ON_CHIP (1L << 9)	/* APIC is present on the chip */
#define CPUID1_EDX_TSC		(1L << 4)	/* Timestamp counter present */
#define CPUID1_EDX_HTT		(1L << 28)	/* Supports HTT */
#define CPUID1_EDX_FXSR		(1L << 24)
#define CPUID1_EDX_SSE		(1L << 25)
#define CPUID1_EDX_SSE2		(1L << 26)
#define CPUID1_ECX_SSE3		(1L)
#define CPUID1_ECX_SSSE3	(1L << 9)
#define CPUID1_ECX_SSE4_1	(1L << 19)
#define CPUID1_ECX_SSE4_2	(1L << 20)

#define CPUID_EF_EDX_SYSENTER	(1L << 11)	/* Intel SYSENTER */

#ifndef __ASSEMBLY__

#include <minix/type.h>

/* structure used by VM to pass data to the kernel while enabling paging */
struct vm_ep_data {
	struct mem_map	* mem_map;
	vir_bytes	data_seg_limit;
};
#endif

#endif /* __SYS_VM_386_H__ */
