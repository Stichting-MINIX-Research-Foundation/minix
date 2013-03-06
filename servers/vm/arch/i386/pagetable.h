
#ifndef _PAGETABLE_H
#define _PAGETABLE_H 1

#include <stdint.h>
#include <machine/vm.h>

#include "vm.h"

/* Mapping flags. */
#define PTF_WRITE	I386_VM_WRITE
#define PTF_READ	I386_VM_READ
#define PTF_PRESENT	I386_VM_PRESENT
#define PTF_USER	I386_VM_USER
#define PTF_GLOBAL	I386_VM_GLOBAL
#define PTF_NOCACHE	(I386_VM_PWT | I386_VM_PCD)

#define ARCH_VM_DIR_ENTRIES	I386_VM_DIR_ENTRIES
#define ARCH_BIG_PAGE_SIZE	I386_BIG_PAGE_SIZE
#define ARCH_VM_ADDR_MASK	I386_VM_ADDR_MASK
#define ARCH_VM_PAGE_PRESENT    I386_VM_PRESENT
#define ARCH_VM_PDE_MASK        I386_VM_PDE_MASK
#define ARCH_VM_PDE_PRESENT     I386_VM_PRESENT
#define ARCH_VM_PTE_PRESENT	I386_VM_PRESENT
#define ARCH_VM_PTE_USER	I386_VM_USER
#define ARCH_VM_PTE_RW		I386_VM_WRITE
#define ARCH_PAGEDIR_SIZE	I386_PAGE_SIZE
#define ARCH_VM_BIGPAGE		I386_VM_BIGPAGE
#define ARCH_VM_PT_ENTRIES      I386_VM_PT_ENTRIES

/* For arch-specific PT routines to check if no bits outside
 * the regular flags are set.
 */
#define PTF_ALLFLAGS   (PTF_READ|PTF_WRITE|PTF_PRESENT|PTF_USER|PTF_GLOBAL|PTF_NOCACHE)

#define PFERR_NOPAGE(e)	(!((e) & I386_VM_PFE_P))
#define PFERR_PROT(e)	(((e) & I386_VM_PFE_P))
#define PFERR_WRITE(e)	((e) & I386_VM_PFE_W)
#define PFERR_READ(e)	(!((e) & I386_VM_PFE_W))

#define VM_PAGE_SIZE	I386_PAGE_SIZE

/* virtual address -> pde, pte macros */
#define ARCH_VM_PTE(v) I386_VM_PTE(v)
#define ARCH_VM_PDE(v) I386_VM_PDE(v)

#endif
