#ifndef _MAGIC_RANGE_H
#define _MAGIC_RANGE_H

#include <magic.h>
#include <magic_def.h>
#include <magic_common.h>
#include <magic_structs.h>

/* Magic memory ranges */
#define magic_null_range              _magic_vars->null_range
#define magic_data_range              _magic_vars->data_range
#define magic_heap_range              _magic_vars->heap_range
#define magic_map_range               _magic_vars->map_range
#define magic_shm_range               _magic_vars->shm_range
#define magic_stack_range             _magic_vars->stack_range
#define magic_text_range              _magic_vars->text_range

#define magic_sentry_range            _magic_vars->sentry_range
#define magic_function_range          _magic_vars->function_range
#define magic_dfunction_range         _magic_vars->dfunction_range

#define magic_heap_start              _magic_vars->heap_start
#define magic_heap_end                _magic_vars->heap_end
#define magic_update_dsentry_ranges   _magic_vars->update_dsentry_ranges
#define magic_update_dfunction_ranges _magic_vars->update_dfunction_ranges

/* Magic address ranges. */
#define MAGIC_ADDR_IS_WITHIN(A, MIN, MAX)   ((MIN) <= (A) && (A) <= (MAX))
#define MAGIC_ADDR_IS_IN_RANGE(A, R)                                           \
    MAGIC_ADDR_IS_WITHIN(A, (R)[0], (R)[1])
#define MAGIC_RANGE_IS_IN_RANGE(r, R)                                          \
    (MAGIC_ADDR_IS_IN_RANGE((r)[0], R) && MAGIC_ADDR_IS_IN_RANGE((r)[1], R))
#define MAGIC_RANGE_COPY(RS, RD)            memcpy(RD, RS, 2 * sizeof(void *))
#define MAGIC_RANGE_INIT(R)                 MAGIC_RANGE_COPY(magic_null_range,R)
#define MAGIC_RANGE_IS_NULL(R)                                                 \
    ((R)[0] == magic_null_range[0] && (R)[1] == magic_null_range[1])
#define MAGIC_RANGE_IS_VALID(R)                                                \
    (MAGIC_RANGE_IS_NULL(R) || ((R)[0] <= (R)[1]))
#define MAGIC_RANGE_UPDATE(R, MIN, MAX)                                        \
    do {                                                                       \
        if(MIN < (R)[0])                                                       \
            (R)[0] = MIN;                                                      \
        if(MAX > (R)[1])                                                       \
            (R)[1] = MAX;                                                      \
    } while(0)
#define MAGIC_RANGE_SET_MIN(R, MIN)         ((R)[0] = MIN)
#define MAGIC_RANGE_SET_MAX(R, MAX)         ((R)[1] = MAX)
#define MAGIC_RANGE_SET(R, MIN, MAX)                                           \
    do {                                                                       \
        MAGIC_RANGE_SET_MIN(R, MIN);                                           \
        MAGIC_RANGE_SET_MAX(R, MAX);                                           \
    } while(0)
#define MAGIC_RANGE_PAGE_ROUND_MIN(R)                                          \
    ((R)[0] = (char*)(R)[0] - (unsigned long)(R)[0] % MAGIC_PAGE_SIZE)
#define MAGIC_RANGE_PAGE_ROUND_MAX(R)                                          \
    do {                                                                       \
        unsigned long diff = (unsigned long)(R)[1] % MAGIC_PAGE_SIZE;          \
        if(diff)                                                               \
            (R)[1] = (char *)(R)[1] + MAGIC_PAGE_SIZE - diff - 1;              \
    } while(0)
#define MAGIC_RANGE_PAGE_ROUND(R)                                              \
    do {                                                                       \
        MAGIC_RANGE_PAGE_ROUND_MIN(R);                                         \
        MAGIC_RANGE_PAGE_ROUND_MAX(R);                                         \
    } while(0)
#define MAGIC_RANGE_PRINT(R)                                                   \
    _magic_printf("RANGE %s=[0x%08x;0x%08x]", #R, (unsigned long)((R)[0]),     \
        (unsigned long)((R)[1]))
#define MAGIC_RANGE_CHECK(R)                                                   \
    assert(MAGIC_RANGE_IS_VALID(R) && "Invalid range " #R);
#define MAGIC_RANGE_SIZE(R)                                                    \
    (!MAGIC_RANGE_IS_VALID(R) || MAGIC_RANGE_IS_NULL(R) ?                      \
        0 : (char *)(R)[1] - (char *)(R)[0])

#if MAGIC_RANGE_DEBUG
#define MAGIC_RANGE_DEBUG_ADDR(A, R)                                           \
    do {                                                                       \
        char *what = MAGIC_ADDR_IS_IN_RANGE(A, R) ? "" : "not ";               \
        _magic_printf("MRD: Address 0x%08x %sin ", A, what);                   \
        MAGIC_RANGE_PRINT(R);                                                  \
        _magic_printf("\n");                                                   \
        MAGIC_RANGE_CHECK(R);                                                  \
    } while(0);
#else
#define MAGIC_RANGE_DEBUG_ADDR(A,R)         MAGIC_RANGE_CHECK(R)
#endif

#define MAGIC_ADDR_LOOKUP_USE_DSENTRY_RANGES      1
#define MAGIC_ADDR_LOOKUP_USE_DFUNCTION_RANGES    1

/* Magic range functions. */
PUBLIC void magic_ranges_init(void);
PUBLIC int magic_range_is_dfunction(void* addr);
PUBLIC int magic_range_is_dsentry(void* addr);
PUBLIC int magic_range_is_stack(void* addr);

/* Lookup functions. */
PUBLIC int magic_range_lookup_by_addr(void *addr, void **container);

#endif /* _MAGIC_RANGE_H */
