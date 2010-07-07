/*	$NetBSD: null.h,v 1.8 2009/10/13 17:19:00 dsl Exp $	*/

#ifndef _SYS_NULL_H_
#define _SYS_NULL_H_
#ifndef	NULL
#if !defined(__GNUG__) || __GNUG__ < 2 || (__GNUG__ == 2 && __GNUC_MINOR__ < 90)
#if !defined(__cplusplus)
#define	NULL	((void *)0)
#else
#define	NULL	0
#endif /* !__cplusplus */
#else
#define	NULL	__null
#endif
#endif
#endif /* _SYS_NULL_H_ */
