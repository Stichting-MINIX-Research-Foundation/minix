/* Prototypes and definitions for VM interface. */

#ifndef _MINIX_VM_H
#define _MINIX_VM_H

#include <sys/types.h>
#include <minix/endpoint.h>

int vm_exit(endpoint_t ep);
int vm_fork(endpoint_t ep, int slotno, endpoint_t *child_ep);
int vm_brk(endpoint_t ep, char *newaddr);
int vm_willexit(endpoint_t ep);
int vm_adddma(endpoint_t proc_e, phys_bytes start, phys_bytes size);
int vm_deldma(endpoint_t proc_e, phys_bytes start, phys_bytes size);
int vm_getdma(endpoint_t *procp, phys_bytes *basep, phys_bytes *sizep);
void *vm_map_phys(endpoint_t who, void *physaddr, size_t len);
int vm_unmap_phys(endpoint_t who, void *vaddr, size_t len);

int vm_notify_sig(endpoint_t ep, endpoint_t ipc_ep);
int vm_set_priv(int procnr, void *buf);
int vm_update(endpoint_t src_e, endpoint_t dst_e);
int vm_memctl(endpoint_t ep, int req);
int vm_query_exit(endpoint_t *endpt);
int vm_watch_exit(endpoint_t ep);
int vm_forgetblock(u64_t id);
void vm_forgetblocks(void);
int vm_yield_block_get_block(u64_t yieldid, u64_t getid, void *mem,
	vir_bytes len);

/* Invalid ID with special meaning for the vm_yield_block_get_block
 * interface.
 */
#define VM_BLOCKID_NONE make64(ULONG_MAX, ULONG_MAX)

/* VM kernel request types. */
#define VMPTYPE_NONE		0
#define VMPTYPE_CHECK		1

struct vm_stats_info {
  unsigned int vsi_pagesize;	/* page size */
  unsigned long vsi_total;	/* total number of memory pages */
  unsigned long vsi_free;	/* number of free pages */
  unsigned long vsi_largest;	/* largest number of consecutive free pages */
  unsigned long vsi_cached;	/* number of pages cached for file systems */
};

struct vm_usage_info {
  vir_bytes vui_total;		/* total amount of process memory */
  vir_bytes vui_common;		/* part of memory mapped in more than once */
  vir_bytes vui_shared;		/* shared (non-COW) part of common memory */
};

struct vm_region_info {
  vir_bytes vri_addr;		/* base address of region */
  vir_bytes vri_length;		/* length of region */
  int vri_prot;			/* protection flags (PROT_) */
  int vri_flags;		/* memory flags (subset of MAP_) */
};

#define MAX_VRI_COUNT	64	/* max. number of regions provided at once */

int vm_info_stats(struct vm_stats_info *vfi);
int vm_info_usage(endpoint_t who, struct vm_usage_info *vui);
int vm_info_region(endpoint_t who, struct vm_region_info *vri, int
	count, vir_bytes *next);
int vm_procctl(endpoint_t ep, int param);

int vm_set_cacheblock(void *block, u32_t dev, u64_t dev_offset,
        u64_t ino, u64_t ino_offset, u32_t *flags, int blocksize);

void *vm_map_cacheblock(u32_t dev, u64_t dev_offset,
        u64_t ino, u64_t ino_offset, u32_t *flags, int blocksize);

/* flags for vm cache functions */
#define VMMC_FLAGS_LOCKED	0x01	/* someone is updating the flags; don't read/write */
#define VMMC_DIRTY		0x02	/* dirty buffer and it may not be evicted */
#define VMMC_EVICTED		0x04	/* VM has evicted the buffer and it's invalid */
#define VMMC_BLOCK_LOCKED	0x08	/* client is using it and it may not be evicted */

/* special inode number for vm cache functions */
#define VMC_NO_INODE		0	/* to reference a disk block, no associated file */

#endif /* _MINIX_VM_H */

