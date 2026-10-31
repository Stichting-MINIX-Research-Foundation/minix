
#ifndef _PAGETABLE_H
#define _PAGETABLE_H 1

#include <stdint.h>
#include <machine/vm.h>

#include "vm.h"

/* Mapping flags — x86_64 uses AMD64_VM_* constants. */
#define PTF_WRITE        AMD64_VM_WRITE
#define PTF_READ         AMD64_VM_READ
#define PTF_PRESENT      AMD64_VM_PRESENT
#define PTF_USER         AMD64_VM_USER
#define PTF_GLOBAL       AMD64_VM_GLOBAL
#define PTF_NOCACHE      (AMD64_VM_PWT | AMD64_VM_PCD)
#define PTF_NOEXEC       AMD64_VM_NX

#define ARCH_VM_DIR_ENTRIES      AMD64_VM_PT_ENTRIES   /* 512 PD entries per PD page */
#define ARCH_PDPT_ENTRIES        AMD64_VM_PT_ENTRIES   /* 512 PDPT entries */
#define ARCH_BIG_PAGE_SIZE       AMD64_BIG_PAGE_SIZE   /* 2MB (PS bit set in PDE) */
#define ARCH_VM_ADDR_MASK        AMD64_VM_ADDR_MASK    /* 52-bit physical addr mask */
#define ARCH_VM_PAGE_PRESENT     AMD64_VM_PRESENT
#define ARCH_VM_PDE_MASK         AMD64_VM_ADDR_MASK
#define ARCH_VM_PDE_PRESENT      AMD64_VM_PRESENT
#define ARCH_VM_PTE_PRESENT      AMD64_VM_PRESENT
#define ARCH_VM_PTE_USER         AMD64_VM_USER
#define ARCH_VM_PTE_RW           AMD64_VM_WRITE
#define ARCH_PAGEDIR_SIZE        AMD64_PAGE_SIZE        /* PML4 root page is 4KB */
#define ARCH_VM_BIGPAGE          AMD64_VM_PS            /* large page flag (2MB) */
#define ARCH_VM_PT_ENTRIES       AMD64_VM_PT_ENTRIES   /* 512 PT entries per PT page */

/* All valid PTF flags (PTF_NOEXEC = bit 63, u64_t only). */
#define PTF_ALLFLAGS (PTF_READ|PTF_WRITE|PTF_PRESENT|PTF_USER|PTF_GLOBAL|PTF_NOCACHE|PTF_NOEXEC)

/* Pagefault error code interpretation. */
#define PFERR_NOPAGE(e)  (!((e) & AMD64_VM_PFE_P))
#define PFERR_PROT(e)    (((e) & AMD64_VM_PFE_P))
#define PFERR_WRITE(e)   ((e) & AMD64_VM_PFE_W)
#define PFERR_READ(e)    (!((e) & AMD64_VM_PFE_W))

#define VM_PAGE_SIZE     AMD64_PAGE_SIZE

/* Virtual address → table index macros (9-bit fields). */
#define ARCH_VM_PDPTE(v) AMD64_VM_PDPT(v)   /* PDPT index: bits 38:30 */
#define ARCH_VM_PDE(v)   AMD64_VM_PD(v)      /* PD   index: bits 29:21 */
#define ARCH_VM_PTE(v)   AMD64_VM_PT(v)      /* PT   index: bits 20:12 */

#endif
