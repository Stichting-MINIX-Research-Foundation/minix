#ifndef _MAGIC_ANALYSIS_H
#define _MAGIC_ANALYSIS_H

#include <magic.h>
#include <magic_mem.h>

/* Special convenience types. */
#define MAGIC_TYPE_SPECIAL_INIT(STR) { 0, STR, NULL, 0, STR, 0, 0, 0, 0, 0, 0, \
    0, MAGIC_TYPE_FUNCTION, MAGIC_TYPE_EXTERNAL, 0, NULL }
EXTERN char magic_ne_str[];
EXTERN char magic_enf_str[];
EXTERN char magic_bo_str[];
EXTERN char magic_be_str[];
EXTERN char magic_bv_str[];
EXTERN char magic_vf_str[];
EXTERN const struct _magic_type magic_NULL_ENTRY_TYPE;
EXTERN const struct _magic_type magic_ENTRY_NOT_FOUND_TYPE;
EXTERN const struct _magic_type magic_BAD_OFFSET_TYPE;
EXTERN const struct _magic_type magic_BAD_ENTRY_TYPE;
EXTERN const struct _magic_type magic_BAD_VALUE_TYPE;
EXTERN const struct _magic_type magic_VALUE_FOUND;
#define MAGIC_TYPE_NULL_ENTRY               (&magic_NULL_ENTRY_TYPE)
#define MAGIC_TYPE_ENTRY_NOT_FOUND          (&magic_ENTRY_NOT_FOUND_TYPE)
#define MAGIC_TYPE_BAD_OFFSET               (&magic_BAD_OFFSET_TYPE)
#define MAGIC_TYPE_BAD_ENTRY                (&magic_BAD_ENTRY_TYPE)
#define MAGIC_TYPE_BAD_VALUE                (&magic_BAD_VALUE_TYPE)
#define MAGIC_TYPE_VALUE_FOUND              (&magic_VALUE_FOUND)
#define MAGIC_TYPE_IS_SPECIAL(T) (T == MAGIC_TYPE_NULL_ENTRY                   \
    || T == MAGIC_TYPE_ENTRY_NOT_FOUND || T == MAGIC_TYPE_BAD_OFFSET           \
    || T == MAGIC_TYPE_BAD_ENTRY || T == MAGIC_TYPE_BAD_VALUE                  \
    || T == MAGIC_TYPE_VALUE_FOUND)

/* Magic state element macros. */
#define MAGIC_SEL_ANALYZE_POINTERS          0x00001
#define MAGIC_SEL_ANALYZE_NONPOINTERS       0x00002
#define MAGIC_SEL_ANALYZE_LIKELYPOINTERS    0x00004
#define MAGIC_SEL_ANALYZE_DATA              0x00008
#define MAGIC_SEL_ANALYZE_INVARIANTS        0x00010
#define MAGIC_SEL_ANALYZE_VIOLATIONS        0x00020
#define MAGIC_SEL_ANALYZE_WALKABLE          0x00040
#define MAGIC_SEL_ANALYZE_DYNAMIC           0x00080
#define MAGIC_SEL_ANALYZE_OUT_OF_BAND       0x00100
#define MAGIC_SEL_ANALYZE_LIB_SRC           0x00200
#define MAGIC_SEL_ANALYZE_ALL                                                  \
    (MAGIC_SEL_ANALYZE_POINTERS | MAGIC_SEL_ANALYZE_NONPOINTERS                \
    | MAGIC_SEL_ANALYZE_DATA | MAGIC_SEL_ANALYZE_INVARIANTS                    \
    | MAGIC_SEL_ANALYZE_VIOLATIONS | MAGIC_SEL_ANALYZE_WALKABLE                \
    | MAGIC_SEL_ANALYZE_DYNAMIC | MAGIC_SEL_ANALYZE_OUT_OF_BAND                \
    | MAGIC_SEL_ANALYZE_LIB_SRC)

#define MAGIC_SEL_SKIP_UNIONS               0x00400
#define MAGIC_SEL_SKIP_INTEGERS             0x00800
#define MAGIC_SEL_ANALYZE_NONPTRS_AS_PTRS   0x01000
#define MAGIC_SEL_ANALYZE_RETURN_TRG_PTRS   0x02000

#define MAGIC_SEL_FOUND_DATA                0x04000
#define MAGIC_SEL_FOUND_INVARIANTS          0x08000
#define MAGIC_SEL_FOUND_VIOLATIONS          0X10000
#define MAGIC_SEL_FOUND_WALKABLE            0x20000

/* Magic state element analyzed. */
typedef enum {
    _ptr_type_found,
    _other_types_found,
    _void_type_found,
    _comp_trg_types_found,
    _badentry_found
} _magic_trg_stats_t;
struct _magic_sel_analyzed_s {
    unsigned type_id, contained_type_id;
    int flags;
    int num;
    union {
        struct {
            void *value;
            union {
                struct _magic_dsentry dsentry;
                struct _magic_dfunction dfunction;
            } trg;
            struct {
                struct _magic_dsentry *dsentry;
                struct _magic_dfunction *dfunction;
            } trg_p;
            int trg_flags;
            int trg_offset;
            _magic_selement_t trg_selements[MAGIC_MAX_RECURSIVE_TYPES + 1];
            _magic_trg_stats_t trg_stats[MAGIC_MAX_RECURSIVE_TYPES + 1];
            int first_legal_trg_type;
            unsigned num_legal_trg_types;
            unsigned num_trg_types;
        } ptr;
        struct {
            int value;
            int trg_flags;
        } nonptr;
    } u;
};
typedef struct _magic_sel_analyzed_s _magic_sel_analyzed_t;

#define MAGIC_SEL_ANALYZED_PTR_HAS_TRG_FUNCTION(E)                             \
    (((E)->u.ptr.trg_flags & MAGIC_STATE_TEXT) != 0)
#define MAGIC_SEL_ANALYZED_PTR_HAS_TRG_SENTRY(E)                               \
    ((E)->u.ptr.trg_flags && !MAGIC_SEL_ANALYZED_PTR_HAS_TRG_FUNCTION(E))
#define MAGIC_SEL_ANALYZED_PTR_SENTRY(E)                                       \
    ((E)->u.ptr.trg_flags & MAGIC_SEL_ANALYZE_RETURN_TRG_PTRS)                 \
    ? (E)->u.ptr.trg_p.dsentry->sentry                                         \
    : (E)->u.ptr.trg.dsentry.sentry
#define MAGIC_SEL_ANALYZED_PTR_SENTRY_ADDRESS(E)                               \
    ((E)->u.ptr.trg_flags & MAGIC_SEL_ANALYZE_RETURN_TRG_PTRS)                 \
    ? &((E)->u.ptr.trg_p.dsentry->sentry)                                      \
    : &((E)->u.ptr.trg.dsentry.sentry)
#define MAGIC_SEL_ANALYZED_PTR_FUNCTION(E)                                     \
    ((E)->u.ptr.trg_flags & MAGIC_SEL_ANALYZE_RETURN_TRG_PTRS)                 \
    ? (E)->u.ptr.trg_p.dfunction->function                                     \
    : (E)->u.ptr.trg.dfunction.function
#define MAGIC_SEL_ANALYZED_PTR_TRG_NAME(E)                                     \
    (MAGIC_SEL_ANALYZED_PTR_HAS_SPECIAL_TRG_TYPE(E) ? ""                       \
    : MAGIC_SEL_ANALYZED_PTR_HAS_TRG_FUNCTION(E)                               \
    ? (MAGIC_SEL_ANALYZED_PTR_FUNCTION(E)).name                                \
    : (MAGIC_SEL_ANALYZED_PTR_HAS_TRG_SENTRY(E)                                \
    ? (MAGIC_SEL_ANALYZED_PTR_SENTRY(E)).name : "?"))
#define MAGIC_SEL_ANALYZED_PTR_TRG_ADDRESS(E)                                  \
    (MAGIC_SEL_ANALYZED_PTR_HAS_SPECIAL_TRG_TYPE(E) ? NULL                     \
    : MAGIC_SEL_ANALYZED_PTR_HAS_TRG_FUNCTION(E)                               \
    ? (MAGIC_SEL_ANALYZED_PTR_FUNCTION(E)).address                             \
    : (MAGIC_SEL_ANALYZED_PTR_SENTRY(E)).address)
#define MAGIC_SEL_ANALYZED_PTR_PRINT_TRG_ABS_NAME(E)                           \
    do {                                                                       \
        if (MAGIC_SEL_ANALYZED_PTR_HAS_SPECIAL_TRG_TYPE(E)                     \
            || MAGIC_SEL_ANALYZED_PTR_HAS_TRG_FUNCTION(E)) {                   \
            _magic_printf(MAGIC_SEL_ANALYZED_PTR_TRG_NAME(E));                 \
        } else {                                                               \
            magic_print_sentry_abs_name(                                       \
                MAGIC_SEL_ANALYZED_PTR_SENTRY_ADDRESS(E));                     \
        }                                                                      \
    } while(0)
#define MAGIC_SEL_ANALYZED_PTR_FIRST_TRG_TYPE(E)                               \
    ((E)->u.ptr.trg_selements[0].type)
#define MAGIC_SEL_ANALYZED_PTR_HAS_SPECIAL_TRG_TYPE(E)                         \
    (MAGIC_TYPE_IS_SPECIAL(MAGIC_SEL_ANALYZED_PTR_FIRST_TRG_TYPE(E)))
#define MAGIC_SEL_ANALYZED_PTR_SET_SPECIAL_TRG_TYPE(E,T)                       \
    do {                                                                       \
        (E)->u.ptr.trg_selements[0].type = T;                                  \
        (E)->u.ptr.num_trg_types = 1;                                          \
        (E)->u.ptr.num_legal_trg_types = 0;                                    \
        (E)->u.ptr.first_legal_trg_type = -1;                                  \
    } while(0)
#define MAGIC_SEL_ANALYZED_TRG_FLAGS(E)                                        \
    ((E)->type_id == MAGIC_TYPE_POINTER ? (E)->u.ptr.trg_flags                 \
    : (E)->u.nonptr.trg_flags)
#define MAGIC_SEL_ANALYZED_FLAG(E,F) (((E)->flags & F) != 0)
#define MAGIC_SEL_ANALYZED_TRG_STATS_HAS_VIOLATIONS(E)                         \
    ((E) == _other_types_found || (E) == _badentry_found)
#define MAGIC_SEL_ANALYZED_TRG_STATS_C(E)                                      \
    ((E) == _ptr_type_found ? 'p' : (E) == _other_types_found ? 'o'            \
    : (E) == _void_type_found ? 'v' : (E) == _comp_trg_types_found ? 'c'       \
    : (E) == _badentry_found ? 'b' : '?')

#define MAGIC_SEL_ANALYZED_PRINT(E, FLAGS) do {                                \
        _magic_printf("SEL_ANALYZED: (num=%d, type=%s, flags(DIVW)=%d%d%d%d",  \
            (E)->num, (E)->type_id == MAGIC_TYPE_POINTER ? "ptr" : "nonptr",   \
            MAGIC_SEL_ANALYZED_FLAG(E, MAGIC_SEL_FOUND_DATA),                  \
            MAGIC_SEL_ANALYZED_FLAG(E, MAGIC_SEL_FOUND_INVARIANTS),            \
            MAGIC_SEL_ANALYZED_FLAG(E, MAGIC_SEL_FOUND_VIOLATIONS),            \
            MAGIC_SEL_ANALYZED_FLAG(E, MAGIC_SEL_FOUND_WALKABLE));             \
        if((E)->type_id == MAGIC_TYPE_POINTER) {                               \
            _magic_printf(", value=0x%08x, trg_name=", (E)->u.ptr.value);      \
            MAGIC_SEL_ANALYZED_PTR_PRINT_TRG_ABS_NAME(E);                      \
            _magic_printf(", trg_offset=%d, trg_flags(RL)=%c%c",               \
                (E)->u.ptr.trg_offset,                                         \
                (E)->u.ptr.trg_flags                                           \
                    ? MAGIC_STATE_FLAGS_REGION_C((E)->u.ptr.trg_flags) : 0,    \
                (E)->u.ptr.trg_flags                                           \
                    ? MAGIC_STATE_FLAGS_LIBSPEC_C((E)->u.ptr.trg_flags) : 0);  \
            if((E)->u.ptr.num_trg_types > 0) {                                 \
                _magic_printf(", trg_selements=(");                            \
                magic_sel_analyzed_trg_selements_print(E, FLAGS);              \
                _magic_printf(")");                                            \
            }                                                                  \
        } else {                                                               \
            _magic_printf(", value=%d/0x%08x",                                 \
                (E)->u.nonptr.value, (E)->u.nonptr.value);                     \
            if((E)->u.nonptr.trg_flags) {                                      \
                _magic_printf(", trg_flags(RL)=%c%c",                          \
                    MAGIC_STATE_FLAGS_REGION_C((E)->u.nonptr.trg_flags),       \
                    MAGIC_STATE_FLAGS_LIBSPEC_C((E)->u.nonptr.trg_flags));     \
            }                                                                  \
        }                                                                      \
        _magic_printf(")");                                                    \
    } while(0)

/* Magic state element stats. */
struct _magic_sel_stats_s {
    unsigned ptr_found;
    unsigned nonptr_found;
    unsigned nonptr_unconstrained_found;
    int trg_flags;
    int ptr_type_found;
    int other_types_found;
    int null_type_found;
    int badoffset_found;
    int unknown_found;
    int void_type_found;
    int comp_trg_types_found;
    int value_found;
    int badvalue_found;
    int badentry_found;
};
typedef struct _magic_sel_stats_s _magic_sel_stats_t;

/* Magic state element stats. */
#define MAGIC_SEL_STAT_INCR(S,I,F) ((S)->F += (I)->F)
#define MAGIC_SEL_STATS_INCR(S,I) do {                                         \
        MAGIC_SEL_STAT_INCR(S,I, ptr_found);                                   \
        MAGIC_SEL_STAT_INCR(S,I, nonptr_found);                                \
        MAGIC_SEL_STAT_INCR(S,I, nonptr_unconstrained_found);                  \
        S->trg_flags |= I->trg_flags;                                          \
        MAGIC_SEL_STAT_INCR(S,I, ptr_type_found);                              \
        MAGIC_SEL_STAT_INCR(S,I, other_types_found);                           \
        MAGIC_SEL_STAT_INCR(S,I, null_type_found);                             \
        MAGIC_SEL_STAT_INCR(S,I, badoffset_found);                             \
        MAGIC_SEL_STAT_INCR(S,I, unknown_found);                               \
        MAGIC_SEL_STAT_INCR(S,I, void_type_found);                             \
        MAGIC_SEL_STAT_INCR(S,I, comp_trg_types_found);                        \
        MAGIC_SEL_STAT_INCR(S,I, value_found);                                 \
        MAGIC_SEL_STAT_INCR(S,I, badvalue_found);                              \
        MAGIC_SEL_STAT_INCR(S,I, badentry_found);                              \
    } while(0)

#define MAGIC_SEL_STATS_HAS_VIOLATIONS(S)                                      \
    (MAGIC_SEL_STATS_NUM_VIOLATIONS(S) > 0)
#define MAGIC_SEL_STATS_NUM_VIOLATIONS(S)                                      \
    ((S)->ptr_found ? MAGIC_SEL_PTR_STATS_NUM_VIOLATIONS(S)                    \
        : MAGIC_SEL_NONPTR_STATS_NUM_VIOLATIONS(S))
#define MAGIC_SEL_PTR_STATS_NUM_VIOLATIONS(S) ((S)->other_types_found          \
    + (S)->badoffset_found + (S)->unknown_found + (S)->badvalue_found          \
    + (S)->badentry_found)
#define MAGIC_SEL_NONPTR_STATS_NUM_VIOLATIONS(S) ((S)->badvalue_found)

#define MAGIC_SEL_STATS_PRINT(E) do {                                          \
        _magic_printf("SEL_STATS: (type=%s",                                   \
            (E)->ptr_found ? "ptr" : "nonptr");                                \
        if((E)->trg_flags) {                                                   \
            _magic_printf(", trg_flags(RL)=%c%c",                              \
                MAGIC_STATE_FLAGS_REGION_C((E)->trg_flags),                    \
                MAGIC_STATE_FLAGS_LIBSPEC_C((E)->trg_flags));                  \
        }                                                                      \
        if((E)->ptr_found) _magic_printf(", ptr_found=%d", (E)->ptr_found);    \
        if((E)->nonptr_found)                                                  \
            _magic_printf(", nonptr_found=%d", (E)->nonptr_found);             \
        if((E)->nonptr_unconstrained_found)                                    \
            _magic_printf(", nonptr_unconstrained_found=%d",                   \
                (E)->nonptr_unconstrained_found);                              \
        if((E)->ptr_type_found)                                                \
            _magic_printf(", ptr_type_found=%d", (E)->ptr_type_found);         \
        if((E)->other_types_found)                                             \
            _magic_printf(", other_types_found=%d", (E)->other_types_found);   \
        if((E)->null_type_found)                                               \
            _magic_printf(", null_type_found=%d", (E)->null_type_found);       \
        if((E)->badoffset_found)                                               \
            _magic_printf(", badoffset_found=%d", (E)->badoffset_found);       \
        if((E)->unknown_found)                                                 \
            _magic_printf(", unknown_found=%d", (E)->unknown_found);           \
        if((E)->void_type_found)                                               \
            _magic_printf(", void_type_found=%d", (E)->void_type_found);       \
        if((E)->comp_trg_types_found)                                          \
            _magic_printf(", comp_trg_types_found=%d",                         \
                (E)->comp_trg_types_found);                                    \
        if((E)->value_found)                                                   \
            _magic_printf(", value_found=%d", (E)->value_found);               \
        if((E)->badvalue_found)                                                \
            _magic_printf(", badvalue_found=%d", (E)->badvalue_found);         \
        if((E)->badentry_found)                                                \
            _magic_printf(", badentry_found=%d", (E)->badentry_found);         \
        _magic_printf(", violations=%d", MAGIC_SEL_STATS_NUM_VIOLATIONS(E));   \
        _magic_printf(")");                                                    \
    } while(0)

/* Magic sentry macros. */
#define MAGIC_SENTRY_ANALYZE_STOP           1
#define MAGIC_SENTRY_ANALYZE_CONTINUE       2
#define MAGIC_SENTRY_ANALYZE_SKIP_PATH      3
#define MAGIC_SENTRY_ANALYZE_IS_VALID_RET(R)                                   \
   ((R)>=MAGIC_SENTRY_ANALYZE_STOP && (R)<=MAGIC_SENTRY_ANALYZE_SKIP_PATH)

#ifndef __MINIX
#define MAGIC_PTR_LIKELY_INTS_START         0xFFFFF000
#else
#define MAGIC_PTR_LIKELY_INTS_START         0xE0000000
#endif
#define MAGIC_PTR_LIKELY_INTS_END           0xFFF
#define MAGIC_PTR_IS_LIKELY_INT(V)                                             \
    ((V) && ((unsigned)(V)>=MAGIC_PTR_LIKELY_INTS_START                        \
    || (unsigned)(V)<=MAGIC_PTR_LIKELY_INTS_END))
#define MAGIC_INT_IS_LIKELY_PTR(V)                                             \
    ((V) && !MAGIC_PTR_IS_LIKELY_INT((void*)V))

/* Magic callbacks. */
typedef int (*magic_cb_sentries_analyze_pre_t)(void);
PUBLIC void magic_setcb_sentries_analyze_pre(magic_cb_sentries_analyze_pre_t cb);

/* Magic state entry functions. */
typedef int (*magic_sentry_analyze_cb_t)(_magic_selement_t* selement,
    _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats,
    void* cb_args);
PUBLIC int magic_sentry_print_ptr_types(struct _magic_sentry* entry);
PUBLIC int magic_sentry_extract_ptrs(struct _magic_sentry* entry,
    void ****ptr_map, const struct _magic_type ***ptr_type_map, int *ptr_num);
PUBLIC int magic_sentry_analyze(struct _magic_sentry* sentry, int flags,
    const magic_sentry_analyze_cb_t cb, void* cb_args,
    _magic_sel_stats_t *sentry_stats);
PUBLIC int magic_sentries_analyze(int flags, const magic_sentry_analyze_cb_t cb,
    void* cb_args, _magic_sel_stats_t *sentries_stats);
PUBLIC int magic_sentry_print_selements(struct _magic_sentry* sentry);
PUBLIC int magic_sentry_print_ptr_selements(struct _magic_sentry* sentry,
    int skip_null_ptrs, int max_target_recusions);

/* Magic dynamic state entry functions. */
PUBLIC int magic_dsentries_analyze(int flags,
    const magic_sentry_analyze_cb_t cb, void* cb_args,
    _magic_sel_stats_t *dsentries_stats);

/* Magic sentry analyze callbacks. */
PUBLIC int magic_sentry_print_el_cb(_magic_selement_t* selement,
    _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats,
    void* cb_args);
PUBLIC int magic_sentry_print_ptr_el_cb(_magic_selement_t* selement,
    _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats,
    void* cb_args);
PUBLIC int magic_sentry_print_el_with_trg_reg_cb(_magic_selement_t* selement,
    _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats,
    void* cb_args);
PUBLIC int magic_sentry_print_el_with_trg_cb(_magic_selement_t* selement,
    _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats,
    void* cb_args);

/* Magic sentry analyze helpers. */
#define magic_sentry_analyze_invariants(sentry, cb, cb_args, sentry_stats)     \
    magic_sentry_analyze(sentry, MAGIC_SEL_ANALYZE_POINTERS                    \
        | MAGIC_SEL_ANALYZE_NONPOINTERS | MAGIC_SEL_ANALYZE_INVARIANTS, cb,    \
        cb_args, sentry_stats)
#define magic_sentries_analyze_invariants(cb, cb_args, sentries_stats)         \
    magic_sentries_analyze(MAGIC_SEL_ANALYZE_POINTERS                          \
        | MAGIC_SEL_ANALYZE_NONPOINTERS | MAGIC_SEL_ANALYZE_INVARIANTS, cb,    \
        cb_args, sentries_stats)
#define magic_dsentries_analyze_invariants(cb, cb_args, dsentries_stats)       \
    magic_dsentries_analyze(MAGIC_SEL_ANALYZE_POINTERS                         \
        | MAGIC_SEL_ANALYZE_NONPOINTERS | MAGIC_SEL_ANALYZE_INVARIANTS, cb,    \
        cb_args, dsentries_stats)
#define magic_allsentries_analyze_invariants(cb, cb_args, sentries_stats)      \
    magic_sentries_analyze(MAGIC_SEL_ANALYZE_POINTERS                          \
        | MAGIC_SEL_ANALYZE_NONPOINTERS | MAGIC_SEL_ANALYZE_INVARIANTS         \
        | MAGIC_SEL_ANALYZE_DYNAMIC, cb, cb_args, sentries_stats)

#define magic_sentry_analyze_violations(sentry, cb, cb_args, sentry_stats)     \
    magic_sentry_analyze(sentry, MAGIC_SEL_ANALYZE_POINTERS                    \
        | MAGIC_SEL_ANALYZE_NONPOINTERS | MAGIC_SEL_ANALYZE_VIOLATIONS, cb,    \
        cb_args, sentry_stats)
#define magic_sentries_analyze_violations(cb, cb_args, sentries_stats)         \
    magic_sentries_analyze(MAGIC_SEL_ANALYZE_POINTERS                          \
        | MAGIC_SEL_ANALYZE_NONPOINTERS | MAGIC_SEL_ANALYZE_VIOLATIONS, cb,    \
        cb_args, sentries_stats)
#define magic_dsentries_analyze_violations(cb, cb_args, dsentries_stats)       \
    magic_dsentries_analyze(MAGIC_SEL_ANALYZE_POINTERS                         \
        | MAGIC_SEL_ANALYZE_NONPOINTERS | MAGIC_SEL_ANALYZE_VIOLATIONS, cb,    \
        cb_args, dsentries_stats)
#define magic_allsentries_analyze_violations(cb, cb_args, sentries_stats)      \
    magic_sentries_analyze(MAGIC_SEL_ANALYZE_POINTERS                          \
        | MAGIC_SEL_ANALYZE_NONPOINTERS | MAGIC_SEL_ANALYZE_VIOLATIONS         \
        | MAGIC_SEL_ANALYZE_DYNAMIC, cb, cb_args, sentries_stats)

#define magic_sentry_analyze_likely_pointers(sentry, cb, cb_args, sentry_stats)\
    magic_sentry_analyze(sentry, MAGIC_SEL_ANALYZE_LIKELYPOINTERS              \
        | MAGIC_SEL_ANALYZE_DATA, cb, cb_args, sentry_stats)
#define magic_sentries_analyze_likely_pointers(cb, cb_args, sentries_stats)    \
    magic_sentries_analyze(MAGIC_SEL_ANALYZE_LIKELYPOINTERS                    \
        | MAGIC_SEL_ANALYZE_DATA, cb, cb_args, sentries_stats)
#define magic_dsentries_analyze_likely_pointers(cb, cb_args, dsentries_stats)  \
    magic_dsentries_analyze(MAGIC_SEL_ANALYZE_LIKELYPOINTERS                   \
        | MAGIC_SEL_ANALYZE_DATA, cb, cb_args, dsentries_stats)
#define magic_allsentries_analyze_likely_pointers(cb, cb_args, sentries_stats) \
    magic_sentries_analyze(MAGIC_SEL_ANALYZE_LIKELYPOINTERS                    \
        | MAGIC_SEL_ANALYZE_DATA | MAGIC_SEL_ANALYZE_DYNAMIC, cb, cb_args,     \
        sentries_stats)

/* Magic state type functions. */
PUBLIC int magic_type_count_ptrs(const struct _magic_type* type, int *ptr_num);

/* Magic type walk callbacks. */
PUBLIC int magic_type_examine_ptr_cb(const struct _magic_type* parent_type,
    const unsigned parent_offset, int child_num, const struct _magic_type* type,
    const unsigned offset, int depth, void* cb_args);
PUBLIC int magic_type_extract_ptr_cb(const struct _magic_type* parent_type,
    const unsigned parent_offset, int child_num, const struct _magic_type* type,
    const unsigned offset, int depth, void* cb_args);
PUBLIC int magic_type_analyzer_cb(const struct _magic_type* parent_type,
    const unsigned parent_offset, int child_num, const struct _magic_type* type,
    const unsigned offset, int depth, void* cb_args);

/* Magic state element functions. */
PUBLIC int magic_selement_analyze(_magic_selement_t *selement, int flags,
    _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats);
PUBLIC int magic_selement_analyze_ptr(_magic_selement_t *selement, int flags,
    _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats);
PUBLIC int magic_selement_analyze_nonptr(_magic_selement_t *selement, int flags,
    _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats);
PUBLIC int magic_selement_analyze_ptr_value_invs(_magic_selement_t *selement,
    _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats);
PUBLIC int magic_selement_analyze_ptr_trg_invs(_magic_selement_t *selement,
    _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats);
PUBLIC _magic_trg_stats_t
magic_selement_analyze_ptr_target(const struct _magic_type *ptr_type,
    const struct _magic_type *trg_type, int trg_flags);
PUBLIC int magic_selement_analyze_ptr_type_invs(_magic_selement_t *selement,
    _magic_sel_analyzed_t *sel_analyzed, _magic_sel_stats_t *sel_stats);
PUBLIC int magic_selement_recurse_ptr(_magic_selement_t *selement,
    _magic_selement_t *new_selement, int max_steps);
PUBLIC void
magic_sel_analyzed_trg_selements_print(_magic_sel_analyzed_t *sel_analyzed,
    int flags);
PUBLIC _magic_selement_t*
magic_selement_type_cast(_magic_selement_t *selement, int flags,
    const struct _magic_type* type, _magic_sel_analyzed_t *sel_analyzed,
    _magic_sel_stats_t *sel_stats);

#endif /* _MAGIC_ANALYSIS_H */

