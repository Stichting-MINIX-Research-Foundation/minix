/* @(#) $Header: /proj/cvs/prod/bind9/contrib/nslint-2.1a3/lbl/gnuc.h,v 1.1 2001-12-21 04:12:05 marka Exp $ (LBL) */

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
 *	__dead void foo(void) __attribute__((volatile));
 *
 */
#ifdef __GNUC__
#ifndef __dead
#define __dead volatile
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
