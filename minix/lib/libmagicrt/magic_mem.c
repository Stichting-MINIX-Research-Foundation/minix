
#ifdef _FILE_OFFSET_BITS
#undef _FILE_OFFSET_BITS
#endif
#define _FILE_OFFSET_BITS 64

#include <magic_mem.h>
#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include <magic_asr.h>

#ifdef __MINIX
#define util_time_tsc_read_ns(x) 0
#define util_strhash(x, y) 0
#define util_stacktrace_hash() 0
#define util_stacktrace_print_custom(x)
#define util_stacktrace_hash_skip(x) 0
#else
#include <common/util/stacktrace.h>
#include <common/util/time.h>
#include <common/util/string.h>
#endif

#define DEBUG                           MAGIC_DEBUG_SET(0)
#define DEBUG_TYPE_SIZE_MISMATCH        MAGIC_DEBUG_SET(0)

#if DEBUG
#define MAGIC_MEM_PRINTF _magic_printf
#else
#define MAGIC_MEM_PRINTF magic_null_printf
#endif

/* CPU frequency (used for timestamp logging) */
PUBLIC double magic_cycles_per_ns = 0;

/*
 * External callbacks.
 */
PUBLIC magic_mem_heap_alloc_cb_t magic_mem_heap_alloc_cb = NULL;
PUBLIC magic_mem_create_dsentry_cb_t magic_mem_create_dsentry_cb = NULL;
PUBLIC magic_mem_heap_free_cb_t magic_mem_heap_free_cb = NULL;

PUBLIC short magic_mem_create_dsentry_site_id = 0;
PUBLIC THREAD_LOCAL short magic_mem_wrapper_active = 0;
PUBLIC THREAD_LOCAL short magic_mempool_mgmt_active_level = 0;
PUBLIC THREAD_LOCAL short magic_mempool_ids[MAGIC_MEMPOOL_MAX_FUNC_RECURSIONS];

const char* const MAGIC_MEMPOOL_NAME_UNKNOWN = "_magic_mempool_unknown#";
const char* const MAGIC_MEMPOOL_NAME_DETACHED = "_magic_mempool_detached#";

__attribute__((weak)) int magic_mempool_allow_reset = 1;
__attribute__((weak)) int magic_mempool_allow_reuse = 1;
__attribute__((weak)) int magic_mempool_allow_external_alloc = 0;

#define MAGIC_MEM_FAILED    ((void*) -1)

#ifndef SHM_REMAP
#define SHM_REMAP 0
#endif

EXTERN char **environ;

PRIVATE magic_dsentry_cb_t magic_destroy_dsentry_ext_cb = NULL;

/* Magic real mem function definitions. */
PUBLIC void *(*magic_real_malloc)(size_t size) = &malloc;
PUBLIC void *(*magic_real_calloc)(size_t nmemb, size_t size) = &calloc;
PUBLIC void  (*magic_real_free)(void *ptr) = &free;
PUBLIC void *(*magic_real_realloc)(void *ptr, size_t size) = &realloc;

PUBLIC int (*magic_real_posix_memalign)(void **memptr, size_t alignment, size_t size) = &posix_memalign;

#ifndef __MINIX
PUBLIC void *(*magic_real_valloc)(size_t size) = &valloc;
PUBLIC void *(*magic_real_memalign)(size_t boundary, size_t size) = &memalign;
#endif

PUBLIC void *(*magic_real_mmap)(void *start, size_t length, int prot, int flags,
    int fd, off_t offset) = &mmap;
PUBLIC int (*magic_real_munmap)(void *start, size_t length) = &munmap;

PUBLIC int (*magic_real_brk)(void *addr) = &brk;
PUBLIC void *(*magic_real_sbrk)(intptr_t increment) = &sbrk;

#ifndef __MINIX
PUBLIC void *(*magic_real_shmat)(int shmid, const void *shmaddr, int shmflg) = &shmat;
PUBLIC int (*magic_real_shmdt)(const void *shmaddr) = &shmdt;

PUBLIC void *(*magic_real_mmap64)(void *start, size_t length, int prot, int flags,
    int fd, off_t pgoffset) = &mmap64;
#else
PUBLIC void *(*magic_real_vm_map_cacheblock)(dev_t dev, off_t dev_offset,
    ino_t ino, off_t ino_offset, u32_t *flags, int blocksize) = &vm_map_cacheblock;
#endif

/* Use magic_real* functions in the rest of the file. */
#include <magic_real_mem.h>

/* Macros for memory usage logging. */
#if MAGIC_MEM_USAGE_OUTPUT_CTL
#define MAGIC_MEM_DEBUG_PREFIX "MEM_USAGE: "
#define TIMESTAMP_STR "%llu"
#define TIMESTAMP_ARG (util_time_tsc_read_ns(magic_cycles_per_ns))
#define MAGIC_MEM_DEBUG_EVENT(EVENT, FORMAT, ...) \
	_magic_printf(MAGIC_MEM_DEBUG_PREFIX TIMESTAMP_STR " " #EVENT " " FORMAT "\n", TIMESTAMP_ARG, __VA_ARGS__)
#define MAGIC_MEM_DEBUG_EVENT_1(event, ptr)                 MAGIC_MEM_DEBUG_EVENT(event, "%u", (unsigned) ptr)
#define MAGIC_MEM_DEBUG_EVENT_2(event, ptr, type)           MAGIC_MEM_DEBUG_EVENT(event, "%u %lu", (unsigned) ptr, type)
#define MAGIC_MEM_DEBUG_EVENT_3(event, ptr, type, size)     MAGIC_MEM_DEBUG_EVENT(event, "%u %lu %d", (unsigned) ptr, type, size)

#if (MAGIC_MEM_USAGE_OUTPUT_CTL == 1)
/* use the hash of the name (extended with line number & file name) as dynamic type */
#define	MAGIC_MEM_GET_DTYPE() util_strhash(0, name)
#elif (MAGIC_MEM_USAGE_OUTPUT_CTL == 2)
/* use the hash of the stacktrace as a dynamic type */
#define	MAGIC_MEM_GET_DTYPE() util_stacktrace_hash()
#endif
#define MAGIC_MEM_DEBUG_ALLOC(ptr, size)            MAGIC_MEM_DEBUG_EVENT_3(alloc, ptr, (MAGIC_MEMPOOL_MGMT_IS_ACTIVE() ? MAGIC_MEMPOOL_GET_DTYPE() : MAGIC_MEM_GET_DTYPE()), size)
#define MAGIC_MEM_DEBUG_FREE(ptr)                   MAGIC_MEM_DEBUG_EVENT_1(dealloc, ptr)
#define MAGIC_MEM_DEBUG_RESET(ptr)                  MAGIC_MEM_DEBUG_EVENT_1(reset, ptr)
#define MAGIC_MEM_DEBUG_REUSE(ptr, type)            MAGIC_MEM_DEBUG_EVENT_2(reuse, ptr, type)
#else
#define MAGIC_MEM_GET_DTYPE() 0
#define MAGIC_MEM_DEBUG_ALLOC(ptr, size)
#define MAGIC_MEM_DEBUG_FREE(ptr)
#define MAGIC_MEM_DEBUG_RESET(ptr)
#define MAGIC_MEM_DEBUG_REUSE(ptr, type)
#endif

/*===========================================================================*
 *                       magic_mempool_alloc_id                              *
 *===========================================================================*/
MAGIC_MACRO_FUNC short magic_mempool_alloc_id(void)
{
    short i, id = -1;

    MAGIC_MPDESC_LOCK();

    /* Find a free slot. */
    for(i = 0; i < MAGIC_MAX_MEMPOOLS; i++) {
        if(MAGIC_MPDESC_IS_FREE(&_magic_mpdescs[i])) {
            MAGIC_MPDESC_ALLOC(&_magic_mpdescs[i]);
            id = i + 1;
            break;
        }
    }

    MAGIC_MPDESC_UNLOCK();

    assert((id > 0) && (id <= MAGIC_MAX_MEMPOOLS) && "Ran out of memory pool descriptors!");

    return id;
}

/*===========================================================================*
 *                       magic_mempool_create_begin                          *
 *===========================================================================*/
MAGIC_HOOK void magic_mempool_create_begin(__MDEBUG_ARGS__)
{
    MAGIC_MEMPOOL_MGMT_SET_ACTIVE();
    assert(MAGIC_MEMPOOL_MGMT_IS_ACTIVE());
    MAGIC_MEMPOOL_SET_ID(magic_mempool_alloc_id());
    MAGIC_MEMPOOL_SET_DTYPE(MAGIC_MEM_GET_DTYPE());
}

/*===========================================================================*
 *                       magic_mempool_create_end                            *
 *===========================================================================*/
MAGIC_HOOK void magic_mempool_create_end(void* addr, int indirection)
{
    void* pool;
    pool = (indirection && addr) ? *((void**)addr) : addr;
    assert(pool && "Cannot have a NULL pool pointer.");
    _magic_mpdescs[MAGIC_MEMPOOL_GET_ID() - 1].addr = pool;
    magic_mempool_mgmt_end();
}

/*===========================================================================*
 *                       magic_mempool_lookup_by_addr                        *
 *===========================================================================*/
MAGIC_MACRO_FUNC short magic_mempool_lookup_by_addr(void* addr)
{
    short i, id = MAGIC_MEMPOOL_ID_UNKNOWN;

    if (addr) {
        for(i = 0; i < MAGIC_MAX_MEMPOOLS; i++) {
            if(_magic_mpdescs[i].addr == addr) {
                id = i + 1;
                break;
            }
        }
    }

    return id;
}

/*===========================================================================*
 *                       magic_mempool_reset                                 *
 *===========================================================================*/
MAGIC_MACRO_FUNC void magic_mempool_reset(const char* mempool_name, int reset_name)
{
    struct _magic_dsentry *prev_dsentry, *dsentry, *block_dsentry;
    struct _magic_sentry* sentry;

    MAGIC_DSENTRY_LOCK();
    MAGIC_DSENTRY_MEMPOOL_ALIVE_ITER(_magic_first_mempool_dsentry, prev_dsentry, dsentry, sentry,
        if (sentry->name == mempool_name) {
            block_dsentry = MAGIC_DSENTRY_NEXT_MEMBLOCK(dsentry);
            if (block_dsentry != NULL) {
                struct _magic_dsentry *tmp_block_dsentry =
                  MAGIC_PCAS(&MAGIC_DSENTRY_NEXT_MEMBLOCK(dsentry), block_dsentry, NULL);
                assert(tmp_block_dsentry == block_dsentry && "New blocks have been allocated from a reseted mempool!");
            }
            if (reset_name) {
                const char *tmp_name =
                  MAGIC_PCAS(&sentry->name, mempool_name, MAGIC_MEMPOOL_NAME_UNKNOWN);
                assert(tmp_name == mempool_name && "The name of the mempool has changed while being reseted!");
            }
            MAGIC_MEM_DEBUG_RESET((char*)dsentry);
        }
    );

    MAGIC_DSENTRY_UNLOCK();
}

/*===========================================================================*
 *                       magic_mempool_destroy_begin                         *
 *===========================================================================*/
MAGIC_HOOK void magic_mempool_destroy_begin(void* addr, int memory_reuse)
{
    magic_mempool_mgmt_begin(addr);
    if (addr && memory_reuse) {
        assert(MAGIC_MEMPOOL_ID_IS_SET() && "Cannot destroy a pool with an unknown id.");
        magic_mempool_reset(MAGIC_MEMPOOL_GET_NAME(), TRUE);
    }
}

/*===========================================================================*
 *                       magic_mempool_destroy_end                           *
 *===========================================================================*/
MAGIC_HOOK void magic_mempool_destroy_end()
{
    MAGIC_MPDESC_LOCK();

    MAGIC_MPDESC_FREE(&_magic_mpdescs[MAGIC_MEMPOOL_GET_ID() - 1]);

    MAGIC_MPDESC_UNLOCK();

    magic_mempool_mgmt_end();
}

/*===========================================================================*
 *                       magic_mempool_mgmt_begin                            *
 *===========================================================================*/
MAGIC_HOOK void  magic_mempool_mgmt_begin(void* addr)
{
    short id;

    MAGIC_MEMPOOL_MGMT_SET_ACTIVE();
    assert(MAGIC_MEMPOOL_MGMT_IS_ACTIVE());

    id = magic_mempool_lookup_by_addr(addr);
    /* For some reason, this mempool has not been registered yet, reserve a new slot in the mempool array. */
    if (addr && (id == MAGIC_MEMPOOL_ID_UNKNOWN)) {
        id = magic_mempool_alloc_id();
        _magic_mpdescs[id - 1].addr = addr;
    }
    MAGIC_MEMPOOL_SET_ID(id);
}

/*===========================================================================*
 *                       magic_mempool_mgmt_end                              *
 *===========================================================================*/
MAGIC_HOOK void magic_mempool_mgmt_end()
{
    MAGIC_MEMPOOL_SET_ID(MAGIC_MEMPOOL_ID_UNKNOWN);
    assert(MAGIC_MEMPOOL_MGMT_IS_ACTIVE());
    MAGIC_MEMPOOL_MGMT_UNSET_ACTIVE();
}

/*===========================================================================*
 *                       magic_mempool_reset_begin                           *
 *===========================================================================*/
MAGIC_HOOK void magic_mempool_reset_begin(void* addr)
{
    magic_mempool_mgmt_begin(addr);
    /* skip reset when it has been disabled by the application. */
    if (magic_mempool_allow_reset) {
        if (addr != NULL) {
            assert(MAGIC_MEMPOOL_ID_IS_SET() && "Cannot reset a pool with an unknown id.");
            magic_mempool_reset(MAGIC_MEMPOOL_GET_NAME(), TRUE);
        }
    }
}

/*===========================================================================*
 *                       magic_mempool_dsentry_set_name                      *
 *===========================================================================*/
MAGIC_MACRO_FUNC void magic_mempool_dsentry_set_name(struct _magic_dsentry* dsentry,
	const char* name)
{
    const char *old_name, *ret;

    if ((name == MAGIC_MEMPOOL_NAME_UNKNOWN) || (name == MAGIC_MEMPOOL_NAME_DETACHED)) {
        do {
            old_name = MAGIC_DSENTRY_TO_SENTRY(dsentry)->name;
        } while (MAGIC_CAS(&MAGIC_DSENTRY_TO_SENTRY(dsentry)->name, old_name, name) != old_name && old_name != name);
    } else {
        old_name = MAGIC_DSENTRY_TO_SENTRY(dsentry)->name;
        if (old_name != name) {
            if (!strncmp(old_name, MAGIC_MEMPOOL_NAME_PREFIX, strlen(MAGIC_MEMPOOL_NAME_PREFIX))) {
                assert(((old_name == MAGIC_MEMPOOL_NAME_UNKNOWN) || (old_name == MAGIC_MEMPOOL_NAME_DETACHED))
                        && "Cannot overwrite an already existing valid memory pool name!");
            }
            ret = MAGIC_CAS(&MAGIC_DSENTRY_TO_SENTRY(dsentry)->name, old_name, name);
            assert((ret == old_name || ret == name) && "Cannot overwrite an already existing valid memory pool name!");
        }
    }
}

/*===========================================================================*
 *                       magic_mempool_dsentry_update                        *
 *===========================================================================*/
MAGIC_MACRO_FUNC void magic_mempool_dsentry_update(struct _magic_dsentry* dsentry,
	const char* name)
{
    struct _magic_sentry* sentry = MAGIC_DSENTRY_TO_SENTRY(dsentry);
    struct _magic_dsentry* next_mempool_dsentry;
    int flags;
    /* set the magic state mempool flag atomically */
    do {
        flags = sentry->flags;
    } while (MAGIC_CAS(&sentry->flags, flags, flags | MAGIC_STATE_MEMPOOL) != flags && !(flags & MAGIC_STATE_MEMPOOL));
    magic_mempool_dsentry_set_name(dsentry, name);
    /* the thread that updates the id adds the dsentry to the mempool dsentry list */
    if (!(flags & MAGIC_STATE_MEMPOOL)) {
        /* Add the new dsentry before the first dsentry atomically. */
        do {
            next_mempool_dsentry = _magic_first_mempool_dsentry;
            MAGIC_DSENTRY_NEXT_MEMPOOL(dsentry) = next_mempool_dsentry;
        } while(MAGIC_CAS(&_magic_first_mempool_dsentry, next_mempool_dsentry, dsentry) != next_mempool_dsentry);
    }
}

/*===========================================================================*
 *                       mempool_block_alloc_template                        *
 *===========================================================================*/
MAGIC_FUNC void *mempool_block_alloc_template(void* addr, size_t size)
{
    return NULL;
}

/*===========================================================================*
 *                       magic_mempool_block_alloc_template                  *
 *===========================================================================*/
PUBLIC void *magic_mempool_block_alloc_template(__MA_ARGS__ void* addr, size_t size)
{
    void *ptr, *data_ptr;
    struct _magic_sentry* mempool_sentry;
    struct _magic_dsentry* mempool_dsentry, *next_block_dsentry;

    magic_mempool_mgmt_begin(addr);
    /* don't set the memory wrapper flag, this function is supposed to allocate memory from a pool
     and not using the standard allocators. it might also call other memory pool management functions */
    if(size > 0) {
        /* this call should be replaced with a call to a "real" block allocation function, when generating custom wrappers */
        ptr = mempool_block_alloc_template(addr, MAGIC_SIZE_TO_REAL(size) + magic_asr_get_padding_size(MAGIC_STATE_HEAP));
        MAGIC_MEM_PRINTF("%s: ptr = malloc(size) <-> 0x%08x = malloc(%d)\n", __FUNCTION__,(unsigned) ptr, MAGIC_SIZE_TO_REAL(size));
        data_ptr = magic_alloc(__MA_VALUES__ ptr, size, MAGIC_STATE_HEAP | MAGIC_STATE_MEMBLOCK);
        if(data_ptr == MAGIC_MEM_FAILED) {
            /* cannot free individual blocks inside the pool */
            data_ptr = NULL;
            errno = ENOMEM;
        } else if (ptr) {
            /* lookup the pool buffer dsentry from which this block was allocated */
            mempool_sentry = magic_mempool_sentry_lookup_by_range(ptr, NULL);
            if (!mempool_sentry && magic_mempool_allow_external_alloc) {
                mempool_sentry = magic_sentry_lookup_by_range(ptr, NULL);
                if (mempool_sentry) {
                    magic_mempool_dsentry_update(MAGIC_DSENTRY_FROM_SENTRY(mempool_sentry), MAGIC_MEMPOOL_GET_NAME());
                }
            }

            assert(mempool_sentry && "XXX Mempool dsentry not found for this memblock dsentry: memory not allocated from a memory pool management function?");
            mempool_dsentry = MAGIC_DSENTRY_FROM_SENTRY(mempool_sentry);

            /* Reuse of buffers across pools - propagate the new pool name */
            if (MAGIC_MEMPOOL_ID_IS_SET() && (mempool_sentry->name != MAGIC_MEMPOOL_GET_NAME())) {
                assert(magic_mempool_allow_reuse && "Pool memory reuse is disabled!");
                magic_mempool_dsentry_set_name(mempool_dsentry, MAGIC_MEMPOOL_GET_NAME());
            }
            MAGIC_DSENTRY_TO_SENTRY(MAGIC_PTR_TO_DSENTRY(MAGIC_PTR_FROM_DATA(data_ptr)))->name = mempool_sentry->name;

            /* Add the new dsentry before the first block dsentry chained to the memory pool dsentry, atomically.
             The list should be circular - the last block dsentry (first one added) points to the memory pool dsentry. */
            do {
                next_block_dsentry = MAGIC_DSENTRY_NEXT_MEMBLOCK(mempool_dsentry);
                MAGIC_DSENTRY_NEXT_MEMBLOCK(MAGIC_PTR_TO_DSENTRY(MAGIC_PTR_FROM_DATA(data_ptr))) = next_block_dsentry ? next_block_dsentry : mempool_dsentry;
            }while(MAGIC_CAS(&(MAGIC_DSENTRY_NEXT_MEMBLOCK(mempool_dsentry)), next_block_dsentry, MAGIC_PTR_TO_DSENTRY(MAGIC_PTR_FROM_DATA(data_ptr))) != next_block_dsentry);

            /* First write to this pool buffer, potential reuse */
            if (next_block_dsentry == NULL) {
            	MAGIC_MEM_DEBUG_REUSE((char*)mempool_dsentry, MAGIC_MEMPOOL_GET_DTYPE());
            }
        }
    }
    else {
        /* Some applications return a valid pointer even if size is 0... */
        data_ptr = mempool_block_alloc_template(addr, size);
    }
    magic_mempool_mgmt_end();

    return data_ptr;
}

/*===========================================================================*
 *                       magic_create_dsentry                                *
 *===========================================================================*/
PUBLIC int magic_create_dsentry(struct _magic_dsentry *dsentry,
    void *data_ptr, struct _magic_type *type, size_t size, int flags,
    const char *name, const char *parent_name)
{
    /* This function does not require any dsentry locking. */
    struct _magic_sentry *sentry = MAGIC_DSENTRY_TO_SENTRY(dsentry);
    struct _magic_dsentry *next_dsentry, *next_mempool_dsentry;
    size_t type_size;
    int is_varsized = 0;
    int num_vsa_elements = 0;

    struct _magic_dsentry *saved_next = dsentry->next;
    int save_linkage = ((flags & MAGIC_STATE_HEAP) && _magic_vars->fake_malloc);

    MAGIC_MEM_PRINTF("Dsentry created from stacktrace:\n");
    if (MAGIC_MEM_PRINTF != magic_null_printf) {
        MAGIC_MEM_WRAPPER_BEGIN();
        util_stacktrace_print_custom(MAGIC_MEM_PRINTF);
        MAGIC_MEM_WRAPPER_END();
    }

    if (!type) {
        type = MAGIC_VOID_TYPE;
    }
    type_size = type->size;
    assert(size > 0);

    memcpy(dsentry, &magic_default_dsentry, sizeof(struct _magic_dsentry));

    /* Catch variable-sized struct allocation. */
    if (magic_type_alloc_needs_varsized_array(type, size, &num_vsa_elements)) {
        is_varsized = 1;
    }

    if (size % type_size != 0 && !is_varsized) {
        /* This should only happen for uncaught variable-sized struct allocations. */
#if DEBUG_TYPE_SIZE_MISMATCH
        _magic_printf("magic_create_dsentry: type <-> size mismatch, reverting to void type: size=%d, type=", size);
        MAGIC_TYPE_PRINT(type, MAGIC_EXPAND_TYPE_STR);
        _magic_printf("\n");
#endif
        type = MAGIC_VOID_TYPE;
        type_size = type->size;
        flags |= MAGIC_STATE_TYPE_SIZE_MISMATCH;
    }

    if (size == type_size && !is_varsized) {
        sentry->type = type;
    }
    else {
        struct _magic_type *array_type = &(dsentry->type);
        MAGIC_TYPE_ARRAY_CREATE_FROM_SIZE(array_type, type,
            MAGIC_DSENTRY_TO_TYPE_ARR(dsentry), size, num_vsa_elements);
        array_type->id = MAGIC_FAA(&_magic_types_next_id, 1);
        assert(_magic_types_next_id < MAGIC_ID_MAX);
        sentry->type = array_type;
    }

    sentry->flags |= flags;
    sentry->address = data_ptr;

    if (name) {
        sentry->name = name;
    }
    if (parent_name) {
        dsentry->parent_name = parent_name;
    }
    sentry->id = MAGIC_FAA(&_magic_sentries_next_id, 1);
    assert(_magic_sentries_next_id < MAGIC_ID_MAX);

    /*
     * TODO: Also add per-callsite index to handle the following:
     * for (;;) { p = malloc(); }
     */
    if (magic_mem_create_dsentry_site_id) {
        MAGIC_MEM_WRAPPER_BEGIN();
        /*
         * XXX: This is so damn ugly, but we don't want to include
         * any magic_* functions in the stacktrace hash.
         * This should probably be done in a much more elegant manner.
         */
        dsentry->site_id = util_stacktrace_hash_skip((char *)"("MAGIC_PREFIX_STR);
        MAGIC_MEM_WRAPPER_END();
    }

    if (save_linkage) {
        dsentry->next = saved_next;
        return 0;
    }

    /* Add the new dsentry before the first dsentry atomically.
     * Skip memblock dsentries to make pool reset/destruction faster.
     */
    if (!MAGIC_STATE_FLAG(sentry, MAGIC_STATE_MEMBLOCK)) {
        do {
            next_dsentry = _magic_first_dsentry;
            MAGIC_DSENTRY_NEXT(dsentry) = next_dsentry;
        } while(MAGIC_CAS(&_magic_first_dsentry, next_dsentry, dsentry) != next_dsentry);
    }

#if MAGIC_DSENTRY_ALLOW_PREV
    next_dsentry = MAGIC_DSENTRY_NEXT(dsentry);
    if (next_dsentry) {
        MAGIC_DSENTRY_PREV(next_dsentry) = dsentry;
    }
    MAGIC_DSENTRY_PREV(dsentry) = NULL;
#endif

    if (MAGIC_STATE_FLAG(sentry, MAGIC_STATE_MEMPOOL)) {
        /* Add the new dsentry before the first mempool dsentry atomically. */
        do {
            next_mempool_dsentry = _magic_first_mempool_dsentry;
            MAGIC_DSENTRY_NEXT_MEMPOOL(dsentry) = next_mempool_dsentry;
        } while(MAGIC_CAS(&_magic_first_mempool_dsentry, next_mempool_dsentry, dsentry) != next_mempool_dsentry);
    }
    magic_update_dsentry_ranges = 1;

    if (magic_mem_create_dsentry_cb)
        magic_mem_create_dsentry_cb(dsentry);

    return 0;
}

/*===========================================================================*
 *                      magic_create_obdsentry                               *
 *===========================================================================*/
PUBLIC struct _magic_obdsentry* magic_create_obdsentry(void *data_ptr,
    struct _magic_type *type, size_t size, int flags,
    const char *name, const char *parent_name)
{
    struct _magic_obdsentry *obdsentry = NULL;
    int i, ret;

    /* Check name. */
    if(!name || !strcmp(name, "")) {
        return NULL;
    }
    else if(strlen(name) >= MAGIC_MAX_OBDSENTRY_NAME_LEN) {
        return NULL;
    }

    /* Check parent name. */
    if(!parent_name || !strcmp(parent_name, "")) {
        parent_name = MAGIC_OBDSENTRY_DEFAULT_PARENT_NAME;
    }
    if(strlen(parent_name) >= MAGIC_MAX_OBDSENTRY_PARENT_NAME_LEN) {
        return NULL;
    }

    MAGIC_MEM_WRAPPER_LBEGIN();

    /* Find a free slot. */
    for(i=0;i<MAGIC_MAX_OBDSENTRIES;i++) {
        if(MAGIC_OBDSENTRY_IS_FREE(&_magic_obdsentries[i])) {
            obdsentry = &_magic_obdsentries[i];
            break;
        }
    }
    if(!obdsentry) {
        MAGIC_MEM_WRAPPER_LEND();
        return NULL;
    }

    /* Create the dsentry. */
    strcpy(obdsentry->name, name);
    strcpy(obdsentry->parent_name, parent_name);
    flags |= MAGIC_STATE_OUT_OF_BAND;
    ret = magic_create_dsentry(MAGIC_OBDSENTRY_TO_DSENTRY(obdsentry), data_ptr, type,
        size, flags, obdsentry->name, obdsentry->parent_name);

    MAGIC_MEM_WRAPPER_LEND();

    if(ret < 0) {
        return NULL;
    }
    assert(!MAGIC_OBDSENTRY_IS_FREE(obdsentry));

    return obdsentry;
}

/*===========================================================================*
 *                       magic_update_dsentry_state                          *
 *===========================================================================*/
PUBLIC int magic_update_dsentry_state(struct _magic_dsentry *dsentry,
    unsigned long state)
{
    int ret = 0;
    unsigned long old_state;
    unsigned long num_dead_dsentries;
    unsigned long size, size_dead_dsentries;
    size = MAGIC_DSENTRY_TO_SENTRY(dsentry)->type->size;

    switch(state) {
        case MAGIC_DSENTRY_MSTATE_FREED:
            old_state = MAGIC_CAS(&dsentry->magic_state, MAGIC_DSENTRY_MSTATE_DEAD,
                MAGIC_DSENTRY_MSTATE_FREED);
            if(old_state != MAGIC_DSENTRY_MSTATE_DEAD) {
                ret = MAGIC_EBADMSTATE;
                break;
            }
            if (!MAGIC_STATE_FLAG(MAGIC_DSENTRY_TO_SENTRY(dsentry), MAGIC_STATE_MEMBLOCK)) {
                num_dead_dsentries = MAGIC_FAS(&magic_num_dead_dsentries, 1) - 1;
                size_dead_dsentries = MAGIC_FAS(&magic_size_dead_dsentries, size) - size;
                MAGIC_MEM_PRINTF("magic_update_dsentry_state:  --magic_num_dead_dsentries (num=%d, size=%d)\n", num_dead_dsentries, size_dead_dsentries);
            }
        break;
        case MAGIC_DSENTRY_MSTATE_DEAD:
            old_state = MAGIC_CAS(&dsentry->magic_state, MAGIC_DSENTRY_MSTATE_ALIVE,
                MAGIC_DSENTRY_MSTATE_DEAD);
            if(old_state != MAGIC_DSENTRY_MSTATE_ALIVE) {
                ret = (old_state == MAGIC_DSENTRY_MSTATE_DEAD
                    || old_state == MAGIC_DSENTRY_MSTATE_FREED)
                    ? MAGIC_EBADMSTATE : MAGIC_EBADENT;
                break;
            }
            MAGIC_DSENTRY_TO_SENTRY(dsentry)->id = MAGIC_FAA(&_magic_sentries_next_id, 1);
            assert(_magic_sentries_next_id < MAGIC_ID_MAX);
            if (!MAGIC_STATE_FLAG(MAGIC_DSENTRY_TO_SENTRY(dsentry), MAGIC_STATE_MEMBLOCK)) {
                num_dead_dsentries = MAGIC_FAA(&magic_num_dead_dsentries, 1) + 1;
                size_dead_dsentries = MAGIC_FAA(&magic_size_dead_dsentries, size) + size;
                MAGIC_MEM_PRINTF("magic_update_dsentry_state:  ++magic_num_dead_dsentries (num=%d, size=%d)\n", num_dead_dsentries, size_dead_dsentries);
                if(!magic_ignore_dead_dsentries
                    && MAGIC_DEAD_DSENTRIES_NEED_FREEING()) {
                    magic_free_dead_dsentries();
                }
            }
        break;
        default:
            ret = MAGIC_EINVAL;
        break;
    }

    return ret;
}

/*===========================================================================*
 *                            magic_free_dsentry                             *
 *===========================================================================*/
PRIVATE int magic_free_dsentry(struct _magic_dsentry *dsentry)
{
    int ret = 0;
    struct _magic_sentry *sentry = MAGIC_DSENTRY_TO_SENTRY(dsentry);
    int region = MAGIC_STATE_REGION(sentry);
    void *ptr = MAGIC_PTR_FROM_DSENTRY(dsentry);
    size_t page_size, size;
    void *data_ptr, *aligned_ptr;
    int from_wrapper = MAGIC_MEM_WRAPPER_IS_ACTIVE();
    assert(dsentry->magic_number == MAGIC_DSENTRY_MNUM_NULL);

    if (!from_wrapper) {
        MAGIC_MEM_WRAPPER_BEGIN();
    }

    if (magic_mem_heap_free_cb) {
        ret = magic_mem_heap_free_cb(dsentry);
        if (ret != MAGIC_ENOENT)
            return ret;

        /*
         * If the callback returned MAGIC_ENOENT, fallback to
         * the default behavior.
         */
        ret = 0;
    }

    /* A MAP_SHARED region will have both MAGIC_STATE_MAP and MAGIC_STATE_SHM. */
    if (region == (MAGIC_STATE_MAP | MAGIC_STATE_SHM))
        region = MAGIC_STATE_MAP;
    switch (region) {
        case MAGIC_STATE_HEAP:
            MAGIC_MEM_DEBUG_FREE(ptr);
            free(ptr);
        break;
        case MAGIC_STATE_MAP:
        case MAGIC_STATE_SHM:
            page_size = MAGIC_PAGE_SIZE;
            size = MAGIC_DSENTRY_TO_SENTRY(dsentry)->type->size;
            data_ptr = MAGIC_PTR_TO_DATA(ptr);
            aligned_ptr = ((char *)data_ptr) - page_size;

            if (!MAGIC_STATE_FLAG(sentry, MAGIC_STATE_DETACHED)) {
                size_t padding_size = (size_t) dsentry->ext;
                MAGIC_MEM_DEBUG_FREE(ptr);
                ret = munmap((char *)aligned_ptr, page_size + size + padding_size);
            }
            else {
#ifndef __MINIX
                if (MAGIC_STATE_FLAG(sentry, MAGIC_STATE_SHM))
                    ret = shmdt(data_ptr);
                else
#endif
                    ret = munmap(data_ptr, size);
                MAGIC_MEM_DEBUG_FREE(ptr);
                munmap(aligned_ptr, page_size);
            }
            if (ret != 0) {
                ret = MAGIC_EBADENT;
            }
        break;
        default:
            ret = MAGIC_EBADENT;
        break;
    }
    if (!from_wrapper) {
        MAGIC_MEM_WRAPPER_END();
    }

    return ret;
}

/*===========================================================================*
 *                    magic_free_dead_dsentries                              *
 *===========================================================================*/
PUBLIC void magic_free_dead_dsentries()
{
    struct _magic_dsentry *prev_dsentry, *dsentry, *next_first_dsentry, *skipped_dsentry;
    struct _magic_sentry *sentry;
    unsigned long num_dead_dsentries;
    int dead_dsentries_left;
    int ret;

    MAGIC_DSENTRY_LOCK();

    skipped_dsentry = NULL;
    num_dead_dsentries = magic_num_dead_dsentries;
    next_first_dsentry = _magic_first_dsentry;
    if (next_first_dsentry) {
        /* if the first dsentry is dead, skip it to eliminate contention on the list head */
        if (next_first_dsentry->magic_state == MAGIC_DSENTRY_MSTATE_DEAD){
            num_dead_dsentries--;
        }
    }

    if(!next_first_dsentry || num_dead_dsentries == 0) {
        MAGIC_DSENTRY_UNLOCK();
        return;
    }

    MAGIC_MEM_PRINTF("magic_free_dead_dsentries: Freeing %d dead dsentries...\n", num_dead_dsentries);

    /* Eliminate the dead dsentries but always skip the first one to eliminate contention on the head. */
    do {
        dead_dsentries_left = 0;
        MAGIC_DSENTRY_ITER(next_first_dsentry->next, prev_dsentry, dsentry, sentry,
            /* normal dsentry to be freed */
            if ((dsentry->magic_state != MAGIC_DSENTRY_MSTATE_DEAD) ||
                    (magic_update_dsentry_state(dsentry, MAGIC_DSENTRY_MSTATE_FREED) < 0)) {
                next_first_dsentry = dsentry;
            } else {
                magic_destroy_dsentry(dsentry, prev_dsentry);
                ret = magic_free_dsentry(dsentry);
                if(ret != 0) {
                    _magic_printf("Warning: magic_free_dsentry failed with return code %d for: ", ret);
                    MAGIC_DSENTRY_PRINT(dsentry, MAGIC_EXPAND_TYPE_STR);
                    MAGIC_MEM_PRINTF("\n");
                }
                num_dead_dsentries--;
                dead_dsentries_left = 1;
                break;
            }
        );
    } while(dead_dsentries_left && num_dead_dsentries > 0);
    assert(num_dead_dsentries == 0);

    MAGIC_DSENTRY_UNLOCK();
}

/*===========================================================================*
 *                      magic_destroy_dsentry                                *
 *===========================================================================*/
PUBLIC void magic_destroy_dsentry(struct _magic_dsentry *dsentry,
    struct _magic_dsentry *prev_dsentry)
{
    struct _magic_dsentry *next_dsentry, *next_mempool_dsentry, *prev_mempool_dsentry = NULL;
    int dsentry_destroyed, mempool_dsentry_destroyed;

    if(magic_destroy_dsentry_ext_cb && MAGIC_DSENTRY_HAS_EXT(dsentry)) {
        magic_destroy_dsentry_ext_cb(dsentry);
    }
    if (MAGIC_STATE_FLAG(MAGIC_DSENTRY_TO_SENTRY(dsentry), MAGIC_STATE_MEMPOOL)) {
        do {
            if(!prev_mempool_dsentry) {
                if(MAGIC_DSENTRY_NEXT_MEMPOOL(dsentry) != MAGIC_DSENTRY_NEXT_MEMPOOL(_magic_first_mempool_dsentry)) {
                    prev_mempool_dsentry = magic_mempool_dsentry_prev_lookup(dsentry);
                    assert(prev_mempool_dsentry != (struct _magic_dsentry *) MAGIC_ENOPTR && "Dsentry not found!");
                }
            }
            if(prev_mempool_dsentry) {
                MAGIC_DSENTRY_NEXT_MEMPOOL(prev_mempool_dsentry) = MAGIC_DSENTRY_NEXT_MEMPOOL(dsentry);
                mempool_dsentry_destroyed = 1;
            }
            else {
                /* Remove the first dsentry atomically. */
                next_mempool_dsentry = MAGIC_DSENTRY_NEXT_MEMPOOL(dsentry);
                mempool_dsentry_destroyed = (MAGIC_CAS(&_magic_first_mempool_dsentry,
                    dsentry, next_mempool_dsentry) == dsentry);
            }
        } while(!mempool_dsentry_destroyed);
        MAGIC_DSENTRY_NEXT_MEMPOOL(dsentry) = NULL;
    }
    do {
#if MAGIC_DSENTRY_ALLOW_PREV
        prev_dsentry = MAGIC_DSENTRY_PREV(dsentry);
#else
        if(!prev_dsentry) {
            if(MAGIC_DSENTRY_NEXT(dsentry) != MAGIC_DSENTRY_NEXT(_magic_first_dsentry)) {
                prev_dsentry = magic_dsentry_prev_lookup(dsentry);
                assert(prev_dsentry != (struct _magic_dsentry *) MAGIC_ENOPTR && "Dsentry not found!");
            }
        }
#endif

        if(prev_dsentry) {
            MAGIC_DSENTRY_NEXT(prev_dsentry) = MAGIC_DSENTRY_NEXT(dsentry);
            dsentry_destroyed = 1;
        }
        else {
            /* Remove the first dsentry atomically. */
            next_dsentry = MAGIC_DSENTRY_NEXT(dsentry);
            dsentry_destroyed = (MAGIC_CAS(&_magic_first_dsentry,
                dsentry, next_dsentry) == dsentry);
        }
    } while(!dsentry_destroyed);

#if MAGIC_DSENTRY_ALLOW_PREV
    next_dsentry = MAGIC_DSENTRY_NEXT(dsentry);
    if(next_dsentry) {
        MAGIC_DSENTRY_PREV(next_dsentry) = MAGIC_DSENTRY_PREV(dsentry);
    }
    MAGIC_DSENTRY_PREV(dsentry) = NULL;
#endif

    dsentry->magic_number = MAGIC_DSENTRY_MNUM_NULL;
    MAGIC_DSENTRY_NEXT(dsentry) = NULL;
}

/*===========================================================================*
 *                       magic_destroy_obdsentry_by_addr                     *
 *===========================================================================*/
PUBLIC int magic_destroy_obdsentry_by_addr(void *data_ptr)
{
    struct _magic_sentry *sentry;
    struct _magic_dsentry *dsentry;
    struct _magic_obdsentry *obdsentry;
    int obflags = (MAGIC_STATE_DYNAMIC|MAGIC_STATE_OUT_OF_BAND);

    MAGIC_MEM_WRAPPER_LBEGIN();

    /* Lookup the obdsentry. */
    sentry = magic_sentry_lookup_by_addr(data_ptr, NULL);
    if(!sentry || ((sentry->flags & obflags) != obflags)) {
        MAGIC_MEM_WRAPPER_LEND();
        return MAGIC_EINVAL;
    }
    dsentry = MAGIC_DSENTRY_FROM_SENTRY(sentry);
    obdsentry = MAGIC_OBDSENTRY_FROM_DSENTRY(dsentry);

    /* Destroy it and free obdsentry slot. */
    magic_destroy_dsentry(dsentry, NULL);
    MAGIC_OBDSENTRY_FREE(obdsentry);

    MAGIC_MEM_WRAPPER_LEND();

    return 0;
}

/*===========================================================================*
 *                       magic_destroy_dsentry_set_ext_cb                    *
 *===========================================================================*/
PUBLIC void magic_destroy_dsentry_set_ext_cb(const magic_dsentry_cb_t cb)
{
    magic_destroy_dsentry_ext_cb = cb;
}

/*===========================================================================*
 *                            magic_update_dsentry                           *
 *===========================================================================*/
PUBLIC int magic_update_dsentry(void* addr, struct _magic_type *type)
{
    struct _magic_dsentry *prev_dsentry, *dsentry;
    struct _magic_sentry* sentry;
    size_t size, type_size;
    int is_varsized = 0;
    int num_vsa_elements = 0;

    MAGIC_DSENTRY_LOCK();
    MAGIC_DSENTRY_ALIVE_BLOCK_ITER(_magic_first_dsentry, prev_dsentry, dsentry, sentry,
        if(sentry->address == addr) {
            size = sentry->type->size;
            type_size = type->size;

            /* Catch variable-sized struct allocation. */
            if(magic_type_alloc_needs_varsized_array(type, size, &num_vsa_elements)) {
                is_varsized = 1;
            }

            if(size % type_size != 0 && !is_varsized) {
                return MAGIC_EBADENT;
            }
            if(size == type_size && !is_varsized) {
                sentry->type = type;
            }
            else {
                struct _magic_type *array_type = &(dsentry->type);
                MAGIC_TYPE_ARRAY_CREATE_FROM_SIZE(array_type, type,
                    MAGIC_DSENTRY_TO_TYPE_ARR(dsentry), size, num_vsa_elements);
                array_type->id = MAGIC_FAA(&_magic_types_next_id, 1);
                assert(_magic_types_next_id < MAGIC_ID_MAX);
                sentry->type = array_type;
            }
            return 0;
        }
    );
    MAGIC_DSENTRY_UNLOCK();

    return MAGIC_ENOENT;
}

/*===========================================================================*
 *                              magic_stack_init                             *
 *===========================================================================*/
PUBLIC void magic_stack_init()
{
    struct _magic_obdsentry *obdsentry;
    char **ptr;
    size_t size;
    void *initial_stack_bottom, *initial_stack_top;
    unsigned long* word_ptr;
    int argc;
    char **argv;

    assert(!_magic_first_stack_dsentry && !_magic_last_stack_dsentry);

    /* Find initial stack bottom and top, this should be portable enough */
    for (ptr = environ ; *ptr ; ptr++);
    if (ptr == environ){
        /* the environment is empty, and ptr still points to environ.
         * decrement ptr twice: once to point at the argv terminator,
         * and once to point to point at the last argument, which will be the stack bottom
         */
        ptr -= 2;
    } else {
        /* environment is not empty. decrement the pointer,
         * because the for loop walked past the last env variable pointer.
         */
        ptr--;
    }

    if (*ptr) {
        initial_stack_bottom = *ptr+strlen(*ptr)+1;
        word_ptr = (unsigned long*) environ;
        word_ptr--;
        assert(*word_ptr == 0); /* argv terminator */
        word_ptr--;
        argc = 0;
        while(*word_ptr != (unsigned long) argc) {
            argc++;
            word_ptr--;
        }
        argv = (char**) (word_ptr+1);
        initial_stack_top = argv;
    } else {
        /* Environ and argv empty?. Resort to defaults. */
        initial_stack_top = ptr;
        initial_stack_bottom = environ;
    }
    size = (size_t)initial_stack_bottom - (size_t)initial_stack_top + 1;

    /* Create the first stack dsentry. */
    obdsentry = magic_create_obdsentry(initial_stack_top, MAGIC_VOID_TYPE,
      size, MAGIC_STATE_STACK, MAGIC_ALLOC_INITIAL_STACK_NAME, NULL);
    assert(obdsentry);
    _magic_first_stack_dsentry = _magic_last_stack_dsentry = MAGIC_OBDSENTRY_TO_DSENTRY(obdsentry);
}

/*===========================================================================*
 *                        magic_stack_dsentries_create                       *
 *===========================================================================*/
PUBLIC void magic_stack_dsentries_create(
    struct _magic_dsentry **prev_last_stack_dsentry, int num_dsentries,
    /* struct _magic_dsentry *dsentry, struct _magic_type *type, void* data_ptr, const char* function_name, const char* name, */ ...)
{
    int i;
    struct _magic_dsentry *dsentry;
    struct _magic_type *type;
    void* data_ptr;
    const char* function_name;
    const char* name;
    char *min_data_ptr = NULL, *max_data_ptr = NULL;
    va_list va;

    assert(num_dsentries > 0);

    MAGIC_DSENTRY_LOCK();
    assert(_magic_first_stack_dsentry && "First stack dsentry not found!");
    va_start(va, num_dsentries);
    *prev_last_stack_dsentry = _magic_last_stack_dsentry;
    for (i = 0 ; i < num_dsentries ; i++) {
        dsentry = va_arg(va, struct _magic_dsentry *);
        type = va_arg(va, struct _magic_type *);
        data_ptr = va_arg(va, void *);
        function_name = va_arg(va, const char *);
        name = va_arg(va, const char *);
        if (i == num_dsentries - 1) {
            /* Return address. */
            int *value_set = (void*) type;
            data_ptr = MAGIC_FRAMEADDR_TO_RETADDR_PTR(data_ptr);
            type = &(dsentry->type);
            magic_create_dsentry(dsentry, data_ptr, MAGIC_VOID_TYPE, MAGIC_VOID_TYPE->size,
                MAGIC_STATE_STACK | MAGIC_STATE_CONSTANT | MAGIC_STATE_ADDR_NOT_TAKEN,
                name, function_name);
            memcpy(type, &magic_default_ret_addr_type, sizeof(struct _magic_type));
            type->contained_types = MAGIC_DSENTRY_TO_TYPE_ARR(dsentry);
            type->contained_types[0] = MAGIC_VOID_TYPE;
            type->value_set = value_set;
            value_set[0] = 1;
            value_set[1] = *((int *)data_ptr);

            /* Safe to override the type non-atomically.
            * The new type is only more restrictive, and nobody could
            * have touched this stack dsentry in the meantime.
            */
            MAGIC_DSENTRY_TO_SENTRY(dsentry)->type = type;
        } else {
            /* Local variable. */
            magic_create_dsentry(dsentry, data_ptr, type, type->size,
                MAGIC_STATE_STACK, name, function_name);
            if (!min_data_ptr || min_data_ptr > (char *) data_ptr) {
                min_data_ptr = (char *) data_ptr;
            }
            if (!max_data_ptr || max_data_ptr < (char *) data_ptr) {
                max_data_ptr = (char *) data_ptr;
            }
        }
    }

#if MAGIC_FORCE_DYN_MEM_ZERO_INIT
    if (min_data_ptr && max_data_ptr) {
        memset(min_data_ptr, 0, max_data_ptr - min_data_ptr + 1);
    }
#endif

    _magic_last_stack_dsentry = dsentry;
    MAGIC_DSENTRY_UNLOCK();

    va_end(va);
}

/*===========================================================================*
 *                        magic_stack_dsentries_destroy                      *
 *===========================================================================*/
PUBLIC void magic_stack_dsentries_destroy(
    struct _magic_dsentry **prev_last_stack_dsentry, int num_dsentries,
    /* struct _magic_dsentry *dsentry, */ ...)
{
    int i;
    struct _magic_dsentry *dsentry;
    va_list va;

    assert(num_dsentries > 0);

    MAGIC_MEM_WRAPPER_LBEGIN();

    va_start(va, num_dsentries);
    _magic_last_stack_dsentry = *prev_last_stack_dsentry;
    for (i = 0 ; i < num_dsentries ; i++) {
        dsentry = va_arg(va, struct _magic_dsentry *);
        magic_destroy_dsentry(dsentry, NULL);
    }
    va_end(va);

    MAGIC_MEM_WRAPPER_LEND();
}

/*===========================================================================*
 *                         magic_create_dfunction                            *
 *===========================================================================*/
PUBLIC int magic_create_dfunction(struct _magic_dfunction *dfunction,
    void *data_ptr, struct _magic_type *type, int flags,
    const char *name, const char *parent_name)
{
    struct _magic_function *function = MAGIC_DFUNCTION_TO_FUNCTION(dfunction);

    if(!type) {
        type = MAGIC_VOID_TYPE;
    }

    memcpy(dfunction, &magic_default_dfunction, sizeof(struct _magic_dfunction));

    assert(!(type->flags & MAGIC_TYPE_DYNAMIC) && "bad type!");

    function->type = type;
    function->flags |= flags;
    function->address = data_ptr;
    if(name) {
        function->name = name;
    }
    if(parent_name) {
        dfunction->parent_name = parent_name;
    }
    function->id = MAGIC_FAA(&_magic_functions_next_id, 1);
    assert(_magic_functions_next_id < MAGIC_ID_MAX);

    if(_magic_first_dfunction) {
        assert(_magic_last_dfunction);
        MAGIC_DFUNCTION_NEXT(_magic_last_dfunction) = dfunction;
    }
    else {
        assert(!_magic_last_dfunction);
        assert(_magic_dfunctions_num == 0);
        _magic_first_dfunction = dfunction;
    }
    MAGIC_DFUNCTION_PREV(dfunction) = _magic_last_dfunction;
    MAGIC_DFUNCTION_NEXT(dfunction) = NULL;
    _magic_last_dfunction = dfunction;
    _magic_dfunctions_num++;

    assert(magic_check_dfunction(dfunction, 0) && "Bad magic dfunction created!");
    magic_update_dfunction_ranges = 1;

    return 0;
}

/*===========================================================================*
 *                         magic_destroy_dfunction                           *
 *===========================================================================*/
PUBLIC void magic_destroy_dfunction(struct _magic_dfunction *dfunction)
{
    dfunction->magic_number = MAGIC_DFUNCTION_MNUM_NULL;
    if(MAGIC_DFUNCTION_HAS_NEXT(dfunction)) {
        MAGIC_DFUNCTION_PREV(MAGIC_DFUNCTION_NEXT(dfunction)) = MAGIC_DFUNCTION_PREV(dfunction);
    }
    else {
        _magic_last_dfunction = MAGIC_DFUNCTION_PREV(dfunction);
    }
    if(MAGIC_DFUNCTION_HAS_PREV(dfunction)) {
        MAGIC_DFUNCTION_NEXT(MAGIC_DFUNCTION_PREV(dfunction)) = MAGIC_DFUNCTION_NEXT(dfunction);
    }
    else {
        _magic_first_dfunction = MAGIC_DFUNCTION_NEXT(dfunction);
    }
    MAGIC_DFUNCTION_NEXT(dfunction) = NULL;
    MAGIC_DFUNCTION_PREV(dfunction) = NULL;
    _magic_dfunctions_num--;
    if(_magic_dfunctions_num == 0) {
        assert(!_magic_first_dfunction && !_magic_last_dfunction);
    }
    else {
        assert(_magic_first_dfunction && _magic_last_dfunction);
    }
}

/*===========================================================================*
 *                          magic_create_sodesc                              *
 *===========================================================================*/
PUBLIC int magic_create_sodesc(struct _magic_sodesc *sodesc)
{
    if(_magic_first_sodesc) {
        assert(_magic_last_sodesc);
        MAGIC_SODESC_NEXT(_magic_last_sodesc) = sodesc;
    }
    else {
        assert(!_magic_last_sodesc);
        assert(_magic_sodescs_num == 0);
        _magic_first_sodesc = sodesc;
    }
    MAGIC_SODESC_PREV(sodesc) = _magic_last_sodesc;
    MAGIC_SODESC_NEXT(sodesc) = NULL;
    _magic_last_sodesc = sodesc;
    _magic_sodescs_num++;

    return 0;
}

/*===========================================================================*
 *                          magic_destroy_sodesc                             *
 *===========================================================================*/
PUBLIC int magic_destroy_sodesc(struct _magic_sodesc *sodesc)
{
    /*
     * NB!: This function requires the calling thread to already
     * hold the DSENTRY and DFUNCTION locks.
     */
    int ret;
    const char *name;
    struct _magic_dsentry *prev_dsentry, *dsentry, *last_dsentry;
    struct _magic_sentry *sentry;
    struct _magic_dfunction *dfunction, *last_dfunction;

    /*
     * Time to destroy all the dsentries and dfunctions
     * linked to the descriptor.
     */
    name = sodesc->lib.name;
    last_dsentry = NULL;
    MAGIC_DSENTRY_ALIVE_ITER(_magic_first_dsentry, prev_dsentry,
        dsentry, sentry,
        if (last_dsentry)
            magic_destroy_dsentry(last_dsentry, NULL);
        last_dsentry = dsentry->parent_name == name ? dsentry : NULL;
    );
    if (last_dsentry)
        magic_destroy_dsentry(last_dsentry, NULL);
    last_dfunction = NULL;
    MAGIC_DFUNCTION_ITER(_magic_first_dfunction, dfunction,
        if (last_dfunction)
            magic_destroy_dfunction(last_dfunction);
        last_dfunction = dfunction->parent_name == name ? dfunction : NULL;
    );
    if(last_dfunction) magic_destroy_dfunction(last_dfunction);

    /* Now get rid of the descriptor. */
    if (MAGIC_SODESC_HAS_NEXT(sodesc)) {
        MAGIC_SODESC_PREV(MAGIC_SODESC_NEXT(sodesc)) =
            MAGIC_SODESC_PREV(sodesc);
    }
    else {
        _magic_last_sodesc = MAGIC_SODESC_PREV(sodesc);
    }
    if (MAGIC_SODESC_HAS_PREV(sodesc)) {
        MAGIC_SODESC_NEXT(MAGIC_SODESC_PREV(sodesc)) =
            MAGIC_SODESC_NEXT(sodesc);
    }
    else {
        _magic_first_sodesc = MAGIC_SODESC_NEXT(sodesc);
    }
    MAGIC_SODESC_NEXT(sodesc) = NULL;
    MAGIC_SODESC_PREV(sodesc) = NULL;
    _magic_sodescs_num--;
    if (_magic_sodescs_num == 0) {
        assert(!_magic_first_sodesc && !_magic_last_sodesc);
    }
    else {
        assert(_magic_first_sodesc && _magic_last_sodesc);
    }

    /*
     * Unmap the memory area that contained the dsentries and dfunctions
     * of this descriptor.
     */
    ret = munmap(sodesc->lib.alloc_address, sodesc->lib.alloc_size);
    assert(ret == 0 && "Unable to unmap SODESC memory segment!");

    return 0;
}

/*===========================================================================*
 *                          magic_create_dsodesc                             *
 *===========================================================================*/
PUBLIC int magic_create_dsodesc(struct _magic_dsodesc *dsodesc)
{
    /*
     * NB!: This function requires the calling thread to already
     * hold the DSODESC lock.
     */
    if (_magic_first_dsodesc) {
        assert(_magic_last_dsodesc);
        MAGIC_DSODESC_NEXT(_magic_last_dsodesc) = dsodesc;
    }
    else {
        assert(!_magic_last_dsodesc);
        assert(_magic_dsodescs_num == 0);
        _magic_first_dsodesc = dsodesc;
    }
    MAGIC_DSODESC_PREV(dsodesc) = _magic_last_dsodesc;
    MAGIC_DSODESC_NEXT(dsodesc) = NULL;
    _magic_last_dsodesc = dsodesc;
    _magic_dsodescs_num++;

    return 0;
}

/*===========================================================================*
 *                          magic_destroy_dsodesc                            *
 *===========================================================================*/
PUBLIC int magic_destroy_dsodesc(struct _magic_dsodesc *dsodesc)
{
    /*
     * NB!: This function requires the calling thread to already
     * hold the DSENTRY, DFUNCTION and DSODESC locks.
     */
    int ret;
    const char *name;
    struct _magic_dsentry *prev_dsentry, *dsentry, *last_dsentry;
    struct _magic_sentry *sentry;
    struct _magic_dfunction *dfunction, *last_dfunction;

    dsodesc->ref_count--;
    /* Don't destroy the DSO descriptor quite yet, we still have references. */
    if (dsodesc->ref_count > 0) {
        return dsodesc->ref_count;
    }

    /*
     * Time to destroy all the dsentries and dfunctions
     * linked to the descriptor.
     */
    name = dsodesc->lib.name;
    last_dsentry = NULL;
    MAGIC_DSENTRY_ALIVE_ITER(_magic_first_dsentry, prev_dsentry,
        dsentry, sentry,
        if (last_dsentry)
            magic_destroy_dsentry(last_dsentry, NULL);
        last_dsentry = dsentry->parent_name == name ? dsentry : NULL;
    );
    if (last_dsentry)
        magic_destroy_dsentry(last_dsentry, NULL);
    last_dfunction = NULL;
    MAGIC_DFUNCTION_ITER(_magic_first_dfunction, dfunction,
        if (last_dfunction)
            magic_destroy_dfunction(last_dfunction);
        last_dfunction = dfunction->parent_name == name ? dfunction : NULL;
    );
    if (last_dfunction)
        magic_destroy_dfunction(last_dfunction);

    /* Now get rid of the descriptor. */
    if (MAGIC_DSODESC_HAS_NEXT(dsodesc)) {
        MAGIC_DSODESC_PREV(MAGIC_DSODESC_NEXT(dsodesc)) =
            MAGIC_DSODESC_PREV(dsodesc);
    }
    else {
        _magic_last_dsodesc = MAGIC_DSODESC_PREV(dsodesc);
    }
    if (MAGIC_DSODESC_HAS_PREV(dsodesc)) {
        MAGIC_DSODESC_NEXT(MAGIC_DSODESC_PREV(dsodesc)) =
            MAGIC_DSODESC_NEXT(dsodesc);
    }
    else {
        _magic_first_dsodesc = MAGIC_DSODESC_NEXT(dsodesc);
    }
    MAGIC_DSODESC_NEXT(dsodesc) = NULL;
    MAGIC_DSODESC_PREV(dsodesc) = NULL;
    _magic_dsodescs_num--;
    if (_magic_dsodescs_num == 0) {
        assert(!_magic_first_dsodesc && !_magic_last_dsodesc);
    }
    else {
        assert(_magic_first_dsodesc && _magic_last_dsodesc);
    }

    /*
     * Unmap the memory area that contained the dsentries and dfunctions
     * of this descriptor.
     */
    ret = munmap(dsodesc->lib.alloc_address, dsodesc->lib.alloc_size);
    assert(ret == 0 && "Unable to unmap DSODESC memory segment!");

    return 0; /* no more references, descriptor is gone. */
}

/*===========================================================================*
 *                              magic_alloc                                  *
 *===========================================================================*/
PUBLIC void *magic_alloc(__MA_ARGS__ void *ptr, size_t size, int flags)
{
    int ret;
    void *data_ptr;
    struct _magic_dsentry *dsentry;

    if(ptr == NULL) {
        return NULL;
    }
    data_ptr = MAGIC_PTR_TO_DATA(ptr);
    dsentry = MAGIC_PTR_TO_DSENTRY(ptr);
    /* Catch pool allocations and update the name & flags */
    if (MAGIC_MEMPOOL_MGMT_IS_ACTIVE() && !(flags & MAGIC_STATE_MEMBLOCK)) {
        flags |= MAGIC_STATE_MEMPOOL;
        name = MAGIC_MEMPOOL_GET_NAME();
    }
    ret = magic_create_dsentry(dsentry, data_ptr, type, size, flags, name, parent_name);
    MAGIC_MEM_PRINTF("magic_alloc: ret = magic_create_dsentry(dsentry, data_ptr, type, size, flags, NULL, NULL) <-> %d = magic_create_dsentry(0x%08x, 0x%08x, 0x%08x, %d, 0x%08x, NULL, NULL)\n", ret, (unsigned) dsentry, (unsigned) data_ptr, type, size, flags);
    if(ret < 0) {
        return MAGIC_MEM_FAILED;
    }

    /* this way we skip the memory pool blocks -they are not real allocations and should not be logged */
    if (!(flags & MAGIC_STATE_MEMBLOCK)) {
        MAGIC_MEM_DEBUG_ALLOC(ptr, (MAGIC_SIZE_TO_REAL(size)));
    }

#if DEBUG
    MAGIC_MEM_PRINTF("magic_alloc: magic_create_dsentry created sentry: ");
    MAGIC_DSENTRY_PRINT(dsentry, MAGIC_EXPAND_TYPE_STR);
    MAGIC_MEM_PRINTF("\n");
#endif

    MAGIC_MEM_PRINTF("magic_alloc: return 0x%08x\n", (unsigned) data_ptr);

    return data_ptr;
}

/*===========================================================================*
 *                           magic_malloc_positioned                         *
 *===========================================================================*/
PUBLIC void *magic_malloc_positioned(__MA_ARGS__ size_t size, void *ptr)
{
    void *data_ptr;
    int dsentry_flags = MAGIC_STATE_HEAP;

#if MAGIC_FORCE_DYN_MEM_ZERO_INIT
    assert(!_magic_vars->fake_malloc);
    do {
        return magic_calloc(__MA_VALUES__ size, 1);
    } while(0);
#endif

    MAGIC_MEM_WRAPPER_BEGIN();

    if(size > 0) {
        if (!ptr || !_magic_vars->fake_malloc) {
            /*
             * Check the external callback first.
             */
            if (magic_mem_heap_alloc_cb)
                ptr = magic_mem_heap_alloc_cb(MAGIC_SIZE_TO_REAL(size) + magic_asr_get_padding_size(MAGIC_STATE_HEAP), name, parent_name);

            if (!ptr)
                ptr = malloc(MAGIC_SIZE_TO_REAL(size) + magic_asr_get_padding_size(MAGIC_STATE_HEAP));
            MAGIC_MEM_PRINTF("magic_malloc: ptr = malloc(size) <-> 0x%08x = malloc(%d)\n", (unsigned) ptr, MAGIC_SIZE_TO_REAL(size));
        }
        data_ptr = magic_alloc(__MA_VALUES__ ptr, size, dsentry_flags);
        if (data_ptr == MAGIC_MEM_FAILED) {
            /*
             * XXX: This doesn't seem likely to happen. However, if it does,
             * we need to distinguish between regular malloc() memory
             * and super-objects. See llvm/shared/libst/include/heap.h for
             * more information.
             */
            free(ptr);
            data_ptr = NULL;
            errno = ENOMEM;
        }
        magic_heap_end = ((char *)sbrk(0)) - 1;
    }
    else {
        data_ptr = NULL;
    }

    MAGIC_MEM_WRAPPER_END();

    return data_ptr;
}

/*===========================================================================*
 *                               magic_malloc                                *
 *===========================================================================*/
PUBLIC void *magic_malloc(__MA_ARGS__ size_t size)
{
    return magic_malloc_positioned(__MA_VALUES__ size, NULL);
}

/*===========================================================================*
 *                               magic_calloc                                *
 *===========================================================================*/
PUBLIC void *magic_calloc(__MA_ARGS__ size_t nmemb, size_t size)
{
    void *ptr = NULL, *data_ptr;
    size_t real_size;
    int dsentry_flags = MAGIC_STATE_HEAP;

    MAGIC_MEM_WRAPPER_BEGIN();

    if(size > 0) {
        real_size = MAGIC_SIZE_TO_REAL(size*nmemb);
        /*
         * Check the external callback first.
         */
        if (magic_mem_heap_alloc_cb)
            ptr = magic_mem_heap_alloc_cb(real_size + magic_asr_get_padding_size(MAGIC_STATE_HEAP), name, parent_name);

        if (!ptr)
            ptr = calloc(real_size + magic_asr_get_padding_size(MAGIC_STATE_HEAP), 1);
        MAGIC_MEM_PRINTF("magic_calloc: ptr = calloc(nmemb, size) <-> 0x%08x = calloc(%d, %d)\n", (unsigned) ptr, nmemb, real_size);
        data_ptr = magic_alloc(__MA_VALUES__ ptr, size*nmemb, dsentry_flags);
        if(data_ptr == MAGIC_MEM_FAILED) {
            /*
             * XXX: This doesn't seem likely to happen. However, if it does,
             * we need to distinguish between regular malloc() memory
             * and super-objects. See llvm/shared/libst/include/heap.h for
             * more information.
             */
            free(ptr);
            data_ptr = NULL;
            errno = ENOMEM;
        }
        magic_heap_end = ((char*)sbrk(0))-1;
    }
    else {
        data_ptr = NULL;
    }

    MAGIC_MEM_WRAPPER_END();

    return data_ptr;
}

/*===========================================================================*
 *                                magic_free                                 *
 *===========================================================================*/
PUBLIC void magic_free(__MD_ARGS__ void *data_ptr)
{
    void *ptr;
    int ret;

    MAGIC_MEM_WRAPPER_BEGIN();

    if(data_ptr) {
        ptr = MAGIC_PTR_FROM_DATA(data_ptr);

        /* Check for legitimate non-indexed chunks of memory and skip. */
        if((!magic_libcommon_active || !MAGIC_USE_DYN_MEM_WRAPPERS)
            && !MAGIC_DSENTRY_MNUM_OK(MAGIC_PTR_TO_DSENTRY(ptr))) {
            MAGIC_MEM_WRAPPER_END();
            free(data_ptr);
            return;
        }

        MAGIC_MEM_PRINTF("magic_free: magic_free(0x%08x) / free(0x%08x)\n", (unsigned) data_ptr, (unsigned) ptr);
        assert(magic_check_dsentry(MAGIC_PTR_TO_DSENTRY(ptr), MAGIC_STATE_HEAP) && "XXX Bad magic dsentry: corruption or memory not allocated from a magic wrapper?");

        if(magic_allow_dead_dsentries) {
            ret = magic_update_dsentry_state(MAGIC_PTR_TO_DSENTRY(ptr), MAGIC_DSENTRY_MSTATE_DEAD);
            assert(ret == 0 && "Bad free!");
        }
        else {
            MAGIC_DSENTRY_LOCK();
            magic_destroy_dsentry(MAGIC_PTR_TO_DSENTRY(ptr), NULL);
            ret = magic_free_dsentry(MAGIC_PTR_TO_DSENTRY(ptr));
            assert(ret == 0 && "Bad free!");
            MAGIC_DSENTRY_UNLOCK();
        }
    }

    MAGIC_MEM_WRAPPER_END();
}

/*===========================================================================*
 *                              magic_realloc                                *
 *===========================================================================*/
PUBLIC void *magic_realloc(__MA_ARGS__ void *data_ptr, size_t size)
{
    void *ptr, *new_ptr, *new_data_ptr;
    size_t old_size;

    if(!data_ptr) {
        return magic_malloc(__MA_VALUES__ size);
    }
    if(size == 0) {
        magic_free(__MD_VALUES_DEFAULT__ data_ptr);
        return NULL;
    }

    ptr = MAGIC_PTR_FROM_DATA(data_ptr);
    new_data_ptr = magic_malloc(__MA_VALUES__ size);
    if(!new_data_ptr) {
        return NULL;
    }
    new_ptr = MAGIC_PTR_FROM_DATA(new_data_ptr);
    assert(magic_check_dsentry(MAGIC_PTR_TO_DSENTRY(ptr), MAGIC_STATE_HEAP) && "XXX Bad magic dsentry: corruption or memory not allocated from a magic wrapper?");
    MAGIC_MEM_PRINTF("magic_realloc: ptr = realloc(ptr, size) <-> 0x%08x = realloc(0x%08x, %d)\n", (unsigned) new_ptr, (unsigned) ptr, MAGIC_SIZE_TO_REAL(size));

    old_size = MAGIC_DSENTRY_TO_SENTRY(MAGIC_PTR_TO_DSENTRY(ptr))->type->size;
    memcpy(new_data_ptr, data_ptr, old_size < size ? old_size : size);
    magic_free(__MD_VALUES_DEFAULT__ data_ptr);

    return new_data_ptr;
}

/*===========================================================================*
 *                           magic_posix_memalign                            *
 *===========================================================================*/
PUBLIC int magic_posix_memalign(__MA_ARGS__ void **memptr, size_t alignment, size_t size)
{
    int ret = 0;
    void *ptr = NULL, *data_ptr;
    int dsentry_flags = MAGIC_STATE_HEAP;

    MAGIC_MEM_WRAPPER_BEGIN();

    if(size > 0) {
        /*
         * Check the external callback first.
         */
        if (magic_mem_heap_alloc_cb)
            ptr = magic_mem_heap_alloc_cb(MAGIC_SIZE_TO_REAL(size), name, parent_name);

        if (!ptr)
            ret = posix_memalign(&ptr, alignment, MAGIC_SIZE_TO_REAL(size));
        MAGIC_MEM_PRINTF("magic_posix_memalign: ret = posix_memalign(ptr, alignment, size) <-> %d = posix_memalign(%p, %d, %d)\n", ret, ptr, alignment, MAGIC_SIZE_TO_REAL(size));
        if(ret == 0) {
            data_ptr = magic_alloc(__MA_VALUES__ ptr, size, dsentry_flags);
            if(data_ptr == MAGIC_MEM_FAILED) {
                /*
                 * XXX: This doesn't seem likely to happen. However, if it does,
                 * we need to distinguish between regular malloc() memory
                 * and super-objects. See llvm/shared/libst/include/heap.h for
                 * more information.
                 */
                free(ptr);
                ret = ENOMEM;
            }
            else {
                *memptr = data_ptr;
#if MAGIC_FORCE_DYN_MEM_ZERO_INIT
                memset(data_ptr, 0, size);
#endif
            }
            magic_heap_end = ((char*)sbrk(0))-1;
        }
    }
    else {
        ret = EINVAL;
    }

    MAGIC_MEM_WRAPPER_END();

    return ret;
}

#ifndef __MINIX
/*===========================================================================*
 *                              magic_valloc                                 *
 *===========================================================================*/
PUBLIC void *magic_valloc(__MA_ARGS__ size_t size)
{
    return magic_memalign(__MA_VALUES__ MAGIC_PAGE_SIZE, size);
}

/*===========================================================================*
 *                              magic_memalign                               *
 *===========================================================================*/
PUBLIC void *magic_memalign(__MA_ARGS__ size_t boundary, size_t size)
{
    void *ptr;
    int ret = magic_posix_memalign(__MA_VALUES__ &ptr, boundary, size);
    if(ret != 0) {
        return NULL;
    }
    return ptr;
}

#endif

/*===========================================================================*
 *                           magic_mmap_positioned                           *
 *===========================================================================*/
PUBLIC void *magic_mmap_positioned(__MA_ARGS__ void *start, size_t length, int prot, int flags,
    int fd, off_t offset, struct _magic_dsentry *cached_dsentry)
{
    void *ptr, *data_ptr, *aligned_start, *aligned_ptr;
    void *new_ptr, *new_start;
    int dsentry_flags = MAGIC_STATE_MAP;
    size_t alloc_length;
    size_t page_size = MAGIC_PAGE_SIZE;
    int padding_type, padding_size;
    static THREAD_LOCAL int magic_is_first_mmap = 1;

    MAGIC_MEM_WRAPPER_BEGIN();

    if (flags & MAP_FIXED) {
        /* Allow safe overmapping. */
        struct _magic_sentry *sentry = magic_sentry_lookup_by_range(start, NULL);
        if (sentry && sentry == magic_sentry_lookup_by_range((char*)start+length-1, NULL))
            return mmap(start, length, prot, flags, fd, offset);
    }
    assert(!(flags & MAP_FIXED) && "MAP_FIXED may override existing mapping, currently not implemented!");
    if (length > 0) {
        if (magic_is_first_mmap) {
            magic_is_first_mmap = 0;
            padding_type = MAGIC_STATE_MAP | MAGIC_ASR_FLAG_INIT;
        } else {
            padding_type = MAGIC_STATE_MAP;
        }
        padding_size = start ? 0 : magic_asr_get_padding_size(padding_type);

        assert(MAGIC_SIZE_TO_REAL(length) <= page_size + length);
        aligned_start = start ? ((char *)start) - page_size : NULL;
        alloc_length = length + (length % page_size == 0 ? 0 : page_size - (length % page_size));
#if 0
        if (_magic_vars->do_skip_mmap) {
            ptr = cached_dsentry ? ((char *)cached_dsentry) - (padding_size + page_size - MAGIC_SIZE_TO_REAL(0)) : NULL;
        } else
#endif
        if (!(flags & MAP_ANONYMOUS) && !(flags & MAP_SHARED) && ((prot & magic_mmap_dsentry_header_prot) == magic_mmap_dsentry_header_prot)) {
            ptr = mmap(aligned_start, page_size + alloc_length + padding_size, prot, flags, fd, offset);
        }
        else {
            /* Preallocate memory for metadata + data. */
            ptr = mmap(aligned_start, page_size + alloc_length + padding_size, magic_mmap_dsentry_header_prot, MAP_ANONYMOUS | MAP_PRIVATE | (flags & MAP_FIXED), -1, 0);

            /* Remap the data part the way the caller wants us to. */
            if (ptr != MAP_FAILED) {
                new_start = ((char *)ptr) + page_size;
                new_ptr = mmap(new_start, length, prot, flags | MAP_FIXED, fd, offset);
                if (new_ptr == MAP_FAILED) {
                    munmap(ptr, page_size + alloc_length + padding_size);
                    ptr = MAP_FAILED;
                }
            }
        }
        aligned_ptr = ptr;
        MAGIC_MEM_PRINTF("magic_mmap: ptr = mmap(start, length, prot, flags, fd, offset) <-> 0x%08x = mmap(0x%08x, %d, 0x%08x, 0x%08x, %d, %d)\n", (unsigned) aligned_ptr, aligned_start, page_size + length, prot, flags, fd, offset);
        if (ptr != MAP_FAILED) {
            ptr = ((char *)ptr) + page_size - MAGIC_SIZE_TO_REAL(0);
        }
        else {
            ptr = NULL;
        }
        if (flags & MAP_SHARED)
            dsentry_flags |= MAGIC_STATE_SHM;
        data_ptr = magic_alloc(__MA_VALUES__ ptr, alloc_length, dsentry_flags);
        if (data_ptr == MAGIC_MEM_FAILED) {
            munmap(aligned_ptr, page_size + alloc_length + padding_size);
            data_ptr = NULL;
        }
        if (!data_ptr) {
            errno = ENOMEM;
            data_ptr = MAP_FAILED;
        } else {
            assert(data_ptr == (char *)aligned_ptr + page_size);
            MAGIC_PTR_TO_DSENTRY(MAGIC_PTR_FROM_DATA(data_ptr))->alloc_mmap_flags = flags;
            MAGIC_PTR_TO_DSENTRY(MAGIC_PTR_FROM_DATA(data_ptr))->alloc_mmap_prot = prot;
            MAGIC_PTR_TO_DSENTRY(MAGIC_PTR_FROM_DATA(data_ptr))->ext = (void *) padding_size;
        }
    }
    else {
        data_ptr = MAP_FAILED;
        errno = EINVAL;
    }

    MAGIC_MEM_WRAPPER_END();

    return data_ptr;
}

/*===========================================================================*
 *                               magic_mmap                                  *
 *===========================================================================*/
PUBLIC void *magic_mmap(__MA_ARGS__ void *start, size_t length, int prot, int flags,
    int fd, off_t offset)
{
    return magic_mmap_positioned(__MA_VALUES__ start, length, prot, flags, fd, offset, NULL);
}

/*===========================================================================*
 *                              magic_munmap                                 *
 *===========================================================================*/
PUBLIC int magic_munmap(__MD_ARGS__ void *data_ptr, size_t length)
{
    int ret;
    void *ptr, *aligned_ptr;
    struct _magic_sentry *sentry;
    size_t alloc_length, old_size;
    size_t page_size = MAGIC_PAGE_SIZE;

    MAGIC_MEM_WRAPPER_BEGIN();

    if(data_ptr) {
        ptr = MAGIC_PTR_FROM_DATA(data_ptr);

        /* Check for legitimate non-indexed chunks of memory and skip. */
        if((!magic_libcommon_active || !MAGIC_USE_DYN_MEM_WRAPPERS)
            && !MAGIC_DSENTRY_MNUM_OK(MAGIC_PTR_TO_DSENTRY(ptr))) {
            MAGIC_MEM_WRAPPER_END();
            return munmap(data_ptr, length);
        }

        sentry = MAGIC_DSENTRY_TO_SENTRY(MAGIC_PTR_TO_DSENTRY(ptr));
        aligned_ptr = ((char*)data_ptr) - page_size;
        MAGIC_MEM_PRINTF("magic_munmap: magic_munmap(0x%08x, %d) / unmap(0x%08x, %d)\n", (unsigned) data_ptr, length, (unsigned) aligned_ptr, page_size+length);
        assert(magic_check_dsentry(MAGIC_PTR_TO_DSENTRY(ptr), MAGIC_STATE_MAP) && "XXX Bad magic dsentry: corruption or memory not allocated from a magic wrapper?");
        old_size = MAGIC_DSENTRY_TO_SENTRY(MAGIC_PTR_TO_DSENTRY(ptr))->type->size;
        alloc_length = length + (length % page_size == 0 ? 0 : page_size-(length % page_size));

        if(alloc_length != old_size) {
            assert(alloc_length >= old_size && "Partial unmapping not supported!");
            ret = -1;
            errno = EINVAL;
        }
        else {
            if(magic_allow_dead_dsentries) {
                ret = magic_update_dsentry_state(MAGIC_PTR_TO_DSENTRY(ptr), MAGIC_DSENTRY_MSTATE_DEAD);
                assert(ret == 0 && "Bad munmap!");
            }
            else {
                MAGIC_DSENTRY_LOCK();
                magic_destroy_dsentry(MAGIC_PTR_TO_DSENTRY(ptr), NULL);
                ret = magic_free_dsentry(MAGIC_PTR_TO_DSENTRY(ptr));
                assert(ret == 0 && "Bad munmap!");
                MAGIC_DSENTRY_UNLOCK();
            }
        }
    }
    else {
        ret = -1;
        errno = EINVAL;
    }

    MAGIC_MEM_WRAPPER_END();

    return ret;
}

/*===========================================================================*
 *                               magic_brk                                   *
 *===========================================================================*/
PUBLIC int magic_brk(__MA_ARGS__ void *addr)
{
    void *ptr;
    void *break_addr;
    int ret;

    MAGIC_MEM_PRINTF("magic_brk: Warning: somebody calling magic_brk()!");
    MAGIC_MEM_WRAPPER_LBLOCK( break_addr = sbrk(0); );
    if(addr >= break_addr) {
        ptr = magic_sbrk(__MA_VALUES__ (char*)addr - (char*)break_addr);
        ret = (ptr == (void*) -1 ? -1 : 0);
        if(ret == -1) {
            errno = ENOMEM;
        }
    }
    else {
        magic_free(__MD_VALUES_DEFAULT__ addr);
        ret = 0;
    }

    return ret;
}

/*===========================================================================*
 *                              magic_sbrk                                   *
 *===========================================================================*/
PUBLIC void *magic_sbrk(__MA_ARGS__ intptr_t increment)
{
    void *ptr;

    if(increment == 0) {
        MAGIC_MEM_WRAPPER_LBLOCK( ptr = sbrk(0); );
    }
    else {
        MAGIC_MEM_PRINTF("magic_sbrk: Warning: somebody calling magic_sbrk(), resorting to magic_malloc()!");
        ptr = magic_malloc(__MA_VALUES__ increment);
    }

    return ptr;
}

#ifndef __MINIX
/*===========================================================================*
 *                             magic_shmat                                   *
 *===========================================================================*/
PUBLIC void *magic_shmat(__MA_ARGS__ int shmid, const void *shmaddr, int shmflg)
{
    void *ptr, *data_ptr, *aligned_shmaddr, *aligned_ptr;
    void *new_ptr, *new_shmaddr;
    size_t size;
    struct shmid_ds buf;
    int ret, flags;
    size_t page_size = MAGIC_PAGE_SIZE;

    MAGIC_MEM_WRAPPER_BEGIN();

    assert(!(shmflg & SHM_REMAP) && "Linux-specific SHM_REMAP not supported!");
    ret = shmctl(shmid, IPC_STAT, &buf);
    if (ret == -1) {
        MAGIC_MEM_WRAPPER_END();
        return NULL;
    }
    size = buf.shm_segsz;
    if (size > 0) {
        assert(size % page_size == 0);
        assert(MAGIC_SIZE_TO_REAL(size) <= size + page_size);
        if (shmaddr && (shmflg & SHM_RND)) {
            unsigned long shmlba = SHMLBA;
            shmflg &= ~SHM_RND;
            shmaddr = (void *) ((((unsigned long)shmaddr) / shmlba) * shmlba);
        }

        /* Preallocate memory for metadata + data. */
        aligned_shmaddr = shmaddr ? ((char *)shmaddr) - page_size : NULL;
        flags = MAP_ANONYMOUS | MAP_PRIVATE | (aligned_shmaddr ? MAP_FIXED : 0);
        ptr = mmap(aligned_shmaddr, page_size + size, magic_mmap_dsentry_header_prot, flags, -1, 0);

        /* Remap the data part the way the caller wants us to. */
        if (ptr != MAP_FAILED) {
            new_shmaddr = ((char *)ptr) + page_size;
            munmap(new_shmaddr, size);
            new_ptr = shmat(shmid, new_shmaddr, shmflg);
            if(new_ptr == (void *) -1) {
                munmap(ptr, page_size);
                ptr = MAP_FAILED;
            }
        }
        aligned_ptr = ptr;
        MAGIC_MEM_PRINTF("magic_shmat: ptr = shmat(shmid, shmaddr, shmflg) <-> 0x%08x = shmat(%d, 0x%08x, 0x%08x)\n", (unsigned) aligned_ptr, shmid, aligned_shmaddr, shmflg);
        if (ptr != MAP_FAILED) {
            ptr = ((char *)ptr) + page_size - MAGIC_SIZE_TO_REAL(0);
        }
        else {
            ptr = NULL;
        }
        data_ptr = magic_alloc(__MA_VALUES__ ptr, size, MAGIC_STATE_SHM | MAGIC_STATE_DETACHED);
        if (data_ptr == MAGIC_MEM_FAILED) {
            munmap(aligned_ptr, page_size);
            munmap(new_ptr, size);
            data_ptr = (void *) -1;
            errno = ENOMEM;
        }
        else {
            MAGIC_PTR_TO_DSENTRY(MAGIC_PTR_FROM_DATA(data_ptr))->alloc_shmat_flags = shmflg;
            MAGIC_PTR_TO_DSENTRY(MAGIC_PTR_FROM_DATA(data_ptr))->alloc_shmat_shmid = shmid;
        }
    }
    else {
        data_ptr = (void *) -1;
        errno = EINVAL;
    }

    MAGIC_MEM_WRAPPER_END();

    return data_ptr;
}

/*===========================================================================*
 *                             magic_shmdt                                   *
 *===========================================================================*/
PUBLIC int magic_shmdt(__MD_ARGS__ const void *data_ptr)
{
    int ret;
    void *ptr, *aligned_ptr;
    size_t page_size = MAGIC_PAGE_SIZE;

    MAGIC_MEM_WRAPPER_LBEGIN();

    if (data_ptr) {
        ptr = MAGIC_PTR_FROM_DATA(data_ptr);

        /* Check for legitimate non-indexed chunks of memory and skip. */
        if ((!magic_libcommon_active || !MAGIC_USE_DYN_MEM_WRAPPERS)
            && !MAGIC_DSENTRY_MNUM_OK(MAGIC_PTR_TO_DSENTRY(ptr))) {
            MAGIC_MEM_WRAPPER_LEND();
            return shmdt(data_ptr);
        }

        aligned_ptr = ((char*)data_ptr) - page_size;
        MAGIC_MEM_PRINTF("magic_shmdt: magic_shmdt(0x%08x) / shmdt(0x%08x)\n", (unsigned) data_ptr, (unsigned) aligned_ptr);
        assert(magic_check_dsentry(MAGIC_PTR_TO_DSENTRY(ptr), MAGIC_STATE_SHM) && "XXX Bad magic dsentry: corruption or memory not allocated from a magic wrapper?");
        ret = shmdt(data_ptr);
        if (ret == 0) {
            magic_destroy_dsentry(MAGIC_PTR_TO_DSENTRY(ptr), NULL);
            munmap(aligned_ptr, page_size);
        }
    }
    else {
        ret = -1;
        errno = EINVAL;
    }

    MAGIC_MEM_WRAPPER_LEND();

    return ret;
}

/*===========================================================================*
 *                               magic_mmap64                                *
 *===========================================================================*/
PUBLIC void *magic_mmap64(__MA_ARGS__ void *start, size_t length, int prot, int flags,
    int fd, off_t pgoffset)
{
    void *ptr, *data_ptr, *aligned_start, *aligned_ptr;
    void *new_ptr, *new_start;
    int dsentry_flags = MAGIC_STATE_MAP;
    size_t alloc_length;
    size_t page_size = MAGIC_PAGE_SIZE;

    MAGIC_MEM_WRAPPER_BEGIN();

    if (flags & MAP_FIXED) {
	/* Allow safe overmapping. */
	struct _magic_sentry *sentry = magic_sentry_lookup_by_range(start, NULL);
	if (sentry && sentry == magic_sentry_lookup_by_range((char*)start+length-1, NULL))
            return mmap64(start, length, prot, flags, fd, pgoffset);
    }
    assert(!(flags & MAP_FIXED) && "MAP_FIXED may override existing mapping, currently not implemented!");
    if(length > 0) {
        assert(MAGIC_SIZE_TO_REAL(length) <= page_size+length);
        aligned_start = start ? ((char*)start) - page_size : NULL;
        alloc_length = length + (length % page_size == 0 ? 0 : page_size-(length % page_size));
        if((flags & MAP_ANONYMOUS) && !(flags & MAP_SHARED) && ((prot & magic_mmap_dsentry_header_prot) == magic_mmap_dsentry_header_prot)) {
            ptr = mmap64(aligned_start, page_size+length, prot, flags, fd, pgoffset);
        }
        else {
            /* Preallocate memory for metadata + data. */
            ptr = mmap64(aligned_start, page_size+alloc_length, magic_mmap_dsentry_header_prot, MAP_ANONYMOUS|MAP_PRIVATE|(flags & MAP_FIXED), -1, 0);

            /* Remap the data part the way the caller wants us to. */
            if(ptr != MAP_FAILED) {
                new_start = ((char*)ptr) + page_size;
                new_ptr = mmap64(new_start, length, prot, flags|MAP_FIXED, fd, pgoffset);
                if(new_ptr == MAP_FAILED) {
                    munmap(ptr, page_size+alloc_length);
                    ptr = MAP_FAILED;
                }
            }
        }
        aligned_ptr = ptr;
        MAGIC_MEM_PRINTF("magic_mmap64: ptr = mmap64(start, length, prot, flags, fd, pgoffset) <-> 0x%08x = mmap64(0x%08x, %d, 0x%08x, 0x%08x, %d, %d)\n", (unsigned) aligned_ptr, aligned_start, page_size+length, prot, flags, fd, pgoffset);
        if(ptr != MAP_FAILED) {
            ptr = ((char*)ptr) + page_size - MAGIC_SIZE_TO_REAL(0);
        }
        else {
            ptr = NULL;
        }
        if (flags & MAP_SHARED)
            dsentry_flags |= MAGIC_STATE_SHM;
        data_ptr = magic_alloc(__MA_VALUES__ ptr, alloc_length, dsentry_flags);
        if(data_ptr == MAGIC_MEM_FAILED) {
            munmap(aligned_ptr, page_size+length);
            data_ptr = NULL;
            errno = ENOMEM;
        }
        if(!data_ptr) {
            errno = ENOMEM;
            data_ptr = MAP_FAILED;
        } else {
            MAGIC_PTR_TO_DSENTRY(MAGIC_PTR_FROM_DATA(data_ptr))->alloc_mmap_flags = flags;
            MAGIC_PTR_TO_DSENTRY(MAGIC_PTR_FROM_DATA(data_ptr))->alloc_mmap_prot = prot;
        }
    }
    else {
        data_ptr = MAP_FAILED;
        errno = EINVAL;
    }

    MAGIC_MEM_WRAPPER_END();

    return data_ptr;
}

#else
/*===========================================================================*
 *                               magic_vm_map_cacheblock                                *
 *===========================================================================*/
PUBLIC void *magic_vm_map_cacheblock(__MA_ARGS__ dev_t dev, off_t dev_offset,
    ino_t ino, off_t ino_offset, u32_t *flags, int length)
{
    void *ptr, *data_ptr, *aligned_ptr;
    int dsentry_flags = MAGIC_STATE_MAP;
    size_t alloc_length;
    size_t page_size = MAGIC_PAGE_SIZE;

    MAGIC_MEM_WRAPPER_BEGIN();

    if(length > 0) {
        assert(MAGIC_SIZE_TO_REAL(length) <= page_size+length);
        alloc_length = length + (length % page_size == 0 ? 0 : page_size-(length % page_size));
        data_ptr = vm_map_cacheblock(dev, dev_offset, ino, ino_offset, flags, length);
        if (data_ptr != MAP_FAILED) {
            ptr = mmap((char *)data_ptr-page_size, page_size, magic_mmap_dsentry_header_prot, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
            MAGIC_MEM_PRINTF("vm_map_cacheblock: ptr = mmap(start, length, prot, flags, fd, offset) <-> 0x%08x = mmap(0x%08x, %d, 0x%08x, 0x%08x, %d, %d)\n", (unsigned) ptr, (char *)data_ptr-page_size, page_size, magic_mmap_dsentry_header_prot, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
            assert(ptr == (char *)data_ptr-page_size); /* Ensured by VM. */
            aligned_ptr = ptr;
            ptr = ((char*)ptr) + page_size - MAGIC_SIZE_TO_REAL(0);
        }
        else {
            aligned_ptr = NULL;
            ptr = NULL;
        }
        data_ptr = magic_alloc(__MA_VALUES__ ptr, alloc_length, dsentry_flags);
        if(data_ptr == MAGIC_MEM_FAILED) {
            munmap(aligned_ptr, page_size+length);
            data_ptr = NULL;
        }
        if(!data_ptr) {
            data_ptr = MAP_FAILED;
            errno = ENOMEM;
        } else {
            MAGIC_PTR_TO_DSENTRY(MAGIC_PTR_FROM_DATA(data_ptr))->alloc_mmap_flags = MAP_ANONYMOUS | MAP_PRIVATE;
            MAGIC_PTR_TO_DSENTRY(MAGIC_PTR_FROM_DATA(data_ptr))->alloc_mmap_prot = magic_mmap_dsentry_header_prot;
        }
    }
    else {
        data_ptr = MAP_FAILED;
        errno = EINVAL;
    }

    MAGIC_MEM_WRAPPER_END();

    return data_ptr;
}

/*===========================================================================*
 *				magic_nested_mmap			     *
 *===========================================================================*/
void *
magic_nested_mmap(void *start, size_t length, int prot, int flags,
	int fd, off_t offset)
{
	void *ptr;
	int i;

	ptr = mmap(start, length, prot, flags, fd, offset);

	if (ptr != MAP_FAILED) {
		MAGIC_MEM_PRINTF("MAGIC: nested mmap (%p, %zu)\n", ptr,
		    length);

		/*
		 * Find a free entry.  We do not expect the malloc code to have
		 * more than two areas mapped at any time.
		 */
		for (i = 0; i < MAGIC_UNMAP_MEM_ENTRIES; i++)
			if (_magic_unmap_mem[i].length == 0)
				break;
		assert(i < MAGIC_UNMAP_MEM_ENTRIES);

		/* Store the mapping in this entry. */
		_magic_unmap_mem[i].start = ptr;
		_magic_unmap_mem[i].length = length;
	}

	return ptr;
}

/*===========================================================================*
 *				magic_nested_munmap			     *
 *===========================================================================*/
int
magic_nested_munmap(void *start, size_t length)
{
	int i, r;

	r = munmap(start, length);

	if (r == 0) {
		MAGIC_MEM_PRINTF("MAGIC: nested munmap (%p, %zu)\n", start,
		    length);

		/* Find the corresponding entry.  This must always succeed. */
		for (i = 0; i < MAGIC_UNMAP_MEM_ENTRIES; i++)
			if (_magic_unmap_mem[i].start == start &&
			    _magic_unmap_mem[i].length == length)
				break;
		assert(i < MAGIC_UNMAP_MEM_ENTRIES);

		/* Clear the entry. */
		_magic_unmap_mem[i].start = NULL;
		_magic_unmap_mem[i].length = 0;
	}

	return r;
}
#endif

