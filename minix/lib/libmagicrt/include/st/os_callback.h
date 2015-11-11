#ifndef ST_OS_CALLBACK_H
#define ST_OS_CALLBACK_H

#include <magic_analysis.h>

/* Callback type definitions and call registration helpers. */

#define CALLBACK_PREFIX st
#define CALLBACK_FAMILY os
#include <st/cb_template.h>

DEFINE_DECL_CALLBACK(void, panic, (const char *fmt, ...));
DEFINE_DECL_CALLBACK(int, old_state_table_lookup, (void *state_info_opaque, void *vars));
DEFINE_DECL_CALLBACK(int, copy_state_region, (void *state_info_opaque, uint32_t address, size_t size, uint32_t dst_address));
DEFINE_DECL_CALLBACK(void *, alloc_contig, (size_t len, int flags, uint32_t *phys));
DEFINE_DECL_CALLBACK(int, free_contig, (void *ptr, size_t length));
DEFINE_DECL_CALLBACK(const char *, debug_header, (void));

/* Default callback values. */
#define ST_CB_OS_PANIC_EMPTY                    NULL
#define ST_CB_OS_OLD_STATE_TABLE_LOOKUP_EMPTY   NULL
#define ST_CB_OS_COPY_STATE_REGION_EMPTY        NULL
#define ST_CB_OS_ALLOC_CONTIG_EMPTY             NULL
#define ST_CB_OS_FREE_CONTIG_EMPTY              NULL
#define ST_CB_OS_DEBUG_HEADER_EMPTY             NULL

struct st_cbs_os_t {
    CALLBACK_TYPENAME(panic)                    panic;
    CALLBACK_TYPENAME(old_state_table_lookup)   old_state_table_lookup;
    CALLBACK_TYPENAME(copy_state_region)        copy_state_region;
    CALLBACK_TYPENAME(alloc_contig)             alloc_contig;
    CALLBACK_TYPENAME(free_contig)              free_contig;
    CALLBACK_TYPENAME(debug_header)             debug_header;
};

/* General callback registration helper. */
PUBLIC void st_setcb_os_all(struct st_cbs_os_t *cbs);

#endif /* ST_OS_CALLBACK_H */
