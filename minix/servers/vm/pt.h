
#ifndef _PT_H
#define _PT_H 1

#include <machine/vm.h>

#include "vm.h"
#include "pagetable.h"

#ifdef __x86_64__
/*
 * 4-level page table for AMD64: PML4 → PDPT → PD → PT.
 * pt_pml4_phys is loaded into CR3.  Only PML4[0] is used for user space
 * (covers 512 GB); pt_mapkernel() wires PML4[511] to the shared kernel PDPT.
 * pt_pd[pdpte] and pt_pt[pdpte][pde] are allocated on demand.
 */
typedef struct {
    u64_t      *pt_pml4;                          /* PML4 virtual addr */
    phys_bytes  pt_pml4_phys;                     /* PML4 physical addr (CR3) */
    u64_t      *pt_pdpt;                          /* user PDPT (PML4[0]) virtual */
    u64_t      *pt_pd[ARCH_PDPT_ENTRIES];         /* PD page virtual addrs [pdpte] */
    u64_t     **pt_pt[ARCH_PDPT_ENTRIES];         /* PT ptr arrays [pdpte][pde] */
    vir_bytes   pt_virtop;                        /* hint for hole-finding */
} pt_t;
#else /* i386 / earm */
/* A pagetable. */
typedef struct {
	/* Directory entries in VM addr space - root of page table.  */
	u32_t *pt_dir;		/* page aligned (ARCH_VM_DIR_ENTRIES) */
	u32_t pt_dir_phys;	/* physical address of pt_dir */

	/* Pointers to page tables in VM address space. */
	u32_t *pt_pt[ARCH_VM_DIR_ENTRIES];

	/* When looking for a hole in virtual address space, start
	 * looking here. This is in linear addresses, i.e.,
	 * not as the process sees it but the position in the page
	 * page table. This is just a hint.
	 */
	u32_t pt_virtop;
} pt_t;
#endif /* __x86_64__ */

#define CLICKSPERPAGE (VM_PAGE_SIZE/CLICK_SIZE)

#endif
