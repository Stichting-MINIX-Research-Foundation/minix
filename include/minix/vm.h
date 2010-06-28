/* Prototypes and definitions for VM interface. */

#ifndef _MINIX_VM_H
#define _MINIX_VM_H

#include <minix/types.h>
#include <minix/endpoint.h>

_PROTOTYPE( int vm_exit, (endpoint_t ep));
_PROTOTYPE( int vm_fork, (endpoint_t ep, int slotno, endpoint_t *child_ep));
_PROTOTYPE( int vm_brk, (endpoint_t ep, char *newaddr));
_PROTOTYPE( int vm_exec_newmem, (endpoint_t ep, struct exec_newmem *args,
	int args_bytes, char **ret_stack_top, int *ret_flags));
_PROTOTYPE( int vm_push_sig, (endpoint_t ep, vir_bytes *old_sp));
_PROTOTYPE( int vm_willexit, (endpoint_t ep));
_PROTOTYPE( int vm_adddma, (endpoint_t req_e, endpoint_t proc_e, 
                                phys_bytes start, phys_bytes size)      );
_PROTOTYPE( int vm_deldma, (endpoint_t req_e, endpoint_t proc_e, 
                                phys_bytes start, phys_bytes size)      );
_PROTOTYPE( int vm_getdma, (endpoint_t req_e, endpoint_t *procp,
				phys_bytes *basep, phys_bytes *sizep)   );
_PROTOTYPE( void *vm_map_phys, (endpoint_t who, void *physaddr, size_t len));
_PROTOTYPE( int vm_unmap_phys, (endpoint_t who, void *vaddr, size_t len));

_PROTOTYPE( int vm_notify_sig, (endpoint_t ep, endpoint_t ipc_ep));
_PROTOTYPE( int vm_set_priv, (int procnr, void *buf));
_PROTOTYPE( int vm_update, (endpoint_t src_e, endpoint_t dst_e));
_PROTOTYPE( int vm_memctl, (endpoint_t ep, int req));
_PROTOTYPE( int vm_query_exit, (int *endpt));
_PROTOTYPE( int vm_forgetblock, (u64_t id));
_PROTOTYPE( void vm_forgetblocks, (void));
_PROTOTYPE( int vm_yield_block_get_block, (u64_t yieldid, u64_t getid,
        void *mem, vir_bytes len));

/* Invalid ID with special meaning for the vm_yield_block_get_block
 * interface.
 */
#define VM_BLOCKID_NONE make64(ULONG_MAX, ULONG_MAX)

/* VM kernel request types. */
#define VMPTYPE_NONE		0
#define VMPTYPE_CHECK		1
#define VMPTYPE_COWMAP		2
#define VMPTYPE_SMAP		3
#define VMPTYPE_SUNMAP		4

struct vm_stats_info {
  int vsi_pagesize;		/* page size */
  int vsi_total;		/* total number of memory pages */
  int vsi_free;			/* number of free pages */
  int vsi_largest;		/* largest number of consecutive free pages */
};

struct vm_usage_info {
  vir_bytes vui_total;		/* total amount of process memory */
  vir_bytes vui_common;		/* part of memory mapped in more than once */
  vir_bytes vui_shared;		/* shared (non-COW) part of common memory */
};

struct vm_region_info {
  int vri_seg;			/* segment of virtual region (T or D) */
  vir_bytes vri_addr;		/* base address of region */
  vir_bytes vri_length;		/* length of region */
  int vri_prot;			/* protection flags (PROT_) */
  int vri_flags;		/* memory flags (subset of MAP_) */
};

#define MAX_VRI_COUNT	64	/* max. number of regions provided at once */

_PROTOTYPE( int vm_info_stats, (struct vm_stats_info *vfi)		);
_PROTOTYPE( int vm_info_usage, (endpoint_t who,
	struct vm_usage_info *vui)					);
_PROTOTYPE( int vm_info_region, (endpoint_t who,
	struct vm_region_info *vri, int count, vir_bytes *next)		);

#endif /* _MINIX_VM_H */

