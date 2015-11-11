#include <magic.h>
#include <magic_mem.h>
#include <magic_analysis.h>
#include <magic_asr.h>
#include <stdarg.h>
#include <st/state_transfer.h>
#include <st/metadata_transfer.h>
#include <st/typedefs.h>
#include <st/private.h>

#define printf _magic_printf

#ifdef __MINIX
EXTERN endpoint_t sef_self_endpoint;
#else
#define DO_SKIP_ENVIRON_HACK 1
#define TODO_DSENTRY_PARENT_NAME_BUG 1
#define DO_SKIP_UNPAIRED_PTR_TARGETS 1
#endif

#define DO_SKIP_INVARIANTS_VIOLATIONS 0

PRIVATE st_alloc_pages *st_alloc_pages_current = NULL;
PRIVATE size_t st_alloc_buff_available = 0;
PRIVATE char *st_alloc_buff_pt = NULL;
PRIVATE char *st_pre_allocated_page_pt = NULL;
PRIVATE struct _magic_dsentry *st_dsentry_buff = NULL;
PRIVATE void *st_data_buff = NULL;
PRIVATE unsigned st_num_type_transformations = 0;

/* Magic variables and counterparts. */
struct _magic_vars_t st_remote_magic_vars, st_cached_magic_vars;
struct _magic_vars_t *st_local_magic_vars_ptr = &_magic_vars_buff;
st_counterparts_t st_counterparts;

/* Private variables. */
PRIVATE int st_init_done = FALSE;
PRIVATE int st_policies = ST_POLICIES_DEFAULT;
PRIVATE double st_unpaired_types_ratio = ST_UNPAIRED_TYPES_RATIO_DEFAULT;
PRIVATE double st_unpaired_struct_types_ratio = ST_UNPAIRED_STRUCT_TYPES_RATIO_DEFAULT;

/* Forward declarations. */
PRIVATE INLINE int default_transfer_selement_sel_cb(_magic_selement_t *selement, _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats, struct st_cb_info *cb_info);

/* State transfer callbacks. */

PRIVATE struct st_cbs_t st_cbs = {
    ST_CB_PAGES_ALLOCATE_DEFAULT,
    ST_CB_PAGES_FREE_DEFAULT,
    ST_CB_STATE_CLEANUP_DEFAULT,
    ST_CB_STATE_CHECKING_DEFAULT,
    ST_CB_SELEMENT_MAP_DEFAULT,
    ST_CB_SELEMENT_TRANSFER_EMPTY
};

/* OS dependent callbacks. */
PRIVATE struct st_cbs_os_t st_cbs_os = {
    ST_CB_OS_PANIC_EMPTY,
    ST_CB_OS_OLD_STATE_TABLE_LOOKUP_EMPTY,
    ST_CB_OS_COPY_STATE_REGION_EMPTY,
    ST_CB_OS_ALLOC_CONTIG_EMPTY,
    ST_CB_OS_FREE_CONTIG_EMPTY,
    ST_CB_OS_DEBUG_HEADER_EMPTY
};

/* State transfer prototypes for st_receive(). */
PUBLIC void do_st_before_receive(void);

/* Callback setters */

PUBLIC void st_setcb_pages_allocate (st_cb_pages_allocate_t cb)
{
  assert(cb != NULL);
  st_cbs.st_cb_pages_allocate = cb;
}

PUBLIC void st_setcb_pages_free (st_cb_pages_free_t cb)
{
  assert(cb != NULL);
  st_cbs.st_cb_pages_free = cb;
}

PUBLIC void st_setcb_state_cleanup (st_cb_state_cleanup_t cb)
{
  assert(cb != NULL);
  st_cbs.st_cb_state_cleanup = cb;
  magic_setcb_sentries_analyze_pre(cb);
}

PUBLIC void st_setcb_state_checking (st_cb_state_checking_t cb)
{
  assert(cb != NULL);
  st_cbs.st_cb_state_checking = cb;
}

PUBLIC void st_setcb_selement_map(st_cb_selement_map_t cb)
{
  assert(cb != NULL);
  st_cbs.st_cb_selement_map = cb;
}

PUBLIC void st_setcb_selement_transfer(st_cb_selement_transfer_t cb, int flags)
{
    int i, j;
    for (i = 0 ; i < NUM_CB_ARRAYS ; i++) {
        if (i & flags) {
            int is_registered = FALSE;
            for (j = 0; j < MAX_NUM_CBS ; j++) {
                if (st_cbs.st_cb_selement_transfer[i][j] == NULL) {
                    st_cbs.st_cb_selement_transfer[i][j] = cb;
                    is_registered = TRUE;
                    break;
                }
            }
            assert(is_registered && "Number of registered callbacks exceeds MAX_NUM_CBS");
        }
    }
}

/* OS Callback setters. */

PUBLIC void st_setcb_os_panic(st_cb_os_panic_t cb)
{
    assert(cb != NULL && "No callback defined for panic().");
    st_cbs_os.panic = cb;
}

PUBLIC void st_setcb_os_old_state_table_lookup(st_cb_os_old_state_table_lookup_t cb)
{
    assert(cb != NULL && "No callback defined for old_state_table_lookup().");
    st_cbs_os.old_state_table_lookup = cb;
}

PUBLIC void st_setcb_os_copy_state_region(st_cb_os_copy_state_region_t cb)
{
    assert(cb != NULL && "No callback defined for copy_state_region().");
    st_cbs_os.copy_state_region = cb;
}

PUBLIC void st_setcb_os_alloc_contig(st_cb_os_alloc_contig_t cb)
{
    assert(cb != NULL && "No callback defined for alloc_contig().");
    st_cbs_os.alloc_contig = cb;
}

PUBLIC void st_setcb_os_free_contig(st_cb_os_free_contig_t cb)
{
    assert(cb != NULL && "No callback defined for free_contig().");
    st_cbs_os.free_contig = cb;
}

PUBLIC void st_setcb_os_debug_header(st_cb_os_debug_header_t cb)
{
    assert(cb != NULL && "No callback defined for debug_header().");
    st_cbs_os.debug_header = cb;
}


PUBLIC void st_setcb_os_all(struct st_cbs_os_t *cbs)
{
    st_setcb_os_panic(cbs->panic);
    st_setcb_os_old_state_table_lookup(cbs->old_state_table_lookup);
    st_setcb_os_copy_state_region(cbs->copy_state_region);
    st_setcb_os_alloc_contig(cbs->alloc_contig);
    st_setcb_os_free_contig(cbs->free_contig);
    st_setcb_os_debug_header(cbs->debug_header);
}

/* Status variables to be transfered at state transfer time. */
PUBLIC int __st_before_receive_enabled = 0;
PRIVATE int __st_before_receive_sc_max_cycles;
PRIVATE int __st_before_receive_sc_max_violations;

/* Typedef registration and lookup */

int st_strcmp_wildcard(const char *with_wildcard, const char *without_wildcard)
{
    /* Note: this implementation only supports basic regexes with a '*'
     * at the beginning or the end of the string.
     */
    const char *star = strchr(with_wildcard, '*');
    if (star) {
        if (star == with_wildcard) {
            size_t len = strlen(with_wildcard+1);
            size_t len_without_wildcard = strlen(without_wildcard);
            const char *match_without_wildcard = without_wildcard+
                len_without_wildcard-len;
            if (match_without_wildcard < without_wildcard) {
                return -1;
            }
            return strncmp(with_wildcard+1, match_without_wildcard, len);
        }
        return strncmp(with_wildcard, without_wildcard, star - with_wildcard);
    }
    return strcmp(with_wildcard, without_wildcard);
}

const char *st_typename_noxfers[] =   { ST_TYPENAME_NO_TRANSFER_NAMES, NULL         };
const char *st_typename_ixfers[] =    { ST_TYPENAME_IDENTITY_TRANSFER_NAMES, NULL   };
const char *st_typename_cixfers[] =   { ST_TYPENAME_CIDENTITY_TRANSFER_NAMES, NULL  };
const char *st_typename_pxfers[] =    { ST_TYPENAME_PTR_TRANSFER_NAMES, NULL        };
const char *st_typename_sxfers[] =    { ST_TYPENAME_STRUCT_TRANSFER_NAMES, NULL     };
const char *st_sentryname_ixfers[] =  { ST_SENTRYNAME_IDENTITY_TRANSFER_NAMES, NULL };
const char *st_sentryname_cixfers[] = { ST_SENTRYNAME_CIDENTITY_TRANSFER_NAMES, NULL};
const char *st_sentryname_pxfers[] =  { ST_SENTRYNAME_PTR_TRANSFER_NAMES, NULL      };

/* Exclude stack references in addition to the default sentry names from state transfer. */
const char *st_sentryname_noxfers[] = {
    ST_SENTRYNAME_NO_TRANSFER_NAMES,
#define __X(R) #R   /* Stringify the symbol names. */
    ST_STACK_REFS_INT_LIST,
#if ST_STACK_REFS_CUSTOM_NUM > 0
    ST_STACK_REFS_CUSTOM_LIST,
#endif
#undef __X
    NULL };
const char *st_sentryname_noxfers_mem[] = { ST_SENTRYNAME_NO_TRANSFER_MEM_NAMES, NULL };

/* Exclude the data segments of certain libs from state transfer. */
const char *st_dsentry_lib_noxfer[] = {
#ifdef ST_DSENTRYLIB_NO_TRANSFER_NAMES
    ST_DSENTRYLIB_NO_TRANSFER_NAMES,
#endif
    NULL };

const char *st_typename_key_registrations[MAX_NUM_TYPENAMES];

static int is_typename(const char *search_key, struct _magic_type *type)
{
    unsigned int i;
    /* We can't use a cached lookup result */
    if (!st_strcmp_wildcard(search_key, type->name)) {
        /* The name matches */
        return TRUE;
    }
    for (i = 0 ; i < type->num_names ; i++) {
        if(!st_strcmp_wildcard(search_key, type->names[i])) {
            /* One of the typename names matches */
            return TRUE;
        }
    }
    /* No match is found */
    return FALSE;
}

PUBLIC void st_register_typename_key(const char *key)
{
    int i, is_registered = FALSE;
    for(i = 0 ; i < MAX_NUM_TYPENAMES ; i++) {
        if (st_typename_key_registrations[i] == NULL) {
            st_typename_key_registrations[i] = key;
            is_registered = TRUE;
            break;
        }
    }
    assert(is_registered && "Error, number of typename registrations > MAX_NUM_TYPENAMES.\n");
}

PUBLIC void st_register_typename_keys(const char **keys)
{
    int i = 0;
    while (keys[i] != NULL) {
        st_register_typename_key(keys[i]);
        i++;
    }
}

PRIVATE void set_typename_key(struct _magic_type *type)
{
    const char **registration = st_typename_key_registrations;

    while (*registration != NULL) {
        if (is_typename(*registration, type)) {
            type->ext = *registration;
            break;
        }
        registration++;
    }
}

PRIVATE void register_typenames(void)
{

    int i;

    /* Register typenames */
    st_register_typename_keys(st_typename_noxfers);
    st_register_typename_keys(st_typename_ixfers);
    st_register_typename_keys(st_typename_cixfers);
    st_register_typename_keys(st_typename_pxfers);
    st_register_typename_keys(st_typename_sxfers);

    for(i = 0 ; i < _magic_types_num ; i++) {
        set_typename_key(&_magic_types[i]);
    }

}

PRIVATE INLINE void register_typenames_and_callbacks(void)
{

    static int st_is_registered = FALSE;
    if(st_is_registered) {
        return;
    }

    register_typenames();

    st_setcb_selement_transfer(st_cb_transfer_sentry_default, ST_CB_TYPE_SENTRY);
    st_setcb_selement_transfer(st_cb_transfer_typename_default, ST_CB_TYPE_TYPENAME);

    st_is_registered = TRUE;

}

PRIVATE int st_type_name_match_any(const char **registered_type_name_keys,
    const char *key)
{
    int i = 0;
    while (registered_type_name_keys[i] != NULL) {
        if (ST_TYPE_NAME_MATCH(registered_type_name_keys[i], key)) {
            return TRUE;
        }
        i++;
    }
    return FALSE;
}

PRIVATE int st_sentry_name_match_any(const char **sentry_wildcard_names,
    const char *name)
{
    int i = 0;
    while (sentry_wildcard_names[i] != NULL) {
        if (ST_SENTRY_NAME_MATCH(sentry_wildcard_names[i], name)) {
            return TRUE;
        }
        i++;
    }
    return FALSE;
}

PRIVATE int st_dsentry_parent_name_match_any(const char **wildcard_names,
    const char *name)
{
    int i = 0;
    while (wildcard_names[i] != NULL) {
        if (ST_DSENTRY_PARENT_NAME_MATCH(wildcard_names[i], name)) {
            return TRUE;
        }
        i++;
    }
    return FALSE;
}

/* Utilities. */
PUBLIC void st_cb_print(int level, const char *msg, _magic_selement_t *selement, _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats, struct st_cb_info *cb_info)
{
    if (ST_CB_PRINT_LEVEL(level)) {
        _magic_printf("[%s] %s. Current state element:\n",
            ST_CB_LEVEL_TO_STR(level), msg);
        MAGIC_SELEMENT_PRINT(selement, MAGIC_EXPAND_TYPE_STR);
        _magic_printf("\n");
        MAGIC_SEL_ANALYZED_PRINT(sel_analyzed, MAGIC_EXPAND_TYPE_STR);
        _magic_printf("\n");
        MAGIC_SEL_STATS_PRINT(sel_stats);
        _magic_printf("\n\n");
    }
}

PUBLIC void st_cb_selement_type_cast(const struct _magic_type* new_selement_type, const struct _magic_type* new_local_selement_type, _magic_selement_t *selement, _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats, struct st_cb_info *cb_info)
{
    magic_selement_type_cast(selement, ST_SEL_ANALYZE_FLAGS,
            new_selement_type, sel_analyzed, sel_stats);
    if (!ST_CB_FLAG(ST_CB_CHECK_ONLY)) {
        cb_info->local_selement->type = new_local_selement_type;
    }
}

/* Stack management. */

PUBLIC void st_stack_refs_save_restore(char* stack_buff, int is_save)
{
    struct _magic_dsentry *prev_dsentry, *dsentry;
    struct _magic_sentry* sentry;
    struct st_stack_refs_buff *buff_ptr;
    int i;

#define __X(P) P
    extern int ST_STACK_REFS_INT_LIST;
#undef __X
#define __X(P) ((int *)&(P))
    int* int_ptrs[] = { ST_STACK_REFS_INT_LIST, ST_STACK_REFS_CUSTOM_LIST };
#undef __X

    assert((ST_STACK_REFS_NUM) == sizeof(int_ptrs)/sizeof(int_ptrs[0]));
    assert(sizeof(int) == sizeof(void*));
    buff_ptr = (struct st_stack_refs_buff*) stack_buff;

    /* Save. */
    if (is_save) {
        buff_ptr->first_stack_dsentry = _magic_first_stack_dsentry;
        buff_ptr->last_stack_dsentry = _magic_last_stack_dsentry;
        if (_magic_first_stack_dsentry) {
            buff_ptr->first_stack_obdsentry_buff = *MAGIC_OBDSENTRY_FROM_DSENTRY(_magic_first_stack_dsentry);
        }
        memcpy(buff_ptr->stack_range, magic_stack_range, 2*sizeof(void*));
        for (i = 0 ; i < ST_STACK_REFS_NUM ; i++) {
            memcpy(&buff_ptr->stack_int_refs[i], int_ptrs[i], sizeof(int));
        }
        return;
    }

    /* Restore. */
    if (_magic_first_dsentry == _magic_last_stack_dsentry) {
        _magic_first_dsentry = buff_ptr->last_stack_dsentry;
    }
    else {
        MAGIC_DSENTRY_ITER(_magic_first_dsentry, prev_dsentry, dsentry, sentry,
            if (MAGIC_DSENTRY_HAS_NEXT(dsentry)
                && MAGIC_DSENTRY_NEXT(dsentry) == _magic_last_stack_dsentry) {
                MAGIC_DSENTRY_NEXT(dsentry) = buff_ptr->last_stack_dsentry;
                break;
            }
        );
    }

    _magic_first_stack_dsentry = buff_ptr->first_stack_dsentry;
    _magic_last_stack_dsentry = buff_ptr->last_stack_dsentry;
    if (_magic_first_stack_dsentry) {
        *MAGIC_OBDSENTRY_FROM_DSENTRY(_magic_first_stack_dsentry) = buff_ptr->first_stack_obdsentry_buff;
    }
    memcpy(magic_stack_range, buff_ptr->stack_range, 2*sizeof(void*));
    for (i = 0 ; i < ST_STACK_REFS_NUM ; i++) {
        memcpy(int_ptrs[i], &buff_ptr->stack_int_refs[i], sizeof(int));
    }
}

/* Metadata management. */
PUBLIC int st_add_special_mmapped_region(void *address, size_t size,
    const char* name)
{
    struct _magic_obdsentry* obdsentry;
    char addr_name[24];

    if (!_magic_enabled) return OK;

    if (!name) {
        snprintf(addr_name, sizeof(addr_name), "%%MMAP_0x%08x",
          (unsigned int) address);
        name = addr_name;
    }
    obdsentry = magic_create_obdsentry(address, MAGIC_VOID_TYPE,
        size, MAGIC_STATE_MAP, name, NULL);
    return obdsentry ? OK : EINVAL;
}

PUBLIC int st_del_special_mmapped_region_by_addr(void *address)
{
    int ret;

    if (!_magic_enabled) return OK;

    ret = magic_destroy_obdsentry_by_addr(address);
    if (ret < 0) {
        return EINVAL;
    }
    return OK;
}

/* Selement transfer callbacks. */

PRIVATE INLINE int transfer_walkable_sel_cb(_magic_selement_t *selement, _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats, struct st_cb_info *cb_info)
{
    /* Do nothing for complex type. process only its members, not the complex type itself */
    return MAGIC_SENTRY_ANALYZE_CONTINUE;
}

PRIVATE INLINE int transfer_try_raw_copy_sel_cb(_magic_selement_t *selement, struct st_cb_info *cb_info)
{
    /* Only do raw copying if there are no type transformations. */
    if ((selement->type->num_child_types == 0 && selement->type->size == cb_info->local_selement->type->size) || selement->type == cb_info->local_selement->type || ST_TYPE_IS_CACHED_COUNTERPART(selement->type, cb_info->local_selement->type)) {
        memcpy(cb_info->local_selement->address, selement->address, cb_info->local_selement->type->size);
        return MAGIC_SENTRY_ANALYZE_SKIP_PATH;
    }

    return MAGIC_SENTRY_ANALYZE_CONTINUE;
}

PRIVATE INLINE int transfer_identity_sel_cb(_magic_selement_t *selement, _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats, struct st_cb_info *cb_info)
{
    if (!ST_CB_FLAG(ST_CB_CHECK_ONLY)) {
        /* First try to do raw copying, assuming there are no type transformations. */
        if (transfer_try_raw_copy_sel_cb(selement, cb_info) == MAGIC_SENTRY_ANALYZE_SKIP_PATH)
            return MAGIC_SENTRY_ANALYZE_SKIP_PATH;

#if CHECK_ASR && !FORCE_SOME_UNPAIRED_TYPES
        if (cb_info->init_info->flags & ST_LU_ASR) {
            st_cbs_os.panic("ASR should never get here!");
        }
#endif
        if (selement->type->type_id == MAGIC_TYPE_UNION) {
            ST_CB_PRINT(ST_CB_ERR, "uncaught ixfer union with type changes", selement, sel_analyzed, sel_stats, cb_info);
            return EFAULT;
        }
        cb_info->st_cb_flags |= ST_CB_FORCE_IXFER;
        return MAGIC_SENTRY_ANALYZE_CONTINUE;
    }
    return MAGIC_SENTRY_ANALYZE_SKIP_PATH;
}

PRIVATE INLINE int transfer_cond_identity_sel_cb(_magic_selement_t *selement, _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats, struct st_cb_info *cb_info)
{
    int r;
    int saved_flags = cb_info->st_cb_flags;
    cb_info->st_cb_flags &= ~ST_CB_PRINT_ERR;
    r = default_transfer_selement_sel_cb(selement, sel_analyzed, sel_stats, cb_info);
    cb_info->st_cb_flags = saved_flags;
    if (r < 0) {
        ST_CB_PRINT(ST_CB_DBG, "conditional ixfer resorting to ixfer", selement, sel_analyzed, sel_stats, cb_info);
        return transfer_identity_sel_cb(selement, sel_analyzed, sel_stats, cb_info);
    }
    return r;
}

PRIVATE INLINE int transfer_nonptr_sel_cb(_magic_selement_t *selement, _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats, struct st_cb_info *cb_info)
{
    if (sel_analyzed->flags & MAGIC_SEL_FOUND_VIOLATIONS) {
        ST_CB_PRINT(ST_CB_ERR, "uncaught non-ptr with violations", selement, sel_analyzed, sel_stats, cb_info);
        return EFAULT;
    }
    return transfer_identity_sel_cb(selement, sel_analyzed, sel_stats, cb_info);
}

PRIVATE int transfer_ptr_sel_with_trg_cb(_magic_selement_t *selement, _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats, struct st_cb_info *cb_info)
{
    int trg_flags, trg_extf_flags, trg_transferred, trg_paired;
    _magic_selement_t cached_trg_selement, local_trg_selement;
    void **local_selement_address = cb_info->local_selement->address;
    int r;

    r = lookup_trg_info(selement, sel_analyzed, sel_stats, cb_info, &cached_trg_selement, &local_trg_selement);
    if (r != OK) {
        return r;
    }

    trg_flags = sel_analyzed->u.ptr.trg_flags;
    trg_extf_flags = MAGIC_STATE_FLAGS_TO_EXTF(trg_flags);
    trg_transferred = (trg_extf_flags & (ST_NEEDS_TRANSFER | ST_TRANSFER_DONE));
    trg_paired = (local_trg_selement.type != NULL);

    if (!trg_transferred && trg_paired && (trg_extf_flags & ST_ON_PTRXFER_CASCADE)) {
        /* Propagate transfer on the target. */
        if (cached_trg_selement.sentry && !(trg_extf_flags & ST_NEEDS_TRANSFER)) {
            ST_CB_PRINT(ST_CB_DBG, "ptr lookup results in cascade transfer for the target", selement, sel_analyzed, sel_stats, cb_info);
            st_set_status_by_sentry_id(ST_NEEDS_TRANSFER, ST_OP_ADD, MAGIC_SENTRY_ID(cached_trg_selement.sentry));
        }
        /* Force code below to transfer the pointer normally. */
        trg_transferred = TRUE;
    }

    if (trg_transferred && trg_paired) {
        *local_selement_address = local_trg_selement.address;
    }
    else if (trg_extf_flags & ST_ON_PTRXFER_SET_NULL) {
        ST_CB_PRINT(ST_CB_DBG, "ptr lookup results in forcefully setting ptr to NULL", selement, sel_analyzed, sel_stats, cb_info);
        *local_selement_address = NULL;
    }
    else if(trg_extf_flags & ST_ON_PTRXFER_SET_DEFAULT) {
        ST_CB_PRINT(ST_CB_DBG, "ptr lookup results in forcefully setting ptr to default value", selement, sel_analyzed, sel_stats, cb_info);
        if (trg_flags & MAGIC_STATE_STRING) {
            *((char**)local_selement_address) = __UNCONST("");
        }
        else {
            *local_selement_address = NULL;
        }
    }
    else if (trg_extf_flags & ST_ON_PTRXFER_SKIP) {
        ST_CB_PRINT(ST_CB_DBG, "ptr lookup results in skipping ptr transfer", selement, sel_analyzed, sel_stats, cb_info);
    }
    else {
        if (trg_paired) {
            ST_CB_PRINT(ST_CB_ERR, "uncaught ptr lookup for non-transferred target", selement, sel_analyzed, sel_stats, cb_info);
        }
        else {
            ST_CB_PRINT(ST_CB_ERR, "uncaught ptr lookup for unpaired target", selement, sel_analyzed, sel_stats, cb_info);
        }
#if DO_SKIP_UNPAIRED_PTR_TARGETS
        return MAGIC_SENTRY_ANALYZE_SKIP_PATH;
#else
        return ENOENT;
#endif
    }

    return MAGIC_SENTRY_ANALYZE_SKIP_PATH;
}

PRIVATE INLINE int transfer_ptr_sel_cb(_magic_selement_t *selement, _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats, struct st_cb_info *cb_info)
{
    const struct _magic_type *first_trg_type;
    if (selement->type->type_id != MAGIC_TYPE_POINTER) {
        if (selement->type->size != sizeof(void*)) {
            ST_CB_PRINT(ST_CB_ERR, "wrong pointer size", selement, sel_analyzed, sel_stats, cb_info);
            return EFAULT;
        }
        ST_CB_PRINT(ST_CB_DBG, "casting non-ptr to ptr element", selement, sel_analyzed, sel_stats, cb_info);
        st_cb_selement_type_cast(MAGIC_VOID_PTR_INT_CAST_TYPE, MAGIC_VOID_PTR_INT_CAST_TYPE, selement, sel_analyzed, sel_stats, cb_info);
    }
    first_trg_type = MAGIC_SEL_ANALYZED_PTR_FIRST_TRG_TYPE(sel_analyzed);
    if (first_trg_type == MAGIC_TYPE_NULL_ENTRY
        || first_trg_type == MAGIC_TYPE_VALUE_FOUND) {

        /* NULL pointer or special value. Don't adjust value */
        return transfer_identity_sel_cb(selement, sel_analyzed, sel_stats, cb_info);

    } else if (!(sel_analyzed->flags & MAGIC_SEL_FOUND_VIOLATIONS)) {
        /* Valid pointer found */
        if (!ST_CB_FLAG(ST_CB_CHECK_ONLY)) {
            return transfer_ptr_sel_with_trg_cb(selement, sel_analyzed, sel_stats, cb_info);
        }

        return MAGIC_SENTRY_ANALYZE_SKIP_PATH;

    } else if(MAGIC_STATE_FLAG(selement->sentry, MAGIC_STATE_STACK)) {
        struct _magic_sentry *trg_sentry = magic_sentry_lookup_by_range(sel_analyzed->u.ptr.value, NULL);
        if (trg_sentry && !strcmp(trg_sentry->name, MAGIC_ALLOC_INITIAL_STACK_NAME)) {
            /* Stack pointer to initial stack area. This is common (e.g., argv).
             * We can safely assume the pointer will be already correctly
             * initialized in the new version and simply skip transfer.
             */
            ST_CB_PRINT(ST_CB_DBG, "skipping stack ptr element pointing to initial stack area", selement, sel_analyzed, sel_stats, cb_info);
            return MAGIC_SENTRY_ANALYZE_SKIP_PATH;
        }
    }
#ifdef __MINIX
#define IS_KERNEL_PTR(p) (((intptr_t)(p) & 0xf0000000) == 0xf0000000) /* TODO: make this more dynamic */
    else if (IS_KERNEL_PTR(sel_analyzed->u.ptr.value))
        return MAGIC_SENTRY_ANALYZE_SKIP_PATH; /* Kernel-mapped pointer */
#endif

    /* Pointer with violations found */
    ST_CB_PRINT(ST_CB_ERR, "uncaught ptr with violations", selement, sel_analyzed, sel_stats, cb_info);
#if DO_SKIP_INVARIANTS_VIOLATIONS
    return MAGIC_SENTRY_ANALYZE_SKIP_PATH;
#else
    return EFAULT;
#endif
}

PRIVATE INLINE int transfer_struct_sel_cb(_magic_selement_t *selement, _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats, struct st_cb_info *cb_info)
{
    static int st_counter = 0;
    unsigned parent_offset, offset;
    int walk_flags, ret;

    if (selement->type->type_id != MAGIC_TYPE_UNION && selement->type->type_id != MAGIC_TYPE_STRUCT) {
        ST_CB_PRINT(ST_CB_ERR, "struct transfer is only for structs and unions!", selement, sel_analyzed, sel_stats, cb_info);
        return EFAULT;
    }
    if (selement->type->type_id == MAGIC_TYPE_STRUCT || st_counter > 0) {
        return MAGIC_SENTRY_ANALYZE_CONTINUE;
    }

    /* Walk the union as a struct. */
    walk_flags = cb_info->walk_flags;
    cb_info->walk_flags = (MAGIC_TYPE_WALK_DEFAULT_FLAGS & (~MAGIC_TYPE_WALK_UNIONS_AS_VOID));
    st_counter++;
    parent_offset = (unsigned)selement->parent_address - (unsigned)selement->sentry->address;
    offset = (unsigned)selement->address - (unsigned)selement->sentry->address;
    ret =  magic_type_walk_flags(selement->parent_type, parent_offset,
        selement->child_num, selement->type, offset,
        0, ULONG_MAX, magic_type_analyzer_cb, selement->cb_args, cb_info->walk_flags);
    st_counter--;
    cb_info->walk_flags = walk_flags;
    if (ret != 0) {
        return ret == MAGIC_TYPE_WALK_STOP ? MAGIC_SENTRY_ANALYZE_STOP : ret;
    }

    return MAGIC_SENTRY_ANALYZE_SKIP_PATH;
}

PRIVATE INLINE int default_transfer_selement_sel_cb(_magic_selement_t *selement, _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats, struct st_cb_info *cb_info)
{
    /* Default handler for walkable, ptr and nonptr types. */
#if ST_TRANSFER_IDENTITY_FOR_NO_INNER_PTRS
    if (MAGIC_TYPE_FLAG(selement->type, MAGIC_TYPE_NO_INNER_PTRS)) {
        /* If the type has no inner pointers, try to do raw copying. */
        if (transfer_try_raw_copy_sel_cb(selement, cb_info) == MAGIC_SENTRY_ANALYZE_SKIP_PATH)
            return MAGIC_SENTRY_ANALYZE_SKIP_PATH;
    }
#endif
    if (selement->type->type_id == MAGIC_TYPE_UNION) {
        if (!(st_policies & ST_IXFER_UNCAUGHT_UNIONS) && !MAGIC_TYPE_FLAG(selement->type, MAGIC_TYPE_NO_INNER_PTRS)) {
            ST_CB_PRINT(ST_CB_ERR, "uncaught union", selement, sel_analyzed, sel_stats, cb_info);
            return EFAULT;
        }
        else {
            int level = (st_policies & ST_REPORT_UNCAUGHT_UNIONS) ? ST_CB_ERR : ST_CB_DBG;
            ST_CB_PRINT(level, "uncaught union", selement, sel_analyzed, sel_stats, cb_info);
            return transfer_identity_sel_cb(selement, sel_analyzed, sel_stats, cb_info);
        }
    } else if (MAGIC_TYPE_IS_WALKABLE(selement->type)) {
        return transfer_walkable_sel_cb(selement, sel_analyzed, sel_stats, cb_info);
    } else if (selement->type->type_id == MAGIC_TYPE_POINTER) {
        return transfer_ptr_sel_cb(selement, sel_analyzed, sel_stats, cb_info);
    } else {
        return transfer_nonptr_sel_cb(selement, sel_analyzed, sel_stats, cb_info);
    }

    /* Not reachable. */
    ST_CB_PRINT(ST_CB_ERR, "Bug!", selement, sel_analyzed, sel_stats, cb_info);
    return EINTR;
}

PUBLIC int st_cb_transfer_sentry_default(_magic_selement_t *selement, _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats, struct st_cb_info *cb_info)
{
    const char *sentry_name = selement->sentry->name;

#if ST_ASSUME_RAW_COPY_BEFORE_TRANSFER
    if (MAGIC_STATE_FLAGS(selement->sentry, MAGIC_STATE_DYNAMIC)) {
        if (MAGIC_STATE_FLAG(selement->sentry, MAGIC_STATE_LIB) || MAGIC_SENTRY_IS_EXT_ALLOC(selement->sentry))
            return MAGIC_SENTRY_ANALYZE_SKIP_PATH;
    }
#endif

    if (ST_SENTRY_NAME_MATCH_ANY(st_sentryname_ixfers, sentry_name)) {
        ST_CB_PRINT(ST_CB_DBG, "sentry name matches ixfer", selement, sel_analyzed, sel_stats, cb_info);
        return transfer_identity_sel_cb(selement, sel_analyzed, sel_stats, cb_info);
    }

    if (ST_SENTRY_NAME_MATCH_ANY(st_sentryname_cixfers, sentry_name)) {
        ST_CB_PRINT(ST_CB_DBG, "sentry name matches cixfer", selement, sel_analyzed, sel_stats, cb_info);
        return transfer_cond_identity_sel_cb(selement, sel_analyzed, sel_stats, cb_info);
    }

    if (ST_SENTRY_NAME_MATCH_ANY(st_sentryname_noxfers, sentry_name)) {
        ST_CB_PRINT(ST_CB_DBG, "sentry name matches noxfer", selement, sel_analyzed, sel_stats, cb_info);
        return MAGIC_SENTRY_ANALYZE_SKIP_PATH;
    }

    /* Skip memory management related sentries only when memory functions have
     * been instrumented (which is *not* the case for the MINIX3 VM service).
     */
    if (_magic_no_mem_inst == 0 && ST_SENTRY_NAME_MATCH_ANY(st_sentryname_noxfers_mem, sentry_name)) {
        ST_CB_PRINT(ST_CB_DBG, "sentry name matches noxfer", selement, sel_analyzed, sel_stats, cb_info);
        return MAGIC_SENTRY_ANALYZE_SKIP_PATH;
    }

    if (ST_SENTRY_NAME_MATCH_ANY(st_sentryname_pxfers, sentry_name)) {
        ST_CB_PRINT(ST_CB_DBG, "sentry name matches pxfer", selement, sel_analyzed, sel_stats, cb_info);
        return transfer_ptr_sel_cb(selement, sel_analyzed, sel_stats, cb_info);
    }

    if (MAGIC_STATE_FLAGS(selement->sentry, MAGIC_STATE_DYNAMIC | MAGIC_STATE_MAP | MAGIC_STATE_LIB)) {
        struct _magic_dsentry *dsentry = MAGIC_DSENTRY_FROM_SENTRY(selement->sentry);
        if (ST_DSENTRY_PARENT_NAME_MATCH_ANY(st_dsentry_lib_noxfer, dsentry->parent_name)) {
            ST_CB_PRINT(ST_CB_DBG, "dsentry is a lib map and parent_name matches dsentry_lib_noxfer", selement, sel_analyzed, sel_stats, cb_info);
            return MAGIC_SENTRY_ANALYZE_SKIP_PATH;
        }
    }

    return ST_CB_NOT_PROCESSED;
}

PUBLIC int st_cb_transfer_typename_default(_magic_selement_t *selement, _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats, struct st_cb_info *cb_info)
{
    const char *typename_key = ST_TYPE_NAME_KEY(selement->type);
    if (ST_TYPE_NAME_MATCH_ANY(st_typename_ixfers, typename_key)) {
        return transfer_identity_sel_cb(selement, sel_analyzed, sel_stats, cb_info);
    }

    if (ST_TYPE_NAME_MATCH_ANY(st_typename_cixfers, typename_key)) {
        return transfer_cond_identity_sel_cb(selement, sel_analyzed, sel_stats, cb_info);
    }

    if (ST_TYPE_NAME_MATCH_ANY(st_typename_noxfers, typename_key)) {
        return MAGIC_SENTRY_ANALYZE_SKIP_PATH;
    }

    if (ST_TYPE_NAME_MATCH_ANY(st_typename_pxfers, typename_key)) {
        return transfer_ptr_sel_cb(selement, sel_analyzed, sel_stats, cb_info);
    }

    if (ST_TYPE_NAME_MATCH_ANY(st_typename_sxfers, typename_key)) {
        return transfer_struct_sel_cb(selement, sel_analyzed, sel_stats, cb_info);
    }

    return ST_CB_NOT_PROCESSED;
}

PUBLIC int st_cb_transfer_walkable(_magic_selement_t *selement, _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats, struct st_cb_info *cb_info)
{
    return transfer_walkable_sel_cb(selement, sel_analyzed, sel_stats, cb_info);
}

PUBLIC int st_cb_transfer_ptr(_magic_selement_t *selement, _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats, struct st_cb_info *cb_info)
{
    return transfer_ptr_sel_cb(selement, sel_analyzed, sel_stats, cb_info);
}

PUBLIC int st_cb_transfer_identity(_magic_selement_t *selement, _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats, struct st_cb_info *cb_info)
{
    return transfer_identity_sel_cb(selement, sel_analyzed, sel_stats, cb_info);
}

PUBLIC int st_cb_transfer_cond_identity(_magic_selement_t *selement, _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats, struct st_cb_info *cb_info)
{
    return transfer_cond_identity_sel_cb(selement, sel_analyzed, sel_stats, cb_info);
}

PUBLIC int st_cb_transfer_nonptr(_magic_selement_t *selement, _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats, struct st_cb_info *cb_info)
{
    return transfer_nonptr_sel_cb(selement, sel_analyzed, sel_stats, cb_info);
}

PUBLIC int st_cb_transfer_struct(_magic_selement_t *selement, _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats, struct st_cb_info *cb_info)
{
    return transfer_struct_sel_cb(selement, sel_analyzed, sel_stats, cb_info);
}

PUBLIC int st_cb_transfer_selement_base(_magic_selement_t *selement, _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats, struct st_cb_info *cb_info)
{
    return default_transfer_selement_sel_cb(selement, sel_analyzed, sel_stats, cb_info);
}

PUBLIC int st_cb_transfer_selement_generic(_magic_selement_t *selement, _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats, struct st_cb_info *cb_info)
{
    return transfer_data_selement(selement, sel_analyzed, sel_stats, cb_info);
}

/* Selement mapping functions and callbacks. */

PRIVATE int st_map_selement_from_sentry_cb(const struct _magic_type* parent_type,
    const unsigned parent_offset, int child_num,
    const struct _magic_type* type, const unsigned offset, int depth, void* cb_args)
{
    void **args_array = (void**) cb_args;
    _magic_selement_t cached_selement;
    _magic_selement_t *local_selement = (_magic_selement_t*) args_array[1];
    _magic_selement_t *selement = (_magic_selement_t*) args_array[0];
    struct _magic_sentry *sentry = selement->sentry;
    void *address = (char*)sentry->address + offset;
    void *selement_address = selement->address;
    int is_trg_mapping;
    struct st_cb_info *cb_info;
    if ((char*) selement_address >= ((char*) address + type->size)) {
        return MAGIC_TYPE_WALK_SKIP_PATH;
    }
    cb_info = (struct st_cb_info*) args_array[2];
    is_trg_mapping = (int) args_array[3];
    cached_selement.sentry = sentry;
    cached_selement.parent_type = parent_type;
    cached_selement.parent_address = (char*)sentry->address + parent_offset;
    cached_selement.child_num = child_num;
    cached_selement.type = type;
    cached_selement.address = address;
    cached_selement.depth = depth;
    st_map_selement(&cached_selement, local_selement, cb_info, is_trg_mapping);
    if (local_selement->type == NULL) {
        return ENOENT;
    }
    if (address == selement_address && type == selement->type) {
        return MAGIC_TYPE_WALK_STOP;
    }
    if (type->num_child_types == 0) {
        return EINVAL;
    }
    local_selement->parent_type = local_selement->type;
    local_selement->parent_address = local_selement->address;
    return MAGIC_TYPE_WALK_CONTINUE;
}

PRIVATE INLINE void st_map_selement_from_sentry(_magic_selement_t *cached_selement, _magic_selement_t *local_selement, struct _magic_sentry *local_sentry, struct st_cb_info *cb_info, int is_trg_mapping)
{
    unsigned max_offset;
    int r;
    void *args_array[4];
    max_offset = (unsigned) ( (char *)cached_selement->address - (char *)cached_selement->sentry->address);
    args_array[0] = cached_selement;
    args_array[1] = magic_selement_from_sentry(local_sentry, local_selement);
    args_array[2] = cb_info;
    args_array[3] = (void*) is_trg_mapping;
    r = magic_type_walk_root(cached_selement->sentry->type, 0, max_offset, st_map_selement_from_sentry_cb, (void*) args_array);
    if (r < 0) {
        assert(r == ENOENT);
        local_selement->type = NULL;
    }
}

PRIVATE INLINE void st_map_sel_analyzed_from_target(_magic_sel_analyzed_t *cached_sel_analyzed, _magic_sel_analyzed_t *local_sel_analyzed, struct _magic_sentry *local_trg_sentry, struct _magic_function *local_trg_function, struct st_cb_info *cb_info)
{
    _magic_selement_t *csel, *lsel;
    assert(local_trg_sentry || local_trg_function);
    assert(cached_sel_analyzed->type_id == MAGIC_TYPE_POINTER);
    assert(cached_sel_analyzed->u.ptr.first_legal_trg_type>=0);
    local_sel_analyzed->type_id = cached_sel_analyzed->type_id;
    local_sel_analyzed->num = cached_sel_analyzed->num;
    local_sel_analyzed->flags = cached_sel_analyzed->flags;
    local_sel_analyzed->u.ptr.trg_flags = local_trg_sentry ? local_trg_sentry->flags : local_trg_function->flags;
    local_sel_analyzed->u.ptr.first_legal_trg_type = -1;
    local_sel_analyzed->u.ptr.num_legal_trg_types = 0;
    if (local_trg_function) {
        assert(cached_sel_analyzed->u.ptr.num_legal_trg_types == 1);
        lsel = &local_sel_analyzed->u.ptr.trg_selements[0];
        memset(lsel, 0, sizeof(_magic_selement_t));
        lsel->sentry = NULL;
        lsel->type = local_trg_function->type;
        lsel->address = local_trg_function->address;
        local_sel_analyzed->u.ptr.num_trg_types = 1;
    }
    else {
        unsigned int i;
        void *address = NULL;
        local_sel_analyzed->u.ptr.num_trg_types = 0;
        for (i = cached_sel_analyzed->u.ptr.first_legal_trg_type ; i < cached_sel_analyzed->u.ptr.num_trg_types ; i++) {
            _magic_trg_stats_t trg_stats = cached_sel_analyzed->u.ptr.trg_stats[i];
            if (MAGIC_SEL_ANALYZED_TRG_STATS_HAS_VIOLATIONS(trg_stats)) {
                continue;
            }
            csel = &cached_sel_analyzed->u.ptr.trg_selements[i];
            lsel = &local_sel_analyzed->u.ptr.trg_selements[local_sel_analyzed->u.ptr.num_trg_types++];
            st_map_selement_from_sentry(csel, lsel, local_trg_sentry, cb_info, TRUE);
            if (lsel->type == NULL || (address && lsel->address != address)) {
                /* Unpaired selement or ambiguous local address. */
                local_sel_analyzed->u.ptr.num_trg_types = 0;
                return;
            }
            address = lsel->address;
        }
        assert(local_sel_analyzed->u.ptr.num_trg_types > 0);
    }
}

PUBLIC void st_map_local_selement_from_child_num(_magic_selement_t *local_selement, struct st_cb_info *cb_info, int child_num)
{
    local_selement->child_num = child_num;
    magic_selement_fill_from_parent_info(local_selement, cb_info->walk_flags);
}

PUBLIC void st_cb_map_from_parent_array_selement_generic(_magic_selement_t *cached_selement, _magic_selement_t *local_selement, struct st_cb_info *cb_info, int is_trg_mapping)
{
    int cached_num_elements, local_num_elements, is_trg_at_array_end, is_trg_at_str_end;
    /* Match arrays/vectors with arrays/vectors. */
    assert(cached_selement->parent_type->type_id == MAGIC_TYPE_ARRAY || cached_selement->parent_type->type_id == MAGIC_TYPE_VECTOR);
    if (local_selement->parent_type->type_id != MAGIC_TYPE_ARRAY && local_selement->parent_type->type_id != MAGIC_TYPE_VECTOR) {
        local_selement->type = NULL;
        return;
    }
    cached_num_elements = cached_selement->parent_type->num_child_types;
    local_num_elements = local_selement->parent_type->num_child_types;
    /* Same size or first child? We are done. */
    if (cached_num_elements == local_num_elements || local_selement->child_num == 0) {
         st_map_local_selement_from_child_num(local_selement, cb_info, cached_selement->child_num);
         return;
    }
    assert(local_num_elements > 0);
    is_trg_at_str_end = FALSE;
    is_trg_at_array_end = FALSE;
    if (is_trg_mapping && cached_selement->child_num == cached_num_elements-1) {
        is_trg_at_str_end = MAGIC_SENTRY_IS_STRING(cached_selement->sentry) || MAGIC_SENTRY_IS_STRING(local_selement->sentry);
        is_trg_at_array_end = !is_trg_at_str_end;
    }
    if (is_trg_at_array_end && (st_policies & ST_DEFAULT_MAP_GUARD_PTRS_TO_ARRAY_END)) {
        /* This should be interpreted as a target of a guard pointer pointing to the last element of an array and needs to be remapped as such. */
        st_map_local_selement_from_child_num(local_selement, cb_info, local_num_elements-1);
    }
    else if (is_trg_at_str_end && (st_policies & ST_DEFAULT_MAP_GUARD_PTRS_TO_STR_END)) {
        /* This should be interpreted as a target of a guard pointer pointing to the last element of a string and needs to be remapped as such. */
        st_map_local_selement_from_child_num(local_selement, cb_info, local_num_elements-1);
    }
    else if (cached_selement->child_num >= local_num_elements) {
        /* New array got truncated and this element is gone. */
        local_selement->type = NULL;
    }
    else {
        /* New array is bigger, just keep the original position in the array. */
        st_map_local_selement_from_child_num(local_selement, cb_info, cached_selement->child_num);
    }
}

PUBLIC void st_cb_map_child_array_selement_generic(_magic_selement_t *cached_selement, _magic_selement_t *local_selement, struct st_cb_info *cb_info, int is_trg_mapping)
{
    size_t cached_size = cached_selement->type->num_child_types, local_size = local_selement->type->num_child_types;

    /* Match arrays/vectors with arrays/vectors. */
    assert(cached_selement->type->type_id == MAGIC_TYPE_ARRAY || cached_selement->type->type_id == MAGIC_TYPE_VECTOR);
    if (local_selement->type->type_id != MAGIC_TYPE_ARRAY && local_selement->type->type_id != MAGIC_TYPE_VECTOR) {
        local_selement->type = NULL;
        return;
    }

    /* Varsized arrays have to be consistent across versions. */
    if (MAGIC_TYPE_FLAG(cached_selement->type, MAGIC_TYPE_VARSIZE) != MAGIC_TYPE_FLAG(local_selement->type, MAGIC_TYPE_VARSIZE)) {
        local_selement->type = NULL;
        return;
    }

    /* Check size. */
    if (cached_size != local_size) {
        int report;
        int is_string = MAGIC_SENTRY_IS_STRING(cached_selement->sentry) || MAGIC_SENTRY_IS_STRING(local_selement->sentry);
        if (local_size < cached_size) {
            report = is_string ? (st_policies & ST_REPORT_SMALLER_STRINGS) : (st_policies & ST_REPORT_SMALLER_ARRAYS);
        }
        else {
            report = is_string ? (st_policies & ST_REPORT_LARGER_STRINGS) : (st_policies & ST_REPORT_LARGER_ARRAYS);
        }

        if (report) {
            printf("st_cb_map_child_array_selement_generic: %s size found while mapping array selements:\n", local_size < cached_size ? "Smaller" : "Larger");
            MAGIC_SELEMENT_PRINT(cached_selement, MAGIC_EXPAND_TYPE_STR); printf("\n");
            MAGIC_SELEMENT_PRINT(local_selement, MAGIC_EXPAND_TYPE_STR); printf("\n");
        }
    }
}

PUBLIC void st_cb_map_from_parent_union_selement_generic(_magic_selement_t *cached_selement, _magic_selement_t *local_selement, struct st_cb_info *cb_info, int is_trg_mapping)
{
    /* This should only be called in case of unions transferred as structs. */
    st_cb_map_from_parent_struct_selement_generic(cached_selement, local_selement, cb_info, is_trg_mapping);
}

PUBLIC void st_cb_map_child_union_selement_generic(_magic_selement_t *cached_selement, _magic_selement_t *local_selement, struct st_cb_info *cb_info, int is_trg_mapping)
{
    /* Match unions just like structs. */
    st_cb_map_child_struct_selement_generic(cached_selement, local_selement, cb_info, is_trg_mapping);
}

PUBLIC void st_cb_map_from_parent_struct_selement_generic(_magic_selement_t *cached_selement, _magic_selement_t *local_selement, struct st_cb_info *cb_info, int is_trg_mapping)
{
    unsigned int i;
    const char *cached_member_name;
    /* Match struct/unions with struct/unions. */
    assert(cached_selement->parent_type->type_id == MAGIC_TYPE_STRUCT || cached_selement->parent_type->type_id == MAGIC_TYPE_UNION);
    if (local_selement->parent_type->type_id != MAGIC_TYPE_STRUCT && local_selement->parent_type->type_id != MAGIC_TYPE_UNION) {
        local_selement->type = NULL;
        return;
    }
    /* Match struct/unions members by name. */
    cached_member_name = cached_selement->parent_type->member_names[cached_selement->child_num];
    for (i = 0 ; i < local_selement->parent_type->num_child_types ; i++) {
        if (!strcmp(local_selement->parent_type->member_names[i], cached_member_name)) {
            st_map_local_selement_from_child_num(local_selement, cb_info, i);
            return;
        }
    }
    local_selement->type = NULL;
}

PUBLIC void st_cb_map_child_struct_selement_generic(_magic_selement_t *cached_selement, _magic_selement_t *local_selement, struct st_cb_info *cb_info, int is_trg_mapping)
{
    unsigned int i, j;
    const struct _magic_type *cached_type = cached_selement->type;
    const struct _magic_type *local_type = local_selement->type;
    assert(cached_type->type_id == MAGIC_TYPE_STRUCT || cached_type->type_id == MAGIC_TYPE_UNION);
    if (local_type->type_id != MAGIC_TYPE_STRUCT && local_type->type_id != MAGIC_TYPE_UNION) {
        local_selement->type = NULL;
        return;
    }
    /* Match struct/unions by name(s). */
    if (!strcmp(cached_type->name, local_type->name)) {
        return;
    }
    if (cached_type->num_names > 1 || local_type->num_names > 1 ) {
        for (i = 0 ; i < cached_type->num_names ; i++) {
            for (j = 0 ; j < local_type->num_names ; j++) {
                if (!strcmp(cached_type->names[i], local_type->names[j])) {
                    return;
                }
            }
        }
    }

    local_selement->type = NULL;
}

PUBLIC void st_cb_map_child_nonaggr_selement_generic(_magic_selement_t *cached_selement, _magic_selement_t *local_selement, struct st_cb_info *cb_info, int is_trg_mapping)
{
    int r;
    static char magic_value_buffer[32];

    r = magic_selement_value_cast(cached_selement, local_selement, magic_value_buffer);
    if (r == 0) {
        return;
    }
    if (r < 0 && r != MAGIC_ERANGE && r != MAGIC_ESIGN) {
        local_selement->type = NULL;
        return;
    }
    if ((r == MAGIC_ERANGE && (st_policies & ST_REPORT_PRECISION_LOSS))
        || (r == MAGIC_ESIGN && (st_policies & ST_REPORT_SIGN_CHANGE))) {
        _magic_selement_t converted_selement = *cached_selement;
        converted_selement.address = magic_value_buffer;
        converted_selement.type = local_selement->type;
        printf("st_cb_map_child_nonaggr_selement_generic: %s while mapping non-aggregate selements:\n", r == MAGIC_ERANGE ? "Precision loss" : "Sign change");
        MAGIC_SELEMENT_PRINT(cached_selement, MAGIC_EXPAND_TYPE_STR); printf("\n");
        MAGIC_SELEMENT_PRINT(local_selement, MAGIC_EXPAND_TYPE_STR); printf("\n");
        printf(" - ORIGINAL VALUE: "); magic_selement_print_value(cached_selement); printf("\n");
        printf(" - MAPPED   VALUE: "); magic_selement_print_value(&converted_selement); printf("\n");
    }
    cached_selement->address = magic_value_buffer;
}

PUBLIC void st_cb_map_from_parent_selement_generic(_magic_selement_t *cached_selement, _magic_selement_t *local_selement, struct st_cb_info *cb_info, int is_trg_mapping)
{
    assert(cached_selement->parent_type && local_selement->parent_type);
    switch(cached_selement->parent_type->type_id) {
        case MAGIC_TYPE_ARRAY:
        case MAGIC_TYPE_VECTOR:
             st_cb_map_from_parent_array_selement_generic(cached_selement, local_selement, cb_info, is_trg_mapping);
        break;

        case MAGIC_TYPE_UNION:
            st_cb_map_from_parent_union_selement_generic(cached_selement, local_selement, cb_info, is_trg_mapping);
        break;

        case MAGIC_TYPE_STRUCT:
            st_cb_map_from_parent_struct_selement_generic(cached_selement, local_selement, cb_info, is_trg_mapping);
        break;

        default:
            st_cbs_os.panic("Invalid parent type!");
        break;
    }
}

PUBLIC void st_cb_map_child_selement_generic(_magic_selement_t *cached_selement, _magic_selement_t *local_selement, struct st_cb_info *cb_info, int is_trg_mapping)
{
    assert(cached_selement->type);
    if (local_selement->type == NULL) {
        return;
    }
    if (cached_selement->type->num_child_types == 0 || cached_selement->type->type_id == MAGIC_TYPE_POINTER) {
        st_cb_map_child_nonaggr_selement_generic(cached_selement, local_selement, cb_info, is_trg_mapping);
        return;
    }
    switch (cached_selement->type->type_id) {
        case MAGIC_TYPE_ARRAY:
        case MAGIC_TYPE_VECTOR:
            st_cb_map_child_array_selement_generic(cached_selement, local_selement, cb_info, is_trg_mapping);
        break;

        case MAGIC_TYPE_UNION:
            st_cb_map_child_union_selement_generic(cached_selement, local_selement, cb_info, is_trg_mapping);
        break;

        case MAGIC_TYPE_STRUCT:
            st_cb_map_child_struct_selement_generic(cached_selement, local_selement, cb_info, is_trg_mapping);
        break;

        default:
            st_cbs_os.panic("Invalid parent type!");
        break;
    }
}


PUBLIC void st_cb_map_selement_generic(_magic_selement_t *cached_selement, _magic_selement_t *local_selement, struct st_cb_info *cb_info, int is_trg_mapping)
{
    int i;
    assert(cached_selement->type->type_id != MAGIC_TYPE_FUNCTION);
    for (i = 0 ; i < MAGIC_ST_TYPE_TRANS_ITERATIONS ; i++) {
        if (cached_selement->parent_type) {
            st_cb_map_from_parent_selement_generic(cached_selement, local_selement, cb_info, is_trg_mapping);
        }
        st_cb_map_child_selement_generic(cached_selement, local_selement, cb_info, is_trg_mapping);
    }
}

PRIVATE INLINE void st_map_selement(_magic_selement_t *cached_selement, _magic_selement_t *local_selement, struct st_cb_info *cb_info, int is_trg_mapping)
{
    const struct _magic_type *cached_parent_type = cached_selement->parent_type;
    const struct _magic_type *local_parent_type = local_selement->parent_type;
    const struct _magic_type *cached_type = cached_selement->type;

    if (cached_parent_type) {
        if (cached_parent_type == local_parent_type) {
            /* Quickly propagate perfect type pairs from parents. */
            local_selement->address = (char *)local_selement->parent_address + ((char *)cached_selement->address - (char *)cached_selement->parent_address);
            local_selement->type = cached_type;
            return;
        }
        else if (ST_TYPE_IS_CACHED_COUNTERPART(cached_parent_type, local_parent_type)) {
            /* Quickly propagate type pairs from parents. */
            st_map_local_selement_from_child_num(local_selement, cb_info, cached_selement->child_num);
            return;
        }
        else {
            local_selement->type = NULL;
        }
    }
    else {
        /* In case of target mapping, we don't care about compatible types. When paired types are found, add a perfect type pair to speed up subsequent lookups. */
        if (ST_TYPE_IS_CACHED_COUNTERPART(cached_type, local_selement->type)) {
            if (is_trg_mapping) local_selement->type = cached_type;
            return;
        }
    }
#if CHECK_ASR && !FORCE_SOME_UNPAIRED_TYPES
    if (cb_info->init_info->flags & ST_LU_ASR) {
        st_cbs_os.panic("ASR should never get here!");
    }
#endif

    st_num_type_transformations++;
    st_cbs.st_cb_selement_map(cached_selement, local_selement, cb_info, is_trg_mapping);

    /* Check again for paired types and add a perfect type pair to speed up subsequent lookups in case of target mapping. */
    if (is_trg_mapping && local_selement->type != NULL && local_selement->type != cached_selement->type) {
        if (ST_TYPE_IS_CACHED_COUNTERPART(cached_selement->type, local_selement->type)) {
            local_selement->type = cached_selement->type;
        }
    }
}

/* main functions */

PUBLIC int st_state_transfer(st_init_info_t *info)
{
    int r;

    /*
     * Set all OS dependent callbacks first.
     */
    st_setcb_os_all(&info->st_cbs_os);

#if ST_ASSUME_RAW_COPY_BEFORE_TRANSFER
    _magic_vars->fake_malloc = 1;
#endif

    r = st_init(info);

#if ST_ASSUME_RAW_COPY_BEFORE_TRANSFER
    _magic_vars->fake_malloc = 0;
#endif

    if (r != OK) {
        return r;
    }

    r = st_data_transfer(info);
    if (r != OK) {
        return r;
    }

#if ST_DEBUG_LEVEL > 0
    printf("st_state_transfer: state transfer is done, num type transformations: %u.\n", st_num_type_transformations);
#endif

    st_cleanup(info);

    return OK;
}

#if APPARENTLY_UNUSED
PUBLIC void st_set_policies(int policies)
{
    st_policies = policies;
}
#endif

#if MAGIC_LOOKUP_SENTRY_ALLOW_RANGE_INDEX
PRIVATE void st_init_rl_index(st_init_info_t *info,
    struct _magic_vars_t *magic_vars)
{
    size_t buff_size;
    void *buff;

    EXEC_WITH_MAGIC_VARS(
        buff_size = magic_sentry_rl_estimate_index_buff_size(0);
        , magic_vars
    );
    buff = st_buff_allocate(info, buff_size);

    EXEC_WITH_MAGIC_VARS(
        magic_sentry_rl_build_index(buff, buff_size);
        , magic_vars
    );
}

PRIVATE void st_cleanup_rl_index(st_init_info_t *info,
    struct _magic_vars_t *magic_vars)
{
    EXEC_WITH_MAGIC_VARS(
        magic_sentry_rl_destroy_index();
        , magic_vars
    );
}
#endif

#if MAGIC_LOOKUP_SENTRY_ALLOW_NAME_HASH
PRIVATE void st_init_sentry_hash(st_init_info_t *info,
    struct _magic_vars_t *magic_vars)
{
    size_t buff_size;
    void *buff;

    EXEC_WITH_MAGIC_VARS(
        buff_size = magic_sentry_hash_estimate_buff_size(0);
        , magic_vars
    );
    buff = st_buff_allocate(info, buff_size);

    EXEC_WITH_MAGIC_VARS(
        magic_sentry_hash_build(buff, buff_size);
        , magic_vars
    );
}

PRIVATE void st_cleanup_sentry_hash(st_init_info_t *info,
    struct _magic_vars_t *magic_vars)
{
    EXEC_WITH_MAGIC_VARS(
        magic_sentry_hash_destroy();
        , magic_vars
    );
}
#endif

#if MAGIC_LOOKUP_FUNCTION_ALLOW_ADDR_HASH
PRIVATE void st_init_function_hash(st_init_info_t *info,
    struct _magic_vars_t *magic_vars)
{
    size_t buff_size;
    void *buff;

    EXEC_WITH_MAGIC_VARS(
        buff_size = magic_function_hash_estimate_buff_size(0);
        , magic_vars
    );
    buff = st_buff_allocate(info, buff_size);

    EXEC_WITH_MAGIC_VARS(
        magic_function_hash_build(buff, buff_size);
        , magic_vars
    );
}

PRIVATE void st_cleanup_function_hash(st_init_info_t *info,
    struct _magic_vars_t *magic_vars)
{
    EXEC_WITH_MAGIC_VARS(
        magic_function_hash_destroy();
        , magic_vars
    );
}
#endif

PRIVATE void st_vars_clear_ptrs(struct _magic_vars_t *magic_vars)
{
#undef __X
#define __X(x) offsetof(struct _magic_vars_t, x)
    size_t offset_list[] = { ST_MAGIC_VARS_PTR_CLEAR_LIST };
#undef __X
    unsigned int i;

    for (i = 0 ; i < sizeof(offset_list) / sizeof(size_t) ; i++)
        *((void **)(((char *)magic_vars) + offset_list[i])) = NULL;
}


#ifdef __MINIX
PRIVATE void st_unmap_mem(struct _magic_vars_t *magic_vars)
{
    int i, r;

    for (i = 0; i < MAGIC_UNMAP_MEM_ENTRIES; i++) {
        if (magic_vars->unmap_mem[i].length != 0) {
#if ST_DEBUG_LEVEL > 0
            printf("st_unmap_mem: unmapping (%p, %zu)\n",
                magic_vars->unmap_mem[i].start,
                magic_vars->unmap_mem[i].length);
#endif
            r = munmap(magic_vars->unmap_mem[i].start,
                magic_vars->unmap_mem[i].length);
            assert(r == 0);
        }
    }
}
#endif

PUBLIC int st_init(st_init_info_t *info)
{
    size_t max_buff_sz = 0;
    int r, dsentries_num;
    int allow_unpaired_types = TRUE;
    if (st_init_done) {
        return OK;
    }
    if (!_magic_enabled) return ENOSYS;
    st_init_done = TRUE;

    /* Ignore nested mempool dsentries for now. */
    magic_lookup_nested_dsentries = 0;

    /* Override default state transfer policies for ASR. */
    if ((info->flags & ST_LU_ASR) && st_policies == ST_POLICIES_DEFAULT) {
        st_policies = ST_POLICIES_DEFAULT_TRANSFER_ASR;
    }

    /* Fixup state transfer policies based on current configuration. */
#if ST_ASSUME_RAW_COPY_BEFORE_TRANSFER
    st_policies &= (~ST_DEFAULT_ALLOC_CASCADE_XFER);
#elif !defined(__MINIX)
    st_policies |= ST_TRANSFER_DIRTY_ONLY;
#endif

    assert((!info->init_buff_start || (info->flags & ST_LU_NOMMAP)) && "st_init: no mmapping allowed, and no buffer is available");
    register_typenames_and_callbacks();

    /* Transfer _magic_vars, which contain addresses of the magic variables */
    r = st_cbs_os.old_state_table_lookup(info->info_opaque, &st_remote_magic_vars);
    assert( r == OK && "ERROR occurred during transfer of _magic_vars.");
    /*
     * Clear all pointers not explictly transferred, as they are not valid in
     * the new address space.
     */
    st_vars_clear_ptrs(&st_remote_magic_vars);

    /*
     * Some magic_vars members do not need transfer or adjustment
     * (e.g. the memory ranges). They are copied to st_cached_magic_vars
     * this way.
     */
    st_cached_magic_vars = st_remote_magic_vars;

    /* Transfer and adjust metadata */
    r = st_transfer_metadata_types(info, &st_cached_magic_vars, &st_remote_magic_vars, &st_counterparts);
    assert( r == OK && "ERROR occurred during transfer of type metadata.");
    r = transfer_metadata_functions(info, &st_cached_magic_vars, &st_remote_magic_vars, &st_counterparts);
    assert( r == OK && "ERROR occurred during transfer of function metadata.");
    r = transfer_metadata_dfunctions(info, &st_cached_magic_vars, &st_remote_magic_vars, &st_counterparts);
    assert( r == OK && "ERROR occurred during transfer of dfunction metadata.");
    r = transfer_metadata_sentries(info, &st_cached_magic_vars, &st_remote_magic_vars, &st_counterparts, &max_buff_sz);
    assert( r == OK && "ERROR occurred during transfer of sentry metadata.");
    r = st_transfer_metadata_dsentries(info, &st_cached_magic_vars, &st_remote_magic_vars, &st_counterparts, &max_buff_sz, &dsentries_num);
    assert( r == OK && "ERROR occurred during transfer of dsentry metadata.");

    /* Allocate buffer for data transfer */
    st_dsentry_buff = st_buff_allocate(info, max_buff_sz + sizeof(struct _magic_dsentry));
    if (!st_dsentry_buff) {
        printf("st_dsentry_buff could not be allocated.\n");
        return EGENERIC;
    }
    st_data_buff = &st_dsentry_buff[1];

    /* Allocate and initialize counterparts buffers. */
    st_counterparts.functions_size = st_cached_magic_vars.functions_num;
    st_counterparts.functions = st_buff_allocate(info, st_counterparts.functions_size * sizeof(st_ptr_mapping));
    assert(st_counterparts.functions && "st_counterparts.functions could not be allocated.");

    st_counterparts.types_size = st_cached_magic_vars.types_num;
    st_counterparts.types = st_buff_allocate(info, st_counterparts.types_size * sizeof(st_ptr_mapping));
    assert(st_counterparts.types && "st_counterparts.types could not be allocated.");
    st_counterparts.ptr_types = st_buff_allocate(info, st_counterparts.types_size * sizeof(st_ptr_mapping));
    assert(st_counterparts.ptr_types && "st_counterparts.ptr_types could not be allocated.");

    st_counterparts.sentries_size = st_cached_magic_vars.sentries_num + dsentries_num;
    st_counterparts.sentries = st_buff_allocate(info, st_counterparts.sentries_size * sizeof(st_ptr_mapping));
    assert(st_counterparts.sentries && "st_counterparts.sentries could not be allocated.");
    st_counterparts.sentries_data = st_buff_allocate(info, st_counterparts.sentries_size * sizeof(st_ptr_mapping));
    assert(st_counterparts.sentries_data && "st_counterparts.sentries_data could not be allocated.");

#if MAGIC_LOOKUP_SENTRY_ALLOW_RANGE_INDEX
    st_init_rl_index(info, &st_cached_magic_vars);
#endif

#if MAGIC_LOOKUP_SENTRY_ALLOW_NAME_HASH
    st_init_sentry_hash(info, &st_cached_magic_vars);
    st_init_sentry_hash(info, _magic_vars);
#endif

#if MAGIC_LOOKUP_FUNCTION_ALLOW_ADDR_HASH
    st_init_function_hash(info, &st_cached_magic_vars);
    st_init_function_hash(info, _magic_vars);
#endif

#ifdef __MINIX
    /* Unmap any memory ranges that are not needed in the new process. */
    st_unmap_mem(&st_cached_magic_vars);
#endif

    /* Pair metadata entities */
    r = pair_metadata_types(info, &st_cached_magic_vars, &st_counterparts, allow_unpaired_types);
    assert( r == OK && "ERROR occurred during call to pair_metadata_types().");
    r = pair_metadata_functions(info, &st_cached_magic_vars, &st_counterparts);
    assert( r == OK && "ERROR occurred during call to pair_metadata_functions().");
    r = pair_metadata_sentries(info, &st_cached_magic_vars, &st_counterparts);
    assert( r == OK && "ERROR occurred during call to pair_metadata_sentries().");
#if ST_ASSUME_RAW_COPY_BEFORE_TRANSFER
    r = allocate_pair_metadata_dsentries_from_raw_copy(info, &st_cached_magic_vars, &st_counterparts);
    assert( r == OK && "ERROR occurred during call to allocate_pair_metadata_dsentries().");
#else
    r = allocate_pair_metadata_dsentries(info, &st_cached_magic_vars, &st_counterparts);
    assert( r == OK && "ERROR occurred during call to allocate_pair_metadata_dsentries().");
#endif

    /* Set state transfer status defaults from the predefined policies. */
    st_set_status_defaults(info);

#if MAGIC_LOOKUP_SENTRY_ALLOW_RANGE_INDEX
    st_init_rl_index(info, _magic_vars);
#endif

    return OK;
}

PRIVATE INLINE char* st_lookup_str_local_data(struct _magic_sentry *cached_sentry)
{
    void *local_data_addr;
    assert(cached_sentry && MAGIC_SENTRY_IS_STRING(cached_sentry));
    ST_GET_CACHED_COUNTERPART(cached_sentry, sentries, sentries_data, local_data_addr);
    assert(local_data_addr && "String data not in cache!");
    return (char*) local_data_addr;
}

#if ST_DEBUG_DATA_TRANSFER
PRIVATE void st_print_sentities(struct _magic_vars_t *magic_vars)
{
    struct _magic_dsentry *dsentry = magic_vars->first_dsentry;
    int i;

    for (i = 0 ; i < magic_vars->sentries_num ; i++) {
        struct _magic_sentry *sentry = &magic_vars->sentries[i];
        ST_SENTRY_PRINT(sentry, 0);
        printf("\n");
    }

    while (dsentry != NULL) {
        ST_DSENTRY_PRINT(dsentry, 0);
        printf("\n");
        dsentry = dsentry->next;
    }

    for (i = 0 ; i < magic_vars->functions_num ; i++) {
        struct _magic_function *function = &magic_vars->functions[i];
        ST_FUNCTION_PRINT(function, 0);
        printf("\n");
    }
}
#endif

PUBLIC void st_map_str_sentries(struct _magic_sentry **cached_sentry_ptr, struct _magic_sentry **local_sentry_ptr)
{
    struct _magic_sentry *cached_sentry = *cached_sentry_ptr;
    struct _magic_sentry *local_sentry = *local_sentry_ptr;
    struct _magic_sentry *sentry = cached_sentry ? cached_sentry : local_sentry;
    int string_flags, match_by_name, match_by_content;
    ST_CHECK_INIT();
    assert((cached_sentry == NULL) ^ (local_sentry == NULL));

    string_flags = sentry->flags & (MAGIC_STATE_STRING|MAGIC_STATE_NAMED_STRING);
    assert(string_flags & MAGIC_STATE_STRING);
    match_by_name = (string_flags & MAGIC_STATE_NAMED_STRING) && (st_policies & ST_MAP_NAMED_STRINGS_BY_NAME);
    match_by_content = ((string_flags & MAGIC_STATE_NAMED_STRING) && (st_policies & ST_MAP_NAMED_STRINGS_BY_CONTENT))
        || (!(string_flags & MAGIC_STATE_NAMED_STRING) && (st_policies & ST_MAP_STRINGS_BY_CONTENT));
    if (match_by_name) {
        /* Pretend it's a regular sentry and match by name */
        sentry->flags &= ~string_flags;
        st_map_sentries(cached_sentry_ptr, local_sentry_ptr);
        sentry->flags |= string_flags;
        if (*cached_sentry_ptr && *local_sentry_ptr) {
            /* Found by name. */
            return;
        }
    }
    if (!match_by_content) {
        /* No match. */
        return;
    }
    if (cached_sentry) {
        EXEC_WITH_MAGIC_VARS(
            local_sentry = magic_sentry_lookup_by_string(st_lookup_str_local_data(cached_sentry));
            , st_local_magic_vars_ptr
        );
    }
    else {
        int i;
        for(i = 0 ; i < st_cached_magic_vars.sentries_num ; i++) {
            sentry = &st_cached_magic_vars.sentries[i];
            if (MAGIC_SENTRY_IS_STRING(sentry) && !strcmp(st_lookup_str_local_data(sentry), (char*)local_sentry->address)) {
                cached_sentry = sentry;
                break;
            }
        }
    }
    *cached_sentry_ptr = cached_sentry;
    *local_sentry_ptr = local_sentry;
}

PUBLIC void st_map_sentries(struct _magic_sentry **cached_sentry_ptr, struct _magic_sentry **local_sentry_ptr)
{
    struct _magic_sentry *cached_sentry = *cached_sentry_ptr;
    struct _magic_sentry *local_sentry = *local_sentry_ptr;
    struct _magic_dsentry *cached_dsentry = cached_sentry ? (MAGIC_STATE_FLAG(cached_sentry, MAGIC_STATE_DYNAMIC) ? MAGIC_DSENTRY_FROM_SENTRY(cached_sentry) : NULL) : NULL;
    struct _magic_dsentry *local_dsentry = local_sentry ? (MAGIC_STATE_FLAG(local_sentry, MAGIC_STATE_DYNAMIC) ? MAGIC_DSENTRY_FROM_SENTRY(local_sentry) : NULL) : NULL;
    ST_CHECK_INIT();
    assert((cached_sentry == NULL) ^ (local_sentry == NULL));

    if ((cached_sentry && MAGIC_STATE_FLAG(cached_sentry, MAGIC_STATE_STRING))
        || (local_sentry && MAGIC_STATE_FLAG(local_sentry, MAGIC_STATE_STRING))) {
            st_map_str_sentries(cached_sentry_ptr, local_sentry_ptr);
            return;
    }
    else if (cached_sentry) {
        EXEC_WITH_MAGIC_VARS(
            local_sentry = magic_sentry_lookup_by_name(cached_dsentry ? cached_dsentry->parent_name : "", cached_sentry->name, cached_dsentry ? cached_dsentry->site_id : 0, NULL);
            , st_local_magic_vars_ptr
        );
        assert(!local_sentry || MAGIC_STATE_FLAG(cached_sentry, MAGIC_STATE_DYNAMIC) == MAGIC_STATE_FLAG(local_sentry, MAGIC_STATE_DYNAMIC));
    }
    else {
        EXEC_WITH_MAGIC_VARS(
            cached_sentry = magic_sentry_lookup_by_name(local_dsentry ? local_dsentry->parent_name : "", local_sentry->name, local_dsentry ? local_dsentry->site_id : 0, NULL);
            , &st_cached_magic_vars
        );
        assert(!cached_sentry || MAGIC_STATE_FLAG(local_sentry, MAGIC_STATE_DYNAMIC) == MAGIC_STATE_FLAG(cached_sentry, MAGIC_STATE_DYNAMIC));
    }
    *cached_sentry_ptr = cached_sentry;
    *local_sentry_ptr = local_sentry;
}

PRIVATE struct _magic_sentry *st_lookup_cached_sentry(struct _magic_sentry *local_sentry)
{
    int i;
    struct _magic_dsentry *cached_dsentry;
    assert(local_sentry);

    for (i = 0 ; i < st_counterparts.sentries_size ; i++) {
        if (st_counterparts.sentries[i].counterpart == local_sentry) {
            break;
        }
    }
    if (i >= st_counterparts.sentries_size) {
        return NULL;
    }
    if (i < st_cached_magic_vars.sentries_num) {
        return &st_cached_magic_vars.sentries[i];
    }
    i -= st_cached_magic_vars.sentries_num;
    cached_dsentry = st_cached_magic_vars.first_dsentry;
    assert(i >= 0);
    assert(cached_dsentry);
    while (i > 0) {
        cached_dsentry = cached_dsentry->next;
        assert(cached_dsentry);
        i--;
    }
    return MAGIC_DSENTRY_TO_SENTRY(cached_dsentry);
}

PUBLIC void st_lookup_sentry_pair(struct _magic_sentry **cached_sentry_ptr, struct _magic_sentry **local_sentry_ptr)
{
    struct _magic_sentry *cached_sentry = *cached_sentry_ptr;
    struct _magic_sentry *local_sentry = *local_sentry_ptr;
    ST_CHECK_INIT();
    assert((cached_sentry == NULL) ^ (local_sentry == NULL));

    if (cached_sentry) {
        ST_GET_CACHED_COUNTERPART(cached_sentry, sentries, sentries, local_sentry);
    }
    else if (MAGIC_SENTRY_IS_STRING(local_sentry)) {
        /* strings are special, they may have multiple local duplicates */
        struct _magic_sentry *csentry = NULL, *lsentry = NULL;
        st_map_str_sentries(&csentry, &local_sentry);
        if (csentry) {
            st_lookup_sentry_pair(&csentry, &lsentry);
            if (lsentry) {
                cached_sentry = csentry;
            }
        }
    }
    else {
        cached_sentry = st_lookup_cached_sentry(local_sentry);
    }
    *cached_sentry_ptr = cached_sentry;
    *local_sentry_ptr = local_sentry;
}

PRIVATE INLINE void st_unpair_local_alloc_sentry(struct _magic_sentry *local_sentry)
{
    if (st_policies & ST_ON_ALLOC_UNPAIR_ERROR) {
        st_cbs_os.panic("st_unpair_local_alloc_sentry: Error: attempting to unpair a local alloc sentry!");
    }
    else if (st_policies & ST_ON_ALLOC_UNPAIR_DEALLOCATE) {
        deallocate_local_dsentry(MAGIC_DSENTRY_FROM_SENTRY(local_sentry));
    }
}

PUBLIC void st_add_sentry_pair(struct _magic_sentry *cached_sentry, struct _magic_sentry *local_sentry)
{
    ST_CHECK_INIT();
    assert(cached_sentry || local_sentry);

    if (local_sentry) {
        struct _magic_sentry *csentry = NULL;
        st_lookup_sentry_pair(&csentry, &local_sentry);
        if (csentry) {
            ST_SET_CACHED_COUNTERPART(csentry, sentries, sentries, NULL);
        }
        if (!cached_sentry && MAGIC_SENTRY_IS_ALLOC(local_sentry)) {
            st_unpair_local_alloc_sentry(local_sentry);
        }
    }
    if (cached_sentry) {
        struct _magic_sentry *lsentry = NULL;
        st_lookup_sentry_pair(&cached_sentry, &lsentry);
        if (lsentry && MAGIC_SENTRY_IS_ALLOC(lsentry)) {
            st_unpair_local_alloc_sentry(lsentry);
        }
        ST_SET_CACHED_COUNTERPART(cached_sentry, sentries, sentries, local_sentry);
    }
}

PUBLIC int st_add_sentry_pair_alloc_by_dsindex(st_init_info_t *info, struct _magic_sentry *cached_sentry, struct _magic_dsindex *local_dsindex, int num_elements, const union __alloc_flags *p_alloc_flags)
{
    int r;
    struct _magic_dsentry *local_dsentry;
    ST_CHECK_INIT();
    assert(cached_sentry);

    if (!local_dsindex) {
        st_add_sentry_pair(cached_sentry, NULL);
        return OK;
    }

    r = allocate_local_dsentry(info, local_dsindex, num_elements, MAGIC_STATE_FLAG(cached_sentry, MAGIC_STATE_TYPE_SIZE_MISMATCH), p_alloc_flags, &local_dsentry, NULL, MAGIC_PTR_TO_DSENTRY(cached_sentry->address));
    if (r != OK) {
        return r;
    }
    st_add_sentry_pair(cached_sentry, MAGIC_DSENTRY_TO_SENTRY(local_dsentry));
    return OK;
}

PUBLIC void st_map_functions(struct _magic_function **cached_function_ptr, struct _magic_function **local_function_ptr)
{
    struct _magic_function *cached_function = *cached_function_ptr;
    struct _magic_function *local_function = *local_function_ptr;
    ST_CHECK_INIT();
    assert((cached_function == NULL) ^ (local_function == NULL));

    if (cached_function) {
        EXEC_WITH_MAGIC_VARS(
            local_function = magic_function_lookup_by_name(NULL, cached_function->name);
            , st_local_magic_vars_ptr
        );
    }
    else {
        EXEC_WITH_MAGIC_VARS(
            cached_function = magic_function_lookup_by_name(NULL, local_function->name);
            , &st_cached_magic_vars
        );
    }
    *cached_function_ptr = cached_function;
    *local_function_ptr = local_function;
}

PUBLIC void st_lookup_function_pair(struct _magic_function **cached_function_ptr, struct _magic_function **local_function_ptr)
{
    struct _magic_function *cached_function = *cached_function_ptr;
    struct _magic_function *local_function = *local_function_ptr;
    int i;
    ST_CHECK_INIT();
    assert((cached_function == NULL) ^ (local_function == NULL));

    if (cached_function) {
        if ((int)cached_function->id - 1 >= st_counterparts.functions_size) {
            /*
             * Try to check if this is a function
             * from an external shared object.
             * XXX: The number of dfunctions can be quite large,
             * so this needs to be done more efficiently.
             */
            struct _magic_dfunction *dfunc;
            struct _magic_function *func;
            MAGIC_DFUNCTION_FUNC_ITER(_magic_vars->first_dfunction, dfunc, func,
                if (func->address == cached_function->address) {
                    local_function = func;
                    break;
                }
            );
            assert(local_function != NULL && "No counterpart found for function.");
        } else {
            ST_GET_CACHED_COUNTERPART(cached_function, functions, functions, local_function);
        }
    }
    else {
        assert(st_counterparts.functions_size == st_cached_magic_vars.functions_num);
        for(i = 0 ; i < st_counterparts.functions_size ; i++) {
            if(st_counterparts.functions[i].counterpart == local_function) {
                cached_function = &st_cached_magic_vars.functions[i];
                break;
            }
        }
    }
    *cached_function_ptr = cached_function;
    *local_function_ptr = local_function;
}

PUBLIC void st_add_function_pair(struct _magic_function *cached_function, struct _magic_function *local_function)
{
    ST_CHECK_INIT();
    assert(cached_function || local_function);

    if (local_function) {
        struct _magic_function *cfunction = NULL;
        st_lookup_function_pair(&cfunction, &local_function);
        if (cfunction) {
            ST_SET_CACHED_COUNTERPART(cfunction, functions, functions, NULL);
        }
    }
    if (cached_function) {
        ST_SET_CACHED_COUNTERPART(cached_function, functions, functions, local_function);
    }
}

PUBLIC int st_sentry_equals(struct _magic_sentry *cached_sentry, struct _magic_sentry *local_sentry)
{
    const char *cached_parent_name = "", *local_parent_name = "";
    int cached_flags = MAGIC_STATE_FLAGS_TO_NONEXTF(cached_sentry->flags) & (~MAGIC_STATE_ADDR_NOT_TAKEN);
    int local_flags = MAGIC_STATE_FLAGS_TO_NONEXTF(local_sentry->flags) & (~MAGIC_STATE_ADDR_NOT_TAKEN);
    if (cached_flags != local_flags) {
        return FALSE;
    }
    if (MAGIC_STATE_FLAG(cached_sentry, MAGIC_STATE_STRING)) {
        return !strcmp(st_lookup_str_local_data(cached_sentry), (char*)local_sentry->address);
    }
    if (MAGIC_STATE_FLAG(cached_sentry, MAGIC_STATE_DYNAMIC)) {
        cached_parent_name = MAGIC_DSENTRY_FROM_SENTRY(cached_sentry)->parent_name;
        local_parent_name = MAGIC_DSENTRY_FROM_SENTRY(local_sentry)->parent_name;
    }
    if (strcmp(cached_sentry->name, local_sentry->name) || strcmp(cached_parent_name, local_parent_name)) {
        return FALSE;
    }
    return magic_type_compatible(cached_sentry->type, local_sentry->type, MAGIC_TYPE_COMPARE_ALL);
}

PUBLIC int st_function_equals(struct _magic_function *cached_function, struct _magic_function *local_function)
{
    if (MAGIC_STATE_FLAGS_TO_NONEXTF(local_function->flags) != MAGIC_STATE_FLAGS_TO_NONEXTF(cached_function->flags)) {
        return FALSE;
    }
    return !strcmp(cached_function->name, local_function->name);
}

PUBLIC void st_print_sentry_diff(st_init_info_t *info, struct _magic_sentry *cached_sentry, struct _magic_sentry *local_sentry, int raw_diff, int print_changed)
{
    int is_paired_sentry;
    ST_CHECK_INIT();

    if (!cached_sentry || !local_sentry) {
        if (raw_diff) {
            st_map_sentries(&cached_sentry, &local_sentry);
        }
        else {
            st_lookup_sentry_pair(&cached_sentry, &local_sentry);
        }
    }
    is_paired_sentry = (cached_sentry != NULL && local_sentry != NULL);
    if (is_paired_sentry && st_sentry_equals(cached_sentry, local_sentry)) {
        return;
    }
    if (is_paired_sentry && !print_changed) {
        return;
    }
    if (cached_sentry) {
        printf("-"); ST_SENTRY_PRINT(cached_sentry, MAGIC_EXPAND_TYPE_STR); printf("\n");
    }
    if (local_sentry) {
        printf("+"); ST_SENTRY_PRINT(local_sentry, MAGIC_EXPAND_TYPE_STR); printf("\n");
    }
    printf("\n");
}

PUBLIC void st_print_function_diff(st_init_info_t *info, struct _magic_function *cached_function, struct _magic_function *local_function, int raw_diff, int print_changed)
{
    int is_paired_function;
    ST_CHECK_INIT();

    if (!cached_function || !local_function) {
        if (raw_diff) {
            st_map_functions(&cached_function, &local_function);
        }
        else {
            st_lookup_function_pair(&cached_function, &local_function);
        }
    }
    is_paired_function = (cached_function != NULL && local_function != NULL);
    if (is_paired_function && st_function_equals(cached_function, local_function)) {
        return;
    }
    if (is_paired_function && !print_changed) {
        return;
    }
    if (cached_function) {
        printf("-"); ST_FUNCTION_PRINT(cached_function, MAGIC_EXPAND_TYPE_STR); printf("\n");
    }
    if (local_function) {
        printf("+"); ST_FUNCTION_PRINT(local_function, MAGIC_EXPAND_TYPE_STR); printf("\n");
    }
    printf("\n");
}

PUBLIC void st_print_sentries_diff(st_init_info_t *info, int raw_diff, int print_changed)
{
    int i;
    ST_CHECK_INIT();

    for (i = 0 ; i < st_cached_magic_vars.sentries_num ; i++) {
        struct _magic_sentry *cached_sentry = &st_cached_magic_vars.sentries[i];
        st_print_sentry_diff(info, cached_sentry, NULL, raw_diff, print_changed);
    }

    print_changed = FALSE;
    for (i = 0 ; i < st_local_magic_vars_ptr->sentries_num ; i++) {
        struct _magic_sentry *local_sentry = &st_local_magic_vars_ptr->sentries[i];
        st_print_sentry_diff(info, NULL, local_sentry, raw_diff, print_changed);
    }
}

PUBLIC void st_print_dsentries_diff(st_init_info_t *info, int raw_diff, int print_changed)
{
    struct _magic_dsentry *dsentry;
    ST_CHECK_INIT();

    dsentry = st_cached_magic_vars.first_dsentry;
    while (dsentry != NULL) {
        struct _magic_sentry *cached_sentry = MAGIC_DSENTRY_TO_SENTRY(dsentry);
        st_print_sentry_diff(info, cached_sentry, NULL, raw_diff, print_changed);
        dsentry = dsentry->next;
    }

    dsentry = st_local_magic_vars_ptr->first_dsentry;
    print_changed = FALSE;
    while (dsentry != NULL) {
        struct _magic_sentry *local_sentry = MAGIC_DSENTRY_TO_SENTRY(dsentry);
        st_print_sentry_diff(info, NULL, local_sentry, raw_diff, print_changed);
        dsentry = dsentry->next;
    }
}

PUBLIC void st_print_functions_diff(st_init_info_t *info, int raw_diff, int print_changed)
{
    int i;
    ST_CHECK_INIT();

    for(i = 0 ; i < st_cached_magic_vars.functions_num ; i++) {
        struct _magic_function *cached_function = &st_cached_magic_vars.functions[i];
        st_print_function_diff(info, cached_function, NULL, raw_diff, print_changed);
    }

    print_changed = FALSE;
    for (i = 0 ; i < st_local_magic_vars_ptr->functions_num ; i++) {
        struct _magic_function *local_function = &st_local_magic_vars_ptr->functions[i];
        st_print_function_diff(info, NULL, local_function, raw_diff, print_changed);
    }
}

PUBLIC void st_print_state_diff(st_init_info_t *info, int raw_diff, int print_changed)
{
    ST_CHECK_INIT();

    printf("Index: sentries\n");
    printf("===================================================================\n");
    st_print_sentries_diff(info, raw_diff, print_changed);

    printf("\nIndex: dsentries\n");
    printf("===================================================================\n");
    st_print_dsentries_diff(info, raw_diff, print_changed);

    printf("\nIndex: functions\n");
    printf("===================================================================\n");
    st_print_functions_diff(info, raw_diff, print_changed);
    printf("\n");
}

PUBLIC int st_data_transfer(st_init_info_t *info)
{
    struct _magic_dsentry *dsentry;
    int i, r;
    int sentry_transferred;
#if ST_DEBUG_DATA_TRANSFER
    int counter = 1;
#endif
    ST_CHECK_INIT();

    /* Check unpaired sentries. */
    for (i = 0 ; i < st_cached_magic_vars.sentries_num ; i++) {
        struct _magic_sentry *sentry = &st_cached_magic_vars.sentries[i];
        int is_paired_sentry = ST_HAS_CACHED_COUNTERPART(sentry, sentries);
        if (!is_paired_sentry) {
            r = check_unpaired_sentry(info, sentry);
            if (r != OK) {
                return r;
            }
        }
    }

    /* Check unpaired dsentries. */
    dsentry = st_cached_magic_vars.first_dsentry;
    while (dsentry != NULL) {
        struct _magic_sentry *sentry = MAGIC_DSENTRY_TO_SENTRY(dsentry);
        int is_paired_sentry = ST_HAS_CACHED_COUNTERPART(sentry, sentries);
        if (!is_paired_sentry) {
            r = check_unpaired_sentry(info, sentry);
            if (r != OK) {
                return r;
            }
        }
        dsentry = dsentry->next;
    }

    /* Data transfer. */
    do {
        sentry_transferred = 0;

#if ST_DEBUG_DATA_TRANSFER
        printf("st_data_transfer: Round %d\n", counter++);
        st_print_sentities(&st_cached_magic_vars);
#endif

        /* process sentries */
#if ST_DEBUG_LEVEL > 0
        printf("st_data_transfer: processing sentries\n");
#endif
        for(i = 0 ; i < st_cached_magic_vars.sentries_num ; i++) {
            struct _magic_sentry *sentry = &st_cached_magic_vars.sentries[i];
            int is_paired_sentry = ST_HAS_CACHED_COUNTERPART(sentry, sentries);
            int sentry_needs_transfer = MAGIC_STATE_EXTF_GET(sentry, ST_NEEDS_TRANSFER | ST_TRANSFER_DONE) == ST_NEEDS_TRANSFER;
            if (sentry_needs_transfer && is_paired_sentry) {
                r = transfer_data_sentry(info, sentry);
                if (r != OK) {
                    return r;
                }
                sentry_transferred = 1;
            }
        }

        /* process dsentries */
#if ST_DEBUG_LEVEL > 0
        printf("st_data_transfer: processing dsentries\n");
#endif
        dsentry = st_cached_magic_vars.first_dsentry;
        while (dsentry != NULL) {
            struct _magic_sentry *sentry = MAGIC_DSENTRY_TO_SENTRY(dsentry);
            int is_paired_sentry = ST_HAS_CACHED_COUNTERPART(sentry, sentries);
            int sentry_needs_transfer = MAGIC_STATE_EXTF_GET(sentry, ST_NEEDS_TRANSFER | ST_TRANSFER_DONE) == ST_NEEDS_TRANSFER;
            if (sentry_needs_transfer && is_paired_sentry) {
                r = transfer_data_sentry(info, sentry);
                if (r != OK) {
                    return r;
                }
                sentry_transferred = 1;
            }
            dsentry = dsentry->next;
        }

    } while(sentry_transferred);

    return OK;
}

PRIVATE INLINE void st_set_transfer_status(int status_flags, int status_op,
    struct _magic_sentry *cached_sentry, struct _magic_function *cached_function)
{
#define __st_set_transfer_status(X)                         \
    switch(status_op) {                                     \
        case ST_OP_NONE:                                    \
            return;                                         \
        break;                                              \
        case ST_OP_ADD:                                     \
            MAGIC_STATE_EXTF_ADD(X, status_flags);          \
        break;                                              \
        case ST_OP_DEL:                                     \
            MAGIC_STATE_EXTF_DEL(X, status_flags);          \
        break;                                              \
        case ST_OP_SET:                                     \
            MAGIC_STATE_EXTF_SET(X, status_flags);          \
        break;                                              \
        case ST_OP_CLEAR:                                   \
            MAGIC_STATE_EXTF_CLEAR(X);                      \
        break;                                              \
        default:                                            \
            st_cbs_os.panic("Invalid operation!");          \
        break;                                              \
    }                                                       \

    if (cached_sentry) {
        __st_set_transfer_status(cached_sentry);
    }
    else {
        assert(cached_function);
        __st_set_transfer_status(cached_function);
    }
}

PUBLIC void st_set_unpaired_types_ratios(float unpaired_types_ratio,
    float unpaired_struct_types_ratio)
{
    st_unpaired_types_ratio = unpaired_types_ratio;
    st_unpaired_struct_types_ratio = unpaired_struct_types_ratio;
}

PUBLIC void st_set_status_defaults(st_init_info_t *info)
{
    int match_all = ~0, skip_none = 0;
    int skip_state_flags = (st_policies & ST_DEFAULT_SKIP_STACK) ? MAGIC_STATE_STACK : 0;

    if (!(st_policies & ST_DEFAULT_TRANSFER_NONE)) {
        /*
         * Transfer all the (d)sentries by default. Skip stack dsentries when
         * requested. In that case, stack dsentries won't be transferred and an
         * error will be raised on stack pointer transfer.
         */
        st_set_status_by_state_flags(ST_NEEDS_TRANSFER, ST_OP_SET,
            match_all, skip_state_flags);
        if (st_policies & ST_DEFAULT_ALLOC_CASCADE_XFER) {
            /*
             * If requested, mark non-stack dsentries for cascade transfer
             * instead of regular transfer.
             */
            st_set_status_by_state_flags(ST_ON_PTRXFER_CASCADE, ST_OP_SET,
                MAGIC_STATE_HEAP | MAGIC_STATE_MAP, skip_none);
        }
    }
    else {
        /*
         * Don't transfer any (d)sentries by default. Mark all the (d)sentries
         * for cascade transfer (except for stack dsentries when requested).
         */
        st_set_status_by_state_flags(ST_ON_PTRXFER_CASCADE, ST_OP_SET,
            match_all, skip_state_flags);
    }

    /*
     * Always transfer all immutable objects.
     */
    st_set_status_by_state_flags(ST_NEEDS_TRANSFER, ST_OP_SET,
        MAGIC_STATE_IMMUTABLE, skip_none);

    /*
     * If requested, mark library state dsentries as already transferred too.
     */
    if (st_policies & ST_DEFAULT_SKIP_LIB_STATE) {
        st_set_status_by_state_flags(ST_NEEDS_TRANSFER | ST_TRANSFER_DONE,
            ST_OP_ADD, MAGIC_STATE_LIB, skip_none);
    }

    /*
     * In addition, mark functions, out-of-band/string sentries
     * and shared dsentries as already transferred.
     */
    st_set_status_by_state_flags(ST_NEEDS_TRANSFER | ST_TRANSFER_DONE,
        ST_OP_ADD, MAGIC_STATE_TEXT | MAGIC_STATE_OUT_OF_BAND |
        MAGIC_STATE_STRING | MAGIC_STATE_CONSTANT | MAGIC_STATE_SHM, skip_none);

    /*
     * Finally, if we only want to transfer dirty sentries, mark all the other ones
     * as already transferred.
     */
    if (st_policies & ST_TRANSFER_DIRTY_ONLY) {
        st_set_status_by_state_flags(ST_TRANSFER_DONE, ST_OP_ADD, match_all, MAGIC_STATE_DIRTY_PAGE);
    }

#if DO_SKIP_ENVIRON_HACK
    st_set_status_by_name(ST_NEEDS_TRANSFER | ST_TRANSFER_DONE,
        ST_OP_ADD, NULL, "__environ", MAGIC_DSENTRY_SITE_ID_NULL);
    st_set_status_by_name(ST_NEEDS_TRANSFER | ST_TRANSFER_DONE,
        ST_OP_ADD, NULL, "stderr", MAGIC_DSENTRY_SITE_ID_NULL);
#endif
}

PUBLIC void st_set_status_by_state_flags(int status_flags, int status_op,
    int match_state_flags, int skip_state_flags)
{
    struct _magic_dsentry *dsentry = st_cached_magic_vars.first_dsentry;
    int i;
    int candidate_sentry_flags = MAGIC_STATE_DATA | MAGIC_STATE_STRING | MAGIC_STATE_CONSTANT | MAGIC_STATE_ADDR_NOT_TAKEN;
    int candidate_function_flags = MAGIC_STATE_TEXT;
    int candidate_dsentry_flags = ~(candidate_sentry_flags | candidate_function_flags);
    ST_CHECK_INIT();

    /* process sentries */
    if (match_state_flags & candidate_sentry_flags) {
        for (i = 0 ; i < st_cached_magic_vars.sentries_num ; i++) {
            int state_flags = st_cached_magic_vars.sentries[i].flags;
            if ((state_flags & match_state_flags) && !(state_flags & skip_state_flags)) {
                st_set_transfer_status(status_flags, status_op, &st_cached_magic_vars.sentries[i], NULL);
            }
        }
    }

    /* process dsentries */
    if (match_state_flags & candidate_dsentry_flags) {
        while (dsentry != NULL) {
            int state_flags = dsentry->sentry.flags;
            if ((state_flags & match_state_flags) && !(state_flags & skip_state_flags)) {
                st_set_transfer_status(status_flags, status_op, MAGIC_DSENTRY_TO_SENTRY(dsentry), NULL);
            }
            dsentry = dsentry->next;
        }
    }

    /* process functions */
    if (match_state_flags & candidate_function_flags) {
        for (i = 0 ; i < st_cached_magic_vars.functions_num ; i++) {
            int state_flags = st_cached_magic_vars.functions[i].flags;
            if ((state_flags & match_state_flags) && !(state_flags & skip_state_flags)) {
                st_set_transfer_status(status_flags, status_op, NULL, &st_cached_magic_vars.functions[i]);
            }
        }
    }
}

PUBLIC int st_set_status_by_function_ids(int status_flags, int status_op, _magic_id_t *ids)
{
    int r, i = 0;
    while (ids[i] != 0) {
        r = st_set_status_by_function_id(status_flags, status_op, ids[i]);
        if (r != OK) {
            return r;
        }
        i++;
    }
    return OK;
}

PUBLIC int st_set_status_by_sentry_ids(int status_flags, int status_op, _magic_id_t *ids)
{
    int r, i=0;
    while (ids[i] != 0) {
        r = st_set_status_by_sentry_id(status_flags, status_op, ids[i]);
        if (r != OK) {
            return r;
        }
        i++;
    }
    return OK;
}

PUBLIC int st_set_status_by_names(int status_flags, int status_op,
    const char **parent_names, const char **names,
    _magic_id_t *dsentry_site_ids)
{
    int r, i = 0;
    while (names[i] != NULL) {
        r = st_set_status_by_name(status_flags, status_op,
            parent_names ? parent_names[i] : NULL, names[i],
            dsentry_site_ids ? dsentry_site_ids[i] :
                MAGIC_DSENTRY_SITE_ID_NULL);
        if (r != OK) {
            return r;
        }
        i++;
    }
    return OK;
}

PUBLIC int st_set_status_by_local_addrs(int status_flags, int status_op,
    void **addrs)
{
    int r, i=0;
    while (addrs[i] != NULL) {
        r = st_set_status_by_local_addr(status_flags, status_op, addrs[i]);
        if (r != OK) {
            return r;
        }
        i++;
    }
    return OK;
}

PUBLIC void st_set_status_by_sentry(int status_flags, int status_op,
    void *cached_sentry)
{
    ST_CHECK_INIT();

    st_set_transfer_status(status_flags, status_op,
        (struct _magic_sentry*) cached_sentry, NULL);
}

PUBLIC void st_set_status_by_function(int status_flags, int status_op,
    void *cached_function)
{
    ST_CHECK_INIT();

    st_set_transfer_status(status_flags, status_op,
        NULL, (struct _magic_function*) cached_function);
}

PUBLIC int st_set_status_by_name(int status_flags, int status_op,
    const char *parent_name, const char *name, _magic_id_t dsentry_site_id)
{
    struct _magic_sentry *cached_sentry = NULL;
    struct _magic_function *cached_function = NULL;
    ST_CHECK_INIT();

    EXEC_WITH_MAGIC_VARS(
        cached_sentry = magic_sentry_lookup_by_name(parent_name ? parent_name : "", name, dsentry_site_id, NULL);
        if (!cached_sentry) {
            cached_function = magic_function_lookup_by_name(parent_name, name);
        }
        , &st_cached_magic_vars
    );
    if (!cached_sentry && !cached_function) {
        return ENOENT;
    }
    st_set_transfer_status(status_flags, status_op, cached_sentry, cached_function);
    if (cached_sentry && MAGIC_SENTRY_IS_ALLOC(cached_sentry)) {
        struct _magic_dsentry *prev_dsentry, *dsentry, *next_dsentry = MAGIC_DSENTRY_NEXT(MAGIC_DSENTRY_FROM_SENTRY(cached_sentry));
        struct _magic_sentry* sentry;
        /*
         * Alloc sentries may have multiple instances with the same name.
         * Use the site_id to distinguish between them.
         */
        assert(parent_name && name);
        MAGIC_DSENTRY_ALIVE_NAME_ID_ITER(next_dsentry, prev_dsentry, dsentry, sentry,
            parent_name, name, dsentry_site_id,
            st_set_transfer_status(status_flags, status_op, sentry, NULL);
        );
    }
    return OK;
}

PUBLIC int st_set_status_by_function_id(int status_flags, int status_op,
    _magic_id_t id)
{
    struct _magic_function *cached_function = NULL;
    ST_CHECK_INIT();

    EXEC_WITH_MAGIC_VARS(
        cached_function = magic_function_lookup_by_id(id, NULL);
        , &st_cached_magic_vars
    );

    if (!cached_function) {
        return ENOENT;
    }

    st_set_transfer_status(status_flags, status_op, NULL, cached_function);
    return OK;
}

PUBLIC int st_set_status_by_sentry_id(int status_flags, int status_op,
    _magic_id_t id)
{
    struct _magic_sentry *cached_sentry = NULL;
    ST_CHECK_INIT();

    EXEC_WITH_MAGIC_VARS(
        cached_sentry = magic_sentry_lookup_by_id(id, NULL);
        , &st_cached_magic_vars
    );

    if (!cached_sentry) {
        return ENOENT;
    }

    st_set_transfer_status(status_flags, status_op, cached_sentry, NULL);
    return OK;
}

PUBLIC int st_set_status_by_local_addr(int status_flags, int status_op,
    void *addr)
{
    const char *parent_name, *name;
    _magic_id_t dsentry_site_id = MAGIC_DSENTRY_SITE_ID_NULL;
    struct _magic_sentry *sentry = NULL;
    struct _magic_function *function = NULL;
    ST_CHECK_INIT();

    sentry = magic_sentry_lookup_by_addr(addr, NULL);
    if (!sentry) {
        function = magic_function_lookup_by_addr(addr, NULL);
    }
    if (sentry && !MAGIC_STATE_FLAG(sentry, MAGIC_STATE_DYNAMIC)) {
        name = sentry->name;
        parent_name = MAGIC_SENTRY_PARENT(sentry);
        dsentry_site_id = MAGIC_SENTRY_SITE_ID(sentry);
    }
    else if (function && !MAGIC_STATE_FLAG(function, MAGIC_STATE_DYNAMIC)) {
       name = function->name;
       parent_name = MAGIC_FUNCTION_PARENT(function);
    }
    else {
        return ENOENT;
    }
    st_set_status_by_name(status_flags, status_op, parent_name, name, dsentry_site_id);
    return OK;
}

PUBLIC int st_pair_by_function_ids(unsigned long *cached_ids, unsigned long *local_ids, int status_flags, int status_op)
{
    int r, i=0;
    ST_CHECK_INIT();

    while (cached_ids[i] != 0) {
        assert(local_ids[i] != 0);
        r = st_pair_by_function_id(cached_ids[i], local_ids[i], status_flags, status_op);
        if (r != OK) {
            return r;
        }
        i++;
    }
    return OK;
}

PUBLIC int st_pair_by_sentry_ids(unsigned long *cached_ids, unsigned long *local_ids, int status_flags, int status_op)
{
    int r, i=0;
    ST_CHECK_INIT();

    while (cached_ids[i] != 0) {
        assert(local_ids[i] != 0);
        r = st_pair_by_sentry_id(cached_ids[i], local_ids[i], status_flags, status_op);
        if (r != OK) {
            return r;
        }
        i++;
    }
    return OK;
}

PUBLIC int st_pair_by_names(char **cached_parent_names, char **cached_names,
    char **local_parent_names, char **local_names, _magic_id_t *dsentry_site_ids,
    int status_flags, int status_op)
{
    int r, i=0;
    while (cached_names[i] != NULL) {
        assert(local_names[i]);
        r = st_pair_by_name(cached_parent_names ? cached_parent_names[i] : NULL, cached_names[i],
            local_parent_names ? local_parent_names[i] : NULL, local_names[i],
            dsentry_site_ids ? dsentry_site_ids[i] : MAGIC_DSENTRY_SITE_ID_NULL,
            status_flags, status_op);
        if (r != OK) {
            return r;
        }
        i++;
    }
    return OK;
}

PUBLIC void st_pair_by_sentry(void *cached_sentry, void *local_sentry, int status_flags, int status_op)
{
    ST_CHECK_INIT();

    st_add_sentry_pair(cached_sentry, local_sentry);
    if (cached_sentry) {
        st_set_status_by_sentry(status_flags, status_op, cached_sentry);
    }
}

PUBLIC void st_pair_by_function(void *cached_function, void* local_function, int status_flags, int status_op)
{
    ST_CHECK_INIT();

    st_add_function_pair(cached_function, local_function);
    if (cached_function) {
        st_set_status_by_function(status_flags, status_op, cached_function);
    }
}

PUBLIC int st_pair_alloc_by_dsindex(st_init_info_t *info, void *cached_sentry, void *local_dsindex, int num_elements, const union __alloc_flags *p_alloc_flags, int status_flags, int status_op)
{
    int r;
    ST_CHECK_INIT();

    r = st_add_sentry_pair_alloc_by_dsindex(info, cached_sentry, local_dsindex, num_elements, p_alloc_flags);
    if (r != OK) {
        return r;
    }
    if (cached_sentry) {
        st_set_status_by_sentry(status_flags, status_op, cached_sentry);
    }
    return OK;
}

PUBLIC int st_pair_by_function_id(unsigned long cached_id, unsigned long local_id, int status_flags, int status_op)
{
    struct _magic_function *cached_function = NULL, *local_function = NULL;
    ST_CHECK_INIT();
    assert(cached_id || local_id);

    if (cached_id) {
        EXEC_WITH_MAGIC_VARS(
            cached_function = magic_function_lookup_by_id(cached_id, NULL);
            , &st_cached_magic_vars
        );
        if (!cached_function) {
            return ENOENT;
        }
    }
    if (local_id) {
        EXEC_WITH_MAGIC_VARS(
            local_function = magic_function_lookup_by_id(local_id, NULL);
            , st_local_magic_vars_ptr
        );
        if (!local_function) {
            return ENOENT;
        }
    }

    st_pair_by_function(cached_function, local_function, status_flags, status_op);
    return OK;
}

PUBLIC int st_pair_by_sentry_id(unsigned long cached_id, unsigned long local_id, int status_flags, int status_op)
{
    struct _magic_sentry *cached_sentry = NULL, *local_sentry = NULL;
    ST_CHECK_INIT();
    assert(cached_id || local_id);

    if (cached_id) {
        EXEC_WITH_MAGIC_VARS(
            cached_sentry = magic_sentry_lookup_by_id(cached_id, NULL);
            , &st_cached_magic_vars
        );
        if (!cached_sentry) {
            return ENOENT;
        }
    }
    if (local_id) {
        EXEC_WITH_MAGIC_VARS(
            local_sentry = magic_sentry_lookup_by_id(local_id, NULL);
            , st_local_magic_vars_ptr
        );
        if (!local_sentry) {
            return ENOENT;
        }
    }

    st_pair_by_sentry(cached_sentry, local_sentry, status_flags, status_op);
    return OK;
}

PUBLIC int st_pair_by_name(char *cached_parent_name, char *cached_name,
    char *local_parent_name, char *local_name, _magic_id_t dsentry_site_id,
    int status_flags, int status_op)
{
    struct _magic_function *cached_function = NULL, *local_function = NULL;
    struct _magic_sentry *cached_sentry = NULL, *local_sentry = NULL;
    ST_CHECK_INIT();
    assert(cached_name || local_name);

    if (cached_name) {
        EXEC_WITH_MAGIC_VARS(
            cached_sentry = magic_sentry_lookup_by_name(cached_parent_name ? cached_parent_name : "", cached_name, dsentry_site_id, NULL);
            if (cached_sentry && MAGIC_SENTRY_IS_ALLOC(cached_sentry)) {
                return EINVAL;
            }
            if (!cached_sentry) {
                cached_function = magic_function_lookup_by_name(NULL, cached_name);
            }
            , &st_cached_magic_vars
        );
        if (!cached_sentry && !cached_function) {
            return ENOENT;
        }
    }
    if (local_name) {
        EXEC_WITH_MAGIC_VARS(
            if (!cached_function) {
                local_sentry = magic_sentry_lookup_by_name(local_parent_name ? local_parent_name : "", local_name, dsentry_site_id, NULL);
                if (local_sentry && MAGIC_SENTRY_IS_ALLOC(local_sentry)) {
                    return EINVAL;
                }
            }
            if (!cached_sentry && !local_sentry) {
                local_function = magic_function_lookup_by_name(NULL, local_name);
            }
            , st_local_magic_vars_ptr
        );
        if (!local_sentry && !local_function) {
            return ENOENT;
        }
    }
    if (cached_function || local_function) {
        assert(!cached_sentry && !local_sentry);
        st_pair_by_function(cached_function, local_function, status_flags, status_op);
        return OK;
    }
    assert(cached_sentry || local_sentry);
    st_pair_by_sentry(cached_sentry, local_sentry, status_flags, status_op);
    return OK;
}

PUBLIC int st_pair_by_alloc_name_policies(st_init_info_t *info, char *cached_parent_name, char *cached_name, _magic_id_t cached_dsentry_site_id, char *local_parent_name, char *local_name, _magic_id_t local_dsentry_site_id, int num_elements, const union __alloc_flags *p_alloc_flags, int alloc_policies, int status_flags, int status_op)
{
    int r, saved_policies = st_policies;
    st_policies &= ~(ST_ON_ALLOC_UNPAIR_MASK);
    st_policies |= (alloc_policies & ST_ON_ALLOC_UNPAIR_MASK);
    r = st_pair_by_alloc_name(info, cached_parent_name, cached_name, cached_dsentry_site_id, local_parent_name, local_name, local_dsentry_site_id, num_elements, p_alloc_flags, status_flags, status_op);
    st_policies = saved_policies;
    return r;
}

PUBLIC int st_pair_by_alloc_name(st_init_info_t *info, char *cached_parent_name, char *cached_name, _magic_id_t cached_dsentry_site_id, char *local_parent_name, char *local_name, _magic_id_t local_dsentry_site_id, int num_elements, const union __alloc_flags *p_alloc_flags, int status_flags, int status_op)
{
    struct _magic_sentry *cached_sentry = NULL, *local_sentry = NULL;
    struct _magic_dsindex *local_dsindex = NULL;
    struct _magic_dsentry *prev_dsentry, *dsentry, *head_dsentry;
    struct _magic_sentry* sentry;
    int r;
    int is_cached_alloc = FALSE, is_local_alloc = FALSE;
    ST_CHECK_INIT();
    assert(cached_name || local_name);
    assert(!((cached_name == NULL) ^ (cached_parent_name == NULL)));
    assert(!((local_name == NULL) ^ (local_parent_name == NULL)));

    if (cached_name) {
        EXEC_WITH_MAGIC_VARS(
            cached_sentry = magic_sentry_lookup_by_name(cached_parent_name,
                cached_name, cached_dsentry_site_id, NULL);
            if (cached_sentry && MAGIC_SENTRY_IS_ALLOC(cached_sentry)) {
                is_cached_alloc = TRUE;
            }
            , &st_cached_magic_vars
        );
    }
    if (local_name) {
        EXEC_WITH_MAGIC_VARS(
            local_sentry = magic_sentry_lookup_by_name(local_parent_name,
                local_name, local_dsentry_site_id, NULL);
            if (local_sentry && MAGIC_SENTRY_IS_ALLOC(local_sentry)) {
                is_local_alloc = TRUE;
            }
            if (!local_sentry || is_local_alloc) {
                local_dsindex = magic_dsindex_lookup_by_name(local_parent_name, local_name);
                if (local_dsindex && !MAGIC_DSINDEX_IS_ALLOC(local_dsindex)) {
                    local_dsindex = NULL;
                }
                if (local_sentry) assert(local_dsindex);
                is_local_alloc = is_local_alloc || local_dsindex != NULL;
            }
            , st_local_magic_vars_ptr
        );
    }
    if (!is_cached_alloc && !is_local_alloc) {
        if (cached_sentry || local_sentry) {
            st_pair_by_sentry(cached_sentry, local_sentry, status_flags, status_op);
            return OK;
        }
        return ENOENT;
    }
    if (local_sentry) {
        if (!is_local_alloc) {
            /* Alloc sentries may have multiple instances with the same name. */
            assert(cached_sentry && is_cached_alloc);
            head_dsentry = MAGIC_DSENTRY_NEXT(MAGIC_DSENTRY_FROM_SENTRY(cached_sentry));
            assert(cached_parent_name && cached_name);
            MAGIC_DSENTRY_ALIVE_NAME_ID_ITER(head_dsentry, prev_dsentry, dsentry, sentry, cached_parent_name, cached_name, cached_dsentry_site_id,
                /* Cannot map multiple cached alloc sentries to a single local non-alloc sentry. */
                return E2BIG;
            );
            /* Map a single cached alloc sentry to a single local non-alloc sentry. */
            st_pair_by_sentry(cached_sentry, local_sentry, status_flags, status_op);
            return OK;
        }
        else {
            /* Unpair all the local alloc sentries. */
            head_dsentry = MAGIC_DSENTRY_FROM_SENTRY(local_sentry);
            assert(local_parent_name && local_name);
            MAGIC_DSENTRY_ALIVE_NAME_ID_ITER(head_dsentry, prev_dsentry, dsentry, sentry, local_parent_name, local_name, local_dsentry_site_id,
                st_pair_by_sentry(NULL, sentry, status_flags, status_op);
            );
        }
    }
    if (!cached_sentry) {
        return OK;
    }

    /* Map a single cached non-alloc sentry to a local to-be-alloc sentry. */
    if (!is_cached_alloc) {
        assert(local_dsindex);
        return st_pair_alloc_by_dsindex(info, cached_sentry, local_dsindex, num_elements, p_alloc_flags, status_flags, status_op);
    }

    /* Map all the cached alloc sentries to the corresponding local to-be-alloc sentries (or NULL). */
    head_dsentry = MAGIC_DSENTRY_FROM_SENTRY(cached_sentry);
    assert(cached_parent_name && cached_name);
    MAGIC_DSENTRY_ALIVE_NAME_ID_ITER(head_dsentry, prev_dsentry, dsentry,
        sentry, cached_parent_name, cached_name, cached_dsentry_site_id,
        r = st_pair_alloc_by_dsindex(info, sentry, local_dsindex, num_elements, p_alloc_flags, status_flags, status_op);
        if (r != OK) {
            return r;
        }
    );

    return OK;
}

/* Metadata transfer and adjustment functions */

PRIVATE int transfer_metadata_functions(st_init_info_t *info,
    struct _magic_vars_t *cached_magic_vars,
    struct _magic_vars_t *remote_magic_vars,
    st_counterparts_t *counterparts)
{

    int i;
    struct _magic_function *cached_function;

    /* transfer magic_functions */
    MD_TRANSFER(info, remote_magic_vars->functions, (void **)&cached_magic_vars->functions, remote_magic_vars->functions_num * sizeof(struct _magic_function));

    /* adjust magic_functions */
    for (i = 0 ; i < cached_magic_vars->functions_num ; i++) {
        cached_function = &cached_magic_vars->functions[i];
        MD_TRANSFER_STR(info, &cached_function->name);
        cached_function->type = &cached_magic_vars->types[cached_function->type - remote_magic_vars->types];
    }

    return OK;
}

PRIVATE int transfer_metadata_dfunctions(st_init_info_t *info,
    struct _magic_vars_t *cached_magic_vars,
    struct _magic_vars_t *remote_magic_vars,
    st_counterparts_t *counterparts)
{

    struct _magic_dfunction **dfunction_ptr;
    struct _magic_dfunction *cached_dfunction, *prev_dfunction = NULL;
    struct _magic_function *cached_function;

    /* Transfer dfunctions. */
    cached_magic_vars->first_dfunction = remote_magic_vars->first_dfunction;
    dfunction_ptr = &cached_magic_vars->first_dfunction;
    while (*dfunction_ptr != NULL) {
        MD_TRANSFER(info, *dfunction_ptr, (void **)dfunction_ptr, sizeof(struct _magic_dfunction));
        cached_dfunction = *dfunction_ptr;

        /* Adjust dfunction parent_name and next/prev links. */
        if (cached_dfunction->parent_name != NULL) {
            MD_TRANSFER_STR(info, &cached_dfunction->parent_name);
            if (strlen(cached_dfunction->parent_name) == 0) {
                printf("ERROR. strlen(dfunction->parent_name) == 0.\n");
                return EGENERIC;
            }
        } else {
            printf("ERROR. dfunction->parent_name == NULL.\n");
            return EGENERIC;
        }

        /* Adjust function name and type. */
        cached_function = &cached_dfunction->function;
        MD_TRANSFER_STR(info, &cached_function->name);
        cached_function->type = &cached_magic_vars->types[cached_function->type - remote_magic_vars->types];

        if (cached_dfunction->prev != NULL)
            cached_dfunction->prev = prev_dfunction;

        dfunction_ptr = &cached_dfunction->next;
        prev_dfunction = cached_dfunction;
    }

    cached_magic_vars->last_dfunction = prev_dfunction;

    return OK;
}


PUBLIC int st_transfer_metadata_types(st_init_info_t *info, struct _magic_vars_t *cached_magic_vars
    , struct _magic_vars_t *remote_magic_vars, st_counterparts_t *counterparts)
{

    int i;

    /* transfer types */
    MD_TRANSFER(info, remote_magic_vars->types, (void **)&cached_magic_vars->types, remote_magic_vars->types_num * sizeof(struct _magic_type));

    /* type adjustments */
    for (i = 0 ; i < cached_magic_vars->types_num ; i++) {
        if (transfer_metadata_type_members(info, &cached_magic_vars->types[i], cached_magic_vars, remote_magic_vars)) {
            printf("ERROR transferring type members metadata.\n");
            return EGENERIC;
        }
        set_typename_key(&cached_magic_vars->types[i]);
    }

    return OK;
}

PRIVATE int transfer_metadata_type_value_set(st_init_info_t *info, struct _magic_type *type, struct _magic_vars_t *cached_magic_vars, struct _magic_vars_t *remote_magic_vars)
{
    int num_elements;
    /* MD_TRANSFER cannot be used, because it will allocate space for num_elements */
    if (st_cbs_os.copy_state_region(info->info_opaque, (uint32_t) type->value_set, sizeof(int), (uint32_t) &num_elements)) {
        printf("ERROR transferring type value set metadata.\n");
        return EGENERIC;
    }
    num_elements++;
    MD_TRANSFER(info, type->value_set, (void **)&type->value_set, num_elements *sizeof(int));
    return OK;
}

PRIVATE int transfer_metadata_type_members(st_init_info_t *info, struct _magic_type *type, struct _magic_vars_t *cached_magic_vars, struct _magic_vars_t *remote_magic_vars)
{
    int r;
    int num_child = MAGIC_TYPE_NUM_CONTAINED_TYPES(type), i;

    MD_TRANSFER_STR(info, &type->name);
    MD_TRANSFER_STR(info, &type->type_str);

    if (type->names != NULL && type->num_names > 0) {
        /* transfer array of name pointers */
        MD_TRANSFER(info, type->names, (void **)&type->names, type->num_names * sizeof(char *));
        for (i = 0 ; (unsigned int)i < type->num_names ; i++) {
            /* transfer individual name */
            MD_TRANSFER_STR(info, &type->names[i]);
        }
    }


#define MD_TRANSFER_ADJUST_MEMBER_PTR(NUM_ELEMENTS,                            \
    ELEMENT_SIZE,PTR_ARRAY,INDEX)                                              \
    if((NUM_ELEMENTS) > 0 && (PTR_ARRAY) != NULL) {                            \
        MD_TRANSFER(info, PTR_ARRAY, (void **)&PTR_ARRAY,                      \
            NUM_ELEMENTS * ELEMENT_SIZE);                                      \
        for(INDEX = 0 ; (INDEX) < (NUM_ELEMENTS) ; INDEX++) {                  \
            PTR_ARRAY[INDEX] = ADJUST_POINTER(cached_magic_vars->types,        \
                remote_magic_vars->types, PTR_ARRAY[INDEX]);                   \
        }                                                                      \
    }

    MD_TRANSFER_ADJUST_MEMBER_PTR(
        (type->type_id == MAGIC_TYPE_FUNCTION ? num_child + 1 : num_child),
        sizeof(struct _magic_type *), type->contained_types, i
    );

    if (type->compatible_types) {
        struct _magic_type *comp_types_element;
        int comp_types_size=0;
        /* determine size of array */
        do {
            if (st_cbs_os.copy_state_region(info->info_opaque, (uint32_t) &type->compatible_types[comp_types_size]
                , sizeof(struct _magic_type *), (uint32_t) &comp_types_element))
            {
                printf("ERROR transferring compatible types array metadata.\n");
                return EGENERIC;
            }
            comp_types_size++;
        } while(comp_types_element != NULL);
        /* We know the size, now transfer the whole array */
        MD_TRANSFER(info, type->compatible_types, (void **) &type->compatible_types, comp_types_size * sizeof(struct _magic_type *));
        for (i = 0; i < comp_types_size; i++) {
            if (type->compatible_types[i] != NULL) {
                /* Adjust the pointer to point to the local counterpart */
                type->compatible_types[i] = ADJUST_POINTER(cached_magic_vars->types,  remote_magic_vars->types, type->compatible_types[i]);
            }
        }
    }

    if (num_child>0 && type->member_names != NULL) {
        MD_TRANSFER(info, type->member_names, (void **)&type->member_names, num_child * sizeof(char *));
        for (i = 0 ; i < num_child ; i++) {
            MD_TRANSFER_STR(info, &type->member_names[i]);
        }
    }

    if (num_child>0 && type->member_offsets != NULL) {
        MD_TRANSFER(info, type->member_offsets, (void **)&type->member_offsets, num_child * sizeof(unsigned));
    }

    if (MAGIC_TYPE_HAS_VALUE_SET(type)) {
        r = transfer_metadata_type_value_set(info, type, cached_magic_vars, remote_magic_vars);
        if (r != OK) {
            return r;
        }
    }
    return OK;
}

PRIVATE int transfer_metadata_sentries(st_init_info_t *info, struct _magic_vars_t *cached_magic_vars
    , struct _magic_vars_t *remote_magic_vars, st_counterparts_t *counterparts
    , size_t *max_buff_sz)
{

    int i;
    int skipped_sentries = 0;
    struct _magic_sentry *cached_sentry;

    /* transfer sentries */
    MD_TRANSFER(info, remote_magic_vars->sentries, (void **)&cached_magic_vars->sentries, remote_magic_vars->sentries_num * sizeof(struct _magic_sentry));
    /* todo: try to use only remote_magic_vars or cached magic_vars */
    /* todo: if transfer is complete, and argument 2 and 3 are always the same, remove 2nd argument */

    /* adjust sentries */
    for (i = 0 ; i < cached_magic_vars->sentries_num ; i++) {
        cached_sentry = &cached_magic_vars->sentries[i];

        if ((st_policies & ST_TRANSFER_DIRTY_ONLY) &&
            !MAGIC_STATE_FLAG(cached_sentry, MAGIC_STATE_DIRTY_PAGE) &&
            !MAGIC_STATE_FLAG(cached_sentry, MAGIC_STATE_IMMUTABLE)) {
            skipped_sentries++;
            continue;
        }
        if (skipped_sentries > 0) {
            cached_magic_vars->sentries[i - skipped_sentries] =
                cached_magic_vars->sentries[i];
            cached_magic_vars->sentries[i - skipped_sentries].id -=
                skipped_sentries;
            cached_sentry = &cached_magic_vars->sentries[i - skipped_sentries];
        }


        if (transfer_metadata_sentry_members(info, cached_sentry)) {
            printf("ERROR transferring sentry members metadata.\n");
            return EGENERIC;
        }

        /*
         * We have to change the type to its cached counterpart,
         * so that it may be compared to the local type of the local sentry counterpart.
         */
        cached_sentry->type = &cached_magic_vars->types[cached_sentry->type - remote_magic_vars->types];

        if (cached_sentry->type->size > *max_buff_sz) {
            *max_buff_sz = cached_sentry->type->size;
        }
    }

    if (skipped_sentries > 0)
        cached_magic_vars->sentries_num -= skipped_sentries;

    return OK;
}

PRIVATE int transfer_metadata_sentry_members(st_init_info_t *info, struct _magic_sentry *sentry)
{
    if (sentry->name != NULL) {
        MD_TRANSFER_STR(info, &sentry->name);
    } else {
        printf("ERROR. sentry->name == NULL.\n");
        return EGENERIC;
    }
    return OK;
}

PUBLIC int st_transfer_metadata_dsentries(st_init_info_t *info, struct _magic_vars_t *cached_magic_vars
    , struct _magic_vars_t *remote_magic_vars, st_counterparts_t *counterparts, size_t *max_buff_sz, int *dsentries_num)
{

    struct _magic_dsentry **dsentry_ptr;
#if MAGIC_DSENTRY_ALLOW_PREV
    struct _magic_dsentry *prev_dsentry = NULL;
#endif
    int r;

    *dsentries_num = 0;

    cached_magic_vars->first_dsentry = remote_magic_vars->first_dsentry;
    dsentry_ptr = &cached_magic_vars->first_dsentry;
    while (*dsentry_ptr != NULL) {

        struct _magic_dsentry *cached_dsentry, *remote_dsentry = *dsentry_ptr;
        struct _magic_sentry *sentry;

        /* transfer dsentry */
        MD_TRANSFER(info, *dsentry_ptr, (void **) dsentry_ptr, sizeof(struct _magic_dsentry));
        cached_dsentry = *dsentry_ptr;

        if ((st_policies & ST_TRANSFER_DIRTY_ONLY) &&
            !MAGIC_STATE_FLAG((&cached_dsentry->sentry), MAGIC_STATE_DIRTY_PAGE) &&
            !MAGIC_STATE_FLAG((&cached_dsentry->sentry), MAGIC_STATE_IMMUTABLE)) {
            *dsentry_ptr = cached_dsentry->next;
            continue;
        }

        if (cached_magic_vars->first_stack_dsentry == remote_dsentry) {
            cached_magic_vars->first_stack_dsentry = cached_dsentry;
        } else if(cached_magic_vars->last_stack_dsentry == remote_dsentry) {
            cached_magic_vars->last_stack_dsentry = cached_dsentry;
        }

        /* adjust dsentry */
        if (cached_dsentry->parent_name != NULL) {
            MD_TRANSFER_STR(info, &cached_dsentry->parent_name);
            if (strlen(cached_dsentry->parent_name) == 0) {
                printf("ERROR. strlen(dsentry->parent_name) == 0.\n");
#if TODO_DSENTRY_PARENT_NAME_BUG
                if (cached_dsentry->next != NULL)
#endif
                    return EGENERIC;
            }
        } else {
            printf("ERROR. dsentry->parent_name == NULL.\n");
            return EGENERIC;
        }

        sentry = &cached_dsentry->sentry;
        if (transfer_metadata_sentry_members(info, sentry)) {
            printf("ERROR transferring sentry members metadata.\n");
            return EGENERIC;
        }

        /* Override original id to simplify pairing later. */
        sentry->id = cached_magic_vars->sentries_num + *dsentries_num + 1;

        /*
         * Report violations for all the pointers pointing to the initial stack area.
         * This is to make sure no assumption is incorrectly made about this area.
         */
        if (!strcmp(sentry->name, MAGIC_ALLOC_INITIAL_STACK_NAME)) {
            sentry->flags |= MAGIC_STATE_ADDR_NOT_TAKEN;
        }

        /*
         * Adjust the type, so that the local and remote type can be compared
         * during a server version update
         */
        if (sentry->type == &remote_dsentry->type) {

            /*
             * sentry->type is contained in dsentry.type. Therefore, this is an
             * array type. In order to allocate a new memory region, we only
             * need the size of the type, and the contained type as arguments
             * to the magic allocation function. Therefore, other members of
             * the type do need to be cached or adjusted.
             */

            /* Adjust pointer to cached location */
            sentry->type = &cached_dsentry->type;

            /* Adjust contained_types to type_array located in dsentry struct. */
            sentry->type->contained_types = cached_dsentry->type_array;

            /*
             * Adjust only pointer in type_array. It currently has the same
             * value as the remote copy, but it has to point to the cached
             * of the contained type.
             */
            sentry->type->contained_types[0] = &cached_magic_vars->types[sentry->type->contained_types[0] - remote_magic_vars->types];

            /* Adjust empty strings. */
            sentry->type->name = "";
            sentry->type->type_str = "";

            /* Adjust value set if necessary. */
            if (MAGIC_TYPE_HAS_VALUE_SET(sentry->type)) {
                r = transfer_metadata_type_value_set(info, sentry->type, cached_magic_vars, remote_magic_vars);
                if (r != OK) {
                    return r;
                }
            }
        } else {

            /*
             * sentry.type must be in the global type array. Adjust pointer accordingly.
             * The pointer is still pointing to the remote version.
             * We have to change it to the cached version.
             */
            sentry->type = &cached_magic_vars->types[sentry->type - remote_magic_vars->types];

        }

        /* see if the buffer needs to be bigger for the dsentry data region. */
        if (!MAGIC_STATE_FLAG(sentry, MAGIC_STATE_OUT_OF_BAND) && *max_buff_sz < sentry->type->size) {
            *max_buff_sz = sentry->type->size;
        }

        dsentry_ptr = &cached_dsentry->next;
#if MAGIC_DSENTRY_ALLOW_PREV
        if (cached_dsentry->prev != NULL)
            cached_dsentry->prev = prev_dsentry;
        prev_dsentry = cached_dsentry;
#endif
        *dsentries_num = *dsentries_num + 1;
    }

    return OK;
}

PRIVATE int pair_metadata_types(st_init_info_t *info,
    struct _magic_vars_t *cached_magic_vars, st_counterparts_t *counterparts, int allow_unpaired_types)
{
    int i, j, num_unpaired_struct_types = 0;
    int num_unpaired_types = 0;
    int num_total_types = 0;
    int num_struct_types = 0;
    int num_unpaired_types_left, num_unpaired_struct_types_left;

    if (st_unpaired_types_ratio > 0 || st_unpaired_struct_types_ratio > 0) {
        for (i = 0 ; i < cached_magic_vars->types_num ; i++) {
            struct _magic_type *type = &cached_magic_vars->types[i];
            if (ST_IS_UNPAIRABLE_STRUCT_TYPE(type)) {
                num_struct_types++;
            }
            if (ST_IS_UNPAIRABLE_TYPE(type)) {
                num_total_types++;
            }
        }
        num_unpaired_types = (int) (st_unpaired_types_ratio*num_total_types);
        num_unpaired_struct_types = (int) (st_unpaired_struct_types_ratio*num_struct_types);
    }
    num_unpaired_types_left = num_unpaired_types;
    num_unpaired_struct_types_left = num_unpaired_struct_types;

    /* type pairing, remote->local */
    for(i = 0 ; i < cached_magic_vars->types_num ; i++) {
        struct _magic_type *type = &cached_magic_vars->types[i];
        counterparts->types[i].counterpart = NULL;

        if (num_unpaired_types_left > 0 && ST_IS_UNPAIRABLE_TYPE(type)) {
            num_unpaired_types_left--;
            continue;
        }
        else if (num_unpaired_struct_types_left > 0 && ST_IS_UNPAIRABLE_STRUCT_TYPE(type)) {
            num_unpaired_struct_types_left--;
            continue;
        }

        for (j = 0 ; j < _magic_types_num ; j++) {
            /* A remote type may be paired to multiple local types.
             * It is safe to index only the first type since counterparts
             * are only used to speed up type matching.
             */
            if (magic_type_compatible(type, &_magic_types[j], MAGIC_TYPE_COMPARE_ALL)) {
                counterparts->types[i].counterpart = &_magic_types[j];
                break;
            }
        }

        if (!allow_unpaired_types && counterparts->types[i].counterpart == NULL) {
             printf("ERROR, remote type cannot be paired with a local type: ");
             MAGIC_TYPE_PRINT(type, MAGIC_EXPAND_TYPE_STR);
             printf("\n");
             return EGENERIC;
        }
    }
    if (st_unpaired_types_ratio > 0 || st_unpaired_struct_types_ratio > 0) {
        assert(num_unpaired_types_left == 0 && (st_unpaired_types_ratio > 0 || num_unpaired_struct_types == 0));
        _magic_printf("Unpaired types stats: unpaired types: %d, total types: %d, unpaired struct types: %d, struct types: %d\n", num_unpaired_types, num_total_types, num_unpaired_struct_types, num_struct_types);
    }

    for (i = 0 ; i < cached_magic_vars->types_num ; i++) {
        struct _magic_type *type = &cached_magic_vars->types[i];
        struct _magic_type *local_type = (struct _magic_type*) counterparts->types[i].counterpart;
        counterparts->ptr_types[i].counterpart = NULL;
        if (local_type && type->type_id == MAGIC_TYPE_POINTER) {
            if (MAGIC_TYPE_HAS_COMP_TYPES(type) != MAGIC_TYPE_HAS_COMP_TYPES(local_type)) {
                continue;
            }
            if (MAGIC_TYPE_HAS_COMP_TYPES(type)) {
                j = 0;
                while (MAGIC_TYPE_HAS_COMP_TYPE(type, j) && MAGIC_TYPE_HAS_COMP_TYPE(local_type, j)) {
                    struct _magic_type *ctype = MAGIC_TYPE_COMP_TYPE(type, j);
                    struct _magic_type *local_ctype = MAGIC_TYPE_COMP_TYPE(local_type, j);
                    if (!ST_TYPE_IS_CACHED_COUNTERPART(ctype, local_ctype)) {
                        break;
                    }
                    j++;
                }
                if (MAGIC_TYPE_HAS_COMP_TYPE(type, j) || MAGIC_TYPE_HAS_COMP_TYPE(local_type, j)) {
                    continue;
                }
            }
            counterparts->ptr_types[i].counterpart = local_type;
        }
    }

    return OK;
}

PRIVATE int pair_metadata_functions(st_init_info_t *info,
    struct _magic_vars_t *cached_magic_vars, st_counterparts_t *counterparts)
{
    int i;
    struct _magic_function *cached_function, *local_function;
#if ST_DEBUG_LEVEL > 0
    int num_relocated = 0;
#endif

    /* map remote functions to local functions */
    for(i = 0 ; i < cached_magic_vars->functions_num ; i++) {
        cached_function = &cached_magic_vars->functions[i];
        local_function = NULL;
        st_map_functions(&cached_function, &local_function);
        ST_SET_CACHED_COUNTERPART(cached_function, functions, functions, local_function);

#if CHECK_SENTITY_PAIRS
        if (local_function) {
            /* debug: see if the function is paired more than once */
            struct _magic_function *cfunction = NULL;
            st_map_functions(&cfunction, &local_function);
            if (cfunction != cached_function) {
                printf("function pairing failed for (1) local function linked to multiple remote functions (2), (3)\n");
                printf("(1) "); MAGIC_FUNCTION_PRINT(local_function, 0); printf("\n");
                printf("(2) "); MAGIC_FUNCTION_PRINT(cached_function, 0); printf("\n");
                printf("(3) "); MAGIC_FUNCTION_PRINT(cfunction, 0); printf("\n");
                return EGENERIC;
            }
        }
#endif

#if ST_DEBUG_LEVEL > 0
        if (local_function && cached_function->address != local_function->address) {
            num_relocated++;
            if (ST_DEBUG_LEVEL > 1) {
                printf("- relocated function: '%s'\n", cached_magic_vars->functions[i].name);
            }
        }
#endif
    }

#if ST_DEBUG_LEVEL > 0
    printf("total remote functions: %d. relocated: %d\n", cached_magic_vars->functions_num, num_relocated);
#endif

    return OK;
}

PRIVATE int pair_metadata_sentries(st_init_info_t *info,
    struct _magic_vars_t *cached_magic_vars, st_counterparts_t *counterparts)
{
    int i, r;
    struct _magic_sentry *cached_sentry, *local_sentry;
#if ST_DEBUG_LEVEL > 0
    int num_relocated_str = 0, num_relocated_normal = 0;
#endif

    /* pair sentries remote->local */
    for (i = 0 ; i < cached_magic_vars->sentries_num ; i++) {
        void *local_data_addr = NULL;
        cached_sentry = &cached_magic_vars->sentries[i];

        /* String data is transferred directly. */
        if (MAGIC_SENTRY_IS_STRING(cached_sentry)) {
            char *string = st_buff_allocate(info, cached_sentry->type->size);
            if (!string) {
                printf("ERROR allocating string.\n");
                return EGENERIC;
            }
            r = st_cbs_os.copy_state_region(info->info_opaque, (uint32_t) cached_sentry->address,
                 cached_sentry->type->size, (uint32_t) string);
            if(r != OK) {
                printf("ERROR transferring string.\n");
                return EGENERIC;
            }
            local_data_addr = string;
        }
        ST_SET_CACHED_COUNTERPART(cached_sentry, sentries, sentries_data, local_data_addr);

        local_sentry = NULL;
        st_map_sentries(&cached_sentry, &local_sentry);
        ST_SET_CACHED_COUNTERPART(cached_sentry, sentries, sentries, local_sentry);

#if CHECK_SENTITY_PAIRS
        if (local_sentry && !MAGIC_SENTRY_IS_STRING(cached_sentry)) {
            /* debug: see if the non-string sentry is paired more than once */
            struct _magic_sentry *csentry = NULL;
            st_map_sentries(&csentry, &local_sentry);
            if (csentry != cached_sentry) {
                printf("sentry pairing failed for (1) local sentry linked to multiple remote sentries (2), (3)\n");
                printf("(1) "); MAGIC_SENTRY_PRINT(local_sentry, 0); printf("\n");
                printf("(2) "); MAGIC_SENTRY_PRINT(cached_sentry, 0); printf("\n");
                printf("(3) "); MAGIC_SENTRY_PRINT(csentry, 0); printf("\n");
                return EGENERIC;
            }
        }
#endif

#if ST_DEBUG_LEVEL > 0
        if (local_sentry && cached_sentry->address != local_sentry->address) {
            if (MAGIC_SENTRY_IS_STRING(cached_sentry)) {
                num_relocated_str++;
            }
            else {
                num_relocated_normal++;
                if (ST_DEBUG_LEVEL > 1) {
                    printf("- relocated non-string sentry: '%s'\n", cached_sentry->name);
                }
            }
        }
#endif
    }

#if ST_DEBUG_LEVEL > 0
    printf("total remote sentries: %d. relocated normal: %d relocated string: %d\n", cached_magic_vars->sentries_num, num_relocated_normal, num_relocated_str);
#endif

    return OK;
}

#if ST_ASSUME_RAW_COPY_BEFORE_TRANSFER
PRIVATE int allocate_pair_metadata_dsentries_from_raw_copy(st_init_info_t *info,
    struct _magic_vars_t *cached_magic_vars, st_counterparts_t *counterparts)
{
    struct _magic_dsentry *dsentry;
    int remote_dsentries = 0, unpaired_dsentries = 0;

#if ST_DEBUG_LEVEL > 3
    EXEC_WITH_MAGIC_VARS(
        magic_print_dsentries();
        , &st_cached_magic_vars
    );
    magic_print_dsentries();
#endif

    dsentry = cached_magic_vars->first_dsentry;
    while (dsentry != NULL) {
        struct _magic_sentry *local_sentry = NULL, *sentry = MAGIC_DSENTRY_TO_SENTRY(dsentry);

        /* Initialize counterpart to NULL. */
        ST_SET_CACHED_COUNTERPART(sentry, sentries, sentries, NULL);

        remote_dsentries++;

        if (!MAGIC_STATE_FLAG(sentry, MAGIC_STATE_STACK) && !MAGIC_STATE_FLAG(sentry, MAGIC_STATE_LIB)) {
            local_sentry = MAGIC_DSENTRY_TO_SENTRY((struct _magic_dsentry *)MAGIC_PTR_FROM_DATA(sentry->address));
        } else {
#if MAGIC_LOOKUP_SENTRY_ALLOW_RANGE_INDEX
            EXEC_WITH_MAGIC_VARS(
                local_sentry = magic_sentry_lookup_by_range(sentry->address, NULL);
                , &st_cached_magic_vars
            );
#else
            local_sentry = magic_sentry_lookup_by_addr(sentry->address, NULL);
#endif
        }

        if (!local_sentry) {
             unpaired_dsentries++;
#if ST_DEBUG_LEVEL > 2
             printf("allocate_pair_metadata_dsentries_from_raw_copy: found unpaired "); MAGIC_DSENTRY_PRINT(dsentry, MAGIC_EXPAND_TYPE_STR); _magic_printf("\n");
#endif
        }
        ST_SET_CACHED_COUNTERPART(sentry, sentries, sentries, local_sentry);
        dsentry = dsentry->next;
    }

#if ST_DEBUG_LEVEL > 0
    printf("total remote dsentries: %d (%d unpaired)\n", remote_dsentries, unpaired_dsentries);
#endif

    return OK;
}

#else

PRIVATE int allocate_pair_metadata_dsentries(st_init_info_t *info,
    struct _magic_vars_t *cached_magic_vars, st_counterparts_t *counterparts)
{
    struct _magic_dsentry *dsentry = cached_magic_vars->first_dsentry, *local_dsentry;
    int remote_dsentries = 0;
#ifndef __MINIX
    int *local_sentry_paired_by_id = st_buff_allocate(info, (_magic_sentries_next_id + 1) * sizeof(int));
#endif

#if ST_DEBUG_LEVEL > 3
    EXEC_WITH_MAGIC_VARS(
        magic_print_dsentries();
        , &st_cached_magic_vars
    );
    magic_print_dsentries();
#endif

#ifdef __MINIX
    /*
     * Since on MINIX the mmaped regions are inherited in the new process,
     * we must first deallocate them. This is not the case on Linux.
     */
    while (dsentry != NULL) {
        int res = 0;
        struct _magic_sentry *sentry = MAGIC_DSENTRY_TO_SENTRY(dsentry);
        int size = sentry->type->size;
        /* For mmap first unmap the old region that is already mapped into this new instance */
        if (!MAGIC_STATE_FLAG(sentry, MAGIC_STATE_OUT_OF_BAND)
                && MAGIC_STATE_REGION(sentry) == MAGIC_STATE_MAP
                && !USE_PRE_ALLOCATED_BUFFER(info)
           )
            {
            /*
             * The 'ext' field in the dsentry is used here to record
             * the padding for ASR.
             */
            size_t padding = (size_t) dsentry->ext;
            /*
             * call munmap(). ptr and size have to be altered,
             * in order to free the preceding page, containing the dsentry struct, too.
             */
            MAGIC_MEM_WRAPPER_BLOCK(
                res = munmap((char *)sentry->address - magic_get_sys_pagesize(), size + magic_get_sys_pagesize() + padding);
            );
            if (res != 0) {
                printf("ERROR, munmap returned NULL.\n");
                return EGENERIC;
            }
        }
        dsentry = dsentry->next;
    }
#endif

    /* Permute dsentries in case of ASR. */
    if (info->flags & ST_LU_ASR) {
        magic_asr_permute_dsentries(&cached_magic_vars->first_dsentry);
    }

    dsentry = cached_magic_vars->first_dsentry;
    while (dsentry != NULL) {
        struct _magic_sentry *local_sentry, *sentry = MAGIC_DSENTRY_TO_SENTRY(dsentry);
        int is_alloc_dsentry = MAGIC_SENTRY_IS_ALLOC(sentry);
        int res = 0;
        struct _magic_dsindex *local_dsindex;

        remote_dsentries++;

#ifdef __MINIX
        /* Cannot deal with dead dsentries. */
        assert(dsentry->magic_state == MAGIC_DSENTRY_MSTATE_ALIVE);
#else
        /*
         * If there are dead dsentries, we simply skip them.
         */
        if (dsentry->magic_state != MAGIC_DSENTRY_MSTATE_ALIVE) {
            dsentry = dsentry->next;
            continue;
        }
#endif

        /* Initialize counterpart to NULL. */
        ST_SET_CACHED_COUNTERPART(sentry, sentries, sentries, NULL);

        /* Handle non-alloc dsentries first. */
        if (!is_alloc_dsentry) {
            local_sentry = magic_sentry_lookup_by_name(dsentry->parent_name,
                sentry->name, dsentry->site_id, NULL);
            if (local_sentry) {
                assert(!MAGIC_SENTRY_IS_ALLOC(local_sentry));
                ST_SET_CACHED_COUNTERPART(sentry, sentries, sentries, local_sentry);
            }

            dsentry = dsentry->next;
            continue;
        }

        /* Out-of-band alloc dsentries next. */
        if (MAGIC_STATE_FLAG(sentry, MAGIC_STATE_OUT_OF_BAND)) {
            struct _magic_type *type;
            /* We can only handle obdsentries with the magic void type, transferred as-is. */
            if (sentry->type != &dsentry->type) {
                /* Not an array type */
                type = sentry->type;
            } else {
                /* This is an array type, use its contained type instead. */
                type = sentry->type->contained_types[0];
            }
            /* We now have the cached version of the type. Compare it to magic void type */
            if (!magic_type_compatible(type, MAGIC_VOID_TYPE, MAGIC_TYPE_COMPARE_ALL)) {
                printf("Can't handle obdsentry with non-void type\n");
                return EGENERIC;
            }
#ifdef __MINIX
            /* On MINIX we need to recreate all the obdsentries. */
            struct _magic_obdsentry *obdsentry;
            int size = sentry->type->size;
            obdsentry = magic_create_obdsentry(sentry->address,
                MAGIC_VOID_TYPE, size, MAGIC_STATE_REGION(sentry), sentry->name, dsentry->parent_name);
            if (obdsentry == NULL) {
                printf("ERROR, magic_create_obdsentry returned NULL.\n");
                return EGENERIC;
            }
            local_dsentry = MAGIC_OBDSENTRY_TO_DSENTRY(obdsentry);
#else
            /* On Linux we only need to pair them. */
            local_sentry = magic_sentry_lookup_by_name(
                MAGIC_SENTRY_PARENT(sentry), sentry->name,
                MAGIC_SENTRY_SITE_ID(sentry), NULL);
            if (local_sentry == NULL) {
                printf("Unable to pair obdsentry.\n");
                return EGENERIC;
            }
            local_dsentry = MAGIC_DSENTRY_FROM_SENTRY(local_sentry);
#endif
            ST_SET_CACHED_COUNTERPART(sentry, sentries, sentries, MAGIC_DSENTRY_TO_SENTRY(local_dsentry));
            dsentry = dsentry->next;
            continue;
        }

        /* Handle regular alloc dsentries last. */
#ifndef __MINIX
        /*
         * For Linux, first pair INIT time remote
         * dsentries with local dsentries.
         */

        if (MAGIC_STATE_FLAG(sentry, MAGIC_STATE_INIT)) {
            local_sentry = NULL;

            if (MAGIC_STATE_FLAG(sentry, MAGIC_STATE_IMMUTABLE)) {
                /*
                 * Immutable init time dsentries should have already been
                 * preallocated, so just pair them by address.
                 */
                local_sentry = magic_sentry_lookup_by_addr(sentry->address, NULL);
            } else {
#if MAGIC_LOOKUP_SENTRY_ALLOW_NAME_HASH
                struct _magic_sentry_list *local_sentry_list;
                local_sentry_list = magic_sentry_list_lookup_by_name_hash(
                    dsentry->parent_name, sentry->name, dsentry->site_id, NULL);

                while (local_sentry_list) {
                    if (!local_sentry_paired_by_id[local_sentry_list->sentry->id]) {
                        local_sentry = local_sentry_list->sentry;
                        break;
                    }
                    local_sentry_list = local_sentry_list->next;
                }

#else
                do {
                    struct _magic_dsentry *prev_dsentry, *tmp_dsentry;
                    struct _magic_sentry *tmp_sentry;
                    MAGIC_DSENTRY_LOCK();
                    MAGIC_DSENTRY_ALIVE_ITER(_magic_first_dsentry, prev_dsentry,
                        tmp_dsentry, tmp_sentry,
                        if (!strcmp(tmp_sentry->name, sentry->name)) {
                            if (!dsentry->parent_name ||
                                    !strcmp(MAGIC_SENTRY_PARENT(tmp_sentry), dsentry->parent_name)) {
                                if (dsentry->site_id == MAGIC_DSENTRY_SITE_ID_NULL ||
                                        tmp_dsentry->site_id == dsentry->site_id) {
                                    if (!local_sentry_paired_by_id[tmp_sentry->id]) {
                                        local_sentry = tmp_sentry;
                                        break;
                                    }
                                }
                            }
                        }
                    );
                    MAGIC_DSENTRY_UNLOCK();
                } while (0);
#endif
            }
            if (local_sentry) {
                ST_SET_CACHED_COUNTERPART(sentry, sentries, sentries, local_sentry);
                local_sentry_paired_by_id[local_sentry->id] = 1;
                dsentry = dsentry->next;
                continue;
            }
        }
#endif

        /*
         * Just recreate all the other dsentries. Immutable objects will
         * have already been inherited and allocate_local_dsentry() will
         * not reallocate them, but instead it will just create a new
         * local dsentry in the right place.
         */
        local_dsindex = magic_dsindex_lookup_by_name(dsentry->parent_name, sentry->name);
        if (local_dsindex || MAGIC_SENTRY_IS_LIB_ALLOC(sentry)) {

            /* Allocate a new local dsentry and pair it with the remote. */
            res = allocate_local_dsentry(info, local_dsindex, 0, 0, NULL, &local_dsentry, dsentry, NULL);
            if (res != ENOSYS) {
                if (res != OK) {
                    return res;
                }
                assert(local_dsentry);
                ST_SET_CACHED_COUNTERPART(sentry, sentries, sentries, MAGIC_DSENTRY_TO_SENTRY(local_dsentry));
            }
        }
        dsentry = dsentry->next;
    }

#if ST_DEBUG_LEVEL > 0
    printf("total remote dsentries: %d\n", remote_dsentries);
#endif

    return OK;
}

PRIVATE int deallocate_nonxferred_dsentries(struct _magic_dsentry *first_dsentry, st_counterparts_t *counterparts)
{
    struct _magic_dsentry *dsentry = first_dsentry;
    struct _magic_sentry *local_sentry;

    while (dsentry != NULL) {
        struct _magic_sentry *sentry = MAGIC_DSENTRY_TO_SENTRY(dsentry);
        int is_paired_dsentry = ST_HAS_CACHED_COUNTERPART(sentry, sentries);
        int is_alloc_dsentry = MAGIC_SENTRY_IS_ALLOC(sentry);
        ST_GET_CACHED_COUNTERPART(sentry, sentries, sentries, local_sentry);

        if (MAGIC_STATE_EXTF_GET(sentry, ST_TRANSFER_DONE) || !is_alloc_dsentry) {
            dsentry = dsentry->next;
            continue;
        }

        /* Report non-transferred alloc dsentries when requested. */
        if (is_paired_dsentry && (st_policies & ST_REPORT_NONXFERRED_ALLOCS)) {
            printf("deallocate_nonxferred_dsentries: Non-transferred dsentry found: ");
            MAGIC_DSENTRY_PRINT(dsentry, MAGIC_EXPAND_TYPE_STR);
            printf("\n");
        }
        if (!is_paired_dsentry && (st_policies & ST_REPORT_NONXFERRED_UNPAIRED_ALLOCS)) {
            printf("deallocate_nonxferred_dsentries: Non-transferred unpaired dsentry found: ");
            MAGIC_DSENTRY_PRINT(dsentry, MAGIC_EXPAND_TYPE_STR);
            printf("\n");
        }

        if (!is_paired_dsentry) {
            dsentry = dsentry->next;
            continue;
        }
        assert(local_sentry);
        if (MAGIC_SENTRY_IS_ALLOC(local_sentry)) {
            deallocate_local_dsentry(MAGIC_DSENTRY_FROM_SENTRY(local_sentry));
        }
        dsentry = dsentry->next;
    }

    return OK;
}
#endif

PRIVATE void deallocate_local_dsentry(struct _magic_dsentry *local_dsentry)
{
    int r, dsentry_type;
    struct _magic_sentry *local_sentry = MAGIC_DSENTRY_TO_SENTRY(local_dsentry);

    assert(MAGIC_SENTRY_IS_ALLOC(local_sentry));
    dsentry_type = MAGIC_STATE_FLAG(local_sentry, MAGIC_STATE_OUT_OF_BAND) ? MAGIC_STATE_OUT_OF_BAND : MAGIC_STATE_REGION(local_sentry);
    /* A MAP_SHARED region will have both MAGIC_STATE_MAP and MAGIC_STATE_SHM. */
    if (dsentry_type == (MAGIC_STATE_MAP | MAGIC_STATE_SHM))
        dsentry_type = MAGIC_STATE_MAP;

    MAGIC_MEM_WRAPPER_BEGIN();
    switch (dsentry_type) {
        case MAGIC_STATE_HEAP:
            /* free */
            magic_free(local_sentry->address);
            break;

        case MAGIC_STATE_MAP:
            /* munmap */
            r = magic_munmap(local_sentry->address, local_sentry->type->size);
            if (r != 0) {
                printf("Warning: magic_munmap failed for ");
                MAGIC_DSENTRY_PRINT(local_dsentry, 0);
                printf("\n");
            }
            break;

#ifndef __MINIX
        case MAGIC_STATE_SHM:
            /* shmdt */
            r = magic_shmdt(local_sentry->address);
            if (r != 0) {
                printf("Warning: magic_shmdt failed for ");
                MAGIC_DSENTRY_PRINT(local_dsentry, 0);
                printf("\n");
            }
            break;
#endif

        case MAGIC_STATE_OUT_OF_BAND:
            /* out-of-band dsentry. */
            r = magic_destroy_obdsentry_by_addr(local_sentry->address);
            if (r != 0) {
                printf("Warning: magic_destroy_obdsentry_by_addr failed for ");
                MAGIC_DSENTRY_PRINT(local_dsentry, 0);
                printf("\n");
            }
            break;

        default:
            st_cbs_os.panic("ERROR. UNSUPPORTED DSENTRY TYPE: %d\n", dsentry_type);
    }
    MAGIC_MEM_WRAPPER_END();
}

PRIVATE int allocate_local_dsentry(st_init_info_t *info, struct _magic_dsindex *local_dsindex, int num_elements, int is_type_mismatch, const union __alloc_flags *p_alloc_flags, struct _magic_dsentry** local_dsentry_ptr, struct _magic_dsentry *cached_dsentry, void *ptr)
{
    struct _magic_dsentry *local_dsentry = NULL;
    struct _magic_sentry *cached_sentry = NULL;
    const char *name, *parent_name;
    struct _magic_type *type;
    int region;
    size_t size;
    union __alloc_flags alloc_flags;

    /* Either a dsindex or a dsentry needs to be set. */
    assert(local_dsindex || cached_dsentry);

    if (cached_dsentry)
        cached_sentry = MAGIC_DSENTRY_TO_SENTRY(cached_dsentry);

    /* name, parent_name: local_dsindex || cached_dsentry. */
    if (local_dsindex) {
        assert(MAGIC_DSINDEX_IS_ALLOC(local_dsindex));
        name = local_dsindex->name;
        parent_name = local_dsindex->parent_name;
    } else {
        assert(MAGIC_SENTRY_IS_ALLOC(cached_sentry));
        /*
         * The external allocation parent_name needs to be readjusted.
         * The external allocation name is adjusted after the new dsentry
         * is created.
         */
        name = cached_sentry->name;
        if (!strcmp(cached_dsentry->parent_name, MAGIC_ALLOC_EXT_PARENT_NAME)) {
            parent_name = MAGIC_ALLOC_EXT_PARENT_NAME;
        } else {
            int found_parent_name = 0;
            struct _magic_sodesc *sodesc;
            struct _magic_dsodesc *dsodesc;
            MAGIC_DSODESC_LOCK();
            MAGIC_SODESC_ITER(_magic_first_sodesc, sodesc,
                if (!strcmp(cached_dsentry->parent_name, sodesc->lib.name)) {
                    parent_name = (const char *)sodesc->lib.name;
                    found_parent_name = 1;
                    break;
                }
            );
            if (!found_parent_name) {
                MAGIC_DSODESC_ITER(_magic_first_dsodesc, dsodesc,
                    if (!strcmp(cached_dsentry->parent_name, dsodesc->lib.name)) {
                        parent_name = (const char *)dsodesc->lib.name;
                        found_parent_name = 1;
                        break;
                    }
                );
            }
            MAGIC_DSODESC_UNLOCK();
            assert(found_parent_name && "Invalid parent name for cached dsentry!");
        }
    }

    /* num_elements: args || cached_sentry. */
    if (num_elements <= 0 && cached_sentry) {
        num_elements = cached_sentry->type->type_id == MAGIC_TYPE_ARRAY ?
            cached_sentry->type->num_child_types : 1;
    }
    assert(num_elements > 0);

    /* alloc_flags: args || cached_dsentry. */
    if (!p_alloc_flags) {
        if (cached_dsentry && MAGIC_SENTRY_IS_ALLOC(cached_sentry)) {
            alloc_flags = cached_dsentry->alloc_flags;
        }
    } else {
        alloc_flags = *p_alloc_flags;
    }

    /* is_type_mismatch: args || cached_dsentry. */
    if (!is_type_mismatch && cached_dsentry)
        is_type_mismatch = MAGIC_STATE_FLAG(cached_sentry, MAGIC_STATE_TYPE_SIZE_MISMATCH);

    /*
     * Use old address for immutable objects.
     */
    /* ptr: args || cached_sentry. */
    if (!ptr && cached_sentry &&
            MAGIC_STATE_FLAG(cached_sentry, MAGIC_STATE_IMMUTABLE))
        ptr = cached_sentry->address;

    /* region: local_dsindex || cached_sentry. */
    if (local_dsindex)
        region = MAGIC_STATE_REGION(local_dsindex);
    else
        region = MAGIC_STATE_REGION(cached_sentry);

    /* Check if the region is ambigous. This shouldn't happen. */
    assert(!((region & (MAGIC_STATE_HEAP | MAGIC_STATE_MAP)) ==
        (MAGIC_STATE_HEAP | MAGIC_STATE_MAP)) &&
        "MAGIC_STATE_HEAP | MAGIC_STATE_MAP detected!");
#if 0
    if ((region & (MAGIC_STATE_HEAP | MAGIC_STATE_MAP)) ==
        (MAGIC_STATE_HEAP | MAGIC_STATE_MAP)) {
        /* Check call flags to determine what to do in the ambiguous cases. */
        region = (alloc_flags.mmap_flags && alloc_flags.mmap_prot) ?
            MAGIC_STATE_MAP : MAGIC_STATE_HEAP;
    }
#endif

    /* type: local_dsindex || cached_sentry. */
    if (local_dsindex) {
        type = local_dsindex->type;

        if (num_elements > 1 && MAGIC_TYPE_FLAG(local_dsindex->type, MAGIC_TYPE_VARSIZE)) {
            size = magic_type_alloc_get_varsized_array_size(local_dsindex->type, num_elements);
            assert(size > 0);
        } else {
            if (is_type_mismatch) {
                type = MAGIC_VOID_TYPE;
                printf("WARNING: Type size mismatch dsentry detected! Ignoring dsindex type and reverting to MAGIC_TYPE_VOID.\n");
                printf("name=%s, parent_name=%s\n", local_dsindex->name, local_dsindex->parent_name);
            }
            size = num_elements * type->size;
        }
    } else {
        /*
         * The type will need adjusting later.
         */
        type = cached_sentry->type;
        size = type->size;
    }

    *local_dsentry_ptr = NULL;

    if (region & MAGIC_STATE_HEAP) {
        /* malloc */
        ptr = magic_malloc_positioned(type, name, parent_name, size, (ptr == NULL ? NULL : MAGIC_PTR_FROM_DATA(ptr)));
        if (ptr == NULL) {
            printf("ERROR, magic_malloc_positioned returned NULL.\n");
            return ENOMEM;
        }
        memset(ptr, 0, size);
        local_dsentry = MAGIC_PTR_TO_DSENTRY(MAGIC_PTR_FROM_DATA(ptr));
    }
    else if (region & MAGIC_STATE_MAP) {
        /* mmap */
        if (!alloc_flags.mmap_flags || !alloc_flags.mmap_prot) {
            /* We need call_flags to perform mmap. */
            return ENOSYS;
        }
        ptr = persistent_mmap(type, name, parent_name, info, NULL, size,
            alloc_flags.mmap_prot, alloc_flags.mmap_flags, -1, 0, ptr);
        if (ptr == NULL) {
            printf("ERROR, persistent_mmap returned NULL.\n");
            return ENOMEM;
        }
        if (!(alloc_flags.mmap_flags & MAP_SHARED))
            memset(ptr, 0, size);
        local_dsentry = MAGIC_PTR_TO_DSENTRY(MAGIC_PTR_FROM_DATA(ptr));
    }
#ifndef __MINIX
    else if (region & MAGIC_STATE_SHM) {
        /* shmat */
        if (!alloc_flags.shmat_flags || !alloc_flags.shmat_shmid) {
            /* We need call_flags to perform shmat. */
            return ENOSYS;
        }
        ptr = magic_shmat(type, name, parent_name, alloc_flags.shmat_shmid,
            ptr, alloc_flags.shmat_flags);
        if (ptr == NULL) {
            printf("ERROR, magic_shmat returned NULL.\n");
            return ENOMEM;
        }
        local_dsentry = MAGIC_PTR_TO_DSENTRY(MAGIC_PTR_FROM_DATA(ptr));
    }
#endif
    else {
        if (local_dsindex) {
            printf("ERROR. UNSUPPORTED DSINDEX TYPE: ");
            MAGIC_DSINDEX_PRINT(local_dsindex, MAGIC_EXPAND_TYPE_STR);
        } else {
            printf("ERROR. UNSUPPORTED DSENTRY: ");
            MAGIC_DSENTRY_PRINT(cached_dsentry, MAGIC_EXPAND_TYPE_STR);
        }
        printf("\n");
        return EINVAL;
    }

    if (!local_dsindex) {
        /*
         * This was an externally allocated type and, as such, needs adjusting.
         */
        assert(cached_sentry->type == &cached_dsentry->type);
        local_dsentry->type = cached_dsentry->type;
        if (cached_dsentry->type_array[0]->type_id == MAGIC_TYPE_POINTER) {
            ST_GET_CACHED_COUNTERPART(cached_dsentry->type_array[0], types, ptr_types, local_dsentry->type_array[0]);
        } else {
            ST_GET_CACHED_COUNTERPART(cached_dsentry->type_array[0], types, types, local_dsentry->type_array[0]);
        }
        local_dsentry->sentry.type = &local_dsentry->type;
        local_dsentry->sentry.type->contained_types = local_dsentry->type_array;
    }

    assert(local_dsentry);
    assert(local_dsentry->parent_name && strcmp(local_dsentry->parent_name, ""));
    assert(local_dsentry->sentry.name && strcmp(local_dsentry->sentry.name, ""));
    assert(magic_check_dsentry(local_dsentry, 0));
    *local_dsentry_ptr = local_dsentry;

    if (is_type_mismatch)
        local_dsentry->sentry.flags |= MAGIC_STATE_TYPE_SIZE_MISMATCH;

    /*
     * Dsentries allocated by shared libraries have the names stored in dsentry
     * buffers (for now).
     * Readjust the local_sentry to do this as well, since after state transfer
     * cleanup the existing names will become invalid.
     */
    if (!local_dsindex && MAGIC_SENTRY_IS_LIB_ALLOC(cached_sentry)) {
        strncpy(local_dsentry->name_ext_buff, local_dsentry->sentry.name,
            MAGIC_DSENTRY_EXT_NAME_BUFF_SIZE);
        local_dsentry->sentry.name = local_dsentry->name_ext_buff;
    }

    return OK;
}

PRIVATE int check_unpaired_sentry(st_init_info_t *info,
    struct _magic_sentry* cached_sentry)
{
    int sentry_needs_transfer = MAGIC_STATE_EXTF_GET(cached_sentry, ST_NEEDS_TRANSFER | ST_TRANSFER_DONE) == ST_NEEDS_TRANSFER;
    int report;

    if (!sentry_needs_transfer && !MAGIC_SENTRY_IS_STRING(cached_sentry)) {
        return OK;
    }

    if (MAGIC_STATE_FLAG(cached_sentry, MAGIC_STATE_DYNAMIC)) {
        report = st_policies & ST_REPORT_UNPAIRED_DSENTRIES;
    }
    else if(MAGIC_SENTRY_IS_STRING(cached_sentry)) {
        report = st_policies & ST_REPORT_UNPAIRED_STRINGS;
    }
    else {
        report = st_policies & ST_REPORT_UNPAIRED_SENTRIES;
    }
    if (report) {
        printf("check_unpaired_sentry: Unpaired sentry found: ");
        ST_SENTRY_PRINT(cached_sentry,MAGIC_EXPAND_TYPE_STR);
        printf("\n");
    }

    return OK;
}

PUBLIC struct _magic_sentry* st_cached_to_remote_sentry(st_init_info_t *info, struct _magic_sentry *cached_sentry)
{
    struct _magic_sentry *remote_sentry;
    void *local_data_addr;
    ST_CHECK_INIT();

    /* Copy metadata into metadata buffer. */
    if (MAGIC_STATE_FLAG(cached_sentry, MAGIC_STATE_DYNAMIC)) {
        magic_copy_dsentry(MAGIC_DSENTRY_FROM_SENTRY(cached_sentry), st_dsentry_buff);
        remote_sentry = MAGIC_DSENTRY_TO_SENTRY(st_dsentry_buff);
    }
    else {
        memcpy(&st_dsentry_buff->sentry, cached_sentry, sizeof(struct _magic_sentry));
        remote_sentry = &st_dsentry_buff->sentry;
    }

    /* Have the remote sentry point to local data. */
    local_data_addr = NULL;
    /* See if we have the data locally already first. */
    ST_GET_CACHED_COUNTERPART(cached_sentry, sentries, sentries_data, local_data_addr);
    if (!local_data_addr) {
        /* Copy remote data into local data buffer. */
        if (st_cbs_os.copy_state_region(info->info_opaque, (uint32_t) remote_sentry->address
                , remote_sentry->type->size, (uint32_t) st_data_buff))
        {
            printf("ERROR transferring sentry data to local buffer.\n");
            return NULL;
        }
        local_data_addr = st_data_buff;
    }
    remote_sentry->address = local_data_addr;

    return remote_sentry;
}

PRIVATE int transfer_data_sentry(st_init_info_t *info,
    struct _magic_sentry* cached_sentry)
{

    int r;
    int st_cb_flags = ST_CB_DEFAULT_FLAGS;
    struct _magic_sentry *local_sentry, *remote_sentry;
    int flags = ST_SEL_ANALYZE_FLAGS;
    struct st_cb_info cb_info_buff;
    struct st_cb_info *cb_info = &cb_info_buff;
    static _magic_selement_t magic_local_selements[MAGIC_MAX_RECURSIVE_TYPES+1];
    static int magic_flags_by_depth[MAGIC_MAX_RECURSIVE_TYPES+1];

    /* Skip extern weak symbols. */
    if (!cached_sentry->address) {
        assert(MAGIC_STATE_FLAG(cached_sentry, MAGIC_STATE_EXTERNAL));
        st_set_transfer_status(ST_TRANSFER_DONE, ST_OP_ADD, cached_sentry, NULL);
        return OK;
    }

    /* Determine local and remote sentries from the cached version. */
    local_sentry = NULL;
    st_lookup_sentry_pair(&cached_sentry, &local_sentry);
    assert(local_sentry && "Unexpected unpaired sentry!");
    remote_sentry = st_cached_to_remote_sentry(info, cached_sentry);
    if (!remote_sentry) {
        printf("No remote sentry found for cached sentry: ");
        MAGIC_SENTRY_PRINT(cached_sentry, 0);
        printf("\n");
        return EFAULT;
    }

    cb_info->local_selements = magic_local_selements;
    cb_info->local_selement = magic_selement_from_sentry(local_sentry, &magic_local_selements[0]);
    cb_info->walk_flags = MAGIC_TYPE_WALK_DEFAULT_FLAGS;
    cb_info->st_cb_flags = st_cb_flags;
    cb_info->init_info = info;
    cb_info->st_cb_saved_flags = magic_flags_by_depth;
    magic_flags_by_depth[0] = st_cb_flags;

    EXEC_WITH_MAGIC_VARS(
        r = magic_sentry_analyze(remote_sentry , flags, transfer_data_selement, cb_info, NULL);
        , &st_cached_magic_vars
    );
    if (r < 0) {
        return r;
    }

    st_set_transfer_status(ST_TRANSFER_DONE, ST_OP_ADD, cached_sentry, NULL);
    return OK;
}

PRIVATE int transfer_data_selement(_magic_selement_t *selement, _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats, void *cb_args)
{

    int r = ST_CB_NOT_PROCESSED;
    int depth, cb_flags;
    struct st_cb_info *cb_info = (struct st_cb_info *) cb_args;
    _magic_selement_t *local_selement, *local_parent_selement;
    st_cb_selement_transfer_t *cb;

    register_typenames_and_callbacks();

    if (!ST_CB_FLAG(ST_CB_CHECK_ONLY)) {
        depth = selement->depth;
        local_selement = &cb_info->local_selements[depth];
        if (depth > 0) {
            local_parent_selement = &cb_info->local_selements[depth-1];
            local_selement->sentry = local_parent_selement->sentry;
            local_selement->parent_type = local_parent_selement->type;
            local_selement->parent_address = local_parent_selement->address;
            cb_info->st_cb_flags = cb_info->st_cb_saved_flags[depth-1];
        }
        /* Map the cached and the local selement. */
        st_map_selement(selement, local_selement, cb_info, FALSE);
        if (local_selement->type == NULL) {
            /* Unpaired selement. */
            if (st_policies & ST_REPORT_UNPAIRED_SELEMENTS) {
                printf("transfer_data_selement: Unpaired selement found: ");
                MAGIC_SELEMENT_PRINT(selement, MAGIC_EXPAND_TYPE_STR);
                printf("\n");
            }
            return MAGIC_SENTRY_ANALYZE_SKIP_PATH;
        }
        cb_info->local_selement = local_selement;

        /* See if identity transfer has been requested. */
        if (cb_info->st_cb_flags & ST_CB_FORCE_IXFER) {
            r = transfer_identity_sel_cb(selement, sel_analyzed, sel_stats, cb_info);
            assert(r != ST_CB_NOT_PROCESSED);
            cb_info->st_cb_saved_flags[depth] = cb_info->st_cb_flags;
            return r;
        }
    }

    cb_flags = ST_CB_TYPE_SELEMENT;
    if (ST_TYPE_NAME_KEY(selement->type) != NULL) {
        cb_flags |= ST_CB_TYPE_TYPENAME;
    }
    if (selement->num == 1) {
        cb_flags |= ST_CB_TYPE_SENTRY;
    }

    cb = st_cbs.st_cb_selement_transfer[cb_flags];
    while (TRUE) {

        if (*cb != NULL) {
            r = (*cb)(selement, sel_analyzed, sel_stats, cb_info);
        } else {
            r = default_transfer_selement_sel_cb(selement, sel_analyzed, sel_stats, cb_info);
            assert(r != ST_CB_NOT_PROCESSED
                && "Default selement callback should always process the selement.");
        }

        if (r != ST_CB_NOT_PROCESSED) {
            assert((r<0 || MAGIC_SENTRY_ANALYZE_IS_VALID_RET(r)) && "Invalid callback return code!");
            if (!ST_CB_FLAG(ST_CB_CHECK_ONLY)) {
                cb_info->st_cb_saved_flags[depth] = cb_info->st_cb_flags;
            }
            return r;
        }

        cb++;
    }

    /* Not reachable. */
    return EINTR;
}

PRIVATE int lookup_trg_info(_magic_selement_t *selement,
    _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats, struct st_cb_info *cb_info,
    _magic_selement_t *cached_trg_selement, _magic_selement_t *local_trg_selement)
{
    _magic_selement_t *local_selement, *trg_selement;
    struct _magic_sentry *cached_trg_sentry, *local_trg_sentry = NULL;
    struct _magic_function *cached_trg_function, *local_trg_function = NULL;
    _magic_sel_analyzed_t local_sel_analyzed;
    _magic_sel_stats_t local_sel_stats;
    void *local_trg_root_address;
    struct _magic_type *cached_trg_root_type, *local_trg_root_type;
    int first_legal_trg_type, is_same_type, is_same_trg_type, local_trg_has_addr_not_taken;

    local_selement = cb_info->local_selement;
    first_legal_trg_type = sel_analyzed->u.ptr.first_legal_trg_type;
    assert(first_legal_trg_type >= 0);
    trg_selement = &sel_analyzed->u.ptr.trg_selements[first_legal_trg_type];
    local_trg_root_type = NULL;

    /* Lookup cached and local targets. */
    if (MAGIC_SEL_ANALYZED_PTR_HAS_TRG_SENTRY(sel_analyzed)) {
        cached_trg_sentry = trg_selement->sentry;
        local_trg_sentry = NULL;
        st_lookup_sentry_pair(&cached_trg_sentry, &local_trg_sentry);
        *cached_trg_selement = *trg_selement;
        cached_trg_root_type = cached_trg_sentry->type;
        local_trg_has_addr_not_taken = local_trg_sentry && MAGIC_STATE_FLAG(local_trg_sentry, MAGIC_STATE_ADDR_NOT_TAKEN);
        local_trg_selement->sentry = local_trg_sentry;
        if (local_trg_sentry) {
            local_trg_root_address = local_trg_sentry->address;
            local_trg_root_type = local_trg_sentry->type;
        }
    }
    else if(MAGIC_SEL_ANALYZED_PTR_HAS_TRG_FUNCTION(sel_analyzed)) {
        cached_trg_function = MAGIC_DFUNCTION_TO_FUNCTION(&sel_analyzed->u.ptr.trg.dfunction);
        local_trg_function = NULL;
        st_lookup_function_pair(&cached_trg_function, &local_trg_function);
        *cached_trg_selement = *trg_selement;
        cached_trg_root_type = cached_trg_function->type;
        local_trg_has_addr_not_taken = local_trg_function && MAGIC_STATE_FLAG(local_trg_function, MAGIC_STATE_ADDR_NOT_TAKEN);
        local_trg_selement->sentry = NULL;
        if (local_trg_function) {
            local_trg_root_address = local_trg_function->address;
            local_trg_root_type = local_trg_function->type;
        }
    }

    /* Check unpaired targets. */
    if (!local_trg_root_type) {
        local_trg_selement->type = NULL;
        return OK;
    }

    /* Check address not taken violations. */
    if (local_trg_has_addr_not_taken) {
        ST_CB_PRINT(ST_CB_ERR, "uncaught ptr with paired target whose address is not taken", selement, sel_analyzed, sel_stats, cb_info);
        return EFAULT;
    }

    /* Check types and return immediately in case of perfect pointer match. */
    is_same_type = selement->type == local_selement->type || ST_PTR_TYPE_IS_CACHED_COUNTERPART(selement->type, local_selement->type);
    is_same_trg_type = ST_TYPE_IS_CACHED_COUNTERPART(cached_trg_root_type, local_trg_root_type);
    if (is_same_type && is_same_trg_type) {
        local_trg_selement->type = cached_trg_selement->type;
        local_trg_selement->address = (char*) local_trg_root_address + sel_analyzed->u.ptr.trg_offset;
        return OK;
    }
#if CHECK_ASR && !FORCE_SOME_UNPAIRED_TYPES
    if (cb_info->init_info->flags & ST_LU_ASR) {
        st_cbs_os.panic("ASR should never get here!");
    }
#endif

    /* Map sel_analyzed to its local counterpart. */
    if (is_same_trg_type) {
        local_sel_analyzed = *sel_analyzed;
        local_sel_analyzed.u.ptr.trg_selements[0].address = (char*) local_trg_root_address + sel_analyzed->u.ptr.trg_offset;
    }
    else {
        st_map_sel_analyzed_from_target(sel_analyzed, &local_sel_analyzed, local_trg_sentry, local_trg_function, cb_info);
        if (local_sel_analyzed.u.ptr.num_trg_types == 0) {
            /* Unpaired target selements. */
            local_trg_selement->type = NULL;
            return OK;
        }
    }

    /* Check violations on the local target. */
    memset(&local_sel_stats, 0, sizeof(local_sel_stats));
    magic_selement_analyze_ptr_type_invs(local_selement, &local_sel_analyzed, &local_sel_stats);
    if (MAGIC_SEL_STATS_HAS_VIOLATIONS(&local_sel_stats)) {
        /* Local pointer with violations found */
        ST_CB_PRINT(ST_CB_ERR, "uncaught ptr with after-transfer violations", selement, sel_analyzed, sel_stats, cb_info);
        ST_CB_PRINT(ST_CB_ERR, "transferred ptr with violations", local_selement, &local_sel_analyzed, &local_sel_stats, cb_info);
        return EFAULT;
    }

    /* All the targets mapped correctly. */
    local_trg_selement->type = local_sel_analyzed.u.ptr.trg_selements[0].type;
    local_trg_selement->address = local_sel_analyzed.u.ptr.trg_selements[0].address;
    return OK;
}

/* transfer helper functions */

PRIVATE int md_transfer_str(st_init_info_t *info, char **str_pt)
{
    char buff[ST_STR_BUFF_SIZE + 2];

    if (st_cbs_os.copy_state_region(info->info_opaque, (uint32_t) *str_pt, ST_STR_BUFF_SIZE + 1, (uint32_t) buff)) {
        st_cbs_os.panic("md_transfer_str(): ERROR transferring string.\n");
        return EGENERIC;
    }
    buff[ST_STR_BUFF_SIZE + 1] = '\0';
    if (strlen(buff) > ST_STR_BUFF_SIZE) {
        st_cbs_os.panic("md_transfer_str(): transferred string has a wrong size: %d\n", strlen(buff));
        return EGENERIC;
    }

    *str_pt = st_buff_allocate(info, strlen(buff) + 1);
    if (!*str_pt) {
        st_cbs_os.panic("md_transfer_str(): string buffer could not be allocated.\n");
        return EGENERIC;
    }
    strcpy(*str_pt, buff);
    return OK;
}

PRIVATE int md_transfer(st_init_info_t *info, void *from, void **to, int len)
{
    /* backup from value, in case &from == to */
    void *from_backup = from;
    *to = st_buff_allocate(info, len);
    if (!*to) {
        st_cbs_os.panic("md_transfer(): buffer could not be allocated.\n");
        return EGENERIC;
    }
    if (st_cbs_os.copy_state_region(info->info_opaque, (uint32_t) from_backup, len, (uint32_t) *to)) {
        st_cbs_os.panic("md_transfer(): ERROR transferring remote data to buffer.\n");
        return EGENERIC;
    }
    return OK;
}


/* Buffer allocation */

PRIVATE void *persistent_mmap(__MA_ARGS__ st_init_info_t *info, void *start, size_t length, int prot, int flags, int fd, off_t offset, struct _magic_dsentry *dsentry) {
    if (USE_PRE_ALLOCATED_BUFFER(info)) {
        size_t alloc_length = length + (length % magic_get_sys_pagesize() == 0 ? 0 : magic_get_sys_pagesize() - (length % magic_get_sys_pagesize()));
        char *ptr, *data_ptr;

        assert(((char *)info->init_buff_cleanup_start) + alloc_length + magic_get_sys_pagesize() <= st_pre_allocated_page_pt && "mmap region hits temporary buffer.");
        assert(((char *)info->init_buff_cleanup_start) + alloc_length + magic_get_sys_pagesize() <= ((char *) info->init_buff_start) + info->init_buff_len && "mmap region hits end of pre-allocated buffer");

        ptr = ((char *)info->init_buff_cleanup_start) + magic_get_sys_pagesize() - MAGIC_SIZE_TO_REAL(0);
        data_ptr = magic_alloc(__MA_VALUES__ ptr, alloc_length, (int) MAGIC_STATE_MAP);
        MAGIC_PTR_TO_DSENTRY(MAGIC_PTR_FROM_DATA(data_ptr))->alloc_mmap_flags = flags;
        MAGIC_PTR_TO_DSENTRY(MAGIC_PTR_FROM_DATA(data_ptr))->alloc_mmap_prot = prot;
        info->init_buff_cleanup_start = &data_ptr[alloc_length];
        return data_ptr;
    } else {
        /* no pre-allocated mmap buffer. Call magic_mmap to allocate region. */
        return magic_mmap_positioned(type, name, parent_name
            , NULL, length, prot, flags, -1, 0, dsentry);
    }
}

PUBLIC void *st_cb_pages_allocate(st_init_info_t *info, uint32_t *phys, int num_pages)
{
    void *result;
    int len = num_pages * magic_get_sys_pagesize();

    if (USE_PRE_ALLOCATED_BUFFER(info)) {
        if (!st_pre_allocated_page_pt) {
#if ST_DEBUG_LEVEL > 0
            printf("st_pages_allocate: initializing pre-allocated page buffer.\n");
#endif
            st_pre_allocated_page_pt = &((char *)info->init_buff_start)[info->init_buff_len];
        }
        st_pre_allocated_page_pt -= len;
        assert(st_pre_allocated_page_pt >= (char *)info->init_buff_cleanup_start
            && "Temporary buffer ran into perminently pre-allocated mmapped pages.");
        return st_pre_allocated_page_pt;
    }

    result = st_cbs_os.alloc_contig(len, 0, NULL);
    if (result == NULL) {
        printf("st_pages_allocate: alloc_contig(%d) failed.\n", len);
        return NULL;
    }

    *phys = (uint32_t) NULL; /* we don't know or need the physical address in order to free */

    return result;
}

PUBLIC void st_cb_pages_free(st_init_info_t *info, st_alloc_pages *current_page)
{
    st_alloc_pages *to_be_freed;
    int result;

    if (USE_PRE_ALLOCATED_BUFFER(info)) {
        /* nothing to do */
        return;
    }

    while (current_page != NULL) {
        to_be_freed = current_page;
        current_page = current_page->previous;

        result = st_cbs_os.free_contig(to_be_freed->virt_addr, to_be_freed->num_pages * magic_get_sys_pagesize());

        if (result != OK) {
            printf("munmap result != ok, using free()\n");
            /*
             * NOTE: in case this is moved out of a magic_* module it needs to be
             * manually annotated so it doesn't get instrumented.
             */
            free(to_be_freed->virt_addr);
        }

    }

}

PUBLIC void *st_buff_allocate(st_init_info_t *info, size_t size)
{
    void *result;

    if (size > st_alloc_buff_available) {

        int pagesize = magic_get_sys_pagesize();
        uint32_t phys;
        st_alloc_pages *buff_previous_page = st_alloc_pages_current;

        /* calculate number of pages needed */
        int pages_needed = (size + sizeof(st_alloc_pages)) / pagesize;
        if ((size + sizeof(st_alloc_pages)) % pagesize)
            pages_needed++;

        /* allocate pages */
        st_alloc_pages_current
            = st_cbs.st_cb_pages_allocate(info, &phys, pages_needed);

        if (!st_alloc_pages_current) {
            printf("Could not allocate buffer.\n");
            return NULL;
        }

        /* set allocation struct */
        st_alloc_pages_current->virt_addr = st_alloc_pages_current;
        st_alloc_pages_current->phys_addr = phys;
        st_alloc_pages_current->num_pages = pages_needed;
        st_alloc_pages_current->previous = buff_previous_page;

        /* requested space is right after the struct */
        st_alloc_buff_pt = (char *) st_alloc_pages_current;
        st_alloc_buff_pt += sizeof(st_alloc_pages);
        /* subtract the struct size from the available buffer */
        st_alloc_buff_available = pages_needed * pagesize - sizeof(st_alloc_pages);

    }

    /* return current buffer pointer */
    result = st_alloc_buff_pt;
    /* set buffer pointer after space that is requested, ready for next allocation */
    st_alloc_buff_pt += size;
    /* adjust available space */
    st_alloc_buff_available -= size;

    return result;

}

PUBLIC void st_buff_cleanup(st_init_info_t *info)
{
    st_cbs.st_cb_pages_free(info, st_alloc_pages_current);
    st_alloc_pages_current = NULL;
    st_alloc_buff_available = 0;
    st_alloc_buff_pt = NULL;
}

PUBLIC void st_cleanup(st_init_info_t *info)
{

#if MAGIC_LOOKUP_SENTRY_ALLOW_RANGE_INDEX
    st_cleanup_rl_index(info, &st_cached_magic_vars);
    st_cleanup_rl_index(info, _magic_vars);
#endif

#if MAGIC_LOOKUP_SENTRY_ALLOW_NAME_HASH
    st_cleanup_sentry_hash(info, &st_cached_magic_vars);
    st_cleanup_sentry_hash(info, _magic_vars);
#endif

#if MAGIC_LOOKUP_FUNCTION_ALLOW_ADDR_HASH
    st_cleanup_function_hash(info, &st_cached_magic_vars);
    st_cleanup_function_hash(info, _magic_vars);
#endif

#if !ST_ASSUME_RAW_COPY_BEFORE_TRANSFER
    assert(
        deallocate_nonxferred_dsentries(st_cached_magic_vars.first_dsentry,
            &st_counterparts) == OK &&
            "ERROR occurred during call to deallocate_nonxferred_dsentries().");
#endif

    /*
     * Free all temporary allocated memory.
     */
    st_buff_cleanup(info);

    /*
     * Reset all values in case of successive state transfers.
     */
    st_init_done = FALSE;
    st_pre_allocated_page_pt = NULL;
    st_dsentry_buff = NULL;
    st_data_buff = NULL;
    st_num_type_transformations = 0;
    st_local_magic_vars_ptr = &_magic_vars_buff;
    st_policies = ST_POLICIES_DEFAULT;
    st_unpaired_types_ratio = ST_UNPAIRED_TYPES_RATIO_DEFAULT;
    st_unpaired_struct_types_ratio = ST_UNPAIRED_STRUCT_TYPES_RATIO_DEFAULT;

    /* Reallow mempool dsentries lookups. */
    magic_lookup_nested_dsentries = 1;
}

/* State cleanup/checking functions. */

/*===========================================================================*
 *                         do_st_before_receive                              *
 *===========================================================================*/
PUBLIC void do_st_before_receive()
{
/* Handle State transfer before receive events. */
  int num_violations;

  assert(st_state_checking_before_receive_is_enabled());

  num_violations = st_do_state_checking();
  if (__st_before_receive_sc_max_cycles < LONG_MAX) {
      __st_before_receive_sc_max_cycles--;
  }
  if (__st_before_receive_sc_max_violations < LONG_MAX) {
      __st_before_receive_sc_max_violations -= num_violations;
  }
  if (__st_before_receive_sc_max_cycles <= 0) {
      st_state_checking_before_receive_set_enabled(0, 0, 0);
      printf("Maximum number of cycles reached\n");
  }
  if (__st_before_receive_sc_max_violations <= 0) {
      st_state_checking_before_receive_set_enabled(0, 0, 0);
      printf("Maximum number of violations reached\n");
  }
}

/*===========================================================================*
 *                st_state_checking_before_receive_is_enabled                *
 *===========================================================================*/
PUBLIC int st_state_checking_before_receive_is_enabled()
{
    return __st_before_receive_enabled;
}

/*===========================================================================*
 *               st_state_checking_before_receive_set_enabled                *
 *===========================================================================*/
PUBLIC int st_state_checking_before_receive_set_enabled(int enabled,
    int max_cycles, int max_violations)
{
    int was_enabled = __st_before_receive_enabled;
    __st_before_receive_enabled = enabled;
    if (enabled) {
        if (max_cycles <= 0) {
            max_cycles = ST_STATE_CHECKING_DEFAULT_MAX_CYCLES;
        }
        if (max_violations <= 0) {
            max_violations = ST_STATE_CHECKING_DEFAULT_MAX_VIOLATIONS;
        }
        __st_before_receive_sc_max_cycles = max_cycles;
        __st_before_receive_sc_max_violations = max_violations;
        printf("Continuous state checking enabled, max cycles=%d, max violations=%d\n",
            max_cycles == LONG_MAX ? 0 : max_cycles,
            max_violations == LONG_MAX ? 0 : max_violations);
    }
    else {
        printf("Continuous state checking disabled\n");
    }
    return was_enabled;
}

/*===========================================================================*
 *                         st_cb_state_checking_wrapper                      *
 *===========================================================================*/
PRIVATE int st_cb_state_checking_wrapper(_magic_selement_t* selement,
    _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats,
    void* cb_args)
{
    struct st_cb_info cb_info_buff;
    struct st_cb_info *cb_info = &cb_info_buff;
    int *num_violations = (int*) cb_args;
    int ret;

    cb_info->local_selements = NULL;
    cb_info->local_selement = NULL;
    cb_info->walk_flags = MAGIC_TYPE_WALK_DEFAULT_FLAGS;
    cb_info->st_cb_flags = ST_CB_CHECK_ONLY;
    cb_info->st_cb_saved_flags = NULL;
    cb_info->init_info = NULL;

    ret = transfer_data_selement(selement, sel_analyzed, sel_stats, cb_info);
    if (ret < 0) {
        ret = st_cbs.st_cb_state_checking(selement, sel_analyzed, sel_stats, cb_args);
        (*num_violations)++;
    }
    return ret;
}

/*===========================================================================*
 *                         st_do_state_checking                              *
 *===========================================================================*/
PUBLIC int st_do_state_checking()
{
    int num_violations = 0;
    magic_sentries_analyze(ST_SEL_ANALYZE_FLAGS,
        st_cb_state_checking_wrapper, &num_violations, NULL);
    return num_violations;
}

/*===========================================================================*
 *                         st_cb_state_checking_null                         *
 *===========================================================================*/
PUBLIC int st_cb_state_checking_null(_magic_selement_t* selement,
    _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats,
    void* cb_args)
{
    return EINTR;
}

/*===========================================================================*
 *                         st_cb_state_checking_print                        *
 *===========================================================================*/
PUBLIC int st_cb_state_checking_print(_magic_selement_t* selement,
    _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats,
    void* cb_args)
{
    printf("%s. Found state violation:\n", st_cbs_os.debug_header());
    magic_sentry_print_el_cb(selement, sel_analyzed, sel_stats, cb_args);
    return MAGIC_SENTRY_ANALYZE_SKIP_PATH;
}

/*===========================================================================*
 *                         st_cb_state_checking_panic                        *
 *===========================================================================*/
PUBLIC int st_cb_state_checking_panic(_magic_selement_t* selement,
    _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats,
    void* cb_args)
{
    st_cb_state_checking_print(selement, sel_analyzed, sel_stats, cb_args);
    st_cbs_os.panic("Time to panic...");
    return MAGIC_SENTRY_ANALYZE_STOP;
}

/*===========================================================================*
 *                         st_do_state_cleanup                               *
 *===========================================================================*/
PUBLIC int st_do_state_cleanup()
{
    return st_cbs.st_cb_state_cleanup();
}

/*===========================================================================*
 *                         st_cb_state_cleanup_null                          *
 *===========================================================================*/
PUBLIC int st_cb_state_cleanup_null() {
    return OK;
}

#ifndef __MINIX
/*===========================================================================*
 *                        st_msync_all_shm_dsentries                         *
 *===========================================================================*/
PUBLIC void st_msync_all_shm_dsentries(void) {
    struct _magic_dsentry *prev_dsentry, *dsentry;
    struct _magic_sentry *sentry;
    MAGIC_DSENTRY_ALIVE_ITER(_magic_first_dsentry, prev_dsentry,
        dsentry, sentry,

        /*
         * TODO:
         * - Don't msync mmaps of /dev/zero
         */
        if (MAGIC_STATE_FLAGS(sentry, MAGIC_STATE_SHM | MAGIC_STATE_MAP) &&
            !(dsentry->alloc_mmap_flags & MAP_ANONYMOUS))
            msync(MAGIC_PTR_TO_DATA(dsentry), sentry->type->size,
                MS_SYNC | MS_INVALIDATE);

    );
}
#endif

