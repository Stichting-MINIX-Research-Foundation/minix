#ifndef _MAGIC_STRUCTS_H
#define _MAGIC_STRUCTS_H

#include <magic_def.h>
#include <magic_common.h>
#include <stddef.h>
#include <common/ut/uthash.h>

/* Magic state type struct. */
struct _magic_type {
    _magic_id_t id;
    const char *name;
    const char **names;
    unsigned num_names;
    const char *type_str;
    unsigned size;
    unsigned num_child_types;
    struct _magic_type **contained_types;
    struct _magic_type **compatible_types;
    const char **member_names;
    unsigned *member_offsets;
    void *value_set;
    unsigned type_id;
    int flags;
    unsigned bit_width;
    const void *ext;
};

/* Magic state entry struct. */
struct _magic_sentry {
    _magic_id_t id;
    const char *name;
    struct _magic_type *type;
    int flags;
    void *address;
    void *shadow_address;
};

/* Magic state entry list struct. */
struct _magic_sentry_list {
    struct _magic_sentry *sentry;
    struct _magic_sentry_list *next;
};

/* Magic state entry hash struct. */
#define MAGIC_SENTRY_NAME_MAX_KEY_LEN       512
struct _magic_sentry_hash {
    struct _magic_sentry_list *sentry_list;
    char key[MAGIC_SENTRY_NAME_MAX_KEY_LEN];
    UT_hash_handle hh;
};

/* Magic state function struct. */
struct _magic_function {
    _magic_id_t id;
    const char *name;
    struct _magic_type *type;
    int flags;
    void *address;
};

/* Magic state function hash struct. */
struct _magic_function_hash {
    struct _magic_function *function;
    void *key;
    UT_hash_handle hh;
};

/* Magic dynamic function struct. */
struct _magic_dfunction {
    unsigned long magic_number;
    const char *parent_name;
    struct _magic_function function;
    struct _magic_dfunction *prev;
    struct _magic_dfunction *next;
};

/* Magic dynamic state index struct. */
struct _magic_dsindex {
    struct _magic_type *type;
    const char *name;
    const char *parent_name;
    int flags;
};

/* Magic dynamic state entry struct. */
#define MAGIC_DSENTRY_ALLOW_PREV            0
/*
 * The name of an externally allocated dsentry will be:
 * strlen("MAGIC_EXT_ALLOC_NAME") + strlen("MAGIC_ALLOC_NAME_SEP") +
 * strlen(0xffffffff) + strlen("MAGIC_ALLOC_NAME_SUFFIX") + 1 =
 * 4 + 1 + 10 + 1 + 1 = 17
 */
#define MAGIC_DSENTRY_EXT_NAME_BUFF_SIZE    17

struct _magic_dsentry {
    unsigned long magic_number;
    const char *parent_name;
    char name_ext_buff[MAGIC_DSENTRY_EXT_NAME_BUFF_SIZE];
    struct _magic_sentry sentry;
    struct _magic_type type;
    struct _magic_type *type_array[1];
#if MAGIC_DSENTRY_ALLOW_PREV
    struct _magic_dsentry *prev;
#endif
    struct _magic_dsentry *next;
    struct _magic_dsentry *next_mpool;
    struct _magic_dsentry *next_mblock;
    /*
     * The following 2 fields are only set if the dsentry
     * is part of a super object.
     * See llvm/shared/magic/libst/include/heap.h for more details.
     */
#ifndef __MINIX
    struct _magic_dsentry *next_sobject;
    void *sobject_base_addr;
#endif
    void *ext;
    unsigned long magic_state;
    union __alloc_flags {
        struct {
            int flags;
            int prot;
        } mmap_call;
#define mmap_flags              mmap_call.flags
#define mmap_prot               mmap_call.prot
        struct {
            int flags;
            int shmid;
        } shmat_call;
#define shmat_flags             shmat_call.flags
#define shmat_shmid             shmat_call.shmid
    } alloc_flags;
#define alloc_mmap_flags        alloc_flags.mmap_call.flags
#define alloc_mmap_prot         alloc_flags.mmap_call.prot
#define alloc_shmat_flags       alloc_flags.shmat_call.flags
#define alloc_shmat_shmid       alloc_flags.shmat_call.shmid
    _magic_id_t site_id;               /* Identifier of the call at a callsite. */
};

/* Magic out-of-band dynamic state entry struct. */
#define MAGIC_MAX_OBDSENTRIES                32
#define MAGIC_MAX_OBDSENTRY_NAME_LEN         32
#define MAGIC_MAX_OBDSENTRY_PARENT_NAME_LEN  32
struct _magic_obdsentry {
    char name[MAGIC_MAX_OBDSENTRY_NAME_LEN];
    char parent_name[MAGIC_MAX_OBDSENTRY_PARENT_NAME_LEN];
    struct _magic_dsentry dsentry;
};
EXTERN struct _magic_obdsentry _magic_obdsentries[MAGIC_MAX_OBDSENTRIES];

/* Magic memory pool state struct. */
#define MAGIC_MAX_MEMPOOLS                  1024
#define MAGIC_MAX_MEMPOOL_NAME_LEN          32
#define MAGIC_MEMPOOL_NAME_PREFIX           "_magic_mempool_"
EXTERN const char *const MAGIC_MEMPOOL_NAME_UNKNOWN;
EXTERN const char *const MAGIC_MEMPOOL_NAME_DETACHED;

struct _magic_mpdesc {
    int is_alive;
    char name[MAGIC_MAX_MEMPOOL_NAME_LEN];
    /* pointer to the pool object */
    void *addr;
#if MAGIC_MEM_USAGE_OUTPUT_CTL
    unsigned long dtype_id;
#endif
};
EXTERN struct _magic_mpdesc _magic_mpdescs[MAGIC_MAX_MEMPOOLS];

/* Magic state element. */
struct _magic_selement_s {
    struct _magic_dsentry dsentry_buff;
    struct _magic_sentry *sentry;
    const struct _magic_type *parent_type;
    void *parent_address;
    int child_num;
    const struct _magic_type *type;
    void *address;
    int depth;
    int num;
    void *cb_args;
};
typedef struct _magic_selement_s _magic_selement_t;

/* Magic external library descriptor. */
struct _magic_libdesc {
    const char *name;
    void *text_range[2];
    void *data_range[2];
    void *alloc_address;
    size_t alloc_size;
};

/* Magic SO library descriptor. */
struct _magic_sodesc {
    struct _magic_libdesc lib;
    struct _magic_sodesc *prev;
    struct _magic_sodesc *next;
};

/* Magic DSO library descriptor. */
struct _magic_dsodesc {
    struct _magic_libdesc lib;
    void *handle;
    int ref_count;
    struct _magic_dsodesc *prev;
    struct _magic_dsodesc *next;
};

/* The following constant is specific to MINIX3; on other platforms, this
 * functionality is unused altogether. On MINIX3, the libc malloc code uses
 * mmap to create page directories. Since malloc state is discarded upon state
 * transfer, we must not instrument its mmap calls in the regular way. On the
 * other hand, since mmap'ed regions are transferred to new instances, we end
 * up with a memory leak if we do not unmap those mmap'ed regions. Therefore,
 * we specifically track the mmap/munmap calls made from the malloc code, and
 * explicitly unmap its regions during state transfer. The following constant
 * defines how many ranges can be mmap'ed at once. The malloc code uses only
 * one page directory, but it may enlarge it by first allocating a new area
 * and then unmapping the old one. Therefore, we need two entries.
 */
#ifdef __MINIX
#define MAGIC_UNMAP_MEM_ENTRIES	2
#endif

/* Magic vars. */
struct _magic_vars_t {

    /* Magic Address Space Randomization (ASRPass) */
    int asr_seed;
    int asr_heap_map_do_permutate;
    int asr_heap_max_offset;
    int asr_heap_max_padding;
    int asr_map_max_offset_pages;
    int asr_map_max_padding_pages;

    /* Runtime flags. */
    int no_mem_inst;

    /* Magic type array. */
    struct _magic_type *types;
    int types_num;
    _magic_id_t types_next_id;

    /* Magic function array. */
    struct _magic_function *functions;
    int functions_num;
    _magic_id_t functions_next_id;

    /* Magic state entry array. */
    struct _magic_sentry *sentries;
    int sentries_num;
    int sentries_str_num;
    _magic_id_t sentries_next_id;

    /* Magic dynamic state index array. */
    struct _magic_dsindex *dsindexes;
    int dsindexes_num;

    /* Magic dynamic state entry list. */
    struct _magic_dsentry *first_dsentry;
    unsigned long num_dead_dsentries;
    unsigned long size_dead_dsentries;

    /* Magic memory pool dynamic state entry list. */
    struct _magic_dsentry *first_mempool_dsentry;

    /* Magic dynamic function list. */
    struct _magic_dfunction *first_dfunction;
    struct _magic_dfunction *last_dfunction;
    int dfunctions_num;

    /* Magic SO library descriptor list. */
    struct _magic_sodesc *first_sodesc;
    struct _magic_sodesc *last_sodesc;
    int sodescs_num;

    /* Magic DSO library descriptor list. */
    struct _magic_dsodesc *first_dsodesc;
    struct _magic_dsodesc *last_dsodesc;
    int dsodescs_num;

    /* Magic stack-related variables. */
    struct _magic_dsentry *first_stack_dsentry;
    struct _magic_dsentry *last_stack_dsentry;

    /* Magic memory ranges */
    void *null_range[2];
    void *data_range[2];
    void *heap_range[2];
    void *map_range[2];
    void *shm_range[2];
    void *stack_range[2];
    void *text_range[2];

    void *sentry_range[2];
    void *function_range[2];
    void *dfunction_range[2];

    void *heap_start;
    void *heap_end;
    int update_dsentry_ranges;
    int update_dfunction_ranges;

#ifdef __MINIX
    /* Memory to unmap after state transfer (MINIX3 only) */
    struct {
         void *start;
         size_t length;
    } unmap_mem[MAGIC_UNMAP_MEM_ENTRIES];
#endif

    /* Range lookup index */
    void *sentry_rl_buff;
    size_t sentry_rl_buff_offset;
    size_t sentry_rl_buff_size;
    void *sentry_rl_index;

    /* Sentry hash */
    void *sentry_hash_buff;
    size_t sentry_hash_buff_offset;
    size_t sentry_hash_buff_size;
    void *sentry_hash_head;

    /* Function hash */
    void *function_hash_buff;
    size_t function_hash_buff_offset;
    size_t function_hash_buff_size;
    void *function_hash_head;

    /*
     * Don't call malloc() in magic_malloc_positioned().
     * Used in ST after RAW COPY.
     */
    int fake_malloc;
};


#endif /* _MAGIC_STRUCTS_H */
