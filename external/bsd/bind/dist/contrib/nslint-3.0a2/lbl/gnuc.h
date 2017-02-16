/*	$NetBSD: gnuc.h,v 1.1.1.3 2014/12/10 03:34:34 christos Exp $	*/

/* @(#) Id: gnuc.h,v 1.4 2006/04/30 03:58:45 leres Exp  (LBL) */

/* Define __P() macro, if necessary */
#ifndef __P
#if __STDC__
#define __P(protos) protos
#else
#define __P(protos) ()
#endif
#endif

/* inline foo */
#ifdef __GNUC__
#define inline __inline
#else
#define inline
#endif

/*
 * Handle new and old "dead" routine prototypes
 *
 * For example:
 *
 *	__dead void foo(void) __attribute__((noreturn));
 *
 */
#ifdef __GNUC__
#ifndef __dead
#if __GNUC__ >= 4
#define __dead
#define noreturn __noreturn__
#else
#define __dead volatile
#define noreturn volatile
#endif
#endif
#if __GNUC__ < 2  || (__GNUC__ == 2 && __GNUC_MINOR__ < 5)
#ifndef __attribute__
#define __attribute__(args)
#endif
#endif
#else
#ifndef __dead
#define __dead
#endif
#ifndef __attribute__
#define __attribute__(args)
#endif
#endif
