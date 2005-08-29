/*	stdint.h - Standard sized integer types.	Author: Kees J. Bot
 *								4 Oct 2003
 *
 * Assumption:	Long is the biggest type.
 * Bug:		C99 requires a 64 bit type, and long isn't 64 bits yet, and
 *		will never be 64 bits under 16-bits Minix.
 * Omission:	Limits like PTR_DIFF_MAX not here yet, maybe <limits.h>?
 */

#ifndef _STDINT_H
#define _STDINT_H

#ifndef _MINIX__TYPES_H
#include <sys/types.h>
#endif
#include <minix/sys_config.h>

#if (_WORD_SIZE != 2 && _WORD_SIZE != 4) || \
	(_PTR_SIZE != _WORD_SIZE && _PTR_SIZE != 2*_WORD_SIZE)
#error Odd word or pointer sizes
#endif

/* Integer types of precisely the given bitsize. */
typedef i8_t	int8_t;
typedef i16_t	int16_t;
typedef i32_t	int32_t;
#if _WORD_SIZE > 2 && __L64
typedef i64_t	int64_t;
#endif

typedef u8_t	uint8_t;
typedef u16_t	uint16_t;
typedef u32_t	uint32_t;
#if _WORD_SIZE > 2 && __L64
typedef u64_t	uint64_t;
#endif

/* Integer types of at least the given bitsize. */
typedef int8_t		int_least8_t;
typedef int16_t		int_least16_t;
typedef int32_t		int_least32_t;
#if _WORD_SIZE > 2 && __L64
typedef int64_t		int_least64_t;
#endif

typedef uint8_t		uint_least8_t;
typedef uint16_t	uint_least16_t;
typedef uint32_t	uint_least32_t;
#if _WORD_SIZE > 2 && __L64
typedef uint64_t	uint_least64_t;
#endif

/* Fast integer types of at least the given bitsize. */
#if _WORD_SIZE == 2
typedef int16_t		int_fast8_t;
typedef int16_t		int_fast16_t;
#else
typedef int32_t		int_fast8_t;
typedef int32_t		int_fast16_t;
#endif
typedef int32_t		int_fast32_t;
#if _WORD_SIZE > 2 && __L64
typedef int64_t		int_fast64_t;
#endif

#if _WORD_SIZE == 2
typedef uint16_t	uint_fast8_t;
typedef uint16_t	uint_fast16_t;
#else
typedef uint32_t	uint_fast8_t;
typedef uint32_t	uint_fast16_t;
#endif
typedef uint32_t	uint_fast32_t;
#if _WORD_SIZE > 2 && __L64
typedef uint64_t	uint_fast64_t;
#endif

/* Integer type capable of holding a pointer and the largest integer type. */
#if _PTR_SIZE == _WORD_SIZE
typedef int		intptr_t;
typedef unsigned	uintptr_t;
#elif _PTR_SIZE == 2*_WORD_SIZE
typedef long		intptr_t;
typedef unsigned long	uintptr_t;
#endif
typedef long		intmax_t;
typedef unsigned long	uintmax_t;

#if !__cplusplus || defined(__STDC_LIMIT_MACROS)
#ifndef _LIMITS_H
#include <limits.h>
#endif

/* Range definitions for each of the above types conform <limits.h>. */
#define INT8_MIN		(-INT8_MAX-1)
#define INT16_MIN		(-INT16_MAX-1)
#define INT32_MIN		(-INT32_MAX-1)
#if _WORD_SIZE > 2 && __L64
#define INT64_MIN		(-INT64_MAX-1)
#endif

#define INT8_MAX		127
#define INT16_MAX		32767
#define INT32_MAX		2147483647
#if _WORD_SIZE > 2 && __L64
#define INT64_MAX		9223372036854775807
#endif

#define UINT8_MAX		255
#define UINT16_MAX		65535
#define UINT32_MAX		4294967295
#if _WORD_SIZE > 2 && __L64
#define UINT64_MAX		18446744073709551615
#endif

#define INT_LEAST8_MIN		INT8_MIN
#define INT_LEAST16_MIN		INT16_MIN
#define INT_LEAST32_MIN		INT32_MIN
#if _WORD_SIZE > 2 && __L64
#define INT_LEAST64_MIN		INT64_MIN
#endif

#define INT_LEAST8_MAX		INT8_MAX
#define INT_LEAST16_MAX		INT16_MAX
#define INT_LEAST32_MAX		INT32_MAX
#if _WORD_SIZE > 2 && __L64
#define INT_LEAST64_MAX		INT64_MAX
#endif

#define UINT_LEAST8_MAX		UINT8_MAX
#define UINT_LEAST16_MAX	UINT16_MAX
#define UINT_LEAST32_MAX	UINT32_MAX
#if _WORD_SIZE > 2 && __L64
#define UINT_LEAST64_MAX	UINT64_MAX
#endif

#define INT_FAST8_MIN		(-INT_FAST8_MAX-1)
#define INT_FAST16_MIN		(-INT_FAST16_MAX-1)
#define INT_FAST32_MIN		INT32_MIN
#if _WORD_SIZE > 2 && __L64
#define INT_FAST64_MIN		INT64_MIN
#endif

#if _WORD_SIZE == 2
#define INT_FAST8_MAX		INT16_MAX
#define INT_FAST16_MAX		INT16_MAX
#else
#define INT_FAST8_MAX		INT32_MAX
#define INT_FAST16_MAX		INT32_MAX
#endif
#define INT_FAST32_MAX		INT32_MAX
#if _WORD_SIZE > 2 && __L64
#define INT_FAST64_MAX		INT64_MAX
#endif

#if _WORD_SIZE == 2
#define UINT_FAST8_MAX		UINT16_MAX
#define UINT_FAST16_MAX		UINT16_MAX
#else
#define UINT_FAST8_MAX		UINT32_MAX
#define UINT_FAST16_MAX		UINT32_MAX
#endif
#define UINT_FAST32_MAX		UINT32_MAX
#if _WORD_SIZE > 2 && __L64
#define UINT_FAST64_MAX		UINT64_MAX
#endif

#if _PTR_SIZE == _WORD_SIZE
#define INTPTR_MIN		INT_MIN
#define INTPTR_MAX		INT_MAX
#define UINTPTR_MAX		UINT_MAX
#elif _PTR_SIZE > _WORD_SIZE
#define INTPTR_MIN		LONG_MIN
#define INTPTR_MAX		LONG_MAX
#define UINTPTR_MAX		ULONG_MAX
#endif
#define INTMAX_MIN		LONG_MIN
#define INTMAX_MAX		LONG_MAX
#define UINTMAX_MAX		ULONG_MAX

#endif /* !__cplusplus || __STDC_LIMIT_MACROS */

#ifndef __CONCAT
#define __CONCAT(x,y)	x ## y
#endif

/* Constants of the proper type. */
#define INT8_C(c)	c
#define INT16_C(c)	c
#if _WORD_SIZE == 2
#define INT32_C(c)	__CONCAT(c,l)
#else
#define INT32_C(c)	c
#endif
#if _WORD_SIZE > 2 && __L64
#define INT64_C(c)	__CONCAT(c,l)
#endif

#define UINT8_C(c)	__CONCAT(c,u)
#define UINT16_C(c)	__CONCAT(c,u)
#if _WORD_SIZE == 2
#define UINT32_C(c)	__CONCAT(c,lu)
#else
#define UINT32_C(c)	__CONCAT(c,u)
#endif
#if _WORD_SIZE > 2 && __L64
#define UINT64_C(c)	__CONCAT(c,lu)
#endif

#if _WORD_SIZE == 2 || !__L64
#define INTMAX_C(c)	INT32_C(c)
#define UINTMAX_C(c)	UINT32_C(c)
#else
#define INTMAX_C(c)	INT64_C(c)
#define UINTMAX_C(c)	UINT64_C(c)
#endif

#endif /* _STDINT_H */

/*
 * $PchId: stdint.h,v 1.2 2005/01/27 17:32:00 philip Exp $
 */
