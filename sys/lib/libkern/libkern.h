/*	$NetBSD: libkern.h,v 1.104 2011/11/28 08:05:06 tls Exp $	*/

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

#include <sys/types.h>
#include <sys/inttypes.h>
#include <sys/null.h>

#ifndef LIBKERN_INLINE
#define LIBKERN_INLINE	static __inline
#define LIBKERN_BODY
#endif

LIBKERN_INLINE int imax(int, int) __unused;
LIBKERN_INLINE int imin(int, int) __unused;
LIBKERN_INLINE u_int max(u_int, u_int) __unused;
LIBKERN_INLINE u_int min(u_int, u_int) __unused;
LIBKERN_INLINE long lmax(long, long) __unused;
LIBKERN_INLINE long lmin(long, long) __unused;
LIBKERN_INLINE u_long ulmax(u_long, u_long) __unused;
LIBKERN_INLINE u_long ulmin(u_long, u_long) __unused;
LIBKERN_INLINE int abs(int) __unused;

LIBKERN_INLINE int isspace(int) __unused;
LIBKERN_INLINE int isascii(int) __unused;
LIBKERN_INLINE int isupper(int) __unused;
LIBKERN_INLINE int islower(int) __unused;
LIBKERN_INLINE int isalpha(int) __unused;
LIBKERN_INLINE int isdigit(int) __unused;
LIBKERN_INLINE int isxdigit(int) __unused;
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
max(u_int a, u_int b)
{
	return (a > b ? a : b);
}
LIBKERN_INLINE u_int
min(u_int a, u_int b)
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

#define	CTASSERT(x)		__CTASSERT(x)

#ifndef DIAGNOSTIC
#define _DIAGASSERT(a)	(void)0
#ifdef lint
#define	KASSERTMSG(e, msg, ...)	/* NOTHING */
#define	KASSERT(e)		/* NOTHING */
#else /* !lint */
#define	KASSERTMSG(e, msg, ...)	((void)0)
#define	KASSERT(e)		((void)0)
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

#define	MTPRNG_RLEN		624
struct mtprng_state {
	unsigned int mt_idx; 
	uint32_t mt_elem[MTPRNG_RLEN];
	uint32_t mt_count;
	uint32_t mt_sparse[3];
};

/* Prototypes for which GCC built-ins exist. */
void	*memcpy(void *, const void *, size_t);
int	 memcmp(const void *, const void *, size_t);
void	*memset(void *, int, size_t);
#if __GNUC_PREREQ__(2, 95) && !defined(_STANDALONE)
#define	memcpy(d, s, l)		__builtin_memcpy(d, s, l)
#define	memcmp(a, b, l)		__builtin_memcmp(a, b, l)
#endif
#if __GNUC_PREREQ__(2, 95) && !defined(_STANDALONE)
#define	memset(d, v, l)		__builtin_memset(d, v, l)
#endif

char	*strcpy(char *, const char *);
int	 strcmp(const char *, const char *);
size_t	 strlen(const char *);
size_t	 strnlen(const char *, size_t);
char	*strsep(char **, const char *);
#if __GNUC_PREREQ__(2, 95) && !defined(_STANDALONE)
#define	strcpy(d, s)		__builtin_strcpy(d, s)
#define	strcmp(a, b)		__builtin_strcmp(a, b)
#define	strlen(a)		__builtin_strlen(a)
#endif

/* Functions for which we always use built-ins. */
#ifdef __GNUC__
#define	alloca(s)		__builtin_alloca(s)
#endif

/* These exist in GCC 3.x, but we don't bother. */
char	*strcat(char *, const char *);
char	*strncpy(char *, const char *, size_t);
int	 strncmp(const char *, const char *, size_t);
char	*strchr(const char *, int);
char	*strrchr(const char *, int);

char	*strstr(const char *, const char *);

/*
 * ffs is an instruction on vax.
 */
int	 ffs(int);
#if __GNUC_PREREQ__(2, 95) && (!defined(__vax__) || __GNUC_PREREQ__(4,1))
#define	ffs(x)		__builtin_ffs(x)
#endif

void	 kern_assert(const char *, ...)
    __attribute__((__format__(__printf__, 1, 2)));
unsigned int
	bcdtobin(unsigned int);
unsigned int
	bintobcd(unsigned int);
u_int32_t
	inet_addr(const char *);
struct in_addr;
int	inet_aton(const char *, struct in_addr *);
char	*intoa(u_int32_t);
#define inet_ntoa(a) intoa((a).s_addr)
void	*memchr(const void *, int, size_t);
void	*memmove(void *, const void *, size_t);
int	 pmatch(const char *, const char *, const char **);
#ifndef SMALL_RANDOM
void	 srandom(unsigned long);
char	*initstate(unsigned long, char *, size_t);
char	*setstate(char *);
#endif /* SMALL_RANDOM */
long	 random(void);
void	 mtprng_init32(struct mtprng_state *, uint32_t);
void	 mtprng_initarray(struct mtprng_state *, const uint32_t *, size_t);
uint32_t mtprng_rawrandom(struct mtprng_state *);
uint32_t mtprng_random(struct mtprng_state *);
int	 scanc(u_int, const u_char *, const u_char *, int);
int	 skpc(int, size_t, u_char *);
int	 strcasecmp(const char *, const char *);
size_t	 strlcpy(char *, const char *, size_t);
size_t	 strlcat(char *, const char *, size_t);
int	 strncasecmp(const char *, const char *, size_t);
u_long	 strtoul(const char *, char **, int);
long long strtoll(const char *, char **, int);
unsigned long long strtoull(const char *, char **, int);
uintmax_t strtoumax(const char *, char **, int);
int	 snprintb(char *, size_t, const char *, uint64_t);
int	 snprintb_m(char *, size_t, const char *, uint64_t, size_t);
int	 kheapsort(void *, size_t, size_t, int (*)(const void *, const void *),
		   void *);
#ifndef __minix
uint32_t crc32(uint32_t, const uint8_t *, size_t);
#endif
unsigned int	popcount(unsigned int) __constfunc;
unsigned int	popcountl(unsigned long) __constfunc;
unsigned int	popcountll(unsigned long long) __constfunc;
unsigned int	popcount32(uint32_t) __constfunc;
unsigned int	popcount64(uint64_t) __constfunc;
#endif /* !_LIB_LIBKERN_LIBKERN_H_ */
