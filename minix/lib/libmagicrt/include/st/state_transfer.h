#ifndef ST_STATE_TRANSFER_H
#define ST_STATE_TRANSFER_H


#include <magic.h>
#include <magic_mem.h>
#include <magic_analysis.h>
#include <magic_asr.h>
#include <stdarg.h>
#include <stdint.h>

#ifdef __MINIX
#include <minix/com.h>
#include <minix/sef.h>
#endif

/*
 * Used for debugging: Iterate the type transformations X times.
 */
#define MAGIC_ST_TYPE_TRANS_ITERATIONS      1

#include <st/os_callback.h>
#include <st/special.h>

#define ST_LU_ASR                           (1 << 1)
#define ST_LU_NOMMAP                        (1 << 2)

/*
 * Type definitions. These need to be OS independent.
 */
typedef struct _st_init_info_t {
    int flags;
    void *init_buff_start;
    void *init_buff_cleanup_start;
    size_t init_buff_len;
    struct st_cbs_os_t st_cbs_os;
    void *info_opaque;
} st_init_info_t;

typedef struct st_alloc_pages {
    int num_pages;
    void *virt_addr;
    uint32_t phys_addr;
    struct st_alloc_pages *previous;
} st_alloc_pages;

#include <st/callback.h>

/* struct for holding a mapping between pointers */
typedef struct {
    void *counterpart;
} st_ptr_mapping;

/* struct for holding arrays of local counterparts to cached variables */
typedef struct {
    st_ptr_mapping *functions;
    int functions_size;
    st_ptr_mapping *types;
    st_ptr_mapping *ptr_types;
    int types_size;
    st_ptr_mapping *sentries;
    st_ptr_mapping *sentries_data;
    int sentries_size;
} st_counterparts_t;

#define ST_STATE_CHECKING_DEFAULT_MAX_CYCLES        LONG_MAX
#define ST_STATE_CHECKING_DEFAULT_MAX_VIOLATIONS          50

#define ST_SEL_ANALYZE_FLAGS (MAGIC_SEL_ANALYZE_ALL & (~MAGIC_SEL_ANALYZE_OUT_OF_BAND))

#define ST_MAX_COMPATIBLE_TYPES 32

/* Fields of _magic_vars_t that need to be zeroed out after transferring. */
#define ST_MAGIC_VARS_PTR_CLEAR_LIST                                           \
    __X(first_sodesc), __X(last_sodesc),                                       \
    __X(first_dsodesc), __X(last_dsodesc),                                     \
    __X(sentry_rl_buff), __X(sentry_rl_index),                                 \
    __X(sentry_hash_buff), __X(sentry_hash_head),                              \
    __X(function_hash_buff), __X(function_hash_head)

/* Public utility functions. */
PUBLIC int st_strcmp_wildcard(const char *with_wildcard, const char *without_wildcard);
PUBLIC void st_cb_print(int level, const char *msg, _magic_selement_t *selement, _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats, struct st_cb_info *cb_info);
PUBLIC void st_map_str_sentries(struct _magic_sentry **cached_sentry_ptr, struct _magic_sentry **local_sentry_ptr);
PUBLIC void st_map_sentries(struct _magic_sentry **cached_sentry_ptr, struct _magic_sentry **local_sentry_ptr);
PUBLIC void st_lookup_sentry_pair(struct _magic_sentry **cached_sentry_ptr, struct _magic_sentry **local_sentry_ptr);
PUBLIC void st_add_sentry_pair(struct _magic_sentry *cached_sentry, struct _magic_sentry *local_sentry);
PUBLIC int st_add_sentry_pair_alloc_by_dsindex(st_init_info_t *info, struct _magic_sentry *cached_sentry, struct _magic_dsindex *local_dsindex, int num_elements, const union __alloc_flags *p_alloc_flags);
PUBLIC void st_map_functions(struct _magic_function **cached_function_ptr, struct _magic_function **local_function_ptr);
PUBLIC void st_lookup_function_pair(struct _magic_function **cached_function_ptr, struct _magic_function **local_function_ptr);
PUBLIC void st_add_function_pair(struct _magic_function *cached_function, struct _magic_function *local_function);
PUBLIC int st_sentry_equals(struct _magic_sentry *cached_sentry, struct _magic_sentry *local_sentry);
PUBLIC int st_function_equals(struct _magic_function *cached_function, struct _magic_function *local_function);
PUBLIC void st_print_sentry_diff(st_init_info_t *info, struct _magic_sentry *cached_sentry, struct _magic_sentry *local_sentry, int raw_diff, int print_changed);
PUBLIC void st_print_function_diff(st_init_info_t *info, struct _magic_function *cached_function, struct _magic_function *local_function, int raw_diff, int print_changed);
PUBLIC void st_cb_selement_type_cast(const struct _magic_type* new_selement_type, const struct _magic_type* new_local_selement_type, _magic_selement_t *selement, _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats, struct st_cb_info *cb_info);
PUBLIC void st_map_local_selement_from_child_num(_magic_selement_t *local_selement, struct st_cb_info *cb_info, int child_num);

/* Stack refs save/restore. */
PUBLIC void st_stack_refs_save_restore(char *stack_buff, int is_save);

#define st_stack_refs_save(B)    st_stack_refs_save_restore(B,1)
#define st_stack_refs_restore(B) st_stack_refs_save_restore(B,0)

#define ST_STACK_REFS_BUFF_SIZE (sizeof(struct st_stack_refs_buff))

/*
 * The stack pointers to the environment and arguments differ from MINIX
 * to Linux.
 */
#if defined(__MINIX)
#define ST_STACK_REFS_INT_NUM       3
#define ST_STACK_REFS_INT_LIST \
    __X(environ), __X(env_argv), __X(env_argc)
#define ST_STACK_REFS_CUSTOM_NUM    0
#define ST_STACK_REFS_CUSTOM_LIST

#else
/*
 * TODO: Complete the list of Linux stack pointer aliases.
 */
#define ST_STACK_REFS_INT_NUM       3
#define ST_STACK_REFS_INT_LIST __X(_environ), __X(__progname),                 \
    __X(__progname_full)

#ifndef __USE_GNU
extern char *program_invocation_name, *program_invocation_short_name;
extern char **environ;
#endif
#define ST_STACK_REFS_CUSTOM_NUM    4
#define ST_STACK_REFS_CUSTOM_LIST __X(environ), __X(__environ),                \
    __X(program_invocation_name), __X(program_invocation_short_name)

#endif

#define ST_STACK_REFS_NUM (ST_STACK_REFS_INT_NUM + ST_STACK_REFS_CUSTOM_NUM)

struct st_stack_refs_buff {
    struct _magic_dsentry *first_stack_dsentry;
    struct _magic_dsentry *last_stack_dsentry;
    struct _magic_obdsentry first_stack_obdsentry_buff;
    void* stack_range[2];
    int stack_int_refs[ST_STACK_REFS_NUM];
};

/* Public functions for metadata transfer. */
PUBLIC int st_transfer_metadata_types(st_init_info_t *info,
    struct _magic_vars_t *cached_magic_vars,
    struct _magic_vars_t *remote_magic_vars,
    st_counterparts_t *counterparts);
PUBLIC int st_transfer_metadata_dsentries(st_init_info_t *info,
    struct _magic_vars_t *cached_magic_vars,
    struct _magic_vars_t *remote_magic_vars, st_counterparts_t *counterparts,
    size_t *max_buff_sz, int *dsentries_num);

/* Public functions for state transfer. */
PUBLIC int st_state_transfer(st_init_info_t *info);
PUBLIC void st_msync_all_shm_dsentries(void);

PUBLIC int st_init(st_init_info_t *info);
PUBLIC void st_set_status_defaults(st_init_info_t *info);
PUBLIC int st_data_transfer(st_init_info_t *info);
PUBLIC void st_cleanup(st_init_info_t *info);

PUBLIC void st_set_unpaired_types_ratios(float unpaired_types_ratio, float unpaired_struct_types_ratio);

PUBLIC struct _magic_sentry* st_cached_to_remote_sentry(st_init_info_t *info, struct _magic_sentry *cached_sentry);

PUBLIC void st_print_sentries_diff(st_init_info_t *info, int raw_diff, int print_changed);
PUBLIC void st_print_dsentries_diff(st_init_info_t *info, int raw_diff, int print_changed);
PUBLIC void st_print_functions_diff(st_init_info_t *info, int raw_diff, int print_changed);
PUBLIC void st_print_state_diff(st_init_info_t *info, int raw_diff, int print_changed);

PUBLIC void st_set_status_by_state_flags(int status_flags, int status_op, int match_state_flags, int skip_state_flags);
PUBLIC int st_set_status_by_function_ids(int status_flags, int status_op, unsigned long *ids);
PUBLIC int st_set_status_by_sentry_ids(int status_flags, int status_op, unsigned long *ids);
PUBLIC int st_set_status_by_names(int status_flags, int status_op, const char **parent_names, const char **names, _magic_id_t *dsentry_site_ids);
PUBLIC int st_set_status_by_local_addrs(int status_flags, int status_op, void **addrs);
PUBLIC void st_set_status_by_sentry(int status_flags, int status_op, void *cached_sentry);
PUBLIC void st_set_status_by_function(int status_flags, int status_op, void *cached_function);
PUBLIC int st_set_status_by_function_id(int status_flags, int status_op, unsigned long id);
PUBLIC int st_set_status_by_sentry_id(int status_flags, int status_op, unsigned long id);
PUBLIC int st_set_status_by_name(int status_flags, int status_op, const char *parent_name, const char *name, _magic_id_t dsentry_site_id);
PUBLIC int st_set_status_by_local_addr(int status_flags, int status_op, void *addr);

PUBLIC int st_pair_by_function_ids(unsigned long *cached_ids, unsigned long *local_ids, int status_flags, int status_op);
PUBLIC int st_pair_by_sentry_ids(unsigned long *cached_ids, unsigned long *local_ids, int status_flags, int status_op);
PUBLIC int st_pair_by_names(char **cached_parent_names, char **cached_names, char **local_parent_names, char **local_names, _magic_id_t *dsentry_site_ids, int status_flags, int status_op);
PUBLIC void st_pair_by_sentry(void *cached_sentry, void *local_sentry, int status_flags, int status_op);
PUBLIC void st_pair_by_function(void *cached_function, void* local_function, int status_flags, int status_op);
PUBLIC int st_pair_alloc_by_dsindex(st_init_info_t *info, void *cached_sentry, void *local_dsindex, int num_elements, const union __alloc_flags *p_alloc_flags, int status_flags, int status_op);
PUBLIC int st_pair_by_function_id(unsigned long cached_id, unsigned long local_id, int status_flags, int status_op);
PUBLIC int st_pair_by_sentry_id(unsigned long cached_id, unsigned long local_id, int status_flags, int status_op);
PUBLIC int st_pair_by_name(char *cached_parent_name, char *cached_name, char *local_parent_name, char *local_name, _magic_id_t dsentry_site_id, int status_flags, int status_op);
PUBLIC int st_pair_by_alloc_name(st_init_info_t *info, char *cached_parent_name, char *cached_name, _magic_id_t cached_dsentry_site_id, char *local_parent_name, char *local_name, _magic_id_t local_dsentry_site_id, int num_elements, const union __alloc_flags *p_alloc_flags, int status_flags, int status_op);
PUBLIC int st_pair_by_alloc_name_policies(st_init_info_t *info, char *cached_parent_name, char *cached_name, _magic_id_t cached_dsentry_site_id, char *local_parent_name, char *local_name, _magic_id_t local_dsentry_site_id, int num_elements, const union __alloc_flags *p_alloc_flags, int alloc_policies, int status_flags, int status_op);

/* Public functions for state checking. */
PUBLIC int st_do_state_cleanup(void);
PUBLIC int st_do_state_checking(void);
PUBLIC int st_state_checking_before_receive_is_enabled(void);
PUBLIC int st_state_checking_before_receive_set_enabled(int enabled, int max_cycles, int max_violations);

/* State transfer status flags and operations. */
#define ST_NEEDS_TRANSFER          0x01
#define ST_TRANSFER_DONE           0x02
#define ST_ON_PTRXFER_CASCADE      0x04
#define ST_ON_PTRXFER_SET_NULL     0x08
#define ST_ON_PTRXFER_SET_DEFAULT  0x10
#define ST_ON_PTRXFER_SKIP         0x20

#define ST_OP_NONE        0
#define ST_OP_ADD         1
#define ST_OP_DEL         2
#define ST_OP_SET         3
#define ST_OP_CLEAR       4

/* State transfer policies. */
#define ST_DEFAULT_TRANSFER_NONE                0x00000001
#define ST_DEFAULT_ALLOC_CASCADE_XFER           0x00000002
#define ST_DEFAULT_SKIP_STACK                   0x00000004
#define ST_DEFAULT_SKIP_LIB_STATE               0x00000008
#define ST_TRANSFER_DIRTY_ONLY                  0x00000010
#define ST_IXFER_UNCAUGHT_UNIONS                0x00000020
#define ST_REPORT_UNCAUGHT_UNIONS               0x00000040
#define ST_REPORT_NONXFERRED_ALLOCS             0x00000080
#define ST_REPORT_NONXFERRED_UNPAIRED_ALLOCS    0x00000100
#define ST_REPORT_UNPAIRED_SENTRIES             0x00000200
#define ST_REPORT_UNPAIRED_DSENTRIES            0x00000400
#define ST_REPORT_UNPAIRED_STRINGS              0x00000800
#define ST_REPORT_UNPAIRED_SELEMENTS            0x00001000
#define ST_MAP_NAMED_STRINGS_BY_NAME            0x00002000
#define ST_MAP_NAMED_STRINGS_BY_CONTENT         0x00004000
#define ST_MAP_STRINGS_BY_CONTENT               0x00008000
#define ST_ON_ALLOC_UNPAIR_ERROR                0x00010000
#define ST_ON_ALLOC_UNPAIR_DEALLOCATE           0x00020000
#define ST_ON_ALLOC_UNPAIR_IGNORE               0x00040000
#define ST_DEFAULT_MAP_GUARD_PTRS_TO_ARRAY_END  0x00080000
#define ST_DEFAULT_MAP_GUARD_PTRS_TO_STR_END    0x00100000
#define ST_REPORT_PRECISION_LOSS                0x00200000
#define ST_REPORT_SIGN_CHANGE                   0x00400000
#define ST_REPORT_SMALLER_ARRAYS                0x00800000
#define ST_REPORT_LARGER_ARRAYS                 0x01000000
#define ST_REPORT_SMALLER_STRINGS               0x02000000
#define ST_REPORT_LARGER_STRINGS                0x04000000

#define ST_REPORT_UNPAIRED_ALL                                                 \
    (ST_REPORT_UNPAIRED_SENTRIES | ST_REPORT_UNPAIRED_DSENTRIES |              \
     ST_REPORT_UNPAIRED_STRINGS | ST_REPORT_UNPAIRED_SELEMENTS)
#define ST_MAP_STRINGS_DEFAULT                                                 \
    (ST_MAP_NAMED_STRINGS_BY_NAME | ST_MAP_STRINGS_BY_CONTENT)
#define ST_ON_ALLOC_UNPAIR_DEFAULT          (ST_ON_ALLOC_UNPAIR_ERROR)
#define ST_ON_ALLOC_UNPAIR_MASK                                                \
    (ST_ON_ALLOC_UNPAIR_ERROR | ST_ON_ALLOC_UNPAIR_DEALLOCATE |                \
     ST_ON_ALLOC_UNPAIR_IGNORE)
#define ST_TYPE_TRANSFORM_DEFAULT                                              \
    (ST_DEFAULT_MAP_GUARD_PTRS_TO_STR_END | ST_REPORT_PRECISION_LOSS |         \
     ST_REPORT_SIGN_CHANGE | ST_REPORT_SMALLER_ARRAYS | ST_REPORT_LARGER_ARRAYS)

#define ST_POLICIES_DEFAULT_TRANSFER_ALL                                       \
    (ST_DEFAULT_ALLOC_CASCADE_XFER | ST_DEFAULT_SKIP_STACK |                   \
     ST_DEFAULT_SKIP_LIB_STATE | ST_REPORT_NONXFERRED_ALLOCS |                 \
     ST_REPORT_NONXFERRED_UNPAIRED_ALLOCS | ST_MAP_STRINGS_DEFAULT |           \
     ST_ON_ALLOC_UNPAIR_DEFAULT | ST_TYPE_TRANSFORM_DEFAULT)
#define ST_POLICIES_DEFAULT_TRANSFER_NONE                                      \
    (ST_DEFAULT_TRANSFER_NONE | ST_DEFAULT_SKIP_STACK |                        \
     ST_DEFAULT_SKIP_LIB_STATE | ST_REPORT_NONXFERRED_ALLOCS |                 \
     ST_REPORT_NONXFERRED_UNPAIRED_ALLOCS | ST_MAP_STRINGS_DEFAULT |           \
     ST_ON_ALLOC_UNPAIR_DEFAULT | ST_TYPE_TRANSFORM_DEFAULT)
#define ST_POLICIES_DEFAULT_TRANSFER_ASR                                       \
    (ST_POLICIES_DEFAULT_TRANSFER_ALL | ST_REPORT_UNPAIRED_ALL)

#define ST_POLICIES_DEFAULT                 ST_POLICIES_DEFAULT_TRANSFER_ALL

/* Macros for predefined state transfer names. */

#define ADJUST_POINTER(local, remote, pointer) ( (local) +  ((pointer) - (remote)) )
#define ST_STR_BUFF_SIZE (MAGIC_MAX_NAME_LEN > MAGIC_MAX_TYPE_STR_LEN ? MAGIC_MAX_NAME_LEN : MAGIC_MAX_TYPE_STR_LEN)
#define USE_PRE_ALLOCATED_BUFFER(INFO) ((INFO)->init_buff_start)
#define EXEC_WITH_MAGIC_VARS(CMD, MAGIC_VARS)                       \
    do {                                                            \
        struct _magic_vars_t *__saved_vars = _magic_vars;           \
        _magic_vars = (MAGIC_VARS);                                 \
        CMD;                                                        \
        _magic_vars = __saved_vars;                                 \
    } while(0);

#define MAX_NUM_TYPENAMES              20

#define CHECK_SENTITY_PAIRS            1
#define ST_DEBUG_LEVEL                 1
#define ST_DEBUG_DATA_TRANSFER         0
#define CHECK_ASR                      0

#define ST_ALLOW_RAW_UNPAIRED_TYPES            1
#define ST_UNPAIRED_TYPES_RATIO_DEFAULT        0
#define ST_UNPAIRED_STRUCT_TYPES_RATIO_DEFAULT 0
#define FORCE_SOME_UNPAIRED_TYPES              1
#define ST_TRANSFER_IDENTITY_FOR_NO_INNER_PTRS 1

#ifndef ST_ASSUME_RAW_COPY_BEFORE_TRANSFER
#define ST_ASSUME_RAW_COPY_BEFORE_TRANSFER     0
#endif

#if defined(__MINIX)
#define ST_IS_UNPAIRABLE_TYPE_OS(T) (sef_self_endpoint != VM_PROC_NR || strcmp((T)->name, "void"))
#else
#define ST_IS_UNPAIRABLE_TYPE_OS(T) 1
#endif

#if ST_ALLOW_RAW_UNPAIRED_TYPES
#define ST_IS_UNPAIRABLE_TYPE(T) (ST_IS_UNPAIRABLE_TYPE_OS(T) && (T)->type_id != MAGIC_TYPE_UNION)
#else
#define ST_IS_UNPAIRABLE_TYPE(T) \
     (ST_IS_UNPAIRABLE_TYPE_OS(T) \
     && !MAGIC_TYPE_IS_RAW_ARRAY(T) \
     && !((T)->type_id == MAGIC_TYPE_ARRAY && !strcmp((T)->contained_types[0]->type_str, "i8")) \
     && !(T)->ext)
#endif

#define ST_IS_UNPAIRABLE_STRUCT_TYPE(T) \
    ((T)->type_id == MAGIC_TYPE_STRUCT)

#define ST_FLAGS_PRINT(E) printf(", sf_flags(NDndsc)=%d%d%d%d%d%d", \
    MAGIC_STATE_EXTF_FLAG(E,ST_NEEDS_TRANSFER), MAGIC_STATE_EXTF_FLAG(E,ST_TRANSFER_DONE), \
    MAGIC_STATE_EXTF_FLAG(E,ST_ON_PTRXFER_SET_NULL), MAGIC_STATE_EXTF_FLAG(E,ST_ON_PTRXFER_SET_DEFAULT), \
    MAGIC_STATE_EXTF_FLAG(E,ST_ON_PTRXFER_SKIP), MAGIC_STATE_EXTF_FLAG(E,ST_ON_PTRXFER_CASCADE));

#define ST_SENTRY_PRINT(E,T)   do { MAGIC_SENTRY_PRINT(E, T); ST_FLAGS_PRINT(E); if (MAGIC_SENTRY_IS_STRING(E)) printf(", string=\"%s\"", (ST_SENTRY_IS_CACHED(E) ? st_lookup_str_local_data(E) : (char*)(E)->address)); } while(0)
#define ST_DSENTRY_PRINT(E,T)  do { MAGIC_DSENTRY_PRINT(E, T); ST_FLAGS_PRINT(&E->sentry); } while(0)
#define ST_FUNCTION_PRINT(E,T) do { MAGIC_FUNCTION_PRINT(E, T); ST_FLAGS_PRINT(E); } while(0)

#define ST_CHECK_INIT() assert(st_init_done && "st_init() should be called first!")

#define ST_SENTRY_IS_CACHED(E) (MAGIC_SENTRY_ID(E) >= 1 && (int)MAGIC_SENTRY_ID(E) <= st_cached_magic_vars.sentries_num && st_cached_magic_vars.sentries[MAGIC_SENTRY_ID(E)-1].address == (E)->address)
#define ST_GET_CACHED_COUNTERPART(CE,T,CT,LE) do {           \
        int _i = (CE)->id - 1;                               \
        assert(_i >= 0 && _i < st_counterparts.T##_size);    \
        LE = st_counterparts.CT[_i].counterpart;             \
    } while(0)
#define ST_SET_CACHED_COUNTERPART(CE,T,CT,LE) do {           \
        int _i = (CE)->id - 1;                               \
        assert(_i >= 0 && _i < st_counterparts.T##_size);    \
        st_counterparts.CT[_i].counterpart = LE;             \
    } while(0)
#define ST_HAS_CACHED_COUNTERPART(CE,CT) (st_counterparts.CT[(CE)->id - 1].counterpart != NULL)

#define ST_TYPE_IS_STATIC_CACHED_COUNTERPART(CE,LE) (((CE) >= &st_cached_magic_vars.types[0] && (CE) < &st_cached_magic_vars.types[st_cached_magic_vars.types_num]) && st_counterparts.types[(CE)->id - 1].counterpart == (LE))
#define ST_TYPE_IS_DYNAMIC_CACHED_COUNTERPART(CE,LE) (MAGIC_TYPE_FLAG(CE,MAGIC_TYPE_DYNAMIC) && (CE)->type_id == MAGIC_TYPE_ARRAY && (LE)->type_id == MAGIC_TYPE_ARRAY && (CE)->num_child_types == (LE)->num_child_types && ST_TYPE_IS_STATIC_CACHED_COUNTERPART((CE)->contained_types[0], (LE)->contained_types[0]))
#define ST_TYPE_IS_CACHED_COUNTERPART(CE,LE) (ST_TYPE_IS_STATIC_CACHED_COUNTERPART(CE,LE) || ST_TYPE_IS_DYNAMIC_CACHED_COUNTERPART(CE,LE))
#define ST_PTR_TYPE_IS_STATIC_CACHED_COUNTERPART(CE,LE) (((CE) >= &st_cached_magic_vars.types[0] && (CE) < &st_cached_magic_vars.types[st_cached_magic_vars.types_num]) && st_counterparts.ptr_types[(CE)->id - 1].counterpart == (LE))
#define ST_PTR_TYPE_IS_DYNAMIC_CACHED_COUNTERPART(CE,LE) (MAGIC_TYPE_FLAG(CE,MAGIC_TYPE_DYNAMIC) && (CE)->type_id == MAGIC_TYPE_ARRAY && (LE)->type_id == MAGIC_TYPE_ARRAY && (CE)->num_child_types == (LE)->num_child_types && ST_PTR_TYPE_IS_STATIC_CACHED_COUNTERPART((CE)->contained_types[0], (LE)->contained_types[0]))
#define ST_PTR_TYPE_IS_CACHED_COUNTERPART(CE,LE) (ST_PTR_TYPE_IS_STATIC_CACHED_COUNTERPART(CE,LE) || ST_PTR_TYPE_IS_DYNAMIC_CACHED_COUNTERPART(CE,LE))


/* Buffer allocation */
PUBLIC void *st_cb_pages_allocate(st_init_info_t *info, uint32_t *phys, int num_pages);
PUBLIC void st_cb_pages_free(st_init_info_t *info, st_alloc_pages *current_page);
PUBLIC void *st_buff_allocate(st_init_info_t *info, size_t size);
PUBLIC void st_buff_cleanup(st_init_info_t *info);

#endif /* ST_STATE_TRANSFER_H */
