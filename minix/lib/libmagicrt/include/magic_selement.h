#ifndef _MAGIC_SELEMENT_H
#define _MAGIC_SELEMENT_H

#include <magic.h>
#include <magic_def.h>
#include <magic_common.h>
#include <magic_structs.h>
#include <magic_analysis.h>

/* Magic state element functions. */
PUBLIC void magic_selement_print_value(const _magic_selement_t *selement);
PUBLIC unsigned long
    magic_selement_to_unsigned(const _magic_selement_t *selement);
PUBLIC long magic_selement_to_int(const _magic_selement_t *selement);
#if MAGIC_LONG_LONG_SUPPORTED
PUBLIC unsigned long long
    magic_selement_to_llu(const _magic_selement_t *selement);
PUBLIC long long magic_selement_to_ll(const _magic_selement_t *selement);
#endif
PUBLIC double magic_selement_to_float(const _magic_selement_t *selement);
PUBLIC void* magic_selement_to_ptr(const _magic_selement_t *selement);
PUBLIC void magic_selement_from_unsigned(const _magic_selement_t *selement,
    unsigned long value);
PUBLIC void magic_selement_from_int(const _magic_selement_t *selement,
    long value);
PUBLIC void magic_selement_from_float(const _magic_selement_t *selement,
    double value);
PUBLIC int magic_selement_ptr_value_cast(const _magic_selement_t *src_selement,
    const _magic_selement_t *dst_selement, void* value_buffer);
PUBLIC int
magic_selement_unsigned_value_cast(const _magic_selement_t *src_selement,
    const _magic_selement_t *dst_selement, void* value_buffer);
PUBLIC int magic_selement_int_value_cast(const _magic_selement_t *src_selement,
    const _magic_selement_t *dst_selement, void* value_buffer);
PUBLIC int
magic_selement_float_value_cast(const _magic_selement_t *src_selement,
    const _magic_selement_t *dst_selement, void* value_buffer);
PUBLIC int magic_selement_value_cast(const _magic_selement_t *src_selement,
    const _magic_selement_t *dst_selement, void* value_buffer);
PUBLIC _magic_selement_t*
magic_selement_get_parent(const _magic_selement_t *selement,
    _magic_selement_t *parent_selement);
PUBLIC void magic_selement_fill_from_parent_info(_magic_selement_t *selement,
    int walk_flags);
PUBLIC _magic_selement_t*
magic_selement_from_sentry(struct _magic_sentry *sentry,
    _magic_selement_t *selement);
PUBLIC _magic_selement_t*
magic_selement_from_relative_name(_magic_selement_t *parent_selement,
    _magic_selement_t *selement, char* name);


#endif /* _MAGIC_SELEMENT_H */
