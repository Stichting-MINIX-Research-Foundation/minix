#ifndef ST_CALLBACK_H
#define ST_CALLBACK_H

#include <st/state_transfer.h>

#define NUM_CB_ARRAYS     8
#define MAX_NUM_CBS       20

/* Struct for holding info for selement transfer callbacks. */
struct st_cb_info {
    _magic_selement_t *local_selement;
    _magic_selement_t *local_selements;
    int walk_flags;
    int st_cb_flags;
    int *st_cb_saved_flags;
    st_init_info_t *init_info;
};

/* Callback type definitions and call registration helpers. */

#define CALLBACK_PREFIX st
#undef CALLBACK_FAMILY
#include <st/cb_template.h>

DEFINE_DECL_CALLBACK(void *, pages_allocate, (st_init_info_t *info, uint32_t *phys, int num_pages));
DEFINE_DECL_CALLBACK(void, pages_free, (st_init_info_t *info, st_alloc_pages *current_page));
DEFINE_DECL_CALLBACK(int, state_cleanup, (void));
typedef magic_sentry_analyze_cb_t st_cb_state_checking_t;
PUBLIC  void st_setcb_state_checking(st_cb_state_checking_t cb);
DEFINE_DECL_CALLBACK(void, selement_map, (_magic_selement_t *cached_selement, _magic_selement_t *local_selement, struct st_cb_info *cb_info, int is_trg_mapping));
DEFINE_DECL_CALLBACK_CUSTOM(int, selement_transfer, (_magic_selement_t *selement, _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats, struct st_cb_info *cb_info), (CALLBACK_TYPENAME(selement_transfer) cb, int flags));

/* Struct for holding state transfer callback functions. */
struct st_cbs_t {
    st_cb_pages_allocate_t    st_cb_pages_allocate;
    st_cb_pages_free_t        st_cb_pages_free;
    st_cb_state_cleanup_t     st_cb_state_cleanup;
    st_cb_state_checking_t    st_cb_state_checking;
    st_cb_selement_map_t      st_cb_selement_map;
    st_cb_selement_transfer_t st_cb_selement_transfer[NUM_CB_ARRAYS][MAX_NUM_CBS];
};

/* Predefined callback implementations. */
PUBLIC void *st_cb_pages_allocate(st_init_info_t *info, uint32_t *phys, int num_pages);
PUBLIC void st_cb_pages_free(st_init_info_t *info, st_alloc_pages *current_page);
PUBLIC int st_cb_state_cleanup_null(void);
PUBLIC int st_cb_state_checking_null(_magic_selement_t* selement,
    _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats,
    void* cb_args);
PUBLIC int st_cb_state_checking_print(_magic_selement_t* selement,
    _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats,
    void* cb_args);
PUBLIC int st_cb_state_checking_panic(_magic_selement_t* selement,
    _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats,
    void* cb_args);

PUBLIC int st_cb_transfer_sentry_default(_magic_selement_t *selement, _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats, struct st_cb_info *cb_info);
PUBLIC int st_cb_transfer_typename_default(_magic_selement_t *selement, _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats, struct st_cb_info *cb_info);
PUBLIC int st_cb_transfer_walkable(_magic_selement_t *selement, _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats, struct st_cb_info *cb_info);
PUBLIC int st_cb_transfer_ptr(_magic_selement_t *selement, _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats, struct st_cb_info *cb_info);
PUBLIC int st_cb_transfer_identity(_magic_selement_t *selement, _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats, struct st_cb_info *cb_info);
PUBLIC int st_cb_transfer_cond_identity(_magic_selement_t *selement, _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats, struct st_cb_info *cb_info);
PUBLIC int st_cb_transfer_nonptr(_magic_selement_t *selement, _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats, struct st_cb_info *cb_info);
PUBLIC int st_cb_transfer_struct(_magic_selement_t *selement, _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats, struct st_cb_info *cb_info);
PUBLIC int st_cb_transfer_selement_base(_magic_selement_t *selement, _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats, struct st_cb_info *cb_info);
PUBLIC int st_cb_transfer_selement_generic(_magic_selement_t *selement, _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats, struct st_cb_info *cb_info);

PUBLIC void st_cb_map_from_parent_array_selement_generic(_magic_selement_t *cached_selement, _magic_selement_t *local_selement, struct st_cb_info *cb_info, int is_trg_mapping);
PUBLIC void st_cb_map_child_array_selement_generic(_magic_selement_t *cached_selement, _magic_selement_t *local_selement, struct st_cb_info *cb_info, int is_trg_mapping);
PUBLIC void st_cb_map_from_parent_union_selement_generic(_magic_selement_t *cached_selement, _magic_selement_t *local_selement, struct st_cb_info *cb_info, int is_trg_mapping);
PUBLIC void st_cb_map_child_union_selement_generic(_magic_selement_t *cached_selement, _magic_selement_t *local_selement, struct st_cb_info *cb_info, int is_trg_mapping);
PUBLIC void st_cb_map_from_parent_struct_selement_generic(_magic_selement_t *cached_selement, _magic_selement_t *local_selement, struct st_cb_info *cb_info, int is_trg_mapping);
PUBLIC void st_cb_map_child_nonaggr_selement_generic(_magic_selement_t *cached_selement, _magic_selement_t *local_selement, struct st_cb_info *cb_info, int is_trg_mapping);
PUBLIC void st_cb_map_child_struct_selement_generic(_magic_selement_t *cached_selement, _magic_selement_t *local_selement, struct st_cb_info *cb_info, int is_trg_mapping);
PUBLIC void st_cb_map_child_primitive_selement_generic(_magic_selement_t *cached_selement, _magic_selement_t *local_selement, struct st_cb_info *cb_info, int is_trg_mapping);
PUBLIC void st_cb_map_child_ptr_selement_generic(_magic_selement_t *cached_selement, _magic_selement_t *local_selement, struct st_cb_info *cb_info, int is_trg_mapping);
PUBLIC void st_cb_map_from_parent_selement_generic(_magic_selement_t *cached_selement, _magic_selement_t *local_selement, struct st_cb_info *cb_info, int is_trg_mapping);
PUBLIC void st_cb_map_child_selement_generic(_magic_selement_t *cached_selement, _magic_selement_t *local_selement, struct st_cb_info *cb_info, int is_trg_mapping);
PUBLIC void st_cb_map_selement_generic(_magic_selement_t *cached_selement, _magic_selement_t *local_selement, struct st_cb_info *cb_info, int is_trg_mapping);

/* Macros for callbacks not defined statically. */
#define ST_CB_SELEMENT_TRANSFER_EMPTY  {{0}}

/* Macros for predefined callback implementations. */
#define ST_CB_PAGES_ALLOCATE_DEFAULT   st_cb_pages_allocate
#define ST_CB_PAGES_FREE_DEFAULT       st_cb_pages_free
#define ST_CB_STATE_CLEANUP_DEFAULT    st_cb_state_cleanup_null
#define ST_CB_STATE_CHECKING_DEFAULT   st_cb_state_checking_print
#define ST_CB_SELEMENT_MAP_DEFAULT     st_cb_map_selement_generic

#define ST_CB_PRINT st_cb_print

#define ST_TYPE_NAME_KEY(TYPE) ((const char *) (TYPE)->ext)

#define ST_TYPE_NAME_MATCH(REGISTERED_TYPE_NAME_KEY, KEY)                      \
    (REGISTERED_TYPE_NAME_KEY == KEY)
#define ST_TYPE_NAME_MATCH_ANY(REGISTERED_TYPE_NAME_KEYS, KEY)                 \
    st_type_name_match_any(REGISTERED_TYPE_NAME_KEYS, KEY)
#define ST_SENTRY_NAME_MATCH(SENTRY_WILDCARD_NAME, NAME)                       \
    (!st_strcmp_wildcard(SENTRY_WILDCARD_NAME, NAME))
#define ST_SENTRY_NAME_MATCH_ANY(SENTRY_WILDCARD_NAMES, NAME)                  \
    st_sentry_name_match_any(SENTRY_WILDCARD_NAMES, NAME)
#define ST_DSENTRY_PARENT_NAME_MATCH(DSENTRY_WILDCARD_PARENT_NAME, PARENT_NAME)\
    (!st_strcmp_wildcard(DSENTRY_WILDCARD_PARENT_NAME, PARENT_NAME))
#define ST_DSENTRY_PARENT_NAME_MATCH_ANY(DSENTRY_WILDCARD_PARENT_NAMES,        \
    PARENT_NAME)                                                               \
    st_dsentry_parent_name_match_any(DSENTRY_WILDCARD_PARENT_NAMES, PARENT_NAME)

#define ST_CB_TYPE_TYPENAME 1
#define ST_CB_TYPE_SENTRY   2
#define ST_CB_TYPE_SELEMENT 4
#define ST_CB_NOT_PROCESSED 1000

#define ST_CB_CHECK_ONLY    0x01
#define ST_CB_PRINT_DBG     0x02
#define ST_CB_PRINT_ERR     0x04
#define ST_CB_FORCE_IXFER   0x08
#define ST_CB_DEFAULT_FLAGS (ST_CB_PRINT_ERR)
#define ST_CB_FLAG(F) (cb_info->st_cb_flags & F)

#define ST_CB_DBG ST_CB_PRINT_DBG
#define ST_CB_ERR ST_CB_PRINT_ERR
#define ST_CB_PRINT_LEVEL(L) ST_CB_FLAG(L)
#define ST_CB_LEVEL_TO_STR(L)               \
    (L == ST_CB_DBG ? "DEBUG" :             \
     L == ST_CB_ERR ? "ERROR" : "???")

#endif /* ST_CALLBACK_H */
