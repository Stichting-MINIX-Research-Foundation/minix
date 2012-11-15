/*	$NetBSD: cdefs.h,v 1.9 2012/01/20 14:08:06 joerg Exp $	*/

#ifndef	_I386_CDEFS_H_
#define	_I386_CDEFS_H_

#if defined(_STANDALONE)
#define	__compactcall	__attribute__((__regparm__(3)))
#endif

#define __ALIGNBYTES	(sizeof(int) - 1)

#if defined(__minix)
#ifndef __ELF__
#define __LEADING_UNDERSCORE
#endif
#endif /* defined(__minix) */
#endif /* !_I386_CDEFS_H_ */
