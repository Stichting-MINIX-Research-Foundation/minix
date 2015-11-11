#ifndef METADATA_TRANSFER_H
#define METADATA_TRANSFER_H

#include <st/state_transfer.h>

/* Metadata transfer and adjustment functions */
PRIVATE int transfer_metadata_functions(st_init_info_t *info,
    struct _magic_vars_t *cached_magic_vars,
    struct _magic_vars_t *remote_magic_vars,
    st_counterparts_t *counterparts);
PRIVATE int transfer_metadata_dfunctions(st_init_info_t *info,
    struct _magic_vars_t *cached_magic_vars,
    struct _magic_vars_t *remote_magic_vars,
    st_counterparts_t *counterparts);
PRIVATE int transfer_metadata_type_members(st_init_info_t *info,
    struct _magic_type *type, struct _magic_vars_t *cached_magic_vars,
    struct _magic_vars_t *remote_magic_vars);
PRIVATE int transfer_metadata_sentries(st_init_info_t *info,
    struct _magic_vars_t *cached_magic_vars,
    struct _magic_vars_t *remote_magic_vars, st_counterparts_t *counterparts,
    size_t *max_buff_sz);
PRIVATE int transfer_metadata_sentry_members(st_init_info_t *info,
    struct _magic_sentry *sentry);

PRIVATE int pair_metadata_types(st_init_info_t *info,
    struct _magic_vars_t *cached_magic_vars,
    st_counterparts_t *counterparts, int allow_unpaired_types);
PRIVATE int pair_metadata_functions(st_init_info_t *info,
    struct _magic_vars_t *cached_magic_vars, st_counterparts_t *counterparts);
PRIVATE int pair_metadata_sentries(st_init_info_t *info,
    struct _magic_vars_t *cached_magic_vars, st_counterparts_t *counterparts);
#if !ST_ASSUME_RAW_COPY_BEFORE_TRANSFER
PRIVATE int allocate_pair_metadata_dsentries(st_init_info_t *info,
    struct _magic_vars_t *cached_magic_vars, st_counterparts_t *counterparts);
#else
PRIVATE int allocate_pair_metadata_dsentries_from_raw_copy(st_init_info_t *info,
    struct _magic_vars_t *cached_magic_vars, st_counterparts_t *counterparts);
#endif

/* metadata transfer helper functions */
PRIVATE int md_transfer_str(st_init_info_t *info, char **str_pt);
#define MD_TRANSFER_STR(INFO, STR_PT)                                          \
    do {                                                                       \
        if (md_transfer_str(INFO, __UNCONST(STR_PT))) {                        \
            printf("%s, line %d. md_transfer_str(): ERROR transferring.\n",    \
                __FILE__, __LINE__);                                           \
            return EGENERIC;                                                   \
        }                                                                      \
    } while(0)
PRIVATE int md_transfer(st_init_info_t *info, void *from, void **to, int len);
#define MD_TRANSFER(INFO, FROM, TO, LEN)                                       \
    do {                                                                       \
        if (md_transfer(INFO, FROM, TO, LEN)) {                                \
            printf("%s, line %d. md_transfer(): ERROR transferring.\n",        \
                __FILE__, __LINE__);                                           \
            return EGENERIC;                                                   \
        }                                                                      \
    } while(0)

#endif /* METADATA_TRANSFER_H */
