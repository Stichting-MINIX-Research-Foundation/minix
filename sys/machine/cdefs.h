/*	$NetBSD: cdefs.h,v 1.8 2011/06/16 13:27:59 joerg Exp $	*/

#ifndef	_I386_CDEFS_H_
#define	_I386_CDEFS_H_

#if defined(_STANDALONE)
#ifdef __PCC__
#define	__compactcall
#else
#define	__compactcall	__attribute__((__regparm__(3)))
#endif
#endif

#ifdef __minix
#ifndef __ELF__
#define __LEADING_UNDERSCORE
#endif
#else /* !__minix */
/* No arch-specific cdefs. */
#endif

#endif /* !_I386_CDEFS_H_ */
