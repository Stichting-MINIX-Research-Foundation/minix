/*	$NetBSD: gnuc.h,v 1.2 2002/07/06 21:51:49 wiz Exp $	*/

/* @(#) Header: gnuc.h,v 1.3 95/10/09 02:47:01 leres Exp  (LBL) */

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
