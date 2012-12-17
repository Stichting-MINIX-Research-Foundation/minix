
#ifndef _VMPROC_H 
#define _VMPROC_H 1

#include <minix/bitmap.h>
#include <machine/archtypes.h>

#include "pt.h"
#include "vm.h"
#include "regionavl.h"

struct vmproc;

typedef void (*callback_t)(struct vmproc *who, message *m);

struct vmproc {
	int		vm_flags;
	endpoint_t	vm_endpoint;
	pt_t		vm_pt;	/* page table data */
	struct boot_image *vm_boot; /* if boot time process */

	/* Regions in virtual address space. */
	region_avl vm_regions_avl;
	vir_bytes  vm_region_top;	/* highest vaddr last inserted */

	bitchunk_t vm_call_mask[VM_CALL_MASK_SIZE];

	/* State for requests pending to be done to vfs on behalf of
	 * this process.
	 */
	callback_t vm_callback;	  /* function to call on vfs reply */
	int vm_callback_type; /* expected message type */

	int vm_slot;		/* process table slot */
	int vm_yielded;		/* yielded regions */

	union {
		struct {
			cp_grant_id_t gid;
		} open;	/* VM_VFS_OPEN */
	} vm_state;		/* Callback state. */
#if VMSTATS
	int vm_bytecopies;
#endif
};

/* Bits for vm_flags */
#define VMF_INUSE	0x001	/* slot contains a process */
#define VMF_EXITING	0x002	/* PM is cleaning up this process */
#define VMF_WATCHEXIT	0x008	/* Store in queryexit table */

#endif
