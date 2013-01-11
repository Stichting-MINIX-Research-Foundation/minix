/*	$NetBSD: stdlib.h,v 1.100 2012/06/21 21:13:29 christos Exp $	*/

/*-
 * Copyright (c) 1990, 1993
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
 *	@(#)stdlib.h	8.5 (Berkeley) 5/19/95
 */

#ifndef _STDLIB_H_
#define _STDLIB_H_

#include <sys/cdefs.h>
#include <sys/featuretest.h>

#if defined(_NETBSD_SOURCE)
#include <sys/types.h>		/* for quad_t, etc. */
#endif

#include <machine/ansi.h>

#ifdef	_BSD_SIZE_T_
typedef	_BSD_SIZE_T_	size_t;
#undef	_BSD_SIZE_T_
#endif

#if defined(_BSD_WCHAR_T_) && !defined(__cplusplus)
typedef	_BSD_WCHAR_T_	wchar_t;
#undef	_BSD_WCHAR_T_
#endif

typedef struct {
	int quot;		/* quotient */
	int rem;		/* remainder */
} div_t;

typedef struct {
	long quot;		/* quotient */
	long rem;		/* remainder */
} ldiv_t;

#if !defined(_ANSI_SOURCE) && \
    (defined(_ISOC99_SOURCE) || (__STDC_VERSION__ - 0) >= 199901L || \
     defined(_NETBSD_SOURCE))
typedef struct {
	/* LONGLONG */
	long long int quot;	/* quotient */
	/* LONGLONG */
	long long int rem;	/* remainder */
} lldiv_t;
#endif

#if defined(_NETBSD_SOURCE)
typedef struct {
	quad_t quot;		/* quotient */
	quad_t rem;		/* remainder */
} qdiv_t;
#endif


#include <sys/null.h>

#define	EXIT_FAILURE	1
#define	EXIT_SUCCESS	0

#define	RAND_MAX	0x7fffffff

extern size_t __mb_cur_max;
#define	MB_CUR_MAX	__mb_cur_max

__BEGIN_DECLS
__dead	 void _Exit(int);
__dead	 void abort(void);
__pure	 int abs(int);
int	 atexit(void (*)(void));
double	 atof(const char *);
int	 atoi(const char *);
long	 atol(const char *);
#ifndef __BSEARCH_DECLARED
#define __BSEARCH_DECLARED
/* also in search.h */
void	*bsearch(const void *, const void *, size_t, size_t,
    int (*)(const void *, const void *));
#endif /* __BSEARCH_DECLARED */
void	*calloc(size_t, size_t);
div_t	 div(int, int);
__dead	 void exit(int);
void	 free(void *);
__aconst char *getenv(const char *);
__pure long
	 labs(long);
ldiv_t	 ldiv(long, long);
void	*malloc(size_t);
void	 qsort(void *, size_t, size_t, int (*)(const void *, const void *));
int	 rand(void);
void	*realloc(void *, size_t);
void	 srand(unsigned);
double	 strtod(const char * __restrict, char ** __restrict);
long	 strtol(const char * __restrict, char ** __restrict, int);
unsigned long
	 strtoul(const char * __restrict, char ** __restrict, int);
int	 system(const char *);

/* These are currently just stubs. */
int	 mblen(const char *, size_t);
size_t	 mbstowcs(wchar_t * __restrict, const char * __restrict, size_t);
int	 wctomb(char *, wchar_t);
int	 mbtowc(wchar_t * __restrict, const char * __restrict, size_t);
size_t	 wcstombs(char * __restrict, const wchar_t * __restrict, size_t);

#if defined(_POSIX_C_SOURCE) || defined(_XOPEN_SOURCE) || \
    defined(_NETBSD_SOURCE)


/*
 * IEEE Std 1003.1c-95, also adopted by X/Open CAE Spec Issue 5 Version 2
 */
#if (_POSIX_C_SOURCE - 0) >= 199506L || (_XOPEN_SOURCE - 0) >= 500 || \
    defined(_REENTRANT) || defined(_NETBSD_SOURCE)
int	 rand_r(unsigned int *);
#endif

/*
 * X/Open Portability Guide >= Issue 4
 */
#if defined(_XOPEN_SOURCE) || defined(_NETBSD_SOURCE)
double	 drand48(void);
double	 erand48(unsigned short[3]);
long	 jrand48(unsigned short[3]);
void	 lcong48(unsigned short[7]);
long	 lrand48(void);
long	 mrand48(void);
long	 nrand48(unsigned short[3]);
unsigned short *
	 seed48(unsigned short[3]);
void	 srand48(long);

#ifndef __LIBC12_SOURCE__
int	 putenv(char *) __RENAME(__putenv50);
#endif
#endif


/*
 * X/Open Portability Guide >= Issue 4 Version 2
 */
#if (defined(_XOPEN_SOURCE) && defined(_XOPEN_SOURCE_EXTENDED)) || \
    (_XOPEN_SOURCE - 0) >= 500 || defined(_NETBSD_SOURCE)
long	 a64l(const char *);
char	*l64a(long);

char	*initstate(unsigned long, char *, size_t);
long	 random(void);
char	*setstate(char *);
void	 srandom(unsigned long);
#ifdef _NETBSD_SOURCE
#define	RANDOM_MAX	0x7fffffff	/* (((long)1 << 31) - 1) */
#endif

char	*mkdtemp(char *);
int	 mkstemp(char *);
char	*mktemp(char *)
#ifdef __MKTEMP_OK__
	__RENAME(_mktemp)
#endif
	;

int	 setkey(const char *);

char	*realpath(const char * __restrict, char * __restrict);

int	 ttyslot(void);

void	*valloc(size_t);		/* obsoleted by malloc() */

int	 grantpt(int);
int	 unlockpt(int);
char	*ptsname(int);
#endif

/*
 * ISO C99
 */
#if defined(_ISOC99_SOURCE) || (__STDC_VERSION__ - 0) >= 199901L || \
    defined(_NETBSD_SOURCE)
/* LONGLONG */
long long int	atoll(const char *);
/* LONGLONG */
long long int	llabs(long long int);
/* LONGLONG */
lldiv_t		lldiv(long long int, long long int);
/* LONGLONG */
long long int	strtoll(const char * __restrict, char ** __restrict, int);
/* LONGLONG */
unsigned long long int
		strtoull(const char * __restrict, char ** __restrict, int);
float		strtof(const char * __restrict, char ** __restrict);
long double	strtold(const char * __restrict, char ** __restrict);
#endif

/*
 * The Open Group Base Specifications, Issue 6; IEEE Std 1003.1-2001 (POSIX)
 */
#if (_POSIX_C_SOURCE - 0) >= 200112L || (_XOPEN_SOURCE - 0) >= 600 || \
    defined(_NETBSD_SOURCE)
int	 setenv(const char *, const char *, int);
#ifndef __LIBC12_SOURCE__
int	 unsetenv(const char *) __RENAME(__unsetenv13);
#endif

int	 posix_openpt(int);
int	 posix_memalign(void **, size_t, size_t);
#endif

/*
 * Implementation-defined extensions
 */
#if defined(_NETBSD_SOURCE)
#if defined(alloca) && (alloca == __builtin_alloca) && \
	defined(__GNUC__) && (__GNUC__ < 2)
void	*alloca(int);     /* built-in for gcc */
#elif defined(__PCC__) && !defined(__GNUC__)
#define alloca(size) __builtin_alloca(size)
#elif (__GNUC__ >= 2)
/* LSC: MINIX Patch needed (2 following lines) */
#define alloca(size) __builtin_alloca(size)
#else
void	*alloca(size_t);
#endif /* __GNUC__ */

uint32_t arc4random(void);
void	 arc4random_stir(void);
void	 arc4random_buf(void *, size_t);
uint32_t arc4random_uniform(uint32_t);
void	 arc4random_addrandom(u_char *, int);
char	*getbsize(int *, long *);
char	*cgetcap(char *, const char *, int);
int	 cgetclose(void);
int	 cgetent(char **, const char * const *, const char *);
int	 cgetfirst(char **, const char * const *);
int	 cgetmatch(const char *, const char *);
int	 cgetnext(char **, const char * const *);
int	 cgetnum(char *, const char *, long *);
int	 cgetset(const char *);
int	 cgetstr(char *, const char *, char **);
int	 cgetustr(char *, const char *, char **);
void	 csetexpandtc(int);

int	 daemon(int, int);
#ifndef __minix
int	 devname_r(dev_t, mode_t, char *, size_t);
#ifndef __LIBC12_SOURCE__
__aconst char *devname(dev_t, mode_t) __RENAME(__devname50);
#endif
#endif /* !__minix */

#define	HN_DECIMAL		0x01
#define	HN_NOSPACE		0x02
#define	HN_B			0x04
#define	HN_DIVISOR_1000		0x08

#define	HN_GETSCALE		0x10
#define	HN_AUTOSCALE		0x20

int	 humanize_number(char *, size_t, int64_t, const char *, int, int);
int	 dehumanize_number(const char *, int64_t *);

#ifndef __minix
devmajor_t getdevmajor(const char *, mode_t);
#endif /* !__minix */
int	 getloadavg(double [], int);

int	 getenv_r(const char *, char *, size_t);

void	 cfree(void *);

int	 heapsort(void *, size_t, size_t, int (*)(const void *, const void *));
int	 mergesort(void *, size_t, size_t,
	    int (*)(const void *, const void *));
int	 radixsort(const unsigned char **, int, const unsigned char *,
	    unsigned);
int	 sradixsort(const unsigned char **, int, const unsigned char *,
	    unsigned);

void	 mi_vector_hash(const void * __restrict, size_t, uint32_t,
	    uint32_t[3]);

void	 setproctitle(const char *, ...)
	    __printflike(1, 2);
const char *getprogname(void) __constfunc;
void	setprogname(const char *);

quad_t	 qabs(quad_t);
quad_t	 strtoq(const char * __restrict, char ** __restrict, int);
u_quad_t strtouq(const char * __restrict, char ** __restrict, int);

	/* LONGLONG */
long long strsuftoll(const char *, const char *, long long, long long);
	/* LONGLONG */
long long strsuftollx(const char *, const char *, long long, long long,
	    		char *, size_t);

int	 l64a_r(long, char *, int);

size_t	shquote(const char *, char *, size_t);
size_t	shquotev(int, char * const *, char *, size_t);
#endif /* _NETBSD_SOURCE */
#endif /* _POSIX_C_SOURCE || _XOPEN_SOURCE || _NETBSD_SOURCE */

#if defined(_NETBSD_SOURCE)
qdiv_t	 qdiv(quad_t, quad_t);
#endif
__END_DECLS

#endif /* !_STDLIB_H_ */
