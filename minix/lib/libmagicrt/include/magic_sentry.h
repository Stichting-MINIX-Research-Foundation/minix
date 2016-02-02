#ifndef _MAGIC_SENTRY_H
#define _MAGIC_SENTRY_H

#include <magic.h>
#include <magic_def.h>
#include <magic_common.h>
#include <magic_structs.h>
#include <common/ut/utlist.h>

/* Magic state entry macros. */
#define MAGIC_SENTRY_SITE_ID(E)                                                \
    (MAGIC_STATE_FLAG(E, MAGIC_STATE_DYNAMIC) ?                                \
    MAGIC_DSENTRY_FROM_SENTRY(E)->site_id : MAGIC_DSENTRY_SITE_ID_NULL)
#define MAGIC_SENTRY_PARENT(E)                                                 \
    (MAGIC_STATE_FLAG(E, MAGIC_STATE_DYNAMIC) ?                                \
    MAGIC_DSENTRY_FROM_SENTRY(E)->parent_name : "")
#define MAGIC_SENTRY_ID(E) ((E)->id)
#define MAGIC_SENTRY_IS_STRING(E) MAGIC_STATE_FLAG(E,MAGIC_STATE_STRING)
#define MAGIC_SENTRY_IS_NAMED_STRING(E)                                        \
    MAGIC_STATE_FLAG(E,MAGIC_STATE_NAMED_STRING)
#define MAGIC_SENTRY_IS_DSENTRY(E) MAGIC_STATE_FLAG(E,MAGIC_STATE_DYNAMIC)
/* XXX: Be careful when negating the following macros! */
#define MAGIC_SENTRY_IS_ALLOC(E)                                               \
    (MAGIC_SENTRY_IS_DSENTRY(E) && !MAGIC_STATE_FLAG(E,MAGIC_STATE_STACK))
#define MAGIC_SENTRY_IS_EXT_ALLOC(E)                                           \
    (MAGIC_SENTRY_IS_ALLOC(E) && !strcmp((E)->name, MAGIC_ALLOC_EXT_NAME) &&   \
    !strcmp(MAGIC_DSENTRY_FROM_SENTRY(E)->parent_name,                         \
    MAGIC_ALLOC_EXT_PARENT_NAME))
#define MAGIC_SENTRY_IS_LIB_ALLOC(E)                                           \
    (MAGIC_SENTRY_IS_ALLOC(E) &&                                               \
    !strncmp((E)->name, MAGIC_ALLOC_EXT_NAME, strlen(MAGIC_ALLOC_EXT_NAME)) && \
    strlen((E)->name) > strlen(MAGIC_ALLOC_EXT_NAME))
#define MAGIC_SENTRY_PRINT(E, EXPAND_TYPE_STR) do {                            \
        _magic_printf("SENTRY: (id=%5lu, name=%s, parent=%s, address=0x%08x, " \
            "flags(RLDCdeTAOSNrwxtpbEZIiP)="                                   \
            "%c%c%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d, type=",               \
            (unsigned long)MAGIC_SENTRY_ID(E), (E)->name,                      \
            MAGIC_SENTRY_PARENT(E), (unsigned) (E)->address,                   \
            MAGIC_STATE_REGION_C(E), MAGIC_STATE_LIBSPEC_C(E),                 \
            MAGIC_STATE_FLAG(E,MAGIC_STATE_DIRTY),                             \
            MAGIC_STATE_FLAG(E,MAGIC_STATE_CONSTANT),                          \
            MAGIC_STATE_FLAG(E,MAGIC_STATE_DYNAMIC),                           \
            MAGIC_STATE_FLAG(E,MAGIC_STATE_EXT),                               \
            MAGIC_STATE_FLAG(E,MAGIC_STATE_DETACHED),                          \
            MAGIC_STATE_FLAG(E,MAGIC_STATE_ADDR_NOT_TAKEN),                    \
            MAGIC_STATE_FLAG(E,MAGIC_STATE_OUT_OF_BAND),                       \
            MAGIC_STATE_FLAG(E,MAGIC_STATE_STRING),                            \
            MAGIC_STATE_FLAG(E,MAGIC_STATE_NAMED_STRING),                      \
            MAGIC_STATE_FLAG(E,MAGIC_STATE_MODE_R),                            \
            MAGIC_STATE_FLAG(E,MAGIC_STATE_MODE_W),                            \
            MAGIC_STATE_FLAG(E,MAGIC_STATE_MODE_X),                            \
            MAGIC_STATE_FLAG(E,MAGIC_STATE_THREAD_LOCAL),                      \
            MAGIC_STATE_FLAG(E,MAGIC_STATE_MEMPOOL),                           \
            MAGIC_STATE_FLAG(E,MAGIC_STATE_MEMBLOCK),                          \
            MAGIC_STATE_FLAG(E,MAGIC_STATE_EXTERNAL),                          \
            MAGIC_STATE_FLAG(E,MAGIC_STATE_TYPE_SIZE_MISMATCH),                \
            MAGIC_STATE_FLAG(E,MAGIC_STATE_IMMUTABLE),                         \
            MAGIC_STATE_FLAG(E,MAGIC_STATE_INIT),                              \
            MAGIC_STATE_FLAG(E,MAGIC_STATE_DIRTY_PAGE));                       \
        MAGIC_TYPE_PRINT((E)->type, EXPAND_TYPE_STR);                          \
        _magic_printf(")");                                                    \
    } while(0)

#define MAGIC_SENTRY_OFF_BY_N_NO_DUPLICATES 0x01
#define MAGIC_SENTRY_OFF_BY_N_POSITIVE      0x02
#define MAGIC_SENTRY_OFF_BY_N_NEGATIVE      0x04
#define MAGIC_SENTRY_OFF_BY_N_ZERO          0x08
#define MAGIC_SENTRY_OFF_BY_N_ALL_N (MAGIC_SENTRY_OFF_BY_N_POSITIVE |          \
    MAGIC_SENTRY_OFF_BY_N_NEGATIVE | MAGIC_SENTRY_OFF_BY_N_ZERO)

/* Magic state entry array. */
#define _magic_sentries                     (_magic_vars->sentries)
#define _magic_sentries_num                 (_magic_vars->sentries_num)
#define _magic_sentries_str_num             (_magic_vars->sentries_str_num)
#define _magic_sentries_next_id             (_magic_vars->sentries_next_id)

/* Range lookup index */
#define magic_sentry_rl_buff                (_magic_vars->sentry_rl_buff)
#define magic_sentry_rl_buff_offset         (_magic_vars->sentry_rl_buff_offset)
#define magic_sentry_rl_buff_size           (_magic_vars->sentry_rl_buff_size)
#define magic_sentry_rl_index               (_magic_vars->sentry_rl_index)

/* Hash vars */
#define magic_sentry_hash_buff              (_magic_vars->sentry_hash_buff)
#define magic_sentry_hash_buff_offset       (_magic_vars->sentry_hash_buff_offset)
#define magic_sentry_hash_buff_size         (_magic_vars->sentry_hash_buff_size)
#define magic_sentry_hash_head              (_magic_vars->sentry_hash_head)

/* Estimated maximum number of buckets needed. Increase as necessary. */
#define MAGIC_SENTRY_NAME_EST_MAX_BUCKETS   32768
/*
 * Since we don't support freeing memory, we need to allocate _all_ the
 * intermediate buckets as well. For simplicity, just assume 1 + 2 + 4 + ...
 * + 2^n, though it will probably be less than that.
 */
#define MAGIC_SENTRY_NAME_EST_TOTAL_BUCKETS                                    \
    ((MAGIC_SENTRY_NAME_EST_MAX_BUCKETS << 1) - 1)
#define MAGIC_SENTRY_NAME_HASH_OVERHEAD                                        \
    (MAGIC_SENTRY_NAME_EST_TOTAL_BUCKETS * sizeof(UT_hash_bucket) +            \
    sizeof(UT_hash_table))

#define MAGIC_SENTRY_TO_HASH_EL(sentry, sentry_hash, sentry_list)              \
    do {                                                                       \
        assert(strlen(sentry->name) + 2 * strlen(MAGIC_DSENTRY_ABS_NAME_SEP)   \
            + 2 < MAGIC_SENTRY_NAME_MAX_KEY_LEN                                \
            && "Sentry key length too long!");                                 \
                                                                               \
        sentry_hash->key[0] = 0;                                               \
        snprintf(sentry_hash->key, sizeof(sentry_hash->key),                   \
            "%s%s%s" MAGIC_ID_FORMAT, MAGIC_DSENTRY_ABS_NAME_SEP,              \
            sentry->name, MAGIC_DSENTRY_ABS_NAME_SEP,                          \
            (_magic_id_t) MAGIC_DSENTRY_SITE_ID_NULL);                         \
        sentry_list->sentry = sentry;                                          \
        sentry_hash->sentry_list = sentry_list;                                \
    } while (0)

#define MAGIC_DSENTRY_TO_HASH_EL(dsentry, sentry, sentry_hash, sentry_list)    \
    do {                                                                       \
        assert(strlen(sentry->name) + strlen(dsentry->parent_name)             \
            + 2 * strlen(MAGIC_DSENTRY_ABS_NAME_SEP)                           \
            + 10 +                                                             \
            + 1 < MAGIC_SENTRY_NAME_MAX_KEY_LEN                                \
            && "Dsentry key length too long!");                                \
                                                                               \
        sentry_hash->key[0] = 0;                                               \
        snprintf(sentry_hash->key, sizeof(sentry_hash->key),                   \
            "%s%s%s%s" MAGIC_ID_FORMAT, dsentry->parent_name,                  \
            MAGIC_DSENTRY_ABS_NAME_SEP, sentry->name,                          \
            MAGIC_DSENTRY_ABS_NAME_SEP, dsentry->site_id);                     \
        sentry_list->sentry = sentry;                                          \
        sentry_hash->sentry_list = sentry_list;                                \
    } while (0)

/* Lookup functions. */
PUBLIC struct _magic_sentry *magic_sentry_lookup_by_id(_magic_id_t id,
    struct _magic_dsentry *dsentry_buff);
PUBLIC struct _magic_sentry *magic_sentry_lookup_by_addr(void *addr,
    struct _magic_dsentry *dsentry_buff);
PUBLIC struct _magic_sentry *
    magic_sentry_lookup_by_name(const char *parent_name, const char *name,
    _magic_id_t site_id, struct _magic_dsentry *dsentry_buff);
PUBLIC struct _magic_sentry *magic_sentry_lookup_by_range(void *addr,
    struct _magic_dsentry *dsentry_buff);
PUBLIC struct _magic_sentry *magic_sentry_lookup_by_min_off_by_n(void *addr,
    int flags, long *min_n_ptr, struct _magic_dsentry *dsentry_buff);
PUBLIC struct _magic_sentry *
    magic_sentry_lookup_by_string(const char *string);

/* Lookup index functions. */
PUBLIC void magic_sentry_rl_build_index(void *buff, size_t buff_size);
PUBLIC void magic_sentry_rl_destroy_index(void);
PUBLIC size_t magic_sentry_rl_estimate_index_buff_size(int sentries_num);
PUBLIC void magic_sentry_rl_print_index(void);
PUBLIC struct _magic_sentry *magic_sentry_rl_lookup(void* start_addr);
PUBLIC struct _magic_sentry *magic_sentry_rl_insert(void* start_addr,
    struct _magic_sentry *sentry);
PUBLIC struct _magic_sentry *magic_sentry_rl_pred_lookup(void* addr);
PUBLIC struct _magic_sentry *magic_sentry_lookup_by_range_index(void *addr,
    struct _magic_dsentry *dsentry_buff);

/* Lookup hash functions. */
PUBLIC void magic_sentry_hash_build(void *buff, size_t buff_size);
PUBLIC void magic_sentry_hash_destroy(void);
PUBLIC size_t magic_sentry_hash_estimate_buff_size(int sentries_num);
PUBLIC struct _magic_sentry *
    magic_sentry_lookup_by_name_hash(const char *parent_name,
    const char *name, _magic_id_t site_id,
    struct _magic_dsentry *dsentry_buff);
PUBLIC void *magic_sentry_hash_alloc(size_t size);
PUBLIC void magic_sentry_hash_dealloc(void *object, size_t size);
PUBLIC struct _magic_sentry_list *magic_sentry_list_lookup_by_name_hash(
    const char *parent_name, const char *name, _magic_id_t site_id,
    struct _magic_dsentry *dsentry_buff);

/* Magic state entry functions. */
PUBLIC int magic_check_sentry(struct _magic_sentry *entry);
PUBLIC int magic_check_sentries(void);
PUBLIC void magic_print_sentry(struct _magic_sentry* entry);
PUBLIC void magic_print_sentry_abs_name(struct _magic_sentry *sentry);
PUBLIC void magic_print_sentries(void);
PUBLIC void magic_print_nonstr_sentries(void);
PUBLIC void magic_print_str_sentries(void);
PUBLIC long magic_sentry_get_off_by_n(struct _magic_sentry* sentry,
    void *addr, int flags);

#endif /* _MAGIC_SENTRY_H */
