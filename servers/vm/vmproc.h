
#ifndef _VMPROC_H 
#define _VMPROC_H 1

#include <minix/bitmap.h>
#include <machine/archtypes.h>

#include "pt.h"
#include "vm.h"
#include "regionavl.h"

struct vmproc;

struct vmproc {
	int		vm_flags;
	endpoint_t	vm_endpoint;
	pt_t		vm_pt;	/* page table data */
	struct boot_image *vm_boot; /* if boot time process */

	/* Regions in virtual address space. */
	region_avl vm_regions_avl;
	vir_bytes  vm_region_top;	/* highest vaddr last inserted */
	bitchunk_t vm_call_mask[VM_CALL_MASK_SIZE];
	int vm_slot;		/* process table slot */
#if VMSTATS
	int vm_bytecopies;
#endif
};

/* Bits for vm_flags */
#define VMF_INUSE	0x001	/* slot contains a process */
#define VMF_EXITING	0x002	/* PM is cleaning up this process */
#define VMF_WATCHEXIT	0x008	/* Store in queryexit table */

#endif
