
#ifndef _PAGETABLE_H
#define _PAGETABLE_H 1

#include <stdint.h>
#include <machine/vm.h>

#include "vm.h"

/* An ARM pagetable. */
typedef struct {
	/* Directory entries in VM addr space - root of page table.  */
	u32_t *pt_dir;		/* 16KB aligned (ARM_VM_DIR_ENTRIES) */
	u32_t pt_dir_phys;	/* physical address of pt_dir */

	/* Pointers to page tables in VM address space. */
	u32_t *pt_pt[ARM_VM_DIR_ENTRIES];

	/* When looking for a hole in virtual address space, start
	 * looking here. This is in linear addresses, i.e.,
	 * not as the process sees it but the position in the page
	 * page table. This is just a hint.
	 */
	u32_t pt_virtop;
} pt_t;

/* Mapping flags. */
#define PTF_WRITE	ARM_VM_PTE_RW
#define PTF_READ	ARM_VM_PTE_RO
#define PTF_PRESENT	ARM_VM_PTE_PRESENT
#define PTF_SUPER	ARM_VM_PTE_SUPER
#define PTF_USER	ARM_VM_PTE_USER
#define PTF_NOCACHE	ARM_VM_PTE_DEVICE
#define PTF_CACHEWB	ARM_VM_PTE_WB
#define PTF_CACHEWT	ARM_VM_PTE_WT
#define PTF_SHARE	ARM_VM_PTE_SHAREABLE

/* For arch-specific PT routines to check if no bits outside
 * the regular flags are set.
 */
#define PTF_ALLFLAGS	(PTF_READ|PTF_WRITE|PTF_PRESENT|PTF_SUPER|PTF_USER|PTF_NOCACHE|PTF_CACHEWB|PTF_CACHEWT|PTF_SHARE)

#if SANITYCHECKS
#define PT_SANE(p) { pt_sanitycheck((p), __FILE__, __LINE__); }
#else
#define PT_SANE(p)
#endif

#endif


