#ifndef _MAGIC_DEF_H
#define _MAGIC_DEF_H

#if defined(_MINIX) || defined(_MINIX_SYSTEM)
#ifndef __MINIX
#define __MINIX 1
#endif
#endif

#include <limits.h>

/* Type macros. */
#ifdef __MINIX
#define MAGIC_LONG_LONG_SUPPORTED     1
#define MAGIC_LONG_DOUBLE_SUPPORTED   0
#else
#ifdef LLONG_MAX
#define MAGIC_LONG_LONG_SUPPORTED     1
#endif
#ifdef __LDBL_MAX__
#define MAGIC_LONG_DOUBLE_SUPPORTED   1
#endif
#endif

/* Modifier macros. */
#define EXTERN  extern
#define PRIVATE static
#ifdef __SHARED__
#define PUBLIC EXTERN
#else
#define PUBLIC
#endif

#ifdef __MINIX
#define INLINE __inline__
#define THREAD_LOCAL
#undef UNUSED
#define FUNCTION_BLOCK(B) B
#else
#define INLINE inline
#define THREAD_LOCAL __thread
#ifdef __cplusplus
#define FUNCTION_BLOCK(B) extern "C"{ B }
#else
#define FUNCTION_BLOCK(B) B
#endif
#endif

#define UNUSED(VAR) VAR __attribute__((unused))
#define USED __attribute__((used))
#define VOLATILE volatile

/* Magic macros. */
#define MAGIC_VAR USED
#define MAGIC_FUNC PUBLIC USED __attribute__((noinline))
#define MAGIC_FUNC_BODY() __asm__("")
#define MAGIC_HOOK PUBLIC USED __attribute__((always_inline)) inline
#define MAGIC_MACRO_FUNC static __attribute__((always_inline))

#define TRUE  1
#define FALSE 0

#ifdef __MINIX
#define SYS_PAGESIZE 4096
#else
#define SYS_PAGESIZE sysconf(_SC_PAGESIZE)
#endif
#define MAGIC_ROUND_DOWN(val, round)        ((val) & ~((round) - 1))
#define MAGIC_ROUND_UP(val, round)           (MAGIC_ROUND_DOWN(val, round) ==  \
    (val) ? (val) : MAGIC_ROUND_DOWN((val) + (round), (round)))
#define MAGIC_ROUND_DOWN_TO_PAGESIZE(addr)  MAGIC_ROUND_DOWN(addr, SYS_PAGESIZE)
#define MAGIC_ROUND_UP_TO_PAGESIZE(addr)    MAGIC_ROUND_UP(addr, SYS_PAGESIZE)

#ifdef __MINIX
#define _MAGIC_CAS(P, O, N) (*(P) == (O) ? *(P)=(N) : (N)+1)
#define MAGIC_CAS(P, O, N)  (_MAGIC_CAS(P, O, N) == (N) ? (O) : *(P))
#define _MAGIC_PCAS(P, O, N) (*(P) == (O) ? *(P)=(N) : (void *)((intptr_t)(N)+1))
#define MAGIC_PCAS(P, O, N)  (_MAGIC_PCAS(P, O, N) == (N) ? (O) : *(P))
#define MAGIC_FAA(P, V) (((*P)+=V)-V)
#define MAGIC_FAS(P, V) (((*P)-=V)+V)
#else
#define MAGIC_CAS(P, O, N) __sync_val_compare_and_swap((P), (O), (N))
#define MAGIC_PCAS(P, O, N) MAGIC_CAS(P, O, N)
#define MAGIC_FAA(P, V) __sync_fetch_and_add(P, V)
#define MAGIC_FAS(P, V) __sync_fetch_and_sub(P, V)
#endif

/* Magic arch-specific macros. */
#define MAGIC_FRAMEADDR_TO_RETADDR_PTR(F) (((char*)(F))+4)

/* Magic ranges. */
#define MAGIC_LINKER_VAR_NAMES   "end", "etext", "edata", NULL

#ifdef __MINIX
#define MAGIC_TEXT_START         ((void*)(0x1000))
#define MAGIC_STACK_GAP          (4*1024)
#else
#define MAGIC_TEXT_START         ((void*)(0x08048000))
#define MAGIC_STACK_GAP          (4*1024*1024)
#endif

#define MAGIC_TEXT_END           0    /* 0 if right before data. */
#define MAGIC_HEAP_START         0    /* 0 if right after data.  */
#define MAGIC_HEAP_GAP           (4*1024)
#define MAGIC_RANGE_ROUND_DATA   1
#define MAGIC_RANGE_ROUND_TEXT   1
#define MAGIC_RANGE_ROUND_STACK  1

/* Magic IDs */
#define MAGIC_ID_NONE 0
#define MAGIC_ID_FORCE_LONG 1
#if defined(__MINIX) || MAGIC_ID_FORCE_LONG
typedef unsigned long _magic_id_t;
#define MAGIC_ID_MAX    ULONG_MAX
#define MAGIC_ID_FORMAT "%lu"
#else
typedef unsigned long long _magic_id_t;
#define MAGIC_ID_MAX ULLONG_MAX
#define MAGIC_ID_FORMAT "%llu"
#endif

/* Magic error codes. */
#define MAGIC_ENOENT      (-100)
#define MAGIC_EBADENT     (-101)
#define MAGIC_EBADMSTATE  (-102)
#define MAGIC_EINVAL      (-103)
#define MAGIC_EGENERIC    (-104)
#define MAGIC_EBADWALK    (-105)
#define MAGIC_ERANGE      (-106)
#define MAGIC_ESIGN       (-107)
#define MAGIC_ENOMEM      (-108)
#define MAGIC_ENOPTR      ((void*)-1)

/*
 * Basic return type definitions. Not really needed, but they make
 * the code easier to read.
 */
#ifndef OK
#define OK                                  0
#endif
#ifndef EGENERIC
#define EGENERIC                            -1
#endif

/* Magic printf. */
#ifdef __MINIX
#define MAGIC_PRINTF_DEFAULT printf
#else
#define MAGIC_PRINTF_DEFAULT magic_err_printf
#endif

FUNCTION_BLOCK(

typedef int (*printf_ptr_t) (char const *str, ...);
EXTERN printf_ptr_t _magic_printf;
EXTERN void magic_assert_failed(const char *assertion, const char *file,
    const char *function, const int line);

)

/* assert() override. */
#define ENABLE_ASSERTIONS 1
#ifndef __MINIX
#define CUSTOM_ASSERTIONS 0
#else
#define CUSTOM_ASSERTIONS 1
#endif

#include <assert.h>

#if CUSTOM_ASSERTIONS
#ifdef assert
#undef assert
#endif
#ifndef __ASSERT_FUNCTION
#define __ASSERT_FUNCTION ""
#endif

#if ENABLE_ASSERTIONS
#define assert(X) do{       \
        if(!(X)) {          \
            magic_assert_failed(#X, __FILE__, __ASSERT_FUNCTION, __LINE__); \
        }                   \
    } while(0)
#else
#  define assert(X)
#endif
#endif

#endif

