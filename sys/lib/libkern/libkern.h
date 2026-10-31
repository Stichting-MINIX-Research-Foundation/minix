/*	$NetBSD: libkern.h,v 1.130 2018/09/03 16:29:35 riastradh Exp $	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)libkern.h	8.2 (Berkeley) 8/5/94
 */

#ifndef _LIB_LIBKERN_LIBKERN_H_
#define _LIB_LIBKERN_LIBKERN_H_

#ifdef _KERNEL_OPT
#include "opt_kasan.h"
#include "opt_kcsan.h"
#include "opt_kmsan.h"
#endif

#include <sys/types.h>
#include <sys/inttypes.h>
#include <sys/null.h>

#include <lib/libkern/strlist.h>

#ifndef LIBKERN_INLINE
#define LIBKERN_INLINE	static __inline
#define LIBKERN_BODY
#endif

LIBKERN_INLINE int imax(int, int) __unused;
LIBKERN_INLINE int imin(int, int) __unused;
LIBKERN_INLINE u_int uimax(u_int, u_int) __unused;
LIBKERN_INLINE u_int uimin(u_int, u_int) __unused;
LIBKERN_INLINE long lmax(long, long) __unused;
LIBKERN_INLINE long lmin(long, long) __unused;
LIBKERN_INLINE u_long ulmax(u_long, u_long) __unused;
LIBKERN_INLINE u_long ulmin(u_long, u_long) __unused;
LIBKERN_INLINE int abs(int) __unused;
LIBKERN_INLINE long labs(long) __unused;
LIBKERN_INLINE long long llabs(long long) __unused;
LIBKERN_INLINE intmax_t imaxabs(intmax_t) __unused;

LIBKERN_INLINE int isspace(int) __unused;
LIBKERN_INLINE int isascii(int) __unused;
LIBKERN_INLINE int isupper(int) __unused;
LIBKERN_INLINE int islower(int) __unused;
LIBKERN_INLINE int isalpha(int) __unused;
LIBKERN_INLINE int isalnum(int) __unused;
LIBKERN_INLINE int isdigit(int) __unused;
LIBKERN_INLINE int isxdigit(int) __unused;
LIBKERN_INLINE int iscntrl(int) __unused;
LIBKERN_INLINE int isgraph(int) __unused;
LIBKERN_INLINE int isprint(int) __unused;
LIBKERN_INLINE int ispunct(int) __unused;
LIBKERN_INLINE int toupper(int) __unused;
LIBKERN_INLINE int tolower(int) __unused;

#ifdef LIBKERN_BODY
LIBKERN_INLINE int
imax(int a, int b)
{
	return (a > b ? a : b);
}
LIBKERN_INLINE int
imin(int a, int b)
{
	return (a < b ? a : b);
}
LIBKERN_INLINE long
lmax(long a, long b)
{
	return (a > b ? a : b);
}
LIBKERN_INLINE long
lmin(long a, long b)
{
	return (a < b ? a : b);
}
LIBKERN_INLINE u_int
uimax(u_int a, u_int b)
{
	return (a > b ? a : b);
}
LIBKERN_INLINE u_int
uimin(u_int a, u_int b)
{
	return (a < b ? a : b);
}
LIBKERN_INLINE u_long
ulmax(u_long a, u_long b)
{
	return (a > b ? a : b);
}
LIBKERN_INLINE u_long
ulmin(u_long a, u_long b)
{
	return (a < b ? a : b);
}

LIBKERN_INLINE int
abs(int j)
{
	return(j < 0 ? -j : j);
}

LIBKERN_INLINE long
labs(long j)
{
	return(j < 0 ? -j : j);
}

LIBKERN_INLINE long long
llabs(long long j)
{
	return(j < 0 ? -j : j);
}

LIBKERN_INLINE intmax_t
imaxabs(intmax_t j)
{
	return(j < 0 ? -j : j);
}

LIBKERN_INLINE int
isspace(int ch)
{
	return (ch == ' ' || (ch >= '\t' && ch <= '\r'));
}

LIBKERN_INLINE int
isascii(int ch)
{
	return ((ch & ~0x7f) == 0);
}

LIBKERN_INLINE int
isupper(int ch)
{
	return (ch >= 'A' && ch <= 'Z');
}

LIBKERN_INLINE int
islower(int ch)
{
	return (ch >= 'a' && ch <= 'z');
}

LIBKERN_INLINE int
isalpha(int ch)
{
	return (isupper(ch) || islower(ch));
}

LIBKERN_INLINE int
isalnum(int ch)
{
	return (isalpha(ch) || isdigit(ch));
}

LIBKERN_INLINE int
isdigit(int ch)
{
	return (ch >= '0' && ch <= '9');
}

LIBKERN_INLINE int
isxdigit(int ch)
{
	return (isdigit(ch) ||
	    (ch >= 'A' && ch <= 'F') ||
	    (ch >= 'a' && ch <= 'f'));
}

LIBKERN_INLINE int
iscntrl(int ch)
{
	return ((ch >= 0x00 && ch <= 0x1F) || ch == 0x7F);
}

LIBKERN_INLINE int
isgraph(int ch)
{
	return (ch != ' ' && isprint(ch));
}

LIBKERN_INLINE int
isprint(int ch)
{
	return (ch >= 0x20 && ch <= 0x7E);
}

LIBKERN_INLINE int
ispunct(int ch)
{
	return (isprint(ch) && ch != ' ' && !isalnum(ch));
}

LIBKERN_INLINE int
toupper(int ch)
{
	if (islower(ch))
		return (ch - 0x20);
	return (ch);
}

LIBKERN_INLINE int
tolower(int ch)
{
	if (isupper(ch))
		return (ch + 0x20);
	return (ch);
}
#endif

#define	__NULL_STMT		do { } while (/* CONSTCOND */ 0)

#define __KASSERTSTR  "kernel %sassertion \"%s\" failed: file \"%s\", line %d "

#ifdef NDEBUG						/* tradition! */
#define	assert(e)	((void)0)
#else
#define	assert(e)	(__predict_true((e)) ? (void)0 :		    \
			    kern_assert(__KASSERTSTR, "", #e, __FILE__, __LINE__))
#endif

#ifdef __COVERITY__
#ifndef DIAGNOSTIC
#define DIAGNOSTIC
#endif
#endif

#ifndef	CTASSERT
#define	CTASSERT(x)		__CTASSERT(x)
#endif
#ifndef	CTASSERT_SIGNED
#define	CTASSERT_SIGNED(x)	__CTASSERT(((typeof(x))-1) < 0)
#endif
#ifndef	CTASSERT_UNSIGNED
#define	CTASSERT_UNSIGNED(x)	__CTASSERT(((typeof(x))-1) >= 0)
#endif

#ifndef DIAGNOSTIC
#define _DIAGASSERT(a)	(void)0
#ifdef lint
#define	KASSERTMSG(e, msg, ...)	/* NOTHING */
#define	KASSERT(e)		/* NOTHING */
#else /* !lint */
/*
 * Make sure the expression compiles, but don't evaluate any of it.  We
 * use sizeof to inhibit evaluation, and cast to long so the expression
 * can be integer- or pointer-valued without bringing in other header
 * files.
 */
#define	KASSERTMSG(e, msg, ...)	((void)sizeof((long)(e)))
#define	KASSERT(e)		((void)sizeof((long)(e)))
#endif /* !lint */
#else /* DIAGNOSTIC */
#define _DIAGASSERT(a)	assert(a)
#define	KASSERTMSG(e, msg, ...)		\
			(__predict_true((e)) ? (void)0 :		    \
			    kern_assert(__KASSERTSTR msg, "diagnostic ", #e,	    \
				__FILE__, __LINE__, ## __VA_ARGS__))

#define	KASSERT(e)	(__predict_true((e)) ? (void)0 :		    \
			    kern_assert(__KASSERTSTR, "diagnostic ", #e,	    \
				__FILE__, __LINE__))
#endif

#ifndef DEBUG
#ifdef lint
#define	KDASSERTMSG(e,msg, ...)	/* NOTHING */
#define	KDASSERT(e)		/* NOTHING */
#else /* lint */
#define	KDASSERTMSG(e,msg, ...)	((void)0)
#define	KDASSERT(e)		((void)0)
#endif /* lint */
#else
#define	KDASSERTMSG(e, msg, ...)	\
			(__predict_true((e)) ? (void)0 :		    \
			    kern_assert(__KASSERTSTR msg, "debugging ", #e,	    \
				__FILE__, __LINE__, ## __VA_ARGS__))

#define	KDASSERT(e)	(__predict_true((e)) ? (void)0 :		    \
			    kern_assert(__KASSERTSTR, "debugging ", #e,	    \
				__FILE__, __LINE__))
#endif

/*
 * XXX: For compatibility we use SMALL_RANDOM by default.
 */
#define SMALL_RANDOM

#ifndef offsetof
#if __GNUC_PREREQ__(4, 0)
#define offsetof(type, member)	__builtin_offsetof(type, member)
#else
#define	offsetof(type, member) \
    ((size_t)(unsigned long)(&(((type *)0)->member)))
#endif
#endif

/*
 * Return the container of an embedded struct.  Given x = &c->f,
 * container_of(x, T, f) yields c, where T is the type of c.  Example:
 *
 *	struct foo { ... };
 *	struct bar {
 *		int b_x;
 *		struct foo b_foo;
 *		...
 *	};
 *
 *	struct bar b;
 *	struct foo *fp = b.b_foo;
 *
 * Now we can get at b from fp by:
 *
 *	struct bar *bp = container_of(fp, struct bar, b_foo);
 *
 * The 0*sizeof((PTR) - ...) causes the compiler to warn if the type of
 * *fp does not match the type of struct bar::b_foo.
 * We skip the validation for coverity runs to avoid warnings.
 */
#if defined(__COVERITY__) || defined(__LGTM_BOT__)
#define __validate_container_of(PTR, TYPE, FIELD) 0
#define __validate_const_container_of(PTR, TYPE, FIELD) 0
#else
#define __validate_container_of(PTR, TYPE, FIELD)			\
    (0 * sizeof((PTR) - &((TYPE *)(((char *)(PTR)) -			\
    offsetof(TYPE, FIELD)))->FIELD))
#define __validate_const_container_of(PTR, TYPE, FIELD)			\
    (0 * sizeof((PTR) - &((const TYPE *)(((const char *)(PTR)) -	\
    offsetof(TYPE, FIELD)))->FIELD))
#endif

#define	container_of(PTR, TYPE, FIELD)					\
    ((TYPE *)(((char *)(PTR)) - offsetof(TYPE, FIELD))			\
	+ __validate_container_of(PTR, TYPE, FIELD))
#define	const_container_of(PTR, TYPE, FIELD)				\
    ((const TYPE *)(((const char *)(PTR)) - offsetof(TYPE, FIELD))	\
	+ __validate_const_container_of(PTR, TYPE, FIELD))

/* Prototypes for which GCC built-ins exist. */
void	*memcpy(void *, const void *, size_t);
int	 memcmp(const void *, const void *, size_t);
void	*memset(void *, int, size_t);
#if __GNUC_PREREQ__(2, 95) && !defined(_STANDALONE)
#if defined(_KERNEL) && defined(KASAN)
void	*kasan_memcpy(void *, const void *, size_t);
int	 kasan_memcmp(const void *, const void *, size_t);
void	*kasan_memset(void *, int, size_t);
#define	memcpy(d, s, l)		kasan_memcpy(d, s, l)
#define	memcmp(a, b, l)		kasan_memcmp(a, b, l)
#define	memset(d, v, l)		kasan_memset(d, v, l)
#elif defined(_KERNEL) && defined(KCSAN)
void	*kcsan_memcpy(void *, const void *, size_t);
int	 kcsan_memcmp(const void *, const void *, size_t);
void	*kcsan_memset(void *, int, size_t);
#define	memcpy(d, s, l)		kcsan_memcpy(d, s, l)
#define	memcmp(a, b, l)		kcsan_memcmp(a, b, l)
#define	memset(d, v, l)		kcsan_memset(d, v, l)
#elif defined(_KERNEL) && defined(KMSAN)
void	*kmsan_memcpy(void *, const void *, size_t);
int	 kmsan_memcmp(const void *, const void *, size_t);
void	*kmsan_memset(void *, int, size_t);
#define	memcpy(d, s, l)		kmsan_memcpy(d, s, l)
#define	memcmp(a, b, l)		kmsan_memcmp(a, b, l)
#define	memset(d, v, l)		kmsan_memset(d, v, l)
#else
#define	memcpy(d, s, l)		__builtin_memcpy(d, s, l)
#define	memcmp(a, b, l)		__builtin_memcmp(a, b, l)
#define	memset(d, v, l)		__builtin_memset(d, v, l)
#endif
#endif
void	*memmem(const void *, size_t, const void *, size_t);

char	*strcpy(char *, const char *);
int	 strcmp(const char *, const char *);
size_t	 strlen(const char *);
#if __GNUC_PREREQ__(2, 95) && !defined(_STANDALONE)
#if defined(_KERNEL) && defined(KASAN)
char	*kasan_strcpy(char *, const char *);
int	 kasan_strcmp(const char *, const char *);
size_t	 kasan_strlen(const char *);
#define	strcpy(d, s)		kasan_strcpy(d, s)
#define	strcmp(a, b)		kasan_strcmp(a, b)
#define	strlen(a)		kasan_strlen(a)
#elif defined(_KERNEL) && defined(KCSAN)
char	*kcsan_strcpy(char *, const char *);
int	 kcsan_strcmp(const char *, const char *);
size_t	 kcsan_strlen(const char *);
#define	strcpy(d, s)		kcsan_strcpy(d, s)
#define	strcmp(a, b)		kcsan_strcmp(a, b)
#define	strlen(a)		kcsan_strlen(a)
#elif defined(_KERNEL) && defined(KMSAN)
char	*kmsan_strcpy(char *, const char *);
int	 kmsan_strcmp(const char *, const char *);
size_t	 kmsan_strlen(const char *);
#define	strcpy(d, s)		kmsan_strcpy(d, s)
#define	strcmp(a, b)		kmsan_strcmp(a, b)
#define	strlen(a)		kmsan_strlen(a)
#else
#define	strcpy(d, s)		__builtin_strcpy(d, s)
#define	strcmp(a, b)		__builtin_strcmp(a, b)
#define	strlen(a)		__builtin_strlen(a)
#endif
#endif
size_t	 strnlen(const char *, size_t);
char	*strsep(char **, const char *);

/* Functions for which we always use built-ins. */
#ifdef __GNUC__
#define	alloca(s)		__builtin_alloca(s)
#endif

/* These exist in GCC 3.x, but we don't bother. */
char	*strcat(char *, const char *);
char	*strchr(const char *, int);
char	*strrchr(const char *, int);
#if defined(_KERNEL) && defined(KASAN)
char	*kasan_strcat(char *, const char *);
char	*kasan_strchr(const char *, int);
char	*kasan_strrchr(const char *, int);
#define	strcat(d, s)		kasan_strcat(d, s)
#define	strchr(s, c)		kasan_strchr(s, c)
#define	strrchr(s, c)		kasan_strrchr(s, c)
#elif defined(_KERNEL) && defined(KMSAN)
char	*kmsan_strcat(char *, const char *);
char	*kmsan_strchr(const char *, int);
char	*kmsan_strrchr(const char *, int);
#define	strcat(d, s)		kmsan_strcat(d, s)
#define	strchr(s, c)		kmsan_strchr(s, c)
#define	strrchr(s, c)		kmsan_strrchr(s, c)
#endif
size_t	 strcspn(const char *, const char *);
char	*strncpy(char *, const char *, size_t);
char	*strncat(char *, const char *, size_t);
int	 strncmp(const char *, const char *, size_t);
char	*strstr(const char *, const char *);
char	*strpbrk(const char *, const char *);
size_t	 strspn(const char *, const char *);

/*
 * ffs is an instruction on vax.
 */
int	 ffs(int);
#if __GNUC_PREREQ__(2, 95) && (!defined(__vax__) || __GNUC_PREREQ__(4,1))
#define	ffs(x)		__builtin_ffs(x)
#endif

void	 kern_assert(const char *, ...)
    __attribute__((__format__(__printf__, 1, 2)));
u_int32_t
	inet_addr(const char *);
struct in_addr;
int	inet_aton(const char *, struct in_addr *);
char	*intoa(u_int32_t);
#define inet_ntoa(a) intoa((a).s_addr)
void	*memchr(const void *, int, size_t);

void	*memmove(void *, const void *, size_t);
#if defined(_KERNEL) && defined(KASAN)
void	*kasan_memmove(void *, const void *, size_t);
#define	memmove(d, s, l)	kasan_memmove(d, s, l)
#elif defined(_KERNEL) && defined(KCSAN)
void	*kcsan_memmove(void *, const void *, size_t);
#define	memmove(d, s, l)	kcsan_memmove(d, s, l)
#elif defined(_KERNEL) && defined(KMSAN)
void	*kmsan_memmove(void *, const void *, size_t);
#define	memmove(d, s, l)	kmsan_memmove(d, s, l)
#endif

int	 pmatch(const char *, const char *, const char **);
#ifndef SMALL_RANDOM
void	 srandom(unsigned long);
char	*initstate(unsigned long, char *, size_t);
char	*setstate(char *);
#endif /* SMALL_RANDOM */
long	 random(void);
void	 mi_vector_hash(const void * __restrict, size_t, uint32_t,
	    uint32_t[3]);
int	 scanc(u_int, const u_char *, const u_char *, int);
int	 skpc(int, size_t, u_char *);
int	 strcasecmp(const char *, const char *);
size_t	 strlcpy(char *, const char *, size_t);
size_t	 strlcat(char *, const char *, size_t);
int	 strncasecmp(const char *, const char *, size_t);
u_long	 strtoul(const char *, char **, int);
long long strtoll(const char *, char **, int);
unsigned long long strtoull(const char *, char **, int);
intmax_t  strtoimax(const char *, char **, int);
uintmax_t strtoumax(const char *, char **, int);
intmax_t strtoi(const char * __restrict, char ** __restrict, int, intmax_t,
    intmax_t, int *);
uintmax_t strtou(const char * __restrict, char ** __restrict, int, uintmax_t,
    uintmax_t, int *);
void	 hexdump(void (*)(const char *, ...) __printflike(1, 2),
    const char *, const void *, size_t);

int	 snprintb(char *, size_t, const char *, uint64_t);
int	 snprintb_m(char *, size_t, const char *, uint64_t, size_t);
int	 kheapsort(void *, size_t, size_t, int (*)(const void *, const void *),
		   void *);
uint32_t crc32(uint32_t, const uint8_t *, size_t);
#if __GNUC_PREREQ__(4, 5) \
    && (defined(__alpha_cix__) || defined(__mips_popcount))
#define	popcount	__builtin_popcount
#define	popcountl	__builtin_popcountl
#define	popcountll	__builtin_popcountll
#define	popcount32	__builtin_popcount
#define	popcount64	__builtin_popcountll
#else
unsigned int	popcount(unsigned int) __constfunc;
unsigned int	popcountl(unsigned long) __constfunc;
unsigned int	popcountll(unsigned long long) __constfunc;
unsigned int	popcount32(uint32_t) __constfunc;
unsigned int	popcount64(uint64_t) __constfunc;
#endif

void	*explicit_memset(void *, int, size_t);
int	consttime_memequal(const void *, const void *, size_t);
int	strnvisx(char *, size_t, const char *, size_t, int);
#define VIS_OCTAL	0x01
#define VIS_SAFE	0x20
#define VIS_TRIM	0x40

struct disklabel;
void	disklabel_swap(struct disklabel *, struct disklabel *);
uint16_t dkcksum(const struct disklabel *);
uint16_t dkcksum_sized(const struct disklabel *, size_t);

#endif /* !_LIB_LIBKERN_LIBKERN_H_ */
