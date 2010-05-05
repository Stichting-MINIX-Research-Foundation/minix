
#ifndef _VMPROC_H 
#define _VMPROC_H 1

#include <pagetable.h>
#include <arch_vmproc.h>
#include <minix/bitmap.h>
#include <machine/archtypes.h>

#include "vm.h"
#include "physravl.h"
#include "yieldedavl.h"

struct vmproc;

typedef void (*callback_t)(struct vmproc *who, message *m);

struct vmproc {
	struct vm_arch	vm_arch; /* architecture-specific data */
	int		vm_flags;
	endpoint_t	vm_endpoint;
	pt_t		vm_pt;	/* page table data, if VMF_HASPT is set */
	vir_bytes	vm_stacktop;	/* top of stack as seen from process */
	vir_bytes	vm_offset;	/* offset of addr 0 for process */

	/* File identification for cs sharing. */
	ino_t vm_ino;		/* inode number of file */
	dev_t vm_dev;		/* device number of file system */
	time_t vm_ctime;	/* inode changed time */

	/* Regions in virtual address space. */
	struct vir_region *vm_regions;
	yielded_avl	vm_yielded_blocks;	 /* avl of yielded physblocks */

	/* Heap for brk() to extend. */
	struct vir_region *vm_heap;

	bitchunk_t vm_call_mask[VM_CALL_MASK_SIZE];

	/* State for requests pending to be done to vfs on behalf of
	 * this process.
	 */
	callback_t vm_callback;	  /* function to call on vfs reply */
	int vm_callback_type; /* expected message type */

	int vm_slot;		/* process table slot */

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
#define VMF_SEPARATE	0x002	/* separate i&d */
#define VMF_HASPT	0x004	/* has private page table */
#define VMF_EXITING	0x008	/* PM is cleaning up this process */
#define VMF_HAS_DMA	0x010	/* Process directly or indirectly granted
				 * DMA buffers.
				 */

#endif
