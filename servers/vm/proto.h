/* Function prototypes. */

struct vmproc;
struct stat;
struct mem_map;
struct memory;

#include <minix/ipc.h>
#include <minix/endpoint.h>
#include <minix/safecopies.h>
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
_PROTOTYPE( int do_allocmem, (message *msg)                              );
_PROTOTYPE( void release_dma, (struct vmproc *vmp)       		);

_PROTOTYPE( void free_mem_f, (phys_clicks base, phys_clicks clicks)	);

#define ALLOC_MEM(clicks, flags) alloc_mem_f(clicks, flags)
#define FREE_MEM(base, clicks) free_mem_f(base, clicks)

_PROTOTYPE( void mem_init, (struct memory *chunks)			);
_PROTOTYPE( void memstats, (void)					);

/* utility.c */
_PROTOTYPE( int get_mem_map, (int proc_nr, struct mem_map *mem_map)     );
_PROTOTYPE( void get_mem_chunks, (struct memory *mem_chunks));
_PROTOTYPE( void reserve_proc_mem, (struct memory *mem_chunks,
        struct mem_map *map_ptr));
_PROTOTYPE( int vm_isokendpt, (endpoint_t ep, int *proc)	     );
_PROTOTYPE( int get_stack_ptr, (int proc_nr, vir_bytes *sp)             );

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

/* mmap.c */
_PROTOTYPE(int do_mmap, (message *msg)					);
_PROTOTYPE(int do_map_phys, (message *msg)                              );
_PROTOTYPE(int do_unmap_phys, (message *msg)                            );

/* pagefaults.c */
_PROTOTYPE( void handle_pagefaults, (void)				);
_PROTOTYPE( void handle_memory, (void)				);

/* $(ARCH)/pagetable.c */
_PROTOTYPE( void pt_init, (void)					);
_PROTOTYPE( int pt_new, (pt_t *pt)					);
_PROTOTYPE( int pt_copy, (pt_t *src, pt_t *dst)				);
_PROTOTYPE( void pt_free, (pt_t *pt)					);
_PROTOTYPE( void pt_freerange, (pt_t *pt, vir_bytes lo, vir_bytes hi)	);
_PROTOTYPE( int pt_allocmap, (pt_t *pt, vir_bytes minv, vir_bytes maxv,
	size_t bytes, u32_t pageflags, u32_t allocflags, vir_bytes *newv));
_PROTOTYPE( int pt_writemap, (pt_t *pt, vir_bytes v, phys_bytes physaddr, 
        size_t bytes, u32_t flags, u32_t writemapflags));
_PROTOTYPE( int pt_bind, (pt_t *pt, struct vmproc *who)			);
_PROTOTYPE( void *vm_allocpages, (phys_bytes *p, int pages, int cat));
_PROTOTYPE( void pt_cycle, (void));
_PROTOTYPE( int pt_mapkernel, (pt_t *pt));
_PROTOTYPE( void phys_readaddr, (phys_bytes addr, phys_bytes *v1, phys_bytes *v2));
_PROTOTYPE( void phys_writeaddr, (phys_bytes addr, phys_bytes v1, phys_bytes v2));
#if SANITYCHECKS
_PROTOTYPE( void pt_sanitycheck, (pt_t *pt, char *file, int line)	);
#endif

/* $(ARCH)/pagefaults.c */
_PROTOTYPE( int arch_get_pagefault, (endpoint_t *who, vir_bytes *addr, u32_t *err));

/* slaballoc.c */
_PROTOTYPE(void *slaballoc,(int bytes));
_PROTOTYPE(void slabfree,(void *mem, int bytes));
_PROTOTYPE(void slabstats,(void));
#define SLABALLOC(var) (var = slaballoc(sizeof(*var)))
#define SLABFREE(ptr) slabfree(ptr, sizeof(*(ptr)))

/* region.c */
_PROTOTYPE(struct vir_region * map_page_region,(struct vmproc *vmp, \
	vir_bytes min, vir_bytes max, vir_bytes length, vir_bytes what, \
	u32_t flags, int mapflags));
_PROTOTYPE(struct vir_region * map_proc_kernel,(struct vmproc *dst));
_PROTOTYPE(int map_region_extend,(struct vir_region *vr, vir_bytes delta));
_PROTOTYPE(int map_region_shrink,(struct vir_region *vr, vir_bytes delta));
_PROTOTYPE(int map_unmap_region,(struct vmproc *vmp, struct vir_region *vr));
_PROTOTYPE(int map_free_proc,(struct vmproc *vmp));
_PROTOTYPE(int map_proc_copy,(struct vmproc *dst, struct vmproc *src));
_PROTOTYPE(struct vir_region *map_lookup,(struct vmproc *vmp, vir_bytes addr));
_PROTOTYPE(int map_pagefault,(struct vmproc *vmp,
	struct vir_region *region, vir_bytes offset, int write));
_PROTOTYPE(int map_handle_memory,(struct vmproc *vmp,
	struct vir_region *region, vir_bytes offset, vir_bytes len, int write));

_PROTOTYPE(struct vir_region * map_region_lookup_tag, (struct vmproc *vmp, u32_t tag));
_PROTOTYPE(void map_region_set_tag, (struct vir_region *vr, u32_t tag));
_PROTOTYPE(u32_t map_region_get_tag, (struct vir_region *vr));


#if SANITYCHECKS
_PROTOTYPE(void map_sanitycheck,(char *file, int line));
#endif

/* $(ARCH)/vm.c */
_PROTOTYPE( void arch_init_vm, (struct memory mem_chunks[NR_MEMS]));
_PROTOTYPE( vir_bytes, arch_map2vir(struct vmproc *vmp, vir_bytes addr));
_PROTOTYPE( vir_bytes, arch_vir2map(struct vmproc *vmp, vir_bytes addr));

