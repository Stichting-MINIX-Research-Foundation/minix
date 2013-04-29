
#ifndef _PAGETABLE_H
#define _PAGETABLE_H 1

#include <stdint.h>
#include <machine/vm.h>

#include "vm.h"

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

#define ARCH_VM_DIR_ENTRIES     ARM_VM_DIR_ENTRIES
#define ARCH_BIG_PAGE_SIZE      ARM_SECTION_SIZE
#define ARCH_VM_ADDR_MASK       ARM_VM_ADDR_MASK
#define ARCH_VM_PDE_MASK	ARM_VM_PDE_MASK
#define ARCH_VM_PDE_PRESENT	ARM_VM_PDE_PRESENT
#define ARCH_VM_PTE_PRESENT	ARM_VM_PTE_PRESENT
#define ARCH_VM_PTE_USER	ARM_VM_PTE_USER
#define ARCH_PAGEDIR_SIZE	ARM_PAGEDIR_SIZE
#define ARCH_VM_PTE_RW		ARM_VM_PTE_RW
#define ARCH_VM_BIGPAGE		ARM_VM_SECTION
#define ARCH_VM_PT_ENTRIES	ARM_VM_PT_ENTRIES
#define ARCH_VM_PTE_RO		ARM_VM_PTE_RO

/* For arch-specific PT routines to check if no bits outside
 * the regular flags are set.
 */
#define PTF_ALLFLAGS	(PTF_READ|PTF_WRITE|PTF_PRESENT|PTF_SUPER|PTF_USER|PTF_NOCACHE|PTF_CACHEWB|PTF_CACHEWT|PTF_SHARE)

#define PFERR_PROT(e)	((ARM_VM_PFE_FS(e) == ARM_VM_PFE_L1PERM) \
			 || (ARM_VM_PFE_FS(e) == ARM_VM_PFE_L2PERM))
#define PFERR_NOPAGE(e) (!PFERR_PROT(e))
#define PFERR_WRITE(e)	((e) & ARM_VM_PFE_W)
#define PFERR_READ(e)	(!((e) & ARM_VM_PFE_W))

#define VM_PAGE_SIZE    ARM_PAGE_SIZE

/* virtual address -> pde, pte macros */
#define ARCH_VM_PTE(v) ARM_VM_PTE(v)
#define ARCH_VM_PDE(v) ARM_VM_PDE(v)   

#endif

