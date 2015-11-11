#ifndef _MAGIC_H
#define _MAGIC_H

#include <magic_def.h>
#include <magic_common.h>
#include <magic_extern.h>
#include <magic_structs.h>
#include <magic_sentry.h>
#include <magic_selement.h>
#include <magic_range.h>
#include <magic_eval.h>

#include <stdio.h>
#include <stddef.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/* Magic type macros. */
#define MAGIC_TYPE_WALK_STOP                    1
#define MAGIC_TYPE_WALK_SKIP_PATH               2
#define MAGIC_TYPE_WALK_CONTINUE                3

#define MAGIC_TYPE_WALK_UNIONS_AS_VOID          0x1
#define MAGIC_TYPE_WALK_DEFAULT_FLAGS           (MAGIC_TYPE_WALK_UNIONS_AS_VOID)

#define MAGIC_TYPE_COMPARE_VALUE_SET            0x01
#define MAGIC_TYPE_COMPARE_FLAGS                0x02
#define MAGIC_TYPE_COMPARE_NAME                 0x04
#define MAGIC_TYPE_COMPARE_NAMES                0x08
#define MAGIC_TYPE_COMPARE_MEMBER_NAMES         0x10
#define MAGIC_TYPE_COMPARE_MEMBER_OFFSETS       0x20
#define MAGIC_TYPE_COMPARE_ALL (MAGIC_TYPE_COMPARE_VALUE_SET |                 \
    MAGIC_TYPE_COMPARE_FLAGS | MAGIC_TYPE_COMPARE_NAME |                       \
    MAGIC_TYPE_COMPARE_NAMES | MAGIC_TYPE_COMPARE_MEMBER_NAMES |               \
    MAGIC_TYPE_COMPARE_MEMBER_OFFSETS)

#define MAGIC_TYPE_IS_WALKABLE(T) ((T)->type_id == MAGIC_TYPE_ARRAY            \
    || (T)->type_id == MAGIC_TYPE_VECTOR || (T)->type_id == MAGIC_TYPE_UNION   \
    || (T)->type_id == MAGIC_TYPE_STRUCT)
#define MAGIC_TYPE_NUM_CONTAINED_TYPES(T)                                      \
    ((T)->type_id == MAGIC_TYPE_ARRAY ? 1 : (T)->num_child_types)

#define MAGIC_TYPE_IS_VOID(T) ((T)->type_id == MAGIC_TYPE_VOID                 \
    || ( ((T)->flags & MAGIC_TYPE_EXTERNAL) && !strcmp((T)->type_str, "i8") ))
#define MAGIC_TYPE_IS_RAW_ARRAY(T)                                             \
    (((T)->type_id == MAGIC_TYPE_ARRAY                                         \
    && (T)->contained_types[0]->type_id == MAGIC_TYPE_VOID)                    \
    || (T)->type_id == MAGIC_TYPE_UNION)
#define MAGIC_TYPE_IS_INT_ARRAY(T)                                             \
    ((T)->type_id == MAGIC_TYPE_ARRAY                                          \
    && (T)->contained_types[0]->type_id == MAGIC_TYPE_INTEGER)

#define MAGIC_EXPAND_TYPE_STR               0x1
#define MAGIC_SKIP_COMP_TYPES               0x2
#define MAGIC_EXPAND_SENTRY                 0x4

#define MAGIC_TYPE_STR_PRINT_LLVM_TYPES     0x01
#define MAGIC_TYPE_STR_PRINT_SOURCE_TYPES   0x02
#define MAGIC_TYPE_STR_PRINT_MEMBER_NAMES   0x04
#define MAGIC_TYPE_STR_PRINT_SKIP_UNIONS    0x08
#define MAGIC_TYPE_STR_PRINT_SKIP_STRUCTS   0x10
#define MAGIC_TYPE_STR_PRINT_MULTI_NAMES    0x20
#define MAGIC_TYPE_STR_PRINT_STYLE_DEFAULT                                     \
    (MAGIC_TYPE_STR_PRINT_LLVM_TYPES | MAGIC_TYPE_STR_PRINT_SOURCE_TYPES |     \
    MAGIC_TYPE_STR_PRINT_MEMBER_NAMES)

#define MAGIC_TYPE_STR_PRINT_DEBUG          MAGIC_DEBUG_SET(0)

#define MAGIC_TYPE_HAS_COMP_TYPES(T)        ((T)->compatible_types != NULL)
#define MAGIC_TYPE_HAS_COMP_TYPE(T, I)                                         \
    ((T)->compatible_types[(I)] != NULL)
#define MAGIC_TYPE_COMP_TYPE(T, I)          ((T)->compatible_types[(I)])
#define MAGIC_TYPE_NUM_COMP_TYPES(T, NUM)                                      \
    do {                                                                       \
        *(NUM) = 0;                                                            \
        while(MAGIC_TYPE_HAS_COMP_TYPE(T, (*(NUM))++));                        \
        (*(NUM))--;                                                            \
    } while(0)
#define MAGIC_TYPE_HAS_VALUE_SET(T)         ((T)->value_set != NULL)
#define MAGIC_TYPE_HAS_VALUE(T, I)          ((I) < ((int*)(T)->value_set)[0])
#define MAGIC_TYPE_VALUE(T, I)              (((int*)(T)->value_set)[(I)+1])
#define MAGIC_TYPE_NUM_VALUES(T, NUM)       (*(NUM) = ((int*)(T)->value_set)[0])
#define MAGIC_TYPE_HAS_MULTI_NAMES(T)       ((T)->num_names > 1)

#define MAGIC_TYPE_FLAG(T,F) (((T)->flags & (F)) != 0)
#define MAGIC_TYPE_ID(T) ((T)->id)
#define MAGIC_TYPE_IS_STRING(T) (MAGIC_TYPE_FLAG(T,MAGIC_TYPE_EXTERNAL)        \
    && (T)->type_id == MAGIC_TYPE_ARRAY                                        \
    && !strcmp((T)->contained_types[0]->type_str, "i8"))
#define MAGIC_TYPE_PRINT(T, FLAGS) do {                                        \
        _magic_printf("TYPE: (id=%5d, name=%s, size=%d, num_child_types=%d, "  \
            "type_id=%d, bit_width=%d, flags(ERDIVvUP)=%d%d%d%d%d%d%d%d, "     \
            "values='", MAGIC_TYPE_ID(T), (T)->name, (T)->size,                \
            (T)->num_child_types, (T)->type_id, (T)->bit_width,                \
            MAGIC_TYPE_FLAG(T,MAGIC_TYPE_EXTERNAL),                            \
            MAGIC_TYPE_FLAG(T,MAGIC_TYPE_IS_ROOT),                             \
            MAGIC_TYPE_FLAG(T,MAGIC_TYPE_DYNAMIC),                             \
            MAGIC_TYPE_FLAG(T,MAGIC_TYPE_INT_CAST),                            \
            MAGIC_TYPE_FLAG(T,MAGIC_TYPE_STRICT_VALUE_SET),                    \
            MAGIC_TYPE_FLAG(T,MAGIC_TYPE_VARSIZE),                             \
            MAGIC_TYPE_FLAG(T,MAGIC_TYPE_UNSIGNED),                            \
            MAGIC_TYPE_FLAG(T,MAGIC_TYPE_NO_INNER_PTRS));                      \
        if(MAGIC_TYPE_HAS_VALUE_SET(T)) magic_type_values_print(T);            \
        if(MAGIC_TYPE_HAS_MULTI_NAMES(T)) {                                    \
            _magic_printf("', names='");                                       \
            magic_type_names_print(T);                                         \
        }                                                                      \
        _magic_printf("', type_str=");                                         \
        if((FLAGS) & MAGIC_EXPAND_TYPE_STR) magic_type_str_print(T);           \
        else _magic_printf("%s", (T)->type_str ? (T)->type_str : "");          \
        if(MAGIC_TYPE_HAS_COMP_TYPES(T)) {                                     \
            _magic_printf(", comp_types=(");                                   \
            magic_type_comp_types_print(T, FLAGS);                             \
            _magic_printf(")");                                                \
        }                                                                      \
        _magic_printf(")");                                                    \
    } while(0)

#define MAGIC_TYPE_VARSIZE_EL_TYPE(T)                                          \
    (T)->contained_types[(T)->num_child_types - 1]->contained_types[0]

#define MAGIC_TYPE_ARRAY_CREATE_FROM_SIZE(AT,T,CT,S,VSN) do {                  \
        assert(((S) && ((S) % (T)->size == 0 || VSN)) && "Bad size!");         \
        (AT)->id = MAGIC_ID_NONE;                                              \
        (AT)->flags |= MAGIC_TYPE_DYNAMIC;                                     \
        (AT)->type_id = MAGIC_TYPE_ARRAY;                                      \
        (AT)->size = S;                                                        \
        (AT)->num_child_types = (S)/(T)->size;                                 \
        (AT)->contained_types = CT;                                            \
        (AT)->contained_types[0] = T;                                          \
        if(VSN) {                                                              \
            (AT)->flags |= MAGIC_TYPE_VARSIZE;                                 \
            (AT)->num_child_types = VSN;                                       \
        }                                                                      \
    } while(0)
#define MAGIC_TYPE_ARRAY_CREATE_FROM_N(AT,T,CT,N)                              \
    MAGIC_TYPE_ARRAY_CREATE_FROM_SIZE(AT,T,CT,(T)->size*N, 0)

#define MAGIC_TYPE_VOID_ARRAY_GET_FROM_SIZE(AT,S) do {                         \
        *(AT) = *MAGIC_VOID_ARRAY_TYPE;                                        \
        MAGIC_TYPE_ARRAY_CREATE_FROM_SIZE(AT,MAGIC_VOID_TYPE,                  \
            MAGIC_VOID_ARRAY_TYPE->contained_types,S,0);                       \
    } while(0)
#define MAGIC_TYPE_VOID_ARRAY_GET_FROM_N(AT,N)                                 \
    MAGIC_TYPE_VOID_ARRAY_GET_FROM_SIZE(AT,MAGIC_VOID_TYPE->size*N)

#define MAGIC_TYPE_PTRINT_ARRAY_GET_FROM_SIZE(AT,S) do {                       \
        *(AT) = *MAGIC_PTRINT_ARRAY_TYPE;                                      \
        MAGIC_TYPE_ARRAY_CREATE_FROM_SIZE(AT,MAGIC_PTRINT_TYPE,                \
            MAGIC_PTRINT_ARRAY_TYPE->contained_types,S,0);                     \
    } while(0)
#define MAGIC_TYPE_PTRINT_ARRAY_GET_FROM_N(AT,N)                               \
    MAGIC_TYPE_PTRINT_ARRAY_GET_FROM_SIZE(AT,MAGIC_PTRINT_TYPE->size*N)

#define MAGIC_TYPE_INT_CREATE(T,S,N,B) do {                                    \
        (T)->id = MAGIC_ID_NONE;                                               \
        (T)->flags |= MAGIC_TYPE_DYNAMIC;                                      \
        (T)->type_id = MAGIC_TYPE_INTEGER;                                     \
        (T)->size = S;                                                         \
        (T)->num_child_types = 0;                                              \
        (T)->contained_types = NULL;                                           \
        (T)->bit_width = (S)*8;                                                \
        (T)->name = N;                                                         \
        snprintf(B, sizeof(B), "i%d", (T)->bit_width);                         \
        (T)->type_str = B;                                                     \
    } while(0)

#define MAGIC_TYPE_COMP_ITER(TYPE,COMP_TYPE,DO) do {                           \
        if(MAGIC_TYPE_HAS_COMP_TYPES(TYPE)) {                                  \
            int __i = 0;                                                       \
            while(MAGIC_TYPE_HAS_COMP_TYPE(TYPE, __i)) {                       \
                COMP_TYPE=MAGIC_TYPE_COMP_TYPE(TYPE, __i);                     \
                DO                                                             \
                __i++;                                                         \
            }                                                                  \
        }                                                                      \
    } while(0)

/* Magic function macros. */
#define MAGIC_FUNCTION_PARENT(F)                                               \
    (MAGIC_STATE_FLAG(F,MAGIC_STATE_DYNAMIC) ?                                 \
        MAGIC_DFUNCTION_FROM_FUNCTION(F)->parent_name : "")
#define MAGIC_FUNCTION_ID(F) ((F)->id)
#define MAGIC_FUNCTION_PRINT(F, EXPAND_TYPE_STR) do {                          \
        _magic_printf("FUNCTION: (id=%5lu, name=%s, parent=%s, address=0x%08x,"\
            " flags(RLDCdTArwxEI)=%c%c%d%d%d%d%d%d%d%d%d%d, type=",            \
            (unsigned long)MAGIC_FUNCTION_ID(F), (F)->name,                    \
            MAGIC_FUNCTION_PARENT(F), (unsigned) (F)->address,                 \
            MAGIC_STATE_REGION_C(F), MAGIC_STATE_LIBSPEC_C(F),                 \
            MAGIC_STATE_FLAG(F,MAGIC_STATE_DIRTY),                             \
            MAGIC_STATE_FLAG(F,MAGIC_STATE_CONSTANT),                          \
            MAGIC_STATE_FLAG(F,MAGIC_STATE_DYNAMIC),                           \
            MAGIC_STATE_FLAG(F,MAGIC_STATE_DETACHED),                          \
            MAGIC_STATE_FLAG(F,MAGIC_STATE_ADDR_NOT_TAKEN),                    \
            MAGIC_STATE_FLAG(F,MAGIC_STATE_MODE_R),                            \
            MAGIC_STATE_FLAG(F,MAGIC_STATE_MODE_W),                            \
            MAGIC_STATE_FLAG(F,MAGIC_STATE_MODE_X),                            \
            MAGIC_STATE_FLAG(F,MAGIC_STATE_EXTERNAL),                          \
            MAGIC_STATE_FLAG(F,MAGIC_STATE_IMMUTABLE));                        \
        MAGIC_TYPE_PRINT((F)->type, EXPAND_TYPE_STR);                          \
        _magic_printf(")");                                                    \
    } while(0)

/* Magic function hash macros. */
#define MAGIC_FUNCTION_TO_HASH_EL(function, function_hash)                     \
    do {                                                                       \
        function_hash->key = function->address;                                \
        function_hash->function = function;                                    \
    } while (0)

#define MAGIC_DFUNCTION_TO_HASH_EL(dfunction, function, function_hash)         \
    do {                                                                       \
        function_hash->key = function->address;                                \
        function_hash->function = function;                                    \
    } while (0)

/* Estimated maximum number of buckets needed. Increase as necessary. */
#define MAGIC_FUNCTION_ADDR_EST_MAX_BUCKETS 32768
/*
 * Since we don't support freeing memory, we need to allocate _all_ the
 * intermediate buckets as well. For simplicity, just assume 1 + 2 + 4 + ...
 * + 2^n, though it will probably be less than that.
 */
#define MAGIC_FUNCTION_ADDR_EST_TOTAL_BUCKETS                                  \
    ((MAGIC_FUNCTION_ADDR_EST_MAX_BUCKETS << 1) - 1)
#define MAGIC_FUNCTION_ADDR_HASH_OVERHEAD                                      \
    (MAGIC_FUNCTION_ADDR_EST_TOTAL_BUCKETS * sizeof(UT_hash_bucket) +          \
    sizeof(UT_hash_table))

/* Magic dynamic function macros. */
#define MAGIC_DFUNCTION_PREV(DF)          ((DF)->prev)
#define MAGIC_DFUNCTION_HAS_PREV(DF)      ((DF)->prev != NULL)
#define MAGIC_DFUNCTION_NEXT(DF)          ((DF)->next)
#define MAGIC_DFUNCTION_HAS_NEXT(DF)      ((DF)->next != NULL)
#define MAGIC_DFUNCTION_TO_FUNCTION(DF)   (&((DF)->function))
#define MAGIC_DFUNCTION_FROM_FUNCTION(F)                                       \
    ((struct _magic_dfunction*)(((char*)(F)) -                                 \
    offsetof(struct _magic_dfunction, function)))

#define MAGIC_DFUNCTION_MNUM              (~(0xFEE1DEAF))
#define MAGIC_DFUNCTION_MNUM_NULL         0

#define MAGIC_DFUNCTION_MNUM_OK(D) ((D)->magic_number == MAGIC_DFUNCTION_MNUM)

#define MAGIC_DFUNCTION_PRINT(DF, EXPAND_TYPE_STR) do {                        \
        _magic_printf("DFUNCTION: (~mnum=%08x, address=0x%08x, prev=0x%08x, "  \
            "next=0x%08x, function=", ~((DF)->magic_number), (unsigned) (DF),  \
            (unsigned) (DF)->prev, (unsigned) (DF)->next);                     \
        MAGIC_FUNCTION_PRINT(MAGIC_DFUNCTION_TO_FUNCTION(DF), EXPAND_TYPE_STR);\
        _magic_printf(")");                                                    \
    } while(0)

#define MAGIC_DFUNCTION_ITER(HEAD, DFUNC, DO) do {                             \
        if(HEAD) {                                                             \
            DFUNC = NULL;                                                      \
            while(!DFUNC || MAGIC_DFUNCTION_HAS_NEXT(DFUNC)) {                 \
                DFUNC = !DFUNC ? HEAD : MAGIC_DFUNCTION_NEXT(DFUNC);           \
                assert(magic_check_dfunction(DFUNC, 0)                         \
                    && "Bad magic dfunction looked up!");                      \
                DO                                                             \
            }                                                                  \
        }                                                                      \
    } while(0)

#define MAGIC_DFUNCTION_FUNC_ITER(HEAD, DFUNC, FUNC, DO)                       \
        MAGIC_DFUNCTION_ITER(HEAD, DFUNC,                                      \
            FUNC = MAGIC_DFUNCTION_TO_FUNCTION(DFUNC);                         \
            DO                                                                 \
        );

/* Magic dynamic state index macros. */
#define MAGIC_DSINDEX_ID(I) ((I) - _magic_dsindexes + 1)
#define MAGIC_DSINDEX_IS_ALLOC(I)                                              \
    ((I) && !MAGIC_STATE_FLAG(I, MAGIC_STATE_STACK))
#define MAGIC_DSINDEX_PRINT(I, EXPAND_TYPE_STR) do {                           \
        _magic_printf("DSINDEX: (id=%5d, name=%s, parent=%s, "                 \
        "flags(SHMs)=%d%d%d%d type=",                                          \
            MAGIC_DSINDEX_ID(I), (I)->name, (I)->parent_name,                  \
            MAGIC_STATE_FLAG((I), MAGIC_STATE_STACK), MAGIC_STATE_FLAG((I),    \
            MAGIC_STATE_HEAP), MAGIC_STATE_FLAG((I), MAGIC_STATE_MAP),         \
            MAGIC_STATE_FLAG((I), MAGIC_STATE_SHM));                           \
        MAGIC_TYPE_PRINT((I)->type, EXPAND_TYPE_STR);                          \
        _magic_printf(")");                                                    \
    } while(0)

/* Magic dynamic state entry macros. */
#define MAGIC_DSENTRY_PREV(DE)                  ((DE)->prev)
#define MAGIC_DSENTRY_HAS_PREV(DE)              ((DE)->prev != NULL)
#define MAGIC_DSENTRY_NEXT(DE)                  ((DE)->next)
#define MAGIC_DSENTRY_HAS_NEXT(DE)              ((DE)->next != NULL)
#define MAGIC_DSENTRY_HAS_EXT(DE)                                              \
    ((DE)->ext && MAGIC_STATE_FLAG(MAGIC_DSENTRY_TO_SENTRY(DE),                \
    MAGIC_STATE_EXT))
#define MAGIC_DSENTRY_TO_SENTRY(DE)             (&((DE)->sentry))
#define MAGIC_DSENTRY_FROM_SENTRY(E)                                           \
    ((struct _magic_dsentry*)(((char*)(E)) -                                   \
    offsetof(struct _magic_dsentry, sentry)))
#define MAGIC_DSENTRY_TO_TYPE_ARR(DE)           ((DE)->type_array)
#define MAGIC_DSENTRY_NEXT_MEMPOOL(DE)          ((DE)->next_mpool)
#define MAGIC_DSENTRY_NEXT_MEMBLOCK(DE)         ((DE)->next_mblock)
#define MAGIC_DSENTRY_HAS_NEXT_MEMPOOL(DE)      ((DE)->next_mpool != NULL)
#define MAGIC_DSENTRY_HAS_NEXT_MEMBLOCK(DE)     ((DE)->next_mblock != NULL)

#define MAGIC_DSENTRY_MSTATE_ALIVE              (~(0xFEE1DEAD))
#define MAGIC_DSENTRY_MSTATE_DEAD               0xFEE1DEAD
#define MAGIC_DSENTRY_MSTATE_FREED              0xDEADBEEF
#define MAGIC_DSENTRY_MNUM                      MAGIC_DSENTRY_MSTATE_ALIVE
#define MAGIC_DSENTRY_MNUM_NULL                 0
#define MAGIC_DSENTRY_SITE_ID_NULL              0

#define MAGIC_DSENTRY_MNUM_OK(D) ((D)->magic_number == MAGIC_DSENTRY_MNUM)
#define MAGIC_DSENTRY_MSTATE_OK(D)                                             \
    ((D)->magic_state == MAGIC_DSENTRY_MSTATE_ALIVE                            \
    || (D)->magic_state == MAGIC_DSENTRY_MSTATE_DEAD                           \
    || (D)->magic_state == MAGIC_DSENTRY_MSTATE_FREED)
#define MAGIC_DSENTRY_MSTATE_C(D)                                              \
    ((D)->magic_state == MAGIC_DSENTRY_MSTATE_ALIVE ? 'A'                      \
    : (D)->magic_state == MAGIC_DSENTRY_MSTATE_DEAD ? 'D'                      \
    : (D)->magic_state == MAGIC_DSENTRY_MSTATE_FREED ? 'F' : '?')

#define MAGIC_DSENTRY_PRINT(DE, EXPAND_TYPE_STR) do {                          \
        _magic_printf("DSENTRY: (~mnum=%08x, mstate=%c, address=0x%08x, "      \
            "site_id=" MAGIC_ID_FORMAT ", next=0x%08x, sentry=",               \
            ~((DE)->magic_number), MAGIC_DSENTRY_MSTATE_C(DE), (unsigned) (DE),\
            (DE)->site_id, (unsigned) (DE)->next);                             \
        MAGIC_SENTRY_PRINT(MAGIC_DSENTRY_TO_SENTRY(DE), EXPAND_TYPE_STR);      \
        _magic_printf(")");                                                    \
    } while(0)

/* Iterate through all the top-level dsentries. */
#define MAGIC_DSENTRY_ITER(HEAD,PREV_DSENTRY,DSENTRY,SENTRY,DO) do {           \
        if(HEAD) {                                                             \
            DSENTRY=NULL;                                                      \
            while(!DSENTRY || MAGIC_DSENTRY_HAS_NEXT(DSENTRY)) {               \
                PREV_DSENTRY = DSENTRY;                                        \
                DSENTRY = !DSENTRY ? HEAD : MAGIC_DSENTRY_NEXT(DSENTRY);       \
                assert(magic_check_dsentry(DSENTRY, 0)                         \
                    && "Bad magic dsentry looked up!");                        \
                SENTRY = MAGIC_DSENTRY_TO_SENTRY(DSENTRY);                     \
                DO                                                             \
            }                                                                  \
        }                                                                      \
    } while(0)

#define MAGIC_DSENTRY_ALIVE_ITER(HEAD,PREV_DSENTRY,DSENTRY,SENTRY,DO) do {     \
        MAGIC_DSENTRY_ITER(HEAD,PREV_DSENTRY,DSENTRY,SENTRY,                   \
            if((DSENTRY)->magic_state == MAGIC_DSENTRY_MSTATE_ALIVE) {       \
                DO                                                             \
            }                                                                  \
        );                                                                     \
    } while(0)

/* Iterate through all the top-level dsentries and nest at the block level. */
#define MAGIC_DSENTRY_NESTED_ITER(HEAD,PREV_DSENTRY,DSENTRY,SENTRY,DO) do {  \
        MAGIC_DSENTRY_ITER(HEAD,PREV_DSENTRY,DSENTRY,SENTRY,                  \
            DO                                                                 \
            if(magic_lookup_nested_dsentries                                  \
                && MAGIC_STATE_FLAG(SENTRY, MAGIC_STATE_MEMPOOL)) {           \
                struct _magic_dsentry *MEMPOOL_DSENTRY = DSENTRY;             \
                MAGIC_DSENTRY_MEMBLOCK_ITER(MEMPOOL_DSENTRY, DSENTRY, SENTRY, \
                    DO                                                         \
                );                                                             \
                DSENTRY = MEMPOOL_DSENTRY;                                     \
            }                                                                  \
        );                                                                     \
    } while(0)

#define MAGIC_DSENTRY_ALIVE_NESTED_ITER(HEAD,PREV_DSENTRY,DSENTRY,SENTRY,DO) \
    do {                                                                      \
        MAGIC_DSENTRY_NESTED_ITER(HEAD,PREV_DSENTRY,DSENTRY,SENTRY,           \
            if((DSENTRY)->magic_state == MAGIC_DSENTRY_MSTATE_ALIVE) {       \
                DO                                                             \
            }                                                                  \
        );                                                                     \
    } while(0)

/* Iterate through all the block-level dsentries. */
#define MAGIC_DSENTRY_BLOCK_ITER(HEAD,PREV_DSENTRY,DSENTRY,SENTRY,DO) do {   \
        MAGIC_DSENTRY_NESTED_ITER(HEAD,PREV_DSENTRY,DSENTRY,SENTRY,           \
            if(!MAGIC_STATE_FLAG(SENTRY, MAGIC_STATE_MEMPOOL)) {              \
                DO                                                             \
            }                                                                  \
        );                                                                     \
    } while(0)

#define MAGIC_DSENTRY_ALIVE_BLOCK_ITER(HEAD,PREV_DSENTRY,DSENTRY,SENTRY,DO) \
    do {                                                                      \
        MAGIC_DSENTRY_BLOCK_ITER(HEAD,PREV_DSENTRY,DSENTRY,SENTRY,           \
            if((DSENTRY)->magic_state == MAGIC_DSENTRY_MSTATE_ALIVE) {       \
                DO                                                             \
            }                                                                  \
        );                                                                     \
    } while(0)

#define MAGIC_DSENTRY_MEMPOOL_ITER(HEAD,PREV_DSENTRY,DSENTRY,SENTRY,DO) do {   \
        if(HEAD) {                                                             \
            DSENTRY=NULL;                                                      \
            while(!DSENTRY || MAGIC_DSENTRY_HAS_NEXT_MEMPOOL(DSENTRY)) {       \
                PREV_DSENTRY = DSENTRY;                                        \
                DSENTRY = !DSENTRY ? HEAD                                      \
                    : MAGIC_DSENTRY_NEXT_MEMPOOL(DSENTRY);                     \
                assert(magic_check_dsentry(DSENTRY, 0)                         \
                    && "Bad magic dsentry looked up!");                        \
                SENTRY = MAGIC_DSENTRY_TO_SENTRY(DSENTRY);                     \
                DO                                                             \
            }                                                                  \
        }                                                                      \
    } while(0)

#define MAGIC_DSENTRY_MEMPOOL_ALIVE_ITER(HEAD,PREV_DSENTRY,DSENTRY,SENTRY,DO)  \
    do {                                                                       \
        MAGIC_DSENTRY_MEMPOOL_ITER(HEAD,PREV_DSENTRY,DSENTRY,SENTRY,           \
            if((DSENTRY)->magic_state == MAGIC_DSENTRY_MSTATE_ALIVE) {         \
                DO                                                             \
            }                                                                  \
        );                                                                     \
    } while(0)

#define MAGIC_DSENTRY_MEMBLOCK_ITER(MEMPOOL_DSENTRY,                           \
    MEMBLOCK_DSENTRY,MEMBLOCK_SENTRY,DO)                                       \
    do {                                                                       \
        if(MEMPOOL_DSENTRY) {                                                  \
            assert(magic_check_dsentry(MEMPOOL_DSENTRY, 0)                     \
                && "Bad magic dsentry looked up!");                            \
            assert(MAGIC_STATE_FLAG(MAGIC_DSENTRY_TO_SENTRY(MEMPOOL_DSENTRY),  \
                MAGIC_STATE_MEMPOOL) && "Bad mempool dsentry looked up!");     \
            MEMBLOCK_DSENTRY = MAGIC_DSENTRY_NEXT_MEMBLOCK(MEMPOOL_DSENTRY);   \
            while (MEMBLOCK_DSENTRY && (MEMBLOCK_DSENTRY != MEMPOOL_DSENTRY)) {\
                assert(magic_check_dsentry(MEMBLOCK_DSENTRY, 0)                \
                    && "Bad magic dsentry looked up!");                        \
                MEMBLOCK_SENTRY = MAGIC_DSENTRY_TO_SENTRY(MEMBLOCK_DSENTRY);   \
                assert(MAGIC_STATE_FLAG(MEMBLOCK_SENTRY, MAGIC_STATE_MEMBLOCK) \
                    && "Bad memblock dsentry looked up!");                     \
                DO                                                             \
                MEMBLOCK_DSENTRY=MAGIC_DSENTRY_NEXT_MEMBLOCK(MEMBLOCK_DSENTRY);\
            }                                                                  \
        }                                                                      \
    } while(0)

#define MAGIC_DSENTRY_MEMPOOL_LOOKUP(MEMBLOCK_DSENTRY,MEMPOOL_DSENTRY) do {    \
        if (MEMBLOCK_DSENTRY) {                                                \
            struct _magic_dsentry *DSENTRY;                                    \
            struct _magic_sentry *SENTRY;                                      \
            DSENTRY = MEMBLOCK_DSENTRY;                                        \
            do {                                                               \
                assert(magic_check_dsentry(DSENTRY, 0)                         \
                    && "Bad magic dsentry looked up!");                        \
                SENTRY = MAGIC_DSENTRY_TO_SENTRY(DSENTRY);                     \
                if (MAGIC_STATE_FLAG(SENTRY, MAGIC_STATE_MEMPOOL)) {           \
                    MEMPOOL_DSENTRY = DSENTRY;                                 \
                    break;                                                     \
                }                                                              \
                DSENTRY = MAGIC_DSENTRY_NEXT_MEMBLOCK(DSENTRY);                \
            } while (DSENTRY != MEMBLOCK_DSENTRY);                             \
        }                                                                      \
    } while(0)

#define MAGIC_DSENTRY_ALIVE_NAME_ID_ITER(HEAD, PREV_DSENTRY, DSENTRY, SENTRY,  \
    PN, N, ID, DO) do {                                                        \
        MAGIC_DSENTRY_ALIVE_ITER(HEAD,PREV_DSENTRY,DSENTRY,SENTRY,             \
            if((!(PN) || !strcmp((DSENTRY)->parent_name, (PN)))                \
                && (!(N) || !strcmp((SENTRY)->name, (N)))                      \
                && (!(ID) || ((ID) == (DSENTRY)->site_id))) {                  \
                DO                                                             \
            }                                                                  \
        );                                                                     \
    } while(0)

#define MAGIC_DSENTRY_NUM(HEAD,NUM) do {                                       \
        struct _magic_dsentry *_prev_dsentry, *_dsentry;                       \
        struct _magic_sentry *_sentry;                                         \
        *(NUM) = 0;                                                            \
        MAGIC_DSENTRY_ITER(HEAD,_prev_dsentry,_dsentry,_sentry,(*(NUM))++;);   \
    } while(0)

#define MAGIC_DSENTRY_ALIVE_NUM(HEAD,NUM) do {                                 \
        struct _magic_dsentry *_prev_dsentry, *_dsentry;                       \
        struct _magic_sentry *_sentry;                                         \
        *(NUM) = 0;                                                            \
        MAGIC_DSENTRY_ALIVE_ITER(HEAD,_prev_dsentry,_dsentry,_sentry,(*(NUM))++;); \
    } while(0)

#define MAGIC_DSENTRY_BLOCK_NUM(HEAD,NUM) do {                                 \
        struct _magic_dsentry *_prev_dsentry, *_dsentry;                       \
        struct _magic_sentry *_sentry;                                         \
        *(NUM) = 0;                                                            \
        MAGIC_DSENTRY_BLOCK_ITER(HEAD,_prev_dsentry,_dsentry,_sentry,(*(NUM))++;); \
    } while(0)

#define MAGIC_DSENTRY_ALIVE_BLOCK_NUM(HEAD,NUM) do {                           \
        struct _magic_dsentry *_prev_dsentry, *_dsentry;                       \
        struct _magic_sentry *_sentry;                                         \
        *(NUM) = 0;                                                            \
        MAGIC_DSENTRY_ALIVE_BLOCK_ITER(HEAD,_prev_dsentry,_dsentry,_sentry,(*(NUM))++;); \
    } while(0)

#define MAGIC_DEAD_DSENTRIES_NEED_FREEING()                                    \
    (magic_num_dead_dsentries > MAGIC_MAX_DEAD_DSENTRIES                       \
    || magic_size_dead_dsentries > MAGIC_MAX_DEAD_DSENTRIES_SIZE)

#define MAGIC_SENTRY_OVERLAPS(S, START, END) \
    ((char*)(S)->address<=(char*)(END) \
    && (char*)(S)->address+(S)->type->size-1>=(char*)(START))

#define MAGIC_SENTRY_RANGE_ALIVE_BLOCK_ITER(SENTRY,START,END,DO) do { \
        int __i; \
        struct _magic_dsentry *__prev_dsentry, *__dsentry; \
	if (magic_sentry_rl_index) { \
	    char *__addr = NULL; \
	    SENTRY = NULL; \
	    while (1) { \
	        __addr = __addr ? SENTRY->address : ((char*)END) + 1; \
                SENTRY = magic_sentry_rl_pred_lookup(__addr); \
                if (!SENTRY || !MAGIC_SENTRY_OVERLAPS(SENTRY, START, ((char*)-1))) break; \
                DO \
            } \
            break; \
        } \
        for (__i=0;__i<_magic_sentries_num;__i++) { \
            SENTRY = &_magic_sentries[__i]; \
            if (MAGIC_SENTRY_OVERLAPS(SENTRY, START, END)) { DO } \
        } \
        MAGIC_DSENTRY_ALIVE_ITER(_magic_first_dsentry, __prev_dsentry, __dsentry, SENTRY, \
            if (MAGIC_SENTRY_OVERLAPS(SENTRY, START, END)) {                     \
                if(magic_lookup_nested_dsentries                                  \
                    && MAGIC_STATE_FLAG(SENTRY, MAGIC_STATE_MEMPOOL)) {           \
                    struct _magic_dsentry *MEMPOOL_DSENTRY = __dsentry;           \
                    struct _magic_dsentry *__dsentry2;                            \
                    MAGIC_DSENTRY_MEMBLOCK_ITER(MEMPOOL_DSENTRY, __dsentry2, SENTRY, \
                        if (MAGIC_SENTRY_OVERLAPS(SENTRY, START, END))  { DO } \
                    );                                                          \
                }                                                               \
                else { DO }                                                    \
            }                                                                   \
        ); \
    } while(0)

/* Magic out-of-band dynamic state entry macros. */
#define MAGIC_OBDSENTRY_TO_DSENTRY(OBDE)     (&((OBDE)->dsentry))
#define MAGIC_OBDSENTRY_FROM_DSENTRY(DE)                                       \
    ((struct _magic_obdsentry*)(((char*)(DE)) -                                \
    offsetof(struct _magic_obdsentry, dsentry)))
#define MAGIC_OBDSENTRY_TO_SENTRY(OBDE)                                        \
    MAGIC_DSENTRY_TO_SENTRY(MAGIC_OBDSENTRY_TO_DSENTRY(OBDE))
#define MAGIC_OBDSENTRY_FROM_SENTRY(E)                                         \
    MAGIC_OBDSENTRY_FROM_DSENTRY(MAGIC_DSENTRY_FROM_SENTRY(E))
#define MAGIC_OBDSENTRY_IS_FREE(OBDE)                                          \
    ((OBDE)->dsentry.sentry.flags == 0)
#define MAGIC_OBDSENTRY_FREE(OBDE)           ((OBDE)->dsentry.sentry.flags = 0)

/* Magic memory pool state entry macros. */
#define MAGIC_MPDESC_IS_FREE(POOL)        ((POOL)->is_alive == 0)
#define MAGIC_MPDESC_ALLOC(POOL)          ((POOL)->is_alive = 1)
#if MAGIC_MEM_USAGE_OUTPUT_CTL
#define MAGIC_MPDESC_FREE(POOL) do {                                           \
    (POOL)->is_alive = 0;                                                      \
    (POOL)->addr = NULL;                                                       \
    (POOL)->dtype_id = 0;                                                      \
} while (0)
#else
#define MAGIC_MPDESC_FREE(POOL) do {                                           \
    (POOL)->is_alive = 0;                                                      \
    (POOL)->addr = NULL;                                                       \
} while (0)
#endif

#define MAGIC_SELEMENT_MAX_PTR_RECURSIONS   100

#define MAGIC_SELEMENT_NAME_PRINT(E) do {                                      \
        if((E)->sentry) {                                                      \
            magic_type_walk_root((E)->sentry->type, 0,                         \
                (unsigned long) ((char*)(E)->address -                         \
                (char*)(E)->sentry->address), magic_selement_name_print_cb,    \
                (void*)__UNCONST((E)));                                        \
        }                                                                      \
        else {                                                                 \
            _magic_printf("???");                                              \
        }                                                                      \
    } while(0)
#define MAGIC_CHECK_TRG(T)                                                     \
        (magic_type_target_walk(T, NULL, NULL, magic_type_count_cb, NULL) >= 0)
#define MAGIC_SELEMENT_HAS_TRG(E)                                              \
        ((E)->type->size == sizeof(void*)                                      \
        && MAGIC_CHECK_TRG(*((void**)(E)->address)))
#define MAGIC_SELEMENT_PRINT(E, FLAGS) do {                                    \
        _magic_printf("SELEMENT: (parent=%s, num=%d, depth=%d, address=0x%08x,"\
            " name=", (E)->sentry ? (E)->sentry->name : "???", (E)->num,       \
            (E)->depth, (E)->address);                                         \
        MAGIC_SELEMENT_NAME_PRINT(E);                                          \
        _magic_printf(", type=");                                              \
        if ((E)->type) MAGIC_TYPE_PRINT((E)->type, FLAGS);                     \
        if(((FLAGS) & MAGIC_EXPAND_SENTRY) && (E)->sentry) {                   \
            _magic_printf(", sentry=");                                        \
            MAGIC_SENTRY_PRINT((E)->sentry, FLAGS);                            \
        }                                                                      \
        _magic_printf(")");                                                    \
    } while(0)

/* Magic external library descriptor macros. */
#define MAGIC_LIBDESC_PRINT(LD)                                                \
    _magic_printf("LIBDESC: (name=%s, text_range=[%p,%p], data_range=[%p,%p], "\
        "alloc_address=%p, alloc_size=%zu)", (LD)->name, (LD)->text_range[0],  \
        (LD)->text_range[1], (LD)->data_range[0], (LD)->data_range[1],         \
        (LD)->alloc_address, (LD)->alloc_size);

/* Magic SO library descriptor macros. */
#define MAGIC_SODESC_PREV(SD)          ((SD)->prev)
#define MAGIC_SODESC_HAS_PREV(SD)      ((SD)->prev != NULL)
#define MAGIC_SODESC_NEXT(SD)          ((SD)->next)
#define MAGIC_SODESC_HAS_NEXT(SD)      ((SD)->next != NULL)

#define MAGIC_SODESC_PRINT(SD) do {                                            \
        _magic_printf("SODESC: (address=%p, prev=%p, next=%p, ", (SD),         \
        (SD)->prev, (SD)->next);                                               \
        MAGIC_LIBDESC_PRINT(&((SD)->lib));                                     \
        _magic_printf(")");                                                    \
    } while(0)

#define MAGIC_SODESC_ITER(HEAD, SODESC, DO) do {                               \
        if (HEAD) {                                                            \
            SODESC = NULL;                                                     \
            while(!SODESC || MAGIC_SODESC_HAS_NEXT(SODESC)) {                  \
                SODESC = !SODESC ? HEAD : MAGIC_SODESC_NEXT(SODESC);           \
                DO                                                             \
            }                                                                  \
        }                                                                      \
    } while(0)

#define MAGIC_SODESC_ITER_SAFE(HEAD, SODESC, DO) do {                          \
        struct _magic_sodesc *sodesc_next;                                     \
        if (HEAD) {                                                            \
            SODESC = NULL;                                                     \
            while(!SODESC || sodesc_next != NULL) {                            \
                SODESC = !SODESC ? HEAD : sodesc_next;                         \
                sodesc_next = MAGIC_SODESC_NEXT(SODESC);                       \
                DO                                                             \
            }                                                                  \
        }                                                                      \
    } while(0)

/* Magic DSO library descriptor macros. */
#define MAGIC_DSODESC_PREV(DD)          ((DD)->prev)
#define MAGIC_DSODESC_HAS_PREV(DD)      ((DD)->prev != NULL)
#define MAGIC_DSODESC_NEXT(DD)          ((DD)->next)
#define MAGIC_DSODESC_HAS_NEXT(DD)      ((DD)->next != NULL)

#define MAGIC_DSODESC_PRINT(DD) do {                                           \
        _magic_printf("DSODESC: (address=%p, prev=%p, next=%p, handle=%p, "    \
            "ref_count=%d, ", (DD), (DD)->prev, (DD)->next, (DD)->handle,      \
            (DD)->ref_count);                                                  \
        MAGIC_LIBDESC_PRINT(&((DD)->lib));                                     \
        _magic_printf(")");                                                    \
    } while(0)

#define MAGIC_DSODESC_ITER(HEAD, DSODESC, DO) do {                             \
        if (HEAD) {                                                            \
            DSODESC = NULL;                                                    \
            while(!DSODESC || MAGIC_DSODESC_HAS_NEXT(DSODESC)) {               \
                DSODESC = !DSODESC ? HEAD : MAGIC_DSODESC_NEXT(DSODESC);       \
                DO                                                             \
            }                                                                  \
        }                                                                      \
    } while(0)

#define MAGIC_DSODESC_ITER_SAFE(HEAD, DSODESC, DO) do {                        \
        struct _magic_dsodesc *dsodesc_next;                                   \
        if (HEAD) {                                                            \
            DSODESC = NULL;                                                    \
            while(!DSODESC || dsodesc_next != NULL) {                          \
                DSODESC = !DSODESC ? HEAD : dsodesc_next;                      \
                dsodesc_next = MAGIC_DSODESC_NEXT(DSODESC);                    \
                DO                                                             \
            }                                                                  \
        }                                                                      \
    } while(0)

/* Magic value casts. */
#define MAGIC_VALUE_CAST_DEBUG MAGIC_DEBUG_SET(0)

#define MAGIC_CHECKED_VALUE_SRC_CAST(SV, ST, DV, DT, RP, CS)                   \
    do {                                                                       \
        ST value = SV;                                                         \
        DV = (DT) value;                                                       \
        if (((ST) (DV)) != value) {                                            \
            *(RP) = MAGIC_ERANGE;                                              \
        }                                                                      \
        else if(CS && value && (((DV) <= (DT) 0 && value >= (ST) 0) ||         \
            ((DV) >= (DT) 0 && value <= (ST) 0))) {                            \
            *(RP) = MAGIC_ESIGN;                                               \
        }                                                                      \
        if(MAGIC_VALUE_CAST_DEBUG) {                                           \
            _magic_printf("SRC ");                                             \
            MAGIC_SELEMENT_PRINT(src_selement, MAGIC_EXPAND_TYPE_STR);         \
            _magic_printf("\n");                                               \
            _magic_printf("DST ");                                             \
            MAGIC_SELEMENT_PRINT(dst_selement, MAGIC_EXPAND_TYPE_STR);         \
            _magic_printf("\n");                                               \
            _magic_printf("MAGIC_CHECKED_VALUE_SRC_CAST: "                     \
                "types=%s-->%s, value=", #ST, #DT);                            \
            magic_selement_print_value(src_selement);                          \
            _magic_printf("-->(long/unsigned long/void*)%d/%u/%08x, "          \
                "ERANGE=%d, ESIGN=%d\n", (long) (DV), (unsigned long) (DV),    \
                (unsigned long) (DV), *(RP) == MAGIC_ERANGE,                   \
                *(RP) == MAGIC_ESIGN);                                         \
        }                                                                      \
    } while(0)

#define MAGIC_CHECKED_VALUE_DST_CAST(SV, ST, DV, DT, RP)                       \
    do {                                                                       \
        DT value = (DT) SV;                                                    \
        if (((ST) value) != (SV)) {                                            \
            *(RP) = MAGIC_ERANGE;                                              \
        }                                                                      \
        *((DT*) (DV)) = value;                                                 \
        if (MAGIC_VALUE_CAST_DEBUG) {                                          \
            _magic_selement_t tmp_selement = *dst_selement;                    \
            tmp_selement.address = DV;                                         \
            _magic_printf("MAGIC_CHECKED_VALUE_DST_CAST: types=%s-->%s, "      \
                "value=(long/unsigned long/void*)%d/%u/%08x-->", #ST, #DT,     \
                (long) (SV), (unsigned long) (SV), (unsigned long) (SV));      \
            magic_selement_print_value(&tmp_selement);                         \
            _magic_printf(", ERANGE=%d\n", *(RP) == MAGIC_ERANGE);             \
        }                                                                      \
    } while(0)

/* Magic utility macros. */
#define MAGIC_ABS(X)                    ((X) >= 0 ? (X) : -(X))

/* Magic page size. */
#define MAGIC_PAGE_SIZE                 (magic_sys_pagesize ? magic_sys_pagesize : magic_get_sys_pagesize())
EXTERN unsigned long magic_sys_pagesize;

/* Magic sections. */
#define MAGIC_DATA_SECTION_START        ((void*)&__start_magic_data)
#define MAGIC_DATA_SECTION_END          ((void*)&__stop_magic_data)
#define MAGIC_DATA_RO_SECTION_START     ((void*)&__start_magic_data_ro)
#define MAGIC_DATA_RO_SECTION_END       ((void*)&__stop_magic_data_ro)
#define MAGIC_ST_DATA_SECTION_START     ((void*)&__start_magic_data_st)
#define MAGIC_ST_DATA_SECTION_END       ((void*)&__stop_magic_data_st)
#define MAGIC_ST_DATA_RO_SECTION_START  ((void*)&__start_magic_data_st_ro)
#define MAGIC_ST_DATA_RO_SECTION_END    ((void*)&__stop_magic_data_st_ro)
#define MAGIC_TEXT_SECTION_START        ((void*)&__start_magic_functions)
#define MAGIC_TEXT_SECTION_END          ((void*)&__stop_magic_functions)
#define MAGIC_ST_TEXT_SECTION_START     ((void*)&__start_magic_functions_st)
#define MAGIC_ST_TEXT_SECTION_END       ((void*)&__stop_magic_functions_st)
EXTERN void* __start_magic_data;
EXTERN void* __stop_magic_data;
EXTERN void* __start_magic_data_ro;
EXTERN void* __stop_magic_data_ro;
EXTERN void* __start_magic_data_st;
EXTERN void* __stop_magic_data_st;
EXTERN void* __start_magic_data_st_ro;
EXTERN void* __stop_magic_data_st_ro;
EXTERN void* __start_magic_functions;
EXTERN void* __stop_magic_functions;
EXTERN void* __start_magic_functions_st;
EXTERN void* __stop_magic_functions_st;

#if MAGIC_THREAD_SAFE
#if MAGIC_FORCE_LOCKS
#define MAGIC_DSENTRY_LOCK()     magic_dsentry_lock(magic_dsentry_lock_args)
#define MAGIC_DSENTRY_UNLOCK()   magic_dsentry_unlock(magic_dsentry_unlock_args)
#define MAGIC_DFUNCTION_LOCK()   magic_dfunction_lock(magic_dfunction_lock_args)
#define MAGIC_DFUNCTION_UNLOCK()                                               \
    magic_dfunction_unlock(magic_dfunction_unlock_args)
#define MAGIC_DSODESC_LOCK()     magic_dsodesc_lock(magic_dsodesc_lock_args)
#define MAGIC_DSODESC_UNLOCK()   magic_dsodesc_unlock(magic_dsodesc_unlock_args)
#define MAGIC_MPDESC_LOCK()      magic_mpdesc_lock(magic_mpdesc_lock_args)
#define MAGIC_MPDESC_UNLOCK()    magic_mpdesc_unlock(magic_mpdesc_unlock_args)
#else
#define MAGIC_GENERIC_LOCK(LOCK,ARGS)                                          \
    do {                                                                       \
        int l;                                                                 \
        if (LOCK) {                                                            \
            l = LOCK(ARGS);                                                    \
            assert(l == 0 && "bad lock");                                      \
        }                                                                      \
    } while (0)

#define MAGIC_DSENTRY_LOCK()                                                   \
    MAGIC_GENERIC_LOCK(magic_dsentry_lock,magic_dsentry_lock_args)
#define MAGIC_DSENTRY_UNLOCK()                                                 \
    MAGIC_GENERIC_LOCK(magic_dsentry_unlock,magic_dsentry_unlock_args)
#define MAGIC_DFUNCTION_LOCK()                                                 \
    MAGIC_GENERIC_LOCK(magic_dfunction_lock,magic_dfunction_lock_args)
#define MAGIC_DFUNCTION_UNLOCK()                                               \
    MAGIC_GENERIC_LOCK(magic_dfunction_unlock,magic_dfunction_unlock_args)
#define MAGIC_DSODESC_LOCK()                                                   \
    MAGIC_GENERIC_LOCK(magic_dsodesc_lock,magic_dsodesc_lock_args)
#define MAGIC_DSODESC_UNLOCK()                                                 \
    MAGIC_GENERIC_LOCK(magic_dsodesc_unlock,magic_dsodesc_unlock_args)
#define MAGIC_MPDESC_LOCK()                                                    \
    MAGIC_GENERIC_LOCK(magic_mpdesc_lock,magic_mpdesc_lock_args)
#define MAGIC_MPDESC_UNLOCK()                                                  \
    MAGIC_GENERIC_LOCK(magic_mpdesc_unlock,magic_mpdesc_unlock_args)
#endif
#define MAGIC_MULTIPLE_LOCK(DS, DF, DSO, MP)                                   \
    do {                                                                       \
        if (DS)                                                                \
            MAGIC_DSENTRY_LOCK();                                              \
        if (DF)                                                                \
            MAGIC_DFUNCTION_LOCK();                                            \
        if (DSO)                                                               \
            MAGIC_DSODESC_LOCK();                                              \
        if (MP)                                                                \
            MAGIC_MPDESC_LOCK();                                               \
    } while (0)
#define MAGIC_MULTIPLE_UNLOCK(DS, DF, DSO, MP)                                 \
    do {                                                                       \
        if (MP)                                                                \
            MAGIC_MPDESC_UNLOCK();                                             \
        if (DSO)                                                               \
            MAGIC_DSODESC_UNLOCK();                                            \
        if (DF)                                                                \
            MAGIC_DFUNCTION_UNLOCK();                                          \
        if (DS)                                                                \
            MAGIC_DSENTRY_UNLOCK();                                            \
    } while (0)
#define MAGIC_MULTIPLE_LOCK_BLOCK(DS, DF, DSO, MP, BLOCK)                      \
    do {                                                                       \
        MAGIC_MULTIPLE_LOCK(DS, DF, DSO, MP);                                  \
        BLOCK                                                                  \
        MAGIC_MULTIPLE_UNLOCK(DS, DF, DSO, MP);                                \
    } while (0)
#else
#define MAGIC_DSENTRY_LOCK()
#define MAGIC_DSENTRY_UNLOCK()
#define MAGIC_DFUNCTION_LOCK()
#define MAGIC_DFUNCTION_UNLOCK()
#define MAGIC_DSODESC_LOCK()
#define MAGIC_DSODESC_UNLOCK()
#define MAGIC_MPDESC_LOCK()
#define MAGIC_MPDESC_UNLOCK()
#define MAGIC_MULTIPLE_LOCK(DS, DF, DSO, MP)
#define MAGIC_MULTIPLE_UNLOCK(DS, DF, DSO, MP)
#define MAGIC_MULTIPLE_LOCK_BLOCK(DS, DF, DSO, MP, BLOCK)                      \
    do {                                                                       \
        BLOCK                                                                  \
    } while (0)
#endif

/* Debug. */
#define MAGIC_DEBUG_HIGH        3
#define MAGIC_DEBUG_AVG         2
#define MAGIC_DEBUG_LOW         1
#define MAGIC_DEBUG_NONE        0
#define MAGIC_DEBUG                                                            \
    MAGIC_DEBUG_SELECT(MAGIC_DEBUG_HIGH, MAGIC_DEBUG_NONE)

#if MAGIC_DEBUG
#define MAGIC_DEBUG_CODE(L, X)                                                 \
    do {                                                                       \
        if(L > MAGIC_DEBUG) break;                                             \
        X                                                                      \
    } while(0)
#else
#define MAGIC_DEBUG_CODE(L, X)
#endif

/* Magic Address Space Randomization (ASRPass) */
#define _magic_asr_seed (_magic_vars->asr_seed)
#define _magic_asr_heap_map_do_permutate (                                     \
    _magic_vars->asr_heap_map_do_permutate)
#define _magic_asr_heap_max_offset (_magic_vars->asr_heap_max_offset)
#define _magic_asr_heap_max_padding (_magic_vars->asr_heap_max_padding)
#define _magic_asr_map_max_offset_pages (_magic_vars->asr_map_max_offset_pages)
#define _magic_asr_map_max_padding_pages (                                     \
    _magic_vars->asr_map_max_padding_pages)

/* Runtime flags. */
#define _magic_no_mem_inst              (_magic_vars->no_mem_inst)

/* Magic type array. */
#define _magic_types                    (_magic_vars->types)
#define _magic_types_num                (_magic_vars->types_num)
#define _magic_types_next_id            (_magic_vars->types_next_id)

/* Magic function array. */
#define _magic_functions                (_magic_vars->functions)
#define _magic_functions_num            (_magic_vars->functions_num)
#define _magic_functions_next_id        (_magic_vars->functions_next_id)

/* Magic dynamic function list. */
#define _magic_first_dfunction          (_magic_vars->first_dfunction)
#define _magic_last_dfunction           (_magic_vars->last_dfunction)
#define _magic_dfunctions_num           (_magic_vars->dfunctions_num)

/* Magic functions hash vars. */
#define magic_function_hash_buff        (_magic_vars->function_hash_buff)
#define magic_function_hash_buff_offset (_magic_vars->function_hash_buff_offset)
#define magic_function_hash_buff_size   (_magic_vars->function_hash_buff_size)
#define magic_function_hash_head        (_magic_vars->function_hash_head)

/* Magic dynamic state index array. */
#define _magic_dsindexes (_magic_vars->dsindexes)
#define _magic_dsindexes_num (_magic_vars->dsindexes_num)

/* Magic dynamic state entry list. */
#define _magic_first_dsentry (_magic_vars->first_dsentry)
#define magic_num_dead_dsentries (_magic_vars->num_dead_dsentries)
#define magic_size_dead_dsentries (_magic_vars->size_dead_dsentries)

/* Magic memory pool dynamic state entry list. */
#define _magic_first_mempool_dsentry (_magic_vars->first_mempool_dsentry)

/* Magic SO library descriptor list. */
#define _magic_first_sodesc (_magic_vars->first_sodesc)
#define _magic_last_sodesc (_magic_vars->last_sodesc)
#define _magic_sodescs_num (_magic_vars->sodescs_num)

/* Magic DSO library descriptor list. */
#define _magic_first_dsodesc (_magic_vars->first_dsodesc)
#define _magic_last_dsodesc (_magic_vars->last_dsodesc)
#define _magic_dsodescs_num (_magic_vars->dsodescs_num)

/* Magic stack-related variables. */
#define _magic_first_stack_dsentry (_magic_vars->first_stack_dsentry)
#define _magic_last_stack_dsentry (_magic_vars->last_stack_dsentry)

/* Magic unmap-memory variables. */
#ifdef __MINIX
#define _magic_unmap_mem (_magic_vars->unmap_mem)
#endif

/* Magic default stubs. */
EXTERN struct _magic_type magic_default_type;
EXTERN struct _magic_dsentry magic_default_dsentry;
EXTERN struct _magic_dfunction magic_default_dfunction;
EXTERN struct _magic_type magic_default_ret_addr_type;

/* Magic vars references. */
EXTERN struct _magic_vars_t _magic_vars_buff;
EXTERN struct _magic_vars_t *_magic_vars;

FUNCTION_BLOCK(

/* Magic vars wrappers. */
PUBLIC void *_magic_vars_addr(void);
PUBLIC size_t _magic_vars_size(void);

/* Magic printf. */
PUBLIC int magic_null_printf(const char* format, ...);
PUBLIC int magic_err_printf(const char* format, ...);
PUBLIC void magic_set_printf(printf_ptr_t func_ptr);
PUBLIC printf_ptr_t magic_get_printf(void);
PUBLIC void magic_assert_failed(const char *assertion, const char *file,
    const char *function, const int line);

/* Magic utility functions. */
PUBLIC unsigned long magic_get_sys_pagesize(void);

/* Magic lock primitives. */
typedef int (*magic_lock_t)(void*);
typedef int (*magic_unlock_t)(void*);

EXTERN magic_lock_t magic_dsentry_lock;
EXTERN magic_unlock_t magic_dsentry_unlock;
EXTERN void *magic_dsentry_lock_args;
EXTERN void *magic_dsentry_unlock_args;

EXTERN magic_lock_t magic_dfunction_lock;
EXTERN magic_unlock_t magic_dfunction_unlock;
EXTERN void *magic_dfunction_lock_args;
EXTERN void *magic_dfunction_unlock_args;

EXTERN magic_lock_t magic_dsodesc_lock;
EXTERN magic_unlock_t magic_dsodesc_unlock;
EXTERN void *magic_dsodesc_lock_args;
EXTERN void *magic_dsodesc_unlock_args;

EXTERN magic_lock_t magic_mpdesc_lock;
EXTERN magic_unlock_t magic_mpdesc_unlock;
EXTERN void *magic_mpdesc_lock_args;
EXTERN void *magic_mpdesc_unlock_args;

PUBLIC void magic_dsentry_set_lock_primitives(magic_lock_t lock,
    magic_unlock_t unlock, void *lock_args, void *unlock_args);
PUBLIC void magic_dfunction_set_lock_primitives(magic_lock_t lock,
    magic_unlock_t unlock, void *lock_args, void *unlock_args);
PUBLIC void magic_dsodesc_set_lock_primitives(magic_lock_t lock,
    magic_unlock_t unlock, void *lock_args, void *unlock_args);
PUBLIC void magic_mpdesc_set_lock_primitives(magic_lock_t lock,
    magic_unlock_t unlock, void *lock_args, void *unlock_args);

/*
 * Magic void ptr and array (force at the least 1 void* and 1 void array in the
 * list of globals).
 */
EXTERN void* MAGIC_VOID_PTR;
EXTERN char MAGIC_VOID_ARRAY[1];

/* Magic special types. */
EXTERN struct _magic_type *MAGIC_VOID_PTR_TYPE;
EXTERN struct _magic_type *MAGIC_VOID_PTR_INT_CAST_TYPE;
EXTERN struct _magic_type *MAGIC_VOID_ARRAY_TYPE;
EXTERN struct _magic_type *MAGIC_PTRINT_TYPE;
EXTERN struct _magic_type *MAGIC_PTRINT_ARRAY_TYPE;

/* Magic annotations. */
EXTERN VOLATILE int MAGIC_CALL_ANNOTATION_VAR;

/* Magic status variables. */
EXTERN int magic_init_done;
EXTERN int magic_libcommon_active;
EXTERN int magic_lookup_nested_dsentries;
EXTERN int magic_allow_dead_dsentries;
EXTERN int magic_ignore_dead_dsentries;
EXTERN int magic_mmap_dsentry_header_prot;
EXTERN int _magic_enabled;
EXTERN int _magic_checkpoint_enabled;
EXTERN int _magic_lazy_checkpoint_enabled;

/* Magic page size. */
EXTERN unsigned long magic_sys_pagesize;

/* Initialization functions. */
PUBLIC void magic_init(void);
PUBLIC void magic_stack_init(void);

/* Dfunction functions. */
PUBLIC int magic_check_dfunction(struct _magic_dfunction *ptr, int flags);
PUBLIC int magic_check_dfunctions(void);
PUBLIC int magic_check_dfunctions_safe(void);
PUBLIC void magic_print_dfunction(struct _magic_dfunction *dfunction);
PUBLIC void magic_print_dfunctions(void);
PUBLIC void magic_print_dfunctions_safe(void);
PUBLIC void magic_copy_dfunction(struct _magic_dfunction *dfunction,
    struct _magic_dfunction *dst_dfunction);

/* Dsindex functions. */
PUBLIC void magic_print_dsindex(struct _magic_dsindex *dsindex);
PUBLIC void magic_print_dsindexes(void);

/* Dsentry functions. */
PUBLIC int magic_check_dsentry(struct _magic_dsentry *ptr, int flags);
PUBLIC int magic_check_dsentries(void);
PUBLIC int magic_check_dsentries_safe(void);
PUBLIC void magic_print_dsentry(struct _magic_dsentry *dsentry);
PUBLIC void magic_print_dsentries(void);
PUBLIC void magic_print_dsentries_safe(void);
PUBLIC void magic_copy_dsentry(struct _magic_dsentry *dsentry,
    struct _magic_dsentry *dst_dsentry);

/* Sodesc functions. */
PUBLIC void magic_print_sodesc(struct _magic_sodesc *sodesc);
PUBLIC void magic_print_sodescs(void);

/* Dsodesc functions. */
PUBLIC void magic_print_dsodesc(struct _magic_dsodesc *dsodesc);
PUBLIC void magic_print_dsodescs(void);
PUBLIC void magic_print_dsodescs_safe(void);

/* Section functions. */
PUBLIC void magic_print_sections(void);

/* Lookup functions. */
PUBLIC struct _magic_sentry* magic_mempool_sentry_lookup_by_range(void *addr,
    struct _magic_dsentry *dsentry_buff);
PUBLIC struct _magic_dsindex*
    magic_dsindex_lookup_by_name(const char *parent_name, const char *name);
PUBLIC struct _magic_dsentry*
    magic_dsentry_prev_lookup(struct _magic_dsentry* dsentry);
PUBLIC struct _magic_dsentry*
    magic_mempool_dsentry_prev_lookup(struct _magic_dsentry* dsentry);
PUBLIC struct _magic_function* magic_function_lookup_by_id(_magic_id_t id,
    struct _magic_dfunction *dfunction_buff);
PUBLIC struct _magic_function* magic_function_lookup_by_addr(void *addr,
    struct _magic_dfunction *dfunction_buff);
PUBLIC struct _magic_function*
    magic_function_lookup_by_name(const char *parent_name, const char *name);
PUBLIC struct _magic_type* magic_type_lookup_by_name(const char *name);
PUBLIC struct _magic_dsodesc* magic_dsodesc_lookup_by_handle(void *handle);
PUBLIC int magic_selement_lookup_by_name(char* name,
    _magic_selement_t *selement, struct _magic_dsentry *dsentry_buff);

/* Magic state function functions. */
PUBLIC void magic_print_function(struct _magic_function *function);
PUBLIC void magic_print_functions(void);

/* Magic state function lookup hash functions. */
PUBLIC void magic_function_hash_build(void *buff, size_t buff_size);
PUBLIC void magic_function_hash_destroy(void);
PUBLIC size_t magic_function_hash_estimate_buff_size(int functions_num);
PUBLIC struct _magic_function *magic_function_lookup_by_addr_hash(void *addr,
    struct _magic_dfunction *dfunction_buff);
PUBLIC void *magic_function_hash_alloc(size_t size);
PUBLIC void magic_function_hash_dealloc(void *object, size_t size);

/* Magic state type functions. */
PUBLIC void magic_print_type(const struct _magic_type* type);
PUBLIC void magic_print_types(void);
PUBLIC void magic_type_str_set_print_style(const int style);
PUBLIC int magic_type_str_get_print_style(void);
PUBLIC void magic_type_str_print(const struct _magic_type* type);
PUBLIC void magic_type_values_print(const struct _magic_type* type);
PUBLIC void magic_type_names_print(const struct _magic_type* type);
PUBLIC void magic_type_comp_types_print(const struct _magic_type* type,
    int flags);
PUBLIC int magic_type_str_print_from_target(void* target);
PUBLIC int magic_type_equals(const struct _magic_type* type,
    const struct _magic_type* other_type);
PUBLIC int magic_type_compatible(const struct _magic_type* type,
    const struct _magic_type* other_type, int flags);
PUBLIC int magic_type_comp_compatible(const struct _magic_type* type,
    const struct _magic_type* other_type);
PUBLIC int magic_type_ptr_is_text(const struct _magic_type* ptr_type);
PUBLIC int magic_type_ptr_is_data(const struct _magic_type* ptr_type);
PUBLIC int magic_type_alloc_needs_varsized_array(const struct _magic_type* type,
    size_t alloc_size, int *num_elements);
PUBLIC size_t
magic_type_alloc_get_varsized_array_size(const struct _magic_type* type,
    int num_elements);
PUBLIC void magic_type_parse_varsized_array(const struct _magic_type *type,
    const struct _magic_type **sub_struct_type,
    const struct _magic_type **sub_array_type,
    size_t *sub_array_offset,
    size_t *sub_array_size);

/* Magic type walk functions. */
typedef int (*magic_type_walk_cb_t)(const struct _magic_type*, unsigned, int,
    const struct _magic_type*, const unsigned, int, void*);
PUBLIC int magic_type_walk_flags(const struct _magic_type* parent_type,
    unsigned parent_offset, int child_num, const struct _magic_type* type,
    unsigned offset, const unsigned min_offset, const unsigned max_offset,
    const magic_type_walk_cb_t cb, void* cb_args, int flags);
PUBLIC int magic_type_target_walk(void* target,
    struct _magic_dsentry** trg_dsentry,
    struct _magic_dfunction** trg_dfunction,
    const magic_type_walk_cb_t cb, void* cb_args);
PUBLIC int magic_type_walk_as_void_array(const struct _magic_type* parent_type,
    unsigned parent_offset, int child_num, const struct _magic_type* type,
    unsigned offset, const unsigned min_offset, const unsigned max_offset,
    const magic_type_walk_cb_t cb, void* cb_args);
PUBLIC int
magic_type_walk_as_ptrint_array(const struct _magic_type* parent_type,
    unsigned parent_offset, int child_num, const struct _magic_type* type,
    void* offset_addr, unsigned offset, const unsigned min_offset,
    const unsigned max_offset, const magic_type_walk_cb_t cb, void* cb_args);
PUBLIC void magic_type_walk_step(const struct _magic_type *type,
    int child_num, const struct _magic_type **child_type,
    unsigned *child_offset, int walk_flags);

/* Magic type walk callbacks. */
PUBLIC int magic_type_str_print_cb(const struct _magic_type* parent_type,
    const unsigned parent_offset, int child_num, const struct _magic_type* type,
    const unsigned offset, int depth, void* cb_args);
PUBLIC int magic_selement_name_print_cb(const struct _magic_type* parent_type,
    const unsigned parent_offset, int child_num, const struct _magic_type* type,
    const unsigned offset, int depth, void* cb_args);
PUBLIC int magic_selement_name_get_cb(const struct _magic_type *parent_type,
    const unsigned parent_offset, int child_num, const struct _magic_type *type,
    const unsigned offset, int depth, void *args_array);
PUBLIC int magic_type_count_cb(const struct _magic_type* parent_type,
    const unsigned parent_offset, int child_num, const struct _magic_type* type,
    const unsigned offset, int depth, void* cb_args);
PUBLIC int magic_type_child_offset_cb(const struct _magic_type* parent_type,
    const unsigned parent_offset, int child_num, const struct _magic_type* type,
    const unsigned offset, int depth, void* cb_args);

)

/* Magic type walk helpers. */
#define magic_type_walk(parent_type, parent_offset, child_num, type, offset,   \
    min_offset, max_offset, cb, cb_args)                                       \
    magic_type_walk_flags(parent_type, parent_offset, child_num, type, offset, \
    min_offset, max_offset, cb, cb_args, MAGIC_TYPE_WALK_DEFAULT_FLAGS)
#define magic_type_walk_root(type, min_offset, max_offset, cb, cb_args)        \
    magic_type_walk(NULL, 0, 0, type, 0, min_offset, max_offset, cb, cb_args)
#define magic_type_walk_root_at_offset(type, offset, cb, cb_args)              \
    magic_type_walk_root(type, offset, offset, cb, cb_args)
#define magic_type_walk_root_all(type, cb, cb_args)                            \
    magic_type_walk_root(type, 0, ULONG_MAX, cb, cb_args)

/* Magic size functions. */
PUBLIC size_t magic_type_get_size(struct _magic_type *type, int flags);
PUBLIC size_t magic_types_get_size(int flags);
PUBLIC size_t magic_function_get_size(struct _magic_function *function,
    int flags);
PUBLIC size_t magic_functions_get_size(int flags);
PUBLIC size_t magic_dfunctions_get_size(int flags);
PUBLIC size_t magic_sentry_get_size(struct _magic_sentry *sentry, int flags);
PUBLIC size_t magic_sentries_get_size(int flags);
PUBLIC size_t magic_dsentries_get_size(int flags);
PUBLIC size_t magic_dsindex_get_size(struct _magic_dsindex *dsindex,
    int flags);
PUBLIC size_t magic_dsindexes_get_size(int flags);
PUBLIC size_t magic_sodesc_get_size(struct _magic_sodesc *sodesc,
    int flags);
PUBLIC size_t magic_sodescs_get_size(int flags);
PUBLIC size_t magic_dsodesc_get_size(struct _magic_dsodesc *dsodesc,
    int flags);
PUBLIC size_t magic_dsodescs_get_size(int flags);
PUBLIC size_t magic_metadata_get_size(int flags);
PUBLIC size_t magic_sentries_data_get_size(int flags);
PUBLIC size_t magic_dsentries_data_get_size(int flags);
PUBLIC size_t magic_other_data_get_size(int flags);
PUBLIC size_t magic_data_get_size(int flags);
PUBLIC void magic_print_size_stats(int flags);

#define MAGIC_SIZE_VALUE_SET                0x0001
#define MAGIC_SIZE_NAMES                    0x0002
#define MAGIC_SIZE_DSENTRY_NAMES            0x0004
#define MAGIC_SIZE_DSINDEX_NAMES            0x0008
#define MAGIC_SIZE_TYPE_NAMES               0x0010
#define MAGIC_SIZE_MEMBER_NAMES             0x0020
#define MAGIC_SIZE_COMP_TYPES               0x0040
#define MAGIC_SIZE_ALL (MAGIC_SIZE_VALUE_SET | MAGIC_SIZE_NAMES |              \
    MAGIC_SIZE_DSENTRY_NAMES | MAGIC_SIZE_DSINDEX_NAMES | MAGIC_SIZE_TYPE_NAMES\
    | MAGIC_SIZE_MEMBER_NAMES | MAGIC_SIZE_COMP_TYPES)

#endif

/* Magic reentrant functions. */
PUBLIC void magic_reentrant_enable(void);
PUBLIC void magic_reentrant_disable(void);
