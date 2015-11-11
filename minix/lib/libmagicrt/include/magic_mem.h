#ifndef _MAGIC_MEM_H
#define _MAGIC_MEM_H

#include <magic.h>

#define __MA_ARGS__             struct _magic_type *type, const char *name, const char *parent_name,
#define __MA_VALUES__           type, name, parent_name,
#define __MA_VALUES_EXT__       MAGIC_VOID_TYPE, MAGIC_ALLOC_EXT_NAME, MAGIC_ALLOC_EXT_PARENT_NAME,

#define __MD_ARGS__
#define __MD_VALUES__
#define __MD_VALUES_EXT__
#define __MD_VALUES_DEFAULT__

/* External callbacks. */
typedef void *(*magic_mem_heap_alloc_cb_t)(size_t size, const char *name, const char *parent_name);
typedef void (*magic_mem_create_dsentry_cb_t)(struct _magic_dsentry *dsentry);
typedef int (*magic_mem_heap_free_cb_t)(struct _magic_dsentry *dsentry);
extern magic_mem_heap_alloc_cb_t magic_mem_heap_alloc_cb;
extern magic_mem_create_dsentry_cb_t magic_mem_create_dsentry_cb;
extern magic_mem_heap_free_cb_t magic_mem_heap_free_cb;

/* Public dsentry functions. */
typedef void (*magic_dsentry_cb_t)(struct _magic_dsentry*);
PUBLIC int magic_create_dsentry(struct _magic_dsentry *dsentry,
    void *data_ptr, struct _magic_type *type, size_t size, int flags,
    const char *name, const char *parent_name);
PUBLIC struct _magic_obdsentry* magic_create_obdsentry(void *data_ptr,
    struct _magic_type *type, size_t size, int flags,
    const char *name, const char *parent_name);
PUBLIC int magic_update_dsentry_state(struct _magic_dsentry *dsentry,
    unsigned long mstate);
PUBLIC void magic_free_dead_dsentries(void);
PUBLIC void magic_destroy_dsentry(struct _magic_dsentry *dsentry,
    struct _magic_dsentry *prev_dsentry);
PUBLIC void magic_destroy_dsentry_set_ext_cb(const magic_dsentry_cb_t cb);
PUBLIC int magic_destroy_obdsentry_by_addr(void *data_ptr);
PUBLIC int magic_update_dsentry(void* addr, struct _magic_type *type);
PUBLIC void magic_stack_dsentries_create(
    struct _magic_dsentry **prev_last_stack_dsentry, int num_dsentries,
    /* struct _magic_dsentry *dsentry, struct _magic_type *type, void* data_ptr, const char* function_name, const char* name, */ ...);
PUBLIC void magic_stack_dsentries_destroy(
    struct _magic_dsentry **prev_last_stack_dsentry, int num_dsentries,
    /* struct _magic_dsentry *dsentry, */ ...);

/* Public dfunction functions. */
PUBLIC int magic_create_dfunction(struct _magic_dfunction *dfunction,
    void *data_ptr, struct _magic_type *type, int flags,
    const char *name, const char *parent_name);
PUBLIC void magic_destroy_dfunction(struct _magic_dfunction *dfunction);

/* Public sodesc functions. */
PUBLIC int magic_create_sodesc(struct _magic_sodesc *sodesc);
PUBLIC int magic_destroy_sodesc(struct _magic_sodesc *sodesc);

/* Public dsodesc functions. */
PUBLIC int magic_create_dsodesc(struct _magic_dsodesc *dsodesc);
PUBLIC int magic_destroy_dsodesc(struct _magic_dsodesc *dsodesc);

/* Memory usage logging support. */
#if MAGIC_MEM_USAGE_OUTPUT_CTL
/* CPU frequency (used for timestamp generation) */
EXTERN double magic_cycles_per_ns;
#endif

/* Magic malloc wrappers. */
#include <stdlib.h>
#ifndef __MINIX
#include <malloc.h>
#endif

PUBLIC void *magic_alloc(__MA_ARGS__ void *ptr, size_t size, int flags);
PUBLIC void *magic_malloc( __MA_ARGS__ size_t size);
PUBLIC void *magic_calloc( __MA_ARGS__ size_t nmemb, size_t size);
PUBLIC void  magic_free(   __MD_ARGS__ void *ptr);
PUBLIC void *magic_realloc(__MA_ARGS__ void *ptr, size_t size);

PUBLIC void *(*magic_real_malloc)(size_t size);
PUBLIC void *(*magic_real_calloc)(size_t nmemb, size_t size);
PUBLIC void  (*magic_real_free)(void *ptr);
PUBLIC void *(*magic_real_realloc)(void *ptr, size_t size);

PUBLIC int magic_posix_memalign(__MA_ARGS__ void **memptr, size_t alignment, size_t size);
PUBLIC int (*magic_real_posix_memalign)(void **memptr, size_t alignment, size_t size);

#ifndef __MINIX
PUBLIC void *magic_valloc(      __MA_ARGS__ size_t size);
PUBLIC void *magic_memalign(    __MA_ARGS__ size_t boundary, size_t size);

PUBLIC void *(*magic_real_valloc)(size_t size);
PUBLIC void *(*magic_real_memalign)(size_t boundary, size_t size);
#endif

/* Magic mmap wrappers. */
#include <sys/mman.h>

#ifdef __MINIX
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif
#endif

#ifndef _GNU_SOURCE
void *mmap64(void *addr, size_t length, int prot, int flags, int fd, off_t pgoffset);
#endif

PUBLIC void *magic_mmap(__MA_ARGS__ void *start, size_t length, int prot, int flags,
    int fd, off_t offset);
PUBLIC int magic_munmap(__MD_ARGS__ void *start, size_t length);

PUBLIC void *(*magic_real_mmap)(void *start, size_t length, int prot, int flags,
    int fd, off_t offset);
PUBLIC int (*magic_real_munmap)(void *start, size_t length);

/* Magic brk wrappers. */
#include <unistd.h>

PUBLIC int magic_brk(   __MA_ARGS__ void *addr);
PUBLIC void *magic_sbrk(__MA_ARGS__ intptr_t increment);

PUBLIC int (*magic_real_brk)(void *addr);
PUBLIC void *(*magic_real_sbrk)(intptr_t increment);

#ifndef __MINIX
/* Magic shm wrappers. */
#include <sys/types.h>
#include <sys/shm.h>

PUBLIC void *magic_shmat(__MA_ARGS__ int shmid, const void *shmaddr, int shmflg);
PUBLIC int magic_shmdt(  __MD_ARGS__ const void *shmaddr);

PUBLIC void *(*magic_real_shmat)(int shmid, const void *shmaddr, int shmflg);
PUBLIC int (*magic_real_shmdt)(const void *shmaddr);

/* Magic other wrappers. */
PUBLIC void *magic_mmap64(__MA_ARGS__ void *start, size_t length, int prot, int flags,
    int fd, off_t pgoffset);

PUBLIC void *(*magic_real_mmap64)(void *start, size_t length, int prot, int flags,
    int fd, off_t pgoffset);
#else
#include <minix/vm.h>

PUBLIC void *magic_vm_map_cacheblock(__MA_ARGS__ dev_t dev, off_t dev_offset,
    ino_t ino, off_t ino_offset, u32_t *flags, int blocksize);
PUBLIC void *(*magic_real_vm_map_cacheblock)(dev_t dev, off_t dev_offset,
    ino_t ino, off_t ino_offset, u32_t *flags, int blocksize);
#endif

/* wrappers to skip alloction */
PUBLIC void *magic_malloc_positioned( __MA_ARGS__ size_t size, void *ptr);
PUBLIC void *magic_mmap_positioned(__MA_ARGS__ void *start, size_t length, int prot, int flags,
    int fd, off_t offset, struct _magic_dsentry *cached_dsentry);

/* Macros. */
#define MAGIC_ALLOC_SIZE                    (sizeof(struct _magic_dsentry))

#define MAGIC_SIZE_TO_REAL(S)               (S + MAGIC_ALLOC_SIZE)
#define MAGIC_SIZE_TO_SOURCE(S)             (S - MAGIC_ALLOC_SIZE)
#define MAGIC_PTR_TO_DSENTRY(P)                                                \
    ((struct _magic_dsentry *) (((char *)P)))
#define MAGIC_PTR_FROM_DSENTRY(P)           ((void *) P)
#define MAGIC_PTR_TO_DATA(P)                                                   \
    ((void *) (((char *)P) + MAGIC_ALLOC_SIZE))
#define MAGIC_PTR_FROM_DATA(P)                                                 \
    ((void *) (((char *)P) - MAGIC_ALLOC_SIZE))

/* Variables to keep track of magic mem wrappers. */
EXTERN THREAD_LOCAL short magic_mem_wrapper_active;
EXTERN short magic_mem_create_dsentry_site_id;

/* Variables to indicate if dsentry site_ids should be created. */

#if MAGIC_ALLOW_DYN_MEM_WRAPPER_NESTING
#define MAGIC_MEM_WRAPPER_IS_ACTIVE()   (magic_mem_wrapper_active > 0)
#define MAGIC_MEM_WRAPPER_BEGIN()       do {                                   \
        magic_mem_wrapper_active++;                                            \
    } while(0)
#define MAGIC_MEM_WRAPPER_END()         do {                                   \
        assert(MAGIC_MEM_WRAPPER_IS_ACTIVE());                                 \
        magic_mem_wrapper_active--;                                            \
    } while(0)
#else
#define MAGIC_MEM_WRAPPER_IS_ACTIVE()   (magic_mem_wrapper_active == 1)
#define MAGIC_MEM_WRAPPER_BEGIN()       do {                                   \
        assert(!MAGIC_MEM_WRAPPER_IS_ACTIVE());                                \
        magic_mem_wrapper_active = 1;                                          \
    } while(0)
#define MAGIC_MEM_WRAPPER_END()         do {                                   \
        assert(MAGIC_MEM_WRAPPER_IS_ACTIVE());                                 \
        magic_mem_wrapper_active = 0;                                          \
    } while(0)
#endif

#define MAGIC_MEM_WRAPPER_LBEGIN()       do {                                  \
        MAGIC_MEM_WRAPPER_BEGIN();                                             \
        MAGIC_DSENTRY_LOCK();                                                  \
    } while(0)
#define MAGIC_MEM_WRAPPER_LEND()         do {                                  \
        MAGIC_MEM_WRAPPER_END();                                               \
        MAGIC_DSENTRY_UNLOCK();                                                \
    } while(0)
#define MAGIC_MEM_WRAPPER_BLOCK(BLOCK)   do {                                  \
        MAGIC_MEM_WRAPPER_BEGIN();                                             \
        BLOCK                                                                  \
        MAGIC_MEM_WRAPPER_END();                                               \
    } while(0)
#define MAGIC_MEM_WRAPPER_LBLOCK(BLOCK)  do {                                  \
        MAGIC_MEM_WRAPPER_LBEGIN();                                            \
        BLOCK                                                                  \
        MAGIC_MEM_WRAPPER_LEND();                                              \
    } while(0)

/* Variables to keep track of memory pool management functions. */
#define MAGIC_MEMPOOL_ID_UNKNOWN            -1
#define MAGIC_MEMPOOL_ID_DETACHED           -2
#define MAGIC_MEMPOOL_MAX_FUNC_RECURSIONS   100
EXTERN THREAD_LOCAL short magic_mempool_mgmt_active_level;
EXTERN THREAD_LOCAL short magic_mempool_ids[MAGIC_MEMPOOL_MAX_FUNC_RECURSIONS];
EXTERN int magic_mempool_allow_reset;
EXTERN int magic_mempool_allow_reuse;

/* TLS flags to be set when pool management functions are active. */
#define MAGIC_MEMPOOL_MGMT_SET_ACTIVE()                                        \
    assert((++magic_mempool_mgmt_active_level <= MAGIC_MEMPOOL_MAX_FUNC_RECURSIONS) \
            && "Reached the maximum number of nested pool function calls!")
#define MAGIC_MEMPOOL_MGMT_IS_ACTIVE()                                         \
    (magic_mempool_mgmt_active_level > 0)
#define MAGIC_MEMPOOL_MGMT_UNSET_ACTIVE()                                      \
    assert((--magic_mempool_mgmt_active_level >= 0) && "Invalid nested pool call level!")
#define MAGIC_MEMPOOL_SET_ID(ID)                                               \
    (magic_mempool_ids[magic_mempool_mgmt_active_level - 1] = ID)
#define MAGIC_MEMPOOL_GET_ID()                                                 \
    (magic_mempool_ids[magic_mempool_mgmt_active_level - 1])
#define MAGIC_MEMPOOL_ID_IS_SET()                                              \
    (magic_mempool_ids[magic_mempool_mgmt_active_level - 1] > 0)
#define MAGIC_MEMPOOL_GET_NAME()                                               \
    (MAGIC_MEMPOOL_ID_IS_SET() ?                                               \
        _magic_mpdescs[MAGIC_MEMPOOL_GET_ID() - 1].name :                      \
        ((MAGIC_MEMPOOL_GET_ID() == MAGIC_MEMPOOL_ID_UNKNOWN) ?                \
            MAGIC_MEMPOOL_NAME_UNKNOWN : MAGIC_MEMPOOL_NAME_DETACHED))
/*  Store dynamic type in TLS if memory usage logging is enabled */
#if MAGIC_MEM_USAGE_OUTPUT_CTL
#define	MAGIC_MEMPOOL_SET_DTYPE(TYPE)                                          \
    do {                                                                       \
        if (MAGIC_MEMPOOL_ID_IS_SET())  {                                      \
            _magic_mpdescs[MAGIC_MEMPOOL_GET_ID() - 1].dtype_id = TYPE;        \
        }                                                                      \
    } while(0)
#define	MAGIC_MEMPOOL_GET_DTYPE()                                              \
    (MAGIC_MEMPOOL_ID_IS_SET() ?                                               \
            _magic_mpdescs[MAGIC_MEMPOOL_GET_ID() - 1].dtype_id : 0)
#else
#define	MAGIC_MEMPOOL_SET_DTYPE(TYPE)
#define	MAGIC_MEMPOOL_GET_DTYPE()   0
#endif

/* Pass call site information when logging is activated. */
#if (MAGIC_MEM_USAGE_OUTPUT_CTL == 1)
#define __MDEBUG_ARGS__             const char* name
#else
#define __MDEBUG_ARGS__
#endif
/* Specific wrapper for the memory pool creation. */
MAGIC_HOOK void magic_mempool_create_begin(__MDEBUG_ARGS__);
MAGIC_HOOK void magic_mempool_create_end(void* addr, int indirection);

/* Specific wrappers for the memory pool destruction. */
MAGIC_HOOK void magic_mempool_destroy_begin(void* addr, int memory_reuse);
MAGIC_HOOK void magic_mempool_destroy_end(void);

/* Specific wrappers for the memory pool resetting */
MAGIC_HOOK void magic_mempool_reset_begin(void* addr);

/* Generic wrappers for the rest of the memory pool management functions. */
MAGIC_HOOK void magic_mempool_mgmt_begin(void* addr);
MAGIC_HOOK void magic_mempool_mgmt_end(void);

/* Pool block allocation template function and magic wrapper. */
MAGIC_FUNC void *mempool_block_alloc_template(void* addr, size_t size);
PUBLIC void *magic_mempool_block_alloc_template(__MA_ARGS__ void* addr, size_t size);

#endif

