/* Function prototypes. */

struct vmproc;
struct stat;
struct memory;
struct vir_region;
struct phys_region;

#include <minix/ipc.h>
#include <minix/endpoint.h>
#include <minix/safecopies.h>
#include <minix/vm.h>
#include <timers.h>
#include <stdio.h>

#include "pt.h"
#include "vm.h"

/* alloc.c */
void *reservedqueue_new(int, int, int, int);
int reservedqueue_alloc(void *, phys_bytes *, void **);
void reservedqueue_add(void *, void *, phys_bytes);
void alloc_cycle(void);
void mem_sanitycheck(char *file, int line);
phys_clicks alloc_mem(phys_clicks clicks, u32_t flags);
void memstats(int *nodes, int *pages, int *largest);
void printmemstats(void);
void usedpages_reset(void);
int usedpages_add_f(phys_bytes phys, phys_bytes len, char *file, int
	line);
void free_mem(phys_clicks base, phys_clicks clicks);
void mem_add_total_pages(int pages);
#define usedpages_add(a, l) usedpages_add_f(a, l, __FILE__, __LINE__)

void mem_init(struct memory *chunks);

/* utility.c */
void get_mem_chunks(struct memory *mem_chunks);
int vm_isokendpt(endpoint_t ep, int *proc);
int get_stack_ptr(int proc_nr, vir_bytes *sp);
int do_info(message *);
int swap_proc_slot(struct vmproc *src_vmp, struct vmproc *dst_vmp);
int swap_proc_dyn_data(struct vmproc *src_vmp, struct vmproc *dst_vmp);
int do_getrusage(message *m);

/* exit.c */
void clear_proc(struct vmproc *vmp);
int do_exit(message *msg);
int do_willexit(message *msg);
int do_procctl(message *msg);
void free_proc(struct vmproc *vmp);

/* fork.c */
int do_fork(message *msg);

/* break.c */
int do_brk(message *msg);
int real_brk(struct vmproc *vmp, vir_bytes v);

/* map_mem.c */
int map_memory(endpoint_t sour, endpoint_t dest, vir_bytes virt_s,
	vir_bytes virt_d, vir_bytes length, int flag);
int unmap_memory(endpoint_t sour, endpoint_t dest, vir_bytes virt_s,
	vir_bytes virt_d, vir_bytes length, int flag);

/* mmap.c */
int do_mmap(message *msg);
int do_munmap(message *msg);
int do_map_phys(message *msg);
int do_unmap_phys(message *msg);
int do_remap(message *m);
int do_get_phys(message *m);
int do_get_refcount(message *m);
int do_vfs_mmap(message *m);

/* pagefaults.c */
void do_pagefaults(message *m);
void do_memory(void);
char *pf_errstr(u32_t err);
int handle_memory(struct vmproc *vmp, vir_bytes mem, vir_bytes len, int
	wrflag, vfs_callback_t cb, void *state, int statelen);

/* $(ARCH)/pagetable.c */
void pt_init();
void vm_freepages(vir_bytes vir, int pages);
void pt_init_mem(void);
void pt_check(struct vmproc *vmp);
int pt_new(pt_t *pt);
void pt_free(pt_t *pt);
int pt_map_in_range(struct vmproc *src_vmp, struct vmproc *dst_vmp,
	vir_bytes start, vir_bytes end);
int pt_ptmap(struct vmproc *src_vmp, struct vmproc *dst_vmp);
int pt_ptalloc_in_range(pt_t *pt, vir_bytes start, vir_bytes end, u32_t
	flags, int verify);
void pt_clearmapcache(void);
int pt_writemap(struct vmproc * vmp, pt_t *pt, vir_bytes v, phys_bytes
	physaddr, size_t bytes, u32_t flags, u32_t writemapflags);
int pt_checkrange(pt_t *pt, vir_bytes v, size_t bytes, int write);
int pt_bind(pt_t *pt, struct vmproc *who);
void *vm_mappages(phys_bytes p, int pages);
void *vm_allocpage(phys_bytes *p, int cat);
void *vm_allocpages(phys_bytes *p, int cat, int pages);
void *vm_allocpagedir(phys_bytes *p);
int pt_mapkernel(pt_t *pt);
void vm_pagelock(void *vir, int lockflag);
int vm_addrok(void *vir, int write);
int get_vm_self_pages(void);
int pt_writable(struct vmproc *vmp, vir_bytes v);

#if SANITYCHECKS
void pt_sanitycheck(pt_t *pt, char *file, int line);
#endif

/* slaballoc.c */
void *slaballoc(int bytes);
void slabfree(void *mem, int bytes);
void slabstats(void);
void slab_sanitycheck(char *file, int line);
#define SLABALLOC(var) (var = slaballoc(sizeof(*var)))
#define SLABFREE(ptr) do { slabfree(ptr, sizeof(*(ptr))); (ptr) = NULL; } while(0)
#if SANITYCHECKS

void slabunlock(void *mem, int bytes);
void slablock(void *mem, int bytes);
int slabsane_f(char *file, int line, void *mem, int bytes);
#endif

/* region.c */
void map_region_init(void);
struct vir_region * map_page_region(struct vmproc *vmp, vir_bytes min,
	vir_bytes max, vir_bytes length, u32_t flags, int mapflags,
	mem_type_t *memtype);
struct vir_region * map_proc_kernel(struct vmproc *dst);
int map_region_extend(struct vmproc *vmp, struct vir_region *vr,
	vir_bytes delta);
int map_region_extend_upto_v(struct vmproc *vmp, vir_bytes vir);
int map_unmap_region(struct vmproc *vmp, struct vir_region *vr,
	vir_bytes offset, vir_bytes len);
int map_unmap_range(struct vmproc *vmp, vir_bytes, vir_bytes);
int map_free_proc(struct vmproc *vmp);
int map_proc_copy(struct vmproc *dst, struct vmproc *src);
int map_proc_copy_from(struct vmproc *dst, struct vmproc *src, struct
	vir_region *start_src_vr);
struct vir_region *map_lookup(struct vmproc *vmp, vir_bytes addr,
	struct phys_region **pr);
int map_pf(struct vmproc *vmp, struct vir_region *region, vir_bytes
	offset, int write, vfs_callback_t pf_callback, void *state, int len,
	int *io);
int map_pin_memory(struct vmproc *vmp);
int map_handle_memory(struct vmproc *vmp, struct vir_region *region,
	vir_bytes offset, vir_bytes len, int write, vfs_callback_t cb,
		void *state, int statelen);
void map_printmap(struct vmproc *vmp);
int map_writept(struct vmproc *vmp);
void printregionstats(struct vmproc *vmp);
void map_setparent(struct vmproc *vmp);
u32_t vrallocflags(u32_t flags);
int map_free(struct vir_region *region);
struct phys_region *physblock_get(struct vir_region *region, vir_bytes offset);
void physblock_set(struct vir_region *region, vir_bytes offset,
	struct phys_region *newphysr);
int map_ph_writept(struct vmproc *vmp, struct vir_region *vr,
        struct phys_region *pr);

struct vir_region * map_region_lookup_tag(struct vmproc *vmp, u32_t
	tag);
void map_region_set_tag(struct vir_region *vr, u32_t tag);
u32_t map_region_get_tag(struct vir_region *vr);
int map_get_phys(struct vmproc *vmp, vir_bytes addr, phys_bytes *r);
int map_get_ref(struct vmproc *vmp, vir_bytes addr, u8_t *cnt);
int physregions(struct vir_region *vr);

void get_usage_info(struct vmproc *vmp, struct vm_usage_info *vui);
void get_usage_info_kernel(struct vm_usage_info *vui);
int get_region_info(struct vmproc *vmp, struct vm_region_info *vri, int
	count, vir_bytes *nextp);
int copy_abs2region(phys_bytes abs, struct vir_region *destregion,
	phys_bytes offset, phys_bytes len);
#if SANITYCHECKS
void map_sanitycheck(char *file, int line);
#endif

/* rs.c */
int do_rs_set_priv(message *m);
int do_rs_update(message *m);
int do_rs_memctl(message *m);

/* queryexit.c */
int do_query_exit(message *m);
int do_watch_exit(message *m);
int do_notify_sig(message *m);
void init_query_exit(void);

/* pb.c */
struct phys_block *pb_new(phys_bytes phys);
void pb_free(struct phys_block *);
struct phys_region *pb_reference(struct phys_block *newpb,
	vir_bytes offset, struct vir_region *region, mem_type_t *);
void pb_unreferenced(struct vir_region *region, struct phys_region *pr, int rm);
void pb_link(struct phys_region *newphysr, struct phys_block *newpb,
        vir_bytes offset, struct vir_region *parent);
int mem_cow(struct vir_region *region,
        struct phys_region *ph, phys_bytes new_page_cl, phys_bytes new_page);

/* mem_directphys.c */
void phys_setphys(struct vir_region *vr, phys_bytes startaddr);

/* mem_shared.c */
void shared_setsource(struct vir_region *vr, endpoint_t ep, struct vir_region *src);

/* mem_cache.c */
int do_mapcache(message *m);
int do_setcache(message *m);

/* cache.c */
struct cached_page *find_cached_page_bydev(dev_t dev, u64_t dev_off,
	ino_t ino, u64_t ino_off, int touchlru);
struct cached_page *find_cached_page_byino(dev_t dev, ino_t ino, u64_t ino_off, int touchlru);
int addcache(dev_t dev, u64_t def_off, ino_t ino, u64_t ino_off, struct phys_block *pb);
void cache_sanitycheck_internal(void);
int cache_freepages(int pages);
void get_stats_info(struct vm_stats_info *vsi);
void cache_lru_touch(struct cached_page *hb);
void rmcache(struct cached_page *cp);

/* vfs.c */
int vfs_request(int reqno, int fd, struct vmproc *vmp, u64_t offset,
	u32_t len, vfs_callback_t reply_callback, void *cbarg, void *state,
	int statelen);
int do_vfs_reply(message *m);

/* mem_file.c */
int mappedfile_setfile(struct vmproc *owner, struct vir_region *region,
	int fd, u64_t offset,
	dev_t dev, ino_t ino, u16_t clearend, int prefill, int mayclose);

/* fdref.c */
struct fdref *fdref_new(struct vmproc *owner, ino_t ino, dev_t dev, int fd);
struct fdref *fdref_dedup_or_new(struct vmproc *owner, ino_t ino, dev_t dev,
	int fd, int mayclose);
void fdref_ref(struct fdref *ref, struct vir_region *region);
void fdref_deref(struct vir_region *region);
void fdref_sanitycheck(void);
