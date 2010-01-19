/* Function prototypes. */

struct vmproc;
struct stat;
struct mem_map;
struct memory;
struct vir_region;
struct phys_region;

#include <minix/ipc.h>
#include <minix/endpoint.h>
#include <minix/safecopies.h>
#include <minix/vm.h>
#include <timers.h>
#include <stdio.h>
#include <pagetable.h>
#include "vmproc.h"
#include "vm.h"

/* alloc.c */
_PROTOTYPE( phys_clicks alloc_mem_f, (phys_clicks clicks, u32_t flags)	);
_PROTOTYPE( int do_adddma, (message *msg)                              );
_PROTOTYPE( int do_deldma, (message *msg)                              );
_PROTOTYPE( int do_getdma, (message *msg)                              );
_PROTOTYPE( void release_dma, (struct vmproc *vmp)       		);
_PROTOTYPE( void memstats, (int *nodes, int *pages, int *largest)	);
_PROTOTYPE( void printmemstats, (void)			       		);
_PROTOTYPE( void usedpages_reset, (void)		       		);
_PROTOTYPE( int usedpages_add_f, (phys_bytes phys, phys_bytes len,
	char *file, int line) 	);
_PROTOTYPE( void free_mem_f, (phys_clicks base, phys_clicks clicks)	);
#define usedpages_add(a, l) usedpages_add_f(a, l, __FILE__, __LINE__)

#define ALLOC_MEM(clicks, flags) alloc_mem_f(clicks, flags)
#define FREE_MEM(base, clicks) free_mem_f(base, clicks)

_PROTOTYPE( void mem_init, (struct memory *chunks)			);

/* utility.c */
_PROTOTYPE( int get_mem_map, (int proc_nr, struct mem_map *mem_map)     );
_PROTOTYPE( void get_mem_chunks, (struct memory *mem_chunks));
_PROTOTYPE( void reserve_proc_mem, (struct memory *mem_chunks,
        struct mem_map *map_ptr));
_PROTOTYPE( int vm_isokendpt, (endpoint_t ep, int *proc)	     );
_PROTOTYPE( int get_stack_ptr, (int proc_nr, vir_bytes *sp)             );
_PROTOTYPE( int do_info, (message *)					);

/* exit.c */
_PROTOTYPE( void clear_proc, (struct vmproc *vmp)			);
_PROTOTYPE( int do_exit, (message *msg)					);
_PROTOTYPE( int do_willexit, (message *msg)				);
_PROTOTYPE( void free_proc, (struct vmproc *vmp)			);

/* fork.c */
_PROTOTYPE( int do_fork, (message *msg)					);

/* exec.c */
_PROTOTYPE( struct vmproc *find_share, (struct vmproc *vmp_ign, Ino_t ino,
                        Dev_t dev, time_t ctime)                        );
_PROTOTYPE( int do_exec_newmem, (message *msg)				);
_PROTOTYPE( int proc_new, (struct vmproc *vmp, phys_bytes start,
	phys_bytes text, phys_bytes data, phys_bytes stack, phys_bytes gap,
	phys_bytes text_here, phys_bytes data_here, vir_bytes stacktop));
_PROTOTYPE( phys_bytes find_kernel_top, (void)				);

/* break.c */
_PROTOTYPE( int do_brk, (message *msg)					);
_PROTOTYPE( int adjust, (struct vmproc *rmp,
	vir_clicks data_clicks, vir_bytes sp)           );
_PROTOTYPE( int real_brk, (struct vmproc *vmp, vir_bytes v));

/* signal.c */
_PROTOTYPE( int do_push_sig, (message *msg)				);

/* vfs.c */
_PROTOTYPE( int do_vfs_reply, (message *msg)				);
_PROTOTYPE( int vfs_open, (struct vmproc *for_who, callback_t callback,
        cp_grant_id_t filename_gid, int filename_len, int flags, int mode));
_PROTOTYPE( int vfs_close, (struct vmproc *for_who, callback_t callback,
	int fd));

/* map_mem.c */
_PROTOTYPE( int map_memory, (endpoint_t sour, endpoint_t dest,
	vir_bytes virt_s, vir_bytes virt_d, vir_bytes length, int flag));
_PROTOTYPE( int unmap_memory, (endpoint_t sour, endpoint_t dest,
	vir_bytes virt_s, vir_bytes virt_d, vir_bytes length, int flag));

/* mmap.c */
_PROTOTYPE(int do_mmap, (message *msg)					);
_PROTOTYPE(int do_munmap, (message *msg)				);
_PROTOTYPE(int do_map_phys, (message *msg)                              );
_PROTOTYPE(int do_unmap_phys, (message *msg)                            );
_PROTOTYPE(int do_remap, (message *m)                                   );
_PROTOTYPE(int do_get_phys, (message *m)                                );
_PROTOTYPE(int do_shared_unmap, (message *m)                            );
_PROTOTYPE(int do_get_refcount, (message *m)                            );

/* pagefaults.c */
_PROTOTYPE( void do_pagefaults, (void)				);
_PROTOTYPE( void do_memory, (void)				);
_PROTOTYPE( char *pf_errstr, (u32_t err));
_PROTOTYPE( int handle_memory, (struct vmproc *vmp, vir_bytes mem,
	vir_bytes len, int wrflag));

/* $(ARCH)/pagetable.c */
_PROTOTYPE( void pt_init, (phys_bytes limit)				);
_PROTOTYPE( void pt_check, (struct vmproc *vmp)				);
_PROTOTYPE( int pt_new, (pt_t *pt)					);
_PROTOTYPE( int pt_identity, (pt_t *pt)					);
_PROTOTYPE( void pt_free, (pt_t *pt)					);
_PROTOTYPE( int pt_writemap, (pt_t *pt, vir_bytes v, phys_bytes physaddr, 
        size_t bytes, u32_t flags, u32_t writemapflags));
_PROTOTYPE( int pt_checkrange, (pt_t *pt, vir_bytes v,  size_t bytes, int write));
_PROTOTYPE( int pt_bind, (pt_t *pt, struct vmproc *who)			);
_PROTOTYPE( void *vm_allocpage, (phys_bytes *p, int cat));
_PROTOTYPE( void pt_cycle, (void));
_PROTOTYPE( int pt_mapkernel, (pt_t *pt));
_PROTOTYPE( void vm_pagelock, (void *vir, int lockflag) 		);

#if SANITYCHECKS
_PROTOTYPE( void pt_sanitycheck, (pt_t *pt, char *file, int line)	);
#endif

/* $(ARCH)/pagefaults.c */
_PROTOTYPE( int arch_get_pagefault, (endpoint_t *who, vir_bytes *addr, u32_t *err));

/* slaballoc.c */
_PROTOTYPE(void *slaballoc,(int bytes));
_PROTOTYPE(void slabfree,(void *mem, int bytes));
_PROTOTYPE(void slabstats,(void));
_PROTOTYPE(void slab_sanitycheck, (char *file, int line));
#define SLABALLOC(var) (var = slaballoc(sizeof(*var)))
#define SLABFREE(ptr) slabfree(ptr, sizeof(*(ptr)))
#if SANITYCHECKS

_PROTOTYPE(void slabunlock,(void *mem, int bytes));
_PROTOTYPE(void slablock,(void *mem, int bytes));
_PROTOTYPE(int slabsane_f,(char *file, int line, void *mem, int bytes));
#endif

/* region.c */
_PROTOTYPE(struct vir_region * map_page_region,(struct vmproc *vmp, \
	vir_bytes min, vir_bytes max, vir_bytes length, vir_bytes what, \
	u32_t flags, int mapflags));
_PROTOTYPE(struct vir_region * map_proc_kernel,(struct vmproc *dst));
_PROTOTYPE(int map_region_extend,(struct vmproc *vmp, struct vir_region *vr, vir_bytes delta));
_PROTOTYPE(int map_region_shrink,(struct vir_region *vr, vir_bytes delta));
_PROTOTYPE(int map_unmap_region,(struct vmproc *vmp, struct vir_region *vr, vir_bytes len));
_PROTOTYPE(int map_free_proc,(struct vmproc *vmp));
_PROTOTYPE(int map_proc_copy,(struct vmproc *dst, struct vmproc *src));
_PROTOTYPE(struct vir_region *map_lookup,(struct vmproc *vmp, vir_bytes addr));
_PROTOTYPE(int map_pf,(struct vmproc *vmp,
	struct vir_region *region, vir_bytes offset, int write));
_PROTOTYPE(int map_handle_memory,(struct vmproc *vmp,
	struct vir_region *region, vir_bytes offset, vir_bytes len, int write));
_PROTOTYPE(void map_printmap, (struct vmproc *vmp));
_PROTOTYPE(int map_writept, (struct vmproc *vmp));
_PROTOTYPE(void printregionstats, (struct vmproc *vmp));

_PROTOTYPE(struct vir_region * map_region_lookup_tag, (struct vmproc *vmp, u32_t tag));
_PROTOTYPE(void map_region_set_tag, (struct vir_region *vr, u32_t tag));
_PROTOTYPE(u32_t map_region_get_tag, (struct vir_region *vr));
_PROTOTYPE(int map_remap, (struct vmproc *dvmp, vir_bytes da, size_t size,
	struct vir_region *region, vir_bytes *r));
_PROTOTYPE(int map_get_phys, (struct vmproc *vmp, vir_bytes addr, phys_bytes *r));
_PROTOTYPE(int map_get_ref, (struct vmproc *vmp, vir_bytes addr, u8_t *cnt));

_PROTOTYPE(int map_copy_ph_block, (struct vmproc *vmp,
	struct vir_region *region, struct phys_region *ph));
_PROTOTYPE(void pb_unreferenced, (struct vir_region *region,
	struct phys_region *pr));
_PROTOTYPE(void get_usage_info, (struct vmproc *vmp,
	struct vm_usage_info *vui));
_PROTOTYPE(int get_region_info, (struct vmproc *vmp,
	struct vm_region_info *vri, int count, vir_bytes *nextp));
#if SANITYCHECKS
_PROTOTYPE(void map_sanitycheck,(char *file, int line));
#endif

/* $(ARCH)/vm.c */
_PROTOTYPE( vir_bytes arch_map2vir, (struct vmproc *vmp, vir_bytes addr));
_PROTOTYPE( char *arch_map2str, (struct vmproc *vmp, vir_bytes addr));
_PROTOTYPE( vir_bytes arch_map2info, (struct vmproc *vmp, vir_bytes addr,
	int *space, int *prot));
_PROTOTYPE( vir_bytes arch_vir2map, (struct vmproc *vmp, vir_bytes addr));
_PROTOTYPE( vir_bytes arch_vir2map_text, (struct vmproc *vmp, vir_bytes addr));
_PROTOTYPE( vir_bytes arch_addrok, (struct vmproc *vmp, vir_bytes addr));

/* rs.c */
_PROTOTYPE(int do_rs_set_priv, (message *m));

/* queryexit.c */
_PROTOTYPE(int do_query_exit, (message *m));
_PROTOTYPE(int do_notify_sig, (message *m));
_PROTOTYPE(void init_query_exit, (void));
