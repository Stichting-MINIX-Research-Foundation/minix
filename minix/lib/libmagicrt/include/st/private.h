#ifndef ST_PRIVATE_H
#define ST_PRIVATE_H


#include <magic.h>
#include <magic_mem.h>
#include <magic_analysis.h>
#include <magic_asr.h>
#include <stdarg.h>
#include <stdint.h>

#include <st/os_callback.h>
#include <st/state_transfer.h>

/* Data transfer and adjustment functions */
#if !ST_ASSUME_RAW_COPY_BEFORE_TRANSFER
PRIVATE int deallocate_nonxferred_dsentries(struct _magic_dsentry *first_dsentry, st_counterparts_t *counterparts);
#endif
PRIVATE void deallocate_local_dsentry(struct _magic_dsentry *local_dsentry);
PRIVATE int allocate_local_dsentry(st_init_info_t *info, struct _magic_dsindex *local_dsindex, int num_elements, int is_type_mismatch, const union __alloc_flags *p_alloc_flags, struct _magic_dsentry** local_dsentry_ptr, struct _magic_dsentry *cached_dsentry, void *ptr);

PRIVATE int check_unpaired_sentry(st_init_info_t *info, struct _magic_sentry* cached_sentry);
PRIVATE int transfer_data_sentry(st_init_info_t *info, struct _magic_sentry* cached_sentry);
PRIVATE int transfer_data_selement(_magic_selement_t *selement, _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats, void *cb_args);
PRIVATE int lookup_trg_info(_magic_selement_t *selement, _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats, struct st_cb_info *cb_info,
    _magic_selement_t *cached_trg_selement, _magic_selement_t *local_trg_selement);
PRIVATE INLINE void st_set_transfer_status(int status_flags, int status_op,
    struct _magic_sentry *cached_sentry, struct _magic_function *cached_function);
PRIVATE INLINE void st_map_selement(_magic_selement_t *cached_selement, _magic_selement_t *local_selement, struct st_cb_info *cb_info, int is_trg_mapping);

/* Buffer allocation */
PRIVATE void *persistent_mmap(__MA_ARGS__ st_init_info_t *info, void *start, size_t length, int prot, int flags, int fd, off_t offset, struct _magic_dsentry *dsentry);

#endif /* ST_PRIVATE_H */
