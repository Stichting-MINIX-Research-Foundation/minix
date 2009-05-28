
#ifndef _PAGETABLE_H
#define _PAGETABLE_H 1

#include <stdint.h>
#include <sys/vm_i386.h>

#include "../vm.h"

/* An i386 pagetable. */
typedef struct {
	/* Directory entries in VM addr space - root of page table.  */
	u32_t *pt_dir;		/* page aligned (I386_VM_DIR_ENTRIES) */
	u32_t pt_dir_phys;	/* physical address of pt_dir */

	/* Pointers to page tables in VM address space. */
	u32_t *pt_pt[I386_VM_DIR_ENTRIES];

	/* When looking for a hole in virtual address space, start
	 * looking here. This is in linear addresses, i.e.,
	 * not as the process sees it but the position in the page
	 * page table. This is just a hint.
	 */
	u32_t pt_virtop;
} pt_t;

/* Mapping flags. */
#define PTF_WRITE	I386_VM_WRITE
#define PTF_PRESENT	I386_VM_PRESENT
#define PTF_USER	I386_VM_USER
#define PTF_GLOBAL	I386_VM_GLOBAL
#define PTF_MAPALLOC	I386_VM_PTAVAIL1 /* Page allocated by pt code. */

/* For arch-specific PT routines to check if no bits outside
 * the regular flags are set.
 */
#define PTF_ALLFLAGS	(PTF_WRITE|PTF_PRESENT|PTF_USER|PTF_GLOBAL)

#if SANITYCHECKS
#define PT_SANE(p) { pt_sanitycheck((p), __FILE__, __LINE__); }
#else
#define PT_SANE(p)
#endif

#endif


